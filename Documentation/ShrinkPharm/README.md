# ShrinkPharm

**Component:** Database Housekeeping Utility
**System:** MACCABI Healthcare C Backend
**Documentation Version:** DOC-SHRINK-001
**Generated:** 2026-02-03

---

## Quick Summary

ShrinkPharm is a standalone database housekeeping utility that purges historical data from the MS-SQL Pharmacy database. According to the source code header (`ShrinkPharm.c:7-10`), it is described as an "ODBC equivalent of ShrinkDoctor."

The program reads purge configuration from a control table (`shrinkpharm`), then deletes rows from target tables based on date retention policies. It operates with below-normal deadlock priority to avoid disrupting real-time operations, and commits deletions in configurable batches.

**Key Characteristics:**
- Standalone batch utility (not a supervised server process)
- Configuration-driven via database control table
- Batch commit model for large purge operations
- Below-normal priority for database contention

---

## Key Files

| File | Lines | Purpose |
|------|------:|---------|
| ShrinkPharm.c | 431 | Main program with purge logic |
| MacODBC_MyOperators.c | 133 | SQL operator definitions (5 operators) |
| MacODBC_MyCustomWhereClauses.c | 10 | Placeholder for custom WHERE clauses |
| Makefile | 26 | Unix build configuration |
| **Total** | **600** | (574 source + 26 build) |

---

## How to Use This Documentation

### File Guide

| Document | Contains |
|----------|----------|
| [01_PROGRAM_SPECIFICATION.md](01_PROGRAM_SPECIFICATION.md) | Overview, file structure, functions, dependencies |
| [02_SYSTEM_ARCHITECTURE.md](02_SYSTEM_ARCHITECTURE.md) | Process model, data flow, integration points |
| [03_TECHNICAL_ANALYSIS.md](03_TECHNICAL_ANALYSIS.md) | Function analysis, SQL operators, error handling |
| [04_BUSINESS_LOGIC.md](04_BUSINESS_LOGIC.md) | Purge algorithm, configuration model, operational behavior |
| [05_CODE_ARTIFACTS.md](05_CODE_ARTIFACTS.md) | Key code snippets with line references |
| [VALIDATION_REPORT.md](VALIDATION_REPORT.md) | Quality verification and certification |

### Reading Order

1. **Quick orientation:** Read this README
2. **Technical overview:** 01_PROGRAM_SPECIFICATION.md
3. **System context:** 02_SYSTEM_ARCHITECTURE.md
4. **Deep dive:** 03_TECHNICAL_ANALYSIS.md + 04_BUSINESS_LOGIC.md
5. **Code reference:** 05_CODE_ARTIFACTS.md
6. **Quality check:** VALIDATION_REPORT.md

---

## Purge Algorithm Summary

Based on code analysis:

1. Connect to database (ODBC, retry until connected)
2. Read enabled configurations from `shrinkpharm` table
3. For each target table:
   - Calculate cutoff: `MinDateToRetain = SysDate - days_to_retain`
   - Select rows where date column < MinDateToRetain
   - Delete rows using `WHERE CURRENT OF` cursor
   - Commit every `commit_count` deletions
   - Record statistics back to control table
4. Log totals and exit

---

## Configuration

### Database Settings (ShrinkPharm.c:124-126)

| Parameter | Value | Purpose |
|-----------|-------|---------|
| LOCK_TIMEOUT | 1000 ms | Wait patiently for locks |
| DEADLOCK_PRIORITY | -2 | Yield to real-time operations |
| ODBC_PRESERVE_CURSORS | 1 | Keep cursors open across commits |

### Environment Variables

| Variable | Purpose |
|----------|---------|
| MAC_LOG | Log file directory |
| MS_SQL_ODBC_DSN | ODBC data source name |
| MS_SQL_ODBC_USER | Database username |
| MS_SQL_ODBC_PASS | Database password |
| MS_SQL_ODBC_DB | Database name |

---

## Related Components

| Component | Relationship |
|-----------|--------------|
| [FatherProcess](../FatherProcess/) | Uses same SQLMD_connect() retry pattern; not a supervisor of ShrinkPharm |
| SqlServer | Comments reference ShrinkPharm as suitable for ODBC_PRESERVE_CURSORS |
| GenLib | Provides date/time utilities, logging |
| GenSql | Provides ODBC abstraction layer |

---

## Known Issues

Based on code analysis:

1. **Copy-paste artifact:** TerminateHandler logs "As400UnixServer" instead of "ShrinkPharm" (`ShrinkPharm.c:388-390, 426-429`)

2. **Unused globals:** Three variables declared but not used: `need_rollback`, `recs_to_commit`, `recs_committed` (`ShrinkPharm.c:30-32`)

3. **NULL pointer risk:** `getenv("MAC_LOG")` used without NULL check (`ShrinkPharm.c:72`)

4. **Dynamic SQL:** Table names from control table are interpolated into SQL without validation - document risk only

---

## Build Instructions

According to Makefile:

```bash
# Build
make all

# Clean
make clean
```

**Target:** ShrinkPharm.exe
**Libraries:** -lodbc
**Defines:** _GNU_SOURCE

---

## Documentation Limitations

### What CANNOT be determined from code:

- Exact schema of the `shrinkpharm` control table (DDL)
- Which tables are configured for purging in production
- How and when ShrinkPharm is scheduled to run
- Business justification for retention policies

### What IS known from code:

- The purge algorithm and configuration model
- Database connection and error handling patterns
- Batch commit and statistics recording behavior
- Priority settings for database contention

---

*Documentation generated by CIDRA Documenter Agent*
*Task ID: DOC-SHRINK-001*
