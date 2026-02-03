# ShrinkPharm - Technical Analysis

**Document ID:** DOC-SHRINK-001
**Generated:** 2026-02-03
**Source:** source_code/ShrinkPharm/ (574 source lines verified)

---

## Function Analysis

### main() - Entry Point

**Location:** `ShrinkPharm.c:35-359` (324 lines)

**Purpose:** Orchestrates the entire purge operation from startup to exit.

**Control Flow:**

1. **Initialization (lines 68-116)**
   - Get process ID, current date/time
   - Configure logging directory and file
   - Detect production vs test environment by hostname
   - Install signal handler for SIGFPE

2. **Database Connection (lines 119-137)**
   - Set ODBC parameters (LOCK_TIMEOUT, DEADLOCK_PRIORITY, ODBC_PRESERVE_CURSORS)
   - Retry SQLMD_connect() until MAIN_DB->Connected
   - Sleep 10 seconds between retries

3. **Main Processing Loop (lines 140-319)**
   - Open control cursor (ShrinkPharmControlCur) to read enabled purge configs
   - For each config row:
     - Strip trailing spaces from table_name and date_column_name
     - Compute MinDateToRetain = SysDate - days_to_retain
     - Optionally execute COUNT query to predict quantity
     - Open selection cursor (ShrinkPharmSelectCur)
     - Delete rows using WHERE CURRENT OF cursor
     - Commit every commit_count deletions
     - Save statistics back to shrinkpharm table
     - Free sticky statement

4. **Cleanup and Exit (lines 322-358)**
   - Close control cursor
   - Final commit
   - Disconnect from database
   - Calculate and log run statistics
   - Exit with code 0

**Local Variables:**

| Variable | Type | Purpose | Line |
|----------|------|---------|------|
| retrys | int | (declared but appears unused) | 37 |
| mypid | int | Process ID | 38 |
| TotalRowsDeleted | int | Count of deleted rows per table | 40 |
| RowsDeletedSinceCommit | int | Rows since last commit | 41 |
| RowsDeletedFullRun | int | Total rows deleted across all tables | 42 |
| SysDate | int | Current system date | 43 |
| SysTime | int | Current system time | 44 |
| StartTime | int | Run start time | 45 |
| EndTime | int | Run end time | 46 |
| RunLenMinutes | int | Duration calculation | 47 |
| DeletionsPerMinute | int | Rate calculation | 48 |
| table_name | char[31] | From shrinkpharm row | 53 |
| date_column_name | char[31] | From shrinkpharm row | 54 |
| use_where_clause | short | Flag from shrinkpharm row | 55 |
| days_to_retain | int | Retention period | 56 |
| commit_count | int | Batch size | 57 |
| MinDateToRetain | int | Computed cutoff date | 60 |
| DateOfRow | int | Date value from target row | 61 |
| SelectCursorCommand | char[601] | Dynamic SQL buffer | 63 |
| PredictQtyCommand | char[601] | Dynamic SQL buffer | 64 |
| DeleteCommand | char[301] | Dynamic SQL buffer | 65 |

---

### TerminateHandler() - Signal Handler

**Location:** `ShrinkPharm.c:370-430` (60 lines)

**Purpose:** Handle fatal/terminating signals with graceful cleanup.

**Parameters:**
- `signo` (int): Signal number received

**Control Flow:**

1. **Reset Signal Handling (line 379)**
   - Re-install handler via sigaction

2. **Endless Loop Detection (lines 386-396)**
   - If same signal at same time as last call, abort immediately
   - Prevents infinite recursion in signal handling

3. **Signal-Specific Actions (lines 405-424)**
   - SIGFPE: RollbackAllWork, describe as "floating-point error"
   - SIGSEGV: RollbackAllWork, describe as "segmentation error"
   - SIGTERM: No rollback, describe as "user-requested shutdown"
   - Default: "check manual page for SIGNAL"

4. **Log and Exit (lines 426-429)**
   - Log message with signal number and description

**Static Variables:**

| Variable | Type | Purpose | Line |
|----------|------|---------|------|
| last_signo | int | Previous signal number | 374 |
| last_called | int | Time of previous call | 375 |

**Code Quality Note:** According to code at lines 388-390 and 426-429, the log messages incorrectly reference "As400UnixServer" instead of "ShrinkPharm". This appears to be a copy-paste artifact from another component.

---

## SQL Operator Analysis

According to `MacODBC_MyOperators.c:90-130`:

### ShrinkPharmControlCur (lines 90-99)

**Purpose:** Read enabled purge configurations from control table

**SQL:**
```sql
SELECT table_name, days_to_retain, date_column_name,
       use_where_clause, commit_count
FROM   shrinkpharm
WHERE  purge_enabled <> 0
ORDER BY table_name
```

**Configuration:**
- CursorName: "ShrinkPharmCtrlCur"
- NumOutputColumns: 5
- OutputColumnSpec: "char(30), int, char(30), short, int"

### ShrinkPharmSaveStatistics (lines 102-110)

**Purpose:** Update execution statistics in control table

**SQL:**
```sql
UPDATE shrinkpharm
SET    last_run_date = ?,
       last_run_time = ?,
       last_run_num_purged = ?
WHERE CURRENT OF ShrinkPharmCtrlCur
```

**Configuration:**
- NumInputColumns: 3
- InputColumnSpec: "int, int, int"

### ShrinkPharmSelectQuantity (lines 113-117)

**Purpose:** Execute dynamic COUNT query to predict deletion quantity

**SQL:** NULL (supplied dynamically by caller)

**Dynamic SQL Example (from ShrinkPharm.c:195-197):**
```sql
SELECT CAST(count(*) AS INTEGER) FROM <table> WHERE <datecol> < <MinDateToRetain>
```

**Configuration:**
- NumOutputColumns: 1
- OutputColumnSpec: "int"

### ShrinkPharmSelectCur (lines 120-125)

**Purpose:** Open cursor to scan rows for deletion

**SQL:** NULL (supplied dynamically by caller)

**Dynamic SQL Examples (from ShrinkPharm.c:204-212):**

With WHERE clause:
```sql
SELECT <datecol> FROM <table> WHERE <datecol> < <MinDateToRetain>
```

Without WHERE clause:
```sql
SELECT <datecol> FROM <table>
```

**Configuration:**
- CursorName: "ShrinkPharmSelCur"
- NumOutputColumns: 1
- OutputColumnSpec: "int"

### ShrinkPharmDeleteCurrentRow (lines 127-130)

**Purpose:** Delete current row from selection cursor

**SQL:** NULL (supplied dynamically by caller)

**Dynamic SQL (from ShrinkPharm.c:215):**
```sql
DELETE FROM <table> WHERE CURRENT OF ShrinkPharmSelCur
```

**Configuration:**
- StatementIsSticky: 1 (for performance in inner loop)

---

## Data Structures

### Control Table Row (inferred from SQL)

Based on `MacODBC_MyOperators.c:91-99`:

| Column | Type | Purpose |
|--------|------|---------|
| table_name | char(30) | Target table to purge |
| days_to_retain | int | Retention period in days |
| date_column_name | char(30) | Date column for comparison |
| use_where_clause | short | Flag: 1=use WHERE in SELECT, 0=filter in code |
| commit_count | int | Batch commit size |
| purge_enabled | (not selected) | Filter: <> 0 means enabled |
| last_run_date | int | Updated by ShrinkPharmSaveStatistics |
| last_run_time | int | Updated by ShrinkPharmSaveStatistics |
| last_run_num_purged | int | Updated by ShrinkPharmSaveStatistics |

### Signal Action Structure

According to `ShrinkPharm.c:28`:

```c
struct sigaction sig_act_terminate;
```

Configured at lines 100-102:
- sa_handler = TerminateHandler
- sa_mask = NullSigset (empty)
- sa_flags = 0

---

## Error Handling

### SQL Error Detection

| Function | Purpose | Usage Example |
|----------|---------|---------------|
| SQLERR_error_test() | Check if error occurred | `ShrinkPharm.c:152, 168, 231, 247, 268` |
| SQLERR_code_cmp() | Compare SQLCODE with constant | `ShrinkPharm.c:162, 241, 276` |

### Error Constants Used

| Constant | Meaning | Usage |
|----------|---------|-------|
| SQLERR_end_of_fetch | No more rows | Break from fetch loop |
| SQLERR_no_rows_affected | DELETE had no effect | Log warning |

### Error Flow Pattern

According to code at `ShrinkPharm.c:152-171`:

```c
if (SQLERR_error_test()) {
    break;  // Exit current loop on error
}
```

The program uses a "do { ... } while(0)" pattern (lines 140-320) to enable clean error handling with break statements.

---

## Performance Considerations

### Batch Commit Strategy

According to code at `ShrinkPharm.c:286-291`:

```c
if (RowsDeletedSinceCommit >= commit_count) {
    CommitAllWork();
    RowsDeletedSinceCommit = 0;
}
```

This batching approach:
- Reduces transaction log growth
- Allows partial progress if interrupted
- Configurable per-table via commit_count column

### Sticky Statement Optimization

According to `MacODBC_MyOperators.c:127-130`:

```c
case ShrinkPharmDeleteCurrentRow:
    SQL_CommandText = NULL;
    StatementIsSticky = 1;  // Avoid re-PREPARE overhead
    break;
```

The DELETE statement is marked "sticky" to avoid repeated PREPARE overhead in the inner delete loop. It is freed after each table is processed (`ShrinkPharm.c:313`).

### ODBC_PRESERVE_CURSORS

According to code at `ShrinkPharm.c:126`:

```c
ODBC_PRESERVE_CURSORS = 1;  // So all cursors will be preserved after COMMITs.
```

This allows the outer control cursor to remain valid across batch commits, enabling the nested cursor pattern.

---

## Robustness Notes

### Potential Issues Identified

1. **NULL Pointer Risk (line 72)**
   - `strcpy(LogDir, getenv("MAC_LOG"))` could crash if MAC_LOG is unset

2. **Copy-Paste Artifact (lines 388-390, 426-429)**
   - Log messages reference "As400UnixServer" instead of "ShrinkPharm"

3. **Unused Variables (lines 30-32)**
   - `need_rollback`, `recs_to_commit`, `recs_committed` declared but not used

4. **Dynamic SQL from DB Values (lines 195-216)**
   - Table names and column names are interpolated from DB without validation
   - If control table is compromised, arbitrary tables could be affected

### Defensive Measures Present

1. **Endless Loop Detection (lines 386-396)**
   - Prevents infinite recursion in signal handling

2. **Batch Commits (lines 286-291, 297-301)**
   - Partial progress preserved if interrupted

3. **Below-Normal Deadlock Priority (line 125)**
   - Yields to real-time operations in contention

---

*Document generated by CIDRA Documenter Agent*
*Task ID: DOC-SHRINK-001*
