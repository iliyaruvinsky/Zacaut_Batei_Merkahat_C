# GenLib - Program Specification

**Component**: GenLib (libgenlib.a)
**Task ID**: DOC-GENLIB-001
**Generated**: 2026-03-16
**Source**: `source_code/GenLib/`

---

## Overview

According to the file headers, GenLib is the shared foundation library for the MACCABI healthcare C backend. Each source file states that its routines are the prescribed way to handle the corresponding subsystem:

- `Memory.c:12-16` -- "Library routines for handling memory buffers. Memory buffers handling should be only with this library routines."
- `SharedMemory.cpp:12-17` -- "library routines for shared memory handling. Shared memory handling should be done only with these library routines."
- `Sockets.c:12-17` -- "library routines for communication sockets handling. Communication sockets handling should be done only with these library routines."
- `Semaphores.c:12-16` -- "Library routines for handling semaphores. Semaphores handling should be only with this library routines."
- `GeneralError.c:12-16` -- "library routines for error handling. Error handling should be done only with these library routines."

**Build Target**: `libgenlib.a` (static archive library), according to `Makefile:L24`.

**Author**: Ely Levy (Reshuma), as stated across file headers (e.g., `Memory.c:8`, `SharedMemory.cpp:8`)
**Original Date**: 02/05/1996 (SharedMemory.cpp, Semaphores.c, Sockets.c, GeneralError.c) and 19/05/1996 (Memory.c)
**Maintainers**: DonR, GAL, Marianna (based on comments throughout the codebase)

---

## Purpose

Based on the source code analysis, GenLib appears to provide five core subsystems:

1. **Process Lifecycle Management** -- Child process bootstrap (`InitSonProcess`), spawning (`Run_server`), shutdown (`ExitSonProcess`), and broadcast messaging (`BroadcastSonProcs`)
2. **IPC via Unix-Domain Sockets** -- Called "named pipes" in the codebase; uses `AF_UNIX`/`SOCK_STREAM` with framed message protocol; also supports TCP/IP (`AF_INET`)
3. **Shared Memory Table Engine** -- Multi-extent System V shared memory with an in-memory table database featuring doubly-linked-list rows, extent chaining, and free-chain reuse
4. **System V Semaphore Facility** -- Single semaphore protecting all shared memory access, with re-entrant (pushable) locking
5. **Error Handling and Logging** -- Variadic log functions writing to per-process log files with doom-loop protection

Additionally, GenLib provides utility functions for memory allocation, string manipulation, Hebrew text encoding conversions, and date arithmetic.

**Important**: GenLib is a library, not a server process. It has no `main()` function and no dispatch loop. It is linked by every server component in the system.

---

## File Inventory

| File | Lines | Functions | Purpose |
|------|------:|----------:|---------|
| SharedMemory.cpp | 4,774 | 46 | Shared memory multi-extent table engine with doubly-linked-list rows |
| Memory.c | 2,196 | 31 | Process lifecycle, utilities, Hebrew encoding, date arithmetic |
| Sockets.c | 1,759 | 17 | Unix-domain and TCP socket IPC system |
| GeneralError.c | 770 | 13 | Error handling and logging with doom-loop protection |
| Semaphores.c | 734 | 6 | System V semaphore operations with re-entrant locking |
| GxxPersonality.c | 27 | 1 | C++ personality stub and `Prn()` debug helper |
| Makefile | 55 | 0 | Build configuration for `libgenlib.a` |
| **Total** | **10,316** | **114** | |

---

## C/C++ Split

According to code at `SharedMemory.cpp:L27`, the file uses `#pragma GCC diagnostic ignored "-Wwrite-strings"` to suppress C++ string-literal warnings. This appears to be the only C++ file in the library. The `OpenTable` function uses `extern "C"` at `SharedMemory.cpp:L1501` to maintain C linkage for callers. All other source files are plain C.

---

## Public API Listing

### Process Lifecycle (Memory.c)

| Function | Line | Signature | Purpose |
|----------|-----:|-----------|---------|
| `InitSonProcess` | 552 | `int InitSonProcess(char *proc_pathname, int proc_type, int retrys, int eicon_port, short int system)` | 13-step child process bootstrap |
| `ExitSonProcess` | 625 | `int ExitSonProcess(int status)` | Clean shutdown: dispose sockets, detach shm, close semaphore, exit |
| `Run_server` | 1023 | `int Run_server(PROC_DATA *proc_data)` | Fork and exec a child process with arguments from shm params |
| `BroadcastSonProcs` | 664 | `int BroadcastSonProcs(char *buffer, int buf_size, int msg_type, int proc_flg)` | Broadcast message to all child processes via sockets |

### Memory Allocation (Memory.c)

| Function | Line | Purpose |
|----------|-----:|---------|
| `MemAllocExit` | 57 | `malloc()` wrapper; logs and exits on failure |
| `MemReallocExit` | 88 | `realloc()` wrapper; logs on failure |
| `MemAllocReturn` | 121 | `malloc()` wrapper; logs and returns NULL on failure |
| `MemReallocReturn` | 151 | `realloc()` wrapper; logs and returns NULL on failure |
| `MemFree` | 181 | `free()` wrapper with NULL-pointer guard |

### String and Buffer Utilities (Memory.c)

| Function | Line | Purpose |
|----------|-----:|---------|
| `ListMatch` | 200 | Find exact string match in NULL-terminated list |
| `ListNMatch` | 251 | Find prefix match in list using `strncmp` |
| `StringToupper` | 302 | In-place uppercase conversion |
| `BufConvert` | 322 | Byte-reversal of buffer |
| `GetCurrProgName` | 353 | Extract filename from path (strip directory prefix) |

### Environment Configuration (Memory.c)

| Function | Line | Purpose |
|----------|-----:|---------|
| `InitEnv` | 389 | Allocate environment name/value hash buffers |
| `HashEnv` | 441 | Read `EnvTabPar` environment variables and cache them |

### Command-Line and Lookup (Memory.c)

| Function | Line | Purpose |
|----------|-----:|---------|
| `GetCommonPars` | 848 | Parse `-r` (retries) from command-line argv |
| `GetFatherNamedPipe` | 910 | Look up FatherProcess named pipe from PROC_TABLE |
| `TssCanGoDown` | 957 | Check if subsystem can safely shut down |

### System Utilities (Memory.c)

| Function | Line | Purpose |
|----------|-----:|---------|
| `GetSystemLoad` | 1261 | Run `sar` and parse CPU load percentage |
| `my_nap` | 1309 | Sleep for milliseconds using `select()` on empty fd_set |
| `run_cmd` | 1331 | Execute shell command via `popen()` with alarm timeout |
| `GetWord` | 1403 | Extract next whitespace-delimited word |
| `ToGoDown` | 1433 | Check if system/subsystem status is GOING_DOWN |

### Hebrew Encoding (Memory.c)

| Function | Line | Purpose |
|----------|-----:|---------|
| `buf_reverse` | 1471 | Reverse bytes in buffer |
| `fix_char` | 1493 | Swap parentheses for RTL text |
| `EncodeHebrew` | 1526 | Flexible encoding dispatcher (added 26Nov2025 by DonR) |
| `WinHebToDosHeb` | 1630 | Convert Windows-1255 to DOS code page 862 |
| `WinHebToUTF8` | 1735 | Convert Windows-1255 to UTF-8 |

### Date Arithmetic (Memory.c)

| Function | Line | Purpose |
|----------|-----:|---------|
| `GetMonsDif` | 1917 | Months difference between two YYYYMMDD dates |
| `GetDaysDiff` | 1969 | Days difference between two YYYYMMDD dates |
| `AddDays` | 2008 | Add N days to YYYYMMDD date |
| `AddMonths` | 2043 | Add N months to YYYYMMDD date |
| `AddSeconds` | 2104 | Add N seconds to HHMMSS time |
| `ConvertUnitAmount` | 2134 | Convert medication amounts between units |
| `ParseISODateTime` | 2168 | Parse ISO 8601 datetime (added 2025-04-22 by Marianna) |

### Socket IPC (Sockets.c)

| Function | Line | Purpose |
|----------|-----:|---------|
| `ListenSocketNamed` | 89 | Create, bind, and listen on AF_UNIX socket |
| `ConnectSocketNamed` | 217 | Create AF_UNIX socket and connect to named path |
| `ListenSocketUnnamed` | 318 | Create, bind, and listen on AF_INET (TCP) socket |
| `ConnectSocketUnnamed` | 489 | Create AF_INET socket and connect to remote address:port |
| `AcceptConnection` | 583 | Accept incoming connection |
| `CloseSocket` | 634 | Close socket (guards fd < 3) |
| `DisposeSockets` | 656 | Unlink named pipe file for cleanup |
| `ReadSocket` | 686 | Read framed message (header + payload) |
| `WriteSocket` | 820 | Write framed message (header + payload) |
| `WriteSocketHead` | 870 | Write message with status byte prepended |
| `read_socket_data` | 948 | Low-level read with select() timeout |
| `write_socket_data` | 1078 | Low-level write with select() timeout |
| `get_header_details` | 1196 | Read and parse MESSAGE_HEADER (static) |
| `set_header_details` | 1248 | Format and send MESSAGE_HEADER |
| `GetPeerAddr` | 1307 | Get dotted-decimal peer address |
| `GetCurrNamedPipe` | 1376 | Build per-process socket path |
| `GetSocketMessage` | 1396 | Select-based multiplexed message receive |
| `GetInterProcMesg` | 1516 | Decode inter-process message (FILE_MESG/DATA_MESG) |
| `SetInterProcMesg` | 1622 | Encode inter-process message (FILE_MESG/DATA_MESG) |

### Shared Memory (SharedMemory.cpp)

| Function | Line | Purpose |
|----------|-----:|---------|
| `InitFirstExtent` | 288 | Allocate and init first shm extent (Father only) |
| `AddNextExtent` | 518 | Allocate additional shm extent (static, Father only) |
| `KillAllExtents` | 711 | Detach and IPC_RMID all extents (Father, shutdown) |
| `OpenFirstExtent` | 869 | Attach to existing shm extent (child processes) |
| `AttachAllExtents` | 999 | Attach newly-allocated extents (static) |
| `DetachAllExtents` | 1122 | Detach all attached extents |
| `AttachShMem` | 1238 | Low-level shmat wrapper (static) |
| `CreateTable` | 1291 | Create named table in shm (Father only) |
| `OpenTable` | 1501 | Open existing table for access (extern "C") |
| `CloseTable` | 1642 | Close table connection |
| `AddItem` | 1700 | Add row to table |
| `DeleteItem` | 1938 | Delete row from table (move to free chain) |
| `DeleteTable` | 2160 | Delete all rows from table |
| `ListTables` | 2293 | Debug: print all table names and row counts |
| `ParmComp` | 2329 | Row comparator for PARM_TABLE |
| `ProcComp` | 2359 | Row comparator for PROC_TABLE |
| `UpdtComp` | 2387 | Row comparator for UPDT_TABLE |
| `ActItems` | 2417 | Core row iteration engine |
| `SetRecordSize` | 2511 | Resize table record size (2.5x inflation) |
| `RewindTable` | 2609 | Reset table cursor to first row |
| `GetItem` | 2668 | Copy current row data to caller buffer |
| `SeqFindItem` | 2724 | Sequential key-range search |
| `SetItem` | 2774 | Overwrite current row data |
| `GetProc` | 2829 | Find PROC_DATA row by pid |
| `get_sys_status` | 2887 | Read system status from STAT_TABLE |
| `AddCurrProc` | 2921 | Register current process in PROC_TABLE |
| `UpdateTimeStat` | 3030 | Update time-based statistics |
| `IncrementCounters` | 3080 | Increment pharmacy statistics counters |
| `IncrementDoctorCounters` | 3356 | Increment doctor statistics counters |
| `RescaleTimeStat` | 3630 | Rescale (zero stale) time statistics |
| `RescaleCounters` | 3663 | Rescale pharmacy statistics |
| `RescaleDoctorCounters` | 3785 | Rescale doctor statistics |
| `GetMessageIndex` | 3896 | Map message number to array index |
| `IncrementRevision` | 3931 | Increment STAT_DATA.param_rev |
| `UpdateShmParams` | 3969 | Reload parameters from shm if revision changed |
| `GetFreeSqlServer` | 4024 | Find and lock idle SQL server process |
| `LockSqlServer` | 4065 | Atomically lock free SQLPROC_TYPE entry |
| `SetFreeSqlServer` | 4096 | Release locked SQL server process |
| `ReleaseSqlServer` | 4123 | Clear busy_flg for process by pid |
| `ShmGetParamsByName` | 4153 | Load/reload parameters from PARM_TABLE |
| `GetPrescriptionId` | 4266 | Atomically read and increment prescription ID |
| `GetMessageRecId` | 4310 | Atomically read and increment message rec_id |
| `DiprComp` | 4354 | Row comparator for DIPR_TABLE |
| `GetSonsCount` | 4397 | Read sons_count from STAT_TABLE |
| `AddToSonsCount` | 4439 | Atomically add to sons_count |
| `GetTabNumByName` | 4495 | Look up table number in TableTab[] by name |
| `UpdateLastAccessTime` | 4525 | Update process last-access timestamp |
| `UpdateLastAccessTimeInternal` | 4552 | Operation: set timestamp fields for matching pid |
| `UpdateTable` | 4612 | Update UPDT_DATA row by table name |
| `GetTable` | 4664 | Get UPDT_DATA row by table name |
| `set_sys_status` | 4716 | Set system/subsystem status in STAT_TABLE |

### Semaphores (Semaphores.c)

| Function | Line | Purpose |
|----------|-----:|---------|
| `CreateSemaphore` | 109 | Create semaphore (Father only) |
| `DeleteSemaphore` | 181 | Delete semaphore (IPC_RMID) |
| `OpenSemaphore` | 275 | Open existing semaphore (child processes) |
| `CloseSemaphore` | 404 | Close semaphore (mark unused locally) |
| `WaitForResource` | 487 | Acquire semaphore (re-entrant) |
| `ReleaseResource` | 625 | Release semaphore |

### Error Logging (GeneralError.c)

| Function | Line | Purpose |
|----------|-----:|---------|
| `GerrLogExit` | 92 | Log error and exit(status) |
| `GerrLogReturn` | 165 | Log error with 250ms delay and return |
| `GerrLogMini` | 248 | Log one-liner message |
| `GerrLogAddBlankLines` | 288 | Write N blank lines to log |
| `GerrLogFnameMini` | 314 | Log one-liner to specified file |
| `GerrLogFnameClear` | 355 | Clear (truncate) specified log file |
| `GerrLogToFileName` | 382 | Log full-format error to specified file (with delay) |
| `GmsgLogExit` | 445 | Log titled message and exit |
| `GmsgLogReturn` | 521 | Log titled message and return |
| `open_log_file` | 594 | Open {LogDir}/{LogFile} for append; fallback to stderr |
| `close_log_file` | 670 | Flush and close log file |
| `open_other_file` | 695 | Open {LogDir}/{fname}; create directory if needed |
| `create_log_directory` | 746 | Create directory via mkdir -m777 -p |

### Debug Helper (GxxPersonality.c)

| Function | Line | Purpose |
|----------|-----:|---------|
| `Prn` | 12 | Debug print with pid/ppid prefix |

---

## Consumer Map

Based on the dependency graph at `CHUNKS/GenLib/graph.json`, the following components depend on GenLib:

| Consumer Component | Key GenLib Functions Used |
|-------------------|--------------------------|
| **FatherProcess** | `CreateSemaphore`, `InitFirstExtent`, `CreateTable`, `Run_server`, `AddCurrProc`, `BroadcastSonProcs`, `KillAllExtents`, `DeleteSemaphore`, `set_sys_status`, `get_sys_status`, `ListenSocketNamed`, `GetSocketMessage` |
| **SqlServer** | `InitSonProcess`, `ExitSonProcess`, `GetSocketMessage`, `UpdateTimeStat`, `UpdateShmParams`, `UpdateLastAccessTime` |
| **As400UnixServer** | `InitSonProcess`, `GetInterProcMesg`, `SetInterProcMesg`, `GetFreeSqlServer`, `SetFreeSqlServer`, `ConnectSocketNamed` |
| **PharmTcpServer** | `InitSonProcess`, `OpenTable` (DIPR_TABLE, PROC_TABLE), `GetSocketMessage` |
| **PharmWebServer** | `InitSonProcess`, `OpenTable`, `EncodeHebrew`, `WinHebToDosHeb` |
| **As400UnixClient** | `InitSonProcess`, `ExitSonProcess` |
| **As400UnixDocServer** | `InitSonProcess`, `ExitSonProcess` |
| **As400UnixDoc2Server** | `InitSonProcess`, `ExitSonProcess` |

**Note**: FatherProcess does NOT call `InitSonProcess()`. According to code at `FatherProcess.c:L350-L351`, it performs its own initialization and calls `AddCurrProc()` directly.

---

## Dependencies

### Include Headers

According to the source files:

| File | Includes | Lines |
|------|----------|-------|
| Memory.c | `Global.h`, `GenSql.h` (Linux only) | 24-28 |
| SharedMemory.cpp | `Global.h`, `CCGlobal.h`, `MsgHndlr.h` | 37-50 |
| Sockets.c | `Global.h`, `<sys/time.h>` | 24-25 |
| Semaphores.c | `Global.h` | 23 |
| GeneralError.c | `Global.h` | 25-27 |
| GxxPersonality.c | `Global.h` | 2 |

### External Globals (from Global.h)

According to the graph analysis, GenLib references the following globals defined elsewhere:

- `CurrNamedPipe`, `CurrProgName`, `NamedPipeSocket` -- per-process IPC state
- `NamedpDir`, `BinDir`, `LogDir`, `LogFile` -- directory configuration from environment
- `MacUid` -- user ID for `setuid()` call
- `ReadTimeout`, `WriteTimeout` -- socket I/O timeout values
- `SharedSize` -- shared memory extent size
- `LockPagesMode` -- whether to lock pages in memory
- `GoDownCheckInterval`, `SharedMemUpdateInterval` -- throttle intervals

### External Data Structures (from GenSql)

- `TableTab[]` -- table schema definitions (referenced at `SharedMemory.cpp:L96`)
- `EnvTabPar` -- environment variable list (referenced at `Memory.c:L462`)
- `SonProcParams` -- per-component parameter descriptors (referenced at `Memory.c:L575`)
- `PcMessages[]`, `PharmMessages[]`, `DoctorMessages[]` -- message arrays

---

## Documentation Limitations

### What CANNOT be determined from GenLib code alone:
- Exact field sizes of `MESSAGE_HEADER` struct (defined in `Global.h`, not in GenLib)
- Contents of `EnvTabPar` and `SonProcParams` arrays (defined per-component)
- Complete list of table schemas in `TableTab[]` (defined in `GenSql.c`)
- Runtime values of environment variables (`NamedpDir`, `BinDir`, etc.)
- Actual compiler flags from `Defines.mak` and `Rules.mak` (not present in repository)

### What IS known from GenLib code:
- Complete function signatures and behavior
- IPC message protocol (framing, FILE_MESG/DATA_MESG pattern)
- Shared memory extent architecture and table engine
- Semaphore configuration and re-entrant locking mechanism
- Process lifecycle bootstrap sequence (InitSonProcess 13 steps)
- Error logging format and doom-loop protection

---

*Generated by CIDRA Documenter Agent - DOC-GENLIB-001*
