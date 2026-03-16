# GenLib Deep-Dive Research Document

**Research ID**: RES-GENLIB-001
**Date**: 2026-03-16
**Status**: Complete
**Researcher**: Claude (RES agent)

---

## 1. Overview

GenLib is the **shared foundation library** (`libgenlib.a`) linked by every server component in the MACCABI pharmacy/doctor backend system. It provides:

- **Process lifecycle management** -- child process bootstrap (`InitSonProcess`), process spawning (`Run_server`), and shutdown (`ExitSonProcess`)
- **IPC via Unix-domain sockets** -- called "named pipes" in the codebase, using `AF_UNIX`/`SOCK_STREAM`; plus TCP/IP socket support
- **Shared memory management** -- System V shared memory extents with an in-memory table engine (doubly-linked-list rows, extent chaining)
- **System V semaphore operations** -- mutual exclusion for shared memory access
- **Error handling and logging** -- variadic log functions writing to per-process log files
- **Utility functions** -- memory allocation wrappers, string manipulation, Hebrew encoding conversions, date arithmetic

According to the Makefile at `source_code/GenLib/Makefile:L24`, the build target is `libgenlib.a` (a static archive library).

---

## 2. File Inventory (Verified Line Counts)

| File | Lines | Purpose (from file header) |
|---|---:|---|
| `SharedMemory.cpp` | 4,775 | "library routines for shared memory handling" (`SharedMemory.cpp:L12-L17`) |
| `Memory.c` | 2,196 | "Library routines for handling memory buffers" (`Memory.c:L12-L16`) |
| `Sockets.c` | 1,759 | "library routines for communication sockets handling" (`Sockets.c:L12-L17`) |
| `GeneralError.c` | 770 | "library routines for error handling" (`GeneralError.c:L12-L16`) |
| `Semaphores.c` | 734 | "Library routines for handling semaphores" (`Semaphores.c:L12-L16`) |
| `GxxPersonality.c` | 27 | C++ personality stub + `Prn()` debug helper (`GxxPersonality.c:L1-L27`) |
| `Makefile` | 55 | Build configuration for `libgenlib.a` (`Makefile:L1-L55`) |
| **Total** | **10,316** | |

**Note on the C/C++ split**: `SharedMemory.cpp` is the only C++ file. Based on code at `SharedMemory.cpp:L27`, it uses `#pragma GCC diagnostic ignored "-Wwrite-strings"` to suppress C++ string-literal warnings. The likely rationale is that the shared memory table engine uses C++ features (or was compiled as C++ for compatibility reasons), while the rest of the library remains plain C. The file uses `extern "C"` for the `OpenTable` function (`SharedMemory.cpp:L1501`) to maintain C linkage for callers.

---

## 3. Complete Function Inventory

### 3.1 Memory.c -- Process Lifecycle, Utilities, Hebrew Encoding

| Function | Line | Purpose |
|---|---:|---|
| `MemAllocExit()` | 57 | `malloc()` wrapper; calls `GerrLogExit` on failure |
| `MemReallocExit()` | 88 | `realloc()` wrapper; calls `GerrLogReturn` on failure |
| `MemAllocReturn()` | 121 | `malloc()` wrapper; logs and returns NULL on failure |
| `MemReallocReturn()` | 151 | `realloc()` wrapper; logs and returns NULL on failure |
| `MemFree()` | 181 | `free()` wrapper with NULL-pointer guard |
| `ListMatch()` | 200 | Find exact string match in NULL-terminated string list |
| `ListNMatch()` | 251 | Find prefix match in NULL-terminated string list |
| `StringToupper()` | 302 | In-place uppercase conversion |
| `BufConvert()` | 322 | Byte-reversal of a buffer |
| `GetCurrProgName()` | 353 | Extract filename from a path (strips directory) |
| `InitEnv()` | 389 | Initialize hash-based environment variable buffers |
| `HashEnv()` | 441 | Read environment variables via `EnvTabPar` and cache them |
| **`InitSonProcess()`** | **552** | **Main child-process bootstrap routine** |
| `ExitSonProcess()` | 625 | Clean shutdown: dispose sockets, detach shm, close semaphore, `exit()` |
| `BroadcastSonProcs()` | 664 | Broadcast a message to all child processes via socket IPC |
| `alarm_handler()` | 830 | No-op SIGALRM handler (used to break out of blocking syscalls) |
| `GetCommonPars()` | 848 | Parse `-r` (retries) from command-line arguments |
| `GetFatherNamedPipe()` | 910 | Look up FatherProcess named pipe path from `PROC_TABLE` |
| `TssCanGoDown()` | 957 | Check if a subsystem can safely shut down (no active workers) |
| **`Run_server()`** | **1023** | **Fork+exec a child process** with arguments from shm params |
| `GetSystemLoad()` | 1261 | Run `sar` and parse CPU load percentage |
| `my_nap()` | 1309 | Sleep for milliseconds using `select()` on empty fd_set |
| `run_cmd()` | 1331 | Execute shell command via `popen()`, read output with alarm timeout |
| `GetWord()` | 1403 | Extract next whitespace-delimited word from string |
| `ToGoDown()` | 1433 | Check if system/subsystem status is `GOING_DOWN` |
| `buf_reverse()` | 1471 | Reverse bytes in a buffer (used by Hebrew text processing) |
| `fix_char()` | 1493 | Swap parentheses `(` <-> `)` for RTL text |
| `EncodeHebrew()` | 1526 | Flexible Hebrew encoding converter (Win-1255 to DOS or UTF-8). Added 26Nov2025. |
| `WinHebToDosHeb()` | 1630 | Convert Windows-1255 Hebrew text to DOS Hebrew (code page 862) |
| `WinHebToUTF8()` | 1735 | Convert Windows-1255 Hebrew text to UTF-8 |
| `GetMonsDif()` | 1917 | Calculate difference in completed months between two YYYYMMDD dates |
| `GetDaysDiff()` | 1969 | Calculate difference in days between two YYYYMMDD dates |
| `AddDays()` | 2008 | Add N days to a YYYYMMDD date |
| `AddMonths()` | 2043 | Add N months to a YYYYMMDD date |
| `AddSeconds()` | 2104 | Add N seconds to a time value |
| `ConvertUnitAmount()` | 2134 | Convert medication amounts between units (mcg/MIU/g to mg) |
| `ParseISODateTime()` | 2168 | Parse ISO 8601 datetime to YYYYMMDD + HHMMSS integers. Added 2025-04-22 by Marianna. |

### 3.2 Sockets.c -- IPC Socket System

| Function | Line | Purpose |
|---|---:|---|
| **`ListenSocketNamed()`** | **89** | Create, bind, and listen on `AF_UNIX`/`SOCK_STREAM` socket |
| **`ConnectSocketNamed()`** | **217** | Create `AF_UNIX` socket and connect to named path |
| `ListenSocketUnnamed()` | 318 | Create, bind, and listen on `AF_INET`/`SOCK_STREAM` (TCP) socket |
| `ConnectSocketUnnamed()` | 489 | Create `AF_INET` socket and connect to remote address:port |
| `AcceptConnection()` | 583 | Accept incoming connection on a listen socket |
| `CloseSocket()` | 634 | Close a socket (guards against closing fd < 3) |
| `DisposeSockets()` | 656 | `unlink()` the named pipe file and clean up |
| **`ReadSocket()`** | **686** | Read a framed message: header then payload, with overflow handling |
| **`WriteSocket()`** | **820** | Write a framed message: header then payload |
| `WriteSocketHead()` | 870 | Write message with a status byte prepended to payload |
| `read_socket_data()` | 948 | Low-level read loop with `select()` timeout (`ReadTimeout`) |
| `write_socket_data()` | 1078 | Low-level write loop with `select()` timeout (`WriteTimeout`) |
| `get_header_details()` | 1196 | Read `MESSAGE_HEADER` struct and parse type/length (static) |
| `set_header_details()` | 1248 | Format and send `MESSAGE_HEADER` struct |
| `GetPeerAddr()` | 1307 | Get dotted-decimal peer address of a connected socket |
| **`GetCurrNamedPipe()`** | **1376** | Build per-process named pipe path: `{NamedpDir}/{NAMEDP_PREFIX}{pid}_{time}` |
| **`GetSocketMessage()`** | **1396** | `select()`-based multiplexed message receive on `NamedPipeSocket` |
| **`GetInterProcMesg()`** | **1516** | Decode inter-process message: `FILE_MESG` reads from file, `DATA_MESG` reads from buffer |
| **`SetInterProcMesg()`** | **1622** | Encode inter-process message: `FILE_MESG` writes data to temp file, `DATA_MESG` copies inline |

### 3.3 SharedMemory.cpp -- Shared Memory Table Engine

| Function | Line | Purpose |
|---|---:|---|
| **`InitFirstExtent()`** | **288** | Allocate and init first shm extent (Father only). Uses `shmget`/`shmat`. |
| `AddNextExtent()` | 518 | Allocate additional shm extent and chain it (static, Father only) |
| `KillAllExtents()` | 711 | Detach and `IPC_RMID` all extents (Father only, shutdown) |
| **`OpenFirstExtent()`** | **869** | Attach to existing first shm extent (child processes) |
| `AttachAllExtents()` | 999 | Attach all newly-allocated extents not yet attached locally (static) |
| `DetachAllExtents()` | 1122 | Detach all attached extents |
| `AttachShMem()` | 1238 | Low-level `shmat()` wrapper (static) |
| **`CreateTable()`** | **1291** | Create a new named table in shm (Father only) |
| **`OpenTable()`** | **1501** | Open existing table for access (extern "C") |
| `CloseTable()` | 1642 | Close table connection |
| **`AddItem()`** | **1700** | Add a row to a table (allocates from extent or reuses free rows) |
| `DeleteItem()` | 1938 | Delete a row from a table (moves to free chain) |
| `DeleteTable()` | 2160 | Delete all rows from a table (bulk move to free chain) |
| `ListTables()` | 2293 | Debug: print all table names and row counts |
| `ParmComp()` | 2329 | Row comparator for `PARM_TABLE` (by `par_key`) |
| `ProcComp()` | 2359 | Row comparator for `PROC_TABLE` (by `pid`) |
| `UpdtComp()` | 2387 | Row comparator for `UPDT_TABLE` (by `table_name`) |
| **`ActItems()`** | **2417** | Iterate table rows, calling an operation function per row |
| `SetRecordSize()` | 2511 | Resize table record size (inflates by 2.5x) |
| `RewindTable()` | 2609 | Reset table cursor to first row |
| `GetItem()` | 2668 | Operation: copy current row data to caller's buffer |
| `SeqFindItem()` | 2724 | Operation: sequential key-range search |
| `SetItem()` | 2774 | Operation: overwrite current row data from caller's buffer |
| `GetProc()` | 2829 | Operation: find `PROC_DATA` row by pid |
| `get_sys_status()` | 2887 | Read system status from `STAT_TABLE` |
| **`AddCurrProc()`** | **2921** | Register current process in `PROC_TABLE` |
| `UpdateTimeStat()` | 3030 | Update time-based statistics (pharmacy or doctor) |
| `IncrementCounters()` | 3080 | Operation: increment pharmacy statistics counters |
| `IncrementDoctorCounters()` | 3356 | Operation: increment doctor statistics counters |
| `RescaleTimeStat()` | 3630 | Rescale time statistics (zero stale entries) |
| `RescaleCounters()` | 3663 | Operation: rescale pharmacy statistics |
| `RescaleDoctorCounters()` | 3785 | Operation: rescale doctor statistics |
| `GetMessageIndex()` | 3896 | Map message number to array index |
| `IncrementRevision()` | 3931 | Operation: increment `STAT_DATA.param_rev` |
| `UpdateShmParams()` | 3969 | Reload parameters from shm if revision changed |
| `GetFreeSqlServer()` | 4024 | Find and lock an idle SQL server process |
| `LockSqlServer()` | 4065 | Operation: atomically lock a free `SQLPROC_TYPE` entry |
| `SetFreeSqlServer()` | 4096 | Release a locked SQL server process |
| `ReleaseSqlServer()` | 4123 | Operation: clear `busy_flg` for a process by pid |
| `ShmGetParamsByName()` | 4153 | Load/reload parameters from `PARM_TABLE` into a `ParamTab` descriptor array |
| `GetPrescriptionId()` | 4266 | Atomically read and increment prescription ID from shm |
| `GetMessageRecId()` | 4310 | Atomically read and increment message rec_id from shm |
| `DiprComp()` | 4354 | Row comparator for `DIPR_TABLE` (by pid, then status) |
| `GetSonsCount()` | 4397 | Read `sons_count` from `STAT_TABLE` |
| `AddToSonsCount()` | 4439 | Atomically add to `sons_count` in `STAT_TABLE` |
| `GetTabNumByName()` | 4495 | Look up table number in `TableTab[]` by name |
| `UpdateLastAccessTime()` | 4525 | Update current process's last-access timestamp in `PROC_TABLE` |
| `UpdateLastAccessTimeInternal()` | 4552 | Operation: set `l_year..l_sec` fields for matching pid |
| `UpdateTable()` | 4612 | Operation: update `UPDT_DATA` row by table name match |
| `GetTable()` | 4664 | Operation: get `UPDT_DATA` row by table name match |
| `set_sys_status()` | 4716 | Set system/subsystem status in `STAT_TABLE` |

### 3.4 Semaphores.c -- System V Semaphore Operations

| Function | Line | Purpose |
|---|---:|---|
| **`CreateSemaphore()`** | **109** | Create semaphore (Father only). BSD: `semget()` + `semctl(SETVAL, 1)` |
| `DeleteSemaphore()` | 181 | Delete semaphore. BSD: `semctl(IPC_RMID)` |
| **`OpenSemaphore()`** | **275** | Open existing semaphore (child processes). BSD: `semget(IPC_CREAT)` |
| `CloseSemaphore()` | 404 | Close semaphore (mark as unused locally) |
| **`WaitForResource()`** | **487** | Acquire semaphore (lock). BSD: `semop(W={0,-1,SEM_UNDO})`. Supports re-entrant ("pushable") locking via `sem_busy_flg` counter. |
| **`ReleaseResource()`** | **625** | Release semaphore (unlock). BSD: `semop(P={0,+1,SEM_UNDO})`. Decrements `sem_busy_flg`; only does actual semop when count reaches 0. |

### 3.5 GeneralError.c -- Error Handling and Logging

| Function | Line | Purpose |
|---|---:|---|
| **`GerrLogExit()`** | **92** | Log error message to log file, then `exit(status)` |
| **`GerrLogReturn()`** | **165** | Log error message to log file, then return (with 250ms delay to prevent doom loops) |
| **`GerrLogMini()`** | **248** | Log minimal-format message (one-liner) to log file |
| `GerrLogAddBlankLines()` | 288 | Write N blank lines to log file |
| `GerrLogFnameMini()` | 314 | Log minimal-format message to a specified file |
| `GerrLogFnameClear()` | 355 | Clear (truncate) a specified log file |
| `GerrLogToFileName()` | 382 | Log full-format error to a specified file (with 250ms delay) |
| `GmsgLogExit()` | 445 | Log titled message to log file, then `exit(status)` |
| `GmsgLogReturn()` | 521 | Log titled message to log file, then return |
| `open_log_file()` | 594 | Open `{LogDir}/{LogFile}` for append; falls back to `stderr` |
| `close_log_file()` | 670 | Flush and close log file (skips close if `stderr`) |
| `open_other_file()` | 695 | Open `{LogDir}/{fname}` with specified mode; creates directory if needed |
| `create_log_directory()` | 746 | Create directory path for log file via `mkdir -m777 -p` |

### 3.6 GxxPersonality.c

| Function | Line | Purpose |
|---|---:|---|
| `Prn()` | 12 | Debug print helper: formats and prints with pid/ppid prefix |

The `__gxx_personality_v0()` stub is `#if 0`'d out -- it was apparently needed at some point for C++ exception handling but is now disabled. (`GxxPersonality.c:L5-L10`)

---

## 4. InitSonProcess() Step-by-Step Sequence

According to code at `Memory.c:L552-L612`, `InitSonProcess()` performs the following steps in order:

```
InitSonProcess (proc_pathname, proc_type, retrys, eicon_port, system)
```

| Step | Code | Line | Description |
|---|---|---:|---|
| 1 | `ABORT_ON_ERR(InitEnv(GerrId))` | 560 | Allocate environment name/value hash buffers |
| 2 | `ABORT_ON_ERR(HashEnv(GerrId))` | 563 | Read all `EnvTabPar` environment variables and cache them |
| 3 | `RETURN_ON_ERR(OpenSemaphore())` | 566 | Open the system semaphore created by FatherProcess |
| 4 | `RETURN_ON_ERR(OpenFirstExtent())` | 569 | Attach to the first shared memory extent |
| 5 | `GetCurrProgName(proc_pathname, CurrProgName)` | 572 | Extract program filename from path |
| 6 | `RETURN_ON_ERR(ShmGetParamsByName(SonProcParams, PAR_LOAD))` | 575 | Load runtime parameters from shared memory `PARM_TABLE` |
| 7 | `plock(PROCLOCK)` or `mlockall(MCL_CURRENT)` | 578-589 | Optionally lock pages in primary memory (if `LockPagesMode == 1`) |
| 8 | `setuid(atoi(MacUid))` | 592 | Drop to unprivileged user ID |
| 9 | `GetCurrNamedPipe(CurrNamedPipe)` | 598 | Build per-process socket path: `{NamedpDir}/{NAMEDP_PREFIX}{pid}_{time}` |
| 10 | `RETURN_ON_ERR(ListenSocketNamed(CurrNamedPipe))` | 601 | Create and listen on AF_UNIX socket at that path |
| 11 | `RETURN_ON_ERR(AddCurrProc(retrys, proc_type, eicon_port, system))` | 604 | Register process in shared memory `PROC_TABLE` |
| 12 | `RETURN_ON_ERR(AddToSonsCount(1))` | 608 | Increment `sons_count` in `STAT_TABLE` |
| 13 | `return (MAC_OK)` | 611 | Return success |

### Calling Components

According to code across the repository, `InitSonProcess()` is called by:

| Component | Call Site | Arguments |
|---|---|---|
| SqlServer | `SqlServer.c:L211` | `SQLPROC_TYPE`, `PHARM_SYS` |
| As400UnixServer | `As400UnixServer.c:L194` | `AS400TOUNIX_TYPE`, `PHARM_SYS` |
| As400UnixClient | `As400UnixClient.c:L174` | `UNIXTOAS400_TYPE`, `PHARM_SYS` |
| As400UnixDocServer | `As400UnixDocServer.c:L88` | `DOCAS400TOUNIX_TYPE`, `DOCTOR_SYS` |
| As400UnixDoc2Server | `As400UnixDoc2Server.c:L101-L107` | `DOC2AS400TOUNIX_TYPE`, `DOCTOR_SYS` |
| PharmTcpServer | `PharmTcpServer.cpp:L124-L128` | `PHARM_TCP_SERVER_TYPE`, variable `SubSystem` |
| PharmWebServer | `PharmWebServer.cpp:L123-L128` | `WEBSERVERPROC_TYPE`, variable `SubSystem` |

**Note**: FatherProcess does NOT call `InitSonProcess()`; it performs its own initialization sequence (create semaphore, init first extent, create tables) and then calls `AddCurrProc()` directly at `FatherProcess.c:L351`.

---

## 5. IPC Socket API Documentation

### 5.1 Architecture

The IPC system uses **Unix-domain stream sockets** (`AF_UNIX`, `SOCK_STREAM`) for inter-process communication. Each process creates a named socket file at a unique path. The system also supports TCP/IP sockets (`AF_INET`) for external clients.

### 5.2 Named Socket Path Generation

According to code at `Sockets.c:L1376-L1387`:
```c
sprintf(buffer, "%s/%s%d_%ld", NamedpDir, NAMEDP_PREFIX, getpid(), (long)time(NULL));
```
Where `NAMEDP_PREFIX` is defined in `Global.h:L151-L152` and `NamedpDir` is an environment variable.

### 5.3 Message Framing Protocol

All socket messages use a `MESSAGE_HEADER` struct (defined in `Global.h`) containing ASCII-formatted `messageType` and `messageLen` fields. According to code at `Sockets.c:L1196-L1239`:
- `get_header_details()` reads the fixed-size header, then `atoi()` parses type and length
- `set_header_details()` formats type and length with `sprintf()` and writes the header

### 5.4 Private MESG_HEADER struct

In addition to `MESSAGE_HEADER` (the socket framing header), there is a private `MESG_HEADER` struct defined at `Sockets.c:L38-L43` used by the inter-process message system:
```c
typedef struct { int length; char mesg_type; } MESG_HEADER;
```
This header prefixes the payload in `GetInterProcMesg()`/`SetInterProcMesg()`.

### 5.5 Key Functions

**`ListenSocketNamed(socket_name)`** (`Sockets.c:L89-L207`):
1. `socket(AF_UNIX, SOCK_STREAM, 0)` -- create socket
2. `bind()` to `socket_name` path
3. `listen(socket_num, SOMAXCONN)`
4. Stores socket in global `NamedPipeSocket`
5. Returns socket number

**`GetSocketMessage(micro_sec, buffer, max_len, size, close_flg)`** (`Sockets.c:L1396-L1510`):
1. `select()` on `NamedPipeSocket` with configurable timeout
2. On connection: `AcceptConnection()`, then `ReadSocket()` to get framed message
3. If `close_flg == LEAVE_OPEN_SOCK`, returns the accepted socket; otherwise closes it

**`GetInterProcMesg(in_buf, length, out_buf)`** (`Sockets.c:L1516-L1615`):
- Parses `MESG_HEADER` (length + mesg_type) from `in_buf`
- `FILE_MESG` (file-based): opens file named in payload, reads content
- `DATA_MESG` (inline): copies data directly from buffer

**`SetInterProcMesg(in_buf, length, mesg_type, out_buf, out_len)`** (`Sockets.c:L1622-L1756`):
- `FILE_MESG`: writes data to a temp file in `NamedpDir` (named with pid + timestamp), puts filename in output buffer
- `DATA_MESG`: copies data inline into output buffer

### 5.6 Select-Based I/O

Both `read_socket_data()` and `write_socket_data()` use `select()` for timeout-controlled I/O:
- Read timeout: `ReadTimeout` global variable (`Sockets.c:L974`)
- Write timeout: `WriteTimeout` global variable (`Sockets.c:L1102`)
- A comment at `Sockets.c:L1438-L1440` notes that `poll()` is preferred over `select()` but migration is not urgent.

---

## 6. Shared Memory Management Documentation

### 6.1 Architecture

The shared memory subsystem implements a **multi-extent, table-based in-memory database**:

- **Extents**: System V shared memory segments (`shmget`/`shmat`), chained via `EXTENT_HEADER` linked list
- **Tables**: Named collections of doubly-linked-list rows within extents
- **Rows**: Each row has a `TABLE_ROW` header (prev/next pointers, data pointer) plus variable-length data
- **Free list**: Deleted rows are moved to a per-table free chain for reuse
- **Connection table**: `ConnTab[CONN_MAX=150]` tracks open table connections per process (`SharedMemory.cpp:L88-L94`)

### 6.2 Key Data Structures (from `Global.h`)

According to `Global.h:L720-L795`:
- `MASTER_SHM_HEADER`: top-level structure at start of first extent (extent_count, first/last table, first/free extent pointers)
- `EXTENT_HEADER`: per-extent metadata (free_space, extent_size, free_addr_off, next/back extent pointers, next_extent_id for shm attachment)
- `TABLE_HEADER`: per-table metadata (TABLE_DATA schema, record_count, free_count, first/last row, first/last free, next/back table)
- `TABLE_ROW`: per-row wrapper (next/back row pointers, row_data extent pointer)
- `TABLE`: connection handle (conn_id, busy_flg, table_header pointer, curr_row cursor)
- `EXTENT_PTR`: {ext_no, offset} pair used as shared-memory-safe pointers

### 6.3 Extent Pointer Resolution

According to code at `SharedMemory.cpp:L134-L142`, the `GET_ABS_ADDR(extent_ptr)` macro resolves an `EXTENT_PTR` to an absolute address:
```c
ExtentChain[extent_ptr.ext_no].start_addr + extent_ptr.offset
```
A null extent pointer is represented by `ext_no == -1` (`SharedMemory.cpp:L121`).

### 6.4 Father Process Initialization

**`InitFirstExtent(shm_size)`** (`SharedMemory.cpp:L288-L508`):
1. Acquires semaphore (`WaitForResource`)
2. Checks for and removes any pre-existing shm segment with key `ShmKey1 = 0x000abcde` (`SharedMemory.cpp:L80-L81`)
3. `shmget(ShmKey1, shm_size, 0777 | IPC_CREAT)` to create new segment
4. `shmat()` to attach
5. Initializes `MASTER_SHM_HEADER` at start of segment
6. Initializes `EXTENT_HEADER` with free space = `shm_size - sizeof(EXTENT_HEADER) - sizeof(MASTER_SHM_HEADER)`
7. Allocates local `ExtentChain` array (40 slots initially)
8. Initializes `ConnTab[150]` connection table
9. Releases semaphore

### 6.5 Child Process Attachment

**`OpenFirstExtent()`** (`SharedMemory.cpp:L869-L990`):
1. Acquires semaphore
2. `shmget(ShmKey1, 0, 0)` to get existing shm id
3. `shmat()` to attach
4. Initializes local `ExtentChain` and `ConnTab`
5. Releases semaphore

### 6.6 Table Operations

**`CreateTable(table_name, tablep)`** (`SharedMemory.cpp:L1291-L1491`):
- Looks up table schema in `TableTab[]` by name
- Checks for duplicate table names
- Allocates `TABLE_HEADER` in a free extent
- Links into master table chain (doubly-linked list)
- Calls `OpenTable()` to return a connection handle

**`AddItem(tablep, row_data)`** (`SharedMemory.cpp:L1700-L1928`):
- Either reuses a row from the free chain, or allocates new space from extent
- Record size is word-aligned (`sizeof(int)` boundary, `SharedMemory.cpp:L1770-L1776`)
- Appends to end of row chain
- Copies user data via `memcpy`

**`ActItems(tablep, rewind_flg, func, args)`** (`SharedMemory.cpp:L2417-L2502`):
- Core iteration engine: walks row chain calling `func(tablep, args)`
- Operation return codes: `OPER_FORWARD` (next row), `OPER_STOP` (break), `OPER_BACKWARD` (prev row)
- Returns `NO_MORE_ROWS` when chain exhausted

### 6.7 Concurrency Model

All shared memory operations acquire the semaphore via `WaitForResource()` at entry and release via `ReleaseResource()` at exit (using the `RETURN(a)` macro defined at `SharedMemory.cpp:L104` that calls `ReleaseResource()` before returning). The semaphore supports re-entrant acquisition via `sem_busy_flg` counter (`Semaphores.c:L508-L513`, `Semaphores.c:L640-L650`).

---

## 7. Semaphore Operations Documentation

### 7.1 Configuration

The semaphore facility is compile-time selected. According to `Semaphores.c:L48`:
```c
#define SEMAPHORES BSD_SEM
```
The active implementation uses **System V (BSD-style) semaphores**. Three implementations are present in the source (XENIX, POSIX, BSD) but only BSD is active.

### 7.2 Key and Identity

- Semaphore key: `SemKey1 = 0x000abcde` (`Semaphores.c:L92`)
- Semaphore name (for path construction): `"mac_sem1"` (`Semaphores.c:L63`)
- Path used in `sprintf`: `{NamedpDir}/mac_sem1` (`Semaphores.c:L116`)

### 7.3 Semaphore Operations (BSD_SEM path)

| Operation struct | Value | Meaning |
|---|---|---|
| `W` | `{0, -1, SEM_UNDO}` | Wait (decrement/acquire) |
| `P` | `{0, +1, SEM_UNDO}` | Post (increment/release) |
| `WNB` | `{0, -1, SEM_UNDO\|IPC_NOWAIT}` | Non-blocking wait |
| `PNB` | `{0, +1, SEM_UNDO\|IPC_NOWAIT}` | Non-blocking post |

Defined at `Semaphores.c:L86-L90`.

`SEM_UNDO` ensures automatic release if a process crashes, which is critical for system stability.

### 7.4 Re-Entrant (Pushable) Locking

According to code at `Semaphores.c:L508-L513`:
```c
if( sem_busy_flg )
{
    sem_busy_flg++;
    return( MAC_OK );
}
```
If the semaphore is already held by the current process, `WaitForResource()` simply increments a counter instead of making a blocking `semop()` call. `ReleaseResource()` decrements the counter and only performs the actual `semop()` when the counter reaches zero (`Semaphores.c:L640-L650`). This prevents deadlocks when GenLib functions call other GenLib functions that also need the semaphore.

### 7.5 What the Semaphore Protects

The single system semaphore protects **all shared memory access**. Every function in `SharedMemory.cpp` that reads or modifies shared memory calls `WaitForResource()` before accessing and `ReleaseResource()` afterward. This includes:
- Table creation, opening, closing
- Row add, delete, iterate
- Status reads/writes
- Parameter revision checks
- Process registration
- Statistics updates

---

## 8. Error Handling Documentation

### 8.1 Logging Destinations

According to `GeneralError.c:L594-L661`:
- Primary: `{LogDir}/{LogFile}` -- per-process log file, opened for append
- Fallback: `stderr` (if `LogDir` or `LogFile` are empty)
- Alternate files: `{LogDir}/{FileName}` via `open_other_file()`

### 8.2 Log Message Formats

**Standard format** (`ErrorFmt`, `GeneralError.c:L48-L54`):
```
MESSAGE:
   PID {pid}    User: {user}    Working dir: {cwd}
   From {source} Line {line} at {timestamp}
   Message: {message}
---------------------------------------------------------------
```

**Titled format** (`TitleFmt`, `GeneralError.c:L55-L66`):
```
*******************<<< START OF MESSAGE >>>********************
        TITLE : {title}
        ...
********************<<< END OF MESSAGE >>>*********************
```

**Mini format** (`GerrLogMini`, `GeneralError.c:L271`):
```
{message} - {month day} {db_timestamp}
```

### 8.3 Severity Levels (Implicit)

The system does not use explicit severity constants. Instead, severity is encoded in the function choice:

| Function | Behavior | Implicit Severity |
|---|---|---|
| `GerrLogExit()` | Log + `exit(status)` | Fatal |
| `GmsgLogExit()` | Log (titled) + `exit(status)` | Fatal |
| `GerrLogReturn()` | Log + 250ms sleep + return | Error |
| `GerrLogToFileName()` | Log to file + 250ms sleep + return | Error |
| `GmsgLogReturn()` | Log (titled) + return | Error |
| `GerrLogMini()` | Log (one-liner) + return | Info/Warning |
| `GerrLogFnameMini()` | Log to file (one-liner) + return | Info/Warning |

### 8.4 Doom-Loop Protection

According to code at `GeneralError.c:L226-L233`, `GerrLogReturn()` includes a 250ms `usleep()` delay before returning. This prevents rapid-fire error logging from filling the disk. The same protection is applied in `GerrLogToFileName()` at `GeneralError.c:L429-L430`.

### 8.5 Error Macro System

Error macros are defined in `Global.h` (based on the `GerrId` macro pattern). Key patterns observed across the codebase:
- `GerrId` expands to `GerrSource, __LINE__` providing automatic file:line
- `GerrStr` expands to `errno, strerror(errno)` for system error formatting
- `RETURN_ON_ERR(expr)` evaluates `expr` and returns if error
- `ABORT_ON_ERR(expr)` evaluates `expr` and aborts process if error
- `CONT_ON_ERR(expr)` evaluates `expr` and continues loop on error
- `ERR_STATE(state)` tests whether a state value represents an error

---

## 9. Build System Documentation

### 9.1 Makefile Analysis

According to `Makefile:L1-L55`:

**Target**: `libgenlib.a` (static archive library)

**Source files**:
```
GeneralError.c  Memory.c  Semaphores.c  Sockets.c  SharedMemory.cpp  GxxPersonality.c
```

**Build process**:
1. Each `.c`/`.cpp` file is compiled to `.o` with `$(CC) $(COMP_OPTS)`
2. All `.o` files are archived into `libgenlib.a` via `$(AR) r`

**External includes**: The Makefile references `../Util/Defines.mak` (`Makefile:L4`) and `../Util/Rules.mak` (`Makefile:L27`) for compiler flags and rules, but these files are not present in the repository. [NEEDS_VERIFICATION: actual `COMP_OPTS` and `CC` values]

**Clean target**: `rm $(TARGET) $(OBJS)` (`Makefile:L52-L53`)

---

## 10. Cross-References to Consuming Components

### 10.1 Complete Bootstrap Sequence

Based on analysis of FatherProcess (`FatherProcess.c`) and GenLib code:

1. **FatherProcess** starts, installs SIGTERM handler (`FatherProcess.c:L239-L253`)
2. Connects to database via `SQLMD_connect()` (`FatherProcess.c:L259-L271`)
3. Builds named pipe path, calls `ListenSocketNamed()` (`FatherProcess.c:L299-L301`)
4. Calls `CreateSemaphore()` (`FatherProcess.c:L303-L305`)
5. Calls `InitFirstExtent(SharedSize)` (`FatherProcess.c:L306-L308`)
6. Iterates `TableTab[]` calling `CreateTable()` for each (`FatherProcess.c:L309-L334`)
7. Calls `AddCurrProc(0, FATHERPROC_TYPE, 0, PHARM_SYS | DOCTOR_SYS)` (`FatherProcess.c:L350-L351`)
8. Spawns child processes via `Run_server()` (`Memory.c:L1023-L1252`)

Each child process then:
1. Calls `InitSonProcess()` (see Section 4 above)
2. Enters its main processing loop

### 10.2 GenLib Functions Called by Components

| GenLib Function | Called By |
|---|---|
| `InitSonProcess()` | SqlServer, As400UnixServer, As400UnixClient, As400UnixDocServer, As400UnixDoc2Server, PharmTcpServer, PharmWebServer |
| `CreateSemaphore()` | FatherProcess |
| `InitFirstExtent()` | FatherProcess |
| `CreateTable()` | FatherProcess |
| `OpenTable()` / `CloseTable()` | All processes (via `ActItems` pattern) |
| `ListenSocketNamed()` | All processes (via `InitSonProcess` or directly by FatherProcess) |
| `ConnectSocketNamed()` | FatherProcess (in `BroadcastSonProcs`), and inter-process communication |
| `GetSocketMessage()` | All server processes in their main loops |
| `WriteSocket()` / `ReadSocket()` | All processes performing IPC |
| `GetFreeSqlServer()` / `SetFreeSqlServer()` | Processes that delegate SQL work (e.g., As400UnixServer) |
| `Run_server()` | FatherProcess (to spawn child processes) |
| `AddCurrProc()` | FatherProcess (directly), all children (via `InitSonProcess`) |
| `AddToSonsCount()` | FatherProcess (decrement on child death at `FatherProcess.c:L787`), all children (increment via `InitSonProcess`) |
| `BroadcastSonProcs()` | FatherProcess (for shutdown/restart messages) |
| `ExitSonProcess()` | All child processes on shutdown |
| `GerrLogMini()` / `GerrLogReturn()` | All processes for logging |
| `OpenTable(DIPR_TABLE/PROC_TABLE)` | PharmTcpServer (`PharmTcpServer.cpp:L137-L142`), PharmWebServer (`PharmWebServer.cpp:L438-L442`) |
| `WinHebToDosHeb()` / `EncodeHebrew()` | Processes handling Hebrew text in transactions |
| `UpdateTimeStat()` | SqlServer and similar processes after processing messages |
| `UpdateShmParams()` | All child processes (periodic parameter refresh) |
| `UpdateLastAccessTime()` | All child processes (heartbeat) |
| `ToGoDown()` | All child processes (shutdown check) |

---

## 11. Security Notes

The following locations contain security-sensitive values. **Values are intentionally not reproduced here.**

| Location | Description |
|---|---|
| `SharedMemory.cpp:L80-L81` | Hard-coded shared memory key `ShmKey1` |
| `Semaphores.c:L92` | Hard-coded semaphore key `SemKey1` (same value as `ShmKey1`) |
| `Semaphores.c:L59`, `SharedMemory.cpp:L76-L77` | File mode `0777` for semaphore and shared memory |
| `Memory.c:L592-L595` | `setuid()` call using `MacUid` from environment |
| `TikrotRPC.h:L69-L72` | Hard-coded RPC credentials (in header, not GenLib, but referenced by GenLib callers) |
| `global_1.h:L5-L7` | Hard-coded connect-password macros |

**Notable**: Both the shared memory key and semaphore key appear to use the same value `0x000abcde`. The file permissions `0777` allow any user on the system to access the semaphore and shared memory. [NEEDS_VERIFICATION: whether this is intentional for the deployment environment or a security concern]

---

## 12. Verification Backlog

Items that could not be fully verified from the available source code:

| ID | Item | Notes |
|---|---|---|
| VB-01 | `Defines.mak` and `Rules.mak` contents | Referenced by `Makefile:L4,L27` but not present in repository. Needed to verify exact compiler flags. |
| VB-02 | `DelCurrProc` function | No `DelCurrProc` function exists anywhere in the codebase. Process unregistration from `PROC_TABLE` appears to be handled by FatherProcess via `DeleteItem()` when a child dies, but this path was not fully traced. |
| VB-03 | `EnvTabPar` definition | Referenced at `Memory.c:L462` -- this appears to be a global array of `EnvTab` structs defined elsewhere (likely in `Global.h` or a component's source). Exact contents determine which environment variables are required. |
| VB-04 | `SonProcParams` definition | Referenced at `Memory.c:L575` and `SharedMemory.cpp:L4007`. This is a `ParamTab` array defining which shared-memory parameters each process needs. Appears to be defined per-component. |
| VB-05 | `MESSAGE_HEADER` struct layout | The socket framing header is referenced but its exact field sizes (which determine the wire protocol) are defined in `Global.h`. The `messageType` and `messageLen` fields are ASCII strings parsed with `atoi()`. |
| VB-06 | Complete list of `PcMessages[]` | Used in `BroadcastSonProcs()` at `Memory.c:L797-L798`. This message array determines what shutdown/control messages can be broadcast. |
| VB-07 | Shared memory key collision risk | Both `ShmKey1` and `SemKey1` use `0x000abcde`. If another application on the same host uses this key, there could be conflicts. |
| VB-08 | `FILE_MESG` temp file cleanup | `SetInterProcMesg()` creates temp files in `NamedpDir` (`Sockets.c:L1650-L1670`) but no cleanup mechanism was observed in GenLib. [NEEDS_VERIFICATION: whether temp files are cleaned up by the receiving process or an external mechanism] |
| VB-09 | `SharedSize` global variable | Referenced in `GET_FREE_EXTENT` macro (`SharedMemory.cpp:L214`) and `InitFirstExtent()`. Its value is passed from FatherProcess but the actual size value is determined by environment/parameters. |
| VB-10 | Thread safety | The library uses process-level semaphores for shared memory protection, but all static variables (`ConnTab`, `ExtentChain`, `sem_busy_flg`, etc.) are not thread-safe. The system appears to be purely multi-process, not multi-threaded. |

---

## Appendix: File Cross-Reference Summary

```
Memory.c
  Calls into: Semaphores.c (OpenSemaphore)
              SharedMemory.cpp (OpenFirstExtent, ShmGetParamsByName, AddCurrProc, AddToSonsCount)
              Sockets.c (GetCurrNamedPipe, ListenSocketNamed, ConnectSocketNamed, WriteSocket, CloseSocket, DisposeSockets)
              GeneralError.c (GerrLogExit, GerrLogReturn, GerrLogMini)

Sockets.c
  Calls into: GeneralError.c (GerrLogReturn, GerrLogMini)
  Uses globals: NamedPipeSocket, CurrNamedPipe, NamedpDir, ReadTimeout, WriteTimeout

SharedMemory.cpp
  Calls into: Semaphores.c (WaitForResource, ReleaseResource)
              Memory.c (MemAllocReturn, MemReallocReturn)
              GeneralError.c (GerrLogReturn, GerrLogMini)
  Uses globals: master_shm_header, ExtentChain, ConnTab, TableTab[] (extern from GenSql)

Semaphores.c
  Calls into: GeneralError.c (GerrLogReturn)
  Uses globals: sem_num, sem_busy_flg, SemKey1, NamedpDir

GeneralError.c
  Calls into: (standalone -- no GenLib dependencies)
  Uses globals: LogDir, LogFile, buf

GxxPersonality.c
  Calls into: (standalone -- debug helper only)
```
