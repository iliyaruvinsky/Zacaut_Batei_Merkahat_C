# SqlServer (source_code/SqlServer) — Deep Dive (RES-DEEPDIVE-001)

**Generated:** 2026-02-02  
**Scope:** `source_code/SqlServer/` (WIP — inventory complete; deep read in progress)  

## File inventory (exact)

Total files: **22** (exact). Total lines across folder: **84,407** (exact).

| File | Lines (exact) | Role / notes |
|------|--------------:|--------------|
| `source_code/SqlServer/ElectronicPr.c` | 26161 | Large feature module (not yet deep-read; requires chunked read). |
| `source_code/SqlServer/DigitalRx.c` | 22955 | Large feature module (not yet deep-read; requires chunked read). |
| `source_code/SqlServer/MessageFuncs.c` | 13631 | Large message/serialization helpers (not yet deep-read). |
| `source_code/SqlServer/SqlHandlers.c` | 8458 | Large SQL request handlers (not yet deep-read). |
| `source_code/SqlServer/MacODBC_MyOperators.c` | 8418 | ODBC operator definitions (not yet deep-read). |
| `source_code/SqlServer/SqlServer.c` | 2362 | Main server loop / orchestration (not yet deep-read). |
| `source_code/SqlServer/As400UnixMediator.c` | 660 | TCP client to AS/400 “Meishar” eligibility/sale-record services (read; details below). |
| `source_code/SqlServer/TikrotRPC.c` | 494 | ODBC call to DB2 stored procedure “Nihul Tikrot” (read; details below). |
| `source_code/SqlServer/MacODBC_MyCustomWhereClauses.c` | 274 | Case fragments defining custom SQL WHERE clauses for operator IDs (read; details below). |
| `source_code/SqlServer/SocketAPI.c` | 265 | Minimal TCP socket wrapper for connect/send/recv (read; details below). |
| `source_code/SqlServer/SocketAPI.h` | 47 | `SocketAPI.c` prototypes + small typedefs (read). |
| `source_code/SqlServer/As400UnixMediator.h` | 153 | AS/400 mediator API + constants (read). |
| `source_code/SqlServer/DebugPrint.c` | 105 | Debug logging helper (read). |
| `source_code/SqlServer/Makefile` | 67 | Unix build for `sqlserver.exe` (read). |
| `source_code/SqlServer/SqlServer.vcxproj` | 101 | Visual Studio project metadata (read). |
| `source_code/SqlServer/SqlServer.vcxproj.filters` | 89 | Visual Studio filter mapping (read). |
| `source_code/SqlServer/SqlServer.vcxproj.vspscc` | 10 | Visual Studio source-control metadata (read). |
| `source_code/SqlServer/SqlServer.vcxproj.user` | 6 | Visual Studio per-user stub (read). |
| `source_code/SqlServer/.project` | 27 | Eclipse CDT project metadata (read). |
| `source_code/SqlServer/.cproject` | 79 | Eclipse CDT toolchain/build-target configuration (read). |
| `source_code/SqlServer/.cdtproject` | 33 | Eclipse CDT path entries + make targets (read). |
| `source_code/SqlServer/.settings/language.settings.xml` | 12 | Eclipse language settings providers config (read). |

## Build + IDE configuration artifacts

### Makefile (Unix build)

- Builds `sqlserver.exe` from `SqlServer.c`, `SqlHandlers.c`, `SocketAPI.c`, `DebugPrint.c`, `As400UnixMediator.c`, `ElectronicPr.c`, `TikrotRPC.c`, `MessageFuncs.c`, `DigitalRx.c` (`source_code/SqlServer/Makefile:L14-L16`).
- Links against ODBC and libcurl (`-lodbc -lcurl`) (`source_code/SqlServer/Makefile:L41-L43`).
- Includes shared build flags/rules from `../Util/Defines.mak` and `../Util/Rules.mak` (`source_code/SqlServer/Makefile:L4-L4`, `source_code/SqlServer/Makefile:L31-L31`).

### Visual Studio project (Windows build metadata)

- `ConfigurationType=Application`, toolset `v143`, `CharacterSet=MultiByte` (`source_code/SqlServer/SqlServer.vcxproj:L24-L36`).
- Compiles a mix of local sources and also pulls in `..\Include\GenSql_ODBC_Operators.c` and `..\Include\cJSON.c` (`source_code/SqlServer/SqlServer.vcxproj:L83-L97`).
- Includes headers `..\Include\MacODBC.h`, `..\Include\MacODBC_MyOperatorIDs.h`, `..\Include\GenSql_ODBC_OperatorIDs.h` (`source_code/SqlServer/SqlServer.vcxproj:L71-L78`).

### Eclipse CDT project metadata (Unix/Linux toolchain)

- Eclipse project name is `SqlServer` with CDT managed build natures (`source_code/SqlServer/.project:L2-L26`).
- Build targets include `All`→`make all` and `Clean`→`make clean` (`source_code/SqlServer/.cproject:L58-L76`; also `.cdtproject` has `all/clean` targets at `source_code/SqlServer/.cdtproject:L16-L30`).

## Code deep dive (in progress)

### `SocketAPI.h` (47 lines) — minimal TCP socket API

Defines a small typedef set (`CINT`, `PBYTE`, `PINT`, `PCHAR`, `BOOL`) and provides the core prototypes:

- `SocketCreate`, `SocketReceive`, `SocketSend`, `SocketConnect`, `GetAddress` (`source_code/SqlServer/SocketAPI.h:L40-L46`).

### `SocketAPI.c` (265 lines) — TCP connect/send/recv using `select()`

**Purpose:** Lightweight TCP wrapper with timeouts.

- `SocketCreate()` creates `AF_INET/SOCK_STREAM` and sets `SO_REUSEADDR` + `SO_KEEPALIVE`; it logs errors via `DebugPrint` and returns `-1` on failure (`source_code/SqlServer/SocketAPI.c:L44-L58`).
- `SocketReceive()` waits on readability using `select()` with timeout then `recv()`s into the caller buffer; it logs select/recv errors via both `DebugPrint` and `GerrLogMini` (`source_code/SqlServer/SocketAPI.c:L70-L113`).
- `SocketSend()` loops until all bytes are sent, using `select()` for writeability; uses `send()` and logs errors (`source_code/SqlServer/SocketAPI.c:L131-L193`).
- `GetAddress()` does a name lookup via `gethostbyname()` and falls back to `inet_addr()` for dotted IPs (deprecated in comments) (`source_code/SqlServer/SocketAPI.c:L222-L257`).

**Dependencies:**

- Includes `Global.h` and uses GenLib logging via `GerrLogMini` (`source_code/SqlServer/SocketAPI.c:L17-L17`, `source_code/SqlServer/SocketAPI.c:L87-L88`).
- Uses `DebugPrint` for trace logging (`source_code/SqlServer/SocketAPI.c:L24-L25`).

### `DebugPrint.c` (105 lines) — ad-hoc debug logger (guarded by `__DEBUG_PRINT__`)

- `DebugPrint()` appends a timestamp and formatted message into a configured file (`source_code/SqlServer/DebugPrint.c:L25-L58`).
- Log file name is initialized on first use via `SetDebugFileName(DEFAULT_LOG_DIR, DEFAULT_FILE_NAME)` (`source_code/SqlServer/DebugPrint.c:L35-L38`).

**Security / robustness notes:**

- Uses unbounded `vsprintf()` into `char szBuf[10000]` (`source_code/SqlServer/DebugPrint.c:L52-L56`) → buffer overflow risk if the format expands beyond the buffer.

### `As400UnixMediator.h` (153 lines) — AS/400 “Meishar” interface + protocol constants

- Targets `SERVER "AS400"` and `PORT 9400` (`source_code/SqlServer/As400UnixMediator.h:L38-L41`).
- Defines timeouts `RECV_TIMEOUT=10` seconds and `SEND_TIMEOUT=4` seconds (`source_code/SqlServer/As400UnixMediator.h:L42-L44`).
- Declares primary APIs:
  - `as400EligibilityTest(...)` (`source_code/SqlServer/As400UnixMediator.h:L102-L119`)
  - `as400SaveSaleRecord(...)` (`source_code/SqlServer/As400UnixMediator.h:L121-L137`)

### `As400UnixMediator.c` (660 lines) — TCP client for “eligibility” + “save sale record”

**High-level behavior:**

- Builds a semicolon-delimited request string `szText` and frames it as `<len>:<payload>` (`source_code/SqlServer/As400UnixMediator.c:L106-L123`).
- Connects to AS/400 host `SERVER` on `PORT` using the local `SocketAPI` wrapper; implements retries and sleeps between attempts (`source_code/SqlServer/As400UnixMediator.c:L124-L278`).
- Parses responses in `ParseString()` using `DELIMITER1=';'` and converts fields using `atol/atoi` (`source_code/SqlServer/As400UnixMediator.c:L308-L389`).

**Operational/logging behavior:**

- Contains recent logic to suppress “sporadic” connection-failure logs from triggering emergency email alerts, using an “error window” (default 180 seconds) and tracking last-success timestamps (`source_code/SqlServer/As400UnixMediator.c:L20-L29`, `source_code/SqlServer/As400UnixMediator.c:L234-L241`).

### `TikrotRPC.c` (494 lines) — ODBC call to DB2 stored procedure for “Nihul Tikrot”

**Purpose:** Call DB2 stored procedure `RKPGMPRD.TIKROT` and return multiple output blocks.

Key behaviors:

- Reads `/pharm/bin/NihulTikrot.cfg` to decide DSN: default `ASTEST`, or `AS400` if the file contains `PRODUCTION` (`source_code/SqlServer/TikrotRPC.c:L331-L351`).
- Uses deprecated ODBC handle allocation APIs (`SQLAllocEnv`, `SQLAllocConnect`, `SQLAllocStmt`) (notes are embedded in comments) (`source_code/SqlServer/TikrotRPC.c:L304-L328`, `source_code/SqlServer/TikrotRPC.c:L417-L431`).
- Sets statement query timeout `SQL_ATTR_QUERY_TIMEOUT` to 3 seconds (`source_code/SqlServer/TikrotRPC.c:L435-L438`).
- Prepares a stored procedure call statement: `call RKPGMPRD.TIKROT (?, ?, ?, ?, ?, ?, ?, ?)` (`source_code/SqlServer/TikrotRPC.c:L440-L444`).
- Implements retry-on-`SQL_ERROR` for `SQLExecute()` with a 500ms sleep, using recursion with `RPC_Recursed` to prevent infinite recursion (`source_code/SqlServer/TikrotRPC.c:L213-L239`).
- Logs DSN + user on connect attempts (but does **not** log password) (`source_code/SqlServer/TikrotRPC.c:L365-L370`).

### `MacODBC_MyCustomWhereClauses.c` (274 lines) — custom WHERE clause fragments (case statements)

This file is **not** a standalone C compilation unit: it appears to contain only `case ...:` fragments assigning `WhereClauseText`, `NumInputColumns`, and `InputColumnSpec` — likely intended to be included into a larger `switch` statement inside an operator/SQL builder.

Examples:

- `ChooseNewlyUpdatedRows` provides a multi-condition update-date/time filter and `ORDER BY` and declares three input columns (`source_code/SqlServer/MacODBC_MyCustomWhereClauses.c:L21-L27`).
- `Find_diagnosis_from_member_diagnoses` and `Find_diagnosis_from_special_prescs` embed `SELECT DISTINCT ...` SQL text (subquery-style clauses) with parameter placeholders (`source_code/SqlServer/MacODBC_MyCustomWhereClauses.c:L30-L53`).

## Dependencies (SqlServer → other components)

- **GenSql / ODBC layer**: uses `MacODBC` headers and operator IDs (Visual Studio project references `..\Include\MacODBC.h`, `..\Include\GenSql_ODBC_OperatorIDs.h`) (`source_code/SqlServer/SqlServer.vcxproj:L71-L78`).
- **GenLib logging**: `SocketAPI.c` calls `GerrLogMini`, which is implemented in `source_code/GenLib/GeneralError.c` (`source_code/SqlServer/SocketAPI.c:L87-L88`).

## Open questions / follow-ups (for continued deep read)

- The large files (`ElectronicPr.c`, `DigitalRx.c`, `MessageFuncs.c`, `SqlHandlers.c`, `MacODBC_MyOperators.c`, `SqlServer.c`) need full chunked read to identify:
  - entrypoints / main accept loop
  - request routing model and message framing
  - SQL operator usage patterns and query construction
  - cross-component IPC patterns (sockets/shared memory) and table usage
  - any credential/PII logging and input validation

