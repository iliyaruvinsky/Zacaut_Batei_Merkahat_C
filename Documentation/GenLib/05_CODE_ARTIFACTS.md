# GenLib - Code Artifacts

**Component**: GenLib (libgenlib.a)
**Task ID**: DOC-GENLIB-001
**Generated**: 2026-03-16
**Source**: `source_code/GenLib/`

---

## 1. Complete Function Catalog

### 1.1 Memory.c -- 31 Functions

#### Memory Allocation Wrappers

| Function | Line | Signature | Return | Description |
|----------|-----:|-----------|--------|-------------|
| `MemAllocExit` | 57 | `void *MemAllocExit(char *source_name, int line_no, size_t size, char *Memfor)` | `void*` or exits | `malloc()` wrapper; calls `GerrLogExit` on failure |
| `MemReallocExit` | 88 | `void *MemReallocExit(char *source_name, int line_no, void *old_ptr, size_t new_size, char *mem_for)` | `void*` | `realloc()` wrapper; logs on failure |
| `MemAllocReturn` | 121 | `void *MemAllocReturn(char *source_name, int line_no, size_t size, char *mem_for)` | `void*` or NULL | `malloc()` wrapper; logs and returns NULL on failure |
| `MemReallocReturn` | 151 | `void *MemReallocReturn(char *source_name, int line_no, void *old_ptr, size_t new_size, char *mem_for)` | `void*` or NULL | `realloc()` wrapper; logs and returns NULL on failure |
| `MemFree` | 181 | `void MemFree(void *ptr)` | void | `free()` wrapper; logs if ptr is NULL (`Memory.c:L187`) |

#### String and Buffer Utilities

| Function | Line | Signature | Return | Description |
|----------|-----:|-----------|--------|-------------|
| `ListMatch` | 200 | `int ListMatch(char *list[], char *str)` | int (index or -1) | Exact string match in NULL-terminated list |
| `ListNMatch` | 251 | `int ListNMatch(char *list[], char *str)` | int (index or -1) | Prefix match in list using `strncmp` |
| `StringToupper` | 302 | `void StringToupper(char *str)` | void | In-place uppercase conversion |
| `BufConvert` | 322 | `void BufConvert(char *buf, int len)` | void | Byte-reversal of buffer |
| `GetCurrProgName` | 353 | `void GetCurrProgName(char *pathname, char *progname)` | void | Extracts filename from path using `strrchr('/')` |

#### Environment Configuration

| Function | Line | Signature | Return | Description |
|----------|-----:|-----------|--------|-------------|
| `InitEnv` | 389 | `int InitEnv(char *source, int line)` | int (MAC_OK or error) | Allocate environment hash buffer arrays (initial size 20) |
| `HashEnv` | 441 | `int HashEnv(char *source, int line)` | int (MAC_OK or ENV_PAR_MISS) | Iterate `EnvTabPar`, read `getenv()`, cache in hash |

#### Process Lifecycle

| Function | Line | Signature | Return | Description |
|----------|-----:|-----------|--------|-------------|
| `InitSonProcess` | 552 | `int InitSonProcess(char *proc_pathname, int proc_type, int retrys, int eicon_port, short int system)` | int (MAC_OK or error) | 13-step child process bootstrap |
| `ExitSonProcess` | 625 | `int ExitSonProcess(int status)` | does not return | Clean shutdown: dispose, detach, close, exit |

#### Broadcast and Alarm

| Function | Line | Signature | Return | Description |
|----------|-----:|-----------|--------|-------------|
| `BroadcastSonProcs` | 664 | `int BroadcastSonProcs(char *buffer, int buf_size, int msg_type, int proc_flg)` | int | Broadcast message to all children via socket IPC |
| `alarm_handler` | 830 | `void alarm_handler(int signo)` | void | No-op SIGALRM handler for connect timeout |

#### Command-Line and Lookup

| Function | Line | Signature | Return | Description |
|----------|-----:|-----------|--------|-------------|
| `GetCommonPars` | 848 | `int GetCommonPars(int argc, char **argv, int *retrys)` | int | Parse `-r` retries from command line |
| `GetFatherNamedPipe` | 910 | `int GetFatherNamedPipe(char *pipe_name)` | int | Look up Father's named pipe from PROC_TABLE |
| `TssCanGoDown` | 957 | `int TssCanGoDown(short int system)` | int | Check if subsystem can shut down (count active workers) |

#### Process Spawning

| Function | Line | Signature | Return | Description |
|----------|-----:|-----------|--------|-------------|
| `Run_server` | 1023 | `int Run_server(PROC_DATA *proc_data)` | int (MAC_OK or error) | Fork+exec child with arguments from shm params |

#### System Utilities

| Function | Line | Signature | Return | Description |
|----------|-----:|-----------|--------|-------------|
| `GetSystemLoad` | 1261 | `int GetSystemLoad(void)` | int (percentage) | Run `sar` and parse CPU load |
| `my_nap` | 1309 | `void my_nap(int milliseconds)` | void | Sleep using `select()` on empty fd_set |
| `run_cmd` | 1331 | `int run_cmd(char *command, char *output, int max_len)` | int | Execute command via `popen()` with 5-second alarm |
| `GetWord` | 1403 | `char *GetWord(char *str, char *word)` | char* (next position) | Extract next whitespace-delimited word |
| `ToGoDown` | 1433 | `int ToGoDown(short int system)` | int | Check if system/subsystem is GOING_DOWN |

#### Hebrew Encoding

| Function | Line | Signature | Return | Description |
|----------|-----:|-----------|--------|-------------|
| `buf_reverse` | 1471 | `void buf_reverse(char *buf, int len)` | void | Reverse bytes in buffer |
| `fix_char` | 1493 | `void fix_char(char *ch)` | void | Swap `(` and `)` for RTL |
| `EncodeHebrew` | 1526 | `int EncodeHebrew(char *in_buf, int in_len, char *out_buf, int *out_len, char src_enc, char dst_enc)` | int | Encoding dispatcher (added 26Nov2025) |
| `WinHebToDosHeb` | 1630 | `int WinHebToDosHeb(char *in_buf, int in_len, char *out_buf, int *out_len)` | int | Win-1255 to DOS-862 |
| `WinHebToUTF8` | 1735 | `int WinHebToUTF8(char *in_buf, int in_len, char **out_buf, int *out_len)` | int | Win-1255 to UTF-8 (dynamic allocation) |

#### Date Arithmetic

| Function | Line | Signature | Return | Description |
|----------|-----:|-----------|--------|-------------|
| `GetMonsDif` | 1917 | `int GetMonsDif(int date1, int date2)` | int (months) | Months difference between YYYYMMDD dates |
| `GetDaysDiff` | 1969 | `int GetDaysDiff(int date1, int date2)` | int (days) | Days difference using `mktime()` |
| `AddDays` | 2008 | `int AddDays(int date, int days)` | int (YYYYMMDD) | Add N days to date |
| `AddMonths` | 2043 | `int AddMonths(int date, int months)` | int (YYYYMMDD) | Add N months with correction |
| `AddSeconds` | 2104 | `int AddSeconds(int time_val, int seconds)` | int (HHMMSS) | Add N seconds to time |
| `ConvertUnitAmount` | 2134 | `double ConvertUnitAmount(double amount, char *from_unit, char *to_unit)` | double | Convert medication units |
| `ParseISODateTime` | 2168 | `int ParseISODateTime(char *iso_str, int *date, int *time_val)` | int | Parse ISO 8601 to YYYYMMDD+HHMMSS |

---

### 1.2 Sockets.c -- 17 Functions

| Function | Line | Signature | Return | Description |
|----------|-----:|-----------|--------|-------------|
| `ListenSocketNamed` | 89 | `int ListenSocketNamed(char *socket_name)` | int (socket_num or error) | Create+bind+listen AF_UNIX socket |
| `ConnectSocketNamed` | 217 | `int ConnectSocketNamed(char *socket_name)` | int (socket_num or error) | Connect to AF_UNIX path |
| `ListenSocketUnnamed` | 318 | `int ListenSocketUnnamed(int port)` | int (socket_num or error) | Create+bind+listen AF_INET TCP |
| `ConnectSocketUnnamed` | 489 | `int ConnectSocketUnnamed(char *host, int port)` | int (socket_num or error) | Connect to AF_INET host:port |
| `AcceptConnection` | 583 | `int AcceptConnection(int listen_sock, struct sockaddr *addr, int *addrlen)` | int (accepted socket or error) | Wrap `accept()` |
| `CloseSocket` | 634 | `void CloseSocket(int sock)` | void | Close socket (guard fd < 3) |
| `DisposeSockets` | 656 | `void DisposeSockets(void)` | void | `unlink()` named pipe file |
| `ReadSocket` | 686 | `int ReadSocket(int sock, char *buf, int max_len, int *type, int *size, int verbose)` | int | Read framed message |
| `WriteSocket` | 820 | `int WriteSocket(int sock, char *buf, int len, int type)` | int | Write framed message |
| `WriteSocketHead` | 870 | `int WriteSocketHead(int sock, char *buf, int len, int type, char status)` | int | Write with status byte |
| `read_socket_data` | 948 | `int read_socket_data(int sock, char *buf, int len)` | int | Low-level read with select timeout |
| `write_socket_data` | 1078 | `int write_socket_data(int sock, char *buf, int len)` | int | Low-level write with select timeout |
| `get_header_details` | 1196 | `static int get_header_details(int sock, int *type, int *len)` | int | Parse MESSAGE_HEADER |
| `set_header_details` | 1248 | `int set_header_details(int sock, int type, int len)` | int | Format+send MESSAGE_HEADER |
| `GetPeerAddr` | 1307 | `char *GetPeerAddr(int sock)` | char* | Dotted-decimal peer address |
| `GetCurrNamedPipe` | 1376 | `void GetCurrNamedPipe(char *buffer)` | void | Build `{NamedpDir}/{prefix}{pid}_{time}` |
| `GetSocketMessage` | 1396 | `int GetSocketMessage(int micro_sec, char *buffer, int max_len, int *size, int close_flg)` | int | Select-based multiplexed receive |
| `GetInterProcMesg` | 1516 | `int GetInterProcMesg(char *in_buf, int *length, char *out_buf)` | int | Decode FILE_MESG/DATA_MESG |
| `SetInterProcMesg` | 1622 | `int SetInterProcMesg(char *in_buf, int length, char mesg_type, char *out_buf, int *out_len)` | int | Encode FILE_MESG/DATA_MESG |

---

### 1.3 SharedMemory.cpp -- 46 Functions

#### Extent Management

| Function | Line | Signature | Return | Description |
|----------|-----:|-----------|--------|-------------|
| `InitFirstExtent` | 288 | `int InitFirstExtent(int shm_size)` | int | Create first shm extent (Father only) |
| `AddNextExtent` | 518 | `static int AddNextExtent(int size)` | int | Allocate additional extent |
| `KillAllExtents` | 711 | `int KillAllExtents(void)` | int | Destroy all extents (Father shutdown) |
| `OpenFirstExtent` | 869 | `int OpenFirstExtent(void)` | int | Attach to existing shm (child) |
| `AttachAllExtents` | 999 | `static int AttachAllExtents(void)` | int | Attach newly-allocated extents |
| `DetachAllExtents` | 1122 | `int DetachAllExtents(void)` | int | Detach all attached extents |
| `AttachShMem` | 1238 | `static char *AttachShMem(int shm_id)` | char* | Low-level shmat wrapper |

#### Table CRUD

| Function | Line | Signature | Return | Description |
|----------|-----:|-----------|--------|-------------|
| `CreateTable` | 1291 | `int CreateTable(char *table_name, TABLE **tablep)` | int | Create table in shm (Father) |
| `OpenTable` | 1501 | `extern "C" int OpenTable(char *table_name, TABLE **tablep)` | int | Open table by name |
| `CloseTable` | 1642 | `int CloseTable(TABLE *tablep)` | int | Close table connection |
| `AddItem` | 1700 | `int AddItem(TABLE *tablep, void *row_data)` | int | Add row (reuse free or allocate) |
| `DeleteItem` | 1938 | `int DeleteItem(TABLE *tablep)` | int | Delete current row (move to free chain) |
| `DeleteTable` | 2160 | `int DeleteTable(TABLE *tablep)` | int | Bulk delete all rows |
| `ListTables` | 2293 | `void ListTables(void)` | void | Debug print all tables |

#### Row Operations

| Function | Line | Signature | Return | Description |
|----------|-----:|-----------|--------|-------------|
| `ParmComp` | 2329 | `int ParmComp(TABLE *tablep, OPER_ARGS args)` | int | Compare by par_key |
| `ProcComp` | 2359 | `int ProcComp(TABLE *tablep, OPER_ARGS args)` | int | Compare by pid |
| `UpdtComp` | 2387 | `int UpdtComp(TABLE *tablep, OPER_ARGS args)` | int | Compare by table_name |
| `ActItems` | 2417 | `int ActItems(TABLE *tablep, int rewind_flg, int (*func)(TABLE*, OPER_ARGS), OPER_ARGS args)` | int | Core iteration engine |
| `SetRecordSize` | 2511 | `int SetRecordSize(TABLE *tablep, int new_size)` | int | Resize record (2.5x inflation) |
| `RewindTable` | 2609 | `int RewindTable(TABLE *tablep)` | int | Reset cursor to first row |
| `GetItem` | 2668 | `int GetItem(TABLE *tablep, OPER_ARGS args)` | int | Copy current row to buffer |
| `SeqFindItem` | 2724 | `int SeqFindItem(TABLE *tablep, OPER_ARGS args)` | int | Sequential key-range search |
| `SetItem` | 2774 | `int SetItem(TABLE *tablep, OPER_ARGS args)` | int | Overwrite current row |
| `GetProc` | 2829 | `int GetProc(TABLE *tablep, OPER_ARGS args)` | int | Find PROC_DATA by pid |
| `get_sys_status` | 2887 | `int get_sys_status(short int system)` | int | Read status from STAT_TABLE |

#### Process Registration and Statistics

| Function | Line | Signature | Return | Description |
|----------|-----:|-----------|--------|-------------|
| `AddCurrProc` | 2921 | `int AddCurrProc(int retrys, int proc_type, int eicon_port, short int system)` | int | Register in PROC_TABLE |
| `UpdateTimeStat` | 3030 | `int UpdateTimeStat(int msg_num, int elapsed, short int system)` | int | Update time statistics |
| `IncrementCounters` | 3080 | `int IncrementCounters(TABLE *tablep, OPER_ARGS args)` | int | Increment pharmacy counters |
| `IncrementDoctorCounters` | 3356 | `int IncrementDoctorCounters(TABLE *tablep, OPER_ARGS args)` | int | Increment doctor counters |
| `RescaleTimeStat` | 3630 | `int RescaleTimeStat(short int system)` | int | Zero stale time stats |
| `RescaleCounters` | 3663 | `int RescaleCounters(TABLE *tablep, OPER_ARGS args)` | int | Rescale pharmacy stats |
| `RescaleDoctorCounters` | 3785 | `int RescaleDoctorCounters(TABLE *tablep, OPER_ARGS args)` | int | Rescale doctor stats |
| `GetMessageIndex` | 3896 | `int GetMessageIndex(int msg_num, short int system)` | int | Map message to array index |

#### Parameter and SQL Server Management

| Function | Line | Signature | Return | Description |
|----------|-----:|-----------|--------|-------------|
| `IncrementRevision` | 3931 | `int IncrementRevision(TABLE *tablep, OPER_ARGS args)` | int | Increment param_rev |
| `UpdateShmParams` | 3969 | `int UpdateShmParams(void)` | int | Reload params if revision changed |
| `GetFreeSqlServer` | 4024 | `int GetFreeSqlServer(int proc_type, char *pipe_name, int *pid)` | int | Find+lock idle SQL server |
| `LockSqlServer` | 4065 | `int LockSqlServer(TABLE *tablep, OPER_ARGS args)` | int | Atomically lock free server |
| `SetFreeSqlServer` | 4096 | `int SetFreeSqlServer(int pid)` | int | Release locked SQL server |
| `ReleaseSqlServer` | 4123 | `int ReleaseSqlServer(TABLE *tablep, OPER_ARGS args)` | int | Clear busy_flg by pid |
| `ShmGetParamsByName` | 4153 | `int ShmGetParamsByName(ParamTab *params, int mode)` | int | Load params from PARM_TABLE |

#### Miscellaneous Operations

| Function | Line | Signature | Return | Description |
|----------|-----:|-----------|--------|-------------|
| `GetPrescriptionId` | 4266 | `int GetPrescriptionId(int *id)` | int | Atomic read+increment prescription ID |
| `GetMessageRecId` | 4310 | `int GetMessageRecId(int *id)` | int | Atomic read+increment message rec_id |
| `DiprComp` | 4354 | `int DiprComp(TABLE *tablep, OPER_ARGS args)` | int | Compare DIPR by pid+status |
| `GetSonsCount` | 4397 | `int GetSonsCount(int *count)` | int | Read sons_count from STAT_TABLE |
| `AddToSonsCount` | 4439 | `int AddToSonsCount(int delta)` | int | Atomic add to sons_count |
| `GetTabNumByName` | 4495 | `int GetTabNumByName(char *name)` | int | Lookup table index in TableTab[] |
| `UpdateLastAccessTime` | 4525 | `int UpdateLastAccessTime(void)` | int | Update process heartbeat |
| `UpdateLastAccessTimeInternal` | 4552 | `int UpdateLastAccessTimeInternal(TABLE *tablep, OPER_ARGS args)` | int | Set timestamp for matching pid |
| `UpdateTable` | 4612 | `int UpdateTable(TABLE *tablep, OPER_ARGS args)` | int | Update UPDT_DATA row |
| `GetTable` | 4664 | `int GetTable(TABLE *tablep, OPER_ARGS args)` | int | Get UPDT_DATA row |
| `set_sys_status` | 4716 | `int set_sys_status(short int system, int status)` | int | Set system status in STAT_TABLE |

---

### 1.4 Semaphores.c -- 6 Functions

| Function | Line | Signature | Return | Description |
|----------|-----:|-----------|--------|-------------|
| `CreateSemaphore` | 109 | `int CreateSemaphore(void)` | int | Create semaphore (Father only); `semget` + `semctl(SETVAL, 1)` |
| `DeleteSemaphore` | 181 | `int DeleteSemaphore(void)` | int | Delete semaphore; `semctl(IPC_RMID)` |
| `OpenSemaphore` | 275 | `int OpenSemaphore(void)` | int | Open existing semaphore (child); `semget(IPC_CREAT)` |
| `CloseSemaphore` | 404 | `int CloseSemaphore(void)` | int | Close semaphore (sets `sem_num = -1`) |
| `WaitForResource` | 487 | `int WaitForResource(void)` | int | Acquire (re-entrant via `sem_busy_flg`) |
| `ReleaseResource` | 625 | `int ReleaseResource(void)` | int | Release (actual `semop` only when count reaches 0) |

---

### 1.5 GeneralError.c -- 13 Functions

| Function | Line | Signature | Return | Description |
|----------|-----:|-----------|--------|-------------|
| `GerrLogExit` | 92 | `void GerrLogExit(char *source, int line, int status, char *printf_fmt, ...)` | does not return | Log + `exit(status)` |
| `GerrLogReturn` | 165 | `void GerrLogReturn(char *source, int line, char *printf_fmt, ...)` | void | Log + 250ms sleep + return |
| `GerrLogMini` | 248 | `void GerrLogMini(char *source, int line, char *printf_fmt, ...)` | void | Log one-liner |
| `GerrLogAddBlankLines` | 288 | `void GerrLogAddBlankLines(int n)` | void | Write N blank lines |
| `GerrLogFnameMini` | 314 | `void GerrLogFnameMini(char *fname, char *source, int line, char *printf_fmt, ...)` | void | Log one-liner to file |
| `GerrLogFnameClear` | 355 | `void GerrLogFnameClear(char *fname)` | void | Truncate specified file |
| `GerrLogToFileName` | 382 | `void GerrLogToFileName(char *fname, char *source, int line, char *printf_fmt, ...)` | void | Log to file + 250ms sleep |
| `GmsgLogExit` | 445 | `void GmsgLogExit(char *source, int line, int status, char *title, char *printf_fmt, ...)` | does not return | Titled log + exit |
| `GmsgLogReturn` | 521 | `void GmsgLogReturn(char *source, int line, char *title, char *printf_fmt, ...)` | void | Titled log + return |
| `open_log_file` | 594 | `static FILE *open_log_file(void)` | FILE* | Open `{LogDir}/{LogFile}` or `stderr` |
| `close_log_file` | 670 | `static void close_log_file(FILE *fp)` | void | Flush+close (skip if `stderr`) |
| `open_other_file` | 695 | `static FILE *open_other_file(char *fname, char *mode)` | FILE* | Open `{LogDir}/{fname}`; mkdir if needed |
| `create_log_directory` | 746 | `int create_log_directory(char *filename_in)` | int | `mkdir -m777 -p` for log path |

---

### 1.6 GxxPersonality.c -- 1 Function

| Function | Line | Signature | Return | Description |
|----------|-----:|-----------|--------|-------------|
| `Prn` | 12 | `void Prn(char *frm, ...)` | void | Debug print: `{pid = %d, ppid = %d}, %s` |

The `__gxx_personality_v0()` stub is `#if 0`'d out at `GxxPersonality.c:L5-L10`.

---

## 2. Key Macros

According to code at `SharedMemory.cpp:L98-L278`:

| Macro | Location | Purpose |
|-------|----------|---------|
| `RETURN(a)` | `SharedMemory.cpp:L104` | `{ReleaseResource(); return(a);}` -- release semaphore before return |
| `VALIDITY_CHECK()` | `SharedMemory.cpp:L110-L114` | Return `SHM_NO_INIT` if `master_shm_header` is NULL |
| `MAKE_NULL_EXT_PTR(p)` | `SharedMemory.cpp:L120-L121` | Set `ext_no = -1` (null pointer) |
| `IS_NULL_EXT_PTR(p)` | `SharedMemory.cpp:L127-L128` | Test if `ext_no == -1` |
| `GET_ABS_ADDR(p)` | `SharedMemory.cpp:L134-L142` | Resolve `EXTENT_PTR` to absolute address via `ExtentChain[ext_no].start_addr + offset` |
| `CHECK_TAB_PTR(t)` | `SharedMemory.cpp:L148-L180` | Validate table connection handle (conn_id range, ConnTab match, sort_flg check) |
| `GET_FREE_EXTENT(eh,en,sz)` | `SharedMemory.cpp:L187-L229` | Find extent with free space or allocate new one |
| `ATTACH_ALL()` | `SharedMemory.cpp:L269-L278` | Attach newly-allocated extents; return error if failed |
| `MAXI(a,b)` | `SharedMemory.cpp:L98-L99` | Maximum of two values |
| `CONN_MAX` | `SharedMemory.cpp:L88` | 150 -- maximum concurrent table connections per process |
| `LOG_EVERYTHING` | `SharedMemory.cpp:L34` | 0 -- suppresses verbose diagnostics |

---

## 3. Key Data Structures

### 3.1 MASTER_SHM_HEADER

According to the research at `RESEARCH/GenLib_deepdive.md` (referencing `Global.h:L720-L795`), this structure resides at the start of the first shared memory extent:

| Field | Type | Purpose |
|-------|------|---------|
| `extent_count` | int | Number of allocated extents |
| `first_table` | EXTENT_PTR | Pointer to first table in chain |
| `last_table` | EXTENT_PTR | Pointer to last table in chain |
| `first_extent` | EXTENT_PTR | Pointer to first extent header |
| `free_extent` | EXTENT_PTR | Pointer to extent with free space |

### 3.2 EXTENT_HEADER

Per-extent metadata at the start of each shared memory segment:

| Field | Type | Purpose |
|-------|------|---------|
| `free_space` | int | Available bytes in this extent |
| `extent_size` | int | Total size of this extent |
| `free_addr_off` | int | Offset to next free address |
| `next_extent` | EXTENT_PTR | Next extent in chain |
| `back_extent` | EXTENT_PTR | Previous extent in chain |
| `next_extent_id` | int | System V shm id for next extent |

### 3.3 EXTENT_PTR

The shared-memory-safe pointer type, used instead of raw pointers (which would be invalid across processes):

| Field | Type | Purpose |
|-------|------|---------|
| `ext_no` | int | Extent number (-1 = null) |
| `offset` | int | Byte offset within extent |

### 3.4 TABLE_HEADER

Per-table metadata:

| Field | Type | Purpose |
|-------|------|---------|
| `table_data` | TABLE_DATA | Schema info (name, record_size, sort_flg) |
| `record_count` | int | Active row count |
| `free_count` | int | Free chain row count |
| `first_row` | EXTENT_PTR | First active row |
| `last_row` | EXTENT_PTR | Last active row |
| `first_free` | EXTENT_PTR | First free chain row |
| `last_free` | EXTENT_PTR | Last free chain row |
| `next_table` | EXTENT_PTR | Next table in chain |
| `back_table` | EXTENT_PTR | Previous table in chain |

### 3.5 TABLE (Connection Handle)

Local per-process connection handle returned by `OpenTable()`:

| Field | Type | Purpose |
|-------|------|---------|
| `conn_id` | int | Index into ConnTab[] |
| `busy_flg` | int | Whether connection is in use |
| `table_header` | EXTENT_PTR | Pointer to TABLE_HEADER in shm |
| `curr_row` | EXTENT_PTR | Current cursor position |
| `curr_addr` | TABLE* | Self-reference for validation |

### 3.6 EXTENT_ADDR (Local Tracking)

According to code at `SharedMemory.cpp:L62-L67`:
```c
typedef struct {
    int   shm_id;
    char  *start_addr;
} EXTENT_ADDR;
```
Used in the local `ExtentChain[]` array to map extent numbers to their shm ids and attached addresses.

### 3.7 MESG_HEADER (Private IPC)

According to code at `Sockets.c:L38-L43`:
```c
typedef struct {
    int   length;
    char  mesg_type;
} MESG_HEADER;
```
Used internally by `GetInterProcMesg()`/`SetInterProcMesg()` to prefix inter-process message payloads.

### 3.8 MESSAGE_HEADER (Wire Protocol)

Defined in `Global.h` [NEEDS_VERIFICATION: exact field sizes]. Contains ASCII-formatted `messageType` and `messageLen` fields parsed with `atoi()`.

---

## 4. Private Static Variables

### 4.1 SharedMemory.cpp

According to code at `SharedMemory.cpp:L60-L96`:

| Variable | Type | Initial Value | Purpose |
|----------|------|---------------|---------|
| `master_shm_header` | MASTER_SHM_HEADER* | NULL | Pointer to master header in first extent |
| `ExtentChain` | EXTENT_ADDR* | (allocated) | Local array mapping extent numbers to addresses |
| `ExtentAlloc` | int | (varies) | Allocated slots in ExtentChain |
| `ShmMode` | int | 0777 | Shared memory file mode |
| `ShmKey1` | sharedmem_key_type | (hard-coded) | Shared memory key (documented by location only) |
| `AttachedExtentCount` | int | 0 | Number of locally attached extents |
| `ParametersRevision` | int | 0 | Last known parameter revision |
| `ConnTab[CONN_MAX]` | TABLE[150] | (zeroed) | Table connection handles |
| `ConnCount` | int | 0 | Active connection count |

### 4.2 Semaphores.c

According to code at `Semaphores.c:L59-L97`:

| Variable | Type | Initial Value | Purpose |
|----------|------|---------------|---------|
| `SemMode` | int | 0777 | Semaphore file mode |
| `SemName1` | char* | "mac_sem1" | Semaphore key name |
| `SemKey1` | key_t | (hard-coded) | Semaphore key (documented by location only) |
| `sem_num` | int | -1 | Semaphore number (-1 = not open) |
| `sem_busy_flg` | short int | 0 | Re-entrant lock counter |
| `W, P, WNB, PNB` | struct sembuf | (see Semaphores.c:L86-L90) | Semaphore operation structs |

### 4.3 Memory.c

According to code at `Memory.c:L42-L48`:

| Variable | Type | Initial Value | Purpose |
|----------|------|---------------|---------|
| `EnvNames` | char** | NULL | Environment names hash buffer |
| `EnvValues` | char** | NULL | Environment values hash buffer |
| `EnvCount` | int | 0 | Cached variable count |
| `EnvSize` | int | 20 | Hash buffer capacity |

---

## 5. Build Configuration

According to `Makefile:L1-L55`:

**Target**: `libgenlib.a`

**Source Files**:
```
GeneralError.c  Memory.c  Semaphores.c  Sockets.c  SharedMemory.cpp  GxxPersonality.c
```

**Build Process**:
1. Each source file compiled to `.o` with `$(CC) $(COMP_OPTS)` (`Makefile:L40-L51`)
2. All `.o` files archived into `libgenlib.a` with `$(AR) r` (`Makefile:L37-L38`)

**External Includes**: `../Util/Defines.mak` (`Makefile:L4`) and `../Util/Rules.mak` (`Makefile:L27`). [NEEDS_VERIFICATION: actual CC, COMP_OPTS, AR values -- these files are not present in the repository]

**Clean Target**: `rm $(TARGET) $(OBJS)` (`Makefile:L52-L53`)

---

*Generated by CIDRA Documenter Agent - DOC-GENLIB-001*
