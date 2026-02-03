# ShrinkPharm (source_code/ShrinkPharm) — Deep Dive (RES-SHRINKPHARM-001)

**Generated:** 2026-02-02  
**Scope:** `source_code/ShrinkPharm/` (read all files in folder; primary analysis focuses on the 3 ShrinkPharm source files totaling **574** lines exactly).  

## File inventory (exact)

### ShrinkPharm source files (verified 574 total)

Total source files: **3**. Total lines (exact): **574**.

| File | Lines (exact) | Role / notes |
|------|--------------:|--------------|
| `source_code/ShrinkPharm/ShrinkPharm.c` | 431 | Main housekeeping utility; stated as ODBC equivalent of “ShrinkDoctor” (`source_code/ShrinkPharm/ShrinkPharm.c:L7-L10`) and implements purge logic driven by the `shrinkpharm` control table (`source_code/ShrinkPharm/MacODBC_MyOperators.c:L91-L99`). |
| `source_code/ShrinkPharm/MacODBC_MyOperators.c` | 133 | “Operator case” definitions intended to be inserted into a switch (`source_code/ShrinkPharm/MacODBC_MyOperators.c:L82-L85`); defines ShrinkPharm SQL operations, including reads/updates of `shrinkpharm` (`source_code/ShrinkPharm/MacODBC_MyOperators.c:L90-L110`). |
| `source_code/ShrinkPharm/MacODBC_MyCustomWhereClauses.c` | 10 | Placeholder for custom WHERE clauses; currently contains only a commented example block (`source_code/ShrinkPharm/MacODBC_MyCustomWhereClauses.c:L3-L9`). |

### Build / IDE metadata (present in folder; not part of 574-line source total above)

| File | Lines (exact) | Role / notes |
|------|--------------:|--------------|
| `source_code/ShrinkPharm/Makefile` | 26 | Unix build for `ShrinkPharm.exe` linking `-lodbc` and using `_GNU_SOURCE` (`source_code/ShrinkPharm/Makefile:L8-L23`). |
| `source_code/ShrinkPharm/ShrinkPharm.vcxproj` | 94 | Visual Studio Makefile project; enumerates the 3 ShrinkPharm `.c` files as compile items (`source_code/ShrinkPharm/ShrinkPharm.vcxproj:L83-L90`). |
| `source_code/ShrinkPharm/ShrinkPharm.vcxproj.filters` | 31 | Visual Studio filter mapping for `ShrinkPharm.c` and the `MacODBC_*` files (`source_code/ShrinkPharm/ShrinkPharm.vcxproj.filters:L18-L26`). |
| `source_code/ShrinkPharm/ShrinkPharm.vcxproj.user` | 4 | Visual Studio per-user stub (`source_code/ShrinkPharm/ShrinkPharm.vcxproj.user:L1-L4`). |
| `source_code/ShrinkPharm/.project` | 26 | Eclipse CDT project metadata (project name `ShrinkPharm`) (`source_code/ShrinkPharm/.project:L1-L3`). |
| `source_code/ShrinkPharm/.cproject` | 71 | Eclipse CDT build targets for `make all` and `make clean` (`source_code/ShrinkPharm/.cproject:L50-L68`). |
| `source_code/ShrinkPharm/.settings/language.settings.xml` | 12 | Eclipse language settings providers config (`source_code/ShrinkPharm/.settings/language.settings.xml:L1-L12`). |

## Overview / purpose (what it “shrinks”)

### Stated purpose

The file header states ShrinkPharm is an “ODBC equivalent of ShrinkDoctor” for the “MS‑SQL Pharmacy application” (`source_code/ShrinkPharm/ShrinkPharm.c:L7-L10`).

### What it deletes (“shrinks”) in practice

According to the control-cursor SQL in `MacODBC_MyOperators.c`, ShrinkPharm reads its purge plan from a database table named **`shrinkpharm`** and processes only rows where `purge_enabled <> 0` (`source_code/ShrinkPharm/MacODBC_MyOperators.c:L91-L95`).

Each control row supplies:
- `table_name` (target table name), `days_to_retain`, `date_column_name`, `use_where_clause`, `commit_count` (`source_code/ShrinkPharm/MacODBC_MyOperators.c:L91-L99`).

For each configured target table, ShrinkPharm computes:
- \( \text{MinDateToRetain} = \text{SysDate} - \text{days\_to\_retain} \) using `IncrementDate(SysDate, 0 - days_to_retain)` (`source_code/ShrinkPharm/ShrinkPharm.c:L189-L191`).

It then deletes rows where the configured date column is **older than** `MinDateToRetain`:
- **If `use_where_clause != 0`**: the select cursor is built with a `WHERE <date_column> < MinDateToRetain` clause (`source_code/ShrinkPharm/ShrinkPharm.c:L193-L207`).
- **If `use_where_clause == 0`**: the select cursor scans the whole table and applies the equivalent filter in code (`if DateOfRow >= MinDateToRetain continue;`) (`source_code/ShrinkPharm/ShrinkPharm.c:L208-L213`, `source_code/ShrinkPharm/ShrinkPharm.c:L253-L257`).

Actual deletion is executed row-by-row using a `DELETE FROM <table_name> WHERE CURRENT OF ShrinkPharmSelCur` command (`source_code/ShrinkPharm/ShrinkPharm.c:L215-L216`, `source_code/ShrinkPharm/ShrinkPharm.c:L266-L267`).

## Build + runtime configuration

### Build (Unix Makefile)

- Target binary is `ShrinkPharm.exe` (`source_code/ShrinkPharm/Makefile:L8-L8`).
- Links with `-lodbc` and defines `_GNU_SOURCE` (`source_code/ShrinkPharm/Makefile:L19-L23`).

### Logging configuration

- Log directory is read from environment variable `MAC_LOG` via `getenv("MAC_LOG")` into `LogDir` (`source_code/ShrinkPharm/ShrinkPharm.c:L72-L72`).
- Log file base name is hard-coded to `"ShrinkPharm_log"` (`source_code/ShrinkPharm/ShrinkPharm.c:L73-L77`).

### Database connection configuration (ODBC via shared GenSql/MacODBC layer)

ShrinkPharm connects via `SQLMD_connect()` in a retry loop until `MAIN_DB->Connected` (`source_code/ShrinkPharm/ShrinkPharm.c:L128-L137`).

In the shared DB layer, `SQLMD_connect()` is a macro that expands to `INF_CONNECT(MacUser, MacPass, MacDb)` (`source_code/Include/GenSql.h:L194-L196`).

`INF_CONNECT()` reads ODBC connection settings from environment variables:
- MS‑SQL: `MS_SQL_ODBC_DSN`, `MS_SQL_ODBC_USER`, `MS_SQL_ODBC_PASS`, `MS_SQL_ODBC_DB` (`source_code/GenSql/GenSql.c:L1088-L1092`).
- Informix: `INF_ODBC_DSN`, `INF_ODBC_USER`, `INF_ODBC_PASS` (`source_code/GenSql/GenSql.c:L1109-L1112`).
- Optional connect timeout override: `ODBC_CONNECT_TIMEOUT` (`source_code/GenSql/GenSql.c:L1083-L1086`).

### “Production vs Test” flag (hostname check)

ShrinkPharm sets `TikrotProductionMode` to 0 for a specific list of test/QA hostnames (`linux01-test`, `linux01-qa`, `pharmlinux-test`, `pharmlinux-qa`) (`source_code/ShrinkPharm/ShrinkPharm.c:L79-L89`).

The file also defines `TikrotProductionMode` as a global initialized to 1 with a comment that it exists to avoid “unresolved external” errors (`source_code/ShrinkPharm/ShrinkPharm.c:L24-L27`).

## Function inventory (all functions in ShrinkPharm source files)

Only `ShrinkPharm.c` defines functions; the two `MacODBC_*` files are switch-case fragments rather than full compilation units (see “Note that this code is inserted inside a switch statement…” in `MacODBC_MyOperators.c`) (`source_code/ShrinkPharm/MacODBC_MyOperators.c:L82-L85`).

| Function | File:Line | Purpose |
|----------|-----------|---------|
| `main` | `source_code/ShrinkPharm/ShrinkPharm.c:L35-L359` | Connects to DB, reads purge plan from `shrinkpharm`, iterates target tables, deletes rows older than retention cutoff, commits in batches, writes per-table statistics back to `shrinkpharm`, logs totals, exits. |
| `TerminateHandler` | `source_code/ShrinkPharm/ShrinkPharm.c:L370-L430` | Signal handler for terminating/fatal signals; attempts rollback/disconnect and logs a termination message. Installed for `SIGFPE` in this program (`source_code/ShrinkPharm/ShrinkPharm.c:L100-L116`). |

## Database operations (ODBC layer + SQL)

### Operator IDs used by ShrinkPharm

The ShrinkPharm-specific operator IDs are declared in the shared operator-ID enum as “ShrinkPharm - added 09Mar2021” (`source_code/Include/MacODBC_MyOperatorIDs.h:L1022-L1028`) and also appear in the `ODBC_Operation_Name[]` string array (`source_code/Include/MacODBC_MyOperatorIDs.h:L1903-L1907`).

### SQL used (from `MacODBC_MyOperators.c`)

1) **Control cursor** (`ShrinkPharmControlCur`)
- SQL text selects the configuration columns from `shrinkpharm` for enabled purges and orders by table name (`source_code/ShrinkPharm/MacODBC_MyOperators.c:L90-L99`).
- Cursor is named `ShrinkPharmCtrlCur` (`source_code/ShrinkPharm/MacODBC_MyOperators.c:L96-L96`).

2) **Save statistics** (`ShrinkPharmSaveStatistics`)
- Updates `shrinkpharm.last_run_date`, `last_run_time`, `last_run_num_purged` **for the current control cursor row** (`WHERE CURRENT OF ShrinkPharmCtrlCur`) (`source_code/ShrinkPharm/MacODBC_MyOperators.c:L102-L110`).

3) **Select quantity** (`ShrinkPharmSelectQuantity`)
- Declares the operator as a 1-column output (COUNT result) but leaves SQL text NULL, to be supplied dynamically by caller (`source_code/ShrinkPharm/MacODBC_MyOperators.c:L113-L117`).

4) **Select cursor** (`ShrinkPharmSelectCur`)
- Declares the operator as a 1-column output cursor with name `ShrinkPharmSelCur`, leaving SQL text NULL for the caller to supply dynamically (`source_code/ShrinkPharm/MacODBC_MyOperators.c:L120-L125`).

5) **Delete current row** (`ShrinkPharmDeleteCurrentRow`)
- Leaves SQL text NULL for the caller to supply dynamically, and marks the statement “sticky” (`StatementIsSticky=1`) to avoid re-PREPARE overhead in the inner delete loop (`source_code/ShrinkPharm/MacODBC_MyOperators.c:L127-L130`).

### Dynamic SQL constructed by `ShrinkPharm.c`

For each configured `table_name`/`date_column_name`:

- Optional predicted quantity query (only when `use_where_clause != 0`):
  - `SELECT CAST (count(*) AS INTEGER) FROM <table> WHERE <datecol> < <MinDateToRetain>` (`source_code/ShrinkPharm/ShrinkPharm.c:L193-L200`).

- Select cursor query:
  - With WHERE: `SELECT <datecol> FROM <table> WHERE <datecol> < <MinDateToRetain>` (`source_code/ShrinkPharm/ShrinkPharm.c:L204-L207`).
  - Without WHERE: `SELECT <datecol> FROM <table>` (`source_code/ShrinkPharm/ShrinkPharm.c:L208-L213`).

- Delete command (used repeatedly for the select cursor):
  - `DELETE FROM <table> WHERE CURRENT OF ShrinkPharmSelCur` (`source_code/ShrinkPharm/ShrinkPharm.c:L215-L216`).

### Cursor + COMMIT model

ShrinkPharm sets:
- `ODBC_PRESERVE_CURSORS = 1` “so all cursors will be preserved after COMMITs” (`source_code/ShrinkPharm/ShrinkPharm.c:L124-L127`).

It commits deletions in batches of `commit_count` rows:
- commit threshold check and `CommitAllWork()` call (`source_code/ShrinkPharm/ShrinkPharm.c:L286-L291`).
- final commit of remainder after finishing a table (`source_code/ShrinkPharm/ShrinkPharm.c:L297-L301`).

In `SqlServer.c`, a comment explicitly calls out `ODBC_PRESERVE_CURSORS` as something that “should be set TRUE only for relatively simple programs like ShrinkPharm or As400UnixClient” (`source_code/SqlServer/SqlServer.c:L299-L312`).

### Per-table stats recording

After purging a table, ShrinkPharm writes stats back into the current `shrinkpharm` control row using `ExecSQL(..., ShrinkPharmSaveStatistics, &SysDate, &SysTime, &TotalRowsDeleted, ...)` (`source_code/ShrinkPharm/ShrinkPharm.c:L303-L308`) which corresponds to the UPDATE SQL in `MacODBC_MyOperators.c` (`source_code/ShrinkPharm/MacODBC_MyOperators.c:L102-L110`).

### Sticky statement cleanup

Because `ShrinkPharmDeleteCurrentRow` is marked sticky (`source_code/ShrinkPharm/MacODBC_MyOperators.c:L127-L130`), the code frees it after finishing a target table:
- `FreeStatement(MAIN_DB, ShrinkPharmDeleteCurrentRow)` (`source_code/ShrinkPharm/ShrinkPharm.c:L309-L314`).

## Dependencies

### Headers / libraries

- Includes `<MacODBC.h>` (`source_code/ShrinkPharm/ShrinkPharm.c:L16-L16`), and then uses the shared SQL abstraction macros like `SQLMD_connect()` (`source_code/ShrinkPharm/ShrinkPharm.c:L130-L131`, `source_code/Include/GenSql.h:L194-L196`).
- Links with `-lodbc` (`source_code/ShrinkPharm/Makefile:L19-L23`).

### Shared GenLib / GenSql routines (used directly in ShrinkPharm.c)

- Logging helpers: `GerrLogFnameMini` and `GerrLogReturn` (`source_code/ShrinkPharm/ShrinkPharm.c:L75-L77`, `source_code/ShrinkPharm/ShrinkPharm.c:L112-L116`).
- Date/time helpers: `GetDate()`, `GetTime()`, `IncrementDate(...)` (`source_code/ShrinkPharm/ShrinkPharm.c:L69-L71`, `source_code/ShrinkPharm/ShrinkPharm.c:L189-L191`).
- SQL error handling: `SQLERR_error_test()`, `SQLERR_code_cmp(...)` (`source_code/ShrinkPharm/ShrinkPharm.c:L152-L170`, `source_code/Include/GenSql.h:L184-L190`).

## Cross-references (relationship to other components)

- **FatherProcess shares the same DB-connect abstraction:** FatherProcess connects via `SQLMD_connect()` in a retry loop until `MAIN_DB->Connected` (`source_code/FatherProcess/FatherProcess.c:L259-L271`), which matches ShrinkPharm’s connection strategy (`source_code/ShrinkPharm/ShrinkPharm.c:L128-L137`). (Whether FatherProcess *launches* ShrinkPharm is environment-dependent and needs verification.)
- **PharmTcpServer appears to use SqlServer IPC for database requests:** `NewSystemSend()` obtains a free SQL server (`GetFreeSqlServer(..., SQLPROC_TYPE)`), connects via `ConnectSocketNamed(PharmSqlNamedPipe)`, and writes the request with `WriteSocket(...)` (`source_code/PharmTcpServer/PharmTcpServer.cpp:L648-L683`). This suggests ShrinkPharm’s direct ODBC maintenance is **not** on the normal PharmTcpServer→SqlServer request path, but instead operates as a standalone housekeeping program against the same database tables (configured via `shrinkpharm`) (`source_code/ShrinkPharm/MacODBC_MyOperators.c:L91-L99`).
- **Shared ODBC operator ID namespace:** ShrinkPharm operator IDs live in the shared `MacODBC_MyOperatorIDs.h` enum alongside SQLSERVER operations (`source_code/Include/MacODBC_MyOperatorIDs.h:L1020-L1028`).
- **SqlServer guidance about cursor preservation:** SqlServer comments explicitly name ShrinkPharm as a “simple program” suitable for `ODBC_PRESERVE_CURSORS` (`source_code/SqlServer/SqlServer.c:L309-L312`).
- **Different bootstrap pattern vs long-running servers:** ShrinkPharm connects directly via `SQLMD_connect()` and exits with `exit(0)` (`source_code/ShrinkPharm/ShrinkPharm.c:L128-L137`, `source_code/ShrinkPharm/ShrinkPharm.c:L358-L359`) rather than the `InitSonProcess(...)` bootstrap used by server processes like SqlServer (`source_code/SqlServer/SqlServer.c:L211-L212`). **How/when ShrinkPharm is launched (cron vs FatherProcess) needs verification** because it depends on runtime configuration rather than static references in code.

## Security / robustness notes (code-level)

- **Dynamic SQL built from DB-controlled identifiers:** `table_name` and `date_column_name` are read from the `shrinkpharm` control table (`source_code/ShrinkPharm/MacODBC_MyOperators.c:L91-L99`) and interpolated directly into SQL via `sprintf(...)` (`source_code/ShrinkPharm/ShrinkPharm.c:L195-L207`, `source_code/ShrinkPharm/ShrinkPharm.c:L210-L216`). If the control table contents are not tightly controlled, this creates risk of unintended SQL execution or purging the wrong objects.
- **LogDir depends on `MAC_LOG`:** `strcpy(LogDir, getenv("MAC_LOG"))` will dereference a NULL pointer if `MAC_LOG` is unset (`source_code/ShrinkPharm/ShrinkPharm.c:L72-L72`).
- **Copy/paste artifacts in TerminateHandler messages:** TerminateHandler logs messages that mention “As400UnixServer” (likely copied) (`source_code/ShrinkPharm/ShrinkPharm.c:L388-L390`, `source_code/ShrinkPharm/ShrinkPharm.c:L426-L429`).

## Open questions / needs verification

- What is the exact schema and governance of the `shrinkpharm` control table (DDL, who can edit it), and which target tables are configured in production? (Code references the table but does not define schema beyond selected columns) (`source_code/ShrinkPharm/MacODBC_MyOperators.c:L91-L110`).
- Operational scheduling: is `ShrinkPharm.exe` executed by cron/systemd, by a job scheduler, or launched by `FatherProcess` based on DB parameters? This cannot be proven from this component alone and likely depends on environment/DB configuration.

