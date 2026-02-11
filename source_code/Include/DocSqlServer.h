#ifndef	__DOC_SQL_SERVER_H__ 
#define __DOC_SQL_SERVER_H__ 
 

	
    #define LOG_FILE_TIME_STAMP_SIZE	80
	#define TEXT_SIZE_CONSREF			7168    // changed by GAL 28/03/04 instead of 5120
	#define CONS_REF_ROW_SIZE			7216    // changed by GAL 28/03/04 instead of 5194
    #define MAX_MSG_SIZE				32000

	#define CONS_REQUEST_LOG	"ConsRequest_log"
	#define CONS_REF_LOG		"Consult_log"
	#define LOG_FOR_174			"174_log"
	#define LOG_FOR_172			"172_log"
	#define LOG_FOR_170			"170_log"
	#define LOG_DIR				"/pharm/log"

	// #define LOG_DIR_RENTGEN		"../log/rentgen"
	#define LOG_DIR_RENTGEN		"/pharm/log/rentgen"
	#define LOG_DIR_CONSREF		"/pharm/log/consref"

	#define DB_ERROR			9999
   	
	enum TransactionType
	{
		enTransOld = 1 ,
		enTransNew // = 2 
	};

	PCHAR LogFileSubDirWithDate( PCHAR szSubDir , PCHAR szLogFileName );
	PCHAR LogFileWithDate( PCHAR );

	// DonR 29Mar2004 New transactions for Clicks 8.5 begin.
	void AuthProc85_Drugs				(TMdxSRMsgInfo *MsgInfo);
	void AuthProc85_GenComp				(TMdxSRMsgInfo *MsgInfo);
	void AuthProc85_DrugGenComp			(TMdxSRMsgInfo *MsgInfo);
	void AuthProc85_GenInter			(TMdxSRMsgInfo *MsgInfo);
	void AuthProc85_GenInterNotes		(TMdxSRMsgInfo *MsgInfo);
	void AuthProc85_GnrlDrugNotes		(TMdxSRMsgInfo *MsgInfo);
	void AuthProc85_DrugNotes			(TMdxSRMsgInfo *MsgInfo);
	void AuthProc85_DrugDocProf			(TMdxSRMsgInfo *MsgInfo);
	void AuthProc85_Economypri			(TMdxSRMsgInfo *MsgInfo);
	void AuthProc85_Pharmacology		(TMdxSRMsgInfo *MsgInfo);
	void AuthProc85_ClicksDiscnt		(TMdxSRMsgInfo *MsgInfo);
	void AuthProc85_UsageInstruct		(TMdxSRMsgInfo *MsgInfo);
	void AuthProc85_DrugForms			(TMdxSRMsgInfo *MsgInfo);
	void AuthProc85_TreatmentGrp		(TMdxSRMsgInfo *MsgInfo);
	void AuthProc85_PrescPeriod			(TMdxSRMsgInfo *MsgInfo);
	void AuthProc85_DrugExtension		(TMdxSRMsgInfo *MsgInfo);
	void AuthProc85_SpecialistPrReq		(TMdxSRMsgInfo *MsgInfo);
	void AuthProc85_SpecialistLtrSeen	(TMdxSRMsgInfo *MsgInfo);
	void AuthProc85_SpecialPrescReq		(TMdxSRMsgInfo *MsgInfo);
	void AuthProc85_InteractionAuth		(TMdxSRMsgInfo *MsgInfo);
	// DonR 29Mar2004 New for Clicks 8.5 end.
#endif
