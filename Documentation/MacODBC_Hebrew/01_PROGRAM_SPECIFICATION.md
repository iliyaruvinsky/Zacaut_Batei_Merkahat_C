# MacODBC.h - מפרט התוכנית

**רכיב**: MacODBC (שכבת תשתית ODBC)
**מזהה משימה**: DOC-MACODBC-001
**תאריך יצירה**: 2026-02-11
**מקור**: `source_code/Include/MacODBC.h`

---

## סקירה כללית

על פי כותרת הקובץ ב-`MacODBC.h:14-15`, הקובץ נכתב על ידי **Don Radlauer** בדצמבר 2019. נראה כי MacODBC.h מהווה את שכבת תשתית ה-ODBC המרכזית עבור כל מערכת ה-C Backend של מכבי, ומחליפה את גישת Informix Embedded SQL (ESQL) הקודמת בממשק ODBC התומך בחיבורים בו-זמניים ל-MS-SQL ו-Informix.

זהו קובץ היברידי (header/implementation): כל הקוד מוגן תחת `#ifdef MAIN` — קובץ ה-`.c` שמגדיר `MAIN` לפני ה-include מקבל את המימוש מקומפל לתוכו, כפי שנצפה ב-`MacODBC.h:168` ו-`MacODBC.h:422`.

---

## מטרה

על בסיס ניתוח הקוד, נראה כי MacODBC.h:

1. **מספק API מאקרואי** — 25 מאקרואים ציבוריים (כגון `ExecSQL`, `DeclareCursor`, `CommitWork`) המנתבים לפונקציית dispatcher יחידה (שורות 311-335)
2. **מנהל חיבורי מסד נתונים** — חיבור, ניתוק, ו-reconnect אוטומטי עבור MS-SQL ו-Informix (שורות 3443-3819)
3. **תומך בשיקוף מסד נתונים** — כתיבה כפולה ל-MAIN_DB ו-ALT_DB (שורות 918, 2023-2094)
4. **מנהל cache של prepared statements** — עד 120 statements "דביקים" (שורות 1086-1231)
5. **מאמת מצביעים בזמן ריצה** — מנגנון SIGSEGV/sigsetjmp/siglongjmp לוולידציה של va_arg pointers (שורות 4013-4118)
6. **מבצע התאמות SQL לפי ספק** — החלפת טוקנים בין תחביר Informix ו-MS-SQL (שורות 2906-3136)

---

## מבנה קבצים

| קובץ | שורות | מטרה |
|------|------:|------|
| MacODBC.h | 4121 | תשתית ODBC — קובץ היברידי header/implementation |

*ספירת שורות מאומתת מנתוני הצ'אנקר ב-`CHUNKS/MacODBC/repository.json`*

**הערה**: זהו קובץ יחיד שמשמש את כל 8 רכיבי המערכת, כפי שנצפה בסריקת ה-include chain ב-`RESEARCH/MacODBC_deepdive.md`.

---

## פונקציות

על פי ניתוח הקוד ב-`MacODBC.h`, 11 פונקציות ממומשות תחת `#ifdef MAIN`:

| פונקציה | שורות | מיקום | מטרה |
|---------|------:|-------|------|
| ODBC_Exec | 2212 | 446-2657 | dispatcher מרכזי — מקבל כל 25 סוגי פקודות |
| SQL_GetMainOperationParameters | 157 | 2661-2817 | פענוח OperationID למטאדטה של SQL |
| SQL_GetWhereClauseParameters | 82 | 2821-2902 | פענוח WhereClauseID לטקסט ופרמטרים |
| SQL_CustomizePerDB | 231 | 2906-3136 | החלפת טוקנים ספציפיים לספק DB |
| ParseColumnList | 191 | 3140-3330 | פענוח מחרוזת column-spec למערך ODBC_ColumnParams |
| find_FOR_UPDATE_or_GEN_VALUES | 106 | 3334-3439 | זיהוי INSERT/SELECT/FOR UPDATE בפקודת SQL |
| ODBC_CONNECT | 352 | 3443-3794 | הקמת חיבור מלא — env, connect, הגדרות ספציפיות |
| CleanupODBC | 22 | 3798-3819 | ניתוק ושחרור — env release כשאחרון מתנתק |
| ODBC_ErrorHandler | 182 | 3823-4004 | טיפול מרכזי בשגיאות — SQLGetDiagRec, המרת שגיאות |
| ODBC_IsValidPointer | 58 | 4013-4070 | בדיקת תקינות מצביע באמצעות sigsetjmp/siglongjmp |
| macODBC_SegmentationFaultCatcher | 46 | 4073-4118 | מטפל SIGSEGV — מצב validation vs מצב fatal |

**סה"כ פונקציות: 11**

---

## API מאקרואי ציבורי (25 מאקרואים)

על פי שורות 311-335 ב-`MacODBC.h`, כל המאקרואים מנתבים ל-`ODBC_Exec` עם ערך `ODBC_CommandType` מתאים:

### ניהול סמנים (Cursors)
| מאקרו | שורה | CommandType |
|--------|------|-------------|
| DeclareCursor | 311 | DECLARE_CURSOR |
| DeclareCursorInto | 312 | DECLARE_CURSOR_INTO |
| DeclareAndOpenCursor | 313 | DECLARE_AND_OPEN_CURSOR |
| DeclareAndOpenCursorInto | 314 | DECLARE_AND_OPEN_CURSOR_INTO |
| DeclareDeferredCursor | 315 | DEFERRED_INPUT_CURSOR |
| DeclareDeferredCursorInto | 316 | DEFERRED_INPUT_CURSOR_INTO |
| OpenCursor | 317 | OPEN_CURSOR |
| OpenCursorUsing | 318 | OPEN_CURSOR_USING |
| FetchCursor | 319 | FETCH_CURSOR |
| FetchCursorInto | 320 | FETCH_CURSOR_INTO |
| CloseCursor | 321 | CLOSE_CURSOR |
| FreeStatement | 322 | FREE_STATEMENT |

### הרצת SQL
| מאקרו | שורה | CommandType |
|--------|------|-------------|
| ExecSQL | 318 | SINGLETON_SQL_CALL |
| ExecSql | 319 | SINGLETON_SQL_CALL |

**הערה**: על פי `MacODBC.h:318-319`, שני האיותים (`ExecSQL` ו-`ExecSql`) קיימים עבור תאימות לאחור.

### טרנזקציות
| מאקרו | שורה | CommandType |
|--------|------|-------------|
| CommitWork | 323 | COMMIT_WORK |
| RollbackWork | 324 | ROLLBACK_WORK |
| RollBackWork | 325 | ROLLBACK_WORK |
| CommitAllWork | 326 | COMMIT_WORK (ENV level) |
| RollbackAllWork | 327 | ROLLBACK_WORK (ENV level) |
| RollBackAllWork | 328 | ROLLBACK_WORK (ENV level) |

**הערה**: על פי `MacODBC.h:324-325`, שני האיותים (`RollbackWork` ו-`RollBackWork`) קיימים.

### בידוד ושירותים
| מאקרו | שורה | CommandType |
|--------|------|-------------|
| SetDirtyRead | 329 | SET_DIRTY_READ |
| SetCommittedRead | 330 | SET_COMMITTED_READ |
| SetRepeatableRead | 331 | SET_REPEATABLE_READ |
| GetLengthsRead | 332 | GET_LENGTHS_READ |
| SetCustomSegmentationFaultHandler | 335 | SET_CUSTOM_SIGSEGV_HANDLER |

### מאקרואים שירותיים
על פי שורות 277-278 ב-`MacODBC.h`:
- `SQL_WORKED(R)` — בודק אם ערך ההחזרה מציין הצלחה
- `SQL_FAILED(R)` — בודק אם ערך ההחזרה מציין כישלון

### מאקרואי שגיאות
על פי שורות 346-348 ב-`MacODBC.h`:
- `ODBC_EnvironmentError` — מנתב ל-ODBC_ErrorHandler עם ODBC_ENVIRONMENT_ERR
- `ODBC_DB_ConnectionError` — מנתב ל-ODBC_ErrorHandler עם ODBC_DB_HANDLE_ERR
- `ODBC_StatementError` — מנתב ל-ODBC_ErrorHandler עם ODBC_STATEMENT_ERR

---

## הגדרות enum

### ODBC_DatabaseProvider (`MacODBC.h:104-111`)
| ערך | משמעות |
|-----|--------|
| ODBC_NoProvider | אין ספק |
| ODBC_Informix | Informix |
| ODBC_DB2 | DB2 |
| ODBC_MS_SqlServer | MS-SQL Server |
| ODBC_Oracle | Oracle |

**הערה**: על פי ניתוח הקוד, רק Informix ו-MS-SQL מיושמים בפועל. DB2 ו-Oracle קיימים ב-enum בלבד.

### ODBC_CommandType (`MacODBC.h:117-139`)
20 ערכים הממפים את כל סוגי הפקודות הנתמכות:
`DECLARE_CURSOR`, `DECLARE_CURSOR_INTO`, `DECLARE_AND_OPEN_CURSOR`, `DECLARE_AND_OPEN_CURSOR_INTO`, `DEFERRED_INPUT_CURSOR`, `DEFERRED_INPUT_CURSOR_INTO`, `OPEN_CURSOR`, `OPEN_CURSOR_USING`, `FETCH_CURSOR`, `FETCH_CURSOR_INTO`, `CLOSE_CURSOR`, `FREE_STATEMENT`, `SINGLETON_SQL_CALL`, `COMMIT_WORK`, `ROLLBACK_WORK`, `GET_LENGTHS_READ`, `SET_DIRTY_READ`, `SET_COMMITTED_READ`, `SET_REPEATABLE_READ`, `SET_CUSTOM_SIGSEGV_HANDLER`.

### tag_bool (`MacODBC.h:145-151`)
| ערך | משמעות |
|-----|--------|
| false | 0 |
| true | 1 |

מוגן תחת `#ifndef BOOL_DEFINED`.

### ODBC_ErrorCategory (`MacODBC.h:339-344`)
| ערך | משמעות |
|-----|--------|
| ODBC_ENVIRONMENT_ERR | שגיאת סביבה |
| ODBC_DB_HANDLE_ERR | שגיאת handle חיבור |
| ODBC_STATEMENT_ERR | שגיאת statement |

---

## מבני נתונים (structs)

### ODBC_DB_HEADER (`MacODBC.h:163`)

| שדה | טיפוס | מטרה |
|------|--------|------|
| Provider | int | מזהה ספק DB (מ-enum ODBC_DatabaseProvider) |
| Connected | int | דגל מצב חיבור |
| HDBC | SQLHDBC | handle חיבור ODBC |
| Name[21] | char[21] | שם DB לאבחון |

### ODBC_ColumnParams (`MacODBC.h:164`)

| שדה | טיפוס | מטרה |
|------|--------|------|
| type | int | קוד טיפוס ODBC/C עבור binding |
| length | int | אורך עבור string/buffer binds |

---

## קבועים (#define)

על פי שורות 47-101 ב-`MacODBC.h`:

| קבוע | ערך | שורה | מטרה |
|------|-----|------|------|
| SQL_COPT_SS_PRESERVE_CURSORS | 1204 | 48 | תכונת MS-SQL לשימור סמנים |
| SQL_PC_ON | 1L | 49 | הפעלת שימור סמנים |
| SQL_PC_OFF | 0L | 50 | כיבוי שימור סמנים |
| SQL_INFX_ATTR_AUTO_FREE | 2263 | 53 | תכונת Informix auto-free |
| ENABLE_POINTER_VALIDATION | 1 | 62 | הפעלת מנגנון SIGSEGV pointer validation |
| CHECK_VALIDITY_ONLY | 0 | 63 | בדיקת קריאה בלבד |
| CHECK_POINTER_IS_WRITABLE | 1 | 64 | בדיקת קריאה וכתיבה |
| ENABLE_PREPARED_STATE_VALIDATION | 1 | 76 | הפעלת בדיקת SQLNumParams לתוקף prepared |
| ODBC_MAX_STICKY_STATEMENTS | 120 | 90 | גבול מירבי של statements דביקים |
| ODBC_MAX_PARAMETERS | 300 | 101 | גבול מירבי של פרמטרי bind/output |

---

## משתנים גלובליים

על פי שורות 168-273 ב-`MacODBC.h`, המשתנים מוגדרים תחת `#ifdef MAIN` (שורה 168) עם הצהרות `extern` תחת `#else` (שורה 242):

### כותרות מסד נתונים
| משתנה | שורה MAIN | שורה extern | טיפוס | מטרה |
|--------|-----------|------------|--------|------|
| MS_DB | 171 | 243 | ODBC_DB_HEADER | כותרת MS-SQL |
| INF_DB | 172 | 244 | ODBC_DB_HEADER | כותרת Informix |
| MAIN_DB | 173 | 245 | ODBC_DB_HEADER* | מצביע ל-DB הראשי |
| ALT_DB | 174 | 246 | ODBC_DB_HEADER* | מצביע ל-DB החלופי |

### מצב שיקוף
| משתנה | שורה MAIN | שורה extern | מטרה |
|--------|-----------|------------|------|
| ODBC_MIRRORING_ENABLED | 180 | 248 | דגל הפעלת שיקוף גלובלי |
| MIRROR_SQLCODE | 181 | 249 | קוד שגיאה מה-mirror |
| MIRROR_ROWS_AFFECTED | 182 | 250 | שורות שהושפעו ב-mirror |

### תצורת חיבור
| משתנה | שורה MAIN | שורה extern | מטרה |
|--------|-----------|------------|------|
| LOCK_TIMEOUT | 183 | 251 | timeout נעילה (MS-SQL) |
| DEADLOCK_PRIORITY | 184 | 252 | עדיפות deadlock |
| ODBC_PRESERVE_CURSORS | 175 | 254 | שימור סמנים (MS-SQL) |

### מצב pointer validation
| משתנה | שורה MAIN | שורה extern | מטרה |
|--------|-----------|------------|------|
| ODBC_ValidatingPointers | 190 | 267 | דגל: נמצאים במצב validation |
| ODBC_PointerIsValid | 193 | 268 | תוצאת בדיקה אחרונה |
| sig_act_ODBC_SegFault | 198 | 270 | מבנה sigaction |
| BeforePointerTest | 199 | 271 | sigjmp_buf לפני בדיקה |
| AfterPointerTest | 200 | 272 | sigjmp_buf אחרי בדיקה |

### משתנים פנימיים (ללא extern)
| משתנה | שורה | מטרה |
|--------|------|------|
| ODBC_henv | 170 | handle סביבת ODBC |
| ODBC_henv_allocated | 177 | דגל: env הוקצה |
| NUM_ODBC_DBS_CONNECTED | 178 | מונה DBs מחוברים |
| ALTERNATE_DB_USED | 179 | דגל: DB חלופי שימש |
| ODBC_Exec_FirstTimeCalled | 225 | דגל: קריאה ראשונה |
| NumStickyHandlesUsed | 226 | מונה handles דביקים בשימוש |
| StatementPrepared[] | 234 | מערך מצב prepared |
| StatementOpened[] | 235 | מערך מצב opened |
| StatementIsSticky[] | 236 | מערך דגלי דביקות |
| BoolType | 240 | טיפוס bool (נבדק בזמן ריצה) |

---

## תלויות (#include)

על פי שורות 31-41 ב-`MacODBC.h`:

| קובץ כותרת | שורה | מטרה |
|------------|------|------|
| `/usr/local/include/sql.h` | 31 | כותרת מערכת ODBC |
| `/usr/local/include/sqlext.h` | 32 | הרחבות ODBC |
| `<errno.h>` | 33 | קודי שגיאה |
| `<locale.h>` | 34 | הגדרות locale (עברית) |
| `<string.h>` | 35 | פונקציות מחרוזת |
| `<setjmp.h>` | 36 | sigsetjmp/siglongjmp עבור pointer validation |
| `"GenSql.h"` | 37 | תשתית SQL של הפרויקט |
| `<MacODBC_MyOperatorIDs.h>` | 41 | מזהי operators ספציפיים לכל רכיב |

### Injection includes (בתוך פונקציות)
| קובץ | שורה | הקשר |
|------|------|------|
| `"MacODBC_MyOperators.c"` | 2747 | switch cases של SQL_GetMainOperationParameters |
| `"MacODBC_MyCustomWhereClauses.c"` | 2866 | switch cases של SQL_GetWhereClauseParameters |

---

## רכיבים צורכים (8 רכיבים)

על פי סריקת include chain ב-`RESEARCH/MacODBC_deepdive.md`:

| רכיב | MacODBC_MyOperators.c | MacODBC_MyCustomWhereClauses.c |
|-------|----------------------|-------------------------------|
| FatherProcess | כן | כן |
| SqlServer | כן | כן |
| As400UnixServer | כן | כן |
| As400UnixClient | כן | כן |
| As400UnixDocServer | כן | כן |
| As400UnixDoc2Server | כן | כן |
| ShrinkPharm | כן | כן |
| GenSql | כן | לא רלוונטי |

---

## מגבלות תיעוד

### מה שלא ניתן לקבוע מהקוד:
- התנהגות DB2 ו-Oracle — קיימים ב-enum בלבד, ללא ענפי חיבור/התאמה ייעודיים
- תוכן הטבלאות שה-SQL queries פונים אליהן — ה-SQL עצמו מוגדר בקבצי MyOperators.c לכל רכיב
- ערכי אישורי DB בפועל — מועברים כפרמטרים ל-ODBC_CONNECT מ-GenSql.c
- התנהגות חיצונית של `SQLMD_connect` ו-`SQLMD_disconnect` (מוגדרים ב-GenSql.c)

### מה שכן ידוע מהקוד:
- מבנה מלא של הקובץ (4121 שורות, 11 פונקציות)
- 25 מאקרואים ציבוריים ומיפוי CommandType שלהם
- 4 הגדרות enum (DatabaseProvider, CommandType, tag_bool, ErrorCategory)
- 2 מבני נתונים (ODBC_DB_HEADER, ODBC_ColumnParams)
- כל המשתנים הגלובליים עם דואליות MAIN/extern
- מנגנון שיקוף DB, cache statements דביקים, pointer validation
- 8 רכיבים הצורכים את MacODBC.h

---

*נוצר על ידי סוכן המתעד של CIDRA — DOC-MACODBC-001*
