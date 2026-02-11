
//20030811
#define INFORMIX_MODE

EXEC SQL include	sqlca;
EXEC SQL include	"Global.h";
EXEC SQL include	"GenSql.h";

//EXEC SQL include	"DB.h";
//EXEC SQL include	"DBFields.h";
EXEC SQL include	"multix.h";
EXEC SQL include	"tpmintr.h";
EXEC SQL include	"global_1.h";
EXEC SQL include	"MsgHndlr.h";
#include <MacODBC.h>

extern short UrgentFlag;
static char *GerrSource = __FILE__;


//#include "DocSqlServer.h"     // GAL 16/02/04
//#include "DbConsRef.h"        // GAL 16/02/04

#include "macsql.h"	// DonR 13Jun2004 function prototypes.

void    StrRev1(char * Buf);

EXEC SQL BEGIN DECLARE SECTION;
	extern short NOT_REPORTED_TO_AS400;
	extern short REPORTED_TO_AS400;
	extern short ROW_NOT_DELETED;
	extern short EmbeddedInformixConnected;
EXEC SQL END DECLARE SECTION;



int	DbFetchAuthSelCursor( TAuthRecord *Ptr) 	/* SQLMD_error_test */
{
	EXEC SQL begin declare section;
		TAuthRecord	*Autst =	Ptr;
		int v_rrn;
	EXEC SQL end declare section;

	EXEC SQL fetch	AuthSelCursor	into
		:Autst->MemberId.Code, 		:Autst->MemberId.Number,
		:Autst->CardIssueDate, 		:Autst->SesTime.DbDate,
		:Autst->SesTime.DbTime,
		:Autst->GnrlElegebility, 	:Autst->DentalElegebility,
		:Autst->Contractor, 		:Autst->ContractType,
		:Autst->ProfessionGroup, 	:Autst->AuthQuarter,
		:Autst->AuthTime.DbDate, 	:Autst->AuthTime.DbTime,
		:Autst->AuthError, 			:Autst->ValidUntil,
		:Autst->TermId, 			:Autst->Location,
		:Autst->Locationprv,
		:Autst->AuthType, 			:Autst->Position,  
       	:Autst->PaymentType,		:Autst->Cancled,
		:Autst->CreditTypeSend,		:Autst->CreditType,
		:Autst->Crowid,
		:v_rrn;
	return(1);
}

int	DbCloseAuthSelCursor(void) 	/* SQLMD_error_test */
{
	EXEC SQL close	AuthSelCursor;
	return(1);
}

int DbOpenActionSelCursor(void) 	/* SQLMD_error_test */
{

	EXEC SQL declare	ActionSelCursor	cursor with hold for
	select
		TermId,		InstId, 	Contractor, 	ContractType,
		Idcode, 	Idnumber, 	AuthDate, 	AuthTime,
		Gnrlelegebility,		Observation,
		ActionCode, 	ActionNumber, 	TreatmentContr,	PayingInst,
		TreatmentDate, 	TreatmentTime, 	TreatmentPlace,
		Profession, 	Location, 	Position, 	PaymentType,
		Locationprv,	Rrn ,	Rowid
	from	action	
	where 	
		reported_to_as400 = :NOT_REPORTED_TO_AS400
	for 	Update;	
	
	EXEC SQL Open	ActionSelCursor;
	return(1);
}

int DbFetchActionSelCursor( TActionRecord	*Ptr) 	/* SQLMD_error_test */
{
	EXEC SQL begin declare section;
		TActionRecord	*Action	=	Ptr;
	EXEC SQL end declare section;

	EXEC SQL Fetch	ActionSelCursor	into
   		:Action->TermId,	 	:Action->InstId,
   		:Action->Contractor, 		:Action->ContractType,
		:Action->MemberId.Code, 	:Action->MemberId.Number,
		:Action->AuthTime.DbDate, 	:Action->AuthTime.DbTime,
		:Action->GnrlElegebility, 	:Action->Observation,
		:Action->ActionCode, 		:Action->ActionNumber,
		:Action->TreatmentContr,	:Action->PayingInst,
		:Action->TreatmentDate, 	:Action->TreatmentTime,
		:Action->TreatmentPlace, 	:Action->Profession,
		:Action->Location, 		:Action->Position,
		:Action->PaymentType, 		:Action->Locationprv,
		:Action->Rrn,				:Action->Crowid;
	return(1);
}

int DbCloseActionSelCursor(void) 	/* SQLMD_error_test */
{
	EXEC SQL close	ActionSelCursor;
	return(1);
}


int DbFetchTaxSendSelCursor( TTaxSendRecord	*Ptr) 	/* SQLMD_error_test */
{
	EXEC SQL begin declare section;
		TTaxSendRecord	*TaxSend	=	Ptr;
	EXEC SQL end declare section;

	EXEC SQL Fetch	TaxSendSelCursor	into
   		:TaxSend->Contractor, 		:TaxSend->ContractType,
		:TaxSend->MemberId.Code, 	:TaxSend->MemberId.Number,
		:TaxSend->AuthTime.DbDate, 	:TaxSend->AuthTime.DbTime,
   		:TaxSend->TermId,
		:TaxSend->TaxAmount,		:TaxSend->TaxCalc,
		:TaxSend->Rrn,				:TaxSend->Crowid;
	return(1);
}

int DbCloseTaxSendSelCursor(void) 	/* SQLMD_error_test */
{
	EXEC SQL close	TaxSendSelCursor;
	return(1);
}

int DbOpenVisNewSelCursor(void) 	/* SQLMD_error_test */
{

	EXEC SQL declare	VisNewSelCursor	cursor with hold 	for
	select
    		Contractor,		ContractType,	       TermId,
		IdCode,		        Idnumber, 	
		CardissueDate, 		AuthDate, 	       AuthTime,
		GnrlElegebility,	Observation0, 	       Observation1, 
		Observation2, 		Observation3, 	       TreatmentContr, 
		MedicineCode0, 		Prescription0, 	       Quantity0,
		Douse0, 		Days0, 		       NumberOfPacages0,
		PrescriptionType0, 	PrescriptionDate0, 
		MedicineCode1, 		PrescripTion1, 	       Quantity1,
		Douse1, 		Days1, 		       NumberOfPacages1,
		PrescriptionType1, 	PrescriptionDate1,
		MedicineCode2, 		Prescription2, 	       Quantity2,
		Douse2, 		Days2, 		       NumberOfPacages2,
		PrescriptionType2, 	PrescriptionDate2,
		MedicineCode3, 		Prescription3, 	       Quantity3,
		Douse3, 		Days3, 		       NumberOfPacages3,
		PrescriptionType3, 	PrescriptionDate3,
		MedicineCode4, 		Prescription4, 	       Quantity4,
		Douse4, 		Days4, 		       NumberOfPacages4,
		PrescriptionType4, 	PrescriptionDate4,
		MedicineCode5, 		Prescription5, 	       Quantity5,
		Douse5, 		Days5, 	       	       NumberOfPacages5,
		PrescriptionType5, 	PrescriptionDate5,
		Reference0, 		Reference1, 	       Reference2,
		Confirmation0, 		Confirmation1, 	       Confirmation2,
		PayingInst, 		Profession, 	       Location,
		Position, 		PaymentType, 	       Locationprv,
		Institute,		Rrn,			Rowid 
	from  	visitnew 
	where 	
		reported_to_as400 = :NOT_REPORTED_TO_AS400
	for	Update;	
	
	EXEC SQL Open	VisNewSelCursor;
	return(1);
}

int DbFetchVisNewSelCursor( TVisitNewRecord	*Ptr) 	/* SQLMD_error_test */
{
	EXEC SQL begin declare section;
	TVisitNewRecord	*VisNew	=	Ptr;
	TCObservationCode Obs1,Obs2,Obs3,Obs0;
	TCCharNew PT0,PT1,PT2,PT3,PT4,PT5;
	TCCharNew7 Ref0,Ref1,Ref2,Conf0,Conf1,Conf2;
	EXEC SQL end declare section;

	EXEC SQL Fetch	VisNewSelCursor	into
    		:VisNew->Contractor, 		:VisNew->ContractType,
    		:VisNew->TermId, 		:VisNew->MemberId.Code,
		:VisNew->MemberId.Number, 	:VisNew->CardIssueDate,
		:VisNew->AuthTime.DbDate, 	:VisNew->AuthTime.DbTime,
		:VisNew->GnrlElegebility,
	    :Obs0,	:Obs1, :Obs2, :Obs3,	
	    	:VisNew->TreatmentContr, 	
		:VisNew->MedicineCode[0], 	:VisNew->Prescription[0], 
		:VisNew->Quantity[0], 		:VisNew->Douse[0],	
		:VisNew->Days[0], 		:VisNew->NumberOfPacages[0],
		:PT0, 	:VisNew->PrescriptionDate[0], 
		:VisNew->MedicineCode[1], 	:VisNew->Prescription[1], 
		:VisNew->Quantity[1], 		:VisNew->Douse[1], 	
		:VisNew->Days[1], 		:VisNew->NumberOfPacages[1],
		:PT1, 	:VisNew->PrescriptionDate[1], 
		:VisNew->MedicineCode[2], 	:VisNew->Prescription[2], 
		:VisNew->Quantity[2], 		:VisNew->Douse[2], 	
		:VisNew->Days[2], 		:VisNew->NumberOfPacages[2],
		:PT2, 	:VisNew->PrescriptionDate[2], 
		:VisNew->MedicineCode[3], 	:VisNew->Prescription[3], 
		:VisNew->Quantity[3], 		:VisNew->Douse[3], 	
		:VisNew->Days[3], 		:VisNew->NumberOfPacages[3],
		:PT3, 	:VisNew->PrescriptionDate[3], 
		:VisNew->MedicineCode[4], 	:VisNew->Prescription[4], 
		:VisNew->Quantity[4], 		:VisNew->Douse[4], 		
		:VisNew->Days[4], 		:VisNew->NumberOfPacages[4],
		:PT4, 	:VisNew->PrescriptionDate[4], 
		:VisNew->MedicineCode[5], 	:VisNew->Prescription[5], 
		:VisNew->Quantity[5], 		:VisNew->Douse[5], 	
		:VisNew->Days[5], 		:VisNew->NumberOfPacages[5],
		:PT5, 	:VisNew->PrescriptionDate[5], 
	    	:Ref0, 	:Ref1, :Ref2,
	    	:Conf0, :Conf1, :Conf2, 	
		:VisNew->PayingInst, 		:VisNew->Profession, 
		:VisNew->Location, 		:VisNew->Position,	
		:VisNew->PaymentType, 		:VisNew->Locationprv,
		:VisNew->Institute, 		:VisNew->Rrn,
		:VisNew->Crowid;

	    	strcpy(VisNew->Observation[0],	Obs0);
	    	strcpy(VisNew->Observation[1],	Obs1);
	    	strcpy(VisNew->Observation[2],	Obs2);
	    	strcpy(VisNew->Observation[3],	Obs3);
		strcpy(VisNew->PrescriptionType[0], PT0); 
		strcpy(VisNew->PrescriptionType[1], PT1); 
		strcpy(VisNew->PrescriptionType[2], PT2); 
		strcpy(VisNew->PrescriptionType[3], PT3); 
		strcpy(VisNew->PrescriptionType[4], PT4); 
		strcpy(VisNew->PrescriptionType[5], PT5); 
		strcpy(VisNew->Reference[0],Ref0);
		strcpy(VisNew->Reference[1],Ref1);
		strcpy(VisNew->Reference[2],Ref2);
	    	strcpy(VisNew->Confirmation[0],Conf0); 	
	    	strcpy(VisNew->Confirmation[1],Conf1); 	
	    	strcpy(VisNew->Confirmation[2],Conf2); 	
	return(1);
}

int DbCloseVisNewSelCursor(void) 	/* SQLMD_error_test */
{
	EXEC SQL close	VisNewSelCursor;
	return(1);
}

int DbOpenVisitSelCursor(void) 	/* SQLMD_error_test */
{
	EXEC SQL BEGIN DECLARE SECTION;
	long int	Rrn_max;
	EXEC SQL END DECLARE SECTION;
	EXEC SQL select 
		rrn 
	into 
		:Rrn_max from upvisit;

	EXEC SQL declare	VisitSelCursor cursor with hold for
	select
		Contractor,	ContractType,	IdCode,		IdNumber,
		CardIssueDate,	AuthDate,	AuthTime, 	GnrlElegebility,
		Observation,
		Action0,	Action1,	Action2,	Action3,
		PayingInst,	TermId,		Profession,	Location,
		VisitCounter,	Position,	PaymentType,	Locationprv,
		Rrn ,Rowid
	from	visit	
	where 	
		Rrn 			<= 	:Rrn_max 		and 	
		reported_to_as400 	= 	:NOT_REPORTED_TO_AS400
	for 	Update;
	
	EXEC SQL open	VisitSelCursor;
	return(1);
}

int DbFetchVisitSelCursor( TVisitRecord	*Ptr) 	/* SQLMD_error_test */
{
	EXEC SQL begin declare section;
		TVisitRecord	*Vistst	=	Ptr;
	EXEC SQL end declare section;

	EXEC SQL fetch	VisitSelCursor
	into
		:Vistst->Contractor,		:Vistst->ContractType,
		:Vistst->MemberId.Code,		:Vistst->MemberId.Number,
		:Vistst->CardIssueDate,		:Vistst->AuthTime.DbDate,
		:Vistst->AuthTime.DbTime,	:Vistst->GnrlElegebility,
		:Vistst->Observation,		:Vistst->Action[0],
		:Vistst->Action[1],		:Vistst->Action[2],
		:Vistst->Action[3],		:Vistst->PayingInst,
		:Vistst->TermId,		:Vistst->Profession,
		:Vistst->Location,		:Vistst->VisitCounter,
       		:Vistst->Position,	    	:Vistst->PaymentType,   
		:Vistst->Locationprv,		:Vistst->Rrn,	:Vistst->Crowid;
	return(1);
}

int DbCloseVisitSelCursor(void) 	/* SQLMD_error_test */
{
	EXEC SQL Close	VisitSelCursor;
	return(1);
}

int DbOpenShiftSelCursor(void) 	/* SQLMD_error_test */
{
	EXEC SQL declare	ShiftsSelCursor	cursor with hold for
	select
		TermId, 	Contractor, 	ContractType,
		ShiftStatus, 	StatusDate, 	StatusTime,
		Error, 		Location,	Locationprv,
		Position,	PaymentType,	Rowid
	from	shift
	where 	
		reported_to_as400 = :NOT_REPORTED_TO_AS400
	for 	Update;

	EXEC SQL Open	ShiftsSelCursor;
	return(1);
}

int DbFetchShiftSelCursor( TShiftRecord	*Ptr) 	/* SQLMD_error_test */
{
	EXEC SQL begin declare section;
		TShiftRecord	*Shift	=	Ptr;
	EXEC SQL end declare section;

	EXEC SQL Fetch	ShiftsSelCursor	into
		:Shift->TermId,			:Shift->Contractor,
		:Shift->ContractType, 		:Shift->ShiftStatus,
		:Shift->StatusTime.DbDate, 	:Shift->StatusTime.DbTime,
		:Shift->Error, 			:Shift->Location,
		:Shift->Locationprv, 		:Shift->Position,
		:Shift->PaymentType,		:Shift->Crowid;
	return(1);
}

int DbCloseShiftSelCursor(void) 	/* SQLMD_error_test */
{
	EXEC SQL close	ShiftsSelCursor;
	return(1);
}

int DbOpenPresenceSelCursor(void) 	/* SQLMD_error_test */
{
	EXEC SQL declare	PresenceSelCursor	cursor with hold for
	select
		Employee, 		Action, 	ActionDate,	 
		ActionTime, 	TermId ,	Rowid
	from	presence  
	where 	
		reported_to_as400 = :NOT_REPORTED_TO_AS400
	for 	Update;
	
	EXEC SQL Open	PresenceSelCursor;
	return(1);
}

int DbFetchPresenceSelCursor( TPresenceRecord	*Ptr) 	/* SQLMD_error_test */
{
	EXEC SQL begin declare section;
		TPresenceRecord *Presence	=	Ptr;
	EXEC SQL end declare section;

	EXEC SQL Fetch	PresenceSelCursor	into
		:Presence->Employee, 		:Presence->Action,
		:Presence->ActionTime.DbDate, :Presence->ActionTime.DbTime,
		:Presence->TermId,			:Presence->Crowid;
	return(1);
}

int DbClosePresenceSelCursor(void) 	/* SQLMD_error_test */
{
	EXEC SQL close	PresenceSelCursor;
	return(1);
}

/*20020529*/
int	DbOpenMedParamSelCursor(void) 	/* SQLMD_error_test */
{
	EXEC SQL declare	MedParamSelCursor	cursor with hold for

	select
		contractor,		contracttype,		idcode,		idnumber,
		termid,			paramnumber,
		measuredate,	writedate,			writetime,
		paramvaluechar1,paramvaluechar2,	paramvaluechar3,
		profession,		location  ,			locationprv,
		position   ,	paymenttype,
		Rowid
	from	MedParam	
	where   
		reported_to_as400 = :NOT_REPORTED_TO_AS400 
	for 	Update;	
	EXEC SQL open	MedParamSelCursor;
	return(1);
}

int	DbFetchMedParamSelCursor( TMedParamRecord *Ptr) 	/* SQLMD_error_test */
{
	EXEC SQL begin declare section;
		TMedParamRecord	*MedParam =	Ptr;
	EXEC SQL end declare section;

	EXEC SQL fetch	MedParamSelCursor	into
		:MedParam->Contractor,
		:MedParam->ContractType,
	    :MedParam->MemberId.Code,
		:MedParam->MemberId.Number,
		:MedParam->TermId,
		:MedParam->ParamNumber,
		:MedParam->MeasureDate,
		:MedParam->WriteDT.DbDate,
		:MedParam->WriteDT.DbTime,
		:MedParam->ParamValueChar1,
		:MedParam->ParamValueChar2,
		:MedParam->ParamValueChar3,
		:MedParam->Profession,
		:MedParam->Location,
		:MedParam->Locationprv,
		:MedParam->Position,
		:MedParam->PaymentType,
		:MedParam->Crowid;

	return(1);
}

int	DbCloseMedParamSelCursor(void) 	/* SQLMD_error_test */
{
	EXEC SQL close	MedParamSelCursor;
	return(1);
}

/*20020529*/

int DbUpdateVisitSelCursor(TVisitRecord	*Ptr) 	/* SQLMD_error_test */
{
	EXEC SQL begin declare section;
		int	rowid_p =	Ptr->Crowid;	
	EXEC SQL end declare section;
/* oracle
	EXEC SQL Update visit
	set 		reported_to_as400 = :REPORTED_TO_AS400	
	where 		current of VisitSelCursor;
*/
	EXEC SQL Update visit
	set 		reported_to_as400 = :REPORTED_TO_AS400	
	where 		rowid = :rowid_p;


	return(1);
}

int DbUpdateActionSelCursor(TActionRecord *Ptr) 	/* SQLMD_error_test */
{
	EXEC SQL begin declare section;
		int	rowid_p =	Ptr->Crowid;
	EXEC SQL end declare section;

	EXEC SQL Update action
	set 		reported_to_as400 = :REPORTED_TO_AS400	
	where 		rowid = :rowid_p;
	return(1);
}

int DbUpdateTaxSendSelCursor(TTaxSendRecord	*Ptr) 	/* SQLMD_error_test */
{
	EXEC SQL begin declare section;
		int	rowid_p =	Ptr->Crowid;
	EXEC SQL end declare section;

	EXEC SQL Update TaxSend
	set 		reported_to_as400 = :REPORTED_TO_AS400	
	where 		rowid = :rowid_p;
	return(1);
}

int DbUpdateShiftSelCursor(TShiftRecord *Ptr) 	/* SQLMD_error_test */
{
	EXEC SQL begin declare section;
		int	rowid_p =	Ptr->Crowid;
	EXEC SQL end declare section;

	EXEC SQL Update shift
	set reported_to_as400 = :REPORTED_TO_AS400	
	where 		rowid = :rowid_p;
	return(1);
}

int DbUpdateVisNewSelCursor(TVisitNewRecord *Ptr) 	/* SQLMD_error_test */
{
	EXEC SQL begin declare section;
		int	rowid_p =	Ptr->Crowid;
	EXEC SQL end declare section;

	EXEC SQL Update visitNew
	set 		reported_to_as400 = :REPORTED_TO_AS400	
	where 		rowid = :rowid_p;
	return(1);
}

int DbUpdatePresenceSelCursor( TPresenceRecord	*Ptr) 	/* SQLMD_error_test */
{
	EXEC SQL begin declare section;
		int	rowid_p =	Ptr->Crowid;
	EXEC SQL end declare section;

	EXEC SQL Update Presence
	set 		reported_to_as400 = :REPORTED_TO_AS400	
	where 		rowid = :rowid_p;
	return(1);
}

int DbUpdateAuthSelCursor( TAuthRecord *Ptr) 	/* SQLMD_error_test */
{
	EXEC SQL begin declare section;
		int	rowid_p =	Ptr->Crowid;
	EXEC SQL end declare section;

	EXEC SQL Update Auth
	set 		reported_to_as400 = :REPORTED_TO_AS400	
	where 		rowid = :rowid_p;
	return(1);
}
int DbUpdateMedParamSelCursor(TMedParamRecord	*Ptr) 	/* SQLMD_error_test */
{
	EXEC SQL begin declare section;
		int	rowid_p =	Ptr->Crowid;	
	EXEC SQL end declare section;
	EXEC SQL Update MedParam
		set 		reported_to_as400 = :REPORTED_TO_AS400	
	where 		rowid = :rowid_p;

	return(1);
}

int	DbInsertLabNoteRecord( TLabNoteRecord	*Ptr)
{
	EXEC SQL BEGIN DECLARE SECTION;
	TLabNoteRecord *Note	=	Ptr;
	int v_count;
	EXEC SQL END DECLARE SECTION;

	EXEC SQL select count(*) into :v_count from labnotes
	where 
		contractor	=	:Note->Contractor	and
		idnumber	=	:Note->MemberId.Number and
		idcode		=	:Note->MemberId.Code and
		reqid		=	:Note->ReqId  		and 
		notetype	=	:Note->NoteType  	and
		reqcode		=	:Note->ReqCode		and
		lineindex	=	:Note->LineIndex 	and
		note		=	:Note->Note  ; 

	if ( v_count == 0 )
	{
		EXEC SQL Insert Into	LabNotes
		(
			Contractor, 	IdCode, 	IdNumber,
			ReqId, 		ReqCode, 	NoteType,
			LineIndex, 	Note
		)	values
		(
			:Note->Contractor, 	:Note->MemberId.Code,
			:Note->MemberId.Number, :Note->ReqId,
			:Note->ReqCode, 	:Note->NoteType,
			:Note->LineIndex, 	:Note->Note
		);
	}
	else
	{
		GerrLogFnameMini("LabDouble_log",GerrId,"notes [%d][%d][%d][%d][%d]",
			Note->Contractor,
			Note->MemberId.Number,
			Note->ReqId,
			Note->ReqCode,
			Note->LineIndex);		
	}

	return(1);
}

int	DbInsertLabResultRecord( TLabRecord	*Ptr)
{
EXEC SQL BEGIN DECLARE SECTION;
	TLabRecord *Lab	=	Ptr;
	int v_count;
EXEC SQL END DECLARE SECTION;


	// DonR 10Sep2020: If we don't have an embedded Informix connection, don't
	// bother with any SQL - just return before generating error messages.
	if (!EmbeddedInformixConnected)
	{
		return (1);
	}


	v_count= 0;


	EXEC SQL select count(*) into :v_count from labresults
				where 
				contractor =    :Lab->Contractor and  /* Yulia 1/04/04 */
				idnumber	=	:Lab->MemberId.Number 	and
				idcode		=	:Lab->MemberId.Code    	and
				reqcode		=	:Lab->ReqCode		and
				labid		=	:Lab->LabId		and
				route		=	:Lab->Route		and 
				result		=	:Lab->Result		and
				lowerlimit	=	:Lab->LowerLimit	and
				upperlimit	=	:Lab->UpperLimit	and
	/*			units		=	:Lab->Units		and*/
				reqdate		=	:Lab->ReqDate		and
				pregnantwoman=	:Lab->PregnantWoman and
				completed	=	:Lab->Completed	and
				reqid		=	:Lab->ReqId; 

	if ( v_count == 0 )
	{
		EXEC SQL Insert Into	LabResults
		(
			Contractor, 	IdCode, 	IdNumber,
			LabId, 		ReqId, 		ReqCode,
			Route, 		Result, 	LowerLimit,
			UpperLimit, 	Units, 		ApprovalDate,
			ForwardDate, 	ReqDate,	PregnantWoman,
			Completed, 	TermId	
		)	values
		(
			:Lab->Contractor, 	:Lab->MemberId.Code, 
			:Lab->MemberId.Number, 	:Lab->LabId, 	
			:Lab->ReqId, 		:Lab->ReqCode,
			:Lab->Route, 		:Lab->Result, 	
			:Lab->LowerLimit, 	:Lab->UpperLimit, 	
			:Lab->Units,		:Lab->ApprovalDate,
			:Lab->ForwardDate,	:Lab->ReqDate, 
			:Lab->PregnantWoman, 	:Lab->Completed,	
			:Lab->TermId	
		);
	}
	else
	{
		GerrLogFnameMini("LabDoubleR_log",GerrId,"labs  [%d][%d][%d][%d]",
			Lab->Contractor,
			Lab->MemberId.Number,
			Lab->ReqId,
			Lab->ReqCode);		
	}
	if(	SQLCODE ==	-391	)
	{
	printf("insert into labresults -391 error\n");
	}
	return(1);
}
/*====================================================================*/
/* New table 37,38                                        11.1999     */
/*====================================================================*/
int	DbInsertLabResultOtherRecord( TLabRecord	*Ptr,int group_num_id)
{
	EXEC SQL BEGIN DECLARE SECTION;
	TLabRecord *Lab	=	Ptr;
	int	gr_main  = group_num_id;
	int	reqid_main;
	int	v_count		= 0;
	int	NowTime;
	int	NowDate;
	EXEC SQL END DECLARE SECTION;

	NowDate = GetDate();
	NowTime = GetTime();

	reqid_main = Lab->ReqId%100000000;

	ExecSQL (	MAIN_DB, DbInsertLabResultOtherRecord_get_labresother_count,
				&v_count,
				&Lab->MemberId.Number,		&Lab->MemberId.Code,		&Lab->ReqCode,
				&Lab->LabId,				&Lab->Route,				&Lab->Result,
				&Lab->LowerLimit,			&Lab->UpperLimit,			&Lab->ReqDate,
				&Lab->PregnantWoman,		&Lab->Completed,			&Lab->ReqId,
				END_OF_ARG_LIST																);

	if (SQLCODE)
		SQLERR_error_test ();

//	EXEC SQL select count(*) into :v_count from labresother
//				where 
//				idnumber	=	:Lab->MemberId.Number 	and
//				idcode		=	:Lab->MemberId.Code    	and
//				reqcode		=	:Lab->ReqCode		and
//				labid		=	:Lab->LabId		and
//				route		=	:Lab->Route		and 
//				result		=	:Lab->Result		and
//				lowerlimit	=	:Lab->LowerLimit	and
//				upperlimit	=	:Lab->UpperLimit	and
//			/*	units		=	:Lab->Units		and */
//				reqdate		=	:Lab->ReqDate		and
//				pregnantwoman=	:Lab->PregnantWoman and
//				completed	=	:Lab->Completed	and
//				reqid		=	:Lab->ReqId;
//			/*	reqid_main	=	:reqid_main 
//				20050925 because problem of delete changed notes records*/; 

	if ( v_count == 0 )
	{
		ExecSQL (	MAIN_DB, DbInsertLabResultOtherRecord_INS_labresother,
					&Lab->Contractor,		&Lab->MemberId.Code, 
					&Lab->MemberId.Number,	&Lab->LabId, 	
					&Lab->ReqId,			&Lab->ReqCode,
					&Lab->Route,			&Lab->Result, 	
					&Lab->LowerLimit,		&Lab->UpperLimit, 	
					&Lab->Units,			&Lab->ApprovalDate,
					&Lab->ForwardDate,		&Lab->ReqDate, 
					&Lab->PregnantWoman,	&Lab->Completed,	
					&Lab->TermId,			&reqid_main,
					&gr_main,				&NowDate,
					&NowTime,				END_OF_ARG_LIST					);

	if (SQLCODE)
		SQLERR_error_test ();

//		EXEC SQL Insert Into	LabResOther
//		(
//			Contractor, 	IdCode, 	IdNumber,
//			LabId, 		ReqId, 		ReqCode,
//			Route, 		Result, 	LowerLimit,
//			UpperLimit, 	Units, 		ApprovalDate,
//			ForwardDate, 	ReqDate,	PregnantWoman,
//			Completed, 	TermId	,	ReqId_Main,
//			Group,
//			insertdate,inserttime
//		)	values
//		(
//			:Lab->Contractor, 	:Lab->MemberId.Code, 
//			:Lab->MemberId.Number, 	:Lab->LabId, 	
//			:Lab->ReqId, 		:Lab->ReqCode,
//			:Lab->Route, 		:Lab->Result, 	
//			:Lab->LowerLimit, 	:Lab->UpperLimit, 	
//			:Lab->Units,		:Lab->ApprovalDate,
//			:Lab->ForwardDate,	:Lab->ReqDate, 
//			:Lab->PregnantWoman,:Lab->Completed,	
//			:Lab->TermId	,   :reqid_main,	:gr_main,
//			:NowDate ,			:NowTime 
//		);
	}
	else
	{
	GerrLogFnameMini("LabDoubleR_log",GerrId,"labso [%d][%d][%d][%d]",
			Lab->Contractor,
			Lab->MemberId.Number,
			reqid_main,
			Lab->ReqCode);		
	
	}
	if(	SQLCODE ==	-391	)
	{
		printf("insert into labresother -391 error\n");
	}

	return(1);
}

int	DbInsertLabNoteOtherRecord( TLabNoteRecord	*Ptr)
{
	EXEC SQL BEGIN DECLARE SECTION;
		TLabNoteRecord *Note	=	Ptr;
		int reqid_main;
		int v_count;
		TTimeHHMMSS		NowTime;
		TDateYYYYMMDD	NowDate;
	EXEC SQL END DECLARE SECTION;

	NowDate = GetDate();
	NowTime = GetTime();

	reqid_main = Note->ReqId%100000000;

	EXEC SQL select count(*) into :v_count from labnotother
	where 
		contractor	=	:Note->Contractor	and
		idnumber	=	:Note->MemberId.Number 	and
		idcode		=	:Note->MemberId.Code    	and
	/*	reqid		=	:v_nreqid  		and  */
		notetype	=	:Note->NoteType  	and
		reqcode		=	:Note->ReqCode		and
		lineindex	=	:Note->LineIndex 	and
		note		=	:Note->Note      	and
		reqid_main	=	:reqid_main; 

	if ( v_count == 0 )
	{
		EXEC SQL Insert Into	LabNotOther
		(
		Contractor, 	IdCode, 	IdNumber,
		ReqId, 		ReqCode, 	NoteType,
		LineIndex, 	Note,		ReqId_Main,
		insertdate,inserttime

		)	values
		(
		:Note->Contractor, 	:Note->MemberId.Code,
		:Note->MemberId.Number, :Note->ReqId,
		:Note->ReqCode, 	:Note->NoteType,
		:Note->LineIndex, 	:Note->Note,   :reqid_main,
		:NowDate ,			:NowTime 
		);
	}
	else
	{
		GerrLogFnameMini("LabDouble_log",GerrId,"noteo [%d][%d][%d][%d][%d]",
			Note->Contractor,
			Note->MemberId.Number,
			reqid_main,
			Note->ReqCode,
			Note->LineIndex);		
	}
	return(1);
}

/*====================================================================*/
/* end  New table 37,38                                        11.1999*/
/*====================================================================*/

int	DbGetTempMembRecord( 	TIdNumber		CardId,
				TTempMembRecord		*Ptr)
{
	EXEC SQL BEGIN DECLARE SECTION;
		TTempMembRecord *Temp	=	Ptr;
	EXEC SQL END DECLARE SECTION;

	Temp->CardId	=	CardId;
	EXEC SQL select
		IdCode, 	IdNumber, 		ValidUntil,
		LastUpdateDate, LastUpdateTime,	 	UpdatedBy
	into
		:Temp->MemberId.Code, 		:Temp->MemberId.Number,
		:Temp->ValidUntil,
		:Temp->LastUpdate.DbDate, 	:Temp->LastUpdate.DbTime,
		:Temp->UpdatedBy
	from tempmemb
	where	
		CardId	=	:Temp->CardId;


	JUMP_ON_ERR;

	return(1);
}


int	DbInsertTempMembRecord( TTempMembRecord	*Ptr)
{
	EXEC SQL BEGIN DECLARE SECTION;
		TTempMembRecord *Temp	=	Ptr;
	EXEC SQL END DECLARE SECTION;
	
	EXEC SQL insert into	tempmemb
	(
		CardId, 	IdCode,		IdNumber, 	ValidUntil,
		LastUpdateDate, LastUpdateTime, UpdatedBy
	)	
	values
	(
		:Temp->CardId, 			:Temp->MemberId.Code,
		:Temp->MemberId.Number, 	:Temp->ValidUntil,
		:Temp->LastUpdate.DbDate, 	:Temp->LastUpdate.DbTime,
		:Temp->UpdatedBy
	);

	return(1);
}


int	DbUpdateTempMembRecord( TIdNumber		CardId,
				TTempMembRecord		*Ptr)
{
	EXEC SQL BEGIN DECLARE SECTION;
		TTempMembRecord *Temp	=	Ptr;
	EXEC SQL END DECLARE SECTION;
//	Ptr->LastUpdate 	=	CurrDateTime;
/*printf("\n validuntil [%d][%d][%d]",
	   Temp->MemberId.Number,
	   Temp->CardId,
	   Temp->ValidUntil);fflush(stdout);
*/

	Temp->CardId		=	CardId;
	EXEC SQL update tempmemb	set
		IdCode		=	:Temp->MemberId.Code,
		IdNumber	=	:Temp->MemberId.Number,
		ValidUntil	=	:Temp->ValidUntil,
		LastUpdateDate	=	:Temp->LastUpdate.DbDate,
		LastUpdateTime	=	:Temp->LastUpdate.DbTime,
		UpdatedBy	=	:Temp->UpdatedBy
	where	
		CardId		=	:Temp->CardId;

	return(1);
}


int	DbDeleteTempMembRecord( TIdNumber	CardId)
{
	EXEC SQL BEGIN DECLARE SECTION;
		TTempMembRecord Temp;
	EXEC SQL END DECLARE SECTION;

	Temp.CardId	=	CardId;
	EXEC SQL delete from	tempmemb
	where	
		CardId	=	:Temp.CardId;

	return(1);
}

int DbCancleAuthVisit( TIdNumber	Contractor,
			TMemberId	*MemberId,
			TSCounter	Quarter,
			TAuthCancelCode CancelCode)
{
	
	EXEC SQL BEGIN DECLARE SECTION;
		TAuthCancRecord	Auth;
		int ContractorVN;
	EXEC SQL END DECLARE SECTION;

	Auth.MemberId.Code	=	MemberId->Code;
	Auth.MemberId.Number	=	MemberId->Number;
	Auth.Contractor		=	Contractor;
	Auth.AuthQuarter	=	Quarter;
	Auth.Cancled		=	CancelCode;

	EXEC SQL update	auth set 
		Cancled 	=	:Auth.Cancled
	where
		Contractor	=	:Auth.Contractor	and
		IdCode		=	:Auth.MemberId.Code	and
		IdNumber	=	:Auth.MemberId.Number	and
		AuthQuarter 	=	:Auth.AuthQuarter	and
		Cancled 	=	0;

//	printf ("\n 3005 [%d][%d][%d][%d]\n",sqlca.sqlcode,Auth.Contractor,Auth.MemberId.Number,sqlca.sqlerrd[2]);fflush(stdout);
	if( SQLERR_code_cmp( SQLERR_no_rows_affected ) == MAC_TRUE )       
	{
	  ContractorVN = 0;
	  EXEC SQL select max(contractor) into :ContractorVN from visitnew
		  where treatmentcontr = :Auth.Contractor	and
			IdCode			=	:Auth.MemberId.Code		and
			IdNumber		=	:Auth.MemberId.Number	;
//	printf ("\n 3005 VN [%d][%d][%d][%d][%d]\n",sqlca.sqlcode,ContractorVN,Auth.Contractor,Auth.MemberId.Number,sqlca.sqlerrd[2]);fflush(stdout);

	  if ( SQLERR_error_test() != MAC_OK )
		{
			return(0);
		}
		else
		{
			if (ContractorVN)
			{
				EXEC SQL update	auth set 	Cancled 	=	:Auth.Cancled
				where
				Contractor		=	:ContractorVN			and
				IdCode			=	:Auth.MemberId.Code		and
				IdNumber		=	:Auth.MemberId.Number	and
				AuthQuarter 	=	:Auth.AuthQuarter		and
				Cancled 		=	0;
//	printf ("\n 3005 UP2 [%d][%d][%d][%d][%d]\n",sqlca.sqlcode,ContractorVN,Auth.Contractor,Auth.MemberId.Number,sqlca.sqlerrd[2]);fflush(stdout);

				if ( SQLERR_error_test()  != MAC_OK )
				{
					return(0);
				}
				else
				{
					return(1);
				}

			}
			else  //have no treatment contractor
			{
				return(0);
			}
		}
	}
	if ( SQLERR_error_test()  != MAC_OK )
	{
		return(0);
	}
	else
	{
		return (1);
	}

	// The lines below are unreachable - commented out to avoid compiler warning.
	// JUMP_ON_ERR;
	// return(1);
}


int	DbCheckContractorLabResults( 	TIdNumber   ContractorParam,
				 	TMemberId   *MemberIdParam)
{
EXEC SQL BEGIN DECLARE SECTION;
	TIdNumber	Contractor	=   ContractorParam;
	TIdNumber      v_Contractor;
	TMemberId	MemberIdFrom;
	TMemberId	MemberIdTo;
	short 		Result;
EXEC SQL END DECLARE SECTION;

	if (MemberIdParam)
	{
		MemberIdFrom	=	MemberIdTo 	=	*MemberIdParam;
    	}
	else
	{
		MemberIdFrom.Code 	= 0;	
		MemberIdFrom.Number 	= 0;	
		MemberIdTo.Code 	= 9;	
		MemberIdTo.Number 	= 999999999;	
    }
	/*$select 
		count(*) 
	into 
		:Result
	from	LabResults
	where	
		Contractor	=	:Contractor 		and
		IdCode		between :MemberIdFrom.Code	and 
					:MemberIdTo.Code 	and
		IdNumber	between :MemberIdFrom.Number 	and 
					:MemberIdTo.Number;
	JUMP_ON_ERR;*/

	EXEC SQL Declare	CheckLabResCursor Cursor	for
	select	Contractor
	from	LabResults
	where	
		Contractor	=	:Contractor 		and
		IdCode		between :MemberIdFrom.Code	and 
					:MemberIdTo.Code 	and
		IdNumber	between :MemberIdFrom.Number 	and 
					:MemberIdTo.Number;
	
	EXEC SQL Open	CheckLabResCursor;

	JUMP_ON_ERR;

	EXEC SQL Fetch	CheckLabResCursor   Into
		:v_Contractor;

	if( SQLERR_code_cmp( SQLERR_not_found ) == MAC_TRUE )
	{
	    Result = 0;
	}
	else if( ! SQLERR_error_test() ) 
	{
	    Result = 1;
	}
	else 
	{
	    JUMP_ON_ERR;
	}

	EXEC SQL Close	CheckLabResCursor;
	if ( SQLERR_error_test() )
	{
	    Result = 0;
	}
	
	return(Result);
}


int	DbCheckContractorMedData( 	TIdNumber	Contractor,
					TTermId	TermId)
{
EXEC SQL BEGIN DECLARE SECTION;
	TIdNumber	RefContractor	=	Contractor;
	TTermId	RefTermId		=	TermId;
	short 	Result;
EXEC SQL END DECLARE SECTION;

	EXEC SQL select 
		count(*) 
	into 
		:Result
	from	MedicalData
	where	
		RefContractor	=	:RefContractor 		and
		RefTermId	=	:RefTermId     		and
		StatusTerm      !=  1;
	
	JUMP_ON_ERR;

	return(Result);
} 

int	DbDecContractKbdEntryCounter( TContractRecord	*Ptr)
{
EXEC SQL BEGIN DECLARE SECTION;
	TContractRecord *Contract	=	Ptr;
EXEC SQL END DECLARE SECTION;

	EXEC SQL Update contract
	set 
		AllowKbdEntry	=	AllowKbdEntry-1
	where	
		Contractor	=	:Contract->Contractor	and
		ContractType	=	:Contract->ContractType;

	JUMP_ON_ERR;

	return(1);
}

int	DbInsertConfLabRes( 	TIdNumber	Contractor,
				TMemberId	*MemberId,
				int	ReqId,int TermId,	TDbDateTime	LabCnfDate)
{
EXEC SQL BEGIN DECLARE SECTION;
	TLabRecord	Lab;
	int Date,Time;
EXEC SQL END DECLARE SECTION;

	Lab.Contractor	=	Contractor;
	Lab.MemberId	=	*MemberId;
	Lab.ReqId	=	ReqId;
	Lab.TermId = TermId;
	Date = 	LabCnfDate.DbDate;
	Time = 	LabCnfDate.DbTime;

	EXEC SQL Insert Into ConfLabRes
		( Contractor, 	IdNumber, 	IdCode,	ReqId,
			TermId,		InsertDate,	InsertTime)
	values	
	(
		:Lab.Contractor, 	:Lab.MemberId.Number,
		:Lab.MemberId.Code, 	:Lab.ReqId,
		:Lab.TermId,	:Date,	:Time
	);

	JUMP_ON_ERR;

	return(1);
}




int	DbInsertConfRentgen( 	TIdNumber	Contractor,
				TMemberId	*MemberId,
				int	RentgenId,
				int TermId,
				TDbDateTime SesTime)
{
EXEC SQL BEGIN DECLARE SECTION;
	TIdNumber	Contractor_p;
	TMemberId	*MemberId_p;
	int	RentgenId_p;
	int TermId_p;
	TDbDateTime SesTime_p; 
EXEC SQL END DECLARE SECTION;

	Contractor_p		=	Contractor;
	MemberId_p			=	MemberId;
	RentgenId_p			=	RentgenId;
	SesTime_p.DbDate	=	SesTime.DbDate;	
	SesTime_p.DbTime	=	SesTime.DbTime;	
	TermId_p			=	TermId;

	EXEC SQL Insert Into ConfRentgen
	(	Contractor, 	IdNumber, 	IdCode,	
		RentgenId,		TermId,		InsertDate,	InsertTime)
	values	
	(
		:Contractor_p, 		:MemberId_p->Number,		:MemberId_p->Code,
		:RentgenId_p,		:TermId_p,
		:SesTime_p.DbDate,	:SesTime_p.DbTime
	);

	JUMP_ON_ERR;
	return(1);
}

int	DbDeleteLabResults( 	TIdNumber	Contractor,
				TMemberId	*MemberId,
				int	ReqId)
{
	EXEC SQL BEGIN DECLARE SECTION;
	TLabRecord	Lab;
	EXEC SQL END DECLARE SECTION;

	Lab.Contractor	=	Contractor;
	Lab.MemberId	=	*MemberId;
	Lab.ReqId	=	ReqId;


	EXEC SQL Delete from LabResults 
	where
		Contractor	=	:Lab.Contractor		and
		IdNumber	=	:Lab.MemberId.Number	and
		IdCode		=	:Lab.MemberId.Code	and
		ReqId		=	:Lab.ReqId;

	EXEC SQL Delete from LabNotes 
	where
		Contractor	=	:Lab.Contractor		and
		IdNumber	=	:Lab.MemberId.Number	and
		IdCode		=	:Lab.MemberId.Code	and
		ReqId		=	:Lab.ReqId;


	JUMP_ON_ERR;

	return(1);
}

int	DbGetAuthRecord( 	TIdNumber	Contractor,
				TMemberId	*MemberId,
				TSCounter	Quarter,
				TAuthRecord	*Ptr)
{
    EXEC SQL BEGIN DECLARE SECTION;
	TAuthRecord	*Auth	=	Ptr;
	EXEC SQL END DECLARE SECTION;

	Auth->MemberId		=	*MemberId;
	Auth->Contractor	=	Contractor;
	Auth->AuthQuarter	=	Quarter;


	EXEC SQL select
		IdCode, 		IdNumber,		CardIssueDate,
		SesDate, 		SesTime,
		GnrlElegebility,	DentalElegebility,
		Contractor, 		ContractType, 		ProfessionGroup,
		AuthQuarter, 		AuthDate,		AuthTime, 	
		AuthError, 		ValidUntil,		TermId,
		Location,		Locationprv,
		AuthType, 		VisitCounter,		Cancled
	into
		:Auth->MemberId.Code,		:Auth->MemberId.Number,
		:Auth->CardIssueDate,
		:Auth->SesTime.DbDate,		:Auth->SesTime.DbTime,
		:Auth->GnrlElegebility,		:Auth->DentalElegebility,
		:Auth->Contractor,		:Auth->ContractType,
		:Auth->ProfessionGroup,
		:Auth->AuthQuarter,
		:Auth->AuthTime.DbDate,		:Auth->AuthTime.DbTime,
		:Auth->AuthError,
		:Auth->ValidUntil,		:Auth->TermId,
		:Auth->Location,
		:Auth->Locationprv,		:Auth->AuthType,
		:Auth->VisitCounter,		:Auth->Cancled
	from	auth
	where	
		Contractor	=	:Auth->Contractor	and
		IdCode		=	:Auth->MemberId.Code	and
		IdNumber	=	:Auth->MemberId.Number	and
		AuthQuarter	=	:Auth->AuthQuarter	and
		Cancled 	=	0;

	JUMP_ON_ERR;

	return(1);
}

int	DbOpenLabResultSelCursor( 	TIdNumber	ContractorParam,
//20010418				        TTermId         TermIdParam)
					TMemberId	*MemberIdParam)
{
EXEC SQL BEGIN DECLARE SECTION;
	TIdNumber	Contractor	=	ContractorParam;
//20010418	TTermId     TermID          =       TermIdParam;
	TMemberId	MemberIdFirst;
	TMemberId	MemberIdLast;

EXEC SQL END DECLARE SECTION;

	/*$TMemberId	MemberIdFirst;
	TMemberId	MemberIdLast;
*/
	if(	MemberIdParam->Number	!=	0	)
	{
		MemberIdFirst	=	*MemberIdParam;
		MemberIdLast	=	*MemberIdParam;
	} 
	else
	{
		MemberIdFirst.Number	=	0;
		MemberIdLast.Number	=	0x7fffffffl;
		MemberIdFirst.Code	=	0;
		MemberIdLast.Code	=	0x7fff;
	} 

	EXEC SQL Declare	LabResultsCursor Cursor	for
	select
		Contractor,	TermId,		IdCode,		IdNumber,
		LabId,		ReqId,		ReqCode,
		Route,		Result,		LowerLimit,	UpperLimit,
		Units,		ApprovalDate,	ForwardDate,	ReqDate,
		PregnantWoman,	Completed
	from	LabResults
	where	
		Contractor	=	:Contractor 		and

		IdNumber	between :MemberIdFirst.Number	and 
							:MemberIdLast.Number		and
		IdCode		between :MemberIdFirst.Code	and
							:MemberIdLast.Code
	Order	By Contractor,IdNumber, IdCode, ReqId, ReqCode, TermId;
	EXEC SQL OPEN   LabResultsCursor;
    //20010118            ( TermID = 0  or :TermID = TermID ) 
	//	EXEC SQL Open	LabResultsCursor;

	JUMP_ON_ERR;

	return(1);
}


int DbFetchLabResultSelCursor( TLabRecord	*Ptr)
{
EXEC SQL BEGIN DECLARE SECTION;
	TLabRecord *Lab	=	Ptr;
EXEC SQL END DECLARE SECTION;
	EXEC SQL Fetch	LabResultsCursor	Into
		:Lab->Contractor,	:Lab->TermId,			
		:Lab->MemberId.Code,	:Lab->MemberId.Number,
		:Lab->LabId,		:Lab->ReqId,
		:Lab->ReqCode,		:Lab->Route,
		:Lab->Result,		:Lab->LowerLimit,
		:Lab->UpperLimit, 	:Lab->Units,
		:Lab->ApprovalDate,	:Lab->ForwardDate,
		:Lab->ReqDate,
		:Lab->PregnantWoman,	:Lab->Completed;

	JUMP_ON_ERR;

	return(1);
}

int DbCloseLabResultSelCursor(void)
{
	EXEC SQL Close	LabResultsCursor;
	if ( SQLERR_error_test() )
	{
	return (0);
	}
	else
	{
	return(1);
	}
}

/*20010218*/
int DbGetLastUpdateDates(char *TableName,TDateYYYYMMDD *LastUpdateDate,TTimeHHMMSS *LastUpdateTime)
{
EXEC SQL BEGIN DECLARE SECTION;
	char table_name[32];
	TDateYYYYMMDD	LastDate;
	TTimeHHMMSS		LastTime;
EXEC SQL END DECLARE SECTION;
	strcpy (table_name,TableName);
	EXEC SQL 		SELECT
			update_date,update_time 
			into 	:LastDate,:LastTime
			from tables_update
			where table_name = :table_name;
	JUMP_ON_ERR;
	*LastUpdateDate = LastDate;
	*LastUpdateTime = LastTime;
	return(1);
}



int	DbOpenDrugsSelectCursor(TDateYYYYMMDD	LastDate,
							TTimeHHMMSS		LastTime)
{
	EXEC SQL BEGIN DECLARE SECTION;
		TDateYYYYMMDD	LastUpdateDate	=	LastDate;
		TTimeHHMMSS		LastUpdateTime	=	LastTime;
		TTimeHHMMSS		NowTime;
		TDateYYYYMMDD	NowDate;
	EXEC SQL END DECLARE SECTION;


	NowDate = GetDate();
	NowTime = GetTime();

	/*20030402 Yulia change update_date/time to _d for difference between pharm and doctor system*/
	// DonR 08Jan2004: Fixed date/time selection logic.
	EXEC SQL
		DECLARE DrugsCursor CURSOR FOR

		SELECT	largo_code,			pharm_group_code,
				short_name,			form_name,
				package,			package_size,
				price_prcnt,		price_prcnt_magen,
				price_prcnt_keren,	drug_type,
				pharm_sale_new,		sale_price,
				drug_book_doct_flg,	cronic_months,
				interact_flg,		magen_wait_months,
				keren_wait_months,	del_flg_cont,
				update_date_d,		update_time_d

		FROM	drug_list

		WHERE	status_send > 0																AND

				((update_date_d > :LastUpdateDate)	OR
				 (update_date_d = :LastUpdateDate	AND update_time_d >= :LastUpdateTime))	AND

				((update_date_d < :NowDate)			OR
				 (update_date_d = :NowDate			AND update_time_d <  :NowTime))

		ORDER BY update_date_d, update_time_d;


	// DonR 13Mar2005: Try avoiding record-locking errors by performing "dirty reads".
	EXEC SQL
		SET ISOLATION TO DIRTY READ;

	EXEC SQL
		OPEN DrugsCursor;

	JUMP_ON_ERR;

	return(1);
}



int DbFetchDrugsSelectCursor( TDrugRecord	*Ptr,int *LastDate,int *LastTime )
{
EXEC SQL BEGIN DECLARE SECTION;
	TDrugRecord *DrugRec	=	Ptr;
	int del_flg ;
	int Date_sel,Time_sel;
EXEC SQL END DECLARE SECTION;
	EXEC SQL Fetch	DrugsCursor	Into
		:DrugRec->largo_code,
		:DrugRec->pharm_group_code,
		:DrugRec->short_name,
		:DrugRec->form_name,
		:DrugRec->package,
		:DrugRec->package_size,
		:DrugRec->price_prcnt,
		:DrugRec->price_prcnt_magen,
		:DrugRec->price_prcnt_keren,
		:DrugRec->drug_type,
		:DrugRec->pharm_sale_new,
		:DrugRec->sale_price,
		:DrugRec->drug_book_flg,
/*old		:DrugRec->priv_pharm_sale_ok,*/
/*old		:DrugRec->yarpa_price,*/
		:DrugRec->cronic_months,
		:DrugRec->interact_flg,
		
		:DrugRec->magen_wait_months ,
		:DrugRec->keren_wait_months ,
		:DrugRec->DelFlag,
		:Date_sel,
		:Time_sel;

	JUMP_ON_ERR;

	*LastDate = Date_sel;
	*LastTime = Time_sel;
	if ( SQLERR_error_test() )
	{
	return (0);
	}
	else
	{
/*		if (del_flg == 0)
		{
			strcpy(DrugRec->DelFlag , " ");
		}
		else
		{
			strcpy(DrugRec->DelFlag , "D");
		}
*/		if (DrugRec->price_prcnt_magen == 0) DrugRec->price_prcnt_magen= 999;
		if (DrugRec->price_prcnt_keren == 0) DrugRec->price_prcnt_keren= 999;
		DrugRec->cronic_months = DrugRec->cronic_months * 30;
			//temp 
//		DrugRec->magen_wait_months = 24;
//		DrugRec->keren_wait_months = 18;

		return(1);
	}

//if (DrugRec->price_prcnt_magen == 0) DrugRec->price_prcnt_magen= 999;
//if (DrugRec->price_prcnt_keren == 0) DrugRec->price_prcnt_keren= 999;
//DrugRec->cronic_months = DrugRec->cronic_months * 30;
	//temp 
//DrugRec->magen_wait_months = 0;
//DrugRec->keren_wait_months = 0;

  }
int DbCloseDrugsSelectCursor(void)
{
	// DonR 23Feb2005: Return to normal isolation level.
	EXEC SQL
		SET ISOLATION TO COMMITTED READ;

	EXEC SQL Close	DrugsCursor;
	if ( SQLERR_error_test() )
	{
	return (0);
	}
	else
	{
	return(1);
	}
}



int	DbOpenGenCompSelectCursor (TDateYYYYMMDD	LastDate,
							   TTimeHHMMSS		LastTime)
{
	EXEC SQL BEGIN DECLARE SECTION;
		TDateYYYYMMDD	LastUpdateDate	=	LastDate;
		TTimeHHMMSS		LastUpdateTime	=	LastTime;
		TTimeHHMMSS		NowTime;
		TTimeHHMMSS		NowDate;
	EXEC SQL END DECLARE SECTION;


	NowDate = GetDate();
	NowTime = GetTime();

	// DonR 08Jan2004: Fixed date/time selection logic.
	EXEC SQL
		DECLARE GenCompCursor CURSOR FOR

		SELECT	gen_comp_code,
				gen_comp_desc,
				del_flg,
				update_date,
				update_time

		FROM	GenComponents

		WHERE	((update_date > :LastUpdateDate)	OR
				 (update_date = :LastUpdateDate	AND update_time >= :LastUpdateTime))	AND

				((update_date < :NowDate)			OR
				 (update_date = :NowDate		AND update_time <  :NowTime))

		ORDER BY update_date, update_time, del_flg DESC;


	EXEC SQL
		OPEN GenCompCursor;

	JUMP_ON_ERR;

	return(1);
}



int DbFetchGenCompSelectCursor( TGenCompRecord	*Ptr,int *LastDate,int *LastTime )
{
EXEC SQL BEGIN DECLARE SECTION;
	int Date_sel,Time_sel;
	TGenCompRecord *GenCompRec	=	Ptr;
EXEC SQL END DECLARE SECTION;
	EXEC SQL Fetch	GenCompCursor	Into
		:GenCompRec->GenCompCode,
		:GenCompRec->GenCompDesc,
		:GenCompRec->DelFlag,
		:Date_sel,
		:Time_sel;
	JUMP_ON_ERR;
	*LastDate = Date_sel;
	*LastTime = Time_sel;
	if ( SQLERR_error_test() )
	{
	return (0);
	}
	else
	{
	return(1);
	}
}

int DbCloseGenCompSelectCursor(void)
{
	EXEC SQL Close	GenCompCursor;
	if ( SQLERR_error_test() )
	{
	return (0);
	}
	else
	{
	return(1);
	}
}



int	DbOpenDrugGenCompSelectCursor (TDateYYYYMMDD	LastDate,
								   TTimeHHMMSS		LastTime)
{
	EXEC SQL BEGIN DECLARE SECTION;
		TDateYYYYMMDD	LastUpdateDate	=	LastDate;
		TTimeHHMMSS		LastUpdateTime	=	LastTime;
		TTimeHHMMSS		NowTime;
		TTimeHHMMSS		NowDate;
	EXEC SQL END DECLARE SECTION;


	NowDate = GetDate();
	NowTime = GetTime();

	// DonR 08Jan2004: Fixed date/time selection logic.
	EXEC SQL
		DECLARE DrugGenCompCursor CURSOR FOR

		SELECT	largo_code,
				gen_comp_code,
				del_flg,
				update_date,
				update_time

		FROM	DrugGenComponents

		WHERE	((update_date > :LastUpdateDate)	OR
				 (update_date = :LastUpdateDate	AND update_time >= :LastUpdateTime))	AND

				((update_date < :NowDate)			OR
				 (update_date = :NowDate		AND update_time <  :NowTime))

		ORDER BY update_date, update_time, del_flg DESC;


	EXEC SQL
		OPEN DrugGenCompCursor;

	JUMP_ON_ERR;

	return(1);
}



int DbFetchDrugGenCompSelectCursor( TDrugGenCompRecord	*Ptr,int *LastDate,int *LastTime )
{
EXEC SQL BEGIN DECLARE SECTION;
	int Date_sel,Time_sel;
	TDrugGenCompRecord *DrugGenCompRec	=	Ptr;
EXEC SQL END DECLARE SECTION;
	EXEC SQL Fetch	DrugGenCompCursor	Into
		:DrugGenCompRec->DrugCode,
		:DrugGenCompRec->GenCompCode,
		:DrugGenCompRec->DelFlag,
		:Date_sel,
		:Time_sel;
	JUMP_ON_ERR;
	*LastDate = Date_sel;
	*LastTime = Time_sel;
	if ( SQLERR_error_test() )
	{
	return (0);
	}
	else
	{
	return(1);
	}
}

int DbCloseDrugGenCompSelectCursor(void)
{
	EXEC SQL Close	DrugGenCompCursor;
	if ( SQLERR_error_test() )
	{
	return (0);
	}
	else
	{
	return(1);
	}
}



int	DbOpenGenInterSelectCursor (TDateYYYYMMDD	LastDate,
								TTimeHHMMSS		LastTime)
{
	EXEC SQL BEGIN DECLARE SECTION;
		TDateYYYYMMDD	LastUpdateDate	=	LastDate;
		TTimeHHMMSS		LastUpdateTime	=	LastTime;
		TTimeHHMMSS		NowTime;
		TTimeHHMMSS		NowDate;
	EXEC SQL END DECLARE SECTION;


	NowDate = GetDate();
	NowTime = GetTime();

	// DonR 08Jan2004: Fixed date/time selection logic.
	EXEC SQL
		DECLARE GenInterCursor CURSOR FOR

		SELECT	first_gen_code,
				second_gen_code,
				interaction_type,
				inter_note_code,
				del_flg,
				update_date,
				update_time

		FROM	GeneryInteraction

		WHERE	((update_date > :LastUpdateDate)	OR
				 (update_date = :LastUpdateDate	AND update_time >= :LastUpdateTime))	AND

				((update_date < :NowDate)			OR
				 (update_date = :NowDate		AND update_time <  :NowTime))

		ORDER BY update_date, update_time, del_flg DESC;


	EXEC SQL
		OPEN GenInterCursor;

	JUMP_ON_ERR;

	return(1);
}



int DbFetchGenInterSelectCursor( TGenInterRecord	*Ptr,int *LastDate,int *LastTime )
{
EXEC SQL BEGIN DECLARE SECTION;
	TGenInterRecord *GenInterRec	=	Ptr;
	int Date_sel,Time_sel;
EXEC SQL END DECLARE SECTION;
	EXEC SQL Fetch	GenInterCursor	Into
		:GenInterRec->GenCompCode1,
		:GenInterRec->GenCompCode2,
		:GenInterRec->InteractType,
		:GenInterRec->InteractCode,
		:GenInterRec->DelFlag,
		:Date_sel,
		:Time_sel;
	JUMP_ON_ERR;
	*LastDate = Date_sel;
	*LastTime = Time_sel;
	if ( SQLERR_error_test() )
	{
	return (0);
	}
	else
	{
	return(1);
	}
}

int DbCloseGenInterSelectCursor(void)
{
	EXEC SQL Close	GenInterCursor;
	if ( SQLERR_error_test() )
	{
	return (0);
	}
	else
	{
	return(1);
	}
}



int	DbOpenGenInterTextSelectCursor (TDateYYYYMMDD	LastDate,
									TTimeHHMMSS		LastTime)
{
	EXEC SQL BEGIN DECLARE SECTION;
		TDateYYYYMMDD	LastUpdateDate	=	LastDate;
		TTimeHHMMSS		LastUpdateTime	=	LastTime;
		TTimeHHMMSS		NowTime;
		TTimeHHMMSS		NowDate;
	EXEC SQL END DECLARE SECTION;


	NowDate = GetDate();
	NowTime = GetTime();

	// DonR 08Jan2004: Fixed date/time selection logic.
	EXEC SQL
		DECLARE GenInterTextCur CURSOR FOR

		SELECT	inter_note_code,
				interaction_note,
				del_flg,
				update_date,
				update_time

		FROM	GenInterNotes

		WHERE	((update_date > :LastUpdateDate)	OR
				 (update_date = :LastUpdateDate	AND update_time >= :LastUpdateTime))	AND

				((update_date < :NowDate)			OR
				 (update_date = :NowDate		AND update_time <  :NowTime))

		ORDER BY update_date, update_time, del_flg DESC;


	EXEC SQL
		OPEN GenInterTextCur;

	JUMP_ON_ERR;

	return(1);
}



int DbFetchGenInterTextSelectCursor( TGenInterTextRecord	*Ptr,int *LastDate,int *LastTime )
{
EXEC SQL BEGIN DECLARE SECTION;
	TGenInterTextRecord *GenInterTextRec	=	Ptr;
	int Date_sel,Time_sel;
EXEC SQL END DECLARE SECTION;
	EXEC SQL Fetch	GenInterTextCur	Into
		:GenInterTextRec->InteractCode,
		:GenInterTextRec->InteractDesc,
		:GenInterTextRec->DelFlag,
		:Date_sel,
		:Time_sel;
	JUMP_ON_ERR;
	*LastDate = Date_sel;
	*LastTime = Time_sel;
	if ( SQLERR_error_test() )
	{
	return (0);
	}
	else
	{
	return(1);
	}
}

int DbCloseGenInterTextSelectCursor(void)
{
	EXEC SQL Close	GenInterTextCur;
	if ( SQLERR_error_test() )
	{
	return (0);
	}
	else
	{
	return(1);
	}
}



int	DbOpenGnrlDrugNotesSelectCursor (TDateYYYYMMDD	LastDate,
									 TTimeHHMMSS		LastTime)
{
	EXEC SQL BEGIN DECLARE SECTION;
		TDateYYYYMMDD	LastUpdateDate	=	LastDate;
		TTimeHHMMSS		LastUpdateTime	=	LastTime;
		TTimeHHMMSS		NowTime;
		TTimeHHMMSS		NowDate;
	EXEC SQL END DECLARE SECTION;


	NowDate = GetDate();
	NowTime = GetTime();

	// DonR 08Jan2004: Fixed date/time selection logic.
	EXEC SQL
		DECLARE GnrlDrugNotesCurs CURSOR FOR

		SELECT	gnrldrugnotetype,
				gnrldrugnotecode,
				gnrldrugnote,
				del_flg,
				update_date,
				update_time

		FROM	GnrlDrugNotes

		WHERE	((update_date > :LastUpdateDate)	OR
				 (update_date = :LastUpdateDate	AND update_time >= :LastUpdateTime)) AND

				((update_date < :NowDate)			OR
				 (update_date = :NowDate		AND update_time <  :NowTime))

		ORDER BY update_date, update_time, del_flg DESC;


	EXEC SQL
		OPEN GnrlDrugNotesCurs;

	JUMP_ON_ERR;

	return(1);
}



int DbFetchGnrlDrugNotesSelectCursor( TGnrlDrugNotesRecord	*Ptr,int *LastDate,int *LastTime )
{
EXEC SQL BEGIN DECLARE SECTION;
	TGnrlDrugNotesRecord *GnrlDrugNotesRec	=	Ptr;
	int Date_sel,Time_sel;
EXEC SQL END DECLARE SECTION;
	EXEC SQL Fetch	GnrlDrugNotesCurs	Into

		:GnrlDrugNotesRec->GnrlDrugNoteType,
		:GnrlDrugNotesRec->GnrlDrugNoteCode,
		:GnrlDrugNotesRec->GnrlDrugNote,
		:GnrlDrugNotesRec->DelFlag,
		:Date_sel,
		:Time_sel;
	JUMP_ON_ERR;
	*LastDate = Date_sel;
	*LastTime = Time_sel;
	if ( SQLERR_error_test() )
	{
	return (0);
	}
	else
	{
	return(1);
	}
}

int DbCloseGnrlDrugNotesSelectCursor(void)
{
	EXEC SQL Close	GnrlDrugNotesCurs;
	if ( SQLERR_error_test() )
	{
	return (0);
	}
	else
	{
	return(1);
	}
}



int	DbOpenDrugNotesSelectCursor (TDateYYYYMMDD	LastDate,
								 TTimeHHMMSS	LastTime)
{
	EXEC SQL BEGIN DECLARE SECTION;
		TDateYYYYMMDD	LastUpdateDate	=	LastDate;
		TTimeHHMMSS		LastUpdateTime	=	LastTime;
		TTimeHHMMSS		NowTime;
		TTimeHHMMSS		NowDate;
	EXEC SQL END DECLARE SECTION;


	NowDate = GetDate();
	NowTime = GetTime();

	// DonR 08Jan2004: Fixed date/time selection logic.
	EXEC SQL
		DECLARE DrugNotesCursor CURSOR FOR

		SELECT	largo_code,
				gnrldrugnotetype,
				gnrldrugnotecode,
				del_flg,
				update_date,
				update_time

		FROM	DrugNotes

		WHERE	((update_date > :LastUpdateDate)	OR
				 (update_date = :LastUpdateDate	AND update_time >= :LastUpdateTime)) AND

				((update_date < :NowDate)			OR
				 (update_date = :NowDate		AND update_time <  :NowTime))

		ORDER BY	update_date,
					update_time,
					del_flg DESC,
					largo_code,
					gnrldrugnotetype,
					gnrldrugnotecode;


	EXEC SQL
		OPEN DrugNotesCursor;

	JUMP_ON_ERR;

	return(1);
}



int DbFetchDrugNotesSelectCursor( TDrugNotesRecord	*Ptr,int *LastDate,int *LastTime )
{
EXEC SQL BEGIN DECLARE SECTION;
	TDrugNotesRecord *DrugNotesRec	=	Ptr;
	int Date_sel,Time_sel;
EXEC SQL END DECLARE SECTION;
	EXEC SQL Fetch	DrugNotesCursor	Into
		:DrugNotesRec->DrugCode,
		:DrugNotesRec->GnrlDrugNoteType,
		:DrugNotesRec->GnrlDrugNoteCode,
		:DrugNotesRec->DelFlag,
		:Date_sel,
		:Time_sel;
	JUMP_ON_ERR;
	*LastDate = Date_sel;
	*LastTime = Time_sel;
	if ( SQLERR_error_test() )
	{
	return (0);
	}
	else
	{
	return(1);
	}
}

int DbCloseDrugNotesSelectCursor(void)
{
	EXEC SQL Close	DrugNotesCursor;
	if ( SQLERR_error_test() )
	{
	return (0);
	}
	else
	{
	return(1);
	}
}



int	DbOpenDrugDoctorProfSelectCursor (TDateYYYYMMDD	LastDate,
									  TTimeHHMMSS		LastTime)
{
	EXEC SQL BEGIN DECLARE SECTION;
		TDateYYYYMMDD	LastUpdateDate	=	LastDate;
		TTimeHHMMSS		LastUpdateTime	=	LastTime;
		TTimeHHMMSS		NowTime;
		TTimeHHMMSS		NowDate;
	EXEC SQL END DECLARE SECTION;


	NowDate = GetDate();
	NowTime = GetTime();

	// DonR 08Jan2004: Fixed date/time selection logic.
	EXEC SQL
		DECLARE DrugDoctorProfCurs CURSOR FOR

		SELECT	largo_code,			professiongroup,
				price_prcnt_gnrl,	price_prcnt_keren,
				price_prcnt_magen,	del_flg,
				update_date,		update_time

		FROM	DrugDoctorProf

		WHERE	((update_date > :LastUpdateDate)	OR
				 (update_date = :LastUpdateDate	AND update_time >= :LastUpdateTime)) AND

				((update_date < :NowDate)			OR
				 (update_date = :NowDate		AND update_time <  :NowTime))

		ORDER BY update_date, update_time, del_flg DESC;


	EXEC SQL
		OPEN DrugDoctorProfCurs;

	JUMP_ON_ERR;

	return(1);
}



int DbFetchDrugDoctorProfSelectCursor( TDrugDoctorProfRecord	*Ptr,int *LastDate,int *LastTime )
{
EXEC SQL BEGIN DECLARE SECTION;
	TDrugDoctorProfRecord *DrugDoctorProfRec	=	Ptr;
	int Date_sel,Time_sel;
EXEC SQL END DECLARE SECTION;
	EXEC SQL Fetch	DrugDoctorProfCurs	Into
		:DrugDoctorProfRec->DrugCode,
		:DrugDoctorProfRec->ProfGroup,
		:DrugDoctorProfRec->PricePrcntGnrl,
		:DrugDoctorProfRec->PricePrcntKeren,
		:DrugDoctorProfRec->PricePrcntMagen,
		:DrugDoctorProfRec->DelFlag,
		:Date_sel,
		:Time_sel;
	JUMP_ON_ERR;
	*LastDate = Date_sel;
	*LastTime = Time_sel;
	if ( SQLERR_error_test() )
	{
	return (0);
	}
	else
	{
	return(1);
	}
}

int DbCloseDrugDoctorProfSelectCursor(void)
{
	EXEC SQL Close	DrugDoctorProfCurs;
	if ( SQLERR_error_test() )
	{
	return (0);
	}
	else
	{
	return(1);
	}
}



// DonR 16Jan2003: Changed sorting of rows, reformatted function.

int	DbOpenEconomyPriSelectCursor (TDateYYYYMMDD	LastDate,
								  TTimeHHMMSS	LastTime)
{
	EXEC SQL BEGIN DECLARE SECTION;
		TDateYYYYMMDD	LastUpdateDate	=	LastDate;
		TTimeHHMMSS		LastUpdateTime	=	LastTime;
		TTimeHHMMSS		NowTime;
		TTimeHHMMSS		NowDate;
	EXEC SQL END DECLARE SECTION;


	NowDate = GetDate();
	NowTime = GetTime();


	// DonR 08Jan2004: Fixed date/time selection logic.
	EXEC SQL
		DECLARE	EconomyPriCursor CURSOR FOR

		SELECT	drug_group,
				group_code,
				item_seq,	/* DonR 06Sep2004: Send Item Seq instead of Group # for old Clicks. */
				largo_code,
				del_flg,
				update_date,
				update_time,
				received_date,
				received_time,
				received_seq

		FROM	EconomyPri

		WHERE	((update_date > :LastUpdateDate)	OR
				 (update_date = :LastUpdateDate	AND update_time >= :LastUpdateTime))	AND

				((update_date < :NowDate)			OR
				 (update_date = :NowDate		AND update_time <  :NowTime))			AND

				(system_code = '0' OR system_code = '2')

		ORDER BY	received_date,
					received_time,
					received_seq;


	EXEC SQL
		OPEN EconomyPriCursor;

	JUMP_ON_ERR;

	return(1);
}

// DonR 16Jan2003 end.



int DbFetchEconomyPriSelectCursor( TEconomyPriRecord	*Ptr,int *LastDate,int *LastTime )
{
EXEC SQL BEGIN DECLARE SECTION;
	TEconomyPriRecord *EconomyPriRec	=	Ptr;
	int Date_sel,Time_sel;
	int RcvDate, RcvTime, RcvSeq;
EXEC SQL END DECLARE SECTION;
	EXEC SQL Fetch	EconomyPriCursor	Into
		:EconomyPriRec->DrugGroup,
		:EconomyPriRec->GroupCode,
		:EconomyPriRec->GroupNbr,
		:EconomyPriRec->DrugCode,
		:EconomyPriRec->DelFlag,
		:Date_sel,
		:Time_sel,
		:RcvDate,
		:RcvTime,
		:RcvSeq;
	JUMP_ON_ERR;
	*LastDate = Date_sel;
	*LastTime = Time_sel;
	if ( SQLERR_error_test() )
	{
	return (0);
	}
	else
	{
	return(1);
	}
}

int DbCloseEconomyPriSelectCursor(void)
{
	EXEC SQL Close	EconomyPriCursor;
	if ( SQLERR_error_test() )
	{
	return (0);
	}
	else
	{
	return(1);
	}
}



int	DbOpenEconomyPriDeleteCursor (TDateYYYYMMDD	LastDate,
								  TTimeHHMMSS	LastTime)
{
	EXEC SQL BEGIN DECLARE SECTION;
		TDateYYYYMMDD	LastUpdateDate	=	LastDate;
		TTimeHHMMSS		LastUpdateTime	=	LastTime;
		TTimeHHMMSS		NowTime;
		TTimeHHMMSS		NowDate;
	EXEC SQL END DECLARE SECTION;


	NowDate = GetDate();
	NowTime = GetTime();


	// DonR 08Jan2004: Fixed date/time selection logic.
	EXEC SQL
		DECLARE	EconomyPriDelCur CURSOR FOR

		SELECT	drug_group,
				group_code,
				group_nbr,
				largo_code,
				del_flg,
				update_date,
				update_time,
				received_date,
				received_time,
				received_seq

		FROM	EconomyPri

		WHERE	((update_date > :LastUpdateDate)	OR
				 (update_date = :LastUpdateDate	AND update_time >= :LastUpdateTime))	AND

				((update_date < :NowDate)			OR
				 (update_date = :NowDate		AND update_time <  :NowTime))			AND

				(system_code = '0' OR system_code = '2')								AND

				del_flg = "D"

		ORDER BY	received_date,
					received_time,
					received_seq;


	EXEC SQL
		OPEN EconomyPriDelCur;

	JUMP_ON_ERR;

	return(1);
}

// DonR 16Jan2003 end.



int DbFetchEconomyPriDeleteCursor( TEconomyPriRecord	*Ptr,int *LastDate,int *LastTime )
{
EXEC SQL BEGIN DECLARE SECTION;
	TEconomyPriRecord *EconomyPriRec	=	Ptr;
	int Date_sel,Time_sel;
	int RcvDate, RcvTime, RcvSeq;
EXEC SQL END DECLARE SECTION;

	EXEC SQL Fetch	EconomyPriDelCur	Into
		:EconomyPriRec->DrugGroup,
		:EconomyPriRec->GroupCode,
		:EconomyPriRec->GroupNbr,
		:EconomyPriRec->DrugCode,
		:EconomyPriRec->DelFlag,
		:Date_sel,
		:Time_sel,
		:RcvDate,
		:RcvTime,
		:RcvSeq;

	JUMP_ON_ERR;

	*LastDate = Date_sel;
	*LastTime = Time_sel;

	if ( SQLERR_error_test() )
	{
	return (0);
	}
	else
	{
	return(1);
	}
}

int DbCloseEconomyPriDeleteCursor(void)
{
	EXEC SQL Close	EconomyPriDelCur;
	if ( SQLERR_error_test() )
	{
	return (0);
	}
	else
	{
	return(1);
	}
}



int	DbOpenPharmacologySelectCursor (TDateYYYYMMDD	LastDate,
									TTimeHHMMSS		LastTime)
{
	EXEC SQL BEGIN DECLARE SECTION;
		TDateYYYYMMDD	LastUpdateDate	=	LastDate;
		TTimeHHMMSS		LastUpdateTime	=	LastTime;
		TTimeHHMMSS		NowTime;
		TTimeHHMMSS		NowDate;
	EXEC SQL END DECLARE SECTION;


	NowDate = GetDate();
	NowTime = GetTime();

	// DonR 08Jan2004: Fixed date/time selection logic.
	EXEC SQL
		DECLARE PharmacologyCursor CURSOR FOR

		SELECT	pharm_group_code,	pharm_group_name,
				del_flg,			update_date,
				update_time

		FROM	Pharmacology

		WHERE	((update_date > :LastUpdateDate)	OR
				 (update_date = :LastUpdateDate	AND update_time >= :LastUpdateTime))	AND

				((update_date < :NowDate)			OR
				 (update_date = :NowDate		AND update_time <  :NowTime))

		ORDER BY update_date, update_time, del_flg DESC;


	EXEC SQL
		OPEN PharmacologyCursor;

	JUMP_ON_ERR;

	return(1);
}



int DbFetchPharmacologySelectCursor( TPharmacologyRecord	*Ptr,int *LastDate,int *LastTime )
{
EXEC SQL BEGIN DECLARE SECTION;
	TPharmacologyRecord *PharmacologyRec	=	Ptr;
	int Date_sel,Time_sel;
EXEC SQL END DECLARE SECTION;
	EXEC SQL Fetch	PharmacologyCursor	Into
		:PharmacologyRec->PharmGroupCode,
		:PharmacologyRec->PharmGroupName,
		:PharmacologyRec->DelFlag,
		:Date_sel,
		:Time_sel;
	JUMP_ON_ERR;
	*LastDate = Date_sel;
	*LastTime = Time_sel;
	if ( SQLERR_error_test() )
	{
	return (0);
	}
	else
	{
	return(1);
	}
}

int DbClosePharmacologySelectCursor(void)
{
	EXEC SQL Close	PharmacologyCursor;
	if ( SQLERR_error_test() )
	{
	return (0);
	}
	else
	{
	return(1);
	}
}

/*20010218*/



// DonR 29Mar2004 New transactions for Clicks 8.5 begin.
int	DbOpenDrugsSelectCursor85 (TDateYYYYMMDD	LastDate,
							   TTimeHHMMSS		LastTime)
{
	EXEC SQL BEGIN DECLARE SECTION;
		TDateYYYYMMDD	LastUpdateDate	=	LastDate;
		TTimeHHMMSS		LastUpdateTime	=	LastTime;
		TTimeHHMMSS		NowTime;
		TDateYYYYMMDD	NowDate;
	EXEC SQL END DECLARE SECTION;


	NowDate = GetDate();
	NowTime = GetTime();

	/*20030402 Yulia change update_date/time to _d for difference between pharm and doctor system*/
	// DonR 08Jan2004: Fixed date/time selection logic.
	EXEC SQL
		DECLARE DrugsCursor85 CURSOR FOR

		SELECT	largo_code,				pharm_group_code,
				short_name,				form_name,
				package,				package_size,
				package_volume,			ingr_1_code,
				ingr_2_code,			ingr_3_code,
				ingr_1_quant,			ingr_2_quant,
				ingr_3_quant,			ingr_1_units,
				ingr_2_units,			ingr_3_units,
				per_1_quant,			per_2_quant,
				per_3_quant,			per_1_units,
				per_2_units,			per_3_units,
				price_prcnt,			price_prcnt_magen,
				price_prcnt_keren,		drug_type,
				pharm_sale_new,			pharm_sale_test,
				sale_price_strudel,		drug_book_doct_flg,
				cronic_months,			rule_status,
				sensitivity_flag,		sensitivity_code,
				sensitivity_severe,		interact_flg,
				openable_pkg,			needs_29_g,
				chronic_flag,			shape_code_new,
				split_pill_flag,		treatment_typ_cod,
				print_generic_flg,		magen_wait_months,
				keren_wait_months,		substitute_drug,
				copies_to_print,		del_flg_cont,
				update_date_d,			update_time_d

		FROM	drug_list

		WHERE	status_send > 0															AND

				((update_date_d > :LastUpdateDate)	OR
				 (update_date_d = :LastUpdateDate	AND update_time_d >= :LastUpdateTime))	AND

				((update_date_d < :NowDate)			OR
				 (update_date_d = :NowDate			AND update_time_d <  :NowTime))

		ORDER BY update_date_d, update_time_d;


	EXEC SQL
		OPEN DrugsCursor85;

	JUMP_ON_ERR;

	return(1);
}

int DbFetchDrugsSelectCursor85 (TDrugRecord85	*Ptr,int *LastDate,int *LastTime )
{
	EXEC SQL BEGIN DECLARE SECTION;
		TDrugRecord85 *DrugRec	=	Ptr;
		int del_flg ;
		int Date_sel,Time_sel;
	EXEC SQL END DECLARE SECTION;

	EXEC SQL
		Fetch	DrugsCursor85	Into
									:DrugRec->largo_code,
									:DrugRec->pharm_group_code,
									:DrugRec->short_name,
									:DrugRec->form_name,
									:DrugRec->package,
									:DrugRec->package_size,
									:DrugRec->package_volume,
									:DrugRec->ingr_1_code,
									:DrugRec->ingr_2_code,
									:DrugRec->ingr_3_code,
									:DrugRec->ingr_1_quant,
									:DrugRec->ingr_2_quant,
									:DrugRec->ingr_3_quant,
									:DrugRec->ingr_1_units,
									:DrugRec->ingr_2_units,
									:DrugRec->ingr_3_units,
									:DrugRec->per_1_quant,
									:DrugRec->per_2_quant,
									:DrugRec->per_3_quant,
									:DrugRec->per_1_units,
									:DrugRec->per_2_units,
									:DrugRec->per_3_units,
									:DrugRec->price_prcnt,
									:DrugRec->price_prcnt_magen,
									:DrugRec->price_prcnt_keren,
									:DrugRec->drug_type,
									:DrugRec->pharm_sale_new,
									:DrugRec->pharm_sale_test,
									:DrugRec->sale_price,
									:DrugRec->drug_book_flg,
									:DrugRec->cronic_months,
									:DrugRec->rule_status,
									:DrugRec->sensitivity_flag,
									:DrugRec->sensitivity_code,
									:DrugRec->sensitivity_severe,
									:DrugRec->interact_flg,
									:DrugRec->openable_pkg,
									:DrugRec->needs_29_g,
									:DrugRec->chronic_flag,
									:DrugRec->shape_code,
									:DrugRec->split_pill_flag,
									:DrugRec->treatment_typ_cod,
									:DrugRec->print_generic_flg,
									:DrugRec->magen_wait_months,
									:DrugRec->keren_wait_months,
									:DrugRec->substitute_drug,
									:DrugRec->copies_to_print,
									:DrugRec->DelFlag,
									:Date_sel,
									:Time_sel;

	JUMP_ON_ERR;

	*LastDate = Date_sel;
	*LastTime = Time_sel;

	if ( SQLERR_error_test() )
	{
		return (0);
	}
	else
	{
		if (DrugRec->price_prcnt_magen == 0)
			DrugRec->price_prcnt_magen= 999;
		if (DrugRec->price_prcnt_keren == 0)
			DrugRec->price_prcnt_keren= 999;

		DrugRec->cronic_months = DrugRec->cronic_months * 30;

		return(1);
	}

}

int DbCloseDrugsSelectCursor85 (void)
{
	EXEC SQL
		Close	DrugsCursor85;

	if ( SQLERR_error_test() )
	{
		return (0);
	}
	else
	{
		return(1);
	}
}



int	DbOpenGenInterTextSelectCursor85 (TDateYYYYMMDD	LastDate,
									  TTimeHHMMSS	LastTime)
{
	EXEC SQL BEGIN DECLARE SECTION;
		TDateYYYYMMDD	LastUpdateDate	=	LastDate;
		TTimeHHMMSS		LastUpdateTime	=	LastTime;
		TTimeHHMMSS		NowTime;
		TDateYYYYMMDD	NowDate;
	EXEC SQL END DECLARE SECTION;


	NowDate = GetDate();
	NowTime = GetTime();

	// DonR 08Jan2004: Fixed date/time selection logic.
	EXEC SQL
		DECLARE GenInterTextCur85 CURSOR FOR

		SELECT	inter_note_code,
				inter_long_note,
				del_flg,
				update_date,
				update_time

		FROM	GenInterNotes

		WHERE	((update_date > :LastUpdateDate)	OR
				 (update_date = :LastUpdateDate	AND update_time >= :LastUpdateTime))	AND

				((update_date < :NowDate)			OR
				 (update_date = :NowDate		AND update_time <  :NowTime))

		ORDER BY update_date, update_time, del_flg DESC;


	EXEC SQL
		OPEN GenInterTextCur85;

	JUMP_ON_ERR;

	return(1);
}

int DbFetchGenInterTextSelectCursor85 (TGenInterTextRecord85 *Ptr,int *LastDate,int *LastTime )
{
	EXEC SQL BEGIN DECLARE SECTION;
		TGenInterTextRecord85 *GenInterTextRec	=	Ptr;
		int Date_sel,Time_sel;
	EXEC SQL END DECLARE SECTION;

	EXEC SQL
		Fetch	GenInterTextCur85	Into
										:GenInterTextRec->InteractCode,
										:GenInterTextRec->InteractLongDesc,
										:GenInterTextRec->DelFlag,
										:Date_sel,
										:Time_sel;

	JUMP_ON_ERR;
	*LastDate = Date_sel;
	*LastTime = Time_sel;
	if ( SQLERR_error_test() )
	{
		return (0);
	}
	else
	{
		return(1);
	}
}

int DbCloseGenInterTextSelectCursor85(void)
{
	EXEC SQL
		Close	GenInterTextCur85;

	if ( SQLERR_error_test() )
	{
		return (0);
	}
	else
	{
		return(1);
	}
}



int	DbOpenGnrlDrugNotesSelectCursor85 (TDateYYYYMMDD	LastDate,
									   TTimeHHMMSS		LastTime)
{
	EXEC SQL BEGIN DECLARE SECTION;
		TDateYYYYMMDD	LastUpdateDate	=	LastDate;
		TTimeHHMMSS		LastUpdateTime	=	LastTime;
		TTimeHHMMSS		NowTime;
		TDateYYYYMMDD	NowDate;
	EXEC SQL END DECLARE SECTION;


	NowDate = GetDate();
	NowTime = GetTime();

	// DonR 08Jan2004: Fixed date/time selection logic.
	EXEC SQL
		DECLARE GnrlDrugNotesCur85 CURSOR FOR

		SELECT	gdn_type_new,
				gnrldrugnotecode,
				gdn_long_note,
				gdn_category,
				gdn_sex,
				gdn_from_age,
				gdn_to_age,
				gdn_seq_num,
				gdn_connect_type,
				gdn_connect_desc,
				gdn_severity,
				gdn_sensitv_type,
				gdn_sensitv_desc,
				del_flg,
				update_date,
				update_time

		FROM	GnrlDrugNotes

		WHERE	((update_date > :LastUpdateDate)	OR
				 (update_date = :LastUpdateDate	AND update_time >= :LastUpdateTime))	AND

				((update_date < :NowDate)			OR
				 (update_date = :NowDate		AND update_time <  :NowTime))

		ORDER BY update_date, update_time, del_flg DESC;


	EXEC SQL
		OPEN GnrlDrugNotesCur85;

	JUMP_ON_ERR;

	return(1);
}

int DbFetchGnrlDrugNotesSelectCursor85 (TGnrlDrugNotesRecord85 *Ptr,int *LastDate,int *LastTime )
{
	EXEC SQL BEGIN DECLARE SECTION;
		TGnrlDrugNotesRecord85 *GnrlDrugNotesRec	=	Ptr;
		int Date_sel,Time_sel;
	EXEC SQL END DECLARE SECTION;

	EXEC SQL
		Fetch	GnrlDrugNotesCur85	Into
										:GnrlDrugNotesRec->GnrlDrugNoteType,
										:GnrlDrugNotesRec->GnrlDrugNoteCode,
										:GnrlDrugNotesRec->gdn_long_note,
										:GnrlDrugNotesRec->gdn_category,
										:GnrlDrugNotesRec->gdn_sex,
										:GnrlDrugNotesRec->gdn_from_age,
										:GnrlDrugNotesRec->gdn_to_age,
										:GnrlDrugNotesRec->gdn_seq_num,
										:GnrlDrugNotesRec->gdn_connect_type,
										:GnrlDrugNotesRec->gdn_connect_desc,
										:GnrlDrugNotesRec->gdn_severity,
										:GnrlDrugNotesRec->gdn_sensitv_type,
										:GnrlDrugNotesRec->gdn_sensitv_desc,
										:GnrlDrugNotesRec->DelFlag,
										:Date_sel,
										:Time_sel;

	JUMP_ON_ERR;
	*LastDate = Date_sel;
	*LastTime = Time_sel;

	if ( SQLERR_error_test() )
	{
		return (0);
	}
	else
	{
		return(1);
	}
}

int DbCloseGnrlDrugNotesSelectCursor85 (void)
{
	EXEC SQL
		Close	GnrlDrugNotesCur85;

	if ( SQLERR_error_test() )
	{
		return (0);
	}
	else
	{
		return(1);
	}
}



int	DbOpenDrugNotesSelectCursor85 (TDateYYYYMMDD	LastDate,
								   TTimeHHMMSS		LastTime)
{
	EXEC SQL BEGIN DECLARE SECTION;
		TDateYYYYMMDD	LastUpdateDate	=	LastDate;
		TTimeHHMMSS		LastUpdateTime	=	LastTime;
		TTimeHHMMSS		NowTime;
		TDateYYYYMMDD	NowDate;
	EXEC SQL END DECLARE SECTION;


	NowDate = GetDate();
	NowTime = GetTime();

	// DonR 08Jan2004: Fixed date/time selection logic.
	// DonR 30Dec2004: Clicks 8.5 doesn't need links to Message #90 - so don't send them.
	EXEC SQL
		DECLARE DrugNotesCursor85 CURSOR FOR

		SELECT	largo_code,
				gnrldrugnotecode,
				del_flg,
				update_date,
				update_time

		FROM	DrugNotes

		WHERE	((update_date > :LastUpdateDate)	OR
				 (update_date = :LastUpdateDate	AND update_time >= :LastUpdateTime))	AND

				((update_date < :NowDate)			OR
				 (update_date = :NowDate		AND update_time <  :NowTime))			AND

				gnrldrugnotecode <> 90	/* DonR 30Dec2004 */

		ORDER BY	update_date,
					update_time,
					del_flg DESC,
					largo_code,
					gnrldrugnotecode;


	EXEC SQL
		OPEN DrugNotesCursor85;

	JUMP_ON_ERR;

	return(1);
}

int DbFetchDrugNotesSelectCursor85 (TDrugNotesRecord85 *Ptr,int *LastDate,int *LastTime )
{
	EXEC SQL BEGIN DECLARE SECTION;
		TDrugNotesRecord85 *DrugNotesRec	=	Ptr;
		int Date_sel,Time_sel;
	EXEC SQL END DECLARE SECTION;

	EXEC SQL
		Fetch	DrugNotesCursor85	Into
										:DrugNotesRec->DrugCode,
										:DrugNotesRec->GnrlDrugNoteCode,
										:DrugNotesRec->DelFlag,
										:Date_sel,
										:Time_sel;

	JUMP_ON_ERR;
	*LastDate = Date_sel;
	*LastTime = Time_sel;

	if ( SQLERR_error_test() )
	{
		return (0);
	}
	else
	{
		return(1);
	}
}

int DbCloseDrugNotesSelectCursor85 (void)
{
	EXEC SQL
		Close	DrugNotesCursor85;

	if ( SQLERR_error_test() )
	{
		return (0);
	}
	else
	{
		return(1);
	}
}



int	DbOpenEconomyPriSelectCursor85 (TDateYYYYMMDD	LastDate,
									TTimeHHMMSS		LastTime)
{
	EXEC SQL BEGIN DECLARE SECTION;
		TDateYYYYMMDD	LastUpdateDate	=	LastDate;
		TTimeHHMMSS		LastUpdateTime	=	LastTime;
		TTimeHHMMSS		NowTime;
		TDateYYYYMMDD	NowDate;
	EXEC SQL END DECLARE SECTION;


	NowDate = GetDate();
	NowTime = GetTime();


	// DonR 08Jan2004: Fixed date/time selection logic.
	// DonR 22Sep2005: Yulia remarked out the select on system_code below on
	// 23 September 2004, in Version 166 of the module. The change was required
	// because in Clicks 8.5 (and later) doctors get to know about all economypri
	// stuff.
	EXEC SQL
		DECLARE	EconomyPriCursor85 CURSOR FOR

		SELECT	drug_group,
				group_code,
				group_nbr,
				item_seq,
				largo_code,
				del_flg,
				update_date,
				update_time,
				received_date,
				received_time,
				received_seq

		FROM	EconomyPri

		WHERE	((update_date > :LastUpdateDate)	OR
				 (update_date = :LastUpdateDate	AND update_time >= :LastUpdateTime))	AND

				((update_date < :NowDate)			OR
				 (update_date = :NowDate		AND update_time <  :NowTime))	/*		AND

				(system_code = '0' OR system_code = '2')*/

		ORDER BY	received_date,
					received_time,
					received_seq;


	EXEC SQL
		OPEN EconomyPriCursor85;

	JUMP_ON_ERR;

	return(1);
}

int DbFetchEconomyPriSelectCursor85 (TEconomyPriRecord85 *Ptr,int *LastDate,int *LastTime)
{
	EXEC SQL BEGIN DECLARE SECTION;
		TEconomyPriRecord85 *EconomyPriRec	=	Ptr;
		int		Date_sel,Time_sel;
		int		RcvDate, RcvTime, RcvSeq;
	EXEC SQL END DECLARE SECTION;

	EXEC SQL
		Fetch	EconomyPriCursor85	Into
										:EconomyPriRec->DrugGroup,
										:EconomyPriRec->GroupCode,
										:EconomyPriRec->GroupNbr,
										:EconomyPriRec->ItemSeq,
										:EconomyPriRec->DrugCode,
										:EconomyPriRec->DelFlag,
										:Date_sel,
										:Time_sel,
										:RcvDate,
										:RcvTime,
										:RcvSeq;

	JUMP_ON_ERR;

	*LastDate = Date_sel;
	*LastTime = Time_sel;

	if (SQLERR_error_test ())
	{
		return (0);
	}
	else
	{
		return (1);
	}
}

int DbCloseEconomyPriSelectCursor85 (void)
{
	EXEC SQL
		Close	EconomyPriCursor85;
	if ( SQLERR_error_test() )
	{
		return (0);
	}
	else
	{
		return(1);
	}
}



int	DbOpenClicksDiscountSelectCursor (TDateYYYYMMDD	LastDate,
									  TTimeHHMMSS	LastTime)
{
	EXEC SQL BEGIN DECLARE SECTION;
		TDateYYYYMMDD	LastUpdateDate	=	LastDate;
		TTimeHHMMSS		LastUpdateTime	=	LastTime;
		TTimeHHMMSS		NowTime;
		TDateYYYYMMDD	NowDate;
	EXEC SQL END DECLARE SECTION;


	NowDate = GetDate();
	NowTime = GetTime();

	// DonR 08Jan2004: Fixed date/time selection logic.
	EXEC SQL
		DECLARE ClicksDiscCursor CURSOR FOR

		SELECT	discount_code,		show2spec,
				show2nonspec,		show_needs_ishur,
				ptn_spec_basic,		ptn_nonspec_basic,
				ptn_spec_keren,		ptn_nonspec_keren,
				spec_msg,			nonspec_msg,
				del_flg,			update_date,
				update_time

		FROM	clicks_discount

		WHERE	((update_date > :LastUpdateDate)	OR
				 (update_date = :LastUpdateDate	AND update_time >= :LastUpdateTime))	AND

				((update_date < :NowDate)			OR
				 (update_date = :NowDate		AND update_time <  :NowTime))

		ORDER BY update_date, update_time, del_flg DESC;


	EXEC SQL
		OPEN ClicksDiscCursor;

	JUMP_ON_ERR;

	return(1);
}

int DbFetchClicksDiscountSelectCursor( TClicksDiscountRecord	*Ptr,int *LastDate,int *LastTime )
{
	EXEC SQL BEGIN DECLARE SECTION;
		TClicksDiscountRecord *ClicksDiscountRecord	=	Ptr;
		int Date_sel,Time_sel;
	EXEC SQL END DECLARE SECTION;

	EXEC SQL
		Fetch	ClicksDiscCursor	Into
										:ClicksDiscountRecord->discount_code,
										:ClicksDiscountRecord->show2spec,
										:ClicksDiscountRecord->show2nonspec,
										:ClicksDiscountRecord->show_needs_ishur,
										:ClicksDiscountRecord->ptn_spec_basic,
										:ClicksDiscountRecord->ptn_nonspec_basic,
										:ClicksDiscountRecord->ptn_spec_keren,
										:ClicksDiscountRecord->ptn_nonspec_keren,
										:ClicksDiscountRecord->spec_msg,
										:ClicksDiscountRecord->nonspec_msg,
										:ClicksDiscountRecord->del_flg,
										:Date_sel,
										:Time_sel;

	JUMP_ON_ERR;
	*LastDate = Date_sel;
	*LastTime = Time_sel;
	if ( SQLERR_error_test() )
	{
		return (0);
	}
	else
	{
		return(1);
	}
}

int DbCloseClicksDiscountSelectCursor(void)
{
	EXEC SQL
		Close	ClicksDiscCursor;

	if ( SQLERR_error_test() )
	{
		return (0);
	}
	else
	{
		return(1);
	}
}



int	DbOpenUseInstructSelectCursor (TDateYYYYMMDD	LastDate,
								   TTimeHHMMSS		LastTime)
{
	EXEC SQL BEGIN DECLARE SECTION;
		TDateYYYYMMDD	LastUpdateDate	=	LastDate;
		TTimeHHMMSS		LastUpdateTime	=	LastTime;
		TTimeHHMMSS		NowTime;
		TDateYYYYMMDD	NowDate;
	EXEC SQL END DECLARE SECTION;


	NowDate = GetDate();
	NowTime = GetTime();

	// DonR 08Jan2004: Fixed date/time selection logic.
	EXEC SQL
		DECLARE UseInstructCursor CURSOR FOR

		SELECT	shape_num,			shape_code,
				shape_desc,			concentration_flag,
				inst_code,			inst_msg,
				inst_seq,			calc_op_flag,
				unit_code,			unit_seq,
				unit_name,			unit_desc,
				open_od_window,		total_unit_name,
				round_units_flag,	del_flg,
				update_date,		update_time

		FROM	usage_instruct

		WHERE	((update_date > :LastUpdateDate)	OR
				 (update_date = :LastUpdateDate	AND update_time >= :LastUpdateTime))	AND

				((update_date < :NowDate)			OR
				 (update_date = :NowDate		AND update_time <  :NowTime))

		ORDER BY update_date, update_time, del_flg DESC;


	EXEC SQL
		OPEN UseInstructCursor;

	JUMP_ON_ERR;

	return(1);
}

int DbFetchUseInstructSelectCursor( TUseInstructRecord	*Ptr,int *LastDate,int *LastTime )
{
	EXEC SQL BEGIN DECLARE SECTION;
		TUseInstructRecord *UseInstructRecord	=	Ptr;
		int Date_sel,Time_sel;
	EXEC SQL END DECLARE SECTION;

	EXEC SQL
		Fetch	UseInstructCursor	Into
										:UseInstructRecord->shape_num,
										:UseInstructRecord->shape_code,
										:UseInstructRecord->shape_desc,
										:UseInstructRecord->concentration_flag,
										:UseInstructRecord->inst_code,
										:UseInstructRecord->inst_msg,
										:UseInstructRecord->inst_seq,
										:UseInstructRecord->calc_op_flag,
										:UseInstructRecord->unit_code,
										:UseInstructRecord->unit_seq,
										:UseInstructRecord->unit_name,
										:UseInstructRecord->unit_desc,
										:UseInstructRecord->open_od_window,
										:UseInstructRecord->total_unit_name,
										:UseInstructRecord->round_units_flag,
										:UseInstructRecord->DelFlag,
										:Date_sel,
										:Time_sel;

	JUMP_ON_ERR;
	*LastDate = Date_sel;
	*LastTime = Time_sel;
	if ( SQLERR_error_test() )
	{
		return (0);
	}
	else
	{
		return(1);
	}
}

int DbCloseUseInstructSelectCursor(void)
{
	EXEC SQL
		Close	UseInstructCursor;

	if ( SQLERR_error_test() )
	{
		return (0);
	}
	else
	{
		return(1);
	}
}



int	DbOpenDrugFormsSelectCursor (TDateYYYYMMDD	LastDate,
								 TTimeHHMMSS		LastTime)
{
	EXEC SQL BEGIN DECLARE SECTION;
		TDateYYYYMMDD	LastUpdateDate	=	LastDate;
		TTimeHHMMSS		LastUpdateTime	=	LastTime;
		TTimeHHMMSS		NowTime;
		TDateYYYYMMDD	NowDate;
	EXEC SQL END DECLARE SECTION;


	NowDate = GetDate();
	NowTime = GetTime();

	// DonR 08Jan2004: Fixed date/time selection logic.
	EXEC SQL
		DECLARE DrugFormsCursor CURSOR FOR

		SELECT	largo_code,			form_number,
				del_flg,			update_date,
				update_time

		FROM	drug_forms

		WHERE	((update_date > :LastUpdateDate)	OR
				 (update_date = :LastUpdateDate	AND update_time >= :LastUpdateTime))	AND

				((update_date < :NowDate)			OR
				 (update_date = :NowDate		AND update_time <  :NowTime))

		ORDER BY update_date, update_time, del_flg DESC;


	EXEC SQL
		OPEN DrugFormsCursor;

	JUMP_ON_ERR;

	return(1);
}

int DbFetchDrugFormsSelectCursor( TDrugFormsRecord	*Ptr,int *LastDate,int *LastTime )
{
	EXEC SQL BEGIN DECLARE SECTION;
		TDrugFormsRecord *DrugFormsRec	=	Ptr;
		int Date_sel,Time_sel;
	EXEC SQL END DECLARE SECTION;

	EXEC SQL
		Fetch DrugFormsCursor Into
									:DrugFormsRec->largo_code,
									:DrugFormsRec->form_number,
									:DrugFormsRec->DelFlag,
									:Date_sel,
									:Time_sel;

	JUMP_ON_ERR;
	*LastDate = Date_sel;
	*LastTime = Time_sel;
	if ( SQLERR_error_test() )
	{
		return (0);
	}
	else
	{
		return(1);
	}
}

int DbCloseDrugFormsSelectCursor(void)
{
	EXEC SQL
		Close	DrugFormsCursor;

	if ( SQLERR_error_test() )
	{
		return (0);
	}
	else
	{
		return(1);
	}
}



int	DbOpenTreatmentGrpSelectCursor (TDateYYYYMMDD	LastDate,
									TTimeHHMMSS		LastTime)
{
	EXEC SQL BEGIN DECLARE SECTION;
		TDateYYYYMMDD	LastUpdateDate	=	LastDate;
		TTimeHHMMSS		LastUpdateTime	=	LastTime;
		TTimeHHMMSS		NowTime;
		TDateYYYYMMDD	NowDate;
	EXEC SQL END DECLARE SECTION;


	NowDate = GetDate();
	NowTime = GetTime();

	// DonR 08Jan2004: Fixed date/time selection logic.
	EXEC SQL
		DECLARE TreatmentGrpCursor CURSOR FOR

		SELECT	pharmacology_code,	treatment_group,
				treat_grp_desc,		presc_valid_days,
				del_flg,			update_date,
				update_time

		FROM	treatment_group

		WHERE	((update_date > :LastUpdateDate)	OR
				 (update_date = :LastUpdateDate	AND update_time >= :LastUpdateTime))	AND

				((update_date < :NowDate)			OR
				 (update_date = :NowDate		AND update_time <  :NowTime))

		ORDER BY update_date, update_time, del_flg DESC;


	EXEC SQL
		OPEN TreatmentGrpCursor;

	JUMP_ON_ERR;

	return(1);
}

int DbFetchTreatmentGrpSelectCursor( TTreatmentGrpRecord	*Ptr,int *LastDate,int *LastTime )
{
	EXEC SQL BEGIN DECLARE SECTION;
		TTreatmentGrpRecord *TreatmentGrpRec	=	Ptr;
		int Date_sel,Time_sel;
	EXEC SQL END DECLARE SECTION;

	EXEC SQL
		Fetch TreatmentGrpCursor Into
										:TreatmentGrpRec->pharmacology_code,
										:TreatmentGrpRec->treatment_group,
										:TreatmentGrpRec->treat_grp_desc,
										:TreatmentGrpRec->presc_valid_days,
										:TreatmentGrpRec->DelFlag,
										:Date_sel,
										:Time_sel;

	JUMP_ON_ERR;
	*LastDate = Date_sel;
	*LastTime = Time_sel;
	if ( SQLERR_error_test() )
	{
		return (0);
	}
	else
	{
		return(1);
	}
}

int DbCloseTreatmentGrpSelectCursor(void)
{
	EXEC SQL
		Close	TreatmentGrpCursor;

	if ( SQLERR_error_test() )
	{
		return (0);
	}
	else
	{
		return(1);
	}
}



int	DbOpenPrescPeriodSelectCursor (TDateYYYYMMDD	LastDate,
								   TTimeHHMMSS		LastTime)
{
	EXEC SQL BEGIN DECLARE SECTION;
		TDateYYYYMMDD	LastUpdateDate	=	LastDate;
		TTimeHHMMSS		LastUpdateTime	=	LastTime;
		TTimeHHMMSS		NowTime;
		TDateYYYYMMDD	NowDate;
	EXEC SQL END DECLARE SECTION;


	NowDate = GetDate();
	NowTime = GetTime();

	// DonR 08Jan2004: Fixed date/time selection logic.
	EXEC SQL
		DECLARE PrescPeriodCursor CURSOR FOR

		SELECT	presc_type,			month,
				from_day,			to_day,
				del_flg,			update_date,
				update_time

		FROM	presc_period

		WHERE	((update_date > :LastUpdateDate)	OR
				 (update_date = :LastUpdateDate	AND update_time >= :LastUpdateTime))	AND

				((update_date < :NowDate)			OR
				 (update_date = :NowDate		AND update_time <  :NowTime))

		ORDER BY update_date, update_time, del_flg DESC;


	EXEC SQL
		OPEN PrescPeriodCursor;

	JUMP_ON_ERR;

	return(1);
}

int DbFetchPrescPeriodSelectCursor( TPrescPeriodRecord	*Ptr,int *LastDate,int *LastTime )
{
	EXEC SQL BEGIN DECLARE SECTION;
		TPrescPeriodRecord *PrescPeriodRec	=	Ptr;
		int Date_sel,Time_sel;
	EXEC SQL END DECLARE SECTION;

	EXEC SQL
		Fetch PrescPeriodCursor	Into
										:PrescPeriodRec->presc_type,
										:PrescPeriodRec->month,
										:PrescPeriodRec->from_day,
										:PrescPeriodRec->to_day,
										:PrescPeriodRec->DelFlag,
										:Date_sel,
										:Time_sel;

	JUMP_ON_ERR;
	*LastDate = Date_sel;
	*LastTime = Time_sel;
	if ( SQLERR_error_test() )
	{
		return (0);
	}
	else
	{
		return(1);
	}
}

int DbClosePrescPeriodSelectCursor(void)
{
	EXEC SQL
		Close	PrescPeriodCursor;

	if ( SQLERR_error_test() )
	{
		return (0);
	}
	else
	{
		return(1);
	}
}



int	DbOpenDrugExtensionSelectCursor (TDateYYYYMMDD	LastDate,
									 TTimeHHMMSS		LastTime)
{
	EXEC SQL BEGIN DECLARE SECTION;
		TDateYYYYMMDD	LastUpdateDate	=	LastDate;
		TTimeHHMMSS		LastUpdateTime	=	LastTime;
		TTimeHHMMSS		NowTime;
		TDateYYYYMMDD	NowDate;
	EXEC SQL END DECLARE SECTION;


	NowDate = GetDate();
	NowTime = GetTime();

	// DonR 08Jan2004: Fixed date/time selection logic.
	EXEC SQL
		DECLARE DrugExtCursor CURSOR FOR

		SELECT	largo_code,			extension_largo_co,
				confirm_authority_,	conf_auth_desc,
				from_age,			to_age,
				member_price_prcnt,	keren_price_prcnt,
				magen_price_prcnt,	max_op,
				max_units,			max_amount_duratio,
				permission_type,	no_presc_sale_flag,
				fixed_price,		fixed_price_keren,
				fixed_price_magen,	ivf_flag,
				refill_period,		rule_name,
				rule_number,		in_health_basket,
				treatment_category,	sex,
				needs_29_g,			del_flg_c,
				member_price_code,	keren_price_code,
				magen_price_code,	update_date,
				update_time

		FROM	drug_extn_doc

		WHERE	((update_date > :LastUpdateDate)	OR
				 (update_date = :LastUpdateDate	AND update_time >= :LastUpdateTime))

		  AND	((update_date < :NowDate)			OR
				 (update_date = :NowDate		AND update_time <  :NowTime))

		  AND	send_and_use	<>  0

		ORDER BY update_date, update_time, del_flg_c DESC;


	EXEC SQL
		OPEN DrugExtCursor;

	JUMP_ON_ERR;

	return(1);
}

int DbFetchDrugExtensionSelectCursor( TDrugExtensionRecord	*Ptr,int *LastDate,int *LastTime )
{
	EXEC SQL BEGIN DECLARE SECTION;
		TDrugExtensionRecord *DrugExtensionRec	=	Ptr;
		int Date_sel,Time_sel;
	EXEC SQL END DECLARE SECTION;

	EXEC SQL
		Fetch DrugExtCursor Into
								:DrugExtensionRec->largo_code,
								:DrugExtensionRec->extension_largo_co,
								:DrugExtensionRec->confirm_authority,
								:DrugExtensionRec->conf_auth_desc,
								:DrugExtensionRec->from_age,
								:DrugExtensionRec->to_age,
								:DrugExtensionRec->member_price_prcnt,
								:DrugExtensionRec->keren_price_prcnt,
								:DrugExtensionRec->magen_price_prcnt,
								:DrugExtensionRec->max_op,
								:DrugExtensionRec->max_units,
								:DrugExtensionRec->max_amount_duratio,
								:DrugExtensionRec->permission_type,
								:DrugExtensionRec->no_presc_sale_flag,
								:DrugExtensionRec->fixed_price,
								:DrugExtensionRec->fixed_price_keren,
								:DrugExtensionRec->fixed_price_magen,
								:DrugExtensionRec->ivf_flag,
								:DrugExtensionRec->refill_period,
								:DrugExtensionRec->rule_name,
								:DrugExtensionRec->rule_number,
								:DrugExtensionRec->in_health_basket,
								:DrugExtensionRec->treatment_category,
								:DrugExtensionRec->sex,
								:DrugExtensionRec->needs_29_g,
								:DrugExtensionRec->DelFlag,
								:DrugExtensionRec->member_price_code,
								:DrugExtensionRec->keren_price_code,
								:DrugExtensionRec->magen_price_code,
								:Date_sel,
								:Time_sel;

	// Reserved for expansion.
	strcpy (DrugExtensionRec->reserved_char, "               ");
	DrugExtensionRec->reserved_long = 0L;

	JUMP_ON_ERR;
	*LastDate = Date_sel;
	*LastTime = Time_sel;
	if ( SQLERR_error_test() )
	{
		return (0);
	}
	else
	{
		return(1);
	}
}

int DbCloseDrugExtensionSelectCursor(void)
{
	EXEC SQL
		Close	DrugExtCursor;

	if ( SQLERR_error_test() )
	{
		return (0);
	}
	else
	{
		return(1);
	}
}



// DonR 12Jun2004: Add four new transactions for real-time query and
// ishur insertion.
DbCheckForSpecialistPr (long	DocTZ_in,
						long	MemberID_in,
						long	Largo_in,
						short	*SpecialistPrStatus_out,
						short	*SpecialtyCode_out,
						long	*ParticipationPercent_out)
{
	int		retval;

	EXEC SQL BEGIN DECLARE SECTION;
		long	DocTZ		= DocTZ_in;
		long	MemberID	= MemberID_in;
		long	Largo		= Largo_in;
		short	SpecialtyCode;
		short	ParticipationPercent;
	EXEC SQL END DECLARE SECTION;


	EXEC SQL
		DECLARE Trn90Cursor CURSOR FOR

		SELECT	ds.speciality_code,
				pr.member_price_prcnt

		FROM	prescr_wr_new		AS pw,
				doctor_speciality	AS ds,
				spclty_largo_prcnt	AS lp,
				member_price		AS pr

		WHERE	pw.idnumber				= :MemberID
		  AND	pw.medicinecode			= :Largo
		  AND	ds.doctor_id			= pw.contractor
		  AND	lp.largo_code			= :Largo
		  AND	lp.speciality_code		= ds.speciality_code
		  AND	((pr.member_price_code	= lp.basic_price_code AND lp.basic_price_code > 0) OR
				 (pr.member_price_code	= lp.kesef_price_code AND lp.kesef_price_code > 0) OR
				 (pr.member_price_code	= lp.zahav_price_code AND lp.zahav_price_code > 0))

		ORDER BY member_price_prcnt;


	retval						= 0;
	*SpecialtyCode_out			= 0;
	*ParticipationPercent_out	= 0;

	EXEC SQL
		OPEN Trn90Cursor;

	JUMP_ON_ERR;

	EXEC SQL
		FETCH	Trn90Cursor
		INTO	:SpecialtyCode,
				:ParticipationPercent;

	if (SQLCODE == 0)
	{
		retval						= 1;					// Found something!
		*SpecialtyCode_out			= SpecialtyCode / 1000;	// Specialty Group.
		*ParticipationPercent_out	= (long)ParticipationPercent;
	}
	else
	{
		if (SQLERR_error_test ())
		{
			if (JumpMode)
				longjmp (RollbackLocation, SQL_ERR_JUMP);
		}
	}

	EXEC SQL
		CLOSE	Trn90Cursor;

	return (retval);
}



DbInsertSpecLetterSeenIshur (long	Contractor_in,
							 short	MemberIDCode_in,
							 long	MemberID_in,
							 long	Largo_in,
							 long	SpecialistID_in,
							 char	*SpecName_in,
							 short	SpecialtyGroupCode_in,
							 long	DateOfLetter_in,
							 short	*RequestStatus_out)
{
	EXEC SQL BEGIN DECLARE SECTION;
		long	Contractor			= Contractor_in;
		short	MemberIDCode		= MemberIDCode_in;
		long	MemberID			= MemberID_in;
		long	Largo				= Largo_in;
		long	SpecialistID		= SpecialistID_in;
		char	*SpecName			= SpecName_in;
		short	SpecialtyGroupCode	= SpecialtyGroupCode_in;
		long	DateOfLetter		= DateOfLetter_in;
		long	sysdate;
		long	systime;
	EXEC SQL END DECLARE SECTION;


	sysdate = GetDate ();
	systime = GetTime ();

	EXEC SQL
		INSERT INTO spec_letter_seen
										(
											contractor,
											member_id_code,
											member_id,
											largo_code,
											specialist_id,
											spec_name,
											spec_group_code,
											date_of_letter,
											update_date,
											update_time,
											reported_to_as400
										)
								values	(
											:Contractor,
											:MemberIDCode,
											:MemberID,
											:Largo,
											:SpecialistID,
											:SpecName,
											:SpecialtyGroupCode,
											:DateOfLetter,
											:sysdate,
											:systime,
											0
										);

	if ((SQLERR_code_cmp (SQLERR_ok) == MAC_TRUE) || (SQLCODE == INF_NOT_UNIQUE_INDEX))
	{
		*RequestStatus_out	= 1;
	}
	else
	{
		*RequestStatus_out	= 0;
	}


	return (1);
}



DbCheckForAS400Ishur (long	DocTZ_in,
					  long	MemberID_in,
					  long	Largo_in,
					  short	*IshurStatus_out,
					  long	*IshurOP_out,
					  long	*IshurUnits_out,
					  short	*IshurRenewFreq_out,
					  long	*PharmacyCode_out,
					  short	*PrivPharmSale_out,
					  long	*StopUseDate_out,
					  short	*PtnPercent_out,
					  long	*FixedPrice_out,
					  long	*MessageCode_out)
{
	EXEC SQL BEGIN DECLARE SECTION;
		long	DocTZ		= DocTZ_in;
		long	MemberID	= MemberID_in;
		long	Largo		= Largo_in;
		long	v_op_in_dose;
		long	v_units_in_dose;
		short	v_dose_renew_freq;
		long	v_pharmacy_code;
		short	v_priv_pharm_sale_ok;
		long	v_stop_use_date;
		short	v_member_price_code;
		long	v_const_sum_to_pay;
		long	v_special_presc_num;
		long	SystemDate;
	EXEC SQL END DECLARE SECTION;

	SystemDate = GetDate();

	*IshurStatus_out = 0; // Default: Ishur not found.

	EXEC SQL
		SELECT	op_in_dose,				units_in_dose,			dose_renew_freq,
				pharmacy_code,			priv_pharm_sale_ok,		stop_use_date,
				member_price_code,		const_sum_to_pay,		largo_code,
				member_id,				special_presc_num
				
		INTO	:v_op_in_dose,			:v_units_in_dose,		:v_dose_renew_freq,
				:v_pharmacy_code,		:v_priv_pharm_sale_ok,	:v_stop_use_date,
				:v_member_price_code,	:v_const_sum_to_pay,	:Largo,
				:MemberID,				:v_special_presc_num

		FROM	special_prescs

		WHERE	member_id			=  :MemberID
		  AND	largo_code			=  :Largo
		  AND	treatment_start		<= :SystemDate
		  AND	del_flg				=  :ROW_NOT_DELETED;

	if (SQLERR_code_cmp (SQLERR_ok) == MAC_TRUE)
	{
		if (v_stop_use_date < SystemDate)
			*IshurStatus_out = 2;	// Expired Ishur.
		else
			*IshurStatus_out = 1;	// Ishur exists.

		// TBD: Add logic for "refused" ishurim.

		// Assign the rest of the "stuff" to return variables.
		*IshurOP_out			= v_op_in_dose;
		*IshurUnits_out			= v_units_in_dose;
		*IshurRenewFreq_out		= v_dose_renew_freq;
		*PharmacyCode_out		= v_pharmacy_code;
		*PrivPharmSale_out		= v_priv_pharm_sale_ok;
		*StopUseDate_out		= v_stop_use_date;
		*PtnPercent_out			= v_member_price_code;
		*FixedPrice_out			= v_const_sum_to_pay;
		*MessageCode_out		= 0;	// TBD: What is this field really for?
	}

	return (1);
}



DbInsertInteractionOkIshur (long	Contractor_in,
							short	MemberIDCode_in,
							long	MemberID_in,
							long	Largo_1_in,
							long	Largo_2_in,
							long	DocPrDate_in,
							long	DocPrID_in,
							short	*RequestStatus_out)
{
	EXEC SQL BEGIN DECLARE SECTION;
		long	Contractor			= Contractor_in;
		short	MemberIDCode		= MemberIDCode_in;
		long	MemberID			= MemberID_in;
		long	Largo_1				= Largo_1_in;
		long	Largo_2				= Largo_2_in;
		long	DocPrDate			= DocPrDate_in;
		long	DocPrID				= DocPrID_in;
		long	sysdate;
		long	systime;
	EXEC SQL END DECLARE SECTION;


	sysdate = GetDate ();
	systime = GetTime ();

	EXEC SQL
		INSERT INTO doc_interact_ishur
										(
											contractor,
											member_id_code,
											member_id,
											largo_code_1,
											largo_code_2,
											doc_pr_date,
											doc_pr_id,
											update_date,
											update_time,
											reported_to_as400
										)
								values	(
											:Contractor,
											:MemberIDCode,
											:MemberID,
											:Largo_1,
											:Largo_2,
											:DocPrDate,
											:DocPrID,
											:sysdate,
											:systime,
											0
										);

	if ((SQLERR_code_cmp (SQLERR_ok) == MAC_TRUE) || (SQLCODE == INF_NOT_UNIQUE_INDEX))
	{
		*RequestStatus_out	= 1;
	}
	else
	{
		*RequestStatus_out	= 0;
	}


	return (1);
}
// DonR 29Mar2004 New transactions for Clicks 8.5 end.



//Yulia 12.99 37-38

int	DbOpenLabResOtherListCursor( TMemberId	*MemberIdParam)
{
EXEC SQL BEGIN DECLARE SECTION;
	int	nMaxRrn;
	TMemberId	MemberId ;
EXEC SQL END DECLARE SECTION;

//	$TMemberId	MemberId    = *MemberIdParam;
//	TMemberId	MemberId ;
	MemberId    = *MemberIdParam;

	EXEC SQL SELECT list_rrn INTO :nMaxRrn FROM forwres1;

	if( SQLERR_code_cmp( SQLERR_ok ) == MAC_FALS )	return 0;

//GerrLogFnameMini("test37",GerrId,  "Max rrn selected: %d id [%d]", nMaxRrn,MemberId.Number );
	


	EXEC SQL Declare	LabResOtherLCurs Cursor	for
	select	distinct
		Contractor,	    Contr_name,	    ReqDate,
		Contr_prof_Descr,   grouplist,	    ReqId_main,
		Profession
	from	LabResList
	where	
		IdNumber	= :MemberId.Number	and 
		IdCode		= :MemberId.Code	and
		rrn		<=:nMaxRrn
		Order	By 	Reqdate desc;

	// DonR 23Feb2005: Try avoiding record-locking errors by performing "dirty reads".
	EXEC SQL
		SET ISOLATION TO DIRTY READ;

	EXEC SQL Open	LabResOtherLCurs;
//	GerrLogFnameMini("test37",GerrId,  "2: sqlerr=[%d] sqlca.sqlerrd[2]=[%d]",sqlca.sqlcode,sqlca.sqlerrd[2]);
	JUMP_ON_ERR;

	return(1);
}


int DbFetchLabResOtherListCursor( TLabContrOtherListReq	*Ptr,int *Prof_par)
{
	
	EXEC SQL BEGIN DECLARE SECTION;
	TLabContrOtherListReq  	    *Lab    =   Ptr;
	int Prof_sel ;
	EXEC SQL END DECLARE SECTION;

	EXEC SQL Fetch	LabResOtherLCurs	Into
		:Lab->Contractor,	
		:Lab->ContrName,
		:Lab->ReqDate,
		:Lab->ContrProfDescr,	
		:Lab->GroupList,	:Lab->ReqIdMain	,
		:Prof_sel;

//	GerrLogFnameMini("test37",GerrId,"point before3:sqlcode[%d]",sqlca.sqlcode);	

	*Prof_par = Prof_sel;

	JUMP_ON_ERR;

	return(1);
}

int DbCloseLabResOtherListCursor(void)
{
	// DonR 23Feb2005: Return to normal isolation level.
	EXEC SQL
		SET ISOLATION TO COMMITTED READ;

	EXEC SQL Close	LabResOtherLCurs;
	if ( SQLERR_error_test() )
	{
	return (0);
	}
	else
	{
	return(1);
	}
}


//end 37-38

/* trans 38 12.99 */
int	DbOpenRentgenCursor( 	TIdNumber	ContractorParam,
							TMemberId	*MemberIdParam)
{
EXEC SQL BEGIN DECLARE SECTION;
    TIdNumber	Contractor	=   ContractorParam;
    TMemberId	MemberIdFirst,MemberIdLast;
EXEC SQL END DECLARE SECTION;	

//GerrLogReturn( GerrId, "19 contractor [%d]", Contractor);


	if(	MemberIdParam->Number	!=	0	)
	{
		MemberIdFirst	=	*MemberIdParam;
		MemberIdLast	=	*MemberIdParam;
	} 
	else
	{
		MemberIdFirst.Number	=	0;
		MemberIdLast.Number	=	0x7fffffffl;
		MemberIdFirst.Code	=	0;
		MemberIdLast.Code	=	0x7fff;
	}

	
	EXEC SQL Declare	RentgenCursor Cursor	for
	select
		contractor,		termid,
		/*		contracttype,*/	idnumber,	idcode,
		answerdate,		answertime,		answercontr,
		rentgendate ,	rentgentime ,	typeresult ,rescount,
		resname1,		resname2,		resname3,	resname4,
		answerflg,		patalogyflg,	rentgenid,	resultsize,
		result        
	from	RentgenRes
	where	
		Contractor	=	:Contractor 				and
		IdNumber	between :MemberIdFirst.Number	
					and 	:MemberIdLast.Number	and
		IdCode		between :MemberIdFirst.Code	
					and		:MemberIdLast.Code
		order by idnumber		;

	EXEC SQL Open	RentgenCursor;

//	JUMP_ON_ERR;
    if( SQLERR_code_cmp( SQLERR_not_found  ) == MAC_TRUE )   
	{
		return (0);
	}
	
	if ( SQLERR_error_test() )
	{
		return (0);
	}
	else
	{
		return(1);
	}


}


int DbFetchRentgenCursor( TRentgenRecord	*Ptr)
{
EXEC SQL BEGIN DECLARE SECTION;
	TRentgenRecord *RentgenR	=	Ptr;
EXEC SQL END DECLARE SECTION;

	EXEC SQL Fetch	RentgenCursor	Into
		:RentgenR->Contractor,
		:RentgenR->TermId,
		/*	:RentgenR->ContractType,*/
		:RentgenR->MemberId.Number,
		:RentgenR->MemberId.Code,
		:RentgenR->AnswerDate,
		:RentgenR->AnswerTime,	:RentgenR->AnswerContr,
		:RentgenR->RentgenDate,	:RentgenR->RentgenTime,	
		:RentgenR->TypeResult,	:RentgenR->ResCount,
		:RentgenR->ResName1,	:RentgenR->ResName2,
		:RentgenR->ResName3,	:RentgenR->ResName4,
		:RentgenR->AnswerFlg,	:RentgenR->PatalogyFlg,
		:RentgenR->RentgenId,	:RentgenR->ResultSize,
		:RentgenR->Result;

//	JUMP_ON_ERR;
    if( SQLERR_code_cmp( SQLERR_not_found  ) == MAC_TRUE )   
	{
		return (0);
	}
	
	if ( SQLERR_error_test() )
	{
		return (0);
	}
	else
	{
		return(1);
	}
}

int DbCloseRentgenCursor(void)
{
	EXEC SQL Close	RentgenCursor;
	if ( SQLERR_error_test() )
	{
		return (0);
	}
	else
	{
		return(1);
	}
}

/*20020619*/
int	DbOpenHospitCursor( 	TIdNumber	ContractorParam,
							TMemberId	*MemberIdParam,
							short int	FromSystem,
							short int	ToSystem)
{
EXEC SQL BEGIN DECLARE SECTION;
    TIdNumber	Contractor	=   ContractorParam;
    TMemberId	MemberIdFirst,MemberIdLast;
	short int	v_FromSystem	=	FromSystem;
	short int	v_ToSystem		=	ToSystem;

EXEC SQL END DECLARE SECTION;	

	if(	MemberIdParam->Number	!=	0	)
	{
		MemberIdFirst	=	*MemberIdParam;
		MemberIdLast	=	*MemberIdParam;
	} 
	else
	{
		MemberIdFirst.Number	=	0;
		MemberIdLast.Number	=	0x7fffffffl;
		MemberIdFirst.Code	=	0;
		MemberIdLast.Code	=	0x7fff;
	}

	
	EXEC SQL Declare	HospitCursor Cursor	for
	select
		contractor,		termid,
		idnumber,	idcode,
		hospdate,		contrName,
		nursedate ,		typeresult ,rescount,
		resname1,		hospitalnumb,
		resultsize,		result ,
		patalogyflg, /*20051229*/
		flg_system
	from	hospitalmsg
	where	
		Contractor	=	:Contractor 				and
		IdNumber	between :MemberIdFirst.Number	
					and 	:MemberIdLast.Number	and
		IdCode		between :MemberIdFirst.Code	
					and		:MemberIdLast.Code	
		and flg_system	between :v_FromSystem and :v_ToSystem
		order by idnumber;

	EXEC SQL Open	HospitCursor;

//	JUMP_ON_ERR;
    if( SQLERR_code_cmp( SQLERR_not_found  ) == MAC_TRUE )   
	{
		return (0);
	}
	
	if ( SQLERR_error_test() )
	{
		return (0);
	}
	else
	{
		return(1);
	}
}


int DbFetchHospitCursor( THospitalRecord	*Ptr)
{
EXEC SQL BEGIN DECLARE SECTION;
	THospitalRecord *Hosp	=	Ptr;
EXEC SQL END DECLARE SECTION;

	EXEC SQL Fetch	HospitCursor	Into
		:Hosp->Contractor,
		:Hosp->TermId,
		:Hosp->MemberId.Number,
		:Hosp->MemberId.Code,
		:Hosp->HospDate,
		:Hosp->ContrName,
		:Hosp->NurseDate,
		:Hosp->TypeResult,	
		:Hosp->ResCount,
		:Hosp->ResName1,
		:Hosp->HospitalNumb,
		:Hosp->ResultSize,
		:Hosp->Result,
		:Hosp->PatFlg,
		:Hosp->FlgSystem;
//		GerrLogFnameMini("hosp_log",	GerrId,	"send [%s]",Hosp->ContrName);


//	JUMP_ON_ERR;
    if( SQLERR_code_cmp( SQLERR_not_found  ) == MAC_TRUE )   
	{
		return (0);
	}
	
	if ( SQLERR_error_test() )
	{
		return (0);
	}
	else
	{
		return(1);
	}
}

int DbCloseHospitCursor(void)
{
	EXEC SQL Close	HospitCursor;
	if ( SQLERR_error_test() )
	{
		return (0);
	}
	else
	{
		return(1);
	}
}

/*20020619end*/

int	DbDeleteRentgen( TIdNumber	Contractor,
					TMemberId		*MemberId,
					TRentgenId		RentgenId)
{
	EXEC SQL BEGIN DECLARE SECTION;
	TRentgenRecord	RentgenR;
	EXEC SQL END DECLARE SECTION;

	RentgenR.Contractor	=	Contractor;
	RentgenR.MemberId	=	*MemberId;
	RentgenR.RentgenId	=	RentgenId;

	EXEC SQL Delete from RentgenRes 
	where
		Contractor	=	:RentgenR.Contractor		and
		IdNumber	=	:RentgenR.MemberId.Number	and
		IdCode		=	:RentgenR.MemberId.Code		and
		RentgenId	=	:RentgenR.RentgenId;
	/*this record from  hospitalmsg table*/
	if( !sqlca.sqlerrd[2])   /*informix value that show how many rows deleted*/
	{
		EXEC SQL Delete from hospitalmsg 
		where
			Contractor	=	:RentgenR.Contractor		and
			IdNumber	=	:RentgenR.MemberId.Number	and
			IdCode		=	:RentgenR.MemberId.Code		and
			HospitalNumb=	:RentgenR.RentgenId;

		if ( SQLERR_error_test() )
		{
			return (0);
		}
		else
		{
			return(1);
		}
	}
	if ( SQLERR_error_test() )
	{
		return (0);
	}
	else
	{
		return(1);
	}
}


// DonR 16Feb2005 begin
int	DbCheckContractorUnseenIshurim (TIdNumber Contractor_p)
{
	EXEC SQL BEGIN DECLARE SECTION;
		TIdNumber	Contractor	=	Contractor_p;
		short 	Result;
	EXEC SQL END DECLARE SECTION;

	Result = 0; // Default.

	EXEC SQL
		SELECT	COUNT(*)
		INTO	:Result
		FROM	special_prescs
		WHERE	doctor_id			= :Contractor
		  AND	country_center		> 0		/* Mercaz Ishurim only!  DonR 14Apr2005 */
		  AND	doc_has_seen_ishur	= 0;

	JUMP_ON_ERR;

	return ((Result > 0) ? 1 : 0);
} 
// DonR 16Feb2005 end


// Yulia 20050222 begin
int	DbCheckContractorPatData (TIdNumber Contractor_p,int Type_p)
{
	EXEC SQL BEGIN DECLARE SECTION;
		TIdNumber	Contractor	=	Contractor_p;
		int		v_type= Type_p;
		short 	Result;
	EXEC SQL END DECLARE SECTION;

	Result = 0; // Default.

	EXEC SQL
		SELECT	COUNT(*)
		INTO	:Result
		FROM	patlabstatus
		WHERE	contractor			= :Contractor
		AND	type					= :v_type
		AND readsign				= 0;

	JUMP_ON_ERR;
//GerrLogFnameMini("log58",GerrId,"Contractor[%d]type[%d]result[%d]",Contractor_p,Type_p,Result);


	return ((Result > 0) ? 1 : 0);
} 
// Yulia 20050222 end

int	DbCheckContractorRentgenData( 	TIdNumber	Contractor_p)
{
EXEC SQL BEGIN DECLARE SECTION;
	TIdNumber	Contractor	=	Contractor_p;
	short 	Result;
EXEC SQL END DECLARE SECTION;

	EXEC SQL select count(*) 	into 	:Result
	from	RentgenRes
	where	Contractor	=	:Contractor;
	JUMP_ON_ERR;
	if (Result == 0 )
	{
		EXEC SQL select count(*) 	into 	:Result
		from	HospitalMsg
		where	Contractor	=	:Contractor;
		JUMP_ON_ERR;

	}
	return(Result);
} 




/* end rentgen 04.05.2000 */


int	DbOpenLabResOtherSelCursor( 	TIdNumber	ContractorParam,
				        TMemberId	*MemberIdParam,
					TLabReqId	ReqIdMParam,
					int profParam)
{
EXEC SQL BEGIN DECLARE SECTION;
    TIdNumber	Contractor	=   ContractorParam;
    TMemberId	MemberId	=   *MemberIdParam;
    TLabReqId	ReqIdMainP	=   ReqIdMParam;
EXEC SQL END DECLARE SECTION;	

//	GerrLogReturn( GerrId, "38 id [%d]", Contractor);
//	printf ("\n trans38_macsql [%d]\n",profParam);
//	fflush (stdout);
/*
if (profParam == 12)  //20011113 Yulia
{*/
    EXEC SQL Declare	LabResOtherCursor Cursor	for
    select
		Contractor,	TermId,		IdCode,		IdNumber,
		LabId,		ReqId,		ReqCode,
		Route,		Result,		LowerLimit,	UpperLimit,
		Units,		ApprovalDate,	ForwardDate,	ReqDate,
		PregnantWoman,	Completed,	ReqId_Main
	from	LabResOther
	where	
	
		IdNumber	=   :MemberId.Number	and 
		IdCode		=   :MemberId.Code	and
		ReqId_Main	=   :ReqIdMainP
	Order	By 

	Contractor,IdNumber, IdCode, ReqId,		 ReqCode;
	EXEC SQL Open	LabResOtherCursor;
/*
}
else
{

	EXEC SQL Declare	LabResOtherCursor1 Cursor	for
    select
		Contractor,	TermId,		IdCode,		IdNumber,
		LabId,		ReqId,		ReqCode,
		Route,		Result,		LowerLimit,	UpperLimit,
		Units,		ApprovalDate,	ForwardDate,	ReqDate,
		PregnantWoman,	Completed,	ReqId_Main
	from	LabResOther
	where	
		IdNumber	=   :MemberId.Number	and 
		IdCode		=   :MemberId.Code	and
		ReqId_Main	=   :ReqIdMainP		and
		(		(reqcode  not in (4701,4702,4703)) or 
				(reqcode	  in (4701,4702,4703)  and result > 0)
				)
	Order	By 
		Contractor,IdNumber, IdCode, ReqId,		 ReqCode;
	EXEC SQL Open	LabResOtherCursor1;
	
}*/ //Yulia 20040921	   

	JUMP_ON_ERR;

	return(1);
}


int DbFetchLabResOtherSelCursor( TLabOtherRecord	*Ptr,
					int profParam)
{
EXEC SQL BEGIN DECLARE SECTION;
	TLabOtherRecord *Lab	=	Ptr;
EXEC SQL END DECLARE SECTION;
/*if (profParam == 12)
{*/
	EXEC SQL Fetch	LabResOtherCursor	Into
		:Lab->Contractor,	:Lab->TermId,			
		:Lab->MemberId.Code,	:Lab->MemberId.Number,
		:Lab->LabId,		:Lab->ReqId,
		:Lab->ReqCode,		:Lab->Route,
		:Lab->Result,		:Lab->LowerLimit,
		:Lab->UpperLimit, 	:Lab->Units,
		:Lab->ApprovalDate,	:Lab->ForwardDate,
		:Lab->ReqDate,
		:Lab->PregnantWoman,	:Lab->Completed,
		:Lab->ReqIdMain;
//	GerrLogFnameMini("test37",GerrId,"point2fetch1[%d]",sqlca.sqlcode);
/*
}
else
{
	EXEC SQL Fetch	LabResOtherCursor1	Into
		:Lab->Contractor,	:Lab->TermId,			
		:Lab->MemberId.Code,	:Lab->MemberId.Number,
		:Lab->LabId,		:Lab->ReqId,
		:Lab->ReqCode,		:Lab->Route,
		:Lab->Result,		:Lab->LowerLimit,
		:Lab->UpperLimit, 	:Lab->Units,
		:Lab->ApprovalDate,	:Lab->ForwardDate,
		:Lab->ReqDate,
		:Lab->PregnantWoman,	:Lab->Completed,
		:Lab->ReqIdMain;
//	GerrLogFnameMini("test37",GerrId,"point2fetch2[%d]",sqlca.sqlcode);

}
*/
	JUMP_ON_ERR;

	return(1);
}


int DbCloseLabResOtherSelCursor(int profParam)
{
	EXEC SQL Close	LabResOtherCursor;
	if ( SQLERR_error_test() )
	{
	return (0);
	}
	else
	{
	return(1);
	}
}




/* end 04.05.2000 */

/* Yulia add 10.97 3-th parameter to procedure for different notetype select */
 //20010418TIdNumber	ContractorParam)
int	DbOpenLabNoteSelCursor(
			        TLabRecord	*Ptr ,
					short		NoteTypeParamFrom,
					short		NoteTypeParamTo)
{

//20010418        TIdNumber	Contractor      =  ContractorParam;

EXEC SQL BEGIN DECLARE SECTION;
	TLabRecord	*Rec		=	Ptr;
	short	NoteTypeFrom	=	NoteTypeParamFrom;
	short	NoteTypeTo		=	NoteTypeParamTo;

EXEC SQL END DECLARE SECTION;

	EXEC SQL Declare	LabNotesCursor Cursor	for
	select
		Contractor,	IdCode,		IdNumber,
		ReqId,		NoteType,	ReqCode,
		LineIndex,	Note
	from	LabNotes
	where	
		Contractor	=	:Rec->Contractor
		and
		IdNumber	=	:Rec->MemberId.Number	and
		IdCode		=	:Rec->MemberId.Code	and
		ReqId		=	:Rec->ReqId 		and
		NoteType	>=	:NoteTypeFrom		and
		NoteType	<=	:NoteTypeTo		and
		ReqCode 	=	:Rec->ReqCode
	Order	By 
		Contractor,IdNumber,IdCode,ReqId,NoteType,ReqCode,LineIndex;
//		Contractor,IdNumber,IdCode,ReqId,ReqCode,NoteType,LineIndex;

	EXEC SQL Open	LabNotesCursor;

	JUMP_ON_ERR;

	NoteRecord.LabNotesFetchSucceed = 0;

	return(1);
}

int	DbFetchLabNoteSelCursor( TLabNoteRecord	  *Ptr)
{
EXEC SQL BEGIN DECLARE SECTION;
	TLabNoteRecord *Note	=	Ptr;
EXEC SQL END DECLARE SECTION;

	EXEC SQL Fetch	LabNotesCursor	Into
		:Note->Contractor,
		:Note->MemberId.Code, 	:Note->MemberId.Number,
		:Note->ReqId, 		:Note->NoteType,
		:Note->ReqCode, 	:Note->LineIndex,
		:Note->Note;

	JUMP_ON_ERR;

	return(1);
}


int	DbCloseLabNoteSelCursor(void)
{
	EXEC SQL Close	LabNotesCursor;
	if ( SQLERR_error_test() )
	{
	return (0);
	}
	else
	{
	return(1);
	}
}

/* begin 38 LabNoteOther */

/*
int	DbOpenLabNoteOtherSelCursor(	TIdNumber	ContractorParam,
					TMemberId	*MemberIdParam,
					TLabReqId	ReqIdMParam)
{
EXEC SQL BEGIN DECLARE SECTION;
    TIdNumber	Contractor      =  ContractorParam;
	TMemberId	MemberId	= *MemberIdParam;
	TLabReqId	ReqIdMainP	=  ReqIdMParam;
EXEC SQL END DECLARE SECTION;

	EXEC SQL Declare	LabNoteOtherCursor Cursor	for
	select
		Contractor,	IdCode,		IdNumber,
		ReqId,		NoteType,	ReqCode,
		LineIndex,	Note	,	ReqId_Main
	from	LabNotOther
	where	
		Contractor	=	:Contractor		and
		IdNumber	=	:MemberId.Number	and
		IdCode		=	:MemberId.Code		and
		ReqId_Main	=	:ReqIdMainP	
	
	Order	By 
		Contractor,IdNumber,IdCode,ReqId,NoteType,ReqCode,LineIndex;


	EXEC SQL Open	LabNoteOtherCursor;

	JUMP_ON_ERR;

	NoteOtherRecord.LabNotesFetchSucceed = 0;

	return(1);
}
*/
/*changed by Yulia 20040222*/
int	DbOpenLabNoteOtherSelCursor	( TLabOtherRecord	*Ptr ,
					              short		NoteTypeParamFrom,
					              short		NoteTypeParamTo)

{
EXEC SQL BEGIN DECLARE SECTION;
	TLabOtherRecord	*Rec		=	Ptr;
	short	NoteTypeFrom	=	NoteTypeParamFrom;
	short	NoteTypeTo		=	NoteTypeParamTo;

EXEC SQL END DECLARE SECTION;

	EXEC SQL Declare	LabNoteOtherCursor Cursor	for
	select  distinct
		Contractor,	IdCode,		IdNumber,
		ReqId,		NoteType,	ReqCode,
		LineIndex,	Note
	from	LabNotOther
	where	
		Contractor	=	:Rec->Contractor		and
		IdNumber	=	:Rec->MemberId.Number	and
		IdCode		=	:Rec->MemberId.Code	    and
		ReqId_Main	=	:Rec->ReqIdMain		    and
		NoteType	>=	:NoteTypeFrom		    and
		NoteType	<=	:NoteTypeTo		        and
		ReqCode 	=	:Rec->ReqCode
	 Order	By 
		Contractor,IdNumber,IdCode,ReqId,NoteType,ReqCode,LineIndex;

	EXEC SQL Open	LabNoteOtherCursor;

	JUMP_ON_ERR;

	NoteOtherRecord.LabNotesFetchSucceed = 0;

	return(1);

}

int	DbFetchLabNoteOtherSelCursor( TLabNoteOtherRecord	  *Ptr)
{
EXEC SQL BEGIN DECLARE SECTION;
	TLabNoteOtherRecord *Note	=	Ptr;
EXEC SQL END DECLARE SECTION;

	EXEC SQL Fetch	LabNoteOtherCursor	Into
		:Note->Contractor,
		:Note->MemberId.Code, 	:Note->MemberId.Number,
		:Note->ReqId, 		:Note->NoteType,
		:Note->ReqCode, 	:Note->LineIndex,
		:Note->Note;

	JUMP_ON_ERR;

	return(1);
}

int	DbCloseLabNoteOtherSelCursor(void)
{
	EXEC SQL Close	LabNoteOtherCursor;
	if ( SQLERR_error_test() )
	{
	return (0);
	}
	else
	{
	return(1);
	}
}

/* end 38 LabNoteOther */
  /* 20080130
int	DbInsertTerminalRecord( TTerminalRecord *Ptr)
{
EXEC SQL BEGIN DECLARE SECTION;
	TTerminalRecord *Terminal	=	Ptr;
EXEC SQL END DECLARE SECTION;

	EXEC SQL insert into	terminal
	(
		TermId,		Descr,		Street,		City,
		LocPhone,	AreaCode,	TermType,	ModemPrefix,
		PhonePrefix0,	PhoneNumber0,
		PhonePrefix1,	PhoneNumber1,
		PhonePrefix2,	PhoneNumber2,
		StartupDate,	StartupTime,
		SoftwareVer, 	HardwareVer,
		Contractor,	ContractType,
		LastUpdateDate, LastUpdateTime,
		UpdatedBy
	)	
	values
	(
		:Terminal->TermId,		:Terminal->Descr,
		:Terminal->Street,		:Terminal->City,
		:Terminal->LocPhone,		:Terminal->AreaCode,
		:Terminal->TermType, 		:Terminal->ModemPrefix,
		:Terminal->Phone[0].Prefix,	:Terminal->Phone[0].Number,
		:Terminal->Phone[1].Prefix,	:Terminal->Phone[1].Number,
		:Terminal->Phone[2].Prefix, 	:Terminal->Phone[2].Number,
		:Terminal->StartupTime.DbDate,	:Terminal->StartupTime.DbTime,
		:Terminal->SoftwareVer,		:Terminal->HardwareVer,
		:Terminal->Contractor, 		:Terminal->ContractType,
		:Terminal->LastUpdate.DbDate,	:Terminal->LastUpdate.DbTime,
		:Terminal->UpdatedBy
	);
	if ( SQLERR_error_test() )
	{
		return (0);
	}
	else
	{
		return (1);
	}
}
*/
int	DbGetTerminalRecord( 	TTermId 		TermId,
				 TTerminalRecord 	*Ptr)
{
EXEC SQL BEGIN DECLARE SECTION;
	TTerminalRecord *Terminal	=	Ptr;
EXEC SQL END DECLARE SECTION;
int nTry;
nTry = 10;

	Terminal->TermId	=	TermId;
	while (nTry)
	{
		EXEC SQL select
		Descr,		Street,		City,		LocPhone,
		AreaCode,	TermType,	ModemPrefix,
		PhonePrefix0,	PhoneNumber0,
		PhonePrefix1,	PhoneNumber1,
		PhonePrefix2,	PhoneNumber2,
		StartupDate,	StartupTime,
		SoftwareVer,	HardwareVer,
		Contractor,	ContractType,
		LastUpdateDate,	LastUpdateTime,
		UpdatedBy
		into
		:Terminal->Descr,		:Terminal->Street,
		:Terminal->City,		:Terminal->LocPhone,
		:Terminal->AreaCode,		:Terminal->TermType,
		:Terminal->ModemPrefix,
		:Terminal->Phone[0].Prefix,	:Terminal->Phone[0].Number,
		:Terminal->Phone[1].Prefix,	:Terminal->Phone[1].Number,
		:Terminal->Phone[2].Prefix,	:Terminal->Phone[2].Number,
		:Terminal->StartupTime.DbDate,	:Terminal->StartupTime.DbTime,
		:Terminal->SoftwareVer,		:Terminal->HardwareVer,
		:Terminal->Contractor,		:Terminal->ContractType,
		:Terminal->LastUpdate.DbDate,	:Terminal->LastUpdate.DbTime,
		:Terminal->UpdatedBy
		from	terminal
		where	
			TermId		=	:Terminal->TermId;
		if( SQLERR_code_cmp( SQLERR_access_conflict  ) == MAC_TRUE )   
		{
			nTry--;
#ifdef _LINUX_
			usleep(100000); //microsec
#else
			sleep(1);
#endif
			continue;
		}

		if( SQLERR_code_cmp( SQLERR_not_found  ) == MAC_TRUE )   
		{
			return (0);
		}
		
		if ( SQLERR_error_test() )
		{
			return (0);
		}
		else
		{
			if (nTry<10)
			{
				GerrLogFnameMini ("TermErrorOK_log", GerrId,
					"termid[%d][%d]", Terminal->TermId,nTry);

			}
			return(1);
		}
	}
	GerrLogFnameMini ("TermError_log", GerrId,
		  "termid[%d]", Terminal->TermId);
	return (0);

}

int	DbUpdateTerminalVersion( TTermId 	TermId,
				 TTerminalRecord *Ptr)
{
EXEC SQL BEGIN DECLARE SECTION;
	TTerminalRecord *Terminal	=	Ptr;
EXEC SQL END DECLARE SECTION;

	Terminal->TermId	=	TermId;

	EXEC SQL update terminal	set
		StartupDate 	=	:Terminal->StartupTime.DbDate,
		StartupTime 	=	:Terminal->StartupTime.DbTime,
		SoftwareVer 	=	:Terminal->SoftwareVer,
		HardwareVer 	=	:Terminal->HardwareVer,
		Contractor	=	:Terminal->Contractor,
		ContractType	=	:Terminal->ContractType
	where	
		TermId		=	:Terminal->TermId;

	JUMP_ON_ERR;

	return(1);
}


int	DbUpdateTerminalRecord( TTermId 	TermId,
				TTerminalRecord *Ptr)
{
EXEC SQL BEGIN DECLARE SECTION;
	TTerminalRecord *Terminal	=	Ptr;
EXEC SQL END DECLARE SECTION;
	
	Terminal->TermId	=	TermId;

	EXEC SQL update terminal	set
		Descr		=	:Terminal->Descr,
		Street		=	:Terminal->Street,
		City		=	:Terminal->City,
		LocPhone	=	:Terminal->LocPhone,
		AreaCode	=	:Terminal->AreaCode,
		TermType	=	:Terminal->TermType,
		ModemPrefix	=	:Terminal->ModemPrefix,
		PhonePrefix0	=	:Terminal->Phone[0].Prefix,
		PhoneNumber0	=	:Terminal->Phone[0].Number,
		PhonePrefix1	=	:Terminal->Phone[1].Prefix,
		PhoneNumber1	=	:Terminal->Phone[1].Number,
		PhonePrefix2	=	:Terminal->Phone[2].Prefix,
		PhoneNumber2	=	:Terminal->Phone[2].Number,
		StartupDate 	=	:Terminal->StartupTime.DbDate,
		StartupTime 	=	:Terminal->StartupTime.DbTime,
		SoftwareVer 	=	:Terminal->SoftwareVer,
		HardwareVer 	=	:Terminal->HardwareVer,
		Contractor	=	:Terminal->Contractor,
		ContractType	=	:Terminal->ContractType,
		LastUpdateDate	=	:Terminal->LastUpdate.DbDate,
		LastUpdateTime	=	:Terminal->LastUpdate.DbTime,
		UpdatedBy	=	:Terminal->UpdatedBy
	where	
		TermId		=	:Terminal->TermId;

	if ( SQLERR_error_test() )
	{
	return (0);
	}
	else
	{
	return(1);
	}
}


int	DbDeleteTerminalRecord( TTermId TermId)
{
EXEC SQL BEGIN DECLARE SECTION;
	TTermId TId	=	TermId;
EXEC SQL END DECLARE SECTION;

	EXEC SQL Delete from Terminal	
	where	
		TermId	=	:TId;

	JUMP_ON_ERR;

	return(1);
}

int DbGetSysParamsRecord( TSysParamsRecord	*Ptr)
{
EXEC SQL BEGIN DECLARE SECTION;	
	TSysParamsRecord	 *Params	=	Ptr;
EXEC SQL END DECLARE SECTION;
//must change last field   20010122
	EXEC SQL select
		TaxAmount,		InactivityTimeout, 	HostRspTimeout,
		LastUpdateDate, LastUpdateTime, 	UpdatedBy,
		Tax10_12_1019,	Tax_machon_mac,		Tax_machon_pr,		Tax_all,
		Tax_Urgent,		Tax_speak,    		tax_machon_mac_12
	into
		:Params->TaxAmount,				:Params->InactivityTimeout,
		:Params->HostRspTimeout,
		:Params->LastUpdate.DbDate, 	:Params->LastUpdate.DbTime,
		:Params->UpdatedBy,
		:Params->Tax10_12_1019,			:Params->Tax_machon_mac,
		:Params->Tax_machon_pr,			:Params->Tax_all,
		:Params->Tax_Urgent,			:Params->Tax_speak,
		:Params->Tax_machon_mac_12
	from	sysparams
	where	
		SysParamsKey	=	1;

	JUMP_ON_ERR;

	return(1);
}


int DbInsertSysParamsRecord( TSysParamsRecord	*Ptr)
{
EXEC SQL BEGIN DECLARE SECTION;
	TSysParamsRecord	 *Params	=	Ptr;
EXEC SQL END DECLARE SECTION;
	Ptr->LastUpdate 			=	CurrDateTime;

	EXEC SQL Insert Into	SysParams
	(
		SysParamsKey, 		TaxAmount,
		InactivityTimeout, 	HostRspTimeout,
		LastUpdateDate, 	LastUpdateTime,
		UpdatedBy
	)	
	Values
	(	 
		1,				:Params->TaxAmount,
		:Params->InactivityTimeout,	:Params->HostRspTimeout,
		:Params->LastUpdate.DbDate,	:Params->LastUpdate.DbTime,
		:Params->UpdatedBy
	);

	JUMP_ON_ERR;

	return(1);
}


int DbUpdateSysParamsRecord( TSysParamsRecord	*Ptr)
{
EXEC SQL BEGIN DECLARE SECTION;
	TSysParamsRecord	 *Params	=	Ptr;
EXEC SQL END DECLARE SECTION;

	Ptr->LastUpdate 	=	CurrDateTime;

	EXEC SQL update	SysParams
	set
		TaxAmount		=	:Params->TaxAmount,
		InactivityTimeout	=	:Params->InactivityTimeout,
		HostRspTimeout		=	:Params->HostRspTimeout,
		LastUpdateDate		=	:Params->LastUpdate.DbDate,
		LastUpdateTime		=	:Params->LastUpdate.DbTime,
		UpdatedBy		=	:Params->UpdatedBy
	where	
		sysparamskey		=	1;

	JUMP_ON_ERR;

	return(1);
}

int DbDeleteContermRecord( 	TTermId		Location,
				TIdNumber	Contractor,
				TContractType	ContractType)
{
EXEC SQL BEGIN DECLARE SECTION;
	TContermRecord Conterm;
	int NowDate,NowTime;
EXEC SQL END DECLARE SECTION;

	NowDate = GetDate();
	NowTime = GetTime();
	Conterm.Location		=	Location;
	Conterm.Contractor	=	Contractor;
	Conterm.ContractType	=	ContractType;

/*	EXEC SQL	INSERT into contermdeleted
				SELECT * from conterm 
	where	
		Contractor		=	:Conterm.Contractor	and
		ContractType	=	:Conterm.ContractType and
		Location		=	:Conterm.Location	;
	printf ("\n ContermDeleted [%d][%d][%d]",Conterm.Contractor,Conterm.ContractType,Conterm.Location);
	fflush(stdout);

	JUMP_ON_ERR;

	EXEC SQL	DELETE from	conterm
	where	
		Contractor		=	:Conterm.Contractor	and
		ContractType	=	:Conterm.ContractType and
		Location		=	:Conterm.Location	;
	JUMP_ON_ERR;
	*/
	EXEC SQL	update	conterm set  
		validuntil		=	:NowDate,
		lastupdatedate	=	:NowDate,
		lastupdatetime	=	:NowTime,
		authtype        =   2,/*Yulia end 20061030*/		
		updatedby		=	7777
	where	
		Contractor		=	:Conterm.Contractor	and
		ContractType	=	:Conterm.ContractType and
		Location		=	:Conterm.Location	;
	JUMP_ON_ERR;

	GerrLogMini (GerrId,"End Contractor:%d-%d in Locat:%d ",
							 Conterm.Contractor,Conterm.ContractType, Conterm.Location);

	return(1);
}


int DbUpdateTerminalConterms(	TTermId OldTermIdParam,
				TTermId NewTermIdParam)
{
EXEC SQL BEGIN DECLARE SECTION;
	TTermId OldTermId	=	OldTermIdParam;
	TTermId NewTermId	=	NewTermIdParam;
EXEC SQL END DECLARE SECTION;

	EXEC SQL update conterm
	set 
		TermId	=	:NewTermId
	where	
		TermId	=	:OldTermId;

	return(1);
}
/*Y 20080130
int DbUpdateContermRecord( 	TTermId		NewTermId,
				TIdNumber	Contractor,
				TContractType	ContractType,
				TContermRecord	*Ptr)
{
EXEC SQL BEGIN DECLARE SECTION;
	TContermRecord *Conterm	=	Ptr;
	TTermId	OldTermId;
EXEC SQL END DECLARE SECTION;


	OldTermId		=	Conterm->TermId;
	Conterm->TermId 	=	NewTermId;
	Conterm->Contractor 	=	Contractor;
	Conterm->ContractType	=	ContractType;
	EXEC SQL update conterm
	set
		AuthType	=	:Conterm->AuthType,
		ValidFrom	=	:Conterm->ValidFrom,
		ValidUntil	=	:Conterm->ValidUntil,
		HasPharmacy 	=	:Conterm->HasPharmacy,
		Location	=	:Conterm->Location,
		Locationprv	=	:Conterm->Locationprv,
		LastUpdateDate	=	:Conterm->LastUpdate.DbDate,
		LastUpdateTime	=	:Conterm->LastUpdate.DbTime,
		UpdatedBy	=	:Conterm->UpdatedBy,
		Position	=	:Conterm->Position,
		Paymenttype	=	:Conterm->Paymenttype,
		Mahoz		=	:Conterm->Mahoz,
		Snif		=	:Conterm->Snif,
		ServiceType	=	:Conterm->ServiceType
	where	
		TermId		=	:OldTermId		and
		Contractor	=	:Conterm->Contractor	and
		ContractType	=	:Conterm->ContractType;

	if ( SQLERR_error_test() )
	{
	return (0);
	}
	else
	{
	return(1);
	}

}
*/
int DbGetContermRecord( 	TTermId		TermId,
				TIdNumber	Contractor,
				TContractType	ContractType,
				TContermRecord	*Ptr)
{
EXEC SQL BEGIN DECLARE SECTION;
	TContermRecord *Conterm	=	Ptr;
EXEC SQL END DECLARE SECTION;

	Conterm->TermId 	=	TermId;
	Conterm->Contractor 	=	Contractor;
	Conterm->ContractType	=	ContractType;

	EXEC SQL select
		AuthType, 		ValidFrom, 		ValidUntil,
		Location, 		Locationprv,	HasPharmacy,
		LastUpdateDate, LastUpdateTime,
		UpdatedBy,		Position,
		Paymenttype, 	Mahoz, 			Snif,
		messageid,		ServiceType,	StatusUrgent,
		DrugDateI,		DrugTimeI,
		DrugDateO,		DrugTimeO,City
	into
		:Conterm->AuthType, 		:Conterm->ValidFrom, 	
		:Conterm->ValidUntil, 		:Conterm->Location, 
		:Conterm->Locationprv, 		:Conterm->HasPharmacy,
		:Conterm->LastUpdate.DbDate, 	:Conterm->LastUpdate.DbTime,
		:Conterm->UpdatedBy, 		:Conterm->Position,
		:Conterm->Paymenttype, 		:Conterm->Mahoz, 
		:Conterm->Snif,
		:Conterm->MsgID,			:Conterm->ServiceType,
		:Conterm->StatusUrgent,
		:Conterm->DrugDateI,			:Conterm->DrugTimeI,
		:Conterm->DrugDateO,			:Conterm->DrugTimeO,
		:Conterm->City
	from	conterm
	where	
		TermId		=	:Conterm->TermId	and
		Contractor	=	:Conterm->Contractor	and
		ContractType	=	:Conterm->ContractType;
        if( SQLERR_code_cmp( SQLERR_not_found  ) == MAC_TRUE )   
	{
	return (0);
	}
	
	if ( SQLERR_error_test() )
	{
	return (0);
	}
	else
	{
	return(1);
	}


}

int DbUpdateContermDrug( 	TContermRecord	*Ptr)
{
EXEC SQL BEGIN DECLARE SECTION;
	TContermRecord *Conterm	=	Ptr;
EXEC SQL END DECLARE SECTION;

	EXEC SQL update conterm
	set
		DrugDateI	=	:Conterm->DrugDateI,
		DrugTimeI	=   :Conterm->DrugTimeI,
		DrugDateO	=	:Conterm->DrugDateO,
		DrugTimeO	=   :Conterm->DrugTimeO

		where
		TermId			=	:Conterm->TermId		and
		Contractor		=	:Conterm->Contractor	and
		ContractType	=	:Conterm->ContractType;

	if ( SQLERR_error_test() )
	{
	return (0);
	}
	else
	{
	return(1);
	}

}

/*20080130

int DbInsertContermRecord( TContermRecord	*Ptr)
{
EXEC SQL BEGIN DECLARE SECTION;
	TContermRecord *Conterm	=	Ptr;
EXEC SQL END DECLARE SECTION;

	EXEC SQL insert into	conterm
	(
		TermId,		Contractor,	ContractType,
		AuthType,	ValidFrom,	ValidUntil,
		Location,	Locationprv,	HasPharmacy,
		LastUpdateDate,	LastUpdateTime,
		UpdatedBy,	Position,
		Paymenttype,	Mahoz,		Snif,
		ServiceType
	)	
	values
	( 
		:Conterm->TermId,
		:Conterm->Contractor,		:Conterm->ContractType,
		:Conterm->AuthType,
		:Conterm->ValidFrom, 		:Conterm->ValidUntil,
		:Conterm->Location, 		:Conterm->Locationprv,
		:Conterm->HasPharmacy,
		:Conterm->LastUpdate.DbDate,	:Conterm->LastUpdate.DbTime,
		:Conterm->UpdatedBy,		:Conterm->Position,
		:Conterm->Paymenttype, 		:Conterm->Mahoz,
		:Conterm->Snif,			:Conterm->ServiceType
	);

	if ( SQLERR_error_test() )
	{
		return (0);
	}
	else
	{
		return(1);
	}
}

*/
int	DbInsertConStatRecord( TConStatRecord	*Ptr)
{
EXEC SQL BEGIN DECLARE SECTION;
	TConStatRecord	*ConStat	=	Ptr;
EXEC SQL END DECLARE SECTION;

	EXEC SQL insert into	constat
	(
		Contractor,		ContractType,
		ShiftTermId,		ShiftOpened,
		Location,		LastReqCode,
		LastDate,		LastTime,
		LabDate,		LabTime,		LabReqTerm,
		LabCnfDate,		LabCnfTime,		LabConfTerm
	)	
	values
	(
		:ConStat->Contractor,		:ConStat->ContractType,
		:ConStat->ShiftTermId,		:ConStat->ShiftOpened,
		:ConStat->Location,		:ConStat->LastReqCode,
		:ConStat->LastDate.DbDate,	:ConStat->LastDate.DbTime,
		0,	0,	0,
		0,	0,	0
	);

	JUMP_ON_ERR;

	return(1);
}


int	DbDeleteConStatRecord( 	TIdNumber	Contractor,
			   	TContractType	ContractType)
{
EXEC SQL BEGIN DECLARE SECTION;
	TConStatRecord	ConStat;
EXEC SQL END DECLARE SECTION;

	ConStat.Contractor	=	Contractor;
	ConStat.ContractType	=	ContractType;

	EXEC SQL delete from constat
	where	
		ContractType	=	:ConStat.ContractType	and
		Contractor	=	:ConStat.Contractor;

	return(1);
}


int	DbGetConStatRecord( 	TIdNumber	Contractor,
				TContractType	ContractType,
				TConStatRecord	*Ptr)
{
EXEC SQL BEGIN DECLARE SECTION;
	TConStatRecord	*ConStat	=	Ptr;
EXEC SQL END DECLARE SECTION;

	ConStat->Contractor	=	Contractor;
	ConStat->ContractType	=	ContractType;

	EXEC SQL select
		ShiftTermId,		ShiftOpened,
		Location, 		LastReqCode,
/*		LastTransTime,  old */
		LastDate,		LastTime,
/*		LabReqTime,     old */
		LabDate,		LabTime,	LabReqTerm,
/*		LabConfTime,   old */
		LabCnfDate,		LabCnfTime,	LabConfTerm
	into
		:ConStat->ShiftTermId, 		:ConStat->ShiftOpened,
		:ConStat->Location, 		:ConStat->LastReqCode,
/*		:ConStat->LastTransTime,old */
		:ConStat->LastDate.DbDate, 	:ConStat->LastDate.DbTime,
/*		:ConStat->LabReqTime,  old */
		:ConStat->LabDate.DbDate, 	:ConStat->LabDate.DbTime,
		:ConStat->LabReqTerm,
/*		:ConStat->LabConfTime, old */
		:ConStat->LabCnfDate.DbDate, 	:ConStat->LabCnfDate.DbTime,
		:ConStat->LabConfTerm
	from	ConStat
	where	
		ContractType	=	:ConStat->ContractType	and
		Contractor	=	:ConStat->Contractor;

	JUMP_ON_ERR;

	return(1);
}


int	DbUpdateConStatRecord( 	TTermId		TermId,
				TIdNumber	Contractor,
				TContractType	ContractType,
				short 		ReqCode,
				TLocationId 	Location,
				TDbDateTime	LastDate)
{
EXEC SQL BEGIN DECLARE SECTION;
	TConStatRecord	ConStat;
EXEC SQL END DECLARE SECTION;

	ConStat.Contractor	=	Contractor;
	ConStat.ContractType	=	ContractType;
	ConStat.ShiftTermId	=	TermId;
	ConStat.Location	=	Location;
	ConStat.LastReqCode	=	ReqCode;
	ConStat.ShiftOpened 	=	1;
	/* ConStat.LastTransTime	=	(int)TransTime; old */
	 ConStat.LastDate.DbDate	=	LastDate.DbDate;	
	 ConStat.LastDate.DbTime	=	LastDate.DbTime;	

	EXEC SQL update ConStat	set
		ShiftTermId		=	:ConStat.ShiftTermId,
		ShiftOpened 		=	:ConStat.ShiftOpened,
		Location		=	:ConStat.Location,
		LastReqCode 		=	:ConStat.LastReqCode,
		LastDate		=	:ConStat.LastDate.DbDate,
		LastTime		=	:ConStat.LastDate.DbTime
	where	
		ContractType	=	:ConStat.ContractType	and
		Contractor	=	:ConStat.Contractor;
//if ( ReqCode == 68 )
//{
//GerrLogFnameMini("log68",GerrId,"in DbUpdateConStat Contr[%d:%d]Term[%d]sqlcode[%d]",
//	ConStat.Contractor,
//	ConStat.ContractType,
//	ConStat.ShiftTermId,
//	sqlca.sqlcode);
//}

	/* change error handling it is not work */
	JUMP_ON_ERR;

	if (sqlca.sqlerrd[2] == 0 )
	{
		DbInsertConStatRecord(&ConStat);
	}

	return(1);
}

int	DbUpdateConStatLabReq( 	TTermId		TermId,
				TIdNumber	Contractor,
				TContractType	ContractType,
				TDbDateTime	LabDate)
{
EXEC SQL BEGIN DECLARE SECTION;
	TConStatRecord ConStat;
EXEC SQL END DECLARE SECTION;
	ConStat.LabReqTerm	=	TermId;
	ConStat.Contractor	=	Contractor;
	ConStat.ContractType	=	ContractType;
/*	ConStat.LabReqTime	=	TransTime;   old */
	ConStat.LabDate.DbDate	=	LabDate.DbDate;	
	ConStat.LabDate.DbTime	=	LabDate.DbTime;	

	EXEC SQL update ConStat	set
		LabReqTerm	=	:ConStat.LabReqTerm,
		LabDate		=	:ConStat.LabDate.DbDate,
		LabTime		=	:ConStat.LabDate.DbTime
	where	
		ContractType	=	:ConStat.ContractType	and
		Contractor	=	:ConStat.Contractor;

	JUMP_ON_ERR;

	return(1);
}


int	DbUpdateConStatLabConf( TTermId		TermId,
				TIdNumber	Contractor,
				TContractType	ContractType,
				TDbDateTime	LabCnfDate)
{
EXEC SQL BEGIN DECLARE SECTION;
	TConStatRecord ConStat;
EXEC SQL END DECLARE SECTION;
		ConStat.LabConfTerm		=	TermId;
		ConStat.Contractor		=	Contractor;
		ConStat.ContractType		=	ContractType;
		ConStat.LabCnfDate.DbDate	=	LabCnfDate.DbDate;	
		ConStat.LabCnfDate.DbTime	=	LabCnfDate.DbTime;	

	EXEC SQL update ConStat	set
		LabConfTerm		=	:ConStat.LabConfTerm,
		LabCnfDate		=	:ConStat.LabCnfDate.DbDate,
		LabCnfTime		=	:ConStat.LabCnfDate.DbTime
	where	
		ContractType		=	:ConStat.ContractType	and
		Contractor		=	:ConStat.Contractor;

	JUMP_ON_ERR;

	return(1);
}

int	DbCloseContractorShift( TIdNumber	Contractor,
				TContractType	ContractType)
{
	EXEC SQL BEGIN DECLARE SECTION;
	TConStatRecord ConStat;
	EXEC SQL END DECLARE SECTION;
	ConStat.Contractor	=	Contractor;
	ConStat.ContractType	=	ContractType;
	EXEC SQL update ConStat set
		ShiftOpened =	0
	where	
		ContractType	=	:ConStat.ContractType		and
		Contractor	=	:ConStat.Contractor;
	return(1);
}

int DbGetContractor( TIdNumber Contractor )
{  // GAL 30/01/2003

  EXEC SQL BEGIN DECLARE SECTION;
	int h_contractor = Contractor;
	int h_count      = 0;
  EXEC SQL END DECLARE SECTION;
	int nRet = 0;  
	
	if(	Contractor <= 0	)
	{
		return nRet;
	}

	EXEC SQL 
		SELECT count( * ) INTO  :h_count
		FROM   contract
	    WHERE  contractor =	:h_contractor;

	// Row not found
	if ( SQLERR_code_cmp( SQLERR_not_found ) == MAC_TRUE )
	{    
	     return nRet;
	}

    // Check for errors
	if ( SQLERR_error_test() )
	{    
		 return nRet;
	}

	// Row found 
	return nRet = h_count;

} // DbGetContractor 

int DbGetContractRecord( 	TIdNumber	Contractor,
				TContractType	ContractType,
				TContractRecord	*Ptr)
{
EXEC SQL BEGIN DECLARE SECTION;
	TContractRecord *Contract	=	Ptr;
EXEC SQL END DECLARE SECTION;
	if(	Contractor	<=	0	)	return(0);

	Contract->Contractor	=	Contractor;
	Contract->ContractType	=	ContractType;

	EXEC SQL select
		FirstName,		LastName,
		Street,			City,
		Mahoz,			Phone,
		Profession, 		AcceptUrgent,
		SexesAllowed, 		AcceptAnyAge,
		AllowKbdEntry, 		NoShifts,
		IgnorePrevAuth,		AuthorizeAlways,		NoAuthRecord,
		LastUpdateDate, 	LastUpdateTime,
		UpdatedBy
	into
		:Contract->FirstName,		:Contract->LastName,
		:Contract->Street,		:Contract->City,
		:Contract->Mahoz,		:Contract->Phone,
		:Contract->Profession,		:Contract->AcceptUrgent,
		:Contract->SexesAllowed,	:Contract->AcceptAnyAge,
		:Contract->AllowKbdEntry,	:Contract->NoShifts,
		:Contract->IgnorePrevAuth,	:Contract->AuthorizeAlways,
		:Contract->NoAuthRecord,
		:Contract->LastUpdate.DbDate,	:Contract->LastUpdate.DbTime,
		:Contract->UpdatedBy
	from	contract
	where	
		Contractor	=	:Contract->Contractor		and
		ContractType	=	:Contract->ContractType;

	//
	// Row not found
	//
	if( SQLERR_code_cmp( SQLERR_not_found ) == MAC_TRUE )
	{
	    return (0);
	}

	//
	// Check for errors
	//
	if ( SQLERR_error_test() )
	{
		return (0);
	}

	//
	// Row found
	//
	return(1);
}

int DbGetNextContractRecord( 	TIdNumber	Contractor,
				TContractType	ContractType,
				TContractRecord	*Ptr)
{
EXEC SQL BEGIN DECLARE SECTION;
    TContractRecord *Contract	=	Ptr;
EXEC SQL END DECLARE SECTION;
    
    Contract->Contractor	=	Contractor;
    Contract->ContractType	=	ContractType;
    
    EXEC SQL declare	FirstContractor	cursor	for
	select 
	    Contractor, ContractType
	from	Contract
	where	
	    Contractor 	>=	:Contract->Contractor
	order	by	
	    Contractor , ContractType;
    
    EXEC SQL open	FirstContractor;
    
    SQLCODE =	0;
    
    while(	SQLCODE ==	0	&&
	Contractor	==	Contract->Contractor	&&
	ContractType	==	Contract->ContractType	)
    {
	EXEC SQL fetch	
	    FirstContractor
	 Into 
	    :Contract->Contractor :Contract->ContractType;
    }
    if(	SQLCODE !=	0	)
    {
	Contract->Contractor	=	Contractor;
	Contract->ContractType	=	ContractType;
    } else
	if(	Contract->Contractor	!=	Contractor	&&
	    Contractor		!=	0	)
	{
	    Contract->Contractor	=	Contractor;
	    Contract->ContractType	=	ContractType;
	    EXEC SQL close	FirstContractor;
	    return(0);   /* change error handling   */
	}
    EXEC SQL close	FirstContractor;
    DbGetContractRecord(Contract->Contractor,Contract->ContractType,Ptr);
    return(1);   /* change error handling   */

}

int DbGetContractorDetails( 	TIdNumber	Contractor,
				TContractRecord	*Ptr)
{
EXEC SQL BEGIN DECLARE SECTION;
	TContractRecord *Contract	=	Ptr;
EXEC SQL END DECLARE SECTION;

	Contract->Contractor	=	Contractor;

	EXEC SQL declare	ContractDetails	cursor	for
	select distinct
		FirstName,	LastName,
		Street,	 	City,
		Phone,	 	Profession
	from	contract
	where	
		Contractor	=	:Contract->Contractor;

	EXEC SQL open	ContractDetails;
	EXEC SQL Fetch	ContractDetails	  into
		:Contract->FirstName,		:Contract->LastName,
		:Contract->Street, 		:Contract->City,
		:Contract->Phone,		:Contract->Profession;
	EXEC SQL Close	ContractDetails;

	return(1);
}

int	DbContractorExists( TIdNumber	Contractor)
{
EXEC SQL BEGIN DECLARE SECTION;
	TIdNumber	Id;
EXEC SQL END DECLARE SECTION;
	Id	=	Contractor;
	EXEC SQL select 
		distinct	contractor 
	into 
		:Id
	from	contract
	where 
		contractor	=	:Id;

	return(1);
}

/* 20080130
int DbUpdateContractRecord( 	TIdNumber	Contractor,
				TContractType	ContractType,
				TContractRecord	*Ptr)
{
EXEC SQL BEGIN DECLARE SECTION;
	TContractRecord *Contract	=	Ptr;
EXEC SQL END DECLARE SECTION;
		Contract->Contractor	=	Contractor;
		Contract->ContractType	=	ContractType;

	EXEC SQL Update contract
	set
		FirstName		=	:Contract->FirstName,
		LastName		=	:Contract->LastName,
		Street			=	:Contract->Street,
		City			=	:Contract->City,
		Mahoz			=	:Contract->Mahoz,
		Phone			=	:Contract->Phone,
		Profession		=	:Contract->Profession,
		AcceptUrgent		=	:Contract->AcceptUrgent,
		SexesAllowed		=	:Contract->SexesAllowed,
		AcceptAnyAge		=	:Contract->AcceptAnyAge,
		AllowKbdEntry		=	:Contract->AllowKbdEntry,
		NoShifts		=	:Contract->NoShifts,
		IgnorePrevAuth		=	:Contract->IgnorePrevAuth,
		AuthorizeAlways	 	=	:Contract->AuthorizeAlways,
		NoAuthRecord		=	:Contract->NoAuthRecord,
		LastUpdateDate		=	:Contract->LastUpdate.DbDate,
		LastUpdateTime		=	:Contract->LastUpdate.DbTime,
		UpdatedBy		=	:Contract->UpdatedBy
	where	
		Contractor	=	:Contract->Contractor		and
		ContractType	=	:Contract->ContractType;

	if ( SQLERR_error_test() )
	{
	return (0);
	}
	else
	{
	return(1);
	}
}


int DbInsertContractRecord( TContractRecord	*Ptr)
{
	EXEC SQL BEGIN DECLARE SECTION;
	TContractRecord *Contract	=	Ptr;
EXEC SQL END DECLARE SECTION;

	EXEC SQL insert into	contract
	(
		Contractor, 		ContractType,
		FirstName, 		LastName,
		Street, 		City,
		Mahoz, 			Phone,
		Profession, 		AcceptUrgent,
		SexesAllowed, 		AcceptAnyAge,
		AllowKbdEntry, 		NoShifts,
		IgnorePrevAuth, 	AuthorizeAlways,
		NoAuthRecord,
		LastUpdateDate, 	LastUpdateTime,
		UpdatedBy
	)	
	values
	(
		:Contract->Contractor, 		:Contract->ContractType,
		:Contract->FirstName, 		:Contract->LastName,
		:Contract->Street, 		:Contract->City,
		:Contract->Mahoz, 		:Contract->Phone,
		:Contract->Profession, 		:Contract->AcceptUrgent,
		:Contract->SexesAllowed, 	:Contract->AcceptAnyAge,
		:Contract->AllowKbdEntry, 	:Contract->NoShifts,
		:Contract->IgnorePrevAuth, 	:Contract->AuthorizeAlways,
		:Contract->NoAuthRecord,
		:Contract->LastUpdate.DbDate,	:Contract->LastUpdate.DbTime,
		:Contract->UpdatedBy
	);

	if ( SQLERR_error_test() )
	{
		return (0);
	}
	else
	{
		return(1);
	}
}

*/
int DbDeleteContractRecord( 	TIdNumber	Contractor,
				TContractType	ContractType)
{
EXEC SQL BEGIN DECLARE SECTION;
	TContractRecord Contract;
EXEC SQL END DECLARE SECTION;

		Contract.Contractor	=	Contractor;
		Contract.ContractType	=	ContractType;

	EXEC SQL delete from contract
	where	
		Contractor	=	:Contract.Contractor		and
		ContractType	=	:Contract.ContractType;

	return(1);
}


int DbGetMemberRecord( 	TMemberId	*MemberId,
			TMemberRecord	*Ptr)
{
EXEC SQL BEGIN DECLARE SECTION;
	TMemberRecord	*Member =	Ptr;
EXEC SQL END DECLARE SECTION;

	Member->MemberId	=	*MemberId;

	EXEC SQL select
		CardIssueDate, 		FatherName, 		FirstName,
		LastName, 		Street, 		City,
		Zip, 			Phone, 			Sex,
		BirthDate, 		FamilyStatus, 		GnrlElegebility,
		DentalElegebility, 	GnrlValidUntil, 	DentalValidFrom,
		DentalValidUntil, 	AuthorizeAlways, 	UrgentOnlyGroup,
		Asaf, 			LastUpdateDate, 	LastUpdateTime,
		UpdatedBy, 		KrnElegebility, 	KerenValidFrom,
		KerenValidUntil, 	Insurance, CreditType , UpdateAdr,
		KerenWaitFlag,		mess,		snif
	into
		:Member->CardIssueDate, 	:Member->FatherName,
		:Member->FirstName, 		:Member->LastName,
		:Member->Street, 		:Member->City,
		:Member->Zip, 			:Member->Phone,
		:Member->Sex, 			:Member->BirthDate,
		:Member->FamilyStatus, 		:Member->GnrlElegebility,
		:Member->DentalElegebility, 	:Member->GnrlValidUntil,
		:Member->DentalValidFrom, 	:Member->DentalValidUntil,
		:Member->AuthorizeAlways, 	:Member->UrgentOnlyGroup,
		:Member->Asaf, 			:Member->LastUpdate.DbDate,
		:Member->LastUpdate.DbTime, 	:Member->UpdatedBy,
		:Member->KrnElegebility, 	:Member->KerenValidFrom,
		:Member->KerenValidUntil, 	:Member->Insurance,
		:Member->CreditType	,		:Member->UpdateAdr,
		:Member->KerenWaitFlag,	
		:Member->Mess,				:Member->Snif

	from	membersyn
	where	
		IdCode		=	:Member->MemberId.Code		and
		IdNumber	=	:Member->MemberId.Number;

	JUMP_ON_ERR;

	if(	sqlca.sqlcode	==	0 ) /* change error handling */
	{
/*	TruncSpace(&Member->FirstName	,sizeof(Member->FirstName)	,1);
	TruncSpace(&Member->LastName	,sizeof(Member->LastName)	,1);
	TruncSpace(&Member->FatherName	,sizeof(Member->FatherName)	,1);
	TruncSpace(&Member->Street	,sizeof(Member->Street)		,1);
	TruncSpace(&Member->City	,sizeof(Member->City)		,1);
	TruncSpace(&Member->Phone	,sizeof(Member->Phone)		,1);
*/

		if(	Member->BirthDate	<	10000000	)
		{
			Member->BirthDate	+=	19000000;
		}
		if ( Member->UpdateAdr == 0 )
		{
			Member->UpdateAdr = 19700101 ; //because Clicks have problem with "0" Y20050209
		}
	}
//	GerrLogFnameMini("23_log",GerrId,"im macsql:id[%d]mess[%d]",
//														MemberRecord.MemberId.Number,
//														MemberRecord.Mess);
	
	return(1);
}


int	DbUpdateMemberAuthorizeBit( 	TMemberId	*MemberId,
					short	Authorize)
{
EXEC SQL BEGIN DECLARE SECTION;
	TMemberId	*Member =	MemberId;
	short	Auth		=	Authorize;
EXEC SQL END DECLARE SECTION;

	EXEC SQL update membersyn	set
		AuthorizeAlways 	=	:Auth
	where	
		IdCode			=	:Member->Code	and
		IdNumber		=	:Member->Number;

	JUMP_ON_ERR;

	return(1);
}

int	DbUpdateMemberUrgentOnlyGroup( 	TMemberId	*MemberId,
					short	Urgent)
{
EXEC SQL BEGIN DECLARE SECTION;
	TMemberId	*Member =	MemberId;
	short	UrgentGr		=	Urgent;
EXEC SQL END DECLARE SECTION;

	EXEC SQL update membersyn	set
		UrgentOnlyGroup 	=	:UrgentGr
	where	
		IdCode			=	:Member->Code	and
		IdNumber		=	:Member->Number;

	JUMP_ON_ERR;

	return(1);
}


int	DbGetProfessionRecord( 	TProfession 	Profession,
				TProfessionRecord	*Ptr)
{
EXEC SQL BEGIN DECLARE SECTION;
	TProfessionRecord	*Prof	=	Ptr;
	int v_tax,v_tax1019;
EXEC SQL END DECLARE SECTION;
	
	Prof->Profession		=	Profession;

	EXEC SQL select
		Group,			 Descr,
		AcceptUrgent,		 SexesAllowed,
		MinAgeAllowed,		 MaxAgeAllowed,
		LastUpdateDate,		 LastUpdateTime,
		UpdatedBy,			 LabOtherAllow,
		Tax,				 Tax1019
	into
		:Prof->Group, 				:Prof->Descr,
		:Prof->AcceptUrgent, 		:Prof->SexesAllowed,
		:Prof->MinAgeAllowed, 		:Prof->MaxAgeAllowed,
		:Prof->LastUpdate.DbDate,	:Prof->LastUpdate.DbTime,
		:Prof->UpdatedBy,			:Prof->LabOtherAllow,
		:v_tax,						:v_tax1019
	from	profession
	where	Profession	=	:Prof->Profession;
	EXEC SQL select taxvalue into :Prof->Tax from taxrecords
	where taxcode = :v_tax;
	EXEC SQL select taxvalue into :Prof->Tax1019 from taxrecords
	where taxcode = :v_tax1019;
	GerrLogFnameMini ("proftax_log", GerrId,"profession[%d]tax[%d]-[%d]",
	Prof->Profession,Prof->Tax,Prof->Tax);

	JUMP_ON_ERR;

	return(1);
}


int	DbInsertProfessionRecord( TProfessionRecord	*Ptr)
{
EXEC SQL BEGIN DECLARE SECTION;
	TProfessionRecord	*Prof	=	Ptr;
EXEC SQL END DECLARE SECTION;
	Ptr->LastUpdate 		=	CurrDateTime;

	EXEC SQL insert into	Profession
	(
		Profession,		Group,		 	Descr,
		AcceptUrgent,		SexesAllowed, 		MinAgeAllowed,
		MaxAgeAllowed, 		LastUpdateDate,		LastUpdateTime,
		UpdatedBy
	)	
	values
	(
		:Prof->Profession, 		:Prof->Group,	 
		:Prof->Descr, 			:Prof->AcceptUrgent,		
		:Prof->SexesAllowed, 		:Prof->MinAgeAllowed, 	
		:Prof->MaxAgeAllowed, 		:Prof->LastUpdate.DbDate,
		:Prof->LastUpdate.DbTime, 	:Prof->UpdatedBy
	);
	return(1);
}


int	DbUpdateProfessionRecord( 	TProfession 	Profession,
					TProfessionRecord	*Ptr)
{
EXEC SQL BEGIN DECLARE SECTION;
	TProfessionRecord	*Prof	=	Ptr;
EXEC SQL END DECLARE SECTION;	
	Prof->Profession		=	Profession;
	Ptr->LastUpdate 		=	CurrDateTime;

	EXEC SQL Update Profession	Set
		Group		=	:Prof->Group,
		Descr		=	:Prof->Descr,
		AcceptUrgent	=	:Prof->AcceptUrgent,
		SexesAllowed	=	:Prof->SexesAllowed,
		MinAgeAllowed	=	:Prof->MinAgeAllowed,
		MaxAgeAllowed	=	:Prof->MaxAgeAllowed,
		LastUpdateDate	=	:Prof->LastUpdate.DbDate,
		LastUpdateTime	=	:Prof->LastUpdate.DbTime,
		UpdatedBy	=	:Prof->UpdatedBy
	where	
		Profession	=	:Prof->Profession;

	return(1);
}


int	DbDeleteProfessionRecord( TProfession 	Profession)
{
EXEC SQL BEGIN DECLARE SECTION;
	TProfessionRecord	Prof;
EXEC SQL END DECLARE SECTION;

		Prof.Profession	=	Profession;

	EXEC SQL delete from profession
	where	
		Profession	=	:Prof.Profession;

	return(1);
}


int	DbOpenMemberVisitsCursor(	TMemberId	*MemberId,
					TSCounter	Quarter,
					TAuthCancelCode CancelCode)
{
EXEC SQL BEGIN DECLARE SECTION;
	static	TAuthRecord Auth;
EXEC SQL END DECLARE SECTION;

	Auth.MemberId		=	*MemberId;
	Auth.AuthQuarter	=	Quarter;
	Auth.Cancled		=	CancelCode;

	EXEC SQL declare	MemberVisits	cursor	for

	select
		IdCode, 		IdNumber,	 CardIssueDate,
		/* InsertTime, old */
		SesDate,		SesTime,
		GnrlElegebility,	DentalElegebility,
		Contractor, 		ContractType,
		ProfessionGroup,	AuthQuarter,
		AuthDate, 		AuthTime,	 AuthError,
		ValidUntil,		TermId,
		Location,		Locationprv,
		AuthType,
		VisitCounter,		Cancled
	from	auth
	where	
		IdCode		=	:Auth.MemberId.Code 	and
		IdNumber	=	:Auth.MemberId.Number	and
		AuthQuarter 	=	:Auth.AuthQuarter	and
		Cancled 	<=	:Auth.Cancled       
	order	by	SesDate desc,SesTime desc;

	// DonR 13Mar2005: Try avoiding record-locking errors by performing "dirty reads".
	EXEC SQL
		SET ISOLATION TO DIRTY READ;

	EXEC SQL open	MemberVisits;

	JUMP_ON_ERR;

	return(1);
}

int	DbFetchMemberVisitsCursor( TAuthRecord *Ptr)
{
EXEC SQL BEGIN DECLARE SECTION;
	TAuthRecord	*Auth	=	Ptr;
EXEC SQL END DECLARE SECTION;

	EXEC SQL fetch	MemberVisits	into
		:Auth->MemberId.Code,		:Auth->MemberId.Number,
		:Auth->CardIssueDate,
/*		:Auth->InsertTime, old 		*/
		:Auth->SesTime.DbDate, 		:Auth->SesTime.DbTime,
		:Auth->GnrlElegebility,		:Auth->DentalElegebility,
		:Auth->Contractor, 		:Auth->ContractType,
		:Auth->ProfessionGroup,		:Auth->AuthQuarter,
		:Auth->AuthTime.DbDate,		:Auth->AuthTime.DbTime,
		:Auth->AuthError, 		:Auth->ValidUntil,
		:Auth->TermId, 			:Auth->Location,
		:Auth->Locationprv,
		:Auth->AuthType,		:Auth->VisitCounter,
		:Auth->Cancled;

	JUMP_ON_ERR;

	return(1);
}

int	DbCloseMemberVisitsCursor(void)
{
	// DonR 13Mar2005: Return to normal isolation level.
	EXEC SQL
		SET ISOLATION TO COMMITTED READ;

	EXEC SQL close	MemberVisits;
	return(1);
}


int	DbOpenFirstVisitsCursor( 	TIdNumber	Contractor,
					TContractType	ContractType,
					TSCounter	Quarter)
{
EXEC SQL BEGIN DECLARE SECTION;
	static	TAuthRecord Auth;
EXEC SQL END DECLARE SECTION;

	Auth.Contractor		=	Contractor;
	Auth.ContractType	=	ContractType;
	Auth.AuthQuarter	=	Quarter;

	EXEC SQL declare	ContractorVisits	cursor	for

	select
		IdCode, 		IdNumber,	 CardIssueDate,
		SesDate,	 	SesTime,
		GnrlElegebility, 	DentalElegebility,
		Contractor, 		ContractType,
		ProfessionGroup, 	AuthQuarter,
		AuthDate, 		AuthTime,
		AuthError, 		ValidUntil,
		TermId, 		Location,
		Locationprv,
		AuthType, 		VisitCounter
	from	auth
	where	
		Contractor	=	:Auth.Contractor	and
		ContractType	=	:Auth.ContractType	and
		AuthQuarter 	=	:Auth.AuthQuarter	and
		Cancled 	<=	1;

	// DonR 13Mar2005: Try avoiding record-locking errors by performing "dirty reads".
	EXEC SQL
		SET ISOLATION TO DIRTY READ;

	EXEC SQL open	ContractorVisits;

	JUMP_ON_ERR;

	return(1);
}


int	DbFetchFirstVisitsCursor( TAuthRecord *Ptr)
{
EXEC SQL BEGIN DECLARE SECTION;
	TAuthRecord	*Auth	=	Ptr;
EXEC SQL END DECLARE SECTION;

	EXEC SQL fetch	ContractorVisits	into
		:Auth->MemberId.Code, 		:Auth->MemberId.Number,
		:Auth->CardIssueDate, 
		:Auth->SesTime.DbDate, 		:Auth->SesTime.DbTime,
		:Auth->GnrlElegebility, 	:Auth->DentalElegebility,
		:Auth->Contractor, 		:Auth->ContractType,
		:Auth->ProfessionGroup, 	:Auth->AuthQuarter,
		:Auth->AuthTime.DbDate, 	:Auth->AuthTime.DbTime,
		:Auth->AuthError, 		:Auth->ValidUntil,
		:Auth->TermId, 			:Auth->Location,
		:Auth->Locationprv,
		:Auth->AuthType,		:Auth->VisitCounter;

	JUMP_ON_ERR;

	return(1);
}

int	DbCloseFirstVisitsCursor(void)
{
	// DonR 13Mar2005: Return to normal isolation level.
	EXEC SQL
		SET ISOLATION TO COMMITTED READ;

	EXEC SQL close	ContractorVisits;
	return(1);
}

int	DbInsertAuthRecord( TAuthRecord *Ptr)
{
EXEC SQL BEGIN DECLARE SECTION;
	TAuthRecord *Auth	=	Ptr;
EXEC SQL END DECLARE SECTION;

//	if(	Auth->Contractor ==	10	)	return(1);
	if(	Auth->MemberId.Number	==	18	)	return(1);

	if(	Auth->Cancled	==	0	)
	{
	EXEC SQL BEGIN DECLARE SECTION;
	    TIdNumber	Contractor = 0;
	EXEC SQL END DECLARE SECTION;

	if(!UrgentFlag)
	{
		// DonR 13Mar2005: Try avoiding record-locking errors by performing "dirty reads".
		EXEC SQL
			SET ISOLATION TO DIRTY READ;

	    EXEC SQL select   min( Contractor)  into :Contractor
	    from	auth
	    where	
		Contractor  =	:Auth->Contractor	and
		IdCode	    =	:Auth->MemberId.Code	and
		idnumber    =	:Auth->MemberId.Number	and
		AuthQuarter =	:Auth->AuthQuarter	and
		Cancled	    <=	1;

	    if( SQLERR_code_cmp( SQLERR_not_found  ) != MAC_TRUE )   
	    {
		JUMP_ON_ERR;
	    }
	    if ( Contractor == Auth->Contractor )
//if(SQLCODE==0)
	    {
		Auth->Cancled	=	MacAuthCancelSecond;
	    }

		// DonR 13Mar2005: Return to normal isolation level.
		EXEC SQL
			SET ISOLATION TO COMMITTED READ;

	}
	}

	EXEC SQL insert into	auth
	(
		IdCode, 		IdNumber, 	CardIssueDate,
		SesDate, 		SesTime, 	
		GnrlElegebility, 	DentalElegebility,
		Contractor, 		ContractType,
		ProfessionGroup, 	AuthQuarter,
		AuthDate, 		AuthTime,
		AuthError, 		ValidUntil,
		TermId, 		Location,
		AuthType, 		VisitCounter,
		Cancled, 		Position,
		Paymenttype, 		Locationprv,
		Credittype,		Tax
	)	values
	(
		:Auth->MemberId.Code, 	:Auth->MemberId.Number,
		:Auth->CardIssueDate,
		:Auth->SesTime.DbDate, 	:Auth->SesTime.DbTime,
		:Auth->GnrlElegebility, :Auth->DentalElegebility,
		:Auth->Contractor, 	:Auth->ContractType,
		:Auth->ProfessionGroup, :Auth->AuthQuarter,
		:Auth->AuthTime.DbDate, :Auth->AuthTime.DbTime,
		:Auth->AuthError, 	:Auth->ValidUntil,
		:Auth->TermId, 		:Auth->Location,
		:Auth->AuthType, 	:Auth->VisitCounter,
		:Auth->Cancled, 	:Auth->Position,
		:Auth->PaymentType, 	:Auth->Locationprv,
		:Auth->CreditType,	:Auth->CreditTypeSend
	);

	JUMP_ON_ERR;

	return(1);
}

int	DbDeleteAuthRecord( 	TIdNumber	Contractor,
				TMemberId	*MemberId,
				TSCounter	Quarter)
{
EXEC SQL BEGIN DECLARE SECTION;
	TAuthRecord	Auth;
EXEC SQL END DECLARE SECTION;

		Auth.MemberId		=	*MemberId;
		Auth.Contractor		=	Contractor;
		Auth.AuthQuarter	=	Quarter;

	EXEC SQL delete from	auth
	where	
		Contractor	=	:Auth.Contractor	and
		IdCode		=	:Auth.MemberId.Code	and
		IdNumber	=	:Auth.MemberId.Number	and
		AuthQuarter 	=	:Auth.AuthQuarter;

	return(1);
}

int	DbGetSubstRecord( 	TIdNumber	Contractor,
				TContractType	ContractType,
				TIdNumber	AltContractor,
				TSubstRecord	*Ptr)
{
EXEC SQL BEGIN DECLARE SECTION;
	TSubstRecord	 *Subst =	Ptr;
EXEC SQL END DECLARE SECTION;
		Subst->Contractor	=	Contractor;
		Subst->ContractType 	=	ContractType;
		Subst->AltContractor	=	AltContractor;

	EXEC SQL select
		SubstType, 	DateFrom, 	DateUntil, 	SubstReason,
		LastUpdateDate, LastUpdateTime, UpdatedBy
	into
		:Subst->SubstType, 		:Subst->DateFrom, 	
		:Subst->DateUntil, 		:Subst->SubstReason,
		:Subst->LastUpdate.DbDate, 	:Subst->LastUpdate.DbTime,
		:Subst->UpdatedBy
	from	subst
	where	
		Contractor	=	:Subst->Contractor	and
		ContractType	=	:Subst->ContractType	and
		AltContractor	=	:Subst->AltContractor;

        if( SQLERR_code_cmp( SQLERR_not_found  ) == MAC_TRUE )   
	{
	return (0);
	}
	
	if ( SQLERR_error_test() )
	{
	return (0);
	}
	else
	{
	return(1);
	}

}

int	DbDeleteSubstRecordsByType( 	TIdNumber	Contractor,
					TContractType	ContractType,
					TSubstType	SubstType)
{
EXEC SQL BEGIN DECLARE SECTION;
	TSubstRecord	Subst;
EXEC SQL END DECLARE SECTION;
		Subst.Contractor	=	Contractor;
		Subst.ContractType	=ContractType;
		Subst.SubstType 	=	SubstType;

	EXEC SQL delete from	subst
	where	
		Contractor	=	:Subst.Contractor	and
		ContractType	=	:Subst.ContractType	and
		SubstType	=	:Subst.SubstType;

	JUMP_ON_ERR;

	return(1);
}
/* Y 20080131
int	DbInsertSubstRecord( TSubstRecord	*Ptr)
{
EXEC SQL BEGIN DECLARE SECTION;
	TSubstRecord	 *Subst =	Ptr;
EXEC SQL END DECLARE SECTION;

	EXEC SQL insert into	subst
	(
		Contractor, 	ContractType, 		AltContractor, 
		SubstType, 	DateFrom, 		DateUntil, 
		SubstReason, 
		LastUpdateDate, LastUpdateTime, 	UpdatedBy
	)	
	values
	(
		:Subst->Contractor, 		:Subst->ContractType, 	
		:Subst->AltContractor, 		:Subst->SubstType, 	
		:Subst->DateFrom, 		:Subst->DateUntil,
		:Subst->SubstReason, 		:Subst->LastUpdate.DbDate, 
		:Subst->LastUpdate.DbTime, 	:Subst->UpdatedBy
	);

	if ( SQLERR_error_test() )
	{
	return (0);
	}
	else
	{
	return(1);
	}

}

int	DbUpdateSubstRecord( 	TIdNumber	Contractor,
				TContractType	ContractType,
				TIdNumber	AltContractor,
				TSubstRecord	*Ptr)
{
EXEC SQL BEGIN DECLARE SECTION;
	TSubstRecord	 *Subst =	Ptr;
EXEC SQL END DECLARE SECTION;
		Subst->Contractor	=	Contractor;
		Subst->ContractType 	=	ContractType;
		Subst->AltContractor	=	AltContractor;

	EXEC SQL update subst	set
		SubstType	=	:Subst->SubstType,
		DateFrom	=	:Subst->DateFrom,
		DateUntil	=	:Subst->DateUntil,
		SubstReason 	=	:Subst->SubstReason,
		LastUpdateDate	=	:Subst->LastUpdate.DbDate,
		LastUpdateTime	=	:Subst->LastUpdate.DbTime,
		UpdatedBy	=	:Subst->UpdatedBy
	where	
		Contractor	=	:Subst->Contractor	and
		ContractType	=	:Subst->ContractType	and
		AltContractor	=	:Subst->AltContractor;

	if ( SQLERR_error_test() )
	{
	return (0);
	}
	else
	{
	return(1);
	}
}

*/
int	DbDeleteSubstRecord( 	TIdNumber	Contractor,
				TContractType	ContractType,
				TIdNumber	AltContractor)
{
EXEC SQL BEGIN DECLARE SECTION;
	TSubstRecord	 Subst;
EXEC SQL END DECLARE SECTION;
		Subst.Contractor	=	Contractor;
		Subst.ContractType	=	ContractType;
		Subst.AltContractor	=	AltContractor;

	EXEC SQL delete from subst
	where	
		Contractor		=	:Subst.Contractor	and
		ContractType		=	:Subst.ContractType	and
		AltContractor		=	:Subst.AltContractor;
	
	return(1);
}

int DbOpenVisitsInsCursor( TVisitRecord	*Ptr)
{
EXEC SQL BEGIN DECLARE SECTION;
	TVisitRecord	*Visit =	Ptr;
EXEC SQL END DECLARE SECTION;

	EXEC SQL declare	VisitsInsCursor cursor for
	insert	into visit
	(
		Contractor, 	ContractType,
		IdCode, 	IdNumber, 	CardIssueDate,
		AuthDate, 	AuthTime, 	GnrlElegebility, 	
		Observation,
		Action0, 	Action1, 	Action2, 	Action3,
		PayingInst, 	TermId, 	Profession,
		Location, 	Locationprv,
		VisitCounter, 	Position, 	PaymentType
	)	
	values
	(
		:Visit->Contractor, 		:Visit->ContractType,
		:Visit->MemberId.Code, 		:Visit->MemberId.Number,
		:Visit->CardIssueDate,
		:Visit->AuthTime.DbDate, 	:Visit->AuthTime.DbTime,
		:Visit->GnrlElegebility, 	:Visit->Observation,
		:Visit->Action[0], 		:Visit->Action[1],
		:Visit->Action[2], 		:Visit->Action[3],
		:Visit->PayingInst, 		:Visit->TermId,
		:Visit->Profession,
		:Visit->Location, 		:Visit->Locationprv,
		:Visit->VisitCounter, 		:Visit->Position,
		:Visit->PaymentType
	);
	EXEC SQL open	VisitsInsCursor;

	JUMP_ON_ERR;

	return(1);
}


int DbPutVisitsInsCursor(void)
{
	if( strlen(VisitRecord.Observation) != 9	)
	{
	memset(VisitRecord.Observation,32,sizeof(VisitRecord.Observation));
	}
	EXEC SQL put	VisitsInsCursor;

	if(	SQLCODE ==	-391	)
	{
	printf("insert into visit -391 error\n");
	}

	JUMP_ON_ERR;
	
	return(1);
}


int DbCloseVisitsInsCursor(void)
{
	EXEC SQL Close	VisitsInsCursor;
	return(1);
}


int DbDeleteCurrentShiftsSelCursor(void)
{
	EXEC SQL delete from shift 
	where 
		current of ShiftsSelCursor;
	
	JUMP_ON_ERR;

	return(1);
}

int DbOpenShiftsInsCursor( TShiftRecord	*Ptr)
{
EXEC SQL BEGIN DECLARE SECTION;
	TShiftRecord	*Shift	=	Ptr;
EXEC SQL END DECLARE SECTION;

	EXEC SQL declare	ShiftsInsCursor	cursor	for
	insert	into	shift
	(
		TermId, 	Contractor, 	ContractType,
		ShiftStatus, 	StatusDate, 	StatusTime,
		Error, 		
		Location, 	Locationprv,
		Position, 	Paymenttype
	)	
	values
	(
		:Shift->TermId, 		:Shift->Contractor,
		:Shift->ContractType, 		:Shift->ShiftStatus,
		:Shift->StatusTime.DbDate, 	:Shift->StatusTime.DbTime,
		:Shift->Error, 		
		:Shift->Location, 		:Shift->Locationprv,
		:Shift->Position, 		:Shift->PaymentType
	);

	EXEC SQL open	ShiftsInsCursor;

	JUMP_ON_ERR;

	return(1);
}

int DbPutShiftsInsCursor(void)
{
	EXEC SQL put	ShiftsInsCursor;

	JUMP_ON_ERR;

	return(1);
}


int DbCloseShiftsInsCursor(void)
{
	EXEC SQL Close	ShiftsInsCursor;
	if ( SQLERR_error_test() )
	{
		return (0);
	}
	else
	{
		return(1);
	}
}


int DbDeleteCurrentPresenceSelCursor(void)
{
	EXEC SQL delete from Presence 
	where 
		current of PresenceSelCursor;
	return(1);
}

int DbOpenPresenceInsCursor( TPresenceRecord	*Ptr)
{
EXEC SQL BEGIN DECLARE SECTION;
	TPresenceRecord	*Presence	=	Ptr;
EXEC SQL END DECLARE SECTION;

	EXEC SQL declare	PresenceInsCursor	cursor	for
	insert	into	Presence
	(
		Employee, 		Action, 	
		ActionDate, 		ActionTime, 	
		TermId
	)	
	values
	(
		:Presence->Employee, 		:Presence->Action,
		:Presence->ActionTime.DbDate, 	:Presence->ActionTime.DbTime,
		:Presence->TermId
	);
	EXEC SQL open	PresenceInsCursor;
	
	JUMP_ON_ERR;

	return(1);
}


int DbPutPresenceInsCursor(void)
{
	EXEC SQL put	PresenceInsCursor;
	
	JUMP_ON_ERR;

	return(1);
}


int DbClosePresenceInsCursor(void)
{
	EXEC SQL Close	PresenceInsCursor;
	return(1);
}


int	DbDeletePcStatRecord( 	TTermId TermIdPar,
				TIdNumber	ContractorPar,
				TContractType	ContractTypePar)
{
EXEC SQL BEGIN DECLARE SECTION;
	TTermId	TermId 		=	TermIdPar;
	TIdNumber	Contractor	=	ContractorPar;
	TContractType	ContractType	=	ContractTypePar;
EXEC SQL END DECLARE SECTION;

	EXEC SQL delete from PcStat
	where 
		TermId		=	:TermId 	and	
		Contractor	=	:Contractor 	and 
		ContractType	=	:ContractType;

	JUMP_ON_ERR;

	return(1);
}


int	DbInsertPcStatRecord( TPcStatRecord	*Ptr)
{
EXEC SQL BEGIN DECLARE SECTION;
	TPcStatRecord	*PcStat =	Ptr;
	char v_host_name[9];
	int v_location;
EXEC SQL END DECLARE SECTION;
	char host_name[20];

FILE *HNAME;

	system("hostname > /pharm/hostname.out");
	HNAME=fopen("/pharm/hostname.out","r");
	fscanf(HNAME,"%s",host_name);
	fclose (HNAME);				// DonR 08Nov2004
	strncpy(v_host_name,host_name,8);
	v_host_name[8] = (char)0;	// DonR 08Nov2004: strncpy doesn't supply termination!
								// This lack of string termination was causing server
								// crashes.
	printf ("\n testhostname[%s]",v_host_name);

	EXEC SQL select location into :v_location from conterm
		where contractor = :PcStat->Data.Contractor
		and contracttype = :PcStat->Data.ContractType
		and termid = :PcStat->TermId;

	if (sqlca.sqlcode ) v_location = 0;

	PcStat->LastUpdate	=	CurrDateTime;


	EXEC SQL Insert into	pcstat
	(
		TermId, 	Contractor, 		ContractType,
		DClicks, 	DMaccabi, 		DMacCom,
		DMacRep, 	DUpdate, 		DLastVer,
		DBackup, 	DiskSpace, 		DataSize,
		HdrSize, 	LabSize, 		LogSize,
		MacabSize, 	DosVer, 		Res1,
		Res2, 		Res3, 			Res4,
		LastUpdate, location,		compname
	)	
	values
	(
		:PcStat->TermId, 		:PcStat->Data.Contractor,
		:PcStat->Data.ContractType, 	:PcStat->Data.DClicks,
		:PcStat->Data.DMaccabi, 	:PcStat->Data.DMacCom,
		:PcStat->Data.DMacRep, 		:PcStat->Data.DUpdate,
		:PcStat->Data.DLastVer, 	:PcStat->Data.DBackup,
		:PcStat->Data.DiskSpace, 	:PcStat->Data.DataSize,
		:PcStat->Data.HdrSize, 		:PcStat->Data.LabSize,
		:PcStat->Data.LogSize, 		:PcStat->Data.MacabSize,
		:PcStat->Data.DosVer, 		:PcStat->Data.Res1,
		:PcStat->Data.Res2, 		:PcStat->Data.Res3,
		:PcStat->Data.Res4, 		:PcStat->LastUpdate.DbDate,
		:v_location,				:v_host_name
	);

	
	JUMP_ON_ERR;

	return(1);
}

int	DbGetPcStatRecord( 	TTermId 	TermId,
				TPcStatRecord	*Ptr)
{
EXEC SQL BEGIN DECLARE SECTION;
	TPcStatRecord	*PcStat =	Ptr;
EXEC SQL END DECLARE SECTION;

	PcStat->TermId	=	TermId;

	EXEC SQL select
		TermId, 	Contractor, 		ContractType,
		DClicks, 	DMaccabi, 		DMacCom,
		DMacRep, 	DUpdate, 		DLastVer,
		DBackup, 	DiskSpace, 		DataSize,
		HdrSize, 	LabSize, 		LogSize,
		MacabSize, 	DosVer, 		Res1,
		Res2, 		Res3, 			Res4
	into
		:PcStat->TermId, 		:PcStat->Data.Contractor,
		:PcStat->Data.ContractType, 	:PcStat->Data.DClicks,
		:PcStat->Data.DMaccabi, 	:PcStat->Data.DMacCom,
		:PcStat->Data.DMacRep, 		:PcStat->Data.DUpdate,
		:PcStat->Data.DLastVer, 	:PcStat->Data.DBackup,
		:PcStat->Data.DiskSpace, 	:PcStat->Data.DataSize,
		:PcStat->Data.HdrSize, 		:PcStat->Data.LabSize,
		:PcStat->Data.LogSize, 		:PcStat->Data.MacabSize,
		:PcStat->Data.DosVer, 		:PcStat->Data.Res1,
		:PcStat->Data.Res2, 		:PcStat->Data.Res3,
		:PcStat->Data.Res4
	from	PcStat	
	where
		TermId	=	:PcStat->TermId;

	JUMP_ON_ERR;

	return(1);
}

int	DbOpenContractLocCursor( TIdNumber	Contractor)
{
EXEC SQL BEGIN DECLARE SECTION;
	static	TIdNumber	Id;
EXEC SQL END DECLARE SECTION;
	Id	=	Contractor;

	EXEC SQL declare	ContractLoc	cursor	for
	select
		TermId, 	Contractor, 		ContractType,
		AuthType, 	ValidFrom, 		ValidUntil,
		Location, 	LastUpdateDate, 	LastUpdateTime,
		UpdatedBy,	Locationprv
	from	conterm
	where	
		Contractor	=	:Id;

	EXEC SQL Open	ContractLoc;

	return(1);
}

int	DbFetchContractLocCursor( TContermRecord	*Ptr)
{
EXEC SQL BEGIN DECLARE SECTION;
	TContermRecord	*Conterm	=	Ptr;
EXEC SQL END DECLARE SECTION;

	EXEC SQL Fetch	ContractLoc	into
		:Conterm->TermId, 		:Conterm->Contractor,
		:Conterm->ContractType,	 	:Conterm->AuthType,
		:Conterm->ValidFrom, 		:Conterm->ValidUntil,
		:Conterm->Location, 		:Conterm->LastUpdate.DbDate,
		:Conterm->LastUpdate.DbTime, 	:Conterm->UpdatedBy,
		:Conterm->Locationprv;
	return(1);
}

int	DbCloseContractLocCursor(void)
{
	EXEC SQL Close	ContractLoc;
	return(1);
}

int	DbOpenContractSubstCursor( TIdNumber	Contractor)
{
EXEC SQL BEGIN DECLARE SECTION;
	static	TIdNumber	Id;
EXEC SQL END DECLARE SECTION;
	Id	=	Contractor;

	EXEC SQL declare	ContractSubst	cursor	for
	select
		Contractor, 		ContractType, 		AltContractor,
		SubstType, 		DateFrom, 		DateUntil,
		SubstReason, 		LastUpdateDate, 	LastUpdateTime,
		UpdatedBy
	from	subst
	where	
		Contractor	=	:Id;

	EXEC SQL Open	ContractSubst;

	return(1);
}

int	DbFetchContractSubstCursor( TSubstRecord	*Ptr)
{
EXEC SQL BEGIN DECLARE SECTION;
	TSubstRecord	*Subst	=	Ptr;
EXEC SQL END DECLARE SECTION;
	EXEC SQL Fetch	ContractSubst	into
		:Subst->Contractor, 		:Subst->ContractType,
		:Subst->AltContractor, 		:Subst->SubstType,
		:Subst->DateFrom, 		:Subst->DateUntil,
		:Subst->SubstReason, 		:Subst->LastUpdate.DbDate,
		:Subst->LastUpdate.DbTime, 	:Subst->UpdatedBy;
	return(1);
}

int	DbCloseContractSubstCursor(void)
{
	EXEC SQL Close	ContractSubst;
	return(1);
}

int	DbOpenProfGroupsCursor(void)
{

	EXEC SQL declare	ProfGroups	cursor	for
	select
		Profession, 	Group, 		Descr,
		AcceptUrgent, 	SexesAllowed, 	MinAgeAllowed,
		MaxAgeAllowed, 	LastUpdateDate,	LastUpdateTime,
		UpdatedBy
	from	Profession
	order	by	
		Group , 	Profession;

	EXEC SQL Open	ProfGroups;
	return(1);
}

int DbFetchProfsGroupCursor( TProfessionRecord	*Ptr)
{
EXEC SQL BEGIN DECLARE SECTION;
	TProfessionRecord	*Prof	=	Ptr;
EXEC SQL END DECLARE SECTION;

	EXEC SQL Fetch	ProfGroups	into
		:Prof->Profession, 		:Prof->Group,
		:Prof->Descr, 			:Prof->AcceptUrgent,
		:Prof->SexesAllowed, 		:Prof->MinAgeAllowed,
		:Prof->MaxAgeAllowed, 		:Prof->LastUpdate.DbDate,
		:Prof->LastUpdate.DbTime, 	:Prof->UpdatedBy;
	return(1);
}

int	DbCloseProfGroupsCursor(void)
{
	EXEC SQL Close	ProfGroups;
	return(1);
}


int	DbOpenTerminalLocCursor( TTermId TermId)
{
EXEC SQL BEGIN DECLARE SECTION;
	static	TTermId	Id;
EXEC SQL END DECLARE SECTION;
	Id	=	TermId;

	EXEC SQL declare	TerminalLoc	cursor	for
	select
		TermId, 	Contractor, 		ContractType,
		AuthType, 	ValidFrom, 		ValidUntil,
		Location, 	LastUpdateDate, 	LastUpdateTime,
		UpdatedBy,	Locationprv
	from	conterm
	where	
		TermId		=	:Id;

	EXEC SQL Open	TerminalLoc;
	return(1);
}

int	DbFetchTerminalLocCursor( TContermRecord	*Ptr)
{
EXEC SQL BEGIN DECLARE SECTION;
	TContermRecord	*Conterm	=	Ptr;
EXEC SQL END DECLARE SECTION;
	EXEC SQL Fetch	TerminalLoc	into
		:Conterm->TermId, 		:Conterm->Contractor,
		:Conterm->ContractType, 	:Conterm->AuthType,
		:Conterm->ValidFrom, 		:Conterm->ValidUntil,
		:Conterm->Location, 		:Conterm->LastUpdate.DbDate,
		:Conterm->LastUpdate.DbTime, 	:Conterm->UpdatedBy,
		:Conterm->Locationprv;
	return(1);
}

int	DbCloseTerminalLocCursor(void)
{
	EXEC SQL Close	TerminalLoc;
	return(1);
}


int	DbOpenTermPhoneCursor( 	TTermId	TermId,
				int	Phone)
{
EXEC SQL BEGIN DECLARE SECTION;
	static	TTermId	Id;
	static int	Phone1;
	static int	Phone2;
EXEC SQL END DECLARE SECTION;

	Id	=	TermId;
	if(	Phone	==	0	)
	{
	Phone1	=	0;
	Phone2	=	999999999;
	} else
	{
	Phone1	=	Phone;
	Phone2	=	Phone;
	}

	EXEC SQL Declare	TermPhone	Cursor	for
	select
		TermId, 		AreaCode, 	
		City, 			ModemPrefix, 
		PhonePrefix0, 		PhoneNumber0,
		PhonePrefix1, 		PhoneNumber1,
		PhonePrefix2, 		PhoneNumber2
	from	Terminal
	where	
	TermId	>	:Id 	and
		PhoneNumber0	between :Phone1 and :Phone2
	Order	By	TermId;

	EXEC SQL Open	TermPhone;
	return(1);
}

int	DbFetchTermPhoneCursor( TTerminalRecord	*Ptr)
{
EXEC SQL BEGIN DECLARE SECTION;
	TTerminalRecord	*Terminal	=	Ptr;
EXEC SQL END DECLARE SECTION;
	
	EXEC SQL Fetch	TermPhone	into
		:Terminal->TermId, 		:Terminal->AreaCode,
		:Terminal->City, 		:Terminal->ModemPrefix,
		:Terminal->Phone[0].Prefix, 	:Terminal->Phone[0].Number,
		:Terminal->Phone[1].Prefix, 	:Terminal->Phone[1].Number,
		:Terminal->Phone[2].Prefix, 	:Terminal->Phone[2].Number;
	return(1);
}
int	DbCloseTermPhoneCursor(void)
{
	EXEC SQL Close	TermPhone;
	return(1);
}


int	DbUpdateTermPhone( TTerminalRecord *Ptr)
{
EXEC SQL BEGIN DECLARE SECTION;
	TTerminalRecord *Terminal	=	Ptr;
EXEC SQL END DECLARE SECTION;

	EXEC SQL update terminal	set
		AreaCode	=	:Terminal->AreaCode,
		ModemPrefix	=	:Terminal->ModemPrefix,
		PhonePrefix0	=	:Terminal->Phone[0].Prefix,
		PhoneNumber0	=	:Terminal->Phone[0].Number,
		PhonePrefix1	=	:Terminal->Phone[1].Prefix,
		PhoneNumber1	=	:Terminal->Phone[1].Number,
		PhonePrefix2	=	:Terminal->Phone[2].Prefix,
		PhoneNumber2	=	:Terminal->Phone[2].Number
	where	
		TermId		=	:Terminal->TermId;
	return(1);
}

int	DbOpenTempMembCursor( 	TMemberId	*MemberId,
				TDateYYYYMMDD	ValidUntil)
{
EXEC SQL BEGIN DECLARE SECTION;
	static TTempMembRecord	Temp;
EXEC SQL END DECLARE SECTION;

	Temp.MemberId	=	*MemberId;
	Temp.ValidUntil =	ValidUntil;

	EXEC SQL declare	TempCards	cursor	for
	select
		CardId, 		IdCode, 
		IdNumber, 		ValidUntil,
		LastUpdateDate, 	LastUpdateTime,
		UpdatedBy
	from	tempmemb
	where	
		IdCode		=	:Temp.MemberId.Code 	and
		IdNumber	=	:Temp.MemberId.Number	and
		ValidUntil	>=	:Temp.ValidUntil;

	EXEC SQL Open	TempCards;
	return(1);
}

int	DbFetchTempMembCursor( TTempMembRecord	*Ptr)
{
EXEC SQL BEGIN DECLARE SECTION;
	TTempMembRecord *Temp	=	Ptr;
EXEC SQL END DECLARE SECTION;

	EXEC SQL Fetch	TempCards	Into
		:Temp->CardId, 			:Temp->MemberId.Code,
		:Temp->MemberId.Number, 	:Temp->ValidUntil,
		:Temp->LastUpdate.DbDate, 	:Temp->LastUpdate.DbTime,
		:Temp->UpdatedBy;
	return(1);
}

int	DbCloseTempMembCursor(void)
{
	EXEC SQL Close	TempCards;
	return(1);
}


int DbOpenLogDInsCursor( TLogRecord	*Ptr)
{
EXEC SQL BEGIN DECLARE SECTION;
	TLogRecord	*Log =	Ptr;
EXEC SQL END DECLARE SECTION;

	EXEC SQL declare	LogDInsCursor cursor for
	insert	into AuthLog
	(
		ProcId,  	SessionId,	
		SesDate, 	SesTime,
		LogDate,	LogTime,
		MdxDt, 		MdxTm, 		RspTime,
		MsgCode, 	Error, 		TermId,
		Contractor, 	ContractType, 	MemberId,
		MemberCode, 	Location, 	Position,
		Paymenttype,	Locationprv
	)	
	values
	(
		:Log->Hdr.ProcId, 		:Log->Hdr.SessionId, 		
		:Log->Hdr.SesTime.DbDate, 	:Log->Hdr.SesTime.DbTime,
		:Log->Hdr.LogTime.DbDate, 	:Log->Hdr.LogTime.DbTime,
		:Log->Hdr.MdxTime.DbDate, 	:Log->Hdr.MdxTime.DbTime, 
		:Log->Hdr.RspTime,
		:Log->Hdr.MsgCode, 		:Log->Hdr.Error,
		:Log->Data.TermId, 		:Log->Data.Contractor,
		:Log->Data.ContractType, 	:Log->Data.MemberId.Number,
		:Log->Data.MemberId.Code, 	:Log->Data.Location,
		:Log->Data.Position, 		:Log->Data.PaymentType,
		:Log->Data.Locationprv
	);
	EXEC SQL set	buffered	log;
	EXEC SQL open	LogDInsCursor;
	
	JUMP_ON_ERR;

	return(1);
}


int DbPutLogDInsCursor(void)
{
	EXEC SQL put	LogDInsCursor;
	JUMP_ON_ERR;
	return(1);
}


int DbCloseLogDInsCursor(void)
{
	EXEC SQL Close	LogDInsCursor;
	return(1);
}

int DbOpenTempMembDInsCursor( TTempMembRecord	*Ptr)
{
EXEC SQL BEGIN DECLARE SECTION;
	TTempMembRecord	*Temp =	Ptr;
EXEC SQL END DECLARE SECTION;

	EXEC SQL declare	TempMInsCursor cursor with hold for
	insert	into TempMemb
	(
		CardId, 		IdCode, 		IdNumber,
		ValidUntil,	 	LastUpdateDate, 	LastUpdateTime,
		UpdatedBy
	)	
	values
	(
		:Temp->CardId, 			:Temp->MemberId.Code,
		:Temp->MemberId.Number, 	:Temp->ValidUntil,
		:Temp->LastUpdate.DbDate, 	:Temp->LastUpdate.DbTime,
		:Temp->UpdatedBy
	);

	EXEC SQL set	buffered	log;
	EXEC SQL open	TempMInsCursor;
	return(1);
}


int DbPutTempMembInsCursor(void)
{
	EXEC SQL put	TempMInsCursor;
	return(1);
}


int DbCloseTempMembInsCursor(void)
{
	EXEC SQL Close	TempMInsCursor;
	return(1);
}

TSCounter   DbGetContractVisitsCountByLocation( TIdNumber	ContractorPar,
						TContractType	ContractTypePar,
						TLocationId	LocationPar,
						TSCounter	QuarterPar)
{
EXEC SQL BEGIN DECLARE SECTION;
	TSCounter	Quarter 	=	QuarterPar;
	TCounter	Count		=	0;
	TIdNumber	Contractor	=	ContractorPar;
	TLocationId	Location	=	LocationPar;
	TContractType	ContractType	=	ContractTypePar;
EXEC SQL END DECLARE SECTION;

	EXEC SQL select 
		count (*) Into	:Count
	from	Auth
	where	
		Contractor	=	:Contractor 	and
		ContractType	=	:ContractType	and
		AuthQuarter	=	:Quarter	and
		Location	=	:Location	and
		Cancled 	<=	1;
	return(1);
}

int DbOpenAuthInsCursor( TAuthRecord	*Ptr)
{
EXEC SQL BEGIN DECLARE SECTION;
	TAuthRecord	*Auth =	Ptr;
EXEC SQL END DECLARE SECTION;

	EXEC SQL declare	AuthInsCursor cursor with hold for
	insert	into Auth
	(
		IdCode, 		IdNumber, 		CardIssueDate,
		SesDate,		SesTime, 	
		GnrlElegebility,	DentalElegebility,
		Contractor, 		ContractType, 		ProfessionGroup,
		AuthQuarter, 		AuthDate, 		AuthTime,
		AuthError, 		ValidUntil, 		TermId,
		Location, 		Locationprv,		AuthType,
		VisitCounter
	)	
	values
	(
		:Auth->MemberId.Code, 		:Auth->MemberId.Number,
		:Auth->CardIssueDate, 		
		:Auth->SesTime.DbDate, 		:Auth->SesTime.DbTime,
		:Auth->GnrlElegebility, 	:Auth->DentalElegebility,
		:Auth->Contractor, 		:Auth->ContractType,
		:Auth->ProfessionGroup, 	:Auth->AuthQuarter,
		:Auth->AuthTime.DbDate, 	:Auth->AuthTime.DbTime,
		:Auth->AuthError, 		:Auth->ValidUntil,
		:Auth->TermId, 			:Auth->Location,
		:Auth->Locationprv,
		:Auth->AuthType, 		:Auth->VisitCounter
	);
	EXEC SQL set	buffered	log;
	EXEC SQL open	AuthInsCursor;

	return(1);
}

int DbPutAuthInsCursor(void)
{
	EXEC SQL put	AuthInsCursor;
	return(1);
}

int DbCloseAuthInsCursor(void)
{
	EXEC SQL Close	AuthInsCursor;
	return(1);
}


int DbDeleteConMembRecord( 	TMemberId	*MemberId,
				TProfession 	Profession)
{
EXEC SQL BEGIN DECLARE SECTION;
	TConMembRecord ConMemb;
EXEC SQL END DECLARE SECTION;

	ConMemb.MemberId	=	*MemberId;
	ConMemb.Profession	=	Profession;

	EXEC SQL delete from	conmemb

	where	
		IdNumber	=	:ConMemb.MemberId.Number	and
		IdCode		=	:ConMemb.MemberId.Code		and
		ConSeq		=	0				and
		Profession	=	:ConMemb.Profession;
	return(1);
}


int DbUpdateConMembRecord( 	TMemberId	*MemberId,
				TProfession 	Profession,
				TConMembRecord	*Ptr)
{
EXEC SQL BEGIN DECLARE SECTION;
	TConMembRecord *ConMemb	=	Ptr;
EXEC SQL END DECLARE SECTION;

	ConMemb->MemberId	=	*MemberId;
	ConMemb->LastUpdate	=	CurrDateTime;
	ConMemb->Profession 	=	Profession;

	EXEC SQL update conmemb
	set
		Contractor	=	:ConMemb->Contractor,
		ContractType	=	:ConMemb->ContractType,
		LinkType	=	:ConMemb->LinkType,
		LastUpdateDate	=	:ConMemb->LastUpdate.DbDate,
		LastUpdateTime	=	:ConMemb->LastUpdate.DbTime,
		ValidFrom	=	:ConMemb->ValidFrom,
		ValidUntil	=	:ConMemb->ValidUntil,
		ConMembNote 	=   	:ConMemb->ConMembNote
	where	
		IdNumber	=	:ConMemb->MemberId.Number	and
		IdCode		=	:ConMemb->MemberId.Code		and
		ConSeq		=	0				and
		Profession	=	:ConMemb->Profession;
	return(1);
}


int DbGetConMembRecord( 	TMemberId	*MemberIdParam,
				TProfession 	ProfessionParam,
				TConMembRecord	*Ptr,short LinkTypeGl)
{
EXEC SQL BEGIN DECLARE SECTION;
	TConMembRecord *ConMemb	=	Ptr;
	short LinkType5 = LinkTypeGl;
	TMemberId	MemberIdSave ;
	short ProfSave;
	short ContractType; /*20010501*/
	int   Contractor;		/*20010501*/
	
EXEC SQL END DECLARE SECTION;
	short Result;
	short flg5=0;
	int DateNow;	
	
	Ptr->MemberId		=	*MemberIdParam;
	Ptr->Profession		=	ProfessionParam;

	MemberIdSave.Number = MemberIdParam->Number;
	MemberIdSave.Code	= MemberIdParam->Code;
	ProfSave			= ProfessionParam;
	DateNow = GetDate();
	

	if (ProfessionParam != 96 )   /* 20010501old format */
	{
/*===============================================================*/
/* Yulia change 07.05.2000                                       */
/* First: program check if exist record with link = 5 (Global)   */
/*        and remember this answer                               */
/* Second:check conmemb	record for profession and return         */
/* TRUE (1) if found or exist link fo 5                          */
/*===============================================================*/
/*First*/

		EXEC SQL declare	ConMembChGlCursor	cursor	for
		select
			IdNumber, 		IdCode, 		ConSeq,
			Contractor, 		ContractType, 		Profession,
			LinkType, 		ValidFrom, 		ValidUntil,
			LastUpdateDate, 	LastUpdateTime,	 	UpdatedBy,
			ConMembNote
		from	ConMemb
		where	
			IdNumber	=	:ConMemb->MemberId.Number	and
			IdCode		=	:ConMemb->MemberId.Code		and
			ConSeq		=	0				and
			LinkType	=	:LinkType5
			order by ValidUntil desc;
		SQLERR_error_test() ;
		
		EXEC SQL Open	ConMembChGlCursor;
		SQLERR_error_test() ;

		EXEC SQL Fetch	ConMembChGlCursor	into
			:ConMemb->MemberId.Number, 	:ConMemb->MemberId.Code,
			:ConMemb->ConSeq, 		:ConMemb->Contractor,
			:ConMemb->ContractType, 	:ConMemb->Profession,
			:ConMemb->LinkType, 		:ConMemb->ValidFrom,
			:ConMemb->ValidUntil, 		:ConMemb->LastUpdate.DbDate,
			:ConMemb->LastUpdate.DbTime, 	:ConMemb->UpdatedBy,
			:ConMemb->ConMembNote;

		if( SQLERR_code_cmp( SQLERR_not_found ) == MAC_TRUE )
		{
			flg5 =0;
		}
		else if( SQLERR_code_cmp( SQLERR_end_of_fetch ) == MAC_TRUE )
		{
			flg5 = 0;
		}
		else  if(  SQLERR_error_test() ) 
		{
			flg5 = 0;
		}
		else if (	(DateNow >= ConMemb->ValidFrom) &&
					(DateNow <= ConMemb->ValidUntil)
					)
		{    
			flg5 = 1;
		}
		else
		{
			flg5 = 0;
		}
	/*	printf ("\n conmemb:DateNow[%d],Contr[%d],idsave[%d]",DateNow,ConMemb->Contractor,MemberIdSave.Number);
		fflush(stdout);
	*/
		EXEC SQL Close	ConMembChGlCursor;
		SQLERR_error_test() ;

	/*Second*/
		EXEC SQL select
			ConSeq, 		Contractor, 		ContractType,
			LinkType, 		ValidFrom, 		ValidUntil,
			LastUpdateDate, 	LastUpdateTime, 	UpdatedBy,
			ConMembNote
		into
			:ConMemb->ConSeq, 		:ConMemb->Contractor,
			:ConMemb->ContractType, 	:ConMemb->LinkType,
			:ConMemb->ValidFrom, 		:ConMemb->ValidUntil,
			:ConMemb->LastUpdate.DbDate, 	:ConMemb->LastUpdate.DbTime,
			:ConMemb->UpdatedBy, 		:ConMemb->ConMembNote
		from	conmemb
		where	
			IdNumber	=	:MemberIdSave.Number	and
			IdCode		=	:MemberIdSave.Code		and
			ConSeq		=	0				and
			Profession	=	:ProfSave;
	/*
		printf ("\n conmemb:Contr[%d],until[%d]",ConMemb->Contractor,ConMemb->ValidUntil);
		fflush(stdout);
	*/
		if( SQLERR_code_cmp( SQLERR_not_found ) == MAC_TRUE )
		{
			if (flg5 > 0 )
			{
				Result =1 ;
			}
			else
			{
				Result = 0;
			}
		}
		else if( ! SQLERR_error_test() ) 
		{
			Result = 1;
		}
		else 
		{
			Result = 0;
			JUMP_ON_ERR;
		}

		return(Result);

	}
	else/*new format 20010501*/
	{
		Contractor = ContractRecord.Contractor;
		ContractType = ContractRecord.ContractType;
//		printf ("\n [%d] Contr[%d]type[%d]",Contractor,ContractType);
		fflush(stdout);
		EXEC SQL select 
			ConSeq, 		Contractor, 		ContractType,
			LinkType, 		ValidFrom, 		ValidUntil,
			LastUpdateDate, 	LastUpdateTime, 	UpdatedBy,
			ConMembNote
		into
			:ConMemb->ConSeq, 		:ConMemb->Contractor,
			:ConMemb->ContractType, 	:ConMemb->LinkType,
			:ConMemb->ValidFrom, 		:ConMemb->ValidUntil,
			:ConMemb->LastUpdate.DbDate, 	:ConMemb->LastUpdate.DbTime,
			:ConMemb->UpdatedBy, 		:ConMemb->ConMembNote
		from	conmemb
		where	
			IdNumber	=	:MemberIdSave.Number	and
			IdCode		=	:MemberIdSave.Code		and
			Profession	=	:ProfSave				and
			Contractor	=	:Contractor				and
			ContractType=	:ContractType;

		if( SQLERR_code_cmp( SQLERR_not_found ) == MAC_TRUE )
		{
				Result = 0;
		}
		else if(  SQLERR_error_test() ) 
		{
			Result = 0;
		}
		else 
		{
			Result = 1;
			JUMP_ON_ERR;
		}

		return(Result);
			
	}
/*
printf ("\n [%d] Contr[%d]id[%d]type[%d]",sqlca.sqlcode,ConMemb->Contractor,
ConMemb->MemberId.Number,ConMemb->LinkType);
fflush(stdout);
*/

}


int DbInsertConMembRecord( TConMembRecord	*Ptr)
{
EXEC SQL BEGIN DECLARE SECTION;
	TConMembRecord *ConMemb	=	Ptr;
	int ConSeqTemp;
EXEC SQL END DECLARE SECTION;

	Ptr->LastUpdate 		=	CurrDateTime;
	Ptr->ConSeq 		=	0;
	if ( ConMemb->Profession == 96 )
	{
		EXEC SQL select max (ConSeq) into :ConSeqTemp from 	conmemb
			where 
			IdNumber	=	:ConMemb->MemberId.Number	and
			IdCode		=	:ConMemb->MemberId.Code		and
			Profession	=	:ConMemb->Profession;
		if ( SQLERR_code_cmp( SQLERR_not_found ) == MAC_TRUE )
		{
		}
		else if(  SQLERR_error_test() ) 
		{
		}
		else
		{
			ConMemb->ConSeq = ConSeqTemp + 1;
		}
	}

	EXEC SQL insert into	conmemb
	(
		IdNumber, 		IdCode, 		ConSeq,
		Contractor, 	ContractType, 	Profession,
		LinkType, 		ValidFrom, 		ValidUntil,
		LastUpdateDate, LastUpdateTime, UpdatedBy,
		ConMembNote
	)	
	values
		(
		:ConMemb->MemberId.Number, 	:ConMemb->MemberId.Code,
		:ConMemb->ConSeq, 			:ConMemb->Contractor,
		:ConMemb->ContractType, 	:ConMemb->Profession,
		:ConMemb->LinkType, 	
		:ConMemb->ValidFrom,		:ConMemb->ValidUntil,
		:ConMemb->LastUpdate.DbDate,:ConMemb->LastUpdate.DbTime,
		:ConMemb->UpdatedBy, 		:ConMemb->ConMembNote
	);

	  if (SQLERR_error_test() )
	  {
		  return (0);
	  }
	  else
	  {
		  return (1);
	  }
}


int	DbOpenConMembSelCursor( TMemberId	*MemberIdParam)
{
EXEC SQL BEGIN DECLARE SECTION;
	static	TMemberId	*MemberId;
EXEC SQL END DECLARE SECTION;
	MemberId	=	MemberIdParam;

	EXEC SQL declare	ConMembCursor	cursor	for
	select
		IdNumber, 		IdCode, 		ConSeq,
		Contractor, 		ContractType, 		Profession,
		LinkType, 		ValidFrom, 		ValidUntil,
		LastUpdateDate, 	LastUpdateTime,	 	UpdatedBy,
		ConMembNote

	from	ConMemb
	where	
		IdNumber	=	:MemberId->Number	and
		IdCode		=	:MemberId->Code;

	EXEC SQL Open	ConMembCursor;
	return(1);
}

int	DbFetchConMembSelCursor( TConMembRecord	*Ptr)
{
EXEC SQL BEGIN DECLARE SECTION;
	TConMembRecord	*ConMemb	=	Ptr;
EXEC SQL END DECLARE SECTION;
	EXEC SQL Fetch	ConMembCursor	into
		:ConMemb->MemberId.Number, 	:ConMemb->MemberId.Code,
		:ConMemb->ConSeq, 		:ConMemb->Contractor,
		:ConMemb->ContractType, 	:ConMemb->Profession,
		:ConMemb->LinkType, 		:ConMemb->ValidFrom,
		:ConMemb->ValidUntil, 		:ConMemb->LastUpdate.DbDate,
		:ConMemb->LastUpdate.DbTime, 	:ConMemb->UpdatedBy,
		:ConMemb->ConMembNote;
	return(1);
}

int	DbCloseConMembSelCursor(void)
{
	EXEC SQL Close	ConMembCursor;
	return(1);
}

int DbSetContermPharmacyStatus( 	TLocationId	LocationParam,
					short	HasPharmacyParam)
{
EXEC SQL BEGIN DECLARE SECTION;
	TLocationId	Location	=	LocationParam;
	short	HasPharmacy 	=	HasPharmacyParam;
EXEC SQL END DECLARE SECTION;

	EXEC SQL update conterm set
		HasPharmacy 	=	:HasPharmacy
	where	
		Location	=	:Location;
	return(1);
}

int DbInsertAddInsRecord( TAddInsRecord	*Ptr)
{
EXEC SQL BEGIN DECLARE SECTION;
	TAddInsRecord *AddIns	=	Ptr;
EXEC SQL END DECLARE SECTION;

	EXEC SQL insert into	addinsurance
	(
		idnumber, 		idcode, 		ins1code,
		ins1elegebility, 	ins1type, 		ins1validfrom,
		ins1validuntil, 	ins2code, 		ins2elegebility,
		ins2type, 		ins2validfrom, 		ins2validuntil,
		ins3code, 		ins3elegebility, 	ins3type,
		ins3validfrom, 		ins3validuntil, 	ins4code,
		ins4elegebility, 	ins4type, 		ins4validfrom,
		ins4validuntil
	)	
	values
	(
		:AddIns->MemberId.Number, 	:AddIns->MemberId.Code,
		:AddIns->Data[0].InsCode, 	:AddIns->Data[0].Elegebility,
		:AddIns->Data[0].InsType, 	:AddIns->Data[0].ValidFrom,
		:AddIns->Data[0].ValidUntil, 	:AddIns->Data[1].InsCode,
		:AddIns->Data[1].Elegebility, 	:AddIns->Data[1].InsType,
		:AddIns->Data[1].ValidFrom, 	:AddIns->Data[1].ValidUntil,
		:AddIns->Data[2].InsCode, 	:AddIns->Data[2].Elegebility,
		:AddIns->Data[2].InsType, 	:AddIns->Data[2].ValidFrom,
		:AddIns->Data[2].ValidUntil, 	:AddIns->Data[3].InsCode,
		:AddIns->Data[3].Elegebility, 	:AddIns->Data[3].InsType,
		:AddIns->Data[3].ValidFrom, 	:AddIns->Data[3].ValidUntil
	);
	return(1);
}


int DbGetAddInsRecord( TMemberId	*MemberId,
			TAddInsRecord	*Ptr)
{
EXEC SQL BEGIN DECLARE SECTION;
	TAddInsRecord *AddIns	=	Ptr;
EXEC SQL END DECLARE SECTION;

	AddIns->MemberId	=	*MemberId;

	EXEC SQL select
		idnumber, 		idcode, 		ins1code,
		ins1elegebility, 	ins1type, 		ins1validfrom,
		ins1validuntil, 	ins2code, 		ins2elegebility,
		ins2type, 		ins2validfrom, 		ins2validuntil,
		ins3code, 		ins3elegebility, 	ins3type,
		ins3validfrom, 		ins3validuntil, 	ins4code,
		ins4elegebility, 	ins4type, 		ins4validfrom,
		ins4validuntil
	into
		:AddIns->MemberId.Number, 	:AddIns->MemberId.Code,
		:AddIns->Data[0].InsCode, 	:AddIns->Data[0].Elegebility,
		:AddIns->Data[0].InsType, 	:AddIns->Data[0].ValidFrom,
		:AddIns->Data[0].ValidUntil, 	:AddIns->Data[1].InsCode,
		:AddIns->Data[1].Elegebility, 	:AddIns->Data[1].InsType,
		:AddIns->Data[1].ValidFrom, 	:AddIns->Data[1].ValidUntil,
		:AddIns->Data[2].InsCode, 	:AddIns->Data[2].Elegebility,
		:AddIns->Data[2].InsType, 	:AddIns->Data[2].ValidFrom,
		:AddIns->Data[2].ValidUntil, 	:AddIns->Data[3].InsCode,
		:AddIns->Data[3].Elegebility, 	:AddIns->Data[3].InsType,
		:AddIns->Data[3].ValidFrom, 	:AddIns->Data[3].ValidUntil
	from	addinsurance
	where	
		IdNumber	=	:AddIns->MemberId.Number	and
		IdCode		=	:AddIns->MemberId.Code;
	return(1);
}


int DbDeleteAddInsRecord( TMemberId	*MemberId)
{
EXEC SQL BEGIN DECLARE SECTION;
	TAddInsRecord	AddIns;
EXEC SQL END DECLARE SECTION;

	AddIns.MemberId	=	*MemberId;

	EXEC SQL delete	from	addinsurance
	where	
		IdCode		=	:AddIns.MemberId.Code		and
		IdNumber	=	:AddIns.MemberId.Number;

	return(1);
}


int DbUpdateAddInsRecord( 	TMemberId	*MemberId,
				TAddInsRecord	*Ptr)
{
EXEC SQL BEGIN DECLARE SECTION;
	TAddInsRecord *AddIns	=	Ptr;
EXEC SQL END DECLARE SECTION;

	AddIns->MemberId	=	*MemberId;

	EXEC SQL update addinsurance set
		ins1code		=	:AddIns->Data[0].InsCode,
		ins1elegebility 	=	:AddIns->Data[0].Elegebility,
		ins1type		=	:AddIns->Data[0].InsType,
		ins1validfrom		=	:AddIns->Data[0].ValidFrom,
		ins1validuntil		=	:AddIns->Data[0].ValidUntil,
		ins2code		=	:AddIns->Data[1].InsCode,
		ins2elegebility 	=	:AddIns->Data[1].Elegebility,
		ins2type		=	:AddIns->Data[1].InsType,
		ins2validfrom		=	:AddIns->Data[1].ValidFrom,
		ins2validuntil		=	:AddIns->Data[1].ValidUntil,
		ins3code		=	:AddIns->Data[2].InsCode,
		ins3elegebility 	=	:AddIns->Data[2].Elegebility,
		ins3type		=	:AddIns->Data[2].InsType,
		ins3validfrom		=	:AddIns->Data[2].ValidFrom,
		ins3validuntil		=	:AddIns->Data[2].ValidUntil,
		ins4code		=	:AddIns->Data[3].InsCode,
		ins4elegebility 	=	:AddIns->Data[3].Elegebility,
		ins4type		=	:AddIns->Data[3].InsType,
		ins4validfrom		=	:AddIns->Data[3].ValidFrom,
		ins4validuntil		=	:AddIns->Data[3].ValidUntil
	where	
		IdNumber		=	:AddIns->MemberId.Number  and
		IdCode			=	:AddIns->MemberId.Code;

	return(1);
}

int DbOpenSendrecSelCursor(void)
{
	EXEC SQL declare	SendrecSelCursor	cursor	for
	select 
		Tablname, 	Recnumb 
	from	sendrec;

	EXEC SQL Open	SendrecSelCursor;
	return(1);
}

int	DbFetchSendrecSelCursor( TSendrecRecord	*Ptr)
{
EXEC SQL BEGIN DECLARE SECTION;
	TSendrecRecord *Send	=	Ptr;
EXEC SQL END DECLARE SECTION;

	EXEC SQL Fetch	SendrecSelCursor	into
		:Send->Tablname,
		:Send->Recnumb;
	return(1);
}

int DbCloseSendrecSelCursor(void)
{
	EXEC SQL close	SendrecSelCursor;
	return(1);
}

int DbOpenSendrecInsCursor( TSendrecRecord	*Ptr)
{
EXEC SQL BEGIN DECLARE SECTION;
	TSendrecRecord	*Send	=	Ptr;
EXEC SQL END DECLARE SECTION;

	Ptr->LastUpdate 	=	CurrDateTime;

	EXEC SQL declare	SendrecInsCursor	cursor	for
	insert	into	sendrec 
	(
	Tablname, Recnumb, LastUpdateDate, LastUpdateTime
	)	values
	(
	:Send->Tablname, 		:Send->Recnumb,
	:Send->LastUpdate.DbDate, 	:Send->LastUpdate.DbTime
	);
	EXEC SQL open	SendrecInsCursor;
	return(1);
}

int DbPutSendrecInsCursor(void)
{
	EXEC SQL put	SendrecInsCursor;
	return(1);
}


int DbCloseSendrecInsCursor(void)
{
	EXEC SQL Close	SendrecInsCursor;
	return(1);
}

int	DbUpdateSndrecRecord( 	TCTablName	Tablname,
				int	Recnumb,
				TSendrecRecord	*Ptr,
				TDbDateTime CurrDateTime)
{
EXEC SQL BEGIN DECLARE SECTION;
	TSendrecRecord	*Send	=	Ptr;
EXEC SQL END DECLARE SECTION;

	Ptr->LastUpdate =	CurrDateTime;
	strcpy(Ptr->Tablname,Tablname);
	Ptr->Recnumb	=	Recnumb;

	EXEC SQL update sendrec	set
		Tablname	=	:Send->Tablname,
		Recnumb		=	:Send->Recnumb,
		LastUpdateDate	=	:Send->LastUpdate.DbDate,
		LastUpdateTime	=	:Send->LastUpdate.DbTime
	where	
		Tablname	=	:Send->Tablname;

	return(1);
}

/*
void	DbOpenAuthLogSelCursor( TRecRelNumber RecRelNumb)
{
EXEC SQL BEGIN DECLARE SECTION;
	long int	Rrn_min;
EXEC SQL END DECLARE SECTION;

	Rrn_min = RecRelNumb;

	EXEC SQL declare	AuthLogSelCursor	cursor	for

	select
		RspTime, 	ProcId, 	SessionId,
		LogDate, 	LogTime, 	MdxTime,
		MsgCode, 	Error, 		TermId,
		Contractor, 	ContractType, 	MemberId,
		MemberCode, 	Location, 	Position,
       	 	Paymenttype, 	Rrn 
	from	authlog	
	where
		Rrn > :Rrn_min;

	EXEC SQL open	AuthLogSelCursor;
}

void	DbFetchAuthLogSelCursor( TLogRecord *Ptr)
{
EXEC SQL BEGIN DECLARE SECTION;
	TLogRecord	*Aulogtst	=Ptr;
EXEC SQL END DECLARE SECTION;

	EXEC SQL fetch	AuthLogSelCursor	into
		:Aulogtst->Hdr.RspTime,
		:Aulogtst->Hdr.ProcId,
		:Aulogtst->Hdr.SessionId,
		:Aulogtst->Hdr.LogTime.DbDate,
		:Aulogtst->Hdr.LogTime.DbTime,
		:Aulogtst->Hdr.MdxTime,
		:Aulogtst->Hdr.MsgCode,
		:Aulogtst->Hdr.Error,
		:Aulogtst->Data.TermId,
		:Aulogtst->Data.Contractor,
		:Aulogtst->Data.ContractType,
		:Aulogtst->Data.MemberId.Number,
		:Aulogtst->Data.MemberId.Code,
		:Aulogtst->Data.Location,
       	 	:Aulogtst->Data.Position,
       		:Aulogtst->Data.Paymenttype, 
		:Aulogtst->Data.Rrn;

}

void	DbCloseAuthLogSelCursor(void)
{
	EXEC SQL close	AuthLogSelCursor;

}
*/

/*20020109 DonR*/

int DBAddPrescrsFromVisNewRow (TVisitNewRecord *VN_arg)
{
	int	tries;
	int	prn_sqlcode;
	int err;
	static TRecipeIdentifier	PRW_id_avail = 0;

	EXEC SQL begin declare section;

		// Alias for VisitNew row to be processed.
		TVisitNewRecord	*VN = VN_arg;

		// Extra columns (added to VisitNew columns) to make up VisitNewProblem row.
		short				vnp_problem_count =	0;
		int					vnp_sql_result	[6];
		int					vnp_prw_id		[6];

		// Copy columns to try to avoid 245 error.
		int		vn_DocPrID;
		int		vn_Largo;
		int		vn_PrDate;

		// Date and Time VisitNew/PrescriptionsWritten added to database.
		int			date_YYYYMMDD;
		int			time_HHMMSS;

		// Check whether existing prescr_wr_new row was from Pharmacy rather than from Doctor.
		short int			PRW_Origin;
		long  int			PRW_AddDate;
		long  int			PRW_AddTime;
		int					PRW_id;

		int			loop_counter;

	EXEC SQL end declare section;



	// Prepare VisitNew Arrival Date/Time 
	time_HHMMSS		= GetTime ();
	date_YYYYMMDD	= GetDate ();


	// Loop through the six prescription "slots" in the VisitNew row, writing
	// any prescriptions we find to the PrescriptionsWritten table.
	for (loop_counter = 0; loop_counter < 6; loop_counter++)
	{
		// Default =  no prescription in this slot.
		vnp_sql_result	[loop_counter] = 9999;
		vnp_prw_id		[loop_counter] = 0;

		if (VN->MedicineCode [loop_counter] > 0)
		{
			for (tries = 0; tries < 2; tries++)
			{
				if (PRW_id_avail > 0)
				{
					PRW_id			= PRW_id_avail;
					PRW_id_avail	= 0;
				}
				else
				{
					// DonR 06Feb2020: The old presscriptions-written table is no longer in use, and GET_PRW_ID
					// has been removed from GenSql.ec.
					PRW_id = 0;
//					err = GET_PRW_ID (&PRW_id);

					if (err != MAC_OK)
					{
						GerrLogReturn (GerrId,
									   "Transaction 28: Error %d getting PRW ID.", err);
						GerrLogFnameMini ("prw_msg_log", GerrId,
										  "Transaction 28: Error %d getting PRW ID.", err);
						if (JumpMode)
							longjmp (RollbackLocation, SQL_ERR_JUMP);
					}
				}

				EXEC SQL
					insert into prescr_wr_new
												(
													contractor,
													contracttype,
													treatmentcontr,
													idcode,
													idnumber,
													authdate,
													authtime,
													payinginst,
													medicinecode,
													prescription,
													quantity,
													douse,
													days,
													op,
													prescriptiontype,
													prescriptiondate,
													status,
													sold_date,
													sold_time,
													prescr_added_date,
													prescr_added_time,
													prw_id,
													reported_to_as400
												)
						values					(	:VN->Contractor,
													:VN->ContractType,
													:VN->TreatmentContr,
													:VN->MemberId.Code,
													:VN->MemberId.Number,
													:VN->AuthTime.DbDate,
													:VN->AuthTime.DbTime,
													:VN->PayingInst,
													:VN->MedicineCode		[loop_counter],
													:VN->Prescription		[loop_counter],
													:VN->Quantity			[loop_counter],
													:VN->Douse				[loop_counter],
													:VN->Days				[loop_counter],
													:VN->NumberOfPacages	[loop_counter],
													:VN->PrescriptionType	[loop_counter],
													:VN->PrescriptionDate	[loop_counter],
													0,
													0,
													0,
													:date_YYYYMMDD,
													:time_HHMMSS,
													:PRW_id,
													:NOT_REPORTED_TO_AS400);

				prn_sqlcode = vnp_sql_result [loop_counter] = SQLCODE;

				sqlca.sqlcode = SQLCODE = 0;

				// DonR 15Mar2005: Rarely, two machines can "get" the same batch of
				// Prescription Written ID's. It happens, I think, when one machine
				// gets a new batch of ID's and then aborts its current database
				// transaction. When this happens, the update to prw_nextid is
				// rolled back, but the shared memory in that computer still reflects
				// the new batch of ID's. The next computer that requests a new batch
				// of ID's will get the same batch that the first computer got - and
				// thus two computers will attempt to write the same bunch of ID's.
				// To remedy this, when we get a duplicate-key error when adding
				// a Prescriptions Written row for Transaction 28, we'll check to see
				// if the duplication is from the Prescription Written ID. If so,
				// we'll re-initialize the shared-memory structure in this machine,
				// get a new batch of ID's, and try once again to write the row.
				if ((tries == 0) &&
					((prn_sqlcode == INF_NOT_UNIQUE_INDEX) || (prn_sqlcode == INF_NOT_UNIQUE_CONSTRAINT)))
				{
					// DonR 06Feb2020: PRW_ID_IN_USE no longer exists; made the minimal code change
					// to avoid an "unreferenced external" error.
					if (0)
//					if (PRW_ID_IN_USE  (PRW_id))
					{
						GerrLogReturn (GerrId,
									   "Trn 28: PRW ID %d already in use - will get new batch!", (int)PRW_id);
						GerrLogFnameMini ("prw_msg_log", GerrId,
										  "Trn 28: PRW ID %d already in use- will get new batch!", (int)PRW_id);
						continue;	// Retry with a new batch of Prescription Written ID's.
					}
					else
					{
						PRW_id_avail = PRW_id;
					}
				}

				// DonR 25Dec2008: Update "legitimate" duplicate prescriptions and write
				// other errors to visitnewproblem table.
				if (prn_sqlcode != 0)
				{
					// DonR 25Dec2008: Per Iris Shaya, if we failed trying to insert to prescr_wr_new because
					// the prescription had already been inserted when the member bought the medication, update
					// relevant fields instead of just throwing away the doctor-supplied information.
					// DonR 23Feb2009: Added update to Origin Code as well, to make the "new"
					// prescription visible to Transaction 2001, the AS/400, and so on.
					if (prn_sqlcode == INF_NOT_UNIQUE_INDEX)
					{
						EXEC SQL
							UPDATE	prescr_wr_new

							SET		contractor			= :VN->Contractor,
									contracttype		= :VN->ContractType,
									authdate			= :VN->AuthTime.DbDate,
									authtime			= :VN->AuthTime.DbTime,
									payinginst			= :VN->PayingInst,
									origin_code			= 28,	/* DonR 23Feb2009 */
									quantity			= :VN->Quantity			[loop_counter],
									douse				= :VN->Douse			[loop_counter],
									days				= :VN->Days				[loop_counter],
									op					= :VN->NumberOfPacages	[loop_counter],
									prescriptiontype	= :VN->PrescriptionType	[loop_counter],
									prescr_added_date	= :date_YYYYMMDD,
									prescr_added_time	= :time_HHMMSS,
									reported_to_as400	= :NOT_REPORTED_TO_AS400

							WHERE	treatmentcontr		= :VN->TreatmentContr
							  AND	idcode				= :VN->MemberId.Code
							  AND	idnumber			= :VN->MemberId.Number
							  AND	medicinecode		= :VN->MedicineCode		[loop_counter]
							  AND	prescription		= :VN->Prescription		[loop_counter]
							  AND	prescriptiondate	= :VN->PrescriptionDate	[loop_counter];

						prn_sqlcode = vnp_sql_result [loop_counter] = SQLCODE;

						sqlca.sqlcode = SQLCODE = 0;
					}

					else	// Some problem other than INF_NOT_UNIQUE_INDEX.
					{
						vnp_problem_count++;	// Other SQL error on add.
						vnp_prw_id[loop_counter] = PRW_id;
					}
				}

				// If we get here, either we've made two attempts to write to
				// prescr_wr_new, or we've succeeded, or else we've failed with
				// some error other than duplicate key on add. In any of these
				// cases, we don't want to retry!
				break;

			}	// Retry loop.

		}	// This VisitNew slot has prescription data.

	}	// Loop through prescription column-groups.


	// If there was a problem writing a prescription to the Prescriptions Written
	// table, copy the VisitNew row to VisitnewProblem.
	if (vnp_problem_count > 0)
	{
		EXEC SQL
			insert into visitnewproblem (
										  contractor,
										  contracttype,
										  termid,
										  locationprv,
										  institute,
										  idcode,
										  idnumber,
										  cardissuedate,
										  authdate,
										  authtime,
										  gnrlelegebility,
										  observation0,
										  observation1,
										  observation2,
										  observation3,
										  treatmentcontr,
										  medicinecode0,
										  prescription0,
										  quantity0,
										  douse0,
										  days0,
										  numberofpacages0,
										  prescriptiontype0,
										  prescriptiondate0,
										  sql_result0,
										  medicinecode1,
										  prescription1,
										  quantity1,
										  douse1,
										  days1,
										  numberofpacages1,
										  prescriptiontype1,
										  prescriptiondate1,
										  sql_result1,
										  medicinecode2,
										  prescription2,
										  quantity2,
										  douse2,
										  days2,
										  numberofpacages2,
										  prescriptiontype2,
										  prescriptiondate2,
										  sql_result2,
										  medicinecode3,
										  prescription3,
										  quantity3,
										  douse3,
										  days3,
										  numberofpacages3,
										  prescriptiontype3,
										  prescriptiondate3,
										  sql_result3,
										  medicinecode4,
										  prescription4,
										  quantity4,
										  douse4,
										  days4,
										  numberofpacages4,
										  prescriptiontype4,
										  prescriptiondate4,
										  sql_result4,
										  medicinecode5,
										  prescription5,
										  quantity5,
										  douse5,
										  days5,
										  numberofpacages5,
										  prescriptiontype5,
										  prescriptiondate5,
										  sql_result5,
										  reference0,
										  reference1,
										  reference2,
										  confirmation0,
										  confirmation1,
										  confirmation2,
										  payinginst,
										  profession,
										  location,
										  position,
										  paymenttype,
										  original_rrn,
										  problem_count,
										  prw_id_0,
										  prw_id_1,
										  prw_id_2,
										  prw_id_3,
										  prw_id_4,
										  prw_id_5,
										  date,
										  time
										 )
								values
										 (
										  :VN->Contractor,
								 		  :VN->ContractType,
										  :VN->TermId,
										  :VN->Locationprv,
										  :VN->Institute,
										  :VN->MemberId.Code,
										  :VN->MemberId.Number,
										  :VN->CardIssueDate,
										  :VN->AuthTime.DbDate,
										  :VN->AuthTime.DbTime,
										  :VN->GnrlElegebility,
										  :VN->Observation [0],
										  :VN->Observation [1],
										  :VN->Observation [2],
										  :VN->Observation [3],
										  :VN->TreatmentContr,
										  :VN->MedicineCode		[0],
										  :VN->Prescription		[0],
										  :VN->Quantity			[0],
										  :VN->Douse			[0],
										  :VN->Days				[0],
										  :VN->NumberOfPacages	[0],
										  :VN->PrescriptionType	[0],
										  :VN->PrescriptionDate	[0],
										  :vnp_sql_result		[0],
										  :VN->MedicineCode		[1],
										  :VN->Prescription		[1],
										  :VN->Quantity			[1],
										  :VN->Douse				[1],
										  :VN->Days				[1],
										  :VN->NumberOfPacages	[1],
										  :VN->PrescriptionType	[1],
										  :VN->PrescriptionDate	[1],
										  :vnp_sql_result			[1],
										  :VN->MedicineCode		[2],
										  :VN->Prescription		[2],
										  :VN->Quantity			[2],
										  :VN->Douse				[2],
										  :VN->Days				[2],
										  :VN->NumberOfPacages	[2],
										  :VN->PrescriptionType	[2],
										  :VN->PrescriptionDate	[2],
										  :vnp_sql_result			[2],
										  :VN->MedicineCode		[3],
										  :VN->Prescription		[3],
										  :VN->Quantity			[3],
										  :VN->Douse				[3],
										  :VN->Days				[3],
										  :VN->NumberOfPacages	[3],
										  :VN->PrescriptionType	[3],
										  :VN->PrescriptionDate	[3],
										  :vnp_sql_result			[3],
										  :VN->MedicineCode		[4],
										  :VN->Prescription		[4],
										  :VN->Quantity			[4],
										  :VN->Douse				[4],
										  :VN->Days				[4],
										  :VN->NumberOfPacages	[4],
										  :VN->PrescriptionType	[4],
										  :VN->PrescriptionDate	[4],
										  :vnp_sql_result			[4],
										  :VN->MedicineCode		[5],
										  :VN->Prescription		[5],
										  :VN->Quantity			[5],
										  :VN->Douse				[5],
										  :VN->Days				[5],
										  :VN->NumberOfPacages	[5],
										  :VN->PrescriptionType	[5],
										  :VN->PrescriptionDate	[5],
										  :vnp_sql_result			[5],
										  :VN->Reference [0],
										  :VN->Reference [1],
										  :VN->Reference [2],
										  :VN->Confirmation [0],
										  :VN->Confirmation [1],
										  :VN->Confirmation [2],
										  :VN->PayingInst,
										  :VN->Profession,
										  :VN->Location,
										  :VN->Position,
										  :VN->PaymentType,
										  :VN->Rrn,
										  :vnp_problem_count,
										  :vnp_prw_id[0],
										  :vnp_prw_id[1],
										  :vnp_prw_id[2],
										  :vnp_prw_id[3],
										  :vnp_prw_id[4],
										  :vnp_prw_id[5],
										  :date_YYYYMMDD,
										  :time_HHMMSS
										 );
	}


	// Think about how - and if - error-trapping should be implemented!
	//JUMP_ON_ERR

	return (1);
}

/*20020109 DonR end*/



int DbOpenVisNewInsCursor( TVisitNewRecord	*Ptr)
{
	EXEC SQL begin declare section;
	TVisitNewRecord	*VisNew	=	Ptr;
//	TCObservationCode Obs0, Obs1,Obs2,Obs3;
	char *Obs0, *Obs1, *Obs2, *Obs3;
//	TCCharNew PT0,PT1,PT2,PT3,PT4,PT5;
	char *PT0, *PT1, *PT2, *PT3, *PT4, *PT5;
//	TCCharNew7 Ref0,Ref1,Ref2,Conf0,Conf1,Conf2;
	char *Ref0, *Ref1, *Ref2, *Conf0, *Conf1, *Conf2;
	EXEC SQL end declare section;

	Obs0 = VisNew->Observation[0];
	Obs1 = VisNew->Observation[1];
	Obs2 = VisNew->Observation[2];
	Obs3 = VisNew->Observation[3];

	PT0 = VisNew->PrescriptionType[0];
	PT1 = VisNew->PrescriptionType[1];
	PT2 = VisNew->PrescriptionType[2];
	PT3 = VisNew->PrescriptionType[3];
	PT4 = VisNew->PrescriptionType[4];
	PT5 = VisNew->PrescriptionType[5];

	Ref0 = VisNew->Reference[0];
	Ref1 = VisNew->Reference[1];
	Ref2 = VisNew->Reference[2];

	Conf0 = VisNew->Confirmation[0];
	Conf1 = VisNew->Confirmation[1];
	Conf2 = VisNew->Confirmation[2];

	EXEC SQL declare	VisNewInsCursor cursor for
	insert	into visitnew
	(
    		Contractor, 		ContractType, 	      TermId,
		Institute, 		IdCode, 	      IdNumber,
	    	CardissueDate, 		AuthDate,	      AuthTime,
	    	GnrlElegebility,
	    	Observation0, 		Observation1,
	    	Observation2, 		Observation3,
	    	TreatmentContr,
	    	MedicineCode0, 		Prescription0,
	    	Quantity0, 		Douse0, 	      Days0,
	    	NumberOfPacages0, 	PrescriptionType0,    PrescriptionDate0,
	    	MedicineCode1, 		Prescription1,
	    	Quantity1, 		Douse1, 	      Days1,
	    	NumberOfPacages1, 	PrescriptionType1,    PrescriptionDate1,
	    	MedicineCode2, 		Prescription2,
	    	Quantity2, 		Douse2, 	      Days2,
	    	NumberOfPacages2, 	PrescriptionType2,    PrescriptionDate2,
	    	MedicineCode3, 		Prescription3,
	    	Quantity3, 		Douse3, 	      Days3,
	    	NumberOfPacages3, 	PrescriptionType3,    PrescriptionDate3,
	    	MedicineCode4, 		Prescription4,
	    	Quantity4, 		Douse4, 	      Days4,
	    	NumberOfPacages4, 	PrescriptionType4,    PrescriptionDate4,
	    	MedicineCode5, 		Prescription5,
	    	Quantity5, 		Douse5, 	      Days5,
	    	NumberOfPacages5, 	PrescriptionType5,    PrescriptionDate5,
	    	Reference0, 		Reference1, 	      Reference2,
	    	Confirmation0, 		Confirmation1, 	      Confirmation2,
	    	PayingInst, 		Profession,
		Location, 		Locationprv,
		Position, 		PaymentType
	) values
	(
    		:VisNew->Contractor, 		:VisNew->ContractType,
    		:VisNew->TermId, 		:VisNew->Institute,
		:VisNew->MemberId.Code, 	:VisNew->MemberId.Number,
	    	:VisNew->CardIssueDate,
		:VisNew->AuthTime.DbDate, 	:VisNew->AuthTime.DbTime,
	    	:VisNew->GnrlElegebility,
		:Obs0, :Obs1, :Obs2, :Obs3,
/*	    	:VisNew->Observation[0],	:VisNew->Observation[1],
	    	:VisNew->Observation[2],	:VisNew->Observation[3],*/
	    	:VisNew->TreatmentContr,
	    	:VisNew->MedicineCode[0], 	:VisNew->Prescription[0],
	    	:VisNew->Quantity[0], 		:VisNew->Douse[0], 	
		:VisNew->Days[0], 		:VisNew->NumberOfPacages[0], 	
		:PT0,
		:VisNew->PrescriptionDate[0],
	    	:VisNew->MedicineCode[1], 	:VisNew->Prescription[1],
	    	:VisNew->Quantity[1], 		:VisNew->Douse[1], 	
		:VisNew->Days[1], 		:VisNew->NumberOfPacages[1], 	
		:PT1,
		:VisNew->PrescriptionDate[1],
	    	:VisNew->MedicineCode[2], 	:VisNew->Prescription[2],
	    	:VisNew->Quantity[2], 		:VisNew->Douse[2], 	
		:VisNew->Days[2], 		:VisNew->NumberOfPacages[2], 	
		:PT2,
		:VisNew->PrescriptionDate[2],
	    	:VisNew->MedicineCode[3], 	:VisNew->Prescription[3],
	    	:VisNew->Quantity[3], 		:VisNew->Douse[3], 	
		:VisNew->Days[3], 		:VisNew->NumberOfPacages[3], 	
		:PT3,
		:VisNew->PrescriptionDate[3],
	    	:VisNew->MedicineCode[4], 	:VisNew->Prescription[4],
	    	:VisNew->Quantity[4], 		:VisNew->Douse[4], 	
		:VisNew->Days[4], 		:VisNew->NumberOfPacages[4], 	
		:PT4,
		:VisNew->PrescriptionDate[4],
	    	:VisNew->MedicineCode[5], 	:VisNew->Prescription[5],
	    	:VisNew->Quantity[5], 		:VisNew->Douse[5], 	
		:VisNew->Days[5], 		:VisNew->NumberOfPacages[5], 	
		:PT5,
		:VisNew->PrescriptionDate[5],
		:Ref0, :Ref1, :Ref2,
		/*:VisNew->Reference[0],    :VisNew->Reference[1],    :VisNew->Reference[2],*/
		:Conf0, :Conf1, :Conf2,
		/*:VisNew->Confirmation[0], :VisNew->Confirmation[1], :VisNew->Confirmation[2],*/
	    	:VisNew->PayingInst, 		:VisNew->Profession,
		:VisNew->Location, 		:VisNew->Locationprv,
		:VisNew->Position, 		:VisNew->PaymentType
	);


	EXEC SQL open	VisNewInsCursor;

	JUMP_ON_ERR;
	return(1);
}


int DbPutVisNewInsCursor(void)
{
	if( strlen(VisitNewRecord.Observation[0]) != 9	)
	{
	memset(VisitNewRecord.Observation[0],32,sizeof(VisitNewRecord.Observation[0]));
	}
	EXEC SQL put	VisNewInsCursor;
	if(	SQLCODE ==	-391	)
	{
	printf("insert into visitnew -391 error\n");
	printf("Contractor %d Terminal %d\n",
		VisitNewRecord.Contractor,VisitNewRecord.TermId);

	
	}
/*20020109 DonR*/	
		
	// DonR_09JAN2002 Begin.
	if (SQLCODE == SQLERR_ok)
	{
//		VisitNewRecord.Rrn = sqlca.sqlerrd[1];
		DBAddPrescrsFromVisNewRow (&VisitNewRecord);
	}
	// DonR_09JAN2002 End.

	JUMP_ON_ERR;

	return(1);
}

int DbCloseVisNewInsCursor(void)
{
	EXEC SQL Close	VisNewInsCursor;
	return(1);
}

/*Yulia 20030415 begin*/
int DbInsertConsultantRecord(TVisitNewRecord	*Ptr,int prof)
{
	EXEC SQL begin declare section;
	TVisitNewRecord	*VisNew	=	Ptr;
	int v_prof = prof;
	EXEC SQL end declare section;
	
	EXEC SQL 
	insert	into CONSULTANTREF
	(
    		Contractor, 		ContractType, 	  TreatmentContr,
			TermId,
		 	IdCode, 			IdNumber,
	    	AuthDate,			AuthTime,
			Profession
	) values
	(
    		:VisNew->Contractor, 		:VisNew->ContractType,
			:VisNew->TreatmentContr,
    		:VisNew->TermId,
			:VisNew->MemberId.Code, 	:VisNew->MemberId.Number,
	    	:VisNew->AuthTime.DbDate, 	:VisNew->AuthTime.DbTime,
			:v_prof
	);
	if( SQLERR_code_cmp( SQLERR_not_unique ) == MAC_TRUE )
	{
		GerrLogFnameMini ("Consult_log",GerrId,
			"double VN send [%d][%d][%d]",VisNew->Contractor,VisNew->MemberId.Number,prof);
	}
    JUMP_ON_ERR;
}


int DbGetConsName	(TIdNumber	Consultant,	TContractType ConsType,char *ConsName)
{
	EXEC SQL BEGIN DECLARE SECTION;
		TIdNumber		v_Consultant	=	Consultant;
		TContractType	v_ConsType		=	ConsType;
		char v_FName[9];
		char v_LName[15];
	EXEC SQL END   DECLARE SECTION;
	int i;
	for(i=0;i<24;i++)
	{
		ConsName[i]='\0';
	}

	EXEC SQL 
		SELECT max(firstname),max(lastname) into :v_FName,:v_LName from contract
		where contractor =	:v_Consultant;
//		GerrLogFnameMini("172_log",GerrId,"DbGetConsName1:[%s][%s][%d][%d]",	
//		v_FName,v_LName,v_Consultant,sqlca.sqlcode);

	SQLERR_error_test();
	strncpy (ConsName,v_FName,8 );
	strncat (ConsName,v_LName,14);
//		GerrLogFnameMini("172_log",GerrId,"DbGetConsName2:[%s]",			ConsName);

//	ConsName[24]='\0';
	return (1);

}
/* Replaced into file DbConsRef.ec
int DbInsertConsConfRecord (	TIdNumber		Contractor,
								TContractType	ContractType,
								TIdNumber		TreatmentContr,
								TMemberId		*MemberId,
								TDbDateTime		*ConsDateTime,
								TIdNumber		ConsContractor,
								// TContractType	ConsContractType,
								TTermId			TermId
								)
{
	EXEC SQL BEGIN DECLARE SECTION;
		TIdNumber		v_Contractor	=	Contractor;
		TContractType	v_ContractType	=	ContractType;
		TMemberId		*v_MemberId		=	MemberId;
		TDbDateTime		*v_ConsDateTime	=	ConsDateTime;
		int				insdate,instime;
		TIdNumber		v_ConsCont		=	ConsContractor;
		//TContractType	v_ConsContType	=	ConsContractType;
		TIdNumber		v_TreatmentContr=	TreatmentContr;
		TTermId			v_TermId		=	TermId;

	EXEC SQL END DECLARE SECTION;
		insdate = GetDate();
		instime	= GetTime();

		EXEC SQL INSERT into CONSCONFIRM
			(
			contractor,
			contracttype ,
			treatmentcontr,
			idcode ,
			idnumber,
			consdate,
			constime,
			consultant,
		//	constype,
			termid,
			insertdate,
			inserttime 
			)
			values
			(
			:v_Contractor,
			:v_ContractType,
			:v_TreatmentContr,
			:v_MemberId->Code,
			:v_MemberId->Number,					
			:v_ConsDateTime->DbDate,
			:v_ConsDateTime->DbTime,
			:v_ConsCont,
		//	:v_ConsContType,
			:v_TermId,
			:insdate,
			:instime
			);
		GerrLogFnameMini("174_log",GerrId,"pont_sql7:[%d][%d][%d]",	
		v_MemberId->Number,v_MemberId->Code,sqlca.sqlcode);

		SQLERR_error_test();

} // DbInsertConsConfRecord
*/



/*Yulia 20030415 end*/



/*20010201*/
/*
int DbOpenMedParamInsCursor( TMedParamRecord	*Ptr,	TDbDateTime	MedParamDate)
{
	EXEC SQL begin declare section;
	TMedParamRecord	*MedParam	=	Ptr;
	int Date,Time;
	EXEC SQL end declare section;

	Date = 	MedParamDate.DbDate;
	Time = 	MedParamDate.DbTime;


	EXEC SQL declare	MedParamInsCursor cursor for
	insert	into medparam
	(
    		Contractor, 		ContractType, 	      TermId,
			IdCode, 			IdNumber,
			paramnumber,
			measuredate    ,
			writedate  ,
			writetime  ,
			paramvaluechar1 ,
			paramvaluechar2,
			paramvaluechar3,
			Profession,
			Location,
			Locationprv,
			Position, 
			PaymentType,
			insertdate,
			inserttime
	) values
	(
    	:MedParam->Contractor, 		
		:MedParam->ContractType,
    	:MedParam->TermId, 
		:MedParam->MemberId.Code, 	
		:MedParam->MemberId.Number,
		:MedParam->ParamNumber,
		:MedParam->MeasureDate ,
		:MedParam->WriteDT.DbDate,
		:MedParam->WriteDT.DbTime,
		:MedParam->ParamValueChar1,
		:MedParam->ParamValueChar2,
		:MedParam->ParamValueChar3,
		:MedParam->Profession,
		:MedParam->Location, 		
		:MedParam->Locationprv,
		:MedParam->Position, 		
		:MedParam->PaymentType,
		:Date,
		:Time
	);


	EXEC SQL open	MedParamInsCursor;

	JUMP_ON_ERR;
	return(1);
}
*/
/* Yulia 20030306
int DbPutMedParamInsCursor(void)
{
	
	EXEC SQL put	MedParamInsCursor;
	JUMP_ON_ERR;
	return(1);
}
*/
int DbPutMedParamInsCursor( TMedParamRecord	*Ptr,	TDbDateTime	MedParamDate)
{
	EXEC SQL begin declare section;
	TMedParamRecord	*MedParam	=	Ptr;
	int Date,Time;
	EXEC SQL end declare section;

	Date = 	MedParamDate.DbDate;
	Time = 	MedParamDate.DbTime;


	EXEC SQL 	insert	into medparam
	(
    		Contractor, 		ContractType, 	      TermId,
			IdCode, 			IdNumber,
			paramnumber,
			measuredate    ,
			writedate  ,
			writetime  ,
			paramvaluechar1 ,
			paramvaluechar2,
			paramvaluechar3,
			Profession,
			Location,
			Locationprv,
			Position, 
			PaymentType,
			insertdate,
			inserttime
	) values
	(
    	:MedParam->Contractor, 		
		:MedParam->ContractType,
    	:MedParam->TermId, 
		:MedParam->MemberId.Code, 	
		:MedParam->MemberId.Number,
		:MedParam->ParamNumber,
		:MedParam->MeasureDate ,
		:MedParam->WriteDT.DbDate,
		:MedParam->WriteDT.DbTime,
		:MedParam->ParamValueChar1,
		:MedParam->ParamValueChar2,
		:MedParam->ParamValueChar3,
		:MedParam->Profession,
		:MedParam->Location, 		
		:MedParam->Locationprv,
		:MedParam->Position, 		
		:MedParam->PaymentType,
		:Date,
		:Time
	);



	JUMP_ON_ERR;
	return(1);
}
/*
int DbCloseMedParamInsCursor(void)
{
	EXEC SQL Close	MedParamInsCursor;
	return(1);
}
*/
/*20010101*/

/*20010227*/
/*20020527*/
int DbPutLabRefInsCursor( TLabRefRecord	*Ptr,	TDbDateTime	LabRefDate)
{
	EXEC SQL begin declare section;
	TLabRefRecord	*LabRef	=	Ptr;
	int Date,Time;
	EXEC SQL end declare section;

	Date = 	LabRefDate.DbDate;
	Time = 	LabRefDate.DbTime;


	EXEC SQL 
	/*declare	LabRefInsCursor cursor for*/
	insert	into LabRef
	(
		contractor,
		contracttype,
		idcode,
		idnumber,
		termid,
		laberror,
		license,
		labdate,
		labtime,
		mensdate,
		pregnantwoman,
		pregnantweek,
		urgent,
		PayingInst,
		phone,
		countcode,
		countobs,
		countobsact,
		insertdate,
		inserttime,
		reported_to_as400
	) values
	(
		:LabRef->Contractor,
		:LabRef->ContractType,
        :LabRef->MemberId.Code,
        :LabRef->MemberId.Number,
		:LabRef->TermId,
		:LabRef->LabError,
		:LabRef->DoctorNumber,
		:LabRef->LabDate,
		:LabRef->LabTime,
		:LabRef->MensDate,
		:LabRef->PregnantYN,
		:LabRef->PregnantWeek,
		:LabRef->UrgentYN,
		:LabRef->PayingInst,
		:LabRef->Phone,
		:LabRef->CountCode,
		:LabRef->CountObserv,
		:LabRef->CountObservAct,
		:Date,
		:Time,
		0
	);

/*	EXEC SQL open	LabRefInsCursor;*/
	JUMP_ON_ERR;
	return(1);
}

/* 20020527
int DbPutLabRefInsCursor(void)
{
	EXEC SQL put	LabRefInsCursor;
	JUMP_ON_ERR;
	return(1);
}

int DbCloseLabRefInsCursor(void)
{
	EXEC SQL Close	LabRefInsCursor;
	return(1);
}
*/
int DbPutLabRefCodeInsCursor( TLabRefRecord	*Ptr,int Code,	TDbDateTime	LabRefDate,int seq)
{
	EXEC SQL begin declare section;
	TLabRefRecord	*LabRef	=	Ptr;
	int ReqCode;
	int Date,Time;
	int v_seq;
	
	EXEC SQL end declare section;

	Date = 	LabRefDate.DbDate;
	Time = 	LabRefDate.DbTime;
	v_seq= seq;
	ReqCode = Code;
	EXEC SQL
	/* declare	LabRefCInsCursor cursor for*/
	insert	into LabRefCode
	(
		contractor,
		contracttype,
		idcode,
		idnumber,
		labdate,
		labtime,
		reqcode,
		reported_to_as400,
		seqnum,
		insertdate,
		inserttime
	) values
	(
		:LabRef->Contractor,
		:LabRef->ContractType,
        :LabRef->MemberId.Code,
        :LabRef->MemberId.Number,
		:LabRef->LabDate,
		:LabRef->LabTime,
		:ReqCode,
		0,
		:v_seq,
		:Date,
		:Time
	);

/*	EXEC SQL open	LabRefCInsCursor;*/
	JUMP_ON_ERR;
	return(1);
}

/*
int DbPutLabRefCodeInsCursor(void)
{
	EXEC SQL put	LabRefCInsCursor;
	JUMP_ON_ERR;
	return(1);
}

int DbCloseLabRefCodeInsCursor(void)
{
	EXEC SQL Close	LabRefCInsCursor;
	return(1);
}
*/
int DbPutLabRefObservInsCursor( TLabRefRecord	*Ptr,TCObservationCode *Observ,	TDbDateTime	LabRefDate)
{
	EXEC SQL begin declare section;
	TLabRefRecord	*LabRef	=  Ptr;
	TCObservationCode     ReqObserv ;
	int Date,Time;
	EXEC SQL end declare section;

	strcpy (	ReqObserv,*Observ);
	Date = 	LabRefDate.DbDate;
	Time = 	LabRefDate.DbTime;

	EXEC SQL 
		/*declare	LabRefOInsCursor cursor for*/
	insert	into LabRefObservation
	(
		contractor,
		contracttype,
		idcode,
		idnumber,
		labdate,
		labtime,
		observation,
		insertdate,
		inserttime,
		reported_to_as400
	) values
	(
		:LabRef->Contractor,
		:LabRef->ContractType,
        :LabRef->MemberId.Code,
        :LabRef->MemberId.Number,
		:LabRef->LabDate,
		:LabRef->LabTime,
		:ReqObserv,
		:Date,
		:Time,
		0
	);

/*	EXEC SQL open	LabRefOInsCursor;*/
	JUMP_ON_ERR;
	return(1);
}

/*
int DbPutLabRefObservInsCursor(void)
{
	EXEC SQL put	LabRefOInsCursor;
	JUMP_ON_ERR;
	return(1);
}

int DbCloseLabRefObservInsCursor(void)
{
	EXEC SQL Close	LabRefOInsCursor;
	return(1);
}
*/
int DbPutLabRefObservActInsCursor( TLabRefRecord	*Ptr,TCObservationCode *Observ,	TDbDateTime	LabRefDate)
{
	EXEC SQL begin declare section;
	TLabRefRecord	*LabRef	=  Ptr;
	TCObservationCode     ReqObserv ;
	int Date,Time;
	EXEC SQL end declare section;

	strcpy (	ReqObserv,*Observ);
	Date = 	LabRefDate.DbDate;
	Time = 	LabRefDate.DbTime;


	EXEC SQL 
		/*declare	LabRefOAInsCursor cursor for*/
	insert	into  labrefobservactive
	(
		contractor,
		contracttype,
		idcode,
		idnumber,
		labdate,
		labtime,
		observationact,
		insertdate,
		inserttime,
		reported_to_as400
	) values
	(
		:LabRef->Contractor,
		:LabRef->ContractType,
        :LabRef->MemberId.Code,
        :LabRef->MemberId.Number,
		:LabRef->LabDate,
		:LabRef->LabTime,
		:ReqObserv,
		:Date,
		:Time,
		0
	);
	JUMP_ON_ERR;
	return(1);
}
/*20010227*/
/*20071231*/
int DbPutLabRefDelInsCursor( TLabRefRecord	*Ptr,	TDbDateTime	LabRefDate)
{
	EXEC SQL begin declare section;
	TLabRefRecord	*LabRef	=	Ptr;
	int Date,Time;
	EXEC SQL end declare section;

	Date = 	LabRefDate.DbDate;
	Time = 	LabRefDate.DbTime;

	EXEC SQL insert	into LabRefDel
	(
		contractor,
		contracttype,
		idcode,
		idnumber,
		termid,
		laberror,
		license,
		labdate,
		labtime,
		insertdate,
		inserttime,
		reported_to_as400
	) values
	(
		:LabRef->Contractor,
		:LabRef->ContractType,
        :LabRef->MemberId.Code,
        :LabRef->MemberId.Number,
		:LabRef->TermId,
		:LabRef->LabError,
		:LabRef->DoctorNumber,
		:LabRef->LabDate,
		:LabRef->LabTime,
		:Date,
		:Time,
		0
	);
	JUMP_ON_ERR;
	return(1);
}
/*end 20071231*/

int DbOpenNurseEntranceInsCursor(TNurseEntranceRecord	*Ptr)
{
	EXEC SQL begin declare section;
	TNurseEntranceRecord	*NurseEntrance	=	Ptr;
	char	*HospN;
	EXEC SQL end declare section;

	HospN = NurseEntrance->HospitalizationNumber;

	EXEC SQL declare	NurseEntrInsCursor cursor for
	insert	into NurseEntrance
			(
    		Contractor, 		ContractType, 	    termid,
			idnumber ,			idcode   ,			nurseerror,
			hospitalcode ,		obligationnumber,		hospitalnumber  ,
			hospitaldate ,senddate ,
			location,	locationprv,
			position,	paymenttype
			)
			values 
			(
			:NurseEntrance->Contractor,
			:NurseEntrance->ContractType,
			:NurseEntrance->TermId,
			:NurseEntrance->MemberId.Number,
			:NurseEntrance->MemberId.Code,
			:NurseEntrance->NurseError,
			:NurseEntrance->HospitalId,
			:NurseEntrance->ObligationNumber,
			:HospN,
			:NurseEntrance->HospitalDate ,
			:NurseEntrance->SendDate,
			:NurseEntrance->Location,
			:NurseEntrance->Locationprv,
			:NurseEntrance->Position,
			:NurseEntrance->PaymentType
			);
	
	EXEC SQL open	NurseEntrInsCursor;
	printf (" sql[%d]",sqlca.sqlcode);
	fflush (stdout);

	JUMP_ON_ERR;
	
}
int DbPutNurseEntranceInsCursor(void)
{

	EXEC SQL put	NurseEntrInsCursor;
	printf ("put  sql[%d]",sqlca.sqlcode);
	fflush (stdout);

	JUMP_ON_ERR;
}
int DbCloseNurseEntranceInsCursor(void)
{
	EXEC SQL Close	NurseEntrInsCursor;
	printf ("close sql[%d]",sqlca.sqlcode);
	fflush (stdout);

	JUMP_ON_ERR;
}

int DbOpenNurseDailyInsCursor(TNurseDailyRecord	*Ptr)
{
	EXEC SQL begin declare section;
	TNurseDailyRecord	*NurseDaily	=	Ptr;
	char	*HospN,*Com1,*Com2;
	EXEC SQL end declare section;

	HospN	=	NurseDaily->HospitalizationNumber;
	Com2	=	NurseDaily->Comment2;
	Com1	=	NurseDaily->Comment1;


	EXEC SQL declare	NurseDayInsCursor cursor for
	Insert	into NurseDaily
	(
		contractor,		contracttype  ,		termid ,
		idnumber  ,		idcode        ,		NurseError,
		hospitalcode  ,	obligationnumber,	
		hospitalnumber,	hospitaldate    ,
		department    ,	right        ,		reasonnotright  ,
		comment1    ,	comment2    ,		senddate    ,
		location    ,	locationprv ,
		position    ,	paymenttype       
	)
	values
	(
			:NurseDaily->Contractor,
			:NurseDaily->ContractType,
			:NurseDaily->TermId,
			:NurseDaily->MemberId.Code,
			:NurseDaily->MemberId.Number,
			:NurseDaily->NurseError,
			:NurseDaily->HospitalId,
			:NurseDaily->ObligationNumber,
			:HospN,
			:NurseDaily->HospitalDate ,
			:NurseDaily->Department,
			:NurseDaily->Right_YN,
			:NurseDaily->ReasonNotRight,
			:Com1,
			:Com2,
			:NurseDaily->SendDate,
			:NurseDaily->Location,
			:NurseDaily->Locationprv,
			:NurseDaily->Position,
			:NurseDaily->PaymentType
	);
	EXEC SQL open	NurseDayInsCursor;
	JUMP_ON_ERR;

}
int DbPutNurseDailyInsCursor(void)
{
	EXEC SQL put	NurseDayInsCursor;
	JUMP_ON_ERR;

}
int DbCloseNurseDailyInsCursor(void)
{
	EXEC SQL Close	NurseDayInsCursor;
	JUMP_ON_ERR;
}


int DbOpenNurseReleaseInsCursor(TNurseReleaseRecord	*Ptr)
{
	EXEC SQL begin declare section;
	TNurseReleaseRecord	*NurseRelease	=	Ptr;
	char	*HospN,*NOp1,*NOp2,*NCh1,*NCh2,*NFName,*NLName,*NDName,*NObs1,*NObs2,*NDCom;
	EXEC SQL end declare section;

	HospN	=	NurseRelease->HospitalizationNumber;
	NOp1		=	NurseRelease->Operation1;
	NOp2		=	NurseRelease->Operation2;
	NCh1	=	NurseRelease->Characterization1;
	NCh2	=	NurseRelease->Characterization2;
	NFName	=	NurseRelease->FirstName;
	NLName	=	NurseRelease->LastName;
	NDName	=	NurseRelease->DoctorName;
	NObs1	=	NurseRelease->ObservationDescr1;
	NObs2	=	NurseRelease->ObservationDescr2;
	NDCom	=	NurseRelease->NotDiffComment;



	EXEC SQL declare	NurseRelInsCursor cursor for
	Insert	into NurseRelease
	(
		contractor,			contracttype,		termid ,
		idnumber,			idcode  ,			NurseError,
		hospitalcode,		obligationnumber,	hospitalnumber,   
		hospitaldate  ,		releasedate    ,  
		holidaydate1  ,		holidaydays1  ,  
		holidaydate2  ,		holidaydays2   ,  
		totalhospitaldays,	totalrightdays  , 
		totalnotrightdays,	doctornumber    ,
		sourcereception ,	descreception   ,
		releasecode     ,	institutecont   ,
		diffgroup1      ,	diffgroup2,
		operation1      ,	seqnumber1,			descr1          ,
		operation2      ,	seqnumber2,			descr2          ,
		memberlastname  ,	memberfirstname ,	doctorname      ,
		observationname1,	observationname2,
		commentnotdiff  ,	senddate    ,
		location    ,	locationprv ,
		position    ,	paymenttype     
	)
	values 
	(
			:NurseRelease->Contractor,
			:NurseRelease->ContractType,
			:NurseRelease->TermId,
			:NurseRelease->MemberId.Number	,
			:NurseRelease->MemberId.Code	,
			:NurseRelease->NurseError,
			:NurseRelease->HospitalId	,
			:NurseRelease->ObligationNumber,
			:HospN,
			:NurseRelease->HospitalDate	,
			:NurseRelease->ReleaseDate	,
			:NurseRelease->HolidayDate1	,
			:NurseRelease->HolidayDays1	,
			:NurseRelease->HolidayDate2	,
			:NurseRelease->HolidayDays2	,
			:NurseRelease->TotalHospitalDays,
			:NurseRelease->RightDays,
			:NurseRelease->NotRightDays,
			:NurseRelease->DoctorNumber	,
			:NurseRelease->SourceReception,
			:NurseRelease->DescReception,
			:NurseRelease->CodeRelease	,
			:NurseRelease->InstituteCont,
			:NurseRelease->DiffGroup1	,
			:NurseRelease->DiffGroup2	,
			:NOp1,
			:NurseRelease->SeqNumber1	,
			:NCh1,
			:NOp2,
			:NurseRelease->SeqNumber2	,
			:NCh2,
			:NFName,
			:NLName,
			:NDName,
			:NObs1,
			:NObs2,
			:NDCom,
			:NurseRelease->SendDate,
			:NurseRelease->Location,
			:NurseRelease->Locationprv,
			:NurseRelease->Position,
			:NurseRelease->PaymentType
		);
	EXEC SQL open	NurseRelInsCursor;
	JUMP_ON_ERR;


}
int DbPutNurseReleaseInsCursor(void)
{
	EXEC SQL Put	NurseRelInsCursor;
	JUMP_ON_ERR;
}
int DbCloseNurseReleaseInsCursor(void)
{
	EXEC SQL Close	NurseRelInsCursor;
	JUMP_ON_ERR;
}
/*20010101*/


int DbUpdateMemberCreditType( TMemberId *parMemberId, int parCreditType )
{
	EXEC SQL BEGIN DECLARE SECTION;
		TMemberId	*pMemberId  = parMemberId;
		int			nCreditType = parCreditType;
		TDbDateTime    LastDT = CurrDateTime;	
	EXEC SQL END DECLARE SECTION;

	EXEC SQL UPDATE membersyn
		SET 
			CreditType  = :nCreditType
			,lastupdatedate = :LastDT.DbDate,
			lastupdatetime = :LastDT.DbTime,
			updatedby  =55
		WHERE
			IdNumber = :pMemberId->Number AND
			IdCode   = :pMemberId->Code;

	SQLERR_error_test();
	return 1;
}

int DbOpenActionInsCursor( TActionRecord	*Ptr)
{
EXEC SQL BEGIN DECLARE SECTION;
	TActionRecord	*Action	=	Ptr;
EXEC SQL END DECLARE SECTION;
	EXEC SQL declare	ActionInsCursor cursor for
	insert	into action
	(
    		TermId,
	    	InstId,
    		Contractor,
	   	ContractType,
	    	Idcode,
	    	Idnumber,
	    	AuthDate,
	    	AuthTime,
	    	GnrlElegebility,
	    	Observation,
		ActionCode,
		ActionNumber,
	    	TreatmentContr,
	    	PayingInst,
	    	TreatmentDate,
	    	TreatmentTime,
	    	TreatmentPlace,
		Profession,
		Location,
		Locationprv,
		Position,
		PaymentType
	) values
	(
    		:Action->TermId,
	    	:Action->InstId,
    		:Action->Contractor,
	   	:Action->ContractType,
		:Action->MemberId.Code,
		:Action->MemberId.Number,
		:Action->AuthTime.DbDate,
		:Action->AuthTime.DbTime,
	    	:Action->GnrlElegebility,
	    	:Action->Observation,
	    	:Action->ActionCode,
	    	:Action->ActionNumber,
	    	:Action->TreatmentContr,
	    	:Action->PayingInst,
	    	:Action->TreatmentDate,
	    	:Action->TreatmentTime,
	    	:Action->TreatmentPlace,
		:Action->Profession,
		:Action->Location,
		:Action->Locationprv,
		:Action->Position,
		:Action->PaymentType
	);
	EXEC SQL open	ActionInsCursor;

	JUMP_ON_ERR;

	return(1);
}

int DbPutActionInsCursor(void)
{
	if( strlen(ActionRecord.Observation) != 9	)
	{
	memset(ActionRecord.Observation,32,sizeof(ActionRecord.Observation));
	}
	EXEC SQL put	ActionInsCursor;
	if(	SQLCODE ==	-391	)
	{

	printf("insert into Action -391 error\n");
	printf("Contractor %d Terminal %d\n",
		ActionRecord.Contractor,ActionRecord.TermId);

/*	TraceBuffer(33,&ActionRecord,sizeof(ActionRecord));  Yulia 07.97 */

	} 

	JUMP_ON_ERR;

	return(1);
}


int DbCloseActionInsCursor(void)
{
	EXEC SQL Close	ActionInsCursor;
	return(1);
}


/*20051205*/
int DbOpenActionAddInsCursor( TActionAddRecord	*Ptr)
{
EXEC SQL BEGIN DECLARE SECTION;
	TActionAddRecord	*Action	=	Ptr;
EXEC SQL END DECLARE SECTION;
	EXEC SQL declare	ActionAddInsCursor cursor for
	insert	into actionadd
	(
    		TermId,         /*1*/
	    	InstId,
    		Contractor,
		   	ContractType,
	    	Idcode,
	    	Idnumber,       /*6*/
	    	AuthDate,
	    	AuthTime,
	    	GnrlElegebility,
			ActionCode,     /*10*/
		   	TreatmentContr,
/*	    	PayingInst,*/
			FirstName,
			LastName,
			KodStorno,     /*15*/
			MemberPrice,
			MacabiPrice,
			Profession,
			Location,
			Locationprv,   /*20*/
			Position,
			PaymentType
	) values
	(
    		:Action->TermId,    /*1*/
	    	:Action->InstId,
    		:Action->Contractor,
		   	:Action->ContractType,
			:Action->MemberId.Code,
			:Action->MemberId.Number,  /*6*/
			:Action->AuthTime.DbDate,
			:Action->AuthTime.DbTime,
	    	:Action->GnrlElegebility,
	    	:Action->ActionCode,       /*10*/
	    	:Action->TreatmentContr,
	   /* 	:Action->PayingInst,*/
	    	:Action->FirstName,
			:Action->LastName,
			:Action->KodStorno,        /*15*/
			:Action->MemberPrice,
			:Action->MacabiPrice,
			:Action->Profession,
			:Action->Location,
			:Action->Locationprv,      /*20*/
			:Action->Position,
			:Action->PaymentType
	);
	EXEC SQL open	ActionAddInsCursor;

	JUMP_ON_ERR;

	return(1);
}

int DbPutActionAddInsCursor(void)
{
	EXEC SQL put	ActionAddInsCursor;
	if(	SQLCODE ==	-391	)
	{

	printf("insert into ActionAdd -391 error\n");
	printf("Contractor %d Terminal %d\n",
		ActionRecord.Contractor,ActionRecord.TermId);

/*	TraceBuffer(33,&ActionRecord,sizeof(ActionRecord));  Yulia 07.97 */

	} 

	JUMP_ON_ERR;

	return(1);
}


int DbCloseActionAddInsCursor(void)
{
	EXEC SQL Close	ActionAddInsCursor;
	return(1);
}

/*end 20051205*/

int DbOpenMedDataInsCursor( TMedicalDataRecord	*Ptr)
{
	EXEC SQL begin declare section;
	TMedicalDataRecord	*MedicalData	=	Ptr;
/*	TCObservationCode Obs1,Obs2,Obs3,Obs0;
	TCObservationCode Par1,Par2,Par3,Par0;
	TCCharNew6 Act0,Act1,Act2,Act3;
*/
	char *Obs1, *Obs2, *Obs3, *Obs0;
	char *Par1, *Par2, *Par3, *Par0;
	char *Act0, *Act1, *Act2, *Act3;

	EXEC SQL end declare section;

	Obs0 = MedicalData->Observation[0];
	Obs1 = MedicalData->Observation[1];
	Obs2 = MedicalData->Observation[2];
	Obs3 = MedicalData->Observation[3];

	Act0 = MedicalData->Action[0];
	Act1 = MedicalData->Action[1];
	Act2 = MedicalData->Action[2];
	Act3 = MedicalData->Action[3];

	Par0 = MedicalData->Param[0];
	Par1 = MedicalData->Param[1];
	Par2 = MedicalData->Param[2];
	Par3 = MedicalData->Param[3];

	EXEC SQL declare	MedDataInsCursor cursor for
	insert	into medicaldata
	(
    		Contractor,
	   	ContractType,
    		TermId,
	    	IdCode,
	    	IdNumber,
	    	RefContractor,
	    	RefTermId,
	    	ReqDate,
	    	TreatContractor,
	    	CauseObserv,
	    	TreatDate,
	    	TreatTime,
	    	PayingInst,
	    	ReferInst,
	    	ActionType,
	    	Observation0,
	    	Observation1,
	    	Observation2,
	    	Observation3,
		Action0,
		Action1,
		Action2,
		Action3,
		Param0,
		Param1,
		Param2,
		Param3,
		AnswerTitle,
		IdFile,
		AnswerCount,
		Location,
		Position,
		Paymenttype
	) values
	(
    		:MedicalData->Contractor,
	   	:MedicalData->ContractType,
    		:MedicalData->TermId,
		:MedicalData->MemberId.Code,
		:MedicalData->MemberId.Number,
	    	:MedicalData->RefContractor,
	    	:MedicalData->RefTermId,
	    	:MedicalData->ReqDate,
	    	:MedicalData->TreatContractor,
	    	:MedicalData->CauseObserv,
		:MedicalData->TreatTime.DbDate,
		:MedicalData->TreatTime.DbTime,
	    	:MedicalData->PayingInst,
	    	:MedicalData->ReferInst,
	    	:MedicalData->ActionType,
	    	:Obs0,
	    	:Obs1,
	    	:Obs2,
	    	:Obs3,
	    	:Act0, :Act1, :Act2, :Act3,
	    	:Par0, :Par1, :Par2, :Par3,
		:MedicalData->AnswerTitle,
		:MedicalData->IdFile,
		:MedicalData->AnswerCount,
		:MedicalData->Location,
		:MedicalData->Position,
		:MedicalData->Paymenttype
	);

	EXEC SQL open	MedDataInsCursor;

	JUMP_ON_ERR;

	return(1);
}

int DbPutMedicalDataInsCursor(void)
{
	if( strlen(MedicalDataRecord.Observation[0]) != 9	)
	{
	TraceBuffer(33,&MedicalDataRecord,sizeof(MedicalDataRecord));
	memset(MedicalDataRecord.Observation[0],32,sizeof(MedicalDataRecord.Observation[0]));
	}
	EXEC SQL put	MedDataInsCursor;
	if(	SQLCODE ==	-391	)
	{

	printf("insert into MedicalData -391 error\n");
	printf("Contractor %d Terminal %d\n",
		MedicalDataRecord.Contractor,MedicalDataRecord.TermId);

	TraceBuffer(33,&MedicalDataRecord,sizeof(MedicalDataRecord));

	} 

	JUMP_ON_ERR;

	return(1);
}


int DbCloseMedicalDataInsCursor(void)
{
	EXEC SQL Close	MedDataInsCursor;
	return(1);
}

int DbOpenMedAnswerInsCursor( TMedAnswerRecord	*Ptr)
{
EXEC SQL BEGIN DECLARE SECTION;
	TMedAnswerRecord	*MedAnswer	=	Ptr;
EXEC SQL END DECLARE SECTION;

	EXEC SQL declare	MedAnswerInsCursor cursor for
	insert	into medanswer
	(
    		Contractor,
	   	ContractType,
    		TermId,
	    	IdCode,
	    	IdNumber,
	    	TreatContractor,
	    	TreatDate,
	    	TreatTime,
	    	ActionType,
	    	LineNbr,
	    	LineType,
		AnswerText,
		IdFile,
		Location,
		Position,
		Paymenttype
	) values
	(
    		:MedAnswer->Contractor,
	   	:MedAnswer->ContractType,
    		:MedAnswer->TermId,
		:MedAnswer->MemberId.Code,
		:MedAnswer->MemberId.Number,
	    	:MedAnswer->TreatContractor,
		:MedAnswer->TreatTime.DbDate,
		:MedAnswer->TreatTime.DbTime,
	    	:MedAnswer->ActionType,
	    	:MedAnswer->LineNbr,
	    	:MedAnswer->LineType,
		:MedAnswer->AnswerText,
		:MedAnswer->IdFile,
		:MedAnswer->Location,
		:MedAnswer->Position,
		:MedAnswer->Paymenttype
	);
	EXEC SQL open	MedAnswerInsCursor;

	JUMP_ON_ERR;

	return(1);
}

int DbPutMedAnswerInsCursor(void)
{
	EXEC SQL put	MedAnswerInsCursor;
	if(	SQLCODE ==	-391	)
	{

	printf("insert into MedAnswer -391 error\n");
	printf("Contractor %d Terminal %d\n",
		MedAnswerRecord.Contractor,MedAnswerRecord.TermId);

	TraceBuffer(33,&MedAnswerRecord,sizeof(MedAnswerRecord));

	} 

	JUMP_ON_ERR;

	return(1);
}

int DbCloseMedAnswerInsCursor(void)
{
	EXEC SQL Close	MedAnswerInsCursor;
	return(1);
}

int DbOpenMedDataSelCursor( 	int		RefContractor,
				int		RefTermId,
				TMemberId	*RefMemberId)
{

EXEC SQL BEGIN DECLARE SECTION;
	int		Contractor	=	RefContractor;
	int		TermId		=	RefTermId;
	TMemberId	MemberId	=	*RefMemberId;
EXEC SQL END DECLARE SECTION;

	EXEC SQL declare	MedDataSelCursor	cursor	for
	select
		Contractor, 	ContractType, 	TermId,
		IdCode, 	IdNumber, 	ReqDate,
	    	TreatFirstName, TreatLastName, 	CauseObserv,
	    	TreatDate, 	TreatTime, 	ActionType,
	    	Observation0, 	Observation1, 	Observation2, 	Observation3
		Action0, 	Action1, 	Action2, 	Action3,
		Param0, 	Param1, 	Param2, 	Param3,
		AnswerTitle, 	IdFile, 	AnswerCount, 	StatusTerm
	from	medicaldata
	where	
		RefContractor	=	:Contractor		and
		RefTermId	=	:TermId	;
/*
		IdCode		=	:MemberId.Code		and
		IdNumber	=	:MemberId.Number;
*/
	EXEC SQL Open	MedDataSelCursor;

	JUMP_ON_ERR;

	return(1);
}

int DbFetchMedDataSelCursor( TMedicalDataRecord	*Ptr)
{
	EXEC SQL begin declare section;
	TMedicalDataRecord	*MedicalData	=	Ptr;
	TCObservationCode Obs1,Obs2,Obs3,Obs0;
	TCObservationCode Par1,Par2,Par3,Par0;
	TCCharNew6 Act0,Act1,Act2,Act3;
	EXEC SQL end declare section;

	EXEC SQL Fetch	MedDataSelCursor	into
	    	:MedicalData->Contractor,
	    	:MedicalData->ContractType,
	    	:MedicalData->TermId,
	    	:MedicalData->MemberId.Code,
	    	:MedicalData->MemberId.Number,
	    	:MedicalData->ReqDate,
	    	:MedicalData->TreatCFirstName,
	    	:MedicalData->TreatCLastName,
	    	:MedicalData->CauseObserv,
		:MedicalData->TreatTime.DbDate,
		:MedicalData->TreatTime.DbTime,
	    	:MedicalData->ActionType,
	    	:Obs0,
	    	:Obs1,
	    	:Obs2,
	    	:Obs3,
	    	:Act0, :Act1, :Act2, :Act3,
	    	:Par0, :Par1, :Par2, :Par3,
		:MedicalData->AnswerTitle,
		:MedicalData->IdFile,
		:MedicalData->AnswerCount,
		:MedicalData->StatusTerm;

	    	strcpy(MedicalData->Observation[0],	Obs0);
	    	strcpy(MedicalData->Observation[1],	Obs1);
	    	strcpy(MedicalData->Observation[2],	Obs2);
	    	strcpy(MedicalData->Observation[3],	Obs3);
	    	strcpy(MedicalData->Action[0],Act0);
	    	strcpy(MedicalData->Action[1],Act1);
	    	strcpy(MedicalData->Action[2],Act2);
	    	strcpy(MedicalData->Action[3],Act3);
	    	strcpy(MedicalData->Param[0],Par0);
	    	strcpy(MedicalData->Param[1],Par1);
	    	strcpy(MedicalData->Param[2],Par2);
	    	strcpy(MedicalData->Param[3],Par3);

	JUMP_ON_ERR;

	return(1);

}

int DbCloseMedDataSelCursor(void)
{
	EXEC SQL close	MedDataSelCursor;
	return(1);
}

int	DbUpdateMedData( TMedicalDataRecord	*Ptr)
{
EXEC SQL BEGIN DECLARE SECTION;
	TMedicalDataRecord *MedData	=	Ptr;
EXEC SQL END DECLARE SECTION;

	EXEC SQL update medicaldata	set
		StatusAs400		=	1
	where	
		RefContractor	=	:MedData->Contractor		and
		ContractType	=	:MedData->ContractType		and
		RefTermId	=	:MedData->TermId		and
		IdCode		=	:MedData->MemberId.Code		and
		IdNumber	=	:MedData->MemberId.Number;


	JUMP_ON_ERR;

	return(1);
}

int	DbUpdateMedAnswer( TMedAnswerRecord	*Ptr)
{
EXEC SQL BEGIN DECLARE SECTION;
	TMedAnswerRecord *MedAnswer	=	Ptr;
EXEC SQL END DECLARE SECTION;

	EXEC SQL update medanswer	set
		StatusAs400		=	1
	where	
		Contractor	=	:MedAnswer->Contractor		and
		ContractType	=	:MedAnswer->ContractType	and
		TermId		=	:MedAnswer->TermId		and
		IdFile		=	:MedAnswer->IdFile		and
		IdCode		=	:MedAnswer->MemberId.Code	and
		IdNumber	=	:MedAnswer->MemberId.Number;


	JUMP_ON_ERR;

	return(1);
}


int	DbUpdateMedDataTable( 	int		Contractor,
				int		TermId,
				TMemberId	*MemberId,
				int		IdFile)
{
EXEC SQL BEGIN DECLARE SECTION;
	TMedicalDataRecord 	MedData;
EXEC SQL END DECLARE SECTION;

	MedData.RefContractor		=	Contractor;
	MedData.RefTermId		=	TermId;
	MedData.MemberId		=	*MemberId;
	MedData.IdFile			=	IdFile;

	EXEC SQL update medicaldata	set
		StatusTerm	=	1
	where	
		RefContractor	=	:MedData.RefContractor		and
		RefTermId	=	:MedData.RefTermId		and
		IdCode		=	:MedData.MemberId.Code		and
		IdNumber	=	:MedData.MemberId.Number 	and
		IdFile		=	:MedData.IdFile;

	JUMP_ON_ERR;

	return(1);
}

int	DbUpdateMedAnswerTable( TMemberId	*MemberId,
				int		IdFile)
{
EXEC SQL BEGIN DECLARE SECTION;
	TMedAnswerRecord 	MedAnswer;
EXEC SQL END DECLARE SECTION;

	MedAnswer.MemberId		=	*MemberId;
	MedAnswer.IdFile		=	IdFile;

	EXEC SQL update medanswer	set
		StatusTerm	=	1
	where	
		IdFile		=	:MedAnswer.IdFile		and
		IdCode		=	:MedAnswer.MemberId.Code	and
		IdNumber	=	:MedAnswer.MemberId.Number;

	JUMP_ON_ERR;

	return(1);
}

int	DbOpenMedDataSelRrnCursor( TRecRelNumber RecRelNumb)
{
EXEC SQL BEGIN DECLARE SECTION;
	long int	Rrn_min;
EXEC SQL END DECLARE SECTION;

	Rrn_min = RecRelNumb;

	EXEC SQL declare	MedDataRrnCurs	cursor	for
	select
    		Contractor,
	   	ContractType,
    		TermId,
	    	IdCode,
	    	IdNumber,
	    	RefContractor,
	    	RefTermid,
	    	ReqDate,
	    	TreatContractor,
	    	TreatFirstName,
	    	TreatLastName,
	    	CauseObserv,
	    	TreatDate,
	    	TreatTime,
	    	PayingInst,
	    	ReferInst,
	    	ActionType,
	    	Observation0,
	    	Observation1,
	    	Observation2,
	    	Observation3,
		Action0,
		Action1,
		Action2,
		Action3,
		Param0,
		Param1,
		Param2,
		Param3,
		AnswerTitle,
		IdFile,
		AnswerCount,
		StatusAs400,
		StatusTerm,
		Location,
		Position,
		Paymenttype,
		Rrn 
	from	medicaldata	
	where
		Rrn > :Rrn_min;

	EXEC SQL open	MedDataRrnCurs;

	return(1);
}

int	DbFetchMedDataSelRrnCursor( TMedicalDataRecord *Ptr)
{
	EXEC SQL begin declare section;
	TMedicalDataRecord	*MedicalData	=	Ptr;
	TCObservationCode Obs1,Obs2,Obs3,Obs0;
	TCObservationCode Par1,Par2,Par3,Par0;
	TCCharNew6 Act0,Act1,Act2,Act3;
	EXEC SQL end declare section;

	EXEC SQL fetch	MedDataRrnCurs	into
    		:MedicalData->Contractor,
	   	:MedicalData->ContractType,
    		:MedicalData->TermId,
		:MedicalData->MemberId.Code,
		:MedicalData->MemberId.Number,
	    	:MedicalData->RefContractor,
	    	:MedicalData->RefTermId,
	    	:MedicalData->ReqDate,
	    	:MedicalData->TreatContractor,
	    	:MedicalData->TreatCFirstName,
	    	:MedicalData->TreatCLastName,
	    	:MedicalData->CauseObserv,
		:MedicalData->TreatTime.DbDate,
		:MedicalData->TreatTime.DbTime,
	    	:MedicalData->PayingInst,
	    	:MedicalData->ReferInst,
	    	:MedicalData->ActionType,
	    	:Obs0,
	    	:Obs1,
	    	:Obs2,
	    	:Obs3,
	    	:Act0, :Act1, :Act2, :Act3,
	    	:Par0, :Par1, :Par2, :Par3,
		:MedicalData->AnswerTitle,
		:MedicalData->IdFile,
		:MedicalData->AnswerCount,
		:MedicalData->StatusAs400,
		:MedicalData->StatusTerm,
		:MedicalData->Location,
		:MedicalData->Position,
		:MedicalData->Paymenttype,
		:MedicalData->Rrn;
	    	strcpy(MedicalData->Observation[0],	Obs0);
	    	strcpy(MedicalData->Observation[1],	Obs1);
	    	strcpy(MedicalData->Observation[2],	Obs2);
	    	strcpy(MedicalData->Observation[3],	Obs3);
	    	strcpy(MedicalData->Action[0],Act0);
	    	strcpy(MedicalData->Action[1],Act1);
	    	strcpy(MedicalData->Action[2],Act2);
	    	strcpy(MedicalData->Action[3],Act3);
	    	strcpy(MedicalData->Param[0],Par0);
	    	strcpy(MedicalData->Param[1],Par1);
	    	strcpy(MedicalData->Param[2],Par2);
	    	strcpy(MedicalData->Param[3],Par3);

	return(1);
}

int	DbCloseMedDataSelRrnCursor(void)
{
	EXEC SQL close	MedDataRrnCurs;
	return(1);
}

int	DbOpenMedAnswerSelRrnCursor( TRecRelNumber RecRelNumb)
{
EXEC SQL BEGIN DECLARE SECTION;
	long int	Rrn_min;
EXEC SQL END DECLARE SECTION;

	Rrn_min = RecRelNumb;

	EXEC SQL declare	MedAnsRrnCursor	cursor	for
	select
    		Contractor,
	   	ContractType,
    		TermId,
	    	IdCode,
	    	IdNumber,
	    	TreatContractor,
	    	TreatDate,
	    	TreatTime,
	    	ActionType,
	    	LineNbr,
	    	LineType,
		AnswerText,
		IdFile,
		StatusAs400,
		StatusTerm,
		Location,
		Position,
		Paymenttype,
		Rrn 
	from	medanswer	
	where
		Rrn > :Rrn_min;

	EXEC SQL open	MedAnsRrnCursor;
	return(1);
}

int	DbFetchMedAnswerSelRrnCursor( TMedAnswerRecord *Ptr)
{
EXEC SQL BEGIN DECLARE SECTION;
	TMedAnswerRecord	*MedAnswer	=	Ptr;
EXEC SQL END DECLARE SECTION;

	EXEC SQL fetch	MedAnsRrnCursor	into
    		:MedAnswer->Contractor,
		:MedAnswer->ContractType,
    		:MedAnswer->TermId,
		:MedAnswer->MemberId.Code,
		:MedAnswer->MemberId.Number,
		:MedAnswer->TreatContractor,
		:MedAnswer->TreatTime.DbDate,
		:MedAnswer->TreatTime.DbTime,
		:MedAnswer->ActionType,
		:MedAnswer->LineNbr,
    		:MedAnswer->LineType,
		:MedAnswer->AnswerText,
		:MedAnswer->IdFile,
		:MedAnswer->StatusAs400,
		:MedAnswer->StatusTerm,
		:MedAnswer->Location,
		:MedAnswer->Position,
		:MedAnswer->Paymenttype,
		:MedAnswer->Rrn;

	return(1);
}

int	DbCloseMedAnswerSelRrnCursor(void)
{
	EXEC SQL close	MedAnsRrnCursor;
	return(1);
}

int DbOpenMedAnsSelCursor( TMedicalDataRecord	*Ptr)
{
EXEC SQL BEGIN DECLARE SECTION;
	TMedicalDataRecord *MedData	=	Ptr;
EXEC SQL END DECLARE SECTION;

	EXEC SQL declare	MedAnsSelCursor	cursor	for
	select distinct
	    	LineNbr,
	    	LineType,
		AnswerText
	from	medanswer
	where	
		Contractor	=	:MedData->Contractor		and
		ContractType	=	:MedData->ContractType		and
		TermId		=	:MedData->TermId		and
		IdFile		=	:MedData->IdFile		and
		IdCode		=	:MedData->MemberId.Code		and
		IdNumber	=	:MedData->MemberId.Number;


	EXEC SQL open	MedAnsSelCursor;

	JUMP_ON_ERR;

	return(1);
}

int DbFetchMedAnsSelCursor( TMedAnswerRecord	*Ptr)
{
EXEC SQL BEGIN DECLARE SECTION;
	TMedAnswerRecord	*MedAnswer	=	Ptr;
EXEC SQL END DECLARE SECTION;

	EXEC SQL Fetch	MedAnsSelCursor	into
	    :MedAnswer->LineNbr,
	    :MedAnswer->LineType,
	    :MedAnswer->AnswerText;

	JUMP_ON_ERR;

	return(1);
}


int DbCloseMedAnsSelCursor(void)
{
	EXEC SQL close	MedAnsSelCursor;
	return(1);
}


int	DbAuthCheckLocation( 	TIdNumber	Contractor,
				TContractType	ContractType,
				TMemberId	*MemberId,
				TLocationId	Location)
{
EXEC SQL BEGIN DECLARE SECTION;
	TAuthRecord 	Auth;
	TMemberId	*Member =	MemberId;
EXEC SQL END DECLARE SECTION;

	Auth.Contractor	=	Contractor;
	Auth.ContractType	=	ContractType;
	Auth.Location	=	Location;

	EXEC SQL declare	AuthChekCursor	cursor	for

	select
		Contractor, 	ContractType, 	IdCode,
		IdNumber, 	Location
	from	auth	
	where
		Contractor 	= 	:Auth.Contractor   	and
		ContractType 	= 	:Auth.ContractType 	and
		Location     	= 	:Auth.Location 	  	and 
		IdCode		=  	:Member->Code	  	and
		IdNumber	=  	:Member->Number		and 
		Cancled < 2;

	EXEC SQL open	AuthChekCursor;

	return(1);
}
int DbGetMahozStatus( short mahoz_0, short mahoz_1 , short *status )
{
EXEC SQL BEGIN DECLARE SECTION;
	short SqlStatus;
	short SqlMahoz_0;
	short SqlMahoz_1;
EXEC SQL END DECLARE SECTION;

	SqlMahoz_0 = mahoz_0;
	SqlMahoz_1 = mahoz_1;

	EXEC SQL select MahozStatus  into		:SqlStatus   from	mahozot
	where	Mahoz0	= :SqlMahoz_0  and		Mahoz1	= :SqlMahoz_1;

	*status = SqlStatus;
	if ( sqlca.sqlcode !=0 )
	{
		return (0);
	}
	return(1);
}
int DbGetMahozContractor( TAuthRecord *Auth , short *mahoz)
{
EXEC SQL BEGIN DECLARE SECTION;
	short 		SqlMahoz;
	TAuthRecord	*SqlAuth;
EXEC SQL END DECLARE SECTION;
	SqlAuth = Auth;

	EXEC SQL select Mahoz  into	:SqlMahoz   from	conterm
	where	TermId	 	= :SqlAuth->TermId		and
			Contractor	= :SqlAuth->Contractor	and
			ContractType= :SqlAuth->ContractType;

	*mahoz = SqlMahoz;
	if ( sqlca.sqlcode !=0 )
	{
		return (0);
	}
	return(1);
}

/*Begin 20080121 Yulia*/
int DbGetCityStatus( int city_0, int city_1 , short *status )
{
EXEC SQL BEGIN DECLARE SECTION;
	short SqlStatus;
	int SqlCity_0;
	int SqlCity_1;
EXEC SQL END DECLARE SECTION;

	SqlCity_0 = city_0;
	SqlCity_1 = city_1;

	EXEC SQL select Status  into		:SqlStatus   from	cities
	where	City0	= :SqlCity_0  and		City1	= :SqlCity_1;

	*status = SqlStatus;
	if ( sqlca.sqlcode !=0 )
	{
		return (0);
	}
	return(1);
}
int DbGetCityContractor( TAuthRecord *Auth , int *city)
{
EXEC SQL BEGIN DECLARE SECTION;
	int 		SqlCity;
	TAuthRecord	*SqlAuth;
EXEC SQL END DECLARE SECTION;
	SqlAuth = Auth;

	EXEC SQL select city  into	:SqlCity   from	conterm
	where	TermId	 	= :SqlAuth->TermId		and
			Contractor	= :SqlAuth->Contractor	and
			ContractType= :SqlAuth->ContractType;

	*city = SqlCity;
	if ( sqlca.sqlcode !=0 )
	{
		GerrLogFnameMini("city_log",GerrId,"macsql:Can't find city of Contractor:%d-%d in TermId:%d city[%d][%d]",
							 SqlAuth->Contractor,SqlAuth->ContractType, SqlAuth->TermId,SqlCity,sqlca.sqlcode);
		return (0);
	}
	return(1);
}

/*end 20080121*/
 /*============================================================================
||									  ||
||		DbDeleteConMembRecordNotUnique()		  	  ||
||									  ||
|| Delete from conmemb according to member's IdNumber (not unique key)	  ||
||									  ||
||			ELI REUVENI - 3.12.96			  	  ||
 =============================================================================*/
int DbDeleteConMembRecordNotUnique(TMemberId *MemberId)
{
EXEC SQL BEGIN DECLARE SECTION;
	TConMembRecord ConMemb;
EXEC SQL END DECLARE SECTION;

	ConMemb.MemberId = *MemberId;

	EXEC SQL delete from	conmemb
	where	
		IdNumber = :ConMemb.MemberId.Number	and
		IdCode	 = :ConMemb.MemberId.Code;

	return(1);
}


int	DbOpenDatabase( char *DbName)
{
/*	$char	NameBuf[200];

	strcpy(NameBuf,DbName);

	EXEC SQL database	:NameBuf;
	if(	SQLCODE ==	0	)
	{
	EXEC SQL set	Lock	Mode to wait 20;
	EXEC SQL set	Isolation to dirty read;
	}
*/	return(1);
}

int	DbSetExplain(
int	OnOff
)
{
	if(	OnOff	)
	{
	EXEC SQL set	explain on;
	} else
	{
	EXEC SQL set	explain off;
	}
	return(1);
}

void	DbBeginWork()
{
	int SqlCode =	SQLCODE;

	if(	TransactionLevel++	)	return;

	EXEC SQL begin	work;

//	MDbCheckSqlError(SqlAllowNoError);
	SQLERR_error_test();

	SQLCODE =	SqlCode;
}

void	DbEndWork(void)
{
	int SqlCode =	SQLCODE;

	if(	--TransactionLevel	)	return;
	EXEC SQL commit	work;

//	MDbCheckSqlError(SqlAllowNoError);
	SQLERR_error_test();

	SQLCODE =	SqlCode;
}
   
void TraceBuffer ( 	Int16 a,
			void * b,
			TBufSize c)
{
}

size_t TruncSpace ( 	void *a,
		size_t b,
	 	short c)
{
}
int DbInsertTaxRecord(  TIdNumber   Contractor,
			TContractType   ContractType,
			TIdNumber       Number,
			Int16           Code,
			TDateYYYYMMDD   AuthDate,
			Int32           AuthTime,
			TTermId         TermId,
			Int16           TaxAmount,
			Int16           TaxCalc)
{
EXEC SQL BEGIN DECLARE SECTION;
TTaxSendRecord Tax;
EXEC SQL END DECLARE SECTION;

Tax.Contractor      = Contractor;
Tax.ContractType    = ContractType;
Tax.MemberId.Number = Number;
Tax.MemberId.Code   = Code;
Tax.AuthTime.DbDate = AuthDate;
Tax.AuthTime.DbTime = AuthTime;
Tax.TermId          = TermId;
Tax.TaxAmount       = TaxAmount;
Tax.TaxCalc         = TaxCalc;


	EXEC SQL Insert Into    TaxSend
(	Contractor,         ContractType,
	IdNumber,           IdCode,
	AuthDate,           AuthTime,
	TermId,             TaxAmount,	    TaxCalc
)   values
    (
    :Tax.Contractor,        :Tax.ContractType,
    :Tax.MemberId.Number,   :Tax.MemberId.Code,
    :Tax.AuthTime.DbDate,   :Tax.AuthTime.DbTime,
    :Tax.TermId,            :Tax.TaxAmount,	:Tax.TaxCalc
);
    
    JUMP_ON_ERR;
    return (!sqlca.sqlcode); /* if insert OK sqlcode = 0 and return 1 */
 /* else return 0                         */
 }


int DbUpdateAuthTaxRecord(  TIdNumber   Contractor,
			TContractType   ContractType,
			TIdNumber       Number,
			Int16           Code,
			TDateYYYYMMDD   AuthDate,
			Int32           AuthTime,
			TTermId         TermId,
			Int16           TaxAmount)
{
EXEC SQL BEGIN DECLARE SECTION;
    TAuthRecord AuthRec;
EXEC SQL END DECLARE SECTION;

    AuthRec.Contractor      = Contractor;
    AuthRec.ContractType    = ContractType;
    AuthRec.MemberId.Number = Number;
    AuthRec.MemberId.Code   = Code;
    AuthRec.AuthTime.DbDate = AuthDate;
    AuthRec.AuthTime.DbTime = AuthTime;
    AuthRec.TermId          = TermId;
//    AuthRec.TaxAmount       = TaxAmount;

/*    EXEC SQL UPDATE AUTH     set tax = :AuthRec.TaxAmount
    where 
        Contractor	= :AuthRec.Contractor and 
        ContractType	= :AuthRec.ContractType and 
        IdNumber	= :AuthRec.MemberId.Number and
	IdCode		= :AuthRec.MemberId.Code and 
	AuthDate	= :AuthRec.AuthTime.DbDate and 
	AuthTime	= :AuthRec.AuthTime.DbTime and 
	TermId		= :AuthRec.TermId;
*/
	if( SQLERR_code_cmp( SQLERR_access_conflict ) == MAC_TRUE )
	{
//		if( sqlca.sqlerrd[2] > 1 )
//		{
//		printf("DocSqlServer : Rows updated %d\n", sqlca.sqlerrd[2] );

		printf("DocSqlServer : ************ Record locked ***********\n");
		printf("DocSqlServer : AuthRec.Contractor      = %d\n", AuthRec.Contractor );
		printf("DocSqlServer : AuthRec.ContractType    = %d\n", AuthRec.ContractType );
		printf("DocSqlServer : AuthRec.MemberId.Number = %d\n", AuthRec.MemberId.Number );
		printf("DocSqlServer : AuthRec.MemberId.Code   = %d\n", AuthRec.MemberId.Code );
		printf("DocSqlServer : AuthRec.AuthTime.DbDate = %d\n", AuthRec.AuthTime.DbDate );
		printf("DocSqlServer : AuthRec.AuthTime.DbTime = %d\n", AuthRec.AuthTime.DbTime );
		printf("DocSqlServer : AuthRec.TermId          = %d\n", AuthRec.TermId );
		printf("DocSqlServer : **************************************\n");
		fflush(stdout);
//		}
	}
    JUMP_ON_ERR;
 }

int DbOpenMsgTxtSelCursor( int MsgID)
{
    
	EXEC SQL BEGIN DECLARE SECTION;    
	int v_MsgID = MsgID;
	EXEC SQL END DECLARE SECTION;

	EXEC SQL declare	MsgTxtSelCursor	cursor	for
	select  message_line, message_text  
	from	message_texts_doc  
	where	
		message_id 	=	:v_MsgID
        order by message_line;


	EXEC SQL open	MsgTxtSelCursor;

	JUMP_ON_ERR;

	return(1);   
}

int DbFetchMsgTxtSelCursor( char *pline)
{
	EXEC SQL BEGIN DECLARE SECTION;
		char	v_line[80];	// DonR 22Aug2005: Avoid compiler warning.
							// DonR 24Aug2005: fixed length of buffer - was 1 char too long.
		int v_line_num;
	EXEC SQL END DECLARE SECTION;
	

	// Make sure the string is null-terminated.
	// DonR 24Aug2005: fixed length of buffer - was 1 char too long.
	v_line[79] = (char)0;

	EXEC SQL Fetch	MsgTxtSelCursor	into
	    :v_line_num,:v_line;

	// DonR 22Aug2005: To avoid compiler warning, we now define v_line
	// explicitly - and thus we have to copy its contents to the output
	// variable.
	strcpy (pline, v_line);

	JUMP_ON_ERR;

	return(1);
}

int DbCloseMsgTxtSelCursor(void)
{
	EXEC SQL close	MsgTxtSelCursor;
	return(1);
}

short DbGetCalendarDay(short StUrgent,short DayOfYear,short Year)
{
	EXEC SQL BEGIN DECLARE SECTION;
	short v_StUrgent,v_Year;
	char	v_str[367];
	EXEC SQL END DECLARE SECTION;

	v_StUrgent = StUrgent;
	v_Year = Year;

	EXEC SQL Select days_string  into :v_str
		from calendar
		where year = :v_Year and nationality = :v_StUrgent;

	JUMP_ON_ERR;

	return (v_str[DayOfYear]);

}
//20020805
int DbGetLabTerm(TLabRecord	*Ptr)
{
EXEC SQL BEGIN DECLARE SECTION;
	TLabRecord *Lab	=	Ptr;
	int count_TID,termid_upd;//conterm
	int location_ct;
	int altcontr;
EXEC SQL END DECLARE SECTION;
int flag_break_conterm =0 ;
int flag_break_subst   =0;
// char fname[]="/pharm/log/labtermupd.step";
// FILE *OUTTEST;
// OUTTEST = fopen(fname,"a");
    Lab->TermId = 0;

	// DonR 10Sep2020: If we don't have an embedded Informix connection, don't
	// bother with any SQL - just return before generating error messages.
	if (!EmbeddedInformixConnected)
	{
		return (1);
	}

//0
		while (1)
		{
			EXEC SQL select count ( distinct termid ) into :count_TID 	from conterm
			where contractor = :Lab->Contractor;
			if( SQLERR_code_cmp( SQLERR_access_conflict ) == MAC_TRUE )
       		{
				continue;   /*243 */
			}
			SQLERR_error_test();
			break;
	    }	
	    if ( count_TID <= 1 ) 
		{
//			fprintf(OUTTEST,"\npoint1 [%9d]",Lab->Contractor);
//			fclose(OUTTEST);
			return (1);
		}
		
//1.AUTH 
		while (2)
		{
			EXEC SQL select max(termid) 	into 	:termid_upd
			from 	auth 
			where	idnumber   = :Lab->MemberId.Number  and
					idcode     = :Lab->MemberId.Code	and 	
					contractor = :Lab->Contractor		and 
					(autherror		< 4000 or autherror		 = 5400 )	
					and cancled <2;
			if( SQLERR_code_cmp( SQLERR_access_conflict ) == MAC_TRUE )
       	 	{
				continue;
			}
			SQLERR_error_test();
			break;
	    }	
	    if (termid_upd < 0) 
		{
			termid_upd = 0;
		}
		else
		{
			Lab->TermId	= termid_upd;
//			fprintf(OUTTEST,"\npoint2 [%9d][%9d][%7d]",Lab->Contractor,Lab->MemberId.Number,Lab->TermId);
//			fclose(OUTTEST);
			return(1);
		}
//2.AUTHPREV 
		while (3)
		{
			EXEC SQL select max(termid) 	into 	:termid_upd
			from 	authprev 
			where	idnumber   = :Lab->MemberId.Number  and
					idcode     = :Lab->MemberId.Code	and 	
					contractor = :Lab->Contractor		and 
					(autherror		< 4000 or autherror		 = 5400 )	
			and cancled <2;
			if( SQLERR_code_cmp( SQLERR_access_conflict ) == MAC_TRUE )
       	 	{
				continue;
			}
			SQLERR_error_test();
			break;
	    }	
	    if (termid_upd < 0) 
		{
			termid_upd = 0;
		}
		else
		{
			Lab->TermId	= termid_upd;
//			fprintf(OUTTEST,"\npoint3 [%9d][%9d][%7d]",Lab->Contractor,Lab->MemberId.Number,Lab->TermId);
//			fclose(OUTTEST);
			return(1);
		}
//3.VISHISTORY
		while (4)
		{
			EXEC SQL select max(termid) 	into 	:termid_upd
			from 	vishistory 
			where	idnumber   = :Lab->MemberId.Number  and
					idcode     = :Lab->MemberId.Code	and 	
					contractor = :Lab->Contractor		;  
			if( SQLERR_code_cmp( SQLERR_access_conflict ) == MAC_TRUE )
       	 	{
				continue;
			}
			SQLERR_error_test();
			break;
	    }	
	    if (termid_upd < 0) 
		{
			termid_upd = 0;
		}
		else
		{
			Lab->TermId	= termid_upd;
//			fprintf(OUTTEST,"\npoint4 [%9d][%9d][%7d]",Lab->Contractor,Lab->MemberId.Number,Lab->TermId);
//			fclose(OUTTEST);
			return(1);
		}
//4.CONTERM LOCATION
		EXEC SQL  DECLARE conterm_curs cursor for 	
			SELECT location from conterm
			WHERE 	 contractor = :Lab->Contractor;
		SQLERR_error_test();

		EXEC SQL open conterm_curs;
		SQLERR_error_test();

		while(5)
		{
		    location_ct = 0;
		    while (10)
		    {
				EXEC SQL fetch next conterm_curs into :location_ct;
          		if( SQLERR_code_cmp( SQLERR_end_of_fetch ) == MAC_TRUE )
       			{
				    flag_break_conterm = 1;
       			    break;
       			}
 				if( SQLERR_code_cmp( SQLERR_access_conflict ) == MAC_TRUE )
       		 	{
				    continue;
				}
				SQLERR_error_test();
				break;
		    }    
		    if ( flag_break_conterm ) break;
//41:CHECK AUTH
		    while (11)
		    {
				EXEC SQL select max(termid) into 	:termid_upd
    			from 	auth 
				where contractor	= :location_ct			and
					idnumber		= :Lab->MemberId.Number and 	
					idcode			= :Lab->MemberId.Code	and 	
					(autherror		< 4000 or autherror		 = 5400 )	;  
				if( SQLERR_code_cmp( SQLERR_access_conflict ) == MAC_TRUE )
       	 		{
					continue;   /*243 */
				}
				SQLERR_error_test();
				break;
		    }
			if (termid_upd < 0) 
			{
				termid_upd = 0;
			}
			else
			{
				Lab->TermId	= termid_upd;
				EXEC SQL close conterm_curs;
				SQLERR_error_test();
//				fprintf(OUTTEST,"\npoint51[%9d][%9d][%7d]",Lab->Contractor,Lab->MemberId.Number,Lab->TermId);
//				fclose(OUTTEST);
				return(1);
			}
//42:CHECK OLD AUTH
		    while (12)
		    {
				EXEC SQL select max(termid) into 	:termid_upd
    			from 	authprev 
				where contractor	= :location_ct			and
					idnumber		= :Lab->MemberId.Number and 	
					idcode			= :Lab->MemberId.Code	and 	
					(autherror		< 4000 or autherror		 = 5400 )	;  
				if( SQLERR_code_cmp( SQLERR_access_conflict ) == MAC_TRUE )
       	 		{
					continue;   /*243 */
				}
				SQLERR_error_test();
				break;
		    }
			if (termid_upd < 0) 
			{
				termid_upd = 0;
			}
			else
			{
				Lab->TermId	= termid_upd;
				EXEC SQL close conterm_curs;
				SQLERR_error_test();
//				fprintf(OUTTEST,"\npoint52[%9d][%9d][%7d]",Lab->Contractor,Lab->MemberId.Number,Lab->TermId);
//				fclose(OUTTEST);
				return(1);
			}
		} 
		EXEC SQL close conterm_curs;
		SQLERR_error_test();
//5: alt from subst
		EXEC SQL  DECLARE alt_curs cursor for 	
			SELECT altcontractor from subst
			WHERE 	 contractor = :Lab->Contractor;
		SQLERR_error_test();

		EXEC SQL open alt_curs;
		SQLERR_error_test();
		while(6)
		{
		    location_ct = 0;
		    while (10)
		    {
				EXEC SQL fetch next alt_curs into :altcontr;
          		if( SQLERR_code_cmp( SQLERR_end_of_fetch ) == MAC_TRUE )
       			{
				    flag_break_subst = 1;
       			    break;
       			}
 				if( SQLERR_code_cmp( SQLERR_access_conflict ) == MAC_TRUE )
       		 	{
				    continue;
				}
				SQLERR_error_test();
				break;
		    }    
		    if ( flag_break_subst ) break;
//51:CHECK AUTH
		    while (11)
		    {
				EXEC SQL select max(termid) into 	:termid_upd
    			from 	auth 
				where contractor	= :altcontr				and
					idnumber		= :Lab->MemberId.Number and 	
					idcode			= :Lab->MemberId.Code   and 	
					(autherror		< 4000 or autherror		 = 5400 )	;  
				if( SQLERR_code_cmp( SQLERR_access_conflict ) == MAC_TRUE )
       	 		{
					continue;   /*243 */
				}
				SQLERR_error_test();
				break;
		    }
			if (termid_upd < 0) 
			{
				termid_upd = 0;
			}
			else
			{
				Lab->TermId	= termid_upd;
//				fprintf(OUTTEST,"\npoint61[%9d][%9d][%7d]",Lab->Contractor,Lab->MemberId.Number,Lab->TermId);
				EXEC SQL close alt_curs;
				SQLERR_error_test();
//				fclose(OUTTEST);
				return(1);
			}
//52:CHECK OLD AUTH
		    while (12)
		    {
				EXEC SQL select max(termid) into 	:termid_upd
    			from 	authprev 
				where contractor	= :altcontr				and
					idnumber		= :Lab->MemberId.Number and 	
					idcode			= :Lab->MemberId.Code   and 	
					(autherror		< 4000 or autherror		 = 5400 )	;  
				if( SQLERR_code_cmp( SQLERR_access_conflict ) == MAC_TRUE )
       	 		{
					continue;   /*243 */
				}
				SQLERR_error_test();
				break;
		    }
			if (termid_upd < 0) 
			{
				termid_upd = 0;
			}
			else
			{
				Lab->TermId	= termid_upd;
//				fprintf(OUTTEST,"\npoint62[%9d][%9d][%7d]",Lab->Contractor,Lab->MemberId.Number,Lab->TermId);
				EXEC SQL close alt_curs;
				SQLERR_error_test();
//				fclose(OUTTEST);
				return(1);
			}
		} 
		EXEC SQL close alt_curs;
		SQLERR_error_test();
	termid_upd =0;
    Lab->TermId = 0;
	GerrLogFnameMini("labtermupdOL_log",GerrId, "point7[%9d][%9d][%7d]",Lab->Contractor,Lab->MemberId.Number,Lab->TermId);
	return(1);
}

int DbGetLastClicksVersDate(TDateYYYYMMDD *LastClicksVersionDate)
{
EXEC SQL BEGIN DECLARE SECTION;	
	int v_lastclicksdate;
EXEC SQL END DECLARE SECTION;


	EXEC SQL 		SELECT
			lastclicksdate
			into 	:v_lastclicksdate
			from sysparams
			where sysparamskey    =   1;
	JUMP_ON_ERR;
	*LastClicksVersionDate = v_lastclicksdate;
	return(1);
}

/*20030520 */
int DbOpenMembDrugCursor(	TMemberId	*MemberId)
{
	EXEC SQL BEGIN DECLARE SECTION;
		TMemberId	v_MemberId;
		TDateYYYYMMDD	Today;
	EXEC SQL END   DECLARE SECTION;
	
	v_MemberId.Number	=	MemberId->Number;
	v_MemberId.Code		=	MemberId->Code;
	Today				= GetDate();

	EXEC SQL declare	MembDrugsCurs	cursor with hold for
		SELECT	a.doctor_id,	
		        largo_code,	
				date,	
				max(first_name),	max(last_name),
		        max(phone)
		FROM	prescription_drugs a,outer doctors b
		WHERE	member_id	=	:v_MemberId.Number	and
		mem_id_extension	=	:v_MemberId.Code	and 
		a.doctor_id			=	b.doctor_id	
		and stop_blood_Date	>	:Today
		and delivered_flg	=	1
		and a.del_flg			=	0
		group by 1,2,3
		order by largo_code;

   	SQLERR_error_test();
	
	EXEC SQL open	MembDrugsCurs;
	SQLERR_error_test();

	return(1);

}
int DbFetchMembDrugCursor(	int			*LargoMemb,
							TIdNumber	*ContractorMemb,
							char		*FirstNameMemb,
							char		*LastNameMemb,
							char		*PhoneMemb,
							TDateYYYYMMDD	*LargoDlvDateMemb,
							char		*DrType
							)
{
	EXEC SQL BEGIN DECLARE SECTION;
		int			v_LargoMemb;
		TIdNumber	v_ContractorMemb;
		char		v_FirstNameMemb[8+1];
		char		v_LastNameMemb[14+1];
		char		v_PhoneMemb[10+1];
		TDateYYYYMMDD	v_LargoDlvDateMemb;
		char		v_largo_type[1+1];
		int v_Ind;
	EXEC SQL END   DECLARE SECTION;
    EXEC SQL fetch	next MembDrugsCurs into
		:v_ContractorMemb,
		:v_LargoMemb,
		:v_LargoDlvDateMemb,
		:v_FirstNameMemb:v_Ind,	
		:v_LastNameMemb,
		:v_PhoneMemb;

		if( SQLERR_code_cmp( SQLERR_end_of_fetch ) == MAC_TRUE )
		{
			return(0);
		}
		if(  SQLERR_error_test() ) 
		{
			return(0);
		}
/*	GerrLogFnameMini("141_log",GerrId,"point3.3 after fetch 1[%d][%d][%d][%s][%s][%d]",
		v_ContractorMemb,
		v_LargoMemb,
		v_LargoDlvDateMemb,
		v_FirstNameMemb,	
		v_LastNameMemb,
		sqlca.sqlcode);
*/
	if (v_Ind != 0)
	{
		sprintf(v_FirstNameMemb,"%8s", "        ");
		sprintf(v_LastNameMemb,"%14s","              ");
		sprintf(v_PhoneMemb,"%10s","          ");
	}

	/*must check only D drugs*/
	EXEC SQL select largo_type into :v_largo_type from drug_list
	where largo_code= :v_LargoMemb;
//t	if(  SQLERR_error_test() ) 
//t	{
//t		return(0);
//t	}

	*LargoMemb			=	v_LargoMemb;
	*ContractorMemb		=	v_ContractorMemb;
	*LargoDlvDateMemb	=	v_LargoDlvDateMemb;
	strcpy(LastNameMemb,v_LastNameMemb);
	strcpy(FirstNameMemb,v_FirstNameMemb);
	strcpy(PhoneMemb,v_PhoneMemb);
	strcpy(DrType,v_largo_type);	
	return (1);

}
int	DbCloseMembDrugCursor()
{
	EXEC SQL close MembDrugsCurs ;
	SQLERR_error_test();
}

int	DbGetInteractionStatus(	int MinLargo,	int MaxLargo,
							short *IntCode,	char *IntText)
{		
	EXEC SQL BEGIN DECLARE SECTION;
		int			v_MinLargo	=	MinLargo;
		int			v_MaxLargo	=	MaxLargo;
		short		v_IntCode;
		char		v_IntText[ 40 + 1];
	EXEC SQL END   DECLARE SECTION;
	
	EXEC SQL
		  SELECT	interaction_type,	note1
		  INTO		:v_IntCode,			:v_IntText
		  FROM		drug_interaction
		  WHERE		first_largo_code = :v_MinLargo	AND
					second_largo_code = :v_MaxLargo;
//GerrLogFnameMini("141_log",GerrId,"interaction select [%d][%d][%d]",sqlca.sqlcode,MinLargo,	MaxLargo);
		if( SQLERR_code_cmp( SQLERR_not_found ) == MAC_TRUE )
		{
			return (0);
		}
		if( SQLERR_error_test() )
		{
			return (0);
		}
		strcpy ( IntText , v_IntText);
		*IntCode	=	v_IntCode;
		return (1);
}

/*20030520 end*/

int	DbInsertDrugNotPrioryRecord( TDrugNotPrioryRecord	*Ptr)
{
	EXEC SQL BEGIN DECLARE SECTION;
		TDrugNotPrioryRecord *Drug	=	Ptr;
		int InsDate,InsTime;
	EXEC SQL END DECLARE SECTION;
	InsDate = GetDate();
	InsTime = GetTime();

	EXEC SQL Insert Into	DrugNotPrior
	(
    contractor,
    contracttype,
    termid,
    idcode,
    idnumber,
    authdate,
    authtime,
    treatmentcontr,
    largo_code,
	largo_code2,
	quantitytext,
    quantity,
    douse,
    days,
    reason,
    reasontext,
    argument,
    argumenttext,
    reported_to_as400,
	insertdate,
	inserttime

	)	values
	(
	:Drug->Contractor, 	
	:Drug->ContractType,
	:Drug->TermId	,
	:Drug->MemberId.Code	,
	:Drug->MemberId.Number	,
	:Drug->AuthDate	,
	:Drug->AuthTime	,
	:Drug->TreatmentContr,
	:Drug->LargoPriory,
	:Drug->Largo	,
	:Drug->QuantityText,
    :Drug->Quantity	,
    :Drug->Douse	,
    :Drug->Days		,
	:Drug->Reason	,
	:Drug->ReasonText	,
	:Drug->Argument,
	:Drug->ArgumentText	,
    0,
	:InsDate,
	:InsTime
	);
//	GerrLogFnameMini("143_log",GerrId,		"point5.3[%d]",sqlca.sqlcode);
	return(1);
}



/*
int DbOpenConsAskOneTextCursor(	TMemberId	*MemberId,
								short		Profession,
								TIdNumber	Consultant,
								short		ConsType,
								TDateYYYYMMDD ConsDate,
								TTimeHHMMSS	ConsTime,
								int			TextCounter
							)
{
	EXEC SQL BEGIN DECLARE SECTION;
		TMemberId	v_MemberId;
		short		v_Profession	=	Profession;
		TIdNumber	v_Consultant	=	Consultant;
		short		v_ConsType		=	ConsType;
		TDateYYYYMMDD v_ConsDate	=	ConsDate;
		TTimeHHMMSS	v_ConsTime		=	ConsTime;
		short		v_TextCounter	=	TextCounter;
	EXEC SQL END   DECLARE SECTION;

	v_MemberId.Number	= MemberId->Number;
	v_MemberId.Code		= MemberId->Code;
	
	EXEC SQL declare	ConsAskOneTextCurs	cursor with hold for
	select			text,		textsize
	from	CONSULTANTTEXT 
	where	contractor		=	:v_Consultant		and
			contracttype	=	:v_ConsType			and
			consultantdate	=	:v_ConsDate			and
			consultanttime	=	:v_ConsTime			and 
			idnumber		=	:v_MemberId.Number	and
			idcode			=	:v_MemberId.Code	and
			TextCounter		=	:v_TextCounter;
//	GerrLogFnameMini("172_log",GerrId,	"point7.5.1 : fetch [%d][%d][%d][%d][%d][%d]",
//		sqlca.sqlcode,
//		v_Consultant,
//		v_ConsType,
//		v_ConsDate,
//		v_ConsTime,
//		v_MemberId.Number);	
	SQLERR_error_test();
					
	EXEC SQL open	ConsAskOneTextCurs;

	SQLERR_error_test();

	return(1);
}
int DbFetchConsAskOneTextCursor(	char		*text,short *TextSize)
{
	EXEC SQL BEGIN DECLARE SECTION;
		short			v_TextSize;
		char			v_text[255 + 1];
	EXEC SQL END   DECLARE SECTION;

	EXEC SQL fetch next ConsAskOneTextCurs into 
		:v_text,	:v_TextSize;
//	GerrLogFnameMini("172_log",GerrId,	"point7.6.1 : fetch [%d][%d][%s]",sqlca.sqlcode,v_TextSize,v_text);	
	
	if( SQLERR_code_cmp( SQLERR_end_of_fetch ) == MAC_TRUE )
	{
		return(0);
	}
	else  if(  SQLERR_error_test() ) 
	{
		return(0);
	}
	*TextSize		=	v_TextSize;
//	GerrLogFnameMini("172_log",GerrId,	"point7.6.2 : fetch [%d][%d][%s]",sqlca.sqlcode,TextSize,text);	
	strcpy(text,v_text);
//	GerrLogFnameMini("172_log",GerrId,	"point7.6.3 : fetch [%d][%d][%s]",sqlca.sqlcode,TextSize,text);	
	return (1);

}
int DbCloseConsAskOneTextCursor()
{
	EXEC SQL close	ConsAskOneTextCurs;
	SQLERR_error_test();
	return(1);

}
*/
/*20031028*/
void	DbBeginAuditRecord(short MsgCode,
						   int   ProcId,
						   int	 MsgCount_in)
{
	static int	GotComputerName = 0;

	EXEC SQL BEGIN DECLARE SECTION;
		short		v_MsgCode;
		int			v_ProcId;		
		int			v_Date,v_Time;
		int			v_MsgCount = MsgCount_in;
		static char	MyShortName [3];
	EXEC SQL END DECLARE SECTION;

	v_Date		= GetDate();
	v_Time		= GetTime();
	v_MsgCode	= MsgCode;
	v_ProcId	= ProcId;

	// DonR 30Jun2005: Get two-character short name for this computer.
	if (!GotComputerName)
	{
		GotComputerName = 1;
		strcpy (MyShortName, GetComputerShortName());
	}

	EXEC SQL INSERT INTO sql_srv_audit_doc
		(
			procid,
			computer_id,
			date,
			time,
			msgcode,
			in_progress,
			err_code,
			msg_count
		)
		VALUES	
		(
			:v_ProcId,
			:MyShortName,
			:v_Date,
			:v_Time,
			:v_MsgCode,
			1,
			0,
			:v_MsgCount
		);
		
		if (SQLCODE == INF_NOT_UNIQUE_INDEX)
		{
			EXEC SQL	UPDATE	sql_srv_audit_doc
			SET		date		= :v_Date,
					time		= :v_Time,
					computer_id	= :MyShortName,
					msgcode		= :v_MsgCode,
					in_progress	= 1,
					err_code	= 0,
					msg_count	= :v_MsgCount
				WHERE	procid = :v_ProcId;
		}
}
void	DbEndAuditRecord(  int   ProcId,int MacErrno)
{
	EXEC SQL BEGIN DECLARE SECTION;
		int		v_MacErrno;
		int		v_ProcId;		
	EXEC SQL END DECLARE SECTION;
	v_ProcId	= ProcId;
	v_MacErrno  = MacErrno;
	EXEC SQL
			UPDATE	sql_srv_audit_doc
			SET		in_progress	= 0,
					err_code	= :v_MacErrno
			WHERE	procid = :v_ProcId;


}

/*end20031028*/
/*Yulia 20040627*/
int DbGetSpeechClientRecord( 	TMemberId	*MemberId,
				TLocationId 	Location)
{
EXEC SQL BEGIN DECLARE SECTION;
	TLocationId 		v_location	=	Location;
	TDateYYYYMMDD		SysDate;
	TMemberId			*Member=MemberId;
	int	v_count;
EXEC SQL END DECLARE SECTION;
	
	SysDate = GetDate();
//GerrLogFnameMini ("speech_cl_log", GerrId, "in macsql before[%d]",SysDate);
//	GerrLogFnameMini ("speech_cl_log", GerrId, "in macsql before [%d][%d]",
//		MemberId->Number,Member->Number);

	EXEC SQL select	count(*) into :v_count
	from	SPEECHCLIENT
	where	
		IdCode		=	:Member->Code		and
		IdNumber	=	:Member->Number		and 
		location	=	:v_location			and
		fromdate	<=	:SysDate			and 
		ToDate		>=	:SysDate;
	GerrLogFnameMini ("speech_cl_log", GerrId, "count[%d]id[%d]loc[%d](%d)",
					  v_count,
					  Member->Number,
					  v_location,
					  sqlca.sqlcode);

	JUMP_ON_ERR;

	if(	v_count	==	0 )return(0);
		return(1);
}
/*end   20040627*/
int DbGetMahoz(int Contractor,short ContractType,int TermId)
{
	
EXEC SQL BEGIN DECLARE SECTION;
int		v_Contractor	= Contractor;
short	v_ContractType	= ContractType;
int		v_TermId		= TermId;
int		v_mahoz;
EXEC SQL END DECLARE SECTION;

EXEC SQL select mahoz into :v_mahoz from conterm
	where	
		TermId		=	:v_TermId		and
		Contractor	=	:v_Contractor	and
		ContractType=	:v_ContractType;

	if ( SQLERR_error_test() )
	{
	return (0);
	}
	else
	{
	return v_mahoz;
	}

}

void	DbGetGnrlElegTrans(int ElegParm,char *msgtext,int *eleg_tr,int *msg_lines)
{
	EXEC SQL begin declare section;
	int v_Eleg = ElegParm;
	char v_msgText[22];
	int v_msg_l,v_msgcode,v_eleg_tr;
	EXEC SQL end declare section;
	int i,j;
	msgtext [0] = (char)0;

//	GerrLogFnameMini("54_msg_log",GerrId,	"point1[%d]",v_Eleg);
	EXEC SQL select msgcode,	eleg_to_clicks 
			 into   :v_msgcode,:v_eleg_tr
			from gnrlelegtrans
			where gnrlelegebility = :v_Eleg;
	if( SQLERR_code_cmp( SQLERR_no_rows_affected ) == MAC_TRUE )       
	{
		v_msgcode = 0;
		*eleg_tr = v_Eleg;
		*msg_lines=0;
		return;
	}
	else
	{
		if (v_msgcode>0)
		{	
			EXEC SQL DECLARE msglinecurs cursor		for
			SELECT msgline, elegtext from gnrlelegtext 
			where msgcode = :v_msgcode
			order by msgline;
			EXEC SQL open msglinecurs ;

			i=0;
			while (1)
			{
				v_msgText[ 0] = (char)0;
				EXEC SQL fetch next msglinecurs into 
					:v_msg_l,:v_msgText;

				if( SQLERR_code_cmp( SQLERR_end_of_fetch ) == MAC_TRUE )
				{
					break;
				}
//				GerrLogFnameMini("54_msg_log",GerrId,"point5[%d][%s]",	strlen(msgtext),msgtext);

				StrRev1(v_msgText);
				if (v_msgcode == 8 || v_msgcode == 14|| v_msgcode == 16) //only for long message problem
				{	
					v_msgText[21]='\n';
				}
				strncat(msgtext,v_msgText,22);
				i++;
			}
			*eleg_tr =v_eleg_tr;
			*msg_lines=v_msg_l;

		}
		else  //have no message but translate gnrlelegebility 11,12 =>1
		{
			*eleg_tr =v_eleg_tr;
			*msg_lines=0;
		}
//		GerrLogFnameMini("3015_log",GerrId,"point5[%d][%s]",
//					strlen(msgtext),msgtext);

		return;
	}
}

void    StrRev1(char * Buf)
{
	int     I;
	int     Len;

	Len     =    strlen(Buf);
	Len--;
	for( I   =    0;I <  Len;I++,Len-- )
	{
		char    Temp;
		Temp            =    Buf[I];
		Buf[I]          =    Buf[Len];
		Buf[Len]        =    Temp;
	}
}
/*20050928*/
int DbCheckSapatMember( 	TMemberId	*MemberId)
{
EXEC SQL BEGIN DECLARE SECTION;
	TMemberId			*Member=MemberId;
	int	v_count;
EXEC SQL END DECLARE SECTION;
	

	EXEC SQL select	count(*) into :v_count
	from	SAPAT
	where	
		IdCode		=	:Member->Code		and
		IdNumber	=	:Member->Number		and 
		del_flg		=	0;
//	GerrLogFnameMini("3015_log",GerrId,	"in sql sapat[%d][%d]sqlcode[%d]",	 MemberId->Number,v_count,sqlca.sqlcode);

	JUMP_ON_ERR;

	if(	v_count	==	0 )return(0);
		return(1);
}
/*end   20050928*/
