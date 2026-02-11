/* << -------------------------------------------------------- >> */
/*                                                           	  */
/*  			  As400UnixClient.c                      */
/*                                                           	  */
/*  -----------------------------------------------------------   */
/*                                                           	  */
/*  Date: 	 Oct-Dec 1996					  */
/*  Written by:  Gilad Haimov ( Reshuma Ltd. )			  */
/*                                                           	  */
/*  -----------------------------------------------------------   */
/*                                                           	  */
/*  Date: 	 Jul     1997					  */
/*  Updated by:  Ely Levy ( Reshuma Ltd. )			  */
/*                                                           	  */
/*  -----------------------------------------------------------   */
/*                                                           	  */
/*  Purpose :							  */
/*       Source file for AS400 <-> Unix client process:		  */
/*       get records from LOCAL DB & pass them to REMOTE DB.	  */
/*                                                           	  */
/* << -------------------------------------------------------- >> */

#define MAIN
static char *GerrSource = __FILE__;

// debug level
//#define AS400_DEBUG

#include <MacODBC.h>
#include "MsgHndlr.h"						//Marianna	24052020
#include "As400UnixClient.h"				//Marianna	24052020
#include "PharmDBMsgs.h"					//Marianna	24052020
#include <netinet/tcp.h>


/**********  debug macro  **************/
#define ASSERT_FIELD_LEN( p )  \
       if( ! (p) ) \
       {\
		  GerrLogReturn( GerrId,\
				"ASSERT_FIELD_LEN -> >%s<..",\
				 #p );\
		  popname();\
		  return SB_LOCAL_DB_ERROR;\
       }


static int	restart_flg			= 0;
static int	commit_msg_counter	= 1;	// DonR 14Mar2013: Increments by 2 each time; loops from 9999 to 1.
int	message_num = 0;
int message_len = 0;   /* Yulia 09.0.2000 */
int	caught_signal = 0;				// DonR 30Nov2003: Avoid "unresolved external" compilation error.
int AS400_database_involved = 1;	// DonR 09Jan2005.
int TikrotProductionMode	= 1;	// To avoid "unresolved external" compilation error.

void	pipe_handler( int );
void	alrm_handler( int );
void	inform_handler( int );
void	change_handler( int );
struct sigaction	sig_act_pipe;
struct sigaction	sig_act_alrm;
struct sigaction	sig_act_inform;
struct sigaction	sig_act_change;
extern short _PauseAfterFailedReconnect;

void	sleep_check( int	seconds, int *system_down_flg );
int	CleanPresc( int presc_id, int reason );


// DonR 11Jan2024: Use this version of send_buffer_to_server if you need to diagnose
// exactly what we're sending to AS/400 - including a character-by-character dump to
// check on ASCII-to-EBCDIC translation.
static TSuccessStat send_buffer_to_server_diagnostic (	TSocket		sock,
														const char	*buf,
														int			len,
														int			diagnose_from,
														int			Mizaheh);


static int	check_2502_size = 1;	// Default: need to check size of Delivered Prescription batch.
									// DonR 23SEP2002.

static int	need_rollback = 0;		// DonR 09OCT2002.

int	manual_change_of_tables = 0;

static int	recs_to_commit			= 0;
static int	recs_committed			= 0;
static int	trailer_recs_to_commit	= 0;
static int	trailer_recs_committed	= 0;


void switch_to_win_heb	(unsigned char * source);
void ReadIniFilePort	(void);  /*20020828*/

// DonR 12DEC2002: Server now sends response messages of different lengths!
// DonR 14Mar2013: Added two additional server response fields.
#define get_msg_from_server(wait_time)	get_server_response(wait_time,4,NULL,NULL,NULL,NULL)

// Function call tracing - DonR 13OCT2002.
#define FTRACE_NONE		0
#define	FTRACE_CALLS	1
#define FTRACE_CALL_RTN	2
#define	FTRACE_MAXDEPTH	1000
#define FTRACE_FILENAME	"/pharm/log/as400unixclient_ftrace_log"

// DonR 02May2005: Disable call-stack tracing.
//#define POP_RET(x) {popname();return(x);}
#define POP_RET(x) {return(x);}

int pushname	(char *name);
int popname		(void);

static char		*fname[FTRACE_MAXDEPTH];
static FILE		*tracefile = NULL;
static int		fdepth = 0;
static int		ftracemode = FTRACE_NONE;
static short	Production	= 1;

short	MinPharmacyToAs400;
char	h_szHostName[ 256 + 1 ];


/* << -------------------------------------------------------- >> */
/*                                                           	  */
/*                                                           	  */
/*                            M A I N                          	  */
/*                                                           	  */
/*                                                           	  */
/* << -------------------------------------------------------- >> */
int  main (int		argc, char		*argv[])
{

/*=======================================================================
||									||
||			Local variables					||
||									||
 =======================================================================*/

  int		retrys;			/* used by INITSONPROCESS      */
  int		rc;			/* terminate/restart process   */
  int		retval;			/* process return-code	       */
  bool		exit_flg;		/* if true - terminate process */
  int		state;
  int		system_down_flg;
  short		DummyShort;	// TEMPORARY FOR ERROR TESTING.
  sigset_t	NullSigset;

/*=======================================================================
||									||
||			Get Command line arguments			||
||									||
 =======================================================================*/

  pushname ("as400unixclient main");

  GetCommonPars (argc, argv, &retrys);

/*=======================================================================
||									||
||			Initialization					||
||									||
 =======================================================================*/

	ReadIniFilePort ();

	// DonR 14May2015: Because this program goes into a rapid loop and fills the disk with logged error
	// messages if the database is down, enable a one-minute pause after a failed auto-reconnect to the
	// database.
	_PauseAfterFailedReconnect = 1;

//    glbInTransaction = 0;

    RETURN_ON_ERR (InitSonProcess (argv[0], UNIXTOAS400_TYPE, retrys, 0, PHARM_SYS));


	memset ((char *)&NullSigset, 0, sizeof(sigset_t));

    sig_act_pipe.sa_handler = pipe_handler;
    sig_act_pipe.sa_flags = 0;

    sigaction( SIGPIPE, &sig_act_pipe, NULL );

    sig_act_alrm.sa_handler = alrm_handler;
    sig_act_alrm.sa_flags = 0;

    sigaction( SIGALRM, &sig_act_alrm, NULL );

    sig_act_inform.sa_handler = inform_handler;
    sig_act_inform.sa_flags = 0;

    sigaction( SIGUSR1, &sig_act_inform, NULL );

    sig_act_change.sa_handler = change_handler;
    sig_act_change.sa_flags = 0;

    sigaction( SIGUSR2, &sig_act_change, NULL );

//	// DonR 13Feb2020: Add signal handler for segmentation faults - used
//	// by ODBC routines to detect invalid pointer parameters.
//	sig_act_ODBC_SegFault.sa_handler	= ODBC_SegmentationFaultCatcher;
//	sig_act_ODBC_SegFault.sa_mask		= NullSigset;
//	sig_act_ODBC_SegFault.sa_flags		= 0;
//
//	if (sigaction (SIGSEGV, &sig_act_ODBC_SegFault, NULL) != 0)
//	{
//		GerrLogReturn (GerrId,
//					   "Can't install signal handler for SIGSEGV" GerrErr,
//					   GerrStr);
//	}
//

	// Connect to database.
	// DonR 03Mar2021: Set MS-SQL attributes for query timeout and deadlock priority.
	// For As400UnixClient, we want a mid-length timeout (so we'll be reasonably
	// patient) and a slightly below-normal deadlock priority (since we want to
	// privilege letting the Pharmacy Server program getting responses to pharmacy
	// over sending updates to AS/400).
	LOCK_TIMEOUT			= 500;	// Milliseconds.
	DEADLOCK_PRIORITY		= -1;	// 0 = normal, -10 to 10 = low to high priority.
	ODBC_PRESERVE_CURSORS	= 1;	// So all cursors will be preserved after COMMITs.

	do
	{
		SQLMD_connect ();
		if (!MAIN_DB->Connected)
		{
			sleep_check( 10, &system_down_flg );
			GerrLogMini (GerrId, "\n\n\nRetrying %s ODBC connect.", MAIN_DB->Name);
		}
	}
	while (!MAIN_DB->Connected);


	/*=======================================================================
	||																		||
	||			Restart connection loop										||
	||																		||
	=======================================================================*/

	exit_flg  = false;
	glbSocket = -1;
	system_down_flg = 0;

	// Determine if we're running on a Test system. The default is Production.
	if (!gethostname (h_szHostName, 255))
	{
		if ((!strcmp (h_szHostName, "linux01-test"))	||
			(!strcmp (h_szHostName, "linux01-qa"))		||
			(!strcmp (h_szHostName, "pharmlinux-test"))	||
			(!strcmp (h_szHostName, "pharmlinux-qa")))
		{
			Production = 0;
		}
	}


	while (1)
	{
		// Make a connection to server
		if (initialize_connection_to_server () == failure)
		{
			GerrLogMini (GerrId,
						 "\nMain: attempt to connect to server failed - system_down = %d.",
						 system_down_flg);

			// If server is down, no point in filling up log-file with frequent retries.
			// For now, hard-code to 300 seconds; later, add a setup parameter, perhaps.
			//sleep_check( SLEEP_BETWEEN_TABLES, &system_down_flg );
			sleep_check (300, &system_down_flg);

			if (system_down_flg)
			{
				break;
			}

			continue;
		}

		GerrLogMini (GerrId, "\nMain: connected to server.");

		puts("AS400 CLIENT IS CONNECTED TO SERVER" );
		fflush (stdout);

		restart_flg = 0;


		// Loop on tables and send each one to as400
		rc = send_table_sequence_to_remote_DB ();

		switch (rc)
		{
			case RC_RESTART_CONNECTION:
				GerrLogReturn (GerrId, "Main: pausing 5 minutes before restarting.");
				sleep_check (300, &system_down_flg);
				continue;

			case RC_FATAL_ERROR_OCCURED:
				retval = MAC_SERV_RESET;		// FatherProc should restart process
				exit_flg = true;
				break;

			case RC_FATHER_GOING_DOWN:
				retval = MAC_SERV_SHUT_DOWN;	// no restarting process */
				exit_flg = true;
				break;

			default:
				GerrLogReturn (GerrId, "Main: unknown return code %d from send_table_seq.", rc);
				break;
		}

		if( exit_flg == true )
		{
			break;
		}

	}	// while


/*=======================================================================
||									||
||		Close connection to database & server			||
||									||
 =======================================================================*/

    terminate_communication_and_DB_connections();

    ExitSonProcess( retval );

}



/* << -------------------------------------------------------- >> */
/*                                                           	  */
/*                                                           	  */
/*              U T I L I T Y    F U N C T I O N S		  */
/*                                                           	  */
/*                                                           	  */
/* << -------------------------------------------------------- >> */



/* << -------------------------------------------------------- >> */
/*                                                           	  */
/*      send the entire table-sequence to remote database		  */
/*																  */
/*	   return codes:											  */
/*			RC_FATAL_ERROR_OCCURED								  */
/*			RC_RESTART_CONNECTION								  */
/*			RC_FATHER_GOING_DOWN								  */
/*																  */
/*	   evaluated in main().										  */
/*																  */
/*                                                           	  */
/* << -------------------------------------------------------- >> */

static int send_table_sequence_to_remote_DB (void)

{

	int		rc;		/* code returned from message handler */
	int		i;
	int		system_down_flg;
	int		started_date;
	int		started_time;
	int		tm_now;
	bool	something_was_run;

	int		len;
	int		trans;
	int		seq_number;
	int		frequency_minutes;
	int		per_1_start;
	int		per_1_end;
	int		per_2_start;
	int		per_2_end;
	int		per_3_start;
	int		per_3_end;
	short	always_on;

	struct	{	int		ID;
				int		len;
				int		freq;
				int		done_date;
				int		done_time;
				short	always_enabled;
				int		period_1_start;
				int		period_1_end;
				int		period_2_start;
				int		period_2_end;
				int		period_3_start;
				int		period_3_end;
			}
			Schedule [100];


	pushname ("send_table_sequence_to_remote_DB");

	/*===========================================================================
	||      Read all records from as400clientpar to Schedule structure			||
	||		ordered by seq_number												||
	============================================================================*/

	DeclareAndOpenCursor (	MAIN_DB, AS400CL_as400clientpar_cur	);

	SQLERR_error_test ();


	i = 0;
	while (1)
	{
		FetchCursorInto (	MAIN_DB, AS400CL_as400clientpar_cur,
							&trans,				&len,			&seq_number,
							&frequency_minutes,	&always_on,		&per_1_start,
							&per_1_end,			&per_2_start,	&per_2_end,
							&per_3_start,		&per_3_end,		END_OF_ARG_LIST		);

//GerrLogMini(GerrId, "Trn %d: freq %d, always on %d, SQLCODE = %d.", trans, frequency_minutes, always_on, SQLCODE);

		if (SQLERR_code_cmp (SQLERR_end_of_fetch) == MAC_TRUE)
		{
			Schedule[i].ID = 0;
			break;
		}

		SQLERR_error_test ();


		Schedule[i].ID				= trans;
		Schedule[i].len				= len;
		Schedule[i].freq			= frequency_minutes * 60;
		Schedule[i].always_enabled	= always_on;
		Schedule[i].period_1_start	= per_1_start;
		Schedule[i].period_1_end	= per_1_end;
		Schedule[i].period_2_start	= per_2_start;
		Schedule[i].period_2_end	= per_2_end;
		Schedule[i].period_3_start	= per_3_start;
		Schedule[i].period_3_end	= per_3_end;
		Schedule[i].done_date		= 0;
		Schedule[i].done_time		= 0;

		i++;
	}

	CloseCursor (	MAIN_DB, AS400CL_as400clientpar_cur	);

	SQLERR_error_test();


	// DonR 31Jul2012: Get additional parameters from sysparams table.
	// DonR 15Jan2020: It's time (and past time!) to get rid of all references
	// to the long-dead prescr_wr_new - so get rid of all the logic based on
	// sysparams/use_new_doc_rx_tbl.
	ExecSQL (	MAIN_DB, AS400CL_READ_sysparams,
				&MinPharmacyToAs400, END_OF_ARG_LIST	);
//GerrLogMini (GerrId, "AS400CL_READ_sysparams: MinPharmacyToAs400 = %d, SQLCODE = %d.", MinPharmacyToAs400, SQLCODE);

	if (SQLCODE != 0)
	{
		MinPharmacyToAs400	= 1000;	// Default: send drug sales only for pharmacies with codes > 999.
//		UseNewDocRxTable	= 0;	// Default to send prescr_wr_new to RK9049, at least for now.
	}


	/*=======================================================================
	||																		||
	||		Loop on tables and send each one to as400						||
	||																		||
	=======================================================================*/
	while (1)
	{
		something_was_run = false;
		tm_now = GetTime ();

		for (i = 0; Schedule[i].ID != 0; i++)
		{
			// DonR 20Jul2016: Add near-real-time processing of queued doctor-prescription updates.
			ProcessQueuedRxUpdates ();

			// If insufficient time has passed since last successful attempt, skip trn.
			if (((Schedule[i].freq > 0)	&&
				(diff_from_now (Schedule[i].done_date, Schedule[i].done_time) < Schedule[i].freq))

				||

				// DonR 24Oct2004: In addition to the maximum-frequency logic, optionally
				// permit given uploads to be performed during one to three specified periods.
				// This allows maximimum performance for "semi-real-time" uploads during
				// peak hours.
				((Schedule[i].always_enabled == 0)												&&
				 ((tm_now < Schedule[i].period_1_start) || (tm_now > Schedule[i].period_1_end))	&&
				 ((tm_now < Schedule[i].period_2_start) || (tm_now > Schedule[i].period_2_end))	&&
				 ((tm_now < Schedule[i].period_3_start) || (tm_now > Schedule[i].period_3_end))))
			{
				continue;
			}


			// Just for debug
			message_num = Schedule[i].ID;
			message_len = Schedule[i].len;

			GerrLogMini (GerrId, "\nTrn %d                       batch size %d.",
						 Schedule[i].ID,
						 Schedule[i].len);

			// If we've gotten here, at least one transaction wasn't skipped because of
			// the retry-frequency feature.
			something_was_run = true;

			// DonR 08Jul2003: To prevent the system's hanging - that is, reaching a certain point,
			// crashing, restarting, continuing to crash at the same point, and failing to execute
			// subsequent uploads in the list - mark the upload to be executed such that it will
			// be put at the end of the list. When the upload completes, restore its normal position.
			// If the program crashes without completing the upload, the "bad" upload will be at the
			// end of the list next time the program restarts.
			trans = Schedule[i].ID;

			ExecSQL (	MAIN_DB, AS400CL_UPD_as400clientpar_while_processing,
						&trans, END_OF_ARG_LIST									);

			CommitWork (MAIN_DB);


			// Call message handler
			started_date = GetDate ();
			started_time = GetTime ();

			// Unless this message is one involving a purely local Unix database
			// update, set flag to indicate that data must be sent to the AS400.
			if ((trans == 2517) || (trans == 2513))
				AS400_database_involved = 0;
			else
				AS400_database_involved = 1;

			rc = call_message_handler (Schedule[i].ID);

			switch (rc)
			{
				case MH_SUCCESS:
					GerrLogMini (GerrId,
								 "Trn %d successful - %d (+ %d) rows processed.",
								 Schedule[i].ID,
								 recs_committed,
								 trailer_recs_committed);
					Schedule[i].done_date = started_date;
					Schedule[i].done_time = started_time;
					break;


				case MH_MAX_ERROR_WAS_EXCEEDED:
					GerrLogReturn (GerrId,
								   "Trn %d max-error-exceeded - %d (+ %d) rows processed.",
								   Schedule[i].ID,
								   recs_committed,
								   trailer_recs_committed);
					break;


				case MH_SYSTEM_IS_BUSY:	/* -> local or remote */
					GerrLogReturn (GerrId,
								   "Trn %d got system-busy from handler - %d (+ %d) rows processed.",
								   Schedule[i].ID,
								   recs_committed,
								   trailer_recs_committed);
					break;


				case MH_FATAL_ERROR:
					GerrLogReturn (GerrId,
								   "Trn %d got FATAL_ERROR from handler - returning - %d (+ %d) rows processed!",
								   Schedule[i].ID,
								   recs_committed,
								   trailer_recs_committed);
					POP_RET (RC_FATAL_ERROR_OCCURED);


				case MH_RESTART_CONNECTION:
					GerrLogReturn (GerrId,
								   "Trn %d got RESTART_CONNECTION from handler - returning - %d (+ %d) rows processed!",
								   Schedule[i].ID,
								   recs_committed,
								   trailer_recs_committed);
					POP_RET (RC_RESTART_CONNECTION);


				case MH_FATHER_GOING_DOWN:
					GerrLogReturn (GerrId,
								   "Trn %d got FATHER_GOING_DOWN from handler - returning - %d (+ %d) rows processed!",
								   Schedule[i].ID,
								   recs_committed,
								   trailer_recs_committed);
					POP_RET (RC_FATHER_GOING_DOWN);

				default:
					GerrLogReturn (GerrId,
								   "Trn %d got unknown rtn code %d from handler - %d (+ %d) rows processed.",
								   Schedule[i].ID,
								   rc,
								   recs_committed,
								   trailer_recs_committed);
					break;

			}	// switch( ret code..


			// DonR 08Jul2003: If we managed to avoid fatal errors, reset the sequence number of the
			// just-completed upload so that it will retain its place in the queue.
			ExecSQL (	MAIN_DB, AS400CL_UPD_as400clientpar_after_processing,
						&trans, END_OF_ARG_LIST									);

			sleep_check (SLEEP_BETWEEN_TABLES, &system_down_flg);


			if (system_down_flg)
			{
				POP_RET (RC_FATHER_GOING_DOWN);
			}

		}	// for( tables..

		// If the loop completed but all transactions were skipped because of their
		// frequency-to-run, check if we should terminate the program. Note that this
		// also prevents the program from thrashing!
		if (something_was_run == false)
		{
//			sleep_check( SLEEP_BETWEEN_TABLES, &system_down_flg );
			sleep_check( 1, &system_down_flg );

			if( system_down_flg )
			{
				POP_RET( RC_FATHER_GOING_DOWN );
			}
		}	// Nothing was done within the "for" loop.

	}		// while( 1 ..

	//GerrLogReturn (GerrId, "send_table_sequence_to_remote_DB fell down below its endless loop!");
	//POP_RET (-1);

}

/* << -------------------------------------------------------- >> */
/*								  */
/*		    rollback_remote_and_local_DBs		  */
/*								  */
/* << -------------------------------------------------------- >> */
/*								  */
/*    returns:							  */
/*	RR_ROLLBACK_DONE  Or  RR_UNABLE_TO_ROLLBACK		  */
/*								  */
/* << -------------------------------------------------------- >> */
static int rollback_remote_and_local_DBs(
					 void
					 )
{
    int 	retval;

	pushname ("rollback_remote_and_local_DBs");

//    if( SQLMD_rollback() == MAC_OK )
	RollbackAllWork ();

    if (SQLCODE == 0)
    {
		retval = RR_ROLLBACK_DONE;
		need_rollback = 0;
		recs_to_commit			= 0;
		trailer_recs_to_commit	= 0;
    }
    else
    {
        retval = RR_UNABLE_TO_ROLLBACK;
    }

	if (AS400_database_involved)
	{
	    if (send_msg_and_get_confirmation (CM_REQUESTING_ROLLBACK,	/* message */
										   CM_CONFIRMING_ROLLBACK	/* confirm */
										  ) != success )
	    {
			retval = RR_UNABLE_TO_ROLLBACK;
		}
	}

	POP_RET (retval);
}



/* << -------------------------------------------------------- >> */
/*																  */
/*		      commit_remote_and_local_DBs						  */
/* << -------------------------------------------------------- >> */
/*																  */
/*    returns:													  */
/* 	COMMIT_DONE													  */
/*	COMMIT_DISCREPANCY_ROLLEDBACK								  */
/*	COMMIT_FAILED_ROLLBACK_DONE									  */
/*	COMMIT_FAILED_ROLLBACK_FAILED								  */
/*																  */
/* << -------------------------------------------------------- >> */

static int commit_remote_and_local_DBs (void)
{
	int		retval;
	int		len;
	int		rc;
	int		server_rows;
	int		server_trailer_rows;
	int		SysTime;
	int		SysTimeReturned		= 0;
	int		MsgCounterExpected;
	int		MsgCounterReturned	= 0;
	char	buf [64];


	pushname ("commit_remote_and_local_DBs");

	SysTime = GetTime ();

	do
	{
		if (AS400_database_involved)
		{
			// Format buffer with request code and rows-to-commit counters.
			// DonR 14Mar2013: Add timestamp and commit-message counter to Commit-N request (and response).
			len = sprintf (buf,
						   "%d%0*d%0*d%0*d%0*d",
						   CM_REQ_COMMIT_N_ROWS,
						   5, recs_to_commit,
						   5, trailer_recs_to_commit,
						   6, SysTime,
						   4, commit_msg_counter);

			// Commit Message Counter increments by two; 9997 rolls back to 1 (since maximum 4-character return is  9998).
			MsgCounterExpected = commit_msg_counter + 1;	// Always an even number, max = 9998.
			commit_msg_counter += 2;						// Always an odd number; if it's over 9997, reset to 1.
			if (commit_msg_counter > 9997)
				commit_msg_counter = 1;

			DbgAssert (len == 24);	// DonR 14Mar2013: Added two new fields, 10 characters total.

			// Send confirmation request to server.
			do
			{
				rc = send_buffer_to_server (glbSocket, buf, len);

				if (rc != success )
				{
					GerrLogReturn (GerrId,
								   "Commit_DBs tried to send %d, but send_buffer returned %d.",
								   CM_REQ_COMMIT_N_ROWS, rc);
					retval = failure;
					break;
				}

				// Get server's response to confirmation request.
				rc = get_server_response (MAX_SELECT_WAIT_TIME_IN_SECS,
										  24,
										  &server_rows,
										  &server_trailer_rows,
										  &SysTimeReturned,
										  &MsgCounterReturned);
GerrLogFnameMini ("commit_log", GerrId,
				  "Sent %s got %d/%0*d/%0*d/%d/%0*d.",
				  &buf[4], rc, 3, server_rows, 3, server_trailer_rows, SysTimeReturned, 4, MsgCounterReturned);

				// Debugging message!
				//GerrLogReturn (GerrId,
				//			   "Commit_DBs: AS400 response is %s (%d), %d (+ %d) rows.",
				//			   (rc == CM_CONF_COMMIT_N_ROWS)			?	"Confirmed" :
				//					(rc == CM_COMMIT_N_ROWS_DISCREP)	?	"Discrepancy!"
				//														:	"unrecognized",
				//			   rc,
				//			   server_rows,
				//			   server_trailer_rows);

				switch (rc)
				{
				case CM_CONF_COMMIT_N_ROWS:

					// DonR 14Mar2013: Confirm that the commitment-confirmation message we received actually pertains
					// to the commit-N-records command that we sent.
					if ((server_rows			!= recs_to_commit)			||
						(server_trailer_rows	!= trailer_recs_to_commit)	||
						(SysTimeReturned		!= SysTime)					||
						(MsgCounterReturned		!= MsgCounterExpected))
					{
						GerrLogReturn (GerrId,
									   "\nTrn. %d: incorrect commit-confirmed message received from server:\n"
									   "    Main rows:    Client = % 6d, Server = % 6d.\n"
									   "    Trailer rows: Client = % 6d, Server = % 6d.\n"
									   "    Timestamp:    Client = % 6d, Server = % 6d.\n"
									   "    Counter:      Expected % 6d, Received % 6d.\n"
									   "             Batch will be re-sent next iteration.",
									   message_num,
									   recs_to_commit,			server_rows,
									   trailer_recs_to_commit,	server_trailer_rows,
									   SysTime,					SysTimeReturned,
									   MsgCounterExpected,		MsgCounterReturned);

						GerrLogFnameMini ("CommitDiscrepancy_log", GerrId,
										  "\nTrn. %d: incorrect commit-confirmed message received from server:\n"
										  "    Main rows:    Client = % 6d, Server = % 6d.\n"
										  "    Trailer rows: Client = % 6d, Server = % 6d.\n"
										  "    Timestamp:    Client = % 6d, Server = % 6d.\n"
										  "    Counter:      Expected % 6d, Received % 6d.\n"
										  "             Batch will be re-sent next iteration.",
										  message_num,
										  recs_to_commit,			server_rows,
										  trailer_recs_to_commit,	server_trailer_rows,
										  SysTime,					SysTimeReturned,
										  MsgCounterExpected,		MsgCounterReturned);

						retval = failure;
					}
					else
					{
						retval = success;
					}

					break;


				case CM_COMMIT_N_ROWS_DISCREP:

					GerrLogReturn (GerrId,
								   "\nCommit_DBs Discrepancy for Trn. %d! Client has %d (+ %d) rows,\n"
								   "                                      Server has %d (+ %d) rows.\n"
								   "             Batch will be re-sent next iteration.",
								   message_num,
								   recs_to_commit,
								   trailer_recs_to_commit,
								   server_rows,
								   server_trailer_rows);

					GerrLogFnameMini ("CommitDiscrepancy_log", GerrId,
									  "\nCommit_DBs Discrepancy for Trn. %d! Client has %d (+ %d) rows,\n"
									  "                                      Server has %d (+ %d) rows.\n"
									  "             Batch will be re-sent next iteration.",
									  message_num,
									  recs_to_commit,
									  trailer_recs_to_commit,
									  server_rows,
									  server_trailer_rows);
					retval = failure;
					break;


				case CM_RESTART_CONNECTION:

					GerrLogReturn (GerrId,
								   "Commit_DBs got RESTART_CONNECTION.");
					retval = failure;
					break;


				case CM_NO_MESSAGE_PASSED:
					GerrLogReturn (GerrId,
								   "Commit_DBs got NO_MESSAGE_PASSED.");
					retval = failure;
					break;

				case CM_COMMAND_EXECUTION_FAILED:
					GerrLogReturn (GerrId,
								   "AS/400 reported failure on COMMIT.");
					retval = failure;
					break;

				default:
					GerrLogReturn (GerrId,
								   "Commit_DBs got unrecognized return of %d.",
								   rc);
					retval = failure;
					break;
				}	// End of switch.

			}
			while (0);
		}	// Need to commit on AS400.

		else
		{
			retval = success;	// No remote commit required.
		}



		if (retval == success)
		{
			CommitAllWork ();

//			if (SQLMD_commit () == MAC_OK)
			if (SQLCODE == 0)
			{
				retval = COMMIT_DONE;
				need_rollback = 0;

				recs_committed			+= recs_to_commit;
				trailer_recs_committed	+= trailer_recs_to_commit;
				recs_to_commit			= 0;
				trailer_recs_to_commit	= 0;

				break;
			}
			else
			{
				GerrLogReturn (GerrId,
							   "Failed to commit transaction on local DB, errno == %d.",
							   SQLCODE);
			}
		}	// Server reported successful commit.

		else
		{
			GerrLogReturn (GerrId,
						   "Server didn't confirm commit - rolling back transaction.");
		}


		// If we get here, something went wrong and we've got to roll back.
		recs_to_commit			= 0;
		trailer_recs_to_commit	= 0;

		if (rollback_remote_and_local_DBs () == RR_ROLLBACK_DONE)
		{
			if (rc == CM_COMMIT_N_ROWS_DISCREP)
			{
				retval = COMMIT_DISCREPANCY_ROLLEDBACK;	// New result code!
			}
			else
			{
				retval = COMMIT_FAILED_ROLLBACK_DONE;
			}

			need_rollback = 0;
		}
		else
		{
			retval = COMMIT_FAILED_ROLLBACK_FAILED;
			break;
		}
	}
	while (0); // one cycle loop


	POP_RET (retval);
}



/* << -------------------------------------------------------- >> */
/*																  */
/*   send a COMMIT/ROLLBACK message to server - no confirmation   */
/*																  */
/* << -------------------------------------------------------- >> */
/*																  */
/*    returns:  success Or failure code.						  */
/* << -------------------------------------------------------- >> */

static TSuccessStat send_a_msg_to_server (int	msg)

{
	char	buf[12];
	int		len;
	int		rc;


	pushname ("send_a_msg_to_server");

	len = sprintf( buf, "%d", msg );

	DbgAssert( len == 4 ); 	/* currently only 4 byte messages exitst */

	rc = send_buffer_to_server (glbSocket, buf, len);

	POP_RET (rc);
}



/* << -------------------------------------------------------- >> */
/*																  */
/*   send a COMMIT/ROLLBACK message to server & get confirmation  */
/*																  */
/* << -------------------------------------------------------- >> */
/*																  */
/*    returns:  success Or failure code.						  */
/* << -------------------------------------------------------- >> */

static TSuccessStat send_msg_and_get_confirmation (int	msg,
												   int	confirmation)
{
	char	buf[12];
	int		len;
	int		rc;
	TSuccessStat	retval;


	pushname ("send_msg_and_get_confirmation");

	len = sprintf( buf, "%d", msg );
	DbgAssert( len == 4 );


	do
	{
		// send message to server
		rc = send_buffer_to_server (glbSocket, buf, len);

		if (rc != success )
		{
			GerrLogReturn (GerrId,
						   "Send_and_get_confirmation tried to send %d, but send_buffer returned %d.",
						   msg, rc);
			retval = failure;
			break;
		}


		// get confirmation for message
		rc = get_msg_from_server( MAX_SELECT_WAIT_TIME_IN_SECS );


		//	printf("Sending <%d> and get <%d> expect <%d>..\n",
		//		msg,
		//		rc,
		//		confirmation
		//		); fflush(stdout);


		// Sadly, since confirmation is a variable, it can't be used as a CASE - thus
		// the awkward structure below.
		if (rc == confirmation)
		{
			retval = success;
		}

		else
		{
			switch (rc)
			{

				case CM_RESTART_CONNECTION:
					GerrLogReturn (GerrId,
								   "Send_and_get_confirmation sent %d and got RESTART_CONNECTION.",
								   msg);
					retval = failure;
					break;

				case CM_NO_MESSAGE_PASSED:
					GerrLogReturn (GerrId,
								   "Send_and_get_confirmation sent %d and got NO_MESSAGE_PASSED.",
								   msg);
					retval = failure;
					break;

				default:
					GerrLogReturn (GerrId,
								   "Send_and_get_confirmation sent %d and got unrecognized return of %d.",
								   msg, rc);
					retval = failure;
					break;
			}
		}	// Got return other than the expected one.

	}
	while( 0 );


	POP_RET (retval);
}



/* << -------------------------------------------------------- >> */
/*								  */
/*		    Is socket ready to recv()			  */
/*								  */
/* << -------------------------------------------------------- >> */
int ReadyToReceive( int sock,   int seconds_to_wait )
{
 //   struct fd_set 	rr;  /* Ready to Receive */
    fd_set 	rr;  /* Ready to Receive */
    struct timeval	wt;  /* max wait time    */
    int		retry_max = 10;
    int		tries;
    int		rc;
	int		result;

	pushname ("ReadyToReceive");

    //
    // Retry for case of interrupted system call
    //
    for( tries = 0; tries < retry_max ; tries ++ )
    {
	FD_ZERO( &rr );
	FD_SET( sock, &rr );

	wt.tv_sec  = seconds_to_wait;
	wt.tv_usec = 0;

	rc = select( sock + 1, &rr, 0, 0, &wt );

	if( rc < 0 )
	{
	    if( errno != EINTR )
	    {
			GerrLogReturn (GerrId,
						   "ReadyToReceive: error at select() - error ( %d ) %s.",
						   GerrStr);
		restart_flg = 1;
	    }
	    else
	    {
		continue;
	    }
	}

	break;
    }

    result = rc > 0 && FD_ISSET( sock , &rr );

	POP_RET (result);
}

/* << -------------------------------------------------------- >> */
/*								  */
/*		    Is socket ready to send()			  */
/*								  */
/* << -------------------------------------------------------- >> */
int ReadyToSend( int sock,   int seconds_to_wait )
{
//    struct fd_set 	rs;  /* Ready to Send */
    fd_set 	rs;  /* Ready to Send */
    struct timeval	wt;  /* max wait time */
    int		retry_max = 10;
    int		tries;
    int		rc;
	int		result;

	pushname ("ReadyToSend");

	//
	// Retry for case of interrupted system call
	//
	for( tries = 0; tries < retry_max ; tries ++ )
	{
		FD_ZERO( &rs );
		FD_SET( sock, &rs );

		wt.tv_sec  = seconds_to_wait;
		wt.tv_usec = 0;

		rc = select( sock + 1, 0, &rs, 0, &wt );

		if( rc < 0 )
		{
			if( errno != EINTR )
			{
				GerrLogReturn (GerrId,
							   "ReadyToSend: Error at select() - error ( %d ) %s.",
							   GerrStr);
				restart_flg = 1;
			}
			else
			{
				continue;
			}
		}
		else
		{
			if( rc == 0 )
			{
				GerrLogReturn (GerrId,
							   "ReadyToSend: connection closed by client - error ( %d ) %s.",
							   GerrStr);
				restart_flg = 1;
			}
		}

		break;
	}

	result = rc > 0 && FD_ISSET( sock, &rs );

	POP_RET (result);
}

/* << -------------------------------------------------------- >> */
/*																  */
/*			get_server_response - a more generalized version	  */
/*					of get_msg_from_server						  */
/*																  */
/* << -------------------------------------------------------- >> */
static int get_server_response (int wait_time,
								int MsgLen,
								int *Param1,	// Len = 5
								int *Param2,	// Len = 5
								int *Param3,	// Len = 6
								int *Param4)	// Len = 4
{
	TSuccessStat	retval;
	int				msg;
	int				rc;
	int				i;
	int				len;
	char			buf[64];
	char			EvalBuf[12];
	char			*pos_in_buf;


	pushname ("get_server_response");


	/* << -------------------------------------------------------- >> */
	/*		      get message sent from server						  */
	/* << -------------------------------------------------------- >> */
	len = MsgLen;	// 4 for "traditional" messages, 24 for new Confirmation Response. (DonR 14Mar2013: 14 -> 24, added two new fields.)
	pos_in_buf = buf;

	if( restart_flg )
	{
		POP_RET (CM_NO_MESSAGE_PASSED);
	}

	while (len > 0)
	{
		// first verify pipe is not broken:
		rc = ReadyToReceive( glbSocket, wait_time );

		if( ! rc )
		{
			if( wait_time != NO_WAIT_MODE )
			{
				GerrLogReturn (GerrId,
							   "recv() Connection reset by peer\n"
							   GerrErr,
							   GerrStr);
				restart_flg = 1;
			}

			POP_RET (CM_NO_MESSAGE_PASSED);
		}


		// receive data from server:
		rc = recv( glbSocket, pos_in_buf, len, 0/* no flags */ );

		switch( rc )
		{
			case -1:
				GerrLogReturn (GerrId,
							   "Error at recv () "
							   GerrErr,
							   GerrStr);
				restart_flg = 1;
				POP_RET (CM_NO_MESSAGE_PASSED);

			case 0:
				if( wait_time != NO_WAIT_MODE )
				{
					GerrLogReturn (GerrId,
								   "recv() Connection reset by peer\n"
								   GerrErr,
								   GerrStr);
					restart_flg = 1;
				}

				POP_RET (CM_NO_MESSAGE_PASSED);

			default:
				pos_in_buf += rc;
				len -= rc;
				break;

		}	// switch( rc ..

	}		// while( len ..

	DbgAssert( len == 0 );
	*pos_in_buf = '\0'; 	/* position string sentinel */

	// Since some server responses are longer than 4 characters, we have to chop up
	// the incoming buffer.
	strncpy (EvalBuf, buf, 4);
	EvalBuf[4]= (char)0;
	msg = atoi (EvalBuf);

	retval = a_legal_message_ID( msg );

	if( retval !=  true )
	{
		GerrLogReturn (GerrId, "NOT a legal message-id BUFFER <%.20s...>", buf);
	}

	// If calling function has requested additional parameters (and set length
	// accordingly), fill them in from server message.
	if ((Param1 != NULL) && (MsgLen >= 9))
	{
		strncpy (EvalBuf, &buf[4],5);
		EvalBuf[5]= (char)0;

		// Replace leading zeroes with spaces, so atoi() will work OK.
		for (i = 0; i < 4; i++)
		{
			if (EvalBuf[i] == '0')
				EvalBuf[i] = ' ';
			else
				break;
		}

		*Param1 = atoi (EvalBuf);
	}

	if ((Param2 != NULL) && (MsgLen >= 14))
	{
		strncpy (EvalBuf, &buf[9],5);
		EvalBuf[5]= (char)0;

		// Replace leading zeroes with spaces, so atoi() will work OK.
		for (i = 0; i < 4; i++)
		{
			if (EvalBuf[i] == '0')
				EvalBuf[i] = ' ';
			else
				break;
		}

		*Param2 = atoi (EvalBuf);
	}

	// DonR 14Mar2013: Two new response fields - the first one is an HHMMSS timestamp.
	if ((Param3 != NULL) && (MsgLen >= 24))
	{
		strncpy (EvalBuf, &buf[14],6);
		EvalBuf[6]= (char)0;

		// Replace leading zeroes with spaces, so atoi() will work OK.
		for (i = 0; i < 5; i++)
		{
			if (EvalBuf[i] == '0')
				EvalBuf[i] = ' ';
			else
				break;
		}

		*Param3 = atoi (EvalBuf);
	}

	// DonR 14Mar2013: Two new response fields - the second one is a "rolling" counter, from 0002 to 9998.
	if ((Param4 != NULL) && (MsgLen >= 24))
	{
		strncpy (EvalBuf, &buf[20],4);
		EvalBuf[4]= (char)0;

		// Replace leading zeroes with spaces, so atoi() will work OK.
		for (i = 0; i < 3; i++)
		{
			if (EvalBuf[i] == '0')
				EvalBuf[i] = ' ';
			else
				break;
		}

		*Param4 = atoi (EvalBuf);
	}


	if (retval == true)
	{
		POP_RET (msg);
	}
	else
	{
		POP_RET (CM_NO_MESSAGE_PASSED);
	}
}


/* << -------------------------------------------------------- >> */
/*								  */
/*		  initialize_connection_to_server		  */
/*								  */
/* << -------------------------------------------------------- >> */
/*								  */
/*    returns:  success Or failure code.			  */
/* << -------------------------------------------------------- >> */
static TSuccessStat initialize_connection_to_server( void )
{
    Tsockaddr_in	remote_addr;
    TSuccessStat	retval;
    int			i,
			nonzero = -1,
			rc;
    u_long		server_ip_addr;
    struct hostent	*hostent_ptr;
    char		dbg_buffer[512];

    struct protoent *tcp_prot;

	pushname ("initialize_connection_to_server");

    /*
     * terminate connection if active:
     */
    if( glbSocket > -1 )
    {
      close( glbSocket );
    }


    retval = success;
    do
    {

      /*
       * get valid socket:
       */
	FD_ZERO( &glbFDBitMask );
	glbSocket = socket( AF_INET, SOCK_STREAM, 0 );
	if( glbSocket < 0 )
	{
	    GerrLogReturn(
			  GerrId,
			  "Failed at function socket() errno == %d.",
			  errno
			  );
	    retval = failure;
	    break;
	}

	/*
	 * get protocol by name -- tcp
	 */
	tcp_prot = getprotobyname("tcp");

	if( tcp_prot == NULL )
	{
	  GerrLogReturn(
			GerrId,
			"Unable to get protocol number for 'tcp'\n"
			GerrErr,
			GerrStr
			);
	}

	/*
	 * set socket options -- nodelay
	 */
	if(
	   setsockopt( glbSocket, tcp_prot->p_proto, TCP_NODELAY,
		     &nonzero, sizeof( nonzero ) ) == -1
	 )
	{
	  GerrLogReturn(
			GerrId,
			"Unable to set accepted-socket options."
			GerrErr,
			GerrStr
			);
	}

	/*
	 * get server-process computer IP-address:
	 */
	hostent_ptr = gethostbyname( AS400_IP_ADDR );
	if( hostent_ptr == NULL )
	{
	  GerrLogReturn(
			GerrId,
			"Failed at function gethostbyname() errno == %d.",
			errno
			);
	  retval = failure;
	  break;
	}
	server_ip_addr = *( u_long * )hostent_ptr->h_addr;

	/*
	 * write remote address:
	 */
	memset( &remote_addr, 0, sizeof( remote_addr ) );
	remote_addr.sin_family = AF_INET;	/* Internet address famly */
	remote_addr.sin_port = htons( PORT_IN_AS400 );
	remote_addr.sin_addr.s_addr = server_ip_addr;

	/*
	 * connect to remote address:
	 */
	alarm( 1 );	// set alarm in 1 seconds

	rc = connect(
		     glbSocket,
		     ( const struct sockaddr * )&remote_addr,
		     sizeof( Tsockaddr_in )
		     );

	alarm( 0 );	// cancel alarm

	if( rc <= -1 )
	{
	    retval = failure;
	}

	/*
	 * add socket to bitmask:
	 */
	FD_SET( glbSocket, &glbFDBitMask );

    }
    while( 0 );

    POP_RET (retval);

}



/* << -------------------------------------------------------- >> */
/*																  */
/*      pass an entire table to server using a pointer to a		  */
/*      function, unique for each table, that sends a batch of	  */
/*      records each time it's called							  */
/*																  */
/* << -------------------------------------------------------- >> */

static int call_message_handler( int  msgID )

{
	int		rc;
	TFuncPtr	fp = NULL;	/* function ptr for a specific message */

	pushname ("call_message_handler");

	//
	// Call the corresponding function
	//
	//GerrLogReturn (GerrId, "Call_message_handler: message id = %d.", msgID);

	// DonR 03Apr2011: NULL-ed out Trn. 2503 (special_prescs upload), since it was never in "real" use.
	// The relevant table, RK9023, is being deleted (or maybe re-used for something else).
	switch (msgID)
	{

		case 2501:
			fp = pass_record_batch_2501;
			break;

		case 2502:		/* -> passing both msg 2502 & 2506 */
			fp = pass_record_batch_2502_2506;
			break;

		case 2503:
//			fp = pass_record_batch_2503;
			fp = NULL;		// DonR 03Apr2011: Do nothing.
			break;

		case 2504:
			fp = pass_record_batch_2504;
			break;

		case 2505:
			fp = pass_record_batch_2505;
			break;

		case 2507:
			fp = pass_record_batch_2507;
			break;

		case 2508:
			fp = pass_record_batch_2508;
			break;

		case 2509:
			fp = pass_record_batch_2509;
			break;

		case 2512:
			fp = pass_record_batch_2512;
			break;

		case 2513:		/* -> passing both msg 2513 & 2514 */
			fp = pass_record_batch_2513_2514;
			break;

		case 2515: // DonR 14Mar2004: pharmacy_ishur
			fp = pass_record_batch_2515;
			break;

		case 2516: // DonR 04Nov2004: prescr_written
			fp = pass_record_batch_2516;
			break;

		case 2517: // DonR 09Jan2005: Update Specialist Drugs table on Unix.
			fp = pass_record_batch_2517;
			break;

		default:
			GerrLogReturn (GerrId, "Unknown message id %d in call_message_handler!", msgID);
			break;
	}


	// DonR 03Apr2011: If fp has not been set to a valid function, there's no point in trying execute
	// the function it points to. If it's still NULL, do nothing.
	if (fp != NULL)
	{
		rc = pass_table_to_server (fp);
	}
	else
	{
		// Nothing to do; no remote commit required. (This still performs a local commit, but
		// in reality there should be nothing to commit.)
		rc = SB_END_OF_FETCH_NOCOMMIT;
	}

	//GerrLogReturn (GerrId, "Call_message_handler: pass_table returned %d for message id %d.", rc, msgID);

	//    printf("return from call_msg_hand <%d>..\n", rc);fflush(stdout);

	POP_RET (rc);

}



/* << -------------------------------------------------------- >> */
/*																  */
/*       pass all records of a certain table to remote DB		  */
/*																  */
/*	 return codes:												  */
/*			MH_FATHER_GOING_DOWN;								  */
/*			MH_SYSTEM_IS_BUSY;									  */
/*			MH_RESTART_CONNECTION;								  */
/*			MH_SUCCESS;											  */
/*			MH_FATAL_ERROR;										  */
/*			MH_MAX_ERROR_WAS_EXCEEDED;							  */
/*																  */
/*	 evaluated in function send_table_sequence_to_remote_DB()	  */
/*																  */
/*																  */
/* << -------------------------------------------------------- >> */

static int pass_table_to_server (TFuncPtr  pass_record_batch_XXXX)

{
	// Local variables
	int		err_counter;
	bool	local_sys_down;
	bool	local_sys_busy;
	int		retval;
	int		err_flg;
	int		rc;
	int		cs;
	int		system_down_flg;



	pushname ("pass_table_to_server");

	err_counter = 0;

	recs_to_commit			= recs_committed			= 0;
	trailer_recs_to_commit	= trailer_recs_committed	= 0;


	get_system_status (&local_sys_down, &local_sys_busy);

	if (local_sys_down == true)
	{
		POP_RET (MH_FATHER_GOING_DOWN);
	}

	if (local_sys_busy == true)
	{
		POP_RET (MH_SYSTEM_IS_BUSY);
	}



	// Retries loop -- for errors in table transmit
	while (err_counter < MAX_ERRORS_FOR_SINGLE_TABLE)
	{
		// Update params from shm & get system status
		UpdateShmParams ();

		// DonR 09May2005: If this is the first iteration of the loop, there's
		// no need to call get_system_status() - as we did it right before the
		// loop started.
		if (err_counter > 0)
		{
			get_system_status (&local_sys_down, &local_sys_busy);

			if (local_sys_down == true)
			{
				POP_RET (MH_FATHER_GOING_DOWN);
			}
		}

//		// Assure in transaction state
//		// DonR 21Jan2020: In ODBC, transactions are started automatically the
//		// first time an SQL operation changes something.
//		if (!glbInTransaction)
//		{
//			SQLMD_begin ();
//
//			if (SQLERR_error_test ())
//			{
//				GerrLogReturn (GerrId, "Failed to open DB transaction, errno == %d.", SQLCODE);
//				continue;
//			}
//
//			glbInTransaction = 1;
//		}
//
//
		// Pass a batch of table records to server
		rc = pass_record_batch_XXXX ();

		switch (rc)
		{
			// 1) data access conflict
			// 2) local database error
			case SB_DATA_ACCESS_CONFLICT:
			case SB_LOCAL_DB_ERROR:

				if (rc == SB_DATA_ACCESS_CONFLICT)
				{
					GerrLogMini (GerrId,
								 "Pass_record_batch attempt #%d failed - access conflict.",
								 (err_counter + 1));
				}
				else
				{
					GerrLogMini (GerrId,
								 "Pass_record_batch attempt #%d failed - local DB error.",
								 (err_counter + 1));
				}


				if (need_rollback)
				{
					//GerrLogReturn (GerrId, "Rolling back remote and local DB's.");
					rollback_remote_and_local_DBs();
					GerrLogMini (GerrId, "DB rollback complete.");
				}

				err_counter++;

				if( rc == SB_DATA_ACCESS_CONFLICT )
				{
					//GerrLogReturn (GerrId, "About to sleep for %d (seconds?).", ACCESS_CONFLICT_SLEEP_TIME);
					sleep( ACCESS_CONFLICT_SLEEP_TIME );
					//GerrLogReturn (GerrId, "Woke up after access conflict.");
				}

				// Let user change tables manually
				if( manual_change_of_tables )
				{
					manual_change_of_tables = 0;
					GerrLogReturn (GerrId, "Manual change of tables - returning.");
					POP_RET (MH_SUCCESS);
				}

				break;


			// 3) tcp/ip error - restart connection
			case CM_RESTART_CONNECTION:

				//GerrLogReturn (GerrId, "Pass_record_batch failed, returning CM_RESTART_CONNECTION");

				if (need_rollback)
				{
					rollback_remote_and_local_DBs();
				}

				POP_RET (MH_RESTART_CONNECTION);


			// 4) table has no rows to send
			//    NOTE: Should we really need a commit here?  -DonR 09 OCT 2002
			//    DonR 08May2013: Yes, we do need a commit. In theory, it's possible that we marked some stuff as "sent"
			//    when in fact it didn't need to be sent to AS/400; so we may want to commit the local transaction even
			//    though we're not committing anything on AS/400.
			case SB_END_OF_FETCH_NOCOMMIT:

				// DonR 22Apr2013: Use explicit SQL so we have control over the error-handling.
				CommitAllWork ();

				// DonR 22Apr2013: If transaction wasn't opened because there was nothing to send, we don't have any
				// real error - treat this case as a success.
				// DonR 08Nov2020: Need to check what the ODBC/MS-SQL equivalent of SQLCODE -255 is, if any.
				if ((SQLCODE == 0) || ((SQLCODE == -255) && ((recs_to_commit + trailer_recs_to_commit) == 0)))
				{
					need_rollback = 0;
					if ((recs_to_commit > 0) || (trailer_recs_to_commit > 0))
					{
						GerrLogReturn (GerrId,
									   "Discrepancy! - Updated %d recs in local DB, but didn't commit on AS400!",
									   (recs_to_commit + trailer_recs_to_commit));
						recs_to_commit = trailer_recs_to_commit = 0;
					}
				}
				else
				{
					// DonR 22Apr2013: Because of the changes above, we should never really get here - since it
					// shouldn't be possible to get any SQL error other than -255.
					GerrLogReturn (GerrId,
								   "End-of-fetch (redundant?) commit of %d recs failed, error = %d.",
								   (recs_to_commit + trailer_recs_to_commit),
								   SQLCODE);
				}
				POP_RET (MH_SUCCESS);


			// 5) batch of rows succesfully sent
			// 6) no more rows to send
			case SB_BATCH_SUCCESSFULLY_PASSED:
			case SB_END_OF_FETCH:

				cs = commit_remote_and_local_DBs();

				switch( cs )
				{
				case COMMIT_DISCREPANCY_ROLLEDBACK:

					// Server reported a discrepancy in the row count, and we rolled
					// back successfully. In this case, we log the problem and continue
					// with the current table - sort of a qualified success.
					err_counter++;
					GerrLogReturn (GerrId,
								   "Discrepancy prevented! Rollback succeeded!! Onwards!!!");

					// If we've completed the whole transfer, we're done!
					if (rc == SB_END_OF_FETCH)
					{
						POP_RET (MH_SUCCESS);
					}
					else
					{
						break;	// Will loop again, getting and sending next batch.
					}


				case COMMIT_FAILED_ROLLBACK_DONE:
				case COMMIT_FAILED_ROLLBACK_FAILED:

					err_counter++;

					// DonR_29OCT2001 begin: If the commit failed, make sure we don't just
					// ignore the problem!  Previously, if the commit failed but we were
					// working on the last batch of the fetch, we'd return as if the commit
					// had succeeded.

					// If we failed to roll-back the transaction on the local DB, pause and
					// try again.
					if (cs == COMMIT_FAILED_ROLLBACK_FAILED)
					{
						FILE *FTEST;
						char f_name []="/pharm/bin/f_c_f_r_f.out";

						GerrLogReturn (GerrId, "Commit failed, Rollback failed - retrying.");

						FTEST= fopen ( f_name,"a");
						fprintf(FTEST,"\n cs[%d] date[%d]:[%d]",cs,GetDate(),GetTime());
						fclose(FTEST);

						sleep (ACCESS_CONFLICT_SLEEP_TIME);

						RollbackAllWork ();
//						if (SQLMD_rollback () == MAC_OK)
						if (SQLCODE == 0)
						{
							need_rollback = 0;

							GerrLogReturn (GerrId,
										   "Commit failed, Rollback failed, Retry Rollback OK.");
						}
						else
						{
							GerrLogReturn (GerrId,
										   "Commit failed, Rollback failed, Retry Rollback failed!");
						}
					}	// Commit failed, rollback failed.

					else
					{
						GerrLogReturn (GerrId, "Commit failed, Rollback succeeded.");
					}


					// Don't just fall through - we need to force a retry!
					sleep_check (SLEEP_BETWEEN_TABLES, &system_down_flg);
					break;

					// DonR_29OCT2001 end.


				case COMMIT_DONE:

					// If we've completed the whole transfer, we're done!
					if( rc == SB_END_OF_FETCH )
					{
						POP_RET (MH_SUCCESS);
					}
					else
					{
						break;	// Will loop again, getting and sending next batch.
					}


				default:

					GerrLogReturn (GerrId, "Unknown(pass_table_to_server commit work) return code : %d",cs);
					break;

				}	// End of inner switch (on results of commit).

				break;


			// 7) error
			default:

				GerrLogReturn (GerrId, "Unknown(pass_table_to_server) return code : %d", rc);

				RollbackAllWork ();

//				if (SQLMD_rollback () == MAC_OK)
				if (SQLCODE == 0)
				{
					need_rollback = 0;
				}

				break;

		}	// switch( rc ...


		if( restart_flg )
		{
			//GerrLogReturn (GerrId,
			//			   "Pass_record_batch failed, rc = %d. Restart_flg = %d - changing rc!",
			//				rc, restart_flg);
			rc = CM_RESTART_CONNECTION;
		}

		// Let user change tables manually
		if( manual_change_of_tables )
		{
			rc = SB_LOCAL_DB_ERROR;
		}


    }		// while( err_counter ....


	//GerrLogReturn (GerrId, "Pass_table_to_server had %d errors - giving up!", err_counter);


    POP_RET (MH_MAX_ERROR_WAS_EXCEEDED);

}



/* << -------------------------------------------------------- >> */
/*								  */
/*		 get num of records in a table's batch		  */
/*								  */
/* << -------------------------------------------------------- >> */
static int get_dimension_of_batch(
				  int msgID
				  )
{
		return message_len;
//    return CL_DIMENSION_OF_BATCH;  /* same value for all messages */
}


#if 0
/* << -------------------------------------------------------- >> */
/*								  */
/*			connect_to_local_database		  */
/*								  */
/* << -------------------------------------------------------- >> */
static void connect_to_local_database(
				      void
				      )
{
    SQLMD_connect_id( CLIENT_CONNECT_STRING );
}
#endif


/* << -------------------------------------------------------- >> */
/*								  */
/*	       terminate_communication_and_DB_connections	  */
/*								  */
/* << -------------------------------------------------------- >> */
static void terminate_communication_and_DB_connections(
						       void
						       )
{
	SQLMD_disconnect();
//    SQLMD_disconnect_id( CLIENT_CONNECT_STRING );

    if( glbSocket > -1 )
    {
	close( glbSocket );
    }
}


/* << -------------------------------------------------------- >> */
/*								  */
/*	      send a buffer of size len to server		  */
/*								  */
/* << -------------------------------------------------------- >> */
/*								  */
/*    returns:  success Or failure code.			  */
/* << -------------------------------------------------------- >> */

static TSuccessStat send_buffer_to_server (TSocket		sock,
										   const char	*buf,
										   int			len)
{
	int				sent_bytes;
	TSuccessStat 	retval = success;
	static char		mybuf [10240];
	char			*bufptr = &mybuf[0];
	int				mylen = len;
	static char		tracebuf[100];


	sprintf (tracebuf, "send_buffer_to_server M = %.4s, L = %d", buf, len);
	//pushname ("send_buffer_to_server");
	pushname (&tracebuf[0]);

	if (restart_flg)
	{
		POP_RET (failure);
	}


	memcpy (mybuf, buf, len);
	ascii_to_ebcdic_dic ((char *)mybuf, len);

	while (mylen > 0)
	{
		if (!ReadyToSend (sock, MAX_SELECT_WAIT_TIME_IN_SECS))
		{
			retval = failure;
			break;
		}

		sent_bytes = send (sock, bufptr, mylen, NO_FLAGS);

		switch (sent_bytes)
		{
			case -1:				/* send error */

				GerrLogReturn (GerrId, "send() error!" GerrErr, GerrStr);
				restart_flg = 1;
				retval = failure;
				break;

			case 0:				/* connection is closed	*/

				GerrLogReturn (GerrId, "send(): Connection was reset by peer." GerrErr,	GerrStr);
				restart_flg = 1;
				retval = failure;
				break;

			default:

				bufptr	+= sent_bytes;
				mylen	-= sent_bytes;
//
//				// DonR 11Jan2024: If we sent only part of the buffer, add a short delay
//				// before sending the rest.
//				if (mylen > 0)
//					usleep (1000);	// 1 millisecond
//
				break;
		}


		if (retval == failure)
		{
			break;
		}

	}	// while (mylen > 0)


	POP_RET (retval);
}



// DonR 11Jan2024: Use this version of send_buffer_to_server if you need to diagnose
// exactly what we're sending to AS/400 - including a character-by-character dump to
// check on ASCII-to-EBCDIC translation.
static TSuccessStat send_buffer_to_server_diagnostic (	TSocket		sock,
														const char	*buf,
														int			len,
														int			diagnose_from,
														int			Mizaheh)
{
	int				sent_bytes;
	TSuccessStat 	retval = success;
	static char		mybuf [10240];
	char			*bufptr = &mybuf[0];
	int				mylen = len;
	static char		tracebuf[500];
	int				i;
	unsigned char	diag_char;
	int				tracebuf_len;


	sprintf (tracebuf, "send_buffer_to_server M = %.4s, L = %d", buf, len);
	//pushname ("send_buffer_to_server");
	pushname (&tracebuf[0]);

	if (restart_flg)
	{
		POP_RET (failure);
	}


	memcpy (mybuf, buf, len);
	ascii_to_ebcdic_dic ((char *)mybuf, len);


	// Dump a diagnostic after translating to EBCDIC.
	tracebuf_len = sprintf (tracebuf, "%d: ", Mizaheh);
	for (i = 0; i < 30; i++)
		tracebuf_len += sprintf (tracebuf + tracebuf_len, "%d ", (int)(unsigned char)buf[i + diagnose_from]);
	GerrLogMini (GerrId, "EBCDIC: %s", tracebuf);

		
	while (mylen > 0)
	{
		if (!ReadyToSend (sock, MAX_SELECT_WAIT_TIME_IN_SECS))
		{
			retval = failure;
			break;
		}

		sent_bytes = send (sock, bufptr, mylen, NO_FLAGS);

		switch (sent_bytes)
		{
			case -1:				/* send error */

				GerrLogReturn (GerrId, "send() error!" GerrErr, GerrStr);
				restart_flg = 1;
				retval = failure;
				break;

			case 0:				/* connection is closed	*/

				GerrLogReturn (GerrId, "send(): Connection was reset by peer." GerrErr,	GerrStr);
				restart_flg = 1;
				retval = failure;
				break;

			default:

				bufptr	+= sent_bytes;
				mylen	-= sent_bytes;
//
//				// DonR 11Jan2024: If we sent only part of the buffer, add a short delay
//				// before sending the rest.
//				if (mylen > 0)
//					usleep (1000);	// 1 millisecond
//
				break;
		}


		if (retval == failure)
		{
			break;
		}

	}	// while (mylen > 0)


	POP_RET (retval);
}


static void get_system_status(
			      bool	*pSysDown,
			      bool	*pSysBusy
			      )
{
	int	temp;

	pushname ("get_system_status");

	if ( *pSysDown !=  true )
	{
		UpdateShmParams( );

		if( ToGoDown( PHARM_SYS ) )
		{
			*pSysDown = true ;
		}
	}

	if ( *pSysDown !=  true )
	{
		// DonR 24Sep2020: Just to speed things up, change from "sar 5" to "sar 1" - the
		// result should be the same, since a seriously overloaded system is going to be
		// consistently overloaded.
//		temp = GetSystemLoad( 5 );
		temp = GetSystemLoad( 1 );

		//
		// The check ( < 100 ) ensures that a number is
		//  tested here and not some garbage...
		// After all - it is just an output of a command line command....
		//
		*pSysBusy = (temp >= SYSTEM_MAX_LOAD && temp < 100 ) ? true : false;

		if( *pSysBusy == true )
		{
			//GerrLogReturn (GerrId, "Load is : %d - setting system-busy flag!", temp);
		}
	}

	popname ();
}


/* << -------------------------------------------------------- >> */
/*								  */
/*	      is received message ID a legal one ?		  */
/*								  */
/* << -------------------------------------------------------- >> */
/*								  */
/*    returns:  true Or false code.				  */
/* << -------------------------------------------------------- >> */
static bool a_legal_message_ID(
			       int msgID
			       )
{
    return( msgID > 1000 && msgID < 10000 ) ? true : false;
}


/* << -------------------------------------------------------- >> */
/*                                                           	  */
/*                                                           	  */
/*		 M E S S A G E    H A N D L E R S		  */
/*                                                           	  */
/*                                                           	  */
/* << -------------------------------------------------------- >> */


/* << -------------------------------------------------------- >> */
/*								  */
/*   Comment:							  */
/*   ---------							  */
/*								  */
/* Functions pass_record_batch_XXXX() can propagate the codes:    */
/*								  */
/*			SB_LOCAL_DB_ERROR			  */
/*			SB_END_OF_FETCH				  */
/*                      SB_END_OF_FETCH_NOCOMMIT		  */
/*			SB_DATA_ACCESS_CONFLICT			  */
/*			SB_BATCH_SUCCESSFULLY_PASSED		  */
/*			CM_RESTART_CONNECTION			  */
/*			CM_SYSTEM_IS_NOT_BUSY			  */
/*								  */
/*        to be evaluated in function pass_table_to_server()	  */
/*								  */
/* << -------------------------------------------------------- >> */


/* << -------------------------------------------------------- >> */
/*                                                           	  */
/*    Attention:						  */
/*    -----------						  */
/*      A.  If you wish to change the generic type( short,	  */
/*	    long, string )of a DB field( Tpharmacy_code etc. )	  */
/*	    do the following:					  */
/*								  */
/*	         1. Change generic type in file DBFields.h	  */
/*		    section = "DF field types".			  */
/*								  */
/*		 2. If needed - change field length in file	  */
/*		    DBFields.h  section = "Length of DB fields"	  */
/*								  */
/*		 3. Change parsing format in func sprintf() to:	  */
/*		      short ->"%d",				  */
/*		      long -> "%ld",				  */
/*		      string -> "%-*.*s".			  */
/*								  */
/*								  */
/*      B.  If you also wish to change message COMPOSITION	  */
/*	    ( == the fields passed in a given message )you	  */
/*	    must change the length of the record attached to	  */
/*	    the message - RECORD_LEN_XXXX in file DBFields.h	  */
/*	    section = "Length of record".			  */
/*                                                           	  */
/* << -------------------------------------------------------- >> */


/*=======================================================================
||																		||
||			Message 2501 --	pharmacy_log								||
||																		||
 =======================================================================*/
static int pass_record_batch_2501( void )
{

/*=======================================================================
||									||
||			Local variables					||
||									||
 =======================================================================*/

  int		BATCH_DIM;
  TSuccessStat	retval = SB_END_OF_FETCH_NOCOMMIT;
  int		server_msg;
  int		n_uncommited_recs;
  int		rc;
  bool		exit_flg;
  int		i;
  int		n_data;		/* length of data to be sent to server */
  static int	cursor_flg = 1;

/*=======================================================================
||									||
||			Sql variables					||
||									||
 =======================================================================*/

  Tpharmacy_code	v_pharmacy_code;
  Tinstitued_code	v_institued_code;
  Tterminal_id		v_terminal_id;
  Terror_code		v_error_code;
  Topen_close_flag	v_open_close_flag;
  Tprice_list_code	v_price_list_code;
  Tprice_list_date	v_price_list_date;
  Tprice_list_macabi_	v_price_list_macabi_;
  Tprice_list_cost_da	v_price_list_cost_da;
  Tdrug_list_date	v_drug_list_date;
  Thardware_version	v_hardware_version;
  Towner		v_owner;
  Tcomm_type		v_comm_type;
  Tphone_1		v_phone_1;
  Tphone_2		v_phone_2;
  Tphone_3		v_phone_3;
  Tmessage_flag		v_message_flag;
  Tprice_list_flag	v_price_list_flag;
  Tprice_l_mcbi_flag	v_price_l_mcbi_flag;
  Tdrug_list_flag	v_drug_list_flag;
  Tdiscounts_date	v_discounts_date;
  Tsuppliers_date	v_suppliers_date;
  Tmacabi_centers_dat	v_macabi_centers_dat;
  Terror_list_date	v_error_list_date;
  Tdisk_space		v_disk_space;
  Tnet_flag		v_net_flag;
  Tcomm_fail_filesize	v_comm_fail_filesize;
  Tbackup_date		v_backup_date;
  Tavailable_mem	v_available_mem;
  Tpc_date		v_pc_date;
  Tpc_time		v_pc_time;
  Tdb_date		v_db_date;
  Tdb_time		v_db_time;
  Tclosing_type		v_closing_type;
  Tuser_ident		v_user_ident;
  Terror_code_list_fl	v_error_code_list_fl;
  Tdiscounts_flag	v_discounts_flag;
  Tsuppliers_flag	v_suppliers_flag;
  Tmacabi_centers_fla	v_macabi_centers_fla;
  Tsoftware_ver_need	v_software_ver_need;
  Tsoftware_ver_pc	v_software_ver_pc;
  int				v_r_count;	// DonR 18SEP2002
  static int		MaxRecCount;

/*=======================================================================
||									||
||			Initialize variables				||
||									||
 =======================================================================*/

	pushname ("pass_record_batch_2501");

	BATCH_DIM = get_dimension_of_batch( 2501 );
	n_uncommited_recs = 0;			/* num of records sent to   */
	/* remote DB without commit */

	if( cursor_flg )
	do
	{
		DeclareCursor (	MAIN_DB, AS400CL_2501_pharmacy_log_cur,
						&NOT_REPORTED_TO_AS400,		&MinPharmacyToAs400,
						&MaxRecCount,				END_OF_ARG_LIST			);

		//
		// Go few rows back in order to
		//  not lock key value in index of table
		//
		ExecSQL (	MAIN_DB, AS400CL_2501_READ_max_r_count,
					&MaxRecCount, END_OF_ARG_LIST			);

		if (SQLERR_error_test ())
		{
			retval = SB_DATA_ACCESS_CONFLICT;
			break;
		}

		MaxRecCount -= 50;

		//
		// Now open table...safely
		//
		OpenCursor (	MAIN_DB, AS400CL_2501_pharmacy_log_cur	);

		if( SQLERR_error_test() )
		{
			retval = SB_DATA_ACCESS_CONFLICT;
			break;
		}

	}	// end of if( cursor_flg ..
	while( 0 );


	/*=======================================================================
	||									||
	||			Loop over pharmacy_log lines			||
	||									||
	=======================================================================*/

	if( retval != SB_DATA_ACCESS_CONFLICT )
	do
	{
		cursor_flg = 0;

		while( 1 )
		{
			FetchCursorInto (	MAIN_DB, AS400CL_2501_pharmacy_log_cur,
								&v_pharmacy_code,		&v_institued_code,		&v_terminal_id,
								&v_error_code,			&v_open_close_flag,		&v_price_list_code,
								&v_price_list_date,		&v_price_list_macabi_,	&v_price_list_cost_da,
								&v_drug_list_date,		&v_hardware_version,	&v_owner,
								&v_comm_type,			&v_phone_1,				&v_phone_2,

								&v_phone_3,				&v_message_flag,		&v_price_list_flag,
								&v_price_l_mcbi_flag,	&v_drug_list_flag,		&v_discounts_date,
								&v_suppliers_date,		&v_macabi_centers_dat,	&v_error_list_date,
								&v_disk_space,			&v_net_flag,			&v_comm_fail_filesize,
								&v_backup_date,			&v_available_mem,		&v_pc_date,

								&v_pc_time,				&v_db_date,				&v_db_time,
								&v_closing_type,		&v_user_ident,			&v_error_code_list_fl,
								&v_discounts_flag,		&v_suppliers_flag,		&v_macabi_centers_fla,
								&v_software_ver_need,	&v_software_ver_pc,		&v_r_count,
								END_OF_ARG_LIST																);

			/*
			28.06.98
			Close cursor if we are close to recent pharmacy_log entries
			help to prevent locks
			*/
			if ((diff_from_now (v_db_date, v_db_time) < TRANSFER_INTERVAL)	||
				(SQLERR_code_cmp (SQLERR_end_of_fetch) == MAC_TRUE))
			{
				cursor_flg = 1;
				retval = n_uncommited_recs ? SB_END_OF_FETCH : SB_END_OF_FETCH_NOCOMMIT;
				break;
			}
			else
			{
				if( SQLERR_code_cmp( SQLERR_access_conflict ) == MAC_TRUE )
				{
					retval = SB_DATA_ACCESS_CONFLICT;
					break;
				}
				else
				{
					if( SQLERR_error_test() != MAC_OK )	/* other SQL error */
					{
						retval = SB_LOCAL_DB_ERROR;
						break;
					}
				}
			}


			/* << -------------------------------------------------------- >> */
			/*		   move record into glbMsgBuffer		  */
			/* << -------------------------------------------------------- >> */
			n_data = 0;

			n_data += sprintf(		/* -> message header */
				glbMsgBuffer + n_data,
				"%0*d",
				MESSAGE_ID_LEN,
				2501
				);

			n_data += sprintf(
				glbMsgBuffer + n_data,
				"%0*d",
				Tpharmacy_code_len,
				v_pharmacy_code
				);

			n_data += sprintf(
				glbMsgBuffer + n_data,
				"%0*d",
				Tinstitued_code_len,
				v_institued_code
				);

			n_data += sprintf(
				glbMsgBuffer + n_data,
				"%0*d",
				Tterminal_id_len,
				v_terminal_id
				);

			n_data += sprintf(
				glbMsgBuffer + n_data,
				"%0*d",
				Terror_code_len,
				v_error_code
				);

			n_data += sprintf(
				glbMsgBuffer + n_data,
				"%0*d",
				Topen_close_flag_len,
				v_open_close_flag
				);

			n_data += sprintf(
				glbMsgBuffer + n_data,
				"%0*d",
				Tprice_list_code_len,
				v_price_list_code
				);

			n_data += sprintf(
				glbMsgBuffer + n_data,
				"%0*d",
				Tprice_list_date_len,
				v_price_list_date
				);

			n_data += sprintf(
				glbMsgBuffer + n_data,
				"%0*d",
				Tprice_list_macabi__len,
				v_price_list_macabi_
				);

			n_data += sprintf(
				glbMsgBuffer + n_data,
				"%0*d",
				Tprice_list_cost_da_len,
				v_price_list_cost_da
				);

			n_data += sprintf(
				glbMsgBuffer + n_data,
				"%0*d",
				Tdrug_list_date_len,
				v_drug_list_date
				);

			n_data += sprintf(
				glbMsgBuffer + n_data,
				"%0*d",
				Thardware_version_len,
				v_hardware_version
				);

			n_data += sprintf(
				glbMsgBuffer + n_data,
				"%0*d",
				Towner_len,
				v_owner
				);

			n_data += sprintf(
				glbMsgBuffer + n_data,
				"%0*d",
				Tcomm_type_len,
				v_comm_type
				);

			n_data += sprintf(
				glbMsgBuffer + n_data,
				"%-*.*s",
				Tphone_1_len,
				Tphone_1_len,
				v_phone_1
				);

			n_data += sprintf(
				glbMsgBuffer + n_data,
				"%-*.*s",
				Tphone_2_len,
				Tphone_2_len,
				v_phone_2
				);

			n_data += sprintf(
				glbMsgBuffer + n_data,
				"%-*.*s",
				Tphone_3_len,
				Tphone_3_len,
				v_phone_3
				);


			n_data += sprintf(
				glbMsgBuffer + n_data,
				"%0*d",
				Tmessage_flag_len,
				v_message_flag
				);

			n_data += sprintf(
				glbMsgBuffer + n_data,
				"%0*d",
				Tprice_list_flag_len,
				v_price_list_flag
				);

			n_data += sprintf(
				glbMsgBuffer + n_data,
				"%0*d",
				Tprice_l_mcbi_flag_len,
				v_price_l_mcbi_flag
				);

			n_data += sprintf(
				glbMsgBuffer + n_data,
				"%0*d",
				Tdrug_list_flag_len,
				v_drug_list_flag
				);

			n_data += sprintf(
				glbMsgBuffer + n_data,
				"%0*d",
				Tdiscounts_date_len,
				v_discounts_date
				);

			n_data += sprintf(
				glbMsgBuffer + n_data,
				"%0*d",
				Tsuppliers_date_len,
				v_suppliers_date
				);

			n_data += sprintf(
				glbMsgBuffer + n_data,
				"%0*d",
				Tmacabi_centers_dat_len,
				v_macabi_centers_dat
				);

			n_data += sprintf(
				glbMsgBuffer + n_data,
				"%0*d",
				Terror_list_date_len,
				v_error_list_date
				);

			n_data += sprintf(
				glbMsgBuffer + n_data,
				"%0*d",
				Tdisk_space_len,
				v_disk_space
				);

			n_data += sprintf(
				glbMsgBuffer + n_data,
				"%0*d",
				Tnet_flag_len,
				v_net_flag
				);

			n_data += sprintf(
				glbMsgBuffer + n_data,
				"%0*d",
				Tcomm_fail_filesize_len,
				v_comm_fail_filesize
				);

			n_data += sprintf(
				glbMsgBuffer + n_data,
				"%0*d",
				Tbackup_date_len,
				v_backup_date
				);

			n_data += sprintf(
				glbMsgBuffer + n_data,
				"%0*d",
				Tavailable_mem_len,
				v_available_mem
				);

			n_data += sprintf(
				glbMsgBuffer + n_data,
				"%0*d",
				Tpc_date_len,
				v_pc_date
				);

			n_data += sprintf(
				glbMsgBuffer + n_data,
				"%0*d",
				Tpc_time_len,
				v_pc_time
				);

			n_data += sprintf(
				glbMsgBuffer + n_data,
				"%0*d",
				Tdb_date_len,
				v_db_date
				);

			n_data += sprintf(
				glbMsgBuffer + n_data,
				"%0*d",
				Tdb_time_len,
				v_db_time
				);

			n_data += sprintf(
				glbMsgBuffer + n_data,
				"%0*d",
				Tclosing_type_len,
				v_closing_type
				);

			n_data += sprintf(
				glbMsgBuffer + n_data,
				"%0*d",
				Tuser_ident_len,
				v_user_ident
				);

			n_data += sprintf(
				glbMsgBuffer + n_data,
				"%0*d",
				Terror_code_list_fl_len,
				v_error_code_list_fl
				);

			n_data += sprintf(
				glbMsgBuffer + n_data,
				"%0*d",
				Tdiscounts_flag_len,
				v_discounts_flag
				);

			n_data += sprintf(
				glbMsgBuffer + n_data,
				"%0*d",
				Tsuppliers_flag_len,
				v_suppliers_flag
				);

			n_data += sprintf(
				glbMsgBuffer + n_data,
				"%0*d",
				Tmacabi_centers_fla_len,
				v_macabi_centers_fla
				);

			n_data += sprintf(
				glbMsgBuffer + n_data,
				"%-*.*s",
				Tsoftware_ver_need_len,
				Tsoftware_ver_need_len,
				v_software_ver_need
				);

			n_data += sprintf(
				glbMsgBuffer + n_data,
				"%-*.*s",
				Tsoftware_ver_pc_len,
				Tsoftware_ver_pc_len,
				v_software_ver_pc
				);
if (n_data != RECORD_LEN_2501 + MESSAGE_ID_LEN) GerrLogMini(GerrId, "2501: Buffer {%s}, Len %d, Correct Len %d.", glbMsgBuffer, n_data, RECORD_LEN_2501 + MESSAGE_ID_LEN);
			StaticAssert( n_data == RECORD_LEN_2501 + MESSAGE_ID_LEN );

			/*=======================================================================
			||									||
			||			Send buffer to server				||
			||									||
			=======================================================================*/

			if (send_buffer_to_server (glbSocket, glbMsgBuffer, n_data) != success)
			{
				retval = CM_RESTART_CONNECTION;
				cursor_flg = 1;
				break;
			}
			else need_rollback = 1;


			/*=======================================================================
			||									||
			||			Delete record logically				||
			||									||
			=======================================================================*/

			ExecSQL (	MAIN_DB, AS400CL_2501_UPD_pharmacy_log_cur,
						&REPORTED_TO_AS400, END_OF_ARG_LIST				);

			if( SQLERR_code_cmp( SQLERR_access_conflict ) == MAC_TRUE )
			{
				retval = SB_DATA_ACCESS_CONFLICT;
				break;
			}
			else
			{
				if( SQLERR_error_test() != MAC_OK )
				{
					retval = SB_LOCAL_DB_ERROR;
					break;
				}
			}

			//	printf("send pharmacy_log line for pharm -----%d----\n",
			//		v_pharmacy_code ); fflush(stdout);

			if( ++n_uncommited_recs == BATCH_DIM )
			{
				retval = SB_BATCH_SUCCESSFULLY_PASSED;
				break;
			}


			/* << -------------------------------------------------------- >> */
			/*		    get server's response-message		  */
			/* << -------------------------------------------------------- >> */
			exit_flg = false;

			server_msg = get_msg_from_server( NO_WAIT_MODE );

			switch( server_msg )
			{
				case CM_SYSTEM_IS_NOT_BUSY:
				case CM_NO_MESSAGE_PASSED:/* NONMESSAGE */

    				usleep( 5000  );
					break;

				default:

					retval = server_msg;
					exit_flg = true;
			}

			if( exit_flg == true )
			{
				break;
			}

		}	// while( 1 )

	}
	while( 0 );

    if( cursor_flg )
    {
		CloseCursor (	MAIN_DB, AS400CL_2501_pharmacy_log_cur	);

		SQLERR_error_test();
    }

	recs_to_commit = n_uncommited_recs;

    POP_RET (retval);

} /* end of handler 2501 */



/*=======================================================================
||																		||
||			Message 2502 --												||
||																		||
 =======================================================================*/
static int pass_a_single_record_2502 (TMsg2502Record *rec_ptr)
{
	// Local variables
	TSuccessStat	retval = success;	// Unless something goes wrong!
	int				n_data;
	int				server_msg;


	pushname ("pass_a_single_record_2502");


	// Initialize variables

	// DonR 08Jul2004: AS400 can cope with only a single digit for
	// reason-to-dispense. Accordingly, translate 99 (dispensed "kacha")
	// into 9.
	if (rec_ptr->v_reason_to_disp == 99)
		rec_ptr->v_reason_to_disp = 9;

	// DonR 22Jun2010: HACK! AS/400 field RK9021P/EANJDT was mistakenly defined as length 7
	// instead of length 8; and since changing the AS/400 field format to what it's supposed
	// to be would be very time-consuming, instead we'll translate the century from "20" to "1".
	if (rec_ptr->del_sale_date > 19000000)
		rec_ptr->del_sale_date -= 19000000;

	// DonR 30Jun2010: ANOTHER HACK! It turns out that the AS/400 people want the Action Code to
	// be zero for drug sales that were made with pre-Nihul-Tikrot transactions. I set up the
	// database column with a default of 1, so here I will un-default the default. Yuck.
	// (Note that as Action Code is sent to AS/400 only for prescriptions, we don't have to
	// worry about prescription_drugs in this regard.)
	if (rec_ptr->OriginCode < 5000)
		rec_ptr->action_type = 0;

	// DonR 17Dec2012 "Yahalom": Set up translated numeric member insurance for AS/400.
	switch (*rec_ptr->insurance_type)
	{
		case 'B':	rec_ptr->member_insurance = BASIC_INS_USED;		break;
		case 'K':	rec_ptr->member_insurance = KESEF_INS_USED;		break;
		case 'Z':	rec_ptr->member_insurance = ZAHAV_INS_USED;		break;
		case 'Y':	rec_ptr->member_insurance = YAHALOM_INS_USED;	break;
		default:	rec_ptr->member_insurance = NO_INS_USED; // Shouldn't ever happen.
	}


	// Move prescription record into buffer.
	// DonR 03Apr2011: Replace member_price_code with sale_req_error; the former was being automatically
	// zeroed-out on the AS/400 side, so no real information is being lost.
	n_data = 0;
	n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	MESSAGE_ID_LEN,				2502							);	//   4
	n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	Tpharmacy_code_len,			rec_ptr->v_pharmacy_code		);	//  11
	n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	Tinstitued_code_len,		rec_ptr->v_institued_code		);	//  13
	n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	Tterminal_id_len,			rec_ptr->v_terminal_id			);	//  15
	n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	Tprice_list_code_len,		rec_ptr->v_price_list_code		);	//  18
	n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	Tmember_institued_len,		rec_ptr->v_member_institued		);	//  20
	n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	Tmember_id_len,				rec_ptr->v_member_id			);	//  29
	n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	Tmem_id_extension_len,		rec_ptr->v_mem_id_extension		);	//  30
	n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	Tcard_date_len,				rec_ptr->v_card_date			);	//  34
	n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	Tdoctor_id_type_len,		rec_ptr->v_doctor_id_type		);	//  35
	n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	Tdoctor_id_len,				rec_ptr->v_doctor_id			);	//  44
	n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	Tdoctor_insert_mode_len,	rec_ptr->v_doctor_insert_mode	);	//  45
	n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	Tpresc_source_len,			rec_ptr->v_presc_source			);	//  47
	n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	Treceipt_num_len,			rec_ptr->v_receipt_num			);	//  54
	n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	Tuser_ident_len,			rec_ptr->v_user_ident			);	//  63
	n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	Tdoctor_presc_id_len,		rec_ptr->v_doctor_presc_id		);	//  69
	n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	Tpresc_pharmacy_id_len,		rec_ptr->v_presc_pharmacy_id	);	//  75
	n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	Tprescription_id_len,		rec_ptr->v_prescription_id		);	//  84
	n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	Tprice_code_len,			rec_ptr->sale_req_error			);	//  88
	n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	Tmember_discount_pt_len,	rec_ptr->v_member_discount_pt	);	//  93
	n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	Tcredit_type_code_len,		rec_ptr->v_credit_type_used		);	//  95
	n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	Tmacabi_code_len,			rec_ptr->v_macabi_code			);	//  98
	n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	Tmacabi_centers_num_len,	rec_ptr->v_macabi_centers_num	);	// 105
	n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	Tmacabi_centers_num_len,	rec_ptr->v_doc_device_code		);	// 112
	n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	Treason_for_discnt_len,		rec_ptr->v_reason_for_discnt	);	// 117
	n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	Treason_to_disp_len,		rec_ptr->v_reason_to_disp		);	// 119
	n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	Tdate_len,					rec_ptr->v_date					);	// 127
	n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	Ttime_len,					rec_ptr->v_time					);	// 133
	n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	Tspecial_presc_num_len,		rec_ptr->v_special_presc_num	);	// 141
	n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	Tspec_pres_num_sors_len,	rec_ptr->v_spec_pres_num_sors	);	// 142
	n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	Tdiary_month_len,			rec_ptr->v_diary_month			);	// 146
	n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	Terror_code_len,			rec_ptr->v_error_code			);	// 150
	n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	Tcomm_error_code_len,		rec_ptr->v_comm_error_code		);	// 154
	n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	Telect_prescr_lines_len,	rec_ptr->v_prescription_lines	);	// 156


	// 17Jun2010: "Nihul Tikrot" fields.
	// 17Dec2012 "Yahalom": Add numeric member-insurance value.
	n_data += sprintf (glbMsgBuffer + n_data,
						"%0*d%0*d%0*d%0*d%0*d%0*d%0*d%0*d%0*d%0*d%0*d%0*d%0*d",
						2,  rec_ptr->action_type,
						9,  rec_ptr->del_presc_id,
						8,  rec_ptr->del_sale_date,
						9,  rec_ptr->card_owner_id,
						1,  rec_ptr->card_owner_id_code,
						2,  rec_ptr->num_payments,
						2,  rec_ptr->payment_method,
						2,  rec_ptr->credit_reason_code,
						1,  rec_ptr->tikra_called_flag,
						4,  rec_ptr->tikra_status_code,
						9,  rec_ptr->tikra_discount,
						9,  rec_ptr->subsidy_amount,
						2,  rec_ptr->member_insurance);																	// 216

	// DonR 01Jun2021 User Story #144324 - add Online Order Number to RK9021P.
	n_data += sprintf (glbMsgBuffer + n_data, "%0*ld",	11,							rec_ptr->online_order_num		);	// 227

	// DonR 09Aug2021 User Story #163882 - add darkonai_plus_sale to RK9021P (EAZRZR).
	n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	 2,							rec_ptr->darkonai_plus_sale		);	// 229

	// DonR 27Apr2023 User Story #432608: Add Magento Order Number.
	//n_data += sprintf (glbMsgBuffer + n_data, "%0*ld",	11,							rec_ptr->MagentoOrderNum		);	// 240
	//Marianna 25Mar2024 - added space to not lose the last num
	n_data += sprintf(glbMsgBuffer + n_data, "%0*ld ",	10,							rec_ptr->MagentoOrderNum		);	// 240

	// DonR 17Nov2016: Instead of treating an over-long row as a database problem,
	// return a new "skip this row and continue" status. This prevents a single
	// bad row from blocking transmission of good data.
	if (n_data != RECORD_LEN_2502 + MESSAGE_ID_LEN)
	{
		GerrLogMini (GerrId,
					 "Rec 2502: PrID %d length %d does not match %d - skipping!",
					 rec_ptr->v_prescription_id, n_data, RECORD_LEN_2502 + MESSAGE_ID_LEN);
		POP_RET (skip_row);
	}

	// Send to server.
	if (send_buffer_to_server (glbSocket, glbMsgBuffer, n_data)	!= success)
	{
		POP_RET (CM_RESTART_CONNECTION);
	}
	else
	{
		need_rollback = 1;		// We actually did send something successfully.
	}

	// Try getting a response at this point.
	server_msg = get_msg_from_server (NO_WAIT_MODE);

	switch (server_msg)
	{
		case CM_SYSTEM_IS_NOT_BUSY:
		case CM_NO_MESSAGE_PASSED:

			usleep (2000);	// 2 milliseconds (was 2.5).
			break;

		default:

			GerrLogMini (GerrId,
						 "Got msg <%d> from server after sending 2502. Sent %s, len = %d.",
						 server_msg, glbMsgBuffer, strlen (glbMsgBuffer));
			retval = server_msg;
			break;
	}


	POP_RET (retval);
} /* end of handler 2502 */



// DonR 03Apr2011: Disabled special_prescs upload, since it was never in "real" use and the AS/400 table,
// RK9023, is being deleted (or maybe reused for something else).



/*=======================================================================
||																		||
||			Message 2506 --	New single-row version						||
||																		||
 =======================================================================*/
static int pass_a_single_record_2506 (TMsg2506Record *p_rec)
{
	// Local variables
	TSuccessStat	retval = success;	// Unless something goes wrong!
	int				n_data;
	int				diagnose_from;		// DonR 11Jan2024: Used for the diagnostic version of send_buffer_to_server().
	int				server_msg;


	pushname ("pass_a_single_record_2506");


	// Initialize variables

	// DonR 31Aug2005: If this was a "gadget" sale, don't store the Link to Addition parameter.
	if ((p_rec->v_particip_meth % 10) == FROM_GADGET_APP)
	{
		p_rec->v_link_ext_to_drug = 0;
	}

	// DonR 15Oct2018 CR #13430: Convert text field(s) to "proper" Hebrew for AS/400 and other systems (like
	// CouchBase). Note that the complaint was only for times_of_day/E2DPUB - but we might as well fix all four.
	// UPDATE, 16Oct2018: Kobi Atari seems to have fixed this problem by changing the AS/400-side port encoding.
//	switch_to_win_heb_new ((unsigned char *)p_rec->times_of_day);
//	switch_to_win_heb_new ((unsigned char *)p_rec->course_units);
//	switch_to_win_heb_new ((unsigned char *)p_rec->days_of_week);
//	switch_to_win_heb_new ((unsigned char *)p_rec->side_of_body);


	// Move drug-line record into buffer.
	// DonR 17Apr2023 User Story #432608: Change length of Total Drug Price and Total Member Price from 9 to 10.
	// This lets us handle deletions of hugely expensive sales.
	n_data = 0;
	n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	MESSAGE_ID_LEN,			2506							);
	n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	Tpharmacy_code_len,		p_rec->v_pharmacy_code			);	//  1
	n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	Tinstitued_code_len,	p_rec->v_institued_code			);	//  2
	n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	Tprescription_id_len,	p_rec->v_prescription_id		);	//  3
	n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	9,						p_rec->v_largo_code				);	//  4 Marianna 14Jul2024 User Story #330877
	n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	Top_len,				p_rec->v_op						);	//  5
	n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	Tunits_len,				p_rec->v_units					);	//  6
	n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	Tduration_len,			p_rec->v_duration				);	//  7
	n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	Tprice_for_op_len,		p_rec->v_price_for_op			);	//  8
	n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	Top_stock_len,			p_rec->v_op_stock				);	//  9
	n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	Tunits_stock_len,		p_rec->v_units_stock			);	// 10
	n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	10,						p_rec->v_total_member_price		);	// 11 DonR 17Apr2023 User Story #432608
	n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	Telect_prescr_lines_len,p_rec->v_line_no				);	// 12
	n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	Tcomm_error_code_len,	p_rec->v_comm_error_code		);	// 13
	n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	Tdrug_answer_code_len,	p_rec->v_drug_answer_code		);	// 14
	n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	10,						p_rec->v_total_drug_price		);	// 15 DonR 17Apr2023 User Story #432608
	n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	Tstop_use_date_len,		p_rec->v_stop_use_date			);	// 16
	n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	Tstop_blood_date_len,	p_rec->v_stop_blood_date		);	// 17
	n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	Tdel_flg_len,			p_rec->v_del_flg				);	// 18
	n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	Tprice_replace_len,		p_rec->v_price_replace			);	// 19
	n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	Tprice_extension_len,	p_rec->v_price_extension		);	// 20
	n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	9,						p_rec->v_link_ext_to_drug		);	// 21 Marianna 16Jul2024 User Story #330877
	n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	Tmacabi_price_flg_len,	p_rec->v_macabi_price_flg		);	// 22
	n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	Tdoctor_presc_id_len,	p_rec->v_dr_presc_id			);	// 23
	n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	Tdate_len,				p_rec->v_dr_presc_date			);	// 24
	n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	Tparticip_method_len,	p_rec->v_particip_meth			);	// 25

	// DonR 25Dec2008: Updated_by holds health-basket status. Sending with length 5 to
	// allow 4 characters for future use.
	n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	5,						p_rec->v_updated_by				);	// 26
	n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	Trrn_len,				p_rec->v_prw_rrn				);	// 27
	n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	9,						p_rec->v_dr_largo_code			);	// 28 Marianna 14Jul2024 User Story #330877
	n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	Tsubst_permitted_len,	p_rec->v_subst_permitted		);	// 29
	n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	Tunits_len,				p_rec->v_units_unsold			);	// 30
	n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	Top_len,				p_rec->v_op_unsold				);	// 31
	n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	Tunits_len,				p_rec->v_units_per_dose			);	// 32
	n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	Tunits_len,				p_rec->v_doses_per_day			);	// 33
	n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	Tdiscount_percent_len,	p_rec->v_member_discount_pt		);	// 34
	n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	Tcalc_member_price_len,	p_rec->v_calc_member_price		);	// 35
    n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	Tspecial_presc_num_len,	p_rec->v_special_presc_num		);	// 36
    n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	Tspec_pres_num_sors_len,p_rec->v_spec_pres_num_sors		);	// 37
    n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	Tsupplier_price_len,	p_rec->v_supplier_price			);	// 38

	// "Nihul Tikrot" fields - four sent in one "glop", plus two more added 18Jun2013.
	n_data += sprintf (glbMsgBuffer + n_data,
					   "%0*d%c%0*d%0*d%0*d%0*d",
					   5,  p_rec->rule_number,
					       p_rec->tikra_type_code,
					   1,  p_rec->subsidized,
					   3,  p_rec->discount_code,
					   3,  p_rec->why_future_sale_ok,
					   3,  p_rec->qty_limit_chk_type);

	// DonR 15Apr2015: Additional fields for Digital Prescriptions - once more, sent as a single "glop".
	// DonR 30Mar2023 User Story #432608: Times of Day is now sent as 100 characters instead of 40.
	// (Note that we get up to 200 characters from the pharmacy.)
	// DonR 09Jan2024 HOT (POSSIBLE) FIX: Strip leading and trailing spaces from times_of_day. There is
	// some kind of very strange problem with this field, where we get sporadic bad values stored on
	// AS/400. I'm not sure if doing the StripAllSpaces will help, but at least it shouldn't hurt. Much.
	// DonR 14Jan2024: In fact, the problem we saw with Hebrew in RK9022P had nothing to do with program
	// code - it was because I had left another copy of As400UnixClient running in a new Linux environment
	// that was not getting Hebrew properly from the database. Accordingly, I'm going to comment out the
	// call to StripAllSpaces().
//	StripAllSpaces (p_rec->times_of_day);
//if (strlen(p_rec->times_of_day)) GerrLogMini (GerrId, "%d: {%s}", p_rec->v_prescription_id, p_rec->times_of_day);
//GerrLogMini (GerrId, "%d:", p_rec->v_prescription_id);

	n_data += sprintf (glbMsgBuffer + n_data,
					   "%0*d%0*d%0*d%0*d%0*d%0*d%0*d%-*.*s%0*d%0*d%-*.*s%-*.*s%-*.*s%-*.*s%0*d",
					     9,			p_rec->doctor_id,
						 1,			p_rec->doctor_id_type,
						 2,			p_rec->presc_source,
						 7,			p_rec->doctor_facility,
						 2,			p_rec->elect_pr_status,
						 1,			p_rec->use_instr_template,
						 3,			p_rec->how_to_take_code,
						 3,   3,	p_rec->unit_code,
						 3,			p_rec->course_treat_days,
						 3,			p_rec->course_length,
					    10,  10,	p_rec->course_units,
					    20,  20,	p_rec->days_of_week,
					   100, 100,	p_rec->times_of_day,
					    10,  10,	p_rec->side_of_body,
					     1,			p_rec->use_instr_changed);

	// DonR 11Jan2024: Set the "diagnose from" offset to the beginning of the times_of_day field.
	diagnose_from = n_data - 111;

	// DonR 11Jun2017 CR #8425: Add Member Discount Computed (based on member discount percent)
	// and Number of Linked Doctor Prescriptions to upload.
	// DonR 22Aug2018: Add Barcode Scanned status and Digital Prescription status.
	// DonR 03Oct2018 CR #13262: Added Member Diagnosis Code.
	// DonR 18Aug2021: Added Voucher Amount Used.
	n_data += sprintf (glbMsgBuffer + n_data,
					   "%0*d%0*d%0*d%0*d%0*d%0*d%0*d",
					   9,		p_rec->member_discount_amt,
					   4,		p_rec->num_linked_rxes,
					   1,		p_rec->BarcodeScanned,
					   1,		p_rec->IsDigital,
					   6,		p_rec->member_diagnosis,
					   9,		p_rec->ph_OTC_unit_price,
					   9,		p_rec->voucher_amount_used);

	// DonR 30Mar2023 User Story #432608: Add eight new fields to prescription_drugs/RK9022P.
	n_data += sprintf (glbMsgBuffer + n_data,
					   "%-*.*s%0*d%0*d%0*d%0*d%0*d%0*d%0*d%0*d",
					   100, 100,	p_rec->UsageInstrGiven,
					     9,			p_rec->MaccabiOtcPrice,
					     9,			p_rec->SalePkgPrice,
					     6,			p_rec->SaleNum4Price,
					     6,			p_rec->SaleNumBuy1Get1,
					     9,			p_rec->Buy1Get1Savings,
					     9,			p_rec->ByHandReduction,
					     1,			p_rec->IsConsignment,
						 3,			p_rec->NumCourses); 

	// Marianna 29Jun2023 User Story #461368: Added alternate_yarpa_price to prescription_drugs/RK9022P.
	n_data += sprintf (glbMsgBuffer + n_data,
					   "%0*d",
						 9,			p_rec->alternate_yarpa_price); 

	// Marianna 10Aug2023 User Story #469361: Added 4 new fields to prescription_drugs/RK9022P.
	n_data += sprintf (glbMsgBuffer + n_data,
					   "%0*d%0*d%0*d%0*d",
						 8,			p_rec->blood_start_date,
						 5,			p_rec->blood_duration,
						 8,			p_rec->blood_last_date,
						 1,			p_rec->blood_data_calculated);
	
	// Marianna 19Feb2024 User Story #540234: Added 1 new field to prescription_drugs/RK9022P - CANNABIS.
	n_data += sprintf(glbMsgBuffer + n_data, "%0*d", 3, p_rec->NumCannabisProducts);	


	// DonR 17Nov2016: Instead of treating an over-long row as a database problem,
	// return a new "skip this row and continue" status. This prevents a single
	// bad row from blocking transmission of good data.
	if (n_data != RECORD_LEN_2506 + MESSAGE_ID_LEN)
	{
		GerrLogMini (GerrId,
					 "Rec 2506: PrID %d Largo %d length %d does not match %d - skipping!",
					 p_rec->v_prescription_id, p_rec->v_largo_code, n_data, RECORD_LEN_2506 + MESSAGE_ID_LEN);
		POP_RET (skip_row);
	}


	// Send to server.
//	// DonR 11Jan2024: We're having data problems which *may* be connected to the volume we're
//	// sending over the TCP port - so let's add a little delay here just to see if it helps.
//	usleep (1000);	// 1 millisecond
//
	// DonR 11Jan2024: Use send_buffer_to_server_diagnostic() if you need to diagnose
	// exactly what we're sending to AS/400 - including a character-by-character dump
	// to check on ASCII-to-EBCDIC translation.
//	if (send_buffer_to_server_diagnostic (glbSocket, glbMsgBuffer, n_data, diagnose_from, p_rec->v_prescription_id)	!= success)
	if (send_buffer_to_server (glbSocket, glbMsgBuffer, n_data)	!= success)
	{
		POP_RET (CM_RESTART_CONNECTION);
	}
	else
	{
		need_rollback = 1;		// We actually did send something successfully.
	}


	// Try getting a response at this point.
	server_msg = get_msg_from_server (NO_WAIT_MODE);

	switch (server_msg)
	{
		case CM_SYSTEM_IS_NOT_BUSY:
		case CM_NO_MESSAGE_PASSED:

			usleep (2000);	// 2 milliseconds (was 2.5).
			break;

		default:

			GerrLogMini (GerrId,
						 "Got msg <%d> from server after sending 2506.",
						 server_msg);
			retval = server_msg;
			break;
	}

	POP_RET (retval);
} // End of single 2506 handler.



/*=======================================================================
||																		||
||			Message 2522 --	Pass a single prescription_msgs row			||
||																		||
 =======================================================================*/
static TSuccessStat pass_a_single_record_2522 (TMsg2522Record *p_rec)
{
	// Local variables
	TSuccessStat	retval = success;	// Unless something goes wrong!
	int				n_data;
	int				server_msg;


	pushname ("pass_a_single_record_2522");


	// Move prescription_msgs record into buffer.
	n_data = 0;
	n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	MESSAGE_ID_LEN,			2522						);
	n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	Tprescription_id_len,	p_rec->v_prescription_id	);
	n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	9,						p_rec->v_largo_code			);	//Marianna 14Jul2024 User Story #330877
	n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	Telect_prescr_lines_len,p_rec->v_line_no			);
	n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	Tdrug_answer_code_len,	p_rec->v_error_code			);
	n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	Tseverity_level_len,	p_rec->v_severity_level		);


	if( n_data != RECORD_LEN_2522 + MESSAGE_ID_LEN )
	{
		GerrLogMini (GerrId,
					 "Rec 2522 length %d does not match %d!",
					 n_data, RECORD_LEN_2522 + MESSAGE_ID_LEN);
	    POP_RET (SB_LOCAL_DB_ERROR);
	}


	// Send to server.
	if (send_buffer_to_server (glbSocket, glbMsgBuffer, n_data)	!= success)
	{
		POP_RET (CM_RESTART_CONNECTION);
	}
	else
	{
		need_rollback = 1;		// We actually did send something successfully.
	}


	// Try getting a response at this point.
	server_msg = get_msg_from_server (NO_WAIT_MODE);

	switch (server_msg)
	{
		case CM_SYSTEM_IS_NOT_BUSY:
		case CM_NO_MESSAGE_PASSED:

			usleep (2000);	// 2 milliseconds (was 2.5).
			break;

		default:

			GerrLogMini (GerrId,
						 "Got msg <%d> from server after sending 2522.",
						 server_msg);
			retval = server_msg;
			break;
	}

	POP_RET (retval);
} // End of single 2522 handler.



/*=======================================================================
||																		||
||			Message 2523 --	Pass a single pd_rx_link row				||
||																		||
 =======================================================================*/
static TSuccessStat pass_a_single_record_2523 (TMsg2523Record *p_rec)
{
	// Local variables
	TSuccessStat	retval = success;	// Unless something goes wrong!
	int				n_data;
	int				server_msg;


	pushname ("pass_a_single_record_2523");


	// Move pd_rx_link record into buffer.
	// DonR 15Jun2015: AS/400 Rokhut needs the Largo Sold in RK9122ACP, so I've added that field
	// to pd_rx_link and we'll send it in place of Largo Prescribed.
	// DonR 23Jun2015: Largo Prescribed has now been added to RK9122ACP, so we'll send it as well.
	// Also, changed length of Largo Code fields from 5 to 9, to anticipate future expansion, and
	// changed sprintf masks from "ld" to "d" (except for Visit Number, which is a 64-bit long int).
	// DonR 01May2019: Instead of sending OP/Units in terms of the drug originally prescribed, send
	// them in terms of the drug that was actually sold.
	// DonR 11Dec2024 User Story #373619: Add rx_origin to values sent to AS/400.
	n_data = 0;
	n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	MESSAGE_ID_LEN,		2523						);
	n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	 9,					p_rec->prescription_id		);
	n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	 3,					p_rec->line_no				);
	n_data += sprintf (glbMsgBuffer + n_data, "%0*ld",	11,					p_rec->visit_number			);
	n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	 5,					p_rec->clicks_line_number	);	// DonR 29Apr2019 CR #288807474
	n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	 6,					p_rec->doctor_presc_id		);
	n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	 9,					p_rec->largo_sold			);
	n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	 9,					p_rec->largo_prescribed		);
	n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	 8,					p_rec->valid_from_date		);
	n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	 5,					p_rec->prev_unsold_op		);
	n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	 5,					p_rec->prev_unsold_units	);
//	n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	 6,					p_rec->op_sold				);
//	n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	 6,					p_rec->units_sold			);
	n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	 6,					p_rec->sold_drug_op			);
	n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	 6,					p_rec->sold_drug_units		);
	n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	 1,					p_rec->close_by_rounding	);
	n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	 1,					p_rec->rx_sold_status		);
	n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	 2,					p_rec->rx_origin			);


	if( n_data != RECORD_LEN_2523 + MESSAGE_ID_LEN )
	{
		GerrLogMini (GerrId,
					 "Rec 2523 length %d does not match %d!",
					 n_data, RECORD_LEN_2523 + MESSAGE_ID_LEN);
	    POP_RET (SB_LOCAL_DB_ERROR);
	}


	// Send to server.
	if (send_buffer_to_server (glbSocket, glbMsgBuffer, n_data)	!= success)
	{
		POP_RET (CM_RESTART_CONNECTION);
	}
	else
	{
		need_rollback = 1;		// We actually did send something successfully.
	}


	// Try getting a response at this point.
	server_msg = get_msg_from_server (NO_WAIT_MODE);

	switch (server_msg)
	{
		case CM_SYSTEM_IS_NOT_BUSY:
		case CM_NO_MESSAGE_PASSED:

			usleep (2000);	// 2 milliseconds (was 2.5).
			break;

		default:

			GerrLogMini (GerrId,
						 "Got msg <%d> from server after sending 2523.",
						 server_msg);
			retval = server_msg;
			break;
	}

	POP_RET (retval);
} // End of single 2523 handler.


// DonR 28Jul2021 User Story #169197 - added prescription_vouchers upload.
/*=======================================================================
||																		||
||			Message 2524 --	Pass a single prescription_vouchers row		||
||																		||
 =======================================================================*/
static TSuccessStat pass_a_single_record_2524 (TMsg2524Record *p_rec)
{
	// Local variables
	TSuccessStat	retval = success;	// Unless something goes wrong!
	int				n_data;
	int				server_msg;


	pushname ("pass_a_single_record_2524");


	// Move prescription_vouchers record into buffer.
	n_data = 0;
	n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	MESSAGE_ID_LEN,		2524							);
	n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	 9,					p_rec->prescription_id			);
	n_data += sprintf (glbMsgBuffer + n_data, "%0*ld",	11,					p_rec->voucher_num				);
	n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	 9,					p_rec->member_id				);
	n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	 1,					p_rec->mem_id_extension			);
	n_data += sprintf (glbMsgBuffer + n_data, "%-*.*s",	15,		15,			p_rec->voucher_type				);
	n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	 9,					p_rec->voucher_discount_given	);
	n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	 9,					p_rec->voucher_amount_remaining	);
	n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	 9,					p_rec->original_voucher_amount	);
	n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	 8,					p_rec->sold_date				);

	if( n_data != RECORD_LEN_2524 + MESSAGE_ID_LEN )
	{
		GerrLogMini (GerrId,
					 "Rec 2524 length %d does not match %d!",
					 n_data, RECORD_LEN_2524 + MESSAGE_ID_LEN);
	    POP_RET (SB_LOCAL_DB_ERROR);
	}


	// Send to server.
	if (send_buffer_to_server (glbSocket, glbMsgBuffer, n_data)	!= success)
	{
		POP_RET (CM_RESTART_CONNECTION);
	}
	else
	{
		need_rollback = 1;		// We actually did send something successfully.
	}


	// Try getting a response at this point.
	server_msg = get_msg_from_server (NO_WAIT_MODE);

	switch (server_msg)
	{
		case CM_SYSTEM_IS_NOT_BUSY:
		case CM_NO_MESSAGE_PASSED:

			usleep (2000);	// 2 milliseconds (was 2.5).
			break;

		default:

			GerrLogMini (GerrId,
						 "Got msg <%d> from server after sending 2524.",
						 server_msg);
			retval = server_msg;
			break;
	}

	POP_RET (retval);
} // End of single 2524 handler.



// Marianna 14Feb2024 User Story #540234 - added Pd_cannabis_products upload.
/*=======================================================================
||																		||
||	Message 2525 --	Pass a single Pd_cannabis_products row				||
||																		||
 =======================================================================*/
static TSuccessStat pass_a_single_record_2525(TMsg2525Record* p_rec)
{
	// Local variables
	TSuccessStat	retval = success;	// Unless something goes wrong!
	int				n_data;
	int				server_msg;


	pushname("pass_a_single_record_2525");


	// Move Pd_cannabis_products record into buffer.
	n_data = 0;
	n_data += sprintf(glbMsgBuffer + n_data, "%0*d", MESSAGE_ID_LEN, 2525);
	n_data += sprintf(glbMsgBuffer + n_data, "%0*d",   9, p_rec->prescription_id);
	n_data += sprintf(glbMsgBuffer + n_data, "%0*d",   2, p_rec->line_no);
	n_data += sprintf(glbMsgBuffer + n_data, "%0*d",   9, p_rec->cannabis_product_code);
	n_data += sprintf(glbMsgBuffer + n_data, "%0*ld", 13, p_rec->cannabis_product_barcode);
	n_data += sprintf(glbMsgBuffer + n_data, "%0*d",   9, p_rec->group_largo_code);
	n_data += sprintf(glbMsgBuffer + n_data, "%0*d",   5, p_rec->op);
	n_data += sprintf(glbMsgBuffer + n_data, "%0*d",   5, p_rec->units);
	n_data += sprintf(glbMsgBuffer + n_data, "%0*d",   9, p_rec->price_per_op);
	n_data += sprintf(glbMsgBuffer + n_data, "%0*d",   9, p_rec->product_sale_amount);
	n_data += sprintf(glbMsgBuffer + n_data, "%0*d",   9, p_rec->product_ptn_amount);


	if (n_data != RECORD_LEN_2525 + MESSAGE_ID_LEN)
	{
		GerrLogMini(GerrId,
			"Rec 2525 length %d does not match %d!",
			n_data, RECORD_LEN_2525 + MESSAGE_ID_LEN);
		POP_RET(SB_LOCAL_DB_ERROR);
	}


	// Send to server.
	if (send_buffer_to_server(glbSocket, glbMsgBuffer, n_data) != success)
	{
		POP_RET(CM_RESTART_CONNECTION);
	}
	else
	{
		need_rollback = 1;		// We actually did send something successfully.
	}


	// Try getting a response at this point.
	server_msg = get_msg_from_server(NO_WAIT_MODE);

	switch (server_msg)
	{
	case CM_SYSTEM_IS_NOT_BUSY:
	case CM_NO_MESSAGE_PASSED:

		usleep(2000);	// 2 milliseconds (was 2.5).
		break;

	default:

		GerrLogMini(GerrId,
			"Got msg <%d> from server after sending 2525.",
			server_msg);
		retval = server_msg;
		break;
	}

	POP_RET(retval);
} // End of single 2525 handler.



/*===============================================================================
||																				||
||			Message 2502 + 2506 - Delivered Prescriptions/Drugs/Msgs/Vouchers	||
||																				||
 ===============================================================================*/
static int pass_record_batch_2502_2506 (void)
{
	// DonR 21Jun2022: Because we were seeing some very strange results occasionally when
	// Transaction 6005 processed a sale deletion and hit an access-conflict error, I've
	// substantially simplified the code in this function. Basically, we no longer leave
	// cursors open; instead, we simply open each cursor when (and if) it's needed, do
	// what we need to do with its contents, then close it and move to the next cursor.
	// Hopefully this will give more predictable results without any real sacrifice in
	// performance.

	// Local variables
	int			BATCH_DIM;
	int			n_uncommited_recs;
	int			rc;
	int			retval = SB_END_OF_FETCH_NOCOMMIT;	// I.e. nothing has happened, until proven otherwise.
	int			ret_code;
	int			n_data;		/* length of data to be sent to server */
	int			first_pass	= 1;		// DonR 23SEP2002
	int			Excess;
	int			AddToMarginDays;
	int			AddToMarginHours;
	int			AddToMarginSeconds;
	int			DayOfWeek;
	int			LastPrIdDeleted	= 0;
	short		member_insurance;
	static int	using_current_time;
	static int	PR_cursor_finished;			// DonR 09May2013
	static int	PD_cursor_finished;			// DonR 09May2013
	static int	PM_cursor_finished;			// DonR 18Jun2013
	static int	RXL_cursor_finished;		// DonR 21Apr2015
	static int	PV_cursor_finished;			// DonR 28Jul2021 User Story #169197.
	static int	DelQ_cursor_finished;		// DonR 03Jul2016
	static int	PDCP_cursor_finished;		// Marianna User Story #540234 - Cannabis
	ISOLATION_VAR;

	// Database variables
	static TMsg2502Record	prescription_record;
	static TMsg2506Record	drug_line_record;
	static TMsg2522Record	pr_message_record;		// DonR 18Jun2013
	static TMsg2523Record	pd_rx_link_record;		// DonR 21Apr2015
	static TMsg2524Record	voucher_record;			// DonR 28Jul2021 User Story #169197.
	static TMsg2525Record	pd_cannabis_record;		// Marianna 14Feb2024 User Story #540234.
	int						prescription_count;		// DonR 23SEP2002
	int						presc_drugs_count;		// DonR 23SEP2002
	int						Today;					// DonR 19Oct2004
	int						TooRecently;			// DonR 19Oct2004
	short					OriginCode;				// DonR 30Jun2010
	short					SetAsReported;			// DonR 08May2013 - either 1, or 2 for stuff that doesn't need to be sent.
	int						DelPrID;				// DonR 03Jul2016
	int						DelPrLargo;				// DonR 03Jul2016


	pushname ("pass_record_batch_2502_2506");

	REMEMBER_ISOLATION;
	SET_ISOLATION_COMMITTED;

	// Initialization
	BATCH_DIM			= get_dimension_of_batch (2502);
	n_uncommited_recs	= 0;

	Today				= GetDate		();
	DayOfWeek			= GetDayOfWeek	();	// Sunday = 0, Saturday = 6.
	TooRecently			= GetTime		();
	TooRecently			= IncrementTime	(TooRecently, -60, NULL);	// 1-minute safety margin.
	using_current_time	= 1;

	// DonR 23SEP2002 begin:
	// The first time we get here (and subsequent times if more than 50,000 prescription rows
	// need to be transferred), check the size of the batch to be transferred. If more than
	// 50,000 rows are eligible for transfer, reduce the time-slice selected so that we select
	// a subset. We'll get the remainder on subsequent passes.
	if (check_2502_size)
	{
		// DonR 02Nov2005: Avoid unnecessary record-locked errors.
		SET_ISOLATION_DIRTY;

		do
		{
			ExecSQL (	MAIN_DB, AS400CL_2502_READ_prescriptions_count,
						&prescription_count,
						&NOT_REPORTED_TO_AS400,		&TooRecently,
						&Today,						END_OF_ARG_LIST			);

			if ((SQLCODE == 0) && (prescription_count <= 50000))
			{
				ExecSQL (	MAIN_DB, AS400CL_2506_READ_prescription_drugs_count,
							&presc_drugs_count,
							&NOT_REPORTED_TO_AS400,		&TooRecently,
							&Today,						END_OF_ARG_LIST			);
			}
			else
			{
				presc_drugs_count = 0;
			}

			GerrLogMini (	GerrId,
							"2502: Max. %d prescr / %d lines to send to AS400; SQLCODE = %d.",
							prescription_count, presc_drugs_count, SQLCODE	);

			// DonR 31Oct2005: New time-interval algorithm. This version should
			// "seek" the proper time interval much faster than the old version.
			// In essence, this method assumes that everything happens from 8:00
			// in the morning until 8:00 in the evening.
			if (prescription_count > 50000L)
			{
				using_current_time	= 0;
				Excess				= prescription_count - 50000L;

				// Assume a really "big" day so we don't go too far back.
				AddToMarginDays		= Excess / 60000L;

				AddToMarginHours	= (Excess % 60000L) / 5000L;

				// Go back one hour minimum.
				if ((AddToMarginDays == 0) && (AddToMarginHours == 0))
					AddToMarginHours = 1;

				// Saturday has almost no sales - so decrement automatically to Friday at 3:00 PM.
				if (DayOfWeek == 6)
				{
					AddToMarginDays++;
					TooRecently = 150000L;
				}

				// If it's now early morning, go back one extra day.
				if (TooRecently < 80000L)
					AddToMarginDays++;

				// Assume a working day of 8:00 AM to 8:00 PM.
				if ((TooRecently < 80000L) || (TooRecently > 200000L))
					TooRecently = 200000L;

				// If the maximum time is going to be decremented past 8:00 AM, (which can't happen,
				// by the way, if we've set TooRecently to 8:00 PM!) go back another day and adjust
				// accordingly. Thus, 9:00 AM minus five "working" hours = 16:00 the previous day.
				if (((TooRecently / 10000L) - AddToMarginHours) < 8)
				{
					AddToMarginDays++;
					AddToMarginHours -= 12;
				}

				// Make a further adjustment based on whether our new date will be a Saturday.
				DayOfWeek -= AddToMarginDays;
				while (DayOfWeek < 0)
					DayOfWeek += 7;

				if (DayOfWeek == 6)	// "Landing" on a Saturday.
				{
					AddToMarginDays++;
					DayOfWeek = 5;
				}

				// Finally, adjust the selection variables according to our calculations.
				if (AddToMarginDays > 0)
					Today = IncrementDate (Today, (0 - AddToMarginDays));

				if (AddToMarginHours != 0)
				{
					AddToMarginSeconds = 0 - (AddToMarginHours * 3600);
					TooRecently = IncrementTime (TooRecently, AddToMarginSeconds, &Today);
				}

				// If we're "landing" on a Friday and the new time is
				// later than 3:00 PM (when things pretty much shut down),
				// decrement the time accordingly.
				if ((DayOfWeek == 5) && (TooRecently > 150000L))
					TooRecently -= 50000L;	// I.e. go five hours further back.


				GerrLogMini (GerrId, "      Changing limit to %d / %d.", Today, TooRecently);

				first_pass = 0;
			}
			else
			{
				// If this is the first pass through this do-loop AND fewer than 50,000
				// prescriptions await transfer to AS400, the program is in "current" mode -
				// meaning that it no longer has to execute a separate select to determine
				// how many rows will be transferred. This logic saves time and DB access,
				// since we don't keep performing extra passes through the prescriptions table.
				if ((first_pass == 1) && (using_current_time))
				{
					check_2502_size = 0;
				}
			}
		}
		while (prescription_count > 50000);

	}	// check_2502_size is non-zero.
	// DonR 23SEP2002 end.

	SET_ISOLATION_COMMITTED;


	// DonR 09May2013: Set flags to indicate that all cursors have stuff to process.
	PR_cursor_finished		=
	PD_cursor_finished		=
	PM_cursor_finished		=
	RXL_cursor_finished		=
	PV_cursor_finished		=
	DelQ_cursor_finished	=
	PDCP_cursor_finished	= 0;

	// Loop over all prescriptions, drugs, etc.
	do
	{
		// 1) Prescriptions
		DeclareAndOpenCursorInto (	MAIN_DB, AS400CL_2502_prescriptions_cur,
									&prescription_record.v_pharmacy_code,				&prescription_record.v_institued_code,
									&prescription_record.v_terminal_id,					&prescription_record.v_price_list_code,
									&prescription_record.v_member_institued,			&prescription_record.v_member_id,
									&prescription_record.v_mem_id_extension,			&prescription_record.v_card_date,
									&prescription_record.v_doctor_id_type,				&prescription_record.v_doctor_id,
									&prescription_record.v_doctor_insert_mode,			&prescription_record.v_presc_source,

									&prescription_record.v_receipt_num,					&prescription_record.v_user_ident,
									&prescription_record.v_doctor_presc_id,				&prescription_record.v_presc_pharmacy_id,
									&prescription_record.v_prescription_id,				&prescription_record.sale_req_error,
									&prescription_record.v_member_discount_pt,			&prescription_record.v_credit_type_code,
									&prescription_record.v_credit_type_used,			&prescription_record.v_macabi_code,
									&prescription_record.v_macabi_centers_num,			&prescription_record.v_doc_device_code,

									&prescription_record.v_reason_for_discnt,			&prescription_record.v_reason_to_disp,
									&prescription_record.v_date,						&prescription_record.v_time,
									&prescription_record.v_special_presc_num,			&prescription_record.v_spec_pres_num_sors,
									&prescription_record.v_diary_month,					&prescription_record.v_error_code,
									&prescription_record.v_comm_error_code,				&prescription_record.v_prescription_lines,
									&prescription_record.action_type,					&prescription_record.del_presc_id,

									&prescription_record.del_sale_date,					&prescription_record.card_owner_id,
									&prescription_record.card_owner_id_code,			&prescription_record.num_payments,
									&prescription_record.payment_method,				&prescription_record.credit_reason_code,
									&prescription_record.tikra_called_flag,				&prescription_record.tikra_status_code,
									&prescription_record.tikra_discount,				&prescription_record.subsidy_amount,
									&prescription_record.insurance_type,				&prescription_record.OriginCode,

									&prescription_record.online_order_num,				&prescription_record.darkonai_plus_sale,
									&prescription_record.paid_for,						&prescription_record.MagentoOrderNum,

									&NOT_REPORTED_TO_AS400,								&TooRecently,
									&Today,												END_OF_ARG_LIST									);

		if (SQLCODE != 0)
		{
			if (SQLERR_error_test ())
			{
				retval = SB_DATA_ACCESS_CONFLICT;
				break;
			}
		}

		while (!PR_cursor_finished)
		{
			// Fetch prescription row.
			FetchCursor (	MAIN_DB, AS400CL_2502_prescriptions_cur	);

			if (SQLERR_code_cmp (SQLERR_end_of_fetch) == MAC_TRUE)
			{
				PR_cursor_finished = 1;
				break;
			}

			if (SQLERR_code_cmp (SQLERR_access_conflict) == MAC_TRUE)
			{
				retval = SB_DATA_ACCESS_CONFLICT;
				break;
			}

			if (SQLERR_error_test () == MAC_TRUE)
			{
				retval = SB_LOCAL_DB_ERROR;
				break;
			}


			// Send prescription row to server.
			// DonR 08May2013: Decide whether we actually want to send this row, based on its Pharmacy Code.
			if (prescription_record.v_pharmacy_code >= MinPharmacyToAs400)
			{
				ret_code = pass_a_single_record_2502 (&prescription_record);
				if (ret_code != success)
				{
					// DonR 17Nov2016: If a single row was over-length, we want to skip past it and continue
					// sending good rows. We intentionally keep the bad row marked as not sent - thus the bad
					// row will continue to generate log messages until someone corrects the data.
					if (ret_code == skip_row)
					{
						SetAsReported = NOT_REPORTED_TO_AS400;
					}
					else
					{
						GerrLogReturn (	GerrId,
										"pass_a_single_record_2502 returned %d for PrID %d.",
										ret_code, prescription_record.v_prescription_id	);

						retval = ret_code;
						break;
					}
				}
				else
				{
					SetAsReported = REPORTED_TO_AS400;
					n_uncommited_recs++;
					recs_to_commit++;
				}
			}
			else	// This is a fictitious sale - so don't send to AS/400.
			{
				SetAsReported = NO_NEED_TO_REPORT_TO_AS400;
			}

			ExecSQL (	MAIN_DB, AS400CL_2502_UPD_prescriptions, &SetAsReported, END_OF_ARG_LIST	);

			if (SQLERR_code_cmp (SQLERR_access_conflict) == MAC_TRUE)
			{
				retval = SB_DATA_ACCESS_CONFLICT;
				break;
			}

			if (SQLERR_error_test () == MAC_TRUE)
			{
				retval = SB_LOCAL_DB_ERROR;
				break;
			}

			if (n_uncommited_recs == BATCH_DIM)
				break;
		}	// while (!PR_cursor_finished)

		CloseCursor (	MAIN_DB, AS400CL_2502_prescriptions_cur	);


		// 2) If we're done with prescriptions, open the prescription_drugs cursor.
		if (PR_cursor_finished)
		{
			// Declare cursor for table PRESCRIPTION_DRUGS
			// DonR 29Mar2023 User Story #432608: Read 11 new columns.
			DeclareAndOpenCursorInto (	MAIN_DB, AS400CL_2506_prescription_drugs_cur,
										&drug_line_record.v_pharmacy_code,			&drug_line_record.v_institued_code,		&drug_line_record.v_prescription_id,
										&drug_line_record.v_largo_code,				&drug_line_record.v_op,					&drug_line_record.v_units,
										&drug_line_record.v_duration,				&drug_line_record.v_price_for_op,		&drug_line_record.v_op_stock,
										&drug_line_record.v_units_stock,			&drug_line_record.v_total_member_price,	&drug_line_record.v_line_no,

										&drug_line_record.v_comm_error_code,		&drug_line_record.v_drug_answer_code,	&drug_line_record.v_total_drug_price,
										&drug_line_record.v_stop_use_date,			&drug_line_record.v_stop_blood_date,	&drug_line_record.v_del_flg,
										&drug_line_record.v_price_replace,			&drug_line_record.v_price_extension,	&drug_line_record.v_link_ext_to_drug,
										&drug_line_record.v_macabi_price_flg,		&drug_line_record.v_calc_member_price,	&drug_line_record.v_dr_presc_id,

										&drug_line_record.v_dr_presc_date,			&drug_line_record.v_particip_meth,		&drug_line_record.v_updated_by,
										&drug_line_record.v_prw_rrn,				&drug_line_record.v_dr_largo_code,		&drug_line_record.v_subst_permitted,
										&drug_line_record.v_units_unsold,			&drug_line_record.v_op_unsold,			&drug_line_record.v_units_per_dose,
										&drug_line_record.v_doses_per_day,			&drug_line_record.v_member_discount_pt,	&drug_line_record.v_special_presc_num,

										&drug_line_record.v_spec_pres_num_sors,		&drug_line_record.v_supplier_price,		&drug_line_record.rule_number,
										&drug_line_record.tikra_type_code,			&drug_line_record.subsidized,			&drug_line_record.discount_code,
										&drug_line_record.why_future_sale_ok,		&drug_line_record.qty_limit_chk_type,	&drug_line_record.doctor_id,
										&drug_line_record.doctor_id_type,			&drug_line_record.presc_source,			&drug_line_record.doctor_facility,

										&drug_line_record.elect_pr_status,			&drug_line_record.use_instr_template,	&drug_line_record.how_to_take_code,
										&drug_line_record.unit_code,				&drug_line_record.course_treat_days,	&drug_line_record.course_length,
										&drug_line_record.course_units,				&drug_line_record.days_of_week,			&drug_line_record.times_of_day,
										&drug_line_record.side_of_body,				&drug_line_record.use_instr_changed,	&drug_line_record.member_discount_amt,

										&drug_line_record.num_linked_rxes,			&drug_line_record.BarcodeScanned,		&drug_line_record.IsDigital,
										&drug_line_record.member_diagnosis,			&drug_line_record.ph_OTC_unit_price,	&drug_line_record.voucher_amount_used,
										&drug_line_record.qty_limit_class_code,		&drug_line_record.UsageInstrGiven,		&drug_line_record.MaccabiOtcPrice,	// DonR 29Mar2023 begin.
										&drug_line_record.SalePkgPrice,				&drug_line_record.SaleNum4Price,		&drug_line_record.SaleNumBuy1Get1,

										&drug_line_record.Buy1Get1Savings,			&drug_line_record.ByHandReduction,		&drug_line_record.IsConsignment,
										&drug_line_record.NumCourses,																							// DonR 29Mar2023 end.
										&drug_line_record.alternate_yarpa_price,																				// Marianna 29Jun2023 User Story #461368.
										&drug_line_record.blood_start_date,			&drug_line_record.blood_duration,		&drug_line_record.blood_last_date,
										&drug_line_record.blood_data_calculated,																				// Marianna 10Aug2023 User Story #469361: Added 4 new fields
										&drug_line_record.NumCannabisProducts,																					// Marianna 19Feb2024 User Story #540234: Added 1 field.

										&NOT_REPORTED_TO_AS400,						&TooRecently,
										&Today,										END_OF_ARG_LIST																		);

			if (SQLCODE != 0)
			{
				if (SQLERR_error_test ())
				{
					retval = SB_DATA_ACCESS_CONFLICT;
					break;
				}
			}
		}	// Prescriptions cursor is finished, so we need to open the prescription_drugs cursor.


		// If we haven't finished sending all the selected prescriptions rows, don't send any
		// prescription_drugs rows - we'll get to them once we've finished with prescriptions.
		// In fact, the only circumstance in which we *do* want to send prescription_drugs
		// rows is when the first loop has terminated happily because of end-of-fetch -
		// so we don't need to test any other variables here.
		while ((PR_cursor_finished) && (!PD_cursor_finished))
		{
			// Fetch prescription_drugs row.
			FetchCursor (	MAIN_DB, AS400CL_2506_prescription_drugs_cur	);

			if (SQLERR_code_cmp (SQLERR_end_of_fetch) == MAC_TRUE)
			{
				// If we get here, we've come to the end of the first two cursors (but not the third one).
				PD_cursor_finished = 1;
				break;
			}

			if (SQLERR_code_cmp (SQLERR_access_conflict) == MAC_TRUE)
			{
				retval = SB_DATA_ACCESS_CONFLICT;
				break;
			}

			if (SQLERR_error_test () == MAC_TRUE)
			{
				retval = SB_LOCAL_DB_ERROR;
				break;
			}


			// Send drug-line row to server.
			// DonR 08May2013: Decide whether we actually want to send this row, based on its Pharmacy Code.
			// DonR 18Aug2021 BUG FIX: The "if" below was using prescription_record instead of drug_line_record!
			if (drug_line_record.v_pharmacy_code >= MinPharmacyToAs400)
			{
				// DonR 01Jul2024 EMERGENCY FIX: On AS/400, the maximum length of Blood Duration (RK9022P/E2IXST)
				// is 3 digits. Accordingly, if we see a value > 999, log a message and set the variable to 999.
				if (drug_line_record.blood_duration > 999)
				{
					GerrLogMini (	GerrId, "Rec 2506: PrID %d Blood Duration %d too long - sending 999.",
									drug_line_record.v_prescription_id, drug_line_record.blood_duration	);
					drug_line_record.blood_duration = 999;
				}

				ret_code = pass_a_single_record_2506 (&drug_line_record);

				if (ret_code != success)
				{
					// DonR 17Nov2016: If a single row was over-length, we want to skip past it and continue
					// sending good rows. We intentionally keep the bad row marked as not sent - thus the bad
					// row will continue to generate log messages until someone corrects the data.
					if (ret_code == skip_row)
					{
						SetAsReported = NOT_REPORTED_TO_AS400;
					}
					else
					{
						GerrLogReturn (	GerrId,
										"Pass_single_rec_2506 returned %d for PrID %d Line %d.",
										ret_code, drug_line_record.v_prescription_id, drug_line_record.v_line_no);

						retval = ret_code;
						break;
					}
				}
				else
				{	// Successful upload!
					SetAsReported = REPORTED_TO_AS400;
					n_uncommited_recs++;
					trailer_recs_to_commit++;
				}
			}
			else	// This is a fictitious sale - so don't send to AS/400.
			{
				SetAsReported = NO_NEED_TO_REPORT_TO_AS400;
			}

			ExecSQL (	MAIN_DB, AS400CL_2506_UPD_prescription_drugs, &SetAsReported, END_OF_ARG_LIST	);

			if (SQLERR_code_cmp (SQLERR_access_conflict) == MAC_TRUE)
			{
				retval = SB_DATA_ACCESS_CONFLICT;
				break;
			}

			if (SQLERR_error_test () == MAC_TRUE)
			{
				retval = SB_LOCAL_DB_ERROR;
				break;
			}

			if (n_uncommited_recs == BATCH_DIM)
				break;

		}	// while (!PD_cursor_finished)

		CloseCursor (	MAIN_DB, AS400CL_2506_prescription_drugs_cur	);


		// 3) If we're done with prescription_drugs, open the prescription_msgs cursor.
		if (PD_cursor_finished)
		{
			// Declare cursor for table PRESCRIPTION_MSGS
			DeclareAndOpenCursorInto (	MAIN_DB, AS400CL_2522_prescription_msgs_cur,
										&pr_message_record.v_prescription_id,	&pr_message_record.v_largo_code,
										&pr_message_record.v_line_no,			&pr_message_record.v_error_code,
										&pr_message_record.v_severity_level,
										&NOT_REPORTED_TO_AS400,					&TooRecently,
										&Today,									END_OF_ARG_LIST						);

			if (SQLCODE != 0)
			{
				if (SQLERR_error_test ())
				{
					retval = SB_DATA_ACCESS_CONFLICT;
					break;
				}
			}
		}	// Prescription_drugs cursor is finished, so we need to open the prescription_msgs cursor.

		// If we haven't finished sending all the selected prescriptions rows AND all the
		// selected prescription_drugs rows, don't send any prescription_msgs rows - we'll
		// get to them once we've finished with prescriptions AND prescription_drugs.
		// In fact, the only circumstance in which we *do* want to send prescription_msgs
		// rows is when the first two loops have terminated happily because of end-of-fetch -
		// so we don't need to test any other variables here.
		while ((PR_cursor_finished) && (PD_cursor_finished) && (!PM_cursor_finished))
		{
			// Fetch prescription_msgs row.
			FetchCursor (	MAIN_DB, AS400CL_2522_prescription_msgs_cur	);

			if (SQLERR_code_cmp (SQLERR_end_of_fetch) == MAC_TRUE)
			{
				// If we get here, we've come to the end of the first three cursors.
				PM_cursor_finished = 1;
				break;
			}

			if (SQLERR_code_cmp (SQLERR_access_conflict) == MAC_TRUE)
			{
				retval = SB_DATA_ACCESS_CONFLICT;
				break;
			}

			if (SQLERR_error_test () == MAC_TRUE)
			{
				retval = SB_LOCAL_DB_ERROR;
				break;
			}

			// Send prescription_msgs row to server.
			ret_code = pass_a_single_record_2522 (&pr_message_record);
			if (ret_code != success)
			{
				GerrLogReturn (	GerrId,
								"Pass_single_rec_2522 returned %d for PrID %d Line %d.",
								ret_code, pr_message_record.v_prescription_id, pr_message_record.v_line_no);

				retval = ret_code;
				break;
			}
			else
			{
				SetAsReported = REPORTED_TO_AS400;
				n_uncommited_recs++;
				trailer_recs_to_commit++;
			}

			ExecSQL (	MAIN_DB, AS400CL_2522_UPD_prescription_msgs, &SetAsReported, END_OF_ARG_LIST	);

			if (SQLERR_code_cmp (SQLERR_access_conflict) == MAC_TRUE)
			{
				retval = SB_DATA_ACCESS_CONFLICT;
				break;
			}

			if (SQLERR_error_test () == MAC_TRUE)
			{
				retval = SB_LOCAL_DB_ERROR;
				break;
			}

			if (n_uncommited_recs == BATCH_DIM)
				break;

		}	// while (!PM_cursor_finished)

		CloseCursor (	MAIN_DB, AS400CL_2522_prescription_msgs_cur	);


		// 4) If we're done with prescription_msgs, open the pd_rx_link cursor.
		if (PM_cursor_finished)
		{
			// Declare cursor for table PD_RX_LINK
			// DonR 11Dec2024 User Story #373619: Add rx_origin to columns selected.
			DeclareAndOpenCursorInto (	MAIN_DB, AS400CL_2523_pd_rx_link_cur,
										&pd_rx_link_record.prescription_id,		&pd_rx_link_record.line_no,
										&pd_rx_link_record.visit_number,		&pd_rx_link_record.doctor_presc_id,
										&pd_rx_link_record.largo_prescribed,	&pd_rx_link_record.largo_sold,
										&pd_rx_link_record.valid_from_date,		&pd_rx_link_record.prev_unsold_op,
										&pd_rx_link_record.prev_unsold_units,	&pd_rx_link_record.op_sold,

										&pd_rx_link_record.units_sold,			&pd_rx_link_record.sold_drug_op,
										&pd_rx_link_record.sold_drug_units,		&pd_rx_link_record.close_by_rounding,
										&pd_rx_link_record.rx_sold_status,		&pd_rx_link_record.clicks_line_number,
										&pd_rx_link_record.rx_origin,

										&NOT_REPORTED_TO_AS400,					&TooRecently,
										&Today,									END_OF_ARG_LIST							);

			if (SQLCODE != 0)
			{
				if (SQLERR_error_test ())
				{
					retval = SB_DATA_ACCESS_CONFLICT;
					break;
				}
			}
		}	// Prescription_msgs cursor is finished, so we need to open the pd_rx_link cursor.

		// If we haven't finished sending all the selected prescriptions rows AND all the
		// selected prescription_drugs rows, AND all the prescription_msgs rows, don't
		// send any pd_rx_link rows - we'll get to them once we've finished with prescriptions
		// AND prescription_drugs AND prescription_msgs.
		// In fact, the only circumstance in which we *do* want to send pd_rx_link
		// rows is when the first three loops have terminated happily because of end-of-fetch -
		// so we don't need to test any other variables here.
		while ((PR_cursor_finished) && (PD_cursor_finished) && (PM_cursor_finished) && (!RXL_cursor_finished))
		{
			// Fetch pd_rx_link row.
			FetchCursor (	MAIN_DB, AS400CL_2523_pd_rx_link_cur	);

			if (SQLERR_code_cmp (SQLERR_end_of_fetch) == MAC_TRUE)
			{
				// If we get here, we've come to the end of the first four cursors.
				RXL_cursor_finished = 1;
				break;
			}

			if (SQLERR_code_cmp (SQLERR_access_conflict) == MAC_TRUE)
			{
				retval = SB_DATA_ACCESS_CONFLICT;
				break;
			}

			if (SQLERR_error_test () == MAC_TRUE)
			{
				retval = SB_LOCAL_DB_ERROR;
				break;
			}

			// DonR 22Jul2015: Perform a bit of data cleanup to avoid "p'koks" sending data to AS/400.
			if ((pd_rx_link_record.prev_unsold_op < -9999) || (pd_rx_link_record.prev_unsold_op > 99999))
				pd_rx_link_record.prev_unsold_op = 0;

			if ((pd_rx_link_record.prev_unsold_units < -9999) || (pd_rx_link_record.prev_unsold_units > 99999))
				pd_rx_link_record.prev_unsold_units = 0;

			if ((pd_rx_link_record.op_sold < -99999) || (pd_rx_link_record.op_sold > 999999))
				pd_rx_link_record.op_sold = 0;

			if ((pd_rx_link_record.units_sold < -99999) || (pd_rx_link_record.units_sold > 999999))
				pd_rx_link_record.units_sold = 0;

			// DonR 09Apr2018: Added two new columns, sold_drug_op and sold_drug_units. These are
			// the equivalent of op_sold and units_sold, except that they are in terms of the sold
			// drug rather than the prescribed drug (which may have different package sizes). If
			// the new fields are populated, we want to send their values to AS/400:RK9122ACP
			// instead of the old variables.
			if ((pd_rx_link_record.sold_drug_op != 0) || (pd_rx_link_record.sold_drug_units != 0))
			{
				pd_rx_link_record.op_sold		= pd_rx_link_record.sold_drug_op;
				pd_rx_link_record.units_sold	= pd_rx_link_record.sold_drug_units;
			}

			// Send pd_rx_link row to server.
			ret_code = pass_a_single_record_2523 (&pd_rx_link_record);
			if (ret_code != success)
			{
				GerrLogReturn (	GerrId,
								"Pass_single_rec_2523 returned %d for PrID %d Line %d.",
								ret_code, pd_rx_link_record.prescription_id, pd_rx_link_record.line_no);

				retval = ret_code;
				break;
			}
			else
			{
				SetAsReported = REPORTED_TO_AS400;
				n_uncommited_recs++;
				trailer_recs_to_commit++;
			}

			ExecSQL (	MAIN_DB, AS400CL_2523_UPD_pd_rx_link, &SetAsReported, END_OF_ARG_LIST	);

			if (SQLERR_code_cmp (SQLERR_access_conflict) == MAC_TRUE)
			{
				retval = SB_DATA_ACCESS_CONFLICT;
				break;
			}

			if (SQLERR_error_test () == MAC_TRUE)
			{
				retval = SB_LOCAL_DB_ERROR;
				break;
			}

			if (n_uncommited_recs == BATCH_DIM)
				break;

		}	// while (!RXL_cursor_finished)

		CloseCursor (	MAIN_DB, AS400CL_2523_pd_rx_link_cur	);


		// 5) If we're done with pd_rx_link, open the prescription_vouchers cursor.
		if (RXL_cursor_finished)
		{
			// Declare cursor for table PRESCRIPTION_VOUCHERS.
			DeclareAndOpenCursorInto (	MAIN_DB, AS400CL_2524_prescription_vouchers_cur,
										&voucher_record.prescription_id,			&voucher_record.voucher_num,
										&voucher_record.member_id,					&voucher_record.mem_id_extension,
										&voucher_record.voucher_type,				&voucher_record.voucher_discount_given,
										&voucher_record.voucher_amount_remaining,	&voucher_record.original_voucher_amount,
										&voucher_record.sold_date,

										&NOT_REPORTED_TO_AS400,						&TooRecently,
										&Today,										END_OF_ARG_LIST								);

			if (SQLCODE != 0)
			{
				if (SQLERR_error_test ())
				{
					retval = SB_DATA_ACCESS_CONFLICT;
					break;
				}
			}
		}	// Pd_rx_link cursor is finished, so we need to open the prescription_vouchers cursor.

		// If we haven't finished sending all the selected prescriptions rows AND all the
		// selected prescription_drugs rows AND all the prescription_msgs rows AND all the
		// pd_rx_link rows, don't send any prescription_vouchers rows - we'll get to them
		// once we've finished with all the others.
		// In fact, the only circumstance in which we *do* want to send prescription_vouchers
		// rows is when the first four loops have terminated happily because of end-of-fetch -
		// so we don't need to test any other variables here.
		while ((PR_cursor_finished) && (PD_cursor_finished) && (PM_cursor_finished) && (RXL_cursor_finished) && (!PV_cursor_finished))
		{
			// Fetch prescription_vouchers row.
			FetchCursor (	MAIN_DB, AS400CL_2524_prescription_vouchers_cur	);

			if (SQLERR_code_cmp (SQLERR_end_of_fetch) == MAC_TRUE)
			{
				// If we get here, we've come to the end of all five cursors.
				PV_cursor_finished = 1;
				break;
			}

			if (SQLERR_code_cmp (SQLERR_access_conflict) == MAC_TRUE)
			{
				retval = SB_DATA_ACCESS_CONFLICT;
				break;
			}

			if (SQLERR_error_test () == MAC_TRUE)
			{
				retval = SB_LOCAL_DB_ERROR;
				break;
			}

			// Send prescription_vouchers row to server.
			ret_code = pass_a_single_record_2524 (&voucher_record);
			if (ret_code != success)
			{
				GerrLogReturn (	GerrId,
								"Pass_single_rec_2524 returned %d for PrID %d Voucher Number %ld.",
								ret_code, voucher_record.prescription_id, voucher_record.voucher_num);

				retval = ret_code;
				break;
			}
			else
			{
				SetAsReported = REPORTED_TO_AS400;
				n_uncommited_recs++;
				trailer_recs_to_commit++;
			}

			ExecSQL (	MAIN_DB, AS400CL_2524_UPD_prescription_vouchers, &SetAsReported, END_OF_ARG_LIST	);

			if (SQLERR_code_cmp (SQLERR_access_conflict) == MAC_TRUE)
			{
				retval = SB_DATA_ACCESS_CONFLICT;
				break;
			}

			if (SQLERR_error_test () == MAC_TRUE)
			{
				retval = SB_LOCAL_DB_ERROR;
				break;
			}

			if (n_uncommited_recs == BATCH_DIM)
				break;

		}	// while (!PV_cursor_finished)

		CloseCursor (	MAIN_DB, AS400CL_2524_prescription_vouchers_cur	);


		// 6) If we're done with prescription_vouchers, open the drugsale_del_queue cursor.
		if (PV_cursor_finished)
		{
			// Declare cursor for table DRUGSALE_DEL_QUEUE
			DeclareAndOpenCursorInto (	MAIN_DB, AS400CL_del_queue_cur, &DelPrID, &DelPrLargo, END_OF_ARG_LIST	);

			if (SQLCODE != 0)
			{
				if (SQLERR_error_test ())
				{
					retval = SB_DATA_ACCESS_CONFLICT;
					break;
				}
			}
		}	// Prescription_vouchers cursor is finished, so we need to open the drugsale_del_queue cursor.

		// DonR 03Jul2016: After everything has been sent to AS/400, check to see if anything
		// has been written to the Drug Sale Deletion Queue; this typically happens when someone
		// tries to delete a sale at the same time that this program is sending it to AS/400.
		while ((PR_cursor_finished) && (PD_cursor_finished) && (PM_cursor_finished) && (RXL_cursor_finished) && (PV_cursor_finished) && (!DelQ_cursor_finished))
		{
			LastPrIdDeleted	= 0;

			// DonR 14Jun2022: Perform a COMMIT now, to make sure we don't lose any
			// valid work if we need to perform a ROLLBACK while we process the
			// deleted-sale update queue. (Remember that in this program, all
			// cursors are COMMIT-proof - so extra COMMITs won't disrupt anything.)
			CommitAllWork ();

			while (1)
			{
				// Fetch drugsale_del_queue row.
				FetchCursor (	MAIN_DB, AS400CL_del_queue_cur	);

				if (SQLERR_code_cmp (SQLERR_end_of_fetch) == MAC_TRUE)
				{
					// If we get here, we've come to the end of all five cursors.
					// Perform a COMMIT just to make sure we don't leave any work
					// unfinished.
					CommitAllWork ();
					DelQ_cursor_finished = 1;
					break;
				}

				if (SQLERR_code_cmp (SQLERR_access_conflict) == MAC_TRUE)
				{
					RollbackAllWork ();	// Any pending work is incomplete and needs to be undone.
					retval = SB_DATA_ACCESS_CONFLICT;
					break;
				}

				if (SQLERR_error_test ())
				{
					RollbackAllWork ();	// Any pending work is incomplete and needs to be undone.
					retval = SB_LOCAL_DB_ERROR;
					break;
				}
if (DelPrID != LastPrIdDeleted)
GerrLogMini (GerrId, "\nGot Del PrID %d Largo %d from drugsale_del_queue, SQLCODE = %d.", DelPrID, DelPrLargo, SQLCODE);


				// If any of the UPDATE commands fails, we don't want to perform further
				// UPDATEs - just drop through to the error-handling code.

				// If the row retrieved is for a new Prescription ID, try marking the previous
				// prescriptions rows as "deleted". (Note that this UPDATE should normally be
				// redundant - we're not really worried about performance here, since this is
				// a very low-volume operation.)
				if ((DelPrID != LastPrIdDeleted) && (LastPrIdDeleted > 0))
				{
					ExecSQL (	MAIN_DB, AS400CL_UPD_del_queue_prescriptions,
								&LastPrIdDeleted, &LastPrIdDeleted, END_OF_ARG_LIST	);
GerrLogMini (GerrId, "DelQueue: Updated previous PrID %d, SQLCODE = %d, %d rows.", LastPrIdDeleted, SQLCODE, sqlca.sqlerrd[2]);

					if (SQLCODE == 0)
					{
						// We need to perform a COMMIT for the UPDATEs associated with the previous prescription_id.
						CommitAllWork ();
					}
					else
					{
						RollbackAllWork ();	// Any pending work is incomplete and needs to be undone.
						break;
					}
				}

				// Remember the current Deleted Prescription ID.
				LastPrIdDeleted = DelPrID;

				// If what we read from drugsale_del_queue is for a specific drug line,
				// mark it as deleted.
				if ((SQLCODE == 0) && (DelPrLargo != 0))
				{
					ExecSQL (	MAIN_DB, AS400CL_UPD_del_queue_prescription_drugs,
								&DelPrID, &DelPrLargo, END_OF_ARG_LIST				);
GerrLogMini (GerrId, "DelQueue: Updated PrID %d  Largo %5d , SQLCODE = %d, %d rows.", DelPrID, DelPrLargo, SQLCODE, sqlca.sqlerrd[2]);

					// DonR 01Aug2023 User Story #469361: Added del_flg to pd_rx_link, so we know which rows
					// are "dead" without having to cross-reference prescription_drugs.
					if (SQLCODE == 0)
					{
						ExecSQL (	MAIN_DB, AS400CL_UPD_del_queue_pd_rx_link,
									&DelPrID, &DelPrLargo, END_OF_ARG_LIST		);
GerrLogMini (GerrId, "DelQueue: Updated pd_rx_link for PrID %d  Largo %5d , SQLCODE = %d, %d rows.", DelPrID, DelPrLargo, SQLCODE, sqlca.sqlerrd[2]);
					}
				}

				// Whether what we read from drugsale_del_queue is for a specific drug line
				// or not, try deleting the "header" prescriptions row. Remember that we're
				// not super-worried about efficience here - the important thing is to make
				// sure that everything that needs to get done gets done.
				if (SQLCODE == 0)
				{
					ExecSQL (	MAIN_DB, AS400CL_UPD_del_queue_prescriptions,
								&DelPrID, &DelPrID, END_OF_ARG_LIST				);
GerrLogMini (GerrId, "DelQueue: Updated PrID %d, SQLCODE = %d, %d rows.", DelPrID, SQLCODE, sqlca.sqlerrd[2]);
				}

				// Note that if SQLCODE == 0, we delete from drugsale_del_queue - but we do
				// NOT want to perform a COMMIT until we get to the next prescription_id!
				if (SQLCODE == 0)
				{
					ExecSQL (	MAIN_DB, AS400CL_DEL_del_queue	);
				}
				else
				{
					// We hit some kind of error on one of the UPDATE commands - roll back
					// whatever work we've done and break out of the FETCH loop.
					if (SQLERR_code_cmp (SQLERR_access_conflict) == MAC_TRUE)
					{
						retval = SB_DATA_ACCESS_CONFLICT;
					}
					else
					{
						if (SQLERR_error_test ())
						{
							retval = SB_LOCAL_DB_ERROR;
						}
					}

					RollbackAllWork ();	// Any pending work is incomplete and needs to be undone.
					break;
				}

			}	// While (1)

			CloseCursor (	MAIN_DB, AS400CL_del_queue_cur	);

		}	// while (!DelQ_cursor_finished)


		// 7) If we're done with drugsale_del_queue, open the pd_cannabis_products cursor. Marianna 14Feb2024 User Story #540234
		if (DelQ_cursor_finished)
		{
			// Declare cursor for table pd_cannabis_products.
			DeclareAndOpenCursorInto	(	MAIN_DB, AS400CL_2525_pd_cannabis_products_cur,
											&pd_cannabis_record.prescription_id,		&pd_cannabis_record.line_no,
											&pd_cannabis_record.cannabis_product_code,	&pd_cannabis_record.cannabis_product_barcode,
											&pd_cannabis_record.group_largo_code,		&pd_cannabis_record.op,
											&pd_cannabis_record.units,					&pd_cannabis_record.price_per_op,
											&pd_cannabis_record.product_sale_amount,	&pd_cannabis_record.product_ptn_amount,
											&NOT_REPORTED_TO_AS400,						&TooRecently,
											&Today,										END_OF_ARG_LIST									);

			if (SQLCODE != 0)
			{
				if (SQLERR_error_test())
				{
					retval = SB_DATA_ACCESS_CONFLICT;
					break;
				}
			}
		}	// drugsale_del_queue cursor is finished, so we need to open the pd_cannabis_products cursor.


		// Marianna 14Feb2024 User Story #540234
		// In fact, the only circumstance in which we *do* want to send Pd_cannabis_products
		// rows is when the first 6 loops have terminated happily because of end-of-fetch -
		// so we don't need to test any other variables here.
		while (	( PR_cursor_finished)	&&
				( PD_cursor_finished)	&&
				( PM_cursor_finished)	&&
				( RXL_cursor_finished)	&&
				( PV_cursor_finished)	&&
				( DelQ_cursor_finished)	&&
				(!PDCP_cursor_finished)	)
		{
			// Fetch pd_cannabis_products row.
			FetchCursor(MAIN_DB, AS400CL_2525_pd_cannabis_products_cur);

			if (SQLERR_code_cmp(SQLERR_end_of_fetch) == MAC_TRUE)
			{
				// If we get here, we've come to the end of all five cursors.
				PDCP_cursor_finished = 1;
				break;
			}

			if (SQLERR_code_cmp(SQLERR_access_conflict) == MAC_TRUE)
			{
				retval = SB_DATA_ACCESS_CONFLICT;
				break;
			}

			if (SQLERR_error_test() == MAC_TRUE)
			{
				retval = SB_LOCAL_DB_ERROR;
				break;
			}

			// Send pd_cannabis_products row to server.
			ret_code = pass_a_single_record_2525 (&pd_cannabis_record);
			if (ret_code != success)
			{
				GerrLogReturn(GerrId,
					"Pass_single_rec_2525 returned %d for PrID %d line_no %d cannabis_product_code %d cannabis_product_barcode %ld.",
					ret_code, pd_cannabis_record.prescription_id, pd_cannabis_record.line_no,
					pd_cannabis_record.cannabis_product_code, pd_cannabis_record.cannabis_product_barcode);

				retval = ret_code;
				break;
			}
			else
			{
				SetAsReported = REPORTED_TO_AS400;
				n_uncommited_recs++;
				trailer_recs_to_commit++;
			}

			ExecSQL(MAIN_DB, AS400CL_2525_UPD_pd_cannabis_products, &SetAsReported, END_OF_ARG_LIST);

			if (SQLERR_code_cmp(SQLERR_access_conflict) == MAC_TRUE)
			{
				retval = SB_DATA_ACCESS_CONFLICT;
				break;
			}

			if (SQLERR_error_test() == MAC_TRUE)
			{
				retval = SB_LOCAL_DB_ERROR;
				break;
			}

			if (n_uncommited_recs == BATCH_DIM)
				break;

		}	// while (!PDCP_cursor_finished)

		CloseCursor (MAIN_DB, AS400CL_2525_pd_cannabis_products_cur);



		// Done reading through cursors - either we've hit the end of both, or we've maxed out on
		// the total number of rows to be sent before a COMMIT, or else we've hit an error. If
		// all has gone smoothly so far, retval will still have its initial value.
		// There are three "happy" return codes for this function:
		//
		// 1) SB_END_OF_FETCH_NOCOMMIT		if nothing was found to send to AS/400;
		// 2) SB_END_OF_FETCH				if we're done with BOTH cursors.
		// 3) SB_BATCH_SUCCESSFULLY_PASSED	if we've sent BATCH_DIM rows and it's time for an interim COMMIT.
		if (retval == SB_END_OF_FETCH_NOCOMMIT)
		{
			if (n_uncommited_recs)
				retval = (n_uncommited_recs == BATCH_DIM) ? SB_BATCH_SUCCESSFULLY_PASSED : SB_END_OF_FETCH;
		}
		else	// Something went wrong!
		{
			// In fact, there's nothing we need to do here - we'll just fall through
			// and return the error code.
		}
	}
	while (0);	// End of one-pass do-loop.


	// If we're finished, close cursors. No error-handling is really needed here.
	// Note that normally these should all be closed already by the time we get here;
	// but just in case, it's harmless to issue an extra CloseCursor command.
	CloseCursor (	MAIN_DB, AS400CL_2506_prescription_drugs_cur	);
	CloseCursor (	MAIN_DB, AS400CL_2502_prescriptions_cur			);
	CloseCursor (	MAIN_DB, AS400CL_2522_prescription_msgs_cur		);
	CloseCursor (	MAIN_DB, AS400CL_2523_pd_rx_link_cur			);
	CloseCursor (	MAIN_DB, AS400CL_2524_prescription_vouchers_cur	);


	RESTORE_ISOLATION;

    POP_RET (retval);

} /* end of handler 2502 + 2506 + 2522 + 2523 + 2524 */



/*=======================================================================
||																		||
||			Message 2504 --	stock										||
||																		||
 =======================================================================*/
static int pass_record_batch_2504 (void)
{
	// Local variables.
	int				BATCH_DIM;
	int				server_msg;
	int				n_uncommited_recs;
	int				rc;
	int				i;
	int				n_data;		/* length of data to be sent to server */
	bool			exit_flg;
	TSuccessStat	retval = SB_END_OF_FETCH_NOCOMMIT;
	static int		cursor_flg = 1;

	// Sql variables.
		Tpharmacy_code		v_pharmacy_code;
		Tinstitued_code		v_institued_code;
		Tterminal_id		v_terminal_id;
		Tuser_ident			v_user_ident;
		Taction_type		v_action_type;
		Tserial_no			v_serial_no;
		Tdate				v_date;
		Ttime				v_time;
		Tsupplier_for_stock	v_supplier_for_stock;
		Tinvoice_no			v_invoice_no;
		Tdiscount_sum		v_discount_sum;
		Tnum_of_lines_4		v_num_of_lines_4;
		Tdiary_monthNew		v_diary_month;
		Terror_code			v_error_code;
		Terror_code			v_comm_err_code;
		Tdate				v_update_date;
		Ttime				v_update_time;
		static int			MaxRecCount;
		TVatAmount			v_vat_amount;
		TBilAmount			v_bil_amount;
		Tdate				v_suppl_bill_date;
		TSupplSendNum		v_suppl_sendnum;
		TBilAmountWithVat	v_bil_amountwithvat;
		TBilAmountBeforeVat	v_bil_amount_beforevat ;
		TBilConstr			v_bill_constr;
		TBilInvDiff			v_bill_invdiff;
		int					v_r_count;	// DonR 18SEP2002
		int					SysDate;	// DonR 30Nov2003: Retrieve only previous-day (and earlier) rows.


	pushname ("pass_record_batch_2504");

	// Initialize variables.
	BATCH_DIM = get_dimension_of_batch( 2504 );
	exit_flg  = false;				/* if true - exit function  */
	n_uncommited_recs = 0;			/* num of records sent to   */
									/* remote DB without commit */
	SysDate = GetDate ();


	if (cursor_flg)
	{
		do
		{
			DeclareCursor (	MAIN_DB, AS400CL_2504_stock_cur,
							&NOT_REPORTED_TO_AS400,		&MinPharmacyToAs400,
							&ROW_NOT_DELETED,			&MaxRecCount,
							&SysDate,					END_OF_ARG_LIST			);

			// Go few rows back in order to not lock key value in index of table
			ExecSQL (	MAIN_DB, AS400CL_2504_READ_max_stock_r_count,
						&MaxRecCount, END_OF_ARG_LIST					);

			if (SQLERR_error_test ())
			{
				retval = SB_DATA_ACCESS_CONFLICT;
				break;
			}


			// DonR 03Mar2005: Maccabi pharmacies now use SAP instead of this table - so
			// we need to eliminate most of this safety margin, since volumes from private
			// pharmacies are much lower.
//			MaxRecCount -= 10;		// DonR 05Apr2005: Eliminated safety margin, since (with SAP
//									// in place) volume has dwindled to almost nothing.
//			MaxRecCount -= 2000;	// DonR 30Nov2003: Increased safety margin from 800 to 2000 rows.

			//GerrLogReturn (GerrId, "Max record num %d", MaxRecCount);


			// Now open table...safely
			OpenCursor (	MAIN_DB, AS400CL_2504_stock_cur	);

			if (SQLERR_error_test ())
			{
				retval = SB_DATA_ACCESS_CONFLICT;
				break;
			}

		}
		while (0);

	}	// cursor_flg non-zero.


	// Loop on stock table lines.
	if (retval != SB_DATA_ACCESS_CONFLICT)
	{
		do
		{
			cursor_flg = 0;

			while (1)
			{
				FetchCursorInto (	MAIN_DB, AS400CL_2504_stock_cur,
									&v_pharmacy_code,		&v_institued_code,			&v_terminal_id,
									&v_user_ident,			&v_action_type,				&v_serial_no,
									&v_date,				&v_time,					&v_supplier_for_stock,
									&v_invoice_no,			&v_discount_sum,			&v_num_of_lines_4,
									&v_diary_month,			&v_error_code,				&v_comm_err_code,
									&v_update_date,			&v_update_time,				&v_vat_amount,
									&v_bil_amount,			&v_suppl_bill_date,			&v_suppl_sendnum,
									&v_bil_amountwithvat,	&v_bil_amount_beforevat,	&v_bill_constr,
									&v_bill_invdiff,		&v_r_count,					END_OF_ARG_LIST			);
//GerrLogMini (GerrId, "Fetch AS400CL_2504_stock_cur: v_r_count = %d, SQLCODE = %d.", v_r_count, SQLCODE);

				// 28.06.98 - Close cursor if we are close to recent stock entries, to
				// help to prevent locks.
				if ((diff_from_now (v_update_date, v_update_time) < TRANSFER_INTERVAL)	||
					(SQLERR_code_cmp (SQLERR_end_of_fetch) == MAC_TRUE))
				{
					cursor_flg = 1;
					retval = n_uncommited_recs ? SB_END_OF_FETCH : SB_END_OF_FETCH_NOCOMMIT;
					break;
				}
				else
				{
					if (SQLERR_code_cmp (SQLERR_access_conflict) == MAC_TRUE)
					{
						retval = SB_DATA_ACCESS_CONFLICT;
						break;
					}
					else
					{
						if (SQLERR_error_test () != MAC_OK)
						{
							retval = SB_LOCAL_DB_ERROR;
							break;
						}
					}	// Not access conflict.
				}	// Not done with fetch.


				// Move record into buffer.
				n_data = 0;

				n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	MESSAGE_ID_LEN,				2504					);

				n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	Tpharmacy_code_len,			v_pharmacy_code			);
				n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	Tinstitued_code_len,		v_institued_code		);
				n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	Tterminal_id_len,			v_terminal_id			);
				n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	Tuser_ident_len,			v_user_ident			);
				n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	Taction_type_len,			v_action_type			);
				n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	Tserial_no_len,				v_serial_no				);
				n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	Tdate_len,					v_date					);
				n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	Ttime_len,					v_time					);
				n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	Tsupplier_for_stock_len,	v_supplier_for_stock	);
				n_data += sprintf (glbMsgBuffer + n_data, "%-*.*s",	Tinvoice_no_len,
																	Tinvoice_no_len,			v_invoice_no			);
				n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	Tdiscount_sum_len,			v_discount_sum			);
				n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	Tnum_of_lines_4_len,		v_num_of_lines_4		);
				n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	Tdiary_new_month_len,		v_diary_month			);
				n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	Terror_code_len,			v_error_code			);
				n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	Terror_code_len,			v_comm_err_code			);
				n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	TVatAmount_len,				v_vat_amount			);
				n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	TBilAmount_len,				v_bil_amount			);
				n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	TDate_len,					v_suppl_bill_date		);
				n_data += sprintf (glbMsgBuffer + n_data, "%-*.*s",	TSupplSendNum_len,
																	TSupplSendNum_len,			v_suppl_sendnum			);
				n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	TBilAmountWithVat_len,		v_bil_amountwithvat		);
				n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	TBilAmountBeforeVat_len,	v_bil_amount_beforevat	);
				n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	TBilConstr_len,				v_bill_constr			);
				n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	TBilInvDiff_len,			v_bill_invdiff			);

				//GerrLogReturn (GerrId,
				//				 "Invoice_num [%s][%d][%d][%d][%d]",
				//				 v_invoice_no,Tinvoice_no_len,v_pharmacy_code,v_date,v_time);

				//GerrLogReturn(GerrId, "Max record num %s", glbMsgBuffer);

				StaticAssert (n_data == RECORD_LEN_2504 + MESSAGE_ID_LEN);


				// Send buffer to server.
				if (send_buffer_to_server (glbSocket, glbMsgBuffer, n_data) != success)
				{
					retval = CM_RESTART_CONNECTION;
					cursor_flg = 1;
					break;
				}
				else need_rollback = 1;


				// Get message from server.
				exit_flg = false;

				usleep( 5000  );
				server_msg = get_msg_from_server (NO_WAIT_MODE);

				switch (server_msg)
				{
					case CM_SYSTEM_IS_NOT_BUSY:
					case CM_NO_MESSAGE_PASSED:	// No message = no problem (we hope).
						//nap (100);
						break;

					default:
						retval = server_msg;
						exit_flg = true;
				}

				if (exit_flg == true)
				{
					break;
				}


				// Mark row as "reported to AS400".
				ExecSQL (	MAIN_DB, AS400CL_2504_UPD_stock,
							&REPORTED_TO_AS400, END_OF_ARG_LIST	);
//GerrLogMini (GerrId, "AS400CL_2504_UPD_stock: SQLCODE = %d.", SQLCODE);

				if (SQLERR_error_test () != MAC_OK)
				{
					retval = SB_LOCAL_DB_ERROR;
					break;
				}

				// Stop on full batch.
				if (++n_uncommited_recs == BATCH_DIM)
				{
					retval = SB_BATCH_SUCCESSFULLY_PASSED;
					break;
				}

			}	// while (1)

		}	// Dummy do loop.
		while (0);

	}	// Table opened without access conflict.


	if (cursor_flg)
	{
		CloseCursor (	MAIN_DB, AS400CL_2504_stock_cur	);
	}

	recs_to_commit = n_uncommited_recs;

	POP_RET (retval);

} /* end of handler 2504 */



/*=======================================================================
||																		||
||			Message 2505 --	money_empty									||
||																		||
 =======================================================================*/
static int pass_record_batch_2505(
				  void
				  )
{

/*=======================================================================
||									||
||			Local variables					||
||									||
 =======================================================================*/

    int		BATCH_DIM;
    TSuccessStat	retval = SB_END_OF_FETCH_NOCOMMIT;
    int		server_msg;
    int		n_uncommited_recs;
    int		rc;
    bool		exit_flg;
    int		i;
    int		n_data;		/* length of data to be sent to server */
    static int	cursor_flg = 1;

/*=======================================================================
||									||
||			Sql variables					||
||									||
 =======================================================================*/


    int				v_pharmacy_code;
    short			v_institued_code;
    short			v_terminal_id;
    int				v_date;
    int				v_time;
    int				v_receipt_num;
    short			v_serial_no_2;
    int				v_serial_no_3;
    int				v_update_date;
    int				v_update_time;
    short			v_num_of_lines_4;
    short			v_diary_month;
    short			v_action_type;
    short		    v_comm_error_code;
    short			v_error_code;
    int				MaxRecCount;
	int				v_r_count;	// DonR 18SEP2002


	pushname ("pass_record_batch_2505");

/*=======================================================================
||									||
||			Initialize variables				||
||									||
 =======================================================================*/

    BATCH_DIM = get_dimension_of_batch( 2505 );
    exit_flg  = false;				/* if true - exit function  */
    n_uncommited_recs = 0;			/* num of records sent to   */
						/* remote DB without commit */
/*=======================================================================
||									||
||			Declare Cursor					||
||									||
 =======================================================================*/

    if( cursor_flg )
    do
    {
		DeclareCursor (	MAIN_DB, AS400CL_2505_money_empty_cur,
						&NOT_REPORTED_TO_AS400,		&MinPharmacyToAs400,
						&MaxRecCount,				END_OF_ARG_LIST			);

	//
	// Go few rows back in order to
	//  not lock key value in index of table
	//
		ExecSQL (	MAIN_DB, AS400CL_2505_READ_money_empty_max_r_count,
					&MaxRecCount, END_OF_ARG_LIST						);

		if( SQLERR_error_test() )
		{
			retval = SB_DATA_ACCESS_CONFLICT;
			break;
		}

		// DonR 17Jul2008: As this is a fairly low-volume table, reduce "safety margin" from 50 to 5.
		// DonR 01May2017: Don't include a "safety margin" for test systems.
		if (Production)
			MaxRecCount -= 5;
		//
		// Now open table...safely
		//
		OpenCursor (	MAIN_DB, AS400CL_2505_money_empty_cur	);

	if( SQLERR_error_test() )
	{
	    retval = SB_DATA_ACCESS_CONFLICT;
	    break;
	}

    }
    while( 0 );

/*=======================================================================
||									||
||			Loop on money_empty table lines			||
||									||
 =======================================================================*/

    if( retval != SB_DATA_ACCESS_CONFLICT )
    do
    {
	cursor_flg = 0;

	while( 1 )
	{
		FetchCursorInto (	MAIN_DB, AS400CL_2505_money_empty_cur,
							&v_pharmacy_code,		&v_institued_code,		&v_terminal_id,
							&v_action_type,			&v_date,				&v_time,
							&v_receipt_num,			&v_serial_no_2,			&v_serial_no_3,
							&v_diary_month,			&v_update_date,			&v_update_time,
							&v_num_of_lines_4,		&v_comm_error_code,		&v_error_code,
							&v_r_count,				END_OF_ARG_LIST								);

	    if( SQLERR_code_cmp( SQLERR_end_of_fetch ) == MAC_TRUE )
	    {
		cursor_flg = 1;

		retval = n_uncommited_recs ? SB_END_OF_FETCH :
					     SB_END_OF_FETCH_NOCOMMIT;
		break;
	    }
	    else if( SQLERR_code_cmp( SQLERR_access_conflict ) == MAC_TRUE )
	    {
		retval = SB_DATA_ACCESS_CONFLICT;
		break;
	    }
	    else if( SQLERR_error_test() != MAC_OK )
	    {
		retval = SB_LOCAL_DB_ERROR;
		break;
	    }

/*=======================================================================
||									||
||			Move record into buffer				||
||									||
 =======================================================================*/

	    n_data = 0;

	    n_data += sprintf(		/* -> message header */
			      glbMsgBuffer + n_data,
			      "%0*d",
			      MESSAGE_ID_LEN,
			      2505
			      );

	    n_data += sprintf(
			      glbMsgBuffer + n_data,
			      "%0*d",
			      Tpharmacy_code_len,
			      v_pharmacy_code
			      );

	    n_data += sprintf(
			      glbMsgBuffer + n_data,
			      "%0*d",
			      Tinstitued_code_len,
			      v_institued_code
			      );

	    n_data += sprintf(
			      glbMsgBuffer + n_data,
			      "%0*d",
			      Tterminal_id_len,
			      v_terminal_id
			      );

	    n_data += sprintf(
			      glbMsgBuffer + n_data,
			      "%0*d",
			      Taction_type_len,
			      v_action_type
			      );

	    n_data += sprintf(
			      glbMsgBuffer + n_data,
			      "%0*d",
			      Tdate_len,
			      v_date
			      );

	    n_data += sprintf(
			      glbMsgBuffer + n_data,
			      "%0*d",
			      Ttime_len,
			      v_time
			      );

	    n_data += sprintf(
			      glbMsgBuffer + n_data,
			      "%0*d",
			      Treceipt_num_len,
			      v_receipt_num
			      );

	    n_data += sprintf(
			      glbMsgBuffer + n_data,
			      "%0*d",
			      Tserial_no_2_len,
			      v_serial_no_2
			      );

	    n_data += sprintf(
			      glbMsgBuffer + n_data,
			      "%0*d",
			      Tserial_no_3_len,
			      v_serial_no_3
			      );

	    n_data += sprintf(
			      glbMsgBuffer + n_data,
			      "%0*d",
			      Tdiary_month_len,
			      v_diary_month
			      );

	    n_data += sprintf(
			      glbMsgBuffer + n_data,
			      "%0*d",
			      Tupdate_date_len,
			      v_update_date
			      );

	    n_data += sprintf(
			      glbMsgBuffer + n_data,
			      "%0*d",
			      Tupdate_time_len,
			      v_update_time
			      );

	    n_data += sprintf(
			      glbMsgBuffer + n_data,
			      "%0*d",
			      Tnum_of_lines_4_len,
			      v_num_of_lines_4
			      );

	    n_data += sprintf(
			      glbMsgBuffer + n_data,
			      "%0*d",
			      Tcomm_error_code_len,
			      v_comm_error_code
			      );

	    n_data += sprintf(
			      glbMsgBuffer + n_data,
			      "%0*d",
			      Terror_code_len,
			      v_error_code
			      );


	    StaticAssert( n_data == RECORD_LEN_2505 + MESSAGE_ID_LEN );

/*=======================================================================
||									||
||			Send buffer to server				||
||									||
 =======================================================================*/

	    if( send_buffer_to_server( glbSocket,glbMsgBuffer, n_data )
		!= success )
	    {
		retval = CM_RESTART_CONNECTION;
		cursor_flg = 1;
		break;
	    }
		else need_rollback = 1;


/*=======================================================================
||									||
||			Delete record logically				||
||									||
 =======================================================================*/

		ExecSQL (	MAIN_DB, AS400CL_2505_UPD_money_empty,
					&REPORTED_TO_AS400, END_OF_ARG_LIST	);

	    if( SQLERR_error_test() != MAC_OK )
	    {
		retval = SB_LOCAL_DB_ERROR;
		break;
	    }

//	    printf("send money_empty line ---------%d---------\n",
//		    v_receipt_num ); fflush(stdout);

    /* << -------------------------------------------------------- >> */
    /*	        Stop on full batch of prescriptions		      */
    /* << -------------------------------------------------------- >> */

	    if( ++n_uncommited_recs == BATCH_DIM )
	    {
		retval = SB_BATCH_SUCCESSFULLY_PASSED;
		break;
	    }

/*=======================================================================
||									||
||			Get message from server				||
||									||
 =======================================================================*/

	    exit_flg = false;

	    server_msg = get_msg_from_server( NO_WAIT_MODE );
	    switch( server_msg )
	    {
		case CM_SYSTEM_IS_NOT_BUSY:
		case CM_NO_MESSAGE_PASSED:/* NONMESSAGE */

		    usleep( 5000  );

		    break;

		default:

		    retval = server_msg;
		    exit_flg = true;

	    }


	    if( exit_flg == true )
	    {
		break;
	    }

	}	// while( 1 )

    }
    while( 0 );

    if( cursor_flg )
    {
		CloseCursor (	MAIN_DB, AS400CL_2505_money_empty_cur	);
    }

	recs_to_commit = n_uncommited_recs;

    POP_RET (retval);

} /* end of handler 2505 */



/*=======================================================================
||																		||
||			Message 2507 --	presc_drug_inter							||
||																		||
 =======================================================================*/
static int pass_record_batch_2507(
				  void
				  )
{

/*=======================================================================
||									||
||			Local variables					||
||									||
 =======================================================================*/

    int		   BATCH_DIM;
    TSuccessStat   retval = SB_END_OF_FETCH_NOCOMMIT;
    int		   server_msg;
    int		   n_uncommited_recs;
    int		   rc;
    bool	   exit_flg;
    int		   i;
    int		   n_data;     /* length of data to be sent to server */
//	static int		cursor_flg = 1;	// RESTORE THIS & RETEST WHEN WE MOVE TO MS SQL SERVER!
	int				cursor_flg = 1;	// INFORMIX ODBC CLOSES CURSORS WITH EVERY "COMMIT", SO WE ALWAYS HAVE TO RE-OPEN!

/*=======================================================================
||									||
||			Sql variables					||
||									||
 =======================================================================*/


    Tpharmacy_code	   v_pharmacy_code;
    Tinstitued_code	   v_institued_code;
    Tterminal_id	   v_terminal_id;
    Tspecial_presc_num	   v_special_presc_num;
    Tspec_pres_num_sors	   v_spec_pres_num_sors;
    Tprescription_id	   v_prescription_id;
    Tline_no		   v_line_no;
    Tlargo_code            v_largo_code;
    Tlargo_code_inter      v_largo_code_inter;
    Tinteraction_type      v_interaction_type;
    Tdate                  v_date;
    Ttime		   v_time;
    Tdel_flg               v_del_flg;
    Tpresc_id_inter	   v_presc_id_inter;
    Tline_no_inter	   v_line_no_inter;
    Tmember_id	           v_member_id;
    Tmem_id_extension      v_mem_id_extension;
    Tdoctor_id             v_doctor_id;
    Tdoctor_id_type	   v_doctor_id_type;
    Tsec_doctor_id         v_sec_doctor_id;
    Tsec_doctor_id_type    v_sec_doctor_id_type;
    Terror_code		   v_error_code;
    Tduration              v_duration;
    Top                    v_op;
    Tunits                 v_units;
    Tstop_blood_date       v_stop_blood_date;
    Tcheck_type            v_check_type;
    Tdestination           v_destination;
    Tprint_flg             v_print_flg;
    Tdate                  v_sec_pres_date;


	pushname ("pass_record_batch_2507");

/*=======================================================================
||									||
||			Initialize variables				||
||									||
 =======================================================================*/

    BATCH_DIM = get_dimension_of_batch( 2507 );
    exit_flg  = false;				/* if true - exit function  */
    n_uncommited_recs = 0;			/* num of records sent to   */
						/* remote DB without commit */

    if (cursor_flg)
    do
    {
		CloseCursor		(	MAIN_DB, AS400CL_2507_presc_drug_inter_cur	);
		FreeStatement	(	MAIN_DB, AS400CL_2507_presc_drug_inter_cur	);
		DeclareCursor	(	MAIN_DB, AS400CL_2507_presc_drug_inter_cur,
							&NOT_REPORTED_TO_AS400, &MinPharmacyToAs400, END_OF_ARG_LIST	);

		OpenCursor (	MAIN_DB, AS400CL_2507_presc_drug_inter_cur	);


	if (SQLERR_code_cmp (SQLERR_access_conflict) == MAC_TRUE)
	{
		retval = SB_DATA_ACCESS_CONFLICT;
		break;
	}
	else if (SQLERR_error_test () != MAC_OK)
	{
		retval = SB_LOCAL_DB_ERROR;
		break;
	}


    }
    while( 0 );

/*=======================================================================
||									||
||			Loop on presc_drug_inter table lines		||
||									||
 =======================================================================*/

    if ((retval != SB_DATA_ACCESS_CONFLICT) && (retval != SB_LOCAL_DB_ERROR))
    do
    {
	cursor_flg = 0;

	while( 1 )
	{
		FetchCursorInto (	MAIN_DB, AS400CL_2507_presc_drug_inter_cur,
							&v_pharmacy_code,		&v_institued_code,		&v_terminal_id,
							&v_special_presc_num,	&v_spec_pres_num_sors,	&v_prescription_id,
							&v_line_no,				&v_largo_code,			&v_largo_code_inter,
							&v_interaction_type,	&v_date,				&v_time,
							&v_del_flg,				&v_presc_id_inter,		&v_line_no_inter,
							&v_member_id,			&v_mem_id_extension,	&v_doctor_id,
							&v_doctor_id_type,		&v_sec_doctor_id,		&v_sec_doctor_id_type,
							&v_error_code,			&v_duration,			&v_op,
							&v_units,				&v_stop_blood_date,		&v_check_type,
							&v_destination,			&v_print_flg,			&v_sec_pres_date,
							END_OF_ARG_LIST															);

	    /*
	    10.12.1997
	    Close cursor if we are close to recent prescriptions
	    help to prevent locks
	    */
	    if( diff_from_now(
			      v_date,
			      v_time ) < TRANSFER_INTERVAL ||

		SQLERR_code_cmp( SQLERR_end_of_fetch ) == MAC_TRUE )
	    {
		cursor_flg = 1;

		retval = n_uncommited_recs ? SB_END_OF_FETCH :
					     SB_END_OF_FETCH_NOCOMMIT;
		break;
	    }
	    else if( SQLERR_code_cmp( SQLERR_access_conflict ) == MAC_TRUE )
	    {
		retval = SB_DATA_ACCESS_CONFLICT;
		break;
	    }
	    else if( SQLERR_error_test() != MAC_OK )
	    {
		retval = SB_LOCAL_DB_ERROR;
		break;
	    }

/*=======================================================================
||									||
||			Move record into buffer				||
||									||
 =======================================================================*/

	    n_data = 0;

	    n_data += sprintf(		/* -> message header */
			      glbMsgBuffer + n_data,
			      "%0*d",
			      MESSAGE_ID_LEN,
			      2507
			      );

	    n_data += sprintf(
			      glbMsgBuffer + n_data,
			      "%0*d",
			      Tpharmacy_code_len,
			      v_pharmacy_code
			      );

	    n_data += sprintf(
			      glbMsgBuffer + n_data,
			      "%0*d",
			      Tinstitued_code_len,
			      v_institued_code
			      );

	    n_data += sprintf(
			      glbMsgBuffer + n_data,
			      "%0*d",
			      Tterminal_id_len,
			      v_terminal_id
			      );

	    n_data += sprintf(
			      glbMsgBuffer + n_data,
			      "%0*d",
			      Tspecial_presc_num_len,
			      v_special_presc_num
			      );

	    n_data += sprintf(
			      glbMsgBuffer + n_data,
			      "%0*d",
			      Tspec_pres_num_sors_len,
			      v_spec_pres_num_sors
			      );

	    n_data += sprintf(
			      glbMsgBuffer + n_data,
			      "%0*d",
			      Tprescription_id_len,
			      v_prescription_id
			      );

	    n_data += sprintf(
                              glbMsgBuffer + n_data,
							  "%0*d",
                              2,	/* Tline_no_len */
                              v_line_no
			      );

	    n_data += sprintf(
                              glbMsgBuffer + n_data,
                              "%0*d",
                              9,
                              v_largo_code
			      );	//Marianna 14Jul2024 User Story #330877

	    n_data += sprintf(
                              glbMsgBuffer + n_data,
                              "%0*d",
                              9,
                              v_largo_code_inter
			      );	//Marianna 14Jul2024 User Story #330877

	    n_data += sprintf(
	                      glbMsgBuffer + n_data,
                              "%0*d",
                              Tinteraction_type_len,
                              v_interaction_type
			      );

	    n_data += sprintf(
			      glbMsgBuffer + n_data,
			      "%0*d",
			      Tdate_len,
			      v_date
			      );

	    n_data += sprintf(
			      glbMsgBuffer + n_data,
			      "%0*d",
			      Ttime_len,
			      v_time
			      );

	    n_data += sprintf(
			      glbMsgBuffer + n_data,
			      "%0*d",
			      Tdel_flg_len,
			      v_del_flg
			      );

	    n_data += sprintf(
			      glbMsgBuffer + n_data,
			      "%0*d",
			      Tpresc_id_inter_len,
			      v_presc_id_inter
			      );

	    n_data += sprintf(
			      glbMsgBuffer + n_data,
			      "%0*d",
			      2,	/* Tline_no_inter_len */
			      v_line_no_inter
			      );

	    n_data += sprintf(
			      glbMsgBuffer + n_data,
			      "%0*d",
			      Tmember_id_len,
			      v_member_id
			      );

	    n_data += sprintf(
			      glbMsgBuffer + n_data,
			      "%0*d",
			      Tmem_id_extension_len,
			      v_mem_id_extension
			      );

	    n_data += sprintf(
			      glbMsgBuffer + n_data,
			      "%0*d",
			      Tdoctor_id_len,
			      v_doctor_id
			      );

	    n_data += sprintf(
                              glbMsgBuffer + n_data,
                              "%0*d",
                              Tdoctor_id_type_len,
                              v_doctor_id_type
			      );

	    n_data += sprintf(
                              glbMsgBuffer + n_data,
                              "%0*d",
                              Tsec_doctor_id_len,
                              v_sec_doctor_id
			      );

	    n_data += sprintf(
                              glbMsgBuffer + n_data,
                              "%0*d",
                              Tsec_doctor_id_type_len,
                              v_sec_doctor_id_type
			      );

	    n_data += sprintf(
			      glbMsgBuffer + n_data,
			      "%0*d",
			      Terror_code_len,
			      v_error_code
			      );

	    n_data += sprintf(
			      glbMsgBuffer + n_data,
			      "%0*d",
			      Tduration_len,
			      v_duration
			      );

            n_data += sprintf(
                              glbMsgBuffer + n_data,
                              "%0*d",
                              Top_len,
                              v_op
			      );

            n_data += sprintf(
                              glbMsgBuffer + n_data,
                              "%0*d",
                              Tunits_len,
                              v_units
			      );

            n_data += sprintf(
                              glbMsgBuffer + n_data,
                              "%0*d",
                              Tstop_blood_date_len,
                              v_stop_blood_date
			      );

            n_data += sprintf(
                              glbMsgBuffer + n_data,
                              "%0*d",
                              Tcheck_type_len,
                              v_check_type
			      );

            n_data += sprintf(
                              glbMsgBuffer + n_data,
                              "%0*d",
                              Tdestination_len,
                              v_destination
			      );

            n_data += sprintf(
                              glbMsgBuffer + n_data,
                              "%0*d",
                              Tprint_flg_len,
                              v_print_flg
			      );

            n_data += sprintf(
                              glbMsgBuffer + n_data,
                              "%0*d",
			      Tdate_len,
                              v_sec_pres_date
			      );

// GerrLogReturn(GerrId,
//			  "2507: Passing PrID %d / Line %d, len = %d.\n[%s]",
//				v_prescription_id, v_line_no, n_data, glbMsgBuffer);


	    // DonR 22Oct2007: HACK! Add two to record length to account for
        // two-byte line numbers.
if (n_data != (RECORD_LEN_2507 + MESSAGE_ID_LEN + 2)) GerrLogMini(GerrId, "2507 bad data = {%s}, len = %d/%d.", glbMsgBuffer,n_data,(RECORD_LEN_2507 + MESSAGE_ID_LEN + 2));
	    StaticAssert( n_data == RECORD_LEN_2507 + MESSAGE_ID_LEN + 2 );

/*=======================================================================
||									||
||			Send buffer to server				||
||									||
 =======================================================================*/

	    if( send_buffer_to_server( glbSocket,glbMsgBuffer, n_data )
		!= success )
	    {
		retval = CM_RESTART_CONNECTION;
		cursor_flg = 1;
		break;
	    }
		else need_rollback = 1;


/*=======================================================================
||									||
||			Delete record logically				||
||									||
 =======================================================================*/

		ExecSQL (	MAIN_DB, AS400CL_2507_UPD_presc_drug_inter,
					&REPORTED_TO_AS400, END_OF_ARG_LIST	);

	    if( SQLERR_error_test() != MAC_OK )
	    {
		retval = SB_LOCAL_DB_ERROR;
		break;
	    }

	    //printf("send presc_drug_inter line ----%d--%d--%d--%d----\n",
	    //        v_prescription_id,
	    //	    v_line_no,
	    //	    v_presc_id_inter,
	    //	    v_line_no_inter
	    //	    ); fflush(stdout);

    /* << -------------------------------------------------------- >> */
    /*	        Stop on full batch of prescriptions		      */
    /* << -------------------------------------------------------- >> */

	    if( ++n_uncommited_recs == BATCH_DIM )
	    {
		retval = SB_BATCH_SUCCESSFULLY_PASSED;
		break;
	    }

/*=======================================================================
||									||
||			Get message from server				||
||									||
 =======================================================================*/

	    exit_flg = false;

	    server_msg = get_msg_from_server( NO_WAIT_MODE );
	    switch( server_msg )
	    {
		case CM_SYSTEM_IS_NOT_BUSY:
		case CM_NO_MESSAGE_PASSED:/* NONMESSAGE */

		    usleep( 5000  );

		    break;

	      default:

		retval = server_msg;
		exit_flg = true;

	    }

	    if( exit_flg == true )
	    {
		break;
	    }

	}	// while( 1 )

    }
    while( 0 );

    if( cursor_flg )
    {
		CloseCursor (	MAIN_DB, AS400CL_2507_presc_drug_inter_cur	);
    }

	recs_to_commit = n_uncommited_recs;

    POP_RET (retval);

} /* end of handler 2507 */



/*=======================================================================
||																		||
||			Message 2508 --	Stock Report								||
||																		||
 =======================================================================*/
static int pass_record_batch_2508 (void)
{
	// Local variables.
	int				BATCH_DIM;
	int				server_msg;
	int				n_uncommited_recs;
	int				rc;
	int				i;
	int				n_data;		/* length of data to be sent to server */
	static int		cursor_flg = 1;
	bool			exit_flg;
	TSuccessStat	retval = SB_END_OF_FETCH_NOCOMMIT;

	// Sql variables.
		TLineNum			v_line_num;
		Tpharmacy_code		v_pharmacy_code;
		Tterminal_id		v_terminal_id;
		Taction_type		v_action_type;
		Tserial_no			v_serial_no;
		Tdiary_monthNew		v_diary_month;
		Tlargo_code			v_largo_code;
		Tinventory_op		v_inventory_op;
		Tunits_amount		v_units_amount;
		Tquantity_type		v_quantity_type;
		Tprice_for_op		v_price_for_op;
		Ttotal_drug_price	v_total_drug_price;
		Top_stock			v_op_stock;
		Tunits_stock		v_units_stock;
		Tmin_stock_in_op	v_min_stock_in_op;
		TDate				SysDate;
		TDate				v_update_date;
		Ttime				v_update_time;
		TBasePriceFor1Op	v_base_op_price;
		static int			MaxRecCount;
		int					v_r_count;	// DonR 18SEP2002

	pushname ("pass_record_batch_2508");


	// Initialize variables.
	BATCH_DIM = get_dimension_of_batch (2508);
	SysDate = GetDate ();
	exit_flg  = false;				/* if true - exit function  */
	n_uncommited_recs = 0;			// Number of records sent to remote DB without commit */

	if (cursor_flg)
	{
		do
		{
			DeclareCursor (	MAIN_DB, AS400CL_2508_stock_report_cur,
							&NOT_REPORTED_TO_AS400,		&MinPharmacyToAs400,
							&MaxRecCount,				&SysDate,
							END_OF_ARG_LIST										);


			// Go few rows back in order to not lock key value in index of table.
			ExecSQL (	MAIN_DB, AS400CL_2508_READ_stock_report_max_r_count,
						&MaxRecCount, END_OF_ARG_LIST							);

			if (SQLERR_error_test ())
			{
				retval = SB_DATA_ACCESS_CONFLICT;
				break;
			}


			// DonR 03Mar2005: Maccabi pharmacies now use SAP instead of this table - so
			// we need to eliminate most of this safety margin, since volumes from private
			// pharmacies are much lower.
//			MaxRecCount -= 10;		// DonR 05Apr2005: Eliminated safety margin, since (with SAP
//									// in place) volume has dwindled to almost nothing.
//			MaxRecCount -= 2000;	// DonR 22Jan2004: Increased safety margin.


			//GerrLogReturn (GerrId, "Max record num %d", MaxRecCount);


			// Now open table...safely
			OpenCursor (	MAIN_DB, AS400CL_2508_stock_report_cur	);

			if (SQLERR_error_test ())
			{
				retval = SB_DATA_ACCESS_CONFLICT;
				break;
			}

		}	// Dummy do loop.
		while (0);

	}	// Need to declare and open cursor.


	// Loop on stock_report table rows.
	if (retval != SB_DATA_ACCESS_CONFLICT)
	{
		do
		{
			cursor_flg = 0;	// Default: Next time we enter this function, we'll still be using
							// the same open cursor.

			while (1)
			{
				FetchCursorInto (	MAIN_DB, AS400CL_2508_stock_report_cur,
									&v_line_num,			&v_pharmacy_code,	&v_terminal_id,
									&v_action_type,			&v_serial_no,		&v_diary_month,
									&v_largo_code,			&v_inventory_op,	&v_units_amount,
									&v_quantity_type,		&v_price_for_op,	&v_total_drug_price,
									&v_op_stock,			&v_units_stock,		&v_min_stock_in_op,
									&v_update_date,			&v_update_time,		&v_base_op_price,
									&v_r_count,				END_OF_ARG_LIST								);

				// 28.06.1998 - Close cursor if we are close to recent stock report rows,
				// in order to prevent locks.
				if ((diff_from_now (v_update_date, v_update_time) < TRANSFER_INTERVAL) ||
					(SQLERR_code_cmp (SQLERR_end_of_fetch) == MAC_TRUE))
				{
					cursor_flg = 1;

					retval = n_uncommited_recs ? SB_END_OF_FETCH : SB_END_OF_FETCH_NOCOMMIT;
					break;
				}
				else
				{
					if (SQLERR_code_cmp (SQLERR_access_conflict) == MAC_TRUE)
					{
						retval = SB_DATA_ACCESS_CONFLICT;
						break;
					}
					else
					{
						if (SQLERR_error_test () != MAC_OK)
						{
							retval = SB_LOCAL_DB_ERROR;
							break;
						}
					}	// No access conflict.
				}	// We're not done with the fetch.


				// Move record into buffer.
				n_data = 0;

				n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	MESSAGE_ID_LEN,			2508				);

				n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	 7,		v_pharmacy_code		);
				n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	 2,		v_terminal_id		);
				n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	 2,		v_action_type		);
				n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	 7,		v_serial_no			);
				n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	 6,		v_diary_month		);
				n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	 9,		v_largo_code		);	//Marianna 14Jul2024 User Story #330877
				n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	 5,		v_inventory_op		);
				n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	 5,		v_units_amount		);
				n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	 1,		v_quantity_type		);
				n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	 9,		v_price_for_op		);
				n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	 9,		v_total_drug_price	);
				n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	 7,		v_op_stock			);
				n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	 7,		v_units_stock		);
				n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	 5,		v_min_stock_in_op	);
				n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	 4,		v_line_num			);
				n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	 9,		v_base_op_price		);

				StaticAssert( n_data == RECORD_LEN_2508 + MESSAGE_ID_LEN );


				// Send buffer to server.
				if (send_buffer_to_server (glbSocket, glbMsgBuffer, n_data) != success)
				{
					retval = CM_RESTART_CONNECTION;
					cursor_flg = 1;
					break;
				}
				else need_rollback = 1;


				// Get message from server.
				exit_flg = false;

			    usleep( 5000  );
				server_msg = get_msg_from_server (NO_WAIT_MODE);

				switch (server_msg)
				{
					case CM_SYSTEM_IS_NOT_BUSY:
					case CM_NO_MESSAGE_PASSED:	// No message = no problem (we hope).
						//nap (100);
						break;

					default:
						retval = server_msg;
						exit_flg = true;
				}

				if (exit_flg == true)
				{
					break;
				}


				// Mark row as "reported to AS400".
				ExecSQL (	MAIN_DB, AS400CL_2508_UPD_stock_report,
							&REPORTED_TO_AS400, END_OF_ARG_LIST	);

				if (SQLERR_error_test () != MAC_OK)
				{
					retval = SB_LOCAL_DB_ERROR;
					break;
				}

				// Stop on full batch.
				if (++n_uncommited_recs == BATCH_DIM)
				{
					retval = SB_BATCH_SUCCESSFULLY_PASSED;
					break;
				}

			}	// while (1) - NON-dummy loop.

		}	// Dummy Loop.
		while (0);

	}	// No access conflict opening cursor.


    if (cursor_flg)
    {
		CloseCursor (	MAIN_DB, AS400CL_2508_stock_report_cur	);
    }

	recs_to_commit = n_uncommited_recs;

    POP_RET (retval);

} /* end of handler 2508 */



/*=======================================================================
||																		||
||			Message 2509 --	money_emp_lines								||
||																		||
 =======================================================================*/
static int pass_record_batch_2509(
				  void
				  )
{

/*=======================================================================
||									||
||			Local variables					||
||									||
 =======================================================================*/

    int		BATCH_DIM;
    TSuccessStat	retval = SB_END_OF_FETCH_NOCOMMIT;
    int		server_msg;
    int		n_uncommited_recs;
    int		rc;
    bool		exit_flg;
    int		i;
    int		n_data;		/* length of data to be sent to server */
    static int	cursor_flg = 1;

/*=======================================================================
||									||
||			Sql variables					||
||									||
 =======================================================================*/


    int				v_pharmacy_code;
    int				v_user_ident;
    int				v_card_num;
    short			v_payment_type;
    int				v_sale_total;
    short			v_sale_num;
    int				v_refund_total;
    short			v_refund_num;
    short			v_terminal_id_proc;
    short			v_terminal_id;
    int				v_receipt_num;
    int				v_update_date;
    int				v_update_time;
    short			v_diary_month;
    short			v_action_type;
    int				MaxRecCount;
	int				v_r_count;	// DonR 18SEP2002


	pushname ("pass_record_batch_2509");

/*=======================================================================
||									||
||			Initialize variables				||
||									||
 =======================================================================*/

    BATCH_DIM = get_dimension_of_batch( 2509 );
    exit_flg  = false;				/* if true - exit function  */
    n_uncommited_recs = 0;			/* num of records sent to   */
						/* remote DB without commit */

    if( cursor_flg )
    do
    {
		DeclareCursor (	MAIN_DB, AS400CL_2509_money_emp_lines_cur,
						&NOT_REPORTED_TO_AS400,		&MinPharmacyToAs400,
						&MaxRecCount,				END_OF_ARG_LIST			);

	//
	// Go few rows back in order to
	//  not lock key value in index of table
	//
		ExecSQL (	MAIN_DB, AS400CL_2509_READ_money_emp_lines_max_r_count,
					&MaxRecCount, END_OF_ARG_LIST								);

	if( SQLERR_error_test() )
	{
	    retval = SB_DATA_ACCESS_CONFLICT;
	    break;
	}

	// DonR 17Jul2008: As this is a fairly low-volume table, reduce "safety margin" from 50 to 20.
	if (Production)
		MaxRecCount -= 20;

	//
	// Now open table...safely
	//
		OpenCursor (	MAIN_DB, AS400CL_2509_money_emp_lines_cur	);

	if( SQLERR_error_test() )
	{
	    retval = SB_DATA_ACCESS_CONFLICT;
	    break;
	}

    }
    while( 0 );

/*=======================================================================
||									||
||			Loop over money_emp_lines			||
||									||
 =======================================================================*/

    if( retval != SB_DATA_ACCESS_CONFLICT )
    do
    {
	cursor_flg = 0;

	while( 1 )
	{
		FetchCursorInto (	MAIN_DB, AS400CL_2509_money_emp_lines_cur,
							&v_diary_month,			&v_pharmacy_code,		&v_terminal_id,
							&v_action_type,			&v_receipt_num,			&v_terminal_id_proc,
							&v_user_ident,			&v_card_num,			&v_payment_type,
							&v_sale_total,			&v_sale_num,			&v_refund_total,
							&v_refund_num,			&v_r_count,				END_OF_ARG_LIST			);

	    if( SQLERR_code_cmp( SQLERR_end_of_fetch ) == MAC_TRUE )
	    {
		cursor_flg = 1;

		retval = n_uncommited_recs ? SB_END_OF_FETCH :
					     SB_END_OF_FETCH_NOCOMMIT;
		break;
	    }
	    else if( SQLERR_code_cmp( SQLERR_access_conflict ) == MAC_TRUE )
	    {
		retval = SB_DATA_ACCESS_CONFLICT;
		break;
	    }
	    else if( SQLERR_error_test() != MAC_OK )
	    {
		retval = SB_LOCAL_DB_ERROR;
		break;
	    }

/*=======================================================================
||									||
||			Move record into buffer				||
||									||
 =======================================================================*/

	    n_data = 0;

	    n_data += sprintf(		/* -> message header */
			      glbMsgBuffer + n_data,
			      "%0*d",
			      MESSAGE_ID_LEN,
			      2509
			      );

	    n_data += sprintf(
			      glbMsgBuffer + n_data,
			      "%0*d",
			      Tdiary_month_len,
			      v_diary_month
			      );

	    n_data += sprintf(
			      glbMsgBuffer + n_data,
			      "%0*d",
			      Tpharmacy_code_len,
			      v_pharmacy_code
			      );

	    n_data += sprintf(
			      glbMsgBuffer + n_data,
			      "%0*d",
			      Tterminal_id_len,
			      v_terminal_id
			      );

	    n_data += sprintf(
			      glbMsgBuffer + n_data,
			      "%0*d",
			      Taction_type_len,
			      v_action_type
			      );

	    n_data += sprintf(
			      glbMsgBuffer + n_data,
			      "%0*d",
			      Treceipt_num_len,
			      v_receipt_num
			      );

	    n_data += sprintf(
			      glbMsgBuffer + n_data,
			      "%0*d",
			      Tterminal_id_proc_len,
			      v_terminal_id_proc
			      );

	    n_data += sprintf(
			      glbMsgBuffer + n_data,
			      "%0*d",
			      Tuser_ident_len,
			      v_user_ident
			      );

	    n_data += sprintf(
			      glbMsgBuffer + n_data,
			      "%0*d",
			      Tcard_num_len,
			      v_card_num
			      );

	    n_data += sprintf(
			      glbMsgBuffer + n_data,
			      "%0*d",
			      Tpayment_type_len,
			      v_payment_type
			      );

	    n_data += sprintf(
			      glbMsgBuffer + n_data,
			      "%0*d",
			      Tsale_total_len,
			      v_sale_total
			      );

	    n_data += sprintf(
			      glbMsgBuffer + n_data,
			      "%0*d",
			      Tsale_num_len,
			      v_sale_num
			      );

	    n_data += sprintf(
			      glbMsgBuffer + n_data,
			      "%0*d",
			      Trefund_total_len,
			      v_refund_total
			      );

	    n_data += sprintf(
			      glbMsgBuffer + n_data,
			      "%0*d",
			      Trefund_num_len,
			      v_refund_num
			      );

	    StaticAssert( n_data == RECORD_LEN_2509 + MESSAGE_ID_LEN );

/*=======================================================================
||									||
||			Send buffer to server				||
||									||
 =======================================================================*/

	    if( send_buffer_to_server( glbSocket,glbMsgBuffer, n_data )
		!= success )
	    {
		retval = CM_RESTART_CONNECTION;
		cursor_flg = 1;
		break;
	    }
		else need_rollback = 1;


/*=======================================================================
||									||
||			Delete record logically				||
||									||
 =======================================================================*/

		ExecSQL (	MAIN_DB, AS400CL_2509_UPD_money_emp_lines,
					&REPORTED_TO_AS400, END_OF_ARG_LIST	);

	    if( SQLERR_error_test() != MAC_OK )
	    {
		retval = SB_LOCAL_DB_ERROR;
		break;
	    }

//	    printf("send money_emp_lines line ---------%d---------\n",
//		    v_receipt_num ); fflush(stdout);

    /* << -------------------------------------------------------- >> */
    /*	        Stop on full batch of prescriptions		      */
    /* << -------------------------------------------------------- >> */

	    if( ++n_uncommited_recs == BATCH_DIM )
	    {
		retval = SB_BATCH_SUCCESSFULLY_PASSED;
		break;
	    }

/*=======================================================================
||									||
||			Get message from server				||
||									||
 =======================================================================*/

	    exit_flg = false;

	    server_msg = get_msg_from_server( NO_WAIT_MODE );
	    switch( server_msg )
	    {
		case CM_SYSTEM_IS_NOT_BUSY:
		case CM_NO_MESSAGE_PASSED:/* NONMESSAGE */

		    usleep( 5000  );

		    break;

		default:

		    retval = server_msg;
		    exit_flg = true;

	    }


	    if( exit_flg == true )
	    {
		break;
	    }

	}	// while( 1 )

    }
    while( 0 );

    if( cursor_flg )
    {
		CloseCursor (	MAIN_DB, AS400CL_2509_money_emp_lines_cur	);
    }

	recs_to_commit = n_uncommited_recs;

    POP_RET (retval);

} /* end of handler 2509 */



/*=======================================================================
||																		||
||			Message 2512 --	pc_error_message							||
||																		||
 =======================================================================*/
static int pass_record_batch_2512 (void)
{

	/*=======================================================================
	||																		||
	||			Local variables												||
	||																		||
	=======================================================================*/

	int				BATCH_DIM;
	int				server_msg;
	int				n_uncommited_recs;
	int				rc;
	int				i;
	int				n_data;     /* length of data to be sent to server */
	static int		cursor_flg = 1;
	bool			exit_flg;
	TSuccessStat	retval = SB_END_OF_FETCH_NOCOMMIT;
	Terror_description	v_error_description;
	Terror_line			v_error_line;
	Terror_code			v_error_code;
	Tseverity_level		v_severity_level;
	Tseverity_pharm		v_severity_pharm;
	Tc_define_name		v_c_define_name;


	pushname ("pass_record_batch_2512");


	/*=======================================================================
	||																		||
	||			Initialize variables										||
	||																		||
	=======================================================================*/

	BATCH_DIM			= get_dimension_of_batch (2512);
	exit_flg			= false;	/* if true - exit function  */
	n_uncommited_recs	= 0;		/* num of records sent to   */
									/* remote DB without commit */

	if (cursor_flg)
	{
		do
		{
			DeclareCursor (	MAIN_DB, AS400CL_2512_pc_error_message_cur,
							&NOT_REPORTED_TO_AS400, END_OF_ARG_LIST		);


			OpenCursor (	MAIN_DB, AS400CL_2512_pc_error_message_cur	);

			if (SQLERR_error_test ())
			{
				retval = SB_DATA_ACCESS_CONFLICT;
				break;
			}

		}
		while( 0 );
	}


	/*=======================================================================
	||																		||
	||			Loop on pc_error_message table lines						||
	||																		||
	=======================================================================*/

	if (retval != SB_DATA_ACCESS_CONFLICT)
	do
	{
		cursor_flg = 0;

		while (1)
		{
			FetchCursorInto (	MAIN_DB, AS400CL_2512_pc_error_message_cur,
								&v_error_description,	&v_error_line,
								&v_error_code,			&v_severity_level,
								&v_severity_pharm,		&v_c_define_name,
								END_OF_ARG_LIST									);

			if (SQLERR_code_cmp (SQLERR_end_of_fetch ) == MAC_TRUE)
			{
				cursor_flg = 1;
				retval = n_uncommited_recs ? SB_END_OF_FETCH : SB_END_OF_FETCH_NOCOMMIT;
				break;
			}
			else
			{
				if (SQLERR_code_cmp (SQLERR_access_conflict) == MAC_TRUE)
				{
					retval = SB_DATA_ACCESS_CONFLICT;
					break;
				}
				else
				{
					if (SQLERR_error_test () != MAC_OK)
					{
						retval = SB_LOCAL_DB_ERROR;
						break;
					}
				}
			}


			/*=======================================================================
			||																		||
			||			Move record into buffer										||
			||																		||
			=======================================================================*/

			switch_to_win_heb ((unsigned char *)v_error_description);
			switch_to_win_heb ((unsigned char *)v_error_line);

			n_data = 0;

			/* -> message header */
			n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	MESSAGE_ID_LEN,			2512				);

			n_data += sprintf (glbMsgBuffer + n_data, "%-*.*s",	Terror_description_len,
																Terror_description_len,	v_error_description	);

			n_data += sprintf (glbMsgBuffer + n_data, "%-*.*s",	Terror_line_len,
																Terror_line_len,		v_error_line		);

			n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	Terror_code_len,		v_error_code		);

			n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	Tseverity_level_len,	v_severity_level	);

			n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	Tseverity_pharm_len,	v_severity_pharm	);

			n_data += sprintf (glbMsgBuffer + n_data, "%-*.*s",	Tc_define_name_len,
																Tc_define_name_len,		v_c_define_name		);


			StaticAssert (n_data == RECORD_LEN_2512 + MESSAGE_ID_LEN);


			/*=======================================================================
			||																		||
			||			Send buffer to server										||
			||																		||
			=======================================================================*/

			if (send_buffer_to_server (glbSocket, glbMsgBuffer, n_data) != success)
			{
				retval = CM_RESTART_CONNECTION;
				cursor_flg = 1;
				break;
			}
			else
			{
				need_rollback = 1;
			}


			/*=======================================================================
			||																		||
			||			Delete record logically										||
			||																		||
			=======================================================================*/

			ExecSQL (	MAIN_DB, AS400CL_2512_UPD_pc_error_message,
						&REPORTED_TO_AS400, END_OF_ARG_LIST	);

			if (SQLERR_error_test () != MAC_OK)
			{
				retval = SB_LOCAL_DB_ERROR;
				break;
			}

			if (0)
			{
				static int counter = 0;

				GerrLogReturn (GerrId,
							   "send pc_error_message line: Err Code = %d, Err Line %s, Counter = %d\n",
							   v_error_code,
							   v_error_line,
							   ++counter);
			}


			/* << -------------------------------------------------------- >> */
			/*	        Stop on full batch.								      */
			/* << -------------------------------------------------------- >> */
			if (++n_uncommited_recs == BATCH_DIM)
			{
				retval = SB_BATCH_SUCCESSFULLY_PASSED;
				break;
			}

			/*=======================================================================
			||																		||
			||			Get message from server										||
			||																		||
			=======================================================================*/

			exit_flg = false;

			server_msg = get_msg_from_server (NO_WAIT_MODE);

			switch (server_msg)
			{
				case CM_SYSTEM_IS_NOT_BUSY:
				case CM_NO_MESSAGE_PASSED:	/* NONMESSAGE */
										    usleep( 5000  );
											break;

				default:
											retval = server_msg;
											exit_flg = true;

			}


			if (exit_flg == true)
			{
				break;
			}

		}	// while( 1 )

	}
	while (0);

	if (cursor_flg)
	{
		CloseCursor (	MAIN_DB, AS400CL_2512_pc_error_message_cur	);
	}

	recs_to_commit = n_uncommited_recs;

	POP_RET (retval);

} /* end of handler 2512 */



// DonR: Deleted Trns. 2513/2514 (upload/mark undelivered prescription/prescription_drugs rows).
// The routine below simply marks stuff as having been sent, but doesn't send anything to AS/400.


static int pass_record_batch_2513_2514 (void)
{
		int		Today;
		int		TooRecently;

	Today		= GetDate ();
	TooRecently	= GetTime ();
	TooRecently	= IncrementTime (TooRecently, -3600, NULL);	// 1-hour safety margin.

	// Note that if we've gone past midnight, don't delete previous-day stuff until 1:00 A.M. - this
	// way we avoid problems if we happen to enter this function just after midnight and someone
	// started a drug sale just *before* midnight.
	// Prescriptions first...
	ExecSQL (	MAIN_DB, AS400CL_2513_UPD_prescriptions,
				&NO_NEED_TO_REPORT_TO_AS400,	&NOT_REPORTED_TO_AS400,
				&DRUG_NOT_DELIVERED,			&TooRecently,
				&Today,							&TooRecently,
				END_OF_ARG_LIST												);

//GerrLogMini(GerrId, "2513: AS400CL_2513_UPD_prescriptions returned SQLCODE %d.", SQLCODE);

	if (SQLERR_error_test ())
		return (SB_DATA_ACCESS_CONFLICT);
	else
		recs_to_commit = sqlca.sqlerrd[2];

	// Prescription_drugs second...
	ExecSQL (	MAIN_DB, AS400CL_2513_UPD_prescription_drugs,
				&NO_NEED_TO_REPORT_TO_AS400,	&NOT_REPORTED_TO_AS400,
				&DRUG_NOT_DELIVERED,			&TooRecently,
				&Today,							&TooRecently,
				END_OF_ARG_LIST												);

	if (SQLERR_error_test ())
		return (SB_DATA_ACCESS_CONFLICT);
	else
		trailer_recs_to_commit = sqlca.sqlerrd[2];

	// ...And, finally, prescription_msgs.
	ExecSQL (	MAIN_DB, AS400CL_2513_UPD_prescription_msgs,
				&NO_NEED_TO_REPORT_TO_AS400,	&NOT_REPORTED_TO_AS400,
				&DRUG_NOT_DELIVERED,			&TooRecently,
				&Today,							&TooRecently,
				END_OF_ARG_LIST												);

	if (SQLERR_error_test ())
		return (SB_DATA_ACCESS_CONFLICT);
	else
	{
		// Note that we add prescription_msgs update count to the count from prescription_drugs.
		trailer_recs_to_commit += sqlca.sqlerrd[2];
		return (SB_END_OF_FETCH);
	}
}



//=======================================================================
//																		||
//			Message 2515		pharmacy_ishur							||
//																		||
//=======================================================================
static int pass_record_batch_2515 (void)
{
/*=======================================================================
||			Local variables												||
 =======================================================================*/
    int				BATCH_DIM;
    TSuccessStat	retval = SB_END_OF_FETCH_NOCOMMIT;
    int				server_msg;
    int				n_uncommited_recs;
    int				rc;
    bool			exit_flg;
    int				i;
    int				n_data;		/* length of data to be sent to server */
    static int		cursor_flg = 1;

/*=======================================================================
||																		||
||			Sql variables												||
||																		||
 =======================================================================*/

		TPharmNum					v_pharmacy_code;
		TInstituteCode				v_institute_code;
		TTerminalNum				v_terminal_id;
		TGenericYYMM				v_diary_month;
		TGenericYYYYMMDD			v_ishur_date;
		TGenericHHMMSS				v_ishur_time;
		TSpecialRecipeNumSource		v_ishur_source;
		TSpecialRecipeNum			v_pharm_ishur_num;
		TMemberIdentification		v_member_id;
		TIdentificationCode			v_mem_id_extension;
		TAuthLevel					v_authority_level;
		TTypeDoctorIdentification	v_auth_id_type;
		TDoctorIdentification		v_authority_id;
		TDoctorFirstName			v_auth_first_name;
		TDoctorFamilyName			v_auth_last_name;
		TSpecialRecipeNumSource		v_as400_ishur_src;
		TSpecialRecipeNum			v_as400_ishur_num;
		short						v_as400_ishur_extend;
		short						v_line_number;
		long						v_largo_code;
		long						v_preferred_largo;
		short						v_member_price_code;
		long						v_const_sum_to_pay;
		long						v_rule_number;
		long						v_speciality_code;
		short						v_treatment_category;
		short						v_in_health_pack;
		short						v_no_presc_sale_flag;
		TGenericText75				v_ishur_text;
		short						v_error_code;
		short						v_comm_error_code;
		short						v_used_flg;
		long						v_used_pr_id;
		short						v_used_pr_line_num;
		long						v_updated_date;
		long						v_updated_time;
		short						v_updated_by_trn;
		short						v_reported_to_as400;
		long						v_rrn;
		long						SysDate;
		long						TooRecently;			// DonR 09May2006


	pushname ("pass_record_batch_2515");

	/*=======================================================================
	||			Initialize variables										||
	 =======================================================================*/
	BATCH_DIM = get_dimension_of_batch( 2515 );
	exit_flg  = false;				/* if true - exit function  */
	n_uncommited_recs = 0;			/* num of records sent to   */
									/* remote DB without commit */

	SysDate		= GetDate ();
	TooRecently	= GetTime ();
	TooRecently	= IncrementTime (TooRecently, -120, NULL);	// 2-minute safety margin.

    if (cursor_flg)
    do
    {
		DeclareCursor (	MAIN_DB, AS400CL_2515_pharmacy_ishur_cur,
						&NOT_REPORTED_TO_AS400,		&SysDate,
						&TooRecently,				&SysDate,
						&TooRecently,				END_OF_ARG_LIST	);


		// Open cursor.
		OpenCursor (	MAIN_DB, AS400CL_2515_pharmacy_ishur_cur	);

		if (SQLERR_error_test ())
		{
			retval = SB_DATA_ACCESS_CONFLICT;
			break;
		}

    }
    while (0);


	/*=======================================================================
	||																		||
	||			Loop on pharmacy_ishur table rows							||
	||																		||
	 =======================================================================*/
	if (retval != SB_DATA_ACCESS_CONFLICT)
	do
	{
		cursor_flg = 0;

		while (1)
		{
			FetchCursorInto (	MAIN_DB, AS400CL_2515_pharmacy_ishur_cur,
								&v_pharmacy_code,		&v_institute_code,
								&v_terminal_id,			&v_diary_month,
								&v_ishur_date,			&v_ishur_time,
								&v_ishur_source,		&v_pharm_ishur_num,
								&v_member_id,			&v_mem_id_extension,
								&v_authority_level,		&v_auth_id_type,
								&v_authority_id,		&v_auth_first_name,
								&v_auth_last_name,		&v_as400_ishur_src,
								&v_as400_ishur_num,		&v_as400_ishur_extend,
								&v_line_number,			&v_largo_code,
								&v_preferred_largo,		&v_member_price_code,
								&v_const_sum_to_pay,	&v_rule_number,
								&v_speciality_code,		&v_treatment_category,
								&v_in_health_pack,		&v_no_presc_sale_flag,
								&v_ishur_text,			&v_error_code,
								&v_comm_error_code,		&v_used_flg,
								&v_used_pr_id,			&v_used_pr_line_num,
								&v_updated_date,		&v_updated_time,
								&v_updated_by_trn,		&v_reported_to_as400,
								&v_rrn,					END_OF_ARG_LIST			);

			if (SQLERR_code_cmp (SQLERR_end_of_fetch) == MAC_TRUE)
			{
				cursor_flg = 1;
				retval = n_uncommited_recs ? SB_END_OF_FETCH : SB_END_OF_FETCH_NOCOMMIT;
				break;
			}
			else if (SQLERR_code_cmp (SQLERR_access_conflict) == MAC_TRUE)
			{
				retval = SB_DATA_ACCESS_CONFLICT;
				break;
			}
			else if (SQLERR_error_test () != MAC_OK)
			{
				retval = SB_LOCAL_DB_ERROR;
				break;
			}

			/*=======================================================================
			||			Move record into buffer										||
			=======================================================================*/
			n_data = 0;
			n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	MESSAGE_ID_LEN,			2515				);
			n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	TGenericFlag_len,		v_used_flg			);
			n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	Tpharmacy_code_len,		v_pharmacy_code		);
			n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	Tinstitued_code_len,	v_institute_code	);
			n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	Tmember_id_len,			v_member_id			);
			n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	Tmem_id_extension_len,	v_mem_id_extension	);
			n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	Tdoctor_id_type_len,	v_auth_id_type		);
			n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	Tdoctor_id_len,			v_authority_id		);
			n_data += sprintf (glbMsgBuffer + n_data, "%-*.*s", Tlast_name_len,
																Tlast_name_len,			v_auth_last_name	);
			n_data += sprintf (glbMsgBuffer + n_data, "%-*.*s",	Tfirst_name_len,
																Tfirst_name_len,		v_auth_first_name	);
			n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	TGenericYYYYMMDD_len,	v_ishur_date		);
			n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	TGenericHHMMSS_len,		v_ishur_time		);
			n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	Tspecial_presc_num_len,	v_pharm_ishur_num	);
			n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	TGenericFlag_len,		v_ishur_source		);
			n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	Tspecial_presc_num_len,	v_as400_ishur_num	);
			n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	TGenericFlag_len,		v_as400_ishur_src	);
			n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	Tconfirm_authority__len,v_authority_level	);
			n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	Trule_number_len,		v_rule_number		);
			n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	Tin_health_pack_len,	v_in_health_pack	);
			n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	Tspeciality_code_len,	v_speciality_code	);
			n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	9,						v_largo_code		);	//Marianna 14Jul2024 User Story #330877
			n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	Tprice_code_len,		v_member_price_code	);
			n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	Tfixed_price_len,		v_const_sum_to_pay	);
			n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	TGenericYYYYMMDD_len,	v_updated_date		);
			n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	TGenericHHMMSS_len,		v_updated_time		);
			n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	Tcomm_error_code_len,	v_comm_error_code	);
			n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	Terror_code_len,		v_error_code		);
			n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	TGenericYYYYMMDD_len,	0L /* letter date */);
			n_data += sprintf (glbMsgBuffer + n_data, "%-*.*s",	Tishur_text_len,
																Tishur_text_len,		v_ishur_text		);
			n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	Tprescription_id_len,	v_used_pr_id		);
			n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	Tdiary_month_len,		v_diary_month		);
			n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	5,						v_as400_ishur_extend);

			StaticAssert (n_data == RECORD_LEN_2515 + MESSAGE_ID_LEN);

			/*=======================================================================
			||			Send buffer to server										||
			=======================================================================*/
			if (send_buffer_to_server (glbSocket, glbMsgBuffer, n_data)	!= success)
			{
				retval = CM_RESTART_CONNECTION;
				cursor_flg = 1;
				break;
			}
			else need_rollback = 1;

			/*=======================================================================
			||			Delete record logically										||
			=======================================================================*/

			ExecSQL (	MAIN_DB, AS400CL_2515_UPD_pharmacy_ishur,
						&REPORTED_TO_AS400, &v_rrn, END_OF_ARG_LIST	);

			if (SQLERR_error_test() != MAC_OK)
			{
				retval = SB_LOCAL_DB_ERROR;
				break;
			}

			/* << -------------------------------------------------------- >> */
			/*	        Stop on full batch of records					      */
			/* << -------------------------------------------------------- >> */
			if (++n_uncommited_recs == BATCH_DIM)
			{
				retval = SB_BATCH_SUCCESSFULLY_PASSED;
				break;
			}

			/*=======================================================================
			||			Get message from server										||
			=======================================================================*/
			exit_flg = false;

			server_msg = get_msg_from_server (NO_WAIT_MODE);

			switch (server_msg)
			{
				case CM_SYSTEM_IS_NOT_BUSY:
				case CM_NO_MESSAGE_PASSED:	/* NONMESSAGE */
										    usleep( 5000  );
											break;

				default:
											retval = server_msg;
											exit_flg = true;
			}

			if (exit_flg == true)
			{
				break;
			}

		}	// while (1)
	}
	while (0);


	if (cursor_flg)
	{
		CloseCursor (	MAIN_DB, AS400CL_2515_pharmacy_ishur_cur	);
	}

	recs_to_commit = n_uncommited_recs;

	POP_RET (retval);

} /* end of handler 2515 */



//=======================================================================
//																		||
//			Message 2516		doctor_presc							||
//																		||
//=======================================================================
static int pass_record_batch_2516 (void)
{
	// Local variables
	int				sqlcode_insert;
	int				BATCH_DIM;
	TSuccessStat	retval = SB_END_OF_FETCH_NOCOMMIT;
	int				server_msg;
	int				n_uncommited_recs;
	int				non_sent_recs;
	static int		tot_non_sent = 0;
	int				rc;
	bool			exit_flg;
	int				i;
	int				n_data;		/* length of data to be sent to server */
//	static int		cursor_flg = 1;	// RESTORE THIS & RETEST WHEN WE MOVE TO MS SQL SERVER!
	int				cursor_flg = 1;	// INFORMIX ODBC CLOSES CURSORS WITH EVERY "COMMIT", SO WE ALWAYS HAVE TO RE-OPEN!
	static int		num_sent = 0;

	// Sql variables
		int				v_contractor;
		short			v_contracttype;
		int				v_treatmentcontr;
		int				v_termid;
		short			v_idcode;
		int				v_idnumber;
		int				v_authdate;
		int				v_authtime;
		short			v_payinginst;
		int				v_medicinecode;
		int				v_prescription;
		short			v_quantity;
		short			v_douse;
		short			v_days;
		short			v_op;
		char			v_prescriptiontype [2];
		int				v_prescriptiondate;
		short			v_status;
		short			v_origin_code;
		short			v_sold_units;
		short			v_sold_op;
		int				v_sold_date;
		int				v_sold_time;
		int				v_prescr_added_dt;
		int				v_prescr_added_tm;
		int				v_prw_id;
		short			reported_to_as400;
		int				Today;
		int				TodayAdjusted;
		int				TooRecently;
//		char			*PRW_cur_name	= "prescr_wr_cur";
//		char			*DRX_cur_name	= "doctor_presc_cur";
//		char			*PRESC_cursor;

	pushname ("pass_record_batch_2516");

	// Initialization
	BATCH_DIM			= get_dimension_of_batch (2516);
	exit_flg			= false;		// if true - exit function
	n_uncommited_recs	= 0;			// num of records sent to remote DB without commit
	non_sent_recs		= 0;			// Rows (origin_code >= 1000) marked as sent
										// without actually sending to AS400
	Today				= GetDate ();
	TooRecently			= GetTime ();
//	PRESC_cursor		= DRX_cur_name;

	if (cursor_flg)
	do
	{
//		if (UseNewDocRxTable)
//		{
		// If we're using the new Doctor Prescriptions table, our "safety" margin is two *hours*
		// rather than two minutes.
		if (TooRecently < 20000)
		{
			// If it's less than 2:00 AM, decrement to yesterday and add 22 hours (e.g. if it's
			// currently 1:30 AM, send stuff that came in before yesterday at 23:30).
			TodayAdjusted	= IncrementDate (Today, -1);
			TooRecently		= IncrementTime (TooRecently, (22 * 60 * 60), NULL);
		}
		else
		{
			TodayAdjusted	= Today;
			TooRecently		= IncrementTime (TooRecently, (-2 * 60  * 60), NULL);	// 2-hour safety margin.
		}

		// Note that "prw_id" of -999 is a trigger value - it tells SR.EC on AS/400 to substitute its own running
		// counter, since doctor_presc does not have an equivalent to prw_id. This doesn't allow updates, but
		// we won't be updating existing rows in RK9049 any more anyway.
		CloseCursor		(	MAIN_DB, AS400CL_2516_doctor_presc_cur	);
		FreeStatement	(	MAIN_DB, AS400CL_2516_doctor_presc_cur	);
		DeclareCursor	(	MAIN_DB, AS400CL_2516_doctor_presc_cur,
							&TooRecently,	&TodayAdjusted,
							&TooRecently,	&TodayAdjusted,
							END_OF_ARG_LIST								);

//GerrLogMini (GerrId, "cursor_flg = %d, DeclareCursor gets SQLCODE %d.", cursor_flg, SQLCODE);

		if (SQLERR_error_test ())
		{
			retval = SB_DATA_ACCESS_CONFLICT;
			break;
		}
//GerrLogMini (GerrId, "Declared AS400CL_2516_doctor_presc_cur for %d/%d/%d/%d, SQLCODE = %d.",TooRecently, TodayAdjusted, TooRecently, TodayAdjusted, SQLCODE);

		OpenCursor (	MAIN_DB, AS400CL_2516_doctor_presc_cur	);
//GerrLogMini (GerrId, "cursor_flg = %d, OpenCursor gets SQLCODE %d.", cursor_flg, SQLCODE);

		if (SQLERR_error_test ())
		{
			retval = SB_DATA_ACCESS_CONFLICT;
			break;
		}

		num_sent = 0;
	}
	while (0);	// If (cursor_flg).
//else GerrLogMini (GerrId, "2516: cursor_flg = 0 on re-entry.");

	// Loop on prescr_written table rows

	if (retval != SB_DATA_ACCESS_CONFLICT)
	do
	{
		cursor_flg = 0;

		while (1)
		{
			FetchCursorInto (	MAIN_DB, AS400CL_2516_doctor_presc_cur,
								&v_contractor,			&v_treatmentcontr,			&v_idcode,				&v_idnumber,
								&v_authdate,			&v_authtime,				&v_medicinecode,		&v_prescription,
								&v_quantity,			&v_douse,					&v_days,				&v_op,
								&v_prescriptiontype,	&v_prescriptiondate,		&v_status,				&v_sold_units,
								&v_sold_op,				&v_sold_date,				&v_sold_time,			&v_prescr_added_dt,
								&v_prescr_added_tm,		END_OF_ARG_LIST															);
//if (SQLCODE) GerrLogMini (GerrId, "FetchCursorInto gets SQLCODE %d.", SQLCODE);

			// DonR 22Jan2020: Instead of SELECTing them, just assign the fields
			// that don't exist in doctor_presc.
			v_contracttype	= 0;
			v_termid		= 0;
			v_payinginst	= 0;
			v_origin_code	= 0;
			v_prw_id		= -999;

			if ((SQLERR_code_cmp (SQLERR_end_of_fetch) == MAC_TRUE) || (num_sent++ > 49999))
			{
				cursor_flg = 1;
				retval = n_uncommited_recs ? SB_END_OF_FETCH : SB_END_OF_FETCH_NOCOMMIT;
				break;
			}
			else if (SQLERR_code_cmp (SQLERR_access_conflict) == MAC_TRUE)
			{
				retval = SB_DATA_ACCESS_CONFLICT;
				break;
			}
			else if (SQLERR_error_test () != MAC_OK)
			{
				retval = SB_LOCAL_DB_ERROR;
				cursor_flg = 1;
				CloseCursor (	MAIN_DB, AS400CL_2516_doctor_presc_cur	);
//GerrLogMini (GerrId, "cursor_flg = %d, CloseCursor (after error) gets SQLCODE %d.", cursor_flg, SQLCODE);

				break;
			}


			// Correct (or clear) bogus field values.
			// Marianna 14Jul2024 User Story #330877: Change "sanity check" value
			// to reflect expansion of Largo Code on AS/400 to six digits.
			if ((v_medicinecode > 999999) || (v_medicinecode < 0))
				v_medicinecode = 0;

			/*=======================================================================
			||			Move record into buffer										||
			 =======================================================================*/
			//
			// DonR 09May2005: We now select prescriptions-written rows created by the
			// Pharmacy system (Origin Codes 2003 and 2005) as well as those created by
			// the Doctor system (Origin Code 28). The former are marked as "reported
			// to AS400", but are not in fact sent because the AS400 is interested only
			// in "real" doctor prescriptions.
			// DonR 23Mar2011: Added "modulo" to prevent bad Contractor ID Types from blowing up the program.
			if (v_origin_code < 1000)
			{
				n_data = 0;
				n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	MESSAGE_ID_LEN,			2516				);
				n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	TGenericTZ_len,			v_contractor		);
				n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	Tdoctor_id_type_len,	v_contracttype % 10	);
				n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	TGenericTZ_len,			v_treatmentcontr	);
				n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	9,						v_termid			);
				n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	Tmem_id_extension_len,	v_idcode			);
				n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	TGenericTZ_len,			v_idnumber			);
				n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	TGenericYYYYMMDD_len,	v_authdate			);
				n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	TGenericHHMMSS_len,		v_authtime			);
				n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	Tinstitued_code_len,	v_payinginst		);
				n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	9,						v_medicinecode		);	//Marianna 14Jul2024 User Story #330877
				n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	6,						v_prescription		);
				n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	5,						v_quantity			);
				n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	5,						v_douse				);
				n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	5,						v_days				);
				n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	Top_len,				v_op				);
				n_data += sprintf (glbMsgBuffer + n_data, "%-*.*s", 1, 1,					v_prescriptiontype	);
				n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	TGenericYYYYMMDD_len,	v_prescriptiondate	);
				n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	2,						v_status			);
				n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	5,						v_origin_code		);
				n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	5,						v_sold_units		);
				n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	Top_len,				v_sold_op			);
				n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	TGenericYYYYMMDD_len,	v_sold_date			);
				n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	TGenericHHMMSS_len,		v_sold_time			);
				n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	TGenericYYYYMMDD_len,	v_prescr_added_dt	);
				n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	TGenericHHMMSS_len,		v_prescr_added_tm	);
				n_data += sprintf (glbMsgBuffer + n_data, "%0*d",	Tprescription_id_len,	v_prw_id			);

//GerrLogMini(GerrId,"Act %d, Nominal %d, Buf [%s]", n_data,RECORD_LEN_2516 + MESSAGE_ID_LEN,glbMsgBuffer);

				// DonR 06Dec2020: Improve logging of length mismatch so we can actually find
				// the offending row.
				if (n_data != RECORD_LEN_2516 + MESSAGE_ID_LEN)
				{
					GerrLogMini (GerrId, "2516: Len %d doesn't match %d for TZ %d PrID %d Largo %d.",
								 n_data, RECORD_LEN_2516 + MESSAGE_ID_LEN, v_idnumber, v_prescription, v_medicinecode);
				}

			    StaticAssert( n_data == RECORD_LEN_2516 + MESSAGE_ID_LEN );

				/*=======================================================================
				||			Send buffer to server										||
				 =======================================================================*/
				if (send_buffer_to_server (glbSocket, glbMsgBuffer, n_data)	!= success)
				{
					retval = CM_RESTART_CONNECTION;
					cursor_flg = 1;
					break;
				}
			}
			else
			{
				non_sent_recs++;
			}

			need_rollback = 1;

			// Mark row as sent-to-AS400.
//			// Note that for some reason, the SQL precompiler
//			// doesn't like a variable cursor name in WHERE CURRENT OF - so we have to use
//			// separate SQL code for the two possibilities. Hmph.
//			if (UseNewDocRxTable)
//			{
//GerrLogMini(GerrId, "Executing  AS400CL_2516_UPD_doctor_presc.");
			ExecSQL (	MAIN_DB, AS400CL_2516_UPD_doctor_presc,
						&REPORTED_TO_AS400, END_OF_ARG_LIST			);
//GerrLogMini(GerrId, "AS400CL_2516_UPD_doctor_presc: SQLCODE %d, num affected %d.", SQLCODE, sqlca.sqlerrd[2]);

			if (SQLERR_error_test () != MAC_OK)
			{
				cursor_flg = 1;
				retval = SB_LOCAL_DB_ERROR;
				break;
			}

			/* << -------------------------------------------------------- >> */
			/*	        Stop on full batch of records					      */
			/* << -------------------------------------------------------- >> */
		    if (++n_uncommited_recs == BATCH_DIM)
		    {
				retval = SB_BATCH_SUCCESSFULLY_PASSED;
//GerrLogMini (GerrId, "SB_BATCH_SUCCESSFULLY_PASSED after %d rows sent.", n_uncommited_recs);
				break;
			}

			/*=======================================================================
			||			Get message from server										||
			 =======================================================================*/
		    exit_flg = false;

			if (v_origin_code < 1000)
			{
				server_msg = get_msg_from_server (NO_WAIT_MODE);
				switch (server_msg)
				{
					case CM_SYSTEM_IS_NOT_BUSY:
					case CM_NO_MESSAGE_PASSED:	// NONMESSAGE = no problem
					    usleep (2000);	// DonR 13May2013: 2 milliseconds - was 5.
						break;

					default:
						retval = server_msg;
						exit_flg = true;
				}
			}	// Row was actually sent to AS400.

			if (exit_flg == true)
			{
				break;
			}

		}	// while (1)

	} while (0);

	tot_non_sent += non_sent_recs;

	if (cursor_flg)
	{
		CloseCursor (	MAIN_DB, AS400CL_2516_doctor_presc_cur	);
//GerrLogMini (GerrId, "cursor_flg = %d, CloseCursor (2) gets SQLCODE %d.", cursor_flg, SQLCODE);

		if (tot_non_sent > 0)
			GerrLogMini (GerrId,"Total unsent rows marked = %d.", tot_non_sent);

		tot_non_sent = 0;
	}

	recs_to_commit = n_uncommited_recs - non_sent_recs;

    POP_RET (retval);

} /* end of handler 2516 */



//=======================================================================
//																		||
//			"Message" 2517		Create Specialist Drug Groups			||
//			(Note: This doesn't send anything to AS400 - it just		||
//			 updates the Unix database.)								||
//																		||
//=======================================================================
static int pass_record_batch_2517 (void)

{
	// Local variables
	TSuccessStat	retval = SB_END_OF_FETCH_NOCOMMIT;
	int				n_uncommited_recs		= 0;
	int				qty_flags_changed		= 0;
	int				shaban_flags_changed	= 0;

	int		MasterGroupCode;
	int		DrugGroupCode;
	int		LastGroupCode;
	int		*EconomypriGroupList;
	int		NumEconomypriGroups;
	int		EconomypriGroupCounter;
	short	DrugGroupNbr;
	short	FirstGroupNbr;
	short	ItemSeq;
	int		LargoCode;
	int		PrefLargoCode;
	int		ParentGroupCode;
	int		SysDate;
	int		SysTime;
	int		NeedUpdate_Date = 0;
	int		NeedUpdate_Time = 0;
	int		LastUpdate_Date = 0;
	int		LastUpdate_Time = 0;
	char	LongName		[31];
	char	ParentGroupName	[26];
	char	TableName		[32 + 1];


	// Initialize variables and check if we actually have to do anything.
	GerrLogMini (GerrId,"Generating Specialty Drug Groups table.");

	SysDate = GetDate ();
	SysTime = GetTime ();


	strcpy (TableName, "specialist_drugs_need_update");

	ExecSQL (	MAIN_DB, AS400CL_2517_READ_tables_update,
				&NeedUpdate_Date,	&NeedUpdate_Time,
				&TableName,			END_OF_ARG_LIST			);

	if (SQLCODE != 0)
	{
		GerrLogMini (GerrId,"Couldn't get need-update date/time - SQLCODE = %d.", SQLCODE);
		NeedUpdate_Date = NeedUpdate_Time = 0;
	}

	// DonR 03Jan2022: If the need-to-update timestamp is set in the future,
	// log a message and exit - this is to prevent running Trn. 2517 while
	// data updates from AS/400 on the relevant tables are still in progress.
	if ((NeedUpdate_Date > SysDate) || ((NeedUpdate_Date == SysDate) && (NeedUpdate_Time > SysTime)))
	{
		GerrLogMini (GerrId,"Spclty Drug Groups will update after %d / %d.\t", NeedUpdate_Date, NeedUpdate_Time);
		return (SB_END_OF_FETCH_NOCOMMIT);	// No need to do anything!
	}

	strcpy (TableName, "specialist_drugs_updated");

	ExecSQL (	MAIN_DB, AS400CL_2517_READ_tables_update,
				&LastUpdate_Date,	&LastUpdate_Time,
				&TableName,			END_OF_ARG_LIST			);

	if (SQLCODE != 0)
	{
		GerrLogMini (GerrId,"Couldn't get last-update date/time - SQLCODE = %d.", SQLCODE);
		LastUpdate_Date = LastUpdate_Time = 99999999;
	}

	if (( LastUpdate_Date >  NeedUpdate_Date) ||
		((LastUpdate_Date == NeedUpdate_Date) && (LastUpdate_Time >= NeedUpdate_Time)))
	{
		GerrLogMini (GerrId,"Spclty Drug Groups already up-to-date.\t");
		return (SB_END_OF_FETCH_NOCOMMIT);	// No need to do anything!
	}

	// If we get here, there is actually a need to process stuff - so let's get going!


	// DonR 23Mar2011 TEMPORARY: Until the fix to the AS/400 software adds rule_status value of 9
	// for drug_extension rules with "unknown" confirm_authority values (actually, only 8 is "unknown"),
	// make sure that rule_status is set to a non-zero value.
	// DonR 21Jan2020: Note that this appears still to be necessary. So much for "TEMPORARY"!
	ExecSQL		(	MAIN_DB, AS400CL_2517_UPD_drug_list_rule_status_4,
					&SysDate, END_OF_ARG_LIST							);
	CommitWork	(	MAIN_DB	);	// DonR 23Sep2020: Add interim COMMITs to reduce deadlocks.
	GerrLogMini	(GerrId, "Updated drug_list rule statuses - SQLCODE = %d.", SQLCODE);
	// End 23Mar2011 TEMPORARY code.


	// DonR 14Mar2018: Update the Calculate OP by Volume flag in drug_list based on a
	// lookup from drug_shape (which, as of now, is still a manually-edited table).
	// DonR 24Apr2018: Slightly more sophisticated version of this update. We now look
	// at more fields, including Largo Type and Package Size.
	ExecSQL		(	MAIN_DB, AS400CL_2517_UPD_drug_list_calc_op_by_volume_clear	);
	CommitWork	(	MAIN_DB	);	// DonR 23Sep2020: Add interim COMMITs to reduce deadlocks.

	ExecSQL		(	MAIN_DB, AS400CL_2517_UPD_drug_list_calc_op_by_volume_set_true	);
	CommitWork	(	MAIN_DB	);	// DonR 23Sep2020: Add interim COMMITs to reduce deadlocks.

	GerrLogMini	(GerrId, "Updated drug_list calculate-OP-by-volume flags - SQLCODE = %d.", SQLCODE);


	// DonR 20May2013: Update the new must_prescribe_qty flag in drug_list. This logic could
	// just as easily been put into As400UnixServer, but it was easier to "shoe-horn" it here.
	ExecSQL (	MAIN_DB, AS400CL_2517_UPD_drug_list_must_prescribe_qty_set_true	);

	if (SQLCODE == 0)
	{
		qty_flags_changed += sqlca.sqlerrd[2];

		ExecSQL (	MAIN_DB, AS400CL_2517_UPD_drug_list_must_prescribe_qty_clear	);

		if (SQLCODE == 0)
			qty_flags_changed += sqlca.sqlerrd[2];
		else
			qty_flags_changed = 0;
	}

	GerrLogMini (GerrId,
				 "Updated DL must-prescribe-qty: SQLCODE = %d, %d rows changed.",
				 SQLCODE, qty_flags_changed);

	// DonR 19Aug2021: This error-handling isn't really all that thorough - but I don't think
	// that we actually need to improve it at this point.
	if (SQLCODE != 0)
	{
		CommitWork (MAIN_DB);	// DonR 23Sep2020: Add interim COMMITs to reduce deadlocks.
		return (SB_LOCAL_DB_ERROR);
	}
	else
	{
		CommitWork (MAIN_DB);	// DonR 23Sep2020: Add interim COMMITs to reduce deadlocks.
	}
	// DonR 20May2013 end.


	// DonR 02Jun2016: Update the new "has_shaban_rules" flag in drug_list.
	ExecSQL (	MAIN_DB, AS400CL_2517_UPD_drug_list_has_shaban_rules_set_true,
				&SysDate, END_OF_ARG_LIST										);

	if (SQLCODE == 0)
	{
		shaban_flags_changed += sqlca.sqlerrd[2];

		ExecSQL (	MAIN_DB, AS400CL_2517_UPD_drug_list_has_shaban_rules_clear,
					&SysDate, END_OF_ARG_LIST										);

		if (SQLCODE == 0)
			shaban_flags_changed += sqlca.sqlerrd[2];
	}

	GerrLogMini (GerrId,
				"Updated DL shaban_rules flags: SQLCODE = %d, %d rows changed.",
				SQLCODE, shaban_flags_changed);
	CommitWork (MAIN_DB);	// DonR 23Sep2020: Add interim COMMITs to reduce deadlocks.
	// DonR 02Jun2016 end.


	// DonR 11Jun2018: Update new "home delivery OK" flag in drug_list. Note that since
	// there are sometimes multiple drug_shape rows for the same drug_shape_desc value,
	// we need to use SELECT MAX and WHERE EXISTS to avoid cardinality violations.
	// DonR 22Aug2021 User Story #181694: This now updates delivery_ok_per_shape_code
	// instead of home_delivery_ok; the latter field now comes directly from RK9001P.
	ExecSQL		(	MAIN_DB, AS400CL_2517_UPD_drug_list_delivery_ok_per_shape_code	);
	CommitWork	(	MAIN_DB	);	// DonR 23Sep2020: Add interim COMMITs to reduce deadlocks.


	// This SELECT retrieves drugs that are have a parent group code but are NOT in the economypri table.
	//
	// DonR 06Dec2009: Deleted specialist_drug flag from selection criteria. Under the new rules,
	// drugs purchased with specialist participation will be used in granting further specialist
	// participation even if they are not specialist drugs (e.g. a non-generic drug sold with
	// doctor's or pharmacist's authorization in place of the generic specialist drug).
	// Also, eliminated drug and parent-group names from selects and from the spclty_drug_grp
	// table; nobody uses them or looks at them, and since the table will now have more rows than
	// it did before, making each row smaller will improve program performance.
	DeclareCursor (	MAIN_DB, AS400CL_2517_drug_list_get_non_ep_drugs_cur	);

	// Dummy loop to avoid GOTO's.
	do
	{
		// First, get rid of previous saved old version of the Specialty Drug Group table
		// and create the empty new version.
		GerrLogMini (GerrId,"Dropping backup table and creating new table.\t");

		ExecSQL (	MAIN_DB, AS400CL_2517_DROP_spclty_drug_grp_o	);
//GerrLogMini(GerrId, "Dropped spclty_drug_grp_o - SQLCODE = %d.", SQLCODE);

		if (SQLCODE) GerrLogMini (GerrId, "2517 drop spclty_drug_grp_o failed, SQLCODE = %d.", SQLCODE	);
//		SQLERR_error_test ();
		// No error-handling necessary on this one.

		ExecSQL (	MAIN_DB, AS400CL_2517_DROP_spclty_drug_grp_n	);
//GerrLogMini(GerrId, "Dropped spclty_drug_grp_n - SQLCODE = %d.", SQLCODE);

		if (SQLCODE) GerrLogMini (GerrId, "2517 drop spclty_drug_grp_n failed, SQLCODE = %d.", SQLCODE	);
//		SQLERR_error_test ();
		// No error-handling necessary on this one either.

		ExecSQL (	MAIN_DB, AS400CL_2517_CREATE_spclty_drug_grp_n	);
//GerrLogMini(GerrId, "Created spclty_drug_grp_n - SQLCODE = %d.", SQLCODE);

		if (SQLERR_error_test () != MAC_OK)
		{
			retval = SB_LOCAL_DB_ERROR;
			break;
		}


		// Update Drug List with current Specialist Drug status.
		// First, mark all those drugs that are currently present in spclty_largo_prcnt.
		GerrLogMini (GerrId,"Updating drug_list specialist status...\t");

		ExecSQL (	MAIN_DB, AS400CL_2517_UPD_drug_list_set_specialist_drug_to_2	);

		if (SQLERR_code_cmp (SQLERR_access_conflict) == MAC_TRUE)
		{
			retval = SB_DATA_ACCESS_CONFLICT;
			break;
		}
		else if (SQLERR_error_test () != MAC_OK)
		{
			retval = SB_LOCAL_DB_ERROR;
			break;
		}

		CommitWork (MAIN_DB);	// DonR 23Sep2020: Add interim COMMITs to reduce deadlocks.


		// Now clear the status of drugs that were previously present but aren't any more.
		ExecSQL (	MAIN_DB, AS400CL_2517_UPD_drug_list_set_specialist_drug_to_0	);

		if (SQLERR_code_cmp (SQLERR_access_conflict) == MAC_TRUE)
		{
			retval = SB_DATA_ACCESS_CONFLICT;
			break;
		}
		else if (SQLERR_error_test () != MAC_OK)
		{
			retval = SB_LOCAL_DB_ERROR;
			break;
		}

		CommitWork (MAIN_DB);	// DonR 23Sep2020: Add interim COMMITs to reduce deadlocks.


		// Finally, reset the flag for currently-present drugs from 2 to 1.
		ExecSQL (	MAIN_DB, AS400CL_2517_UPD_drug_list_set_specialist_drug_to_1	);

		if (SQLERR_code_cmp (SQLERR_access_conflict) == MAC_TRUE)
		{
			retval = SB_DATA_ACCESS_CONFLICT;
			break;
		}
		else if (SQLERR_error_test () != MAC_OK)
		{
			retval = SB_LOCAL_DB_ERROR;
			break;
		}

		CommitWork (MAIN_DB);	// DonR 23Sep2020: Add interim COMMITs to reduce deadlocks.
		GerrLogMini (GerrId,"Done updating drug_list specialist status.\t");


		// Open cursor to loop through Economypri groups.
		// DonR 21Dec2011: To make more room for "real" generic-substitution groups, the boundary
		// for "therapoietic" groups is being moved from 600 to 800.
		// DonR 05Nov2014: Changing the 799/800 boundary to 999/1000. This is temporary, as the
		// code is being expanded to six digits. When the next phase comes in, the boundary will
		// be at 19999/20000.
		// DonR 30Jan2018: Use System Code instead of a range of Group Codes to select what's relevant from economypri.
		// DonR 31Jan2018: Use new send_and_use_pharm flag instead of a range of Group Codes OR
		// the System Code. This allows us to locate all the table-specific logic for economypri
		// in Transaction 2054/4054 in As400UnixServer. Note that if someone decides (for example)
		// that System Code 4 is not relevant for specialist-drug lookup, we may need to add System
		// Code back into the WHERE clause here.
		// DonR 23Sep2020: MS-SQL wasn't happy with nesting cursors AS400CL_2517_economypri_get_ep_groups_cur
		// and AS400CL_2517_drug_list_get_group_drugs_cur - possibly because they both involve the same table,
		// since the error returned was a "database contention" complaint. To get around the problem, I'm
		// now allocating an integer array EconomypriGroupList (with its size based on what's there - thus
		// the new query AS400CL_2517_CountEconomypriGroups), loading it from AS400CL_2517_economypri_get_ep_groups_cur,
		// then closing the cursor and scrolling through the values in the array rather than scrolling through
		// the cursor. Performance should be essentially identical, and I didn't have to change anything in
		// the two database cursors - just use them a little differently.
		ExecSQL (	MAIN_DB, AS400CL_2517_CountEconomypriGroups,
					&NumEconomypriGroups,	END_OF_ARG_LIST			);

		if (SQLCODE != 0)
		{
			GerrLogMini (GerrId, "AS400CL_2517_CountEconomypriGroups returned SQLCODE %d.", SQLCODE);
			NumEconomypriGroups = 1500;	// Real value as of 23Sep2020 = 965.
		}

		EconomypriGroupList = malloc ((NumEconomypriGroups + 1) * sizeof(int));
		if (EconomypriGroupList == NULL)
		{
			GerrLogMini (GerrId, "Allocated %d Economypri groups - array %s NULL.", NumEconomypriGroups, (EconomypriGroupList == NULL) ? "IS" : "IS NOT");
			retval = ERR_NO_MEMORY;
			break;
		}


		DeclareAndOpenCursor (	MAIN_DB, AS400CL_2517_economypri_get_ep_groups_cur	);

		if (SQLERR_code_cmp (SQLERR_access_conflict) == MAC_TRUE)
		{
			retval = SB_DATA_ACCESS_CONFLICT;
			break;
		}
		else if (SQLERR_error_test () != MAC_OK)
		{
			retval = SB_LOCAL_DB_ERROR;
			break;
		}

		EconomypriGroupCounter = 0;

		// Loop through Economypri groups.
		do
		{
			FetchCursorInto (	MAIN_DB, AS400CL_2517_economypri_get_ep_groups_cur,
								&MasterGroupCode, END_OF_ARG_LIST						);

			if (SQLERR_code_cmp (SQLERR_end_of_fetch) == MAC_TRUE)
			{
				retval = SB_END_OF_FETCH;
				break;
			}
			else if (SQLERR_code_cmp (SQLERR_access_conflict) == MAC_TRUE)
			{
				retval = SB_DATA_ACCESS_CONFLICT;
				break;
			}
			else if (SQLERR_error_test () != MAC_OK)
			{
				retval = SB_LOCAL_DB_ERROR;
				break;
			}

			EconomypriGroupList [EconomypriGroupCounter++] = MasterGroupCode;
		}
		while (1);
		// Loop through groups in Economypri.

		CloseCursor (	MAIN_DB, AS400CL_2517_economypri_get_ep_groups_cur	);
		NumEconomypriGroups = EconomypriGroupCounter;
GerrLogMini (GerrId, "Loaded array with %d Economypri groups.", NumEconomypriGroups);


		for (EconomypriGroupCounter = 0; EconomypriGroupCounter < NumEconomypriGroups; EconomypriGroupCounter++)
		{
			MasterGroupCode = EconomypriGroupList [EconomypriGroupCounter];

			// This SELECT retrieves all non-deleted drugs that are either members of a given Economypri
			// group (indicated by the variable MasterGroupCode FETCHed from cursor get_ep_groups), or
			// else members of other Economypri groups that share at least one member with the same
			// "parent group" as at least one member of the given Economypri group. In other words, if
			// Economypri group 123 contains a drug with parent group code 999, and Economypri group
			// 456 also contains a drug with parent group code 999, all members of Economypri group 456
			// will be included in the spclty_drug_grp table as part of "master group" 123.
			//
			// DonR 06Dec2009: Deleted specialist_drug flag from selection criteria. Under the new rules,
			// drugs purchased with specialist participation will be used in granting further specialist
			// participation even if they are not specialist drugs (e.g. a non-generic drug sold with
			// doctor's or pharmacist's authorization in place of the generic specialist drug).
			// Also, eliminated drug and parent-group names from selects and from the spclty_drug_grp
			// table; nobody uses them or looks at them, and since the table will now have more rows than
			// it did before, making each row smaller will improve program performance.
			DeclareAndOpenCursor (	MAIN_DB, AS400CL_2517_drug_list_get_group_drugs_cur,
									&MasterGroupCode, &MasterGroupCode, END_OF_ARG_LIST		);

			if (SQLERR_code_cmp (SQLERR_access_conflict) == MAC_TRUE)
			{
				retval = SB_DATA_ACCESS_CONFLICT;
				break;
			}
			else if (SQLERR_error_test () != MAC_OK)
			{
				retval = SB_LOCAL_DB_ERROR;
				break;
			}

			// Loop through drugs and add to new version of Specialty Drug Group table.
			do
			{
				FetchCursorInto (	MAIN_DB, AS400CL_2517_drug_list_get_group_drugs_cur,
									&LargoCode, &DrugGroupCode, &ParentGroupCode, END_OF_ARG_LIST	);
//GerrLogMini (GerrId, "Fetched AS400CL_2517_drug_list_get_group_drugs_cur, SQLCODE = %d.", SQLCODE);

				if (SQLERR_code_cmp (SQLERR_end_of_fetch) == MAC_TRUE)
				{
					break;
				}
				else if (SQLERR_code_cmp (SQLERR_access_conflict) == MAC_TRUE)
				{
					retval = SB_DATA_ACCESS_CONFLICT;
					break;
				}
				else if (SQLERR_error_test () != MAC_OK)
				{
					retval = SB_LOCAL_DB_ERROR;
					break;
				}

				// DonR 03Jun2018: In order to simplify SELECTs against spclty_drug_grp, change
				// zeroes in parent_group_code to -999. (Note that the SELECT for get_group_drugs
				// ensures that we get only positive values for DrugGroupCode.)
				if (ParentGroupCode	==	0)
					ParentGroupCode	= -999;

				ExecSQL (	MAIN_DB, AS400CL_2517_INS_spclty_drug_grp_n,
							&MasterGroupCode,	&LargoCode,		&DrugGroupCode,
							&ParentGroupCode,	&SysDate,		&SysTime,
							END_OF_ARG_LIST										);

				if (SQLERR_code_cmp (SQLERR_access_conflict) == MAC_TRUE)
				{
					retval = SB_DATA_ACCESS_CONFLICT;
					break;
				}
				else if (SQLERR_error_test () != MAC_OK)
				{
					retval = SB_LOCAL_DB_ERROR;
					break;
				}

				n_uncommited_recs++;
			}
			while (1);
			// Loop through drugs in extended Economypri group.

			CloseCursor (	MAIN_DB, AS400CL_2517_drug_list_get_group_drugs_cur	);
//GerrLogMini (GerrId, "Closed AS400CL_2517_drug_list_get_group_drugs_cur, SQLCODE = %d.", SQLCODE);

			// If we've hit an error, stop looping through groups.
			if ((retval == SB_DATA_ACCESS_CONFLICT) || (retval == SB_LOCAL_DB_ERROR))
				break;
		}
		// Loop through groups in Economypri.


		// DonR 23Jan2020: Because this cursor is declared once and then repeatedly
		// opened and closed, it needs to be set "sticky". Since we don't actually
		// want it staying around after we done with it, we have to explicitly
		// free it.
		FreeStatement (	MAIN_DB, AS400CL_2517_drug_list_get_group_drugs_cur	);

		CommitWork (MAIN_DB);	// DonR 23Sep2020: Add interim COMMITs to reduce deadlocks.



		// DonR 21Jul2005: Add specialist drugs with non-zero Parent Group that are
		// not in the EconomyPri table to the spclty_drug_grp table,
		// so we can retrieve relevant drugs without bothering with UNION selects.
		MasterGroupCode = -999,	// DonR 03Jun2018: Insert nonsense value instead of zero.

		OpenCursor (	MAIN_DB, AS400CL_2517_drug_list_get_non_ep_drugs_cur	);

		if (SQLERR_code_cmp (SQLERR_access_conflict) == MAC_TRUE)
		{
			retval = SB_DATA_ACCESS_CONFLICT;
			break;
		}
		else if (SQLERR_error_test () != MAC_OK)
		{
			retval = SB_LOCAL_DB_ERROR;
			break;
		}

		// Loop through drugs and add to new version of Specialty Drug Group table.
		do
		{
			FetchCursorInto (	MAIN_DB, AS400CL_2517_drug_list_get_non_ep_drugs_cur,
								&LargoCode, &DrugGroupCode, &ParentGroupCode, END_OF_ARG_LIST	);

			if (SQLERR_code_cmp (SQLERR_end_of_fetch) == MAC_TRUE)
			{
				break;
			}
			else if (SQLERR_code_cmp (SQLERR_access_conflict) == MAC_TRUE)
			{
				retval = SB_DATA_ACCESS_CONFLICT;
				break;
			}
			else if (SQLERR_error_test () != MAC_OK)
			{
				retval = SB_LOCAL_DB_ERROR;
				break;
			}

			// DonR 03Jun2018: Cursor get_non_ep_drugs retrieves only stuff with a positive value for parent_group_code
			// and a zero value for group_code. This means that parent_group_code doesn't need any special treatment
			// here; and for group_code, we'll INSERT a hard-coded -999 (set into MasterGroupCode at the top of the loop)
			// instead of a hard-coded zero.
			ExecSQL (	MAIN_DB, AS400CL_2517_INS_spclty_drug_grp_n,
						&MasterGroupCode,	&LargoCode,		&DrugGroupCode,
						&ParentGroupCode,	&SysDate,		&SysTime,
						END_OF_ARG_LIST										);

			if (SQLERR_code_cmp (SQLERR_access_conflict) == MAC_TRUE)
			{
				retval = SB_DATA_ACCESS_CONFLICT;
				break;
			}
			else if (SQLERR_error_test () != MAC_OK)
			{
				retval = SB_LOCAL_DB_ERROR;
				break;
			}

			n_uncommited_recs++;
		}
		while (1);
		// Loop through drugs in extended Economypri group.

		CloseCursor (	MAIN_DB, AS400CL_2517_drug_list_get_non_ep_drugs_cur	);
		// Done adding non-EconomyPri drugs.


		// DonR 26Jul2020: In order to avoid (I hope!) database errors in
		// TR6001_READ_CheckPriorPurchaseToAllowDelivery, force a DB COMMIT at
		// this point; this should reduce the timespan where the spclty_drug_grp
		// table is inaccessible to almost zero. (I'm doing this both before
		// *and* after the two RENAME TABLE operations, just to be paranoid.
		CommitAllWork ();

		ExecSQL (	MAIN_DB, AS400CL_2517_RENAME_spclty_drug_grp_to_old	);

		if (SQLERR_code_cmp (SQLERR_access_conflict) == MAC_TRUE)
		{
			retval = SB_DATA_ACCESS_CONFLICT;
			break;
		}
		// DonR 04Nov2020: Changed the table-not-found test to one that
		// works for MS-SQL as well as Informix.
		else if (SQLERR_code_cmp (SQLERR_table_not_found) == MAC_TRUE)
		{
			// Current table doesn't exist - no need to do anything!
		}
		else if (SQLERR_error_test () != MAC_OK)
		{
			retval = SB_LOCAL_DB_ERROR;
			break;
		}

		ExecSQL (	MAIN_DB, AS400CL_2517_RENAME_spclty_drug_grp_n_to_current	);

		if (SQLERR_code_cmp (SQLERR_access_conflict) == MAC_TRUE)
		{
			retval = SB_DATA_ACCESS_CONFLICT;
			break;
		}
		else if (SQLERR_error_test () != MAC_OK)
		{
			retval = SB_LOCAL_DB_ERROR;
			break;
		}

		// DonR 26Jul2020: In order to avoid (I hope!) database errors in
		// TR6001_READ_CheckPriorPurchaseToAllowDelivery, force a DB COMMIT at
		// this point; this should reduce the timespan where the spclty_drug_grp
		// table is inaccessible to almost zero. (I'm doing this both before
		// *and* after the two RENAME TABLE operations, just to be paranoid.
		CommitAllWork ();


		// DonR 10Apr2011: Update preferred_largo field in drug_list to reflect current
		// contents of economypri. This allows us to avoid checking economypri in the drug-sale
		// transactions - see function find_preferred_drug() in SqlHandlers.ec.
		GerrLogMini (GerrId,"Updating drug_list/preferred_largo...\t");

		LastGroupCode = FirstGroupNbr = -1;

		// DonR 21Dec2011: To make more room for "real" generic-substitution groups, the boundary
		// for "therapoietic" groups is being moved from 600 to 800.
		// DonR 05Nov2014: Changing the 799/800 boundary to 999/1000. This is temporary, as the
		// code is being expanded to six digits. When the next phase comes in, the boundary will
		// be at 19999/20000.
		// DonR 30Jan2018: Use System Code instead of a range of Group Codes.
		// DonR 31Jan2018: Use new send_and_use_pharm flag instead of a range of Group Codes OR
		// the System Code. This allows us to locate all the table-specific logic for economypri
		// in Transaction 2054/4054 in As400UnixServer.
		// DonR 19Aug2021: Use a single DeclareAndOpenCursorInto call instead of declare + open.
		DeclareAndOpenCursorInto (	MAIN_DB, AS400CL_2517_economypri_get_pref_largo_cur,
									&DrugGroupCode,		&DrugGroupNbr,		&ItemSeq,
									&LargoCode,			END_OF_ARG_LIST						);

		if (SQLERR_code_cmp (SQLERR_access_conflict) == MAC_TRUE)
		{
			retval = SB_DATA_ACCESS_CONFLICT;
			break;
		}
		else if (SQLERR_error_test () != MAC_OK)
		{
			retval = SB_LOCAL_DB_ERROR;
			break;
		}

		// Loop through economypri and update drug_list as necessary.
		do
		{
			FetchCursor (	MAIN_DB, AS400CL_2517_economypri_get_pref_largo_cur	);

			if (SQLERR_code_cmp (SQLERR_end_of_fetch) == MAC_TRUE)
			{
				break;
			}
			else if (SQLERR_code_cmp (SQLERR_access_conflict) == MAC_TRUE)
			{
				retval = SB_DATA_ACCESS_CONFLICT;
				break;
			}
			else if (SQLERR_error_test () != MAC_OK)
			{
				retval = SB_LOCAL_DB_ERROR;
				break;
			}

			// Grab preferred Largo Code from the first entry for the group.
			if (DrugGroupCode != LastGroupCode)
			{
				LastGroupCode	= DrugGroupCode;
				FirstGroupNbr	= DrugGroupNbr;
				PrefLargoCode	= LargoCode;
			}

			// For all drugs with the first Group Number for this Group Code, set preferred_drug to zero -
			// there will be no substitution to perform, as they're already in the best category.
			// DonR 24Mar2015: Just to be on the safe side, set initial default value of 1 for
			// multi_pref_largo; later, we'll set it to zero for drugs with only a single preferred
			// drug in their group.
			if (DrugGroupNbr == FirstGroupNbr)
			{
				ExecSQL (	MAIN_DB, AS400CL_2517_UPD_drug_list_preferred_largo_to_zero,
							&LargoCode, END_OF_ARG_LIST										);
				// Don't check errors, as most of the time we'll get "no rows affected".
			}
			else
			{
				// We've found a non-preferred drug; update its preferred_largo field accordingly.
				ExecSQL (	MAIN_DB, AS400CL_2517_UPD_drug_list_preferred_largo_for_non_pref_drug,
							&PrefLargoCode, &LargoCode, &PrefLargoCode, END_OF_ARG_LIST				);
				// Don't check errors, as most of the time we'll get "no rows affected".
			}
		}
		while (1);
		// Loop through Economypri table to update preferred_largo in drug_list.

		CloseCursor (	MAIN_DB, AS400CL_2517_economypri_get_pref_largo_cur	);

		if ((retval == SB_DATA_ACCESS_CONFLICT) || (retval == SB_LOCAL_DB_ERROR))
			break;

		CommitWork (MAIN_DB);	// DonR 23Sep2020: Add interim COMMITs to reduce deadlocks.


		// DonR 23Mar2015: At this point, all "top of the list" preferred drugs have zero in
		// their preferred_largo field. Because we now look at each member's purchase history
		// to allow him/her to continue to receive the same generic drug as s/he bought last
		// time, we need to designate those "top of the list" preferred drugs that have
		// another preferred drug on the list as well. The result will be that "singleton"
		// preferred drugs will still have zero for preferred_largo, while "duplicate"
		// preferred drugs will always have a non-zero preferred_largo even if it's equal to
		// the drug's own Largo Code.
		ExecSQL (	MAIN_DB, AS400CL_2517_DROP_temp_pref_largos	);

		// Create a temporary table containing the largo codes of first-in-list preferred
		// drugs that have other preferred drugs in their group.
		ExecSQL (	MAIN_DB, AS500CL_2517_SELECT_INTO_temp_pref_largos	);

		SQLERR_error_test ();
		GerrLogMini (GerrId, "Created temp preferred-largo, SQLCODE = %d, %d largo codes.", SQLCODE, sqlca.sqlerrd[2]);

		// Alter these non-unique first-in-list preferred drugs so that they have a non-zero
		// preferred_largo value - equal to their own Largo Code, of course.
		ExecSQL (	MAIN_DB, AS400CL_2517_UPD_drug_list_preferred_largo_for_first_of_multiple	);

		SQLERR_error_test ();
		GerrLogMini (GerrId, "Updated duplicate preferred drugs, SQLCODE = %d.", SQLCODE);
		CommitWork (MAIN_DB);	// DonR 23Sep2020: Add interim COMMITs to reduce deadlocks.

		// For all drugs in groups that have only a single preferred drug (i.e. they have
		// a non-zero preferred_largo that is *not* in our temporary table), set the
		// multi_pref_largo flag to zero; this will disable history checking in
		// find_preferred_drug() and thus improve performance. Note that I've included
		// a second WHERE criterion that should be redundant - it's there out of pure
		// paranoia, in case the database fails to notice the previous UPDATE statement
		// that set preferred_largo = largo_code for non-unique first-in-list preferred
		// drugs.
		ExecSQL (	MAIN_DB, AS400CL_2517_UPD_drug_list_multi_pref_largo_clear	);

		GerrLogMini (GerrId, "Set multi_pref_largo to zero for single-preferred-drug groups, SQLCODE = %d, %d drugs updated.", SQLCODE, sqlca.sqlerrd[2]);

		ExecSQL		(	MAIN_DB, AS400CL_2517_DROP_temp_pref_largos	);
		CommitWork	(	MAIN_DB	);	// DonR 23Sep2020: Add interim COMMITs to reduce deadlocks.


		// DonR 03Oct2018 CR #13262: Update the new has_diagnoses flag in drug_list.
		ExecSQL		(	MAIN_DB, AS400CL_2517_UPD_drug_list_has_diagnoses_flip_true	);
		CommitWork	(	MAIN_DB	);	// DonR 23Sep2020: Add interim COMMITs to reduce deadlocks.

		ExecSQL		(	MAIN_DB, AS400CL_2517_UPD_drug_list_has_diagnoses_flip_false	);
		CommitWork	(	MAIN_DB	);	// DonR 23Sep2020: Add interim COMMITs to reduce deadlocks.


		// Make sure that preferred_largo is zero for all drugs not found in economypri.
		// DonR 21Dec2011: To make more room for "real" generic-substitution groups, the boundary
		// for "therapoietic" groups is being moved from 600 to 800.
		// DonR 05Nov2014: Changing the 799/800 boundary to 999/1000. This is temporary, as the
		// code is being expanded to six digits. When the next phase comes in, the boundary will
		// be at 19999/20000.
		// DonR 30Jan2018: Use System Code instead of a range of Group Codes.
		// DonR 31Jan2018: Use new send_and_use_pharm flag instead of a range of Group Codes OR
		// the System Code. This allows us to locate all the table-specific logic for economypri
		// in Transaction 2054/4054 in As400UnixServer.
		// DonR 04Feb2018: Also zero out new economypri_seq field, which is a copy of economypri/item_seq.
		ExecSQL (	MAIN_DB, AS400CL_2517_UPD_drug_list_clear_stuff_for_drugs_not_in_economypri	);

		// If we've already hit an error, don't mark the process has having been completed.
		if (SQLERR_code_cmp (SQLERR_no_rows_affected) != MAC_TRUE)
		{
			if (SQLERR_code_cmp (SQLERR_access_conflict) == MAC_TRUE)
			{
				retval = SB_DATA_ACCESS_CONFLICT;
				break;
			}
			else if (SQLERR_error_test () != MAC_OK)
			{
				retval = SB_LOCAL_DB_ERROR;
				break;
			}
		}

		CommitWork (MAIN_DB);	// DonR 23Sep2020: Add interim COMMITs to reduce deadlocks.

		// Finally, mark the process as complete in tables_update.
		strcpy (TableName, "specialist_drugs_updated");
		ExecSQL (	MAIN_DB, AS400CL_2517_UPD_tables_update,
					&SysDate, &SysTime, &TableName, END_OF_ARG_LIST	);

		// Check for errors again after updating tables_update.
		if (SQLERR_code_cmp (SQLERR_access_conflict) == MAC_TRUE)
		{
			retval = SB_DATA_ACCESS_CONFLICT;
			break;
		}
		else if (SQLERR_error_test () != MAC_OK)
		{
			retval = SB_LOCAL_DB_ERROR;
			break;
		}
	}
	while (0);
	// End of dummy loop.


	recs_to_commit = n_uncommited_recs;

	return (retval);
}
// End of 2517 (Create Specialist Drug Groups) handler.





// DonR 20Jul2016: Near-real-time processing of queued doctor-prescription
// status updates.
static int ProcessQueuedRxUpdates  ()
{
	static int	LastRunDate			= 0;
	static int	LastRunTime			= 0;
	static int	MinRetryInterval	= 5;
	int			LastPrIdDeleted		= 0;

	// Database variables
	// Doctor-prescription status update queue.
	int						Qmember_id;
	short					Qid_code;
	int						Qdoctor_id;
	long					Qvisit_number;
	int						Qdoctor_presc_id;
	short					Qline_number;
	int						Qlargo_prescribed;
	int						Qvalid_from_date;
	short					Qsold_status;
	short					Qclose_by_rounding;
	short					Qupd_units_sold;
	short					Qupd_op_sold;
	int						Qlast_sold_date;
	int						Qlast_sold_time;
	short					Qlinux_update_flag;
	int						Qlinux_update_date;
	int						Qlinux_update_time;
	int						Qqueued_date;
	int						Qqueued_time;
	short					Qreported_to_cds;

	// Prescription-deletion status update queue.
	int						DelPrID;
	int						DelPrLargo;


	// If less than MinRetryInterval (in seconds) has passed since we last ran this routine,
	// just return without doing anything.
	if ((LastRunDate == 0) || (diff_from_now (LastRunDate, LastRunTime) >= MinRetryInterval))
	{
		LastRunDate = GetDate ();
		LastRunTime = GetTime ();

		// DonR 18Jul2016: Add queue processing for doctor_presc status updates that failed (after retries)
		DeclareAndOpenCursorInto (	MAIN_DB, AS400CL_drx_status_queue_cur,
									&Qmember_id,			&Qid_code,				&Qdoctor_id,			&Qvisit_number,
									&Qdoctor_presc_id,		&Qline_number,			&Qlargo_prescribed,		&Qvalid_from_date,
									&Qsold_status,			&Qclose_by_rounding,	&Qupd_units_sold,		&Qupd_op_sold,
									&Qlast_sold_date,		&Qlast_sold_time,		&Qlinux_update_flag,	&Qlinux_update_date,
									&Qlinux_update_time,	&Qqueued_date,			&Qqueued_time,			&Qreported_to_cds,
									END_OF_ARG_LIST																					);
//GerrLogMini(GerrId, "Declared AS400CL_drx_status_queue_cur, SQLCODE = %d.", SQLCODE);

//		// DonR 18Jul2016: Add queue processing for doctor_presc status updates that failed (after retries)
//		// with some form of access-conflict error.
//		OpenCursor (	MAIN_DB, AS400CL_drx_status_queue_cur	);
//
		// No real error-trapping here - just log the error and return.
		if (SQLERR_error_test ())
		{
			CloseCursor (	MAIN_DB, AS400CL_drx_status_queue_cur	);
			return (-1);
		}

		while (1)
		{
			// Fetch drx_status_queue row.
			FetchCursor (	MAIN_DB, AS400CL_drx_status_queue_cur	);
//GerrLogMini(GerrId, "Fetched AS400CL_drx_status_queue_cur, SQLCODE = %d.", SQLCODE);

			if (SQLCODE != 0)
			{
				if (SQLERR_code_cmp (SQLERR_end_of_fetch) == MAC_TRUE)
				{
					break;
				}

				// No error-trapping - just give up and try again in five seconds.
				if (SQLERR_code_cmp (SQLERR_access_conflict) == MAC_TRUE)
				{
					break;
				}

				// No error-trapping - just log the error, give up, and try again in five seconds.
				if (SQLERR_error_test () == MAC_TRUE)
				{
					break;
				}
			}	// Non-zero SQLCODE on FETCH.

			ExecSQL (	MAIN_DB, AS400CL_UPD_doctor_presc,
						&Qsold_status,			&Qclose_by_rounding,
						&Qlast_sold_date,		&Qlast_sold_time,
						&Qupd_units_sold,		&Qupd_op_sold,
						&Qlinux_update_flag,	&Qlinux_update_date,
						&Qlinux_update_time,	&Qreported_to_cds,

						&Qvisit_number,			&Qdoctor_presc_id,
						&Qline_number,			END_OF_ARG_LIST			);
//GerrLogMini(GerrId, "Executed AS400CL_UPD_doctor_presc, SQLCODE = %d.", SQLCODE);

			GerrLogMini (GerrId, "RxUpdQueue: Update %ld/%6d/%3d: SQLCODE = %d, %d rows.", Qvisit_number, Qdoctor_presc_id, Qline_number, SQLCODE, sqlca.sqlerrd[2]);

			// If everything was OK, go ahead and delete this row from the queue table.
//			if ((SQLCODE == 0) || (SQLERR_code_cmp (SQLERR_no_rows_affected) == MAC_TRUE))
			if (SQLCODE == 0)
			{
				ExecSQL (	MAIN_DB, AS400CL_DEL_drx_status_queue	);

				if (SQLCODE != 0)
					GerrLogMini (GerrId, "RxUpdQueue: Delete %ld/%6d/%3d: SQLCODE = %d, %d rows.", Qvisit_number, Qdoctor_presc_id, Qline_number, SQLCODE, sqlca.sqlerrd[2]);
			}
		}	// While (1)

		CloseCursor (	MAIN_DB, AS400CL_drx_status_queue_cur	);

//
//		// DonR 28Jul2016: Run a copy of the drug-sale deletion queue handling routine here (same
//		// as we do in Transaction 2502) just to ensure fast response time.
//      CloseCursor		(	MAIN_DB, AS400CL_del_queue_cur					);	//Marianna 14Jun2020
//      FreeStatement	(	MAIN_DB, AS400CL_del_queue_cur					);	//Marianna 14Jun2020
//
//		DeclareCursorInto (	MAIN_DB, AS400CL_del_queue_cur,
//							&DelPrID, &DelPrLargo, END_OF_ARG_LIST	);
//GerrLogMini(GerrId, "Declared AS400CL_del_queue_cur, SQLCODE = %d.", SQLCODE);
//
//
//		LastPrIdDeleted	= 0;
//
//		OpenCursor (	MAIN_DB, AS400CL_del_queue_cur	);
//
//		// No real error-trapping here - just log the error and return.
//		if (SQLERR_error_test ())
//		{
//			CloseCursor (	MAIN_DB, AS400CL_del_queue_cur	);
//			return (-1);
//		}
//
//		while (1)
//		{
//			// Fetch drugsale_del_queue row.
//			FetchCursor (	MAIN_DB, AS400CL_del_queue_cur	);
//GerrLogMini(GerrId, "Fetched AS400CL_del_queue_cur, SQLCODE = %d.", SQLCODE);
//
//			if (SQLCODE != 0)
//			{
//				if (SQLERR_code_cmp (SQLERR_end_of_fetch) == MAC_TRUE)
//				{
//					break;
//				}
//
//				if (SQLERR_code_cmp (SQLERR_access_conflict) == MAC_TRUE)
//				{
//					break;
//				}
//
//				if (SQLERR_error_test () == MAC_TRUE)
//				{
//					break;
//				}
//			}	// Non-zero SQLCODE on FETCH.
//
//			// If the current item from the queue pertains to a prescription_drugs row, flag
//			// that row as deleted.
//			if (DelPrLargo != 0)
//			{
//				ExecSQL (	MAIN_DB, AS400CL_UPD_del_queue_prescription_drugs,
//							&DelPrID, &DelPrLargo, END_OF_ARG_LIST				);
//
//				GerrLogMini (GerrId, "DelQ2:    PD %d  Largo %5d , SQLCODE = %d, %d rows.", DelPrID, DelPrLargo, SQLCODE, sqlca.sqlerrd[2]);
//			}
//			else
//			{
//				SQLCODE = 0;
//			}
//
//			// We want to update the prescriptions row if EITHER this is the update that failed initially,
//			// OR we updated a prescription_drugs row and the drug sale is now entirely deleted. Thus we
//			// perform this update unconditionally, whether the queue row had a Largo Code or not.
//			if ((SQLCODE == 0) || (SQLERR_code_cmp (SQLERR_no_rows_affected) == MAC_TRUE))
//			{
//				// If we just marked this prescriptions row as deleted (i.e. because we deleted the last
//				// un-deleted drug in the sale), don't bother trying to mark it again.
//				if (DelPrID == LastPrIdDeleted)
//				{
//					GerrLogMini (GerrId, "DelQ2:    PR %d has already been marked as deleted.", DelPrID);
//				}
//				else
//				{
//					ExecSQL (	MAIN_DB, AS400CL_UPD_del_queue_prescriptions,
//								&DelPrID, &DelPrID, END_OF_ARG_LIST				);
//GerrLogMini (GerrId, "Executed AS400CL_UPD_del_queue_prescriptions:    PD %d  Largo %5d , SQLCODE = %d, %d rows.", DelPrID, DelPrLargo, SQLCODE, sqlca.sqlerrd[2]);
//
//					// Print the diagnostic only if it's got something vaguely interesting.
//					if ((DelPrLargo == 0) || (sqlca.sqlerrd[2] > 0) || (SQLCODE != 0))
//						GerrLogMini (GerrId, "DelQ2:    PR %d (Largo %5d), SQLCODE = %d, %d rows.", DelPrID, DelPrLargo, SQLCODE, sqlca.sqlerrd[2]);
//
//					// If we successfully marked a prescriptions row as deleted, remember it so we can
//					// avoid running the same SQL twice.
//					if ((SQLCODE == 0) && (sqlca.sqlerrd[2] == 1))
//						LastPrIdDeleted	= DelPrID;
//				}
//			}
//
//			// If everything was OK, go ahead and delete this row from the queue table.
//			if ((SQLCODE == 0) || (SQLERR_code_cmp (SQLERR_no_rows_affected) == MAC_TRUE))
//			if (SQLCODE == 0)
//			{
//				ExecSQL (	MAIN_DB, AS400CL_DEL_del_queue	);
//
//				if (SQLCODE != 0)
//					GerrLogMini (GerrId, "AS400CL_DEL_del_queue:    DQ %d  Largo %5d , SQLCODE = %d, %d rows.", DelPrID, DelPrLargo, SQLCODE, sqlca.sqlerrd[2]);
//			}
//		}	// While (1)
//
//		CloseCursor (	MAIN_DB, AS400CL_del_queue_cur	);


		CommitAllWork ();
//
//		glbInTransaction = 0;
	}	// At least MinRetryInterval seconds since last attempt.


	return(0);
}


/*=======================================================================
||									||
||				pipe_handler()				||
||									||
 =======================================================================*/
void	pipe_handler( int signo )
{
    restart_flg = 1;

    sigaction( SIGPIPE, &sig_act_pipe, NULL );
}

/*=======================================================================
||									||
||				alrm_handler()				||
||									||
 =======================================================================*/
void	alrm_handler( int signo )
{
    sigaction( SIGALRM, &sig_act_alrm, NULL );
}

/*=======================================================================
||									||
||				inform_handler()			||
||									||
 =======================================================================*/
void	inform_handler( int signo )
{
    printf("=========================================================\n"
	   " Here is current message number for as400client : <%d>\n"
	   "=========================================================\n",
	   message_num
	   );
    fflush(stdout);

    sigaction( SIGUSR1, &sig_act_inform, NULL );
}

/*=======================================================================
||									||
||				change_handler()			||
||									||
 =======================================================================*/
void	change_handler( int signo )
{
    printf("==================================================\n"
	   " Leaving message number for as400client : <%d>\n"
	   "==================================================\n",
	   message_num
	   );
    fflush(stdout);

    manual_change_of_tables = 1;

    sigaction( SIGUSR2, &sig_act_change, NULL );
}

/*=======================================================================
||									||
||				sleep_check()				||
||									||
 =======================================================================*/
void	sleep_check( int	seconds, int *system_down_flg )
{
    time_t	stop_time;

    stop_time = time( NULL ) + seconds;

    //
    // sleep for desired seconds & exit immediately on system going down
    //
    *system_down_flg = 0;

    while( time(NULL) < stop_time )
    {
		// DonR 26Apr2022: X25TimeOut is set (in the setup table) to 100 - which
		// is a ridiculously low value in microseconds. Instead, since this routine
		// is delaying for a non-fractional number of seconds, use a much larger,
		// hard-coded value of 250,000 = 1/4 of a second.
//		usleep( X25TimeOut  );
		usleep (250000);

		if( ToGoDown( PHARM_SYS ) )
		{
			*system_down_flg = 1;

			return;
		}

		if( manual_change_of_tables )
		{
			return;
		}

    }

}


void ReadIniFilePort ( void )  /*20020828*/
{
	int PortNumber;
	char *inifile = "/pharm/bin/inifileporto";	// DonR (per Yulia) 23SEP2002
	FILE *fp;


	pushname ("ReadIniFilePort");


	fp = fopen(inifile,"r");
	if ( fp == NULL )
	{
		//fp = fopen ("donr_diag", "w");
	    //fprintf(fp,"Can't open file %s to get port number.\n", inifile);
		//fclose(fp);
	    GerrLogReturn(GerrId,"Can't open file %s to get port number.\n", inifile);
		popname ();
		return;
			//printf ("\n can't open file inifile for port number\n ");
			//fflush(stdout);
			//exit(0);
	}
	fscanf(fp,"%d",&PortNumber) ;
	fclose(fp);
	//fp = fopen ("donr_diag", "w");
    //fprintf(fp,"Port number %d set from file.\n", PortNumber);
	//fclose(fp);
    GerrLogReturn(GerrId,"Port number %d set from file.\n", PortNumber);
	if( PortNumber == 0)
	{
		printf("\n can't read inifile port  \n");fflush(stdout);
//		exit(0);
	}
	else
	{
		printf ("\n port number  =  [%d] ",PortNumber); fflush(stdout);
		PORT_IN_AS400 = PortNumber;
		printf ("\n PortNumber = [%d]",PORT_IN_AS400);
		fflush(stdout);
	}

	popname ();
	return;
}






// Function call tracing - DonR 13OCT2002.

int pushname (char *name)

{
	int			rc = 0;


	// DonR 02May2005: Disable call-stack tracing.
#if 0

	if (tracefile == NULL)
	{
		tracefile = fopen (FTRACE_FILENAME, "wt");
	}


	if (fdepth >= FTRACE_MAXDEPTH)
	{
		if (tracefile != NULL)
		{
			fprintf (tracefile,
					 "\n\nERROR: Maximum call-tracing depth of %d exceeded by %d!\n\n",
					 FTRACE_MAXDEPTH, (1+ fdepth - FTRACE_MAXDEPTH));
			fflush (tracefile);
		}

		GerrLogReturn (GerrId,
					   "ERROR: Maximum call-tracing depth of %d exceeded by %d!",
					   FTRACE_MAXDEPTH, (1+ fdepth - FTRACE_MAXDEPTH));

		rc = -1;
	}

	else
	{
		fname [fdepth++] = name;

		if (ftracemode >= FTRACE_CALLS)
		{
			if (tracefile != NULL)
			{
				int i;

				for (i = 1; i < fdepth; i++)
					fprintf (tracefile, " ");

				fprintf (tracefile, "%s\n", name);

				fflush (tracefile);
			}
		}
	}

#endif

	return (rc);
}



int popname (void)

{
	int			rc = 0;


	// DonR 02May2005: Disable call-stack tracing.
#if 0

	if (tracefile == NULL)
	{
		tracefile = fopen (FTRACE_FILENAME, "wt");
	}


	if (fdepth <= 0)
	{
		if (tracefile != NULL)
		{
			fprintf (tracefile,
					 "\n\nERROR: Call-tracing stack underflow!\n\n");
			fflush (tracefile);
		}

		GerrLogReturn (GerrId,
					   "ERROR: Call-tracing stack underflow!");

		rc = -1;
	}

	else
	{
		fdepth--;

		if (ftracemode >= FTRACE_CALL_RTN)
		{
			if (tracefile != NULL)
			{
				int i;

				for (i = 0; i < fdepth; i++)
					fprintf (tracefile, " ");

				fprintf (tracefile, "%s returns.\n", fname [fdepth]);

				fflush (tracefile);
			}
		}
	}

#endif

	return (rc);
}

// DonR 13OCT2002 end.
