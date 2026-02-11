
#include <sys/types.h>
#include <arpa/inet.h>
#include <strings.h>
#include <fcntl.h>
// #include <stropts.h>
#include "misc.h"

#include "CSocketConn.h"

#include "DebugPrn.h"

extern "C"
{
	#include "Global.h"
}
//NO_PRN 11/10/00
#include "Log.h"
// #include <string.h>    	// GAL 16/10/2013
// for strerror definition
// Define source name
// static char THIS_FILE[] = __FILE__;
static char *GerrSource = PCHAR(__FILE__);

extern "C" int errno;

int caught_signal; // For successful compiling only GAL 25/08/2004

int CSocketConnection::Close()
{

	if( close( nSocket ) < 0 )
	{
		GerrLogReturn( GerrId,
			PCHAR("Can't close socket (%d). Error: %s."),
			nSocket, strerror(errno) );
		return ERR_FATAL;
	}

	return 0;
}

int CSocketConnection::CloseChild()
{
	bConnected = false;
//	Close();
	return 0;
}

int CSocketConnection::OpenChild()
{
	bConnected = true;
	return 0;
}

bool CSocketConnection::GetStatus()
{
	return Connected();
}

int CSocketConnection::Receive( Byte *bData, int nCount, int nTimeOut )
{
	int 	nLen;				/*number of bytes received*/
    fd_set	ReadSet;
    struct  timeval TimeOut;
//	char* pszRemote;
	
    TimeOut.tv_sec = (nTimeOut == TO_DEFAULT) ? nRecvTimeOut : nTimeOut;
    TimeOut.tv_usec = 0;

    while( nCount > 0 )
    {
		FD_ZERO( &ReadSet );
		FD_SET( nSocket, &ReadSet );
		
		switch(select( nSocket + 1, &ReadSet, (fd_set*)NULL, (fd_set*)NULL,	&TimeOut ))
		{
		case -1:				/* SELECT ERROR		*/
			GerrLogReturn( GerrId,
				PCHAR("Error at select. error ( %d ) %s."),
				errno, strerror(errno) );
			bConnected = false;
			return ERR_FATAL;

		case 0:				/* TIMEOUT REACHED	*/
			bConnected = false;
			return( ERR_TIMEOUT );
			
		default:				/* DATA ARRIVES		*/
			if( ! FD_ISSET( nSocket, &ReadSet	) )
			{
				GerrLogReturn(GerrId,
					PCHAR("Other socket has signaled.Error  %s."),
					strerror(errno) );
				return ERR_FATAL;	// DonR 03May2018: Possible bug fix. If we return zero here, we can possibly get caught in an infinite loop - I think!
//				return 0;
			}
			break;
		} // end switch

		nLen = recv( nSocket, bData, nCount, 0 );

		switch( nLen )
		{
		case -1:				/* Error 		*/

			// DonR 30Apr2018: If errno = 104, that's an ordinary "reset by peer" - which really
			// shouldn't be treated any differently than a "plain vanilla" closed connection.
			// So if that's the case, just treat this as a closed connection and don't log anything.

			if (errno == 104)
			{
				bConnected = false;
				return ERR_RESET_BY_PEER;
			}
			else
			{
				GerrLogMini (GerrId, PCHAR("CSocketConnection::Receive - Data receive error. Error (%d) %s"), errno, strerror(errno) );
	
//			    pszRemote = GetRemoteName( nSocket );
//				DebugPrn( pszRemote != NULL  ?
//						 	pszRemote : (char*)"pszRemote == NULL" );

				bConnected = false;
				return ERR_FATAL;
			}
			

		case 0: 				/* Remote closed connect*/
			// DonR 05Mar2003: Switched to shorter version of log message, since this is not
			// an error but the normal (endlessly repeated) course of events.

			// DonR 06Mar2003: Took it one step further, and suppressed the message entirely.
			// At this rate, next I'll be stealing messages from other processes' log files!
			
			// GerrLogMini( GerrId, "TCP connection closed by remote." );

			bConnected = false;
			return ERR_CONNCLOSED;
		}


		nCount -= nLen;
		bData  += nLen;
	}

	return 0;
}

int CSocketConnection::Send( Byte *bData, int nCount, int nTimeOut )
{
	int     nLen;				/* number of bytes sent	*/
    fd_set  WriteSet;
    struct  timeval TimeOut;
//	int     nWritten = 0;

    TimeOut.tv_sec = (nTimeOut == TO_DEFAULT) ? nSendTimeOut : nTimeOut;
    TimeOut.tv_usec = 0;

    while( nCount > 0 )
    {
		FD_ZERO( &WriteSet );
		FD_SET( nSocket, &WriteSet );

		switch( select( nSocket + 1, (fd_set*)NULL, &WriteSet, (fd_set*)NULL, &TimeOut ))
		{
		case -1:				/* SELECT ERROR		*/
			GerrLogReturn( GerrId,
				PCHAR("Error at select. error ( %d ) %s."),
				errno, strerror(errno) );
			bConnected = false;
			return ERR_FATAL;

		case 0:				/* TIMEOUT REACHED	*/
			bConnected = false;
			return ERR_TIMEOUT;

		default:				/* DATA ARRIVES		*/
			if( !FD_ISSET( nSocket, &WriteSet ) )
			{
				GerrLogReturn(GerrId,
					PCHAR("Other socket has signaled.Error  %s."),
					strerror(errno) );
				bConnected = false;
				return ERR_FATAL;	// DonR 03May2018: Possible bug fix. If we return zero here, we can possibly get caught in an infinite loop - I think!
			}
			break;
		}

		nLen = send( nSocket, bData, nCount, 0 );

		switch( nLen )
		{
		case -1:				/* Error 		*/
			GerrLogMini( GerrId,
				PCHAR("CSocketConnection::Send - Data transmit error. Error (%d) %s"),
				errno, strerror(errno) );
			bConnected = false;
			return ERR_FATAL;

		case 0: 				/* Remote closed connect*/
			GerrLogReturn( GerrId, PCHAR("(send)Connection closed by client.") );
			bConnected = false;
			return ERR_CONNCLOSED;
		}
		
		nCount -= nLen;
		bData  += nLen;
	}

	return 0;
}

int CSocketConnection::Init()
{
	// Create a socket;
	nSocket = CreateSocket();
	
	if( nSocket < 0 )
	{
		GerrLogReturn( GerrId,
			PCHAR("Can't create a socket. Error: %s."),
			strerror(errno) );
		return ERR_FATAL;
	}

	if( fcntl( nSocket, F_SETFD, 1 ) < 0 )
	{
		GerrLogReturn( GerrId,
			PCHAR("Can't set socket flag. Error: %s."),
			strerror(errno) );
		return ERR_FATAL;
	}

	return 0;
}

int CSocketConnection::Listen( char *szAddress, int nAPort )
{

    // if( bind( nSocket, GetAddress( NULL, nAPort ), GetAddrLen() ) )
	if( bind( nSocket, GetAddress( NULL, nAPort ), GetAddrLen() ) == -1 )
    {	// GAL 14/10/2013
		GerrLogReturn( GerrId, PCHAR("Bind error: %s."), strerror(errno) );
		return( ERR_FATAL );
    }

    if( listen( nSocket,SOMAXCONN ) )
    {
		GerrLogReturn( GerrId, PCHAR("Listen error: %s."), strerror(errno) );
		return( ERR_FATAL );
    }
	
	nPort = nAPort;
	
	return 0;
} // Listen

int CSocketConnection::Connect( char *szAddress, int nAPort, char *szLocalAddr )
{
	//int nNonZero = 1;

    //setsockopt( nSocket, SOL_SOCKET, SO_DEBUG,
	//	  &nNonZero, sizeof(int) );

	//===========================================================================
	//								Get connected!
	//===========================================================================
	
    if( connect( nSocket, GetAddress( szAddress, nAPort ), GetAddrLen() ) )
    {
		GerrLogReturn( GerrId, 
			PCHAR("Can't connect to ip: %s.Error: %s."),
			szAddress, strerror(errno) );
		
		return( ERR_FATAL );
    }

	prn_1("CConnection: Connected to %s", szAddress );

	nPort = nAPort;
	bConnected = true;

	return 0;
}

int CSocketConnection::Accept( CConnection **ppChildConnection )
{
    int nChildSocket;                   // new socket from accept()	
	int nAddrLen = GetAddrLen();
	struct timeval Timeout;
	fd_set FDSet;

	(*ppChildConnection) = NULL;
	Timeout.tv_sec  = nListenTimeOut;
	Timeout.tv_usec = 0;

	FD_ZERO(&FDSet);
	FD_SET(nSocket, &FDSet);

	switch (select( nSocket+1, &FDSet, (fd_set*)NULL, (fd_set*)NULL, &Timeout ))
	{
	case -1:  // Error
		GerrLogReturn( GerrId,PCHAR("Select error: %s"), strerror(errno));
		return ERR_FATAL;
		
	case 0: // Timeout
		return 0;

	default:
		
		if( FD_ISSET(nSocket,&FDSet) )
		{
	     #ifndef _LINUX_	// GAL 25/01/06		
			nChildSocket = accept( nSocket, GetAddress(), &nAddrLen );
		 #else
			nChildSocket = accept( nSocket, GetAddress(), ( socklen_t* )&nAddrLen );
	     #endif
			if( nChildSocket < 0 )
			{
				GerrLogReturn(GerrId,
					PCHAR("Can't accept a new connection.Error  %s."),
					strerror(errno) );
				return ERR_FATAL;
			}
			
			if( fcntl( nChildSocket, F_SETFD, 0 ) < 0 )
			{
				GerrLogReturn( GerrId,
					PCHAR("Can't set socket flag. Error: %s."),
					strerror(errno) );
				return ERR_FATAL;
			}

			*ppChildConnection = CreateConnection(nChildSocket);

			if( !*ppChildConnection )
			{
				GerrLogReturn(GerrId,
					PCHAR("Can`t create child connection.Error  %s."),
					strerror(errno) );
				return ERR_FATAL;
			}

			(*ppChildConnection)->SetTimeOuts( 
				nRecvTimeOut, 
				nSendTimeOut,
				nListenTimeOut, 
				nCallTimeOut );


		}
		else
		{
			// Other socket has signaled
			GerrLogReturn(GerrId,
				PCHAR("Other socket has signaled.Error  %s."),
				strerror(errno) );
			return 0;
		}
		break;  // GAL 04/11/2013
	}

	if ( nChildSocket >= FD_SETSIZE )
	{    // GAL 25/01/06
		 GerrLogReturn( GerrId , 
				 PCHAR("ALERT : Child Socket Handler( %d ) >= FD_SETSIZE( %d )") ,
						nChildSocket , FD_SETSIZE );
	}

	return 1;
}

//***************************************************************************//
//***************************************************************************//
//
//			Internet (Unnamed) Socket Connection
//
//***************************************************************************//
//***************************************************************************//
int CInetConnection::CreateSocket()
{
	int nSock;
	int nNonZero = 1;

	nSock = socket( AF_INET, SOCK_STREAM, 0 );

    if( ( setsockopt( nSock, SOL_SOCKET, SO_REUSEADDR,
		  &nNonZero, sizeof(int) ) == -1 ) 
	 #ifndef _LINUX_ // GAL 25/01/06
        || ( setsockopt( nSock, SOL_SOCKET, SO_REUSEPORT,
		  &nNonZero, sizeof(int) ) == -1 ) 
	 #endif	
//		|| ( setsockopt( nSock, SOL_SOCKET, SO_KEEPALIVE,
//		  &nNonZero, sizeof(int) ) == -1 ) 
	  )
    {
		GerrLogReturn( GerrId,
			(PCHAR)"Can't set socket I/O options,"
			"Error: %s.", strerror(errno) );
    }

	return nSock;
}

int CInetConnection::GetAddrLen()
{
	return( sizeof(sockaddr_in) );
}

sockaddr *CInetConnection::GetAddress( char *szAddress, int nAPort )
{
    bzero( (char*)&Address, sizeof(struct sockaddr_in) );

    Address.sin_family = AF_INET;             //Internet family
    Address.sin_port   = htons( nAPort );     //Port number
	Address.sin_addr.s_addr = 
		szAddress ? inet_addr( szAddress ) : htonl( INADDR_ANY ); //Internet address

	return (sockaddr *)&Address;
}


CSocketConnection *CInetConnection::CreateConnection( int nChildSocket )
{
	CInetConnection *Conn = new CInetConnection();
	if( Conn )
	{
		Conn->nSocket    = nChildSocket;
		Conn->nPort      = nPort;
		Conn->bConnected = true;
	}
	return Conn;
}

char *CInetConnection::GetRemote()
{
	struct sockaddr_in ClientAddr; 	    // client addres
	int nAddrLen = sizeof(ClientAddr);  // address lengt

  #ifndef _LINUX_ // GAL 25/01/06
	if( getpeername( nSocket, (struct sockaddr*)&ClientAddr, &nAddrLen ) < 0 )
  #else
	if( getpeername( nSocket, (struct sockaddr*)&ClientAddr, ( socklen_t*) &nAddrLen ) < 0 )
  #endif
	{
		GerrLogMini (GerrId, PCHAR("CInetConnection::GetRemote can't get remote name. Linux error %s."), strerror(errno) );
		prn( "CInetConnection::GetRemote returns NULL" ); // GAL 25/01/06
		return NULL;
	}

    strcpy( szRemote, inet_ntoa( ClientAddr.sin_addr ) );
	return szRemote;
}

char *CInetConnection::GetLocal()
{
	struct sockaddr_in ClientAddr; 	    // client addres
	int nAddrLen = sizeof(ClientAddr);  // address lengt

  #ifndef _LINUX_
	if( getsockname( nSocket, (struct sockaddr *)&ClientAddr, &nAddrLen ) < 0 )
  #else
	if( getsockname( nSocket, (struct sockaddr *)&ClientAddr, (socklen_t*)&nAddrLen ) < 0 )
  #endif
	{
		GerrLogReturn(GerrId,
			PCHAR("Can't get local name.Error  %s."),
			strerror(errno) );
		return NULL;
	}

    strcpy( szLocal, inet_ntoa( ClientAddr.sin_addr ) );
	return szLocal;
}

