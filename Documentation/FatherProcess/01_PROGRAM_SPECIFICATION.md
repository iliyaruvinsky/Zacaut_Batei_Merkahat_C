# FatherProcess - Program Specification

**Component**: FatherProcess
**Task ID**: DOC-FATHER-001
**Generated**: 2026-02-02
**Source**: `source_code/FatherProcess/`

---

## Overview

According to the file header at `FatherProcess.c:7-10`, FatherProcess is described as:
> "Server for starting the MACABI system and keeping all servers up. Servers to start up and parameters are read from database."

This program appears to function as the watchdog/supervisor daemon for the MACCABI healthcare backend system.

**Author**: Ely Levy (Reshuma), as stated at `FatherProcess.c:18-19`
**Original Date**: 30.05.1996

---

## Purpose

Based on the code analysis, FatherProcess appears to:

1. **Initialize system resources** - Creates semaphores, shared memory, and tables (lines 303-334)
2. **Start child processes** - Spawns configured server programs from database parameters (line 445)
3. **Monitor child processes** - Watchdog loop detects dead children and restarts them (lines 471-1321)
4. **Handle IPC commands** - Receives control messages via Unix-domain sockets (lines 1104-1198)
5. **Manage graceful shutdown** - Terminates all children on SIGTERM signal (lines 1322-1469)

---

## File Structure

| File | Lines | Purpose |
|------|------:|---------|
| FatherProcess.c | 1972 | Main watchdog daemon source |
| MacODBC_MyOperators.c | 114 | ODBC SQL cursor definitions |
| MacODBC_MyCustomWhereClauses.c | 10 | Custom WHERE clauses (stub) |
| Makefile | 44 | Build configuration |
| **Total** | **2140** | |

*Line counts verified via PowerShell `(Get-Content file).Count`*

---

## Functions

| Function | Lines | Location | Purpose |
|----------|------:|----------|---------|
| main | 1323 | 146-1469 | Entry point, initialization, and watchdog loop |
| sql_dbparam_into_shm | 116 | 1481-1597 | Load parameters from database to shared memory |
| SqlGetParamsByName | 149 | 1608-1757 | Get parameters from database by program name |
| run_system | 118 | 1766-1884 | Start child processes from parameters table |
| GetProgParm | 33 | 1893-1926 | Lookup single parameter from shared memory |
| TerminateHandler | 35 | 1937-1972 | Signal handler for SIGTERM/SIGFPE/SIGSEGV |
| **Total Functions** | **6** | | |

---

## Dependencies (#include)

According to lines 29-30 of `FatherProcess.c`:

| Header | Line | Purpose |
|--------|------|---------|
| `<MacODBC.h>` | 29 | ODBC database infrastructure layer |
| `"MsgHndlr.h"` | 30 | Message handling definitions (T-SWITCH data types) |

Note: `MacODBC.h` transitively includes `GenSql.h` and `Global.h`, which provide the shared-memory table engine, process types, and system constants.

---

## Preprocessor Defines

According to lines 23-76 of `FatherProcess.c`, the following defines are present:

| Define | Line | Value/Purpose |
|--------|------|---------------|
| MAIN | 23 | Marks this as the main program file |
| FATHER | 24 | Marks this as FatherProcess |
| NO_PRN | 25 | Disables diagnostic printing |
| prn | 34/50 | Diagnostic print macro |
| prn_1 | 38/53 | Diagnostic print with 1 argument |
| prn_2 | 44/59 | Diagnostic print with 2 arguments |
| Conflict_Test | 67 | SQL conflict test macro |
| PROG_KEY_LEN | 74 | Value: 31 |
| PARAM_NAM_LEN | 75 | Value: 31 |
| PARAM_VAL_LEN | 76 | Value: 31 |

Total: 13 `#define` statements (verified via PowerShell)

---

## Global Variables

According to lines 78-136 of `FatherProcess.c`:

### TABLE Pointers (lines 78-94)
| Variable | Purpose |
|----------|---------|
| parm_tablep | Parameters table |
| proc_tablep | Process registry table |
| updt_tablep | Table update timestamps |
| stat_tablep | System status table |
| tstt_tablep | Pharmacy statistics table |
| dstt_tablep | Doctor statistics table |
| prsc_tablep | Prescription table |
| msg_recid_tablep | Message record-id table |
| dipr_tablep | Died processes table |

### Other Globals (lines 97-136)
| Variable | Type | Line | Purpose |
|----------|------|------|---------|
| table_ptrs | TABLE**[] | 97 | Maps table indices to pointers |
| InstanceControl | TInstanceControl[] | 122 | Multi-instance program control array |
| running_system | int | 128 | Bitmap of active subsystems |
| caught_signal | int | 130 | Signal number caught for shutdown |
| TikrotProductionMode | int | 131 | Avoids unresolved external errors |
| UseODBC | int | 132 | ODBC usage flag (always 1) |
| sig_act_terminate | struct sigaction | 136 | SIGTERM handler configuration |

---

## Command-Line Interface

According to lines 13-15 of `FatherProcess.c`:

```
Usage: FatherProcess.exe
```

The program takes no command-line arguments. All configuration is read from:
1. Environment variables (e.g., `MAC_SYS` at line 409)
2. Database `setup_new` table (via `SqlGetParamsByName` at line 277)

---

## Exit Codes

Based on the switch statement at lines 858-965, the following exit codes appear to be recognized:

| Code | Constant | Meaning |
|------|----------|---------|
| 0 | MAC_SERV_SHUT_DOWN | Normal shutdown |
| - | MAC_SYST_SHUT_DOWN | System shutdown |
| - | MAC_EXIT_NO_MEM | Memory error |
| - | MAC_EXIT_SQL_ERR | SQL error |
| - | MAC_EXIT_SQL_CONNECT | SQL connect error |
| - | MAC_EXIT_SELECT | System error |
| - | MAC_EXIT_TCP | Network error |
| - | MAC_EXIT_SIGNAL | Killed by trapped signal |
| - | MAC_CHILD_NOT_STARTED | Child process failed to start |
| - | MAC_SERV_RESET | Server reset requested |

The program itself exits with `MAC_OK` at line 1464.

---

## Documentation Limitations

### What CANNOT be determined from code:
- Actual contents of the `setup_new` database table
- Runtime behavior under specific load conditions
- Complete list of child programs that may be started (depends on database configuration)
- Performance characteristics

### What IS known from code:
- Boot sequence order (lines 259-351)
- Signal handling behavior (lines 239-253, 1937-1972)
- State machine states (GOING_UP, SYSTEM_UP, GOING_DOWN, SYSTEM_DOWN)
- IPC command set (LOAD_PAR, STRT_PH_ONLY, STRT_DC_ONLY, etc.)
- Shutdown cleanup sequence (lines 1322-1469)

---

*Generated by CIDRA Documenter Agent - DOC-FATHER-001*
