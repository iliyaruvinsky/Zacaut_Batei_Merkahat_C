# ShrinkPharm - Program Specification

**Document ID:** DOC-SHRINK-001
**Generated:** 2026-02-03
**Source:** source_code/ShrinkPharm/ (574 source lines verified)

---

## Overview

ShrinkPharm is a standalone database housekeeping utility for the MACCABI healthcare system. According to the file header at `ShrinkPharm.c:7-10`, it is described as an "ODBC equivalent of ShrinkDoctor for use in the MS-SQL Pharmacy application."

This program appears to be a batch utility that purges old rows from configured database tables based on date retention policies. Unlike long-running server processes in this codebase (such as SqlServer or As400UnixServer), ShrinkPharm does not use the `InitSonProcess()` bootstrap pattern - it connects directly to the database via `SQLMD_connect()` and exits upon completion.

---

## Purpose

Based on code analysis, ShrinkPharm appears to:

1. Connect to the database via ODBC (`ShrinkPharm.c:128-137`)
2. Read purge configuration from a control table named `shrinkpharm` (`MacODBC_MyOperators.c:90-99`)
3. For each enabled configuration row, delete database rows older than a specified retention period
4. Record execution statistics back to the control table
5. Log operation results and exit

---

## File Structure

| File | Lines | Purpose |
|------|------:|---------|
| ShrinkPharm.c | 431 | Main program - database connection, purge logic, statistics |
| MacODBC_MyOperators.c | 133 | ODBC SQL operator case definitions (5 operators) |
| MacODBC_MyCustomWhereClauses.c | 10 | Placeholder for custom WHERE clauses (currently unused) |
| Makefile | 26 | Unix build configuration |
| **Source Total** | **574** | Verified exact count |
| **With Build** | **600** | Including Makefile |

---

## Functions

| Function | File | Lines | Purpose |
|----------|------|------:|---------|
| main | ShrinkPharm.c | 35-359 (324 lines) | Entry point - connects to DB, reads purge config, iterates target tables, deletes old rows, records statistics |
| TerminateHandler | ShrinkPharm.c | 370-430 (60 lines) | Signal handler for SIGFPE, SIGSEGV, SIGTERM - performs rollback and logs termination |

---

## Global Variables

According to code at `ShrinkPharm.c:14-32`:

| Variable | Type | Initial Value | Purpose | Notes |
|----------|------|---------------|---------|-------|
| GerrSource | char* | __FILE__ | Error logging source identification | Line 14 |
| restart_flg | static int | 0 | Appears unused | Line 24 |
| caught_signal | int | 0 | Stores caught signal number | Line 25 |
| TikrotProductionMode | int | 1 | Production vs test mode flag | Line 26 |
| sig_act_terminate | struct sigaction | (uninitialized) | Signal handler configuration | Line 28 |
| need_rollback | static int | 0 | Appears unused | Line 30 |
| recs_to_commit | static int | 0 | Appears unused | Line 31 |
| recs_committed | static int | 0 | Appears unused | Line 32 |

**Note:** Three globals (`need_rollback`, `recs_to_commit`, `recs_committed`) appear to be declared but not used in the current code.

---

## Preprocessor Definitions

| Directive | Line | Purpose |
|-----------|-----:|---------|
| #define MAIN | 13 | Marks this as the main program file |
| #include \<MacODBC.h\> | 16 | ODBC database infrastructure header |

---

## Dependencies

### Headers (from ShrinkPharm.c:16)

| Header | Provides |
|--------|----------|
| MacODBC.h | ODBC database abstraction layer (includes GenSql.h) |

### External Libraries (from Makefile:19)

| Library | Purpose |
|---------|---------|
| -lodbc | ODBC database connectivity |

### GenLib Functions Used

Based on code analysis:

| Function | Purpose | Example Usage |
|----------|---------|---------------|
| GetDate() | Get current system date as integer | `ShrinkPharm.c:69` |
| GetTime() | Get current system time as integer | `ShrinkPharm.c:70` |
| IncrementDate() | Add/subtract days from date | `ShrinkPharm.c:189` |
| strip_spaces() | Remove trailing spaces from string | `ShrinkPharm.c:187-188` |
| GerrLogFnameMini() | Write to named log file | `ShrinkPharm.c:75-77` |
| GerrLogReturn() | Log error and return | `ShrinkPharm.c:113-115` |
| ExitSonProcess() | Exit with status code | `ShrinkPharm.c:395` |

### GenSql/ODBC Functions Used

| Function | Purpose | Example Usage |
|----------|---------|---------------|
| SQLMD_connect() | Connect to database | `ShrinkPharm.c:130` |
| SQLMD_disconnect() | Disconnect from database | `ShrinkPharm.c:328` |
| DeclareAndOpenCursorInto() | Declare and open cursor with output variables | `ShrinkPharm.c:147-150` |
| FetchCursor() | Fetch next row from cursor | `ShrinkPharm.c:160` |
| CloseCursor() | Close cursor | `ShrinkPharm.c:312, 322` |
| ExecSQL() | Execute SQL statement | `ShrinkPharm.c:199, 266, 306-307` |
| FreeStatement() | Free prepared statement | `ShrinkPharm.c:313` |
| CommitAllWork() | Commit transaction | `ShrinkPharm.c:288, 299, 327` |
| RollbackAllWork() | Rollback transaction | `ShrinkPharm.c:408, 413` |
| SQLERR_error_test() | Check for SQL errors | `ShrinkPharm.c:152` |
| SQLERR_code_cmp() | Compare SQLCODE with constant | `ShrinkPharm.c:162` |

---

## SQL Operators

According to `MacODBC_MyOperators.c:90-130`:

| Operator ID | Cursor Name | Purpose |
|-------------|-------------|---------|
| ShrinkPharmControlCur | ShrinkPharmCtrlCur | SELECT enabled purge configs from shrinkpharm table |
| ShrinkPharmSaveStatistics | (none) | UPDATE shrinkpharm with run statistics |
| ShrinkPharmSelectQuantity | (none) | Dynamic COUNT query for predicted quantity |
| ShrinkPharmSelectCur | ShrinkPharmSelCur | Dynamic SELECT cursor for rows to delete |
| ShrinkPharmDeleteCurrentRow | (none) | Dynamic DELETE WHERE CURRENT OF (sticky) |

---

## Configuration Parameters

### Database Settings (ShrinkPharm.c:124-126)

| Parameter | Value | Purpose |
|-----------|-------|---------|
| LOCK_TIMEOUT | 1000 | Lock timeout in milliseconds |
| DEADLOCK_PRIORITY | -2 | Below normal (-10 to 10 range) to privilege real-time operations |
| ODBC_PRESERVE_CURSORS | 1 | Preserve cursors after COMMIT operations |

### Runtime Configuration

| Setting | Source | Purpose |
|---------|--------|---------|
| Log Directory | getenv("MAC_LOG") | Directory for log files (`ShrinkPharm.c:72`) |
| Log File Name | "ShrinkPharm_log" | Hard-coded log file name (`ShrinkPharm.c:73`) |

### Test/Production Detection (ShrinkPharm.c:79-89)

The program detects test environments by hostname. If hostname matches any of these values, `TikrotProductionMode` is set to 0:
- linux01-test
- linux01-qa
- pharmlinux-test
- pharmlinux-qa

---

## Documentation Limitations

### What CANNOT be determined from code alone:

- The exact schema and governance of the `shrinkpharm` control table (DDL, who can edit it)
- Which target tables are configured for purging in production
- How and when ShrinkPharm is scheduled to run (cron, systemd, job scheduler, or manual execution)
- The relationship with the original "ShrinkDoctor" program mentioned in comments

### What IS known from code:

- ShrinkPharm is a standalone batch utility (not a long-running server)
- It reads configuration from the `shrinkpharm` database table
- It deletes rows based on date column comparisons
- It commits in batches and records statistics
- It uses below-normal deadlock priority to privilege real-time operations

---

*Document generated by CIDRA Documenter Agent*
*Task ID: DOC-SHRINK-001*
