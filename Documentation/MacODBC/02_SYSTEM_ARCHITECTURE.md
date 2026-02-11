# MacODBC.h - System Architecture

**Component**: MacODBC (ODBC Infrastructure Layer)
**Task ID**: DOC-MACODBC-002
**Date Created**: 2026-02-11

---

## Single Dispatcher Pattern

Based on code analysis, MacODBC.h appears to be built around a single central dispatcher pattern:

```
25 Public Macros (MacODBC.h:311-335)
        │
        ▼
   ODBC_Exec (MacODBC.h:446-2657, 2212 lines)
        │
        ├─── ODBC_CommandType enum (20 values, MacODBC.h:117-139)
        │
        └─── Central switch (MacODBC.h:634-763)
             Maps each CommandType to specific behavior flags
```

All 25 public macros (DeclareCursor, ExecSQL, CommitWork, etc.) route to `ODBC_Exec` with an appropriate `ODBC_CommandType` value, as seen in `MacODBC.h:311-335`.

---

## #ifdef MAIN Duality

According to the file structure, MacODBC.h is a hybrid header/implementation file:

```
MacODBC.h
├── Lines 1-166:    Declarations (enums, structs, includes) — always active
├── Lines 168-241:  #ifdef MAIN — global variable storage
├── Lines 242-273:  #else — extern declarations
├── Lines 276-420:  Macros and function declarations — always active
├── Lines 422-4120: #ifdef MAIN — 11 function implementations
└── Line 4121:      #endif MacODBC_H
```

The `.c` component that defines `#define MAIN` before `#include <MacODBC.h>` receives the full implementation compiled into it. All system components do this, as observed in `RESEARCH/MacODBC_deepdive.md`.

---

## 8-Phase ODBC_Exec Pipeline

Based on analysis of `ODBC_Exec` (lines 446-2657), the function appears to progress through 8 sequential phases:

```
┌───────────────────────────────────────────────────────────────┐
│  Phase 1: Command Decode & Init           (lines 422-772)     │
│  ├── va_start, memset, flag initialization                    │
│  └── switch on CommandType → sets behavior flags              │
├───────────────────────────────────────────────────────────────┤
│  Phase 2: First-Call Initialization        (lines 775-887)    │
│  ├── memset of cache arrays                                   │
│  ├── Signal handler installation (SIGSEGV) via sigaction      │
│  └── Runtime bool size check (sizeof(bool) → BoolType)       │
├───────────────────────────────────────────────────────────────┤
│  Phase 3: Operation Metadata Loading       (lines 888-1072)   │
│  ├── SQL_GetMainOperationParameters → SQL text, columns       │
│  ├── Mirroring decision (CommandIsMirrored)                   │
│  ├── find_FOR_UPDATE_or_GEN_VALUES → command type detection   │
│  └── SQL_GetWhereClauseParameters → optional WHERE clause     │
├───────────────────────────────────────────────────────────────┤
│  Phase 4: Sticky Statement Lifecycle       (lines 1075-1231)  │
│  ├── DB provider switch handling                              │
│  ├── Admission control (max 120 sticky statements)            │
│  ├── Prepared-state validation (SQLNumParams)                 │
│  └── SQLAllocStmt allocation (primary + mirror)               │
├───────────────────────────────────────────────────────────────┤
│  Phase 5: SQL Construction & PREPARE       (lines 1233-1447)  │
│  ├── SQL_CmdBuffer assembly                                   │
│  ├── SQL_CustomizePerDB → provider-specific token rewriting   │
│  ├── SQLPrepare — failure triggers auto-reconnect             │
│  └── Mirror PREPARE on alternate DB                           │
├───────────────────────────────────────────────────────────────┤
│  Phase 6: Pointer Validation & Binding     (lines 1449-1976)  │
│  ├── Pointer collection with SIGSEGV validation               │
│  ├── SQLBindCol for output columns                            │
│  ├── SQLBindParameter for input columns                       │
│  └── SQLBindParameter for WHERE clause + mirror               │
├───────────────────────────────────────────────────────────────┤
│  Phase 7: Execute, Mirror, Fetch           (lines 1978-2288)  │
│  ├── SQLExecute on mirror (first!)                            │
│  ├── SQLExecute on primary DB                                 │
│  ├── SQLRowCount + error handling                             │
│  ├── SQLFetchScroll SQL_FETCH_NEXT                            │
│  └── Cardinality check (second fetch)                         │
├───────────────────────────────────────────────────────────────┤
│  Phase 8: Transaction, Close, Return       (lines 2290-2657)  │
│  ├── Commit/Rollback (ENV level vs DBC level)                 │
│  ├── Isolation level setting                                  │
│  ├── SQLCloseCursor / SQLFreeHandle                           │
│  └── va_end, return SQLCODE                                   │
└───────────────────────────────────────────────────────────────┘
```

---

## Database Mirroring Architecture

According to the code, the system appears to support mirroring writes to an alternate DB:

```
┌─────────────┐          ┌──────────────┐
│   MAIN_DB   │          │   ALT_DB     │
│  (primary)  │          │  (mirror)    │
└──────┬──────┘          └──────┬───────┘
       │                        │
       │    ODBC_MIRRORING_ENABLED (MacODBC.h:180)
       │         │
       ▼         ▼
┌──────────────────────────────────────┐
│        ODBC_Exec                      │
│                                       │
│  1. CommandIsMirrored set (line 918) │
│  2. SELECTs not mirrored (line 994)  │
│  3. MIRROR_DB = opposite of Database  │
│     (line 1002)                       │
│  4. Mirror statement allocated        │
│     (line 1224)                       │
│  5. Mirror PREPARE (line 1402)        │
│  6. Mirror Bind (lines 1902-1975)     │
│  7. Mirror Execute first! (line 2029) │
│  8. Commit: ENV level when            │
│     ALTERNATE_DB_USED (line 2396)     │
└──────────────────────────────────────┘
```

**Key mirroring points:**
- According to `MacODBC.h:2029`, the mirror executes **before** the primary
- Mirror failures are logged but are not fatal, as seen in `MacODBC.h:2047-2079`
- CommitAllWork uses `SQLEndTran` at ENV level when both DBs were used, as seen in `MacODBC.h:2396-2446`

---

## Include Injection Pattern

According to the code, MacODBC.h uses an `#include` injection pattern for per-component customization:

```
SQL_GetMainOperationParameters (MacODBC.h:2661-2817)
    │
    └── switch(OperationIdentifier) line 2745
        │
        └── #include "MacODBC_MyOperators.c"  ← line 2747
            (each component provides its own file)

SQL_GetWhereClauseParameters (MacODBC.h:2821-2902)
    │
    └── switch(WhereClauseIdentifier) line 2864
        │
        └── #include "MacODBC_MyCustomWhereClauses.c"  ← line 2866
            (each component provides its own file)
```

**Known component files** (according to `CHUNKS/MacODBC/analysis.json`):

| Component | MacODBC_MyOperators.c | MacODBC_MyCustomWhereClauses.c |
|-----------|----------------------|-------------------------------|
| FatherProcess | `source_code/FatherProcess/` | `source_code/FatherProcess/` |
| SqlServer | `source_code/SqlServer/` | `source_code/SqlServer/` |
| As400UnixServer | `source_code/As400UnixServer/` | `source_code/As400UnixServer/` |
| As400UnixClient | `source_code/As400UnixClient/` | `source_code/As400UnixClient/` |
| As400UnixDocServer | `source_code/As400UnixDocServer/` | `source_code/As400UnixDocServer/` |
| As400UnixDoc2Server | `source_code/As400UnixDoc2Server/` | `source_code/As400UnixDoc2Server/` |
| ShrinkPharm | `source_code/ShrinkPharm/` | `source_code/ShrinkPharm/` |
| GenSql | `source_code/GenSql/` | N/A |

---

## Component Integration

According to `RESEARCH/MacODBC_deepdive.md`, the following source files include MacODBC.h directly:

```
source_code/FatherProcess/FatherProcess.c:29
source_code/SqlServer/SqlServer.c:29
source_code/SqlServer/SqlHandlers.c:25
source_code/SqlServer/MessageFuncs.c:32
source_code/SqlServer/ElectronicPr.c:21
source_code/SqlServer/DigitalRx.c:20
source_code/As400UnixClient/As400UnixClient.c:29
source_code/As400UnixServer/As400UnixServer.c:32
source_code/As400UnixDocServer/As400UnixDocServer.c:31
source_code/As400UnixDoc2Server/As400UnixDoc2Server.c:34
source_code/ShrinkPharm/ShrinkPharm.c:16
source_code/GenSql/GenSql.c:32
```

---

## Data Flow

```
GenSql.c:INF_CONNECT()
        │
        ▼
ODBC_CONNECT (MacODBC.h:3443-3794)
        │
        ├── SQLAllocEnv → ODBC_henv
        ├── SQLAllocConnect → DB.HDBC
        ├── SQLConnect (DSN, user, password)
        ├── MS-SQL: USE <dbname>, SET LOCK_TIMEOUT, SET DEADLOCK_PRIORITY
        ├── Informix: disable AUTOFREE
        └── AUTOCOMMIT off, dirty-read isolation

Source component:
        │
        ├── ExecSQL(op_id, &output_var, input_var)
        │       │
        │       ▼
        │   ODBC_Exec → SQL_GetMainOperationParameters
        │       │         └── #include "MacODBC_MyOperators.c"
        │       │                 → SQL text, column specs, flags
        │       │
        │       ├── ParseColumnList → ODBC_ColumnParams[]
        │       ├── SQL_CustomizePerDB → token rewriting
        │       ├── SQLPrepare → SQLBindCol/SQLBindParameter
        │       ├── SQLExecute (mirror first, then primary)
        │       └── SQLFetchScroll → output to caller
        │
        └── CommitWork / RollbackWork
                │
                ▼
            SQLEndTran (ENV level or DBC level)
```

---

## Security Notes

According to `RESEARCH/MacODBC_deepdive.md`:
- DB credentials are passed to `ODBC_CONNECT` as plain text (DSN, username, password) at `MacODBC.h:3444-3448`
- `GenSql.c` receives these values from environment variables (`MS_SQL_ODBC_USER`, `MS_SQL_ODBC_PASS`) at `GenSql.c:1088-1091`
- `setlocale(LC_ALL, "he_IL.UTF-8")` is called during connection at `MacODBC.h:3488` — Hebrew locale dependency
- These locations are documented but values are not copied to documentation

---

*Generated by the CIDRA Documenter Agent — DOC-MACODBC-002*
