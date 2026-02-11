
//#include <sys/select.h>
extern "C"
{
#include <sys/time.h>
}

class CTimer
{
public:
	enum ResetType { rtAuto, rtManual } m_resettype;

	CTimer(): m_resettype(rtAuto)
	{
		gettimeofday(&m_last, &m_tz);
	}

	void LogTime( char * msg )
	{
		gettimeofday(&m_cur, &m_tz);

		tv_sec = m_cur.tv_sec - m_last.tv_sec;
		tv_usec = m_cur.tv_usec - m_last.tv_usec;
		if (tv_usec<0)
		{
			tv_usec += 1000000;
			tv_sec--;
		}

//		printf("{pid=%d, ppid=%d, time=%d.%03d}, %s\n", getpid(), getppid(), tv_sec, tv_usec/1000, msg); 
//		fflush(stdout);

		if( m_resettype == rtAuto ) Reset();
	}

	void Reset()
	{
		gettimeofday(&m_last, &m_tz);
	}


protected:
	struct timeval m_last;
	struct timeval m_cur;
	long tv_sec; 
    long tv_usec;
	struct timezone m_tz;
};

