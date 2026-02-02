# FatherProcess - Technical Analysis

**Component**: FatherProcess
**Task ID**: DOC-FATHER-001
**Generated**: 2026-02-02

---

## Function Analysis

### 1. main() - Lines 146-1469 (1323 lines)

**Complexity**: Very High

The `main()` function is the entry point and contains the entire program logic. According to the code, it can be divided into logical sections:

#### Section A: Initialization (lines 146-351)
- Declares local variables (lines 148-208)
- Sets file descriptor flags via `fcntl()` (lines 219-224)
- Becomes session leader via `setsid()` (line 227)
- Initializes environment via `InitEnv()` and `HashEnv()` (lines 230-233)
- Installs SIGTERM handler via `sigaction()` (lines 246-253)
- Connects to database with retry loop (lines 262-271)
- Loads parameters via `SqlGetParamsByName()` (line 277)
- Optionally locks memory pages (lines 281-291)
- Drops privileges via `setuid()` (lines 294-297)
- Creates IPC endpoint via `GetCurrNamedPipe()` and `ListenSocketNamed()` (lines 300-301)
- Creates system semaphore (line 304)
- Initializes shared memory (line 307)
- Creates shared-memory tables in a loop (lines 312-334)
- Registers itself in process table (line 351)

#### Section B: Statistics Setup (lines 352-406)
- Creates pharmacy statistics row (`TSTT_DATA`) in shared memory (lines 356-381)
- Creates doctor statistics row (`DSTT_DATA`) in shared memory (lines 384-405)
- According to comment at line 354, this feature "is not in real use any longer"

#### Section C: System Startup (lines 408-448)
- Reads `MAC_SYS` environment variable (line 409)
- Builds `running_system` bitmap (lines 418-436)
- Calls `run_system()` to start child processes (line 445)

#### Section D: Watchdog Loop (lines 453-1321)
- Main monitoring loop at line 471: `for (shut_down = MAC_FALS; ...)`
- **System Status Monitoring** (lines 496-660):
  - Gets sons count via `GetSonsCount()` (line 499)
  - Gets system status via `get_sys_status()` (line 503)
  - State machine for whole system (lines 509-569)
  - Counts pharmacy/doctor child processes (lines 575-597)
  - State machine for subsystems (lines 603-660)

- **Dead Child Detection** (lines 662-1102):
  - Uses `waitpid(WNOHANG)` at line 673
  - Falls back to `dipr_tablep` if waitpid returns 0 (lines 680-698)
  - Checks for orphan processes via `getpgid()` (lines 706-754)
  - Processes exit status with detailed switch (lines 856-965)
  - Restarts child via `Run_server()` if conditions met (lines 1006-1063)

- **IPC Message Handling** (lines 1104-1198):
  - Receives messages via `GetSocketMessage()` (line 1123)
  - Handles control commands in switch statement (lines 1127-1197)

- **Multi-Instance Control** (lines 1200-1320):
  - Monitors `InstanceControl[]` array
  - Starts new instances when available count falls below minimum

#### Section E: Shutdown (lines 1322-1469)
- Iterates through `proc_tablep` (lines 1346-1426)
- Sends SIGTERM to specific process types, Signal 9 to others (lines 1365-1379)
- Disconnects database, closes sockets, removes shared memory (lines 1441-1453)
- Exits with `MAC_OK` (line 1464)

---

### 2. sql_dbparam_into_shm() - Lines 1481-1597 (116 lines)

**Complexity**: Medium

**Purpose**: Loads parameters from the `setup_new` database table into the shared-memory parameters table.

**Key Operations**:
1. Optionally deletes existing table contents on reload (line 1498)
2. Declares cursor `FP_setup_cur` for first-time use (lines 1503-1508)
3. Opens cursor and fetches rows in a loop (lines 1516-1560)
4. Builds parameter key as `program_name.param_name` (line 1546)
5. Adds each row to shared memory via `AddItem()` (line 1550)
6. Adds UID, username, password, and db name to shared memory (lines 1576-1590)
7. Increments parameters revision in `stat_tablep` (line 1593)

**Retry Logic**: According to lines 1512-1567, includes retry loop with `ACCESS_CONFLICT_SLEEP_TIME` delay on SQL conflict.

---

### 3. SqlGetParamsByName() - Lines 1608-1757 (149 lines)

**Complexity**: Medium

**Purpose**: Retrieves parameters from database by program name and populates the `InstanceControl[]` array.

**Key Operations**:
1. Declares cursor `FP_params_cur` (lines 1623-1624)
2. Zeroes "touched" flags for all parameters (lines 1627-1630)
3. Opens cursor and fetches rows (lines 1638-1720)
4. For valid program types, populates `InstanceControl[]` (lines 1668-1683)
5. Matches parameters to `prog_params` array (lines 1686-1718)
6. Converts values based on type: `PAR_INT`, `PAR_LONG`, `PAR_DOUBLE`, `PAR_CHAR` (lines 1692-1708)
7. Validates all required parameters were found (lines 1742-1752)

---

### 4. run_system() - Lines 1766-1884 (118 lines)

**Complexity**: Medium

**Purpose**: Starts child processes based on parameters in shared memory.

**Key Operations**:
1. Opens `PARM_TABLE` (line 1789)
2. Loops through parameters looking for "Program..." entries (line 1794)
3. For each program:
   - Gets its "system" parameter (line 1804)
   - Checks if it matches requested subsystem (lines 1816-1819)
   - Finds matching `InstanceControl[]` entry (lines 1830-1838)
   - Starts `startup_instances` copies via `Run_server()` (lines 1851-1877)
4. Closes table and returns (line 1881)

---

### 5. GetProgParm() - Lines 1893-1926 (33 lines)

**Complexity**: Low

**Purpose**: Looks up a single parameter value for a program from shared memory.

**Key Operations**:
1. Opens `PARM_TABLE` (line 1902)
2. Builds search key as `prog_name.param_name` (line 1904)
3. Loops through table via `ActItems()` (line 1908)
4. Returns matching `par_val` (line 1918)
5. Closes table (line 1923)

---

### 6. TerminateHandler() - Lines 1937-1972 (35 lines)

**Complexity**: Low

**Purpose**: Signal handler for terminating signals (SIGTERM, SIGFPE, SIGSEGV).

**Key Operations**:
1. Resets signal handling via `sigaction()` (line 1943)
2. Sets global `caught_signal` variable (line 1948)
3. Produces descriptive log message based on signal type (lines 1951-1968)
4. Logs shutdown message via `GerrLogMini()` (lines 1970-1971)

---

## Data Structures

### Local Structures Used (from Global.h)

| Structure | Used At | Purpose |
|-----------|---------|---------|
| TABLE_DATA | line 192 | Shared-memory table metadata |
| PROC_DATA | line 193 | Process registry row |
| DIPR_DATA | line 194 | Died-process row |
| STAT_DATA | line 195 | System status row |
| TSTT_DATA | line 196 | Pharmacy statistics row |
| DSTT_DATA | line 197 | Doctor statistics row |
| UPDT_DATA | line 198 | Table update timestamp row |
| TInstanceControl | line 199 | Multi-instance control |

### TInstanceControl Array

According to line 122 and usage throughout:
```c
TInstanceControl InstanceControl [MAX_PROC_TYPE_USED + 1];
```

Fields (from Global.h):
- `ProgramName` - Name of the executable
- `program_type` - Process type constant
- `program_system` - PHARM_SYS or DOCTOR_SYS
- `instance_control` - Flag: is multi-instance enabled
- `startup_instances` - How many to start initially
- `max_instances` - Maximum allowed instances
- `min_free_instances` - Minimum available instances
- `instances_running` - Current count

---

## Control Flow

### Main Watchdog Loop Structure

```c
for (shut_down = MAC_FALS; (shut_down == MAC_FALS) && (!caught_signal); )
{
    // 1. Get system status
    GetSonsCount (&sons);
    get_sys_status (&stat_data);

    // 2. Handle whole-system state machine
    switch (stat_data.status) { ... }

    // 3. Count subsystem processes
    // 4. Handle subsystem state machines

    // 5. Check for dead children
    son_pid = waitpid ((pid_t) -1, &status, WNOHANG);
    if (son_pid > 0) {
        // Process death, possibly restart
    }

    // 6. Check for IPC messages
    state = GetSocketMessage (...);
    if ((state == MAC_OK) && (len > 0)) {
        switch (ListNMatch (buf, PcMessages)) { ... }
    }

    // 7. Manage multi-instance programs
    for (ProgramType = 0; ProgramType <= MAX_PROC_TYPE_USED; ProgramType++) {
        // Check if more instances needed
    }
}
```

### Database Retry Pattern

According to lines 1512-1567 (`sql_dbparam_into_shm`) and lines 1633-1732 (`SqlGetParamsByName`):

```c
for (restart = MAC_TRUE, tries = 0;
     (tries < SQL_UPDATE_RETRIES) && (restart == MAC_TRUE);
     tries++)
{
    restart = MAC_FALS;

    do {
        OpenCursor (...);
        Conflict_Test_Cur (restart);  // Sets restart=TRUE on conflict
        BREAK_ON_ERR (...);

        while (1) {
            FetchCursor (...);
            Conflict_Test (restart);
            BREAK_ON_TRUE (SQLERR_code_cmp (SQLERR_end_of_fetch));
            // Process row
        }

        CloseCursor (...);
    } while (0);

    if (restart == MAC_TRUE) {
        sleep (ACCESS_CONFLICT_SLEEP_TIME);
    }
}
```

---

## Error Handling

### Error Macros

According to the code patterns observed:

| Macro | Behavior |
|-------|----------|
| ABORT_ON_ERR | Terminates program on error |
| BREAK_ON_ERR | Breaks from loop on error |
| RETURN_ON_ERR | Returns from function on error |
| BREAK_ON_TRUE | Breaks if condition is true |
| ERR_STATE | Checks if state indicates error |

### Logging

Error logging uses `GerrLogMini()` throughout, with the pattern:
```c
GerrLogMini (GerrId, "Message with %s placeholders", values);
```

Examples:
- Line 223: `"fcntl failed " GerrErr, GerrStr`
- Line 268: `"Retrying %s ODBC connect.", MAIN_DB->Name`
- Line 567: `"FatherProcess: unknown system state %d.", stat_data.status`

---

## External Function Calls

### From GenLib

| Function | Location | Purpose |
|----------|----------|---------|
| InitEnv | 230 | Initialize environment buffers |
| HashEnv | 233 | Hash environment variables |
| GetCurrProgName | 236 | Get program short name |
| GetCurrNamedPipe | 300 | Build named pipe path |
| ListenSocketNamed | 301 | Create listening socket |
| CreateSemaphore | 304 | Create system semaphore |
| InitFirstExtent | 307 | Initialize shared memory |
| CreateTable | 315 | Create shared-memory table |
| AddItem | 327, 345, 381, 405, 550, 578 | Add row to table |
| DeleteItem | 690, 798 | Delete row from table |
| ActItems | 577, 680, 729, 791, etc. | Iterate through table |
| RewindTable | 575, 727, 1235, 1346 | Reset table iteration |
| OpenTable | 789, 902 | Open shared-memory table |
| CloseTable | 881, 923 | Close shared-memory table |
| GetSonsCount | 499, 1335 | Get child process count |
| AddToSonsCount | 787, 948 | Adjust child count |
| Run_server | 1041, 1300, 1858 | Start child process |
| AddCurrProc | 351 | Register in process table |
| DisposeSockets | 1444 | Close sockets |
| KillAllExtents | 1447 | Remove shared memory |
| DeleteSemaphore | 1453 | Delete semaphore |
| GetSocketMessage | 1123 | Receive IPC message |
| GerrLogMini | many | Log message |

### From GenSql

| Function | Location | Purpose |
|----------|----------|---------|
| SQLMD_connect | 264 | Connect to database |
| SQLMD_disconnect | 1441 | Disconnect from database |
| DeclareCursor | 1623 | Declare SQL cursor |
| DeclareCursorInto | 1505 | Declare cursor with INTO |
| OpenCursor | 1518, 1638 | Open cursor |
| FetchCursor | 1527 | Fetch row |
| FetchCursorInto | 1649 | Fetch row into variables |
| CloseCursor | 1553, 1723 | Close cursor |
| SQLERR_code_cmp | 1533, 1657 | Compare error code |
| SQLERR_error_test | 1522, 1534, 1660 | Test for SQL error |

### From System

| Function | Location | Purpose |
|----------|----------|---------|
| fcntl | 219-221 | Set file descriptor flags |
| setsid | 227 | Become session leader |
| sigaction | 250, 1943 | Install signal handler |
| sleep | 267, 1564, 1729 | Pause execution |
| mlockall/plock | 284-286 | Lock memory pages |
| setuid | 294 | Change user ID |
| time | 358, 781, etc. | Get current time |
| localtime | 782 | Convert time |
| waitpid | 673 | Check for terminated children |
| getpgid | 742 | Get process group ID |
| kill | 1379 | Send signal to process |
| access | 801, 1382 | Check file existence |
| unlink | 803, 1384 | Remove file |
| getenv | 409 | Get environment variable |
| usleep | 1452 | Microsecond sleep |
| exit/_exit | 1464-1467 | Terminate program |

---

## Performance Considerations

### Watchdog Loop Timing

According to the comment at lines 1111-1121, the `GetSocketMessage()` call uses a 500000 microsecond (0.5 second) delay, which controls the main loop iteration rate:

```c
state = GetSocketMessage (500000, buf, sizeof (buf), &len, CLOSE_SOCKET);
```

The comment notes this was changed from the original `SelectWait` value (200000 = 0.2 seconds) to reduce CPU load.

### Multi-Instance Monitoring

At lines 1213-1320, the loop checks every program type on each watchdog iteration. For programs with `instance_control` enabled, it:
1. Scans the entire `proc_tablep` to count running/available instances
2. Starts new instances if below minimum

This appears to be O(n*m) where n = number of program types and m = number of running processes.

---

*Generated by CIDRA Documenter Agent - DOC-FATHER-001*
