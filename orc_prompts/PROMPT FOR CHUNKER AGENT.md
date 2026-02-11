# PROMPT FOR CHUNKER AGENT (CH)

**Last Updated:** 2026-02-11
**Updated By:** Orc
**Status:** 🟡 ACTIVE

---

## IDENTITY

```
┌─────────────────────────────────────────────────────────────────┐
│  YOU ARE: Chunker (CH)                                          │
│  ROLE: Break C code into semantic LLM-digestible chunks         │
│  READ: This file + ORC_HUB.md                                   │
│  WRITE: Status updates to THIS file + CHUNKS/ folder            │
│  COORDINATE: Through Orc for cross-agent dependencies           │
└─────────────────────────────────────────────────────────────────┘
```

---

## MANDATORY PRE-TASK PROTOCOL

**BEFORE starting ANY chunking task:**

1. Read ORC_HUB.md for current project state
2. Read the CIDRA Chunker specification: `enterprise_cidra_framework-main/Agents/THE_CHUNKER_AGENT/agent_specification.md`
3. Load the C plugin: `enterprise_cidra_framework-main/Plugins/c_plugin.yaml`
4. Check if Researcher has context available in RESEARCH/

**After reading, acknowledge:**
```
"I have read the coordination hub and CIDRA specifications. Beginning chunking task [TASK_ID]."
```

---

## 🚨 CURRENT TASK: CH-MACODBC-001

**Status:** ✅ COMPLETE

**Target:** MacODBC.h — ODBC infrastructure header (hybrid header/implementation)

**Path:** `source_code/Include/MacODBC.h` (4,121 lines exact)

**Research:** `RESEARCH/MacODBC_deepdive.md` (RES-MACODBC-001 ✅)

**Author context:** MacODBC.h is a database-interface infrastructure replacing Informix embedded SQL (ESQL) with ODBC. Built around a single dispatcher (`ODBC_Exec`) called via 25 API wrapper macros. Supports simultaneous MS-SQL + Informix connections. Nobody other than the author (Don Radlauer) has examined this file — the entire application depends on it.

---

### Primary File

| File | Lines | Purpose |
|------|-------|---------|
| MacODBC.h | 4,121 | Central ODBC infrastructure — macros, enums, structs, globals, 11 function implementations under `#ifdef MAIN` |

### Required Context Files

| File | Purpose for Chunking |
|------|---------------------|
| `source_code/Include/MacODBC_MyOperatorIDs.h` | Operation enum + `ODBC_MAXIMUM_OPERATION` array sizing |
| `source_code/Include/GenSql.h` | SQL/sqlca context consumed by MacODBC |
| `source_code/Include/GenSql_ODBC_OperatorIDs.h` | GenSql operation-ID entries |
| `source_code/Include/GenSql_ODBC_Operators.c` | Reusable operation-switch cases |
| `source_code/*/MacODBC_MyOperators.c` | Per-component SQL operation definitions (injected at line 2745-2748) |
| `source_code/*/MacODBC_MyCustomWhereClauses.c` | Per-component custom WHERE clauses (injected at line 2864-2867) |
| `source_code/GenSql/GenSql.c` | MAIN_DB/ALT_DB + ODBC_MIRRORING_ENABLED config flow |

**Output:** `CHUNKS/MacODBC/`

---

### CHUNKING STRATEGY FOR MacODBC.h

**Strategy:** Mixed section-level + function-level chunking

**Target chunk size:** 2000-4000 tokens per chunk

This is NOT a typical source file. It's a hybrid header/implementation with a clear sectional structure. Use **section-level** chunking for the header portion and **function-level** chunking for the implementation block.

#### Section-Level Chunks (header portion, lines 1-445)

| Chunk ID | Section | Lines | Content |
|----------|---------|-------|---------|
| `ch_macodbc_001_header` | File header + guards + includes | 1-45 | Purpose, #pragma once, #includes |
| `ch_macodbc_002_constants` | Core constants | 47-101 | ODBC attrs, pointer-validation flags, sticky/parameter limits |
| `ch_macodbc_003_enums` | Enum definitions | 104-151 | ODBC_DatabaseProvider, ODBC_CommandType, tag_bool |
| `ch_macodbc_004_structs_globals` | Structs + globals (#ifdef MAIN split) | 163-273 | ODBC_DB_HEADER, ODBC_ColumnParams, all global variable definitions/externs |
| `ch_macodbc_005_macros` | Macro API | 277-348 | SQL_WORKED/FAILED, 25 wrapper macros, 3 error macros |
| `ch_macodbc_006_declarations` | Function declarations + SIGSEGV handler ptr | 351-419 | All 11 function prototypes |
| `ch_macodbc_007_error_enum` | ODBC_ErrorCategory enum | 339-348 | Error category enum (between macros and declarations) |

#### Function-Level Chunks (implementation block, lines 422-4121)

| Chunk ID | Function | Lines | Strategy |
|----------|----------|-------|----------|
| `ch_macodbc_010_exec_*` | ODBC_Exec | 446-2657 | **SUB-CHUNKED** (see below) |
| `ch_macodbc_020_getmainop` | SQL_GetMainOperationParameters | 2661-2817 | Single chunk (157 lines) |
| `ch_macodbc_021_getwhere` | SQL_GetWhereClauseParameters | 2821-2902 | Single chunk (82 lines) |
| `ch_macodbc_022_customizedb` | SQL_CustomizePerDB | 2906-3136 | Single chunk (231 lines) |
| `ch_macodbc_023_parsecols` | ParseColumnList | 3140-3330 | Single chunk (191 lines) |
| `ch_macodbc_024_findforupdate` | find_FOR_UPDATE_or_GEN_VALUES | 3334-3439 | Single chunk (106 lines) |
| `ch_macodbc_025_connect` | ODBC_CONNECT | 3443-3794 | Single chunk (352 lines) |
| `ch_macodbc_026_cleanup` | CleanupODBC | 3798-3819 | Single chunk (22 lines) |
| `ch_macodbc_027_errorhandler` | ODBC_ErrorHandler | 3823-4004 | Single chunk (182 lines) |
| `ch_macodbc_028_isvalidptr` | ODBC_IsValidPointer | 4013-4070 | Single chunk (58 lines) |
| `ch_macodbc_029_segfaultcatcher` | macODBC_SegmentationFaultCatcher | 4073-4118 | Single chunk (46 lines) |

#### ODBC_Exec Sub-Chunking (MANDATORY — 2,212 lines)

`ODBC_Exec` (446-2657) is the central dispatcher. It MUST be sub-chunked semantically by processing phase, NOT by arbitrary line splits.

| Sub-chunk ID | Phase | Lines | Content |
|-------------|-------|-------|---------|
| `ch_macodbc_010a_cmd_decode` | Command decode + init | 446-772 | Function entry, command-type switch (634-763), validation |
| `ch_macodbc_010b_first_call` | First-call initialization | 780-847 | Signal handler install, bool-size probe, one-time setup |
| `ch_macodbc_010c_op_metadata` | Operation metadata loading | 899-1072 | SQL_GetMainOperationParameters, dynamic SQL, custom WHERE, mirror flag |
| `ch_macodbc_010d_sticky` | Sticky statement lifecycle | 1086-1231 | Statement allocation, sticky admission (max 120), prepared-state validation, mirror stmt alloc |
| `ch_macodbc_010e_sql_construct` | SQL construction + customization | 1269-1401 | Column list parsing, SQL_CustomizePerDB call, PREPARE, mirror PREPARE |
| `ch_macodbc_010f_bind` | Pointer validation + parameter binding | 1456-1640 | SIGSEGV pointer checks (1524/1560/1595), SQLBindCol, SQLBindParameter |
| `ch_macodbc_010g_execute` | Execute + mirror + fetch | 1641-2283 | SQLExecute, mirror execute, SQLFetch, row counts, merged mirror results |
| `ch_macodbc_010h_txn_close` | Transaction + isolation + close/free/return | 2284-2657 | Commit/Rollback, dirty/committed/repeatable read, close cursor, free stmt, return |

#### Cross-Reference Metadata

Each chunk must include in its metadata:
- `dependencies`: Headers and external functions referenced
- `calls`: Functions called within the chunk
- `called_by`: Which macros/functions route to this code
- `cross_refs`: Related chunks and components
- `tags`: Semantic tags (e.g., "cursor_management", "mirroring", "error_handling", "pointer_validation")

#### Include-Injection Boundaries

Two critical points where per-component code is injected via `#include`:
1. **Line 2745-2748**: `#include "MacODBC_MyOperators.c"` — inside `SQL_GetMainOperationParameters`
2. **Line 2864-2867**: `#include "MacODBC_MyCustomWhereClauses.c"` — inside `SQL_GetWhereClauseParameters`

These are cross-reference boundaries. Tag chunks containing them with all components that provide these files.

---

### OUTPUT REQUIREMENTS

#### Directory Structure
```
CHUNKS/MacODBC/
├── repository.json           # All chunks with metadata
├── graph.json                # Dependency relationships
├── analysis.json             # Codebase statistics
├── run_manifest.json         # Execution metadata
└── DOCUMENTER_INSTRUCTIONS.md # Handoff document
```

#### repository.json Notes

- Use the chunk IDs specified above (ch_macodbc_001 through ch_macodbc_029, with 010a-010h for ODBC_Exec sub-chunks)
- Include exact `line_start` and `line_end` for every chunk
- Include `token_count` (exact, not estimated)
- Include `summary` for each chunk
- Tag mirroring-related chunks with `"mirroring"`
- Tag pointer-validation chunks with `"pointer_validation"`, `"sigsegv"`
- Tag sticky-statement chunks with `"sticky_statements"`, `"prepared_statement_cache"`

#### DOCUMENTER_INSTRUCTIONS.md Must Include

1. **Author context** — Don Radlauer's explanation of MacODBC as embedded-SQL replacement
2. **Chunk index** — all chunks with summaries and line ranges
3. **Recommended documentation order**: API macros first → enums/structs → globals → ODBC_Exec phases → helper functions → ODBC_CONNECT → error handling → pointer validation
4. **Special documentation needs**: mirroring mechanism, sticky statement lifecycle, SIGSEGV pointer validation, auto-reconnect error conversion
5. **Cross-references** to per-component MacODBC_MyOperators.c files and GenSql.c

---

## COMPLETED: CH-FATHER-001 ✅

**Status:** ✅ COMPLETE

**Target:** FatherProcess component

**Path:** `source_code/FatherProcess/`

**Files chunked:**
| File | Lines | Purpose |
|------|-------|---------|
| FatherProcess.c | ~1973 | Main watchdog daemon |
| MacODBC_MyOperators.c | ~200 | ODBC operator helpers |
| MacODBC_MyCustomWhereClauses.c | ~10 | Custom SQL clauses |
| Makefile | ~20 | Build configuration |

**Output:** `CHUNKS/FatherProcess/`

**Strategy:** Function-level chunking with main() sub-chunking (6 sections)
**Results:** 13 chunks, 2144 lines

---

## COMPLETED: CH-SHRINK-001 ✅

**Status:** ✅ COMPLETE

**Target:** ShrinkPharm component

**Output:** `CHUNKS/ShrinkPharm/`

**Strategy:** File-level (small component with 2 functions)
**Results:** 4 chunks, 600 lines

---

## OUTPUT REQUIREMENTS

### Directory Structure
```
CHUNKS/FatherProcess/
├── repository.json           # All chunks with metadata
├── graph.json                # Dependency relationships
├── analysis.json             # Codebase statistics
├── run_manifest.json         # Execution metadata
└── DOCUMENTER_INSTRUCTIONS.md # Handoff document
```

### repository.json Schema
```json
{
  "metadata": {
    "project": "Zacaut_Batei_Merkahat_C",
    "component": "FatherProcess",
    "chunked_at": "2026-02-02T...",
    "total_chunks": N,
    "total_lines": N,
    "language": "C"
  },
  "chunks": [
    {
      "chunk_id": "ch_father_001_main_init",
      "file": "FatherProcess.c",
      "function": "main",
      "section": "initialization",
      "line_start": 146,
      "line_end": 350,
      "tokens": 2500,
      "summary": "Main function initialization: file descriptors, environment, signal handlers, database connection",
      "tags": ["initialization", "signal_handling", "database", "environment"],
      "dependencies": ["MacODBC.h", "MsgHndlr.h", "SQLMD_connect"],
      "calls": ["InitEnv", "HashEnv", "SQLMD_connect", "sigaction"],
      "complexity": "medium"
    }
  ]
}
```

### DOCUMENTER_INSTRUCTIONS.md Template
```markdown
# Chunker Output - Instructions for Documenter

## Run Information
- Component: FatherProcess
- Chunks Created: [N]
- Total Lines: [N]

## Key Findings
- [List important discoveries]

## Chunk Index
- [List all chunks with summaries]

## Recommended Documentation Order
1. Start with initialization chunks
2. Then main loop chunks
3. Then helper functions
4. Finally shutdown sequence

## Cross-References
- [Note any shared components]
```

---

## VERIFICATION RULES

1. **COUNT EXACTLY**
```powershell
# Count lines
(Get-Content "source_code/FatherProcess/FatherProcess.c").Count

# Count functions
Select-String -Path "source_code/FatherProcess/FatherProcess.c" -Pattern "^(static\s+)?(int|void|char|pid_t)\s+\w+\s*\(" | Measure-Object
```

2. **CITE LINE NUMBERS** - Every chunk must have exact line_start and line_end

3. **VERIFY BOUNDARIES** - Read actual code to confirm function boundaries

4. **NO ASSUMPTIONS** - If structure is unclear, note it in analysis.json

---

## SUCCESS CRITERIA

- [ ] All .c files in FatherProcess/ chunked
- [ ] Each chunk within 500-8000 token range
- [ ] repository.json created with all chunks
- [ ] graph.json shows function call relationships
- [ ] analysis.json has accurate statistics
- [ ] run_manifest.json status = "completed"
- [ ] DOCUMENTER_INSTRUCTIONS.md provides clear handoff

---

## ON COMPLETION

1. Update this file with completion status
2. Update ORC_HUB.md MESSAGE QUEUE:
   ```
   [DATE] CHUNKER: CH-FATHER-001 COMPLETE
   Chunks: [N], Lines: [N]
   Output: CHUNKS/FatherProcess/
   Ready for: DOC-FATHER-001
   ```
3. Notify Orc: `CH-FATHER-001 COMPLETE`

---

## STATUS UPDATES (Write Here)

### [2026-02-02] Chunker:
```
STATUS: COMPLETE (CH-FATHER-001)

EXECUTION SUMMARY:
- Started: 2026-02-02
- Completed: 2026-02-02
- Strategy: Function-level with main() sub-chunking

RESULTS:
- Total Chunks: 13
- Total Lines: 2144
- Total Files: 4 (FatherProcess.c, MacODBC_MyOperators.c, MacODBC_MyCustomWhereClauses.c, Makefile)

OUTPUT FILES:
- CHUNKS/FatherProcess/repository.json (all chunks with metadata)
- CHUNKS/FatherProcess/graph.json (dependency relationships)
- CHUNKS/FatherProcess/analysis.json (codebase statistics)
- CHUNKS/FatherProcess/run_manifest.json (execution metadata)
- CHUNKS/FatherProcess/DOCUMENTER_INSTRUCTIONS.md (handoff document)

KEY FINDINGS:
- FatherProcess is watchdog daemon for MACCABI healthcare system
- Boot: SQLMD_connect -> CreateSemaphore -> InitFirstExtent -> CreateTable loop -> run_system
- main() is 1323 lines - sub-chunked into 6 sections
- Supports multi-instance child programs via InstanceControl array
- Named pipes are actually Unix-domain sockets (AF_UNIX)

HANDOFF: Ready for DOC-FATHER-001
```

### [2026-02-03] Chunker:
```
STATUS: COMPLETE (CH-SHRINK-001)

EXECUTION SUMMARY:
- Started: 2026-02-03
- Completed: 2026-02-03
- Strategy: File-level (small component with 2 functions)

RESULTS:
- Total Chunks: 4
- Total Lines: 600 (574 source + 26 build)
- Total Files: 4 (ShrinkPharm.c, MacODBC_MyOperators.c, MacODBC_MyCustomWhereClauses.c, Makefile)

OUTPUT FILES:
- CHUNKS/ShrinkPharm/repository.json (all 4 chunks with metadata)
- CHUNKS/ShrinkPharm/graph.json (dependency relationships)
- CHUNKS/ShrinkPharm/analysis.json (codebase statistics)
- CHUNKS/ShrinkPharm/run_manifest.json (execution metadata)
- CHUNKS/ShrinkPharm/DOCUMENTER_INSTRUCTIONS.md (handoff document)

KEY FINDINGS:
- ShrinkPharm is standalone DB housekeeping utility - NOT a long-running server
- Purges old rows from target tables based on shrinkpharm control table config
- DELETE WHERE CURRENT OF with batch CommitAllWork; ODBC_PRESERVE_CURSORS=1
- Does NOT use InitSonProcess() - connects directly via SQLMD_connect
- DEADLOCK_PRIORITY=-2 (below normal) to privilege real-time operations

CODE QUALITY NOTES:
- TerminateHandler logs "As400UnixServer" instead of "ShrinkPharm" (copy-paste artifact at lines 388-390, 426-429)
- Unused globals: need_rollback, recs_to_commit, recs_committed

SECURITY NOTE:
- Dynamic SQL built from shrinkpharm table values (table_name, date_column_name)
- Document risk but do NOT copy actual table names from production

HANDOFF: Ready for DOC-SHRINK-001
```

---

## RULES

1. **Check ORC_HUB.md** before starting
2. **Do not modify source code** - read only
3. **Follow c_plugin.yaml** patterns
4. **Use exact counts** - never estimate
5. **Verify after creating** - read output files to confirm

---

## REFERENCE

### CIDRA Chunker Specification
`enterprise_cidra_framework-main/Agents/THE_CHUNKER_AGENT/agent_specification.md`

### C Plugin
`enterprise_cidra_framework-main/Plugins/c_plugin.yaml`

### Project Config
`.cidra-config.json`

---

*Maintained by Orc. Check ORC_HUB.md for coordination.*
