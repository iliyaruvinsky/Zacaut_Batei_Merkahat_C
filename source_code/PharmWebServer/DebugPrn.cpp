#include <stdio.h>
#include <stdarg.h> 

#include <time.h>

#include "Global.h"
#include "Log.h"
#include "DebugPrn.h"

char*  pszLogFile = PCHAR("/pharm/log/PharmWebTrace.log");
static char szRemote[ 256 ];

void DebugPrn( char *pFrm , ... )
{
	struct tm  *pTime;
	time_t      lTime;
	FILE* 		pFile;
	char 		szBuf[ 1024 ];
	char 		*pBufPos = szBuf;
	va_list 	values;

	if ( ( pFile = fopen( pszLogFile , "a" ) ) != NULL )
	{

 			time( &lTime );                
			pTime = localtime( &lTime );

	 		fprintf( pFile , "%02i/%02i/%04i %02i:%02i:%02i " ,
	          pTime->tm_mday , pTime->tm_mon + 1 , 2000 + pTime->tm_year % 100,
			  pTime->tm_hour , pTime->tm_min , pTime->tm_sec );

			va_start( values, pFrm );
			vsprintf( pBufPos, pFrm , values );

		   	fprintf( pFile , "%s\n" , szBuf );
		   	fclose( pFile );
	}

}

char* GetRemoteName( int nSocket  )
{
	struct sockaddr_in ClientAddr; 	    // client addres
	int nAddrLen = sizeof(ClientAddr);  // address lengt
	
	DebugPrn( PCHAR("GetRemoteName : socket = %d") , nSocket );
  #ifndef _LINUX_ // GAL 25/01/06
	if( getpeername( nSocket, (struct sockaddr*)&ClientAddr, &nAddrLen ) < 0 )
  #else
	if( getpeername( nSocket, (struct sockaddr*)&ClientAddr, ( socklen_t*) &nAddrLen ) < 0 )
  #endif
	{
		// GerrLogReturn(GerrId,
		//	"Can't get remote name.Error  %s.",
		//	strerror(errno) );
		// prn( "CInetConnection::GetRemote returns NULL" ); // GAL 25/01/06
		DebugPrn( PCHAR("GetRemoteName can't get remote name. Linux error %s."), strerror(errno) );
		return NULL;
	}

    strcpy( szRemote, inet_ntoa( ClientAddr.sin_addr ) );
    DebugPrn( PCHAR("Peer : \"%s\"" ), szRemote );
	return szRemote;
}

