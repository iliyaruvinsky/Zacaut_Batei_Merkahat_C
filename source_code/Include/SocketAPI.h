#if !defined __SOCKET_H_
  
 #define __SOCKET_H_ 

#ifndef __CINT__
#define __CINT__
  typedef const int		CINT;
#endif

#ifndef __PBYTE__
#define __PBYTE__
  typedef unsigned char* PBYTE;
#endif


#ifndef __PINT__
#define __PINT__
typedef int*			PINT;
#endif

#ifndef __PCHAR__
#define __PCHAR__
typedef char*			PCHAR;
#endif

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


  int SocketCreate ( void );
  int SocketReceive( CINT nSock , PBYTE pbData , CINT nCount , CINT nTimeOut );
  int SocketSend( CINT nSock , PBYTE pbData , CINT nCount , CINT nTimeOut );
  
  int SocketConnect( int nSock , char* szServer , int nPort );
  struct sockaddr* GetAddress( char* szHost , int nPort );
  
#endif
