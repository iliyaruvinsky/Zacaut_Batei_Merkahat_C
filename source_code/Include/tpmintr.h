#pragma pack(1)

#define TpmRecoveryMsgPriority	0x7fff
#define TpmVersion				100

typedef Int16	TTpmId;
typedef UInt16	TTraceLevel;
typedef Int16	TTpmServerType;
typedef Int32	TTpmProcessState;

// DonR 23Aug2005: Linux compatibility.
#ifdef _LINUX_
	#define BEGIN_ENUM typedef enum {
	#define END_ENUM(x) } x;
#else
	#define BEGIN_ENUM enum {
	#define	END_ENUM(x)	END	; typedef	Int16	x
#endif

BEGIN_ENUM
	TpErrInvalidMsgCode			=	TpErrFirstError,
	TpErrIllegalMsgCode,
	TpErrReceiverNotFound,
	TpErrUnableToForwardTrans,
	TpErrLinkNotFound,
	TpErrLinkExists,
	TpErrTransExists,
	TpErrTransNotFound,
	TpErrGroupNotFound,
	TpErrGroupExists,
	TpErrServerNotFound,
	TpErrServerExists,
	TpErrDialCmdTooLong,
	TpErrServerRspDifferent,
	TpErrInvalidTime
END_ENUM(TTpmError);




BEGIN_ENUM
	TpmReqCodeFirst 		=	MdxTpmFirstMsgCode,
	TpmGetConfigDataReqCode =	TpmReqCodeFirst,
	TpmGetLinksDataReqCode,
	TpmSetProcessStateReqCode,
	TpmLogTransReqMsgCode,
	TpmLogTransRspMsgCode,
	TpmLogTransCommitMsgCode,
	TpmGetPendLogsMsgCode,
	TpmGetCommitedLogsMsgCode,
	TpmGetLogDataMsgCode,
	TpmDelCommitedLogMsgCode,
	TpmSetTimeMsgCode,
	TpmExecCommandMsgCode,

	TpmReqCodeLast			=	TpmReqCodeFirst+20
END_ENUM(TTpmReqCode);

typedef struct	TTpmServerConfigMsg
BEGIN
	TTpmServerType	ServerType;
	TTraceLevel 	TraceLevel;
	TBoolean		StartedByTpm;
END TTpmServerConfigMsg;

typedef struct	TTpmServerLinkData
BEGIN
	TTpmId			LinkId;
	TMdxLinkParams	Params;
END TTpmServerLinkData;


typedef struct	TTpmServerLinksMsg
BEGIN
	TTpmServerLinkData	Data[1];
END TTpmServerLinksMsg;

typedef struct	TTpmProcessStateMsg
BEGIN
	TTpmProcessState	State;
END TTpmProcessStateMsg;

typedef struct	TTpmLogMsgHdr
BEGIN
	TMdxTime		TpmSessionId;
	TMdxTime		LogTime;
	TListKey		TransKey;
	TTimer			Timer;
	TMdxProcId		ProcId;
	TMdxMsgCode 	MsgCode;
	TMdxError		Error;
END TTpmLogMsgHdr;

typedef struct	TTpmLogReqRspData
BEGIN
	TTpmLogMsgHdr	Req;
	TSCounter		RspCount;
	TTpmLogMsgHdr	Rsp[1];
END TTpmLogReqRspData;


typedef struct	TTpmLogsListMsg
BEGIN
	TSCounter			ReqRspCount;
	TTpmLogReqRspData	ReqRsp;
END TTpmLogsListMsg;

typedef struct	TTpmSetTimeMsg
BEGIN
	TMdxTime	CurrTime;
END TTpmSetTimeMsg;



#pragma pack()
