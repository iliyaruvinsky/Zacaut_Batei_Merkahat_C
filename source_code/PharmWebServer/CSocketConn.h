#ifndef __CSOCKETCONNECTION_INCLUDED__
#define __CSOCKETCONNECTION_INCLUDED__

#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/unistd.h> // GAL 10/10/2013

#include "CConnection.h"

class CSocketConnection : public CConnection
{
protected:
	int nSocket;
	int nPort;

	virtual sockaddr *GetAddress( char *szAddress = NULL, int nAPort = 0 ) = 0;
	virtual int GetAddrLen() = 0;
	virtual int CreateSocket() = 0;
	virtual CSocketConnection *CreateConnection( int nChildSocket )= 0;
public:
	// CSocketConnection():CConnection(){};
	CSocketConnection():CConnection()
	{  nSocket = nPort = 0;	};
	~CSocketConnection(){};

    virtual int Init();
    virtual int Listen( char *szAddress, int nAPort );
    virtual int Connect( char *szAddress, int nAPort = 0, char *szLocalAddr = NULL );
    virtual int Accept( CConnection ** ppChildConnection );
    virtual int Close();

    virtual int CloseChild();
    virtual int OpenChild();

    virtual char *GetRemote() = 0;
    virtual char *GetLocal() = 0;
	virtual void SetRemote( char * szARemote ){};
    virtual void SetLocal( char * szALocal ){};
	virtual bool GetStatus();

    virtual int Receive( Byte *bData, int nCount, int nTimeOut = TO_DEFAULT );
    virtual int Send( Byte *bData, int nCount, int nTimeOut = TO_DEFAULT );
};

class CInetConnection : public CSocketConnection
{
	char szRemote[30];
	char szLocal[30];

private:
	sockaddr_in Address;

protected:
//	int nPort;

	virtual sockaddr *GetAddress( char *szAddress = NULL, int nAPort = 0 );
	virtual int GetAddrLen();
	virtual int CreateSocket();
	virtual CSocketConnection *CreateConnection( int nChildSocket );

public:
	CInetConnection():CSocketConnection(){};
	~CInetConnection(){};

    virtual char *GetRemote();
    virtual char *GetLocal();
};

/*
class CNamedPipeConnection : public CSocketConnection
{
public:
	CNamedPipeConnection():CSocketConnection(){};
	~CNamedPipeConnection(){};

    virtual int Init() = 0;
    virtual int Listen( int nAPort ) = 0;
    virtual int Connect( char *szAddress, int nAPort, char *szLocalAddr ) = 0;
    virtual int Accept( CConnection ** ppChildConnection ) = 0;
    virtual char *GetRemote() = 0;
    virtual char *GetLocal() = 0;
};
*/
#endif //__CSOCKETCONNECTION_INCLUDED__
