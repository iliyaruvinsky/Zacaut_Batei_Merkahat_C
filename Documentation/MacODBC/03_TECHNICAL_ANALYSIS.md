# MacODBC.h - ניתוח טכני מפורט

**רכיב**: MacODBC (שכבת תשתית ODBC)
**מזהה משימה**: DOC-MACODBC-001
**תאריך יצירה**: 2026-02-11

---

## ניתוח ODBC_Exec — 8 שלבים

### שלב 1: פענוח פקודה ואתחול (שורות 422-772)

על פי `MacODBC.h:446-450`, הפונקציה מוצהרת עם variadic args:
```c
int ODBC_Exec(ODBC_DB_HEADER *Database, int CommandType_in, ...)
```

נראה כי שלב זה כולל:
- הצהרת משתנים מקומיים כולל מערכי cache (StatementCache, MirrorStatementCache) בגודל `ODBC_MAXIMUM_OPERATION`, כפי שנצפה בשורות 452-531
- `va_start` ואתחול `FieldLengthsRead` ו-`ColumnParameterPointers` באמצעות memset (שורות 569-583)
- הגדרת `ODBC_AvoidRecursion=1` (שורה 579) למניעת רקורסיה
- switch מרכזי (שורות 634-763) הממפה כל ערך `ODBC_CommandType` לדגלי התנהגות:
  - `NeedToPrepare`, `NeedToExecute`, `NeedToFetch`
  - `NeedToInterpretOp`, `NeedToCommit`, `NeedToRollBack`
  - `InterpretOutputOnly` (למצב FetchCursorInto)
- אימות `OperationIdentifier` (שורות 765-772)

### שלב 2: אתחול קריאה ראשונה (שורות 775-887)

על פי `MacODBC.h:780`, מותנה ב-`ODBC_Exec_FirstTimeCalled`:
- memset של כל מערכי cache (StatementCache, MirrorStatementCache, Statement_DB_Provider) — שורות 791-801
- איפוס מונים ודגלים (שורות 803-811)
- התקנת signal handler SIGSEGV באמצעות `sigaction` עם `SIGSEGV_Handler` (שורות 817-831)
- בדיקת גודל bool בזמן ריצה: `sizeof(bool)` קובע את `BoolType` עבור ODBC binding (שורות 836-845). דרוש כי מהדרי C שונים ממפים bool לגדלים שונים (1 vs 4 bytes).

לאחר בלוק ה-init:
- מונה פעילות עבור `INS_messages_details` (שורות 851-852)
- אבחון ו-FreeStatement מאולץ עבור prepared statements שנותרו דולפים (שורות 866-874)

### שלב 3: טעינת מטאדטה של פעולה (שורות 888-1072)

תחילת do-while(0) dummy loop (שורה 888). אם `NeedToInterpretOp`:
- קריאה ל-`SQL_GetMainOperationParameters` (שורות 899-913) — מחזירה: טקסט SQL, מפרטי עמודות, דביקות, דגל שיקוף, שם cursor
- לוגיקת שיקוף: `CommandIsMirrored` נקבע רק אם `ODBC_MIRRORING_ENABLED` (שורה 918)
- טקסט SQL דינמי מ-va_arg אם `SQL_CommandText==NULL` (שורות 953-964)
- קריאה ל-`find_FOR_UPDATE_or_GEN_VALUES` (שורות 975-980) — מזהה INSERT/SELECT/FOR UPDATE
- `CommandIsMirrored=0` עבור SELECTs (שורות 994-996)
- `MIRROR_DB` מחושב כהפך של Database הנוכחי (שורה 1002)
- טיפול ב-WHERE clause: `WhereClauseIdentifier` מ-va_arg (שורה 1021), קריאה ל-`SQL_GetWhereClauseParameters` (שורות 1030-1034)

### שלב 4: מחזור חיי Statement דביק (שורות 1075-1231)

בתוך בלוק `if(NeedToPrepare)` (שורה 1086):
- **החלפת ספק**: אם ה-provider השתנה מהשימוש האחרון, משחרר handle ישן ו-mirror cache, מפחית `NumStickyHandlesUsed` (שורות 1095-1131)
- **בקרת כניסה**: אם `NumStickyHandlesUsed >= ODBC_MAX_STICKY_STATEMENTS` (120), מבטל דביקות עם אבחון (שורות 1145-1155); אחרת מגדיל מונה (שורה 1158)
- **וולידציית prepared-state**: קורא `SQLNumParams` כש-`ENABLE_PREPARED_STATE_VALIDATION=1` (שורה 1182); בכישלון, משחרר handle ומכריח re-PREPARE (שורות 1195-1202)
- **הקצאת statement חדש**: `SQLAllocStmt` ל-DB ראשי (שורה 1213); אם `CommandIsMirrored`, גם הקצאה ל-mirror (שורה 1224)

### שלב 5: בניית SQL ו-PREPARE (שורות 1233-1447)

- הרכבת `SQL_CmdBuffer` מ-`SQL_CommandText` + `WhereClauseText` אופציונלי (שורות 1269-1277)
- הרכבת VALUES clause אוטומטית עבור INSERTs (שורות 1284-1295)
- קריאה ל-`SQL_CustomizePerDB` להחלפת טוקנים ספציפיים לספק (שורה 1300)
- הגדרת FOR UPDATE cursor: `SQLSetStmtAttr` עבור concurrency locking + הוספת FOR UPDATE אוטומטית (שורות 1314-1336)
- `SQLPrepare` של ה-SQL המורכב (שורה 1355); בכישלון: disconnect via `SQLMD_disconnect`, איפוס `ODBC_Exec_FirstTimeCalled`, החזרת `DB_CONNECTION_BROKEN` (שורות 1377-1392) — **זהו trigger ל-auto-reconnect**
- Mirror PREPARE על DB חלופי (שורות 1402-1417)
- `SQLSetCursorName` אם `CursorName != NULL` (שורות 1421-1433)

### שלב 6: וולידציית מצביעים ו-Binding (שורות 1449-1976)

**חלק א — איסוף מצביעים עם SIGSEGV validation** (שורות 1449-1651):
- הגדרת `ODBC_ValidatingPointers=1` (שורה 1456)
- לולאת מצביעי פלט: שליפת va_arg pointers, דילוג על WHERE clause ID ו-SQL דינמי עבור FetchCursorInto (שורות 1479-1519), וולידציה באמצעות `ODBC_IsValidPointer` עם `CHECK_POINTER_IS_WRITABLE` (שורה 1524)
- לולאת מצביעי קלט: וולידציה עם `CHECK_VALIDITY_ONLY` (שורה 1560)
- לולאת מצביעי WHERE clause: וולידציה (שורה 1595)
- איפוס `ODBC_ValidatingPointers` (שורה 1618)
- בדיקת sentinel `END_OF_ARG_LIST` (שורות 1638-1649)

**חלק ב — ODBC binding** (שורות 1661-1976):
- לולאת `SQLBindCol` לעמודות פלט (שורות 1679-1781): נורמליזציית טיפוסים (VARCHAR→CHAR, single-char→UTINYINT), חישוב bind-length, מערך FieldLengthsRead
- לולאת `SQLBindParameter` לעמודות קלט (שורות 1787-1896): מיפוי C-to-SQL type, חיתוך מחרוזות ארוכות (שורות 1820-1835), binding mirror
- לולאת `SQLBindParameter` עבור WHERE clause עם offset NumInputColumns + mirror (שורות 1902-1975)

### שלב 7: Execute, Mirror, Fetch (שורות 1978-2288)

**Execute** (NeedToExecute, שורה 1981):
- בדיקת pre-execute: החזרת `DB_CONNECTION_BROKEN` אם `Database->Connected==0` (שורות 1990-1993)
- **Mirror execute**: `SQLExecute` על `MirrorStatementPtr` **קודם** (שורה 2029); switch שגיאות (שורות 2047-2079); בהצלחה: `ALTERNATE_DB_USED=1` + `SQLRowCount` (שורות 2089-2094)
- **Primary execute**: `SQLExecute` על `ThisStatementPtr` (שורה 2100); switch שגיאות עבור sticky statements (שורות 2133-2183); בהצלחה: `StatementOpened=1` (שורה 2206), `SQLRowCount` (שורות 2212-2213)
- תיקון rows-affected שלילי (שורות 2232-2240)
- זיהוי כישלון INSERT שקט ב-MS-SQL (שורות 2252-2261)
- מיזוג תוצאות mirror: שימוש בנמוך יותר מבין שני rows-affected, החלפה ל-MIRROR_SQLCODE אם non-zero (שורות 2266-2284)

**Fetch** (NeedToFetch, שורה 2302):
- `SQLFetchScroll SQL_FETCH_NEXT` (שורה 2328)
- לוגיקת `Convert_NF_to_zero` (שורות 2333-2337)
- טיפול בשגיאת fetch עם ניקוי sticky (שורות 2340-2372)
- בדיקת cardinality באמצעות SQLFetchScroll שני (שורות 2385-2392)

### שלב 8: טרנזקציה, סגירה, החזרה (שורות 2290-2657)

**Commit** (NeedToCommit, שורות 2396-2446):
- CommitAllWork: אם `ALTERNATE_DB_USED` אז `SQLEndTran` ברמת ENV, אחרת ברמת DBC ל-MAIN_DB בלבד
- CommitWork רגיל: `SQLEndTran` ברמת DBC

**Rollback** (NeedToRollBack, שורות 2449-2490):
- לוגיקה דומה ל-commit (ENV vs DBC) ללא אופטימיזציית `ALTERNATE_DB_USED`

**Isolation level** (שורות 2493-2529):
- SetDirtyRead/SetCommittedRead/SetRepeatableRead באמצעות `SQLSetConnectAttr` (מדלג על Informix)

**Close** (שורות 2539-2606):
- Sticky statements: `SQLCloseCursor` soft close + mirror close (שורות 2590-2597)
- Non-sticky: `NeedToFreeStatement=1` (שורה 2604)

**Free** (שורות 2609-2639):
- `SQLFreeHandle` + mirror free, הפחתת מונה sticky, איפוס `StatementPrepared`/`Opened`

**Cleanup** (שורות 2642-2657):
- `va_end`, הקצאת `ColumnOutputLengths`, איפוס `ODBC_AvoidRecursion` ו-`ODBC_ValidatingPointers`, החזרת `SQLCODE`

---

## ניתוח פונקציות עזר

### SQL_GetMainOperationParameters (שורות 2661-2817)

נראה כי פונקציה זו מפענחת `OperationIdentifier` למטאדטה של SQL:
- אופטימיזציית `SameOpAsLastTime` לדילוג על re-parsing (שורה 2713)
- switch מרכזי בשורה 2745 כולל `#include "MacODBC_MyOperators.c"` (שורה 2747) — **גבול הזרקת cross-reference**
- קריאה ל-`ParseColumnList` למפרטי עמודות פלט וקלט (שורות 2760, 2765)
- וולידציית `Convert_NF_to_zero` (רק עבור פלט INT יחיד) (שורות 2775-2778)
- העתקת תוצאות ל-14 פרמטרי פלט (שורות 2782-2812)

### SQL_GetWhereClauseParameters (שורות 2821-2902)

נראה כי פונקציה זו מפענחת `WhereClauseIdentifier`:
- אופטימיזציית `SameOpAsLastTime` (שורה 2843)
- switch מרכזי בשורה 2864 כולל `#include "MacODBC_MyCustomWhereClauses.c"` (שורה 2866) — **גבול הזרקת cross-reference**
- קריאה ל-`ParseColumnList` (שורה 2879)
- העתקת תוצאות ל-4 פרמטרי פלט (שורות 2886-2898)

### SQL_CustomizePerDB (שורות 2906-3136)

נראה כי פונקציה זו מבצעת החלפת טוקנים ספציפית לספק DB:

| טוקן | Informix | MS-SQL | שורות |
|------|----------|--------|-------|
| {MODULO}/{MOD} | MOD(A,B) | (A % B) | 2948-2986 |
| {TOP}/{FIRST} | FIRST | TOP | 2990-3019 |
| {TRANSACTION} | TRANSACTION | "TRANSACTION" (escaped) | 3064-3093 |
| {GROUP} | GROUP | "GROUP" (escaped) | 3098-3127 |

משתמש ב-`strcasestr` לחיפוש case-insensitive (דורש `_GNU_SOURCE`).
הערה: תמיכת `{ROWID}` מוערת (שורות 3021-3055) — ספציפי ל-Oracle, לא הושלם.

### ParseColumnList (שורות 3140-3330)

מפענח מחרוזת column-spec (למשל `"INT, CHAR(20), SHORT"`) למערך `ODBC_ColumnParams`. על פי הקוד, 12 טיפוסי C נתמכים:

| טיפוס מחרוזת | טיפוס ODBC | הערות |
|--------------|-----------|-------|
| INT/INTEGER | SQL_C_SLONG | |
| SHORT/SMALLINT | SQL_C_SSHORT | |
| CHAR(n) | SQL_C_CHAR | עם אורך |
| VARCHAR(n) | SQL_VARCHAR | עם אורך |
| DOUBLE/FLOAT | SQL_C_DOUBLE | |
| LONG/BIGINT | SQL_C_SBIGINT | |
| BOOL/BOOLEAN | BoolType | נבדק בזמן ריצה |
| UNSIGNED | SQL_C_ULONG | |
| USHORT | SQL_C_USHORT | |
| REAL | SQL_C_FLOAT | |
| SINGLE_CHAR/ONECHAR | SQL_C_CHAR length 0 | טיפול מיוחד |

### find_FOR_UPDATE_or_GEN_VALUES (שורות 3334-3439)

מתקן SQL לזיהוי: האם זה INSERT (שורות 3377-3380), האם זה SELECT (שורות 3382-3385), האם מכיל FOR UPDATE (שורות 3403-3416), והאם יש נקודת הכנסת WHERE מותאמת (%s, שורה 3392).

### ODBC_CONNECT (שורות 3443-3794)

מקים חיבור מלא:
- אתחול ראשון: אתחול MS_DB (ODBC_MS_SqlServer) ו-INF_DB (ODBC_Informix) (שורות 3476-3482)
- `setlocale` לעברית (שורה 3488)
- `SQLAllocEnv` (שורות 3494-3505)
- `SQLAllocConnect` + timeout (שורות 3513-3528)
- MS-SQL: preserve-cursors (שורות 3542-3547), `USE <dbname>` (שורות 3630-3656), `SET LOCK_TIMEOUT` (שורות 3670-3689), `SET DEADLOCK_PRIORITY` (שורות 3702-3721)
- Informix: disable AUTOFREE (שורות 3752-3761)
- AUTOCOMMIT off (שורה 3729), dirty-read isolation default (שורה 3742)

### CleanupODBC (שורות 3798-3819)

`SQLDisconnect` + `SQLFreeConnect`, `Connected=0`. מפחית `NUM_ODBC_DBS_CONNECTED`; אם אחרון, גם `SQLFreeEnv` (שורות 3812-3814).

### ODBC_ErrorHandler (שורות 3823-4004)

מטפל שגיאות מרכזי:
- dispatch על `ErrorCategory` (שורות 3847-3884)
- **המרת access-conflict סינתטית**: שגיאות native -11103, -11031, -211 מומרות ל-`SQLERR_CANNOT_POSITION_WITHIN_TABL` (-243) — שורות 3903-3926
- **המרת auto-reconnect**: `SQL_TRN_CLOSED_BY_OTHER_SESSION`, `DB_TCP_PROVIDER_ERROR`, `TABLE_SCHEMA_CHANGED`, `DB_CONNECTION_BROKEN` → קריאה ל-`SQLMD_disconnect`, איפוס `ODBC_Exec_FirstTimeCalled`, המרה ל-`DB_CONNECTION_BROKEN` — שורות 3936-3959

---

## מנגנון וולידציית מצביעים (SIGSEGV)

דפוס לא שגרתי — מאמת מצביעי va_arg על ידי ניסיון גישה מכוון בתוך בלוק `sigsetjmp`/`siglongjmp`:

### ODBC_IsValidPointer (שורות 4013-4070)
1. `sigsetjmp(BeforePointerTest)` (שורה 4027) — שומר מצב
2. ניסיון קריאת byte מהמצביע (שורה 4032)
3. אופציונלית: כתיבה חזרה אם `CheckPointerWritable_in` (שורה 4041)
4. אם SIGSEGV נורה: ה-handler מגדיר `ODBC_PointerIsValid=0` ומבצע `siglongjmp` חזרה (שורות 4046-4052)
5. החזרת `ODBC_PointerIsValid` + `ErrDesc_out` ("not readable" / "not writable")

### macODBC_SegmentationFaultCatcher (שורות 4073-4118)
- **מצב validation** (`ODBC_ValidatingPointers==true`): מגדיר `ODBC_PointerIsValid=0`, מתקין handler מחדש, `siglongjmp` חזרה (שורות 4086-4095)
- **מצב רגיל** (`ODBC_ValidatingPointers==false`): רושם segfault fatal, קורא `RollbackAllWork` + `SQLMD_disconnect`, יציאה באמצעות `ExitSonProcess` (שורות 4099-4106)

**מדוע דפוס זה קיים**: variadic arguments אינם ניתנים לבדיקת טיפוסים בזמן קומפילציה. מצביע שגוי שמועבר ל-ExecSQL יגרום ל-crash ב-SQLBindCol/SQLBindParameter — מנגנון זה תופס זאת מראש.

---

## מנגנון Auto-Reconnect

על פי ניתוח הקוד, נראה כי שלושה נתיבים מפעילים reconnect:

1. **כישלון PREPARE** (שורות 1377-1392): disconnect + איפוס `FirstTimeCalled` → `DB_CONNECTION_BROKEN`
2. **Execute על DB מנותק** (שורות 1990-1993): החזרת `DB_CONNECTION_BROKEN`
3. **המרת Error Handler** (שורות 3936-3959): שגיאות TCP/transaction/schema → disconnect + `DB_CONNECTION_BROKEN`

הקורא צפוי לנסות שוב; בקריאה הבאה, `ODBC_Exec_FirstTimeCalled` מאופס, ומפעיל אתחול מלא מחדש.

---

## הפצחת טיפוסים ב-Provider Abstraction

על פי `SQL_CustomizePerDB` (שורות 2906-3136), נראה כי MacODBC מגשר על הבדלי תחביר SQL בזמן ריצה:

- **MODULO**: Informix `MOD(A,B)` ↔ MS-SQL `(A % B)` — עם parsing של strtok_r
- **TOP/FIRST**: Informix `FIRST` ↔ MS-SQL `TOP` — מיקום שונה בפקודה
- **TRANSACTION**: Informix מילה שמורה ↔ MS-SQL דורש escaping במרכאות
- **GROUP**: אותו דפוס כמו TRANSACTION

---

*נוצר על ידי סוכן המתעד של CIDRA — DOC-MACODBC-001*
