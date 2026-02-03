# ShrinkPharm - Business Logic

**Document ID:** DOC-SHRINK-001
**Generated:** 2026-02-03
**Source:** source_code/ShrinkPharm/ (574 source lines verified)

---

## Business Purpose

According to the file header at `ShrinkPharm.c:7-10`:

> "ODBC equivalent of ShrinkDoctor for use in the MS-SQL Pharmacy application"

ShrinkPharm appears to be a database maintenance utility that:
- Purges historical data from the MS-SQL Pharmacy database
- Enforces data retention policies
- Maintains database performance by removing old records

---

## Purge Algorithm

### High-Level Flow

Based on code analysis of `ShrinkPharm.c`:

```
1. Read configuration from shrinkpharm control table
2. For each enabled target table:
   a. Calculate retention cutoff date
   b. Identify rows older than cutoff
   c. Delete old rows in batches
   d. Record execution statistics
3. Log totals and exit
```

### Detailed Algorithm

#### Step 1: Date Calculation

According to code at `ShrinkPharm.c:189`:

```c
MinDateToRetain = IncrementDate(SysDate, 0 - days_to_retain);
```

The retention cutoff is calculated by subtracting `days_to_retain` from the current date.

**Example:**
- If SysDate = 20260203 (February 3, 2026)
- And days_to_retain = 365
- Then MinDateToRetain = 20250203 (February 3, 2025)
- All rows with dates before 20250203 will be purged

#### Step 2: Row Selection

Two selection modes are supported, controlled by `use_where_clause`:

**Mode 1: WHERE Clause in SQL (use_where_clause != 0)**

According to code at `ShrinkPharm.c:193-206`:

```sql
SELECT <date_column> FROM <table> WHERE <date_column> < <MinDateToRetain>
```

This mode:
- Uses database index (if available) for efficient filtering
- Predicts quantity with COUNT query first
- Appears to be the preferred mode

**Mode 2: Filter in Code (use_where_clause == 0)**

According to code at `ShrinkPharm.c:208-213` and `ShrinkPharm.c:253-257`:

```sql
SELECT <date_column> FROM <table>
```

Then in code:
```c
if ((!use_where_clause) && (DateOfRow >= MinDateToRetain)) {
    continue;  // Skip this row
}
```

This mode:
- Scans entire table
- Filters rows in application code
- Used when table lacks appropriate index
- Comment at lines 176-182 explains this matches "old ShrinkDoctor" behavior

#### Step 3: Row Deletion

According to code at `ShrinkPharm.c:215-216` and `ShrinkPharm.c:266`:

```sql
DELETE FROM <table> WHERE CURRENT OF ShrinkPharmSelCur
```

Rows are deleted one at a time using positioned delete (WHERE CURRENT OF cursor).

#### Step 4: Batch Commit

According to code at `ShrinkPharm.c:286-291`:

```c
if (RowsDeletedSinceCommit >= commit_count) {
    CommitAllWork();
    RowsDeletedSinceCommit = 0;
}
```

Commits occur every `commit_count` deletions, configurable per table.

#### Step 5: Statistics Recording

According to code at `ShrinkPharm.c:303-307`:

After processing each table, the program updates the control row:
- last_run_date
- last_run_time
- last_run_num_purged

---

## Configuration Model

### Control Table Schema (Inferred)

Based on `MacODBC_MyOperators.c:91-99` and `ShrinkPharm.c:52-57`:

| Column | Type | Purpose |
|--------|------|---------|
| table_name | char(30) | Name of table to purge |
| date_column_name | char(30) | Name of date column for comparison |
| days_to_retain | int | Number of days of data to keep |
| use_where_clause | short | 0=scan all rows, else=use WHERE clause |
| commit_count | int | Number of deletions between commits |
| purge_enabled | (filter) | Non-zero enables purging for this row |
| last_run_date | int | Date of last execution (output) |
| last_run_time | int | Time of last execution (output) |
| last_run_num_purged | int | Rows deleted in last run (output) |

### Configuration Example

A hypothetical configuration row:

| Column | Value | Meaning |
|--------|-------|---------|
| table_name | (target table) | Table to be purged |
| date_column_name | (date column) | Date column to check |
| days_to_retain | 365 | Keep 1 year of data |
| use_where_clause | 1 | Use index-based filtering |
| commit_count | 1000 | Commit every 1000 deletes |
| purge_enabled | 1 | Enable purging |

---

## Operational Behavior

### Priority and Contention

According to code at `ShrinkPharm.c:121-125`:

```c
// For ShrinkPharm, we want a longish timeout (so we'll be pretty
// patient) and a below-normal deadlock priority (since we want to
// privilege real-time application operations over housekeeping).
LOCK_TIMEOUT        = 1000;  // Milliseconds.
DEADLOCK_PRIORITY   = -2;    // 0 = normal, -10 to 10 = low to high priority.
```

ShrinkPharm is designed to:
- Wait patiently for locks (1 second timeout)
- Yield to real-time operations in deadlock situations
- Avoid disrupting normal database operations

### Production vs Test Detection

According to code at `ShrinkPharm.c:79-89`:

The program detects test environments by hostname:
- linux01-test
- linux01-qa
- pharmlinux-test
- pharmlinux-qa

If running on a test host, `TikrotProductionMode` is set to 0.

---

## Logging and Monitoring

### Log File Location

According to code at `ShrinkPharm.c:72-73`:

- Directory: Environment variable `MAC_LOG`
- Filename: "ShrinkPharm_log"

### Log Messages

| Event | Message Location | Content |
|-------|------------------|---------|
| Start | Line 75-77 | "ShrinkPharm.exe started [date] at [time]" |
| Table start | Line 191 | "Purge from [table] where [column] < [date]..." |
| Predicted count | Line 201 | "[N] rows to delete, SQLCODE = [code]" |
| Table complete | Line 315-316 | "Purged [N] rows with [column] < [date] from [table]" |
| Run complete | Lines 354-356 | Total rows, duration, rate |

### Statistics Calculated

According to code at `ShrinkPharm.c:330-351`:

| Metric | Calculation |
|--------|-------------|
| RunLenMinutes | EndTime - StartTime (with midnight handling) |
| DeletionsPerMinute | (RowsDeletedFullRun * 60) / RunLenSeconds |

---

## Error Scenarios

### Database Connection Failure

According to code at `ShrinkPharm.c:128-137`:

- Retries every 10 seconds until connected
- Logs retry attempts

### SQL Errors During Processing

According to code at `ShrinkPharm.c:152-155, 168-171`:

- Error causes exit from current processing loop
- Partial work is committed before error (batch model)

### Signal Interruption

According to code at `ShrinkPharm.c:370-430`:

| Signal | Response |
|--------|----------|
| SIGFPE | Rollback, log, exit |
| SIGSEGV | Rollback, log, exit |
| SIGTERM | Log, exit (no rollback) |

---

## Healthcare System Context

### Relationship to Other Components

Based on research context:

| Component | Relationship |
|-----------|--------------|
| FatherProcess | Both use same SQLMD_connect() retry pattern |
| SqlServer | Comments mention ShrinkPharm as suitable for ODBC_PRESERVE_CURSORS |
| PharmTcpServer | Appears to use same target database |

### Operational Independence

ShrinkPharm appears to:
- Run independently of the FatherProcess supervision tree
- Connect directly to the database rather than via SqlServer
- Be scheduled externally (cron, scheduler, or manual execution)

---

## Business Rules Summary

Based on code analysis:

1. **Retention Policy:** Data older than `days_to_retain` days is purged
2. **Enabled Filter:** Only rows where `purge_enabled <> 0` are processed
3. **Batch Processing:** Commits occur every `commit_count` deletions
4. **Low Priority:** Housekeeping yields to real-time operations
5. **Statistics Tracking:** Each run records date, time, and count

---

## Documentation Limitations

### What CANNOT be determined from code:

- The specific tables configured for purging in production
- The retention periods and commit counts in use
- How and when the utility is scheduled to run
- Business justification for specific retention policies
- Relationship with the original "ShrinkDoctor" mentioned in comments

### What IS known from code:

- The algorithm for date-based row deletion
- The configuration model (control table structure)
- The batch commit and statistics recording behavior
- The priority settings for database contention

---

*Document generated by CIDRA Documenter Agent*
*Task ID: DOC-SHRINK-001*
