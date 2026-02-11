# MacODBC.h - Detailed Technical Analysis

**Component**: MacODBC (ODBC Infrastructure Layer)
**Task ID**: DOC-MACODBC-002
**Date Created**: 2026-02-11

---

## ODBC_Exec Analysis — 8 Phases

### Phase 1: Command Decode & Initialization (lines 422-772)

According to `MacODBC.h:446-450`, the function is declared with variadic args:
```c
int ODBC_Exec(ODBC_DB_HEADER *Database, int CommandType_in, ...)
```

This phase appears to include:
- Local variable declarations including cache arrays (StatementCache, MirrorStatementCache) sized to `ODBC_MAXIMUM_OPERATION`, as seen in lines 452-531
- `va_start` and initialization of `FieldLengthsRead` and `ColumnParameterPointers` via memset (lines 569-583)
- Setting `ODBC_AvoidRecursion=1` (line 579) to prevent recursion
- Central switch (lines 634-763) mapping each `ODBC_CommandType` value to behavior flags:
  - `NeedToPrepare`, `NeedToExecute`, `NeedToFetch`
  - `NeedToInterpretOp`, `NeedToCommit`, `NeedToRollBack`
  - `InterpretOutputOnly` (for FetchCursorInto mode)
- Validation of `OperationIdentifier` (lines 765-772)

### Phase 2: First-Call Initialization (lines 775-887)

According to `MacODBC.h:780`, this is conditional on `ODBC_Exec_FirstTimeCalled`:
- memset of all cache arrays (StatementCache, MirrorStatementCache, Statement_DB_Provider) — lines 791-801
- Counter and flag resets (lines 803-811)
- SIGSEGV signal handler installation via `sigaction` with `SIGSEGV_Handler` (lines 817-831)
- Runtime bool size check: `sizeof(bool)` determines `BoolType` for ODBC binding (lines 836-845). This is needed because different C compilers map bool to different sizes (1 vs 4 bytes).

After the init block:
- Activity counter for `INS_messages_details` (lines 851-852)
- Diagnostic and forced FreeStatement for prepared statements that remain leaked (lines 866-874)

### Phase 3: Operation Metadata Loading (lines 888-1072)

Start of do-while(0) dummy loop (line 888). When `NeedToInterpretOp`:
- Call to `SQL_GetMainOperationParameters` (lines 899-913) — returns: SQL text, column specs, stickiness, mirroring flag, cursor name
- Mirroring logic: `CommandIsMirrored` is set only when `ODBC_MIRRORING_ENABLED` (line 918)
- Dynamic SQL text from va_arg when `SQL_CommandText==NULL` (lines 953-964)
- Call to `find_FOR_UPDATE_or_GEN_VALUES` (lines 975-980) — detects INSERT/SELECT/FOR UPDATE
- `CommandIsMirrored=0` for SELECTs (lines 994-996)
- `MIRROR_DB` computed as the opposite of the current Database (line 1002)
- WHERE clause handling: `WhereClauseIdentifier` from va_arg (line 1021), call to `SQL_GetWhereClauseParameters` (lines 1030-1034)

### Phase 4: Sticky Statement Lifecycle (lines 1075-1231)

Inside `if(NeedToPrepare)` block (line 1086):
- **Provider switch**: if the provider changed since last use, releases old handle and mirror cache, decrements `NumStickyHandlesUsed` (lines 1095-1131)
- **Admission control**: if `NumStickyHandlesUsed >= ODBC_MAX_STICKY_STATEMENTS` (120), cancels stickiness with diagnostic (lines 1145-1155); otherwise increments counter (line 1158)
- **Prepared-state validation**: calls `SQLNumParams` when `ENABLE_PREPARED_STATE_VALIDATION=1` (line 1182); on failure, releases handle and forces re-PREPARE (lines 1195-1202)
- **New statement allocation**: `SQLAllocStmt` for primary DB (line 1213); if `CommandIsMirrored`, also allocates for mirror (line 1224)

### Phase 5: SQL Construction & PREPARE (lines 1233-1447)

- Assembly of `SQL_CmdBuffer` from `SQL_CommandText` + optional `WhereClauseText` (lines 1269-1277)
- Automatic VALUES clause assembly for INSERTs (lines 1284-1295)
- Call to `SQL_CustomizePerDB` for provider-specific token replacement (line 1300)
- FOR UPDATE cursor setup: `SQLSetStmtAttr` for concurrency locking + automatic FOR UPDATE addition (lines 1314-1336)
- `SQLPrepare` of the assembled SQL (line 1355); on failure: disconnect via `SQLMD_disconnect`, reset `ODBC_Exec_FirstTimeCalled`, return `DB_CONNECTION_BROKEN` (lines 1377-1392) — **this is the auto-reconnect trigger**
- Mirror PREPARE on alternate DB (lines 1402-1417)
- `SQLSetCursorName` when `CursorName != NULL` (lines 1421-1433)

### Phase 6: Pointer Validation & Binding (lines 1449-1976)

**Part A — Pointer collection with SIGSEGV validation** (lines 1449-1651):
- Sets `ODBC_ValidatingPointers=1` (line 1456)
- Output pointer loop: extracts va_arg pointers, skips WHERE clause ID and dynamic SQL for FetchCursorInto (lines 1479-1519), validates via `ODBC_IsValidPointer` with `CHECK_POINTER_IS_WRITABLE` (line 1524)
- Input pointer loop: validates with `CHECK_VALIDITY_ONLY` (line 1560)
- WHERE clause pointer loop: validates (line 1595)
- Resets `ODBC_ValidatingPointers` (line 1618)
- Checks `END_OF_ARG_LIST` sentinel (lines 1638-1649)

**Part B — ODBC binding** (lines 1661-1976):
- `SQLBindCol` loop for output columns (lines 1679-1781): type normalization (VARCHAR→CHAR, single-char→UTINYINT), bind-length calculation, FieldLengthsRead array
- `SQLBindParameter` loop for input columns (lines 1787-1896): C-to-SQL type mapping, long string truncation (lines 1820-1835), mirror binding
- `SQLBindParameter` loop for WHERE clause with offset NumInputColumns + mirror (lines 1902-1975)

### Phase 7: Execute, Mirror, Fetch (lines 1978-2288)

**Execute** (NeedToExecute, line 1981):
- Pre-execute check: returns `DB_CONNECTION_BROKEN` if `Database->Connected==0` (lines 1990-1993)
- **Mirror execute**: `SQLExecute` on `MirrorStatementPtr` **first** (line 2029); error switch (lines 2047-2079); on success: `ALTERNATE_DB_USED=1` + `SQLRowCount` (lines 2089-2094)
- **Primary execute**: `SQLExecute` on `ThisStatementPtr` (line 2100); error switch for sticky statements (lines 2133-2183); on success: `StatementOpened=1` (line 2206), `SQLRowCount` (lines 2212-2213)
- Negative rows-affected fix (lines 2232-2240)
- Silent INSERT failure detection in MS-SQL (lines 2252-2261)
- Mirror result merging: uses lower of two rows-affected values, switches to MIRROR_SQLCODE if non-zero (lines 2266-2284)

**Fetch** (NeedToFetch, line 2302):
- `SQLFetchScroll SQL_FETCH_NEXT` (line 2328)
- `Convert_NF_to_zero` logic (lines 2333-2337)
- Fetch error handling with sticky cleanup (lines 2340-2372)
- Cardinality check via second SQLFetchScroll (lines 2385-2392)

### Phase 8: Transaction, Close, Return (lines 2290-2657)

**Commit** (NeedToCommit, lines 2396-2446):
- CommitAllWork: if `ALTERNATE_DB_USED` then `SQLEndTran` at ENV level, otherwise at DBC level for MAIN_DB only
- Regular CommitWork: `SQLEndTran` at DBC level

**Rollback** (NeedToRollBack, lines 2449-2490):
- Similar logic to commit (ENV vs DBC) without the `ALTERNATE_DB_USED` optimization

**Isolation level** (lines 2493-2529):
- SetDirtyRead/SetCommittedRead/SetRepeatableRead via `SQLSetConnectAttr` (skips Informix)

**Close** (lines 2539-2606):
- Sticky statements: `SQLCloseCursor` soft close + mirror close (lines 2590-2597)
- Non-sticky: `NeedToFreeStatement=1` (line 2604)

**Free** (lines 2609-2639):
- `SQLFreeHandle` + mirror free, sticky counter decrement, `StatementPrepared`/`Opened` reset

**Cleanup** (lines 2642-2657):
- `va_end`, `ColumnOutputLengths` assignment, reset of `ODBC_AvoidRecursion` and `ODBC_ValidatingPointers`, return `SQLCODE`

---

## Helper Function Analysis

### SQL_GetMainOperationParameters (lines 2661-2817)

This function appears to decode `OperationIdentifier` to SQL metadata:
- `SameOpAsLastTime` optimization to skip re-parsing (line 2713)
- Central switch at line 2745 includes `#include "MacODBC_MyOperators.c"` (line 2747) — **include injection cross-reference boundary**
- Calls `ParseColumnList` for output and input column specs (lines 2760, 2765)
- `Convert_NF_to_zero` validation (only for single INT output) (lines 2775-2778)
- Copies results to 14 output parameters (lines 2782-2812)

### SQL_GetWhereClauseParameters (lines 2821-2902)

This function appears to decode `WhereClauseIdentifier`:
- `SameOpAsLastTime` optimization (line 2843)
- Central switch at line 2864 includes `#include "MacODBC_MyCustomWhereClauses.c"` (line 2866) — **include injection cross-reference boundary**
- Calls `ParseColumnList` (line 2879)
- Copies results to 4 output parameters (lines 2886-2898)

### SQL_CustomizePerDB (lines 2906-3136)

This function appears to perform provider-specific SQL token replacement:

| Token | Informix | MS-SQL | Lines |
|-------|----------|--------|-------|
| {MODULO}/{MOD} | MOD(A,B) | (A % B) | 2948-2986 |
| {TOP}/{FIRST} | FIRST | TOP | 2990-3019 |
| {TRANSACTION} | TRANSACTION | "TRANSACTION" (escaped) | 3064-3093 |
| {GROUP} | GROUP | "GROUP" (escaped) | 3098-3127 |

Uses `strcasestr` for case-insensitive search (requires `_GNU_SOURCE`).
Note: `{ROWID}` support is commented out (lines 3021-3055) — Oracle-specific, not completed.

### ParseColumnList (lines 3140-3330)

Parses a column-spec string (e.g., `"INT, CHAR(20), SHORT"`) to an `ODBC_ColumnParams` array. According to the code, 12 C types are supported:

| String Type | ODBC Type | Notes |
|-------------|-----------|-------|
| INT/INTEGER | SQL_C_SLONG | |
| SHORT/SMALLINT | SQL_C_SSHORT | |
| CHAR(n) | SQL_C_CHAR | With length |
| VARCHAR(n) | SQL_VARCHAR | With length |
| DOUBLE/FLOAT | SQL_C_DOUBLE | |
| LONG/BIGINT | SQL_C_SBIGINT | |
| BOOL/BOOLEAN | BoolType | Determined at runtime |
| UNSIGNED | SQL_C_ULONG | |
| USHORT | SQL_C_USHORT | |
| REAL | SQL_C_FLOAT | |
| SINGLE_CHAR/ONECHAR | SQL_C_CHAR length 0 | Special handling |

### find_FOR_UPDATE_or_GEN_VALUES (lines 3334-3439)

Scans SQL to detect: whether it is an INSERT (lines 3377-3380), whether it is a SELECT (lines 3382-3385), whether it contains FOR UPDATE (lines 3403-3416), and whether there is a custom WHERE insertion point (%s, line 3392).

### ODBC_CONNECT (lines 3443-3794)

Sets up a full connection:
- First-time initialization: sets MS_DB (ODBC_MS_SqlServer) and INF_DB (ODBC_Informix) (lines 3476-3482)
- `setlocale` for Hebrew (line 3488)
- `SQLAllocEnv` (lines 3494-3505)
- `SQLAllocConnect` + timeout (lines 3513-3528)
- MS-SQL: preserve-cursors (lines 3542-3547), `USE <dbname>` (lines 3630-3656), `SET LOCK_TIMEOUT` (lines 3670-3689), `SET DEADLOCK_PRIORITY` (lines 3702-3721)
- Informix: disable AUTOFREE (lines 3752-3761)
- AUTOCOMMIT off (line 3729), dirty-read isolation default (line 3742)

### CleanupODBC (lines 3798-3819)

`SQLDisconnect` + `SQLFreeConnect`, `Connected=0`. Decrements `NUM_ODBC_DBS_CONNECTED`; if last, also calls `SQLFreeEnv` (lines 3812-3814).

### ODBC_ErrorHandler (lines 3823-4004)

Central error handler:
- Dispatch on `ErrorCategory` (lines 3847-3884)
- **Synthetic access-conflict conversion**: native errors -11103, -11031, -211 are converted to `SQLERR_CANNOT_POSITION_WITHIN_TABL` (-243) — lines 3903-3926
- **Auto-reconnect conversion**: `SQL_TRN_CLOSED_BY_OTHER_SESSION`, `DB_TCP_PROVIDER_ERROR`, `TABLE_SCHEMA_CHANGED`, `DB_CONNECTION_BROKEN` → calls `SQLMD_disconnect`, resets `ODBC_Exec_FirstTimeCalled`, converts to `DB_CONNECTION_BROKEN` — lines 3936-3959

---

## Pointer Validation Mechanism (SIGSEGV)

An unusual pattern — validates va_arg pointers by deliberately attempting access inside a `sigsetjmp`/`siglongjmp` block:

### ODBC_IsValidPointer (lines 4013-4070)
1. `sigsetjmp(BeforePointerTest)` (line 4027) — saves state
2. Attempts to read a byte from the pointer (line 4032)
3. Optionally: writes back if `CheckPointerWritable_in` (line 4041)
4. If SIGSEGV fires: the handler sets `ODBC_PointerIsValid=0` and performs `siglongjmp` back (lines 4046-4052)
5. Returns `ODBC_PointerIsValid` + `ErrDesc_out` ("not readable" / "not writable")

### macODBC_SegmentationFaultCatcher (lines 4073-4118)
- **Validation mode** (`ODBC_ValidatingPointers==true`): sets `ODBC_PointerIsValid=0`, reinstalls handler, `siglongjmp` back (lines 4086-4095)
- **Normal mode** (`ODBC_ValidatingPointers==false`): logs fatal segfault, calls `RollbackAllWork` + `SQLMD_disconnect`, exits via `ExitSonProcess` (lines 4099-4106)

**Why this pattern exists**: variadic arguments cannot be type-checked at compile time. A bad pointer passed to ExecSQL would crash in SQLBindCol/SQLBindParameter — this mechanism catches it early.

---

## Auto-Reconnect Mechanism

Based on code analysis, three paths appear to trigger reconnect:

1. **PREPARE failure** (lines 1377-1392): disconnect + reset `FirstTimeCalled` → `DB_CONNECTION_BROKEN`
2. **Execute on disconnected DB** (lines 1990-1993): returns `DB_CONNECTION_BROKEN`
3. **Error Handler conversion** (lines 3936-3959): TCP/transaction/schema errors → disconnect + `DB_CONNECTION_BROKEN`

The caller is expected to retry; on the next call, `ODBC_Exec_FirstTimeCalled` is reset, triggering full re-initialization.

---

## Provider Abstraction

According to `SQL_CustomizePerDB` (lines 2906-3136), MacODBC appears to bridge SQL syntax differences at runtime:

- **MODULO**: Informix `MOD(A,B)` vs MS-SQL `(A % B)` — with strtok_r parsing
- **TOP/FIRST**: Informix `FIRST` vs MS-SQL `TOP` — different position in command
- **TRANSACTION**: Informix reserved word vs MS-SQL requires escaping with quotes
- **GROUP**: Same pattern as TRANSACTION

---

*Generated by the CIDRA Documenter Agent — DOC-MACODBC-002*
