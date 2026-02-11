#ifndef	__PRIVATE_TYPES_DEFINITIONS_H__
#define __PRIVATE_TYPES_DEFINITIONS_H__

#ifndef __BOOLEAN__
#define __BOOLEAN__
typedef int BOOL;

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif
#endif	// BOOLEAN
  
#ifndef MAX_PATH
 #define MAX_PATH ( 256 )
 #define MAX_PATH_PLUSNULL ( MAX_PATH + 1 )
#endif
 
	 typedef const char*	LPCSTR;
     typedef FILE*			PFILE;

	 typedef unsigned char	UCHAR;
     typedef char*			PCHAR;
     
	 typedef UCHAR			BYTE;
	 typedef unsigned char* PBYTE;

	 typedef long			LONG;
	 typedef LONG*		    PLONG;

	 typedef int*			PINT;
	 typedef const int		CINT;

	 
	 enum Found
	 {
		enNo		= -1 ,
		enUnknown	= 0 ,
		enYes		// = 1 
	 };


#endif
