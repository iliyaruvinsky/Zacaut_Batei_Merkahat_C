// TikrotRPC.c - Call DB2 stored procedure for "Nihul Tikrot"

#define _TIKROTRPC_C_
static  char *GerrSource = __FILE__;

#include "/usr/local/include/sql.h"
#include "/usr/local/include/sqlext.h"
#include "TikrotRPC.h"
#include "Global.h"
#include "GenSql.h"

void TestTikraCall ();

#define SQL_WORKED(R)	((R == SQL_SUCCESS) || (R == SQL_SUCCESS_WITH_INFO))
#define SQL_FAILED(R)	((R != SQL_SUCCESS) && (R != SQL_SUCCESS_WITH_INFO))

static short	InitializeFirstTime = 1;


int CallTikrotSP (char *Header_in,
				  char *CurrentSale_in,
				  char *PriorSales_in,
				  char **Header_out,
				  char **Tikrot_out,
				  char **CurrentSale_out,
				  char **Coupons_out)
{
	SQLRETURN		rc;
	char			DummyInputArg	[2];
	short			NumParams;			// Used to validate that the RPC statement is in fact still in PREPAREd state.
	static char		HeaderRtn		[RPC_OUT_HEADER_LEN		+ 1];
	static char		TikrotRtn		[RPC_OUT_TIKROT_LEN		+ 1];
	static char		CurrentSaleRtn	[RPC_OUT_CURRSALE_LEN	+ 1];
	static char		CouponsRtn		[RPC_OUT_COUPONS_LEN	+ 1];
	struct timeval	StartRPC;
	struct timeval	EndRPC;
	long			RPC_msec;


	// Set first Tikrot RPC time marker (if we're not in recursion).
	if (!RPC_Recursed)
		gettimeofday (&EventTime[EVENT_CALL_RPC_START], 0);

	// DonR 17Jun2020: Just in case ODBC doesn't like constants, make
	// DummyInputArg a "real" variable.
	strcpy (DummyInputArg, "");

	// Set output pointers equal to static output buffers.
	*Header_out			= &HeaderRtn		[0];
	*Tikrot_out			= &TikrotRtn		[0];
	*CurrentSale_out	= &CurrentSaleRtn	[0];
	*Coupons_out		= &CouponsRtn		[0];

	// Initialize output buffers.
	memset (HeaderRtn,		(char)0, RPC_OUT_HEADER_LEN			+ 1);
	memset (TikrotRtn,		(char)0, RPC_OUT_TIKROT_LEN			+ 1);
	memset (CurrentSaleRtn,	(char)0, RPC_OUT_CURRSALE_LEN		+ 1);
	memset (CouponsRtn,		(char)0, RPC_OUT_COUPONS_LEN		+ 1);


	// If necessary, initialize the ODBC connection.
 	if (!RPC_Connected)
 	{
		rc = RPC_Initialize ();
		if (rc != 0)
		{
			GerrLogMini (GerrId, "InitializeRPC failed, rc = %d.", rc);
			RPC_Cleanup ();
			return rc;
		}
//GerrLogMini (GerrId, "CallTikrotSP: Initialize returns %d.", rc);
 	}

// if (!TikrotProductionMode) GerrLogMini (GerrId, "CallTikrotSP binding input params.");

	// Bind the input parameters for the stored procedure.
	do
	{
		rc = SQLBindParameter (RPC_hstmt, 1, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_CHAR,
							   RPC_INP_DUMMY_LEN, 0, DummyInputArg, strlen (DummyInputArg), NULL);
		if (SQL_FAILED (rc))
		{
			GerrLogMini (GerrId, "PID %d: RPC SQLBindParameter #1 (input) failed, rc = %d, re-PREPARE.", (int)getpid(), rc);

			// DonR 09Jun2020: For the first bound parameter, if the BIND failed try
			// cleaning up the ODBC stuff, re-initializing, and re-BINDing.
			RPC_Cleanup ();

			rc = RPC_Initialize ();
			if (SQL_FAILED (rc))
			{
				GerrLogMini (GerrId, "Re-initializeRPC failed, rc = %d.", rc);
				RPC_Cleanup ();
				break; 
			}
			else
			{
				// Re-initializing apparently succeeded, so try the BIND again.
				rc = SQLBindParameter (RPC_hstmt, 1, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_CHAR,
									   RPC_INP_DUMMY_LEN, 0, DummyInputArg, strlen (DummyInputArg), NULL);
				if (SQL_FAILED (rc))
				{
					GerrLogMini (GerrId, "RPC SQLBindParameter #1 (input) after re-PREPARE: rc = %d.", rc);
					break;
				}
			}
		}

		rc = SQLBindParameter (RPC_hstmt, 2, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_CHAR,
							   RPC_INP_HEADER_LEN, 0, Header_in, strlen (Header_in), NULL);
		if (SQL_FAILED (rc))
		{
			GerrLogMini (GerrId, "RPC SQLBindParameter #2 (input) failed, rc = %d.", rc);
			break; 
		}

		rc = SQLBindParameter (RPC_hstmt, 3, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_CHAR,
							   RPC_INP_CURRSALE_LEN, 0, CurrentSale_in, strlen (CurrentSale_in), NULL);
		if (SQL_FAILED (rc))
		{
			GerrLogMini (GerrId, "RPC SQLBindParameter #3 (input) failed, rc = %d.", rc);
			break; 
		}

		rc = SQLBindParameter (RPC_hstmt, 4, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_CHAR,
							   RPC_INP_PRIORSALES_LEN, 0, PriorSales_in, strlen (PriorSales_in), NULL);
		if (SQL_FAILED (rc))
		{
			GerrLogMini (GerrId, "RPC SQLBindParameter #4 (input) failed, rc = %d.", rc);
			break; 
		}
	}
	while (0);

	if (SQL_FAILED (rc))
	{
		RPC_Cleanup ();
		return rc;
	}


// if (!TikrotProductionMode) GerrLogMini (GerrId, "CallTikrotSP binding output params.");
	// Bind output buffers.
	do
	{
		rc = SQLBindParameter (RPC_hstmt, 5, SQL_PARAM_OUTPUT, SQL_C_CHAR, SQL_CHAR,
							   RPC_OUT_HEADER_LEN, 0, HeaderRtn, RPC_OUT_HEADER_LEN + 1, NULL);
		if (SQL_FAILED (rc))
		{
			GerrLogMini (GerrId, "SQLBindParameter #5 (output) failed, rc = %d.\n", rc);
			break; 
		}

		rc = SQLBindParameter (RPC_hstmt, 6, SQL_PARAM_OUTPUT, SQL_C_CHAR, SQL_CHAR,
							   RPC_OUT_TIKROT_LEN, 0, TikrotRtn, RPC_OUT_TIKROT_LEN + 1, NULL);
		if (SQL_FAILED (rc))
		{
			GerrLogMini (GerrId, "SQLBindParameter #6 (output) failed, rc = %d.\n", rc);
			break; 
		}

		rc = SQLBindParameter (RPC_hstmt, 7, SQL_PARAM_OUTPUT, SQL_C_CHAR, SQL_CHAR,
							   RPC_OUT_CURRSALE_LEN, 0, CurrentSaleRtn, RPC_OUT_CURRSALE_LEN + 1, NULL);
		if (SQL_FAILED (rc))
		{
			GerrLogMini (GerrId, "SQLBindParameter #7 (output) failed, rc = %d.\n", rc);
			break; 
		}

		rc = SQLBindParameter (RPC_hstmt, 8, SQL_PARAM_OUTPUT, SQL_C_CHAR, SQL_CHAR,
							   RPC_OUT_COUPONS_LEN, 0, CouponsRtn, RPC_OUT_COUPONS_LEN + 1, NULL);
		if (SQL_FAILED (rc))
		{
			GerrLogMini (GerrId, "SQLBindParameter #8 (output) failed, rc = %d.\n", rc);
			break; 
		}
	}
	while (0);

	if (SQL_FAILED (rc))
	{
		RPC_Cleanup ();
		return rc;
	}


	// Set time marker for end-Initialization / beginning-RPC call.
	gettimeofday (&StartRPC, 0);
	EventTime[(RPC_Recursed) ? EVENT_CALL_RPC_RE_INIT : EVENT_CALL_RPC_INIT] = StartRPC;
// if (!TikrotProductionMode) GerrLogMini (GerrId, "CallTikrotSP executing statement.");

	// Call the stored procedure.
	rc = SQLExecute (RPC_hstmt);
// if (!TikrotProductionMode) GerrLogMini (GerrId, "CallTikrotSP SQLExecute returned %d.", rc);

	// Set time marker for SQLExecute, and copy to time marker 3 (since we normally
	// don't have to retry the SQLExecute call).
	gettimeofday (&EndRPC, 0);
	EventTime[(RPC_Recursed) ? EVENT_CALL_RPC_RETRY : EVENT_CALL_RPC_EXEC] = EndRPC;

	if (!RPC_Recursed)
	{
		EventTime[EVENT_CALL_RPC_RE_INIT] = EventTime[EVENT_CALL_RPC_RETRY] = EventTime[EVENT_CALL_RPC_EXEC];
	}

	// DonR 07Apr2021: Add a log message if we got a very slow response from the RPC call.
	RPC_msec = DiffTime (StartRPC, EndRPC);
	if (RPC_msec >= 5000)	// = 5 seconds
	{
		GerrLogMini (GerrId, "Nihul Tikrot SQLExecute() took %d milliseconds. Recursed = %d, rc = %d.", RPC_msec, RPC_Recursed, rc);
	}

	// DonR 01Aug2010: Added retry (with re-initialization) in the case of failure-to-communicate.
	if (rc == SQL_ERROR)
	{
		if (!RPC_Recursed)
		{
//			GerrLogMini (GerrId, "SQLExecute first attempt failed - retrying.");  DonR 08Aug2016: Suppressed logging of daily reconnect.

			usleep (500000);	// DonR 02Aug2020: Wait for 500 milliseconds before retry.
			RPC_Cleanup ();		// Force re-initialization of everything.
			RPC_Recursed = 1;	// Set ON to avoid endless recursion.

			// Since the return buffers are static, we shouldn't have to worry about aliasing.
			rc = CallTikrotSP (Header_in,
							   CurrentSale_in,
							   PriorSales_in,
							   Header_out,
							   Tikrot_out,
							   CurrentSale_out,
							   Coupons_out);

			RPC_Recursed = 0;	// Set OFF since we're back in non-recursive state.
		}
//		else
//		{
//			GerrLogMini (GerrId, "SQLExecute second attempt failed!");
//		}
	}	// Error communicating with Nihul Tikrot.

// if (!TikrotProductionMode) GerrLogMini (GerrId, "CallTikrotSP getting ready to return.", rc);

    if (SQL_WORKED (rc))
	{
		// Just to keep things neat and compact, strip trailing spaces from output buffers.
		strip_trailing_spaces (HeaderRtn);
		strip_trailing_spaces (TikrotRtn);
		strip_trailing_spaces (CurrentSaleRtn);
		strip_trailing_spaces (CouponsRtn);
	}
	else
    {
		// DonR 02Aug2020: Log the error only if the *second* attempt fails.
		if (RPC_Recursed)
			GerrLogMini (GerrId, "SQLExecute second attempt failed, rc = %d.", rc);
		RPC_Cleanup ();
    }

	// DonR 28Mar2017: In Test and QA, we seem to be getting return codes of 1 (= OK with info)
	// from SQLExecute. We want to translate 1 to zero, so the calling routine knows that the
	// Nihul Tikrot call was actually successful.
	return (SQL_WORKED (rc)) ? 0 : rc;
		
} // CallTikrotSP



// "Public" shell function to allocate variables and connect to ODBC database.
int InitTikrotSP (void)
{
	int rc = 0;	// Default in case we're already connected.

	// If necessary, initialize the ODBC connection.
 	if (!RPC_Connected)
 	{
		rc = RPC_Initialize ();
		if (rc != 0)
		{
			GerrLogMini (GerrId, "InitTikrotSP: InitializeRPC failed, rc = %d.", rc);
			RPC_Cleanup ();
		}
GerrLogMini (GerrId, "InitTikrotSP: Initialize returns %d.", rc);
 	}
else
GerrLogMini (GerrId, "InitTikrotSP: Already connected to database.");

	return rc;
}


// Allocate the environment, connection, statement handles
int RPC_Initialize()
{
	int				res;
	FILE			*ConfigFile = NULL;
	char			buf				[256];
	char			szStoredProc	[256];
	char			*ConnectResult;
	int				L;
 

	if (!RPC_Allocated)
	{
		// Allocate an environment handle.
		// NOTE: SQLAllocEnv is now deprecated, and should be replaced by a call to
		// SQLAllocHandle with appropriate arguments. (DonR 31Oct2019)
		res = SQLAllocEnv (&RPC_henv);
		if (SQL_FAILED (res))
		{
			GerrLogMini (GerrId,
						 "TikrotRPC: Unable to allocate environment handle (ret = %d).",
						 res);
			return (1);
		}

		// Allocate a connection handle.
		// NOTE: SQLAllocConnect is now deprecated, and should be replaced by a call to
		// SQLAllocHandle with appropriate arguments. (DonR 31Oct2019)
		res = SQLAllocConnect (RPC_henv, &RPC_hdbc);
		if (SQL_FAILED (res))
		{
			GerrLogMini (GerrId,
						 "TikrotRPC: Unable to allocate connection handle (ret = %d).",
						 res);
			RPC_Cleanup ();
			return (1);
		}
	}	// RPC_Allocated == 0.


	// Read configuration file. By default, we're in "test" mode.
	RPC_DSN					= "ASTEST";
	TikrotProductionMode	= 0;

	ConfigFile = fopen ("/pharm/bin/NihulTikrot.cfg", "r");
	if (ConfigFile != NULL)
	{
		fgets (buf, 255, ConfigFile);
		L = strlen (buf) - 1;
		if (L >= 0)
		{
			if (buf[L] == '\n')
				buf[L] = (char)0;
		}
		if (!strcasecmp (buf, "PRODUCTION"))
		{
			RPC_DSN					= "AS400";
			TikrotProductionMode	= 1;
		}
		fclose (ConfigFile);
	}
	

	// Connect to the database.
	res = SQLConnect (RPC_hdbc, RPC_DSN, SQL_NTS, RPC_DSN_userid, SQL_NTS, RPC_DSN_password, SQL_NTS);

	// Log connection parameters and result.
	switch (res)
	{
		case SQL_SUCCESS:			ConnectResult = "OK";			break;
		case SQL_SUCCESS_WITH_INFO:	ConnectResult = "OK w/info";	break;
		default:					ConnectResult = "FAILED";		break;
	}

	// DonR 08Aug2016: Suppress logging for daily reconnect - unless the reconnect failed.
	if ((InitializeFirstTime) || (SQL_FAILED (res)))
	{
		GerrLogMini (GerrId, "Connect to %s/%s, Prod = %d, res = %d (%s).",
					 RPC_DSN, RPC_DSN_userid, TikrotProductionMode, res, ConnectResult);
	}

	InitializeFirstTime = 0;	// From now on, we will log the connection attempt only if it fails.

	if (SQL_FAILED (res))
	{
//		GerrLogMini (GerrId, "TikrotRPC: Unable to connect to datasource (ret = %d).", res);
		RPC_Cleanup ();
		return (1);
	}


#if 0
			// DonR 03Mar2021: Add setting of Lock Timeout and Deadlock Priority for MS-SQL.
			// Allocate a statement handle - works only after connecting to database.
			result = SQLAllocStmt (Database->HDBC, &SetTimeout_hstmt);
			if (SQL_FAILED (result))
			{
				GerrLogMini (GerrId, "Unable to allocate statement handle for LOCK_TIMEOUT (result = %d).\n", result);
				CleanupODBC (Database);
				break;
			}

			// Set up the command.
			sprintf (StatementBuffer, "SET LOCK_TIMEOUT %d", LOCK_TIMEOUT);
			result = SQLPrepare (SetTimeout_hstmt, StatementBuffer, strlen (StatementBuffer));
			if (SQL_FAILED (result))
			{
				GerrLogMini (GerrId, "Unable to prepare %s (result = %d).\n", StatementBuffer, result);
				CleanupODBC (Database);
				break;
			}

			result = SQLExecute (SetTimeout_hstmt);	// USE command.

			if (SQL_FAILED (result))
			{
				ODBC_StatementError (&SetTimeout_hstmt, 0);
				break;
			}

			SQLFreeStmt (SetTimeout_hstmt, SQL_CLOSE);
			// SET TIMEOUT done.
#endif



	if (!RPC_Allocated)
	{
		// Allocate a statement handle - works only after connecting to database.
		// NOTE: SQLAllocStmt is now deprecated, and should be replaced by a call to
		// SQLAllocHandle with appropriate arguments. (DonR 31Oct2019)
		res = SQLAllocStmt (RPC_hdbc, &RPC_hstmt);
		if (SQL_FAILED (res))
		{
			GerrLogMini (GerrId, "TikrotRPC: Unable to allocate statement handle (ret = %d).", res);
			RPC_Cleanup ();
			return (1);
		}
		
		RPC_Allocated = 1;
	}	// RPC_Allocated == 0.

	RPC_Connected = 1;

	// DonR 21Apr2021: Try setting the timeout for this statement; if this works, maybe it
	// will prevent Nihul Tikrot calls from hanging during the nightly backup on AS/400.
	res = SQLSetStmtAttr (RPC_hstmt, SQL_ATTR_QUERY_TIMEOUT, (SQLPOINTER)3, SQL_IS_INTEGER);
if (res) GerrLogMini (GerrId, "SQLSetStmtAttr to set Nihul Tikrot timeout returned %d.", res);

	// Prepare the stored procedure statement.
	strcpy (szStoredProc,
			"call RKPGMPRD.TIKROT (?, ?, ?, ?, ?, ?, ?, ?)");
    res = SQLPrepare (RPC_hstmt, (unsigned char *)szStoredProc, strlen (szStoredProc));
	if (SQL_FAILED (res))
    {
		GerrLogMini (GerrId, "SQLPrepare failed, res = %d.\n", res);
		RPC_Cleanup ();
    	return (1);
    }

//	TestTikraCall ();

	return 0;
} // Initialize



// RPC_Cleanup before exit
void RPC_Cleanup()
{
// Disconnect from the database and free all handles
	SQLFreeStmt		(RPC_hstmt, SQL_CLOSE);
	SQLDisconnect	(RPC_hdbc);
	SQLFreeConnect	(RPC_hdbc);
	SQLFreeEnv		(RPC_henv);

	RPC_Allocated = RPC_Connected = 0;

	return;
}

void TestTikraCall ()
{
	char				TikrotHeader		[RPC_INP_HEADER_LEN];
	char				TikrotCurrentSale	[RPC_INP_CURRSALE_LEN];
	char				TikrotPriorSales	[RPC_INP_PRIORSALES_LEN];
	char				*HeaderRtn;
	char				*TikrotRtn;
	char				*CurrentSaleRtn;
	char				*CouponsRtn;
	short			TikrotRPC_Error;


	strcpy (TikrotHeader		, "                                         ");
	strcpy (TikrotCurrentSale	, "                                         ");
	strcpy (TikrotPriorSales	, "                                         ");

				// Finally (well, not quite finally), call the RPC to invoke the AS/400 "Tikrot" program.
				TikrotRPC_Error = CallTikrotSP (TikrotHeader,	TikrotCurrentSale,	TikrotPriorSales,
												&HeaderRtn,		&TikrotRtn,			&CurrentSaleRtn,	&CouponsRtn);

				GerrLogMini (GerrId, "Test Tikra: Rtn = %d, Hdr {%s}, Tik {%s}, Curr {%s}, Coup {%s}\n",
					TikrotRPC_Error, HeaderRtn, TikrotRtn, CurrentSaleRtn, CouponsRtn);
}
