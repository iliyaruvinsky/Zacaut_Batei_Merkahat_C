/* << ------------------------------------------------------ >> */
/*  		        As400UnixDoc2Server.ec                      */
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


//#include <sys/netinet/tcp.h> Y20060725
#include <netinet/tcp.h>

struct sigaction	sig_act;
struct sigaction	sig_act_terminate;

short	EmbeddedInformixConnected = 0;

void	pipe_handler			(int signo);
void	TerminateHandler		(int signo);

void	switch_to_win_heb		(unsigned char *source);
void	EmbeddedInformixConnect	(char	*username, char	*password, char	*dbname);
  
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
int main (int argc, char *argv[])
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
#ifdef AS400_DEBUG
 printf("\n point 1");
 fflush(stdout);
#endif // AS400_DEBUG

    GetCommonPars( argc, argv, &retrys );

	// DonR 14Aug2022: Changed Program Type to new value, since this program
	// was previously using the same value as As400DocServer and this appears
	// to have been causing problems with auto-restart. (In any case, the new
	// version of FatherProcess means that these Program Type values should
	// be unique for each program in the system.)
    RETURN_ON_ERR(
		  InitSonProcess(
				 argv[0],
				 DOC2AS400TOUNIX_TYPE,
				 retrys,
				 0,
				 DOCTOR_SYS
				 )
		   );

#ifdef AS400_DEBUG
 printf("\n point 2");
 fflush(stdout);
#endif // AS400_DEBUG

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

//	// DonR 13Feb2020: Add signal handler for segmentation faults - used
//	// by ODBC routines to detect invalid pointer parameters.
//	sig_act_ODBC_SegFault.sa_handler	= ODBC_SegmentationFaultCatcher;
//	sig_act_ODBC_SegFault.sa_mask		= NullSigset;
//	sig_act_ODBC_SegFault.sa_flags		= 0;

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
	    puts("Connection map for as400unixdoc2server :");
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
//    struct fd_set   rw; Y20060725 /* Ready to Write */
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
#ifdef AS400_DEBUG
 printf("\n to_send >%s< ",to_send);
 fflush(stdout);
#endif // AS400_DEBUG

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


//    rc = SQLMD_connect_id( str_pos ) ;
//
//    if( ERR_STATE( rc ) )
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
//       setsockopt( l_socket, SOL_SOCKET, SO_REUSEPORT,   Y20060725
//		   &nonzero, sizeof( int ) ) == -1	||             Y20060725 
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
    serveraddr.sin_port        = htons( AS400_UNIX2_PORT ); 

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
static int commit_handler (	TConnection	*p_connect,
							TShmUpdate	shm_table_to_update)
{
	int			save_flg = p_connect->sql_err_flg;

//GerrLogMini (GerrId, "commit_handler: save_flg = %d, SQLCODE = %d.", save_flg, SQLCODE);

	p_connect->sql_err_flg = 0;


	//	if (r3009_uncommitted > 0)
	//	GerrLogFnameMini ("3009_log",
	//					   GerrId,
	//					   "About to commit %d 3009-related DB updates.",
	//					   (int)r3009_uncommitted);

	// Update date of shared memory table if needed
//GerrLogMini (GerrId, "commit_handler: updating shared memory.");
	// DonR 13Sep2020: If we don't have a connection to embedded Informix,
	// don't bother calling UpdateSharedMem() - that routine is just a
	// bunch of embedded-SQL calls.
	if (EmbeddedInformixConnected)
		UpdateSharedMem (shm_table_to_update);

	//	if (r3009_uncommitted > 0)
	//	GerrLogFnameMini ("3009_log",
	//					   GerrId,
	//					   "About to commit 3009-related DB updates - returned from UpdateSharedMem().");

	// pipe is broken -- rollback & close connection
//GerrLogMini (GerrId, "commit_handler: checking pipe.");
	if (pipe_is_broken (p_connect->socket, 100))
	{
//GerrLogMini (GerrId, "commit_handler: pipe is broken!");
		puts("the pipe to AS4000 is broken...");
		puts("-------------------------------");
		fflush(stdout);
		terminate_a_connection (p_connect, p_connect->in_tran_flag ? WITH_ROLLBACK : WITHOUT_ROLLBACK);
	}
	else
	{
//GerrLogMini (GerrId, "commit_handler: pipe is NOT broken!");
		// Sql error occured -- rollback & inform client
		//
		// DonR 13Sep2020: If the last SQLCODE was zero, force save_flg zero as well. This
		// should avoid unwanted rollbacks when the embedded Informix connection is not
		// working but ODBC is.
		if (!SQLCODE)
			save_flg = 0;

		if (save_flg)
		{
#ifdef AS400_DEBUG
	printf("\n save_Flg");
	fflush(stdout);
#endif // AS400_DEBUG

//			GerrLogMini (GerrId, "commit_handler: running DO_ROLLBACK().");
			DO_ROLLBACK( p_connect );

//	if (r3009_uncommitted > 0)
//	{
//		GerrLogFnameMini ("3009_log",
//						   GerrId,
//						   "Rolled back 3009-related DB updates - SQLCODE = %d.",
//						   (int)SQLCODE);
//		r3009_uncommitted = 0;
//	}

			if (inform_client (p_connect, CM_SQL_ERROR_OCCURED, "" ) == failure)
			{
				terminate_a_connection( p_connect, WITHOUT_ROLLBACK );
			}
		}	// Need to rollback.

		else
		{
			// Commit the transaction
//	SQLMD_set_connection( p_connect->db_connection_id );

#ifdef AS400_DEBUG
	printf("\n commit____");
	fflush(stdout);
#endif // AS400_DEBUG
//			SQLMD_set_connection (p_connect->db_connection_id);

//GerrLogMini (GerrId, "Calling CommitAllWork()...");
			CommitAllWork ();
//			CommitWork (MAIN_DB);
//GerrLogMini (GerrId, "After CommitAllWork() to %s, SQLCODE = %d.", MAIN_DB->Name, SQLCODE);

			if (SQLCODE == 0)
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

				set_connection_to_not_in_transaction (p_connect);

				if (inform_client (p_connect, CM_CONFIRMING_COMMIT, "" ) == failure)
				{
					terminate_a_connection (p_connect, WITHOUT_ROLLBACK);
				}
			}	// Commit succeeded.
			else
			{
				terminate_a_connection (p_connect, WITH_ROLLBACK);
			}	// Commit failed.
		}	// Need to commit.
	}	// Pipe is NOT broken.

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
// handler for message 3001 - insert to table LABNOTES
static int message_3001_handler( TConnection	*p_connect,
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
	GerrLogFnameMini ("3001_log", GerrId,"cnt[%9d-%1d]id[%9d]reqid[%9d][%5d]",
			LabNoteRec.Contractor,Contr_N,
			LabNoteRec.MemberId.Number,
			LabNoteRec.ReqId,LabNoteRec.ReqCode);

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
// end of msg 3001 handler


// handler for message 3002 - insert to table LABRESULTS
static int message_3002_handler( TConnection	*p_connect,
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
 int sysdate;
 int systime;
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
  if (LabResRec.TermId > 0)
  {
	GerrLogFnameMini ("3002_log", GerrId,"lab[%d]term[%d]con[%d-%d]id[%d]",
			LabResRec.LabId,LabResRec.TermId,LabResRec.Contractor,Contr_N,
			LabResRec.MemberId.Number);
  }
  group_num_id			=  get_intv( &PosInBuff,   5);
  RETURN_ON_ERROR;
  Contr_N =  get_short( &PosInBuff,  2);
//GerrLogMini (GerrId, "Doc2Server: Contr_N = %d.", Contr_N);
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

	for (	n_retries = 0;
			( n_retries < SQL_UPDATE_RETRIES ) && ( stop_retrying == false );
			n_retries++
		)
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
		}	// if (LabResRec.Contractor > 0)

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

		if ( retcode == 0)
			stop_retrying = true;
	}	// Retry loop.

	prev_contractor = LabResRec.Contractor;
	prev_idnumber	= LabResRec.MemberId.Number;
	prev_idcode		= LabResRec.MemberId.Code;
	prev_termid_upd = LabResRec.TermId;
	return MSG_HANDLER_SUCCEEDED;
}
// end of msg 3002 handler



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

#ifdef AS400_DEBUG
 printf("\n PosInBuff>%s<",PosInBuff);
 fflush(stdout);
#endif // AS400_DEBUG

  ContractRecord.Contractor = 		get_long( &PosInBuff,
			     		     IdNumber_9_len);
#ifdef AS400_DEBUG
 printf("\n contractor>%d<",ContractRecord.Contractor);
 fflush(stdout);
#endif // AS400_DEBUG
  RETURN_ON_ERROR;

  ContractRecord.ContractType	=	get_short ( &PosInBuff,
						ContractType_2_len);
#ifdef AS400_DEBUG
 printf("\n contracttype>%d<",ContractRecord.ContractType);
 fflush(stdout);
#endif // AS400_DEBUG
  RETURN_ON_ERROR;

			   get_string(
				      &PosInBuff,
				      ContractRecord.FirstName,
				      FirstName_8_len);
//switch_to_win_heb(( unsigned char *)ContractRecord.FirstName);
#ifdef AS400_DEBUG
 printf("\n firstname>%s<",ContractRecord.FirstName);
 fflush(stdout);
#endif // AS400_DEBUG
			   get_string(
				      &PosInBuff,
				      ContractRecord.LastName,
				      LastName_14_len);
#ifdef AS400_DEBUG
 printf("\n Lastname>%s<",ContractRecord.LastName);
 fflush(stdout);
#endif // AS400_DEBUG
			   get_string(
				      &PosInBuff,
				      ContractRecord.Street,
				      Street_16_len);
#ifdef AS400_DEBUG
 printf("\n Street>%s<",ContractRecord.Street);
 fflush(stdout);
#endif // AS400_DEBUG
			   get_string(
				      &PosInBuff,
				      ContractRecord.City,
				      City_20_len);
 
#ifdef AS400_DEBUG
 printf("\n City>%s<",ContractRecord.City);
 fflush(stdout);
#endif // AS400_DEBUG
			   get_string(
				      &PosInBuff,
				      ContractRecord.Phone,
				      Phone_10_len);
#ifdef AS400_DEBUG
 printf("\n Phone>%s<",ContractRecord.Phone);
 fflush(stdout);
#endif // AS400_DEBUG
  // ContractRecord.Mahoz =  get_short( &PosInBuff, 1); Changed by GAL 13/01/2003
  ContractRecord.Mahoz = get_short( &PosInBuff , Mahoz_len );
  RETURN_ON_ERROR;

  ContractRecord.Profession =  get_short( &PosInBuff, 2);
  RETURN_ON_ERROR;

  ContractRecord.AcceptUrgent =  get_short( &PosInBuff, 1);
  RETURN_ON_ERROR;

  ContractRecord.SexesAllowed =  get_short( &PosInBuff, 1);
  RETURN_ON_ERROR;

  ContractRecord.AcceptAnyAge =  get_short( &PosInBuff, 1);
  RETURN_ON_ERROR;

  ContractRecord.AllowKbdEntry =  get_short( &PosInBuff, 3);
  RETURN_ON_ERROR;

  ContractRecord.NoShifts =  get_short( &PosInBuff, 1);
  RETURN_ON_ERROR;

  ContractRecord.IgnorePrevAuth =  get_short( &PosInBuff, 1);
  RETURN_ON_ERROR;

  ContractRecord.AuthorizeAlways =  get_short( &PosInBuff, 1);
  RETURN_ON_ERROR;

  ContractRecord.NoAuthRecord =  get_short( &PosInBuff, 1);
  RETURN_ON_ERROR;

  ContermRecord.TermId = get_long( &PosInBuff, TermId_7_len);
  RETURN_ON_ERROR;

  ContermRecord.AuthType = get_short( &PosInBuff,1);
  RETURN_ON_ERROR;

  ContermRecord.ValidFrom = get_long( &PosInBuff,DateYYYYMMDD_len);
  RETURN_ON_ERROR;

  ContermRecord.ValidUntil = get_long( &PosInBuff,DateYYYYMMDD_len);
  RETURN_ON_ERROR;

  ContermRecord.Location = get_long( &PosInBuff,LocationId_7_len);
  RETURN_ON_ERROR;

  //  Changed by GAL 13/01/2003 1->2
  ContermRecord.Mahoz = get_short( &PosInBuff , Mahoz_len );
  RETURN_ON_ERROR;

  ContermRecord.Snif = get_long( &PosInBuff,5);
  RETURN_ON_ERROR;

  ContermRecord.Locationprv = get_long( &PosInBuff,LocationId_7_len);
  RETURN_ON_ERROR;

			   get_string( &PosInBuff,
					ContermRecord.Position,
				      	1);
			   get_string( &PosInBuff,
					ContermRecord.Paymenttype,
				      	1);
  ContermRecord.ServiceType = get_short( &PosInBuff,3);
  RETURN_ON_ERROR;

  SubstRecord.AltContractor =  get_long( &PosInBuff,
			     		 IdNumber_9_len);
  RETURN_ON_ERROR;

  SubstRecord.SubstType = get_short( &PosInBuff,2);
  RETURN_ON_ERROR;

  SubstRecord.DateFrom = get_long( &PosInBuff,DateYYYYMMDD_len);
  RETURN_ON_ERROR;

  SubstRecord.DateUntil = get_long( &PosInBuff,DateYYYYMMDD_len);
  RETURN_ON_ERROR;

  SubstRecord.SubstReason = get_short( &PosInBuff,2);
  RETURN_ON_ERROR;

//switch_to_win_heb(( unsigned char *)ContractRecord.LastName);
//switch_to_win_heb(( unsigned char *)ContractRecord.Street);
//switch_to_win_heb(( unsigned char *)ContractRecord.City);

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
		ret_contract = 
			DbUpdateContractRecord(	ContractRecord.Contractor,
						ContractRecord.ContractType,
						&ContractRecord) ;
//		printf ("\n This contractor updated in the system already: %d,%d",
//				ContractRecord.Contractor,
//				ContractRecord.ContractType); fflush(stdout);

	}
	else
	{
//		printf ("\n Insert contractor in the system : %d,%d",
//				ContractRecord.Contractor,
//				ContractRecord.ContractType); fflush(stdout);
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

     	strcpy(TerminalRecord.Descr,"AZL ");  // test 2000.05.07
//		strcpy(TerminalRecord.Descr,"\224\246\236 ");
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
		if ( DbGetSubstRecord( ContractRecord.Contractor,
					ContractRecord.ContractType,
					SubstRecord.AltContractor,
					&SubstRecordDummy) )
		{
			ret_subst=DbUpdateSubstRecord(ContractRecord.Contractor,
					ContractRecord.ContractType,
					SubstRecord.AltContractor,
					&SubstRecord) ;
//		printf ("\n This altcontractor updated in the system already: %d,%d,%d------",
//				ContractRecord.Contractor,
//				ContractRecord.ContractType,
//				SubstRecord.AltContractor);

		}
		else
		{
//			printf (" \n Insert altcontractor in the system :%d %d %d-------",
//				ContractRecord.Contractor,
//				ContractRecord.ContractType,
//				SubstRecord.AltContractor);
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
			ret_subst_inv = 
				DbUpdateSubstRecord(
						SubstRecord.Contractor,
				     		ContractRecord.ContractType,
					 	SubstRecord.AltContractor,
				     		&SubstRecord) ;
//			 printf ("\n This invert alt updated in the system:%d,%d,%d",
//		 		SubstRecord.Contractor,
//				ContractRecord.ContractType,
//				SubstRecord.AltContractor);

		}
		else
		{
//		     printf (" \n Insert invert alt in the system:%d %d %d",
//				     SubstRecord.Contractor,
//				     ContractRecord.ContractType,
//				     SubstRecord.AltContractor);
		     ret_subst_inv = DbInsertSubstRecord(    &SubstRecord );
		 }         
	}



 /* HANDLE_INSERT_SQL_CODE( "3006", p_connect,
			  &do_update, &retcode, 
			  &stop_retrying );
			  */
	/*************************************************/
   	/* only if all 4 tables are all right:           */
	/*		- need insert + do it            */
	/*		- don't need insert              */
	/*	                          we have commit */
	/*************************************************/
   retcode = 1- ret_contract * ret_conterm * ret_terminal * 
   				ret_subst * ret_subst_inv;
   if( retcode == 0) stop_retrying = true;
#ifdef AS400_DEBUG

 printf("\n retcodes:contr>%d<,conterm>%d<,terminal>%d<,subst>%d<,subst1>%d<,stop<%d>",
	ret_contract,ret_conterm,ret_terminal,ret_subst,ret_subst_inv,stop_retrying);
 fflush(stdout);
#endif // AS400_DEBUG
   }
#ifdef AS400_DEBUG
// printf("\n retcode:%d",retcode);
// fflush(stdout);
#endif // AS400_DEBUG
  return retcode;
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

#ifdef AS400_DEBUG_3007
 printf("\n PosInBuff>%s<",PosInBuff);
 fflush(stdout);
#endif // AS400_DEBUG_3007

Contractor			=  get_long( &PosInBuff,
			     		     IdNumber_9_len);

#ifdef AS400_DEBUG_3007
 printf("\n Contractor:%d",Contractor);
 fflush(stdout);
#endif // AS400_DEBUG_3007
  RETURN_ON_ERROR;

ContractType			=  get_short( &PosInBuff,
			     		     ContractType_2_len);

#ifdef AS400_DEBUG_3007
 printf("\n ContractType:%d",ContractType);
 fflush(stdout);
#endif // AS400_DEBUG_3007
  RETURN_ON_ERROR;

  TermId			=  get_long( &PosInBuff,
			     		     TermId_7_len);

#ifdef AS400_DEBUG_3007
 printf("\n TermId:%d",TermId);
 fflush(stdout);
#endif // AS400_DEBUG_3007
  RETURN_ON_ERROR;

  Location			=  get_long( &PosInBuff,
			     		     Location_7_len);

#ifdef AS400_DEBUG_3007
 printf("\n Location:%d",Location);
 fflush(stdout);
#endif // AS400_DEBUG_3007
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



// handler for message 3008 - insert to table Hospmessage

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
long int sysdate;
long int systime;
char str_typeresult[25];
EXEC SQL END DECLARE SECTION;
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
#ifdef AS400_DEBUG
printf("\n 3008  HospitalRec.MemberId.Code:%d",HospitalRec.MemberId.Code);
fflush(stdout);
#endif // AS400_DEBUG
  RETURN_ON_ERROR;

  HospitalRec.MemberId.Number	=  get_intv( &PosInBuff,
				     		     IdNumber_9_len);
#ifdef AS400_DEBUG
 printf("\n 3008  HospitalRec.MemberId.Number:%d",HospitalRec.MemberId.Number);
 fflush(stdout);
#endif // AS400_DEBUG
  RETURN_ON_ERROR;

  HospitalRec.HospDate	=  get_intv( &PosInBuff,    DateYYYYMMDD_len);
#ifdef AS400_DEBUG
 printf("\n 3008 HospitalRec.HospDate:%d",HospitalRec.HospDate);
 fflush(stdout);
#endif // AS400_DEBUG
  RETURN_ON_ERROR;

  HospitalRec.NurseDate	=  get_intv( &PosInBuff,    DateYYYYMMDD_len);
#ifdef AS400_DEBUG
 printf("\n 3008  HospitalRec.NurseDate:%d",HospitalRec.NurseDate);
 fflush(stdout);
#endif // AS400_DEBUG
  RETURN_ON_ERROR;

			   get_string(
				      &PosInBuff,
  				      HospitalRec.ContrName,
				      30);
  HospitalRec.HospitalNumb			=  get_intv( &PosInBuff,   ReqId_9_len);
#ifdef AS400_DEBUG
 printf("\n 3008 HospitalRec.HospitanNumb:%d",HospitalRec.HospitalNumb);
 fflush(stdout);
#endif // AS400_DEBUG
  RETURN_ON_ERROR;

			   get_string(
				      &PosInBuff,
  				      HospitalRec.Result,
				      1120);
/* string_to_heb*/				       

//#ifdef AS400_DEBUG
// printf("\n 3008  HospitalRec.Result:%s",HospitalRec.Result);
// fflush(stdout);
//#endif AS400_DEBUG
/*			   get_string(
				      &PosInBuff,
  				      HospitalRec.HospName,
				      25);
*/
/*Yulia 20040422*/
/*	put these 2 field into contr_name and typeresult for improve clicks information*/
/*temp 0603			   get_string(
				      &PosInBuff,
  				      str_typeresult,
				      25);
			   */
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
/*	reverse hospital_name that write into "typeresult" field because 
			   macauth don't reverse this field*/
//temp 0603	for (i=0;i< 25;i++)HospitalRec.HospName[i]=str_typeresult[24-i];

    strcpy(HospitalRec.TypeResult,    " Beit holim     ");
	HospitalRec.ResCount	=	1;
	//only windows hebrew without reverse 20040607 Yulia
	//insert hospital and department names into contrname and typeresult fields
	//for Win version
	if (HospitalRec.FlgSystem == 1)
	{
		strcpy(HospitalRec.ResName1,  "               ushpuz ");
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
		strcpy(HospitalRec.ResName1,  "                     Ishpuz ");
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

GerrLogFnameMini ("3010_log",  GerrId,"[%s]",glbMsgBuffer);

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
  
  get_string(  &PosInBuff,  HospitalRec.Result,       2048);
GerrLogFnameMini ("3010_log",  GerrId,"[%d][%d]",ReadInput, data_len);

  HospitalRec.FlgSystem		=  2; //0 - old DOS format,1-new WIN format,2-PATO system WIN 
  HospitalRec.ResCount	=	1;
  HospitalRec.ResultSize = 2048;
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



/*===============================================================
||								||
||			call_msg_handler()			||
||								||
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

	case 3001:
	  retcode = message_3001_handler(p_connect, len, ATTEMPT_INSERT_FIRST);
	  shm_table_to_update = LABNOTES;
	  break ;

	case 3002:
	  retcode = message_3002_handler(p_connect, len, ATTEMPT_INSERT_FIRST);
	  shm_table_to_update = LABRESULTS;
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
	  GerrLogFnameMini ("3010_log",  GerrId,"1",glbMsgBuffer);
	  shm_table_to_update = HOSPITALMSG2;
	  break ;

	case 3011:
	  retcode = message_3011_handler(p_connect, len, ATTEMPT_INSERT_FIRST);
//	  printf("\n 3009 retcode[%d][%d]",retcode,MSG_HANDLER_SUCCEEDED);fflush(stdout);
	  shm_table_to_update = SPEECHCLIENT;
	  break ;

	//
	//   COMMUNICATION MESSAGES -> no data-records are passed
	/////////////////////////////////////////////////////////
	//
	case CM_REQUESTING_COMMIT:/* 9901 */
#ifdef AS400_DEBUG
 printf(" p_connect->sql_err_flg >%d<",p_connect->sql_err_flg);
 fflush(stdout);
#endif // AS400_DEBUG
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
	msg = get_message_ID( a_client );


// printf("\n 3008 msg=%d,conn_id=%d",msg,conn_id);
 fflush(stdout);

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

	    retval = receive_from_socket( a_client, glbMsgBuffer, data_len );
#ifdef AS400_DEBUG
 printf("\n data_len>%d<,glbMsgBuffer:<%s>",data_len,glbMsgBuffer);
 fflush(stdout);
#endif // AS400_DEBUG

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
//GerrLogMini (GerrId, "Doc2Server received message %d.", msg);
	rc = call_msg_handler( msg, a_client, data_len );

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
					   "As400UnixDoc2Server aborting endless loop (signal %d). Terminating process.",
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
				   "As400UnixDoc2Server received Signal %d (%s). Terminating process.",
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



