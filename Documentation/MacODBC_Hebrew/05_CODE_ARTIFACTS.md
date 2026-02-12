# MacODBC.h - קטעי קוד מרכזיים

**רכיב**: MacODBC (שכבת תשתית ODBC)
**מזהה משימה**: DOC-MACODBC-001
**תאריך יצירה**: 2026-02-11

---

## 1. API מאקרואי — דפוס הנתוב

על פי `MacODBC.h:277-278` ושורות 311-335, כל מאקרו ציבורי מנתב ל-ODBC_Exec:

```c
// MacODBC.h:277-278 — מאקרואים שירותיים
#define SQL_WORKED(R)  (R >= 0)
#define SQL_FAILED(R)  (R < 0)

// MacODBC.h:311-335 — דוגמאות מאקרואי API (25 סה"כ)
#define DeclareCursor(db, op_id, ...)           ODBC_Exec(db, DECLARE_CURSOR, op_id, __VA_ARGS__)
#define DeclareAndOpenCursorInto(db, op_id, ...) ODBC_Exec(db, DECLARE_AND_OPEN_CURSOR_INTO, op_id, __VA_ARGS__)
#define ExecSQL(db, op_id, ...)                 ODBC_Exec(db, SINGLETON_SQL_CALL, op_id, __VA_ARGS__)
#define ExecSql(db, op_id, ...)                 ODBC_Exec(db, SINGLETON_SQL_CALL, op_id, __VA_ARGS__)
#define CommitWork(db)                          ODBC_Exec(db, COMMIT_WORK, 0)
#define RollbackWork(db)                        ODBC_Exec(db, ROLLBACK_WORK, 0)
#define RollBackWork(db)                        ODBC_Exec(db, ROLLBACK_WORK, 0)
#define CommitAllWork(db)                       ODBC_Exec(db, COMMIT_WORK, -1)
```

**הערה**: על פי שורות 318-319, `ExecSQL` ו-`ExecSql` זהים (תאימות לאחור). על פי שורות 324-325, `RollbackWork` ו-`RollBackWork` זהים.

---

## 2. מאקרואי שגיאות

על פי `MacODBC.h:339-348`:

```c
// MacODBC.h:339-344 — enum קטגוריות שגיאה
enum ODBC_ErrorCategory {
    ODBC_ENVIRONMENT_ERR,
    ODBC_DB_HANDLE_ERR,
    ODBC_STATEMENT_ERR
};

// MacODBC.h:346-348 — מאקרואי נתוב שגיאות
#define ODBC_EnvironmentError(...)   ODBC_ErrorHandler(ODBC_ENVIRONMENT_ERR, __VA_ARGS__)
#define ODBC_DB_ConnectionError(...) ODBC_ErrorHandler(ODBC_DB_HANDLE_ERR, __VA_ARGS__)
#define ODBC_StatementError(...)     ODBC_ErrorHandler(ODBC_STATEMENT_ERR, __VA_ARGS__)
```

---

## 3. switch פענוח פקודות — ODBC_Exec

על פי `MacODBC.h:634-763`, ה-switch המרכזי ממפה CommandType לדגלי התנהגות. דוגמת cases:

```c
// MacODBC.h:634-763 — switch מרכזי (מקוצר)
switch (CommandType_in)
{
    case DECLARE_CURSOR:
        NeedToPrepare       = 1;
        NeedToInterpretOp   = 1;
        CurrentMacroName    = "DeclareCursor";
        break;

    case DECLARE_AND_OPEN_CURSOR:
        NeedToPrepare       = 1;
        NeedToExecute       = 1;
        NeedToInterpretOp   = 1;
        CurrentMacroName    = "DeclareAndOpenCursor";
        break;

    case SINGLETON_SQL_CALL:
        NeedToPrepare       = 1;
        NeedToExecute       = 1;
        NeedToFetch         = 1;
        NeedToInterpretOp   = 1;
        CurrentMacroName    = "ExecSQL";
        break;

    case COMMIT_WORK:
        NeedToCommit        = 1;
        CurrentMacroName    = "CommitWork";
        break;

    case ROLLBACK_WORK:
        NeedToRollBack      = 1;
        CurrentMacroName    = "RollbackWork";
        break;

    // ... 15 cases נוספים ...
}
```

---

## 4. אתחול קריאה ראשונה — התקנת SIGSEGV handler

על פי `MacODBC.h:817-831`:

```c
// MacODBC.h:817-831 — התקנת signal handler
sig_act_ODBC_SegFault.sa_handler = SIGSEGV_Handler;
sigemptyset(&sig_act_ODBC_SegFault.sa_mask);
sig_act_ODBC_SegFault.sa_flags = 0;

if (sigaction(SIGSEGV, &sig_act_ODBC_SegFault, NULL) != 0)
{
    GerrLogMini("MacODBC ODBC_Exec: sigaction failed for SIGSEGV");
}
```

על פי `MacODBC.h:419`:
```c
// MacODBC.h:419 — function pointer ברירת מחדל
static void (*SIGSEGV_Handler)(int) = macODBC_SegmentationFaultCatcher;
```

---

## 5. בדיקת גודל bool בזמן ריצה

על פי `MacODBC.h:836-845`:

```c
// MacODBC.h:836-845 — בדיקת sizeof(bool)
if (sizeof(bool) == sizeof(int))
    BoolType = SQL_C_SLONG;
else if (sizeof(bool) == sizeof(short))
    BoolType = SQL_C_SSHORT;
else
    BoolType = SQL_C_UTINYINT;  // sizeof(bool) == 1
```

**הסבר**: מהדרי C שונים ממפים `bool` לגדלים שונים (1, 2, או 4 bytes). מנגנון זה מזהה בזמן ריצה ומגדיר את טיפוס ה-ODBC binding המתאים.

---

## 6. גבולות הזרקת Include

### MacODBC_MyOperators.c — שורה 2747

על פי `MacODBC.h:2745-2748`:

```c
// MacODBC.h:2745-2748 — בתוך SQL_GetMainOperationParameters
    switch(OperationIdentifier)
    {
        #include "MacODBC_MyOperators.c"
    }
```

כל רכיב מספק את קובץ ה-`MacODBC_MyOperators.c` שלו עם switch cases ספציפיים. דוגמה מ-FatherProcess:

```c
// source_code/FatherProcess/MacODBC_MyOperators.c (קטע)
case OPER_SELECT_SETUP_NEW:
    SQL_CommandText = "SELECT ... FROM setup_new WHERE ...";
    SQL_OutputColumnSpec = "CHAR(30), INT, CHAR(30)";
    IsSticky = 1;
    break;
```

### MacODBC_MyCustomWhereClauses.c — שורה 2866

על פי `MacODBC.h:2864-2867`:

```c
// MacODBC.h:2864-2867 — בתוך SQL_GetWhereClauseParameters
    switch(WhereClauseIdentifier)
    {
        #include "MacODBC_MyCustomWhereClauses.c"
    }
```

---

## 7. מנגנון SIGSEGV Pointer Validation

### ODBC_IsValidPointer — שורות 4013-4070

על פי `MacODBC.h:4026-4052`:

```c
// MacODBC.h:4026-4052 — בדיקת מצביע באמצעות sigsetjmp
ODBC_PointerIsValid = 1;

if (sigsetjmp(BeforePointerTest, 1) == 0)
{
    // ניסיון ראשון — נסה לקרוא byte מהמצביע
    char TestByte = *((char *)Pointer_in);

    if (CheckPointerWritable_in)
    {
        // נסה לכתוב חזרה
        *((char *)Pointer_in) = TestByte;
    }
}
else
{
    // הגענו לכאן רק דרך siglongjmp מה-SIGSEGV handler
    ODBC_PointerIsValid = 0;
    // ... ErrDesc_out = "not readable" / "not writable"
}
```

### macODBC_SegmentationFaultCatcher — שורות 4073-4118

על פי `MacODBC.h:4086-4106`:

```c
// MacODBC.h:4086-4106 — SIGSEGV handler דו-מצבי
void macODBC_SegmentationFaultCatcher(int sig)
{
    if (ODBC_ValidatingPointers)
    {
        // מצב validation — קפוץ חזרה בבטחה
        ODBC_PointerIsValid = 0;
        sigaction(SIGSEGV, &sig_act_ODBC_SegFault, NULL); // re-install
        siglongjmp(BeforePointerTest, 1);
    }
    else
    {
        // מצב רגיל — segfault fatal
        GerrLogMini("MacODBC: Segmentation fault!");
        RollbackAllWork(MAIN_DB);
        SQLMD_disconnect();
        ExitSonProcess(MAC_SERV_SHUT_DOWN);
    }
}
```

---

## 8. Mirror Execute — קודם לראשי

על פי `MacODBC.h:2029-2094`:

```c
// MacODBC.h:2029-2094 — הרצת mirror (מקוצר)
if (CommandIsMirrored)
{
    // Mirror מבוצע קודם!
    MirrorReturnCode = SQLExecute(MirrorStatementPtr);

    if (MirrorReturnCode == SQL_SUCCESS || MirrorReturnCode == SQL_SUCCESS_WITH_INFO)
    {
        ALTERNATE_DB_USED = 1;
        SQLRowCount(MirrorStatementPtr, &MIRROR_ROWS_AFFECTED);
    }
    else
    {
        // כישלון mirror — נרשם אך לא fatal
        ODBC_StatementError(...);
    }
}

// Primary execute
ReturnCode = SQLExecute(ThisStatementPtr);
```

---

## 9. Commit ברמת ENV vs DBC

על פי `MacODBC.h:2396-2446`:

```c
// MacODBC.h:2396-2446 — CommitAllWork logic (מקוצר)
if (OperationIdentifier == -1)  // CommitAllWork
{
    if (ALTERNATE_DB_USED)
    {
        // שני DBs שימשו — commit ברמת ENV (מכסה את שניהם)
        SQLEndTran(SQL_HANDLE_ENV, ODBC_henv, SQL_COMMIT);
    }
    else
    {
        // רק MAIN_DB שימש — commit ברמת DBC (אופטימיזציה)
        SQLEndTran(SQL_HANDLE_DBC, MAIN_DB->HDBC, SQL_COMMIT);
    }
}
else  // CommitWork רגיל
{
    SQLEndTran(SQL_HANDLE_DBC, Database->HDBC, SQL_COMMIT);
}
```

---

## 10. החלפת טוקנים — SQL_CustomizePerDB

על פי `MacODBC.h:2948-2986` (דוגמת MODULO):

```c
// MacODBC.h:2948-2986 — החלפת {MODULO} (מקוצר)
if (strcasestr(SQL_CmdBuffer, "{MODULO}") || strcasestr(SQL_CmdBuffer, "{MOD}"))
{
    if (Database->Provider == ODBC_Informix)
    {
        // Informix: MOD(A,B)
        // parsing with strtok_r to extract A and B
        // replace {MODULO}(A,B) with MOD(A,B)
    }
    else
    {
        // MS-SQL: (A % B)
        // replace {MODULO}(A,B) with (A % B)
    }
}
```

---

## 11. ODBC_CONNECT — רצף חיבור

על פי `MacODBC.h:3476-3552` (מקוצר):

```c
// MacODBC.h:3476-3488 — אתחול ראשון
MS_DB.Provider = ODBC_MS_SqlServer;
INF_DB.Provider = ODBC_Informix;
setlocale(LC_ALL, "he_IL.UTF-8");

// MacODBC.h:3494-3505 — הקצאת environment
SQLAllocEnv(&ODBC_henv);

// MacODBC.h:3513-3536 — הקצאת חיבור + timeouts
SQLAllocConnect(ODBC_henv, &Database->HDBC);
SQLSetConnectAttr(Database->HDBC, SQL_ATTR_LOGIN_TIMEOUT, ...);
SQLSetConnectAttr(Database->HDBC, SQL_ATTR_CONNECTION_TIMEOUT, ...);

// MacODBC.h:3552 — חיבור
SQLConnect(Database->HDBC, DSN, SQL_NTS, username, SQL_NTS, password, SQL_NTS);
```

---

## 12. המרת שגיאות Auto-Reconnect

על פי `MacODBC.h:3936-3959`:

```c
// MacODBC.h:3936-3959 — המרת שגיאות לreconnect
case SQL_TRN_CLOSED_BY_OTHER_SESSION:
case DB_TCP_PROVIDER_ERROR:
case TABLE_SCHEMA_CHANGED:
case DB_CONNECTION_BROKEN:
    GerrLogMini("ODBC_ErrorHandler: Connection error, triggering reconnect");
    SQLMD_disconnect();
    ODBC_Exec_FirstTimeCalled = 1;  // force re-init on next call
    ODBC_NativeError = DB_CONNECTION_BROKEN;
    break;
```

---

## 13. מבני נתונים

על פי `MacODBC.h:163-164`:

```c
// MacODBC.h:163
typedef struct {
    int     Provider;       // ODBC_DatabaseProvider enum
    int     Connected;      // 0=disconnected, 1=connected
    SQLHDBC HDBC;           // ODBC connection handle
    char    Name[21];       // diagnostic DB name
} ODBC_DB_HEADER;

// MacODBC.h:164
typedef struct {
    int type;               // ODBC/C type code
    int length;             // length for string/buffer binds
} ODBC_ColumnParams;
```

---

## הערות אבטחה

- על פי `MacODBC.h:3444-3448`, אישורי DB מועברים ל-ODBC_CONNECT כטקסט רגיל
- מיקום מתועד אך ערכים אינם מועתקים לתיעוד
- על פי `MacODBC.h:3550`, קיימת הערה מוערת עם format של credentials בטקסט גלוי

---

*נוצר על ידי סוכן המתעד של CIDRA — DOC-MACODBC-001*
