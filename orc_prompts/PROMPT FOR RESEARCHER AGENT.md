# PROMPT FOR RESEARCHER AGENT (RES)

**Last Updated:** 2026-02-13
**Updated By:** Orc
**Status:** ✅ COMPLETE — RES-SQL-001 (SqlServer deep dive) | Merged baseline delivered

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

### STEADY PRE-READING (PERMANENT — applies to ALL tasks)

**BEFORE starting ANY research task, ALWAYS read these documents first:**

| # | Document | Path | Why |
|---|----------|------|-----|
| 1 | Coordination Hub | `orc_prompts/ORC_HUB.md` | Current project state, task registry, agent statuses, shared resources |
| 2 | System Context Summary | `RESEARCH/context_summary.md` | System-wide architecture: IPC, shared memory, process hierarchy, DB access patterns |
| 3 | Header Inventory | `RESEARCH/header_inventory.md` | What every header file in `source_code/Include/` provides |
| 4 | Component Profiles | `RESEARCH/component_profiles.md` | Overview of all 7+ server components and their roles |

These 4 documents provide the architectural foundation for understanding ANY component in the system. A fresh agent has ZERO context — these files fill that gap.

### THEN follow these steps:
1. Check if other agents need your findings (see ORC_HUB task registry)
2. Read any **DYNAMIC PRE-READING** listed in the current task section below
3. Plan your investigation approach
4. Document your methodology

**After reading, acknowledge:**
```
"I have read the coordination hub and steady pre-reading documents. Beginning research task [TASK_ID]."
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

## COMPLETED: RES-SQL-001 ✅

**Status:** ✅ COMPLETE — Merged baseline from Agents A/B/C delivered to Orc

**Goal:** Deep-dive research on SqlServer — the **largest and most complex component** in the entire system

**Priority:** P0 — Client priority. SqlServer is the central transaction handler for the MACCABI pharmacy backend. It processes ~60+ transaction codes across 13 source files (~84,000 lines total).

**Target:** `source_code/SqlServer/` (13 files)

**Context:** SqlServer is the primary server process responsible for handling ALL pharmacy transaction types — from shift management (1xxx), to electronic prescription reference tables (2xxx), to Nihul Tikrot subsidy operations (5xxx), to digital prescription operations (6xxx). It communicates with pharmacy terminals via a wire protocol where each transaction has a request (odd code) and response (even code, usually +1).

---

## RES-SQL-001: SqlServer DEEP-DIVE RESEARCH

### ⚠️ DYNAMIC PRE-READING FOR RES-SQL-001 (after steady pre-reading)

After completing the steady pre-reading (see MANDATORY PRE-TASK PROTOCOL above), read these **task-specific** documents before touching any source file in `source_code/SqlServer/`:

#### Step A — MacODBC Infrastructure (CRITICAL)
| Document | Path | Why |
|----------|------|-----|
| MacODBC Deep Dive | `RESEARCH/MacODBC_deepdive.md` | **SqlServer.c defines `#define MAIN` before `#include <MacODBC.h>`** — it is THE compilation unit that instantiates all ODBC functions, globals, and the central ODBC_Exec dispatcher. Without understanding this, you cannot understand any SQL operation in SqlServer. |

#### Step B — Key Headers Used by SqlServer
Read these **source headers** that define the transaction framework:
| Header | Path | Why |
|--------|------|-----|
| MsgHndlr.h | `source_code/Include/MsgHndlr.h` | **THE transaction framework**: message structures, PRESCR_SOURCE, PHARM_TRN_PERMISSIONS, ALL global variables (under `#ifdef MAIN`), prescription data types, DUR interaction structs, member/drug record types. This is ~1000+ lines and defines the entire pharmacy data model. |
| PharmDBMsgs.h | `source_code/Include/PharmDBMsgs.h` | Wire protocol field definitions — transaction request/response message layouts |
| MacODBC_MyOperatorIDs.h | `source_code/Include/MacODBC_MyOperatorIDs.h` | Enum of ALL SQL operator IDs used in MacODBC_MyOperators.c (hundreds of operators). Each operator maps to a SQL query. |
| MessageFuncs.h | `source_code/Include/MessageFuncs.h` | Function prototypes for MessageFuncs.c + key macros (dentist drug tests, participation codes) |
| DBFields.h | `source_code/Include/DBFields.h` | Database field name/type mapping |
| PharmacyErrors.h | `source_code/Include/PharmacyErrors.h` | Error code definitions used throughout all handlers |
| TikrotRPC.h | `source_code/Include/TikrotRPC.h` | Tikrot (Nihul Tikrot) subsidy RPC definitions + **contains credentials (note location only, redact values)** |
| GenSql.h | `source_code/Include/GenSql.h` | GenSql framework: TableTab[], cursor macros, DB initialization |

#### Step C — Existing Partial Research
| Document | Path | Why |
|----------|------|-----|
| Existing SqlServer deep dive (partial) | `RESEARCH/SqlServer_deepdive.md` | **A previous Researcher already started this file** (dated 2026-02-02). It has exact file inventory (22 files including build files = 84,407 lines) and analysis of the 6 smaller files (supporting files). **EXTEND this file — do NOT start from scratch.** The large files (SqlServer.c, SqlHandlers.c, ElectronicPr.c, DigitalRx.c, MessageFuncs.c, MacODBC_MyOperators.c) are marked as "not yet deep-read" and need your analysis. |

#### Step D — Business Context (Transaction Specifications)
The `source_documents/` folder contains **21 specification files** (Hebrew, mostly .docx + 1 .xlsx + 1 .pdf) that define the wire protocol for each transaction. These are the "why" behind the code. **You do NOT need to read all 21 files now** — but you MUST be aware they exist and reference them when analyzing corresponding handlers. The Orc has already mapped specs to handlers in the table below.

**After completing steady + dynamic pre-reading, acknowledge:**
```
"I have completed all pre-reading. Key context loaded:
- System architecture from RESEARCH/*.md (steady)
- MacODBC infrastructure from RESEARCH/MacODBC_deepdive.md
- Transaction framework from MsgHndlr.h
- SQL operator IDs from MacODBC_MyOperatorIDs.h
- Existing partial research from RESEARCH/SqlServer_deepdive.md
Beginning deep-read of SqlServer source files."
```

---

### Why This Matters

SqlServer is the **backbone of the pharmacy backend**. Every pharmacy terminal communicates through it. It routes ~60+ transaction codes to handler functions spread across 4 main source files, uses shared message utilities, and interfaces with both MS-SQL databases and AS/400 mainframes. Understanding this component is prerequisite for accurate documentation.

### File Inventory (verified from previous session)

| File | Lines | Role |
|------|-------|------|
| ElectronicPr.c | 26,161 | Electronic prescription handlers (TR 2xxx) |
| DigitalRx.c | 22,955 | Digital prescription handlers (TR 6xxx) |
| MessageFuncs.c | 13,631 | Shared message handling utilities |
| MacODBC_MyOperators.c | 8,418 | ODBC SQL operators for SqlServer |
| SqlHandlers.c | 8,458 | Core pharmacy handlers (TR 1xxx + misc) |
| SqlServer.c | 2,362 | Main entry, init, dispatch switch |
| As400UnixMediator.c | 660 | AS/400 RPC bridge |
| TikrotRPC.c | 494 | Tikrot (subsidy) RPC calls to AS/400 |
| MacODBC_MyCustomWhereClauses.c | 274 | Custom SQL WHERE clauses |
| SocketAPI.c | 265 | Socket wrapper |
| As400UnixMediator.h | 153 | AS/400 mediator header |
| DebugPrint.c | 105 | Debug logging |
| SocketAPI.h | 47 | Socket header |
| **TOTAL** | **~83,983** | |

**IMPORTANT:** This is the largest component by far. Read files in sections (500-700 lines at a time for large files). Budget your analysis carefully.

### Business Context — Transaction Specifications

The `source_documents/` folder contains **21 specification files** describing the wire protocol for each transaction. These specs define the request/response field layouts. Key mappings:

| Transaction | Spec File | Handler Function | Source File |
|-------------|-----------|-----------------|-------------|
| 1001/1002 | טרנזקציה 1001-1002.docx | HandlerToMsg_1001, HandlerToSpool_1001 | SqlHandlers.c |
| 1011/1012 | טרנזקציה 1011-1012.docx | DownloadDrugList | SqlHandlers.c |
| 1013/1042 | טרנזקציה 1013-1042.docx | HandlerToMsg_1013, HandlerToSpool_1013 | SqlHandlers.c |
| 1014/1015 | טרנזקציה 1014-1015.docx | HandlerToMsg_1014 | SqlHandlers.c |
| 1043/1044 | טרנזקציה 1043-1044.docx | HandlerToMsg_1043 | SqlHandlers.c |
| 2064/2065 | טרנזקציה 2064-2065.docx | HandlerToMsg_2064 | ElectronicPr.c |
| 2074/2075 | טרנזקציה 2074-2075.docx | HandlerToMsg_2074 | ElectronicPr.c |
| 2078/2079 | טרנזקציה 2078-2079.docx | HandlerToMsg_2078 | ElectronicPr.c |
| 2080/2081 | טרנזקציה 2080-2081.docx | HandlerToMsg_2080 | ElectronicPr.c |
| 2082/2083 | טרנזקציה 2082-טבלת הערות.pdf | HandlerToMsg_2082 | ElectronicPr.c |
| 2084/2085 | טרנזקציה 2084-2085.docx | HandlerToMsg_2084 | ElectronicPr.c |
| 2092/2093 | טרנזקציה 2092-2093.docx | HandlerToMsg_2070_2092 | ElectronicPr.c |
| 2094/2095 | טרנזקציה 2094-2095.docx | DownloadDrugList | SqlHandlers.c |
| 6001/6002 | טרנזקציה 6001-6002.docx | HandlerToMsg_6001_6101 | DigitalRx.c |
| 6003/6004 | טרנזקציה 6003-6004.docx | HandlerToMsg_6003 | DigitalRx.c |
| 6005/6006 | טרנזקציה 6005-6006.docx | HandlerToMsg_6005, HandlerToSpool_6005 | DigitalRx.c |
| 6022/6023 | טרנזקציה 6022-6023.docx | HandlerToMsg_1022_6022 | SqlHandlers.c |
| 6033/6034 | טרנזקציה 6033-6034.docx | HandlerToMsg_2033_6033 | SqlHandlers.c |

**Additional transactions in code WITHOUT specs:** 1022, 1026, 1028, 1047, 1049, 1051, 1053, 1055, 1080, 2001, 2003, 2005, 2033, 2060, 2062, 2066, 2068, 2070, 2072, 2076, 2086, 2088, 2090, 2096, 2101, 5003, 5005, 5051, 5053, 5055, 5057, 5059, 5061, 5090, 6011, 6102, 6103

**Excel business logic:** `source_documents/טבלת לוגיקה-חנות וירטואלית-17.12.24.xlsx` — 3 tabs: (1) Virtual pharmacy eligibility logic (29 reason codes), (2) System parameters (6 params), (3) Usage instruction fields (#48-#63). These map to TR 6001/6101 response fields #67-#78 in DigitalRx.c.

### Research Objectives

#### 1. SqlServer.c — Main Entry & Dispatch (2,362 lines)

This is the architectural backbone. Document:

- **main() function** — initialization sequence, signal handlers, connections
- **Central dispatch switch** (lines ~1286-1687) — EVERY case label with:
  - Transaction code
  - Handler function called
  - Source file where handler lives
  - Brief description of what the transaction does
- **Initialization functions** — DB connection setup, shared memory attachment, socket setup
- **Signal handlers** — TerminateHandler, any others
- **Global variables** and configuration

#### 2. SqlHandlers.c — Core Pharmacy Handlers (8,458 lines)

Document ALL handler functions for the 1xxx transaction series:

- **HandlerToMsg_1001** / **HandlerToSpool_1001** — Open shift
- **DownloadDrugList** — Item table download (handles both 1011 and 2094)
- **HandlerToMsg_1013** / **HandlerToSpool_1013** — Close shift
- **HandlerToMsg_1014** — Maccabi price list update
- **HandlerToMsg_1043** — Error table download
- **HandlerToMsg_1022_6022** — Inventory operations
- **HandlerToMsg_2033_6033** — Pharmacist approval
- ALL other handler functions in this file

For each function:
- Exact start/end line numbers
- Function signature
- Transaction code(s) it handles
- Database tables accessed
- Wire protocol fields parsed/built
- AS/400 calls (if any)

#### 3. ElectronicPr.c — Electronic Prescription Handlers (26,161 lines)

This is the **largest single file**. Document ALL handler functions for 2xxx transactions:

- **HandlerToMsg_2064** — Units of measure table
- **HandlerToMsg_2074** — Item procedures/protocols
- **HandlerToMsg_2078** — Promotions (MaccabiCare)
- **HandlerToMsg_2080** — Gadgets (item-service mapping)
- **HandlerToMsg_2082** — Drug notes table
- **HandlerToMsg_2084** — Item-drug notes mapping
- **HandlerToMsg_2070_2092** — Generic groups (shared handler)
- ALL other handler functions (2060, 2062, 2066, 2068, 2072, 2076, 2086, 2088, 2090, 2096, 2101)

For each function: same detail as SqlHandlers.c above.

#### 4. DigitalRx.c — Digital Prescription Handlers (22,955 lines)

Document ALL handler functions for 6xxx transactions:

- **HandlerToMsg_6001_6101** — Prescription request (THE BIG ONE — includes virtual store logic)
  - This handler implements the virtual pharmacy eligibility logic from the Excel spreadsheet
  - Fields #67-#78 in the response correspond to the virtual store reason codes
- **HandlerToMsg_6003** — Dispensing request
- **HandlerToMsg_6005** / **HandlerToSpool_6005** — Dispensing report
- **HandlerToMsg_6011** — (identify from code)
- **HandlerToMsg_6102** — (identify from code)
- **HandlerToMsg_6103** — (identify from code)

For each function: same detail as above.

#### 5. MessageFuncs.c — Shared Message Utilities (13,631 lines)

Document:
- ALL utility functions (message parsing, field extraction, buffer management)
- Wire protocol helpers (how messages are structured: header, body, fields)
- Shared data structures used across handlers
- Error handling utilities
- Logging/debug functions

#### 6. MacODBC_MyOperators.c — SQL Operators (8,418 lines)

**IMPORTANT:** This file `#include "GenSql_ODBC_Operators.c"` at line 87 — meaning the GenSql shared operators are compiled into it. Also read `source_code/Include/GenSql_ODBC_Operators.c` and `source_code/Include/GenSql_ODBC_OperatorIDs.h` for the full operator picture.

Document:
- ALL SQL operator definitions (SQL_GetMainOperationParameters cases)
- Database tables referenced (from SQL queries)
- SQL query patterns (SELECT, INSERT, UPDATE, DELETE)
- Relationship to MacODBC.h macro API
- Which operators are SqlServer-specific vs. GenSql shared

#### 7. Supporting Files

- **MacODBC_MyCustomWhereClauses.c** (274 lines) — Custom WHERE clause builders
- **TikrotRPC.c** (494 lines) — Tikrot (subsidy/threshold) RPC calls to AS/400
- **As400UnixMediator.c** (660 lines) + **As400UnixMediator.h** (153 lines) — AS/400 communication bridge
- **SocketAPI.c** (265 lines) + **SocketAPI.h** (47 lines) — Socket wrapper
- **DebugPrint.c** (105 lines) — Debug logging

For each: file:line inventory of all functions, purpose, dependencies.

### Reading Strategy

Given the massive size (~84K lines), use this prioritized reading order:

```
Phase 1: SqlServer.c (2,362 lines) — Read fully. Get the architecture.
Phase 2: SqlHandlers.c (8,458 lines) — Read in 500-line passes.
Phase 3: ElectronicPr.c (26,161 lines) — Read in 600-line passes. Focus on function boundaries.
Phase 4: DigitalRx.c (22,955 lines) — Read in 600-line passes. Focus on function boundaries.
Phase 5: MessageFuncs.c (13,631 lines) — Read in 600-line passes.
Phase 6: MacODBC_MyOperators.c (8,418 lines) — Read in 500-line passes.
Phase 7: All supporting files (1,998 lines total) — Read fully.
```

For the large files (ElectronicPr.c, DigitalRx.c, MessageFuncs.c), focus on:
1. Function boundaries (start/end lines) — identify ALL functions first
2. Function signatures and purpose
3. Database table references
4. Transaction code references
5. Key business logic patterns

### Output

Create: `RESEARCH/SqlServer_deepdive.md`

```markdown
# SqlServer — Deep Dive

## Overview
[What this component does, its role in the MACCABI pharmacy system]

## File Inventory
| File | Lines | Purpose |
|------|-------|---------|
(exact counts verified)

## Architecture
### Main Entry Point (SqlServer.c)
- Initialization sequence
- Signal handlers
- Dispatch mechanism

### Transaction Dispatch Map
| Code | Handler | File:Line | Description |
|------|---------|-----------|-------------|
(EVERY transaction code with exact handler location)

## Transaction Handler Analysis

### 1xxx — Core Pharmacy Operations
#### HandlerToMsg_1001 (SqlHandlers.c:N-M)
- Purpose: Open shift
- Request fields: [list]
- Response fields: [list]
- DB tables: [list]
- SQL operations: [list]

(repeat for each 1xxx handler)

### 2xxx — Electronic Prescription Reference Tables
(same format for each 2xxx handler)

### 5xxx — Nihul Tikrot (Subsidy) Operations
(same format)

### 6xxx — Digital Prescription Operations
(same format, with special attention to 6001/6101 virtual store logic)

## Message Handling (MessageFuncs.c)
### Utility Functions
| Function | File:Line | Purpose |
|----------|-----------|---------|
(list all)

### Wire Protocol Structure
- Header format
- Field encoding
- Message types

## SQL Operator Definitions (MacODBC_MyOperators.c)
### Database Tables
| Table | Operations | Referenced By |
|-------|-----------|---------------|
(list all tables with SQL operations)

### Operator Inventory
| Operator ID | SQL Template | Tables | Type |
|-------------|-------------|--------|------|
(list all operators)

## AS/400 Integration
### TikrotRPC.c
- Functions and RPC calls
- Transaction codes handled

### As400UnixMediator
- Communication mechanism
- Functions provided

## Supporting Infrastructure
### SocketAPI
- Functions provided
- Usage patterns

### DebugPrint
- Logging mechanism

## Dependencies
### Headers Included
| Header | Provides | Used By |
|--------|----------|---------|
(list all #include directives)

### External Components
- GenLib functions used
- GenSql functions used
- MacODBC.h API calls

## Cross-References
### Business Context (source_documents/)
| Transaction | Spec Document | Handler | Verified Match |
|-------------|--------------|---------|----------------|
(correlate specs with code)

### Virtual Store Logic (Excel → Code mapping)
- Reason codes → DigitalRx.c fields
- System parameters → configuration
- Usage instruction fields → TR 6002 response fields #48-#63

## Global Variables
| Variable | Type | File:Line | Purpose |
|----------|------|-----------|---------|
(list all globals)

## Security Notes
- [Credential handling — location only, redact values]
- [Input validation patterns]

## Open Questions
- [Anything unclear after research]
```

### Quality Gates

- [ ] All 13 files read completely (every line accounted for)
- [ ] Exact line counts verified for each file
- [ ] ALL functions listed with exact file:line citations
- [ ] ALL transaction codes mapped to handlers with exact line numbers
- [ ] Database tables identified from SQL operators
- [ ] Wire protocol patterns documented
- [ ] AS/400 integration documented
- [ ] Cross-references to source_documents verified
- [ ] Virtual store logic (Excel) mapped to code
- [ ] Dependencies mapped (includes, GenLib, GenSql, MacODBC)
- [ ] Global variables inventoried
- [ ] Security-sensitive locations noted (values redacted)
- [ ] Zero forbidden words without verification
- [ ] Every claim has file:line citation

### Anti-Hallucination Rules (CRITICAL)

1. **DO NOT GUESS** line numbers or function counts. Read the code.
2. **DO NOT ASSUME** handler behavior from transaction names. Read the implementation.
3. **DO NOT ESTIMATE** counts. Verify with actual reading.
4. **DO NOT SKIP** any file or section. All 13 files must be read.
5. **IF UNCERTAIN**, mark as `[NEEDS VERIFICATION]`.
6. **VERIFY AFTER WRITING** — Read back the output file to confirm accuracy.
7. **BUDGET CAREFULLY** — With ~84K lines, focus on function boundaries and key logic. Don't try to document every local variable.

### On Completion

1. Update this file (PROMPT FOR RESEARCHER AGENT.md) with status update
2. Include in status update:
   - Exact line count per file (verified)
   - Count of handler functions found
   - Count of transaction codes mapped
   - Count of database tables identified
   - Any surprising findings
   - Any open questions
3. Notify Orc: `RES-SQL-001 COMPLETE`

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
