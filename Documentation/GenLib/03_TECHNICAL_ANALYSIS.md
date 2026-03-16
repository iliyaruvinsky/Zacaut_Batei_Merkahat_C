# GenLib - Technical Analysis

**Component**: GenLib (libgenlib.a)
**Task ID**: DOC-GENLIB-001
**Generated**: 2026-03-16
**Source**: `source_code/GenLib/`

---

## 1. IPC Socket System

### 1.1 Socket Architecture

According to the file header at `Sockets.c:L12-L17`, the IPC system provides "library routines for communication sockets handling." Despite being called "named pipes" throughout the codebase, the system uses **AF_UNIX stream sockets** (`SOCK_STREAM`), not POSIX FIFOs.

The system supports two socket families:
- **AF_UNIX** (Unix-domain sockets) -- Used for all internal inter-process communication
- **AF_INET** (TCP/IP sockets) -- Used for external connections (e.g., PharmTcpServer)

### 1.2 Named Socket Creation

**ListenSocketNamed()** (`Sockets.c:L89-L207`):
According to code at `Sockets.c:L110-L114`, the function:
1. Creates a socket: `socket(AF_UNIX, SOCK_STREAM, 0)`
2. Binds to the specified filesystem path
3. Calls `listen(socket_num, SOMAXCONN)` with the system's maximum backlog
4. Stores the socket number in the global `NamedPipeSocket` variable

**ConnectSocketNamed()** (`Sockets.c:L217-L303`):
Creates an AF_UNIX socket and connects to a named path. According to the repository metadata, it includes EINTR detection for alarm-interrupted connects, which is used by `BroadcastSonProcs()` with its 1-second SIGALRM timeout.

### 1.3 Named Socket Path Generation

According to code at `Sockets.c:L1376-L1387`, `GetCurrNamedPipe()` builds a per-process socket path using:
```c
sprintf(buffer, "%s/%s%d_%ld", NamedpDir, NAMEDP_PREFIX, getpid(), (long)time(NULL));
```
This generates a unique path incorporating the process ID and current timestamp, ensuring no collisions between processes.

### 1.4 TCP Socket Support

**ListenSocketUnnamed()** (`Sockets.c:L318-L572`): Creates, binds, and listens on an AF_INET/SOCK_STREAM (TCP) socket with `SO_REUSEADDR`.

**ConnectSocketUnnamed()** (`Sockets.c:L489-L572`): Creates an AF_INET socket and connects to a remote address:port using `gethostbyname()` for DNS resolution.

### 1.5 Socket Lifecycle

According to the code:
- **AcceptConnection()** (`Sockets.c:L583`): Wraps `accept()` with error handling
- **CloseSocket()** (`Sockets.c:L634`): Wraps `close()` with a guard against closing file descriptors less than 3 (stdin, stdout, stderr)
- **DisposeSockets()** (`Sockets.c:L656`): Calls `unlink()` on the `CurrNamedPipe` path to remove the named socket file from the filesystem

---

## 2. Message Framing Protocol

### 2.1 MESSAGE_HEADER Wire Format

All socket messages use a `MESSAGE_HEADER` struct defined in `Global.h` [NEEDS_VERIFICATION: exact field sizes]. According to code at `Sockets.c:L1196-L1239`:

- `get_header_details()` reads the fixed-size header struct, then parses `messageType` and `messageLen` using `atoi()`, indicating these are ASCII-formatted string fields
- `set_header_details()` formats the type and length with `sprintf()` and writes the header

This ASCII-based framing appears to allow human-readable debugging of the wire protocol.

### 2.2 ReadSocket

According to code at `Sockets.c:L686-L820`, `ReadSocket()`:
1. Reads the `MESSAGE_HEADER` via `get_header_details()` to obtain message type and length
2. Reads the payload via `read_socket_data()` for the specified length
3. Handles overflow: if the declared message length exceeds the caller's buffer, it reads excess data and discards it
4. Returns the message type and actual bytes read to the caller

### 2.3 WriteSocket

According to code at `Sockets.c:L820-L870`, `WriteSocket()`:
1. Sends the `MESSAGE_HEADER` via `set_header_details()` with the message type and payload length
2. Sends the payload via `write_socket_data()`

### 2.4 WriteSocketHead

According to code at `Sockets.c:L870-L940`, `WriteSocketHead()` prepends a status byte before the payload, effectively adding a one-byte header to the data portion of the framed message.

---

## 3. FILE_MESG vs DATA_MESG Pattern

### 3.1 Overview

The inter-process messaging system supports two payload modes, controlled by a `mesg_type` field in the private `MESG_HEADER` struct defined at `Sockets.c:L38-L43`:
```c
typedef struct { int length; char mesg_type; } MESG_HEADER;
```

### 3.2 DATA_MESG (Inline Data)

For small payloads:
- **SetInterProcMesg()** (`Sockets.c:L1622`): Copies data directly into the output buffer after the `MESG_HEADER`
- **GetInterProcMesg()** (`Sockets.c:L1516`): Copies data directly from the input buffer

### 3.3 FILE_MESG (File-Based Transfer)

For large payloads that may exceed socket buffer sizes:
- **SetInterProcMesg()**: Writes the data to a temporary file in `NamedpDir` (named with pid + timestamp), then puts the filename in the output buffer
- **GetInterProcMesg()**: Opens the file named in the payload, reads its content into the output buffer

According to the analysis at `CHUNKS/GenLib/analysis.json` (VB-GL-06), no cleanup mechanism for these temporary files was observed within GenLib itself. [NEEDS_VERIFICATION: whether temp files are cleaned up by the receiving process or an external mechanism]

---

## 4. Select Multiplexing

### 4.1 GetSocketMessage

According to code at `Sockets.c:L1396-L1510`, `GetSocketMessage()` is the primary message reception function:

1. Initializes `fd_set` with `FD_ZERO` and `FD_SET(NamedPipeSocket, &read_fds)`
2. Converts `micro_sec` parameter to `timeval` struct (`Sockets.c:L1433-L1434`)
3. Calls `select(NamedPipeSocket + 1, &read_fds, NULL, NULL, &timeout)`
4. On `select()` returning -1: logs error and returns `SELECT_ERR` (unless a signal was caught, per `Sockets.c:L1452`)
5. On timeout (return 0): falls through to return `MAC_OK`
6. On connection ready: calls `AcceptConnection()`, then `ReadSocket()` to get the framed message
7. If `close_flg == LEAVE_OPEN_SOCK`: returns the accepted socket for bidirectional communication
8. Otherwise: closes the accepted socket via `CloseSocket()`

A comment at `Sockets.c:L1438-L1440` by DonR notes: "Select() is considered an 'old' library function, and poll() is now preferred. At some point we should look at changing over to poll() - but it's not at all urgent."

### 4.2 Low-Level I/O with Select

**read_socket_data()** (`Sockets.c:L948-L1078`):
According to the chunk metadata, this function loops reading from a socket with `select()` timeout (using the `ReadTimeout` global variable), handling partial reads and EINTR interruptions.

**write_socket_data()** (`Sockets.c:L1078-L1196`):
Similarly loops writing with `select()` timeout using the `WriteTimeout` global variable.

Both functions handle partial I/O by looping until the full requested byte count has been transferred or an error occurs.

---

## 5. Semaphore Locking

### 5.1 Implementation Selection

According to code at `Semaphores.c:L41-L48`, three semaphore implementations exist:
```c
#define XENIX_SEM  1  /* Xenix semaphores */
#define POSIX_SEM  2  /* Posix semaphores */
#define BSD_SEM    3  /* Bsd semaphores   */

#define SEMAPHORES BSD_SEM
```
Only the BSD (System V) implementation is active. The XENIX and POSIX implementations remain in the source as dead code.

### 5.2 Semaphore Operations (BSD Path)

According to code at `Semaphores.c:L86-L90`, the `sembuf` operations are defined as:

| Struct | Value | Purpose |
|--------|-------|---------|
| `W` | `{0, -1, SEM_UNDO}` | Wait (decrement/acquire) |
| `P` | `{0, +1, SEM_UNDO}` | Post (increment/release) |
| `WNB` | `{0, -1, SEM_UNDO\|IPC_NOWAIT}` | Non-blocking wait |
| `PNB` | `{0, +1, SEM_UNDO\|IPC_NOWAIT}` | Non-blocking post |

### 5.3 CreateSemaphore (Father Only)

According to code at `Semaphores.c:L109` and the function comment at `Semaphores.c:L104-L107`:
> "Create semaphore and return its id. This function should be called only by Father process. It also initializes & opens the semaphore for access."

The function constructs a path `{NamedpDir}/mac_sem1` (`Semaphores.c:L116`), then uses `semget()` + `semctl(SETVAL, 1)` to create and initialize the semaphore to value 1 (unlocked).

### 5.4 WaitForResource -- Detailed Flow

According to code at `Semaphores.c:L487-L616`:

1. **Re-entrant check** (`L508-L513`): If `sem_busy_flg > 0`, increment counter and return immediately
2. **Semaphore open check** (`L565-L568`): If `sem_num == -1`, return `SEM_NOT_OPEN`
3. **Blocking semop** (`L577-L590`): Call `semop(sem_num, &W, 1)` which decrements the semaphore (blocks if already 0)
4. **Error handling** (`L598-L606`): On `semop()` failure, log error and return `SEM_WAIT_ERR`
5. **Set busy flag** (`L612`): Set `sem_busy_flg = 1` to indicate semaphore is held

### 5.5 ReleaseResource -- Detailed Flow

According to code at `Semaphores.c:L625-L734`:

1. **Not held check** (`L640-L643`): If `sem_busy_flg == 0`, return `SEM_RELISED` (note: typo in original code)
2. **Re-entrant decrement** (`L645-L650`): If `sem_busy_flg > 1`, decrement counter and return (do not actually release)
3. **Actual release**: Only when `sem_busy_flg == 1`, perform `semop(sem_num, &P, 1)` to increment the semaphore

---

## 6. Error Handling Patterns

### 6.1 Logging Functions

According to code at `GeneralError.c`, the logging API has three implicit severity levels:

**Fatal** (process exits):
- `GerrLogExit()` (`GeneralError.c:L92`): Logs full-format error, then `exit(status)`
- `GmsgLogExit()` (`GeneralError.c:L445`): Logs titled message, then `exit(status)`

**Error** (process continues):
- `GerrLogReturn()` (`GeneralError.c:L165`): Logs full-format error, sleeps 250ms, then returns
- `GerrLogToFileName()` (`GeneralError.c:L382`): Logs to specified file, sleeps 250ms, then returns
- `GmsgLogReturn()` (`GeneralError.c:L521`): Logs titled message, then returns

**Info/Warning** (minimal logging):
- `GerrLogMini()` (`GeneralError.c:L248`): Logs one-liner message
- `GerrLogFnameMini()` (`GeneralError.c:L314`): Logs one-liner to specified file

### 6.2 Log File Destinations

According to code at `GeneralError.c:L594-L661`:
- Primary log: `{LogDir}/{LogFile}` -- opened for append
- Fallback: `stderr` (if `LogDir` or `LogFile` are empty or unavailable)
- Alternate files: `{LogDir}/{fname}` via `open_other_file()` which creates the directory path if needed (`GeneralError.c:L695`)

### 6.3 Log Message Formats

According to code at `GeneralError.c:L47-L77`:

**Standard format** (`ErrorFmt`):
```
MESSAGE:
   PID {pid}    User: {user}    Working dir: {cwd}
   From {source} Line {line} at {timestamp}
   Message: {message}
---------------------------------------------------------------
```

**Titled format** (`TitleFmt`):
```
*******************<<< START OF MESSAGE >>>********************
        TITLE : {title}
        ...
********************<<< END OF MESSAGE >>>*********************
```

**Mini format** (`GerrLogMini`): Single-line format with message, date, and database timestamp.

### 6.4 Doom-Loop Protection

According to the comment by DonR at `GeneralError.c:L226-L233`:
> "Since these errors are normally 'fatal' (and should occur almost never), add a 1/4-second delay before returning to the calling routine. This will prevent 'doom loop' situations from filling up the whole disk drive with endless error messages - four per second is still plenty if we're in an endless loop, but it won't push the logfile to multiple gigabytes in any hurry."

The protection is implemented as `usleep(250000)` (250 milliseconds) at `GeneralError.c:L232`. The same protection is applied in `GerrLogToFileName()` at `GeneralError.c:L429-L430`.

### 6.5 Error Macro System

Based on the patterns observed throughout the codebase, the error macros (defined in `Global.h`) appear to include:

| Macro | Behavior |
|-------|----------|
| `GerrId` | Expands to `GerrSource, __LINE__` providing automatic file:line identification |
| `GerrStr` | Expands to `errno, strerror(errno)` for system error formatting |
| `RETURN_ON_ERR(expr)` | Evaluates expression and returns if error |
| `ABORT_ON_ERR(expr)` | Evaluates expression and aborts process if error |
| `CONT_ON_ERR(expr)` | Evaluates expression and continues loop on error |
| `RETURN_ON_NULL(ptr)` | Returns error if pointer is NULL |

Each source file defines `static char *GerrSource = __FILE__;` (e.g., `Memory.c:L30`, `SharedMemory.cpp:L29`, `Sockets.c:L27`) to provide the source filename for error messages.

---

## 7. Key Design Patterns

### 7.1 Operation Function Pattern

The shared memory table engine uses a callback-based iteration pattern. Functions like `GetItem`, `SetItem`, `IncrementCounters`, `LockSqlServer`, etc. are "operation functions" with the signature `int func(TABLE *tablep, OPER_ARGS args)` that are passed to `ActItems()`. The operation function returns:
- `OPER_FORWARD` -- advance to next row
- `OPER_STOP` -- stop iteration
- `OPER_BACKWARD` -- move to previous row

### 7.2 Connection Handle Pattern

Table access is managed through connection handles:
1. `OpenTable()` walks the table chain, finds the table by name, allocates a `ConnTab[]` entry, and returns a `TABLE*` pointer
2. The `TABLE` struct contains a `conn_id`, cursor position (`curr_row`), and pointer to the `TABLE_HEADER`
3. `CloseTable()` releases the `ConnTab[]` entry
4. `CHECK_TAB_PTR()` macro validates that a connection handle is valid before use (`SharedMemory.cpp:L148-L180`)

### 7.3 Extent Chaining Pattern

When a table operation needs space and the current extent is full:
1. The `GET_FREE_EXTENT()` macro (`SharedMemory.cpp:L187-L229`) iterates all extents looking for one with sufficient free space
2. If no extent has enough space, it calls `AddNextExtent()` to allocate a new shared memory segment
3. The new extent is chained into the master header
4. The `ATTACH_ALL()` macro (`SharedMemory.cpp:L269-L278`) ensures the new extent is attached in the local process

### 7.4 Validity Check Pattern

The `VALIDITY_CHECK()` macro at `SharedMemory.cpp:L110-L114` returns `SHM_NO_INIT` if `master_shm_header` is NULL, appearing at the start of shared memory functions to catch calls before initialization.

---

## 8. LOG_EVERYTHING Flag

According to code at `SharedMemory.cpp:L33-L34`:
```c
// DonR 25Apr2022: Suppress diagnostics that make the "kuku" file huge if the system runs for a long time.
#define LOG_EVERYTHING  0
```

This flag appears to control verbose diagnostic logging in `SharedMemory.cpp`. When set to 0, it suppresses detailed log messages that would otherwise cause the log file (referred to as "kuku") to grow excessively during long-running system operation.

---

*Generated by CIDRA Documenter Agent - DOC-GENLIB-001*
