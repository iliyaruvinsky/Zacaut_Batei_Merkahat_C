#include <stdio.h>
#include <stdarg.h> 

#include <time.h>

char*  pszLogFile = "/pharm/bin/trace.log";

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
