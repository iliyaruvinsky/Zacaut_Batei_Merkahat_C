# MACCABI C Backend - Research Summary

## System Overview

The system is a multi-process C backend built around a watchdog supervisor (`FatherProcess`) that initializes shared resources and starts configured child processes.

Key observable behaviors in `FatherProcess`:
- Installs a SIGTERM handler for “soft” shutdown (`sigaction(SIGTERM, ...)`). (`source_code/FatherProcess/FatherProcess.c:L239-L253`)
- Connects to the database via `SQLMD_connect()` in a retry loop until `MAIN_DB->Connected`. (`source_code/FatherProcess/FatherProcess.c:L259-L271`)
- Creates an internal “named pipe” endpoint and listens on it via `ListenSocketNamed`. (`source_code/FatherProcess/FatherProcess.c:L299-L301`)
- Creates a system semaphore and initializes shared memory. (`source_code/FatherProcess/FatherProcess.c:L303-L308`)
- Creates/refreshes shared-memory tables by iterating `TableTab[]`. (`source_code/FatherProcess/FatherProcess.c:L309-L334`)
- Registers itself in shared memory as `FATHERPROC_TYPE` for both subsystems (`PHARM_SYS | DOCTOR_SYS`). (`source_code/FatherProcess/FatherProcess.c:L350-L351`)

## Architecture

### High-level process + IPC map

```mermaid
flowchart TD
  FatherProcess[FatherProcess] -->|CreateSemaphore| Semaphore[SystemSemaphore]
  FatherProcess -->|"InitFirstExtent(CreateSharedMemory)"| SharedMemory[SharedMemoryExtents]
  FatherProcess -->|"CreateTableLoop(TableTab)"| ShmTables[SharedMemoryTables]
  FatherProcess -->|ListenSocketNamed| FatherNamedSock[FatherNamedSocket]

  ChildProc[ChildProcess] -->|OpenSemaphore| Semaphore
  ChildProc -->|OpenFirstExtent| SharedMemory
  ChildProc -->|AddCurrProc| ShmTables
  ChildProc -->|ListenSocketNamed| ChildNamedSock[ChildNamedSocket]
  OtherProc[PeerProcess] -->|ConnectSocketNamed| ChildNamedSock

  SqlLayer[SQLMD_connect] -->|INF_CONNECT| DB[ODBC_Databases]
  As400Bridge[AS400_Unix_Bridge] -->|TCP| AS400[AS400_System]
  WebGateway[PharmWebServer] -->|HTTP(served)| Client[HTTP_Client]
```

### Boot / initialization sequence (FatherProcess)

1) **Signals**: installs a SIGTERM handler (`TerminateHandler`) to trigger “soft shutdown”. (`source_code/FatherProcess/FatherProcess.c:L239-L253`, `source_code/FatherProcess/FatherProcess.c:L1937-L1972`)

2) **DB connect**: runs `SQLMD_connect()` until `MAIN_DB->Connected` becomes true. (`source_code/FatherProcess/FatherProcess.c:L259-L271`)

3) **Named IPC endpoint**: builds a per-process path (via `GetCurrNamedPipe`) and calls `ListenSocketNamed` to accept unix-domain connections. (`source_code/FatherProcess/FatherProcess.c:L299-L301`, `source_code/GenLib/Sockets.c:L89-L115`)

4) **Semaphore**: creates a system semaphore (intended to be called “only by Father process”). (`source_code/FatherProcess/FatherProcess.c:L303-L305`, `source_code/GenLib/Semaphores.c:L100-L167`)

5) **Shared memory**:
- Initializes shared memory extents (`InitFirstExtent(SharedSize)`). (`source_code/FatherProcess/FatherProcess.c:L306-L308`)
- Creates each shared-memory table by iterating `TableTab[]`, and invokes refresh functions when present. (`source_code/FatherProcess/FatherProcess.c:L309-L334`)
- `TableTab[]` is defined in `GenSql.c` and lists the canonical shared-memory tables and their row sizes/refresh behavior. (`source_code/GenSql/GenSql.c:L86-L126`)

### Child-process startup pattern

Child processes follow a shared initialization routine `InitSonProcess()`:
- Initializes and hashes environment
- Opens semaphore (`OpenSemaphore()`)
- Attaches to shared memory (`OpenFirstExtent()`)
- Creates a named socket path and listens (`GetCurrNamedPipe` + `ListenSocketNamed`)
- Registers itself into shared memory tables (`AddCurrProc`) (`source_code/GenLib/Memory.c:L552-L612`)

FatherProcess spawns children via `Run_server()` which:
- Resolves executable under `BinDir`
- Builds argument list from command string + shared-memory parameters
- `fork()`s and executes the new process (parent/child branch in `switch(fork())`) (`source_code/GenLib/Memory.c:L1023-L1193`)

## Components

For full per-component details (including exact line counts and include sets), see `RESEARCH/component_profiles.md`.

| Component | Main entry | Subsystem/process evidence |
|---|---|---|
| FatherProcess | `source_code/FatherProcess/FatherProcess.c` | Registers as `PHARM_SYS | DOCTOR_SYS` (`source_code/FatherProcess/FatherProcess.c:L350-L351`) |
| SqlServer | `source_code/SqlServer/SqlServer.c` | `InitSonProcess(..., SQLPROC_TYPE, ..., PHARM_SYS)` (`source_code/SqlServer/SqlServer.c:L211-L211`) |
| As400UnixServer | `source_code/As400UnixServer/As400UnixServer.c` | `InitSonProcess(..., AS400TOUNIX_TYPE, ..., PHARM_SYS)` (`source_code/As400UnixServer/As400UnixServer.c:L194-L194`) |
| As400UnixClient | `source_code/As400UnixClient/As400UnixClient.c` | `InitSonProcess(..., UNIXTOAS400_TYPE, ..., PHARM_SYS)` (`source_code/As400UnixClient/As400UnixClient.c:L174-L174`) |
| As400UnixDocServer | `source_code/As400UnixDocServer/As400UnixDocServer.c` | `InitSonProcess(..., DOCAS400TOUNIX_TYPE, ..., DOCTOR_SYS)` (`source_code/As400UnixDocServer/As400UnixDocServer.c:L88-L88`) |
| As400UnixDoc2Server | `source_code/As400UnixDoc2Server/As400UnixDoc2Server.c` | `InitSonProcess(..., DOC2AS400TOUNIX_TYPE, ..., DOCTOR_SYS)` (`source_code/As400UnixDoc2Server/As400UnixDoc2Server.c:L101-L107`) |
| PharmTcpServer | `source_code/PharmTcpServer/PharmTcpServer.cpp` | `SubSystem = DOCTOR_SYS` and `InitSonProcess(..., PHARM_TCP_SERVER_TYPE, ..., SubSystem)` (`source_code/PharmTcpServer/PharmTcpServer.cpp:L91-L92`, `source_code/PharmTcpServer/PharmTcpServer.cpp:L124-L128`) |
| PharmWebServer | `source_code/PharmWebServer/PharmWebServer.cpp` | `SubSystem = PHARM_SYS` and `InitSonProcess(..., WEBSERVERPROC_TYPE, ..., SubSystem)` (`source_code/PharmWebServer/PharmWebServer.cpp:L92-L92`, `source_code/PharmWebServer/PharmWebServer.cpp:L123-L128`) |
| ShrinkPharm | `source_code/ShrinkPharm/ShrinkPharm.c` | ODBC maintenance utility (“ODBC equivalent of ShrinkDoctor…”) (`source_code/ShrinkPharm/ShrinkPharm.c:L7-L10`) |

## Shared Resources

### Shared-memory tables (schema)

The shared-memory table schema is defined by `TableTab[]` in `GenSql.c`: (`source_code/GenSql/GenSql.c:L86-L126`)

| Table name macro | Table name string | Row type | Notes |
|---|---|---|---|
| `PARM_TABLE` | `parm_table` | `PARM_DATA` | Parameters (loaded from DB into shm) (`source_code/Include/Global.h:L514-L523`) |
| `PROC_TABLE` | `proc_table` | `PROC_DATA` | Process registry (`source_code/Include/Global.h:L555-L582`) |
| `STAT_TABLE` | `stat_table` | `STAT_DATA` | System status (`source_code/Include/Global.h:L599-L608`) |
| `TSTT_TABLE` | `tstt_table` | `TSTT_DATA` | Pharmacy statistics (`source_code/Include/Global.h:L626-L646`) |
| `DSTT_TABLE` | `dstt_table` | `DSTT_DATA` | Doctor statistics (`source_code/Include/Global.h:L674-L693`) |
| `UPDT_TABLE` | `updt_table` | `UPDT_DATA` | “table updated” timestamps (`source_code/Include/Global.h:L524-L533`) |
| `MSG_RECID_TABLE` | `msg_recid_table` | `MESSAGE_RECID_ROW` | Message rec-id tracking (`source_code/GenSql/GenSql.c:L112-L117`) |
| `PRSC_TABLE` | `prsc_table` | `PRESCRIPTION_ROW` | Prescription tracking (`source_code/GenSql/GenSql.c:L116-L117`) |
| `DIPR_TABLE` | `dipr_table` | `DIPR_DATA` | Died-process status/pid (`source_code/Include/Global.h:L585-L590`) |

### Shared-memory table engine

The shared-memory “table engine” types are defined in `Global.h` (`TABLE_DATA`, `TABLE_HEADER`, `TABLE`, `EXTENT_HEADER`, `MASTER_SHM_HEADER`, etc.). (`source_code/Include/Global.h:L720-L795`)

The implementation is centralized in GenLib’s shared memory library. (`source_code/GenLib/SharedMemory.cpp:L12-L17`)

## IPC Mechanisms

### Unix-domain named sockets (aka “named pipes”)

The system’s internal IPC uses unix-domain stream sockets (`AF_UNIX`, `SOCK_STREAM`) via:
- `ListenSocketNamed()` which creates, binds, and listens on an `AF_UNIX` socket path. (`source_code/GenLib/Sockets.c:L82-L206`)
- `ConnectSocketNamed()` which creates an `AF_UNIX` socket and connects to the named path. (`source_code/GenLib/Sockets.c:L209-L295`)

Named socket path generation uses `NamedpDir` and `NAMEDP_PREFIX` with `pid` and timestamp. (`source_code/GenLib/Sockets.c:L1376-L1387`, `source_code/Include/Global.h:L151-L152`, `source_code/Include/Global.h:L862-L867`)

### Inter-process message payload modes

Messages can include payload directly or by filename:
- `GetInterProcMesg()` reads payload either from a referenced file (`FILE_MESG`) or directly from the buffer (`DATA_MESG`). (`source_code/GenLib/Sockets.c:L1516-L1614`)

### Semaphores

Semaphore creation is intended to be done by the Father process (`CreateSemaphore()` comment). (`source_code/GenLib/Semaphores.c:L100-L108`)

`CreateSemaphore()` supports multiple semaphore implementations (XENIX/POSIX/BSD); BSD path uses `semget(...)` + `semctl(..., SETVAL, 1)`. (`source_code/GenLib/Semaphores.c:L121-L164`)

## Database Access

### SQLMD abstraction (GenSql.h)

In Informix mode, `SQLMD_connect()` maps to `INF_CONNECT(MacUser, MacPass, MacDb)`. (`source_code/Include/GenSql.h:L194-L196`)

### ODBC connections and database selection (GenSql.c)

`INF_CONNECT()` reads ODBC configuration from environment variables and connects to MS-SQL and/or Informix via `ODBC_CONNECT(...)`. (`source_code/GenSql/GenSql.c:L1087-L1128`)

It explicitly sets `MAIN_DB` based on how env vars are configured (not contingent on connection success), and can choose defaults based on `ODBC_DEFAULT_DATABASE`; it also sets `ALT_DB` and optional mirroring switch. (`source_code/GenSql/GenSql.c:L1132-L1213`)

## Cross-Reference Map (dependencies)

### GenLib (shared runtime library)

**GenLib provides the runtime substrate** used by most processes:
- Shared memory handling. (`source_code/GenLib/SharedMemory.cpp:L12-L17`)
- Unix-domain socket IPC (“named sockets”). (`source_code/GenLib/Sockets.c:L12-L17`)
- Semaphore lifecycle and wait/release. (`source_code/GenLib/Semaphores.c:L12-L16`)
- Process bootstrap/spawn primitives (`InitSonProcess`, `Run_server`). (`source_code/GenLib/Memory.c:L552-L612`, `source_code/GenLib/Memory.c:L1023-L1193`)
- Error logging primitives (`GerrLog*`). (`source_code/GenLib/GeneralError.c:L12-L16`)

### Components that depend on GenLib

- **FatherProcess**: directly uses GenLib IPC/shm primitives (`ListenSocketNamed`, `CreateSemaphore`, `InitFirstExtent`, `CreateTable`). (`source_code/FatherProcess/FatherProcess.c:L299-L334`)
- **Child processes**: use the common bootstrap (`InitSonProcess`) which opens semaphore + attaches shared memory + listens on a per-process unix-domain socket + registers in `PROC_TABLE`. (`source_code/GenLib/Memory.c:L552-L612`)
  Concrete call sites:
  - `SqlServer` (`source_code/SqlServer/SqlServer.c:L211-L211`)
  - `As400UnixClient` (`source_code/As400UnixClient/As400UnixClient.c:L174-L174`)
  - `As400UnixServer` (`source_code/As400UnixServer/As400UnixServer.c:L194-L194`)
  - `As400UnixDocServer` (`source_code/As400UnixDocServer/As400UnixDocServer.c:L88-L88`)
  - `As400UnixDoc2Server` (`source_code/As400UnixDoc2Server/As400UnixDoc2Server.c:L101-L107`)
  - `PharmTcpServer` (`source_code/PharmTcpServer/PharmTcpServer.cpp:L124-L128`)
  - `PharmWebServer` (`source_code/PharmWebServer/PharmWebServer.cpp:L123-L128`)

### GenSql + MacODBC (database access layer)

- `GenSql.h` defines the `SQLMD_*` abstraction macros; in Informix mode, `SQLMD_connect()` maps to `INF_CONNECT(MacUser, MacPass, MacDb)`. (`source_code/Include/GenSql.h:L194-L196`)
- `MacODBC.h` is the ODBC “infrastructure” layer and includes `GenSql.h`. (`source_code/Include/MacODBC.h:L7-L41`)
- `INF_CONNECT()` connects to MS-SQL and/or Informix based on environment variables, and selects `MAIN_DB`/`ALT_DB` pointers. (`source_code/GenSql/GenSql.c:L1087-L1213`)

### Components that depend on GenSql/MacODBC

Evidence of use is via direct include of `<MacODBC.h>` and/or calling `SQLMD_connect()`:
- `FatherProcess` includes `<MacODBC.h>` and calls `SQLMD_connect()` in a retry loop. (`source_code/FatherProcess/FatherProcess.c:L29-L30`, `source_code/FatherProcess/FatherProcess.c:L259-L271`)
- `SqlServer` includes `<MacODBC.h>`. (`source_code/SqlServer/SqlServer.c:L29-L35`)
- `As400UnixServer` includes `<MacODBC.h>` and calls `SQLMD_connect()` (also sets `LOCK_TIMEOUT` and `DEADLOCK_PRIORITY`). (`source_code/As400UnixServer/As400UnixServer.c:L32-L40`, `source_code/As400UnixServer/As400UnixServer.c:L228-L239`)
- `As400UnixClient` includes `<MacODBC.h>`. (`source_code/As400UnixClient/As400UnixClient.c:L29-L34`)
- `As400UnixDocServer` / `As400UnixDoc2Server` include `<MacODBC.h>` and `GenSql.h` and call `SQLMD_connect()`. (`source_code/As400UnixDocServer/As400UnixDocServer.c:L31-L42`, `source_code/As400UnixDocServer/As400UnixDocServer.c:L122-L132`)
- `ShrinkPharm` includes `<MacODBC.h>` and calls `SQLMD_connect()` in a retry loop. (`source_code/ShrinkPharm/ShrinkPharm.c:L16-L16`, `source_code/ShrinkPharm/ShrinkPharm.c:L128-L137`)

### Shared headers and shared data structures

- `Global.h` is the central shared header, defining shared-memory table engine types and row types like `PROC_DATA`, `STAT_DATA`, `DIPR_DATA`, etc. (`source_code/Include/Global.h:L505-L795`)
- `MsgHndlr.h` is a cross-cutting header for transaction/message formats and handlers; it is included by multiple server processes/libraries. (`source_code/Include/MsgHndlr.h:L6-L16`, `source_code/SqlServer/SqlServer.c:L29-L35`, `source_code/As400UnixServer/As400UnixServer.c:L32-L36`)

### Shared-memory table touchpoints (examples)

- `TableTab[]` defines the system’s canonical shared-memory tables. (`source_code/GenSql/GenSql.c:L86-L126`)
- `DIPR_TABLE` and `PROC_TABLE` are explicitly opened in both `PharmTcpServer` and `PharmWebServer` (used for “died process” tracking and process state). (`source_code/PharmTcpServer/PharmTcpServer.cpp:L137-L142`, `source_code/PharmWebServer/PharmWebServer.cpp:L438-L442`)

## External Integrations

### AS/400 bridge

`As400UnixGlob.h` defines shared constants for AS400↔Unix client/server processes, including compile-time conditional port variables and ASCII↔EBCDIC mapping pairs. (`source_code/Include/As400UnixGlob.h:L12-L15`, `source_code/Include/As400UnixGlob.h:L71-L84`, `source_code/Include/As400UnixGlob.h:L94-L106`)

### Tikrot RPC

`TikrotRPC.h` declares `CallTikrotSP()` / `InitTikrotSP()` and defines RPC input/output buffer sizes. (`source_code/Include/TikrotRPC.h:L11-L19`, `source_code/Include/TikrotRPC.h:L39-L49`)

### HTTP gateway (Served library)

`PharmWebServer.cpp` depends on Served via `<served/served.hpp>`. (`source_code/PharmWebServer/PharmWebServer.cpp:L24-L33`)

`served.hpp` is third-party and includes `served/net/server.hpp` and `served/multiplexer.hpp`. (`source_code/Served_includes/served.hpp:L23-L29`)

## Key Findings

- **IPC is Unix-domain socket based** (not POSIX FIFO/mkfifo in the parts verified): `ListenSocketNamed` uses `socket(AF_UNIX, SOCK_STREAM, ...)` and binds a filesystem path. (`source_code/GenLib/Sockets.c:L109-L165`)
- **Shared memory is a first-class system backbone**: `FatherProcess` initializes extents and creates tables from `TableTab[]`. (`source_code/FatherProcess/FatherProcess.c:L306-L334`, `source_code/GenSql/GenSql.c:L86-L126`)
- **Child processes have a standard bootstrap** via `InitSonProcess()`. (`source_code/GenLib/Memory.c:L552-L612`)
- **ODBC supports both MS-SQL and Informix** with runtime selection via env vars. (`source_code/GenSql/GenSql.c:L1087-L1213`)
- **Sensitive secrets exist in headers** (values must be removed/redacted in any outputs): hard-coded credentials appear in `TikrotRPC.h` and `global_1.h`. (`source_code/Include/TikrotRPC.h:L69-L72`, `source_code/Include/global_1.h:L5-L7`)

## Open Questions (needs verification)

- Some process-type constants exist in `Global.h` (e.g., X25 listener/worker types), but their runtime usage in this repo has not been fully traced here. (`source_code/Include/Global.h:L224-L227`)
- The complete runtime process set depends on the DB-driven parameter table scan in `run_system()`; fully enumerating which binaries are started requires profiling the parameters schema/contents. (`source_code/FatherProcess/FatherProcess.c:L1793-L1869`)

