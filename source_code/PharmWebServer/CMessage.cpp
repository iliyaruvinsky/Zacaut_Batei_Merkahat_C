//===========================================================================
// Reshuma ltd. @{SRCH}
//								CMessage.cpp
//
//---------------------------------------------------------------------------
// DESCRIPTION: @{HDES}
// -----------
// Implementation for messages clases
//---------------------------------------------------------------------------
// CHANGES LOG: @{HREV}
// -----------
// Revision: 01.00
// By      : Boris Evsikov & Alexey Tulin
// Date    : 11/03/99 10:27:59
// Comments: First Issue
//===========================================================================


// Define source name
// static char THIS_FILE[] = __FILE__;
// static char *GerrSource = __FILE__;

#include <stdlib.h>

#include "misc.h"
#include "Log.h"
#include "CMessage.h"
#include "DebugPrn.h"

CMemoryMessage::CMemoryMessage( int nMessageSize ) : 
CMessage()
{
    bMessage   = new Byte[nMessageSize];
    nMaxSize   = nMessageSize;
    bAllocated = true;
}

CMemoryMessage::CMemoryMessage( Byte *bData, int nMessageSize ) : 
CMessage()
{
    bMessage   = bData;
    nMaxSize   = nMessageSize;
    bAllocated = false;
    nSize      = nMessageSize;
}

CMemoryMessage::~CMemoryMessage()
{
    if( bAllocated ) delete [] bMessage;
}

int CMemoryMessage::Read( Byte *bDest, int nCount )
{
    if( nCount > GetSize() - GetPosition() )
	nCount = GetSize() - GetPosition();

    memcpy( bDest, bMessage + GetPosition(), nCount );
    Seek( GetPosition() + nCount );

    return nCount;
}

int CMemoryMessage::Write( Byte *bSrc, int nCount )
{
    if( !bAllocated ) return -1;
    if( nCount > GetMaxSize() - GetPosition() )
	nCount = GetMaxSize() - GetPosition();

    memcpy( bMessage + GetPosition(), bSrc, nCount );
    SetSize( GetSize() + nCount );
    Seek( GetPosition() + nCount );

    return nCount;
}

int CMemoryMessage::Seek( int nPos )
{
    nPosition = nPos < nSize ? nPos : nSize;
    return nPosition;
};

int CMemoryMessage::SetSize( int nNewSize )
{
    nSize = nNewSize < nMaxSize ? nNewSize : nMaxSize;
    return nSize;
}


bool CMemoryMessage::operator==( CMemoryMessage& mes )
{
	if ( nSize != mes.nSize )
		return false;

	return !(::memcmp(bMessage, mes.bMessage, nSize));
}




CFileMessage::CFileMessage( char *szFileName ):CMessage()
{
    nFileHandle = open( szFileName, O_CREAT | O_RDWR ); 
	if( nFileHandle > 0 )
	{
		if( GetSize() >= 5 )
		{
			char szMsgNo[6];

			Read( (Byte*)szMsgNo, 5 );

			szMsgNo[5]	= 0;
			nMsgNo		= atoi( szMsgNo );
		}
	}
}

CFileMessage::~CFileMessage()
{
    if(nFileHandle > 0) close(nFileHandle);
}

int CFileMessage::Read( Byte *bDest, int nCount )
{
    return read( nFileHandle, bDest, nCount );
}

int CFileMessage::Write( Byte *bSrc, int nCount )
{
    return write( nFileHandle, bSrc, nCount );
}

int CFileMessage::Seek( int nPos )
{
    return lseek( nFileHandle, nPos, SEEK_SET );
}

int CFileMessage::GetSize()
{
    off_t nPos, nSize;
    nPos  = lseek( nFileHandle, 0, SEEK_CUR );
    nSize = lseek( nFileHandle, 0, SEEK_END );
    lseek( nFileHandle, nPos, SEEK_SET );

    return nSize;
}

int CFileMessage::GetPosition()
{
    return lseek( nFileHandle, 0, SEEK_CUR );
}
