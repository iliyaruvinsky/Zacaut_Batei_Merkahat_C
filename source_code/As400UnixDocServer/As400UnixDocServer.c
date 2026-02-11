/* << ------------------------------------------------------ >> */
/*  		        As400UnixDocServer.c                        */
/*  ----------------------------------------------------------- */
/*  Date: 		7/1998			 								*/
/*  Written by:  	Revital & Yulia          	  				*/
/*  ----------------------------------------------------------- */
/*  Purpose :													*/
/*       Source file for client-process in AS400 <-> Unix   	*/
/*       communication system.       							*/
/* << ------------------------------------------------------ >> */

// DonR 11Mar2024 User Story #297879: This is a new version of the program, intended
// to work *only* with ODBC (which will in effect mean only with MS-SQL, since Informix
// is finally being turned off). In order to accomplish this, I'm renaming the main
// source module from .ec to .c, and getting rid of macsql.ec and as400docin.ec.
// Most of the incoming transactions will be basically stubs - receiving data from
// AS/400, doing nothing with it, and telling the AS/400 client that everything is OK.


/*===============================================================
||				defines											||
 ===============================================================*/
#define MAIN
#define DOCTORS_SRC

char *GerrSource = __FILE__;

/*debug_level */
//#define AS400_DEBUG

#include <MacODBC.h>
#include "MsgHndlr.h"
#include "multix.h"
#include "global_1.h"
#include "As400UnixServer.h"
#include "DB.h"
#include "GenSql.h"
#include "tpmintr.h"
#include "DBFields.h"
#include "PharmDBMsgs.h"
#include <netinet/tcp.h>


struct sigaction	sig_act;
struct sigaction	sig_act_terminate;

void	pipe_handler		(int signo);
void	TerminateHandler	(int signo);

  
int		caught_signal	= 0;

int		TikrotProductionMode	= 1;	// To avoid "unresolved external" compilation error.

char	*PosInBuff;		// current pos in data buffer

static int rows_received	= 0;
static int rows_processed	= 0;

/*===============================================================
||																||
||				main											||
||																||
 ===============================================================*/
int main (int argc, char *argv[])
{
    fd_set			called_sockets;	/* sockets called during select	*/
    int				rc;		/* select() return code		*/
    int				data_len;	/* message len excluding header	*/
    bool			flg_sys_down;	/* if true -  sys going down	*/
    bool			flg_sys_busy;	/* if true -  system busy	*/
    bool			stop_on_err;	/* terminate proc with err code */
    int				retrys;		/* used by INIT_SON_PROCESS	*/
    struct timeval	timeout;
    TConnection		*cp;
    int				sock_max;
    int				i;
	sigset_t		NullSigset;


	/*===============================================================
	||																||
	||			initializations										||
	||																||
	 ===============================================================*/
    GetCommonPars (argc, argv, &retrys);

    RETURN_ON_ERR (InitSonProcess (argv[0], DOCAS400TOUNIX_TYPE, retrys, 0, DOCTOR_SYS));

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
//
//	if (sigaction (SIGSEGV, &sig_act_ODBC_SegFault, NULL) != 0)
//	{
//		GerrLogReturn (GerrId,
//						"Can't install signal handler for SIGSEGV" GerrErr,
//						GerrStr);
//	}


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

	/*===============================================================
	||			Initialize connection array							||
	===============================================================*/

	initialize_connection_array();

	/*===============================================================
	||			Make listen socket									||
	===============================================================*/

	stop_on_err = false;

	glbListenSocket = make_listen_socket ();

	if (glbListenSocket < 0)
	{
		stop_on_err = true;
	}

	/*===============================================================
	||																||
	||			Main loop											||
	||																||
	===============================================================*/

	flg_sys_down = false;

	while (stop_on_err == false && flg_sys_down == false)
	{
		/*===============================================================
		||			Handle client connections							||
		===============================================================*/
		FD_ZERO (&called_sockets);

		time_t curr_time = time (NULL);

		// Loop over connections
		for (sock_max = i = 0; i < MAX_NUM_OF_CLIENTS; i++)
		{
			cp = glbConnectionsArray + i;

			// Check only active connections
			if (cp->server_state == 0)
			{
				break;
			}

			// Close connections in "low-water-mark"
			if ((time (NULL) - cp->last_access) > 1200)
			{
				terminate_a_connection (cp, (cp->in_tran_flag == 1) ? WITH_ROLLBACK : WITHOUT_ROLLBACK);
				continue;
			}

			// Add connection socket to bit mask
			FD_SET (cp->socket, &called_sockets);

			sock_max = (sock_max > cp->socket) ? sock_max : cp->socket;
		}	// Loop over connections

		// Add listen socket to bit mask
		FD_SET (glbListenSocket, &called_sockets);

		sock_max = (sock_max > glbListenSocket) ? sock_max : glbListenSocket;

		// set timeout for select
		timeout.tv_sec	= SelectWait / 1000000;
		timeout.tv_usec	= SelectWait % 1000000;

		rc = select (	sock_max + 1,		// no. of bits to scan
						&called_sockets,	// sockets called for recv ()
						NULL,
						NULL,
						&timeout			// time to wait
					);

		switch (rc)
		{
			case -1:	// SELECT ERROR
						GerrLogMini (GerrId, "Select() error\n" GerrErr, GerrStr);
						stop_on_err = true;
						continue;

			default:	// CONNECTION REQUESTS
						// Update last access time for process
						state = UpdateLastAccessTime ();

						if (ERR_STATE (state))
						{
							GerrLogMini (GerrId, "Error while updating last access time of process");
						}

						if (FD_ISSET (glbListenSocket, &called_sockets))
						{
							// Create new connection
							if (create_a_new_connection () == failure)
							{
								GerrLogMini (GerrId, "Unable to create new client-connection. Error (%d) %s.", GerrStr);
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

    }	// while (stop_on_err == false && flg_sys_down == false)

	/*=======================================================================
	||			termination of process										||
	=======================================================================*/
	ExitSonProcess (MAC_SERV_SHUT_DOWN);

}	// End of main().


/* << ---------------------------------------------------------- >> */
/*																	*/
/*			Get length of a given message-structure					*/
/*																	*/
/* << ---------------------------------------------------------- >> */
static int get_struct_len (TSystemMsg msg)
{
	int	  i;

	for (i = 0; NOT_END_OF_TABLE (i);  i++)
	{
		if (msg == MessageSequence[i].ID)
		{
			return (MessageSequence[i].len);
		}
	}

	GerrLogMini (GerrId, "message %d not found in messages table...\n", msg);
	return -1;
}


/* << ---------------------------------------------------------- >> */
/*																	*/
/*			Does a socket connection to client still exist			*/
/*																	*/
/* << ---------------------------------------------------------- >> */
int pipe_is_broken (TSocket  sock, int milisec)
{
	struct timeval	wt;
	fd_set			rw;  /* Ready to Write */
	int				rc;


	FD_ZERO	(&rw);
	FD_SET	(sock, &rw);

	milisec *= 1000;

	wt.tv_sec  = milisec / 1000000;       /* seconds      */
	wt.tv_usec = milisec % 1000000;       /* microseconds */

	rc = select (sock + 1, NULL, &rw, NULL, &wt);

	if (rc < 1)
	{
		return 1;
	}
	else
	{
		return !FD_ISSET (sock, &rw);
	}
}


/* << ---------------------------------------------------------- >> */
/*																	*/
/*			Send a specific message to a single connected Client	*/
/*																	*/
/* << ---------------------------------------------------------- >> */
/*																	*/
/*			returns:  successs Or failure code						*/
/*																	*/
/* << ---------------------------------------------------------- >> */
static TSuccessStat inform_client (	TConnection	 	*connect,
									TSystemMsg   	msg,
									char			text_buffer[]	// DonR 06Feb2020: Added for compatibility with As400UnixServer.h
								  )
{
	char			to_send[64];
	char			*buf_ptr;
	int				len;
	bool			failure_flg;
	int				rc;
	fd_set			write_fds;
	struct timeval	timeout;

	// Get data to send
	sprintf (to_send, "%d", msg);
	len = strlen (to_send);

	ascii_to_ebcdic_dic (to_send, len);

	// Check if data can be sent
	if (pipe_is_broken (connect->socket, 1000))
	{
		GerrLogMini (GerrId, "the pipe is broken to as400");
		return failure;
	}

	// Send data to client
	buf_ptr = to_send;
	failure_flg = false;

	while (len && failure_flg == false)
	{
		rc = send (connect->socket, buf_ptr, len, 0);

		switch (rc)
		{
			case -1:	GerrLogMini (GerrId, "Error at send()\n" GerrErr, GerrStr);
						failure_flg = true;
						break;

			case 0:		GerrLogMini (GerrId, "(send) Connection closed by client - error (%d) %s.", GerrStr);
						failure_flg = true;
						break;

			default:	len -= rc;
						buf_ptr += rc;
		}

	} 		// while()

	return ((failure_flg == false) ? success : failure) ;
}


/* << ---------------------------------------------------------- >> */
/*																	*/
/*			close a socket & remove from socket-set					*/
/*																	*/
/* << ---------------------------------------------------------- >> */
static void close_socket (TSocket   sock)
{
    close (sock);
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

	close_socket (c->socket);

	GerrLogMini (GerrId, "Closing a connection to as400: Entry: %d Socket : %d",
				 c - glbConnectionsArray, c->socket);

	initialize_struct_to_not_connected (c) ;      /* reset as not connected */
}


/*===============================================================
||																||
||			initialize_struct_to_not_connected()				||
||																||
 ===============================================================*/
static void initialize_struct_to_not_connected (TConnection *c)
{
	c->server_state	= 0;
	c->in_tran_flag	= 0;
	c->last_access	= time(NULL);
	c->socket		= -1;
	c->ip_address	= -1L;
	c->sys_is_busy	= false;

	strcpy (c->db_connection_id, "-1");
}


/*===============================================================
||																||
||			receive_from_socket()								||
||																||
 ===============================================================*/
static TSuccessStat receive_from_socket	(	TConnection	*connect,
											char		*buf,
											int			msg_len
										)
{
	int				received_bytes;
	int				rc;
	fd_set			ready_for_recv;
	struct timeval	w_time;

	// Receive data from client
	while (msg_len > 0)
	{
		// Check if data ready to receive
		FD_ZERO	(&ready_for_recv);
		FD_SET	(connect->socket, &ready_for_recv);

		w_time.tv_sec	= 100;		 //ReadTimeout;
		w_time.tv_usec	= 0;

		if( pipe_is_broken( connect->socket, 1000 ) )
		{
			GerrLogMini (GerrId, "the pipe to as400 is broken..\n----------------");
			return failure;
		}

		//GerrLogMini (GerrId, "before select");    
		rc = select (connect->socket + 1, &ready_for_recv, 0, 0, &w_time);
		//GerrLogMini (GerrId,"after select");    

		switch (rc )
		{
			case -1:	GerrLogMini (GerrId,
									 "Select() error at function receive_from_socket. Error (%d) %s.",
									 GerrStr);
						return failure;

			case 0:		GerrLogMini (GerrId, "No data ready for receive");
						return failure;

			default:	if (!FD_ISSET (connect->socket, &ready_for_recv))
						{
							GerrLogMini (GerrId, "unexpected error in select()\n" GerrErr, GerrStr);
							return failure;
						}
						break;
		}		// switch

		// Receive data
		//GerrLogMini ( GerrId,"before recv [%d][%s]",msg_len,buf);    
		received_bytes = recv (connect->socket, buf, msg_len, 0);
		//GerrLogMini ( GerrId,"after recv");

		switch (received_bytes)
		{
			case -1:		// RECV ERROR
							GerrLogMini (GerrId, "recv () error at function receive_from_socket. Error (%d) %s.", GerrStr);
							return failure;

			case 0:			// CONNECTION CLOSED
							GerrLogMini (GerrId, "Connection closed by client");
							return failure;

			default:		// DATA ARRIVED
							//GerrLogMini ( GerrId,"after recv1 default buf[%s][%d]",buf,msg_len);    
							buf		+= received_bytes;
							msg_len	-= received_bytes;
							//GerrLogMini ( GerrId,"after recv2 default buf[%s][%d]",buf,msg_len);    
							break;
		}		// switch ( recived bytes

		//GerrLogMini ( GerrId,"after recv3 default buf[%s][%d]",buf,msg_len);   
	}	// while( message to come

	//GerrLogMini ( GerrId,"after recv4 default buf[%s][%d]",buf,msg_len);

	return success;
}


/* << ---------------------------------------------------------- >> */
/*																	*/
/*			Connection is not currently active						*/
/*																	*/
/* << ---------------------------------------------------------- >> */
static bool connection_is_not_in_transaction (TConnection *a_client)
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

	memset (glbMsgBuffer, 0, sizeof (glbMsgBuffer));

	if (receive_from_socket (c, glbMsgBuffer, MSG_HEADER_LEN) == failure)
	{
		return ERROR_ON_RECEIVE;
	}

	glbMsgBuffer [MSG_HEADER_LEN] = 0;

	numericID = atoi (glbMsgBuffer);

	if (!LEGAL_MESSAGE_ID (numericID))
	{
		return BAD_DATA_RECEIVED;
	}

	return (numericID);
}


/* << ---------------------------------------------------------- >> */
/*																	*/
/*			redefine an active connection as inactive				*/
/*																	*/
/* << ---------------------------------------------------------- >> */
static void set_connection_to_not_in_transaction (TConnection *c)
{
	c->in_tran_flag = 0;
}


/* << ---------------------------------------------------------- >> */
/*																	*/
/*			redefine an inactive connection as active				*/
/*																	*/
/* << ---------------------------------------------------------- >> */
static void set_connection_to_in_transaction_state (TConnection *client)
{
	client->in_tran_flag = 1;

	// DonR 26Jan2020: ODBC opens transactions automatically at the first DB
	// update; there is no such thing in ODBC as an explicit transaction-open.
//	// (But until everything is working in ODBC, we still need this!)
//	SQLMD_begin();
//if (SQLCODE) GerrLogMini (GerrId, "Ignore the preceding error!");
}


/* << ---------------------------------------------------------- >> */
/*																	*/
/*		get an unused connect-structure from glbConnectionsArray	*/
/*																	*/
/* << ---------------------------------------------------------- >> */
int	 find_free_connection (void)
{
	int	i;

	for (i = 0; i < MAX_NUM_OF_CLIENTS; i++)
	{
		if (glbConnectionsArray[i].server_state == 0)
		{
			return (i);
		}
	}

	return -1;
}


/*===============================================================
||																||
||			create_a_new_connection()							||
||																||
 ===============================================================*/
static TSuccessStat create_a_new_connection (void)
{
	TSocket			new_socket;
	TSocketAddress	client_sock_addr;
	TConnection		*a_connection;
	int				addr_len;
	int				rc;
	int				pos;		    /*position in array*/
    char			str_pos[32] ;
	int				i;
	int				nonzero = -1;
	struct protoent	*tcp_prot;

	// Free all connections
	for (i = 0; i < MAX_NUM_OF_CLIENTS; i++)
	{
		a_connection = glbConnectionsArray + i;

		if (a_connection->server_state == 1)
		{
			terminate_a_connection (a_connection, (a_connection->in_tran_flag == 1) ? WITH_ROLLBACK : WITHOUT_ROLLBACK);
		}
	}

	// Get free connection
	pos = find_free_connection ();

	if (pos == -1)
	{
		return failure;
	}

	a_connection = glbConnectionsArray + pos;

	// Connect to DB if necessary.
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

	// Accept tcp/ip connection.
	addr_len = sizeof (struct sockaddr);
	new_socket = accept (glbListenSocket, (struct sockaddr *)&client_sock_addr, &addr_len);

	if (new_socket < 0)
	{
		return failure;
	}

	// Set socket options
	tcp_prot = getprotobyname ("tcp");

	if (tcp_prot == NULL)
	{
		GerrLogMini (GerrId, "Unable to get protocol number for 'tcp'\n" GerrErr, GerrStr);
	}

	// Set socket IO options.
	if (setsockopt (new_socket, tcp_prot->p_proto, TCP_NODELAY, &nonzero, sizeof (nonzero)) == -1)
	{
		GerrLogMini (GerrId, "Unable to set listen-socket's I/O options." GerrErr, GerrStr);
	}

	// Set connection attributes
	a_connection->socket		= new_socket;
	a_connection->ip_address	= client_sock_addr.sin_addr.s_addr;
	a_connection->server_state	= 1;
	a_connection->last_access	= time (NULL);
	a_connection->in_tran_flag	= 0;
	a_connection->sql_err_flg	= 0;

	strcpy (a_connection->db_connection_id, str_pos);

	GerrLogMini (GerrId, "\nCreating a new connection to as400: Entry %d, Socket %d", pos, new_socket);

	return success;
}


/*===============================================================
||																||
||			initialize_connection_array()						||
||																||
 ===============================================================*/
static void initialize_connection_array (void)
{
	int	i;

	for (i = 0; i < MAX_NUM_OF_CLIENTS; i++)
	{
		initialize_struct_to_not_connected (glbConnectionsArray + i);
	}
}


/*===============================================================
||																||
||			make_listen_socket()								||
||																||
 ===============================================================*/
static TSocket make_listen_socket (void)
{
	struct servent	*sp;
	TSocketAddress	serveraddr;
	TSocket			l_socket;
	int				nonzero = -1;
	int				rc;
	struct protoent	*tcp_prot;

	// Get Socket
	l_socket = socket (AF_INET, SOCK_STREAM, 0);

	if (l_socket < 0)
	{
		GerrLogMini (GerrId, "Unable to create listen socket. Error (%d) %s.", GerrStr);
		return (l_socket);
	}

	// Set socket options
	tcp_prot = getprotobyname ("tcp");

	if (tcp_prot == NULL)
	{
		GerrLogMini (GerrId, "Unable to get protocol number for 'tcp'\n" GerrErr, GerrStr);
	}

	// Set socket IO options.
	if ((setsockopt (l_socket, SOL_SOCKET, SO_REUSEADDR, &nonzero, sizeof (nonzero))		== -1)	||
		(setsockopt (l_socket, SOL_SOCKET, SO_KEEPALIVE, &nonzero, sizeof (int))			== -1)	||
		(setsockopt (l_socket, tcp_prot->p_proto, TCP_NODELAY, &nonzero, sizeof (nonzero))	== -1))
	{
		GerrLogMini (GerrId, "Unable to set listen-socket's I/O options. " GerrErr, GerrStr);
	}

	// Bind socket
	bzero ((char *)&serveraddr, sizeof (serveraddr));
	serveraddr.sin_family		= AF_INET;
	serveraddr.sin_addr.s_addr	= htonl (INADDR_ANY);
	serveraddr.sin_port			= htons (AS400_UNIX_PORT); 

	rc = bind (l_socket, (struct sockaddr *)&serveraddr, sizeof (struct sockaddr_in));

	if (rc < 0)
	{
		GerrLogMini (GerrId, "Unable to bind() listen-socket. Error (%d) %s.", GerrStr);
		return (rc);
	}

	// Listen on socket
	rc = listen (l_socket, SOMAXCONN);

	if (rc < 0)
	{
		GerrLogMini (GerrId, "Failure at listen() at function make_listen_socket(). Error (%d) %s.", GerrStr);
		return (rc);
	}

	return (l_socket);
}


/* << ---------------------------------------------------------- >> */
/*																	*/
/*			Set active connection & rollback						*/
/*																	*/
/* << ---------------------------------------------------------- >> */
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


/* << ---------------------------------------------------------- >> */
/*																	*/
/*			handler function executing "commit"						*/
/*																	*/
/* << ---------------------------------------------------------- >> */
static int commit_handler	(	TConnection	*p_connect,
								TShmUpdate	shm_table_to_update	)
{
	int	save_flg = p_connect->sql_err_flg;

	p_connect->sql_err_flg = 0;

	// Update date of shared memory table if needed
	// DonR 11Mar2024: Disabling this - I don't think anybody has used
	// this shared-memory data for years.
//	UpdateSharedMem (shm_table_to_update);


	// pipe is broken -- rollback & close connection
	if (pipe_is_broken (p_connect->socket, 100))
	{
		GerrLogMini (GerrId, "the pipe to AS4000 is broken...");
		terminate_a_connection (p_connect, (p_connect->in_tran_flag) ? WITH_ROLLBACK : WITHOUT_ROLLBACK);
	}
	else

	// Sql error occured -- rollback & inform client
	if (save_flg)
	{
		#ifdef AS400_DEBUG
		printf("\n save_Flg");
		fflush(stdout);
		#endif // AS400_DEBUG

		DO_ROLLBACK (p_connect);

		if (inform_client (p_connect, CM_SQL_ERROR_OCCURED, "") == failure)
		{
			terminate_a_connection (p_connect, WITHOUT_ROLLBACK);
		}
	}
	else

	// Commit the transaction
	{
		#ifdef AS400_DEBUG
		printf("\n commit____");
		fflush(stdout);
		#endif // AS400_DEBUG

		CommitAllWork ();

		if (SQLCODE  == 0)
		{
			set_connection_to_not_in_transaction (p_connect);

			if (inform_client (p_connect, CM_CONFIRMING_COMMIT, "") == failure)
			{
				terminate_a_connection (p_connect, WITHOUT_ROLLBACK);
			}
		}
		else
		{
			terminate_a_connection (p_connect, WITH_ROLLBACK);
		}
	}	// Need to commit.

	return	MSG_HANDLER_SUCCEEDED ;
}


/* << ---------------------------------------------------------- >> */
/*																	*/
/*			handler function executing "rollback"					*/
/*																	*/
/* << ---------------------------------------------------------- >> */
static int rollback_handler (TConnection *p_connect)
{
	TSuccessStat	rc ;

	p_connect->sql_err_flg = 0;

	rc = DO_ROLLBACK (p_connect);

	if (rc == success)
	{
		if (inform_client (p_connect, CM_CONFIRMING_ROLLBACK, "") == failure)
		{
			terminate_a_connection (p_connect, WITH_ROLLBACK);
		}
	}
	else
	{
		terminate_a_connection (p_connect, WITH_ROLLBACK);
	}

	return MSG_HANDLER_SUCCEEDED;
}


/* << ---------------------------------------------------------- >> */
/*																	*/
/*																	*/
/*			M E S S A G E    H A N D L E R S						*/
/*																	*/
/*																	*/
/* << ---------------------------------------------------------- >> */


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


// handler for message 3008 - writes to hospital_member and updates
// members with hospitalization status.
// This one actually does something!
static int message_3008_handler (TConnection *p_connect, int data_len, bool do_insert)
{
	int				ReadInput;		/* amount of Bytesread from buffer  */
	int				retcode;		/* code returned from function	    */
	bool			stop_retrying;		/* if true - return from function   */
	int				n_retries;		/* retrying counter		    */
	int				SysDate;
	int				SysTime;
	int				HospMemb_DischDate;
	int				HospMemb_InsDate;
	short			Member_InHospital;
	short			MemberDied	= 0;
	THospitalRecord	HospitalRec;

	// DonR 21Jun2021 Bug #169132: Fields to read old values from hospital_member.
	int				o_entry_date;
	int				o_discharge_date;
	short			o_in_hospital;
	int				o_hospital_num;
	char			o_hospital_name		[25 + 1];
	char			o_department_name	[30 + 1];
	int				o_update_date;
	int				o_update_time;
	int				o_insertdate;
	short			SuppressObsoleteUpdate	= 0;


	SysDate = GetDate();
	SysTime = GetTime();

	// read input buffer
	INIT_ERROR_DETECTION_MECHANISM;
	PosInBuff = glbMsgBuffer;

	HospitalRec.Contractor		= get_intv	(&PosInBuff,	IdNumber_9_len);
	HospitalRec.MemberId.Code	= get_short	(&PosInBuff,	1);
	HospitalRec.MemberId.Number	= get_intv	(&PosInBuff,	IdNumber_9_len);
	HospitalRec.HospDate		= get_intv	(&PosInBuff,	DateYYYYMMDD_len);
	HospitalRec.NurseDate		= get_intv	(&PosInBuff,	DateYYYYMMDD_len);
	get_string (&PosInBuff, HospitalRec.ContrName,			30);
	HospitalRec.HospitalNumb	= get_intv	(&PosInBuff,	ReqId_9_len);
	get_string (&PosInBuff, HospitalRec.Result,				1120);
	get_string (&PosInBuff, HospitalRec.HospName,			25);
	get_string (&PosInBuff, HospitalRec.DepartName,			30);
	HospitalRec.FlgSystem		= get_short	(&PosInBuff,	1);

	// DonR 25Nov2021 User Story #206812 - add another "doctor TZ", used
	// only to indicate member's death in hospital (value = 3).
	HospitalRec.SecondDoctorTZ	= get_intv	(&PosInBuff, 9);

	// End of incoming message.

	// verify all data was read:
	ReadInput = PosInBuff - glbMsgBuffer;
	StaticAssert (ReadInput == data_len);

	// If we don't actually need to write anything to the database,
	// default to successful return code MSG_HANDLER_SUCCEEDED.
	retcode = MSG_HANDLER_SUCCEEDED;

	// DonR 01Aug2011: Write member hospitalization information to hospital_member and update
	// the in_hospital flag in the members table. Note that since each hospitalization record
	// is sent twice from AS/400, once with flg_system = 0 and once with flg_system = 1, we
	// execute this logic only for one of them.
	if (HospitalRec.FlgSystem == 1)
	{
		// Set in-hospital/member-dead flags and dates.
		if (HospitalRec.NurseDate <= 20000000)
		{
			// This member is currently a hospital inpatient.
			HospMemb_DischDate	= 0;
			HospMemb_InsDate	= 21991231;	// High value to prevent auto-purging of rows for hospitalized members.
			Member_InHospital	= 1;		// Member is in hospital.
			MemberDied			= 0;		// If member is still an inpatient, s/he has not died.
		}
		else
		{
			// This member has been discharged from hospital.
			HospMemb_DischDate	= HospitalRec.NurseDate;
			HospMemb_InsDate	= HospitalRec.NurseDate;	// Rows will be purged based on Discharge Date.

			// DonR 23Aug2011: If this is a non-official release from hospital (i.e. a "vacation"),
			// the Contractor Teudat Zehut will be zero. In this case, use status 3 rather than 2,
			// just to keep things clear. (They're functionally equivalent as far as pharmacies
			// are concerned.)
			Member_InHospital	= (HospitalRec.Contractor		!= 0) ? 2 : 3;	// Discharged or merely "absent".

			// DonR 25Nov2021 User Story #206812: Add second "doctor TZ" to download, with a value of 3
			// (on discharge) indicating that the member died in hospital.
			MemberDied			= (HospitalRec.SecondDoctorTZ	== 3) ? 1 : 0;	// Discharged dead or discharged alive.

			// As of 31 July 2011, AS/400 allows entry of bogus discharge dates. If the discharge date
			// is later than today, write out a diagnostic message - but since we don't know what the
			// true discharge date is, write the bogus date to hospital_member as received.
			if (HospMemb_DischDate > SysDate)
			{
				GerrLogMini (GerrId,"Trn. 3008: Got future discharge date of %d for member %d/%d!",
								HospMemb_DischDate, HospitalRec.MemberId.Number, HospitalRec.MemberId.Code);
			}

		}	// This member has been discharged from hospital.


		// up to SQL_UPDATE_RETRIES retries will be made to overcome access conflict
		retcode = ALL_RETRIES_WERE_USED;
		stop_retrying = false;

		for (n_retries = 0; (n_retries < SQL_UPDATE_RETRIES) && (stop_retrying == false); n_retries++)
		{

			// sleep for a while.
			if (n_retries > 0)
			{
				sleep (SLEEP_TIME_BETWEEN_RETRIES);
			}

			SuppressObsoleteUpdate = 0;	// DonR 22Jun2021: Paranoid re-initialization.


			// DonR 21Jun2021 Bug Fix #169132: In some cases, when a member moves from one hospital to another
			// in the same day (or even if there's short interval between hospitalizations), we get the
			// disharge information from the earlier hospitalization *after* we get the update for the new
			// hospitalization - and in that case, we flag the member as "discharged" even though s/he is
			// in fact still in hospital. In these cases, we need to suppress the update of the late-arriving
			// discharge message and leave the existing status - which is, in fact, more current - intact.
			// First, read the information we stored about the member's current hospitalization status from
			// hospital_member.
			ExecSQL (	MAIN_DB, AS400UDS_T3008_READ_hospital_member,
						&o_entry_date,			&o_discharge_date,		&o_in_hospital,
						&o_hospital_num,		&o_hospital_name,		&o_department_name,
						&o_update_date,			&o_update_time,			&o_insertdate,

						&HospitalRec.MemberId.Number, &HospitalRec.MemberId.Code,	END_OF_ARG_LIST		);

			// If the hospital_member read fails, log the error but don't do more than that - this SQL
			// should really never throw errors, as there's almost zero "traffic" for this table.
			if (SQLCODE == 0)
			{
				// The new data should be obsolete if it's for a hospital stay that began *before* the current hospital stay.
				// Note that while in reality obsolete updates seem to happen only for discharges, this logic will also
				// prevent writing old hospitalizations on top of more current information.
				if (HospitalRec.HospDate < o_entry_date)
				{
					// This update looks like it's no longer relevant, so set flag to suppress members/hospital_member updates.
					SuppressObsoleteUpdate = 1;

					GerrLogMini (	GerrId, "3008 ignoring %d's %d-%d %s - newer hospital stay began %d.",
									HospitalRec.MemberId.Number, HospitalRec.HospDate, HospMemb_DischDate,
									(Member_InHospital == 1) ? "admittance" : ((Member_InHospital == 3) ? "absence" : "discharge"),
									o_entry_date);
//
//					GerrLogFnameMini (	"strange_discharges_log", GerrId,
//										"Trn. 3008: Ignoring %s TZ %d's %d-%d hospital stay - newer hospitalization started %d.",
//										(Member_InHospital == 1) ? "admittance to" : ((Member_InHospital == 3) ? "absence from" : "discharge from"),
//										Hosp->MemberId.Number, Hosp->HospDate, HospMemb_DischDate, o_entry_date);
				}	// "New" discharge data appears to be obsolete, as a new hospital stay has already started.
			}
			else
			{
				// "Not found" isn't really an error - or at least it's not worth logging, even though it
				// would be somewhat strange to get a discharge message and not find previous data in
				// hospital_member.
				// Note that since hospital_member is not actually used for anything - it's really there
				// just so humans can see some detail if a case needs to be investigated - we don't need
				// any real error handling for the hospital_member SQL operations.
				if (SQLERR_code_cmp (SQLERR_not_found) != MAC_TRUE)
				{
					GerrLogMini (	GerrId, "AS400UDS_T3008_READ_hospital_member for %d returned SQLCODE %d.",
									HospitalRec.MemberId.Number, SQLCODE	);
					GerrLogFnameMini (	"strange_discharges_log", GerrId,
										"\nAS400UDS_T3008_READ_hospital_member for %d returned SQLCODE %d.",
										HospitalRec.MemberId.Number, SQLCODE	);
				}
			}	// Error reading hospital_member.


			// DonR 22Jun2021 Bug #169132 (continued): If the new data is a discharge from a previous hospitalization
			// for a member who has already started a new hospitalization, do *not* update the member's status or write
			// the new data to hospital_member - what is already in those tables is more current than the "new" data.
			if (!SuppressObsoleteUpdate)
			{
				rows_processed++;

				// Per Yulia's suggestion, execute a DELETE (with no error-checking) followed by an INSERT,
				// rather than worrying about insert-or-update logic.
				ExecSQL (	MAIN_DB, AS400UDS_T3008_DEL_hospital_member,
							&HospitalRec.MemberId.Number, &HospitalRec.MemberId.Code, END_OF_ARG_LIST	);

				// DonR 23Aug2011: Added in_hospital to hospital_member table, just for clarity.
				// Note that Discharge Date may be different from Hosp->NurseDate.
				ExecSQL (	MAIN_DB, AS400UDS_T3008_INS_hospital_member,
							&HospitalRec.MemberId.Number,	&HospitalRec.MemberId.Code,
							&HospitalRec.HospDate,			&HospMemb_DischDate,
							&Member_InHospital,				&HospitalRec.HospitalNumb,
							&HospitalRec.HospName,			&HospitalRec.DepartName,
							&SysDate,						&SysTime,
							&HospMemb_InsDate,				&MemberDied,
							END_OF_ARG_LIST													);

				// Since hospital_member is not used by any online programs, don't bother checking for
				// access-conflict errors. Also, don't abort - we still want to flag the member, since
				// that's the important part of this whole business!
				if (SQLERR_error_test ())
				{
					GerrLogMini (GerrId,"Trn. 3008: Error %d inserting to hospital_member.", sqlca.sqlcode);
				}

				// Update members table. Note that no-rows-affected is OK here. If somehow a member joins Maccabi
				// from his/her hospital bed and we receive the hospitalization record before we get the member
				// update, the later INSERT to members will check for the existence of a hospital_member row and
				// set the in_hospital flag accordingly.
				ExecSQL (	MAIN_DB, AS400UDS_T3008_UPD_members,
							&Member_InHospital,				&MemberDied,
							&HospitalRec.MemberId.Number,	&HospitalRec.MemberId.Code,
							END_OF_ARG_LIST													);

				// No-rows-affected is an OK result in this case.
				if ((SQLCODE == 0) || (SQLERR_code_cmp (SQLERR_no_rows_affected) == MAC_TRUE))
				{
					retcode			= MSG_HANDLER_SUCCEEDED;	// No error.
					stop_retrying	= true;
					continue;
				}
				else
				{
					if (SQLERR_code_cmp (SQLERR_access_conflict) == MAC_TRUE)
					{
						continue;
					}

					if (SQLERR_error_test ())
					{
						GerrLogMini (GerrId,"Trn. 3008: Error %d updating member %d/%d.",
										sqlca.sqlcode, HospitalRec.MemberId.Number, HospitalRec.MemberId.Code);

						stop_retrying	= true;
						retcode			= SQL_ERROR_OCCURED;
						continue;
					}
				}	// Something other than OK or no-rows-affected.
			}	// DonR 22Jun2021 Bug Fix #169132 end - SuppressObsoleteUpdate is set FALSE.

			else
			{	// SuppressObsoleteUpdate is NOT zero.
				// If we didn't have to write anything to the database, that's an OK result.
				stop_retrying	= true;
				retcode			= MSG_HANDLER_SUCCEEDED;	// No error.
				continue;
			}

		}	// SQL retry loop.

	}	// Record is a hospitalization notification with flg_system = 1.

	return (retcode);
}
// end of msg 3008 handler


// handler for message 3022 - insert to table LabResOther from mokeds
// This one actually does something!
static int message_3022_handler (TConnection *p_connect, int data_len, bool do_insert)
{
	int			ReadInput;							// Number of Bytes read from buffer
	int			retcode	= MSG_HANDLER_SUCCEEDED;	// Default = no errors.
	int			v_count;
	bool		stop_retrying;
	int			n_retries;
	TLabRecord	LabResRec;
	char 		YesNo	[2];
	short		Contr_N;	// 1 == write to LabResOther. All other values are now discarded.
	int			group_num_id;
	int			reqid_main;
	int			sysdate;
	int			systime;

	sysdate = GetDate();
	systime = GetTime();

	// read input buffer
	INIT_ERROR_DETECTION_MECHANISM;
	PosInBuff = glbMsgBuffer;

	LabResRec.MemberId.Code		= get_short	(&PosInBuff,	1);					RETURN_ON_ERROR;
	LabResRec.MemberId.Number	= get_long	(&PosInBuff,	IdNumber_9_len);	RETURN_ON_ERROR;
	LabResRec.LabId				= get_short	(&PosInBuff,	3);					RETURN_ON_ERROR;
	LabResRec.Route				= get_long	(&PosInBuff,	9);					RETURN_ON_ERROR;
	LabResRec.ReqId				= get_long	(&PosInBuff,	ReqId_9_len);		RETURN_ON_ERROR;
	LabResRec.ReqCode			= get_long	(&PosInBuff,	ReqCode_5_len);		RETURN_ON_ERROR;
	LabResRec.Result			= get_long	(&PosInBuff,	Result_10_len);		RETURN_ON_ERROR;
	get_string (&PosInBuff, LabResRec.Units,				Units_15_len);		// Result units
	LabResRec.LowerLimit		= get_long	(&PosInBuff,	Limit_10_len);		RETURN_ON_ERROR;
	LabResRec.UpperLimit		= get_long	(&PosInBuff,	Limit_10_len);		RETURN_ON_ERROR;
	get_string (&PosInBuff, &YesNo[0],						1);					// Pregnant Woman
	get_string (&PosInBuff, &YesNo[1],						1);					// Completed
	LabResRec.ApprovalDate		= get_long	(&PosInBuff,	DateYYYYMMDD_len);	RETURN_ON_ERROR;
	LabResRec.ForwardDate		= get_long	(&PosInBuff,	DateYYYYMMDD_len);	RETURN_ON_ERROR;
	LabResRec.ReqDate			= get_long	(&PosInBuff,	DateYYYYMMDD_len);	RETURN_ON_ERROR;
	LabResRec.Contractor		= get_long	(&PosInBuff,	IdNumber_9_len);	RETURN_ON_ERROR;
	LabResRec.TermId			= get_long	(&PosInBuff,	TermId_9_len);		RETURN_ON_ERROR;
	group_num_id				= get_intv	(&PosInBuff,	5);					RETURN_ON_ERROR;
	Contr_N						= get_short	(&PosInBuff,	2);					RETURN_ON_ERROR;
  
	// verify all data was read
	ReadInput = PosInBuff - glbMsgBuffer;
	StaticAssert (ReadInput == data_len);

	LabResRec.PregnantWoman	= (YesNo[0] == 'Y') ? 1 : 0;
	LabResRec.Completed		= (YesNo[1] == 'Y') ? 1 : 0;

	if (LabResRec.ApprovalDate	< 1000000)
		LabResRec.ApprovalDate	+= 19000000; 

	if (LabResRec.ForwardDate	< 1000000)
		LabResRec.ForwardDate	+= 19000000;

	if (LabResRec.ReqDate		< 1000000)
		LabResRec.ReqDate		+= 19000000;

	reqid_main = LabResRec.ReqId % 100000000;

	// Paranoid re-initialization to default (== no error).
	retcode	 = MSG_HANDLER_SUCCEEDED;


	// Write first-contractor rows to the LabResOther table - ignore the rest.
	if (Contr_N == 1)
	{
		// up to SQL_UPDATE_RETRIES retries will be made to overcome access conflict
		retcode = ALL_RETRIES_WERE_USED;	// Now that we have to look at the database, change the default.
		stop_retrying = false;

		for (n_retries = 0; (n_retries < SQL_UPDATE_RETRIES) && (stop_retrying == false); n_retries++)
		{
			// Sleep for a while before retrying.
			if (n_retries > 0)
			{
				sleep (SLEEP_TIME_BETWEEN_RETRIES);
			}

			// Check to see if this lab result is already in the table.
			ExecSQL (	MAIN_DB, DbInsertLabResultOtherRecord_get_labresother_count,
						&v_count,
						&LabResRec.MemberId.Number,		&LabResRec.MemberId.Code,		&LabResRec.ReqCode,
						&LabResRec.LabId,				&LabResRec.Route,				&LabResRec.Result,
						&LabResRec.LowerLimit,			&LabResRec.UpperLimit,			&LabResRec.ReqDate,
						&LabResRec.PregnantWoman,		&LabResRec.Completed,			&LabResRec.ReqId,
						END_OF_ARG_LIST																			);

			if (SQLERR_code_cmp (SQLERR_access_conflict) == MAC_TRUE)
			{
				continue;
			}

			if (SQLERR_error_test ())
			{
				GerrLogMini (GerrId,"Trn. 3022: Error %d looking for lab result for member %d ReqID %d.",
								SQLCODE, LabResRec.MemberId.Number, LabResRec.ReqId);

				stop_retrying	= true;
				retcode			= SQL_ERROR_OCCURED;
				continue;
			}

			// If the result is already there, don't bother adding it.
			if (v_count == 0)
			{
				rows_processed++;

				ExecSQL (	MAIN_DB, DbInsertLabResultOtherRecord_INS_labresother,
							&LabResRec.Contractor,		&LabResRec.MemberId.Code, 
							&LabResRec.MemberId.Number,	&LabResRec.LabId, 	
							&LabResRec.ReqId,			&LabResRec.ReqCode,
							&LabResRec.Route,			&LabResRec.Result, 	
							&LabResRec.LowerLimit,		&LabResRec.UpperLimit, 	
							&LabResRec.Units,			&LabResRec.ApprovalDate,
							&LabResRec.ForwardDate,		&LabResRec.ReqDate, 
							&LabResRec.PregnantWoman,	&LabResRec.Completed,	
							&LabResRec.TermId,			&reqid_main,
							&group_num_id,				&sysdate,
							&systime,					END_OF_ARG_LIST					);

				if (SQLCODE == 0)
				{
					retcode			= MSG_HANDLER_SUCCEEDED;	// No error.
					stop_retrying	= true;
					continue;
				}
				else
				{
					if (SQLERR_code_cmp (SQLERR_access_conflict) == MAC_TRUE)
					{
						continue;
					}

					if (SQLERR_error_test ())
					{
						GerrLogMini (GerrId,"Trn. 3022: Error %d adding lab result for member %d ReqID %d.",
										SQLCODE, LabResRec.MemberId.Number, LabResRec.ReqId);

						stop_retrying	= true;
						retcode			= SQL_ERROR_OCCURED;
						continue;
					}	// Unexpected error on INSERT to LabResOther.
				}	// Something other than a successful INSERT.
			}	// LabResOther row wasn't already in the table, so we need to INSERT it.

			else	// Row is already in LabResOther, so we just declare victory and go home.
			{
				retcode			= MSG_HANDLER_SUCCEEDED;	// No error.
				stop_retrying	= true;
				continue;
			}	// No need to INSERT a redundant LabResOther row.

		}	// SQL retry loop.
	}	// This is a first-contractor row, so we (potentially) add to LabResOther.

	return retcode;
}
// end of msg 3022 handler


// All the remaining message handlers are "stubs" - they just take data from
// AS/400 and return MSG_HANDLER_SUCCEEDED without actually doing anything.


// Message 3003 (TempMemb) - no database updates.
static int message_3003_handler (TConnection *p_connect, int data_len, bool do_insert)
{
	TTempMembRecord	TempMembRec;

	// read input buffer
	INIT_ERROR_DETECTION_MECHANISM;
	PosInBuff = glbMsgBuffer;

	TempMembRec.CardId			= get_long	(&PosInBuff, IdNumber_9_len);
	TempMembRec.MemberId.Code	= get_short	(&PosInBuff, 1);
	TempMembRec.MemberId.Number	= get_long	(&PosInBuff, IdNumber_9_len);
	TempMembRec.ValidUntil		= get_long	(&PosInBuff, DateYYYYMMDD_len);

	return MSG_HANDLER_SUCCEEDED;
}
// end of msg 3003 handler


// Message 3004 (delete from table TempMemb) - no database updates.
static int message_3004_handler (TConnection *p_connect, int data_len)
{
	TIdNumber	CardId;

	// read input buffer
	INIT_ERROR_DETECTION_MECHANISM;
	PosInBuff = glbMsgBuffer;

	CardId	= get_long (&PosInBuff, IdNumber_9_len);

	return MSG_HANDLER_SUCCEEDED;
}
// end of msg 3004 handler


// Message 3005 - update to table AUTH - no database updates.
static int message_3005_handler (TConnection *p_connect, int data_len, bool do_update)
{
	TMemberId	MemberId;
	TIdNumber	Contractor;

	// read input buffer
	INIT_ERROR_DETECTION_MECHANISM;
	PosInBuff = glbMsgBuffer;

	Contractor		= get_long	(&PosInBuff, IdNumber_9_len);
	MemberId.Code	= get_short	(&PosInBuff, 1);
	MemberId.Number	= get_long	(&PosInBuff, IdNumber_9_len);

	return MSG_HANDLER_SUCCEEDED;
}
// end of msg 3005 handler


// handler for message 3006 - insert to table CONTRACT, CONTERM,
//                                            TERMINAL, SUBST - no database updates.
static int message_3006_handler (TConnection *p_connect, int data_len, bool do_update)
{
	TContractRecord		ContractRecord;
	TContermRecord		ContermRecord;
	TSubstRecord		SubstRecord;

	// read input buffer
	INIT_ERROR_DETECTION_MECHANISM;
	PosInBuff = glbMsgBuffer;

	ContractRecord.Contractor		= get_long	(&PosInBuff,	IdNumber_9_len);
	ContractRecord.ContractType		= get_short	(&PosInBuff,	ContractType_2_len);
	get_string ( &PosInBuff, ContractRecord.FirstName,			FirstName_8_len);
	get_string ( &PosInBuff, ContractRecord.LastName,			LastName_14_len);
	get_string ( &PosInBuff, ContractRecord.Street,				Street_16_len);
	get_string ( &PosInBuff, ContractRecord.City,				City_20_len);
	get_string ( &PosInBuff, ContractRecord.Phone,				Phone_10_len);
	ContractRecord.Mahoz			= get_short	(&PosInBuff,	Mahoz_len);
	ContractRecord.Profession		= get_short	(&PosInBuff,	2);
	ContractRecord.AcceptUrgent		= get_short	(&PosInBuff,	1);
	ContractRecord.SexesAllowed		= get_short	(&PosInBuff,	1);
	ContractRecord.AcceptAnyAge		= get_short	(&PosInBuff,	1);
	ContractRecord.AllowKbdEntry	= get_short	(&PosInBuff,	3);
	ContractRecord.NoShifts			= get_short	(&PosInBuff,	1);
	ContractRecord.IgnorePrevAuth	= get_short	(&PosInBuff,	1);
	ContractRecord.AuthorizeAlways	= get_short	(&PosInBuff,	1);
	ContractRecord.NoAuthRecord		= get_short	(&PosInBuff,	1);
	ContermRecord.TermId			= get_long	(&PosInBuff,	TermId_7_len);
	ContermRecord.AuthType			= get_short	(&PosInBuff,	1);
	ContermRecord.ValidFrom			= get_long	(&PosInBuff,	DateYYYYMMDD_len);
	ContermRecord.ValidUntil		= get_long	(&PosInBuff,	DateYYYYMMDD_len);
	ContermRecord.Location			= get_long	(&PosInBuff,	LocationId_7_len);
	ContermRecord.Mahoz				= get_short	(&PosInBuff,	Mahoz_len);
	ContermRecord.Snif				= get_long	(&PosInBuff,	5);
	ContermRecord.Locationprv		= get_long	(&PosInBuff,	LocationId_7_len);
	get_string (&PosInBuff, ContermRecord.Position,				1);
	get_string (&PosInBuff, ContermRecord.Paymenttype,			1);
	ContermRecord.ServiceType		= get_short	(&PosInBuff,	3);
	SubstRecord.AltContractor		= get_long	(&PosInBuff,	IdNumber_9_len);
	SubstRecord.SubstType			= get_short	(&PosInBuff,	2);
	SubstRecord.DateFrom			= get_long	(&PosInBuff,	DateYYYYMMDD_len);
	SubstRecord.DateUntil			= get_long	(&PosInBuff,	DateYYYYMMDD_len);
	SubstRecord.SubstReason			= get_short	(&PosInBuff,	2);
	ContermRecord.City				= get_long	(&PosInBuff,	9);
	ContermRecord.Add1				= get_long	(&PosInBuff,	9);
	ContermRecord.Add2				= get_long	(&PosInBuff,	9);
	ContermRecord.interface			= get_short	(&PosInBuff,	2);
	get_string (&PosInBuff, ContermRecord.owner,				1);
	ContermRecord.group8			= get_long	(&PosInBuff,	8);
	get_string (&PosInBuff, ContermRecord.group_c,				1);
	ContermRecord.group_join		= get_long	(&PosInBuff,	7);
	ContermRecord.group_act			= get_long	(&PosInBuff,	7);
	ContermRecord.tax				= get_double(&PosInBuff,	10);
	ContermRecord.assign			= get_short	(&PosInBuff,	2);
	ContermRecord.gnrl_days			= get_short	(&PosInBuff,	1);
	ContermRecord.act				= get_short	(&PosInBuff,	3);
	ContermRecord.from_months		= get_short	(&PosInBuff,	4);
	ContermRecord.to_months			= get_short	(&PosInBuff,	4);
	ContermRecord.flg_cont			= get_short	(&PosInBuff,	1);
	ContractRecord.prof2			= get_short	(&PosInBuff,	2);
	ContractRecord.prof3			= get_short	(&PosInBuff,	2);
	ContractRecord.prof4			= get_short	(&PosInBuff,	2);

	return MSG_HANDLER_SUCCEEDED;
}
// end of msg 3006 handler


// Message 3007 - delete from table CONTERM - no database updates.
static int message_3007_handler (TConnection *p_connect, int data_len)
{
	TIdNumber		Contractor;
	TContractType	ContractType;
	TLocationId		Location;
	TTermId			TermId;


	// read input buffer
	INIT_ERROR_DETECTION_MECHANISM;
	PosInBuff = glbMsgBuffer;

	Contractor		= get_long	(&PosInBuff, IdNumber_9_len);
	ContractType	= get_short	(&PosInBuff, ContractType_2_len);
	TermId			= get_long	(&PosInBuff, TermId_7_len);
	Location		= get_long	(&PosInBuff, Location_7_len);

	return MSG_HANDLER_SUCCEEDED;
}
// end of msg 3007 handler


// Message 3009 - insert to/delete from table CONMEMB (for dieticians only) - no database updates.
static int message_3009_handler (TConnection *p_connect, int data_len, bool do_insert_NIU)
{
	short			DB_action;		// 1 = Delete, 2 = Insert
	TConMembRecord	ConMembRec;

	// read input buffer
	INIT_ERROR_DETECTION_MECHANISM;
	PosInBuff = glbMsgBuffer;

	DB_action					= get_short	(&PosInBuff, 1					); RETURN_ON_ERROR;
	ConMembRec.MemberId.Code	= get_short	(&PosInBuff, IdCode_1_len		); RETURN_ON_ERROR;
	ConMembRec.MemberId.Number	= get_long	(&PosInBuff, IdNumber_9_len		); RETURN_ON_ERROR;
	ConMembRec.Contractor		= get_long	(&PosInBuff, IdNumber_9_len		); RETURN_ON_ERROR;
	ConMembRec.ValidFrom		= get_long	(&PosInBuff, DateYYYYMMDD_len	); RETURN_ON_ERROR;

	return MSG_HANDLER_SUCCEEDED;
}
// end of msg 3009 handler


// Message 3010 - insert to table Hospitalmsg (pathology) - no database updates.
static int message_3010_handler (TConnection *p_connect, int data_len, bool do_insert)
{
	THospitalRecord	HospitalRec;

	// read input buffer
	INIT_ERROR_DETECTION_MECHANISM;
	PosInBuff = glbMsgBuffer;

	HospitalRec.MemberId.Code	= get_short	(&PosInBuff,	1);
	HospitalRec.MemberId.Number	= get_intv	(&PosInBuff,	IdNumber_9_len);
	HospitalRec.Contractor		= get_intv	(&PosInBuff,	IdNumber_9_len);
	HospitalRec.HospitalNumb	= get_intv	(&PosInBuff,	ReqId_9_len);
	get_string (&PosInBuff, HospitalRec.ResName1,			30);
	HospitalRec.HospDate		= get_intv	(&PosInBuff,	DateYYYYMMDD_len);
	HospitalRec.NurseDate		= get_intv	(&PosInBuff,	DateYYYYMMDD_len);
	get_string (&PosInBuff, HospitalRec.TypeResult,			25);
	get_string (&PosInBuff, HospitalRec.ContrName,			30);
	HospitalRec.PatFlg			= get_short	(&PosInBuff,	1);
	get_string (&PosInBuff, HospitalRec.Result,				2048);

	return MSG_HANDLER_SUCCEEDED;
}
// end of msg 3010 handler


// Message 3011 - insert to table SpeechClient - no database updates.
static int message_3011_handler (TConnection *p_connect, int data_len, bool do_insert)
{
	TSpeechClientRecord	SpeechClientRecord;

	// read input buffer
	INIT_ERROR_DETECTION_MECHANISM;
	PosInBuff = glbMsgBuffer;

	SpeechClientRecord.MemberId.Code	= get_short	(&PosInBuff, 1);
	SpeechClientRecord.MemberId.Number	= get_intv	(&PosInBuff, IdNumber_9_len);
	SpeechClientRecord.Location			= get_intv	(&PosInBuff, LocationId_7_len);
	SpeechClientRecord.FromDate			= get_intv	(&PosInBuff, DateYYYYMMDD_len);
	SpeechClientRecord.ToDate			= get_intv	(&PosInBuff, DateYYYYMMDD_len);

	return MSG_HANDLER_SUCCEEDED;
}
// end of msg 3011 handler


// Message 3012 - insert to table rasham - no database updates.
static int message_3012_handler (TConnection *p_connect, int data_len, bool do_insert)
{
	TRashamRecord	RashamRecord;

	// read input buffer
	INIT_ERROR_DETECTION_MECHANISM;
	PosInBuff = glbMsgBuffer;

	RashamRecord.MemberId.Code		= get_short	(&PosInBuff,	2);
	RashamRecord.MemberId.Number	= get_intv	(&PosInBuff,	IdNumber_9_len);
	RashamRecord.Contractor			= get_intv	(&PosInBuff,	IdNumber_9_len);
	RashamRecord.ContractType		= get_short	(&PosInBuff,	2);
	RashamRecord.KodRasham			= get_short	(&PosInBuff,	2);
	get_string (&PosInBuff, RashamRecord.Rasham,				30);
	RashamRecord.TermId				= get_intv	(&PosInBuff,	TermId_7_len);
	RashamRecord.UpdateDate			= get_intv	(&PosInBuff,	DateYYYYMMDD_len);

	return MSG_HANDLER_SUCCEEDED;
}
// end of msg 3012 handler


// Message 3013 - insert to Member Message - no database updates.
static int message_3013_handler (TConnection *p_connect, int data_len, bool do_insert)
{
	TMemberMessageRecord	MemberMessageRecord;
	char					message_text [41];

	// read input buffer
	INIT_ERROR_DETECTION_MECHANISM;
	PosInBuff = glbMsgBuffer;

	MemberMessageRecord.MemberId.Code	= get_short	(&PosInBuff,	1);
	MemberMessageRecord.MemberId.Number	= get_intv	(&PosInBuff,	IdNumber_9_len);
	get_string (&PosInBuff, message_text,							40);

	return MSG_HANDLER_SUCCEEDED;
}
// end of msg 3013 handler


// Message 3014 - insert to Member Mammogram - no database updates.
static int message_3014_handler (TConnection *p_connect, int data_len, bool do_insert)
{
	TMemberId	MemberId;
	int			BirthDate;

	// read input buffer
	INIT_ERROR_DETECTION_MECHANISM;
	PosInBuff = glbMsgBuffer;

	MemberId.Code	= get_short (&PosInBuff, 1);
	MemberId.Number	= get_intv	(&PosInBuff, IdNumber_9_len);
	BirthDate		= get_intv	(&PosInBuff, 8);

	return MSG_HANDLER_SUCCEEDED;
}
// end of msg 3014 handler


// Message 3015 - insert to Member "Sapat" - no database updates.
static int message_3015_handler (TConnection *p_connect, int data_len, bool do_insert)
{
	TMemberId	MemberId;

	// read input buffer
	INIT_ERROR_DETECTION_MECHANISM;
	PosInBuff = glbMsgBuffer;

	MemberId.Number	= get_intv	(&PosInBuff, IdNumber_9_len);
	MemberId.Code	= get_short	(&PosInBuff, 1);

	return MSG_HANDLER_SUCCEEDED;
}
// end of msg 3015 handler


// Message 3016 - update to Member "Sapat" - no database updates.
static int message_3016_handler (TConnection *p_connect, int data_len, bool do_update)
{
	TMemberId	MemberId;

	// read input buffer
	INIT_ERROR_DETECTION_MECHANISM;
	PosInBuff = glbMsgBuffer;

	MemberId.Number	= get_intv	(&PosInBuff, IdNumber_9_len);
	MemberId.Code	= get_short	(&PosInBuff, 1);

	return MSG_HANDLER_SUCCEEDED;
}
// end of msg 3016 handler


// Message 3017 - insert to Member Remark - no database updates.
static int message_3017_handler (TConnection *p_connect, int data_len, bool do_insert)
{
	TMemberId	MemberId;
	int			v_validuntil;
	short		v_remark;

	// read input buffer
	INIT_ERROR_DETECTION_MECHANISM;
	PosInBuff = glbMsgBuffer;

	MemberId.Number	= get_intv	(&PosInBuff, IdNumber_9_len);
	MemberId.Code	= get_short	(&PosInBuff, 1);
	v_remark		= get_short	(&PosInBuff, 4);
	v_validuntil	= get_long	(&PosInBuff, DateYYYYMMDD_len);

	return MSG_HANDLER_SUCCEEDED;
}
// end of msg 3017 handler


// Message 3018 - insert to Clicks Tables Updated - no database updates.
static int message_3018_handler (TConnection *p_connect, int data_len, bool do_insert)
{
	TClicksTblUpdRecord ClicksTblRec;

	// read input buffer
	INIT_ERROR_DETECTION_MECHANISM;
	PosInBuff = glbMsgBuffer;

	ClicksTblRec.update_date	= get_long	(&PosInBuff,	DateYYYYMMDD_len);
	ClicksTblRec.update_time	= get_long	(&PosInBuff,	TimeHHMMSS_len);
	ClicksTblRec.key			= get_short	(&PosInBuff,	Tkey_len);
	ClicksTblRec.action_type	= get_short	(&PosInBuff,	Taction1_type_len);
	ClicksTblRec.kod_tbl		= get_long	(&PosInBuff,	Tkod_tbl_len);
	get_string (&PosInBuff, ClicksTblRec.descr,				Tdescr_len);
	get_string (&PosInBuff, ClicksTblRec.small_descr,		Tsmall_descr_len);
	get_string (&PosInBuff, ClicksTblRec.algoritm,			Tfld_9_len);
	get_string (&PosInBuff, ClicksTblRec.add_fld1,			Tfld_9_len);
	get_string (&PosInBuff, ClicksTblRec.add_fld2,			Tfld_4_len);
	get_string (&PosInBuff, ClicksTblRec.add_fld3,			Tfld_4_len);
	get_string (&PosInBuff, ClicksTblRec.add_fld4,			Tfld_4_len);
	get_string (&PosInBuff, ClicksTblRec.add_fld5,			Tfld_4_len);
	get_string (&PosInBuff, ClicksTblRec.add_fld6,			Tdescr_len);
	ClicksTblRec.times			= get_short	(&PosInBuff,	Ttimes_len);

	return MSG_HANDLER_SUCCEEDED;
}
// end of msg 3018 handler


// Message 3019 - insert to Gastro - no database updates.
static int message_3019_handler (TConnection *p_connect, int data_len, bool do_insert)
{
	TGastroRecord	GastroRec;
	char			status [3];

	// read input buffer
	INIT_ERROR_DETECTION_MECHANISM;
	PosInBuff = glbMsgBuffer;

	GastroRec.Contractor		= get_long	(&PosInBuff,	IdNumber_9_len);
	GastroRec.TreatmentContr	= get_long	(&PosInBuff,	IdNumber_9_len);
	GastroRec.Location			= get_long	(&PosInBuff,	LocationId_7_len);
	GastroRec.ValidFrom			= get_long	(&PosInBuff,	DateYYYYMMDD_len);
	GastroRec.ValidUntil		= get_long	(&PosInBuff,	DateYYYYMMDD_len);
	GastroRec.ContractType		= get_short	(&PosInBuff,	ContractType_2_len);
	GastroRec.TermId			= get_long	(&PosInBuff,	TermId_7_len);
	GastroRec.AuthType			= get_short	(&PosInBuff,	2);
	get_string (&PosInBuff, status,							2);

	return MSG_HANDLER_SUCCEEDED;
}
// end of msg 3019 handler


// Message 3021 - insert to table LabNotes from mokeds - no database updates.
static int message_3021_handler (TConnection *p_connect, int data_len, bool do_insert)
{
	short			Contr_N;
	TLabNoteRecord	LabNoteRec;

	// read input buffer
	INIT_ERROR_DETECTION_MECHANISM;
	PosInBuff = glbMsgBuffer;

	LabNoteRec.MemberId.Code	= get_short	(&PosInBuff,	1);
	LabNoteRec.MemberId.Number	= get_intv	(&PosInBuff,	IdNumber_9_len);
	LabNoteRec.ReqId			= get_intv	(&PosInBuff,	ReqId_9_len);
	LabNoteRec.ReqCode			= get_intv	(&PosInBuff,	ReqCode_5_len);
	LabNoteRec.NoteType			= get_short	(&PosInBuff,	NoteType_2_len);
	LabNoteRec.LineIndex		= get_short	(&PosInBuff,	LineIndex_2_len);
	get_string (&PosInBuff, LabNoteRec.Note,				Note_30_len);
	LabNoteRec.Contractor		= get_intv	(&PosInBuff,	IdNumber_9_len);
	Contr_N						= get_short	(&PosInBuff,	2);
  
	return MSG_HANDLER_SUCCEEDED;
}
// end of msg 3021 handler LABNOTES from mokeds


// Message 3023 - insert to table MemberContr - no database updates.
static int message_3023_handler (TConnection *p_connect, int data_len, bool do_insert)
{
	TMemberContrRecord	MembContrRec;

	// read input buffer
	INIT_ERROR_DETECTION_MECHANISM;
	PosInBuff = glbMsgBuffer;

	MembContrRec.Contractor			= get_long	(&PosInBuff, IdNumber_9_len);
	MembContrRec.MemberId.Number	= get_long	(&PosInBuff, IdNumber_9_len);
	MembContrRec.MemberId.Code		= get_short	(&PosInBuff, 1);
	MembContrRec.ContractType		= get_short	(&PosInBuff, ContractType_2_len);
	MembContrRec.Grade				= get_short	(&PosInBuff, 2);
	MembContrRec.TermId				= get_long	(&PosInBuff, TermId_7_len);

	return MSG_HANDLER_SUCCEEDED;
}
// end of msg 3023 handler


// Message 3024 - insert to table LabAdmin - no database updates.
static int message_3024_handler (TConnection *p_connect, int data_len, bool do_insert)
{
	TLabAdminRecord	LabAdminRec;

	// read input buffer
	INIT_ERROR_DETECTION_MECHANISM;
	PosInBuff = glbMsgBuffer;

	LabAdminRec.Contractor		= get_long	(&PosInBuff, IdNumber_9_len);
	LabAdminRec.MemberId.Code	= get_short	(&PosInBuff, 1);
	LabAdminRec.MemberId.Number	= get_long	(&PosInBuff, IdNumber_9_len);
	LabAdminRec.ReqIdMain		= get_long	(&PosInBuff, 8);
	LabAdminRec.ReqDate			= get_long	(&PosInBuff, 8);

	return MSG_HANDLER_SUCCEEDED;
}
// end of msg 3024 handler


// Message 3025 - insert/update to table cities - no database updates.
static int message_3025_handler (TConnection *p_connect, int data_len, bool do_insert)
{
	TCityDistance CityDistRec;

	// read input buffer
	INIT_ERROR_DETECTION_MECHANISM;
	PosInBuff = glbMsgBuffer;

	CityDistRec.city0		= get_long	(&PosInBuff, 5);
	CityDistRec.city1		= get_long	(&PosInBuff, 5);
	CityDistRec.distance	= get_long	(&PosInBuff, 5);
	CityDistRec.status		= get_long	(&PosInBuff, 5);	// 1 upd, 2 ins

	return MSG_HANDLER_SUCCEEDED;
}
// end of msg 3025 handler


// Message 3026 - insert/update to table ContractMsg - no database updates.
static int message_3026_handler (TConnection *p_connect, int data_len, bool do_insert)
{
	TPsicStat	PsicStatRec;

	// read input buffer
	INIT_ERROR_DETECTION_MECHANISM;
	PosInBuff = glbMsgBuffer;

	PsicStatRec.MemberId.Code	= get_short	(&PosInBuff, 1);
	PsicStatRec.MemberId.Number	= get_long	(&PosInBuff, 9);
	PsicStatRec.Contractor		= get_long	(&PosInBuff, 9);
	PsicStatRec.PsicStatus		= get_short	(&PosInBuff, 1);	// 1 upd, 2 ins

	return MSG_HANDLER_SUCCEEDED;
}
// end of msg 3026 handler


// Message 3027 - insert/update to table memberdietcntas - no database updates.
static int message_3027_handler (TConnection *p_connect, int data_len, bool do_insert)
{
	TMembDietCnt	MembDietCnt;

	// read input buffer
	INIT_ERROR_DETECTION_MECHANISM;
	PosInBuff = glbMsgBuffer;

	MembDietCnt.MemberId.Code	= get_short	(&PosInBuff, 1);
	MembDietCnt.MemberId.Number	= get_long	(&PosInBuff, 9);
	MembDietCnt.ValidFrom		= get_long	(&PosInBuff, 8);
	MembDietCnt.ValidUntil		= get_long	(&PosInBuff, 8);
	MembDietCnt.DietCnt			= get_long	(&PosInBuff, 5);	// 1 upd, 2 ins

	return MSG_HANDLER_SUCCEEDED;
}
// end of msg 3027 handler


// Message 3031 - insert/update to table contract - no database updates.
static int message_3031_handler (TConnection *p_connect, int data_len, bool do_update)
{
	TContractRecord	ContractRecord;
	int				FlagAction ;

	// read input buffer
	INIT_ERROR_DETECTION_MECHANISM;
	PosInBuff = glbMsgBuffer;

	ContractRecord.Contractor		= get_long	(&PosInBuff,	IdNumber_9_len);
	ContractRecord.ContractType		= get_short	(&PosInBuff,	ContractType_2_len);
	get_string (&PosInBuff,	ContractRecord.FirstName,			FirstName_8_len);
	get_string (&PosInBuff,	ContractRecord.LastName,			LastName_14_len);
	get_string (&PosInBuff,	ContractRecord.Street,				Street_16_len);
	get_string (&PosInBuff,	ContractRecord.City,				City_20_len);
	get_string (&PosInBuff,	ContractRecord.Phone,				Phone_10_len);
	ContractRecord.Mahoz			= get_short	(&PosInBuff,	Mahoz_len );
	ContractRecord.Profession		= get_short	(&PosInBuff,	2);
	ContractRecord.AcceptUrgent		= get_short	(&PosInBuff,	1);
	ContractRecord.SexesAllowed		= get_short	(&PosInBuff,	1);
	ContractRecord.AcceptAnyAge		= get_short	(&PosInBuff,	1);
	ContractRecord.AllowKbdEntry	= get_short	(&PosInBuff,	3);
	ContractRecord.NoShifts			= get_short	(&PosInBuff,	1);
	ContractRecord.IgnorePrevAuth	= get_short	(&PosInBuff,	1);
	ContractRecord.AuthorizeAlways	= get_short	(&PosInBuff,	1);
	ContractRecord.NoAuthRecord		= get_short	(&PosInBuff,	1);
	ContractRecord.prof2			= get_short	(&PosInBuff,	2);
	ContractRecord.prof3			= get_short	(&PosInBuff,	2);
	ContractRecord.prof4			= get_short	(&PosInBuff,	2);
	ContractRecord.TaxValue			= get_short	(&PosInBuff,	2);
	FlagAction						= get_short	(&PosInBuff,	2);

	return MSG_HANDLER_SUCCEEDED;
}
// end of msg 3031 handler


// Message 3032 - insert/update to table conterm - no database updates.
static int message_3032_handler (TConnection *p_connect, int data_len, bool do_update)
{
	TContermRecord	ContermRecord;
	int				FlagAction;

	// read input buffer
	INIT_ERROR_DETECTION_MECHANISM;
	PosInBuff = glbMsgBuffer;

	ContermRecord.TermId		= get_long	(&PosInBuff,	TermId_7_len);
	ContermRecord.Contractor	= get_long	(&PosInBuff,	IdNumber_9_len);
	ContermRecord.ContractType	= get_short	(&PosInBuff,	ContractType_2_len);
	ContermRecord.AuthType		= get_short	(&PosInBuff,	1);
	ContermRecord.ValidFrom		= get_long	(&PosInBuff,	DateYYYYMMDD_len);
	ContermRecord.ValidUntil	= get_long	(&PosInBuff,	DateYYYYMMDD_len);
	ContermRecord.Location		= get_long	(&PosInBuff,	LocationId_7_len);
	ContermRecord.Mahoz			= get_short	(&PosInBuff,	Mahoz_len);
	ContermRecord.Snif			= get_long	(&PosInBuff,	5);
	ContermRecord.Locationprv	= get_long	(&PosInBuff,	LocationId_7_len);
	get_string (&PosInBuff,	ContermRecord.Position,			1);
	get_string (&PosInBuff,	ContermRecord.Paymenttype,		1);
	ContermRecord.ServiceType	= get_short	(&PosInBuff,	3);
	ContermRecord.City			= get_long	(&PosInBuff,	9);
	FlagAction					= get_short	(&PosInBuff,	2);

	return MSG_HANDLER_SUCCEEDED;
}
// end of msg 3032 handler


// Message 3033 - insert/update to table terminal - no database updates.
static int message_3033_handler (TConnection *p_connect, int data_len, bool do_update)
{
	TTerminalRecord	TerminalRecord;
	int				FlagAction ;

	// read input buffer
	INIT_ERROR_DETECTION_MECHANISM;
	PosInBuff = glbMsgBuffer;

	TerminalRecord.TermId	= get_long	(&PosInBuff,	TermId_7_len);
	get_string (&PosInBuff,	TerminalRecord.Descr,		38);
	get_string (&PosInBuff,	TerminalRecord.Street,		Street_16_len);
	get_string (&PosInBuff,	TerminalRecord.City,		City_20_len);
	get_string (&PosInBuff,	TerminalRecord.LocPhone,	Phone_10_len);
	FlagAction				= get_short	(&PosInBuff,	2);

	return MSG_HANDLER_SUCCEEDED;
}
// end of msg 3033 handler


// Message 3034 - insert/update to table subst - no database updates.
static int message_3034_handler (TConnection *p_connect, int data_len, bool do_update)
{
	TSubstRecord	SubstRecord;
	int				FlagAction ;

	// read input buffer
	INIT_ERROR_DETECTION_MECHANISM;
	PosInBuff = glbMsgBuffer;

	SubstRecord.Contractor		= get_long	(&PosInBuff,	IdNumber_9_len);
	SubstRecord.ContractType	= get_short	(&PosInBuff,	ContractType_2_len);
	SubstRecord.AltContractor	= get_long	(&PosInBuff,	IdNumber_9_len);
	SubstRecord.SubstType		= get_short	(&PosInBuff,	2);
	SubstRecord.DateFrom		= get_long	(&PosInBuff,	DateYYYYMMDD_len);
	SubstRecord.DateUntil		= get_long	(&PosInBuff,	DateYYYYMMDD_len);
	SubstRecord.SubstReason		= get_short	(&PosInBuff,	2);
	FlagAction					= get_short	(&PosInBuff,	2);

	return MSG_HANDLER_SUCCEEDED;
}
// end of msg 3034 handler


// Message 3035 - insert/update to table ContractNew - no database updates.
static int message_3035_handler (TConnection *p_connect, int data_len, bool do_update)
{
	TContractNewRecord	ContractNewRecord;
	int					FlagAction;

	// read input buffer
	INIT_ERROR_DETECTION_MECHANISM;
	PosInBuff = glbMsgBuffer;

	ContractNewRecord.Contractor		= get_long	(&PosInBuff,	IdNumber_9_len);
	ContractNewRecord.ContractType		= get_short	(&PosInBuff,	ContractType_2_len);
	get_string (&PosInBuff,	ContractNewRecord.FirstName,			FirstName_8_len);
	get_string (&PosInBuff,	ContractNewRecord.LastName,				LastName_14_len);
	get_string (&PosInBuff,	ContractNewRecord.Street,				Street_16_len);
	get_string (&PosInBuff,	ContractNewRecord.City,					City_20_len);
	get_string (&PosInBuff,	ContractNewRecord.Phone,				Phone_10_len);
	ContractNewRecord.Mahoz				= get_short	(&PosInBuff,	Mahoz_len );
	ContractNewRecord.Profession		= get_short	(&PosInBuff,	2);
	ContractNewRecord.AcceptUrgent		= get_short	(&PosInBuff,	1);
	ContractNewRecord.SexesAllowed		= get_short	(&PosInBuff,	1);
	ContractNewRecord.AcceptAnyAge		= get_short	(&PosInBuff,	1);
	ContractNewRecord.AllowKbdEntry		= get_short	(&PosInBuff,	3);
	ContractNewRecord.NoShifts			= get_short	(&PosInBuff,	1);
	ContractNewRecord.IgnorePrevAuth	= get_short	(&PosInBuff,	1);
	ContractNewRecord.AuthorizeAlways	= get_short	(&PosInBuff,	1);
	ContractNewRecord.NoAuthRecord		= get_short	(&PosInBuff,	1);
	ContractNewRecord.prof2				= get_short	(&PosInBuff,	2);
	ContractNewRecord.prof3				= get_short	(&PosInBuff,	2);
	ContractNewRecord.prof4				= get_short	(&PosInBuff,	2);
	ContractNewRecord.TaxValue			= get_short	(&PosInBuff,	2);
	FlagAction							= get_short	(&PosInBuff,	2);
	ContractNewRecord.TermId			= get_long	(&PosInBuff,	TermId_7_len);

	return MSG_HANDLER_SUCCEEDED;
}
// end of msg 3035 handler



/*===============================================================
||																||
||			call_msg_handler()									||
||																||
 ===============================================================*/
static int call_msg_handler (TSystemMsg msg, TConnection *p_connect, int len)
{
	int					retcode;
	static int			last_data_msg = 0;


	// Save the current data message in last_data_msg.
	if ((msg >= 3000) && (msg <= 3999))
		last_data_msg = msg;

	// switch over message type & handle it
	switch (msg)
	{
		// Data messages

		// The first two actually do something. The rest are for the old
		// Informix-based Doctors system, and are now just "stubs" that
		// receive data from AS/400 and throw it away.

		case 3008:	// Hospitalization.
					rows_received++;
					retcode = message_3008_handler (p_connect, len, ATTEMPT_INSERT_FIRST);
					break;

		case 3022:	// Lab Results (from moked)
					rows_received++;
					retcode = message_3022_handler (p_connect, len, ATTEMPT_INSERT_FIRST);
					break;

		// "Stubs":

		case 3003:
					rows_received++;
					retcode = message_3003_handler (p_connect, len, ATTEMPT_INSERT_FIRST);
					break;

		case 3004:
					rows_received++;
					retcode = message_3004_handler (p_connect, len );
					break;

		case 3005:
					rows_received++;
					retcode = message_3005_handler (p_connect, len, ATTEMPT_UPDATE_FIRST);
					break;

		case 3006:
					rows_received++;
					retcode = message_3006_handler (p_connect, len, ATTEMPT_INSERT_FIRST);
					break;

		case 3007:
					rows_received++;
					retcode = message_3007_handler (p_connect, len);
					break;

		case 3009:
					rows_received++;
					retcode = message_3009_handler (p_connect, len, ATTEMPT_INSERT_FIRST);
					break;

		case 3010:
					rows_received++;
					retcode = message_3010_handler (p_connect, len, ATTEMPT_INSERT_FIRST);
					break;

		case 3011:
					rows_received++;
					retcode = message_3011_handler (p_connect, len, ATTEMPT_INSERT_FIRST);
					break;

		case 3012:
					rows_received++;
					retcode = message_3012_handler (p_connect, len, ATTEMPT_INSERT_FIRST);
					break;

		case 3013:
					rows_received++;
					retcode = message_3013_handler (p_connect, len, ATTEMPT_UPDATE_FIRST);
					break;

		case 3014:
					rows_received++;
					retcode = message_3014_handler (p_connect, len, ATTEMPT_UPDATE_FIRST);
					break;

		case 3015:
					rows_received++;
					retcode = message_3015_handler (p_connect, len, ATTEMPT_INSERT_FIRST);
					break;

		case 3016:
					rows_received++;
					retcode = message_3016_handler (p_connect, len, ATTEMPT_INSERT_FIRST);
					break;

		case 3017:
					rows_received++;
					retcode = message_3017_handler (p_connect, len, ATTEMPT_INSERT_FIRST);
					break;

		case 3018:
					rows_received++;
					retcode = message_3018_handler (p_connect, len, ATTEMPT_INSERT_FIRST);
					break;

		case 3019:
					rows_received++;
					retcode = message_3019_handler (p_connect, len, ATTEMPT_INSERT_FIRST);
					break;

		case 3021:  /* Yulia add mokeds 20070213*/
					rows_received++;
					retcode = message_3021_handler (p_connect, len, ATTEMPT_INSERT_FIRST);
					break;

		case 3023:
					rows_received++;
					retcode = message_3023_handler (p_connect, len, ATTEMPT_INSERT_FIRST);
					break;

		case 3024:
					rows_received++;
					retcode = message_3024_handler (p_connect, len, ATTEMPT_INSERT_FIRST);
					break;

		case 3025:
					rows_received++;
					retcode = message_3025_handler (p_connect, len, ATTEMPT_INSERT_FIRST);
					break;

		case 3026:
					rows_received++;
					retcode = message_3026_handler (p_connect, len, ATTEMPT_INSERT_FIRST);
					break;

		case 3027:
					rows_received++;
					retcode = message_3027_handler (p_connect, len, ATTEMPT_INSERT_FIRST);
					break;

		case 3031:	//CONTRACTNEW
					rows_received++;
					retcode = message_3031_handler (p_connect, len, ATTEMPT_INSERT_FIRST);
					break;

		case 3032:	//CONTERMNEW
					rows_received++;
					retcode = message_3032_handler (p_connect, len, ATTEMPT_INSERT_FIRST);
					break;

		case 3033:	//TERMINALNEW
					rows_received++;
					retcode = message_3033_handler (p_connect, len, ATTEMPT_INSERT_FIRST);
					break;

		case 3034:	//SUBSTNEW
					rows_received++;
					retcode = message_3034_handler (p_connect, len, ATTEMPT_INSERT_FIRST);
					break;

		case 3035:	//CONTRACTNEWNEW
					rows_received++;
					retcode = message_3035_handler (p_connect, len, ATTEMPT_INSERT_FIRST);
					break ;


		// Communications messages

		case CM_REQUESTING_COMMIT:		// 9901
					retcode = commit_handler (p_connect, 0);

					if (last_data_msg != 0)
					{
						if ((last_data_msg == 3008) || (last_data_msg == 3022))
						{
							GerrLogMini (	GerrId,
											"%s: %d rows received, %d rows processed.",
											(last_data_msg == 3008) ? "Hospitalization" : "Lab Results",
											rows_received, rows_processed	);
						}
						else
						{
							GerrLogMini	(	GerrId,
											"%d: %d rows received.",
											last_data_msg, rows_received	);

						}
					}	// Log results of last data type downloaded.

					rows_received = rows_processed = last_data_msg = 0;

					break ;


		case CM_REQUESTING_ROLLBACK:	// 9903
					retcode = rollback_handler (p_connect);
					rows_received = rows_processed = last_data_msg = 0;
					break ;

		//   The messages bellow can only be SENT (& not be received)
		//   by AS400<->Unix Server process:
		//   
		//   CM_CONFIRMING_COMMIT
		//   CM_CONFIRMING_ROLLBACK
		//   CM_SYSTEM_GOING_DOWN
		//   CM_SYSTEM_IS_BUSY
		//   CM_COMMAND_EXECUTION_FAILED
		//   CM_SYSTEM_IS_NOT_BUSY
		//   CM_SQL_ERROR_OCCURED
		//   CM_DATA_PARSING_ERR
		//   CM_ALL_RETRIES_WERE_USED

		default:
					GerrLogMini (GerrId, "Got unknown message id %d", msg);
					break;

	}	// switch on message received

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
				   "As400UnixDocServer received Signal %d (%s). Terminating process.",
				   signo,
				   sig_description);
}

