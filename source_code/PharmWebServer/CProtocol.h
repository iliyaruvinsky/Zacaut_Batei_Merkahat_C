#ifndef __CPROTOCOL_INCLUDED__
#define __CPROTOCOL_INCLUDED__

#include "CConnection.h"
#include "CMessage.h"

class CConnection;
class CFrame;
class CMessage;

class CProtocol
{
protected:
    CConnection *Connection;

protected:
    virtual int ReceiveFrame( CFrame *Frame, int nTimeOut = CConnection::TO_DEFAULT ) = 0;
    virtual int SendFrame( CFrame *Frame, int nTimeOut = CConnection::TO_DEFAULT ) = 0;

public:
    CProtocol( CConnection *AConnection )
		{ Connection = AConnection; };
    virtual ~CProtocol(){};  // GAL 04/11/2013

    CConnection *GetConnection(){ return Connection; };

    virtual int Hello() = 0;
    virtual int Bye() = 0;

    virtual int ReceiveMessage( CMessage *Message ) = 0;
    virtual int SendMessage( CMessage *Message ) = 0;
};

#endif // __CPROTOCOL_INCLUDED__
