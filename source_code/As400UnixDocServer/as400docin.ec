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
#include <MacODBC.h>
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
		int		RetValue	= 1;	// Default: everything's OK.
		int		InsDate;
		int		InsTime;
		int		HospMemb_DischDate;
		int		HospMemb_InsDate;
		short	Member_InHospital;
		short	MemberDied	= 0;

		// DonR 21Jun2021 Bug #169132: Fields to read old values from hospital_member.
		int		o_entry_date;
		int		o_discharge_date;
		short	o_in_hospital;
		int		o_hospital_num;
		char	o_hospital_name		[25 + 1];
		char	o_department_name	[30 + 1];
		int		o_update_date;
		int		o_update_time;
		int		o_insertdate;
		short	SuppressObsoleteUpdate	= 0;
	EXEC SQL END DECLARE SECTION;


	InsDate = GetDate();
	InsTime = GetTime();
	SuppressObsoleteUpdate = 0;	// DonR 22Jun2021: Paranoid re-initialization.

	// DonR 01Aug2011: If we're receiving a hospitalization "refresh" from AS/400, there isn't any doctor
	// to notify (and thus Contractor will be equal to zero). In that case, don't write anything to hospitalmsg.
	if (((Hosp->FlgSystem	!= 0) && (Hosp->FlgSystem != 1))	||	// Something other than a hospitalization record.
		( Hosp->Contractor	!= 0))									// This is a new notification for a doctor to see.
	{
		ExecSQL (	MAIN_DB, AS400UDS_T3008_INS_HospitalMsg,
					&Hosp->Contractor,			&Hosp->TermId,
					&Hosp->MemberId.Number,		&Hosp->MemberId.Code,
					&Hosp->HospDate,			&Hosp->NurseDate,
					&Hosp->ContrName,			&Hosp->HospitalNumb,
					&Hosp->TypeResult,			&Hosp->ResCount,
					&Hosp->ResName1,			&Hosp->ResultSize,
					&Hosp->Result,				&Hosp->HospName,
					&Hosp->DepartName,			&Hosp->FlgSystem,
					&Hosp->PatFlg,				&InsDate,
					&InsTime,					END_OF_ARG_LIST			);

		if (SQLERR_code_cmp (SQLERR_access_conflict) == MAC_TRUE)
		{
			RetValue = 0;	// Trigger retry in calling routine.
		}
		else
		{
			if (SQLERR_error_test ())
			{
				GerrLogMini (GerrId,"Trn. 3008: Error %d inserting to hospitalmsg.", sqlca.sqlcode);
			}
		}
	}	// Need to write to hospitalmsg.


	// DonR 01Aug2011: Write member hospitalization information to hospital_member and update
	// the in_hospital flag in the members table. Note that since each hospitalization record
	// is sent twice from AS/400, once with flg_system = 0 and once with flg_system = 1, we
	// execute this logic only for one of them.
	if (Hosp->FlgSystem == 1)
	{
		if (Hosp->NurseDate <= 20000000)
		{
			// This member is currently a hospital inpatient.
			HospMemb_DischDate	= 0;
			HospMemb_InsDate	= 21991231;	// High value to prevent auto-purging of rows for hospitalized members.
			Member_InHospital	= 1;		// In hospital.
			MemberDied			= 0;		// If member is still an inpatient, s/he has not died.
		}
		else
		{
			// This member has been discharged from hospital.
			HospMemb_DischDate	= Hosp->NurseDate;
			HospMemb_InsDate	= Hosp->NurseDate;	// Rows will be purged based on Discharge Date.

			// DonR 23Aug2011: If this is a non-official release from hospital (i.e. a "vacation"),
			// the Contractor Teudat Zehut will be zero. In this case, use status 3 rather than 2,
			// just to keep things clear. (They're functionally equivalent as far as pharmacies
			// are concerned.)
			Member_InHospital	= (Hosp->Contractor	!= 0) ? 2 : 3;	// Discharged or merely "absent".

			// DonR 25Nov2021 User Story #206812: Add second "doctor TZ" to download, with a value of 3
			// (on discharge) indicating that the member died in hospital.
			MemberDied			= (Hosp->SecondDoctorTZ	== 3) ? 1 : 0;	// Discharged dead or discharged alive.

			// As of 31 July 2011, AS/400 allows entry of bogus discharge dates. If the discharge date
			// is later than today, write out a diagnostic message - but since we don't know what the
			// true discharge date is, write the bogus date to hospital_member as received.
			if (HospMemb_DischDate > InsDate)
			{
				GerrLogMini (GerrId,"Trn. 3008: Got future discharge date of %d for member %d/%d!",
							 HospMemb_DischDate, Hosp->MemberId.Number, Hosp->MemberId.Code);
			}

		}	// This member has been discharged from hospital.


		// DonR 21Jun2021 Bug Fix #169132: In some cases, when a member moves from one hospital to another
		// in the same day (or even if there's short interval between hospitalizations), we get the
		// disharge information from the earlier hospitalization *after* we get the update for the new
		// hospitalization - and in that case, we flag the member as "discharged" even though s/he is
		// in fact still in hospital. In these cases, we need to suppress the update of the late-arriving
		// discharge message and leave the existing status - which is, in fact, more current - intact.
		// First, read the information we stored about the member's current hospitalization status from
		// hospital_member.
		ExecSQL (	MAIN_DB, AS400UDS_T3008_READ_hospital_member,
					&o_entry_date,			&o_discharge_date,		&o_in_hospital,
					&o_hospital_num,		&o_hospital_name,		&o_department_name,
					&o_update_date,			&o_update_time,			&o_insertdate,
					&Hosp->MemberId.Number, &Hosp->MemberId.Code,	END_OF_ARG_LIST		);

		// If the hospital_member read fails, log the error but don't do more than that - this SQL
		// should really never throw errors, as there's almost zero "traffic" for this table.
		if (SQLCODE == 0)
		{
			// The new data should be obsolete if it's for a hospital stay that began *before* the current hospital stay.
			// Note that while in reality obsolete updates seem to happen only for discharges, this logic will also
			// prevent writing old hospitalizations on top of more current information.
			if (Hosp->HospDate < o_entry_date)
			{
				// This update looks like it's no longer relevant, so set flag to suppress members/hospital_member updates.
				SuppressObsoleteUpdate = 1;

				GerrLogMini (	GerrId, "3008 ignoring %d's %d-%d %s - newer hospital stay began %d.",
								Hosp->MemberId.Number, Hosp->HospDate, HospMemb_DischDate,
								(Member_InHospital == 1) ? "admittance" : ((Member_InHospital == 3) ? "absence" : "discharge"),
								o_entry_date);
//
//				GerrLogFnameMini (	"strange_discharges_log", GerrId,
//									"Trn. 3008: Ignoring %s TZ %d's %d-%d hospital stay - newer hospitalization started %d.",
//									(Member_InHospital == 1) ? "admittance to" : ((Member_InHospital == 3) ? "absence from" : "discharge from"),
//									Hosp->MemberId.Number, Hosp->HospDate, HospMemb_DischDate, o_entry_date);
			}	// "New" discharge data appears to be obsolete, as a new hospital stay has already started.
		}
		else
		{
			// "Not found" isn't really an error - or at least it's not worth logging, even though it
			// would be somewhat strange to get a discharge message and not find previous data in
			// hospital_member.
			if (SQLERR_code_cmp (SQLERR_not_found) != MAC_TRUE)
			{
				GerrLogMini (	GerrId, "AS400UDS_T3008_READ_hospital_member for %d returned SQLCODE %d.",
								Hosp->MemberId.Number, SQLCODE	);
				GerrLogFnameMini (	"strange_discharges_log", GerrId,
									"\nAS400UDS_T3008_READ_hospital_member for %d returned SQLCODE %d.",
									Hosp->MemberId.Number, SQLCODE	);
			}
		}	// Error reading hospital_member.



		// DonR 22Jun2021 Bug #169132 (continued): If the new data is a discharge from a previous hospitalization
		// for a member who has already started a new hospitalization, do *not* update the member's status or write
		// the new data to hospital_member - what is already in those tables is more current than the "new" data.
		if (!SuppressObsoleteUpdate)
		{
			// Per Yulia's suggestion, execute a DELETE (with no error-checking) followed by an INSERT,
			// rather than worrying about insert-or-update logic.
			ExecSQL (	MAIN_DB, AS400UDS_T3008_DEL_hospital_member,
						&Hosp->MemberId.Number, &Hosp->MemberId.Code, END_OF_ARG_LIST	);

			// DonR 23Aug2011: Added in_hospital to hospital_member table, just for clarity.
			// Note that Discharge Date may be different from Hosp->NurseDate.
			ExecSQL (	MAIN_DB, AS400UDS_T3008_INS_hospital_member,
						&Hosp->MemberId.Number,		&Hosp->MemberId.Code,
						&Hosp->HospDate,			&HospMemb_DischDate,
						&Member_InHospital,			&Hosp->HospitalNumb,
						&Hosp->HospName,			&Hosp->DepartName,
						&InsDate,					&InsTime,
						&HospMemb_InsDate,			&MemberDied,
						END_OF_ARG_LIST										);

			// Since hospital_member is not used by any online programs, don't bother checking for
			// access-conflict errors. Also, don't abort - we still want to flag the member, since
			// that's the important part of this whole business!
			if (SQLERR_error_test ())
			{
				GerrLogMini (GerrId,"Trn. 3008: Error %d inserting to hospital_member.", sqlca.sqlcode);
			}

			// Update members table. Note that no-rows-affected is OK here. If somehow a member joins Maccabi
			// from his/her hospital bed and we receive the hospitalization record before we get the member
			// update, the later INSERT to members will check for the existence of a hospital_member row and
			// set the in_hospital flag accordingly.
			ExecSQL (	MAIN_DB, AS400UDS_T3008_UPD_members,
						&Member_InHospital,			&MemberDied,
						&Hosp->MemberId.Number,		&Hosp->MemberId.Code,
						END_OF_ARG_LIST										);

			if (SQLERR_code_cmp (SQLERR_access_conflict) == MAC_TRUE)
			{
				RetValue = 0;	// Trigger retry in calling routine.
			}
			else
			{
				if (SQLERR_code_cmp (SQLERR_no_rows_affected) != MAC_TRUE)	// Ignore no-rows-affected.
				{
					if (SQLERR_error_test ())
					{
						GerrLogMini (GerrId,"Trn. 3008: Error %d updating member %d/%d.",
									 sqlca.sqlcode, Hosp->MemberId.Number, Hosp->MemberId.Code);
					}
				}
			}	// No access conflict.
		}	// DonR 22Jun2021 Bug Fix #169132 end - SuppressObsoleteUpdate is set FALSE.

	}	// Record is a hospitalization notification with flg_system = 1.


	return (RetValue);
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
		// DonR 18Oct2020: Duplicate Key on insert is perfectly normal and should *not*
		// be logged!
		if (SQLERR_code_cmp (SQLERR_not_unique) == MAC_FALSE)
		{
			GerrLogMini (GerrId,"3012[%d] insert[%d]id[%d]kod[%d]",sqlca.sqlcode,
				Rasham->MemberId.Number,
				Rasham->KodRasham);
		}

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
		// DonR 18Oct2020: Reduce log size by not logging Duplicate Key errors.
		if (SQLERR_code_cmp (SQLERR_not_unique) == MAC_FALSE)
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
//	GerrLogFnameMini ("3013_log",GerrId,"3013before delete[%d]",
//	Message->MemberId.Number);
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
// Yulia 20140729. Different way for 5580 message and other tipuly bait
// change unique key for memberremark table , but process like unique  + 5580
// Yulia 20141019. Different way for 8000 message and other tipuly bait
// change unique key for memberremark table , but process like unique  + 8000

	if (remark == 5580 ) 
	{
		EXEC SQL delete from memberremarks where 
			idnumber	= 	:MemberId->Number	and
			idcode		=	:MemberId->Code and 
	 		remarkcode	=	:v_remark       ;
		if( SQLERR_code_cmp( SQLERR_not_found ) == MAC_TRUE )
		{
		}
		else if(  SQLERR_error_test() ) 
		{
			GerrLogFnameMini ("3017_log",GerrId,"3017[%d]delete 5580 problem[%d]",
				sqlca.sqlcode,MemberId->Number);
		    return 0;
		}
	}
	else if (remark == 8000 ) 
	{
		EXEC SQL delete from memberremarks where 
			idnumber	= 	:MemberId->Number	and
			idcode		=	:MemberId->Code and 
	 		remarkcode	=	:v_remark       ;
		if( SQLERR_code_cmp( SQLERR_not_found ) == MAC_TRUE )
		{
		}
		else if(  SQLERR_error_test() ) 
		{
			GerrLogFnameMini ("3017_log",GerrId,"3017[%d]delete 8000 problem[%d]",
				sqlca.sqlcode,MemberId->Number);
		    return 0;
		}
	}
	
	else
	{ //old remark from 20061016 for felete all idnumber records
			EXEC SQL delete from memberremarks where 
			idnumber	= 	:MemberId->Number	and
			idcode		=	:MemberId->Code and 
	 		remarkcode	between 5550 and 5579       ;
		if( SQLERR_code_cmp( SQLERR_not_found ) == MAC_TRUE )
		{
		}
		else if(  SQLERR_error_test() ) 
		{
			GerrLogFnameMini ("3017_log",GerrId,"3017[%d]delete [%d] problem[%d]",
				sqlca.sqlcode,MemberId->Number,remark);
		    return 0;
		}
	
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
		UpdatedBy			=	:Contract->UpdatedBy,
		Profession2			=	:Contract->prof2,
		Profession3			=	:Contract->prof3,
		Profession4			=	:Contract->prof4
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
		UpdatedBy,
		profession2,	profession3,	profession4
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
		:Contract->LastUpdate.DbDate,:Contract->LastUpdate.DbTime,
		:Contract->UpdatedBy,		:Contract->prof2,
		:Contract->prof3,			:Contract->prof4
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


int DbUpdateContractNewRecord( 	TIdNumber	Contractor,
				TContractType	ContractType,
				TContractRecord	*Ptr)
{
EXEC SQL BEGIN DECLARE SECTION;
	TContractRecord *Contract	=	Ptr;
EXEC SQL END DECLARE SECTION;
		Contract->Contractor	=	Contractor;
		Contract->ContractType	=	ContractType;

	EXEC SQL Update contract /*20120228*/
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
		UpdatedBy			=	:Contract->UpdatedBy,
		Profession2			=	:Contract->prof2,
		Profession3			=	:Contract->prof3,
		Profession4			=	:Contract->prof4,
		taxcode 			=	:Contract->TaxValue
	where	
		Contractor	=	:Contract->Contractor		and
		ContractType	=	:Contract->ContractType;
	if (sqlca.sqlerrd[2] == 0)
	{
	//have no records to update
//		GerrLogFnameMini ("3031_log",GerrId,"sqlcode[%d]sqlca.sqlerrd[2][%d]",sqlca.sqlcode,sqlca.sqlerrd[2]);	
		return (3);
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
int DbInsertContractNewRecord( TContractRecord	*Ptr)
{
	EXEC SQL BEGIN DECLARE SECTION;
	TContractRecord *Contract	=	Ptr;
EXEC SQL END DECLARE SECTION;

	EXEC SQL insert into	contract /*20120228*/
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
		UpdatedBy,
		profession2,	profession3,	profession4,taxcode
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
		:Contract->LastUpdate.DbDate,:Contract->LastUpdate.DbTime,
		:Contract->UpdatedBy,		:Contract->prof2,
		:Contract->prof3,			:Contract->prof4,
		:Contract->TaxValue
	);
	//Yulia 20130121
	if (SQLERR_code_cmp (SQLERR_not_unique) == MAC_TRUE)
	{
		return (2);
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

int DbDeleteContractNewRecord( 	TIdNumber	Contractor,
				TContractType	ContractType)
{
EXEC SQL BEGIN DECLARE SECTION;
	TContractRecord Contract;
EXEC SQL END DECLARE SECTION;

		Contract.Contractor	=	Contractor;
		Contract.ContractType	=	ContractType;

	EXEC SQL delete from contract /*20120228*/
	where	
		Contractor	=	:Contract.Contractor		and
		ContractType	=	:Contract.ContractType;

	return(1);
}
/*begin ContractNewNew 20130915 Yulia*/
int DbUpdateContractNewNewRecord( 	TIdNumber	Contractor,
				TContractType	ContractType, TTermId TermId,
				TContractNewRecord	*Ptr)
{
EXEC SQL BEGIN DECLARE SECTION;
	TContractNewRecord *Contract	=	Ptr;
EXEC SQL END DECLARE SECTION;
		Contract->Contractor	=	Contractor;
		Contract->ContractType	=	ContractType;
		Contract->TermId 		= 	TermId;

	EXEC SQL Update contractnew
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
		UpdatedBy			=	:Contract->UpdatedBy,
		Profession2			=	:Contract->prof2,
		Profession3			=	:Contract->prof3,
		Profession4			=	:Contract->prof4,
		taxcode 			=	:Contract->TaxValue
	where	
		Contractor		=	:Contract->Contractor		and
		ContractType	=	:Contract->ContractType and
		TermId 			= 	:Contract->TermId;

	if ( SQLERR_error_test() )
	{
	return (0);
	}
	else
	{
	return(1);
	}
}
int DbInsertContractNewNewRecord( TContractNewRecord	*Ptr)
{
	EXEC SQL BEGIN DECLARE SECTION;
	TContractNewRecord *Contract	=	Ptr;
EXEC SQL END DECLARE SECTION;

	EXEC SQL insert into	contractnew
	(
		Contractor, 		ContractType, 	TermId,
		FirstName, 		LastName,
		Street, 		City,
		Mahoz, 			Phone,
		Profession, 		AcceptUrgent,
		SexesAllowed, 		AcceptAnyAge,
		AllowKbdEntry, 		NoShifts,
		IgnorePrevAuth, 	AuthorizeAlways,
		NoAuthRecord,
		LastUpdateDate, 	LastUpdateTime,
		UpdatedBy,
		profession2,	profession3,	profession4,taxcode
	)	
	values
	(
		:Contract->Contractor, 		:Contract->ContractType,
		:Contract->TermId,
		:Contract->FirstName, 		:Contract->LastName,
		:Contract->Street, 		:Contract->City,
		:Contract->Mahoz, 		:Contract->Phone,
		:Contract->Profession, 		:Contract->AcceptUrgent,
		:Contract->SexesAllowed, 	:Contract->AcceptAnyAge,
		:Contract->AllowKbdEntry, 	:Contract->NoShifts,
		:Contract->IgnorePrevAuth, 	:Contract->AuthorizeAlways,
		:Contract->NoAuthRecord,
		:Contract->LastUpdate.DbDate,:Contract->LastUpdate.DbTime,
		:Contract->UpdatedBy,		:Contract->prof2,
		:Contract->prof3,			:Contract->prof4,
		:Contract->TaxValue
	);
	//Yulia 20130121
	if (SQLERR_code_cmp (SQLERR_not_unique) == MAC_TRUE)
	{
		return (2);
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

int DbDeleteContractNewNewRecord( 	TIdNumber	Contractor,
				TContractType	ContractType, TTermId TermId)
{
EXEC SQL BEGIN DECLARE SECTION;
	TContractNewRecord Contract;
EXEC SQL END DECLARE SECTION;

		Contract.Contractor	=	Contractor;
		Contract.ContractType	=	ContractType;
		Contract.TermId = TermId;
	EXEC SQL delete from contractnew
	where	
		Contractor		=	:Contract.Contractor		and
		ContractType	=	:Contract.ContractType and
		TermId 			= 	:Contract.TermId;
	return(1);
}  
/*end  20130915*/

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
		Snif		=  :Conterm->Snif,
		ServiceType	=  :Conterm->ServiceType,
		City 		=  :Conterm->City,
		Add1 		=  :Conterm->Add1,
		Add2 		=  :Conterm->Add2,
		interface 	=  :Conterm->interface,
		owner 		=  :Conterm->owner, 
		group8		=  :Conterm->group8,
		group_c	   	=  :Conterm->group_c, 	
		group_join	=  :Conterm->group_join,
		group_act	=  :Conterm->group_act,
		tax	  		=  :Conterm->tax ,
		assign	 	=  :Conterm->assign,
		gnrl_days	=  :Conterm->gnrl_days ,
		act 	  	=  :Conterm->act ,
		from_months	=  :Conterm->from_months,
		to_months	=  :Conterm->to_months,
		flg_cont	=  :Conterm->flg_cont 
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
		ServiceType,	City,	Add1,	Add2,
		interface,		owner,		group8,		group_c,
		group_join,		group_act,	tax,		assign,
		gnrl_days,		act,		from_months,		to_months,		flg_cont
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
		:Conterm->City,	:Conterm->Add1, :Conterm->Add2,
		:Conterm->interface,
		:Conterm->owner, 
		:Conterm->group8,
	    :Conterm->group_c, 	
	    :Conterm->group_join,
	    :Conterm->group_act,
	    :Conterm->tax ,
	    :Conterm->assign,
	    :Conterm->gnrl_days ,
 	    :Conterm->act ,
	    :Conterm->from_months,
	    :Conterm->to_months,
	    :Conterm->flg_cont 
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
/*20130108*/

int DbUpdateContermNewRecord( 	TTermId		TermId,
				TIdNumber	Contractor,
				TContractType	ContractType,
				TContermRecord	*Ptr)
{
EXEC SQL BEGIN DECLARE SECTION;
	TContermRecord *Conterm	=	Ptr;
EXEC SQL END DECLARE SECTION;
	EXEC SQL update conterm /*20120228*/
	set
		AuthType	=	:Conterm->AuthType,
		ValidFrom	=	:Conterm->ValidFrom,
		ValidUntil	=	:Conterm->ValidUntil,

		Location	=	:Conterm->Location,
		Locationprv	=	:Conterm->Locationprv,
		LastUpdateDate	=	:Conterm->LastUpdate.DbDate,
		LastUpdateTime	=	:Conterm->LastUpdate.DbTime,
		UpdatedBy	=	:Conterm->UpdatedBy,
		Position	=	:Conterm->Position,
		Paymenttype	=	:Conterm->Paymenttype,
		Mahoz		=	:Conterm->Mahoz,
		Snif		=  :Conterm->Snif,
		ServiceType	=  :Conterm->ServiceType,
		City 		=  :Conterm->City
			where	
		TermId		=	:Conterm->TermId		and
		Contractor	=	:Conterm->Contractor	and
		ContractType	=	:Conterm->ContractType;
	if (sqlca.sqlerrd[2] == 0)
	{
	//have no records to update
		GerrLogFnameMini ("3032_log",GerrId,"sqlcode[%d]sqlca.sqlerrd[2][%d]",sqlca.sqlcode,sqlca.sqlerrd[2]);	
		return (3);
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
int DbInsertContermNewRecord( TContermRecord	*Ptr)
{
EXEC SQL BEGIN DECLARE SECTION;
	TContermRecord *Conterm	=	Ptr;
EXEC SQL END DECLARE SECTION;

	EXEC SQL insert into	conterm /*20120228*/
	(
		TermId,		Contractor,	ContractType,
		AuthType,	ValidFrom,	ValidUntil,
		Location,	Locationprv,
		LastUpdateDate,	LastUpdateTime,
		UpdatedBy,	Position,
		Paymenttype,	Mahoz,		Snif,
		ServiceType,	City		)	
	values
	( 
		:Conterm->TermId,
		:Conterm->Contractor,		:Conterm->ContractType,
		:Conterm->AuthType,
		:Conterm->ValidFrom, 		:Conterm->ValidUntil,
		:Conterm->Location, 		:Conterm->Locationprv,
		
		:Conterm->LastUpdate.DbDate,	:Conterm->LastUpdate.DbTime,
		:Conterm->UpdatedBy,		:Conterm->Position,
		:Conterm->Paymenttype, 		:Conterm->Mahoz,
		:Conterm->Snif,			:Conterm->ServiceType,
		:Conterm->City	);
	if (SQLERR_code_cmp (SQLERR_not_unique) == MAC_TRUE)
	{
	GerrLogFnameMini ("3032_log",GerrId,"record exist must update[%d]",sqlca.sqlcode );
		return (2);
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

int DbDeleteContermNewRecord( 	TIdNumber	Contractor,
								TContractType	ContractType,
								TTermId		TermId)
{
EXEC SQL BEGIN DECLARE SECTION;
	TContermRecord Conterm;
EXEC SQL END DECLARE SECTION;
//GerrLogMini (GerrId," conterm delete1: [%9d-%2d-%7d]",	Contractor,ContractType,TermId	);
	Conterm.TermId 	=	TermId;
	Conterm.Contractor =	Contractor;
	Conterm.ContractType	=	ContractType;
//GerrLogMini (GerrId," conterm delete2:[%9d-%2d-%7d]",
	//			Conterm.Contractor,Conterm.ContractType,Conterm.TermId	);
	EXEC SQL delete from  conterm /*20120228*/
	where	
		TermId		=	:Conterm.TermId		and
		Contractor	=	:Conterm.Contractor	and
		ContractType	=	:Conterm.ContractType;
	if ( SQLERR_error_test() )
	{
		GerrLogMini (GerrId," conterm delete3:[%d] [%9d-%2d-%7d]",sqlca.sqlcode,
			Conterm.Contractor,Conterm.ContractType,Conterm.TermId	);
		return (0);
	}
	else
	{
		return(1);
	}
}
/*end 20130108*/
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
/*20130127*/
int	DbInsertTerminalNewRecord( TTerminalRecord *Ptr)
{
EXEC SQL BEGIN DECLARE SECTION;
	TTerminalRecord *Terminal	=	Ptr;
EXEC SQL END DECLARE SECTION;

	EXEC SQL insert into	terminal    /*20120228*/
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
	if (SQLERR_code_cmp (SQLERR_not_unique) == MAC_TRUE)
	{
		return (2);
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
int DbUpdateTerminalNewRecord( 	TTerminalRecord	*Ptr)
{
	EXEC SQL BEGIN DECLARE SECTION;
		TTerminalRecord *Terminal	=	Ptr;
	EXEC SQL END DECLARE SECTION;
	EXEC SQL update terminal /*20120228*/
	set	Descr 			= :Terminal->Descr,		
		Street 			= :Terminal->Street,		
		City			= :Terminal->City,
		LocPhone 		= :Terminal->LocPhone,
		lastupdatedate 	= :Terminal->LastUpdate.DbDate,	
		lastupdatetime 	= :Terminal->LastUpdate.DbTime,
		updatedby = :Terminal->UpdatedBy
		where termid = :Terminal->TermId;
	if ( SQLERR_error_test() )
	{
		return (0);
	}
	else
	{
		return(1);
	}
}

int DbDeleteTerminalNewRecord( 	TTerminalRecord	*Ptr)
{
	EXEC SQL BEGIN DECLARE SECTION;
		TTerminalRecord *Terminal	=	Ptr;
	EXEC SQL END DECLARE SECTION;
	EXEC SQL delete from terminal     /*20120228*/
		where termid = :Terminal->TermId;
	if ( SQLERR_error_test() )
	{
		return (0);
	}
	else
	{
		return(1);
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

/*20130128*/
int	DbInsertSubstNewRecord( TSubstRecord	*Ptr)
{
EXEC SQL BEGIN DECLARE SECTION;
	TSubstRecord	 *Subst =	Ptr;
EXEC SQL END DECLARE SECTION;

	EXEC SQL insert into	subst /*20120228*/
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
	if (SQLERR_code_cmp (SQLERR_not_unique) == MAC_TRUE)
	{
		return (2);
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

int	DbUpdateSubstNewRecord(	TSubstRecord	*Ptr)
{
EXEC SQL BEGIN DECLARE SECTION;
	TSubstRecord	 *Subst =	Ptr;
EXEC SQL END DECLARE SECTION;

	EXEC SQL update subst	set      /*20120228*/
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
	if (sqlca.sqlerrd[2] == 0)
	{
	//have no records to update
		GerrLogFnameMini ("3034_log",GerrId,"sqlcode[%d][%d]",sqlca.sqlcode,sqlca.sqlerrd[2]);	
		return (3);
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

int	DbDeleteSubstNewRecord(	TSubstRecord	*Ptr)
{
EXEC SQL BEGIN DECLARE SECTION;
	TSubstRecord	 *Subst =	Ptr;
EXEC SQL END DECLARE SECTION;
/*GerrLogMini (GerrId," SubstType1 delete [%9d-%9d-%2[%d]]",
		Subst->Contractor, Subst->AltContractor,Subst->ContractType,
sqlca.sqlcode		);*/
	EXEC SQL delete from subst /*20120228*/
	where	
		Contractor	=	:Subst->Contractor	and
		ContractType	=	:Subst->ContractType	and
		AltContractor	=	:Subst->AltContractor;
GerrLogMini (GerrId," SubstType2 delete [%9d-%9d-%2d[%d]]",
		Subst->Contractor, Subst->AltContractor,Subst->ContractType,
sqlca.sqlcode		);


//20130131

	if ( SQLERR_error_test() )
	{
	return (0);
	}
	else
	{
	return(1);
	}
}



/* end 20130128*/


int	DbInsertSubstAbsRecord( TSubstRecord	*Ptr)
{
EXEC SQL BEGIN DECLARE SECTION;
	TSubstRecord	 *Subst =	Ptr;
EXEC SQL END DECLARE SECTION;
	EXEC SQL delete from subst 
	where 	contractor = :Subst->Contractor	and
			contracttype = :Subst->ContractType and 
			altcontractor = 	:Subst->AltContractor;
	EXEC SQL insert into	subst
	(   Contractor, 	ContractType, 		AltContractor, 
		SubstType, 	DateFrom, 		DateUntil, 
		SubstReason, 
		LastUpdateDate, LastUpdateTime, 	UpdatedBy	)	
	values
	(	:Subst->Contractor, 		:Subst->ContractType, 	
		:Subst->AltContractor, 		:Subst->SubstType, 	
		:Subst->DateFrom, 			:Subst->DateUntil,
		:Subst->SubstReason, 		:Subst->LastUpdate.DbDate, 
		:Subst->LastUpdate.DbTime, 	:Subst->UpdatedBy	);
/*GerrLogMini (GerrId," SubstType(4)insert [%9d-%9d-%8d-%8d-%1d[%d]]",
		Subst->Contractor, Subst->AltContractor,
		Subst->DateFrom,   Subst->DateUntil,
		Subst->SubstType,sqlca.sqlcode		);*/

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

int	DbUpdateCityDistRecord( TCityDistance	*Ptr)
{
	EXEC SQL BEGIN DECLARE SECTION;
	TCityDistance *CityDistRec	=	Ptr;
	int InsDate,InsTime;
	int v_status = 0;
	EXEC SQL END DECLARE SECTION;

	InsDate = GetDate();
	InsTime = GetTime();
	if ( CityDistRec->distance > 29) v_status = 1;	// DonR 13Sep2012: Per Yulia, changed from 49 to 29.
	if ( CityDistRec->status == 1 ) //update = delete + insert
	{
		EXEC SQL delete from cities 
		where city0 = :CityDistRec->city0 and
		city1 = :CityDistRec->city1;
		if ( SQLERR_error_test())
		{
			GerrLogFnameMini ("3025_log",GerrId,"[%d]delete",sqlca.sqlcode);
		}
	}
		EXEC SQL insert into  cities 
		values
		(:CityDistRec->city0,
		:CityDistRec->city1,
		:CityDistRec->distance,
		:InsDate,
		:InsTime,
		:v_status);
		if ( SQLERR_error_test())
		{
			GerrLogFnameMini ("3025_log",GerrId,"[%d]insert",sqlca.sqlcode);
		}
}
int	DbUpdateMembDietCntRecord( TMembDietCnt	*Ptr)
{
	EXEC SQL BEGIN DECLARE SECTION;
	TMembDietCnt *MembDietCnt	=	Ptr;
	int InsDate,InsTime;
	int v_status = 0;
	EXEC SQL END DECLARE SECTION;

	InsDate = GetDate();
	InsTime = GetTime();
	if (InsDate <= MembDietCnt->ValidUntil  && InsDate >=MembDietCnt->ValidFrom)
	{
		EXEC SQL delete from memberdietcntas 
		where idnumber  = :MembDietCnt->MemberId.Number and
		idcode = :MembDietCnt->MemberId.Code;
		if ( SQLERR_error_test())
		{
			GerrLogFnameMini ("3027_log",GerrId,"[%d]delete",sqlca.sqlcode);
		}
		EXEC SQL insert into  memberdietcntas 
		(idnumber,idcode,validfrom,validuntil,dietcnt,insertdate,inserttime)
		values( 
		:MembDietCnt->MemberId.Number,
		:MembDietCnt->MemberId.Code,
		:MembDietCnt->ValidFrom,
		:MembDietCnt->ValidUntil,
		:MembDietCnt->DietCnt,
		:InsDate,
		:InsTime);
		if ( SQLERR_error_test())
		{
			GerrLogFnameMini ("3027_log",GerrId,"[%d]insert[%9d][%8d][%8d][%5d]",sqlca.sqlcode,
			MembDietCnt->MemberId.Number,
			MembDietCnt->ValidFrom,
			MembDietCnt->ValidUntil,
			MembDietCnt->DietCnt);
		}
	}
	else
	{
		GerrLogFnameMini ("3027_old_log",GerrId,"[%9d][[%8d][%8d]",
		MembDietCnt->MemberId.Number,
		MembDietCnt->ValidFrom,
		MembDietCnt->ValidUntil);
	}
	return 1;
}
int	DbInsertPsicStatRecord( TPsicStat	*Ptr)
{
	EXEC SQL BEGIN DECLARE SECTION;
	TPsicStat *PsicStat	=	Ptr;
	int InsDate,InsTime;
	int v_status = 0;
	EXEC SQL END DECLARE SECTION;

		InsDate = GetDate();
		InsTime = GetTime();
		EXEC SQL delete from contractmsg
		where 
		idnumber  	= :PsicStat->MemberId.Number 	and
		idcode 		= :PsicStat->MemberId.Code 		and 
		contractor	= :PsicStat->Contractor			and 
		type 		=	10;
		if ( SQLERR_error_test())
		{
			GerrLogFnameMini ("3026_log",GerrId,"[%d]delete",sqlca.sqlcode);
		}
		EXEC SQL insert into  contractmsg 
		(contractor,idnumber,idcode,type,readsign,insertdate,inserttime)
		values( 
		:PsicStat->Contractor,
		:PsicStat->MemberId.Number,
		:PsicStat->MemberId.Code,
		10,
		:PsicStat->PsicStatus,
		:InsDate,
		:InsTime);
		if ( SQLERR_error_test())
		{
			GerrLogFnameMini ("3026_log",GerrId,"[%d]insert[%9d][%9d][%1d]",sqlca.sqlcode,
			PsicStat->Contractor,
			PsicStat->MemberId.Number,
			PsicStat->PsicStatus);
		}
	
}
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
		UpdatedBy,profession2,profession3,profession4
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
		:Contract->UpdatedBy,			:Contract->prof2,
		:Contract->prof3,				:Contract->prof4
	from	contract
	where	
		Contractor	=	:Contract->Contractor		and
		ContractType	=	:Contract->ContractType;
	if( SQLERR_code_cmp( SQLERR_not_found ) == MAC_TRUE )
	{
	    return (0);
	}
	if ( SQLERR_error_test() )
	{
		return (0);
	}
	return(1);
}
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
		DrugDateO,		DrugTimeO,City,
		interface,
		owner,
		group8,
		group_c,
		group_join,
		group_act,
		tax,
		assign,
		gnrl_days,
		act,
		from_months,
		to_months,
		flg_cont
	into
		:Conterm->AuthType, 		:Conterm->ValidFrom, 	
		:Conterm->ValidUntil, 		:Conterm->Location, 
		:Conterm->Locationprv, 		:Conterm->HasPharmacy,
		:Conterm->LastUpdate.DbDate,:Conterm->LastUpdate.DbTime,
		:Conterm->UpdatedBy, 		:Conterm->Position,
		:Conterm->Paymenttype, 		:Conterm->Mahoz, 
		:Conterm->Snif,
		:Conterm->MsgID,			:Conterm->ServiceType,
		:Conterm->StatusUrgent,
		:Conterm->DrugDateI,		:Conterm->DrugTimeI,
		:Conterm->DrugDateO,		:Conterm->DrugTimeO,
		:Conterm->City,	  			:Conterm->interface,
		:Conterm->owner, 
		:Conterm->group8
	    :Conterm->group_c, 	
	    :Conterm->group_join,
	    :Conterm->group_act,
	    :Conterm->tax ,
	    :Conterm->assign,
	    :Conterm->gnrl_days ,
 	    :Conterm->act ,
	    :Conterm->from_months,
	    :Conterm->to_months,
	    :Conterm->flg_cont 
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
