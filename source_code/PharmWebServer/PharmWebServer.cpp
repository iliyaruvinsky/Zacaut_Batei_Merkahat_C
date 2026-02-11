//===========================================================================
// Reshuma ltd. @{SRCH}
//								PharmWebServer.cpp
//
//---------------------------------------------------------------------------
// DESCRIPTION: @{HDES}
// -----------
// main source for network server for doctor system
//---------------------------------------------------------------------------
// CHANGES LOG: @{HREV}
// -----------
// Revision: 01.00
// By      : Boris Evsikov
// Date    : 20/06/99 12:23:45
// Comments: First Issue
//===========================================================================

//# include <unistd.h>

#define MAIN_SRC
#define MAIN
#define NON_SQL

#include <stdlib.h>

#include "misc.h"
#include "Log.h"
#include "CSocketConn.h"
#include "CPharmLiteProtocol.h"
#include <served/served.hpp>
#include "DebugPrn.h"
#include <boost/algorithm/string.hpp>

#pragma GCC diagnostic ignored "-Wwrite-strings"
// suppress warnings : deprecated conversion from string constant to 'char*'
// caused by Global.h inclusion  		GAL 04/11/2013
extern "C"
{
	#include "Global.h"
}

// Define source name
static char *GerrSource = PCHAR( __FILE__ );

//
//	Parameters
//	----------
char	WebHostName		[81];
char	WebHostPort		[ 9];
int		WebServerThreads;

//int ClientPort;
//int PharmReadTimeout;


ParamTab PharmParams[] =
{
	{ (void*)WebHostName	,		PCHAR("WebHostName"),			PAR_CHAR,	PAR_RELOAD, MAC_TRUE },
	{ (void*)WebHostPort	,		PCHAR("WebHostPort"),			PAR_CHAR,	PAR_RELOAD, MAC_TRUE },
	{ (void*)&WebServerThreads,		PCHAR("WebServerThreads"),		PAR_INT,	PAR_RELOAD, MAC_TRUE },
	{ NULL, NULL, 0, 0, MAC_TRUE }
};


//
//	External functions
//	------------------
extern "C" int TssCanGoDown( int SubSystem );

extern "C" int	tm_to_date( struct tm *tms );
extern "C" char * GetDbTimestamp ();

TABLE_DATA	TableTab[] =
{
  { 0, 0, NOT_SORTED, NULL, NULL, "", "",	}
};


//
//	Public variables
//	----------------
int debug = 0;
int	mypid = 0;
char DiagFileName[100];

//
//	Local variables
//	---------------
static TABLE *DiedProcessTable;
static TABLE *ProcessTable;

static int SubSystem = PHARM_SYS;

static Byte PharmSqlBuffer		[128000];
static int  PharmSqlBufLen;
static char PharmSqlNamedPipe	[256];

//	Local functions
static int		ProcessCmdLine			(int argc, char* argv[]);
static int		NewSystemSend			(int *PharmSqlSock, CMemoryMessage *Message, short ServerType);
static int		NewSystemRecv			(int  PharmSqlSock, CMessage **Message);
static void		AddHttpHeaders			(const served::request & HTTP_req, std::string & JSON_request_in_out, short TerminateOuterObject);
static void		ProcessServerRequest	(short ServerType, const std::string TransactionNum_in, const std::string JsonRequest_in, served::response & Response, short EnableDiagnostics_in);
static char *	GetServerName			(short ServerType, char **ServerDescription_out);


int	main (int argc, char *argv[])
{
    DIPR_DATA	DiprData;
    PROC_DATA	ProcData;
	enum { Up , Down } RunState;
    time_t		DownTime;
	int			err;


	// Served multiplexer for handling HTTP requests.
	served::multiplexer mux;

	// Get command-line parameters.
	ProcessCmdLine (argc, argv);	// DonR 27Jan2021: Add number of sessions to command line!
// printf("Processed command line - argv[0] = {%s}.\n", argv[0]);

	//	Initialize son process
	err = InitSonProcess (argv[0], WEBSERVERPROC_TYPE, retrys, 0, SubSystem);
	if (err)
	{
		GerrLogMini (GerrId, "\nPharmWebServer: InitSonProcess() returned %d.\n", err);
	}
	else
	{
		GerrLogMini (GerrId, "\nPharmWebServer.exe starting up.");
	}
//printf("InitSonProcess returned %d.\n", err);


	// DonR 27Jan2021: Load shared-memory parameters. If anything wasn't
	// found, use a default but do *not* abort the program.
	// First, supply a couple of defaults.
	strcpy				(WebHostName,	"");		// Will force a call to gethostname() if nothing is found in shared memory.
	strcpy				(WebHostPort,	"8080");
	WebServerThreads	= 15;						// Set default thread count equal to roughly the maximum number of sqlserver.exe instances.

	// Now read in parameters from shared memory - which may overwrite the defaults.
	// Note that Error -500 for WebHostName is not fatal, since we automatically fall
	// back to getting a value using gethostname().
	err = ShmGetParamsByName (PharmParams, PAR_LOAD);
	if (err)
	{
		GerrLogMini (GerrId, "\nShmGetParamsByName() returned %d.", err);
	}
//printf("ShmGetParamsByName returned %d.\n", err);

	GerrLogMini (	GerrId,
					"Values from shared memory:\n   WebHostName      = \"%s\",\n   WebHostPort      = \"%s\",\n   WebServerThreads = \"%d\".",
					WebHostName, WebHostPort, WebServerThreads);

	// If the database (loaded to shared memory) supplied a blank Web Host Name (which
	// should be the case if two or more servers are working off the same database but
	// should expose different web addresses), substitute the local host name.
	//
	// NOTE: I'm not sure if we actually have to use unique Web Host Names for each
	// Linux server. If we define pharm-web-server in each server's /etc/hosts as
	// pointing to that server's TCP address, we're fine. OTOH if we use gethostname(),
	// we don't have to add anything to /etc/hosts. Hmmmm...
	if (strlen (WebHostName) < 2)
	{
		err = gethostname	(WebHostName, 80);
		GerrLogMini (GerrId, "\nSupplied my own Web Host Name %s, err = %d.", WebHostName, err);
//printf("Supplied my own Web Host Name %s, err = %d.\n", WebHostName, err);
	}


	// 1) Define Served multiplexer handling for "legacy" pharmacy transactions - fixed-format strings
	//    passed to the Pharmacy Server, with fixed-format responses passed back to the pharmacy.
	mux.handle ("/mac/v1/pharmacies/legacy")
		.post ([](served::response & res, const served::request & req)
		{
			std::string		PharmRequest		= req.body();
			std::string		EnableDiagnostics	= req.header ("EnableDiagnostics");
			short			DiagnosticsEnabled	= (EnableDiagnostics.length() > 0);	// Don't bother looking at the actual value.

			// Strip leading and trailing spaces from the request body.
			// DonR 07Jun2021: Move this to before we use PharmRequest.length to initialize RequestMsg.
			boost::trim (PharmRequest);
			
			std::string		TrnID				= PharmRequest.substr (1, 4);

			// At least for now, don't do any validation - just pass whatever we got in the
			// HTTP Body to the Pharmacy Server. But leave the possibility open to re-enable
			// this code!
#if 0
			std::string		TrnID_5_Digits		= PharmRequest.substr (0, 5);	// Only for diagnosing bad/missing Trn. ID.
			int				Transaction			= 0;
			short			IsValidOldStyle		= 1;							// Default - assume the request starts with a valid Transaction ID.

			// If the request doesn't start with a "0", it's not a valid old-style request string -
			// assuming, of course, that we don't get up to Transaction ID's greater than 9999!
			if (PharmRequest.substr (0, 1) == "0")
			{
				Transaction	= std::stoi (TrnID);
// GerrLogMini (GerrId, "Old-style Transction ID = %d.", Transaction);
			}

			switch (Transaction)
			{
				case 1001:
				case 1007:
				case 1009:
				case 1011:	// Drug-list download - old format.
				case 1013:
				case 1014:
				case 1022:
				case 1026:
				case 1028:
				case 1043:
				case 1047:
				case 1049:
				case 1051:
				case 1053:
				case 1055:
				case 1063:	// Ancient drug-sale request - should be NIU.
				case 1065:	// Ancient drug-sale completion - should be NIU.
				case 1080:	// Request to Update Pharmacy's Contract data
				case 2001:	// Request for Prescriptions to Fill - old Electronic Prescriptions version
				case 2003:	// Sale Request - old Electronic Prescriptions version
				case 2005:	// Delivery - old Electronic Prescriptions version
				case 2033:	// Pharmacy Ishur.
				case 2060:	// how_to_take full-table download.
				case 2062:	// drug_shape full-table download.
				case 2064:	// unit_of_measure full-table download.
				case 2066:	// reason_not_sold full-table download.
				case 2068:	// discount_remarks full-table download.
				case 2070:	// Request to Update Pharmacy's EconomyPri data (for generic substitution)
				case 2092:	// EconomyPri (new version, supports Group Code up to 99999)
				case 2072:	// Request to Update Pharmacy's Prescription Source data.
				case 2074:	// Request to Update Pharmacy's Drug_extension (simplified) data.
				case 2076:	// Request to Update Pharmacy's Confirm_authority data.
				case 2078:	// Request to Update Pharmacy's Sale data (for MaccabiCare).
				case 2080:	// Request to download Gadgets data.
				case 2082:	// Request to download PharmDrugNotes data.
				case 2084:	// Request to download DrugNotes data.
				case 2086:	// Request for pharmacy_daily_sum data by diary month.
				case 2088:	// Request for sales data by diary month/sale date.
				case 2090:	// Request for single-sale details.
				case 2094:	// Separate transaction for drug-list supplemental fields.
				case 2096:	// Marianna 15Oct2020 Feature #97279/CR #32620: Usage_instruct full-table download.
				case 2101:	// Marianna 15Oct2020 Feature #97279/CR #32620: Usage_instr_reason_changed full-table download.
				case 5003:	// "Nihul Tikrot" version Sale/Credit/Deletion request.
				case 5005:	// "Nihul Tikrot" version Delivery Confirmation.
				case 5051:	// Full download of credit_reasons table.
				case 5053:	// Full download of subsidy_messages table.
				case 5055:	// Full download of tikra_type table.
				case 5057:	// Full download of hmo_membership table.
				case 5059:	// Full download of virtual_store_reason_texts table.
				case 5061:	// Incremental download of DrugGenComponents table.
				case 5090:	// Request for single-sale details - new version for Nihul Tikrot.
				case 6001:	// Request for Prescriptions to Fill - "Digital Prescription" version.
				case 6003:	// "Digital Prescription" Sale/Credit/Deletion request.
				case 6005:	// "Digital Prescription" Delivery Confirmation.
				case 6011:	// Member prior-purchase / ishur report to pharmacy. (Jul2025, User Story #417785.)

							IsValidOldStyle = 1;
							break;

				default:	IsValidOldStyle = 0;
							GerrLogMini (GerrId, "Received unrecognized old-style Trn ID %d (%s).", Transaction, TrnID_5_Digits.c_str());
							break;
			}	// Switch on Transaction ID.

			if (!IsValidOldStyle)
			{
				served::response::stock_reply(400, res);

				// Add any cleanup here that would normally be done by ProcessServerRequest().

				return;
			}
#endif

			ProcessServerRequest (SQLPROC_TYPE, TrnID, PharmRequest, res, DiagnosticsEnabled);
		}	);
	// Define Served multiplexer handling for "legacy" fixed-format pharmacy requests.
	// /mac/v1/pharmacies/legacy.POST


	// 2) Define Served multiplexer handling for "Chanut Virtualit" Request for Valid Member Prescriptions (Trn. 6101).
	mux.handle ("/mac/v1/members/{technical_id}/prescriptions/pharmacies/{pharmacy_id}/valid")
		.get ([](served::response & res, const served::request & req)
		{
			std::string		PharmRequest;
			std::string		TechnicalID			= req.params["technical_id"];
			std::string		PharmacyCode		= req.params["pharmacy_id"];
			std::string		EnableDiagnostics	= req.header ("EnableDiagnostics");
			std::string		TransactionID		= "6101";
			short			DiagnosticsEnabled	= (EnableDiagnostics.length() > 0);	// Don't bother looking at the actual value.

			// Build the pharmacy request as a JSON object. Note that request_number is hard-coded, since
			// we're handling a URL that we know invokes Transaction 6101.
			PharmRequest =		"{\"PharmServerRequest\": {\"request_number\": "	+	TransactionID
							+	", \"technical_id\": "								+	TechnicalID
							+	", \"pharmacy_id\": "								+	PharmacyCode
							+	"}";

			AddHttpHeaders (req, PharmRequest, true);

			ProcessServerRequest (SQLPROC_TYPE, TransactionID, PharmRequest, res, DiagnosticsEnabled);
		}	);
	// Define Served multiplexer handling for "Chanut Virtualit" Request for Valid Member Prescriptions (Trn. 6101).
	// /mac/v1/members/{technical_id}/prescriptions/pharmacies/{pharmacy_id}/valid.GET


	// 3) Define Served multiplexer handling for "Chanut Virtualit" get prescription count for a group of members (Trn. 6102).
	mux.handle ("/mac/v1/prescriptions/valid/count")
		.post ([](served::response & res, const served::request & req)
		{
			std::string		PharmRequest		= req.body();
			std::string		EnableDiagnostics	= req.header ("EnableDiagnostics");
			std::string		TransactionID		= "6102";
			short			DiagnosticsEnabled	= (EnableDiagnostics.length() > 0);	// Don't bother looking at the actual value.

			// Strip leading and trailing spaces from the HTTP request body, then build the pharmacy request
			// as a JSON object. Note that request_number is hard-coded, since we're handling a URL that we
			// know invokes Transaction 6102. Note also that in order to add the incoming JSON data to the
			// PharmServerRequest object without adding another layer of tags, we skip past its initial "{";
			// but we can leave its final "}" in place as long as we make the incoming JSON data the last
			// thing we add to the PharmServerRequest object.
			boost::trim (PharmRequest);
			PharmRequest =		"{\"PharmServerRequest\": {\"request_number\": "	+	TransactionID
							+	", "												+	PharmRequest.substr (1);
																						// Includes the closing bracket for the PharmServerRequest tag.

			AddHttpHeaders (req, PharmRequest, true);

			ProcessServerRequest (SQLPROC_TYPE, TransactionID, PharmRequest, res, DiagnosticsEnabled);
		}	);
	// Define Served multiplexer handling for "Chanut Virtualit" get prescription count for a group of members (Trn. 6102).
	// /mac/v1/prescriptions/valid/count.POST


	// 4) Define Served multiplexer handling for "Chanut Virtualit" get participation for a list of drugs (Trn. 6103).
	mux.handle ("/mac/v1/members/{technical_id}/prescriptions/pharmacies/{pharmacy_id}/participation")
		.post ([](served::response & res, const served::request & req)
		{
			std::string		PharmRequest		= req.body();
			std::string		TechnicalID			= req.params["technical_id"];
			std::string		PharmacyCode		= req.params["pharmacy_id"];
			std::string		EnableDiagnostics	= req.header ("EnableDiagnostics");
			std::string		TransactionID		= "6103";
			short			DiagnosticsEnabled	= (EnableDiagnostics.length() > 0);	// Don't bother looking at the actual value.

			// Strip leading and trailing spaces from the HTTP request body, then build the pharmacy request
			// as a JSON object. Note that request_number is hard-coded, since we're handling a URL that we
			// know invokes Transaction 6103. Note also that in order to add the incoming JSON data to the
			// PharmServerRequest object without adding another layer of tags, we skip past its initial "{";
			// but we can leave its final "}" in place as long as we make the incoming JSON data the last
			// thing we add to the PharmServerRequest object.
			boost::trim (PharmRequest);
			PharmRequest =		"{\"PharmServerRequest\": {\"request_number\": "	+	TransactionID
							+	", \"technical_id\": "								+	TechnicalID
							+	", \"pharmacy_id\": "								+	PharmacyCode
							+	", "												+	PharmRequest.substr (1);
																						// Includes the closing bracket for the PharmServerRequest tag.

			AddHttpHeaders (req, PharmRequest, true);

			ProcessServerRequest (SQLPROC_TYPE, TransactionID, PharmRequest, res, DiagnosticsEnabled);
		}	);
	// Define Served multiplexer handling for "Chanut Virtualit" get participation for a list of drugs (Trn. 6103).
	// /mac/v1/members/{technical_id}/prescriptions/pharmacies/{pharmacy_id}/participation.POST


	// 5) Define Served multiplexer handling for "regular" pharmacy REST/JSON transactions.
	//    (Added 16Jul2025 to support User Story #417785.)
	mux.handle ("/mac/v1/pharmacies/transactions/{transaction_number}")
		.post ([](served::response & res, const served::request & req)
		{
			std::string		PharmRequest		= req.body();
			std::string		TransactionID		= req.params["transaction_number"];
			std::string		EnableDiagnostics	= req.header ("EnableDiagnostics");
			short			DiagnosticsEnabled	= (EnableDiagnostics.length() > 0);	// Don't bother looking at the actual value.

			// Strip leading and trailing spaces from the HTTP request body, then build the pharmacy request
			// as a JSON object. Note that in order to add the incoming JSON data to the PharmServerRequest
			// object without adding another layer of tags, we skip past its initial "{"; but we can leave
			// its final "}" in place as long as we make the incoming JSON data the last thing we add to the
			// PharmServerRequest object.
			boost::trim (PharmRequest);
			PharmRequest =		"{\"PharmServerRequest\": {\"request_number\": "	+	TransactionID
							+	", "												+	PharmRequest.substr (1);
																						// Includes the closing bracket for the PharmServerRequest tag.

			AddHttpHeaders (req, PharmRequest, true);

			ProcessServerRequest (SQLPROC_TYPE, TransactionID, PharmRequest, res, DiagnosticsEnabled);
		}	);
	// Define Served multiplexer handling for "regular" pharmacy REST/JSON transactions.
	// /mac/v1/pharmacies/transactions/{transaction_number}.POST


	// 6) Define Served multiplexer handling for Member Purchase History microservice.
	//    (Note: I'm leaving open the possibility of more restrictive options on retrieving
	//    a member's purchase history - thus the "full" at the end of this URL, to distinguish
	//    this call from possible other versions of the request.)
	mux.handle ("/mac/v1/members/tz/{member_tz}/tz_code/{member_tz_code}/purchase_history/full")
		.get ([](served::response & res, const served::request & req)
		{
			std::string		HistoryRequest		= req.body();
			std::string		MemberTZ			= req.params["member_tz"];
			std::string		MemberTZ_Code		= req.params["member_tz_code"];
			std::string		EnableDiagnostics	= req.header ("EnableDiagnostics");
			std::string		TransactionID		= "0";
			short			DiagnosticsEnabled	= (EnableDiagnostics.length() > 0);	// Don't bother looking at the actual value.

			// Strip leading and trailing spaces from the HTTP request body, then build the pharmacy request
			// as a JSON object. Note that request_number is hard-coded. Note also that in order to add the
			// incoming JSON data to the PharmServerRequest object without adding another layer of tags, we
			// skip past its initial "{"; but we can leave its final "}" in place as long as we make the incoming
			// JSON data the last thing we add to the PharmServerRequest object.
//			HistoryRequest =		"{\"HistoryServerRequest\": {\"request_type\": "	+	TransactionID
//								+	", \"member_tz\": "									+	MemberTZ
			HistoryRequest =		"{\"HistoryServerRequest\": {\"member_tz\": "		+	MemberTZ
								+	", \"member_tz_code\": "							+	MemberTZ_Code
								+	"}";

			AddHttpHeaders (req, HistoryRequest, true);

			ProcessServerRequest (PURCHASE_HIST_TYPE, TransactionID, HistoryRequest, res, DiagnosticsEnabled);
		}	);
	// Define Served multiplexer handling for Member Purchase History microservice, full-history version.
	// /mac/v1/members/tz/{member_tz}/tz_code/{member_tz_code}/purchase_history/full.GET


	// Create the server and run with the configured number of handler threads.
	served::net::server server (WebHostName, WebHostPort, mux);
	server.run (WebServerThreads);


	//	Open params table, died process table
    ABORT_ON_ERR (OpenTable (PCHAR(DIPR_TABLE), &DiedProcessTable));
    // DebugPrn( "DiedProcessTable opened.");
    ABORT_ON_ERR (OpenTable (PCHAR(PROC_TABLE), &ProcessTable));
    // DebugPrn( "ProcessTable opened.");


	//===========================================================================
	//									Master loop
	//===========================================================================
    RunState = Up;
    while (1)
    {
		//
		//	Update parametrs from shared memory if needed
		//	---------------------------------------------
		// DonR 27Jan2021: Are these still needed?
		RETURN_ON_ERR (UpdateShmParams ());
		// DebugPrn( "UpdateShmParams executed.");

		//
		//	Get system state & son processes count
		//	--------------------------------------
		//
		// Go down only when all son processes but tsservers & control
		// went down -- because this process informs father of died
		// process and must go down last.
		//
		if (RunState == Up)
		{
			if (ToGoDown (SubSystem))
			{
				RunState = Down;
				DownTime = time(NULL);
			}
		}
		else
		{
			if (TssCanGoDown (SubSystem) && time (NULL) - DownTime > 1)
			{
				break;
			}
		}

		//
		//	Inform father of died processes
		//	-------------------------------
		//
		// Wait for all childs non blocking option
		//
		DiprData.pid = waitpid ((pid_t)-1, &DiprData.status, WNOHANG);
		
		//
		// Add son process data to table of died processes
		//
		if (DiprData.pid > 0)
		{
			ProcData.pid = DiprData.pid;
			
			if (ActItems (ProcessTable, 1, GetProc, (OPER_ARGS)&ProcData) == MAC_OK)
			{
				state = AddItem (DiedProcessTable, (ROW_DATA)&DiprData);
			}
			else
			{
				GerrLogReturn (GerrId, PCHAR( "Worker process didn't start" ));
			}
		}

		//
		//	Suspend execution for short period - ZZZZZZZZ...
		//	------------------------------------------------
		#ifndef _LINUX_  // GAL 25/01/06
			nap (X25TimeOut * 3);
		#else
			usleep (X25TimeOut * 300);
		#endif
		// DebugPrn( "After %d microseconds sleep." , X25TimeOut * 300 );

		//
		//	Check running state
		//	-------------------
		if (RunState == Down)
			continue;
		// DebugPrn( "After RunState check." );

//		//
//		//	Check for incoming connection requests
//		//	--------------------------------------
//		if( CheckConnection() ) break;
//		// DebugPrn( "After CheckConnection() check." );

	} // while(1)

//	//
//	//	Cleanup listener & X25 engine
//	//	-----------------------------
//	if (connListener != NULL)
//	{
//		delete connListener;
//	}
//
	//
	//	Cleanup process
	//	---------------
    ExitSonProcess (MAC_SERV_RESET);
} // main


//===========================================================================
// @{FUNH}
//								ProcessCmdLine()
//
//---------------------------------------------------------------------------
// Description: Process command line
// Return type: int - 
//   Argument : int argc - 
//   Argument : char* argv[] - 
//===========================================================================
int ProcessCmdLine(int argc, char* argv[])
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
			debug++;
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
		GerrLogReturn(GerrId, PCHAR("Illegal option '%c'"), c);
		exit(1);
    }
    return 0;
}


static void AddHttpHeaders (const served::request & HTTP_req, std::string & JSON_request_in_out, short TerminateOuterObject)

{
			short	NeedComma				= 0;
			short	HTTP_HeaderNum;
			std::string		HeaderValue;
			char	*HTTP_headers[]		=	{	"MAC-ConsumerChannel", 
												"MAC-ConsumerSysID", 
												"MAC-ConsumerSysName", 
												"MAC-ConsumerAppID",
												"MAC-ConsumerAppName", 
												"MAC-SysID", 
												"MAC-SysName", 
												"MAC-AppID", 
												"MAC-AppName", 
												"MAC-AppVersion", 
												"MAC-MessageVersion", 
												"MAC-MessageID", 
												"MAC-TransactionID", 
												"MAC-CorrelationID",
												"MAC-From", 
												"MAC-OS", 
												"MAC-IP",
												"MAC-UserID",
												NULL
											};

			// Build the additional JSON object for any HTTP headers that we recognize.
			// Note that an HTTP_Headers object is created even if no recognized headers are seen.
			JSON_request_in_out += ", \"HTTP_Headers\": {";

			for (HTTP_HeaderNum = NeedComma = 0; HTTP_headers [HTTP_HeaderNum] != NULL; HTTP_HeaderNum++)
			{
				HeaderValue = HTTP_req.header (HTTP_headers [HTTP_HeaderNum]);
// GerrLogMini (GerrId, "HTTP_req.header (%s) = %s.", HTTP_headers [HTTP_HeaderNum], HeaderValue.data());

				if (HeaderValue.length() > 0)
				{
//					JSON_request_in_out = JSON_request_in_out + (NeedComma ? "," : ", \"HTTP_Headers\": {") + "\"" + HTTP_headers [HTTP_HeaderNum] + "\":\"" + HeaderValue + "\"";
					JSON_request_in_out = JSON_request_in_out + (NeedComma ? "," : "") + "\"" + HTTP_headers [HTTP_HeaderNum] + "\":\"" + HeaderValue + "\"";
					NeedComma = 1;
				}
			}	// Loop through a list of known HTTP headers to add to JSON object "HTTP_Headers".

			// Terminate the HTTP_Headers object - but *not* the outer object,
			// unless TerminateOuterObject is TRUE.
			JSON_request_in_out += (TerminateOuterObject) ? "}}" : "}";
}


static void ProcessServerRequest (short ServerType, const std::string TransactionNum_in, const std::string JsonRequest_in, served::response & Response, short EnableDiagnostics_in)
{
	int				errorNo					= 0;
	int				PharmSqlSock			= 0;	// Socket for connection to sqlserver.exe instance.
	int				nLen					= 0;
	int				AnswerBytesReceived		= 0;
	int				HTTP_response_status	= 200;	// Default = "OK".
	CMessage		*NewAnswerMsg			= NULL;
	Byte			bBuffer					[65536];
	char			OutputLenBuf			[   11];
	char			StrTok_buffer			[   51];
	char			*ServerName;
	char			*ServerDesc;
	std::string		tag_HTTP_Response		= "\"HTTP_response_status\":";
	std::string		tag_MAC_Severity		= "\"mac_status_severity\":";
	std::string		tag_MAC_Status			= "\"mac_status_code\":";
	std::string		tag_MAC_Notification	= "\"mac_notification\":";
	std::string		ServerResponse;


	// Get pointers to the short and long server names, just for nice diagnostic messages.
	ServerName = GetServerName (ServerType, &ServerDesc);
// GerrLogMini (GerrId, "ProcessServerRequest: Server Type %d resolves to %s.", ServerType, ServerDesc);

	// Copy the JSON request we built into a CMemoryMessage instance.
	CMemoryMessage	RequestMsg (JsonRequest_in.length());
	RequestMsg.Write ((unsigned char *)JsonRequest_in.data(), JsonRequest_in.length());

	// Output the constructed request message (only for testing).
	if (EnableDiagnostics_in)
	{
		Response << "\n";
		Response << ServerDesc;
		Response << " request = \n";
		Response << JsonRequest_in;
	}

	// Send the request to the server.
	// DonR 02Feb2022: To support "microservices", Server Type is now variable.
//	if ((errorNo = NewSystemSend (&PharmSqlSock, &RequestMsg, SQLPROC_TYPE)) != 0)
	if ((errorNo = NewSystemSend (&PharmSqlSock, &RequestMsg, ServerType)) != 0)
	{
		GerrLogMini (GerrId, "%s Trn. %s: NewSystemSend returns ERROR = %d.", ServerName, TransactionNum_in.c_str(), errorNo);
	}
// else GerrLogMini (GerrId, "%s Trn. %s: NewSystemSend returns ERROR = %d.", ServerName, TransactionNum_in.c_str(), errorNo);

	// Get response from sqlserver.exe.
	if ((errorNo = NewSystemRecv (PharmSqlSock, &NewAnswerMsg)) != 0)
	{
		GerrLogMini (GerrId, "%s Trn. %s: NewSystemRecv returns ERROR = %d.", ServerName, TransactionNum_in.c_str(), errorNo);
	}
// else GerrLogMini (GerrId, "%s Trn. %s: NewSystemRecv returns ERROR = %d.", ServerName, TransactionNum_in.c_str(), errorNo);

	if (NewAnswerMsg == NULL)
	{
		GerrLogMini (GerrId, "%s Trn. %s: NewAnswerMsg is NULL.", ServerName, TransactionNum_in.c_str());
		Response << "\n\nCould not get a response from the ";
		Response << ServerDesc;
		Response << ".\n";
	}
	else
	{
		// Decode sqlserver's answer.
		NewAnswerMsg->GetMsgNo();
		NewAnswerMsg->Reset();

		nLen = NewAnswerMsg->GetSize();
		if (nLen < 0)
		{
			GerrLogMini (GerrId, "%s Trn. %s: Got negative response-message length %d.", ServerName, TransactionNum_in.c_str(), nLen);
		}

		if (EnableDiagnostics_in)
		{
			Response << "\n\nResponse =\n";
			sprintf (OutputLenBuf, "%07d", nLen);
			Response << "  Length = ";	// TEMPORARY FOR TESTING.
			Response << OutputLenBuf;
			Response << "\n\n";			// TEMPORARY FOR TESTING.

		}

		while (nLen > 0)
		{
			AnswerBytesReceived = NewAnswerMsg->Read (bBuffer, sizeof(bBuffer));
			if (AnswerBytesReceived < 0)
			{
				GerrLogMini (GerrId, "%s Trn. %s: Message->Read returned %d bytes read!", ServerName, TransactionNum_in.c_str(), AnswerBytesReceived);
				break;
			}

			// DonR 04May2018: If, for some reason, there's nothing more to send, break out of the loop.
			if (AnswerBytesReceived == 0)
				break;

			// If, for some reason, the "Read" command returned more bytes than we were
			// supposed to get, take only as much as we were supposed to get.
			if (AnswerBytesReceived > nLen)
			{
				AnswerBytesReceived = nLen;
			}

			// Decrement the total response size by the amount we've received
			// in this iteration of the while loop.
			nLen -= AnswerBytesReceived;

			// Add a "Content-Type: application/json" output header, plus an encoding specifier.
			// Apparently we need to tell the "downstream" JSON recipients that we're sending
			// Hebrew text in Win-1255 encoding, since otherwise they're looking for UTF-8 (or
			// perhaps something else).
			// DonR 22Nov2021: Changed output charset to UTF-8 - supported by the new
			// translation routine WinHebToUTF8().
			Response.set_header ("Content-Type", "application/json; charset=UTF-8");
//			Response.set_header ("Content-Type", "application/json; charset=windows-1255");

			// Look for an HTTP response code to send; if it's there, it will be attached to the tag "HTTP_response_status".
			const char *HTTP_Response = strcasestr ((const char *)bBuffer, tag_HTTP_Response.data());
			if (HTTP_Response != NULL)
			{
				HTTP_response_status = atoi (HTTP_Response + tag_HTTP_Response.length());
			}

			// Set the HTTP Response Status Code.
			Response.set_status (HTTP_response_status);


			// Look for Maccabi status variables and output them as HTTP headers as well.
			// NOTE: Because the strtok() library function is destructive, I copy 50 bytes from
			// the relevant offset in the output buffer and then run strtok() on the copy. This
			// is a fairly primitive solution, but it has the advantage of (relative) simplicity.
			const char *mac_severity		= strcasestr ((const char *)bBuffer, tag_MAC_Severity.data());
			const char *mac_status			= strcasestr ((const char *)bBuffer, tag_MAC_Status.data());
			const char *mac_notification	= strcasestr ((const char *)bBuffer, tag_MAC_Notification.data());

			if (mac_severity != NULL)
			{
				memcpy (StrTok_buffer, mac_severity + tag_MAC_Severity.length(), 50);
				Response.set_header ("MAC-StatusSeverity",	strtok (StrTok_buffer, " \t\","));
			}

			if (mac_status != NULL)
			{
				memcpy (StrTok_buffer, mac_status + tag_MAC_Status.length(), 50);
				Response.set_header ("MAC-StatusCode",		strtok (StrTok_buffer, " \t\","));
			}

			if (mac_notification != NULL)
			{
				memcpy (StrTok_buffer, mac_notification + tag_MAC_Notification.length(), 50);
				Response.set_header ("MAC-Notification",		strtok (StrTok_buffer, " \t\","));
			}


			ServerResponse = (char *)bBuffer;
			ServerResponse.resize (AnswerBytesReceived);

			// DonR 23Oct2022 BUG FIX: If the output length is greater than 65,535 characters,
			// we *don't* want to insert newline characters in the middle - this can make
			// JSON data unparsable!
			if (nLen > 0)	// I.e. we're not finished getting the response message.
			{
				Response << ServerResponse;	// Just send the output without adding a newline.
			}
			else
			{
				// Should the final newline character be conditional on EnableDiagnostics_in?
				Response << ServerResponse << "\n";
			}

		}	// While nLen > 0.
	}	// NewAnswerMsg is NOT NULL.


	// Clean up our messes!
	delete NewAnswerMsg;
}


int NewSystemSend (int *PharmSqlSock, CMemoryMessage *Message, short ServerType)
{
	int PharmSqlPid = 0;


	// Find a free server.
	// DonR 02Feb2022: The Server Type is now a variable rather than hard-coded SQLPROC_TYPE;
	// this allows us to use new "microservices" with new Server Types.
	int errorNo = GetFreeSqlServer( &PharmSqlPid, PharmSqlNamedPipe, ServerType );
	if (!PharmSqlPid)
	{
		GerrLogMini (GerrId, "Can't find free type %d server: error = %d", ServerType, errorNo);
		return -1;
	}
// else GerrLogMini (GerrId, "Connected to server type %d: PID = %d.", ServerType, PharmSqlPid);
	
	//	Connect to the server's pipe.
	*PharmSqlSock = ConnectSocketNamed (PharmSqlNamedPipe);
	if (*PharmSqlSock < 0)
	{
		prn_2( "SqlSession : Error connect to sqlserver. State : %d. Transaction : %d", 
			*PharmSqlSock, Message->GetMsgNo() );
		return -1;
	}
	
	//	Send request to server.
// GerrLogMini (GerrId, "Writing %d bytes to socket %d:\n%s", Message->GetSize(), *PharmSqlSock, (char *)Message->GetBuffer());
	errorNo = WriteSocket( *PharmSqlSock, (char *)Message->GetBuffer(), Message->GetSize(), 0 );
	if (errorNo)
	{
		prn_2 ("SqlSession : Error writing to server. State: %d. Transaction : %d", 
			errorNo, Message->GetMsgNo() );
		CloseSocket (*PharmSqlSock);
		return -1;
	}

	return 0;
} // NewSystemSend


int NewSystemRecv (int  PharmSqlSock, CMessage **Message)
{
	int type;
	int errorNo = 0;
	int nOutputType;

	// Get output type.
	errorNo = ReadSocket (PharmSqlSock, (char *)PharmSqlBuffer, sizeof(PharmSqlBuffer), 
						  &type, &PharmSqlBufLen, VERBOSE);
	if (errorNo != 0)
	{
		prn_1 ("SqlSession : Error read (1) from sqlserver. State : %d", errorNo);
		CloseSocket (PharmSqlSock);
		return -1;
	}

	nOutputType = atoi ((char*)PharmSqlBuffer);

	// Get output
	errorNo = ReadSocket (PharmSqlSock, (char *)PharmSqlBuffer, sizeof(PharmSqlBuffer),
						  &type, &PharmSqlBufLen, VERBOSE);
	if (errorNo != 0)
	{
		prn_1 ("SqlSession : Error read (2) from sqlserver. State : %d.", errorNo);
		CloseSocket (PharmSqlSock);
		return -1;
	}

	CloseSocket (PharmSqlSock);

	switch (nOutputType)
	{
		case ANSWER_IN_BUFFER:
			*Message = new CMemoryMessage (PharmSqlBufLen + 10);
			(*Message)->Write (PharmSqlBuffer, PharmSqlBufLen);
			break;

		case ANSWER_IN_FILE:
			*Message = new CFileMessage ((char*)PharmSqlBuffer);
			break;

		default:
			GerrLogReturn (GerrId, PCHAR ("Got unknown output type %d."), nOutputType);
			return -1;
	}

	(*Message)->SetLocal (PCHAR ( "SqlServer" ));
	(*Message)->Reset ();

	return 0;
} // NewSystemRecv


static char *	GetServerName (short ServerType, char **ServerDescription_out)
{
	char *ShortName			= "";
	char *ServerDescription	= "";

	switch (ServerType)
	{
		case SQLPROC_TYPE:			ShortName			= "SqlServer";
									ServerDescription	= "Pharmacy Server";
									break;

		case PURCHASE_HIST_TYPE:	ShortName			= "PurchaseHistoryServer";
									ServerDescription	= "Purchase History Server";
									break;

		default:					ShortName			= "Unknown server";
									ServerDescription	= "Undefined/unexpected Server ID";
									break;
	}

	if (ServerDescription_out != NULL)
		*ServerDescription_out = ServerDescription;

	return (ShortName);
}


extern "C" char * GetDbTimestamp ()
{
	time_t	err_time;
	static char TS_buffer [50];

	time	(&err_time);
	sprintf	(TS_buffer, ctime(&err_time));

	return (char *)TS_buffer;
}


/*=======================================================================
||																		||
||				date_to_tm()											||
||																		||
|| Break date into structure tm.										||
||																		||
 =======================================================================*/
extern "C" void    date_to_tm( int inp,  struct tm *TM )
{
    memset( (char*)TM, 0, sizeof(struct tm) );
    TM->tm_mday  =   inp % 100;
    TM->tm_mon   = ( inp / 100 ) % 100 - 1;
    TM->tm_year  = ( inp / 10000 ) - 1900;
}


/*=======================================================================
||																		||
||				time_to_tm()											||
||																		||
|| Break time into structure tm.										||
||																		||
 =======================================================================*/
extern "C" void    time_to_tm( int inp,  struct tm *TM )
{
    TM->tm_sec  =   inp % 100;
    TM->tm_min  = ( inp / 100 ) % 100;
    TM->tm_hour = inp / 10000;
}


/*=======================================================================
||																		||
||				datetime_to_unixtime()									||
||																		||
|| Converts db date & time in format 'YYYYMMDD' and 'HHMMSS				||
|| to UNIX system time													||
||																		||
 =======================================================================*/
extern "C" time_t    datetime_to_unixtime( int db_date, int db_time )
{
    struct tm dbt;

    date_to_tm( db_date, &dbt );
    time_to_tm( db_time, &dbt );

    return ( mktime(&dbt) );
}


/*=======================================================================
||																		||
||				tm_to_date()											||
||																		||
 =======================================================================*/
extern "C" int	tm_to_date( struct tm *tms )
{
    return(  tms->tm_mday +
	  (tms->tm_mon  +    1) * 100 +
	  (tms->tm_year + 1900) * 10000 );
}


/*=======================================================================
||																		||
||				tm_to_time()											||
||																		||
 =======================================================================*/
extern "C" int	tm_to_time( struct tm *tms )
{
    return( tms->tm_sec          +
	    tms->tm_min  * 100   +
	    tms->tm_hour * 10000  );
}


/*=======================================================================
||																		||
||				GetDate ()												||
||																		||
||		get current date in format YYMMDD								||
 =======================================================================*/
int	GetDate (void)
{
	time_t		curr_time;
	struct tm	*tm_strct;

	curr_time = time (NULL);

	tm_strct  = localtime (&curr_time);

	return (tm_to_date (tm_strct));
}
