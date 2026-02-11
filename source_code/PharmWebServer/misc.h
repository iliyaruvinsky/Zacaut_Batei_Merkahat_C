#ifndef __MISC_INCLUDED__ 
#define __MISC_INCLUDED__ 

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>

#ifndef _LINUX_
typedef int bool;
#endif
typedef unsigned char Byte;

#define true  1
#define false 0

#define TRUE  1
#define FALSE 0

const int nMesBufSize = 1024*64;

//#define SwapShort(x) (unsigned short)(((unsigned short)(x)>>8)|((unsigned short)(x)<<8))

#define SwapShort(x) (unsigned short)( \
	(unsigned short)(x) >> 8 | \
	(unsigned short)(x) << 8 )

#define SwapLong(x) (unsigned long)( \
	(((unsigned long)(x)& 0xFF000000 ) >> 24 ) | \
	(((unsigned long)(x)& 0x00FF0000 ) >>  8 ) | \
	(((unsigned long)(x)& 0x0000FF00 ) <<  8 ) | \
	(((unsigned long)(x)& 0x000000FF ) << 24 )   )


//Errors
#define ERR_FATAL	-1

    //Connection
#define ERR_TIMEOUT			-101
#define ERR_CONNCLOSED		-102
#define ERR_TERMINATED		-103
#define ERR_RESET_BY_PEER	-104

    //Frame
#define ERR_INVFRAME		-201
#define ERR_DARINFRAME		-202
#define ERR_CRC				-203

    //Protocol
#define ERR_WRONGSEQ		-301
#define ERR_RESENT			-302
#define ERR_INVLEN			-303


#endif //__MISC_INCLUDED__ 
