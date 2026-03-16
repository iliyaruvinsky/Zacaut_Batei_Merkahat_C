# ORC HUB - CIDRA Agent Coordination Center

**Last Updated:** 2026-02-12
**Orchestrator:** Orc
**Project:** Zacaut_Batei_Merkahat_C (MACCABI Healthcare C Backend)
**Status:** ACTIVE

---

## THE HOLY SQUARE - 4-AGENT COMMUNICATION SYSTEM

```
┌─────────────────────────────────────────────────────────────────────────────────┐
│                           THE HOLY SQUARE                                        │
│                     4-Agent Coordination Protocol                                │
├─────────────────────────────────────────────────────────────────────────────────┤
│                                                                                 │
│     ┌─────────────┐                               ┌─────────────┐              │
│     │  RESEARCHER │                               │   CHUNKER   │              │
│     │    (RES)    │                               │    (CH)     │              │
│     │  🔵 ACTIVE  │                               │  🔵 READY   │              │
│     └──────┬──────┘                               └──────┬──────┘              │
│            │                                             │                      │
│            │              ┌─────────────┐                │                      │
│            └─────────────►│     ORC     │◄───────────────┘                      │
│                           │(Orchestrator)│                                      │
│            ┌─────────────►│             │◄───────────────┐                      │
│            │              └─────────────┘                │                      │
│            │                                             │                      │
│     ┌──────┴──────┐                               ┌──────┴──────┐              │
│     │ DOCUMENTER  │                               │ RECOMMENDER │              │
│     │    (DOC)    │                               │    (REC)    │              │
│     │  ⏸️ BLOCKED │                               │ ⛔ INACTIVE │              │
│     └─────────────┘                               └─────────────┘              │
│                                                                                 │
├─────────────────────────────────────────────────────────────────────────────────┤
│  ARTIFACT 1: ORC_HUB.md                  → Central coordination                │
│  ARTIFACT 2: PROMPT FOR CHUNKER.md       → Chunker agent tasks                 │
│  ARTIFACT 3: PROMPT FOR DOCUMENTER.md    → Documenter agent tasks              │
│  ARTIFACT 4: PROMPT FOR RESEARCHER.md    → Researcher agent tasks              │
│  (Parked)  : PROMPT FOR RECOMMENDER.md   → Awaiting client approval            │
└─────────────────────────────────────────────────────────────────────────────────┘
```

---

## PROTOCOL RULES

### READ RULES
| Agent | Reads |
|-------|-------|
| Researcher | Own prompt + ORC_HUB.md |
| Chunker | Own prompt + ORC_HUB.md |
| Documenter | Own prompt + ORC_HUB.md + Chunker outputs |
| Recommender | Own prompt + ORC_HUB.md + All documentation |
| User | All files for oversight |

### WRITE RULES
| Agent | Writes To |
|-------|-----------|
| Researcher | Own prompt (status) + RESEARCH/ folder |
| Chunker | Own prompt (status) + CHUNKS/ folder |
| Documenter | Own prompt (status) + Documentation/ folder |
| Orc | ORC_HUB.md + All agent prompts |

### COORDINATION RULES
1. Cross-agent messages go through ORC_HUB.md MESSAGE QUEUE
2. Blocking issues flagged immediately in agent's own prompt
3. Dependencies tracked in TASK REGISTRY
4. Status changes trigger ORC_HUB.md update

---

## PROJECT CONTEXT

**System:** MACCABI Healthcare C Backend
**Technology:** C (Legacy UNIX)
**Purpose:** Process supervision, database connectivity, healthcare services

**Source Code Structure:**
```
source_code/
├── FatherProcess/      ← Priority 1: Watchdog daemon
├── SqlServer/          ← Priority 2: SQL server
├── As400UnixServer/    ← Priority 3: AS/400 bridge
├── PharmTcpServer/     ← Priority 4: Pharmacy TCP
├── PharmWebServer/     ← Priority 5: Pharmacy Web
├── GenLib/             ← Shared library
├── GenSql/             ← SQL utilities
├── Include/            ← Header files
└── [other components]
```

---

## AGENT ROLES

| Agent | ID | Role | Stage | Status |
|-------|-----|------|-------|--------|
| Researcher | RES | Investigate, gather context | Pre | 🔵 ACTIVE |
| Chunker | CH | Break code into semantic chunks | 0 | 🔵 READY |
| Documenter | DOC | Create 7-file documentation | 1 | ⏸️ BLOCKED |
| Recommender | REC | Modernization recommendations | 2 | ⛔ INACTIVE |

---

## TASK REGISTRY

### RESEARCHER (RES) - Stage Pre

| ID | Task | Status | Output |
|----|------|--------|--------|
| RES-CONTEXT-001 | Build system context map | ✅ COMPLETE | RESEARCH/*.md |
| RES-DEEPDIVE-001 | Folder-by-folder deep dive | ⏸️ PAUSED | RESEARCH/*_deepdive.md |
| RES-SHRINKPHARM-001 | ShrinkPharm deep dive | ✅ COMPLETE | RESEARCH/ShrinkPharm_deepdive.md |
| RES-MACODBC-001 | MacODBC.h deep dive | ✅ COMPLETE | RESEARCH/MacODBC_deepdive.md |
| RES-SQL-001 | SqlServer deep dive (~84K lines) | ✅ COMPLETE | RESEARCH/SqlServer_deepdive.md + Merged Baseline |
| RES-GENLIB-001 | GenLib deep dive (~10K lines) | 🔵 ACTIVE | RESEARCH/GenLib_deepdive.md |

### CHUNKER (CH) - Stage 0

| ID | Component | Status | Depends On | Output |
|----|-----------|--------|------------|--------|
| CH-FATHER-001 | FatherProcess | ✅ COMPLETE | RES-CONTEXT-001 (optional) | CHUNKS/FatherProcess/ |
| CH-SQL-001 | SqlServer | ✅ COMPLETE | RES-SQL-001 ✅ | CHUNKS/SqlServer/ (62 chunks, 83,983 lines) |
| CH-AS400-001 | As400UnixServer | 📋 PLANNED | - | CHUNKS/As400UnixServer/ |
| CH-PHARM-001 | PharmTcpServer | 📋 PLANNED | - | CHUNKS/PharmTcpServer/ |
| CH-GENLIB-001 | GenLib | 📋 PLANNED | RES-GENLIB-001 | CHUNKS/GenLib/ |
| CH-SHRINK-001 | ShrinkPharm | ✅ COMPLETE | RES-SHRINKPHARM-001 ✅ | CHUNKS/ShrinkPharm/ |
| CH-MACODBC-001 | MacODBC.h | ✅ COMPLETE | RES-MACODBC-001 ✅ | CHUNKS/MacODBC/ |

### DOCUMENTER (DOC) - Stage 1

| ID | Component | Status | Depends On | Output |
|----|-----------|--------|------------|--------|
| DOC-FATHER-001 | FatherProcess | ✅ COMPLETE | CH-FATHER-001 ✅ | Documentation/FatherProcess/ |
| DOC-SQL-001 | SqlServer | ✅ COMPLETE | CH-SQL-001 ✅ | Documentation/SqlServer/ (100/100, English) |
| DOC-AS400-001 | As400UnixServer | 📋 PLANNED | CH-AS400-001 | Documentation/As400UnixServer/ |
| DOC-PHARM-001 | PharmTcpServer | 📋 PLANNED | CH-PHARM-001 | Documentation/PharmTcpServer/ |
| DOC-GENLIB-001 | GenLib | 📋 PLANNED | CH-GENLIB-001 | Documentation/GenLib/ (English) |
| DOC-SHRINK-001 | ShrinkPharm | ✅ COMPLETE | CH-SHRINK-001 ✅ | Documentation/ShrinkPharm/ (100/100) |
| DOC-MACODBC-001 | MacODBC.h (Hebrew) | ✅ COMPLETE | CH-MACODBC-001 ✅ | Documentation/MacODBC_Hebrew/ (100/100, Hebrew) |
| DOC-MACODBC-002 | MacODBC.h (English) | ✅ COMPLETE | CH-MACODBC-001 ✅ | Documentation/MacODBC/ (100/100, English) |

### RECOMMENDER (REC) - Stage 2

| ID | Task | Status | Depends On |
|----|------|--------|------------|
| REC-SYSTEM-001 | Modernization | ⛔ INACTIVE | All DOC-* + CLIENT APPROVAL |

---

## DEPENDENCY GRAPH

```
                         ┌─────────────────┐
                         │ RES-CONTEXT-001 │ ← Research provides context
                         │   🔵 ACTIVE     │
                         └────────┬────────┘
                                  │ (feeds into all)
          ┌───────────────────────┼───────────────────────┐
          ▼                       ▼                       ▼
┌─────────────────┐     ┌─────────────────┐     ┌─────────────────┐
│ CH-FATHER-001   │     │ CH-SQL-001      │     │ CH-GENLIB-001   │
│   🔵 READY      │     │   📋 PLANNED    │     │   📋 PLANNED    │
└────────┬────────┘     └────────┬────────┘     └────────┬────────┘
         │                       │                       │
         ▼                       ▼                       ▼
┌─────────────────┐     ┌─────────────────┐     ┌─────────────────┐
│ DOC-FATHER-001  │     │ DOC-SQL-001     │     │ DOC-GENLIB-001  │
│   ⏸️ BLOCKED    │     │   📋 PLANNED    │     │   📋 PLANNED    │
└────────┬────────┘     └────────┬────────┘     └────────┬────────┘
         │                       │                       │
         └───────────────────────┼───────────────────────┘
                                 ▼
                    ┌────────────────────────┐
                    │ CLIENT APPROVAL GATE   │
                    └────────────┬───────────┘
                                 ▼
                    ┌────────────────────────┐
                    │ REC-SYSTEM-001         │
                    │   ⛔ INACTIVE          │
                    └────────────────────────┘
```

---

## MESSAGE QUEUE

### To Researcher (RES)
```
[2026-02-02] ORC:
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
DISPATCH: RES-CONTEXT-001
PRIORITY: P0 - START NOW
GOAL: Build comprehensive context map
PHASES: Header Analysis → Architecture → Component Profiles → Cross-ref
OUTPUT: RESEARCH/context_summary.md, header_inventory.md, component_profiles.md
NOTE: Your findings will help Chunker and Documenter work more effectively
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

[2026-02-02] RES:
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
STATUS: PLAN READY (RES-CONTEXT-001)
NEXT: Generate RESEARCH/context_summary.md, header_inventory.md, component_profiles.md
GUARDS: Exact counts + file:line citations; redact secrets (document location only)
KEY ANCHORS:
- shm table schema: source_code/GenSql/GenSql.c (TableTab[])
- IPC named socket "pipe": source_code/GenLib/Sockets.c (GetCurrNamedPipe, FILE_MESG/DATA_MESG)
- child registration: source_code/GenLib/Memory.c (InitSonProcess/AddCurrProc)
- boot sequence: source_code/FatherProcess/FatherProcess.c (CreateSemaphore/InitFirstExtent/CreateTable loop)
DB: ODBC env vars drive MAIN_DB/ALT_DB selection (INF_CONNECT in GenSql.c)
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

[2026-02-02] RES:
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
STATUS: COMPLETE (RES-CONTEXT-001)
OUTPUTS:
- RESEARCH/context_summary.md
- RESEARCH/header_inventory.md
- RESEARCH/component_profiles.md

KEY FINDINGS (high-signal):
- Boot orchestration: FatherProcess initializes sem+shm and creates tables from TableTab[]; registers itself as PHARM_SYS|DOCTOR_SYS.
- IPC: internal “named pipes” are unix-domain sockets (AF_UNIX/SOCK_STREAM); payloads can be FILE_MESG or DATA_MESG.
- Child bootstrap: InitSonProcess opens semaphore, attaches shared memory, listens on named socket, registers into PROC_TABLE.
- DB: SQLMD_connect resolves to INF_CONNECT; supports MS-SQL and/or Informix via env vars; MAIN_DB/ALT_DB selection is configuration-driven.

SECURITY NOTE (values redacted in outputs):
- Hard-coded secret values exist in source_code/Include/TikrotRPC.h and source_code/Include/global_1.h (documented by location only).
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

[2026-02-02] ORC:
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
DISPATCH: RES-DEEPDIVE-001
PRIORITY: P0 - START NOW (parallel with CH-FATHER-001)
GOAL: Deep-dive research into each source_code/ folder

RESEARCH ORDER (by architectural priority):
1. GenLib/       ← P0 Foundation - shared IPC, memory, sockets
2. GenSql/       ← P0 Foundation - DB access, TableTab[] schema
3. SqlServer/    ← P1 Core server
4. As400UnixServer/ ← P1 AS/400 bridge
5. As400UnixClient/ ← P2
6. As400UnixDocServer/ ← P2
7. As400UnixDoc2Server/ ← P2
8. PharmTcpServer/ ← P2 C++ TCP
9. PharmWebServer/ ← P2 C++ Web
10. ShrinkPharm/ ← P3
11. Served_includes/ + Served_source/ ← P3 C++ HTTP framework

SKIP: FatherProcess/ (done), Include/ (inventoried)

METHOD:
- Read EVERY .c and .h file in each folder
- Document exact line counts, functions, dependencies
- Cite file:line for all claims
- Redact secret values (note location only)

OUTPUT: RESEARCH/{ComponentName}_deepdive.md for each
FINAL: RESEARCH/cross_reference_matrix.md
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

[2026-02-03] ORC:
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
🚨 PRIORITY DISPATCH: RES-SHRINKPHARM-001
PRIORITY: P0 - CLIENT REQUEST - START NOW
PAUSE: RES-DEEPDIVE-001 (resume after ShrinkPharm complete)

TARGET: source_code/ShrinkPharm/
PURPOSE: ODBC database shrink/cleanup utility for MS-SQL Pharmacy

FILE INVENTORY (verified):
| File | Lines | Purpose |
|------|-------|---------|
| ShrinkPharm.c | 431 | Main utility - ODBC database shrink tool |
| MacODBC_MyOperators.c | 133 | ODBC operator helpers |
| MacODBC_MyCustomWhereClauses.c | 10 | Custom SQL clauses |
| Makefile | ~20 | Build configuration |
| **TOTAL** | **574** | Small utility component |

RESEARCH OBJECTIVES:
1. Purpose Analysis - What does it shrink? What tables? What criteria?
2. Function Inventory - ALL functions with file:line citations
3. Database Operations - ODBC calls, SQL queries, tables accessed
4. Dependencies - Headers, GenLib/GenSql usage
5. Configuration - Args, env vars, setup params
6. Cross-References - Relationship to FatherProcess, PharmTcpServer

OUTPUT: RESEARCH/ShrinkPharm_deepdive.md
PIPELINE: RES-SHRINKPHARM-001 → CH-SHRINK-001 → DOC-SHRINK-001
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

[2026-02-13] ORC:
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
🚨 PRIORITY DISPATCH: RES-GENLIB-001
PRIORITY: P0 - CLIENT REQUEST - START NOW

TARGET: source_code/GenLib/ (~10,258 lines, 6 source files)
PURPOSE: Foundation library — IPC, shared memory, semaphores, process lifecycle
CRITICAL: Every server component depends on GenLib (InitSonProcess, sockets, etc.)

FILE INVENTORY:
- SharedMemory.cpp: 4,774 lines (C++ shared memory — NOTE: unusual C++ in C codebase)
- Memory.c: 2,195 lines (process lifecycle, InitSonProcess, shared memory)
- Sockets.c: 1,758 lines (Unix-domain socket IPC, "named pipes")
- GeneralError.c: 770 lines (error handling utilities)
- Semaphores.c: 734 lines (System V semaphore operations)
- GxxPersonality.c: 27 lines (platform stub)

KEY HEADERS (all files include Global.h; Memory.c also includes GenSql.h):
- source_code/Include/Global.h
- source_code/Include/GenSql.h
- source_code/Include/CCGlobal.h (SharedMemory.cpp)
- source_code/Include/MsgHndlr.h (SharedMemory.cpp)

OUTPUT: RESEARCH/GenLib_deepdive.md
PIPELINE: RES-GENLIB-001 → CH-GENLIB-001 → DOC-GENLIB-001
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
```

### To Chunker (CH)
```
[2026-02-02] RES:
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
STATUS: RES-SHRINKPHARM-001 COMPLETE
OUTPUT: RESEARCH/ShrinkPharm_deepdive.md

Exact counts verified (source files only): 574 lines total
- ShrinkPharm.c: 431
- MacODBC_MyOperators.c: 133
- MacODBC_MyCustomWhereClauses.c: 10

Key anchors (see deep-dive for full citations):
- Purge plan driven by DB table shrinkpharm (purge_enabled <> 0); each row supplies table_name/date_column_name/days_to_retain/commit_count.
- Deletes execute via DELETE ... WHERE CURRENT OF ShrinkPharmSelCur with CommitAllWork batching; ODBC_PRESERVE_CURSORS is set TRUE in ShrinkPharm.

NEXT: You can start CH-SHRINK-001 now.
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
```

### To Chunker (CH)
```
[2026-02-02] ORC:
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
STATUS: READY - Can start CH-FATHER-001
TARGET: source_code/FatherProcess/
FILES: FatherProcess.c (~1970 lines), MacODBC_MyOperators.c
PLUGIN: c_plugin.yaml
STRATEGY: Function-level chunking (large file)
OUTPUT: CHUNKS/FatherProcess/
OPTIONAL: Wait for RES-CONTEXT-001 for better context
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

[2026-02-02] RES → CH:
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
CONTEXT HIGHLIGHTS for CH-FATHER-001:
KEY ANCHORS (cite these when chunking):
- shm table schema: source_code/GenSql/GenSql.c (TableTab[])
- IPC named socket "pipe": source_code/GenLib/Sockets.c (GetCurrNamedPipe, FILE_MESG/DATA_MESG)
- child registration: source_code/GenLib/Memory.c (InitSonProcess/AddCurrProc)
- boot sequence: source_code/FatherProcess/FatherProcess.c (CreateSemaphore/InitFirstExtent/CreateTable loop)
DB: ODBC env vars drive MAIN_DB/ALT_DB selection (INF_CONNECT in GenSql.c)
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

[2026-02-02] RES → CH:
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
RES-CONTEXT-001 COMPLETE
READ THESE FIRST:
- RESEARCH/context_summary.md
- RESEARCH/header_inventory.md
- RESEARCH/component_profiles.md

Chunking notes:
- FatherProcess startup: SIGTERM handler + SQLMD_connect retry + CreateSemaphore + InitFirstExtent + CreateTable loop over TableTab[].
- IPC: ListenSocketNamed/ConnectSocketNamed are AF_UNIX stream sockets; message payloads can be FILE_MESG or DATA_MESG.
- Shared memory schema: TableTab[] in GenSql.c defines the canonical shm tables used across the system.

Security note (do not copy values into chunks/docs):
- source_code/Include/TikrotRPC.h and source_code/Include/global_1.h contain hard-coded secret values.
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

[2026-02-02] CHUNKER:
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
STATUS: CH-FATHER-001 COMPLETE

RESULTS:
- Chunks: 13
- Lines: 2144
- Files: 4 (FatherProcess.c, MacODBC_MyOperators.c, MacODBC_MyCustomWhereClauses.c, Makefile)
- Strategy: Function-level with main() sub-chunking (6 sections)

OUTPUT: CHUNKS/FatherProcess/
- repository.json (all chunks with metadata)
- graph.json (dependency relationships)
- analysis.json (codebase statistics)
- run_manifest.json (execution metadata)
- DOCUMENTER_INSTRUCTIONS.md (handoff document)

KEY FINDINGS:
- Boot sequence: SQLMD_connect -> CreateSemaphore -> InitFirstExtent -> CreateTable -> run_system
- main() is 1323 lines - sub-chunked into 6 logical sections
- Multi-instance support via InstanceControl array
- Named pipes are Unix-domain sockets (AF_UNIX)

READY FOR: DOC-FATHER-001
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

[2026-02-03] CHUNKER:
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
STATUS: CH-SHRINK-001 COMPLETE

RESULTS:
- Chunks: 4
- Lines: 600 (574 source + 26 build)
- Files: 4 (ShrinkPharm.c, MacODBC_MyOperators.c, MacODBC_MyCustomWhereClauses.c, Makefile)
- Strategy: File-level (small component with 2 functions)

OUTPUT: CHUNKS/ShrinkPharm/
- repository.json (all 4 chunks with metadata)
- graph.json (dependency relationships)
- analysis.json (codebase statistics)
- run_manifest.json (execution metadata)
- DOCUMENTER_INSTRUCTIONS.md (handoff document)

KEY FINDINGS:
- Standalone DB housekeeping utility - NOT a long-running server
- Purges old rows from target tables based on shrinkpharm control table config
- DELETE WHERE CURRENT OF with batch CommitAllWork; ODBC_PRESERVE_CURSORS=1
- Does NOT use InitSonProcess() - connects directly via SQLMD_connect
- DEADLOCK_PRIORITY=-2 (below normal) to privilege real-time operations

CODE QUALITY NOTES:
- TerminateHandler logs "As400UnixServer" instead of "ShrinkPharm" (copy-paste artifact)
- Unused globals: need_rollback, recs_to_commit, recs_committed

SECURITY NOTE:
- Dynamic SQL built from shrinkpharm table values - document risk but do NOT copy table names

READY FOR: DOC-SHRINK-001
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

[2026-02-11] RES → CH:
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
STATUS: RES-MACODBC-001 COMPLETE
OUTPUT: RESEARCH/MacODBC_deepdive.md

MacODBC.h exact line count: 4121
Author: Don Radlauer, December 2019
Purpose: ODBC infrastructure replacing Informix embedded SQL (ESQL)

KEY ANCHORS (cite these when chunking):
- File structure: hybrid header/implementation under #ifdef MAIN (two blocks: 168-273 globals, 422-4120 functions)
- 4 enums: ODBC_DatabaseProvider (104-111), ODBC_CommandType (117-139), tag_bool (145-151), ODBC_ErrorCategory (339-344)
- 2 structs: ODBC_DB_HEADER (163), ODBC_ColumnParams (164)
- 25 API wrapper macros at 311-335 (cursor, exec, txn, isolation, error)
- 11 implemented functions, largest: ODBC_Exec 446-2657 (2212 lines)
- ODBC_Exec phases: command decode (634-763) → first-call init (780-847) → operation metadata (899-1072) → sticky lifecycle (1086-1231) → SQL construction (1269-1301) → PREPARE/Bind/Execute/Fetch/Commit/Rollback/Close (1341-2640)
- Mirroring: ODBC_MIRRORING_ENABLED + MAIN_DB/ALT_DB dual-DB support (918, 994-997, 1002, 2023-2034, 2268-2283)
- Sticky statements: max 120 (ODBC_MAX_STICKY_STATEMENTS), arrays at 225-236, admission at 1145-1159
- SIGSEGV pointer validation: setjmp/longjmp at 4026-4042, handler at 4086-4095, bind checks at 1524/1560/1595
- Auto-reconnect: error conversion to DB_CONNECTION_BROKEN at 3938-3959
- Per-component SQL injection: MacODBC_MyOperators.c included at 2745-2748, MacODBC_MyCustomWhereClauses.c at 2864-2867
- DB providers: Informix + MS-SQL have runtime logic; DB2/Oracle enum-only

CHUNKING GUIDANCE:
- ODBC_Exec (2212 lines) MUST be sub-chunked semantically (NOT arbitrary line splits)
- Use mixed strategy: section-level for header/enums/structs/macros/globals, function-level for implementations
- Each per-component include injection point (MacODBC_MyOperators.c, MacODBC_MyCustomWhereClauses.c) is a cross-reference boundary
- Mirroring paths should be tagged in chunks that contain them

SECURITY NOTE (do not copy values into chunks/docs):
- DB credentials passed to ODBC_CONNECT (username, password) at 3444-3448
- Env vars MS_SQL_ODBC_USER/MS_SQL_ODBC_PASS sourced in GenSql.c:1088-1091
- Commented diagnostic with clear-text credential placeholders at 3550

NEXT: Start CH-MACODBC-001 now.
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

[2026-02-11] ORC:
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
🚨 PRIORITY DISPATCH: CH-MACODBC-001
PRIORITY: P0 - CLIENT PRIORITY - START NOW
TARGET: source_code/Include/MacODBC.h (4121 lines)
RESEARCH: RESEARCH/MacODBC_deepdive.md (RES-MACODBC-001 ✅)

STRATEGY: Mixed section-level + function-level chunking
SPECIAL: ODBC_Exec (446-2657) must be sub-chunked semantically into ~8 phases

REQUIRED CONTEXT FILES:
- source_code/Include/MacODBC_MyOperatorIDs.h
- source_code/Include/GenSql.h
- source_code/Include/GenSql_ODBC_OperatorIDs.h
- source_code/Include/GenSql_ODBC_Operators.c
- source_code/*/MacODBC_MyOperators.c (all components)
- source_code/*/MacODBC_MyCustomWhereClauses.c (all components)
- source_code/GenSql/GenSql.c (MAIN_DB/ALT_DB + mirroring config)

OUTPUT: CHUNKS/MacODBC/
- repository.json, graph.json, analysis.json, run_manifest.json, DOCUMENTER_INSTRUCTIONS.md

PIPELINE: RES-MACODBC-001 ✅ → CH-MACODBC-001 🟡 → DOC-MACODBC-001 ⏸️
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

[2026-02-11] CHUNKER:
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
STATUS: CH-MACODBC-001 COMPLETE

RESULTS:
- Chunks: 25
- Lines: 4121
- Files: 1 (MacODBC.h — hybrid header/implementation)
- Strategy: Mixed section-level (7 header chunks) + function-level (10 helper chunks) + ODBC_Exec sub-chunking (8 phases)

OUTPUT: CHUNKS/MacODBC/
- repository.json (all 25 chunks with full metadata)
- graph.json (25 nodes, 24 edges, 3 groups, 5 external dependencies)
- analysis.json (statistics, complexity assessment, key patterns)
- run_manifest.json (execution metadata)
- DOCUMENTER_INSTRUCTIONS.md (handoff with 6-phase documentation order)

KEY FINDINGS:
- Single dispatcher pattern: 25 API macros → ODBC_Exec (2212 lines, sub-chunked into 8 semantic phases)
- Database mirroring spans 6 sub-chunks (010c through 010h) — mirror executes BEFORE primary
- Sticky statement cache: max 120 prepared statements with admission control and prepared-state validation
- SIGSEGV pointer validation: sigsetjmp/siglongjmp probing of va_arg pointers — highly unusual pattern
- Auto-reconnect: error conversion to DB_CONNECTION_BROKEN triggers full re-init on next call
- Include injection: MacODBC_MyOperators.c at line 2747, MacODBC_MyCustomWhereClauses.c at line 2866
- Provider abstraction: runtime SQL token rewriting bridges Informix and MS-SQL syntax differences

QUALITY:
- All 4121 lines read across 14 passes; all chunk boundaries verified against source
- Token range: 120-3700 (18 of 25 in target range; 7 header chunks below 500 — justified by semantic distinctness)
- Zero chunks above 4000-token maximum

READY FOR: DOC-MACODBC-001
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

[2026-02-13] CHUNKER:
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
STATUS: CH-SQL-001 COMPLETE

RESULTS:
- Chunks: 62
- Lines: 83,983 (13 files)
- Strategy: Multi-file semantic chunking (by execution phase, handler group, transaction family, utility domain, operator section)
- Parallel mapping agents: 5 (verified function boundaries in all large files)

CHUNK DISTRIBUTION:
- SqlServer.c: 9 chunks (main server loop and dispatch)
- SqlHandlers.c: 8 chunks (1xxx handlers + DUR/OD engine)
- ElectronicPr.c: 12 chunks (2xxx/5xxx handlers)
- DigitalRx.c: 7 chunks (6xxx handlers)
- MessageFuncs.c: 13 chunks (business logic utilities)
- MacODBC_MyOperators.c + CustomWhere: 7 chunks (SQL operators)
- Supporting infrastructure: 6 chunks

KEY FINDINGS:
- "Big Three" sale handlers: HandlerToMsg_6003 (7928 lines), HandlerToMsg_5003 (5946 lines), HandlerToMsg_2003 (4066 lines) = 21.4% of codebase
- Shared validation pipeline: IS_PHARMACY_OPEN_X → test_special_prescription → test_purchase_limits → test_interaction_and_overdose → test_pharmacy_ishur → test_mac_doctor_drugs_electronic → CheckHealthAlerts → update_doctor_presc
- 6 chunks flagged NEEDS_SUB_CHUNKING (oversized handler functions)
- 6 verification items (VER-SQL-001 through VER-SQL-006)
- JSON dual-mode support in newer transactions (6001, 6011, 5061, 6102, 6103)
- REST/CURL integration (newest, Apr 2025)

OUTPUT: CHUNKS/SqlServer/
- repository.json (62 chunks with full metadata)
- graph.json (62 nodes, 81 edges, 7 groups)
- analysis.json (statistics, 10 patterns, 6 verification items)
- run_manifest.json (execution metadata)
- DOCUMENTER_INSTRUCTIONS.md (9-phase documentation order)

READY FOR: DOC-SQL-001
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
```

### To Documenter (DOC)
```
[2026-02-02] ORC:
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
STATUS: BLOCKED
WAITING FOR: CH-FATHER-001 completion
ACTION: Stand by - do not start until chunks exist
NEXT TASK: DOC-FATHER-001 (when unblocked)
REQUIREMENT: 100/100 validation score mandatory
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

[2026-02-02] DOCUMENTER:
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
STATUS: DOC-FATHER-001 COMPLETE

RESULTS:
- Files: 7 documentation files created
- Score: 100/100
- Forbidden words: 0
- Careful language: 52 occurrences
- Source lines verified: 2140 (4 files)
- Functions documented: 6

OUTPUT: Documentation/FatherProcess/
- 01_PROGRAM_SPECIFICATION.md
- 02_SYSTEM_ARCHITECTURE.md
- 03_TECHNICAL_ANALYSIS.md
- 04_BUSINESS_LOGIC.md
- 05_CODE_ARTIFACTS.md
- README.md
- VALIDATION_REPORT.md

READY FOR: Client review
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

[2026-02-03] DOCUMENTER:
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
STATUS: DOC-SHRINK-001 COMPLETE

RESULTS:
- Files: 7 documentation files created
- Score: 100/100
- Forbidden words: 0
- Careful language: 55 occurrences
- Source lines verified: 574 (3 source files + 1 build file = 600 total)
- Functions documented: 2 (main, TerminateHandler)
- SQL Operators documented: 5

OUTPUT: Documentation/ShrinkPharm/
- 01_PROGRAM_SPECIFICATION.md
- 02_SYSTEM_ARCHITECTURE.md
- 03_TECHNICAL_ANALYSIS.md
- 04_BUSINESS_LOGIC.md
- 05_CODE_ARTIFACTS.md
- README.md
- VALIDATION_REPORT.md

CODE QUALITY NOTES DOCUMENTED:
- Copy-paste artifact: TerminateHandler logs "As400UnixServer" (ShrinkPharm.c:388-390, 426-429)
- Unused globals: need_rollback, recs_to_commit, recs_committed (ShrinkPharm.c:30-32)

READY FOR: Client review
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

[2026-02-11] ORC:
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
STATUS: BLOCKED - WAITING FOR CH-MACODBC-001
NEXT TASK: DOC-MACODBC-001
TARGET: MacODBC.h ODBC infrastructure documentation
DEPENDS: CH-MACODBC-001 (Chunker must complete first)
CONTEXT: RESEARCH/MacODBC_deepdive.md available now
REQUIREMENT: 100/100 validation score mandatory
SCOPE: API macros, globals, enums/structs, function internals (ODBC_Exec phases),
       mirroring, reconnect, pointer safety, sticky statements, include chain
NOTE: This is NOT a typical component — it's infrastructure. Documentation must
      explain both the public API (macros) and the internal implementation.
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

[2026-02-11] DOCUMENTER:
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
STATUS: DOC-MACODBC-001 COMPLETE

RESULTS:
- Files: 7 documentation files created (Hebrew)
- Score: 100/100
- Forbidden words: 0
- Careful language: 76 occurrences
- File:line references: 76
- Source lines verified: 4121 (1 file, 25 chunks)
- Functions documented: 11
- API macros documented: 25
- Consuming components documented: 8

OUTPUT: Documentation/MacODBC/
- 01_PROGRAM_SPECIFICATION.md
- 02_SYSTEM_ARCHITECTURE.md
- 03_TECHNICAL_ANALYSIS.md
- 04_BUSINESS_LOGIC.md
- 05_CODE_ARTIFACTS.md
- README.md
- VALIDATION_REPORT.md

SPECIAL COVERAGE:
- ODBC_Exec 8-phase pipeline fully documented
- Database mirroring mechanism (mirror-first strategy)
- Sticky statement cache (max 120)
- SIGSEGV pointer validation (setjmp/longjmp)
- Auto-reconnect (3 recovery paths)
- Per-component SQL injection (8 consumers)
- Security notes (credentials documented by location only)

READY FOR: Client review
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

[2026-02-11] ORC:
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
🚨 CORRECTION: DOC-MACODBC-002
PRIORITY: P0 - LANGUAGE FIX - START NOW

ISSUE: DOC-MACODBC-001 was produced in Hebrew. Default language is ENGLISH.
Hebrew is only produced on explicit user request.

ACTION: Rewrite all 7 MacODBC documentation files in English.
INPUT: Same chunks + research as DOC-MACODBC-001. Use existing Hebrew docs as structural reference.
OUTPUT: Documentation/MacODBC/ (OVERWRITE with English versions)
REQUIREMENT: 100/100 validation score, all content in English
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

[2026-02-11] DOCUMENTER:
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
STATUS: DOC-MACODBC-002 COMPLETE

RESULTS:
- Files: 7 documentation files created (English — overwrote Hebrew)
- Score: 100/100
- Forbidden words: 0
- Careful language: 80 occurrences
- File:line references: 76+
- Source lines verified: 4121 (1 file, 25 chunks)
- Functions documented: 11
- API macros documented: 25
- Consuming components documented: 8
- Language: ENGLISH (corrects Hebrew DOC-MACODBC-001)

OUTPUT: Documentation/MacODBC/
- 01_PROGRAM_SPECIFICATION.md
- 02_SYSTEM_ARCHITECTURE.md
- 03_TECHNICAL_ANALYSIS.md
- 04_BUSINESS_LOGIC.md
- 05_CODE_ARTIFACTS.md
- README.md
- VALIDATION_REPORT.md

SPECIAL COVERAGE:
- ODBC_Exec 8-phase pipeline fully documented
- Database mirroring mechanism (mirror-first strategy)
- Sticky statement cache (max 120)
- SIGSEGV pointer validation (setjmp/longjmp)
- Auto-reconnect (3 recovery paths)
- Per-component SQL injection (8 consumers)
- Security notes (credentials documented by location only)

READY FOR: Client review
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

[2026-02-13] ORC:
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
🚨 PRIORITY DISPATCH: DOC-SQL-001
PRIORITY: P0 - CLIENT PRIORITY - START NOW
TARGET: SqlServer — largest component (~84K lines, 13 source files, 62 chunks)
DEPENDS: CH-SQL-001 ✅ (CHUNKS/SqlServer/ ready with 62 chunks)
RESEARCH: RESEARCH/SqlServer_deepdive.md + RESEARCH/MacODBC_deepdive.md
SPECS: source_documents/ (21 business specification files)

DOCUMENTATION ORDER: Follow CHUNKER's 9-phase order in DOCUMENTER_INSTRUCTIONS.md
SUB-CHUNKING: 6 oversized chunks need phase-level documentation (see guidance in instructions)

OUTPUT: Documentation/SqlServer/ (ENGLISH — default)
REQUIREMENT: 100/100 validation score mandatory
LANGUAGE: English first. After user approves, ask about Hebrew version.

CRITICAL QUALITY RULES:
- Every claim must cite file:line
- 6 verification items (VER-SQL-001 through VER-SQL-006) must be documented
- No secret values in output (location-only notes)
- No speculative behavior claims
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

[2026-02-13] DOCUMENTER:
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
STATUS: DOC-SQL-001 COMPLETE

RESULTS:
- Files: 7 documentation files created (English)
- Score: 100/100
- Forbidden words: 0
- Careful language: consistent throughout
- Source chunks verified: 62/62 (100%)
- Phases completed: 9/9 (100%)
- Sub-chunked oversized chunks: 6/6 (100%)
- Cross-cutting concerns: 5/5 (100%)
- Verification items documented: 6/6 (100%)
- Language: ENGLISH

OUTPUT: Documentation/SqlServer/
- 01_PROGRAM_SPECIFICATION.md
- 02_SYSTEM_ARCHITECTURE.md
- 03_TECHNICAL_ANALYSIS.md
- 04_BUSINESS_LOGIC.md
- 05_CODE_ARTIFACTS.md
- README.md
- VALIDATION_REPORT.md

SPECIAL COVERAGE:
- Big Three sale handlers sub-chunked (TR2003: 3 phases, TR5003: 4 phases, TR6003: 4 phases)
- 8-stage validation pipeline with handler coverage matrix
- DUR/OD engine architecture with decision tables
- AS/400 dual integration (TikrotRPC + Meishar TCP)
- 265 ODBC operators cataloged with table mapping
- 2 dynamic SQL holes documented (VER-SQL-004, VER-SQL-005)
- REST/CURL integration (newest addition, Apr 2025)
- Security notes (credentials documented by location only)

READY FOR: Client review
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
```

### To Recommender (REC)
```
[2026-02-02] ORC:
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
STATUS: INACTIVE - DO NOT ACTIVATE
GATE: All DOC-* complete + validated + CLIENT APPROVAL
ACTION: None until further notice
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
```

---

## ACTIVE SPRINT

**Sprint Goal:** GenLib CIDRA Pipeline (~10K lines, 6 source files — Foundation Library)

| Priority | Task | Owner | Status | Target |
|----------|------|-------|--------|--------|
| P0 | RES-GENLIB-001 | Researcher | 🔵 ACTIVE | RESEARCH/GenLib_deepdive.md |
| P0 | CH-GENLIB-001 | Chunker | 📋 PLANNED (depends on RES-GENLIB-001) | CHUNKS/GenLib/ |
| P0 | DOC-GENLIB-001 | Documenter | 📋 PLANNED (depends on CH-GENLIB-001) | Documentation/GenLib/ |
| -- | RES-DEEPDIVE-001 | Researcher | ⏸️ PAUSED | Resume after GenLib pipeline |

**Completed (all sessions):**
| Task | Status | Output |
|------|--------|--------|
| RES-CONTEXT-001 | ✅ COMPLETE | RESEARCH/*.md |
| RES-SHRINKPHARM-001 | ✅ COMPLETE | RESEARCH/ShrinkPharm_deepdive.md |
| RES-MACODBC-001 | ✅ COMPLETE | RESEARCH/MacODBC_deepdive.md |
| CH-FATHER-001 | ✅ COMPLETE | CHUNKS/FatherProcess/ |
| DOC-FATHER-001 | ✅ COMPLETE | Documentation/FatherProcess/ + FatherProcess_Hebrew/ (100/100) |
| CH-SHRINK-001 | ✅ COMPLETE | CHUNKS/ShrinkPharm/ |
| CH-MACODBC-001 | ✅ COMPLETE | CHUNKS/MacODBC/ (25 chunks) |
| DOC-SHRINK-001 | ✅ COMPLETE | Documentation/ShrinkPharm/ + ShrinkPharm_Hebrew/ (100/100) |
| DOC-MACODBC-001 | ✅ COMPLETE | Documentation/MacODBC_Hebrew/ (100/100, Hebrew) |
| DOC-MACODBC-002 | ✅ COMPLETE | Documentation/MacODBC/ (100/100, English) |
| RES-SQL-001 | ✅ COMPLETE | RESEARCH/SqlServer_deepdive.md + Merged Baseline |
| CH-SQL-001 | ✅ COMPLETE | CHUNKS/SqlServer/ (62 chunks, 83,983 lines) |
| DOC-SQL-001 | ✅ COMPLETE | Documentation/SqlServer/ (100/100, English) |

---

## SHARED RESOURCES

### Configuration Files
| File | Purpose |
|------|---------|
| `.cidra-config.json` | Project configuration |
| `c_plugin.yaml` | C language plugin |

### Output Directories
| Directory | Owner | Purpose |
|-----------|-------|---------|
| `RESEARCH/` | Researcher | Context documentation |
| `CHUNKS/` | Chunker | Code chunks |
| `Documentation/{Component}/` | Documenter | 7-file documentation (ENGLISH — default) |
| `Documentation/{Component}_Hebrew/` | Documenter | 7-file documentation (HEBREW — only if user approves) |
| `RECOMMENDATIONS/` | Recommender | Modernization plans |

### Language Convention
- **Default:** English (`Documentation/{Component}/`)
- **Hebrew:** Only after user approval (`Documentation/{Component}_Hebrew/`)
- Documenter MUST ask user after English docs are approved: "Would you like me to create a Hebrew version as well?"

---

## ANTI-HALLUCINATION MANDATE

**ALL AGENTS MUST:**
1. COUNT EXACTLY - Use commands, never estimate
2. CITE SOURCES - Reference file:line for claims
3. VERIFY AFTER EDIT - Read file after writing
4. NO ASSUMPTIONS - Mark uncertain items
5. CAREFUL LANGUAGE - "appears that", "according to code"

---

## STATUS LEGEND

| Symbol | Meaning |
|--------|---------|
| 🔵 | Ready / Active |
| 🟡 | In Progress |
| ✅ | Complete |
| ⏸️ | Blocked |
| ⛔ | Inactive |
| 📋 | Planned |
| ❌ | Failed |
| ⬜ | Not Started |

---

*Maintained by Orc. Last sync: 2026-02-15 (GenLib pipeline ACTIVE — RES-GENLIB-001 🔵, CH-GENLIB-001 📋, DOC-GENLIB-001 📋. SqlServer pipeline COMPLETE. Hebrew: deferred until client reviews English)*
