#define __DEBUG_PRINT__
#if !defined( __DEBUG_PRINT_H_ )
  
#define __DEBUG_PRINT_H_

//#include "AddTypes.h"
typedef const char*	LPCSTR;

#ifdef __DEBUG_PRINT__ 
  
  #define DEFAULT_FILE_NAME				"Debug.log"
  #define DEFAULT_LOG_DIR				"/pharm/log"
  
  LPCSTR FileSubDirDateStamp( LPCSTR szSubDir , LPCSTR szLogFileName );
  void   SetDebugFileName( LPCSTR szSubDir , LPCSTR szLogFileName );
  void   DebugPrint( LPCSTR  lpcszText , ... );
	
#endif

#endif
