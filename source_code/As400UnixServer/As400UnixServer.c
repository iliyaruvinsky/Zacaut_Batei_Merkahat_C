/* << -------------------------------------------------------- >> */
/*   		        As400UnixServer.c                    	      */
/*  -----------------------------------------------------------   */
/*  Date: 		10/1996 - 5/1997			                      */
/*  Written by:  	Gilad Haimov ( reshuma )		              */
/*  -----------------------------------------------------------   */
/*  Purpose :							                          */
/*       Source file for client-process in AS400 <-> Unix   	  */
/*       communication system.       	            			  */
/*  -----------------------------------------------------------   */
/*  Comments:                                                     */
/*	 The file uses the include file As400UnixServer.h	          */
/* << -------------------------------------------------------- >> */
/*  Date: 		02/2001         			                      */
/*  Updated by: Yulia Zilberstein ( macabi )		              */
/*              New transaction 2055 for drug update for		  */
/*              click's doctor system transaction 49 doctors      */
/*  -----------------------------------------------------------   */

/*===============================================================
||				defines											||
 ===============================================================*/
#define MAIN

#define VIRTUAL_PORTION			120		// DonR 09Jun2004 (was 170)
#define VIRTUAL_MORNING_START	50000	// DonR 15Jan2007
#define HEB_CONV				1
#define	NO_CONV					0

char *GerrSource = __FILE__;

#include <MacODBC.h>
#include "MsgHndlr.h"
#include "As400UnixServer.h"
#include "DBFields.h"
#include "PharmDBMsgs.h"
#include <netinet/tcp.h>
#include "TreatmentTypes.h"
#include "PharmacyErrors.h"


void	pipe_handler		(int signo);
void	TerminateHandler	(int signo);
void	switch_to_win_heb	(unsigned char * source);
void	ReadIniFilePort		(void);  /*20020828*/
void	AllCaps				(char *buf);
int		GetVirtualTimestamp (int *virtual_date_out, int *virtual_time_out);	// DonR 15Jan2007
short	ProcessHealthBasket	(short *);	// DonR 26Oct2008.

struct sigaction	sig_act_pipe;
struct sigaction	sig_act_terminate;

bool	RealTimeMode	= false;

int		VirtualMorningDate;	// DonR 15Jan2007
int		VirtualMorningTime;	// DonR 15Jan2007

int		caught_signal;

char	TableName		[32 + 1];

int		TikrotProductionMode	= 1;	// To avoid "unresolved external" compilation error.

ISOLATION_VAR;


static int rows_added				= 0;
static int rows_updated				= 0;
static int rows_deleted				= 0;
static int rows_refreshed			= 0;

static int download_start_date		= 0;
static int download_start_time		= 0;
static int download_msg_initialized	= 0;
static int download_row_count		= 0;

static short GlbMsgId				= 0;

static char	*PosInBuff;	// current position in data buffer

static short AllowedByDefault	[100];	// DonR 13Mar2018 HOT FIX.
static short IdTypeAccepted		[100];


// DonR 26Oct2008: Declare global variable for new version of in-health-basket flag.
// This is slightly tacky, but makes it easier to make the same change for six tables.
static short	v_HealthBasketNew;
static int		nocard_ph_maxvalid;
static int		svc_w_o_card_days;
int				total_rows		= 0;
int				active_rows		= 0;

#define GET_SHORT(var,len)		var =	get_short	(&PosInBuff, len); RETURN_ON_ERROR;
#define GET_LONG(var,len)		var =	get_long	(&PosInBuff, len); RETURN_ON_ERROR;
#define GET_DOUBLE(var,len)		var =	get_double	(&PosInBuff, len); RETURN_ON_ERROR;
#define GET_STRING(var,len)				get_string	(&PosInBuff, var, len);
#define GET_HEB_STR(var,len)			get_str_h	(&PosInBuff, var, len, 1);

#define GET_SIGNED(var,unsignedvar,sign,len)						\
			get_string	(&PosInBuff, unsignedvar,	len);			\
			get_string	(&PosInBuff, sign,			1);				\
			RETURN_ON_ILLEGAL_SIGN (sign);							\
			var = atoi (strcat (sign, unsignedvar));


// DonR 26Oct2008: Nice (?) little macro to translate new health-basket value from AS400 and
// put the new value into a new version of the variable. 
#define	PROCESS_HEALTH_BASKET	v_HealthBasketNew = ProcessHealthBasket (&v_in_health_basket);

// DonR 09Oct2011: New macro for "Tzahal" enhancement.
#define SET_TZAHAL_FLAGS					\
	switch (v_enabled_status)				\
	{										\
		case 0:								\
				v_enabled_mac	= 0;		\
				v_enabled_hova	= 0;		\
				v_enabled_keva	= 0;		\
				break;						\
											\
		case 1:	/* Everyone */				\
				v_enabled_mac	= 1;		\
				v_enabled_hova	= 1;		\
				v_enabled_keva	= 1;		\
				break;						\
											\
		case 2:	/* Maccabi only */			\
				v_enabled_mac	= 1;		\
				v_enabled_hova	= 0;		\
				v_enabled_keva	= 0;		\
				break;						\
											\
		case 3:	/* Tzahal Hova only */		\
				v_enabled_mac	= 0;		\
				v_enabled_hova	= 1;		\
				v_enabled_keva	= 0;		\
				break;						\
											\
		case 4:	/* Tzahal only. */			\
				v_enabled_mac	= 0;		\
				v_enabled_hova	= 1;		\
				v_enabled_keva	= 1;		\
				break;						\
											\
		case 5:	/* Tzahal keva only. */		\
				v_enabled_mac	= 0;		\
				v_enabled_hova	= 0;		\
				v_enabled_keva	= 1;		\
				break;						\
											\
		/* Default: Maccabi-only. */		\
		default:							\
				v_enabled_mac	= 1;		\
				v_enabled_hova	= 0;		\
				v_enabled_keva	= 0;		\
				break;						\
	}

#define DARKONAI	9


/*===============================================================
||																||
||				main											||
||																||
 ===============================================================*/
int main (int	argc, char	*argv[])
{
/*===============================================================
||			Local variables				||
 ===============================================================*/

    fd_set 	called_sockets;	/* sockets called during select	*/
    int		rc;		/* select() return code		*/
    int		data_len;	/* message len excluding header	*/
    bool	flg_sys_down;	/* if true -  sys going down	*/
    bool	flg_sys_busy;	/* if true -  system busy	*/
    bool	stop_on_err;	/* terminate proc with err code */
    int		retrys;		/* used by INIT_SON_PROCESS	*/
    struct timeval	timeout;
    TConnection	*cp;
    int		sock_max;
    int		i;
    int		print_flg;
    int		first_flg = 1;
	sigset_t	NullSigset;

/*===============================================================
||																||
||			initializations										||
||																||
 ===============================================================*/
    GetCommonPars (argc, argv, &retrys);

    RETURN_ON_ERR (InitSonProcess (argv[0], AS400TOUNIX_TYPE, retrys, 0, PHARM_SYS));

	caught_signal = 0;
	memset ((char *)&NullSigset, 0, sizeof(sigset_t));

    sig_act_pipe.sa_handler = pipe_handler;
    sig_act_pipe.sa_mask = NullSigset;  // DonR 12Nov2007 - Unix/Linux compatibility.
    sig_act_pipe.sa_flags = 0;
    
    sig_act_terminate.sa_handler	= TerminateHandler;
    sig_act_terminate.sa_mask		= NullSigset;
    sig_act_terminate.sa_flags		= 0;


    sigaction( SIGPIPE, &sig_act_pipe, NULL );
  	sigaction( SIGTERM, &sig_act_terminate, NULL);
    
 
//	// DonR 13Feb2020: Add signal handler for segmentation faults - used
//	// by ODBC routines to detect invalid pointer parameters.
//  sig_act_ODBC_SegFault.sa_handler	= ODBC_SegmentationFaultCatcher;
//  sig_act_ODBC_SegFault.sa_mask		= NullSigset;
//  sig_act_ODBC_SegFault.sa_flags		= 0;
//
//	if (sigaction (SIGSEGV, &sig_act_ODBC_SegFault, NULL) != 0)
//	{
//		GerrLogReturn (GerrId,
//					   "Can't install signal handler for SIGSEGV" GerrErr,
//					   GerrStr);
//	}
//
//  
    flg_sys_busy = false;

	// Connect to database.
	// DonR 03Mar2021: Set MS-SQL attributes for query timeout and deadlock priority.
	// For As400UnixServer, we want a mid-length timeout (so we'll be reasonably
	// patient) and a normal deadlock priority (since we want to privilege getting
	// table updates over sending updates to AS/400, but *below* getting responses
	// to pharmacies in the Pharmacy Server program).
	LOCK_TIMEOUT			= 500;	// Milliseconds.
	DEADLOCK_PRIORITY		= 0;	// 0 = normal, -10 to 10 = low to high priority.

	do
	{
		SQLMD_connect ();
		if (!MAIN_DB->Connected)
		{
			sleep (10);
			GerrLogMini (GerrId, "\n\nRetrying %s ODBC connect.", MAIN_DB->Name);
		}
	}
	while (!MAIN_DB->Connected);
	
/*===============================================================
||	Init flag to False to perform initilisation of          || 
||	  	       glbPriceListUpdateArray                  ||
  =============================================================*/
    PriceArrayInitilized = false;


	// DonR 25Feb2018: Get valid period for service-without-card requests from sysparams.
	ExecSQL (	MAIN_DB, AS400SRV_READ_sysparams_svc_without_card_params,
				&nocard_ph_maxvalid, &svc_w_o_card_days, END_OF_ARG_LIST	);

	if ((SQLCODE == 0) && (nocard_ph_maxvalid > 0))
		NoCardSvc_Max_Validity = nocard_ph_maxvalid * 60;	// Sysparams has validity time in minutes.
	else
		NoCardSvc_Max_Validity = 3 * 60 * 60;				// Default = 3 hours.

/*===============================================================
||			Initialize connection array		||
 ===============================================================*/

    initialize_connection_array();

	ReadIniFilePort ();	/*20020828*/

/*===============================================================
||			Make listen socket			||
 ===============================================================*/

    stop_on_err = false;

    glbListenSocket = make_listen_socket();

    if( glbListenSocket < 0 )
    {
		stop_on_err = true;
    }

/*===============================================================
||								||
||				Main loop			||
||								||
 ===============================================================*/

    flg_sys_down = false;

    while ((stop_on_err == false) && (flg_sys_down == false))
    {

		/*===============================================================
		||			Handle client connections							||
		 ===============================================================*/

		FD_ZERO (&called_sockets);

		{
			time_t curr_time = time(NULL);

			first_flg = ! print_flg;
			print_flg = (! (curr_time % 3600) ) ;
		}

		// Loop over connections
		if (print_flg && first_flg)
		{
			puts("Connection map for as400unixserver :");
			puts("------------------------------------");
			puts("------------------------------------");
		}

		for (sock_max = i = 0; i < MAX_NUM_OF_CLIENTS;  i++)
		{
			cp = glbConnectionsArray + i;

		    // Check only active connections
			if (cp->server_state == 0)
			{
				break;
			}

			// Close connections in "low-water-mark"
			if (time (NULL) - cp->last_access > 1200)
			{
				terminate_a_connection (cp, (cp->in_tran_flag == 1) ? WITH_ROLLBACK : WITHOUT_ROLLBACK);
				continue;
			}

		    if (print_flg && first_flg)
		    {
				printf ("Connection %d : sock <%d> db <%s> state <%d> in_tran <%d>\n",
						i, cp->socket, cp->db_connection_id, cp->server_state, cp->in_tran_flag );
				fflush (stdout);
		    }

		    // Add connection socket to bit mask
		    FD_SET (cp->socket, &called_sockets);

		    sock_max = sock_max > cp->socket ? sock_max : cp->socket;
		}	// Loop through sockets.


		// Add listen socket to bit mask
		FD_SET (glbListenSocket, &called_sockets);

		sock_max = sock_max > glbListenSocket ? sock_max : glbListenSocket;

		// Set timeout for select.
		// Note that as of 08Apr2021 (and for a long time already) SelectWait = 200000 = 1/5 second.
		//
		// DonR 02May2022: Instead of using the global SelectWait, hard-code a longer
		// timeout interval. This should cut processor usage, since most of the time
		// the program is sitting around waiting for input from AS/400 CL.EC. And when
		// FatherProcess shuts the system down, it uses a signal to do so anyway - and
		// that will interrupt the select() in any case. I'll try 2.0 seconds, for now.
//		timeout.tv_sec	= SelectWait / 1000000;
//		timeout.tv_usec	= SelectWait % 1000000;
		timeout.tv_sec	= 2;
		timeout.tv_usec	= 0;

		rc = select	(	sock_max + 1,		/* no. of bits to scan		*/
						&called_sockets,	/* sockets called for recv ()	*/
						NULL,
						NULL,
						&timeout			/* time to wait			*/
					);

		switch (rc)
		{
			case -1:	// SELECT ERROR
						GerrLogReturn (GerrId, "Select() error\n" GerrErr, GerrStr);
						stop_on_err = true;
						continue;

			default:	// CONNECTION REQUESTS

						// Update last access time for process
						state = UpdateLastAccessTime ();
						if (ERR_STATE (state))
						{
							GerrLogReturn (GerrId, "Error while updating last access time of process");
						}

						if (FD_ISSET (glbListenSocket, &called_sockets))
						{
							// Create new connection
							if (create_a_new_connection () == failure)
							{
								GerrLogReturn (GerrId, "Unable to create new client-connection. Error ( %d ) %s.", GerrStr);
							}

							continue;
						}

						// Handle all client requests
						answer_to_all_client_requests (&called_sockets);


						/* -- FALL THROUGH -- */

			case 0:		// TIMEOUT
						RETURN_ON_ERR (get_sysem_status (&flg_sys_down, &flg_sys_busy));
						break;

		}	// switch (rc)

    }	// while ((stop_on_err == false) && (flg_sys_down == false))


	/*=======================================================================
	||    			termination of process					             	||
	 =======================================================================*/
	ExitSonProcess ((caught_signal == 0) ? MAC_SERV_SHUT_DOWN : MAC_EXIT_SIGNAL);

}


/* << -------------------------------------------------------- >> */
/*                                                                */
/*			Initialize global array with              */
/*	  	        last date and time updating               */
/*                                                                */
/* << -------------------------------------------------------- >> */
void initGlbPriceListArray( void )
{

   bool     do_first_select;
   bool     stop_retrying;
   int      retries;

	int	DateYarpa	= 0,
		TimeYarpa	= 0,
		DateMacabi	= 0,
		TimeMacabi	= 0;

   do_first_select = true;

   for ( retries = 0, stop_retrying = false; retries < SQL_UPDATE_RETRIES
		&& !stop_retrying; retries++ )
   {
     if ( retries )
     {
       sleep( SLEEP_TIME_BETWEEN_RETRIES );
     }

     if ( do_first_select )
     {
		strcpy (TableName, "Price_list");
		ExecSQL (	MAIN_DB, AS400SRV_READ_tables_update,
					&DateYarpa,	&TimeYarpa,
					&TableName,	END_OF_ARG_LIST					);

        if ( SQLERR_code_cmp( SQLERR_access_conflict ) == MAC_TRUE )
        {
          continue;
        }
        else if ( SQLERR_code_cmp( SQLERR_not_found ) == MAC_TRUE )
        {
           DateYarpa = 0;
           TimeYarpa = 0;
        }
        else if ( SQLERR_error_test() )
        {
           DateYarpa = 0;
           TimeYarpa = 0;
           GerrLogReturn ( GerrId,
		       "Failure to select from tables_update,err:%d",
			sqlca.sqlcode);
        }

        do_first_select = false;
     }	

     if ( !do_first_select )
     {
		strcpy (TableName, "Price_list_macabi");
		ExecSQL (	MAIN_DB, AS400SRV_READ_tables_update,
					&DateMacabi,	&TimeMacabi,
					&TableName,		END_OF_ARG_LIST				);

        if ( SQLERR_code_cmp( SQLERR_access_conflict ) == MAC_TRUE )
        {
          continue;
        }
        else if ( SQLERR_code_cmp( SQLERR_not_found ) == MAC_TRUE )
        {
           DateMacabi = 0;
           TimeMacabi = 0;
        }
        else if ( SQLERR_error_test() )
        {
           DateMacabi = 0;
           TimeMacabi = 0;
           GerrLogReturn ( GerrId,
		       "Failure to select from tables_update");
        }

     } // end of else

     stop_retrying = true;

   } // end of for


    glbPriceListUpdateArray[0] = DateYarpa;

    glbPriceListUpdateArray[1] = TimeYarpa;

    glbPriceListUpdateArray[2] = DateMacabi;

    glbPriceListUpdateArray[3] = TimeMacabi;

}    



/* << -------------------------------------------------------- >> */
/*                                                                */
/*   		  Get length of a given message-structure	          */
/*                                                                */
/* << -------------------------------------------------------- >> */
static int get_struct_len (TSystemMsg msg)
{
	int	i;


	for (i = 0; NOT_END_OF_TABLE (i); i++)
	{
		if (msg == MessageSequence[i].ID)
		{
			return (MessageSequence[i].len);
		}
	}

	GerrLogReturn (GerrId, "Message %d not found in messages table.\n",	msg);
	return -1;
}



/* << -------------------------------------------------------- >> */
/*                                                                */
/*   	   Does a socket connection to client still exist         */
/*                                                                */
/* << -------------------------------------------------------- >> */
int pipe_is_broken( TSocket  sock, int milisec )
{
    struct timeval  wt;
//    struct fd_set   rw;  /* Ready to Write */  Yulia linux 20071018
	fd_set   rw;
    int             rc;


    FD_ZERO( &rw );
    FD_SET( sock, &rw );

    milisec *= 1000;

    wt.tv_sec  = milisec / 1000000;       /* seconds      */
    wt.tv_usec = milisec % 1000000;       /* microseconds */

    rc = select( sock + 1, NULL, &rw, NULL, &wt );

    if( rc < 1 )
    {
       return 1;
    }
    else
    {
       return ! FD_ISSET( sock, &rw );
    }
}

/* << -------------------------------------------------------- >> */
/*                                                                */
/*   	 Send a specific message to a single connected Client	  */
/*                                                                */
/* << -------------------------------------------------------- >> */
/*                                                                */
/*       returns:  successs Or failure code			  */
/*                                                                */
/* << -------------------------------------------------------- >> */
static TSuccessStat inform_client (	TConnection  	*connect,
									TSystemMsg   	msg,
									char			text_buffer[]	)
{
    char	build_message[64];
    char	*to_send;
    char	*buf_ptr;
    int		len ;
    bool	failure_flg;
    int		rc ;
    fd_set	write_fds;
    struct timeval timeout;

	// Get data to send.
	// DonR 15Nov2018: Added capability of sending an externally-constructed buffer
	// instead of just a single integer.
	if (text_buffer  != NULL)
	{
		// Set up to send the externally-constructed buffer.
		len		= strlen (text_buffer);
		to_send	= &text_buffer[0];
	}
	else
	{
		// Standard mode - construct a buffer with just the incoming message code.
	    sprintf (build_message, "%d", msg);
		len		= strlen (build_message);
		to_send	= &build_message[0];
	}

	ascii_to_ebcdic_dic (to_send, len);


/*===============================================================
||			Check if data can be sent		||
 ===============================================================*/

    if( pipe_is_broken( connect->socket, 1000 ) )
    {
		GerrLogMini (GerrId, "inform_client: Pipe to AS/400 is broken!");
		puts("the pipe is broken to as400");
		puts("---------------------------");
		fflush(stdout);
		return failure;
    }

/*===============================================================
||			Send data to client			||
 ===============================================================*/

    buf_ptr = to_send;
    failure_flg = false;

    while (len && failure_flg == false)
    {
		rc = send (connect->socket, buf_ptr, len, 0);

		switch (rc)
		{
			case -1:
				GerrLogMini (GerrId, "Error at send()\n" GerrErr, GerrStr);
				failure_flg = true;
				break;

			case 0:
				GerrLogMini (GerrId, "(send) Connection closed by client Error ( %d ) %s.", GerrStr);
				failure_flg = true;
				break;

			default:
				len -= rc;
				buf_ptr += rc;
		}

    } 		// while()

    return (failure_flg == false ?  success : failure) ;

}

/* << -------------------------------------------------------- >> */
/*                                                                */
/*	      close a socket & remove from socket-set             */
/*                                                                */
/* << -------------------------------------------------------- >> */
static void close_socket(
			 TSocket   sock
			 )
{
    close( sock ) ;			/* close socket			*/
}

/*===============================================================
||																||
||			terminate_a_connection()							||
||																||
 ===============================================================*/
static void terminate_a_connection (TConnection	*c, int perform_rollback)
{
	if (perform_rollback)
	{
		DO_ROLLBACK (c);
	}

//	SQLMD_disconnect();
//	SQLMD_disconnect_id( c->db_connection_id ) ;   /* disconnect from DB     */

	close_socket (c->socket);

	printf ("Closing a connection to as400 :\n"
			"-------------------------------\n"
			"Entry : %d"
			"Socket : %d\n",
			c - glbConnectionsArray, c->socket);
	fflush (stdout);

	initialize_struct_to_not_connected (c);	// reset as not connected
}

/*===============================================================
||								||
||		initialize_struct_to_not_connected()		||
||								||
 ===============================================================*/
static void initialize_struct_to_not_connected(
					       TConnection * c
					       )
{
    c->server_state   = 0;
    c->in_tran_flag    = 0;
    c->last_access    = time(NULL);
    c->socket      =   -1;
    c->ip_address     =   -1L;
    c->sys_is_busy    =   false;

    strcpy( c->db_connection_id, "-1" ) ;
}

/*===============================================================
||								||
||			receive_from_socket()			||
||								||
 ===============================================================*/
static TSuccessStat receive_from_socket(
					TConnection *	connect,
					char *		buf,
					int		msg_len
					)
{
    int		received_bytes,
		rc;
    fd_set	ready_for_recv;
    struct timeval	w_time;

/*===============================================================
||			Receive data from client		||
 ===============================================================*/

    while( msg_len > 0 )
    {

/*===============================================================
||			Check if data ready to receive		||
 ===============================================================*/

	FD_ZERO( &ready_for_recv );
	FD_SET( connect->socket,  & ready_for_recv );

	w_time.tv_sec  = 100;		 //ReadTimeout;
	w_time.tv_usec = 0;

	if( pipe_is_broken( connect->socket, 1000 ) )
	{
	    puts("the pipe to as400 is broken..\n----------------");fflush
	    (stdout);
	    return failure;
	}

	rc = select( connect->socket + 1,  &ready_for_recv, 0, 0, &w_time );

	switch( rc )
	{
	    case -1:

		GerrLogReturn(
			      GerrId,
			      "Select() error at function receive_from_socket"
			      ".Error ( %d ) %s.",
			      GerrStr
			      );
		return failure;

	    case 0:

		GerrLogReturn(
			      GerrId,
			      "No data ready for receive"
			      );
		return failure;

	    default:

		if( ! FD_ISSET( connect->socket, &ready_for_recv ) )
		{
		    GerrLogReturn(
				  GerrId,
				  "unexpected error in select()\n"
				  GerrErr,
				  GerrStr
				  );
		    return failure;
		}
		break;
	}		// switch

/*===============================================================
||			Receive data				||
 ===============================================================*/

	received_bytes = recv(
			      connect->socket,
			      buf,
			      msg_len,
			      0
			      ) ;

	switch( received_bytes )
	{
	    case -1:				// RECV ERROR

		GerrLogReturn(
			      GerrId,
			      "recv () error at function receive_from_socket"
			      ".Error ( %d ) %s.",
			      GerrStr
			      );
		return failure;

	    case 0:				// CONNECTION CLOSED

		puts("Connection closed by client");
		puts("---------------------------");
		fflush(stdout);
		return failure;

	    default:				// DATA ARRIVED

		buf	  += received_bytes;
		msg_len   -= received_bytes;

		break;
	}		// switch ( recived bytes
    }			// while( message to come

    return success;

}


/* << ---------------------------------------------------------- >>	*/
/*																	*/
/*  		   Connection is currently active						*/
/*																	*/
/* << ---------------------------------------------------------- >>	*/
static bool connection_is_in_transaction (TConnection *  a_client)
{
    return a_client->in_tran_flag;
}



/* << -------------------------------------------------------- >> */
/*                                                                */
/*  		   Connection is not currently active		          */
/*                                                                */
/* << -------------------------------------------------------- >> */
static bool connection_is_not_in_transaction (TConnection *  a_client)
{
    return !a_client->in_tran_flag;
}



/*===============================================================
||																||
||			get_message_ID()									||
||																||
 ===============================================================*/
static TSystemMsg get_message_ID (TConnection *c)
{
	TSystemMsg	numericID;


	memset (glbMsgBuffer, 0, sizeof(glbMsgBuffer));

	if (receive_from_socket (c, glbMsgBuffer, MSG_HEADER_LEN) == failure)
	{
		return ERROR_ON_RECEIVE;
	}

	glbMsgBuffer[MSG_HEADER_LEN] = 0;

	numericID = atoi (glbMsgBuffer);

	if (!LEGAL_MESSAGE_ID (numericID))
	{
		return BAD_DATA_RECEIVED;
	}

	return	(numericID);

}



/*===============================================================
||																||
||			get_data_message_type()								||
||																||
 ===============================================================*/
static TSystemMsg get_data_message_type (TConnection *c, TSystemMsg msg)
{
	TSystemMsg	numericType;


	// DonR 11Aug2011: As of now, all transactions should include a message-type flag.
	memset (glbMsgBuffer, 0, sizeof(glbMsgBuffer));

	if (receive_from_socket (c, glbMsgBuffer, DATA_MSG_TYPE_LEN) == failure)
	{
		numericType = ERROR_ON_RECEIVE;
	}
	else
	{
		glbMsgBuffer[DATA_MSG_TYPE_LEN] = 0;

		numericType = atoi (glbMsgBuffer);

		if ((numericType < 1) || (numericType > 5))
		{
			numericType = BAD_DATA_RECEIVED;
		}
	}

	return	(numericType);
}



/* << ---------------------------------------------------------- >>	*/
/*																	*/
/*		redefine an active connection as inactive					*/
/*																	*/
/* << ---------------------------------------------------------- >> */
static void set_connection_to_not_in_transaction (TConnection *c)
{
    c->in_tran_flag = 0 ;
}

/* << -------------------------------------------------------- >> */
/*                                                                */
/*		redefine an inactive connection as active         */
/*                                                                */
/* << -------------------------------------------------------- >> */
static void set_connection_to_in_transaction_state (TConnection *client)
{
    client->in_tran_flag = 1 ;
//	// DonR 26Jan2020: ODBC opens transactions automatically at the first DB
//	// update; there is no such thing in ODBC as an explicit transaction-open.
//	// (But until everything is working in ODBC, we still need this!)
//	SQLMD_begin();
//	if (SQLCODE) GerrLogMini (GerrId, "Ignore the preceding error!");
}

/* << -------------------------------------------------------- >> */
/*                                                                */
/*    get an unused connect-structure from glbConnectionsArray    */
/*                                                                */
/* << -------------------------------------------------------- >> */
int	 find_free_connection (void)
{
    int	i ;

    for( i = 0 ; i < MAX_NUM_OF_CLIENTS ; i++ )
    {
	if( glbConnectionsArray[i].server_state  == 0 )
	{
	    return( i ) ;
	}
    }

    return -1 ;

}

/*===============================================================
||								||
||			create_a_new_connection()		||
||								||
 ===============================================================*/
static TSuccessStat create_a_new_connection(
					    void
					    )
{
    TSocket		new_socket ;
    TSocketAddress	client_sock_addr ;
    TConnection	 	*a_connection ;
    int			addr_len ;
    int			rc ;
    int			pos ;		    /*position in array*/
    char		str_pos[32] ;
    int			i;
    int			nonzero = -1;
    struct protoent 	*tcp_prot;

/*===============================================================
||			Free all connections			||
 ===============================================================*/

    for( i = 0 ; i < MAX_NUM_OF_CLIENTS ; i++ )
    {
	a_connection = glbConnectionsArray + i;

	if( a_connection->server_state == 1 )
	{
	    terminate_a_connection( a_connection,
				    a_connection->in_tran_flag == 1 ?
				    WITH_ROLLBACK : WITHOUT_ROLLBACK );
	}
    }

/*===============================================================
||			Get free connection			||
 ===============================================================*/

    pos = find_free_connection();

    if( pos == -1 )
    {
	return failure;
    }

/*===============================================================
||			Connect to DB 				||
 ===============================================================*/

    a_connection = glbConnectionsArray + pos;
    sprintf( str_pos, "%d", pos ) ;

	// DonR 26Jan2020: Use SQLMD_connect() to connect to "old" Informix
	// connection (for now) as well as ODBC DB connections. The do-while
	// loop is mainly for the QA environment, which has a rather wonky
	// Informix connection that sometimes needs retries.
	if (!MAIN_DB->Connected)
 		do
		{
GerrLogMini (GerrId, "PID %d create_a_new_connection() connecting to database.", getpid());
			SQLMD_connect ();
			if (!MAIN_DB->Connected)
			{
				sleep( 10 );
				GerrLogMini (GerrId, "\n\n\nRetrying %s ODBC connect.", MAIN_DB->Name);
			}
		}
		while (!MAIN_DB->Connected);


//	rc = SQLMD_connect_id( str_pos ) ;
//
//	if( ERR_STATE( rc ) )
//	{
//		return failure;
//	}

/*===============================================================
||			Accept tcp/ip connection		||
 ===============================================================*/

    addr_len = sizeof( struct sockaddr ) ;
    new_socket = accept(
			glbListenSocket,
			( struct sockaddr * )&client_sock_addr,
			&addr_len
			) ;

    if( new_socket < 0 )
    {
//		SQLMD_disconnect();
//	SQLMD_disconnect_id( str_pos ) ;

		return failure;
    }

/*===============================================================
||			Set socket options			||
 ===============================================================*/

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
     * Set socket IO options:
     */
    if(
       setsockopt( new_socket, tcp_prot->p_proto, TCP_NODELAY,
		   &nonzero, sizeof( nonzero ) ) == -1
       )
    {
	GerrLogReturn(
		      GerrId,
		      "Unable to set listen-socket's I/O options."
		      GerrErr,
		      GerrStr
		      );
    }

/*===============================================================
||			Set connection attributes		||
 ===============================================================*/

    a_connection->socket 	= new_socket ;
    a_connection->ip_address 	= client_sock_addr.sin_addr.s_addr ;
    a_connection->server_state 	= 1 ;
    a_connection->last_access 	= time(NULL) ;
    a_connection->in_tran_flag 	= 0 ;
    a_connection->sql_err_flg 	= 0 ;

    strcpy( a_connection->db_connection_id, str_pos ) ;

    printf("Creating a new connection to as400 :\n"
	   "------------------------------------\n"
	   "Entry : %d"
	   "Socket : %d\n",
	   pos,
	   new_socket
	   );
	   fflush(stdout);

    return success;

}

/*===============================================================
||								||
||			initialize_connection_array()		||
||								||
 ===============================================================*/
static void initialize_connection_array(
					void
					)
{
    int	i ;

    for( i = 0 ; i < MAX_NUM_OF_CLIENTS ; i++ )
    {
	initialize_struct_to_not_connected( glbConnectionsArray + i ) ;
    }
}

/*===============================================================
||								||
||			make_listen_socket()			||
||								||
 ===============================================================*/
static TSocket make_listen_socket(
				  void
				  )
{
    struct servent	*sp;
    TSocketAddress 	serveraddr;
    TSocket		l_socket;
    int 		nonzero = -1;
    int			rc;
    struct protoent 	*tcp_prot;

/*===============================================================
||			Get Socket				||
 ===============================================================*/

    l_socket = socket( AF_INET, SOCK_STREAM, 0 );

    if( l_socket < 0 )
    {
	GerrLogReturn(
		      GerrId,
		      "Unable to create listen socket..Error ( %d ) %s.",
		      GerrStr
		      );
	return( l_socket );
    }

/*===============================================================
||			Set socket options			||
 ===============================================================*/

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
     * Set socket IO options:
     */
    if(
       setsockopt( l_socket, SOL_SOCKET, SO_REUSEADDR,
		   &nonzero, sizeof( nonzero ) ) == -1	||
//       setsockopt( l_socket, SOL_SOCKET, SO_REUSEPORT,
//		   &nonzero, sizeof( int ) ) == -1	||             Yulia linux 20071018 
       setsockopt( l_socket, SOL_SOCKET, SO_KEEPALIVE,
		   &nonzero, sizeof( int ) ) == -1	||
       setsockopt( l_socket, tcp_prot->p_proto, TCP_NODELAY,
		   &nonzero, sizeof( nonzero ) ) == -1
       )
    {
	GerrLogReturn(
		      GerrId,
		      "Unable to set listen-socket's I/O options."
		      GerrErr,
		      GerrStr
		      );
    }

/*===============================================================
||			Bind socket				||
 ===============================================================*/

    bzero( ( char* )&serveraddr, sizeof( serveraddr ) );
    serveraddr.sin_family      = AF_INET;
    serveraddr.sin_addr.s_addr = htonl( INADDR_ANY );
    serveraddr.sin_port        = htons( AS400_UNIX_PORT );

    rc = bind(
	      l_socket,
	      ( struct sockaddr* )&serveraddr,
	      sizeof( struct sockaddr_in )
	      ) ;

    if( rc < 0 )
    {
	GerrLogReturn(
		      GerrId,
		      "Unable to bind() listen-socket."
		      "Error ( %d ) %s.",
		      GerrStr
		      );
	return( rc );
    }

/*===============================================================
||			Listen on socket			||
 ===============================================================*/
    rc = listen( l_socket, SOMAXCONN );

    if( rc < 0 )
    {
	GerrLogReturn(
		      GerrId,
		      "Failure at listen() at function make_listen_socket()."
		      "Error ( %d ) %s.",
		      GerrStr
		      );
	return( rc );
    }

    return( l_socket);
}

/* << ---------------------------------------------------------- >>	*/
/*																	*/
/*	  	 Set active connection & rollback							*/
/*																	*/
/* << ---------------------------------------------------------- >>	*/
static TSuccessStat DO_ROLLBACK (TConnection *c)
{
	TSuccessStat	retval = success;

	RollbackAllWork ();

	if (SQLCODE == 0)
	{
		set_connection_to_not_in_transaction (c);
	}
	else
	{
		retval = failure;
	}

	return retval;
}

/* << -------------------------------------------------------- >> */
/*                                                                */
/*	 	  handler function executing "commit"             */
/*                                                                */
/* << -------------------------------------------------------- >> */
static int commit_handler (TConnection	*p_connect,
						   TShmUpdate	shm_table_to_update)
{

	int	save_flg	= p_connect->sql_err_flg;


	p_connect->sql_err_flg = 0;

	// Update date of shared memory table if needed
	UpdateSharedMem (shm_table_to_update);
	
	// pipe is broken -- rollback & close connection
	if (pipe_is_broken (p_connect->socket, 100))
	{
		GerrLogMini (GerrId, "Commit_handler: AS/400 pipe is broken!");
		puts ("the pipe to AS4000 is broken...");
		puts ("-------------------------------");
		fflush (stdout);
		terminate_a_connection (p_connect, p_connect->in_tran_flag ? WITH_ROLLBACK : WITHOUT_ROLLBACK);
	}
	else
	{
		if (save_flg)
		{
			// Sql error occured -- rollback & inform client
			DO_ROLLBACK (p_connect);
			GerrLogMini (GerrId, "Commit_handler: DO_ROLLBACK, SQLCODE = %d.", SQLCODE);

			if (inform_client (p_connect, CM_SQL_ERROR_OCCURED, NULL) == failure)
			{
				terminate_a_connection (p_connect, WITHOUT_ROLLBACK);
			}
		}
		else
		{
			// Commit the transaction
			CommitAllWork ();

			if (SQLCODE  == 0)
			{
				set_connection_to_not_in_transaction (p_connect);

				if (inform_client (p_connect, CM_CONFIRMING_COMMIT, NULL) == failure)
				{
					terminate_a_connection (p_connect, WITHOUT_ROLLBACK);
				}
			}
			else
			{
				GerrLogMini (GerrId, "Commit_handler: CommitAllWork() failed, SQLCODE = %d.", SQLCODE);
				terminate_a_connection (p_connect, WITH_ROLLBACK);
			}
		}	// !save_flg
	}	// Pipe is not broken.

	return MSG_HANDLER_SUCCEEDED ;
}


/* << ---------------------------------------------------------- >>	*/
/*																	*/
/*	 	  handler function executing "rollback"						*/
/*																	*/
/* << ---------------------------------------------------------- >>	*/
static int rollback_handler (TConnection *p_connect)
{
	TSuccessStat	rc ;

	p_connect->sql_err_flg = 0;

	rc = DO_ROLLBACK (p_connect);

	if (rc == success)
	{
		if (inform_client (p_connect, CM_CONFIRMING_ROLLBACK, NULL) == failure)
		{
			terminate_a_connection (p_connect, WITH_ROLLBACK);
		}
	}
	else
	{
		terminate_a_connection (p_connect, WITH_ROLLBACK);
	}

	return	MSG_HANDLER_SUCCEEDED ;
}



/* << -------------------------------------------------------- >> */
/*                                                                */
/*	 	  Acknowledge a begin-download or end-download command    */
/*                                                                */
/* << -------------------------------------------------------- >> */
static int acknowledge_command (TConnection *p_connect)
{
    TSuccessStat	rc ;

	if (inform_client (p_connect, CM_CONFIRMING_COMMIT, NULL) == failure)
	{
		GerrLogMini (GerrId, "acknowledge_command: inform_client() failed.");
	    terminate_a_connection( p_connect, WITHOUT_ROLLBACK );
	}

    return	MSG_HANDLER_SUCCEEDED ;
}



/* << -------------------------------------------------------- >> */
/*                                                           	  */
/*                                                           	  */
/*                                                           	  */
/*                M E S S A G E    H A N D L E R S		  */
/*                                                           	  */
/*                                                           	  */
/*                                                           	  */
/* << -------------------------------------------------------- >> */


/* << -------------------------------------------------------- >> */
/*                                                           	  */
/*    Attension:						  */
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
/*		 3. Change parsing function to one of the three:  */
/*		    get_short(), get_long() or get_string(),	  */
/*								  */
/*								  */
/*      B.  If you also wish to change message COMPOSITION	  */
/*	    ( == the fields passed in a given message )you	  */
/*	    must change the length of the record attached to	  */
/*	    the message - RECORD_LEN_XXXX in file DBFields.h	  */
/*	    section = "Length of record".			  */
/*                                                           	  */
/* << -------------------------------------------------------- >> */


/* << -------------------------------------------------------- >> */
/*                                                                */
/*     handler for message 2001 - upd/ins to table PHARMACY       */
/*                                                                */
/* << -------------------------------------------------------- >> */
static int message_2001_handler(
				TConnection	*p_connect,
				int		data_len,
				bool		do_insert
				)
{
/* << -------------------------------------------------------- >> */
/*			  local variables			  */
/* << -------------------------------------------------------- >> */
  char          *PosInBuff;		/* current pos in data buffer	    */
  int		ReadInput;		/* amount of Bytesread from buffer  */
  int		retcode;		/* code returned from function	    */
  bool		stop_retrying;		/* if true - return from function   */
  int		n_retries;		/* retrying counter		    */
  int		i;


	/* << -------------------------------------------------------- >> */
	/*			    DB variables			  */
	/* << -------------------------------------------------------- >> */
	Tpharmacy_code		v_pharmacy_code;
	Tinstitued_code		v_institued_code;
	Tpharmacy_name		v_pharmacy_name;
	Tpharmacist_last_na	v_pharmacist_last_na;
	Tpharmacist_first_n	v_pharmacist_first_n;
	Tstreet_and_no		v_street_and_no;
	Tcity					v_city;
	Tphone				v_phone;
	short					owner;
	short					hesder_category;
	Tcomm_type			v_comm_type;
	Tphone_1				v_phone_1;
	Tphone_2				v_phone_2;
	Tphone_3				v_phone_3;
	TPermissionType		v_permission_type;
	Tprice_list_code		v_price_list_code;
	int					sysdate;
	int					systime;
	TSoftwareVersion		v_software_version_need;
	short			v_card;
	Tfee			v_fps_fee_level;
	Tfee			v_fps_fee_lower;
	Tfee			v_fps_fee_upper;
	Tpharm_credit_flag	v_pharm_credit_flag;	// DonR 16Sep2003
	short			v_maccabicare_flag;
	short			v_meishar_enabled;		// DonR 15Jan2014
	short			can_sell_future_rx;		// DonR 10Oct2018

	// DonR 21Oct2003 begin.
	short			v_contract_type;
	short			v_contract_tax;
	int			v_contract_code;
	int			v_contract_effective;
	// DonR 21Oct2003 end.

	short			v_pharm_category;	// DonR 23Aug2009

	// DonR 01Jun2021 User Story #144324 begin.
	int				web_pharmacy_code;
	short			vat_exempt;
	short			software_house;
	short			order_originator;
	short			order_fulfiller;
	// DonR 01Jun2021 User Story #144324 end.

	// Marianna 20Apr2023 User Story #432608
	short			ConsignmentPharmacy;
	short			PharmNohalEnabled;

	// Marianna 22May2024 User Story #314887
	short			mahoz;

  sysdate = GetDate();
  systime = GetTime();


/* << -------------------------------------------------------- >> */
/*			read input buffer			  */
/* << -------------------------------------------------------- >> */
  INIT_ERROR_DETECTION_MECHANISM;
  PosInBuff = glbMsgBuffer;


	v_pharmacy_code			= get_long	(&PosInBuff, Tpharmacy_code_len			);	RETURN_ON_ERROR;
	v_institued_code		= get_short	(&PosInBuff, Tinstitued_code_len		);	RETURN_ON_ERROR;

	get_string	(&PosInBuff, v_pharmacy_name,		Tpharmacy_name_len			);	switch_to_win_heb ((unsigned char *)v_pharmacy_name			);
	get_string	(&PosInBuff, v_pharmacist_last_na,	Tpharmacist_last_na_len		);	switch_to_win_heb ((unsigned char *)v_pharmacist_last_na	);
	get_string	(&PosInBuff, v_pharmacist_first_n,	Tpharmacist_first_n_len		);	switch_to_win_heb ((unsigned char *)v_pharmacist_first_n	);
	get_string	(&PosInBuff, v_street_and_no,		Tstreet_and_no_len			);	switch_to_win_heb ((unsigned char *)v_street_and_no			);
	get_string	(&PosInBuff, v_city,				Tcity_len					);	switch_to_win_heb ((unsigned char *)v_city					);
	get_string	(&PosInBuff, v_phone,				Tphone_len					);

	owner					= get_short	(&PosInBuff, 1							);	RETURN_ON_ERROR;
	v_comm_type				= get_short	(&PosInBuff, Tcomm_type_len				);	RETURN_ON_ERROR;

	get_string	(&PosInBuff, v_phone_1,				Tphone_1_len				);
	get_string	(&PosInBuff, v_phone_2,				Tphone_2_len				);
	get_string	(&PosInBuff, v_phone_3,				Tphone_3_len				);

	v_permission_type		= get_short	(&PosInBuff, TPermissionType_len		);	RETURN_ON_ERROR;
	v_price_list_code		= get_short	(&PosInBuff, Tprice_list_code_len		);	RETURN_ON_ERROR;
	v_card					= get_short	(&PosInBuff, 1							);	RETURN_ON_ERROR;

	// DonR 13APR2003 Begin (Fee Per Service):
	v_fps_fee_level			= get_long	(&PosInBuff, Tfee_len					);	RETURN_ON_ERROR;
	v_fps_fee_lower			= get_long	(&PosInBuff, Tfee_len					);	RETURN_ON_ERROR;
	v_fps_fee_upper			= get_long	(&PosInBuff, Tfee_len					);	RETURN_ON_ERROR;
	// DonR 13APR2003 End.

	// DonR 16Sep2003 Begin.
	v_pharm_credit_flag		= get_short	(&PosInBuff, Tpharm_credit_flag_len		);	RETURN_ON_ERROR;
	// DonR 16Sep2003 End.

	// DonR 21Oct2003 begin.
	v_contract_type			= get_short	(&PosInBuff, 1							);	RETURN_ON_ERROR;
	v_contract_tax			= get_short	(&PosInBuff, 1							);	RETURN_ON_ERROR;
	v_contract_code			= get_long	(&PosInBuff, 5							);	RETURN_ON_ERROR;
	v_contract_effective	= get_long	(&PosInBuff, 8							);	RETURN_ON_ERROR;
	// DonR 21Oct2003 end.

	v_maccabicare_flag		= get_short	(&PosInBuff, 1							);	RETURN_ON_ERROR;
	v_pharm_category		= get_short	(&PosInBuff, 2							);	RETURN_ON_ERROR;
	v_meishar_enabled		= get_short	(&PosInBuff, 1							);	RETURN_ON_ERROR;	// DonR 15Jan2014
	can_sell_future_rx		= get_short	(&PosInBuff, 1							);	RETURN_ON_ERROR;	// DonR 10Oct2018

	// DonR 01Jun2021 User Story #144324 begin.
	web_pharmacy_code		= get_long	(&PosInBuff, Tpharmacy_code_len			);	RETURN_ON_ERROR;
	vat_exempt				= get_short	(&PosInBuff, 1							);	RETURN_ON_ERROR;
	software_house			= get_short	(&PosInBuff, 2							);	RETURN_ON_ERROR;
	order_originator		= get_short	(&PosInBuff, 1							);	RETURN_ON_ERROR;
	order_fulfiller			= get_short	(&PosInBuff, 1							);	RETURN_ON_ERROR;
	// DonR 01Jun2021 User Story #144324 end.

	// Marianna 20Apr2023 User Story #432608
	ConsignmentPharmacy		= get_short	(&PosInBuff, 1							);	RETURN_ON_ERROR;
	PharmNohalEnabled		= get_short	(&PosInBuff, 1							);	RETURN_ON_ERROR;

	// Marianna 22May2024 User Story #314887
	mahoz					= get_short (&PosInBuff, 2							);	RETURN_ON_ERROR;


	strcpy (v_software_version_need, "-");

	// DonR 04Feb2019: Hesder_category is a derived field; it's equal to Owner *except*
	// when Owner = 1 (private pharmacy) and Category = 6 (new value = no hesder).
	// Then it gets a value of 2 = non-hesder private pharmacy.
	hesder_category			= ((owner == 1) && (v_pharm_category == 6)) ? 2 : owner;


/* << -------------------------------------------------------- >> */
/*			    debug messages			  */
/* << -------------------------------------------------------- >> */

  /************************   COMMENTED OUT  ************************
      printf( "\n\nTpharmacy_code == %d\n",    v_pharmacy_code );
      printf( "Tinstitued_code == %d\n",	v_institued_code );
      printf( "Tpharmacy_name == >%s<	\n",	v_pharmacy_name );
      printf( "Tpharmacist_last_na == >%s<\n",  v_pharmacist_last_na );
      printf( "Tpharmacist_first_n == >%s<\n",  v_pharmacist_first_n );
      printf( "Tstreet_and_no == >%s<	\n",	v_street_and_no );
      printf( "Tcity == >%s<\n",		v_city );
      printf( "Tphone == >%s<\n",		v_phone );
      printf( "Towner == %d\n",		        v_owner );
      printf( "Tcomm_type == %d\n",		v_comm_type );
      printf( "Tphone_1 == >%s<\n",		v_phone_1 );
      printf( "Tphone_2 == >%s<\n",		v_phone_2 );
      printf( "Tphone_3 == >%s<\n",		v_phone_3 );
      printf( "TPermissionType == %d\n",	v_permission_type );
      printf( "Tprice_list_code == %d\n\n\n",   v_price_list_code );
      fflush( stdout );
  **************************************************/


  /*
   *   verify all data was read:
   */
  ReadInput = PosInBuff - glbMsgBuffer;
  StaticAssert( ReadInput == data_len );

  // 15Jan2014: For now at least, the Meishar Enabled flag will be set by AS/400 only for
  // private pharmacies. Just to keep things clean, force a value of 1 for Maccabi pharmacies.
  if (v_permission_type == 1)
	  v_meishar_enabled = 1;
//  // THE CODE BELOW IS TEMPORARY!
//  else
//	  v_meishar_enabled = 0;	// TEMPORARY: force Meishar Enabled flag to zero for all private pharmacies,
//								// since we're not yet getting any value from AS/400.

/* << -------------------------------------------------------- >> */
/*	   up to SQL_UPDATE_RETRIES retries will be made	  */
/*		    to overcome access conflict			  */
/* << -------------------------------------------------------- >> */
  retcode = ALL_RETRIES_WERE_USED;
  stop_retrying = false;

  for( n_retries = 0;
       ( n_retries < SQL_UPDATE_RETRIES ) && ( stop_retrying == false );
       n_retries++ )
    {

      /*
       * sleep for a while:
       */
      if( n_retries )
      {
         sleep( SLEEP_TIME_BETWEEN_RETRIES );
      }

      /*
       *  try to insert/update new rec with the order depending on "do_insert":
       */
      for( i = 0;  i < 2;  i++ )
	{
	  if( do_insert == ATTEMPT_INSERT_FIRST )
	    {
			ExecSQL (	MAIN_DB, AS400SRV_T2001_INS_pharmacy,
						&v_pharmacy_code,			&v_institued_code,			&v_pharmacy_name,
						&v_pharmacist_last_na,		&v_pharmacist_first_n,		&v_street_and_no,
						&v_city,					&v_phone,					&owner,
						&hesder_category,			&v_comm_type,				&v_phone_1,

						&v_phone_2,					&v_phone_3,					&v_permission_type,
						&v_price_list_code,			&sysdate,					&systime,
						&v_software_version_need,	&ShortZero,					&ROW_NOT_DELETED,
						&IntZero,					&ShortZero,					&v_card,

						&v_fps_fee_level,			&v_fps_fee_lower,			&v_fps_fee_upper,
						&v_pharm_credit_flag,		&v_contract_type,			&v_contract_tax,
						&v_contract_code,			&v_contract_effective,		&v_maccabicare_flag,
						&v_meishar_enabled,			&can_sell_future_rx,		&v_pharm_category,

						&web_pharmacy_code,			&vat_exempt,				&software_house,
						&order_originator,			&order_fulfiller,			
						&ConsignmentPharmacy,		&PharmNohalEnabled,			&mahoz,			/* Marianna 22May2024 User Story #314887 */ 
						END_OF_ARG_LIST																	);	

              HANDLE_INSERT_SQL_CODE( "2001",
					p_connect,
				      &do_insert,
				      &retcode,
				      &stop_retrying );

	    }
	  else
	    {
			ExecSQL (	MAIN_DB, AS400SRV_T2001_UPD_pharmacy,
						&v_institued_code,			&v_pharmacy_name,		&v_pharmacist_last_na,
						&v_pharmacist_first_n,		&v_street_and_no,		&v_city,
						&v_phone,					&owner,					&hesder_category,
						&v_comm_type,				&v_phone_1,				&v_phone_2,
						&v_phone_3,					&v_permission_type,		&v_price_list_code,
						&v_software_version_need,	&sysdate,				&systime,
						&v_card,					&v_fps_fee_level,		&v_fps_fee_lower,
						&v_fps_fee_upper,			&v_pharm_credit_flag,	&v_contract_type,
						&v_contract_tax,			&v_contract_code,		&v_contract_effective,
						&v_maccabicare_flag,		&v_meishar_enabled,		&can_sell_future_rx,
						&v_pharm_category,			&web_pharmacy_code,		&vat_exempt,
						&software_house,			&order_originator,		&order_fulfiller,
						&ConsignmentPharmacy,		&PharmNohalEnabled,		&mahoz,						/* Marianna 22May2024 User Story #314887 */
						&v_pharmacy_code,			&ROW_NOT_DELETED,		END_OF_ARG_LIST				);

			HANDLE_UPDATE_SQL_CODE ("2001", p_connect, &do_insert, &retcode, &stop_retrying);
	    }

	}/* for( i... */

    }/* for( n_retries... */

  if( n_retries == SQL_UPDATE_RETRIES )
  {
     WRITE_ACCESS_CONFLICT_TO_LOG( "2001" );
  }


  if (retcode == MSG_HANDLER_SUCCEEDED)
  {
	  if (do_insert == ATTEMPT_INSERT_FIRST)
		  rows_added++;
	  else
		  rows_updated++;
  }

  
  return retcode;
} /* end of msg 2001 handler */


/* << -------------------------------------------------------- >> */
/*                                                                */
/*     handler for message 2002 - delete rec from PHARMACY	  */
/*																  */
/*                                                                */
/* << -------------------------------------------------------- >> */
static int message_2002_handler(
				TConnection	*p_connect,
				int		data_len
				)
{

/* << -------------------------------------------------------- >> */
/*			  local variables			  */
/* << -------------------------------------------------------- >> */
  char          *PosInBuff;		/* current pos in data buffer	    */
  int		ReadInput;		/* amount of Bytesread from buffer  */
  int		retcode;		/* code returned from function	    */
  int		n_retries;		/* retrying counter		    */
  int		i;
  int           db_changed_flg;

	/* << -------------------------------------------------------- >> */
	/*			    DB variables			  */
	/* << -------------------------------------------------------- >> */
	Tpharmacy_code	v_pharmacy_code;


/* << -------------------------------------------------------- >> */
/*			  read input buffer			  */
/* << -------------------------------------------------------- >> */
  INIT_ERROR_DETECTION_MECHANISM;
  PosInBuff = glbMsgBuffer;

  v_pharmacy_code	=  get_long(
				    &PosInBuff,
				    Tpharmacy_code_len
				    );
  RETURN_ON_ERROR;

/* << -------------------------------------------------------- >> */
/*		      verify all deta was read			  */
/* << -------------------------------------------------------- >> */
  ReadInput = PosInBuff - glbMsgBuffer;
  StaticAssert( ReadInput == data_len );


/* << -------------------------------------------------------- >> */
/*			   attempt delete			  */
/* << -------------------------------------------------------- >> */
  retcode = ALL_RETRIES_WERE_USED;
  db_changed_flg = 0;

  for( n_retries = 0;
       n_retries < SQL_UPDATE_RETRIES;
       n_retries++ )
    {

      /*
       * sleep for a while:
       */
      if( n_retries )
      {
          sleep( SLEEP_TIME_BETWEEN_RETRIES );
      }

		ExecSQL (	MAIN_DB, AS400SRV_T2002_DEL_pharmacy,
					&v_pharmacy_code, &ROW_NOT_DELETED, END_OF_ARG_LIST	);

      HANDLE_DELETE_SQL_CODE( "2002", p_connect, &retcode, &db_changed_flg );

      break;
    }

  if( n_retries == SQL_UPDATE_RETRIES )
  {
     WRITE_ACCESS_CONFLICT_TO_LOG( "2002" );
  }

  if (retcode == MSG_HANDLER_SUCCEEDED)
	  rows_deleted++;

  return retcode;
} /* end of msg 2002 handler */



/* << -------------------------------------------------------- >> */
/*                                                                */
/*     handler for message 2003 - upd/ins to SPECIAL_PRESCS	  */
/*                                                                */
/* << -------------------------------------------------------- >> */
static int message_2003_handler(
				TConnection	*p_connect,
				int		data_len,
				bool		do_insert
				)
{

/* << -------------------------------------------------------- >> */
/*			  local variables			  */
/* << -------------------------------------------------------- >> */
  char          *PosInBuff;		/* current pos in data buffer	    */
  int		ReadInput;		/* amount of Bytesread from buffer  */
  int		retcode;		/* code returned from function	    */
  bool		stop_retrying;		/* if true - return from function   */
  int		n_retries;		/* retrying counter		    */
  int		i;


/* << -------------------------------------------------------- >> */
/*			    DB variables			  */
/* << -------------------------------------------------------- >> */
	Tpharmacy_code		v_pharmacy_code;
	Tinstitued_code		v_institued_code;
	Tmember_id			v_member_id;
	Tmem_id_extension	v_mem_id_extension;
	Tdoctor_id_type		v_doctor_id_type;
	Tdoctor_id			v_doctor_id;
	Tspecial_presc_num	v_special_presc_num;
	Tspecial_presc_ext	v_special_presc_ext;
	Tspecial_presc_ext	ishur_source_send;	// DonR 01May2012 - processed version, set to either 1 or 3
											// depending on Confirming Authority.
	Tprice_code			v_member_price_code;
	Tlargo_code			v_largo_code;
	Top_in_dose			v_op_in_dose;
	Tunits_in_dose  	v_units_in_dose;
	Tstart_use_date		v_start_use_date;
	Tstop_use_date		v_stop_use_date;
	Tauthority_id		v_authority_id;
	Tconfirm_authority_	v_confirm_authority_;
	short				v_insurance_used;
	Tconfirm_reason		v_confirm_reason;
	Tdose_num			v_dose_num;
	Tdose_renew_freq	v_dose_renew_freq;
	Tpriv_pharm_sale_ok	v_priv_pharm_sale_ok;
	Tconfirmation_type	v_confirmation_type;
	Thospital_code		v_hospital_code;
	Tconst_sum_to_pay	v_const_sum_to_pay;
	Tin_health_pack		v_in_health_basket;
	int					sysdate;
	int					systime;
	TTreatmentCategory	v_treatment_category;
	short				v_form_number;		// DonR 27Mar2007
	short				v_tikra_flag;		// DonR 27Mar2007
	short				v_tikra_type_code;	// DonR 25May2010
	short				needs_29_g;			// DonR 09Aug2017

	/*Yulia add next 5 fields 20050203*/
	short				v_country_center;
	int					v_mess_code  ;
	short				v_reason_code  ;
	char				v_mess_text [60];  
	int					v_made_date  ;  
	short				v_doc_saw_ishur;
	short				v_abrupt_close_flg;
	int					v_portal_update_dt;	// DonR 08Apr2013.

	// DonR 06Feb2006: New Quantity Limits for Ishurim.
	short				v_QtyLim_ingr;
	short				v_QtyLim_flg;
	char				v_QtyLim_units[4];
	double				v_QtyLim_qty_each;
	double				v_QtyLim_per_day;
	short				v_QtyLim_treat_days;
	short				v_QtyLim_course_len;
	short				v_QtyLim_courses;
	double				v_QtyLim_all_ishur;
	double				v_QtyLim_all_warn;
	double				v_QtyLim_all_err;
	double				v_QtyLim_per_course;
	double				v_QtyLim_course_wrn;
	double				v_QtyLim_course_err;
	double				v_ql_all_ishur_std;
	double				v_ql_all_warn_std;
	double				v_ql_all_err_std;
	double				v_ql_per_course_std;
	double				v_ql_course_wrn_std;
	double				v_ql_course_err_std;
	double				v_ql_per_day_std;
	int					v_QtyLim_hist_start;
	int					member_diagnosis;
	short   			monthly_qlm_flag;		// Marianna 05Aug2020 CR #27955
	double  			monthly_dosage;			// Marianna 05Aug2020 CR #27955
	double				monthly_dosage_std; 	// Marianna 06Aug2020 CR #27955
	short   			monthly_duration;		// Marianna 05Aug2020 CR #27955
	short 				cont_approval_flag;		// Marianna 05Aug2020 CR #28605
	int  				orig_confirm_num ;		// Marianna 05Aug2020 CR #28605
	short				exceptional_ishur;		// Marianna 19Jan2023 User Story #276372


	sysdate = GetDate();
	systime = GetTime();

	/* << -------------------------------------------------------- >> */
	/*			read input buffer			  */
	/* << -------------------------------------------------------- >> */
	INIT_ERROR_DETECTION_MECHANISM;
	PosInBuff = glbMsgBuffer;

	v_pharmacy_code		= get_long	(&PosInBuff, Tpharmacy_code_len		); RETURN_ON_ERROR;
	v_institued_code	= get_short	(&PosInBuff, Tinstitued_code_len	); RETURN_ON_ERROR;
	v_member_id			= get_long	(&PosInBuff, Tmember_id_len			); RETURN_ON_ERROR;
	v_mem_id_extension	= get_short	(&PosInBuff, Tmem_id_extension_len	); RETURN_ON_ERROR;
	v_doctor_id_type	= get_short	(&PosInBuff, Tdoctor_id_type_len	); RETURN_ON_ERROR;
	v_doctor_id			= get_long	(&PosInBuff, Tdoctor_id_len			); RETURN_ON_ERROR;
	v_special_presc_num	= get_long	(&PosInBuff, Tspecial_presc_num_len	); RETURN_ON_ERROR;
	v_special_presc_ext	= get_short	(&PosInBuff, Tspecial_presc_ext_len	); RETURN_ON_ERROR;
	v_member_price_code	= get_short	(&PosInBuff, Tprice_code_len		); RETURN_ON_ERROR;
	v_largo_code		= get_long	(&PosInBuff, 9						); RETURN_ON_ERROR;	//Marianna 14Jul2024 User Story #330877
	v_op_in_dose		= get_long	(&PosInBuff, Top_in_dose_len		); RETURN_ON_ERROR;
	v_units_in_dose  	= get_long	(&PosInBuff, Tunits_in_dose_len		); RETURN_ON_ERROR;
	v_start_use_date	= get_long	(&PosInBuff, Tstart_use_date_len	); RETURN_ON_ERROR;
	v_stop_use_date		= get_long	(&PosInBuff, Tstop_use_date_len		); RETURN_ON_ERROR;
	v_authority_id		= get_long	(&PosInBuff, Tauthority_id_len		); RETURN_ON_ERROR;
	v_confirm_authority_= get_short	(&PosInBuff, Tconfirm_authority__len); RETURN_ON_ERROR;
	v_insurance_used	= get_short	(&PosInBuff, Tpresc_source_len		); RETURN_ON_ERROR;
	v_confirm_reason	= get_short (&PosInBuff, Tconfirm_reason_len	); RETURN_ON_ERROR;
	v_dose_num			= get_short	(&PosInBuff, Tdose_num_len			); RETURN_ON_ERROR;
	v_dose_renew_freq	= get_short	(&PosInBuff, Tdose_renew_freq_len	); RETURN_ON_ERROR;
	v_priv_pharm_sale_ok= get_short	(&PosInBuff, Tpriv_pharm_sale_ok_len); RETURN_ON_ERROR;
	v_confirmation_type	= get_short	(&PosInBuff, Tconfirmation_type_len	); RETURN_ON_ERROR;
	v_hospital_code		= get_long	(&PosInBuff, Thospital_code_len		); RETURN_ON_ERROR;
	v_const_sum_to_pay	= get_long	(&PosInBuff, Tconst_sum_to_pay_len	); RETURN_ON_ERROR;
	v_in_health_basket	= get_long	(&PosInBuff, Tin_health_pack_len	); RETURN_ON_ERROR;
	v_treatment_category= get_short	(&PosInBuff, Ttreatment_category_len); RETURN_ON_ERROR;
	v_country_center	= get_short	(&PosInBuff, Tcountry_center_len	); RETURN_ON_ERROR;
	v_mess_code			= get_long	(&PosInBuff, Tmess_code_len			); RETURN_ON_ERROR;
	v_reason_code		= get_short	(&PosInBuff, Treason_mess_len		); RETURN_ON_ERROR;

	get_string (&PosInBuff,	v_mess_text, Tmess_text_len); 
	switch_to_win_heb ((unsigned char *)v_mess_text);

	// DonR 29May2017: For some reason, we got a rash of integrity violations on this field
	// today. Temporary hack was to replace the value with a single space; now let's try
	// more of a real fix! (Addendum 30May: It appears that the problem was specifically
	// due to zero-length text violating the NOT NULL constraint - so the second part of
	// the fix below is the relevant one.)
	v_mess_text [Tmess_text_len] = (char)0;	// Reinforce maximum length.
	if (v_mess_text [0] < 1)
		strcpy (v_mess_text, " ");				// Reinforce minimum contents of one space.

	v_made_date			= get_long	(&PosInBuff, Tmade_date_len			); RETURN_ON_ERROR;
	v_doc_saw_ishur		= get_short	(&PosInBuff, TGenericFlag_len		); RETURN_ON_ERROR;
	v_abrupt_close_flg	= get_short	(&PosInBuff, TGenericFlag_len		); RETURN_ON_ERROR;

	v_QtyLim_ingr		= get_short	(&PosInBuff, 4						); RETURN_ON_ERROR;
	v_QtyLim_flg		= get_short	(&PosInBuff, TGenericFlag_len		); RETURN_ON_ERROR;
	/* v_QtyLim_units */  get_string(&PosInBuff, v_QtyLim_units, 3		); 
	v_QtyLim_qty_each	= get_double(&PosInBuff, 20						); RETURN_ON_ERROR;
	v_QtyLim_per_day	= get_double(&PosInBuff, 20						); RETURN_ON_ERROR;
	v_QtyLim_treat_days	= get_short	(&PosInBuff, 2						); RETURN_ON_ERROR;
	v_QtyLim_course_len	= get_short	(&PosInBuff, 3						); RETURN_ON_ERROR;
	v_QtyLim_courses	= get_short	(&PosInBuff, 2						); RETURN_ON_ERROR;
	v_QtyLim_all_ishur	= get_double(&PosInBuff, 20						); RETURN_ON_ERROR;
	v_QtyLim_all_warn	= get_double(&PosInBuff, 20						); RETURN_ON_ERROR;
	v_QtyLim_all_err	= get_double(&PosInBuff, 20						); RETURN_ON_ERROR;
	v_QtyLim_per_course	= get_double(&PosInBuff, 20						); RETURN_ON_ERROR;
	v_QtyLim_course_wrn	= get_double(&PosInBuff, 20						); RETURN_ON_ERROR;
	v_QtyLim_course_err	= get_double(&PosInBuff, 20						); RETURN_ON_ERROR;

	// DonR 27Mar2007: New fields for Clicks 10.1
	v_form_number		= get_short	(&PosInBuff, 3						); RETURN_ON_ERROR;
	v_tikra_flag		= get_short	(&PosInBuff, 1						); RETURN_ON_ERROR;
	v_tikra_type_code	= get_short	(&PosInBuff, 3						); RETURN_ON_ERROR;

	// DonR 08Apr2013: New Portal Update Date to simplify (and correct) doctor-must-view-ishurim logic.
	v_portal_update_dt	= get_long	(&PosInBuff, 8						); RETURN_ON_ERROR;

	v_QtyLim_hist_start	= get_long	(&PosInBuff, 8						); RETURN_ON_ERROR;
	needs_29_g			= get_short	(&PosInBuff, 2						); RETURN_ON_ERROR;
	member_diagnosis	= get_long	(&PosInBuff, 6						); RETURN_ON_ERROR;

	// Marianna 05Aug2020 CR #27955
	monthly_qlm_flag	= get_short	(&PosInBuff, 1						); RETURN_ON_ERROR;	
	monthly_dosage		= get_double(&PosInBuff, 20						); RETURN_ON_ERROR;			
	monthly_duration	= get_short	(&PosInBuff, 2						); RETURN_ON_ERROR;

	// Marianna 05Aug2020 CR #28605
	cont_approval_flag	= get_short	(&PosInBuff, 1						); RETURN_ON_ERROR;	
	orig_confirm_num	= get_long	(&PosInBuff, 8						); RETURN_ON_ERROR;

	// Marianna 19Jan2023 User Story #276372 
	exceptional_ishur	= get_short	(&PosInBuff, 1						); RETURN_ON_ERROR;


  // Translate AS400 Treatment Category into Unix version.
  i = v_treatment_category;
  switch (i)
  {
	  case TREATMENT_TYPE_NOCHECK:		break;		// No translation needed.

	  case TREAT_TYPE_AS400_FERTILITY:	v_treatment_category = TREATMENT_TYPE_FERTILITY;	break;

	  case TREAT_TYPE_AS400_DIALYSIS:	v_treatment_category = TREATMENT_TYPE_DIALYSIS;		break;

	  // Marianna 11May2025: User Story #410137
	  case TREAT_TYPE_AS400_PREP:		v_treatment_category = TREATMENT_TYPE_PREP;			break;

	  case TREAT_TYPE_AS400_SMOKING:	v_treatment_category = TREATMENT_TYPE_SMOKING;		break;

	  // If we get an unrecognized value, set it to NOCHECK. (Is this really a good idea?)
	  default:							v_treatment_category = TREATMENT_TYPE_NOCHECK;		break;
  }

	// DonR 26Oct2008: Set "old" health-basket flag to 1 or 0 based on the value received
	// from AS400, and move the actual value received to the "new" health-basket flag
	// variable.
	PROCESS_HEALTH_BASKET;

	// DonR 01May2012: For "Nihul Tikrot", we need a "processed" version of the Ishur Source based on the ishur's
	// Authorizing Authority. If this is zero or >= 49, the Ishur Source passed to the AS/400 "nihul tikrot"
	// program (and stored in prescription_drugs) should always be 1; otherwise, it should always be 3.
	// (This is usually already the case - but, for whatever reasons, there are exceptions.)
	if ((v_confirm_authority_ == 0) || (v_confirm_authority_ > 48))
	{
		ishur_source_send = 1;
	}
	else
	{
		ishur_source_send = 3;
	}

	// DonR 20Jun2013: Set up "standardized" ingredient-quantity fields. These will give
	// ingredient limits in mg when the original unit is mcg, mg, or g; IU when the
	// original unit is IU or MIU; and will be the same as the original quantity for any
	// other unit, such as mL.
	v_ql_all_ishur_std	= ConvertUnitAmount (v_QtyLim_all_ishur,	v_QtyLim_units);
	v_ql_all_warn_std	= ConvertUnitAmount (v_QtyLim_all_warn,		v_QtyLim_units);
	v_ql_all_err_std	= ConvertUnitAmount (v_QtyLim_all_err,		v_QtyLim_units);
	v_ql_per_course_std	= ConvertUnitAmount (v_QtyLim_per_course,	v_QtyLim_units);
	v_ql_course_wrn_std	= ConvertUnitAmount (v_QtyLim_course_wrn,	v_QtyLim_units);
	v_ql_course_err_std	= ConvertUnitAmount (v_QtyLim_course_err,	v_QtyLim_units);
	v_ql_per_day_std	= ConvertUnitAmount (v_QtyLim_per_day,		v_QtyLim_units);
	monthly_dosage_std	= ConvertUnitAmount (monthly_dosage,		v_QtyLim_units); 	// Marianna 06Aug2020 CR #27955


/* << -------------------------------------------------------- >> */
/*			    debug messages			  */
/* << -------------------------------------------------------- >> */
  /************************    COMMENTED OUT : *************************
  printf( "pharmacy_code == %d\n",	v_pharmacy_code );
  printf( "institued_code == %d\n", 	v_institued_code );
  printf( "member_id == %d\n",		v_member_id );
  printf( "mem_id_extension == %d\n", 	v_mem_id_extension );
  printf( "doctor_id_type == %d\n", 	v_doctor_id_type );
  printf( "doctor_id == %d\n",		v_doctor_id );
  printf( "special_presc_num == %d\n", v_special_presc_num );
  printf( "special_presc_ext == %d\n", 	v_special_presc_ext );
  printf( "member_price_code == %d\n", 	v_member_price_code );
  printf( "largo_code == %d\n", 	v_largo_code );
  printf( "op_in_dose   == %d\n", 	v_op_in_dose );
  printf( "units_in_dose   == %d\n", 	v_units_in_dose );
  printf( "start_use_date == %d\n", 	v_start_use_date );
  printf( "stop_use_date == %d\n", 	v_stop_use_date );
  printf( "authority_id == %d\n", 	v_authority_id );
  printf( "confirm_authority_ == %d\n", v_confirm_authority_ );
  printf( "insurance_used == %d\n", 	v_insurance_used );
  printf( "confirm_reason == %d\n",     v_confirm_reason );
  printf( "dose_num == %d\n", 		v_dose_num );
  printf( "dose_renew_freq == %d\n", 	v_dose_renew_freq );
  printf( "priv_pharm_sale_ok == %d\n", v_priv_pharm_sale_ok );
  printf( "confirmation_type == %d\n", 	v_confirmation_type );
  printf( "hospital_code == %d\n", 	v_hospital_code );
  fflush( stdout );
  *************************************************/


  /*
   *   verify all data was read:
   */
  ReadInput = PosInBuff - glbMsgBuffer;
  StaticAssert (ReadInput == data_len);


/* << -------------------------------------------------------- >> */
/*	   up to SQL_UPDATE_RETRIES retries will be made	  */
/*		    to overcome access conflict			  */
/* << -------------------------------------------------------- >> */
  retcode = ALL_RETRIES_WERE_USED;
  stop_retrying = false;

  for(
      n_retries = 0;
      ( n_retries < SQL_UPDATE_RETRIES ) && ( stop_retrying == false );
      n_retries++
      )
    {

	  // sleep for a while:
      if (n_retries)
      {
          sleep (SLEEP_TIME_BETWEEN_RETRIES);
      }

		// try to insert/update new rec with the order depending on "do_insert":
		for (i = 0; i < 2; i++)
		{
			// DonR 22Aug2005: Don't set member's Special Prescription Flag true
			// unless the incoming data is an approved (and thus usable) ishur.
			if (v_confirmation_type == 1)
			{
				ExecSQL (	MAIN_DB, AS400SRV_T2003_UPD_members_spec_presc,
							&v_member_id, &v_mem_id_extension, END_OF_ARG_LIST	);
			}


			if (do_insert == ATTEMPT_INSERT_FIRST)
			{
				ExecSQL (	MAIN_DB, AS400SRV_T2003_INS_special_prescs,
							&v_pharmacy_code,		&v_institued_code,		&v_member_id,
							&v_mem_id_extension,	&v_doctor_id_type,		&v_doctor_id,
							&v_special_presc_num,	&v_special_presc_ext,	&ishur_source_send,
							&v_member_price_code,	&v_largo_code,			&v_op_in_dose,

							&v_units_in_dose,		&v_start_use_date,		&v_stop_use_date,
							&v_stop_use_date,		&v_authority_id,		&v_confirm_authority_,
							&v_insurance_used,		&v_insurance_used,		&v_confirm_reason,
							&v_dose_num,			&v_dose_renew_freq,		&v_priv_pharm_sale_ok,

							&v_confirmation_type,	&v_hospital_code,		&sysdate,
							&systime,				&v_const_sum_to_pay,	&v_in_health_basket,
							&v_HealthBasketNew,		&v_treatment_category,	&ShortZero,
							&ShortZero,				&ShortZero,				&ShortZero,

							&ShortZero,				&IntZero,				&ShortZero,
							&IntZero,				&ShortZero,				&ShortZero,
							&ROW_NOT_DELETED,		&IntZero,				&v_mess_code,
							&v_mess_text,			&v_country_center,		&v_reason_code,

							&v_made_date,			&v_doc_saw_ishur,		&v_portal_update_dt,
							&v_abrupt_close_flg,	&v_QtyLim_ingr,			&v_QtyLim_flg,
							&v_QtyLim_units,		&v_QtyLim_qty_each,		&v_QtyLim_per_day,
							&v_QtyLim_treat_days,	&v_QtyLim_course_len,	&v_QtyLim_courses,

							&v_QtyLim_all_ishur,	&v_QtyLim_all_warn,		&v_QtyLim_all_err,
							&v_QtyLim_per_course,	&v_QtyLim_course_wrn,	&v_QtyLim_course_err,
							&v_ql_all_ishur_std,	&v_ql_all_warn_std,		&v_ql_all_err_std,
							&v_ql_per_course_std,	&v_ql_course_wrn_std,	&v_ql_course_err_std,

							&v_form_number,			&v_tikra_flag,			&v_tikra_type_code,
							&v_ql_per_day_std,		&v_QtyLim_hist_start,	&needs_29_g,
							&member_diagnosis,		&monthly_qlm_flag,		&monthly_dosage,			// Marianna 06Aug2020 CR #27955/28605 added 6 columns.
							&monthly_dosage_std,	&monthly_duration,		&cont_approval_flag,
							
							&orig_confirm_num,		&exceptional_ishur,		END_OF_ARG_LIST				);	//Marianna 06Aug2020 CR #27955/28605 end. 19Jan2023 User Story 276372

				// DonR 30May2017: The integrity-constraint violations we saw yesterday should be fixed; but just
				// in case, add a diagnostic in case they happen again.
				if (SQLCODE == -391)
					GerrLogMini (GerrId, "Insert ishur: TZ %d Ishur %d Largo %d Message Text [%s] gave integrity constraint violation!",
								 v_member_id, v_special_presc_num, v_largo_code, v_mess_text);

				HANDLE_INSERT_SQL_CODE ("2003", p_connect, &do_insert, &retcode, &stop_retrying);

			}	// Do insert.

			else
			{
				ExecSQL (	MAIN_DB,
							AS400SRV_T2003_UPD_special_prescs,
							AS400SRV_T2003_UPD_special_prescs_with_TZ_code,
							&v_pharmacy_code,			&v_institued_code,			&v_doctor_id_type,
							&v_doctor_id,				&v_member_price_code,		&v_largo_code,
							&v_op_in_dose,				&v_units_in_dose,			&v_start_use_date,
							&v_stop_use_date,			&v_stop_use_date,			&v_authority_id,
							&v_confirm_authority_,		&v_insurance_used,			&v_insurance_used,

							&v_confirm_reason,			&v_dose_num,				&v_dose_renew_freq,
							&v_priv_pharm_sale_ok,		&v_confirmation_type,		&v_hospital_code,
							&v_const_sum_to_pay,		&sysdate,					&systime,
							&v_in_health_basket,		&v_HealthBasketNew,			&v_treatment_category,
							&v_mess_code,				&v_made_date,				&v_mess_text,

							&v_country_center,			&v_reason_code,				&v_doc_saw_ishur,
							&v_portal_update_dt,		&v_abrupt_close_flg,		&v_QtyLim_ingr,
							&v_QtyLim_flg,				&v_QtyLim_units,			&v_QtyLim_qty_each,
							&v_QtyLim_per_day,			&v_QtyLim_treat_days,		&v_QtyLim_course_len,
							&v_QtyLim_courses,			&v_QtyLim_all_ishur,		&v_QtyLim_all_warn,

							&v_QtyLim_all_err,			&v_QtyLim_per_course,		&v_QtyLim_course_wrn,
							&v_QtyLim_course_err,		&v_ql_all_ishur_std,		&v_ql_all_warn_std,
							&v_ql_all_err_std,			&v_ql_per_course_std,		&v_ql_course_wrn_std,
							&v_ql_course_err_std,		&v_ql_per_day_std,			&v_QtyLim_hist_start,
							&v_form_number,				&v_tikra_flag,				&v_tikra_type_code,

							&ishur_source_send,			&needs_29_g,				&member_diagnosis,
							&ROW_NOT_DELETED,			&monthly_qlm_flag,			&monthly_dosage,		// Marianna 06Aug2020 CR #27955/28605 added 6 columns.
							&monthly_dosage_std,		&monthly_duration,			&cont_approval_flag,
							&orig_confirm_num,			&exceptional_ishur,									// Marianna 06Aug2020 end (orig_confirm_num). 19Jan2023 User Story #276372

							&v_member_id,				&v_largo_code,				&v_special_presc_num,
							&v_mem_id_extension,		END_OF_ARG_LIST										);

				// DonR 29Apr2015: AS/400 sometimes sends updates with a new Member ID Code without first deleting the old
				// version. Accordingly, if the first UPDATE failed, try once more to update the ishur without reference
				// to the Member ID Code.
				if (SQLERR_code_cmp (SQLERR_no_rows_affected) == MAC_TRUE)
				{
				ExecSQL (	MAIN_DB,
							AS400SRV_T2003_UPD_special_prescs,
							AS400SRV_T2003_UPD_special_prescs_without_TZ_code,
							&v_pharmacy_code,			&v_institued_code,			&v_doctor_id_type,
							&v_doctor_id,				&v_member_price_code,		&v_largo_code,
							&v_op_in_dose,				&v_units_in_dose,			&v_start_use_date,
							&v_stop_use_date,			&v_stop_use_date,			&v_authority_id,
							&v_confirm_authority_,		&v_insurance_used,			&v_insurance_used,

							&v_confirm_reason,			&v_dose_num,				&v_dose_renew_freq,
							&v_priv_pharm_sale_ok,		&v_confirmation_type,		&v_hospital_code,
							&v_const_sum_to_pay,		&sysdate,					&systime,
							&v_in_health_basket,		&v_HealthBasketNew,			&v_treatment_category,
							&v_mess_code,				&v_made_date,				&v_mess_text,

							&v_country_center,			&v_reason_code,				&v_doc_saw_ishur,
							&v_portal_update_dt,		&v_abrupt_close_flg,		&v_QtyLim_ingr,
							&v_QtyLim_flg,				&v_QtyLim_units,			&v_QtyLim_qty_each,
							&v_QtyLim_per_day,			&v_QtyLim_treat_days,		&v_QtyLim_course_len,
							&v_QtyLim_courses,			&v_QtyLim_all_ishur,		&v_QtyLim_all_warn,

							&v_QtyLim_all_err,			&v_QtyLim_per_course,		&v_QtyLim_course_wrn,
							&v_QtyLim_course_err,		&v_ql_all_ishur_std,		&v_ql_all_warn_std,
							&v_ql_all_err_std,			&v_ql_per_course_std,		&v_ql_course_wrn_std,
							&v_ql_course_err_std,		&v_ql_per_day_std,			&v_QtyLim_hist_start,
							&v_form_number,				&v_tikra_flag,				&v_tikra_type_code,

							&ishur_source_send,			&needs_29_g,				&member_diagnosis,
							&ROW_NOT_DELETED,			&monthly_qlm_flag,			&monthly_dosage,		// Marianna 06Aug2020 CR #27955/28605 added 6 columns.
							&monthly_dosage_std,		&monthly_duration,			&cont_approval_flag,
							&orig_confirm_num,			&exceptional_ishur,									// Marianna 06Aug2020 end (orig_confirm_num), 19Jan2023 User Story #276372 (exceptional_ishur)

							&v_member_id,				&v_largo_code,				&v_special_presc_num,
							END_OF_ARG_LIST																	);
				}	// First UPDATE attempt found nothing to update.

				// DonR 30May2017: The integrity-constraint violations we saw yesterday should be fixed; but just
				// in case, add a diagnostic in case they happen again.
				if (SQLCODE == -391)
					GerrLogMini (GerrId, "Update ishur: TZ %d Ishur %d Largo %d Message Text [%s] gave integrity constraint violation!",
								 v_member_id, v_special_presc_num, v_largo_code, v_mess_text);

				HANDLE_UPDATE_SQL_CODE ("2003", p_connect, &do_insert, &retcode, &stop_retrying);
			}

		}/* for( i... */

	}/* for( n_retries... */

  if( n_retries == SQL_UPDATE_RETRIES )
  {
     WRITE_ACCESS_CONFLICT_TO_LOG( "2003" );
  }

  if (retcode == MSG_HANDLER_SUCCEEDED)
  {
	  if (do_insert == ATTEMPT_INSERT_FIRST)
		  rows_added++;
	  else
		  rows_updated++;
  }


  return retcode;
} /* end of msg 2003 handler */



/* << -------------------------------------------------------- >> */
/*                                                                */
/*   handler for message 2004 - delete rec from SPECIAL_PRESCS	  */
/*																  */
/* << -------------------------------------------------------- >> */
static int message_2004_handler(
				TConnection	*p_connect,
				int		data_len
				)
{

	/* << -------------------------------------------------------- >> */
	/*			  local variables			  */
	/* << -------------------------------------------------------- >> */
	char	*PosInBuff;		/* current pos in data buffer	    */
	int		ReadInput;		/* amount of Bytesread from buffer  */
	int		retcode;		/* code returned from function	    */
	int		n_retries;		/* retrying counter		    */
	int		i;
	int		db_changed_flg;


	/* << -------------------------------------------------------- >> */
	/*			    DB variables			  */
	/* << -------------------------------------------------------- >> */
	Tmember_id			v_member_id;
	Tmem_id_extension	v_mem_id_extension;
	Tspecial_presc_num	v_special_presc_num;
	Tspecial_presc_ext	v_special_presc_ext;
	Tlargo_code			v_largo_code;



/* << -------------------------------------------------------- >> */
/*			  read input buffer			  */
/* << -------------------------------------------------------- >> */
	INIT_ERROR_DETECTION_MECHANISM;
	PosInBuff = glbMsgBuffer;

	v_member_id			= get_long	(&PosInBuff, Tmember_id_len			); RETURN_ON_ERROR;
	v_mem_id_extension	= get_short	(&PosInBuff, Tmem_id_extension_len	); RETURN_ON_ERROR;
	v_special_presc_num	= get_long	(&PosInBuff, Tspecial_presc_num_len	); RETURN_ON_ERROR;
	v_special_presc_ext	= get_short	(&PosInBuff, Tspecial_presc_ext_len	); RETURN_ON_ERROR;
	v_largo_code		= get_long	(&PosInBuff, 9						); RETURN_ON_ERROR;	//Marianna 14Jul2024 User Story #330877


/* << -------------------------------------------------------- >> */
/*			  debug messages			  */
/* << -------------------------------------------------------- >> */
  /*************************  COMMENTED OUT: ****************************
  printf( "Tmember_id == %d\n", 	v_member_id );
  printf( "Tmem_id_extension == %d\n", 	v_mem_id_extension );
  printf( "Tspecial_presc_num == %d\n",v_special_presc_num );
  printf( "Tspecial_presc_ext == %dn", 	v_special_presc_ext );
  fflush( stdout );
  **********************************************************************/


/* << -------------------------------------------------------- >> */
/*		      verify all deta was read			  */
/* << -------------------------------------------------------- >> */
  ReadInput = PosInBuff - glbMsgBuffer;
  StaticAssert( ReadInput == data_len );


/* << -------------------------------------------------------- >> */
/*			   attempt delete			  */
/* << -------------------------------------------------------- >> */
  retcode = ALL_RETRIES_WERE_USED;
  db_changed_flg = 0;

  for( n_retries = 0;
       n_retries < SQL_UPDATE_RETRIES;
       n_retries++ )
    {

      /*
       * sleep for a while:
       */
      if( n_retries )
      {
          sleep( SLEEP_TIME_BETWEEN_RETRIES );
      }

	// DonR 26April2015: Ishur Source is no longer part of the unique key for special_prescs.
	// This change was made in May 2012 for ordinary updates; I have no idea why I didn't make
	// it for deletions at the same time.
	ExecSQL (	MAIN_DB,
				AS400SRV_T2004_DEL_special_prescs,
				AS400SRV_T2003_UPD_special_prescs_with_TZ_code,
				&v_member_id,			&v_largo_code,		&v_special_presc_num,
				&v_mem_id_extension,	END_OF_ARG_LIST								);

	// DonR 29Apr2015: AS/400 sometimes sends updates with a new Member ID Code without first deleting the old
	// version. Accordingly, if the first UPDATE failed, try once more to update the ishur without reference
	// to the Member ID Code.
	if (SQLERR_code_cmp (SQLERR_no_rows_affected) == MAC_TRUE)
	{
		ExecSQL (	MAIN_DB,
					AS400SRV_T2004_DEL_special_prescs,
					AS400SRV_T2003_UPD_special_prescs_without_TZ_code,
					&v_member_id,			&v_largo_code,		&v_special_presc_num,
					END_OF_ARG_LIST														);
	}	// First DELETE attempt found nothing to update.

      HANDLE_DELETE_SQL_CODE( "2004", p_connect, &retcode, &db_changed_flg );

      break;
    }

  if( n_retries == SQL_UPDATE_RETRIES )
  {
     WRITE_ACCESS_CONFLICT_TO_LOG( "2004" );
  }

  if (retcode == MSG_HANDLER_SUCCEEDED)
	  rows_deleted++;


  return retcode;
} /* end of msg 2004 handler */



// DonR 06Apr2008: The transaction 2005 handler is no longer in use. Even when 
// Trn. 2005 is sent, the new handler data_4005_handler() can take care of
// drug_list updates and insertions. The advantage of the new routine is that
// it works incrementally, differentiating between changes relevant to pharmacies
// only and those changes that apply to doctors as well as to pharmacies. The new
// version also works in "real time", avoiding the problems created by the "virtual
// timestamp" system.



// << -------------------------------------------------------- >>
//
//		Handler for message 4005 - upd/ins to DRUG_LIST
//
//		This version is designed for full-table updates,and
//		tests each field to determine if an update is really needed.
//
//		In fact, it will be used for intra-day updates as well.
//
// << -------------------------------------------------------- >>
static int data_4005_handler (TConnection	*p_connect,
							  int			data_len)
{
	// local variables
	int		ReadInput;			/* amount of Bytesread from buffer  */
	int		retcode;			/* code returned from function	    */
	int		n_retries;			/* retrying counter				    */
	int		i;
	int		RowsFound;
	bool	stop_retrying;		/* if true - return from function   */
	bool	need_to_read;
	bool	something_changed = 0;
	short	do_insert = ATTEMPT_UPDATE_FIRST;
	short	AS400_open_pkg_forbidden;
//	char	diffs[51];


	// DB variables
	int						v_largo_code;
	Tpriv_pharm_sale_ok		v_priv_pharm_sale_ok;
	char					v_largo_type	[ 1 + 1];
	char					v_long_name		[30 + 1];
	char					v_package		[10 + 1];
	int						v_package_size;
	Tdrug_book_flg			v_drug_book_flg;
	Tprice_code				v_basic_price_code;
	Tprice_code				v_prev_bas_prc_cod;	// DonR 05Jan2012: Not really in use.
	Tt_half					v_t_half;
	short					v_chronic_months;
	Tsupplemental_1			v_supplemental_1;
	Tsupplemental_2			v_supplemental_2;
	Tsupplemental_3			v_supplemental_3;
	Tsupplier_code			v_supplier_code;
	Tcomputersoft_code		v_computersoft_code;
	Tadditional_price		v_additional_price;
	Tno_presc_sale_flag		v_no_presc_sale_flag;
	Tin_health_pack			v_in_health_basket;
	short					v_del_flg;
	Tdrug_book_flg			v_drug_book_doct_flg;
	short					v_pharm_group_code;
	char					v_short_name	[17 + 1];
	char					v_form_name		[25 + 1];
	Tprice_prcnt			v_price_prcnt_b;
	Tprice_prcnt			v_price_prcnt_k;
	Tprice_prcnt			v_price_prcnt_m;
	char					v_drug_type		[ 1 + 1];
	short					is_narcotic;
	short					psychotherapy_drug;
	short					preparation_type;
	int						chronic_days;
	Tpharm_sale_new			v_pharm_sale_new;
	char					v_sale_price	[ 5 + 1];				
	char					v_sale_price_strudel	[ 5 + 1];				
	char					v_interact_flg	[ 1 + 1];
	Tstatus_send			v_status_send;
	int						v_magen_wait_months;
	int						v_keren_wait_months;
	char					v_del_flg_cont	[ 1 + 1];
	Topenable_package		v_openable_pkg;
	Tishur_required			v_ishur_required;
	short					v_pharm_real_time;	// Whether to send drug to pharmacies
												// in "real time" or not. (DonR 14Feb2005)
	short					v_maccabicare_flag;
	short					v_IshurQtyLimFlg;
	short					tight_ishur_limits;
	short					v_IshurQtyLimIngr;
	int						v_Economypri_Group;
	short					v_Economypri_Seq;
	double					v_package_volume;
	short					v_ingr_1_code;
	short					v_ingr_2_code;
	short					v_ingr_3_code;
	double					v_ingr_1_quant;
	double					v_ingr_2_quant;
	double					v_ingr_3_quant;
	double					v_ingr_1_quant_std;
	double					v_ingr_2_quant_std;
	double					v_ingr_3_quant_std;
	char					v_ingr_1_units		[3 + 1];
	char					v_ingr_2_units		[3 + 1];
	char					v_ingr_3_units		[3 + 1];
	double					v_per_1_quant;
	double					v_per_2_quant;
	double					v_per_3_quant;
	char					v_per_1_units		[3 + 1];
	char					v_per_2_units		[3 + 1];
	char					v_per_3_units		[3 + 1];

	// DonR 14Jan2026 User Story #450353 (Pefcent Spray enhancement): Add new
	// columns for alternate package size - in this case, per-bottle values.
	// The first set is the new stuff we get from AS/400 RK9001P, sent exactly
	// as it's written in the AS/400 table.
	bool					two_pkg_size_levels;
	double					sec_ingr_1_quant;
	double					sec_ingr_2_quant;
	double					sec_ingr_3_quant;
	char					sec_ingr_1_units	[3 + 1];
	char					sec_ingr_2_units	[3 + 1];
	char					sec_ingr_3_units	[3 + 1];
	double					sec_per_1_quant;
	double					sec_per_2_quant;
	double					sec_per_3_quant;
	char					sec_per_1_units		[3 + 1];
	char					sec_per_2_units		[3 + 1];
	char					sec_per_3_units		[3 + 1];
	double					sec_pkg_volume;

	// The second set is what we actually write to drug_list. For almost
	// all drugs (two_pkg_size_levels == 0) we'll write zero/empty values;
	// but for Pefcent Spray 100 mcg./dose x 8 x 4, these will get the
	// "original" values: 4 bottles/OP, 1550 mcg./bottle, and so on.
	double					orig_ingr_1_quant;
	double					orig_ingr_2_quant;
	double					orig_ingr_3_quant;
	char					orig_ingr_1_units	[3 + 1];
	char					orig_ingr_2_units	[3 + 1];
	char					orig_ingr_3_units	[3 + 1];
	double					orig_per_1_quant;
	double					orig_per_2_quant;
	double					orig_per_3_quant;
	char					orig_per_1_units	[3 + 1];
	char					orig_per_2_units	[3 + 1];
	char					orig_per_3_units	[3 + 1];
	double					orig_pkg_volume;
	int						orig_pkg_size;
	// DonR 14Jan2026 User Story #450353 (Pefcent Spray enhancement) end.

	char					v_pharm_sale_test	[1 + 1];
	short					v_rule_status;
	short					v_sensitivity_flag;
	int						v_sensitivity_code;
	short					v_sensitivity_severe;
	short					v_needs_29_g;
	short					v_chronic_flag;
	short					v_shape_code;
	short					v_shape_code_new;
	short					v_split_pill_flag;
	short					v_treatment_typ_cod;
	short					v_print_generic_flg;
	int						v_substitute_drug;
	short					v_copies_to_print;
	TGenericFlag			v_preferred_flg;
	int						v_parent_group_code;
	char					v_parent_group_name	[25 + 1];
	char					v_name_25			[25 + 1];
	int						v_fps_group_code;
	short					v_assuta_drug_status;
	short					v_expiry_date_flag;
	char					v_bar_code_value	[14 + 1];
	short					v_tikrat_mazon;
	short					v_class_code;
	short					v_in_overdose_tbl;
	short					v_enabled_status;
	short					v_enabled_mac;
	short					v_enabled_hova;
	short					v_enabled_keva;
	int						v_percnt_y;
	short					v_wait_yahalom;
	short					v_future_sales_ok;
	short					qualify_future_sales;
	short					maccabi_profit_rating;
	short					cont_treatment;
	short					time_of_day_taken;
	short					treatment_days;
	short					how_to_take_code;
	char					unit_abbreviation	[ 3 + 1];
	short					for_cystic_fibro;
	short					for_tuberculosis;
	short					for_gaucher;
	short					for_aids;
	short					for_dialysis;
	short					for_thalassemia;
	short					for_hemophilia;
	short					for_cancer;
	short					for_car_accident;
	short					for_work_accident;
	short					for_reserved_1;
//	short					for_reserved_2;
	short					for_reserved_3;
	short					for_reserved_99;
	int						illness_bitmap;

	short					o_enabled_status;
	Tpriv_pharm_sale_ok		o_priv_pharm_sale_ok;
	char					o_largo_type	[ 1 + 1];
	char					o_long_name		[30 + 1];
	char					o_package		[10 + 1];
	int						o_package_size;
	Tdrug_book_flg			o_drug_book_flg;
	Tprice_code				o_basic_price_code;
	Tt_half					o_t_half;
	short					o_chronic_months;
	Tsupplemental_1			o_supplemental_1;
	Tsupplemental_2			o_supplemental_2;
	Tsupplemental_3			o_supplemental_3;
	Tsupplier_code			o_supplier_code;
	Tcomputersoft_code		o_computersoft_code;
	Tadditional_price		o_additional_price;
	Tno_presc_sale_flag		o_no_presc_sale_flag;
	Tin_health_pack			o_in_health_basket;
	Tin_health_pack			o_HealthBasketNew;
	short					o_del_flg;
	Tdrug_book_flg			o_drug_book_doct_flg;
	Tpharm_group_code		o_pharm_group_code;
	char					o_short_name	[17 + 1];
	char					o_form_name		[25 + 1];
	Tprice_prcnt			o_price_prcnt_b;
	Tprice_prcnt			o_price_prcnt_k;
	Tprice_prcnt			o_price_prcnt_m;
	char					o_drug_type		[ 1 + 1];
	short					o_is_narcotic;
	short					o_psychotherapy_drug;
	short					o_preparation_type;
	int						o_chronic_days;
	Tpharm_sale_new			o_pharm_sale_new;
	char					o_sale_price	[ 5 + 1];				
	char					o_sale_price_strudel	[ 5 + 1];
	char					o_interact_flg	[ 1 + 1];
	Tstatus_send			o_status_send;
	short					o_magen_wait_months;
	short					o_keren_wait_months;
	char					o_del_flg_cont	[ 1 + 1];
	Topenable_package		o_openable_pkg;
	Tishur_required			o_ishur_required;
	short					o_maccabicare_flag;
	short					o_IshurQtyLimFlg;
	short					o_tight_limits;
	short					o_IshurQtyLimIngr;
	int						o_Economypri_Group;
	short					o_Economypri_Seq;
	double					o_package_volume;
	short					o_ingr_1_code;
	short					o_ingr_2_code;
	short					o_ingr_3_code;
	double					o_ingr_1_quant;
	double					o_ingr_2_quant;
	double					o_ingr_3_quant;
	char					o_ingr_1_units		[3 + 1];
	char					o_ingr_2_units		[3 + 1];
	char					o_ingr_3_units		[3 + 1];
	double					o_per_1_quant;
	double					o_per_2_quant;
	double					o_per_3_quant;
	char					o_per_1_units		[3 + 1];
	char					o_per_2_units		[3 + 1];
	char					o_per_3_units		[3 + 1];
	char					o_pharm_sale_test	[1 + 1];
	short					o_rule_status;
	short					o_sensitivity_flag;
	int						o_sensitivity_code;
	short					o_sensitivity_severe;
	short					o_needs_29_g;
	short					o_chronic_flag;
	short					o_shape_code;
	short					o_shape_code_new;
	short					o_split_pill_flag;
	short					o_treatment_typ_cod;
	short					o_print_generic_flg;
	int						o_substitute_drug;
	short					o_copies_to_print;
	TGenericFlag			o_preferred_flg;
	int						o_parent_group_code;
	char					o_parent_group_name	[25 + 1];
	char					o_name_25			[25 + 1];
	int						o_fps_group_code;
	short					o_assuta_drug_status;
	short					o_expiry_date_flag;
	char					o_bar_code_value	[14 + 1];
	short					o_tikrat_mazon;
	short					o_class_code;
	int						o_percnt_y;
	short					o_wait_yahalom;
	short					o_future_sales_ok;
	short					o_qualify_future_sales;
	short					o_maccabi_profit_rating;
	short					o_cont_treatment;
	short					o_time_of_day;
	short					o_treatment_days;
	short					o_how_to_take;
	char					o_unit_abbrev	[ 3 + 1];
	int						o_illness_bitmap;
	bool					o_two_pkg_size_levels;
	double					o_orig_ingr_1_quant;
	double					o_orig_ingr_2_quant;
	double					o_orig_ingr_3_quant;
	char					o_orig_ingr_1_units	[3 + 1];
	char					o_orig_ingr_2_units	[3 + 1];
	char					o_orig_ingr_3_units	[3 + 1];
	double					o_orig_per_1_quant;
	double					o_orig_per_2_quant;
	double					o_orig_per_3_quant;
	char					o_orig_per_1_units	[3 + 1];
	char					o_orig_per_2_units	[3 + 1];
	char					o_orig_per_3_units	[3 + 1];
	double					o_orig_pkg_volume;
	int						o_orig_pkg_size;

	// DonR 22Aug2021 User Story #181694: Add 8 new short-integer fields.
	short					needs_refrigeration;
	short					needs_patient_instruction;
	short					needs_dilution;
	short					needs_preparation;
	short					home_delivery_ok;
	short					online_order_pickup_ok;
	short					is_orthopedic_device;
	short					needs_patient_measurement;
	short					ConsignmentItem;		// Marianna 20Apr2023 User Story #432608
	short					TikratPiryonPharmType;	// DonR 12Feb2025 User Story #376480

	int						v_refresh_date;
	int						v_refresh_time;
	int						v_changed_date;
	int						v_changed_time;
	int						v_pharm_update_dt;
	int						v_pharm_update_tm;
	int						v_doc_update_date;
	int						v_doc_update_time;
	int						v_as400_batch_date;
	int						v_as400_batch_time;
	int						v_update_date;
	int						v_update_time;
	int						v_update_date_d;
	int						v_update_time_d;

	char					v_diffs[51];
	char					o_diffs[51];
	


	// Calculate virtual time. Note that for this download the time is based
	// on the real time, adjusted only to ensure that we don't have too many
	// drugs with the exact same as-of time.
	v_refresh_date		=  (download_start_date > 0) ? download_start_date : GetDate();
	v_refresh_time		=  (download_start_time > 0) ? download_start_time : GetTime();
	v_as400_batch_date	=  v_refresh_date;
	v_as400_batch_time	=  v_refresh_time;
	v_refresh_time		+= (download_row_count++ / VIRTUAL_PORTION);

	// DonR 23Sep2020: Eliminate the "diffs" logic, since it's been years since I actually used
	// it to check on anything. After the switchover to MS-SQL, we can drop the "diffs" column
	// from drug_list and all its SQL operations.
	strcpy (v_diffs, "");

	// read input buffer
	INIT_ERROR_DETECTION_MECHANISM;
	PosInBuff = glbMsgBuffer;

	GET_LONG	(v_largo_code				, 9							);	//Marianna 14Jul2024 User Story #330877
	GET_SHORT	(v_priv_pharm_sale_ok		, Tpriv_pharm_sale_ok_len	);
	GET_STRING	(v_largo_type				, Tlargo_type_len			);
	GET_HEB_STR (v_long_name				, Tlong_name_len			);
	GET_HEB_STR (v_package					, Tpackage_len				);
	GET_LONG	(v_package_size				, Tpackage_size_len			);
	GET_SHORT	(v_drug_book_flg			, Tdrug_book_flg_len		);
	GET_SHORT	(v_basic_price_code			, Tprice_code_len			);
	GET_SHORT	(v_prev_bas_prc_cod			, Tprice_code_len			);	// DonR 05Jan2012: Not really in use.
	GET_SHORT	(v_t_half					, Tt_half_len				);
	GET_SHORT	(v_chronic_months			, 2							);
	GET_LONG	(v_supplemental_1			, Tsupplemental_1_len		);
	GET_LONG	(v_supplemental_2			, Tsupplemental_2_len		);
	GET_LONG	(v_supplemental_3			, Tsupplemental_3_len		);
	GET_LONG	(v_supplier_code			, Tsupplier_code_len		);
	GET_LONG	(v_computersoft_code		, Tcomputersoft_code_len	);
	GET_SHORT	(v_additional_price			, Tadditional_price_len		);
	GET_SHORT	(v_no_presc_sale_flag		, Tno_presc_sale_flag_len	);
	GET_SHORT	(v_del_flg					, Tdel_flg_len				);
	GET_SHORT	(v_in_health_basket			, Tin_health_pack_len		);
	GET_SHORT	(v_pharm_group_code			, Tpharm_group_code_len		);
	GET_HEB_STR (v_short_name				, Tshort_name_len			);
	GET_STRING	(v_form_name				, Tform_name_len			);
	GET_LONG	(v_price_prcnt_b			, Tprice_prcnt_len			);
	GET_LONG	(v_price_prcnt_m			, Tprice_prcnt_len			);
	GET_LONG	(v_price_prcnt_k			, Tprice_prcnt_len			);
	GET_STRING	(v_drug_type				, Tdrug_type_len			);
	GET_SHORT	(v_pharm_sale_new			, Tpharm_sale_new_len		);
	GET_SHORT	(v_status_send				, Tstatus_send_len			);
	GET_STRING	(v_sale_price				, Tsale_price_len			);
	GET_STRING	(v_interact_flg				, Tinteract_flg_len			);
	GET_SHORT	(v_magen_wait_months		, Twait_months_len			);
	GET_SHORT	(v_keren_wait_months		, Twait_months_len			);
	GET_SHORT	(v_drug_book_doct_flg		, Tdrug_book_flg_len		);
	GET_SHORT	(AS400_open_pkg_forbidden	, Topenable_package_len		);
	GET_SHORT	(v_ishur_required			, Tishur_required_len		);
	GET_SHORT	(v_ingr_1_code				, 4							);
	GET_SHORT	(v_ingr_2_code				, 4							);
	GET_SHORT	(v_ingr_3_code				, 4							);
	GET_DOUBLE	(v_ingr_1_quant				, 20						);
	GET_DOUBLE	(v_ingr_2_quant				, 20						);
	GET_DOUBLE	(v_ingr_3_quant				, 20						);
	GET_STRING	(v_ingr_1_units				, 3							);
	GET_STRING	(v_ingr_2_units				, 3							);
	GET_STRING	(v_ingr_3_units				, 3							);
	GET_DOUBLE	(v_per_1_quant				, 20						);
	GET_DOUBLE	(v_per_2_quant				, 20						);
	GET_DOUBLE	(v_per_3_quant				, 20						);
	GET_STRING	(v_per_1_units				, 3							);
	GET_STRING	(v_per_2_units				, 3							);
	GET_STRING	(v_per_3_units				, 3							);
	GET_STRING	(v_pharm_sale_test			, 1							);
	GET_SHORT	(v_rule_status				, 1							);
	GET_SHORT	(v_sensitivity_flag			, 1							);
	GET_LONG	(v_sensitivity_code			, 5							);
	GET_SHORT	(v_sensitivity_severe		, 1							);
	GET_SHORT	(v_needs_29_g				, 1							);
	GET_SHORT	(v_chronic_flag				, 1							);
	GET_SHORT	(v_shape_code				, 3							);
	GET_SHORT	(v_shape_code_new			, 3							);
	GET_SHORT	(v_split_pill_flag			, 1							);
	GET_SHORT	(v_treatment_typ_cod		, 2							);
	GET_SHORT	(v_print_generic_flg		, 1							);
	GET_LONG	(v_substitute_drug			, 9							);	// DonR 14Oct2024 User Story #359136: Change from length 5 to 9.
	GET_DOUBLE	(v_package_volume			, 20						);
	GET_SHORT	(v_copies_to_print			, 2							);
	GET_STRING	(v_sale_price_strudel		, Tsale_price_len			);
	GET_SHORT	(v_preferred_flg			, TGenericFlag_len			);
	GET_LONG	(v_parent_group_code		, 5							);
	GET_STRING	(v_parent_group_name		, 25						);
	GET_SHORT	(v_pharm_real_time			, TGenericFlag_len			);
	GET_SHORT	(v_maccabicare_flag			, TGenericFlag_len			);
	GET_SHORT	(v_IshurQtyLimFlg			, TGenericFlag_len			);
	GET_SHORT	(v_IshurQtyLimIngr			, 4							);
	GET_HEB_STR (v_name_25					, Tname_25_len				);
	GET_LONG	(v_fps_group_code			, 5							);
	GET_SHORT	(v_assuta_drug_status		, 1							);
	GET_SHORT	(v_expiry_date_flag			, 1							);
	GET_STRING	(v_bar_code_value			, 13						);
	GET_SHORT	(v_tikrat_mazon				, 1							);
	GET_SHORT	(v_class_code				, 5							);
	GET_SHORT	(v_enabled_status			, 2							);
	GET_LONG	(v_percnt_y					, Tprice_prcnt_len			);
	GET_SHORT	(v_wait_yahalom				, Twait_months_len			);
	GET_SHORT	(v_future_sales_ok			, 2							);
	GET_SHORT	(cont_treatment				, 1							);
	GET_SHORT	(time_of_day_taken			, 1							);
	GET_SHORT	(treatment_days				, 3							);
	GET_SHORT	(for_cystic_fibro			, 2							);
	GET_SHORT	(for_tuberculosis			, 2							);
	GET_SHORT	(for_gaucher				, 2							);
	GET_SHORT	(for_aids					, 2							);
	GET_SHORT	(for_dialysis				, 2							);
	GET_SHORT	(for_thalassemia			, 2							);
	GET_SHORT	(for_hemophilia				, 2							);
	GET_SHORT	(for_cancer					, 2							);
	GET_SHORT	(for_car_accident			, 2							);
	GET_SHORT	(for_reserved_1				, 2							);
	GET_SHORT	(for_work_accident			, 2							);
	GET_SHORT	(for_reserved_3				, 2							);
	GET_SHORT	(for_reserved_99			, 2							);
	GET_SHORT	(how_to_take_code			, 3							);
	GET_STRING	(unit_abbreviation			, 3							);
	GET_SHORT	(tight_ishur_limits			, 1							);
	GET_SHORT	(qualify_future_sales		, 1							); // Marianna 14Sep2020 CR #30106  	
	GET_SHORT	(maccabi_profit_rating		, 1							); // DonR 27Apr2021 User Story #149963
	GET_SHORT	(needs_refrigeration		, 1							); // DonR 22Aug2021 User Story #181694
	GET_SHORT	(needs_patient_instruction	, 1							); // DonR 22Aug2021 User Story #181694
	GET_SHORT	(needs_dilution				, 1							); // DonR 22Aug2021 User Story #181694
	GET_SHORT	(needs_preparation			, 1							); // DonR 22Aug2021 User Story #181694
	GET_SHORT	(home_delivery_ok			, 1							); // DonR 22Aug2021 User Story #181694
	GET_SHORT	(online_order_pickup_ok		, 1							); // DonR 22Aug2021 User Story #181694
	GET_SHORT	(is_orthopedic_device		, 1							); // DonR 22Aug2021 User Story #181694
	GET_SHORT	(needs_patient_measurement	, 1							); // DonR 22Aug2021 User Story #181694
	GET_SHORT	(ConsignmentItem			, 1							); // Marianna 20Apr2023 User Story #432608
	GET_SHORT	(TikratPiryonPharmType		, 1							); // DonR 12Feb2025 User Story #376480
	GET_SHORT	(is_narcotic				, 1							); // DonR 22Jul2025 User Story #427521
	GET_SHORT	(psychotherapy_drug			, 1							); // DonR 22Jul2025 User Story #427521
	GET_SHORT	(preparation_type			, 1							); // DonR 22Jul2025 User Story #417800
	GET_LONG	(chronic_days				, 6							); // DonR 06Aug2025 User Story #427521

	GET_SHORT	(two_pkg_size_levels		, 1							); // DonR 14Jan2026 User Story #450353
	GET_DOUBLE	(sec_ingr_1_quant			, 20						); // DonR 14Jan2026 User Story #450353
	GET_DOUBLE	(sec_ingr_2_quant			, 20						); // DonR 14Jan2026 User Story #450353
	GET_DOUBLE	(sec_ingr_3_quant			, 20						); // DonR 14Jan2026 User Story #450353
	GET_STRING	(sec_ingr_1_units			, 3							); // DonR 14Jan2026 User Story #450353
	GET_STRING	(sec_ingr_2_units			, 3							); // DonR 14Jan2026 User Story #450353
	GET_STRING	(sec_ingr_3_units			, 3							); // DonR 14Jan2026 User Story #450353
	GET_DOUBLE	(sec_per_1_quant			, 20						); // DonR 14Jan2026 User Story #450353
	GET_DOUBLE	(sec_per_2_quant			, 20						); // DonR 14Jan2026 User Story #450353
	GET_DOUBLE	(sec_per_3_quant			, 20						); // DonR 14Jan2026 User Story #450353
	GET_STRING	(sec_per_1_units			, 3							); // DonR 14Jan2026 User Story #450353
	GET_STRING	(sec_per_2_units			, 3							); // DonR 14Jan2026 User Story #450353
	GET_STRING	(sec_per_3_units			, 3							); // DonR 14Jan2026 User Story #450353
	GET_DOUBLE	(sec_pkg_volume				, 20						); // DonR 14Jan2026 User Story #450353


	// DonR 05Feb2020: ODBC appears to be automatically stripping trailing spaces from varchars
	// it reads from the database - at least for Informix. This causes the comparison to old
	// values to think that data has been changed, when in fact the only difference is in the
	// trailing spaces. The easiest solution is to strip trailing spaces from all the_xxx fields
	// we're storing as varchars.
	// Note also that we get 13 characters of Barcode from AS/400, but the DB field is defined
	// as char(14) - so we always see a mismatch. The solution is to strip trailing spaces
	// from the AS/400 value *and* from the old value we read from the database.
	strip_trailing_spaces (v_long_name);
	strip_trailing_spaces (v_short_name);
	strip_trailing_spaces (v_name_25);
	strip_trailing_spaces (v_form_name);
	strip_trailing_spaces (v_bar_code_value);

	// The AS400 database sets field BMYLRT to 1 if the package is NOT openable, or 0 if it is.
	// This is the opposite of the way it was set up on the Unix system. Rather than embark on
	// a whole project to change the system around - or worse, to have a database field called
	// "openable_pkg" with a TRUE value indicating that the package CAN'T be opened (horrors!) - 
	// we'll just do a little switcheroo. So a zero from the AS400 will become a one, and a one
	// will become a zero. Any other strange number will be left as-is, so we can be alerted that
	// the AS400 is on drugs.
	if (AS400_open_pkg_forbidden == 0)
	{
		v_openable_pkg = 1;
	}
	else
	{
		if (AS400_open_pkg_forbidden == 1)
		{
			v_openable_pkg = 0;
		}
		else
		{
			v_openable_pkg = AS400_open_pkg_forbidden;	// Unrecognized value - use what was sent.
		}
	}

	// DonR 09Oct2011: Translate v_enabled_status into individual enabled-flag variables.
	SET_TZAHAL_FLAGS;

	// DonR 20Jun2013: Set up "standardized" ingredient-quantity fields. These will give
	// ingredient usage in mg when the original unit is mcg, mg, or g; IU when the
	// original unit is IU or MIU; and will be the same as the original quantity for any
	// other unit, such as mL.
	v_ingr_1_quant_std = ConvertUnitAmount (v_ingr_1_quant, v_ingr_1_units);
	v_ingr_2_quant_std = ConvertUnitAmount (v_ingr_2_quant, v_ingr_2_units);
	v_ingr_3_quant_std = ConvertUnitAmount (v_ingr_3_quant, v_ingr_3_units);


	// DonR 19May2014: Set up illness_bitmap according to the set of serious-illness
	// flags sent from AS/400. Note that at least for now, we are going to trust
	// the AS/400 to send us meaningful numbers rather than just yes/no flags;
	// thus we will use the illness numbers to decide which bit to set for each
	// applicable illness, rather than hard-coding the illness list.
//	// Note that "for_reserved_2" gets a value of 99 rather than 10; translate 99
//	// to 10 so that we don't even theoretically blow up the bitmap.
//	if (for_reserved_2 > 10)
//		for_reserved_2 = 10;

	illness_bitmap = 0;

	// DonR 07Jan2015: Just in case AS/400 passes incorrect values for illness codes,
	// switch from addition to bitwise-OR-ing. This fix applies more to the members
	// table than to drug_list, but it can't hurt.
	// DonR 10Sep2015: Add for_car_accident, plus two more reserved illness codes.
	// For accident victims and reserved codes, force in specific numbers (if non-zero)
	// to build our bitmap properly. (We already know that we get "proper" values for
	// illnesses 1 through 8.)
	// DonR 02Mar2016: Work Accidents will get an AS/400 Illness Code of 97, which will
	// be translated into bitmap position 28. I need to check which field this value
	// will be put into.
	if (for_reserved_1)		for_reserved_1		=  9;
//	if (for_reserved_2)		for_reserved_2		= 10;
	if (for_reserved_3)		for_reserved_3		= 11;
	if (for_work_accident)	for_work_accident	= 28;
	if (for_car_accident)	for_car_accident	= 29;
	if (for_reserved_99)	for_reserved_99		= 30;

	if (for_cystic_fibro	> 0)	illness_bitmap |= (1 << (for_cystic_fibro	- 1));
	if (for_tuberculosis	> 0)	illness_bitmap |= (1 << (for_tuberculosis	- 1));
	if (for_gaucher			> 0)	illness_bitmap |= (1 << (for_gaucher		- 1));
	if (for_aids			> 0)	illness_bitmap |= (1 << (for_aids			- 1));
	if (for_dialysis		> 0)	illness_bitmap |= (1 << (for_dialysis		- 1));
	if (for_thalassemia		> 0)	illness_bitmap |= (1 << (for_thalassemia	- 1));
	if (for_hemophilia		> 0)	illness_bitmap |= (1 << (for_hemophilia		- 1));
	if (for_cancer			> 0)	illness_bitmap |= (1 << (for_cancer			- 1));
	if (for_reserved_1		> 0)	illness_bitmap |= (1 << (for_reserved_1		- 1));
//	if (for_reserved_2		> 0)	illness_bitmap |= (1 << (for_reserved_2		- 1));
	if (for_reserved_3		> 0)	illness_bitmap |= (1 << (for_reserved_3		- 1));
	if (for_reserved_99		> 0)	illness_bitmap |= (1 << (for_reserved_99	- 1));
	if (for_work_accident	> 0)	illness_bitmap |= (1 << (for_work_accident	- 1));
	if (for_car_accident	> 0)	illness_bitmap |= (1 << (for_car_accident	- 1));


	// DonR 06Apr2008:
	// NOTE: In this version, we don't use v_pharm_real_time to control how data is
	// timestamped, because we no longer use virtual timestamps. Instead, we will use
	// v_pharm_real_time as an "update forcer": if it's set non-zero, we will ignore
	// the field-by-field comparison results and assume that the affected drug needs
	// to be refreshed at doctors and pharmacies even if no fields have changed.
	
	// verify all data was read
	ReadInput = PosInBuff - glbMsgBuffer;
	StaticAssert( ReadInput == data_len );

	if (v_del_flg == 0)
	{
		strcpy (v_del_flg_cont, " ");
	}
	else
	{
		strcpy (v_del_flg_cont, "D");
	}

	// DonR 26Oct2008: Set "old" health-basket flag to 1 or 0 based on the value received
	// from AS400, and move the actual value received to the "new" health-basket flag
	// variable.
	PROCESS_HEALTH_BASKET;

	// DonR 19Jun2013: Pharmasoft Code 99999 (stored in "computersoft_code") is now used as a "null value"
	// for this field - but pharmacies need to see this as a zero.
	if (v_computersoft_code == 99999)
		v_computersoft_code = 0;

	// DonR 13Nov2017 TEMPORARY HACK: For the moment, zero out Computersoft Codes greater than 99999,
	// since pharmacies (and Transaction 1011 in Production) are not yet ready for them.
	// DonR 14Oct2024: See, I said it was temporary! As part of the switch to 6-digit Largo Codes,
	// we're now fully enabling 6-digit ComputerSoft (PharmaSoft) codes as well. So I'll change the
	// maximum value from 99999 to 999999.
	if (v_computersoft_code > 999999)
		v_computersoft_code = 0;


	// DonR 30Jun2013: AS/400 sends a value of 1 to indicate that future drug sales are not permitted
	// regardless of pharmacy type. Since the new database field is called "allow_future_sales",
	// translate this value into a zero. The permitted values are thus 0 = only current sales allowed,
	// 2 = future sales at Maccabi pharmacies only, and 3 = future sales allowed at all pharmacies.
	if (v_future_sales_ok == 1)
		v_future_sales_ok = 0;


	// DonR 14Jan2026 User Story #450353: Process new "secondary" package data. If
	// two_pkg_size_levels is TRUE, we want to swap these values in place of the
	// "normal" values, with package size/volume adjusted as necessary. Otherwise -
	// for almost everything in the table - we'll write the "secondary" data to the
	// new "original" columns, but they have no real significance and should be all
	// zero/blank.
	if (two_pkg_size_levels)	// As of now, TRUE only for Largo 77041 (Pefcent Spray 4 8-dose bottles).
	{
		// First, copy default package information to "original" fields.
		orig_ingr_1_quant	=		v_ingr_1_quant;
		orig_ingr_2_quant	=		v_ingr_2_quant;
		orig_ingr_3_quant	=		v_ingr_3_quant;
		orig_per_1_quant	=		v_per_1_quant;
		orig_per_2_quant	=		v_per_2_quant;
		orig_per_3_quant	=		v_per_3_quant;
		strcpy (orig_ingr_1_units,	v_ingr_1_units	);
		strcpy (orig_ingr_2_units,	v_ingr_2_units	);
		strcpy (orig_ingr_3_units,	v_ingr_3_units	);
		strcpy (orig_per_1_units,	v_per_1_units	);
		strcpy (orig_per_2_units,	v_per_2_units	);
		strcpy (orig_per_3_units,	v_per_3_units	);
		orig_pkg_volume		=		v_package_volume;
		orig_pkg_size		=		v_package_size;

		// Second, copy most of the "secondary" package information
		// to the "operative" fields.
		v_ingr_1_quant		=		sec_ingr_1_quant;
		v_ingr_2_quant		=		sec_ingr_2_quant;
		v_ingr_3_quant		=		sec_ingr_3_quant;
		v_per_1_quant		=		sec_per_1_quant;
		v_per_2_quant		=		sec_per_2_quant;
		v_per_3_quant		=		sec_per_3_quant;
		strcpy (v_ingr_1_units,		sec_ingr_1_units	);
		strcpy (v_ingr_2_units,		sec_ingr_2_units	);
		strcpy (v_ingr_3_units,		sec_ingr_3_units	);
		strcpy (v_per_1_units,		sec_per_1_units		);
		strcpy (v_per_2_units,		sec_per_2_units		);
		strcpy (v_per_3_units,		sec_per_3_units		);

		// Since we're replacing the amounts with the "secondary" values,
		// we need to replace the standardized amounts as well.
		v_ingr_1_quant_std = ConvertUnitAmount (v_ingr_1_quant, v_ingr_1_units);
		v_ingr_2_quant_std = ConvertUnitAmount (v_ingr_2_quant, v_ingr_2_units);
		v_ingr_3_quant_std = ConvertUnitAmount (v_ingr_3_quant, v_ingr_3_units);

		// Finally, create the adjusted operative package size/volume.
		// For the relevant drug(s), the number we get from AS/400 is
		// the total number of "operative units" - for example, for
		// Largo 77041 (Pefcent Spray 4 bottles x 8 doses/bottle) we
		// get a "total size" value of 32.000 that we want to convert
		// reflect 8 doses per bottle.
		if (v_package_size > 0)	// Avoid division by zero!
		{
			// Add a smidgin to the float volume just to make sure the integer
			// math gives us the result we want.
			v_package_size		= (int)(sec_pkg_volume + 0.0000001)	/ v_package_size;
			v_package_volume	= sec_pkg_volume					/ (double)v_package_size;
		}
		else	// This should never happen in real life.
		{
			// Don't do anything - just keep the reported "operative" values
			// for package size and package volume.
		}
	}	// Process package data for 2-level-package-size drugs.
	else
	{
		// Store the "secondary" package information to the "original" fields,
		// where they will be safely ignored. Since two_pkg_size_levels is
		// FALSE, these values should all be zero/blank.
		orig_ingr_1_quant	=		sec_ingr_1_quant;
		orig_ingr_2_quant	=		sec_ingr_2_quant;
		orig_ingr_3_quant	=		sec_ingr_3_quant;
		orig_per_1_quant	=		sec_per_1_quant;
		orig_per_2_quant	=		sec_per_2_quant;
		orig_per_3_quant	=		sec_per_3_quant;
		strcpy (orig_ingr_1_units,	sec_ingr_1_units	);
		strcpy (orig_ingr_2_units,	sec_ingr_2_units	);
		strcpy (orig_ingr_3_units,	sec_ingr_3_units	);
		strcpy (orig_per_1_units,	sec_per_1_units		);
		strcpy (orig_per_2_units,	sec_per_2_units		);
		strcpy (orig_per_3_units,	sec_per_3_units		);
		orig_pkg_volume		=		sec_pkg_volume;
		orig_pkg_size		=		(int)(sec_pkg_volume + 0.0000001);
	}	// Process package data for ordinary drugs.


	retcode			= ALL_RETRIES_WERE_USED;
	stop_retrying	= false;
	need_to_read	= true;


	// Perform up to SQL_UPDATE_RETRIES retries to overcome access conflicts.
	for (n_retries = 0;
		 (n_retries < SQL_UPDATE_RETRIES) && (stop_retrying == false);
		 n_retries++)
	{
		// If this isn't the first iteration, sleep for a while.
		if (n_retries > 0)
		{
			sleep (SLEEP_TIME_BETWEEN_RETRIES);
		}

		// Attempt to read the row first. If we get a result other than access conflict, we'll
		// know whether to insert or update. If we have an access conflict, need_to_read will
		// stay true, so we'll know to retry from this point; otherwise we'll skip this section
		// on the retry.
		if (need_to_read == true)
		{
			ExecSQL (	MAIN_DB, AS400SRV_T4005_READ_old_drug_list_values,
						&o_largo_type			, &o_priv_pharm_sale_ok	, &o_long_name			,
						&o_package				, &o_package_size		, &o_package_volume		,
						&o_drug_book_flg		, &o_basic_price_code	,
						&o_t_half				, &o_chronic_months		, &o_supplemental_1		,
						&o_supplemental_2		, &o_supplemental_3		, &o_supplier_code		,

						&o_computersoft_code	, &o_additional_price	, &o_no_presc_sale_flag	,
						&o_IshurQtyLimIngr		, &o_name_25			, &o_del_flg			,
						&o_in_health_basket		, &o_pharm_group_code	, &o_short_name			,
						&o_form_name			, &o_price_prcnt_b		, &o_price_prcnt_m		,
						&o_price_prcnt_k		, &o_drug_type			, &o_pharm_sale_new		,

						&o_status_send			, &o_sale_price			, &o_interact_flg		,
						&o_magen_wait_months	, &o_keren_wait_months	, &o_del_flg_cont		,
						&o_drug_book_doct_flg	, &o_openable_pkg		, &o_ishur_required		,
						&o_ingr_1_code			, &o_ingr_2_code		, &o_ingr_3_code		,
						&o_ingr_1_quant			, &o_ingr_2_quant		, &o_ingr_3_quant		,

						&o_ingr_1_units			, &o_ingr_2_units		, &o_ingr_3_units		,
						&o_per_1_quant			, &o_per_2_quant		, &o_per_3_quant		,
						&o_per_1_units			, &o_per_2_units		, &o_per_3_units		,
						&o_pharm_sale_test		, &o_rule_status		, &o_sensitivity_flag	,
						&o_sensitivity_code		, &o_sensitivity_severe	, &o_needs_29_g			,

						&o_chronic_flag			, &o_shape_code			, &o_shape_code_new		,
						&o_split_pill_flag		, &o_treatment_typ_cod	, &o_print_generic_flg	,
						&o_substitute_drug		, &o_copies_to_print	, &o_sale_price_strudel	,
						&o_preferred_flg		, &o_Economypri_Group	, &o_parent_group_code	,
						&o_parent_group_name	, &o_maccabicare_flag	, &o_IshurQtyLimFlg		,

						&o_fps_group_code		, &o_assuta_drug_status	, &o_expiry_date_flag	,
						&o_bar_code_value		, &o_tikrat_mazon		, &o_class_code			,
						&o_enabled_status		, &o_percnt_y			, &o_wait_yahalom		,
						&o_future_sales_ok		, &o_cont_treatment		, &o_time_of_day		,
						&o_treatment_days		, &o_illness_bitmap		, &o_how_to_take		,
						&o_unit_abbrev			, &o_Economypri_Seq		, &o_tight_limits		,

						&v_changed_date			, &v_changed_time		,
						&v_pharm_update_dt		, &v_pharm_update_tm	,
						&v_doc_update_date		, &v_doc_update_time	,
						&v_update_date			, &v_update_time		, &o_HealthBasketNew	,
						&v_update_date_d		, &v_update_time_d		, &o_diffs,
						&o_qualify_future_sales	, &o_maccabi_profit_rating,
						&o_is_narcotic			, &o_psychotherapy_drug	, &o_preparation_type	,

						&o_chronic_days			,
				
						// DonR 15Jan2026 User Story #450353: 15 new columns.
						&o_two_pkg_size_levels	, &o_orig_ingr_1_quant	, &o_orig_ingr_2_quant	,
						&o_orig_ingr_3_quant	, &o_orig_ingr_1_units	, &o_orig_ingr_2_units	,
						&o_orig_ingr_3_units	, &o_orig_per_1_quant	, &o_orig_per_2_quant	,
						&o_orig_per_3_quant		, &o_orig_per_1_units	, &o_orig_per_2_units	,
						&o_orig_per_3_units		, &o_orig_pkg_volume	, &o_orig_pkg_size		,

						&v_largo_code,									  END_OF_ARG_LIST			);

			// First possibility: the row isn't already in the database, so we need to insert it.
			if (SQLERR_code_cmp (SQLERR_not_found) == MAC_TRUE)
			{
				do_insert = ATTEMPT_INSERT_FIRST;
				need_to_read = false;
			}
			else
			{
				// Second possibility: there was an access conflict, so advance to the next retry iteration.
				if (SQLERR_code_cmp (SQLERR_access_conflict) == MAC_TRUE)
				{
					retcode = ALL_RETRIES_WERE_USED;
					continue;
				}
				else
				{
					// Third possibility: there was a database error, so don't retry - just give up.
					if (SQLERR_error_test ())
					{
						retcode = SQL_ERROR_OCCURED;
						break;
					}

					else
					// Fourth possibility: fetch succeeded, so we'll do a conditional update.
					{
						do_insert = ATTEMPT_UPDATE_FIRST;
						need_to_read = false;
					}	// Read succeeded.
				}	// Not an access conflict.
			}	// Not record-not-found.
		}	// need_to_read is true.


		// If we get here, we've either read the row to update, or else we've determined that
		// the row isn't there and needs to be inserted.
		// DonR 04Jul2010: HOT FIX: Need to have dummy "for" loop here so that HANDLE_UPDATE_SQL_CODE
		// and HANDLE_INSERT_SQL_CODE work properly.
		for (i = 0; i < 1; i++)
		{
		if (do_insert == ATTEMPT_INSERT_FIRST)
		{
			// DonR 31Oct2006: Sometimes economypri information is received for a new drug
			// before the drug itself is sent to Unix. Just in case this has happened, check
			// the economypri table each time a new drug is added to the system.
			// DonR 21Dec2011: To make more room for "real" generic-substitution groups, the boundary
			// for "therapoietic" groups is being moved from 600 to 800.
			// DonR 05Nov2014: Changing the 799/800 boundary to 999/1000. This is temporary, as the
			// code is being expanded to six digits. When the next phase comes in, the boundary will
			// be at 19999/20000.
			// DonR 31Jan2018: Use new send_and_use_pharm flag instead of a range of Group Codes OR
			// the System Code. This allows us to locate all the table-specific logic for economypri
			// in Transaction 2054/4054.
			v_Economypri_Group	= 0;
			v_Economypri_Seq	= 0;

			ExecSQL (	MAIN_DB, AS400SRV_T4005_READ_get_count_from_economypri,
						&v_Economypri_Group, &v_largo_code, END_OF_ARG_LIST		);

			if ((SQLCODE == 0) && (v_Economypri_Group > 0))	// I.e. something was found.
			{
				ExecSQL (	MAIN_DB, AS400SRV_T4005_READ_get_min_economypri_group_for_drug,
							&v_Economypri_Group, &v_largo_code, END_OF_ARG_LIST				);

				// DonR 04Feb2018: Also copy in the item_seq value for the applicable economypri row we found. This will let us get rid
				// of the last remaining economypri lookup in sqlserver.exe, except for the economypri table-download transaction itself.
				ExecSQL (	MAIN_DB, AS400SRV_T4005_READ_get_economypri_item_seq,
							&v_Economypri_Seq,
							&v_largo_code,		&v_Economypri_Group,	END_OF_ARG_LIST	);
			}

			if (SQLCODE != 0)
			{
				v_Economypri_Group	= 0;
				v_Economypri_Seq	= 0;
			}


			// DonR 29Mar2011: Check if new drug is already in over_dose table.
			// Note that if we hit any DB error, we'll set the flag TRUE - since the worst that
			// can happen is that we'll perform a few unnecessary over_dose lookups. Note also
			// that we don't have to worry about this flag when we update drug_list, as it's set
			// and unset when we receive over_dose information.
			ExecSQL (	MAIN_DB, AS400SRV_T4005_READ_get_count_from_over_dose,
						&RowsFound, &v_largo_code, END_OF_ARG_LIST				);

			v_in_overdose_tbl = ((SQLCODE == 0) && (RowsFound == 0)) ? 0 : 1;


			ExecSQL (	MAIN_DB, AS400SRV_T4005_INS_drug_list,
						&v_largo_code			,
						&v_largo_type			, &v_priv_pharm_sale_ok	, &v_long_name			,
						&v_package				, &v_package_size		, &v_package_volume		,
						&v_drug_book_flg		, &v_basic_price_code	,
						&v_t_half				, &v_chronic_months		, &v_supplemental_1		,
						&v_supplemental_2		, &v_supplemental_3		, &v_supplier_code		,	//  15

						&v_computersoft_code	, &v_additional_price	, &v_no_presc_sale_flag	,
						&v_IshurQtyLimIngr		, &v_name_25			, &v_del_flg			,
						&v_in_health_basket		, &v_pharm_group_code	, &v_short_name			,
						&v_form_name			, &v_price_prcnt_b		, &v_price_prcnt_m		,
						&v_price_prcnt_k		, &v_drug_type			, &v_pharm_sale_new		,	//  30

						&v_status_send			, &v_sale_price			, &v_interact_flg		,
						&v_magen_wait_months	, &v_keren_wait_months	, &v_del_flg_cont		,
						&v_drug_book_doct_flg	, &v_openable_pkg		, &v_ishur_required		,
						&v_ingr_1_code			, &v_ingr_2_code		, &v_ingr_3_code		,
						&v_ingr_1_quant			, &v_ingr_2_quant		, &v_ingr_3_quant		,	//  45

						&v_ingr_1_units			, &v_ingr_2_units		, &v_ingr_3_units		,
						&v_per_1_quant			, &v_per_2_quant		, &v_per_3_quant		,
						&v_per_1_units			, &v_per_2_units		, &v_per_3_units		,
						&v_pharm_sale_test		, &v_rule_status		, &v_sensitivity_flag	,
						&v_sensitivity_code		, &v_sensitivity_severe	, &v_needs_29_g			,	//  60

						&v_chronic_flag			, &v_shape_code			, &v_shape_code_new		,
						&v_split_pill_flag		, &v_treatment_typ_cod	, &v_print_generic_flg	,
						&v_substitute_drug		, &v_copies_to_print	, &v_sale_price_strudel	,
						&v_preferred_flg		, &v_Economypri_Group	, &v_parent_group_code	,
						&v_parent_group_name	, &v_maccabicare_flag	, &v_IshurQtyLimFlg		,	//  75

						&v_fps_group_code		, &v_assuta_drug_status	, &v_expiry_date_flag	,
						&v_bar_code_value		, &v_tikrat_mazon		, &v_class_code			,
						&v_in_overdose_tbl		, &v_enabled_status		, &v_enabled_mac		,
						&v_enabled_hova			, &v_enabled_keva		,
						&v_percnt_y				, &v_wait_yahalom		, &v_future_sales_ok	,	//  89

						&cont_treatment			, &time_of_day_taken	, &treatment_days		,
						&for_cystic_fibro		, &for_tuberculosis		, &for_gaucher			,
						&for_aids				, &for_dialysis			, &for_thalassemia		,
						&for_hemophilia			, &for_cancer			, &for_car_accident		,
						&for_reserved_1			, &for_work_accident	, &for_reserved_3		,	// 104

						&for_reserved_99		, &illness_bitmap		, &how_to_take_code		,
						&unit_abbreviation		, &v_Economypri_Seq		, &tight_ishur_limits	,
						&v_refresh_date			, &v_refresh_time		, &v_HealthBasketNew	,
						&v_as400_batch_date		, &v_as400_batch_time	, &v_ingr_1_quant_std	,
						&v_refresh_date			, &v_refresh_time		, &v_ingr_2_quant_std	,	// 119

						&v_refresh_date			, &v_refresh_time		, &v_ingr_3_quant_std	,
						&v_refresh_date			, &v_refresh_time		,
						&v_refresh_date			, &v_refresh_time		,
						&v_refresh_date			, &v_refresh_time		, &qualify_future_sales	,	// Marianna 14Sep2020 CR #30106
						&maccabi_profit_rating	,													// 130

						&needs_refrigeration	, &needs_patient_instruction	, &needs_dilution			,	// DonR 22Aug2021 User Story #181694
						&needs_preparation		, &home_delivery_ok				, &online_order_pickup_ok	,	// DonR 22Aug2021 User Story #181694
						&is_orthopedic_device	, &needs_patient_measurement	,								// DonR 22Aug2021 User Story #181694
						&ConsignmentItem		,																// Marianna 20Apr2023 User Story #432608
						&TikratPiryonPharmType	,																// DonR 12Feb2025 User Story #376480
						&is_narcotic			, &psychotherapy_drug			, &preparation_type			,

						&chronic_days			, &two_pkg_size_levels			, &orig_ingr_1_quant		,	// 144 DonR 22Jul2025 User Story #427521/417800
						&orig_ingr_2_quant		, &orig_ingr_3_quant			, &orig_ingr_1_units		,
						&orig_ingr_2_units		, &orig_ingr_3_units			, &orig_per_1_quant			,
						&orig_per_2_quant		, &orig_per_3_quant				, &orig_per_1_units			,
						&orig_per_2_units		, &orig_per_3_units				, &orig_pkg_volume			,
						&orig_pkg_size			,																// 159 DonR 22Jul2025 User Story #
				
						END_OF_ARG_LIST
					);

			HANDLE_INSERT_SQL_CODE ("4005", p_connect, &do_insert, &retcode, &stop_retrying);
		}	// Need to insert new drug_list row.


		// If the row is already present, it needs to be updated.
		else
		{
			// If status_send has been changed from true to false,
			// the drug needs to be sent out as a deletion.
			if ((!v_status_send) && (v_largo_code != 0) && o_status_send)
			{
				v_status_send = 1;
				strcpy (v_del_flg_cont, "D");
			}


			// Set up change-diagnostic string.
			// Note that even if every possible change has occurred, the maximum length of
			// the string is 44 - so even with a prepended hyphen to indicate a subsequent
			// refresh, we're safely within the size limit of 50 characters.
			// DonR 20Feb2020: The Barcode value is stored as a char(14) in the database, but
			// we get only 13 characters from AS/400 - which means that every time we compare
			// new to old values, we get a "difference". The solution is to strip trailing
			// spaces from both the old *and* the new values.
			strip_trailing_spaces (o_bar_code_value);

			// DonR 23Sep2020: Eliminate the "diffs" logic, since it's been years since I actually used
			// it to check on anything. After the switchover to MS-SQL, we can drop the "diffs" column
			// from drug_list and all its SQL operations.
#if 0
			strcpy (diffs, "");

			if			(v_pharm_real_time		>  0)							strcat (diffs, "!");

			if			((v_price_prcnt_b		!= o_price_prcnt_b			)	||
						 (v_price_prcnt_m		!= o_price_prcnt_m			)	||
						 (v_percnt_y			!= o_percnt_y				)	||
						 (v_price_prcnt_k		!= o_price_prcnt_k			))	strcat (diffs, "%");

			if			(strcmp (v_sale_price		, o_sale_price			))	strcat (diffs, "$");
			if			(strcmp (v_sale_price_strudel, o_sale_price_strudel	))	strcat (diffs, "@");
			if			(v_pharm_group_code		!= o_pharm_group_code		)	strcat (diffs, "A");
			if			(strcmp (v_short_name		, o_short_name			))	strcat (diffs, "B");

			if			((v_chronic_flag		!= o_chronic_flag			)	||
						 (v_chronic_months		!= o_chronic_months			))	strcat (diffs, "C");

			if			((v_pharm_sale_new		!= o_pharm_sale_new			)	||	
						 strcmp (v_pharm_sale_test	, o_pharm_sale_test		))	strcat (diffs, "D");

			if			((v_drug_book_doct_flg	!= o_drug_book_doct_flg		)	||
						 (v_needs_29_g			!= o_needs_29_g				)	||
						 (v_substitute_drug		!= o_substitute_drug		))	strcat (diffs, "E");

			if			((v_shape_code_new		!= o_shape_code_new			)	||
						 (how_to_take_code		!= o_how_to_take			)	||
						 strcmp (unit_abbreviation		, o_unit_abbrev		)	||
						 strcmp (v_form_name			, o_form_name		)	||
						 (v_split_pill_flag		!= o_split_pill_flag		))	strcat (diffs, "F");

			if			(v_treatment_typ_cod	!= o_treatment_typ_cod		)	strcat (diffs, "G");

			if			((v_copies_to_print		!= o_copies_to_print		)	||
						 (v_print_generic_flg	!= o_print_generic_flg		))	strcat (diffs, "H");

			if			((v_ingr_1_code			!= o_ingr_1_code			)	||
						 (v_ingr_2_code			!= o_ingr_2_code			)	||
						 (v_ingr_3_code			!= o_ingr_3_code			))	strcat (diffs, "I");

			if			((v_package_size		!= o_package_size			)	||
						 strcmp (v_package		,  o_package				)	||
						 (v_openable_pkg		!= o_openable_pkg			)	||
						 (v_package_volume		!= o_package_volume			))	strcat (diffs, "P");

			if			((v_ingr_1_quant		!= o_ingr_1_quant			)	||
						 (v_ingr_2_quant		!= o_ingr_2_quant			)	||
						 (v_ingr_3_quant		!= o_ingr_3_quant			)	||
						 (v_per_1_quant			!= o_per_1_quant			)	||
						 (v_per_2_quant			!= o_per_2_quant			)	||
						 (v_per_3_quant			!= o_per_3_quant			)	||	
						 strcmp (v_ingr_1_units		, o_ingr_1_units		)	||
						 strcmp (v_ingr_2_units		, o_ingr_2_units		)	||
						 strcmp (v_ingr_3_units		, o_ingr_3_units		)	||
						 strcmp (v_per_1_units		, o_per_1_units			)	||
						 strcmp (v_per_2_units		, o_per_2_units			)	||
						 strcmp (v_per_3_units		, o_per_3_units			))	strcat (diffs, "Q");

			if			(v_rule_status			!= o_rule_status			)	strcat (diffs, "R");

			if			((v_sensitivity_flag	!= o_sensitivity_flag		)	||
						 (v_sensitivity_code	!= o_sensitivity_code		)	||
						 strcmp (v_interact_flg		, o_interact_flg		)	||
						 (v_sensitivity_severe	!= o_sensitivity_severe		))	strcat (diffs, "S");

			if			(strcmp (v_drug_type		, o_drug_type			))	strcat (diffs, "T");

			if			((v_magen_wait_months	!= o_magen_wait_months		)	||
						 (v_keren_wait_months	!= o_keren_wait_months		)	||
						 (v_wait_yahalom		!= o_wait_yahalom			))	strcat (diffs, "W");
			if			((v_magen_wait_months	!= o_magen_wait_months		)	||
						 (v_keren_wait_months	!= o_keren_wait_months		)	||
						 (v_wait_yahalom		!= o_wait_yahalom			))	if (v_largo_code < 1000) GerrLogMini(GerrId, "Magen_wait %d/%d, Keren Wait %d/%d, Sheli wait %d/%d.",
							 v_magen_wait_months,o_magen_wait_months, v_keren_wait_months, o_keren_wait_months, v_wait_yahalom, o_wait_yahalom);

			if			(strcmp (v_del_flg_cont		, o_del_flg_cont		))	strcat (diffs, "X");

			if			(v_enabled_status		!= o_enabled_status			)	strcat (diffs, "Y");

			if			(v_priv_pharm_sale_ok	!= o_priv_pharm_sale_ok		)	strcat (diffs, "a");
			if			(v_drug_book_flg		!= o_drug_book_flg			)	strcat (diffs, "b");
			if			(v_basic_price_code		!= o_basic_price_code		)	strcat (diffs, "c");
			if			(v_t_half				!= o_t_half					)	strcat (diffs, "e");
			if			(strcmp (v_name_25			, o_name_25				))	strcat (diffs, "f");

			if			((v_supplemental_1		!= o_supplemental_1			)	||
						 (v_supplemental_2		!= o_supplemental_2			)	||
						 (v_supplemental_3		!= o_supplemental_3			))	strcat (diffs, "h");

			if			(v_supplier_code		!= o_supplier_code			)	strcat (diffs, "i");
			if			(v_computersoft_code	!= o_computersoft_code		)	strcat (diffs, "j");
			if			(v_additional_price		!= o_additional_price		)	strcat (diffs, "k");
			if			(v_no_presc_sale_flag	!= o_no_presc_sale_flag		)	strcat (diffs, "l");
			if			(v_del_flg				!= o_del_flg				)	strcat (diffs, "m");
			if			(v_in_health_basket		!= o_in_health_basket		)	strcat (diffs, "n");
			if			(v_HealthBasketNew		!= o_HealthBasketNew		)	strcat (diffs, "^");
			if			(v_status_send			!= o_status_send			)	strcat (diffs, "o");
			if			(v_ishur_required		!= o_ishur_required			)	strcat (diffs, "p");
			if			(v_shape_code			!= o_shape_code				)	strcat (diffs, "q");
			if			(v_preferred_flg		!= o_preferred_flg			)	strcat (diffs, "r");
			if			(v_parent_group_code	!= o_parent_group_code		)	strcat (diffs, "s");
			if			(v_maccabicare_flag		!= o_maccabicare_flag		)	strcat (diffs, "u");
			if			(v_IshurQtyLimFlg		!= o_IshurQtyLimFlg			)	strcat (diffs, "v");
			if			(v_IshurQtyLimIngr		!= o_IshurQtyLimIngr		)	strcat (diffs, "w");
			if			(strcmp (v_largo_type		, o_largo_type			))	strcat (diffs, "x");
			if			(strcmp (v_long_name		, o_long_name			))	strcat (diffs, "y");
			if			(strcmp (v_parent_group_name, o_parent_group_name	))	strcat (diffs, "z");
			if			(tight_ishur_limits		!= o_tight_limits			)	strcat (diffs, "Z");

			// DonR 23Aug2009: For four new fields, use capital letters in the "gap".
			if			(v_fps_group_code		!= o_fps_group_code			)	strcat (diffs, "J");
			if			(v_assuta_drug_status	!= o_assuta_drug_status		)	strcat (diffs, "K");
			if			(v_expiry_date_flag		!= o_expiry_date_flag		)	strcat (diffs, "L");
			if			(v_tikrat_mazon			!= o_tikrat_mazon			)	strcat (diffs, "M");
			if			(strcmp (v_bar_code_value	, o_bar_code_value		))	strcat (diffs, "N");
			if			(v_class_code			!= o_class_code				)	strcat (diffs, "O");

			// DonR 19May2014: Added treatment fields for doctors, plus illness bitmap.
			if			((cont_treatment		!= o_cont_treatment			)	||
						 (time_of_day_taken		!= o_time_of_day			)	||
						 (treatment_days		!= o_treatment_days			))	strcat (diffs, "U");
			if			(illness_bitmap			!= o_illness_bitmap			)	strcat (diffs, "V");

			// If the current update is just a refresh with no changes, we want to preserve
			// the record of the last time something significant changed - but we'll prepend
			// a hyphen (if there isn't already one) to indicate that a refresh has taken
			// place. If there is a real change, we'll replace the previous change diagnostic.
			if (*diffs == (char)0)	// No change detected.
			{
				if (*o_diffs != '-')
					strcpy (v_diffs, "-");
				else
					strcpy (v_diffs, "");

				strcat (v_diffs, o_diffs);
				v_diffs [50] = (char)0;	// Just to make sure string isn't over length.
			}
			else					// Something "real" has changed.
			{
				strcpy (v_diffs, diffs);
			}
#endif


			// Set update date/time fields based on whether changes have occurred.
			// DonR 06Apr2008: If v_pharm_real_time is non-zero, force an update
			// even if no field values have been changed.
			if ((v_pharm_real_time		>  0)							||
				(v_pharm_group_code		!= o_pharm_group_code		)	||
				(v_package_size			!= o_package_size			)	||
				(v_package_volume		!= o_package_volume			)	||
				(v_ingr_1_code			!= o_ingr_1_code			)	||
				(v_ingr_2_code			!= o_ingr_2_code			)	||
				(v_ingr_3_code			!= o_ingr_3_code			)	||
				(v_ingr_1_quant			!= o_ingr_1_quant			)	||
				(v_ingr_2_quant			!= o_ingr_2_quant			)	||
				(v_ingr_3_quant			!= o_ingr_3_quant			)	||
				(v_per_1_quant			!= o_per_1_quant			)	||
				(v_per_2_quant			!= o_per_2_quant			)	||
				(v_per_3_quant			!= o_per_3_quant			)	||
				(v_price_prcnt_b		!= o_price_prcnt_b			)	||
				(v_price_prcnt_m		!= o_price_prcnt_m			)	||
				(v_price_prcnt_k		!= o_price_prcnt_k			)	||
				(v_percnt_y				!= o_percnt_y				)	||
				(v_pharm_sale_new		!= o_pharm_sale_new			)	||
				(v_drug_book_doct_flg	!= o_drug_book_doct_flg		)	||
				(v_chronic_months		!= o_chronic_months			)	||
				(v_rule_status			!= o_rule_status			)	||
				(v_sensitivity_flag		!= o_sensitivity_flag		)	||
				(v_sensitivity_code		!= o_sensitivity_code		)	||
				(v_sensitivity_severe	!= o_sensitivity_severe		)	||
				(v_openable_pkg			!= o_openable_pkg			)	||
				(v_needs_29_g			!= o_needs_29_g				)	||
				(v_chronic_flag			!= o_chronic_flag			)	||
				(v_shape_code_new		!= o_shape_code_new			)	||
				(v_split_pill_flag		!= o_split_pill_flag		)	||
				(v_treatment_typ_cod	!= o_treatment_typ_cod		)	||
				(v_print_generic_flg	!= o_print_generic_flg		)	||
				(v_magen_wait_months	!= o_magen_wait_months		)	||
				(v_keren_wait_months	!= o_keren_wait_months		)	||
				(v_wait_yahalom			!= o_wait_yahalom			)	||
				(v_substitute_drug		!= o_substitute_drug		)	||
				(v_copies_to_print		!= o_copies_to_print		)	||
				(v_enabled_status		!= o_enabled_status			)	||
				(cont_treatment			!= o_cont_treatment			)	||
				(time_of_day_taken		!= o_time_of_day			)	||
				(treatment_days			!= o_treatment_days			)	||
				(illness_bitmap			!= o_illness_bitmap			)	||
				(how_to_take_code		!= o_how_to_take			)	||
				strcmp (unit_abbreviation	, o_unit_abbrev			)	||
				strcmp (v_short_name		, o_short_name			)	||
				strcmp (v_form_name			, o_form_name			)	||
				strcmp (v_package			, o_package				)	||
				strcmp (v_ingr_1_units		, o_ingr_1_units		)	||
				strcmp (v_ingr_2_units		, o_ingr_2_units		)	||
				strcmp (v_ingr_3_units		, o_ingr_3_units		)	||
				strcmp (v_per_1_units		, o_per_1_units			)	||
				strcmp (v_per_2_units		, o_per_2_units			)	||
				strcmp (v_per_3_units		, o_per_3_units			)	||
				strcmp (v_drug_type			, o_drug_type			)	||
				strcmp (v_pharm_sale_test	, o_pharm_sale_test		)	||
				strcmp (v_sale_price		, o_sale_price			)	||
				strcmp (v_sale_price_strudel, o_sale_price_strudel	)	||
				strcmp (v_interact_flg		, o_interact_flg		)	||
				strcmp (v_del_flg_cont		, o_del_flg_cont		)		)
			{
				// At least one of the fields sent to doctors has changed.
				// Assume that any change at all is relevant to pharmacies.
				v_changed_date			=
					v_doc_update_date	=
					v_pharm_update_dt	=
					v_update_date		=
					v_update_date_d		=	v_refresh_date;

				v_changed_time			=
					v_doc_update_time	=
					v_pharm_update_tm	=
					v_update_time		=
					v_update_time_d		=	v_refresh_time;
					
				something_changed = 1;
			}
			// If nothing relevant to doctors has changed, it's still possible
			// that a change has occurred in one of the remaining fields.
			else
			{
				if ((v_priv_pharm_sale_ok	!= o_priv_pharm_sale_ok		)	||
					(v_drug_book_flg		!= o_drug_book_flg			)	||
					(v_basic_price_code		!= o_basic_price_code		)	||
					(v_t_half				!= o_t_half					)	||
					(v_supplemental_1		!= o_supplemental_1			)	||
					(v_supplemental_2		!= o_supplemental_2			)	||
					(v_supplemental_3		!= o_supplemental_3			)	||
					(v_supplier_code		!= o_supplier_code			)	||
					(v_computersoft_code	!= o_computersoft_code		)	||
					(v_additional_price		!= o_additional_price		)	||
					(v_no_presc_sale_flag	!= o_no_presc_sale_flag		)	||
					(v_del_flg				!= o_del_flg				)	||
					(v_in_health_basket		!= o_in_health_basket		)	||
					(v_HealthBasketNew		!= o_HealthBasketNew		)	||
					(v_status_send			!= o_status_send			)	||
					(v_ishur_required		!= o_ishur_required			)	||
					(v_shape_code			!= o_shape_code				)	||
					(v_preferred_flg		!= o_preferred_flg			)	||
					(v_parent_group_code	!= o_parent_group_code		)	||
					(v_maccabicare_flag		!= o_maccabicare_flag		)	||
					(v_IshurQtyLimFlg		!= o_IshurQtyLimFlg			)	||
					(tight_ishur_limits		!= o_tight_limits			)	||
					(v_IshurQtyLimIngr		!= o_IshurQtyLimIngr		)	||
					(v_fps_group_code		!= o_fps_group_code			)	||
					(v_assuta_drug_status	!= o_assuta_drug_status		)	||
					(v_expiry_date_flag		!= o_expiry_date_flag		)	||
					(v_tikrat_mazon			!= o_tikrat_mazon			)	||
					(v_class_code			!= o_class_code				)	||
					(is_narcotic			!= o_is_narcotic			)	||
					(psychotherapy_drug		!= o_psychotherapy_drug		)	||
					(preparation_type		!= o_preparation_type		)	||
					(chronic_days			!= o_chronic_days			)	||

					// DonR 15Jan2026 User Story #450353: Add all the new "original"
					// package-related columns to the comparison, even though it's
					// probably not strictly necessary.
					(two_pkg_size_levels	!= o_two_pkg_size_levels	)	||
					(orig_ingr_1_quant		!= o_orig_ingr_1_quant		)	||
					(orig_ingr_2_quant		!= o_orig_ingr_2_quant		)	||
					(orig_ingr_3_quant		!= o_orig_ingr_3_quant		)	||
					(orig_per_1_quant		!= o_orig_per_1_quant		)	||
					(orig_per_2_quant		!= o_orig_per_2_quant		)	||
					(orig_per_3_quant		!= o_orig_per_3_quant		)	||
					(orig_pkg_volume		!= o_orig_pkg_volume		)	||
					(orig_pkg_size			!= o_orig_pkg_size			)	||
					strcmp (orig_ingr_1_units	, o_orig_ingr_1_units	)	||
					strcmp (orig_ingr_2_units	, o_orig_ingr_2_units	)	||
					strcmp (orig_ingr_3_units	, o_orig_ingr_3_units	)	||
					strcmp (orig_per_1_units	, o_orig_per_1_units	)	||
					strcmp (orig_per_2_units	, o_orig_per_2_units	)	||
					strcmp (orig_per_3_units	, o_orig_per_3_units	)	||

					strcmp (v_largo_type		, o_largo_type			)	||
					strcmp (v_long_name			, o_long_name			)	||
					strcmp (v_parent_group_name	, o_parent_group_name	)	||
					strcmp (v_bar_code_value	, o_bar_code_value		)	||
					strcmp (v_name_25			, o_name_25				)		)
				{
					// At least one non-doctor-relevant field changed - update
					// pharmacies.
					v_changed_date			=
						v_pharm_update_dt	=
						v_update_date		=	v_refresh_date;

					v_changed_time			=
						v_pharm_update_tm	=
						v_update_time		=	v_refresh_time;
						
					something_changed = 1;
				}
			}	// Nothing changed relevant to doctors.


			ExecSQL (	MAIN_DB, AS400SRV_T4005_UPD_drug_list,
						&v_largo_type,			&v_priv_pharm_sale_ok,		&v_long_name,
						&v_package,				&v_package_size,			&v_package_volume,
						&v_drug_book_flg,		&v_basic_price_code,		&v_t_half,
						&v_chronic_months,		&v_supplemental_1,			&v_supplemental_2,
						&v_supplemental_3,		&v_supplier_code,			&v_computersoft_code,	//  15

						&v_additional_price,	&v_no_presc_sale_flag,		&v_del_flg,
						&v_in_health_basket,	&v_HealthBasketNew,			&v_pharm_group_code,
						&v_short_name,			&v_form_name,				&v_price_prcnt_b,
						&v_price_prcnt_m,		&v_price_prcnt_k,			&v_drug_type,
						&v_pharm_sale_new,		&v_status_send,				&v_sale_price,			//  30

						&v_interact_flg,		&v_magen_wait_months,		&v_keren_wait_months,
						&v_drug_book_doct_flg,	&v_del_flg_cont,			&v_openable_pkg,
						&v_ishur_required,		&v_ingr_1_code,				&v_ingr_2_code,
						&v_ingr_3_code,			&v_ingr_1_quant,			&v_ingr_2_quant,
						&v_ingr_3_quant,		&v_ingr_1_quant_std,		&v_ingr_2_quant_std,	//  45

						&v_ingr_3_quant_std,	&v_ingr_1_units,			&v_ingr_2_units,
						&v_ingr_3_units,		&v_per_1_quant,				&v_per_2_quant,
						&v_per_3_quant,			&v_per_1_units,				&v_per_2_units,
						&v_per_3_units,			&v_pharm_sale_test,			&v_rule_status,
						&v_sensitivity_flag,	&v_sensitivity_code,		&v_sensitivity_severe,	//  60

						&v_needs_29_g,			&v_chronic_flag,			&v_shape_code,
						&v_shape_code_new,		&v_split_pill_flag,			&v_treatment_typ_cod,
						&v_print_generic_flg,	&v_substitute_drug,			&v_copies_to_print,
						&v_sale_price_strudel,	&v_preferred_flg,			&v_parent_group_code,
						&v_parent_group_name,	&v_maccabicare_flag,		&v_IshurQtyLimFlg,		//  75

						&tight_ishur_limits,	&v_IshurQtyLimIngr,			&v_name_25,
						&v_fps_group_code,		&v_assuta_drug_status,		&v_expiry_date_flag,
						&v_bar_code_value,		&v_tikrat_mazon,			&v_class_code,
						&v_enabled_status,		&v_enabled_mac,				&v_enabled_hova,
						&v_enabled_keva,		&v_percnt_y,				&v_wait_yahalom,		//  90
				
						&v_future_sales_ok,		&cont_treatment,			&time_of_day_taken,
						&treatment_days,		&for_cystic_fibro,			&for_tuberculosis,
						&for_gaucher,			&for_aids,					&for_dialysis,
						&for_thalassemia,		&for_hemophilia,			&for_cancer,
						&for_car_accident,		&for_reserved_1,			&for_work_accident,		// 105
				
						&for_reserved_3,		&for_reserved_99,			&illness_bitmap,
						&how_to_take_code,		&unit_abbreviation,			&v_refresh_date,
						&v_refresh_time,		&v_as400_batch_date,		&v_as400_batch_time,
						&v_changed_date,		&v_changed_time,			&v_pharm_update_dt,
						&v_pharm_update_tm,		&v_doc_update_date,			&v_doc_update_time,		// 120
				
						&v_update_date,			&v_update_time,				&v_diffs,
						&v_update_date_d,		&v_update_time_d,			&qualify_future_sales,	// Marianna 14Sep2020 CR #30106
						&maccabi_profit_rating,														// 127

						&needs_refrigeration	, &needs_patient_instruction	, &needs_dilution			,	// DonR 22Aug2021 User Story #181694
						&needs_preparation		, &home_delivery_ok				, &online_order_pickup_ok	,	// DonR 22Aug2021 User Story #181694
						&is_orthopedic_device	, &needs_patient_measurement	,								// DonR 22Aug2021 User Story #181694
						&ConsignmentItem		,																// Marianna 20Apr2023 User Story #432608
						&TikratPiryonPharmType	,																// 137 DonR 12Feb2025 User Story #376480
						&is_narcotic			, &psychotherapy_drug			, &preparation_type			,
						&chronic_days			,																// 141 DonR 22Jul2025 User Story #427521/417800

						&two_pkg_size_levels	, &orig_ingr_1_quant			, &orig_ingr_2_quant		,
						&orig_ingr_3_quant		, &orig_ingr_1_units			, &orig_ingr_2_units		,
						&orig_ingr_3_units		, &orig_per_1_quant				, &orig_per_2_quant			,
						&orig_per_3_quant		, &orig_per_1_units				, &orig_per_2_units			,
						&orig_per_3_units		, &orig_pkg_volume				, &orig_pkg_size			,	// 156 DonR 14Jan2026 User Story #450353

						&v_largo_code,			  END_OF_ARG_LIST												// 157
					);

			HANDLE_UPDATE_SQL_CODE ("4005", p_connect, &do_insert, &retcode, &stop_retrying);

		}	// In update mode (as row already exists).
		}	// Dummy loop.

	}	// Retry Loop.


	if (n_retries == SQL_UPDATE_RETRIES)
	{
		WRITE_ACCESS_CONFLICT_TO_LOG ("4005");
	}

  if (retcode == MSG_HANDLER_SUCCEEDED)
  {
	  if (do_insert == ATTEMPT_INSERT_FIRST)
	  {
		  rows_added++;
	  }
	  else
	  {
          if (something_changed)
			  rows_updated++;
		  else
		      rows_refreshed++;
	  }
  }


	return retcode;

}	// end of msg 4005 handler


// DonR 13Nov2018 CR #15919: Added new postprocess routine just to get the new table size.
static int postprocess_4005 (void)
{
	// DB variables
	int				v_as400_batch_date;
	int				v_as400_batch_time;


	// Initialize variables.
	v_as400_batch_date	= download_start_date;
	v_as400_batch_time	= download_start_time;

	// DonR 13Nov2018 CR #15919: Record current table size.
	REMEMBER_ISOLATION;
	SET_ISOLATION_DIRTY;

	ExecSQL (	MAIN_DB, AS400SRV_T4005PP_READ_drug_list_table_size,
				&total_rows, END_OF_ARG_LIST							);

	ExecSQL (	MAIN_DB, AS400SRV_T4005PP_READ_drug_list_num_active_rows,
				&active_rows,
				&v_as400_batch_date, &v_as400_batch_time, END_OF_ARG_LIST		);

	//Marianna 10Aug2023 :User Story #469361
	ExecSQL (	MAIN_DB, AS400SRV_UPD_drug_list_compute_duration, END_OF_ARG_LIST);

	if (SQLCODE == 0)
		GerrLogMini (GerrId, "Postprocess 4005: Updated compute_duration for %d drugs.", sqlca.sqlerrd [2]);

	RESTORE_ISOLATION;
	// DonR 13Nov2018 CR #15919 end.
}	// postprocess_4005() end.


/* << -------------------------------------------------------- >> */
/*                                                                */
/*     handler for message 2006 - delete rec from DRUG_LIST	  */
/*                                                                */
/* << -------------------------------------------------------- >> */
static int message_2006_handler(
				TConnection	*p_connect,
				int		data_len
				)
{

/* << -------------------------------------------------------- >> */
/*			  local variables			  */
/* << -------------------------------------------------------- >> */
  char          *PosInBuff;		/* current pos in data buffer	    */
  int		ReadInput;		/* amount of Bytesread from buffer  */
  int		retcode;		/* code returned from function	    */
  int		n_retries;		/* retrying counter		    */
  int		i;
  int           db_changed_flg;


	/* << -------------------------------------------------------- >> */
	/*			    DB variables			  */
	/* << -------------------------------------------------------- >> */
	Tlargo_code		v_largo_code;
	int				sysdate;
	int				systime;


	sysdate = GetDate ();
	systime = GetTime ();

/* << -------------------------------------------------------- >> */
/*			  read input buffer			  */
/* << -------------------------------------------------------- >> */
  INIT_ERROR_DETECTION_MECHANISM;
  PosInBuff = glbMsgBuffer;

  v_largo_code		=  get_long(
				    &PosInBuff,
				    9
				    );	//Marianna 14Jul2024 User Story #330877
  RETURN_ON_ERROR;




/* << -------------------------------------------------------- >> */
/*			  debug messages			  */
/* << -------------------------------------------------------- >> */
  /************************  COMMENTED OUT:  *****************************
  printf( "Tlargo_code == %d\n", v_largo_code );
  fflush( stdout );
  ************************************************************************/


/* << -------------------------------------------------------- >> */
/*		      verify all deta was read			  */
/* << -------------------------------------------------------- >> */
  ReadInput = PosInBuff - glbMsgBuffer;
  StaticAssert( ReadInput == data_len );


/* << -------------------------------------------------------- >> */
/*								  */
/*		   Delete from all relevant tables		  */
/*								  */
/* << -------------------------------------------------------- >> */

  db_changed_flg = 0;

  do
  {

      /* < ---------------------------- *
       *   1.  table DRUG_LIST: 	*
       * ---------------------------- > */
      retcode = ALL_RETRIES_WERE_USED;
      for( n_retries = 0;
	   n_retries < SQL_UPDATE_RETRIES;
	   n_retries++ )
      {

	  /*
	   * sleep for a while:
	   */
	  if( n_retries )
	  {
	      sleep( SLEEP_TIME_BETWEEN_RETRIES );
	  }

	  ExecSQL (	MAIN_DB, AS400SRV_T2006_DEL_drug_list,
				&v_largo_code, END_OF_ARG_LIST			);

	  HANDLE_DELETE_SQL_CODE( "2006", p_connect, &retcode,&db_changed_flg);

	  ExecSQL (	MAIN_DB, AS400SRV_T2006_DEL_price_list,
				&v_largo_code, END_OF_ARG_LIST				);

	  HANDLE_DELETE_SQL_CODE( "2006", p_connect, &retcode,&db_changed_flg);

	  ExecSQL (	MAIN_DB, AS400SRV_T2006_DEL_doctor_percents,
				&v_largo_code, END_OF_ARG_LIST					);

	  HANDLE_DELETE_SQL_CODE( "2006", p_connect, &retcode,&db_changed_flg);

	  ExecSQL (	MAIN_DB, AS400SRV_T2006_DEL_drug_extension,
				&v_largo_code, END_OF_ARG_LIST				);

	  HANDLE_DELETE_SQL_CODE( "2006", p_connect, &retcode,&db_changed_flg);

	  ExecSQL (	MAIN_DB, AS400SRV_T2006_DEL_drug_interaction,
				&v_largo_code, END_OF_ARG_LIST					);

	  HANDLE_DELETE_SQL_CODE( "2006", p_connect, &retcode,&db_changed_flg);

	  ExecSQL (	MAIN_DB, AS400SRV_T2006_DEL_over_dose,
				&v_largo_code, END_OF_ARG_LIST			);

	  HANDLE_DELETE_SQL_CODE( "2006", p_connect, &retcode,&db_changed_flg);

	  ExecSQL (	MAIN_DB, AS400SRV_T2006_DEL_spclty_largo_prcnt,
				&v_largo_code, END_OF_ARG_LIST						);

	  HANDLE_DELETE_SQL_CODE( "2006", p_connect, &retcode,&db_changed_flg);

	  break;
      }

      if( retcode != MSG_HANDLER_SUCCEEDED )
      {
	  if( n_retries == SQL_UPDATE_RETRIES )
	  {
	     WRITE_ACCESS_CONFLICT_TO_LOG( "2006" );
	  }
	  break;
      }

  }
  while( 0 );

  if (retcode == MSG_HANDLER_SUCCEEDED)
	  rows_deleted++;


  return retcode;
} /* end of msg 2006 handler */



/* << -------------------------------------------------------- >> */
/*                                                                */
/*       handler for message 2007/4007 - upd/ins to OVER_DOSE	  */
/*                                                                */
/* << -------------------------------------------------------- >> */
static int data_2007_handler (TConnection	*p_connect,
							  int			data_len)
{

/* << -------------------------------------------------------- >> */
/*			  local variables			  */
/* << -------------------------------------------------------- >> */
  char      *PosInBuff;		/* current pos in data buffer	    */
  int		ReadInput;		/* amount of Bytesread from buffer  */
  int		retcode;		/* code returned from function	    */
  bool		stop_retrying;		/* if true - return from function   */
  bool		do_insert = ATTEMPT_UPDATE_FIRST;
  int		n_retries;		/* retrying counter		    */
  int		i;


	/* << -------------------------------------------------------- >> */
	/*			    DB variables			  */
	/* << -------------------------------------------------------- >> */
	int					v_largo_code;
	short				v_from_age;
	int					v_max_units_per_day;
	int					sysdate;
	int					systime;
	int					v_as400_batch_date;
	int					v_as400_batch_time;
	short				InFullTableMode;


	InFullTableMode		= (download_start_date > 0);
	sysdate = GetDate();
	systime = GetTime();
	v_as400_batch_date = (InFullTableMode) ? download_start_date : sysdate;
	v_as400_batch_time = (InFullTableMode) ? download_start_time : systime;

/* << -------------------------------------------------------- >> */
/*			read input buffer			  */
/* << -------------------------------------------------------- >> */
  INIT_ERROR_DETECTION_MECHANISM;
  PosInBuff = glbMsgBuffer;

  v_largo_code		=  get_long(
				    &PosInBuff,
				    9
				    );	//Marianna 14Jul2024 User Story #330877
  RETURN_ON_ERROR;

  v_from_age		=  get_short(
				     &PosInBuff,
				     Tfrom_age_len
				     );
  RETURN_ON_ERROR;
  /*
  v_from_age *= -1;
  */

  v_max_units_per_day	=  get_long(
				    &PosInBuff,
				    Tmax_units_per_day_len
				    );
  RETURN_ON_ERROR;


/* << -------------------------------------------------------- >> */
/*			    debug messages			  */
/* << -------------------------------------------------------- >> */
  /************************* COMMENTED OUT: *************************
  printf( "largo_code == %d\n", 	v_largo_code );
  printf( "from_age == %d\n", 		v_from_age );
  printf( "max_units_per_day == %d\n", v_max_units_per_day );
  fflush( stdout );
  *******************************************************************/

  /*
   *   verify all data was read:
   */
  ReadInput = PosInBuff - glbMsgBuffer;
  StaticAssert( ReadInput == data_len );


/* << -------------------------------------------------------- >> */
/*	   up to SQL_UPDATE_RETRIES retries will be made	  */
/*		    to overcome access conflict			  */
/* << -------------------------------------------------------- >> */
	retcode = ALL_RETRIES_WERE_USED;
	stop_retrying = false;

	for (n_retries = 0; (n_retries < SQL_UPDATE_RETRIES) && (stop_retrying == false); n_retries++)
	{
		// Sleep for a while:
		if (n_retries)
		{
			sleep (SLEEP_TIME_BETWEEN_RETRIES);
		}

		// DonR 18Mar2020: If we're in full-table mode, the postprocessor will take
		// care of updating drug_list/in_overdose_table. And if we're *not* in
		// full-table mode, we want to update in_overdose_table even if we're not
		// inserting a new row - just in case something is out of sync.
		if (!InFullTableMode)
		{
			ExecSQL (	MAIN_DB, AS400SRV_T4007_UPD_drug_list,
						&v_largo_code, END_OF_ARG_LIST			);
		}

		// Try to insert/update new rec with the order depending on "do_insert".
		for (i = 0;  i < 2;  i++)
		{
			if (do_insert == ATTEMPT_INSERT_FIRST)
			{
				ExecSQL (	MAIN_DB, AS400SRV_T4007_INS_over_dose,
							&v_largo_code,			&v_from_age,
							&sysdate,				&systime,
							&v_as400_batch_date,	&v_as400_batch_time,
							&v_max_units_per_day,	&ROW_NOT_DELETED,
							END_OF_ARG_LIST									);

				HANDLE_INSERT_SQL_CODE ("2007", p_connect, &do_insert, &retcode, &stop_retrying);
			}
			else
			{
				ExecSQL (	MAIN_DB, AS400SRV_T4007_UPD_over_dose,
							&sysdate,				&systime,
							&v_as400_batch_date,	&v_as400_batch_time,
							&v_max_units_per_day,
							&v_largo_code,			&v_from_age,
							END_OF_ARG_LIST									);

				HANDLE_UPDATE_SQL_CODE ("2007", p_connect, &do_insert, &retcode, &stop_retrying);

			}	// In update mode.

		}	// for ( i...

	}	/* for( n_retries... */

	if (n_retries == SQL_UPDATE_RETRIES)
	{
		WRITE_ACCESS_CONFLICT_TO_LOG ( "2007" );
	}

	if (retcode == MSG_HANDLER_SUCCEEDED)
	{
		if (do_insert == ATTEMPT_INSERT_FIRST)
			rows_added++;
		else
			rows_updated++;
	}


	return retcode;
} /* end of msg 2007 handler */


/* << -------------------------------------------------------- >> */
/*                                                                */
/*     handler for message 2008 - delete rec from OVER_DOSE	  */
/*                                                                */
/* << -------------------------------------------------------- >> */
static int message_2008_handler(
				TConnection	*p_connect,
				int		data_len
				)
{

/* << -------------------------------------------------------- >> */
/*			  local variables			  */
/* << -------------------------------------------------------- >> */
  char          *PosInBuff;		/* current pos in data buffer	    */
  int		ReadInput;		/* amount of Bytesread from buffer  */
  int		retcode;		/* code returned from function	    */
  int		n_retries;		/* retrying counter		    */
  int		i;
  int		db_changed_flg;


/* << -------------------------------------------------------- >> */
/*			    DB variables			  */
/* << -------------------------------------------------------- >> */
  Tlargo_code	v_largo_code;
  int			v_count;


/* << -------------------------------------------------------- >> */
/*			  read input buffer			  */
/* << -------------------------------------------------------- >> */
  INIT_ERROR_DETECTION_MECHANISM;
  PosInBuff = glbMsgBuffer;

  v_largo_code		=  get_long(
				    &PosInBuff,
				    9
				    );	//Marianna 14Jul2024 User Story #330877
  RETURN_ON_ERROR;




/* << -------------------------------------------------------- >> */
/*			  debug messages			  */
/* << -------------------------------------------------------- >> */
  /************************ COMMENTED OUT: *************************
  printf( "Tlargo_code == %d\n", v_largo_code );
  fflush( stdout );
  ******************************************************************/


/* << -------------------------------------------------------- >> */
/*		      verify all deta was read			  */
/* << -------------------------------------------------------- >> */
  ReadInput = PosInBuff - glbMsgBuffer;
  StaticAssert( ReadInput == data_len );


/* << -------------------------------------------------------- >> */
/*			   attempt delete			  */
/* << -------------------------------------------------------- >> */
  retcode = ALL_RETRIES_WERE_USED;
  db_changed_flg = 0;

  for( n_retries = 0;
       n_retries < SQL_UPDATE_RETRIES;
       n_retries++ )
    {

      /*
       * sleep for a while:
       */
      if( n_retries )
      {
		  sleep( SLEEP_TIME_BETWEEN_RETRIES );
      }

	  // DonR 19Jan2012: Because full-table download from RKUNIX is now implemented for this table, the
	  // largo_code = -99 delete-all-old-stuff command is no longer sent by the AS/400 - so we can get
	  // rid of this code on the receiving end.

		ExecSQL (	MAIN_DB, AS400SRV_T2006_DEL_over_dose,
				&v_largo_code, END_OF_ARG_LIST			);

      HANDLE_DELETE_SQL_CODE( "2008", p_connect, &retcode, &db_changed_flg );

      break;
    }


  if( n_retries == SQL_UPDATE_RETRIES )
  {
     WRITE_ACCESS_CONFLICT_TO_LOG( "2008" );
  }

  if (retcode == MSG_HANDLER_SUCCEEDED)
	  rows_deleted++;


  return retcode;
} /* end of msg 2008 handler */



// -------------------------------------------------------------------------
//
//       Post-processor for message 4007 - delete OVER_DOSE
//			rows that are no longer in the AS/400 full-table download.
//		DonR 18Mar2020: Also set the flag TRUE if necessary.
//
// -------------------------------------------------------------------------
static int postprocess_4007 (TConnection *p_connect)
{
	// local variables
	bool	stop_retrying;	// if true - return from function
	int		rows_deleted	= 0;
	int		retcode;		// code returned from function
	int		n_retries;		// retrying counter
	int		db_changed_flg;

	// DB variables
	int				v_as400_batch_date;
	int				v_as400_batch_time;
	int				v_row_count;


	// Initialize variables.
	v_as400_batch_date	= download_start_date;
	v_as400_batch_time	= download_start_time;


	retcode			= ALL_RETRIES_WERE_USED;
	stop_retrying	= false;


	while (1)
	{
		ExecSQL (	MAIN_DB, AS400SRV_T4007PP_READ_over_dose_stale_count,
					&v_row_count,
					&v_as400_batch_date, &v_as400_batch_time, END_OF_ARG_LIST	);

		if (SQLERR_error_test ())
		{
			retcode = SQL_ERROR_OCCURED;
			break;
		}
		else
		{
			if (v_row_count == 0)
			{
				retcode = MSG_HANDLER_SUCCEEDED;
				break;
			}
		}

		ExecSQL (	MAIN_DB, AS400SRV_T4007PP_DEL_over_dose,
					&v_as400_batch_date, &v_as400_batch_time, END_OF_ARG_LIST	);

		if (SQLERR_code_cmp (SQLERR_no_rows_affected) != MAC_TRUE)	// Don't bother if there's nothing to do.
		{
			// DonR 29Mar2011: Make sure the in-overdose-table flag in drug_list is up to date.
			ExecSQL (	MAIN_DB, AS400SRV_T4007PP_UPD_drug_list_clear_OD_flags,	END_OF_ARG_LIST	);
			ExecSQL (	MAIN_DB, AS400SRV_T4007PP_UPD_drug_list_set_OD_flags,	END_OF_ARG_LIST	);
		}

		HANDLE_DELETE_SQL_CODE ("Postprocess 4007", p_connect, &retcode, &db_changed_flg);

		break;
	}

	rows_deleted = v_row_count;


	// DonR 13Nov2018 CR #15919: Record current table size.
	REMEMBER_ISOLATION;
	SET_ISOLATION_DIRTY;

	ExecSQL (	MAIN_DB, AS400SRV_T4007PP_READ_over_dose_table_size,
				&total_rows, END_OF_ARG_LIST							);

	active_rows = total_rows;

	RESTORE_ISOLATION;
	// DonR 13Nov2018 CR #15919 end.


	GerrLogMini (GerrId,
				 "Postprocess 4007: %d rows deleted, retcode = %d.",
				 rows_deleted, retcode);

	return retcode;

}	// End of msg 4007 postprocessor.



/* << -------------------------------------------------------- >> */
/*                                                                */
/*   handler for message 2009 - upd/ins to INTERACTION_TYPE	  */
/*                                                                */
/* << -------------------------------------------------------- >> */
static int message_2009_handler(
				TConnection	*p_connect,
				int		data_len,
				bool		do_insert
				)
{

/* << -------------------------------------------------------- >> */
/*			  local variables			  */
/* << -------------------------------------------------------- >> */
  char          *PosInBuff;		/* current pos in data buffer	    */
  int		ReadInput;		/* amount of Bytesread from buffer  */
  int		retcode;		/* code returned from function	    */
  bool		stop_retrying;		/* if true - return from function   */
  int		n_retries;		/* retrying counter		    */
  int		i;


	/* << -------------------------------------------------------- >> */
	/*			    DB variables			  */
	/* << -------------------------------------------------------- >> */
	Tinteraction_type	v_interaction_type;
	Tdur_message		v_dur_message;
	Tinteraction_level	v_interaction_level;
	int					sysdate,
						systime;
	int					v_as400_batch_date;
	int					v_as400_batch_time;


  sysdate = GetDate();
  systime = GetTime();
	v_as400_batch_date = (download_start_date > 0) ? download_start_date : sysdate;
	v_as400_batch_time = (download_start_time > 0) ? download_start_time : systime;

/* << -------------------------------------------------------- >> */
/*			read input buffer			  */
/* << -------------------------------------------------------- >> */
  INIT_ERROR_DETECTION_MECHANISM;
  PosInBuff = glbMsgBuffer;


  v_interaction_type	=  get_short(
				     &PosInBuff,
				     Tinteraction_type_len
				     );
  RETURN_ON_ERROR;

			   get_string(
				      &PosInBuff,
				      v_dur_message,
				      Tdur_message_len
				      );
  switch_to_win_heb( ( unsigned char * )v_dur_message );


  v_interaction_level	=  get_short(
				     &PosInBuff,
				     Tinteraction_level_len
				     );
  RETURN_ON_ERROR;



/* << -------------------------------------------------------- >> */
/*			    debug messages			  */
/* << -------------------------------------------------------- >> */
  /************************* COMMENTED OUT: ***************************
  printf( "Tinteraction_type == %d\n", 	v_interaction_type );
  printf( "Tdur_message == %s\n", 	v_dur_message );
  printf( "Tinteraction_level == %d\n", v_interaction_level );
  fflush( stdout );
  *********************************************************************/


  /*
   *   verify all data was read:
   */
  ReadInput = PosInBuff - glbMsgBuffer;
  StaticAssert( ReadInput == data_len );


/* << -------------------------------------------------------- >> */
/*	   up to SQL_UPDATE_RETRIES retries will be made	  */
/*		    to overcome access conflict			  */
/* << -------------------------------------------------------- >> */
  retcode = ALL_RETRIES_WERE_USED;
  stop_retrying = false;

  for(
      n_retries = 0;
      ( n_retries < SQL_UPDATE_RETRIES ) && ( stop_retrying == false );
      n_retries++
      )
    {

      /*
       * sleep for a while:
       */
      if( n_retries )
      {
	  sleep( SLEEP_TIME_BETWEEN_RETRIES );
      }


      /*
       *  try to insert/update new rec with the order depending on "do_insert":
       */
      for( i = 0;  i < 2;  i++ )
	{
	  if( do_insert == ATTEMPT_INSERT_FIRST )
	    {
			ExecSQL (	MAIN_DB, AS400SRV_T4009_INS_interaction_type,
						&v_interaction_type,	&v_dur_message,
						&sysdate,				&systime,
						&v_as400_batch_date,	&v_as400_batch_time,
						&v_interaction_level,	&ROW_NOT_DELETED,
						END_OF_ARG_LIST									);

              HANDLE_INSERT_SQL_CODE( "2009",
					p_connect,
				      &do_insert,
				      &retcode,
				      &stop_retrying );

	    }
	  else
	    {
			ExecSQL (	MAIN_DB, AS400SRV_T4009_UPD_interaction_type,
						&v_dur_message,			&v_interaction_level,
						&v_as400_batch_date,	&v_as400_batch_time,
						&sysdate,				&systime,
						&v_interaction_type,	END_OF_ARG_LIST			);

              HANDLE_UPDATE_SQL_CODE( "2009",
					p_connect,
				      &do_insert,
				      &retcode,
				      &stop_retrying );
	    }

	}/* for( i... */

    }/* for( n_retries... */

  if( n_retries == SQL_UPDATE_RETRIES )
  {
     WRITE_ACCESS_CONFLICT_TO_LOG( "2009" );
  }

  if (retcode == MSG_HANDLER_SUCCEEDED)
  {
	  if (do_insert == ATTEMPT_INSERT_FIRST)
		  rows_added++;
	  else
		  rows_updated++;
  }


  return retcode;
} /* end of msg 2009 handler */


/* << -------------------------------------------------------- >> */
/*                                                                */
/*  handler for message 2010 - delete rec from INTERACTION_TYPE	  */
/*                                                                */
/* << -------------------------------------------------------- >> */
static int message_2010_handler(
				TConnection	*p_connect,
				int		data_len
				)
{

/* << -------------------------------------------------------- >> */
/*			  local variables			  */
/* << -------------------------------------------------------- >> */
  char          *PosInBuff;		/* current pos in data buffer	    */
  int		ReadInput;		/* amount of Bytesread from buffer  */
  int		retcode;		/* code returned from function	    */
  int		n_retries;		/* retrying counter		    */
  int		i;
  int		db_changed_flg;


	/* << -------------------------------------------------------- >> */
	/*			    DB variables			  */
	/* << -------------------------------------------------------- >> */
	Tinteraction_type	v_interaction_type;



/* << -------------------------------------------------------- >> */
/*			  read input buffer			  */
/* << -------------------------------------------------------- >> */
  INIT_ERROR_DETECTION_MECHANISM;
  PosInBuff = glbMsgBuffer;

  v_interaction_type	=  get_short(
				     &PosInBuff,
				     Tinteraction_type_len
				     );
  RETURN_ON_ERROR;




/* << -------------------------------------------------------- >> */
/*			  debug messages			  */
/* << -------------------------------------------------------- >> */
  /************************ COMMENTED OUT: *************************
  printf( "interaction_type == %d\n", v_interaction_type );
  fflush( stdout );
  ******************************************************************/


/* << -------------------------------------------------------- >> */
/*		      verify all deta was read			  */
/* << -------------------------------------------------------- >> */
  ReadInput = PosInBuff - glbMsgBuffer;
  StaticAssert( ReadInput == data_len );


/* << -------------------------------------------------------- >> */
/*			   attempt delete			  */
/* << -------------------------------------------------------- >> */
  retcode = ALL_RETRIES_WERE_USED;
  db_changed_flg = 0;

  for( n_retries = 0;
       n_retries < SQL_UPDATE_RETRIES;
       n_retries++ )
    {

      /*
       * sleep for a while:
       */
      if( n_retries )
      {
	  sleep( SLEEP_TIME_BETWEEN_RETRIES );
      }

		ExecSQL (	MAIN_DB, AS400SRV_T2010_DEL_interaction_type,
					&v_interaction_type, END_OF_ARG_LIST			);

      HANDLE_DELETE_SQL_CODE( "2010", p_connect, &retcode, &db_changed_flg );

      break;
    }

  if( n_retries == SQL_UPDATE_RETRIES )
  {
     WRITE_ACCESS_CONFLICT_TO_LOG( "2010" );
  }

  if (retcode == MSG_HANDLER_SUCCEEDED)
	  rows_deleted++;


  return retcode;
} /* end of msg 2010 handler */


// -------------------------------------------------------------------------
//
//       Post-processor for message 4009 - delete INTERACTION_TYPE
//			rows that are no longer in the AS/400 full-table download.
//
// -------------------------------------------------------------------------
static int postprocess_4009 (TConnection *p_connect)
{
	// local variables
	bool	stop_retrying;	// if true - return from function
	int		rows_deleted	= 0;
	int		retcode;		// code returned from function
	int		n_retries;		// retrying counter
	int		db_changed_flg;

	// DB variables
	int				v_as400_batch_date;
	int				v_as400_batch_time;
	int				v_row_count;


	// Initialize variables.
	v_as400_batch_date	= download_start_date;
	v_as400_batch_time	= download_start_time;


	retcode			= ALL_RETRIES_WERE_USED;
	stop_retrying	= false;


	while (1)
	{
		ExecSQL (	MAIN_DB, AS400SRV_T4009PP_READ_interaction_type_stale_count,
					&v_row_count,			&v_as400_batch_date,
					&v_as400_batch_time,	END_OF_ARG_LIST							);

		if (SQLERR_error_test ())
		{
			retcode = SQL_ERROR_OCCURED;
			break;
		}
		else
		{
			if (v_row_count == 0)
			{
				retcode = MSG_HANDLER_SUCCEEDED;
				break;
			}
		}

		ExecSQL (	MAIN_DB, AS400SRV_T4009PP_DEL_interaction_type,
					&v_as400_batch_date, &v_as400_batch_time, END_OF_ARG_LIST	);

		HANDLE_DELETE_SQL_CODE ("Postprocess 4009", p_connect, &retcode, &db_changed_flg);

		break;
	}

	rows_deleted = v_row_count;


	// DonR 13Nov2018 CR #15919: Record current table size.
	REMEMBER_ISOLATION;
	SET_ISOLATION_DIRTY;

	ExecSQL (	MAIN_DB, AS400SRV_T4009PP_READ_interaction_type_table_size,
				&total_rows, END_OF_ARG_LIST									);

	active_rows = total_rows;

	RESTORE_ISOLATION;
	// DonR 13Nov2018 CR #15919 end.


	GerrLogMini (GerrId,
				 "Postprocess 4009: %d rows deleted, retcode = %d.",
				 rows_deleted, retcode);

	return retcode;

}	// End of msg 4009 postprocessor.


/* << ------------------------------------------------------------ >> */
/*                                                                    */
/*     handler for message 2011/4011 - upd/ins to DRUG_INTERACTION	  */
/*                                                                    */
/* << ------------------------------------------------------------ >> */
static int message_2011_handler (TConnection	*p_connect,
								 int			data_len)
{

/* << -------------------------------------------------------- >> */
/*			  local variables			  */
/* << -------------------------------------------------------- >> */
  char          *PosInBuff;		/* current pos in data buffer	    */
  int		ReadInput;		/* amount of Bytesread from buffer  */
  int		retcode;		/* code returned from function	    */
  bool		stop_retrying;		/* if true - return from function   */
  bool		do_insert = ATTEMPT_UPDATE_FIRST;
  int		n_retries;		/* retrying counter		    */
  int		i;


	/* << -------------------------------------------------------- >> */
	/*			    DB variables			  */
	/* << -------------------------------------------------------- >> */
	int		v_first_largo_code;
	int		v_second_largo_code;
	short	v_interaction_type;
	char	v_dur_message1			[40 + 1];
	char	v_dur_message2			[40 + 1];
	int		sysdate;
	int		systime;
	int		v_as400_batch_date;
	int		v_as400_batch_time;


  sysdate = GetDate();
  systime = GetTime();
	v_as400_batch_date = (download_start_date > 0) ? download_start_date : sysdate;
	v_as400_batch_time = (download_start_time > 0) ? download_start_time : systime;


/* << -------------------------------------------------------- >> */
/*			read input buffer			  */
/* << -------------------------------------------------------- >> */
  INIT_ERROR_DETECTION_MECHANISM;
  PosInBuff = glbMsgBuffer;


  v_first_largo_code	=  get_long(
				    &PosInBuff,
				    9
				    );	//Marianna 14Jul2024 User Story #330877
  RETURN_ON_ERROR;

  v_second_largo_code	=  get_long(
				    &PosInBuff,
				    9
				    );	//Marianna 14Jul2024 User Story #330877
  RETURN_ON_ERROR;

  v_interaction_type	=  get_short(
				     &PosInBuff,
				     Tinteraction_type_len
				     );

  			   get_string(
				      &PosInBuff,
				      v_dur_message1,
				      Tdur_message_len
				      );
  switch_to_win_heb( ( unsigned char * )v_dur_message1 );

  			   get_string(
				      &PosInBuff,
				      v_dur_message2,
				      Tdur_message_len
				      );
  switch_to_win_heb( ( unsigned char * )v_dur_message2 );

  RETURN_ON_ERROR;


/* << -------------------------------------------------------- >> */
/*			    debug messages			  */
/* << -------------------------------------------------------- >> */
  /************************  COMMENTED OUT: ***************************
  printf( "Tfirst_largo_code == %d\n", 	v_first_largo_code );
  printf( "Tsecond_largo_code == %d\n",	v_second_largo_code );
  printf( "Tinteraction_type == %d\n",		v_interaction_type );
  fflush( stdout );
  *********************************************************************/


  /*
   *   verify all data was read:
   */
  ReadInput = PosInBuff - glbMsgBuffer;
  StaticAssert( ReadInput == data_len );


/* << -------------------------------------------------------- >> */
/*	   up to SQL_UPDATE_RETRIES retries will be made	  */
/*		    to overcome access conflict			  */
/* << -------------------------------------------------------- >> */
  retcode = ALL_RETRIES_WERE_USED;
  stop_retrying = false;

  for(
      n_retries = 0;
      ( n_retries < SQL_UPDATE_RETRIES ) && ( stop_retrying == false );
      n_retries++
      )
    {

      /*
       * sleep for a while:
       */
      if( n_retries )
      {
	  sleep( SLEEP_TIME_BETWEEN_RETRIES );
      }

      /*
       *  try to insert/update new rec with the order depending on "do_insert":
       */
      for( i = 0;  i < 2;  i++ )
	{
	  if( do_insert == ATTEMPT_INSERT_FIRST )
	    {
			// DonR 16Jul2015: Set new in_drug_interact flag in drug_list.
			ExecSQL (	MAIN_DB, AS400SRV_T4011_UPD_drug_list,
						&v_first_largo_code, &v_second_largo_code, END_OF_ARG_LIST	);

			ExecSQL (	MAIN_DB, AS400SRV_T4011_INS_drug_interaction,
						&v_first_largo_code,	&v_second_largo_code,
						&sysdate,				&systime,
						&v_as400_batch_date,	&v_as400_batch_time,
						&v_interaction_type,	&ROW_NOT_DELETED,
						&v_dur_message1,		&v_dur_message2,
						END_OF_ARG_LIST									);

              HANDLE_INSERT_SQL_CODE( "4011",
					p_connect,
				      &do_insert,
				      &retcode,
				      &stop_retrying );

	  }
	  else
	  { 
			// DonR 16Jul2015: We shouldn't really have to update the in_drug_interact flag in
			// drug_list if we're only updating drug_interaction - but it can't hurt to be a
			// little bit paranoid!
			ExecSQL (	MAIN_DB, AS400SRV_T4011_UPD_drug_list,
						&v_first_largo_code, &v_second_largo_code, END_OF_ARG_LIST	);

			ExecSQL (	MAIN_DB, AS400SRV_T4011_UPD_drug_interaction,
						&sysdate,				&systime,
						&v_as400_batch_date,	&v_as400_batch_time,
						&v_interaction_type,	&v_dur_message1,
						&v_dur_message2,
						&v_first_largo_code,	&v_second_largo_code,
						END_OF_ARG_LIST									);

              HANDLE_UPDATE_SQL_CODE( "4011",
					p_connect,
				      &do_insert,
				      &retcode,
				      &stop_retrying );
	  }

	}/* for( i... */

    }/* for( n_retries... */

  if( n_retries == SQL_UPDATE_RETRIES )
  {
     WRITE_ACCESS_CONFLICT_TO_LOG( "2011" );
  }

  if (retcode == MSG_HANDLER_SUCCEEDED)
  {
	  if (do_insert == ATTEMPT_INSERT_FIRST)
		  rows_added++;
	  else
		  rows_updated++;
  }


  return retcode;
} /* end of msg 2011 handler */



/* << -------------------------------------------------------- >> */
/*                                                                */
/*  handler for message 2012 - delete rec from DRUG_INTERACTION	  */
/*																  */
/* << -------------------------------------------------------- >> */
static int message_2012_handler(
				TConnection	*p_connect,
				int		data_len
				)
{

/* << -------------------------------------------------------- >> */
/*			  local variables			  */
/* << -------------------------------------------------------- >> */
  char          *PosInBuff;		/* current pos in data buffer	    */
  int		ReadInput;		/* amount of Bytesread from buffer  */
  int		retcode;		/* code returned from function	    */
  int		n_retries;		/* retrying counter		    */
  int		i;
  int		db_changed_flg;


	/* << -------------------------------------------------------- >> */
	/*			    DB variables			  */
	/* << -------------------------------------------------------- >> */
	Tfirst_largo_code	v_first_largo_code;
	Tsecond_largo_code	v_second_largo_code;


/* << -------------------------------------------------------- >> */
/*			  read input buffer			  */
/* << -------------------------------------------------------- >> */
  INIT_ERROR_DETECTION_MECHANISM;
  PosInBuff = glbMsgBuffer;

  v_first_largo_code	=  get_long(
				    &PosInBuff,
				    9
				    );	//Marianna 14Jul2024 User Story #330877
  RETURN_ON_ERROR;

  v_second_largo_code	=  get_long(
				    &PosInBuff,
				    9
				    );	//Marianna 14Jul2024 User Story #330877
  RETURN_ON_ERROR;




/* << -------------------------------------------------------- >> */
/*			  debug messages			  */
/* << -------------------------------------------------------- >> */
  /************************* COMMENTED OUT:  **************************
  printf( "Tfirst_largo_code == %d\n", v_first_largo_code );
  printf( "Tsecond_largo_code == %d\n", v_second_largo_code );
  fflush( stdout );
  *********************************************************************/


/* << -------------------------------------------------------- >> */
/*		      verify all deta was read			  */
/* << -------------------------------------------------------- >> */
  ReadInput = PosInBuff - glbMsgBuffer;
  StaticAssert( ReadInput == data_len );


/* << -------------------------------------------------------- >> */
/*			   attempt delete			  */
/* << -------------------------------------------------------- >> */
  retcode = ALL_RETRIES_WERE_USED;
  db_changed_flg = 0;

  for( n_retries = 0;
       n_retries < SQL_UPDATE_RETRIES;
       n_retries++ )
    {

      /*
       * sleep for a while:
       */
      if( n_retries )
      {
	  sleep( SLEEP_TIME_BETWEEN_RETRIES );
      }

		ExecSQL (	MAIN_DB, AS400SRV_T2012_DEL_drug_interaction,
					&v_first_largo_code, &v_second_largo_code, END_OF_ARG_LIST	);

      HANDLE_DELETE_SQL_CODE( "2012", p_connect, &retcode, &db_changed_flg );

      break;
    }

  if( n_retries == SQL_UPDATE_RETRIES )
  {
     WRITE_ACCESS_CONFLICT_TO_LOG( "2012" );
  }

  if (retcode == MSG_HANDLER_SUCCEEDED)
	  rows_deleted++;


  return retcode;
} /* end of msg 2012 handler */


// -------------------------------------------------------------------------
//
//       Post-processor for message 4011 - delete DRUG_INTERACTION
//			rows that are no longer in the AS/400 full-table download.
//
// -------------------------------------------------------------------------
static int postprocess_4011 (TConnection *p_connect)
{
	// local variables
	bool	stop_retrying;	// if true - return from function
	int		rows_deleted	= 0;
	int		retcode;		// code returned from function
	int		n_retries;		// retrying counter
	int		db_changed_flg;

	// DB variables
	int				v_as400_batch_date;
	int				v_as400_batch_time;
	int				v_row_count;


	// Initialize variables.
	v_as400_batch_date	= download_start_date;
	v_as400_batch_time	= download_start_time;


	retcode			= ALL_RETRIES_WERE_USED;
	stop_retrying	= false;


	while (1)
	{
		ExecSQL (	MAIN_DB, AS400SRV_T4011PP_READ_drug_interaction_stale_count,
					&v_row_count,			&v_as400_batch_date,
					&v_as400_batch_time,	END_OF_ARG_LIST							);

		if (SQLERR_error_test ())
		{
			retcode = SQL_ERROR_OCCURED;
			break;
		}
		else
		{
			if (v_row_count == 0)
			{
				retcode = MSG_HANDLER_SUCCEEDED;
				break;
			}
		}

		ExecSQL (	MAIN_DB, AS400SRV_T4011PP_DEL_drug_interaction,
					&v_as400_batch_date, &v_as400_batch_time, END_OF_ARG_LIST	);

		HANDLE_DELETE_SQL_CODE ("Postprocess 4011", p_connect, &retcode, &db_changed_flg);

		break;
	}

	rows_deleted = v_row_count;

	// DonR 16Jul2015: Clear the new drug_list in_drug_interact flag for drugs that are no longer
	// found in drug_interaction.
	ExecSQL (	MAIN_DB, AS400SRV_T4011PP_UPD_drug_list, END_OF_ARG_LIST	);

	// DonR 13Nov2018 CR #15919: Record current table size.
	REMEMBER_ISOLATION;
	SET_ISOLATION_DIRTY;

	ExecSQL (	MAIN_DB, AS400SRV_T4011PP_READ_drug_interaction_table_size,
				&total_rows, END_OF_ARG_LIST									);

	active_rows = total_rows;

	RESTORE_ISOLATION;
	// DonR 13Nov2018 CR #15919 end.


	GerrLogMini (GerrId,
				 "Postprocess 4011: %d rows deleted, retcode = %d.",
				 rows_deleted, retcode);

	return retcode;

}	// End of msg 4011 postprocessor.



/* << ------------------------------------------------------------ >> */
/*                                                                    */
/*        handler for message 2013/4013 - upd/ins to DOCTORS		  */
/*                                                                    */
/* << ------------------------------------------------------------ >> */
static int data_2013_handler (TConnection	*p_connect,
							  int			data_len)
{

/* << -------------------------------------------------------- >> */
/*			  local variables			  */
/* << -------------------------------------------------------- >> */
  char      *PosInBuff;		/* current pos in data buffer	    */
  int		ReadInput;		/* amount of Bytesread from buffer  */
  int		retcode;		/* code returned from function	    */
  bool		stop_retrying;		/* if true - return from function   */
  bool		do_insert = ATTEMPT_UPDATE_FIRST;
  int		n_retries;		/* retrying counter		    */
  int		i;


	/* << -------------------------------------------------------- >> */
	/*			    DB variables			  */
	/* << -------------------------------------------------------- >> */
	Tdoctor_id		v_doctor_id;
	Tfirst_name		v_first_name;
	Tlast_name		v_last_name ;
	Tstreet_and_no	v_street_and_no;
	Tcity			v_city;
	Tphone			v_phone;
	int				v_DoctorLicense;
	char			v_DocContactPhone	[40 + 1];
	short			profession_type;
	short			LastProfessionType	= -1;		// So we know if we need to do a PrescriberProfession lookup.
	short			ProfCodeToPharmacy;				// From PrescriberProfession table.
	char			ProfDescription		[25 + 1];	// From PrescriberProfession table.
	int		 		sysdate;
	int				systime;
	int				v_as400_batch_date;
	int				v_as400_batch_time;


	 sysdate = GetDate();
	 systime = GetTime();
	v_as400_batch_date = (download_start_date > 0) ? download_start_date : sysdate;
	v_as400_batch_time = (download_start_time > 0) ? download_start_time : systime;



/* << -------------------------------------------------------- >> */
/*			read input buffer			  */
/* << -------------------------------------------------------- >> */
  INIT_ERROR_DETECTION_MECHANISM;
  PosInBuff = glbMsgBuffer;


	v_doctor_id				=	get_long	(&PosInBuff,					Tdoctor_id_len		);	RETURN_ON_ERROR;
	/* v_first_name			*/	get_string	(&PosInBuff, v_first_name,		Tfirst_name_len		);
	/* v_last_name			*/	get_string	(&PosInBuff, v_last_name,		Tlast_name_len		);
	/* v_street_and_no		*/	get_string	(&PosInBuff, v_street_and_no,	Tstreet_and_no_len	);
	/* v_city				*/	get_string	(&PosInBuff, v_city,			Tcity_len			);
	/* v_phone				*/	get_string	(&PosInBuff, v_phone,			Tphone_len			);
	v_DoctorLicense			=	get_long	(&PosInBuff,					8					);
	/* v_DocContactPhone	*/	get_string	(&PosInBuff, v_DocContactPhone,	40					);
	profession_type			=	get_short	(&PosInBuff,					2					);	RETURN_ON_ERROR;

	switch_to_win_heb( ( unsigned char * ) v_first_name		);
	switch_to_win_heb( ( unsigned char * ) v_last_name		);
	switch_to_win_heb( ( unsigned char * ) v_street_and_no	);
	switch_to_win_heb( ( unsigned char * ) v_city			);


/* << -------------------------------------------------------- >> */
/*			    debug messages			  */
/* << -------------------------------------------------------- >> */
  /************************* COMMENTED OUT:  **************************
  printf( "Tdoctor_id == %d\n", 	v_doctor_id );
  printf( "Tfirst_name == %s\n", 	v_first_name );
  printf( "Tlast_name == %s\n", 	v_last_name );
  printf( "Tstreet_and_no == %s\n", 	v_street_and_no );
  printf( "Tcity == %s\n", 		v_city );
  printf( "Tphone == %s\n", 		v_phone );
  fflush( stdout );
  ********************************************************************/

  /*
   *   verify all data was read:
   */
  ReadInput = PosInBuff - glbMsgBuffer;
  StaticAssert( ReadInput == data_len );


/* << -------------------------------------------------------- >> */
/*	   up to SQL_UPDATE_RETRIES retries will be made	  */
/*		    to overcome access conflict			  */
/* << -------------------------------------------------------- >> */
  retcode = ALL_RETRIES_WERE_USED;
  stop_retrying = false;

  for(
      n_retries = 0;
      ( n_retries < SQL_UPDATE_RETRIES ) && ( stop_retrying == false );
      n_retries++
      )
    {

		// sleep for a while:
		if( n_retries )
		{
			sleep( SLEEP_TIME_BETWEEN_RETRIES );
		}


		// DonR 10Dec2025 User Story #441076: Get profession-to-pharmacy and profession description
		// from new table PrescriberProfession. It would probably be possible to do this with fancy
		// SQL code in the INSERT and UPDATE queries, but just this once I'll do things the simple way.
		// OTOH there's no point in doing the same lookup twice in a row, so keep track of the last
		// profession type we read from the table.
		if (profession_type != LastProfessionType)
		{
			LastProfessionType = profession_type;

			ExecSQL (	MAIN_DB, AS400SRV_T4013_GetProfessionData,
						&ProfCodeToPharmacy,	&ProfDescription,
						&profession_type,		END_OF_ARG_LIST		);

			if (SQLCODE)
			{
				ProfCodeToPharmacy = 0;
				strcpy (ProfDescription, "");

			    if (SQLERR_code_cmp (SQLERR_not_found) == MAC_TRUE)
				{
					GerrLogMini (GerrId, "AS400SRV_T4013_GetProfessionData couldn't find anything for profession type %d.", profession_type);
				}
				else
				{
					SQLERR_error_test ();
				}
			}
		}	// New profession type to look up.


      /*
       *  try to insert/update new rec with the order depending on "do_insert":
       */
      for( i = 0;  i < 2;  i++ )
	{
	  if( do_insert == ATTEMPT_INSERT_FIRST )
	    {
			ExecSQL (	MAIN_DB, AS400SRV_T4013_INS_doctors,
						&v_doctor_id,			&v_DoctorLicense,		&v_first_name,			&v_last_name,
						&v_street_and_no,		&v_city,				&v_phone,				&v_DocContactPhone,
						&profession_type,		&ProfCodeToPharmacy,	&ProfDescription,
						&sysdate,				&systime,				&v_as400_batch_date,	&v_as400_batch_time,
						&ROW_NOT_DELETED,		END_OF_ARG_LIST			);

              HANDLE_INSERT_SQL_CODE( "2013",
					p_connect,
				      &do_insert,
				      &retcode,
				      &stop_retrying );

	    }
	  else
	    {
			ExecSQL (	MAIN_DB, AS400SRV_T4013_UPD_doctors,
						&v_DoctorLicense,		&v_first_name,			&v_last_name,
						&v_street_and_no,		&v_city,				&v_phone,
						&v_DocContactPhone,		&profession_type,		&ProfCodeToPharmacy,
						&ProfDescription,
						&v_as400_batch_date,	&v_as400_batch_time,
						&sysdate,				&systime,
						&v_doctor_id,			END_OF_ARG_LIST			);

              HANDLE_UPDATE_SQL_CODE( "2013",
					p_connect,
				      &do_insert,
				      &retcode,
				      &stop_retrying );
	    }

	}/* for( i... */

    }/* for( n_retries... */

  if( n_retries == SQL_UPDATE_RETRIES )
  {
     WRITE_ACCESS_CONFLICT_TO_LOG( "2013" );
  }

  if (retcode == MSG_HANDLER_SUCCEEDED)
  {
	  if (do_insert == ATTEMPT_INSERT_FIRST)
		  rows_added++;
	  else
		  rows_updated++;
  }


  return retcode;
} /* end of msg 2013 handler */


/* << -------------------------------------------------------- >> */
/*                                                                */
/*     handler for message 2014 - delete rec from DOCTORS	  */
/*                                                                */
/* << -------------------------------------------------------- >> */
static int message_2014_handler(
				TConnection	*p_connect,
				int		data_len
				)
{

/* << -------------------------------------------------------- >> */
/*			  local variables			  */
/* << -------------------------------------------------------- >> */
  char          *PosInBuff;		/* current pos in data buffer	    */
  int		ReadInput;		/* amount of Bytesread from buffer  */
  int		retcode;		/* code returned from function	    */
  int		n_retries;		/* retrying counter		    */
  int		i;
  int		db_changed_flg;


	/* << -------------------------------------------------------- >> */
	/*			    DB variables			  */
	/* << -------------------------------------------------------- >> */
	Tdoctor_id	v_doctor_id;


/* << -------------------------------------------------------- >> */
/*			  read input buffer			  */
/* << -------------------------------------------------------- >> */
  INIT_ERROR_DETECTION_MECHANISM;
  PosInBuff = glbMsgBuffer;
//GerrLogReturn(GerrId," 2014 [%s]",glbMsgBuffer);fflush(stdout);

  v_doctor_id	=  get_long(
			    &PosInBuff,
			    Tdoctor_id_len
			    );
  RETURN_ON_ERROR;

/* << -------------------------------------------------------- >> */
/*			  debug messages			  */
/* << -------------------------------------------------------- >> */
  /************************* COMMENTED OUT: **************************
  printf( "doctor_id == %d\n", v_doctor_id );
  fflush( stdout );
  ********************************************************************/


/* << -------------------------------------------------------- >> */
/*		      verify all deta was read			  */
/* << -------------------------------------------------------- >> */
  ReadInput = PosInBuff - glbMsgBuffer;
  StaticAssert( ReadInput == data_len );


/* << -------------------------------------------------------- >> */
/*			   attempt delete			  */
/* << -------------------------------------------------------- >> */
  retcode = ALL_RETRIES_WERE_USED;
  db_changed_flg = 0;

  for( n_retries = 0;
       n_retries < SQL_UPDATE_RETRIES;
       n_retries++ )
    {

      /*
       * sleep for a while:
       */
      if( n_retries )
      {
	  sleep( SLEEP_TIME_BETWEEN_RETRIES );
      }

		ExecSQL (	MAIN_DB, AS400SRV_T2014_DEL_doctors,
					&v_doctor_id, END_OF_ARG_LIST			);

      HANDLE_DELETE_SQL_CODE( "2014", p_connect, &retcode, &db_changed_flg );

      break;
    }

  if( n_retries == SQL_UPDATE_RETRIES )
  {
     WRITE_ACCESS_CONFLICT_TO_LOG( "2014" );
  }

  if (retcode == MSG_HANDLER_SUCCEEDED)
	  rows_deleted++;


  return retcode;
} /* end of msg 2014 handler */


// -------------------------------------------------------------------------
//
//       Post-processor for message 4013 - delete DOCTORS
//			rows that are no longer in the AS/400 full-table download.
//
// -------------------------------------------------------------------------
static int postprocess_4013 (TConnection *p_connect)
{
	// local variables
	bool	stop_retrying;	// if true - return from function
	int		rows_deleted	= 0;
	int		retcode;		// code returned from function
	int		n_retries;		// retrying counter
	int		db_changed_flg;

	// DB variables
	int				v_as400_batch_date;
	int				v_as400_batch_time;
	int				v_row_count;


	// Initialize variables.
	v_as400_batch_date	= download_start_date;
	v_as400_batch_time	= download_start_time;


	retcode			= ALL_RETRIES_WERE_USED;
	stop_retrying	= false;


	while (1)
	{
		ExecSQL (	MAIN_DB, AS400SRV_T4013PP_READ_doctors_stale_count,
					&v_row_count,			&v_as400_batch_date,
					&v_as400_batch_time,	END_OF_ARG_LIST				);

		if (SQLERR_error_test ())
		{
			retcode = SQL_ERROR_OCCURED;
			break;
		}
		else
		{
			if (v_row_count == 0)
			{
				retcode = MSG_HANDLER_SUCCEEDED;
				break;
			}
		}

		ExecSQL (	MAIN_DB, AS400SRV_T4013PP_DEL_doctors,
					&v_as400_batch_date, &v_as400_batch_time, END_OF_ARG_LIST	);

		HANDLE_DELETE_SQL_CODE ("Postprocess 4013", p_connect, &retcode, &db_changed_flg);

		break;
	}

	rows_deleted = v_row_count;


	// DonR 13Nov2018 CR #15919: Record current table size.
	REMEMBER_ISOLATION;
	SET_ISOLATION_DIRTY;

	ExecSQL (	MAIN_DB, AS400SRV_T4013PP_READ_doctors_table_size,
				&total_rows, END_OF_ARG_LIST						);

	active_rows = total_rows;

	RESTORE_ISOLATION;
	// DonR 13Nov2018 CR #15919 end.


	GerrLogMini (GerrId,
				 "Postprocess 4013: %d rows deleted, retcode = %d.",
				 rows_deleted, retcode);

	return retcode;

}	// End of msg 4013 postprocessor.



/* << -------------------------------------------------------- >> */
/*                                                                */
/*   handler for message 2015 - upd/ins to SPECIALITY_TYPES	  */
/*                                                                */
/* << -------------------------------------------------------- >> */
static int message_2015_handler(
				TConnection	*p_connect,
				int		data_len,
				bool		do_insert
				)
{

/* << -------------------------------------------------------- >> */
/*			  local variables			  */
/* << -------------------------------------------------------- >> */
  char		*PosInBuff;		/* current pos in data buffer	    */
  int		ReadInput;		/* amount of Bytesread from buffer  */
  int		retcode;		/* code returned from function	    */
  bool		stop_retrying;		/* if true - return from function   */
  int		n_retries;		/* retrying counter		    */
  int		i;


	/* << -------------------------------------------------------- >> */
	/*			    DB variables			  */
	/* << -------------------------------------------------------- >> */
	Tspeciality_code	v_speciality_code;
	Tdescription		v_description;
	int					sysdate,
						systime;
	int					v_as400_batch_date;
	int					v_as400_batch_time;


 sysdate = GetDate();
 systime = GetTime();
	v_as400_batch_date = (download_start_date > 0) ? download_start_date : sysdate;
	v_as400_batch_time = (download_start_time > 0) ? download_start_time : systime;

/* << -------------------------------------------------------- >> */
/*			read input buffer			  */
/* << -------------------------------------------------------- >> */
  INIT_ERROR_DETECTION_MECHANISM;
  PosInBuff = glbMsgBuffer;


  v_speciality_code	=  get_long(
				    &PosInBuff,
				    Tspeciality_code_len
				    );
  RETURN_ON_ERROR;

			   get_string(
				      &PosInBuff,
				      v_description,
				      Tdescription_len
				      );
  switch_to_win_heb( ( unsigned char * )v_description );



/* << -------------------------------------------------------- >> */
/*			    debug messages			  */
/* << -------------------------------------------------------- >> */
  /*************************  COMMENTED OUT:  ************************
  printf( "speciality_code == %d\n", 	v_speciality_code );
  printf( "description == %s\n", 	v_description );
  fflush( stdout );
  ********************************************************************/

  /*
   *   verify all data was read:
   */
  ReadInput = PosInBuff - glbMsgBuffer;
  StaticAssert( ReadInput == data_len );


/* << -------------------------------------------------------- >> */
/*	   up to SQL_UPDATE_RETRIES retries will be made	  */
/*		    to overcome access conflict			  */
/* << -------------------------------------------------------- >> */
  retcode = ALL_RETRIES_WERE_USED;
  stop_retrying = false;

  for(
      n_retries = 0;
      ( n_retries < SQL_UPDATE_RETRIES ) && ( stop_retrying == false );
      n_retries++
      )
    {

      /*
       * sleep for a while:
       */
      if( n_retries )
      {
	  sleep( SLEEP_TIME_BETWEEN_RETRIES );
      }


      /*
       *  try to insert/update new rec with the order depending on "do_insert":
       */
      for( i = 0;  i < 2;  i++ )
	{
	  if( do_insert == ATTEMPT_INSERT_FIRST )
	    {
			ExecSQL (	MAIN_DB, AS400SRV_T4015_INS_speciality_types,
						&v_speciality_code,		&v_description,
						&sysdate,				&systime,
						&v_as400_batch_date,	&v_as400_batch_time,
						&ROW_NOT_DELETED,		END_OF_ARG_LIST			);

              HANDLE_INSERT_SQL_CODE( "2015",
					p_connect,
				      &do_insert,
				      &retcode,
				      &stop_retrying );

	    }
	  else
	    {
			ExecSQL (	MAIN_DB, AS400SRV_T4015_UPD_speciality_types,
						&v_description,			&v_as400_batch_date,
						&v_as400_batch_time,	&sysdate,
						&systime,
						&v_speciality_code,		END_OF_ARG_LIST			);

              HANDLE_UPDATE_SQL_CODE( "2015",
					p_connect,
				      &do_insert,
				      &retcode,
				      &stop_retrying );
	    }

	}/* for( i... */

    }/* for( n_retries... */

  if( n_retries == SQL_UPDATE_RETRIES )
  {
     WRITE_ACCESS_CONFLICT_TO_LOG( "2015" );
  }

  if (retcode == MSG_HANDLER_SUCCEEDED)
  {
	  if (do_insert == ATTEMPT_INSERT_FIRST)
		  rows_added++;
	  else
		  rows_updated++;
  }


  return retcode;
} /* end of msg 2015 handler */


/* << -------------------------------------------------------- >> */
/*                                                                */
/*  handler for message 2016 - delete rec from SPECIALITY_TYPES	  */
/*                                                                */
/* << -------------------------------------------------------- >> */
static int message_2016_handler(
				TConnection	*p_connect,
				int		data_len
				)
{

/* << -------------------------------------------------------- >> */
/*			  local variables			  */
/* << -------------------------------------------------------- >> */
  char		*PosInBuff;		/* current pos in data buffer	    */
  int		ReadInput;		/* amount of Bytesread from buffer  */
  int		retcode;		/* code returned from function	    */
  int		n_retries;		/* retrying counter		    */
  int		i;
  int		db_changed_flg;


	/* << -------------------------------------------------------- >> */
	/*			    DB variables			  */
	/* << -------------------------------------------------------- >> */
	Tspeciality_code	v_speciality_code;


/* << -------------------------------------------------------- >> */
/*			  read input buffer			  */
/* << -------------------------------------------------------- >> */
  INIT_ERROR_DETECTION_MECHANISM;
  PosInBuff = glbMsgBuffer;

  v_speciality_code	=  get_long(
				    &PosInBuff,
				    Tspeciality_code_len
				    );
  RETURN_ON_ERROR;

/* << -------------------------------------------------------- >> */
/*			  debug messages			  */
/* << -------------------------------------------------------- >> */
  /*************************  COMMENTED OUT:  ************************
  printf( "speciality_code == %d\n", v_speciality_code );
  fflush( stdout );
  ********************************************************************/


/* << -------------------------------------------------------- >> */
/*		      verify all deta was read			  */
/* << -------------------------------------------------------- >> */
  ReadInput = PosInBuff - glbMsgBuffer;
  StaticAssert( ReadInput == data_len );


/* << -------------------------------------------------------- >> */
/*			   attempt delete			  */
/* << -------------------------------------------------------- >> */
  retcode = ALL_RETRIES_WERE_USED;
  db_changed_flg = 0;

  for( n_retries = 0;
       n_retries < SQL_UPDATE_RETRIES;
       n_retries++ )
    {

      /*
       * sleep for a while:
       */
      if( n_retries )
      {
	  sleep( SLEEP_TIME_BETWEEN_RETRIES );
      }

		ExecSQL (	MAIN_DB, AS400SRV_T2016_DEL_speciality_types,
					&v_speciality_code, END_OF_ARG_LIST				);

      HANDLE_DELETE_SQL_CODE( "2016", p_connect, &retcode, &db_changed_flg );

      break;
    }

  if( n_retries == SQL_UPDATE_RETRIES )
  {
     WRITE_ACCESS_CONFLICT_TO_LOG( "2016" );
  }

  if (retcode == MSG_HANDLER_SUCCEEDED)
	  rows_deleted++;


  return retcode;
} /* end of msg 2016 handler */


// -------------------------------------------------------------------------
//
//       Post-processor for message 4015 - delete SPECIALITY_TYPES
//			rows that are no longer in the AS/400 full-table download.
//
// -------------------------------------------------------------------------
static int postprocess_4015 (TConnection *p_connect)
{
	// local variables
	bool	stop_retrying;	// if true - return from function
	int		rows_deleted	= 0;
	int		retcode;		// code returned from function
	int		n_retries;		// retrying counter
	int		db_changed_flg;

	// DB variables
	int				v_as400_batch_date;
	int				v_as400_batch_time;
	int				v_row_count;


	// Initialize variables.
	v_as400_batch_date	= download_start_date;
	v_as400_batch_time	= download_start_time;


	retcode			= ALL_RETRIES_WERE_USED;
	stop_retrying	= false;


	while (1)
	{
		ExecSQL (	MAIN_DB, AS400SRV_T4015PP_READ_speciality_types_stale_count,
					&v_row_count,			&v_as400_batch_date,
					&v_as400_batch_time,	END_OF_ARG_LIST							);

		if (SQLERR_error_test ())
		{
			retcode = SQL_ERROR_OCCURED;
			break;
		}
		else
		{
			if (v_row_count == 0)
			{
				retcode = MSG_HANDLER_SUCCEEDED;
				break;
			}
		}

		ExecSQL (	MAIN_DB, AS400SRV_T4015PP_DEL_speciality_types,
					&v_as400_batch_date, &v_as400_batch_time, END_OF_ARG_LIST	);

		HANDLE_DELETE_SQL_CODE ("Postprocess 4015", p_connect, &retcode, &db_changed_flg);

		break;
	}

	rows_deleted = v_row_count;


	// DonR 13Nov2018 CR #15919: Record current table size.
	REMEMBER_ISOLATION;
	SET_ISOLATION_DIRTY;

	ExecSQL (	MAIN_DB, AS400SRV_T4015PP_READ_speciality_types_table_size,
				&total_rows, END_OF_ARG_LIST								);

	active_rows = total_rows;

	RESTORE_ISOLATION;
	// DonR 13Nov2018 CR #15919 end.


	GerrLogMini (GerrId,
				 "Postprocess 4015: %d rows deleted, retcode = %d.",
				 rows_deleted, retcode);

	return retcode;

}	// End of msg 4015 postprocessor.



/* << ------------------------------------------------------------ >> */
/*                                                                    */
/*   handler for message 2017/4017 - upd/ins to DOCTOR_SPECIALITY	  */
/*                                                                    */
/* << ------------------------------------------------------------ >> */
static int data_2017_handler (TConnection	*p_connect,
							  int			data_len)
{

/* << -------------------------------------------------------- >> */
/*			  local variables			  */
/* << -------------------------------------------------------- >> */
  char      *PosInBuff;		/* current pos in data buffer	    */
  int		ReadInput;		/* amount of Bytesread from buffer  */
  int		retcode;		/* code returned from function	    */
  bool		stop_retrying;		/* if true - return from function   */
  bool		do_insert = ATTEMPT_UPDATE_FIRST;
  int		n_retries;		/* retrying counter		    */
  int		i;


	/* << -------------------------------------------------------- >> */
	/*			    DB variables			  */
	/* << -------------------------------------------------------- >> */
	Tdoctor_id		v_doctor_id;
	int				v_speciality_code;
	int				sysdate,
					systime;
	int				v_as400_batch_date;
	int				v_as400_batch_time;


  sysdate = GetDate();
  systime = GetTime();
	v_as400_batch_date = (download_start_date > 0) ? download_start_date : sysdate;
	v_as400_batch_time = (download_start_time > 0) ? download_start_time : systime;

/* << -------------------------------------------------------- >> */
/*			read input buffer			  */
/* << -------------------------------------------------------- >> */
  INIT_ERROR_DETECTION_MECHANISM;
  PosInBuff = glbMsgBuffer;


  v_doctor_id		=  get_long(
				    &PosInBuff,
				    Tdoctor_id_len
				    );
  RETURN_ON_ERROR;

  v_speciality_code	=  get_long(
				    &PosInBuff,
				    Tspeciality_code_len
				    );
  RETURN_ON_ERROR;


/* << -------------------------------------------------------- >> */
/*			    debug messages			  */
/* << -------------------------------------------------------- >> */
  /*************************  COMMENTED OUT:  **************************
  printf( "doctor_id == %d\n",		v_doctor_id );
  printf( "speciality_code == %d\n", 	v_speciality_code );
  fflush( stdout );
  **********************************************************************/

  /*
   *   verify all data was read:
   */
  ReadInput = PosInBuff - glbMsgBuffer;
  StaticAssert( ReadInput == data_len );


/* << -------------------------------------------------------- >> */
/*	   up to SQL_UPDATE_RETRIES retries will be made	  */
/*		    to overcome access conflict			  */
/* << -------------------------------------------------------- >> */
  retcode = ALL_RETRIES_WERE_USED;
  stop_retrying = false;

  for(
      n_retries = 0;
      ( n_retries < SQL_UPDATE_RETRIES ) && ( stop_retrying == false );
      n_retries++
      )
    {

      /*
       * sleep for a while:
       */
      if( n_retries )
      {
	  sleep( SLEEP_TIME_BETWEEN_RETRIES );
      }


      /*
       *  try to insert/update new rec with the order depending on "do_insert":
       */
      for( i = 0;  i < 2;  i++ )
	{
	  if( do_insert == ATTEMPT_INSERT_FIRST )
	    {
			ExecSQL (	MAIN_DB, AS400SRV_T4017_INS_doctor_speciality,
						&v_doctor_id,			&v_speciality_code,
						&sysdate,				&systime,
						&v_as400_batch_date,	&v_as400_batch_time,
						&ROW_NOT_DELETED,		END_OF_ARG_LIST			);

              HANDLE_INSERT_SQL_CODE( "2017",
					p_connect,
				      &do_insert,
				      &retcode,
				      &stop_retrying );
	    }
	  else
	    {
			ExecSQL (	MAIN_DB, AS400SRV_T4017_UPD_doctor_speciality,
						&v_as400_batch_date,	&v_as400_batch_time,
						&sysdate,				&systime,
						&v_doctor_id,			&v_speciality_code,
						END_OF_ARG_LIST									);

              HANDLE_UPDATE_SQL_CODE( "2017",
					p_connect,
				      &do_insert,
				      &retcode,
				      &stop_retrying );
	    }

	}/* for( i... */

    }/* for( n_retries... */

  if( n_retries == SQL_UPDATE_RETRIES )
  {
     WRITE_ACCESS_CONFLICT_TO_LOG( "2017" );
  }

  if (retcode == MSG_HANDLER_SUCCEEDED)
  {
	  if (do_insert == ATTEMPT_INSERT_FIRST)
		  rows_added++;
	  else
		  rows_updated++;
  }


  return retcode;
} /* end of msg 2017 handler */


/* << -------------------------------------------------------- >> */
/*                                                                */
/*  handler for message 2018 - delete rec from DOCTOR_SPECIALITY  */
/*                                                                */
/* << -------------------------------------------------------- >> */
static int message_2018_handler(
				TConnection	*p_connect,
				int		data_len
				)
{

/* << -------------------------------------------------------- >> */
/*			  local variables			  */
/* << -------------------------------------------------------- >> */
  char		*PosInBuff;		/* current pos in data buffer	    */
  int		ReadInput;		/* amount of Bytesread from buffer  */
  int		retcode;		/* code returned from function	    */
  int		n_retries;		/* retrying counter		    */
  int		i;
  int		db_changed_flg;


	/* << -------------------------------------------------------- >> */
	/*			    DB variables			  */
	/* << -------------------------------------------------------- >> */
	Tdoctor_id			v_doctor_id;
	Tspeciality_code	v_speciality_code;


/* << -------------------------------------------------------- >> */
/*			  read input buffer			  */
/* << -------------------------------------------------------- >> */
  INIT_ERROR_DETECTION_MECHANISM;
  PosInBuff = glbMsgBuffer;

  v_doctor_id		=  get_long(
				    &PosInBuff,
				    Tdoctor_id_len
				    );
  RETURN_ON_ERROR;

  v_speciality_code	=  get_long(
				    &PosInBuff,
				    Tspeciality_code_len
				    );
  RETURN_ON_ERROR;

/* << -------------------------------------------------------- >> */
/*			  debug messages			  */
/* << -------------------------------------------------------- >> */
  /*************************  COMMENTED OUT:  **************************
  printf( "doctor_id == %d\n",		v_doctor_id );
  printf( "speciality_code == %d\n", 	v_speciality_code );
  fflush( stdout );
  *********************************************************************/


/* << -------------------------------------------------------- >> */
/*		      verify all deta was read			  */
/* << -------------------------------------------------------- >> */
  ReadInput = PosInBuff - glbMsgBuffer;
  StaticAssert( ReadInput == data_len );


/* << -------------------------------------------------------- >> */
/*			   attempt delete			  */
/* << -------------------------------------------------------- >> */
  retcode = ALL_RETRIES_WERE_USED;
  db_changed_flg = 0;

  for( n_retries = 0;
       n_retries < SQL_UPDATE_RETRIES;
       n_retries++ )
    {

      /*
       * sleep for a while:
       */
      if( n_retries )
      {
	  sleep( SLEEP_TIME_BETWEEN_RETRIES );
      }

		ExecSQL (	MAIN_DB, AS400SRV_T2018_DEL_doctor_speciality,
					&v_doctor_id, &v_speciality_code, END_OF_ARG_LIST	);

      HANDLE_DELETE_SQL_CODE( "2018", p_connect, &retcode, &db_changed_flg );

      break;
    }

  if( n_retries == SQL_UPDATE_RETRIES )
  {
     WRITE_ACCESS_CONFLICT_TO_LOG( "2018" );
  }

  if (retcode == MSG_HANDLER_SUCCEEDED)
	  rows_deleted++;


  return retcode;
} /* end of msg 2018 handler */


// -------------------------------------------------------------------------
//
//       Post-processor for message 4017 - delete DOCTOR_SPECIALITY
//			rows that are no longer in the AS/400 full-table download.
//
// -------------------------------------------------------------------------
static int postprocess_4017 (TConnection *p_connect)
{
	// local variables
	bool	stop_retrying;	// if true - return from function
	int		rows_deleted	= 0;
	int		retcode;		// code returned from function
	int		n_retries;		// retrying counter
	int		db_changed_flg;

	// DB variables
	int				v_as400_batch_date;
	int				v_as400_batch_time;
	int				v_row_count;


	// Initialize variables.
	v_as400_batch_date	= download_start_date;
	v_as400_batch_time	= download_start_time;


	retcode			= ALL_RETRIES_WERE_USED;
	stop_retrying	= false;


	while (1)
	{
		ExecSQL (	MAIN_DB, AS400SRV_T4017PP_READ_doctor_speciality_stale_count,
					&v_row_count,			&v_as400_batch_date,
					&v_as400_batch_time,	END_OF_ARG_LIST							);

		if (SQLERR_error_test ())
		{
			retcode = SQL_ERROR_OCCURED;
			break;
		}
		else
		{
			if (v_row_count == 0)
			{
				retcode = MSG_HANDLER_SUCCEEDED;
				break;
			}
		}

		ExecSQL (	MAIN_DB, AS400SRV_T4017PP_DEL_doctor_speciality,
					&v_as400_batch_date, &v_as400_batch_time, END_OF_ARG_LIST	);

		HANDLE_DELETE_SQL_CODE ("Postprocess 4017", p_connect, &retcode, &db_changed_flg);

		break;
	}

	rows_deleted = v_row_count;


	// DonR 13Nov2018 CR #15919: Record current table size.
	REMEMBER_ISOLATION;
	SET_ISOLATION_DIRTY;

	ExecSQL (	MAIN_DB, AS400SRV_T4017PP_READ_doctor_speciality_table_size,
				&total_rows, END_OF_ARG_LIST									);

	active_rows = total_rows;

	RESTORE_ISOLATION;
	// DonR 13Nov2018 CR #15919 end.


	GerrLogMini (GerrId,
				 "Postprocess 4017: %d rows deleted, retcode = %d.",
				 rows_deleted, retcode);

	return retcode;

}	// End of msg 4017 postprocessor.



// -------------------------------------------------------------------------
//
//       handler for message 2019/4019 - upd/ins to SPCLTY_LARGO_PRCNT
//
//            Performs detailed comparison to detect rows that have changed.
//
// -------------------------------------------------------------------------
static int data_4019_handler (TConnection	*p_connect,
							  int			data_len)
{
	// local variables
	char	*PosInBuff;		// current pos in data buffer
	bool	stop_retrying;	// if true - return from function
	bool	need_to_read;
	bool	something_changed	= 0;
	short	do_insert			= ATTEMPT_UPDATE_FIRST;
	int		ReadInput;		// amount of Bytesread from buffer
	int		retcode;		// code returned from function
	int		n_retries;		// retrying counter
	int		i;

	// DB variables.
	Tspeciality_code	v_speciality_code;
	Tlargo_code			v_largo_code;
	Tprice_code			v_basic_price_code;
	Tprice_code			v_prev_bas_prc_cod;	// DonR 05Jan2012: Not really in use.
	Tprice_code			v_zahav_price_code;
	Tprice_code			v_prev_zhv_prc_cod;	// DonR 05Jan2012: Not really in use.
	Tprice_code			v_kesef_price_code;
	Tprice_code			v_prev_ksf_prc_cod;	// DonR 05Jan2012: Not really in use.
	Tprice_code			v_lookup_price_code;
	int					v_fixed_price_basic;
	int					v_fixed_price_kesef;
	int					v_fixed_price_zahav;
	TGenericFlag		v_in_health_basket;
	short				v_enabled_status;
	short				v_enabled_mac;
	short				v_enabled_hova;	
	short				v_enabled_keva;
	short				v_ptn_percent_sort;
	short				v_ins_type_sort		= 0;
	char				v_insurance_type [2];
	short				v_yahalom_prc_code;
	int					v_fixed_pr_yahalom;
	short				v_wait_months;
	short				v_price_code;
	int					v_fixed_price;

	Tprice_code			o_basic_price_code;
	Tprice_code			o_zahav_price_code;
	Tprice_code			o_kesef_price_code;
	int					o_fixed_price_basic;
	int					o_fixed_price_kesef;
	int					o_fixed_price_zahav;
	TGenericFlag		o_in_health_basket;
	TGenericFlag		o_HealthBasketNew;
	short				o_yahalom_prc_code;
	int					o_fixed_pr_yahalom;
	short				o_wait_months;

	int					v_refresh_date;
	int					v_refresh_time;
	int					v_as400_batch_date;
	int					v_as400_batch_time;
	int					v_update_date;
	int					v_update_time;


	// Calculate virtual time. Note that for this download the time is based
	// on the real time, adjusted only to ensure that we don't have too many
	// rules with the exact same as-of time.
	v_refresh_date		=  (download_start_date > 0) ? download_start_date : GetDate();
	v_refresh_time		=  (download_start_time > 0) ? download_start_time : GetTime();
	v_as400_batch_date	=  v_refresh_date;
	v_as400_batch_time	=  v_refresh_time;
	v_refresh_time		+= (download_row_count++ / VIRTUAL_PORTION);


	// Read input buffer.
	INIT_ERROR_DETECTION_MECHANISM;
	PosInBuff = glbMsgBuffer;

	GET_LONG	(v_speciality_code			, Tspeciality_code_len	);
	GET_LONG	(v_largo_code				, 9						);	//Marianna 14Jul2024 User Story #330877
	GET_SHORT	(v_basic_price_code			, Tprice_code_len		);
	GET_SHORT	(v_prev_bas_prc_cod			, Tprice_code_len		);	// DonR 05Jan2012: Not really in use.
	GET_SHORT	(v_zahav_price_code			, Tprice_code_len		);
	GET_SHORT	(v_prev_zhv_prc_cod			, Tprice_code_len		);	// DonR 05Jan2012: Not really in use.
	GET_SHORT	(v_kesef_price_code			, Tprice_code_len		);
	GET_SHORT	(v_prev_ksf_prc_cod			, Tprice_code_len		);	// DonR 05Jan2012: Not really in use.
	GET_LONG	(v_fixed_price_basic		, Tfixed_price_len		);
	GET_LONG	(v_fixed_price_kesef		, Tfixed_price_len		);
	GET_LONG	(v_fixed_price_zahav		, Tfixed_price_len		);
	GET_SHORT	(v_in_health_basket			, TGenericFlag_len		);
	GET_SHORT	(v_enabled_status			, 2						);
	GET_SHORT	(v_yahalom_prc_code			, Tprice_code_len		);
	GET_LONG	(v_fixed_pr_yahalom			, Tfixed_price_len		);
	GET_SHORT	(v_wait_months				, Twait_months_len		);

	// Verify all data was read.
	ReadInput = PosInBuff - glbMsgBuffer;
	StaticAssert (ReadInput == data_len);


	// DonR 26Oct2008: Set "old" health-basket flag to 1 or 0 based on the value received
	// from AS400, and move the actual value received to the "new" health-basket flag
	// variable.
	PROCESS_HEALTH_BASKET;

	// DonR 09Oct2011: Translate v_enabled_status into individual enabled-flag variables.
	SET_TZAHAL_FLAGS;

	// Set Insurance Type flag (and some other stuff) based on which price-code field has a non-zero value.
	// DonR 20Oct2015: Add another sorting variable used to choose "best" insurance first (it will sort *descending*).
	if (v_basic_price_code > 0)
	{
		strcpy (v_insurance_type, "B");
		v_price_code	= v_basic_price_code;
		v_fixed_price	= v_fixed_price_basic;
		v_ins_type_sort	= 0;
	}
	else
	if (v_kesef_price_code > 0)
	{
		strcpy (v_insurance_type, "K");
		v_price_code	= v_kesef_price_code;
		v_fixed_price	= v_fixed_price_kesef;
		v_ins_type_sort	= 5;
	}
	else
	if (v_zahav_price_code > 0)
	{
		strcpy (v_insurance_type, "Z");
		v_price_code	= v_zahav_price_code;
		v_fixed_price	= v_fixed_price_zahav;
		v_ins_type_sort	= 10;
	}
	else
	if (v_yahalom_prc_code > 0)
	{
		strcpy (v_insurance_type, "Y");
		v_price_code	= v_yahalom_prc_code;
		v_fixed_price	= v_fixed_pr_yahalom;
		v_ins_type_sort	= 15;
	}
	else
	{
		strcpy (v_insurance_type, "U");	// Unknown - shouldn't ever happen.
		v_price_code	= 1;
		v_fixed_price	= 0;
		v_ins_type_sort	= -1;
	}

	// Just to be sure, force "generic" fixed price to zero if "generic" price code <> 1.
	if (v_price_code != 1)
		v_fixed_price	= 0;


	// DonR 24Nov2011: All but one of the four price-code fields should be zero - so add them together to get
	// the price code to look up in member_price.
	v_lookup_price_code = v_basic_price_code + v_kesef_price_code + v_zahav_price_code + v_yahalom_prc_code;

	retcode			= ALL_RETRIES_WERE_USED;
	stop_retrying	= false;
	need_to_read	= true;


	// Perform up to SQL_UPDATE_RETRIES retries to overcome access conflicts.
	for (n_retries = 0;
		 (n_retries < SQL_UPDATE_RETRIES) && (stop_retrying == false);
		 n_retries++)
	{
		// If this isn't the first iteration, sleep for a while.
		if (n_retries > 0)
		{
			sleep (SLEEP_TIME_BETWEEN_RETRIES);
		}

		// DonR 24Nov2011: Perform the lookup in member_price in order to set v_ptn_percent_sort, which is used
		// to retrieve the best member participation first without having to loop through all applicable rows.
		ExecSQL (	MAIN_DB, AS400SRV_READ_member_price_ptn_percent_sort,
					&v_ptn_percent_sort, &v_lookup_price_code, END_OF_ARG_LIST	);

		if (SQLERR_code_cmp (SQLERR_not_found) == MAC_TRUE)
		{
			v_ptn_percent_sort = 10001;	// Sort at end - 100.01% participation.
		}
		else
		{
			// Second possibility: there was an access conflict, so advance to the next retry iteration.
			if (SQLERR_code_cmp (SQLERR_access_conflict) == MAC_TRUE)
			{
				retcode = ALL_RETRIES_WERE_USED;
				continue;
			}
			else
			{
				// Third possibility: there was a database error, so don't retry - just give up.
				if (SQLERR_error_test ())
				{
					retcode = SQL_ERROR_OCCURED;
					break;
				}
			}
		}


		// Attempt to read the row first. If we get a result other than access conflict, we'll
		// know whether to insert or update. If we have an access conflict, need_to_read will
		// stay true, so we'll know to retry from this point; otherwise we'll skip this section
		// on the retry.
		// DonR 14Nov2011: Added enabled_status to the unique index for this table, and altered
		// the logic here accordingly. Since there is no equivalent in this table to the Rule
		// Number in drug_extension, the only way to allow different specialist rules for soldiers
		// is to enable different rows to be added/updated for different values of enabled_status.
		// DonR 24Nov2011: Needed to add Insurance Type to the index as well, because Magen Zahav
		// and Magen Kesef are becoming separate products and will require separate rows in
		// this table.
		if (need_to_read == true)
		{
			ExecSQL (	MAIN_DB, AS400SRV_T4019_READ_spclty_largo_prcnt_old_values,
						&o_basic_price_code,	&o_fixed_price_basic,	&o_kesef_price_code,
						&o_fixed_price_kesef,	&o_zahav_price_code,	&o_fixed_price_zahav,
						&o_in_health_basket,	&o_HealthBasketNew,		&o_yahalom_prc_code,
						&o_fixed_pr_yahalom,	&o_wait_months,			&v_update_date,
						&v_update_time,
						&v_speciality_code,		&v_largo_code,			&v_enabled_status,
						&v_insurance_type,		END_OF_ARG_LIST									);

			// First possibility: the row isn't already in the database, so we need to insert it.
			if (SQLERR_code_cmp (SQLERR_not_found) == MAC_TRUE)
			{
				do_insert = ATTEMPT_INSERT_FIRST;
				need_to_read = false;
			}
			else
			{
				// Second possibility: there was an access conflict, so advance to the next retry iteration.
				if (SQLERR_code_cmp (SQLERR_access_conflict) == MAC_TRUE)
				{
					retcode = ALL_RETRIES_WERE_USED;
					continue;
				}
				else
				{
					// Third possibility: there was a database error, so don't retry - just give up.
					if (SQLERR_error_test ())
					{
						retcode = SQL_ERROR_OCCURED;
						break;
					}

					else
					// Fourth possibility: fetch succeeded, so we'll do a conditional update.
					{
						do_insert = ATTEMPT_UPDATE_FIRST;
						need_to_read = false;
					}	// Read succeeded.
				}	// Not an access conflict.
			}	// Not record-not-found.
		}	// need_to_read is true.


		// If we get here, we've either read the row to update, or else we've determined that
		// the row isn't there and needs to be inserted.
		// DonR 04Jul2010: HOT FIX: Need to have dummy "for" loop here so that HANDLE_UPDATE_SQL_CODE
		// and HANDLE_INSERT_SQL_CODE work properly.
		for (i = 0; i < 1; i++)
		{
		if (do_insert == ATTEMPT_INSERT_FIRST)
		{
			ExecSQL (	MAIN_DB, AS400SRV_T4019_INS_spclty_largo_prcnt,
						&v_speciality_code,		&v_largo_code,			&v_basic_price_code,
						&v_fixed_price_basic,	&v_kesef_price_code,	&v_fixed_price_kesef,
						&v_zahav_price_code,	&v_fixed_price_zahav,	&v_in_health_basket,
						&v_HealthBasketNew,		&ROW_NOT_DELETED,		&v_enabled_status,
						&v_enabled_mac,			&v_enabled_hova,		&v_enabled_keva,
						&v_insurance_type,		&v_ptn_percent_sort,	&v_ins_type_sort,
						&v_yahalom_prc_code,	&v_fixed_pr_yahalom,	&v_wait_months,
						&v_price_code,			&v_fixed_price,

						&v_refresh_date,		&v_refresh_time,
						&v_refresh_date,		&v_refresh_time,
						&v_as400_batch_date,	&v_as400_batch_time,	END_OF_ARG_LIST			);

			HANDLE_INSERT_SQL_CODE ("4019", p_connect, &do_insert, &retcode, &stop_retrying);
		}	// Need to insert new spclty_largo_prcnt row.


		// If the row is already present, it needs to be updated.
		else
		{
			// Set update date/time fields based on whether changes have occurred.
			if ((v_basic_price_code			!=  o_basic_price_code		)	||
				(v_fixed_price_basic		!=  o_fixed_price_basic		)	||
				(v_kesef_price_code			!=  o_kesef_price_code		)	||
				(v_fixed_price_kesef		!=  o_fixed_price_kesef		)	||
				(v_zahav_price_code			!=  o_zahav_price_code		)	||
				(v_fixed_price_zahav		!=  o_fixed_price_zahav		)	||
				(v_yahalom_prc_code			!=  o_yahalom_prc_code		)	||
				(v_fixed_pr_yahalom			!=  o_fixed_pr_yahalom		)	||
				(v_wait_months				!=  o_wait_months			)	||
				(v_in_health_basket			!=  o_in_health_basket		)	||
				(v_HealthBasketNew			!=  o_HealthBasketNew		)		)
			{
				// At least one of the fields has changed.
				// Note that (at least as of August 2011) this table isn't sent to pharmacies
				// or Clicks - so we don't really need to worry about what changes are "real",
				// and this logic could be dispensed with.
				v_update_date		= v_refresh_date;
				v_update_time		= v_refresh_time;
				something_changed	= 1;
			}


			ExecSQL (	MAIN_DB, AS400SRV_T4019_UPD_spclty_largo_prcnt,
						&v_basic_price_code,	&v_fixed_price_basic,	&v_kesef_price_code,
						&v_fixed_price_kesef,	&v_zahav_price_code,	&v_fixed_price_zahav,
						&v_ptn_percent_sort,	&v_ins_type_sort,		&v_in_health_basket,
						&v_HealthBasketNew,		&v_enabled_mac,			&v_enabled_hova,
						&v_enabled_keva,		&ROW_NOT_DELETED,		&v_yahalom_prc_code,
						&v_fixed_pr_yahalom,	&v_wait_months,			&v_price_code,
						&v_fixed_price,			&v_refresh_date,		&v_refresh_time,
						&v_as400_batch_date,	&v_as400_batch_time,	&v_update_date, 
						&v_update_time,
						&v_speciality_code,		&v_largo_code,			&v_enabled_status,
						&v_insurance_type,		END_OF_ARG_LIST									);

			HANDLE_UPDATE_SQL_CODE ("4019", p_connect, &do_insert, &retcode, &stop_retrying);

		}	// In update mode (as row already exists).
		}	// Dummy loop.

	}	// Retry Loop.


	if (n_retries == SQL_UPDATE_RETRIES)
	{
		WRITE_ACCESS_CONFLICT_TO_LOG ("4019");
	}

	if (retcode == MSG_HANDLER_SUCCEEDED)
	{
		if (do_insert == ATTEMPT_INSERT_FIRST)
		{
			rows_added++;
		}
		else
		{
			if (something_changed)
				rows_updated++;
			else
				rows_refreshed++;
		}
	}


	return retcode;

}	// End of msg 2019/4019 handler.


// -------------------------------------------------------------------------
//
//       Post-processor for message 4019 - delete SPCLTY_LARGO_PRCNT
//			rows that are no longer in the AS/400 full-table download.
//
// -------------------------------------------------------------------------
static int postprocess_4019 (TConnection *p_connect)
{
	// local variables
	bool	stop_retrying;	// if true - return from function
	int		rows_deleted	= 0;
	int		retcode;		// code returned from function
	int		n_retries;		// retrying counter
	int		db_changed_flg;

	// DB variables
	int				v_as400_batch_date;
	int				v_as400_batch_time;
	int				v_row_count;


	// Initialize variables.
	v_as400_batch_date	= download_start_date;
	v_as400_batch_time	= download_start_time;


	retcode			= ALL_RETRIES_WERE_USED;
	stop_retrying	= false;


	while (1)
	{
		ExecSQL (	MAIN_DB, AS400SRV_T4019PP_READ_spclty_largo_prcnt_stale_count,
					&v_row_count,			&v_as400_batch_date,
					&v_as400_batch_time,	END_OF_ARG_LIST							);

		if (SQLERR_error_test ())
		{
			retcode = SQL_ERROR_OCCURED;
			break;
		}
		else
		{
			if (v_row_count == 0)
			{
				retcode = MSG_HANDLER_SUCCEEDED;
				break;
			}
		}

		ExecSQL (	MAIN_DB, AS400SRV_T4019PP_DEL_spclty_largo_prcnt,
					&v_as400_batch_date, &v_as400_batch_time, END_OF_ARG_LIST	);

		HANDLE_DELETE_SQL_CODE ("Postprocess 4019", p_connect, &retcode, &db_changed_flg);

		break;
	}

	rows_deleted = v_row_count;


	// DonR 13Nov2018 CR #15919: Record current table size.
	REMEMBER_ISOLATION;
	SET_ISOLATION_DIRTY;

	ExecSQL (	MAIN_DB, AS400SRV_T4019PP_READ_spclty_largo_prcnt_table_size,
				&total_rows, END_OF_ARG_LIST									);

	active_rows = total_rows;

	RESTORE_ISOLATION;
	// DonR 13Nov2018 CR #15919 end.


	GerrLogMini (GerrId,
				 "Postprocess 4019: %d rows deleted, retcode = %d.",
				 rows_deleted, retcode);

	return retcode;

}	// End of msg 4019 postprocessor.



/* << -------------------------------------------------------- >> */
/*                                                                */
/*  handler for message 2020 - delete rec from SPCLTY_LARGO_PRCNT */
/*                                                                */
/* << -------------------------------------------------------- >> */
static int message_2020_handler(
				TConnection	*p_connect,
				int		data_len
				)
{

/* << -------------------------------------------------------- >> */
/*			  local variables			  */
/* << -------------------------------------------------------- >> */
  char		*PosInBuff;		/* current pos in data buffer	    */
  int		ReadInput;		/* amount of Bytesread from buffer  */
  int		retcode;		/* code returned from function	    */
  int		n_retries;		/* retrying counter		    */
  int		i;
  int		db_changed_flg;


	/* << -------------------------------------------------------- >> */
	/*			    DB variables			  */
	/* << -------------------------------------------------------- >> */
	Tspeciality_code	v_speciality_code;
	Tlargo_code			v_largo_code;
	short				v_enabled_status;
	Tprice_code			v_basic_price_code;
	Tprice_code			v_zahav_price_code;
	Tprice_code			v_kesef_price_code;
	Tprice_code			v_yahalom_prc_code;
	char				v_insurance_type [2];
	int					sysdate;
	int					systime;


  sysdate = GetDate();
  systime = GetTime();

/* << -------------------------------------------------------- >> */
/*			  read input buffer			  */
/* << -------------------------------------------------------- >> */
  INIT_ERROR_DETECTION_MECHANISM;
  PosInBuff = glbMsgBuffer;

	GET_LONG	(v_speciality_code			, Tspeciality_code_len	);
	GET_LONG	(v_largo_code				, 9						);	//Marianna 14Jul2024 User Story #330877
	GET_SHORT	(v_enabled_status			, 2						);
	GET_SHORT	(v_basic_price_code			, Tprice_code_len		);
	GET_SHORT	(v_zahav_price_code			, Tprice_code_len		);
	GET_SHORT	(v_kesef_price_code			, Tprice_code_len		);
	GET_SHORT	(v_yahalom_prc_code			, Tprice_code_len		);

/* << -------------------------------------------------------- >> */
/*			  debug messages			  */
/* << -------------------------------------------------------- >> */
  /************************* COMMENTED OUT: **************************
  printf( "largo_code == %d\n",	v_largo_code );
  fflush( stdout );
  ********************************************************************/


/* << -------------------------------------------------------- >> */
/*		      verify all data was read			  */
/* << -------------------------------------------------------- >> */
  ReadInput = PosInBuff - glbMsgBuffer;
  StaticAssert( ReadInput == data_len );

	// DonR 02Jan2012: Set Insurance Type flag based on which price-code field has a non-zero value.
	if (v_basic_price_code > 0)
		strcpy (v_insurance_type, "B");
	else
	if (v_kesef_price_code > 0)
		strcpy (v_insurance_type, "K");
	else
	if (v_zahav_price_code > 0)
		strcpy (v_insurance_type, "Z");
	else
	if (v_yahalom_prc_code > 0)
		strcpy (v_insurance_type, "Y");
	else
		strcpy (v_insurance_type, "U");	// Unknown - shouldn't ever happen.

/* << -------------------------------------------------------- >> */
/*			   attempt delete			  */
/* << -------------------------------------------------------- >> */
  retcode = ALL_RETRIES_WERE_USED;
  db_changed_flg = 0;

  for( n_retries = 0;
       n_retries < SQL_UPDATE_RETRIES;
       n_retries++ )
    {

      /*
       * sleep for a while:
       */
      if( n_retries )
      {
	  sleep( SLEEP_TIME_BETWEEN_RETRIES );
      }

		ExecSQL (	MAIN_DB, AS400SRV_T2020_DEL_spclty_largo_prcnt,
					&v_speciality_code,		&v_largo_code,
					&v_enabled_status,		&v_insurance_type,
					END_OF_ARG_LIST									);

      HANDLE_DELETE_SQL_CODE( "2020", p_connect, &retcode, &db_changed_flg );

      break;
    }

  if( n_retries == SQL_UPDATE_RETRIES )
  {
     WRITE_ACCESS_CONFLICT_TO_LOG( "2020" );
  }

  if (retcode == MSG_HANDLER_SUCCEEDED)
	  rows_deleted++;

  return retcode;
} /* end of msg 2020 handler */


/* << ------------------------------------------------------------ >> */
/*                                                                    */
/*    handler for message 2021/4021 - upd/ins to DOCTOR_PERCENTS	  */
/*                                                                    */
/* << ------------------------------------------------------------ >> */
static int data_2021_handler (TConnection	*p_connect,
							  int			data_len)
{

/* << -------------------------------------------------------- >> */
/*			  local variables			  */
/* << -------------------------------------------------------- >> */
  char		*PosInBuff;		/* current pos in data buffer	    */
  int		ReadInput;		/* amount of Bytesread from buffer  */
  int		retcode;		/* code returned from function	    */
  bool		stop_retrying;		/* if true - return from function   */
  bool		do_insert = ATTEMPT_UPDATE_FIRST;
  int		n_retries;		/* retrying counter		    */
  int		i;


	/* << -------------------------------------------------------- >> */
	/*			    DB variables			  */
	/* << -------------------------------------------------------- >> */
	Tdoctor_id			v_doctor_id;
	Tlargo_code			v_largo_code;
	Tprice_code			v_basic_price_code;
	Tprice_code			v_prev_bas_prc_cod;	// DonR 05Jan2012: Not really in use.
	Tprice_code			v_zahav_price_code;
	Tprice_code			v_prev_zhv_prc_cod;	// DonR 05Jan2012: Not really in use.
	Tprice_code			v_kesef_price_code;
	Tprice_code			v_prev_ksf_prc_cod;	// DonR 05Jan2012: Not really in use.
	Tdoct_number		v_doct_number;
	int					sysdate;
	int					systime;
	int					v_as400_batch_date;
	int					v_as400_batch_time;
	TTreatmentCategory	v_treatment_category;


  sysdate = GetDate();
  systime = GetTime();
	v_as400_batch_date = (download_start_date > 0) ? download_start_date : sysdate;
	v_as400_batch_time = (download_start_time > 0) ? download_start_time : systime;

/* << -------------------------------------------------------- >> */
/*			read input buffer			  */
/* << -------------------------------------------------------- >> */
  INIT_ERROR_DETECTION_MECHANISM;
  PosInBuff = glbMsgBuffer;


  v_doctor_id		=  get_long(
				    &PosInBuff,
				    Tdoctor_id_len
				    );
  RETURN_ON_ERROR;

  v_largo_code		=  get_long(
				    &PosInBuff,
				    9
				    );	//Marianna 14Jul2024 User Story #330877
  RETURN_ON_ERROR;

  v_basic_price_code	=  get_short(
				     &PosInBuff,
				     Tprice_code_len
				     );
  RETURN_ON_ERROR;

  v_prev_bas_prc_cod	=  get_short(
				     &PosInBuff,
				     Tprice_code_len
				     );	// DonR 05Jan2012: Not really in use.
  RETURN_ON_ERROR;

  v_zahav_price_code	=  get_short(
				     &PosInBuff,
				     Tprice_code_len
				     );
  RETURN_ON_ERROR;

  v_prev_zhv_prc_cod	=  get_short(
				     &PosInBuff,
				     Tprice_code_len
				     );	// DonR 05Jan2012: Not really in use.
  RETURN_ON_ERROR;

  v_kesef_price_code	=  get_short(
				     &PosInBuff,
				     Tprice_code_len
				     );
  RETURN_ON_ERROR;

  v_prev_ksf_prc_cod	=  get_short(
				     &PosInBuff,
				     Tprice_code_len
				     );	// DonR 05Jan2012: Not really in use.
  RETURN_ON_ERROR;

  v_doct_number	=  get_long(    //insteead short 20021020
				     &PosInBuff,
				     Tdoct_number_len
				     );
  RETURN_ON_ERROR;


  v_treatment_category	=  get_short(
				    &PosInBuff,
				    Ttreatment_category_len
				    );
  RETURN_ON_ERROR;


  // Translate AS400 Treatment Category into Unix version.
  i = v_treatment_category;
  switch (i)
  {
	  case TREAT_TYPE_AS400_FERTILITY:	v_treatment_category = TREATMENT_TYPE_FERTILITY;	break;

	  case TREAT_TYPE_AS400_DIALYSIS:	v_treatment_category = TREATMENT_TYPE_DIALYSIS;		break;

	  // Marianna 11May2025: User Story #410137
	  case TREAT_TYPE_AS400_PREP:		v_treatment_category = TREATMENT_TYPE_PREP;			break;

	  case TREAT_TYPE_AS400_SMOKING:	v_treatment_category = TREATMENT_TYPE_SMOKING;		break;

	  // If we get an unrecognized value, set it to GENERAL.
	  default:							v_treatment_category = TREATMENT_TYPE_GENERAL;		break;
  }


/* << -------------------------------------------------------- >> */
/*			    debug messages			  */
/* << -------------------------------------------------------- >> */
  /************************* COMMENTED OUT:  *************************
  printf( "Tdoctor_id == %d\n",	v_doctor_id );
  printf( "Tlargo_code == %d\n", 	v_largo_code );
  printf( "Tbasic_price_code == %d\n",  v_basic_price_code );
  printf( "Tzahav_price_code == %d\n", 	v_zahav_price_code );
  printf( "Tkesef_price_code == %d\n", 	v_kesef_price_code );
  fflush( stdout );
  ******************************************************************/

  /*
   *   verify all data was read:
   */
  ReadInput = PosInBuff - glbMsgBuffer;
  StaticAssert( ReadInput == data_len );


/* << -------------------------------------------------------- >> */
/*	   up to SQL_UPDATE_RETRIES retries will be made	  */
/*		    to overcome access conflict			  */
/* << -------------------------------------------------------- >> */
  retcode = ALL_RETRIES_WERE_USED;
  stop_retrying = false;

  for(  n_retries = 0;
       ( n_retries < SQL_UPDATE_RETRIES ) && ( stop_retrying == false );
       n_retries++ )
    {

      /*
       * sleep for a while:
       */
      if( n_retries )
      {
	  sleep( SLEEP_TIME_BETWEEN_RETRIES );
      }


	  // DonR 24Nov2011: Update in_doctor_percents flag in drug_list if this doctor_percents row is to be
	  // used in setting member participation.
	  if (((v_basic_price_code + v_kesef_price_code + v_zahav_price_code) > 0)	&&
		  (v_treatment_category == TREATMENT_TYPE_GENERAL))
	  {
			ExecSQL (	MAIN_DB, AS400SRV_T4021_UPD_drug_list_in_doctor_percents,
						&v_largo_code, END_OF_ARG_LIST								);
	  }

      /*
       *  try to insert/update new rec with the order depending on "do_insert":
       */
      for( i = 0;  i < 2;  i++ )
	{
	  if( do_insert == ATTEMPT_INSERT_FIRST )
	    {
			ExecSQL (	MAIN_DB, AS400SRV_T4021_INS_doctor_percents,
						&v_doctor_id,				&v_largo_code,
						&v_basic_price_code,		&v_zahav_price_code,
						&v_kesef_price_code,		&v_doct_number,
						&v_treatment_category,		&sysdate,
						&systime,					&v_as400_batch_date,
						&v_as400_batch_time,		&ROW_NOT_DELETED,
						END_OF_ARG_LIST										);

              HANDLE_INSERT_SQL_CODE( "4021",
					p_connect,
				      &do_insert,
				      &retcode,
				      &stop_retrying );

	    }
	  else
	    {
			ExecSQL (	MAIN_DB, AS400SRV_T4021_UPD_doctor_percents,
						&v_basic_price_code,	&v_zahav_price_code,
						&v_kesef_price_code,	&v_doct_number,
						&sysdate,				&systime,
						&v_as400_batch_date,	&v_as400_batch_time,
						&ROW_NOT_DELETED,		&v_doctor_id,
						&v_largo_code,			&v_treatment_category,
						END_OF_ARG_LIST									);

              HANDLE_UPDATE_SQL_CODE( "4021",
					p_connect,
				      &do_insert,
				      &retcode,
				      &stop_retrying );

	    }

	}/* for( i... */

    }/* for( n_retries... */

  if( n_retries == SQL_UPDATE_RETRIES )
  {
     WRITE_ACCESS_CONFLICT_TO_LOG( "4021" );
  }

  if (retcode == MSG_HANDLER_SUCCEEDED)
  {
	  if (do_insert == ATTEMPT_INSERT_FIRST)
		  rows_added++;
	  else
		  rows_updated++;
  }


  return retcode;
} /* end of msg 2021/4021 handler */


/* << -------------------------------------------------------- >> */
/*                                                                */
/*   handler for message 2022 - delete rec from DOCTOR_PERCENTS	  */
/*																  */
/*			NOT IN USE!	(although CL.EC still supports it)		  */
/*                                                                */
/* << -------------------------------------------------------- >> */
static int message_2022_handler(
				TConnection	*p_connect,
				int		data_len
				)
{

/* << -------------------------------------------------------- >> */
/*			  local variables			  */
/* << -------------------------------------------------------- >> */
  char		*PosInBuff;		/* current pos in data buffer	    */
  int		ReadInput;		/* amount of Bytesread from buffer  */
  int		retcode;		/* code returned from function	    */
  int		n_retries;		/* retrying counter		    */
  int		i;
  int		db_changed_flg;


	/* << -------------------------------------------------------- >> */
	/*			    DB variables			  */
	/* << -------------------------------------------------------- >> */
	Tdoctor_id	v_doctor_id;
	Tlargo_code	v_largo_code;


/* << -------------------------------------------------------- >> */
/*			  read input buffer			  */
/* << -------------------------------------------------------- >> */
  INIT_ERROR_DETECTION_MECHANISM;
  PosInBuff = glbMsgBuffer;

  v_doctor_id		=  get_long(
				    &PosInBuff,
				    Tdoctor_id_len
				    );
  RETURN_ON_ERROR;

  v_largo_code		=  get_long(
				    &PosInBuff,
				    9
				    );	//Marianna 14Jul2024 User Story #330877
  RETURN_ON_ERROR;

/* << -------------------------------------------------------- >> */
/*			  debug messages			  */
/* << -------------------------------------------------------- >> */
  /************************* COMMENTED OUT: ***************************
  printf( "doctor_id == %d\n",	v_doctor_id );
  printf( "largo_code == %d\n",v_largo_code );
  fflush( stdout );
  *********************************************************************/

/* << -------------------------------------------------------- >> */
/*		      verify all deta was read			  */
/* << -------------------------------------------------------- >> */
  ReadInput = PosInBuff - glbMsgBuffer;
  StaticAssert( ReadInput == data_len );


/* << -------------------------------------------------------- >> */
/*			   attempt delete			  */
/* << -------------------------------------------------------- >> */
  retcode = ALL_RETRIES_WERE_USED;
  db_changed_flg = 0;

  for( n_retries = 0;
       n_retries < SQL_UPDATE_RETRIES;
       n_retries++ )
    {

      /*
       * sleep for a while:
       */
      if( n_retries )
      {
	  sleep( SLEEP_TIME_BETWEEN_RETRIES );
      }

		ExecSQL (	MAIN_DB, AS400SRV_T2022_DEL_doctor_percents,
					&v_doctor_id, &v_largo_code, END_OF_ARG_LIST	);

      HANDLE_DELETE_SQL_CODE( "2022", p_connect, &retcode, &db_changed_flg );

      break;
    }

  if( n_retries == SQL_UPDATE_RETRIES )
  {
     WRITE_ACCESS_CONFLICT_TO_LOG( "2022" );
  }

  if (retcode == MSG_HANDLER_SUCCEEDED)
	  rows_deleted++;


  return retcode;
} /* end of msg 2022 handler */


// -------------------------------------------------------------------------
//
//       Post-processor for message 4021 - delete DOCTOR_PERCENTS
//			rows that are no longer in the AS/400 full-table download.
//
// -------------------------------------------------------------------------
static int postprocess_4021 (TConnection *p_connect)
{
	// local variables
	bool	stop_retrying;	// if true - return from function
	int		rows_deleted	= 0;
	int		retcode;		// code returned from function
	int		n_retries;		// retrying counter
	int		db_changed_flg;

	// DB variables
	int				v_as400_batch_date;
	int				v_as400_batch_time;
	int				v_row_count;
	short			v_treat_category = TREATMENT_TYPE_GENERAL;


	// Initialize variables.
	v_as400_batch_date	= download_start_date;
	v_as400_batch_time	= download_start_time;


	retcode			= ALL_RETRIES_WERE_USED;
	stop_retrying	= false;


	while (1)
	{
		ExecSQL (	MAIN_DB, AS400SRV_T4021PP_READ_doctor_percents_stale_count,
					&v_row_count,			&v_as400_batch_date,
					&v_as400_batch_time,	END_OF_ARG_LIST						);

		if (SQLERR_error_test ())
		{
			retcode = SQL_ERROR_OCCURED;
			break;
		}
		else
		{
			if (v_row_count == 0)
			{
				retcode = MSG_HANDLER_SUCCEEDED;
				break;
			}
		}

		ExecSQL (	MAIN_DB, AS400SRV_T4021PP_DEL_doctor_percents,
					&v_as400_batch_date, &v_as400_batch_time, END_OF_ARG_LIST	);

		HANDLE_DELETE_SQL_CODE ("Postprocess 4021", p_connect, &retcode, &db_changed_flg);

		// DonR 24Nov2011: Clear any in_doctor_percents flags in drug_list that are no longer applicable.
		ExecSQL (	MAIN_DB, AS400SRV_T4021PP_UPD_drug_list_clear_in_doctor_percents,
					&v_treat_category, END_OF_ARG_LIST									);

		break;
	}

	rows_deleted = v_row_count;


	// DonR 13Nov2018 CR #15919: Record current table size.
	REMEMBER_ISOLATION;
	SET_ISOLATION_DIRTY;

	ExecSQL (	MAIN_DB, AS400SRV_T4021PP_READ_doctor_percents_table_size,
				&total_rows, END_OF_ARG_LIST								);

	active_rows = total_rows;

	RESTORE_ISOLATION;
	// DonR 13Nov2018 CR #15919 end.


	GerrLogMini (GerrId,
				 "Postprocess 4021: %d rows deleted, retcode = %d.",
				 rows_deleted, retcode);

	return retcode;

}	// End of msg 4021 postprocessor.



/* << -------------------------------------------------------- >> */
/*                                                                */
/*        handler for message 2025 - upd/ins to MEMBERS		  */
/*                                                                */
/* << -------------------------------------------------------- >> */
static int message_2025_handler (
									TConnection	*p_connect,
									int			data_len,
									bool		do_insert
								)
{
	// local variables
	char	*PosInBuff;		/* current pos in data buffer	    */
	int		ReadInput;		/* amount of Bytesread from buffer  */
	int		retcode;		/* code returned from function	    */
	bool	stop_retrying;	/* if true - return from function   */
	int		n_retries;		/* retrying counter		    */
	int		i;
	int		member_is_mivaker;	// DonR 05Nov2019: This is the value we get from APPLIB/MEMBERS/KDSNIF; it's
								// an indicator of whether the member is a "mivaker", exempt from interaction
								// and overdose checking. What we want to store in members/check_od_interact
								// is the *inverse* of this value: ordinary members get 1 in check_od_interact,
								// and mivakrim get 0.
	short	darkonai_type;		// DonR 12Aug2021 User Story #163882: This is from APPLIB/MEMBERS/DRK;
								// it's an indicator of whether the member is of a special "darkonai" type.
								// Currently recognized types are:
								// 1 =	"Darkonai-Plus", given automatic service without card (without
								//		checking any other conditions) and suppressing all normal discounts.
								//		These members pay 100%, which can be offset by vouchers issued by
								//		Harel insurance.
								// 2 =	"Darkonaim" who are given automatic service without card, but are
								//		otherwise treated as normal Maccabi members - so they get regular
								//		discounts and so on.
								// 0 (or any other value) means the person is treated as an ordinary member,
								// even if s/he is in fact a darkonai.
	static struct timeval tv_start, tv_end;

	short int          Exist,cnt;
    
    /*
     * List of contractors that participated
     * in the transaction to: conmemb
     * -----------------------------------------
     */

    #define NUM_TARIF_ROFE_REC 5/* Number of records defined */
                                /* in "TarifRofeList"        */

	/* << -------------------------------------------------------- >> */
	/*			    DB variables			  */
	/* << -------------------------------------------------------- >> */
	typedef struct          
	{
		int                 TarifRofe;
		Tmember_id          Contractor;
		short int           ProfessionGroup;
		short int           ContractType;
		short int           LinkType;
	} TTarifRofeRecord;

	char				sign[64];    /* do NOT change len to 2 */
	Tmember_id			v_member_id;
	Tmem_id_extension	v_mem_id_extension;
	Tcard_date			v_card_date;
	Tfather_name		v_father_name;
	Tfirst_name			v_first_name;
	Tlast_name	 		v_last_name;
	Tstreet				v_street;
	Thouse_num			v_house_num;
	Tcity  				v_city;
	Tphone 				v_phone;
	Tzip_code			v_zip_code;
	Tsex				v_sex;
	Tdate_of_bearth		v_date_of_bearth;
	Tmarital_status		v_marital_status;

	/*  < ------------------------------ > */
	/*    The value & sign of some codes   */
	/*       will be passed separatly.     */
	/*  < ------------------------------ > */

	TCodeNoSign			v_maccabi_code_nosign;
	Tmaccabi_code		v_maccabi_code;
	Tmaccabi_until		v_maccabi_until;

	TCodeNoSign			v_mac_magen_code_nosign;
	Tmac_magen_code		v_mac_magen_code;
	Tmac_magen_from		v_mac_magen_from;
	Tmac_magen_until	v_mac_magen_until;

	TCodeNoSign  		v_keren_macabi_nosign;
	Tkeren_macabi   	v_keren_macabi;
	Tkeren_mac_from		v_keren_mac_from;
	Tkeren_mac_until	v_keren_mac_until;

	TCodeNoSign  		v_yahalom_nosign;
	short				v_yahalom_code;			// DonR 12Nov2012 Yahalom.
	int					v_yahalom_from;			// DonR 12Nov2012 Yahalom.
	int					v_yahalom_until;		// DonR 12Nov2012 Yahalom.

	Tasaf_code			v_asaf_code;
	Tinsurance_status	v_insurance_status;
	Tdoctor_status		v_doctor_status;
	Tcredit_type_code	v_credit_type_code;
	Tmember_discount_pt	v_member_discount_pt;
	Told_member_id		v_old_member_id;
	Told_id_extension	v_old_id_extension	= 0;	// DonR 29Jul2025 User Story #435969 - no longer comes from AS/400.
	short				dangerous_drug_status;		// DonR 29Jul2025 User Story #435969 - repurpose MEMBERS/OLDKTZ.
	Told_member_id		v_number_family;
	Told_id_extension	v_code_family;
	Tdate_of_bearth		v_update_address;
	Tenglish_name		v_last_english;			//Yulia 20060914
	Tenglish_name		v_first_english;		//Yulia20060914
	int 				v_mess_2025;
	int					check_od_interact;		// DonR 05Nov2019: Comes from the KDSNIF column in APPLIB/MEMBERS,
												// but it has nothing to do with snifs (or sniffles); it's now an
												// indicator of whether the member is a "mivaker" who is exempt
												// from interaction and overdose checking. Note that this value
												// is the *inverse* of KDSNIF - if the "mivaker flag" is non-zero,
												// we want to *disable* interaction/overdose checking.
	short				force_100_percent_ptn;
	short				darkonai_no_card;


	short				v_LinkType ;//yulia 07.2000
	Tkeren_wait_flag	v_keren_wait_flag;	// DonR 11Dec2003
	short				v_has_tikra;	// DonR 16Mar2010 - "Nihul Tikrot"
	short				v_has_coupon;	// DonR 16Mar2010 - "Nihul Tikrot"
	short				v_spec_presc;	// DonR 19Dec2018
	int					num_blocked_drugs;	// DonR 17Oct2021 User Story #196891.
	short				has_blocked_drugs;	// DonR 17Oct2021 User Story #196891.
	short				v_in_hospital;		// DonR 02Aug2011 - Member Hospitalization enhancement.
	int					v_discharge_date;	// DonR 02Aug2011 - Member Hospitalization enhancement.
	int					v_vetek;				// DonR 12Nov2012 Yahalom.
	char				v_insurance_type [2];	// DonR 12Nov2012 Yahalom.

	short				Illness [10];
	short				illness_1;
	short				illness_2;
	short				illness_3;
	short				illness_4;
	short				illness_5;
	short				illness_6;
	short				illness_7;
	short				illness_8;
	short				illness_9;
	short				illness_10;
	int					illness_bitmap;

	int					sysdate;
	int					systime;

	// DonR 29Jan2020: Since the ODBC routines don't like constants, use variables instead.
	int					UpdatedBy		= 7;
	int					Forever			= 20391231;
	short				FourSevens		= 7777;
	char				*BlankString	= "";

	TTarifRofeRecord TarifRofeList[]=                          
	{
		/*| Tarif | Contractor | Profession | Contract | Link  |
		| Rofe  |            | Group      | Type     | Type  |
		|-------+------------+------------+----------+-------|*/
		{  6800 ,  50687011  ,    10      ,    0     ,    3  },
		{  6851 ,  12988788  ,    10      ,    0     ,    3  },
		{  6850 ,  15765415  ,    10      ,    0     ,    3  },
		{  6852 ,  13935085  ,    10      ,    0     ,    3  },
		{  9999 ,  9999999   ,    99      ,    9     ,  999  }
	};
	/*|                    IMPORTANT                       |
	|====>>>>>>>>      DO NOT FORGET         <<<<<<<<====|
	|          to update NUM_TARIF_ROFE_REC              |*/



/* << -------------------------------------------------------- >> */
/*		prepare host variables for update,insert cursor	  */
/* << -------------------------------------------------------- >> */
  v_LinkType =   4;//ConMembLinkDiet; yulia 07.2000
  v_member_id = 0;
  v_mem_id_extension = 0;
  v_card_date = 0;
  v_father_name[0] = 0;
  v_first_name[0] = 0;
  v_last_name[0] = 0;
  v_street[0] = 0;
  v_city[0] = 0;
  v_phone[0] = 0;
  v_zip_code = 0;
  v_sex = 0;
  v_date_of_bearth = 0;
  v_marital_status = 0;
  v_maccabi_code = 0;
  v_house_num[0] = 0;
  v_maccabi_until = 0;
  v_mac_magen_code = 0;
  v_mac_magen_from = 0;
  v_mac_magen_until = 0;
  v_keren_macabi = 0;
  v_keren_mac_from = 0;
  v_keren_mac_until = 0;
  v_asaf_code = 0;
  v_insurance_status = 0;
  v_doctor_status = 0;
  v_credit_type_code = 0;
  v_member_discount_pt = 0;
  v_old_member_id = 0;
  dangerous_drug_status = 0;		// DonR 29Jul2025 User Story #435969.
  v_number_family = 0;
  v_code_family = 0;
  v_update_address = 0;
  v_last_english[0] = 0;
  v_first_english[0] = 0;
  v_has_tikra	= 0;
  v_has_coupon	= 0;
  has_blocked_drugs	= 0;
  v_in_hospital	= 0;
  v_yahalom_code	= 0;
  v_yahalom_from	= 0;
  v_yahalom_until	= 0;
  v_vetek			= 0;
  illness_1 = illness_2 = illness_3 = illness_4 = illness_5  = 0;
  illness_6 = illness_7 = illness_8 = illness_9 = illness_10 = 0;
  illness_bitmap = 0;
  strcpy (v_insurance_type, "N");	// Default: none or unknown.

  sysdate = GetDate();
  systime = GetTime();

	/* << -------------------------------------------------------- >> */
	/*			read input buffer			  */
	/* << -------------------------------------------------------- >> */
	INIT_ERROR_DETECTION_MECHANISM;
	PosInBuff = glbMsgBuffer;
	// GerrLogReturn (GerrId, "[%s]",  PosInBuff);

	GET_LONG		(v_member_id,				Tmember_id_len);
	GET_SHORT		(v_mem_id_extension,		Tmem_id_extension_len);
	GET_SHORT		(v_card_date,				Tcard_date_len);
	GET_HEB_STR		(v_father_name,				Tfather_name_len);
	GET_HEB_STR		(v_first_name,				Tfirst_name_len);
	GET_HEB_STR		(v_last_name,				Tlast_name_len);
	GET_HEB_STR		(v_street,					Tstreet_len);
	GET_HEB_STR		(v_house_num,				Thouse_num_len);
	GET_HEB_STR		(v_city,					Tcity_len);
	GET_STRING		(v_phone,					Tphone_len);
	GET_LONG		(v_zip_code,				Tzip_code_len);
	GET_SHORT		(v_sex,						Tsex_len);
	GET_LONG		(v_date_of_bearth,			Tdate_len);
	GET_SHORT		(v_marital_status,			Tmarital_status_len);
	GET_SIGNED		(v_maccabi_code,			v_maccabi_code_nosign,		sign,	TCodeNoSign_len);
	GET_LONG		(v_maccabi_until,			Tdate_len);
	GET_SIGNED		(v_mac_magen_code,			v_mac_magen_code_nosign,	sign,	TCodeNoSign_len);
	GET_LONG		(v_mac_magen_from,			Tdate_len);
	GET_LONG		(v_mac_magen_until,			Tdate_len);
	GET_SIGNED		(v_keren_macabi,			v_keren_macabi_nosign,		sign,	TCodeNoSign_len);
	GET_LONG		(v_keren_mac_from,			Tdate_len);
	GET_LONG		(v_keren_mac_until,			Tdate_len);
	GET_SHORT		(v_keren_wait_flag,			Tkeren_wait_flag_len);
	GET_SHORT		(v_asaf_code,				Tasaf_code_len);
	GET_SHORT		(v_insurance_status,		Tinsurance_status_len);
	GET_LONG		(v_doctor_status,			Tdoctor_status_len);
	GET_SHORT		(v_credit_type_code,		Tcredit_type_code_len);
	GET_SHORT		(v_member_discount_pt,		Tmember_discount_pt_len);
	GET_LONG		(v_old_member_id,			Told_member_id_len);
	GET_SHORT		(dangerous_drug_status,		Told_id_extension_len);	// DonR 29Jul2025 User Story #435969.
	GET_LONG		(v_number_family,			Told_member_id_len);
	GET_SHORT		(v_code_family,				Told_id_extension_len);
	GET_LONG		(v_update_address,			Tdate_of_bearth_len);
	GET_STRING		(v_last_english,			Tenglish_name_len);
	GET_STRING		(v_first_english,			Tenglish_name_len);
	GET_LONG		(v_mess_2025,				9);
	GET_LONG		(member_is_mivaker,			9);	// DonR 05Nov2019 - this is the value from APPLIB/MEMBERS/KDSNIF.
	GET_SHORT		(v_has_tikra,				1);	// DonR 14Mar2010 - "Nihul Tikrot"

	// DonR 12Nov2012 Yahalom begin.
	GET_SIGNED		(v_yahalom_code,			v_yahalom_nosign,		sign,	TCodeNoSign_len);
	GET_LONG		(v_yahalom_from,			Tdate_len);
	GET_LONG		(v_yahalom_until,			Tdate_len);
	GET_LONG		(v_vetek,					Tdate_len);

	for (i = 0; i < 10; i++)
	{
		GET_SHORT	(Illness [i],				2);	// DonR 28May2014
	}

	GET_SHORT		(darkonai_type,				1);	// DonR 12Aug2021 User Story #163882.


	// Verify all data was read.
	ReadInput = PosInBuff - glbMsgBuffer;
	StaticAssert( ReadInput == data_len );


	// DonR 03Jan2012: On the Test system, the AS/400 sometimes sends bogus values for Father Name
	// that cause integrity-violation (-391) errors on the INSERT/UPDATE. Accordingly, attempt to
	// substitute something halfway reasonable when these values appear.
	if (*v_father_name < ' ')
	{
// GerrLogMini (GerrId, "Intercepted bogus Father Name for member %d - first char = %d.", v_member_id, (short)*v_father_name);
		strcpy (v_father_name, " ");
	}

	// Set Insurance Type flag based on which insurance-status field has a value above zero.
	// (Note that we shouldn't have to check dates, since an expired insurance should have a negative code.)
	// DonR 29Nov2012: Assign sequentially, so member gets flagged with the "best" insurance s/he has.
	// (Note that member *should* have only one insurance anyway!)
	if (v_maccabi_code		> 0)	strcpy (v_insurance_type, "B");
	if (v_keren_macabi		> 0)	strcpy (v_insurance_type, "K");
	if (v_mac_magen_code	> 0)	strcpy (v_insurance_type, "Z");
	if (v_yahalom_code		> 0)	strcpy (v_insurance_type, "Y");

	// DonR 10Dec2012: For Magen Kesef and "Yahalom", translate the eligibility flag into a 
	// unique number (for Assuta et al).
	v_keren_macabi = (v_keren_macabi > 0) ? 2 : -2;
	v_yahalom_code = (v_yahalom_code > 0) ? 7 : -7;

	// DonR 05Nov2019: Assign values to check_od_interact based on the
	// *invert* of member_is_mivaker.
	check_od_interact = (member_is_mivaker) ? 0 : 1;

	// DonR 09Aug2021 User Story #163882: Process new types of "darkonai" to
	// support darkonai-plus features.
	// DonR 30Aug2021: In cases where we don't get a recognized value of 1 or 2 for darkonai_type,
	// set darkonai_no_card based on the person's TZ Code - a value of 9 means that s/he is a
	// darkonai and gets service without a magnetic card even if darkonai_type was not set to
	// 1 or 2.
	switch (darkonai_type)
	{
		case 0:		// Treat as ordinary member (with darknonai_no_card set based on TZ Code).
					force_100_percent_ptn	= 0;
					darkonai_no_card		= (v_mem_id_extension == DARKONAI) ? 1 : 0;
					break;

		case 1:		// "Darkonai-Plus" who always pays 100%, minus discounts from
					// insurance-company vouchers.
					force_100_percent_ptn	= 1;
					darkonai_no_card		= 1;
					break;

		case 2:		// "Darkonai-Plus" who has regular Maccabi insurance, but gets
					// unconditional service without magnetic card.
					force_100_percent_ptn	= 0;
					darkonai_no_card		= 1;
					break;

		default:	// Any strange value will be treated like an ordinary member (with darknonai_no_card set based on TZ Code).
					GerrLogMini (GerrId, "Strange Darkonai Type %d for ID %d - changing to zero!", darkonai_type, v_member_id);
					GerrLogFnameMini ("darkonaim_log", GerrId, "Strange Darkonai Type %d for ID %d - changing to zero!", darkonai_type, v_member_id);
					force_100_percent_ptn	= 0;
					darkonai_no_card		= (v_mem_id_extension == DARKONAI) ? 1 : 0;
					darkonai_type			= 0;	// So we store the "corrected" value.
					break;
	}	// switch (darkonai_type)


	// DonR 28May2014: Set up Illness Bitmap based on the Illness Codes received from AS/400.
	illness_bitmap = 0;	// Redundant, but paranoia is a good thing sometimes.

	// DonR 07Jan2015: AS/400 can send duplicate illness codes. In this case,
	// adding to the illness bitmap causes problems - for example, "cancer
	// plus cancer" equals 256 instead of 128, and the member winds up
	// *not* getting discounts for cancer drugs! The solution is to use
	// bitwise-OR's rather than arithmetic addition.
	// DonR 10Sep2015: Added more illness codes, including 98 for car-accident victims.
	// Translate high codes (98 and 99) to reasonable bit-shift values, and also allow
	// "reserved" illness codes to extend to 11 rather than 10.
	// DonR 02Mar2016: Added Work Accident as another pseudo-illness; in this case
	// we translate the value we get from AS/400 of 97 to bitmap position 28.
	for (i = 0; i < 10; i++)
	{
		if (Illness [i] == 97)	// Work accident.
		{
			Illness [i] = 28;
		}
		else
		if (Illness [i] == 98)	// Traffic accident.
		{
			Illness [i] = 29;
		}
		else
		if (Illness [i] == 99)
		{
			Illness [i] = 30;
		}
		else
		if ((Illness [i] < 0) || (Illness [i] > 11))
		{
			Illness [i] = 0;
		}

		if (Illness [i])
		{
			illness_bitmap |= (1 << (Illness [i] - 1));
		}
	}
	
	illness_1	= Illness [0];
	illness_2	= Illness [1];
	illness_3	= Illness [2];
	illness_4	= Illness [3];
	illness_5	= Illness [4];
	illness_6	= Illness [5];
	illness_7	= Illness [6];
	illness_8	= Illness [7];
	illness_9	= Illness [8];
	illness_10	= Illness [9];



//  GerrLogFnameMini("2025_log",GerrId,"Member %d: Mac Code %d, Kesef %d, Zahav %d, Yahalom %d, Ins Type {%s}",
//  v_member_id, v_maccabi_code, v_keren_macabi, v_mac_magen_code, v_yahalom_code, v_insurance_type);

  // DonR 12Nov2012 Yahalom end.


/* << -------------------------------------------------------- >> */
/*			    debug messages			  */
/* << -------------------------------------------------------- >> */

  /************************* COMMENTED OUT:  ****************************
  printf( "\n\nTmember_id == %d\n", 	v_member_id );
  printf( "Tmem_id_extension == %d\n", 	v_mem_id_extension );
  printf( "Tcard_date == %d\n", 	v_card_date );
  printf( "Tfather_name == >%s< \n", 	v_father_name );
  printf( "Tfirst_name == >%s<\n", 	v_first_name );
  printf( "Tlast_name == >%s<\n", 	v_last_name );
  printf( "Tstreet == >%s<\n", 		v_street );
  printf( "Tcity == >%s<\n", 		v_city );
  printf( "Tphone ==>%s<\n", 		v_phone );
  printf( "Tzip_code == %d\n", 	v_zip_code );
  printf( "Tsex == %d\n", 		v_sex );
  printf( "Tdate_of_bearth == %d\n", 	v_date_of_bearth );
  printf( "Tmarital_status == %d\n", 	v_marital_status );
  printf( "Tmaccabi_code == %d\n", 	v_maccabi_code );
  printf( "Thouse num  == >%s<\n", 	v_house_num );
  printf( "Tmaccabi_until == %d\n", 	v_maccabi_until );
  printf( "Tmac_magen_code == %d\n", 	v_mac_magen_code );
  printf( "Tmac_magen_from == %d\n", 	v_mac_magen_from );
  printf( "Tmac_magen_until == %d\n", 	v_mac_magen_until );
  printf( "Tkeren_macabi == %d\n", 	v_keren_macabi );
  printf( "Tkeren_mac_from == %d\n", 	v_keren_mac_from );
  printf( "Tkeren_mac_until == %d\n", 	v_keren_mac_until );
  printf( "Tasaf_code == %d\n", 	v_asaf_code );
  printf( "Tinsurance_status == %d\n", 	v_insurance_status );
  printf( "Tdoctor_status == %d\n", 	v_doctor_status );
  printf( "Tcredit_type_code == %d\n", 	v_credit_type_code );
  printf( "Tmember_discount_pt == %d\n",v_member_discount_pt );
  printf( "Told_member_id == %d\n", 	v_old_member_id );
  printf( "dangerous_drug_status == %d\n", 	dangerous_drug_status );
  fflush( stdout );
  ********************************************************************/


	/* << -------------------------------------------------------- >> */
	/*	   up to SQL_UPDATE_RETRIES retries will be made	  */
	/*		    to overcome access conflict			  */
	/* << -------------------------------------------------------- >> */
	retcode = ALL_RETRIES_WERE_USED;
	stop_retrying = false;

	for (n_retries = 0; (n_retries < SQL_UPDATE_RETRIES) && (stop_retrying == false); n_retries++)
	{

		// Sleep for a while.
		if (n_retries)
		{
			sleep (SLEEP_TIME_BETWEEN_RETRIES);
		}

		// Try to insert/update new rec with the order depending on "do_insert".
		// DonR 22Feb2021: Because we're now "mirroring" the members table (at least for
		// a couple of months), we need to loop (potentially) 3 times instead of just 2.
		// This allows us to deal with a member who is present in one database but not in
		// the other. Here's what happens:
		//
		// i = 0: We try an UPDATE, and only one database gets UPDATEd. The ODBC routines should
		//        return the *lower* value for rows-affected, so HANDLE_UPDATE_SQL_CODE will
		//        switch us to INSERT mode.
		// i = 1: We try an INSERT, and only one database gets INSERTed. The ODBC routines should
		//        return the *worse* result code, so HANDLE_INSERT_SQL_CODE will switch us back
		//        to UPDATE mode.
		// i = 2: We try to UPDATE again, and this time we should succeed in *both* databases.
		// 
		for (i = 0;  i < 3;  i++)
		{
			if (do_insert == ATTEMPT_INSERT_FIRST)
			{
				// DonR 16Mar2010: See if by some chance we got a coupon ("shovar") row for this member before
				// we got the member him/herself. Note that we don't change the coupon flag when we update a member,
				// since the flag is normally set or unset by the coupons download.
				// DonR 19Aug2018 BUG FIX: While coding CR #15260, I noticed that we were setting the "has coupon"
				// flag TRUE even if the row in COUPONS was logically deleted - this was obviously not OK!
				ExecSQL (	MAIN_DB, AS400SRV_T2025_READ_coupons_count,
							&v_has_coupon,
							&v_member_id,		&v_mem_id_extension,
							&ROW_NOT_DELETED,	END_OF_ARG_LIST			);

				if ((SQLCODE == 0) && (v_has_coupon > 0))
					v_has_coupon = 1;	// Should already be one, since Member ID/Code is a unique key - just being paranoid!
				else
					v_has_coupon = 0;	// Just in case SQL put some funky value here.


				// DonR 17Oct2021 User Story #196891: See if the new member already has drugs blocked for purchase.
				ExecSQL (	MAIN_DB, AS400SRV_T2025_READ_blocked_drugs_count,
							&num_blocked_drugs,
							&v_member_id,		&v_mem_id_extension,
							END_OF_ARG_LIST								);

				if ((SQLCODE == 0) && (num_blocked_drugs > 0))
					has_blocked_drugs = 1;	// Convert real COUNT value to boolean.
				else
					has_blocked_drugs = 0;


				// DonR 02Aug2011: Check if new member is already a hospital inpatient, or has been discharged
				// from hospital.
				// DonR 23Aug2011: Added the in_hospital status flag to hospital_member, so we can read the member's
				// current status directly instead of figuring it out according to discharge date.
				ExecSQL (	MAIN_DB, AS400SRV_T2025_READ_hospital_member_in_hospital,
							&v_in_hospital,			&v_member_id,
							&v_mem_id_extension,	END_OF_ARG_LIST						);

				if (SQLCODE != 0)
				{
					v_in_hospital = 0;	// Default value: member is not a hospital patient.
				}


				// DonR 19Dec2018: See if this member already has AS/400 ishurim. This shouldn't happen normally,
				// but it can occur if a member is mistakenly deleted and then re-added. (This recently happened.)
				ExecSQL (	MAIN_DB, AS400SRV_T2025_READ_special_prescs_count,
							&v_spec_presc,			&v_member_id,
							&v_mem_id_extension,	END_OF_ARG_LIST				);

				if ((SQLCODE == 0) && (v_spec_presc > 0))
					v_spec_presc = 1;	// Convert real COUNT value to boolean.
				else
					v_spec_presc = 0;	// Just in case SQL put some funky value here.


				// DonR 13Feb2018: Instead of setting update_date and update_time, use the new
				// fields data_update_date and data_update_time. This leaves the old update
				// timestamp fields to be used exclusively for service-without-card stuff.
				// DonR 05Nov2019: Snif now always gets zero; the value from APPLIB/MEMBERS/KDSNIF
				// is now stored in check_od_interact, and indicates members who are "mivakrim"
				// and thus exempt from interaction/overdose checking.
				// DonR 25Nov2021: Added died_in_hospital to table - always with an initial value
				// of zero.
				ExecSQL (	MAIN_DB, AS400SRV_T2025_INS_members,
							&v_member_id,			&v_mem_id_extension,	&v_card_date,
							&v_father_name,			&v_first_name,			&v_last_name,
							&v_street,				&v_house_num,			&v_city,
							&v_phone,				&v_zip_code,			&v_sex,
							&v_date_of_bearth,		&v_marital_status,		&v_maccabi_code,

							&v_maccabi_until,		&v_mac_magen_code,		&v_mac_magen_from,
							&v_mac_magen_until,		&v_keren_macabi,		&v_keren_mac_from,
							&v_keren_mac_until,		&v_keren_wait_flag,		&v_asaf_code,
							&v_insurance_status,	&v_doctor_status,		&v_credit_type_code,
							&v_member_discount_pt,	&v_old_member_id,		&v_old_id_extension,

							&sysdate,				&systime,				&UpdatedBy,
							&ROW_NOT_DELETED,		&v_number_family,		&v_code_family,
							&v_update_address,		&v_last_english,		&v_first_english,
							&v_spec_presc,			&sysdate,				&systime,
							&v_mess_2025,			&IntZero,				&v_has_tikra,

							&v_has_coupon,			&v_in_hospital,			&v_yahalom_code,
							&v_yahalom_from,		&v_yahalom_until,		&v_vetek,
							&v_insurance_type,		&illness_1,				&illness_2,
							&illness_3,				&illness_4,				&illness_5,
							&illness_6,				&illness_7,				&illness_8,

							&illness_9,				&illness_10,			&illness_bitmap,
							&check_od_interact,		&force_100_percent_ptn,	&darkonai_no_card,
							&darkonai_type,			&has_blocked_drugs,		&ShortZero,
							&dangerous_drug_status,
							END_OF_ARG_LIST															);

				// DonR 18Jul2022 TEMPORARY LOGGING FOR DARKONAIM.
				if ((v_mem_id_extension == 9) && (SQLERR_code_cmp (SQLERR_not_unique) != MAC_TRUE))
				{
					GerrLogFnameMini ("darkonaim_log", GerrId, "Insert TZ %d Darkonai Type %d: SQLCODE = %d.", v_member_id, darkonai_type, SQLCODE);
				}

				HANDLE_INSERT_SQL_CODE ("2025", p_connect, &do_insert, &retcode, &stop_retrying);
			}	// Need to insert a new members row.

			else
			{
				// DonR 13Feb2018 HOT FIX: Do *not* update the "updated_by" field, since we're not
				// in fact getting a value from AS/400 for it. Updates to this field come from one
				// of the Linux-Doctors programs, and we don't want to overwrite those updates
				// with a meaningless value. The field is used to determine whether a "service
				// without card" request came from Haverut or from a member's app or website
				// request.
				// DonR 05Nov2019: Snif now always gets zero; the value from APPLIB/MEMBERS/KDSNIF
				// is now stored in check_od_interact, and indicates members who are "mivakrim"
				// and thus exempt from interaction/overdose checking.
				ExecSQL (	MAIN_DB, AS400SRV_T2025_UPD_members,
							&v_card_date,				&v_father_name,			&v_first_name,
							&v_last_name,				&v_street,				&v_house_num,
							&v_city,					&v_phone,				&v_zip_code,
							&v_sex,						&v_date_of_bearth,		&v_marital_status,

							&v_maccabi_code,			&v_maccabi_until,		&v_mac_magen_code,
							&v_mac_magen_from,			&v_mac_magen_until,		&v_keren_macabi,
							&v_keren_mac_from,			&v_keren_mac_until,		&v_keren_wait_flag,
							&v_asaf_code,				&v_insurance_status,	&v_doctor_status,

							&v_credit_type_code,		&v_member_discount_pt,	&v_old_member_id,
							&v_old_id_extension,		&sysdate,				&systime,
							&v_number_family,			&v_code_family,			&v_update_address,
							&v_last_english,			&v_first_english,		&v_mess_2025,

							&IntZero,					&check_od_interact,		&v_has_tikra,
							&v_yahalom_code,			&v_yahalom_from,		&v_yahalom_until,
							&v_vetek,					&v_insurance_type,		&illness_1,
							&illness_2,					&illness_3,				&illness_4,

							&illness_5,					&illness_6,				&illness_7,
							&illness_8,					&illness_9,				&illness_10,
							&illness_bitmap,			&force_100_percent_ptn,	&darkonai_no_card,
							&darkonai_type,				&dangerous_drug_status,

							&v_member_id,				&v_mem_id_extension,	END_OF_ARG_LIST			);

				// DonR 18Jul2022 TEMPORARY LOGGING FOR DARKONAIM.
				if ((v_mem_id_extension == 9) && (SQLERR_code_cmp (SQLERR_no_rows_affected) != MAC_TRUE))
				{
					GerrLogFnameMini ("darkonaim_log", GerrId, "Update TZ %d Darkonai Type %d: SQLCODE = %d.", v_member_id, darkonai_type, SQLCODE);
				}

				HANDLE_UPDATE_SQL_CODE ("2025", p_connect, &do_insert, &retcode, &stop_retrying);

			}	// Need to update existing row.

		}	// for( i... 

		//GerrLogFnameMini("2025_log",GerrId,"1:[%d][%d][%d]",
		//				 (long)v_member_id,
		//				 (long)v_doctor_status,
		//				 (long)SQLCODE);

		
		// DonR 29Jan2020: We need to check if all this conmemb logic is still needed!
		if (v_doctor_status == 0)
		{
			ExecSQL (	MAIN_DB, AS400SRV_T2025_DEL_conmemb,
						&v_member_id,		&v_mem_id_extension,
						&v_LinkType,		END_OF_ARG_LIST			);
		}
		else
		{
			Exist = 0;

			for (cnt = 0; cnt < NUM_TARIF_ROFE_REC; cnt++)
			{
				if (v_doctor_status != TarifRofeList[cnt].TarifRofe)
					continue;
				else
				{
					Exist = 1;
					break;
				}
			}

			if (Exist == 1)
			{
				ExecSQL (	MAIN_DB, AS400SRV_T2025_INS_conmemb,
							&v_member_id,							&v_mem_id_extension,
							&ShortZero,								&TarifRofeList[cnt].Contractor,
							&TarifRofeList[cnt].ContractType,		&TarifRofeList[cnt].ProfessionGroup,
							&TarifRofeList[cnt].LinkType,			&sysdate,
							&Forever,								&sysdate,
							&systime,								&FourSevens,
							&BlankString,							END_OF_ARG_LIST							);

//				GerrLogFnameMini ("2025_log", GerrId,
//								  "insert:cnt[%d][%d][%d][%d][%d][%d]prof[%d]sqlcode[%d]",
//								  cnt,
//								  (long)v_member_id,
//								  (long)v_doctor_status,
//								  (long)TarifRofeList[cnt].Contractor,
//								  NUM_TARIF_ROFE_REC,
//								  TarifRofeList[cnt].TarifRofe,
//								  TarifRofeList[cnt].ProfessionGroup,
//								  sqlca.sqlcode);
//

				if (SQLERR_code_cmp (SQLERR_not_unique) == MAC_TRUE)   
				{
					ExecSQL (	MAIN_DB, AS400SRV_T2025_UPD_conmemb,
								&TarifRofeList[cnt].Contractor,			&TarifRofeList[cnt].ContractType,
								&TarifRofeList[cnt].LinkType,			&sysdate,
								&systime,								&sysdate,
								&Forever,								&FourSevens,
								&BlankString,							&v_member_id,
								&v_mem_id_extension,					&ShortZero,
								&TarifRofeList[cnt].ProfessionGroup,	END_OF_ARG_LIST						);
//
//					GerrLogFnameMini ("2025_log", GerrId,
//									  "update:cnt[%d][%d][%d][%d][%d][%d]prof[%d]sqlcode[%d]",
//									  cnt,
//									  (long)v_member_id,
//									  (long)v_doctor_status,
//									  (long)TarifRofeList[cnt].Contractor,
//									  NUM_TARIF_ROFE_REC,
//									  TarifRofeList[cnt].TarifRofe,
//									  TarifRofeList[cnt].ProfessionGroup,  
//									  sqlca.sqlcode);
				}	// Conmemb row exists, so update it.	
			}	// Exist == 1.

			else //Exist == 0 not found
			{
				GerrLogFnameMini ("2025_NF_log", GerrId,
								  "not found:cnt[%d][%d][%d][%d][%d][%d]prof[%d]",
								  cnt,
								  v_member_id,
								  v_doctor_status,
								  TarifRofeList[cnt].Contractor,
								  NUM_TARIF_ROFE_REC,
								  TarifRofeList[cnt].TarifRofe,
								  TarifRofeList[cnt].ProfessionGroup);
			}
		}	// v_doctor_status <> 0

	}	// for (n_retries...


	if (n_retries == SQL_UPDATE_RETRIES)
	{
		WRITE_ACCESS_CONFLICT_TO_LOG ("2025");
	}

	if (retcode == MSG_HANDLER_SUCCEEDED)
	{
		if (do_insert == ATTEMPT_INSERT_FIRST)
			rows_added++;
		else
			rows_updated++;
	}


	return retcode;
} /* end of msg 2025 handler */


/* << -------------------------------------------------------- >> */
/*                                                                */
/*      handler for message 2026 - delete rec from MEMBERS	  */
/*                                                                */
/* << -------------------------------------------------------- >> */
static int message_2026_handler(
				TConnection	*p_connect,
				int		data_len
				)
{

/* << -------------------------------------------------------- >> */
/*			  local variables			  */
/* << -------------------------------------------------------- >> */
  char          *PosInBuff;		/* current pos in data buffer	    */
  int		ReadInput;		/* amount of Bytesread from buffer  */
  int		retcode;		/* code returned from function	    */
  int		n_retries;		/* retrying counter		    */
  int		i;
  int		db_changed_flg;


	/* << -------------------------------------------------------- >> */
	/*			    DB variables			  */
	/* << -------------------------------------------------------- >> */
	Tmember_id			v_member_id;
	Tmem_id_extension	v_mem_id_extension;


/* << -------------------------------------------------------- >> */
/*			  read input buffer			  */
/* << -------------------------------------------------------- >> */
  INIT_ERROR_DETECTION_MECHANISM;
  PosInBuff = glbMsgBuffer;

  v_member_id		=  get_long(
				    &PosInBuff,
				    Tmember_id_len
				    );
  RETURN_ON_ERROR;

  v_mem_id_extension	=  get_short(
				     &PosInBuff,
				     Tmem_id_extension_len
				     );
  RETURN_ON_ERROR;

/* << -------------------------------------------------------- >> */
/*			  debug messages			  */
/* << -------------------------------------------------------- >> */
  /************************* COMMENTED OUT:  ***************************
  printf( "member_id == %d\n",		v_member_id );
  printf( "mem_id_extension == %d\n",	v_mem_id_extension );
  fflush( stdout );
  *********************************************************************/


/* << -------------------------------------------------------- >> */
/*		      verify all deta was read			  */
/* << -------------------------------------------------------- >> */
  ReadInput = PosInBuff - glbMsgBuffer;
  StaticAssert( ReadInput == data_len );

/* << -------------------------------------------------------- >> */
/*			   attempt delete			  */
/* << -------------------------------------------------------- >> */
  retcode = ALL_RETRIES_WERE_USED;
  db_changed_flg = 0;

  for( n_retries = 0;
       n_retries < SQL_UPDATE_RETRIES;
       n_retries++ )
    {

      /*
       * sleep for a while:
       */
      if( n_retries )
      {
	  sleep( SLEEP_TIME_BETWEEN_RETRIES );
      }

		// DonR 02Aug2011: Delete any hospital_member row for this member. Don't bother checking for errors.
		ExecSQL (	MAIN_DB, AS400SRV_T2026_DEL_hospital_member,
					&v_member_id, &v_mem_id_extension, END_OF_ARG_LIST	);

		ExecSQL (	MAIN_DB, AS400SRV_T2026_DEL_members,
					&v_member_id, &v_mem_id_extension, END_OF_ARG_LIST	);

      HANDLE_DELETE_SQL_CODE( "2026", p_connect, &retcode, &db_changed_flg );

      break;
    }

  if( n_retries == SQL_UPDATE_RETRIES )
  {
     WRITE_ACCESS_CONFLICT_TO_LOG( "2026" );
  }

  if (retcode == MSG_HANDLER_SUCCEEDED)
	  rows_deleted++;


  return retcode;
} /* end of msg 2026 handler */



// << -------------------------------------------------------- >>
//
//		Handler for message 2029/4029 - upd/ins to MEMBER_DRUG_29G
//
//		This table is not sent to pharmacies or doctors, so no
//		testing is performed to see if data has changed.
//
// << -------------------------------------------------------- >>
static int data_4029_handler (TConnection	*p_connect,
							  int			data_len)
{
	// local variables
	int		ReadInput;			/* amount of Bytesread from buffer  */
	int		retcode;			/* code returned from function	    */
	int		n_retries;			/* retrying counter				    */
	int		i;
	bool	stop_retrying;		/* if true - return from function   */
	bool	something_changed = 0;
	short	do_insert = ATTEMPT_UPDATE_FIRST;


	// DB variables
	int		v_member_id;
	short	v_mem_id_extension;
	int		v_largo_code;
	int		v_start_date;
	int		v_end_date;
	int		v_pharmacy_code;
	short	v_form_29g_type;
	int		v_refresh_date;
	int		v_refresh_time;
	int		v_as400_batch_date;
	int		v_as400_batch_time;


	// Initialize timestamp variables.
	v_refresh_date		=  (download_start_date > 0) ? download_start_date : GetDate();
	v_refresh_time		=  (download_start_time > 0) ? download_start_time : GetTime();
	v_as400_batch_date	=  v_refresh_date;
	v_as400_batch_time	=  v_refresh_time;


	// read input buffer
	INIT_ERROR_DETECTION_MECHANISM;
	PosInBuff = glbMsgBuffer;

	GET_LONG	(v_member_id		,	9);
	GET_SHORT	(v_mem_id_extension	,	1);
	GET_LONG	(v_largo_code		,	9);
	GET_LONG	(v_start_date		,	8);
	GET_LONG	(v_end_date			,	8);
	GET_LONG	(v_pharmacy_code	,	7);
	GET_SHORT	(v_form_29g_type	,	2);

	// verify all data was read
	ReadInput = PosInBuff - glbMsgBuffer;
	StaticAssert( ReadInput == data_len );


	retcode			= ALL_RETRIES_WERE_USED;
	stop_retrying	= false;


	// Perform up to SQL_UPDATE_RETRIES retries to overcome access conflicts.
	for (n_retries = 0;
		 (n_retries < SQL_UPDATE_RETRIES) && (stop_retrying == false);
		 n_retries++)
	{
		// If this isn't the first iteration, sleep for a while.
		if (n_retries > 0)
		{
			sleep (SLEEP_TIME_BETWEEN_RETRIES);
		}

		for (i = 0; i < 2; i++)
		{
			if (do_insert == ATTEMPT_INSERT_FIRST)
			{
				ExecSQL (	MAIN_DB, AS400SRV_T4029_INS_member_drug_29g,
							&v_member_id,			&v_mem_id_extension,	&v_largo_code,
							&v_start_date,			&v_end_date,			&v_pharmacy_code,
							&v_form_29g_type,		&v_refresh_date,		&v_refresh_time,
							&v_as400_batch_date,	&v_as400_batch_time,	END_OF_ARG_LIST		);

				HANDLE_INSERT_SQL_CODE ("4029", p_connect, &do_insert, &retcode, &stop_retrying);
			}	// Need to insert new member_drug_29g row.

			// If the row is already present, it needs to be updated.
			else
			{
				ExecSQL (	MAIN_DB, AS400SRV_T4029_UPD_member_drug_29g,
							&v_start_date,			&v_end_date,
							&v_pharmacy_code,		&v_form_29g_type,
							&v_refresh_date,		&v_refresh_time,
							&v_as400_batch_date,	&v_as400_batch_time,
							&v_member_id,			&v_largo_code,
							&v_mem_id_extension,	END_OF_ARG_LIST			);

				HANDLE_UPDATE_SQL_CODE ("4029", p_connect, &do_insert, &retcode, &stop_retrying);
			}	// In update mode.
		}	// Insert/update loop.
	}	// Retry Loop.


	if (n_retries == SQL_UPDATE_RETRIES)
	{
		WRITE_ACCESS_CONFLICT_TO_LOG ("4029");
	}

  if (retcode == MSG_HANDLER_SUCCEEDED)
  {
	  if (do_insert == ATTEMPT_INSERT_FIRST)
	  {
		  rows_added++;
	  }
	  else
	  {
		  rows_updated++;
	  }
  }


	return retcode;

}	// end of msg 4029 handler


// -------------------------------------------------------------------------
//
//       Post-processor for message 4029 - delete MEMBER_DRUG_29G
//			rows that are no longer in the AS/400 full-table download.
//
// -------------------------------------------------------------------------
static int postprocess_4029 (TConnection *p_connect)
{
	// local variables
	bool	stop_retrying;	// if true - return from function
	int		rows_deleted	= 0;
	int		retcode;		// code returned from function
	int		n_retries;		// retrying counter
	int		db_changed_flg;

	// DB variables
	int				v_as400_batch_date;
	int				v_as400_batch_time;
	int				v_row_count;


	// Initialize variables.
	v_as400_batch_date	= download_start_date;
	v_as400_batch_time	= download_start_time;


	retcode			= ALL_RETRIES_WERE_USED;
	stop_retrying	= false;


	while (1)
	{
		ExecSQL (	MAIN_DB, AS400SRV_T4029PP_READ_member_drug_29g_stale_count,
					&v_row_count,			&v_as400_batch_date,
					&v_as400_batch_time,	END_OF_ARG_LIST						);

		if (SQLERR_error_test ())
		{
			retcode = SQL_ERROR_OCCURED;
			break;
		}
		else
		{
			if (v_row_count == 0)
			{
				retcode = MSG_HANDLER_SUCCEEDED;
				break;
			}
		}

		ExecSQL (	MAIN_DB, AS400SRV_T4029PP_DEL_member_drug_29g,
					&v_as400_batch_date, &v_as400_batch_time, END_OF_ARG_LIST	);

		HANDLE_DELETE_SQL_CODE ("Postprocess 4029", p_connect, &retcode, &db_changed_flg);

		break;
	}

	rows_deleted = v_row_count;


	// DonR 13Nov2018 CR #15919: Record current table size.
	REMEMBER_ISOLATION;
	SET_ISOLATION_DIRTY;

	ExecSQL (	MAIN_DB, AS400SRV_T4029PP_READ_member_drug_29g_table_size,
				&total_rows, END_OF_ARG_LIST								);

	active_rows = total_rows;

	RESTORE_ISOLATION;
	// DonR 13Nov2018 CR #15919 end.


	GerrLogMini (GerrId,
				 "Postprocess 4029: %d rows deleted, retcode = %d.",
				 rows_deleted, retcode);

	return retcode;

}	// End of msg 4029 postprocessor.



// << -------------------------------------------------------- >>
//
//     handler for message 2030/4030 - upd/ins to DRUG_EXTENSION
//
// << -------------------------------------------------------- >>
static int data_4030_handler (TConnection	*p_connect,
							  int			data_len)
{
	// local variables
	char	*PosInBuff;		// current pos in data buffer
	bool	stop_retrying;	// if true - return from function
	int		ReadInput;		// amount of Bytesread from buffer
	int		retcode;		// code returned from function
	int		n_retries;		// retrying counter
	int		i;
	bool	do_insert = ATTEMPT_UPDATE_FIRST;

	// DB variables

	// Drug Extension Table.
	Tlargo_code			v_largo_code;
	Textension_largo_co	v_extension_largo_co;
	int					v_rule_number;			// DonR 05Nov2003
	TTreatmentCategory	v_treatment_category;	// DonR 05Nov2003
	TGenericYYYYMMDD	v_effective_date;		// DonR 05Nov2003
	TGenericText75		v_rule_name;			// DonR 05Nov2003
	Tconfirm_authority_	v_confirm_authority;
	Tfrom_age			v_from_age;
	Tto_age				v_to_age;
	Tsex				v_sex;					// DonR 05Nov2003
	Tmust_confirm_deliv	v_must_confirm_deliv;
	Tmax_amount_duratio	v_max_amount_duratio;
	TPermissionType		v_permission_type;
	short int			v_sort_sequence;
	short int			v_no_presc_sale_flag;
	Tprice_code			v_basic_price_code;
	int					v_basic_fixed_price;
	Tprice_code			v_kesef_price_code;
	int					v_kesef_fixed_price;
	Tprice_code			v_zahav_price_code;
	int					v_zahav_fixed_price;
	Tprice_code			v_prev_bas_prc_cod;	// DonR 05Jan2012: Not really in use.
	Tprice_code			v_prev_ksf_prc_cod;	// DonR 05Jan2012: Not really in use.
	Tprice_code			v_prev_zhv_prc_cod;	// DonR 05Jan2012: Not really in use.
	Tprice_code			v_lookup_price_code;
	Tmax_op     		v_max_op_long;
	Tmax_units			v_max_units;
	TGenericFlag		v_in_health_basket;
	short int			v_extend_rule_days;
	int					v_drug_extn_rrn;
	int					v_changed_date;
	int					v_changed_time;
	int					v_as400_batch_date;
	int					v_as400_batch_time;
	char				v_ivf_flag	[1 + 1];
	short int			v_send_and_use;
	short				v_enabled_status;
	short				v_enabled_mac;
	short				v_enabled_hova;
	short				v_enabled_keva;
	short				v_ptn_percent_sort;
	short				v_ins_type_sort		= 0;
	short				v_yahalom_prc_code;
	int					v_fixed_pr_yahalom;
	short				v_wait_months;
	char				v_insurance_type [2];
	short				v_price_code;
	int					v_fixed_price;
	short				pharm_real_time;	// Marianna 09Feb2023 User Story #417219 


	// Previous values in Drug Extension table.
	short				o_enabled_status;
	Tlargo_code			o_largo_code;
	Textension_largo_co	o_extension_largo_co;
	int					o_rule_number;
	TTreatmentCategory	o_treatment_category;
	TGenericYYYYMMDD	o_effective_date;
	TGenericText75		o_rule_name;
	Tconfirm_authority_	o_confirm_authority;
	Tfrom_age			o_from_age;
	Tto_age				o_to_age;
	Tsex				o_sex;
	Tmust_confirm_deliv	o_must_confirm_deliv;
	Tmax_amount_duratio	o_max_amount_duratio;
	TPermissionType		o_permission_type;
	short int			o_sort_sequence;
	short int			o_no_presc_sale_flag;
	Tprice_code			o_basic_price_code;
	int					o_basic_fixed_price;
	Tprice_code			o_kesef_price_code;
	int					o_kesef_fixed_price;
	Tprice_code			o_zahav_price_code;
	int					o_zahav_fixed_price;
	Tmax_op     		o_max_op_long;
	Tmax_units			o_max_units;
	TGenericFlag		o_in_health_basket;
	TGenericFlag		o_HealthBasketNew;
	short int			o_extend_rule_days;
	int					o_drug_extn_rrn;
	int					o_changed_date;
	int					o_changed_time;
	short				o_del_flg;
	char				o_ivf_flag	[1 + 1];
	short int			o_send_and_use;
	short				o_yahalom_prc_code;
	int					o_fixed_pr_yahalom;
	short				o_wait_months;

	// Miscellaneous.
	int					sysdate;
	int					systime;


	sysdate = GetDate();
	systime = GetTime();
	v_as400_batch_date = (download_start_date > 0) ? download_start_date : sysdate;
	v_as400_batch_time = (download_start_time > 0) ? download_start_time : systime;


	// read input buffer
	INIT_ERROR_DETECTION_MECHANISM;
	PosInBuff = glbMsgBuffer;

	v_largo_code			= get_long	(&PosInBuff, 9							); RETURN_ON_ERROR;	//Marianna 14Jul2024 User Story #330877

	get_string (&PosInBuff, v_extension_largo_co, Textension_largo_co_len);

	v_rule_number			= get_long	(&PosInBuff, Trule_number_len			); RETURN_ON_ERROR;
	v_treatment_category	= get_short	(&PosInBuff, Ttreatment_category_len	); RETURN_ON_ERROR;
	v_effective_date		= get_long	(&PosInBuff, TGenericYYYYMMDD_len		); RETURN_ON_ERROR;

	get_string (&PosInBuff, v_rule_name, Trule_name_len);

	v_confirm_authority		= get_short	(&PosInBuff, Tconfirm_authority__len	); RETURN_ON_ERROR;
	v_from_age				= get_short	(&PosInBuff, Tfrom_age_len				); RETURN_ON_ERROR;
	v_to_age				= get_short	(&PosInBuff, Tto_age_len				); RETURN_ON_ERROR;
	v_sex					= get_short	(&PosInBuff, TGenericFlag_len			); RETURN_ON_ERROR;
	v_basic_price_code		= get_short	(&PosInBuff, Tprice_code_len			); RETURN_ON_ERROR;
	v_prev_bas_prc_cod		= get_short	(&PosInBuff, Tprice_code_len			); RETURN_ON_ERROR;	// DonR 05Jan2012: Not really in use.
	v_zahav_price_code		= get_short	(&PosInBuff, Tprice_code_len			); RETURN_ON_ERROR;
	v_prev_zhv_prc_cod		= get_short	(&PosInBuff, Tprice_code_len			); RETURN_ON_ERROR;	// DonR 05Jan2012: Not really in use.
	v_kesef_price_code		= get_short	(&PosInBuff, Tprice_code_len			); RETURN_ON_ERROR;
	v_prev_ksf_prc_cod		= get_short	(&PosInBuff, Tprice_code_len			); RETURN_ON_ERROR;	// DonR 05Jan2012: Not really in use.
	v_must_confirm_deliv	= get_short	(&PosInBuff, Tmust_confirm_deliv_len	); RETURN_ON_ERROR;
	v_max_op_long			= get_long	(&PosInBuff, Tmax_op_len				); RETURN_ON_ERROR;
	v_max_units				= get_long	(&PosInBuff, Tmax_units_len				); RETURN_ON_ERROR;
	v_max_amount_duratio	= get_short	(&PosInBuff, Tmax_amount_duratio_len	); RETURN_ON_ERROR;
	v_permission_type		= get_short	(&PosInBuff, TPermissionType_len		); RETURN_ON_ERROR;
	v_no_presc_sale_flag	= get_short	(&PosInBuff, Tno_presc_sale_flag_len	); RETURN_ON_ERROR;
	v_basic_fixed_price		= get_long	(&PosInBuff, Tfixed_price_len			); RETURN_ON_ERROR;
	v_kesef_fixed_price		= get_long	(&PosInBuff, Tfixed_price_len			); RETURN_ON_ERROR;
	v_zahav_fixed_price		= get_long	(&PosInBuff, Tfixed_price_len			); RETURN_ON_ERROR;	// DonR 29Dec2011
	v_in_health_basket		= get_short	(&PosInBuff, TGenericFlag_len			); RETURN_ON_ERROR;
	v_extend_rule_days		= get_short	(&PosInBuff, Textend_rule_days_len		); RETURN_ON_ERROR;
	get_string (&PosInBuff, v_ivf_flag, TGenericFlag_len);
	v_send_and_use			= get_short	(&PosInBuff, TGenericFlag_len			); RETURN_ON_ERROR;
	GET_SHORT	(v_enabled_status			, 2							);
	GET_SHORT	(v_yahalom_prc_code			, Tprice_code_len			);
	GET_LONG	(v_fixed_pr_yahalom			, Tfixed_price_len			);
	GET_SHORT	(v_wait_months				, Twait_months_len			);
	pharm_real_time			= get_short	(&PosInBuff, 1							);RETURN_ON_ERROR;	// Marianna 09Feb2023 User Story #417219 



	// Translate AS400 Treatment Category into Unix version.
	i = v_treatment_category;
	switch (i)
	{
		case TREAT_TYPE_AS400_FERTILITY:	v_treatment_category = TREATMENT_TYPE_FERTILITY;	break;

		case TREAT_TYPE_AS400_DIALYSIS:		v_treatment_category = TREATMENT_TYPE_DIALYSIS;		break;

		// Marianna 11May2025: User Story #410137
		case TREAT_TYPE_AS400_PREP:		v_treatment_category = TREATMENT_TYPE_PREP;	break;

		case TREAT_TYPE_AS400_SMOKING:	v_treatment_category = TREATMENT_TYPE_SMOKING;	break;

		// If we get an unrecognized value, set it to GENERAL.
		default:							v_treatment_category = TREATMENT_TYPE_GENERAL;		break;
	}

	// DonR 20Nov2003: Assign rule sort sequence based on Permission Type.
	// Priorities are as follows:
	//      Permission Type 1 (Maccabi Pharmacy only)
	//                      6 (Prati Plus or Maccabi Pharmacy)
	//                      0 (Private pharmacy only)
	//                      2 (all pharmacies)
	//                  other (unknown - sort last)
	switch (v_permission_type)
	{
		case 1:		v_sort_sequence =  100;	break; 
		case 6:		v_sort_sequence =  200;	break; 
		case 0:		v_sort_sequence =  300;	break; 
		case 2:		v_sort_sequence = 1000;	break; 
		default:	v_sort_sequence = 9999;
	}

	// DonR 24Nov2003: Oops! Forgot to put the Rule Name in correct order so it won't come out backwards.
	switch_to_win_heb ((unsigned char *)v_rule_name);

	// DonR 26Oct2008: Set "old" health-basket flag to 1 or 0 based on the value received
	// from AS400, and move the actual value received to the "new" health-basket flag
	// variable.
	PROCESS_HEALTH_BASKET;

	// DonR 09Oct2011: Translate v_enabled_status into individual enabled-flag variables.
	SET_TZAHAL_FLAGS;

	// DonR 24Nov2011: All but one of the three price-code fields should be zero - so add them together to get
	// the price code to look up in member_price.
	v_lookup_price_code = v_basic_price_code + v_kesef_price_code + v_zahav_price_code + v_yahalom_prc_code;


	// DonR 12Nov2012 - Yahalom processing.
	// Set Insurance Type flag based on which price-code field has a non-zero value.
	// DonR 20Oct2015: Add another sorting variable used to choose "best" insurance first (it will sort *descending*).
	if (v_basic_price_code > 0)
	{
		strcpy (v_insurance_type, "B");
		v_price_code	= v_basic_price_code;
		v_fixed_price	= v_basic_fixed_price;
		v_ins_type_sort	= 0;
	}
	else
	if (v_kesef_price_code > 0)
	{
		strcpy (v_insurance_type, "K");
		v_price_code	= v_kesef_price_code;
		v_fixed_price	= v_kesef_fixed_price;
		v_ins_type_sort	= 5;
	}
	else
	if (v_zahav_price_code > 0)
	{
		strcpy (v_insurance_type, "Z");
		v_price_code	= v_zahav_price_code;
		v_fixed_price	= v_zahav_fixed_price;
		v_ins_type_sort	= 10;
	}
	else
	if (v_yahalom_prc_code > 0)
	{
		strcpy (v_insurance_type, "Y");
		v_price_code	= v_yahalom_prc_code;
		v_fixed_price	= v_fixed_pr_yahalom;
		v_ins_type_sort	= 15;
	}
	else
	{
		strcpy (v_insurance_type, "U");	// Unknown - shouldn't ever happen.
		v_price_code	= 1;
		v_fixed_price	= 0;
		v_ins_type_sort	= -1;
	}

	// Just to be sure, force "generic" fixed price to zero if "generic" price code <> 1.
	if (v_price_code != 1)
		v_fixed_price	= 0;
	// DonR 12Nov2012 - Yahalom processing end.


	// debug messages
	/************************* COMMENTED OUT:  **************************
	printf( "Tlargo_code == %d\n", 	v_largo_code );
	printf( "Textension_largo_co == %s\n",v_extension_largo_co );
	printf( "Tconfirm_authority == %d\n",v_confirm_authority );
	printf( "Tfrom_age == %d\n", 		v_from_age );
	printf( "Tto_age == %d\n", 		v_to_age );
	printf( "Tbasic_price_code == %d\n",	v_basic_price_code );
	printf( "Tzahav_price_code == %d\n", 	v_zahav_price_code );
	printf( "Tkesef_price_code == %d\n", 	v_kesef_price_code );
	printf( "Tmust_confirm_deliv == %d\n",v_must_confirm_deliv );
	printf( "Tmax_op_long == %d\n", 	v_max_op_long );
	printf( "Tmax_units == %d\n", 	v_max_units );
	printf( "Tmax_amount_duratio == %d\n",v_max_amount_duratio );
	printf( "TPermissionType == %d\n",v_permission_type );
	fflush( stdout );
	**********************************************************************/

	// verify all data was read:
	ReadInput = PosInBuff - glbMsgBuffer;
	StaticAssert( ReadInput == data_len );


	// up to SQL_UPDATE_RETRIES retries will be made to overcome access conflict
	retcode = ALL_RETRIES_WERE_USED;
	stop_retrying = false;

	for (n_retries = 0; (n_retries < SQL_UPDATE_RETRIES) && (stop_retrying == false); n_retries++)
	{

		// sleep for a while:
		if (n_retries > 0)
		{
			sleep (SLEEP_TIME_BETWEEN_RETRIES);
		}

		// DonR 24Nov2011: Perform the lookup in member_price in order to set v_ptn_percent_sort, which is used
		// to retrieve the best member participation first without having to loop through all applicable rows.
		ExecSQL (	MAIN_DB, AS400SRV_READ_member_price_ptn_percent_sort,
					&v_ptn_percent_sort, &v_lookup_price_code, END_OF_ARG_LIST	);

		if (SQLERR_code_cmp (SQLERR_not_found) == MAC_TRUE)
		{
			v_ptn_percent_sort = 10001;	// Sort at end - 100.01% participation.
		}
		else
		{
			// Second possibility: there was an access conflict, so advance to the next retry iteration.
			if (SQLERR_code_cmp (SQLERR_access_conflict) == MAC_TRUE)
			{
				retcode = ALL_RETRIES_WERE_USED;
				continue;
			}
			else
			{
				// Third possibility: there was a database error, so don't retry - just give up.
				if (SQLERR_error_test ())
				{
					retcode = SQL_ERROR_OCCURED;
					break;
				}
			}
		}

		// DonR 05Nov2003: If this is a numbered rule, check DB to see if it's already present for this
		// Largo Code. Using the unique index of the table won't work here, because a change in the
		// age-bracket of the rule would make it (incorrectly) into a new rule rather than an update.
		// For numbered rules, we'll use the rule's RRN rather than other criteria to update the rule.
		if (v_rule_number > 0)
		{
			ExecSQL (	MAIN_DB, AS400SRV_T4030_READ_drug_extension_rrn,
						&v_drug_extn_rrn,		&v_largo_code,
						&v_rule_number,			END_OF_ARG_LIST				);

			if (SQLERR_code_cmp (SQLERR_access_conflict) == MAC_TRUE)
			{
				v_drug_extn_rrn = 0;	// Just in case SQL put some strange value there.
				retcode = ALL_RETRIES_WERE_USED;
				continue;
			}

			if (SQLERR_code_cmp (SQLERR_not_found) == MAC_TRUE)
			{
				v_drug_extn_rrn = 0;	// Just in case SQL put some strange value there.
			}
			else
			{
				// Severe DB error.
				if (SQLERR_error_test ())
				{
					retcode = SQL_ERROR_OCCURED;
					break;
				}
			}	// Good read OR severe error.

			// If we get here, we either succeeded or legitimately failed to read the rule.
			// Set do_insert accordingly.
			do_insert = (v_drug_extn_rrn > 0) ? ATTEMPT_UPDATE_FIRST : ATTEMPT_INSERT_FIRST;

		}	// Rule Number is non-zero.

		else	// Rule is of "standard" type.
		{
			v_drug_extn_rrn = 0;
		}


		// try to insert/update new rec with the order depending on "do_insert":
		for (i = 0; i < 2; i++)
		{
			if (do_insert == ATTEMPT_INSERT_FIRST)
			{
				ExecSQL (	MAIN_DB, AS400SRV_T4030_INS_drug_extension,
							&v_largo_code,			&v_extension_largo_co,		&v_rule_number,
							&v_treatment_category,	&v_effective_date,			&v_rule_name,
							&v_confirm_authority,	&v_from_age,				&v_to_age,
							&v_sex,					&v_must_confirm_deliv,		&v_max_amount_duratio,
							&v_permission_type,		&v_no_presc_sale_flag,		&v_basic_price_code,

							&v_basic_fixed_price,	&v_kesef_price_code,		&v_kesef_fixed_price,
							&v_zahav_price_code,	&v_zahav_fixed_price,		&v_ptn_percent_sort,
							&v_max_op_long,			&v_max_units,				&sysdate,
							&systime,				&sysdate,					&systime,
							&v_as400_batch_date,	&v_as400_batch_time,		&ROW_NOT_DELETED,

							&v_in_health_basket,	&v_HealthBasketNew,			&v_sort_sequence,
							&v_extend_rule_days,	&v_ivf_flag,				&v_enabled_status,
							&v_enabled_mac,			&v_enabled_hova,			&v_enabled_keva,
							&v_send_and_use,		&v_yahalom_prc_code,		&v_fixed_pr_yahalom,
							&v_wait_months,			&v_insurance_type,			&v_price_code,
							&v_fixed_price,			&v_ins_type_sort,			END_OF_ARG_LIST			);

				HANDLE_INSERT_SQL_CODE ("2030", p_connect, &do_insert, &retcode, &stop_retrying);
			}

			else	// Update existing row.
			{
				// DonR 05Nov2003: Note the particularly evil WHERE clause in the SQL statement
				// below. It works just as it used to (updating based on Largo Code, Permission
				// Type, and From Age) if v_drug_extn_rrn is zero; but if v_drug_extn_rrn is
				// non-zero (meaning that we're updating a numbered rule) the first three
				// selects are disabled (that is, always true) and the fourth select (by RRN) is
				// enabled (that is, not always true). I feel deeply ashamed for having thought
				// this one up. <g>
				// DonR 08Jul2004: Added one more condition to the WHERE clause, to prevent
				// rules with rule_number set to zero from updating rules with non-zero rule
				// numbers.

				// DonR 25Aug2004: Restored check for actual data changes.
				// DonR 14Dec2017: Split the SQL into two separate versions, just to (I hope!) speed up
				// execution. In all or almost all real-world cases, v_drug_extn_rrn *will* be non-zero.
				if (v_drug_extn_rrn > 0)
				{
					ExecSQL (	MAIN_DB, AS400SRV_T4030_READ_drug_extension_old_data,
								AS400SRV_T4030_READ_drug_extension_old_data_by_rrn,
								&o_extension_largo_co,	&o_effective_date,		&o_rule_name,
								&o_confirm_authority,	&o_from_age,			&o_to_age,
								&o_sex,					&o_basic_price_code,	&o_basic_fixed_price,
								&o_kesef_price_code,	&o_kesef_fixed_price,	&o_zahav_price_code,

								&o_zahav_fixed_price,	&o_must_confirm_deliv,	&o_max_op_long,
								&o_max_units,			&o_max_amount_duratio,	&o_permission_type,
								&o_no_presc_sale_flag,	&o_treatment_category,	&o_changed_date,
								&o_changed_time,		&o_del_flg,				&o_in_health_basket,

								&o_HealthBasketNew,		&o_sort_sequence,		&o_extend_rule_days,
								&o_ivf_flag,			&o_enabled_status,		&o_send_and_use,
								&o_yahalom_prc_code,	&o_fixed_pr_yahalom,	&o_wait_months,
								&v_drug_extn_rrn,

								&v_drug_extn_rrn,		END_OF_ARG_LIST									);
				}
				else
				{
					ExecSQL (	MAIN_DB, AS400SRV_T4030_READ_drug_extension_old_data,
								AS400SRV_T4030_READ_drug_extension_old_data_by_largo,
								&o_extension_largo_co,	&o_effective_date,		&o_rule_name,
								&o_confirm_authority,	&o_from_age,			&o_to_age,
								&o_sex,					&o_basic_price_code,	&o_basic_fixed_price,
								&o_kesef_price_code,	&o_kesef_fixed_price,	&o_zahav_price_code,
								&o_zahav_fixed_price,	&o_must_confirm_deliv,	&o_max_op_long,

								&o_max_units,			&o_max_amount_duratio,	&o_permission_type,
								&o_no_presc_sale_flag,	&o_treatment_category,	&o_changed_date,
								&o_changed_time,		&o_del_flg,				&o_in_health_basket,
								&o_HealthBasketNew,		&o_sort_sequence,		&o_extend_rule_days,
								&o_ivf_flag,			&o_enabled_status,		&o_send_and_use,
								&o_yahalom_prc_code,	&o_fixed_pr_yahalom,	&o_wait_months,

								&v_largo_code,			&v_rule_number,			&v_from_age,
								&v_permission_type,		END_OF_ARG_LIST									);
				}

				// By default, assume that something has changed.
				// DonR 18Aug2011: If we're downloading from RKFILPRD, we want to update pharmacies
				// even if nothing "real" has changed. In this case download_start_date will be zero;
				// so we can force the update by making non-zero download_start_date one of the
				// conditions for retaining the previous changed_date/changed_time.
				// DonR 22Jan2012: Temporarily disable the logic to force an update for all RKFILPRD
				// downloads, since every time QBATCH2 is restarted the whole table is dumped to
				// RKFILPRD.
				v_changed_date = sysdate;
				v_changed_time = systime;

				strip_trailing_spaces (v_rule_name);	// DonR 09Feb2020: Since ODBC (at least for Informix) does the same automatically on SELECT.

				if ((SQLERR_code_cmp (SQLERR_ok) == MAC_TRUE			)	&&
//					(download_start_date	>		0					)	&&	// DonR 18Aug2011.
					(pharm_real_time		==		0					)	&&	// Marianna 12Feb2023 User Story #417219
					(!strcmp (v_extension_largo_co,	o_extension_largo_co))	&&
					(!strcmp (v_rule_name,			o_rule_name			))	&&
					(!strcmp (v_ivf_flag,			o_ivf_flag			))	&&
					(v_effective_date		==		o_effective_date	)	&&
					(v_confirm_authority	==		o_confirm_authority	)	&&
					(v_from_age				==		o_from_age			)	&&
					(v_to_age				==		o_to_age			)	&&
					(v_sex					==		o_sex				)	&&
					(v_basic_price_code		==		o_basic_price_code	)	&&
					(v_basic_fixed_price	==		o_basic_fixed_price	)	&&
					(v_kesef_price_code		==		o_kesef_price_code	)	&&
					(v_kesef_fixed_price	==		o_kesef_fixed_price	)	&&
					(v_zahav_price_code		==		o_zahav_price_code	)	&&
					(v_zahav_fixed_price	==		o_zahav_fixed_price	)	&&
					(v_yahalom_prc_code		==		o_yahalom_prc_code	)	&&
					(v_fixed_pr_yahalom		==		o_fixed_pr_yahalom	)	&&
					(v_wait_months			==		o_wait_months		)	&&
					(v_must_confirm_deliv	==		o_must_confirm_deliv)	&&
					(v_max_op_long			==		o_max_op_long		)	&&
					(v_max_units			==		o_max_units			)	&&
					(v_max_amount_duratio	==		o_max_amount_duratio)	&&
					(v_permission_type		==		o_permission_type	)	&&
					(v_no_presc_sale_flag	==		o_no_presc_sale_flag)	&&
					(v_treatment_category	==		o_treatment_category)	&&
					(o_del_flg				==		0					)	&&
					(v_in_health_basket		==		o_in_health_basket	)	&&
					(v_HealthBasketNew		==		o_HealthBasketNew	)	&&
					(v_extend_rule_days		==		o_extend_rule_days	)	&&
					(v_send_and_use			==		o_send_and_use		)	&&
					(v_enabled_status		==		o_enabled_status	)	&&
					(v_sort_sequence		==		o_sort_sequence		))
				{
					// Nothing's changed after all!
					v_changed_date = o_changed_date;
					v_changed_time = o_changed_time;
				}



				ExecSQL (	MAIN_DB, AS400SRV_T4030_UPD_drug_extension,
							&v_extension_largo_co,		&v_effective_date,
							&v_rule_name,				&v_confirm_authority,
							&v_from_age,				&v_to_age,
							&v_sex,						&v_basic_price_code,
							&v_basic_fixed_price,		&v_kesef_price_code,
							&v_kesef_fixed_price,		&v_zahav_price_code,
							&v_zahav_fixed_price,		&v_yahalom_prc_code,
							&v_fixed_pr_yahalom,		&v_wait_months,
							&v_insurance_type,			&v_price_code,
							&v_fixed_price,				&v_ptn_percent_sort,
							&v_ins_type_sort,			&v_must_confirm_deliv,
							&v_max_op_long,				&v_max_units,
							&v_max_amount_duratio,		&v_permission_type,
							&v_no_presc_sale_flag,		&v_treatment_category,
							&sysdate,					&systime,
							&v_changed_date,			&v_changed_time,
							&v_as400_batch_date,		&v_as400_batch_time,
							&ROW_NOT_DELETED,			&v_in_health_basket,
							&v_HealthBasketNew,			&v_sort_sequence,
							&v_extend_rule_days,		&v_ivf_flag,
							&v_enabled_status,			&v_enabled_mac,
							&v_enabled_hova,			&v_enabled_keva,
							&v_send_and_use,
							&v_drug_extn_rrn,			END_OF_ARG_LIST			);

				HANDLE_UPDATE_SQL_CODE ("2030", p_connect, &do_insert, &retcode, &stop_retrying);
			}

		}/* for( i... */

	}/* for( n_retries... */


	if (n_retries == SQL_UPDATE_RETRIES)
	{
		WRITE_ACCESS_CONFLICT_TO_LOG ("2030");
	}

	if (retcode == MSG_HANDLER_SUCCEEDED)
	{
		if (do_insert == ATTEMPT_INSERT_FIRST)
			rows_added++;
		else
			if ((v_changed_date == o_changed_date) && (v_changed_time == o_changed_time))
				rows_refreshed++;
			else
				rows_updated++;
	}


	return retcode;
} /* end of msg 2030 handler */


/* << -------------------------------------------------------- >> */
/*                                                                */
/*   handler for message 2043 - delete rec from DRUG_EXTENSION	  */
/*                                                                */
/* << -------------------------------------------------------- >> */
static int message_2043_handler(
				TConnection	*p_connect,
				int		data_len
				)
{

/* << -------------------------------------------------------- >> */
/*			  local variables			  */
/* << -------------------------------------------------------- >> */
  char      *PosInBuff;		/* current pos in data buffer	    */
  int		ReadInput;		/* amount of Bytesread from buffer  */
  int		retcode;		/* code returned from function	    */
  int		n_retries;		/* retrying counter		    */
  int		i;
  int		do_insert;		/* ignored */
  int		stop_retrying;
  int		db_changed_flg;


	/* << -------------------------------------------------------- >> */
	/*			    DB variables			  */
	/* << -------------------------------------------------------- >> */
	Tlargo_code			v_largo_code;
	int					v_rule_number;			// DonR 05Nov2003
	int					SysDate;
	int					SysTime;


/* << -------------------------------------------------------- >> */
/*			  read input buffer			  */
/* << -------------------------------------------------------- >> */
  INIT_ERROR_DETECTION_MECHANISM;
  PosInBuff = glbMsgBuffer;

	v_largo_code			= get_long	(&PosInBuff, 9							); RETURN_ON_ERROR;	//Marianna 14Jul2024 User Story #330877
	v_rule_number			= get_long	(&PosInBuff, Trule_number_len			); RETURN_ON_ERROR;

/* << -------------------------------------------------------- >> */
/*			  debug messages			  */
/* << -------------------------------------------------------- >> */
   /************************* COMMENTED OUT: *************************
  printf( "largo_code == %d\n",	v_largo_code );
  fflush( stdout );
  *******************************************************************/

/* << -------------------------------------------------------- >> */
/*		      verify all deta was read			  */
/* << -------------------------------------------------------- >> */
  ReadInput = PosInBuff - glbMsgBuffer;
  StaticAssert( ReadInput == data_len );


/* << -------------------------------------------------------- >> */
/*			   attempt delete			  */
/* << -------------------------------------------------------- >> */
  retcode = ALL_RETRIES_WERE_USED;
  db_changed_flg = 0;

  stop_retrying = false;
  SysDate = GetDate ();
  SysTime = GetTime ();

  for(  n_retries = 0;
       ( n_retries < SQL_UPDATE_RETRIES ) && ( stop_retrying == false );
       n_retries++ )
    {

      /*
       * sleep for a while:
       */
      if( n_retries )
      {
	  sleep( SLEEP_TIME_BETWEEN_RETRIES );
      }

		ExecSQL (	MAIN_DB, AS400SRV_T2043_DEL_drug_extension,
					&SysDate,			&SysTime,		&SysDate,
					&SysTime,			&v_largo_code,	&v_rule_number,
					END_OF_ARG_LIST											);

GerrLogFnameMini("2043_log",GerrId,"Deleting Rule #%d for Largo %d - SQLCODE = %d.",
				 v_rule_number,
				 v_largo_code,
				 SQLCODE);

	  if( SQLERR_code_cmp( SQLERR_access_conflict ) == MAC_TRUE )
	  {
	      retcode = ALL_RETRIES_WERE_USED;
		  continue;
	  }
	  if( SQLERR_code_cmp( SQLERR_no_rows_affected ) == MAC_TRUE )
	  {
		  if( ! sqlca.sqlcode )
		  {
			  retcode = MSG_HANDLER_SUCCEEDED;
			  break;	// No problem - just exit happily.
		  }
	  }
	  if( SQLERR_error_test() == MAC_OK )
	  {
		  retcode = MSG_HANDLER_SUCCEEDED;
	  }
	  else
	  {
		  retcode = SQL_ERROR_OCCURED;
		  p_connect->sql_err_flg = 1;
	  }
	  stop_retrying = true;
	  break;
    }

  if( n_retries == SQL_UPDATE_RETRIES )
  {
     WRITE_ACCESS_CONFLICT_TO_LOG( "2043" );
  }

  if (retcode == MSG_HANDLER_SUCCEEDED)
	  rows_deleted++;


  return retcode;
} /* end of msg 2043 handler */


// -------------------------------------------------------------------------
//
//       Post-processor for message 4030 - logically delete DRUG_EXTENSION
//			rows that are no longer in the AS/400 full-table download.
//
// -------------------------------------------------------------------------
static int postprocess_4030 (TConnection *p_connect)
{
	// local variables
	bool	stop_retrying;	// if true - return from function
	int		rows_deleted	= 0;
	int		retcode;		// code returned from function
	int		n_retries;		// retrying counter
	int		db_changed_flg;

	// DB variables
	int				v_as400_batch_date;
	int				v_as400_batch_time;
	int				v_row_count;
	int				sysdate;
	int				systime;


	sysdate = GetDate();
	systime = GetTime();


	// Initialize variables.
	v_as400_batch_date	= download_start_date;
	v_as400_batch_time	= download_start_time;


	retcode			= ALL_RETRIES_WERE_USED;
	stop_retrying	= false;


	while (1)
	{
		ExecSQL (	MAIN_DB, AS400SRV_T4030PP_READ_drug_extension_stale_count,
					&v_row_count,			&v_as400_batch_date,
					&v_as400_batch_time,	END_OF_ARG_LIST						);

		if (SQLERR_error_test ())
		{
			retcode = SQL_ERROR_OCCURED;
			break;
		}
		else
		{
			if (v_row_count == 0)
			{
				retcode = MSG_HANDLER_SUCCEEDED;
				break;
			}
		}

		ExecSQL (	MAIN_DB, AS400SRV_T4030PP_UPD_drug_extension_logical_delete,
					&sysdate,				&systime,
					&v_as400_batch_date,	&v_as400_batch_time,
					END_OF_ARG_LIST													);

		HANDLE_DELETE_SQL_CODE ("Postprocess 4030", p_connect, &retcode, &db_changed_flg);

		break;
	}

	rows_deleted = v_row_count;


	// DonR 13Nov2018 CR #15919: Record current table size.
	REMEMBER_ISOLATION;
	SET_ISOLATION_DIRTY;

	ExecSQL (	MAIN_DB, AS400SRV_T4030PP_READ_drug_extension_table_size,
				&total_rows, END_OF_ARG_LIST								);

	ExecSQL (	MAIN_DB, AS400SRV_T4030PP_READ_drug_extension_active_size,
				&active_rows, END_OF_ARG_LIST								);

	RESTORE_ISOLATION;
	// DonR 13Nov2018 CR #15919 end.


	GerrLogMini (GerrId,
				 "Postprocess 4030: %d rows logically deleted, retcode = %d.",
				 rows_deleted, retcode);

	return retcode;

}	// End of msg 4030 postprocessor.



/* << -------------------------------------------------------- >> */
/*                                                                */
/*      handler for message 2031 - upd/ins to PRICE_LIST	  */
/*                                                                */
/* << -------------------------------------------------------- >> */
static int data_2031_handler (TConnection	*p_connect,
							  int			data_len)
{

	/* << -------------------------------------------------------- >> */
	/*			  local variables			  */
	/* << -------------------------------------------------------- >> */
	char	*PosInBuff;		/* current pos in data buffer	    */
	int		ReadInput;		/* amount of Bytesread from buffer  */
	int		retcode;		/* code returned from function	    */
	bool	stop_retrying;	/* if true - return from function   */
	bool	err_flag;		/* if true - exit from internal loop*/ 
	bool	something_changed;
	bool	do_insert = ATTEMPT_UPDATE_FIRST;
	int		fetch_count;	/* fetching counter                 */
	int		n_retries;		/* retrying counter				    */
	int		i;

	/* << -------------------------------------------------------- >> */
	/*			    DB variables			  */
	/* << -------------------------------------------------------- >> */
	Tprice_list_code	v_price_list_code;
	Tlargo_code			v_largo_code;
	Tmacabi_price		v_macabi_price;
	Tyarpa_price		v_yarpa_price;
	Tsupplier_price		v_supplier_price;
	Tsupplier_price		v_supplier_yarpa; /*20021111Yulia*/
	int					v_date_yarpa	= 0,
						v_time_yarpa	= 0,
						v_date_macabi	= 0,
						v_time_macabi	= 0,
						v_changed_date	= 0,
						v_changed_time	= 0,
						v_update_date	= 0,
						v_update_time	= 0;
	int					sysdate	= 0,
						systime	= 0;
	int					v_as400_batch_date;
	int					v_as400_batch_time;

	Tmacabi_price		old_macabi_price;
	Tyarpa_price		old_yarpa_price;
	Tsupplier_price		old_supplier_price;
	Tsupplier_price		old_supplier_yarpa;
	int					old_date_yarpa		= 0,
						old_time_yarpa		= 0,
						old_date_macabi		= 0,
						old_time_macabi		= 0,
						old_changed_date	= 0,
						old_changed_time	= 0,
						old_update_date		= 0,
						old_update_time		= 0;
	
	short				pharm_real_time;	// Marianna 09Feb2023 User Story #417219 		


	/*===============================================================
	||		Initialize global array with						   || 
	||     last date and time updating if not initialized          ||
	===============================================================*/
	if ( PriceArrayInitilized != true )
	{
		initGlbPriceListArray ();
		PriceArrayInitilized = true;
	}

	sysdate = GetDate();
	systime = GetTime();
	v_as400_batch_date = (download_start_date > 0) ? download_start_date : sysdate;
	v_as400_batch_time = (download_start_time > 0) ? download_start_time : systime;

	/* << -------------------------------------------------------- >> */
	/*			read input buffer			  */
	/* << -------------------------------------------------------- >> */
	INIT_ERROR_DETECTION_MECHANISM;
	PosInBuff = glbMsgBuffer;

	//printf("\n 2031 [%s]",PosInBuff);fflush(stdout);
	v_price_list_code	= get_short	(&PosInBuff, Tprice_list_code_len	); RETURN_ON_ERROR;
	v_largo_code		= get_long	(&PosInBuff, 9						); RETURN_ON_ERROR;	//Marianna 14Jul2024 User Story #330877
	v_macabi_price		= get_long	(&PosInBuff, Tmacabi_price_len		); RETURN_ON_ERROR;
	v_yarpa_price		= get_long	(&PosInBuff, Tyarpa_price_len		); RETURN_ON_ERROR;
	v_supplier_price	= get_long	(&PosInBuff, Tsupplier_price_len	); RETURN_ON_ERROR;
	v_date_yarpa		= get_long	(&PosInBuff, Tdate_yarpa_len		); RETURN_ON_ERROR;
	v_time_yarpa		= get_long	(&PosInBuff, Ttime_yarpa_len		); RETURN_ON_ERROR;
	v_date_macabi		= get_long	(&PosInBuff, Tdate_macabi_len		); RETURN_ON_ERROR;
	v_time_macabi		= get_long	(&PosInBuff, Ttime_macabi_len		); RETURN_ON_ERROR;
	v_supplier_yarpa	= get_long	(&PosInBuff, Tsupplier_price_len	); RETURN_ON_ERROR;
	pharm_real_time		= get_short	(&PosInBuff, 1						); RETURN_ON_ERROR;	// Marianna 09Feb2023 User Story #417219 


	/* << -------------------------------------------------------- >> */
	/*			    debug messages			  */
	/* << -------------------------------------------------------- >> */
	/************************* COMMENTED OUT:  *************************
	printf( "Tprice_list_code == %d\n", 	v_price_list_code );
	printf( "Tlargo_code == %d\n", 	v_largo_code );
	printf( "Tmacabi_price == %d\n", 	v_macabi_price );
	printf( "Tyarpa_price == %d\n", 	v_yarpa_price );
	printf( "Tsupplier_price == %d\n", 	v_supplier_price );
	printf( "date_yarpa == %d\n", 	v_date_yarpa );
	printf( "time_yarpa == %d\n", 	v_time_yarpa );
	printf( "date_macabi == %d\n", 	v_date_macabi );
	printf( "time_macabi == %d\n", 	v_time_macabi );
	fflush( stdout );
	*********************************************************************/

	// verify all data was read
	ReadInput = PosInBuff - glbMsgBuffer;
	StaticAssert( ReadInput == data_len );


	/* << -------------------------------------------------------- >> */
	/*	   up to SQL_UPDATE_RETRIES retries will be made	  */
	/*		    to overcome access conflict			  */
	/* << -------------------------------------------------------- >> */
	retcode = ALL_RETRIES_WERE_USED;
	stop_retrying = false;

	for (n_retries = 0;	(n_retries < SQL_UPDATE_RETRIES) && (stop_retrying == false); n_retries++)
	{
		// If this isn't the first attempt, sleep for a while.
		if (n_retries)
		{
			sleep (SLEEP_TIME_BETWEEN_RETRIES);
		}

		ExecSQL (	MAIN_DB, AS400SRV_T4031_READ_price_list_old_data,
					&old_macabi_price,		&old_yarpa_price,	&old_supplier_price,
					&old_supplier_yarpa,	&old_date_yarpa,	&old_time_yarpa,
					&old_date_macabi,		&old_time_macabi,	&old_changed_date,
					&old_changed_time,		&old_update_date,	&old_update_time,
					&v_price_list_code,		&v_largo_code,		END_OF_ARG_LIST			);
	
		if (SQLERR_code_cmp (SQLERR_not_found) == MAC_TRUE)
		{
			something_changed = 1;
 			do_insert = ATTEMPT_INSERT_FIRST; 
		}
		else if (SQLERR_code_cmp (SQLERR_access_conflict) == MAC_TRUE)
		{
			retcode = ALL_RETRIES_WERE_USED;
			continue;
		}
		else if (SQLERR_error_test())
		{
			retcode = SQL_ERROR_OCCURED;
			break;
		}
		else   // No errors on fetch
		{
			do_insert = ATTEMPT_UPDATE_FIRST;

			// DonR 15Jan2012: For now, don't do anything special when the download comes from RKFILPRD -
			// in other words, treat this as incremental data no matter where the update came from.
			// Note that in Transaction 1014 (Pharmacy Price List Download) the only timestamp that's tested
			// is date_macabi/time_macabi.
//			something_changed =	(download_start_date == 0);
			v_changed_date = old_changed_date;
			v_changed_time = old_changed_time;
			something_changed = 0;

			if (v_macabi_price == old_macabi_price) 
			{
				v_date_macabi = old_date_macabi;
				v_time_macabi = old_time_macabi;
			}
			else
			{
				something_changed = 1;
				v_date_macabi = v_changed_date = sysdate;
				v_time_macabi = v_changed_time = systime;
			}

			if (v_yarpa_price == old_yarpa_price)
			{
				v_date_yarpa = old_date_yarpa;
				v_time_yarpa = old_time_yarpa;
			}
			else
			{
				something_changed = 1;
				v_date_yarpa = v_changed_date = sysdate;
				v_time_yarpa = v_changed_time = systime;
			}

			if (v_supplier_price == old_supplier_price)
			{
				v_update_date = old_update_date;
				v_update_time = old_update_time;
			}
			else
			{
				something_changed = 1;
				v_date_macabi = v_update_date = v_changed_date = sysdate;
				v_time_macabi = v_update_time = v_changed_time = systime;
			}

			//	Marianna 12Feb2023 User Story #417219: add pharm_real_time if it's set non-zero,
			//	field needs to be refreshed even if no fields have changed.
			if (pharm_real_time > 0)
			{
				v_date_macabi = v_update_date = v_changed_date = sysdate;
				v_time_macabi = v_update_time = v_changed_time = systime;

				v_date_yarpa = v_changed_date = sysdate;
				v_time_yarpa = v_changed_time = systime;
			}

		}	// Successful read of existing price_list row.	


		// try to insert/update new rec with the order depending on "do_insert":
		for (i = 0, err_flag = false; (i < 2) && (!err_flag); i++)
		{
			err_flag = false;

			if (do_insert == ATTEMPT_INSERT_FIRST)
			{
				//GerrLogReturn (GerrId,"2031: Inserting new row into price_list.");

				//printf("\n 2031 insert");fflush(stdout);

				// Set to 0 supplier price, update date & time
				// for private pharmacies.
				// DonR 15Jan2012: Why is this done only for INSERTing a new row, and not for UPDATEs as well?
				// Either it should happen for both, or for neither!
				if ( v_price_list_code == 1 )
				{
					v_supplier_price = 0;
				}


				ExecSQL (	MAIN_DB, AS400SRV_T4031_INS_price_list,
							&v_price_list_code,		&v_largo_code,			&v_macabi_price,
							&v_yarpa_price,			&v_supplier_price,		&v_supplier_yarpa,
							&sysdate,				&systime,				&sysdate,
							&systime,				&sysdate,				&systime,
							&sysdate,				&systime,				&sysdate,
							&systime,				&v_as400_batch_date,	&v_as400_batch_time,
							&ROW_NOT_DELETED,		END_OF_ARG_LIST									);

				HANDLE_INSERT_SQL_CODE ("4031", p_connect, &do_insert, &retcode, &stop_retrying);

			}


			else
			{
				// Update rather than Insert.

				//GerrLogReturn (GerrId,"2031: Updating row in price_list.");
				//printf ("\n 2031 update"); fflush(stdout);


				ExecSQL (	MAIN_DB, AS400SRV_T4031_UPD_price_list,
							&v_macabi_price,		&v_yarpa_price,			&v_supplier_price,
							&v_supplier_yarpa,		&v_date_yarpa,			&v_time_yarpa,
							&v_date_macabi,			&v_time_macabi,			&sysdate,
							&systime,				&v_changed_date,		&v_changed_time,
							&v_as400_batch_date,	&v_as400_batch_time,	&v_update_date,
							&v_update_time,
							&v_price_list_code,		&v_largo_code,			END_OF_ARG_LIST		);

				HANDLE_UPDATE_SQL_CODE ("4031",
										p_connect,
										&do_insert,
										&retcode,
										&stop_retrying);

			}	// Update rather than Insert.

		}/* for( i... */

	}/* for( n_retries... */


	if( n_retries == SQL_UPDATE_RETRIES )
	{
		WRITE_ACCESS_CONFLICT_TO_LOG( "4031" );
	}


	// glbPriceListUpdateArray containes date & time values to
	// be entered into TABLES_UPDATE for "Price_list" & "Price_list_macabi"

	if ((glbPriceListUpdateArray [0] < v_date_yarpa)			||
		((glbPriceListUpdateArray [0] == v_date_yarpa)		&&
		 (glbPriceListUpdateArray [1] <  v_time_yarpa)))
	{
		glbPriceListUpdateArray[0] = v_date_yarpa;
		glbPriceListUpdateArray[1] = v_time_yarpa;
	}

	if ((glbPriceListUpdateArray [2] < v_date_macabi)			||
		((glbPriceListUpdateArray [2] == v_date_macabi)		&&
		 (glbPriceListUpdateArray [3] <  v_time_macabi)))
	{
		glbPriceListUpdateArray[2] = v_date_macabi;
		glbPriceListUpdateArray[3] = v_time_macabi;
	}

	if ((glbPriceListUpdateArray [2] < v_update_date)			||
		((glbPriceListUpdateArray [2] == v_update_date)		&&
		 (glbPriceListUpdateArray [3] <  v_update_time)))
	{
		glbPriceListUpdateArray[2] = v_update_date;
		glbPriceListUpdateArray[3] = v_update_time;
	}


	if (retcode == MSG_HANDLER_SUCCEEDED)
	{
		if (do_insert == ATTEMPT_INSERT_FIRST)
			rows_added++;
		else
			if (something_changed)
				rows_updated++;
			else
				rows_refreshed++;
	}


	return retcode;

} /* end of msg 2031/4031 handler */


/* << -------------------------------------------------------- >> */
/*                                                                */
/*      handler for message 2042 - delete rec from PRICE_LIST	  */
/*                                                                */
/* << -------------------------------------------------------- >> */
static int message_2042_handler(
				TConnection	*p_connect,
				int		data_len
				)
{

/* << -------------------------------------------------------- >> */
/*			  local variables			  */
/* << -------------------------------------------------------- >> */
  char		*PosInBuff;		/* current pos in data buffer	    */
  int		ReadInput;		/* amount of Bytesread from buffer  */
  int		retcode;		/* code returned from function	    */
  int		n_retries;		/* retrying counter		    */
  int		i;
  int		db_changed_flg;


	/* << -------------------------------------------------------- >> */
	/*			    DB variables			  */
	/* << -------------------------------------------------------- >> */
	Tprice_list_code   v_price_list_code;
	Tlargo_code        v_largo_code;


/* << -------------------------------------------------------- >> */
/*			  read input buffer			  */
/* << -------------------------------------------------------- >> */
  INIT_ERROR_DETECTION_MECHANISM;
  PosInBuff = glbMsgBuffer;

  v_price_list_code	=  get_short(
				     &PosInBuff,
				     Tprice_list_code_len
				     );
  RETURN_ON_ERROR;

  v_largo_code		=  get_long(
				    &PosInBuff,
				    9
				    );	//Marianna 14Jul2024 User Story #330877
  RETURN_ON_ERROR;


/* << -------------------------------------------------------- >> */
/*			  debug messages			  */
/* << -------------------------------------------------------- >> */
   /************************* COMMENTED OUT: ***************************
  printf( "price_list_code == %d\n",	v_price_list_code );
  printf( "largo_code == %d\n",	v_largo_code );
  fflush( stdout );
  *********************************************************************/

/* << -------------------------------------------------------- >> */
/*		      verify all deta was read			  */
/* << -------------------------------------------------------- >> */
  ReadInput = PosInBuff - glbMsgBuffer;
  StaticAssert( ReadInput == data_len );


/* << -------------------------------------------------------- >> */
/*			   attempt delete			  */
/* << -------------------------------------------------------- >> */
  retcode = ALL_RETRIES_WERE_USED;
  db_changed_flg = 0;

  for( n_retries = 0;
       n_retries < SQL_UPDATE_RETRIES;
       n_retries++ )
    {

      /*
       * sleep for a while:
       */
      if( n_retries )
      {
	  sleep( SLEEP_TIME_BETWEEN_RETRIES );
      }

	  ExecSQL (	MAIN_DB, AS400SRV_T2042_DEL_price_list,
				&v_price_list_code, &v_largo_code, END_OF_ARG_LIST	);

      HANDLE_DELETE_SQL_CODE( "2042", p_connect, &retcode, &db_changed_flg );

      break;
    }

  if( n_retries == SQL_UPDATE_RETRIES )
  {
     WRITE_ACCESS_CONFLICT_TO_LOG( "2042" );
  }

  if (retcode == MSG_HANDLER_SUCCEEDED)
	  rows_deleted++;


  return retcode;
} /* end of msg 2042 handler */

				
// -------------------------------------------------------------------------
//
//       Post-processor for message 4031 - logically delete PRICE_LIST
//			rows that are no longer in the AS/400 full-table download.
//
// -------------------------------------------------------------------------
static int postprocess_4031 (TConnection *p_connect)
{
	// local variables
	bool	stop_retrying;	// if true - return from function
	int		rows_deleted	= 0;
	int		retcode;		// code returned from function
	int		n_retries;		// retrying counter
	int		db_changed_flg;

	// DB variables
	int				v_as400_batch_date;
	int				v_as400_batch_time;
	int				v_row_count;


	// Initialize variables.
	v_as400_batch_date	= download_start_date;
	v_as400_batch_time	= download_start_time;


	retcode			= ALL_RETRIES_WERE_USED;
	stop_retrying	= false;


	while (1)
	{
		ExecSQL (	MAIN_DB, AS400SRV_T4031PP_READ_price_list_stale_count,
					&v_row_count,			&v_as400_batch_date,
					&v_as400_batch_time,	END_OF_ARG_LIST					);

		if (SQLERR_error_test ())
		{
			retcode = SQL_ERROR_OCCURED;
			break;
		}
		else
		{
			if (v_row_count == 0)
			{
				retcode = MSG_HANDLER_SUCCEEDED;
				break;
			}
		}

		ExecSQL (	MAIN_DB, AS400SRV_T4031PP_DEL_price_list,
					&v_as400_batch_date, &v_as400_batch_time, END_OF_ARG_LIST	);

		HANDLE_DELETE_SQL_CODE ("Postprocess 4031", p_connect, &retcode, &db_changed_flg);

		break;
	}

	rows_deleted = v_row_count;


	// DonR 13Nov2018 CR #15919: Record current table size.
	REMEMBER_ISOLATION;
	SET_ISOLATION_DIRTY;

	ExecSQL (	MAIN_DB, AS400SRV_T4031PP_READ_price_list_table_size,
				&total_rows, END_OF_ARG_LIST							);

	active_rows = total_rows;

	RESTORE_ISOLATION;
	// DonR 13Nov2018 CR #15919 end.


	GerrLogMini (GerrId,
				 "Postprocess 4031: %d rows deleted, retcode = %d.",
				 rows_deleted, retcode);

	return retcode;

}	// End of msg 4031 postprocessor.



/* << -------------------------------------------------------- >> */
/*                                                                */
/*      handler for message 2032/4032 - upd/ins to MEMBER_PRICE	  */
/*                                                                */
/* << -------------------------------------------------------- >> */
static int data_2032_handler (TConnection	*p_connect,
							  int			data_len)
{

/* << -------------------------------------------------------- >> */
/*			  local variables			  */
/* << -------------------------------------------------------- >> */
  char      *PosInBuff;		/* current pos in data buffer	    */
  int		ReadInput;		/* amount of Bytesread from buffer  */
  int		retcode;		/* code returned from function	    */
  bool		stop_retrying;		/* if true - return from function   */
  bool		do_insert = ATTEMPT_UPDATE_FIRST;
  int		n_retries;		/* retrying counter		    */
  int		i;


	/* << -------------------------------------------------------- >> */
	/*			    DB variables			  */
	/* << -------------------------------------------------------- >> */
	Tprice_code			v_member_price_code;
	Tmember_price_prcnt	v_member_price_prcnt;
	Tmember_price_prcnt	v_ptn_percent_sort;
	Ttax				v_tax;
	Tparticipation_name	v_participation_name;
	Tcalc_type_code		v_calc_type_code;
	Tmember_institued	v_member_institued;
	Tyarpa_part_code	v_yarpa_part_code;
	int					v_max_price;

	int					sysdate,
						systime;
	int					v_as400_batch_date;
	int					v_as400_batch_time;


  sysdate = GetDate();
  systime = GetTime();
	v_as400_batch_date = (download_start_date > 0) ? download_start_date : sysdate;
	v_as400_batch_time = (download_start_time > 0) ? download_start_time : systime;

/* << -------------------------------------------------------- >> */
/*			read input buffer			  */
/* << -------------------------------------------------------- >> */
  INIT_ERROR_DETECTION_MECHANISM;
  PosInBuff = glbMsgBuffer;

  v_member_price_code	=  get_short(
				     &PosInBuff,
				     Tprice_code_len
				     );
  RETURN_ON_ERROR;

  v_member_price_prcnt	=  get_short(
				     &PosInBuff,
				     Tmember_price_prcnt_len
				     );
  RETURN_ON_ERROR;

  v_tax       		 =  get_short(
				     &PosInBuff,
				     Ttax_len
				     );
  RETURN_ON_ERROR;

			   get_string( &PosInBuff,
				       v_participation_name,
				       Tparticipation_name_len );
  switch_to_win_heb( ( unsigned char * )v_participation_name );

  v_max_price	=  get_short(
				     &PosInBuff,
				     Tmax_price_len
				     );
  RETURN_ON_ERROR;

  v_calc_type_code	=  get_short(
				     &PosInBuff,
				     Tcalc_type_code_len
				     );
  RETURN_ON_ERROR;

  v_member_institued	=  get_short(
				     &PosInBuff,
				     Tmember_institued_len
				     );
  RETURN_ON_ERROR;

  v_yarpa_part_code	=  get_short(
				     &PosInBuff,
				     Tyarpa_part_code_len
				     );
  RETURN_ON_ERROR;

/* << -------------------------------------------------------- >> */
/*			    debug messages			  */
/* << -------------------------------------------------------- >> */
   /************************* COMMENTED OUT:  *************************
  printf( "Tmember_price_code == %d\n", 	v_member_price_code );
  printf( "Tmember_price_prcnt == %d\n", 	v_member_price_prcnt );
  printf( "Ttax == %d\n", 		        v_tax );
  fflush( stdout );
  ********************************************************************/

  /*
   *   verify all data was read:
   */
  ReadInput = PosInBuff - glbMsgBuffer;
  StaticAssert( ReadInput == data_len );


	// DonR 24Nov2011: Copy Member Price Percent to v_ptn_percent_sort, then alter the latter
	// for special cases: 100% participation (which normally implies a fixed discounted price)
	// sorts early (as .01%), and Participation Type 5 (15% with no minimum payment) sorts
	// just before Type 2 (15% *with* minimum payment), so subtract .01% from its percentage.
	switch (v_member_price_code)
	{
		case 1:		v_ptn_percent_sort = 1;
					break;

		case 5:		v_ptn_percent_sort = v_member_price_prcnt - 1;
					break;

		default:	v_ptn_percent_sort = v_member_price_prcnt;
					break;
	}



/* << -------------------------------------------------------- >> */
/*	   up to SQL_UPDATE_RETRIES retries will be made	  */
/*		    to overcome access conflict			  */
/* << -------------------------------------------------------- >> */
  retcode = ALL_RETRIES_WERE_USED;
  stop_retrying = false;

  for(
      n_retries = 0;
      ( n_retries < SQL_UPDATE_RETRIES ) && ( stop_retrying == false );
      n_retries++
      )
    {

      /*
       * sleep for a while:
       */
      if( n_retries )
      {
	  sleep( SLEEP_TIME_BETWEEN_RETRIES );
      }


      /*
       *  try to insert/update new rec with the order depending on "do_insert":
       */
      for( i = 0;  i < 2;  i++ )
	{
	  if( do_insert == ATTEMPT_INSERT_FIRST )
	    {
			ExecSQL (	MAIN_DB, AS400SRV_T4032_INS_member_price,
						&v_member_price_code,		&v_member_price_prcnt,
						&v_ptn_percent_sort,		&v_tax,
						&v_participation_name,		&v_calc_type_code,
						&v_member_institued,		&v_yarpa_part_code,
						&v_max_price,				&sysdate,
						&systime,					&v_as400_batch_date,
						&v_as400_batch_time,		&ROW_NOT_DELETED,
						END_OF_ARG_LIST										);

              HANDLE_INSERT_SQL_CODE( "2032",
					p_connect,
				      &do_insert,
				      &retcode,
				      &stop_retrying );

	    }
	  else
	    {
			ExecSQL (	MAIN_DB, AS400SRV_T4032_UPD_member_price,
						&v_member_price_prcnt,		&v_ptn_percent_sort,
						&v_tax,						&v_participation_name,
						&v_calc_type_code,			&v_member_institued,
						&v_yarpa_part_code,			&v_max_price,
						&sysdate,					&systime,
						&v_as400_batch_date,		&v_as400_batch_time,
						&ROW_NOT_DELETED,			&v_member_price_code,
						END_OF_ARG_LIST										);

              HANDLE_UPDATE_SQL_CODE( "2032",
					p_connect,
				      &do_insert,
				      &retcode,
				      &stop_retrying );

	    }

	}/* for( i... */

    }/* for( n_retries... */

  if( n_retries == SQL_UPDATE_RETRIES )
  {
     WRITE_ACCESS_CONFLICT_TO_LOG( "2032" );
  }

  if (retcode == MSG_HANDLER_SUCCEEDED)
  {
	  if (do_insert == ATTEMPT_INSERT_FIRST)
		  rows_added++;
	  else
		  rows_updated++;
  }


  return retcode;
} /* end of msg 2032 handler */


/* << -------------------------------------------------------- >> */
/*                                                                */
/*    handler for message 2041 - delete rec from MEMBER_PRICE	  */
/*                                                                */
/* << -------------------------------------------------------- >> */
static int message_2041_handler(
				TConnection	*p_connect,
				int		data_len
				)
{

/* << -------------------------------------------------------- >> */
/*			  local variables			  */
/* << -------------------------------------------------------- >> */
  char		*PosInBuff;		/* current pos in data buffer	    */
  int		ReadInput;		/* amount of Bytesread from buffer  */
  int		retcode;		/* code returned from function	    */
  int		n_retries;		/* retrying counter		    */
  int		i;
  int		db_changed_flg;


	/* << -------------------------------------------------------- >> */
	/*			    DB variables			  */
	/* << -------------------------------------------------------- >> */
	Tprice_code   v_member_price_code;


/* << -------------------------------------------------------- >> */
/*			  read input buffer			  */
/* << -------------------------------------------------------- >> */
  INIT_ERROR_DETECTION_MECHANISM;
  PosInBuff = glbMsgBuffer;

  v_member_price_code	=  get_short(
				     &PosInBuff,
				     Tprice_code_len
				     );
  RETURN_ON_ERROR;


/* << -------------------------------------------------------- >> */
/*			  debug messages			  */
/* << -------------------------------------------------------- >> */
   /************************* COMMENTED OUT: *************************
  printf( "member_price_code == %d\n",v_member_price_code );
  fflush( stdout );
  *******************************************************************/


/* << -------------------------------------------------------- >> */
/*		      verify all deta was read			  */
/* << -------------------------------------------------------- >> */
  ReadInput = PosInBuff - glbMsgBuffer;
  StaticAssert( ReadInput == data_len );


/* << -------------------------------------------------------- >> */
/*			   attempt delete			  */
/* << -------------------------------------------------------- >> */
  retcode = ALL_RETRIES_WERE_USED;
  db_changed_flg = 0;

  for( n_retries = 0;
       n_retries < SQL_UPDATE_RETRIES;
       n_retries++ )
    {

      /*
       * sleep for a while:
       */
      if( n_retries )
      {
	  sleep( SLEEP_TIME_BETWEEN_RETRIES );
      }

		ExecSQL (	MAIN_DB, AS400SRV_T2041_DEL_member_price,
					&v_member_price_code, END_OF_ARG_LIST		);

      HANDLE_DELETE_SQL_CODE( "2041", p_connect, &retcode, &db_changed_flg );

      break;
    }

  if( n_retries == SQL_UPDATE_RETRIES )
  {
     WRITE_ACCESS_CONFLICT_TO_LOG( "2041" );
  }

  if (retcode == MSG_HANDLER_SUCCEEDED)
	  rows_deleted++;


  return retcode;
} /* end of msg 2041 handler */


// -------------------------------------------------------------------------
//
//       Post-processor for message 4032 - delete MEMBER_PRICE
//			rows that are no longer in the AS/400 full-table download.
//
// -------------------------------------------------------------------------
static int postprocess_4032 (TConnection *p_connect)
{
	// local variables
	bool	stop_retrying;	// if true - return from function
	int		rows_deleted	= 0;
	int		retcode;		// code returned from function
	int		n_retries;		// retrying counter
	int		db_changed_flg;

	// DB variables
	int					v_as400_batch_date;
	int					v_as400_batch_time;
	int					v_row_count;
	int					sysdate;
	int					systime;


	sysdate = GetDate();
	systime = GetTime();


	// Initialize variables.
	v_as400_batch_date	= download_start_date;
	v_as400_batch_time	= download_start_time;


	retcode			= ALL_RETRIES_WERE_USED;
	stop_retrying	= false;


	while (1)
	{
		ExecSQL (	MAIN_DB, AS400SRV_T4032PP_READ_member_price_stale_count,
					&v_row_count,			&v_as400_batch_date,
					&v_as400_batch_time,	END_OF_ARG_LIST						);

		if (SQLERR_error_test ())
		{
			retcode = SQL_ERROR_OCCURED;
			break;
		}
		else
		{
			if (v_row_count == 0)
			{
				retcode = MSG_HANDLER_SUCCEEDED;
				break;
			}
		}

		ExecSQL (	MAIN_DB, AS400SRV_T4032PP_DEL_member_price,
					&v_as400_batch_date, &v_as400_batch_time, END_OF_ARG_LIST	);

		HANDLE_DELETE_SQL_CODE ("Postprocess 4032", p_connect, &retcode, &db_changed_flg);

		break;
	}

	rows_deleted = v_row_count;


	// DonR 13Nov2018 CR #15919: Record current table size.
	REMEMBER_ISOLATION;
	SET_ISOLATION_DIRTY;

	ExecSQL (	MAIN_DB, AS400SRV_T4032PP_READ_member_price_table_size,
				&total_rows, END_OF_ARG_LIST							);

	active_rows = total_rows;

	RESTORE_ISOLATION;
	// DonR 13Nov2018 CR #15919 end.


	GerrLogMini (GerrId,
				 "Postprocess 4032: %d rows deleted, retcode = %d.",
				 rows_deleted, retcode);

	return retcode;

}	// End of msg 4032 postprocessor.



/* << -------------------------------------------------------- >> */
/*                                                                */
/*       handler for message 2035/4035 - upd/ins to SUPPLIERS	  */
/*                                                                */
/* << -------------------------------------------------------- >> */
static int data_2035_handler (TConnection	*p_connect,
							  int			data_len)
{
	// Local variables
	char	*PosInBuff;		/* current pos in data buffer	    */
	int		ReadInput;		/* amount of Bytesread from buffer  */
	int		retcode;		/* code returned from function	    */
	bool	stop_retrying;	/* if true - return from function   */
	bool	do_insert			= ATTEMPT_UPDATE_FIRST;
	int		n_retries;		/* retrying counter				   */
	int		i;

	Tsupplier_table_cod	v_supplier_table_cod;
	Tsupplier_code		v_supplier_code;
	Tsupplier_name		v_supplier_name			, o_supplier_name;
	Tsupplier_type		v_supplier_type			, o_supplier_type;
	Tstreet_and_no		v_street_and_no			, o_street_and_no;
	Tcity				v_city					, o_city;
	Tzip_code			v_zip_code				, o_zip_code;
	Tphone_1			v_phone_1				, o_phone_1;
	Tphone_2			v_phone_2				, o_phone_2;
	Tfax_num			v_fax_num				, o_fax_num;
	Tcomm_supplier		v_comm_supplier			, o_comm_supplier;
	Temployee_id		v_employee_id			, o_employee_id;
	Tallowed_proc_list	v_allowed_proc_list		, o_allowed_proc_list;
	Temployee_password	v_employee_password		, o_employee_password;
	Tcheck_type			v_check_type			, o_check_type;
	Tdel_flg			v_del_flg				, o_del_flg;
	int					v_update_date			, o_update_date;
	int					v_update_time			, o_update_time;
	int					v_as400_batch_date;
	int					v_as400_batch_time;
	int					sysdate;
	int					systime;


	sysdate = GetDate ();
	systime = GetTime ();
	v_as400_batch_date = (download_start_date > 0) ? download_start_date : sysdate;
	v_as400_batch_time = (download_start_time > 0) ? download_start_time : systime;


	// Read input buffer.
	INIT_ERROR_DETECTION_MECHANISM;
	PosInBuff = glbMsgBuffer;

	GET_SHORT	(v_supplier_table_cod	, Tsupplier_table_cod_len	);
	GET_STRING	(v_supplier_type		, Tsupplier_type_len		);
	GET_LONG	(v_supplier_code		, Tsupplier_code_len		);
	GET_HEB_STR	(v_supplier_name		, Tsupplier_name_len		);
	GET_STRING	(v_street_and_no		, Tstreet_and_no_len		);
	GET_HEB_STR	(v_city					, Tcity_len					);
	GET_LONG	(v_zip_code				, Tzip_code_len				);
	GET_STRING	(v_phone_1				, Tphone_1_len				);
	GET_STRING	(v_phone_2				, Tphone_2_len				);
	GET_STRING	(v_fax_num				, Tfax_num_len				);
	GET_LONG	(v_comm_supplier		, Tcomm_supplier_len		);
	GET_LONG	(v_employee_id			, Temployee_id_len			);
	GET_HEB_STR	(v_allowed_proc_list	, Tallowed_proc_list_len	);
	GET_LONG	(v_employee_password	, Temployee_password_len	);
	GET_SHORT	(v_check_type			, Tcheck_type_len			);
	GET_SHORT	(v_del_flg				, Tdel_flg_len				);

	// Verify all data was read.
	ReadInput = PosInBuff - glbMsgBuffer;
	StaticAssert (ReadInput == data_len);

	// DonR 09Feb2020: Because ODBC (at least for Informix) automatically strips
	// trailing spaces when it reads VARCHAR columns, we need to strip the trailing
	// spaces we get from AS/400. Otherwise the comparison below will always see
	// a mismatch between stored and "new" data, even when there is none.
	strip_trailing_spaces (v_supplier_name);
	strip_trailing_spaces (v_street_and_no);
	strip_trailing_spaces (v_city);
	strip_trailing_spaces (v_phone_1);
	strip_trailing_spaces (v_phone_2);
	strip_trailing_spaces (v_fax_num);


	// Up to SQL_UPDATE_RETRIES retries will be made to overcome access conflict.
	retcode = ALL_RETRIES_WERE_USED;
	stop_retrying = false;

	for (n_retries = 0;
		 (n_retries < SQL_UPDATE_RETRIES) && (stop_retrying == false);
		 n_retries++)
	{
		// If this isn't the first attempt, sleep for a while.
		if (n_retries)
		{
			sleep (SLEEP_TIME_BETWEEN_RETRIES);
		}

		// Read old data to see if anything has changed.
		ExecSQL (	MAIN_DB, AS400SRV_T4035_READ_suppliers_old_data,
					&o_supplier_name,	&o_supplier_type,		&o_street_and_no,
					&o_city,			&o_zip_code,			&o_phone_1,
					&o_phone_2,			&o_fax_num,				&o_comm_supplier,
					&o_employee_id,		&o_allowed_proc_list,	&o_employee_password,
					&o_check_type,		&o_update_date,			&o_update_time,
					&o_del_flg,
					&v_supplier_code,	&v_supplier_table_cod,	END_OF_ARG_LIST			);

		if (SQLERR_code_cmp (SQLERR_not_found) == MAC_TRUE)
		{
 			do_insert = ATTEMPT_INSERT_FIRST; 
		}
		else if (SQLERR_code_cmp (SQLERR_access_conflict) == MAC_TRUE)
		{
			retcode = ALL_RETRIES_WERE_USED;
			continue;
		}
		else if (SQLERR_error_test())
		{
			retcode = SQL_ERROR_OCCURED;
			break;
		}
		else   // No errors on fetch
		{
			do_insert = ATTEMPT_UPDATE_FIRST;

			// See if anything has changed. All RKFILPRD updates are considered significant, so 
			// if download_start_date == 0 that's sufficient to indicate a change.
			if ((download_start_date			>	0						 )	&&	// = RKUNIX download.
				(!strcmp (v_supplier_type		,	o_supplier_type			))	&&
				(!strcmp (v_supplier_name		,	o_supplier_name			))	&&
				(!strcmp (v_street_and_no		,	o_street_and_no			))	&&
				(!strcmp (v_city				,	o_city					))	&&
				(!strcmp (v_phone_1				,	o_phone_1				))	&&
				(!strcmp (v_phone_2				,	o_phone_2				))	&&
				(!strcmp (v_fax_num				,	o_fax_num				))	&&
				(!strcmp (v_allowed_proc_list	,	o_allowed_proc_list		))	&&
				(v_zip_code						==	o_zip_code				 )	&&
				(v_comm_supplier				==	o_comm_supplier			 )	&&
				(v_employee_id					==	o_employee_id			 )	&&
				(v_employee_password			==	o_employee_password		 )	&&
				(v_check_type					==	o_check_type			 )	&&
				(v_del_flg						==	o_del_flg				 ))
			{
				// Nothing's changed after all.
				v_update_date = o_update_date;
				v_update_time = o_update_time;
			}
			else
			{
				// Something has indeed changed!
				v_update_date = sysdate;
				v_update_time = systime;
			}
		}	// Successful read of existing price_list row.	


		// try to insert/update new rec with the order depending on "do_insert":
		for (i = 0; i < 2; i++)
		{
			if (do_insert == ATTEMPT_INSERT_FIRST)
			{
				ExecSQL (	MAIN_DB, AS400SRV_T4035_INS_suppliers,
							&v_supplier_table_cod,	&v_supplier_code,		&v_supplier_name,
							&v_supplier_type,		&v_street_and_no,		&v_city,
							&v_zip_code,			&v_phone_1,				&v_phone_2,
							&v_fax_num,				&v_comm_supplier,		&v_employee_id,
							&v_allowed_proc_list,	&v_employee_password,	&v_check_type,
							&sysdate,				&systime,				&v_del_flg,
							&v_as400_batch_date,	&v_as400_batch_time,	&sysdate,
							&systime,				END_OF_ARG_LIST								);
				
				HANDLE_INSERT_SQL_CODE ("4035", p_connect, &do_insert, &retcode, &stop_retrying);
			}
			else
			{
				ExecSQL (	MAIN_DB, AS400SRV_T4035_UPD_suppliers,
							&v_supplier_name,			&v_supplier_type,
							&v_street_and_no,			&v_city,
							&v_zip_code,				&v_phone_1,
							&v_phone_2,					&v_fax_num,
							&v_comm_supplier,			&v_employee_id,
							&v_allowed_proc_list,		&v_employee_password,
							&v_check_type,				&v_update_date,
							&v_update_time,				&sysdate,
							&systime,					&v_as400_batch_date,
							&v_as400_batch_time,		&v_del_flg,
							&v_supplier_code,			&v_supplier_table_cod,
							END_OF_ARG_LIST										);
				
				HANDLE_UPDATE_SQL_CODE ("4035", p_connect, &do_insert, &retcode, &stop_retrying);
			}

		}	// for (i...

	}	// for (n_retries...


	if (n_retries == SQL_UPDATE_RETRIES)
	{
		WRITE_ACCESS_CONFLICT_TO_LOG ("4035");
	}

	if (retcode == MSG_HANDLER_SUCCEEDED)
	{
		if (do_insert == ATTEMPT_INSERT_FIRST)
			rows_added++;
		else
			if ((v_update_date == o_update_date) && (v_update_time = o_update_time))
				rows_refreshed++;
			else
				rows_updated++;
	}

	return retcode;
} // End of msg 2035/4035 handler.


// -------------------------------------------------------------------------
//
//       Post-processor for message 4035 - logically delete SUPPLIERS
//			rows that are no longer in the AS/400 full-table download.
//
// -------------------------------------------------------------------------
static int postprocess_4035 (TConnection *p_connect)
{
	// local variables
	bool	stop_retrying;	// if true - return from function
	int		rows_deleted	= 0;
	int		retcode;		// code returned from function
	int		n_retries;		// retrying counter
	int		db_changed_flg;

	// DB variables
	int					v_as400_batch_date;
	int					v_as400_batch_time;
	int					v_row_count;
	int					sysdate;
	int					systime;


	sysdate = GetDate();
	systime = GetTime();


	// Initialize variables.
	v_as400_batch_date	= download_start_date;
	v_as400_batch_time	= download_start_time;


	retcode			= ALL_RETRIES_WERE_USED;
	stop_retrying	= false;


	while (1)
	{
		ExecSQL (	MAIN_DB, AS400SRV_T4035PP_READ_suppliers_stale_count,
					&v_row_count,			&v_as400_batch_date,
					&v_as400_batch_time,	END_OF_ARG_LIST					);

		if (SQLERR_error_test ())
		{
			retcode = SQL_ERROR_OCCURED;
			break;
		}
		else
		{
			if (v_row_count == 0)
			{
				retcode = MSG_HANDLER_SUCCEEDED;
				break;
			}
		}

		ExecSQL (	MAIN_DB, AS400SRV_T4035PP_UPD_suppliers_logical_delete,
					&sysdate,				&systime,
					&v_as400_batch_date,	&v_as400_batch_time,
					END_OF_ARG_LIST											);

		HANDLE_DELETE_SQL_CODE ("Postprocess 4035", p_connect, &retcode, &db_changed_flg);

		break;
	}

	rows_deleted = v_row_count;


	// DonR 13Nov2018 CR #15919: Record current table size.
	REMEMBER_ISOLATION;
	SET_ISOLATION_DIRTY;

	ExecSQL (	MAIN_DB, AS400SRV_T4035PP_READ_suppliers_table_size,
				&total_rows, END_OF_ARG_LIST							);

	ExecSQL (	MAIN_DB, AS400SRV_T4035PP_READ_suppliers_active_size,
				&active_rows, END_OF_ARG_LIST							);

	RESTORE_ISOLATION;
	// DonR 13Nov2018 CR #15919 end.


	GerrLogMini (GerrId,
				 "Postprocess 4035: %d rows logically deleted, retcode = %d.",
				 rows_deleted, retcode);

	return retcode;

}	// End of msg 4035 postprocessor.



/* << -------------------------------------------------------- >> */
/*                                                                */
/*     handler for message 2036 - upd/ins to MACABI_CENTERS	  */
/*                                                                */
/* << -------------------------------------------------------- >> */
static int message_2036_handler (	TConnection	*p_connect,
									int			data_len,
									bool		do_insert		)
{

/* << -------------------------------------------------------- >> */
/*			  local variables			  */
/* << -------------------------------------------------------- >> */
  char		*PosInBuff;		/* current pos in data buffer	    */
  int		ReadInput;		/* amount of Bytesread from buffer  */
  int		retcode;		/* code returned from function	    */
  bool		stop_retrying;		/* if true - return from function   */
  int		n_retries;		/* retrying counter		    */
  int		i;


	/* << -------------------------------------------------------- >> */
	/*			    DB variables			  */
	/* << -------------------------------------------------------- >> */
	Tmacabi_centers_num	v_macabi_centers_num;
	Tfirst_center_type	v_first_center_type;
	Tsecond_center_type	v_second_center_type;
	Tmacabi_centers_nam	v_macabi_centers_nam;
	Tstreet_and_no		v_street_and_no;
	Tcity				v_city;
	Tzip_code			v_zip_code;
	Tphone_1			v_phone_1;
	char				v_assuta_card_number [10 + 1];	// DonR 23Aug2009 - was v_phone_2.
	Tfax_num			v_fax_num;
	Tdiscount_percent	v_discount_percent;
	Tcredit_flag		v_credit_flag;
	Tallowed_proc_list	v_allowed_proc_list;
	Tallowed_belongings	v_allowed_belongings;

	// DonR 01Jun2021 User Story #144324 begin.
	char				delivery_type[15 + 1];		// Marianna 19Jan2023 User Story #276011: change from short to char(15)
	int					sms_code;
	int					sms_format;
	int					sms_subformat;
	// DonR 01Jun2021 User Story #144324 end.

	int					sysdate;
	int					systime;


	sysdate = GetDate ();
	systime = GetTime ();

	strcpy (v_second_center_type, "  " ); /* hard coded */

	/* << -------------------------------------------------------- >> */
	/*			read input buffer									  */
	/* << -------------------------------------------------------- >> */
	INIT_ERROR_DETECTION_MECHANISM;
	PosInBuff = glbMsgBuffer;

	v_macabi_centers_num	= get_long	(&PosInBuff, Tmacabi_centers_num_len	);	RETURN_ON_ERROR;

	get_string	(&PosInBuff, v_first_center_type,	Tfirst_center_type_len		);	switch_to_win_heb ((unsigned char *)v_first_center_type		);
	get_string	(&PosInBuff, v_macabi_centers_nam,	Tmacabi_centers_nam_len		);	switch_to_win_heb ((unsigned char *)v_macabi_centers_nam	);
	get_string	(&PosInBuff, v_street_and_no,		Tstreet_and_no_len			);	switch_to_win_heb ((unsigned char *)v_street_and_no			);
	get_string	(&PosInBuff, v_city,				Tcity_len					);	switch_to_win_heb ((unsigned char *)v_city					);

	v_zip_code				= get_long	(&PosInBuff, Tzip_code_len				);	RETURN_ON_ERROR;

	get_string	(&PosInBuff, v_phone_1,				Tphone_1_len				);

	// DonR 23Aug2009: The second-phone-number slot is now used for Assuta Card Number.
	get_string	(&PosInBuff, v_assuta_card_number,	10							);

	get_string	(&PosInBuff, v_fax_num,				Tfax_num_len				);

	v_discount_percent		= get_short	(&PosInBuff, Tdiscount_percent_len		);	RETURN_ON_ERROR;
	v_credit_flag			= get_short	(&PosInBuff, Tcredit_flag_len			);	RETURN_ON_ERROR;

	get_string	(&PosInBuff, v_allowed_proc_list,	Tallowed_proc_list_len		);
	get_string	(&PosInBuff, v_allowed_belongings,	Tallowed_belongings_len		);

	// DonR 01Jun2021 User Story #144324 begin.
	//delivery_type			= get_short	(&PosInBuff, 1							);	RETURN_ON_ERROR; // Marianna 19Jan2023 User Story #276011: change from type
	get_string	(&PosInBuff, delivery_type, 15									);	// Marianna 19Jan2023 User Story #276011: change from to type char
	sms_code				= get_long	(&PosInBuff, 9							);	RETURN_ON_ERROR;
	sms_format				= get_long	(&PosInBuff, 9							);	RETURN_ON_ERROR;
	sms_subformat			= get_long	(&PosInBuff, 9							);	RETURN_ON_ERROR;
	// DonR 01Jun2021 User Story #144324 end.

/* << -------------------------------------------------------- >> */
/*			    debug messages			  */
/* << -------------------------------------------------------- >> */
   /************************* COMMENTED OUT: *************************
  printf( "macabi_centers_num == %d\n",v_macabi_centers_num );
  printf( "Tfirst_center_type == %s\n", v_first_center_type );
  printf( "Tmacabi_centers_nam == %s\n",v_macabi_centers_nam );
  printf( "Tstreet_and_no == %s\n", 	v_street_and_no );
  printf( "Tcity == %s\n", 		v_city );
  printf( "Tzip_code == %d\n", 	v_zip_code );
  printf( "Tphone_1 == %s\n", 		v_phone_1 );
  printf( "Tphone_2 == %s\n", 		v_phone_2 );
  printf( "Tfax_num == %s\n", 		v_fax_num );
  printf( "Tdiscount_percent == %d\n", 	v_discount_percent );
  printf( "Tcredit_flag == %d\n", 	v_credit_flag );
  fflush( stdout );
  ******************************************************************/

  /*
   *   verify all data was read:
   */
  ReadInput = PosInBuff - glbMsgBuffer;
  StaticAssert( ReadInput == data_len );


/* << -------------------------------------------------------- >> */
/*	   up to SQL_UPDATE_RETRIES retries will be made	  */
/*		    to overcome access conflict			  */
/* << -------------------------------------------------------- >> */
  retcode = ALL_RETRIES_WERE_USED;
  stop_retrying = false;

  for(
      n_retries = 0;
      ( n_retries < SQL_UPDATE_RETRIES ) && ( stop_retrying == false );
      n_retries++
      )
    {

      /*
       * sleep for a while:
       */
      if( n_retries )
      {
	  sleep( SLEEP_TIME_BETWEEN_RETRIES );
      }


		// try to insert/update new rec with the order depending on "do_insert":
		for (i = 0; i < 2; i++)
		{
			if (do_insert == ATTEMPT_INSERT_FIRST)
			{
				ExecSQL (	MAIN_DB, AS400SRV_T2036_INS_macabi_centers,
							&v_macabi_centers_num,		&v_first_center_type,
							&v_second_center_type,		&v_macabi_centers_nam,
							&v_street_and_no,			&v_city,
							&v_zip_code,				&v_phone_1,
							&v_fax_num,					&v_discount_percent,
							&v_credit_flag,				&v_allowed_proc_list,
							&v_allowed_belongings,		&v_assuta_card_number,
							&delivery_type,				&sms_code,
							&sms_format,				&sms_subformat,
							&sysdate,					&systime,
							&ROW_NOT_DELETED,			END_OF_ARG_LIST			);

				HANDLE_INSERT_SQL_CODE ("2036",
										p_connect,
										&do_insert,
										&retcode,
										&stop_retrying);

			}
			else
			{
				ExecSQL (	MAIN_DB, AS400SRV_T2036_UPD_macabi_centers,
							&v_first_center_type,		&v_second_center_type,
							&v_macabi_centers_nam,		&v_street_and_no,
							&v_city,					&v_zip_code,
							&v_phone_1,					&v_fax_num,
							&v_discount_percent,		&v_credit_flag,
							&v_allowed_proc_list,		&v_allowed_belongings,
							&v_assuta_card_number,		&delivery_type,
							&sms_code,					&sms_format,
							&sms_subformat,				&sysdate,
							&systime,					&v_macabi_centers_num,
							END_OF_ARG_LIST											);

				HANDLE_UPDATE_SQL_CODE ("2036",
										p_connect,
										&do_insert,
										&retcode,
										&stop_retrying);

			}

		}/* for( i... */

    }/* for( n_retries... */

  if( n_retries == SQL_UPDATE_RETRIES )
  {
     WRITE_ACCESS_CONFLICT_TO_LOG( "2036" );
  }

  if (retcode == MSG_HANDLER_SUCCEEDED)
  {
	  if (do_insert == ATTEMPT_INSERT_FIRST)
		  rows_added++;
	  else
		  rows_updated++;
  }


  return retcode;
} /* end of msg 2036 handler */


/* << -------------------------------------------------------- >> */
/*                                                                */
/*   handler for message 2037 - delete rec from MACABI_CENTERS	  */
/*                                                                */
/* << -------------------------------------------------------- >> */
static int message_2037_handler(
				TConnection	*p_connect,
				int		data_len
				)
{

/* << -------------------------------------------------------- >> */
/*			  local variables			  */
/* << -------------------------------------------------------- >> */
  char          *PosInBuff;		/* current pos in data buffer	    */
  int		ReadInput;		/* amount of Bytesread from buffer  */
  int		retcode;		/* code returned from function	    */
  int		n_retries;		/* retrying counter		    */
  int		i;
  int		db_changed_flg;


	/* << -------------------------------------------------------- >> */
	/*			    DB variables			  */
	/* << -------------------------------------------------------- >> */
	Tmacabi_centers_num	v_macabi_centers_num;


/* << -------------------------------------------------------- >> */
/*			  read input buffer			  */
/* << -------------------------------------------------------- >> */
  INIT_ERROR_DETECTION_MECHANISM;
  PosInBuff = glbMsgBuffer;

  v_macabi_centers_num	=  get_long(
				    &PosInBuff,
				    Tmacabi_centers_num_len
				    );
  RETURN_ON_ERROR;

/* << -------------------------------------------------------- >> */
/*			  debug messages			  */
/* << -------------------------------------------------------- >> */
   /************************* COMMENTED OUT:  ***************************
  printf( "macabi_centers_num == %d\n",v_macabi_centers_num );
  fflush( stdout );
  ***********************************************************************/

/* << -------------------------------------------------------- >> */
/*		      verify all deta was read			  */
/* << -------------------------------------------------------- >> */
  ReadInput = PosInBuff - glbMsgBuffer;
  StaticAssert( ReadInput == data_len );


/* << -------------------------------------------------------- >> */
/*			   attempt delete			  */
/* << -------------------------------------------------------- >> */
  retcode = ALL_RETRIES_WERE_USED;
  db_changed_flg = 0;

  for( n_retries = 0;
       n_retries < SQL_UPDATE_RETRIES;
       n_retries++ )
    {

      /*
       * sleep for a while:
       */
      if( n_retries )
      {
	  sleep( SLEEP_TIME_BETWEEN_RETRIES );
      }

		ExecSQL (	MAIN_DB, AS400SRV_T2037_DEL_macabi_centers,
					&v_macabi_centers_num, END_OF_ARG_LIST		);

      HANDLE_DELETE_SQL_CODE( "2037", p_connect, &retcode, &db_changed_flg );

      break;
    }

  if( n_retries == SQL_UPDATE_RETRIES )
  {
     WRITE_ACCESS_CONFLICT_TO_LOG( "2037" );
  }

  if (retcode == MSG_HANDLER_SUCCEEDED)
	  rows_deleted++;


  return retcode;
} /* end of msg 2037 handler */


//Marianna 20Jul2023 User Story #456129
/* << -------------------------------------------------------------- >> */
/*                                                                		*/
/*       handler for message 2048/4048 - upd/ins to GenComponents		*/
/*                                                                		*/
/* << -------------------------------------------------------------- >> */
static int data_2048_handler (TConnection	*p_connect,
							  int			data_len)
{
/* << -------------------------------------------------------- >> */
/*			  local variables			  */
/* << -------------------------------------------------------- >> */
  char      *PosInBuff;		/* current pos in data buffer	    */
  int		ReadInput;		/* amount of Bytesread from buffer  */
  int		retcode;		/* code returned from function	    */
  bool		stop_retrying;		/* if true - return from function   */
  bool		do_insert = ATTEMPT_UPDATE_FIRST;
  int		n_retries;		/* retrying counter		    */
  int		i;
  short		InFullTableMode;
  int		v_as400_batch_date;
  int		v_as400_batch_time;


	/* << -------------------------------------------------------- >> */
	/*			    DB variables			  */
	/* << -------------------------------------------------------- >> */
	Tgen_comp_code	v_gen_comp_code;
	Tgen_comp_desc	v_gen_comp_desc;
	Tdel_flg_c		v_del_flg;
	int				sysdate;	
	int				systime;	
	Tgen_comp_desc	old_gen_comp_desc;
	Tdel_flg_c		old_del_flg;

	
	InFullTableMode		= (download_start_date > 0);

	sysdate = GetDate();
	systime = GetTime();

	v_as400_batch_date = (InFullTableMode) ? download_start_date : sysdate;
	v_as400_batch_time = (InFullTableMode) ? download_start_time : systime;

/* << -------------------------------------------------------- >> */
/*			read input buffer			  */
/* << -------------------------------------------------------- >> */
  INIT_ERROR_DETECTION_MECHANISM;
  PosInBuff = glbMsgBuffer;

	v_gen_comp_code		=  get_short(  &PosInBuff,    Tgen_comp_code_len  );  RETURN_ON_ERROR;
	get_string(   &PosInBuff, v_gen_comp_desc,Tgen_comp_desc_len    );
	get_string(   &PosInBuff, v_del_flg,Tdel_flg_c_len    );
	

/* << -------------------------------------------------------- >> */
/*			    debug messages			  */
/* << -------------------------------------------------------- >> */
  

  /*
   *   verify all data was read:
   */
  ReadInput = PosInBuff - glbMsgBuffer;
  StaticAssert( ReadInput == data_len );

  strip_trailing_spaces (v_gen_comp_desc);	// DonR 09Feb2020 - since ODBC (at least for Informix) strips trailing spaces when SELECTing.

  // check difference in date 20020307
	ExecSQL (	MAIN_DB, AS400SRV_T2048_READ_GenComponents_old_data,
				&old_gen_comp_desc,		&old_del_flg,
				&v_gen_comp_code,		END_OF_ARG_LIST					);
				
	if ( SQLERR_code_cmp( SQLERR_not_found ) == MAC_TRUE )
	{
		//*shm_table_to_update = GENCOMPONENTS;
		do_insert = ATTEMPT_INSERT_FIRST; 
	}
	else if (SQLERR_code_cmp(SQLERR_access_conflict) == MAC_TRUE)
	{
	   sleep( SLEEP_TIME_BETWEEN_RETRIES );
	   retcode = ALL_RETRIES_WERE_USED;
	}
	else if ( SQLERR_error_test() )
    {
	    retcode = SQL_ERROR_OCCURED;
	}

	else   // No errors on fetch
	{
		do
		{
			if ( strcmp(old_gen_comp_desc ,v_gen_comp_desc))
			{
				GerrLogFnameMini("2048",GerrId,"1:[%d][%s][%s]",v_gen_comp_code,old_gen_comp_desc ,v_gen_comp_desc);
				//*shm_table_to_update = GENCOMPONENTS;
				break;
			}
			if ( strcmp (old_del_flg , v_del_flg))
			{
				GerrLogFnameMini("2048",GerrId,"2:[%d]",v_gen_comp_code);
				//*shm_table_to_update = GENCOMPONENTS;
				break;
			}
		}
		while (0);
	}	

/* << -------------------------------------------------------- >> */
/*	   up to SQL_UPDATE_RETRIES retries will be made	  */
/*		    to overcome access conflict			  */
/* << -------------------------------------------------------- >> */
	retcode = ALL_RETRIES_WERE_USED;
	stop_retrying = false;

	for (n_retries = 0; (n_retries < SQL_UPDATE_RETRIES) && (stop_retrying == false); n_retries++)
	{
		// Sleep for a while:
		if (n_retries)
		{
			sleep (SLEEP_TIME_BETWEEN_RETRIES);
		}

		// Try to insert/update new rec with the order depending on "do_insert".
		for (i = 0;  i < 2;  i++)
		{
			if (do_insert == ATTEMPT_INSERT_FIRST)
			{
				ExecSQL (	MAIN_DB, AS400SRV_T2048_INS_GenComponents,
						&v_gen_comp_code,		&v_gen_comp_desc,
						&sysdate,				&systime,
						&UPDATED_BY_AS400,		&v_del_flg,
						&v_as400_batch_date,	&v_as400_batch_time,
						END_OF_ARG_LIST								);


				HANDLE_INSERT_SQL_CODE( "2048",p_connect,   &do_insert,   &retcode,  &stop_retrying );

			}
			else
			{
				ExecSQL (	MAIN_DB, AS400SRV_T2048_UPD_GenComponents,
						&v_gen_comp_desc,		&sysdate,
						&systime,				&UPDATED_BY_AS400,
						&v_del_flg,
						&v_as400_batch_date,	&v_as400_batch_time,
						&v_gen_comp_code,		END_OF_ARG_LIST		);

				HANDLE_UPDATE_SQL_CODE( "2048",	p_connect,  &do_insert,    &retcode, &stop_retrying );

			}	// In update mode.

		}	// for ( i...

	}	/* for( n_retries... */


	if (n_retries == SQL_UPDATE_RETRIES)
	{
		WRITE_ACCESS_CONFLICT_TO_LOG ( "2048" );
	}

	if (retcode == MSG_HANDLER_SUCCEEDED)
	{
		if (do_insert == ATTEMPT_INSERT_FIRST)
			rows_added++;
		else
			rows_updated++;
	}

	return retcode;
} /* end of msg 2048_4048 handler */


// Marianna 23Jul2023 User Story #456129
// -------------------------------------------------------------------------
//
//       Post-processor for message 4048 - delete GenComponents
//			rows that are no longer in the AS/400 full-table download.
//
// -------------------------------------------------------------------------
static int postprocess_4048 (TConnection	*p_connect)
{
	// local variables
	bool	stop_retrying;	// if true - return from function
	int		rows_deleted	= 0;
	int		retcode;		// code returned from function
	int		db_changed_flg;

	// DB variables
	Tgen_comp_code	v_gen_comp_code;
	Tdel_flg_c		v_del_flg;
	
	int		v_as400_batch_date;
	int		v_as400_batch_time;
	

	// Initialize variables.
	v_as400_batch_date	= download_start_date;
	v_as400_batch_time	= download_start_time;


	retcode			= ALL_RETRIES_WERE_USED;
	stop_retrying	= false;


	DeclareCursorInto (	MAIN_DB, AS400SRV_T4048PP_postproc_cur,
						&v_gen_comp_code,
						&v_as400_batch_date,	&v_as400_batch_date,
						&v_as400_batch_time,	END_OF_ARG_LIST			);

	OpenCursor (	MAIN_DB, AS400SRV_T4048PP_postproc_cur	);

	if (SQLERR_error_test ())
	{
		retcode = SQL_ERROR_OCCURED;
//		break;
	}

	while (1)
	{
		FetchCursor (	MAIN_DB, AS400SRV_T4048PP_postproc_cur	);

		if (SQLERR_code_cmp (SQLERR_end_of_fetch) == MAC_TRUE)
		{
			retcode = MSG_HANDLER_SUCCEEDED;	// In case there is nothing that needs to be deleted.
			break;
		}
		else
		{
			if (SQLERR_error_test ())
			{
				retcode = SQL_ERROR_OCCURED;
				break;
			}
		}

		// Physical delete.
		ExecSQL (	MAIN_DB, AS400SRV_T4048PP_DEL_gencomponents	);

		HANDLE_DELETE_SQL_CODE ("postprocess_4048", p_connect, &retcode, &db_changed_flg);

		rows_deleted++;
	}	// Cursor-scrolling loop.

	CloseCursor (	MAIN_DB, AS400SRV_T4048PP_postproc_cur	);

	// Record current table size.
	REMEMBER_ISOLATION;
	SET_ISOLATION_DIRTY;

	ExecSQL (	MAIN_DB, AS400SRV_T4048PP_READ_gencomponents_table_size,
				&total_rows, END_OF_ARG_LIST								);

	active_rows = total_rows;

	RESTORE_ISOLATION;


	GerrLogMini (GerrId,
				 "Postprocess 4048: %d rows Physically deleted, retcode = %d.",
				 rows_deleted, retcode);

	return retcode;

}// End of msg 4048 (GenComponents) postprocessor.


/* << -------------------------------------------------------- >> */
/*                                                                */
/*       handler for message 2049 - upd/ins to DRUGGENCOMPONENTS  */
/* << -------------------------------------------------------- >> */
static int message_2049_handler(
				TConnection	*p_connect,
				int		data_len,
				bool	do_insert,
				TShmUpdate	*shm_table_to_update
				)
{
/* << -------------------------------------------------------- >> */
/*			  local variables			  */
/* << -------------------------------------------------------- >> */
  char      *PosInBuff;		/* current pos in data buffer	    */
  int		ReadInput;		/* amount of Bytesread from buffer  */
  int		retcode;		/* code returned from function	    */
  bool		stop_retrying;	/* if true - return from function   */
  int		n_retries;		/* retrying counter		    */
  int		i;

	/* << -------------------------------------------------------- >> */
	/*			    DB variables			  */
	/* << -------------------------------------------------------- >> */
	Tlargo_code		v_largo_code;
	Tgen_comp_code	v_gen_comp_code;
	Tdel_flg_c		v_del_flg;

	char			ingr_description		[40 + 1];
	char			ingredient_type			[ 1 + 1];
	double			package_size;
	double			ingredient_qty;
	char			ingredient_units		[ 3 + 1];
	double			ingredient_per_qty;
	char			ingredient_per_units	[ 3 + 1];

	char			o_ingr_description		[40 + 1];
	char			o_ingredient_type		[ 1 + 1];
	double			o_package_size;
	double			o_ingredient_qty;
	char			o_ingredient_units		[ 3 + 1];
	double			o_ingredient_per_qty;
	char			o_ingredient_per_units	[ 3 + 1];

	int				sysdate,	systime;
	Tdel_flg_c		old_del_flg;
	int				v_sysdate,	v_systime;
	int				v_count;


	// Initialization.
	GetVirtualTimestamp (&sysdate, &systime);

/* << -------------------------------------------------------- >> */
/*			read input buffer			  */
/* << -------------------------------------------------------- >> */
  INIT_ERROR_DETECTION_MECHANISM;
  PosInBuff = glbMsgBuffer;

	// DonR 20Nov2025 User Story #434188: Add 7 new columns to table.
	v_largo_code			=		get_long	( &PosInBuff,							 9					);	RETURN_ON_ERROR;	//Marianna 14Jul2024 User Story #330877
	v_gen_comp_code			=		get_short	( &PosInBuff,							Tgen_comp_code_len	);	RETURN_ON_ERROR;
	/* v_del_flg			=	*/	get_string	( &PosInBuff,	v_del_flg,				Tdel_flg_c_len		);
	/* ingr_description		=	*/	get_string	( &PosInBuff,	ingr_description,		40					);
	/* ingredient_type		=	*/	get_string	( &PosInBuff,	ingredient_type,		 1					);
	package_size			=		get_double	( &PosInBuff,							20					);	RETURN_ON_ERROR;
	ingredient_qty			=		get_double	( &PosInBuff,							20					);	RETURN_ON_ERROR;
	/* ingredient_units		=	*/	get_string	( &PosInBuff,	ingredient_units,		 3					);
	ingredient_per_qty		=		get_double	( &PosInBuff,							20					);	RETURN_ON_ERROR;
	/* ingredient_per_units	=	*/	get_string	( &PosInBuff,	ingredient_per_units,	 3					);

/* << -------------------------------------------------------- >> */
/*			    debug messages			  */
/* << -------------------------------------------------------- >> */

  /* verify all data was read:   */
  ReadInput = PosInBuff - glbMsgBuffer;
  StaticAssert( ReadInput == data_len );

  // check difference in date 20020307
	ExecSQL (	MAIN_DB, AS400SRV_T2049_READ_DrugGenComponents_old_data,
				&v_sysdate,					&v_systime,				&old_del_flg,
				&o_ingr_description,		&o_ingredient_type,		&o_package_size,
				&o_ingredient_qty,			&o_ingredient_units,	&o_ingredient_per_qty,
				&o_ingredient_per_units,
				&v_gen_comp_code,			&v_largo_code,			END_OF_ARG_LIST		);
		
	if ( SQLERR_code_cmp( SQLERR_not_found ) == MAC_TRUE )
	{
      *shm_table_to_update = DRUGGENCOMPONENTS;
		do_insert = ATTEMPT_INSERT_FIRST; 
	}
	else if (SQLERR_code_cmp(SQLERR_access_conflict) == MAC_TRUE)
	{
	   sleep( SLEEP_TIME_BETWEEN_RETRIES );
	   retcode = ALL_RETRIES_WERE_USED;
	}
	else if ( SQLERR_error_test() )
    {
	    retcode = SQL_ERROR_OCCURED;
	}
	else   // No errors on fetch
	{
		do
		{
			if	(	strcmp (old_del_flg				,	v_del_flg			)	||
					strcmp (o_ingr_description		,	ingr_description	)	||
					strcmp (o_ingredient_type		,	ingredient_type		)	||
					(o_package_size					!=	package_size		)	||
					(o_ingredient_qty				!=	ingredient_qty		)	||
					strcmp (o_ingredient_units		,	ingredient_units	)	||
					(o_ingredient_per_qty			!=	ingredient_per_qty	)	||
					strcmp (o_ingredient_per_units	,	ingredient_units	)		)
			{
//				GerrLogFnameMini("2049",GerrId,"1:[%d][%d]",v_gen_comp_code,v_largo_code);
				*shm_table_to_update = DRUGGENCOMPONENTS;
				v_sysdate = sysdate ;
				v_systime = systime ;
				break;
			}
		}
		while (0);
	}	

/* << -------------------------------------------------------- >> */
/*	   up to SQL_UPDATE_RETRIES retries will be made			  */
/*		    to overcome access conflict							  */
/* << -------------------------------------------------------- >> */
  retcode = ALL_RETRIES_WERE_USED;
  stop_retrying = false;

  for(n_retries = 0;
      ( n_retries < SQL_UPDATE_RETRIES ) && ( stop_retrying == false );
      n_retries++ )
    {
      /* sleep for a while:    */
      if( n_retries )
      {
          sleep( SLEEP_TIME_BETWEEN_RETRIES );
      }

      /*
       *  try to insert/update new rec with the order depending on "do_insert":
       */
      for( i = 0;  i < 2;  i++ )
	{
	  if( do_insert == ATTEMPT_INSERT_FIRST )
	    {
			ExecSQL (	MAIN_DB, AS400SRV_T2049_INS_DrugGenComponents,
						&v_largo_code,			&v_gen_comp_code,
						&sysdate,				&systime,
						&UPDATED_BY_AS400,		&v_del_flg,
						&ingr_description,		&ingredient_type,
						&package_size,			&ingredient_qty,
						&ingredient_units,		&ingredient_per_qty,
						&ingredient_per_units,	END_OF_ARG_LIST				);

         HANDLE_INSERT_SQL_CODE( "2049",p_connect,  &do_insert,   &retcode,  &stop_retrying );

	    }
	  else
	    {
			ExecSQL (	MAIN_DB, AS400SRV_T2049_UPD_DrugGenComponents,
						&v_sysdate,				&v_systime,
						&UPDATED_BY_AS400,		&v_del_flg,
						&ingr_description,		&ingredient_type,
						&package_size,			&ingredient_qty,
						&ingredient_units,		&ingredient_per_qty,
						&ingredient_per_units,

						&v_gen_comp_code,		&v_largo_code,
						END_OF_ARG_LIST									);

          HANDLE_UPDATE_SQL_CODE( "2049",p_connect, &do_insert,  &retcode,   &stop_retrying );
	    }

	}/* for( i... */

    }/* for( n_retries... */

  if( n_retries == SQL_UPDATE_RETRIES )
  {
     WRITE_ACCESS_CONFLICT_TO_LOG( "2049" );
  }

  if( do_insert == ATTEMPT_INSERT_FIRST && retcode == MSG_HANDLER_SUCCEEDED )
  {
//      *shm_table_to_update = DRUGGENCOMPONENTS;
  }

  if (retcode == MSG_HANDLER_SUCCEEDED)
  {
	  if (do_insert == ATTEMPT_INSERT_FIRST)
		  rows_added++;
	  else
		  rows_updated++;
  }


  return retcode;
} /* end of msg 2049 handler */


/* << -------------------------------------------------------- >> */
/*                                                                */
/*       handler for message 2050 - upd/ins to GENERYINTERACTION  */
/* << -------------------------------------------------------- >> */
static int message_2050_handler(
				TConnection	*p_connect,
				int		data_len,
				bool	do_insert,
				TShmUpdate	*shm_table_to_update
				)
{
/* << -------------------------------------------------------- >> */
/*			  local variables			  */
/* << -------------------------------------------------------- >> */
  char      *PosInBuff;		/* current pos in data buffer	    */
  int		ReadInput;		/* amount of Bytesread from buffer  */
  int		retcode;		/* code returned from function	    */
  bool		stop_retrying;	/* if true - return from function   */
  int		n_retries;		/* retrying counter		    */
  int		i;

	/* << -------------------------------------------------------- >> */
	/*			    DB variables			  */
	/* << -------------------------------------------------------- >> */
	Tgen_comp_code		v_gen_comp_code_first;
	Tgen_comp_code		v_gen_comp_code_second;
	Tinteraction_type	v_interaction_type;
	Tinter_note_code	v_inter_note_code;
	Tdel_flg_c			v_del_flg;
	int					sysdate,	systime;
	Tinteraction_type	old_interaction_type;
	Tinter_note_code	old_inter_note_code;
	Tdel_flg_c			old_del_flg;
	int					v_sysdate,	v_systime;
	int					v_count;


	// Initialization.
	GetVirtualTimestamp (&sysdate, &systime);

/* << -------------------------------------------------------- >> */
/*			read input buffer			  */
/* << -------------------------------------------------------- >> */
  INIT_ERROR_DETECTION_MECHANISM;
  PosInBuff = glbMsgBuffer;

  v_gen_comp_code_first		=  get_short(  &PosInBuff,    Tgen_comp_code_len  );		RETURN_ON_ERROR;
  v_gen_comp_code_second	=  get_short(  &PosInBuff,    Tgen_comp_code_len  );		RETURN_ON_ERROR;
  v_interaction_type		=  get_short(  &PosInBuff,    Tinteraction_type_len  );		RETURN_ON_ERROR;
  v_inter_note_code			=  get_short(  &PosInBuff,    Tinter_note_code_len  );		RETURN_ON_ERROR;
  get_string		(   &PosInBuff, v_del_flg,Tdel_flg_c_len    );


/* << -------------------------------------------------------- >> */
/*			    debug messages			  */
/* << -------------------------------------------------------- >> */

  /* verify all data was read:   */
  ReadInput = PosInBuff - glbMsgBuffer;
  StaticAssert( ReadInput == data_len );


    // check difference in date 20020307
	ExecSQL (	MAIN_DB, AS400SRV_T2050_READ_GeneryInteraction_old_data,
				&old_interaction_type,		&old_inter_note_code,
				&v_sysdate,					&v_systime,
				&old_del_flg,
				&v_gen_comp_code_first,		&v_gen_comp_code_second,
				END_OF_ARG_LIST												);
		
	if ( SQLERR_code_cmp( SQLERR_not_found ) == MAC_TRUE )
	{
       *shm_table_to_update = GENERYINTERACTION;
		do_insert = ATTEMPT_INSERT_FIRST; 
	}
	else if (SQLERR_code_cmp(SQLERR_access_conflict) == MAC_TRUE)
	{
	   sleep( SLEEP_TIME_BETWEEN_RETRIES );
	   retcode = ALL_RETRIES_WERE_USED;
	}
	else if ( SQLERR_error_test() )
    {
	    retcode = SQL_ERROR_OCCURED;
	}
	else   // No errors on fetch
	{
		do
		{

			if ( old_interaction_type != v_interaction_type)
			{
				GerrLogFnameMini("2050",GerrId,"1:[%d][%d]",v_gen_comp_code_first,v_gen_comp_code_second);		   
				*shm_table_to_update = GENERYINTERACTION;
				v_sysdate = sysdate ;
				v_systime = systime ;
				break;
			}
			if ( old_inter_note_code != v_inter_note_code)
			{
				GerrLogFnameMini("2050",GerrId,"2:[%d][%d]",v_gen_comp_code_first,v_gen_comp_code_second);		   
				*shm_table_to_update = GENERYINTERACTION;
				v_sysdate = sysdate ;
				v_systime = systime ;
				break;
			}
			if ( strcmp (old_del_flg , v_del_flg))
			{
				GerrLogFnameMini("2050",GerrId,"3:[%d][%d]",v_gen_comp_code_first,v_gen_comp_code_second);		   
				*shm_table_to_update = GENERYINTERACTION;
				v_sysdate = sysdate ;
				v_systime = systime ;
				break;
			}
		}
		while (0);
	}	

/* << -------------------------------------------------------- >> */
/*	   up to SQL_UPDATE_RETRIES retries will be made			  */
/*		    to overcome access conflict							  */
/* << -------------------------------------------------------- >> */
  retcode = ALL_RETRIES_WERE_USED;
  stop_retrying = false;

  for(n_retries = 0;
      ( n_retries < SQL_UPDATE_RETRIES ) && ( stop_retrying == false );
      n_retries++ )
    {
      /* sleep for a while:    */
      if( n_retries )
      {
          sleep( SLEEP_TIME_BETWEEN_RETRIES );
      }

      /*
       *  try to insert/update new rec with the order depending on "do_insert":
       */
      for( i = 0;  i < 2;  i++ )
	{
	  if( do_insert == ATTEMPT_INSERT_FIRST )
	    {
			ExecSQL (	MAIN_DB, AS400SRV_T2050_INS_GeneryInteraction,
						&v_gen_comp_code_first,		&v_gen_comp_code_second,
						&v_interaction_type,		&v_inter_note_code ,
						&sysdate,					&systime,
						&UPDATED_BY_AS400,			&v_del_flg,
						END_OF_ARG_LIST											);

			 // Dummy loop so statements below aren't unreachable.
			 do
			 {
				 HANDLE_INSERT_SQL_CODE( "2050",p_connect,  &do_insert,   &retcode,  &stop_retrying );
			 }
			 while (0);

			*shm_table_to_update = GENERYINTERACTION; //20020828
			v_sysdate = sysdate ;
			v_systime = systime ;

		}
	  else
	    {
			ExecSQL (	MAIN_DB, AS400SRV_T2050_UPD_GeneryInteraction,
						&v_inter_note_code,			&v_interaction_type,
						&v_sysdate,					&v_systime,
						&UPDATED_BY_AS400,			&v_del_flg,
						&v_gen_comp_code_first,		&v_gen_comp_code_second,
						END_OF_ARG_LIST											);

          HANDLE_UPDATE_SQL_CODE( "2050",p_connect,   &do_insert,  &retcode,   &stop_retrying );
	    }

	}/* for( i... */

    }/* for( n_retries... */

  if( n_retries == SQL_UPDATE_RETRIES )
  {
     WRITE_ACCESS_CONFLICT_TO_LOG( "2050" );
  }

  if( do_insert == ATTEMPT_INSERT_FIRST && retcode == MSG_HANDLER_SUCCEEDED )
  {
//      *shm_table_to_update = GENERYINTERACTION;
  }

  if (retcode == MSG_HANDLER_SUCCEEDED)
  {
	  if (do_insert == ATTEMPT_INSERT_FIRST)
		  rows_added++;
	  else
		  rows_updated++;
  }


  return retcode;
} /* end of msg 2050 handler */


/* << -------------------------------------------------------- >> */
/*                                                                */
/*       handler for message 2056 - upd/ins to GENINTERNOTES      */
/* << -------------------------------------------------------- >> */
static int message_2056_handler(
				TConnection	*p_connect,
				int		data_len,
				bool	do_insert,
				TShmUpdate	*shm_table_to_update
				)
{
/* << -------------------------------------------------------- >> */
/*			  local variables			  */
/* << -------------------------------------------------------- >> */
  char      *PosInBuff;		/* current pos in data buffer	    */
  int		ReadInput;		/* amount of Bytesread from buffer  */
  int		retcode;		/* code returned from function	    */
  bool		stop_retrying;	/* if true - return from function   */
  int		n_retries;		/* retrying counter		    */
  int		i;

	/* << -------------------------------------------------------- >> */
	/*			    DB variables			  */
	/* << -------------------------------------------------------- >> */
	Tinter_note_code	v_inter_note_code;
	Tinteraction_note	v_interaction_note;
	Tinteraction_note	old_interaction_note;
	char				v_inter_long_note [100 + 1];
	char				old_inter_long_note [100 + 1];
	Tdel_flg_c			v_del_flg;
	int					sysdate,	systime;
	Tdel_flg_c			old_del_flg;
	int					v_sysdate,	v_systime;


	GetVirtualTimestamp (&sysdate, &systime);

/* << -------------------------------------------------------- >> */
/*			read input buffer			  */
/* << -------------------------------------------------------- >> */
  INIT_ERROR_DETECTION_MECHANISM;
  PosInBuff = glbMsgBuffer;

  v_inter_note_code	= get_short (&PosInBuff, Tinter_note_code_len); RETURN_ON_ERROR;
  get_string					(&PosInBuff, v_interaction_note,	Tinteraction_note_len);
  get_string					(&PosInBuff, v_inter_long_note,		100);
  get_string					(&PosInBuff, v_del_flg,				Tdel_flg_c_len);

/* << -------------------------------------------------------- >> */
/*			    debug messages			  */
/* << -------------------------------------------------------- >> */

  /* verify all data was read:   */
  ReadInput = PosInBuff - glbMsgBuffer;
  StaticAssert( ReadInput == data_len );

  // DonR 20Feb2020: ODBC (at least for Informix) automatically strips trailing spaces
  // when SELECTing VARCHAR columns - so unless we do the same for the "new" values
  // from AS/400, we're going to think that every refresh is an actual change.
  strip_trailing_spaces (v_interaction_note);
  strip_trailing_spaces (v_inter_long_note);

  switch_to_win_heb ((unsigned char *)v_interaction_note);
  switch_to_win_heb ((unsigned char *)v_inter_long_note);


  // check difference in date 20020307
	ExecSQL (	MAIN_DB, AS400SRV_T2056_READ_GenInterNotes_old_data,
				&old_interaction_note,		&old_inter_long_note,
				&v_sysdate,					&v_systime,
				&old_del_flg,
				&v_inter_note_code,			END_OF_ARG_LIST					);
		
	if ( SQLERR_code_cmp( SQLERR_not_found ) == MAC_TRUE )
	{
		*shm_table_to_update = GENINTERNOTE;
		do_insert = ATTEMPT_INSERT_FIRST; 
	}
	else if (SQLERR_code_cmp(SQLERR_access_conflict) == MAC_TRUE)
	{
	   sleep( SLEEP_TIME_BETWEEN_RETRIES );
	   retcode = ALL_RETRIES_WERE_USED;
	}
	else if ( SQLERR_error_test() )
    {
	    retcode = SQL_ERROR_OCCURED;
	}
	else   // No errors on fetch
	{
		do
		{
			if ( strcmp(old_interaction_note ,v_interaction_note))
			{
				GerrLogFnameMini("2056",GerrId,"1:[%d]",v_inter_note_code);
				*shm_table_to_update = GENINTERNOTE;
				v_sysdate = sysdate ;
				v_systime = systime ;
				break;
			}
			if ( strcmp (old_del_flg , v_del_flg))
			{
				GerrLogFnameMini("2056",GerrId,"2:[%d]",v_inter_note_code);
				*shm_table_to_update = GENINTERNOTE;
				v_sysdate = sysdate ;
				v_systime = systime ;
				break;
			}
			if ( strcmp(old_inter_long_note ,v_inter_long_note))
			{
				GerrLogFnameMini("2056",GerrId,"3:[%d]",v_inter_note_code);
				*shm_table_to_update = GENINTERNOTE;
				v_sysdate = sysdate ;
				v_systime = systime ;
				break;
			}
		}
		while (0);
	}	

/* << -------------------------------------------------------- >> */
/*	   up to SQL_UPDATE_RETRIES retries will be made			  */
/*		    to overcome access conflict							  */
/* << -------------------------------------------------------- >> */
  retcode = ALL_RETRIES_WERE_USED;
  stop_retrying = false;

  for(n_retries = 0;
      ( n_retries < SQL_UPDATE_RETRIES ) && ( stop_retrying == false );
      n_retries++ )
    {
      /* sleep for a while:    */
      if( n_retries )
      {
          sleep( SLEEP_TIME_BETWEEN_RETRIES );
      }

      /*
       *  try to insert/update new rec with the order depending on "do_insert":
       */
      for( i = 0;  i < 2;  i++ )
	{
	  if( do_insert == ATTEMPT_INSERT_FIRST )
	    {
			ExecSQL (	MAIN_DB, AS400SRV_T2056_INS_GenInterNotes,
						&v_inter_note_code,			&v_interaction_note,
						&v_inter_long_note,			&sysdate,
						&systime,					&UPDATED_BY_AS400,
						&v_del_flg,					END_OF_ARG_LIST			);

         HANDLE_INSERT_SQL_CODE( "2056",p_connect, &do_insert,   &retcode,  &stop_retrying );

	    }
		else
	    {
			ExecSQL (	MAIN_DB, AS400SRV_T2056_UPD_GenInterNotes,
						&v_interaction_note,		&v_inter_long_note,
						&v_sysdate,					&v_systime,
						&UPDATED_BY_AS400,			&v_del_flg,
						&v_inter_note_code,			END_OF_ARG_LIST		);

          HANDLE_UPDATE_SQL_CODE( "2056",p_connect,  &do_insert,  &retcode,   &stop_retrying );
	    }

	}/* for( i... */

    }/* for( n_retries... */

  if( n_retries == SQL_UPDATE_RETRIES )
  {
     WRITE_ACCESS_CONFLICT_TO_LOG( "2056" );
  }

  if( do_insert == ATTEMPT_INSERT_FIRST && retcode == MSG_HANDLER_SUCCEEDED )
  {
//      *shm_table_to_update = GENINTERNOTE;
  }

  if (retcode == MSG_HANDLER_SUCCEEDED)
  {
	  if (do_insert == ATTEMPT_INSERT_FIRST)
		  rows_added++;
	  else
		  rows_updated++;
  }

  return retcode;
} /* end of msg 2056 handler */



/* << -------------------------------------------------------- >> */
/*                                                                */
/*       handler for message 2051 - upd/ins to GNRLDRUGNOTES	  */
/* << -------------------------------------------------------- >> */
static int message_2051_handler(
				TConnection	*p_connect,
				int		data_len,
				bool	do_insert,
				TShmUpdate	*shm_table_to_update
				)
{
	// local variables
	char		*PosInBuff;		/* current pos in data buffer	    */
	int			ReadInput;		/* amount of Bytesread from buffer  */
	int			retcode;		/* code returned from function	    */
	bool		stop_retrying;	/* if true - return from function   */
	int			n_retries;		/* retrying counter		    */
	int			i;

	// DB variables
	Tgnrldrugnotetype	v_gnrldrugnotetype;
	Tgnrldrugnotecode	v_gnrldrugnotecode;
	Tgnrldrugnote		v_gnrldrugnote;
	char				v_gdn_type_new		[  1 + 1];
	char				v_gdn_long_note		[200 + 1];
	short				v_gdn_category;
	short				v_gdn_sex;
	short				v_gdn_from_age;
	short				v_gdn_to_age;
	short				v_gdn_seq_num;
	short				v_gdn_connect_type;
	char				v_gdn_connect_desc	[ 25 + 1];
	short				v_gdn_severity;
	int					v_gdn_sensitv_type;
	char				v_gdn_sensitv_desc	[ 25 + 1];
	Tdel_flg_c			v_del_flg;
	int					sysdate,	systime;
	Tgnrldrugnote		old_gnrldrugnote;
	Tdel_flg_c			old_del_flg;


	GetVirtualTimestamp (&sysdate, &systime);

	// read input buffer
	INIT_ERROR_DETECTION_MECHANISM;
	PosInBuff = glbMsgBuffer;

	get_string						(&PosInBuff, v_gnrldrugnotetype,	Tgnrldrugnotetype_len	);
	get_string						(&PosInBuff, v_gdn_type_new,		Tgdn_type_new_len		);
	v_gnrldrugnotecode	=  get_long	(&PosInBuff, Tgnrldrugnotecode_len	);	RETURN_ON_ERROR;
	get_string						(&PosInBuff, v_gnrldrugnote,		Tgnrldrugnote_len		);
	get_string						(&PosInBuff, v_gdn_long_note,		Tgdn_long_note_len		);
	v_gdn_category		=  get_short(&PosInBuff, Tgdn_category_len		);	RETURN_ON_ERROR;
	v_gdn_sex				=  get_short(&PosInBuff, Tgdn_sex_len			);	RETURN_ON_ERROR;
	v_gdn_from_age		=  get_short(&PosInBuff, Tgdn_from_age_len		);	RETURN_ON_ERROR;
	v_gdn_to_age			=  get_short(&PosInBuff, Tgdn_to_age_len		);	RETURN_ON_ERROR;
	v_gdn_seq_num			=  get_short(&PosInBuff, Tgdn_seq_num_len		);	RETURN_ON_ERROR;
	v_gdn_connect_type	=  get_short(&PosInBuff, Tgdn_connect_type_len	);	RETURN_ON_ERROR;
	get_string						(&PosInBuff, v_gdn_connect_desc,	Tgdn_connect_desc_len	);
	v_gdn_severity		=  get_short(&PosInBuff, Tgdn_severity_len		);	RETURN_ON_ERROR;
	v_gdn_sensitv_type	=  get_long	(&PosInBuff, Tgdn_sensitv_type_len	);	RETURN_ON_ERROR;
	get_string						(&PosInBuff, v_gdn_sensitv_desc,	Tgdn_sensitv_desc_len	);
	get_string						(&PosInBuff, v_del_flg,				Tdel_flg_c_len			);


	/* verify all data was read:   */
	ReadInput = PosInBuff - glbMsgBuffer;
	StaticAssert( ReadInput == data_len );


	switch_to_win_heb ((unsigned char *)v_gnrldrugnote);
	switch_to_win_heb ((unsigned char *)v_gdn_long_note);
	switch_to_win_heb ((unsigned char *)v_gdn_connect_desc);
	switch_to_win_heb ((unsigned char *)v_gdn_sensitv_desc);


	*shm_table_to_update = GNRLDRUGNOTES;
//	v_sysdate = sysdate ;
//	v_systime = systime ;


/* << -------------------------------------------------------- >> */
/*	   up to SQL_UPDATE_RETRIES retries will be made			  */
/*		    to overcome access conflict							  */
/* << -------------------------------------------------------- >> */
  retcode = ALL_RETRIES_WERE_USED;
  stop_retrying = false;

  for(n_retries = 0;
      ( n_retries < SQL_UPDATE_RETRIES ) && ( stop_retrying == false );
      n_retries++ )
    {
      /* sleep for a while:    */
      if( n_retries )
      {
          sleep( SLEEP_TIME_BETWEEN_RETRIES );
      }

      /*
       *  try to insert/update new rec with the order depending on "do_insert":
       */
      for( i = 0;  i < 2;  i++ )
	{
	  if( do_insert == ATTEMPT_INSERT_FIRST )
	    {
			ExecSQL (	MAIN_DB, AS400SRV_T2051_INS_GnrlDrugNotes,
						&v_gnrldrugnotetype,		&v_gnrldrugnotecode,
						&v_gnrldrugnote,			&v_gdn_type_new,
						&v_gdn_long_note,			&v_gdn_category,
						&v_gdn_sex,					&v_gdn_from_age,
						&v_gdn_to_age,				&v_gdn_seq_num,
						&v_gdn_connect_type,		&v_gdn_connect_desc,
						&v_gdn_severity,			&v_gdn_sensitv_type,
						&v_gdn_sensitv_desc,		&sysdate,
						&systime,					&UPDATED_BY_AS400,
						&v_del_flg,					END_OF_ARG_LIST			);

         HANDLE_INSERT_SQL_CODE( "2051",p_connect,  &do_insert,   &retcode,  &stop_retrying );

	    }
	  else
	    {
			ExecSQL (	MAIN_DB, AS400SRV_T2051_UPD_GnrlDrugNotes,
						&v_gnrldrugnote,		&v_gdn_type_new,
						&v_gdn_long_note,		&v_gdn_category,
						&v_gdn_sex,				&v_gdn_from_age,
						&v_gdn_to_age,			&v_gdn_seq_num,
						&v_gdn_connect_type,	&v_gdn_connect_desc,
						&v_gdn_severity,		&v_gdn_sensitv_type,
						&v_gdn_sensitv_desc,	&sysdate,
						&systime,				&UPDATED_BY_AS400,
						&v_del_flg,
						&v_gnrldrugnotetype,	&v_gnrldrugnotecode,
						END_OF_ARG_LIST									);

          HANDLE_UPDATE_SQL_CODE( "2051",p_connect,  &do_insert,  &retcode,   &stop_retrying );
	    }

	}/* for( i... */

    }/* for( n_retries... */

  if( n_retries == SQL_UPDATE_RETRIES )
  {
     WRITE_ACCESS_CONFLICT_TO_LOG( "2051" );
  }

  if( do_insert == ATTEMPT_INSERT_FIRST && retcode == MSG_HANDLER_SUCCEEDED )
  {
//      *shm_table_to_update = GNRLDRUGNOTES;
  }

  if (retcode == MSG_HANDLER_SUCCEEDED)
  {
	  if (do_insert == ATTEMPT_INSERT_FIRST)
		  rows_added++;
	  else
		  rows_updated++;
  }
  return retcode;
} /* end of msg 2051 handler */


// << -------------------------------------------------------- >>
//
//       handler for message 2052/4052 - upd/ins to DRUGNOTES
//			
//		DonR 07Jan2004: Deletions and insertions/updates are
//		now stored separately in the DrugNotes table, so that
//		CLICKS is sure to get a deletion (which the AS400
//		promises to provide) before every update.
//
//		DonR 16Jan2012: It appears that deletion is no longer
//		required before every update - so ignore the second part
//		of the previous comment. However, the table is still
//		keyed by deletion flag.
// << -------------------------------------------------------- >>
static int data_2052_handler (TConnection	*p_connect,
							  int			data_len)
{
/* << -------------------------------------------------------- >> */
/*			  local variables			  */
/* << -------------------------------------------------------- >> */
  char      *PosInBuff;		/* current pos in data buffer	    */
  int		ReadInput;		/* amount of Bytesread from buffer  */
  int		retcode;		/* code returned from function	    */
  bool		stop_retrying;	/* if true - return from function   */
  bool		do_insert = ATTEMPT_UPDATE_FIRST;
  int		n_retries;		/* retrying counter		    */
  int		i;

	/* << -------------------------------------------------------- >> */
	/*			    DB variables			  */
	/* << -------------------------------------------------------- >> */
	Tlargo_code			v_largo_code;
	Tgnrldrugnotetype	v_gnrldrugnotetype;
	Tgnrldrugnotecode	v_gnrldrugnotecode;
	Tdel_flg_c			v_del_flg;
	int					v_as400_batch_date;
	int					v_as400_batch_time;
	int					v_refresh_date;
	int					v_refresh_time;
	int					v_update_date;
	int					v_update_time;
	int					v_update_date_o;
	int					v_update_time_o;
	short				pharm_real_time;	// Marianna 09Feb2023 User Story #417219 


	GetVirtualTimestamp (&v_update_date, &v_update_time);
	v_refresh_date = GetDate();
	v_refresh_time = GetTime();
	v_as400_batch_date = (download_start_date > 0) ? download_start_date : v_refresh_date;
	v_as400_batch_time = (download_start_time > 0) ? download_start_time : v_refresh_time;

/* << -------------------------------------------------------- >> */
/*			read input buffer			  */
/* << -------------------------------------------------------- >> */
  INIT_ERROR_DETECTION_MECHANISM;
  PosInBuff = glbMsgBuffer;

	GET_LONG	(v_largo_code				, 9							);	//Marianna 14Jul2024 User Story #330877
	GET_STRING	(v_gnrldrugnotetype			, Tgnrldrugnotetype_len		);
	GET_LONG	(v_gnrldrugnotecode			, Tgnrldrugnotecode_len		);
	GET_STRING	(v_del_flg					, Tdel_flg_c_len			);
	
	pharm_real_time = get_short(&PosInBuff	, 1							); RETURN_ON_ERROR;	// Marianna 09Feb2023 User Story #417219 


/* << -------------------------------------------------------- >> */
/*			    debug messages			  */
/* << -------------------------------------------------------- >> */

  /* verify all data was read:   */
  ReadInput = PosInBuff - glbMsgBuffer;
  StaticAssert( ReadInput == data_len );


	/* << -------------------------------------------------------- >> */
	/*	   up to SQL_UPDATE_RETRIES retries will be made			  */
	/*		    to overcome access conflict							  */
	/* << -------------------------------------------------------- >> */
	retcode = ALL_RETRIES_WERE_USED;
	stop_retrying = false;

	for (n_retries = 0;
		 (n_retries < SQL_UPDATE_RETRIES ) && ( stop_retrying == false );
		 n_retries++)
	{
		/* sleep for a while:    */
		if (n_retries)
		{
			sleep (SLEEP_TIME_BETWEEN_RETRIES);
		}

		// See if row is already present.
		// DonR 07Jan2004: Del_flg is now part of the key to DrugNotes, and the
		// comparison is performed on the existing row's update date and time
		// rather than on its deletion flag.
		ExecSQL (	MAIN_DB, AS400SRV_T4052_READ_DrugNotes_old_data,
					&v_update_date_o,		&v_update_time_o,
					&v_largo_code,			&v_gnrldrugnotetype,
					&v_gnrldrugnotecode,	&v_del_flg,
					END_OF_ARG_LIST										);

		if (SQLERR_code_cmp (SQLERR_not_found) == MAC_TRUE)
		{
			do_insert = ATTEMPT_INSERT_FIRST; 
		}
		else if (SQLERR_code_cmp (SQLERR_access_conflict) == MAC_TRUE)
		{
		   sleep (SLEEP_TIME_BETWEEN_RETRIES);
		   retcode = ALL_RETRIES_WERE_USED;
		   continue;
		}
		else if (SQLERR_error_test ())
		{
			retcode = SQL_ERROR_OCCURED;
			break;
		}
		else   // No errors on fetch
		{
			// DonR 16Jan2012: For RKUNIX downloads, we want to leave the previous update_date/time
			// alone; but for RKFILPRD intra-day downloads, we want to update the timestamp even
			// if the row already exists.
			if ((download_start_date > 0)	&&	// RKUNIX full-table download.
				(pharm_real_time == 0))			//	Marianna 12Feb2023 User Story #417219
			{
				v_update_date = v_update_date_o;
				v_update_time = v_update_time_o;
			}
		}	


		// Try to insert/update new rec with the order depending on "do_insert":
		for (i = 0;  i < 2;  i++)
		{
			if (do_insert == ATTEMPT_INSERT_FIRST)
			{
				ExecSQL (	MAIN_DB, AS400SRV_T4052_INS_DrugNotes,
							&v_largo_code,			&v_gnrldrugnotetype,
							&v_gnrldrugnotecode,	&v_update_date,
							&v_update_time,			&v_refresh_date,
							&v_refresh_time,		&v_as400_batch_date,
							&v_as400_batch_time,	&UPDATED_BY_AS400,
							&v_del_flg,				END_OF_ARG_LIST			);

				HANDLE_INSERT_SQL_CODE( "4052",p_connect, &do_insert,   &retcode,  &stop_retrying );
			}
			else
			{
				// DonR 07Jan2004: Deletion Flag is now part of the row's key, and is no
				// longer an updated value.
				ExecSQL (	MAIN_DB, AS400SRV_T4052_UPD_DrugNotes,
							&v_update_date,			&v_update_time,
							&v_refresh_date,		&v_refresh_time,
							&v_as400_batch_date,	&v_as400_batch_time,
							&UPDATED_BY_AS400,		&v_largo_code,
							&v_gnrldrugnotetype,	&v_gnrldrugnotecode,
							&v_del_flg,				END_OF_ARG_LIST			);

				HANDLE_UPDATE_SQL_CODE( "4052",p_connect, &do_insert,  &retcode,   &stop_retrying );
			}
		}/* for( i... */
	}/* for( n_retries... */

	if (n_retries == SQL_UPDATE_RETRIES)
	{
		WRITE_ACCESS_CONFLICT_TO_LOG ("4052");
	}

  if (retcode == MSG_HANDLER_SUCCEEDED)
  {
	  if (do_insert == ATTEMPT_INSERT_FIRST)
		  rows_added++;
	  else
	  {
		  if (download_start_date > 0)
			  rows_refreshed++;
		  else
			  rows_updated++;
	  }
  }

	return retcode;
} /* end of msg 2052/4052 handler */

// -------------------------------------------------------------------------
//
//       Post-processor for message 4052 - logically delete DRUGNOTES
//			rows that are no longer in the AS/400 full-table download.
//
// -------------------------------------------------------------------------
static int postprocess_4052 (TConnection *p_connect)
{
	// local variables
	bool	stop_retrying;	// if true - return from function
	int		rows_deleted	= 0;
	int		retcode;		// code returned from function
	int		n_retries;		// retrying counter
	int		db_changed_flg;

	// DB variables
	Tlargo_code			v_largo_code;
	Tgnrldrugnotetype	v_gnrldrugnotetype;
	Tgnrldrugnotecode	v_gnrldrugnotecode;
	int					v_as400_batch_date;
	int					v_as400_batch_time;
	int					v_update_date;
	int					v_update_time;


	// Initialize variables.
	v_as400_batch_date	= download_start_date;
	v_as400_batch_time	= download_start_time;

	retcode			= ALL_RETRIES_WERE_USED;
	stop_retrying	= false;

	DeclareCursorInto (	MAIN_DB, AS400SRV_T4052PP_stale_DrugNotes_cur,
						&v_largo_code,			&v_gnrldrugnotetype,
						&v_gnrldrugnotecode,
						&v_as400_batch_date,	&v_as400_batch_time,
						END_OF_ARG_LIST									);

	OpenCursor (	MAIN_DB, AS400SRV_T4052PP_stale_DrugNotes_cur	);

	if (SQLERR_error_test ())
	{
		retcode = SQL_ERROR_OCCURED;
	}

	while (1)
	{
		FetchCursor (	MAIN_DB, AS400SRV_T4052PP_stale_DrugNotes_cur	);

		if (SQLERR_code_cmp (SQLERR_end_of_fetch) == MAC_TRUE)
		{
			retcode = MSG_HANDLER_SUCCEEDED;	// In case there is nothing that needs to be deleted.
			break;
		}
		else
		{
			if (SQLERR_error_test ())
			{
				retcode = SQL_ERROR_OCCURED;
				break;
			}
		}

		// Calculate virtual time. Note that for this download the time is based
		// on the real time, adjusted only to ensure that we don't have too many
		// rules with the exact same as-of time.
		v_update_date		= download_start_date;
		v_update_time		= download_start_time + (download_row_count++ / VIRTUAL_PORTION);

		// DonR 16Jan2012: There may already be an old deleted row for this note; delete the old
		// version so the update (with a nice new update_date/time) succeeds.
		ExecSQL (	MAIN_DB, AS400SRV_T4052PP_DEL_DrugNotes_old_deletions,
					&v_largo_code,			&v_gnrldrugnotetype,
					&v_gnrldrugnotecode,	END_OF_ARG_LIST					);


		ExecSQL (	MAIN_DB, AS400SRV_T4052PP_UPD_DrugNotes_logically_delete,
					&v_update_date, &v_update_time, END_OF_ARG_LIST				);

		HANDLE_DELETE_SQL_CODE ("postprocess_4052", p_connect, &retcode, &db_changed_flg);

		rows_deleted++;
	}	// Cursor-scrolling loop.

	CloseCursor (	MAIN_DB, AS400SRV_T4052PP_stale_DrugNotes_cur	);


	// DonR 13Nov2018 CR #15919: Record current table size.
	REMEMBER_ISOLATION;
	SET_ISOLATION_DIRTY;

	ExecSQL (	MAIN_DB, AS400SRV_T4052PP_READ_DrugNotes_table_size,
				&total_rows, END_OF_ARG_LIST							);

	ExecSQL (	MAIN_DB, AS400SRV_T4052PP_READ_DrugNotes_active_size,
				&active_rows, END_OF_ARG_LIST							);

	RESTORE_ISOLATION;
	// DonR 13Nov2018 CR #15919 end.


	GerrLogMini (GerrId,
				 "Postprocess 4052: %d rows logically deleted, retcode = %d.",
				 rows_deleted, retcode);

	return retcode;

}	// End of msg 4052 postprocessor.


/* << ------------------------------------------------------------- >> */
/*                                                                     */
/*       handler for message 2053/4053 - upd/ins to DRUGDOCTORPROF     */
/* << ------------------------------------------------------------- >> */
static int data_4053_handler (TConnection	*p_connect,
							  int			data_len)
{
/* << -------------------------------------------------------- >> */
/*			  local variables			  */
/* << -------------------------------------------------------- >> */
  char      *PosInBuff;		/* current pos in data buffer	    */
  int		ReadInput;		/* amount of Bytesread from buffer  */
  int		retcode;		/* code returned from function	    */
  bool		stop_retrying;	/* if true - return from function   */
  int		n_retries;		/* retrying counter		    */
  int		i;
  bool		something_changed;
  short		do_insert = ATTEMPT_UPDATE_FIRST;

	/* << -------------------------------------------------------- >> */
	/*			    DB variables			  */
	/* << -------------------------------------------------------- >> */
	Tlargo_code		v_largo_code;
	Tprof			v_prof;
	Tprcnt			v_prcnt_gnrl;
	Tprcnt			v_prcnt_krn;
	Tprcnt			v_prcnt_mgn;
	Tdel_flg_c		v_del_flg;
	short			v_enabled_status;
	int				sysdate,	systime;
	Tprcnt			old_prcnt_gnrl;
	Tprcnt			old_prcnt_krn;
	Tprcnt			old_prcnt_mgn;
	Tdel_flg_c		old_del_flg;
	int				v_basic_fixed_price;
	int				v_kesef_fixed_price;
	int				v_zahav_fixed_price;
	int				v_basic_fp_old;
	int				v_kesef_fp_old;
	int				v_zahav_fp_old;
	int				v_sysdate,	v_systime;
	int				v_as400_batch_date;
	int				v_as400_batch_time;
	TGenericFlag	v_in_health_basket; /* DonR 18Dec2007 */
	TGenericFlag	o_in_health_basket; /* DonR 18Dec2007 */
	TGenericFlag	o_HealthBasketNew;
	int				v_percnt_y;
	int				v_fixed_pr_yahalom;
	short			v_wait_months;
	int				o_percnt_y;
	int				o_fixed_pr_yahalom;
	short			o_wait_months;


	GetVirtualTimestamp (&sysdate, &systime);
	v_as400_batch_date = (download_start_date > 0) ? download_start_date : GetDate();
	v_as400_batch_time = (download_start_time > 0) ? download_start_time : GetTime();

/* << -------------------------------------------------------- >> */
/*			read input buffer			  */
/* << -------------------------------------------------------- >> */
  INIT_ERROR_DETECTION_MECHANISM;
  PosInBuff = glbMsgBuffer;
  v_largo_code		=  get_long(  &PosInBuff,	9						);  RETURN_ON_ERROR;	//Marianna 14Jul2024 User Story #330877
  get_string(   &PosInBuff, v_prof,Tprof_len    );
  v_prcnt_gnrl		=  get_long(  &PosInBuff,    Tprcnt_len  );		  RETURN_ON_ERROR;
  v_prcnt_krn	=  get_long(  &PosInBuff,    Tprcnt_len  );			  RETURN_ON_ERROR;
  v_prcnt_mgn	=  get_long(  &PosInBuff,    Tprcnt_len  );			  RETURN_ON_ERROR;
  get_string(   &PosInBuff, v_del_flg,Tdel_flg_c_len    );
  v_basic_fixed_price	=  get_long(  &PosInBuff,    Tfixed_price_len  );	  RETURN_ON_ERROR;
  v_kesef_fixed_price	=  get_long(  &PosInBuff,    Tfixed_price_len  );	  RETURN_ON_ERROR;
  v_zahav_fixed_price	=  get_long(  &PosInBuff,    Tfixed_price_len  );	  RETURN_ON_ERROR;
  v_in_health_basket	= get_short	(&PosInBuff, TGenericFlag_len			); RETURN_ON_ERROR;
  GET_LONG	(v_percnt_y					, Tprcnt_len			);
  GET_LONG	(v_fixed_pr_yahalom			, Tfixed_price_len		);
  GET_SHORT	(v_wait_months				, Twait_months_len		);

/* << -------------------------------------------------------- >> */
/*			    debug messages			  */
/* << -------------------------------------------------------- >> */

  /* verify all data was read:   */
  ReadInput = PosInBuff - glbMsgBuffer;
  StaticAssert( ReadInput == data_len );

	// DonR 26Oct2008: Set "old" health-basket flag to 1 or 0 based on the value received
	// from AS400, and move the actual value received to the "new" health-basket flag
	// variable.
	PROCESS_HEALTH_BASKET;

	// DonR 21Nov2011: Interpret Deletion Flag to derive the Enabled Status, which is now part of the key
	// for this table.
	if ((*v_del_flg >= '0') && (*v_del_flg <= '9'))
	{
		v_enabled_status = *v_del_flg - '0';
	}
	else
	{
		if ((*v_del_flg >= 'A') && (*v_del_flg <= 'E'))
		{
			v_enabled_status = 1 + *v_del_flg - 'A';
		}
		else
		{
			v_enabled_status = 0;
		}
	}

    // check difference in date 20020307
	ExecSQL (	MAIN_DB, AS400SRV_T4053_READ_DrugDoctorProf_old_data,
				&old_prcnt_gnrl,		&old_prcnt_krn,			&old_prcnt_mgn,
				&v_basic_fp_old,		&v_kesef_fp_old,		&v_zahav_fp_old,
				&o_in_health_basket,	&o_HealthBasketNew,		&o_percnt_y,
				&o_fixed_pr_yahalom,	&o_wait_months,			&v_sysdate,
				&v_systime,				&old_del_flg,
				&v_largo_code,			&v_prof,				&v_enabled_status,
				END_OF_ARG_LIST														);
		
	if ( SQLERR_code_cmp( SQLERR_not_found ) == MAC_TRUE )
	{
 		do_insert = ATTEMPT_INSERT_FIRST; 
		something_changed = 1;
	}
	else if (SQLERR_code_cmp(SQLERR_access_conflict) == MAC_TRUE)
	{
	   sleep( SLEEP_TIME_BETWEEN_RETRIES );
	   retcode = ALL_RETRIES_WERE_USED;
	}
	else if ( SQLERR_error_test() )
    {
	    retcode = SQL_ERROR_OCCURED;
	}
	else   // No errors on fetch
	{
		// DonR 11Jan2012: If this is an RKFILPRD update (with download_start_date = 0),
		// flag this row as containing new data even if nothing has really changed.
		something_changed =
			(	(download_start_date	== 0					)		||		// DonR 11Jan2012
				(old_prcnt_gnrl			!= v_prcnt_gnrl			)		||
				(old_prcnt_krn			!= v_prcnt_krn			)		||
				(old_prcnt_mgn			!= v_prcnt_mgn			)		||
				(o_percnt_y				!= v_percnt_y			)		||
				(v_basic_fp_old			!= v_basic_fixed_price	)		||
				(v_kesef_fp_old			!= v_kesef_fixed_price	)		||
				(v_zahav_fp_old			!= v_zahav_fixed_price	)		||
				(o_fixed_pr_yahalom		!= v_fixed_pr_yahalom	)		||
				(o_wait_months			!= v_wait_months	)		||
				(o_in_health_basket		!= v_in_health_basket	)		||
				(o_HealthBasketNew		!= v_HealthBasketNew	)		||
				(strcmp (old_del_flg,	   v_del_flg		   ))	);

		if (something_changed)
		{
			v_sysdate = sysdate ;
			v_systime = systime ;
		}
	}	

/* << -------------------------------------------------------- >> */
/*	   up to SQL_UPDATE_RETRIES retries will be made			  */
/*		    to overcome access conflict							  */
/* << -------------------------------------------------------- >> */
  retcode = ALL_RETRIES_WERE_USED;
  stop_retrying = false;

  for(n_retries = 0;
      ( n_retries < SQL_UPDATE_RETRIES ) && ( stop_retrying == false );
      n_retries++ )
    {
      /* sleep for a while:    */
      if( n_retries )
      {
          sleep( SLEEP_TIME_BETWEEN_RETRIES );
      }

      /*
       *  try to insert/update new rec with the order depending on "do_insert":
       */
      for( i = 0;  i < 2;  i++ )
	{
	  if( do_insert == ATTEMPT_INSERT_FIRST )
	    {
			ExecSQL (	MAIN_DB, AS400SRV_T4053_INS_DrugDoctorProf,
						&v_largo_code,				&v_prof,
						&v_enabled_status,			&v_prcnt_gnrl,
						&v_basic_fixed_price,		&v_prcnt_krn,
						&v_kesef_fixed_price,		&v_prcnt_mgn,
						&v_zahav_fixed_price,		&v_in_health_basket,
						&v_HealthBasketNew,			&v_percnt_y,
						&v_fixed_pr_yahalom,		&v_wait_months,
						&sysdate,					&systime,
						&v_as400_batch_date,		&v_as400_batch_time,
						&UPDATED_BY_AS400,			&v_del_flg,
						END_OF_ARG_LIST										);

         HANDLE_INSERT_SQL_CODE( "4053",p_connect, &do_insert,   &retcode,  &stop_retrying );
	    }
	    else
	    {
			ExecSQL (	MAIN_DB, AS400SRV_T4053_UPD_DrugDoctorProf,
						&v_prcnt_gnrl,				&v_prcnt_krn,
						&v_prcnt_mgn,				&v_basic_fixed_price,
						&v_kesef_fixed_price,		&v_zahav_fixed_price,
						&v_in_health_basket,		&v_HealthBasketNew,
						&v_percnt_y,				&v_fixed_pr_yahalom,
						&v_wait_months,				&v_sysdate,
						&v_systime,					&v_as400_batch_date,
						&v_as400_batch_time,		&UPDATED_BY_AS400,
						&v_del_flg,
						&v_largo_code,				&v_prof,
						&v_enabled_status,			END_OF_ARG_LIST			);

          HANDLE_UPDATE_SQL_CODE( "4053",p_connect, &do_insert,  &retcode,   &stop_retrying );
	    }

	}/* for( i... */

    }/* for( n_retries... */

  if( n_retries == SQL_UPDATE_RETRIES )
  {
     WRITE_ACCESS_CONFLICT_TO_LOG( "4053" );
  }

  if (retcode == MSG_HANDLER_SUCCEEDED)
  {
	if (do_insert == ATTEMPT_UPDATE_FIRST)
	{
		if (something_changed)
		{
			// Assume that any change to a deleted row is a deletion - probably more or less correct...
			if ((*v_del_flg >= 'A') && (*v_del_flg <= 'E'))
				rows_deleted++;
			else
				rows_updated++;
		}
		else
		{
			rows_refreshed++;
		}
	}
	else
	{
		// We shouldn't be inserting any deleted rows - so assume that all insertions are additions.
		rows_added++;
	}
  }


  return retcode;
} /* end of msg 2053/4053 handler */


// -------------------------------------------------------------------------
//
//       Post-processor for message 4053 - logically delete DRUGDOCTORPROF
//			rows that are no longer in the AS/400 full-table download.
//
// -------------------------------------------------------------------------
static int postprocess_4053 (TConnection *p_connect)
{
	// local variables
	bool	stop_retrying;	// if true - return from function
	int		rows_deleted	= 0;
	int		retcode;		// code returned from function
	int		n_retries;		// retrying counter
	int		db_changed_flg;

	// DB variables
	Tdel_flg_c		v_del_flg;
	int				v_as400_batch_date;
	int				v_as400_batch_time;
	int				v_update_date;
	int				v_update_time;


	// Initialize variables.
	v_as400_batch_date	= download_start_date;
	v_as400_batch_time	= download_start_time;


	retcode			= ALL_RETRIES_WERE_USED;
	stop_retrying	= false;


	DeclareCursorInto (	MAIN_DB, AS400SRV_T4053PP_stale_DrugDoctorProf_cur,
						&v_del_flg,				&v_as400_batch_date,
						&v_as400_batch_time,	END_OF_ARG_LIST				);

	OpenCursor (	MAIN_DB, AS400SRV_T4053PP_stale_DrugDoctorProf_cur	);

	if (SQLERR_error_test ())
	{
		retcode = SQL_ERROR_OCCURED;
	}

	while (1)
	{
		FetchCursor (	MAIN_DB, AS400SRV_T4053PP_stale_DrugDoctorProf_cur	);

		if (SQLERR_code_cmp (SQLERR_end_of_fetch) == MAC_TRUE)
		{
			retcode = MSG_HANDLER_SUCCEEDED;	// In case there is nothing that needs to be deleted.
			break;
		}
		else
		{
			if (SQLERR_error_test ())
			{
				retcode = SQL_ERROR_OCCURED;
				break;
			}
		}

		// Calculate virtual time. Note that for this download the time is based
		// on the real time, adjusted only to ensure that we don't have too many
		// rules with the exact same as-of time.
		v_update_date		= download_start_date;
		v_update_time		= download_start_time + (download_row_count++ / VIRTUAL_PORTION);

		// Adjust del_flg from numeric (= Tzahal status) to appropriate character (= deleted) value.
		*v_del_flg += ('A' - '1');

		ExecSQL (	MAIN_DB, AS400SRV_T4053PP_UPD_DrugDoctorProf_logically_delete,
					&v_del_flg,			&v_update_date,
					&v_update_time,		END_OF_ARG_LIST								);

		HANDLE_DELETE_SQL_CODE ("postprocess_4053", p_connect, &retcode, &db_changed_flg);

		rows_deleted++;
	}	// Cursor-scrolling loop.

	CloseCursor (	MAIN_DB, AS400SRV_T4053PP_stale_DrugDoctorProf_cur	);


	// DonR 13Nov2018 CR #15919: Record current table size.
	REMEMBER_ISOLATION;
	SET_ISOLATION_DIRTY;

	ExecSQL (	MAIN_DB, AS400SRV_T4053PP_READ_DrugDoctorProf_table_size,
				&total_rows, END_OF_ARG_LIST								);

	ExecSQL (	MAIN_DB, AS400SRV_T4053PP_READ_DrugDoctorProf_active_size,
				&active_rows, END_OF_ARG_LIST								);

	RESTORE_ISOLATION;
	// DonR 13Nov2018 CR #15919 end.


	GerrLogMini (GerrId,
				 "Postprocess 4053: %d rows logically deleted, retcode = %d.",
				 rows_deleted, retcode);

	return retcode;

}	// End of msg 4053 postprocessor.



/* << -------------------------------------------------------- >> */
/*                                                                */
/*       handler for message 2054/4054 - upd/ins to ECONOMYPRI    */
/*                                                                */
/* << -------------------------------------------------------- >> */
static int data_4054_handler (TConnection	*p_connect,
							  int			data_len)
{
	// local variables
	char	*PosInBuff;		/* current pos in data buffer	    */
	bool	stop_retrying;	/* if true - return from function   */
	bool	do_insert;
	bool	old_row_exists;
	bool	something_changed;
	int		ReadInput;		/* amount of Bytesread from buffer  */
	int		retcode;		/* code returned from function	    */
	int		n_retries;		/* retrying counter					*/
	int		i;

	// DB variables
	Tdrug_group			v_drug_group,		old_drug_group;
	Titem_seq			v_item_seq,			old_item_seq;
	int					v_drug_group_code;
	Tgroup_nbr			v_group_nbr;
	Tlargo_code			v_largo_code;
	Tdel_flg_c			v_del_flg,			old_del_flg;
	Tsystem_code		v_system_code,		old_system_code;	// = char[2]
	int					v_update_date;
	int					v_update_time;
	int					v_changed_date;
	int					v_changed_time;
	int					v_refresh_date;
	int					v_refresh_time;
	int					v_received_date;
	int					v_received_time;
	int					v_as400_batch_date;
	int					v_as400_batch_time;
	int					v_row_count;
	short				SystemCode;
	short				SendAndUsePharm;
	short				pharm_real_time;	// Marianna 09Feb2023 User Story #417219


	// Initialization.
	// Calculate virtual time. Note that for this download the time is based
	// on the real time, adjusted only to ensure that we don't have too many
	// rules with the exact same as-of time.
	v_refresh_date = GetDate();
	v_refresh_time = GetTime();
	v_as400_batch_date = (download_start_date > 0) ? download_start_date : v_refresh_date;
	v_as400_batch_time = (download_start_time > 0) ? download_start_time : v_refresh_time;


	// read input buffer
	INIT_ERROR_DETECTION_MECHANISM;
	PosInBuff = glbMsgBuffer;

	//GerrLogReturn(GerrId,"2054: inbuf = {%s}",PosInBuff);		   

	get_string (&PosInBuff, v_drug_group, Tdrug_group_len);

	v_drug_group_code	= get_long	(&PosInBuff, Tdrug_group_code_len	); RETURN_ON_ERROR;
	v_group_nbr			= get_short	(&PosInBuff, Tgroup_nbr_len			); RETURN_ON_ERROR;
	v_item_seq			= get_short	(&PosInBuff, Titem_seq_len			); RETURN_ON_ERROR;
	v_largo_code		= get_long	(&PosInBuff, 9						); RETURN_ON_ERROR;	//Marianna 14Jul2024 User Story #330877

	get_string (&PosInBuff, v_del_flg, Tdel_flg_c_len);
	get_string (&PosInBuff, v_system_code, Tdel_flg_c_len);

	pharm_real_time		= get_short	(&PosInBuff, 1						); RETURN_ON_ERROR;	// Marianna 12Feb2023 User Story #417219 


	/* verify all data was read:   */
	ReadInput = PosInBuff - glbMsgBuffer;
	StaticAssert( ReadInput == data_len );

//	// DonR 13Nov2017 TEMPORARY: Force all rows with Group Code > 999 to be deletions.
//	if (v_drug_group_code > 999)
//	{
//		strcpy (v_del_flg,		"D");
//		strcpy (v_system_code,	"1");
//	}

	// DonR 30Jan2018: Just for convenience, copy the System Code to a numeric variable.
	// DonR 31Jan2018: Add a new "send and use" flag column send_and_use_pharm to economypri.
	// This allows us to keep all the table-specific logic right here.
	SystemCode		= (short)v_system_code[0] - (short)'0';
	SendAndUsePharm	= ((SystemCode == 0) || (SystemCode == 1) || (SystemCode == 4)) ? 1 : 0;


	// Try to read existing row.
	ExecSQL (	MAIN_DB, AS400SRV_T4054_READ_economypri_old_data,
				&old_drug_group,		&old_item_seq,
				&old_system_code,		&old_del_flg,
				&v_update_date,			&v_update_time,
				&v_received_date,		&v_received_time,
				&v_changed_date,		&v_changed_time,
				&v_drug_group_code,		&v_group_nbr,
				&v_largo_code,			END_OF_ARG_LIST				);

	// If old row not found, set variables accordingly.
	if (SQLERR_code_cmp (SQLERR_not_found) == MAC_TRUE)	
	{
		strcpy (old_drug_group,		"");
		strcpy (old_system_code,	"");
		strcpy (old_del_flg,		"");
		old_item_seq	= 0;

		old_row_exists		= 0;	// Triggers insertion rather than update.
		something_changed	= 1;	// Forces all dates to be set.
	}
	else
	{
		if (SQLERR_code_cmp (SQLERR_access_conflict) == MAC_TRUE)
		{
			sleep (SLEEP_TIME_BETWEEN_RETRIES);
			retcode = ALL_RETRIES_WERE_USED;
		}
		else
		{
			if (SQLERR_error_test ())
			{
				retcode = SQL_ERROR_OCCURED;
			}
			else
			{
				// Successful read - see if the new data is different from the old.
				old_row_exists	= 1;

				something_changed = ((strcmp (v_drug_group	,	old_drug_group	))	||
									 (strcmp (v_system_code	,	old_system_code	))	||
									 (strcmp (v_del_flg		,	old_del_flg		))	||
									 (		  v_item_seq	!=	old_item_seq	)	||
									 (pharm_real_time > 0						));	//	Marianna 12Feb2023 User Story #417219
			}
		}	// Not an access conflict.
	}	// Not a not-found error.


	// If this is Transaction 2054 (intra-day update), download_start_date will be zero.
	// For intra-day updates, we always want to refresh information at pharmacies and
	// doctors' systems - so set update_date/time even if no "real" data has changed.
	if (something_changed)
	{
		v_update_date = v_changed_date = v_received_date = v_refresh_date;
		v_update_time = v_changed_time = v_received_time = v_refresh_time;
	}
	else
	{
		// Row already exists and no data has changed; but if this is an intra-day update,
		// send it to doctors and pharmacies anyway.
		if (download_start_date == 0)
		{
			v_update_date = v_received_date = v_refresh_date;
			v_update_time = v_received_time = v_refresh_time;
		}
	}

	// After setting all our other dates and times, alter received_time for sorting purposes -
	// IF we actually have new data to send.
	// DonR 02Jan2012: download_row_count needs to be incremented even if no data has changed - 
	// otherwise we write zero to received_seq and stuff is received by pharmacies in the wrong
	// order.
	download_row_count++;
	if ((v_received_date == v_refresh_date) && (v_received_time == v_refresh_time))
	{
		v_received_time += (download_row_count / VIRTUAL_PORTION);
	}

	v_row_count = download_row_count;


	/* << -------------------------------------------------------- >> */
	/*	   up to SQL_UPDATE_RETRIES retries will be made			  */
	/*		    to overcome access conflict							  */
	/* << -------------------------------------------------------- >> */
	retcode = ALL_RETRIES_WERE_USED;
	stop_retrying = false;

	for (n_retries = 0;
		 (n_retries < SQL_UPDATE_RETRIES) && (stop_retrying == false);
		 n_retries++)
	{
		// sleep for a while
		if (n_retries)
		{
			sleep (SLEEP_TIME_BETWEEN_RETRIES);
		}


		// Update or insert, depending on whether we were able to read the row.
		// DonR 04Jul2010: HOT FIX: Need to have dummy "for" loop here so that HANDLE_UPDATE_SQL_CODE
		// and HANDLE_INSERT_SQL_CODE work properly.
		for (i = 0; i < 1; i++)
		{
			if (old_row_exists)
			{
				ExecSQL (	MAIN_DB, AS400SRV_T4054_UPD_economypri_new_data,
							&v_drug_group,			&v_item_seq,
							&SendAndUsePharm,		&v_del_flg,
							&v_system_code,			&v_update_date,
							&v_update_time,			&UPDATED_BY_AS400,
							&v_received_date,		&v_received_time,
							&v_refresh_date,		&v_refresh_time,
							&v_changed_date,		&v_changed_time,
							&v_as400_batch_date,	&v_as400_batch_time,
							&v_row_count,			&v_drug_group_code,
							&v_group_nbr,			&v_largo_code,
							END_OF_ARG_LIST										);

				HANDLE_UPDATE_SQL_CODE ("4054", p_connect, &do_insert, &retcode, &stop_retrying);
			}
			else
			{
				ExecSQL (	MAIN_DB, AS400SRV_T4054_INS_economypri,
							&v_drug_group_code,		&v_group_nbr,			&v_largo_code,
							&v_drug_group,			&v_item_seq,			&v_del_flg,
							&v_system_code,			&SendAndUsePharm,		&v_update_date,
							&v_update_time,			&UPDATED_BY_AS400,		&v_received_date,
												
							&v_received_time,		&v_refresh_date,		&v_refresh_time,
							&v_changed_date,		&v_changed_time,		&v_as400_batch_date,
							&v_as400_batch_time,	&v_row_count,			END_OF_ARG_LIST			);

				HANDLE_INSERT_SQL_CODE ("4054", p_connect, &do_insert, &retcode, &stop_retrying);
			}
		}	// End of dummy "for" loop.



		// If the current action was the insertion/update of a non-deleted row, make sure there
		// are no old non-deleted rows in economypri with different Group Code or Group Number.
		// This is to avoid duplicate-select errors. Also, if we're deleting the last economypri
		// row (with Group Code < 600) for a drug, set its economypri_group to zero.
		// DonR 18Mar2010: Since economypri groups of 600 and up are irrelevant, make this whole
		// block of code conditional on that.
		// DonR 21Dec2011: To make more room for "real" generic-substitution groups, the boundary
		// for "therapoietic" groups is being moved from 600 to 800.
		// DonR 05Nov2014: Changing the 799/800 boundary to 999/1000. This is temporary, as the
		// code is being expanded to six digits. When the next phase comes in, the boundary will
		// be at 19999/20000.
		// DonR 30Jan2018: Use System Code instead of a range of Group Codes to decide what's relevant.
		// DonR 31Jan2018: Add a new "send and use" flag column send_and_use_pharm to economypri.
		// This tells us (among other things) whether the row is relevant to updating drug_list.
		if (SendAndUsePharm)
		{
			if ((v_del_flg[0] != 'd') && (v_del_flg[0] != 'D'))
			{
//				// DonR 21Mar2010: I checked with Eyal Hamber and also against the economypri_problems
//				// logs on all servers, and it appears that we're no longer getting duplicate "real"
//				// economypri rows from AS/400; so the logic to prevent duplicate "live" rows should
//				// really be unnecessary. I'll leave it in for now, just to be paranoid.
				// DonR 10Jun2012: Because the 4054 Postprocessor takes care of duplicate rows present after
				// the full-table download, we need to take special steps here only for incremental downloads.
				// Also, we don't want to perform a physical deletion, but only a logical one - since otherwise
				// we leave undeleted duplicates in the Clicks database!
				if (download_start_date <= 0)	// I.e. we're in incremental mode.
				{
					ExecSQL (	MAIN_DB, AS400SRV_T4054_UPD_economypri_logically_delete_old_version,
								&v_update_date,			&v_update_time,			&v_changed_date,
								&v_changed_time,		&v_received_date,		&v_received_time,
								&v_largo_code,			&v_drug_group_code,		&v_group_nbr,
								END_OF_ARG_LIST															);

					if ((SQLCODE == 0) && (sqlca.sqlerrd[2] > 0))
					{
						GerrLogFnameMini ("economypri_problems", GerrId,
										  "Logically deleted old (duplicate) entry for Largo %d!", (int)v_largo_code);
					}
				}	// Incremental mode - so we need to process deletions as they come from AS/400.

				// DonR 21Mar2010 end - the UPDATE below is NOT redundant!!!

				// DonR 04Jan2005: Update drug_list based on economypri data received. (Remember
				// that we're inside an "if current row is not a deletion" block!)
				// DonR 04Feb2018: Also copy item_seq from economypri to drug_list.
				ExecSQL (	MAIN_DB, AS400SRV_T4054_UPD_drug_list_set_economypri_columns,
							&v_drug_group_code,		&v_item_seq,
							&v_largo_code,			END_OF_ARG_LIST							);
			}	// Row received has Deletion Flag set to zero.
			else
			{	// Deleting a row. This may or may not be the last "live" economypri row for this drug.
				// DonR 04Jan2005: Update drug_list based on economypri data received.
				// DonR 21Mar2010: Add select parameters (A) to include small-'d' deletions (which I
				// don't think really exist, but just in case; and (B) to exclude drugs that already
				// have their Economypri Group set to zero and thus don't need to be updated.
				// DonR 11Apr2011: While we're at it, zero out the new preferred_largo field as well.
				// DonR 31Jan2018: Use new send_and_use_pharm flag instead of a range of Group Codes or
				// the System Code. This keeps the number of references to specific System Codes to a
				// bare minimum.
				// DonR 03Jan2022: Switch to a new version of the UPDATE query that leaves
				// preferred_largo alone. This is necessary because sometimes we get deletions
				// before updates - and AS400SRV_T4054_UPD_drug_list_set_economypri_columns
				// does not set preferred_largo.
// GerrLogMini (GerrId, "%d got logical deletion from AS/400 for Group %d Largo %d.", (download_start_date <= 0) ? 2054 : 4054, v_drug_group_code, v_largo_code);
				ExecSQL (	MAIN_DB, AS400SRV_T4054_UPD_drug_list_clear_2_economypri_columns,
							&v_largo_code, &v_largo_code, END_OF_ARG_LIST					);
			}	// New data is a deletion.

		}	// SendAndUsePharm set TRUE.

	}	// for( n_retries...


	// Update counters.
	if (old_row_exists)
	{
		if (something_changed)
		{
			if ((v_del_flg[0] == 'd') || (v_del_flg[0] == 'D'))
				rows_deleted++;
			else
				rows_updated++;

		}
		else
		{
			rows_refreshed++;
		}
	}
	else
	{
		// We shouldn't be inserting any deleted rows - so assume that all insertions are additions.
		rows_added++;
	}


	if (n_retries == SQL_UPDATE_RETRIES)
	{
		WRITE_ACCESS_CONFLICT_TO_LOG ("2054/4054");
	}


	return retcode;

}	// end of msg 2054/4054 handler


// -------------------------------------------------------------------------
//
//       Post-processor for message 4054 - logically delete ECONOMYPRI
//			rows that are no longer in the AS/400 full-table download.
//
// -------------------------------------------------------------------------
static int postprocess_4054 (TConnection *p_connect)
{
	// local variables
	bool	stop_retrying;	// if true - return from function
	int		rows_deleted	= 0;
	int		retcode;		// code returned from function
	int		n_retries;		// retrying counter
	int		db_changed_flg;

	// DB variables
	Tlargo_code		v_largo_code;
	char			v_system_code[2];
	int				v_drug_group_code;
	int				v_as400_batch_date;
	int				v_as400_batch_time;
	int				v_update_date;
	int				v_update_time;
	int				v_received_time;
	int				v_row_count;
	int				db_as400_batch_date;	// DonR 31Jul2022 for diagnostics.
	int				db_as400_batch_time;	// DonR 31Jul2022 for diagnostics.
	short			SystemCode;
	short			SendAndUsePharm;


	// Initialize variables.
	v_as400_batch_date	= download_start_date;
	v_as400_batch_time	= download_start_time;


	retcode			= ALL_RETRIES_WERE_USED;
	stop_retrying	= false;


	DeclareCursorInto (	MAIN_DB, AS400SRV_T4054PP_stale_economypri_cur,
						&v_largo_code,			&v_drug_group_code,
						&v_system_code,			&db_as400_batch_date,
						&db_as400_batch_time,

						&v_as400_batch_date,	&v_as400_batch_time,
						END_OF_ARG_LIST									);

	OpenCursor (	MAIN_DB, AS400SRV_T4054PP_stale_economypri_cur	);

	if (SQLERR_error_test ())
	{
		retcode = SQL_ERROR_OCCURED;
	}

	while (1)
	{
		FetchCursor (	MAIN_DB, AS400SRV_T4054PP_stale_economypri_cur	);

		if (SQLERR_code_cmp (SQLERR_end_of_fetch) == MAC_TRUE)
		{
			retcode = MSG_HANDLER_SUCCEEDED;	// In case there is nothing that needs to be deleted.
			break;
		}
		else
		{
			if (SQLERR_error_test ())
			{
				retcode = SQL_ERROR_OCCURED;
				break;
			}
		}

		// Calculate virtual time. Note that for this download the time is based
		// on the real time, adjusted only to ensure that we don't have too many
		// rules with the exact same as-of time.
// GerrLogMini (GerrId, "4054 postprocessor: download_start_date = %d, download_start_time = %d, DB has %d/%d for Group %d Largo %d.",
//	download_start_date, download_start_time, db_as400_batch_date, db_as400_batch_time, v_drug_group_code, v_largo_code);
		v_update_date		= download_start_date;
		v_update_time		= download_start_time;
		v_received_time		= download_start_time + (download_row_count++ / VIRTUAL_PORTION);
		v_row_count			= download_row_count;
		SystemCode			= (short)v_system_code[0] - (short)'0';	// Copy to numeric variable, just for convenience.
		SendAndUsePharm		= ((SystemCode == 0) || (SystemCode == 1) || (SystemCode == 4)) ? 1 : 0;

		ExecSQL (	MAIN_DB, AS400SRV_T4054PP_UPD_economypri_logically_delete,
					&v_update_date,			&v_update_time,
					&v_update_date,			&v_received_time,
					&v_row_count,			END_OF_ARG_LIST						);

		HANDLE_DELETE_SQL_CODE ("postprocess_4054", p_connect, &retcode, &db_changed_flg);

		rows_deleted++;

		// If there is no longer a "live" economypri entry for this drug AND the drug still "thinks"
		// that it has an entry, set the drug's Economypri Group value to zero. Note that we don't
		// bother checking if the current economypri row's Group Code is >= 600; these rows are
		// irrelevant to drug_list.
		// DonR 21Dec2011: To make more room for "real" generic-substitution groups, the boundary
		// for "therapoietic" groups is being moved from 600 to 800.
		// DonR 05Nov2014: Changing the 799/800 boundary to 999/1000. This is temporary, as the
		// code is being expanded to six digits. When the next phase comes in, the boundary will
		// be at 19999/20000.
		// DonR 30Jan2018: Use System Code instead of a range of Group Codes.
		// DonR 31Jan2018: Add a new "send and use" flag column send_and_use_pharm to economypri.
		// This tells us (among other things) whether the row is relevant to updating drug_list.
		if (SendAndUsePharm)
		{
			ExecSQL (	MAIN_DB, AS400SRV_T4054_UPD_drug_list_clear_3_economypri_columns,
						&v_largo_code, &v_largo_code, END_OF_ARG_LIST					);
		}	// The economypri row being logically deleted is relevant to the pharmacy system.
	}	// Cursor-scrolling loop.

	CloseCursor (	MAIN_DB, AS400SRV_T4054PP_stale_economypri_cur	);


	// DonR 13Nov2018 CR #15919: Record current table size.
	REMEMBER_ISOLATION;
	SET_ISOLATION_DIRTY;

	ExecSQL (	MAIN_DB, AS400SRV_T4054PP_READ_economypri_table_size,
				&total_rows, END_OF_ARG_LIST							);

	ExecSQL (	MAIN_DB, AS400SRV_T4054PP_READ_economypri_active_size,
				&active_rows, END_OF_ARG_LIST							);

	RESTORE_ISOLATION;
	// DonR 13Nov2018 CR #15919 end.


	GerrLogMini (GerrId,
				 "Postprocess 4054: %d rows logically deleted, retcode = %d.",
				 rows_deleted, retcode);

	return retcode;

}	// End of msg 4054 postprocessor.



/* << -------------------------------------------------------- >> */
/*                                                                */
/*       handler for message 2055 - upd/ins to PHARMACOLOGY       */
/* << -------------------------------------------------------- >> */
static int message_2055_handler(
				TConnection	*p_connect,
				int		data_len,
				bool	do_insert,
				TShmUpdate	*shm_table_to_update
				)
{
/* << -------------------------------------------------------- >> */
/*			  local variables			  */
/* << -------------------------------------------------------- >> */
  char      *PosInBuff;		/* current pos in data buffer	    */
  int		ReadInput;		/* amount of Bytesread from buffer  */
  int		retcode;		/* code returned from function	    */
  bool		stop_retrying;	/* if true - return from function   */
  int		n_retries;		/* retrying counter		    */
  int		i;

	/* << -------------------------------------------------------- >> */
	/*			    DB variables			  */
	/* << -------------------------------------------------------- >> */
	Tpharm_group_code	v_pharm_group_code;
	Tpharm_group_name	v_pharm_group_name;
	Tdel_flg_c			v_del_flg;
	int					sysdate,	systime;
	Tpharm_group_name	old_pharm_group_name;
	Tdel_flg_c			old_del_flg;
	int					v_sysdate,	v_systime;


	GetVirtualTimestamp (&sysdate, &systime);

/* << -------------------------------------------------------- >> */
/*			read input buffer			  */
/* << -------------------------------------------------------- >> */
  INIT_ERROR_DETECTION_MECHANISM;
  PosInBuff = glbMsgBuffer;

  v_pharm_group_code		=  get_short(  &PosInBuff,    Tpharm_group_code_len  );  RETURN_ON_ERROR;
  get_string(   &PosInBuff, v_pharm_group_name,Tpharm_group_name_len    );  switch_to_win_heb( ( unsigned char * ) v_pharm_group_name );
  get_string(   &PosInBuff, v_del_flg,Tdel_flg_c_len    );

/* << -------------------------------------------------------- >> */
/*			    debug messages			  */
/* << -------------------------------------------------------- >> */

  /* verify all data was read:   */
  ReadInput = PosInBuff - glbMsgBuffer;
  StaticAssert( ReadInput == data_len );

  strip_trailing_spaces (v_pharm_group_name);	// DonR 09Feb2020 - since ODBC strips trailing spaces when SELECTing VARCHAR columns.

  // check difference in date 20020307
	ExecSQL (	MAIN_DB, AS400SRV_T2055_READ_pharmacology_old_data,
				&old_pharm_group_name,		&v_sysdate,
				&v_systime,					&old_del_flg,
				&v_pharm_group_code,		END_OF_ARG_LIST			);
		
	if ( SQLERR_code_cmp( SQLERR_not_found ) == MAC_TRUE )
	{
		*shm_table_to_update = PHARMACOLOGY;
		do_insert = ATTEMPT_INSERT_FIRST; 
	}
	else if (SQLERR_code_cmp(SQLERR_access_conflict) == MAC_TRUE)
	{
	   sleep( SLEEP_TIME_BETWEEN_RETRIES );
	   retcode = ALL_RETRIES_WERE_USED;
	}
	else if ( SQLERR_error_test() )
    {
	    retcode = SQL_ERROR_OCCURED;
	}
	else   // No errors on fetch
	{
		do
		{
			if ( strcmp(old_pharm_group_name ,v_pharm_group_name))
			{
				GerrLogFnameMini("2055",GerrId,"1:[%s][%s]",old_pharm_group_name,v_pharm_group_name);
				*shm_table_to_update = PHARMACOLOGY;
				v_sysdate = sysdate ;
				v_systime = systime ;
				break;
			}
			if ( strcmp (old_del_flg , v_del_flg))
			{
				GerrLogFnameMini("2055",GerrId,"2:[%d]",v_pharm_group_code);
				*shm_table_to_update = PHARMACOLOGY;
				v_sysdate = sysdate ;
				v_systime = systime ;
				break;
			}
		}
		while (0);
	}	

/* << -------------------------------------------------------- >> */
/*	   up to SQL_UPDATE_RETRIES retries will be made			  */
/*		    to overcome access conflict							  */
/* << -------------------------------------------------------- >> */
  retcode = ALL_RETRIES_WERE_USED;
  stop_retrying = false;

  for(n_retries = 0;
      ( n_retries < SQL_UPDATE_RETRIES ) && ( stop_retrying == false );
      n_retries++ )
    {
      /* sleep for a while:    */
      if( n_retries )
      {
          sleep( SLEEP_TIME_BETWEEN_RETRIES );
      }

      /*
       *  try to insert/update new rec with the order depending on "do_insert":
       */
      for( i = 0;  i < 2;  i++ )
	{
	  if( do_insert == ATTEMPT_INSERT_FIRST )
	    {
			ExecSQL (	MAIN_DB, AS400SRV_T2055_INS_pharmacology,
						&v_pharm_group_code,	&v_pharm_group_name,
						&sysdate,				&systime,
						&UPDATED_BY_AS400,		&v_del_flg,
						END_OF_ARG_LIST								);

         HANDLE_INSERT_SQL_CODE( "2055",p_connect, &do_insert,   &retcode,  &stop_retrying );

	    }
	  else
	    {
			ExecSQL (	MAIN_DB, AS400SRV_T2055_UPD_pharmacology,
						&v_pharm_group_name,	&v_sysdate,
						&v_systime,				&UPDATED_BY_AS400,
						&v_del_flg,				&v_pharm_group_code,
						END_OF_ARG_LIST									);

          HANDLE_UPDATE_SQL_CODE( "2055",p_connect, &do_insert,  &retcode,   &stop_retrying );
	    }

	}/* for( i... */

    }/* for( n_retries... */

  if( n_retries == SQL_UPDATE_RETRIES )
  {
     WRITE_ACCESS_CONFLICT_TO_LOG( "2055" );
  }

  if( do_insert == ATTEMPT_INSERT_FIRST && retcode == MSG_HANDLER_SUCCEEDED )
  {
//      *shm_table_to_update = PHARMACOLOGY;
  }

  if (retcode == MSG_HANDLER_SUCCEEDED)
  {
	  if (do_insert == ATTEMPT_INSERT_FIRST)
		  rows_added++;
	  else
		  rows_updated++;
  }


  return retcode;
} /* end of msg 2055 handler */


/*20010128 end new transaction 2055*/



/* << -------------------------------------------------------------------- >> */
/*																			  */
/*     handler for message 2061 - delete ALL rows from PHARMACY CONTRACT	  */
/*																			  */
/*     Added 21Oct2003 by Don Radlauer.										  */
/*							                                                  */
/* << -------------------------------------------------------------------- >> */
static int message_2061_handler(
				TConnection	*p_connect,
				int		data_len
				)
{

/* << -------------------------------------------------------- >> */
/*			  local variables			  */
/* << -------------------------------------------------------- >> */
  int		retcode;		/* code returned from function	    */
  int		n_retries;		/* retrying counter		    */
  int       db_changed_flg;



/* << -------------------------------------------------------- >> */
/*			   attempt delete			  */
/* << -------------------------------------------------------- >> */
  retcode = ALL_RETRIES_WERE_USED;
  db_changed_flg = 0;

  for( n_retries = 0;
       n_retries < SQL_UPDATE_RETRIES;
       n_retries++ )
    {

      /*
       * sleep for a while:
       */
      if( n_retries )
      {
          sleep( SLEEP_TIME_BETWEEN_RETRIES );
      }

		ExecSQL (	MAIN_DB, AS400SRV_T2061_DEL_pharmacy_contract_delete_all,
					&ROW_NOT_DELETED, END_OF_ARG_LIST							);

      HANDLE_DELETE_SQL_CODE( "2061", p_connect, &retcode, &db_changed_flg );

      break;
    }

  if( n_retries == SQL_UPDATE_RETRIES )
  {
     WRITE_ACCESS_CONFLICT_TO_LOG( "2061" );
  }

  if (retcode == MSG_HANDLER_SUCCEEDED)
	  rows_deleted++;

  return retcode;
} /* end of msg 2061 handler */



/* << -------------------------------------------------------- >> */
/*                                                                */
/*     handler for message 2062 - add to table PHARMACY CONTRACT  */
/*                                                                */
/*     Added 21Oct2003 by Don Radlauer.							  */
/*							                                      */
/* << -------------------------------------------------------- >> */
static int message_2062_handler (TConnection	*p_connect,
								 int			data_len)
{
	// local variables
	char		*PosInBuff;		/* current pos in data buffer	    */
	bool		stop_retrying;	/* if true - return from function   */
	bool		do_insert;
	int			ReadInput;		/* amount of Bytesread from buffer  */
	int			retcode;		/* code returned from function	    */
	int			n_retries;		/* retrying counter		    */
	int			i;


	// DB variables
	int				sysdate;
	int				systime;
	int				contract_code;
	int				contract_start;
	int				contract_end;
	int				fps_group_code;	// DonR 03Nov2009: Changed from short to long.
	Tfee			contract_min_amt;
	Tfee			contract_max_amt;
	Tfee			contract_fee;
	short int		contract_status;
	char			fps_group_name [15 + 1];


	// DonR 12Dec2011: This table is not a real-time one; so there was actually a discrepancy between
	// the update date/time written to the table and the update date/time written to tables_update.
	// The fix is to call GetVirtualTimestamp instead of GetDate/GetTime.
//	sysdate = GetDate();
//	systime = GetTime();
	GetVirtualTimestamp (&sysdate, &systime);


	// read input buffer
	INIT_ERROR_DETECTION_MECHANISM;
	PosInBuff = glbMsgBuffer;


	contract_code		= get_long	(&PosInBuff,  5);	RETURN_ON_ERROR;
	contract_start		= get_long	(&PosInBuff,  8);	RETURN_ON_ERROR;
	contract_end		= get_long	(&PosInBuff,  8);	RETURN_ON_ERROR;
	contract_min_amt	= get_long	(&PosInBuff, 11);	RETURN_ON_ERROR;
	contract_max_amt	= get_long	(&PosInBuff, 11);	RETURN_ON_ERROR;
	contract_fee		= get_long	(&PosInBuff, 11);	RETURN_ON_ERROR;
	fps_group_code		= get_long	(&PosInBuff,  5);	RETURN_ON_ERROR;
	GET_HEB_STR			(fps_group_name		   , 15);

	// verify all data was read
	ReadInput = PosInBuff - glbMsgBuffer;
	StaticAssert( ReadInput == data_len );

	// contract_status is always zero, at least for now.
	contract_status = 0;


	// up to SQL_UPDATE_RETRIES retries will be made to overcome access conflict
	retcode = ALL_RETRIES_WERE_USED;
	stop_retrying = false;

	for (n_retries = 0; (n_retries < SQL_UPDATE_RETRIES) && (stop_retrying == false); n_retries++)
	{
		// sleep for a while
		if( n_retries )
		{
			sleep (SLEEP_TIME_BETWEEN_RETRIES);
		}

		// try to insert new row
		// DonR 04Jul2010: HOT FIX: Need to have dummy "for" loop here so that HANDLE_UPDATE_SQL_CODE
		// and HANDLE_INSERT_SQL_CODE work properly.
		for (i = 0; i < 1; i++)
		{
			// DonR 23Jul2012: Just in case AS/400 sends us duplicate information, do a delete
			// (without error-checking but with a diagnostic).
			ExecSQL (	MAIN_DB, AS400SRV_T2062_DEL_pharmacy_contract_duplicate_rows,
						&contract_code,			&contract_min_amt,
						&contract_start,		&contract_end,
						&fps_group_code,		END_OF_ARG_LIST							);

//			if (SQLCODE == 0)
//				GerrLogMini (GerrId, "Received duplicate Pharmacy Contract info for %d/%d/%d/%d/%d.",
//							 contract_code,
//							 contract_min_amt,
//							 contract_start,
//							 contract_end,
//							 fps_group_code);
//
			ExecSQL (	MAIN_DB, AS400SRV_T2062_INS_pharmacy_contract,
						&contract_code,			&contract_start,
						&contract_end,			&contract_min_amt,
						&contract_max_amt,		&contract_fee,
						&contract_status,		&fps_group_code,
						&fps_group_name,		&sysdate,
						&systime,				&ROW_NOT_DELETED,
						END_OF_ARG_LIST									);

//				GerrLogMini (GerrId, "Received Pharmacy Contract info for %d/%d/%d/%d/%d, SQLCODE = %d.",
//							 contract_code,
//							 contract_min_amt,
//							 contract_start,
//							 contract_end,
//							 fps_group_code,
//							 SQLCODE);
				stop_retrying = 1;
				retcode = MSG_HANDLER_SUCCEEDED;
				break;
//			HANDLE_INSERT_SQL_CODE ("2062", p_connect, &do_insert, &retcode, &stop_retrying);

		}	// Dummy loop.

	}/* for( n_retries... */

	if( n_retries == SQL_UPDATE_RETRIES )
	{
		WRITE_ACCESS_CONFLICT_TO_LOG( "2062" );
	}


	if (retcode == MSG_HANDLER_SUCCEEDED)
	{
		rows_added++;
	}


	return retcode;
} /* end of msg 2062 handler */




/* << -------------------------------------------------------------------- >> */
/*																			  */
/*     handler for message 2063 - delete ALL rows from PRESCRIPTION SOURCE	  */
/*																			  */
/*     Added 12Nov2003 by Don Radlauer.										  */
/*							                                                  */
/* << -------------------------------------------------------------------- >> */
static int message_2063_handler(
				TConnection	*p_connect,
				int		data_len
				)
{

/* << -------------------------------------------------------- >> */
/*			  local variables			  */
/* << -------------------------------------------------------- >> */
  int		retcode;		/* code returned from function	    */
  int		n_retries;		/* retrying counter		    */
  int       db_changed_flg;

	short	pr_src_code;
	short	allowed_by_default;
	short	id_type_accepted;


/* << -------------------------------------------------------- >> */
/*			   attempt delete			  */
/* << -------------------------------------------------------- >> */
  retcode = ALL_RETRIES_WERE_USED;
  db_changed_flg = 0;

  for( n_retries = 0;
       n_retries < SQL_UPDATE_RETRIES;
       n_retries++ )
    {

      /*
       * sleep for a while:
       */
      if( n_retries )
      {
          sleep( SLEEP_TIME_BETWEEN_RETRIES );
      }

	  // DonR 13Mar2018 HOT FIX: Before deleting everything from prescr_source, read all the
	  // allowed_by_default values into a static global array. This will allow us to replace
	  // the table without losing all the allowed_by_default values, since they are not
	  // supplied by AS/400.
	memset ((char *)AllowedByDefault,	0, sizeof(AllowedByDefault));
	memset ((char *)IdTypeAccepted,		0, sizeof(IdTypeAccepted));

	DeclareCursorInto (	MAIN_DB, AS400SRV_T2063_prescr_source_allowed_by_default_cur,
						&pr_src_code,		&allowed_by_default,
						&id_type_accepted,	END_OF_ARG_LIST								);

	OpenCursor (	MAIN_DB, AS400SRV_T2063_prescr_source_allowed_by_default_cur	);

	do
	{
		FetchCursor (	MAIN_DB, AS400SRV_T2063_prescr_source_allowed_by_default_cur	);

		// Add one to the "allowed by default" and "ID type accepted" values, so that a zero
		// indicates no information and will revert to the database default value when we rebuild the table.
		if (SQLCODE == 0)
		{
			AllowedByDefault	[pr_src_code] = 1 + allowed_by_default;
			IdTypeAccepted		[pr_src_code] = 1 + id_type_accepted;
		}

	}	while (SQLCODE == 0);

	CloseCursor (	MAIN_DB, AS400SRV_T2063_prescr_source_allowed_by_default_cur	);


		ExecSQL (	MAIN_DB, AS400SRV_T2063_DEL_prescr_source	);

      HANDLE_DELETE_SQL_CODE( "2063", p_connect, &retcode, &db_changed_flg );

      break;
    }

  if( n_retries == SQL_UPDATE_RETRIES )
  {
     WRITE_ACCESS_CONFLICT_TO_LOG( "2063" );
  }

  if (retcode == MSG_HANDLER_SUCCEEDED)
	  rows_deleted++;

  return retcode;
} /* end of msg 2063 handler */



/* << ---------------------------------------------------------- >> */
/*																	*/
/*     handler for message 2064 - add to table PRESCRIPTION SOURCE	*/
/*																	*/
/*     Added 21Oct2003 by Don Radlauer.								*/
/*																	*/
/* << ---------------------------------------------------------- >> */
static int message_2064_handler (TConnection	*p_connect,
								 int			data_len)
{
	// local variables
	char		*PosInBuff;		/* current pos in data buffer	    */
	bool		stop_retrying;	/* if true - return from function   */
	bool		do_insert;
	int			ReadInput;		/* amount of Bytesread from buffer  */
	int			retcode;		/* code returned from function	    */
	int			n_retries;		/* retrying counter		    */
	int			i;

	// DB variables
	int		sysdate;
	int		systime;
	short	v_pr_src_code;
	char	v_pr_src_desc[26];
	short	v_pr_src_docid_type;
	short	v_pr_src_doc_device;
	short	v_pr_src_priv_pharm;
	short	allowed_by_default;
	short	id_type_accepted;


	sysdate = GetDate();
	systime = GetTime();


	// read input buffer
	INIT_ERROR_DETECTION_MECHANISM;
	PosInBuff = glbMsgBuffer;


	v_pr_src_code		= get_short		(&PosInBuff, 2);	RETURN_ON_ERROR;
	get_string	(&PosInBuff, v_pr_src_desc, 15);
	v_pr_src_docid_type	= get_short		(&PosInBuff, 1);	RETURN_ON_ERROR;
	v_pr_src_doc_device	= get_short		(&PosInBuff, 1);	RETURN_ON_ERROR;
	v_pr_src_priv_pharm	= get_short		(&PosInBuff, 1);	RETURN_ON_ERROR;

	// DonR 24Nov2003: Oops! Forgot to put the Prescription Source Description in correct order
	// so it won't come out backwards.
	switch_to_win_heb ((unsigned char *)v_pr_src_desc);


	// verify all data was read
	ReadInput = PosInBuff - glbMsgBuffer;
	StaticAssert( ReadInput == data_len );

	// Take "Allowed by default" value from the global array AllowedByDefault, which was
	// loaded in the "delete all" transaction 2063. Note that when we read values in from the
	// table, we added one to them - so a zero in the array means that we didn't see anything
	// for this prescription source, and we default to 1 = permitted. Otherwise 1 decrements
	// to zero (= not permitted) and 2 decrements to 1 (= permitted).
	// DonR 31Dec2018: Add similar handling for ID Type Accepted. In this case we'll default
	// to the same value we get in v_pr_src_docid_type, since normally the values are in fact
	// the same.
	allowed_by_default	= (AllowedByDefault	[v_pr_src_code] > 0) ? AllowedByDefault	[v_pr_src_code] - 1 : 1;
	id_type_accepted	= (IdTypeAccepted	[v_pr_src_code] > 0) ? IdTypeAccepted	[v_pr_src_code] - 1 : v_pr_src_docid_type;


	// up to SQL_UPDATE_RETRIES retries will be made to overcome access conflict
	retcode = ALL_RETRIES_WERE_USED;
	stop_retrying = false;

	for (n_retries = 0; (n_retries < SQL_UPDATE_RETRIES) && (stop_retrying == false); n_retries++)
	{
		// sleep for a while
		if( n_retries )
		{
			sleep (SLEEP_TIME_BETWEEN_RETRIES);
		}

		// try to insert new row
		// DonR 04Jul2010: HOT FIX: Need to have dummy "for" loop here so that HANDLE_UPDATE_SQL_CODE
		// and HANDLE_INSERT_SQL_CODE work properly.
		for (i = 0; i < 1; i++)
		{
			ExecSQL (	MAIN_DB, AS400SRV_T2064_INS_prescr_source,
						&v_pr_src_code,				&v_pr_src_desc,
						&v_pr_src_docid_type,		&id_type_accepted,
						&v_pr_src_doc_device,		&v_pr_src_priv_pharm,
						&allowed_by_default,		END_OF_ARG_LIST			);

			HANDLE_INSERT_SQL_CODE ("2064", p_connect, &do_insert, &retcode, &stop_retrying);

		}	// Dummy loop.

	}/* for( n_retries... */

	if( n_retries == SQL_UPDATE_RETRIES )
	{
		WRITE_ACCESS_CONFLICT_TO_LOG( "2064" );
	}


	if (retcode == MSG_HANDLER_SUCCEEDED)
	{
		rows_added++;
	}


	return retcode;
} /* end of msg 2064 handler */




/* << -------------------------------------------------------------------- >> */
/*																			  */
/*     handler for message 2065 - delete ALL rows from AUTHORIZING AUTHORITY  */
/*																			  */
/*     Added 12Nov2003 by Don Radlauer.										  */
/*							                                                  */
/* << -------------------------------------------------------------------- >> */
static int message_2065_handler(
				TConnection	*p_connect,
				int		data_len
				)
{

/* << -------------------------------------------------------- >> */
/*			  local variables			  */
/* << -------------------------------------------------------- >> */
  int		retcode;		/* code returned from function	    */
  int		n_retries;		/* retrying counter		    */
  int       db_changed_flg;



/* << -------------------------------------------------------- >> */
/*			   attempt delete			  */
/* << -------------------------------------------------------- >> */
  retcode = ALL_RETRIES_WERE_USED;
  db_changed_flg = 0;

  for( n_retries = 0;
       n_retries < SQL_UPDATE_RETRIES;
       n_retries++ )
    {

      /*
       * sleep for a while:
       */
      if( n_retries )
      {
          sleep( SLEEP_TIME_BETWEEN_RETRIES );
      }

		ExecSQL (	MAIN_DB, AS400SRV_T2065_DEL_confirm_authority	);

      HANDLE_DELETE_SQL_CODE( "2065", p_connect, &retcode, &db_changed_flg );

      break;
    }

  if( n_retries == SQL_UPDATE_RETRIES )
  {
     WRITE_ACCESS_CONFLICT_TO_LOG( "2065" );
  }

  if (retcode == MSG_HANDLER_SUCCEEDED)
	  rows_deleted++;

  return retcode;
} /* end of msg 2065 handler */



/* << -------------------------------------------------------------- >> */
/*																		*/
/*     handler for message 2066 - add to table AUTHORIZING AUTHORITY	*/
/*																		*/
/*     Added 21Oct2003 by Don Radlauer.									*/
/*																		*/
/* << -------------------------------------------------------------- >> */
static int message_2066_handler (TConnection	*p_connect,
								 int			data_len)
{
	// local variables
	char		*PosInBuff;		/* current pos in data buffer	    */
	bool		stop_retrying;	/* if true - return from function   */
	bool		do_insert;
	int			ReadInput;		/* amount of Bytesread from buffer  */
	int			retcode;		/* code returned from function	    */
	int			n_retries;		/* retrying counter		    */
	int			i;

	// DB variables
	int		sysdate;
	int		systime;
	short	v_authority_code;
	char	v_authority_desc[26];


	sysdate = GetDate();
	systime = GetTime();


	// read input buffer
	INIT_ERROR_DETECTION_MECHANISM;
	PosInBuff = glbMsgBuffer;


	v_authority_code	= get_short		(&PosInBuff, 2);	RETURN_ON_ERROR;
	get_string	(&PosInBuff, v_authority_desc, 15);

	// DonR 24Nov2003: Oops! Forgot to put the Authority Description in correct order so it won't
	// come out backwards.
	switch_to_win_heb ((unsigned char *)v_authority_desc);

	// verify all data was read
	ReadInput = PosInBuff - glbMsgBuffer;
	StaticAssert( ReadInput == data_len );


	// up to SQL_UPDATE_RETRIES retries will be made to overcome access conflict
	retcode = ALL_RETRIES_WERE_USED;
	stop_retrying = false;

	for (n_retries = 0; (n_retries < SQL_UPDATE_RETRIES) && (stop_retrying == false); n_retries++)
	{
		// sleep for a while
		if( n_retries )
		{
			sleep (SLEEP_TIME_BETWEEN_RETRIES);
		}

		// try to insert new row
		// DonR 04Jul2010: HOT FIX: Need to have dummy "for" loop here so that HANDLE_UPDATE_SQL_CODE
		// and HANDLE_INSERT_SQL_CODE work properly.
		for (i = 0; i < 1; i++)
		{
			ExecSQL (	MAIN_DB, AS400SRV_T2066_INS_confirm_authority,
						&v_authority_code,	&v_authority_desc,
						END_OF_ARG_LIST									);

			HANDLE_INSERT_SQL_CODE ("2066", p_connect, &do_insert, &retcode, &stop_retrying);

		}	// Dummy loop.

	}/* for( n_retries... */

	if( n_retries == SQL_UPDATE_RETRIES )
	{
		WRITE_ACCESS_CONFLICT_TO_LOG( "2066" );
	}


	if (retcode == MSG_HANDLER_SUCCEEDED)
	{
		rows_added++;
	}


	return retcode;
} /* end of msg 2066 handler */



/* << -------------------------------------------------------------------- >> */
/*																			  */
/*     handler for message 2067 - delete ALL rows from GADGETS				  */
/*																			  */
/*     Added 31May2004 by Don Radlauer.										  */
/*		26Nov2018: No longer in use!					                                                  */
/* << -------------------------------------------------------------------- >> */
static int message_2067_handler (TConnection *p_connect, int data_len)
{
	int	retcode;		/* code returned from function	    */
	int	n_retries;		/* retrying counter		    */
	int	db_changed_flg;
	int	dummy_changed_flg;
	int	old_stuff_margin	= -3600;	// Delete stuff that's older than one hour.

	// DB variables
	int	v_largo_code;
	int	SysDate;
	int	SysTime;
	int	OldStuffDate;
	int	OldStuffTime;


	DeclareCursor (	MAIN_DB, AS400SRV_T2067_del_gadget_cur,
					&OldStuffDate,		&OldStuffDate,
					&OldStuffTime,		END_OF_ARG_LIST		);


	// Set up date/time for selecting rows to delete.
	// DonR 28Jul2016: Change "old stuff" deletion threshold to a variable
	// (set to one hour - see above) and decrement date if necessary.
	SysDate			= OldStuffDate = GetDate ();
	SysTime			= GetTime ();
	OldStuffTime	= IncrementTime	(SysTime, old_stuff_margin, &OldStuffDate);

	/* << -------------------------------------------------------- >> */
	/*			   attempt delete			  */
	/* << -------------------------------------------------------- >> */
	retcode = ALL_RETRIES_WERE_USED;
	db_changed_flg = 0;

	for (n_retries = 0; n_retries < SQL_UPDATE_RETRIES; n_retries++)
	{
		// sleep for a while
		if( n_retries )
		{
			sleep (SLEEP_TIME_BETWEEN_RETRIES);
		}

		OpenCursor (	MAIN_DB, AS400SRV_T2067_del_gadget_cur	);
//GerrLogMini (GerrId,"Delete All Gadgets: SysDate %d, TwoMinutesAgo %d.", SysDate, TwoMinutesAgo);

		HANDLE_DELETE_SQL_CODE( "2067", p_connect, &retcode, &dummy_changed_flg );

		do
		{
			FetchCursorInto (	MAIN_DB, AS400SRV_T2067_del_gadget_cur,
								&v_largo_code,		END_OF_ARG_LIST		);
//GerrLogMini (GerrId,"Delete All Gadgets: Fetched Largo %d, SQLCODE = %d.", v_largo_code, SQLCODE);

			if (SQLERR_code_cmp (SQLERR_access_conflict) == MAC_TRUE)
			{
				retcode = ALL_RETRIES_WERE_USED;
				break; /* retry */
			}

			if (SQLERR_code_cmp (SQLERR_end_of_fetch) == MAC_TRUE)
			{
				break;
			}
			else
			{
				if (SQLERR_error_test ())
				{
					p_connect->sql_err_flg = 1;
					retcode = SQL_ERROR_OCCURED;
					break;
				}
				else
				{
					// We actually read a gadget row!
					// DonR 31Aug2015: When we update a drug's "gadget" status, we also need to update its timestamp so pharmacies get the message.
					ExecSQL (	MAIN_DB, AS400SRV_T2067_UPD_drug_list_clear_gadget_columns,
								&SysDate,		&SysTime,
								&v_largo_code,	END_OF_ARG_LIST								);
//GerrLogMini (GerrId,"Delete All Gadgets: Cleared flag for Largo %d, SQLCODE = %d.", v_largo_code, SQLCODE);

					ExecSQL (	MAIN_DB, AS400SRV_T2067_DEL_gadgets,
								&v_largo_code,	END_OF_ARG_LIST			);
//GerrLogMini (GerrId,"Delete All Gadgets: Deleted from Gadgets for Largo %d, SQLCODE = %d.", v_largo_code, SQLCODE);

					HANDLE_DELETE_SQL_CODE ("2067", p_connect, &retcode, &db_changed_flg);
					rows_deleted++;
				}
			}
		}
		while (1);


		CloseCursor (	MAIN_DB, AS400SRV_T2067_del_gadget_cur	);

		break;
	}

	if (n_retries == SQL_UPDATE_RETRIES)
	{
		WRITE_ACCESS_CONFLICT_TO_LOG ("2067");
	}


	return retcode;
} /* end of msg 2067 handler */



/* << -------------------------------------------------------------- >> */
/*																		*/
/*     handler for message 2068/4068 - add/update GADGETS				*/
/*																		*/
/*     Added 31May2004 by Don Radlauer.									*/
/*																		*/
/* << -------------------------------------------------------------- >> */
static int data_4068_handler (TConnection	*p_connect,
							  int			data_len)
{
	// local variables
	char		*PosInBuff;		/* current pos in data buffer	    */
	bool		stop_retrying;	/* if true - return from function   */
	bool		do_insert;
	int			ReadInput;		/* amount of Bytesread from buffer  */
	int			retcode;		/* code returned from function	    */
	int			n_retries;		/* retrying counter		    */
	int			i;
	bool		something_changed;

	// DB variables
	int			refresh_date;
	int			refresh_time;
	int			changed_date;
	int			changed_time;
	int			as400_batch_date;
	int			as400_batch_time;
	int			v_largo_code;		// (Key)
	int			v_gadget_code;		// (Key)
	int			v_service_code;
	short		v_service_number;
	short		v_service_type_num;
	short		v_enabled_status;
	short		v_enabled_mac;
	short		v_enabled_hova;
	short		v_enabled_keva;
	short		v_enabled_pvt_ph;
	short		v_enabled_no_rx;
	short		GadgetPrevPurchaseClassCode;	// DonR 28Feb2024 User Story #555590 (and also #551403).
	short		GadgetMinPrevPurchases;			// DonR 28Feb2024 User Story #555590 (and also #551403).
	short		GadgetMaxPrevPurchases;			// DonR 28Feb2024 User Story #555590 (and also #551403).
	char		v_service_type_alpha[5];
	char		v_service_desc[26];

	int			o_changed_date;
	int			o_changed_time;
	int			o_service_code;
	short		o_service_number;
	short		o_service_type_num;
	short		o_enabled_status;
	short		o_enabled_mac;
	short		o_enabled_hova;
	short		o_enabled_keva;
	short		o_enabled_pvt_ph;
	short		o_enabled_no_rx;
	short		o_PrevPurchaseClassCode;	// DonR 28Feb2024 User Story #555590 (and also #551403).
	short		o_MinPrevPurchases;			// DonR 28Feb2024 User Story #555590 (and also #551403).
	short		o_MaxPrevPurchases;			// DonR 28Feb2024 User Story #555590 (and also #551403).
	char		o_service_desc[26];


	refresh_date = as400_batch_date = (download_start_date > 0) ? download_start_date : GetDate();
	refresh_time = as400_batch_time = (download_start_time > 0) ? download_start_time : GetTime();


	// read input buffer
	INIT_ERROR_DETECTION_MECHANISM;
	PosInBuff = glbMsgBuffer;


	v_largo_code				= get_long		(&PosInBuff, 9);	RETURN_ON_ERROR;	//Marianna 14Jul2024 User Story #330877
	v_service_code				= get_long		(&PosInBuff, 5);	RETURN_ON_ERROR;
	v_service_number			= get_short		(&PosInBuff, 4);	RETURN_ON_ERROR;
	get_string	(&PosInBuff, v_service_type_alpha,	 2);
	get_string	(&PosInBuff, v_service_desc,		25);
	v_gadget_code				= get_long		(&PosInBuff, 5);	RETURN_ON_ERROR;
	v_enabled_status			= get_short		(&PosInBuff, 2);	RETURN_ON_ERROR;
	v_enabled_pvt_ph			= get_short		(&PosInBuff, 1);	RETURN_ON_ERROR;
	v_enabled_no_rx				= get_short		(&PosInBuff, 1);	RETURN_ON_ERROR;

	// DonR 28Feb2024 User Story #555590 (and also #551403): Three new fields from AS/400.
	GadgetPrevPurchaseClassCode	= get_short		(&PosInBuff, 4);	RETURN_ON_ERROR;
	GadgetMinPrevPurchases		= get_short		(&PosInBuff, 5);	RETURN_ON_ERROR;
	GadgetMaxPrevPurchases		= get_short		(&PosInBuff, 5);	RETURN_ON_ERROR;

	// verify all data was read
	ReadInput = PosInBuff - glbMsgBuffer;
	StaticAssert( ReadInput == data_len );

	strip_trailing_spaces (v_service_desc);	// DonR 09Feb2020 - since ODBC (at least for Informix) does the same on SELECT.

	// Translate Service Type to short integer.
	if (*v_service_type_alpha == '0')
		*v_service_type_alpha = ' ';
	v_service_type_num = atoi(v_service_type_alpha);

	// DonR 30Aug2005: Oops! Forgot to put the Service Description in correct order
	// so it won't come out backwards.
	switch_to_win_heb ((unsigned char *)v_service_desc);

	// DonR 24Nov2011: Translate v_enabled_status into individual enabled-flag variables.
	SET_TZAHAL_FLAGS;

	// up to SQL_UPDATE_RETRIES retries will be made to overcome access conflict
	retcode = ALL_RETRIES_WERE_USED;
	stop_retrying = false;

	for (n_retries = 0; (n_retries < SQL_UPDATE_RETRIES) && (stop_retrying == false); n_retries++)
	{
		// sleep for a while
		if (n_retries)
		{
			sleep (SLEEP_TIME_BETWEEN_RETRIES);
		}

		// DonR 31Aug2015: When we update a drug's "gadget" status, we also need to update its timestamp
		// so pharmacies get the message.
		ExecSQL (	MAIN_DB, AS400SRV_T4068_UPD_drug_list_gadget_columns,
					&refresh_date,		&refresh_time,
					&v_largo_code,		END_OF_ARG_LIST						);

		// try to insert/update new rec with the order depending on "do_insert":
		for (i = 0; i < 2; i++)
		{
			if (do_insert == ATTEMPT_INSERT_FIRST)
			{
				ExecSQL (	MAIN_DB, AS400SRV_T4068_INS_gadgets,
							&v_gadget_code,					&v_service_desc,			&v_largo_code,
							&v_service_code,				&v_service_number,			&v_service_type_num,
							&v_enabled_status,				&v_enabled_mac,				&v_enabled_hova,
							&v_enabled_keva,				&v_enabled_pvt_ph,			&v_enabled_no_rx,
							&GadgetPrevPurchaseClassCode,	&GadgetMinPrevPurchases,	&GadgetMaxPrevPurchases,

							&refresh_date,					&refresh_time,
							&refresh_date,					&refresh_time,
							&as400_batch_date,				&as400_batch_time,			END_OF_ARG_LIST				);

				HANDLE_INSERT_SQL_CODE ("2068", p_connect, &do_insert, &retcode, &stop_retrying);
			}

			else	// Update existing row.
			{
				ExecSQL (	MAIN_DB, AS400SRV_T4068_READ_gadgets_old_data,
							&o_service_desc,			&o_service_code,		&o_service_number,
							&o_service_type_num,		&o_enabled_pvt_ph,		&o_enabled_status,
							&o_enabled_mac,				&o_enabled_hova,		&o_enabled_keva,
							&o_enabled_no_rx,			&o_changed_date,		&o_changed_time,
							&o_PrevPurchaseClassCode,	&o_MinPrevPurchases,	&o_MaxPrevPurchases,
							&v_largo_code,				&v_gadget_code,			END_OF_ARG_LIST			);


				// By default, assume that something has changed.
				changed_date = refresh_date;
				changed_time = refresh_time;

				something_changed =	!	(	(SQLERR_code_cmp (SQLERR_ok)	==		MAC_TRUE				)	&&
											(!strcmp (v_service_desc,				o_service_desc			))	&&
											(v_service_code					==		o_service_code			)	&&
											(v_service_number				==		o_service_number		)	&&
											(v_service_type_num				==		o_service_type_num		)	&&
											(v_enabled_pvt_ph				==		o_enabled_pvt_ph		)	&&
											(v_enabled_status				==		o_enabled_status		)	&&
											(v_enabled_mac					==		o_enabled_mac			)	&&
											(v_enabled_hova					==		o_enabled_hova			)	&&
											(v_enabled_keva					==		o_enabled_keva			)	&&
											(GadgetPrevPurchaseClassCode	==		o_PrevPurchaseClassCode	)	&&
											(GadgetMinPrevPurchases			==		o_MinPrevPurchases		)	&&
											(GadgetMaxPrevPurchases			==		o_MaxPrevPurchases		)	&&
											(v_enabled_no_rx				==		o_enabled_no_rx			)		);

				if (!something_changed)
				{
					// Nothing's changed after all!
					changed_date = o_changed_date;
					changed_time = o_changed_time;
				}


				ExecSQL (	MAIN_DB, AS400SRV_T4068_UPD_gadgets,
							&v_service_desc,				&v_service_code,
							&v_service_number,				&v_service_type_num,
							&v_enabled_status,				&v_enabled_mac,
							&v_enabled_hova,				&v_enabled_keva, 
							&v_enabled_pvt_ph,				&v_enabled_no_rx,
							&GadgetPrevPurchaseClassCode,	&GadgetMinPrevPurchases,
							&GadgetMaxPrevPurchases,
							&refresh_date,					&refresh_time,
							&changed_date,					&changed_time,
							&as400_batch_date,				&as400_batch_time,
							&v_largo_code,					&v_gadget_code,
							END_OF_ARG_LIST												);

				HANDLE_UPDATE_SQL_CODE ("2068", p_connect, &do_insert, &retcode, &stop_retrying);
			}

		}/* for( i... */
	}/* for( n_retries... */

	if (n_retries == SQL_UPDATE_RETRIES)
	{
		WRITE_ACCESS_CONFLICT_TO_LOG ("2068");
	}


	if (retcode == MSG_HANDLER_SUCCEEDED)
	{
	  if (do_insert == ATTEMPT_INSERT_FIRST)
	  {
		  rows_added++;
	  }
	  else
	  {
          if (something_changed)
			  rows_updated++;
		  else
		      rows_refreshed++;
	  }
	}

	return retcode;
} // End of data_4068_handler().

// -------------------------------------------------------------------------
//
//       Post-processor for message 4068 - delete GADGETS
//			rows that are no longer in the AS/400 full-table download.
//
// -------------------------------------------------------------------------
static int postprocess_4068 (TConnection *p_connect)
{
	// local variables
	int		retcode						= MSG_HANDLER_SUCCEEDED;		// code returned from function

	// DB variables
	int				rows_deleted	= 0;
	int				sysdate;
	int				systime;
	int				v_as400_batch_date;
	int				v_as400_batch_time;


	// Initialize variables.
	v_as400_batch_date	= download_start_date;
	v_as400_batch_time	= download_start_time;
	sysdate				= GetDate ();
	systime				= GetTime ();


	ExecSQL (	MAIN_DB, AS400SRV_T4068PP_READ_gadgets_stale_count,
				&rows_deleted,			&v_as400_batch_date,
				&v_as400_batch_time,	END_OF_ARG_LIST				);

	ExecSQL (	MAIN_DB, AS400SRV_T4068PP_DEL_gadgets,
				&v_as400_batch_date,	&v_as400_batch_time,
				END_OF_ARG_LIST									);

	if (SQLCODE == 0)
	{
		ExecSQL (	MAIN_DB, AS400SRV_T4068PP_UPD_drug_list_clear_gadget_flag,
					&sysdate, &systime, END_OF_ARG_LIST							);
	}
		
	if (SQLERR_error_test ())
	{
		retcode = SQL_ERROR_OCCURED;
	}

	// DonR 13Nov2018 CR #15919: Record current table size.
	REMEMBER_ISOLATION;
	SET_ISOLATION_DIRTY;

	ExecSQL (	MAIN_DB, AS400SRV_T4068PP_READ_gadgets_table_size,
				&total_rows, END_OF_ARG_LIST						);

	active_rows	= total_rows;

	RESTORE_ISOLATION;
	// DonR 13Nov2018 CR #15919 end.


	GerrLogMini (GerrId,
				 "Postprocess 4068: %d rows deleted, retcode = %d.",
				 rows_deleted, retcode);

	return retcode;

}	// End of msg 4068 postprocessor.



/* << -------------------------------------------------------- >> */
/*                                                                */
/*       handler for message 2069 - upd/ins to CLICKS_DISCOUNT    */
/*                                                                */
/* << -------------------------------------------------------- >> */
static int message_2069_handler (TConnection	*p_connect,
								 int			data_len,
								 bool			do_insert)
{
	// local variables
	char	*PosInBuff;		/* current pos in data buffer	    */
	bool	stop_retrying;	/* if true - return from function   */
	bool	deleted_a_row;
	int		ReadInput;		/* amount of Bytesread from buffer  */
	int		retcode;		/* code returned from function	    */
	int		n_retries;		/* retrying counter					*/
	int		i;

	// DB variables
	char		v_discount_code		[ 1 + 1];
	char		v_show2spec			[ 3 + 1];
	char		v_show2nonspec		[ 3 + 1];
	char		v_ptn_spec_basic	[ 3 + 1];
	char		v_ptn_nonspec_basic	[ 3 + 1];
	char		v_ptn_spec_keren	[ 3 + 1];
	char		v_ptn_nonspec_keren	[ 3 + 1];
	char		v_spec_msg			[75 + 1];
	char		v_nonspec_msg		[75 + 1]; 
	short		v_show_needs_ishur;
	Tdel_flg_c	v_del_flg;

	int			sysdate;
	int			systime;


	// Initialization.
	GetVirtualTimestamp (&sysdate, &systime);


	// read input buffer
	INIT_ERROR_DETECTION_MECHANISM;
	PosInBuff = glbMsgBuffer;


	get_string (&PosInBuff, v_discount_code,		 1);
	get_string (&PosInBuff, v_show2spec,			 3);
	get_string (&PosInBuff, v_show2nonspec,			 3);
	v_show_needs_ishur	= get_short	(&PosInBuff, 1); RETURN_ON_ERROR;
	get_string (&PosInBuff, v_ptn_spec_basic,		 3);
	get_string (&PosInBuff, v_ptn_nonspec_basic,	 3);
	get_string (&PosInBuff, v_ptn_spec_keren,		 3);
	get_string (&PosInBuff, v_ptn_nonspec_keren,	 3);
	get_string (&PosInBuff, v_spec_msg,				75);
	get_string (&PosInBuff, v_nonspec_msg,			75);
	get_string (&PosInBuff, v_del_flg,				Tdel_flg_c_len);

	
	/* verify all data was read:   */
	ReadInput = PosInBuff - glbMsgBuffer;
	StaticAssert( ReadInput == data_len );

	switch_to_win_heb ((unsigned char *)v_spec_msg);
	switch_to_win_heb ((unsigned char *)v_nonspec_msg);

	/* << -------------------------------------------------------- >> */
	/*	   up to SQL_UPDATE_RETRIES retries will be made			  */
	/*		    to overcome access conflict							  */
	/* << -------------------------------------------------------- >> */
	retcode = ALL_RETRIES_WERE_USED;
	stop_retrying = false;

	for (n_retries = 0;
		 (n_retries < SQL_UPDATE_RETRIES) && (stop_retrying == false);
		 n_retries++)
	{
		// sleep for a while
		if( n_retries )
		{
			sleep( SLEEP_TIME_BETWEEN_RETRIES );
		}


		// try to insert/update new rec with the order depending on "do_insert":
		for (i = 0; i < 2; i++)
		{
			if (do_insert == ATTEMPT_INSERT_FIRST)
			{
				// Marianna 16Feb2023 - RK9060- clicks_discount is not part of the 
				// Pharmacy system and doesn't exist in the Linux database. 
				// The solution (at least for now) is to disable the SQL commands.
				if (1)
				{
					SQLCODE = 0;    // Fake success. 
				}
				else
				{
					ExecSQL (	MAIN_DB, AS400SRV_T2069_INS_clicks_discount,
							&v_discount_code,			&v_show2spec,
							&v_show2nonspec,			&v_show_needs_ishur,
							&v_ptn_spec_basic,			&v_ptn_nonspec_basic,
							&v_ptn_spec_keren,			&v_ptn_nonspec_keren,
							&v_spec_msg,				&v_nonspec_msg,
							&sysdate,					&systime,
							&UPDATED_BY_AS400,			&v_del_flg,
							END_OF_ARG_LIST										);
				}
				
				HANDLE_INSERT_SQL_CODE ("2069", p_connect, &do_insert, &retcode, &stop_retrying);
			}
			else
			{
				// Marianna 16Feb2023 - RK9060- clicks_discount is not part of the 
				// Pharmacy system and doesn't exist in the Linux database. 
				// The solution (at least for now) is to disable the SQL commands.
				if (1)
				{
					SQLCODE = 0;    // Fake success. 
				}
				else
				{
					ExecSQL (	MAIN_DB, AS400SRV_T2069_UPD_clicks_discount,
							&sysdate,					&systime,
							&UPDATED_BY_AS400,			&v_show2spec,
							&v_show2nonspec,			&v_show_needs_ishur,
							&v_ptn_spec_basic,			&v_ptn_nonspec_basic,
							&v_ptn_spec_keren,			&v_ptn_nonspec_keren,
							&v_spec_msg,				&v_nonspec_msg,
							&v_del_flg,
							&v_discount_code,			END_OF_ARG_LIST			);
				}
				
				HANDLE_UPDATE_SQL_CODE ("2069", p_connect, &do_insert, &retcode, &stop_retrying);
			}

		}/* for( i... */

	}/* for( n_retries... */


	// Update counters at this point - since we'll be altering retocode and do_insert below.
	if (retcode == MSG_HANDLER_SUCCEEDED)
	{
		if (do_insert == ATTEMPT_INSERT_FIRST)
			rows_added++;
		else
			rows_updated++;
	}


	if( n_retries == SQL_UPDATE_RETRIES )
	{
		WRITE_ACCESS_CONFLICT_TO_LOG ("2069");
	}


	return retcode;

}	// end of msg 2069 handler



/* << -------------------------------------------------------- >> */
/*                                                                */
/*       handler for message 2070 - upd/ins to DRUG_FORMS		  */
/*                                                                */
/* << -------------------------------------------------------- >> */
static int message_2070_handler (TConnection	*p_connect,
								 int			data_len,
								 bool			do_insert)
{
	// local variables
	char	*PosInBuff;		/* current pos in data buffer	    */
	bool	stop_retrying;	/* if true - return from function   */
	bool	deleted_a_row;
	int		ReadInput;		/* amount of Bytesread from buffer  */
	int		retcode;		/* code returned from function	    */
	int		n_retries;		/* retrying counter					*/
	int		i;

	// DB variables
	Tlargo_code		v_largo_code;
	short			v_form_number;
	Tdel_flg_c		v_del_flg;
	int				v_valid_from;
	int				v_valid_until;

	int				sysdate;
	int				systime;


	// Initialization.
	sysdate = GetDate();
//	systime = GenerateVirtualTime (1);
	systime = GetTime();	// DonR 02Aug2006: Change table to real-time mode.


	// read input buffer
	INIT_ERROR_DETECTION_MECHANISM;
	PosInBuff = glbMsgBuffer;


	GET_LONG	(v_largo_code	,	9);	//Marianna 14Jul2024 User Story #330877
	GET_SHORT	(v_form_number	,	3);
	GET_STRING	(v_del_flg		,	1);
	GET_LONG	(v_valid_from	,	8);
	GET_LONG	(v_valid_until	,	8);

	
	/* verify all data was read:   */
	ReadInput = PosInBuff - glbMsgBuffer;
	StaticAssert( ReadInput == data_len );



	/* << -------------------------------------------------------- >> */
	/*	   up to SQL_UPDATE_RETRIES retries will be made			  */
	/*		    to overcome access conflict							  */
	/* << -------------------------------------------------------- >> */
	retcode = ALL_RETRIES_WERE_USED;
	stop_retrying = false;

	for (n_retries = 0;
		 (n_retries < SQL_UPDATE_RETRIES) && (stop_retrying == false);
		 n_retries++)
	{
		// sleep for a while
		if( n_retries )
		{
			sleep( SLEEP_TIME_BETWEEN_RETRIES );
		}


		// try to insert/update new rec with the order depending on "do_insert":
		for (i = 0; i < 2; i++)
		{
			if (do_insert == ATTEMPT_INSERT_FIRST)
			{
				ExecSQL (	MAIN_DB, AS400SRV_T2070_INS_drug_forms,
							&v_largo_code,			&v_form_number,
							&sysdate,				&systime,
							&UPDATED_BY_AS400,		&v_del_flg,
							&v_valid_from,			&v_valid_until,
							END_OF_ARG_LIST							);

				HANDLE_INSERT_SQL_CODE ("2070", p_connect, &do_insert, &retcode, &stop_retrying);
			}
			else
			{
				ExecSQL (	MAIN_DB, AS400SRV_T2070_UPD_drug_forms,
							&sysdate,				&systime,
							&UPDATED_BY_AS400,		&v_del_flg,
							&v_valid_from,			&v_valid_until,
							&v_largo_code,			&v_form_number,
							END_OF_ARG_LIST								);

				HANDLE_UPDATE_SQL_CODE ("2070", p_connect, &do_insert, &retcode, &stop_retrying);
			}

		}/* for( i... */

	}/* for( n_retries... */


	// Update counters at this point - since we'll be altering retocode and do_insert below.
	if (retcode == MSG_HANDLER_SUCCEEDED)
	{
		if (do_insert == ATTEMPT_INSERT_FIRST)
			rows_added++;
		else
			rows_updated++;
	}


	if( n_retries == SQL_UPDATE_RETRIES )
	{
		WRITE_ACCESS_CONFLICT_TO_LOG ("2070");
	}


	return retcode;

}	// end of msg 2070 handler


#if 0
/* << -------------------------------------------------------- >> */
/*                                                                */
/*       handler for message 2071 - upd/ins to USAGE_INSTRUCTIONS */
/*                                                                */
/* << -------------------------------------------------------- >> */
static int message_2071_handler (TConnection	*p_connect,
								 int			data_len,
								 bool			do_insert)
{
	// local variables
	char	*PosInBuff;		/* current pos in data buffer	    */
	bool	stop_retrying;	/* if true - return from function   */
	bool	deleted_a_row;
	int		ReadInput;		/* amount of Bytesread from buffer  */
	int		retcode;		/* code returned from function	    */
	int		n_retries;		/* retrying counter					*/
	int		i;

	// DB variables
	short		v_shape_num;            
	short		v_inst_code;            
	short		v_inst_seq;             
	short		v_calc_op_flag;         
	short		v_open_od_window;
	short		v_concentration_flag;
	short		v_unit_seq;
	short		v_round_units_flag;
	char		v_shape_code		[ 4 /* Tinst_shape_code_len */	+ 1]; 
	char		v_shape_desc		[25 /* Tshape_desc_len */		+ 1]; 
	char		v_inst_msg			[40 /* Tinst_msg_len */			+ 1]; 
	char		v_unit_code			[ 3 /* Tunit_code_len */		+ 1]; 
	char		v_unit_name			[ 8 /* Tunit_name_len */		+ 1]; 
	char		v_unit_desc			[25 /* Tunit_desc_len */		+ 1]; 
	char		v_total_unit_name	[10 /* Ttotal_unit_name_len */	+ 1];
	Tdel_flg_c	v_del_flg;

	int			sysdate;
	int			systime;
	int			RefreshDate;
	int			RefreshTime;


	// Initialization.
	GetVirtualTimestamp (&sysdate, &systime);
	RefreshDate = GetDate();
	RefreshTime = GetTime();


	// read input buffer
	INIT_ERROR_DETECTION_MECHANISM;
	PosInBuff = glbMsgBuffer;

	v_shape_num				=	get_short	(&PosInBuff,	Tshape_num_len			); RETURN_ON_ERROR;
								get_string	(&PosInBuff,
	v_shape_code,											Tinst_shape_code_len	);
								get_string	(&PosInBuff,
	v_shape_desc,											Tshape_desc_len			);
	v_concentration_flag	=	get_short	(&PosInBuff,	Tconcentration_flag_len	); RETURN_ON_ERROR;
	v_inst_code				=	get_short	(&PosInBuff,	Tinst_code_len			); RETURN_ON_ERROR;
								get_string	(&PosInBuff,
	v_inst_msg,												Tinst_msg_len			);
	v_inst_seq				=	get_short	(&PosInBuff,	Tinst_seq_len			); RETURN_ON_ERROR;
	v_calc_op_flag			=	get_short	(&PosInBuff,	Tcalc_op_flag_len		); RETURN_ON_ERROR;
								get_string	(&PosInBuff,
	v_unit_code,											Tunit_code_len			);
	v_unit_seq				=	get_short	(&PosInBuff,	Tunit_seq_len			); RETURN_ON_ERROR;
								get_string	(&PosInBuff,
	v_unit_name,											Tunit_name_len			);
								get_string	(&PosInBuff,
	v_unit_desc,											Tunit_desc_len			);
	v_open_od_window		=	get_short	(&PosInBuff,	Topen_od_window_len		); RETURN_ON_ERROR;
								get_string	(&PosInBuff,
	v_total_unit_name,										Ttotal_unit_name_len	);
	v_round_units_flag		=	get_short	(&PosInBuff,	Tround_units_flag_len	); RETURN_ON_ERROR;
								get_string	(&PosInBuff,
	v_del_flg,												Tdel_flg_c_len			);

	
	/* verify all data was read:   */
	ReadInput = PosInBuff - glbMsgBuffer;
	StaticAssert( ReadInput == data_len );

	switch_to_win_heb ((unsigned char *)v_shape_desc);
	switch_to_win_heb ((unsigned char *)v_inst_msg);
	switch_to_win_heb ((unsigned char *)v_unit_name);
	switch_to_win_heb ((unsigned char *)v_unit_desc);
	switch_to_win_heb ((unsigned char *)v_total_unit_name);


	/* << -------------------------------------------------------- >> */
	/*	   up to SQL_UPDATE_RETRIES retries will be made			  */
	/*		    to overcome access conflict							  */
	/* << -------------------------------------------------------- >> */
	retcode = ALL_RETRIES_WERE_USED;
	stop_retrying = false;

	for (n_retries = 0;
		 (n_retries < SQL_UPDATE_RETRIES) && (stop_retrying == false);
		 n_retries++)
	{
		// sleep for a while
		if( n_retries )
		{
			sleep( SLEEP_TIME_BETWEEN_RETRIES );
		}


		// try to insert/update new rec with the order depending on "do_insert":
		for (i = 0; i < 2; i++)
		{
			if (do_insert == ATTEMPT_INSERT_FIRST)
			{
				ExecSQL (	MAIN_DB, AS400SRV_T2071_INS_usage_instruct,
							&v_shape_num,				&v_shape_code,
							&v_shape_desc,				&v_inst_code,
							&v_inst_msg,				&v_inst_seq,
							&v_calc_op_flag,			&v_unit_code,
							&v_unit_seq,				&v_unit_name,
							&v_unit_desc,				&v_open_od_window,
							&v_concentration_flag,		&v_total_unit_name,
							&v_round_units_flag,		&sysdate,
							&systime,					&UPDATED_BY_AS400,
							&v_del_flg,					END_OF_ARG_LIST			);

				HANDLE_INSERT_SQL_CODE ("2071", p_connect, &do_insert, &retcode, &stop_retrying);
			}
			else
			{
				ExecSQL (	MAIN_DB, AS400SRV_T2071_UPD_usage_instruct,
							&sysdate,					&systime,
							&UPDATED_BY_AS400,			&v_shape_code,
							&v_shape_desc,				&v_inst_msg,
							&v_calc_op_flag,			&v_unit_seq,
							&v_unit_name,				&v_unit_desc,
							&v_open_od_window,			&v_concentration_flag,
							&v_total_unit_name,			&v_round_units_flag,
							&v_del_flg,
							&v_shape_num,				&v_inst_code,
							&v_inst_seq,				&v_unit_code,
							END_OF_ARG_LIST										);

				HANDLE_UPDATE_SQL_CODE ("2071", p_connect, &do_insert, &retcode, &stop_retrying);
			}

		}/* for( i... */

	}/* for( n_retries... */


	// Update counters at this point - since we'll be altering retocode and do_insert below.
	if (retcode == MSG_HANDLER_SUCCEEDED)
	{
		if (do_insert == ATTEMPT_INSERT_FIRST)
			rows_added++;
		else
			rows_updated++;
	}


	if( n_retries == SQL_UPDATE_RETRIES )
	{
		WRITE_ACCESS_CONFLICT_TO_LOG ("2071");
	}


	return retcode;

}	// end of msg 2071 handler
#endif



/* << -------------------------------------------------------------- >> */
/*																        */
/*       handler for message 2071/4071 - upd/ins to USAGE_INSTRUCTIONS	*/
/*																        */
/* << -------------------------------------------------------------- >> */
static int data_4071_handler (TConnection	*p_connect,
							  int			data_len,
							  bool			do_insert)
{
	// local variables
	char	*PosInBuff;		/* current pos in data buffer	    */
	bool	stop_retrying;	/* if true - return from function   */
	bool	deleted_a_row;
	int		ReadInput;		/* amount of Bytesread from buffer  */
	int		retcode;		/* code returned from function	    */
	int		n_retries;		/* retrying counter					*/
	int		i;

	// DB variables
	short		v_shape_num;            
	short		v_inst_code;            
	short		v_inst_seq;             
	short		v_calc_op_flag;         
	short		v_open_od_window;
	short		v_concentration_flag;
	short		v_unit_seq;
	short		v_round_units_flag;
	char		v_shape_code		[ 4 /* Tinst_shape_code_len */	+ 1]; 
	char		v_shape_desc		[25 /* Tshape_desc_len */		+ 1]; 
	char		v_inst_msg			[40 /* Tinst_msg_len */			+ 1]; 
	char		v_unit_code			[ 3 /* Tunit_code_len */		+ 1]; 
	char		v_unit_name			[ 8 /* Tunit_name_len */		+ 1]; 
	char		v_unit_desc			[25 /* Tunit_desc_len */		+ 1]; 
	char		v_total_unit_name	[10 /* Ttotal_unit_name_len */	+ 1];
	Tdel_flg_c	v_del_flg;

//	int			sysdate;
//	int			systime;
	int			RefreshDate;
	int			RefreshTime;


	// Initialization.
//	GetVirtualTimestamp (&sysdate, &systime);
//	RefreshDate = GetDate();
//	RefreshTime = GetTime();
//	RefreshDate = as400_batch_date = (download_start_date > 0) ? download_start_date : GetDate();
//	RefreshTime = as400_batch_time = (download_start_time > 0) ? download_start_time : GetTime();
	RefreshDate = (download_start_date > 0) ? download_start_date : GetDate();
	RefreshTime = (download_start_time > 0) ? download_start_time : GetTime();


	// read input buffer
	INIT_ERROR_DETECTION_MECHANISM;
	PosInBuff = glbMsgBuffer;

	v_shape_num				=	get_short	(&PosInBuff,	Tshape_num_len			); RETURN_ON_ERROR;
								get_string	(&PosInBuff,
	v_shape_code,											Tinst_shape_code_len	);
								get_string	(&PosInBuff,
	v_shape_desc,											Tshape_desc_len			);
	v_concentration_flag	=	get_short	(&PosInBuff,	Tconcentration_flag_len	); RETURN_ON_ERROR;
	v_inst_code				=	get_short	(&PosInBuff,	Tinst_code_len			); RETURN_ON_ERROR;
								get_string	(&PosInBuff,
	v_inst_msg,												Tinst_msg_len			);
	v_inst_seq				=	get_short	(&PosInBuff,	Tinst_seq_len			); RETURN_ON_ERROR;
	v_calc_op_flag			=	get_short	(&PosInBuff,	Tcalc_op_flag_len		); RETURN_ON_ERROR;
								get_string	(&PosInBuff,
	v_unit_code,											Tunit_code_len			);
	v_unit_seq				=	get_short	(&PosInBuff,	Tunit_seq_len			); RETURN_ON_ERROR;
								get_string	(&PosInBuff,
	v_unit_name,											Tunit_name_len			);
								get_string	(&PosInBuff,
	v_unit_desc,											Tunit_desc_len			);
	v_open_od_window		=	get_short	(&PosInBuff,	Topen_od_window_len		); RETURN_ON_ERROR;
								get_string	(&PosInBuff,
	v_total_unit_name,										Ttotal_unit_name_len	);
	v_round_units_flag		=	get_short	(&PosInBuff,	Tround_units_flag_len	); RETURN_ON_ERROR;
								get_string	(&PosInBuff,
	v_del_flg,												Tdel_flg_c_len			);

	
	/* verify all data was read:   */
	ReadInput = PosInBuff - glbMsgBuffer;
	StaticAssert( ReadInput == data_len );

	switch_to_win_heb ((unsigned char *)v_shape_desc);
	switch_to_win_heb ((unsigned char *)v_inst_msg);
	switch_to_win_heb ((unsigned char *)v_unit_name);
	switch_to_win_heb ((unsigned char *)v_unit_desc);
	switch_to_win_heb ((unsigned char *)v_total_unit_name);


	/* << -------------------------------------------------------- >> */
	/*	   up to SQL_UPDATE_RETRIES retries will be made			  */
	/*		    to overcome access conflict							  */
	/* << -------------------------------------------------------- >> */
	retcode = ALL_RETRIES_WERE_USED;
	stop_retrying = false;

	for (n_retries = 0;
		 (n_retries < SQL_UPDATE_RETRIES) && (stop_retrying == false);
		 n_retries++)
	{
		// sleep for a while
		if (n_retries)
		{
			sleep (SLEEP_TIME_BETWEEN_RETRIES);
		}


		// try to insert/update new rec with the order depending on "do_insert":
		for (i = 0; i < 2; i++)
		{
			if (do_insert == ATTEMPT_INSERT_FIRST)
			{
				ExecSQL (	MAIN_DB, AS400SRV_T2071_INS_usage_instruct,
							&v_shape_num,				&v_shape_code,
							&v_shape_desc,				&v_inst_code,
							&v_inst_msg,				&v_inst_seq,
							&v_calc_op_flag,			&v_unit_code,
							&v_unit_seq,				&v_unit_name,
							&v_unit_desc,				&v_open_od_window,
							&v_concentration_flag,		&v_total_unit_name,
							&v_round_units_flag,		&RefreshDate,
							&RefreshTime,				&UPDATED_BY_AS400,
							&v_del_flg,					END_OF_ARG_LIST			);

				HANDLE_INSERT_SQL_CODE ("4071", p_connect, &do_insert, &retcode, &stop_retrying);
			}
			else
			{
				ExecSQL (	MAIN_DB, AS400SRV_T2071_UPD_usage_instruct,
							&RefreshDate,				&RefreshTime,
							&UPDATED_BY_AS400,			&v_shape_code,
							&v_shape_desc,				&v_inst_msg,
							&v_calc_op_flag,			&v_unit_seq,
							&v_unit_name,				&v_unit_desc,
							&v_open_od_window,			&v_concentration_flag,
							&v_total_unit_name,			&v_round_units_flag,
							&v_del_flg,
							&v_shape_num,				&v_inst_code,
							&v_inst_seq,				&v_unit_code,
							END_OF_ARG_LIST										);

				HANDLE_UPDATE_SQL_CODE ("4071", p_connect, &do_insert, &retcode, &stop_retrying);
			}

		}/* for( i... */

	}/* for( n_retries... */


	// Update counters at this point - since we'll be altering retocode and do_insert below.
	if (retcode == MSG_HANDLER_SUCCEEDED)
	{
		if (do_insert == ATTEMPT_INSERT_FIRST)
			rows_added++;
		else
			rows_updated++;
	}


	if( n_retries == SQL_UPDATE_RETRIES )
	{
		WRITE_ACCESS_CONFLICT_TO_LOG ("4071");
	}


	return retcode;

}	// end of msg 2071/4071 handler


// -------------------------------------------------------------------------
//
//       Post-processor for message 4071 - delete usage_instruct
//			rows that are no longer in the AS/400 full-table download.
//
// -------------------------------------------------------------------------
static int postprocess_4071 (TConnection	*p_connect)
{
	// local variables
	bool	stop_retrying;	// if true - return from function
	int		rows_deleted	= 0;
	int		retcode;		// code returned from function
	int		n_retries;		// retrying counter
	int		db_changed_flg;

	// DB variables
	short	shape_num;
	int		v_as400_batch_date;
	int		v_as400_batch_time;


	// Initialize variables.
	v_as400_batch_date	= download_start_date;
	v_as400_batch_time	= download_start_time;


	retcode			= ALL_RETRIES_WERE_USED;
	stop_retrying	= false;


	DeclareAndOpenCursorInto (	MAIN_DB, AS400SRV_T4071PP_postproc_cur,
								&shape_num,
								&v_as400_batch_date,	&v_as400_batch_date,
								&v_as400_batch_time,	END_OF_ARG_LIST			);

	if (SQLERR_error_test ())
	{
		retcode = SQL_ERROR_OCCURED;
//		break;
	}

	while (1)
	{
		FetchCursor (	MAIN_DB, AS400SRV_T4071PP_postproc_cur	);

		if (SQLERR_code_cmp (SQLERR_end_of_fetch) == MAC_TRUE)
		{
			retcode = MSG_HANDLER_SUCCEEDED;	// In case there is nothing that needs to be deleted.
			break;
		}
		else
		{
			if (SQLERR_error_test ())
			{
				retcode = SQL_ERROR_OCCURED;
				break;
			}
		}

		// Nothing fancy here - just a physical delete.
		ExecSQL (	MAIN_DB, AS400SRV_T4071PP_DEL_usage_instruct	);

		HANDLE_DELETE_SQL_CODE ("postprocess_4071", p_connect, &retcode, &db_changed_flg);

		rows_deleted++;
	}	// Cursor-scrolling loop.

	CloseCursor (	MAIN_DB, AS400SRV_T4071PP_postproc_cur	);


	// DonR 13Nov2018 CR #15919: Record current table size.
	REMEMBER_ISOLATION;
	SET_ISOLATION_DIRTY;

	ExecSQL (	MAIN_DB, AS400SRV_T4071PP_READ_usage_instruct_table_size,
				&total_rows, END_OF_ARG_LIST											);

	active_rows = total_rows;

	RESTORE_ISOLATION;
	// DonR 13Nov2018 CR #15919 end.


	GerrLogMini (GerrId,
				 "Postprocess 4071: %d rows logically deleted, retcode = %d.",
				 rows_deleted, retcode);

	return retcode;

}	// End of msg 4071 (usage_instruct) postprocessor.



/* << -------------------------------------------------------- >> */
/*                                                                */
/*       handler for message 2072 - upd/ins to TREATMENT_GROUPS   */
/*                                                                */
/* << -------------------------------------------------------- >> */
static int message_2072_handler (TConnection	*p_connect,
								 int			data_len,
								 bool			do_insert)
{
	// local variables
	char	*PosInBuff;		/* current pos in data buffer	    */
	bool	stop_retrying;	/* if true - return from function   */
	bool	deleted_a_row;
	int		ReadInput;		/* amount of Bytesread from buffer  */
	int		retcode;		/* code returned from function	    */
	int		n_retries;		/* retrying counter					*/
	int		i;

	// DB variables
	short		v_pharmacology_code;   
	short		v_treatment_group;     
	char		v_treat_grp_desc  [40 + 1];
	short		v_presc_valid_days;    
	Tdel_flg_c	v_del_flg;

	int			sysdate;
	int			systime;


	// Initialization.
	GetVirtualTimestamp (&sysdate, &systime);


	// read input buffer
	INIT_ERROR_DETECTION_MECHANISM;
	PosInBuff = glbMsgBuffer;


	v_pharmacology_code	= get_short	(&PosInBuff,		 2); RETURN_ON_ERROR;
	v_treatment_group	= get_short	(&PosInBuff,		 2); RETURN_ON_ERROR;
	get_string			(&PosInBuff, v_treat_grp_desc,	40);
	v_presc_valid_days	= get_short	(&PosInBuff,		 3); RETURN_ON_ERROR;
	get_string			(&PosInBuff, v_del_flg,			Tdel_flg_c_len);

	
	/* verify all data was read:   */
	ReadInput = PosInBuff - glbMsgBuffer;
	StaticAssert( ReadInput == data_len );

	switch_to_win_heb ((unsigned char *)v_treat_grp_desc);


	/* << -------------------------------------------------------- >> */
	/*	   up to SQL_UPDATE_RETRIES retries will be made			  */
	/*		    to overcome access conflict							  */
	/* << -------------------------------------------------------- >> */
	retcode = ALL_RETRIES_WERE_USED;
	stop_retrying = false;

	for (n_retries = 0;
		 (n_retries < SQL_UPDATE_RETRIES) && (stop_retrying == false);
		 n_retries++)
	{
		// sleep for a while
		if( n_retries )
		{
			sleep( SLEEP_TIME_BETWEEN_RETRIES );
		}


		// try to insert/update new rec with the order depending on "do_insert":
		for (i = 0; i < 2; i++)
		{
			if (do_insert == ATTEMPT_INSERT_FIRST)
			{
				ExecSQL (	MAIN_DB, AS400SRV_T2072_INS_treatment_group,
							&v_pharmacology_code,		&v_treatment_group,
							&v_treat_grp_desc,			&v_presc_valid_days,
							&sysdate,					&systime,
							&UPDATED_BY_AS400,			&v_del_flg,
							END_OF_ARG_LIST										);

				HANDLE_INSERT_SQL_CODE ("2072", p_connect, &do_insert, &retcode, &stop_retrying);
			}
			else
			{
				ExecSQL (	MAIN_DB, AS400SRV_T2072_UPD_treatment_group,
							&sysdate,					&systime,
							&UPDATED_BY_AS400,			&v_treat_grp_desc,
							&v_presc_valid_days,		&v_del_flg,
							&v_pharmacology_code,		&v_treatment_group,
							END_OF_ARG_LIST										);

				HANDLE_UPDATE_SQL_CODE ("2072", p_connect, &do_insert, &retcode, &stop_retrying);
			}

		}/* for( i... */

	}/* for( n_retries... */


	// Update counters at this point - since we'll be altering retocode and do_insert below.
	if (retcode == MSG_HANDLER_SUCCEEDED)
	{
		if (do_insert == ATTEMPT_INSERT_FIRST)
			rows_added++;
		else
			rows_updated++;
	}


	if( n_retries == SQL_UPDATE_RETRIES )
	{
		WRITE_ACCESS_CONFLICT_TO_LOG ("2072");
	}


	return retcode;

}	// end of msg 2072 handler



/* << -------------------------------------------------------- >> */
/*                                                                */
/*       handler for message 2073 - upd/ins to PRESC_PERIOD		  */
/*                                                                */
/* << -------------------------------------------------------- >> */
static int message_2073_handler (TConnection	*p_connect,
								 int			data_len,
								 bool			do_insert)
{
	// local variables
	char	*PosInBuff;		/* current pos in data buffer	    */
	bool	stop_retrying;	/* if true - return from function   */
	bool	deleted_a_row;
	int		ReadInput;		/* amount of Bytesread from buffer  */
	int		retcode;		/* code returned from function	    */
	int		n_retries;		/* retrying counter					*/
	int		i;

	// DB variables
	short		v_presc_type;
	short		v_month;
	short		v_from_day;
	short		v_to_day;
	Tdel_flg_c	v_del_flg;

	int			sysdate;
	int			systime;


	// Initialization.
	GetVirtualTimestamp (&sysdate, &systime);


	// read input buffer
	INIT_ERROR_DETECTION_MECHANISM;
	PosInBuff = glbMsgBuffer;


	v_presc_type	= get_short	(&PosInBuff, 1); RETURN_ON_ERROR;
	v_month			= get_short	(&PosInBuff, 2); RETURN_ON_ERROR;
	v_from_day		= get_short	(&PosInBuff, 3); RETURN_ON_ERROR;
	v_to_day		= get_short	(&PosInBuff, 3); RETURN_ON_ERROR;
	get_string		(&PosInBuff, v_del_flg,	 Tdel_flg_c_len);

	
	/* verify all data was read:   */
	ReadInput = PosInBuff - glbMsgBuffer;
	StaticAssert( ReadInput == data_len );



	/* << -------------------------------------------------------- >> */
	/*	   up to SQL_UPDATE_RETRIES retries will be made			  */
	/*		    to overcome access conflict							  */
	/* << -------------------------------------------------------- >> */
	retcode = ALL_RETRIES_WERE_USED;
	stop_retrying = false;

	for (n_retries = 0;
		 (n_retries < SQL_UPDATE_RETRIES) && (stop_retrying == false);
		 n_retries++)
	{
		// sleep for a while
		if( n_retries )
		{
			sleep( SLEEP_TIME_BETWEEN_RETRIES );
		}


		// try to insert/update new rec with the order depending on "do_insert":
		for (i = 0; i < 2; i++)
		{
			if (do_insert == ATTEMPT_INSERT_FIRST)
			{
				ExecSQL (	MAIN_DB, AS400SRV_T2073_INS_presc_period,
							&v_presc_type,			&v_month,
							&v_from_day,			&v_to_day,
							&sysdate,				&systime,
							&UPDATED_BY_AS400,		&v_del_flg,
							END_OF_ARG_LIST								);

				HANDLE_INSERT_SQL_CODE ("2073", p_connect, &do_insert, &retcode, &stop_retrying);
			}
			else
			{
				ExecSQL (	MAIN_DB, AS400SRV_T2073_UPD_presc_period,
							&sysdate,				&systime,
							&UPDATED_BY_AS400,		&v_from_day,
							&v_to_day,				&v_del_flg,
							&v_presc_type,			&v_month,
							END_OF_ARG_LIST								);

				HANDLE_UPDATE_SQL_CODE ("2073", p_connect, &do_insert, &retcode, &stop_retrying);
			}

		}/* for( i... */

	}/* for( n_retries... */


	// Update counters at this point - since we'll be altering retocode and do_insert below.
	if (retcode == MSG_HANDLER_SUCCEEDED)
	{
		if (do_insert == ATTEMPT_INSERT_FIRST)
			rows_added++;
		else
			rows_updated++;
	}


	if( n_retries == SQL_UPDATE_RETRIES )
	{
		WRITE_ACCESS_CONFLICT_TO_LOG ("2073");
	}


	return retcode;

}	// end of msg 2073 handler



// -------------------------------------------------------------------------
//
//       handler for message 2074/4074 - upd/ins to DRUG_EXTN_DOC
//
//            Performs detailed comparison to detect rows that have changed.
//
// -------------------------------------------------------------------------
static int data_4074_handler (TConnection	*p_connect,
							  int			data_len)
{
	// local variables
	char	*PosInBuff;		// current pos in data buffer
	bool	stop_retrying;	// if true - return from function
	bool	need_to_read;
	bool	something_changed	= 0;
	short	do_insert			= ATTEMPT_UPDATE_FIRST;
	int		ReadInput;		// amount of Bytesread from buffer
	int		retcode;		// code returned from function
	int		n_retries;		// retrying counter
	int		i;

	// DB variables
	int					v_rule_number;
	Tlargo_code			v_largo_code;
	Textension_largo_co	v_extension_largo_co;
	Tconfirm_authority_	v_confirm_authority;
	char				v_conf_auth_desc[25 /* Tconf_auth_desc_len */ + 1];
	Tfrom_age			v_from_age;
	Tto_age				v_to_age;
	Tprice_code			v_basic_price_code;
	Tprice_code			v_kesef_price_code;
	Tprice_code			v_zahav_price_code;
	Tprice_code			v_yahalom_prc_code;
	Tprice_percent		v_member_price_prcnt;
	Tprice_percent		v_keren_price_prcnt;
	Tprice_percent		v_magen_price_prcnt;
	Tprice_percent		v_percnt_y;
	Tmax_op     		v_max_op_long;
	Tmax_units			v_max_units;
	Tmax_amount_duratio	v_max_amount_duratio;
	TPermissionType		v_permission_type;
	short int			v_no_presc_sale_flag;
	int					v_basic_fixed_price;
	int					v_fixed_price_keren;
	int					v_fixed_price_magen;
	int					v_fixed_pr_yahalom;
	char				v_ivf_flag	[1 + 1];
	short				v_refill_period;
	TGenericText75		v_rule_name;
	TGenericFlag		v_in_health_basket;
	TTreatmentCategory	v_treatment_category;
	Tsex				v_sex;
	TGenericFlag		v_needs_29_g;
	Tdel_flg_c			v_del_flg;
	short				v_send_and_use;
	short				v_wait_months;

	Textension_largo_co	o_extension_largo_co;
	Tconfirm_authority_	o_confirm_authority;
	char				o_conf_auth_desc[25 /* Tconf_auth_desc_len */ + 1];
	Tfrom_age			o_from_age;
	Tto_age				o_to_age;
	Tprice_code			o_basic_price_code;
	Tprice_code			o_kesef_price_code;
	Tprice_code			o_zahav_price_code;
	Tprice_code			o_yahalom_prc_code;
	Tprice_percent		o_member_price_prcnt;
	Tprice_percent		o_keren_price_prcnt;
	Tprice_percent		o_magen_price_prcnt;
	Tprice_percent		o_percnt_y;
	Tmax_op     		o_max_op_long;
	Tmax_units			o_max_units;
	Tmax_amount_duratio	o_max_amount_duratio;
	TPermissionType		o_permission_type;
	short int			o_no_presc_sale_flag;
	int					o_basic_fixed_price;
	int					o_fixed_price_keren;
	int					o_fixed_price_magen;
	int					o_fixed_pr_yahalom;
	char				o_ivf_flag	[1 + 1];
	short				o_refill_period;
	TGenericText75		o_rule_name;
	TGenericFlag		o_in_health_basket;
	TGenericFlag		o_HealthBasketNew;
	TTreatmentCategory	o_treatment_category;
	Tsex				o_sex;
	TGenericFlag		o_needs_29_g;
	Tdel_flg_c			o_del_flg;
	short				o_send_and_use;
	short				o_wait_months;

	int					v_refresh_date;
	int					v_refresh_time;
	int					v_as400_batch_date;
	int					v_as400_batch_time;
	int					v_update_date;
	int					v_update_time;
	int					v_virtual_date;
	int					v_virtual_time;


	// DonR 12Dec2011: This table is not a real-time one; so there was actually a discrepancy between
	// the update date/time written to the table and the update date/time written to tables_update.
	// The fix is to call GetVirtualTimestamp instead of GetDate/GetTime for update_date/time.
	// Note that the AS/400 batch date/time are still based on the download-start date/time when
	// the download comes from RKUNIX; and the refresh date/time no longer needs to be incremented
	// based on the number of rows downloaded, since it is not used for controlling the download
	// to Clicks.
	GetVirtualTimestamp (&v_virtual_date, &v_virtual_time);

	// Calculate other times - see comment above.
	v_as400_batch_date	=  download_start_date;
	v_as400_batch_time	=  download_start_time;
	v_refresh_date		=  (download_start_date > 0) ? download_start_date : GetDate();
	v_refresh_time		=  (download_start_time > 0) ? download_start_time : GetTime();
//	v_refresh_time		+= (download_row_count++ / VIRTUAL_PORTION);



	// read input buffer
	INIT_ERROR_DETECTION_MECHANISM;
	PosInBuff = glbMsgBuffer;

	GET_LONG	(v_rule_number				, Trule_number_len			);
	GET_LONG	(v_largo_code				, 9							);	//Marianna 14Jul2024 User Story #330877
	GET_STRING	(v_extension_largo_co		, Textension_largo_co_len	);
	GET_SHORT	(v_confirm_authority		, Tconfirm_authority__len	);
	GET_HEB_STR (v_conf_auth_desc			, Tconf_auth_desc_len		);
	GET_SHORT	(v_from_age					, Tfrom_age_len				);
	GET_SHORT	(v_to_age					, Tto_age_len				);
	GET_SHORT	(v_basic_price_code			, Tprice_code_len			);
	GET_SHORT	(v_kesef_price_code			, Tprice_code_len			);
	GET_SHORT	(v_zahav_price_code			, Tprice_code_len			);
	GET_SHORT	(v_member_price_prcnt		, Tprice_percent_len		);
	GET_SHORT	(v_keren_price_prcnt		, Tprice_percent_len		);
	GET_SHORT	(v_magen_price_prcnt		, Tprice_percent_len		);
	GET_LONG	(v_max_op_long				, Tmax_op_len				);
	GET_LONG	(v_max_units				, Tmax_units_len			);
	GET_SHORT	(v_max_amount_duratio		, Tmax_amount_duratio_len	);
	GET_SHORT	(v_permission_type			, TPermissionType_len		);
	GET_SHORT	(v_no_presc_sale_flag		, Tno_presc_sale_flag_len	);
	GET_LONG	(v_basic_fixed_price		, Tfixed_price_len			);
	GET_LONG	(v_fixed_price_keren		, Tfixed_price_len			);
	GET_LONG	(v_fixed_price_magen		, Tfixed_price_len			);
	GET_STRING	(v_ivf_flag					, TGenericFlag_len			);
	GET_SHORT	(v_refill_period			, 3							);
	GET_HEB_STR (v_rule_name				, Trule_name_len			);
	GET_SHORT	(v_in_health_basket			, TGenericFlag_len			);
	GET_SHORT	(v_treatment_category		, Ttreatment_category_len	);
	GET_SHORT	(v_sex						, TGenericFlag_len			);
	GET_SHORT	(v_needs_29_g				, TGenericFlag_len			);
	GET_STRING	(v_del_flg					, Tdel_flg_c_len			);
	GET_SHORT	(v_send_and_use				, TGenericFlag_len			);
	GET_SHORT	(v_yahalom_prc_code			, Tprice_code_len			);
	GET_SHORT	(v_percnt_y					, Tprice_percent_len		);
	GET_LONG	(v_fixed_pr_yahalom			, Tfixed_price_len			);
	GET_SHORT	(v_wait_months				, Twait_months_len			);

	
	/* verify all data was read:   */
	ReadInput = PosInBuff - glbMsgBuffer;
	StaticAssert( ReadInput == data_len );

	strip_trailing_spaces (v_rule_name);	// DonR 09Feb2020 to match behavior of ODBC SELECT.


	// Translate AS400 Treatment Category into Unix version.
	i = v_treatment_category;
	switch (i)
	{
		case TREAT_TYPE_AS400_FERTILITY:	v_treatment_category = TREATMENT_TYPE_FERTILITY;	break;

		case TREAT_TYPE_AS400_DIALYSIS:		v_treatment_category = TREATMENT_TYPE_DIALYSIS;		break;

		// Marianna 11May2025: User Story #410137
		case TREAT_TYPE_AS400_PREP:			v_treatment_category = TREATMENT_TYPE_PREP;			break;

		case TREAT_TYPE_AS400_SMOKING:		v_treatment_category = TREATMENT_TYPE_SMOKING;		break;

		// If we get an unrecognized value, set it to GENERAL.
		default:							v_treatment_category = TREATMENT_TYPE_GENERAL;		break;
	}


	// DonR 26Oct2008: Set "old" health-basket flag to 1 or 0 based on the value received
	// from AS400, and move the actual value received to the "new" health-basket flag
	// variable.
	PROCESS_HEALTH_BASKET;



	retcode			= ALL_RETRIES_WERE_USED;
	stop_retrying	= false;
	need_to_read	= true;


	// Perform up to SQL_UPDATE_RETRIES retries to overcome access conflicts.
	for (n_retries = 0;
		 (n_retries < SQL_UPDATE_RETRIES) && (stop_retrying == false);
		 n_retries++)
	{
		// If this isn't the first iteration, sleep for a while.
		if (n_retries > 0)
		{
			sleep (SLEEP_TIME_BETWEEN_RETRIES);
		}

		// Attempt to read the row first. If we get a result other than access conflict, we'll
		// know whether to insert or update. If we have an access conflict, need_to_read will
		// stay true, so we'll know to retry from this point; otherwise we'll skip this section
		// on the retry.
		if (need_to_read == true)
		{
			ExecSQL (	MAIN_DB, AS400SRV_T4074_READ_drug_extn_doc_old_data,
						&o_extension_largo_co	, &o_confirm_authority	, &o_conf_auth_desc		,
						&o_from_age				, &o_to_age				, &o_basic_price_code	,
						&o_kesef_price_code		, &o_zahav_price_code	, &o_member_price_prcnt	,
						&o_keren_price_prcnt	, &o_magen_price_prcnt	, &o_max_op_long		,

						&o_max_units			, &o_max_amount_duratio	, &o_permission_type	,
						&o_no_presc_sale_flag	, &o_basic_fixed_price	, &o_fixed_price_keren	,
						&o_fixed_price_magen	, &o_ivf_flag			, &o_refill_period		,
						&o_rule_name			, &o_in_health_basket	, &o_HealthBasketNew	,

						&o_treatment_category	, &o_sex				, &o_needs_29_g			,
						&o_del_flg				, &o_send_and_use		, &o_yahalom_prc_code	,
						&o_percnt_y				, &o_fixed_pr_yahalom	, &o_wait_months		,
						&v_update_date			, &v_update_time		,

						&v_largo_code			, &v_rule_number		, END_OF_ARG_LIST			);

			// First possibility: the row isn't already in the database, so we need to insert it.
			if (SQLERR_code_cmp (SQLERR_not_found) == MAC_TRUE)
			{
				do_insert = ATTEMPT_INSERT_FIRST;
				need_to_read = false;
			}
			else
			{
				// Second possibility: there was an access conflict, so advance to the next retry iteration.
				if (SQLERR_code_cmp (SQLERR_access_conflict) == MAC_TRUE)
				{
					retcode = ALL_RETRIES_WERE_USED;
					continue;
				}
				else
				{
					// Third possibility: there was a database error, so don't retry - just give up.
					if (SQLERR_error_test ())
					{
						retcode = SQL_ERROR_OCCURED;
						break;
					}

					else
					// Fourth possibility: fetch succeeded, so we'll do a conditional update.
					{
						do_insert = ATTEMPT_UPDATE_FIRST;
						need_to_read = false;
					}	// Read succeeded.
				}	// Not an access conflict.
			}	// Not record-not-found.
		}	// need_to_read is true.


		// If we get here, we've either read the row to update, or else we've determined that
		// the row isn't there and needs to be inserted.
		// DonR 04Jul2010: HOT FIX: Need to have dummy "for" loop here so that HANDLE_UPDATE_SQL_CODE
		// and HANDLE_INSERT_SQL_CODE work properly.
		for (i = 0; i < 1; i++)
		{
		if (do_insert == ATTEMPT_INSERT_FIRST)
		{
			ExecSQL (	MAIN_DB, AS400SRV_T4074_INS_drug_extn_doc,
						&v_rule_number			, &v_largo_code			, &v_extension_largo_co	,
						&v_confirm_authority	, &v_conf_auth_desc		, &v_from_age			,
						&v_to_age				, &v_basic_price_code	, &v_kesef_price_code	,
						&v_zahav_price_code		, &v_member_price_prcnt	, &v_keren_price_prcnt	,
						&v_magen_price_prcnt	, &v_max_op_long		, &v_max_units			,

						&v_max_amount_duratio	, &v_permission_type	, &v_no_presc_sale_flag	,
						&v_basic_fixed_price	, &v_fixed_price_keren	, &v_fixed_price_magen	,
						&v_ivf_flag				, &v_refill_period		, &v_rule_name			,
						&v_in_health_basket		, &v_HealthBasketNew	, &v_treatment_category	,
						&v_sex					, &v_needs_29_g			, &v_del_flg			,

						&v_send_and_use			, &v_yahalom_prc_code	, &v_percnt_y			,
						&v_fixed_pr_yahalom		, &v_wait_months		,

						&v_virtual_date			, &v_virtual_time		,
						&v_refresh_date			, &v_refresh_time		,
						&v_as400_batch_date		, &v_as400_batch_time	, END_OF_ARG_LIST			);

			HANDLE_INSERT_SQL_CODE ("4074", p_connect, &do_insert, &retcode, &stop_retrying);
		}	// Need to insert new drug_extn_doc row.


		// If the row is already present, it needs to be updated.
		else
		{
			// If send_and_use has been changed from true to false,
			// the "nohal" needs to be sent out as a deletion.
			if ((!v_send_and_use) && (v_largo_code != 0) && o_send_and_use)
			{
				v_send_and_use = 1;
				strcpy (v_del_flg, "D");
			}


			// Set update date/time fields based on whether changes have occurred.
			if ((v_confirm_authority		!=  o_confirm_authority		)	||
				(v_from_age					!=  o_from_age				)	||
				(v_to_age					!=  o_to_age				)	||
				(v_basic_price_code			!=  o_basic_price_code		)	||
				(v_kesef_price_code			!=  o_kesef_price_code		)	||
				(v_zahav_price_code			!=  o_zahav_price_code		)	||
				(v_yahalom_prc_code			!=  o_yahalom_prc_code		)	||
				(v_member_price_prcnt		!=  o_member_price_prcnt	)	||
				(v_keren_price_prcnt		!=  o_keren_price_prcnt		)	||
				(v_magen_price_prcnt		!=  o_magen_price_prcnt		)	||
				(v_percnt_y					!=  o_percnt_y				)	||
				(v_send_and_use				!=  o_send_and_use			)	||
				(v_max_amount_duratio		!=  o_max_amount_duratio	)	||
				(v_permission_type			!=  o_permission_type		)	||
				(v_no_presc_sale_flag		!=  o_no_presc_sale_flag	)	||
				(v_refill_period			!=  o_refill_period			)	||
				(v_in_health_basket			!=  o_in_health_basket		)	||
				(v_HealthBasketNew			!=  o_HealthBasketNew		)	||
				(v_treatment_category		!=  o_treatment_category	)	||
				(v_sex						!=  o_sex					)	||
				(v_needs_29_g				!=  o_needs_29_g			)	||
				(v_max_op_long				!=  o_max_op_long			)	||
				(v_max_units				!=  o_max_units				)	||
				(v_basic_fixed_price		!=  o_basic_fixed_price		)	||
				(v_fixed_price_keren		!=  o_fixed_price_keren		)	||
				(v_fixed_price_magen		!=  o_fixed_price_magen		)	||
				(v_fixed_pr_yahalom			!=  o_fixed_pr_yahalom		)	||
				(v_wait_months				!=  o_wait_months			)	||
				strcmp (v_ivf_flag			, o_ivf_flag				)	||
				strcmp (v_del_flg			, o_del_flg					)	||
				strcmp (v_extension_largo_co, o_extension_largo_co		)	||
				strcmp (v_conf_auth_desc	, o_conf_auth_desc			)	||
				strcmp (v_rule_name			, o_rule_name				)		)
			{
				// At least one of the fields has changed.
				v_update_date		= v_virtual_date;	// DonR 12Dec2011: use virtual timestamp.
				v_update_time		= v_virtual_time;	// DonR 12Dec2011: use virtual timestamp.
				something_changed	= 1;
			}


			ExecSQL (	MAIN_DB, AS400SRV_T4074_UPD_drug_extn_doc,
						&v_extension_largo_co,		&v_confirm_authority,
						&v_conf_auth_desc,			&v_from_age,
						&v_to_age,					&v_basic_price_code,
						&v_kesef_price_code,		&v_zahav_price_code,
						&v_member_price_prcnt,		&v_keren_price_prcnt,
						&v_magen_price_prcnt,		&v_max_op_long,
						&v_max_units,				&v_max_amount_duratio,
						&v_permission_type,			&v_no_presc_sale_flag,
						&v_basic_fixed_price,		&v_fixed_price_keren,
						&v_fixed_price_magen,		&v_ivf_flag,
						&v_refill_period,			&v_rule_name,
						&v_in_health_basket,		&v_HealthBasketNew,
						&v_treatment_category,		&v_sex,
						&v_needs_29_g,				&v_del_flg,
						&v_send_and_use,			&v_yahalom_prc_code,
						&v_percnt_y,				&v_fixed_pr_yahalom,
						&v_wait_months,				&v_refresh_date, 
						&v_refresh_time,			&v_as400_batch_date, 
						&v_as400_batch_time,		&v_update_date, 
						&v_update_time,
						&v_largo_code,				&v_rule_number,
						END_OF_ARG_LIST											);

			HANDLE_UPDATE_SQL_CODE ("4074", p_connect, &do_insert, &retcode, &stop_retrying);

		}	// In update mode (as row already exists).
		}	// Dummy loop.

	}	// Retry Loop.


	if (n_retries == SQL_UPDATE_RETRIES)
	{
		WRITE_ACCESS_CONFLICT_TO_LOG ("4074");
	}

	if (retcode == MSG_HANDLER_SUCCEEDED)
	{
		if (do_insert == ATTEMPT_INSERT_FIRST)
		{
			rows_added++;
		}
		else
		{
			if (something_changed)
				rows_updated++;
			else
				rows_refreshed++;
		}
	}


	return retcode;

}	// end of msg 4074 handler



// -------------------------------------------------------------------------
//
//       Post-processor for message 4074 - logically delete DRUG_EXTN_DOC
//			rows that are no longer in the AS/400 full-table download.
//
// -------------------------------------------------------------------------
static int postprocess_4074 (TConnection	*p_connect)
{
	// local variables
	bool	stop_retrying;	// if true - return from function
	int		rows_deleted	= 0;
	int		retcode;		// code returned from function
	int		n_retries;		// retrying counter
	int		db_changed_flg;

	// DB variables
	int				v_rule_number;
	Tlargo_code		v_largo_code;
	Tdel_flg_c		v_del_flg;
	short			v_send_and_use;
	int				v_as400_batch_date;
	int				v_as400_batch_time;
	int				v_update_date;
	int				v_update_time;


	// Initialize variables.
	v_as400_batch_date	= download_start_date;
	v_as400_batch_time	= download_start_time;


	retcode			= ALL_RETRIES_WERE_USED;
	stop_retrying	= false;


	DeclareCursorInto (	MAIN_DB, AS400SRV_T4074PP_stale_drug_extn_doc_cur,
						&v_rule_number,			&v_as400_batch_date,
						&v_as400_batch_date,	&v_as400_batch_time,
						END_OF_ARG_LIST										);

	OpenCursor (	MAIN_DB, AS400SRV_T4074PP_stale_drug_extn_doc_cur	);

	if (SQLERR_error_test ())
	{
		retcode = SQL_ERROR_OCCURED;
//		break;
	}

	while (1)
	{
		// DonR 14Mar2010: Got rid of redundant INTO clause - the destination variable is specified in the
		// cursor declaration.
		FetchCursor (	MAIN_DB, AS400SRV_T4074PP_stale_drug_extn_doc_cur	);

		if (SQLERR_code_cmp (SQLERR_end_of_fetch) == MAC_TRUE)
		{
			retcode = MSG_HANDLER_SUCCEEDED;	// In case there is nothing that needs to be deleted.
			break;
		}
		else
		{
			if (SQLERR_error_test ())
			{
				retcode = SQL_ERROR_OCCURED;
				break;
			}
		}

		// Calculate virtual time. Note that for this download the time is based
		// on the real time, adjusted only to ensure that we don't have too many
		// rules with the exact same as-of time.
		v_update_date		= download_start_date;
		v_update_time		= download_start_time + (download_row_count++ / VIRTUAL_PORTION);

		ExecSQL (	MAIN_DB, AS400SRV_T4074PP_UPD_drug_extn_doc_logically_delete,
					&v_update_date, &v_update_time, END_OF_ARG_LIST					);

			HANDLE_DELETE_SQL_CODE( "postprocess_4074", p_connect, &retcode, &db_changed_flg );

		rows_deleted++;
	}	// Cursor-scrolling loop.

	CloseCursor (	MAIN_DB, AS400SRV_T4074PP_stale_drug_extn_doc_cur	);


	// DonR 13Nov2018 CR #15919: Record current table size.
	REMEMBER_ISOLATION;
	SET_ISOLATION_DIRTY;

	ExecSQL (	MAIN_DB, AS400SRV_T4074PP_READ_drug_extn_doc_table_size,
				&total_rows, END_OF_ARG_LIST								);

	ExecSQL (	MAIN_DB, AS400SRV_T4074PP_READ_drug_extn_doc_active_size,
				&active_rows, END_OF_ARG_LIST								);

	RESTORE_ISOLATION;
	// DonR 13Nov2018 CR #15919 end.


	GerrLogMini (GerrId,
				 "Postprocess 4074: %d rows logically deleted, retcode = %d.",
				 rows_deleted, retcode);

	return retcode;

}	// End of msg 4074 postprocessor.



/* << -------------------------------------------------------- >> */
/*                                                                */
/*       handler for message 2075 - upd/ins to SALE			      */
/*                                                                */
/* << -------------------------------------------------------- >> */
static int message_2075_handler (TConnection	*p_connect,
								 int			data_len,
								 bool			do_insert)
{
	// local variables
	char	*PosInBuff;		/* current pos in data buffer	    */
	bool	stop_retrying;	/* if true - return from function   */
	bool	deleted_a_row;
	int		ReadInput;		/* amount of Bytesread from buffer  */
	int		retcode;		/* code returned from function	    */
	int		n_retries;		/* retrying counter					*/
	int		i;

	// DB variables
	char		v_sale_name [76];
	int			v_sale_number;
	int			v_start_date;
	int			v_end_date;
	int			v_min_op_to_buy;
	int			v_op_to_receive;
	int			v_min_purchase_amt;
	int			v_tav_knia_amt;
	short		v_sale_owner;
	short		v_sale_audience;
	short		v_sale_type;
	short		v_purchase_disc;

	int			sysdate;
	int			systime;


	// Initialization
	sysdate = GetDate ();
	systime = GetTime ();


	// Read input buffer
	INIT_ERROR_DETECTION_MECHANISM;
	PosInBuff = glbMsgBuffer;

	v_sale_number			= get_long	(&PosInBuff, 6); RETURN_ON_ERROR;
	get_string (&PosInBuff, v_sale_name, 75);
	v_sale_owner			= get_short	(&PosInBuff, 1); RETURN_ON_ERROR;
	v_sale_audience			= get_short	(&PosInBuff, 1); RETURN_ON_ERROR;
	v_start_date			= get_long	(&PosInBuff, 8); RETURN_ON_ERROR;
	v_end_date				= get_long	(&PosInBuff, 8); RETURN_ON_ERROR;
	v_sale_type				= get_short	(&PosInBuff, 2); RETURN_ON_ERROR;
	v_min_op_to_buy			= get_long	(&PosInBuff, 5); RETURN_ON_ERROR;
	v_op_to_receive			= get_long	(&PosInBuff, 5); RETURN_ON_ERROR;
	v_min_purchase_amt		= get_long	(&PosInBuff, 9); RETURN_ON_ERROR;
	v_purchase_disc			= get_short	(&PosInBuff, 5); RETURN_ON_ERROR;
	v_tav_knia_amt			= get_long	(&PosInBuff, 9); RETURN_ON_ERROR;

	
	/* verify all data was read:   */
	ReadInput = PosInBuff - glbMsgBuffer;
	StaticAssert( ReadInput == data_len );


	switch_to_win_heb ((unsigned char *)v_sale_name);

	/* << -------------------------------------------------------- >> */
	/*	   up to SQL_UPDATE_RETRIES retries will be made			  */
	/*		    to overcome access conflict							  */
	/* << -------------------------------------------------------- >> */
	retcode = ALL_RETRIES_WERE_USED;
	stop_retrying = false;

	for (n_retries = 0;
		 (n_retries < SQL_UPDATE_RETRIES) && (stop_retrying == false);
		 n_retries++)
	{
		// sleep for a while
		if (n_retries > 0)
		{
			sleep (SLEEP_TIME_BETWEEN_RETRIES);
		}

		// try to insert/update new rec with the order depending on "do_insert":
		for (i = 0; i < 2; i++)
		{
			if (do_insert == ATTEMPT_INSERT_FIRST)
			{
				ExecSQL (	MAIN_DB, AS400SRV_T2075_INS_sale,
							&v_sale_number,			&v_sale_name,
							&v_sale_owner,			&v_sale_audience,
							&v_start_date,			&v_end_date,
							&v_sale_type,			&v_min_op_to_buy,
							&v_op_to_receive,		&v_min_purchase_amt,
							&v_purchase_disc,		&v_tav_knia_amt,
							&sysdate,				&systime,
							&ROW_NOT_DELETED,		END_OF_ARG_LIST			);

				HANDLE_INSERT_SQL_CODE ("2075", p_connect, &do_insert, &retcode, &stop_retrying);
			}
			else	// Update existing row.
			{
				ExecSQL (	MAIN_DB, AS400SRV_T2075_UPD_sale,
							&v_sale_name,			&v_sale_owner,
							&v_sale_audience,		&v_start_date,
							&v_end_date,			&v_sale_type,
							&v_min_op_to_buy,		&v_op_to_receive,
							&v_min_purchase_amt,	&v_purchase_disc,
							&v_tav_knia_amt,		&sysdate,
							&systime,				&ROW_NOT_DELETED,
							&v_sale_number,			END_OF_ARG_LIST		);

				HANDLE_UPDATE_SQL_CODE ("2075", p_connect, &do_insert, &retcode, &stop_retrying);
			}

		}	/* for( i... */

	}	/* for( n_retries... */


	// Update counters at this point - since we'll be altering retocode and do_insert below.
	if (retcode == MSG_HANDLER_SUCCEEDED)
	{
		if (do_insert == ATTEMPT_INSERT_FIRST)
			rows_added++;
		else
			rows_updated++;
	}


	if( n_retries == SQL_UPDATE_RETRIES )
	{
		WRITE_ACCESS_CONFLICT_TO_LOG ("2075");
	}


	return retcode;

}	// end of msg 2075 handler



/* << -------------------------------------------------------- >> */
/*                                                                */
/*       handler for message 2076 - upd/ins to SALE_FIXED_PRICE   */
/*                                                                */
/* << -------------------------------------------------------- >> */
static int message_2076_handler (TConnection	*p_connect,
								 int			data_len,
								 bool			do_insert)
{
	// local variables
	char	*PosInBuff;		/* current pos in data buffer	    */
	bool	stop_retrying;	/* if true - return from function   */
	bool	deleted_a_row;
	int		ReadInput;		/* amount of Bytesread from buffer  */
	int		retcode;		/* code returned from function	    */
	int		n_retries;		/* retrying counter					*/
	int		i;

	// DB variables
	int			v_sale_number;
	int			v_largo_code;
	int			v_sale_price;

	int			sysdate;
	int			systime;


	// Initialization
	sysdate = GetDate ();
	systime = GetTime ();


	// Read input buffer
	INIT_ERROR_DETECTION_MECHANISM;
	PosInBuff = glbMsgBuffer;

	v_sale_number			= get_long	(&PosInBuff, 6); RETURN_ON_ERROR;
	v_largo_code			= get_long	(&PosInBuff, 9); RETURN_ON_ERROR;	//Marianna 14Jul2024 User Story #330877
	v_sale_price			= get_long	(&PosInBuff, 9); RETURN_ON_ERROR;

	
	/* verify all data was read:   */
	ReadInput = PosInBuff - glbMsgBuffer;
	StaticAssert( ReadInput == data_len );


	/* << -------------------------------------------------------- >> */
	/*	   up to SQL_UPDATE_RETRIES retries will be made			  */
	/*		    to overcome access conflict							  */
	/* << -------------------------------------------------------- >> */
	retcode = ALL_RETRIES_WERE_USED;
	stop_retrying = false;

	for (n_retries = 0;
		 (n_retries < SQL_UPDATE_RETRIES) && (stop_retrying == false);
		 n_retries++)
	{
		// sleep for a while
		if (n_retries > 0)
		{
			sleep (SLEEP_TIME_BETWEEN_RETRIES);
		}

		// try to insert/update new rec with the order depending on "do_insert":
		for (i = 0; i < 2; i++)
		{
			if (do_insert == ATTEMPT_INSERT_FIRST)
			{
				ExecSQL (	MAIN_DB, AS400SRV_T2076_INS_sale_fixed_price,
							&v_sale_number,		&v_largo_code,
							&v_sale_price,		&sysdate,
							&systime,			&ROW_NOT_DELETED,
							END_OF_ARG_LIST									);

				HANDLE_INSERT_SQL_CODE ("2076", p_connect, &do_insert, &retcode, &stop_retrying);
			}
			else	// Update existing row.
			{
				ExecSQL (	MAIN_DB, AS400SRV_T2076_UPD_sale_fixed_price,
							&v_sale_price,			&sysdate,
							&systime,				&ROW_NOT_DELETED,
							&v_sale_number,			&v_largo_code,
							END_OF_ARG_LIST									);

				HANDLE_UPDATE_SQL_CODE ("2076", p_connect, &do_insert, &retcode, &stop_retrying);
			}

		}	/* for( i... */

	}	/* for( n_retries... */


	// Update counters at this point - since we'll be altering retocode and do_insert below.
	if (retcode == MSG_HANDLER_SUCCEEDED)
	{
		if (do_insert == ATTEMPT_INSERT_FIRST)
			rows_added++;
		else
			rows_updated++;
	}


	if( n_retries == SQL_UPDATE_RETRIES )
	{
		WRITE_ACCESS_CONFLICT_TO_LOG ("2076");
	}


	return retcode;

}	// end of msg 2076 handler



/* << -------------------------------------------------------- >> */
/*                                                                */
/*       handler for message 2077 - upd/ins to SALE_BONUS_RECV    */
/*                                                                */
/* << -------------------------------------------------------- >> */
static int message_2077_handler (TConnection	*p_connect,
								 int			data_len,
								 bool			do_insert)
{
	// local variables
	char	*PosInBuff;		/* current pos in data buffer	    */
	bool	stop_retrying;	/* if true - return from function   */
	bool	deleted_a_row;
	int		ReadInput;		/* amount of Bytesread from buffer  */
	int		retcode;		/* code returned from function	    */
	int		n_retries;		/* retrying counter					*/
	int		i;

	// DB variables
	int			v_sale_number;
	int			v_largo_code;
	int			v_op_to_receive;
	int			v_sale_price;
	short		v_discount_percent;

	int			sysdate;
	int			systime;


	// Initialization
	sysdate = GetDate ();
	systime = GetTime ();


	// Read input buffer
	INIT_ERROR_DETECTION_MECHANISM;
	PosInBuff = glbMsgBuffer;

	v_sale_number			= get_long	(&PosInBuff, 6); RETURN_ON_ERROR;
	v_largo_code			= get_long	(&PosInBuff, 9); RETURN_ON_ERROR;	//Marianna 14Jul2024 User Story #330877
	v_op_to_receive			= get_long	(&PosInBuff, 5); RETURN_ON_ERROR;
	v_discount_percent		= get_long	(&PosInBuff, 5); RETURN_ON_ERROR;
	v_sale_price			= get_long	(&PosInBuff, 9); RETURN_ON_ERROR;

	
	/* verify all data was read:   */
	ReadInput = PosInBuff - glbMsgBuffer;
	StaticAssert( ReadInput == data_len );


	/* << -------------------------------------------------------- >> */
	/*	   up to SQL_UPDATE_RETRIES retries will be made			  */
	/*		    to overcome access conflict							  */
	/* << -------------------------------------------------------- >> */
	retcode = ALL_RETRIES_WERE_USED;
	stop_retrying = false;

	for (n_retries = 0;
		 (n_retries < SQL_UPDATE_RETRIES) && (stop_retrying == false);
		 n_retries++)
	{
		// sleep for a while
		if (n_retries > 0)
		{
			sleep (SLEEP_TIME_BETWEEN_RETRIES);
		}

		// try to insert/update new rec with the order depending on "do_insert":
		for (i = 0; i < 2; i++)
		{
			if (do_insert == ATTEMPT_INSERT_FIRST)
			{
				ExecSQL (	MAIN_DB, AS400SRV_T2077_INS_sale_bonus_recv,
							&v_sale_number,			&v_largo_code,
							&v_op_to_receive,		&v_discount_percent,
							&v_sale_price,			&sysdate,
							&systime,				&ROW_NOT_DELETED,
							END_OF_ARG_LIST									);

				HANDLE_INSERT_SQL_CODE ("2077", p_connect, &do_insert, &retcode, &stop_retrying);
			}
			else	// Update existing row.
			{
				ExecSQL (	MAIN_DB, AS400SRV_T2077_UPD_sale_bonus_recv,
							&v_op_to_receive,		&v_discount_percent,
							&v_sale_price,			&sysdate,
							&systime,				&ROW_NOT_DELETED,
							&v_sale_number,			&v_largo_code,
							END_OF_ARG_LIST									);

				HANDLE_UPDATE_SQL_CODE ("2077", p_connect, &do_insert, &retcode, &stop_retrying);
			}

		}	/* for( i... */

	}	/* for( n_retries... */


	// Update counters at this point - since we'll be altering retocode and do_insert below.
	if (retcode == MSG_HANDLER_SUCCEEDED)
	{
		if (do_insert == ATTEMPT_INSERT_FIRST)
			rows_added++;
		else
			rows_updated++;
	}


	if( n_retries == SQL_UPDATE_RETRIES )
	{
		WRITE_ACCESS_CONFLICT_TO_LOG ("2077");
	}


	return retcode;

}	// end of msg 2077 handler



/* << -------------------------------------------------------- >> */
/*                                                                */
/*       handler for message 2078 - upd/ins to SALE_TARGET		  */
/*                                                                */
/* << -------------------------------------------------------- >> */
static int message_2078_handler (TConnection	*p_connect,
								 int			data_len,
								 bool			do_insert)
{
	// local variables
	char	*PosInBuff;		/* current pos in data buffer	    */
	bool	stop_retrying;	/* if true - return from function   */
	bool	deleted_a_row;
	int		ReadInput;		/* amount of Bytesread from buffer  */
	int		retcode;		/* code returned from function	    */
	int		n_retries;		/* retrying counter					*/
	int		i;

	// DB variables
	int			v_sale_number;
	int			v_target_op;
	int			v_max_op;
	short		v_pharmacy_type;
	short		v_pharmacy_size;

	int			sysdate;
	int			systime;


	// Initialization
	sysdate = GetDate ();
	systime = GetTime ();


	// Read input buffer
	INIT_ERROR_DETECTION_MECHANISM;
	PosInBuff = glbMsgBuffer;

	v_sale_number		= get_long	(&PosInBuff, 6); RETURN_ON_ERROR;
	v_pharmacy_type		= get_short	(&PosInBuff, 2); RETURN_ON_ERROR;
	v_pharmacy_size		= get_short	(&PosInBuff, 1); RETURN_ON_ERROR;
	v_target_op			= get_long	(&PosInBuff, 9); RETURN_ON_ERROR;
	v_max_op			= get_long	(&PosInBuff, 9); RETURN_ON_ERROR;

	
	/* verify all data was read:   */
	ReadInput = PosInBuff - glbMsgBuffer;
	StaticAssert( ReadInput == data_len );


	/* << -------------------------------------------------------- >> */
	/*	   up to SQL_UPDATE_RETRIES retries will be made			  */
	/*		    to overcome access conflict							  */
	/* << -------------------------------------------------------- >> */
	retcode = ALL_RETRIES_WERE_USED;
	stop_retrying = false;

	for (n_retries = 0;
		 (n_retries < SQL_UPDATE_RETRIES) && (stop_retrying == false);
		 n_retries++)
	{
		// sleep for a while
		if (n_retries > 0)
		{
			sleep (SLEEP_TIME_BETWEEN_RETRIES);
		}

		// try to insert/update new rec with the order depending on "do_insert":
		for (i = 0; i < 2; i++)
		{
			if (do_insert == ATTEMPT_INSERT_FIRST)
			{
				ExecSQL (	MAIN_DB, AS400SRV_T2078_INS_sale_target,
							&v_sale_number,			&v_pharmacy_type,
							&v_pharmacy_size,		&v_target_op,
							&v_max_op,				&sysdate,
							&systime,				&ROW_NOT_DELETED,
							END_OF_ARG_LIST								);

				HANDLE_INSERT_SQL_CODE ("2078", p_connect, &do_insert, &retcode, &stop_retrying);
			}
			else	// Update existing row.
			{
				ExecSQL (	MAIN_DB, AS400SRV_T2078_UPD_sale_target,
							&v_target_op,		&v_max_op,
							&sysdate,			&systime,
							&ROW_NOT_DELETED,
							&v_sale_number,		&v_pharmacy_type,
							&v_pharmacy_size,	END_OF_ARG_LIST			);

				HANDLE_UPDATE_SQL_CODE ("2078", p_connect, &do_insert, &retcode, &stop_retrying);
			}

		}	/* for( i... */

	}	/* for( n_retries... */


	// Update counters at this point - since we'll be altering retocode and do_insert below.
	if (retcode == MSG_HANDLER_SUCCEEDED)
	{
		if (do_insert == ATTEMPT_INSERT_FIRST)
			rows_added++;
		else
			rows_updated++;
	}


	if( n_retries == SQL_UPDATE_RETRIES )
	{
		WRITE_ACCESS_CONFLICT_TO_LOG ("2078");
	}


	return retcode;

}	// end of msg 2078 handler



/* << -------------------------------------------------------- >> */
/*                                                                */
/*       handler for message 2079 - upd/ins to SALE_PHARMACY	  */
/*                                                                */
/* << -------------------------------------------------------- >> */
static int message_2079_handler (TConnection	*p_connect,
								 int			data_len,
								 bool			do_insert)
{
	// local variables
	char	*PosInBuff;		/* current pos in data buffer	    */
	bool	stop_retrying;	/* if true - return from function   */
	bool	deleted_a_row;
	int		ReadInput;		/* amount of Bytesread from buffer  */
	int		retcode;		/* code returned from function	    */
	int		n_retries;		/* retrying counter					*/
	int		i;

	// DB variables
	int			v_sale_number;
	int			v_sale_pharmacy;

	int			sysdate;
	int			systime;


	// Initialization
	sysdate = GetDate ();
	systime = GetTime ();


	// Read input buffer
	INIT_ERROR_DETECTION_MECHANISM;
	PosInBuff = glbMsgBuffer;

	v_sale_number		= get_long	(&PosInBuff, 6); RETURN_ON_ERROR;
	v_sale_pharmacy		= get_long	(&PosInBuff, 7); RETURN_ON_ERROR;

	
	/* verify all data was read:   */
	ReadInput = PosInBuff - glbMsgBuffer;
	StaticAssert( ReadInput == data_len );


	/* << -------------------------------------------------------- >> */
	/*	   up to SQL_UPDATE_RETRIES retries will be made			  */
	/*		    to overcome access conflict							  */
	/* << -------------------------------------------------------- >> */
	retcode = ALL_RETRIES_WERE_USED;
	stop_retrying = false;

	for (n_retries = 0;
		 (n_retries < SQL_UPDATE_RETRIES) && (stop_retrying == false);
		 n_retries++)
	{
		// sleep for a while
		if (n_retries > 0)
		{
			sleep (SLEEP_TIME_BETWEEN_RETRIES);
		}

		// try to insert/update new rec with the order depending on "do_insert":
		for (i = 0; i < 2; i++)
		{
			if (do_insert == ATTEMPT_INSERT_FIRST)
			{
				ExecSQL (	MAIN_DB, AS400SRV_T2079_INS_sale_pharmacy,
							&v_sale_number,		&v_sale_pharmacy,
							&sysdate,			&systime,
							&ROW_NOT_DELETED,	END_OF_ARG_LIST			);

				HANDLE_INSERT_SQL_CODE ("2079", p_connect, &do_insert, &retcode, &stop_retrying);
			}
			else	// Update existing row.
			{
				ExecSQL (	MAIN_DB, AS400SRV_T2079_UPD_sale_pharmacy,
							&sysdate,			&systime,
							&ROW_NOT_DELETED,
							&v_sale_number,		&v_sale_pharmacy,
							END_OF_ARG_LIST								);

				HANDLE_UPDATE_SQL_CODE ("2079", p_connect, &do_insert, &retcode, &stop_retrying);
			}

		}	/* for( i... */

	}	/* for( n_retries... */


	// Update counters at this point - since we'll be altering retocode and do_insert below.
	if (retcode == MSG_HANDLER_SUCCEEDED)
	{
		if (do_insert == ATTEMPT_INSERT_FIRST)
			rows_added++;
		else
			rows_updated++;
	}


	if( n_retries == SQL_UPDATE_RETRIES )
	{
		WRITE_ACCESS_CONFLICT_TO_LOG ("2079");
	}


	return retcode;

}	// end of msg 2079 handler


// Marianna 20Jul2023 User Story #456129
#if 0
/* << -------------------------------------------------------- >> */
/*                                                                */
/*       handler for message 2080 - upd/ins to PHARMDRUGNOTES	  */
/* << -------------------------------------------------------- >> */
static int message_2080_handler(
				TConnection	*p_connect,
				int		data_len,
				bool	do_insert,
				TShmUpdate	*shm_table_to_update
				)
{
	// local variables
	char	*PosInBuff;		/* current pos in data buffer	    */
	int		ReadInput;		/* amount of Bytesread from buffer  */
	int		retcode;		/* code returned from function	    */
	bool	stop_retrying;	/* if true - return from function   */
	int		n_retries;		/* retrying counter		    */
	int		i;

	// DB variables
	Tgnrldrugnotetype	v_gnrldrugnotetype;
	Tgnrldrugnotecode	v_gnrldrugnotecode;
	Tgnrldrugnote		v_gnrldrugnote;
	short				v_gdn_category;
	short				v_gdn_seq_num;
	short				v_gdn_connect_type;
	char				v_gdn_connect_desc	[ 25 + 1];
	short				v_gdn_severity;
	Tdel_flg_c			v_del_flg;
	int					sysdate,	systime;
	Tgnrldrugnote		old_gnrldrugnote;
	Tdel_flg_c			old_del_flg;


	// DonR 12Dec2011: This table is not a real-time one; so there was actually a discrepancy between
	// the update date/time written to the table and the update date/time written to tables_update.
	// The fix is to call GetVirtualTimestamp instead of GetDate/GetTime.
//	sysdate = GetDate();
//	systime = GetTime();
	GetVirtualTimestamp (&sysdate, &systime);

	// read input buffer
	INIT_ERROR_DETECTION_MECHANISM;
	PosInBuff = glbMsgBuffer;

	/* v_gnrldrugnotetype */	  get_string (&PosInBuff, v_gnrldrugnotetype,	Tgnrldrugnotetype_len	);
	v_gnrldrugnotecode			= get_long   (&PosInBuff, Tgnrldrugnotecode_len	);	RETURN_ON_ERROR;
	/* v_gnrldrugnote */		  get_string (&PosInBuff, v_gnrldrugnote,		Tgnrldrugnote_len		);
	v_gdn_category				= get_short  (&PosInBuff, Tgdn_category_len		);	RETURN_ON_ERROR;
	v_gdn_seq_num				= get_short  (&PosInBuff, Tgdn_seq_num_len		);	RETURN_ON_ERROR;
	v_gdn_connect_type			= get_short  (&PosInBuff, Tgdn_connect_type_len	);	RETURN_ON_ERROR;
	/* v_gdn_connect_desc */	  get_string (&PosInBuff, v_gdn_connect_desc,	Tgdn_connect_desc_len	);
	v_gdn_severity				= get_short  (&PosInBuff, Tgdn_severity_len		);	RETURN_ON_ERROR;
	/* v_del_flg */				  get_string (&PosInBuff, v_del_flg,				Tdel_flg_c_len			);


	/* verify all data was read:   */
	ReadInput = PosInBuff - glbMsgBuffer;
	StaticAssert( ReadInput == data_len );


	switch_to_win_heb ((unsigned char *)v_gnrldrugnote);
	switch_to_win_heb ((unsigned char *)v_gdn_connect_desc);


	*shm_table_to_update = PHARMDRUGNOTES;
//	v_sysdate = sysdate ;
//	v_systime = systime ;


/* << -------------------------------------------------------- >> */
/*	   up to SQL_UPDATE_RETRIES retries will be made			  */
/*		    to overcome access conflict							  */
/* << -------------------------------------------------------- >> */
  retcode = ALL_RETRIES_WERE_USED;
  stop_retrying = false;

  for(n_retries = 0;
      ( n_retries < SQL_UPDATE_RETRIES ) && ( stop_retrying == false );
      n_retries++ )
    {
      /* sleep for a while:    */
      if( n_retries )
      {
          sleep( SLEEP_TIME_BETWEEN_RETRIES );
      }

      /*
       *  try to insert/update new rec with the order depending on "do_insert":
       */
      for( i = 0;  i < 2;  i++ )
	{
	  if( do_insert == ATTEMPT_INSERT_FIRST )
	    {
			ExecSQL (	MAIN_DB, AS400SRV_T2080_INS_PharmDrugNotes,
						&v_gnrldrugnotetype,		&v_gnrldrugnotecode,
						&v_gnrldrugnote,			&v_gdn_category,
						&v_gdn_seq_num,				&v_gdn_connect_type,
						&v_gdn_connect_desc,		&v_gdn_severity,
						&sysdate,					&systime,
						&UPDATED_BY_AS400,			&v_del_flg,
						END_OF_ARG_LIST										);

         HANDLE_INSERT_SQL_CODE( "2080",p_connect,  &do_insert,   &retcode,  &stop_retrying );

	    }
	  else
	    {
			ExecSQL (	MAIN_DB, AS400SRV_T2080_UPD_PharmDrugNotes,
						&v_gnrldrugnote,			&v_gdn_category,
						&v_gdn_seq_num,				&v_gdn_connect_type,
						&v_gdn_connect_desc,		&v_gdn_severity,
						&sysdate,					&systime,
						&UPDATED_BY_AS400,			&v_del_flg,
						&v_gnrldrugnotetype,		&v_gnrldrugnotecode,
						END_OF_ARG_LIST										);

          HANDLE_UPDATE_SQL_CODE( "2080",p_connect,  &do_insert,  &retcode,   &stop_retrying );
	    }

	}/* for( i... */

    }/* for( n_retries... */

  if( n_retries == SQL_UPDATE_RETRIES )
  {
     WRITE_ACCESS_CONFLICT_TO_LOG( "2080" );
  }


  if (retcode == MSG_HANDLER_SUCCEEDED)
  {
	  if (do_insert == ATTEMPT_INSERT_FIRST)
		  rows_added++;
	  else
		  rows_updated++;
  }
  return retcode;
} /* end of msg 2080 handler */
#endif

// Marianna 20Jul2023 User Story #456129
/* << -------------------------------------------------------------- >> */
/*                                                                		*/
/*       handler for message 2080/4080 - upd/ins to PharmDrugNotes 		*/
/*                                                                		*/
/* << -------------------------------------------------------------- >> */
static int data_2080_handler (TConnection	*p_connect,
							  int			data_len)
{
/* << -------------------------------------------------------- >> */
/*			  local variables			  */
/* << -------------------------------------------------------- >> */
  char      *PosInBuff;		/* current pos in data buffer	    */
  int		ReadInput;		/* amount of Bytesread from buffer  */
  int		retcode;		/* code returned from function	    */
  bool		stop_retrying;		/* if true - return from function   */
  bool		do_insert = ATTEMPT_UPDATE_FIRST;
  int		n_retries;		/* retrying counter		    */
  int		i;
  short		InFullTableMode;
  int		v_as400_batch_date;
  int		v_as400_batch_time;

	/* << -------------------------------------------------------- >> */
	/*			    DB variables			  */
	/* << -------------------------------------------------------- >> */
	Tgnrldrugnotetype	v_gnrldrugnotetype;
	Tgnrldrugnotecode	v_gnrldrugnotecode;
	Tgnrldrugnote		v_gnrldrugnote;
	short				v_gdn_category;
	short				v_gdn_seq_num;
	short				v_gdn_connect_type;
	char				v_gdn_connect_desc	[ 25 + 1];
	short				v_gdn_severity;
	Tdel_flg_c			v_del_flg;
	int					sysdate;
	int					systime;
	Tgnrldrugnote		old_gnrldrugnote;
	Tdel_flg_c			old_del_flg;


	InFullTableMode		= (download_start_date > 0);
	
	GetVirtualTimestamp (&sysdate, &systime); //This timestamp controls how stuff is sent to the pharmacy

	v_as400_batch_date = (InFullTableMode) ? download_start_date : GetDate();
	v_as400_batch_time = (InFullTableMode) ? download_start_time : GetTime();

/* << -------------------------------------------------------- >> */
/*			read input buffer			  */
/* << -------------------------------------------------------- >> */
  INIT_ERROR_DETECTION_MECHANISM;
  PosInBuff = glbMsgBuffer;

	/* v_gnrldrugnotetype */	  get_string (&PosInBuff, v_gnrldrugnotetype,	Tgnrldrugnotetype_len	);
	v_gnrldrugnotecode			= get_long   (&PosInBuff, Tgnrldrugnotecode_len	);	RETURN_ON_ERROR;
	/* v_gnrldrugnote */		  get_string (&PosInBuff, v_gnrldrugnote,		Tgnrldrugnote_len		);
	v_gdn_category				= get_short  (&PosInBuff, Tgdn_category_len		);	RETURN_ON_ERROR;
	v_gdn_seq_num				= get_short  (&PosInBuff, Tgdn_seq_num_len		);	RETURN_ON_ERROR;
	v_gdn_connect_type			= get_short  (&PosInBuff, Tgdn_connect_type_len	);	RETURN_ON_ERROR;
	/* v_gdn_connect_desc */	  get_string (&PosInBuff, v_gdn_connect_desc,	Tgdn_connect_desc_len	);
	v_gdn_severity				= get_short  (&PosInBuff, Tgdn_severity_len		);	RETURN_ON_ERROR;
	/* v_del_flg */				  get_string (&PosInBuff, v_del_flg,				Tdel_flg_c_len			);
	

/* << -------------------------------------------------------- >> */
/*			    debug messages			  */
/* << -------------------------------------------------------- >> */

  /*
   *   verify all data was read:
   */
  ReadInput = PosInBuff - glbMsgBuffer;
  StaticAssert( ReadInput == data_len );

	switch_to_win_heb ((unsigned char *)v_gnrldrugnote);
	switch_to_win_heb ((unsigned char *)v_gdn_connect_desc);


/* << -------------------------------------------------------- >> */
/*	   up to SQL_UPDATE_RETRIES retries will be made	  */
/*		    to overcome access conflict			  */
/* << -------------------------------------------------------- >> */
	retcode = ALL_RETRIES_WERE_USED;
	stop_retrying = false;

	for (n_retries = 0; (n_retries < SQL_UPDATE_RETRIES) && (stop_retrying == false); n_retries++)
	{
		// Sleep for a while:
		if (n_retries)
		{
			sleep (SLEEP_TIME_BETWEEN_RETRIES);
		}

		// Try to insert/update new rec with the order depending on "do_insert".
		for (i = 0;  i < 2;  i++)
		{
			if (do_insert == ATTEMPT_INSERT_FIRST)
			{
				ExecSQL (	MAIN_DB, AS400SRV_T2080_INS_PharmDrugNotes,
						&v_gnrldrugnotetype,		&v_gnrldrugnotecode,
						&v_gnrldrugnote,			&v_gdn_category,
						&v_gdn_seq_num,				&v_gdn_connect_type,
						&v_gdn_connect_desc,		&v_gdn_severity,
						&sysdate,					&systime,
						&UPDATED_BY_AS400,			&v_del_flg,
						&v_as400_batch_date,		&v_as400_batch_time,
						END_OF_ARG_LIST										);


				HANDLE_INSERT_SQL_CODE( "2080",p_connect,  &do_insert,   &retcode,  &stop_retrying );

			}
			else
			{
				ExecSQL (	MAIN_DB, AS400SRV_T2080_UPD_PharmDrugNotes,
						&v_gnrldrugnote,			&v_gdn_category,
						&v_gdn_seq_num,				&v_gdn_connect_type,
						&v_gdn_connect_desc,		&v_gdn_severity,
						&sysdate,					&systime,
						&UPDATED_BY_AS400,			&v_del_flg,
						&v_as400_batch_date,		&v_as400_batch_time,
						&v_gnrldrugnotetype,		&v_gnrldrugnotecode,
						END_OF_ARG_LIST										);

				
				HANDLE_UPDATE_SQL_CODE( "2080",p_connect,  &do_insert,  &retcode,   &stop_retrying );

			}	// In update mode.

		}	// for ( i...

	}	/* for( n_retries... */


	if (n_retries == SQL_UPDATE_RETRIES)
	{
		WRITE_ACCESS_CONFLICT_TO_LOG ( "2080" );
	}

	if (retcode == MSG_HANDLER_SUCCEEDED)
	{
		if (do_insert == ATTEMPT_INSERT_FIRST)
			rows_added++;
		else
			rows_updated++;
	}

	return retcode;
} /* end of msg 2080_4080 handler */


// Marianna 23Jul2023 User Story #456129
// -------------------------------------------------------------------------
//
//       Post-processor for message 4080 - Logically delete PharmDrugNotes 
//			rows that are no longer in the AS/400 full-table download.
//
// -------------------------------------------------------------------------
static int postprocess_4080 (TConnection *p_connect)
{
	// local variables
	bool	stop_retrying;	// if true - return from function
	int		rows_deleted	= 0;
	int		retcode;		// code returned from function
	int		db_changed_flg;

	// DB variables
	Tgnrldrugnotetype	v_gnrldrugnotetype;
	Tgnrldrugnotecode	v_gnrldrugnotecode;
	
	int		v_as400_batch_date;
	int		v_as400_batch_time;
	int		v_update_date;
	int		v_update_time;


	// Initialize variables.
	v_as400_batch_date	= download_start_date;
	v_as400_batch_time	= download_start_time;


	retcode			= ALL_RETRIES_WERE_USED;
	stop_retrying	= false;


	DeclareCursorInto (	MAIN_DB, AS400SRV_T4080PP_postproc_cur,
						&v_gnrldrugnotetype,	&v_gnrldrugnotecode,			
						&v_as400_batch_date,	&v_as400_batch_date,		&v_as400_batch_time,
						END_OF_ARG_LIST									);


	OpenCursor (	MAIN_DB, AS400SRV_T4080PP_postproc_cur	);

	if (SQLERR_error_test ())
	{
		retcode = SQL_ERROR_OCCURED;
//		break;
	}

	while (1)
	{
		FetchCursor (	MAIN_DB, AS400SRV_T4080PP_postproc_cur	);

		if (SQLERR_code_cmp (SQLERR_end_of_fetch) == MAC_TRUE)
		{
			retcode = MSG_HANDLER_SUCCEEDED;	// In case there is nothing that needs to be deleted.
			break;
		}
		else
		{
			if (SQLERR_error_test ())
			{
				retcode = SQL_ERROR_OCCURED;
				break;
			}
		}

		// Calculate virtual time. Note that for this download the time is based
		// on the real time, adjusted only to ensure that we don't have too many
		// rules with the exact same as-of time.
		v_update_date		= download_start_date;
		v_update_time		= download_start_time + (download_row_count++ / VIRTUAL_PORTION);

		ExecSQL (	MAIN_DB, AS400SRV_T4080PP_UPD_pharmdrugnotes_logically_delete,
					&v_update_date,	&v_update_time,		END_OF_ARG_LIST						);
		
		
		HANDLE_DELETE_SQL_CODE ("postprocess_4080", p_connect, &retcode, &db_changed_flg);

		rows_deleted++;
	}	// Cursor-scrolling loop.

	CloseCursor (	MAIN_DB, AS400SRV_T4080PP_postproc_cur	);

	// DonR 13Nov2018 CR #15919: Record current table size.
	REMEMBER_ISOLATION;
	SET_ISOLATION_DIRTY;

	ExecSQL (	MAIN_DB, AS400SRV_T4080PP_READ_pharmdrugnotes_table_size,
				&total_rows, END_OF_ARG_LIST						);

	ExecSQL (	MAIN_DB, AS400SRV_T4080PP_READ_pharmdrugnotes_active_size,
				&active_rows,	END_OF_ARG_LIST			);

	RESTORE_ISOLATION;


	GerrLogMini (GerrId,
				 "Postprocess 4080: %d rows logically deleted, retcode = %d.",
				 rows_deleted, retcode);

	return retcode;

}	// End of msg 4080 (PharmDrugNotes) postprocessor.



/* << -------------------------------------------------------- >> */
/*                                                                */
/*     handler for message 2081 - upd/ins to Purchase Limit Ishur */
/*                                                                */
/* << -------------------------------------------------------- >> */
static int message_2081_handler (TConnection	*p_connect,
								 int			data_len)
{
	// local variables
	char	*PosInBuff;		/* current pos in data buffer	    */
	int		ReadInput;		/* amount of Bytesread from buffer  */
	int		retcode;		/* code returned from function	    */
	bool	stop_retrying;		/* if true - return from function   */
	bool	do_insert = ATTEMPT_UPDATE_FIRST;
	bool	found_old_row = 0;
	int		n_retries;		/* retrying counter		    */
	int		i;

	// DB variables
	int		v_member_id;
	short	v_mem_id_extension;
	int		v_largo_code;
	int		v_start_date;
	int		v_end_date;
	short	v_ishur_type;
	short	v_reason_code;
	char	v_ishur_text[61];
	int		v_max_units_long;
	short	v_duration;
	int		v_authority_id;
	short	v_ishur_source;
	int		v_pl_ishur_num;
	int		v_doctor_id;
	short	v_doc_saw_ishur;
	short	v_del_flg;
	int		sysdate;
	int		systime;
	double	ingr_qty_std;
	char	ingr_units [4];


	REMEMBER_ISOLATION;
	SET_ISOLATION_DIRTY;
	sysdate = GetDate ();
	systime = GetTime ();

	// read input buffer
	INIT_ERROR_DETECTION_MECHANISM;
	PosInBuff = glbMsgBuffer;

	v_member_id			= get_long	(&PosInBuff, Tmember_id_len			); RETURN_ON_ERROR;
	v_mem_id_extension	= get_short	(&PosInBuff, Tmem_id_extension_len	); RETURN_ON_ERROR;
	v_largo_code		= get_long	(&PosInBuff, 9						); RETURN_ON_ERROR;	//Marianna 14Jul2024 User Story #330877
	v_start_date		= get_long	(&PosInBuff, TGenericYYYYMMDD_len	); RETURN_ON_ERROR;
	v_end_date			= get_long	(&PosInBuff, TGenericYYYYMMDD_len	); RETURN_ON_ERROR;
	v_ishur_type		= get_short	(&PosInBuff, Tconfirmation_type_len	); RETURN_ON_ERROR;
	v_reason_code		= get_short (&PosInBuff, Tconfirm_reason_len	); RETURN_ON_ERROR;

	get_string (&PosInBuff,	v_ishur_text, Tmess_text_len); switch_to_win_heb ((unsigned char *)v_ishur_text);

	v_max_units_long	= get_long	(&PosInBuff, 8						); RETURN_ON_ERROR;
	v_duration			= get_short (&PosInBuff, Tduration_len			); RETURN_ON_ERROR;
	v_authority_id		= get_long	(&PosInBuff, Tdoctor_id_len			); RETURN_ON_ERROR;
	v_ishur_source		= get_short	(&PosInBuff, TGenericFlag_len		); RETURN_ON_ERROR;
	v_pl_ishur_num		= get_long	(&PosInBuff, 9						); RETURN_ON_ERROR;
	v_doctor_id			= get_long	(&PosInBuff, Tdoctor_id_len			); RETURN_ON_ERROR;
	v_doc_saw_ishur		= get_short	(&PosInBuff, TGenericFlag_len		); RETURN_ON_ERROR;
	v_del_flg			= get_short	(&PosInBuff, TGenericFlag_len		); RETURN_ON_ERROR;


	// verify all data was read:
	ReadInput = PosInBuff - glbMsgBuffer;
	StaticAssert (ReadInput == data_len);


	// up to SQL_UPDATE_RETRIES retries will be made to overcome access conflict
	retcode = ALL_RETRIES_WERE_USED;
	stop_retrying = false;

	for (n_retries = 0;
		 (n_retries < SQL_UPDATE_RETRIES) && (stop_retrying == false);
		 n_retries++)
	{
		// sleep for a while:
		if (n_retries > 0)
		{
			sleep (SLEEP_TIME_BETWEEN_RETRIES);
		}

		// Read drug information to set up Limit Ingredient and the unit conversion factor.
		ExecSQL (	MAIN_DB, AS400SRV_T2081_READ_drug_list_ingr_1_units,
					&ingr_units, &v_largo_code, END_OF_ARG_LIST				);

		// If we couldn't read drug_list, something is very odd - but in this case we'll just fill in dummy values.
		if (SQLCODE != 0)
		{
			strcpy (ingr_units, "mg");	// Default leads to conversion factor of 1.0 - i.e. no conversion.
		}

		// Set up Adjusted Ingredient Limit.
		ingr_qty_std = ConvertUnitAmount ((double)v_max_units_long, ingr_units);


		// DonR 06May2012: Delete ALL previous ishurim for this member/Largo Code, then ALWAYS insert.
		ExecSQL (	MAIN_DB, AS400SRV_T2081_DEL_purchase_lim_ishur_previous_rows,
					&v_member_id,			&v_largo_code,
					&v_mem_id_extension,	END_OF_ARG_LIST							);

		// Force correct diagnostic based on whether we found a pre-existing ishur to delete.
		found_old_row = (SQLCODE == 0);
//GerrLogMini (GerrId, "\nDeleted for %d/%d/%d, SQLCODE = %d.", v_member_id, v_largo_code, v_mem_id_extension, SQLCODE);

		ExecSQL (	MAIN_DB, AS400SRV_T2081_INS_purchase_lim_ishur,
					&v_member_id,				&v_mem_id_extension,
					&v_largo_code,				&v_ishur_source,
					&v_pl_ishur_num,			&v_ishur_type,
					&v_del_flg,					&v_reason_code,
					&v_ishur_text,				&v_max_units_long,
					&ingr_qty_std,				&v_duration,
					&v_authority_id,			&v_doctor_id,
					&v_doc_saw_ishur,			&sysdate,
					&systime,					&sysdate,
					&systime,					&sysdate,
					&systime,					&v_start_date,
					&v_end_date,				END_OF_ARG_LIST			);

//GerrLogMini (GerrId, "\nInserted for %d/%d/%d, SQLCODE = %d.", v_member_id, v_largo_code, v_mem_id_extension, SQLCODE);
		HANDLE_INSERT_SQL_CODE ("2081", p_connect, &do_insert, &retcode, &stop_retrying);

	}	// for (n_retries...


	if (n_retries == SQL_UPDATE_RETRIES)
	{
		WRITE_ACCESS_CONFLICT_TO_LOG ("2081");
	}

	if (retcode == MSG_HANDLER_SUCCEEDED)
	{
		if (found_old_row == 0)
			rows_added++;
		else
			rows_updated++;
	}

	RESTORE_ISOLATION;

	return retcode;
} /* end of msg 2081 handler */



/* << -------------------------------------------------------------- >> */
/*																		*/
/*     handler for message 2082 - add to table Drug Puchase Limits		*/
/*																		*/
/*     Added 27Jun2006 by Don Radlauer.									*/
/*																		*/
/* << -------------------------------------------------------------- >> */
static int message_2082_handler (TConnection	*p_connect,
								 int			data_len)
{
	// local variables
	char		*PosInBuff;		/* current pos in data buffer	    */
	bool		stop_retrying;	/* if true - return from function   */
	bool		do_insert;
	int			ReadInput;		/* amount of Bytesread from buffer  */
	int			retcode;		/* code returned from function	    */
	int			n_retries;		/* retrying counter		    */
	int			i;

	// DB variables
	int		sysdate;
	int		systime;
	int		v_largo_code;
	int		v_parent_group_code;
	int		v_max_units_long;
	int		v_history_start;
	short	v_duration;
	short	v_class_code;
	short	limit_method;
	short	warning_only;
	short	exempt_diseases;
	short	ingredient_code;
	short	ingr_1_code;			// DonR 18Mar2024 User Story #299818
	short	ishur_qty_lim_ingr;		// DonR 18Mar2024 User Story #299818
	short	presc_source;
	short	permit_source;
	short	month_is_28_days;		// DonR 08Feb2022 3 new fields.
	short	min_prev_purchases;		// DonR 08Feb2022 3 new fields.
	short	custom_error_code;		// DonR 08Feb2022 3 new fields.
	bool	include_full_price_sales;	// DonR 22Jul2025 User Story #427783
	double	ingr_qty_std;
	char	ingr_units [4];
	short	no_ishur_pharmacology;	// DonR 13Nov2025 User Story #453336.
	short	no_ishur_treat_typ;		// DonR 13Nov2025 User Story #453336.


	REMEMBER_ISOLATION;
	SET_ISOLATION_DIRTY;
	sysdate = GetDate();
	systime = GetTime();


	// read input buffer
	INIT_ERROR_DETECTION_MECHANISM;
	PosInBuff = glbMsgBuffer;


	v_largo_code				= get_long		(&PosInBuff, 9);	RETURN_ON_ERROR;	//Marianna 14Jul2024 User Story #330877
	v_class_code				= get_short		(&PosInBuff, 5);	RETURN_ON_ERROR;
	v_parent_group_code			= get_long		(&PosInBuff, 5);	RETURN_ON_ERROR;
	limit_method				= get_short		(&PosInBuff, 2);	RETURN_ON_ERROR;
	v_max_units_long			= get_long		(&PosInBuff, 8);	RETURN_ON_ERROR;
	presc_source				= get_short		(&PosInBuff, 3);	RETURN_ON_ERROR;
	permit_source				= get_short		(&PosInBuff, 2);	RETURN_ON_ERROR;
	warning_only				= get_short		(&PosInBuff, 2);	RETURN_ON_ERROR;
	exempt_diseases				= get_short		(&PosInBuff, 2);	RETURN_ON_ERROR;
	v_duration					= get_short		(&PosInBuff, 3);	RETURN_ON_ERROR;
	v_history_start				= get_long		(&PosInBuff, 8);	RETURN_ON_ERROR;	// DonR 20Nov2013: Fixed length (was 5).
	month_is_28_days			= get_short		(&PosInBuff, 1);	RETURN_ON_ERROR;
	min_prev_purchases			= get_short		(&PosInBuff, 3);	RETURN_ON_ERROR;
	custom_error_code			= get_short		(&PosInBuff, 4);	RETURN_ON_ERROR;
	include_full_price_sales	= get_short		(&PosInBuff, 1);	RETURN_ON_ERROR;
	no_ishur_pharmacology		= get_short		(&PosInBuff, 2);	RETURN_ON_ERROR;;	// DonR 13Nov2025 User Story #453336.
	no_ishur_treat_typ			= get_short		(&PosInBuff, 2);	RETURN_ON_ERROR;;	// DonR 13Nov2025 User Story #453336.

	// verify all data was read
	ReadInput = PosInBuff - glbMsgBuffer;
	StaticAssert (ReadInput == data_len);

	// up to SQL_UPDATE_RETRIES retries will be made to overcome access conflict
	retcode = ALL_RETRIES_WERE_USED;
	stop_retrying = false;

	for (n_retries = 0; (n_retries < SQL_UPDATE_RETRIES) && (stop_retrying == false); n_retries++)
	{
		// sleep for a while
		if( n_retries )
		{
			sleep (SLEEP_TIME_BETWEEN_RETRIES);
		}

		// try to insert new row
		// DonR 04Jul2010: HOT FIX: Need to have dummy "for" loop here so that HANDLE_UPDATE_SQL_CODE
		// and HANDLE_INSERT_SQL_CODE work properly.
		for (i = 0; i < 1; i++)
		{
			// Read drug information to set up Limit Ingredient and the unit conversion factor.
			// DonR 18Mar2024 User Story #299818: If there is an Ishur Quantity Limit Ingredient
			// specified, this is the "main" active ingredient that should be used for Purchase
			// Limit checking. In most cases this is the same ingredient as Ingredient #1, but
			// there are some cases where the main ingredient is #2 on the list.
			ExecSQL (	MAIN_DB, AS400SRV_T2082_READ_drug_list_ingr_1_columns,
						&ingr_1_code,	&ingr_units,		&ishur_qty_lim_ingr,
						&v_largo_code,	END_OF_ARG_LIST								);

			// If we couldn't read drug_list, something is very odd - but in this case we'll just fill in dummy values.
			if (SQLCODE != 0)
			{
				ingredient_code	= -999;
				strcpy (ingr_units, "mg");	// Default leads to conversion factor of 1.0 - i.e. no conversion.
			}
			else
			{
				// DonR 18Mar2024 User Story #299818: Use the Ishur Quantity Limit Ingredient
				// if it exists; otherwise use Ingredient #1. Also, if Ingredient #1 Units is
				// blank, default to "mg".
				ingredient_code = (ishur_qty_lim_ingr > 0) ? ishur_qty_lim_ingr : ingr_1_code;

				if (strlen (StripAllSpaces (ingr_units)) < 1)
					strcpy (ingr_units, "mg");	// Default leads to conversion factor of 1.0 - i.e. no conversion.
			}

			// Set up Adjusted Ingredient Limit.
			ingr_qty_std = ConvertUnitAmount ((double)v_max_units_long, ingr_units);


			ExecSQL (	MAIN_DB, AS400SRV_T2082_INS_drug_purchase_lim,
						&v_largo_code,		&v_class_code,				&v_parent_group_code,
						&limit_method,		&v_max_units_long,			&ingr_qty_std,
						&presc_source,		&permit_source,				&warning_only,

						&ingredient_code,	&exempt_diseases,			&v_duration,
						&v_history_start,	&month_is_28_days,			&min_prev_purchases,
						&custom_error_code,	&include_full_price_sales,	&no_ishur_pharmacology,
						&no_ishur_treat_typ,

						&sysdate,			&systime,
						&sysdate,			&systime,
						&sysdate,			&systime,					END_OF_ARG_LIST				);

			// DonR 22Mar2017: For purposes of "real" quantity limit checking, we're interested only in
			// Limit Methods 0 (= units) and 1 (= milligrams of active ingredient). Limit Method 2
			// deals only with whether a particular drug can be sold with a particular Prescription
			// Source - and thus should not be used to update the drug_list Quantity Limit Group
			// column. This could probably be accomplished with a single SQL command somehow, but
			// I'll keep it simple by using two different SQL commands for the two different cases.
			// DonR 11Dec2017 CR #12458: In addition to Limit Methods 0 and 1, the new Limit Methods
			// 3 and 4 count as "real" quantity limits. This doesn't require any change to the logic
			// below, but it's still worth commenting on!
			if (limit_method == 2)
			{
				// Not a "real" drug purchase limit, so don't update drug_list/qty_lim_grp_code.
				ExecSQL (	MAIN_DB, AS400SRV_T2082_UPD_drug_list_purchase_limit_flg_only,
							&v_largo_code, END_OF_ARG_LIST									);
			}
			else
			{
				// "Real" drug purchase limit.
				ExecSQL (	MAIN_DB, AS400SRV_T2082_UPD_drug_list_normal_purchase_limit,
							&v_parent_group_code, &v_largo_code, END_OF_ARG_LIST			);
			}

			HANDLE_INSERT_SQL_CODE ("2082", p_connect, &do_insert, &retcode, &stop_retrying);
		}	// Dummy loop.

	}/* for( n_retries... */

	if( n_retries == SQL_UPDATE_RETRIES )
	{
		WRITE_ACCESS_CONFLICT_TO_LOG( "2082" );
	}


	if (retcode == MSG_HANDLER_SUCCEEDED)
	{
		rows_added++;
	}

	RESTORE_ISOLATION;

	return retcode;
} /* end of msg 2082 handler */



/* << -------------------------------------------------------------------- >> */
/*																			  */
/*     handler for message 2084 - delete ALL rows from Drug Puchase Limits	  */
/*																			  */
/*     Added 27Jun2006 by Don Radlauer.										  */
/*							                                                  */
/* << -------------------------------------------------------------------- >> */
static int message_2084_handler (TConnection *p_connect, int data_len)
{
	int	retcode;		/* code returned from function	    */
	int	n_retries;		/* retrying counter		    */
	int	db_changed_flg;
	int	dummy_changed_flg;

	// DB variables
	int	v_largo_code;


	// DonR 04Feb2020: Shouldn't this be a SELECT DISTINCT?
	DeclareCursor (	MAIN_DB, AS400SRV_T2084_del_druglim_cur	);

	/* << -------------------------------------------------------- >> */
	/*			   attempt delete			  */
	/* << -------------------------------------------------------- >> */
	retcode = ALL_RETRIES_WERE_USED;
	db_changed_flg = 0;

	for (n_retries = 0; n_retries < SQL_UPDATE_RETRIES; n_retries++)
	{
		// sleep for a while
		if( n_retries )
		{
			sleep (SLEEP_TIME_BETWEEN_RETRIES);
		}

		OpenCursor (	MAIN_DB, AS400SRV_T2084_del_druglim_cur	);

		HANDLE_DELETE_SQL_CODE( "2084", p_connect, &retcode, &dummy_changed_flg );

		do
		{
			FetchCursorInto (	MAIN_DB, AS400SRV_T2084_del_druglim_cur,
								&v_largo_code, END_OF_ARG_LIST				);

			if (SQLERR_code_cmp (SQLERR_access_conflict) == MAC_TRUE)
			{
				retcode = ALL_RETRIES_WERE_USED;
				break; /* retry */
			}

			if (SQLERR_code_cmp (SQLERR_end_of_fetch) == MAC_TRUE)
			{
				break;
			}
			else
			{
				if (SQLERR_error_test ())
				{
					p_connect->sql_err_flg = 1;
					retcode = SQL_ERROR_OCCURED;
					break;
				}
				else
				{
					// We actually read a drug_purchase_lim row!
					ExecSQL (	MAIN_DB, AS400SRV_T2084_UPD_drug_list_clear_1_purchase_limit_flg,
								&v_largo_code, END_OF_ARG_LIST										);

					HANDLE_DELETE_SQL_CODE ("2084", p_connect, &retcode, &db_changed_flg);
					rows_deleted++;
				}
			}
		}
		while (1);


		CloseCursor (	MAIN_DB, AS400SRV_T2084_del_druglim_cur	);

		// Finally, delete all old Drug Purchase Limit data.
		ExecSQL (	MAIN_DB, AS400SRV_T2084_DEL_drug_purchase_lim_all_rows	);

		HANDLE_DELETE_SQL_CODE( "2084", p_connect, &retcode, &db_changed_flg );

		// DonR 07Apr2010: Just to be (justifiably) paranoid, set any "leftover" flags and group codes
		// in drug_list to zero.
		ExecSQL (	MAIN_DB, AS400SRV_T2084_UPD_drug_list_clear_all_purchase_limit_flg	);

		break;
	}

	if( n_retries == SQL_UPDATE_RETRIES )
	{
		WRITE_ACCESS_CONFLICT_TO_LOG( "2084" );
	}


	return retcode;
} /* end of msg 2084 handler */



/* << -------------------------------------------------------------------- >> */
/*																			  */
/*     Handlers for message 2086 - MemberPharm data & postprocessing		  */
/*																			  */
/*     Added 22Nov2006 by Don Radlauer.										  */
/*							                                                  */
/* << -------------------------------------------------------------------- >> */
static int data_2086_handler (TConnection *p_connect, int data_len)
{
	// local variables
	char		*PosInBuff;		// current pos in data buffer
	bool		stop_retrying;	// if true - return from function
	bool		do_insert = ATTEMPT_UPDATE_FIRST;
	int			ReadInput;		// amount of Bytesread from buffer
	int			retcode;		// code returned from function
	int			n_retries;		// retrying counter
	int			i;

	// DB variables
	int		v_member_id;
	short	v_mem_id_extension;
	int		v_seq_number;
	int		v_pharmacy_code;
	short	v_pharmacy_type;
	int		v_open_date;
	int		v_close_date;
	int		v_pharm_code_temp;
	short	v_pharm_type_temp;
	int		v_open_date_temp;
	int		v_close_date_temp;
	short	v_restriction_type;
	char	v_description		[71];
	char	v_description_temp	[71];
	int		v_unix_recv_date	= download_start_date;
	int		v_unix_recv_time	= download_start_time;
	short	v_del_flg;
	short	v_permitted_owner;	// DonR 24Jul2023 User Story #448931


	// read input buffer
	INIT_ERROR_DETECTION_MECHANISM;
	PosInBuff = glbMsgBuffer;

	v_seq_number		= get_long		(&PosInBuff, 9);	RETURN_ON_ERROR;
	v_member_id			= get_long		(&PosInBuff, 9);	RETURN_ON_ERROR;
	v_mem_id_extension	= get_short		(&PosInBuff, 1);	RETURN_ON_ERROR;
	v_pharmacy_code		= get_long		(&PosInBuff, 7);	RETURN_ON_ERROR;
	v_pharmacy_type		= get_short		(&PosInBuff, 1);	RETURN_ON_ERROR;
	v_open_date			= get_long		(&PosInBuff, 8);	RETURN_ON_ERROR;
	v_close_date		= get_long		(&PosInBuff, 8);	RETURN_ON_ERROR;
	v_pharm_code_temp	= get_long		(&PosInBuff, 7);	RETURN_ON_ERROR;
	v_pharm_type_temp	= get_short		(&PosInBuff, 1);	RETURN_ON_ERROR;
	v_open_date_temp	= get_long		(&PosInBuff, 8);	RETURN_ON_ERROR;
	v_close_date_temp	= get_long		(&PosInBuff, 8);	RETURN_ON_ERROR;
	v_restriction_type	= get_short		(&PosInBuff, 1);	RETURN_ON_ERROR;

	get_string (&PosInBuff,	v_description,		70); switch_to_win_heb ((unsigned char *)v_description);
	get_string (&PosInBuff,	v_description_temp,	70); switch_to_win_heb ((unsigned char *)v_description_temp);

	v_del_flg			= get_short		(&PosInBuff, 1);	RETURN_ON_ERROR;


	// verify all data was read
	ReadInput = PosInBuff - glbMsgBuffer;
	StaticAssert (ReadInput == data_len);


	// up to SQL_UPDATE_RETRIES retries will be made to overcome access conflict
	retcode = ALL_RETRIES_WERE_USED;
	stop_retrying = false;

	for (n_retries = 0; (n_retries < SQL_UPDATE_RETRIES) && (stop_retrying == false); n_retries++)
	{
		// sleep for a while
		if( n_retries )
		{
			sleep (SLEEP_TIME_BETWEEN_RETRIES);
		}

		// DonR 24Jul2023 User Story #448931: Get ownership of permitted pharmacy from the pharmacy table.
		// Don't bother with error-handling; just default to 1 (= private pharmacy) if the pharmacy code
		// is under 100000.
		// DonR 03Sep2023 User Story #448931 FIX part 1: If the pharmacy code is zero, don't bother looking it up.
		if (v_pharmacy_code > 0)
		{
			ExecSQL (	MAIN_DB, AS400SRV_T2086_GetPharmOwner,
						&v_permitted_owner, &v_pharmacy_code, END_OF_ARG_LIST	);
		}
		else
		{
			FORCE_NOT_FOUND;
		}

		if (SQLCODE)
		{
			// DonR 03Sep2023 User Story #448931 FIX part 2: If the pharmacy code is zero, set Permitted
			// Owner based on Pharmacy Type, not Pharmacy Code. Pharmacy Type 1 == Maccabi, which is
			// the *opposite* of how Pharmacy Owner is assigned. So Type 1 = Owner 0, and vice versa!
			if (v_pharmacy_code == 0)
				v_permitted_owner = (v_pharmacy_type	== 0)		? 1 : 0;
			else
				v_permitted_owner = (v_pharmacy_code	<  100000)	? 1 : 0;
		}

		// Try to insert/update new rec with the order depending on "do_insert":
		for (i = 0; i < 2; i++)
		{
			if (do_insert == ATTEMPT_INSERT_FIRST)
			{
				// try to insert new row.
				ExecSQL (	MAIN_DB, AS400SRV_T2086_INS_MemberPharm,
							&v_member_id,			&v_mem_id_extension,
							&v_seq_number,			&v_pharmacy_code,
							&v_pharmacy_type,		&v_open_date,
							&v_close_date,			&v_pharm_code_temp,
							&v_pharm_type_temp,		&v_open_date_temp,
							&v_close_date_temp,		&v_restriction_type,
							&v_description,			&v_description_temp,
							&v_unix_recv_date,		&v_unix_recv_time,
							&v_unix_recv_date,		&v_unix_recv_time,
							&v_del_flg,				&v_permitted_owner,
							END_OF_ARG_LIST									);

				HANDLE_INSERT_SQL_CODE ("2086", p_connect, &do_insert, &retcode, &stop_retrying);
			}	// Attempting to insert new row.

			else
			{
				// Perform full update only if something has changed.
				ExecSQL (	MAIN_DB, AS400SRV_T2086_UPD_MemberPharm,
							&v_seq_number,			&v_pharmacy_code,
							&v_pharmacy_type,		&v_open_date,
							&v_close_date,			&v_pharm_code_temp,
							&v_pharm_type_temp,		&v_open_date_temp,
							&v_close_date_temp,		&v_restriction_type,
							&v_description,			&v_description_temp,
							&v_unix_recv_date,		&v_unix_recv_time,
							&v_unix_recv_date,		&v_unix_recv_time,
							&v_del_flg,				&v_permitted_owner,

							&v_member_id,			&v_mem_id_extension,

							&v_seq_number,			&v_pharmacy_code,
							&v_pharmacy_type,		&v_open_date,
							&v_close_date,			&v_pharm_code_temp,
							&v_pharm_type_temp,		&v_open_date_temp,
							&v_close_date_temp,		&v_restriction_type,
							&v_description,			&v_description_temp,
							&v_del_flg,				&v_permitted_owner,
							END_OF_ARG_LIST									);

				// If nothing has changed, try changing only the received date/time.
				if (SQLERR_code_cmp (SQLERR_no_rows_affected) == MAC_TRUE)
				{
					ExecSQL (	MAIN_DB, AS400SRV_T2086_UPD_MemberPharm_recv_timestamp_only,
								&v_unix_recv_date,		&v_unix_recv_time,
								&v_member_id,			&v_mem_id_extension,
								END_OF_ARG_LIST													);
				}

				// If neither update command found anything to update, try inserting a new row.
				HANDLE_UPDATE_SQL_CODE ("2086", p_connect, &do_insert, &retcode, &stop_retrying);

			}	// Attempting to update.

		}	// Insert/update loop. 

	}	// Retry loop.


	if (n_retries == SQL_UPDATE_RETRIES)
	{
		WRITE_ACCESS_CONFLICT_TO_LOG ("2086/2");
	}


	if (retcode == MSG_HANDLER_SUCCEEDED)
	{
		if (do_insert == ATTEMPT_INSERT_FIRST)
			rows_added++;
		else
			rows_updated++;
	}


	return retcode;
}


static int postprocess_2086 (TConnection *p_connect)
{
	int	retcode;		// code returned from function
	int	n_retries;		// retrying counter
	int	db_changed_flg;

	// DB variables
	int		SaveDate;


	// Initialization.
	SaveDate		= IncrementDate (GetDate(), -7);
	retcode			= ALL_RETRIES_WERE_USED;
	db_changed_flg	= 0;

	
	// Post-download processing for MemberPharm: Get rid of anything in the
	// table that expired more than one week ago. For now, we can safely (?)
	// assume that the table won't get big enough for the number of deletions
	// to be too large to be handled in a single transaction.
	for (n_retries = 0; n_retries < SQL_UPDATE_RETRIES; n_retries++)
	{
		// sleep for a while
		if (n_retries)
		{
			sleep (SLEEP_TIME_BETWEEN_RETRIES);
		}

		ExecSQL (	MAIN_DB, AS400SRV_T2086PP_DEL_MemberPharm_old_data,
					&SaveDate, &SaveDate, END_OF_ARG_LIST				);

		HANDLE_DELETE_SQL_CODE ("2086", p_connect, &retcode, &db_changed_flg);

		break;
	}

	if (n_retries == SQL_UPDATE_RETRIES)
	{
		WRITE_ACCESS_CONFLICT_TO_LOG ("2086/3");
	}


	// DonR 13Nov2018 CR #15919: Record current table size.
	REMEMBER_ISOLATION;
	SET_ISOLATION_DIRTY;

	ExecSQL (	MAIN_DB, AS400SRV_T2086PP_READ_table_size,
				&total_rows, END_OF_ARG_LIST				);

	active_rows = total_rows;

	RESTORE_ISOLATION;
	// DonR 13Nov2018 CR #15919 end.


	GerrLogMini (GerrId, "Postprocess 2086: retcode = %d.", retcode);

	return retcode;
}




// -------------------------------------------------------------------------
//
//       Handler for message 2096 - update/insert to PURCHASE LIMIT CLASS
//
//            Performs detailed comparison to detect rows that have changed.
//
// -------------------------------------------------------------------------
static int data_2096_handler (TConnection	*p_connect,
							  int			data_len)
{
	// local variables
	char	*PosInBuff;		// current pos in data buffer
	int		ReadInput;		// amount of Bytesread from buffer
	int		retcode;		// code returned from function
	int		n_retries;		// retrying counter
	float	days;

	// DB variables
	short	v_class_code;
	short	v_class_type;
	short	v_class_sex;
	char	v_class_sex_as400	[2];
	short	v_class_min_age;
	short	v_class_max_age;
	short	v_class_priority;
	short	v_class_hist_mons;
	int		v_class_hist_days;
	int		v_class_hist_start;
	short	prev_sales_span_mons;	// DonR 16Nov2025 User Story #453336.
	int		prev_sales_span_days;	// DonR 16Nov2025 User Story #453336.
	char	v_class_long_desc	[76];
	char	v_class_short_desc	[26];
	int		v_as400_batch_date;
	int		v_as400_batch_time;


	// Get batch start date (which should always have a value); set it conditionally
	// from the system clock just to be paranoid.
	v_as400_batch_date = (download_start_date > 0) ? download_start_date : GetDate();
	v_as400_batch_time = (download_start_time > 0) ? download_start_time : GetTime();

	// read input buffer
	INIT_ERROR_DETECTION_MECHANISM;
	PosInBuff = glbMsgBuffer;

	// DonR 30Nov2020: Use GET_HEB_STR to get description fields, so Hebrew text will look OK.
	GET_SHORT	(v_class_code			,	 5);
	GET_HEB_STR	(v_class_long_desc		,	75);
	GET_HEB_STR	(v_class_short_desc		,	25);
	GET_SHORT	(v_class_type			,	 1);
	GET_STRING	(v_class_sex_as400		,	 1);
	GET_SHORT	(v_class_min_age		,	 3);
	GET_SHORT	(v_class_max_age		,	 4);	// 3->4 - DonR 02May2022 User Story #239501.
	GET_SHORT	(v_class_priority		,	 5);
	GET_SHORT	(v_class_hist_mons		,	 2);
	GET_LONG	(v_class_hist_start		,	 8);	// AS of 05Feb2020, this is not stored or used.
	GET_SHORT	(prev_sales_span_mons	,	 2);	// DonR 16Nov2025 User Story #453336.

	// Verify all data was read.
	ReadInput = PosInBuff - glbMsgBuffer;
	StaticAssert (ReadInput == data_len);


	// DonR 03Jun2010: AS/400 sex variable is now a character, with ' ' the equivalent of zero.
	// Process it back into an integer!
	if ((v_class_sex_as400[0] >= '0') && (v_class_sex_as400[0] <= '2'))
		v_class_sex = (short)v_class_sex_as400[0] - (short)'0';
	else
		v_class_sex = 0;	// Anything other than '0', '1', or '2' means no sex selection.


	// Before writing anything to the database, do some data checking/massaging.
	switch (v_class_type)
	{
	case 0:		// "Default" category - everyone qualifies.
				// Note that while the default class should always have Class Code
				// equal to zero, we don't enforce this - since a default class with
				// a non-zero Class Code won't actually harm anything. (The only danger
				// is that such a class, unlike the "proper" Class 0/Type 0 default
				// class, is not protected from deletion by the Trn. 2096 Postprocessor.)
				v_class_priority		= 9999;	// Force lowest priority for default class.
				v_class_sex				= 0;	// Both sexes qualify.
				v_class_min_age			= -1;	// I.e. no minimum age.
				v_class_max_age			= 999;	// I.e. no maximum age - unless someone lives 1,000 years!
				v_class_hist_days		= 0;	// No purchase-history check.
				prev_sales_span_days	= 0;	// No purchase-history check.
				break;


	case 2:		// Drug-purchase category - no demographic criteria.
				v_class_sex			= 0;	// Both sexes qualify.
				v_class_min_age		= -1;	// I.e. no minimum age.
				v_class_max_age		= 999;	// I.e. no maximum age - unless someone lives 1,000 years!

				// DonR 16Nov2025 User Story #453336: Do more accurate conversion from months
				// to days. This version gives us a 368-day year, which is still a bit long,
				// but it's as close as we can come to accuracy in all cases with a simple
				// computation.
				// DonR 17Dec2025: Just to keep stuff "clean", assign a non-zero value to
				// v_class_hist_days and prev_sales_span_days only if v_class_hist_mons (or
				// prev_sales_span_mons) is greater than zero. Otherwise we get meaningless
				// values of 1.
//				v_class_hist_days	= v_class_hist_mons * 31;	// DonR 05May2011: Get value from AS/400 and multiply by 31.
				if (v_class_hist_mons > 0)
				{
					days					= ((float)v_class_hist_mons		* 30.5001) + 1.0;
					v_class_hist_days		= (int)days;
				}
				else v_class_hist_days = 0;

				if (prev_sales_span_mons > 0)
				{
					days					= ((float)prev_sales_span_mons	* 30.5001) + 1.0;
					prev_sales_span_days	= (int)days;
				}
				else prev_sales_span_days = 0;

				break;


	case 1:		// Demographic category - age and/or sex criteria, no drug purchase required.
				v_class_hist_days		= 0;	// No purchase-history check.
				prev_sales_span_days	= 0;	// No purchase-history check.
				break;


	default:	// Something strange - write a message to the log, and set age criteria to disable
				// the category.
				GerrLogMini (GerrId, "Got strange Class Type %d for Purchase Limit Class %d.",
							 v_class_type, v_class_code);
				v_class_max_age			= -1;	// Disable.
				v_class_min_age			= 999;	// Disable even more.
				v_class_sex				= -1;	// Let's *really* disable this sucker!
				v_class_hist_days		= 0;	// No purchase-history check.
				prev_sales_span_days	= 0;	// No purchase-history check.
				break;
	}

	// Perform up to SQL_UPDATE_RETRIES retries to overcome access conflicts.
	for (n_retries = 0, retcode = ALL_RETRIES_WERE_USED;
		 n_retries < SQL_UPDATE_RETRIES;
		 n_retries++)
	{
		// If this isn't the first iteration, sleep for a while.
		if (n_retries > 0)
		{
			sleep (SLEEP_TIME_BETWEEN_RETRIES);
		}


		// Because this is a low-volume table, we'll do updates in two stages: first a non-conditional
		// update of AS/400 batch date/time to indicate fresh data and see if we need to insert instead,
		// and then a conditional update that will set update_date/time only if something has actually
		// changed.
		ExecSQL (	MAIN_DB, AS400SRV_T2096_UPD_as400_batch_timestamp,
					&v_as400_batch_date,	&v_as400_batch_time,
					&v_class_code,			END_OF_ARG_LIST				);

		// First possibility: the row isn't already in the database, so we need to insert (or un-delete) it.
		if (SQLERR_code_cmp (SQLERR_no_rows_affected) == MAC_TRUE)
		{
			// There is nothing in the table for this Class Code.
			// Just fall through to the next stage - attempt to insert a new row.
		}
		else
		{
			// Second possibility: there was an access conflict, so advance to the next retry iteration.
			if (SQLERR_code_cmp (SQLERR_access_conflict) == MAC_TRUE)
			{
				retcode = ALL_RETRIES_WERE_USED;
				continue;
			}
			else
			{
				// Third possibility: there was a "real" database error, so don't retry - just give up.
				if (SQLERR_error_test ())
				{
					retcode = SQL_ERROR_OCCURED;
					break;
				}
				else
				// Fourth possibility: the refresh succeeded, so we'll try updating "real" fields.
				{
					ExecSQL (	MAIN_DB, AS400SRV_T2096_UPD_purchase_lim_class,
								&v_class_long_desc,			&v_class_short_desc,
								&v_class_type,				&v_class_sex,
								&v_class_min_age,			&v_class_max_age,
								&v_class_hist_days,			&v_class_priority,
								&prev_sales_span_days,		&v_as400_batch_date,
								&v_as400_batch_time,

								&v_class_code,				&v_class_long_desc,
								&v_class_short_desc,		&v_class_type,
								&v_class_sex,				&v_class_min_age,
								&v_class_max_age,			&v_class_hist_days,
								&v_class_priority,			&prev_sales_span_days,
								END_OF_ARG_LIST										);

					// First possibility: nothing "real" changed, so no rows were affected.
					if (SQLERR_code_cmp (SQLERR_no_rows_affected) == MAC_TRUE)
					{
						retcode = MSG_HANDLER_SUCCEEDED;
						rows_refreshed++;
						break;
					}
					else
					{
						// Second possibility: there was an access conflict, so advance to the next retry iteration.
						if (SQLERR_code_cmp (SQLERR_access_conflict) == MAC_TRUE)
						{
							retcode = ALL_RETRIES_WERE_USED;
							continue;
						}
						else
						{
							// Third possibility: there was a "real" database error, so don't retry - just give up.
							if (SQLERR_error_test ())
							{
								retcode = SQL_ERROR_OCCURED;
								break;
							}
							else
							{
								// Fourth possibility: the update succeeded.
								retcode = MSG_HANDLER_SUCCEEDED;
								rows_updated++;
								break;
							}	// Successful update of changed data.
						}	// No access conflict on "real" update.
					}	// Something other than "no rows affected" on "real" update. 
				}	// Batch-date refresh succeeded.
			}	// Not an access conflict on batch-date refresh.
		}	// Something other than record-not-found on batch-date refresh.


		// If we get here, we have to insert a new purchase_lim_class row.
		ExecSQL (	MAIN_DB, AS400SRV_T2096_INS_purchase_lim_class,
					&v_class_code,				&v_class_long_desc,
					&v_class_short_desc,		&v_class_type,
					&v_class_sex,				&v_class_min_age,
					&v_class_max_age,			&v_class_priority,
					&v_class_hist_days,			&prev_sales_span_days,

					&v_as400_batch_date,		&v_as400_batch_time,
					&v_as400_batch_date,		&v_as400_batch_time,
					END_OF_ARG_LIST										);

		if (SQLERR_code_cmp (SQLERR_ok) == MAC_TRUE)	// Successful insertion - we're done!
		{
			rows_added++;
			retcode = MSG_HANDLER_SUCCEEDED;
			break;
		}

		else
		{
			// If we get here, we hit a "real" error, either on the INSERT or on the UPDATE.
			if (SQLERR_code_cmp (SQLERR_access_conflict) == MAC_TRUE)
			{
				retcode = ALL_RETRIES_WERE_USED;
				continue;
			}
			else
			{
				if (SQLERR_error_test ())
				{
					retcode = SQL_ERROR_OCCURED;
					break;
				}
			}	// Some error other than an access conflict.

		}	// INSERT failed.

	}	// Retry Loop.


	if (n_retries == SQL_UPDATE_RETRIES)
	{
		WRITE_ACCESS_CONFLICT_TO_LOG ("2096");
	}

	return retcode;

}	// End of msg 2096 (Purchase Limit Class) handler.


// -------------------------------------------------------------------------
//
//       Post-processor for message 2096 - logically delete Purchase Limit Class
//			rows that are no longer in the AS/400 full-table download.
//
// -------------------------------------------------------------------------
static int postprocess_2096 (TConnection	*p_connect)
{
	// local variables
	int		retcode;		// code returned from function
	int		n_retries;		// retrying counter

	// DB variables
	int		rows_deleted	= 0;
	int		v_as400_batch_date;
	int		v_as400_batch_time;


	// Initialize variables.
	v_as400_batch_date	= download_start_date;
	v_as400_batch_time	= download_start_time;

	// Decrement batch time by an hour. Since this download comes from RKFILPRD rather than RKUNIX,
	// there is some possibility that CL.EC will pick up the table before it's complete, and we'll
	// receive the table in two batches. By deleting only rows that are more than an hour old, we
	// should avoid partial-table-download crises.
	if (v_as400_batch_time > 9999)
	{
		v_as400_batch_time -= 10000;
	}
	else
	{
		v_as400_batch_time += 230000;
		v_as400_batch_date = IncrementDate (v_as400_batch_date, -1);
	}


	// Delete everything old in the table except for Class Code 0 / Class Type 0. This is the
	// default "everyone qualifies" class, and while the AS/400 is supposed to supply it,
	// there's no advantage in deleting it and some safety in retaining it.
	// To provide a nice log message, count what will be deleted before deleting it.
	ExecSQL (	MAIN_DB, AS400SRV_T2096PP_READ_purchase_lim_class_stale_count,
				&rows_deleted,
				&v_as400_batch_date,	&v_as400_batch_date,
				&v_as400_batch_time,	END_OF_ARG_LIST							);

	ExecSQL (	MAIN_DB, AS400SRV_T2096PP_DEL_purchase_lim_class,
				&v_as400_batch_date,	&v_as400_batch_date,
				&v_as400_batch_time,	END_OF_ARG_LIST				);

	if (SQLERR_error_test ())
	{
		rows_deleted = 0;
		retcode = SQL_ERROR_OCCURED;
	}
	else
	{
		retcode = MSG_HANDLER_SUCCEEDED;
	}


	// DonR 13Nov2018 CR #15919: Record current table size.
	REMEMBER_ISOLATION;
	SET_ISOLATION_DIRTY;

	ExecSQL (	MAIN_DB, AS400SRV_T2096PP_READ_purchase_lim_class_table_size,
				&total_rows, END_OF_ARG_LIST									);

	active_rows = total_rows;

	RESTORE_ISOLATION;
	// DonR 13Nov2018 CR #15919 end.


	GerrLogMini (GerrId,
				 "Postprocess 2096: %d rows deleted, retcode = %d.",
				 rows_deleted, retcode);

	return retcode;

}	// End of msg 2096 (Purchase Limit Class) postprocessor.



// -------------------------------------------------------------------------
//
//       Handler for message 4100 - update/insert to COUPONS
//
//            Performs detailed comparison to detect rows that have changed.
//				(Comparison applies only to del_flg - everything else is
//				 key fields.)
//
// -------------------------------------------------------------------------
static int data_4100_handler (TConnection	*p_connect,
							  int			data_len)
{
	// local variables
	char	*PosInBuff;		// current pos in data buffer
	int		ReadInput;		// amount of Bytesread from buffer
	int		retcode;		// code returned from function
	int		n_retries;		// retrying counter

	// DB variables
	int		v_member_id;
	short	v_mem_id_extension;
	int		expires_cyymm;
	short	expiry_status;
	short	fund_code;
	int		v_insert_date;
	int		v_insert_time;
	int		v_as400_batch_date;
	int		v_as400_batch_time;


	// Calculate virtual time. Note that for this download the time is based
	// on the real time, adjusted only to ensure that we don't have too many
	// items with the exact same as-of time. (Note that this routine will function
	// in full-table mode or incremental-insert mode - so we can't count on
	// download_start_date/time having a value.)
	v_insert_date  = v_as400_batch_date = (download_start_date > 0) ? download_start_date : GetDate();
	v_insert_time  = v_as400_batch_time = (download_start_time > 0) ? download_start_time : GetTime();
	v_insert_time += (download_row_count++ / VIRTUAL_PORTION);


	// read input buffer
	INIT_ERROR_DETECTION_MECHANISM;
	PosInBuff = glbMsgBuffer;

	GET_LONG	(v_member_id				, 9);
	GET_SHORT	(v_mem_id_extension			, 1);
	GET_LONG	(expires_cyymm				, 8);
	GET_SHORT	(expiry_status				, 1);
	GET_SHORT	(fund_code					, 3);
	
	// Verify all data was read.
	ReadInput = PosInBuff - glbMsgBuffer;
	StaticAssert (ReadInput == data_len);


	// Perform up to SQL_UPDATE_RETRIES retries to overcome access conflicts.
	for (n_retries = 0, retcode = ALL_RETRIES_WERE_USED;
		 n_retries < SQL_UPDATE_RETRIES;
		 n_retries++)
	{
		// If this isn't the first iteration, sleep for a while.
		if (n_retries > 0)
		{
			sleep (SLEEP_TIME_BETWEEN_RETRIES);
		}


		// The most frequent operation will be a refresh of an existing, non-deleted row; in this
		// case, there is no need to update the members table. The AS/400 batch date and time need
		// to be updated so we know not to delete the row.
		// DonR 19Aug2018 CR #15260: For both UPDATEs and INSERTs, add three new fields so we can
		// tell pharmacies when members have a coupon that will expire soon.
		ExecSQL (	MAIN_DB, AS400SRV_T4100_UPD_coupons,
					&expires_cyymm,			&expiry_status,
					&fund_code,				&v_as400_batch_date,
					&v_as400_batch_time,
					&v_member_id,			&v_mem_id_extension,
					&ROW_NOT_DELETED,		END_OF_ARG_LIST			);

		// First possibility: the row isn't already in the database, so we need to insert (or un-delete) it.
		if (SQLERR_code_cmp (SQLERR_no_rows_affected) == MAC_TRUE)
		{
			// Either there is nothing in the coupons table for this member, or the member's row is deleted.
			// In either case, just fall through to the next stage - attempt to insert a new row.
		}
		else
		{
			// Second possibility: there was an access conflict, so advance to the next retry iteration.
			if (SQLERR_code_cmp (SQLERR_access_conflict) == MAC_TRUE)
			{
				retcode = ALL_RETRIES_WERE_USED;
				continue;
			}
			else
			{
				// Third possibility: there was a "real" database error, so don't retry - just give up.
				if (SQLERR_error_test ())
				{
					retcode = SQL_ERROR_OCCURED;
					break;
				}
				else
				// Fourth possibility: the refresh succeeded, so we don't have to do anything else.
				{
					retcode = MSG_HANDLER_SUCCEEDED;
					rows_refreshed++;
					break;
				}	// Refresh succeeded.
			}	// Not an access conflict.
		}	// Not record-not-found.


		// If we get here, the first UPDATE command found nothing to refresh. This means that we have
		// to perform a logical insertion - either an actual insertion or (less likely) an un-deletion.
		// Either way, we have to update the has_coupon flag in the members table.
		ExecSQL (	MAIN_DB, AS400SRV_T4100_UPD_members_set_has_coupon_true,
					&v_member_id, &v_mem_id_extension, END_OF_ARG_LIST			);

		// If the members row wasn't found, do nothing special - the members INSERT will read the coupons
		// table to set the flag.
		if (SQLERR_code_cmp (SQLERR_no_rows_affected) == MAC_TRUE)
		{
			// No members row to update - just go ahead and insert into coupons.
		}
		else
		{
			// Second possibility: there was an access conflict, so advance to the next retry iteration.
			if (SQLERR_code_cmp (SQLERR_access_conflict) == MAC_TRUE)
			{
				retcode = ALL_RETRIES_WERE_USED;
				continue;
			}
			else
			{
				// Third possibility: there was a "real" database error, so don't retry - just give up.
				if (SQLERR_error_test ())
				{
					retcode = SQL_ERROR_OCCURED;
					break;
				}
				else
				// Fourth possibility: the members update succeeded. We still have to insert (or undelete)
				// the coupons row.
				{
					// Do nothing - just continue to the coupons insert/undelete section.
				}	// Members update succeeded.
			}	// Not an access conflict.
		}	// Not record-not-found.


		// If we get here, we have to perform either an insertion to the coupons table, or else
		// undelete a deleted coupons row. Since insertions will probably be more common, we'll
		// try that first.
		ExecSQL (	MAIN_DB, AS400SRV_T4100_INS_coupons,
					&v_member_id			, &v_mem_id_extension	, &ROW_NOT_DELETED	,
					&expires_cyymm			, &expiry_status		, &fund_code		,
					&v_insert_date			, &v_insert_time		,
					&v_insert_date			, &v_insert_time		,
					&v_as400_batch_date		, &v_as400_batch_time	, END_OF_ARG_LIST		);

		if (SQLERR_code_cmp (SQLERR_ok) == MAC_TRUE)	// Successful insertion - we're done!
		{
			rows_added++;
			retcode = MSG_HANDLER_SUCCEEDED;
			break;
		}

		else
		{
			// If the INSERT failed with a "row already present" message, it should mean that the row was
			// previously logically deleted - so we'll try to un-delete it. (Note that this is the only
			// "update" operation for this transaction, even though it's sorta-kinda a logical insertion.)
			if (SQLERR_code_cmp (SQLERR_not_unique) == MAC_TRUE)
			{
				ExecSQL (	MAIN_DB, AS400SRV_T4100_UPD_coupons_previously_deleted,
							&ROW_NOT_DELETED,			&expires_cyymm,
							&expiry_status,				&fund_code,
							&v_insert_date,				&v_insert_time,
							&v_as400_batch_date,		&v_as400_batch_time,
							&v_member_id,				&v_mem_id_extension,
							END_OF_ARG_LIST											);

				if (SQLERR_code_cmp (SQLERR_ok) == MAC_TRUE)	// Successful update - we're done!
				{
					rows_updated++;
					retcode = MSG_HANDLER_SUCCEEDED;
					break;
				}
			}	// INSERT failed because the row was already present.

			// If we get here, we hit a "real" error, either on the INSERT or on the UPDATE.
			if (SQLERR_code_cmp (SQLERR_access_conflict) == MAC_TRUE)
			{
				retcode = ALL_RETRIES_WERE_USED;
				continue;
			}
			else
			{
				if (SQLERR_error_test ())
				{
					retcode = SQL_ERROR_OCCURED;
					break;
				}
			}	// Some error other than an access conflict.

		}	// INSERT failed.

	}	// Retry Loop.


	if (n_retries == SQL_UPDATE_RETRIES)
	{
		WRITE_ACCESS_CONFLICT_TO_LOG ("4100");
	}

	return retcode;

}	// End of msg 4100 (coupons) handler.



// -------------------------------------------------------------------------
//
//       Post-processor for message 4100 - logically delete COUPONS
//			rows that are no longer in the AS/400 full-table download.
//
// -------------------------------------------------------------------------
static int postprocess_4100 (TConnection	*p_connect)
{
	// local variables
	bool	stop_retrying;	// if true - return from function
	int		rows_deleted	= 0;
	int		retcode;		// code returned from function
	int		n_retries;		// retrying counter
	int		db_changed_flg;

	// DB variables
	int		v_member_id;
	short	v_mem_id_extension;
	int		v_as400_batch_date;
	int		v_as400_batch_time;
	int		v_update_date;
	int		v_update_time;


	// Initialize variables.
	v_as400_batch_date	= download_start_date;
	v_as400_batch_time	= download_start_time;


	retcode			= ALL_RETRIES_WERE_USED;
	stop_retrying	= false;


	DeclareCursorInto (	MAIN_DB, AS400SRV_T4100PP_postproc_cur,
						&v_member_id,			&v_mem_id_extension,
						&ROW_NOT_DELETED,		&v_as400_batch_date,
						&v_as400_batch_date,	&v_as400_batch_time,
						END_OF_ARG_LIST									);

	OpenCursor (	MAIN_DB, AS400SRV_T4100PP_postproc_cur	);

	if (SQLERR_error_test ())
	{
		retcode = SQL_ERROR_OCCURED;
//		break;
	}

	while (1)
	{
		FetchCursor (	MAIN_DB, AS400SRV_T4100PP_postproc_cur	);

		if (SQLERR_code_cmp (SQLERR_end_of_fetch) == MAC_TRUE)
		{
			retcode = MSG_HANDLER_SUCCEEDED;	// In case there is nothing that needs to be deleted.
			break;
		}
		else
		{
			if (SQLERR_error_test ())
			{
				retcode = SQL_ERROR_OCCURED;
				break;
			}
		}

		// Calculate virtual time. Note that for this download the time is based
		// on the real time, adjusted only to ensure that we don't have too many
		// rules with the exact same as-of time.
		v_update_date		= download_start_date;
		v_update_time		= download_start_time + (download_row_count++ / VIRTUAL_PORTION);

		ExecSQL (	MAIN_DB, AS400SRV_T4100PP_UPD_coupons_logically_delete,
					&ROW_DELETED,		&v_update_date,
					&v_update_time,		END_OF_ARG_LIST						);

		HANDLE_DELETE_SQL_CODE ("postprocess_4100", p_connect, &retcode, &db_changed_flg);

		// Reminder: The coupons table is keyed by Member TZ/TZ Code, so there will be at most one
		// row per member. If that row is being logically deleted, we can safely set this member's
		// has_coupon flag FALSE.
		ExecSQL (	MAIN_DB, AS400SRV_T4100PP_UPD_members_set_has_coupon_false,
					&v_member_id, &v_mem_id_extension, END_OF_ARG_LIST			);

		// If the members row wasn't found, do nothing special - the members INSERT will read the coupons
		// table to set the flag.
		if (SQLERR_code_cmp (SQLERR_no_rows_affected) == MAC_TRUE)
		{
			// No members row to update - don't worry about it, just go on with the next coupon to delete.
		}
		else
		{
			if (SQLERR_error_test ())
			{
				retcode = SQL_ERROR_OCCURED;
				break;
			}
		}

		rows_deleted++;
	}	// Cursor-scrolling loop.

	CloseCursor (	MAIN_DB, AS400SRV_T4100PP_postproc_cur	);

	// DonR 13Nov2018 CR #15919: Record current table size.
	REMEMBER_ISOLATION;
	SET_ISOLATION_DIRTY;

	ExecSQL (	MAIN_DB, AS400SRV_T4100PP_READ_coupons_table_size,
				&total_rows, END_OF_ARG_LIST						);

	// DonR 07Sep2020 BUG FIX: I left out the ROW_NOT_DELETED parameter, so
	// this SQL operation was failing consistently. Ooops!
	ExecSQL (	MAIN_DB, AS400SRV_T4100PP_READ_coupons_active_size,
				&active_rows, &ROW_NOT_DELETED,	END_OF_ARG_LIST			);

	RESTORE_ISOLATION;
	// DonR 13Nov2018 CR #15919 end.


	GerrLogMini (GerrId,
				 "Postprocess 4100: %d rows logically deleted, retcode = %d.",
				 rows_deleted, retcode);

	return retcode;

}	// End of msg 4100 (coupons) postprocessor.



// -------------------------------------------------------------------------
//
//       Handler for message 4101 - update/insert to SUBSIDY_MESSAGES
//
//            No content comparison - this table is sent non-incrementally.
//
// -------------------------------------------------------------------------
static int data_4101_handler (TConnection	*p_connect,
							  int			data_len)
{
	// local variables
	char	*PosInBuff;			// current pos in data buffer
	int		ReadInput;			// amount of Bytesread from buffer
	int		retcode;			// code returned from function
	int		n_retries;			// retrying counter
	int		row_was_inserted = 0;

	// DB variables
	short	v_subsidy_code;
	char	v_short_desc	[10 + 1];
	char	v_long_desc		[25 + 1];
	int		v_update_date;
	int		v_update_time;
	int		v_as400_batch_date;
	int		v_as400_batch_time;


	// Calculate virtual time. Note that for this download the time is based
	// on the real time, adjusted only to ensure that we don't have too many
	// items with the exact same as-of time.
	v_update_date  = v_as400_batch_date = (download_start_date > 0) ? download_start_date : GetDate();
	v_update_time  = v_as400_batch_time = (download_start_time > 0) ? download_start_time : GetTime();
	v_update_time += (download_row_count++ / VIRTUAL_PORTION);


	// read input buffer
	INIT_ERROR_DETECTION_MECHANISM;
	PosInBuff = glbMsgBuffer;

	GET_SHORT	(v_subsidy_code	,  4);
	GET_HEB_STR	(v_short_desc	, 10);
	GET_HEB_STR	(v_long_desc	, 25);
	
	// Verify all data was read.
	ReadInput = PosInBuff - glbMsgBuffer;
	StaticAssert (ReadInput == data_len);


	// Perform up to SQL_UPDATE_RETRIES retries to overcome access conflicts.
	for (n_retries = 0, retcode = ALL_RETRIES_WERE_USED;
		 n_retries < SQL_UPDATE_RETRIES;
		 n_retries++)
	{
		// If this isn't the first iteration, sleep for a while.
		if (n_retries > 0)
		{
			sleep (SLEEP_TIME_BETWEEN_RETRIES);
		}


		// The most frequent operation will be a refresh/update of an existing row; the AS/400
		// batch date and time need to be updated even if nothing has changed, so the postprocessor
		// knows not to delete the row.
		ExecSQL (	MAIN_DB, AS400SRV_T4101_UPD_subsidy_messages,
					&v_short_desc,			&v_long_desc,
					&v_update_date,			&v_update_time,
					&v_as400_batch_date,	&v_as400_batch_time,
					&v_subsidy_code,		END_OF_ARG_LIST			);

		// If the row isn't already in the database, we need to insert it.
		if (SQLERR_code_cmp (SQLERR_no_rows_affected) == MAC_TRUE)
		{
			ExecSQL (	MAIN_DB, AS400SRV_T4101_INS_subsidy_messages,
						&v_subsidy_code,		&v_short_desc,
						&v_long_desc,			&v_update_date,
						&v_update_time,			&v_as400_batch_date,
						&v_as400_batch_time,	END_OF_ARG_LIST			);

			row_was_inserted = 1;
		}

		if (SQLERR_code_cmp (SQLERR_access_conflict) == MAC_TRUE)
		{
			retcode = ALL_RETRIES_WERE_USED;
			continue;
		}
		else
		{
			if (SQLERR_error_test ())
			{
				retcode = SQL_ERROR_OCCURED;
				break;
			}
			else
			// Fourth possibility: the refresh succeeded, so we don't have to do anything else.
			{
				retcode = MSG_HANDLER_SUCCEEDED;

				if (row_was_inserted)
					rows_added++;
				else
					rows_updated++;

				break;
			}	// Update or insert succeeded.
		}	// Not an access conflict.
	}	// Retry Loop.


	if (n_retries == SQL_UPDATE_RETRIES)
	{
		WRITE_ACCESS_CONFLICT_TO_LOG ("4101");
	}

	return retcode;

}	// End of msg 4101 (subsidy_messages) handler.



// -------------------------------------------------------------------------
//
//       Post-processor for message 4101 - delete SUBSIDY_MESSAGES
//			rows that are no longer in the AS/400 full-table download.
//
// -------------------------------------------------------------------------
static int postprocess_4101 (TConnection	*p_connect)
{
	// local variables
	bool	stop_retrying;	// if true - return from function
	int		rows_deleted	= 0;
	int		retcode;		// code returned from function
	int		n_retries;		// retrying counter
	int		db_changed_flg;

	// DB variables
	short	v_subsidy_code;
	int		v_as400_batch_date;
	int		v_as400_batch_time;


	// Initialize variables.
	v_as400_batch_date	= download_start_date;
	v_as400_batch_time	= download_start_time;


	retcode			= ALL_RETRIES_WERE_USED;
	stop_retrying	= false;


	DeclareCursorInto (	MAIN_DB, AS400SRV_T4101PP_postproc_cur,
						&v_subsidy_code,
						&v_as400_batch_date,	&v_as400_batch_date,
						&v_as400_batch_time,	END_OF_ARG_LIST			);

	OpenCursor (	MAIN_DB, AS400SRV_T4101PP_postproc_cur	);

	if (SQLERR_error_test ())
	{
		retcode = SQL_ERROR_OCCURED;
//		break;
	}

	while (1)
	{
		FetchCursor (	MAIN_DB, AS400SRV_T4101PP_postproc_cur	);

		if (SQLERR_code_cmp (SQLERR_end_of_fetch) == MAC_TRUE)
		{
			retcode = MSG_HANDLER_SUCCEEDED;	// In case there is nothing that needs to be deleted.
			break;
		}
		else
		{
			if (SQLERR_error_test ())
			{
				retcode = SQL_ERROR_OCCURED;
				break;
			}
		}

		// Nothing fancy here - just a physical delete.
		ExecSQL (	MAIN_DB, AS400SRV_T4101PP_DEL_subsidy_messages	);

		HANDLE_DELETE_SQL_CODE ("postprocess_4101", p_connect, &retcode, &db_changed_flg);

		rows_deleted++;
	}	// Cursor-scrolling loop.

	CloseCursor (	MAIN_DB, AS400SRV_T4101PP_postproc_cur	);


	// DonR 13Nov2018 CR #15919: Record current table size.
	REMEMBER_ISOLATION;
	SET_ISOLATION_DIRTY;

	ExecSQL (	MAIN_DB, AS400SRV_T4101PP_READ_subsidy_messages_table_size,
				&total_rows, END_OF_ARG_LIST								);

	active_rows = total_rows;

	RESTORE_ISOLATION;
	// DonR 13Nov2018 CR #15919 end.


	GerrLogMini (GerrId,
				 "Postprocess 4101: %d rows logically deleted, retcode = %d.",
				 rows_deleted, retcode);

	return retcode;

}	// End of msg 4101 (subsidy_messages) postprocessor.



// -------------------------------------------------------------------------
//
//       Handler for message 4102 - update/insert to TIKRA_TYPE
//
//            No content comparison - this table is sent non-incrementally.
//
// -------------------------------------------------------------------------
static int data_4102_handler (TConnection	*p_connect,
							  int			data_len)
{
	// local variables
	char	*PosInBuff;			// current pos in data buffer
	int		ReadInput;			// amount of Bytesread from buffer
	int		retcode;			// code returned from function
	int		n_retries;			// retrying counter
	int		row_was_inserted = 0;

	// DB variables
	char	v_tikra_type_code	[ 1 + 1];
	char	v_short_desc		[15 + 1];
	char	v_long_desc			[30 + 1];
	int		v_update_date;
	int		v_update_time;
	int		v_as400_batch_date;
	int		v_as400_batch_time;


	// Calculate virtual time. Note that for this download the time is based
	// on the real time, adjusted only to ensure that we don't have too many
	// items with the exact same as-of time.
	v_update_date  = v_as400_batch_date = (download_start_date > 0) ? download_start_date : GetDate();
	v_update_time  = v_as400_batch_time = (download_start_time > 0) ? download_start_time : GetTime();
	v_update_time += (download_row_count++ / VIRTUAL_PORTION);


	// read input buffer
	INIT_ERROR_DETECTION_MECHANISM;
	PosInBuff = glbMsgBuffer;

	GET_STRING	(v_tikra_type_code	,  1);
	GET_HEB_STR	(v_short_desc		, 15);
	GET_HEB_STR	(v_long_desc		, 30);
	
	// Verify all data was read.
	ReadInput = PosInBuff - glbMsgBuffer;
	StaticAssert (ReadInput == data_len);


	// Perform up to SQL_UPDATE_RETRIES retries to overcome access conflicts.
	for (n_retries = 0, retcode = ALL_RETRIES_WERE_USED;
		 n_retries < SQL_UPDATE_RETRIES;
		 n_retries++)
	{
		// If this isn't the first iteration, sleep for a while.
		if (n_retries > 0)
		{
			sleep (SLEEP_TIME_BETWEEN_RETRIES);
		}


		// The most frequent operation will be a refresh/update of an existing row; the AS/400
		// batch date and time need to be updated even if nothing has changed, so the postprocessor
		// knows not to delete the row.
		ExecSQL (	MAIN_DB, AS400SRV_T4102_UPD_tikra_type,
					&v_short_desc,			&v_long_desc,
					&v_update_date,			&v_update_time,
					&v_as400_batch_date,	&v_as400_batch_time,
					&v_tikra_type_code,		END_OF_ARG_LIST			);

		// If the row isn't already in the database, we need to insert it.
		if (SQLERR_code_cmp (SQLERR_no_rows_affected) == MAC_TRUE)
		{
			ExecSQL (	MAIN_DB, AS400SRV_T4102_INS_tikra_type,
						&v_tikra_type_code,		&v_short_desc,
						&v_long_desc,			&v_update_date,
						&v_update_time,			&v_as400_batch_date,
						&v_as400_batch_time,	END_OF_ARG_LIST			);

			row_was_inserted = 1;
		}

		if (SQLERR_code_cmp (SQLERR_access_conflict) == MAC_TRUE)
		{
			retcode = ALL_RETRIES_WERE_USED;
			continue;
		}
		else
		{
			if (SQLERR_error_test ())
			{
				retcode = SQL_ERROR_OCCURED;
				break;
			}
			else
			// Fourth possibility: the refresh succeeded, so we don't have to do anything else.
			{
				retcode = MSG_HANDLER_SUCCEEDED;

				if (row_was_inserted)
					rows_added++;
				else
					rows_updated++;

				break;
			}	// Update or insert succeeded.
		}	// Not an access conflict.
	}	// Retry Loop.


	if (n_retries == SQL_UPDATE_RETRIES)
	{
		WRITE_ACCESS_CONFLICT_TO_LOG ("4102");
	}

	return retcode;

}	// End of msg 4102 (tikra_type) handler.



// -------------------------------------------------------------------------
//
//       Post-processor for message 4102 - delete TIKRA_TYPE
//			rows that are no longer in the AS/400 full-table download.
//
// -------------------------------------------------------------------------
static int postprocess_4102 (TConnection	*p_connect)
{
	// local variables
	bool	stop_retrying;	// if true - return from function
	int		rows_deleted	= 0;
	int		retcode;		// code returned from function
	int		n_retries;		// retrying counter
	int		db_changed_flg;

	// DB variables
	char	v_tikra_type_code	[ 1 + 1];
	int		v_as400_batch_date;
	int		v_as400_batch_time;


	// Initialize variables.
	v_as400_batch_date	= download_start_date;
	v_as400_batch_time	= download_start_time;


	retcode			= ALL_RETRIES_WERE_USED;
	stop_retrying	= false;


	DeclareCursorInto (	MAIN_DB, AS400SRV_T4102PP_postproc_cur,
						&v_tikra_type_code,
						&v_as400_batch_date,	&v_as400_batch_date,
						&v_as400_batch_time,	END_OF_ARG_LIST			);

	OpenCursor (	MAIN_DB, AS400SRV_T4102PP_postproc_cur	);

	if (SQLERR_error_test ())
	{
		retcode = SQL_ERROR_OCCURED;
//		break;
	}

	while (1)
	{
		FetchCursor (	MAIN_DB, AS400SRV_T4102PP_postproc_cur	);

		if (SQLERR_code_cmp (SQLERR_end_of_fetch) == MAC_TRUE)
		{
			retcode = MSG_HANDLER_SUCCEEDED;	// In case there is nothing that needs to be deleted.
			break;
		}
		else
		{
			if (SQLERR_error_test ())
			{
				retcode = SQL_ERROR_OCCURED;
				break;
			}
		}

		// Nothing fancy here - just a physical delete.
		ExecSQL (	MAIN_DB, AS400SRV_T4102PP_DEL_tikra_type	);

		HANDLE_DELETE_SQL_CODE ("postprocess_4102", p_connect, &retcode, &db_changed_flg);

		rows_deleted++;
	}	// Cursor-scrolling loop.

	CloseCursor (	MAIN_DB, AS400SRV_T4102PP_postproc_cur	);


	// DonR 13Nov2018 CR #15919: Record current table size.
	REMEMBER_ISOLATION;
	SET_ISOLATION_DIRTY;

	ExecSQL (	MAIN_DB, AS400SRV_T4102PP_READ_tikra_type_table_size,
				&total_rows, END_OF_ARG_LIST							);

	active_rows = total_rows;

	RESTORE_ISOLATION;
	// DonR 13Nov2018 CR #15919 end.


	GerrLogMini (GerrId,
				 "Postprocess 4102: %d rows deleted, retcode = %d.",
				 rows_deleted, retcode);

	return retcode;

}	// End of msg 4102 (tikra_type) postprocessor.



// -------------------------------------------------------------------------
//
//       Handler for message 4103 - update/insert to CREDIT_REASONS
//
//            No content comparison - this table is sent non-incrementally.
//
// -------------------------------------------------------------------------
static int data_4103_handler (TConnection	*p_connect,
							  int			data_len)
{
	// local variables
	char	*PosInBuff;			// current pos in data buffer
	int		ReadInput;			// amount of Bytesread from buffer
	int		retcode;			// code returned from function
	int		n_retries;			// retrying counter
	int		row_was_inserted = 0;

	// DB variables
	short	v_credit_rsn_code;
	char	v_short_desc		[30 + 1];
	char	v_long_desc			[50 + 1];
	int		v_update_date;
	int		v_update_time;
	int		v_as400_batch_date;
	int		v_as400_batch_time;


	// Calculate virtual time. Note that for this download the time is based
	// on the real time, adjusted only to ensure that we don't have too many
	// items with the exact same as-of time.
	v_update_date  = v_as400_batch_date = (download_start_date > 0) ? download_start_date : GetDate();
	v_update_time  = v_as400_batch_time = (download_start_time > 0) ? download_start_time : GetTime();
	v_update_time += (download_row_count++ / VIRTUAL_PORTION);


	// read input buffer
	INIT_ERROR_DETECTION_MECHANISM;
	PosInBuff = glbMsgBuffer;

	GET_SHORT	(v_credit_rsn_code	,  4);
	GET_HEB_STR	(v_short_desc		, 30);
	GET_HEB_STR	(v_long_desc		, 50);
	
	// Verify all data was read.
	ReadInput = PosInBuff - glbMsgBuffer;
	StaticAssert (ReadInput == data_len);


	// Perform up to SQL_UPDATE_RETRIES retries to overcome access conflicts.
	for (n_retries = 0, retcode = ALL_RETRIES_WERE_USED;
		 n_retries < SQL_UPDATE_RETRIES;
		 n_retries++)
	{
		// If this isn't the first iteration, sleep for a while.
		if (n_retries > 0)
		{
			sleep (SLEEP_TIME_BETWEEN_RETRIES);
		}


		// The most frequent operation will be a refresh/update of an existing row; the AS/400
		// batch date and time need to be updated even if nothing has changed, so the postprocessor
		// knows not to delete the row.
		ExecSQL (	MAIN_DB, AS400SRV_T4103_UPD_credit_reasons,
					&v_short_desc,			&v_long_desc,
					&v_update_date,			&v_update_time,
					&v_as400_batch_date,	&v_as400_batch_time,
					&v_credit_rsn_code,		END_OF_ARG_LIST			);

		// If the row isn't already in the database, we need to insert it.
		if (SQLERR_code_cmp (SQLERR_no_rows_affected) == MAC_TRUE)
		{
			ExecSQL (	MAIN_DB, AS400SRV_T4103_INS_credit_reasons,
						&v_credit_rsn_code,		&v_short_desc,
						&v_long_desc,			&v_update_date,
						&v_update_time,			&v_as400_batch_date,
						&v_as400_batch_time,	END_OF_ARG_LIST			);

			row_was_inserted = 1;
		}

		if (SQLERR_code_cmp (SQLERR_access_conflict) == MAC_TRUE)
		{
			retcode = ALL_RETRIES_WERE_USED;
			continue;
		}
		else
		{
			if (SQLERR_error_test ())
			{
				retcode = SQL_ERROR_OCCURED;
				break;
			}
			else
			// Fourth possibility: the refresh succeeded, so we don't have to do anything else.
			{
				retcode = MSG_HANDLER_SUCCEEDED;

				if (row_was_inserted)
					rows_added++;
				else
					rows_updated++;

				break;
			}	// Update or insert succeeded.
		}	// Not an access conflict.
	}	// Retry Loop.


	if (n_retries == SQL_UPDATE_RETRIES)
	{
		WRITE_ACCESS_CONFLICT_TO_LOG ("4103");
	}

	return retcode;

}	// End of msg 4103 (credit_reasons) handler.



// -------------------------------------------------------------------------
//
//       Post-processor for message 4103 - delete CREDIT_REASONS
//			rows that are no longer in the AS/400 full-table download.
//
// -------------------------------------------------------------------------
static int postprocess_4103 (TConnection	*p_connect)
{
	// local variables
	bool	stop_retrying;	// if true - return from function
	int		rows_deleted	= 0;
	int		retcode;		// code returned from function
	int		n_retries;		// retrying counter
	int		db_changed_flg;

	// DB variables
	short	v_credit_rsn_code;
	int		v_as400_batch_date;
	int		v_as400_batch_time;


	// Initialize variables.
	v_as400_batch_date	= download_start_date;
	v_as400_batch_time	= download_start_time;


	retcode			= ALL_RETRIES_WERE_USED;
	stop_retrying	= false;


	DeclareCursorInto (	MAIN_DB, AS400SRV_T4103PP_postproc_cur,
						&v_credit_rsn_code,
						&v_as400_batch_date,	&v_as400_batch_date,
						&v_as400_batch_time,	END_OF_ARG_LIST			);

	OpenCursor (	MAIN_DB, AS400SRV_T4103PP_postproc_cur	);

	if (SQLERR_error_test ())
	{
		retcode = SQL_ERROR_OCCURED;
//		break;
	}

	while (1)
	{
		FetchCursor (	MAIN_DB, AS400SRV_T4103PP_postproc_cur	);

		if (SQLERR_code_cmp (SQLERR_end_of_fetch) == MAC_TRUE)
		{
			retcode = MSG_HANDLER_SUCCEEDED;	// In case there is nothing that needs to be deleted.
			break;
		}
		else
		{
			if (SQLERR_error_test ())
			{
				retcode = SQL_ERROR_OCCURED;
				break;
			}
		}

		// Nothing fancy here - just a physical delete.
		ExecSQL (	MAIN_DB, AS400SRV_T4103PP_DEL_credit_reasons	);

		HANDLE_DELETE_SQL_CODE ("postprocess_4103", p_connect, &retcode, &db_changed_flg);

		rows_deleted++;
	}	// Cursor-scrolling loop.

	CloseCursor (	MAIN_DB, AS400SRV_T4103PP_postproc_cur	);


	// DonR 13Nov2018 CR #15919: Record current table size.
	REMEMBER_ISOLATION;
	SET_ISOLATION_DIRTY;

	ExecSQL (	MAIN_DB, AS400SRV_T4103PP_READ_credit_reasons_table_size,
				&total_rows, END_OF_ARG_LIST								);

	active_rows = total_rows;

	RESTORE_ISOLATION;
	// DonR 13Nov2018 CR #15919 end.


	GerrLogMini (GerrId,
				 "Postprocess 4103: %d rows logically deleted, retcode = %d.",
				 rows_deleted, retcode);

	return retcode;

}	// End of msg 4103 (credit_reasons) postprocessor.



// -------------------------------------------------------------------------
//
//       Handler for message 4104 - update/insert to HMO_MEMBERSHIP
//
//            No content comparison - this table is sent non-incrementally.
//
// -------------------------------------------------------------------------
static int data_4104_handler (TConnection	*p_connect,
							  int			data_len)
{
	// local variables
	char	*PosInBuff;			// current pos in data buffer
	int		ReadInput;			// amount of Bytesread from buffer
	int		retcode;			// code returned from function
	int		n_retries;			// retrying counter
	int		row_was_inserted = 0;

	// DB variables
	short	v_org_code;
	char	v_description	[15 + 1];
	int		v_update_date;
	int		v_update_time;
	int		v_as400_batch_date;
	int		v_as400_batch_time;


	// Calculate virtual time. Note that for this download the time is based
	// on the real time, adjusted only to ensure that we don't have too many
	// items with the exact same as-of time.
	v_update_date  = v_as400_batch_date = (download_start_date > 0) ? download_start_date : GetDate();
	v_update_time  = v_as400_batch_time = (download_start_time > 0) ? download_start_time : GetTime();
	v_update_time += (download_row_count++ / VIRTUAL_PORTION);


	// read input buffer
	INIT_ERROR_DETECTION_MECHANISM;
	PosInBuff = glbMsgBuffer;

	GET_SHORT	(v_org_code		,  4);
	GET_HEB_STR	(v_description	, 15);
	
	// Verify all data was read.
	ReadInput = PosInBuff - glbMsgBuffer;
	StaticAssert (ReadInput == data_len);


	// Perform up to SQL_UPDATE_RETRIES retries to overcome access conflicts.
	for (n_retries = 0, retcode = ALL_RETRIES_WERE_USED;
		 n_retries < SQL_UPDATE_RETRIES;
		 n_retries++)
	{
		// If this isn't the first iteration, sleep for a while.
		if (n_retries > 0)
		{
			sleep (SLEEP_TIME_BETWEEN_RETRIES);
		}


		// The most frequent operation will be a refresh/update of an existing row; the AS/400
		// batch date and time need to be updated even if nothing has changed, so the postprocessor
		// knows not to delete the row.
		ExecSQL (	MAIN_DB, AS400SRV_T4104_UPD_hmo_membership,
					&v_description,			&v_update_date,
					&v_update_time,			&v_as400_batch_date,
					&v_as400_batch_time,
					&v_org_code,			END_OF_ARG_LIST			);

		// If the row isn't already in the database, we need to insert it.
		if (SQLERR_code_cmp (SQLERR_no_rows_affected) == MAC_TRUE)
		{
			ExecSQL (	MAIN_DB, AS400SRV_T4104_INS_hmo_membership,
						&v_org_code,			&v_description,
						&v_update_date,			&v_update_time,
						&v_as400_batch_date,	&v_as400_batch_time,
						END_OF_ARG_LIST									);

			row_was_inserted = 1;
		}

		if (SQLERR_code_cmp (SQLERR_access_conflict) == MAC_TRUE)
		{
			retcode = ALL_RETRIES_WERE_USED;
			continue;
		}
		else
		{
			if (SQLERR_error_test ())
			{
				retcode = SQL_ERROR_OCCURED;
				break;
			}
			else
			// Fourth possibility: the refresh succeeded, so we don't have to do anything else.
			{
				retcode = MSG_HANDLER_SUCCEEDED;

				if (row_was_inserted)
					rows_added++;
				else
					rows_updated++;

				break;
			}	// Update or insert succeeded.
		}	// Not an access conflict.
	}	// Retry Loop.


	if (n_retries == SQL_UPDATE_RETRIES)
	{
		WRITE_ACCESS_CONFLICT_TO_LOG ("4104");
	}

	return retcode;

}	// End of msg 4104 (hmo_membership) handler.



// -------------------------------------------------------------------------
//
//       Post-processor for message 4104 - delete HMO_MEMBERSHIP
//			rows that are no longer in the AS/400 full-table download.
//
// -------------------------------------------------------------------------
static int postprocess_4104 (TConnection	*p_connect)
{
	// local variables
	bool	stop_retrying;	// if true - return from function
	int		rows_deleted	= 0;
	int		retcode;		// code returned from function
	int		n_retries;		// retrying counter
	int		db_changed_flg;

	// DB variables
	short	v_org_code;
	int		v_as400_batch_date;
	int		v_as400_batch_time;


	// Initialize variables.
	v_as400_batch_date	= download_start_date;
	v_as400_batch_time	= download_start_time;


	retcode			= ALL_RETRIES_WERE_USED;
	stop_retrying	= false;


	DeclareCursorInto (	MAIN_DB, AS400SRV_T4104PP_postproc_cur,
						&v_org_code,
						&v_as400_batch_date,	&v_as400_batch_date,
						&v_as400_batch_time,	END_OF_ARG_LIST			);

	OpenCursor (	MAIN_DB, AS400SRV_T4104PP_postproc_cur	);

	if (SQLERR_error_test ())
	{
		retcode = SQL_ERROR_OCCURED;
//		break;
	}

	while (1)
	{
		FetchCursor (	MAIN_DB, AS400SRV_T4104PP_postproc_cur	);

		if (SQLERR_code_cmp (SQLERR_end_of_fetch) == MAC_TRUE)
		{
			retcode = MSG_HANDLER_SUCCEEDED;	// In case there is nothing that needs to be deleted.
			break;
		}
		else
		{
			if (SQLERR_error_test ())
			{
				retcode = SQL_ERROR_OCCURED;
				break;
			}
		}

		// Nothing fancy here - just a physical delete.
		ExecSQL (	MAIN_DB, AS400SRV_T4104PP_DEL_hmo_membership	);

		HANDLE_DELETE_SQL_CODE ("postprocess_4104", p_connect, &retcode, &db_changed_flg);

		rows_deleted++;
	}	// Cursor-scrolling loop.

	CloseCursor (	MAIN_DB, AS400SRV_T4104PP_postproc_cur	);


	// DonR 13Nov2018 CR #15919: Record current table size.
	REMEMBER_ISOLATION;
	SET_ISOLATION_DIRTY;

	ExecSQL (	MAIN_DB, AS400SRV_T4104PP_READ_hmo_membership_table_size,
				&total_rows, END_OF_ARG_LIST								);

	active_rows = total_rows;

	RESTORE_ISOLATION;
	// DonR 13Nov2018 CR #15919 end.


	GerrLogMini (GerrId,
				 "Postprocess 4104: %d rows logically deleted, retcode = %d.",
				 rows_deleted, retcode);

	return retcode;

}	// End of msg 4104 (hmo_membership) postprocessor.



// -------------------------------------------------------------------------
//
//       Handler for message 4105 - update/insert to DRUG_SHAPE
//
//            No content comparison - this table is sent non-incrementally.
//
// -------------------------------------------------------------------------
static int data_4105_handler (TConnection	*p_connect,
							  int			data_len)
{
	// local variables
	char	*PosInBuff;			// current pos in data buffer
	int		ReadInput;			// amount of Bytesread from buffer
	int		retcode;			// code returned from function
	int		n_retries;			// retrying counter
	int		row_was_inserted = 0;

	// DB variables
	short	drug_shape_code;
	char	drug_shape_desc	[ 4 + 1];
	char	shape_name_eng	[25 + 1];
	char	shape_name_heb	[25 + 1];
	short	calc_op_by_volume;
	short	home_delivery_ok;
	short	calc_actual_usage;
	short	compute_duration;	//Marianna 10Aug2023:User Story #469361
	int		v_update_date;
	int		v_update_time;
	int		v_as400_batch_date;
	int		v_as400_batch_time;


	// Calculate virtual time. Note that for this download the time is based
	// on the real time, adjusted only to ensure that we don't have too many
	// items with the exact same as-of time.
	v_update_date  = v_as400_batch_date = (download_start_date > 0) ? download_start_date : GetDate();
	v_update_time  = v_as400_batch_time = (download_start_time > 0) ? download_start_time : GetTime();
	v_update_time += (download_row_count++ / VIRTUAL_PORTION);


	// read input buffer
	INIT_ERROR_DETECTION_MECHANISM;
	PosInBuff = glbMsgBuffer;

	GET_SHORT	(drug_shape_code	,	 4);
	GET_STRING	(drug_shape_desc	,	 4);
	GET_STRING	(shape_name_eng		,	25);
	GET_HEB_STR	(shape_name_heb		,	25);
	GET_SHORT	(calc_op_by_volume	,	 1);
	GET_SHORT	(home_delivery_ok	,	 1);
	GET_SHORT	(calc_actual_usage	,	 1);
	GET_SHORT	(compute_duration	,	 1);	//Marianna 10Aug2023:User Story #469361	

	
	// Verify all data was read.
	ReadInput = PosInBuff - glbMsgBuffer;
	StaticAssert (ReadInput == data_len);


	// Perform up to SQL_UPDATE_RETRIES retries to overcome access conflicts.
	for (n_retries = 0, retcode = ALL_RETRIES_WERE_USED;
		 n_retries < SQL_UPDATE_RETRIES;
		 n_retries++)
	{
		// If this isn't the first iteration, sleep for a while.
		if (n_retries > 0)
		{
			sleep (SLEEP_TIME_BETWEEN_RETRIES);
		}


		// The most frequent operation will be a refresh/update of an existing row; the AS/400
		// batch date and time need to be updated even if nothing has changed, so the postprocessor
		// knows not to delete the row.
		// DonR 10Feb2020: Added update of del_flg to ROW_NOT_DELETED.
		ExecSQL (	MAIN_DB, AS400SRV_T4105_UPD_drug_shape,
					&drug_shape_desc,		&shape_name_eng,
					&shape_name_heb,		&calc_op_by_volume,
					&home_delivery_ok,		&calc_actual_usage,
					&compute_duration,		&ROW_NOT_DELETED,
					&v_update_date,			&v_update_time,
					&v_as400_batch_date,	&v_as400_batch_time,	
					&drug_shape_code,		END_OF_ARG_LIST			);

		// If the row isn't already in the database, we need to insert it.
		// DonR 10Feb2020: Added explicit setting of del_flg to ROW_NOT_DELETED.
		if (SQLERR_code_cmp (SQLERR_no_rows_affected) == MAC_TRUE)
		{
			ExecSQL (	MAIN_DB, AS400SRV_T4105_INS_drug_shape,
						&drug_shape_code,		&drug_shape_desc,
						&shape_name_eng,		&shape_name_heb,
						&calc_op_by_volume,		&home_delivery_ok,
						&calc_actual_usage,		&compute_duration,	
						&ROW_NOT_DELETED,		&v_update_date,			
						&v_update_time,			&v_as400_batch_date,	
						&v_as400_batch_time,	END_OF_ARG_LIST									);

			row_was_inserted = 1;
		}

		if (SQLERR_code_cmp (SQLERR_access_conflict) == MAC_TRUE)
		{
			retcode = ALL_RETRIES_WERE_USED;
			continue;
		}
		else
		{
			if (SQLERR_error_test ())
			{
				retcode = SQL_ERROR_OCCURED;
				break;
			}
			else
			// Fourth possibility: the refresh succeeded, so we don't have to do anything else.
			{
				retcode = MSG_HANDLER_SUCCEEDED;

				if (row_was_inserted)
					rows_added++;
				else
					rows_updated++;

				break;
			}	// Update or insert succeeded.
		}	// Not an access conflict.
	}	// Retry Loop.


	if (n_retries == SQL_UPDATE_RETRIES)
	{
		WRITE_ACCESS_CONFLICT_TO_LOG ("4105");
	}

	return retcode;

}	// End of msg 4105 (drug_shape) handler.



// -------------------------------------------------------------------------
//
//       Post-processor for message 4105 - delete DRUG_SHAPE
//			rows that are no longer in the AS/400 full-table download.
//
// -------------------------------------------------------------------------
static int postprocess_4105 (TConnection	*p_connect)
{
	// local variables
	bool	stop_retrying;	// if true - return from function
	int		rows_deleted	= 0;
	int		retcode;		// code returned from function
	int		n_retries;		// retrying counter
	int		db_changed_flg;

	// DB variables
	short	drug_shape_code;
	int		v_as400_batch_date;
	int		v_as400_batch_time;


	// Initialize variables.
	v_as400_batch_date	= download_start_date;
	v_as400_batch_time	= download_start_time;


	retcode			= ALL_RETRIES_WERE_USED;
	stop_retrying	= false;


	DeclareCursorInto (	MAIN_DB, AS400SRV_T4105PP_postproc_cur,
						&drug_shape_code,
						&v_as400_batch_date,	&v_as400_batch_date,
						&v_as400_batch_time,	END_OF_ARG_LIST			);

	OpenCursor (	MAIN_DB, AS400SRV_T4105PP_postproc_cur	);

	if (SQLERR_error_test ())
	{
		retcode = SQL_ERROR_OCCURED;
//		break;
	}

	while (1)
	{
		FetchCursor (	MAIN_DB, AS400SRV_T4105PP_postproc_cur	);

		if (SQLERR_code_cmp (SQLERR_end_of_fetch) == MAC_TRUE)
		{
			retcode = MSG_HANDLER_SUCCEEDED;	// In case there is nothing that needs to be deleted.
			break;
		}
		else
		{
			if (SQLERR_error_test ())
			{
				retcode = SQL_ERROR_OCCURED;
				break;
			}
		}

		// Nothing fancy here - just a physical delete.
		// TEMPORARY FOR TESTING ONLY: SET DELETION FLAG INSTEAD OF PERFORMING A PHYSICAL DELETION.
		// DonR 10Feb2020: For ODBC version, switch to a real deletion.
		ExecSQL (	MAIN_DB, AS400SRV_T4105PP_DEL_drug_shape	);

		HANDLE_DELETE_SQL_CODE ("postprocess_4105", p_connect, &retcode, &db_changed_flg);

		rows_deleted++;
	}	// Cursor-scrolling loop.

	CloseCursor (	MAIN_DB, AS400SRV_T4105PP_postproc_cur	);


	// DonR 13Nov2018 CR #15919: Record current table size.
	REMEMBER_ISOLATION;
	SET_ISOLATION_DIRTY;

	ExecSQL (	MAIN_DB, AS400SRV_T4105PP_READ_drug_shape_table_size,
				&total_rows, END_OF_ARG_LIST							);

	active_rows = total_rows;

	//Marianna 10Aug2023 :User Story #469361
	ExecSQL (	MAIN_DB, AS400SRV_UPD_drug_list_compute_duration, END_OF_ARG_LIST	);

	if (SQLCODE == 0) 
		GerrLogMini (GerrId, "Postprocess 4105: Updated compute_duration for %d drugs.", sqlca.sqlerrd [2]);

	RESTORE_ISOLATION;
	// DonR 13Nov2018 CR #15919 end.

	GerrLogMini (GerrId,
				 "Postprocess 4105: %d rows logically deleted, retcode = %d.",
				 rows_deleted, retcode);

	return retcode;
}	// End of msg 4105 (drug_shape) postprocessor.



// -------------------------------------------------------------------------
//
//       Handler for message 4106 - update/insert to DRUG_DIAGNOSES
//
//            No content comparison - this table is sent non-incrementally.
//
// -------------------------------------------------------------------------
static int data_4106_handler (TConnection	*p_connect,
							  int			data_len)
{
	// local variables
	char	*PosInBuff;			// current pos in data buffer
	int		ReadInput;			// amount of Bytesread from buffer
	int		retcode;			// code returned from function
	int		n_retries;			// retrying counter
	int		row_was_inserted = 0;

	// DB variables
	int		largo_code;
	short	illness_code;
	int		diagnosis_code;
	int		v_as400_batch_date;
	int		v_as400_batch_time;


//	// Calculate virtual time. Note that for this download the time is based
//	// on the real time, adjusted only to ensure that we don't have too many
//	// items with the exact same as-of time.
//	v_update_date  = v_as400_batch_date = (download_start_date > 0) ? download_start_date : GetDate();
//	v_update_time  = v_as400_batch_time = (download_start_time > 0) ? download_start_time : GetTime();
//	v_update_time += (download_row_count++ / VIRTUAL_PORTION);

	// Initialize variables.
	v_as400_batch_date	= download_start_date;
	v_as400_batch_time	= download_start_time;

	// read input buffer
	INIT_ERROR_DETECTION_MECHANISM;
	PosInBuff = glbMsgBuffer;

	GET_LONG	(largo_code		,  9);
	GET_SHORT	(illness_code	,  4);
	GET_LONG	(diagnosis_code	,  6);
	
	// Verify all data was read.
	ReadInput = PosInBuff - glbMsgBuffer;
	StaticAssert (ReadInput == data_len);


	// Perform up to SQL_UPDATE_RETRIES retries to overcome access conflicts.
	for (n_retries = 0, retcode = ALL_RETRIES_WERE_USED;
		 n_retries < SQL_UPDATE_RETRIES;
		 n_retries++)
	{
		// If this isn't the first iteration, sleep for a while.
		if (n_retries > 0)
		{
			sleep (SLEEP_TIME_BETWEEN_RETRIES);
		}


		// The most frequent operation will be a refresh/update of an existing row; the AS/400
		// batch date and time need to be updated even if nothing has changed, so the postprocessor
		// knows not to delete the row.
		// Note that we're assuming that a given Diagnosis Code can exist only for a single major
		// disease (cancer, Gaucher's, etc.) - so we can treat Largo Code + Diagnosis Code as if
		// it were a unique index, even though it wasn't defined that way.
		ExecSQL (	MAIN_DB, AS400SRV_T4106_UPD_drug_diagnoses,
					&illness_code,			&v_as400_batch_date,
					&v_as400_batch_time,
					&largo_code,			&diagnosis_code,
					END_OF_ARG_LIST									);

		// If the row isn't already in the database, we need to insert it.
		if (SQLERR_code_cmp (SQLERR_no_rows_affected) == MAC_TRUE)
		{
			ExecSQL (	MAIN_DB, AS400SRV_T4106_INS_drug_diagnoses,
						&largo_code,			&illness_code,
						&diagnosis_code,		&v_as400_batch_date,
						&v_as400_batch_time,	END_OF_ARG_LIST			);

			row_was_inserted = 1;
		}

		if (SQLERR_code_cmp (SQLERR_access_conflict) == MAC_TRUE)
		{
			retcode = ALL_RETRIES_WERE_USED;
			continue;
		}
		else
		{
			if (SQLERR_error_test ())
			{
				retcode = SQL_ERROR_OCCURED;
				break;
			}
			else
			// Fourth possibility: the refresh succeeded, so we don't have to do anything else.
			{
				retcode = MSG_HANDLER_SUCCEEDED;

				if (row_was_inserted)
					rows_added++;
				else
					rows_updated++;

				break;
			}	// Update or insert succeeded.
		}	// Not an access conflict.
	}	// Retry Loop.


	if (n_retries == SQL_UPDATE_RETRIES)
	{
		WRITE_ACCESS_CONFLICT_TO_LOG ("4106");
	}

	return retcode;

}	// End of msg 4106 (drug_diagnoses) handler.



// -------------------------------------------------------------------------
//
//       Post-processor for message 4106 - delete DRUG_DIAGNOSES
//			rows that are no longer in the AS/400 full-table download.
//
// -------------------------------------------------------------------------
static int postprocess_4106 (TConnection	*p_connect)
{
	// local variables
	bool	stop_retrying;	// if true - return from function
	int		rows_deleted	= 0;
	int		retcode;		// code returned from function
	int		n_retries;		// retrying counter
	int		db_changed_flg;

	// DB variables
	int		diagnosis_code;
	int		v_as400_batch_date;
	int		v_as400_batch_time;


	// Initialize variables.
	v_as400_batch_date	= download_start_date;
	v_as400_batch_time	= download_start_time;


	retcode			= ALL_RETRIES_WERE_USED;
	stop_retrying	= false;


	DeclareCursorInto (	MAIN_DB, AS400SRV_T4106PP_postproc_cur,
						&diagnosis_code,
						&v_as400_batch_date,	&v_as400_batch_date,
						&v_as400_batch_time,	END_OF_ARG_LIST			);

	OpenCursor (	MAIN_DB, AS400SRV_T4106PP_postproc_cur	);

	if (SQLERR_error_test ())
	{
		retcode = SQL_ERROR_OCCURED;
//		break;
	}

	while (1)
	{
		FetchCursor (	MAIN_DB, AS400SRV_T4106PP_postproc_cur	);

		if (SQLERR_code_cmp (SQLERR_end_of_fetch) == MAC_TRUE)
		{
			retcode = MSG_HANDLER_SUCCEEDED;	// In case there is nothing that needs to be deleted.
			break;
		}
		else
		{
			if (SQLERR_error_test ())
			{
				retcode = SQL_ERROR_OCCURED;
				break;
			}
		}

		// Nothing fancy here - just a physical delete.
		ExecSQL (	MAIN_DB, AS400SRV_T4106PP_DEL_drug_diagnoses	);

		HANDLE_DELETE_SQL_CODE ("postprocess_4106", p_connect, &retcode, &db_changed_flg);

		rows_deleted++;
	}	// Cursor-scrolling loop.

	CloseCursor (	MAIN_DB, AS400SRV_T4106PP_postproc_cur	);


	// DonR 13Nov2018 CR #15919: Record current table size.
	REMEMBER_ISOLATION;
	SET_ISOLATION_DIRTY;

	ExecSQL (	MAIN_DB, AS400SRV_T4106PP_READ_drug_diagnoses_table_size,
				&total_rows, END_OF_ARG_LIST								);

	active_rows = total_rows;

	RESTORE_ISOLATION;
	// DonR 13Nov2018 CR #15919 end.


	GerrLogMini (GerrId,
				 "Postprocess 4106: %d rows logically deleted, retcode = %d.",
				 rows_deleted, retcode);

	return retcode;

}	// End of msg 4106 (drug_diagnoses) postprocessor.



// -------------------------------------------------------------------------
//
//       Handler for message 4107 - update/insert to HOW_TO_TAKE
//
//            No content comparison - this table is sent non-incrementally.
//		CR #32620		
// -------------------------------------------------------------------------
static int data_4107_handler (TConnection	*p_connect,
							  int			data_len)
{
	// local variables
	char	*PosInBuff;			// current pos in data buffer
	int		ReadInput;			// amount of Bytesread from buffer
	int		retcode;			// code returned from function
	int		n_retries;			// retrying counter
	int		row_was_inserted = 0;

	// DB variables
	short	how_to_take_code;
	char	how_to_take_desc[40 + 1];
	int		v_update_date;
	int		v_update_time;
	int		v_as400_batch_date;
	int		v_as400_batch_time;


	// Calculate virtual time. Note that for this download the time is based
	// on the real time, adjusted only to ensure that we don't have too many
	// items with the exact same as-of time.
	v_update_date  = v_as400_batch_date = (download_start_date > 0) ? download_start_date : GetDate();
	v_update_time  = v_as400_batch_time = (download_start_time > 0) ? download_start_time : GetTime();
	v_update_time += (download_row_count++ / VIRTUAL_PORTION);


	// read input buffer
	INIT_ERROR_DETECTION_MECHANISM;
	PosInBuff = glbMsgBuffer;

	GET_SHORT	(how_to_take_code		,  3);
	GET_HEB_STR	(how_to_take_desc		, 40);
	
	// Verify all data was read.
	ReadInput = PosInBuff - glbMsgBuffer;
	StaticAssert (ReadInput == data_len);


	// Perform up to SQL_UPDATE_RETRIES retries to overcome access conflicts.
	for (n_retries = 0, retcode = ALL_RETRIES_WERE_USED;
		 n_retries < SQL_UPDATE_RETRIES;
		 n_retries++)
	{
		// If this isn't the first iteration, sleep for a while.
		if (n_retries > 0)
		{
			sleep (SLEEP_TIME_BETWEEN_RETRIES);
		}

		// The most frequent operation will be a refresh/update of an existing row; the AS/400
		// batch date and time need to be updated even if nothing has changed, so the postprocessor
		// knows not to delete the row.
		ExecSQL (	MAIN_DB, AS400SRV_T4107_UPD_how_to_take,
					&how_to_take_desc,
					&v_as400_batch_date,		&v_as400_batch_time,
					
					&how_to_take_code,			END_OF_ARG_LIST			);

		// If the row isn't already in the database, we need to insert it.
		if (SQLERR_code_cmp (SQLERR_no_rows_affected) == MAC_TRUE)
		{
			ExecSQL (	MAIN_DB, AS400SRV_T4107_INS_how_to_take,
						&how_to_take_code,			&how_to_take_desc,
						&v_as400_batch_date,		&v_as400_batch_time,		
						END_OF_ARG_LIST			);
					
			row_was_inserted = 1;
		}

		if (SQLERR_code_cmp (SQLERR_access_conflict) == MAC_TRUE)
		{
			retcode = ALL_RETRIES_WERE_USED;
			continue;
		}
		else
		{
			if (SQLERR_error_test ())
			{
				retcode = SQL_ERROR_OCCURED;
				break;
			}
			else
			// Fourth possibility: the refresh succeeded, so we don't have to do anything else.
			{
				retcode = MSG_HANDLER_SUCCEEDED;

				if (row_was_inserted)
					rows_added++;
				else
					rows_updated++;

				break;
			}	// Update or insert succeeded.
		}	// Not an access conflict.
	}	// Retry Loop.


	if (n_retries == SQL_UPDATE_RETRIES)
	{
		WRITE_ACCESS_CONFLICT_TO_LOG ("4107");
	}

	return retcode;

}	// End of msg 4107 (HOW_TO_TAKE) handler.



// -------------------------------------------------------------------------
//
//       Post-processor for message 4107 - delete HOW_TO_TAKE
//			rows that are no longer in the AS/400 full-table download.
//
// -------------------------------------------------------------------------
static int postprocess_4107 (TConnection	*p_connect)
{
	// local variables
	bool	stop_retrying;	// if true - return from function
	int		rows_deleted	= 0;
	int		retcode;		// code returned from function
	int		n_retries;		// retrying counter
	int		db_changed_flg;

	// DB variables
	short	how_to_take_code;
	int		v_as400_batch_date;
	int		v_as400_batch_time;


	// Initialize variables.
	v_as400_batch_date	= download_start_date;
	v_as400_batch_time	= download_start_time;


	retcode			= ALL_RETRIES_WERE_USED;
	stop_retrying	= false;


	DeclareCursorInto (	MAIN_DB, AS400SRV_T4107PP_postproc_cur,
						&how_to_take_code,
						&v_as400_batch_date,	&v_as400_batch_date,
						&v_as400_batch_time,	END_OF_ARG_LIST			);

	OpenCursor (	MAIN_DB, AS400SRV_T4107PP_postproc_cur	);

	if (SQLERR_error_test ())
	{
		retcode = SQL_ERROR_OCCURED;
//		break;
	}

	while (1)
	{
		FetchCursor (	MAIN_DB, AS400SRV_T4107PP_postproc_cur	);

		if (SQLERR_code_cmp (SQLERR_end_of_fetch) == MAC_TRUE)
		{
			retcode = MSG_HANDLER_SUCCEEDED;	// In case there is nothing that needs to be deleted.
			break;
		}
		else
		{
			if (SQLERR_error_test ())
			{
				retcode = SQL_ERROR_OCCURED;
				break;
			}
		}

		// Nothing fancy here - just a physical delete.
		// TEMPORARY FOR TESTING ONLY: SET DELETION FLAG INSTEAD OF PERFORMING A PHYSICAL DELETION.
		// DonR 10Feb2020: For ODBC version, switch to a real deletion.
		ExecSQL (	MAIN_DB, AS400SRV_T4107PP_DEL_how_to_take	);

		HANDLE_DELETE_SQL_CODE ("postprocess_4107", p_connect, &retcode, &db_changed_flg);

		rows_deleted++;
	}	// Cursor-scrolling loop.

	CloseCursor (	MAIN_DB, AS400SRV_T4107PP_postproc_cur	);


	// DonR 13Nov2018 CR #15919: Record current table size.
	REMEMBER_ISOLATION;
	SET_ISOLATION_DIRTY;

	ExecSQL (	MAIN_DB, AS400SRV_T4107PP_READ_how_to_take_table_size,
				&total_rows, END_OF_ARG_LIST							);

	active_rows = total_rows;

	RESTORE_ISOLATION;
	// DonR 13Nov2018 CR #15919 end.


	GerrLogMini (GerrId,
				 "Postprocess 4107: %d rows logically deleted, retcode = %d.",
				 rows_deleted, retcode);

	return retcode;

}	// End of msg 4107 (how_to_take) postprocessor.



// -------------------------------------------------------------------------
//
//       Handler for message 4108 - update/insert to UNIT_OF_MEASURE
//
//            No content comparison - this table is sent non-incrementally.
//		 CR #32620
// -------------------------------------------------------------------------
static int data_4108_handler (TConnection	*p_connect,
							  int			data_len)
{
	// local variables
	char	*PosInBuff;			// current pos in data buffer
	int		ReadInput;			// amount of Bytesread from buffer
	int		retcode;			// code returned from function
	int		n_retries;			// retrying counter
	int		row_was_inserted = 0;

	// DB variables
	char	unit_abbreviation   [3 + 1];
	char	short_desc_english  [8 + 1]; 
	char	short_desc_hebrew   [8 + 1];
	int		v_update_date;
	int		v_update_time;
	int		v_as400_batch_date;
	int		v_as400_batch_time;


	// Calculate virtual time. Note that for this download the time is based
	// on the real time, adjusted only to ensure that we don't have too many
	// items with the exact same as-of time.
	v_update_date  = v_as400_batch_date = (download_start_date > 0) ? download_start_date : GetDate();
	v_update_time  = v_as400_batch_time = (download_start_time > 0) ? download_start_time : GetTime();
	v_update_time += (download_row_count++ / VIRTUAL_PORTION);


	// read input buffer
	INIT_ERROR_DETECTION_MECHANISM;
	PosInBuff = glbMsgBuffer;

	GET_STRING		(unit_abbreviation    		,  3);
	GET_STRING		(short_desc_english   		,  8);
	GET_HEB_STR		(short_desc_hebrew    		,  8);
	
	// Verify all data was read.
	ReadInput = PosInBuff - glbMsgBuffer;
	StaticAssert (ReadInput == data_len);


	// Perform up to SQL_UPDATE_RETRIES retries to overcome access conflicts.
	for (n_retries = 0, retcode = ALL_RETRIES_WERE_USED;
		 n_retries < SQL_UPDATE_RETRIES;
		 n_retries++)
	{
		// If this isn't the first iteration, sleep for a while.
		if (n_retries > 0)
		{
			sleep (SLEEP_TIME_BETWEEN_RETRIES);
		}


		// The most frequent operation will be a refresh/update of an existing row; the AS/400
		// batch date and time need to be updated even if nothing has changed, so the postprocessor
		// knows not to delete the row.
		ExecSQL (	MAIN_DB, AS400SRV_T4108_UPD_unit_of_measure,
					&short_desc_english		,		&short_desc_hebrew 	,   
					&v_as400_batch_date		,		&v_as400_batch_time	,
					
					&unit_abbreviation		,		END_OF_ARG_LIST		);


		// If the row isn't already in the database, we need to insert it.
		if (SQLERR_code_cmp (SQLERR_no_rows_affected) == MAC_TRUE)
		{
			ExecSQL (	MAIN_DB, AS400SRV_T4108_INS_unit_of_measure,
						&unit_abbreviation		,			&short_desc_english ,
						&short_desc_hebrew		,			&v_as400_batch_date	,		
						&v_as400_batch_time		,			END_OF_ARG_LIST		);
					

			row_was_inserted = 1;
		}

		if (SQLERR_code_cmp (SQLERR_access_conflict) == MAC_TRUE)
		{
			retcode = ALL_RETRIES_WERE_USED;
			continue;
		}
		else
		{
			if (SQLERR_error_test ())
			{
				retcode = SQL_ERROR_OCCURED;
				break;
			}
			else
			// Fourth possibility: the refresh succeeded, so we don't have to do anything else.
			{
				retcode = MSG_HANDLER_SUCCEEDED;

				if (row_was_inserted)
					rows_added++;
				else
					rows_updated++;

				break;
			}	// Update or insert succeeded.
		}	// Not an access conflict.
	}	// Retry Loop.


	if (n_retries == SQL_UPDATE_RETRIES)
	{
		WRITE_ACCESS_CONFLICT_TO_LOG ("4108");
	}

	return retcode;

}	// End of msg 4108 (UNIT_OF_MEASURE) handler.



// -------------------------------------------------------------------------
//
//       Post-processor for message 4108 - delete UNIT_OF_MEASURE
//			rows that are no longer in the AS/400 full-table download.
//
// -------------------------------------------------------------------------
static int postprocess_4108 (TConnection	*p_connect)
{
	// local variables
	bool	stop_retrying;	// if true - return from function
	int		rows_deleted	= 0;
	int		retcode;		// code returned from function
	int		n_retries;		// retrying counter
	int		db_changed_flg;

	// DB variables
	char	unit_abbreviation   [3 + 1];
	int		v_as400_batch_date;
	int		v_as400_batch_time;


	// Initialize variables.
	v_as400_batch_date	= download_start_date;
	v_as400_batch_time	= download_start_time;


	retcode			= ALL_RETRIES_WERE_USED;
	stop_retrying	= false;


	DeclareCursorInto (	MAIN_DB, AS400SRV_T4108PP_postproc_cur,
						&unit_abbreviation,
						&v_as400_batch_date,	&v_as400_batch_date,
						&v_as400_batch_time,	END_OF_ARG_LIST			);

	OpenCursor (	MAIN_DB, AS400SRV_T4108PP_postproc_cur	);

	if (SQLERR_error_test ())
	{
		retcode = SQL_ERROR_OCCURED;
//		break;
	}

	while (1)
	{
		FetchCursor (	MAIN_DB, AS400SRV_T4108PP_postproc_cur	);

		if (SQLERR_code_cmp (SQLERR_end_of_fetch) == MAC_TRUE)
		{
			retcode = MSG_HANDLER_SUCCEEDED;	// In case there is nothing that needs to be deleted.
			break;
		}
		else
		{
			if (SQLERR_error_test ())
			{
				retcode = SQL_ERROR_OCCURED;
				break;
			}
		}

		// Nothing fancy here - just a physical delete.
		// TEMPORARY FOR TESTING ONLY: SET DELETION FLAG INSTEAD OF PERFORMING A PHYSICAL DELETION.
		// DonR 10Feb2020: For ODBC version, switch to a real deletion.
		ExecSQL (	MAIN_DB, AS400SRV_T4108PP_DEL_unit_of_measure	);

		HANDLE_DELETE_SQL_CODE ("postprocess_4108", p_connect, &retcode, &db_changed_flg);

		rows_deleted++;
	}	// Cursor-scrolling loop.

	CloseCursor (	MAIN_DB, AS400SRV_T4108PP_postproc_cur	);


	// DonR 13Nov2018 CR #15919: Record current table size.
	REMEMBER_ISOLATION;
	SET_ISOLATION_DIRTY;

	ExecSQL (	MAIN_DB, AS400SRV_T4108PP_READ_unit_of_measure_table_size,
				&total_rows, END_OF_ARG_LIST							);

	active_rows = total_rows;

	RESTORE_ISOLATION;
	// DonR 13Nov2018 CR #15919 end.


	GerrLogMini (GerrId,
				 "Postprocess 4108: %d rows logically deleted, retcode = %d.",
				 rows_deleted, retcode);

	return retcode;

}	// End of msg 4108 (unit_of_measure) postprocessor.



// -------------------------------------------------------------------------
//
//       Handler for message 4109 - update/insert to REASON_NOT_SOLD
//
//            No content comparison - this table is sent non-incrementally.
//		CR #32620
// -------------------------------------------------------------------------
static int data_4109_handler (TConnection	*p_connect,
							  int			data_len)
{
	// local variables
	char	*PosInBuff;			// current pos in data buffer
	int		ReadInput;			// amount of Bytesread from buffer
	int		retcode;			// code returned from function
	int		n_retries;			// retrying counter
	int		row_was_inserted = 0;

	// DB variables
	short	reason_code		;
	char	reason_desc			[50 + 1]; 
	int		v_update_date;
	int		v_update_time;
	int		v_as400_batch_date;
	int		v_as400_batch_time;


	// Calculate virtual time. Note that for this download the time is based
	// on the real time, adjusted only to ensure that we don't have too many
	// items with the exact same as-of time.
	v_update_date  = v_as400_batch_date = (download_start_date > 0) ? download_start_date : GetDate();
	v_update_time  = v_as400_batch_time = (download_start_time > 0) ? download_start_time : GetTime();
	v_update_time += (download_row_count++ / VIRTUAL_PORTION);


	// read input buffer
	INIT_ERROR_DETECTION_MECHANISM;
	PosInBuff = glbMsgBuffer;

	GET_SHORT		(reason_code   		,  3);
	GET_HEB_STR		(reason_desc   		,  50);
	
	// Verify all data was read.
	ReadInput = PosInBuff - glbMsgBuffer;
	StaticAssert (ReadInput == data_len);


	// Perform up to SQL_UPDATE_RETRIES retries to overcome access conflicts.
	for (n_retries = 0, retcode = ALL_RETRIES_WERE_USED;
		 n_retries < SQL_UPDATE_RETRIES;
		 n_retries++)
	{
		// If this isn't the first iteration, sleep for a while.
		if (n_retries > 0)
		{
			sleep (SLEEP_TIME_BETWEEN_RETRIES);
		}


		// The most frequent operation will be a refresh/update of an existing row; the AS/400
		// batch date and time need to be updated even if nothing has changed, so the postprocessor
		// knows not to delete the row.
		ExecSQL (	MAIN_DB, AS400SRV_T4109_UPD_reason_not_sold,
					&reason_desc		,		  
					&v_as400_batch_date	,	&v_as400_batch_time,
					&reason_code		,	END_OF_ARG_LIST			);


		// If the row isn't already in the database, we need to insert it.
		if (SQLERR_code_cmp (SQLERR_no_rows_affected) == MAC_TRUE)
		{
			ExecSQL (	MAIN_DB, AS400SRV_T4109_INS_reason_not_sold,
						&reason_code		,		&reason_desc 			,
						&v_as400_batch_date	,		&v_as400_batch_time		,			
						END_OF_ARG_LIST		);
					

			row_was_inserted = 1;
		}

		if (SQLERR_code_cmp (SQLERR_access_conflict) == MAC_TRUE)
		{
			retcode = ALL_RETRIES_WERE_USED;
			continue;
		}
		else
		{
			if (SQLERR_error_test ())
			{
				retcode = SQL_ERROR_OCCURED;
				break;
			}
			else
			// Fourth possibility: the refresh succeeded, so we don't have to do anything else.
			{
				retcode = MSG_HANDLER_SUCCEEDED;

				if (row_was_inserted)
					rows_added++;
				else
					rows_updated++;

				break;
			}	// Update or insert succeeded.
		}	// Not an access conflict.
	}	// Retry Loop.


	if (n_retries == SQL_UPDATE_RETRIES)
	{
		WRITE_ACCESS_CONFLICT_TO_LOG ("4109");
	}

	return retcode;

}	// End of msg 4109 (REASON_NOT_SOLD) handler.


// -------------------------------------------------------------------------
//
//       Post-processor for message 4109 - delete REASON_NOT_SOLD
//			rows that are no longer in the AS/400 full-table download.
//
// -------------------------------------------------------------------------
static int postprocess_4109 (TConnection	*p_connect)
{
	// local variables
	bool	stop_retrying;	// if true - return from function
	int		rows_deleted	= 0;
	int		retcode;		// code returned from function
	int		n_retries;		// retrying counter
	int		db_changed_flg;

	// DB variables
	short	reason_code;
	int		v_as400_batch_date;
	int		v_as400_batch_time;


	// Initialize variables.
	v_as400_batch_date	= download_start_date;
	v_as400_batch_time	= download_start_time;


	retcode			= ALL_RETRIES_WERE_USED;
	stop_retrying	= false;


	DeclareCursorInto (	MAIN_DB, AS400SRV_T4109PP_postproc_cur,
						&reason_code,
						&v_as400_batch_date,	&v_as400_batch_date,
						&v_as400_batch_time,	END_OF_ARG_LIST			);

	OpenCursor (	MAIN_DB, AS400SRV_T4109PP_postproc_cur	);

	if (SQLERR_error_test ())
	{
		retcode = SQL_ERROR_OCCURED;
//		break;
	}

	while (1)
	{
		FetchCursor (	MAIN_DB, AS400SRV_T4109PP_postproc_cur	);

		if (SQLERR_code_cmp (SQLERR_end_of_fetch) == MAC_TRUE)
		{
			retcode = MSG_HANDLER_SUCCEEDED;	// In case there is nothing that needs to be deleted.
			break;
		}
		else
		{
			if (SQLERR_error_test ())
			{
				retcode = SQL_ERROR_OCCURED;
				break;
			}
		}

		// Nothing fancy here - just a physical delete.
		// TEMPORARY FOR TESTING ONLY: SET DELETION FLAG INSTEAD OF PERFORMING A PHYSICAL DELETION.
		// DonR 10Feb2020: For ODBC version, switch to a real deletion.
		ExecSQL (	MAIN_DB, AS400SRV_T4109PP_DEL_reason_not_sold	);

		HANDLE_DELETE_SQL_CODE ("postprocess_4109", p_connect, &retcode, &db_changed_flg);

		rows_deleted++;
	}	// Cursor-scrolling loop.

	CloseCursor (	MAIN_DB, AS400SRV_T4109PP_postproc_cur	);

	// DonR 13Nov2018 CR #15919: Record current table size.
	REMEMBER_ISOLATION;
	SET_ISOLATION_DIRTY;

	ExecSQL (	MAIN_DB, AS400SRV_T4109PP_READ_reason_not_sold_table_size,
				&total_rows, END_OF_ARG_LIST							);

	active_rows = total_rows;

	RESTORE_ISOLATION;
	// DonR 13Nov2018 CR #15919 end.


	GerrLogMini (GerrId,
				 "Postprocess 4109: %d rows logically deleted, retcode = %d.",
				 rows_deleted, retcode);

	return retcode;

}	// End of msg 4109 (reason_not_sold) postprocessor.



// --------------------------------------------------------------------------------
//
//       Handler for message 4110 - update/insert to usage_instr_reason_changed
//
//            No content comparison - this table is sent non-incrementally.
//		 CR #32620
// --------------------------------------------------------------------------------
static int data_4110_handler (TConnection	*p_connect,
							  int			data_len)
{
	// local variables
	char	*PosInBuff;			// current pos in data buffer
	int		ReadInput;			// amount of Bytesread from buffer
	int		retcode;			// code returned from function
	int		n_retries;			// retrying counter
	int		row_was_inserted = 0;

	// DB variables
	short	reason_changed_code;
	char	reason_short_desc  [30 + 1]; 
	char	reason_long_desc   [50 + 1];
	int		v_update_date;
	int		v_update_time;
	int		v_as400_batch_date;
	int		v_as400_batch_time;


	// Calculate virtual time. Note that for this download the time is based
	// on the real time, adjusted only to ensure that we don't have too many
	// items with the exact same as-of time.
	v_update_date  = v_as400_batch_date = (download_start_date > 0) ? download_start_date : GetDate();
	v_update_time  = v_as400_batch_time = (download_start_time > 0) ? download_start_time : GetTime();
	v_update_time += (download_row_count++ / VIRTUAL_PORTION);


	// read input buffer
	INIT_ERROR_DETECTION_MECHANISM;
	PosInBuff = glbMsgBuffer;

	GET_SHORT		(reason_changed_code   		,  3);
	GET_HEB_STR		(reason_short_desc   		,  30);
	GET_HEB_STR		(reason_long_desc    		,  50);
	
	// Verify all data was read.
	ReadInput = PosInBuff - glbMsgBuffer;
	StaticAssert (ReadInput == data_len);


	// Perform up to SQL_UPDATE_RETRIES retries to overcome access conflicts.
	for (n_retries = 0, retcode = ALL_RETRIES_WERE_USED;
		 n_retries < SQL_UPDATE_RETRIES;
		 n_retries++)
	{
		// If this isn't the first iteration, sleep for a while.
		if (n_retries > 0)
		{
			sleep (SLEEP_TIME_BETWEEN_RETRIES);
		}


		// The most frequent operation will be a refresh/update of an existing row; the AS/400
		// batch date and time need to be updated even if nothing has changed, so the postprocessor
		// knows not to delete the row.
		ExecSQL (	MAIN_DB, AS400SRV_T4110_UPD_usage_instr_reason_changed,
					&reason_short_desc		,		&reason_long_desc 	,   
					&v_as400_batch_date		,		&v_as400_batch_time	,
					
					&reason_changed_code	,		END_OF_ARG_LIST				);


		// If the row isn't already in the database, we need to insert it.
		if (SQLERR_code_cmp (SQLERR_no_rows_affected) == MAC_TRUE)
		{
			ExecSQL (	MAIN_DB, AS400SRV_T4110_INS_usage_instr_reason_changed,
						&reason_changed_code	,			&reason_short_desc ,
						&reason_long_desc		,			&v_as400_batch_date	,		
						&v_as400_batch_time		,			END_OF_ARG_LIST			);
					

			row_was_inserted = 1;
		}

		if (SQLERR_code_cmp (SQLERR_access_conflict) == MAC_TRUE)
		{
			retcode = ALL_RETRIES_WERE_USED;
			continue;
		}
		else
		{
			if (SQLERR_error_test ())
			{
				retcode = SQL_ERROR_OCCURED;
				break;
			}
			else
			// Fourth possibility: the refresh succeeded, so we don't have to do anything else.
			{
				retcode = MSG_HANDLER_SUCCEEDED;

				if (row_was_inserted)
					rows_added++;
				else
					rows_updated++;

				break;
			}	// Update or insert succeeded.
		}	// Not an access conflict.
	}	// Retry Loop.


	if (n_retries == SQL_UPDATE_RETRIES)
	{
		WRITE_ACCESS_CONFLICT_TO_LOG ("4110");
	}

	return retcode;

}	// End of msg 4110 (usage_instr_reason_changed) handler.



// -------------------------------------------------------------------------
//
//       Post-processor for message 4110 - delete usage_instr_reason_changed
//			rows that are no longer in the AS/400 full-table download.
//
// -------------------------------------------------------------------------
static int postprocess_4110 (TConnection	*p_connect)
{
	// local variables
	bool	stop_retrying;	// if true - return from function
	int		rows_deleted	= 0;
	int		retcode;		// code returned from function
	int		n_retries;		// retrying counter
	int		db_changed_flg;

	// DB variables
	short	reason_changed_code;
	int		v_as400_batch_date;
	int		v_as400_batch_time;


	// Initialize variables.
	v_as400_batch_date	= download_start_date;
	v_as400_batch_time	= download_start_time;


	retcode			= ALL_RETRIES_WERE_USED;
	stop_retrying	= false;


	DeclareCursorInto (	MAIN_DB, AS400SRV_T4110PP_postproc_cur,
						&reason_changed_code,
						&v_as400_batch_date,	&v_as400_batch_date,
						&v_as400_batch_time,	END_OF_ARG_LIST			);

	OpenCursor (	MAIN_DB, AS400SRV_T4110PP_postproc_cur	);

	if (SQLERR_error_test ())
	{
		retcode = SQL_ERROR_OCCURED;
//		break;
	}

	while (1)
	{
		FetchCursor (	MAIN_DB, AS400SRV_T4110PP_postproc_cur	);

		if (SQLERR_code_cmp (SQLERR_end_of_fetch) == MAC_TRUE)
		{
			retcode = MSG_HANDLER_SUCCEEDED;	// In case there is nothing that needs to be deleted.
			break;
		}
		else
		{
			if (SQLERR_error_test ())
			{
				retcode = SQL_ERROR_OCCURED;
				break;
			}
		}

		// Nothing fancy here - just a physical delete.
		// TEMPORARY FOR TESTING ONLY: SET DELETION FLAG INSTEAD OF PERFORMING A PHYSICAL DELETION.
		// DonR 10Feb2020: For ODBC version, switch to a real deletion.
		ExecSQL (	MAIN_DB, AS400SRV_T4110PP_DEL_usage_instr_reason_changed	);

		HANDLE_DELETE_SQL_CODE ("postprocess_4110", p_connect, &retcode, &db_changed_flg);

		rows_deleted++;
	}	// Cursor-scrolling loop.

	CloseCursor (	MAIN_DB, AS400SRV_T4110PP_postproc_cur	);


	// DonR 13Nov2018 CR #15919: Record current table size.
	REMEMBER_ISOLATION;
	SET_ISOLATION_DIRTY;

	ExecSQL (	MAIN_DB, AS400SRV_T4110PP_READ_usage_instr_reason_changed_table_size,
				&total_rows, END_OF_ARG_LIST											);

	active_rows = total_rows;

	RESTORE_ISOLATION;
	// DonR 13Nov2018 CR #15919 end.


	GerrLogMini (GerrId,
				 "Postprocess 4110: %d rows logically deleted, retcode = %d.",
				 rows_deleted, retcode);

	return retcode;

}	// End of msg 4110 (usage_instr_reason_changed) postprocessor.


// DonR 23Jul2023 User Story 448931/458942: Define values for restriction_type.
// DonR 13Mar2025 User Story #384811: Add some new restriction type by insurer/Darkonai type.
// These are similar to RESTRICT_ALL_BY_TZ_CODE, but they apply to subsets of the Darkonaim.
#define BLOCK_DRUG_BY_MEMBER_ID			 1
#define RESTRICT_ALL_BY_TZ_CODE			 2
#define EXEMPT_DRUG_FROM_MEMBER_PHARM	 3
#define RESTRICT_MACCABI_DARKONAIM		11
#define RESTRICT_HAREL_TOURISTS			12
#define RESTRICT_HAREL_WORKERS			13

// --------------------------------------------------------------------------------
//
//       Handler for message 4111 - update/insert to member_blocked_drugs
//
//            No content comparison - this table is sent non-incrementally.
//		 User Story #196891
// --------------------------------------------------------------------------------
static int data_4111_handler (TConnection	*p_connect,
							  int			data_len)
{
	// local variables
	char	*PosInBuff;			// current pos in data buffer
	int		ReadInput;			// amount of Bytesread from buffer
	int		retcode;			// code returned from function
	int		n_retries;			// retrying counter
	int		row_was_inserted = 0;

	// DB variables
	int		member_id;
	short	mem_id_extension;
	int		largo_code;
	short	restriction_type;	// DonR 23Jul2023 User Story 448931/458942.
	int		v_as400_batch_date;
	int		v_as400_batch_time;


	// Initialize variables.
	v_as400_batch_date	= download_start_date;
	v_as400_batch_time	= download_start_time;

	// read input buffer
	INIT_ERROR_DETECTION_MECHANISM;
	PosInBuff = glbMsgBuffer;

	GET_LONG		(member_id					,  9);
	GET_SHORT		(mem_id_extension   		,  1);
	GET_LONG		(largo_code					,  9);
	GET_SHORT		(restriction_type   		,  2);	// DonR 23Jul2023 User Story 448931/458942. 13Mar2025 User Story #384811: Length 1 -> 2.
	
	// Verify all data was read.
	ReadInput = PosInBuff - glbMsgBuffer;
	StaticAssert (ReadInput == data_len);

	// DonR 23Jul2023 User Story 448931/458942: Depending on what Restriction Type is being used,
	// make sure the correct fields have zero values. (Shouldn't really be necessary since AS/400
	// should already be sending zeroes, but paranaoia is our friend!)
	switch (restriction_type)
	{
		case BLOCK_DRUG_BY_MEMBER_ID:		// In this case, we use whatever AS/400 sent.
											break;

		case RESTRICT_ALL_BY_TZ_CODE:		// In these cases, Member ID Code is significant but Member ID should be zero.
		case RESTRICT_MACCABI_DARKONAIM:	// DonR 13Mar2025 User Story #384811
		case RESTRICT_HAREL_TOURISTS:		// DonR 13Mar2025 User Story #384811
		case RESTRICT_HAREL_WORKERS:		// DonR 13Mar2025 User Story #384811
											member_id = 0;
											break;

		case EXEMPT_DRUG_FROM_MEMBER_PHARM:	// In this case, both Member ID Code *and* Member ID should be zero.
											member_id			= 0;
											mem_id_extension	= 0;
											break;

		default:							// Log a message from unknown values, but don't do anything else.
											GerrLogMini (GerrId,
														 "Trn. 4111 (member_blocked_drugs): Got strange value %d for Restriction Type!",
														 restriction_type);
											break;
	}

	// Perform up to SQL_UPDATE_RETRIES retries to overcome access conflicts.
	for (n_retries = 0, retcode = ALL_RETRIES_WERE_USED;
		 n_retries < SQL_UPDATE_RETRIES;
		 n_retries++)
	{
		// If this isn't the first iteration, sleep for a while.
		if (n_retries > 0)
		{
			sleep (SLEEP_TIME_BETWEEN_RETRIES);
		}


		// The most frequent operation will be a refresh/update of an existing row; the AS/400
		// batch date and time need to be updated even if nothing has changed, so the postprocessor
		// knows not to delete the row.
		// DonR 23Jul2023 User Story 448931/458942: Add restriction_type to member_blocked_drugs;
		// it's part of the table's unique index.
		ExecSQL (	MAIN_DB, AS400SRV_T4111_UPD_member_blocked_drugs,
					&v_as400_batch_date		,	&v_as400_batch_time	,
					
					&member_id				,	&largo_code,
					&mem_id_extension		,	&restriction_type,
					END_OF_ARG_LIST										);


		// If the row isn't already in the database, we need to insert it.
		// We also need to set the member's has_blocked_drugs flag TRUE.
		// DonR 23Jul2023 User Story 448931/458942: Add restriction_type to member_blocked_drugs; it's
		// part of the table's unique index. With this change, some rows (with restriction_type = 1)
		// set the member's has_blocked_drugs flag TRUE; restriction_type values 2 and 3 set flags in
		// the drug_list table instead.
		if (SQLERR_code_cmp (SQLERR_no_rows_affected) == MAC_TRUE)
		{
			ExecSQL (	MAIN_DB, AS400SRV_T4111_INS_member_blocked_drugs,
						&member_id			,	&mem_id_extension	,
						&largo_code			,	&restriction_type	,
						&v_as400_batch_date	,	&v_as400_batch_time	,
						END_OF_ARG_LIST										);

			if (SQLCODE == 0)
			{
				switch (restriction_type)
				{
					case BLOCK_DRUG_BY_MEMBER_ID:		// Set has_blocked_drugs TRUE for a specific member.
														ExecSQL	(	MAIN_DB, AS400SRV_T4111_SetMemberBlockedDrugsFlag,
																	&member_id, &mem_id_extension, END_OF_ARG_LIST		);
														break;

					case RESTRICT_ALL_BY_TZ_CODE:		// Set has_member_type_exclusion TRUE for this Largo Code.
					case RESTRICT_MACCABI_DARKONAIM:	// DonR 13Mar2025 User Story #384811
					case RESTRICT_HAREL_TOURISTS:		// DonR 13Mar2025 User Story #384811
					case RESTRICT_HAREL_WORKERS:		// DonR 13Mar2025 User Story #384811
														ExecSQL	(	MAIN_DB, AS400SRV_T4111_SetMemberTypeExclusionFlag,
																	&largo_code, END_OF_ARG_LIST						);
														break;

					case EXEMPT_DRUG_FROM_MEMBER_PHARM:	// Set bypass_member_pharm_restriction TRUE for this Largo Code.
														ExecSQL	(	MAIN_DB, AS400SRV_T4111_SetBypassMemberPharmFlag,
																	&largo_code, END_OF_ARG_LIST						);
														break;

					default:							// We already logged a message for unknown restriction_type.
														break;
				}
			}

			row_was_inserted = 1;
		}

		if (SQLERR_code_cmp (SQLERR_access_conflict) == MAC_TRUE)
		{
			retcode = ALL_RETRIES_WERE_USED;
			continue;
		}
		else
		{
			if (SQLERR_error_test ())
			{
				retcode = SQL_ERROR_OCCURED;
				break;
			}
			else
			// The refresh succeeded, so we don't have to do anything else.
			{
				retcode = MSG_HANDLER_SUCCEEDED;

				if (row_was_inserted)
					rows_added++;
				else
					rows_updated++;

				break;
			}	// Update or insert succeeded.
		}	// Not an access conflict.
	}	// Retry Loop.


	if (n_retries == SQL_UPDATE_RETRIES)
	{
		WRITE_ACCESS_CONFLICT_TO_LOG ("4111");
	}

	return retcode;

}	// End of msg 4111 (member_blocked_drugs) handler.



// -------------------------------------------------------------------------
//
//       Post-processor for message 4111 - delete member_blocked_drugs
//			rows that are no longer in the AS/400 full-table download.
//
// -------------------------------------------------------------------------
static int postprocess_4111 (TConnection	*p_connect)
{
	// local variables
	bool	stop_retrying;	// if true - return from function
	int		rows_deleted	= 0;
	int		retcode;		// code returned from function
	int		n_retries;		// retrying counter
	int		db_changed_flg;

	// DB variables
	int		member_id;
	short	mem_id_extension;
	int		prev_member_id			= -1;
	short	prev_mem_id_extension	= -1;
	int		v_as400_batch_date;
	int		v_as400_batch_time;


	// Initialize variables.
	v_as400_batch_date	= download_start_date;
	v_as400_batch_time	= download_start_time;


	retcode			= ALL_RETRIES_WERE_USED;
	stop_retrying	= false;


	DeclareAndOpenCursorInto (	MAIN_DB, AS400SRV_T4111PP_postproc_cur,
								&member_id			, &mem_id_extension		,
								&v_as400_batch_date	, &v_as400_batch_time	,
								END_OF_ARG_LIST									);

	if (SQLERR_error_test ())
	{
		retcode = SQL_ERROR_OCCURED;
//		break;
	}

	while (1)
	{
		FetchCursor (	MAIN_DB, AS400SRV_T4111PP_postproc_cur	);

		if (SQLERR_code_cmp (SQLERR_end_of_fetch) == MAC_TRUE)
		{
			retcode = MSG_HANDLER_SUCCEEDED;	// In case there is nothing that needs to be deleted.
			break;
		}
		else
		{
			if (SQLERR_error_test ())
			{
				retcode = SQL_ERROR_OCCURED;
				break;
			}
		}

		// Once for each TZ number/code, clear the members/has_blocked_drugs flag.
		// The SQL statement checks for the existence of any current blocked drugs
		// before clearing the flag, so we don't have to put any fancy logic here.
		// DonR 23Jul2023 User Story 448931/458942: Update the members table only
		// if the obsolete member_blocked_drugs row has a non-zero member_id. This
		// will be *not* be the case for Restriction Types 2 and 3, which do not
		// pertain to individual members.
		if ((member_id > 0)	&&
			((member_id != prev_member_id) || (mem_id_extension != prev_mem_id_extension)))
		{
			prev_member_id			= member_id;
			prev_mem_id_extension	= mem_id_extension;

			ExecSQL (	MAIN_DB, AS400SRV_T4111PP_ClearMemberBlockedDrugsFlag,
						&member_id			, &mem_id_extension		,
						&member_id			, &mem_id_extension		,
						&v_as400_batch_date	, &v_as400_batch_time	,
						END_OF_ARG_LIST											);

			if ((SQLCODE != 0) && (SQLERR_code_cmp (SQLERR_no_rows_affected) != MAC_TRUE))
			{
				SQLERR_error_test ();
			}
		}

		// Nothing fancy here - just a physical delete.
		ExecSQL (	MAIN_DB, AS400SRV_T4111PP_DEL_member_blocked_drugs	);

		HANDLE_DELETE_SQL_CODE ("postprocess_4111", p_connect, &retcode, &db_changed_flg);

		rows_deleted++;
	}	// Cursor-scrolling loop.

	CloseCursor (	MAIN_DB, AS400SRV_T4111PP_postproc_cur	);


	// DonR 23Jul2023 User Story 448931/458942: Restriction Types 2 and 3 involve flags in the
	// drug_list table: has_member_type_exclusion and bypass_member_pharm_restriction. These
	// are set TRUE when member_blocked_drugs rows with these Restriction Types are INSERTed;
	// but we need to set them false here. (NOTE: New restriction types 11, 12, and 13 work
	// similarly to 2. We don't have to do anything differently here.)
	ExecSQL (	MAIN_DB, AS400SRV_T4111PP_ClearMemberTypeExclusionFlag,
				&v_as400_batch_date	, &v_as400_batch_time	,
				END_OF_ARG_LIST												);

	if ((SQLCODE != 0) && (SQLERR_code_cmp (SQLERR_no_rows_affected) != MAC_TRUE))
	{
		SQLERR_error_test ();
	}
else if (SQLERR_code_cmp (SQLERR_no_rows_affected) != MAC_TRUE) GerrLogMini (GerrId, "Postprocess 4111: Cleared has_member_type_exclusion for %d drugs.", sqlca.sqlerrd[2]);

	ExecSQL (	MAIN_DB, AS400SRV_T4111PP_ClearBypassMemberPharmFlag,
				&v_as400_batch_date	, &v_as400_batch_time	,
				END_OF_ARG_LIST											);

	if ((SQLCODE != 0) && (SQLERR_code_cmp (SQLERR_no_rows_affected) != MAC_TRUE))
	{
		SQLERR_error_test ();
	}
else if (SQLERR_code_cmp (SQLERR_no_rows_affected) != MAC_TRUE) GerrLogMini (GerrId, "Postprocess 4111: Cleared bypass_member_pharm_restriction for %d drugs.", sqlca.sqlerrd[2]);



	// DonR 13Nov2018 CR #15919: Record current table size.
	REMEMBER_ISOLATION;
	SET_ISOLATION_DIRTY;

	ExecSQL (	MAIN_DB, AS400SRV_T4111PP_READ_member_blocked_drugs_table_size,
				&total_rows, END_OF_ARG_LIST										);

	active_rows = total_rows;

	RESTORE_ISOLATION;


	GerrLogMini (GerrId,
				 "Postprocess 4111: %d rows logically deleted, retcode = %d.",
				 rows_deleted, retcode);

	return retcode;

}	// End of msg 4111 (member_blocked_drugs) postprocessor.



/*===============================================================
||																||
||			call_msg_handler_pharm()							||
||																||
 ===============================================================*/
static int call_msg_handler_pharm (TSystemMsg		msg,
								   TSystemMsg		msg_type,
								   TConnection		*p_connect,
								   int			len)
{
	static TShmUpdate	shm_table_to_update = NONE;
	static int			last_data_msg = 0;
	short				need_postprocess_commit;
	long				SizeWorkVar;
	int					min_new_size;
	int					max_new_size;
	char				TableSizeResponseBuf [101];

	static char	*FromTable		= "Unknown source";
	static char	*ToTable		= "Unknown dest";
	char		DB_FromTable	[30 + 1];
	char		DB_ToTable		[30 + 1];
	static int	TrnCode			= 0;
	int			TrnCodeDb;				// DonR 09Mar2020: ODBC routines sometimes don't like static variables.
	int			CurrActiveRows	= 0;
	int			retcode;
	int			tot_actions;
	int			SysDate;
	int			SysTime;
	int			IntZero			= 0;
	int			MessageCode	= msg;				// DonR 02Nov2022 HOT FIX: Was a short int, but it's an integer in the database.
	short		max_grow_percent;
	short		max_shrink_percent;
	int			DrugUpdateTriggerDate;			// DonR 03Jan2022
	int			DrugUpdateTriggerTime;			// DonR 03Jan2022
	static int	LastTriggerDate			= 0;	// DonR 03Jan2022
	static int	LastTriggerTime			= 0;	// DonR 03Jan2022


	if (((msg >= 2000) && (msg < 3000))	||
		((msg >= 4000) && (msg < 4999)))
	{
		last_data_msg = msg;
	}

	// DonR 23Sep2020: Copy the current transaction number to a global variable, so dual-library
	// functions can always tell whether they're working on incremental RKFILPRD data or full-
	// table RKUNIX downloads.
	GlbMsgId = msg;

	// Top-level switch - Message Type.
	switch (msg_type)
	{
	case COMMUNICATIONS_MESSAGE:	// GerrLogMini (GerrId, "Got communications message %d, last_data_msg = %d.", msg, last_data_msg);
									switch (msg)
									{
									case CM_REQUESTING_COMMIT:/* 9901 */

										// DonR 06Apr2008: DoctSystem, which was never initialized,
										// is unnecessary. We can assume that almost all drug_list
										// changes should affect both timestamps.
										if (shm_table_to_update == DRUG_LIST)
										{
											UpdateSharedMem (DRUG_LIST_DOCTOR);
										}

										retcode = commit_handler (p_connect,
																  shm_table_to_update);

										// Do not update table update date if nothing updated
										shm_table_to_update = NONE;

										if (last_data_msg != 0)
										{
											GerrLogMini (GerrId,
														 "%d: %4d INS, %4d UPD, %4d DEL, %4d REFRESH",
														 last_data_msg, rows_added, rows_updated, rows_deleted, rows_refreshed);

											tot_actions = rows_added + rows_updated + rows_deleted + rows_refreshed;
											SysDate = GetDate();
											SysTime = GetTime();

											if (TrnCode > 0)
											{
												strcpy (DB_FromTable,	FromTable);	// Because ODBC doesn't like static host variables.
												strcpy (DB_ToTable,		ToTable);	// Because ODBC doesn't like static host variables.
												TrnCodeDb = TrnCode;				// Because ODBC doesn't like static host variables.

												ExecSQL (	MAIN_DB, AS400SRV_UPD_as400in_audit_general_columns,
															&DB_FromTable,		&DB_ToTable,
															&SysDate,			&SysTime,
															&retcode,			&tot_actions,
															&TrnCodeDb,			END_OF_ARG_LIST					);

												if (SQLERR_code_cmp (SQLERR_no_rows_affected) == MAC_TRUE)
												{
													ExecSQL (	MAIN_DB, AS400SRV_INS_as400in_audit,
																&TrnCodeDb,			&DB_FromTable,
																&DB_ToTable,		&SysDate,
																&SysTime,			&retcode,
																&tot_actions,		END_OF_ARG_LIST		);
												}

												if (SQLERR_error_test() != MAC_OK)
												{
													GerrLogReturn (GerrId, "SQL error %d writing to as400in_audit table.", SQLCODE);
												}
												
												CommitAllWork ();
											}
										}

										rows_added = rows_updated = rows_deleted = rows_refreshed = last_data_msg = total_rows = active_rows = 0;
										break;


									case CM_REQUESTING_ROLLBACK:/* 9903 */
										retcode = rollback_handler( p_connect );
										rows_added = rows_updated = rows_deleted = rows_refreshed = last_data_msg = 0;
										break ;


									//  Comment:
									// ------------
									//   The messages bellow can only be SENT (& not be received)
									//   by AS400<->Unix Server process:
									//
									//		CM_CONFIRMING_COMMIT
									//		CM_CONFIRMING_ROLLBACK
									//		CM_SYSTEM_GOING_DOWN
									//		CM_SYSTEM_IS_BUSY
									//		CM_COMMAND_EXECUTION_FAILED
									//		CM_SYSTEM_IS_NOT_BUSY
									//		CM_SQL_ERROR_OCCURED
									//		CM_DATA_PARSING_ERR
									//		CM_ALL_RETRIES_WERE_USED
									//
									// < --------------------------------------------

									default:
										GerrLogReturn (GerrId,
													   "Got unknown communications message id: %d",
													   msg);
										break;

									}	// End of communications-message switch.
									break;


									// DonR 15Nov2018 CR #15919 begin.
									case GET_TABLE_SIZE_RANGE:
										ExecSQL (	MAIN_DB, AS400SRV_READ_as400in_audit_table_size_control_columns,
													&CurrActiveRows,		&max_grow_percent,
													&max_shrink_percent,	&MessageCode,
													END_OF_ARG_LIST														);

										if (SQLCODE != 0)
										{
											// DonR 02Nov2022: I just made a "hot fix", changing MessageCode from a short to
											// a regular integer; this SQL was consistently failing, and since the SQL was
											// expecting this value to be an integer, that's almost certainly what the problem
											// was. However, it may be a good idea to add a diagnostic here for SQL errors!

											// -1 represents a "null" value - disabling size-control logic on the AS/400 side.
											CurrActiveRows = max_grow_percent = max_shrink_percent = -1;
										}

										if (CurrActiveRows == 0)
										{
											// If the table is currently empty, (A) it can't shrink, and (B) *any*
											// added row is an increase of infinite proportion.
											min_new_size	= 0;
											max_new_size	= -1;
										}
										else
										{
											// If we get here, either we have successfully read values from as400in_audit
											// (in which case CurrActiveRows should be zero or greater), or else all three
											// variables (CurrActiveRows, max_grow_percent, and max_shrink_percent) will be
											// -1.

											if (max_grow_percent > -1)
											{
												// Just to make sure we don't get integer arithmetic errors,
												// do the growing/shrinking in two stages using a 64-bit integer.
												SizeWorkVar		= (long)CurrActiveRows * (100 + (long)max_grow_percent);
												max_new_size	= (int)(SizeWorkVar / 100);
											}
											else max_new_size = -1;	// No percentage = no maximum growth.

											// Any "maximum shrinkage percent" > 100 is assumed to be invalid,
											// and will disable shrinkage control.
											if ((max_shrink_percent > -1) && (max_shrink_percent <= 100))
											{
												// Just to make sure we don't get integer arithmetic errors,
												// do the growing/shrinking in two stages using a 64-bit integer.
												SizeWorkVar		= (long)CurrActiveRows * (100 - (long)max_shrink_percent);
												min_new_size	= (int)(SizeWorkVar / 100);
											}
											else min_new_size = -1;	// No percentage = no maximum shrinkage.
										}

										// Create and send output buffer.
										sprintf (TableSizeResponseBuf,
												 "%04d%09d%09d%09d",
												 CM_SUPPLYING_TABLE_SIZE_DATA, CurrActiveRows, min_new_size, max_new_size);

										retcode = inform_client (p_connect, 0, TableSizeResponseBuf);
										if (retcode == failure)
										{
											terminate_a_connection (p_connect, WITHOUT_ROLLBACK);
										}

										break;
										// DonR 15Nov2018 CR #15919 end.



									case BEGIN_TABLE_COMMAND:
// GerrLogReturn (GerrId, "Got Message %d INIT command.", msg);

									// Save the date/time of the beginning of the download.
									// DonR 16Aug2011: For the RKFILPRD (incremental) side of downloads
									// that also come in an RKUNIX full-table version, suppress this
									// initialization so the data-handling routines know to treat all
									// updates as significant even if they're just "refreshes".
									// Note that as new full-table downloads are implemented, their
									// incremental versions should be added to this list.
									download_row_count	= 0;
									total_rows			= 0;
									active_rows			= 0;
									retcode = MSG_HANDLER_SUCCEEDED;
									switch (msg)
									{
										// DonR 23Sep2020: When getting downloads for drug_list, we want to turn off
										// the trigger for "outgoing" Transaction 2517, which (re)sets a number of
										// flags in drug_list and can lead to contention problems. The simple way to
										// do this is to simply set the update_date/time values to zero.
										case 2005:	// Incremental Drug_list updates from RKFILPRD.
										case 2006:	// Drug_list deletion - not really in use.
										case 4005:	// Full Drug_list download from RKUNIX.
										case 2019:	// Spclty_largo_prcnt incremental from RKFILPRD.
										case 2020:	// Spclty_largo_prcnt deletion.
										case 4019:	// Spclty_largo_prcnt full table from RKUNIX.
										case 2054:	// Economypri incremental from RKFILPRD.
										case 4054:	// Economypri full table from RKUNIX.
										case 2071:	// Usage_instruct incremental from RKFILPRD..
										case 4071:	// Usage_instruct full table from RKUNIX..
										case 4106:	// Drug Diagnoses.

													// DonR 03Jan2022: Instead of zeroing out the drug-update trigger timestamp, set
													// it to the current time plus 60 seconds. This should protect us from
													// simultaneous updates, but still ensure that Transaction 2517 will be triggered
													// to make sure all relevant updates are performed. Add a little logic to limit
													// the number of redundant UPDATEs we execute - we really don't need the timestamp
													// updated every single second once we're giving it a 60-second "cushion".
													DrugUpdateTriggerDate = GetDate ();
													DrugUpdateTriggerTime = IncrementTime (GetTime (), 60, &DrugUpdateTriggerDate);

													if (( DrugUpdateTriggerDate >  LastTriggerDate) ||
														((DrugUpdateTriggerDate == LastTriggerDate) && (DrugUpdateTriggerTime > LastTriggerTime)))
													{
														LastTriggerDate = DrugUpdateTriggerDate;
														LastTriggerTime = IncrementTime (DrugUpdateTriggerTime, 10, &LastTriggerDate);

														strcpy (TableName, "specialist_drugs_need_update");

														ExecSQL (	MAIN_DB,
																	AS400SRV_UPD_tables_update,
																	AS400SRV_UPD_tables_update_unconditional,
																	&DrugUpdateTriggerDate,		&DrugUpdateTriggerTime,
																	&TableName,					END_OF_ARG_LIST				);

														CommitAllWork ();
													}

													// Full-table downloads need the same treatment as the "default" case below.
													if ((msg >= 4000) || (msg == 2071))
													{
														download_start_date			= GetDate ();
														download_start_time			= GetTime ();
														download_msg_initialized	= msg;
// if (msg == 4054) GerrLogMini (GerrId, "BEGIN TABLE for 4054: download_start_date = %d, download_start_time = %d.", download_start_date, download_start_time);
														break;
													}

										// DonR 23Sep2020 end. Note that we do NOT break here for msg below 4000, except for 2071.

										case 2007:	// Over_dose.
										case 2009:	// Interaction_type.
										case 2011:	// Drug_interaction.
										case 2012:	// Drug_interaction deletion.
										case 2013:	// Doctors deletion.
										case 2014:	// Doctors.
										case 2015:	// Speciality_types.
										case 2017:	// Doctor_speciality.
										case 2021:	// Doctor_percents.
										case 2022:	// Doctor_percents deletion - appears to be NIU.
										case 2030:	// Drug_extension.
										case 2031:	// Price_list.
										case 2032:	// Member_price.
										case 2035:	// Suppliers.
										case 2041:	// Member_price deletion.
										case 2042:	// Price_list deletion.
										case 2052:	// DrugNotes.
										case 2053:	// DrugDoctorProf.
										case 2068:	// Gadgets.
										case 2074:	// Drug_extn_doc.
										case 2100:	// Coupons.
											download_start_date			= 0;
											download_start_time			= 0;
											download_msg_initialized	= 0;
											break;

										default:
											download_start_date			= GetDate ();
											download_start_time			= GetTime ();
											download_msg_initialized	= msg;
											break;
									}	// End of initialization switch.


									// At this point, no further processing is done for begin-table commands, and
									// all downloads begin with a begin-table command even if it doesn't have any
									// real significance. Accordingly, there's no point in providing even a
									// diagnostic for an "unexpected" begin-table command.
									// I'll keep the "if" in place just in case later on we actually do need to
									// do something more interesting with begin-table commands.
									if (retcode == MSG_HANDLER_SUCCEEDED)
									{
										retcode = acknowledge_command (p_connect);
									}

									break;	// End of Begin-table command section.


	case TABLE_DATA:
									// DonR 14Mar2010: Provide default value for TrnCode, since it's usually
									// equal to msg.
									TrnCode = msg;

									switch (msg)
									{
									case 2001:	// Pharmacy insert/update.
										retcode				= message_2001_handler (p_connect, len, ATTEMPT_UPDATE_FIRST);
										shm_table_to_update	= PHARMACY;
										FromTable			= "rkfilprd/rk9000p";
										ToTable				= "pharmacy";
										break;

									case 2002:	// Pharmacy delete.
										retcode				= message_2002_handler (p_connect, len);
										shm_table_to_update	= PHARMACY;
										FromTable			= "rkfilprd/rk9000p";
										ToTable				= "pharmacy";
										TrnCode				= 2001;
										break;

									case 2003:	// Special Prescriptions insert/update.
										retcode				= message_2003_handler (p_connect, len, ATTEMPT_UPDATE_FIRST);
										FromTable			= "rkfilprd/rk9002p";
										ToTable				= "special_prescs";
										break;

									case 2004:	// Special Prescriptions delete.
										retcode				= message_2004_handler (p_connect, len);
										FromTable			= "rkfilprd/rk9002p";
										ToTable				= "special_prescs";
										TrnCode				= 2003;
										break;

									// DonR 06Apr2008: Switch to new data-handling routine
									// for drug_list updates. The new version takes the
									// same input data, but does a full incremental compare
									// between new and old data, and updates a larger set
									// of timestamps.
									case 2005:	// Normal drug_list update
									case 2085:	// Drug_list update with no timestamp change -
												// not sent to doctors/pharmacies
										retcode				= data_4005_handler (p_connect, len);
										shm_table_to_update	= DRUG_LIST;
										FromTable			= "rkfilprd/rk9001p";
										ToTable				= "drug_list";
										TrnCode				= 2005;
										break;

									case 2006:	// Delete from drug_list.
										retcode				= message_2006_handler (p_connect, len);
										shm_table_to_update	= DRUG_LIST;
										FromTable			= "rkfilprd/rk9001p";
										ToTable				= "drug_list";
										TrnCode				= 2005;
										break;

									case 2007:	// Over_dose insert/update.
										retcode				= data_2007_handler (p_connect, len);
										FromTable			= "rkfilprd/rk9004p";
										ToTable				= "over_dose";
										break;

									case 2008:	// Over_dose delete.
										retcode				= message_2008_handler (p_connect, len);
										FromTable			= "rkfilprd/rk9004p";
										ToTable				= "over_dose";
										TrnCode				= 2007;
										break;

									case 2009:	// Interaction_type insert/update.
										retcode				= message_2009_handler (p_connect, len, ATTEMPT_UPDATE_FIRST);
										FromTable			= "rkfilprd/rk9012p";
										ToTable				= "interaction_type";
										break;

									case 2010:	// Interaction_type delete.
										retcode				= message_2010_handler (p_connect, len);
										FromTable			= "rkfilprd/rk9012p";
										ToTable				= "interaction_type";
										TrnCode				= 2009;
										break;

									case 2011:	// Drug_interaction insert/update.
										retcode				= message_2011_handler (p_connect, len);
										shm_table_to_update	= DRUG_INTERACTION;
										FromTable			= "rkfilprd/rk9007p";
										ToTable				= "drug_interaction";
										break;

									case 2012:	// Drug_interaction delete.
										retcode				= message_2012_handler (p_connect, len);
										shm_table_to_update	= DRUG_INTERACTION;
										FromTable			= "rkfilprd/rk9007p";
										ToTable				= "drug_interaction";
										TrnCode				= 2011;
										break;

									case 2013:	// Doctors insert/update.
										retcode				= data_2013_handler (p_connect, len);
										FromTable			= "rkfilprd/rk9016p";
										ToTable				= "doctors";
										break;

									case 2014:	// Doctors delete.
										retcode				= message_2014_handler (p_connect, len);
										FromTable			= "rkfilprd/rk9016p";
										ToTable				= "doctors";
										TrnCode				= 2013;
										break;

									case 2015:	// Speciality_types insert/update.
										retcode				= message_2015_handler (p_connect, len, ATTEMPT_UPDATE_FIRST);
										FromTable			= "rkfilprd/rk9018p";
										ToTable				= "speciality_types";
										break;

									case 2016:	// Speciality_types delete.
										retcode				= message_2016_handler (p_connect, len);
										FromTable			= "rkfilprd/rk9018p";
										ToTable				= "speciality_types";
										TrnCode				= 2015;
										break;

									case 2017:	// Doctor_speciality insert/update.
										retcode				= data_2017_handler (p_connect, len);
										shm_table_to_update	= DOCTOR_SPECIALITY;
										FromTable			= "rkfilprd/rk9011p";
										ToTable				= "doctor_speciality";
										break;

									case 2018:	// Doctor_speciality delete.
										retcode				= message_2018_handler (p_connect, len);
										shm_table_to_update	= DOCTOR_SPECIALITY;
										FromTable			= "rkfilprd/rk9011p";
										ToTable				= "doctor_speciality";
										TrnCode				= 2017;
										break;

									case 2019:	// Spclty_largo_prcnt insert/update.
										retcode				= data_4019_handler (p_connect, len);
										shm_table_to_update	= SPCLTY_LARGO_PRCNT;
										FromTable			= "rkfilprd/rk9005p";
										ToTable				= "spclty_largo_prcnt";
										break;

									case 2020:	// Spclty_largo_prcnt delete.
										retcode				= message_2020_handler (p_connect, len);
										shm_table_to_update	= SPCLTY_LARGO_PRCNT;
										FromTable			= "rkfilprd/rk9005p";
										ToTable				= "spclty_largo_prcnt";
										TrnCode				= 2019;
										break;

									case 2021:	// Doctor_percents insert/update.
										retcode				= data_2021_handler (p_connect, len);
										shm_table_to_update	= DOCTOR_PERCENTS;
										FromTable			= "rkfilprd/rk9006p";
										ToTable				= "doctor_percents";
										break;

									case 2022:	// Doctor_percents delete.
										retcode				= message_2022_handler (p_connect, len);
										shm_table_to_update	= DOCTOR_PERCENTS;
										FromTable			= "rkfilprd/rk9006p";
										ToTable				= "doctor_percents";
										TrnCode				= 2021;
										break;

									case 2025:	// Members insert/update.
										retcode				= message_2025_handler (p_connect, len, ATTEMPT_UPDATE_FIRST);
										FromTable			= "applib/members";
										ToTable				= "members";
										break;

									case 2026:	// Members delete.
										retcode				= message_2026_handler (p_connect, len);
										FromTable			= "applib/members";
										ToTable				= "members";
										TrnCode				= 2025;
										break;

									case 2029:	// Member_drug_29g incremental update from RKFILPRD.
										retcode				= data_4029_handler (p_connect, len);
										FromTable			= "rkfilprd/rk9048amp";
										ToTable				= "member_drug_29g";
										TrnCode				= 2029;
										break;

									case 2030:	// Drug_extension insert/update.
										retcode				= data_4030_handler (p_connect, len);
										shm_table_to_update	= DRUG_EXTENSION;
										FromTable			= "rkfilprd/rk9003p";
										ToTable				= "drug_extension";
										break;

									case 2031:	// Price_list insert/update.
										retcode				= data_2031_handler (p_connect, len);
										shm_table_to_update	= PRICE_LIST;
										FromTable			= "rkfilprd/rk9017p";
										ToTable				= "price_list";
										break;

									case 2032:	// Member_price insert/update.
										retcode				= data_2032_handler (p_connect, len);
										shm_table_to_update	= MEM_PRICE;
										FromTable			= "rkfilprd/rk9014p";
										ToTable				= "member_price";
										break;

									case 2035:	// Suppliers insert/update.
										retcode				= data_2035_handler (p_connect, len);
										shm_table_to_update	= SUPPLIERS;
										FromTable			= "rkfilprd/rk9019p";
										ToTable				= "suppliers";
										break;

									case 2036:	// Macabi_centers insert/update.
										retcode				= message_2036_handler (p_connect, len, ATTEMPT_UPDATE_FIRST);
										shm_table_to_update	= MAC_CENTERS;
										FromTable			= "rkfilprd/rk9015p";
										ToTable				= "macabi_centers";
										break;

									case 2037:	// Macabi_centers delete.
										retcode				= message_2037_handler (p_connect, len);
										shm_table_to_update	= MAC_CENTERS;
										FromTable			= "rkfilprd/rk9015p";
										ToTable				= "macabi_centers";
										TrnCode				= 2036;
										break;

									case 2041:	// Member_price delete.
										retcode				= message_2041_handler (p_connect, len);
										shm_table_to_update	= MEM_PRICE;
										FromTable			= "rkfilprd/rk9014p";
										ToTable				= "member_price";
										TrnCode				= 2032;
										break;

									case 2042:	// Price_list delete.
										retcode				= message_2042_handler (p_connect, len);
										shm_table_to_update	= PRICE_LIST;
										FromTable			= "rkfilprd/rk9017p";
										ToTable				= "price_list";
										TrnCode				= 2031;
										break;

									case 2043:	// Drug_extension delete.
										retcode				= message_2043_handler (p_connect, len);
										FromTable			= "rkfilprd/rk9003p";
										ToTable				= "drug_extension";
										TrnCode				= 2030;
										break;

									case 2048:	// GenComponents insert/update.
										retcode				= data_2048_handler (p_connect, len);
										FromTable			= "rkfilprd/rk9032p";
										ToTable				= "GenComponents";
										break;

									case 2049:	// DrugGenComponents insert/update.
										retcode				= message_2049_handler (p_connect, len, ATTEMPT_UPDATE_FIRST,
																					&shm_table_to_update);
										FromTable			= "rkfilprd/rk9033p";
										ToTable				= "DrugGenComponents";
										break;

									case 2050:	// GeneryInteraction insert/update.
										retcode				= message_2050_handler (p_connect, len, ATTEMPT_UPDATE_FIRST,
																					&shm_table_to_update);
										FromTable			= "rkfilprd/rk9034p";
										ToTable				= "GeneryInteraction";
										break;

									case 2051:	// GnrlDrugNotes insert/update.
										retcode				= message_2051_handler (p_connect, len, ATTEMPT_UPDATE_FIRST,
																					&shm_table_to_update);
										FromTable			= "rkfilprd/rk9035p";
										ToTable				= "GnrlDrugNotes";
										break;

									case 2052:	// DrugNotes insert/update.
										retcode				= data_2052_handler (p_connect, len);
										FromTable			= "rkfilprd/rk9036p";
										ToTable				= "DrugNotes";
										shm_table_to_update = DRUGNOTES;
										break;

									case 2053:	// DrugDoctorProf insert/update.
										retcode				= data_4053_handler (p_connect, len);
										shm_table_to_update = DRUGDOCTORPROF;
										FromTable			= "rkfilprd/rk9037p";
										ToTable				= "DrugDoctorProf";
										break;

									case 2054:	// EconomyPri insert/update.
										retcode				= data_4054_handler (p_connect, len);
										shm_table_to_update	= ECONOMYPRI;
										FromTable			= "rkfilprd/rk9038p";
										ToTable				= "EconomyPri";
										break;

									case 2055:	// Pharmacology insert/update.
										retcode				= message_2055_handler (p_connect, len, ATTEMPT_UPDATE_FIRST,
																					&shm_table_to_update);
										FromTable			= "rkfilprd/rk9039p";
										ToTable				= "Pharmacology";
										break;

									case 2056:	// GenInterNotes insert/update.
										retcode				= message_2056_handler (p_connect, len, ATTEMPT_UPDATE_FIRST,
																					&shm_table_to_update);
										FromTable			= "rkfilprd/rk9040p";
										ToTable				= "GenInterNotes";
										break;

									case 2061:	// Pharmacy_contract DELETE ALL ROWS.
										retcode				= message_2061_handler (p_connect, len);
										shm_table_to_update	= PHARMACY_CONTRACT;
										FromTable			= "rkfilprd/rk9045p";
										ToTable				= "pharmacy_contract";
										TrnCode				= 2062;
										break;

									case 2062:	// Pharmacy_contract insert.
										retcode				= message_2062_handler (p_connect, len);
										shm_table_to_update	= PHARMACY_CONTRACT;
										FromTable			= "rkfilprd/rk9045p";
										ToTable				= "pharmacy_contract";
										break;

									case 2063:	// Prescr_source DELETE ALL ROWS.
										retcode				= message_2063_handler (p_connect, len);
										shm_table_to_update	= PRESC_SOURCE;
										FromTable			= "rkfilprd/rk9043p";
										ToTable				= "prescr_source";
										TrnCode				= 2064;
										break;

									case 2064:	// Prescr_source insert.
										retcode				= message_2064_handler (p_connect, len);
										shm_table_to_update	= PRESC_SOURCE;
										FromTable			= "rkfilprd/rk9043p";
										ToTable				= "prescr_source";
										break;

									case 2065:	// Confirm_authority DELETE ALL ROWS.
										retcode				= message_2065_handler (p_connect, len);
										shm_table_to_update	= CONFIRM_AUTHORITY;
										FromTable			= "rkfilprd/rk9044p";
										ToTable				= "confirm_authority";
										TrnCode				= 2066;
										break;

									case 2066:	// Confirm_authority insert.
										retcode				= message_2066_handler (p_connect, len);
										shm_table_to_update	= CONFIRM_AUTHORITY;
										FromTable			= "rkfilprd/rk9044p";
										ToTable				= "confirm_authority";
										break;

									case 2067:	// Gadgets DELETE ALL ROWS.
										retcode				= message_2067_handler (p_connect, len);
										shm_table_to_update	= GADGETS;
										FromTable			= "rkfilprd/rk9099p";
										ToTable				= "gadgets";
										TrnCode				= 2068;
										break;

									case 2068:	// Gadgets incremental update from RKFILPRD.
										retcode				= data_4068_handler (p_connect, len);
										shm_table_to_update	= GADGETS;
										FromTable			= "rkfilprd/rk9099p";
										ToTable				= "gadgets";
										break;

									case 2069:	// ClicksDiscount insert/update.
										retcode				= message_2069_handler (p_connect, len, ATTEMPT_UPDATE_FIRST);
										shm_table_to_update	= CLICKS_DISCOUNT;
										FromTable			= "rkfilprd/rk9060p";
										ToTable				= "ClicksDiscount";
										break;

									case 2070:	// DrugForms insert/update.
										retcode				= message_2070_handler (p_connect, len, ATTEMPT_UPDATE_FIRST);
										shm_table_to_update	= DRUG_FORMS;
										FromTable			= "rkfilprd/rk9062p";
										ToTable				= "DrugForms";
										break;

									case 2071:	// UsageInstructions insert/update incremental from RKFILPRD.
										retcode				= data_4071_handler (p_connect, len, ATTEMPT_UPDATE_FIRST);
										shm_table_to_update	= USAGE_INSTRUCTIONS;
										FromTable			= "rkfilprd/rk9067p";
										ToTable				= "UsageInstructions";
										break;

									case 2072:	// TreatmentGroup insert/update.
										retcode				= message_2072_handler (p_connect, len, ATTEMPT_UPDATE_FIRST);
										shm_table_to_update	= TREATMENT_GROUP;
										FromTable			= "rkfilprd/rk9063p";
										ToTable				= "TreatmentGroup";
										break;

									case 2073:	// PrescrPeriod insert/update.
										retcode				= message_2073_handler (p_connect, len, ATTEMPT_UPDATE_FIRST);
										shm_table_to_update	= PRESCR_PERIOD;
										FromTable			= "rkfilprd/rk9064p";
										ToTable				= "PrescrPeriod";
										break;

									// DonR 02Sep2009: Switch to new data-handling routine
									// for drug_extn_doc updates. The new version takes the
									// same input data, but does a full incremental compare
									// between new and old data, and updates a larger set
									// of timestamps.
									case 2074:	// DrugExtnDoc insert/update.
										retcode				= data_4074_handler (p_connect, len);
										shm_table_to_update	= DRUG_EXTN_DOC;
										FromTable			= "rkfilprd/rk9065p";
										ToTable				= "DrugExtnDoc";
										break;

									case 2075:	// Sale insert/update.
										retcode				= message_2075_handler (p_connect, len, ATTEMPT_UPDATE_FIRST);
										shm_table_to_update	= SALE;
										FromTable			= "rkfilprd/rk9070p";
										ToTable				= "sale";
										break;

									case 2076:	// Sale_fixed_price insert/update.
										retcode				= message_2076_handler (p_connect, len, ATTEMPT_UPDATE_FIRST);
										shm_table_to_update	= SALE_FIXED_PRICE;
										FromTable			= "rkfilprd/rk9071p";
										ToTable				= "sale_fixed_price";
										break;

									case 2077:	// Sale_bonus_recv insert/update.
										retcode				= message_2077_handler (p_connect, len, ATTEMPT_UPDATE_FIRST);
										shm_table_to_update	= SALE_BONUS_RECV;
										FromTable			= "rkfilprd/rk9072p";
										ToTable				= "sale_bonus_recv";
										break;

									case 2078:	// Sale_target insert/update.
										retcode				= message_2078_handler (p_connect, len, ATTEMPT_UPDATE_FIRST);
										shm_table_to_update	= SALE_TARGET;
										FromTable			= "rkfilprd/rk9073p";
										ToTable				= "sale_target";
										break;

									case 2079:	// Sale_pharmacy insert/update.
										retcode				= message_2079_handler (p_connect, len, ATTEMPT_UPDATE_FIRST);
										shm_table_to_update	= SALE_PHARMACY;
										FromTable			= "rkfilprd/rk9074p";
										ToTable				= "sale_pharmacy";
										break;

									case 2080:	// PharmDrugNotes insert/update.
										retcode				= data_2080_handler (p_connect, len);
										shm_table_to_update	= PHARMDRUGNOTES;	// DonR 07Jan2025 Bug #381240 HOT FIX.
										FromTable			= "rkfilprd/rk9055p";
										ToTable				= "PharmDrugNotes";
										break;

									case 2081:	// Purchase Limit Ishur insert/update.
										retcode				= message_2081_handler (p_connect, len);
										shm_table_to_update	= PURCHASE_LIM_ISHUR;
										FromTable			= "rkfilprd/rk9068p";
										ToTable				= "PurchaseLimitIshur";
										break;

									case 2082:	// Drug Purchase Limit insert/update.
										retcode				= message_2082_handler (p_connect, len);
										shm_table_to_update	= DRUG_PURCHASE_LIM;
										FromTable			= "rkfilprd/rk9069p";
										ToTable				= "DrugPurchaseLimit";
										break;

									case 2084:	// DrugPurchaseLimit DELETE ALL ROWS.
										retcode				= message_2084_handler (p_connect, len);
										shm_table_to_update	= DRUG_PURCHASE_LIM;
										FromTable			= "rkfilprd/rk9069p";
										ToTable				= "DrugPurchaseLimit";
										TrnCode				= 2082;
										break;

									case 2086:	// MemberPharm insert to scratch table.
										retcode				= data_2086_handler (p_connect, len);
										shm_table_to_update	= MEMBER_PHARM;
										FromTable			= "rkfilprd/rk9097p";
										ToTable				= "MemberPharm";
										break;

									case 2096:	// Purchase Limit Class full download.
										retcode				= data_2096_handler (p_connect, len);
										shm_table_to_update	= PURCH_LIM_CLASS;
										FromTable			= "rkfilprd/rk9057p";
										ToTable				= "purchase_lim_class";
										break;

									// DonR 14Mar2010: New table for "Nihul Tikrot".
									case 2100:	// Intra-day coupons update - insertions only.
										retcode				= data_4100_handler (p_connect, len);
										shm_table_to_update	= COUPONS;
										FromTable			= "rkfilprd/rk9059p";
										ToTable				= "Coupons";
										break;

									case 4005:	// Full Drug-list download.
										retcode				= data_4005_handler (p_connect, len);
										shm_table_to_update	= DRUG_LIST;
										FromTable			= "rkunix/rk9001p";
										ToTable				= "drug_list full";
										break;

									case 4007:	// Full Over_dose download.
										retcode				= data_2007_handler (p_connect, len);
										FromTable			= "rkunix/rk9004p";
										ToTable				= "over_dose full";
										break;

									case 4009:	// Full interaction_type download.
										retcode				= message_2009_handler (p_connect, len, ATTEMPT_UPDATE_FIRST);
										FromTable			= "rkunix/rk9012p";
										ToTable				= "interaction_type full";
										break;

									case 4011:	// Full drug_interaction download.
										retcode				= message_2011_handler (p_connect, len);
										shm_table_to_update	= DRUG_INTERACTION;
										FromTable			= "rkunix/rk9007p";
										ToTable				= "drug_interaction full";
										break;

									case 4013:	// Full Doctors download.
										retcode				= data_2013_handler (p_connect, len);
										FromTable			= "rkunix/rk9016p";
										ToTable				= "doctors full";
										break;

									case 4015:	// Full speciality_types download.
										retcode				= message_2015_handler (p_connect, len, ATTEMPT_UPDATE_FIRST);
										FromTable			= "rkunix/rk9018p";
										ToTable				= "speciality_types full";
										break;

									case 4017:	// Full doctor_speciality download.
										retcode				= data_2017_handler (p_connect, len);
										shm_table_to_update	= DOCTOR_SPECIALITY;
										FromTable			= "rkunix/rk9011p";
										ToTable				= "doctor_speciality full";
										break;

									case 4019:	// Full spclty_largo_prcnt download.
										retcode				= data_4019_handler (p_connect, len);
										shm_table_to_update	= SPCLTY_LARGO_PRCNT;
										FromTable			= "rkunix/rk9005p";
										ToTable				= "spclty_largo_prcnt full";
										break;

									case 4021:	// Full doctor_percents download.
										retcode				= data_2021_handler (p_connect, len);
										shm_table_to_update	= DOCTOR_PERCENTS;
										FromTable			= "rkunix/rk9006p";
										ToTable				= "doctor_percents full";
										break;

									case 4029:	// Full member_drug_29g download.
										retcode				= data_4029_handler (p_connect, len);
										FromTable			= "rkunix/rk9048amp";
										ToTable				= "member_drug_29g full";
										break;

									case 4030:	// Full drug_extension download.
										retcode				= data_4030_handler (p_connect, len);
										shm_table_to_update	= DRUG_EXTENSION;
										FromTable			= "rkunix/rk9003p";
										ToTable				= "drug_extension full";
										break;

									case 4031:	// Full price_list download.
										retcode				= data_2031_handler (p_connect, len);
										shm_table_to_update	= PRICE_LIST;
										FromTable			= "rkunix/rk9017p";
										ToTable				= "price_list full";
										break;

									case 4032:	// Full member_price insert/update.
										retcode				= data_2032_handler (p_connect, len);
										shm_table_to_update	= MEM_PRICE;
										FromTable			= "rkunix/rk9014p";
										ToTable				= "member_price full";
										break;

									case 4035:	// Full Suppliers insert/update.
										retcode				= data_2035_handler (p_connect, len);
										shm_table_to_update	= SUPPLIERS;
										FromTable			= "rkunix/rk9019p";
										ToTable				= "suppliers full";
										break;
									
									// Marianna 18Jul2023 User Story #456129
									case 4048:	// GenComponents insert/update.
										retcode				= data_2048_handler (p_connect, len);
										FromTable			= "rkunix/rk9032p";
										ToTable				= "GenComponents full";
										break;

									case 4052:	// Full DrugNotes download.
										retcode				= data_2052_handler (p_connect, len);
										FromTable			= "rkunix/rk9036p";
										ToTable				= "DrugNotes full";
										shm_table_to_update = DRUGNOTES;
										break;

									case 4053:	// Full DrugDoctorProf download.
										retcode				= data_4053_handler (p_connect, len);
										shm_table_to_update = DRUGDOCTORPROF;
										FromTable			= "rkunix/rk9037p";
										ToTable				= "DrugDoctorProf full";
										break;

									case 4054:	// Full EconomyPri download.
										retcode				= data_4054_handler (p_connect, len);
										shm_table_to_update	= ECONOMYPRI;
										FromTable			= "rkunix/rk9038p";
										ToTable				= "EconomyPri full";
										break;

									case 4068:	// Gadgets full-table update from RKUNIX.
										retcode				= data_4068_handler (p_connect, len);
										shm_table_to_update	= GADGETS;
										FromTable			= "rkunix/rk9099p";
										ToTable				= "gadgets full";
										break;

									case 4071:	// UsageInstructions full-table update from RKUNIX.
										retcode				= data_4071_handler (p_connect, len, ATTEMPT_UPDATE_FIRST);
										shm_table_to_update	= USAGE_INSTRUCTIONS;
										FromTable			= "rkunix/rk9067p";
										ToTable				= "UsageInstructions";
										break;

									case 4074:	// Full drug_extn_doc download.
										retcode				= data_4074_handler (p_connect, len);
										shm_table_to_update	= DRUG_EXTN_DOC;
										FromTable			= "rkunix/rk9065p";
										ToTable				= "DrugExtnDoc full";
										break;

									//Marianna 18Jul2023 User Story #456129
									case 4080:	// Full PharmDrugNotes insert/update.
										retcode				= data_2080_handler (p_connect, len);
										shm_table_to_update	= PHARMDRUGNOTES;	// DonR 07Jan2025 Bug #381240 HOT FIX.
										FromTable			= "rkunix/rk9055p";
										ToTable				= "PharmDrugNotes full";
										break;

									// DonR 14Mar2010: New tables for "Nihul Tikrot".
									case 4100:	// Full coupons download.
										retcode				= data_4100_handler (p_connect, len);
//										shm_table_to_update	= COUPONS;
										FromTable			= "rkunix/rk9059p";
										ToTable				= "Coupons full";
										break;

									case 4101:	// Full subsidy_messages download.
										retcode				= data_4101_handler (p_connect, len);
//										shm_table_to_update	= SUBSIDY_MESSAGES;
										FromTable			= "rkunix/rk9076p";
										ToTable				= "Subsidy_messages full";
										break;

									case 4102:	// Full tikra_type download.
										retcode				= data_4102_handler (p_connect, len);
//										shm_table_to_update	= TIKRA_TYPE;
										FromTable			= "rkunix/rk9077p";
										ToTable				= "Tikra_type full";
										break;

									case 4103:	// Full credit_reasons download.
										retcode				= data_4103_handler (p_connect, len);
//										shm_table_to_update	= CREDIT_REASONS;
										FromTable			= "rkunix/rk9078p";
										ToTable				= "Credit_reasons full";
										break;

									case 4104:	// Full hmo_membership download.
										retcode				= data_4104_handler (p_connect, len);
//										shm_table_to_update	= HMO_MEMBERSHIP;
										FromTable			= "rkunix/rk9079p";
										ToTable				= "HMO_membership full";
										break;

									case 4105:	// Full drug_shape download.
										retcode				= data_4105_handler (p_connect, len);
										FromTable			= "rkunix/rk9096p";
										ToTable				= "Drug_shape full";
										break;

									case 4106:	// Full drug_diagnoses download.
										retcode				= data_4106_handler (p_connect, len);
										FromTable			= "rkunix/rk9103p";
										ToTable				= "Drug_diagnoses full";
										break;
									
									// Marianna 15Sep2020 CR #32620
									case 4107:	// Full how_to_take download.
										retcode				= data_4107_handler (p_connect, len);
										FromTable			= "rkunix/rk9254P";
										ToTable				= "How_to_take full";
										break;
										
									case 4108:	// Full unit_of_measure download.
										retcode				= data_4108_handler (p_connect, len);
										FromTable			= "rkunix/rk9256P";
										ToTable				= "Unit_of_measure full";
										break;
										
									case 4109:	// Full reason_not_sold download.
										retcode				= data_4109_handler (p_connect, len);
										FromTable			= "rkunix/rk9253P";
										ToTable				= "Reason_not_sold full";
										break;
									
									case 4110:	// Full usage_instr_reason_changed download.
										retcode				= data_4110_handler (p_connect, len);
										FromTable			= "rkunix/rk9257P";
										ToTable				= "Usage_instr_reason_changed ful";	// DonR 18Mar2025 avoid SQL truncation message.
										break;									
									// END CR #32620

									// DonR 17Oct2021 User Story #196891.
									case 4111:	// Full member_blocked_drugs download.
										retcode				= data_4111_handler (p_connect, len);
										FromTable			= "rkunix/rk9310P";
										ToTable				= "Member_blocked_drugs full";
										break;									

									// DonR 14Mar2010: Established ghetto for transactions/tables
									// no longer in use (if they ever were).
									case 2023:	// Limited Doctors - no longer in use!
									case 2024:	// Delete Limited Doctors - no longer in use!
									case 2033:	// Refund_presc - no longer in use, if it ever was.
									case 2034:	// Major_pharmacist - no longer in use, if it ever was.
									case 2039:	// Major_pharmacist - no longer in use, if it ever was.
									case 2040:	// Refund_presc - no longer in use, if it ever was.
										break;


									default:
										GerrLogReturn (GerrId,
													   "Got unknown table-data message id: %d",
													   msg);
										break;

									}	// End of table-data switch.
									break;


	case END_TABLE_COMMAND:
									// By default, assume we need a "manual" COMMIT after postprocessing.
									need_postprocess_commit = MAC_TRUE;

									// Set row counters to a nonsense value; only if it's been changed to
									// something reasonable will we use it for updating as400in_audit.
									active_rows = total_rows = -999;


									switch (msg)
									{
										case 2086:	// Process MemberPharm download.
											retcode	= postprocess_2086 (p_connect);
											break;

										case 2096:	// Process Purchase Limit Class download.
											retcode	= postprocess_2096 (p_connect);
											break;

										case 2005:	// Incremental Drug-list update.
										case 2085:	// Incremental Drug-list update (special).
										case 4005:	// Process full Drug-list download.
											need_postprocess_commit = MAC_FALSE;
											postprocess_4005 ();	// Just to get the current drug_list table size and update
																	// the x timestamp.
											retcode = MSG_HANDLER_SUCCEEDED;
											break;

										case 4007:	// Process full over_dose download.
											retcode = postprocess_4007 (p_connect);	// Logically delete "missing" rows.
											break;

										case 4009:	// Process full interaction_type download.
											retcode = postprocess_4009 (p_connect);	// Logically delete "missing" rows.
											break;

										case 4011:	// Process full drug_interaction download.
											retcode = postprocess_4011 (p_connect);	// Logically delete "missing" rows.
											break;

										case 4013:	// Process full doctors download.
											retcode = postprocess_4013 (p_connect);	// Logically delete "missing" rows.
											break;

										case 4015:	// Process full speciality_types download.
											retcode = postprocess_4015 (p_connect);	// Logically delete "missing" rows.
											break;

										case 4017:	// Process full doctor_speciality download.
											retcode = postprocess_4017 (p_connect);	// Logically delete "missing" rows.
											break;

										case 4019:	// Process full spclty_largo_29prcnt download.
											retcode = postprocess_4019 (p_connect);	// Logically delete "missing" rows.
											break;

										case 4021:	// Process full doctor_percents download.
											retcode = postprocess_4021 (p_connect);	// Logically delete "missing" rows.
											break;

										case 4029:	// Process full member_drug_29g download.
											retcode = postprocess_4029 (p_connect);	// Delete "missing" rows.
											break;

										case 4030:	// Process full drug_extension download.
											retcode = postprocess_4030 (p_connect);	// Logically delete "missing" rows.
											break;

										case 4031:	// Process full price_list download.
											retcode = postprocess_4031 (p_connect);	// Delete "missing" rows.
											break;

										case 4032:	// Process full member_price download.
											retcode = postprocess_4032 (p_connect);	// Delete "missing" rows.
											break;

										case 4035:	// Process full suppliers download.
											retcode = postprocess_4035 (p_connect);	// Delete "missing" rows.
											break;

										// Marianna 23Jul2023 User Story #456129
										case 4048:	// Process full GenComponents download.
											retcode = postprocess_4048 (p_connect);	//  Delete "missing" rows.
											break;

										case 4052:	// Process full drugnotes download.
											retcode = postprocess_4052 (p_connect);	// Logically delete "missing" rows.
											break;

										case 4053:	// Process full drugdoctorprof download.
											retcode = postprocess_4053 (p_connect);	// Logically delete "missing" rows.
											break;

										case 4054:	// Process full economypri download.
											retcode = postprocess_4054 (p_connect);	// Logically delete "missing" rows.
											break;

										case 4068:	// Process full gadgets download.
											retcode = postprocess_4068 (p_connect);	// Physically delete "missing" rows.
											break;

										case 4071:	// Process full usage_instruct download.
											retcode = postprocess_4071 (p_connect);	// Physically delete "missing" rows.
											break;

										case 4074:	// Process full drug_extn_doc download.
											retcode = postprocess_4074 (p_connect);	// Logically delete "missing" rows.
											break;

										// Marianna 23Jul2023 User Story #456129
										case 4080:	// Process full PharmDrugNotes download.
											retcode = postprocess_4080 (p_connect);	// Logically delete "missing" rows.
											break;

										// DonR 14Mar2010: New tables for "Nihul Tikrot".
										case 4100:	// Full coupons download.
											retcode = postprocess_4100 (p_connect);	// Logically delete "missing" rows.
											break;

										case 4101:	// Full subsidy_messages download.
											retcode = postprocess_4101 (p_connect);	// Delete "missing" rows.
											break;

										case 4102:	// Full tikra_type download.
											retcode = postprocess_4102 (p_connect);	// Delete "missing" rows.
											break;

										case 4103:	// Full credit_reasons download.
											retcode = postprocess_4103 (p_connect);	// Delete "missing" rows.
											break;

										case 4104:	// Full hmo_membership download.
											retcode = postprocess_4104 (p_connect);	// Delete "missing" rows.
											break;

										case 4105:	// Full drug_shape download.
											retcode = postprocess_4105 (p_connect);	// Delete "missing" rows.
											break;

										case 4106:	// Full drug_diagnoses download.
											retcode = postprocess_4106 (p_connect);	// Delete "missing" rows.
											break;
										
										// Marianna 23Sep2020 CR #32620 begin.
										case 4107:	// Full how_to_take download.
											retcode = postprocess_4107 (p_connect);	// Delete "missing" rows.
											break;
										
										case 4108:	// Full unit_of_measure download.
											retcode = postprocess_4108 (p_connect);	// Delete "missing" rows.
											break;
										
										case 4109:	// Full reason_not_sold download.
											retcode = postprocess_4109 (p_connect);	// Delete "missing" rows.
											break;

										case 4110:	// Full usage_instr_reason_changed download.				// Marianna 23Sep2020 CR #32620  
											retcode = postprocess_4110 (p_connect);	// Delete "missing" rows.
											break;
										// Marianna 23Sep2020 CR #32620 end.

										// DonR 17Oct2021 User Story #196891.
										case 4111:	// Full member_blocked_drugs download.
											retcode = postprocess_4111 (p_connect);	// Delete "missing" rows and clear members/has_blocked_drugs flags.
											break;

										// DonR 11Aug2011: Accept "unknown" end-table commands without
										// generating a message or an error.
										default:
//											GerrLogMini (GerrId, "Got unknown end-table message id: %d", msg);
											need_postprocess_commit	= MAC_FALSE;
											retcode					= MSG_HANDLER_SUCCEEDED;
//											retcode = UNKNOWN_COMMAND;
											break;

									}	// End of end-table-command switch.


									// Postprocess routines are responsible for setting correct values for total_rows
									// and active_rows. Store them in as400in_audit.
									if (total_rows > -1)
									{
										ExecSQL (	MAIN_DB, AS400SRV_UPD_as400in_audit_table_size_columns,
													&total_rows,	&active_rows,
													&MessageCode,	END_OF_ARG_LIST							);
									}


									if (retcode == MSG_HANDLER_SUCCEEDED)
									{
										retcode = acknowledge_command (p_connect);

										// DonR 08Dec2009: Since AS/400 doesn't send a COMMIT message
										// for postprocessors, we have to trigger the DB commit "by hand".
										if (need_postprocess_commit)
										{
											retcode = commit_handler (p_connect, shm_table_to_update);
										}
									}	// Postprocessor succeeded.


									// Clear download-status variables.
									download_start_date			= 0;
									download_start_time			= 0;
									download_msg_initialized	= 0;
									download_row_count			= 0;

									break;


	default:
									GerrLogReturn (GerrId,
												   "Got unknown message type: %d",
												   msg_type);
									break;

	}	// End of switch on Message Type.


	return retcode ;
}



/*===============================================================
||																||
||		answer_to_all_client_requests()							||
||																||
 ===============================================================*/
static int answer_to_all_client_requests (fd_set * called_sock_mask)
{
	// local variables
	TSocket			sock;		// socket-array iterator
	TConnection		*a_client;	// ptr to current client-struct
	TSystemMsg		msg;		// ID of last received message
	TSystemMsg		msg_type;	// type of last received data message
	TSuccessStat	retval;		// indicating receive() success
	int				data_len;	// length of message without header
	int				rc;			// code returned from message handler
	int				conn_id;
	static int		load_time	= 0;
	static int		load_count	= 0;


	// scan all connections
    for (conn_id = 0; conn_id < MAX_NUM_OF_CLIENTS; conn_id++)
    {
		a_client = &glbConnectionsArray [conn_id];

		// Check if connection is online
		if (a_client->server_state == 0)
		{
		    continue;
		}

		// Check if connection has a message.
		if (!FD_ISSET (a_client->socket, called_sock_mask))
		{
			// DonR 08Apr2021: If we are already in the middle of a receiving a batch of
			// updates, any long pause indicates a probable problem on the client (CL.EC)
			// end. In this case, we don't want to leave a database transaction open,
			// since (at least now that we're working in MS-SQL) this can cause contention
			// problems for other programs. Accordingly, if the in-transaction flag is
			// set TRUE but FD_ISSET() returned FALSE, test how long we've been waiting
			// for the next message - and if it's 3 seconds or longer, terminate the
			// connection with an ERROR_ON_RECEIVE error.
			if ((connection_is_in_transaction (a_client)) && ((time (NULL) - a_client->last_access) > 2))
			{
				GerrLogMini (GerrId, "Stopped in middle of batch - terminating with rollback!");
				terminate_a_connection (a_client, WITH_ROLLBACK);
			}

			continue;
		}	// Socket does NOT have data ready to read.

		// Set to connection in-transaction.
		// DonR 08Apr2021: There's no need to check whether the connection is already in a
		// transaction before setting the in-transaction flag - just set it unconditionally.
		a_client->last_access = time (NULL);
		set_connection_to_in_transaction_state (a_client);
//		if (connection_is_not_in_transaction (a_client))
//		{
//			set_connection_to_in_transaction_state (a_client);
//		}

		// Get message id from socket
		msg = get_message_ID (a_client);

		if ((msg == BAD_DATA_RECEIVED) || (msg == ERROR_ON_RECEIVE))
		{
			GerrLogReturn(
				  GerrId,
				  "Received bad data -- terminating with rollback"
				  "\ndata : \"%.60s\""
				  "\nsocket : %d"
				  "\nentry : %d"
				  "\nError = %s",
				  glbMsgBuffer,
				  a_client->socket,
				  a_client-glbConnectionsArray,
				  (msg == BAD_DATA_RECEIVED) ? "BAD_DATA_RECEIVED" : "ERROR_ON_RECEIVE"
				  );

			terminate_a_connection (a_client, WITH_ROLLBACK);

			continue;
		}

		//printf("Got message < %d >\n", msg);fflush(stdout);

		// If not a control message -- read message data
		if (NOT_A_COMMUNICATION_MSG (msg))
		{
			// If too loaded -- nap a bit
			ret = time (NULL);

			if (ret != load_time || load_count > 15)
			{
				load_count = 0;
				load_time  = ret;
			}

			load_count++;


			msg_type = get_data_message_type (a_client, msg);

			if (msg_type == TABLE_DATA)
			{
				// Read message data
				data_len = get_struct_len (msg);

				retval = receive_from_socket (a_client, glbMsgBuffer, data_len);

				if (retval == failure)
				{
					GerrLogReturn(
								  GerrId,
								  "received bad data -- terminating with rollback"
								  "\ndata : \"%.20s...\""
								  "\nsocket : %d"
								  "\nentry : %d",
								  glbMsgBuffer,
								  a_client->socket,
								  a_client-glbConnectionsArray
								 );

					terminate_a_connection( a_client, WITH_ROLLBACK );

					continue;
				}	// Failed to get table data message.
			}	// Current message contains table data.

			else if (msg_type == CLIENT_LOG_MESSAGE)
			{
				receive_from_socket (a_client, glbMsgBuffer, 70);
				glbMsgBuffer[70] = (char)0;
				strip_spaces (glbMsgBuffer);
				GerrLogMini (GerrId, "Trn %d: client reports \"%s\"", msg, glbMsgBuffer);
				continue;
			}

			else
			{
				data_len = -1; // Not used for begin-table/end-table commands.
			}

		}	// Not a communications message.
		else
		{
			msg_type = COMMUNICATIONS_MESSAGE;
		    data_len = -1; // Not used for Commit or other communications messages.
		}


		// Call message handler
		rc = call_msg_handler_pharm (msg, msg_type, a_client, data_len);

		switch (rc)
		{
			case MSG_HANDLER_SUCCEEDED:
										break;

			case SQL_ERROR_OCCURED:
										a_client->sql_err_flg = 1;
										break;

			case DATA_PARSING_ERR:

										GerrLogReturn (GerrId,
													   "DATA_PARSING_ERR for msg %d",
													   msg);
										terminate_a_connection (a_client, WITH_ROLLBACK);
										break;

			case ALL_RETRIES_WERE_USED:
										GerrLogReturn (GerrId,
													   "ALL_RETRIES_WERE_USED for msg %d",
													   msg);
										a_client->sql_err_flg = 1;
										break;

			default:
										GerrLogReturn (GerrId,
													   "Unknown call_msg_handler_pharm() return code: %d",
													   rc);
										break;
		}			// switch( return code

    }				// for( sockets


    return success;
}



/* << -------------------------------------------------------- >> */
/*                                                                */
/*	    update shared memory params & get system's		  */
/*		    "busy" & "going down" state			  */
/*                                                                */
/* << -------------------------------------------------------- >> */
static int get_sysem_status(
			     bool	*pSysDown,
			     bool	*pSysBusy
			     )
{
    if ( *pSysDown != true )
    {
	RETURN_ON_ERR( UpdateShmParams( ) );

	if( ToGoDown( PHARM_SYS ) )
	{
	    *pSysDown = true ;
	}
    }

    return(MAC_OK);

}


/* << -------------------------------------------------------- >> */
/*                                                                */
/*     get a short-int value from buffer & promote buf-ptr		  */
/*                                                                */
/* << -------------------------------------------------------- >> */
static short get_short (	char	**p_buffer,
							int		ascii_len	)
{
	char	tmp[20];
	int		i;


	memcpy (tmp, *p_buffer, ascii_len);
	tmp[ascii_len] = 0;

	// verify token is numeric:
	for( i = 0  ; i < ascii_len ;  i++ )
	{
		if( legal_digit( tmp, i ) == false )
		{
			glbErrorFlg = true;

			GerrLogReturn (GerrId,
						   "Get_short: non-numeric character at pos %d in {%s}, L = %d",
						   i, tmp, ascii_len);

			return -1;
		}
	}

	*p_buffer  +=  ascii_len ;    /*  promote buffer position  */

	return (short)atoi( tmp );
}



/* << -------------------------------------------------------- >> */
/*                                                                */
/*     get a long-int value from buffer & promote buf-ptr		  */
/* DonR 22Sep2020: Leave the function name as-is, but make it	  */
/* type int rather than long.									  */
/*                                                                */
/* << -------------------------------------------------------- >> */
static int get_long (	char	**p_buffer,
						int		ascii_len	)
{
	char	tmp[64];
	int	i;


	memcpy( tmp , *p_buffer , ascii_len ) ;
	tmp[ascii_len] = 0;

	// verify token is numeric:
	for( i = 0  ; i < ascii_len ;  i++ )
	{
		if( legal_digit( tmp, i ) == false )
		{
			glbErrorFlg = true;

			GerrLogReturn (GerrId,
						   "Get_long: non-numeric character at pos %d in {%s}, L = %d",
						   i, tmp, ascii_len);

			return -1L;
		}
	}

	*p_buffer  +=  ascii_len ;    /*  promote buffer position  */

	return atoi( tmp );
}


#if 0
/* << -------------------------------------------------------- >> */
/*                                                                */
/*     get a long-int value from buffer & promote buf-ptr		  */
/*                                                                */
/*     DonR 17Jan2011: Special version for Special Prescs         */
/*	   also gets the number not including the first digit.		  */
/*                                                                */
/* << -------------------------------------------------------- >> */
static long get_long_special (
		     char	**p_buffer,
		     int	ascii_len,
			 long	*minus_first_digit_out
		     )
{
	char	tmp[64];
	int		i;
	char	*second_digit		= NULL;
	long	minus_first_digit	= 0L;


	memcpy( tmp , *p_buffer , ascii_len ) ;
	tmp[ascii_len] = 0;

	// verify token is numeric:
	for( i = 0  ; i < ascii_len ;  i++ )
	{
		if( legal_digit( tmp, i ) == false )
		{
			glbErrorFlg = true;

			GerrLogReturn (GerrId,
						   "Get_long: non-numeric character at pos %d in {%s}, L = %d",
						   i, tmp, ascii_len);

			return -1L;
		}

		// Ignore signs and any leading zeroes.
		if ((second_digit == NULL) && (tmp[i] > '0') && (tmp[i] <= '9'))
		{
			// tmp[i] is the first "real" digit of the number.
			second_digit = &tmp[i + 1];	// I.e. the address of the second digit.
		}
	}

	*p_buffer  +=  ascii_len ;    /*  promote buffer position  */

	// If the number is of more than one digit, set minus_first_digit_out equal to the number after
	// the first digit; otherwise set minus_first_digit_out equal to zero. If second_digit points
	// to the terminating NULL character of the string, the incoming number had only one digit.
	if ((second_digit != NULL) && (*second_digit != (char)0))
		minus_first_digit = atol (second_digit);	// Otherwise leave at default of zero.

	*minus_first_digit_out = minus_first_digit;

	return atol( tmp );
}
#endif



/* << -------------------------------------------------------------------- >> */
/*																              */
/*     get a double-prescision float value from buffer & promote buf-ptr	  */
/*												                              */
/* << -------------------------------------------------------------------- >> */
static double get_double(
		       char	**p_buffer,
		       int	ascii_len
		       )
{
	char	tmpbuf[30];
	char	*tmp = &tmpbuf[0];
	int		i;
	int		true_len		= ascii_len;
	int		found_decimal	= false;


	memcpy (tmp, *p_buffer, ascii_len);
	tmp[ascii_len] = 0;

	// Skip past leading spaces.
	while (*tmp == ' ')
	{
		tmp++;
		true_len--;
	}

	// verify token is numeric:
	for( i = 0  ; i < true_len ;  i++ )
	{
		if ((legal_digit (tmp, i) == false) && (tmp[i] != '.'))
		{
			glbErrorFlg = true;

			GerrLogReturn (GerrId,
						   "get_double: non-numeric character at pos %d in {%s}, L = %d",
						   i, tmp, ascii_len);

			return -1;
		}
		else
		{
			if (tmp[i] ==  '.')
			{
				if (found_decimal)
				{
					glbErrorFlg = true;

					GerrLogReturn (GerrId,
								   "Get_double: second decimal point at pos %d in {%s}, L = %d",
								   i, tmp, ascii_len);

					return -1;
				}
				else
				{
					found_decimal = true;
				}
			}
		}

	}

	*p_buffer  +=  ascii_len ;    /*  promote buffer position  */

	return atof (tmp);
}



/* << -------------------------------------------------------- >> */
/*                                                                */
/*		a legal digit ( allowing -/+ at init )		  */
/*                                                                */
/* << -------------------------------------------------------- >> */
static bool legal_digit(
			const char	*buf,
			int		pos
			)
{
  bool	retval = true;

  switch( buf[pos] )
    {
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
      break;

    case '+':
    case '-':
      if( pos == 0 )		/* exmp: "-1200", "+43" */
	{
	  break;
	}

      /* -- FALL THROUGH -- */

    default:
      retval = false;
    }

  return retval;
}



/* << -------------------------------------------------------- >> */
/*                                                                */
/*        get a string value from buffer & promote buf-ptr		  */
/*                                                                */
/* << -------------------------------------------------------- >> */
static void get_string (
						  char	** p_buffer,
						  char	*str,
						  int	ascii_len
						 )
{
  memcpy( str, *p_buffer , ascii_len ) ;
  str[ascii_len]  =  0 ;
  *p_buffer  +=  ascii_len ;
}


/* << -------------------------------------------------------- >>	*/
/*																	*/
/*        get a string value from buffer & promote buf-ptr			*/
/*		New version - returns the char * and has conversion option	*/
/*																	*/
/* << -------------------------------------------------------- >>	*/
static void get_str_h (
						  char	** p_buffer,
						  char	*str,
						  int	ascii_len,
						  short	WinHebFlag
					  )
{
  memcpy( str, *p_buffer , ascii_len ) ;
  str[ascii_len]  =  0 ;
  *p_buffer  +=  ascii_len ;

  if (WinHebFlag)
  {
    switch_to_win_heb ((unsigned char *)str);
  }
}



/*=======================================================================
||																		||
||            Inform shared memory tables change has occured.			||
||																		||
 =======================================================================*/
void UpdateSharedMem (TShmUpdate tbl_to_update)
{
	int		update_date;
	int		update_time;
//	char	table_name[32 + 1];

	bool        update_pl_macabi_flg = false;
	time_t      curr_time;
	struct tm   *tmp;
	int			retries;

	// Do not update table date if nothing updated
	if (tbl_to_update == NONE)
	{
		return;
	}

    /// Get current time + ten minutes
    time( &curr_time );

    curr_time += SHM_UPDATE_INTERVAL * 60;

    tmp = localtime( &curr_time );

    update_date = (tmp->tm_year + 1900) * 10000+
		  (tmp->tm_mon + 1 ) * 100 +
		   tmp->tm_mday;

    update_time = tmp->tm_hour * 10000 +
		  tmp->tm_min * 100+
		  tmp->tm_sec;


	// DonR 10Apr2008: Drug-list no longer uses virtual timestamps. In order not to miss
	// any updates because of this change, make sure that the tables_update timestamp can
	// be advanced, but not regressed.
    switch( tbl_to_update )
    {
	case DRUG_LIST:
	    strcpy( TableName, "Drug_list" );
	    break;

	case DRUG_LIST_DOCTOR: //Yulia 20030331
	    strcpy( TableName, "Drug_list_doctor" );
	    break;

	case PRICE_LIST:
	    update_pl_macabi_flg = true;       /* -> update PRICE_LIST_MACABI */
	    strcpy( TableName, "Price_list" );

	    /*   Use yarpa date & time instead of current:
	     */
	    update_date = glbPriceListUpdateArray[0];    /* v_date_yarpa */
	    update_time = glbPriceListUpdateArray[1];    /* v_time_yarpa */
	     break;

	case MEM_PRICE:
	    strcpy( TableName, "Member_price" );
	    break;

	case PHARMACY:
	    strcpy( TableName, "Pharmacy" );
	    break;

	case DOCTOR_PERCENTS:
	    strcpy( TableName, "Doctor_Percents" );
	    break;

	case DOCTOR_SPECIALITY:
	    strcpy( TableName, "Doctor_Speciality" );
	    break;

	case SPCLTY_LARGO_PRCNT:
	    strcpy( TableName, "Spclty_largo_prcnt" );
	    break;

	case DRUG_INTERACTION:
	    strcpy( TableName, "Drug_interaction" );
	    break;

	case DRUG_EXTENSION:
	    strcpy( TableName, "Drug_extension" );
	    break;

	case SUPPLIERS:
	    strcpy( TableName, "Suppliers" );
	    break;

	case MAC_CENTERS:
	    strcpy( TableName, "Macabi_centers" );
	    break;

	case ERR_MSG:
	    strcpy( TableName, "Pc_error_message" );
	    break;

	case GENCOMPONENTS:
		update_time = VirtualMorningTime;
		update_date = VirtualMorningDate;
	    strcpy( TableName, "GenComponents" );
	    break;

	case DRUGGENCOMPONENTS:
		update_time = VirtualMorningTime;
		update_date = VirtualMorningDate;
	    strcpy( TableName, "DrugGenComponents" );
	    break;

	case GENERYINTERACTION:
		update_time = VirtualMorningTime;
		update_date = VirtualMorningDate;
	    strcpy( TableName, "GeneryInteraction" );
	    break;

	case GENINTERNOTE:
		update_time = VirtualMorningTime;
		update_date = VirtualMorningDate;
	    strcpy( TableName, "GenInterNotes" );
	    break;

	case GNRLDRUGNOTES:
		update_time = VirtualMorningTime;
		update_date = VirtualMorningDate;
	    strcpy( TableName, "GnrlDrugNotes" );
	    break;

	case PHARMDRUGNOTES:
		update_time = VirtualMorningTime;
		update_date = VirtualMorningDate;
	    strcpy( TableName, "PharmDrugNotes" );
	    break;

	case DRUGNOTES:
		update_time = VirtualMorningTime;
		update_date = VirtualMorningDate;
	    strcpy( TableName, "DrugNotes" );
	    break;

	case DRUGDOCTORPROF:
		update_time = VirtualMorningTime;
		update_date = VirtualMorningDate;
	    strcpy( TableName, "DrugDoctorProf" );
	    break;

	case ECONOMYPRI:
	    strcpy( TableName, "EconomyPri" );
	    break;

    case PHARMACOLOGY:
		update_time = VirtualMorningTime;
		update_date = VirtualMorningDate;
	    strcpy( TableName, "Pharmacology" );
	    break;

    case PHARMACY_CONTRACT:
		update_time = VirtualMorningTime;
		update_date = VirtualMorningDate;
	    strcpy( TableName, "Pharmacy_Contract" );
	    break;

    case PRESC_SOURCE:
		update_time = VirtualMorningTime;
		update_date = VirtualMorningDate;
	    strcpy( TableName, "Prescr_Source" );
	    break;

    case CONFIRM_AUTHORITY:
		update_time = VirtualMorningTime;
		update_date = VirtualMorningDate;
	    strcpy( TableName, "Confirm_Authority" );
	    break;

    case CLICKS_DISCOUNT:
		update_time = VirtualMorningTime;
		update_date = VirtualMorningDate;
	    strcpy( TableName, "Clicks_Discount" );
	    break;

    case DRUG_FORMS:
	    strcpy( TableName, "Drug_Forms" );
	    break;

    case USAGE_INSTRUCTIONS:
		update_time = VirtualMorningTime;
		update_date = VirtualMorningDate;
	    strcpy( TableName, "Usage_Instruct" );
	    break;

    case TREATMENT_GROUP:
		update_time = VirtualMorningTime;
		update_date = VirtualMorningDate;
	    strcpy( TableName, "Treatment_Group" );
	    break;

    case PRESCR_PERIOD:
		update_time = VirtualMorningTime;
		update_date = VirtualMorningDate;
	    strcpy( TableName, "Presc_Period" );
	    break;

    case DRUG_EXTN_DOC:
		update_time = VirtualMorningTime;
		update_date = VirtualMorningDate;
	    strcpy( TableName, "Drug_Extn_Doc" );
	    break;

	case GADGETS:
	    strcpy( TableName, "Gadgets" );
	    break;

	case COUPONS:
	    strcpy( TableName, "Coupons" );
	    break;

	case CREDIT_REASONS:
	    strcpy( TableName, "CreditReasons" );
	    break;

	case SUBSIDY_MESSAGES:
	    strcpy( TableName, "SubsidyMessages" );
	    break;

	case TIKRA_TYPE:
	    strcpy( TableName, "TikraType" );
	    break;

	case HMO_MEMBERSHIP:
	    strcpy( TableName, "HMO_Membership" );
	    break;


	case NONE:
	    strcpy (TableName, "");	// DonR 06Apr2008: Added default.
	    return;

	default:
	    strcpy (TableName, "");	// DonR 06Apr2008: Added default.
//	    GerrLogMini (GerrId, "Unknown shm table num to update : <%d>", tbl_to_update);
	    break;
    }		// switch( table_num ..


    // DonR 06Apr2008: If there's no valid table-name to update, don't write to
	// the tables_update table.
	// DonR 10Apr2008: Make update date/time update conditional on the new set of values
	// being greater than the previous values.
	if (*TableName != (char)0)
	{
	
		//
		// Update shamred memory table date & time
		//
		for( retries = 0; retries < SQL_UPDATE_RETRIES; retries++)
		{
		/*
		 *  update table's row with current date & time:
		 */
		ExecSQL (	MAIN_DB,
					AS400SRV_UPD_tables_update,
					AS400SRV_UPD_tables_update_conditional,
					&update_date,		&update_time,
					&TableName,			&update_date,
					&update_date,		&update_time,
					END_OF_ARG_LIST									);
			
		if( SQLERR_code_cmp( SQLERR_access_conflict ) == MAC_TRUE )
		{
			sleep( 1 );
			continue;
		}

		/*
		 *  if row does not exist - insert one:
		 */
		if( SQLERR_code_cmp( SQLERR_no_rows_affected ) == MAC_TRUE )
		{
			ExecSQL (	MAIN_DB, AS400SRV_INS_tables_update,
						&TableName,		&update_date,
						&update_time,	END_OF_ARG_LIST			);

			if( SQLERR_code_cmp( SQLERR_access_conflict ) == MAC_TRUE )
			{
			sleep( 1 );
			continue;
			}
			else
			{
					if (!SQLERR_code_cmp (SQLERR_not_unique))
						SQLERR_error_test();	// DonR 10Apr2008: Duplicate key is OK here - 
												// it just means that the UPDATE didn't do
												// anything because the new time stamps were
												// not any greater than the existing ones.
			}

		}

		// SQLERR_error_test() of last action : insert or update.

		if( update_pl_macabi_flg == true  &&  SQLCODE != 0 )
		{
			strcpy( TableName, "Price_list_macabi" );

			/*   Use Macabi date & time as passed in msg 2031:
			 */
			update_date = glbPriceListUpdateArray[2];    /* v_date_macabi */
			update_time = glbPriceListUpdateArray[3];    /* v_time_macabi */

				//
				// Update shamred memory table date & time
			// of Price_list_macabi
				//
				for( retries = 0; retries < SQL_UPDATE_RETRIES; retries++)
				{
			   /*
			   *  update table's row with current date & time:
			   */
				ExecSQL (	MAIN_DB,
							AS400SRV_UPD_tables_update,
							AS400SRV_UPD_tables_update_conditional,
							&update_date,		&update_time,
							&TableName,			&update_date,
							&update_date,		&update_time,
							END_OF_ARG_LIST								);

			   if( SQLERR_code_cmp( SQLERR_access_conflict ) == MAC_TRUE )
			   {
				 sleep( 1 );
				 continue;
			   }

			   /*
				*  if row does not exist - insert one:
				*/
			   if( SQLERR_code_cmp( SQLERR_no_rows_affected ) == MAC_TRUE )
			   {
					ExecSQL (	MAIN_DB, AS400SRV_INS_tables_update,
								&TableName,		&update_date,
								&update_time,	END_OF_ARG_LIST			);

				 if( SQLERR_code_cmp( SQLERR_access_conflict ) == MAC_TRUE )
				 {
			   sleep( 1 );
			   continue;
				 }
				else
				{
					 if (!SQLERR_code_cmp (SQLERR_not_unique))
//					if (SQLCODE != -239)
						SQLERR_error_test();	// DonR 10Apr2008: Duplicate key is OK here - 
												// it just means that the UPDATE didn't do
												// anything because the new time stamps were
												// not any greater than the existing ones.
				}
				   }
			   
			   break;

				} // Loop for update Price_list_macabi

		 } // end of if update_pl_macabi_flag

		break;

		}		// for ( retries ..

	}	// Table_name has a real value.

}


/*=======================================================================
||																		||
||            			pipe_handler()									||
||																		||
 =======================================================================*/
void	pipe_handler( int signo )
{
    GerrLogReturn(
		  GerrId,
		  "Closed pipe - hung up by client"
		  );

    sigaction( SIGPIPE, &sig_act_pipe, NULL );
    
    caught_signal = 0;
    
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
	
	GerrLogReturn (GerrId,
				   "As400UnixServer received Signal %d (%s). Terminating process.",
				   signo,
				   sig_description);
}


void ReadIniFilePort ( void )  /*20020828*/

{
	int		PortNumber	= 0;
	char	*inifile	= "/pharm/bin/inifileport";
	char	*separators	= "= \t\n";
	char	*token1;
	char	*token2;
	char	inbuf [512];
	char	*bufptr;
	FILE	*fp;


	fp = fopen (inifile, "r");

	if (fp == NULL)
	{
		GerrLogMini (GerrId,"ReadIni: Can't open ini file %s for port number, etc.\n ", inifile); 
		exit (0);
	}

	
	while (!feof (fp))
	{
		fgets (inbuf, 510, fp);
//		GerrLogMini (GerrId, "ReadIni: read line {%s}, eof now = %d.", inbuf, feof(fp));
		if (feof (fp))
			break;
		
		// For first call to strtok(), use the address of the buffer. Subsequent calls use NULL.
		bufptr = inbuf;

		token1 = strtok (bufptr, separators);
		bufptr = NULL;

		while (token1 != NULL)
		{
			// Just to be thorough, make sure IDENTIFIER is all-caps.
			AllCaps (token1);

//			GerrLogMini (GerrId,"ReadIni: Got token1 {%s}",token1);

			// For backward compatibility, a numeric first token is the port number.
			// Anything else will be in the form of IDENTIFIER=VALUE, except that the "=" could
			// equally be a space or a tab.
			if ((*token1 >= '0') && (*token1 <= '9'))
			{
				PortNumber = atoi (token1);
				GerrLogMini (GerrId,"ReadIni: Port (old style) = %d",PortNumber);

				// Jump to next iteration of the outer file-reading loop.
				break;
			}


			// Deal with "new-style" tokens.
			token2 = strtok (bufptr, separators);
			AllCaps (token2);
//			GerrLogMini (GerrId,"ReadIni: Got token2 {%s}",token2);


			if (!strcmp (token1, "REAL_TIME"))
			{
				// REAL_TIME with no value defaults to TRUE.
				if (token2 == NULL)
				{
					RealTimeMode = true;
				}
				else
				{

					if (!strcmp (token2, "TRUE"))
					{
						RealTimeMode = true;
					}
					else
					{
						if (!strcmp (token2, "FALSE"))
						{
							RealTimeMode = false;
						}
						else
						{
							if ((*token2 >= '0') && (*token2 <= '9'))
							{
								RealTimeMode = (atoi (token2) > 0) ? true : false;
							}
						}
					}	// Token2 <> "TRUE"
				}	// Token2 not NULL

				break;	// Read next line from file.
			}	// Token1 == REAL_TIME


			if (!strcmp (token1, "PORT"))
			{
				PortNumber = atoi (token2);
				GerrLogMini (GerrId,"ReadIni: Port (new style) = %d",PortNumber);
				break;	// Read next line from file.
			}	// Token1 == PORT


			// Don't forget to get the next identifier token!
			GerrLogMini (GerrId, "ReadIni: Couldn't interpret first token - reading next line from file.");
			break;

		}	// While Token1 not NULL
	}	// Not end-of-file



	if( PortNumber == 0) 
	{
		GerrLogMini (GerrId, "ReadIni: Didn't get a valid port number from inifileport!");
		printf("\nReadIni: Didn't get a valid port number from inifileport!\n");fflush(stdout);
	}
	else
	{
		AS400_UNIX_PORT = PortNumber;
		printf ("\nReadIni: port number  =  %d.\n",PortNumber); fflush(stdout); 
	}

	fclose (fp);

	GerrLogMini (GerrId,"ReadIni: RealTimeMode = %d, Port = %d.",
				 RealTimeMode, AS400_UNIX_PORT);
}



// Generate Date-Time-Sequence Number timestamp.
// DonR 27Jan2003: Slight change to the algorithm. Now instead of starting at zero and
// counting by ones, we start at two and count by twos. This way, the artificial
// deletion record sent before economypri insertions/updates will be sent for the same
// time, but with the sequence number one less. In the case where the AS400 sends us
// two successive changes to the same position in the same group (which they aren't
// supposed to do, but that's another story), we will now send the sequence
// delete-insert-delete-insert instead of delete-delete-insert-insert, which blows
// up the CLICKS system.

int GenerateArrivalTimestamp (long *dt, long *tm, long *sq)

{
	static int	LastDate	= 0;
	static int	LastTime	= 0;
	static int	SeqNum		= 2;

	int			CurrDate;
	int			CurrTime;
	int			RetVal;


	CurrDate = GetDate ();
	CurrTime = GetTime ();

	if ((CurrDate == LastDate) && (CurrTime == LastTime))
	{
		SeqNum += 2;
	}
	else
	{
		LastDate = CurrDate;
		LastTime = CurrTime;
		SeqNum= 2;
	}


	if ((dt == NULL) || (tm == NULL) || (sq == NULL))
	{
		RetVal = -1;
	}
	else
	{
		*dt = LastDate;
		*tm = LastTime;
		*sq = SeqNum;

		RetVal = 0;
	}

	return (RetVal);
}



void AllCaps (char *buf)

{
	char	c;
	int		i;
	int		L;


	L = strlen (buf);

	for (i = 0; i < L; i++)
	{
		c = toupper (buf [i]);
		buf [i] = c;
	}
}



int GetVirtualTimestamp (int *virtual_date_out, int *virtual_time_out)
{
	static int	initialized_date	= 0;
	static int	running_count		= 0;
	int			SysDate;
	int			SysTime;

	int		count_drg,
			count_gen,
			count_c2g,
			count_inx,
			count_int,
			count_txm,
			count_alm,
			count_spc,
			count_pri,
			count_phm,
			count_cld,
			count_usi,
			count_drf,
			count_trg,
			count_prp,
			count_ded;
	int		v_VirtualTime;
	int		v_VirtualDate;


	SysDate = GetDate();
	SysTime = GetTime();

	// If we're in real-time mode, just give the calling function the current date and
	// time, then exit.
	if (RealTimeMode)
	{
		if (virtual_date_out != NULL)
			*virtual_date_out = SysDate;

		if (virtual_time_out != NULL)
			*virtual_time_out = SysTime;

		return (MAC_OK);
	}


	// Check which virtual date we're going to use. If it's not the same as the last one,
	// (re-) initialize the function. For this purpose, assume that 5:00 AM is the beginning
	// of "today".
	if (SysTime >= 50000)
		v_VirtualDate = IncrementDate (SysDate, 1);
	else
		v_VirtualDate = SysDate;


	// If we haven't already initialized for this virtual date, we need to do so.
	if (initialized_date	!= v_VirtualDate)
	{
		initialized_date	=  v_VirtualDate;

		count_drg = 0;
		count_gen = 0;
		count_c2g = 0;
		count_inx = 0;
		count_int = 0;
		count_txm = 0;
		count_alm = 0;
		count_spc = 0;
		count_pri = 0;
		count_phm = 0;
		count_cld = 0;
		count_usi = 0;
		count_drf = 0;
		count_trg = 0;
		count_prp = 0;
		count_ded = 0;


		// For Drug List, we use the doctor's update date. Other tables have only one
		// update date.
		ExecSQL (	MAIN_DB, AS400SRV_READ_todays_update_size_drug_list,
					&count_drg, &v_VirtualDate, END_OF_ARG_LIST					);

		ExecSQL (	MAIN_DB, AS400SRV_READ_todays_update_size_gencomponents,
					&count_gen, &v_VirtualDate, END_OF_ARG_LIST					);

		ExecSQL (	MAIN_DB, AS400SRV_READ_todays_update_size_druggencomponents,
					&count_c2g, &v_VirtualDate, END_OF_ARG_LIST					);

		ExecSQL (	MAIN_DB, AS400SRV_READ_todays_update_size_generyinteraction,
					&count_inx, &v_VirtualDate, END_OF_ARG_LIST					);

		ExecSQL (	MAIN_DB, AS400SRV_READ_todays_update_size_geninternotes,
					&count_int, &v_VirtualDate, END_OF_ARG_LIST					);

		ExecSQL (	MAIN_DB, AS400SRV_READ_todays_update_size_gnrldrugnotes,
					&count_txm, &v_VirtualDate, END_OF_ARG_LIST					);

		ExecSQL (	MAIN_DB, AS400SRV_READ_todays_update_size_drugnotes,
					&count_alm, &v_VirtualDate, END_OF_ARG_LIST					);

		ExecSQL (	MAIN_DB, AS400SRV_READ_todays_update_size_drugdoctorprof,
					&count_spc, &v_VirtualDate, END_OF_ARG_LIST					);

		ExecSQL (	MAIN_DB, AS400SRV_READ_todays_update_size_economypri,
					&count_pri, &v_VirtualDate, END_OF_ARG_LIST					);

		ExecSQL (	MAIN_DB, AS400SRV_READ_todays_update_size_pharmacology,
					&count_phm, &v_VirtualDate, END_OF_ARG_LIST					);

		ExecSQL (	MAIN_DB, AS400SRV_READ_todays_update_size_clicks_discount,
					&count_cld, &v_VirtualDate, END_OF_ARG_LIST					);

		ExecSQL (	MAIN_DB, AS400SRV_READ_todays_update_size_usage_instruct,
					&count_usi, &v_VirtualDate, END_OF_ARG_LIST					);

		ExecSQL (	MAIN_DB, AS400SRV_READ_todays_update_size_drug_forms,
					&count_drf, &v_VirtualDate, END_OF_ARG_LIST					);

		ExecSQL (	MAIN_DB, AS400SRV_READ_todays_update_size_treatment_group,
					&count_trg, &v_VirtualDate, END_OF_ARG_LIST					);

		ExecSQL (	MAIN_DB, AS400SRV_READ_todays_update_size_presc_period,
					&count_prp, &v_VirtualDate, END_OF_ARG_LIST					);

		ExecSQL (	MAIN_DB, AS400SRV_READ_todays_update_size_drug_extn_doc,
					&count_ded, &v_VirtualDate, END_OF_ARG_LIST					);


		running_count =	  count_drg
						+ count_gen
						+ count_c2g
						+ count_inx
						+ count_int
						+ count_txm
						+ count_alm
						+ count_spc
						+ count_pri
						+ count_phm
						+ count_cld
						+ count_usi
						+ count_drf
						+ count_trg
						+ count_prp
						+ count_ded;
	}

	// If the virtual date hasn't changed, just increment the running total of updates
	// for the current virtual date.
	else
	{
		running_count++;
	}


	// Calculate virtual time.
	// DonR 18Nov2025 BUG FIX #461285: Rather than just add an arbitrary number to 50000,
	// use the IncrementTime() function so we get a valid time value (rather than things
	// like "50189"). SuperPharm was choking on "bad seconds"!
//	v_VirtualTime = VIRTUAL_MORNING_START + (running_count / VIRTUAL_PORTION);
	v_VirtualTime = IncrementTime (VIRTUAL_MORNING_START, (running_count / VIRTUAL_PORTION), NULL);

	// Copy virtual date/time into global variables.
	VirtualMorningDate = v_VirtualDate;
	VirtualMorningTime = v_VirtualTime;

	// Copy virtual date/time into return variables.
	if (virtual_date_out != NULL)
		*virtual_date_out = v_VirtualDate;

	if (virtual_time_out != NULL)
		*virtual_time_out = v_VirtualTime;

	return (MAC_OK);
}



static int subst_newlines(char *buffer, int LineLen)

{
	int		i;
	int		L;
	int		FoundSpace;
	int		Start		= 0;


	L = strlen (buffer);

	while ((L - Start) > LineLen)
	{
		FoundSpace = 0;

		for (i = (Start + LineLen - 1); i > Start; i--)
		{
			if (buffer[i] == ' ')
			{
				buffer[i] = '\n';
				Start = i;
				FoundSpace = 1;
				break;
			}
		}

		// Avoid infinite loop if no spaces are found.
		if (FoundSpace == 0)
			break;
	}
}


// DonR 26Oct2008: Store in-health-basket value received from AS400 in a new variable,
// and translate the version stored in the old variable into the old 0/1 format.
// HealthBasket_in_out initially has the value received from AS400; its value is copied
// to the return variable, and then it is translated into an "old style" 1 or 0.
// NOTE: The one thing this function does *not* do is correct unrecognized values of
// the AS400-supplied health-basket flag; the new health-basket variable (the return
// value of the function) will retain whatever value the AS400 gave it.
short	ProcessHealthBasket	(short *HealthBasket_in_out)
{
	short	RetValue	= *HealthBasket_in_out;

	switch (RetValue)
	{
		case HEALTH_BASKET_NO:
		case HEALTH_BASKET_YES:		break;	// No need for translation.


		case HEALTH_BASKET_KE_ILU:
		case HEALTH_BASKET_NEW_YES:	*HealthBasket_in_out = HEALTH_BASKET_YES;
									break;	// Translate new "versions of yes" to old version.

		default:					*HealthBasket_in_out = HEALTH_BASKET_NO;
									break;	// Translate any unrecognized value to "no".
	}

	return (RetValue);
}
