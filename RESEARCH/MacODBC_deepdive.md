# MacODBC.h — Deep Dive

## Overview

`MacODBC.h` is the ODBC infrastructure layer for this codebase. It provides macro-driven APIs (`ExecSQL`, `DeclareAndOpenCursorInto`, `CommitWork`, etc.) that call one dispatcher (`ODBC_Exec`) and preserve an embedded-SQL style calling pattern in C modules (`source_code/Include/MacODBC.h:281-335`, `source_code/Include/MacODBC.h:446-450`).

This file is a hybrid header/implementation unit:
- It defines API/macros and public declarations.
- Under `#ifdef MAIN`, it also compiles full implementations (11 functions) and internal state/caches (`source_code/Include/MacODBC.h:168-273`, `source_code/Include/MacODBC.h:422-4120`).

Operation metadata is injected from per-component files:
- SQL operation switch-cases via `#include "MacODBC_MyOperators.c"` (`source_code/Include/MacODBC.h:2745-2748`).
- Custom WHERE-clause switch-cases via `#include "MacODBC_MyCustomWhereClauses.c"` (`source_code/Include/MacODBC.h:2864-2867`).

## File Statistics

- Path: `source_code/Include/MacODBC.h`
- Exact line count: `4121` (`source_code/Include/MacODBC.h:4121`)
- Author: Don Radlauer (`source_code/Include/MacODBC.h:14`)
- Date: December 2019 (`source_code/Include/MacODBC.h:15`)

## Methodology

Full-file read completed in 7 contiguous passes:
- Pass 1: `1-600`
- Pass 2: `601-1200`
- Pass 3: `1201-1800`
- Pass 4: `1801-2400`
- Pass 5: `2401-3000`
- Pass 6: `3001-3600`
- Pass 7: `3601-4121`

## File Structure Map

| Section | Lines | Content |
|---|---:|---|
| File header + guards | `1-27` | Purpose block, `#pragma once`, include guard |
| Includes | `31-41` | ODBC headers + C stdlib + `GenSql.h` + `MacODBC_MyOperatorIDs.h` |
| Core constants | `47-101` | ODBC attrs, pointer-validation flags, sticky/parameter limits |
| Enums + bool typedef | `104-151` | Provider enum, command enum, local bool enum |
| Struct typedefs | `163-164` | `ODBC_DB_HEADER`, `ODBC_ColumnParams` |
| Global storage / extern split | `167-273` | `#ifdef MAIN` storage, `#else extern` exports |
| Macro API | `277-348` | wrapper macros for cursor/exec/txn/error |
| Function declarations | `351-419` | prototypes + SIGSEGV handler pointer |
| Implementation block | `422-4120` | full code under `#ifdef MAIN` |
| End of include guard | `4121` | `#endif // MacODBC_H defined` |

`#ifdef MAIN` appears in two places:
- Globals block: `168-273`
- Function implementations block: `422-4120`

## Enum Definitions

### `enum ODBC_DatabaseProvider` (`source_code/Include/MacODBC.h:104-111`)

Values:
- `ODBC_NoProvider`
- `ODBC_Informix`
- `ODBC_DB2`
- `ODBC_MS_SqlServer`
- `ODBC_Oracle`

### `enum ODBC_CommandType` (`source_code/Include/MacODBC.h:117-139`)

Values:
- `DECLARE_CURSOR`
- `DECLARE_CURSOR_INTO`
- `DECLARE_AND_OPEN_CURSOR`
- `DECLARE_AND_OPEN_CURSOR_INTO`
- `DEFERRED_INPUT_CURSOR`
- `DEFERRED_INPUT_CURSOR_INTO`
- `OPEN_CURSOR`
- `OPEN_CURSOR_USING`
- `FETCH_CURSOR`
- `FETCH_CURSOR_INTO`
- `CLOSE_CURSOR`
- `FREE_STATEMENT`
- `SINGLETON_SQL_CALL`
- `COMMIT_WORK`
- `ROLLBACK_WORK`
- `GET_LENGTHS_READ`
- `SET_DIRTY_READ`
- `SET_COMMITTED_READ`
- `SET_REPEATABLE_READ`
- `SET_CUSTOM_SIGSEGV_HANDLER`

### `enum tag_bool` (`source_code/Include/MacODBC.h:145-151`)

Values:
- `false = 0`
- `true`

### `enum ODBC_ErrorCategory` (`source_code/Include/MacODBC.h:339-344`)

Values:
- `ODBC_ENVIRONMENT_ERR`
- `ODBC_DB_HANDLE_ERR`
- `ODBC_STATEMENT_ERR`

## Struct Definitions

### `ODBC_DB_HEADER` (`source_code/Include/MacODBC.h:163`)

| Field | Type | Purpose |
|---|---|---|
| `Provider` | `int` | Current DB provider ID |
| `Connected` | `int` | Connection state flag |
| `HDBC` | `SQLHDBC` | ODBC connection handle |
| `Name[21]` | `char[21]` | Display/diagnostic DB name |

### `ODBC_ColumnParams` (`source_code/Include/MacODBC.h:164`)

| Field | Type | Purpose |
|---|---|---|
| `type` | `int` | ODBC/C type code for bind/column |
| `length` | `int` | Length for string/buffer binds |

## Macro API

### Core wrapper macros (ODBC_Exec front-end)

25 wrapper macros route into `ODBC_Exec` (`source_code/Include/MacODBC.h:311-335`):
- Cursor lifecycle: `DeclareCursor`, `DeclareCursorInto`, `DeclareAndOpenCursor`, `DeclareAndOpenCursorInto`, `DeclareDeferredCursor`, `DeclareDeferredCursorInto`, `OpenCursor`, `OpenCursorUsing`, `FetchCursor`, `FetchCursorInto`, `CloseCursor`, `FreeStatement`
- SQL execution: `ExecSQL`, `ExecSql`
- Transaction: `CommitWork`, `RollbackWork`, `RollBackWork`, `CommitAllWork`, `RollbackAllWork`, `RollBackAllWork`
- Isolation/utility: `SetDirtyRead`, `SetCommittedRead`, `SetRepeatableRead`, `GetLengthsRead`, `SetCustomSegmentationFaultHandler`

### Error macros

- `ODBC_EnvironmentError`, `ODBC_DB_ConnectionError`, `ODBC_StatementError` (`source_code/Include/MacODBC.h:346-348`)

### Utility macros

- `SQL_WORKED`, `SQL_FAILED` (`source_code/Include/MacODBC.h:277-278`)

## Global Variables

### Exported globals (extern-visible)

These have storage in `#ifdef MAIN` and extern declarations in `#else`:
- `MS_DB` (`171` / `243`)
- `INF_DB` (`172` / `244`)
- `MAIN_DB` (`173` / `245`)
- `ALT_DB` (`174` / `246`)
- `ODBC_MIRRORING_ENABLED` (`180` / `248`)
- `MIRROR_SQLCODE` (`181` / `249`)
- `MIRROR_ROWS_AFFECTED` (`182` / `250`)
- `LOCK_TIMEOUT` (`183` / `251`)
- `DEADLOCK_PRIORITY` (`184` / `252`)
- `ODBC_SQLCODE` (`185` / `253`)
- `ODBC_PRESERVE_CURSORS` (`175` / `254`)
- `ODBC_ErrorBuffer` (`186` / `255`)
- `IntZero`, `IntOne`, `IntTwo` (`210-212` / `256-258`)
- `ShortZero`, `ShortOne`, `ShortTwo` (`213-215` / `259-261`)
- `LongZero` (`216` / `262`)
- `ColumnOutputLengths` (`218` / `263`)
- `ODBC_LastSQLStatementText` (`219` / `264`)
- `END_OF_ARG_LIST` (`220` / `265`)
- `ODBC_AvoidRecursion` (`188` / `266`)
- `ODBC_ValidatingPointers` (`190` / `267`)
- `ODBC_PointerIsValid` (`193` / `268`)
- `Connect_ODBC_Only` (`196` / `269`)
- `sig_act_ODBC_SegFault` (`198` / `270`)
- `BeforePointerTest` (`199` / `271`)
- `AfterPointerTest` (`200` / `272`)

### Internal (module-scope static/private)

Internal-only state without extern declarations:
- `ODBC_henv` (`170`)
- `ODBC_henv_allocated` (`177`)
- `NUM_ODBC_DBS_CONNECTED` (`178`)
- `ALTERNATE_DB_USED` (`179`)
- `CurrentMacroName` (`202`)
- `ODBC_ActivityCount` (`205`)
- `PrintDiagnosticsAfter` (`206`)
- `ODBC_Exec_FirstTimeCalled` (`225`)
- `NumStickyHandlesUsed` (`226`)
- `StatementPrepared[]`, `StatementOpened[]`, `StatementIsSticky[]` (`234-236`)
- `BoolType` (`240`)

(All citations in this section are from `source_code/Include/MacODBC.h:170-272`.)

## Function Implementations

### Inventory (implemented in this file under `#ifdef MAIN`)

| Function | Start-End | Exact Lines |
|---|---:|---:|
| `ODBC_Exec` | `446-2657` | `2212` |
| `SQL_GetMainOperationParameters` | `2661-2817` | `157` |
| `SQL_GetWhereClauseParameters` | `2821-2902` | `82` |
| `SQL_CustomizePerDB` | `2906-3136` | `231` |
| `ParseColumnList` | `3140-3330` | `191` |
| `find_FOR_UPDATE_or_GEN_VALUES` | `3334-3439` | `106` |
| `ODBC_CONNECT` | `3443-3794` | `352` |
| `CleanupODBC` | `3798-3819` | `22` |
| `ODBC_ErrorHandler` | `3823-4004` | `182` |
| `ODBC_IsValidPointer` | `4013-4070` | `58` |
| `macODBC_SegmentationFaultCatcher` | `4073-4118` | `46` |

### `ODBC_Exec` key dispatch behavior

`ODBC_Exec` decodes command type into behavior flags in a command switch (`source_code/Include/MacODBC.h:634-763`). Cases map one-to-one with `ODBC_CommandType` values and macro wrappers (`source_code/Include/MacODBC.h:636-752`, `source_code/Include/MacODBC.h:311-335`).

High-signal internal phases:
1. Interpret command and validate operation ID (`source_code/Include/MacODBC.h:634-772`)
2. First-call initialization, signal handler install, bool-size probe (`source_code/Include/MacODBC.h:780-847`)
3. Load operation metadata + optional dynamic SQL/custom WHERE (`source_code/Include/MacODBC.h:899-1072`)
4. Sticky statement lifecycle and statement allocation (`source_code/Include/MacODBC.h:1086-1231`)
5. SQL construction + provider customization (`source_code/Include/MacODBC.h:1269-1301`, `source_code/Include/MacODBC.h:2906-3136`)
6. PREPARE/Bind/Execute/Fetch/Commit/Rollback/Close (`source_code/Include/MacODBC.h:1341-2640`)

## Declared vs Defined Functions

All major prototypes declared in `MacODBC.h` are defined later in the same file under `#ifdef MAIN`:
- `ODBC_Exec` declared `352` / defined `446`
- `SQL_GetMainOperationParameters` declared `358` / defined `2661`
- `SQL_GetWhereClauseParameters` declared `374` / defined `2821`
- `SQL_CustomizePerDB` declared `381` / defined `2906`
- `ParseColumnList` declared `385` / defined `3140`
- `find_FOR_UPDATE_or_GEN_VALUES` declared `389` / defined `3334`
- `ODBC_CONNECT` declared `396` / defined `3443`
- `CleanupODBC` declared `403` / defined `3798`
- `ODBC_ErrorHandler` declared `405` / defined `3823`
- `ODBC_IsValidPointer` declared `411` / defined `4013`
- `macODBC_SegmentationFaultCatcher` declared `418` / defined `4073`

(All from `source_code/Include/MacODBC.h`.)

## Key Constants

| Constant | Value | Line | Notes |
|---|---:|---:|---|
| `ODBC_MAX_STICKY_STATEMENTS` | `120` | `90` | Sticky PREPARE cache ceiling |
| `ODBC_MAX_PARAMETERS` | `300` | `101` | Max combined bind/output pointer array size |
| `SQL_COPT_SS_PRESERVE_CURSORS` | `1204` | `48` | MS-SQL preserve-cursor attr |
| `SQL_PC_ON` / `SQL_PC_OFF` | `1L` / `0L` | `49-50` | preserve-cursor values |
| `SQL_INFX_ATTR_AUTO_FREE` | `2263` | `53` | Informix attr toggled in connect |
| `ENABLE_POINTER_VALIDATION` | `1` | `62` | Enables SIGSEGV pointer validation path |
| `ENABLE_PREPARED_STATE_VALIDATION` | `1` | `76` | Enables SQLNumParams sticky validity check |
| `ODBC_MAXIMUM_OPERATION` | enum sentinel | `source_code/Include/MacODBC_MyOperatorIDs.h:1035` | Array sizing anchor used in MacODBC |

Not defined in this file:
- `ODBC_MAX_COLUMNS` (no match)
- `ODBC_MAX_RETRIES` (no match)

## Database Provider Support

Defined providers: Informix, DB2, MS-SQL, Oracle (`source_code/Include/MacODBC.h:104-111`).

Implemented runtime logic is explicit for Informix + MS-SQL:
- Connection initialization names only these two (`source_code/Include/MacODBC.h:3476-3482`)
- Provider-specific SQL token rewriting in `SQL_CustomizePerDB` for modulo/top-first/transaction/group (`source_code/Include/MacODBC.h:2971-3010`, `source_code/Include/MacODBC.h:3074-3117`)

DB2/Oracle:
- Present in enum only; no dedicated connection/customization branches found in this file.

## Database Mirroring Mechanism

Mirroring state:
- Globals: `ODBC_MIRRORING_ENABLED`, `MIRROR_SQLCODE`, `MIRROR_ROWS_AFFECTED` (`source_code/Include/MacODBC.h:180-182`, `source_code/Include/MacODBC.h:248-250`)
- `MAIN_DB`/`ALT_DB` pointers select primary/alternate DB (`source_code/Include/MacODBC.h:173-174`)

Configuration source:
- `ODBC_MIRRORING_ENABLED` env var parsed in `GenSql.c`; enables mirroring when set to Yes/On/Enabled/True (`source_code/GenSql/GenSql.c:1167-1177`)
- `ALT_DB` assigned to the non-primary connected DB (`source_code/GenSql/GenSql.c:1195-1205`)

Execution flow inside `ODBC_Exec`:
- Operation-level mirror flag from metadata, then disabled for SELECTs (`source_code/Include/MacODBC.h:918`, `source_code/Include/MacODBC.h:994-997`)
- Mirror target DB computed as the opposite of current DB (`source_code/Include/MacODBC.h:1002`)
- Mirror statement allocated/prepared/executed (`source_code/Include/MacODBC.h:1221-1231`, `source_code/Include/MacODBC.h:1401-1417`, `source_code/Include/MacODBC.h:2023-2034`)
- Rows-affected/return code merged to worst-case view (`source_code/Include/MacODBC.h:2268-2283`)

## Sticky Statement Management

Storage/counters:
- Status arrays and counters: `StatementPrepared[]`, `StatementOpened[]`, `StatementIsSticky[]`, `NumStickyHandlesUsed` (`source_code/Include/MacODBC.h:225-236`, `source_code/Include/MacODBC.h:461-467`)

Lifecycle:
- Admission control against `ODBC_MAX_STICKY_STATEMENTS` (`source_code/Include/MacODBC.h:1145-1159`)
- Prepared-state validation via `SQLNumParams` when enabled (`source_code/Include/MacODBC.h:1182-1199`)
- Forced unprepare/decrement on selected failures (`source_code/Include/MacODBC.h:2175-2180`, `source_code/Include/MacODBC.h:2360-2364`, `source_code/Include/MacODBC.h:2622-2631`)

## Pointer Validation (SIGSEGV Path)

Call sites before binds:
- Output pointer checks (`source_code/Include/MacODBC.h:1524`)
- Input pointer checks (`source_code/Include/MacODBC.h:1560`)
- Custom WHERE input pointer checks (`source_code/Include/MacODBC.h:1595`)

Mechanism:
- `ODBC_Exec` sets validation mode (`source_code/Include/MacODBC.h:1456`)
- `ODBC_IsValidPointer` uses `sigsetjmp` + forced read/write probe (`source_code/Include/MacODBC.h:4026-4042`)
- Handler flips validity and `siglongjmp`s back (`source_code/Include/MacODBC.h:4086-4095`)

## Auto-Reconnect Behavior

Reconnect trigger paths:
- PREPARE infrastructure failure: disconnect + force `DB_CONNECTION_BROKEN` (`source_code/Include/MacODBC.h:1375-1392`)
- Execute on closed DB: force `DB_CONNECTION_BROKEN` (`source_code/Include/MacODBC.h:1990-1993`)
- Error-handler conversion of certain native errors to `DB_CONNECTION_BROKEN` (`source_code/Include/MacODBC.h:3938-3959`)

Observed reconnect-target errors converted:
- `SQL_TRN_CLOSED_BY_OTHER_SESSION`
- `DB_TCP_PROVIDER_ERROR`
- `TABLE_SCHEMA_CHANGED`
- `DB_CONNECTION_BROKEN`
(`source_code/Include/MacODBC.h:3938-3941`)

The comment explicitly states this return code is used by higher-level error handling to force reconnect (`source_code/Include/MacODBC.h:3932-3933`).

## Include Chain

### MacODBC.h includes

- `/usr/local/include/sql.h` (`source_code/Include/MacODBC.h:31`)
- `/usr/local/include/sqlext.h` (`source_code/Include/MacODBC.h:32`)
- `<errno.h>` (`source_code/Include/MacODBC.h:33`)
- `<locale.h>` (`source_code/Include/MacODBC.h:34`)
- `<string.h>` (`source_code/Include/MacODBC.h:35`)
- `<setjmp.h>` (`source_code/Include/MacODBC.h:36`)
- `"GenSql.h"` (`source_code/Include/MacODBC.h:37`)
- `<MacODBC_MyOperatorIDs.h>` (`source_code/Include/MacODBC.h:41`)

### Source files that include MacODBC.h

Direct includes were found in these source modules:
- `source_code/FatherProcess/FatherProcess.c` (`29`)
- `source_code/SqlServer/SqlServer.c` (`29`)
- `source_code/SqlServer/SqlHandlers.c` (`25`)
- `source_code/SqlServer/MessageFuncs.c` (`32`)
- `source_code/SqlServer/ElectronicPr.c` (`21`)
- `source_code/SqlServer/DigitalRx.c` (`20`)
- `source_code/As400UnixClient/As400UnixClient.c` (`29`)
- `source_code/As400UnixServer/As400UnixServer.c` (`32`)
- `source_code/As400UnixDocServer/As400UnixDocServer.c` (`31`)
- `source_code/As400UnixDoc2Server/As400UnixDoc2Server.c` (`34`)
- `source_code/ShrinkPharm/ShrinkPharm.c` (`16`)
- `source_code/GenSql/GenSql.c` (`32`)
- plus `.ec` sources under `GenSql` / `As400UnixDoc*` (see repository search results)

(all include hits from repository-wide search.)

## Related Files and Role

| File | Relationship to MacODBC |
|---|---|
| `source_code/Include/MacODBC_MyOperatorIDs.h` | Defines operation enum and `ODBC_MAXIMUM_OPERATION`; also operation-name array used by diagnostics (`source_code/Include/MacODBC_MyOperatorIDs.h:34-36`, `source_code/Include/MacODBC_MyOperatorIDs.h:1035`, `source_code/Include/MacODBC_MyOperatorIDs.h:1098`) |
| `source_code/Include/GenSql_ODBC_OperatorIDs.h` | Contains GenSql operation-ID entries (include-fragment style) (`source_code/Include/GenSql_ODBC_OperatorIDs.h:33-44`) |
| `source_code/Include/GenSql_ODBC_Operators.c` | Reused operation-switch cases, included by component `MacODBC_MyOperators.c` files (e.g. `source_code/FatherProcess/MacODBC_MyOperators.c:87`) |
| `source_code/*/MacODBC_MyOperators.c` | Component-specific SQL operation definitions consumed by `SQL_GetMainOperationParameters` include injection (`source_code/Include/MacODBC.h:2745-2748`) |
| `source_code/*/MacODBC_MyCustomWhereClauses.c` | Component-specific custom WHERE definitions consumed by `SQL_GetWhereClauseParameters` include injection (`source_code/Include/MacODBC.h:2864-2867`) |
| `source_code/Include/GenSql.h` | Provides SQL/`sqlca` context and macros used by this infrastructure (`source_code/Include/MacODBC.h:37`, `source_code/Include/GenSql.h:194`) |

## Security Notes

- This file processes DB credentials passed into `ODBC_CONNECT` (`username`, `password`) (`source_code/Include/MacODBC.h:3444-3448`).
- `GenSql.c` sources those from env vars (`MS_SQL_ODBC_USER`, `MS_SQL_ODBC_PASS`, etc.) instead of hardcoded literals (`source_code/GenSql/GenSql.c:1088-1091`).
- A commented diagnostic string includes user/password placeholders in clear text format (`source_code/Include/MacODBC.h:3550`, commented).
- Pointer validation hardens against bad bind pointers and prevents uncontrolled crashes during argument processing (`source_code/Include/MacODBC.h:1456`, `source_code/Include/MacODBC.h:4026-4042`, `source_code/Include/MacODBC.h:4086-4095`).

## Open Questions

1. `ODBC_DB2` and `ODBC_Oracle` exist in provider enum, but this file has no dedicated connect/customization branches for them; confirm whether this support is planned-only or implemented elsewhere.
2. `ODBC_MAX_COLUMNS` / `ODBC_MAX_RETRIES` are not defined in this file; verify whether equivalent limits are intentionally represented by other symbols (`ODBC_MAX_PARAMETERS`, external retry loops in callers).

