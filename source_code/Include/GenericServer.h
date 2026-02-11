#pragma once

#include <sys/time.h>
#include <stdio.h>
#include <Global.h>
#include <traceback.h>

typedef enum tag_bool
{
	false = 0,
	true
}
bool;                  // simple boolean typedef

#ifdef	MAIN
	char			LastSignOfLife			[512];
	int				reStart					= false;

	// Global variables.
	// Audit-table variables are global so they can be written to the database
	// by signal-processing routines.
	static char	*OutputBuffer			= NULL;
	static char	*RequestBuffer			= NULL;
	static char	MyShortName		[ 2 + 1];
	static char	MyServerName	[30 + 1];
	static int	MyPid;
	static int	message_count			=  0;
	int			accept_sock				= -1;
	int			caught_signal			=  0;
	int			ReqErrorCode			=  0;
	double		TotalProcTime			= 0.0;
	long		NumRequestsHandled		= 0;	// A regular int would really be adequate - just being paranoid!


	typedef enum
	{
		BEFORE_UPDATE,
		AFTER_UPDATE,
		CONN_HANGUP
	}
	SEND_STATES;

	SEND_STATES	send_state;


	struct sigaction	sig_act_pipe;
	struct sigaction	sig_act_terminate;

	// Modify and enable these lines if you need custom signal switches for SIGUSR1/2.
	//struct sigaction	sig_act_NihulTikrotEnable;
	//struct sigaction	sig_act_NihulTikrotDisable;

	// Global variables for sysparams - update as the table and the associated SQL are updated.
	short	min_refill_days;
	int		expensive_drug_prc;
	short	nihultikrotenabled;
	float	vat_percent;
	int		OverseasMaxSaleAmt;
	short	enablepplusmeishar;
	int		maxdailybuy_pvt;
	int		maxdailybuy_pplus;
	int		maxdailybuy_mac;
	short	max_future_presc;
	int		ishur_cycle_ovrlap;
	short	sale_ok_early_days;
	short	sale_ok_late_days;
	int		nocard_ph_maxvalid;
	int		nocard_ph_pvt_kill;
	int		nocard_ph_mac_kill;
	short	round_pkgs_2_pharm;
	short	round_pkgs_sold;
	short	del_valid_months;
	short	use_sale_tables;
	short	test_sale_equality;
	short	check_card_date;
	short	ddi_pilot_enabled;
	short	no_show_reject_num;
	short	no_show_hist_days;
	short	no_show_warn_code;
	short	no_show_err_code;
	short	svc_w_o_card_days;
	short	usage_memory_mons;
	short	max_order_sit_days;
	short	max_order_work_hrs;
	double	IngrSubstMaxRatio;
	short	max_unsold_op_pct;
	float	max_units_op_off;
	short	bakarakamutit_mons;
	int		max_otc_credit_mon;
	char	sick_disc_4_types	[20 + 1];
	char	vent_disc_4_types	[20 + 1];
	char	memb_disc_4_types	[20 + 1];
	int		MinNormalMemberTZ;
	int		pkg_price_for_sms;
	short	future_sales_max_hist;
	short	future_sales_min_hist;
	short	mac_pharm_max_otc_op_per_day;
	short	EnableSecretInvestigatorLogic;
	short	DarkonaiMaxHishtatfutPct; 

#else
	extern char		LastSignOfLife			[];
	extern int		reStart;

	// Global variables for sysparams - update as the table and the associated SQL are updated.
	extern short	min_refill_days;
	extern int		expensive_drug_prc;
	extern short	nihultikrotenabled;
	extern float	vat_percent;
	extern int		OverseasMaxSaleAmt;
	extern short	enablepplusmeishar;
	extern int		maxdailybuy_pvt;
	extern int		maxdailybuy_pplus;
	extern int		maxdailybuy_mac;
	extern short	max_future_presc;
	extern int		ishur_cycle_ovrlap;
	extern short	sale_ok_early_days;
	extern short	sale_ok_late_days;
	extern int		nocard_ph_maxvalid;
	extern int		nocard_ph_pvt_kill;
	extern int		nocard_ph_mac_kill;
	extern short	round_pkgs_2_pharm;
	extern short	round_pkgs_sold;
	extern short	del_valid_months;
	extern short	use_sale_tables;
	extern short	test_sale_equality;
	extern short	check_card_date;
	extern short	ddi_pilot_enabled;
	extern short	no_show_reject_num;
	extern short	no_show_hist_days;
	extern short	no_show_warn_code;
	extern short	no_show_err_code;
	extern short	svc_w_o_card_days;
	extern short	usage_memory_mons;
	extern short	max_order_sit_days;
	extern short	max_order_work_hrs;
	extern double	IngrSubstMaxRatio;
	extern short	max_unsold_op_pct;
	extern float	max_units_op_off;
	extern short	bakarakamutit_mons;
	extern int		max_otc_credit_mon;
	extern char		sick_disc_4_types	[];
	extern char		vent_disc_4_types	[];
	extern char		memb_disc_4_types	[];
	extern int		MinNormalMemberTZ;
	extern int		pkg_price_for_sms;
	extern short	future_sales_max_hist;
	extern short	future_sales_min_hist;
	extern short	mac_pharm_max_otc_op_per_day;
	extern short	EnableSecretInvestigatorLogic;
	extern short	DarkonaiMaxHishtatfutPct; 
#endif


// For the Generic Microservice Server, include the delay between retries as
// part of the Conflict_Test macro. This means we don't need to include the
// delay explicitly in application code - Conflict_Test takes care of it.
#define Conflict_Test(r)									\
if (SQLERR_code_cmp (SQLERR_access_conflict) == MAC_TRUE)	\
{															\
	SQLERR_error_test ();									\
	r = MAC_TRUE;											\
	usleep (AccessConflictTime);							\
	break;													\
}

#define NO_ERROR			 0
#define NO_JSON_INPUT		-1
#define JSON_PARSING_ERROR	-2
#define NO_JSON_OUTPUT		-3

#define	NORMAL_EXIT			0
#define	ENDLESS_LOOP		1


// Function prototypes.
void		ClosedPipeHandler	(int	signo);
void		TerminateHandler	(int	signo);
int			TerminateProcess	(int	signo, int in_loop);

// Modify and enable these lines if you need custom signal switches for SIGUSR1/2.
// void		NihulTikrotEnableHandler		(int	signo);
// void		NihulTikrotDisableHandler		(int	signo);

// Take the incoming (text) request, parse it into JSON, invoke the processing
// routine, and output the response.
int			HandleRequest		(char	*input_buf, int	input_size);

// Update the Microservice Server Audit table.
int			UpdateAuditTable	(short EndSignal);

// Process command-line parameters.
static int	ProcessCmdLine		(int argc, char* argv[]);


// Function code for routines that should not need to be altered
// for different microservice servers.
#ifdef	MAIN

/*=======================================================================
||																		||
||			     UpdateAuditTable (EndSignal)							||
||																		||
 =======================================================================*/
int UpdateAuditTable (short EndSignal)
{
	int		tries;
	int		reStart;
	int		SysDate;
	int		SysTime;
	double	AverageProcTime;

	SysDate			= GetDate ();
	SysTime			= GetTime ();
	AverageProcTime	= (NumRequestsHandled > 0) ? (TotalProcTime / (double)NumRequestsHandled) : 0.0;
	SQLCODE			= 0;	// Default in case we're compiling with DATABASE_NEEDED set FALSE.

#if DATABASE_NEEDED
	// Retry loop.
    for (tries = 0, reStart = MAC_TRUE; tries < SQL_UPDATE_RETRIES && reStart == MAC_TRUE; tries++)
    {
		// Inner dummy loop to support the Conflict_Test() macro.
		do
		{
			// Unless we hit a DB contention error, we want to execute our SQLs only once -
			// so we set reStart FALSE. If Conflict_Test() detects a contention error, it sets
			// reStart TRUE and we automatically retry the SQL.
			reStart = MAC_FALS;

			ExecSQL (	MAIN_DB, update_microservice_server_audit,
						&SysDate,				&SysTime,				&ReqErrorCode,
						&NumRequestsHandled,	&AverageProcTime,		&EndSignal,

						&MyShortName,			&MyPid,					END_OF_ARG_LIST		);

			if (SQLERR_code_cmp (SQLERR_no_rows_affected) == MAC_TRUE)
			{
				ExecSQL (	MAIN_DB, insert_microservice_server_audit,
							&MyShortName,		&MyPid,					&ServerType,
							&MyServerName,		&SysDate,				&SysTime,
							&ReqErrorCode,		&NumRequestsHandled,	&AverageProcTime,
							&EndSignal,			END_OF_ARG_LIST								);
			}	// UPDATE didn't find anything to update.

			Conflict_Test (reStart);

			SQLERR_error_test ();
		}
		while (0);
	}	// Retry loop.

	CommitAllWork ();
#endif	// DATABASE_NEEDED.

	return (SQLCODE);
}



/*=======================================================================
||																		||
||			     HandleRequest ()										||
||																		||
 =======================================================================*/

// This function should return 0 when everything is OK, a negative error
// code when the incoming request is missing or faulty (or there is another
// problem that prevents the request-processing routine from creating JSON
// output), or a positive error code when there is valid JSON output but
// there is a significant application-level error code.

int	 HandleRequest	(	char	*input_buf,
						int		input_size		)
{
	cJSON		*JSON_request			= NULL;
	cJSON		*JSON_response			= NULL;
	char		*JSON_ErrPtr			= NULL;
	int			ErrorCode				= NO_ERROR;	// I.e. zero.
	int			OutputSize				= 0;

	// NOTE: buf (length 128,000 bytes), state (integer), and len (integer)
	// are defined in global.h.

	// We have not yet processed the current request.
	send_state = BEFORE_UPDATE;

	if (*input_buf == '{')
	{
		JSON_request = cJSON_Parse (input_buf);
		if (JSON_request == NULL)
		{
			ErrorCode = JSON_PARSING_ERROR;
			JSON_ErrPtr = (char *)cJSON_GetErrorPtr ();
			GerrLogMini (GerrId, "cJSON_Parse() error before %s.", (JSON_ErrPtr == NULL) ? "NULL" : JSON_ErrPtr);
		}
		else
		{
//			sprintf (LastSignOfLife, "About to call process_JSON_request.");

			// If we get here, we managed to parse a JSON object from the incoming request.
			// Pass that object to a processing routine, which should return two things:
			// a numeric error code (using the standard C convention that 0 == no error)
			// and a pointer to a JSON object that the processing routine has allocated
			// and populated. Note that only "significant" errors should be returned, since
			// a non-zero ErrorCode value will be logged.
			ErrorCode = process_JSON_request (JSON_request, &JSON_response);
		}
	}
	else
	{
			ErrorCode = NO_JSON_INPUT;
	}


	// If we have a non-NULL JSON response, print it to the output buffer;
	// otherwise set the output buffer to length zero. Set the formatting
	// flag FALSE, so we won't take up memory with extra tabs - we're not
	// interested in pretty output here.
	if (JSON_response != NULL)
	{
		cJSON_PrintPreallocated (JSON_response, OutputBuffer, OutputBufferSize, 0);

		// cJSON_PrintPreallocated() does *not* give an indicator of how much
		// output it created - so our only option is to use strlen() to tell
		// us the output size to send back to the calling communications server.
		OutputSize = strlen (OutputBuffer);

		// In real life, we should always create output buffers a little larger than we
		// really need. For debugging purposes, print a diagnostic if we didn't allocate
		// enough. (Note that we may hit a segmentation fault first!)
		if (OutputSize > OutputBufferSize)
			GerrLogMini (GerrId, "OUTPUT BUFFER OVERFLOW! Allocated %d, used %d.", OutputBufferSize, OutputSize);
	}
	else
	{
		// Set OuputBuffer to length zero; at least for now, let's not bother
		// clearing the whole buffer.
		OutputBuffer [0]	= (char)0;
		OutputSize			= 0;	// Redundant, but good practice to be paranoid.

		// If we haven't already set an error code for a missing or invalid
		// JSON request, set a special error code for missing JSON output.
		// Note, though, that this should never really happen: process_JSON_request()
		// should always create an output JSON object, even if it's only to report
		// an error.
		if (ErrorCode == NO_ERROR)
		{
			ErrorCode = NO_JSON_OUTPUT;
		}
	}	// JSON response object is NULL.


	// Send_state is set to CONN_HANGUP by the closed-pipe signal handler
	// only if the pipe was closed while a request was pending.
    if (send_state == CONN_HANGUP)	// Pipe to PharmWebServer.exe was closed while a request was pending.
    {
		GerrLogMini (GerrId, "Pipe to PharmWebServer was closed!");
		CloseSocket (accept_sock);
		accept_sock = -1;
		return (CONN_END);
    }

	// Set send_state to indicate that we're done with the current request.
    send_state = AFTER_UPDATE;


	// Send output to the calling communications server.
	//
	// NOTE: Unlike the older code in SqlServer.c, here we are using the
	// traditional C standard that 0 == no error and any non-zero return
	// code represents some kind of error. More specifically, negative
	// error codes represent missing or unparsable JSON, and positive
	// error codes represent appication-level errors (e.g. database error,
	// and anything else that might go wrong while dealing with a request).
	//
	// Also note that in general, process_JSON_request() should return a
	// valid JSON object, even if it's only to report an error; OutputBuffer
	// should be empty only when there was no parsable JSON request at all.
	if (OutputBuffer [0] == (char)0)
	{
		sprintf (OutputBuffer, "{\"HTTP_response_status\": 404, \"error_code\": %d}", ErrorCode);
		OutputSize = strlen (OutputBuffer);
	}


	// Because "microservices" need to run as quickly as possible, we are always writing
	// output to memory rather than to a disk file. Accordingly, ANSWER_IN_BUFFER is
	// hard-coded here rather than using a variable. Note that "buf", "state", and "len"
	// are defined in global.h.

	// Write output type to socket.
    len		= sprintf (buf, "%d", ANSWER_IN_BUFFER);
	state	= WriteSocket (accept_sock, buf, len, 0);

	// If that worked, send the rest of the output to socket.
	// (Note that ERR_STATE is another global.h thing.)
	if (!ERR_STATE (state))
	{
		// Write actual transaction response to socket.
		state = WriteSocket (accept_sock, OutputBuffer, OutputSize, 0);
    }


	// Close the TCP socket.
    CloseSocket (accept_sock);
    accept_sock = -1;

	// If we failed to send either the output type or the actual output to
	// the calling communications server, log the error.
    if (ERR_STATE (state))
    {
		// LogCommErr is declared in global.h, and comes from the setup table(s).
		if (LogCommErr == 1)
		{
			GerrLogMini (GerrId, "COMM ERROR - can't send return message to PharmWebServer.");
		}
    }


	// Because the "generic" server code doesn't know anything about the content of
	// the requests being handled or the responses given, any transaction-level
	// log messages should be handled within process_JSON_request().


	// De-allocate cJSON objects.
	if (JSON_request != NULL)
		cJSON_Delete (JSON_request);
	if (JSON_response != NULL)
		cJSON_Delete (JSON_response);

    return (ErrorCode);
}


/*=======================================================================
||																		||
||			     ClosedPipeHandler ()									||
||																		||
 =======================================================================*/
void	 ClosedPipeHandler (int signo)
{
	sigaction (SIGPIPE, &sig_act_pipe, NULL);

	// Do *not* terminate the program when we get a closed pipe error.
	caught_signal = 0;	// Instead of setting it to signo.


	if (accept_sock != -1)
	{
		CloseSocket (accept_sock);

		accept_sock == -1;

		// Reset process if already updated database.
		if (send_state == AFTER_UPDATE)
		{
			// Don't log an error here - it's not a significant issue.
			// GerrLogMini (GerrId, "\nPID %d got hung-up by PharmWebServer.", MyPid);
		}
		else
		{
			GerrLogReturn (GerrId, "Got hung-up by PharmWebServer -- reset connection.");

			send_state = CONN_HANGUP;
		}

	}
	else
	{
		GerrLogReturn (GerrId, "Got hung up but no connection was active -- strange & spooky!");
	}
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


	// DonR 13Feb2020: Add special handling for segmentation faults caused by invalid
	// column/parameter pointers passed to ODBC database-interface routines. In this
	// case we just set a status variable to indicate an invalid pointer, then perform
	// a siglongjump to go back to the pointer-testing function ODBC_IsValidPointer in
	// macODBC.h.
	if ((signo == SIGSEGV) && (ODBC_ValidatingPointers))
	{
		// Segmentation fault means that the pointer we're checking is *not* OK.
		ODBC_PointerIsValid = 0;

//		// Reset signal handling for the segmentation fault signal.
//		sigaction (SIGSEGV, &sig_act_terminate, NULL);
//
		// Go back to the point just before we forced the segmentation-fault error.
		siglongjmp (BeforePointerTest, 1);
	}

	// We don't want to copy the signal caught into caught_signal if it was a "deliberate"
	// segmentation fault trapped when we tested ODBC parameter pointers - so I moved
	// these two lines after the pointer-validation block.
	caught_signal	= signo;
	time_now		= GetTime();


	// DonR 04Jan2004: This morning, two instances of this process went into an endless loop
	// of division-by-zero errors - presumably because Signal 8 was trapped inside a loop
	// somewhere that wasn't smart enough to break out of itself or to avoid the problem
	// entirely. To prevent future occurrences of this type, detect endless loops and
	// terminate the process "manually".
	if ((signo == last_signo) && (time_now == last_called))
	{
		GerrLogMini (GerrId, "PID %d aborting endless loop (signal %d). Terminating process.", MyPid, signo);

		TerminateProcess (signo, ENDLESS_LOOP);
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
			RollbackAllWork ();	// Any pending work is incomplete and needs to be undone.
			sig_description = "floating-point error - probably division by zero";
			break;

		case SIGSEGV:
			RollbackAllWork ();	// Any pending work is incomplete and needs to be undone.
			sig_description = "segmentation error";
			break;

		case SIGTERM:
			sig_description = "user-requested program shutdown";
			break;

		default:
			sig_description = "check manual page for SIGNAL";
			break;
	}
	
	GerrLogMini (GerrId,
				 "\nPID %d received Signal %d (%s). Terminating process.",
				 MyPid, signo, sig_description);

	// DonR 14Apr2015: Added a new global message buffer to help track down segmentation faults and
	// other mysterious, fatal errors.
//	if (strlen (LastSignOfLife) > 0)	// TEST VERSION!
	if ((signo != SIGTERM) && (strlen (LastSignOfLife) > 0))
	{
		GerrLogMini (GerrId,
						"\nPID %d last sign of life:\n===============================\n%s\n===============================\n",
						MyPid, LastSignOfLife);
	}
}




int TerminateProcess (int signo, int in_loop)

{
	int		SysDate;
	int		SysTime;
	short	EndSignal;


	SysDate		= GetDate ();
	SysTime		= GetTime ();
	EndSignal	= (in_loop == ENDLESS_LOOP) ? (9800 + signo) : signo;


#if DATABASE_NEEDED

	// Update the Microservice Server Audit table - including the regular
	// statistics variables as well as the signal number that ended the
	// program.
	UpdateAuditTable (EndSignal);

	// Make sure we're not leaving open transactions around!
	CommitAllWork ();

	// Disconnect from database.
	SQLMD_disconnect	();

	// Probably no real point in checking for errors here - but it's harmless, I suppose.
	SQLERR_error_test	();
#endif


	// DonR 06Jul2011: In case of segmentation fault, dump called-function diagnostics to file.
	if (in_loop == ENDLESS_LOOP)
		TRACE_DUMP ();

	// Exit child process.
	ExitSonProcess ((signo == 0) ? MAC_SERV_SHUT_DOWN : MAC_EXIT_SIGNAL);
}


//===========================================================================
//
//								ProcessCmdLine()
//
//---------------------------------------------------------------------------
// Description: Process command line
// Return type: int - 
//   Argument : int argc - 
//   Argument : char* argv[] - 
//===========================================================================
int ProcessCmdLine (int argc, char* argv[])
{
    int		c;
    int		errflag = 0;
	
#define ARGS    "dDp:P:m:M:r:R"
	
    retrys = 0;
    optarg = 0;
    while (!errflag && -1 != (c = getopt(argc, argv, ARGS)))
    {
		switch (c)
		{
		// Support option -rRETRIES for rerun by father process
		case 'r'	       :
		case 'R'	       :
			retrys = atoi(optarg);
			break;
		case 'd'	       :
		case 'D'	       :
//			debug++;
			break;
		case 'p'	       :
		case 'P'	       :
//			ClientPort   = atoi(optarg);
			break;
			
		case 'm'	       :
		case 'M'	       :
			break;

		default:
			errflag++;
			break; 		// GAL 03/11/2013
		}
    }
	
    if (errflag)
    {
		GerrLogMini (GerrId, "Illegal command-line option '%c'", (char)c);
		exit(1);
    }
    return 0;
}






#endif