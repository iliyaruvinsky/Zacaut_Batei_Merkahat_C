//===========================================================================
// Reshuma ltd. @{SRCH}
//								PharmTcpServer.cpp
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

#define LOG_ALL_INCOMING	0

#include <stdlib.h>

#include "misc.h"
#include "Log.h"
#include "CSocketConn.h"
#include "CPharmLiteProtocol.h"

#include "DebugPrn.h"

#pragma GCC diagnostic ignored "-Wwrite-strings"
// suppress warnings : deprecated conversion from string constant to 'char*'
// caused by Global.h inclusion  		GAL 04/11/2013
extern "C"
{
	#include "Global.h"
}

// Define source name
// static char THIS_FILE[] = __FILE__;
static char *GerrSource = PCHAR( __FILE__ );

//
//	Parameters
//	----------
int ClientPort;
int PharmReadTimeout;

//int ClientHangupTimeout;

ParamTab PharmParams[] =
{
//	{ (void*)&ClientHangupTimeout,	"ClientHangupTimeout",	PAR_INT, PAR_RELOAD, MAC_TRUE },
	{ (void*)&PharmReadTimeout,		PCHAR("ReadTimeout"),			PAR_INT, PAR_RELOAD, MAC_TRUE },
	{ (void*)&ClientPort,			PCHAR("ListenPort" ),			PAR_INT, PAR_RELOAD, MAC_TRUE },
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
  { 0, 0,			NOT_SORTED,		NULL,
    NULL,		"",			"",	}
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

static int SubSystem = DOCTOR_SYS;
// static unsigned long SessionID;  GAL 04/11/2013
static char WorkerName[]="PharmTcpWorker.exe";

static Byte PharmSqlBuffer[128000];
static int  PharmSqlBufLen;
static char PharmSqlNamedPipe[256];

static CInetConnection *connListener = NULL;
static CInetConnection *connWorker = NULL;


//	Local functions
static int ProcessCmdLine	(int argc, char* argv[]);
static int CheckConnection	();
static int InitListener		();
static int StartWorker		();
static int ClientSession	();
static int NewSystemSend	(int *PharmSqlSock, CMemoryMessage *Message);
static int NewSystemRecv	(int  PharmSqlSock, CMessage **Message);


int	main (int argc, char *argv[])
{
    DIPR_DATA	DiprData;
    PROC_DATA	ProcData;
	enum { Up , Down } RunState;
    time_t	DownTime;
    // DebugPrn( "PharmTcp Started.");

	//	Initialize child process
	ProcessCmdLine (argc, argv);

	// DonR 14Aug2022: Changed to new process type PHARM_TCP_SERVER_TYPE,
	// since the new version of FatherProcess prefers to have unique
	// process type values and the old value was shared with DocTcpServer.
    RETURN_ON_ERR (InitSonProcess (argv[0], PHARM_TCP_SERVER_TYPE, retrys, ClientPort, SubSystem));
    // DebugPrn( "InitSonProcess executed.");

	RETURN_ON_ERR (ShmGetParamsByName (PharmParams, PAR_LOAD));
	// DebugPrn( "ShmGetParamsByName executed.");

	if (InitListener () != 0)
		return -1;


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

		//	Update parametrs from shared memory if needed
		RETURN_ON_ERR (UpdateShmParams ());
		// DebugPrn( "UpdateShmParams executed.");

		//	Get system state & child processes count
		//
		// Go down only when all child processes but tsservers & control
		// went down -- because this process informs father of died process and
		// must go down last
		if (RunState == Up)
		{
			if (ToGoDown (SubSystem))
			{
				RunState = Down;
				DownTime = time (NULL);
			}
		}
		else
		{
			if( TssCanGoDown( SubSystem ) && time(NULL) - DownTime > 1 )
			{
				break;
			}
		}

		//	Inform father of died processes
		//
		// Wait for all children - non blocking option
		DiprData.pid = waitpid ((pid_t)-1, &DiprData.status, WNOHANG);
		
		// Add child process data to table of died processes
		if (DiprData.pid > 0)
		{
			ProcData.pid = DiprData.pid;
			
			if (ActItems (ProcessTable, 1, GetProc, (OPER_ARGS)&ProcData) == MAC_OK)
			{
				state = AddItem (DiedProcessTable, (ROW_DATA)&DiprData);
			}
			else
			{
				GerrLogMini (GerrId, PCHAR("Worker process didn't start!"));
			}
		}

		//	Suspend execution for short period - ZZZZZZZZ...
		 #ifndef _LINUX_  // GAL 25/01/06
			nap( X25TimeOut * 3 );
		 #else
			usleep( X25TimeOut * 300  ); 
		 #endif
		// DebugPrn( "After %d microseconds sleep." , X25TimeOut * 300 );

		//	Check running state
		if (RunState == Down)
			continue;
		// DebugPrn( "After RunState check." );

		//	Check for incoming connection requests
		if (CheckConnection ())
			break;
		// DebugPrn( "After CheckConnection() check." );
	} // while(1)


	//	Cleanup listener & X25 engine
	if (connListener != NULL)
	{
		delete connListener;
	}

	//	Cleanup process
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

//===========================================================================
// @{FUNH}
//								CheckConnection()
//
//---------------------------------------------------------------------------
// Description: Check for incoming connection and start worker
// Return type: int - 
//===========================================================================
int CheckConnection()
{
//
//	Check if someone called
//	-----------------------
    switch( connListener->Accept((CConnection**)&connWorker) )
    {
	case 0:
		return 0;
		
	case 1:
		break;
		
	default:
		//		GerrLogReturn(GerrId, "x25 accept failure");
		return 0;
    }

//===========================================================================
//					Fork worker and return to listen mode
//===========================================================================
    switch( fork() )
    {
	case -1:			/*    ERROR		*/
	    GerrLogReturn( GerrId, PCHAR( "fork() failed! : %d : %s" ),
			errno, PCHAR( strerror(errno) ) );

		//connListener->Close();
		//connListener->Listen( NULL, ClientPort );
		connWorker->Close();
		delete connWorker;
		connWorker = NULL;
		break;

	case 0:				/* CHILD PROCESS	*/
	    //
	    /// Do worker work in child process
	    /// exit child process when work is done
	    //
		mypid = getpid ();
		sprintf (DiagFileName, "TCP_%d_log", GetDate());
//		GerrLogFnameMini (DiagFileName, GerrId, PCHAR("Child PID %d started."), mypid);
	    StartWorker();
	    return -1;

	default:			/* CURRENT PROCESS	*/

		// detach x25 connection in listener process
		//connListener->CloseChild();
		//connListener->Listen( NULL, ClientPort );
		connWorker->Close();
		delete connWorker;
		connWorker = NULL;
		break;
    }

//
//	Update statistics
//	-----------------
	state = UpdateLastAccessTime( );
	if( ERR_STATE( state ) )
	{
		GerrLogReturn(	GerrId,
		PCHAR( "Error while updating last access time of process" ) );
	}

	return 0;
}

//===========================================================================
// @{FUNH}
//								InitListener()
//
//---------------------------------------------------------------------------
// Description: Init X25 engine & listener
// Return type: int - 
//===========================================================================
int InitListener()
{
	connListener = new CInetConnection();

	if (connListener == NULL)
	{
		GerrLogReturn(GerrId, PCHAR( "Memory allocation failure" ) );
		return -1;
	}

	connListener->SetTimeOuts( 
		PharmReadTimeout, 
		PharmReadTimeout,
		PharmReadTimeout,
		PharmReadTimeout );
	// DebugPrn("connListener->SetTimeOuts");
	if ( connListener->Init() ) return -1;
	// DebugPrn("connListener->Init");
	if ( connListener->Listen( NULL, ClientPort ) ) return -1;
	// DebugPrn("connListener->Listen");

	return 0;
}

//===========================================================================
// @{FUNH}
//								StartWorker()
//
//---------------------------------------------------------------------------
// Description: Run worker process
// Return type: int - 
//===========================================================================
int StartWorker()
{
//	GerrLogFnameMini (DiagFileName, GerrId, PCHAR("PID %d calling InitSonProcess."), mypid);
	// DonR 14Aug2022: Changed to new process type PHARM_TCP_SERVER_TYPE,
	// since the new version of FatherProcess prefers to have unique
	// process type values and the old value was shared with DocTcpServer.
    RETURN_ON_ERR(
		  InitSonProcess(
				 WorkerName,
				 PHARM_TCP_SERVER_TYPE,
				 retrys,
				 ClientPort,
				 SubSystem
				 )
		  );

//	GerrLogFnameMini (DiagFileName, GerrId, PCHAR("PID %d calling ClientSession."), mypid);
	ClientSession();

//	GerrLogFnameMini (DiagFileName, GerrId, PCHAR("PID %d closing and exiting."), mypid);
	connWorker->Close();

	delete connWorker;
	connWorker=NULL;

	delete connListener;
	connListener = NULL;

    ExitSonProcess( MAC_SERV_SHUT_DOWN );
	return 0;

}

//===========================================================================
// @{FUNH}
//								ClientSession()
//
//---------------------------------------------------------------------------
// Description: Process client requests
// Return type: int - 
//===========================================================================
int ClientSession()
{
	int PharmSqlSock;
//===========================================================================
//							Init client connection
//===========================================================================
	mypid = getpid ();

//	DebugPrn( "Client Session Started.");
//	GerrLogFnameMini (DiagFileName, GerrId, PCHAR("PID %d calling OpenChild."), mypid);
	int errorNo = connWorker->OpenChild();
	if (errorNo != 0)	// In fact, there is not value returned other than zero in OpenChild()!
	{
		GerrLogReturn( GerrId, PCHAR( "Can't init connection to client" ));
		return errorNo;
	}

//===========================================================================
//						Create & init client protocol
//===========================================================================
	CPharmLiteServerProtocol protClient(connWorker);

//	protClient.nReadTimeOut = PharmReadTimeout;
//	protClient.nSendTimeOut = PharmReadTimeout;
//	protClient.nAckTimeOut =  PharmReadTimeout;
//	protClient.nRetries = 3;

//
//	Say hello to client
//	-------------------
//	GerrLogFnameMini (DiagFileName, GerrId, PCHAR("PID %d calling Hello."), mypid);
	errorNo = protClient.Hello();
	if ( errorNo != 0 )
	{   // GAL 07/02/2006
		GerrLogMini (GerrId, PCHAR("PID %d ClientSession(): protClient.Hello() returned %d."), mypid, errorNo);
		char* pszRemoteName = connWorker->GetRemote();

		prn_2( "Handshaking with client has failed, remote address: %s, error code: %d", 
				NULL == pszRemoteName ? "NULL" : pszRemoteName , errorNo );

		return -1;
	}
  
	// Previous variant commented by GAL 07/02/06
	// if ( errorNo != 0 )
	// {
	//	 prn_2("Handshaking with client has failed, remote address: %s, error code: %d", 
	//		connWorker->GetRemote(), errorNo);
	//	 return -1;
	// }

//===========================================================================
//							Init broken pipe handler
//===========================================================================

//===========================================================================
//								Begin session
//===========================================================================
	CMemoryMessage RequestMsg  (nMesBufSize);

	//	time_t nLastTransTime = 0;
	//	SessionID = time(0);
	timer.m_resettype = CTimer::rtManual;

	//
	//	Main loop
	//	---------
	//	for (int nIndex=0; ;nIndex++)
	// DonR 01May2018: Trial version - loop will now execute only a maximum number of times - 10 for now.
	for (int nIndex = 0; nIndex < 10; nIndex++)
	{
//		GerrLogFnameMini (DiagFileName, GerrId, PCHAR("PID %d ClientSession loop counter = %d."), mypid, nIndex);
		//
		//	Check system status
		//	-------------------
		if (ToGoDown (SubSystem))
		{
			break;
		}

		//
		//	Check connection status
		//	-----------------------
		if (connWorker->GetStatus() != TRUE)
		{
			prn_1("GetStatus returns false, index: %d", nIndex);
//			GerrLogFnameMini (DiagFileName, GerrId, PCHAR("PID %d GetStatus() returned NOT CONNECTED."), mypid);
			break;
		} 

		//	Sleep
		#ifndef _LINUX_  // GAL 25/01/06
			nap( X25TimeOut * 3 );
		#else
			usleep( X25TimeOut * 300  ); 
		#endif


		//
		//	Check if disconnect request
		//	---------------------------
//		if (RequestMsg.GetMsgNo()==10)
//		{
//			// Check for timeout
//			if (time(NULL)-nLastTransTime > ClientHangupTimeout)
//			{
//				GerrLogReturn(GerrId, "A client with address <%s> does not hangup connection \n"
//								"after %d sec - exit on timeout.", 
//								connWorker->GetRemote(), ClientHangupTimeout);
//				break;
//			}
//
//			continue;
//		}
//
//		// Save start transaction time
//		nLastTransTime = time(NULL);

		timer.Reset();
	
		//
		//	Receive message from client
		//	---------------------------
		if (LOG_ALL_INCOMING)
			prn("Receiving message from client ... " );

//		GerrLogFnameMini (DiagFileName, GerrId, PCHAR("PID %d calling ReceiveMessage()..."), mypid);
//		if ( errorNo = protClient.ReceiveMessage(&RequestMsg) ) GAL 03/11/2013
		errorNo = protClient.ReceiveMessage (&RequestMsg);
//		GerrLogFnameMini (DiagFileName, GerrId, PCHAR("PID %d ReceiveMessage() returned %d."), mypid, errorNo);
		if (errorNo != 0)
		{
//			prn_1("recv returns ERROR=%d", errorNo );
//			if (errorNo != -102)
//				GerrLogMini (GerrId, PCHAR("PID %d ClientSession(): protClient.ReceiveMessage() returned %d."), mypid, errorNo);
			break;
		}

		char* pszRemoteAddr = connWorker->GetRemote();
		if ( pszRemoteAddr == NULL )
		{    // GAL 07/02/06
			prn( "connWorker->GetRemote() = NULL" ); 			
		}
		RequestMsg.SetLocal (pszRemoteAddr ? pszRemoteAddr : (char*)"Unknown" ); 
		// Original commented by GAL 07/02/06
		// RequestMsg.SetLocal( connWorker->GetRemote() );

		//
		//	Send message to new system
		//	--------------------------
		// if ( errorNo = NewSystemSend( &PharmSqlSock, &RequestMsg ) ) // GAL 04/11/2013
		if ((errorNo = NewSystemSend (&PharmSqlSock, &RequestMsg)) != 0)
		{
			prn_1("new send returns ERROR=%d", errorNo );
			GerrLogMini (GerrId, PCHAR("PID 5d ClientSession(): NewSystemSend() returned %d."), mypid, errorNo);
			break;
		}

		//
		//	Receive answer from new system
		//	------------------------------
		CMessage *NewAnswerMsg = NULL;

//		NewAnswerMsg.Clear();

		// if ( errorNo = NewSystemRecv( PharmSqlSock, &NewAnswerMsg ) )
		if ((errorNo = NewSystemRecv (PharmSqlSock, &NewAnswerMsg)) != 0)
		{
			prn_1("new recv returns ERROR=%d", errorNo );
			GerrLogMini (GerrId, PCHAR("PID %d ClientSession(): NewSystemRecv() returned %d."), mypid, errorNo);
			break;
		}

		if( !NewAnswerMsg )
		{
			prn("NewAnswerMsg is NULL pointer" );
			GerrLogMini (GerrId, PCHAR("PID %d ClientSession(): NewAnswerMsg IS NULL."), mypid);
			break;
		}

		//		NewAnswerMsg->SetMsgNo( RequestMsg.GetMsgNo()+1 );
		//
		//	Send answer to client
		//	---------------------
//		if ( errorNo = protClient.SendMessage(NewAnswerMsg) ) GAL 04/11/2013
//		GerrLogFnameMini (DiagFileName, GerrId, PCHAR("PID %d calling SendMessage()..."), mypid);
		errorNo = protClient.SendMessage (NewAnswerMsg);
//		GerrLogFnameMini (DiagFileName, GerrId, PCHAR("PID %d SendMessage() returned %d."), mypid, errorNo);
		if (errorNo != 0)
		{
			delete NewAnswerMsg;

			prn_1("send returns ERROR=%d", errorNo );
			GerrLogMini (GerrId, PCHAR("PID %d ClientSession(): protClient.SendMessage() returned %d."), mypid, errorNo);
//			GerrLogFnameMini (DiagFileName, GerrId, PCHAR("PID %d SendMessage() returned %d."), mypid, errorNo);
			break;
		}

		delete NewAnswerMsg;
	}
	//	-------------
	//	End main loop
	//

	return 0;
}

int NewSystemSend( int *PharmSqlSock, CMemoryMessage *Message )
{
	int PharmSqlPid = 0;

//
//	Capture sql server
//	------------------
	int errorNo = GetFreeSqlServer( &PharmSqlPid, PharmSqlNamedPipe, SQLPROC_TYPE );
	if( !PharmSqlPid )
	{
		GerrLogReturn(GerrId, PCHAR( "Can not find free SqlServer : state = %d" ), errorNo );
		return -1;
	}
	
//
//	Connect to sql server's pipe
//	----------------------------
	*PharmSqlSock = ConnectSocketNamed( PharmSqlNamedPipe );
	if( *PharmSqlSock < 0 )
	{
		prn_2( "SqlSession : Error connect to sqlserver. State : %d. Transaction : %d", 
			*PharmSqlSock, Message->GetMsgNo() );
		return -1;
	}
	
//
//	Send request to sql server
//	--------------------------
	errorNo = WriteSocket( *PharmSqlSock, (char*)Message->GetBuffer(), Message->GetSize(), 0 );
	if( errorNo )
	{
		prn_2( "SqlSession : Error write to sqlserver. State : %d. Transaction : %d", 
			errorNo, Message->GetMsgNo() );
		CloseSocket( *PharmSqlSock );
		return -1;
	}

	return 0;
} // NewSystemSend

int NewSystemRecv( int  PharmSqlSock, CMessage **Message )
{
	int type;
	int errorNo = 0;
	int nOutputType;

	//
	// Get output type
	// ---------------
	errorNo = ReadSocket( PharmSqlSock, (char *)PharmSqlBuffer, sizeof(PharmSqlBuffer), 
						  &type, &PharmSqlBufLen, VERBOSE );
	if( errorNo )
	{
		prn_1( "SqlSession : Error read (1) from sqlserver. State : %d", errorNo );
		CloseSocket( PharmSqlSock );
		return -1;
	}

	nOutputType = atoi((char*)PharmSqlBuffer);

	//
	// Get output
	// ----------
	errorNo = ReadSocket( PharmSqlSock, (char *)PharmSqlBuffer, sizeof(PharmSqlBuffer),
						  &type, &PharmSqlBufLen, VERBOSE );
	if( errorNo )
	{
		prn_1( "SqlSession : Error read (2) from sqlserver. State : %d.", errorNo );
		CloseSocket( PharmSqlSock );
		return -1;
	}

	CloseSocket( PharmSqlSock );

	switch( nOutputType )
	{
	case ANSWER_IN_BUFFER:
		*Message = new CMemoryMessage( PharmSqlBufLen + 10 );
		(*Message)->Write( PharmSqlBuffer, PharmSqlBufLen );
		break;

	case ANSWER_IN_FILE:
		*Message = new CFileMessage( (char*)PharmSqlBuffer );
		break;

	default:
	    GerrLogReturn( GerrId, PCHAR("Got unknown output type '%d'"), nOutputType );
		return -1;
	}

	(*Message)->SetLocal( PCHAR ( "SqlServer" ) );
	(*Message)->Reset();

	return 0;
} // NewSystemRecv


extern "C" char * GetDbTimestamp ()
{
	time_t	err_time;
	static char TS_buffer [50];

	time	(&err_time);
	sprintf	(TS_buffer, ctime(&err_time));

	return (char *)TS_buffer;
}


/*=======================================================================
||									||
||				date_to_tm()				||
||									||
|| Break date into structure tm.					||
||									||
 =======================================================================*/
extern "C" void    date_to_tm( int inp,  struct tm *TM )
{
    memset( (char*)TM, 0, sizeof(struct tm) );
    TM->tm_mday  =   inp % 100;
    TM->tm_mon   = ( inp / 100 ) % 100 - 1;
    TM->tm_year  = ( inp / 10000 ) - 1900;
}
/*=======================================================================
||									||
||				time_to_tm()				||
||									||
|| Break time into structure tm.					||
||									||
 =======================================================================*/
extern "C" void    time_to_tm( int inp,  struct tm *TM )
{
    TM->tm_sec  =   inp % 100;
    TM->tm_min  = ( inp / 100 ) % 100;
    TM->tm_hour = inp / 10000;
}
/*=======================================================================
||									||
||				datetime_to_unixtime()			||
||									||
|| Converts db date & time in format 'YYYYMMDD' and 'HHMMSS		||
|| to UNIX system time							||
||									||
 =======================================================================*/
extern "C" time_t    datetime_to_unixtime( int db_date, int db_time )
{
    struct tm dbt;

    date_to_tm( db_date, &dbt );
    time_to_tm( db_time, &dbt );

    return ( mktime(&dbt) );
}


#if 0
extern "C" int    AddDays( int base_day,    int add_day )
{
/*=======================================================================
||			    Local declarations				||
 =======================================================================*/
    struct tm   time_str;
    time_t      vac;
/*=======================================================================
||				Body of function			||
 =======================================================================*/
    date_to_tm( base_day, &time_str );
//20020201 add zero in all fields because other way that not work 
	time_str.tm_sec   = 0;
	time_str.tm_min   = 0;
	time_str.tm_hour  = 0;
	time_str.tm_wday  = 0;
	time_str.tm_yday  = 0;
	time_str.tm_isdst = 0;
#ifndef _LINUX_
	time_str.tm_tzadj = (long)0;
	time_str.tm_name[ 0 ] = '\0';
#endif


    vac = mktime( &time_str ) + add_day * 86400;
    return( tm_to_date( localtime( &vac )));
}
#endif

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
||									||
||				tm_to_time()				||
||									||
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
