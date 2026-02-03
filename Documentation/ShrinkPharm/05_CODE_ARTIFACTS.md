# ShrinkPharm - Code Artifacts

**Document ID:** DOC-SHRINK-001
**Generated:** 2026-02-03
**Source:** source_code/ShrinkPharm/ (574 source lines verified)

---

## Key Code Snippets

### File Header (ShrinkPharm.c:1-11)

```c
/* << -------------------------------------------------------- >> */
/*                                                           	  */
/*  			  ShrinkPharm.c				                      */
/*                                                           	  */
/*  -----------------------------------------------------------   */
/*                                                           	  */
/*  Purpose :													  */
/*       ODBC equivalent of ShrinkDoctor for use in the MS-SQL	  */
/*		 Pharmacy application									  */
/*                                                           	  */
/* << -------------------------------------------------------- >> */
```

### Global Declarations (ShrinkPharm.c:13-32)

```c
#define MAIN
char *GerrSource = __FILE__;

#include <MacODBC.h>

void	TerminateHandler	(int signo);

static int	restart_flg		= 0;
int	caught_signal			= 0;	// DonR 30Nov2003: Avoid "unresolved external" compilation error.
int TikrotProductionMode	= 1;	// To avoid "unresolved external" compilation error.

struct sigaction	sig_act_terminate;

static int	need_rollback			= 0;
static int	recs_to_commit			= 0;
static int	recs_committed			= 0;
```

**Note:** Variables `need_rollback`, `recs_to_commit`, and `recs_committed` appear to be declared but unused in current code.

---

### Database Configuration (ShrinkPharm.c:119-137)

```c
// Connect to database.
// Set MS-SQL attributes for query timeout and deadlock priority.
// For ShrinkPharm, we want a longish timeout (so we'll be pretty
// patient) and a below-normal deadlock priority (since we want to
// privilege real-time application operations over housekeeping).
LOCK_TIMEOUT			= 1000;	// Milliseconds.
DEADLOCK_PRIORITY		= -2;	// 0 = normal, -10 to 10 = low to high priority.
ODBC_PRESERVE_CURSORS	= 1;	// So all cursors will be preserved after COMMITs.

do
{
    SQLMD_connect ();
    if (!MAIN_DB->Connected)
    {
        sleep (10);
        GerrLogFnameMini ("ShrinkPharm_log", GerrId, "\n\nRetrying %s ODBC connect.", MAIN_DB->Name);
    }
}
while (!MAIN_DB->Connected);
```

---

### Purge Date Calculation (ShrinkPharm.c:187-191)

```c
strip_spaces (table_name);
strip_spaces (date_column_name);
MinDateToRetain = IncrementDate (SysDate, 0 - days_to_retain);

GerrLogFnameMini ("ShrinkPharm_log", GerrId, "\nPurge from %s where %s < %d...", table_name, date_column_name, MinDateToRetain);
```

---

### Dynamic SQL Generation (ShrinkPharm.c:193-216)

```c
if (use_where_clause)
{
    sprintf (	PredictQtyCommand,
                "SELECT	CAST (count(*) AS INTEGER) FROM %s WHERE %s < %d",
                table_name, date_column_name, MinDateToRetain);

    ExecSQL (	MAIN_DB, ShrinkPharmSelectQuantity, &PredictQtyCommand, &PredictedQuantity, END_OF_ARG_LIST	);

    GerrLogFnameMini ("ShrinkPharm_log", GerrId, "%'d rows to delete, SQLCODE = %d.", PredictedQuantity, SQLCODE);


    sprintf (	SelectCursorCommand,
                "SELECT %s FROM %s WHERE %s < %d",
                date_column_name, table_name, date_column_name, MinDateToRetain);
}
else
{
    sprintf (	SelectCursorCommand,
                "SELECT %s FROM %s",
                date_column_name, table_name);
}

sprintf (DeleteCommand, "DELETE FROM %s WHERE CURRENT OF ShrinkPharmSelCur", table_name);
```

---

### Batch Delete and Commit (ShrinkPharm.c:266-294)

```c
ExecSQL (	MAIN_DB, ShrinkPharmDeleteCurrentRow, &DeleteCommand, END_OF_ARG_LIST	);

if (SQLERR_error_test ())
{
    break;
}
else
{
    // We shouldn't ever get no-rows-affected on a CURRENT OF command, but
    // check just to make sure.
    if (SQLERR_code_cmp (SQLERR_no_rows_affected) == MAC_TRUE)
    {
        GerrLogFnameMini ("ShrinkPharm_log", GerrId, "DELETE operation didn't delete anything!");
    }
    else
    {
        TotalRowsDeleted++;
        RowsDeletedSinceCommit++;
        RowsDeletedFullRun++;

        if (RowsDeletedSinceCommit >= commit_count)
        {
            CommitAllWork ();
            RowsDeletedSinceCommit = 0;
        }	// Need to commit.
    }	// NOT no-rows-affected.
}	// NOT a database error.
```

---

### Statistics Recording (ShrinkPharm.c:303-316)

```c
// Write results to log and to ShrinkPharm table.
SysDate	= GetDate ();
SysTime = GetTime ();
ExecSQL (	MAIN_DB, ShrinkPharmSaveStatistics,
            &SysDate, &SysTime, &TotalRowsDeleted, END_OF_ARG_LIST	);

// Because ShrinkPharmDeleteCurrentRow is sticky (to improve performance,
// since it's used repeatedly on the inner loop) we need to free it before
// moving on to the next table.
CloseCursor		(	MAIN_DB, ShrinkPharmSelectCur	);
FreeStatement	(	MAIN_DB, ShrinkPharmDeleteCurrentRow	);

GerrLogFnameMini ("ShrinkPharm_log", GerrId, "Purged %'d rows with %s < %d from %s.",
             TotalRowsDeleted, date_column_name, MinDateToRetain, table_name);
```

---

### TerminateHandler (ShrinkPharm.c:370-430)

```c
void	 TerminateHandler (int signo)
{
    char		*sig_description;
    int			time_now;
    static int	last_signo	= 0;
    static int	last_called	= 0;


    // Reset signal handling for the caught signal.
    sigaction (signo, &sig_act_terminate, NULL);

    caught_signal	= signo;
    time_now		= GetTime();


    // Detect endless loops and terminate the process "manually".
    if ((signo == last_signo) && (time_now == last_called))
    {
        GerrLogReturn (GerrId,
                       "As400UnixServer aborting endless loop (signal %d). Terminating process.",
                       signo);

        SQLMD_disconnect ();
        SQLERR_error_test ();

        ExitSonProcess ((signo == 0) ? MAC_SERV_SHUT_DOWN : MAC_EXIT_SIGNAL);
    }
    else
    {
        last_signo	= signo;
        last_called	= time_now;
    }


    // Produce a friendly and informative message in the SQL Server logfile.
    switch (signo)
    {
        case SIGFPE:
            RollbackAllWork ();
            sig_description = "floating-point error - probably division by zero";
            break;

        case SIGSEGV:
            RollbackAllWork ();
            sig_description = "segmentation error";
            break;

        case SIGTERM:
            sig_description = "user-requested program shutdown";
            break;

        default:
            sig_description = "check manual page for SIGNAL";
            break;
    }

    GerrLogReturn (GerrId,
                   "As400UnixServer received Signal %d (%s). Terminating process.",
                   signo,
                   sig_description);
}
```

**Code Quality Note:** The log messages at lines 388-390 and 426-429 incorrectly reference "As400UnixServer" instead of "ShrinkPharm". This appears to be a copy-paste artifact.

---

## SQL Operator Definitions

### MacODBC_MyOperators.c - Control Cursor (lines 90-99)

```c
case ShrinkPharmControlCur:
            SQL_CommandText		=	"	SELECT		table_name,			days_to_retain,		date_column_name,	"
                                    "				use_where_clause,	commit_count							"
                                    "	FROM		shrinkpharm													"
                                    "	WHERE		purge_enabled	<> 0										"
                                    "	ORDER BY	table_name													";
            CursorName			=	"ShrinkPharmCtrlCur";
            NumOutputColumns	=	5;
            OutputColumnSpec	=	"	char(30), int, char(30), short, int	";
            break;
```

### MacODBC_MyOperators.c - Save Statistics (lines 102-110)

```c
case ShrinkPharmSaveStatistics:
            SQL_CommandText		=	"	UPDATE	shrinkpharm					"
                                    "	SET		last_run_date		= ?,	"
                                    "			last_run_time		= ?,	"
                                    "			last_run_num_purged	= ?		"
                                    "	WHERE CURRENT OF ShrinkPharmCtrlCur	";
            NumInputColumns		=	3;
            InputColumnSpec		=	"	int, int, int	";
            break;
```

### MacODBC_MyOperators.c - Select Quantity (lines 113-117)

```c
case ShrinkPharmSelectQuantity:
            SQL_CommandText		=	NULL;	// Variable SQL text set up by calling routine. Output will be one INTEGER (COUNT(*)).
            NumOutputColumns	=	1;
            OutputColumnSpec	=	"	int	";
            break;
```

### MacODBC_MyOperators.c - Select Cursor (lines 120-125)

```c
case ShrinkPharmSelectCur:
            SQL_CommandText		=	NULL;	// Variable SQL text set up by calling routine. Output will be one INTEGER (a date).
            CursorName			=	"ShrinkPharmSelCur";
            NumOutputColumns	=	1;
            OutputColumnSpec	=	"	int	";
            break;
```

### MacODBC_MyOperators.c - Delete Current Row (lines 127-130)

```c
case ShrinkPharmDeleteCurrentRow:
            SQL_CommandText		=	NULL;	// Variable SQL text set up by calling routine.
            StatementIsSticky	=	1;
            break;
```

---

## Build Configuration

### Makefile (lines 1-26)

```makefile
include ../Util/Defines.mak

# Files
# ----
SRCS	=	ShrinkPharm.c
OBJS	=	ShrinkPharm.o
#INTERMS	=	ShrinkPharm.c	#Marianna 24.05.2020
TARGET = ShrinkPharm.exe

include  ../Util/Rules.mak

# Dependencies
# ------------
.TARGET: all

all: $(TARGET)

$(TARGET):  $(OBJS)
	$(CC) -o $(TARGET) $(OBJS) $(LIB_OPTS) $(ESQL_LIB_OPTS) -lodbc -D_GNU_SOURCE

ShrinkPharm.o			:	ShrinkPharm.c
	$(CC)	$(INC_OPTS)	$(COMP_OPTS)	ShrinkPharm.o	ShrinkPharm.c -lodbc -D_GNU_SOURCE

clean:
	rm $(TARGET) $(OBJS)
```

---

## Custom WHERE Clauses Placeholder

### MacODBC_MyCustomWhereClauses.c (lines 1-10)

```c


/*
		case WHERE_TEMPMEMB:	// Dummy just to show what needs to be set.
					WhereClauseText		=	"	cardid = ?	";
					NumInputColumns		=	1;
					InputColumnSpec		= "int";
					break;
*/

```

This file contains only a commented example, indicating it is a placeholder for future custom WHERE clause definitions.

---

## Run-Time Statistics Calculation (ShrinkPharm.c:330-356)

```c
// Compute the total run time in seconds, then convert to minutes for output.
// If the initial calculation gives us a negative run duration, the run went
// past midnight - so add one day (in seconds).
EndTime = GetTime ();
RunLenMinutes  = (3600 * (EndTime   / 10000)) + (60 * ((EndTime   % 10000) / 100)) + (EndTime   % 100);
RunLenMinutes -= (3600 * (StartTime / 10000)) + (60 * ((StartTime % 10000) / 100)) + (StartTime % 100);

if (RunLenMinutes < 0)
    RunLenMinutes += 24 * 60 * 60;

// It shouldn't happen in any realistic scenario, but just in case,
// avoid division by zero.
if (RunLenMinutes == 0)
    RunLenMinutes = 1;


// Calculate deletion rate per minute. Note that we calculate based on seconds *before*
// we round to get the number of minutes of the run!
DeletionsPerMinute = (RowsDeletedFullRun * 60) / RunLenMinutes;

// Convert run length to minutes, rounding up.
RunLenMinutes = (RunLenMinutes + 30) / 60;


GerrLogFnameMini ("ShrinkPharm_log", GerrId, "\nShrinkPharm.exe finished %d at %d.", SysDate, SysTime);
GerrLogFnameMini ("ShrinkPharm_log", GerrId, "%'d total rows deleted in %'d minutes.", RowsDeletedFullRun, RunLenMinutes);
GerrLogFnameMini ("ShrinkPharm_log", GerrId, "%'d average rows deleted / minute.", DeletionsPerMinute);
```

**Note:** Time values appear to be stored as integers in HHMMSS format (e.g., 143025 = 14:30:25).

---

*Document generated by CIDRA Documenter Agent*
*Task ID: DOC-SHRINK-001*
