# SqlServer - System Architecture

**Component**: SqlServer
**Task ID**: DOC-SQL-001
**Generated**: 2026-02-13
**Source**: `source_code/SqlServer/`

---

## Process Model

SqlServer operates as a **child worker process** within the FatherProcess supervisor hierarchy. According to code at `SqlServer.c:211`, it registers via:
```
InitSonProcess(..., SQLPROC_TYPE, ..., PHARM_SYS)
```

FatherProcess spawns **multiple instances** of SqlServer and maintains minimum non-busy copies during system operation, as stated in the file header at `SqlServer.c:10-12`.

### Process Hierarchy

```
FatherProcess (Supervisor)
├── SqlServer #1 (SQLPROC_TYPE, PHARM_SYS)
├── SqlServer #2 (SQLPROC_TYPE, PHARM_SYS)
├── SqlServer #N ...
├── As400UnixServer (AS400TOUNIX_TYPE)
├── As400UnixClient (UNIXTOAS400_TYPE)
├── PharmTcpServer (PHARM_TCP_SERVER_TYPE)
├── PharmWebServer (WEBSERVERPROC_TYPE)
└── ShrinkPharm (standalone utility)
```

Each SqlServer instance processes one transaction at a time from pharmacy terminals, then returns to a wait state.

---

## Lifecycle: Startup → Steady Loop → Dispatch → Shutdown

### Phase 1: Startup Initialization (SqlServer.c:129-280)

According to code at `SqlServer.c:129-280`, the startup sequence appears to follow this order:

1. **Parse arguments**: `GetCommonPars(argc, argv)` extracts shared-memory parameters from command line (as seen in `SqlServer.c:137`)
2. **Register as child**: `InitSonProcess(SQLPROC_TYPE, PHARM_SYS)` opens semaphore, attaches shared memory, listens on named socket, registers in `PROC_TABLE` (as seen in `SqlServer.c:211`)
3. **Install signal handlers** (as seen in `SqlServer.c:218-272`):
   - `SIGPIPE` → `ClosedPipeHandler` (broken connection cleanup)
   - `SIGTERM` → `TerminateHandler` (graceful shutdown)
   - `SIGFPE` → `TerminateHandler` (arithmetic error)
   - `SIGSEGV` → `SetCustomSegmentationFaultHandler` (passes through to MacODBC pointer validation)
   - `SIGUSR1` → `NihulTikrotEnableHandler` (enables TikrotRPC)
   - `SIGUSR2` → `NihulTikrotDisableHandler` (disables TikrotRPC)

### Phase 2: Database Connection and Configuration (SqlServer.c:281-626)

According to code at `SqlServer.c:281-626`:

1. **Set ODBC parameters**:
   - `LOCK_TIMEOUT = 250` (milliseconds)
   - `DEADLOCK_PRIORITY = 8` (above normal, privileges realtime operations)
   - `ODBC_PRESERVE_CURSORS = 0` (disabled for SqlServer)
2. **Connect to database**: `SQLMD_connect()` in retry loop until `MAIN_DB->Connected`
3. **Load illness bitmaps**: via `bypass_basket_cur` cursor
4. **Load system parameters**: Massive `ExecSQL(READ_sysparams)` populating ~70+ global variables from the `sysparams` table. These include VAT percentages, refill intervals, sale thresholds, and feature flags.
5. **Initialize caches**: `init_package_size_array()`, `LoadTrnPermissions()`, `LoadPrescrSource()`, `InitTikrotSP()`

### Phase 3: Steady-State Main Loop (SqlServer.c:627-952)

According to code at `SqlServer.c:627-952`, the main loop structure appears to be:

```
while (1) {
    UpdateShmParams();            // Refresh from shared memory
    ToGoDown check;               // Check for system shutdown
    caught_signal handling;       // Check for pending signals

    GetSocketMessage(5000000);    // Wait for client request (5-second timeout)

    if (message_received) {
        MakeAndSendReturnMessage();  // Central dispatch
        UpdateLastAccessTime();
    }

    process_spools();             // Process deferred AS/400 spool queue

    // Hourly refresh (every 60+ minutes):
    //   - Re-read sysparams
    //   - init_package_size_array()
    //   - GET_ERROR_SEVERITY()
    //   - LoadTrnPermissions()
    //   - LoadPrescrSource()

    SetFreeSqlServer();           // Mark self as available
}
```

### Phase 4: Shutdown (SqlServer.c:2136-2217)

According to code at `SqlServer.c:2136-2217`, `TerminateProcess()` performs:
1. Audit trail update: `ExecSQL(UPD_sql_srv_audit_ProgramEnd)`
2. Database disconnect: `SQLMD_disconnect()`
3. Process deregistration: `ExitSonProcess()`

---

## Central Dispatch Architecture

### Dispatch Flow (SqlServer.c:955-1918)

The dispatch system is implemented as a single function `MakeAndSendReturnMessage()` spanning approximately 960 lines.

#### Pre-Dispatch Preamble (SqlServer.c:955-1285)

According to code at `SqlServer.c:955-1285`:

1. **JSON detection**: Attempts `cJSON_Parse()` on incoming buffer. If a valid JSON object is found containing `PharmServerRequest.request_number`, the transaction code is extracted from JSON. Otherwise falls back to binary message parsing.
2. **Audit logging**: `ExecSQL(INS_messages_details)` and `ExecSQL(INS_sql_srv_audit)` record the incoming request.
3. **Error tracking setup**: Initializes error arrays and tracking state.
4. **Message header extraction**: Parses terminal ID, pharmacy ID, member ID from the binary or JSON request.

#### Central Switch (SqlServer.c:1286-1687)

According to code at `SqlServer.c:1286-1687`, the dispatch switch `switch(MsgIndex)` routes approximately 60+ transaction codes to handler functions across three files:

- **1xxx** (14 transaction codes) → SqlHandlers.c
- **2xxx** (23 transaction codes) → ElectronicPr.c
- **5xxx** (9 transaction codes) → ElectronicPr.c
- **6xxx** (10 transaction codes) → DigitalRx.c
- **Default**: Sets `ERR_ILLEGAL_MESSAGE_CODE`

#### Post-Dispatch (SqlServer.c:1689-1918)

According to code at `SqlServer.c:1689-1918`:

1. **Spool error handling**: Checks if the error code is in `SpoolErr[]` and queues for retry if needed
2. **Statistics update**: `UpdateTimeStat()` records transaction processing time
3. **Audit update**: `ExecSQL(UPD_messages_details)`, `ExecSQL(UPD_sql_srv_audit_NotInProgress)`
4. **Socket response**: `WriteSocket(output_type_flg + output_buf)` sends response to client
5. **Cleanup**: `CloseSocket()`, `cJSON_Delete()` for JSON requests

---

## Signal Handler Architecture

### Signal Installation (SqlServer.c:218-272)

According to code at `SqlServer.c:218-272`, signals are installed using POSIX `sigaction()`:

| Signal | Handler | Purpose |
|--------|---------|---------|
| SIGPIPE | ClosedPipeHandler | Handles broken socket connections |
| SIGTERM | TerminateHandler | Graceful shutdown |
| SIGFPE | TerminateHandler | Arithmetic error handling |
| SIGSEGV | MacODBC via SetCustomSegmentationFaultHandler | Pointer validation passthrough |
| SIGUSR1 | NihulTikrotEnableHandler | Enables TikrotRPC at runtime |
| SIGUSR2 | NihulTikrotDisableHandler | Disables TikrotRPC at runtime |

### Signal Handler Implementations (SqlServer.c:1920-2217)

According to code at `SqlServer.c:1920-2217`:

**ClosedPipeHandler** (`SqlServer.c:1920-1960`): Handles SIGPIPE by closing the current socket and reinstalling itself. Contains a potential bug at line 1940: `accept_sock == -1` appears to be a comparison (`==`) rather than an assignment (`=`), which could result in a socket resource leak. See VER-SQL-001 in VALIDATION_REPORT.md.

**TerminateHandler** (`SqlServer.c:1968-2130`): Multi-signal handler that:
- For **SIGSEGV**: Passes through to MacODBC's `ODBC_PointerIsValid` mechanism via `siglongjmp`, which allows the ODBC layer to validate va_arg pointers without crashing. If not in pointer validation mode, treats as fatal with endless-loop detection.
- For **SIGTERM/SIGFPE**: Initiates graceful shutdown via `TerminateProcess()`.

**NihulTikrotEnableHandler/DisableHandler** (`SqlServer.c:2131-2135`): Toggle `TikrotRPC_Enabled` flag, allowing runtime control of AS/400 DB2 stored procedure integration.

---

## IPC and Shared Memory Integration

### Shared Memory Tables Used

According to the system context (from `RESEARCH/context_summary.md`), SqlServer accesses the following shared-memory tables through `InitSonProcess`:

| Table | Type | Purpose |
|-------|------|---------|
| PARM_TABLE | PARM_DATA | Runtime parameters (refreshed hourly) |
| PROC_TABLE | PROC_DATA | Process registry (self-registration) |
| STAT_TABLE | STAT_DATA | System status tracking |
| TSTT_TABLE | TSTT_DATA | Transaction time statistics |

### Socket Communication

According to the code, SqlServer communicates via:
- **Named socket (AF_UNIX)**: Receives requests from FatherProcess dispatch via `GetSocketMessage()` with a 5-second timeout
- **Response socket**: Sends responses back via `WriteSocket()` / `CloseSocket()`
- **TCP sockets**: External connections to AS/400 Meishar via SocketAPI.c

---

## Hourly Cache Refresh

According to code at `SqlServer.c:627-952`, the main loop performs periodic refresh operations approximately every hour:

| Cache | Function | Source |
|-------|----------|--------|
| System parameters (~70+ globals) | `ExecSQL(READ_sysparams)` | sysparams DB table |
| Drug package sizes | `init_package_size_array()` | drug_list DB table |
| Error severities | `GET_ERROR_SEVERITY()` | error severity DB table |
| Transaction permissions | `LoadTrnPermissions()` | pharm_trn_permit DB table |
| Prescription sources | `LoadPrescrSource()` | prescr_source DB table |

---

## ODBC Compilation Model

SqlServer is the **primary compilation unit** that instantiates all MacODBC infrastructure. According to code at `SqlServer.c:25`:

```c
#define MAIN
```

This causes `MacODBC.h` to compile all 11 ODBC functions (including the 2,212-line `ODBC_Exec` dispatcher) and instantiate all global variables into the SqlServer binary. Without `#define MAIN`, other source files that include `MacODBC.h` receive `extern` declarations only.

### Include-Injection Chain

According to research (`MacODBC_deepdive.md`), the ODBC operator integration follows this path:

```
Handler function calls ExecSQL(OPERATOR_NAME, ...)
  → ExecSQL macro expands to ODBC_Exec(SINGLETON_SQL_CALL, OPERATOR_NAME, ...)
    → ODBC_Exec looks up OPERATOR_NAME in SQL_GetMainOperationParameters()
      → SQL_GetMainOperationParameters() switch includes MacODBC_MyOperators.c (at MacODBC.h:2747)
        → MacODBC_MyOperators.c includes GenSql_ODBC_Operators.c (at line 87)
```

A parallel injection at `MacODBC.h:2866` includes `MacODBC_MyCustomWhereClauses.c` for custom WHERE clause fragments.

---

## Infrastructure Components

### SocketAPI (SocketAPI.h:1-47, SocketAPI.c:1-265)

According to code at `SocketAPI.c:44-257`, this module provides a lightweight TCP socket wrapper with `select()`-based timeouts:

| Function | Purpose |
|----------|---------|
| SocketCreate | Creates `AF_INET/SOCK_STREAM` with `SO_REUSEADDR` + `SO_KEEPALIVE` |
| SocketReceive | `select()` + `recv()` with configurable timeout |
| SocketSend | `select()` + `send()` loop handling partial writes |
| SocketConnect | `connect()` with timeout via `select()` |
| GetAddress | DNS lookup via `gethostbyname()` with `inet_addr()` fallback |

Note: `gethostbyname()` is a deprecated, non-thread-safe API (VER-SQL-006).

### DebugPrint (DebugPrint.c:1-105)

According to code at `DebugPrint.c:25-58`, this module provides conditional debug logging guarded by `#ifdef __DEBUG_PRINT__`. It uses `vsprintf()` into a `char[10000]` buffer without bounds checking, creating a potential buffer overflow risk (VER-SQL-002).

### AS/400 Meishar TCP Client (As400UnixMediator.h:1-153, As400UnixMediator.c:1-660)

According to code at `As400UnixMediator.h:38-44` and `As400UnixMediator.c:106-278`:

- **Target**: Server "AS400" on port 9400
- **Timeouts**: RECV_TIMEOUT=10s, SEND_TIMEOUT=4s
- **Protocol**: Semicolon-delimited text, framed as `<length>:<payload>`
- **Error suppression**: 180-second window to avoid emergency email alerts on sporadic failures
- **Maintenance window**: Suppresses alerts between 23:00-01:00

### TikrotRPC (TikrotRPC.c:1-494)

According to code at `TikrotRPC.c:304-444`:

- **Target**: DB2 stored procedure `RKPGMPRD.TIKROT`
- **DSN selection**: Reads `/pharm/bin/NihulTikrot.cfg` — defaults to "ASTEST", uses "AS400" if file contains "PRODUCTION"
- **Retry**: Retry-with-recursion on `SQL_ERROR` with 500ms sleep
- **Timeout**: 3-second query timeout via `SQL_ATTR_QUERY_TIMEOUT`
- **APIs**: Uses deprecated ODBC 2.x handle allocation (SQLAllocEnv, SQLAllocConnect, SQLAllocStmt) — see VER-SQL-003

---

## Data Flow Diagram

```
Pharmacy Terminal
    │
    ▼ (TCP via FatherProcess/PharmTcpServer)
┌──────────────────────────────────────────────────────────┐
│ SqlServer Process                                        │
│                                                          │
│  GetSocketMessage(5s timeout)                            │
│       │                                                  │
│       ▼                                                  │
│  MakeAndSendReturnMessage()                              │
│       │                                                  │
│       ├─ JSON parse (cJSON) ──or── Binary parse          │
│       │                                                  │
│       ▼                                                  │
│  switch(MsgIndex) ── ~60+ transaction codes              │
│       │                                                  │
│       ├── 1xxx ──► SqlHandlers.c (pharmacy ops)          │
│       ├── 2xxx ──► ElectronicPr.c (E-Rx)                 │
│       ├── 5xxx ──► ElectronicPr.c (Nihul Tikrot)         │
│       └── 6xxx ──► DigitalRx.c (Digital Rx)              │
│                        │                                 │
│                        ├── MessageFuncs.c (validation)   │
│                        ├── MacODBC (ODBC operations)     │
│                        ├── AS/400 Meishar (TCP:9400)     │
│                        └── TikrotRPC (DB2 SP)            │
│                                                          │
│  WriteSocket(response) ──► Pharmacy Terminal              │
└──────────────────────────────────────────────────────────┘
        │                           │
        ▼                           ▼
   MS-SQL/Informix            AS/400 Systems
   (via MacODBC)              (Meishar + DB2)
```

---

*Generated by CIDRA Documenter Agent - DOC-SQL-001*
