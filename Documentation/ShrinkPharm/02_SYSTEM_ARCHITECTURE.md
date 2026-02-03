# ShrinkPharm - System Architecture

**Document ID:** DOC-SHRINK-001
**Generated:** 2026-02-03
**Source:** source_code/ShrinkPharm/ (574 source lines verified)

---

## Process Model

ShrinkPharm operates as a **standalone batch utility**, distinct from the long-running server processes in the MACCABI healthcare system.

### Comparison with Server Processes

| Aspect | ShrinkPharm | Server Processes (SqlServer, As400UnixServer) |
|--------|-------------|-----------------------------------------------|
| Bootstrap | Direct `SQLMD_connect()` | `InitSonProcess()` pattern |
| Lifetime | Run-to-completion batch | Long-running daemon |
| IPC | None | Named sockets, shared memory |
| Registration | None | Registers in PROC_TABLE |
| Supervision | Not supervised by FatherProcess | Supervised and auto-restarted |

According to code at `ShrinkPharm.c:128-137`, ShrinkPharm connects to the database in a simple retry loop without using the `InitSonProcess()` bootstrap that server processes use (as seen in `GenLib/Memory.c:552-612`).

### Process Hierarchy

```
┌─────────────────────────────────────────────────────────────────┐
│                    MACCABI System Context                        │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│   ┌───────────────────┐                                         │
│   │   FatherProcess   │ ◄─── Watchdog supervisor                │
│   │   (not parent)    │                                         │
│   └─────────┬─────────┘                                         │
│             │                                                    │
│             │ spawns supervised servers                          │
│             ▼                                                    │
│   ┌───────────────────┐  ┌───────────────────┐                  │
│   │    SqlServer      │  │  As400UnixServer  │  ...servers      │
│   │  (InitSonProcess) │  │  (InitSonProcess) │                  │
│   └───────────────────┘  └───────────────────┘                  │
│                                                                  │
│   ═══════════════════════════════════════════════════════════   │
│                                                                  │
│   ┌───────────────────┐                                         │
│   │    ShrinkPharm    │ ◄─── Standalone utility                 │
│   │  (direct connect) │     (runs independently)                │
│   └─────────┬─────────┘                                         │
│             │                                                    │
│             │ direct ODBC connection                             │
│             ▼                                                    │
│   ┌───────────────────┐                                         │
│   │    MS-SQL DB      │                                         │
│   │ (via ODBC driver) │                                         │
│   └───────────────────┘                                         │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

---

## Data Flow

### Purge Operation Data Flow

```
┌──────────────────────────────────────────────────────────────────┐
│                    ShrinkPharm Execution Flow                     │
├──────────────────────────────────────────────────────────────────┤
│                                                                   │
│  1. STARTUP                                                       │
│     ┌─────────────┐                                              │
│     │   main()    │                                              │
│     │  line 35    │                                              │
│     └──────┬──────┘                                              │
│            │                                                      │
│            ▼                                                      │
│  2. CONNECT TO DATABASE                                          │
│     ┌─────────────────────────────────────────┐                  │
│     │  SQLMD_connect() retry loop             │                  │
│     │  lines 128-137                          │                  │
│     │  until MAIN_DB->Connected               │                  │
│     └──────────────┬──────────────────────────┘                  │
│                    │                                              │
│                    ▼                                              │
│  3. READ CONFIGURATION                                           │
│     ┌─────────────────────────────────────────┐                  │
│     │  ShrinkPharmControlCur                  │                  │
│     │  SELECT from shrinkpharm                │                  │
│     │  WHERE purge_enabled <> 0               │                  │
│     │  lines 147-150                          │                  │
│     └──────────────┬──────────────────────────┘                  │
│                    │                                              │
│                    ▼                                              │
│  4. FOR EACH TARGET TABLE:                                       │
│     ┌─────────────────────────────────────────┐                  │
│     │  a. Compute MinDateToRetain             │ line 189        │
│     │     = SysDate - days_to_retain          │                  │
│     │                                          │                  │
│     │  b. Open ShrinkPharmSelectCur           │ line 227        │
│     │     SELECT date_column FROM table       │                  │
│     │                                          │                  │
│     │  c. For each row:                       │ lines 236-295   │
│     │     - DELETE WHERE CURRENT OF cursor    │                  │
│     │     - COMMIT every commit_count rows    │                  │
│     │                                          │                  │
│     │  d. Save statistics via                 │ lines 306-307   │
│     │     ShrinkPharmSaveStatistics           │                  │
│     └──────────────┬──────────────────────────┘                  │
│                    │                                              │
│                    ▼                                              │
│  5. CLEANUP AND EXIT                                             │
│     ┌─────────────────────────────────────────┐                  │
│     │  CommitAllWork()                        │ line 327        │
│     │  SQLMD_disconnect()                     │ line 328        │
│     │  exit(0)                                │ line 358        │
│     └─────────────────────────────────────────┘                  │
│                                                                   │
└──────────────────────────────────────────────────────────────────┘
```

---

## Database Interaction

### Connection Strategy

According to code at `ShrinkPharm.c:128-137`:

```
do {
    SQLMD_connect();
    if (!MAIN_DB->Connected) {
        sleep(10);
        // retry...
    }
} while (!MAIN_DB->Connected);
```

This pattern is similar to FatherProcess (`FatherProcess.c:259-271`) but without the `InitSonProcess()` infrastructure.

### ODBC Configuration

As seen at `ShrinkPharm.c:124-126`:

| Setting | Value | Purpose |
|---------|-------|---------|
| LOCK_TIMEOUT | 1000 ms | Patient wait for locks |
| DEADLOCK_PRIORITY | -2 | Below normal to privilege real-time operations |
| ODBC_PRESERVE_CURSORS | 1 | Preserve cursors after COMMIT |

According to a comment in `SqlServer.c:299-312`, `ODBC_PRESERVE_CURSORS` "should be set TRUE only for relatively simple programs like ShrinkPharm or As400UnixClient."

### Cursor Model

```
┌─────────────────────────────────────────────────────────────────┐
│                     Cursor Relationships                         │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│   ┌───────────────────────────────────────┐                     │
│   │  ShrinkPharmCtrlCur (control cursor)  │                     │
│   │  - Reads shrinkpharm config rows      │                     │
│   │  - Named cursor for UPDATE CURRENT OF │                     │
│   └─────────────────┬─────────────────────┘                     │
│                     │                                            │
│                     │ for each config row                        │
│                     ▼                                            │
│   ┌───────────────────────────────────────┐                     │
│   │  ShrinkPharmSelCur (select cursor)    │                     │
│   │  - Reads rows to delete from target   │                     │
│   │  - Named cursor for DELETE CURRENT OF │                     │
│   └─────────────────┬─────────────────────┘                     │
│                     │                                            │
│                     │ for each selected row                      │
│                     ▼                                            │
│   ┌───────────────────────────────────────┐                     │
│   │  DELETE WHERE CURRENT OF              │                     │
│   │  ShrinkPharmSelCur                    │                     │
│   │  (sticky statement for performance)   │                     │
│   └───────────────────────────────────────┘                     │
│                                                                  │
│   Key: ODBC_PRESERVE_CURSORS=1 allows cursors to survive        │
│        CommitAllWork() calls                                     │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

---

## Integration Points

### Shared Infrastructure

| Component | How ShrinkPharm Uses It |
|-----------|-------------------------|
| GenLib | Date/time utilities, logging, error handling |
| GenSql | ODBC abstraction layer, cursor operations |
| MacODBC.h | Database connectivity infrastructure |

### Database Tables

| Table | Role | Operations |
|-------|------|------------|
| shrinkpharm | Control table | SELECT (read config), UPDATE (write stats) |
| (dynamic) | Target tables | SELECT (read rows), DELETE (purge rows) |

**Security Note:** Target table names and date column names are read from the `shrinkpharm` control table and interpolated into dynamic SQL. If the control table is not properly governed, this could allow unintended data deletion.

---

## Signal Handling

According to code at `ShrinkPharm.c:100-116` and `ShrinkPharm.c:370-430`:

### Installed Signal Handlers

| Signal | Handler | Action |
|--------|---------|--------|
| SIGFPE | TerminateHandler | RollbackAllWork, log, exit |
| SIGSEGV | (commented out) | Would handle segmentation fault |
| SIGTERM | (not installed) | Would handle termination request |

### TerminateHandler Behavior (lines 370-430)

1. Reset signal handling via sigaction
2. Detect endless loops (same signal at same time)
3. For SIGFPE/SIGSEGV: call RollbackAllWork()
4. Log descriptive message
5. Call SQLMD_disconnect() and ExitSonProcess()

---

## Environment Dependencies

### Required Environment Variables

| Variable | Purpose | Used At |
|----------|---------|---------|
| MAC_LOG | Log file directory | `ShrinkPharm.c:72` |
| MS_SQL_ODBC_DSN | ODBC data source name | Via GenSql |
| MS_SQL_ODBC_USER | Database username | Via GenSql |
| MS_SQL_ODBC_PASS | Database password | Via GenSql |
| MS_SQL_ODBC_DB | Database name | Via GenSql |

**Note:** According to code at `ShrinkPharm.c:72`, `getenv("MAC_LOG")` is used without NULL check, which could cause a crash if the variable is unset.

---

## Build Configuration

According to `Makefile:1-26`:

### Build Targets

| Target | Command |
|--------|---------|
| all | Build ShrinkPharm.exe |
| clean | Remove TARGET and OBJS |

### Compilation

```makefile
TARGET = ShrinkPharm.exe
SRCS   = ShrinkPharm.c
OBJS   = ShrinkPharm.o
LIBS   = -lodbc -D_GNU_SOURCE
```

The Makefile includes shared definitions from `../Util/Defines.mak` and rules from `../Util/Rules.mak`.

---

*Document generated by CIDRA Documenter Agent*
*Task ID: DOC-SHRINK-001*
