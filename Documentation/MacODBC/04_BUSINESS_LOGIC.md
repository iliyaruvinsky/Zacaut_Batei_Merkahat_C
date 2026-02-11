# MacODBC.h - לוגיקה עסקית

**רכיב**: MacODBC (שכבת תשתית ODBC)
**מזהה משימה**: DOC-MACODBC-001
**תאריך יצירה**: 2026-02-11

---

## מחזור חיי Cursor

על פי 25 המאקרואים ב-`MacODBC.h:311-335` ו-switch ה-CommandType ב-`MacODBC.h:634-763`, נראה כי מחזור חיי cursor מלא כולל:

### 1. הצהרה (Declare)
```
DeclareCursor(db, op_id, &output_var)           → DECLARE_CURSOR
DeclareCursorInto(db, op_id, &out1, &out2, ...) → DECLARE_CURSOR_INTO
```
מגדיר: `NeedToPrepare=1`, `NeedToInterpretOp=1`
**לא** מגדיר: `NeedToExecute` או `NeedToFetch`

### 2. הצהרה ופתיחה (Declare + Open)
```
DeclareAndOpenCursor(db, op_id, &output_var, input_var)    → DECLARE_AND_OPEN_CURSOR
DeclareAndOpenCursorInto(db, op_id, &o1, &o2, ..., in)     → DECLARE_AND_OPEN_CURSOR_INTO
```
מגדיר: `NeedToPrepare=1`, `NeedToExecute=1`, `NeedToInterpretOp=1`
**לא** מגדיר: `NeedToFetch`

### 3. פתיחה (Open)
```
OpenCursor(db, op_id)                           → OPEN_CURSOR
OpenCursorUsing(db, op_id, input_var)           → OPEN_CURSOR_USING
```
מגדיר: `NeedToExecute=1`

### 4. Fetch
```
FetchCursor(db, op_id)                          → FETCH_CURSOR
FetchCursorInto(db, op_id, &o1, &o2, ...)      → FETCH_CURSOR_INTO
```
מגדיר: `NeedToFetch=1`
FetchCursorInto גם מגדיר: `NeedToInterpretOp=1`, `InterpretOutputOnly=1`

### 5. סגירה ושחרור
```
CloseCursor(db, op_id)                          → CLOSE_CURSOR
FreeStatement(db, op_id)                        → FREE_STATEMENT
```
`CloseCursor`: soft close (SQLCloseCursor) — statement נשאר prepared
`FreeStatement`: full free (SQLFreeHandle) — שחרור מלא

### 6. הרצת SQL חד-פעמית
```
ExecSQL(db, op_id, &output_var, input_var)      → SINGLETON_SQL_CALL
```
מגדיר: `NeedToPrepare=1`, `NeedToExecute=1`, `NeedToFetch=1`, `NeedToInterpretOp=1`

---

## ניהול טרנזקציות

על פי `MacODBC.h:2396-2490`, נראה כי ישנם שני מודלים:

### CommitWork / RollbackWork (ברמת DBC)
על פי שורות 2428-2446, נראה כי:
- `SQLEndTran(SQL_HANDLE_DBC, Database->HDBC, SQL_COMMIT)` — commit ל-DB ספציפי
- אם ה-DB אינו MAIN_DB, גם מנקה `ALTERNATE_DB_USED`

### CommitAllWork / RollbackAllWork (ברמת ENV או DBC)
על פי שורות 2396-2427, נראה כי:
- אם `ALTERNATE_DB_USED`: `SQLEndTran(SQL_HANDLE_ENV, ODBC_henv, SQL_COMMIT)` — commit ברמת ENV (שני DBs)
- אם לא: `SQLEndTran(SQL_HANDLE_DBC, MAIN_DB->HDBC, SQL_COMMIT)` — commit ל-MAIN_DB בלבד (אופטימיזציה)

**משמעות**: CommitAllWork משתמש ב-ENV level רק כשני DBs היו בשימוש, מונע commit מיותר ל-DB שלא נגע.

### Isolation Levels
על פי שורות 2493-2529:
- `SetDirtyRead` → `SQL_TXN_READ_UNCOMMITTED`
- `SetCommittedRead` → `SQL_TXN_READ_COMMITTED`
- `SetRepeatableRead` → `SQL_TXN_REPEATABLE_READ`

הערה: מדלג על Informix, כפי שנצפה בבדיקת `Database->Provider != ODBC_Informix`.

---

## החלטת שיקוף

על פי ניתוח שלב 3 של ODBC_Exec (שורות 888-1072), נראה כי החלטת השיקוף מתבצעת ב-3 שלבים:

### שלב 1: בדיקת דגל גלובלי
```
CommandIsMirrored = ODBC_MIRRORING_ENABLED (שורה 918)
```
`ODBC_MIRRORING_ENABLED` נקבע ב-`GenSql.c` ממשתנה סביבה.

### שלב 2: בדיקת דגל לכל פעולה
ה-`Mirrored` flag מגיע מ-`SQL_GetMainOperationParameters` — כל operation מגדיר האם הוא צריך שיקוף.

### שלב 3: מניעת שיקוף ל-SELECTs
```
if (IsSelectStatement) CommandIsMirrored = 0;  (שורות 994-996)
```

### סיכום לוגיקה:
```
CommandIsMirrored = ODBC_MIRRORING_ENABLED
                  && per-operation Mirrored flag
                  && NOT SELECT
```

### סדר הרצה
על פי `MacODBC.h:2029`, נראה כי ה-**mirror מבוצע קודם**:
1. `SQLExecute` על `MirrorStatementPtr` (שורה 2029)
2. בדיקת שגיאות mirror — כישלון נרשם אך **אינו fatal** (שורות 2047-2079)
3. `SQLExecute` על `ThisStatementPtr` (שורה 2100)
4. מיזוג תוצאות: rows-affected = min(primary, mirror), SQLCODE = mirror אם non-zero (שורות 2266-2284)

---

## בקרת כניסה (Admission Control) לstatements דביקים

על פי `MacODBC.h:1145-1159`, נראה כי:

### מנגנון:
1. כל prepared statement שסומן כדביק (StatementIsSticky) מחזיק handle פתוח
2. מונה `NumStickyHandlesUsed` עוקב אחרי handles פעילים
3. כשמגיעים לגבול `ODBC_MAX_STICKY_STATEMENTS` (120):
   - ה-statement החדש **מאבד** את הדביקות שלו
   - אבחון נרשם
   - ה-statement עדיין עובד אך לא יישמר ב-cache

### מחזור חיי statement דביק:
```
הקצאה (SQLAllocStmt)
    ↓
PREPARE (SQLPrepare) → StatementPrepared[op] = 1
    ↓
Execute/Fetch (ניתן לחזור מספר פעמים)
    ↓
Close (SQLCloseCursor — soft close, statement נשאר prepared)
    ↓
[הקריאה הבאה: מדלג על PREPARE, ישר ל-Bind/Execute]
    ↓
Free (SQLFreeHandle — שחרור, NumStickyHandlesUsed--)
```

### וולידציית prepared-state
על פי שורות 1182-1202, כש-`ENABLE_PREPARED_STATE_VALIDATION=1`:
- קורא `SQLNumParams` על ה-statement
- אם נכשל → ה-handle מושחרר ומתבצע re-PREPARE
- מנגנון הגנה נגד statements שהפכו "stale" (למשל אחרי reconnect)

---

## שרשראות המרת שגיאות

על פי `ODBC_ErrorHandler` (שורות 3823-4004), נראה כי שתי שרשראות המרה עיקריות:

### שרשרת 1: המרת access-conflict (שורות 3903-3926)
```
שגיאות native -11103 (S1002), -11031, -211
    ↓
מומרות ל: SQLERR_CANNOT_POSITION_WITHIN_TABL (-243)
    ↓
מטרה: להפעיל retry אוטומטי ברמת הקורא
    ↓
פעולה נוספת: שחרור handle sticky + הפחתת מונה
```

### שרשרת 2: המרת auto-reconnect (שורות 3936-3959)
```
SQL_TRN_CLOSED_BY_OTHER_SESSION
DB_TCP_PROVIDER_ERROR
TABLE_SCHEMA_CHANGED
DB_CONNECTION_BROKEN
    ↓
מומרות ל: DB_CONNECTION_BROKEN
    ↓
פעולות: SQLMD_disconnect + איפוס ODBC_Exec_FirstTimeCalled
    ↓
מטרה: הקריאה הבאה תבצע אתחול מלא מחדש
```

---

## הקמת חיבור

על פי `ODBC_CONNECT` (שורות 3443-3794), נראה כי תהליך החיבור:

### רצף אתחול
1. אתחול כותרות DB: MS_DB (ODBC_MS_SqlServer), INF_DB (ODBC_Informix) — שורות 3476-3482
2. `setlocale(LC_ALL, "he_IL.UTF-8")` — שורה 3488
3. `SQLAllocEnv` עבור ODBC environment (רק פעם ראשונה) — שורות 3494-3505
4. `SQLAllocConnect` — שורה 3513
5. הגדרת timeouts (login, connection) — שורות 3525-3536
6. MS-SQL: `SQL_COPT_SS_PRESERVE_CURSORS` אם `ODBC_PRESERVE_CURSORS` — שורות 3542-3547
7. `SQLConnect(DSN, username, password)` — שורה 3552

### הגדרות post-connect ספציפיות לספק
**MS-SQL** (שורות 3630-3721):
- `USE <dbname>` — מעבר ל-database הרצוי
- `SET LOCK_TIMEOUT <ms>` — timeout נעילה
- `SET DEADLOCK_PRIORITY <level>` — עדיפות deadlock

**Informix** (שורות 3752-3761):
- disable `SQL_INFX_ATTR_AUTO_FREE` — מנגנון auto-free של Informix

**כל הספקים** (שורות 3729-3742):
- AUTOCOMMIT off
- dirty-read isolation default

### CleanupODBC (שורות 3798-3819)
- `SQLDisconnect` + `SQLFreeConnect` + `Connected=0`
- מפחית `NUM_ODBC_DBS_CONNECTED`
- אם אחרון: `SQLFreeEnv` — שורות 3812-3814

---

## זיהוי כישלון INSERT שקט

על פי `MacODBC.h:2252-2261`, נראה כי ב-MS-SQL, INSERT שנכשל בשקט (ללא שגיאה אך עם `sqlerrd[2]==0`) מזוהה ומטופל:
- בודק אם הפעולה היא INSERT ו-`sqlerrd[2]==0` (0 שורות הושפעו)
- בודק אם זו לא תוצאה צפויה (IsSelectStatement)
- אם מזוהה — מגדיר `ReturnCode` בהתאם

---

## בדיקת Cardinality

על פי `MacODBC.h:2385-2392`, לאחר fetch ראשון מוצלח, מבוצע fetch שני:
- אם ה-fetch השני מצליח → יותר משורה אחת קיימת
- משמש להתרעה על cardinality לא צפויה (singleton query שמחזיר יותר משורה)

---

*נוצר על ידי סוכן המתעד של CIDRA — DOC-MACODBC-001*
