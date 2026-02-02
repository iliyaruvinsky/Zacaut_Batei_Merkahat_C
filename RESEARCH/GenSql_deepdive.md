# GenSql (source_code/GenSql) — Deep Dive (RES-DEEPDIVE-001)

**Generated:** 2026-02-02  
**Scope:** `source_code/GenSql/` (read every file in folder)  

## File inventory (exact)

Total files: **14** (exact). Total lines across folder: **7,760** (exact).

| File | Lines (exact) | Role / notes |
|------|--------------:|--------------|
| `source_code/GenSql/GenSql.ec` | 4502 | Legacy embedded-SQL source (many blocks now commented out); contains refreshers for many shm tables and “classic” isolation globals. |
| `source_code/GenSql/GenSql.c` | 2836 | Current ODBC-first implementation (mirroring-aware), defines `TableTab[]` schema used by shared-memory engine; core DB error handling and server refresh routines. |
| `source_code/GenSql/DebugPrn.c` | 34 | Standalone debug logger to `/pharm/bin/trace.log` (unbounded `vsprintf`). |
| `source_code/GenSql/MacODBC_MyCustomWhereClauses.c` | 10 | Comment-only placeholder (example `WHERE_...` case). |
| `source_code/GenSql/Makefile` | 47 | Builds `libgensql.a` from `GenSql.c` (ODBC), links `-lodbc`. |
| `source_code/GenSql/Makefile_EMBEDDED` | 47 | Builds `libgensql.a` from `GenSql.ec` using `$(ESQL)`; emits intermediate `GenSql.c`. |
| `source_code/GenSql/GenSql.vcxproj` | 80 | Visual Studio project metadata (builds `DebugPrn.c` + `GenSql.c`). |
| `source_code/GenSql/GenSql.vcxproj.filters` | 30 | Visual Studio filters. |
| `source_code/GenSql/GenSql.vcxproj.vspscc` | 10 | Visual Studio source-control metadata. |
| `source_code/GenSql/GenSql.vcxproj.user` | 3 | Visual Studio per-user stub. |
| `source_code/GenSql/.project` | 38 | Eclipse CDT project metadata. |
| `source_code/GenSql/.cproject` | 78 | Eclipse CDT toolchain/build-target configuration. |
| `source_code/GenSql/.cdtproject` | 33 | Eclipse CDT path entries + make targets. |
| `source_code/GenSql/.settings/language.settings.xml` | 12 | Eclipse language settings providers config. |

## Build + IDE configuration artifacts

### Makefile (ODBC build)

- Builds static library `libgensql.a` from `GenSql.c` (`source_code/GenSql/Makefile:L16-L23`).
- Compiles with ODBC linkage flags `-lodbc` (and `-D _GNU_SOURCE`) (`source_code/GenSql/Makefile:L40-L41`).

### Makefile_EMBEDDED (embedded-SQL build)

- Uses `$(ESQL)` to compile `GenSql.ec`, emitting intermediate `GenSql.c` (`INTERMS=GenSql.c`) and producing `libgensql.a` (`source_code/GenSql/Makefile_EMBEDDED:L16-L24`, `source_code/GenSql/Makefile_EMBEDDED:L40-L47`).

### Visual Studio project (Windows build metadata)

- `ConfigurationType=Application` (not a static lib) and compiles `DebugPrn.c` + `GenSql.c` (`source_code/GenSql/GenSql.vcxproj:L24-L36`, `source_code/GenSql/GenSql.vcxproj:L70-L76`).

## Code deep dive

### `DebugPrn.c` (34 lines) — ad-hoc trace logger

- Writes to fixed path `"/pharm/bin/trace.log"` (`source_code/GenSql/DebugPrn.c:L6-L6`).
- Uses `vsprintf()` into a 1024-byte stack buffer (`source_code/GenSql/DebugPrn.c:L13-L15`, `source_code/GenSql/DebugPrn.c:L27-L31`) → overflow risk if format expands beyond buffer.

### `GenSql.c` (2836 lines) — ODBC-first DB layer + shared-memory schema

#### Shared-memory schema: `TableTab[]`

`GenSql.c` defines the `TableTab[]` array consumed by the GenLib shared-memory engine (`extern TABLE_DATA TableTab[];` in `SharedMemory.cpp`) to describe tables, record sizes, refresh hooks, and comparators.

- Active entries include: `PARM_TABLE`, `PROC_TABLE`, `STAT_TABLE`, `TSTT_TABLE`, `DSTT_TABLE`, `UPDT_TABLE`, `MSG_RECID_TABLE`, `PRSC_TABLE`, `DIPR_TABLE` (`source_code/GenSql/GenSql.c:L86-L126`).
- Many historical tables (pharmacy, percent, interactions, etc.) exist only as commented-out entries in this ODBC build (`source_code/GenSql/GenSql.c:L102-L121`).

#### DB connection model (ODBC, dual DB support)

`INF_CONNECT()` opens ODBC connections based on environment variables, with optional MS‑SQL + Informix in the same process and a configurable default (`MAIN_DB`) (`source_code/GenSql/GenSql.c:L1065-L1217`).

- MS‑SQL env vars: `MS_SQL_ODBC_DSN`, `MS_SQL_ODBC_USER`, `MS_SQL_ODBC_PASS`, `MS_SQL_ODBC_DB` (`source_code/GenSql/GenSql.c:L1088-L1096`).
- Informix env vars: `INF_ODBC_DSN`, `INF_ODBC_USER`, `INF_ODBC_PASS` (`source_code/GenSql/GenSql.c:L1109-L1117`).
- Optional connect timeout override: `ODBC_CONNECT_TIMEOUT` (`source_code/GenSql/GenSql.c:L1083-L1086`).
- If both DBs are configured, `ODBC_DEFAULT_DATABASE` selects which DB becomes `MAIN_DB` (`source_code/GenSql/GenSql.c:L1144-L1164`); `ALT_DB` is set to the “other” DB if connected (`source_code/GenSql/GenSql.c:L1195-L1205`).
- Optional mirroring flag: `ODBC_MIRRORING_ENABLED` (`source_code/GenSql/GenSql.c:L1167-L1177`).

#### Error handling + auto-reconnect

`INF_ERROR_TEST()` is the central error handler: it logs diagnostics, includes the last SQL statement (from ODBC layer), and attempts auto-disconnect/reconnect for “DB disconnected” error families (`source_code/GenSql/GenSql.c:L505-L741`).

- Uses `ODBC_ErrorBuffer` and `ODBC_LastSQLStatementText` for message + SQL text (`source_code/GenSql/GenSql.c:L561-L574`).
- Auto-reconnect triggers for a list of disconnection codes including `DB_CONNECTION_BROKEN` and sets `SQLCODE` to synthetic `DB_CONNECTION_RESTORED` on success (`source_code/GenSql/GenSql.c:L687-L728`, `source_code/GenSql/GenSql.c:L720-L728`).
- Adds a general 250ms delay after any DB error if `_PauseAfterFailedReconnect` is set (`source_code/GenSql/GenSql.c:L736-L738`).

`INF_CODE_CMP()` implements “error classification” for many SQLERR_* cases and also contains a second auto-reconnect path in the access-conflict classifier (`source_code/GenSql/GenSql.c:L769-L936`, `source_code/GenSql/GenSql.c:L808-L877`).

#### Refresh + ID allocation via shared memory

`RefreshPrescriptionTable()` populates the shared-memory “ID tables” for prescriptions and message record IDs:

- Reads ranges from `presc_per_host` and then stores the next values into `PRSC_TABLE` and `MSG_RECID_TABLE` using `OpenTable` + `SetRecordSize` + `AddItem` (`source_code/GenSql/GenSql.c:L1408-L1649`).
- ID consumers then call `GET_PRESCRIPTION_ID()` / `GET_MSG_REC_ID()` which read+increment IDs using `ActItems(..., GetPrescriptionId)` / `ActItems(..., GetMessageRecId)` (`source_code/GenSql/GenSql.c:L2001-L2033`, `source_code/GenSql/GenSql.c:L2044-L2077`).

#### `GetDbTimestamp()` used by logging

`GetDbTimestamp()` attempts to read `READ_GetCurrentDatabaseTime` via `ExecSQL` when connected and not inside ODBC recursion; otherwise it falls back to a host `ctime()` timestamp and returns an `HH:MM:SS` string (`source_code/GenSql/GenSql.c:L2260-L2329`).

Notable: it saves and restores `SQLCODE` and `sqlerrd[2]` around its internal query to avoid clobbering the caller’s SQL status (`source_code/GenSql/GenSql.c:L2276-L2283`, `source_code/GenSql/GenSql.c:L2321-L2328`).

#### Security notes (GenSql.c)

- **Credential logging risk:** On some connect-failure paths, `INF_CONNECT()` logs ODBC DSN/user/**password**/db (`source_code/GenSql/GenSql.c:L1101-L1105`, `source_code/GenSql/GenSql.c:L1121-L1125`). This can leak DB credentials into logs.
- **SQL text logging risk:** `INF_ERROR_TEST()` logs the last SQL statement text (`source_code/GenSql/GenSql.c:L564-L574`). SQL can include PII depending on query construction patterns elsewhere.

### `GenSql.ec` (4502 lines) — embedded-SQL variant (legacy/compat)

This file contains older embedded SQL (`EXEC SQL ...`) patterns; many of them are now commented out and replaced with `ExecSQL(...)` / ODBC glue. It also defines a *different* (larger) `TableTab[]` schema than the ODBC build.

#### Key differences vs `GenSql.c`

- `TableTab[]` includes many additional refresh-driven shared memory tables (pharmacy, percent, interactions, delete-prescription IDs, etc.) (`source_code/GenSql/GenSql.ec:L92-L132`).
- `_db_connected_` and `_PauseAfterFailedReconnect` defaults differ (`source_code/GenSql/GenSql.ec:L137-L142`).
- Several refreshers exist here (e.g., `RefreshSeverityTable`) that actively populate shm tables and are used by lookup helpers (`source_code/GenSql/GenSql.ec:L1607-L1766`, `source_code/GenSql/GenSql.ec:L3347-L3453`).

#### Security notes (GenSql.ec)

- **Credential logging risk:** `INF_CONNECT()` logs ODBC DSN/user/**password**/db for MS‑SQL (`source_code/GenSql/GenSql.ec:L1042-L1045`).

### `MacODBC_MyCustomWhereClauses.c` (10 lines)

Only contains a commented example case; no compiled code (`source_code/GenSql/MacODBC_MyCustomWhereClauses.c:L1-L10`).

## Dependencies (GenSql → other components)

- **GenLib (shared memory + IPC):**
  - Uses `OpenTable`, `CloseTable`, `SetRecordSize`, `AddItem`, `ActItems` which are implemented in `source_code/GenLib/SharedMemory.cpp` (see `RESEARCH/GenLib_deepdive.md`).
  - Uses GenLib logging (`GerrLogMini`, `GerrLogReturn`, `GmsgLogReturn`) implemented in `source_code/GenLib/GeneralError.c`.
- **ODBC layer:** Uses `MacODBC.h` and links against `-lodbc` (`source_code/GenSql/GenSql.c:L32-L33`, `source_code/GenSql/Makefile:L40-L41`).

## Open questions / follow-ups

- Where are the `ExecSQL(..., READ_...)` operator IDs and the actual SQL text defined (likely in a MacODBC operator table elsewhere)? GenSql references many `READ_*`/`SET_*` identifiers (e.g., `READ_presc_per_host`) but does not define them (`source_code/GenSql/GenSql.c:L1460-L1464`).

