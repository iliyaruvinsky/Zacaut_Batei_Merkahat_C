# MacODBC.h - Business Logic

**Component**: MacODBC (ODBC Infrastructure Layer)
**Task ID**: DOC-MACODBC-002
**Date Created**: 2026-02-11

---

## Cursor Lifecycle

According to the 25 macros at `MacODBC.h:311-335` and the CommandType switch at `MacODBC.h:634-763`, the full cursor lifecycle appears to include:

### 1. Declare
```
DeclareCursor(db, op_id, &output_var)           → DECLARE_CURSOR
DeclareCursorInto(db, op_id, &out1, &out2, ...) → DECLARE_CURSOR_INTO
```
Sets: `NeedToPrepare=1`, `NeedToInterpretOp=1`
Does **not** set: `NeedToExecute` or `NeedToFetch`

### 2. Declare + Open
```
DeclareAndOpenCursor(db, op_id, &output_var, input_var)    → DECLARE_AND_OPEN_CURSOR
DeclareAndOpenCursorInto(db, op_id, &o1, &o2, ..., in)     → DECLARE_AND_OPEN_CURSOR_INTO
```
Sets: `NeedToPrepare=1`, `NeedToExecute=1`, `NeedToInterpretOp=1`
Does **not** set: `NeedToFetch`

### 3. Open
```
OpenCursor(db, op_id)                           → OPEN_CURSOR
OpenCursorUsing(db, op_id, input_var)           → OPEN_CURSOR_USING
```
Sets: `NeedToExecute=1`

### 4. Fetch
```
FetchCursor(db, op_id)                          → FETCH_CURSOR
FetchCursorInto(db, op_id, &o1, &o2, ...)      → FETCH_CURSOR_INTO
```
Sets: `NeedToFetch=1`
FetchCursorInto also sets: `NeedToInterpretOp=1`, `InterpretOutputOnly=1`

### 5. Close and Free
```
CloseCursor(db, op_id)                          → CLOSE_CURSOR
FreeStatement(db, op_id)                        → FREE_STATEMENT
```
`CloseCursor`: soft close (SQLCloseCursor) — statement remains prepared
`FreeStatement`: full free (SQLFreeHandle) — complete release

### 6. Singleton SQL Execution
```
ExecSQL(db, op_id, &output_var, input_var)      → SINGLETON_SQL_CALL
```
Sets: `NeedToPrepare=1`, `NeedToExecute=1`, `NeedToFetch=1`, `NeedToInterpretOp=1`

---

## Transaction Management

According to `MacODBC.h:2396-2490`, there appear to be two models:

### CommitWork / RollbackWork (DBC level)
According to lines 2428-2446:
- `SQLEndTran(SQL_HANDLE_DBC, Database->HDBC, SQL_COMMIT)` — commit to specific DB
- If the DB is not MAIN_DB, also clears `ALTERNATE_DB_USED`

### CommitAllWork / RollbackAllWork (ENV or DBC level)
According to lines 2396-2427:
- If `ALTERNATE_DB_USED`: `SQLEndTran(SQL_HANDLE_ENV, ODBC_henv, SQL_COMMIT)` — commit at ENV level (both DBs)
- If not: `SQLEndTran(SQL_HANDLE_DBC, MAIN_DB->HDBC, SQL_COMMIT)` — commit to MAIN_DB only (optimization)

**Implication**: CommitAllWork uses ENV level only when both DBs were used, avoiding an unnecessary commit to the DB that was not touched.

### Isolation Levels
According to lines 2493-2529:
- `SetDirtyRead` → `SQL_TXN_READ_UNCOMMITTED`
- `SetCommittedRead` → `SQL_TXN_READ_COMMITTED`
- `SetRepeatableRead` → `SQL_TXN_REPEATABLE_READ`

Note: Skips Informix, as seen in the `Database->Provider != ODBC_Informix` check.

---

## Mirroring Decision

According to analysis of ODBC_Exec phase 3 (lines 888-1072), the mirroring decision appears to occur in 3 steps:

### Step 1: Global flag check
```
CommandIsMirrored = ODBC_MIRRORING_ENABLED (line 918)
```
`ODBC_MIRRORING_ENABLED` is set in `GenSql.c` from an environment variable.

### Step 2: Per-operation flag check
The `Mirrored` flag comes from `SQL_GetMainOperationParameters` — each operation defines whether it needs mirroring.

### Step 3: SELECT exclusion
```
if (IsSelectStatement) CommandIsMirrored = 0;  (lines 994-996)
```

### Logic summary:
```
CommandIsMirrored = ODBC_MIRRORING_ENABLED
                  && per-operation Mirrored flag
                  && NOT SELECT
```

### Execution order
According to `MacODBC.h:2029`, the **mirror executes first**:
1. `SQLExecute` on `MirrorStatementPtr` (line 2029)
2. Mirror error check — failure is logged but **not fatal** (lines 2047-2079)
3. `SQLExecute` on `ThisStatementPtr` (line 2100)
4. Result merging: rows-affected = min(primary, mirror), SQLCODE = mirror if non-zero (lines 2266-2284)

---

## Admission Control for Sticky Statements

According to `MacODBC.h:1145-1159`:

### Mechanism:
1. Each prepared statement marked as sticky (StatementIsSticky) holds an open handle
2. Counter `NumStickyHandlesUsed` tracks active handles
3. When the limit `ODBC_MAX_STICKY_STATEMENTS` (120) is reached:
   - The new statement **loses** its stickiness
   - A diagnostic is logged
   - The statement still works but will not be cached

### Sticky statement lifecycle:
```
Allocation (SQLAllocStmt)
    ↓
PREPARE (SQLPrepare) → StatementPrepared[op] = 1
    ↓
Execute/Fetch (can repeat multiple times)
    ↓
Close (SQLCloseCursor — soft close, statement remains prepared)
    ↓
[Next call: skips PREPARE, goes straight to Bind/Execute]
    ↓
Free (SQLFreeHandle — release, NumStickyHandlesUsed--)
```

### Prepared-state validation
According to lines 1182-1202, when `ENABLE_PREPARED_STATE_VALIDATION=1`:
- Calls `SQLNumParams` on the statement
- If it fails → the handle is released and re-PREPARE is performed
- Protection mechanism against statements that became "stale" (e.g., after reconnect)

---

## Error Conversion Chains

According to `ODBC_ErrorHandler` (lines 3823-4004), there appear to be two main conversion chains:

### Chain 1: Access-conflict conversion (lines 3903-3926)
```
Native errors -11103 (S1002), -11031, -211
    ↓
Converted to: SQLERR_CANNOT_POSITION_WITHIN_TABL (-243)
    ↓
Purpose: trigger automatic retry at caller level
    ↓
Additional action: release sticky handle + decrement counter
```

### Chain 2: Auto-reconnect conversion (lines 3936-3959)
```
SQL_TRN_CLOSED_BY_OTHER_SESSION
DB_TCP_PROVIDER_ERROR
TABLE_SCHEMA_CHANGED
DB_CONNECTION_BROKEN
    ↓
Converted to: DB_CONNECTION_BROKEN
    ↓
Actions: SQLMD_disconnect + reset ODBC_Exec_FirstTimeCalled
    ↓
Purpose: next call will perform full re-initialization
```

---

## Connection Establishment

According to `ODBC_CONNECT` (lines 3443-3794), the connection process appears to be:

### Initialization sequence
1. DB header initialization: MS_DB (ODBC_MS_SqlServer), INF_DB (ODBC_Informix) — lines 3476-3482
2. `setlocale(LC_ALL, "he_IL.UTF-8")` — line 3488
3. `SQLAllocEnv` for ODBC environment (first time only) — lines 3494-3505
4. `SQLAllocConnect` — line 3513
5. Timeout settings (login, connection) — lines 3525-3536
6. MS-SQL: `SQL_COPT_SS_PRESERVE_CURSORS` if `ODBC_PRESERVE_CURSORS` — lines 3542-3547
7. `SQLConnect(DSN, username, password)` — line 3552

### Post-connect provider-specific settings
**MS-SQL** (lines 3630-3721):
- `USE <dbname>` — switch to target database
- `SET LOCK_TIMEOUT <ms>` — lock timeout
- `SET DEADLOCK_PRIORITY <level>` — deadlock priority

**Informix** (lines 3752-3761):
- Disable `SQL_INFX_ATTR_AUTO_FREE` — Informix auto-free mechanism

**All providers** (lines 3729-3742):
- AUTOCOMMIT off
- Dirty-read isolation default

### CleanupODBC (lines 3798-3819)
- `SQLDisconnect` + `SQLFreeConnect` + `Connected=0`
- Decrements `NUM_ODBC_DBS_CONNECTED`
- If last: `SQLFreeEnv` — lines 3812-3814

---

## Silent INSERT Failure Detection

According to `MacODBC.h:2252-2261`, in MS-SQL, an INSERT that fails silently (no error but `sqlerrd[2]==0`) is detected and handled:
- Checks if the operation is an INSERT and `sqlerrd[2]==0` (0 rows affected)
- Checks if this is not an expected result (IsSelectStatement)
- If detected — sets `ReturnCode` accordingly

---

## Cardinality Check

According to `MacODBC.h:2385-2392`, after a successful first fetch, a second fetch is performed:
- If the second fetch succeeds → more than one row exists
- Used to warn about unexpected cardinality (singleton query returning more than one row)

---

*Generated by the CIDRA Documenter Agent — DOC-MACODBC-002*
