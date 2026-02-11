# PROMPT FOR RESEARCHER AGENT (RES)

**Last Updated:** 2026-02-11
**Updated By:** Orc
**Status:** 🔵 ACTIVE

---

## IDENTITY

```
┌─────────────────────────────────────────────────────────────────┐
│  YOU ARE: Researcher (RES)                                      │
│  ROLE: Investigate codebase, gather context, answer questions   │
│  READ: This file + ORC_HUB.md                                   │
│  WRITE: Status updates to THIS file                             │
│  COORDINATE: Through Orc for cross-agent information sharing    │
└─────────────────────────────────────────────────────────────────┘
```

---

## MANDATORY PRE-TASK PROTOCOL

**BEFORE starting ANY research task:**

1. Read ORC_HUB.md for current project state
2. Check if other agents need your findings
3. Plan your investigation approach
4. Document your methodology

**After reading, acknowledge:**
```
"I have read the coordination hub. Beginning research task [TASK_ID]."
```

---

## COMPLETED: RES-CONTEXT-001 ✅

**Status:** ✅ COMPLETE
**Output:** RESEARCH/context_summary.md, header_inventory.md, component_profiles.md

---

## 🚨 PRIORITY TASK: RES-SHRINKPHARM-001

**Status:** ✅ COMPLETE

**Goal:** Deep-dive research on ShrinkPharm component (client priority)

**Priority:** P0 - Client wants this component documented next

**Target:** `source_code/ShrinkPharm/`

---

## RES-SHRINKPHARM-001: SHRINKPHARM RESEARCH

### File Inventory (verified)

| File | Lines | Purpose |
|------|-------|---------|
| ShrinkPharm.c | 431 | Main utility - ODBC database shrink/cleanup |
| MacODBC_MyOperators.c | 133 | ODBC operator helpers |
| MacODBC_MyCustomWhereClauses.c | 10 | Custom SQL WHERE clauses |
| Makefile | ~20 | Build configuration |
| **TOTAL** | **574** | Small utility component |

### Research Objectives

1. **Purpose Analysis**
   - What is "ShrinkPharm" vs "ShrinkDoctor"?
   - What database records does it delete/shrink?
   - What tables does it operate on?
   - What is the deletion criteria (date-based? count-based?)?

2. **Function Inventory**
   - List ALL functions with exact file:line citations
   - Document main() flow
   - Document TerminateHandler signal handling

3. **Database Operations**
   - What ODBC calls are made?
   - What SQL queries are executed (DELETE, SELECT)?
   - What tables are accessed?
   - Transaction handling (commit patterns, rollback)

4. **Dependencies**
   - Headers included (especially MacODBC.h usage)
   - GenLib/GenSql functions used
   - External system calls

5. **Configuration**
   - Command line arguments
   - Environment variables
   - Setup table parameters (if any)

6. **Cross-References**
   - Relationship to FatherProcess (is it spawned as child?)
   - Relationship to PharmTcpServer/PharmWebServer
   - Relationship to SqlServer component

### Output

Create: `RESEARCH/ShrinkPharm_deepdive.md`

```markdown
# ShrinkPharm - Deep Dive

## Overview
[What this utility does - from code analysis]

## File Inventory
| File | Lines | Purpose |
|------|-------|---------|
(exact counts verified)

## Key Functions
| Function | File:Line | Purpose |
|----------|-----------|---------|
(cite exact line numbers)

## Database Operations
- Tables accessed (with SQL patterns)
- Query types (DELETE, SELECT)
- Transaction handling

## Dependencies
- Headers included
- Library functions used

## Cross-References
- Called by (which components)
- Relationship to other components

## Configuration
- Arguments/parameters
- Environment dependencies

## Security Notes
- Credentials (location only, redact values)

## Open Questions
- Unclear sections
```

### Quality Gates

- [ ] All 3 .c files read completely
- [ ] Exact line counts verified (574 total)
- [ ] All functions listed with file:line citations
- [ ] Database tables identified
- [ ] ODBC call patterns documented
- [ ] Cross-references to other components identified

---

## COMPLETED: RES-MACODBC-001 ✅

**Status:** ✅ COMPLETE

**Goal:** Deep-dive research on MacODBC.h — the central ODBC infrastructure header for the entire system

**Priority:** P0 — Client priority. This file is the ODBC backbone for every server component. Must be fully understood before CIDRA documentation pipeline can produce accurate documentation.

**Target:** `source_code/Include/MacODBC.h` (4,121 lines exact)

**Context:** MacODBC.h is NOT a typical header. It contains both declarations AND full function implementations (guarded by `#ifdef MAIN`). It is the single most complex file in the entire codebase. Every server component includes it. The existing `RESEARCH/header_inventory.md` mentions it in only 2 lines — that is insufficient.

---

## RES-MACODBC-001: MacODBC.h DEEP-DIVE RESEARCH

### Why This Matters

MacODBC.h is the **ODBC abstraction layer** for the MACCABI healthcare backend. It wraps raw ODBC API calls into a macro-based interface used by every component (FatherProcess, SqlServer, As400UnixServer, As400UnixClient, As400UnixDocServer, As400UnixDoc2Server, ShrinkPharm). Without understanding this file, no component documentation can be accurate.

### Author's Context (from Don Radlauer, the sole author of MacODBC.h)

> **CRITICAL BACKGROUND — Read this before examining the code:**
>
> MacODBC.h is a **database-interface infrastructure** that provides functionality similar to **embedded SQL** in C programs. It was built specifically because the system needed to transition away from the previous version, which used **Informix with "ESQL" code** (embedded SQL compiled via a precompiler from `.ec` → `.c` files). **MS-SQL does not support embedded SQL — but it does support ODBC.** This is why MacODBC.h exists.
>
> The system is built around a **single main function `ODBC_Exec`**, but calling routines use a variety of **macros** (defined in MacODBC.h) such as `ExecSql` and `DeclareAndOpenCursorInto` to accomplish what they need. It has support for **both MS-SQL and Informix**, and could easily be extended to deal with any other database that has a (reasonably) standard ODBC interface. It currently supports **simultaneous connections to two databases**; it has been used with both Informix and MS-SQL active at the same time.
>
> **Example — the old embedded SQL:**
> ```c
> EXEC SQL
>      SELECT     MIN(contracttype), MIN(profession)
>                  INTO :ConMembRec.ContractType, :ConMembRec.Profession
>                  FROM contract
>                  WHERE contractor = :ConMembRec.Contractor;
> ```
> A precompiler would convert this from `xxxx.ec` to `xxxx.c`, with the SQL code altered to talk directly to Informix.
>
> **The same operation with MacODBC.h:**
> ```c
> ExecSql (MAIN_DB, GetContractTypeProfession,
>    &ConMembRec.ContractType,     &ConMembRec.Profession,
>    &ConMembRec.Contractor,    END_OF_ARG_LIST            );
> ```
> The actual SQL query is defined in an include file — each program has its own `MacODBC_MyOperators.c` that has what's needed to interpret `GetContractTypeProfession`. The conversion from embedded SQL to the new infrastructure was basically a **one-to-one operation**, and the new infrastructure provides pretty much identical functionality.
>
> **Documentation goal:** Explain to someone what MacODBC.h does and how it works — including **error handling and problem recovery**. Nobody other than Don has looked at MacODBC.h since he wrote it — and the entire application depends on it.

### File Target

| File | Path | Lines |
|------|------|-------|
| MacODBC.h | `source_code/Include/MacODBC.h` | **4,121** (verified via `(Get-Content "source_code/Include/MacODBC.h").Count`) |

**IMPORTANT:** This file is very large. Read it in sections (500-700 lines at a time). Do NOT attempt to read the entire file in one pass.

### Research Objectives

#### 1. File Structure Map

Create a **section-by-section map** of the file showing what is defined where. The file has a complex structure with preprocessor guards. Document:

- Where `#ifdef MAIN` begins and ends (line numbers)
- Where declarations live vs. where implementations live
- The overall layout: includes → defines → enums → structs → macros → extern globals → function implementations

#### 2. Enum Definitions (COMPLETE inventory)

Find and document EVERY enum in the file. Known enums include:

| Enum | Purpose | Approx Location |
|------|---------|-----------------|
| `ODBC_DatabaseProvider` | DB vendor types (Informix, DB2, MS-SQL, Oracle) | Early in file |
| `ODBC_CommandType` | SQL operation types (SELECT, INSERT, UPDATE, DELETE, etc.) | Early in file |
| `ODBC_ErrorCategory` | Error classification categories | Early in file |

For each enum:
- Exact line number where it starts/ends
- List ALL values with their names
- Document any comments explaining values

#### 3. Struct Definitions (COMPLETE inventory)

Find and document EVERY struct. Known structs include:

| Struct | Purpose | Approx Location |
|--------|---------|-----------------|
| `ODBC_DB_HEADER` | Database connection state | Early in file |
| `ODBC_ColumnParams` | Column metadata for queries | Early in file |

For each struct:
- Exact line number where it starts/ends
- List ALL fields with types and purposes
- Document any embedded structs or unions

#### 4. Macro API (COMPLETE inventory — ~25 macros)

This is the **most critical section**. The entire codebase accesses ODBC through these macros. Document EVERY macro defined in MacODBC.h.

Known macros (verify and expand):

| Macro | Purpose | Category |
|-------|---------|----------|
| `DeclareCursor` | Declare an ODBC cursor for SELECT | Cursor management |
| `FetchCursor` | Fetch next row from cursor | Cursor management |
| `CloseCursor` | Close an open cursor | Cursor management |
| `ExecSQL` | Execute SQL statement | Execution |
| `ExecSQLWithParams` | Execute with bound parameters | Execution |
| `CommitWork` | Commit transaction | Transaction |
| `CommitAllWork` | Commit all pending work | Transaction |
| `RollbackWork` | Rollback transaction | Transaction |
| `SQLMD_connect` / `ODBC_CONNECT` | Open database connection | Connection |
| `CleanupODBC` | Cleanup ODBC resources | Connection |
| `BindCol` | Bind result column | Parameter binding |
| `BindParameter` | Bind input parameter | Parameter binding |

For each macro:
- Exact `#define` line number
- Parameters (names and types)
- What it expands to (which function/ODBC call)
- Usage pattern (how components call it)

#### 5. Global Variables (`#ifdef MAIN` section)

When `MAIN` is defined, MacODBC.h declares and initializes global variables. When `MAIN` is NOT defined, they are declared as `extern`. Document:

- Every global variable name
- Its type
- Initial value (when MAIN is defined)
- Purpose
- Line numbers for both the `MAIN` definition and the `extern` declaration

#### 6. Function Implementations (COMPLETE inventory)

When `MAIN` is defined, the following functions are implemented INSIDE MacODBC.h. This is the bulk of the file (~3,500+ lines).

**Known functions to document:**

| Function | Approx Lines | Purpose |
|----------|-------------|---------|
| `ODBC_Exec` | ~3,300 lines | **THE central dispatcher** — routes all ODBC operations. This is the largest single function. |
| `ODBC_CONNECT` | ~100-200 lines | Open/reopen database connection |
| `CleanupODBC` | ~50-100 lines | Free ODBC handles and resources |
| `ODBC_ErrorHandler` | ~100-200 lines | Process ODBC errors, classify, log |
| `ODBC_IsValidPointer` | ~20-50 lines | Validate pointer before ODBC use (SIGSEGV protection) |
| `macODBC_SegmentationFaultCatcher` | ~20-50 lines | Signal handler for SIGSEGV during pointer validation |

For each function:
- Exact start line and end line
- Exact line count (end - start + 1)
- Function signature (return type, parameters)
- Purpose and behavior summary
- Key internal logic:
  - For `ODBC_Exec`: Document the `switch` cases for each `ODBC_CommandType`
  - For `ODBC_CONNECT`: Document connection string construction, retry logic
  - For `ODBC_ErrorHandler`: Document error classification scheme
  - For `ODBC_IsValidPointer`: Document the SIGSEGV/setjmp/longjmp trick

**ALSO document these declared-but-not-defined functions** (implemented in per-component files):

| Function | Declared Where | Implemented In |
|----------|---------------|----------------|
| `SQL_GetMainOperationParameters` | MacODBC.h | Each component's `MacODBC_MyOperators.c` |
| `SQL_GetWhereClauseParameters` | MacODBC.h | Each component's `MacODBC_MyOperators.c` |
| `SQL_CustomizePerDB` | MacODBC.h | Each component's `MacODBC_MyOperators.c` |
| `ParseColumnList` | MacODBC.h | Per-component or GenSql |
| `find_FOR_UPDATE_or_GEN_VALUES` | MacODBC.h | Per-component or GenSql |

For each:
- Exact declaration line in MacODBC.h
- Function signature
- Purpose (from comments or context)

#### 7. Key `#define` Constants

Document the important constants that control system behavior:

| Constant | Purpose | Approx Value |
|----------|---------|-------------|
| `ODBC_MAX_STICKY_STATEMENTS` | Max cached prepared statements | 120 |
| `ODBC_MAX_PARAMETERS` | Max bound parameters per query | Verify |
| `ODBC_MAX_COLUMNS` | Max result columns | Verify |
| `ODBC_PRESERVE_CURSORS` | Flag for cursor preservation across commits | 1 |
| `ODBC_MAX_RETRIES` | Connection retry count | Verify |

For each:
- Exact `#define` line number
- Exact value
- Where/how it is used

#### 8. Database Provider Support

MacODBC.h supports multiple database vendors. Document:

- Which providers are supported: Informix, DB2, MS-SQL Server, Oracle
- How provider selection works (enum value → connection string → ODBC driver)
- Provider-specific SQL dialect handling (if any)
- Provider-specific error handling (if any)

#### 9. Database Mirroring Mechanism

Document:

- What is the mirroring concept? (MAIN_DB / ALT_DB / mirror)
- How does failover work?
- Which functions implement mirroring?
- Configuration mechanism (env vars, #defines)

#### 10. Sticky Statement Management

Document:

- What are "sticky statements"? (Prepared statement caching)
- How are they stored? (Array? Hash? Linked list?)
- Max count (ODBC_MAX_STICKY_STATEMENTS = 120)
- Lifecycle: creation → reuse → eviction
- Which functions manage them?

#### 11. Pointer Validation with SIGSEGV Handling

Document:

- Purpose: Why does ODBC code need pointer validation?
- Mechanism: setjmp/longjmp + SIGSEGV signal handler
- Functions: `ODBC_IsValidPointer`, `macODBC_SegmentationFaultCatcher`
- When is it triggered? (Before SQLBindParameter calls?)

#### 12. Auto-Reconnect on Connection Errors

Document:

- When does auto-reconnect trigger?
- How many retries?
- Backoff strategy (if any)?
- Which errors trigger reconnect vs. which are fatal?

#### 13. Related Files (Cross-References)

Document the relationship between MacODBC.h and these related files:

| File | Relationship |
|------|-------------|
| `source_code/Include/MacODBC_MyOperatorIDs.h` | Operator ID constants used by MacODBC |
| `source_code/Include/GenSql_ODBC_OperatorIDs.h` | GenSql-level operator IDs |
| `source_code/Include/GenSql_ODBC_Operators.c` | GenSql operator implementations |
| Each component's `MacODBC_MyOperators.c` | Per-component operator callbacks |
| Each component's `MacODBC_MyCustomWhereClauses.c` | Per-component WHERE clause builders |
| `source_code/Include/GenSql.h` | GenSql header (TableTab[], cursor macros) |

For each:
- How it connects to MacODBC.h
- What it provides that MacODBC.h consumes (or vice versa)

#### 14. Include Chain

Document exactly what MacODBC.h `#include`s, and what `#include`s MacODBC.h:

**MacODBC.h includes:**
- List every `#include` with line number

**Files that include MacODBC.h:**
- Search across all components for `#include.*MacODBC`
- List every file that includes it

### Reading Strategy

Because MacODBC.h is 4,121 lines, read it in ordered sections:

```
Pass 1: Lines 1-600      — File header, includes, defines, enums, structs
Pass 2: Lines 600-1200   — Macros, extern declarations, global variables
Pass 3: Lines 1200-1800  — Start of ODBC_Exec function
Pass 4: Lines 1800-2400  — ODBC_Exec continued (switch cases)
Pass 5: Lines 2400-3000  — ODBC_Exec continued
Pass 6: Lines 3000-3600  — ODBC_Exec completion, other functions start
Pass 7: Lines 3600-4121  — Remaining functions, file end (#endif guard)
```

After each pass, document findings before proceeding to the next pass.

### Output

Create: `RESEARCH/MacODBC_deepdive.md`

```markdown
# MacODBC.h — Deep Dive

## Overview
[What this file is, why it exists, its role in the system]

## File Statistics
- Path: source_code/Include/MacODBC.h
- Lines: [exact count]
- Author: [from file header]
- Date: [from file header]

## File Structure Map
| Section | Lines | Content |
|---------|-------|---------|
| Header/Includes | 1-N | ... |
| Defines/Constants | N-M | ... |
| Enums | M-P | ... |
| Structs | P-Q | ... |
| Macros | Q-R | ... |
| Extern Globals | R-S | ... |
| #ifdef MAIN Globals | S-T | ... |
| ODBC_Exec | T-U | ... |
| Other Functions | U-4120 | ... |

## Enum Definitions
### ODBC_DatabaseProvider (line N)
| Value | Name | Meaning |
|-------|------|---------|
(list all values)

### ODBC_CommandType (line N)
(same format)

### ODBC_ErrorCategory (line N)
(same format)

## Struct Definitions
### ODBC_DB_HEADER (line N-M)
| Field | Type | Purpose |
|-------|------|---------|
(list all fields)

### ODBC_ColumnParams (line N-M)
(same format)

## Macro API
### Cursor Management
| Macro | Line | Parameters | Expands To |
|-------|------|-----------|------------|
| DeclareCursor | N | ... | ... |
(list all cursor macros)

### SQL Execution
(same format)

### Transaction Control
(same format)

### Connection Management
(same format)

### Parameter Binding
(same format)

## Global Variables
| Variable | Type | MAIN Value | Purpose | Def Line | Extern Line |
|----------|------|-----------|---------|----------|-------------|
(list all globals)

## Function Implementations
### ODBC_Exec (lines N-M, ~X lines)
**Signature:** `return_type ODBC_Exec(params...)`
**Purpose:** Central ODBC dispatcher
**Switch Cases:**
| Case | CommandType | Lines | Behavior |
|------|-----------|-------|----------|
(list all cases)
**Key Logic:**
- [Describe major decision points]

### ODBC_CONNECT (lines N-M)
**Signature:** ...
**Purpose:** ...
**Connection Flow:**
1. ...

### CleanupODBC (lines N-M)
(same format)

### ODBC_ErrorHandler (lines N-M)
(same format)

### ODBC_IsValidPointer (lines N-M)
(same format)

### macODBC_SegmentationFaultCatcher (lines N-M)
(same format)

## Declared-But-Not-Defined Functions
| Function | Declaration Line | Signature | Implemented By |
|----------|-----------------|-----------|----------------|
(list all with cross-references)

## Key Constants
| Constant | Value | Line | Purpose |
|----------|-------|------|---------|
(list all significant #defines)

## Database Provider Support
| Provider | Enum Value | Driver | Dialect Notes |
|----------|-----------|--------|---------------|
(list all providers)

## Database Mirroring
- Mechanism: [how it works]
- Configuration: [env vars, defines]
- Failover logic: [describe]

## Sticky Statement Management
- Storage: [data structure]
- Max count: [value]
- Lifecycle: [create → reuse → evict]

## Pointer Validation (SIGSEGV)
- Purpose: [why needed]
- Mechanism: [setjmp/longjmp + signal handler]
- Functions involved: [list]

## Auto-Reconnect
- Trigger: [which errors]
- Retry count: [value]
- Backoff: [strategy]

## Include Chain
### MacODBC.h includes:
| Line | Include |
|------|---------|
(list all #include directives)

### Files that include MacODBC.h:
| Component | File |
|-----------|------|
(list all files across all components)

## Related Files
| File | Relationship to MacODBC.h |
|------|--------------------------|
(list all related files)

## Security Notes
- [Credential handling patterns — location only, redact values]
- [Input validation patterns]

## Open Questions
- [Anything unclear after reading the full file]
```

### Quality Gates

- [x] Full file read (all 4,121 lines, in 7 passes)
- [x] Exact line count verified via `(Get-Content "source_code/Include/MacODBC.h").Count` = 4121
- [ ] File structure map with exact line ranges
- [ ] ALL enums documented with ALL values
- [ ] ALL structs documented with ALL fields
- [ ] ALL macros documented (~25 expected) with line numbers
- [ ] ALL global variables documented with both MAIN and extern lines
- [ ] ALL implemented functions documented with exact start/end lines
- [ ] ALL declared-but-not-defined functions listed with declaration lines
- [ ] ALL #define constants documented with exact values
- [ ] Database provider support documented
- [ ] Mirroring mechanism documented
- [ ] Sticky statement management documented
- [ ] Pointer validation (SIGSEGV) mechanism documented
- [ ] Auto-reconnect mechanism documented
- [ ] Include chain documented (both directions)
- [ ] Related files cross-referenced
- [ ] Security-sensitive locations noted (values redacted)
- [ ] Zero forbidden words without verification (no "approximately", "typically", "seems like", etc.)
- [ ] Every claim has file:line citation

### Anti-Hallucination Rules (CRITICAL)

1. **DO NOT GUESS** line numbers. Read the file and cite exactly what you see.
2. **DO NOT ASSUME** function behavior from names. Read the implementation.
3. **DO NOT ESTIMATE** counts. Use `(Get-Content ...).Count` for line counts.
4. **DO NOT SKIP** sections of the file. Read all 4,121 lines across 7 passes.
5. **IF UNCERTAIN** about any finding, mark it as `[NEEDS VERIFICATION]` — never fill gaps with plausible-sounding content.
6. **VERIFY AFTER WRITING** — After creating MacODBC_deepdive.md, read it back to confirm accuracy.

### On Completion

1. Update this file (PROMPT FOR RESEARCHER AGENT.md) with a status update in the STATUS UPDATES section
2. Include in status update:
   - Exact line count (verified)
   - Count of enums, structs, macros, functions found
   - Any surprising findings
   - Any open questions
3. Notify Orc: `RES-MACODBC-001 COMPLETE`

---

## PAUSED TASK: RES-DEEPDIVE-001

**Status:** ⏸️ PAUSED - Resume after RES-SHRINKPHARM-001 complete

**Goal:** Deep-dive research into each source_code/ component, folder by folder

**Priority:** P1 - Chunker needs detailed context for each component

**Approach:** Read ALL files in each folder. Comprehend thoroughly before moving to next.

---

## RES-DEEPDIVE-001: FOLDER-BY-FOLDER RESEARCH PLAN

### Research Order (by architectural priority)

| Phase | Folder | Priority | Reason |
|-------|--------|----------|--------|
| 1 | GenLib/ | P0 | Foundation - shared IPC, memory, sockets |
| 2 | GenSql/ | P0 | Foundation - DB access, TableTab[] schema |
| 3 | SqlServer/ | P1 | Core server - SQL handling |
| 4 | As400UnixServer/ | P1 | AS/400 mainframe bridge |
| 5 | As400UnixClient/ | P2 | AS/400 client component |
| 6 | As400UnixDocServer/ | P2 | Doctor system server |
| 7 | As400UnixDoc2Server/ | P2 | Doctor system server 2 |
| 8 | PharmTcpServer/ | P2 | C++ Pharmacy TCP |
| 9 | PharmWebServer/ | P2 | C++ Pharmacy Web |
| 10 | ShrinkPharm/ | P3 | Utility |
| 11 | Served_includes/ | P3 | C++ HTTP framework |
| 12 | Served_source/ | P3 | C++ shared source |

**SKIP:** FatherProcess/ (already researched), Include/ (already inventoried)

### For Each Folder, Document:

```markdown
## [ComponentName] - Deep Dive

### File Inventory
| File | Lines | Purpose |
|------|-------|---------|
(exact counts using PowerShell)

### Key Functions
| Function | File:Line | Purpose |
|----------|-----------|---------|
(cite exact line numbers)

### Data Structures
- Structs defined
- Global variables
- Constants/macros

### Dependencies
- Headers included
- GenLib functions used
- GenSql functions used
- External libraries

### IPC/Communication
- Socket usage
- Shared memory access
- Semaphore usage

### Database Access
- Tables accessed
- Query patterns
- ODBC calls

### Cross-References
- Called by (which components)
- Calls (which components)

### Security Notes
- Any credentials (location only, redact values)
- Input validation patterns

### Open Questions
- Unclear code sections
- Needs clarification
```

### Output Files

For each component, create:
```
RESEARCH/
├── GenLib_deepdive.md
├── GenSql_deepdive.md
├── SqlServer_deepdive.md
├── As400UnixServer_deepdive.md
├── As400UnixClient_deepdive.md
├── As400UnixDocServer_deepdive.md
├── As400UnixDoc2Server_deepdive.md
├── PharmTcpServer_deepdive.md
├── PharmWebServer_deepdive.md
├── ShrinkPharm_deepdive.md
├── Served_deepdive.md
└── cross_reference_matrix.md   ← Final cross-component analysis
```

### Quality Gates

- [ ] Every .c and .h file in folder read
- [ ] Exact line counts verified
- [ ] All functions listed with file:line citations
- [ ] Dependencies mapped
- [ ] IPC patterns documented
- [ ] Security-sensitive locations noted (values redacted)
- [ ] Cross-references to other components identified

---

## RESEARCH SCOPE

### Project: Zacaut_Batei_Merkahat_C

**Technology:** C (Legacy UNIX system)
**Domain:** Healthcare (MACCABI)
**Components:** 7 server applications + shared libraries

```
source_code/
├── FatherProcess/      ← Watchdog/supervisor daemon
├── SqlServer/          ← SQL server component
├── As400UnixServer/    ← AS/400 mainframe bridge
├── As400UnixClient/    ← AS/400 client
├── As400UnixDocServer/ ← Doctor system AS/400 server
├── As400UnixDoc2Server/← Additional doctor server
├── PharmTcpServer/     ← Pharmacy TCP server
├── PharmWebServer/     ← Pharmacy web server
├── GenLib/             ← General shared library
├── GenSql/             ← SQL utility library
├── Include/            ← Header files
├── Served_includes/    ← Additional headers
└── Served_source/      ← Shared source files
```

---

## PHASE 1: HEADER ANALYSIS

### Task: Analyze Include/ directory

**Files to examine:**
```
source_code/Include/
source_code/Served_includes/
```

**Questions to answer:**

| # | Question | Output |
|---|----------|--------|
| 1 | What is `MacODBC.h`? What functions/macros does it provide? | Document API |
| 2 | What is `MsgHndlr.h`? What message handling system exists? | Document patterns |
| 3 | What data structures are defined? (TABLE, PROC_DATA, STAT_DATA, etc.) | List all structs |
| 4 | What constants/macros are system-wide? | List #defines |
| 5 | What are the subsystems? (PHARM_SYS, DOCTOR_SYS) | Document enum values |

**Method:**
```
1. List all .h files in Include/
2. Read each header file
3. Extract: structs, typedefs, #defines, function declarations
4. Document dependencies between headers
```

---

## PHASE 2: ARCHITECTURE MAPPING

### Task: Understand system architecture

**Questions to answer:**

| # | Question | Output |
|---|----------|--------|
| 1 | How do processes communicate? (IPC mechanisms) | Diagram |
| 2 | What shared memory tables exist? | List with purposes |
| 3 | What is the process hierarchy? (Father → children) | Process tree |
| 4 | What database is used? How is it accessed? | DB access patterns |
| 5 | What external systems does it integrate with? | Integration map |

**Key patterns to identify:**
- Shared memory (shm*, InitFirstExtent, CreateTable)
- Semaphores (sem*, CreateSemaphore)
- Sockets (socket, bind, listen, GetSocketMessage)
- Named pipes (mkfifo, named_pipe)
- Signals (sigaction, SIGTERM, SIGCHLD)
- Database (SQLMD_connect, DeclareCursor, FetchCursor)

---

## PHASE 3: COMPONENT PROFILING

### Task: Profile each component

**For each component in source_code/, document:**

| Field | Description |
|-------|-------------|
| Name | Component name |
| Path | Directory path |
| Main file | Primary .c file |
| Lines | Exact line count |
| Purpose | What does it do? |
| Dependencies | What does it #include? |
| Process type | Daemon/server/library |
| Subsystem | PHARM_SYS / DOCTOR_SYS / Both |

**Start with FatherProcess (highest priority):**
```
FatherProcess.c analysis:
- Line count: (use PowerShell: (Get-Content file).Count)
- Function count: (grep pattern)
- Key functions: main, run_system, sql_dbparam_into_shm, etc.
- Signal handlers: TerminateHandler
- Child process management: fork, waitpid, kill
```

---

## PHASE 4: CROSS-REFERENCE MAP

### Task: Build dependency graph

**Document:**
1. Which components depend on GenLib?
2. Which components depend on GenSql?
3. What headers are shared across all components?
4. What are the critical shared data structures?

**Output format:**
```
GenLib/
├── Used by: FatherProcess, SqlServer, PharmTcpServer, ...
├── Provides: [list functions]
└── Key files: [list .c files]
```

---

## OUTPUT REQUIREMENTS

### Create: RESEARCH/context_summary.md

**Structure:**
```markdown
# MACCABI C Backend - Research Summary

## System Overview
[High-level description]

## Architecture
[Diagram and explanation]

## Components
[Table of all components]

## Shared Resources
[Headers, libraries, data structures]

## IPC Mechanisms
[How processes communicate]

## Database Access
[How DB is accessed]

## Key Findings
[Important discoveries for Chunker/Documenter]

## Open Questions
[Things that need clarification]
```

### Create: RESEARCH/header_inventory.md

**Structure:**
```markdown
# Header File Inventory

## Include/
[List each .h file with purpose]

## Key Structures
[Document each struct]

## Key Macros
[Document important #defines]
```

### Create: RESEARCH/component_profiles.md

**Structure:**
```markdown
# Component Profiles

## FatherProcess
[Full profile]

## SqlServer
[Full profile]

[... etc for each component]
```

---

## VERIFICATION RULES

1. **COUNT EXACTLY** - Use commands, not estimates
2. **CITE SOURCES** - Reference file:line for all claims
3. **NO ASSUMPTIONS** - If uncertain, mark as "needs verification"
4. **READ BEFORE CLAIMING** - Verify file contents before documenting

**Verification commands:**
```powershell
# Count lines
(Get-Content "file.c").Count

# Count functions
Select-String -Path "file.c" -Pattern "^(static\s+)?(int|void|char)" | Measure-Object

# Find includes
Select-String -Path "file.c" -Pattern "#include"

# Find structs
Select-String -Path "file.h" -Pattern "^typedef struct|^struct"
```

---

## SUCCESS CRITERIA

- [ ] All headers in Include/ documented
- [ ] Architecture diagram created
- [ ] All 7 components profiled
- [ ] Cross-reference map complete
- [ ] RESEARCH/ folder created with 3 summary files
- [ ] Key findings shared with Chunker via ORC_HUB.md

---

## DELIVERABLES

| File | Purpose | For Agent |
|------|---------|-----------|
| RESEARCH/context_summary.md | System overview | All |
| RESEARCH/header_inventory.md | Header documentation | Chunker, Documenter |
| RESEARCH/component_profiles.md | Component details | Chunker, Documenter |

---

## ON COMPLETION

1. Update this file with findings summary
2. Update ORC_HUB.md message queue with key findings for Chunker
3. Notify Orc: `RES-CONTEXT-001 COMPLETE`

---

## STATUS UPDATES (Write Here)

### [2026-02-02] Researcher:
```
Task: RES-CONTEXT-001
Status: PLAN READY
Next: Generate RESEARCH/context_summary.md, header_inventory.md, component_profiles.md
Guards: Exact counts + file:line citations; redact secrets (document location only)
KEY ANCHORS:
- shm table schema: source_code/GenSql/GenSql.c (TableTab[])
- IPC named socket "pipe": source_code/GenLib/Sockets.c (GetCurrNamedPipe, FILE_MESG/DATA_MESG)
- child registration: source_code/GenLib/Memory.c (InitSonProcess/AddCurrProc)
- boot sequence: source_code/FatherProcess/FatherProcess.c (CreateSemaphore/InitFirstExtent/CreateTable loop)
DB: ODBC env vars drive MAIN_DB/ALT_DB selection (INF_CONNECT in GenSql.c)
```

### [2026-02-02] Researcher:
```
Task: RES-CONTEXT-001
Status: COMPLETE
Outputs:
- RESEARCH/context_summary.md
- RESEARCH/header_inventory.md
- RESEARCH/component_profiles.md

Key findings (high-signal):
- Boot orchestration: FatherProcess sets SIGTERM handler, connects DB in retry loop, creates semaphore+shared memory, creates tables from TableTab[], registers itself as PHARM_SYS|DOCTOR_SYS.
- IPC: internal “named pipes” are unix-domain sockets (AF_UNIX/SOCK_STREAM) + supports FILE_MESG vs DATA_MESG payload modes.
- Child bootstrap: InitSonProcess opens semaphore, attaches shared memory, listens on per-process named socket, registers into PROC_TABLE.
- DB: SQLMD_connect maps to INF_CONNECT and supports MS-SQL and/or Informix via env vars; MAIN_DB/ALT_DB selected by configuration.

Security note (values redacted in outputs):
- Hard-coded secret values exist in source_code/Include/TikrotRPC.h and source_code/Include/global_1.h (documented by location only).
```

### [2026-02-02] Researcher:
```
Task: RES-SHRINKPHARM-001
Status: COMPLETE
Output:
- RESEARCH/ShrinkPharm_deepdive.md

Exact counts verified:
- source_code/ShrinkPharm/ShrinkPharm.c: 431 lines
- source_code/ShrinkPharm/MacODBC_MyOperators.c: 133 lines
- source_code/ShrinkPharm/MacODBC_MyCustomWhereClauses.c: 10 lines
TOTAL (source files): 574 lines exact

High-signal findings (with citations in the deep-dive):
- ShrinkPharm is an ODBC “ShrinkDoctor-equivalent” for Pharmacy MS-SQL housekeeping.
- Purge plan is driven by DB control table shrinkpharm (purge_enabled <> 0) which supplies table_name/date_column_name/days_to_retain/commit_count.
- Deletes are executed row-by-row via DELETE ... WHERE CURRENT OF ShrinkPharmSelCur, with CommitAllWork() batching and ODBC_PRESERVE_CURSORS=1.
```

### [2026-02-11] Researcher:
```
Task: RES-MACODBC-001
Status: COMPLETE
Output:
- RESEARCH/MacODBC_deepdive.md

Exact counts verified:
- source_code/Include/MacODBC.h: 4121 lines exact
- Enums: 4 (ODBC_DatabaseProvider, ODBC_CommandType, tag_bool, ODBC_ErrorCategory)
- Structs: 2 (ODBC_DB_HEADER, ODBC_ColumnParams)
- #define macros: 42 total (25 ODBC_Exec wrapper API macros)
- Implemented functions in MacODBC.h (#ifdef MAIN): 11
- ODBC_Exec size: 2212 lines (446-2657)

Surprising findings:
- MacODBC.h is a hybrid header/implementation unit with full function bodies under #ifdef MAIN.
- Mirroring/failover logic is live and integrated (MAIN_DB/ALT_DB, mirrored execute, merged rows/code paths).
- SIGSEGV-based pointer validation is deeply integrated in argument binding to avoid process crashes.
- Provider enum includes DB2/Oracle, but concrete provider-specific logic in this file is implemented for Informix and MS-SQL paths.

Open questions:
- Confirm whether DB2/Oracle support is implemented outside MacODBC.h or remains planned-only.
- Confirm whether expected constants ODBC_MAX_COLUMNS / ODBC_MAX_RETRIES are intentionally absent (no definitions in MacODBC.h).

Notify Orc: RES-MACODBC-001 COMPLETE
```

---

## RULES

1. **Check ORC_HUB.md** before starting
2. **Do not modify source code** - read only
3. **Share findings** that help other agents
4. **Document uncertainties** - don't guess
5. **Use exact counts** - never estimate

---

*Maintained by Orc. Check ORC_HUB.md for coordination.*
