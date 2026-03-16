# GenLib - System Architecture

**Component**: GenLib (libgenlib.a)
**Task ID**: DOC-GENLIB-001
**Generated**: 2026-03-16
**Source**: `source_code/GenLib/`

---

## 1. Architectural Overview

GenLib appears to function as the runtime substrate for every process in the MACCABI healthcare backend. Based on the source code analysis, it provides five tightly integrated subsystems:

```
+-------------------------------------------------------------------+
|                        GenLib (libgenlib.a)                        |
|                                                                   |
|  +------------------+  +------------------+  +------------------+ |
|  |  Process         |  |  IPC Socket      |  |  Error Logging   | |
|  |  Lifecycle       |  |  System          |  |  (GeneralError.c)| |
|  |  (Memory.c)      |  |  (Sockets.c)     |  +------------------+ |
|  +--------+---------+  +--------+---------+          ^            |
|           |                     |                    |            |
|           v                     v                    |            |
|  +------------------+  +------------------+          |            |
|  |  Shared Memory   |  |  Semaphore       +----------+            |
|  |  Engine          |  |  Facility        |                       |
|  |  (SharedMemory   |  |  (Semaphores.c)  |                       |
|  |   .cpp)          |  +------------------+                       |
|  +------------------+                                             |
|                                                                   |
|  +------------------+                                             |
|  |  Utilities:      |                                             |
|  |  Hebrew, Date,   |                                             |
|  |  String, Alloc   |                                             |
|  |  (Memory.c)      |                                             |
|  +------------------+                                             |
+-------------------------------------------------------------------+
```

### Dependency Flow

According to the dependency graph (`CHUNKS/GenLib/graph.json`), the internal call hierarchy flows as follows:

1. **GeneralError.c** is the leaf dependency -- called by all other files, but calls no GenLib functions itself
2. **Semaphores.c** depends only on GeneralError.c
3. **SharedMemory.cpp** depends on Semaphores.c, Memory.c (allocation wrappers), and GeneralError.c
4. **Sockets.c** depends only on GeneralError.c
5. **Memory.c** (process lifecycle) depends on all of the above: Semaphores.c, SharedMemory.cpp, Sockets.c, and GeneralError.c

---

## 2. Process Lifecycle

### 2.1 InitSonProcess -- The 13-Step Child Bootstrap

According to code at `Memory.c:L552-L612`, `InitSonProcess()` is the standard bootstrap function called by every child process at startup. Based on the code comments and function calls observed, it performs the following steps:

| Step | Code Reference | Description |
|------|---------------|-------------|
| 1 | `Memory.c:L560` | `ABORT_ON_ERR(InitEnv(GerrId))` -- Allocate environment name/value hash buffers |
| 2 | `Memory.c:L563` | `ABORT_ON_ERR(HashEnv(GerrId))` -- Read all `EnvTabPar` environment variables and cache them |
| 3 | `Memory.c:L566` | `RETURN_ON_ERR(OpenSemaphore())` -- Open the system semaphore created by FatherProcess |
| 4 | `Memory.c:L569` | `RETURN_ON_ERR(OpenFirstExtent())` -- Attach to the first shared memory extent |
| 5 | `Memory.c:L572` | `GetCurrProgName(proc_pathname, CurrProgName)` -- Extract program filename from path |
| 6 | `Memory.c:L575` | `RETURN_ON_ERR(ShmGetParamsByName(SonProcParams, PAR_LOAD))` -- Load runtime parameters from shared memory PARM_TABLE |
| 7 | `Memory.c:L578-L589` | `plock(PROCLOCK)` or `mlockall(MCL_CURRENT)` -- Optionally lock pages in primary memory (if `LockPagesMode == 1`) |
| 8 | `Memory.c:L592` | `setuid(atoi(MacUid))` -- Drop to unprivileged user ID |
| 9 | `Memory.c:L598` | `GetCurrNamedPipe(CurrNamedPipe)` -- Build per-process socket path |
| 10 | `Memory.c:L601` | `RETURN_ON_ERR(ListenSocketNamed(CurrNamedPipe))` -- Create and listen on AF_UNIX socket |
| 11 | `Memory.c:L604` | `RETURN_ON_ERR(AddCurrProc(retrys, proc_type, eicon_port, system))` -- Register in PROC_TABLE |
| 12 | `Memory.c:L608` | `RETURN_ON_ERR(AddToSonsCount(1))` -- Increment sons_count in STAT_TABLE |
| 13 | `Memory.c:L611` | `return(MAC_OK)` -- Return success |

**Error handling strategy**: Steps 1-2 use `ABORT_ON_ERR` (process exits on failure). Steps 3-12 use `RETURN_ON_ERR` (error is returned to caller). This appears to reflect that environment setup is considered unrecoverable, while subsequent steps may allow the caller to handle errors.

### 2.2 ExitSonProcess -- Clean Shutdown

According to code at `Memory.c:L625-L655`, `ExitSonProcess(status)` performs a three-step shutdown:

1. `DisposeSockets()` -- Unlinks the named pipe file and closes connections (`Memory.c:L633`)
2. `DetachAllExtents()` -- Detaches all shared memory extents (`Memory.c:L639`)
3. `CloseSemaphore()` -- Marks the semaphore as unused locally (`Memory.c:L645`)
4. `exit(status)` / `_exit(status)` -- Terminates the process (`Memory.c:L651-L653`)

### 2.3 Calling Components

According to the research at `RESEARCH/GenLib_deepdive.md`, `InitSonProcess()` is called by the following components:

| Component | Call Site | proc_type | system |
|-----------|-----------|-----------|--------|
| SqlServer | `SqlServer.c:L211` | SQLPROC_TYPE | PHARM_SYS |
| As400UnixServer | `As400UnixServer.c:L194` | AS400TOUNIX_TYPE | PHARM_SYS |
| As400UnixClient | `As400UnixClient.c:L174` | UNIXTOAS400_TYPE | PHARM_SYS |
| As400UnixDocServer | `As400UnixDocServer.c:L88` | DOCAS400TOUNIX_TYPE | DOCTOR_SYS |
| As400UnixDoc2Server | `As400UnixDoc2Server.c:L101-L107` | DOC2AS400TOUNIX_TYPE | DOCTOR_SYS |
| PharmTcpServer | `PharmTcpServer.cpp:L124-L128` | PHARM_TCP_SERVER_TYPE | variable |
| PharmWebServer | `PharmWebServer.cpp:L123-L128` | WEBSERVERPROC_TYPE | variable |

**Note**: FatherProcess does NOT call `InitSonProcess()`. It performs its own initialization sequence and calls `AddCurrProc()` directly at `FatherProcess.c:L351`.

---

## 3. FatherProcess Relationship

Based on the research at `RESEARCH/context_summary.md` and the GenLib source code, the complete system boot sequence appears to proceed as follows:

### 3.1 Father Initialization (before children)

1. FatherProcess installs SIGTERM handler (`FatherProcess.c:L239-L253`)
2. Connects to database via `SQLMD_connect()` (`FatherProcess.c:L259-L271`)
3. Builds named pipe path, calls `ListenSocketNamed()` (`FatherProcess.c:L299-L301`)
4. Calls `CreateSemaphore()` (`FatherProcess.c:L303-L305`)
5. Calls `InitFirstExtent(SharedSize)` (`FatherProcess.c:L306-L308`)
6. Iterates `TableTab[]` calling `CreateTable()` for each table (`FatherProcess.c:L309-L334`)
7. Calls `AddCurrProc(0, FATHERPROC_TYPE, 0, PHARM_SYS | DOCTOR_SYS)` (`FatherProcess.c:L350-L351`)
8. Spawns child processes via `Run_server()` for each configured program

### 3.2 Child Initialization (per child)

Each child process, after being forked and exec'd by `Run_server()`:
1. Calls `InitSonProcess()` (the 13-step sequence described above)
2. Enters its main processing loop (component-specific)

### 3.3 Father Shutdown Sequence

According to the FatherProcess documentation and GenLib code:
1. FatherProcess calls `BroadcastSonProcs()` to send shutdown messages to all children
2. Waits for children to exit
3. Calls `KillAllExtents()` to destroy shared memory
4. Calls `DeleteSemaphore()` to remove the system semaphore

---

## 4. Shared Memory Architecture

### 4.1 Extent System

According to code at `SharedMemory.cpp:L60-L86`, the shared memory subsystem uses a multi-extent architecture:

- **Extents** are System V shared memory segments created with `shmget()` and attached with `shmat()`
- The first extent contains the `MASTER_SHM_HEADER` which tracks all extents and tables
- Additional extents can be allocated dynamically via `AddNextExtent()` when existing extents run out of space
- Each extent has an `EXTENT_HEADER` with free space tracking and next/back extent pointers

**Extent Pointer Resolution**: According to the macro at `SharedMemory.cpp:L134-L142`, the `GET_ABS_ADDR(extent_ptr)` macro resolves an `EXTENT_PTR` (an `{ext_no, offset}` pair) to an absolute memory address via:
```
ExtentChain[extent_ptr.ext_no].start_addr + extent_ptr.offset
```
A null extent pointer is represented by `ext_no == -1` (`SharedMemory.cpp:L121`).

### 4.2 Local Tracking Structures

Each process maintains its own local tracking structures (not in shared memory):

- **ExtentChain[]**: Array of `EXTENT_ADDR` structs mapping extent numbers to `{shm_id, start_addr}` pairs. Initially allocated with 40 slots (`SharedMemory.cpp:L69-L73`)
- **ConnTab[CONN_MAX=150]**: Array of `TABLE` connection handles tracking open table connections (`SharedMemory.cpp:L88-L94`)
- **ConnCount**: Global count of active connections (`SharedMemory.cpp:L93-L94`)
- **AttachedExtentCount**: Number of locally attached extents (`SharedMemory.cpp:L84-L86`)

### 4.3 Shared Memory Tables

Based on the research at `RESEARCH/context_summary.md`, the system defines the following shared memory tables via `TableTab[]` in `GenSql.c:L86-L126`:

| Table Macro | String Name | Row Type | Purpose |
|-------------|-------------|----------|---------|
| PARM_TABLE | parm_table | PARM_DATA | Runtime parameters from database |
| PROC_TABLE | proc_table | PROC_DATA | Process registry (pid, type, named_pipe, system) |
| STAT_TABLE | stat_table | STAT_DATA | System status and sons_count |
| UPDT_TABLE | updt_table | UPDT_DATA | Table update timestamps |
| TSTT_TABLE | tstt_table | TSTT_DATA | Pharmacy time statistics |
| DSTT_TABLE | dstt_table | DSTT_DATA | Doctor time statistics |
| DIPR_TABLE | dipr_table | DIPR_DATA | Died processes tracking |
| PRSC_TABLE | prsc_table | PRESCRIPTION_ROW | Prescription ID counter |
| MSG_RECID_TABLE | msg_recid_table | MESSAGE_RECID_ROW | Message record ID counter |

### 4.4 Table Data Model

According to the analysis, each table in shared memory uses a doubly-linked-list structure:

- **TABLE_HEADER**: Contains table metadata (name, record_count, free_count, first/last row pointers, first/last free pointers, next/back table pointers)
- **TABLE_ROW**: Per-row wrapper with next/back row pointers and a row_data extent pointer
- **Free chain**: Deleted rows are moved to a per-table free chain. `AddItem()` checks the free chain first before allocating new space from the extent
- **Word alignment**: Record sizes are word-aligned to `sizeof(int)` boundary, according to code at `SharedMemory.cpp:L1770-L1776`

---

## 5. Concurrency Model

### 5.1 Single Semaphore

According to code at `Semaphores.c:L92` and `SharedMemory.cpp:L80-L81`, both the semaphore and shared memory use a hard-coded key (documented by location only for security). A single System V semaphore protects all shared memory access across the entire system.

### 5.2 The RETURN Macro

According to code at `SharedMemory.cpp:L104`:
```c
#define RETURN(a) {ReleaseResource();return(a);}
```
This macro is used throughout `SharedMemory.cpp` to ensure the semaphore is released before returning from any function. Every function that acquires the semaphore via `WaitForResource()` uses `RETURN()` instead of plain `return` for all exit paths.

### 5.3 Re-Entrant Locking

According to code at `Semaphores.c:L508-L513` and `Semaphores.c:L640-L650`, the semaphore supports re-entrant (pushable) acquisition:

- **WaitForResource()**: If `sem_busy_flg > 0` (semaphore already held by this process), increments the counter and returns immediately without calling `semop()`
- **ReleaseResource()**: If `sem_busy_flg > 1`, decrements the counter. Only performs the actual `semop()` release when the counter reaches zero

This appears to prevent deadlocks when GenLib functions call other GenLib functions that also require the semaphore. For example, `CreateTable()` calls `OpenTable()`, both of which need semaphore access.

### 5.4 SEM_UNDO for Crash Safety

According to code at `Semaphores.c:L86-L90`, all `sembuf` operations include the `SEM_UNDO` flag:
```c
W   = { 0, -1, SEM_UNDO }
P   = { 0,  1, SEM_UNDO }
```
This ensures that if a process crashes while holding the semaphore, the kernel automatically releases it, preventing system-wide deadlock.

### 5.5 Thread Safety Note

Based on the analysis at `CHUNKS/GenLib/analysis.json`, all static variables (`ConnTab`, `ExtentChain`, `sem_busy_flg`) are not thread-safe. The system appears to be designed as purely multi-process, not multi-threaded. [NEEDS_VERIFICATION: whether any component uses threads]

---

## 6. Run_server -- Process Spawning

According to code at `Memory.c:L1013-L1252`, `Run_server()` spawns a child process by:

1. Extracting the program name and initial arguments from `proc_data->proc_name` (whitespace-delimited) (`Memory.c:L1051-L1052`)
2. Constructing the executable path using `BinDir` (`Memory.c:L1056`)
3. Verifying the executable exists via `fopen()` (`Memory.c:L1060-L1069`)
4. Allocating an arguments array, initially 10 slots (`Memory.c:L1072-L1073`)
5. Parsing additional arguments from the command string (`Memory.c:L1081-L1093`)
6. Adding the `-r` retries argument (`Memory.c:L1099-L1103`)
7. Looking up additional arguments from PARM_TABLE matching `{prog_name}.*` pattern (`Memory.c:L1106-L1112`)
8. Rotating the child's log file (`Memory.c:L1108`)
9. Calling `fork()` -- child calls `execv()`, parent stores child PID (`Memory.c` fork section)

---

## 7. Broadcast System

According to code at `Memory.c:L664-L839`, `BroadcastSonProcs()` sends a message to all child processes by:

1. Iterating PROC_TABLE using `ActItems()`
2. Filtering processes by `proc_flg` (FIRST_SQL, NOT_CONTROL, JUST_CONTROL)
3. For each matching process: setting a 1-second SIGALRM timeout, connecting to the child's named pipe via `ConnectSocketNamed()`, sending the message via `WriteSocket()`, and closing the connection
4. The `alarm_handler()` function at `Memory.c:L830` is a no-op SIGALRM handler used to break out of blocking `connect()` calls if a child process is unresponsive

---

*Generated by CIDRA Documenter Agent - DOC-GENLIB-001*
