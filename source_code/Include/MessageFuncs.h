// MessageFuncs.h

#define	LETTER_IN_HAND		0
#define	NON_MAC_SPECIALIST	1
#define	MAC_SPECIALIST_NOTE	2
#define EXPIRED_AS400_ISHUR	3
#define	TREATMENT_COMMITTEE	4

#define MAX_DAYS_EXPIRED	31

#define PARTI_CODE_ONE			 1
#define PARTI_CODE_TWO			 2
#define PARTI_CODE_THREE		 3
#define PARTI_CODE_FIVE			 5
#define PARTI_CODE_MACCABICARE	17
#define PARTI_MACCABICARE_PVT	97

#define GET_DRUGS_IN_BLOOD		0
#define GET_ALL_DRUGS_BOUGHT	1

#ifdef MAIN
		TDrugsBuf		v_DrugsBuf;						// DonR 16Feb2006 - made drugs-in-blood global.
#endif

// DonR 23Feb2008: test_dentist_drugs_electronic(), and the new function
// test_home_visit_drugs_electronic(), are defined as macros. The underlying
// "real" function is test_special_drugs_electronic(), which takes as a
// parameter the starting point for the range of profession codes to be
// checked for specialist participation. 
//
// DonR 27Feb2008: The same is now true for the non-electronic versions!
#define test_dentist_drugs_electronic(prescdrug,errorcode)	\
		test_special_drugs_electronic (prescdrug, 58000, errorcode)

#define test_home_visit_drugs_electronic(prescdrug,errorcode)	\
		test_special_drugs_electronic (prescdrug,  4000, errorcode)

#define test_dentist_drugs(prescdrugs,drugcnt,presParti,errorcode)	\
		test_special_drugs (prescdrugs, drugcnt, presParti, 58000, errorcode)

#define test_home_visit_drugs(prescdrugs,drugcnt,presParti,errorcode)	\
		test_special_drugs (prescdrugs, drugcnt, presParti,  4000, errorcode)


void AsciiToBin (
		 char	*buff,
		 int	len,
		 int	*p_val
		 );

void WriteErrAndexit(
		     int       errcode ,
		     int       msg_index
		     );

int	get_message_header(
    char	*input_buf,
    int		*pMsgID,
    int		*pMsgEC
    );

short int verify_numeric( char *ptr, int ascii_len );

char GetChar (char **p_buffer);

short	GetShort	(char **p_buffer, int ascii_len);
int		GetInt		(char **p_buffer, int ascii_len);
long	GetLong		(char **p_buffer, int ascii_len);

void GetString (
		char		** p_buffer ,
		char		*str,
		int		ascii_len
		);

int PrintErr(
	      char		*output_buf,
	      int		errcode,
	      int		msgcode
	      );

void ChckInputLen (int diff     /* = [total-input] - [processed input] */);

void ChckInputLenAllowTrailingSpaces (char *buff_in, int diff     /* = [total-input] - [processed input] */);

//void AddHeader(
//	       int 			msgIdx ,
//	       int 			error_code
//	       );
//
void ChckDate (
	       int		date	/*  format:  YYYYMMDD  */
	       );

void ChckTime (
	       int		time	/*  format:  HHNNSS  */
	       );

int	GetMonsDif( int base_date,    int sub_date );

int    GetDaysDiff( int base_day,    int sub_day );

int    AddDays( int base_day,    int add_day );

int		AddMonths (int base_date, int add_months);

int    AddSeconds( int base_time,    int add_sec );

int	GetTblUpFlg( TDate	PharmecyDate,
		     char	*TableName );


// DonR 11May2010: New version of SetErrorVar that (optionally) works with an error array.
#define SET_ERR_INIT		0
#define SET_ERR_MODE_NEW	1
#define SET_ERR_SET_OLD		2
#define SET_ERR_SET_NEW		3
#define SET_ERR_DELETE		4
#define SET_ERR_TELL		5
#define SET_ERR_TELL_MAX	6
#define SET_ERR_SWAP		7
#define SET_ERR_FINDTOP		8

// DonR 01Oct2017: Macros for late-arrival-mode for update_doctor_presc ().
#define NORMAL_MODE			0
#define LATE_ARRIVAL_MODE	1


typedef struct
{
	int		LargoCode;
	int		Error;
	short	Severity;
	short	ReportToPharm;
	short	LineNo;
}	TErrorArray;

// DonR 19May2013: Added "BumpFlag".
int	SetErrorVar_x (int			ActionCode_in,
				   short		*ErrorCode_Var,
				   short		Old_ErrorCode,
				   short		New_ErrorCode,
				   int			LargoCode_in,
				   short		LineNo_in,
				   short		BumpFlag_in,
				   TErrorArray	**ArrayAddr_out,
				   short		*FatalFlag_out,
				   int			*OverflowFlag_out);

#define	SET_ERR_MAX_ERRORS	999

//					   ActionCode_in		ErrVar	OldErr	NewErr	Largo	LineNo	BumpFlag	ArrayAddr		FatalFlag	Overflow
#define SetErrorVarInit()																											\
		SetErrorVar_x (SET_ERR_INIT,		NULL,	0,		0,		0,		0,		0,			NULL,			NULL,		NULL)

#define SetErrorVarEnableArr()																										\
		SetErrorVar_x (SET_ERR_MODE_NEW,	NULL,	0,		0,		0,		0,		0,			NULL,			NULL,		NULL)

#define SetErrorVar(VarPtr,Err)																										\
		SetErrorVar_x (SET_ERR_SET_OLD,		VarPtr,	0,		Err,	0,		0,		0,			NULL,			NULL,		NULL)

#define SetErrorVarBump(VarPtr,Err)																									\
		SetErrorVar_x (SET_ERR_SET_OLD,		VarPtr,	0,		Err,	0,		0,		1,			NULL,			NULL,		NULL)

#define SetErrorVarF(VarPtr,Err,FatalFlag)																							\
		SetErrorVar_x (SET_ERR_SET_OLD,		VarPtr,	0,		Err,	0,		0,		0,			NULL,			FatalFlag,	NULL)

#define SetErrorVarBumpF(VarPtr,Err,FatalFlag)																						\
		SetErrorVar_x (SET_ERR_SET_OLD,		VarPtr,	0,		Err,	0,		0,		1,			NULL,			FatalFlag,	NULL)

#define SetErrorVarArr(VarPtr,Err,Largo,LineNo,FatalFlag,OverflowFlag)																		\
		SetErrorVar_x (SET_ERR_SET_NEW,		VarPtr,	0,		Err,	Largo,	LineNo,	0,			NULL,			FatalFlag,	OverflowFlag)

#define SetErrorVarArrAsIf(VarPtr,Err,AsIfErr,Largo,LineNo,FatalFlag,OverflowFlag)															\
		SetErrorVar_x (SET_ERR_SET_NEW,		VarPtr,	AsIfErr,Err,	Largo,	LineNo,	0,			NULL,			FatalFlag,	OverflowFlag)

#define SetErrorVarArrSpecSeverity(VarPtr,Err,Severity,Largo,LineNo,FatalFlag,OverflowFlag)															\
		SetErrorVar_x (SET_ERR_SET_NEW,		VarPtr,	(0 - Severity), Err,	Largo,	LineNo,	0,			NULL,			FatalFlag,	OverflowFlag)

#define SetErrorVarArrBump(VarPtr,Err,Largo,LineNo,FatalFlag,OverflowFlag)																	\
		SetErrorVar_x (SET_ERR_SET_NEW,		VarPtr,	0,		Err,	Largo,	LineNo,	1,			NULL,			FatalFlag,	OverflowFlag)

#define SetErrorVarDelete(Err,Largo)																								\
		SetErrorVar_x (SET_ERR_DELETE,		NULL,	Err,	0,		Largo,	0,		0,			NULL,			NULL,		NULL)

#define SetErrorVarTell(ArrayAddrPtr,NumRepErrs)																					\
		SetErrorVar_x (SET_ERR_TELL,		NULL,	0,		0,		0,		0,		0,			ArrayAddrPtr,	NULL,		NumRepErrs)

#define SetErrorVarTellMax()																										\
		SetErrorVar_x (SET_ERR_TELL_MAX,	NULL,	0,		0,		0,		0,		0,			NULL,			NULL,		NULL)

#define SetErrorVarSwap(VarPtr,OldErr,NewErr,Largo,FatalFlag)																		\
		SetErrorVar_x (SET_ERR_SWAP,		VarPtr,	OldErr,	NewErr,	Largo,	0,		0,			NULL,			FatalFlag,	NULL)

// Existence check works the same as Swap, but it "swaps" in the same error code, so the only result is
// to send back 1 if the error code was found.
#define SetErrorVarExists(Err,Largo)																								\
		SetErrorVar_x (SET_ERR_SWAP,		NULL,	Err,	Err,	Largo,	0,		0,			NULL,			NULL,		NULL)

// Set error-pointer variable to the most severe error found for a particular drug.
// This is useful after calling SetErrorVarDelete.
#define SetErrorVarTop(VarPtr,Largo,FatalFlag)																						\
		SetErrorVar_x (SET_ERR_FINDTOP,		VarPtr,	0,		0,		Largo,	0,		0,			NULL,			FatalFlag,	NULL)

// DonR 11May2010 end.


int	TransactionBegin( void );

#define IS_PHARMACY_OPEN(a,b,c)		IS_PHARMACY_OPEN_X (a, b, c, ssmd_data_ptr->msg_idx, ssmd_data_ptr->spool_flg)
// #define IS_HESDER_PHARMACY_OPEN(a,b,c)	IS_PHARMACY_OPEN_X(a,b,c,1)

TErrorCode	IS_PHARMACY_OPEN_X (TPharmNum v_PharmNum, TInstituteCode v_InstituteCode, PHARMACY_INFO *Phrm_info, short TransactionNumber_in, short Spool_in);

int	IS_DRUG_INTERACAT( TDrugCode		drugA,
			   TDrugCode		drugB,
			   TInteractionType	*interaction,
			   TErrorCode		*ErrorCode );

// DonR 26Dec2016: Add function to check for Doctor Interaction Ishurim.
int CheckForInteractionIshur (	int				MemberTZ_in,
								short			MemberTzCode_in,
								TPresDrugs		*SaleDrug_1_in,
								TPresDrugs		*SaleDrug_2_in,
								TDocRx			*DocRxArray_in,
								DURDrugsInBlood	*BloodDrug,
								short			*DocApproved_out,
								short			*InteractionType_in_out	);

int	TransactionRestart( void );

int	get_drugs_in_blood (TMemberData				*Member,
						TDrugsBuf				*drugbuf,
						short					CallType_in,
						TErrorCode				*ErrorCode,
						int						*Restart_out);

int	test_special_prescription (TNumofSpecialConfirmation		*confnum,
							   TSpecialConfirmationNumSource	*confnumsrc,
							   TMemberData						*Member,
							   int								overseas_member_flag,
							   TTypeDoctorIdentification		DocIdType_in,
							   TDoctorIdentification			DocId_in,
							   TPresDrugs						*presdrugs,
							   TPresDrugs						*DrugArrayPtr,
							   int								NumDrugs,
							   TDrugCode						AlternateLargo_in,
							   TErrorCode						*ErrorCode,
							   short							TypeCheckSpecPr,
							   TPharmNum						v_PharmNum,
							   TPermissionType					v_MaccabiPharmacy,
							   TParticipatingType				v_ParticipatingType,
							   int								MaxExpiredDays_in,
							   short							*IshurWithTikra_out);

int	determine_actual_usage	(	TPresDrugs						*DrugArrayPtr,
								int								NumDrugs,
								short							ThisDrugSubscript,
								short							NumTreatmentDays,
								short							IngredientCode,
								double							IngrAmountPerDay,
								short							TreatmentDaysPerCycle
							);

TErrorCode	test_purchase_limits (TPresDrugs				*DrugToCheck,
								  TPresDrugs				*DrugArrayPtr,
								  int						NumDrugs,
								  TMemberData				*Member,
								  int						maccabi_pharmacy_flag,
								  int						overseas_member_flag,
								  short						ExemptFromQtyLimit,
								  int						PharmacyCode,
								  TErrorCode				*ErrorCode);

TErrorCode	test_pharmacy_ishur (TPharmNum					   	PharmIn,
								 TRecipeSource					RecipeSourceIn,
								 TMemberIdentification			MemIdIn,
								 TIdentificationCode			IdExtIn,
								 TMemberRights					MemberRightsIn,
								 TMemberDiscountPercent			MemberDiscountPercentIn,
								 int							MemberIllnessBitmapIn,
								 TTypeDoctorIdentification		DocIdType_in,
								 TDoctorIdentification			DocId_in,
								 TPresDrugs						*DrugPtrIn,
								 int							AsIf_Preferred_Largo_in,
								 TNumofSpecialConfirmation		PharmIshurNumIn,
								 TSpecialConfirmationNumSource	PharmIshurSrcIn,
								 TRecipeIdentifier				PrescriptionIdIn,
								 short int						LineNumIn,
								 short int						TrnNumIn,
								 int							PharmIshurProfessionIn,
								 TErrorCode						*ErrorCodeOut);

TErrorCode	check_sale_against_pharm_ishur (TPharmNum					   	PharmIn,
											TMemberIdentification			MemIdIn,
											TIdentificationCode				IdExtIn,
											TPresDrugs						*DrugPtrIn,
											int								NumDrugsIn,
											TNumofSpecialConfirmation		PharmIshurNumIn,
											TSpecialConfirmationNumSource	PharmIshurSrcIn,
											short							ExtendAS400IshurDaysOut[],
											int								PharmIshurProfessionOut[],
											TErrorCode						*ErrorCodeOut);



// DonR 23Feb2008: test_dentist_drugs_electronic(), and the new function
// test_home_visit_drugs_electronic(), are defined as macros. The underlying
// "real" function is test_special_drugs_electronic(), which takes as a
// parameter the starting point for the range of profession codes to be
// checked for specialist participation. 
//
// DonR 27Feb2008: The same is now true for the non-electronic versions!
int	test_special_drugs_electronic (TPresDrugs					*presdrug,
								   int							RuleStart_in,
								   TErrorCode					*ErrorCode);

int	test_special_drugs			  (TPresDrugs					*presdrugs,
								   TNumOfDrugLinesRecs			drugcnt,
								   TMemberParticipatingType		presParti,
								   int							RuleStart_in,
								   TErrorCode					*ErrorCode);


TErrorCode test_mac_doctor_drugs_electronic (TTypeDoctorIdentification	DocIdType_in,
											 TDoctorIdentification		docid,
											 TPresDrugs					*ThisDrug,
											 short						LineNo_in,
											 TAge						memberAge,
											 TMemberSex					memberGender,
											 short int					phrm_perm,
											 TRecipeSource				RecipeSource_in,
											 TMemberIdentification		MemberId,
											 TIdentificationCode		IdType,
											 short int					SpecialtySearchFlag,
											 int						*PharmIshurProfessionInOut,
											 TErrorCode					*ErrorCode);

TErrorCode test_no_presc_drugs( TPresDrugs		*presdrugs,
				TNumOfDrugLinesRecs	drugcnt,
				TMemberParticipatingType presParti,
				short int		memberAge,
				TMemberSex		memberGender,
				short int		phrm_perm,
				TMemberIdentification	MemberId,
				TIdentificationCode	IdType,
				TErrorCode		*ErrorCode );

TErrorCode GetParticipatingCode (TMemberParticipatingType	presParti,
								 TParticTst					*prt,
								 TPresDrugs					*PresLinP,
								 int						FixedPriceEnabled);

TErrorCode SimpleFindLowestPtn	(short			price_code_in,
								 int			fixed_price_in,
								 char			*insurance_type_in,
								 short			HealthBasket_in,
								 TPresDrugs		*PresLinP,
								 int			FixedPriceEnabled);

TErrorCode FindLowestPtnForPrescRequest	(	short	NewPriceCode_in,
											int		NewFixedPrice_in,
											short	OldPriceCode_in,
											int		OldFixedPrice_in,
											char	*insurance_type_in,
											short	*AssignedPriceCode_out,
											short	*AssignedPricePercent_out,
											int		*AssignedFixedPrice_out,
											short	*AssignedFixedPriceExists_out,
											short	*AssignedInsuranceUsed_out		);

TErrorCode SumMemberDrugsUsed( TPresDrugs		*pDrug,
			       TMemberIdentification	membId,
			       TIdentificationCode      idType,
			       TDuration		duration,
			       int			*SumUnits );

// DonR 11Sep2024 Bug #350304: Add address of v_ErrorCode to the argument list for CheckHealthAlerts,
// so that critical errors from the module will be written to prescriptions/error_code and sale
// completion by the pharmacy will be prevented.
int CheckHealthAlerts	(	short			ActionType,
							TMemberData		*Member_in,
							TPresDrugs		*SPres,
							short			NumCurrDrugs,
							TDocRx			*DocRx,
							TDrugsBuf		*drugbuf,
							PHARMACY_INFO	*Phrm_info_in,
							short			PrescSource_in,
							short			*v_ErrorCode_out	// Added by DonR 11Sep2024 Bug #350304.
						);

int update_doctor_presc (	short			LateArrivalMode_in,		// 0 = normal, 1 = process late-arriving doctor prescription
							short			ActionCode_in,
							int				Member_ID_in,
							short			Member_ID_Code_in,
							int				PrescID_in,
							short			LineNo_in,
							int				DocID_in,
							short			DocIDType_in,
							int				DocPrID_in,
							int				LargoPrescribed_in,
							int				LargoSold_in,
							TDrugListRow	*DL_in,
							int				ValidFromDate_in,
							int				SoldDate_in,
							int				SoldTime_in,
							short			OP_sold_in,
							short			UnitsSold_in,
							short			PrevUnsoldOP_in,
							short			PrevUnsoldUnits_in,
							short			PrescriptionType_in,	// DonR 01Aug2024 Bug #334612.
							int				DeletedPrescID_in,
							bool			ThisIsTheLastRx_in,
							int				*CarryForwardUnits_in_out,
							int				*LargoPrescribed_out,
							int				*ValidFromDate_out,
							short			*RxLinkCounter_out,
							int				*VisitDate_out,
							int				*VisitTime_out,
							int				*Rx_Added_Date_out,
							int				*Rx_Added_Time_out
						);

short ProcessRxLateArrivalQueue (int PrescID_in, int LargoPrescribed_in, int DocPrID_in, int DocID_in);

int CheckMember29G (TMemberData		*Member_in,
					TPresDrugs		*DrugLine_in,
					PHARMACY_INFO	*PhrmInfo_in);

short CheckForPartiallySoldDoctorPrescription (int		MemberId_in,
											   short	MemberIdCode_in,
											   int		DocId_in,
											   int		DoctorPrId_in,
											   int		EarliestUntilDate_in,
											   int		LatestUntilDate_in);

short MarkDeleted (char	*Caller_in,
				   int	PrescID_in,
				   int	Largo_in,
				   int	DeletionPrescId_in);

short GetSpoolLock (int PharmacyCode_in, int TransactionID_in, short SpoolFlg_in);

int ClearSpoolLock (int PharmacyCode_in);

short is_valid_presc_source (short PrescSource_in, short PrivatePharmacy_in, int *ErrorCode_out, short *DefaultSourceReject_out);

short LoadPrescrSource (void);

int TranslateTechnicalID (int Technical_ID_in, int *TZ_out, short *TZ_code_out, TMemberData *Member_out, int *HTTP_ResponseCode_out);

int EligibilityWithoutCard (int Member_id_in, short Member_id_code_in, TEligibilityData *EligibilityData_out, int *HTTP_ResponseCode_out);

void CHECK_JSON_ERROR ();

#include "TikrotRPC.h"
