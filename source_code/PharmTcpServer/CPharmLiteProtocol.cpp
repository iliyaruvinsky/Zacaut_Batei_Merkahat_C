
#include "misc.h"
#include "Log.h"
#include "CPharmLiteProtocol.h"
#include "DebugPrn.h"

// Define source name
// static char THIS_FILE[] = __FILE__;
static char *GerrSource = (PCHAR)__FILE__;

extern "C"
{
	#include "Global.h"
}

//extern "C" unsigned int ntohl( unsigned int netlong );
//extern "C" unsigned long strlen( char *str );
//extern "C" int strcmp( char *, char * );

static unsigned int PharmLiteMessageID = 0x20000828;
static char *szHello = PCHAR("PHARMACY TCPIP SERVER");


int CPharmLiteProtocol::ReceiveMessage( CMessage *Message )
{
	unsigned int nID, nLen;
	int nMsgNo;
	int ErrNo;
	Byte bBuffer[4096];

	if( !Connection->Connected() ) return ERR_FATAL;

	Message->Clear();

	ErrNo = Connection->Receive( (Byte*)&nID, 4 );
	if( ErrNo < 0 )
	{
//		if ((ErrNo != ERR_CONNCLOSED) && (ErrNo != ERR_RESET_BY_PEER))	// Don't log what's ordinary.
//			GerrLogMini (GerrId, PCHAR("CPharmLiteProtocol::ReceiveMessage 1: ErrNo = %d."), ErrNo);
//		prn_1("Receive Error on ID : %d.", ErrNo );
		return ErrNo;
	}

	if( nID != PharmLiteMessageID )
	{
		prn_1( "Invalid ID received (0x%x).", nID );
		GerrLogReturn( GerrId,
			PCHAR("Invalid ID received (0x%x)."),
			nID );
		return ERR_FATAL;
	}

	ErrNo = Connection->Receive( (Byte*)&nLen, 4 );
	if( ErrNo < 0 )
	{
		GerrLogMini (GerrId, PCHAR("ReceiveMessage 2: ErrNo = %d."), ErrNo);
		prn_1( "Receive Error on nLen : %d.", ErrNo );
		return ErrNo;
	}

	ErrNo = Connection->Receive( bBuffer, 5 );
	if( ErrNo < 0 )
	{
		GerrLogMini (GerrId, PCHAR("ReceiveMessage 3: ErrNo = %d."), ErrNo);
		prn_1( "Receive Error on nMsgNo : %d.", ErrNo );
		return ErrNo;
	}

	nMsgNo = atoi((char*)bBuffer);

	Message->SetMsgNo( nMsgNo );

//	prn_3( "Header received : ID = 0x%X : nLen = %d : nMsgNo = %d",	nID, nLen, nMsgNo );

	Message->Write( bBuffer, 5 );

	if( nLen < 0 || nLen > Message->GetMaxSize() )
		return ERR_INVLEN;

	unsigned int 
			nRest = nLen - 5,
			nBytesRead;

	while( Message->GetSize() < nLen )
	{
		nBytesRead = nRest < sizeof(bBuffer) ? nRest : sizeof(bBuffer);

		ErrNo = Connection->Receive( bBuffer, nBytesRead );
		if( ErrNo < 0 ) return ErrNo;

		if( Message->Write( bBuffer, nBytesRead ) < 1 )
			return ERR_INVLEN;

		nRest -= sizeof(bBuffer);
	}

	return 0;
}

int CPharmLiteProtocol::SendMessage( CMessage *Message )
{
	Byte bBuffer[4096];
	int ErrNo = 0;
	int nLen = 0;

	int nMsgNo = Message->GetMsgNo();
	Message->Reset();

	if( !Connection->Connected() ) return ERR_FATAL;

	Message->Reset();

	nLen = Message->GetSize();
	if( nLen < 0 ) return ERR_INVLEN;

	ErrNo = Connection->Send( (Byte*)&PharmLiteMessageID, 4 );
	if( ErrNo < 0 ) return ErrNo;

	ErrNo = Connection->Send( (Byte*)&nLen, 4 );
	if( ErrNo < 0 ) return ErrNo;

	while( nLen > 0 )
	{
		ErrNo = Message->Read( bBuffer, sizeof(bBuffer) );
		if( ErrNo < 0 ) return ErrNo;

		// DonR 04May2018: If, for some reason, there's nothing more to send, break out of the loop.
		if (ErrNo == 0)
			break;

		if( ErrNo > nLen ) ErrNo = nLen;
		nLen -= ErrNo;

		ErrNo = Connection->Send( bBuffer, ErrNo );
		if( ErrNo < 0 ) return ErrNo;
	}

	return 0;
}

int CPharmLiteServerProtocol::Hello()
{
	return Connection->Send( (Byte*)szHello, strlen(szHello)+1 );
}

int CPharmLiteClientProtocol::Hello()
{
	char	szHelloBuffer[513];
	int		nErrNo = 0;

	nErrNo = Connection->Receive( (Byte*)szHelloBuffer, strlen(szHello)+1 );
	if( nErrNo < 0 ) return nErrNo;

	if( strcmp(szHelloBuffer, szHello) != 0 )
	{
		GerrLogReturn( GerrId, PCHAR("Invalid Hello string received '%s'."), szHelloBuffer );
		return ERR_FATAL;
	}
	return 0;
}
