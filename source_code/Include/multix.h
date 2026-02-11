#ifndef	MultiXHIncluded
#define	MultiXHIncluded

#define _MSC_VER 500

//oracle#ifdef	NON_SQL
//oracle#define EXEC
//oracle#define SQL
//#define $typedef typedef
//oracle#endif

#include	<stdio.h>
#include	<stdlib.h>

#define	MultiXVersion					233
#define MultiXOldestVersionSupported	220

#define	MUtilArrayIndexNew				-1
#define	MUtilQueueSeqInvalid			-1
#define	MUtilArrayIndexInvalid			-2
#define	MMdxXonChar						0x11
#define	MMdxXoffChar					0x13

#define	MMdxParityNone		0l
#define	MMdxParityEven		0x80000000l
#define	MMdxParityOdd		0xc0000000l
#define	MMdxNoXonXoff		0x20000000l
#define	MMdxWordSize8		0x10000000l


#ifndef	SHRT_MAX
#include	<limits.h>
#endif

#if	SHRT_MAX	==	INT_MAX
#define	Use16
// #define	MaxMdxAllocSize	0xff00l
#elif	LONG_MAX	==	INT_MAX
#define	Use32
// #define	MaxMdxAllocSize	0x7fffff00l
#else
#define Use64
// CompileError	Unknown	word	size
#endif

// DonR 23Aug2005: Linux compatibility.
#ifdef _LINUX_
	#define BEGIN_ENUM typedef enum {
	#define END_ENUM(x) } x;
#else
	#define BEGIN_ENUM enum {
	#define	END_ENUM(x)	END	; typedef	Int16	x
#endif

#if	defined(__MSDOS__)	||	defined(MSDOS)
	#define	MsDosOs
	#ifdef	_WINDOWS
		#define	WindowsOs
	#endif
#elif	defined(__OS2__)
	#define	Os2Os
#elif	defined(_M_UNIX)
	#define	ScoUnixOs
#elif	defined(_WIN32)
	#define WindowsNtOs
#elif	defined(_LINUX_)
#else
	CompileError	UnKnown	Operating	System
#endif

#if	defined(ScoUnixOs) || defined(_LINUX_)
	#define	cdecl
#endif


#ifdef	__BORLANDC__
	#define	BorlandC
	#define	CDeclPtr				cdecl	*
	#define	MsDosCdecl				cdecl
	#define	CompilerDepMainAttr		cdecl

#elif	defined(_MSC_VER)
	#define	MicrosoftC
	#if defined(MsDosOs)	||	defined(WindowsOs)
		#define StdCall 				__pascal
	#elif	defined(ScoUnixOs) || defined(_LINUX_)
		#define	StdCall	cdecl
	#else
		#define StdCall 				__stdcall
	#endif
	#define	CDeclPtr				cdecl	*
	#define	CompilerDepMainAttr		cdecl
	#define	MsDosCdecl				cdecl

#elif	defined(__IBMC__)
	#define	IbmC
	#define StdCall 				_Optlink
	#define	CompilerDepMainAttr 	_Optlink
	#define	CDeclPtr				*
	#define	MsDosCdecl

#elif	defined(__AttC__)
	#define	AttC
	#define	CompilerDepMainAttr
	#define	MsDosCdecl
#else
	CompileError	Unknown	Compiler
#endif



#if	defined(MsDosOs)	||	\
	defined(Os2Os)		||	\
	defined(ScoUnixOs)	||	\
	defined(_LINUX_)	||	\
	defined(WindowsNtOs)
#define	NoNetworkTranslation
#else
CompileError	Unknown	Operating	System
#endif


#if	defined(MicrosoftC)	&&	(_MSC_VER	==	700)
#define	TimerAjustValue			2209075200l
#else
#define	TimerAjustValue			0l
#endif


#ifdef	__cplusplus
#define Public	extern	"C"
#else
#define	Public
#endif
#define	PrivateD	static
#define Private 	static
#define	PrivateC	static

#define	BEGIN		{
#define	END			}
#define	ELSE		else
#define	IF if		(
#define	THEN		)
#define	AND			&&
#define	OR			||
#define	LAND		&
#define	LOR			|
#define	XOR			^
#define	MOD			%
#define	SLAND(x,y)	(x)	&= (y)
#define	SLOR(x,y)	(x)	|= (y)
#define	SXOR(x,y)	(x)	^= (y)
#define	LANDE		&=
#define	LORE		|=
#define	XORE		^=
#define	MODE		%=
#define	REPEAT		do
#define	UNTIL(x)	while	(x)
#define	WHILE		while	(
//yulia291198#define	FOREVER		for(;;)
#define	FOR			for	(
#define	DO			)
#define	GETS		=
#define	EQ			==
#define	NE			!=
#define	GE			>=
#define	LE			<=
#define	LT			<
#define	GT			>
#define	NOT			!
#define	LNEG		~
#define	NOTEQ		!=

#define	On	1
#define	Off	0

#define	MswGet(x)	(UInt16)(((UInt32)(x))	>>	16)
#define	LswGet(x)	(UInt16)(((UInt32)(x))	LAND	0xffff)

#define	MswSet(x,y)	x	GETS	(y	<<	16)	LOR	LswGet(x)
#define	LswSet(X,y)	x	GETS	y	LOR	((UInt32)(x)	LAND	0xffff0000)


#define	MakeFp(_Seg,_Off)	((FarPtr)(((UInt32)_Seg<<16)	LOR	_Off))

#define	TypedMax(t,a,b)	(t)(((t)(a)	> (t)(b)	?	(a)	: (b)))
#define	TypedMin(t,a,b)	(t)(((t)(a)	< (t)(b)	?	(a)	: (b)))

#define	Max16(a,b)	TypedMax(Int16,a,b)
#define	Min16(a,b)	TypedMin(Int16,a,b)

#define	MaxU16(a,b)	TypedMax(UInt16,a,b)
#define	MinU16(a,b)	TypedMin(UInt16,a,b)

#define	Max32(a,b)	TypedMax(Int32,a,b)
#define	Min32(a,b)	TypedMin(Int32,a,b)

#define	MaxU32(a,b)	TypedMax(UInt32,a,b)
#define	MinU32(a,b)	TypedMin(UInt32,a,b)

#define	SwapInt16(_Num)	(UInt16)((((UInt16)(_Num))	>>	8)	LOR	(((UInt16)(_Num))	<<	8))

#define SwapInt32(_Num) (UInt32)( \
				((((UInt32)(_Num)) & 0xFF000000 ) >> 24 ) | \
				((((UInt32)(_Num)) & 0x00FF0000 ) >>  8 ) | \
				((((UInt32)(_Num)) & 0x0000FF00 ) <<  8 ) | \
				((((UInt32)(_Num)) & 0x000000FF ) << 24 )   \
				)

#define	MIntSwap(_Int)		\
	(sizeof((_Int))	EQ	sizeof(Int16)	?	\
	(int)SwapInt16((UInt16)(_Int))			:	\
	(int)SwapInt32((UInt32)(_Int)))

#ifdef	ScoUnixOs

#define	MIntSwapSet(_Int)	\
	IF	sizeof((_Int))	EQ	sizeof(Int16)	THEN	\
	BEGIN											\
		_Int	GETS	SwapInt16((UInt16)(_Int));	\
	END else										\
	BEGIN											\
		*((UInt32	*)&_Int)	GETS	SwapInt32((UInt32)(_Int));	\
	END
#else
#define	MIntSwapSet(_Int)	_Int	=	MIntSwap(_Int)
#endif

#ifdef	NoNetworkTranslation

#define	MNetToHostInt(_Int)	(_Int)
#define	MHostToNetInt(_Int)	(_Int)
#define	MNetToHostIntSet(_Int)
#define	MHostToNetIntSet(_Int)

#else

#define	MNetToHostInt(_Int)	MIntSwap(_Int)
#define	MHostToNetInt(_Int)	MIntSwap(_Int)
#define	MNetToHostIntSet(_Int)	_Int	GETS	MIntSwap(_Int)
#define	MHostToNetIntSet(_Int)	_Int	GETS	MIntSwap(_Int)
#endif

#define	Use(x)	IF	x THEN
#ifdef	__cplusplus
#define	NullP	0
#else
#define	NullP			(void	*)0
#endif
#define	MByteArray(x)	struct	{char	Byte[x];}
#define	IsNullP(x)		((x)	EQ	NullP)
#define	NotNullP(x)		((x)	NE	NullP)
#define	IfNotNullP(x)	IF	(x)	NE NullP	THEN
#define	IfNullP(x)		IF	(x)	EQ NullP	THEN
#define	MCreateNew(_X)	IfNullP((_X)	GETS	MdxCalloc(sizeof(*(_X))))	FatalError("No	Memory");
#define	MOccurs(_X)		(sizeof(_X)	/ sizeof((_X)[0]))

#define	FatalError(_ErrorText)	FatalErrorHandler(_ErrorText)

#define	NoDataDestructor	(TEventDataDestructor)0

#define	MMdxSetNewCrc(Crc,Byte)			\
BEGIN						\
	Crc	GETS	(UInt16)((Crc>>8)^MdxCrc16Table[(((Crc)^((UInt16)(Byte)))&0x0ff)]);	\
END
/* oracle change 2000.05.02*/

typedef			short	Int16;
typedef			int	Int32;
typedef			int	Int;
typedef			char	Int8;
typedef			double	Double64;
typedef			float	Float32;

typedef		unsigned	short	UInt16;
typedef	unsigned			UInt32;
typedef	unsigned		short	UShort;
typedef	unsigned		int	TBit;
typedef	unsigned		int	UInt;
typedef	unsigned		char	UInt8;

typedef				Int32	TFileDescr;
typedef				Int32	TLinkDescr;
typedef				Int16	TSLinkDescr;
typedef				Int16	TPtrArrayIndex;
typedef				Int32	TIndex;
typedef				Int32	TCounter;
typedef				Int16	TSCounter;
typedef				Int32	TQueueSeq;
typedef				Int32	TReqSeq;
typedef				Int16	TSReqSeq;
typedef				Int32	TListKey;
typedef				Int8	*Int8Ptr;
typedef				Int32	TMdxMsgSize;
typedef				Int16	TMdxBlockNo;
typedef				Int32	TMdxProcId;
typedef				Int16	TMdxMsgCode;
typedef				Int8	TSResult;
typedef				Int8	TSBoolean;

typedef	char	TMdxPassword[10];
typedef	char	TMdxLinkName[30];
typedef	char	TMdxService[80];
typedef	char	TMdxNetAddr[20];
typedef	char	TMdxProcDescr[24];

typedef					UInt16	TSIndex;
typedef					UInt16	TBufSize;
typedef					UInt16	TMdxObjectId;
typedef					UInt32	TBaudRate;
typedef					UInt32	TTimer;
typedef					UInt32	TTimerId;
typedef					UInt32	TTimerTag;
typedef					UInt32	*UInt32Ptr;
typedef					UInt8	*UInt8Ptr;
typedef					void	Void;
typedef					UInt16	TMdxMsgPri;
typedef					UInt32	TMdxTime;
typedef					UInt16	TMdxVersion;
typedef					UInt8	TOnOff;



#if	defined(IbmC)	||	defined(MicrosoftC)	||	defined(AttC)
#pragma	pack(1)
#elif	defined(BorlandC)
#pragma	-a
#else
CompileError	UnKnown	Complier
#endif

typedef union	TVarChar
BEGIN
	Int16	Len;
	Int8	Text[2];
END TVarChar;

#if	defined(IbmC)	||	defined(MicrosoftC)	||	defined(AttC)
#pragma	pack()
#elif	defined(BorlandC)
#pragma	-a.
#else
CompileError	UnKnown	Complier
#endif



BEGIN_ENUM
	MdxStartAsGateWay		=	0x0001,
	MdxUseDualEventQueues	=	0x0002,
	MdxReportAllProcesses	=	0x0004,
	MdxReportAllLinks		=	0x0008,
	MdxStartAsRouter		=	0x0010,
	MdxDisplayVersion		=	0x4000
END_ENUM(TMdxStartupAttributes);


BEGIN_ENUM

	MdxLinkTypeAcceptOnly	=	0,/*	Indicates	a	server	link	type,
								usually	as a	result	of	accept
								on	tcp/ip	or	something	like	that
							*/
	MdxLinkTypeFirst			=	1,
	MdxLinkTypeAsyncLocal		=	1,
	MdxLinkTypeNetBios			=	2,
	MdxLinkTypeTcpIpSocket		=	3,
	MdxLinkTypeNPipe			=	4,
	MdxLinkTypeAsyncModem		=	5,
	MdxLinkTypeX25				=	6,
	MdxLinkTypeLoopBack			=	7,
	MdxLinkTypeSpxIpx			=	8,
	MdxLinkTypeLast
END_ENUM(TMdxLinkType);

BEGIN_ENUM
	MdxL2DlcProtoNone		=		0,
	MdxL2DlcProtoHdlc		=		1
END_ENUM(TMdxL2DlcProtocol);


BEGIN_ENUM
	Failure	=	0,
	Success	=	1
END_ENUM(TResult);

BEGIN_ENUM
	False	=	0,
	True	=	1
END_ENUM(TBoolean);


BEGIN_ENUM
	MdxCallOk,
	MdxInvalidPassword
END_ENUM(TMdxCallError);


BEGIN_ENUM
	L1WarnDontFreeSendBuf	=	-300	,
	L1ErrLinkNameMissing			,
	L1ErrUnableToInitSocket			,
	L1ErrAcceptError				,
	L1ErrLinkNameTooLong			,
	L1ErrInvalidLinkType			,
	L1ErrTooManyLinks				,
	L1ErrInvalidL1Ld				,
	L1ErrUnableToOpenAsync			,
	L1ErrUnableToSetupAsync			,
	L1ErrUnableToCreateNPipe		,
	L1ErrIllegalOperation			,
	L1ErrInvalidReqCode				,
	L1ErrTimeout					,
	L2ErrTooManyLinks				,
	L2ErrTooManyConnectRetries		,
	L2ErrSendQueueFull				,
	L2ErrSendBufferEmpty			,
	L2ErrSendBufferTooBig			,
	L2ErrMaxResetCount				,
	L2ErrMaxPollCount				,
	L2ErrInvalidConnectMode			,
	L2ErrInvalidFd					,
	L2ErrInvalidApprSeq				,
	L2ErrInvalidFrameType			,
	L2ErrInvalidReqCode				,
	L2ErrConnectFailed				,
	L2ErrNoResetResponse			,
	L2ErrNoPollResponse				,
	L3ErrProcessAlreadyDefined		,
	L3ErrNoLinkAvailable			,
	L3ErrSendBufferEmpty			,
	L3ErrMsgCanceled				,
	L3ErrInvalidReqCode				,
	L3ErrProcessDoesNotExist		,
	L3ErrNoGateWayAvailable			,
	L4ErrMsgSendCanceled			,
	L4ErrInvalidReqCode				,
	L4ErrInvalidReqSeq				,
	L5ErrProcessClosed				,
	L5ErrInvalidReqCode				,
	L6ErrInvalidReqCode				,
	UtErrTooManyLinks				,
	UtErrInvalidHashTable			,
	UtErrDuplicateHashKey			,
	UtErrHashKeyNotFound			,
	UtErrInvalidStackTable			,
	UtErrInvalidParam				,
	TsrErrMsgNotFound				,
	L2ErrMissingBuffer
END_ENUM(TMdxError);

BEGIN_ENUM
	DdeErrFirstError			=	-400,
	DdeErrLastError				=	-381,
	DbmErrFirstError			=	-380,
	DbmErrLastError				=	-330,
	MdxErrFirstError			=	-300,
	MdxErrLastError				=	-201,
	TpErrFirstError				=	-200,
	TpErrLastError				=	-101,
	MdErrSessionRestarted		=	-100,
	MdErrMsgTimedOut,
	MdErrMsgCanceled,
	MdErrInvalidProcId,
	MdErrUnableToOpenLink,
	MdErrInvalidLinkLd,
	MdErrInvalidDriverIndex,
	MdErrLinkDisconnected,
	MdErrDataReplyNotAllowed,
	MdErrInvalidReplyInfo,
	MdErrMsgIsEmpty,
	MdErrInvalidEventCode,
	MdErrLinkClosed,
	MdErrLinkAlreadyOpened,
	MdErrProcessNotReady,
	MdErrNoMemory,
	MdErrLinkTypeNotSupported,
	MdErrNoError	=	0
END_ENUM(TMdxApplError);


BEGIN_ENUM
	MdxDdeErrInvalidMsgCode	=	-400,
	MdxDdeErrAdviseStopped,
	MdxDdeErrInvalidDataFormat,
	MdxDdeErrInvalidItem
END_ENUM(TMdxDdeError);



BEGIN_ENUM
	MdxConnectModeListen,
	MdxConnectModeCall
END_ENUM(TMdxConnectMode);

BEGIN_ENUM
	MdxTimerEvent						=	110,
	MdxStdInAvailable					=	111,
	MdxIdleEvent						=	120,
	MdxL1L2EvConnectEnabled				=	130,
	MdxL1L2EvAcceptEnabled				=	140,
	MdxL1L2EvReadEnabled				=	150,
	MdxL1L2EvWriteEnabled				=	160,
	MdxL1L2EvLinkDisconnected			=	170,
	MdxL2L3EvSendComplete				=	180,
	MdxL2L3EvRecvComplete				=	190,
	MdxL2L3EvLinkClosed					=	200,
	MdxL2L3EvConnectComplete			=	210,
	MdxL2L3EvQueuesReset				=	220,
	MdxL2L3EvTransmitEnabled			=	225,
	MdxL2L3EvResumeTransmision			=	227,
	MdxL3L4EvMsgCancled					=	230,
	MdxL3L4EvMsgSentOk					=	240,
	MdxL3L4EvMsgReceived				=	250,
	MdxL3L4EvProcessReady				=	260,
	MdxL3L4EvProcessFailed				=	270,
	MdxL4L5EvMsgReceived				=	280,
	MdxL4L5EvControlMsgReceived			=	285,
	MdxL4L5EvMsgCancled					=	290,
	MdxL4L5EvMsgSentOk					=	300,
	MdxL4L5EvProcessReady				=	310,
	MdxL4L5EvProcessFailed				=	320,
	MdxL4L5EvProcessRestarted			=	330,
	MdxL4L5EvProcessConnected			=	340,
	MdxL5L6EvMsgReceived				=	350,
	MdxL5L6EvControlMsgReceived			=	355,
	MdxL5L6EvCallReqReceived			=	360,
	MdxL5L6EvCallRejected				=	370,
	MdxL5L6EvMsgCancled					=	380,
	MdxL5L6EvMsgSentOk					=	390,
	MdxL5L6EvCallCompleted				=	400,
	MdxL5L6EvProcessReady				=	402,
	MdxL5L6EvProcessNotReady			=	404,

	MdxEvDataMsgReceived				=	410,
	MdxEvDataReplyReceived				=	420,
	MdxEvSendMsgCompleted				=	430,

	MdxEvCallReqReceived				=	440,
	MdxEvCallRejected					=	450,
	MdxEvCallCompleted					=	460,


	MdxEvProcessReady					=	462,
	MdxEvProcessNotReady				=	464,

	MdxEventApplInit					=	470,
	MdxEvLinkStatus						=	480,
	MdxEvProcessAdded					=	490,
	MdxEvProcessRemoved					=	491,
	MdxEvNonMultiXDataReceived			=	500,


	MdxApplEventCodeFirst				=	30000,
	MdxApplEventCodeLast				=	31000

END_ENUM(TMdxEventCode);

BEGIN_ENUM
	/*	Application	Assigned	Attributes	*/

	MdxResponseRequired	=	0x0001,
	MdxReportSuccess	=	0x0002,
	MdxReportError		=	0x0004,
	MdxResendOnRestart	=	0x0008,
	MdxReportAll		=	0x0007,
	MdxSendReliable		=	0x000f,
	MdxUseDataBuffer	=	0x0010,
	MdxUseDbProtection	=	0x0020,
	MdxUseMultiCast		=	0x0040,
	MdxDontUseAllocMod	=	0x0100,

	/*	Kernel	Assigned	Attributes	*/

	MdxL6IsReplyMsg			=	0x1000
END_ENUM(TMdxSendAttr);

BEGIN_ENUM
	MdxL2EventHandlerIndex			=	2,
	MdxL3EventHandlerIndex			=	3,
	MdxL4EventHandlerIndex			=	4,
	MdxL5EventHandlerIndex			=	5,
	MdxL6EventHandlerIndex			=	6,
	MdxL7EventHandlerIndex			=	7,
	MdxTimerMgrEventHandlerIndex	=	8,
	MdxLinkMgrEventHandlerIndex		=	9,
	MdxProcMgrEventHandlerIndex		=	10,
	MdxEventHandlerIndexLast
END_ENUM(TMdxEventHandlerIndex);


BEGIN_ENUM
	MdxTpmFirstMsgCode			=	-32700,
	MdxTpmLastMsgCode			=	-32601,
	MdxMvxFirstMsgCode			=	-32500,
	MdxMvxLastMsgCode			=	-32401,
	MdxSqlFirstMsgCode			=	-32400,
	MdxSqlLastMsgCode			=	-32351,
	MdxDdeAdviseStartMsgCode	=	-32100,
	MdxDdeAdviseDataMsgCode		=	-32099,
	MdxDdeDataReqMsgCode		=	-32098,
	MdxDdeDataReplyMsgCode		=	-32097,
	MdxDdePokeMsgCode			=	-32096,
	MdxDdeExecuteMsgCode		=	-32095
END_ENUM(TMdxReservedMsgCodes);

BEGIN_ENUM
	MdxDdeDataFormatIsText	=	1
END_ENUM(TMdxDdeDataFormat);


BEGIN_ENUM
	MdxSeekStart,
	MdxSeekFwrd,
	MdxSeekBack,
	MdxSeekEnd
END_ENUM(TMdxMsgSeekOrigin);


#if	defined(IbmC)	||	defined(MicrosoftC)	||	defined(AttC)
#pragma	pack(2)
#elif	defined(BorlandC)
#pragma	-a
#else
CompileError	UnKnown	Complier
#endif

typedef	struct	TMdxMsg
BEGIN
	TMdxMsgCode		MsgCode;	// --> for debug purpose only
	TMdxMsgSize		BytesRead;
	TMdxMsgSize		BytesWrite;
	TMdxMsgSize		BytesAlloc;
	Void			*Global;
END	TMdxMsg;

typedef	struct	TMdxProtocol
BEGIN
	TBoolean	Supported;
	Void		(*StartupRoutine)(Void);
	Void		(*CleanUpRoutine)(Void);
	TMdxError	(*ServiceRoutine)(Void	*);
	Void		(*EventHandler)(Void	*);
	Void		(*SetPoll)(Void);
	Void		(*CheckPoll)(Void);
END	TMdxProtocol;

BEGIN_ENUM
	MdxL2NetDriverIndex	=	0,
	MdxL2RawDriverIndex	=	2
END_ENUM(TMdxL2DriverIndex);

BEGIN_ENUM
	MdxL3NetDriverIndex	=	0
END_ENUM(TMdxL3DriverIndex);

BEGIN_ENUM
	MdxLinkClosed,
	MdxLinkDisconnected,
	MdxLinkWaitConnect,
	MdxLinkConnected
END_ENUM(TMdxLinkStatus);



typedef	struct	TMdxLinkParams
BEGIN
	TLinkDescr			Ld;
	TMdxLinkType		LinkType;
	TMdxLinkName		LinkName;
	TBaudRate			LinkBaud;
	TOnOff				UseDlcFraming;
	TOnOff				Ebcdic;
	TMdxNetAddr			NetAddr;
	TMdxL2DriverIndex	L2DriverIndex;
	TMdxL3DriverIndex	L3DriverIndex;
	TMdxLinkStatus		LinkStatus;
	TMdxService			Service;
	TMdxConnectMode		ConnectMode;
	TTimer				ConnectTimeout;
	TTimer				ConnectRetriesDelay;
	TBufSize			L1MaxSendSize;
	TCounter			MaxConnectRetries;	/*	L3	*/
	TTimer				IdleTimeout;		/*	L3	*/
	TTimer				ImAliveInterval;	/*	L3	*/
	TTimer				PollInterval;		/*	L2	*/
	TSCounter			MaxPollRetries;		/*	L2	*/
	TMdxProcId			DefaultProcId;
END	TMdxLinkParams;

typedef	struct	TMdxProcessParams
BEGIN
	TMdxProcId		ProcId;
	TMdxPassword	PasswordToSend;
	TMdxPassword	ExpectedPassword;
	TTimer			InactivityTimer;
	TTimer			ConnectRetriesInterval;
END	TMdxProcessParams;

typedef	struct	TMdxApplTimerInfo
BEGIN
	TTimerId		TimerId;
	Int16			Code;
	TTimer			Timer;
	TTimerTag		Tag1;
	TTimerTag		Tag2;
	TTimerTag		Tag3;
	TCounter		Count;
END	TMdxApplTimerInfo;




/**************************************************************************/
/**************************************************************************/
/**************************************************************************/
/**************************************************************************/

/****	The internal structure of following structures are for			***/
/****	Application use.												***/

/**************************************************************************/
/**************************************************************************/
/**************************************************************************/
/**************************************************************************/

typedef	struct	TMdxEvent
BEGIN
	TMdxEventCode	Code;
	TLinkDescr		Ld;
	TMdxError		Error;
	TReqSeq			ReqSeq;
	TMdxProcId		ProcId;
	Void			*Data;
	TBufSize		DataCount;
	TMdxEventHandlerIndex	DataDestructorIndex;
	TBoolean		KeepEvent;
	TMdxObjectId	Id;
	TTimer			EventTimer;
END	TMdxEvent;

/*	Send / Receive Msg Info */
typedef	struct	TMdxSRMsgInfo
BEGIN
	struct
	BEGIN
		TMdxMsg			*Msg;
		TMdxMsgCode		MsgCode;
		TMdxProcId		SentFrom;
		TQueueSeq		SenderMsgId;
		TMdxSendAttr	SendAttr;
		TMdxMsgPri		MsgPri;
	END	Received;
	struct
	BEGIN
		TMdxMsg			*Msg;
		TMdxMsgCode		MsgCode;
	END	Sent;
END	TMdxSRMsgInfo;

/*
	Every message sent to or received from a "WINDOWS DDE SERVER",
	contains the following structure at the start of the message.

	On messages received from the DDE SERVER , usualy immidiatly after
	this structure there is the "ITEM"	sent and its size is indicated
	by "ItemLength". On "POKE" Messages , the Item data is located after
	the "ITEM".

	Messages Sent TO the DDE SERVER should contain the structure only and
	immidiatly after that the data. ALWAYS include "ReqSeq" in these
	messages, and especialy on "ADVISE" messages where it should have the
	same value as received on "ADVISE START".

*/


typedef struct	TMdxDdeSendInfo
BEGIN
	UInt16		TransFmt;
	TBufSize	ItemLength;
	TReqSeq 	ReqSeq;
END TMdxDdeSendInfo;


#if defined(IbmC)	||	defined(MicrosoftC)	||	defined(AttC)
#pragma	pack()
#elif	defined(BorlandC)
#pragma	-a.
#else
CompileError	UnKnown	Complier
#endif


#ifndef	AttC

typedef	Void			(CDeclPtr	TEventHandler)(TMdxEvent	*);
typedef	Void			(CDeclPtr	TEventDataDestructor)(TMdxEvent	*);

Public	TMdxProtocol		StdCall		ProtocolNotSupported(Void);

Public	TMdxTime		StdCall	MdxGetTime(Void);
Public	UInt32			StdCall	MdxTimeTo_time_t(TMdxTime);
Public	Void			*StdCall	MdxAlloc(UInt32);
Public	Void			*StdCall	MdxCalloc(UInt32);
Public	Void			*StdCall	MdxRealloc(Void	*,UInt32);
Public	Void			StdCall	MdxFree(Void	*);
Public	TTimer			StdCall	MdxGetCurrTimerValue(Void);



Public	TTimerId		StdCall	MdxSetApplTimer(Int16,TTimer,TTimerTag,
													TTimerTag,TTimerTag,
													TCounter);
Public	Void			StdCall	MdxClrTimer(TTimerId);
Public	TResult			StdCall	MdxGetApplTimerInfo(Void	*,
														TMdxApplTimerInfo	*);


Public	TMdxMsg			*StdCall	MdxMsgNew(TMdxMsgSize);
Public	TMdxMsg			*StdCall	MdxMsgNewFile(Int8Ptr,UInt8Ptr,
												TSCounter);
Public	TMdxMsg			*StdCall	MdxMsgDup(TMdxMsg	*);
Public	TResult			StdCall	MdxMsgDelete(TMdxMsg	*);
Public	TResult			StdCall	MdxMsgAppend(TMdxMsg	*,Void	*,
												TBufSize);
Public	TResult			StdCall	MdxMsgStore(TMdxMsg	*,TMdxMsgSize,Void	*,
												TBufSize);
Public	TBufSize		StdCall	MdxMsgRead(TMdxMsg	*,Void	*,
												TBufSize);
Public	TMdxMsgSize		StdCall	MdxMsgSeekRead(TMdxMsg	*,TMdxMsgSize,TMdxMsgSeekOrigin);
Public	TResult			StdCall	MdxMsgRewindRead(TMdxMsg	*);
Public	TMdxMsgSize		StdCall	MdxMsgSizeGet(TMdxMsg	*);
Public	TMdxApplError MdxSendMsg(TMdxProcId ProcId,
								 TMdxMsg *Msg,
								 TMdxMsgCode MsgCode,
								 TMdxMsgPri MsgPri,
								 TMdxSendAttr MsgAttr,
								 TReqSeq  RequestSequence,
								 TTimer TimeOut);
Public	TMdxApplError MdxSendData(TMdxProcId ProcId,
								  Void *Data,
								  TBufSize DataSize,
								  TMdxMsgCode MsgCode,
								  TMdxMsgPri MsgPri,
								  TMdxSendAttr MsgAttr,
								  TReqSeq  RequestSequence,
								  TTimer TimeOut);
Public	TMdxApplError MdxReply(TMdxSRMsgInfo   *MsgInfo,
						       TMdxError Error);
Public	TMdxApplError MdxReplyWithData(TMdxSRMsgInfo   *MsgInfo,
									   TMdxError Error,
									   Void *Data,
									   TBufSize DataSize,
									   TMdxMsgCode MsgCode,
									   TMdxMsgPri MsgPri,
									   TMdxSendAttr MsgAttr,
									   TReqSeq  RequestSequence,
									   TTimer TimeOut);
Public	TMdxApplError		MdxReplyWithMsg(TMdxSRMsgInfo	*,
											TMdxError,TMdxMsg	*,
											TMdxMsgCode,TMdxMsgPri,
											TMdxSendAttr,TReqSeq,
											TTimer);
Public	TMdxError		MsDosCdecl	MdxL3SendNonMultiXData(TMdxProcId,UInt8Ptr,TBufSize);



Public	TMdxError		StdCall	MdxConnectProcess(TMdxProcessParams	*);
Public	TMdxError		StdCall	MdxDisconnectProcess(TMdxProcId);
Public	TMdxError		MsDosCdecl	MdxCheckProcessStatus(TMdxProcId);
Public	TMdxApplError	MdxAcceptProcess(TMdxProcId,TMdxPassword	*);
#define	MdxAcceptCall(ProcId)	MdxAcceptProcess(ProcId,NullP)
Public	TMdxApplError	MdxRejectProcess(TMdxProcId,
													TMdxCallError);


TMdxApplError MdxOpenLink(TMdxLinkParams *LinkParams);
Public	TMdxApplError MdxCloseLink(TLinkDescr LinkDescriptor);
Public	TMdxError		MsDosCdecl	MdxCheckLinkStatus(TLinkDescr);
Public	TMdxError		MsDosCdecl	MdxGetLinkParams(TMdxLinkParams	*);


Public	Void			MsDosCdecl	MultiXStart(TMdxProcId,Int8Ptr,
												TMdxStartupAttributes,
												TEventHandler);
Public	Void			MsDosCdecl	MultiXWaitEvent(Void);
Public	Void			MsDosCdecl	MultiXCheckEvent(Void);
Public	Void			MsDosCdecl	MdxDeleteEvent(TMdxEvent	*);
Public	Void			MsDosCdecl	FatalErrorHandler(Int8Ptr);
Public	TMdxProcId		MsDosCdecl	MdxGetMyId(TMdxProcDescr	*);
Public	TMdxError		MsDosCdecl	MdxCreateApplEvent(TMdxEventCode,Void*,TBufSize);
Public	Void			StdCall	TraceBuffer(short,Void	*,TBufSize);
Public	size_t			StdCall	TruncSpace(Void	*,size_t,short);

#else
/*	IF	at&t	compiler , we supply no prototype */

typedef	Void		(CDeclPtr	TEventHandler)();

Public	TMdxTime		StdCall	MdxGetTime();
Public	UInt32			StdCall	MdxTimeTo_time_t();
Public	Void			StdCall	MdxClrTimer();
Public	TResult			StdCall	MdxGetApplTimerInfo();
Public	TTimer			StdCall	MdxGetCurrTimerValue();
Public	TTimerId		StdCall	MdxSetApplTimer();


Public	Void			*StdCall	MdxRealloc();
Public	Void			*StdCall	MdxAlloc();
Public	Void			*StdCall	MdxCalloc();
Public	Void			StdCall	MdxFree();

Public	TMdxMsg			*StdCall	MdxMsgNew();
Public	TMdxMsg			*StdCall	MdxMsgNewFile();
Public	Void			StdCall	MdxMsgDelete();
Public	TResult			StdCall	MdxMsgAppend();
Public	TResult			StdCall	MdxMsgStore();
Public	TBufSize		StdCall	MdxMsgRead();
Public	TMdxMsgSize		StdCall	MdxMsgSizeGet();
Public	Void			StdCall	MdxMsgRewindRead();

Public	TMdxError		StdCall	MdxConnectProcess();
Public	TMdxError		StdCall	MdxDisconnectProcess();
Public	TMdxError		StdCall	MdxAcceptProcess();
#define	MdxAcceptCall(ProcId)	MdxAcceptProcess(ProcId,NullP)
Public	TMdxError		StdCall	MdxRejectProcess();

Public	TMdxError		StdCall	MdxSendData();
Public	TMdxError		StdCall	MdxSendMsg();
Public	TMdxError		StdCall	MdxReply();
Public	TMdxError		StdCall	MdxReplyWithData();
Public	TMdxError		StdCall	MdxReplyWithMsg();

Public	Void			MsDosCdecl	MultiXStart();
Public	Void			MsDosCdecl	MultiXWaitEvent();
Public	Void			MsDosCdecl	MultiXCheckEvent();
Public	Void			MsDosCdecl	MdxDeleteEvent();
Public	Void			MsDosCdecl	DebugTrapHandler();
Public	TMdxError		MsDosCdecl	MdxOpenLink();
Public	TMdxError		MsDosCdecl	MdxCloseLink();
Public	TMdxError		MsDosCdecl	MdxCheckLinkStatus();
Public	TMdxError		MsDosCdecl	MdxCheckProcessStatus();
Public	TMdxError		MsDosCdecl	MdxCreateApplEvent();
Public	Void			StdCall	TraceBuffer();
Public	size_t			StdCall	TruncSpace();
#endif


Public	TBoolean	MdxDebugModeOn;
Public	TBoolean	MdxDebugModeWasOff;

#endif
