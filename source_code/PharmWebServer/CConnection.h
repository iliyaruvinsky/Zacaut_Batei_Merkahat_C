#ifndef __CCONNECTION_INCLUDED__
#define __CCONNECTION_INCLUDED__

class CConnection
{
    bool bTerminated;

public:
    int nRecvTimeOut;
    int nSendTimeOut;
    int nListenTimeOut;
	int nCallTimeOut;

protected:
    bool bConnected;

public:
    enum
    {
	TO_DEFAULT  = -1,
	TO_INFINITE =  0
    };

    CConnection()
	{ 
		bTerminated    = false; 
		bConnected     = false;
		nRecvTimeOut   = 0;
		nSendTimeOut   = 0;
		nListenTimeOut = 0;
		nCallTimeOut   = 0; // to prevent warning message GAL 04/11/2013
    };
    virtual ~CConnection(){};  // GAL 04/11/2013
    virtual int Init() = 0;
    virtual int Listen( char *szAddress, int nPort ) = 0;
    virtual int Connect( char *szAddress, int nPort = 0, char *szLocalAddr = NULL ) = 0;
    virtual int Accept( CConnection ** ppChildConnection ) = 0;
    virtual int Close() = 0;

    virtual int CloseChild() = 0;
    virtual int OpenChild() = 0;

    virtual bool Connected(){ return bConnected; };

    virtual char *GetRemote() = 0;
    virtual char *GetLocal() = 0;
	virtual void SetRemote( char * szARemote ) = 0;
    virtual void SetLocal( char * szALocal ) = 0;
	virtual bool GetStatus() = 0;

    virtual int Receive( Byte *bData, int nCount, int nTimeOut = TO_DEFAULT ) = 0;
    virtual int Send( Byte *bData, int nCount, int nTimeOut = TO_DEFAULT ) = 0;

    virtual int Terminate(){ bTerminated = true; return 0; };

    bool Terminated(){ return bTerminated; };
    void SetTimeOuts( int nARecvTimeOut, int nASendTimeOut, int nAListenTimeOut, int nACallTimeOut )
    {
		nRecvTimeOut   = nARecvTimeOut;
		nSendTimeOut   = nASendTimeOut;
		nListenTimeOut = nAListenTimeOut;
		nCallTimeOut   = nACallTimeOut;
    };


};

#endif // __CCONNECTION_INCLUDED__
