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

#define MAIN
char *GerrSource = __FILE__;

#include <MacODBC.h>
//#include "MsgHndlr.h"						//Marianna	24052020
//#include "As400UnixClient.h"				//Marianna	24052020
//#include "PharmDBMsgs.h"					//Marianna	24052020
//#include <netinet/tcp.h>

void	TerminateHandler	(int signo);

static int	restart_flg		= 0;
int	caught_signal			= 0;	// DonR 30Nov2003: Avoid "unresolved external" compilation error.
int TikrotProductionMode	= 1;	// To avoid "unresolved external" compilation error.

struct sigaction	sig_act_terminate;

static int	need_rollback			= 0;
static int	recs_to_commit			= 0;
static int	recs_committed			= 0;


int  main (int argc, char *argv[])
{
	int			retrys;
	int			mypid;
	int			reStart;
	int			TotalRowsDeleted;
	int			RowsDeletedSinceCommit;
	int			RowsDeletedFullRun		= 0;
	int			SysDate;
	int			SysTime;
	int			StartTime;
	int			EndTime;
	int			RunLenMinutes;
	int			DeletionsPerMinute;
	sigset_t	NullSigset;
	char		szHostName			[ 256 + 1 ];

	// ShrinkPharm table.
	char		table_name			[30 + 1];
	char		date_column_name	[30 + 1];
	short		use_where_clause;
	int			days_to_retain;
	int			commit_count;

	// Working variables;
	int			MinDateToRetain;
	int			DateOfRow;
	int			PredictedQuantity;
	char		SelectCursorCommand	[600 + 1];
	char		PredictQtyCommand	[600 + 1];
	char		DeleteCommand		[300 + 1];


	mypid	= getpid ();
	SysDate	= GetDate ();
	StartTime = SysTime = GetTime ();

	strcpy (LogDir, getenv ("MAC_LOG"));
	strcpy (LogFile, "ShrinkPharm_log");

	GerrLogFnameMini (	"ShrinkPharm_log", GerrId,
						"\n\n=======================================================================\n\n"
						"ShrinkPharm.exe started %d at %d.", SysDate, SysTime);

	// Determine if we're running on a Test system. The default is Production.
	if (!gethostname (szHostName, 255))
	{
		if ((!strcmp (szHostName, "linux01-test"))	||
			(!strcmp (szHostName, "linux01-qa"))		||
			(!strcmp (szHostName, "pharmlinux-test"))	||
			(!strcmp (szHostName, "pharmlinux-qa")))
		{
			TikrotProductionMode = 0;
		}
	}


	// DonR 13Feb2020: Add signal handler for segmentation faults - used
	// by ODBC routines to detect invalid pointer parameters.
	memset ((char *)&NullSigset, 0, sizeof(sigset_t));

//	sig_act_ODBC_SegFault.sa_handler	= ODBC_SegmentationFaultCatcher;
//	sig_act_ODBC_SegFault.sa_mask		= NullSigset;
//	sig_act_ODBC_SegFault.sa_flags		= 0;
//
	sig_act_terminate.sa_handler		= TerminateHandler;
	sig_act_terminate.sa_mask			= NullSigset;
	sig_act_terminate.sa_flags			= 0;

//	if (sigaction (SIGSEGV, &sig_act_ODBC_SegFault, NULL) != 0)
//	{
//		GerrLogReturn (GerrId,
//					   "Can't install signal handler for SIGSEGV" GerrErr,
//					   GerrStr);
//	}
//
	if (sigaction (SIGFPE, &sig_act_terminate, NULL) != 0)
	{
		GerrLogReturn (GerrId,
					   "Can't install signal handler for SIGFPE" GerrErr,
					   GerrStr);
	}


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


	do
	{
		// Main loop - scan through the ShrinkPharm table and build the scan and update queries.
		// NOTE: In order to remain open and advance properly when there are intermediate
		// COMMITs, cursors need to have a cursor name defined - even if there are no updates
		// performed on them. For ShrinkPharmControlCur, I added some status-saving columns
		// and an UPDATE statement - but even if I hadn't, it would need its cursor name.
		DeclareAndOpenCursorInto (	MAIN_DB, ShrinkPharmControlCur,
									&table_name,		&days_to_retain,
									&date_column_name,	&use_where_clause,
									&commit_count,		END_OF_ARG_LIST		);

		if (SQLERR_error_test ())
		{
			break;
		}

		for (;;)
		{
			// Fetch ShrinkPharm rows and test DB errors.
			FetchCursor (	MAIN_DB, ShrinkPharmControlCur	);

			if (SQLERR_code_cmp (SQLERR_end_of_fetch) == MAC_TRUE)
			{
				break;
			}
			else
			{
				if (SQLERR_error_test ())
				{
					break;
				}
			}	// Something other than end-of-fetch.

			// If we get here, we successfully read a ShrinkPharm row.
			// The old ShrinkDoctor program scans through the whole table, and compares
			// the date it reads with the date it computes rather than using a WHERE
			// clause; the reason is that not all the tables involved had indexes on
			// the required date fields. This program will be a little more flexible:
			// if use_where_clause is non-zero, it'll construct a SELECT cursor with
			// a WHERE clause; but if the table doesn't have the right index and we
			// don't want to add one, use_where_clause can be set FALSE (= 0) and we'll
			// operate using the same logic as the old program.
			// Note that we do *not* want to add the FOR UPDATE clause here, because
			// (I think) the ODBC infrastructure routine will do it automatically and
			// (since the original SQL command text in MacODBC_MyOperators.c is NULL)
			// a FOR UPDATE we supply will not be noticed. We don't want two FOR UPDATEs!
			strip_spaces (table_name);
			strip_spaces (date_column_name);
			MinDateToRetain = IncrementDate (SysDate, 0 - days_to_retain);

			GerrLogFnameMini ("ShrinkPharm_log", GerrId, "\nPurge from %s where %s < %d...", table_name, date_column_name, MinDateToRetain);

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

//			// TEMPORARY FOR TESTING: USE "FAKE" DELETE COMMAND SO WE DON'T RUN OUT
//			// OF STUFF TO "PURGE".
//			if (!strcasecmp (table_name, "members"))
//				sprintf (DeleteCommand, "UPDATE %s SET first_english = 'FakeName' WHERE CURRENT OF ShrinkPharmSelCur", table_name);
//			if (!strcasecmp (table_name, "prescriptions"))
//				sprintf (DeleteCommand, "UPDATE %s SET user_ident = 123456 WHERE CURRENT OF ShrinkPharmSelCur", table_name);
//GerrLogFnameMini ("ShrinkPharm_log", GerrId, "Select command: {%s}", SelectCursorCommand);
//GerrLogFnameMini ("ShrinkPharm_log", GerrId, "Delete command: {%s}", DeleteCommand);

			// Declare and open the deletion (or full-table) cursor.
			DeclareAndOpenCursorInto (	MAIN_DB, ShrinkPharmSelectCur, &SelectCursorCommand, &DateOfRow, END_OF_ARG_LIST	);

//GerrLogFnameMini ("ShrinkPharm_log", GerrId, "Opened ShrinkPharmSelectCur, SQLCODE = %d.", SQLCODE);

			if (SQLERR_error_test ())
			{
				break;
			}

			for (TotalRowsDeleted = RowsDeletedSinceCommit = 0; ; )
			{
				// Fetch ShrinkPharm rows and test DB errors.
				FetchCursor (	MAIN_DB, ShrinkPharmSelectCur	);

				if (SQLERR_code_cmp (SQLERR_end_of_fetch) == MAC_TRUE)
				{
					break;
				}
				else
				{
					if (SQLERR_error_test ())
					{
						break;
					}
				}	// Something other than end-of-fetch.

				// If we're not using a WHERE clause, perform the equivalent here.
				if ((!use_where_clause) && (DateOfRow >= MinDateToRetain))
				{
					continue;
				}

if (0)
{
						TotalRowsDeleted++;
						RowsDeletedSinceCommit++;
}
else
{
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
//							GerrLogFnameMini ("ShrinkPharm_log", GerrId, "Committed %d deletions for table %s, SQLCODE = %d.", RowsDeletedSinceCommit, table_name, SQLCODE);
							RowsDeletedSinceCommit = 0;
						}	// Neet to commit.
					}	// NOT no-rows-affected.
				}	// NOT a database error.
}
			}	// Loop through all SELECTed rows in table.

			if (RowsDeletedSinceCommit > 0)
			{
				CommitAllWork ();
//				GerrLogFnameMini ("ShrinkPharm_log", GerrId, "Committed %d rows for table %s, SQLCODE = %d.", RowsDeletedSinceCommit, table_name, SQLCODE);
			}	// Neet to commit last batch.

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
		}	// Loop through ShrinkPharm table.

	} while (0);
	// Dummy loop for error handling.

	CloseCursor (	MAIN_DB, ShrinkPharmControlCur	);

//		Conflict_Test (reStart);

	// We should already have everything COMMITted, but just in case...
	CommitAllWork ();
	SQLMD_disconnect ();

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

	exit(0);
}



/*=======================================================================
||																		||
||			     TerminateHandler ()									||
||																		||
|| Note: This routine should handle any terminating/fatal signal        ||
||       that doesn't require special processing.                       ||
 =======================================================================*/
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

