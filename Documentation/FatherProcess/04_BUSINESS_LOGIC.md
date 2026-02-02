# FatherProcess - Business Logic

**Component**: FatherProcess
**Task ID**: DOC-FATHER-001
**Generated**: 2026-02-02

---

## Process Supervision

According to the code analysis, FatherProcess appears to implement a process supervision model where it:

### 1. Starts Child Processes

At line 445, `run_system()` is called to start all configured child processes:
```c
run_system ( PHARM_SYS | DOCTOR_SYS | DOCTOR_SYS_TCP );
```

The `run_system()` function (lines 1766-1884):
1. Opens the shared-memory parameters table (line 1789)
2. Iterates through "Program..." entries (line 1797)
3. Checks if the program belongs to the requested subsystem (lines 1816-1819)
4. Starts the configured number of instances via `Run_server()` (lines 1851-1877)

### 2. Monitors Child Processes

The watchdog loop (lines 471-1321) continuously monitors child processes using:

**Primary Method - waitpid()** (line 673):
```c
son_pid = waitpid ((pid_t) -1, &status, WNOHANG);
```

**Fallback Method - Died-Processes Table** (lines 680-698):
If `waitpid()` returns 0, the code checks `dipr_tablep` for process death notifications that may have arrived via other means.

**Orphan Detection** (lines 706-754):
During shutdown, uses `getpgid()` to detect processes that have terminated but were not caught by `waitpid()`:
```c
if (getpgid (proc_data.pid) == -1)
```

### 3. Restarts Failed Processes

According to lines 1006-1063, when a child process dies and conditions permit:

**Restart Conditions** (lines 1017-1021):
- Process was found in process table (`proc_found == MAC_OK`)
- Restart flag is true (`restart == MAC_TRUE`)
- System status is UP (`stat_data.status == SYSTEM_UP`)
- Relevant subsystem is UP (`relevant_status == SYSTEM_UP`)
- Process is not X.25 worker (legacy, line 1021)

**Retry Limit Logic** (lines 1023-1037):
- If enough time has passed since last start (based on `interval` parameter), retry count resets
- If retries exceed `ProcRetrys` limit, process is not restarted

**Restart Action** (lines 1039-1055):
```c
state = Run_server (&proc_data);
```

---

## System States

According to lines 509-569 and 603-660, the system maintains a state machine with four states:

### State Definitions

| State | Constant | Value | Description |
|-------|----------|-------|-------------|
| Starting | GOING_UP | 0 | System is initializing |
| Running | SYSTEM_UP | 1 | System is fully operational |
| Stopping | GOING_DOWN | 2 | System is shutting down |
| Stopped | SYSTEM_DOWN | 3 | System is fully stopped |

### Whole-System State Logic

**GOING_UP -> SYSTEM_UP** (lines 511-529):
Transition occurs when `sons > 0` (at least one child process is running):
```c
case GOING_UP:
    if (sons > 0)
    {
        set_sys_status (SYSTEM_UP, 0, 0);
    }
    break;
```

**GOING_DOWN -> SYSTEM_DOWN** (lines 536-550):
Transition occurs when `sons < 1` (no child processes running):
```c
case GOING_DOWN:
    if (sons < 1)
    {
        set_sys_status (SYSTEM_DOWN, 0, 0);
    }
    break;
```

**SYSTEM_DOWN -> Exit** (lines 553-562):
When sons count reaches zero, the watchdog loop exits:
```c
case SYSTEM_DOWN:
    if (sons < 1)
    {
        shut_down = MAC_TRUE;
        continue;
    }
    break;
```

### Per-Subsystem State Logic

According to lines 603-660, each subsystem (pharmacy, doctor) has independent state tracking:

**Pharmacy System** (`stat_data.pharm_status`):
- Counted via `pharm_proc_count` at lines 575-597
- GOING_UP -> SYSTEM_UP when `pharm_proc_count > 0` (lines 605-611)
- GOING_DOWN -> SYSTEM_DOWN when `pharm_proc_count < 1` (lines 613-619)

**Doctor System** (`stat_data.doctor_status`):
- Counted via `doc_proc_count` at lines 575-597
- GOING_UP -> SYSTEM_UP when `doc_proc_count > 0` (lines 635-641)
- GOING_DOWN -> SYSTEM_DOWN when `doc_proc_count < 1` (lines 643-649)

---

## Restart Logic

### Exit Code Handling

According to the switch statement at lines 858-965, different exit codes trigger different behaviors:

| Exit Code | Constant | Restart | Action |
|-----------|----------|---------|--------|
| Normal | MAC_SERV_SHUT_DOWN | No | No logging |
| System shutdown | MAC_SYST_SHUT_DOWN | No | Set system to GOING_DOWN |
| Memory error | MAC_EXIT_NO_MEM | Yes | Log and restart |
| SQL error | MAC_EXIT_SQL_ERR | Yes | Log and restart |
| SQL connect error | MAC_EXIT_SQL_CONNECT | Yes | Log and restart |
| System error | MAC_EXIT_SELECT | Yes | Log and restart |
| Network error | MAC_EXIT_TCP | Yes | Log and restart |
| Trapped signal | MAC_EXIT_SIGNAL | Yes | Decrement retrys, log and restart |
| Not started | MAC_CHILD_NOT_STARTED | No | Increment sons count |
| Server reset | MAC_SERV_RESET | Yes | Decrement retrys, log and restart |

### Signal-Based Termination

**Stopped by signal** (lines 971-975):
```c
if (WIFSTOPPED (status))
{
    restart = MAC_FALS;  // Do NOT restart
}
```

**Killed by signal** (lines 977-992):
```c
if (WIFSIGNALED (status))
{
    proc_data.retrys--;
    restart = MAC_TRUE;  // DO restart
}
```

### Retry Interval Reset

According to lines 1023-1031, if a process has been running for longer than the `interval` parameter, its retry count is reset:
```c
if ((cur_time - proc_data.start_time) > interval)
{
    proc_data.retrys = 0;
}
```

---

## Healthcare System Context

### Subsystem Identification

Based on the code at lines 409-436:

| System | Constant | Purpose |
|--------|----------|---------|
| PHARM_SYS | Bit 0 | Pharmacy subsystem |
| DOCTOR_SYS | Bit 1 | Doctor subsystem |
| DOCTOR_SYS_TCP | Bit 2 | Doctor TCP subsystem |

### Environment Variable: MAC_SYS

According to lines 409-436, the `MAC_SYS` environment variable determines which subsystems to run:

```c
system_env = getenv ("MAC_SYS");

running_system = (strstr( system_env, "pharm"       ) ? PHARM_SYS       : 0 ) |
                 (strstr( system_env, "doctor"      ) ? DOCTOR_SYS      : 0 ) |
                 (strstr( system_env, "doc_tcp_only") ? DOCTOR_SYS_TCP  : 0 );
```

**Default Behavior** (lines 418-427):
If `MAC_SYS` is not set or empty, defaults to `PHARM_SYS | DOCTOR_SYS` (both systems).

### Process Types

According to the code at lines 1365-1377, different process types receive different shutdown signals:

**SIGTERM (soft kill)** - lines 1367-1373:
- SQLPROC_TYPE (SqlServer.exe)
- AS400TOUNIX_TYPE (As400UnixServer.exe)
- DOCSQLPROC_TYPE (DocSqlServer.exe)
- PURCHASE_HIST_TYPE (PurchaseHistoryServer)

**Signal 9 (hard kill)** - line 1375:
- All other process types

### Registration

At line 351, FatherProcess registers itself in the shared-memory process table:
```c
ABORT_ON_ERR (AddCurrProc (0, FATHERPROC_TYPE, 0, PHARM_SYS | DOCTOR_SYS));
```

This indicates FatherProcess belongs to both pharmacy and doctor subsystems.

---

## Multi-Instance Management

According to lines 1200-1320, FatherProcess supports running multiple instances of certain programs:

### Configuration Source

Instance control parameters are loaded from the database at lines 1668-1683:
- `program_type` - Process type constant
- `instance_control` - Enable flag (0 = disabled)
- `startup_instances` - Initial count to start
- `max_instances` - Maximum allowed
- `min_free_instances` - Minimum available (non-busy)

### Runtime Monitoring

The watchdog loop (lines 1213-1320) monitors multi-instance programs:

1. **Check conditions** (lines 1225-1232):
   - Array element was initialized
   - Instance control is enabled
   - Multiple instances are configured
   - Minimum free instances is set
   - Relevant subsystem is UP

2. **Count instances** (lines 1235-1248):
   - Scans `proc_tablep` for matching `proc_type`
   - Counts total running and non-busy instances

3. **Start new instances if needed** (lines 1262-1318):
   - If `nonbusy_count < min_free_instances` AND `running_count < max_instances`
   - Calculates `NewInstancesToOpen`
   - Calls `Run_server()` for each new instance

---

## Parameters Loading

### From Database to Shared Memory

The `sql_dbparam_into_shm()` function (lines 1481-1597) loads all parameters:

1. Uses cursor `FP_setup_cur` to SELECT from `setup_new` table
2. Builds key as `program_name.param_name` (line 1546)
3. Stores in shared memory via `AddItem()` (line 1550)
4. Additionally stores credentials (lines 1576-1590):
   - `All.mac_uid`
   - `All.mac_user`
   - `All.mac_pass`
   - `All.mac_db`

### Program-Specific Parameters

The `SqlGetParamsByName()` function (lines 1608-1757) loads parameters for FatherProcess specifically:

1. Uses cursor `FP_params_cur` to SELECT by `program_name`
2. Matches against `prog_params` array
3. Converts values based on type (lines 1692-1708):
   - `PAR_INT` -> `atoi()`
   - `PAR_LONG` -> `atol()`
   - `PAR_DOUBLE` -> `atof()`
   - `PAR_CHAR` -> `strcpy()`

---

## IPC Command Processing

According to lines 1127-1197, the following control commands are processed:

### LOAD_PAR (line 1133)
Reloads parameters from database:
```c
BREAK_ON_ERR (SqlGetParamsByName (FatherParams, PAR_RELOAD));
BREAK_ON_ERR (sql_dbparam_into_shm (parm_tablep, stat_tablep, PAR_RELOAD));
```

### STRT_PH_ONLY / STRT_DC_ONLY (lines 1139-1163)
Starts a specific subsystem:
- Checks if subsystem is currently DOWN
- Calls `run_system()` with appropriate flag
- Sets subsystem status to GOING_UP

### STDN_PH_ONLY / STDN_DC_ONLY (lines 1165-1174)
Initiates graceful shutdown of a specific subsystem:
- Sets subsystem status to GOING_DOWN

### SHUT_DWN (lines 1176-1184)
Initiates graceful full system shutdown:
- Sets whole-system status to GOING_DOWN

### STDN_IMM (lines 1186-1196)
Initiates immediate full system shutdown:
- Sets whole-system status to SYSTEM_DOWN
- Sets `shut_down = MAC_TRUE` to exit watchdog loop

---

## Statistics Collection

According to lines 352-405, FatherProcess creates statistics rows in shared memory:

### Pharmacy Statistics (TSTT_DATA)

Created at lines 356-381:
- `start_time` - System start timestamp
- `last_time[][]` - Per-message timestamps for various intervals
- `time_count[][]` - Message counters

**Note**: According to the comment at line 354-370, this feature "is not in real use any longer" and the `PharmMessages` list is out of date.

### Doctor Statistics (DSTT_DATA)

Created at lines 384-405:
- Same structure as pharmacy statistics
- Uses `DoctorMessages` array

---

## Security Considerations

### Privilege Dropping

According to lines 294-297, the process drops to an unprivileged user after initialization:
```c
if (setuid (atoi (MacUid)))
{
    GerrLogMini (GerrId, "Can't setuid to %d - error (%d) %s.", atoi (MacUid), GerrStr);
}
```

### Credential Storage

According to lines 1576-1590, database credentials are stored in shared memory:
- `All.mac_uid` - User ID
- `All.mac_user` - Username
- `All.mac_pass` - Password (stored in shared memory - see security notes in RESEARCH/)
- `All.mac_db` - Database name

**Security Note**: As documented in `RESEARCH/context_summary.md`, hard-coded secret values exist in `TikrotRPC.h` and `global_1.h`. These locations are documented but values are not copied.

---

*Generated by CIDRA Documenter Agent - DOC-FATHER-001*
