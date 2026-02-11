#ifndef __CMESSAGE_INCLUDED__
#define __CMESSAGE_INCLUDED__

#include "misc.h"

class CMessage
{
protected:
    int nPosition;
    int nSize;
    int nMsgNo;
	char szLocal[32];
public:
    CMessage()
	{
		nPosition = 0;
		nSize     = 0;
		nMsgNo    = 0;
    };
    virtual ~CMessage(){};  // GAL 04/11/2013

    virtual int Read( Byte *bDest, int nCount ) = 0;
    virtual int Write( Byte *bSrc, int nCount ) = 0;
    virtual int Seek( int nPos ) = 0;
    virtual int GetSize(){ return nSize; };
    virtual int GetMaxSize() = 0;
    virtual int GetPosition(){ return nPosition; };
    virtual void Reset(){ nPosition = 0; };
    virtual void Clear(){ Reset(); nSize = 0; };
	virtual void SetLocal( char * szALocal ) { strcpy(szLocal, szALocal); }
	virtual char* GetLocal( ) { return szLocal; }

    int GetMsgNo(){ return nMsgNo; };
    int SetMsgNo( int nAMsgNo ){ nMsgNo = nAMsgNo; return 0; };
};

class CNamedPipe;
class CMemoryMessage : public CMessage
{
protected:
    int nMaxSize;
    Byte *bMessage;
    bool bAllocated;

protected:
	friend class CNamedPipe;
    virtual int SetSize( int nNewSize );

public:
    CMemoryMessage( int nMessageSize );
    CMemoryMessage( Byte *bData, int nMessageSize );
    ~CMemoryMessage();

    virtual int Read( Byte *bDest, int nCount );
    virtual int Write( Byte *bSrc, int nCount );
    virtual int Seek( int nPos );
    virtual int GetMaxSize(){ return nMaxSize; };
	virtual Byte* GetBuffer() { return bMessage; }

	bool operator==( CMemoryMessage& mes );

};

class CFileMessage : public CMessage
{
    int nFileHandle;
public:
    CFileMessage( char *szFileName );
    ~CFileMessage();

    virtual int Read( Byte *bDest, int nCount );
    virtual int Write( Byte *bSrc, int nCount );
    virtual int Seek( int nPos );
    virtual int GetSize();
    virtual int GetPosition();

    virtual void Reset(){ Seek(0); };
    virtual int GetMaxSize(){ return 0x7FFFFFFF; };
};

#endif // __CMESSAGE_INCLUDED__
