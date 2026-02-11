#ifndef __LOG_INCLUDED__
#define __LOG_INCLUDED__

//#define NO_PRN

#ifndef NO_PRN
#define prn(msg) \
printf("{pid=%d, ppid=%d}, %s\n", getpid(), getppid(), msg); \
fflush(stdout);

#define prn_1(msg,d) \
{ \
	char buf[256]; \
	sprintf(buf,msg,d); \
	prn(buf); \
}

#define prn_2(msg,d,e) \
{ \
	char buf[256]; \
	sprintf(buf,msg,d,e); \
	prn(buf); \
}

#define prn_3(msg,d,e,f) \
{ \
	char buf[256]; \
	sprintf(buf,msg,d,e,f); \
	prn(buf); \
}

#define log_timer(msg) timer.LogTime(msg)

#else  // NO_PRN
#define prn
#define prn_1
#define prn_2
#define prn_3
#define log_timer
#endif  // NO_PRN

#include "timer.h"

#ifdef MAIN
CTimer timer;
#else
extern CTimer timer;
#endif

#ifndef __pchar_defined
	typedef char* PCHAR;
	#define __pchar_defined
#endif

#endif // __LOG_INCLUDED__
