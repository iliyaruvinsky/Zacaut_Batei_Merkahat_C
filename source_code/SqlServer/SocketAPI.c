  #include <stdio.h>

  #include <sys/socket.h> 
  #include <netinet/in.h> 
  #include <arpa/inet.h>  

  #include <strings.h>
  #include <string.h> 
  #include <fcntl.h>    
  #include <netdb.h>    

  #include <unistd.h> 

  #include <sys/select.h> 

  #include <errno.h> 
  #include "Global.h"


  // Define source name
  static char THIS_FILE[] = __FILE__;
  static char *GerrSource = __FILE__;

  #include "DebugPrint.h"

  #include "SocketAPI.h"

  extern int errno;  

 #ifndef TRUE
  #define TRUE 1
 #endif

 #ifndef FALSE
  #define FALSE 0
 #endif

  // #define SERVER "127.0.0.1"
  // #define PORT	  10001
		  
  // #define RECV_TIMEOUT 3 // sec
  // #define SEND_TIMEOUT 3 // sec

  int SocketCreate( void )
  {
	int nSock    = -1;
	int nNonZero =  1;

	nSock = socket( AF_INET , SOCK_STREAM , 0 );
	if ( ( setsockopt( nSock , SOL_SOCKET , SO_REUSEADDR ,
		 ( PCHAR )  &nNonZero, sizeof( int ) ) < 0 ) ||
		 ( setsockopt( nSock , SOL_SOCKET , SO_KEEPALIVE ,
				( PCHAR ) &nNonZero, sizeof( int ) ) < 0 ) )
	{
		   DebugPrint( "Can't set socket I/O options , Error : %s" ,
										strerror( errno ) );
		   nSock = -1;
	}

	if ( nSock >= FD_SETSIZE )
	{
		 DebugPrint( "ALERT : Socket Handler( %d ) >= FD_SETSIZE( %d )" ,
											         nSock , FD_SETSIZE );
	}

	return nSock;

  } // SocketCreate 

  int SocketReceive(  CINT nSock , PBYTE pbData , CINT nCount , CINT nTimeOut )
  {                                                                            
	fd_set ReadSet;
	struct  timeval TimeOut;
	int  nRet = -1; // return code  | number of bytes received

	TimeOut.tv_sec = nTimeOut;
	TimeOut.tv_usec = 0;
	FD_ZERO( &ReadSet );
	FD_SET( nSock , &ReadSet );
				
	switch( select( nSock + 1 , &ReadSet ,                                                   
			( fd_set* ) NULL, ( fd_set* ) NULL , &TimeOut ) )
	{
		case -1:    // SELECT ERROR                                                         
		  
		  DebugPrint( "Error %d at select :  %s.", errno , strerror( errno ) );
		  GerrLogMini (GerrId, "SocketReceive: Error %d (%s) at socket select.", errno, strerror (errno));
		  return nRet;                                                                                                                                                      
						
		case 0:    // TIMEOUT REACHED

		  return nRet;
						
		default:    // DATA ARRIVES                                                         

		  if ( ! FD_ISSET( nSock , &ReadSet ) )
		  {                                                                         
				 DebugPrint( "Other socket has signaled.Error  %s.",
								strerror( errno ) );                               
				 return nRet;                                                
		  }
		  break;                                                        
	
	} // end switch                                                                                                                       
				
	nRet = recv( nSock , pbData , nCount , 0 );
	switch( nRet )                                                               
	{
	  case -1:    // Error                                                           

		DebugPrint( "Data receive error : %s(#%d)" , strerror(errno) , errno );
		GerrLogMini (GerrId, "SocketReceive: Data receive error %d (%s).", errno, strerror(errno));
		break;                                                                                                                                                  
						
	  case 0:     // Remote closed connect

		// DonR 28Feb2024: Disabled logging of "connection closed by remote", since it's not
		// worth getting excited about.
		DebugPrint( "( Receive )Connection closed by remote." );                     
//		GerrLogMini (GerrId, "SocketReceive error: Connection closed by remote.");
		break;                                                                                                                                                 
						
	  default :                                                                
		break;                 
	} // switch by nRet                                   

	return nRet;                                        
		
  }// SocketReceive

  int SocketSend( CINT nSock , PBYTE pbData , CINT nBytes , CINT nTimeOut )
  {
	fd_set  WriteSet;
	struct  timeval TimeOut;
	int     nCount = nBytes; // number of bytes to send
	int     nLen;            // number of bytes sent
	int     nBytesSent = 0;  // All bytes sent
	int     nRet       = 0;
				
	TimeOut.tv_sec  = nTimeOut;
	TimeOut.tv_usec = 0;

	while( nCount > 0 )
	{
		FD_ZERO( &WriteSet );
		FD_SET ( nSock , &WriteSet );
		switch( select(nSock+1,(fd_set*)NULL,&WriteSet,(fd_set*)NULL,&TimeOut) )
		{
		  case -1:    // SELECT ERROR

		    DebugPrint( "Error #%d at select : %s.", errno , strerror( errno ) );
			GerrLogMini (GerrId, "SocketSend: Error %d (%s) on socket select.", errno, strerror (errno));
		    return nRet;

		  case 0:     // TIMEOUT REACHED 

			return nRet;                                                                                                                                        
								
		  default:    // DATA ARRIVES

			if ( ! FD_ISSET( nSock , &WriteSet ) )
			{

				DebugPrint("Other socket has signaled.Error : %s.",strerror(errno));
			}
			break;
		} // Switch for select
						
		nLen = send( nSock , ( char* ) pbData , nCount , 0 );
		DebugPrint( "send returns %d" , nLen );
		switch( nLen )
		{
		  case -1:    // Error

			DebugPrint("Data transmit error : %s ( %d )",strerror(errno),errno );
			GerrLogMini (GerrId, "SocketSend: Data transmit error %d (%s).", errno, strerror (errno));
			return nRet;
								
		  case 0:     // Remote closed connect

			DebugPrint( "(send)Connection closed by client." );
			return nRet;

		} // switch on length                                                                                                         
		
		nBytesSent += nLen;
		nCount     -= nLen;
		pbData      += nLen;
	} // while on nCount
	
	return nRet = nBytesSent;
		
  } // SocketSend                

  int SocketConnect( int nSock , char* szServer , int nPort )
  {
    int bRet = FALSE;

    int nRetVal = connect( nSock , GetAddress( szServer , nPort ) , 
																																								sizeof( struct sockaddr_in ) );
	if ( nRetVal )
    {
		 DebugPrint( "Can't connect to ip: \"%s\" (Port %d). Error #%d: %s.",
						szServer , nPort , errno , strerror (errno));
		 GerrLogMini (GerrId, "SocketConnect: Can't connect to %s port %d. Function connnect() returned %d (%s).",
						szServer , nPort , errno , strerror (errno));

    }
    else
	{
		DebugPrint( "Connected to %s ( Port %d )" , szServer , nPort );

		bRet = TRUE;
	}
				
	return bRet;
		
  } // Connect



// DonR 07Nov2010: Add logic to try gethostbyname() before attempting to interpret
// the host as a dotted-number address. Only if gethostbyname() fails do we fall
// back to trying the interpret the host as a numeric IP address.
struct sockaddr *GetAddress (char* szHost, int nPort)
{
	static struct sockaddr_in	Addr;
	unsigned long				server_ip_addr;
    struct hostent				*hostent_ptr;


	memset ((void *)&Addr, 0, sizeof(Addr));


	// Try getting host by name first.
	hostent_ptr = gethostbyname (szHost);

	if (hostent_ptr != NULL)
	{
		// Good lookup of host name.
		server_ip_addr = *(unsigned long *)hostent_ptr->h_addr;
		Addr.sin_addr.s_addr = server_ip_addr;
	}
	else
	{
		// As of November 2010, nobody should be calling this routine with an x.x.x.x IP address.
		// If someone does, print a diagnostic to log, but (at least for now) DON'T fail.
		if (szHost)
		{
			DebugPrint ("GetAddress: received request for host %s. DEPRECATED - CHANGE TO NAME LOOKUP!", szHost);
			Addr.sin_addr.s_addr = inet_addr (szHost);	// Internet address
		}
		else
		{
			DebugPrint ("GetAddress: received request for NULL host lookup!");
			Addr.sin_addr.s_addr = htonl (INADDR_ANY);	// Internet address
		}
	}	// (Deprecated) lookup of x.x.x.x IP address.


	Addr.sin_family      = AF_INET;			// Internet family
	Addr.sin_port        = htons (nPort);	// Port number

	return (struct sockaddr *)&Addr;
} // GetAddress
