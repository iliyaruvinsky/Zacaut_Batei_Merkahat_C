# GenLib (source_code/GenLib) — Deep Dive (RES-DEEPDIVE-001)

**Generated:** 2026-02-02  
**Scope:** `source_code/GenLib/` (read every file in folder)  

## File inventory (exact)

Total files: **15** (exact). Total lines across folder: **10,604** (exact).

| File | Lines (exact) | Role / notes |
|------|--------------:|--------------|
| `source_code/GenLib/Memory.c` | 2196 | Process bootstrap + shared resources access (semaphore/shm) + process spawning helpers (details below). |
| `source_code/GenLib/Sockets.c` | 1758 | IPC socket layer (Unix-domain “named pipes” + message framing) (details below). |
| `source_code/GenLib/SharedMemory.cpp` | 4774 | Shared-memory table engine implementation (extents, table ops) (details below). |
| `source_code/GenLib/GeneralError.c` | 770 | Standard error logging/exit/return helpers (details below). |
| `source_code/GenLib/Semaphores.c` | 734 | Semaphore wrappers/utilities (details below). |
| `source_code/GenLib/GxxPersonality.c` | 27 | Debug-print helper; contains a disabled `__gxx_personality_v0` stub (details below). |
| `source_code/GenLib/Makefile` | 54 | Builds `libgenlib.a` from GenLib sources. |
| `source_code/GenLib/GenLib.vcxproj` | 84 | Visual Studio project file referencing GenLib sources. |
| `source_code/GenLib/GenLib.vcxproj.filters` | 42 | Visual Studio filter mapping. |
| `source_code/GenLib/GenLib.vcxproj.vspscc` | 10 | Visual Studio source-control metadata. |
| `source_code/GenLib/GenLib.vcxproj.user` | 3 | Visual Studio per-user settings (empty project stub). |
| `source_code/GenLib/.project` | 27 | Eclipse CDT project metadata. |
| `source_code/GenLib/.cproject` | 80 | Eclipse CDT toolchain/build-target configuration. |
| `source_code/GenLib/.cdtproject` | 33 | Eclipse CDT path entries + make targets. |
| `source_code/GenLib/.settings/language.settings.xml` | 12 | Eclipse language settings providers config. |

## Build + IDE configuration artifacts

### Makefile (Unix build)

- Produces static library target `libgenlib.a` from sources `GeneralError.c`, `Memory.c`, `Semaphores.c`, `Sockets.c`, `SharedMemory.cpp`, `GxxPersonality.c` (`source_code/GenLib/Makefile:L16-L25`).
- Uses `../Util/Defines.mak` and `../Util/Rules.mak` for shared build flags/rules (`source_code/GenLib/Makefile:L4-L5`, `source_code/GenLib/Makefile:L27-L28`).

### Visual Studio project (Windows build metadata)

- Declares `Debug|Win32` and `Release|Win32`, `CharacterSet=MultiByte`, toolset `v143` (`source_code/GenLib/GenLib.vcxproj:L3-L11`, `source_code/GenLib/GenLib.vcxproj:L24-L36`).
- Lists GenLib compilation units: `GeneralError.c`, `GxxPersonality.c`, `Memory.c`, `Semaphores.c`, `SharedMemory.cpp`, `Sockets.c` (`source_code/GenLib/GenLib.vcxproj:L70-L80`).

### Eclipse CDT project metadata (Unix/Linux toolchain)

- Eclipse CDT project is named `GenLib` and uses CDT managed build natures (`source_code/GenLib/.project:L2-L26`).
- Toolchain includes ELF binary parser and GNU Make build targets, with build targets `All`→`make all` and `Clean`→`make clean` (`source_code/GenLib/.cproject:L8-L14`, `source_code/GenLib/.cproject:L60-L78`).
- CDT make targets list `all` and `clean` (`source_code/GenLib/.cdtproject:L16-L30`).

## Code deep dive

### `GxxPersonality.c` (27 lines) — debug printing helper

- Includes `../Include/Global.h` and defines `Prn(char *frm, ...)` which prints `{pid, ppid}` + formatted message to stdout and flushes (`source_code/GenLib/GxxPersonality.c:L2-L4`, `source_code/GenLib/GxxPersonality.c:L12-L26`).
- Contains a disabled (compiled out via `#if 0`) stub for `__gxx_personality_v0` (commented out) (`source_code/GenLib/GxxPersonality.c:L5-L10`).

### `GeneralError.c` (770 lines) — error handling/logging

**Purpose:** Standard error logging helpers. The code uses two formatted layouts: a “sleeker” message layout (`ErrorFmt`) and a titled message layout (`TitleFmt`) (`source_code/GenLib/GeneralError.c:L46-L77`).

**Primary dependency:** `Global.h` for system globals/constants (`source_code/GenLib/GeneralError.c:L24-L28`).

#### Function inventory (all functions in file)

| Function | File:Line | Summary |
|----------|-----------|---------|
| `GerrLogExit` | `source_code/GenLib/GeneralError.c:L92-L156` | Format an error string with `vsprintf`, write using `ErrorFmt`, close log file, then terminate process (`exit` / `_exit`). |
| `GerrLogReturn` | `source_code/GenLib/GeneralError.c:L165-L237` | Same as `GerrLogExit` but returns; also sleeps 250ms to prevent disk-filling “doom loop” logs (`usleep(250000)`). |
| `GerrLogMini` | `source_code/GenLib/GeneralError.c:L248-L277` | Minimal log line with time + DB timestamp (no big header). |
| `GerrLogAddBlankLines` | `source_code/GenLib/GeneralError.c:L288-L303` | Append N blank lines to the log. |
| `GerrLogFnameMini` | `source_code/GenLib/GeneralError.c:L314-L344` | Minimal log line to a specified file. |
| `GerrLogFnameClear` | `source_code/GenLib/GeneralError.c:L355-L371` | Truncate/clear a specified file with a timestamp line. |
| `GerrLogToFileName` | `source_code/GenLib/GeneralError.c:L382-L433` | Full `ErrorFmt` message written to a specified file; includes 250ms “doom loop” delay. |
| `GmsgLogExit` | `source_code/GenLib/GeneralError.c:L445-L512` | “Titled” message (`TitleFmt`) then exit. |
| `GmsgLogReturn` | `source_code/GenLib/GeneralError.c:L521-L585` | “Titled” message (`TitleFmt`) then return. |
| `open_log_file` | `source_code/GenLib/GeneralError.c:L594-L661` | Resolve log target: returns `stderr` if `LogDir`/`LogFile` not set; otherwise opens `LogDir/LogFile` in append mode. |
| `close_log_file` | `source_code/GenLib/GeneralError.c:L670-L684` | Flushes; closes file unless it is `stderr`. |
| `open_other_file` | `source_code/GenLib/GeneralError.c:L695-L740` | Open arbitrary file under `LogDir`; attempts to create missing directories (via `create_log_directory`). |
| `create_log_directory` | `source_code/GenLib/GeneralError.c:L746-L770` | Builds parent directory and runs `mkdir -m777 -p <dir>` via `system()`. |

#### Key behaviors (with citations)

- **Log routing**: if either `LogDir[0]` or `LogFile[0]` is empty, `open_log_file()` returns `stderr` instead of opening a file (`source_code/GenLib/GeneralError.c:L633-L635`).
- **Timestamps**: both `GerrLogExit/Return` and the titled variants use `GetDbTimestamp()` (not `ctime`) in output (`source_code/GenLib/GeneralError.c:L137-L139`, `source_code/GenLib/GeneralError.c:L492-L496`).
- **Anti “doom loop” throttling**: `GerrLogReturn` sleeps 250ms before returning (`source_code/GenLib/GeneralError.c:L226-L234`). Same delay exists in `GerrLogToFileName` (`source_code/GenLib/GeneralError.c:L423-L431`).

#### Security / robustness notes (code-level)

- **Unbounded formatting**: several functions use `vsprintf()` into buffers without size limits (`source_code/GenLib/GeneralError.c:L115-L117`, `source_code/GenLib/GeneralError.c:L186-L189`). This is a classic overflow risk unless `buf` is guaranteed large enough.
- **Command execution**: `create_log_directory()` uses `system("mkdir -m777 -p ...")` where the path is derived from the filename (`source_code/GenLib/GeneralError.c:L763-L769`). If the filename/path is attacker-controlled, this is command-injection risk.

---

### `Semaphores.c` (734 lines) — semaphore helpers (System V / “BSD_SEM” path)

**Purpose:** Library routines for semaphore usage (“should be only with this library routines”) (`source_code/GenLib/Semaphores.c:L12-L16`).

**Implementation selection:** The file defines XENIX/POSIX/BSD semaphore backends and chooses `SEMAPHORES = BSD_SEM` (`source_code/GenLib/Semaphores.c:L41-L49`). In practice, the “BSD” backend uses System V APIs `semget/semctl/semop` (`source_code/GenLib/Semaphores.c:L146-L162`, `source_code/GenLib/Semaphores.c:L580-L591`).

#### Key globals / parameters

- **Semaphore mode**: `SemMode = 0777` (`source_code/GenLib/Semaphores.c:L59-L60`).
- **Key**: `SemKey1 = 0x000abcde` (`source_code/GenLib/Semaphores.c:L92-L93`).
- **Local re-entrancy (“pushable”)**: `sem_busy_flg` supports nested `WaitForResource()` calls in-process (`source_code/GenLib/Semaphores.c:L97-L98`, `source_code/GenLib/Semaphores.c:L508-L513`).
- **Semop operations**: W/P and WNB/PNB `struct sembuf` templates include `SEM_UNDO` (and optional `IPC_NOWAIT`) (`source_code/GenLib/Semaphores.c:L86-L90`).

#### Function inventory (all functions in file)

| Function | File:Line | Summary |
|----------|-----------|---------|
| `CreateSemaphore` | `source_code/GenLib/Semaphores.c:L109-L168` | Create + initialize semaphore (Father process). Uses `semget(SemKey1,1, SemMode|IPC_CREAT)` then `semctl(...SETVAL,1)` (`source_code/GenLib/Semaphores.c:L146-L162`). |
| `DeleteSemaphore` | `source_code/GenLib/Semaphores.c:L181-L264` | Remove semaphore (System V `IPC_RMID`) (`source_code/GenLib/Semaphores.c:L232-L238`). |
| `OpenSemaphore` | `source_code/GenLib/Semaphores.c:L275-L393` | Open existing semaphore via `semget(SemKey1,1, IPC_CREAT)` (“changed ... Linux”) (`source_code/GenLib/Semaphores.c:L368-L373`). |
| `CloseSemaphore` | `source_code/GenLib/Semaphores.c:L404-L477` | Logical “close” (resets local `sem_num` / `sem_p`); does not `IPC_RMID`. |
| `WaitForResource` | `source_code/GenLib/Semaphores.c:L487-L616` | Acquire lock. If already held locally, increments `sem_busy_flg` and returns (`source_code/GenLib/Semaphores.c:L508-L513`). Otherwise uses `semop(...,&W,1)` (`source_code/GenLib/Semaphores.c:L586-L591`) and sets `sem_busy_flg=1` (`source_code/GenLib/Semaphores.c:L612-L613`). |
| `ReleaseResource` | `source_code/GenLib/Semaphores.c:L625-L734` | Release lock. Supports nested release via decrementing `sem_busy_flg` (`source_code/GenLib/Semaphores.c:L645-L650`); otherwise uses `semop(...,&P,1)` (`source_code/GenLib/Semaphores.c:L704-L710`) and sets `sem_busy_flg=0` (`source_code/GenLib/Semaphores.c:L730-L732`). |

---

### `Sockets.c` (1758 lines) — IPC sockets + message framing

**Purpose:** “communication sockets handling” library (`source_code/GenLib/Sockets.c:L12-L16`).

**Primary dependency:** `Global.h` (`source_code/GenLib/Sockets.c:L24-L24`).

#### Named “pipes” are Unix-domain stream sockets

- `ListenSocketNamed()` creates `AF_UNIX/SOCK_STREAM` sockets and `bind()`s to the supplied pathname (`source_code/GenLib/Sockets.c:L110-L115`, `source_code/GenLib/Sockets.c:L154-L166`), then `listen()`s (`source_code/GenLib/Sockets.c:L184-L189`) and saves the listening FD into global `NamedPipeSocket` (`source_code/GenLib/Sockets.c:L203-L205`).
- `ConnectSocketNamed()` creates `AF_UNIX/SOCK_STREAM` and `connect()`s to the remote named socket (`source_code/GenLib/Sockets.c:L237-L242`, `source_code/GenLib/Sockets.c:L266-L272`).

#### Internet (TCP) sockets

- `ListenSocketUnnamed()` is `AF_INET/SOCK_STREAM`, sets `SO_REUSEADDR` + `SO_KEEPALIVE`, binds `INADDR_ANY:port`, and listens (`source_code/GenLib/Sockets.c:L339-L352`, `source_code/GenLib/Sockets.c:L361-L420`, `source_code/GenLib/Sockets.c:L433-L472`).
- `ConnectSocketUnnamed()` connects to a remote `AF_INET` socket (`source_code/GenLib/Sockets.c:L511-L546`).

#### Message framing model

1) **Socket header + payload**: read/write uses a `MESSAGE_HEADER` (from `Global.h`) and stores numeric fields as ASCII strings, parsed by `atoi()` (`source_code/GenLib/Sockets.c:L1196-L1234`) and produced by `sprintf()` (`source_code/GenLib/Sockets.c:L1273-L1282`).

2) **Inter-process payload modes**: `MESG_HEADER` in this file defines `(length, mesg_type)` (`source_code/GenLib/Sockets.c:L38-L43`) supporting:
- `FILE_MESG`: payload is a filename; receiver opens and reads file content (`source_code/GenLib/Sockets.c:L1543-L1567`, `source_code/GenLib/Sockets.c:L1583-L1586`).
- `DATA_MESG`: payload is copied directly from buffer (`source_code/GenLib/Sockets.c:L1588-L1597`).

#### Function inventory (all functions in file)

| Function | File:Line | Summary |
|----------|-----------|---------|
| `ListenSocketNamed` | `source_code/GenLib/Sockets.c:L89-L207` | Create + bind + listen on `AF_UNIX/SOCK_STREAM` named socket; sets `NamedPipeSocket`. |
| `ConnectSocketNamed` | `source_code/GenLib/Sockets.c:L217-L303` | Connect to named `AF_UNIX` socket; treats `EINTR` specially (`source_code/GenLib/Sockets.c:L274-L283`). |
| `ListenSocketUnnamed` | `source_code/GenLib/Sockets.c:L318-L481` | Bind/listen TCP socket; uses `SO_REUSEADDR`, `SO_KEEPALIVE`. |
| `ConnectSocketUnnamed` | `source_code/GenLib/Sockets.c:L489-L572` | Connect TCP socket; returns `CONNECT_ERR` on failure. |
| `AcceptConnection` | `source_code/GenLib/Sockets.c:L583-L626` | Wrapper around `accept()`. |
| `CloseSocket` | `source_code/GenLib/Sockets.c:L634-L648` | Closes socket; refuses “weird” FDs `< 3` (`source_code/GenLib/Sockets.c:L639-L644`). |
| `DisposeSockets` | `source_code/GenLib/Sockets.c:L656-L677` | Unlink current named-socket pathname `CurrNamedPipe` if set. |
| `ReadSocket` | `source_code/GenLib/Sockets.c:L686-L812` | Read framed message; discard extra bytes if message doesn’t fit provided buffer (`source_code/GenLib/Sockets.c:L744-L810`). |
| `WriteSocket` | `source_code/GenLib/Sockets.c:L820-L862` | Write framed message (header + payload). |
| `WriteSocketHead` | `source_code/GenLib/Sockets.c:L870-L941` | Write framed message with one-byte “status” prefix (`source_code/GenLib/Sockets.c:L887-L917`). |
| `read_socket_data` | `source_code/GenLib/Sockets.c:L948-L1070` | Low-level receive loop with `select()` timeout based on `ReadTimeout` (`source_code/GenLib/Sockets.c:L974-L1009`). |
| `write_socket_data` | `source_code/GenLib/Sockets.c:L1078-L1188` | Low-level send loop with `select()` timeout based on `WriteTimeout` (`source_code/GenLib/Sockets.c:L1102-L1134`). |
| `get_header_details` (static) | `source_code/GenLib/Sockets.c:L1196-L1239` | Read and parse `MESSAGE_HEADER` to `{type,len}` via `atoi()`. |
| `set_header_details` | `source_code/GenLib/Sockets.c:L1248-L1299` | Build and send `MESSAGE_HEADER` with `sprintf()`. |
| `GetPeerAddr` | `source_code/GenLib/Sockets.c:L1307-L1368` | Return peer IPv4 address as dotted string (`inet_ntoa`). |
| `GetCurrNamedPipe` | `source_code/GenLib/Sockets.c:L1376-L1388` | Create per-process unique path `NamedpDir/NAMEDP_PREFIX<pid>_<time>` (`source_code/GenLib/Sockets.c:L1380-L1387`). |
| `GetSocketMessage` | `source_code/GenLib/Sockets.c:L1396-L1510` | Wait for a connection request on `NamedPipeSocket`, accept, then `ReadSocket()` (`source_code/GenLib/Sockets.c:L1472-L1489`). |
| `GetInterProcMesg` | `source_code/GenLib/Sockets.c:L1516-L1615` | Decode `MESG_HEADER` + payload; FILE vs DATA mode. |
| `SetInterProcMesg` | `source_code/GenLib/Sockets.c:L1622-L1756` | Build `MESG_HEADER` + payload; in `FILE_MESG` mode writes data to a timestamped file under `NamedpDir` (`source_code/GenLib/Sockets.c:L1652-L1670`). |

---

### `Memory.c` (2196 lines) — process bootstrap + utilities + spawning helpers

Despite the filename, this unit is also the **child-process bootstrap** layer and contains multiple utility routines used across the system.

**Primary dependencies:**
- `Global.h` (always) (`source_code/GenLib/Memory.c:L23-L28`)
- `GenSql.h` (Linux only; included “to avoid compiler warnings”) (`source_code/GenLib/Memory.c:L23-L26`)

#### Function inventory (all functions in file)

| Function | File:Line | Summary |
|----------|-----------|---------|
| `MemAllocExit` | `source_code/GenLib/Memory.c:L57-L79` | `malloc()` wrapper; logs+exits on allocation failure (`source_code/GenLib/Memory.c:L66-L75`). |
| `MemReallocExit` | `source_code/GenLib/Memory.c:L88-L112` | `realloc()` wrapper; logs on failure. |
| `MemAllocReturn` | `source_code/GenLib/Memory.c:L121-L142` | `malloc()` wrapper; logs and returns `NULL` on failure. |
| `MemReallocReturn` | `source_code/GenLib/Memory.c:L151-L173` | `realloc()` wrapper; logs and returns `NULL` on failure. |
| `MemFree` | `source_code/GenLib/Memory.c:L181-L193` | `free()` wrapper with null-pointer warning (`source_code/GenLib/Memory.c:L185-L190`). |
| `ListMatch` | `source_code/GenLib/Memory.c:L200-L243` | Find exact string match in `char**` list; returns index or -1. |
| `ListNMatch` | `source_code/GenLib/Memory.c:L251-L295` | Find prefix match (`strncmp`) in `char**` list; returns index or -1. |
| `StringToupper` | `source_code/GenLib/Memory.c:L302-L316` | Uppercase a string in place. |
| `BufConvert` | `source_code/GenLib/Memory.c:L322-L347` | Reverse a buffer in place (swap ends). |
| `GetCurrProgName` | `source_code/GenLib/Memory.c:L353-L380` | Extract filename from path (splits on `/`) (`source_code/GenLib/Memory.c:L369-L380`). |
| `InitEnv` | `source_code/GenLib/Memory.c:L389-L432` | Allocate environment hash arrays `EnvNames/EnvValues` (`source_code/GenLib/Memory.c:L399-L412`). |
| `HashEnv` | `source_code/GenLib/Memory.c:L441-L538` | Populate `EnvTabPar[*].param_value` from cached hash or `getenv()`; logs missing vars (`source_code/GenLib/Memory.c:L484-L525`). |
| `InitSonProcess` | `source_code/GenLib/Memory.c:L552-L612` | Child-process bootstrap: env hash → open semaphore → attach shm → load params → create listen socket (named pipe) → register in PROC table (`source_code/GenLib/Memory.c:L559-L611`). |
| `ExitSonProcess` | `source_code/GenLib/Memory.c:L625-L655` | Cleanup (unlink named socket, detach shm, close semaphore) and exit (`source_code/GenLib/Memory.c:L633-L654`). |
| `BroadcastSonProcs` | `source_code/GenLib/Memory.c:L664-L821` | Father helper: iterate PROC table, connect to each child’s named socket, send shutdown message (`WriteSocket`) with alarm-based connect timeout (`source_code/GenLib/Memory.c:L773-L800`). |
| `alarm_handler` | `source_code/GenLib/Memory.c:L830-L839` | SIGALRM handler used to break out of blocking syscalls (no-op body). |
| `GetCommonPars` | `source_code/GenLib/Memory.c:L848-L903` | Parses common `-r` retry option from argv. |
| `GetFatherNamedPipe` | `source_code/GenLib/Memory.c:L910-L950` | Find FatherProcess record in `PROC_TABLE` and return its `named_pipe` (`source_code/GenLib/Memory.c:L930-L946`). |
| `TssCanGoDown` | `source_code/GenLib/Memory.c:L957-L1009` | Periodically checks if system can shut down by scanning proc table for workers (`source_code/GenLib/Memory.c:L968-L1008`). |
| `Run_server` | `source_code/GenLib/Memory.c:L1023-L1252` | Fork+exec child process using proc-table record + arguments from `PARM_TABLE`; logs startup; sets `DOCTOR_SYS` env var for doctor processes (`source_code/GenLib/Memory.c:L1050-L1173`, `source_code/GenLib/Memory.c:L1186-L1236`). |
| `GetSystemLoad` | `source_code/GenLib/Memory.c:L1261-L1302` | Executes `sar <interval>` via `popen` and parses load (shell-command dependency). |
| `my_nap` | `source_code/GenLib/Memory.c:L1309-L1324` | Sleep helper implemented with `select()` timeout (`source_code/GenLib/Memory.c:L1314-L1324`). |
| `run_cmd` | `source_code/GenLib/Memory.c:L1331-L1396` | Run command via `popen`, collect output lines into `answer` buffer, with SIGALRM timeout (`source_code/GenLib/Memory.c:L1336-L1394`). |
| `GetWord` | `source_code/GenLib/Memory.c:L1403-L1424` | Extract next whitespace-delimited word from a string. |
| `ToGoDown` | `source_code/GenLib/Memory.c:L1433-L1461` | Uses `get_sys_status()` to decide whether system or subsystem is “GOING_DOWN” (`source_code/GenLib/Memory.c:L1449-L1459`). |
| `buf_reverse` | `source_code/GenLib/Memory.c:L1471-L1485` | Reverse buffer using temp copy (renamed “for accuracy”) (`source_code/GenLib/Memory.c:L1464-L1485`). |
| `fix_char` | `source_code/GenLib/Memory.c:L1493-L1510` | Swap parentheses for Hebrew-directionality handling (`source_code/GenLib/Memory.c:L1495-L1510`). |
| `EncodeHebrew` | `source_code/GenLib/Memory.c:L1526-L1621` | Convert Hebrew strings between Win-1255, DOS, UTF-8 (limited input support) (`source_code/GenLib/Memory.c:L1542-L1576`, `source_code/GenLib/Memory.c:L1584-L1616`). |
| `WinHebToDosHeb` | `source_code/GenLib/Memory.c:L1630-L1729` | Reverse Hebrew runs + convert Win-1255 Hebrew byte range to DOS Hebrew (`source_code/GenLib/Memory.c:L1672-L1728`). |
| `WinHebToUTF8` | `source_code/GenLib/Memory.c:L1735-L1906` | Translate Win-1255 bytes to UTF-8 using an internal translation table and dynamic output buffer (`source_code/GenLib/Memory.c:L1844-L1857`, `source_code/GenLib/Memory.c:L1871-L1905`). |
| `GetMonsDif` | `source_code/GenLib/Memory.c:L1917-L1955` | Month-difference between two `YYYYMMDD` dates, with end-of-month adjustments (`source_code/GenLib/Memory.c:L1941-L1952`). |
| `GetDaysDiff` | `source_code/GenLib/Memory.c:L1969-L1996` | Day-difference between `YYYYMMDD` dates using `mktime()` (`source_code/GenLib/Memory.c:L1983-L1995`). |
| `AddDays` | `source_code/GenLib/Memory.c:L2008-L2034` | Add days to `YYYYMMDD` date; note “effective range” comment (`source_code/GenLib/Memory.c:L2000-L2034`). |
| `AddMonths` | `source_code/GenLib/Memory.c:L2043-L2092` | Add months to `YYYYMMDD` date with month-length correction (`source_code/GenLib/Memory.c:L2062-L2092`). |
| `AddSeconds` | `source_code/GenLib/Memory.c:L2104-L2122` | Add seconds to a time value via `mktime()` (`source_code/GenLib/Memory.c:L2114-L2121`). |
| `ConvertUnitAmount` | `source_code/GenLib/Memory.c:L2134-L2156` | Convert MIU/mcg→mg and g→mg; leave other units as-is (`source_code/GenLib/Memory.c:L2138-L2155`). |
| `ParseISODateTime` | `source_code/GenLib/Memory.c:L2168-L2196` | Parse `YYYY-MM-DDThh:mm:ss` into `YYYYMMDD` and `HHMMSS` with range checks (`source_code/GenLib/Memory.c:L2172-L2194`). |

#### Key workflows (with citations)

- **Child bootstrap (“son process”):** the canonical flow is `OpenSemaphore()` → `OpenFirstExtent()` → `GetCurrNamedPipe()` → `ListenSocketNamed()` → `AddCurrProc()` (`source_code/GenLib/Memory.c:L566-L605`).
- **Privilege drop:** after parameters are loaded, the process attempts `setuid(atoi(MacUid))` (`source_code/GenLib/Memory.c:L591-L595`).
- **Parent broadcast to children:** `BroadcastSonProcs()` connects to each `proc_data.named_pipe` from shared memory and sends a message using `WriteSocket()` (`source_code/GenLib/Memory.c:L775-L804`), with a 1-second alarm timeout around connect (`source_code/GenLib/Memory.c:L773-L786`).

#### Security / robustness notes (code-level)

- **Shell execution:** `GetSystemLoad()` and `run_cmd()` execute shell commands via `popen()` (`source_code/GenLib/Memory.c:L1271-L1273`, `source_code/GenLib/Memory.c:L1336-L1343`). If command strings become attacker-controlled, this is command-injection risk.
- **In-place argv tokenization:** `Run_server()` calls `strtok()` directly on `proc_data->proc_name` (`source_code/GenLib/Memory.c:L1050-L1053`), mutating the stored string (this can surprise callers if the structure is reused).

---

### `SharedMemory.cpp` (4774 lines) — shared-memory table engine + runtime state tables

**Purpose:** Shared memory “extent” allocator + table engine, plus shared tables for processes, params, statistics, and status (`source_code/GenLib/SharedMemory.cpp:L12-L16`).

**Primary dependencies:**
- `Global.h` + `CCGlobal.h` (`source_code/GenLib/SharedMemory.cpp:L36-L42`)
- `MsgHndlr.h` (included even under `NON_SQL`) (`source_code/GenLib/SharedMemory.cpp:L46-L50`)

#### Core data model (extents + table connections)

- **Master header:** `master_shm_header` points to `MASTER_SHM_HEADER` at the start of the first shared-memory segment (`source_code/GenLib/SharedMemory.cpp:L60-L60`, `source_code/GenLib/SharedMemory.cpp:L403-L423`).
- **Extents chain:** `ExtentChain[]` stores `{shm_id,start_addr}` for each attached extent (`source_code/GenLib/SharedMemory.cpp:L62-L71`).
- **Fixed key + mode:** first extent uses `ShmKey1 = 0x000abcde` (`source_code/GenLib/SharedMemory.cpp:L79-L82`) and `ShmMode = 0777` (`source_code/GenLib/SharedMemory.cpp:L75-L77`).
- **Table connections:** `ConnTab[CONN_MAX=150]` is a per-process connection table with `busy_flg`, `table_header`, `curr_row` etc (`source_code/GenLib/SharedMemory.cpp:L88-L95`).

#### Function inventory (all functions in file)

| Function | File:Line | Summary |
|----------|-----------|---------|
| `InitFirstExtent` | `source_code/GenLib/SharedMemory.cpp:L288-L508` | Father-only init: remove old shm (if exists), create new first segment, initialize master + first extent, init `ConnTab` (`source_code/GenLib/SharedMemory.cpp:L321-L368`, `source_code/GenLib/SharedMemory.cpp:L403-L507`). |
| `AddNextExtent` (static) | `source_code/GenLib/SharedMemory.cpp:L518-L701` | Allocate next extent via `IPC_PRIVATE`, link it from previous extent header, extend `ExtentChain` (`source_code/GenLib/SharedMemory.cpp:L562-L693`). |
| `KillAllExtents` | `source_code/GenLib/SharedMemory.cpp:L711-L860` | Detach + `IPC_RMID` all extents (Father-only shutdown) (`source_code/GenLib/SharedMemory.cpp:L760-L815`). |
| `OpenFirstExtent` | `source_code/GenLib/SharedMemory.cpp:L869-L990` | Attach to first extent using fixed key, init `ConnTab` + `ExtentChain` for current process (`source_code/GenLib/SharedMemory.cpp:L898-L939`, `source_code/GenLib/SharedMemory.cpp:L944-L989`). |
| `AttachAllExtents` (static) | `source_code/GenLib/SharedMemory.cpp:L999-L1113` | If new extents exist (extent_count increased), attach them using the `next_extent_id` links (`source_code/GenLib/SharedMemory.cpp:L1053-L1106`). |
| `DetachAllExtents` | `source_code/GenLib/SharedMemory.cpp:L1122-L1229` | Detach all currently attached extents, reset globals, error on detach failures (`source_code/GenLib/SharedMemory.cpp:L1162-L1221`). |
| `AttachShMem` (static) | `source_code/GenLib/SharedMemory.cpp:L1238-L1281` | Wrapper for `shmat(shm_id)` with error logging (`source_code/GenLib/SharedMemory.cpp:L1255-L1273`). |
| `CreateTable` | `source_code/GenLib/SharedMemory.cpp:L1291-L1491` | Father-only: allocate `TABLE_HEADER` for a named table (from `TableTab[]` schema), link into master list, then `OpenTable()` (`source_code/GenLib/SharedMemory.cpp:L1325-L1338`, `source_code/GenLib/SharedMemory.cpp:L1384-L1490`). |
| `OpenTable` | `source_code/GenLib/SharedMemory.cpp:L1501-L1633` | Child: find table header by name, allocate a `ConnTab` slot, set `curr_row=first_row` (`source_code/GenLib/SharedMemory.cpp:L1555-L1586`, `source_code/GenLib/SharedMemory.cpp:L1592-L1632`). |
| `CloseTable` | `source_code/GenLib/SharedMemory.cpp:L1642-L1690` | Release a connection slot in `ConnTab` (`source_code/GenLib/SharedMemory.cpp:L1671-L1689`). |
| `AddItem` | `source_code/GenLib/SharedMemory.cpp:L1700-L1928` | Insert new row: allocate from free list or free extent; append to row chain; copy payload into row_data (`source_code/GenLib/SharedMemory.cpp:L1762-L1776`, `source_code/GenLib/SharedMemory.cpp:L1914-L1921`). |
| `DeleteItem` | `source_code/GenLib/SharedMemory.cpp:L1938-L2150` | Find row by comparator (`row_comp`) then move it to free list; update counts (`source_code/GenLib/SharedMemory.cpp:L1996-L2031`, `source_code/GenLib/SharedMemory.cpp:L2096-L2141`). |
| `DeleteTable` | `source_code/GenLib/SharedMemory.cpp:L2160-L2284` | Truncate table: move all rows to free chain, reset row list, adjust counts (`source_code/GenLib/SharedMemory.cpp:L2212-L2276`). |
| `ListTables` | `source_code/GenLib/SharedMemory.cpp:L2293-L2320` | Debug print table names and record/free counts. |
| `ParmComp` | `source_code/GenLib/SharedMemory.cpp:L2329-L2350` | Comparator for `PARM_DATA` by `par_key`. |
| `ProcComp` | `source_code/GenLib/SharedMemory.cpp:L2359-L2378` | Comparator for `PROC_DATA` by `pid`. |
| `UpdtComp` | `source_code/GenLib/SharedMemory.cpp:L2387-L2408` | Comparator for `UPDT_DATA` by `table_name`. |
| `ActItems` | `source_code/GenLib/SharedMemory.cpp:L2417-L2502` | Iterate rows calling callback; callback returns `OPER_FORWARD/STOP/BACKWARD` (`source_code/GenLib/SharedMemory.cpp:L2464-L2493`). |
| `SetRecordSize` | `source_code/GenLib/SharedMemory.cpp:L2511-L2600` | Adjust table record size (inflates to `ceil(data_size*2.5)` or truncates) (`source_code/GenLib/SharedMemory.cpp:L2562-L2586`). |
| `RewindTable` | `source_code/GenLib/SharedMemory.cpp:L2609-L2658` | Set `curr_row = first_row`. |
| `GetItem` | `source_code/GenLib/SharedMemory.cpp:L2668-L2712` | Copy current row’s data into caller buffer, then `OPER_STOP`. |
| `SeqFindItem` | `source_code/GenLib/SharedMemory.cpp:L2724-L2764` | NIU sequential range search using `memcmp` on key bounds (`source_code/GenLib/SharedMemory.cpp:L2752-L2761`). |
| `SetItem` | `source_code/GenLib/SharedMemory.cpp:L2774-L2818` | Copy caller buffer into current row’s data, then `OPER_STOP`. |
| `GetProc` | `source_code/GenLib/SharedMemory.cpp:L2829-L2878` | Find process row by `pid` while iterating; copies row into provided `PROC_DATA`. |
| `get_sys_status` | `source_code/GenLib/SharedMemory.cpp:L2887-L2912` | Read single `STAT_DATA` record from `STAT_TABLE` (`source_code/GenLib/SharedMemory.cpp:L2898-L2911`). |
| `AddCurrProc` | `source_code/GenLib/SharedMemory.cpp:L2921-L3022` | Append current process record into `PROC_TABLE` (sets pid/ppid/time, named_pipe, log_file, system/subsystem) (`source_code/GenLib/SharedMemory.cpp:L2959-L3012`). |
| `UpdateTimeStat` | `source_code/GenLib/SharedMemory.cpp:L3030-L3071` | Update `TSTT_TABLE`/`DSTT_TABLE` counters using `IncrementCounters`/`IncrementDoctorCounters` callbacks (`source_code/GenLib/SharedMemory.cpp:L3054-L3066`). |
| `IncrementCounters` | `source_code/GenLib/SharedMemory.cpp:L3080-L3347` | Per-message rolling window counts + timing stats update (pharmacy). |
| `IncrementDoctorCounters` | `source_code/GenLib/SharedMemory.cpp:L3356-L3621` | Per-message rolling window counts + timing stats update (doctor). |
| `RescaleTimeStat` | `source_code/GenLib/SharedMemory.cpp:L3630-L3654` | Rescale statistics by “clearing” stale buckets. |
| `RescaleCounters` | `source_code/GenLib/SharedMemory.cpp:L3663-L3776` | Rescale pharmacy stats buckets. |
| `RescaleDoctorCounters` | `source_code/GenLib/SharedMemory.cpp:L3785-L3888` | Rescale doctor stats buckets. |
| `GetMessageIndex` | `source_code/GenLib/SharedMemory.cpp:L3896-L3922` | Map message number to stats index via `PharmMessages`/`DoctorMessages` arrays (`source_code/GenLib/SharedMemory.cpp:L3903-L3921`). |
| `IncrementRevision` | `source_code/GenLib/SharedMemory.cpp:L3931-L3960` | Increment `STAT_DATA.param_rev` (`source_code/GenLib/SharedMemory.cpp:L3946-L3959`). |
| `UpdateShmParams` | `source_code/GenLib/SharedMemory.cpp:L3969-L4014` | Periodic parameter reload gate using `STAT_TABLE.param_rev` vs local `ParametersRevision` (`source_code/GenLib/SharedMemory.cpp:L3997-L4013`). |
| `GetFreeSqlServer` | `source_code/GenLib/SharedMemory.cpp:L4024-L4056` | Find and lock an available SqlServer process via `LockSqlServer` callback (`source_code/GenLib/SharedMemory.cpp:L4038-L4052`). |
| `LockSqlServer` | `source_code/GenLib/SharedMemory.cpp:L4065-L4087` | In `PROC_TABLE`, choose matching `proc_type` with `busy_flg==MAC_FALS`, then set busy flag (`source_code/GenLib/SharedMemory.cpp:L4076-L4086`). |
| `SetFreeSqlServer` | `source_code/GenLib/SharedMemory.cpp:L4096-L4114` | Free a previously locked SqlServer pid via `ReleaseSqlServer`. |
| `ReleaseSqlServer` | `source_code/GenLib/SharedMemory.cpp:L4123-L4143` | Clear `busy_flg` for matching pid. |
| `ShmGetParamsByName` | `source_code/GenLib/SharedMemory.cpp:L4153-L4255` | Load params from `PARM_TABLE` by names `CurrProgName.<param>` and `ALL.<param>`, supports reload gating; warns on missing params (`source_code/GenLib/SharedMemory.cpp:L4171-L4251`). |
| `GetPrescriptionId` | `source_code/GenLib/SharedMemory.cpp:L4266-L4299` | Read & increment prescription id from a dedicated row (`source_code/GenLib/SharedMemory.cpp:L4292-L4298`). |
| `GetMessageRecId` | `source_code/GenLib/SharedMemory.cpp:L4310-L4343` | Read & increment message record id from a dedicated row (`source_code/GenLib/SharedMemory.cpp:L4336-L4341`). |
| `DiprComp` | `source_code/GenLib/SharedMemory.cpp:L4354-L4386` | Comparator for “died processes” by `{pid,status}` (`source_code/GenLib/SharedMemory.cpp:L4373-L4385`). |
| `GetSonsCount` | `source_code/GenLib/SharedMemory.cpp:L4397-L4430` | Read `STAT_DATA.sons_count` from `STAT_TABLE` (`source_code/GenLib/SharedMemory.cpp:L4414-L4428`). |
| `AddToSonsCount` | `source_code/GenLib/SharedMemory.cpp:L4439-L4486` | Increment `STAT_DATA.sons_count` with shm lock (`WaitForResource`) (`source_code/GenLib/SharedMemory.cpp:L4449-L4477`). |
| `GetTabNumByName` | `source_code/GenLib/SharedMemory.cpp:L4495-L4514` | Find table schema index in `TableTab[]` by name (`source_code/GenLib/SharedMemory.cpp:L4501-L4513`). |
| `UpdateLastAccessTime` | `source_code/GenLib/SharedMemory.cpp:L4525-L4543` | Update current process record’s last-access timestamp (`source_code/GenLib/SharedMemory.cpp:L4531-L4541`). |
| `UpdateLastAccessTimeInternal` | `source_code/GenLib/SharedMemory.cpp:L4552-L4603` | Callback that matches current `pid` and writes `l_*` fields from `localtime()` (`source_code/GenLib/SharedMemory.cpp:L4580-L4599`). |
| `UpdateTable` | `source_code/GenLib/SharedMemory.cpp:L4612-L4655` | Update `UPDT_DATA` record by table name (date/time). |
| `GetTable` | `source_code/GenLib/SharedMemory.cpp:L4664-L4707` | Get `UPDT_DATA` record by table name (date/time). |
| `set_sys_status` | `source_code/GenLib/SharedMemory.cpp:L4716-L4774` | Update `STAT_DATA.status` and subsystem statuses under shm lock (`source_code/GenLib/SharedMemory.cpp:L4731-L4774`). |

#### Key behaviors (with citations)

- **Global shm lock**: many operations take the semaphore lock via `WaitForResource()` and release via `ReleaseResource()` (e.g., `InitFirstExtent` (`source_code/GenLib/SharedMemory.cpp:L312-L313`, `source_code/GenLib/SharedMemory.cpp:L506-L507`) and `AddItem` (`source_code/GenLib/SharedMemory.cpp:L1738-L1741`, `source_code/GenLib/SharedMemory.cpp:L1926-L1927`)).
- **Extent strategy**: first extent uses fixed key; later extents use `IPC_PRIVATE` and are linked via `next_extent_id` (`source_code/GenLib/SharedMemory.cpp:L321-L357`, `source_code/GenLib/SharedMemory.cpp:L562-L666`).
- **Table schema is external**: table definitions come from `TableTab[]` (`source_code/GenLib/SharedMemory.cpp:L96-L97`), and `CreateTable()` resolves `table_no` via `GetTabNumByName()` (`source_code/GenLib/SharedMemory.cpp:L1325-L1334`).

#### Security / robustness notes (code-level)

- **Permissions**: `ShmMode = 0777` makes segments broadly accessible (`source_code/GenLib/SharedMemory.cpp:L75-L77`). Combined with IPC keys, this is sensitive in multi-tenant environments.


