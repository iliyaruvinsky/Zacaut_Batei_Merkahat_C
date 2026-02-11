#define INFORMIX_MODE
EXEC SQL BEGIN DECLARE SECTION;
	static  short int   REPORTED_TO_AS400 =	1;
	static  short NOT_REPORTED_TO_AS400 =	0;
EXEC SQL END DECLARE SECTION;


EXEC SQL include	sqlca;
EXEC SQL include	"Global.h";
EXEC SQL include	"GenSql.h";

//EXEC SQL include	"DB.h";
//EXEC SQL include	"DBFields.h";
EXEC SQL include	"multix.h";
EXEC SQL include	"tpmintr.h";
EXEC SQL include	"global_1.h";
static char *GerrSource = __FILE__;

/*DbInsertSpeechClientRecord Yulia 20040624*/
int	DbInsertSpeechClientRecord( TSpeechClientRecord	*Ptr)
{
	EXEC SQL BEGIN DECLARE SECTION;
	TSpeechClientRecord *SpeechCl	=	Ptr;
	int InsDate,InsTime;
	EXEC SQL END DECLARE SECTION;
	InsDate = GetDate();
	InsTime = GetTime();
	EXEC SQL Insert Into	SpeechClient
	(
		 idnumber,
		 idcode,
		 location,
		 fromdate,
		 todate,
		 insertdate,
		 inserttime
		)
		VALUES(	       
		:SpeechCl->MemberId.Number,
		:SpeechCl->MemberId.Code,
		:SpeechCl->Location,
		:SpeechCl->FromDate,
		:SpeechCl->ToDate,
		:InsDate,
		:InsTime   );
	if ( SQLERR_error_test())
	{
		GerrLogMini (GerrId,"3011[%d]",sqlca.sqlcode);
	}

	return(1);
}

/*end 20040624*/
int	DbInsertHospitalRecord( THospitalRecord	*Ptr)
{
	EXEC SQL BEGIN DECLARE SECTION;
	THospitalRecord *Hosp	=	Ptr;
	int InsDate,InsTime;
	EXEC SQL END DECLARE SECTION;
	InsDate = GetDate();
	InsTime = GetTime();
	EXEC SQL Insert Into	HospitalMsg
	(
		 contractor,
		 termid,
		 idnumber,
		 idcode,
		 hospdate,
		 nursedate,
		 contrname,
		 hospitalnumb,
		 typeresult,
		 rescount,
		 resname1,
		 resultsize,
		 result,
		 hospname,
		 departname,
		 flg_system,
		 patalogyflg,
		 insertdate,
		 inserttime
		)
		VALUES(	       
		:Hosp->Contractor,
		:Hosp->TermId,
		:Hosp->MemberId.Number,
		:Hosp->MemberId.Code,
		:Hosp->HospDate,
		:Hosp->NurseDate,
		:Hosp->ContrName,
		:Hosp->HospitalNumb,
		:Hosp->TypeResult,
		:Hosp->ResCount,
		:Hosp->ResName1,
		:Hosp->ResultSize,
		:Hosp->Result,
		:Hosp->HospName,
		:Hosp->DepartName,
		:Hosp->FlgSystem,
		:Hosp->PatFlg,
		:InsDate,
		:InsTime   );
	if ( SQLERR_error_test())
	{
		GerrLogMini (GerrId,"3011[%d]",sqlca.sqlcode);
	}
	return(1);
}

int	DbInsertRashamRecord( TRashamRecord	*Ptr)
{
	EXEC SQL BEGIN DECLARE SECTION;
	TRashamRecord *Rasham	=	Ptr;
	int InsDate,InsTime;
	EXEC SQL END DECLARE SECTION;
	InsDate = GetDate();
	InsTime = GetTime();
	EXEC SQL Insert Into	Rashamas400
	(
		contractor,
		 contracttype,
		 termid,
		 idnumber,
		 idcode,
		 kodrasham,
		 rasham,
		 updatedate,
		 insertdate,
		 inserttime
			)
		VALUES(	       
		:Rasham->Contractor,
		:Rasham->ContractType,
		:Rasham->TermId,
		:Rasham->MemberId.Number,
		:Rasham->MemberId.Code,
		:Rasham->KodRasham,
		:Rasham->Rasham,
		:Rasham->UpdateDate,
		:InsDate,
		:InsTime   );
	if( SQLERR_code_cmp( SQLERR_ok ) == MAC_FALS )
	{
		GerrLogMini (GerrId,"3012[%d] insert[%d]id[%d]kod[%d]",sqlca.sqlcode,
			Rasham->MemberId.Number,
			Rasham->KodRasham);
			return(0);
	}
	return(1);
}
int	DbUpdateRashamRecord( TRashamRecord	*Ptr)
{
	EXEC SQL BEGIN DECLARE SECTION;
	TRashamRecord *Rasham	=	Ptr;
	int InsDate,InsTime;
	EXEC SQL END DECLARE SECTION;
	InsDate = GetDate();
	InsTime = GetTime();
	EXEC SQL update	Rashamas400 set 
		contractor 	=	:Rasham->Contractor,
		rasham		=	:Rasham->Rasham,
		updatedate	=	:Rasham->UpdateDate,
		insertdate =   :InsDate,
		inserttime =   :InsTime
		WHERE
		idnumber = :Rasham->MemberId.Number and 
		idcode	 = :Rasham->MemberId.Code and 
		kodrasham =	:Rasham->KodRasham;
		if( SQLERR_code_cmp( SQLERR_ok ) == MAC_FALS )
	{
		GerrLogMini (GerrId,"3012[%d]",sqlca.sqlcode);
		return (0);
	}
	return(1);
}
/*20050201*/
int	DbInsertMemberMessageRecord( TMemberMessageRecord	*Ptr)
{
	EXEC SQL BEGIN DECLARE SECTION;
	TMemberMessageRecord *Message	=	Ptr;
	int InsDate,InsTime;
	EXEC SQL END DECLARE SECTION;
	InsDate = GetDate();
	InsTime = GetTime();
	EXEC SQL Insert Into	update_member_msg
	(
		 idnumber,
		 idcode,
		 msg_index,
		 line_index,
		 text,
		 insertdate,
		 inserttime
			)
		VALUES(	       
		:Message->MemberId.Number,
		:Message->MemberId.Code,
		:Message->MessageNo,
		:Message->LineNo,
		:Message->Message,
		:InsDate,
		:InsTime   );
	if ( SQLERR_error_test())
	{
		GerrLogFnameMini ("3013_log",GerrId,"3013[%d]insert",sqlca.sqlcode);
	}
	return(1);
}
/*
commented by Yulia 20061101
int	DbUpdateMemberMessageRecord( TMemberMessageRecord	*Ptr)
{
	EXEC SQL BEGIN DECLARE SECTION;
	TMemberMessageRecord *Message	=	Ptr;
	int InsDate,InsTime;
	EXEC SQL END DECLARE SECTION;
	InsDate = GetDate();
	InsTime = GetTime();
	GerrLogFnameMini ("3013_log",GerrId,"3013before update[%d][%d][%d][%s]",
	Message->MemberId.Number,
	Message->MessageNo,
	Message->LineNo,
	Message->Message);
	EXEC SQL update	update_member_msg
	set text = :Message->Message,
	insertdate=:InsDate,
	inserttime=:InsTime
	where 
		 idnumber	=	:Message->MemberId.Number	and
		 idcode		=	:Message->MemberId.Code		and
		 msg_index	=	:Message->MessageNo			and
		 line_index	=	:Message->LineNo;
GerrLogFnameMini ("3013_log",GerrId,"3013code[%d]update",sqlca.sqlcode);		 
	if ( SQLERR_error_test())
	{
		GerrLogFnameMini ("3013_log",GerrId,"3013[%d]update",sqlca.sqlcode);
	}
	return(1);
}
Yulia 20061101*/
int	DbDeleteMemberMessageRecord( TMemberMessageRecord	*Ptr)
{
	EXEC SQL BEGIN DECLARE SECTION;
	TMemberMessageRecord *Message	=	Ptr;
	EXEC SQL END DECLARE SECTION;
	GerrLogFnameMini ("3013_log",GerrId,"3013before delete[%d]",
	Message->MemberId.Number);
	EXEC SQL delete from update_member_msg
	where 
		 idnumber	=	:Message->MemberId.Number	and
		 idcode		=	:Message->MemberId.Code;
	if ( SQLERR_error_test())
	{
		GerrLogFnameMini ("3013_log",GerrId,"3013[%d]delete for member[%d]",sqlca.sqlcode,Message->MemberId.Number);
	}
	return(1);
}


/**/
/*20050828*/
int	DbInsertMemberMammoRecord( TMemberId	*Ptr,int Birthdate)
{
	EXEC SQL BEGIN DECLARE SECTION;
	TMemberId *MemberId	=	Ptr;
	int v_Birthdate=Birthdate;
	int InsDate,InsTime;
	EXEC SQL END DECLARE SECTION;
	InsDate = GetDate();
	InsTime = GetTime();
	EXEC SQL Insert Into	mammo2
	(
		 idnumber,
		 idcode,
		 birthdate,
		 insertdate,
		 inserttime
			)
		VALUES(	       
		:MemberId->Number,
		:MemberId->Code,
		:v_Birthdate,
		:InsDate,
		:InsTime   );
//	if ( SQLERR_error_test())
//	{
//		GerrLogFnameMini ("3014_log",GerrId,"3014[%d]insert",sqlca.sqlcode);
//	}
	return(1);
}
int	DbUpdateMemberMammoRecord( TMemberId	*Ptr,int Birthdate)
{
	EXEC SQL BEGIN DECLARE SECTION;
	TMemberId *MemberId	=	Ptr;
	int v_Birthdate=Birthdate;
	int InsDate,InsTime;
	EXEC SQL END DECLARE SECTION;
	InsDate = GetDate();
	InsTime = GetTime();

	EXEC SQL update	mammo2
		set birthdate = :v_Birthdate,
			insertdate=:InsDate,
			inserttime=:InsTime
	where 
		 idnumber	=	:MemberId->Number	and
		 idcode		=	:MemberId->Code	;
//	if ( SQLERR_error_test())
//	{
//		GerrLogFnameMini ("3014_log",GerrId,"3014[%d]update",sqlca.sqlcode);
//	}
	return(1);
}

/*end 20050828*/

int	DbInsertMemberSapatRecord( TMemberId	*Ptr)
{
	EXEC SQL BEGIN DECLARE SECTION;
	TMemberId *MemberId	=	Ptr;
	int InsDate,InsTime;
	EXEC SQL END DECLARE SECTION;
	InsDate = GetDate();
	InsTime = GetTime();
	EXEC SQL Insert Into	sapat
	(
		 idnumber,
		 idcode,
		 del_flg,
		 insertdate,
		 inserttime
			)
		VALUES(	       
		:MemberId->Number,
		:MemberId->Code,
		0,
		:InsDate,
		:InsTime   );
	if ( SQLERR_error_test())
	{
		GerrLogFnameMini ("3015_log",GerrId,"3015[%d]insert[%d]",sqlca.sqlcode,MemberId->Number);
	}
	return(1);
}

int	DbUpdateMemberSapatRecord( TMemberId	*Ptr)
{
	EXEC SQL BEGIN DECLARE SECTION;
	TMemberId *MemberId	=	Ptr;
	int InsDate,InsTime;
	EXEC SQL END DECLARE SECTION;
	InsDate = GetDate();
	InsTime = GetTime();

	EXEC SQL update	sapat
		set del_flg = 1,
			insertdate=:InsDate,
			inserttime=:InsTime
	where 
		 idnumber	=	:MemberId->Number	and
		 idcode		=	:MemberId->Code	;
	GerrLogFnameMini ("3015_log",GerrId,"3016[%d]in DbUpdateMemberSapatRecord [%d]",sqlca.sqlcode,MemberId->Number);

	if ( SQLERR_error_test())
	{
		GerrLogFnameMini ("3015_log",GerrId,"3016[%d]update problem[%d]",sqlca.sqlcode,MemberId->Number);
	}
	return(1);
}
/*20060302*/
int	DbInsertMemberRemarkRecord( TMemberId	*Ptr,short remark,int valid)
{
	EXEC SQL BEGIN DECLARE SECTION;
	TMemberId *MemberId	=	Ptr;
	int InsDate,InsTime;
	int v_validuntil = valid;
	short v_remark=remark;
	EXEC SQL END DECLARE SECTION;
	InsDate = GetDate();
	InsTime = GetTime();
	EXEC SQL delete from memberremarks where 
		idnumber	= 	:MemberId->Number	and
		idcode		=	:MemberId->Code /*	and  Yulia 20061016
 		remarkcode	=	:v_remark       */;
	if( SQLERR_code_cmp( SQLERR_not_found ) == MAC_TRUE )
	{
	}
	else if(  SQLERR_error_test() ) 
	{
		GerrLogFnameMini ("3017_log",GerrId,"3017[%d]delete problem[%d]",
			sqlca.sqlcode,MemberId->Number);
	    return 0;
	}

	EXEC SQL Insert Into	memberremarks
	(
		 idnumber,
		 idcode,
		 remarkcode,
		 validuntil,
		 insertdate,
		 inserttime
			)
		VALUES(	       
		:MemberId->Number,
		:MemberId->Code,
		:v_remark,
		:v_validuntil,
		:InsDate,
		:InsTime   );
	if ( SQLERR_error_test())
	{
		GerrLogFnameMini ("3017_log",GerrId,"3017[%d]insert[%d]",sqlca.sqlcode,MemberId->Number);
	}
	return(1);
}


int	DbInsertClicksTblUpdRecord( TClicksTblUpdRecord	*Ptr)
{
	EXEC SQL BEGIN DECLARE SECTION;
	TClicksTblUpdRecord *ClicksTbl	=	Ptr;
	int InsDate,InsTime;
	EXEC SQL END DECLARE SECTION;

	InsDate = GetDate();
	InsTime = GetTime();

	EXEC SQL Insert Into Clicks_Tbl_Upd 
	(
		update_date,
		update_time,
		key,
		action_type,
		kod_tbl,
		descr,
		small_descr,
		algoritm,
		add_fld1,
		add_fld2,
		add_fld3,
		add_fld4,
		add_fld5,
		add_fld6,
		times,
		insertdate,
		inserttime
	)
		VALUES(	       
		:ClicksTbl->update_date,
		:ClicksTbl->update_time,
		:ClicksTbl->key,
		:ClicksTbl->action_type,
		:ClicksTbl->kod_tbl,
		:ClicksTbl->descr,
		:ClicksTbl->small_descr,
		:ClicksTbl->algoritm,
		:ClicksTbl->add_fld1,
		:ClicksTbl->add_fld2,
		:ClicksTbl->add_fld3,
		:ClicksTbl->add_fld4,
		:ClicksTbl->add_fld5,
		:ClicksTbl->add_fld6,
		:ClicksTbl->times,
		:InsDate,
		:InsTime   );
	//	GerrLogFnameMini ("3018_log",   GerrId,"point8[%d]",sqlca.sqlcode);
	if ( SQLERR_error_test())
	{
		GerrLogFnameMini ("3018_log",GerrId,"3018[%d]insert",sqlca.sqlcode);
	}
	return(1);
}
int	DbInsertGastroRecord( TGastroRecord	*Ptr)
{
	EXEC SQL BEGIN DECLARE SECTION;
	TGastroRecord *Gastro	=	Ptr;
	int InsDate,InsTime;
	EXEC SQL END DECLARE SECTION;

	InsDate = GetDate();
	InsTime = GetTime();

	EXEC SQL Insert Into locterm
	(
		termid,
		contractor,
		contracttype,
		treatmentcontr,
		realcontracttype,
		servicetype,
		locationprv,
		validfrom,
		validuntil,
		updatedby,
		status,
		insertdate,
		inserttime
	)
		VALUES(	       
		:Gastro->TermId,
		:Gastro->Contractor,
		:Gastro->ContractType,
		:Gastro->TreatmentContr,
		:Gastro->AuthType,
		0,
		:Gastro->Location,
		:Gastro->ValidFrom,
		:Gastro->ValidUntil,
		7777,
		:Gastro->StatusRec,
		:InsDate,
		:InsTime   );
	if ( SQLERR_error_test())
	{
		GerrLogFnameMini ("3019_log",GerrId,"3019[%d]insert",sqlca.sqlcode);
	}
	return(1);
}
int	DbUpdateGastroRecord( TGastroRecord	*Ptr)
{
	EXEC SQL BEGIN DECLARE SECTION;
	TGastroRecord *Gastro	=	Ptr;
	int InsDate,InsTime;
	EXEC SQL END DECLARE SECTION;

	InsDate = GetDate();
	InsTime = GetTime();

	EXEC SQL update locterm 
	set  	termid 				= :Gastro->TermId,
			realcontracttype	= :Gastro->AuthType,
			servicetype			= 0,
			locationprv			= :Gastro->Location,
			validfrom			= :Gastro->ValidFrom,
			validuntil			= :Gastro->ValidUntil,
			updatedby			=	7777,
			status				= :Gastro->StatusRec,
			insertdate			= :InsDate,
			inserttime			= :InsTime
		where
		contractor			=	 :Gastro->Contractor 	and 
		contracttype 		=	 :Gastro->ContractType	and 
		treatmentcontr		=	 :Gastro->TreatmentContr;
	if ( SQLERR_error_test())
	{
		GerrLogFnameMini ("3019_log",GerrId,"3019[%d]insert",sqlca.sqlcode);
	}
	return(1);
}
int	DbInsertMembContrRecord( TMemberContrRecord	*Ptr)
{
	EXEC SQL BEGIN DECLARE SECTION;
	TMemberContrRecord *MembContr	=	Ptr;
	int InsDate,InsTime;
	EXEC SQL END DECLARE SECTION;

	InsDate = GetDate();
	InsTime = GetTime();
	EXEC SQL delete from membercontr
	where idnumber = :MembContr->MemberId.Number
	and idcode = :MembContr->MemberId.Code;
	if ( SQLERR_error_test())
	{
		GerrLogFnameMini ("3023_log",GerrId,"3023[%d]delete",sqlca.sqlcode);
	}
	EXEC SQL Insert Into membercontr
	(
		termid,
		contractor,
		contracttype,
		idnumber,
		idcode,
		grade,
		insertdate,
		inserttime
	)
		VALUES(	       
		:MembContr->TermId,
		:MembContr->Contractor,
		:MembContr->ContractType,
		:MembContr->MemberId.Number,
		:MembContr->MemberId.Code,
		:MembContr->Grade,
		:InsDate,
		:InsTime   );
	if ( SQLERR_error_test())
	{
		GerrLogFnameMini ("3023_log",GerrId,"3023[%d]insert",sqlca.sqlcode);
	}
	return(1);
}
int	DbUpdateMembContrRecord( TMemberContrRecord	*Ptr)
{
	EXEC SQL BEGIN DECLARE SECTION;
	TMemberContrRecord *MembContr	=	Ptr;
	int InsDate,InsTime;
	EXEC SQL END DECLARE SECTION;
	InsDate = GetDate();
	InsTime = GetTime();
	EXEC SQL update membercontr
	set  	termid 				= :MembContr->TermId,
			contractor			= :MembContr->Contractor ,
			contracttype 		= :MembContr->ContractType,
			grade				= :MembContr->Grade,
			insertdate			= :InsDate,
			inserttime			= :InsTime
		where
			idnumber 	=		:MembContr->MemberId.Number	and
			idcode 		=		:MembContr->MemberId.Code;
	if ( SQLERR_error_test())
	{
		GerrLogFnameMini ("3023_log",GerrId,"3023[%d]id[%d]update",sqlca.sqlcode,MembContr->MemberId.Number);
	}
	return(1);
}
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
		ServiceType	=	:Conterm->ServiceType,
		City = :Conterm->City,
		Add1 = :Conterm->Add1,
		Add2 = :Conterm->Add2
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
		ServiceType,	City,	Add1,	Add2
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
		:Conterm->Snif,			:Conterm->ServiceType,
		:Conterm->City,	:Conterm->Add1, :Conterm->Add2
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
int	DbInsertLabAdminRecord( TLabAdminRecord	*Ptr)
{
	EXEC SQL BEGIN DECLARE SECTION;
	TLabAdminRecord *LabAdminRec	=	Ptr;
	int InsDate,InsTime;
	int v_status_lr,v_status_ln,v_status_clr,v_status_lro,v_status_lno,v_status_ll;
	int v_del_lr,v_del_ln,v_del_lro,v_del_lno,v_del_ll;	
	EXEC SQL END DECLARE SECTION;

	InsDate = GetDate();
	InsTime = GetTime();
	EXEC SQL select count(*) into :v_status_lr from  labresults
	where contractor = :LabAdminRec->Contractor
	  and idnumber   = :LabAdminRec->MemberId.Number
	  and idcode     = :LabAdminRec->MemberId.Code
	  and mod(reqid,100000000) = :LabAdminRec->ReqIdMain;
	EXEC SQL delete from  labresults
	where contractor = :LabAdminRec->Contractor
	  and idnumber   = :LabAdminRec->MemberId.Number
	  and idcode     = :LabAdminRec->MemberId.Code
	  and mod(reqid,100000000) = :LabAdminRec->ReqIdMain;
v_del_lr = sqlca.sqlcode;
EXEC SQL select count(*) into :v_status_ln from labnotes
	where contractor = :LabAdminRec->Contractor
	  and idnumber   = :LabAdminRec->MemberId.Number
	  and idcode     = :LabAdminRec->MemberId.Code
	  and mod(reqid,100000000) = :LabAdminRec->ReqIdMain;
EXEC SQL delete from labnotes
	where contractor = :LabAdminRec->Contractor
	  and idnumber   = :LabAdminRec->MemberId.Number
	  and idcode     = :LabAdminRec->MemberId.Code
	  and mod(reqid,100000000) = :LabAdminRec->ReqIdMain;
v_del_ln = sqlca.sqlcode;
	EXEC SQL select count(*) into :v_status_clr from  conflabres
	where contractor = :LabAdminRec->Contractor
	  and idnumber   = :LabAdminRec->MemberId.Number
	  and idcode     = :LabAdminRec->MemberId.Code
	  and mod(reqid,100000000) = :LabAdminRec->ReqIdMain;
//	  v_status_clr = sqlca.sqlcode;
	EXEC SQL select count(*) into :v_status_ll from labreslist
	where contractor = :LabAdminRec->Contractor
	  and idnumber   = :LabAdminRec->MemberId.Number
	  and idcode     = :LabAdminRec->MemberId.Code
	  and reqid_main = :LabAdminRec->ReqIdMain;
//	  v_status_ll = sqlca.sqlcode;
	EXEC SQL delete from labreslist
	where contractor = :LabAdminRec->Contractor
	  and idnumber   = :LabAdminRec->MemberId.Number
	  and idcode     = :LabAdminRec->MemberId.Code
	  and reqid_main = :LabAdminRec->ReqIdMain;
	v_del_ll = sqlca.sqlcode;

	EXEC SQL select count(*) into :v_status_lro from labresother
	where contractor = :LabAdminRec->Contractor
	  and idnumber   = :LabAdminRec->MemberId.Number
	  and idcode     = :LabAdminRec->MemberId.Code
	  and reqid_main = :LabAdminRec->ReqIdMain;
//	  v_status_lro = sqlca.sqlcode;
	EXEC SQL delete from labresother
	where contractor = :LabAdminRec->Contractor
	  and idnumber   = :LabAdminRec->MemberId.Number
	  and idcode     = :LabAdminRec->MemberId.Code
	  and reqid_main = :LabAdminRec->ReqIdMain;
  v_del_lro = sqlca.sqlcode;

	EXEC SQL select count(*) into :v_status_lno from labnotother
	where contractor = :LabAdminRec->Contractor
	  and idnumber   = :LabAdminRec->MemberId.Number
	  and idcode     = :LabAdminRec->MemberId.Code
	  and reqid_main = :LabAdminRec->ReqIdMain;
//	  v_status_lno = sqlca.sqlcode;
	EXEC SQL delete from labnotother
	where contractor = :LabAdminRec->Contractor
	  and idnumber   = :LabAdminRec->MemberId.Number
	  and idcode     = :LabAdminRec->MemberId.Code
	  and reqid_main = :LabAdminRec->ReqIdMain;
	  v_del_lno = sqlca.sqlcode;

GerrLogFnameMini ("3024_log",GerrId,"[%d][%d][%d]-lr[%d]ln[%d]clr[%d]lro[%d]lno[%d]ll[%d]adm[%d]del[%d-%d-%d-%d-%d]",
LabAdminRec->Contractor,LabAdminRec->MemberId.Number,LabAdminRec->ReqIdMain,
v_status_lr,v_status_ln,v_status_clr,v_status_lro,v_status_lno,v_status_ll,InsTime,
v_del_lr,v_del_ln,v_del_lro,v_del_lno,v_del_ll);
	  
/*	EXEC SQL update labresults set contractor = contractor *(-1) 
	where contractor = :LabAdminRec->Contractor
	  and idnumber   = :LabAdminRec->MemberId.Number
	  and idcode     = :LabAdminRec->MemberId.Code
	  and mod(reqid,100000000) = :LabAdminRec->ReqIdMain;
	  v_status_lr = sqlca.sqlcode;
	EXEC SQL update labnotes set contractor = contractor *(-1) 
	where contractor = :LabAdminRec->Contractor
	  and idnumber   = :LabAdminRec->MemberId.Number
	  and idcode     = :LabAdminRec->MemberId.Code
	  and mod(reqid,100000000) = :LabAdminRec->ReqIdMain;
	  v_status_ln = sqlca.sqlcode;
	EXEC SQL update labreslist set contractor = contractor *(-1) 
	where contractor = :LabAdminRec->Contractor
	  and idnumber   = :LabAdminRec->MemberId.Number
	  and idcode     = :LabAdminRec->MemberId.Code
	  and reqid_main = :LabAdminRec->ReqIdMain;
	  v_status_ll = sqlca.sqlcode;
	EXEC SQL update labresother set contractor = contractor *(-1) 
	where contractor = :LabAdminRec->Contractor
	  and idnumber   = :LabAdminRec->MemberId.Number
	  and idcode     = :LabAdminRec->MemberId.Code
	  and reqid_main = :LabAdminRec->ReqIdMain;
	  v_status_lro = sqlca.sqlcode;
	EXEC SQL update labnotother set contractor = contractor *(-1) 
	where contractor = :LabAdminRec->Contractor
	  and idnumber   = :LabAdminRec->MemberId.Number
	  and idcode     = :LabAdminRec->MemberId.Code
	  and reqid_main = :LabAdminRec->ReqIdMain;
	  v_status_lno = sqlca.sqlcode;
*/
	EXEC SQL Insert Into labadmin
	(
		contractor,
		idnumber,
		idcode,
		reqid,
		deletedate,
		status_lr,
		status_ln,
		status_clr,
		status_lro,
		status_lno,
		status_ll,
		del_lr,
		del_ln,
		del_lro,
		del_lno,
		del_ll,		
		insertdate,
		inserttime
	)
		VALUES(	       
		:LabAdminRec->Contractor,
		:LabAdminRec->MemberId.Number,
		:LabAdminRec->MemberId.Code,
		:LabAdminRec->ReqIdMain,
		:LabAdminRec->ReqDate,
		:v_status_lr,
		:v_status_ln,
		:v_status_clr,
		:v_status_lro,
		:v_status_lno,
		:v_status_ll,
		:v_del_lr,
		:v_del_ln,
		:v_del_lro,
		:v_del_lno,
		:v_del_ll,
		:InsDate,
		:InsTime   );
GerrLogFnameMini ("3024_log",GerrId,"lr[%d]ln[%d]lro[%d]lno[%d]ll[%d]adm[%d][%d]",
v_status_lr,v_status_ln,v_status_lro,v_status_lno,v_status_ll,sqlca.sqlcode,InsTime);
		
	if ( SQLERR_error_test())
	{
		GerrLogFnameMini ("3024_log",GerrId,"[%d]insert",sqlca.sqlcode);
	}
	return(1);
}

