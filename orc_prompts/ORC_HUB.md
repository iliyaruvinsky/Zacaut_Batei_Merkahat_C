# ORC HUB - CIDRA Agent Coordination Center

**Last Updated:** 2026-02-02
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
| RES-DEEPDIVE-001 | Folder-by-folder deep dive | 🔵 ACTIVE | RESEARCH/*_deepdive.md |

### CHUNKER (CH) - Stage 0

| ID | Component | Status | Depends On | Output |
|----|-----------|--------|------------|--------|
| CH-FATHER-001 | FatherProcess | ✅ COMPLETE | RES-CONTEXT-001 (optional) | CHUNKS/FatherProcess/ |
| CH-SQL-001 | SqlServer | 📋 PLANNED | - | CHUNKS/SqlServer/ |
| CH-AS400-001 | As400UnixServer | 📋 PLANNED | - | CHUNKS/As400UnixServer/ |
| CH-PHARM-001 | PharmTcpServer | 📋 PLANNED | - | CHUNKS/PharmTcpServer/ |
| CH-GENLIB-001 | GenLib | 📋 PLANNED | - | CHUNKS/GenLib/ |

### DOCUMENTER (DOC) - Stage 1

| ID | Component | Status | Depends On | Output |
|----|-----------|--------|------------|--------|
| DOC-FATHER-001 | FatherProcess | ✅ COMPLETE | CH-FATHER-001 ✅ | Documentation/FatherProcess/ |
| DOC-SQL-001 | SqlServer | 📋 PLANNED | CH-SQL-001 | Documentation/SqlServer/ |
| DOC-AS400-001 | As400UnixServer | 📋 PLANNED | CH-AS400-001 | Documentation/As400UnixServer/ |
| DOC-PHARM-001 | PharmTcpServer | 📋 PLANNED | CH-PHARM-001 | Documentation/PharmTcpServer/ |
| DOC-GENLIB-001 | GenLib | 📋 PLANNED | CH-GENLIB-001 | Documentation/GenLib/ |

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

**Sprint Goal:** FatherProcess chunking + Deep-dive research (parallel)

| Priority | Task | Owner | Status | Target |
|----------|------|-------|--------|--------|
| P0 | RES-CONTEXT-001 | Researcher | ✅ COMPLETE | Research complete |
| P0 | RES-DEEPDIVE-001 | Researcher | 🔵 ACTIVE | Folder-by-folder deep dives |
| P0 | CH-FATHER-001 | Chunker | ✅ COMPLETE | CHUNKS/FatherProcess/ |
| P1 | DOC-FATHER-001 | Documenter | ✅ COMPLETE | Documentation/FatherProcess/ |

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
| `Documentation/` | Documenter | 7-file documentation |
| `RECOMMENDATIONS/` | Recommender | Modernization plans |

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

*Maintained by Orc. Last sync: 2026-02-02*
