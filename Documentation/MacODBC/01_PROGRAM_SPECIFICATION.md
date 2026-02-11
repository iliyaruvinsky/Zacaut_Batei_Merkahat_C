# MacODBC.h - Program Specification

**Component**: MacODBC (ODBC Infrastructure Layer)
**Task ID**: DOC-MACODBC-002
**Date Created**: 2026-02-11
**Source**: `source_code/Include/MacODBC.h`

---

## Overview

According to the file header at `MacODBC.h:14-15`, the file was written by **Don Radlauer** in December 2019. MacODBC.h appears to be the central ODBC infrastructure layer for the entire MACCABI C Backend system, replacing the previous Informix Embedded SQL (ESQL) approach with an ODBC interface that supports simultaneous connections to MS-SQL and Informix.

This is a hybrid header/implementation file: all code is guarded under `#ifdef MAIN` — the `.c` file that defines `MAIN` before the include receives the full implementation compiled into it, as seen in `MacODBC.h:168` and `MacODBC.h:422`.

---

## Purpose

Based on code analysis, MacODBC.h appears to:

1. **Provide a macro API** — 25 public macros (such as `ExecSQL`, `DeclareCursor`, `CommitWork`) that route to a single dispatcher function (lines 311-335)
2. **Manage database connections** — connect, disconnect, and auto-reconnect for MS-SQL and Informix (lines 3443-3819)
3. **Support database mirroring** — dual writes to MAIN_DB and ALT_DB (lines 918, 2023-2094)
4. **Manage a prepared statement cache** — up to 120 "sticky" statements (lines 1086-1231)
5. **Validate pointers at runtime** — SIGSEGV/sigsetjmp/siglongjmp mechanism for va_arg pointer validation (lines 4013-4118)
6. **Perform per-provider SQL adaptation** — token replacement between Informix and MS-SQL syntax (lines 2906-3136)

---

## File Structure

| File | Lines | Purpose |
|------|------:|---------|
| MacODBC.h | 4121 | ODBC infrastructure — hybrid header/implementation file |

*Line count verified from chunker data in `CHUNKS/MacODBC/repository.json`*

**Note**: This is a single file that serves all 8 system components, as seen in the include chain scan in `RESEARCH/MacODBC_deepdive.md`.

---

## Functions

According to code analysis of `MacODBC.h`, 11 functions are implemented under `#ifdef MAIN`:

| Function | Lines | Location | Purpose |
|----------|------:|----------|---------|
| ODBC_Exec | 2212 | 446-2657 | Central dispatcher — handles all 25 command types |
| SQL_GetMainOperationParameters | 157 | 2661-2817 | Decode OperationID to SQL metadata |
| SQL_GetWhereClauseParameters | 82 | 2821-2902 | Decode WhereClauseID to text and parameters |
| SQL_CustomizePerDB | 231 | 2906-3136 | Provider-specific token replacement |
| ParseColumnList | 191 | 3140-3330 | Parse column-spec string to ODBC_ColumnParams array |
| find_FOR_UPDATE_or_GEN_VALUES | 106 | 3334-3439 | Detect INSERT/SELECT/FOR UPDATE in SQL command |
| ODBC_CONNECT | 352 | 3443-3794 | Full connection setup — env, connect, provider-specific settings |
| CleanupODBC | 22 | 3798-3819 | Disconnect and release — env release when last disconnects |
| ODBC_ErrorHandler | 182 | 3823-4004 | Central error handler — SQLGetDiagRec, error conversion |
| ODBC_IsValidPointer | 58 | 4013-4070 | Pointer validity check via sigsetjmp/siglongjmp |
| macODBC_SegmentationFaultCatcher | 46 | 4073-4118 | SIGSEGV handler — validation mode vs fatal mode |

**Total functions: 11**

---

## Public Macro API (25 macros)

According to lines 311-335 in `MacODBC.h`, all macros route to `ODBC_Exec` with the appropriate `ODBC_CommandType` value:

### Cursor Management
| Macro | Line | CommandType |
|-------|------|-------------|
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

### SQL Execution
| Macro | Line | CommandType |
|-------|------|-------------|
| ExecSQL | 318 | SINGLETON_SQL_CALL |
| ExecSql | 319 | SINGLETON_SQL_CALL |

**Note**: According to `MacODBC.h:318-319`, both spellings (`ExecSQL` and `ExecSql`) exist for backward compatibility.

### Transactions
| Macro | Line | CommandType |
|-------|------|-------------|
| CommitWork | 323 | COMMIT_WORK |
| RollbackWork | 324 | ROLLBACK_WORK |
| RollBackWork | 325 | ROLLBACK_WORK |
| CommitAllWork | 326 | COMMIT_WORK (ENV level) |
| RollbackAllWork | 327 | ROLLBACK_WORK (ENV level) |
| RollBackAllWork | 328 | ROLLBACK_WORK (ENV level) |

**Note**: According to `MacODBC.h:324-325`, both spellings (`RollbackWork` and `RollBackWork`) exist.

### Isolation and Utility
| Macro | Line | CommandType |
|-------|------|-------------|
| SetDirtyRead | 329 | SET_DIRTY_READ |
| SetCommittedRead | 330 | SET_COMMITTED_READ |
| SetRepeatableRead | 331 | SET_REPEATABLE_READ |
| GetLengthsRead | 332 | GET_LENGTHS_READ |
| SetCustomSegmentationFaultHandler | 335 | SET_CUSTOM_SIGSEGV_HANDLER |

### Utility Macros
According to lines 277-278 in `MacODBC.h`:
- `SQL_WORKED(R)` — checks if return value indicates success
- `SQL_FAILED(R)` — checks if return value indicates failure

### Error Macros
According to lines 346-348 in `MacODBC.h`:
- `ODBC_EnvironmentError` — routes to ODBC_ErrorHandler with ODBC_ENVIRONMENT_ERR
- `ODBC_DB_ConnectionError` — routes to ODBC_ErrorHandler with ODBC_DB_HANDLE_ERR
- `ODBC_StatementError` — routes to ODBC_ErrorHandler with ODBC_STATEMENT_ERR

---

## Enum Definitions

### ODBC_DatabaseProvider (`MacODBC.h:104-111`)
| Value | Meaning |
|-------|---------|
| ODBC_NoProvider | No provider |
| ODBC_Informix | Informix |
| ODBC_DB2 | DB2 |
| ODBC_MS_SqlServer | MS-SQL Server |
| ODBC_Oracle | Oracle |

**Note**: Based on code analysis, only Informix and MS-SQL are actually implemented. DB2 and Oracle exist in the enum only.

### ODBC_CommandType (`MacODBC.h:117-139`)
20 values mapping all supported command types:
`DECLARE_CURSOR`, `DECLARE_CURSOR_INTO`, `DECLARE_AND_OPEN_CURSOR`, `DECLARE_AND_OPEN_CURSOR_INTO`, `DEFERRED_INPUT_CURSOR`, `DEFERRED_INPUT_CURSOR_INTO`, `OPEN_CURSOR`, `OPEN_CURSOR_USING`, `FETCH_CURSOR`, `FETCH_CURSOR_INTO`, `CLOSE_CURSOR`, `FREE_STATEMENT`, `SINGLETON_SQL_CALL`, `COMMIT_WORK`, `ROLLBACK_WORK`, `GET_LENGTHS_READ`, `SET_DIRTY_READ`, `SET_COMMITTED_READ`, `SET_REPEATABLE_READ`, `SET_CUSTOM_SIGSEGV_HANDLER`.

### tag_bool (`MacODBC.h:145-151`)
| Value | Meaning |
|-------|---------|
| false | 0 |
| true | 1 |

Guarded under `#ifndef BOOL_DEFINED`.

### ODBC_ErrorCategory (`MacODBC.h:339-344`)
| Value | Meaning |
|-------|---------|
| ODBC_ENVIRONMENT_ERR | Environment error |
| ODBC_DB_HANDLE_ERR | Connection handle error |
| ODBC_STATEMENT_ERR | Statement error |

---

## Data Structures (structs)

### ODBC_DB_HEADER (`MacODBC.h:163`)

| Field | Type | Purpose |
|-------|------|---------|
| Provider | int | DB provider identifier (from ODBC_DatabaseProvider enum) |
| Connected | int | Connection state flag |
| HDBC | SQLHDBC | ODBC connection handle |
| Name[21] | char[21] | DB name for diagnostics |

### ODBC_ColumnParams (`MacODBC.h:164`)

| Field | Type | Purpose |
|-------|------|---------|
| type | int | ODBC/C type code for binding |
| length | int | Length for string/buffer binds |

---

## Constants (#define)

According to lines 47-101 in `MacODBC.h`:

| Constant | Value | Line | Purpose |
|----------|-------|------|---------|
| SQL_COPT_SS_PRESERVE_CURSORS | 1204 | 48 | MS-SQL cursor preservation attribute |
| SQL_PC_ON | 1L | 49 | Enable cursor preservation |
| SQL_PC_OFF | 0L | 50 | Disable cursor preservation |
| SQL_INFX_ATTR_AUTO_FREE | 2263 | 53 | Informix auto-free attribute |
| ENABLE_POINTER_VALIDATION | 1 | 62 | Enable SIGSEGV pointer validation mechanism |
| CHECK_VALIDITY_ONLY | 0 | 63 | Read-only check |
| CHECK_POINTER_IS_WRITABLE | 1 | 64 | Read and write check |
| ENABLE_PREPARED_STATE_VALIDATION | 1 | 76 | Enable SQLNumParams validity check for prepared state |
| ODBC_MAX_STICKY_STATEMENTS | 120 | 90 | Maximum sticky statement limit |
| ODBC_MAX_PARAMETERS | 300 | 101 | Maximum bind/output parameter limit |

---

## Global Variables

According to lines 168-273 in `MacODBC.h`, variables are defined under `#ifdef MAIN` (line 168) with `extern` declarations under `#else` (line 242):

### Database Headers
| Variable | MAIN Line | extern Line | Type | Purpose |
|----------|-----------|-------------|------|---------|
| MS_DB | 171 | 243 | ODBC_DB_HEADER | MS-SQL header |
| INF_DB | 172 | 244 | ODBC_DB_HEADER | Informix header |
| MAIN_DB | 173 | 245 | ODBC_DB_HEADER* | Pointer to primary DB |
| ALT_DB | 174 | 246 | ODBC_DB_HEADER* | Pointer to alternate DB |

### Mirroring State
| Variable | MAIN Line | extern Line | Purpose |
|----------|-----------|-------------|---------|
| ODBC_MIRRORING_ENABLED | 180 | 248 | Global mirroring enable flag |
| MIRROR_SQLCODE | 181 | 249 | Error code from mirror |
| MIRROR_ROWS_AFFECTED | 182 | 250 | Rows affected in mirror |

### Connection Configuration
| Variable | MAIN Line | extern Line | Purpose |
|----------|-----------|-------------|---------|
| LOCK_TIMEOUT | 183 | 251 | Lock timeout (MS-SQL) |
| DEADLOCK_PRIORITY | 184 | 252 | Deadlock priority |
| ODBC_PRESERVE_CURSORS | 175 | 254 | Cursor preservation (MS-SQL) |

### Pointer Validation State
| Variable | MAIN Line | extern Line | Purpose |
|----------|-----------|-------------|---------|
| ODBC_ValidatingPointers | 190 | 267 | Flag: currently in validation mode |
| ODBC_PointerIsValid | 193 | 268 | Result of last validation check |
| sig_act_ODBC_SegFault | 198 | 270 | sigaction structure |
| BeforePointerTest | 199 | 271 | sigjmp_buf before test |
| AfterPointerTest | 200 | 272 | sigjmp_buf after test |

### Internal Variables (no extern)
| Variable | Line | Purpose |
|----------|------|---------|
| ODBC_henv | 170 | ODBC environment handle |
| ODBC_henv_allocated | 177 | Flag: env allocated |
| NUM_ODBC_DBS_CONNECTED | 178 | Connected DB counter |
| ALTERNATE_DB_USED | 179 | Flag: alternate DB was used |
| ODBC_Exec_FirstTimeCalled | 225 | Flag: first call |
| NumStickyHandlesUsed | 226 | Active sticky handle counter |
| StatementPrepared[] | 234 | Prepared state array |
| StatementOpened[] | 235 | Opened state array |
| StatementIsSticky[] | 236 | Stickiness flag array |
| BoolType | 240 | Bool type (determined at runtime) |

---

## Dependencies (#include)

According to lines 31-41 in `MacODBC.h`:

| Header File | Line | Purpose |
|-------------|------|---------|
| `/usr/local/include/sql.h` | 31 | ODBC system header |
| `/usr/local/include/sqlext.h` | 32 | ODBC extensions |
| `<errno.h>` | 33 | Error codes |
| `<locale.h>` | 34 | Locale settings (Hebrew) |
| `<string.h>` | 35 | String functions |
| `<setjmp.h>` | 36 | sigsetjmp/siglongjmp for pointer validation |
| `"GenSql.h"` | 37 | Project SQL infrastructure |
| `<MacODBC_MyOperatorIDs.h>` | 41 | Per-component operator identifiers |

### Injection Includes (inside functions)
| File | Line | Context |
|------|------|---------|
| `"MacODBC_MyOperators.c"` | 2747 | Switch cases in SQL_GetMainOperationParameters |
| `"MacODBC_MyCustomWhereClauses.c"` | 2866 | Switch cases in SQL_GetWhereClauseParameters |

---

## Consuming Components (8 components)

According to the include chain scan in `RESEARCH/MacODBC_deepdive.md`:

| Component | MacODBC_MyOperators.c | MacODBC_MyCustomWhereClauses.c |
|-----------|----------------------|-------------------------------|
| FatherProcess | Yes | Yes |
| SqlServer | Yes | Yes |
| As400UnixServer | Yes | Yes |
| As400UnixClient | Yes | Yes |
| As400UnixDocServer | Yes | Yes |
| As400UnixDoc2Server | Yes | Yes |
| ShrinkPharm | Yes | Yes |
| GenSql | Yes | N/A |

---

## Documentation Limitations

### What CANNOT be determined from code:
- DB2 and Oracle behavior — present in enum only, no dedicated connection/adaptation branches
- SQL table contents — SQL itself is defined in per-component MacODBC_MyOperators.c files
- Actual DB credential values — passed as parameters to ODBC_CONNECT from GenSql.c
- External behavior of `SQLMD_connect` and `SQLMD_disconnect` (defined in GenSql.c)

### What IS known from code:
- Complete file structure (4121 lines, 11 functions)
- 25 public API macros and their CommandType mappings
- 4 enum definitions (DatabaseProvider, CommandType, tag_bool, ErrorCategory)
- 2 data structures (ODBC_DB_HEADER, ODBC_ColumnParams)
- All global variables with MAIN/extern duality
- DB mirroring mechanism, sticky statement cache, pointer validation
- 8 components consuming MacODBC.h

---

*Generated by the CIDRA Documenter Agent — DOC-MACODBC-002*
