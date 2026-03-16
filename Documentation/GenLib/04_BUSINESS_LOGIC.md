# GenLib - Business Logic

**Component**: GenLib (libgenlib.a)
**Task ID**: DOC-GENLIB-001
**Generated**: 2026-03-16
**Source**: `source_code/GenLib/`

---

## 1. Shared Memory Table Engine

### 1.1 Table Creation (Father Only)

According to code at `SharedMemory.cpp:L1291-L1491`, `CreateTable()` performs the following:

1. Acquires the semaphore via `WaitForResource()`
2. Looks up the table schema in `TableTab[]` by name
3. Checks for duplicate table names by walking the existing table chain
4. Uses the `GET_FREE_EXTENT` macro to find an extent with sufficient free space (or allocate a new one)
5. Allocates a `TABLE_HEADER` from the free space in the extent
6. Links the new table into the master table chain (doubly-linked list anchored in `MASTER_SHM_HEADER`)
7. Calls `OpenTable()` to create and return a connection handle
8. Releases the semaphore via `RETURN()` macro

This function is intended to be called only by FatherProcess during system initialization, as indicated by `FatherProcess.c:L309-L334` which iterates `TableTab[]` creating each table.

### 1.2 Opening and Closing Tables

**OpenTable()** (`SharedMemory.cpp:L1501`):
According to the code, this function uses `extern "C"` linkage (`SharedMemory.cpp:L1501`) to maintain C compatibility. It:
1. Walks the table chain starting from `master_shm_header->first_table`
2. Compares table names using `strcmp()`
3. Allocates a `ConnTab[]` entry (up to `CONN_MAX=150` concurrent connections per process, per `SharedMemory.cpp:L88`)
4. Returns a `TABLE*` pointer that serves as a connection handle

**CloseTable()** (`SharedMemory.cpp:L1642`):
Releases the `ConnTab[]` entry, making the slot available for reuse.

### 1.3 Adding Rows (AddItem)

According to code at `SharedMemory.cpp:L1700-L1928`, `AddItem()`:

1. Acquires the semaphore
2. Checks the table's free chain for reusable rows
3. If a free row is available: removes it from the free chain (doubly-linked list removal) and reuses it
4. If no free rows: allocates new space from the extent
   - Record size is word-aligned to `sizeof(int)` boundary (`SharedMemory.cpp:L1770-L1776`)
   - If the current extent lacks space, the `GET_FREE_EXTENT` macro triggers `AddNextExtent()`
5. Appends the new row to the end of the table's row chain
6. Copies the caller's data into the row via `memcpy()`
7. Increments `record_count`
8. Releases the semaphore

### 1.4 Deleting Rows (DeleteItem)

According to code at `SharedMemory.cpp:L1938`, `DeleteItem()`:

1. Acquires the semaphore
2. Removes the current row from the table's active chain (doubly-linked list pointer updates)
3. Moves the row to the table's free chain (prepends to free list)
4. Decrements `record_count`, increments `free_count`
5. Releases the semaphore

### 1.5 Bulk Delete (DeleteTable)

According to code at `SharedMemory.cpp:L2160`, `DeleteTable()`:

Moves the entire active row chain to the free list in one operation, rather than deleting rows individually. This appears to be used when a table needs to be refreshed (e.g., reloading parameters from the database).

### 1.6 Row Iteration Engine (ActItems)

According to code at `SharedMemory.cpp:L2417-L2502`, `ActItems()` is the core iteration engine used throughout the system:

1. Acquires the semaphore
2. If `rewind_flg` is set, resets the cursor to the first row
3. Walks the row chain starting from the current cursor position
4. For each row: resolves the row data pointer via `GET_ABS_ADDR()`, then calls the operation function `func(tablep, args)`
5. The operation function's return value controls iteration:
   - `OPER_FORWARD` -- advance cursor to next row
   - `OPER_STOP` -- stop iteration immediately
   - `OPER_BACKWARD` -- move cursor to previous row
6. Returns `NO_MORE_ROWS` when the chain is exhausted

### 1.7 Row Comparators

The system uses specific comparator functions for each table type:

| Comparator | Table | Key Field | Code Reference |
|-----------|-------|-----------|---------------|
| `ParmComp` | PARM_TABLE | `par_key` | `SharedMemory.cpp:L2329` |
| `ProcComp` | PROC_TABLE | `pid` | `SharedMemory.cpp:L2359` |
| `UpdtComp` | UPDT_TABLE | `table_name` | `SharedMemory.cpp:L2387` |
| `DiprComp` | DIPR_TABLE | `pid`, then `status` | `SharedMemory.cpp:L4354` |

---

## 2. Process Table Management

### 2.1 Process Registration (AddCurrProc)

According to code at `SharedMemory.cpp:L2921`, `AddCurrProc()` registers the current process in PROC_TABLE by:

1. Opening PROC_TABLE
2. Populating a `PROC_DATA` struct with:
   - Current process PID via `getpid()`
   - Process name from `CurrProgName`
   - Process type (e.g., SQLPROC_TYPE, AS400TOUNIX_TYPE)
   - Subsystem (PHARM_SYS or DOCTOR_SYS)
   - Named pipe path from `CurrNamedPipe`
   - Retry count
   - Current timestamp
3. Calling `AddItem()` to insert the row
4. Closing the table

### 2.2 SQL Server Locking

The system implements a pool of SQL server processes that can be reserved by other components:

**GetFreeSqlServer()** (`SharedMemory.cpp:L4024`):
1. Opens PROC_TABLE
2. Iterates using `ActItems()` with `LockSqlServer` as the operation function
3. `LockSqlServer()` (`SharedMemory.cpp:L4065`): Atomically checks if a PROC_DATA row has the matching `proc_type` and `busy_flg == 0`, then sets `busy_flg = 1` and returns `OPER_STOP`
4. Returns the locked process's named pipe path and PID to the caller

**SetFreeSqlServer()** (`SharedMemory.cpp:L4096`):
1. Opens PROC_TABLE
2. Iterates using `ActItems()` with `ReleaseSqlServer` as the operation function
3. `ReleaseSqlServer()` (`SharedMemory.cpp:L4123`): Clears `busy_flg` for the process matching the specified PID

This mechanism appears to be used by components like As400UnixServer to delegate SQL work to an available SqlServer process.

### 2.3 Process Access Time Tracking

**UpdateLastAccessTime()** (`SharedMemory.cpp:L4525`):
Updates the current process's last-access timestamp in PROC_TABLE, serving as a heartbeat mechanism. Uses `localtime()` to break the current time into year/month/day/hour/minute/second fields.

---

## 3. Environment and Configuration

### 3.1 Environment Variable Hash Cache

According to code at `Memory.c:L389-L538`:

**InitEnv()** (`Memory.c:L389`): Allocates initial hash buffer arrays (`EnvNames` and `EnvValues`) with a default size of 20 entries using `MemAllocReturn()`.

**HashEnv()** (`Memory.c:L441`): Iterates the `EnvTabPar` array (an extern list of required environment variable names defined per-component). For each entry:
1. First checks the local hash cache using `ListMatch()`
2. If not cached, calls `getenv()` to read the value
3. Caches the result with `strdup()` into `EnvNames[]`/`EnvValues[]`
4. Grows the buffers with `MemReallocReturn()` if needed
5. Returns `ENV_PAR_MISS` if any required variable is missing

### 3.2 Shared Memory Parameters (ShmGetParamsByName)

According to code at `SharedMemory.cpp:L4153`, `ShmGetParamsByName()` loads or reloads parameters from PARM_TABLE into a `ParamTab` descriptor array. It supports four parameter types:
- `PAR_INT` -- integer (via `atoi()`)
- `PAR_LONG` -- long integer (via `atol()`)
- `PAR_DOUBLE` -- double (via `atof()`)
- `PAR_CHAR` -- string (via `strcpy()`)

The function can operate in two modes:
- `PAR_LOAD` -- initial load of parameters
- `PAR_RELOAD` -- reload parameters (used when the parameter revision changes)

### 3.3 Parameter Revision System

**IncrementRevision()** (`SharedMemory.cpp:L3931`): An operation function that increments the `param_rev` field in STAT_DATA.

**UpdateShmParams()** (`SharedMemory.cpp:L3969`): Checks whether the parameter revision in shared memory has changed since last check. If so, reloads parameters via `ShmGetParamsByName(SonProcParams, PAR_RELOAD)`. This check is throttled by `SharedMemUpdateInterval` to avoid excessive polling.

This mechanism allows FatherProcess to update parameters in shared memory and have all child processes pick up the changes on their next check cycle.

---

## 4. Process Statistics

### 4.1 Time Statistics (UpdateTimeStat)

According to code at `SharedMemory.cpp:L3030`, `UpdateTimeStat()` updates time-based processing statistics. It appears to operate on either TSTT_TABLE (pharmacy) or DSTT_TABLE (doctor) depending on the subsystem.

### 4.2 Counter Operations

**IncrementCounters()** (`SharedMemory.cpp:L3080`): An operation function that increments pharmacy statistics counters in TSTT_DATA, tracking message processing times and counts.

**IncrementDoctorCounters()** (`SharedMemory.cpp:L3356`): Same pattern for doctor statistics in DSTT_DATA.

### 4.3 Rescaling

**RescaleTimeStat()** (`SharedMemory.cpp:L3630`): Zeroes stale time statistics entries.

**RescaleCounters()** (`SharedMemory.cpp:L3663`) and **RescaleDoctorCounters()** (`SharedMemory.cpp:L3785`): Rescale the respective statistics, which appears to involve resetting counters that have become stale.

### 4.4 Message Indexing

**GetMessageIndex()** (`SharedMemory.cpp:L3896`): Maps a message number to an array index, used when updating statistics for specific message types. The mapping references `PcMessages[]`, `PharmMessages[]`, and `DoctorMessages[]` arrays defined in GenSql.

---

## 5. System Status Management

### 5.1 Reading Status

**get_sys_status()** (`SharedMemory.cpp:L2887`): Reads the system status from STAT_TABLE. Returns status values such as GOING_UP, SYSTEM_UP, GOING_DOWN, SYSTEM_DOWN.

**GetSonsCount()** (`SharedMemory.cpp:L4397`): Reads the `sons_count` field from STAT_TABLE, which tracks the number of active child processes.

### 5.2 Writing Status

**set_sys_status()** (`SharedMemory.cpp:L4716`): Sets the system or subsystem status in STAT_TABLE. Acquires the semaphore, reads the current STAT_DATA, modifies the appropriate status field, and writes it back.

**AddToSonsCount()** (`SharedMemory.cpp:L4439`): Atomically adds to `sons_count` with semaphore protection. Called with `+1` during `InitSonProcess()` and with `-1` by FatherProcess when a child dies (`FatherProcess.c:L787`).

### 5.3 Shutdown Checking

**ToGoDown()** (`Memory.c:L1433`): Checks if the system or subsystem status is GOING_DOWN by calling `get_sys_status()`. Uses `GoDownCheckInterval` as a throttle to avoid checking too frequently.

**TssCanGoDown()** (`Memory.c:L957`): Checks if a subsystem can safely shut down by counting active TSX25WORKPROC_TYPE workers in PROC_TABLE. According to the chunker notes, TSX25WORKPROC_TYPE is obsolete X.25 code.

---

## 6. Atomic ID Generation

### 6.1 Prescription IDs

**GetPrescriptionId()** (`SharedMemory.cpp:L4266`): Atomically reads and increments a prescription ID counter stored in PRSC_TABLE. Uses semaphore protection to ensure no two processes receive the same ID.

### 6.2 Message Record IDs

**GetMessageRecId()** (`SharedMemory.cpp:L4310`): Atomically reads and increments a message record ID counter stored in MSG_RECID_TABLE. Same atomic pattern as prescription IDs.

---

## 7. Broadcast System

### 7.1 BroadcastSonProcs

According to code at `Memory.c:L664-L839`, the broadcast function:

1. Opens PROC_TABLE and iterates all registered processes
2. Filters by `proc_flg` parameter to select which types of processes receive the message:
   - `FIRST_SQL` -- appears to target SQL server processes only
   - `NOT_CONTROL` -- appears to target non-control processes
   - `JUST_CONTROL` -- appears to target control processes only
3. For each selected process:
   a. Sets a SIGALRM for 1 second (using `alarm_handler` at `Memory.c:L830`)
   b. Connects to the child's named pipe via `ConnectSocketNamed()`
   c. Sends the message via `WriteSocket()`
   d. Closes the connection
4. The alarm handler is a no-op -- its purpose is to break out of a blocking `connect()` call if a child is unresponsive

The broadcast message payload appears to reference `PcMessages[]` at `Memory.c:L797-L798`. [NEEDS_VERIFICATION: exact content of PcMessages array]

---

## 8. Hebrew Encoding

### 8.1 EncodeHebrew Dispatcher

According to code at `Memory.c:L1526`, `EncodeHebrew()` was added on 26Nov2025 by DonR (User Story #251264). It serves as a flexible encoding dispatcher:
- Source encoding 'W' (Windows-1255) to destination 'D' (DOS): delegates to `WinHebToDosHeb()`
- Source encoding 'W' to destination 'U' (UTF-8): delegates to `WinHebToUTF8()` with buffer size management

### 8.2 WinHebToDosHeb

According to code at `Memory.c:L1630`, this function converts Windows-1255 Hebrew text to DOS code page 862 by:
1. Reversing Hebrew text segments (for RTL display)
2. Re-reversing embedded numbers within the reversed text
3. Subtracting 96 from byte values in the Hebrew character range
4. Using `fix_char()` to swap parentheses for RTL text

### 8.3 WinHebToUTF8

According to code at `Memory.c:L1735`, this function converts Windows-1255 to UTF-8 using:
- A static 128-entry translation table initialized on first call (lazy initialization)
- Dynamic output buffer allocation via `realloc()` since UTF-8 characters may require more bytes than the input

---

## 9. Date and Unit Utilities

### 9.1 Date Arithmetic

According to code at `Memory.c:L1909-L2196`, the date functions operate on YYYYMMDD integer date format:

- `GetMonsDif()` (`Memory.c:L1917`): Calculates months difference with leap-year awareness
- `GetDaysDiff()` (`Memory.c:L1969`): Calculates days difference using `mktime()`
- `AddDays()` (`Memory.c:L2008`): Adds N days to a date
- `AddMonths()` (`Memory.c:L2043`): Adds N months with month-length correction
- `AddSeconds()` (`Memory.c:L2104`): Adds N seconds to an HHMMSS time value

### 9.2 Medication Unit Conversion

**ConvertUnitAmount()** (`Memory.c:L2134`): Converts medication amounts between units. According to the code, the conversion factors include:
- mcg/MIU to mg: multiply by 0.001
- g to mg: multiply by 1000

### 9.3 ISO DateTime Parsing

**ParseISODateTime()** (`Memory.c:L2168`): Added 2025-04-22 by Marianna. Parses ISO 8601 datetime strings (e.g., "2025-04-22T14:30:00") into separate YYYYMMDD and HHMMSS integer values.

---

## 10. Table Update Tracking

**UpdateTable()** (`SharedMemory.cpp:L4612`): An operation function that updates an `UPDT_DATA` row in UPDT_TABLE when a table's data changes. Matches by table name.

**GetTable()** (`SharedMemory.cpp:L4664`): An operation function that retrieves an `UPDT_DATA` row by table name match.

**GetTabNumByName()** (`SharedMemory.cpp:L4495`): Looks up a table number in `TableTab[]` by name, providing the index into the table schema array.

These functions appear to support a caching mechanism where components can check whether a table has been updated since their last read.

---

*Generated by CIDRA Documenter Agent - DOC-GENLIB-001*
