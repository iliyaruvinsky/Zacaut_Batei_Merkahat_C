# FatherProcess - System Architecture

**Component**: FatherProcess
**Task ID**: DOC-FATHER-001
**Generated**: 2026-02-02

---

## Process Model

According to the code at `FatherProcess.c:350-351`, FatherProcess registers itself as `FATHERPROC_TYPE` for both subsystems (`PHARM_SYS | DOCTOR_SYS`), indicating it serves as the parent supervisor for the entire MACCABI healthcare backend.

### Process Hierarchy

```
                    ┌─────────────────────┐
                    │    FatherProcess    │
                    │  (FATHERPROC_TYPE)  │
                    │   PID: [dynamic]    │
                    └──────────┬──────────┘
                               │
           ┌───────────────────┼───────────────────┐
           │                   │                   │
           ▼                   ▼                   ▼
    ┌─────────────┐     ┌─────────────┐     ┌─────────────┐
    │  SqlServer  │     │As400UnixSrv │     │ Other Child │
    │(SQLPROC_TYPE)│     │(AS400TOUNIX │     │  Processes  │
    │             │     │  _TYPE)     │     │             │
    └─────────────┘     └─────────────┘     └─────────────┘
```

### Subsystem Flags

According to the code at lines 409-445, the system determines which subsystems to run based on the `MAC_SYS` environment variable:

| Flag | Constant | Meaning |
|------|----------|---------|
| Bit 0 | PHARM_SYS | Pharmacy system |
| Bit 1 | DOCTOR_SYS | Doctor system |
| Bit 2 | DOCTOR_SYS_TCP | Doctor TCP system |

As seen at lines 418-436, if `MAC_SYS` is not set or empty, the system defaults to running both pharmacy and doctor systems (`PHARM_SYS | DOCTOR_SYS`).

---

## Boot Sequence

Based on the initialization section at `FatherProcess.c:210-351`, the boot sequence appears to be:

```
┌─────────────────────────────────────────────────────────────────────┐
│                        BOOT SEQUENCE                                 │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│  1. fcntl() - Keep file descriptors 0-2 open       [line 219-224]   │
│        │                                                            │
│        ▼                                                            │
│  2. setsid() - Become session leader               [line 227]       │
│        │                                                            │
│        ▼                                                            │
│  3. InitEnv() / HashEnv() - Environment setup      [line 230-233]   │
│        │                                                            │
│        ▼                                                            │
│  4. sigaction(SIGTERM) - Install signal handler    [line 250-253]   │
│        │                                                            │
│        ▼                                                            │
│  5. SQLMD_connect() - Database connection (retry)  [line 262-271]   │
│        │                                                            │
│        ▼                                                            │
│  6. SqlGetParamsByName() - Load parameters         [line 277]       │
│        │                                                            │
│        ▼                                                            │
│  7. mlockall()/plock() - Lock pages in memory      [line 281-291]   │
│        │                                                            │
│        ▼                                                            │
│  8. setuid() - Drop to unprivileged user           [line 294-297]   │
│        │                                                            │
│        ▼                                                            │
│  9. GetCurrNamedPipe() / ListenSocketNamed()       [line 300-301]   │
│        │     - Create IPC endpoint                                  │
│        ▼                                                            │
│ 10. CreateSemaphore() - System semaphore           [line 304]       │
│        │                                                            │
│        ▼                                                            │
│ 11. InitFirstExtent() - Shared memory init         [line 307]       │
│        │                                                            │
│        ▼                                                            │
│ 12. CreateTable loop - Create shm tables           [line 312-334]   │
│        │     (iterates over TableTab[])                             │
│        ▼                                                            │
│ 13. AddCurrProc() - Register in process table      [line 351]       │
│        │                                                            │
│        ▼                                                            │
│ 14. sql_dbparam_into_shm() - Load params to shm    [line 348]       │
│        │                                                            │
│        ▼                                                            │
│ 15. run_system() - Start all child processes       [line 445]       │
│        │                                                            │
│        ▼                                                            │
│     [Enter Watchdog Loop]                                           │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
```

---

## IPC Mechanisms

### 1. Unix-Domain Sockets (Named Pipes)

According to the research context and code at lines 299-301, the system uses Unix-domain sockets (`AF_UNIX`, `SOCK_STREAM`) for inter-process communication:

**Creation** (FatherProcess.c:299-301):
```c
GetCurrNamedPipe (CurrNamedPipe);
ABORT_ON_ERR (ListenSocketNamed (CurrNamedPipe));
```

**Message Reception** (FatherProcess.c:1123):
```c
state = GetSocketMessage (500000, buf, sizeof (buf), &len, CLOSE_SOCKET);
```

**IPC Commands** (lines 1127-1197):
| Command | Purpose |
|---------|---------|
| LOAD_PAR | Reload parameters from database to shared memory |
| STRT_PH_ONLY | Start pharmacy system |
| STRT_DC_ONLY | Start doctor system |
| STDN_PH_ONLY | Shutdown pharmacy system |
| STDN_DC_ONLY | Shutdown doctor system |
| SHUT_DWN | Graceful full system shutdown |
| STDN_IMM | Immediate full system shutdown |

### 2. Semaphore

According to line 304, a system semaphore is created:
```c
ABORT_ON_ERR (CreateSemaphore ());
```

The semaphore is deleted during shutdown at line 1453:
```c
DeleteSemaphore ();
```

### 3. Shared Memory

**Initialization** (line 307):
```c
ABORT_ON_ERR (InitFirstExtent (SharedSize));
```

**Table Creation Loop** (lines 312-334):
Tables are created by iterating over `TableTab[]` (defined in GenSql.c).

**Cleanup** (line 1447):
```c
KillAllExtents ();
```

---

## Shared Memory Tables

Based on the global variables at lines 78-116 and the `table_ptrs` array, the following tables appear to be created:

| Table Name | Pointer | Row Type | Purpose |
|------------|---------|----------|---------|
| PARM_TABLE | parm_tablep | PARM_DATA | Parameters from database |
| PROC_TABLE | proc_tablep | PROC_DATA | Process registry |
| STAT_TABLE | stat_tablep | STAT_DATA | System status |
| TSTT_TABLE | tstt_tablep | TSTT_DATA | Pharmacy statistics |
| DSTT_TABLE | dstt_tablep | DSTT_DATA | Doctor statistics |
| UPDT_TABLE | updt_tablep | UPDT_DATA | Table update timestamps |
| MSG_RECID_TABLE | msg_recid_tablep | MESSAGE_RECID_ROW | Message record IDs |
| PRSC_TABLE | prsc_tablep | PRESCRIPTION_ROW | Prescriptions |
| DIPR_TABLE | dipr_tablep | DIPR_DATA | Died processes |

---

## Data Flow

### Startup Data Flow

```
┌──────────────┐     ┌──────────────┐     ┌──────────────┐
│   Database   │────▶│ FatherProcess│────▶│    Shared    │
│  setup_new   │     │              │     │    Memory    │
└──────────────┘     └──────────────┘     └──────────────┘
       │                    │                    │
       │                    │                    │
       │              ┌─────▼─────┐              │
       │              │   Child   │              │
       │              │ Processes │◀─────────────┘
       │              └───────────┘     (attach + read)
       │                    │
       └────────────────────┘
         (direct DB access)
```

### Runtime Control Flow

```
┌──────────────┐     ┌──────────────┐     ┌──────────────┐
│  External    │────▶│ FatherProcess│────▶│    Child     │
│  Controller  │     │ (socket IPC) │     │  Processes   │
│ (control.exe)│     │              │     │              │
└──────────────┘     └──────────────┘     └──────────────┘
                           │
                           ▼
                    ┌──────────────┐
                    │    Shared    │
                    │    Memory    │
                    │ (stat_tablep)│
                    └──────────────┘
```

---

## Integration Points

### Database

**Connection** (lines 262-271):
- Uses `SQLMD_connect()` with retry loop until `MAIN_DB->Connected`
- Sleep 10 seconds between retries

**Cursors** (defined in MacODBC_MyOperators.c):

| Cursor | Query | Purpose |
|--------|-------|---------|
| FP_params_cur | SELECT from setup_new (by program_name) | Get program parameters |
| FP_setup_cur | SELECT all from setup_new | Load all parameters to shared memory |

**Disconnection** (line 1441):
```c
SQLMD_disconnect ();
```

### External Libraries

Based on the #include statements and code patterns:

| Library | Functions Used | Purpose |
|---------|---------------|---------|
| GenLib | InitFirstExtent, CreateTable, Run_server, etc. | Shared memory, process spawn |
| GenSql | SQLMD_connect, cursors, TableTab[] | Database access |

---

## State Machine

According to lines 509-661, the system maintains a state machine for tracking system status:

### System States

| State | Constant | Meaning |
|-------|----------|---------|
| 0 | GOING_UP | System is starting up |
| 1 | SYSTEM_UP | System is running |
| 2 | GOING_DOWN | System is shutting down |
| 3 | SYSTEM_DOWN | System is fully stopped |

### State Transitions

```
                 ┌───────────────┐
                 │   GOING_UP    │
                 │   (initial)   │
                 └───────┬───────┘
                         │ sons > 0
                         ▼
                 ┌───────────────┐
          ┌──────│   SYSTEM_UP   │◀────────┐
          │      │   (running)   │         │
          │      └───────┬───────┘         │
          │              │                 │
 start    │              │ shutdown        │ restart
 subsys   │              │ command         │ subsys
          │              ▼                 │
          │      ┌───────────────┐         │
          │      │  GOING_DOWN   │─────────┘
          │      │  (stopping)   │
          │      └───────┬───────┘
          │              │ sons < 1
          │              ▼
          │      ┌───────────────┐
          └─────▶│  SYSTEM_DOWN  │
                 │   (stopped)   │
                 └───────────────┘
```

### Per-Subsystem Status

According to lines 603-660, each subsystem (pharmacy, doctor) maintains its own status, tracked separately from the whole-system status:
- `stat_data.pharm_status`
- `stat_data.doctor_status`

---

## Signal Handling

### Handled Signals

According to lines 250-253 and 1937-1972:

| Signal | Number | Handler | Action |
|--------|--------|---------|--------|
| SIGTERM | 15 | TerminateHandler | Graceful shutdown |
| SIGFPE | - | TerminateHandler | Log and shutdown |
| SIGSEGV | - | TerminateHandler | Log and shutdown |

### Signal Handler Behavior

As seen at lines 1937-1972, `TerminateHandler()`:
1. Resets signal handling via `sigaction()`
2. Sets global `caught_signal` variable
3. Logs a descriptive message
4. Triggers orderly shutdown of the system

---

## Shutdown Sequence

According to lines 1322-1469:

```
┌─────────────────────────────────────────────────────────────────────┐
│                      SHUTDOWN SEQUENCE                               │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│  1. Loop through proc_tablep                      [line 1348]       │
│        │                                                            │
│        ▼                                                            │
│  2. For each child process:                                         │
│     - SqlServer, As400UnixServer, DocSqlServer,                     │
│       PurchaseHistoryServer: kill(SIGTERM)        [line 1372]       │
│     - All others: kill(Signal 9)                  [line 1375]       │
│        │                                                            │
│        ▼                                                            │
│  3. Remove child's named pipe                     [line 1382-1388]  │
│        │                                                            │
│        ▼                                                            │
│  4. SQLMD_disconnect() - Close database           [line 1441]       │
│        │                                                            │
│        ▼                                                            │
│  5. DisposeSockets() - Close sockets              [line 1444]       │
│        │                                                            │
│        ▼                                                            │
│  6. KillAllExtents() - Remove shared memory       [line 1447]       │
│        │                                                            │
│        ▼                                                            │
│  7. usleep(1000000) - Wait 1 second               [line 1452]       │
│        │                                                            │
│        ▼                                                            │
│  8. DeleteSemaphore()                             [line 1453]       │
│        │                                                            │
│        ▼                                                            │
│  9. exit(MAC_OK)                                  [line 1464]       │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
```

---

*Generated by CIDRA Documenter Agent - DOC-FATHER-001*
