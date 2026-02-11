# MacODBC.h - Key Code Artifacts

**Component**: MacODBC (ODBC Infrastructure Layer)
**Task ID**: DOC-MACODBC-002
**Date Created**: 2026-02-11

---

## 1. API Macros — Routing Pattern

According to `MacODBC.h:277-278` and lines 311-335, every public macro routes to ODBC_Exec:

```c
// MacODBC.h:277-278 — Utility macros
#define SQL_WORKED(R)  (R >= 0)
#define SQL_FAILED(R)  (R < 0)

// MacODBC.h:311-335 — API macro examples (25 total)
#define DeclareCursor(db, op_id, ...)           ODBC_Exec(db, DECLARE_CURSOR, op_id, __VA_ARGS__)
#define DeclareAndOpenCursorInto(db, op_id, ...) ODBC_Exec(db, DECLARE_AND_OPEN_CURSOR_INTO, op_id, __VA_ARGS__)
#define ExecSQL(db, op_id, ...)                 ODBC_Exec(db, SINGLETON_SQL_CALL, op_id, __VA_ARGS__)
#define ExecSql(db, op_id, ...)                 ODBC_Exec(db, SINGLETON_SQL_CALL, op_id, __VA_ARGS__)
#define CommitWork(db)                          ODBC_Exec(db, COMMIT_WORK, 0)
#define RollbackWork(db)                        ODBC_Exec(db, ROLLBACK_WORK, 0)
#define RollBackWork(db)                        ODBC_Exec(db, ROLLBACK_WORK, 0)
#define CommitAllWork(db)                       ODBC_Exec(db, COMMIT_WORK, -1)
```

**Note**: According to lines 318-319, `ExecSQL` and `ExecSql` are identical (backward compatibility). According to lines 324-325, `RollbackWork` and `RollBackWork` are identical.

---

## 2. Error Macros

According to `MacODBC.h:339-348`:

```c
// MacODBC.h:339-344 — Error category enum
enum ODBC_ErrorCategory {
    ODBC_ENVIRONMENT_ERR,
    ODBC_DB_HANDLE_ERR,
    ODBC_STATEMENT_ERR
};

// MacODBC.h:346-348 — Error routing macros
#define ODBC_EnvironmentError(...)   ODBC_ErrorHandler(ODBC_ENVIRONMENT_ERR, __VA_ARGS__)
#define ODBC_DB_ConnectionError(...) ODBC_ErrorHandler(ODBC_DB_HANDLE_ERR, __VA_ARGS__)
#define ODBC_StatementError(...)     ODBC_ErrorHandler(ODBC_STATEMENT_ERR, __VA_ARGS__)
```

---

## 3. Command Decode Switch — ODBC_Exec

According to `MacODBC.h:634-763`, the central switch maps CommandType to behavior flags. Sample cases:

```c
// MacODBC.h:634-763 — Central switch (abbreviated)
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

    // ... 15 additional cases ...
}
```

---

## 4. First-Call Initialization — SIGSEGV Handler Installation

According to `MacODBC.h:817-831`:

```c
// MacODBC.h:817-831 — Signal handler installation
sig_act_ODBC_SegFault.sa_handler = SIGSEGV_Handler;
sigemptyset(&sig_act_ODBC_SegFault.sa_mask);
sig_act_ODBC_SegFault.sa_flags = 0;

if (sigaction(SIGSEGV, &sig_act_ODBC_SegFault, NULL) != 0)
{
    GerrLogMini("MacODBC ODBC_Exec: sigaction failed for SIGSEGV");
}
```

According to `MacODBC.h:419`:
```c
// MacODBC.h:419 — Default function pointer
static void (*SIGSEGV_Handler)(int) = macODBC_SegmentationFaultCatcher;
```

---

## 5. Runtime Bool Size Check

According to `MacODBC.h:836-845`:

```c
// MacODBC.h:836-845 — sizeof(bool) check
if (sizeof(bool) == sizeof(int))
    BoolType = SQL_C_SLONG;
else if (sizeof(bool) == sizeof(short))
    BoolType = SQL_C_SSHORT;
else
    BoolType = SQL_C_UTINYINT;  // sizeof(bool) == 1
```

**Explanation**: Different C compilers map `bool` to different sizes (1, 2, or 4 bytes). This mechanism detects the size at runtime and sets the appropriate ODBC binding type.

---

## 6. Include Injection Boundaries

### MacODBC_MyOperators.c — Line 2747

According to `MacODBC.h:2745-2748`:

```c
// MacODBC.h:2745-2748 — Inside SQL_GetMainOperationParameters
    switch(OperationIdentifier)
    {
        #include "MacODBC_MyOperators.c"
    }
```

Each component provides its own `MacODBC_MyOperators.c` file with component-specific switch cases. Example from FatherProcess:

```c
// source_code/FatherProcess/MacODBC_MyOperators.c (excerpt)
case OPER_SELECT_SETUP_NEW:
    SQL_CommandText = "SELECT ... FROM setup_new WHERE ...";
    SQL_OutputColumnSpec = "CHAR(30), INT, CHAR(30)";
    IsSticky = 1;
    break;
```

### MacODBC_MyCustomWhereClauses.c — Line 2866

According to `MacODBC.h:2864-2867`:

```c
// MacODBC.h:2864-2867 — Inside SQL_GetWhereClauseParameters
    switch(WhereClauseIdentifier)
    {
        #include "MacODBC_MyCustomWhereClauses.c"
    }
```

---

## 7. SIGSEGV Pointer Validation Mechanism

### ODBC_IsValidPointer — Lines 4013-4070

According to `MacODBC.h:4026-4052`:

```c
// MacODBC.h:4026-4052 — Pointer validation via sigsetjmp
ODBC_PointerIsValid = 1;

if (sigsetjmp(BeforePointerTest, 1) == 0)
{
    // First attempt — try to read a byte from the pointer
    char TestByte = *((char *)Pointer_in);

    if (CheckPointerWritable_in)
    {
        // Try to write back
        *((char *)Pointer_in) = TestByte;
    }
}
else
{
    // We got here only via siglongjmp from the SIGSEGV handler
    ODBC_PointerIsValid = 0;
    // ... ErrDesc_out = "not readable" / "not writable"
}
```

### macODBC_SegmentationFaultCatcher — Lines 4073-4118

According to `MacODBC.h:4086-4106`:

```c
// MacODBC.h:4086-4106 — Dual-mode SIGSEGV handler
void macODBC_SegmentationFaultCatcher(int sig)
{
    if (ODBC_ValidatingPointers)
    {
        // Validation mode — jump back safely
        ODBC_PointerIsValid = 0;
        sigaction(SIGSEGV, &sig_act_ODBC_SegFault, NULL); // re-install
        siglongjmp(BeforePointerTest, 1);
    }
    else
    {
        // Normal mode — segfault is fatal
        GerrLogMini("MacODBC: Segmentation fault!");
        RollbackAllWork(MAIN_DB);
        SQLMD_disconnect();
        ExitSonProcess(MAC_SERV_SHUT_DOWN);
    }
}
```

---

## 8. Mirror Execute — Before Primary

According to `MacODBC.h:2029-2094`:

```c
// MacODBC.h:2029-2094 — Mirror execution (abbreviated)
if (CommandIsMirrored)
{
    // Mirror executes first!
    MirrorReturnCode = SQLExecute(MirrorStatementPtr);

    if (MirrorReturnCode == SQL_SUCCESS || MirrorReturnCode == SQL_SUCCESS_WITH_INFO)
    {
        ALTERNATE_DB_USED = 1;
        SQLRowCount(MirrorStatementPtr, &MIRROR_ROWS_AFFECTED);
    }
    else
    {
        // Mirror failure — logged but not fatal
        ODBC_StatementError(...);
    }
}

// Primary execute
ReturnCode = SQLExecute(ThisStatementPtr);
```

---

## 9. Commit at ENV vs DBC Level

According to `MacODBC.h:2396-2446`:

```c
// MacODBC.h:2396-2446 — CommitAllWork logic (abbreviated)
if (OperationIdentifier == -1)  // CommitAllWork
{
    if (ALTERNATE_DB_USED)
    {
        // Both DBs were used — commit at ENV level (covers both)
        SQLEndTran(SQL_HANDLE_ENV, ODBC_henv, SQL_COMMIT);
    }
    else
    {
        // Only MAIN_DB was used — commit at DBC level (optimization)
        SQLEndTran(SQL_HANDLE_DBC, MAIN_DB->HDBC, SQL_COMMIT);
    }
}
else  // Regular CommitWork
{
    SQLEndTran(SQL_HANDLE_DBC, Database->HDBC, SQL_COMMIT);
}
```

---

## 10. Token Replacement — SQL_CustomizePerDB

According to `MacODBC.h:2948-2986` (MODULO example):

```c
// MacODBC.h:2948-2986 — {MODULO} replacement (abbreviated)
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

## 11. ODBC_CONNECT — Connection Sequence

According to `MacODBC.h:3476-3552` (abbreviated):

```c
// MacODBC.h:3476-3488 — First-time initialization
MS_DB.Provider = ODBC_MS_SqlServer;
INF_DB.Provider = ODBC_Informix;
setlocale(LC_ALL, "he_IL.UTF-8");

// MacODBC.h:3494-3505 — Environment allocation
SQLAllocEnv(&ODBC_henv);

// MacODBC.h:3513-3536 — Connection allocation + timeouts
SQLAllocConnect(ODBC_henv, &Database->HDBC);
SQLSetConnectAttr(Database->HDBC, SQL_ATTR_LOGIN_TIMEOUT, ...);
SQLSetConnectAttr(Database->HDBC, SQL_ATTR_CONNECTION_TIMEOUT, ...);

// MacODBC.h:3552 — Connect
SQLConnect(Database->HDBC, DSN, SQL_NTS, username, SQL_NTS, password, SQL_NTS);
```

---

## 12. Auto-Reconnect Error Conversion

According to `MacODBC.h:3936-3959`:

```c
// MacODBC.h:3936-3959 — Error conversion to reconnect
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

## 13. Data Structures

According to `MacODBC.h:163-164`:

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

## Security Notes

- According to `MacODBC.h:3444-3448`, DB credentials are passed to ODBC_CONNECT as plain text
- Locations are documented but values are not copied to documentation
- According to `MacODBC.h:3550`, there is a commented-out line with a clear-text credential format

---

*Generated by the CIDRA Documenter Agent — DOC-MACODBC-002*
