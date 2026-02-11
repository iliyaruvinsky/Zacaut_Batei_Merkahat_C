#ifndef __CDOCLITEPROTOCOL_INCLUDED__
#define __CDOCLITEPROTOCOL_INCLUDED__

#include "CProtocol.h"

class CPharmLiteProtocol : public CProtocol
{
protected:
    virtual int ReceiveFrame( CFrame *Frame, int nTimeOut = CConnection::TO_DEFAULT )
	{return 0; };
    virtual int SendFrame( CFrame *Frame, int nTimeOut = CConnection::TO_DEFAULT )
	{return 0; };

public:
    CPharmLiteProtocol( CConnection *AConnection ): 
	  CProtocol(AConnection) {};
    virtual ~CPharmLiteProtocol(){};  // GAL 04/11/2013

    virtual int ReceiveMessage( CMessage *Message );
    virtual int SendMessage( CMessage *Message );
};

class CPharmLiteServerProtocol : public CPharmLiteProtocol
{
public:
    CPharmLiteServerProtocol( CConnection *AConnection ): 
	  CPharmLiteProtocol(AConnection) {};

    virtual int Hello();
    virtual int Bye(){ return 0; };
};

class CPharmLiteClientProtocol : public CPharmLiteProtocol
{
public:
    CPharmLiteClientProtocol( CConnection *AConnection ): 
	  CPharmLiteProtocol(AConnection) {};

    virtual int Hello();
    virtual int Bye(){ return 0; };
};

#endif //__CDOCLITEPROTOCOL_INCLUDED__
