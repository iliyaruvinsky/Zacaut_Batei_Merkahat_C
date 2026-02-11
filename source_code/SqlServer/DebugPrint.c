  
  #include <stdarg.h> 
  #include <stdio.h>

  #include <sys/types.h>
  #include <sys/stat.h>  
  
  #include <time.h>

  #include "Global.h" 

  #include "DebugPrint.h"


 #ifdef __DEBUG_PRINT__

#ifndef MAX_PATH
#define MAX_PATH ( 256 )
#endif

  static int bFileNameInit = 0;   

  char szDebugFile[ 256 ];

  void DebugPrint( LPCSTR  lpcszText , ... ) 
  {
    va_list   par;
    FILE     *pFile;
	char      szBuf[ 10000 ];
    int       nLogDirLen;

	time_t      lTime;
	struct tm  *pTime;

    if ( ! bFileNameInit )
	{
		   SetDebugFileName( DEFAULT_LOG_DIR , DEFAULT_FILE_NAME );
	}

    if ( pFile = fopen( szDebugFile , "a" )  )
    {

		 time( &lTime );                
		 pTime = localtime( &lTime );

//		 fprintf( pFile , "%02i/%02i/%04i %02i:%02i:%02i " ,
//		          pTime->tm_mday , pTime->tm_mon + 1 , 2000 + pTime->tm_year % 100,
//				  pTime->tm_hour , pTime->tm_min , pTime->tm_sec );

		 fprintf( pFile , "%02i:%02i:%02i " ,
		          pTime->tm_hour , pTime->tm_min , pTime->tm_sec );
		 va_start( par , lpcszText );
         vsprintf( szBuf , lpcszText , par );
         va_end( par );

         fprintf( pFile , "%s\n" , szBuf ); 
		 fflush( pFile );
         fclose( pFile );

    }
   
  } // DebugPrintFile
    
  LPCSTR FileSubDirDateStamp( LPCSTR szSubDir , LPCSTR szLogFileName )
  {
    FILE *pFile;
	static char  szFileName[ MAX_PATH ];
	char  szLogSubDir[ MAX_PATH ];
	
	
	// sprintf( szLogSubDir , "./%s" , szSubDir );
	strcpy( szLogSubDir , szSubDir );

	pFile = fopen( szLogSubDir , "r" );
	if ( NULL == pFile )
	{
		 // sprintf( szFileName , "%s.%d" , szLogFileName , GetDate() ); 
		 sprintf( szFileName , "%s/%s.%d" , DEFAULT_LOG_DIR , 
										szLogFileName , GetDate() ); 
	}
	else
	{
		 fclose( pFile );

		 // sprintf( szFileName , "./%s/%s.%d" , szLogSubDir, 
		 sprintf( szFileName , "%s/%s.%d" , szLogSubDir, 
									szLogFileName , GetDate() );
		 
	}

    return szFileName;
	  
  } // FileSubDirDateStamp

  void SetDebugFileName( LPCSTR szSubDir , LPCSTR szLogFileName )
  {
	
	strcpy( szDebugFile , FileSubDirDateStamp( szSubDir , szLogFileName ) );
	
	bFileNameInit = 1;
  
  } // SetDebugFileName

 #endif

