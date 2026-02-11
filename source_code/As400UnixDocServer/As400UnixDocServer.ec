/* << ------------------------------------------------------ >> */
/*  		        As400UnixDocServer.ec                       */
/*  ----------------------------------------------------------- */
/*  Date: 		7/1998			 								*/
/*  Written by:  	Revital & Yulia          	  				*/
/*  ----------------------------------------------------------- */
/*  Purpose :													*/
/*       Source file for client-process in AS400 <-> Unix   	*/
/*       communication system.       							*/
/* << ------------------------------------------------------ >> */
/*===============================================================
||				defines											||
 ===============================================================*/
#define MAIN
#define DOCTORS_SRC
char *GerrSource = __FILE__;
/*debug_level */
//#define AS400_DEBUG
#define AS400_DEBUG_3007
# define CONTRACTOR_MOKED 999985
/*===============================================================
||				include files			||
 ===============================================================*/
EXEC SQL include sqlca;
EXEC SQL include "DB.h"; 
EXEC SQL include "GenSql.h";
#include <MacODBC.h>
EXEC SQL include "MsgHndlr.h";
EXEC SQL include "As400UnixServer.h";
EXEC SQL include "DBFields.h";
EXEC SQL include "multix.h";
EXEC SQL include "global_1.h";

//Y20060720 next 6 lines for erase hebrew from program
// DonR 13Sep2012: Added two new text fields for hospital discharge description.
static FLAG_SELECT =0;  
EXEC SQL BEGIN DECLARE SECTION;
	char v_typeres_dos[25];
	char v_resname_doc[30];
	char v_resname_win[30];
	char v_relname_doc[30];		// DonR 13Sep2012.
	char v_relname_win[30];		// DonR 13Sep2012.
	char v_termdesc_win[4];
EXEC SQL END DECLARE SECTION;

void	EmbeddedInformixConnect	(char	*username, char	*password, char	*dbname);
short	EmbeddedInformixConnected = 0;

static void strnrev( char * str , int n );//Yulia 20040708
#define isneutral( c ) ( strchr( " -\"'.,/\\_=+|!@#$%^&*()`:", (unsigned char)(c) ) != NULL )
void DosWinHebConv( char *str, int n );
void DosWinHebConvMultiLine( char * str, int n );
#define ishebrew( c )									\
  (														\
  ( ( unsigned char )( c ) >= 128 && ( unsigned char )( c ) <= 154 ) ||	\
  ( ( unsigned char )( c ) >= 224 && ( unsigned char )( c ) <= 250 ) )



//#include <sys/netinet/tcp.h>  Y20060724
#include <netinet/tcp.h>
struct sigaction	sig_act;
struct sigaction	sig_act_terminate;

void	pipe_handler( int signo );
void	TerminateHandler	(int signo);

void   switch_to_win_heb( unsigned char *source );
  
short UrgentFlag = 0;      // GAL 23/10/2001
//20020806 Yulia for DbGetLabTerm
int	    prev_contractor = 0;
int	    prev_idnumber	= 0;
int	    prev_idcode		= 0;
int	    prev_termid_upd = 0;
int		caught_signal	= 0;

int		TikrotProductionMode	= 1;	// To avoid "unresolved external" compilation error.

//static int r3009_counter = 0;
//static int r3009_uncommitted = 0;

/*===============================================================
||								||
||				main				||
||								||
 ===============================================================*/
int main(
	 int	argc,
	 char	*argv[]
	 )
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
    struct 	timeval		timeout;
    TConnection	*cp;
    int		sock_max;
    int		i;
    int		print_flg;
    int		first_flg = 1;
	sigset_t	NullSigset;


/*===============================================================
||								||
||			initializations				||
||								||
 ===============================================================*/
    GetCommonPars( argc, argv, &retrys );

    RETURN_ON_ERR(
		  InitSonProcess(
				 argv[0],
				 DOCAS400TOUNIX_TYPE,
				 retrys,
				 0,
				 DOCTOR_SYS
				 )
		   );


 	caught_signal = 0;
	memset ((char *)&NullSigset, 0, sizeof(sigset_t));

    sig_act.sa_handler = pipe_handler;
    sig_act.sa_mask = NullSigset;  // DonR 12Nov2007 - Unix/Linux compatibility.
    sig_act.sa_flags = 0;

   
    sig_act_terminate.sa_handler	= TerminateHandler;
    sig_act_terminate.sa_mask		= NullSigset;
    sig_act_terminate.sa_flags		= 0;

    sigaction( SIGPIPE, &sig_act, NULL );
  	sigaction( SIGTERM, &sig_act_terminate, NULL);


	// DonR 13Feb2020: Add signal handler for segmentation faults - used
	// by ODBC routines to detect invalid pointer parameters.
    sig_act_ODBC_SegFault.sa_handler	= ODBC_SegmentationFaultCatcher;
    sig_act_ODBC_SegFault.sa_mask		= NullSigset;
    sig_act_ODBC_SegFault.sa_flags		= 0;

	if (sigaction (SIGSEGV, &sig_act_ODBC_SegFault, NULL) != 0)
	{
		GerrLogReturn (GerrId,
					   "Can't install signal handler for SIGSEGV" GerrErr,
					   GerrStr);
	}


    flg_sys_busy = false;


	// Connect to database.
	do
	{
		SQLMD_connect ();
		if (!MAIN_DB->Connected)
		{
			sleep (10);
			GerrLogMini (GerrId, "\n\n\nRetrying %s ODBC connect.", MAIN_DB->Name);
		}
	}
	while (!MAIN_DB->Connected);

	// DonR 10Sep2020: SQLMD_connect (which resolves to INF_CONNECT) is now ODBC-only;
	// so call a local routine to connect to embedded-SQL Informix.
	EmbeddedInformixConnect (MacUser, MacPass, MacDb);


/*===============================================================
||			Initialize connection array		||
 ===============================================================*/

    initialize_connection_array();

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

    while( stop_on_err == false &&
	   flg_sys_down == false )
    {

/*===============================================================
||			Handle client connections		||
 ===============================================================*/

	FD_ZERO( & called_sockets );

	{
	    time_t curr_time = time(NULL);

	    first_flg = ! print_flg;
	    print_flg = (! (curr_time % 3600) ) ;
	}
	//
	// Loop over connections
	//
	if( print_flg && first_flg )
	{
	    puts("Connection map for as400unixdocserver :");
	    puts("---------------------------------------");
	    puts("---------------------------------------");
	}

	for( sock_max = i = 0  ;  i < MAX_NUM_OF_CLIENTS  ;  i++ )
	{
	    cp = glbConnectionsArray + i;
	    //
	    // Check only active connections
	    //
	    if( cp->server_state == 0 )
	    {
		break;
	    }

	    //
	    // Close connections in "low-water-mark"
	    //
	    if( time( NULL ) - cp->last_access > 1200 )
	    {
		terminate_a_connection( cp,
				       cp->in_tran_flag == 1 ?
				       WITH_ROLLBACK : WITHOUT_ROLLBACK);
		continue;
	    }

	    if( print_flg && first_flg )
	    {
		printf("Connection %d : sock <%d> db <%s> state <%d>"
			" in_tran <%d>\n",
			i,
			cp->socket,
			cp->db_connection_id,
			cp->server_state,
			cp->in_tran_flag ); fflush(stdout);
	    }

	    //
	    // Add connection socket to bit mask
	    //
	    FD_SET( cp->socket, & called_sockets );

	    sock_max = sock_max > cp->socket ? sock_max : cp->socket;
	}

	//
	// Add listen socket to bit mask
	//
	FD_SET( glbListenSocket, & called_sockets );

	sock_max = sock_max > glbListenSocket ? sock_max : glbListenSocket;

	//
	// set timeout for select
	//
	timeout.tv_sec = SelectWait / 1000000;
	timeout.tv_usec = SelectWait % 1000000;

	rc = select(
		    sock_max + 1,	/* no. of bits to scan		*/
		    &called_sockets,	/* sockets called for recv ()	*/
		    NULL,
		    NULL,
		    &timeout		/* time to wait			*/
		    );

	switch( rc )
	{
	    case -1:				// SELECT ERROR

		GerrLogReturn(
			      GerrId,
			      "Select() error\n"
			      GerrErr,
			      GerrStr
			      );
		stop_on_err = true;
		continue;

	    default:				// CONNECTION REQUESTS

		//
		// Update last access time for process
		//////////////////////////////////////
		//
		state = UpdateLastAccessTime( );

		if( ERR_STATE( state ) )
		{
		    GerrLogReturn(
				  GerrId,
				  "Error while updating last"
				  " access time of process"
				  );
		}

		if( FD_ISSET( glbListenSocket, &called_sockets ) )
		{
		    //
		    // Create new connection
		    ////////////////////////
		    //
		    if( create_a_new_connection() == failure )
		    {

			GerrLogReturn(
				      GerrId,
				      "Unable to create new client-connection."
				      "Error ( %d ) %s.",
				      GerrStr
				      );
		    }

		    continue;

                }

		//
		// Handle all client requests
		/////////////////////////////
		//

		answer_to_all_client_requests( & called_sockets );

		/* -- FALL THROUGH -- */

	    case 0:				// TIMEOUT

		RETURN_ON_ERR(
			      get_sysem_status(
					       &flg_sys_down,
					       &flg_sys_busy
					       )
			      );
		break;

	}	// switch( rc ..

    }		// while( stop_on_err ...

/*=======================================================================
||    			termination of process                    	||
 =======================================================================*/

    ExitSonProcess( MAC_SERV_SHUT_DOWN );

}

/* << -------------------------------------------------------- >> */
/*                                                                */
/*   		  Get length of a given message-structure         */
/*                                                                */
/* << -------------------------------------------------------- >> */
static int get_struct_len(
			  TSystemMsg msg
			  )
{
    int	  i;

    for( i = 0; NOT_END_OF_TABLE( i );  i++ )
    {
	if( msg == MessageSequence[i].ID )
	{
	    return( MessageSequence[i].len );
	}
    }

    GerrLogReturn( GerrId,
		   "message %d not found in messages table...\n",
		   msg );
    return -1;

}

/* << -------------------------------------------------------- >> */
/*                                                                */
/*   	   Does a socket connection to client still exists        */
/*                                                                */
/* << -------------------------------------------------------- >> */
int pipe_is_broken( TSocket  sock, int milisec )
{
    struct timeval  wt;
  //  struct fd_set   rw;  /* Ready to Write */  Y20060724
    fd_set   rw;  /* Ready to Write */
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
static TSuccessStat inform_client(
				  TConnection  	*connect,
				  TSystemMsg   	msg,
				  char			text_buffer[]		// DonR 06Feb2020: Added for compatibility with As400UnixServer.h
				  )
{
    char	to_send[64] ;
    char	*buf_ptr;
    int		len ;
    bool	failure_flg;
    int		rc ;
    fd_set	write_fds;
    struct timeval timeout;

/*===============================================================
||			Get data to send			||
 ===============================================================*/
    sprintf( to_send, "%d", msg ) ;

    len = strlen( to_send ) ;

    ascii_to_ebcdic_dic( to_send, len );

/*===============================================================
||			Check if data can be sent		||
 ===============================================================*/

    if( pipe_is_broken( connect->socket, 1000 ) )
    {
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

    while( len && failure_flg == false )
    {
	rc = send( connect->socket, buf_ptr, len, 0 ) ;

	switch( rc )
	{
	    case -1:
		GerrLogReturn(
			      GerrId,
			      "Error at send()\n"
			      GerrErr,
			      GerrStr
			      );
		failure_flg = true;
		break;

	    case 0:
		GerrLogReturn(
			      GerrId,
			      "(send) Connection closed by client"
			      "Error ( %d ) %s.",
			      GerrStr
			      );
		failure_flg = true;
		break;

	    default:
		len -= rc;
		buf_ptr += rc;
	}

    } 		// while()

    return( failure_flg == false ?  success : failure ) ;

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
||								||
||			terminate_a_connection()		||
||								||
 ===============================================================*/
static void terminate_a_connection(
				   TConnection	*c,
				   int		perform_rollback
				   )
{
    if( perform_rollback )
    {
	DO_ROLLBACK( c ) ;
    }

//    SQLMD_disconnect_id( c->db_connection_id ) ;   /* disconnect from DB     */

    close_socket( c->socket ) ;

    printf("Closing a connection to as400 :\n"
	   "-------------------------------\n"
	   "Entry : %d"
	   "Socket : %d\n",
	   c - glbConnectionsArray,
	   c->socket
	   );
	   fflush(stdout);

    initialize_struct_to_not_connected( c ) ;      /* reset as not connected */

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
//20130103
//GerrLogMini ( GerrId,"before select");    
	rc = select( connect->socket + 1,  &ready_for_recv, 0, 0, &w_time );
//GerrLogMini ( GerrId,"after select");    
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
//GerrLogMini ( GerrId,"before recv [%d][%s]",msg_len,buf);    
	received_bytes = recv(
			      connect->socket,
			      buf,
			      msg_len,
			      0
			      ) ;
//GerrLogMini ( GerrId,"after recv");    
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
//GerrLogMini ( GerrId,"after recv1 default buf[%s][%d]",buf,msg_len);    
		buf	  += received_bytes;
		msg_len   -= received_bytes;
//GerrLogMini ( GerrId,"after recv2 default buf[%s][%d]",buf,msg_len);    
		break;
	}		// switch ( recived bytes
//GerrLogMini ( GerrId,"after recv3 default buf[%s][%d]",buf,msg_len);   
    }			// while( message to come
//GerrLogMini ( GerrId,"after recv4 default buf[%s][%d]",buf,msg_len);   
    return success;

}

/* << -------------------------------------------------------- >> */
/*                                                                */
/*  		   Connection is not currently active             */
/*                                                                */
/* << -------------------------------------------------------- >> */
static bool connection_is_not_in_transaction(
					     TConnection *  a_client
					     )
{
    return ! a_client->in_tran_flag;
}

/*===============================================================
||								||
||			get_message_ID()			||
||								||
 ===============================================================*/
static TSystemMsg get_message_ID(
				 TConnection *  c
				 )
{
    TSystemMsg	numericID ;
    memset( glbMsgBuffer, 0, sizeof(glbMsgBuffer));

    if( receive_from_socket( c, glbMsgBuffer, MSG_HEADER_LEN ) == failure )
    {
	return ERROR_ON_RECEIVE;
    }

    glbMsgBuffer[MSG_HEADER_LEN] = 0;
//20130103
//GerrLogMini ( GerrId,"numericID[%d] glbMsgBuffer:<%s>",atoi( glbMsgBuffer ),glbMsgBuffer);    

    numericID = atoi( glbMsgBuffer );

    if( ! LEGAL_MESSAGE_ID( numericID ) )
    {
	return BAD_DATA_RECEIVED;
    }

    return( numericID ) ;

}

/* << -------------------------------------------------------- >> */
/*                                                                */
/*		redefine an active connection as inactive         */
/*                                                                */
/* << -------------------------------------------------------- >> */
static void set_connection_to_not_in_transaction(
						 TConnection *  c
						 )
{
    c->in_tran_flag   = 0 ;
}

/* << -------------------------------------------------------- >> */
/*                                                                */
/*		redefine an inactive connection as active         */
/*                                                                */
/* << -------------------------------------------------------- >> */
static void set_connection_to_in_transaction_state(
						   TConnection *   client
						   )
{
    client->in_tran_flag = 1 ;

	// DonR 26Jan2020: ODBC opens transactions automatically at the first DB
	// update; there is no such thing in ODBC as an explicit transaction-open.
	// (But until everything is working in ODBC, we still need this!)
    SQLMD_begin();
if (SQLCODE) GerrLogMini (GerrId, "Ignore the preceding error!");
}

/* << -------------------------------------------------------- >> */
/*                                                                */
/*    get an unused connect-structure from glbConnectionsArray    */
/*                                                                */
/* << -------------------------------------------------------- >> */
int	 find_free_connection(
			      void
			      )
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
				sleep (10);
				GerrLogMini (GerrId, "\n\n\nRetrying %s ODBC connect.", MAIN_DB->Name);
			}
		}
		while (!MAIN_DB->Connected);

//   rc = SQLMD_connect_id( str_pos ) ;
//
//   if( ERR_STATE( rc ) )
//    {
//	return failure;
//    }

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
//	SQLMD_disconnect_id( str_pos ) ;
//
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
//       setsockopt( l_socket, SOL_SOCKET, SO_REUSEPORT,  Y20060724
//		   &nonzero, sizeof( int ) ) == -1	||            Y20060724 
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

/* << -------------------------------------------------------- >> */
/*                                                                */
/*	  	 Set active connection & rollback                 */
/*                                                                */
/* << -------------------------------------------------------- >> */
static TSuccessStat DO_ROLLBACK(
				TConnection  *c
				)
{
    TSuccessStat	retval = success;

//    SQLMD_set_connection( c->db_connection_id );

	RollbackAllWork ();

	if (SQLCODE == 0)
//    if( SQLMD_rollback() == MAC_OK )
    {
		set_connection_to_not_in_transaction( c );
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
static int commit_handler(
			   TConnection *  p_connect,
			   TShmUpdate    shm_table_to_update
			 )
{
    int			save_flg = p_connect->sql_err_flg;



    p_connect->sql_err_flg = 0;
    //
    // Update date of shared memory table if needed
    //

//	if (r3009_uncommitted > 0)
//	GerrLogFnameMini ("3009_log",
//					   GerrId,
//					   "About to commit %d 3009-related DB updates.",
//					   (int)r3009_uncommitted);

    UpdateSharedMem( shm_table_to_update );

//	if (r3009_uncommitted > 0)
//	GerrLogFnameMini ("3009_log",
//					   GerrId,
//					   "About to commit 3009-related DB updates - returned from UpdateSharedMem().");

    //
    // pipe is broken -- rollback & close connection
    //
    if( pipe_is_broken( p_connect->socket, 100 ) )
    {
	  puts("the pipe to AS4000 is broken...");
	  puts("-------------------------------");
	  fflush(stdout);
	  terminate_a_connection( p_connect,
				  p_connect->in_tran_flag ?
				  WITH_ROLLBACK : WITHOUT_ROLLBACK );
    }
    else
    //
    // Sql error occured -- rollback & inform client
    //
    if( save_flg )
    {
#ifdef AS400_DEBUG
 printf("\n save_Flg");
 fflush(stdout);
#endif // AS400_DEBUG
	DO_ROLLBACK( p_connect );

//	if (r3009_uncommitted > 0)
//	{
//		GerrLogFnameMini ("3009_log",
//						   GerrId,
//						   "Rolled back 3009-related DB updates - SQLCODE = %d.",
//						   (int)SQLCODE);
//		r3009_uncommitted = 0;
//	}

	if( inform_client( p_connect, CM_SQL_ERROR_OCCURED, "" ) == failure )
	{
	    terminate_a_connection( p_connect, WITHOUT_ROLLBACK );
	}
    }
    else
    //
    // Commit the transaction
    //
    {
//	SQLMD_set_connection( p_connect->db_connection_id );

#ifdef AS400_DEBUG
 printf("\n commit____");
 fflush(stdout);
#endif // AS400_DEBUG

			CommitAllWork ();

			if (SQLCODE  == 0)
//			if (SQLMD_commit () == MAC_OK)
	{

//	if (r3009_uncommitted > 0)
//	{
//		GerrLogFnameMini ("3009_log",
//						   GerrId,
//						   "Committed 3009-related DB updates - SQLCODE = %d.",
//						   (int)SQLCODE);
//		r3009_uncommitted = 0;
//	}

	    set_connection_to_not_in_transaction( p_connect );

	    if( inform_client( p_connect, CM_CONFIRMING_COMMIT, "" ) == failure )
	    {
		terminate_a_connection( p_connect, WITHOUT_ROLLBACK );
	    }
	}
	else
	{
	    terminate_a_connection( p_connect, WITH_ROLLBACK );
	}
    }

    return	MSG_HANDLER_SUCCEEDED ;

}

/* << -------------------------------------------------------- >> */
/*                                                                */
/*	 	  handler function executing "rollback"           */
/*                                                                */
/* << -------------------------------------------------------- >> */
static int rollback_handler(
			    TConnection *  p_connect
			    )
{
    TSuccessStat	rc ;

    p_connect->sql_err_flg = 0;

    rc = DO_ROLLBACK( p_connect );

    if( rc == success )
    {
	if( inform_client( p_connect, CM_CONFIRMING_ROLLBACK, "" ) == failure )
	{
	    terminate_a_connection( p_connect, WITH_ROLLBACK );
	}
    }
    else
    {
	terminate_a_connection( p_connect, WITH_ROLLBACK );
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

// handler for message 3003 - ins/update to table TEMPMEMB

static int message_3003_handler( TConnection	*p_connect,
				int		data_len,
				bool		do_insert)
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
EXEC SQL BEGIN DECLARE SECTION;
TTempMembRecord TempMembRec;
long int sysdate;
long int systime;
EXEC SQL END DECLARE SECTION;

  sysdate = GetDate();
  systime = GetTime();


/* << -------------------------------------------------------- >> */
/*			read input buffer			  */
/* << -------------------------------------------------------- >> */
  INIT_ERROR_DETECTION_MECHANISM;
  PosInBuff = glbMsgBuffer;

#ifdef AS400_DEBUG
 printf("\n PosInBuff>%s<",PosInBuff);
 fflush(stdout);
#endif // AS400_DEBUG

  TempMembRec.CardId			=  get_long( &PosInBuff,
				     		     IdNumber_9_len);
#ifdef AS400_DEBUG
// printf("\n TempMembRec.CardId:%d",TempMembRec.CardId);
// fflush(stdout);
#endif // AS400_DEBUG
  RETURN_ON_ERROR;

  TempMembRec.MemberId.Code		=  get_short( &PosInBuff,
				    		      1);
#ifdef AS400_DEBUG
// printf("\n TempMembRec.MemberId.Code:%d",TempMembRec.MemberId.Code);
// fflush(stdout);
#endif // AS400_DEBUG
  RETURN_ON_ERROR;

  TempMembRec.MemberId.Number		=  get_long( &PosInBuff,
				     		     IdNumber_9_len);
#ifdef AS400_DEBUG
// printf("\n TempMembRec.MemberId.Number:%d",TempMembRec.MemberId.Number);
// fflush(stdout);
#endif // AS400_DEBUG
  RETURN_ON_ERROR;
  TempMembRec.ValidUntil		=  get_long( &PosInBuff,
				     	    DateYYYYMMDD_len);
#ifdef AS400_DEBUG
// printf("\n TempMembRec.ValidUntil:%d",TempMembRec.ValidUntil);
// fflush(stdout);
#endif // AS400_DEBUG
  RETURN_ON_ERROR;


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

	for ( i = 0 ;i < 2; i++)
	{


		TempMembRec.UpdatedBy = 7777;/* Yulia 16.5.99*/
		TempMembRec.LastUpdate.DbDate  = sysdate;/* Yulia 24.6.99*/
		TempMembRec.LastUpdate.DbTime  = systime;/* Yulia 24.6.99*/

/*	  GerrLogReturn(
			GerrId,
			"tempmemb test: %d %d %d %d {%d}",
		TempMembRec.LastUpdate.DbDate,
		TempMembRec.LastUpdate.DbTime,
		TempMembRec.CardId ,TempMembRec.MemberId.Number,i);
*/

		if ( do_insert == ATTEMPT_INSERT_FIRST )	
		{
//			TempMembRec.UpdatedBy = 7777;/* Yulia 16.5.99*/
//			TempMembRec.LastUpdate.DbDate  = sysdate;/* Yulia 24.6.99*/
//			TempMembRec.LastUpdate.DbTime  = systime;/* Yulia 24.6.99*/

  			DbInsertTempMembRecord(&TempMembRec);
  			HANDLE_INSERT_SQL_CODE(  "3003",      p_connect,
						  &do_insert, &retcode,
				 		  &stop_retrying );
		}
		else
		{
  			DbUpdateTempMembRecord(TempMembRec.CardId,&TempMembRec);
#ifdef AS400_DEBUG
// printf("\n Update tempmemb,sqlca %d",sqlca.sqlcode);
// fflush(stdout);
#endif // AS400_DEBUG
  			HANDLE_UPDATE_SQL_CODE(  "3003",      p_connect,
						  &do_insert, &retcode,
				  		  &stop_retrying );

		}
	}
  }
  return retcode;
}
// end of msg 3003 handler



// handler for message 3004 - delete from table TEMPMEMB

static int message_3004_handler( TConnection	*p_connect,
				int		data_len)
{
/* << -------------------------------------------------------- >> */
/*			  local variables			  */
/* << -------------------------------------------------------- >> */
  char          *PosInBuff;		/* current pos in data buffer	    */
  int		ReadInput;		/* amount of Bytesread from buffer  */
  int		retcode;		/* code returned from function	    */
  bool		db_changed_flg;		/* if true - return from function   */
  int		n_retries;		/* retrying counter		    */
  int		i;


/* << -------------------------------------------------------- >> */
/*			    DB variables			  */
/* << -------------------------------------------------------- >> */
EXEC SQL BEGIN DECLARE SECTION;
TIdNumber CardId;
long int sysdate;
long int systime;
EXEC SQL END DECLARE SECTION;

  sysdate = GetDate();
  systime = GetTime();


/* << -------------------------------------------------------- >> */
/*			read input buffer			  */
/* << -------------------------------------------------------- >> */
  INIT_ERROR_DETECTION_MECHANISM;
  PosInBuff = glbMsgBuffer;

#ifdef AS400_DEBUG
 printf("\n PosInBuff>%s<",PosInBuff);
 fflush(stdout);
#endif // AS400_DEBUG

  CardId			=  get_long( &PosInBuff,
			     		     IdNumber_9_len);
#ifdef AS400_DEBUG
// printf("\n CardId:%d",CardId);
// fflush(stdout);
#endif // AS400_DEBUG
  RETURN_ON_ERROR;


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
  db_changed_flg = 0;

  for( n_retries = 0; n_retries < SQL_UPDATE_RETRIES ; n_retries++ )
    {

      /*
       * sleep for a while:
       */
      if( n_retries )
      {
         sleep( SLEEP_TIME_BETWEEN_RETRIES );
      }

  DbDeleteTempMembRecord(CardId);

  HANDLE_DELETE_SQL_CODE( "3004", p_connect,
				  &retcode, 
				  &db_changed_flg);

   break;
   }
   if( n_retries == SQL_UPDATE_RETRIES )
   {
	WRITE_ACCESS_CONFLICT_TO_LOG ("3004");
   }

  return retcode;
}
// end of msg 3004 handler



// handler for message 3006 - insert to table CONTRACT,CONTERM,
//                                            TERMINAL,SUBST
static int message_3006_handler( TConnection	*p_connect,
				int		data_len,
				bool		do_update)
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
  int		ret_contract;	/* code returned from contract insert */
  int		ret_conterm;	/* code returned from conterm insert */
  int		ret_terminal;	/* code returned from terminal insert */
  int		ret_subst;	/* code returned from subst insert */
  int		ret_subst_inv;	/* code returned from subst insert invert */


/* << -------------------------------------------------------- >> */
/*			    DB variables			  */
/* << -------------------------------------------------------- >> */
EXEC SQL BEGIN DECLARE SECTION;
long int sysdate;
long int systime;
	int 			Locationprv;
	int			Contractor_tmp;               
	char 			ServiceType[3];
	TContractRecord 	ContractRecordDummy;
	TContractRecord 	ContractRecord;
	TContermRecord 		ContermRecord;
	TContermRecord 		ContermRecordDummy;
	TSubstRecord 		SubstRecord;
	TSubstRecord 		SubstRecordDummy;
	TTerminalRecord 	TerminalRecord;
EXEC SQL END DECLARE SECTION;

//Y20060720  for erase hebrew from program add to 3008
	if (FLAG_SELECT == 0 )
	{
		// DonR 13Sep2012: Added two new text fields for hospital discharge description.
		EXEC SQL select	hosptyperesd,	hospresnamew,	hospresnamed ,	hosprelnamew,	hosprelnamed ,	termdescwin
			into		:v_typeres_dos,	:v_resname_doc,	:v_resname_win,	:v_relname_doc,	:v_relname_win,	:v_termdesc_win
			 from systexts
			 where keytext = 1;
		if ( SQLERR_error_test() != MAC_OK )
		{
			GerrLogFnameMini ("systext_log",
					   GerrId,
					   "NOK [%s][%s][%s]",
					   v_typeres_dos,	v_resname_doc,	v_resname_win);


		//		return(0);
		}

		GerrLogFnameMini ("systext_log",
					   GerrId,
					   "OK [%s][%s][%s][%s]",
					   v_typeres_dos,	v_resname_doc,	v_resname_win,	v_termdesc_win);
		FLAG_SELECT = 1;

		}

  sysdate = GetDate();
  systime = GetTime();
  ret_contract = 1;
  ret_terminal = 1;
  ret_conterm = 1;
  ret_subst = 1;



/* << -------------------------------------------------------- >> */
/*			read input buffer			  */
/* << -------------------------------------------------------- >> */
  INIT_ERROR_DETECTION_MECHANISM;
  PosInBuff = glbMsgBuffer;

  ContractRecord.Contractor = 		get_long( &PosInBuff,   IdNumber_9_len);  		RETURN_ON_ERROR;

  ContractRecord.ContractType	=	get_short ( &PosInBuff,	ContractType_2_len);  	RETURN_ON_ERROR;

  get_string( &PosInBuff,    ContractRecord.FirstName,      FirstName_8_len);
  get_string( &PosInBuff,    ContractRecord.LastName,      LastName_14_len);
  get_string( &PosInBuff,    ContractRecord.Street,	      Street_16_len);
  get_string( &PosInBuff,    ContractRecord.City,	      City_20_len);
  get_string( &PosInBuff,    ContractRecord.Phone,	      Phone_10_len);
  ContractRecord.Mahoz = get_short( &PosInBuff , Mahoz_len );  RETURN_ON_ERROR;
  ContractRecord.Profession =  get_short( &PosInBuff, 2);	  RETURN_ON_ERROR;
  ContractRecord.AcceptUrgent =  get_short( &PosInBuff, 1);	  RETURN_ON_ERROR;
  ContractRecord.SexesAllowed =  get_short( &PosInBuff, 1);	  RETURN_ON_ERROR;
  ContractRecord.AcceptAnyAge =  get_short( &PosInBuff, 1);	  RETURN_ON_ERROR;
  ContractRecord.AllowKbdEntry =  get_short( &PosInBuff, 3);  RETURN_ON_ERROR;
  ContractRecord.NoShifts =  get_short( &PosInBuff, 1);		  RETURN_ON_ERROR;
  ContractRecord.IgnorePrevAuth =  get_short( &PosInBuff, 1); RETURN_ON_ERROR;
  ContractRecord.AuthorizeAlways =  get_short( &PosInBuff, 1);RETURN_ON_ERROR;
  ContractRecord.NoAuthRecord =  get_short( &PosInBuff, 1);	  RETURN_ON_ERROR;
  ContermRecord.TermId = get_long( &PosInBuff, TermId_7_len); RETURN_ON_ERROR;
  ContermRecord.AuthType = get_short( &PosInBuff,1);		  RETURN_ON_ERROR;
  ContermRecord.ValidFrom = get_long( &PosInBuff,DateYYYYMMDD_len);  RETURN_ON_ERROR;
  ContermRecord.ValidUntil = get_long( &PosInBuff,DateYYYYMMDD_len); RETURN_ON_ERROR;
  ContermRecord.Location = get_long( &PosInBuff,LocationId_7_len);	 RETURN_ON_ERROR;
  ContermRecord.Mahoz = get_short( &PosInBuff , Mahoz_len );		 RETURN_ON_ERROR;
  ContermRecord.Snif = get_long( &PosInBuff,5);				  RETURN_ON_ERROR;
  ContermRecord.Locationprv = get_long( &PosInBuff,LocationId_7_len);  RETURN_ON_ERROR;
  get_string( &PosInBuff,ContermRecord.Position,    	1);
  get_string( &PosInBuff,ContermRecord.Paymenttype,    	1);
  ContermRecord.ServiceType = get_short( &PosInBuff,3);  RETURN_ON_ERROR;
  SubstRecord.AltContractor =  get_long( &PosInBuff, IdNumber_9_len); RETURN_ON_ERROR;
  SubstRecord.SubstType = get_short( &PosInBuff,2);  RETURN_ON_ERROR;
  SubstRecord.DateFrom = get_long( &PosInBuff,DateYYYYMMDD_len);  RETURN_ON_ERROR;
  SubstRecord.DateUntil = get_long( &PosInBuff,DateYYYYMMDD_len); RETURN_ON_ERROR;
  SubstRecord.SubstReason = get_short( &PosInBuff,2);  RETURN_ON_ERROR;
  ContermRecord.City = get_long( &PosInBuff,9);  RETURN_ON_ERROR;
  ContermRecord.Add1 = get_long( &PosInBuff,9);  RETURN_ON_ERROR;
  ContermRecord.Add2 = get_long( &PosInBuff,9);  RETURN_ON_ERROR;
  //new 20110403
  ContermRecord.interface = get_short( &PosInBuff,2);RETURN_ON_ERROR;
  get_string( &PosInBuff,ContermRecord.owner, 	1);
  ContermRecord.group8  = get_long( &PosInBuff,8);  RETURN_ON_ERROR;
  get_string( &PosInBuff,ContermRecord.group_c, 	1);
  ContermRecord.group_join  = get_long( &PosInBuff,7);  RETURN_ON_ERROR;
  ContermRecord.group_act  = get_long( &PosInBuff,7);  RETURN_ON_ERROR;
  ContermRecord.tax   =	get_double	( &PosInBuff,10);  RETURN_ON_ERROR;
  ContermRecord.assign = get_short( &PosInBuff,2);RETURN_ON_ERROR;
  ContermRecord.gnrl_days = get_short( &PosInBuff,1);RETURN_ON_ERROR;
  ContermRecord.act = get_short( &PosInBuff,3);RETURN_ON_ERROR;
  ContermRecord.from_months = get_short( &PosInBuff,4);RETURN_ON_ERROR;
  ContermRecord.to_months = get_short( &PosInBuff,4);RETURN_ON_ERROR;
  ContermRecord.flg_cont = get_short( &PosInBuff,1);RETURN_ON_ERROR;
  ContractRecord.prof2 = get_short( &PosInBuff,2);RETURN_ON_ERROR;
  ContractRecord.prof3 = get_short( &PosInBuff,2);RETURN_ON_ERROR;
  ContractRecord.prof4 = get_short( &PosInBuff,2);RETURN_ON_ERROR;

  if (ContermRecord.ValidFrom && ContermRecord.ValidFrom < 1000000)
	ContermRecord.ValidFrom =  ContermRecord.ValidFrom + 19000000;
  if (ContermRecord.ValidUntil && ContermRecord.ValidUntil < 1000000)
	ContermRecord.ValidUntil = ContermRecord.ValidUntil + 19000000;
  if (SubstRecord.DateFrom  && SubstRecord.DateFrom < 1000000)
	SubstRecord.DateFrom = SubstRecord.DateFrom + 19000000;
  if (SubstRecord.DateUntil && SubstRecord.DateUntil < 1000000)
	SubstRecord.DateUntil = SubstRecord.DateUntil + 19000000;


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

  for( n_retries = 0;
       ( n_retries < SQL_UPDATE_RETRIES ) && ( stop_retrying == false );
       n_retries++ )
    {

  ret_contract = 1;
  ret_terminal = 1;
  ret_conterm = 1;
  ret_subst = 1;
  ret_subst_inv = 1;
      /*
       * sleep for a while:
       */
      if( n_retries )
      {
         sleep( SLEEP_TIME_BETWEEN_RETRIES );
      }
/*-----------------------------------------------------------------------
| 1. Update /Insert contract record   |
-----------------------------------------------------------------------*/ 
	ContractRecord.LastUpdate.DbDate 	= sysdate;
	ContractRecord.LastUpdate.DbTime 	= systime;
	ContractRecord.UpdatedBy = 7777 ;
	if ( DbGetContractRecord( 	ContractRecord.Contractor,
								ContractRecord.ContractType,
								&ContractRecordDummy) )
	{
		ret_contract = 	DbUpdateContractRecord(	ContractRecord.Contractor,
												ContractRecord.ContractType,
												&ContractRecord) ;
	}
	else
	{
		ret_contract = DbInsertContractRecord(	&ContractRecord );
	}
/*-----------------------------------------------------------------------
| 2. Insert terminal record with previosly check if he exist already    |
-----------------------------------------------------------------------*/ 
	if ( DbGetTerminalRecord( ContermRecord.TermId,
				&TerminalRecord) )
	{
//		printf ("\n This terminal exist in the system already: %d",
//				ContermRecord.TermId); fflush(stdout);

	}
	else
	{

//     	strcpy(TerminalRecord.Descr," AZL ");  // test 2000.05.07
		strcpy(TerminalRecord.Descr,v_termdesc_win);  // test 20060723
		strcat(TerminalRecord.Descr," ");
		strcat(TerminalRecord.Descr,ContractRecord.LastName);
		strcat(TerminalRecord.Descr," ");
		strcat(TerminalRecord.Descr,ContractRecord.FirstName);
		strcpy(TerminalRecord.Street,ContractRecord.Street);
		strcpy(TerminalRecord.City,ContractRecord.City);
//01.02.2000    strcat(TerminalRecord.LocPhone,ContractRecord.Phone);
		strcpy(TerminalRecord.LocPhone,ContractRecord.Phone);
		TerminalRecord.AreaCode			= 0;
		TerminalRecord.TermType			= 1;
		strcpy(TerminalRecord.ModemPrefix," ");
		TerminalRecord.Phone[0].Prefix 	= 0;
		TerminalRecord.Phone[0].Number 	= 0;
		TerminalRecord.Phone[1].Prefix 	= 0;
		TerminalRecord.Phone[1].Number 	= 0;
		TerminalRecord.Phone[2].Prefix 	= 0;
		TerminalRecord.Phone[2].Number 	= 0;
		TerminalRecord.StartupTime.DbDate= sysdate;
		TerminalRecord.StartupTime.DbTime= systime;
		TerminalRecord.SoftwareVer	= 0;
		TerminalRecord.HardwareVer	= 0;
		TerminalRecord.TermId	= ContermRecord.TermId;
		TerminalRecord.Contractor 	=  ContractRecord.Contractor;
		TerminalRecord.ContractType 	=  ContractRecord.ContractType;
		TerminalRecord.LastUpdate.DbDate 	= sysdate;
		TerminalRecord.LastUpdate.DbTime  	= systime;
		TerminalRecord.UpdatedBy 	= 7777 ;
//		printf ("\n Insert terminal the system : >%d<",
//					ContermRecord.TermId); fflush(stdout);
		ret_terminal = DbInsertTerminalRecord(	&TerminalRecord );
	}
/*-----------------------------------------------------------------------
| 3. Insert conterm  record with previosly check if he exist already    |
-----------------------------------------------------------------------*/ 
	ContermRecord.Contractor =  ContractRecord.Contractor;
	ContermRecord.ContractType =  ContractRecord.ContractType;
	ContermRecord.LastUpdate.DbDate 	= sysdate;
	ContermRecord.LastUpdate.DbTime  	= systime;
	ContermRecord.UpdatedBy = 7777 ;
	ContermRecord.HasPharmacy = 1;

	if( ContermRecord.AuthType == 2 &&
	    ContermRecord.ValidFrom == 0 && ContermRecord.ValidUntil == 0)
	{
	} /* contractor work in his clinic only to period from subst.alt */
	else
	{
	    if ( DbGetContermRecord( ContermRecord.TermId,
					ContractRecord.Contractor,
			    		ContractRecord.ContractType,
					&ContermRecordDummy) )
		{
	    		 ret_conterm = 
					DbUpdateContermRecord(
						ContermRecord.TermId,
						ContractRecord.Contractor,
				    		ContractRecord.ContractType,
						&ContermRecord );

		}
		else
		{
			ret_conterm = DbInsertContermRecord(&ContermRecord );
		}
    } /* else authtype <> 2 || from,to <>0 */
/*----------------------------------------------------------------------
| 4. Update/Insert subst record     |
-----------------------------------------------------------------------*/ 
	if ( SubstRecord.AltContractor )  /* only if exist */
	{
		SubstRecord.Contractor 	=  ContractRecord.Contractor;
		SubstRecord.ContractType=  ContractRecord.ContractType;
		SubstRecord.LastUpdate.DbDate 	= sysdate;
		SubstRecord.LastUpdate.DbTime  	= systime;
		SubstRecord.UpdatedBy 	= 7777 ;
		if ( DbGetSubstRecord( 	ContractRecord.Contractor,
								ContractRecord.ContractType,
								SubstRecord.AltContractor,
								&SubstRecordDummy) )
		{
			ret_subst	=	DbUpdateSubstRecord(	ContractRecord.Contractor,
													ContractRecord.ContractType,
													SubstRecord.AltContractor,
													&SubstRecord) ;
		}
		else
		{
			ret_subst =  DbInsertSubstRecord(&SubstRecord );
		}

 /*--------------------------------------------------*/
 /*  4a) insert invert subst record                  */
 /*--------------------------------------------------*/
    	Contractor_tmp              =  SubstRecord.Contractor ;
		SubstRecord.Contractor      =  SubstRecord.AltContractor;
		SubstRecord.AltContractor   =  Contractor_tmp  ;

		if ( DbGetSubstRecord(	SubstRecord.Contractor,
							  	ContractRecord.ContractType,
								SubstRecord.AltContractor,
						     	&SubstRecordDummy) )
		 {
			ret_subst_inv = DbUpdateSubstRecord(
								SubstRecord.Contractor,
					     		ContractRecord.ContractType,
							 	SubstRecord.AltContractor,
			     				&SubstRecord) ;
		}
		else
		{
		     ret_subst_inv = DbInsertSubstRecord(    &SubstRecord );
		 }         
	}
if ( SubstRecord.AltContractor == 0 &&  SubstRecord.DateFrom &&  SubstRecord.DateUntil  ) 
	{
		SubstRecord.Contractor 			=  ContractRecord.Contractor;
		SubstRecord.ContractType		=  ContractRecord.ContractType;
		SubstRecord.LastUpdate.DbDate 	= sysdate;
		SubstRecord.LastUpdate.DbTime  	= systime;
		SubstRecord.UpdatedBy 			= 7777 ;
	GerrLogMini (GerrId," SubstType(4) [%9d-%1d-%7d-%9d-%8d-%8d-%1d]",
		ContractRecord.Contractor,		ContractRecord.ContractType,
		ContermRecord.TermId,			SubstRecord.Contractor,
		SubstRecord.DateFrom,			SubstRecord.DateUntil,
		SubstRecord.SubstType		);
		if (SubstRecord.SubstType != 4)
		{
			SubstRecord.SubstType = 4;
		}
		ret_subst=DbInsertSubstAbsRecord(&SubstRecord) ;
	}
	/*************************************************/
   	/* only if all 4 tables are all right:           */
	/*		- need insert + do it            */
	/*		- don't need insert              */
	/*	                          we have commit */
	/*************************************************/
   retcode = 1- ret_contract * ret_conterm * ret_terminal * 
   				ret_subst * ret_subst_inv;
   if( retcode == 0) stop_retrying = true;
	GerrLogMini (GerrId," retcodes:[%d-%d-%d-%d-%d-%d] [%9d-%1d-%7d-%8d-%8d-subst-%9d-%8d-%8d]",
		ret_contract,
		ret_conterm,
		ret_terminal,
		ret_subst,
		ret_subst_inv,
		stop_retrying,
		ContractRecord.Contractor,
		ContractRecord.ContractType,
		ContermRecord.TermId,
		ContermRecord.ValidFrom,
		ContermRecord.ValidUntil,
		SubstRecord.Contractor,
		SubstRecord.DateFrom,
		SubstRecord.DateUntil
		);
   }
    return 1;  //Yulia 20040809
}
// end of msg 3006 handler



// handler for message 3005 - update to table AUTH

static int message_3005_handler( TConnection	*p_connect,
				int		data_len,
				bool		do_update)
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
EXEC SQL BEGIN DECLARE SECTION;
TMemberId MemberId;
TIdNumber Contractor;
short 	  CurrQuater;
long int sysdate;
long int systime;
EXEC SQL END DECLARE SECTION;

  sysdate = GetDate();
  systime = GetTime();


/* << -------------------------------------------------------- >> */
/*			read input buffer			  */
/* << -------------------------------------------------------- >> */
  INIT_ERROR_DETECTION_MECHANISM;
  PosInBuff = glbMsgBuffer;

#ifdef AS400_DEBUG
 printf("\n PosInBuff>%s<",PosInBuff);
 fflush(stdout);
#endif // AS400_DEBUG
//	  printf("\n 3005 PosInBuff>%s<\n",PosInBuff); fflush(stdout);

  Contractor			=  get_long( &PosInBuff,
			     		     IdNumber_9_len);
  RETURN_ON_ERROR;
  MemberId.Code				=  get_short( &PosInBuff,
			     		     1);
  RETURN_ON_ERROR;
  MemberId.Number			=  get_long( &PosInBuff,
			     		     IdNumber_9_len);
  RETURN_ON_ERROR;

  CurrQuater 	= (((((sysdate/100) % 100 ) - 1)/3 ) +1 );
#ifdef AS400_DEBUG
 printf("  quart>%d< sysdate >%d< >%d< ",CurrQuater,sysdate,MacAuthCancelByAs400);
 fflush(stdout);
#endif // AS400_DEBUG

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

  
	if (!	    DbCancleAuthVisit (Contractor,&MemberId,CurrQuater,MacAuthCancelByAs400))
	{
	  GerrLogReturn(
			GerrId,
			"3005 problem upd VN: [%d][%d]",
			Contractor,MemberId.Number
			);
	}

	  stop_retrying = true;
		retcode = MSG_HANDLER_SUCCEEDED;	
		//break;

   }
  return retcode;
}
// end of msg 3005 handler



// handler for message 3007 - delete from table CONTERM

static int message_3007_handler( TConnection	*p_connect,
				int		data_len)
{
/* << -------------------------------------------------------- >> */
/*			  local variables			  */
/* << -------------------------------------------------------- >> */
  char          *PosInBuff;		/* current pos in data buffer	    */
  int		ReadInput;		/* amount of Bytesread from buffer  */
  int		retcode;		/* code returned from function	    */
  bool		db_changed_flg;		/* if true - return from function   */
  int		n_retries;		/* retrying counter		    */
  int		i;


/* << -------------------------------------------------------- >> */
/*			    DB variables			  */
/* << -------------------------------------------------------- >> */
EXEC SQL BEGIN DECLARE SECTION;
TIdNumber		Contractor;
TContractType	ContractType;
TLocationId		Location;
TTermId			TermId;
long int sysdate;
long int systime;
EXEC SQL END DECLARE SECTION;

  sysdate = GetDate();
  systime = GetTime();


/* << -------------------------------------------------------- >> */
/*			read input buffer			  */
/* << -------------------------------------------------------- >> */
  INIT_ERROR_DETECTION_MECHANISM;
  PosInBuff = glbMsgBuffer;


Contractor			=  get_long( &PosInBuff,
			     		     IdNumber_9_len);

  RETURN_ON_ERROR;

ContractType			=  get_short( &PosInBuff,
			     		     ContractType_2_len);

  RETURN_ON_ERROR;

  TermId			=  get_long( &PosInBuff,
			     		     TermId_7_len);

  RETURN_ON_ERROR;

  Location			=  get_long( &PosInBuff,
			     		     Location_7_len);

  RETURN_ON_ERROR;

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
  db_changed_flg = 0;

  for( n_retries = 0; n_retries < SQL_UPDATE_RETRIES ; n_retries++ )
    {

      /*
       * sleep for a while:
       */
      if( n_retries )
      {
         sleep( SLEEP_TIME_BETWEEN_RETRIES );
      }

//  DbDeleteTempMembRecord(CardId);
	DbDeleteContermRecord(Location,Contractor,ContractType);


    HANDLE_DELETE_SQL_CODE( "3007", p_connect,
				  &retcode, 
				  &db_changed_flg);

   break;
   }
   if( n_retries == SQL_UPDATE_RETRIES )
   {
	WRITE_ACCESS_CONFLICT_TO_LOG ("3007");
   }

  return retcode;
}
// end of msg 3007 handler



// handler for message 3008 - insert to table Hospitalmsg

static int message_3008_handler( TConnection	*p_connect,
				 int		data_len,
				 bool		do_insert	)
{
/* << -------------------------------------------------------- >> */
/*			  local variables			  */
/* << -------------------------------------------------------- >> */
  char          *PosInBuff;		/* current pos in data buffer	    */
  int		ReadInput;		/* amount of Bytesread from buffer  */
  int		retcode1;		/* code returned from function	    */
  bool		stop_retrying;		/* if true - return from function   */
  int		n_retries;		/* retrying counter		    */
  int		i;
  int		j,k,line_flg,line_no;
/* << -------------------------------------------------------- >> */
/*			    DB variables			  */
/* << -------------------------------------------------------- >> */
EXEC SQL BEGIN DECLARE SECTION;
THospitalRecord		HospitalRec;
TLabRecord	LabResRec;/*20030727*/
int sysdate;
int systime;
int KeyText = 1;
char str_typeresult[25];
EXEC SQL END DECLARE SECTION;

//Y20060720  for erase hebrew from program add to 3006
	if (FLAG_SELECT == 0 )
	{
		// DonR 13Sep2012: Added two new text fields for hospital discharge description.
		ExecSQL (	MAIN_DB, AS400UDS_T3008_READ_systexts,
					&v_typeres_dos,		&v_resname_doc,
					&v_resname_win,		&v_relname_doc,
					&v_relname_win,		&v_termdesc_win,
					&KeyText,			END_OF_ARG_LIST		);

		if ( SQLERR_error_test() != MAC_OK )
		{
			GerrLogFnameMini ("systext_log",
					   GerrId,
					   "NOK [%s][%s][%s]",
					   v_typeres_dos,	v_resname_doc,	v_resname_win);


		//		return(0);
		}

		GerrLogFnameMini ("systext_log",
					   GerrId,
					   "OK [%s][%s][%s]",
					   v_typeres_dos,	v_resname_doc,	v_resname_win);
		FLAG_SELECT = 1;

		}


  sysdate = GetDate();
  systime = GetTime();
  line_no = 0;

/* << -------------------------------------------------------- >> */
/*			read input buffer			  */
/* << -------------------------------------------------------- >> */
  INIT_ERROR_DETECTION_MECHANISM;
  PosInBuff = glbMsgBuffer;



//GerrLogFnameMini ("3008_log", GerrId,"PosInBuff>%s<",glbMsgBuffer);


 HospitalRec.Contractor	=  get_intv( &PosInBuff,
				     	    IdNumber_9_len);
#ifdef AS400_DEBUG
 printf("\n 3008 HospitalRec.Contractor:%d",HospitalRec.Contractor);
 fflush(stdout);
#endif // AS400_DEBUG
  RETURN_ON_ERROR;

  HospitalRec.MemberId.Code		=  get_short( &PosInBuff,
				    		      1);
  RETURN_ON_ERROR;

  HospitalRec.MemberId.Number	=  get_intv( &PosInBuff,
				     		     IdNumber_9_len);
  RETURN_ON_ERROR;

  HospitalRec.HospDate	=  get_intv( &PosInBuff,    DateYYYYMMDD_len);
  RETURN_ON_ERROR;

  HospitalRec.NurseDate	=  get_intv( &PosInBuff,    DateYYYYMMDD_len);
  RETURN_ON_ERROR;

			   get_string(
				      &PosInBuff,
  				      HospitalRec.ContrName,
				      30);
  HospitalRec.HospitalNumb			=  get_intv( &PosInBuff,   ReqId_9_len);
  RETURN_ON_ERROR;

			   get_string(
				      &PosInBuff,
  				      HospitalRec.Result,
				      1120);
			   get_string(
				      &PosInBuff,
  				      HospitalRec.HospName,
				      25);
   			   get_string(
				      &PosInBuff,
  				      HospitalRec.DepartName,
				      30);

  HospitalRec.FlgSystem		=  get_short( &PosInBuff,
				    		      1);

  // DonR 25Nov2021 User Story #206812 - add another "doctor TZ", used only to indicate member's death in hospital (value = 3).
  HospitalRec.SecondDoctorTZ	=  get_intv( &PosInBuff, 9);

	strcpy(HospitalRec.TypeResult,  v_typeres_dos);

	HospitalRec.ResCount	=	1;
	//only windows hebrew without reverse 20040607 Yulia
	//insert hospital and department names into contrname and typeresult fields
	//for Win version
	if (HospitalRec.FlgSystem == 1)
	{
		// DonR 13Sep2012: Assign description based on whether this is a hospitalization or a discharge from hospital.
		if (HospitalRec.NurseDate <= 20000000)
		{
			// This member is currently a hospital inpatient.
			strcpy(HospitalRec.ResName1, v_resname_win);
		}
		else
		{
			// This member has been discharged from hospital.
			strcpy(HospitalRec.ResName1, v_relname_win);
		}

		strcpy (HospitalRec.ContrName,HospitalRec.DepartName);
		strcpy (HospitalRec.TypeResult,HospitalRec.HospName);
		if (HospitalRec.Contractor == 12)
		{
			GerrLogFnameMini ("3008_log", GerrId,"Wresname1[%s]",
				HospitalRec.ResName1 );
			GerrLogFnameMini ("3008_log", GerrId,"WType    [%s]",
				HospitalRec.TypeResult);
			GerrLogFnameMini ("3008_log", GerrId,"Wcname   [%s]",
				HospitalRec.ContrName);

		}
	}
	else
	{
		// DonR 13Sep2012: Assign description based on whether this is a hospitalization or a discharge from hospital.
		if (HospitalRec.NurseDate <= 20000000)
		{
			// This member is currently a hospital inpatient.
			strcpy(HospitalRec.ResName1, v_resname_doc);
		}
		else
		{
			// This member has been discharged from hospital.
			strcpy(HospitalRec.ResName1, v_relname_doc);
		}

		if (HospitalRec.Contractor == 12)
		{
			GerrLogFnameMini ("3008_log", GerrId,"Dresname1[%s]",
				HospitalRec.ResName1 );
			GerrLogFnameMini ("3008_log", GerrId,"DType    [%s]",
				HospitalRec.TypeResult);
			GerrLogFnameMini ("3008_log", GerrId,"Dcname   [%s]",
				HospitalRec.ContrName);

		}

	}
	HospitalRec.ResultSize = 1120;;
	for (j = 0; j< 1120; j = j +56)
	{
		line_flg = 0;
		for (k = 0;k < 55;k++)
		{
			if (HospitalRec.Result[ j +k] != ' ' ) 
			{
//				printf("|%d|{%d:%c}",k,HospitalRec.Result[ j +k],HospitalRec.Result[ j +k]);fflush(stdout);
				line_flg = 1;
				break;
			}

		}
		if (!line_flg ) 
		{
//			line_no++;
//			HospitalRec.ResultSize = HospitalRec.ResultSize + line_no;
			HospitalRec.ResultSize = j ;
//			HospitalRec.ResultSize = j + 57; // one addition space for new line in macauth
			break;
		}
		else
		{
//			HospitalRec.Result[ j + 55] ='\n';
		}
	}
/* << -------------------------------------------------------- >> */
/*			    debug messages			  */
/* << -------------------------------------------------------- >> */

  /*
   *   verify all data was read:
   */
  ReadInput = PosInBuff - glbMsgBuffer;
//	GerrLogFnameMini ("3008_log", GerrId,"[%d][%d]",ReadInput ,data_len);
//  GerrLogMini (GerrId, "3008: ReadInput = %d, data_len = %d, SecondDoctorTZ = %d.", ReadInput, data_len, HospitalRec.SecondDoctorTZ); 
  StaticAssert( ReadInput == data_len );

/* << -------------------------------------------------------- >> */
/*	   up to SQL_UPDATE_RETRIES retries will be made	  */
/*		    to overcome access conflict			  */
/* << -------------------------------------------------------- >> */
  retcode1 = ALL_RETRIES_WERE_USED;
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
       *  try to insert new rec with the order depending on "do_insert":
       */

/*20030727 Yulia*/
	  LabResRec.MemberId.Number		=	HospitalRec.MemberId.Number;
	  LabResRec.MemberId.Code		=	HospitalRec.MemberId.Code;
	  LabResRec.Contractor			=	HospitalRec.Contractor;
	  HospitalRec.TermId			=	0;
	  DbGetLabTerm(&LabResRec);
	  HospitalRec.TermId			=	LabResRec.TermId;
/*end 20030727*/

	  retcode1	=   DbInsertHospitalRecord(&HospitalRec);

 //     HANDLE_INSERT_SQL_CODE( "3008",	 	p_connect,
//			      &do_insert, 	&retcode1,
//			      &stop_retrying );


		if( retcode1 == 1) stop_retrying = true;
   }

	return MSG_HANDLER_SUCCEEDED;

}
// end of msg 3008 handler



// handler for message 3009 - insert to/delete from table CONMEMB
// (for dieticians only!)

static int message_3009_handler (TConnection	*p_connect,
								 int			data_len,
								 bool			do_insert_NIU)
{
	// local variables
	char	*PosInBuff;		// current pos in data buffer
	int		ReadInput;		// number of Bytes read from buffer
	int		retcode1;		// code returned from function
	bool	stop_retrying;	// if true - return from function
	int		n_retries;		// retrying counter
	short	DB_action;		// 1 = Delete, 2 = Insert

	short	day;
	short	month;
	short	year;
	short	leap_day;
	short	MonthDays[12] = {31,28,31,30,31,30,31,31,30,31,30,31};

	// DB variables
	EXEC SQL BEGIN DECLARE SECTION;
		TConMembRecord	ConMembRec;
		long int		sysdate;
		long int		systime;
	EXEC SQL END DECLARE SECTION;


	sysdate = GetDate ();
	systime = GetTime ();

	// read input buffer
	INIT_ERROR_DETECTION_MECHANISM;
	PosInBuff = glbMsgBuffer;


	DB_action					= get_short	(&PosInBuff, 1					); RETURN_ON_ERROR;
	ConMembRec.MemberId.Code	= get_short	(&PosInBuff, IdCode_1_len		); RETURN_ON_ERROR;
	ConMembRec.MemberId.Number	= get_long	(&PosInBuff, IdNumber_9_len		); RETURN_ON_ERROR;
	ConMembRec.Contractor		= get_long	(&PosInBuff, IdNumber_9_len		); RETURN_ON_ERROR;
	ConMembRec.ValidFrom		= get_long	(&PosInBuff, DateYYYYMMDD_len	); RETURN_ON_ERROR;

//	GerrLogFnameMini ("3009_log",
//					   GerrId,
//					   "Action %d, MemberID %d, Contractor %d, ValidFrom %d.",
//					   (int)DB_action,
//					   (int)ConMembRec.MemberId.Number,
//					   (int)ConMembRec.Contractor,
//					   (int)ConMembRec.ValidFrom);


	// verify all data was read
	ReadInput = PosInBuff - glbMsgBuffer;
	StaticAssert (ReadInput == data_len);


	retcode1 = ALL_RETRIES_WERE_USED;
	stop_retrying = false;

	// Set Profession to nonsense value to indicate that we haven't retrieved it yet.
	ConMembRec.Profession = -99;


	// up to SQL_UPDATE_RETRIES retries will be made to overcome access conflict
	for (n_retries = 0;
		 (n_retries < SQL_UPDATE_RETRIES) && (stop_retrying == false);
		 n_retries++)
	{

		// If this a retry, sleep a bit.
		if (n_retries > 0)
		{
			sleep (SLEEP_TIME_BETWEEN_RETRIES);
		}


		// DonR 17Aug2003: Assume, for the moment, that we're going to succeed.
		stop_retrying = true;

		// Fill in Contract Type & Profession from Contractor table.
		if (ConMembRec.Profession == -99)
		{
			EXEC SQL
				SELECT	MIN(contracttype),
						MIN(profession)
				INTO	:ConMembRec.ContractType,
						:ConMembRec.Profession
				FROM	contract
				WHERE	contractor = :ConMembRec.Contractor;

//			GerrLogFnameMini ("3009_log",
//							   GerrId,
//							   "Contract Type = %d, Profession = %d for Contractor %d - SQLCODE = %d.",
//							   (int)ConMembRec.ContractType,
//							   (int)ConMembRec.Profession,
//							   (int)ConMembRec.Contractor,
//							   (int)SQLCODE);
		}
					 

		// Insert new row or delete old one, depending on DB_action.
		// 20Mar2003: If we're doing an insertion, do a deletion automatically.
		if ((DB_action == 1) || (DB_action == 2))		// Delete or Insert
		{
			EXEC SQL
				DELETE
				FROM	conmemb
				WHERE	IdNumber	=	:ConMembRec.MemberId.Number
				  AND	IdCode		=	:ConMembRec.MemberId.Code
				  AND	ConSeq		=	0
				  AND	Profession	=	:ConMembRec.Profession;
			
//			r3009_uncommitted++;

	  		if (SQLERR_error_test ())
			{
				GerrLogToFileName ("3009_log",
								   GerrId,
								   "DB error on delete - SQLCODE = %d.",
								   SQLCODE);

				// DonR 17Aug2003 - turn retry back on, since we hit some kind of error.
				stop_retrying = false;

			}
//			else
//			GerrLogFnameMini ("3009_log",
//							   GerrId,
//							   "Deleted conmemb row for Member %d / Profession %d - SQLCODE = %d.",
//							   (int)ConMembRec.MemberId.Number,
//							   (int)ConMembRec.Profession,
//							   (int)SQLCODE);

		}
		else	// Unrecognized Action Code!
		{
			return (BAD_DATA_RECEIVED);
		}


		if (DB_action == 2)		// Insert.
		{
			// Compute Valid-Until Date from Valid-From Date.
			day		= ConMembRec.ValidFrom			% 100;
			month	= (ConMembRec.ValidFrom / 100)	% 100;
			year	= ConMembRec.ValidFrom / 10000;

			if (day == 1)
			{
				if (month == 1)
				{
					year = year - 1;
					month = 12;
				}
				else
				{
					month = month - 1;
				}

				// Note: This logic will have 2100 as a leap year - which isn't true.
				if (((year + 1) % 4 == 0) && (month == 2))
					leap_day = 1;
				else
					leap_day = 0;

				ConMembRec.ValidUntil =   ((year + 1) * 10000)
										+ (month * 100)
										+ MonthDays [month -1] + leap_day;
			}
			else
			{
				ConMembRec.ValidUntil = ConMembRec.ValidFrom + 10000 - 1;
			}
			
			
			// Constant values.
			ConMembRec.LinkType				= ConMembLinkDiet;		
			ConMembRec.UpdatedBy			= UPDATED_BY_AS400;
			ConMembRec.ConSeq				= 0;
			ConMembRec.LastUpdate.DbDate	= sysdate;
			ConMembRec.LastUpdate.DbTime	= systime;
			strcpy (ConMembRec.ConMembNote, "");


			EXEC SQL
				INSERT INTO	conmemb		(
										IdNumber,
										IdCode,
										ConSeq,
										Contractor,
										ContractType,
										Profession,
										LinkType,
										ValidFrom,
										ValidUntil,
										LastUpdateDate,
										LastUpdateTime,
										UpdatedBy,
										ConMembNote
										)

							values		(
										:ConMembRec.MemberId.Number,
										:ConMembRec.MemberId.Code,
										:ConMembRec.ConSeq,
										:ConMembRec.Contractor,
										:ConMembRec.ContractType, 	
										:ConMembRec.Profession,
										:ConMembRec.LinkType,
										:ConMembRec.ValidFrom,
										:ConMembRec.ValidUntil,
										:ConMembRec.LastUpdate.DbDate,
										:ConMembRec.LastUpdate.DbTime,
										:ConMembRec.UpdatedBy,
										:ConMembRec.ConMembNote
										);

	  		if (SQLERR_error_test ())
			{
				GerrLogToFileName ("3009_log",
								   GerrId,
								   "DB error on insert - SQLCODE = %d.",
								   SQLCODE);

				// DonR 17Aug2003 - turn retry back on, since we hit some kind of error.
				stop_retrying = false;
			}
//			else
//			GerrLogFnameMini ("3009_log",
//							   GerrId,
//							   "Inserted conmemb row for Member %d / Profession %d - SQLCODE = %d.",
//							   (int)ConMembRec.MemberId.Number,
//							   (int)ConMembRec.Profession,
//							   (int)SQLCODE);
	
		}	// Insert new CONMEMB row.

	}	// Retries loop.

//	r3009_counter++;
//	r3009_uncommitted++;
//	GerrLogFnameMini ("3009_log",
//					   GerrId,
//					   "Finished with 3009 message. %d processed, %d changes to commit.",
//					   (int)r3009_counter,
//					   (int)r3009_uncommitted);


	return MSG_HANDLER_SUCCEEDED;
}
// end of msg 3009 handler

/*Yulia 20040623*/
// handler for message 3010 - insert to table Hospitalmsg(patology)

static int message_3010_handler( TConnection	*p_connect,
				 int		data_len,
				 bool		do_insert	)
{
/* << -------------------------------------------------------- >> */
/*			  local variables			  */
/* << -------------------------------------------------------- >> */
  char      *PosInBuff;		/* current pos in data buffer	    */
  int		ReadInput;		/* amount of Bytesread from buffer  */
  int		retcode1;		/* code returned from function	    */
  bool		stop_retrying;		/* if true - return from function   */
  int		n_retries;		/* retrying counter		    */
  int		i;
  int		j,k,line_flg,line_no;
/* << -------------------------------------------------------- >> */
/*			    DB variables			  */
/* << -------------------------------------------------------- >> */
EXEC SQL BEGIN DECLARE SECTION;
THospitalRecord		HospitalRec;
TLabRecord	LabResRec;/*20030727*/
EXEC SQL END DECLARE SECTION;
  line_no = 0;

/* << -------------------------------------------------------- >> */
/*			read input buffer			  */
/* << -------------------------------------------------------- >> */
  INIT_ERROR_DETECTION_MECHANISM;
  PosInBuff = glbMsgBuffer;

//GerrLogFnameMini ("3010_log",  GerrId,"[%s]",glbMsgBuffer);

  HospitalRec.MemberId.Code		=  get_short( &PosInBuff,      1);
  RETURN_ON_ERROR;
	
  HospitalRec.MemberId.Number	=  get_intv( &PosInBuff,    IdNumber_9_len);
  RETURN_ON_ERROR;

  HospitalRec.Contractor	=  get_intv( &PosInBuff,  IdNumber_9_len);
  RETURN_ON_ERROR;

  HospitalRec.HospitalNumb			=  get_intv( &PosInBuff,   ReqId_9_len);
  RETURN_ON_ERROR;

  get_string(  &PosInBuff,  HospitalRec.ResName1,    30);

  HospitalRec.HospDate	=  get_intv( &PosInBuff,    DateYYYYMMDD_len);
  RETURN_ON_ERROR;

  HospitalRec.NurseDate	=  get_intv( &PosInBuff,    DateYYYYMMDD_len);
  RETURN_ON_ERROR;

  get_string(  &PosInBuff,  HospitalRec.TypeResult,   25);
  
  get_string(  &PosInBuff,  HospitalRec.ContrName,    30);

  HospitalRec.PatFlg		=  get_short( &PosInBuff,      1); //20051129
  RETURN_ON_ERROR;
  
  get_string(  &PosInBuff,  HospitalRec.Result,       2048);

 DosWinHebConvMultiLine( HospitalRec.Result    , strlen(HospitalRec.Result) ); //20040708
  DosWinHebConvMultiLine( HospitalRec.ContrName , strlen(HospitalRec.ContrName) ); //20040908

  //  GerrLogFnameMini ("3010_log",  GerrId,"[%s]",HospitalRec.Result);

  HospitalRec.FlgSystem		=  2; //0 - old DOS format,1-new WIN format,2-PATO system WIN 
  HospitalRec.ResCount	=	1;
  HospitalRec.ResultSize = 2048;
  for (j=HospitalRec.ResultSize-1;j>0;j--)
  {
//GerrLogFnameMini ("3010_log",  GerrId,"[%s]num[%d][%c][%d]",HospitalRec.Result[j],j,
//				  HospitalRec.Result[j],HospitalRec.Result[j]);
	  if (HospitalRec.Result[j]!=32) break;
  }
//GerrLogFnameMini ("3010_log",  GerrId,"[%d-real%d][%d][%d]",HospitalRec.ResultSize,j,
//				  HospitalRec.Contractor,HospitalRec.MemberId.Number);
HospitalRec.ResultSize=j+1;
/* << -------------------------------------------------------- >> */
/*			    debug messages			  */
/* << -------------------------------------------------------- >> */
  /*   verify all data was read:   */
  ReadInput = PosInBuff - glbMsgBuffer;
//GerrLogFnameMini ("3010_log",  GerrId,"[%d][%d]",ReadInput,data_len);
  StaticAssert( ReadInput == data_len );
/* << -------------------------------------------------------- >> */
/*	   up to SQL_UPDATE_RETRIES retries will be made	  */
/*		    to overcome access conflict			  */
/* << -------------------------------------------------------- >> */
  retcode1 = ALL_RETRIES_WERE_USED;
  stop_retrying = false;

  for( n_retries = 0;
       ( n_retries < SQL_UPDATE_RETRIES ) && ( stop_retrying == false );
       n_retries++ )
    {
      /* sleep for a while:  */
      if( n_retries )
      {
         sleep( SLEEP_TIME_BETWEEN_RETRIES );
      }
      /*  try to insert new rec with the order depending on "do_insert":      */

	  LabResRec.MemberId.Number		=	HospitalRec.MemberId.Number;
	  LabResRec.MemberId.Code		=	HospitalRec.MemberId.Code;
	  LabResRec.Contractor			=	HospitalRec.Contractor;
	  HospitalRec.TermId			=	0;
	  DbGetLabTerm(&LabResRec);
	  HospitalRec.TermId			=	LabResRec.TermId;

	  retcode1	=   DbInsertHospitalRecord(&HospitalRec);
 	  if( retcode1 == 1) stop_retrying = true;
   }

	return MSG_HANDLER_SUCCEEDED;

}
// end of msg 3010 handler

// handler for message 3011 - insert to table Speechclient
static int message_3011_handler( TConnection	*p_connect,
				 int		data_len,
				 bool		do_insert	)
{
/* << -------------------------------------------------------- >> */
/*			  local variables			  */
/* << -------------------------------------------------------- >> */
  char      *PosInBuff;		/* current pos in data buffer	    */
  int		ReadInput;		/* amount of Bytesread from buffer  */
  int		retcode1;		/* code returned from function	    */
  bool		stop_retrying;		/* if true - return from function   */
  int		n_retries;		/* retrying counter		    */
/* << -------------------------------------------------------- >> */
/*			    DB variables			  */
/* << -------------------------------------------------------- >> */
EXEC SQL BEGIN DECLARE SECTION;
TSpeechClientRecord		SpeechClientRecord;
EXEC SQL END DECLARE SECTION;


/* << -------------------------------------------------------- >> */
/*			read input buffer			  */
/* << -------------------------------------------------------- >> */
  INIT_ERROR_DETECTION_MECHANISM;
  PosInBuff = glbMsgBuffer;

  SpeechClientRecord.MemberId.Code		=  get_short( &PosInBuff,      1);
  RETURN_ON_ERROR;

  SpeechClientRecord.MemberId.Number	=  get_intv( &PosInBuff,    IdNumber_9_len);
  RETURN_ON_ERROR;

  SpeechClientRecord.Location			=  get_intv( &PosInBuff,   LocationId_7_len);
  RETURN_ON_ERROR;

  SpeechClientRecord.FromDate			=  get_intv( &PosInBuff,    DateYYYYMMDD_len);
  RETURN_ON_ERROR;

  SpeechClientRecord.ToDate				=  get_intv( &PosInBuff,    DateYYYYMMDD_len);
  RETURN_ON_ERROR;

/* << -------------------------------------------------------- >> */
/*			    debug messages			  */
/* << -------------------------------------------------------- >> */
  /*   verify all data was read:   */
  ReadInput = PosInBuff - glbMsgBuffer;
  StaticAssert( ReadInput == data_len );
/* << -------------------------------------------------------- >> */
/*	   up to SQL_UPDATE_RETRIES retries will be made	  */
/*		    to overcome access conflict			  */
/* << -------------------------------------------------------- >> */
  retcode1 = ALL_RETRIES_WERE_USED;
  stop_retrying = false;

  for( n_retries = 0;
       ( n_retries < SQL_UPDATE_RETRIES ) && ( stop_retrying == false );
       n_retries++ )
    {
      /* sleep for a while:  */
      if( n_retries )
      {
         sleep( SLEEP_TIME_BETWEEN_RETRIES );
      }
      /*  try to insert new rec with the order depending on "do_insert":      */


	  retcode1	=   DbInsertSpeechClientRecord(&SpeechClientRecord);
 	  if( retcode1 == 1) stop_retrying = true;
   }

	return MSG_HANDLER_SUCCEEDED;

}
// end of msg 3011 handler
/*end   20040623*/
static int message_3012_handler( TConnection	*p_connect,
				 int		data_len,
				 bool		do_insert	)
{
/* << -------------------------------------------------------- >> */
/*			  local variables			  */
/* << -------------------------------------------------------- >> */
  char      *PosInBuff;		/* current pos in data buffer	    */
  int		ReadInput;		/* amount of Bytesread from buffer  */
  int		retcode1;		/* code returned from function	    */
  bool		stop_retrying;		/* if true - return from function   */
  int		n_retries;		/* retrying counter		    */
/* << -------------------------------------------------------- >> */
/*			    DB variables			  */
/* << -------------------------------------------------------- >> */
EXEC SQL BEGIN DECLARE SECTION;
TRashamRecord		RashamRecord;
EXEC SQL END DECLARE SECTION;


/* << -------------------------------------------------------- >> */
/*			read input buffer			  */
/* << -------------------------------------------------------- >> */
  INIT_ERROR_DETECTION_MECHANISM;
  PosInBuff = glbMsgBuffer;

  RashamRecord.MemberId.Code		=  get_short( &PosInBuff,      2);
  RETURN_ON_ERROR;

  RashamRecord.MemberId.Number	=  get_intv( &PosInBuff,    IdNumber_9_len);
  RETURN_ON_ERROR;

  RashamRecord.Contractor	=  get_intv( &PosInBuff,    IdNumber_9_len);
  RETURN_ON_ERROR;

  RashamRecord.ContractType		=  get_short( &PosInBuff,      2);
  RETURN_ON_ERROR;

  RashamRecord.KodRasham		=  get_short( &PosInBuff,      2);
  RETURN_ON_ERROR;

  get_string(   &PosInBuff,    RashamRecord.Rasham, 30);

  RashamRecord.TermId	=  get_intv( &PosInBuff,    TermId_7_len);
  RETURN_ON_ERROR;

  RashamRecord.UpdateDate	=  get_intv( &PosInBuff,    DateYYYYMMDD_len);
  RETURN_ON_ERROR;

/* << -------------------------------------------------------- >> */
/*			    debug messages			  */
/* << -------------------------------------------------------- >> */
  /*   verify all data was read:   */
  ReadInput = PosInBuff - glbMsgBuffer;
  StaticAssert( ReadInput == data_len );
/* << -------------------------------------------------------- >> */
/*	   up to SQL_UPDATE_RETRIES retries will be made	  */
/*		    to overcome access conflict			  */
/* << -------------------------------------------------------- >> */
  retcode1 = ALL_RETRIES_WERE_USED;
  stop_retrying = false;
  for( n_retries = 0;
       ( n_retries < SQL_UPDATE_RETRIES ) && ( stop_retrying == false );
       n_retries++ )
  {
      /* sleep for a while:  */
      if( n_retries )
      {
         sleep( SLEEP_TIME_BETWEEN_RETRIES );
      }
      /*  try to insert new rec with the order depending on "do_insert":      */
  		retcode1 = DbInsertRashamRecord(&RashamRecord);
		if (!retcode1)
		{
			retcode1 = DbUpdateRashamRecord(&RashamRecord);
  		}
		if( retcode1 ) stop_retrying = true;
   }
	return MSG_HANDLER_SUCCEEDED;
}
// end of msg 3012 handler
//20040811

/*20050131*/
static int message_3013_handler( TConnection	*p_connect,
				 int		data_len,
				 bool		do_insert	)
{
/* << -------------------------------------------------------- >> */
/*			  local variables			  */
/* << -------------------------------------------------------- >> */
  char      *PosInBuff;		/* current pos in data buffer	    */
  int		ReadInput;		/* amount of Bytesread from buffer  */
  int		retcode1;		/* code returned from function	    */
  int		retcode2;		/* code returned from function	    */

  bool		stop_retrying;		/* if true - return from function   */
  int		n_retries;		/* retrying counter		    */
/* << -------------------------------------------------------- >> */
/*			    DB variables			  */
/* << -------------------------------------------------------- >> */
EXEC SQL BEGIN DECLARE SECTION;
TMemberMessageRecord	MemberMessageRecord;
EXEC SQL END DECLARE SECTION;
int i;
char message_text[40];
char text1[30];//=0; Y20060724
char text2[30];//=0; Y20060724
int  blank_place=0;
text2[0]=0;
text1[0]=0;

/* << -------------------------------------------------------- >> */
/*			read input buffer			  */
/* << -------------------------------------------------------- >> */
  INIT_ERROR_DETECTION_MECHANISM;
  PosInBuff = glbMsgBuffer;
  MemberMessageRecord.MemberId.Code		=  get_short( &PosInBuff,      1);
  RETURN_ON_ERROR;

  MemberMessageRecord.MemberId.Number	=  get_intv( &PosInBuff,    IdNumber_9_len);
  RETURN_ON_ERROR;

  get_string(   &PosInBuff,    message_text, 40);
    switch_to_win_heb( ( unsigned char * )message_text ); //Yulia 20050223

for (i=0;i<30;i++)
{
text1[i] = 32;
text2[i] = 32;
}
/*find blank space for split 40 char to 2 * 30 char */
  for (i=20;i<30;i++)
  {
	  if (message_text[i] == 32)
	  {
		  strncpy(text1,message_text,i);
		  strncpy(text2,message_text+i,40-i);
		  blank_place = i;
		  text1[30]=0;
		  text2[30]=0;		  
//GerrLogFnameMini ("3013_log",GerrId,"point1[%d][%s][%s]",blank_place,text1,text2);		  
		  break;
	  }
  }
  if ( !blank_place )
  {
	  for (i=20;i>10;i--)
	  {
		  if (message_text[i] == 32)
		  {
			  strncpy(text1,message_text,20-i);
			  strncpy(text2,message_text+20-i,20+i);
			  blank_place = i;
		  text1[30]=0;
		  text2[30]=0;		  
//GerrLogFnameMini ("3013_log",GerrId,"point2[%d][%30s][%30s]",blank_place,text1,text2);		  			  
			  break;
		  }
	  }
  }
  if ( !blank_place )
  {
	  strncpy(text1,message_text,30);
	  strncpy(text2,message_text+30,10);
		  text1[30]=0;
		  text2[30]=0;		  
//GerrLogFnameMini ("3013_log",GerrId,"point3[%d][%30s][%30s]",blank_place,text1,text2);		  	  
  }
/* << -------------------------------------------------------- >> */
/*			    debug messages			  */
/* << -------------------------------------------------------- >> */
  /*   verify all data was read:   */
  ReadInput = PosInBuff - glbMsgBuffer;

  StaticAssert( ReadInput == data_len );
/* << -------------------------------------------------------- >> */
/*	   up to SQL_UPDATE_RETRIES retries will be made	  */
/*		    to overcome access conflict			  */
/* << -------------------------------------------------------- >> */
  retcode1 = ALL_RETRIES_WERE_USED;
  retcode2 = ALL_RETRIES_WERE_USED;
  /*Yulia 20061101 for all members before delete old messages and after this add*/
  DbDeleteMemberMessageRecord(&MemberMessageRecord);
  MemberMessageRecord.MessageNo = 0;
  strcpy(MemberMessageRecord.Message,	text1);
  MemberMessageRecord.LineNo	= 0;
  retcode1=	 DbInsertMemberMessageRecord(&MemberMessageRecord);
  strcpy(MemberMessageRecord.Message,	text2);
  MemberMessageRecord.LineNo	= 1;
  retcode2 = 		DbInsertMemberMessageRecord(&MemberMessageRecord);
  return (retcode2 * retcode1);
}
// end of msg 3013 handler

/*20050131 end*/

/*20050828*/
static int message_3014_handler( TConnection	*p_connect,
				 int		data_len,
				 bool		do_insert	)
{
/* << -------------------------------------------------------- >> */
/*			  local variables			  */
/* << -------------------------------------------------------- >> */
  char      *PosInBuff;		/* current pos in data buffer	    */
  int		ReadInput;		/* amount of Bytesread from buffer  */
  int		retcode1;		/* code returned from function	    */
  int		retcode2;		/* code returned from function	    */

  bool		stop_retrying;		/* if true - return from function   */
  int		n_retries;		/* retrying counter		    */
/* << -------------------------------------------------------- >> */
/*			    DB variables			  */
/* << -------------------------------------------------------- >> */
EXEC SQL BEGIN DECLARE SECTION;
TMemberId MemberId;
int 	  BirthDate;

EXEC SQL END DECLARE SECTION;

/* << -------------------------------------------------------- >> */
/*			read input buffer			  */
/* << -------------------------------------------------------- >> */
  INIT_ERROR_DETECTION_MECHANISM;
  PosInBuff = glbMsgBuffer;
  MemberId.Code		=  get_short( &PosInBuff,      1);
  RETURN_ON_ERROR;

  MemberId.Number	=  get_intv( &PosInBuff,    IdNumber_9_len);
  BirthDate			=  get_intv( &PosInBuff,    8);
  
  if (BirthDate < 1000000) 
  {
	   BirthDate = BirthDate + 19000000;
  }
  else  if (BirthDate < 10000000)
  {
	   BirthDate = BirthDate - 1000000 + 20000000;
  }

  RETURN_ON_ERROR;

/* << -------------------------------------------------------- >> */
/*			    debug messages			  */
/* << -------------------------------------------------------- >> */
  /*   verify all data was read:   */
  ReadInput = PosInBuff - glbMsgBuffer;

  StaticAssert( ReadInput == data_len );
/* << -------------------------------------------------------- >> */
/*	   up to SQL_UPDATE_RETRIES retries will be made	  */
/*		    to overcome access conflict			  */
/* << -------------------------------------------------------- >> */
  retcode1 = ALL_RETRIES_WERE_USED;

  stop_retrying = false;

  for( n_retries = 0;
       ( n_retries < SQL_UPDATE_RETRIES ) && ( stop_retrying == false );
       n_retries++ )
    {
      /* sleep for a while:  */
      if( n_retries )
      {
         sleep( SLEEP_TIME_BETWEEN_RETRIES );
      }
      /*  try to insert new rec with the order depending on "do_insert":      */

	  if ( do_insert == ATTEMPT_INSERT_FIRST )	
	 {

		retcode1=	 DbInsertMemberMammoRecord(&MemberId,BirthDate);
//		GerrLogFnameMini ("3014_log",GerrId,"insert");
// 	    if( retcode1 == 1) stop_retrying = true;

		HANDLE_INSERT_SQL_CODE(  "3014",      p_connect,
								&do_insert, &retcode1,
			 					&stop_retrying );


	 }
	 else
	 {
		retcode1 = DbUpdateMemberMammoRecord(&MemberId,BirthDate);
// 	    if( retcode1 == 1) stop_retrying = true;

  		HANDLE_UPDATE_SQL_CODE(  "3014",      p_connect,
					  &do_insert, &retcode1,
			  		  &stop_retrying );

	 }
	}


//	return (retcode1);
	return MSG_HANDLER_SUCCEEDED;

}
// end of msg 3014 handler

/*end 20050828*/



/*20050922*/
static int message_3015_handler( TConnection	*p_connect,
				 int		data_len,
				 bool		do_insert	)
{
/* << -------------------------------------------------------- >> */
/*			  local variables			  */
/* << -------------------------------------------------------- >> */
  char      *PosInBuff;		/* current pos in data buffer	    */
  int		ReadInput;		/* amount of Bytesread from buffer  */
  int		retcode;		/* code returned from function	    */
  
  bool		stop_retrying;		/* if true - return from function   */
  int		n_retries;		/* retrying counter		    */
/* << -------------------------------------------------------- >> */
/*			    DB variables			  */
/* << -------------------------------------------------------- >> */
EXEC SQL BEGIN DECLARE SECTION;
TMemberId MemberId;

EXEC SQL END DECLARE SECTION;

/* << -------------------------------------------------------- >> */
/*			read input buffer			  */
/* << -------------------------------------------------------- >> */
  INIT_ERROR_DETECTION_MECHANISM;
  PosInBuff = glbMsgBuffer;

  MemberId.Number	=  get_intv( &PosInBuff,    IdNumber_9_len);
  RETURN_ON_ERROR;

  MemberId.Code		=  get_short( &PosInBuff,      1);
  RETURN_ON_ERROR;

/* << -------------------------------------------------------- >> */
/*			    debug messages			  */
/* << -------------------------------------------------------- >> */
  /*   verify all data was read:   */
  ReadInput = PosInBuff - glbMsgBuffer;

  StaticAssert( ReadInput == data_len );
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
      /* sleep for a while:  */
      if( n_retries )
      {
         sleep( SLEEP_TIME_BETWEEN_RETRIES );
      }
      /*  try to insert new rec with the order depending on "do_insert":      */
	  


		retcode=	 DbInsertMemberSapatRecord(&MemberId);
		GerrLogFnameMini ("3015_log",GerrId,"insert[%d]",MemberId.Number);
 	    if( retcode == 1) stop_retrying = true;

//		HANDLE_INSERT_SQL_CODE(  "3015",      p_connect,
//								&do_insert, &retcode,
//			 					&stop_retrying );


	}


	return (retcode);
//	return MSG_HANDLER_SUCCEEDED;

}
// end of msg 3015 handler
static int message_3016_handler( TConnection	*p_connect,
				 int		data_len,
				 bool		do_update	)
{
/* << -------------------------------------------------------- >> */
/*			  local variables			  */
/* << -------------------------------------------------------- >> */
  char      *PosInBuff;		/* current pos in data buffer	    */
  int		ReadInput;		/* amount of Bytesread from buffer  */
  int		retcode;		/* code returned from function	    */
  
  bool		stop_retrying;		/* if true - return from function   */
  int		n_retries;		/* retrying counter		    */
/* << -------------------------------------------------------- >> */
/*			    DB variables			  */
/* << -------------------------------------------------------- >> */
EXEC SQL BEGIN DECLARE SECTION;
TMemberId MemberId;

EXEC SQL END DECLARE SECTION;

/* << -------------------------------------------------------- >> */
/*			read input buffer			  */
/* << -------------------------------------------------------- >> */
  INIT_ERROR_DETECTION_MECHANISM;
  PosInBuff = glbMsgBuffer;
  MemberId.Number	=  get_intv( &PosInBuff,    IdNumber_9_len);

  RETURN_ON_ERROR;

  MemberId.Code		=  get_short( &PosInBuff,      1);

  RETURN_ON_ERROR;

/* << -------------------------------------------------------- >> */
/*			    debug messages			  */
/* << -------------------------------------------------------- >> */
  /*   verify all data was read:   */
  ReadInput = PosInBuff - glbMsgBuffer;

  StaticAssert( ReadInput == data_len );
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
      /* sleep for a while:  */
      if( n_retries )
      {
         sleep( SLEEP_TIME_BETWEEN_RETRIES );
      }
      /*  try to insert new rec with the order depending on "do_insert":      */
	  


		retcode=	 DbUpdateMemberSapatRecord(&MemberId);
//		GerrLogFnameMini ("3015_log",GerrId,"update[%d]",MemberId.Number);
 	    if( retcode == 1) stop_retrying = true;
//		HANDLE_UPDATE_SQL_CODE(  "3016",      p_connect,
//								&do_update, &retcode,
//			 					&stop_retrying );


	}


	return (retcode);
//	return MSG_HANDLER_SUCCEEDED;

}
// end of msg 3016 handler


/*end 20050828*/


/*Yulia 20060302*/
static int message_3017_handler( TConnection	*p_connect,
				 int		data_len,
				 bool		do_insert	)
{
/* << -------------------------------------------------------- >> */
/*			  local variables			  */
/* << -------------------------------------------------------- >> */
  char      *PosInBuff;		/* current pos in data buffer	    */
  int		ReadInput;		/* amount of Bytesread from buffer  */
  int		retcode;		/* code returned from function	    */
  
  bool		stop_retrying;		/* if true - return from function   */
  int		n_retries;		/* retrying counter		    */
/* << -------------------------------------------------------- >> */
/*			    DB variables			  */
/* << -------------------------------------------------------- >> */
EXEC SQL BEGIN DECLARE SECTION;
TMemberId MemberId;
int	v_validuntil;
short 	v_remark;
EXEC SQL END DECLARE SECTION;

/* << -------------------------------------------------------- >> */
/*			read input buffer			  */
/* << -------------------------------------------------------- >> */
  INIT_ERROR_DETECTION_MECHANISM;
  PosInBuff = glbMsgBuffer;

  MemberId.Number	=  get_intv( &PosInBuff,    IdNumber_9_len);
  RETURN_ON_ERROR;

  MemberId.Code		=  get_short( &PosInBuff,      1);
  RETURN_ON_ERROR;

  v_remark			=	get_short( &PosInBuff,      4);
  RETURN_ON_ERROR;

  v_validuntil		=  get_long( &PosInBuff,   DateYYYYMMDD_len);
  RETURN_ON_ERROR;

/* << -------------------------------------------------------- >> */
/*			    debug messages			  */
/* << -------------------------------------------------------- >> */
  /*   verify all data was read:   */
  ReadInput = PosInBuff - glbMsgBuffer;

  StaticAssert( ReadInput == data_len );
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
      /* sleep for a while:  */
      if( n_retries )
      {
         sleep( SLEEP_TIME_BETWEEN_RETRIES );
      }
      /*  try to insert new rec with the order depending on "do_insert":      */
	  


		retcode=	 DbInsertMemberRemarkRecord(&MemberId,v_remark,v_validuntil);
//		GerrLogFnameMini ("3017_log",GerrId,"insert[%d]",MemberId.Number);
 	    if( retcode == 1) stop_retrying = true;

//		HANDLE_INSERT_SQL_CODE(  "3015",      p_connect,
//								&do_insert, &retcode,
//			 					&stop_retrying );


	}


	return (retcode);
//	return MSG_HANDLER_SUCCEEDED;

}
// end of msg 3017 handler

/*end Yulia 20060302*/

/*Yulia 20060302*/
static int message_3018_handler( TConnection	*p_connect,
				 int		data_len,
				 bool		do_insert	)
{
/* << -------------------------------------------------------- >> */
/*			  local variables			  */
/* << -------------------------------------------------------- >> */
  char      *PosInBuff;		/* current pos in data buffer	    */
  int		ReadInput;		/* amount of Bytesread from buffer  */
  int		retcode;		/* code returned from function	    */
  
  bool		stop_retrying;		/* if true - return from function   */
  int		n_retries;		/* retrying counter		    */
/* << -------------------------------------------------------- >> */
/*			    DB variables			  */
/* << -------------------------------------------------------- >> */
EXEC SQL BEGIN DECLARE SECTION;
TClicksTblUpdRecord ClicksTblRec;
EXEC SQL END DECLARE SECTION;

/* << -------------------------------------------------------- >> */
/*			read input buffer			  */
/* << -------------------------------------------------------- >> */
  INIT_ERROR_DETECTION_MECHANISM;
  PosInBuff = glbMsgBuffer;

  	GerrLogFnameMini ("3018_log",
					   GerrId,
					   "[%d][%s]",data_len,   glbMsgBuffer);

  ClicksTblRec.update_date	=  get_long( &PosInBuff,    DateYYYYMMDD_len);  RETURN_ON_ERROR;
  ClicksTblRec.update_time	=  get_long( &PosInBuff,    TimeHHMMSS_len);	RETURN_ON_ERROR;
  GerrLogFnameMini ("3018_log",   GerrId,"point1");
  ClicksTblRec.key	=  get_short( &PosInBuff,    Tkey_len);		RETURN_ON_ERROR;
  ClicksTblRec.action_type	=  get_short( &PosInBuff,   Taction1_type_len);  RETURN_ON_ERROR;
  ClicksTblRec.kod_tbl	=  get_long( &PosInBuff,    Tkod_tbl_len);		RETURN_ON_ERROR;
  get_string(   &PosInBuff,    ClicksTblRec.descr, Tdescr_len);			
    GerrLogFnameMini ("3018_log",   GerrId,"point2");
  switch_to_win_heb( ( unsigned char * )ClicksTblRec.descr );//Yulia 20070410
  get_string(   &PosInBuff,    ClicksTblRec.small_descr, Tsmall_descr_len);
  switch_to_win_heb( ( unsigned char * )ClicksTblRec.small_descr );//Yulia 20070410
  GerrLogFnameMini ("3018_log",   GerrId,"point3");
  get_string(   &PosInBuff,    ClicksTblRec.algoritm, Tfld_9_len);
  switch_to_win_heb( ( unsigned char * )ClicksTblRec.algoritm );//Yulia 20070410
  get_string(   &PosInBuff,    ClicksTblRec.add_fld1, Tfld_9_len);
  switch_to_win_heb( ( unsigned char * )ClicksTblRec.add_fld1 );//Yulia 20070410
  GerrLogFnameMini ("3018_log",   GerrId,"point4");
  get_string(   &PosInBuff,    ClicksTblRec.add_fld2, Tfld_4_len);
  switch_to_win_heb( ( unsigned char * )ClicksTblRec.add_fld2 );//Yulia 20070410
  get_string(   &PosInBuff,    ClicksTblRec.add_fld3, Tfld_4_len);
  switch_to_win_heb( ( unsigned char * )ClicksTblRec.add_fld3 );//Yulia 20070410
  get_string(   &PosInBuff,    ClicksTblRec.add_fld4, Tfld_4_len);
  switch_to_win_heb( ( unsigned char * )ClicksTblRec.add_fld4 );//Yulia 20070410
  GerrLogFnameMini ("3018_log",   GerrId,"point5");
  get_string(   &PosInBuff,    ClicksTblRec.add_fld5, Tfld_4_len);
  switch_to_win_heb( ( unsigned char * )ClicksTblRec.add_fld5 );//Yulia 20070410
  get_string(   &PosInBuff,    ClicksTblRec.add_fld6, Tdescr_len);
  	switch_to_win_heb( ( unsigned char * )ClicksTblRec.add_fld6 );//Yulia 20060914
  ClicksTblRec.times	=  get_short( &PosInBuff,    Ttimes_len);		RETURN_ON_ERROR;
    GerrLogFnameMini ("3018_log",   GerrId,"point6 [%d]",data_len);
/* << -------------------------------------------------------- >> */
/*			    debug messages			  */
/* << -------------------------------------------------------- >> */
  /*   verify all data was read:   */
  ReadInput = PosInBuff - glbMsgBuffer;
  GerrLogFnameMini ("3018_log",
					   GerrId,
					   "[%d]",ReadInput);

  StaticAssert( ReadInput == data_len );
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
      /* sleep for a while:  */
      if( n_retries )
      {
         sleep( SLEEP_TIME_BETWEEN_RETRIES );
      }
      /*  try to insert new rec with the order depending on "do_insert":      */
		retcode=	 DbInsertClicksTblUpdRecord(&ClicksTblRec);
		  GerrLogFnameMini ("3018_log",   GerrId,"point7[%d]",retcode);
//Y0924 	    if( retcode == 1) stop_retrying = true;

		HANDLE_INSERT_SQL_CODE(  "3018",      p_connect,
								&do_insert, &retcode,
			 					&stop_retrying );
	}
//	return (retcode);
	return MSG_HANDLER_SUCCEEDED;
}
// end of msg 3018 handler

/*end Yulia 20060525*/
/*begin 3019 20070909*/
static int message_3019_handler( TConnection	*p_connect,
				 int		data_len,
				 bool		do_insert	)
{
/* << -------------------------------------------------------- >> */
/*			  local variables			  */
/* << -------------------------------------------------------- >> */
  char      *PosInBuff;		/* current pos in data buffer	    */
  int		ReadInput;		/* amount of Bytesread from buffer  */
  int		retcode;		/* code returned from function	    */
  
  bool		stop_retrying;		/* if true - return from function   */
  int		n_retries;		/* retrying counter		    */
/* << -------------------------------------------------------- >> */
/*			    DB variables			  */
/* << -------------------------------------------------------- >> */
EXEC SQL BEGIN DECLARE SECTION;
TGastroRecord GastroRec;
EXEC SQL END DECLARE SECTION;
char status[3];
/* << -------------------------------------------------------- >> */
/*			read input buffer			  */
/* << -------------------------------------------------------- >> */
  INIT_ERROR_DETECTION_MECHANISM;
  PosInBuff = glbMsgBuffer;

  	GerrLogFnameMini ("3019_log",
					   GerrId,
					   "[%d][%s]",data_len,   glbMsgBuffer);

	GastroRec.Contractor 	=	 get_long( &PosInBuff,    IdNumber_9_len);  RETURN_ON_ERROR;
	GastroRec.TreatmentContr=	 get_long( &PosInBuff,    IdNumber_9_len);  RETURN_ON_ERROR;
	GastroRec.Location		=	 get_long( &PosInBuff,    LocationId_7_len);  RETURN_ON_ERROR;
	GastroRec.ValidFrom		=	 get_long( &PosInBuff,    DateYYYYMMDD_len);  RETURN_ON_ERROR;
	GastroRec.ValidUntil	=	 get_long( &PosInBuff,    DateYYYYMMDD_len);  RETURN_ON_ERROR;
	GastroRec.ContractType	=	 get_short( &PosInBuff,   ContractType_2_len);  RETURN_ON_ERROR;
	GastroRec.TermId		=	 get_long( &PosInBuff,    TermId_7_len);  RETURN_ON_ERROR;
	GastroRec.AuthType		=	 get_short( &PosInBuff,   2);  RETURN_ON_ERROR;
	  get_string(   &PosInBuff,    status, 2);
	GastroRec.StatusRec= atoi (status);
/* << -------------------------------------------------------- >> */
/*			    debug messages			  */
/* << -------------------------------------------------------- >> */
  /*   verify all data was read:   */
  ReadInput = PosInBuff - glbMsgBuffer;
  GerrLogFnameMini ("3019_log",
					   GerrId,
					   "[%d]",ReadInput);

  StaticAssert( ReadInput == data_len );
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
      /* sleep for a while:  */
      if( n_retries )
      {
         sleep( SLEEP_TIME_BETWEEN_RETRIES );
      }
      /*  try to insert new rec with the order depending on "do_insert":      */
     if ( do_insert == ATTEMPT_INSERT_FIRST )	
	 {
  			DbInsertGastroRecord(&GastroRec);
  			HANDLE_INSERT_SQL_CODE(  "3019",      p_connect,
						  &do_insert, &retcode,
				 		  &stop_retrying );
		}
		else
		{
  			DbUpdateGastroRecord(&GastroRec);
  			HANDLE_UPDATE_SQL_CODE(  "3019",      p_connect,
						  &do_insert, &retcode,
				  		  &stop_retrying );
		}
	}
//	return (retcode);
	return MSG_HANDLER_SUCCEEDED;

}
// end of msg 3019 handler

/*end 3019 20070909*/
// handler for message 3021 - insert to table LABNOTES  mokeds
static int message_3021_handler( TConnection	*p_connect,
				 int		data_len,
				 bool		do_insert	)
{
/* << -------------------------------------------------------- >> */
/*			  local variables			  */
/* << -------------------------------------------------------- >> */
  char          *PosInBuff;		/* current pos in data buffer	    */
  int		ReadInput;		/* amount of Bytesread from buffer  */
  int		retcode1;		/* code returned from function	    */
  bool		stop_retrying;		/* if true - return from function   */
  int		n_retries;		/* retrying counter		    */
  int		i;
  int		stam_9;
  short		stam_3;
  short		ret_labnot;
  short		ret_labnot_other;
  short		Contr_N; // 17.07.2000 1=>other,>1 all +labid
/* << -------------------------------------------------------- >> */
/*			    DB variables			  */
/* << -------------------------------------------------------- >> */
EXEC SQL BEGIN DECLARE SECTION;
TLabNoteRecord		LabNoteRec;
long int sysdate;
long int systime;
EXEC SQL END DECLARE SECTION;
  sysdate = GetDate();
  systime = GetTime();
/* << -------------------------------------------------------- >> */
/*			read input buffer			  */
/* << -------------------------------------------------------- >> */
  INIT_ERROR_DETECTION_MECHANISM;
  PosInBuff = glbMsgBuffer;
  LabNoteRec.MemberId.Code		=  get_short( &PosInBuff,  1); 
  RETURN_ON_ERROR;
  LabNoteRec.MemberId.Number	=  get_intv( &PosInBuff,   IdNumber_9_len);
  RETURN_ON_ERROR;
  LabNoteRec.ReqId			=  get_intv( &PosInBuff,     ReqId_9_len);
  RETURN_ON_ERROR;
  LabNoteRec.ReqCode			=  get_intv( &PosInBuff, ReqCode_5_len);
  RETURN_ON_ERROR;
  LabNoteRec.NoteType		=  get_short( &PosInBuff,    NoteType_2_len);
  RETURN_ON_ERROR;
  LabNoteRec.LineIndex		=  get_short( &PosInBuff,    LineIndex_2_len);
  RETURN_ON_ERROR;
  get_string(  &PosInBuff,    LabNoteRec.Note,      Note_30_len);
  LabNoteRec.Contractor	=  get_intv( &PosInBuff,    IdNumber_9_len);
  RETURN_ON_ERROR;
  Contr_N =  get_short( &PosInBuff,  2);
  RETURN_ON_ERROR;
  GerrLogFnameMini ("3021_log", GerrId,"con[%9d-%1d]id[%9d]reqid[%9d][%5d]",
			LabNoteRec.Contractor,Contr_N,
			LabNoteRec.MemberId.Number,
			LabNoteRec.ReqId,LabNoteRec.ReqCode);
  
/* << -------------------------------------------------------- >> */
/*			    debug messages			  */
/* << -------------------------------------------------------- >> */
  /*   verify all data was read: */
  ReadInput = PosInBuff - glbMsgBuffer;
  StaticAssert( ReadInput == data_len );
/* << -------------------------------------------------------- >> */
/*	   up to SQL_UPDATE_RETRIES retries will be made	          */
/*		    to overcome access conflict			                  */
/* << -------------------------------------------------------- >> */
  retcode1 = ALL_RETRIES_WERE_USED;
  stop_retrying = false;

  for( n_retries = 0;
       ( n_retries < SQL_UPDATE_RETRIES ) && ( stop_retrying == false );
       n_retries++ )
    {
      /* sleep for a while:      */
      if( n_retries )
      {
         sleep( SLEEP_TIME_BETWEEN_RETRIES );
      }
      /*
       *  try to insert new rec with the order depending on "do_insert":
       */
	  /*--------------------------------------------------------
		we couldn't check labid 23,472,473,64,65,68,513,931	    |
		because we have not it in note record DB will be        |
	    inconsistance from sunday to saturday                   |
	  ----------------------------------------------------------*/
	  if (LabNoteRec.Contractor > 0 )
	  {
	  	ret_labnot	=   DbInsertLabNoteRecord(&LabNoteRec);
	  }
	  else
	  {
		ret_labnot = 1;
	  }
	 // all first contractor to the labother table
	  if (Contr_N == 1)
	  {
		ret_labnot_other	=  DbInsertLabNoteOtherRecord(&LabNoteRec); 	
	  }
	  else
	  {
		ret_labnot_other = 1;
	  }
   retcode1 = 1- ret_labnot * ret_labnot_other;
   if( retcode1 == 0) stop_retrying = true;
   }
	return MSG_HANDLER_SUCCEEDED;
}
// end of msg 3021 handler mokeds


// handler for message 3022 - insert to table LABRESULTS mokeds
static int message_3022_handler( TConnection	*p_connect,
				 int		data_len,
				 bool		do_insert	)
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
  char 		YesNo[2];
  int		group_num_id;
  short		ret_labres;
  short		ret_labres_other;
  short		Contr_N; // 17.07.2000 1=>other,>1 all +labid
  short		Moked_Flg;
/* << -------------------------------------------------------- >> */
/*			    DB variables			  */
/* << -------------------------------------------------------- >> */
EXEC SQL BEGIN DECLARE SECTION;
TLabRecord	LabResRec;
long int sysdate;
long int systime;
EXEC SQL END DECLARE SECTION;

  sysdate = GetDate();
  systime = GetTime();

/* << -------------------------------------------------------- >> */
/*			read input buffer			  */
/* << -------------------------------------------------------- >> */
  INIT_ERROR_DETECTION_MECHANISM;
  PosInBuff = glbMsgBuffer;
  LabResRec.MemberId.Code		=  get_short( &PosInBuff,     1);
  RETURN_ON_ERROR;
  LabResRec.MemberId.Number		=  get_long( &PosInBuff,     IdNumber_9_len);
  RETURN_ON_ERROR;
  LabResRec.LabId 			=  get_short( &PosInBuff,	3);
  RETURN_ON_ERROR;
  LabResRec.Route			=  get_long( &PosInBuff,	9);
  RETURN_ON_ERROR;
  LabResRec.ReqId			=  get_long( &PosInBuff,    ReqId_9_len);
  RETURN_ON_ERROR;
  LabResRec.ReqCode			=  get_long( &PosInBuff,    ReqCode_5_len);
  RETURN_ON_ERROR;
  LabResRec.Result			=  get_long( &PosInBuff,    Result_10_len);
  RETURN_ON_ERROR;
  get_string( &PosInBuff,	LabResRec.Units,	Units_15_len);
  LabResRec.LowerLimit		=  get_long( &PosInBuff,  Limit_10_len);
  RETURN_ON_ERROR;
  LabResRec.UpperLimit		=  get_long( &PosInBuff,   Limit_10_len);
  RETURN_ON_ERROR;
/* Field PregnantWoman */  	   	get_string( &PosInBuff,	&YesNo[0],	1);
/* Field Completed */			get_string( &PosInBuff,	&YesNo[1],	1);
  LabResRec.ApprovalDate	=  get_long( &PosInBuff,    DateYYYYMMDD_len);
  RETURN_ON_ERROR;
  LabResRec.ForwardDate		=  get_long( &PosInBuff, DateYYYYMMDD_len);
  RETURN_ON_ERROR;
  LabResRec.ReqDate			=  get_long( &PosInBuff, DateYYYYMMDD_len);
  RETURN_ON_ERROR;
  LabResRec.Contractor		=  get_long( &PosInBuff,    IdNumber_9_len);
  RETURN_ON_ERROR;
  LabResRec.TermId			=  get_long( &PosInBuff,    TermId_9_len);
  RETURN_ON_ERROR;
//  if (LabResRec.TermId > 0)
//  {
//	GerrLogFnameMini ("3022_log", GerrId,"data_len [%d]lab[%d]term[%d]con[%d-%d]id[%d]",
//			data_len,
//			LabResRec.LabId,LabResRec.TermId,LabResRec.Contractor,Contr_N,
//			LabResRec.MemberId.Number);
//  }
  group_num_id			=  get_intv( &PosInBuff,   5);
  RETURN_ON_ERROR;
  Contr_N =  get_short( &PosInBuff,  2);
  RETURN_ON_ERROR;
  
	if(	YesNo[0]	==	'Y' )
	{
		LabResRec.PregnantWoman =	1;
	} else
	{
		LabResRec.PregnantWoman =	0;
	}
	if(	YesNo[1]	==	'Y' )
	{
		LabResRec.Completed =	1;
	} else
	{
		LabResRec.Completed =	0;
	}
	if ( LabResRec.ApprovalDate < 1000000 )
	{
		LabResRec.ApprovalDate	+= 19000000; 
	}
	if ( LabResRec.ForwardDate  < 1000000 )
	{
		LabResRec.ForwardDate	+= 19000000;
	}
	if ( LabResRec.ReqDate      < 1000000 )
	{
		LabResRec.ReqDate	+= 19000000;
	}
/*	StringToHeb(&LabRecord.Units,sizeof(LabRecord.Units));*/
/* << -------------------------------------------------------- >> */
/*			    debug messages			  */
/* << -------------------------------------------------------- >> */
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
  for( n_retries = 0;
       ( n_retries < SQL_UPDATE_RETRIES ) && ( stop_retrying == false );
       n_retries++ )
    {
      /* sleep for a while:    */
      if( n_retries )
      {
         sleep( SLEEP_TIME_BETWEEN_RETRIES );
      }
      /*   try to insert new rec with the order depending on "do_insert":     */
	  Moked_Flg = 0;
  
	  // 20030518 get out check if this lab moked and set flag to 1

	  if (	(LabResRec.LabId == 23)  ||
			(LabResRec.LabId == 472) ||
			(LabResRec.LabId == 473) ||
			(LabResRec.LabId == 64)  ||
			(LabResRec.LabId == 65)  ||
			(LabResRec.LabId == 68)  ||
			(LabResRec.LabId == 513) ||
			(LabResRec.LabId == 67)  ||      
			(LabResRec.LabId == 800) || 
			(LabResRec.LabId == 77)  ||  /*20051220*/
			(LabResRec.LabId == 931) )
	  {
		  Moked_Flg = 1;
	  }
	  if (LabResRec.Contractor > 0)
	  {
		if (( prev_contractor	!= LabResRec.Contractor)		||
			( prev_idnumber		!= LabResRec.MemberId.Number)	||
			( prev_idcode		!= LabResRec.MemberId.Code)
			)
		{
/* If we have moked record for contr = 2,3 we must reupdate termid*/
/* because other way any doctor in his clinik can not see results on computer*/
/* all records in this case arrived moked,not,moked,not ...sorted by reqcode */
			if ( (Moked_Flg == 1 ) && (Contr_N > 1 ) )
			{
				LabResRec.TermId = 0;
			}
			if (LabResRec.TermId == 0)
			{
   				DbGetLabTerm(&LabResRec);//20020806
			}
		}
		else
		{
			LabResRec.TermId = prev_termid_upd ;
		}
		ret_labres	=   DbInsertLabResultRecord(&LabResRec);
	  }
	  else
	  {
		ret_labres = 1;
	  }
	 // all first contractor to the labother table
	  if (Contr_N == 1)
	  {
		ret_labres_other	=   DbInsertLabResultOtherRecord(&LabResRec,group_num_id); 	
	  }
	  else
	  {
		ret_labres_other = 1;
	  }
      retcode = 1- ret_labres * ret_labres_other;
      if( retcode == 0) stop_retrying = true;
	}
    prev_contractor = LabResRec.Contractor;
    prev_idnumber	= LabResRec.MemberId.Number;
    prev_idcode		= LabResRec.MemberId.Code;
    prev_termid_upd = LabResRec.TermId;
	return MSG_HANDLER_SUCCEEDED;
}
// end of msg 3022 handler
/*begin 3023 20071014*/
static int message_3023_handler( TConnection	*p_connect,
				 int		data_len,
				 bool		do_insert	)
{
/* << -------------------------------------------------------- >> */
/*			  local variables			  */
/* << -------------------------------------------------------- >> */
  char      *PosInBuff;		/* current pos in data buffer	    */
  int		ReadInput;		/* amount of Bytesread from buffer  */
  int		retcode;		/* code returned from function	    */
  
  bool		stop_retrying;		/* if true - return from function   */
  int		n_retries;		/* retrying counter		    */
/* << -------------------------------------------------------- >> */
/*			    DB variables			  */
/* << -------------------------------------------------------- >> */
EXEC SQL BEGIN DECLARE SECTION;
TMemberContrRecord MembContrRec;
EXEC SQL END DECLARE SECTION;
char status[3];
/* << -------------------------------------------------------- >> */
/*			read input buffer			  */
/* << -------------------------------------------------------- >> */
  INIT_ERROR_DETECTION_MECHANISM;
  PosInBuff = glbMsgBuffer;

  	GerrLogFnameMini ("3023_log",  GerrId,  "[%d][%s]",data_len,   glbMsgBuffer);

	MembContrRec.Contractor 	=	 get_long( &PosInBuff,    IdNumber_9_len);  RETURN_ON_ERROR;
    MembContrRec.MemberId.Number		=  get_long( &PosInBuff,   IdNumber_9_len); RETURN_ON_ERROR;
    MembContrRec.MemberId.Code		=  get_short( &PosInBuff,	      1);  RETURN_ON_ERROR;
	MembContrRec.ContractType	=	 get_short( &PosInBuff,   ContractType_2_len);  RETURN_ON_ERROR;
	MembContrRec.Grade		=	 get_short( &PosInBuff,   2);  RETURN_ON_ERROR;
	MembContrRec.TermId		=	 get_long( &PosInBuff,    TermId_7_len);  RETURN_ON_ERROR;	
/* << -------------------------------------------------------- >> */
/*			    debug messages			  */
/* << -------------------------------------------------------- >> */
  /*   verify all data was read:   */
  ReadInput = PosInBuff - glbMsgBuffer;
 // GerrLogFnameMini ("3023_log",   GerrId,	   "[%d]",ReadInput);
  StaticAssert( ReadInput == data_len );
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
      /* sleep for a while:  */
      if( n_retries )
      {
         sleep( SLEEP_TIME_BETWEEN_RETRIES );
      }
      /*  try to insert new rec with the order depending on "do_insert":      */
     if ( do_insert == ATTEMPT_INSERT_FIRST )	
	 {
  			DbInsertMembContrRecord(&MembContrRec);
  			HANDLE_INSERT_SQL_CODE(  "3023",      p_connect,
						  &do_insert, &retcode,
				 		  &stop_retrying );
		}
		else
		{
  			DbUpdateMembContrRecord(&MembContrRec);
  			HANDLE_UPDATE_SQL_CODE(  "3023",      p_connect,
						  &do_insert, &retcode,
				  		  &stop_retrying );
		}
	}
//	return (retcode);
	return MSG_HANDLER_SUCCEEDED;

}
// end of msg 3023 handler

/*end 3023 20071014*/

/*begin3024 20080804*/
static int message_3024_handler( TConnection	*p_connect,
				 int		data_len,
				 bool		do_insert	)
{
/* << -------------------------------------------------------- >> */
/*			  local variables			  */
/* << -------------------------------------------------------- >> */
  char      *PosInBuff;		/* current pos in data buffer	    */
  int		ReadInput;		/* amount of Bytesread from buffer  */
  int		retcode;		/* code returned from function	    */
  
  bool		stop_retrying;		/* if true - return from function   */
  int		n_retries;		/* retrying counter		    */
/* << -------------------------------------------------------- >> */
/*			    DB variables			  */
/* << -------------------------------------------------------- >> */
EXEC SQL BEGIN DECLARE SECTION;
TLabAdminRecord LabAdminRec;
EXEC SQL END DECLARE SECTION;
char status[3];
/* << -------------------------------------------------------- >> */
/*			read input buffer			  */
/* << -------------------------------------------------------- >> */
  INIT_ERROR_DETECTION_MECHANISM;
  PosInBuff = glbMsgBuffer;

  	GerrLogFnameMini ("3024_log",  GerrId,  "[%d][%s]",data_len,   glbMsgBuffer);

	LabAdminRec.Contractor 	=	 get_long( &PosInBuff,    IdNumber_9_len);  RETURN_ON_ERROR;
    LabAdminRec.MemberId.Code		=  get_short( &PosInBuff,	      1);  RETURN_ON_ERROR;
    LabAdminRec.MemberId.Number		=  get_long( &PosInBuff,   IdNumber_9_len); RETURN_ON_ERROR;
	LabAdminRec.ReqIdMain	=	 get_long( &PosInBuff,   8);  RETURN_ON_ERROR;
	LabAdminRec.ReqDate		=	 get_long( &PosInBuff,   8);  RETURN_ON_ERROR;
/* << -------------------------------------------------------- >> */
/*			    debug messages			  */
/* << -------------------------------------------------------- >> */
  /*   verify all data was read:   */
  ReadInput = PosInBuff - glbMsgBuffer;
  StaticAssert( ReadInput == data_len );
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
      /* sleep for a while:  */
      if( n_retries )
      {
         sleep( SLEEP_TIME_BETWEEN_RETRIES );
      }
      /*  try to insert new rec with the order depending on "do_insert":      */
     if ( do_insert == ATTEMPT_INSERT_FIRST )	
	 {
  			DbInsertLabAdminRecord(&LabAdminRec);
  			HANDLE_INSERT_SQL_CODE(  "3024",      p_connect,
						  &do_insert, &retcode,
				 		  &stop_retrying );
		}
		else
		{
//  			DbUpdateMembContrRecord(&MembContrRec);
//  			HANDLE_UPDATE_SQL_CODE(  "3024",      p_connect,
//						  &do_insert, &retcode,
//				  		  &stop_retrying );
		}
	}
//	return (retcode);
	return MSG_HANDLER_SUCCEEDED;

}
// end of msg 3024 handler

/*begin3025 20090705*/
static int message_3025_handler( TConnection	*p_connect,
				 int		data_len,
				 bool		do_insert	)
{
/* << -------------------------------------------------------- >> */
/*			  local variables			  */
/* << -------------------------------------------------------- >> */
  char      *PosInBuff;		/* current pos in data buffer	    */
  int		ReadInput;		/* amount of Bytesread from buffer  */
  int		retcode;		/* code returned from function	    */
  
  bool		stop_retrying;		/* if true - return from function   */
  int		n_retries;		/* retrying counter		    */
/* << -------------------------------------------------------- >> */
/*			    DB variables			  */
/* << -------------------------------------------------------- >> */
EXEC SQL BEGIN DECLARE SECTION;
TCityDistance CityDistRec;
EXEC SQL END DECLARE SECTION;
/* << -------------------------------------------------------- >> */
/*			read input buffer			  */
/* << -------------------------------------------------------- >> */
  INIT_ERROR_DETECTION_MECHANISM;
  PosInBuff = glbMsgBuffer;

  	GerrLogFnameMini ("3025_log",  GerrId,  "[%d][%s]",data_len,   glbMsgBuffer);

	CityDistRec.city0 	=	 get_long( &PosInBuff,  5);  RETURN_ON_ERROR;
	CityDistRec.city1 	=	 get_long( &PosInBuff,  5);  RETURN_ON_ERROR;
	CityDistRec.distance=	 get_long( &PosInBuff,  5);  RETURN_ON_ERROR;
	CityDistRec.status 	=	 get_long( &PosInBuff,  5);  RETURN_ON_ERROR;//1 upd,2 ins
/* << -------------------------------------------------------- >> */
/*			    debug messages			  */
/* << -------------------------------------------------------- >> */
  /*   verify all data was read:   */
  ReadInput = PosInBuff - glbMsgBuffer;
  StaticAssert( ReadInput == data_len );
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
      /* sleep for a while:  */
      if( n_retries )
      {
         sleep( SLEEP_TIME_BETWEEN_RETRIES );
      }
      /*  try to insert new rec with the order depending on "do_insert":      */
  			DbUpdateCityDistRecord(&CityDistRec);
  			HANDLE_INSERT_SQL_CODE(  "3025",      p_connect,
						  &do_insert, &retcode,
				 		  &stop_retrying );
	}
//	return (retcode);
	return MSG_HANDLER_SUCCEEDED;

}
/*begin3027 20091207*/
static int message_3027_handler( TConnection	*p_connect,
				 int		data_len,
				 bool		do_insert	)
{
/* << -------------------------------------------------------- >> */
/*			  local variables			  */
/* << -------------------------------------------------------- >> */
  char      *PosInBuff;		/* current pos in data buffer	    */
  int		ReadInput;		/* amount of Bytesread from buffer  */
  int		retcode;		/* code returned from function	    */
  
  bool		stop_retrying;		/* if true - return from function   */
  int		n_retries;		/* retrying counter		    */
/* << -------------------------------------------------------- >> */
/*			    DB variables			  */
/* << -------------------------------------------------------- >> */
EXEC SQL BEGIN DECLARE SECTION;
TMembDietCnt MembDietCnt;
EXEC SQL END DECLARE SECTION;
/* << -------------------------------------------------------- >> */
/*			read input buffer			  */
/* << -------------------------------------------------------- >> */
  INIT_ERROR_DETECTION_MECHANISM;
  PosInBuff = glbMsgBuffer;

  	GerrLogFnameMini ("3027_log",  GerrId,  "[%d][%s]",data_len,   glbMsgBuffer);

	MembDietCnt.MemberId.Code 	=	 get_short( &PosInBuff,  1);  RETURN_ON_ERROR;
	MembDietCnt.MemberId.Number=	 get_long( &PosInBuff,  9);  RETURN_ON_ERROR;
	MembDietCnt.ValidFrom 		=	 get_long( &PosInBuff,  8);  RETURN_ON_ERROR;
	MembDietCnt.ValidUntil 		=	 get_long( &PosInBuff,  8);  RETURN_ON_ERROR;
	MembDietCnt.DietCnt	 	=	 get_long( &PosInBuff,  5);  RETURN_ON_ERROR;//1 upd,2 ins
/* << -------------------------------------------------------- >> */
/*			    debug messages			  */
/* << -------------------------------------------------------- >> */
  /*   verify all data was read:   */
  ReadInput = PosInBuff - glbMsgBuffer;
  StaticAssert( ReadInput == data_len );
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
      /* sleep for a while:  */
      if( n_retries )
      {
         sleep( SLEEP_TIME_BETWEEN_RETRIES );
      }
      /*  try to insert new rec with the order depending on "do_insert":      */
  			DbUpdateMembDietCntRecord(&MembDietCnt);
  			HANDLE_INSERT_SQL_CODE(  "3027",      p_connect,
						  &do_insert, &retcode,
				 		  &stop_retrying );
	}
//	return (retcode);
	return MSG_HANDLER_SUCCEEDED;

}

static int message_3026_handler( TConnection	*p_connect,
				 int		data_len,
				 bool		do_insert	)
{
/* << -------------------------------------------------------- >> */
/*			  local variables			  */
/* << -------------------------------------------------------- >> */
  char      *PosInBuff;		/* current pos in data buffer	    */
  int		ReadInput;		/* amount of Bytesread from buffer  */
  int		retcode;		/* code returned from function	    */
  
  bool		stop_retrying;		/* if true - return from function   */
  int		n_retries;		/* retrying counter		    */
/* << -------------------------------------------------------- >> */
/*			    DB variables			  */
/* << -------------------------------------------------------- >> */
EXEC SQL BEGIN DECLARE SECTION;
TPsicStat PsicStatRec;
EXEC SQL END DECLARE SECTION;
/* << -------------------------------------------------------- >> */
/*			read input buffer			  */
/* << -------------------------------------------------------- >> */
  INIT_ERROR_DETECTION_MECHANISM;
  PosInBuff = glbMsgBuffer;

  	GerrLogFnameMini ("3026_log",  GerrId,  "[%d][%s]",data_len,   glbMsgBuffer);

	PsicStatRec.MemberId.Code 	=	 get_short( &PosInBuff,  1);  RETURN_ON_ERROR;
	PsicStatRec.MemberId.Number=	 get_long( &PosInBuff,  9);  RETURN_ON_ERROR;
	PsicStatRec.Contractor 		=	 get_long( &PosInBuff,  9);  RETURN_ON_ERROR;
	PsicStatRec.PsicStatus	 	=	 get_short( &PosInBuff, 1);  RETURN_ON_ERROR;//1 upd,2 ins
/* << -------------------------------------------------------- >> */
/*			    debug messages			  */
/* << -------------------------------------------------------- >> */
  /*   verify all data was read:   */
  ReadInput = PosInBuff - glbMsgBuffer;
  StaticAssert( ReadInput == data_len );
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
      /* sleep for a while:  */
      if( n_retries )
      {
         sleep( SLEEP_TIME_BETWEEN_RETRIES );
      }
      /*  try to insert new rec with the order depending on "do_insert":      */
  			DbInsertPsicStatRecord(&PsicStatRec);
  			HANDLE_INSERT_SQL_CODE(  "3026",      p_connect,
						  &do_insert, &retcode,
				 		  &stop_retrying );
	}
//	return (retcode);
	return MSG_HANDLER_SUCCEEDED;

}

//Yulia 20121227 Contract
static int message_3031_handler( TConnection	*p_connect,
				int		data_len,
				bool		do_update)
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
  int		ret_contract;	/* code returned from contract insert */

/* << -------------------------------------------------------- >> */
/*			    DB variables			  */
/* << -------------------------------------------------------- >> */
EXEC SQL BEGIN DECLARE SECTION;
long int sysdate;
long int systime;
	TContractRecord 	ContractRecord;
	int FlagAction ;
	int taxcode;
EXEC SQL END DECLARE SECTION;

  sysdate = GetDate();
  systime = GetTime();
  ret_contract = 1;
/* << -------------------------------------------------------- >> */
/*			read input buffer			  */
/* << -------------------------------------------------------- >> */
  INIT_ERROR_DETECTION_MECHANISM;
  PosInBuff = glbMsgBuffer;
  ContractRecord.Contractor = 		get_long( &PosInBuff,   IdNumber_9_len);  		RETURN_ON_ERROR;
  ContractRecord.ContractType	=	get_short ( &PosInBuff,	ContractType_2_len);  	RETURN_ON_ERROR;
  get_string( &PosInBuff,    ContractRecord.FirstName,      FirstName_8_len);
  get_string( &PosInBuff,    ContractRecord.LastName,      LastName_14_len);
  get_string( &PosInBuff,    ContractRecord.Street,	      Street_16_len);
  get_string( &PosInBuff,    ContractRecord.City,	      City_20_len);
  get_string( &PosInBuff,    ContractRecord.Phone,	      Phone_10_len);
  ContractRecord.Mahoz = get_short( &PosInBuff , Mahoz_len );  RETURN_ON_ERROR;
  ContractRecord.Profession =  get_short( &PosInBuff, 2);	  RETURN_ON_ERROR;
  ContractRecord.AcceptUrgent =  get_short( &PosInBuff, 1);	  RETURN_ON_ERROR;
  ContractRecord.SexesAllowed =  get_short( &PosInBuff, 1);	  RETURN_ON_ERROR;
  ContractRecord.AcceptAnyAge =  get_short( &PosInBuff, 1);	  RETURN_ON_ERROR;
  ContractRecord.AllowKbdEntry =  get_short( &PosInBuff, 3);  RETURN_ON_ERROR;
  ContractRecord.NoShifts =  get_short( &PosInBuff, 1);		  RETURN_ON_ERROR;
  ContractRecord.IgnorePrevAuth =  get_short( &PosInBuff, 1); RETURN_ON_ERROR;
  ContractRecord.AuthorizeAlways =  get_short( &PosInBuff, 1);RETURN_ON_ERROR;
  ContractRecord.NoAuthRecord =  get_short( &PosInBuff, 1);	  RETURN_ON_ERROR;
  ContractRecord.prof2 = get_short( &PosInBuff,2);RETURN_ON_ERROR;
  ContractRecord.prof3 = get_short( &PosInBuff,2);RETURN_ON_ERROR;
  ContractRecord.prof4 = get_short( &PosInBuff,2);RETURN_ON_ERROR;
  ContractRecord.TaxValue = get_short( &PosInBuff,2);RETURN_ON_ERROR;
  FlagAction = get_short( &PosInBuff,2);RETURN_ON_ERROR;
  
  /*   verify all data was read:  */
    ReadInput = PosInBuff - glbMsgBuffer;
  	StaticAssert( ReadInput == data_len );
/* << -------------------------------------------------------- >> */
/*	   up to SQL_UPDATE_RETRIES retries will be made	  */
/*		    to overcome access conflict			  */
/* << -------------------------------------------------------- >> */
  retcode = ALL_RETRIES_WERE_USED;
  stop_retrying = false;
  for( n_retries = 0;( n_retries < SQL_UPDATE_RETRIES ) && ( stop_retrying == false ); n_retries++ )
    {
	  ret_contract = 1;
      if( n_retries )
      {
//         sleep( SLEEP_TIME_BETWEEN_RETRIES );
      }
	ContractRecord.LastUpdate.DbDate 	= sysdate;
	ContractRecord.LastUpdate.DbTime 	= systime;
	ContractRecord.UpdatedBy = 8888 ;
	switch (FlagAction)
		{
		case 10:
		//insert 
			ret_contract = DbInsertContractNewRecord(	&ContractRecord );
//			GerrLogFnameMini ("3031_log",GerrId,"after insert1:[%d] [%9d-%2d]",	ret_contract,
//				ContractRecord.Contractor,ContractRecord.ContractType	);

			if (ret_contract == 2) /**problem to insert new record, exist now */
			{
//				GerrLogFnameMini ("3031_log",GerrId,"record exist must update:[%d] [%9d-%2d]",	ret_contract,
//				ContractRecord.Contractor,ContractRecord.ContractType	);
				ret_contract = 1;
				ret_contract = DbUpdateContractNewRecord(	ContractRecord.Contractor,
								ContractRecord.ContractType,&ContractRecord) ;
//			GerrLogFnameMini ("3031_log",GerrId,"after update1:[%d] [%9d-%2d]",	ret_contract,
//				ContractRecord.Contractor,ContractRecord.ContractType	);

			}
			break;
		case 20:
		//update
			ret_contract = 	DbUpdateContractNewRecord(	ContractRecord.Contractor,
							ContractRecord.ContractType,&ContractRecord) ;
//			GerrLogFnameMini ("3031_log",GerrId,"after update2:[%d] [%9d-%2d-%9d]",	ret_contract,
//				ContractRecord.Contractor,ContractRecord.ContractType	);


			if (ret_contract == 3 )
			{
				ret_contract = 	DbInsertContractNewRecord(	&ContractRecord );
//			GerrLogFnameMini ("3031_log",GerrId,"after insert2:[%d] [%9d-%2d]",	ret_contract,
//				ContractRecord.Contractor,ContractRecord.ContractType	);
			}

							
			break;
		case 30:
		//delete check if function exist
			ret_contract = 	DbDeleteContractNewRecord(	ContractRecord.Contractor,
													ContractRecord.ContractType) ;
			break;
		default:
			break;
		}
		if (ret_contract == 2) /**problem to insert new record, exist now */
		{
			FlagAction = 20;
//			GerrLogFnameMini ("3031_log",GerrId," contract:[%d] [%9d-%1d]",	ret_contract,
//				ContractRecord.Contractor,ContractRecord.ContractType	);
		}
   if(ret_contract == 1) stop_retrying = true;
   }
     
   if( retcode == 0) stop_retrying = true;
//GerrLogFnameMini ("3031_log",GerrId," contract:[%d] [%9d-%1d]",	ret_contract,
//		ContractRecord.Contractor,ContractRecord.ContractType	);
       return 1; 
}
// end of msg 3031 handler
//Yulia 20130107 Conterm
static int message_3032_handler( TConnection	*p_connect,
				int		data_len,
				bool		do_update)
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
  int		ret_conterm;	/* code returned from conterm action */

/* << -------------------------------------------------------- >> */
/*			    DB variables			  */
/* << -------------------------------------------------------- >> */
EXEC SQL BEGIN DECLARE SECTION;
	long int sysdate;
	long int systime;
	TContermRecord 	ContermRecord;
	int FlagAction ;
EXEC SQL END DECLARE SECTION;

  sysdate = GetDate();
  systime = GetTime();
  ret_conterm = 1;
/* << -------------------------------------------------------- >> */
/*			read input buffer			  */
/* << -------------------------------------------------------- >> */
  INIT_ERROR_DETECTION_MECHANISM;
  PosInBuff = glbMsgBuffer;
  ContermRecord.TermId = get_long( &PosInBuff, TermId_7_len); RETURN_ON_ERROR;
  ContermRecord.Contractor = 		get_long( &PosInBuff,   IdNumber_9_len);  		RETURN_ON_ERROR;
  ContermRecord.ContractType	=	get_short ( &PosInBuff,	ContractType_2_len);  	RETURN_ON_ERROR;
  ContermRecord.AuthType = get_short( &PosInBuff,1);		  RETURN_ON_ERROR;
  ContermRecord.ValidFrom = get_long( &PosInBuff,DateYYYYMMDD_len);  RETURN_ON_ERROR;
  ContermRecord.ValidUntil = get_long( &PosInBuff,DateYYYYMMDD_len); RETURN_ON_ERROR;
  ContermRecord.Location = get_long( &PosInBuff,LocationId_7_len);	 RETURN_ON_ERROR;
  ContermRecord.Mahoz = get_short( &PosInBuff , Mahoz_len );		 RETURN_ON_ERROR;
  ContermRecord.Snif = get_long( &PosInBuff,5);				  RETURN_ON_ERROR;
  ContermRecord.Locationprv = get_long( &PosInBuff,LocationId_7_len);  RETURN_ON_ERROR;
  get_string( &PosInBuff,ContermRecord.Position,    	1);
  get_string( &PosInBuff,ContermRecord.Paymenttype,    	1);
  ContermRecord.ServiceType = get_short( &PosInBuff,3);  RETURN_ON_ERROR;
  ContermRecord.City = get_long( &PosInBuff,9);  RETURN_ON_ERROR;
  FlagAction = get_short( &PosInBuff,2);RETURN_ON_ERROR;
  /*   verify all data was read:  */
    ReadInput = PosInBuff - glbMsgBuffer;
  	StaticAssert( ReadInput == data_len );
/* << -------------------------------------------------------- >> */
/*	   up to SQL_UPDATE_RETRIES retries will be made	  */
/*		    to overcome access conflict			  */
/* << -------------------------------------------------------- >> */
  retcode = ALL_RETRIES_WERE_USED;
  stop_retrying = false;
  for( n_retries = 0;( n_retries < SQL_UPDATE_RETRIES ) && ( stop_retrying == false ); n_retries++ )
    {
	  ret_conterm = 1;
	ContermRecord.LastUpdate.DbDate 	= sysdate;
	ContermRecord.LastUpdate.DbTime 	= systime;	
	ContermRecord.UpdatedBy = 8888 ;
	switch (FlagAction)
	{
		case 10:
		//insert 
			ret_conterm = DbInsertContermNewRecord(	&ContermRecord );
			GerrLogFnameMini ("3032_log",GerrId,"after insert1:[%d] [%9d-%2d-%9d]",	ret_conterm,
				ContermRecord.Contractor,ContermRecord.ContractType,ContermRecord.TermId	);
			if (ret_conterm == 2) /**problem to insert new record, exist now */
			{
				GerrLogFnameMini ("3032_log",GerrId,"record exist must update:[%d] [%9d-%2d-%9d]",	ret_conterm,
				ContermRecord.Contractor,ContermRecord.ContractType,ContermRecord.TermId	);
				ret_conterm = 1;
				ret_conterm = DbUpdateContermNewRecord(	ContermRecord.Contractor,
								ContermRecord.ContractType,ContermRecord.TermId,
								&ContermRecord) ;
			GerrLogFnameMini ("3032_log",GerrId,"after update1:[%d] [%9d-%2d-%9d]",	ret_conterm,
				ContermRecord.Contractor,ContermRecord.ContractType,ContermRecord.TermId	);

			}
			break;
		case 20:
		//update
			ret_conterm = 	DbUpdateContermNewRecord(	ContermRecord.Contractor,
							ContermRecord.ContractType,ContermRecord.TermId,
							&ContermRecord) ;
GerrLogFnameMini ("3032_log",GerrId,"after update2:[%d] [%9d-%2d-%9d]",	ret_conterm,
				ContermRecord.Contractor,ContermRecord.ContractType,ContermRecord.TermId	);							
			if (ret_conterm == 3 )
			{
				ret_conterm = 	DbInsertContermNewRecord(	&ContermRecord );
GerrLogFnameMini ("3032_log",GerrId,"after insert2:[%d] [%9d-%2d-%9d]",	ret_conterm,
				ContermRecord.Contractor,ContermRecord.ContractType,ContermRecord.TermId	);
				GerrLogFnameMini ("3032_log",
				GerrId,"insert after update [%9d-%9d-%7d]",
				ContermRecord.Contractor,
				ContermRecord.ContractType,ContermRecord.TermId);
			}
			break;
		case 30:
		//delete check if function exist
			ret_conterm = 	DbDeleteContermNewRecord(	ContermRecord.Contractor,
							ContermRecord.ContractType,ContermRecord.TermId) ;
			break;
		default:
			break;
		}
		if(ret_conterm == 1) stop_retrying = true;
GerrLogFnameMini ("3032_log",GerrId,"after check:[%d] [%9d-%2d-%9d]",	ret_conterm,
				ContermRecord.Contractor,ContermRecord.ContractType,ContermRecord.TermId	);		
   }
   if( retcode == 0) stop_retrying = true;
       return 1; 
}
// end of msg 3032 handler

static int message_3033_handler( TConnection	*p_connect,
				int		data_len,
				bool		do_update)
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
  int		ret_terminal;	/* code returned from terminal insert */
  int   	FlagAction ;
/* << -------------------------------------------------------- >> */
/*			    DB variables			  */
/* << -------------------------------------------------------- >> */
EXEC SQL BEGIN DECLARE SECTION;
	long int sysdate;
	long int systime;
	TTerminalRecord 	TerminalRecord;
EXEC SQL END DECLARE SECTION;

  sysdate = GetDate();
  systime = GetTime();
  ret_terminal = 1;
/* << -------------------------------------------------------- >> */
/*			read input buffer			  */
/* << -------------------------------------------------------- >> */
  INIT_ERROR_DETECTION_MECHANISM;
  PosInBuff = glbMsgBuffer;
  TerminalRecord.TermId = get_long( &PosInBuff, TermId_7_len); RETURN_ON_ERROR;
  get_string( &PosInBuff,    TerminalRecord.Descr,	      38);
  get_string( &PosInBuff,    TerminalRecord.Street,	      Street_16_len);
  get_string( &PosInBuff,    TerminalRecord.City,	      City_20_len);
  get_string( &PosInBuff,    TerminalRecord.LocPhone,     Phone_10_len);
  FlagAction = get_short( &PosInBuff,2);RETURN_ON_ERROR;
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

  for( n_retries = 0;
       ( n_retries < SQL_UPDATE_RETRIES ) && ( stop_retrying == false );
       n_retries++ )
    {
  ret_terminal = 1;
      /*
       * sleep for a while:
       */
      if( n_retries )
      {
//         sleep( SLEEP_TIME_BETWEEN_RETRIES );
      }
		TerminalRecord.LastUpdate.DbDate 	= sysdate;
		TerminalRecord.LastUpdate.DbTime 	= systime;	
		TerminalRecord.UpdatedBy = 8888 ;
		TerminalRecord.AreaCode			= 0;
		TerminalRecord.TermType			= 1;
		strcpy(TerminalRecord.ModemPrefix," ");
		TerminalRecord.Phone[0].Prefix 	= 0;
		TerminalRecord.Phone[0].Number 	= 0;
		TerminalRecord.Phone[1].Prefix 	= 0;
		TerminalRecord.Phone[1].Number 	= 0;
		TerminalRecord.Phone[2].Prefix 	= 0;
		TerminalRecord.Phone[2].Number 	= 0;
		TerminalRecord.StartupTime.DbDate= sysdate;
		TerminalRecord.StartupTime.DbTime= systime;
		TerminalRecord.SoftwareVer	= 0;
		TerminalRecord.HardwareVer	= 0;
	
	switch (FlagAction)
		{
		case 10: 		//insert 
			ret_terminal = DbInsertTerminalNewRecord(	&TerminalRecord );
			break;
		case 20: 		//update
			ret_terminal = 	DbUpdateTerminalNewRecord(	&TerminalRecord) ;
//			if (ret_terminal == 2 )
//			{
//				ret_terminal = 	DbInsertTerminalNewRecord(	&TerminalRecord );
//				GerrLogFnameMini ("3033_log",
//				GerrId,"  insert after update [%9d]",
//				TerminalRecord.TermId);
//			}
			
			break;
		case 30: 		//delete check if function exist
			ret_terminal = 	DbDeleteTerminalNewRecord(	&TerminalRecord) ;
			break;
		default:
			break;
		}
		if (ret_terminal == 2) /**problem to insert new record, exist now */
		{
			FlagAction = 20;
			GerrLogMini (GerrId," message_3033_handler: terminal -239:[%d][%9d]",	ret_terminal,TerminalRecord.TermId);
		}
   	}
   	if( retcode == 0) stop_retrying = true;
//	GerrLogMini (GerrId," terminal:[%d] [%7d]",	ret_terminal,TerminalRecord.TermId	);
       return 1; 
}//end 3033
static int message_3034_handler( TConnection	*p_connect,
								int				data_len,
								bool			do_update)
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
  int		ret_subst;	/* code returned from subst insert */
  int		ret_subst_inv;	/* code returned from subst insert invert */
  int   	FlagAction ;
/* << -------------------------------------------------------- >> */
/*			    DB variables			  */
/* << -------------------------------------------------------- >> */
EXEC SQL BEGIN DECLARE SECTION;
long int sysdate;
long int systime;
TSubstRecord 		SubstRecord;
EXEC SQL END DECLARE SECTION;

  ret_subst = 1;
/* << -------------------------------------------------------- >> */
/*			read input buffer			  */
/* << -------------------------------------------------------- >> */
  INIT_ERROR_DETECTION_MECHANISM;
  PosInBuff = glbMsgBuffer;
 sysdate=GetDate();
 systime=GetTime();
 SubstRecord.Contractor = 		get_long( &PosInBuff,   IdNumber_9_len);  		RETURN_ON_ERROR;
 SubstRecord.ContractType	=	get_short ( &PosInBuff,	ContractType_2_len);  	RETURN_ON_ERROR;
 SubstRecord.AltContractor =  get_long( &PosInBuff, IdNumber_9_len); RETURN_ON_ERROR;
 SubstRecord.SubstType = get_short( &PosInBuff,2);  RETURN_ON_ERROR;
 SubstRecord.DateFrom = get_long( &PosInBuff,DateYYYYMMDD_len);  RETURN_ON_ERROR;
 SubstRecord.DateUntil = get_long( &PosInBuff,DateYYYYMMDD_len); RETURN_ON_ERROR;
 SubstRecord.SubstReason = get_short( &PosInBuff,2);  RETURN_ON_ERROR;
 FlagAction = get_short( &PosInBuff,2);RETURN_ON_ERROR;
  if (SubstRecord.DateFrom  && SubstRecord.DateFrom < 1000000)
	SubstRecord.DateFrom = SubstRecord.DateFrom + 19000000;
  if (SubstRecord.DateUntil && SubstRecord.DateUntil < 1000000)
	SubstRecord.DateUntil = SubstRecord.DateUntil + 19000000;
  ReadInput = PosInBuff - glbMsgBuffer;
  StaticAssert( ReadInput == data_len );

  retcode = ALL_RETRIES_WERE_USED;
  stop_retrying = false;

  for( n_retries = 0;
       ( n_retries < SQL_UPDATE_RETRIES ) && ( stop_retrying == false );
       n_retries++ )
  {
  	ret_subst = 1;
   	SubstRecord.LastUpdate.DbDate 	= sysdate;
	SubstRecord.LastUpdate.DbTime  	= systime;
	SubstRecord.UpdatedBy 			= 8888 ;

	switch (FlagAction)
	{
		case 10: 		//insert 
			ret_subst = 	DbInsertSubstNewRecord(	&SubstRecord );
			if (ret_subst == 2) /**problem to insert new record, exist now */
			{
				GerrLogFnameMini ("3034_log",GerrId,"record exist must update:[%d] [%9d-%2d-%9d]",	ret_subst,SubstRecord.Contractor,
				SubstRecord.ContractType,	SubstRecord.AltContractor);
				ret_subst = 1;
				ret_subst = 	DbUpdateSubstNewRecord(	&SubstRecord) ;		
			}
			break;
		case 20: 		//update
			ret_subst = 	DbUpdateSubstNewRecord(	&SubstRecord) ;
			if (ret_subst == 3 )
			{
				ret_subst = 	DbInsertSubstNewRecord(	&SubstRecord );
				GerrLogFnameMini ("3034_log",
				GerrId,"insert after update [%9d-%9d-%2d]",
				SubstRecord.Contractor, 
				SubstRecord.AltContractor,
				SubstRecord.ContractType);
			}
			break;
		case 30: 		//delete check if function exist
			GerrLogFnameMini ("3034_log",GerrId,"  delete [%9d-%9d-%2d]",
			SubstRecord.Contractor, SubstRecord.AltContractor,SubstRecord.ContractType);
			ret_subst = 	DbDeleteSubstNewRecord(	&SubstRecord) ;
			break;
		default:
			break;
		}
	if (ret_subst == 1) stop_retrying = true;		
	}

   	if( retcode == 0) stop_retrying = true;
    return 1; 
}//end 3034
static int message_3035_handler( TConnection	*p_connect,
				int		data_len,
				bool		do_update)
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
  int		ret_contract;	/* code returned from contract insert */

/* << -------------------------------------------------------- >> */
/*			    DB variables			  */
/* << -------------------------------------------------------- >> */
EXEC SQL BEGIN DECLARE SECTION;
long int sysdate;
long int systime;
	TContractNewRecord 	ContractNewRecord;
	int FlagAction ;
	int taxcode;
EXEC SQL END DECLARE SECTION;

  sysdate = GetDate();
  systime = GetTime();
  ret_contract = 1;
/* << -------------------------------------------------------- >> */
/*			read input buffer			  */
/* << -------------------------------------------------------- >> */
  INIT_ERROR_DETECTION_MECHANISM;
  PosInBuff = glbMsgBuffer;
  ContractNewRecord.Contractor = 		get_long( &PosInBuff,   IdNumber_9_len);  		RETURN_ON_ERROR;
  ContractNewRecord.ContractType	=	get_short ( &PosInBuff,	ContractType_2_len);  	RETURN_ON_ERROR;
  get_string( &PosInBuff,    ContractNewRecord.FirstName,      FirstName_8_len);
  get_string( &PosInBuff,    ContractNewRecord.LastName,      LastName_14_len);
  get_string( &PosInBuff,    ContractNewRecord.Street,	      Street_16_len);
  get_string( &PosInBuff,    ContractNewRecord.City,	      City_20_len);
  get_string( &PosInBuff,    ContractNewRecord.Phone,	      Phone_10_len);
  ContractNewRecord.Mahoz = get_short( &PosInBuff , Mahoz_len );  RETURN_ON_ERROR;
  ContractNewRecord.Profession =  get_short( &PosInBuff, 2);	  RETURN_ON_ERROR;
  ContractNewRecord.AcceptUrgent =  get_short( &PosInBuff, 1);	  RETURN_ON_ERROR;
  ContractNewRecord.SexesAllowed =  get_short( &PosInBuff, 1);	  RETURN_ON_ERROR;
  ContractNewRecord.AcceptAnyAge =  get_short( &PosInBuff, 1);	  RETURN_ON_ERROR;
  ContractNewRecord.AllowKbdEntry =  get_short( &PosInBuff, 3);  RETURN_ON_ERROR;
  ContractNewRecord.NoShifts =  get_short( &PosInBuff, 1);		  RETURN_ON_ERROR;
  ContractNewRecord.IgnorePrevAuth =  get_short( &PosInBuff, 1); RETURN_ON_ERROR;
  ContractNewRecord.AuthorizeAlways =  get_short( &PosInBuff, 1);RETURN_ON_ERROR;
  ContractNewRecord.NoAuthRecord =  get_short( &PosInBuff, 1);	  RETURN_ON_ERROR;
  ContractNewRecord.prof2 = get_short( &PosInBuff,2);RETURN_ON_ERROR;
  ContractNewRecord.prof3 = get_short( &PosInBuff,2);RETURN_ON_ERROR;
  ContractNewRecord.prof4 = get_short( &PosInBuff,2);RETURN_ON_ERROR;
  ContractNewRecord.TaxValue = get_short( &PosInBuff,2);RETURN_ON_ERROR;
  FlagAction = get_short( &PosInBuff,2);RETURN_ON_ERROR;
  ContractNewRecord.TermId = 		get_long( &PosInBuff, TermId_7_len);  		RETURN_ON_ERROR;  
  /*   verify all data was read:  */
    ReadInput = PosInBuff - glbMsgBuffer;
  	StaticAssert( ReadInput == data_len );
/* << -------------------------------------------------------- >> */
/*	   up to SQL_UPDATE_RETRIES retries will be made	  */
/*		    to overcome access conflict			  */
/* << -------------------------------------------------------- >> */
  retcode = ALL_RETRIES_WERE_USED;
  stop_retrying = false;
  for( n_retries = 0;( n_retries < SQL_UPDATE_RETRIES ) && ( stop_retrying == false ); n_retries++ )
    {
	  ret_contract = 1;
      if( n_retries )
      {
//         sleep( SLEEP_TIME_BETWEEN_RETRIES );
      }
	ContractNewRecord.LastUpdate.DbDate 	= sysdate;
	ContractNewRecord.LastUpdate.DbTime 	= systime;
	ContractNewRecord.UpdatedBy = 8888 ;
	switch (FlagAction)
		{
		case 10:
		//insert 
			ret_contract = DbInsertContractNewNewRecord(	&ContractNewRecord );
			break;
		case 20:
		//update
			ret_contract = 	DbUpdateContractNewNewRecord(	ContractNewRecord.Contractor,
							ContractNewRecord.ContractType,
							ContractNewRecord.TermId ,&ContractNewRecord) ;
			break;
		case 30:
		//delete check if function exist
	/*		ret_contract = 	DbDeleteContractNewNewRecord(	ContractNewRecord.Contractor,
													ContractNewRecord.ContractType,ContractNewRecord.TermId ) ;
		*/	break;
		default:
			break;
		}
		if (ret_contract == 2) /**problem to insert new record, exist now */
		{
			FlagAction = 20;
			GerrLogMini (GerrId," contractnew:[%d] [%9d-%1d]",	ret_contract,
				ContractNewRecord.Contractor,ContractNewRecord.ContractType	);
		}
   }
   if( retcode == 0) stop_retrying = true;
       return 1; 
}
// end of msg 3035 handler

/*===============================================================
||																||
||			call_msg_handler()									||
||																||
 ===============================================================*/
static int call_msg_handler(
			    TSystemMsg		msg,
			    TConnection		*p_connect,
			    int			len
			    )
{
    int			retcode;
    static TShmUpdate	shm_table_to_update = NONE;

/*===============================================================
||		switch over message type & handle it		||
 ===============================================================*/
    switch( msg )
    {
	//
	//   RECEIVE MESSAGES -> move data from remote to local DB
	//////////////////////////////////////////////////////////
	//

	case 3021:  /* Yulia add mokeds 20070213*/
	  retcode = message_3021_handler(p_connect, len, ATTEMPT_INSERT_FIRST);
//	  shm_table_to_update = LABNOTES;
	  shm_table_to_update = NONE;
	  break ;

	case 3022:  /* Yulia add mokeds 20070213*/
	  retcode = message_3022_handler(p_connect, len, ATTEMPT_INSERT_FIRST);
//	  shm_table_to_update = LABRESULTS;
	  shm_table_to_update = NONE;
	  break ;

	case 3003:
	  retcode = message_3003_handler(p_connect, len, ATTEMPT_INSERT_FIRST);
	  shm_table_to_update = TEMPMEMB;
	  break ;

	case 3004:
	  retcode = message_3004_handler(p_connect, len );
	  shm_table_to_update = TEMPMEMB;
	  break ;

	case 3005:
	  retcode = message_3005_handler(p_connect, len, ATTEMPT_UPDATE_FIRST);
	  shm_table_to_update = AUTH;
	  break ;
	case 3006:
	  retcode = message_3006_handler(p_connect, len, ATTEMPT_INSERT_FIRST);
//	  shm_table_to_update = NONE;
	  shm_table_to_update = CONTRACT;
	  break ;

	case 3007:
	  retcode = message_3007_handler(p_connect, len);
//	  shm_table_to_update = NONE;
	  shm_table_to_update = CONTERMDEL;
	  break ;

	case 3008:
	  retcode = message_3008_handler(p_connect, len, ATTEMPT_INSERT_FIRST);
//	  printf("\n 3008 retcode[%d][%d]",retcode,MSG_HANDLER_SUCCEEDED);fflush(stdout);
	  shm_table_to_update = HOSPITALMSG1;
	  break ;

	case 3009:
	  retcode = message_3009_handler(p_connect, len, ATTEMPT_INSERT_FIRST);
//	  printf("\n 3009 retcode[%d][%d]",retcode,MSG_HANDLER_SUCCEEDED);fflush(stdout);
	  shm_table_to_update = CONMEMB;
	  break ;

	case 3010:
	  retcode = message_3010_handler(p_connect, len, ATTEMPT_INSERT_FIRST);
//	  GerrLogFnameMini ("3010_log",  GerrId,"1",glbMsgBuffer);
	  shm_table_to_update = HOSPITALMSG2;
	  break ;

	case 3011:
	  retcode = message_3011_handler(p_connect, len, ATTEMPT_INSERT_FIRST);
//	  printf("\n 3009 retcode[%d][%d]",retcode,MSG_HANDLER_SUCCEEDED);fflush(stdout);
	  shm_table_to_update = SPEECHCLIENT;
	  break ;

	case 3012:
//	  GerrLogFnameMini ("3012_log",  GerrId,"1[%s]",glbMsgBuffer);
		retcode = message_3012_handler(p_connect, len, ATTEMPT_INSERT_FIRST);
	  shm_table_to_update = RASHAM;
	  break ;

	case 3013:
//	  GerrLogFnameMini ("3013_log",  GerrId,"1[%s]",glbMsgBuffer);
//		retcode = message_3013_handler(p_connect, len, ATTEMPT_INSERT_FIRST);
		retcode = message_3013_handler(p_connect, len, ATTEMPT_UPDATE_FIRST);
//	  shm_table_to_update = MEMB_MESS;
	  shm_table_to_update = NONE;
	  break ;

	case 3014:
//		retcode = message_3014_handler(p_connect, len, ATTEMPT_INSERT_FIRST);
	  retcode = message_3014_handler(p_connect, len, ATTEMPT_UPDATE_FIRST);
//	  shm_table_to_update = MAMMO2;
	  shm_table_to_update = NONE;
	  break ;

	case 3015:
//	  GerrLogFnameMini ("3015_log",  GerrId,"insert[%s]",glbMsgBuffer);
		retcode = message_3015_handler(p_connect, len, ATTEMPT_INSERT_FIRST);
//	  shm_table_to_update = SAPAT;
	  shm_table_to_update = NONE;
	  break ;

	case 3016:
//	  GerrLogFnameMini ("3015_log",  GerrId,"delete[%s]",glbMsgBuffer);
		retcode = message_3016_handler(p_connect, len, ATTEMPT_INSERT_FIRST);
//	  shm_table_to_update = MAMMO2;
	  shm_table_to_update = NONE;
	  break ;

	case 3017:
//	  GerrLogFnameMini ("3017_log",  GerrId,"insert[%s]",glbMsgBuffer);
		retcode = message_3017_handler(p_connect, len, ATTEMPT_INSERT_FIRST);
//	  shm_table_to_update = SAPAT;
	  shm_table_to_update = NONE;
	  break ;

	case 3018:
//	  GerrLogFnameMini ("3018_log",  GerrId,"insert[%s]",glbMsgBuffer);
	  retcode = message_3018_handler(p_connect, len, ATTEMPT_INSERT_FIRST);
	  shm_table_to_update = CLICKSTBLUPD;
	  break;

	case 3019:
//	  GerrLogFnameMini ("3019_log",  GerrId,"insert[%s]",glbMsgBuffer);
	  retcode = message_3019_handler(p_connect, len, ATTEMPT_INSERT_FIRST);
	  shm_table_to_update = NONE;
	  break ;

	case 3023:
//	  GerrLogFnameMini ("3023_log",  GerrId,"insert[%s]",glbMsgBuffer);
	  retcode = message_3023_handler(p_connect, len, ATTEMPT_INSERT_FIRST);
	  shm_table_to_update = NONE;
	  break ;

	case 3024:
//	  GerrLogFnameMini ("3024_log",  GerrId,"insert[%s]",glbMsgBuffer);
	  retcode = message_3024_handler(p_connect, len, ATTEMPT_INSERT_FIRST);
	  shm_table_to_update = NONE;
	  break ;

	case 3025:
//	  GerrLogFnameMini ("3025_log",  GerrId,"insert[%s]",glbMsgBuffer);
	  retcode = message_3025_handler(p_connect, len, ATTEMPT_INSERT_FIRST);
	  shm_table_to_update = NONE;
	  break ;

	case 3027:
//	  GerrLogFnameMini ("3027_log",  GerrId,"insert[%s]",glbMsgBuffer);
	  retcode = message_3027_handler(p_connect, len, ATTEMPT_INSERT_FIRST);
	  shm_table_to_update = NONE;
	  break ;

	case 3026:
//	  GerrLogFnameMini ("3026_log",  GerrId,"insertlen[%d][%s]",len,glbMsgBuffer);
	  retcode = message_3026_handler(p_connect, len, ATTEMPT_INSERT_FIRST);
	  shm_table_to_update = NONE;
	  break ;
	case 3031://CONTRACTNEW
//	GerrLogMini (GerrId," 3031:[%d] [%s]",len,glbMsgBuffer);
	  retcode = message_3031_handler(p_connect, len, ATTEMPT_INSERT_FIRST);
	  shm_table_to_update = CONTRACTNEW;
	  break ;
	case 3032://CONTERMNEW
//	GerrLogMini (GerrId," 3032:[%d] [%s]",len,glbMsgBuffer);
	  retcode = message_3032_handler(p_connect, len, ATTEMPT_INSERT_FIRST);
	  shm_table_to_update = CONTERMNEW;
	  break ;
	case 3033://TERMINALNEW
//	GerrLogMini (GerrId," 3033:[%d] [%s]",len,glbMsgBuffer);
	  retcode = message_3033_handler(p_connect, len, ATTEMPT_INSERT_FIRST);
	  shm_table_to_update = TERMINALNEW;
	  break ;
	case 3034://SUBSTNEW
//	GerrLogMini (GerrId," 3034:[%d] [%s]",len,glbMsgBuffer);
	  retcode = message_3034_handler(p_connect, len, ATTEMPT_INSERT_FIRST);
	  shm_table_to_update = SUBSTNEW;
	  break ;
	case 3035://CONTRACTNEWNEW
//	GerrLogMini (GerrId," 3035:[%d] [%s]",len,glbMsgBuffer);
	  retcode = message_3035_handler(p_connect, len, ATTEMPT_INSERT_FIRST);
	  shm_table_to_update = SUBSTNEW;
	  break ;

	//
	//   COMMUNICATION MESSAGES -> no data-records are passed
	/////////////////////////////////////////////////////////
	//
	case CM_REQUESTING_COMMIT:/* 9901 */
	  retcode = commit_handler( p_connect,
				    shm_table_to_update );
	  //
	  // Do not update table update date if nothing updated
	  //
	  shm_table_to_update = NONE;
	  break ;

	case CM_REQUESTING_ROLLBACK:/* 9903 */
	  retcode = rollback_handler( p_connect );
	  break ;

    /* < ---------------------------------------------------------------- > */
    /*									*/
    /*  Comment:							*/
    /* ------------							*/
    /*   The messages bellow can only be SENT (& not be received)	*/
    /*   by AS400<->Unix Server process:				*/
    /*									*/
    /*		CM_CONFIRMING_COMMIT					*/
    /*		CM_CONFIRMING_ROLLBACK					*/
    /*		CM_SYSTEM_GOING_DOWN					*/
    /*		CM_SYSTEM_IS_BUSY					*/
    /*		CM_COMMAND_EXECUTION_FAILED				*/
    /*		CM_SYSTEM_IS_NOT_BUSY					*/
    /*		CM_SQL_ERROR_OCCURED					*/
    /*		CM_DATA_PARSING_ERR					*/
    /*		CM_ALL_RETRIES_WERE_USED				*/
    /*									*/
    /* < ---------------------------------------------------------------- > */


	default:
	  GerrLogReturn(
			GerrId,
			"Got unknown message id : %d",
			msg
			);
	  break;

    }		// switch( msg

    return retcode ;

}

/*===============================================================
||								||
||		answer_to_all_client_requests()			||
||								||
 ===============================================================*/
static int answer_to_all_client_requests(
					 fd_set * called_sock_mask
					 )
{

/*===============================================================
||			local variables				||
 ===============================================================*/
    TSocket	sock ;	/* socket-array iterator		*/
    TConnection * a_client ;  /* ptr to current client-structi	*/
    TSystemMsg	msg ;	/* ID of last received message		*/
    TSuccessStat retval ;/* indicating receive() success	*/
    int		data_len ;/* length of message without header	*/
    int		rc;	/* code returned from message handler   */
    int		conn_id;
    static int	load_time = 0;
    static int	load_count = 0;

/*===============================================================
||			scan all connections			||
 ===============================================================*/
    for( conn_id = 0 ; conn_id < MAX_NUM_OF_CLIENTS ; conn_id++ )
    {
	a_client = &glbConnectionsArray[conn_id];
	
	//
	// Check if connection has a message
	////////////////////////////////////
	//
	if( a_client->server_state == 0 )
	{
	    continue;
	}

	//
	// Check if connection has a message
	////////////////////////////////////
	//
	if( ! FD_ISSET( a_client->socket, called_sock_mask ) )
	{
	    continue;
	}

	//
	// Set to connection with db & in transaction
	/////////////////////////////////////////////
	//
	a_client->last_access 	= time(NULL) ;

//	SQLMD_set_connection( a_client->db_connection_id ) ;

	if( connection_is_not_in_transaction( a_client ) )
	{
	    set_connection_to_in_transaction_state( a_client ) ;
	}

	//
	// Get message id from socket
	/////////////////////////////
	//
//20130103
//GerrLogMini ( GerrId,"before get_message_id");    
	msg = get_message_ID( a_client );
	if( msg == BAD_DATA_RECEIVED ||
	    msg == ERROR_ON_RECEIVE )
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
	}

	//printf("Got message < %d >\n", msg);fflush(stdout);
	//
	// If not a control message -- read message data
	////////////////////////////////////////////////
	//
	if( NOT_A_COMMUNICATION_MSG( msg ) )
	{
	    //
	    // If too loaded -- nap a bit
	    /////////////////////////////
	    //
	    ret = time( NULL );

	    if( ret != load_time || load_count > 15 )
	    {
		//if( load_count > 15 ) nap( 500 );

		load_count = 0;
		load_time  = ret;
	    }

	    load_count++;

	    //
	    // Read message data
	    ////////////////////
	    //
	    data_len = get_struct_len( msg );
//GerrLogMini ( GerrId,"msg[%d] data_len[%d]glbMsgBuffer:[%d]<%s>",msg,data_len,strlen(glbMsgBuffer),glbMsgBuffer);
	    retval = receive_from_socket( a_client, glbMsgBuffer, data_len );
//	    GerrLogMini ( GerrId,"after recv5 default buf");   
//GerrLogMini ( GerrId,"msg[%d] retval[%d],glbMsgBuffer:<%s>",msg,retval,glbMsgBuffer);
	    if( retval == failure )
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
	    }
	}
	else
	{
	    data_len = -1; /* not used */
	}

	//
	// Call message handler
	///////////////////////
	//
//    GerrLogMini ( GerrId,"after recv6 default buf");
//GerrLogMini (GerrId, "DocServer received message %d.", msg);
	rc = call_msg_handler( msg, a_client, data_len );
//	    GerrLogMini ( GerrId,"after recv7 default buf");
//GerrLogMini ( GerrId,"1[%d][%d]",msg,data_len);
//	printf("\n 3008 msg[%d]rc[%d][%d]conn_id[%d]",msg,rc,MSG_HANDLER_SUCCEEDED,conn_id);fflush(stdout);
	switch( rc )
	{
	    case MSG_HANDLER_SUCCEEDED:

		break;

	    case SQL_ERROR_OCCURED:

		a_client->sql_err_flg = 1;
		break;

	    case DATA_PARSING_ERR:

		GerrLogReturn(
			      GerrId,
			      "DATA_PARSING_ERR for msg %d",
			      msg
			      );

		terminate_a_connection( a_client, WITH_ROLLBACK );
		break;

	    case ALL_RETRIES_WERE_USED:

		GerrLogReturn(
			      GerrId,
  			      "ALL_RETRIES_WERE_USED for msg %d",
			      msg
			      );

		a_client->sql_err_flg = 1;
		break;

	    default:
		GerrLogReturn(
			      GerrId,
			      "unknown return code : %d for Transaction %d.",
			      rc,
				  msg
			      );
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

	if( ToGoDown( DOCTOR_SYS ) )
	{
	    *pSysDown = true ;
	}
    }

    return(MAC_OK);

}


/* << -------------------------------------------------------- >> */
/*                                                                */
/*     get a short-int value from buffer & promote buf-ptr	  */
/*                                                                */
/* << -------------------------------------------------------- >> */
static short get_short(
		       char	**p_buffer,
		       int	ascii_len
		       )
{
  char	tmp[16];
  int	i;


  memcpy( tmp , *p_buffer , ascii_len ) ;

  /*
   *  verify token is numeric:
   */
  for( i = 0  ; i < ascii_len ;  i++ )
    {
      if( legal_digit( tmp, i ) == false )
	{
	  glbErrorFlg = true;
	  return -1;
	}
    }

  tmp[ascii_len]  =  0 ;	/*  null terminate string    */
  *p_buffer  +=  ascii_len ;    /*  promote buffer position  */

  return atoi( tmp );
}



/* << -------------------------------------------------------- >> */
/*                                                                */
/*     get a int value from buffer & promote buf-ptr	  */
/*                                                                */
/* << -------------------------------------------------------- >> */
static long get_intv(
		     char	**p_buffer,
		     int	ascii_len
		     )
{
  char	tmp[32];
  int	i;


  memcpy( tmp , *p_buffer , ascii_len ) ;

  /*
   *  verify token is numeric:
   */
  for( i = 0  ; i < ascii_len ;  i++ )
    {
      if( legal_digit( tmp, i ) == false )
	{
	  glbErrorFlg = true;
	  return -1;
	}
    }

  tmp[ascii_len]  =  0 ;	/*  null terminate string    */
  *p_buffer  +=  ascii_len ;    /*  promote buffer position  */

  return atoi( tmp );
}


/* << -------------------------------------------------------- >> */
/*                                                                */
/*     get a long-int value from buffer & promote buf-ptr	  */
/*                                                                */
/* << -------------------------------------------------------- >> */
static int get_long(
		     char	**p_buffer,
		     int	ascii_len
		     )
{
  char	tmp[64];
  int	i;


  memcpy( tmp , *p_buffer , ascii_len ) ;

  /*
   *  verify token is numeric:
   */
  for( i = 0  ; i < ascii_len ;  i++ )
    {
      if( legal_digit( tmp, i ) == false )
	{
	  glbErrorFlg = true;
	  return -1L;
	}
    }

  tmp[ascii_len]  =  0 ;	/*  null terminate string    */
  *p_buffer  +=  ascii_len ;    /*  promote buffer position  */

  return atol( tmp );
}
//Yulia 20110406
/* << -------------------------------------------------------- >> */
/*     get a double value from buffer & promote buf-ptr	  */
/* << -------------------------------------------------------- >> */
static double get_double(  char	**p_buffer, int	ascii_len  )
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
/*        get a string value from buffer & promote buf-ptr	  */
/*                                                                */
/* << -------------------------------------------------------- >> */
static void get_string(
		       char	** p_buffer ,
		       char	*str,
		       int	ascii_len
		       )
{
  memcpy( str, *p_buffer , ascii_len ) ;
  str[ascii_len]  =  0 ;
  *p_buffer  +=  ascii_len ;
}

/*=======================================================================
||									||
||            Inform shared memory tables change has occured.		||
||									||
 =======================================================================*/
void UpdateSharedMem(
		     TShmUpdate  tbl_to_update
		     )
{

    EXEC SQL BEGIN DECLARE SECTION;
    long int    update_date;
    long int    update_time;
    char        table_name[64];
    EXEC SQL END DECLARE SECTION;

    bool        update_pl_macabi_flg;
    time_t      curr_time;
    struct tm   *tmp;
    int		retries;

    //
    // Do not update table date if nothing updated
    //
    if( tbl_to_update == NONE )
    {
	return;
    }

    /////
    /// Get current time + ten minutes
    //
    time( &curr_time );

    curr_time += SHM_UPDATE_INTERVAL * 60;

    tmp = localtime( &curr_time );

    update_date = (tmp->tm_year + 1900) * 10000+
		  (tmp->tm_mon + 1 ) * 100 +
		   tmp->tm_mday;

    update_time = tmp->tm_hour * 10000 +
		  tmp->tm_min * 100+
		  tmp->tm_sec;

    update_pl_macabi_flg = false;

    switch( tbl_to_update )
    {
//	case DRUG_LIST:
//	    strcpy( table_name, "Drug_list" );
//	    break;

//	case PRICE_LIST:
//	    update_pl_macabi_flg = true;       /* -> update PRICE_LIST_MACABI */
//	    strcpy( table_name, "Price_list" );

	    /*   Use yarpa date & time instead of current:
	     */
//	    update_date = glbPriceListUpdateArray[0];    /* v_date_yarpa */
//	    update_time = glbPriceListUpdateArray[1];    /* v_time_yarpa */
//	     break;

	case LABNOTES:
	    strcpy( table_name, "Labnotes" );
	    break;

	case LABRESULTS:
	    strcpy( table_name, "Labresults" );
	    break;

	case TEMPMEMB:
	    strcpy( table_name, "Tempmemb" );
	    break;

	case AUTH:
	    strcpy( table_name, "Auth" );
	    break;

	case CONTRACT:
    	    strcpy( table_name, "Contract" );
	    break;

	case CONTERMDEL:
    	    strcpy( table_name, "Contermdeleted" );
	    break;

	case CONMEMB:
    	    strcpy( table_name, "Conmemb" );
	    break;

	case HOSPITALMSG1:
    	    strcpy( table_name, "HospitalMsg1" );
	    break;

	case HOSPITALMSG2:
    	    strcpy( table_name, "HospitalMsg2" );
	    break;

	case SPEECHCLIENT:
    	    strcpy( table_name, "SpeechClient" );
	    break;

	case RASHAM:
    	    strcpy( table_name, "Rasham" );
	    break;
	case CLICKSTBLUPD:
    	    strcpy( table_name, "ClicksTblUpd" );
	    break;
	case CONTRACTNEW:
   	    strcpy( table_name, "CONTRACTNEW" );
	    break;
	case CONTERMNEW:
   	    strcpy( table_name, "CONTERMNEW" );
	    break;
	case TERMINALNEW:
   	    strcpy( table_name, "TERMINALNEW" );
	    break;
	case SUBSTNEW:
   	    strcpy( table_name, "SUBSTNEW" );
	    break;

	case NONE:
	    return;

	default:
	    GerrLogReturn(
			  GerrId,
			  "Unknown shm table num to update : <%d>",
			  tbl_to_update
			  );
	    return;
    }		// switch( table_num ..

    //
    // Update shamred memory table date & time
    //
    for( retries = 0; retries < SQL_UPDATE_RETRIES; retries++)
    {
	/*
	 *  update table's row with current date & time:
	 */
	EXEC SQL
	   UPDATE tables_update
	   SET    update_date = :update_date,
		  update_time = :update_time
	   WHERE  table_name  = :table_name;

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
	    EXEC SQL
	      INSERT INTO tables_update
	      ( table_name, update_date, update_time )
	      VALUES
	      ( :table_name, :update_date, :update_time );

	    if( SQLERR_code_cmp( SQLERR_access_conflict ) == MAC_TRUE )
	    {
		sleep( 1 );
		continue;
	    }
	}

	if( update_pl_macabi_flg == true  &&  SQLERR_error_test() == MAC_OK )
	{
	    strcpy( table_name, "Price_list_macabi" );

	    /*   Use Macabi date & time as passed in msg 2031:
	     */
	    update_date = glbPriceListUpdateArray[2];    /* v_date_macabi */
	    update_time = glbPriceListUpdateArray[3];    /* v_time_macabi */
	}

	break;

    }		// for ( retries ..

}

/*=======================================================================
||									||
||            			pipe_handler()				||
||									||
 =======================================================================*/
void	pipe_handler( int signo )
{
    GerrLogReturn(
		  GerrId,
		  "Got hanged-up by client"
		  );

    sigaction( SIGPIPE, &sig_act, NULL );
}



  void DosWinHebConv( char * str, int n )
  { // Conversion from Dos Encoding to Windows. GAL 04/04/2004
	LangState state = stHebrew, lastState;
    int nEngStart = 0;
    int nEngEnd	  = 0;
    int nNumStart = 0;
    int nNumEnd	  = 0;
    int i;
	char *source_chars = ")(", *target_chars="()", *pos;

	if( strlen( target_chars ) < strlen( source_chars ) )
	{
		printf( "EngNumHebIndent : target chars array shorter than source chars array\n" );
		return;
	}

    for( i = 0;  i < n;  i ++ )
    {
		// 1) break on end of string
		if ( str[i] == 0 ) 
		{
			 break;
		}
		
		// 2) fix chars
		pos = strchr( source_chars , str[i] );
		if( pos )
		{
			str[i] = target_chars[ pos - source_chars ];
		}
		else
		{
			if( ishebrew( str[ i ] ) )
			{   // From Dos Hebrew to Windows Hebrew
//			    str[ i ]  = ( UCHAR ) str[ i ] + 96;
			}
		}

		// 3) handle hebrew-english-number strings
		lastState = state;

		switch( state )
		{
		  case stHebrew:			// HEBREW STATE

			if ( isenglish( str[ i ] ) )
			{
				 state	= stEnglish;
				 nEngStart = i;
				 break;
			}
			if ( isnumber( str[ i ] ) )
			{
				 state	= stNumber;
				 nNumStart = i;
				 break;
			}
			break;


		  case stEnglish:
 		
			if ( ishebrew( str[ i ] ) || isend( str[ i + 1 ] ) )
			{
				 nEngEnd = i - 1;
				 if( ! ishebrew( str[ i ] ) )
				 {
					   nEngEnd = i;
				 }

				 for( ; 
				      nEngEnd >= 0 && isneutral( str[ nEngEnd ] );
					  nEngEnd -- );
				 state = stHebrew;
			}
			break;


		  case stNumber:

			if ( ishebrew( str[ i ] )									||
			   ( isnumbersemistopper( str[ i ] )						&&
			   ! isnumber( str[ i + 1 ] ) )								||
				 isnumberstopper( str[ i ] )							||
			   ( isspace( str[ i ] ) && ! isenglish( str[ i + 1 ] ) )	||
				 isend( str[ i + 1 ] )									||
			   ( str[i] == '.'											&&
				 isspace( str[ i + 1 ] ) ) )
			{
				 nNumEnd = i-1;
				 if( isend( str[ i + 1 ] ) && str[ i ] != '.' )
				 {
					 nNumEnd = i;
				 }
				
				 for( ; 
				      nNumEnd >= 0 && isnumberextra( str[ nNumEnd ] );
					  nNumEnd -- );
				 state	= stHebrew;
				 break;
			}
			break;
		}


		// Finally : do the reverse of english and numbers
		if ( lastState == stEnglish && state != stEnglish )
		{
			 strnrev( str + nEngStart , nEngEnd - nEngStart + 1 );
		}
		if ( lastState == stNumber && state != stNumber )
		{
			 strnrev( str + nNumStart , nNumEnd - nNumStart + 1 );
		}
    }
  
  } // DosWinHebConv

  void DosWinHebConvMultiLine( char * str, int length )
  {
	int line_length;
	char *from , *to, *end_of_str = str + MIN(strlen(str), length);
	
	for( from = str; from < end_of_str; )
	{
//		to = strchr( from, '\n' );
		to = strchr( from, 13 );

		line_length = (to == NULL) ?
			strlen(from) : to - from;

		// Translate it for proper display
		DosWinHebConv( from , line_length );
        strnrev( from , line_length ); // ?!?
		// Jump to next line
		from += line_length + 1;
	}
  
  } // DosWinHebConvMultiLine

  static void strnrev( char *str, int n )
  {
    char c, *end;
    
    for( end = str + n - 1; str < end; str++, end-- )
    {
		c    = *str;
		*str = *end;
		*end = c;
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

	caught_signal	= signo;
	time_now		= GetTime();


	// Detect endless loops and terminate the process "manually".
	if ((signo == last_signo) && (time_now == last_called))
	{
		GerrLogReturn (GerrId,
					   "As400UnixDocServer aborting endless loop (signal %d). Terminating process.",
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
//			EXEC SQL ROLLBACK WORK;	/* Any pending work is incomplete and needs to be undone. */
			sig_description = "floating-point error - probably division by zero";
			break;

		case SIGSEGV:
			RollbackAllWork ();
//			EXEC SQL ROLLBACK WORK;	/* Any pending work is incomplete and needs to be undone. */
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
				   "As400UnixDocServer received Signal %d (%s). Terminating process.",
				   signo,
				   sig_description);
}


void EmbeddedInformixConnect (char	*username, char	*password, char	*dbname)
{
	EXEC SQL BEGIN DECLARE SECTION;
		char	*user;
		char	*pass;
		char	*db;
		int		sql_result;
	EXEC SQL END DECLARE SECTION;

	user	=	username;
	pass	=	password;
	db		=	dbname;

	// Connect to database - if we're retrying after an ODBC connect
	// failure and we're already connected the old way, don't attempt
	// to reconnect.
	if (!EmbeddedInformixConnected)
	{
		EXEC SQL
			CONNECT TO	:db
			USER		:user
			USING		:pass;

		// Test error
		sql_result = SQLERR_error_test();

		EmbeddedInformixConnected = (sql_result == INF_OK) ? 1 : 0;

//		if (sql_result != 0)
			GerrLogMini (GerrId, "CONNECT (old style) to %s/%s/%s returns %d.", db, user, pass, sql_result);
	}
}

