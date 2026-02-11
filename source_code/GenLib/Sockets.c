/*=======================================================================
||									||
||				Sockets.c				||
||									||
||----------------------------------------------------------------------||
||									||
|| Date : 02/05/96							||
|| Written By : Ely Levy ( reshuma )					||
||									||
||----------------------------------------------------------------------||
||									||
||  Purpose:								||
||  --------								||
||  	library routines for communication sockets handling.		||
||	Communication sockets handling should be done			||
||	only with these library routines.				||
||									||
||----------------------------------------------------------------------||
||									||
|| 				Include files				||
||									||
 =======================================================================*/

#include "../Include/Global.h"
#include <sys/time.h>	// DonR 24Sep2024: Added to get rid of warning message compiling on new Dev server.

static char	*GerrSource = __FILE__;
extern int	caught_signal;

/*=======================================================================
||                                                                      ||
||                      Private parameters                              ||
||                                                                      ||
||      CAUTION : DO NOT !!! CHANGE THIS PARAGRAPH -- IMPORTANT !!!     ||
||                                                                      ||
 =======================================================================*/

typedef	struct
{
  int	length;
  char	mesg_type;
}
MESG_HEADER;

#ifndef NO_PRN  // GAL
    #define prn(msg)						 \
    printf("{pid=%d, ppid=%d}, %s\n", getpid(), getppid(), msg); \
    fflush(stdout);

    #define prn_1(msg,d)			\
    {						\
      char buf[256];				\
      sprintf(buf,msg,d);			\
      prn(buf);					\
    }
    #define prn_2(msg,d,e)			\
    {						\
      char buf[256];				\
      sprintf(buf,msg,d,e);			\
      prn(buf);					\
    }
    #define prn_3(msg,d,e,f)			\
    {						\
      char buf[256];				\
      sprintf( buf,msg,d,e,f);			\
      prn(buf);					\
    }
#endif

// DonR 24Sep2024: Add prototypes for local function(s).
int		set_header_details	(	int socket_num, int type, int len	);


/*=======================================================================
||									||
||									||
||		 N A M E D   S O C K E T S ( U N I X )			||
||									||
||									||
 =======================================================================*/

/*=======================================================================
||									||
||				ListenSocketNamed()			||
||									||
||	Obtain a stream named unix-domain socket and bind a name to it.	||
||	Return the socket number.					||
 =======================================================================*/
int	ListenSocketNamed(
			  char *socket_name	/* Socket name -- file	*/
			  )
{

/*=======================================================================
||									||
||				LOCAL VARIABLES				||
||									||
 =======================================================================*/

    int 	socket_num,
		nonzero = 1;
    struct 	sockaddr_un name;

/*=======================================================================
||									||
||			GET A STREAM SOCKET				||
||									||
 =======================================================================*/

    socket_num = socket(
			AF_UNIX,
			SOCK_STREAM,
			0
			);

    if( socket_num < 0 )
    {
        GerrLogReturn(
		      GerrId,
		      "Can't open a stream unix-domain"
		      " socket.Error ( %d ) %s.",
		      GerrStr
		      );
	return( NO_SOCKET );
    }

    if( socket_num < 3 )
    {
	GerrLogReturn(GerrId,
		      "Got weird socket (%d)",
		      socket_num );
    }

/*
 * Set socket I/O options
 * ----------------------
 */
							// user address again
							// socket KEEPALIVE
    /*
     * Not working on SCO ....
    if( setsockopt(socket_num, SOL_SOCKET, SO_KEEPALIVE,
	&nonzero, sizeof(int)) == -1 )
	GerrLogExit( GerrId, -1,
	"Can't set socket I/O options:SO_KEEPALIVE.Error ( %d ) %s.", GerrStr );
     */

/*=======================================================================
||									||
||			BIND SOCKET TO A NAME				||
||									||
 =======================================================================*/

    name.sun_family	= AF_UNIX;
    strcpy(
	   name.sun_path,
	   socket_name
	   );

    if(
       bind(
	    socket_num,
	    (struct sockaddr *)&name,
	    sizeof(struct sockaddr_un)
	    )
       )
    {
        GerrLogReturn(
		      GerrId,
		      "Can't bind socket to name %s"
		      ".Error ( %d ) %s.",
		      name.sun_path,
		      GerrStr
		      );
	return( BIND_ERR );
    }

/*=======================================================================
||									||
||				LISTEN ON SOCKET			||
||									||
 =======================================================================*/

    if(
       listen(
	      socket_num,
	      SOMAXCONN
	      )
       )
    {
	GerrLogReturn(
		      GerrId,
		      "Can't listen on socket.Error ( %d ) %s.",
		      GerrStr
		      );
	return( LISTEN_ERR );
    }

/*
 * Return socket
 * -------------
 */
    NamedPipeSocket = socket_num;

    return( socket_num );

}

/*=======================================================================
||									||
||				ConnectSocketNamed()			||
||									||
||	Obtain a stream unix-domain socket and connect it to a named	||
||	unix-domain bound socket.					||
||	Return the socket number ( connected to remote bound socket ).	||
 =======================================================================*/
int	ConnectSocketNamed(
			   char *socket_name	/* socket name -- file	*/
			   )
{

/*=======================================================================
||									||
||				LOCAL VARIABLES				||
||									||
 =======================================================================*/

    int 	socket_num;
    struct 	sockaddr_un name;

/*=======================================================================
||									||
||			GET A STREAM SOCKET				||
||									||
 =======================================================================*/

    socket_num = socket(
			AF_UNIX,
			SOCK_STREAM,
			0
			);

    if( socket_num < 0 )
    {
        GerrLogReturn(
		      GerrId,
		      "Can't open a stream unix-domain"
		      " socket.Error ( %d ) %s.",
		      GerrStr
		      );
	return( NO_SOCKET );
    }

/*=======================================================================
||									||
||			CONNECT TO A NAMED SOCKET			||
||									||
 =======================================================================*/

    name.sun_family = AF_UNIX;
    strcpy(
	   name.sun_path,
	   socket_name
	   );

    if(
       connect(
	       socket_num,
	       (struct sockaddr *)&name,
	       sizeof(struct sockaddr_un)
	       )
       )
    {
	if( errno == EINTR )
	{
	    GerrLogReturn(
			  GerrId,
			  "Connect was interrupted by signal\n\t\t\t\t"
			  "probably -- timeout....\n\tError ( %d ) %s.",
			  name.sun_path,
			  GerrStr
			  );
	}
	else
	{
	    GerrLogReturn(
			  GerrId,
			  "Can't connect to socket named '%s'"
			  ".Error ( %d ) %s.",
			  name.sun_path,
			  GerrStr
			  );
	}
	return( CONNECT_ERR );
    }

/*
 * Return socket
 * -------------
 */
    return( socket_num );

}

/*=======================================================================
||									||
||									||
||		 U N N A M E D   S O C K E T S ( I N T E R N E T )	||
||									||
||									||
 =======================================================================*/

/*=======================================================================
||									||
||			   ListenSocketUnnamed()			||
||									||
 =======================================================================*/
int	ListenSocketUnnamed(
			    int port
			    )
{

/*=======================================================================
||									||
||				LOCAL VARIABLES				||
||									||
 =======================================================================*/

    struct sockaddr_in	server_addr;		/* internet address	*/
    int	nonzero = 1,				/* nonzero value 4 toggl*/
	socket_num;				/* socket number	*/

/*=======================================================================
||									||
||			GET A STREAM SOCKET				||
||									||
 =======================================================================*/

    socket_num = socket(
			AF_INET,
			SOCK_STREAM,
			0
			);

    if( socket_num < 0 )
    {
	GerrLogReturn(
		      GerrId,
		      "Can't get TCP socket.Error ( %d ) %s.",
		      GerrStr
		      );
	return( NO_SOCKET );
    }

/*=======================================================================
||									||
||			SET SOCKET OPTIONS				||
||									||
 =======================================================================*/

							// user address again
    if(
       setsockopt(
		  socket_num,
		  SOL_SOCKET,
		  SO_REUSEADDR,
		  &nonzero,
		  sizeof(int)
		  )
       == -1
       )
    {
	GerrLogReturn(
		      GerrId,
		      "Can't set socket I/O options:"
		      "SO_REUSEADDR.Error ( %d ) %s.",
		      GerrStr
		      );
	return( SOCK_OPT_ERR );
    }
    /* GAL 6/01/3005 Linux			// user port again
    if(
       setsockopt(
		  socket_num,
		  SOL_SOCKET,
		  SO_REUSEPORT,
		  &nonzero,
		  sizeof(int)
		  )
       == -1
       )
    {
	GerrLogReturn(
		      GerrId,
		      "Can't set socket I/O options:"
		      "SO_REUSEPORT.Error ( %d ) %s.",
		      GerrStr
		      );
	return( SOCK_OPT_ERR );
    }
    */							// socket KEEPALIVE
    if(
       setsockopt(
		  socket_num,
		  SOL_SOCKET,
		  SO_KEEPALIVE,
		  &nonzero,
		  sizeof(int)
		  )
       == -1
       )
    {
	GerrLogReturn(
		      GerrId,
		      "Can't set socket I/O options:"
		      "SO_KEEPALIVE.Error ( %d ) %s.",
		      GerrStr
		      );
	return( SOCK_OPT_ERR );
    }

/*=======================================================================
||									||
||			BIND SOCKET TO ADDRESS, PORT			||
||									||
 =======================================================================*/

    bzero(
	  (char*)&server_addr,
	  sizeof(server_addr)
	  );

    server_addr.sin_family 	= AF_INET;		/*Internet famil*/
    server_addr.sin_addr.s_addr = htonl( INADDR_ANY );	/*Internet adres*/
    server_addr.sin_port 	= htons( port );/* port number	*/

    if(
       bind(
	    socket_num,
	    (struct sockaddr*)&server_addr,
	    sizeof(struct sockaddr_in)
	    )
       )
    {
	GerrLogReturn(
		      GerrId,
		      "Can't bind TCP port %d.Error ( %d ) %s.",
		      MacTcpPort,
		      GerrStr
		      );
	return( BIND_ERR );
    }

/*=======================================================================
||									||
||			LISTEN ON SOCKET				||
||									||
 =======================================================================*/

    if(
       listen(
	      socket_num,
	      SOMAXCONN
	      )
       )
    {
	GerrLogReturn(
		      GerrId,
		      "Can't listen on socket.Error ( %d ) %s.",
		      GerrStr
		      );
	return( LISTEN_ERR );
    }

/*
 * Return the socket number
 * ------------------------
 */
    return( socket_num );

}

/*=======================================================================
||									||
||			ConnectSocketUnnamed()				||
||									||
||	Obtain unnamed sockcet and connect to remote adrress,port.	||
 =======================================================================*/
int	ConnectSocketUnnamed(
			     int	port_num,/* remote port number	*/
			     u_long	address	/* remote address 	*/
			     )
{

/*=======================================================================
||									||
||				LOCAL VARIABLES				||
||									||
 =======================================================================*/

    int			socket_num;		/* socket number	*/
    struct sockaddr_in 	remote_addr;		/* remote address struct*/
    struct in_addr 	in_addr;		/* 32-bit address	*/

/*=======================================================================
||									||
||			GET A STREAM SOCKET				||
||									||
 =======================================================================*/

    socket_num = socket(
			AF_INET,
			SOCK_STREAM,
			0
			);

    if( socket_num < 0 )
    {
        GerrLogReturn(
		      GerrId,
		      "Can't get AF_INET SOCK_STREAM socket."
		      "Error (%d) %s",
		      GerrStr
		      );
	return( NO_SOCKET );
    }

/*=======================================================================
||									||
||			CONNECT TO AN INTERNET SOCKET			||
||									||
 =======================================================================*/

    in_addr.s_addr = address;

    remote_addr.sin_family = AF_INET;		/*Internet address famly*/
    remote_addr.sin_port   = htons( port_num );	/* port number          */
    remote_addr.sin_addr   = in_addr;		/* internet address	*/

    if(
       connect(
	       socket_num,
	       (struct sockaddr *)&remote_addr,
	       sizeof(struct sockaddr_in)
	       )
       == -1
       )
    {
	/**
	 ** Add this line to source when "connect refuse .." error needed
	 ** in log file. otherwise leave it in comment..

	 GerrLogReturn(
		       GerrId,
		       "Can't connect to address %s port %d."
		       "Error (%d) %s",
		       address,
		       port_num,
		       GerrStr
		       );
	 **/

	return( CONNECT_ERR );
    }

/*
 * Return socket
 * -------------
 */
    return( socket_num );

}

/*=======================================================================
||									||
||			   AcceptConnection()				||
||									||
|| 		 Get a socket for accepted requested connection.	||
|| 		 Socket family can be unix or internet so		||
||		 Client address is struct sockaddr_in / sockaddr_un	||
||		 according to socket family type.			||
 =======================================================================*/
int	AcceptConnection(
			 int		listen_sock,	/* listen socket*/
			 struct sockaddr *client_addr, 	/* client addres*/
			 int		*addr_len 	/* address lengt*/
			 )
{

/*=======================================================================
||									||
||				LOCAL VARIABLES				||
||									||
 =======================================================================*/

    int		socket_num; 		/* new socket from accept()	*/

/*=======================================================================
||									||
||			ACCEPT NEW CONNECTION				||
||									||
 =======================================================================*/

    socket_num = accept(
			listen_sock,
			client_addr,
			addr_len
			);

    if( socket_num < 0 )
    {
	GerrLogReturn(
		      GerrId,
		      "Can`t accept a new connection.Error ( %d ) %s.",
		      GerrStr
		      );
	return( ACCEPT_ERR );
    }

/*
 * Return the socket number
 * ------------------------
 */
    return( socket_num );

}

/*=======================================================================
||									||
||			     CloseSocket()				||
||									||
||				Close socket.				||
 =======================================================================*/
int	CloseSocket(
		    int socket_num		/* Socket number	*/
		    )
{

    if( socket_num < 3 )
    {
	GerrLogReturn( GerrId,
			"Attempt to close socket (%d)", socket_num );
	return 0;
    }

    return( close( socket_num ) );

}

/*=======================================================================
||									||
||			     DisposeNamedPipe()				||
||									||
||				Dispose a named pipe			||
 =======================================================================*/
int	DisposeSockets(
		       void
		       )
{

/*
 * Remove named pipe file if exists
 */
    if( CurrNamedPipe[0] )
    {
	if( unlink( CurrNamedPipe ) )
	{
	    return( NAMEDP_DEL_ERR );
	}
    }

/*
 * Return -- ok
 */
    return( MAC_OK );

}

/*=======================================================================
||									||
||				ReadSocket()				||
||									||
||			Read from socket				||
||		Also append a null character after last byte read.	||
 =======================================================================*/
int	ReadSocket(
		   int		socket,		/* socket to read from	*/
		   char		*buffer,	/* buffer to recive data*/
		   int		max_len,	/* length of data 	*/
		   int		*type,		/* type of message	*/
		   int		*size,		/* type of message	*/
		   int		verbose_flg	/* verbose flag	*/
		   )
{

/*=======================================================================
||									||
||				LOCAL VARIABLES				||
||									||
 =======================================================================*/

    MESSAGE_HEADER	header;		/* general header of message	*/
    int			len,		/* length of a batch in read	*/
			batch,		/* size of batch to read	*/
			discard_flg = 0;/* discard data flag		*/
    char		tmp_buf[8192];

/*=======================================================================
||									||
||			GET HEADER OF MESSAGE				||
||									||
 =======================================================================*/

    state = get_header_details(
			       socket,
			       &header,
			       type,
			       size,
			       verbose_flg
			       );

    if( ERR_STATE( state )	&&
	verbose_flg == NO_VERBOSE )
    {
	return( state );
    }

    RETURN_ON_ERR( state );

/*
 * Check message len
 * -----------------
 */
    if( *size <= 0 )				/* length of message	*/
    {
	GerrLogReturn(
		      GerrId,
		      "Got non-positive length from message header.:%d",
		      *size
		      );
	return( HDR_ERR );
    }

    if( (len = *size) > max_len-1 )
    {
	*size = max_len-1;		/* rest of data will be discard */

	discard_flg = 1;
    }

/*=======================================================================
||									||
||			GET MESSAGE CONTENTS				||
||									||
 =======================================================================*/

    state = read_socket_data(
			     socket,
			     buffer,
			     *size,
			     verbose_flg
			     );

    RETURN_ON_ERR( state );

    buffer[*size] = 0;

    if( ! discard_flg )
    {
	return( MAC_OK );
    }

    GerrLogReturn(
		  GerrId,
		  "Some data discarded -- got %d bytes into buffer size %d"
		  "\nmessage is : %.20s...",
		  len,
		  max_len - 1,
		  buffer
		  );

/*=======================================================================
||									||
||			GET ALL OTHER DATA TO SYNCHRONIZE		||
||									||
 =======================================================================*/

    len -= *size;

    while( len > 0 )
    {
	batch = (len > sizeof(tmp_buf)) ? sizeof(tmp_buf) : len;

	state = read_socket_data(
				 socket,
				 tmp_buf,
				 batch,
				 verbose_flg
				 );

	RETURN_ON_ERR( state );

	len -= batch;
    }

/*
 * Return number of bytes read into buffer
 * ---------------------------------------
 */
    return( DATA_DISCARDED );

}

/*=======================================================================
||									||
||				WriteSocket()				||
||									||
||				Write to socket				||
 =======================================================================*/
int	WriteSocket(
		    int		socket,		/* socket to write thru	*/
		    const char	*buffer,	/* buffer to recive data*/
		    int		buf_len,	/* length of data 	*/
		    int		type		/* type of message	*/
		    )
{

/*=======================================================================
||									||
||		PREPARE AND SEND HEADER OF MESSAGE			||
||									||
 =======================================================================*/

    state =
      set_header_details(
			 socket,
			 type,
			 buf_len
			 );

    RETURN_ON_ERR(
		  state
		  );

/*=======================================================================
||									||
||			SEND MESSAGE					||
||									||
 =======================================================================*/

    state =
      write_socket_data(
			socket,
			buffer,
			buf_len
			);

    return(
	   state
	   );

}

/*=======================================================================
||									||
||				WriteSocketHead()			||
||									||
||			Write to socket with header			||
 =======================================================================*/
int	WriteSocketHead(
			int		socket,	/* socket to write thru	*/
			short int	status,	/* system status	*/
			char		*buffer,/* buffer to recive data*/
			int		buf_len,/* length of data 	*/
			int		type	/* type of message	*/
			)
{

    char	small_buf[64];
  
/*=======================================================================
||									||
||		PREPARE NEW BUFFER WITH BUFFER HEADER			||
||									||
 =======================================================================*/

    small_buf[0] = (u_char)status;

/*=======================================================================
||									||
||		PREPARE AND SEND HEADER OF MESSAGE			||
||									||
 =======================================================================*/

    state =
      set_header_details(
			 socket,
			 type,
			 buf_len + 1
			 );

    RETURN_ON_ERR(
		  state
		  );

/*=======================================================================
||									||
||			SEND STATUS					||
||									||
 =======================================================================*/

    state =
      write_socket_data(
			socket,
			small_buf,
			1
			);

    RETURN_ON_ERR(
		  state
		  );

/*=======================================================================
||									||
||			SEND MESSAGE					||
||									||
 =======================================================================*/

    state =
      write_socket_data(
			socket,
			buffer,
			buf_len
			);

    return(
	   state
	   );

}

/*=======================================================================
||									||
||			     read_socket_data()				||
||									||
||			Read data from  connected socket.		||
 =======================================================================*/
int
	read_socket_data(
			 int	socket_num,	/* socket number	*/
			 char	*buffer,	/* buffer to receive	*/
			 int	size,		/* size of data to read	*/
			 int	verbose_flg	/* verbose flag		*/
			 )
{

/*=======================================================================
||									||
||				LOCAL VARIABLES				||
||									||
 =======================================================================*/

    int 	len;				/*number of byts recived*/
    fd_set	read_fds;
    struct timeval	tv;
    int		ret;

/*=======================================================================
||									||
||			READ LOOP : READ UNTIL ERROR /END.		||
||									||
 =======================================================================*/

    tv.tv_sec = ReadTimeout;
    tv.tv_usec = 0;

    while( size > 0 )
    {
	FD_ZERO( &read_fds );

	FD_SET( socket_num, &read_fds );

	switch(
		(ret=select(
		      socket_num + 1,
		      &read_fds,
		      (fd_set*)NULL,
		      (fd_set*)NULL,
		      &tv
		      ))
		)
	{
	    case -1:				/* SELECT ERROR		*/
		GerrLogReturn(
			      GerrId,
			      "Error at select. error ( %d ) %s.",
			      GerrStr
			      );
		return( SELECT_ERR );

	    case 0:				/* TIMEOUT REACHED	*/
		if( verbose_flg == VERBOSE )
		{
		    GerrLogReturn(
				  GerrId,
				  "timeout at read -- exiting recv."
				  );
		}
		return( TIMEOUT_READ );

	    default:				/* DATA ARRIVES		*/
		if( ! FD_ISSET(
			     socket_num,
			     &read_fds
			     )
		    )
		{
		    GerrLogReturn(
				  GerrId,
				  "funny error occured -- select return <%d>."
				  " but socket is not set in fd_set...",
				  ret
				  );
		    return( TIMEOUT_READ );
		}
		break;
	}

	len = recv(
		   socket_num,
		   buffer,
		   size,
		   0
		   );

	switch( len )
	{
	    case -1:				/* Error 		*/
		if( verbose_flg == VERBOSE )
		{
		    GerrLogReturn(
				  GerrId,
				  "Data receive error.Error (%d) %s",
				  GerrStr
				  );
		}
		return( READ_ERR );

	    case 0: 				/* Remote closed connect*/
		if( verbose_flg == VERBOSE )
		{
		    GerrLogReturn(
				  GerrId,
				  "(Receive)Connection closed by client."
				  );
		}
		return( CONN_END );
	}

	size	-= len;
	buffer	+= len;
    }

/*
 * Return number of bytes read
 * ---------------------------
 */
    return( MAC_OK );

}

/*=======================================================================
||									||
||			     write_socket_data()			||
||									||
||			Write data to connected socket			||
 =======================================================================*/
int
	write_socket_data(
			  int	socket_num,	/* socket number	*/
			  const char	*buffer,/* buffer to send data	*/
			  int	size		/* size of data to send	*/
			  )
{

/*=======================================================================
||									||
||				LOCAL VARIABLES				||
||									||
 =======================================================================*/

    int 	len;				/* number of byts sent	*/
    fd_set	write_fds;
    struct timeval	tv;

/*=======================================================================
||									||
||			WRITE LOOP : WRITE UNTIL ALL DATA SENT		||
||									||
 =======================================================================*/

    tv.tv_sec = WriteTimeout;
    tv.tv_usec = 0;

    while( size > 0 )
    {
	FD_ZERO( &write_fds );

	FD_SET( socket_num, &write_fds );

	switch(
		select(
		      socket_num + 1,
		      (fd_set*)NULL,
		      &write_fds,
		      (fd_set*)NULL,
		      &tv
		      )
		)
	{
	    case -1:				/* SELECT ERROR		*/
		GerrLogReturn(
			      GerrId,
			      "Error at select. error ( %d ) %s.",
			      GerrStr
			      );
		return( SELECT_ERR );

	    case 0:				/* TIMEOUT REACHED	*/
		GerrLogReturn(
			      GerrId,
			      "timeout at write -- exiting send."
			      );
		return( TIMEOUT_WRITE );
		break;

	    default:				/* DATA ARRIVES		*/
		if( ! FD_ISSET(
			     socket_num,
			     &write_fds
			     )
		    )
		{
		    GerrLogReturn(
				  GerrId,
				  "funny error occured -- select return > 0."
				  " but socket is not up in fd_set..."
				  );
		}
		break;
	}

	len = send(
		   socket_num,
		   buffer,
		   size,
		   0
		   );

	switch( len )
	{
	    case -1:				/* Error 		*/
		GerrLogMini(
			      GerrId,
			      "Data transmit error.Error (%d) %s",
			      GerrStr
			      );
		return( WRITE_ERR );

	    case 0: 				/* Remote closed connect*/
		GerrLogMini(
			      GerrId,
			      "(send)Connection closed by client."
			      );
		return( CONN_END );
	}
	// prn_1( "write_socket_data : %d bytes sent" , len ); // GAL
			    
	size    -= len;
	buffer  += len;
    }

/*
 * Return number of bytes read
 * ---------------------------
 */
    return( MAC_OK );
}

/*=======================================================================
||									||
||				get_header_details()			||
||									||
||		get message details from message header			||
 =======================================================================*/
static int
	get_header_details(
			   int	socket_num,	/* socket number	*/
			   MESSAGE_HEADER *headerp,/*gen message header	*/
			   int	*typep,		/* type pointer		*/
			   int	*lenp,		/* length pointer	*/
			   int	verbose_flg	/* verbose flag		*/
			   )
{

/*=======================================================================
||									||
||			GET HEADER OF MESSAGE				||
||									||
 =======================================================================*/

    state =
	  read_socket_data(
			   socket_num,
			   (char*)headerp,
			   sizeof(MESSAGE_HEADER),
			   verbose_flg
			   );

    if( ERR_STATE( state )	&&
	verbose_flg == NO_VERBOSE )
    {
	return( state );
    }

    RETURN_ON_ERR( state );

/*
 * Get message details from header
 * -------------------------------
 */
    *typep = atoi(headerp->messageType);
    *lenp = atoi(headerp->messageLen);

    // prn_2( "get_header_details : type = %d , len = %d " , *typep ,*lenp ); // GAL

    return( MAC_OK );

}

/*=======================================================================
||									||
||				set_header_details()			||
||									||
|| set message details in message header.				||
 =======================================================================*/
// static int commented by GAL 18/01/05
int	set_header_details(
			   int	socket_num,	/* socket number	*/
			   int	type,		/* message type 	*/
			   int	len		/* message length	*/
			   )
{

/*=======================================================================
||									||
||				LOCAL VARIABLES				||
||									||
 =======================================================================*/

    MESSAGE_HEADER	header;			/*general message header*/
    // prn_1( "sizeof( MESSAGE_HEADER ) = %d" ,  sizeof( MESSAGE_HEADER ) ); // GAL
/*=======================================================================
||									||
||			SET AND SEND MESSAGE HEADER			||
||									||
 =======================================================================*/

    bzero(
	  (char*)&header,
	  sizeof(MESSAGE_HEADER)
	  );
    sprintf(
	    header.messageType,
	    "%d",
	    type
	    );
    sprintf(
	    header.messageLen,
	    "%d",
	    len
	    );

    // prn_2( "set_header_details : messageType : \"%s\" , messageLen : \"%s\" " ,
    // header.messageType ,  header.messageLen );  // GAL
/*
 * Send header thru socket
 * -----------------------
 */
    RETURN_ON_ERR( write_socket_data(
				     socket_num,
				     (char*)&header,
				     sizeof(MESSAGE_HEADER)
				     )
		   );

    return( MAC_OK );

}

/*=======================================================================
||									||
||			   GetPeerAddr()				||
||									||
|| 		 Get the peer address of a socket.			||
 =======================================================================*/
char	*GetPeerAddr(
		     int socket_num		/* socket number	*/
		     )
{

/*=======================================================================
||									||
||				LOCAL VARIABLES				||
||									||
 =======================================================================*/

    struct sockaddr_in 	sock_data;
    static char		*cp;
    int  		addr_len = sizeof( struct sockaddr_in );

/*=======================================================================
||									||
||			GET PEER ADDRESS OF A SOCKET			||
||									||
 =======================================================================*/

    if(
       getpeername(
		   socket_num,
		   (struct sockaddr *)&sock_data,
		   &addr_len
		   )
       )
    {
	GerrLogReturn(
		      GerrId,
		      "Can't get peer socket details."
		      "Error ( %d ) %s.",
		      GerrStr
		      );
	return( NULL );
    }

/*
 * Transfer 32-Bit address into dotted address
 * -------------------------------------------
 */
    cp = inet_ntoa( sock_data.sin_addr );
    if( cp == (char *)-1 )
    {
	GerrLogReturn(
		      GerrId,
		      "Can't get inet_ntoa of 32-Bit addr : %12f"
		      ".Error ( %d ) %s.",
		      sock_data.sin_addr,
		      GerrStr
		      );
	return( NULL );
    }

/*
 * Return the address of socket
 * ----------------------------
 */
    return( cp );

}	

/*=======================================================================
||									||
||			   GetCurrNamedPipe()				||
||									||
|| 		 Get the peer address of a socket.			||
 =======================================================================*/
void    GetCurrNamedPipe(
			 char	*buffer		/* buffer to get data	*/
			 )
{
    sprintf(					/* get proces named pipe*/
	    buffer,
	    "%s/%s%d_%ld",
	    NamedpDir,
	    NAMEDP_PREFIX,
	    getpid(),
	    (long)time(NULL)
	    );
}

/*=======================================================================
||									||
||			   GetSocketMessage()				||
||									||
|| 		 Get the peer address of a socket.			||
 =======================================================================*/
int	GetSocketMessage(
			 int	micro_sec,	/* micro seconds to wait*/
			 char	*buffer,	/* buffer to get data	*/
			 int	max_len,	/* max length to read	*/
			 int	*size,		/* actual size read	*/
			 int	close_flg	/* socket close flag	*/
			 )
{

/*=======================================================================
||									||
||				LOCAL VARIABLES				||
||									||
 =======================================================================*/

    fd_set		read_fds;
    struct timeval	timeout;
    UNIV_ADDR		client_addr;
    int			accept_sock,
			type,
			addr_len = sizeof(client_addr);

/*=======================================================================
||									||
||		WAIT A WHILE FOR GETTING CONNECTION REQUESTS		||
||									||
 =======================================================================*/

    *size = buffer[0]	= 0;

    FD_ZERO( &read_fds );

    FD_SET(
	   NamedPipeSocket,
	   &read_fds
	   );

    timeout.tv_sec	= micro_sec / 1000000;
    timeout.tv_usec	= micro_sec % 1000000;

    accept_sock = 0;
   
	// DonR 28Apr2022: Select() is considered an "old" library function, and poll()
	// is now preferred. At some point we should look at changing over to poll() -
	// but it's not at all urgent.
    switch(
	   select(
		  NamedPipeSocket + 1,
		  &read_fds,
		  (fd_set*)NULL,
		  (fd_set*)NULL,
		  &timeout
		  )
	   )
    {
	case -1:				/* SELECT ERROR		*/
		if (caught_signal == 0)
			GerrLogReturn(
						  GerrId,
						  "Error at select. error ( %d ) %s."
						  "\nNamedPipeSocket : %d",
						  GerrStr,
						  NamedPipeSocket
						  );
	    return( SELECT_ERR );

	case 0:					/* TIMEOUT REACHED	*/
	    break;

	default:				/* CONNECTION REQUEST	*/
	    if( FD_ISSET(
			 NamedPipeSocket,
			 &read_fds
			 )
		)
	    {
		accept_sock =
		  AcceptConnection(
				   NamedPipeSocket,
				   (struct sockaddr*)&client_addr,
				   &addr_len
				   );
		RETURN_ON_ERR( accept_sock );

		RETURN_ON_ERR(
			      ReadSocket(
					 accept_sock,
					 buffer,
					 max_len,
					 &type,
					 size,
					 VERBOSE
					 )
			      );
	    }
	    break;
    }

/*
 * Return -- ok
 * ------------
 */
    if( close_flg == LEAVE_OPEN_SOCK )
    {
	return( accept_sock );
    }
    
    if( accept_sock )
    {
	CloseSocket( accept_sock );
    }

    return( MAC_OK );

}
/*=======================================================================
||									||
||				GetInterProcMesg()			||
||									||
 =======================================================================*/
int	GetInterProcMesg(
			 char	*in_buf,	/* input buffer		*/
			 int	*length,	/* length of data	*/
			 char	*out_buf	/* output buffer	*/
			 )
{

    MESG_HEADER	*mesg_header;
    int		fd;

    /*
     * Get header of data
     */
    mesg_header =
      (MESG_HEADER*)in_buf;

    *length =
      mesg_header->length;

    in_buf +=
      sizeof(MESG_HEADER);

    /*
     * Get data from input buffer / file
     */
    switch( mesg_header->mesg_type )
    {
	case FILE_MESG:				/* READ FILE DATA	*/
	    fd =
	      open(
		   in_buf,
		   O_RDONLY
		   );

	    if( fd == -1 )
	    {
		GerrLogReturn(
			      GerrId,
			      "Got non existed file name '%s'..",
			      "error ( %d ) %s",
			      in_buf,
			      GerrStr
			      );
		return( NO_FILE );
	    }

	    state =
	      read(
		   fd,
		   out_buf,
		   BUFFER_SIZE
		   );

	    close( fd );

	    if( state == -1 )
	    {
		GerrLogReturn(
			      GerrId,
			      "Error while reading file name '%s'.."
			      "error ( %d ) %s",
			      in_buf,
			      GerrStr
			      );
		return( NO_FILE );
	    }

	    out_buf[state] =
	      0;

	    break;
		   
	case DATA_MESG:				/* READ BUFFER DATA	*/

	    memcpy(
		   out_buf,
		   in_buf,
		   mesg_header->length
		   );

	    out_buf[mesg_header->length] =
	      0;

	    break;

	default:				/* UNKNOWN TYPE		*/

	  GerrLogReturn(
			GerrId,
			"Unknown message type '%d'",
			mesg_header->mesg_type
			);

	  return( NO_FILE );
	  
    }

    return( MAC_OK );
    
}

/*=======================================================================
||									||
||				SetInterProcMesg()			||
||									||
 =======================================================================*/
int	SetInterProcMesg(
			 char	*in_buf,	/* input buffer		*/
			 int	length,		/* length of data	*/
			 int	mesg_type,	/* direct / via file	*/
			 char	*out_buf,	/* output buffer	*/
			 int	*out_len	/* output buffer length	*/
			 )
{

    MESG_HEADER	mesg_header;
    int		fd;
    struct timeval	tv;
    struct tm	*str_tm;

    /*
     * Set header of data
     */
    mesg_header.mesg_type =
      mesg_type;

    out_buf +=
      sizeof(mesg_header);

    /*
     * Set data to output buffer / file
     */
    switch( mesg_type )
    {
	case FILE_MESG:				/* READ FILE DATA	*/

	    gettimeofday( &tv, NULL );
	  
	    str_tm =
	      localtime( &tv.tv_sec );

	    len =
	      sprintf(
		      out_buf,
		      "%s/from_pid_%d_time_%d-%d-%d_%d:%02d:%02d:%06d",
		      NamedpDir,
		      getpid(),
		      str_tm->tm_year + 1900,
		      str_tm->tm_mon + 1,
		      str_tm->tm_mday,
		      str_tm->tm_hour,
		      str_tm->tm_min,
		      str_tm->tm_sec,
		      tv.tv_usec
		      );

	    mesg_header.length =
	      len + 1;

	    fd =
	      open(
		   out_buf,
		   O_WRONLY | O_CREAT
		   );

	    if( fd == -1 )
	    {
		GerrLogReturn(
			      GerrId,
			      "Can't open file name '%s' for write."
			      "error ( %d ) %s",
			      out_buf,
			      GerrStr
			      );
		return( NO_FILE );
	    }

	    state =
	      write(
		    fd,
		    in_buf,
		    length
		    );

	    close( fd );

	    if( state == -1 )
	    {
		GerrLogReturn(
			      GerrId,
			      "Error while writing file name '%s'.."
			      "error ( %d ) %s",
			      in_buf,
			      GerrStr
			      );
		return( NO_FILE );
	    }

	    break;
		   
	case DATA_MESG:				/* READ BUFFER DATA	*/
	  
	    mesg_header.length =
	      length;

	    memcpy(
		   out_buf,
		   in_buf,
		   length
		   );

	    break;


	default:				/* UNKNOWN TYPE		*/

	  GerrLogReturn(
			GerrId,
			"Unknown message type '%d'",
			mesg_type
			);

	  return( NO_FILE );
	    
    }

    out_buf -=
      sizeof(mesg_header);

    memcpy(
	   out_buf,
	   (char*)&mesg_header,
	   sizeof(mesg_header)
	   );

    *out_len =
      sizeof(mesg_header) + mesg_header.length;

    return( MAC_OK );
    
}


