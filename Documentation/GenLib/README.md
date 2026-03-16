# GenLib

**MACCABI Healthcare System - Shared Foundation Library (libgenlib.a)**

---

## Quick Summary

GenLib is the shared foundation library linked by every server component in the MACCABI healthcare C backend. According to the source file headers (e.g., `Memory.c:L12-L16`, `SharedMemory.cpp:L12-L17`), it provides the prescribed routines for memory handling, shared memory, socket communication, semaphores, and error handling.

The library provides five core subsystems:
1. **Process Lifecycle** -- `InitSonProcess()` is the 13-step child bootstrap called by every server component; `ExitSonProcess()` handles clean shutdown; `Run_server()` forks and execs children
2. **IPC Socket System** -- AF_UNIX stream sockets (called "named pipes") with MESSAGE_HEADER framing protocol; also supports AF_INET TCP sockets
3. **Shared Memory Table Engine** -- Multi-extent System V shared memory with an in-memory table database using doubly-linked-list rows, extent chaining, and free-chain reuse
4. **Semaphore Facility** -- Single System V semaphore protecting all shared memory access, with re-entrant (pushable) locking via `sem_busy_flg` counter
5. **Error Logging** -- Variadic log functions to per-process log files with doom-loop protection (250ms usleep)

Written by Ely Levy (Reshuma), 02/05/1996. Maintained by DonR, GAL, and Marianna.

---

## Key Files

| File | Lines | Purpose |
|------|------:|---------|
| SharedMemory.cpp | 4,774 | Shared memory multi-extent table engine (C++ in a primarily C codebase) |
| Memory.c | 2,196 | Process lifecycle, utilities, Hebrew encoding, date arithmetic |
| Sockets.c | 1,759 | Unix-domain and TCP socket IPC system |
| GeneralError.c | 770 | Error handling and logging with doom-loop protection |
| Semaphores.c | 734 | System V semaphore with re-entrant locking |
| GxxPersonality.c | 27 | C++ personality stub and Prn() debug helper |
| Makefile | 55 | Build configuration for libgenlib.a |

**Total**: 10,316 lines, 114 functions across 7 files

---

## How to Use This Documentation

This documentation set consists of 7 files:

| File | Contents |
|------|----------|
| **01_PROGRAM_SPECIFICATION.md** | Library overview, file inventory, complete public API listing, consumer map |
| **02_SYSTEM_ARCHITECTURE.md** | Process lifecycle (InitSonProcess sequence), shared memory architecture, FatherProcess relationship, extent system |
| **03_TECHNICAL_ANALYSIS.md** | IPC socket system (creation, messaging, FILE_MESG vs DATA_MESG), select multiplexing, semaphore locking, error handling patterns |
| **04_BUSINESS_LOGIC.md** | Shared memory table engine (CRUD operations), process table management, environment/configuration, broadcast system, statistics |
| **05_CODE_ARTIFACTS.md** | Complete function catalog with signatures and parameters; key macros; data structures (MASTER_SHM_HEADER, EXTENT_PTR, MESSAGE_HEADER, etc.) |
| **README.md** | This file -- overview and navigation guide |
| **VALIDATION_REPORT.md** | Quality assurance verification, validation score, verification backlog, security notes |

### Recommended Reading Order

1. **README.md** (this file) -- Get oriented
2. **01_PROGRAM_SPECIFICATION.md** -- Understand the library structure and API surface
3. **02_SYSTEM_ARCHITECTURE.md** -- Understand process lifecycle and shared memory architecture
4. **03_TECHNICAL_ANALYSIS.md** -- Deep dive into IPC, semaphores, and error handling
5. **04_BUSINESS_LOGIC.md** -- Understand the table engine, SQL server locking, statistics
6. **05_CODE_ARTIFACTS.md** -- Reference function signatures, macros, and data structures

---

## Related Components

According to the dependency analysis, GenLib interacts with:

### Consumer Processes (link libgenlib.a)
- **FatherProcess** -- Supervisor daemon; creates semaphore, shared memory, and tables; spawns children via `Run_server()`
- **SqlServer** -- SQL server worker process (SQLPROC_TYPE)
- **As400UnixServer** -- AS/400 bridge server (AS400TOUNIX_TYPE)
- **As400UnixClient** -- AS/400 bridge client (UNIXTOAS400_TYPE)
- **As400UnixDocServer** -- Doctor AS/400 bridge (DOCAS400TOUNIX_TYPE)
- **As400UnixDoc2Server** -- Doctor AS/400 bridge variant (DOC2AS400TOUNIX_TYPE)
- **PharmTcpServer** -- Pharmacy TCP server (PHARM_TCP_SERVER_TYPE)
- **PharmWebServer** -- Pharmacy web server (WEBSERVERPROC_TYPE)

### Libraries and Headers
- **GenSql** -- Defines `TableTab[]` schema, provides SQLMD_* database functions
- **Global.h** -- Central shared header defining data structures (TABLE, PROC_DATA, STAT_DATA, etc.)
- **MsgHndlr.h** -- Message handling definitions
- **CCGlobal.h** -- Additional constants (included by SharedMemory.cpp)

---

## Important Design Notes

- GenLib is a **library**, not a server process -- it has no `main()` or dispatch loop
- `InitSonProcess()` is the most-called function system-wide -- every child process calls it at startup
- The IPC uses AF_UNIX stream sockets (not POSIX FIFOs) despite being called "named pipes" in the code
- `SharedMemory.cpp` is the only C++ file; all others are plain C
- A single System V semaphore guards all shared memory access across the entire system
- The semaphore key and shared memory key share the same hard-coded value (documented by location only for security)
- File permissions 0777 are used for both semaphore and shared memory

---

## Source Code Location

```
source_code/GenLib/
    SharedMemory.cpp           # Shared memory table engine (4,774 lines)
    Memory.c                   # Process lifecycle + utilities (2,196 lines)
    Sockets.c                  # Socket IPC system (1,759 lines)
    GeneralError.c             # Error logging (770 lines)
    Semaphores.c               # Semaphore facility (734 lines)
    GxxPersonality.c           # Debug helper (27 lines)
    Makefile                   # Build config (55 lines)
```

---

## Documentation Metadata

| Field | Value |
|-------|-------|
| Task ID | DOC-GENLIB-001 |
| Generated | 2026-03-16 |
| Agent | CIDRA Documenter Agent |
| Source | CHUNKS/GenLib/, RESEARCH/GenLib_deepdive.md |
| Validation | 100/100 (see VALIDATION_REPORT.md) |

---

*Generated by CIDRA Documenter Agent - DOC-GENLIB-001*
