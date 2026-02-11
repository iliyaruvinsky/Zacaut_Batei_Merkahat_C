/*=======================================================================
||																		||
||				ElectronicPr.c											||
||																		||
||======================================================================||
||																		||
||  PURPOSE:															||
||																		||
||  Electronic Prescription-related functions, split off from 			||
||  SqlHandlers.ec to shorten the file. (This was required because		||
||  compilation was failing on Linux.)									||
||																		||
||----------------------------------------------------------------------||
|| PROJECT:	SqlServer. (for macabi)										||
||----------------------------------------------------------------------||
 =======================================================================*/



// Include files.
#include <MacODBC.h>
#include "PharmacyErrors.h"
#include "MsgHndlr.h"
#include "MessageFuncs.h"		// DonR 26Aug2003: Function prototypes.
#include "TikrotRPC.h"
#include "As400UnixMediator.h"


static char *GerrSource = __FILE__;

extern	TDrugsBuf	v_DrugsBuf;	// DonR 16Feb2006 - made drugs-in-blood global.
extern	short		Form29gMessagesEnabled;	// DonR 21Oct2015 TEMPORARY.

/*=======================================================================
||			    Local declarations										||
 =======================================================================*/
#define Conflict_Test( r )									\
if (SQLERR_code_cmp (SQLERR_access_conflict) == MAC_TRUE)	\
{															\
	SQLERR_error_test ();									\
	r = MAC_TRUE;											\
	break;													\
}

#define MACCABI_PHARMACY	(Phrm_info.maccabi_pharmacy)	    
#define PRIVATE_PHARMACY	(Phrm_info.private_pharmacy)	    
#define PRATI_PLUS_PHARMACY	(Phrm_info.prati_plus_pharmacy)
#define	CAN_SELL_FUTURE_RX	(Phrm_info.can_sell_future_rx)
#define HESDER_MACCABI		(Phrm_info.has_hesder_maccabi)
#define PHARM_NOHAL_ENABLED	(Phrm_info.PharmNohalEnabled)		// DonR 23Apr2023 User Story #432608.

// DonR 15Jan2014: New conditions for calling "Meishar" for private pharmacies: we now use
// a per-pharmacy flag rather than the more general "prati plus" flag, and we enable calling
// Meishar from a private pharmacy only if the member has used his/her magnetic card.
// Note that the global "PPlusMeisharEnabled" flag (from sysparams/enablepplusmeishar) must
// also be set to enable private-pharmacy transactions to call Meishar.
// DonR 07Sep2015: Per Iris Shaya, removed the member-passed-card and Maccabi Doctor prescription criteria.
//#define	MEISHAR_PHARMACY	(Phrm_info.maccabi_pharmacy || (Phrm_info.meishar_enabled	&&	\
//															PPlusMeisharEnabled			&&	\
//															(v_MoveCard > 0)			&&	\
//															(v_MoveCard < 9999)			&&	\
//															(v_RecipeSource == RECIP_SRC_MACABI_DOCTOR)))
#define	MEISHAR_PHARMACY	(Phrm_info.maccabi_pharmacy || (Phrm_info.meishar_enabled && PPlusMeisharEnabled))



// DonR 09Jan2012: To replace some repetitive code, set up a macro to indicate a non-discounted sale.
// Note that this logic recognizes that the "Meishar" application can assign a fixed price of zero.
// DonR 12Jun2012: Added a further macro, adding a condition that an AS/400 ishur is *not* being used.
// This new macro, FULL_PRICE_NO_ISHUR_ETC, is used to decide whether to send various discount-available
// warnings. (Note that in Trn. 2003 we still have to test for no-error, since discount-available
// warnings are lower-priority than most other warnings.)
#define FULL_PRICE_SALE(i)	((SPres[i].RetPartCode				== 1)					&&	\
							 (SPres[i].PriceSwap				== 0)					&&	\
							 (SPres[i].ret_part_source.table	!= FROM_GADGET_APP))

// DonR 09Aug2021 User Story #163882: Another condition for *not* giving "discount possible" messages is where
// the person buying drugs is a "darkonai-plus" who is not subject to any discounts other than "shovarim"
// provided by his/her insurance company. The relevant flag is Member.force_100_percent_ptn.
#define FULL_PRICE_NO_ISHUR_ETC(i)	((SPres[i].RetPartCode				== 1)							&&	\
									 (SPres[i].PriceSwap				== 0)							&&	\
									 (SPres[i].ret_part_source.table	!= FROM_GADGET_APP)				&&	\
									 (SPres[i].ret_part_source.table	!= FROM_SPEC_PRESCS_TBL)		&&	\
									 (SPres[i].DFatalErr				== 0)							&&	\
									 (v_IdentificationCode				!= 3) /* Not Kishon diver */	&&	\
									 (Member.force_100_percent_ptn		== 0) /* Not a Darkonai+  */	&&	\
									 (v_RecipeSource					!= RECIP_SRC_NO_PRESC)			&&	\
									 (v_RecipeSource					!= RECIP_SRC_EMERGENCY_NO_RX))


// DonR 10Apr2013: Macro to tell us if member used a magnetic card - "real" or temporary.
#define MEMBER_USED_MAG_CARD	((v_MoveCard > 0) && (v_MoveCard < 9999))


// DonR 04Dec2014: Macro to identify members entitled to 100% discount - either for all
// drugs, or based on a match between member and drug illness bitmaps.
// DonR 30Dec2014: Serious illnesses now require Code 7 or 17 to give 100% discounts; if there are no
// serious-illness codes, the discount applies across the board.
// DonR 13May2020 CR #31591: Add "IllnessDiscounts" flag to these three macros, so discounts will be given
// only for those Largo Types specified in Sysparams/sick_disc_4_types.
#define GETS_100PCT_DISCOUNT(DRUG)	(((	Member.maccabi_code == 7)	|| (Member.maccabi_code == 17)) &&	\
									 (	DRUG.IllnessDiscounts	)									&&	\
									 ((	!Member.illness_bitmap	)	|| (Member.illness_bitmap & DRUG.illness_bitmap)))

// DonR 02Oct2018 CR #13262: Add new macros to identify member-disease matches that do and do not
// require a specific diagnosis code to grant discounts.
#define GETS_100PCT_DISCOUNT_WITHOUT_DIAGNOSIS(DRUG)	(((	Member.maccabi_code == 7)	|| (Member.maccabi_code == 17))								&&	\
														 (	DRUG.IllnessDiscounts	)																&&	\
														 ((!Member.illness_bitmap)																||		\
														  ( Member.illness_bitmap & DRUG.illness_bitmap & DiseasesWithoutDiagnosesBitmap)		||		\
														  ((Member.illness_bitmap & DRUG.illness_bitmap & DiseasesWithDiagnosesBitmap)	 && (!DRUG.has_diagnoses))))

#define GETS_100PCT_DISCOUNT_WITH_DIAGNOSIS(DRUG)	(((	Member.maccabi_code == 7	)	|| (Member.maccabi_code == 17))							&&	\
													 (	DRUG.IllnessDiscounts	)																&&	\
													 (	DRUG.has_diagnoses		)																&&	\
													 (	Member.illness_bitmap & DRUG.illness_bitmap & DiseasesWithDiagnosesBitmap))



// DonR 01Dec2016: Under the new Bakara Kamutit logic, some purchase limits can exclude members who
// have illnesses relevant to the drug being bought from limit checking. This applies only to
// actual diseases, not to accidents.
// DonR 29Jul2025 User Story #435969: Add new conditions for people in home-hospice care.
// For these members, don't bother looking at Maccabi Code.
#define EXEMPT_FROM_LIMIT	(																						\
								(	((Member.maccabi_code == 7)	|| (Member.maccabi_code == 17))					&&	\
									(SPres[i].PurchaseLimitExemptDisease > 0)									&&	\
									(Member.illness_bitmap & SPres[i].DL.illness_bitmap & ActualDiseaseBitmap)		\
								)																					\
								||																					\
								(																					\
									(SPres[i].PurchaseLimitExemptDisease == 2)									&&	\
									((Member.dangerous_drug_status == 1) || (Member.dangerous_drug_status == 2))	\
								)																					\
							)

// DonR 29Dec2014: Macro to identify members who have some form of discount - either  100% or
// some other percentage across-the-board, or else one or more specific illnesses for which
// they get a full discount on relevant drugs.
// DonR 30Dec2014: Serious-illness codes are no longer an independent source of discounts - they
// function only as an (optional) qualifier to the effect of Codes 7 and 17.
// DonR 13May2020 CR #31591: Add new VentilatorDiscount flag as a new way member can get a discount.
// Marianna 28Mar2022 Epic 232192: add darkonai type and mem_id_extension to support discounts for Harel Foreign Workers.
//#define MEMBER_GETS_DISCOUNTS		((Member.maccabi_code == 7) || (Member.maccabi_code == 17) || (Member.discount_percent > 0) || (Member.VentilatorDiscount))
#define MEMBER_GETS_DISCOUNTS		(	((Member.darkonai_type		==  2) && (Member.mem_id_extension == 9))	||	\
										( Member.maccabi_code		==  7)										||	\
										( Member.maccabi_code		== 17)										||	\
										( Member.discount_percent	>   0)										||	\
										( Member.VentilatorDiscount)													)

// DonR 28Mar2016: Macro to identify situations where discounts can be granted even if the drug in question is not in the
// health basket. The relevant illness codes are set up in the new illness_codes table.
// DonR 02Jun2016: We want to ignore the Health Basket requirement only for drugs that have at least one
// "nohal automati" (either a regular drug_extension rule or a specialist rule from spclty_largo_prcnt)
// that uses supplemental health insurance - regardless of whether the rule in question applies to the
// member buying drugs.
//#define IGNORE_HEALTH_BASKET(DRUG)	(Member.illness_bitmap & DRUG.illness_bitmap & BypassHealthBasketBitmap)
#define IGNORE_HEALTH_BASKET(DRUG)	((Member.illness_bitmap & DRUG.illness_bitmap & BypassHealthBasketBitmap) && (DRUG.has_shaban_rules))

char *PosInBuff;
static int  nOut;


#define	NO_MAX_LARGO	9999999

// DonR 09Sep2009: Added global SQL variables for identifying "visible" drugs.
static short	drug_deleted		= DRUG_DELETED;
static short	pharmacy_assuta		= PHARMACY_ASSUTA;
static short	assuta_drug_active	= ASSUTA_DRUG_ACTIVE;
static short	assuta_drug_past	= ASSUTA_DRUG_PAST;

extern int		ExpensiveDrugMacDocRx;			// DonR 23Jun2024 User Story #318200. Value comes from sysparams/ExpensiveDrugMacDocRx.
extern int		ExpensiveDrugNotMacDocRx;		// DonR 23Jun2024 User Story #318200. Value comes from sysparams/ExpensiveDrugNotMacDocRx.
extern short	min_refill_days;				// sysparams table, read by SqlServer.ec.
extern int		OverseasMaxSaleAmt;				// DonR 12Mar2012 - also from sysparams.
extern short	NihulTikrotLargoLen;			// DonR 30Jul2024 User Story #338533.
extern short	MaxShortDurationDays;			// DonR 25Nov2024 User Story #366220.
extern short	MaxShortDurationOverlapDays;	// DonR 25Nov2024 User Story #366220.
extern short	MaxLongDurationOverlapDays;		// DonR 25Nov2024 User Story #366220.
extern short	MaxPharmIshurConfirmAuthority;	// DonR 27Apr2025 User Story #390071.

// DonR 23Aug2011: Factor to calculate prices for VAT-exempt (Eilat) pharmacies.
extern double no_vat_multiplier;


#define	NO_ERROR 				   0
#define	ERR_DATABASE_ERROR 		3010


int	process_spools					(void);
int	init_package_size_array			(void);
int	SALE_compare					(const void *data1, const void *data2);
int	DRUG_compare					(const void *data1, const void *data2);


#define GADGET_NONE		0
#define GADGET_RIGHT	1
#define GADGET_LEFT		2


static short	pkg_size [100000];
static int		pkg_size_asof_date	= 0;
static int		pkg_size_asof_time	= 0;


// Electronic Prescription functions.

// These need to be replaced with configuration-table entries!
#define PRESCR_VALID_DAYS			75
#define SELECT_PRESC_EARLY_DAYS		75
#define OPEN_PACKAGE_BELOW			1.0		// DonR 29APR2003 - was 0.8 before.



/*=======================================================================
||																		||
||			 HandlerToMsg_2001											||
||	Message handler for message 2001:									||
||			 Prescription Drugs List Request.							||
||																		||
 =======================================================================*/

int HandlerToMsg_2001 (char			*IBuffer,
					   int			TotalInputLen,
					   char			*OBuffer,
					   int			*p_outputWritten,
					   int			*output_type_flg,
					   SSMD_DATA	*ssmd_data_ptr)
{
	// Local declarations.
	int				tries;
	int				reStart;
	int				i;
	int				flg;
	int				err;
	int				MaxFutureDate			= 29999999;	// I.e. we should all live so long.
	short			FindPref_err			= 0;		// DonR 16Sep2012 - so we don't overwrite v_ErrorCode.
	short			GoodFetch = 0;
	short			DispenseAsWritten;
	//short			MemberTzahal;
	short			GaveFutureDrugWarning	= 0;
	ISOLATION_VAR;

	// Package-opening variables.
	float			OpenPkgPercent = OPEN_PACKAGE_BELOW;
	float			fOpenPkgUnits;
	int				iOpenPkgUnits;


	// SQL variables.

	// Arrays to hold drugs prescribed data.
	TPresDrugs			SPres					[MAX_REC_ELECTRONIC];
	short int			PR_NumPerDose			[MAX_REC_ELECTRONIC];
	short int			PR_DosesPerDay			[MAX_REC_ELECTRONIC];
	short int			PR_Status				[MAX_REC_ELECTRONIC];
	short int			PR_PrevSoldUnits		[MAX_REC_ELECTRONIC];
	short int			PR_PrevSoldOP			[MAX_REC_ELECTRONIC];
	short int			DL_PreferredFlg			[MAX_REC_ELECTRONIC];
	int					DL_PreferredLargo		[MAX_REC_ELECTRONIC];
	int					PR_DoctorPrID			[MAX_REC_ELECTRONIC];
	int					PR_Date					[MAX_REC_ELECTRONIC];
	int					PR_Original_Largo_Code	[MAX_REC_ELECTRONIC];
	int					PR_ID					[MAX_REC_ELECTRONIC];
	char				PR_PrescriptionType		[MAX_REC_ELECTRONIC] [2];


	// Input Message fields.
	TPharmNum				v_PharmNum;
	TInstituteCode			v_InstituteCode;
	TTerminalNum			v_TerminalNum;
	TGenericTZ				v_MemberIdentification;
	TIdentificationCode		v_IdentificationCode;
	TGenericTZ				DocID;
	TRecipeSource			v_RecipeSource;
	int						v_DoctorPrescID;
	int						MinDocPrID;
	int						MaxDocPrID;

	// Drug lines are pulled from DB, not from incoming message.
	TNumOfDrugLinesRecs		v_NumOfDrugLinesRecs = 0;
	TDrugCode				v_DrugCode;
	short int				DL_openable_pkg;
	int						DL_package_size;
	int						DP_package_size;
	short					DL_del_flg;
	short					DL_assuta_drug;
	short					DL_chronic_flag;
	short					DL_chronic_months;
	int						UnitsPrescribed;
	int						UnitsToSell;
	int						OPToSell;

	// Member, Doctor, Pharmacy, and other response fields.
	PHARMACY_INFO			Phrm_info;
	TMemberFamilyName		v_MemberFamilyName;
	TMemberFirstName		v_MemberFirstName;
	short					v_SpecPresc;
	TGenericTZ			   	v_TempMembIdentification;
	TIdentificationCode		v_TempIdentifCode;
	TDate					v_TempMemValidUntil;
	TDoctorFamilyName		v_DoctorFamilyName;
	TDoctorFirstName		v_DoctorFirstName;
	TErrorCode				v_ErrorCode;
	double					DecimalNumPerDose;
	short					NumericPrescType;

	// General SQL variables.
	int						SysDate;
	int						EarliestPrescrDate;
	int						LatestPrescrDate;
	TDrugCode				TestLargo;
	int						RowsFound;
	char					DummyPhone	[10 + 1];
	short					DummyCheckInteractions;
	int						WHERE_clause_ID;



	// Body of function
 
	// Initialize variables.
	REMEMBER_ISOLATION;
	PosInBuff				= IBuffer;
	v_ErrorCode				= NO_ERROR;
	reStart					= MAC_FALS;
	SysDate					= GetDate();

	v_MemberFamilyName	[0]	= 0;
	v_MemberFirstName	[0]	= 0;
	v_DoctorFamilyName	[0]	= 0;
	v_DoctorFirstName	[0]	= 0;

	memset ((char *)SPres, 0, sizeof(SPres));


	// Read message fields data into variables.
	v_PharmNum				= GetLong	(&PosInBuff, TPharmNum_len				); CHECK_ERROR ();
	v_InstituteCode			= GetShort	(&PosInBuff, TInstituteCode_len			); CHECK_ERROR ();
	v_TerminalNum			= GetShort	(&PosInBuff, TTerminalNum_len			); CHECK_ERROR ();
	v_MemberIdentification	= GetLong	(&PosInBuff, TGenericTZ_len				); CHECK_ERROR ();
	v_IdentificationCode	= GetShort	(&PosInBuff, TIdentificationCode_len	); CHECK_ERROR ();
	DocID					= GetLong	(&PosInBuff, TGenericTZ_len				); CHECK_ERROR ();
	v_RecipeSource			= GetShort	(&PosInBuff, TRecipeSource_len			); CHECK_ERROR ();
	v_DoctorPrescID			= GetLong	(&PosInBuff, TDate_len					); CHECK_ERROR ();


	// Copy relevant fields to SSMD structure.
	ssmd_data_ptr->pharmacy_num = v_PharmNum;
	ssmd_data_ptr->member_id	= v_MemberIdentification;
	ssmd_data_ptr->member_id_ext= v_IdentificationCode;


	// DonR 23Mar2011: For better performance, revamped SELECT criteria to conform to table index and
	// avoid conditional logic; also got rid of criteria for origin_code and status, since pharmacy
	// transactions no longer add prescription rows and all existing statuses were being selected anyway.
	//
	// DonR 04Apr2011: For even better performance, I've built two new indices for prescriptions.
	// The first one should be used when Doctor Prescription ID is non-zero, and consists
	// of idnumber, prescription, treatmentcontr, and idcode. The second, for when the pharmacy sends
	// Doctor Prescription ID of zero, consists of idnumber, treatmentcontr, prescriptiondate, and
	// idcode. To take advantage of these indices, build the SELECT statement conditionally based on
	// the criteria supplied by the pharmacy.
	if (v_DoctorPrescID > 0)
	{
		// DonR 11Sep2012: If pharmacy is selecting a single doctor prescription, set the
		// maximum future date paramater based on the system date and the sysparams
		// parameter sale_ok_early_days.
		// DonR 21Aug2023: Switch to a single sysparams variable instead of two redundant ones.
		MaxFutureDate = IncrementDate (SysDate, SaleOK_EarlyDays);

		// Select all doctor presciptions matching the supplied Prescription ID regardless of date.
		// DonR 22Apr2015: Add capability of reading from new doctor_presc table instead of prescr_wr_new.
		// DonR 28Apr2015: Note that we could just use an SQL CAST() operator to force type compatibility
		// between the old prescriptiontype field and the new, numeric prescription_type field; but this
		// would be somewhat dangerous if somebody put a value there less than zero or greater than nine,
		// since that would cause an SQL error when the row was FETCHed.
		WHERE_clause_ID = TR2001_Rx_cursor_by_Pr_ID;

	}	// Pharmacy supplied a Doctor Prescription ID.

	else
	{	// Select all prescriptions for the specified doctor/patient within the standard date range.
		EarliestPrescrDate	= AddDays (SysDate, (0 - PRESCR_VALID_DAYS));
		LatestPrescrDate	= AddDays (SysDate, SELECT_PRESC_EARLY_DAYS);
		WHERE_clause_ID		= TR2001_Rx_cursor_without_Pr_ID;
	}	// Pharmacy did not supply a Doctor Prescription ID, so select by Prescription Date.


	// SQL Retry Loop.
	reStart = MAC_TRUE;

	// DonR 19Apr2005: Avoid unnecessary lock errors by downgrading "isolation"
	// for this transaction.
	SET_ISOLATION_DIRTY;

	for (tries = 0; tries < SQL_UPDATE_RETRIES && reStart == MAC_TRUE; tries++)
	{
		reStart = MAC_FALS;

		// Test pharmacy data.
		err = IS_PHARMACY_OPEN (v_PharmNum, v_InstituteCode, &Phrm_info);
		if (err != MAC_OK)
		{
			SetErrorVar (&v_ErrorCode, err);
			break;
		}


		// Test Member validity.

		// Member ID must be non-zero.
		if (v_MemberIdentification == 0)
		{
			SetErrorVar (&v_ErrorCode, ERR_MEMBER_ID_CODE_WRONG);
			break;
		}

		// Test temporary members.
		if (v_IdentificationCode == 7)
		{
			ExecSQL (	MAIN_DB, READ_tempmemb,
						&v_TempIdentifCode,			&v_TempMembIdentification,	&v_TempMemValidUntil,
						&v_MemberIdentification,	END_OF_ARG_LIST										);

			Conflict_Test (reStart);

			if (SQLERR_code_cmp (SQLERR_not_found) == MAC_TRUE)
			{
				SetErrorVar (&v_ErrorCode, ERR_MEMBER_ID_CODE_WRONG);
				break;
			}

			if (SQLERR_error_test ())
			{
				SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR);
				break;
			}

			if (v_TempMemValidUntil >= SysDate)
			{
				v_IdentificationCode   = v_TempIdentifCode;
				v_MemberIdentification = v_TempMembIdentification;
			}
		}	// Temporary Member test.


		// After Temporary Member stuff, Member ID must be 58 or greater.
		if (v_MemberIdentification < 58)
		{
			SetErrorVar (&v_ErrorCode, ERR_MEMBER_ID_CODE_WRONG);
			break;
		}

		// Try to read Member row.
		ExecSQL (	MAIN_DB, TR2001_READ_member_data,
					&v_MemberFamilyName,		&v_MemberFirstName,
					&Member.date_of_bearth,		&Member.maccabi_code,
					&v_SpecPresc,				&Member.maccabi_until,
					&Member.died_in_hospital,
					&v_MemberIdentification,	&v_IdentificationCode,
					END_OF_ARG_LIST											);

		Conflict_Test (reStart);
			
		// DonR 04Dec2022 User Story #408077: Use a new function to set some Member flags and values.
		SetMemberFlags (SQLCODE);
			
		if (SQLERR_code_cmp (SQLERR_not_found) == MAC_TRUE)
		{
			SetErrorVar (&v_ErrorCode, ERR_MEMBER_ID_CODE_WRONG);
			break;
		}
			
		if (SQLERR_error_test ())
		{
			SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR);
			break;
		}

		// Logical data tests on Member eligibility.
//		// DonR 25Nov2021 User Story #206812: Get died-in-hospital indicator from the Ishpuz system.
		// DonR 30Nov2022 User Story #408077: Use new macros defined in MsgHndlr.h to determine
		// eligibility.
		if ((MEMBER_INELIGIBLE) || (MEMBER_IN_MEUHEDET))
//			(Member.died_in_hospital)	||
		{
			SetErrorVar (&v_ErrorCode, ERR_MEMBER_NOT_ELEGEBLE);
			break;
		}


		// Test doctors ID-CODE.
		// DonR 02Sep2004: Perform this lookup for Maccabi-by-hand prescriptions as well.
		// DonR 11Aug2008: Added new Maccabi Nurse prescription source.
		// DonR 04Apr2016: Per Iris Shaya, added new prescription source for Maccabi Dieticians.
		if ((v_RecipeSource == RECIP_SRC_MACABI_DOCTOR)		||
			(v_RecipeSource == RECIP_SRC_MAC_DOC_BY_HAND)	||
			(v_RecipeSource == RECIP_SRC_MAC_DIETICIAN)		||
			(v_RecipeSource == RECIP_SRC_MACCABI_NURSE))
		{
			ExecSQL (	MAIN_DB, READ_doctors,
						&v_DoctorFirstName,		&v_DoctorFamilyName,
						&DummyPhone,			&DummyCheckInteractions,
						&DocID,					END_OF_ARG_LIST			);

			Conflict_Test (reStart);

			if (SQLERR_code_cmp (SQLERR_not_found) == MAC_TRUE)
			{
				SetErrorVar (&v_ErrorCode, ERR_DOCTOR_ID_CODE_WRONG);
				break;
			}
			else
			{
				if (SQLERR_error_test ())
				{
					SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR);
					break;
				}
			}
		}	// Prescription is from Maccabi doctor.
	

		// Read applicable drugs prescribed from Prescriptions-Written table.
		v_NumOfDrugLinesRecs = 0;

		// DonR 29Jul2020: To avoid (rare) cursor-already-prepared messages, moved
		// the declarations of TR2001_Rx_cursor here and changed them to 
		// declare-and-open commands.
		if (WHERE_clause_ID == TR2001_Rx_cursor_by_Pr_ID)
		{
			DeclareAndOpenCursor (	MAIN_DB, TR2001_Rx_cursor, WHERE_clause_ID,
									&v_MemberIdentification,	&v_DoctorPrescID,
									&DocID,						&v_IdentificationCode,
									END_OF_ARG_LIST											);
		}
		else
		{
			DeclareAndOpenCursor (	MAIN_DB, TR2001_Rx_cursor, WHERE_clause_ID,
									&v_MemberIdentification,	&DocID,
									&EarliestPrescrDate,		&LatestPrescrDate,
									&v_IdentificationCode,		END_OF_ARG_LIST		);
		}

		if (SQLERR_error_test ())
		{
			SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR);
			break;
		}


		// We shouldn't get here if we've hit a fatal error - but just to be paranoid, we'll
		// make the prescription-reading conditional.
		if (!SetErrorVar (&v_ErrorCode, v_ErrorCode))
		{
			do
			{
				// Try to read the next drug prescribed.
				FetchCursorInto (	MAIN_DB, TR2001_Rx_cursor,
									&PR_DoctorPrID		[v_NumOfDrugLinesRecs],
									&SPres				[v_NumOfDrugLinesRecs].DrugCode,
									&DecimalNumPerDose,
									&PR_NumPerDose		[v_NumOfDrugLinesRecs],
									&PR_DosesPerDay		[v_NumOfDrugLinesRecs],
									&SPres				[v_NumOfDrugLinesRecs].Duration,
									&SPres				[v_NumOfDrugLinesRecs].Op,
									&PR_Date			[v_NumOfDrugLinesRecs],
									&PR_PrescriptionType[v_NumOfDrugLinesRecs],
									&NumericPrescType,
									&PR_Status			[v_NumOfDrugLinesRecs],
									&PR_PrevSoldUnits	[v_NumOfDrugLinesRecs],
									&PR_PrevSoldOP		[v_NumOfDrugLinesRecs],
									&PR_ID				[v_NumOfDrugLinesRecs],
									END_OF_ARG_LIST												);

				Conflict_Test (reStart);
		

				// If we successfully read something, process this prescribed drug!
				// Note: We need to remember that the fetch was (or wasn't) successful, since the
				// generic-substitution logic will override the SQL status variables. Thus "GoodFetch"!
				// WORKINGPOINT: Is an unceremonious exit here really appropriate? Discuss with Iris!
				GoodFetch = (SQLERR_code_cmp (SQLERR_ok) == MAC_TRUE);

				// DonR 22Mar2011: If the doctor_presc fetch failed, there's no point in reading
				// from drug_list - so move the "if (GoodFetch)" up here.
				if (GoodFetch)
				{
					// DonR 27Apr2015: Since we're reading from the new doctor_presc table, translate a few fields
					// to be compatible with the old-format data.
					// For now, assume that we don't have to do any special translation of Prescription Type
					// other than converting it from a SMALLINT to a character field.
					if ((NumericPrescType >= 0) && (NumericPrescType <= 9))
					{
						sprintf (PR_PrescriptionType [v_NumOfDrugLinesRecs], "%d", NumericPrescType);
					}
					else
					{
						// If the number is out of range, default to a blank Prescription Type.
						strcpy (PR_PrescriptionType [v_NumOfDrugLinesRecs], " ");
					}

					// If the doctor sent a fractional amount-per-dose value, zero out the usage-instruction
					// fields so pharmacy will know that they have to go by the instructions on the printed
					// prescription sheet.
					// DonR 11Jun2015: Also suppress usage-instruction fields for non-daily-use prescriptions,
					// as indicated by Prescription Type between 1 and 4.
					if (((NumericPrescType >= 1) && (NumericPrescType <= 4))		||
						((DecimalNumPerDose - (double)PR_NumPerDose [v_NumOfDrugLinesRecs]) > .01))
					{
						PR_NumPerDose [v_NumOfDrugLinesRecs] = PR_DosesPerDay [v_NumOfDrugLinesRecs] = 0;
					}

					// DonR 21May2013: If this is not a daily-use prescription, zero out the usage-instruction
					// fields EXCEPT for treatment length.
					if ((*PR_PrescriptionType [v_NumOfDrugLinesRecs] == '1')	|| 
						(*PR_PrescriptionType [v_NumOfDrugLinesRecs] == '2'))
					{
						PR_NumPerDose [v_NumOfDrugLinesRecs] = PR_DosesPerDay [v_NumOfDrugLinesRecs] = 0;
					}

					// DonR 26Nov2003: Per Iris Shaya, if the drug prescribed is not in the drug_list
					// table, don't report this drug to the pharmacy.
					ExecSQL (	MAIN_DB, TR2001_READ_drug_list,
								&TestLargo,
								&SPres[v_NumOfDrugLinesRecs].DL.economypri_group,
								&DL_PreferredFlg[v_NumOfDrugLinesRecs],
								&DL_PreferredLargo[v_NumOfDrugLinesRecs],
								&SPres[v_NumOfDrugLinesRecs].DL.multi_pref_largo,
								&DL_package_size,
								&DL_openable_pkg,
								&DL_chronic_flag,
								&DL_chronic_months,
								&DL_del_flg,
								&DL_assuta_drug,
								&SPres[v_NumOfDrugLinesRecs].DrugCode,
								END_OF_ARG_LIST											);

					if (SQLERR_code_cmp (SQLERR_not_found) == MAC_TRUE)
					{
						continue;	// Skip past this drug.
					}

					if (SQLERR_error_test ())
					{
						SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR);
						break;
					}
					else
					{
						// Good read; now do the "post-SELECT select" on del_flg and assuta_drug_status.
						if ((DL_del_flg != drug_deleted) ||
								((Phrm_info.pharm_category == pharmacy_assuta) &&
								 ((DL_assuta_drug == assuta_drug_active) || (DL_assuta_drug == assuta_drug_past))))
						{
							// This drug is OK - just drop through.
						}
						else
						{
							// This drug is deleted and not an Assuta drug for an Assuta pharmacy - skip it.
							continue;
						}
					}	// Successful read of drug_list.

					// Substitute Generic Drugs.
					PR_Original_Largo_Code[v_NumOfDrugLinesRecs] = SPres[v_NumOfDrugLinesRecs].DrugCode;
					DP_package_size = DL_package_size;	// So we can computer the amount of previous sales accurately.

					// DonR 24Jun2010: Per Iris Shaya, if there is an AS/400 ishur for this member/drug,
					// don't perform generic-drug substitution. Note that (at least for now) we aren't
					// worrying about the extension of expired ishurim here!
					// DonR 22Mar2011: Reordered last two WHERE criteria to conform to index structure.
					// DonR 23Mar2011: Don't look for an AS/400 ishur if we already know member doesn't have one.
					// DonR 12Sep2012: Ishurim with Member Price Code of zero are invalid - ignore them!
					if (v_SpecPresc)
					{
						ExecSQL (	MAIN_DB, TR2001_READ_count_special_prescs,
									&RowsFound,
									&v_MemberIdentification,	&v_IdentificationCode,
									&TestLargo,					&SysDate,
									&SysDate,					END_OF_ARG_LIST				);
					}
					else
					{
						// If we know member doesn't have an AS/400 ishur, we'll just pretend that
						// we looked and didn't find anything.
						SQLCODE = RowsFound = 0;
					}

					if ((SQLCODE != 0) || (RowsFound < 1))
					{
						// DonR 03Feb2004: Use separate function to find preferred (generic) drug.
						find_preferred_drug (SPres[v_NumOfDrugLinesRecs].DrugCode,
											 SPres[v_NumOfDrugLinesRecs].DL.economypri_group,
											 DL_PreferredFlg	[v_NumOfDrugLinesRecs],
											 DL_PreferredLargo	[v_NumOfDrugLinesRecs],
											 SPres[v_NumOfDrugLinesRecs].DL.multi_pref_largo,	// Enable history check for alternate preferred drug.
											 1,													// Perform substitution on drugs with Preferred Status = 3.
											 DocID,
											 v_MemberIdentification,
											 v_IdentificationCode,
											 &TestLargo,
											 NULL,	// Specialist Drug flag pointer
											 NULL,	// Parent Group Code pointer
											 NULL,	// Preferred Drug Member Price pointer
											 &DispenseAsWritten,
											 NULL,	// In-health-basket pointer
											 NULL,	// Rule-status pointer
											 NULL,	// DL structure pointer
											 &FindPref_err);

						// DonR 16Sep2012: We don't want to lose a previous value of v_ErrorCode; so instead
						// of passing it to find_preferred_drug(), which zeroes it out, pass a separate variable
						// and then assign it with SetErrorVar(), which tests error severity and overwrites only
						// when the new error is more severe than any previous error.
						SetErrorVar (&v_ErrorCode, FindPref_err);

						// DonR 13Jul2005: Don't substitute if doctor has filled out a
						// "dispense as written" ishur (Form 90/92).
						// DonR 08Mar2006: If drug's preferred_flg is 2, don't do the substitution.
						// If the doctor has provided a "dispense as written" ishur, Trn. 2003 will
						// allow "as-if-preferred" participation; and if not, member will pay based
						// on the drug's default participation conditions.
						if ((SPres[v_NumOfDrugLinesRecs].DrugCode	!= TestLargo)	&&
							(DL_PreferredFlg[v_NumOfDrugLinesRecs]	!= 2)			&&	// DonR 08Mar2006.
							(!DispenseAsWritten))
						{
							SPres[v_NumOfDrugLinesRecs].DrugCode = TestLargo;	// Swap in preferred drug.

							// DonR 22Mar2011: Since we're now normally NOT re-reading drug_list, we have to
							// force a re-read when we swap in a preferred drug for the prescribed drug.
							ExecSQL (	MAIN_DB, TR2001_READ_drug_list_preferred_largo_fields,
										&DL_package_size,			&DL_openable_pkg,
										&DL_chronic_flag,			&DL_chronic_months,
										&TestLargo,					&drug_deleted,
										&Phrm_info.pharm_category,	&pharmacy_assuta,
										&assuta_drug_active,		&assuta_drug_past,
										END_OF_ARG_LIST										);

							Conflict_Test (reStart);

							if (SQLERR_code_cmp (SQLERR_not_found) == MAC_TRUE)
							{
								if (SetErrorVarF (&SPres[v_NumOfDrugLinesRecs].DrugAnswerCode,
												  ERR_DRUG_CODE_NOT_FOUND,
												  &SPres[v_NumOfDrugLinesRecs].DFatalErr))
								{
									GerrLogReturn (GerrId,
												   "Trn2001: Couldn't find Largo %d in drug_list.", (int)TestLargo);
									SetErrorVar (&v_ErrorCode, ERR_DRUG_S_WRONG_IN_PRESC);
									continue;
								}
							}	// Drug not found.
							else
							{
								if (SQLERR_error_test ())
								{
									SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR);
									break;
								}
							}
						}	// Substituting preferred drug for prescribed drug.
					}	// Prescribed drug is not covered by AS/400 ishur.


					// DonR 22Mar2011: No need to loop through drugs three times - just do everything
					// in one loop!
					SPres[v_NumOfDrugLinesRecs].DrugAnswerCode		= NO_ERROR;
					SPres[v_NumOfDrugLinesRecs].DFatalErr			= MAC_FALS;

					// DonR 12Aug2003: Avoid division-by-zero problems by ensuring that package
					// size has some real value. (I don't know if there's any real chance of
					// problems, but paranoia is sometimes a good thing.)
					if (DL_package_size < 1)
						DL_package_size = 1;
					if (DP_package_size < 1)
						DP_package_size = 1;

					// DonR 05Feb2003: If the package size is 1, we must assume that the
					// package is non-openable EVEN IF OPENABLE_PKG IS NON-ZERO. This
					// assumption is necessary for now, because many drugs in the database
					// have openable_pkg set true even though they are not in fact openable.
					if (DL_package_size == 1)
					{
						DL_openable_pkg = 0;
					}

					// DonR 05Feb2003, per Iris Shaya: If one or more of the three
					// numerical parameters (num/dose, doses/day, days) is zero,
					// treat the prescription as if the drug were in an unopenable
					// package: that is, just use the OP as the doctor prescribed.
					if ((  PR_NumPerDose	[v_NumOfDrugLinesRecs]
						 * PR_DosesPerDay	[v_NumOfDrugLinesRecs]
						 * SPres[v_NumOfDrugLinesRecs].Duration)	< 1)
					{
						DL_openable_pkg = 0;
					}


					// Compute quantity.
					//
					// There are two basic cases to handle:
					//
					//    1) Medication is in openable form and is not OTC; this is
					//       indicated by drug_list.openable_pkg = 1. In this case,
					//       multiply Units/Dose x Doses/Day x Duration to get total
					//       units prescribed. Subtract
					//            ((PrevSoldOP x package_size) + PrevSoldUnits) from
					//       units prescribed to get units to sell. Then compute OP and
					//       units to sell, based on package size and open-package
					//       threshold (initially hard-coded at 80%, but will be
					//       replaced by a configuration parameter).
					//
					//    2) Medication is OTC or in non-openable form (such as syrup);
					//       this is indicated by drug_list.openable_pkg = 0.
					//       In this case, Doses/Day, Units/Dose, and Duration are
					//       transmitted as-is, Total Units is set to zero, and OP is
					//       sent as prescribed, minus any previously-sold OP's.
					//
					// 29Jan2003: Per Iris Shaya, if Units/Dose x Doses/Day x Duration
					// equals zero, use the OP as requested by the doctor, even if the
					// drug comes in an openable package.
					if (DL_openable_pkg != 0)	// Package is openable.
					{
						// First, compute units per day.
						if (PR_NumPerDose[v_NumOfDrugLinesRecs] < 1)
						{
							UnitsPrescribed = PR_DosesPerDay[v_NumOfDrugLinesRecs];
						}
						else
						{
							UnitsPrescribed = PR_NumPerDose[v_NumOfDrugLinesRecs] * PR_DosesPerDay[v_NumOfDrugLinesRecs];
						}

						// Multiply by duration (in days) of treatment.
						if (SPres[v_NumOfDrugLinesRecs].Duration > 1)
						{
							UnitsPrescribed *= SPres[v_NumOfDrugLinesRecs].Duration;
						}

						// Subtract any previously-sold stuff.
						// DonR 08Jul2015: Use the package size of the originally-prescribed drug
						// to calculate the units previously sold.
						UnitsToSell =			UnitsPrescribed
										-	((	PR_PrevSoldOP    [v_NumOfDrugLinesRecs] * DP_package_size	)
											 +	PR_PrevSoldUnits [v_NumOfDrugLinesRecs]						 );

						// DonR 07Jul2015: If we've already sold more than the original prescription amount
						// (which happens with chronic prescriptions), UnitsToSell will be negative. In this
						// case, force it to zero.
						if (UnitsToSell < 0)
							UnitsToSell = 0;

						// Divide UnitsToSell into OP and remainder.
						OPToSell	= UnitsToSell / DL_package_size;
						UnitsToSell	= UnitsToSell % DL_package_size;

						// Compute Open-package Threshold.
						fOpenPkgUnits = OpenPkgPercent * DL_package_size;
						iOpenPkgUnits = fOpenPkgUnits;

						// If UnitsToSell is above the threshold, add one
						// OP (and set Units to zero) instead of opening
						// another package. (Note: check if this should be
						// tested as GTE or just GT.)
						// Note that as the open-package threshold is currently set to 100%, this feature is
						// effectively disabled.
						if (UnitsToSell >= iOpenPkgUnits)
						{
							OPToSell++;
							UnitsToSell = 0;
						}

						// Finally, store the results!
						SPres[v_NumOfDrugLinesRecs].Units	= UnitsToSell;
						SPres[v_NumOfDrugLinesRecs].Op		= OPToSell;

					}		// Package is openable.


					else	// Unopenable pkg. or OTC drug.
					{
						// Do nothing special.
						// DonR 17Jun2015: THIS IS A BUG! Even if the package is not openable, we need
						// to subtract previously-bought packages!
						// Instead of "doing nothing special", let's do something a *little* special: figure
						// out how much to sell!
						if ((PR_Status		[v_NumOfDrugLinesRecs] == 1)	&&	// Prescription was already partially sold...
							(PR_PrevSoldOP	[v_NumOfDrugLinesRecs] >  0))		// ...and we do have OP already sold.
						{
							SPres [v_NumOfDrugLinesRecs].Op -= PR_PrevSoldOP [v_NumOfDrugLinesRecs];

							// If we somehow sold more than was prescribed, send zero instead of a negative number.
							if (SPres [v_NumOfDrugLinesRecs].Op < 0)
								SPres [v_NumOfDrugLinesRecs].Op = 0;
						}
					}


					// DonR 11Sep2012: Add a warning message if:
					// (A)	We haven't already given the warning;
					// (B)	This is a single-prescription-ID request;
					// (C)	The Doctor Prescription Date is farther into the future than allowed by
					//		the sysparams parameter sale_ok_early_days;
					// (D)	There is at least one non-chronic drug in the prescription (after
					//		generic-drug substitution).
					// Note that if (B) is false, MaxFutureDate will have its default value of 29999999 -
					// so we can combine (B) and (C) in a single clause of the "if".
					// DonR 16Sep2012: Don't look at Chronic Flag; Chronic Months must be >= 3 for a
					// drug to be considered chronic.
					// DonR 19Sep2012: Per Iris Shaya, disable this message - but keep the logic in
					// place, in case we decide to reinstate it.
					if ((GaveFutureDrugWarning			== 0)				&&
						(PR_Date [v_NumOfDrugLinesRecs]	>  MaxFutureDate)	&&
						(DL_chronic_months				<  3))
					{
						GaveFutureDrugWarning = 1;	// So we don't bother checking the conditions again.
					}


					v_NumOfDrugLinesRecs++;

					if (v_NumOfDrugLinesRecs >= MAX_REC_ELECTRONIC)
					{
						// DonR 20Jul2006: Per Iris Shaya, if we've got more than 99
						// prescriptions to send, don't return an error; just send
						// the first 99 prescriptions and don't make a big deal out of it.
						GoodFetch = 0;
						continue;
					}
				}	// Successful read.

				else
				{
					if (SQLERR_code_cmp (SQLERR_end_of_fetch) == MAC_TRUE)
					{
						continue;	// Jumps to "while" and exits loop.
					}

					else
					{
						if (SQLERR_error_test ())
						{
							SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR);
							break;
						}
					}
				}	// Read failed.

			}	// "Do" block.
			while (GoodFetch);
			// End of loop for reading doctor_presc rows.

		}	// Nothing fatal in Doctor, Member, Pharmacy data.


		CloseCursor (	MAIN_DB, TR2001_Rx_cursor	);

		if (SQLERR_error_test ())
		{
			SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR);
			break;
		}


		// Retry if we hit a DB problem.
		if (reStart != MAC_FALS)	// DB errors - do a retry.
		{
			// DonR 26Jul2011: Suppress log message until the fourth attempt fails.
			if (tries > 2)
				GerrLogReturn (GerrId, "Table is locked for the <%d> time", tries);
			
			sleep (ACCESS_CONFLICT_SLEEP_TIME);
		}	// DB error occurred.

	}	// End of Database Retries loop.
	
	
	// See if we've exceeded the maximum retry count.
	if (reStart == MAC_TRUE)
	{
		GerrLogReturn (GerrId, "Locked for <%d> times", SQL_UPDATE_RETRIES);
		SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR);
	}


	// Prepare and send Response Message (Transaction 2002).
	WinHebToDosHeb ((unsigned char *)v_MemberFamilyName);
	WinHebToDosHeb ((unsigned char *)v_MemberFirstName);
	WinHebToDosHeb ((unsigned char *)v_DoctorFamilyName);
	WinHebToDosHeb ((unsigned char *)v_DoctorFirstName);

	nOut =  sprintf (OBuffer,		 "%0*d",  MSG_ID_LEN,				2002					);
	nOut += sprintf (OBuffer + nOut, "%0*d",  MSG_ERROR_CODE_LEN,		MAC_OK					);
	nOut += sprintf (OBuffer + nOut, "%0*d",  TPharmNum_len,			v_PharmNum				);
	nOut += sprintf (OBuffer + nOut, "%0*d",  TInstituteCode_len,		v_InstituteCode			);
	nOut += sprintf (OBuffer + nOut, "%0*d",  TTerminalNum_len,			v_TerminalNum			);
	nOut += sprintf (OBuffer + nOut, "%0*d",  TErrorCode_len,			v_ErrorCode				);
	nOut += sprintf	(OBuffer + nOut, "%0*d",  TGenericTZ_len,			v_MemberIdentification	);
	nOut += sprintf (OBuffer + nOut, "%0*d",  TIdentificationCode_len,	v_IdentificationCode	);

	nOut += sprintf (OBuffer + nOut, "%-*.*s", TMemberFamilyName_len,
											   TMemberFamilyName_len,	v_MemberFamilyName		);
    nOut += sprintf (OBuffer + nOut, "%-*.*s", TMemberFirstName_len,
											   TMemberFirstName_len,	v_MemberFirstName		);

	nOut += sprintf (OBuffer + nOut, "%0*d",   TMemberDateOfBirth_len,	Member.date_of_bearth	);

	nOut += sprintf (OBuffer + nOut, "%-*.*s", TDoctorFamilyName_len,
											   TDoctorFamilyName_len,	v_DoctorFamilyName		);
	nOut += sprintf (OBuffer + nOut, "%-*.*s", TDoctorFirstName_len,
											   TDoctorFirstName_len,	v_DoctorFirstName		);

	nOut += sprintf (OBuffer + nOut, "%0*d",  2,						v_NumOfDrugLinesRecs	);


	// Repeating portion of message.
	for (i = 0; i < v_NumOfDrugLinesRecs; i++)
	{
		nOut += sprintf (OBuffer + nOut, "%0*d", 2,					(i + 1)						);	// Line Number.
		nOut += sprintf (OBuffer + nOut, "%0*d", TDoctorRecipeNum_len,	PR_DoctorPrID[i]			);
		nOut += sprintf (OBuffer + nOut, "%0*d", TDate_len,			PR_Date[i]					);
		nOut += sprintf (OBuffer + nOut, "%0*d", TDrugCode_len,		PR_Original_Largo_Code[i]	);
		nOut += sprintf (OBuffer + nOut, "%0*d", TDrugCode_len,		SPres[i].DrugCode			);
		nOut += sprintf (OBuffer + nOut, "%0*d", 1,					PR_Status[i]				);
		nOut += sprintf (OBuffer + nOut, "%0*d", TUnits_len,			SPres[i].Units				);
		nOut += sprintf (OBuffer + nOut, "%0*d", TOp_len,				SPres[i].Op					);
		nOut += sprintf (OBuffer + nOut, "%0*d", TUnits_len,			PR_NumPerDose[i]			);
		nOut += sprintf (OBuffer + nOut, "%0*d", TUnits_len,			PR_DosesPerDay[i]			);
		nOut += sprintf (OBuffer + nOut, "%0*d", TDuration_len,		SPres[i].Duration			);
	}


	// Return the size in Bytes of response message.
	*p_outputWritten			= nOut;
	*output_type_flg			= ANSWER_IN_BUFFER;
	ssmd_data_ptr->error_code	= v_ErrorCode;
	ssmd_data_ptr->pharmacy_num	= v_PharmNum;
	ssmd_data_ptr->terminal_num	= v_TerminalNum;

	RESTORE_ISOLATION;

	return  RC_SUCCESS;
}	// End of 2001 handler.



/*=======================================================================
||																		||
||			 HandlerToMsg_2003											||
||	Message handler for message 2003:									||
||        Electronic Prescription Drugs Sale Request.					||
||																		||
 =======================================================================*/

int HandlerToMsg_2003 (char			*IBuffer,
					   int			TotalInputLen,
					   char			*OBuffer,
					   int			*p_outputWritten,
					   int			*output_type_flg,
					   SSMD_DATA	*ssmd_data_ptr)
{
	// Local declarations.
	int						TrnID = 2003;
	int						tries;
	int						retries;
	int						reStart;
	int						i;
	int						j;
	int						ingred;
	int						is;
	int						flg;
	int						UnitsSold;
	int						ErrorCodeToAssign		= 0;
	int						MaxFutureDate;	// DonR 11Sep2012.
	int						HoldLargo;
	int						DrugStartDate;
	int						HoldSpecialistDrug;
	int						HoldParentGroupCode;
	int						FullPriceTotal			= 0;	// DonR 12Mar2012
	double					OverseasWorkVar;
	short					HoldRuleStatus;
	short					TypeCheckSpecPr;
	short					v_Message;
	short					DispenseAsWritten;
	short					MajorDrugIndex [MAX_REC_ELECTRONIC];
	short					ThisIshurHasTikra		= 0;	// DonR 28Mar2007: Clicks 10.1
	short					AnIshurHasTikra			= 0;	// DonR 28Mar2007: Clicks 10.1
	//short					MemberTzahal;
	short					Agorot;							// DonR 16Dec2013;
	short					DefaultSourceReject		= 0;	// DonR 27Dec2017 HOT FIX.
	TErrorCode				err;
	TErrorCode				FunctionError;
	TParticipatingPrecent	percent1;
	TParticipatingPrecent	percent2;
	TParticipatingPrecent	percents;
	ISOLATION_VAR;

	// Package-opening variables.
	float					OpenPkgPercent = OPEN_PACKAGE_BELOW;

	// Return variables for AS400 Gadget participation check routine.
	int						i_dummy;
	int						l_dummy1;
	int						l_dummy2;
	int						UnitPrice;
	int						QuantityPermitted;
	int						RequestNum;
	int						PtnPrice;
	int						Insurance;


	// SQL variables.

	// Arrays to hold drugs prescribed data.
	TPresDrugs			SPres					[MAX_REC_ELECTRONIC];
	TDocRx				DocRx					[MAX_REC_ELECTRONIC];
	short int			PR_WhyNonPreferredSold	[MAX_REC_ELECTRONIC];
	short int			PR_NumPerDose			[MAX_REC_ELECTRONIC];
	short int			PR_DosesPerDay			[MAX_REC_ELECTRONIC];
	short int			PR_LineNumber			[MAX_REC_ELECTRONIC];
	short int			PR_Status				[MAX_REC_ELECTRONIC];
	short int			PR_ExtendAS400IshurDays	[MAX_REC_ELECTRONIC];
	short int			preferred_mbr_prc_code	[MAX_REC_ELECTRONIC];
	short int			gadget_svc_number		[MAX_REC_ELECTRONIC];
	short int			gadget_svc_type			[MAX_REC_ELECTRONIC];
	short int			PR_AsIf_Preferred_SpDrug[MAX_REC_ELECTRONIC];
	short int			PR_AsIf_Preferred_Basket[MAX_REC_ELECTRONIC];
	short int			PR_AsIf_RuleStatus		[MAX_REC_ELECTRONIC];
	int					gadget_svc_code			[MAX_REC_ELECTRONIC];
	int					PR_Date					[MAX_REC_ELECTRONIC];
	int					PR_Original_Largo_Code	[MAX_REC_ELECTRONIC];
	int					PR_DoctorPrID			[MAX_REC_ELECTRONIC];
	int					PR_AsIf_Preferred_Largo	[MAX_REC_ELECTRONIC];
	int					PR_AsIf_ParentGroupCode	[MAX_REC_ELECTRONIC];
	int					PharmIshurProfession	[MAX_REC_ELECTRONIC];

	// INPUT MESSAGE
	PHARMACY_INFO				Phrm_info;
	int							v_PharmNum;
	short						v_InstituteCode;
	short						v_TerminalNum;
	int							v_MemberIdentification;
	short						v_IdentificationCode;
	short						v_IdentificationCode_s;
	short						v_MemberBelongCode;
	short					 	v_MoveCard;
	short						v_TypeDoctorIdentification;
	int							DocID;
	short						v_RecipeSource;
	int							v_DoctorDeviceCode;
	int							v_DoctorLocationCodeMinus990000;
	int							v_ClientLocationCode;
	short						v_NumOfDrugLinesRecs;
	short						v_MsgExistsFlg;
	short						v_MsgIsUrgentFlg;

	// Variables for Generic Drug Substitution.
	int							EP_LargoCode;

	// Variables for Purchase Limit checking.
	int							PL_IshurNum = 0;
	int							PL_Units;
	short int					PL_Months;
	short int					PL_Type;
	int							PL_OpenDate;
	int							PL_CloseDate;
	int							PL_CloseDate_Min;

	int							AlreadyBoughtToday;

	// Pharmacy Special Confirmation. The source isn't transmitted in 2003.
	int							v_SpecialConfNum;
	short						v_SpecialConfSource;

	short int					v_card_date;
	short int					v_authorizealways;	/* from MEMBERS  *//*20001022*/

	// Pharmacy data.
	short int					v_pharm_card		= 0;
	short int					v_leumi_permission	= 0;
	short int					v_CreditPharm		= 0;
	short int					maccabicare_pharm	= 0;
	short int					vat_exempt			= 0;

	// Maccabi Centers
	short int					v_MacCent_cred_flg	= 0;

	// Individual Drugs prescribed
	TPresDrugs					*PrsLinP;
	TDrugCode					v_DrugCode;
	TUnits						v_Units;

	// RETURN MESSAGE (note - go over these fields!)
	// header
	int							v_FamilyHeadTZ;
	short						v_FamilyHeadTZCode;
	int							v_FamilyCreditUsed;
	int							v_CreditToBeUsed;
	int							v_CreditThisDrug;
	TSqlInd						v_FamilyCreditInd;
	int							v_FirstOfMonth;
	TRecipeIdentifier			v_RecipeIdentifier;
	TMemberFamilyName			v_MemberFamilyName;
	TMemberFirstName			v_MemberFirstName;
	TMemberSex					v_MemberGender;
	TDoctorFamilyName			v_DoctorFamilyName;
	TDoctorFirstName			v_DoctorFirstName;
	TConfiramtionDate			v_ConfirmationDate;
	TErrorCode					v_ErrorCode;
	TMemberDiscountPercent		v_MemberDiscountPercent;
	TConfirmationHour			v_ConfirmationHour;
	TCreditYesNo				v_CreditYesNo;
	short int					v_insurance_status; 
	short int					v_MemberSpecPresc;
	int							illness_bitmap;
	int							v_MaxLargoVisible;

	// Drug lines
	char						PW_PrescriptionType [2];
	short int					PW_NumPerDose;
	short int					PW_DosesPerDay;
	short int					PW_status_dummy;
	short int					PW_Origin_Code;
	short int					PW_SubstPermitted;
	int							PW_Pr_Date;
	int							PW_Doc_Largo_Code;
	int							PW_DoctorPrID;
	short int					PW_status	= 0;	// DonR 29Dec2009: Added initialization.
	int							PW_ID		= 0;	// DonR 29Dec2009: Added initialization.
	int							PW_AuthDate	= 0;	// DonR 24Jul2013.
	double						PW_RealAmtPerDose;
	int							UnitsToSell;
	int							OPToSell;

	// Gadgets table.
	int							v_service_code;
	int							v_gadget_code;
	short int					v_service_number;
	short int					v_service_type;
	short						v_enabled_status;
	short						v_enabled_mac;
	short						v_enabled_hova;
	short						v_enabled_keva;
	short						enabled_pvt_pharm;

	// MemberPharm table - member/pharmacy purchase restrictions
	short int					v_MemPharm_Exists;
	int							v_MemPharm_PhCode;
	short int					v_MemPharm_PhType;
	int							v_MemPharm_FromDt;
	int							v_MemPharm_ToDt;
	int							v_MemPharm_PhCode2;
	short int					v_MemPharm_PhType2;
	int							v_MemPharm_FromDt2;
	int							v_MemPharm_ToDt2;
	short int					v_MemPharm_ResType;
	short						v_MemPharm_PermittedOwner;
	short int					MemPharm_Result;

	// General SQL variables.
	TSqlInd						v_DiscInd;
	TSqlInd						v_CredInd;
	char						PossibleInsuranceType[2];
	char						first_center_type [5];
	int							max_drug_date;
	int							SysDate;
	int							StopUseDate;
	int							StopBloodDate;
	int							MaxRefillDate;
	int							ShortOverlapDate;	// DonR 19Apr2010 for new early-refill warning.
	int							LongOverlapDate;	// DonR 19Apr2010 for new early-refill warning.
	int							Yarpa_Price;
	int							Macabi_Price;
	int							Supplier_Price;	// 21NOV2002 Fee Per Service enh.
	TPriceListNum				v_PriceListCode;
	short int					LineNum;
	TGenericTZ			      	v_TempMembIdentification;
	TIdentificationCode			v_TempIdentifCode;
	TDate						v_TempMemValidUntil;
	TParticipatingType			RetPartCode;
	int							TaxRetPart;
	int							TaxPart;
	int							EarliestPrescrDate;
	int							LatestPrescrDate;
	int							RowsFound;
	int							high_rules_found;
	int							low_rules_found;
	TRecipeIdentifier			v_prw_id;
	short						SomeRuleApplies			= 0;	// DonR 22Mar2011
	short						v_Severity;
	char						InsuranceTypeToCheck	[ 1 + 1];
	char						DummyPhone				[10 + 1];
	short						DummyCheckInteractions;
	int							DummyInt;
	int							DummyRealDoctorTZ;


	// Body of function
 
	// Initialize variables.
	REMEMBER_ISOLATION;
	PosInBuff				= IBuffer;
	v_ErrorCode				= NO_ERROR;
	SysDate					= GetDate ();
	v_ConfirmationHour		= GetTime ();
	v_MsgExistsFlg			= 0;
	v_MsgIsUrgentFlg		= 0;
	v_NumOfDrugLinesRecs	= 0;
	EarliestPrescrDate		= AddDays (SysDate, (0 - PRESCR_VALID_DAYS));
	LatestPrescrDate		= AddDays (SysDate, SELECT_PRESC_EARLY_DAYS);
	MaxFutureDate			= AddDays (SysDate, SaleOK_EarlyDays);
	v_DrugsBuf.max			= 0;	// DonR 16Feb2006: Initialize global buffer.

	// Vacation ishurim should be ignored for the last five days - so the member
	// can buy more drugs when s/he gets back from his/her trip.
	PL_CloseDate_Min = AddDays (SysDate, 5);

	memset ((char *)SPres,		0, sizeof(SPres));
	memset ((char *)DocRx,		0, sizeof(DocRx));


	// Read message fields data into variables.
	v_PharmNum					= GetLong	(&PosInBuff, TPharmNum_len					); CHECK_ERROR ();
	v_InstituteCode				= GetShort	(&PosInBuff, TInstituteCode_len				); CHECK_ERROR ();
	v_TerminalNum				= GetShort	(&PosInBuff, TTerminalNum_len				); CHECK_ERROR ();
	v_MemberIdentification		= GetLong	(&PosInBuff, TGenericTZ_len					); CHECK_ERROR ();
	v_IdentificationCode		= GetShort	(&PosInBuff, TIdentificationCode_len		); CHECK_ERROR ();
	v_MemberBelongCode			= GetShort	(&PosInBuff, TMemberBelongCode_len			); CHECK_ERROR ();
	v_MoveCard					= GetShort	(&PosInBuff, TMoveCard_len					); CHECK_ERROR ();
	v_TypeDoctorIdentification	= GetShort	(&PosInBuff, TTypeDoctorIdentification_len	); CHECK_ERROR ();
	DocID						= GetLong	(&PosInBuff, TGenericTZ_len					); CHECK_ERROR ();
	v_RecipeSource				= GetShort	(&PosInBuff, TRecipeSource_len				); CHECK_ERROR ();
	v_ClientLocationCode		= GetLong	(&PosInBuff, TDeviceCode_len				); CHECK_ERROR ();
	v_DoctorDeviceCode			= GetLong	(&PosInBuff, TDeviceCode_len				); CHECK_ERROR ();
	v_SpecialConfSource			= GetShort	(&PosInBuff, TSpecConfNumSrc_len			); CHECK_ERROR ();
	v_SpecialConfNum			= GetLong	(&PosInBuff, TNumofSpecialConfirmation_len	); CHECK_ERROR ();
	v_NumOfDrugLinesRecs		= GetShort	(&PosInBuff, TElectPR_LineNumLen			); CHECK_ERROR ();


	ssmd_data_ptr->pharmacy_num		= v_PharmNum;
	ssmd_data_ptr->member_id		= v_MemberIdentification;
	ssmd_data_ptr->member_id_ext	= v_IdentificationCode_s = v_IdentificationCode;

	
	// Validate Number of Drugs Prescribed.
	if (v_NumOfDrugLinesRecs < 1)
	{
		SET_ERROR (ERR_FILE_TOO_SHORT);
		RESTORE_ISOLATION;
		return (ERR_FILE_TOO_SHORT);
	}

	if (v_NumOfDrugLinesRecs > MAX_REC_ELECTRONIC)
	{
		SET_ERROR (ERR_FILE_TOO_LONG);
		RESTORE_ISOLATION;
		return (ERR_FILE_TOO_LONG);
	}


	// Get repeating portion of message.
	for (i = 0; i < v_NumOfDrugLinesRecs; i++)
	{
		PR_LineNumber			[i]	= GetShort	(&PosInBuff, TElectPR_LineNumLen	); CHECK_ERROR ();
		PR_DoctorPrID			[i]	= GetLong	(&PosInBuff, TDoctorRecipeNum_len	); CHECK_ERROR ();
		PR_Date					[i]	= GetLong	(&PosInBuff, TDate_len				); CHECK_ERROR ();
		PR_Original_Largo_Code	[i]	= GetLong	(&PosInBuff, TDrugCode_len			); CHECK_ERROR ();
		PR_WhyNonPreferredSold	[i]	= GetShort	(&PosInBuff, TElectPR_SubstPermLen	); CHECK_ERROR ();

		// Drug actually dispensed.
		SPres[i].DrugCode			= GetLong	(&PosInBuff, TDrugCode_len			); CHECK_ERROR ();

		// For prescription lines reported in Trn. 2002, the next two fields should
		// be the same values that were in 2002. For new prescription lines, units
		// should normally be zero, and OP should be based on what the doctor prescribed -
		// zero is OK for openable medications, but for non-openable medicines the
		// doctor-supplied OP should be entered. For any prescription lines that were not
		// in the database already, but somehow had already been partially sold, logic MAY
		// be added to compute the previously-sold amounts, based on the difference between
		// the reported Units/OP to sell and the values computed based upon dosage/duration
		// numbers.
		SPres[i].Units				= GetLong	(&PosInBuff, TUnits_len				); CHECK_ERROR ();
		SPres[i].Op					= GetLong	(&PosInBuff, TOp_len				); CHECK_ERROR ();
		PR_NumPerDose [i]			= GetShort	(&PosInBuff, TUnits_len				); CHECK_ERROR ();
		PR_DosesPerDay [i]			= GetShort	(&PosInBuff, TUnits_len				); CHECK_ERROR ();
		SPres[i].Duration			= GetShort	(&PosInBuff, TDuration_len			); CHECK_ERROR ();
		SPres[i].LinkDrugToAddition	= GetLong	(&PosInBuff, TLinkDrugToAddition_len); CHECK_ERROR ();

		// DonR 12Jul2010: Overdose checking is now done based on what the doctor prescribed, NOT
		// on the (possibly lower) amount being sold. Since only Trn. 5003 provides separate OP/Units
		// for the original prescription, here we have to copy the values into the new variables.
		SPres[i].Doctor_Units	= SPres[i].Units;	// DonR 12Jul2010.
		SPres[i].Doctor_Op		= SPres[i].Op;		// DonR 12Jul2010.

		// DonR 13Dec2016: Copy Prescription Source into sPres array - it's used in Quantity Limit checking now.
		// DonR 26Dec2016: Also copy Doctor ID and Doc ID Type, for interaction checking.
		SPres[i].PrescSource	= v_RecipeSource;
		SPres[i].DocIDType		= v_TypeDoctorIdentification;
		SPres[i].DocID			= DocID;

		// DonR 27Dec2016: Load up the new DocRx array with some doctor-prescription data, just for
		// compatibility with routines like CheckForInteractionIshur().
		if (PR_DoctorPrID [i] > 0)
		{
			SPres [i].NumDocRxes	= 1;
			SPres [i].FirstRx		= i;
			SPres [i].FirstDocPrID	= PR_DoctorPrID [i];	// DonR 27Mar2017 - for doc-saw-specialist-letter lookup.
			DocRx [i].PrID			= PR_DoctorPrID [i];
			DocRx [i].FromDate		= PR_Date		[i];
			DocRx [i].Units			= SPres [i].Units;
			DocRx [i].OP			= SPres [i].Op;
		}
		else
		{
			SPres [i].NumDocRxes = SPres [i].FirstRx = SPres [i].FirstDocPrID = 0;
		}
	}

	// Get amount of input not eaten.
	ChckInputLen (TotalInputLen - (PosInBuff - IBuffer));
	CHECK_ERROR ();


	// SQL Transaction Retry Loop.
	reStart = MAC_TRUE;

	for (tries = 0; tries < SQL_UPDATE_RETRIES && reStart == MAC_TRUE; tries++)
	{
		// Initialize answer variables.
		v_ErrorCode				= NO_ERROR;
		reStart					= MAC_FALS;
		v_RecipeIdentifier		= 0;
		v_MemberGender			= 0;
		v_MemberDiscountPercent	= 0;
		v_CreditYesNo			= 0;
		v_PriceListCode 		= 0;
		TypeCheckSpecPr			= 0;
		v_MemberFamilyName	[0]	= 0;
		v_MemberFirstName	[0]	= 0;
		v_DoctorFamilyName	[0]	= 0;
		v_DoctorFirstName	[0]	= 0;
		v_ConfirmationDate		= GetDate ();
		SysDate					= GetDate ();

		// Initialize drug-level arrays, including those relating to Pharmacy Ishur.
		for (i = 0; i < v_NumOfDrugLinesRecs; i++)
		{
		    SPres[i].DL.package_size				= 1;	// Set default to avoid division by zero.
			SPres[i].ret_part_source.insurance_used	= BASIC_INS_USED;
			PR_AsIf_Preferred_Largo	[i]				= 0;
			PR_AsIf_Preferred_SpDrug[i]				= 0;
			PR_AsIf_Preferred_Basket[i]				= 0;
			PR_AsIf_ParentGroupCode	[i]				= 0;
			PR_AsIf_RuleStatus		[i]				= 0;
			preferred_mbr_prc_code	[i]				= 0;
			MajorDrugIndex			[i]				= -1;	// Default - no major drug found.
															// (Zero is a valid index value.)
			gadget_svc_number		[i]				= 0;
			gadget_svc_type			[i]				= 0;
			gadget_svc_code			[i]				= 0;
			PR_ExtendAS400IshurDays	[i]				= 0;
			PharmIshurProfession	[i]				= 0;
		}

		// If pharmacy supplied a Pharmacy Ishur Number, check the sale request
		// against the Ishur. Initialize the PR_ExtendAS400IshurDays array
		// first, since check_sale_against_pharm_ishur() stores values there.
		err = check_sale_against_pharm_ishur (v_PharmNum,
											  v_MemberIdentification,
											  v_IdentificationCode,
											  SPres,
											  v_NumOfDrugLinesRecs,
											  v_SpecialConfNum,
											  v_SpecialConfSource,
											  PR_ExtendAS400IshurDays,
											  PharmIshurProfession,
											  &FunctionError);

	    SetErrorVar (&v_ErrorCode, FunctionError);

		if (err == 2)	// DB conflict.
		{
			reStart = MAC_TRUE;
		}


		// Get prescription id (for Prescriptions/Prescription Drugs tables).
		err = GET_PRESCRIPTION_ID (&v_RecipeIdentifier);

		if (err != NO_ERROR)
		{
		    SetErrorVar (&v_ErrorCode, err);

		    GerrLogReturn (GerrId, "Can't get PRESCRIPTION_ID error %d", err);
			break;	// Exit and send reply.
			// Note - will this leave us stuck inside a transaction? WORKINGPOINT
		}


		// DonR 27Apr2005: Avoid unnecessary lock errors by downgrading "isolation"
		// for this section.
		SET_ISOLATION_DIRTY;

		// DUMMY LOOP #1 TO PREVENT goto.
		do	// Exiting from LOOP jumps to the drug-handling stuff.
		{
			// Test member belong code. Assume no error to begin.
		    err = MAC_FALS;

		    switch (v_MemberBelongCode)
			{
				case MACABI_INSTITUTE:
				    break;

				default:
					// DonR 05Jun2005: To support MaccabiCare feature, permit non-Maccabi
					// members to purchase non-prescription drugs.
					if (v_RecipeSource != RECIP_SRC_NO_PRESC)
					{
						SetErrorVarF (&v_ErrorCode, ERR_WORNG_MEMBERSHIP_CODE, &err);
					}
				    break;
			}
		    if (err == MAC_TRUE)
				break;


			// Test pharmacy data.
		    err = IS_PHARMACY_OPEN (v_PharmNum, v_InstituteCode, &Phrm_info);

			// DonR 08Sep2009: Copy pharmacy data already read by IS_PHARMACY_OPEN() into local variables.
		    v_PriceListCode		= Phrm_info.price_list_num;
			v_pharm_card		= Phrm_info.pharm_card;
			v_leumi_permission	= Phrm_info.leumi_permission;
			v_CreditPharm		= Phrm_info.CreditPharm;
			maccabicare_pharm	= Phrm_info.maccabicare_pharm;
			vat_exempt			= Phrm_info.vat_exempt;

			// DonR 31Jul2013: Move error test to after we've loaded pharmacy values, since we still do things
			// like price-list lookups based on them even if the pharmacy hasn't done an open-shift.
			if (err != MAC_OK)
		    {
				SetErrorVar (&v_ErrorCode, err);
				break;
			}


			// DonR 19Feb2008: Drugs with Largo Code >= 90000 are visible only
			// to Maccabi pharmacies.
			v_MaxLargoVisible = (MACCABI_PHARMACY) ? NO_MAX_LARGO : 89999;

			// Test prescription source.
			// DonR 23Feb2008: Added new Prescription Source 11, for "Home Visit" prescriptions.
			// DonR 24Sep2009: Added new Prescription Source 13, for "Affiliated Clinic" prescriptions.
		    err = MAC_FALS;

		    switch (v_RecipeSource)
		    {
				case RECIP_SRC_OLD_PEOPLE_HOUSE:	// Only maccabi pharmacies can sell these drugs.
										    if (PRIVATE_PHARMACY)
											{
												SetErrorVarF (&v_ErrorCode, ERR_PHARM_CANT_SALE_PRECR_SRC, &err);
										    }
											break;

				case RECIP_SRC_NO_PRESC:	// DonR 05Jun2005: Private pharmacies with MaccabiCare
											// flag non-zero *can* sell non-prescription drugs.
										    if ((PRIVATE_PHARMACY) && (maccabicare_pharm == 0))
											{
												SetErrorVarF (&v_ErrorCode, ERR_PHARM_CANT_SALE_PRECR_SRC, &err);
										    }
											break;

				default:	// Any other prescription source is erroneous or unsupported.
							// DonR 29Aug2016, CR #8956: Before rejecting the transaction, check to see if
							// the prescription source is present in the prescr_source table; this allows
							// new codes to be added without needing a new program version.
							if (!is_valid_presc_source (v_RecipeSource, PRIVATE_PHARMACY, &ErrorCodeToAssign, &DefaultSourceReject))
							{
								SetErrorVar (&v_ErrorCode, ErrorCodeToAssign);
								err = MAC_TRUE;
							}
							break;
			}

			if (err == MAC_TRUE)
				break;


			// Read and test Member information.
			// DonR 05Jun2005: For MaccabiCare, allow T.Z. of zero for non-Maccabi members.
		    if ((v_MemberIdentification == 0) && (v_MemberBelongCode == MACABI_INSTITUTE))
			{
				SetErrorVar (&v_ErrorCode, ERR_MEMBER_ID_CODE_WRONG);
				break;
			}

			// Test temporary members.
		    if (v_IdentificationCode == 7)
		    {
				ExecSQL (	MAIN_DB, READ_tempmemb,
							&v_TempIdentifCode,			&v_TempMembIdentification,	&v_TempMemValidUntil,
							&v_MemberIdentification,	END_OF_ARG_LIST										);

				Conflict_Test (reStart);

				if (SQLERR_code_cmp (SQLERR_not_found) == MAC_TRUE)
				{
					SetErrorVar (&v_ErrorCode, ERR_MEMBER_ID_CODE_WRONG);
					err = MAC_TRUE;
				}
				else	// DonR 20Feb2005: Added "else" so we don't treat not-found
						// as a critical error.
				if (SQLERR_error_test ())
				{
					SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR);
					err = MAC_TRUE;
				}
				else	// DonR 20Feb2005: Added "else" so we use the values from
						// tempmemb only if row was actually read from the database.
				if (v_TempMemValidUntil >= SysDate)
				{
				    v_IdentificationCode   = v_TempIdentifCode;
				    v_MemberIdentification = v_TempMembIdentification;
				}
			}	// Temporary Member test.


			// Test member ID-CODE.
			// DonR 05Jun2005: For MaccabiCare, allow T.Z. of zero for non-Maccabi members.
			// Since we test for zero above, here we need test only for T.Z.'s between
			// 1 and 57 - which are invalid for anyone, Maccabi member or not.
		    if ((v_MemberIdentification > 0) & (v_MemberIdentification < 58))
		    {
				SetErrorVar (&v_ErrorCode, ERR_MEMBER_ID_CODE_WRONG);
				err = MAC_TRUE;
			}

			// DonR 05Jun2005: If this is a case where Member ID can be zero, allow
			// Doctor ID to be zero as well - since we're dealing with non-prescription drugs.
			// DonR 10Jul2005: Just in case some non-Maccabi doctor has a license number
			// equal to the Teudat Zehut number of some patient, ignore that equality (see
			// third clause of the "if").
			if ((DocID == v_MemberIdentification)	&&
				(v_MemberIdentification > 0)		&&
				(v_TypeDoctorIdentification != 1))
			{
				SetErrorVar (&v_ErrorCode, ERR_DOCTOR_EQ_MEMBER);
				err = MAC_TRUE;
			}


			// DonR 30Jun2005: If Member ID = 0, plug in appropriate values for a
			// non-Maccabi member.
			// DonR 05Sep2006: Moved this code up, so that we avoid looking up
			// non-existent data in the Members table.
			if (v_MemberIdentification == 0)
			{
				strcpy (v_MemberFamilyName,	"");
				strcpy (v_MemberFirstName,	"");
				v_CreditYesNo		= 0;
				v_MemberSpecPresc	= 0;	// DonR 24Jul2011: Save a meaningless special_prescs lookup for ID zero.
				Member.maccabi_until = Member.mac_magen_until = Member.keren_mac_until = Member.yahalom_until = 0;
				Member.date_of_bearth = SysDate;	// For lack of a more sensible default.
				strcpy (Member.insurance_type, "B");	// For lack of a more sensible default - or should we use "N"?

				// DonR 27Nov2011: For purposes of reading drug_extension and similar tables, set flags
				// for this non-member to enable reading of Maccabi rules but not IDF rules.
				Member.MemberMaccabi = 1;
				Member.MemberTzahal = Member.MemberHova = Member.MemberKeva = 0;
			}

			else
			{	// Look up (hopefully) "real" member.
				// DonR 13May2020 CR #31591: Add new Member-on-Ventilator flag. This is
				// currently stored in the old column "asaf_code" (which was sent from
				// AS/400 but never used for anything); when we switch to MS-SQL, the
				// column should be renamed.
				ExecSQL (	MAIN_DB, TR2003_READ_members,
							&v_MemberFamilyName,			&v_MemberFirstName,
							&Member.date_of_bearth,			&v_CreditYesNo,
							&Member.maccabi_code,			&Member.maccabi_until,
							&Member.mac_magen_code,			&Member.mac_magen_from,
							&Member.mac_magen_until,		&Member.keren_mac_code,
							&Member.keren_mac_from,			&Member.keren_mac_until,
							&Member.keren_wait_flag,		&max_drug_date,
							&Member.payer_tz,				&Member.payer_tz_code,
							&v_MemberDiscountPercent,		&Member.insurance_type,
							&Member.carry_over_vetek,		&v_insurance_status,
							&Member.yahalom_code,			&Member.yahalom_from,
							&Member.yahalom_until,			&v_card_date,
							&v_authorizealways,				&v_FamilyHeadTZ,
							&v_FamilyHeadTZCode,			&v_MemberSpecPresc,
							&v_MemberGender,				&illness_bitmap,
							&Member.updated_by,				&Member.check_od_interact,
							&Member.VentilatorDiscount,		&Member.darkonai_type,
							&Member.force_100_percent_ptn,	&Member.darkonai_no_card,
							&Member.has_blocked_drugs,		&Member.died_in_hospital,
							&v_MemberIdentification,		&v_IdentificationCode,
							END_OF_ARG_LIST												);

				Conflict_Test (reStart);

				// DonR 04Dec2022 User Story #408077: Use a new function to set some Member flags and values.
				SetMemberFlags (SQLCODE);
			
				if (SQLERR_code_cmp (SQLERR_not_found) == MAC_TRUE)
				{
					SetErrorVar (&v_ErrorCode, ERR_MEMBER_ID_CODE_WRONG);
					err = MAC_TRUE;
				}
				else
				{
					if (SQLERR_error_test ())
					{
						SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR);
						err = MAC_TRUE;
					}
					else Member.RowExists = 1;

				}	// Something other than not-found: either success, or a DB problem.


				// See if this member has an all-drug vacation limit-multiplier ishur.
				ExecSQL (	MAIN_DB, READ_Largo_99997_ishur, Largo_99997_ishur_standard_WHERE,
							&Member.PL_99997_IshurNum,		&Member.PL_99997_OpenDate,
							&v_MemberIdentification,		&v_IdentificationCode,
							&SysDate,						&SysDate,
							END_OF_ARG_LIST															);

				if (SQLCODE == 0)
				{
					Member.Has_PL_99997_Ishur	= 1;
				}
				else
				{
					Member.Has_PL_99997_Ishur = Member.PL_99997_IshurNum = Member.PL_99997_OpenDate = 0;
				}


				// See if this member has a pharmacy purchase restriction.
				ExecSQL (	MAIN_DB, READ_MemberPharm,
							&v_MemPharm_PhCode,			&v_MemPharm_PhType,		&v_MemPharm_FromDt,
							&v_MemPharm_ToDt,			&v_MemPharm_PhCode2,	&v_MemPharm_PhType2,
							&v_MemPharm_FromDt2,		&v_MemPharm_ToDt2,		&v_MemPharm_ResType,
							&v_MemPharm_PermittedOwner,

							&v_MemberIdentification,	&v_IdentificationCode,	&ROW_NOT_DELETED,
							&SysDate,					&SysDate,				&SysDate,
							&SysDate,					END_OF_ARG_LIST									);

				v_MemPharm_Exists = (SQLERR_code_cmp (SQLERR_ok) == MAC_TRUE);

				// If member has a pharmacy purchase restriction, see how it
				// applies to this sale.
				//
				// ASSUMPTIONS:
				// 1) Restriction is still operative on its "to-date".
				// 2) If there is a match on Pharmacy Code, we don't have to look at
				//    the Pharmacy Type - assume that the people entering this data
				//    know what they're doing and there won't be inconsistency.
				// 3) There is no such thing as a blanket permission - that is, either
				//    there will be a specific Pharmacy Code OR a Pharmacy Type of 1
				//    (sale permitted at all Maccabi pharmacies).
				// 4) Member is allowed to buy non-prescription drugs anywhere.
				if (v_MemPharm_Exists)
				{
					// By default, assume that the sale is forbidden.
					MemPharm_Result = ERR_MEMBER_PHARM_SALE_FORBIDDEN;

					// Look at temporary fields first.
					if ((v_MemPharm_FromDt2 <= SysDate) && (v_MemPharm_ToDt2 >= SysDate))
					{
						if (((v_MemPharm_PhCode2 == v_PharmNum)	&& (v_PharmNum != 0)) ||
							((v_MemPharm_PhCode2 == 0) && (v_MemPharm_PhType2 == 1) && (MACCABI_PHARMACY)))
						{
							MemPharm_Result = ERR_MEMBER_PHARM_PERMITTED_TEMP;
						}
					}

					// Next look at "normal" restriction fields.
					if ((v_MemPharm_FromDt  <= SysDate) && (v_MemPharm_ToDt  >= SysDate))
					{
						if (((v_MemPharm_PhCode  == v_PharmNum)	&& (v_PharmNum != 0)) ||
							((v_MemPharm_PhCode  == 0) && (v_MemPharm_PhType  == 1) && (MACCABI_PHARMACY)))
						{
							MemPharm_Result = ERR_MEMBER_PHARM_PERMITTED;
						}
					}

					// If the restriction applies to all prescription sales, apply it
					// at the "header" level now.
					if ((v_MemPharm_ResType == 9) && (v_RecipeSource != RECIP_SRC_NO_PRESC))
					{
						SetErrorVarF (&v_ErrorCode, MemPharm_Result, &err);
					}
				}

			}	// Member ID is non-zero.


			// LOGICAL data tests.

			/* Member eligibility */
//			// DonR 25Nov2021 User Story #206812: Get died-in-hospital indicator from the Ishpuz system.
			// DonR 30Nov2022 User Story #408077: Use new macros defined in MsgHndlr.h to determine
			// eligibility.
			if ((MEMBER_INELIGIBLE) || (MEMBER_IN_MEUHEDET))
//				(Member.died_in_hospital)	||
			{
				// DonR 30Jun2005: If this is a MaccabiCare sale to a non-Maccabi member
				// (T.Z. = 0), don't worry about eligibility.
				if (v_MemberIdentification != 0)
				{
					SetErrorVar (&v_ErrorCode, ERR_MEMBER_NOT_ELEGEBLE);
					err = MAC_TRUE;
				}
			}

			// If card had a "real" date that was different from the card-issue
			// date in the database, set error.
			if ((v_IdentificationCode_s	!= 7						)	&&	// Don't check temp. members
				(v_MoveCard				>  0						)	&&	// 10Jan2005: 1 is a valid card date!
				(v_MoveCard				<  9998						)	&&	// Card was used. 07Mar2006: 9998 = Temp. Card!
				((v_MoveCard != 9997) || (Member.MemberTzahal == 0)	)	&&	// 10Oct2011: Special card for soldiers.
				(v_MoveCard				!= v_card_date				)	&&	// Date on card doesn't match DB version.
				(v_card_date			>  0						)	&&  // Yulia 20030213: DB date is "for real".
				(v_MemberIdentification	!= 59						)	&&	// Special case - exempt real members
				(v_MemberIdentification	!= 83						))		// whose ID's have been used for testing.
			{
				// DonR 30Jul2013: Because a lot of new Maccabi Sheli cards were sent out by mail - and members'
				// data was updated with the new card dates before those cards had arrived - a new table,
				// membercard, has been added to list additional/alternative valid card dates for any given
				// member. Accordingly, check this table before sending a "card expired" error.
				ExecSQL (	MAIN_DB, READ_MemberCard,
							&v_card_date,
							&v_MemberIdentification,	&v_IdentificationCode,
							&v_MoveCard,				END_OF_ARG_LIST				);

				if (SQLCODE != 0)
				{
					SetErrorVar (&v_ErrorCode, ERR_CARD_EXPIRED);
					err = MAC_TRUE;
				}
			}

			// leumit 20010101
			// If Member belongs to Leumit, and either (A) pharmacy isn't allowed to sell
			// Maccabi prescriptions to Leumit members, or (B) the prescription isn't from
			// a Maccabi doctor, reject the sale. If the pharmacy is permitted but the
			// prescription source is wrong, give a different error.
			// DonR 11Aug2008: Added new Maccabi Nurse prescription source.
			// DonR 04Apr2016: Per Iris Shaya, added new prescription source for Maccabi Dieticians.
			if (((!v_leumi_permission) ||
				((v_RecipeSource != RECIP_SRC_MACABI_DOCTOR)	&&
				 (v_RecipeSource != RECIP_SRC_MAC_DOC_BY_HAND)	&&
				 (v_RecipeSource != RECIP_SRC_MAC_DIETICIAN)	&&
				 (v_RecipeSource != RECIP_SRC_MACCABI_NURSE)))		&&
				(v_insurance_status == 2))
			{
				if (!v_leumi_permission)
					SetErrorVar (&v_ErrorCode, ERR_LEUMI_NOT_VALID);
				else
					SetErrorVar (&v_ErrorCode, ERR_PHARM_CANT_SALE_PRECR_SRC);

				err = MAC_TRUE;
			}
			// end leumit 20010101

			if ((v_pharm_card == 1) && (v_MemberIdentification != 0))	// only to pharmacy with flag ON
																		// DonR 30Jun2005: Doesn't apply
																		// to sales with T.Z. of zero.
			{
				if (v_IdentificationCode_s != 7) // 20010903 don't check tempmemb
				{
					if (v_card_date > 0) // member have card ,9999 read with ...
					{
						// If member didn't have card and is isn't set to "authorize always", set error. 
						if (!MEMBER_USED_MAG_CARD)
						{
							if (v_authorizealways)
							{
								// DonR 04Jun2015: If the Authorize Always flag was set because member requested it through
								// the Maccabi website or our mobile app, we don't want to zero it out - these requests expire
								// after a short time anyway. In fact, I'm not sure we should be zeroing-out AuthorizeAlways
								// in any case!
								if (Member.updated_by != 8888)
								{
									ExecSQL (	MAIN_DB, UPD_members_ClearAuthorizeAlways,
												&TrnID,					&v_MemberIdentification,
												&v_IdentificationCode,	END_OF_ARG_LIST				);

									SQLERR_error_test ();
								}	// Updated_by <> 8888, meaning that we're *not* dealing with a temporary user request.
							}	// ID entered by hand and authorize-always was set.
							else
							{
								SetErrorVar (&v_ErrorCode, ERR_PHARM_HAND_MADE);
							}	// ID entered by hand and authorize-always was NOT set.
						}	// ID was entered by hand, not by passing magnetic card.
					}	// Member does have Maccabi card.
				}	// NOT a temporary member.
			}	// Pharmacy card flag == 1.

			// DonR 30Mar2014: For convenience, store some member stuff in a structure - this will make function
			// arguments simpler.
			Member.ID				= v_MemberIdentification;
			Member.ID_code			= v_IdentificationCode;
			Member.sex				= v_MemberGender;
			Member.discount_percent	= v_MemberDiscountPercent;
			Member.illness_bitmap	= illness_bitmap;
			Member.in_hospital		= 0;	// Trn. 2003 doesn't use this flag; if necessary, add it to member-reading logic.

			// DonR 10Jul2012: Validate Doctor Location Code against Prescription Source.
			// Three circumstances can trigger an error:
			// (1) Prescription source does not require a Doctor Location Code, but one was supplied.
			// (2) Prescription source *does* require a Doctor Location Code, but one was *not* supplied.
			// (3) A Doctor Location Code (other than 999999) was supplied, but the location is invalid
			//     based on lookup in macabi_centers.
			// DonR 31Oct2012: Since Trn. 2003 deals only with drug sales, we do not have to make this test
			// conditional on "action code".
			ErrorCodeToAssign = 0;

			if (PrescrSource [v_RecipeSource].PrSrcDocLocnRequired == 0)
			{
				if (v_DoctorDeviceCode != 0)
					ErrorCodeToAssign = ERR_DOC_LOCATION_INVALID;
			}
			else
			{	// Prescription source *does* require a valid Doctor Location Code.
				if ((v_DoctorDeviceCode < 990001) || (v_DoctorDeviceCode > 999999))
				{
					ErrorCodeToAssign = ERR_DOC_LOCATION_INVALID;
				}
				else
				{
					// Don't look up Code 9999.
					if (v_DoctorDeviceCode < 999999)
					{
						v_DoctorLocationCodeMinus990000 = v_DoctorDeviceCode - 990000;

						ExecSQL (	MAIN_DB, READ_MaccabiCenters_ValidateDocFacility,
									&RowsFound, &v_DoctorLocationCodeMinus990000, END_OF_ARG_LIST	);

						Conflict_Test (reStart);
						
						if (SQLERR_error_test ())
						{
							ErrorCodeToAssign = ERR_DATABASE_ERROR;
						}
						else
						{
							if (RowsFound < 1)
								ErrorCodeToAssign = ERR_DOC_LOCATION_INVALID;
						}	// Valid read.
					}	// Doctor Location Code needs to be looked up in macabi_centers.
				}	// Doctor Location Code is in correct range.
			}	// Prescription source *does* require a valid Doctor Location Code.
			
			// If we found something wrong with the Doctor Location Code, report it.
			if (ErrorCodeToAssign > 0)
			{
				if (SetErrorVar (&v_ErrorCode, ErrorCodeToAssign))
				{
					err = MAC_TRUE;
					break;
				}
			}


			// If a Client Location Code was sent (e.g. member lives in an old-age home),
			// validate against the macabi_centers table. This will also allow us to
			// sell drugs against a credit line without using the member's Maccabi Card,
			// if the location has a Credit Flag of 9.
			// DonR 24Dec2013: If this is a private-pharmacy sale and a Client Location Code
			// was sent, return an error just as if a non-valid code had been sent.
			if (v_ClientLocationCode > 1)
			{
				if (MACCABI_PHARMACY)
				{
					ExecSQL (	MAIN_DB, READ_MaccabiCenters_PatientFacility,
								&v_MacCent_cred_flg,	&first_center_type,
								&v_ClientLocationCode,	END_OF_ARG_LIST			);

					Conflict_Test (reStart);
				}
				else FORCE_NOT_FOUND;

				if (SQLERR_code_cmp (SQLERR_not_found) == MAC_TRUE)
				{
					SetErrorVar (&v_ErrorCode, ERR_LOCATION_CODE_WRONG);
					err = MAC_TRUE;
					break;
				}
				else
				{
					if (SQLERR_error_test ())
					{
						SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR);
						break;
					}

					// DonR 04Jan2018 CR #13453: If this is an Al Tor order, check how many times this member
					// has ordered through Al Tor and failed to pick up his/her drugs.
					else	// Successful read of macabi_centers.
					{
						if (!strncmp (first_center_type, "01", 2))
						{
							ErrorCodeToAssign = MemberFailedToPickUpDrugs (&Member);

							if (ErrorCodeToAssign > 0)
							{
								if (SetErrorVar (&v_ErrorCode, ErrorCodeToAssign))
								{
									err = MAC_TRUE;
									break;
								}
							}	// MemberFailedToPickUpDrugs() detected enough no-shows to throw an error.
						}	// Current action is an Al Tor sale.
					}	// Successful read of macabi_centers.
				}	// Something other than not-found for macabi_centers lookup.
			}	// Client Location Code > 1.


			// DonR 10Jul2005: Maccabi Doctor prescriptions *must* have the doctor's
			// Teudat Zehut number, *not* her license number.
			if ((v_RecipeSource				== RECIP_SRC_MACABI_DOCTOR)	&&
				(v_TypeDoctorIdentification	== 1))
			{
				SetErrorVar (&v_ErrorCode, ERR_DOCTOR_ID_CODE_WRONG);
				err = MAC_TRUE;
				break;
			}

			// Test doctor's ID-CODE.
			// DonR 02Sep2004: Perform this lookup for Maccabi-by-hand prescriptions as well.
			//
			// DonR 10Jul2005: Perform this lookup for *any* prescription where the doctor-id
			// type is not set to 1. Prescriptions from doctors who are not in the doctors
			// table should have the doctor's licence number (ID Type 1), *not* the doctor's
			// Teudat Zehut number. However, if this is a non-prescription sale and the
			// Doctor ID is zero, don't check.
			if ((v_TypeDoctorIdentification	!= 1)	&&
				((v_RecipeSource != RECIP_SRC_NO_PRESC) || (DocID > 0)))
			{
				ExecSQL (	MAIN_DB, READ_doctors,
							&v_DoctorFirstName,		&v_DoctorFamilyName,
							&DummyPhone,			&DummyCheckInteractions,
							&DocID,					END_OF_ARG_LIST			);
 
				Conflict_Test (reStart);

				if (SQLERR_code_cmp (SQLERR_not_found) == MAC_TRUE)
				{
					SetErrorVar (&v_ErrorCode, ERR_DOCTOR_ID_CODE_WRONG);
					err = MAC_TRUE;
					break;
				}
				else
				{
					if (SQLERR_error_test ())
					{
						SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR);
						break;
					}
				}
			}	// Prescription is from Maccabi doctor.

			// DonR 24Nov2011: For non-Maccabi doctors, perform the name lookup here so that we don't
			// have to access doctor_percents for every drug in the sale.
			// DonR 05Sep2016: Changed SELECT DISTINCT to SELECT FIRST 1 to avoid a cardinality violation.
			if ((v_TypeDoctorIdentification	== 1) && (DocID > 0))
			{
				ExecSQL (	MAIN_DB, READ_DoctorByLicenseNumber,
							&v_DoctorFirstName,		&v_DoctorFamilyName,
							&DummyPhone,			&DummyCheckInteractions,
							&DummyRealDoctorTZ,
							&DocID,					END_OF_ARG_LIST				);

				// If we didn't get a "hit", blank out the doctor-name variables.
				if (SQLERR_code_cmp (SQLERR_ok) != MAC_TRUE)
				{
					strcpy (v_DoctorFamilyName,	"");
					strcpy (v_DoctorFirstName,	"");

					if (SQLERR_code_cmp (SQLERR_not_found) != MAC_TRUE)
					{
						if (SQLERR_error_test ())
						{
							SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR);
							break;
						}
					}	// Something other than not-found.
				}	// Something other than successful read.
			}	// Non-zero Doctor License Number needs to be looked up.

		}
		while (0);
		// End of Big Dummy Loop #1.

		// DonR 27Apr2005: No need to restore "normal" database isolation here.


		// DUMMY LOOP #2 TO PREVENT goto.

		// DonR 21Apr2005: Avoid unnecessary lock errors by downgrading "isolation"
		// for this section. (Note that it should already be downgraded!)
		SET_ISOLATION_DIRTY;

		do	// Exiting from LOOP writes to database and sends reply.
		{
			// Drugs validation.
			// DonR 09Jun2010: Split the previous loop into two loops. The first one reads
			// all drugs from the drug_list table (using the new function read_drug)
			// and does some minimal processing of the data read; the second one deals with
			// Drug Purchase Limits, etc. This eliminates the ugly "read ahead" logic I added
			// on 06Jun2010, since now by the time we look at limits, all the drug_list data
			// for the current sale is already available.
			for (i = 0; i < v_NumOfDrugLinesRecs; i++)
			{
				v_DrugCode = SPres[i].DrugCode;

				// DonR 05Mar2017: Set default Prescription Source Reject flag (based on values
				// in the prescr_source table).
				SPres[i].PurchaseLimitSourceReject = DefaultSourceReject;

				// DonR 25Jul2013: If we found a vacation ishur in special_prescs, copy its ishur number into all drugs.
				if (Member.Has_PL_99997_Ishur)
					SPres[i].vacation_ishur_num = Member.PL_99997_IshurNum;

				if (!read_drug (v_DrugCode,
								v_MaxLargoVisible,
								&Phrm_info,
								false,	// Deleted drugs are "invisible".
								NULL,
								&SPres[i]))
				{
					Conflict_Test (reStart);

					if (SQLERR_code_cmp (SQLERR_not_found) == MAC_TRUE)
					{
						if (SetErrorVar (&SPres[i].DrugAnswerCode, ERR_DRUG_CODE_NOT_FOUND))
						{
							// DonR 27Feb2008: Provide some defaults when drug isn't read.
							SetErrorVar (&v_ErrorCode, ERR_DRUG_S_WRONG_IN_PRESC);
							SPres[i].DFatalErr			= MAC_TRUE;
							SPres[i].BasePartCode		= 1;
							SPres[i].RetPartCode		= 1;
							SPres[i].part_for_discount	= 1;
							SPres[i].in_health_pack		= 0;
							continue;
						}
					}	// Drug not found.
					else
					{
						if (SQLERR_error_test ())
						{
							SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR);
							break;
						}
					}
				}	// read_drug() returned zero (i.e. something went wrong).

				// If we get here, we have successfully read the drug_list row.

				// DonR 17Oct2021 User Story #196891: Check whether this drug is blocked for this user.
				// DonR 13Mar2025 User Story #384811: To support more detailed drug-blocking based on
				// different categories of Darkonaim (Maccabi, Harel Tourists, Harel Foreign Workers),
				// add Darkonai Type (3 times) to the READ_CheckDrugBlockedForMember parameter list.
				if (Member.has_blocked_drugs)	// Don't waste a DB lookup if we know we won't get results.
				{
					ExecSQL (	MAIN_DB, READ_CheckDrugBlockedForMember,
								&RowsFound,
								&Member.ID,
								&Member.darkonai_type,		&Member.darkonai_type,	&Member.darkonai_type,
								&SPres[i].DL.largo_code,	&Member.ID_code,		END_OF_ARG_LIST			);

					if ((SQLCODE == 0) && (RowsFound > 0))
					{
						// According to Iris, a single version of the error code is enough - we don't need
						// different error text for Maccabi versus private pharmacies.
						if (SetErrorVar (&SPres[i].DrugAnswerCode, MACCABI_PHARMACY ? LARGO_CODE_BLOCKED_FOR_MEMBER_MAC : LARGO_CODE_BLOCKED_FOR_MEMBER_PVT))
						{
							SetErrorVar (&v_ErrorCode, ERR_DRUG_S_WRONG_IN_PRESC);
							continue;
						}
					}
					else
					{
						if (SQLERR_code_cmp (SQLERR_not_found) != MAC_TRUE)
						{
							SQLERR_error_test ();
						}
					}
				}

				// DonR 11Jul2010: Calling routine is now responsible for copying health-basket parameter
				// from the DL structure to the sPres structure.
				SPres[i].in_health_pack		= SPres[i].DL.health_basket_new;

				// DonR 11Sep2012: Send pharmacy a warning message if the drug being sold in non-chronic
				// and the prescription date is too far in the future (based on the sale_ok_early_days
				// parameter in sysparams) OR the Duration parameter is greater than 45 days.
				// DonR 16Sep2012: Don't look at Chronic Flag; Chronic Months must be >= 3 for a
				// drug to be considered chronic.
				// DonR 07Jul2013: New "Bakara Kamutit" logic includes new controls on future sales.
				// Duration is no longer relevant.
				if (PR_Date[i] > MaxFutureDate)
				{
					// Future sale can be permitted in several different ways; proceed from the most general
					// to the most specific. (Note that SPres[i].why_future_sale_ok has an initial value of zero.)
					// For now at least, future sales are permitted only at Maccabi pharmacies except in the case
					// of a vacation ishur - but this may change in the future for members who live overseas.
					// DonR 09Oct2018 CR #26700: Pharmacies are now authorized to sell future-dated prescriptions
					// based on a new flag, can_sell_future_rx, in the Pharmacy table.
					if (CAN_SELL_FUTURE_RX)
					{
						if ((SPres[i].DL.allow_future_sales == FUTURE_SALE_MAC_ONLY)	||
							(SPres[i].DL.allow_future_sales == FUTURE_SALE_ALL_PHARM))
						{
							SPres[i].why_future_sale_ok = (SPres[i].DL.allow_future_sales == FUTURE_SALE_ALL_PHARM) ?
															FUT_PERMITTED_ALL_PHARM : FUT_PERMITTED_MACCABI;
						}	// Future sale permitted by drug_list allow_future_sales flag.
						else
						{
							// Members who live overseas can fill future-dated prescriptions at Maccabi pharmacies.
							if (OVERSEAS_MEMBER)
								SPres[i].why_future_sale_ok = FUT_PERMITTED_OVRSEAS;
						}	// Future-dated sales of this drug are not normally permitted.
					}	// Sale at Maccabi pharmacy (or a private pharmacy with can_sell_future_rx set non-zero).

					// Vacation ishurim allow future sales at all pharmacies.
					if ((SPres[i].why_future_sale_ok == 0) && (Member.Has_PL_99997_Ishur))
						SPres[i].why_future_sale_ok = FUT_PERMITTED_ISHUR;

					if (SPres[i].why_future_sale_ok == 0)
					{
						if (SetErrorVar (&SPres[i].DrugAnswerCode, ERR_PR_DATE_IN_FUTURE))
						{
							SetErrorVar (&v_ErrorCode, ERR_DRUG_S_WRONG_IN_PRESC);
//							continue;
						}	// Error is fatal.
					}	// Future sale is NOT approved!
				}	// Future drug sale.

				// Compute ingredient usage for Ishur Limit checking - and possibly, eventually,
				// for ingredient-based Overdose/Interaction checking.
				for (ingred = 0; ingred < 3; ingred++)
				{
					if ((SPres[i].DL.Ingredient[ingred].code		<  1)	||
						(SPres[i].DL.Ingredient[ingred].quantity	<= 0.0)	||
						(SPres[i].DL.Ingredient[ingred].per_quant	<= 0.0)	||
						(SPres[i].DL.package_size					<  1))
					{
						// Invalid values - set this slot to zero.
						SPres[i].DL.Ingredient	[ingred].code		= 0;
						SPres[i].DL.Ingredient	[ingred].quantity	= 0.0;
						SPres[i].TotIngrQuantity[ingred]			= 0.0;
						SPres[i].IngrQuantityStd[ingred]			= 0.0;
					}

					else
					{
						if (SPres[i].DL.package_size == 1)
						{
							// Syrups and similar drugs.
							SPres[i].TotIngrQuantity[ingred] =	  (double)SPres[i].Op
																* SPres[i].DL.package_volume
																* SPres[i].DL.Ingredient[ingred].quantity
																/ SPres[i].DL.Ingredient[ingred].per_quant;
							SPres[i].IngrQuantityStd[ingred] =	  (double)SPres[i].Op
																* SPres[i].DL.package_volume
																* SPres[i].DL.Ingredient[ingred].quantity_std
																/ SPres[i].DL.Ingredient[ingred].per_quant;
						}

						else	// Package size > 1.
						{
							// Tablets, ampules, and similar drugs.
							// For these medications, ingredient is always given per unit.
							UnitsSold	=	  (SPres[i].Op		* SPres[i].DL.package_size)
											+  SPres[i].Units;

							SPres[i].TotIngrQuantity[ingred] =   SPres[i].DL.Ingredient[ingred].quantity
															   * (double)UnitsSold;
							SPres[i].IngrQuantityStd[ingred] =   SPres[i].DL.Ingredient[ingred].quantity_std
															   * (double)UnitsSold;
						}
					}	// Valid quantity values available.
				}	// Loop through ingredients for this drug.

		    }	// Drug reading/preliminary processing loop.

		    if ((reStart == MAC_TRUE) || (v_ErrorCode == ERR_DATABASE_ERROR))
		    {
				break;
			}


			// DonR 09Jun2010: Second drug loop, to handle Drug Purchase Limits and other processing.
			for (i = 0; i < v_NumOfDrugLinesRecs; i++)
			{
				v_DrugCode = SPres[i].DrugCode;

				// DonR 12Dec2006: Check drug against applicable member-pharmacy restrictions.
				// We're interested here only in Hypnotics and Narcotics.
				if ((v_MemPharm_Exists) && (v_MemPharm_ResType == 1))
				{
					if ((SPres[i].DL.drug_type == 'H') || (SPres[i].DL.drug_type == 'N'))
					{
						// Remember that the "error code" may be just telling the pharmacy
						// that the sale is permitted!
						SetErrorVarF (&SPres[i].DrugAnswerCode, MemPharm_Result, &SPres[i].DFatalErr);
					}
				}


				// DonR 29Jun2006: If this drug has a Drug Purchase Limit, attempt to read
				// the relevant fields from the drug_purchase_lim table and store them.
				if (SPres[i].DL.purchase_limit_flg > 0)
				{
					// DonR 26May2010 "Stickim": read previously-purchased drugs now, so we'll
					// be able to categorize member for new drug-purchase-limit logic based on
					// his/her previous drug purchases.
					FunctionError = get_drugs_in_blood (&Member,
														&v_DrugsBuf,
														GET_ALL_DRUGS_BOUGHT,
														&v_ErrorCode,
														&reStart);
					if (FunctionError == 1)
					{
						break;	// high severity error OR deadlock occurred.
					}

					// DonR 01Dec2016: New function ReadDrugPurchaseLimitData() replaces separate code in Transactions
					// 2003, 5003, and 6003, and adds new functionality to support ingredient-based purchase limits.
					FunctionError = ReadDrugPurchaseLimitData (&Member, SPres, i, v_NumOfDrugLinesRecs, &v_DrugsBuf, NULL, &v_ErrorCode, &reStart);
				}	// Drug has a Drug Purchase Limit.


				// Logical tests on drug.
				// 1. Prescription drug sold w/o prescription.
				if ((v_RecipeSource					== RECIP_SRC_NO_PRESC) &&
					(SPres[i].DL.no_presc_sale_flag	== MAC_FALS))
				{
				    if (SetErrorVarF (&SPres[i].DrugAnswerCode, ERR_DRUG_REQUIRE_PRESCRIPTION, &SPres[i].DFatalErr))
				    {
						SetErrorVar (&v_ErrorCode, ERR_DRUG_S_WRONG_IN_PRESC);
						continue;
				    }
				}	// Drug Test 1 - presc. drug sold w/o prescription.

				// 2. DonR 09Jun2005: If this is a non-prescription purchase from a private pharmacy,
				// either the drug must have its MaccabiCare flag set true (meaning it's a
				// Maccabi-branded OTC drug) OR the pharmacy must have sent a Maccabi Sale Number
				// (which is sent and stored in the Doctor Prescription ID field). If neither of
				// these conditions is met, the pharmacy should never have sent us this drug-line.
				if ((v_RecipeSource			== RECIP_SRC_NO_PRESC)	&&
					(PRIVATE_PHARMACY)								&&
					(SPres[i].DL.maccabicare_flag	== 0)			&&
					(PR_DoctorPrID		[i] == 0))
				{
				    if (SetErrorVarF (&SPres[i].DrugAnswerCode, ERR_NOT_MACCABICARE_DRUG, &SPres[i].DFatalErr))
				    {
						SetErrorVar (&v_ErrorCode, ERR_DRUG_S_WRONG_IN_PRESC);
						continue;
				    }
				}



				// DonR 03Feb2004: Use separate function to find preferred (generic) drug.
				// DonR 14APR2003: Per Iris Shaya - if a non-preferred drug is being sold for
				// a reason (e.g. "dispense as written" or pharmacy is out of preferred drug),
				// member participates as if s/he were being given the preferred drug.
				// DonR 13Jan2005: Moved this block of code down to after drug_list is
				// read, so that we know beforehand whether the drug is in economypri.
				// Also, note that find_preferred_drug now looks up the Member Price Code
				// of the preferred drug, if any.
				find_preferred_drug (SPres[i].DrugCode,
									 SPres[i].DL.economypri_group,
									 SPres[i].DL.preferred_flg,
									 SPres[i].DL.preferred_largo,
									 0,	// No need to check purchase history for alternate preferred drugs.
									 0,	// Allow drugs with Preferred Status = 3 to be sold without requiring an explanation from pharmacy.
									 DocID,
									 v_MemberIdentification,
									 v_IdentificationCode,
									 &EP_LargoCode,
									 &PR_AsIf_Preferred_SpDrug	[i],
									 &PR_AsIf_ParentGroupCode	[i],
									 &preferred_mbr_prc_code	[i],
									 &DispenseAsWritten,
									 &PR_AsIf_Preferred_Basket	[i],
									 &PR_AsIf_RuleStatus		[i],
									 NULL,	// DL structure pointer
									 &FunctionError);

				SetErrorVar (&v_ErrorCode, FunctionError);

				// Check if the drug to be sold is the one Maccabi wants to sell. If not, send
				// a warning message to the pharmacy.
				if (EP_LargoCode != SPres[i].DrugCode)
				{
					// If the pharmacist has a reason for selling a drug other
					// than the "preferred" one, store the preferred drug's Largo Code
					// for later use in checking member's participation level. Otherwise,
					// return a "Not Preferred Drug" warning code.
					//
					// DonR 28Mar2010: Changed handling of reason sent by pharmacy; instead of
					// excluding NO_REASON_GIVEN and DISPENSED_KACHA (= 0 and 99), now we
					// *include* only OUT_OF_STOCK_SUB_EQUIV and EXCESS_STOCK (= 2 and 3).
					// DOCTOR_NIXED_SUBSTITUTION (= 1) and any other strange value will *not*
					// allow sale of non-preferred drugs at preferred prices.
					if ((PR_WhyNonPreferredSold[i] == OUT_OF_STOCK_SUB_EQUIV)	||
						(PR_WhyNonPreferredSold[i] == EXCESS_STOCK			)	||
						(DispenseAsWritten))
					{
						// Non-substitution is legitimate.
						PR_AsIf_Preferred_Largo[i] = EP_LargoCode;

						// DonR 14Jan2009: Give the AS/400 a record of why we gave "as-if-preferred"
						// participation.
						// DonR 28Mar2010: Give a separate reason-code if participation was granted
						// based on a "tofes" that expired within the last 91 days.
						// DonR 20Dec2011: If a private pharmacy is selling a non-generic medicine because
						// the generic version is out of stock, send a warning message. Note that if the
						// doctor has nixed substitution anyway, there's no need to send this message - so
						// we execute this logic only for DispenseAsWritten == 0.
						switch (DispenseAsWritten)
						{
							case 0:		SPres[i].WhyNonPreferredSold = PHARM_AUTH_NON_GENERIC;

										if ((PRIVATE_PHARMACY)											&&
											(v_RecipeSource				!= RECIP_SRC_NO_PRESC		)	&&
											(PR_WhyNonPreferredSold[i]	== OUT_OF_STOCK_SUB_EQUIV	))
										{
											SetErrorVar (&SPres[i].DrugAnswerCode, ERR_OUT_OF_STOCK_WARN);
										}
										break;

							case 1:		SPres[i].WhyNonPreferredSold = DOC_AUTH_NON_GENERIC;
										break;

							case 2:		SPres[i].WhyNonPreferredSold = DOC_AUTH_EXPIRED_TOFES;
										break;

							// Default condition should never really happen - but just in case...!
							default:	SPres[i].WhyNonPreferredSold = PHARM_AUTH_NON_GENERIC;	break;
						}
					}
					else
					{
						PR_AsIf_Preferred_Largo [i] = 0;	// Disable preferred-drug swapping - redundant, but paranoia is good!

						// If we're not going to use an "as-if-preferred" drug, zero
						// out the "as-if-preferred in-health-basket" parameter.
						PR_AsIf_Preferred_Basket[i] = 0;

						// DonR 14Jan2009: Give the AS/400 a record of the fact that a non-preferred
						// drug was sold without using "as-if-preferred" conditions.
						SPres[i].WhyNonPreferredSold = NON_GENERIC_SOLD_AS_IS;

						// 22Dec2003: Per Iris Shaya, non-preferred drug warning is given only if the
						// non-preferred drug is being dispensed without any indication that the
						// pharmacist is aware of the problem. If the pharmacist gives the Reason Code
						// 99 ("kacha"), no "acceptable" reason for the substitution is being offered,
						// but the pharmacist is already aware of the situation and doesn't need a message.
						// DonR 08Mar2006: Per Iris Shaya, if the drug's preferred_flg is 2, don't give
						// a warning.
						// DonR 28Mar2010: If pharmacy sent reason 1 (DOCTOR_NIXED_SUBSTITUTION) but
						// there is no valid doctor's ishur for dispensing the prescription as written,
						// give pharmacy a new, severe error message telling them so.
						// DonR 23Mar2015: Don't give an error if the drug being sold is, in fact, preferred.
						// This can happen when member bought Preferred Drug X last time but (for whatever
						// reason) is buying Preferred Drug Y this time.
						if (((PR_WhyNonPreferredSold[i]	== NO_REASON_GIVEN)				||
							 (PR_WhyNonPreferredSold[i]	== DOCTOR_NIXED_SUBSTITUTION))		&&
							(SPres[i].DL.preferred_flg	!= 1)								&&	// DonR 23Mar2015.
							(SPres[i].DL.preferred_flg	!= 2)								&&
							(v_RecipeSource				!= RECIP_SRC_NO_PRESC))
						{
							// DonR 09May2010 HOT FIX: Use new non-critical error code so that we
							// don't give up on this drug yet; it's possible member has an AS/400
							// ishur for the non-generic drug, in which case we'll use that and
							// suppress the non-critical error. If there isn't an AS/400 ishur
							// applicable, we'll replace the non-critical error with the "real"
							// critical error later on.
							SetErrorVar (&SPres[i].DrugAnswerCode,
											(PR_WhyNonPreferredSold[i] == DOCTOR_NIXED_SUBSTITUTION) ?
												ERR_NO_DOCTORS_ISHUR_WARN :
												ERR_NOT_PREFERRED_DRUG_WARNING);
						}

					}	// No reason given for dispensing non-preferred drug.
				}	// Drug being sold isn't the preferred one.

				else
				// No preferred drug found - zero appropriate variables.
				{
					preferred_mbr_prc_code  [i]		= 0;
					PR_AsIf_Preferred_SpDrug[i]		= 0;
					PR_AsIf_Preferred_Basket[i]		= 0;
					PR_AsIf_ParentGroupCode	[i]		= 0;
					PR_AsIf_RuleStatus		[i]		= 0;
					PR_AsIf_Preferred_Largo [i]		= 0;	// Disable preferred-drug swapping.
					SPres[i].WhyNonPreferredSold	= 0;	// DonR 14Jan2009: shouldn't really be necessary.
				}
				// DonR 13Jan2005 end.


				// Set up pricing defaults.
				SPres[i].ret_part_source.table			= FROM_DRUG_LIST_TBL;
				SPres[i].ret_part_source.insurance_used	= BASIC_INS_USED;

				// Previous Member Price Code is not used for Electronic Prescriptions!
				// DonR 16May2006: Kishon Divers (who are supposed to get their medications
				// direct from the government) pay 100% unconditionally.
				// DonR 09Aug2021 User Story #163882: Treat darkonaim-plus people who always
				// pay 100% (minus shovarim they get from their insurance company) the same
				// as Kishon divers.
				if ((v_IdentificationCode == 3)		||	// Kishon divers.
					(Member.force_100_percent_ptn))		// Darkonaim-plus not getting any discounts.
				{
					SPres[i].BasePartCode				= PARTI_CODE_ONE;
					SPres[i].RetPartCode				= PARTI_CODE_ONE;
					SPres[i].part_for_discount			= PARTI_CODE_ONE;
				}
				else	// Normal members.
				{
					SPres[i].BasePartCode				= SPres[i].DL.member_price_code;
					SPres[i].RetPartCode				= SPres[i].DL.member_price_code;
					SPres[i].part_for_discount			= SPres[i].DL.member_price_code;
				}

			    SPres[i].BasePartCodeFrom			= CURR_CODE;
			    SPres[i].ret_part_source.state		= CURR_CODE;

				// DonR 06Dec2012 "Yahalom": Disable the drug-level date logic; drug_extension and
				// spclty_largo_prcnt rules are now assigned individual wait times, and the relevant
				// SELECTs will use the member's "vetek" values on a case-by-case basis.
//				if (0)
//				{
//
//				// DonR 27Nov2003: Set insurance flags based on drug-specific waiting time
//				// for supplemental insurance.
//				// DonR 10Dec2003: If this member is exempt from waiting times, force waiting periods to zero.
//				// DonR 29Dec2011: At least for the moment, treat keren_wait_flag == 2 the same as
//				// keren_wait_flag == 0.
//				if ((Member.keren_wait_flag == 0) || (Member.keren_wait_flag == 2))
//				{
//					SPres[i].DL.magen_wait_months = 0;
//					SPres[i].DL.keren_wait_months = 0;
//				}
//
//				// Note: we subtract 8800 (i.e. subtract one year and add twelve months) to enable negative
//				// wait times.
//				// Magen first...
//				DrugStartDate = mac_magen_from - 8800 + (SPres[i].DL.magen_wait_months * 100);
//				while ((DrugStartDate % 10000) > 1299)
//				{
//					DrugStartDate += 8800;	// = plus one year (10000) minus twelve months (1200).
//				}
//
//				if (DrugStartDate > SysDate)
//					SPres[i].mac_magen_code = NO_MACBI_MAGE;	// Forced false by wait time.
//				else
//					SPres[i].mac_magen_code = mac_magen_code;	// Drug wait time irrelevant - use default.
//
//
//				// ...and now Keren.
//				DrugStartDate = keren_mac_from - 8800 + (SPres[i].DL.keren_wait_months * 100);
//				while ((DrugStartDate % 10000) > 1299)
//				{
//					DrugStartDate += 8800;	// = plus one year (10000) minus twelve months (1200).
//				}
//
//				if (DrugStartDate > SysDate)
//					SPres[i].keren_mac_code = NO_KEREN_MACBI;	// Forced false by wait time.
//				else
//					SPres[i].keren_mac_code = keren_mac_code;	// Drug wait time irrelevant - use default.
//
//				// DonR 27Nov2003 end.
//
//				}	// DonR 06Dec2012 "Yahalom" end: Disable the drug-level date logic.

			}	// Loop through drugs.

		    if ((reStart == MAC_TRUE) || (v_ErrorCode == ERR_DATABASE_ERROR))
		    {
				break;
			}


			// Drugs SUPPLEMENTs test.
			for (i = 0; i < v_NumOfDrugLinesRecs; i++)
		    {
				// Skip this drug if one of the following is true:
				// 1. There's no supplement link - meaning that drug[i] isn't a supplement.
				// 2. It's a Package or Preparation type drug.
				// 3. We've already hit a severe error for this drug.

				// DonR 22Dec2003: Per Iris Shaya, bottles (Largo Type 'A') should be treated like other
				// parts of the preparation - that is, they get the same Participation Code as the
				// Preparation (Largo Type 'X', the 'major drug') itself.
				// DonR 23Aug2005: For "gadgets", LinkDrugToAddition is used optionally to indicate
				// the "gadget code" - e.g. in order to differentiate between knee braces sold for
				// use on the right knee as opposed to identical braces sold for the left knee.
				// Thus, if the "drug" is a gadget, skip the drug-supplement stuff.
				if ((SPres[i].LinkDrugToAddition	== 0)			||
					(SPres[i].DL.in_gadget_table	>  0)			||
					(SPres[i].DL.largo_type			== 'X')			||
					(SPres[i].DFatalErr				== MAC_TRUE))
				{
					continue;
				}

				flg = MAC_FALS;		// Major drug found flag.

				// Loop through the other drugs in the prescription, searching for the major
				// drug to which drug[i] is linked.
				for (is = 0; is < v_NumOfDrugLinesRecs; is++)
				{
					// Skip past drug[i] - we don't want to compare it to itself!
				    if (is == i)
						continue;

					// If this drug isn't the one that drug[i] links to, skip it.
				    if (SPres[is].DrugCode != SPres[i].LinkDrugToAddition)
						continue;

				    flg = MAC_TRUE;	// Major drug found for supplemental drug[i].
					MajorDrugIndex[i] = is;

					// If the major drug is a Preparation (type 'X'), skip
					// the linkage and quantity tests.
					if (SPres[is].DL.largo_type != 'X')
					{
						// If drug[i] isn't recognized as a supplement to drug[is], set error.
						if ((SPres[i].DrugCode != SPres[is].DL.supplemental_1) &&
							(SPres[i].DrugCode != SPres[is].DL.supplemental_2) &&
							(SPres[i].DrugCode != SPres[is].DL.supplemental_3))
						{
							if (SetErrorVarF (&SPres[i].DrugAnswerCode, ERR_SUPPLIMENT_NOT_APPRUVED, &SPres[i].DFatalErr))
							{
								SetErrorVar (&v_ErrorCode, ERR_DRUG_S_WRONG_IN_PRESC);
							}
						}	// Drug[i] isn't valid supplement to drug[is].

					    else
						{
							// The supplement is the right stuff. Now match quantity
							// of supplement to that of the major drug.
							if (SPres[i].Op > (((SPres[is].Units > 0) ? 1 : 0 ) + SPres[is].Op))
							{
								// More packages of supplement than of major drug.
								if (SetErrorVarF (&SPres[i].DrugAnswerCode, ERR_SUPPL_OP_NOT_MATCH_DRUG_OP, &SPres[i].DFatalErr))
								{
								    SetErrorVar (&v_ErrorCode, ERR_DRUG_S_WRONG_IN_PRESC);
								}
						    }	// Too much of drug[i] to supplement quantity of drug[is].
						}	// Drug[i] is a valid supplement to drug[is].
					}	// The major drug[is] is not a Preparation (type 'X').


					// We've found the major drug and performed our tests - so we can quit
					// the inner loop.
				    break;
				}	// Loop on [is] to find Major drug for supplemental drug[i].


				// If no major drug was found for supplemental drug[i], set error.
				if (flg == MAC_FALS)
				{
				    if (SetErrorVarF (&SPres[i].DrugAnswerCode, ERR_SUPPLIMENT_NOT_APPRUVED, &SPres[i].DFatalErr))
					{
						SetErrorVar (&v_ErrorCode, ERR_DRUG_S_WRONG_IN_PRESC);
						break;
					}
				}
			}	// Loop through drugs to perform supplement-drug tests.


			// Get Drug prices and check for Special Prescriptions in the DB.
			for (i = 0; i < v_NumOfDrugLinesRecs; i++)
			{
				// Skip drugs for which we've already discovered a major error.
				if (SPres[i].DFatalErr == MAC_TRUE)
					continue;

				v_DrugCode = SPres[i].DrugCode;

				ExecSQL (	MAIN_DB, READ_PriceList_simple,
							&Yarpa_Price,	&Macabi_Price,		&Supplier_Price,
							&v_DrugCode,	&v_PriceListCode,	END_OF_ARG_LIST		);

				Conflict_Test (reStart);

				if (SQLERR_code_cmp (SQLERR_not_found) == MAC_TRUE)
				{
				    if (SetErrorVarF (&SPres[i].DrugAnswerCode, ERR_DRUG_PRICE_NOT_FOUND, &SPres[i].DFatalErr))
				    {
						SetErrorVar (&v_ErrorCode, ERR_DRUG_S_WRONG_IN_PRESC);
						continue;
					}
				}
				else
				{
					if (SQLERR_error_test ())
					{
					    SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR);
						break;
					}
				}


				// Store drug price in SPres array.
				// NOTE: The following call to GET_PARTICIPATING_PERCENT is ONLY to ensure that
				// the Participation Type is one we know something about. The retrieved
				// participation percentage is not used!
				state = GET_PARTICIPATING_PERCENT (SPres[i].BasePartCode,	&percents);
				if (state != NO_ERROR)
				{
				    if (SetErrorVar (&v_ErrorCode, state))
				    {
						break;
					}
				}

				
				// 25Mar2003: Per Iris, the logic for use of Maccabi Price is changing.
				// For normal participation calculation, we will always use the Yarpa
				// (i.e. regular) price; at the end, we'll use the Maccabi Price as
				// a fixed participation price if:
				//     A) A Maccabi Price exists;
				//     B) Participation is 100%; and
				//     C) No fixed participation has been set from other sources, such as
				//        Drug-Extension.
				SPres[i].RetOpDrugPrice		= Yarpa_Price;
				SPres[i].YarpaDrugPrice	    = Yarpa_Price;
				SPres[i].MacabiDrugPrice    = Macabi_Price;
				SPres[i].SupplierDrugPrice  = Supplier_Price;



				// Test Special Prescription.
				// 20020122: If a member has a special prescription, it is
				// written in the members table (column spec_prescs). For such
				// members, we must check for a constant amount to pay.
				// DonR 21Jul2008: Per Iris Shaya, we should check Special Prescriptions only if drugs are
				// being sold based on some form of prescription. Non-prescription sales are not covered
				// by Special Prescriptions.
				if ((v_MemberSpecPresc /* from members table */)	&&
					(v_RecipeSource != RECIP_SRC_NO_PRESC)			&&
					(v_RecipeSource != RECIP_SRC_EMERGENCY_NO_RX))
				{
					TypeCheckSpecPr = 1;
				}

				// All the stuff about special special-prescription numbers
				// shouldn't apply to Electronic Prescriptions.
				if (TypeCheckSpecPr > 0)
			    {
					// DonR 04Sep2003: If there was a Pharmacy Ishur to use an expired AS400
					// Special Prescription (and such a Special Prescription was found), the
					// number of days to extend it (currently always 31) will be stored in
					// PR_ExtendAS400IshurDays[]. Otherwise the array will have a zero, and
					// things will work as normal.
					flg = test_special_prescription (NULL,//&v_DummySpecialConfirmationNum,
													 NULL,//&v_DummySpecialConfirmationNumSource,
													 &Member,
													 OVERSEAS_MEMBER,
													 v_TypeDoctorIdentification,
													 DocID,
													 &SPres[i],
													 &SPres[0],
													 v_NumOfDrugLinesRecs,
													 PR_AsIf_Preferred_Largo[i],		// DonR 23Feb2004
													 &FunctionError,
													 TypeCheckSpecPr,
													 v_PharmNum,
													 Phrm_info.pharmacy_permission,		// DonR 14Aug2003
													 SPres[i].BasePartCode,
													 PR_ExtendAS400IshurDays[i],
													 &ThisIshurHasTikra);

				    SetErrorVar (&v_ErrorCode, FunctionError);

					if (flg == 1)		/* high severity error occurred	*/
					{
					    break;
					}
					else
					{
						if (flg == 2)	/* deadlock; access conflict	*/
						{
							reStart = MAC_TRUE;
							break;
						}
						else	// DonR 28Mar2007: If function returned OK, check whether
								// an ishur was found that had its "Tikra" flag set. If
								// so, remember that at least one such ishur was found
								// for the sale, so we can advise the member to pay by
								// Credit Line.
						{
							if (ThisIshurHasTikra)
							{
								AnIshurHasTikra = SPres[i].IshurWithTikra = 1;
							}

							// DonR 09May2010 HOT FIX: If there is an AS/400 ishur for this member/drug,
							// suppress errors involving sale of non-generic medication without a valid
							// reason. The AS/400 ishur is a valid reason!
							if ((SPres[i].ret_part_source.table == FROM_SPEC_PRESCS_TBL)	&&
								((SPres[i].DrugAnswerCode == ERR_NO_DOCTORS_ISHUR_WARN)	||
								 (SPres[i].DrugAnswerCode == ERR_NOT_PREFERRED_DRUG_WARNING)))
							{
								SPres[i].DrugAnswerCode = 0;	// Kill the error.
							}
						}
					}

				}	// Need to test Special Prescription.

				// DonR 09May2010 HOT FIX: Replace fictitious warning message with the real, critical-error
				// version.
				if (SPres[i].DrugAnswerCode == ERR_NO_DOCTORS_ISHUR_WARN)
				{
					SPres[i].DrugAnswerCode = ERR_NO_DOCTORS_ISHUR;
				}

			}	// Get Drug Prices / Check Special Prescriptions loop.

			// If we've hit a major problem, break out of big DUMMY loop.
		    if ((reStart == MAC_TRUE) || (v_ErrorCode == ERR_DATABASE_ERROR))
		    {
				break;
			}


			// Check for interactions and overdoses.
			// DonR 27Jul2003: Made these tests a separate function, since the same 1000+ lines of code
			// are present in at least three transactions.
			// DonR 24Jan2018: Moved member validation inside test_interaction_and_overdose(), so we can
			// now execute it unconditionally.
			test_interaction_and_overdose (&Member,
											max_drug_date,
											v_PharmNum,
											&Phrm_info,
											v_InstituteCode,
											v_TerminalNum,
											v_RecipeIdentifier,
											SPres,
											v_NumOfDrugLinesRecs,
											v_SpecialConfNum,
											v_SpecialConfSource,
											1,	// DonR 23Mar2004 - Error Mode = "new style"
											1,	// DonR 31Aug2005: Electronic Prescription Flag.
											&DocRx[0],
											&v_MsgExistsFlg,
											&v_MsgIsUrgentFlg,
											&FunctionError,
											&reStart);

			SetErrorVar (&v_ErrorCode, FunctionError);


			// Member participating percent calculation.
			// Set Participation Code according to Prescription Source.
			// DonR 03Aug2003: Moved this section up, because we need to know the calculated
			// participation type *before* calculating Member Discount.
			flg = 0;

			// DonR 16May2006: Kishon Divers (who are supposed to get their medications
			// direct from the government) pay 100% unconditionally. This is implemented
			// (sneakily) by disabling the for-loop: Kishon divers have Identification Code
			// of 3, which makes the test condition FALSE for all values of i.
			// DonR 09Aug2021 User Story #163882: Add darkonai-plus people who don't get
			// any discounts (other than shovarim from their insurance company) to the
			// criteria for disabling this loop.
			for (i = 0;
				((i < v_NumOfDrugLinesRecs) && (v_IdentificationCode != 3) && (!Member.force_100_percent_ptn));
				i++)
			{

				// If the member gets this drug at no charge due to a Special
				// Prescription, don't bother performing further participation
				// tests.
				if ((SPres[i].RetPartCode == 4) && (SPres[i].SpecPrescNum))
					continue;

				switch (v_RecipeSource)
				{
					case RECIP_SRC_DENTIST:

							flg = test_dentist_drugs_electronic (&SPres[i],
																 &FunctionError);
							SetErrorVar (&v_ErrorCode, FunctionError);
							break;


					// DonR 23Feb2008: Added new Prescription Source 11, for
					// "Home Visit" prescriptions.
					case RECIP_SRC_HOME_VISIT:

							flg = test_home_visit_drugs_electronic (&SPres[i],
																	&FunctionError);
							SetErrorVar (&v_ErrorCode, FunctionError);
							break;

					default:							// DonR 29Aug2016 CR #8956.

							// DonR 05Aug2003: test_mac_doctor_drugs_electronic () now handles
							// retrieval of the name of non-Maccabi doctors (DocID = license number
							// rather than Teudat Zehut number) with entries in doctor_percents
							// and doctors tables.
							flg = test_mac_doctor_drugs_electronic (v_TypeDoctorIdentification,
																	DocID,
																	&SPres[i],
																	i + 1,
																	Member.age_months,
																	v_MemberGender,
																	Phrm_info.pharmacy_permission,
																	v_RecipeSource,
																	v_MemberIdentification,
																	v_IdentificationCode,
																	(v_ClientLocationCode > 1),
																	&PharmIshurProfession[i],
																	&FunctionError);

							SetErrorVar (&v_ErrorCode, FunctionError);
							break;

				}	// End of switch on prescription source.


				// DonR 14APR2003: If we are treating a non-preferred drug as if it
				// were the preferred drug (for purposes of determining member participation),
				// swap in the preferred drug for checking participation levels.
				// DonR 04Dec2003: Use the preferred largo only if the participation for the
				// dispensed largo would be 100% with no fixed price.
				// DonR 09Jan2012: Replaced two conditions with macro FULL_PRICE_SALE.
				if ((PR_AsIf_Preferred_Largo[i]	> 0) && (FULL_PRICE_SALE(i)))
				{
					// Switch pricing variables to preferred drug.
					SPres[i].BasePartCode				= preferred_mbr_prc_code[i];
					SPres[i].RetPartCode				= preferred_mbr_prc_code[i];
					SPres[i].part_for_discount			= preferred_mbr_prc_code[i];
					SPres[i].BasePartCodeFrom			= CURR_CODE;
					SPres[i].ret_part_source.state		= CURR_CODE;

					// Redo the participation-determination stuff with the preferred Largo Code.
					HoldLargo			= SPres[i].DrugCode;
					HoldSpecialistDrug	= SPres[i].DL.specialist_drug;
					HoldParentGroupCode	= SPres[i].DL.parent_group_code;
					HoldRuleStatus		= SPres[i].DL.rule_status;
					SPres[i].DrugCode				= PR_AsIf_Preferred_Largo	[i];
					SPres[i].DL.specialist_drug		= PR_AsIf_Preferred_SpDrug	[i];
					SPres[i].DL.parent_group_code	= PR_AsIf_ParentGroupCode	[i];
					SPres[i].DL.rule_status			= PR_AsIf_RuleStatus		[i];
					SPres[i].in_health_pack			= PR_AsIf_Preferred_Basket	[i];

					switch (v_RecipeSource)
					{
						case RECIP_SRC_DENTIST:

								flg = test_dentist_drugs_electronic (&SPres[i],
																	 &FunctionError);
								SetErrorVar (&v_ErrorCode, FunctionError);
								break;


						// DonR 23Feb2008: Added new Prescription Source 11, for
						// "Home Visit" prescriptions.
						case RECIP_SRC_HOME_VISIT:

								flg = test_home_visit_drugs_electronic (&SPres[i],
																		&FunctionError);
								SetErrorVar (&v_ErrorCode, FunctionError);
								break;

						default:							// DonR 29Aug2016 CR #8956.

								flg = test_mac_doctor_drugs_electronic (v_TypeDoctorIdentification,
																		DocID,
																		&SPres[i],
																		i + 1,
																		Member.age_months,
																		v_MemberGender,
																		Phrm_info.pharmacy_permission,
																		v_RecipeSource,
																		v_MemberIdentification,
																		v_IdentificationCode,
																		(v_ClientLocationCode > 1),
																		&PharmIshurProfession[i],
																		&FunctionError);

								SetErrorVar (&v_ErrorCode, FunctionError);
								break;

					}	// End of switch on prescription source.


					// Swap back the drug code actually being sold.
					SPres[i].DrugCode				= HoldLargo;
					SPres[i].DL.specialist_drug		= HoldSpecialistDrug;
					SPres[i].DL.parent_group_code	= HoldParentGroupCode;
					SPres[i].DL.rule_status			= HoldRuleStatus;
				}	// Need to recompute participation with preferred Largo Code.

				// DonR 22Mar2015: If we *didn't* need to compute participation for the preferred generic
				// drug (e.g. if there sas an applicable "nohal" for the non-generic drug), zero out the
				// "tens" digit of the why-non-preferred-drug-was-sold flag.
				else
				{
					SPres[i].WhyNonPreferredSold	= 0;
				}

			}	// Loop through drugs - yet again!



			// Test for deadlock or severe error.
			if (flg == 2) // deadlock or access conflict
			{
				reStart = MAC_TRUE;
			}

			// If we've hit a serious database error, break out of outer loop and restart.
		    if ((reStart == MAC_TRUE) || (v_ErrorCode == ERR_DATABASE_ERROR) || (flg == 1))
				break;



			// Member participating percent validation.
			//
			// Compare Prescription participation with Drug participation.
			flg = 0;

			for (i = 0; i < v_NumOfDrugLinesRecs; i++)
			{
			    if (SPres[i].DFatalErr == MAC_TRUE )
					continue;

				// DonR 07Aug2003: The supplement gets its participation (i.e. RetPartCode)
				// from the "major" drug.
				// DonR 23Aug2005: This is not true for "gadgets" - which sometimes use
				// the Link to Addition parameter for a different purpose.
			    if ((SPres[i].LinkDrugToAddition	>  0	)	&&
					(SPres[i].DL.largo_type			!= 'X'	)	&&
					(SPres[i].DL.in_gadget_table	== 0	)	&&
					(MajorDrugIndex[i]				>= 0	))
			    {
					SPres[i].RetPartCode = SPres[MajorDrugIndex[i]].RetPartCode;
					continue;
				}

				// Percent2 is the "default" participation for this drug.
			    state =	GET_PARTICIPATING_PERCENT (SPres[i].BasePartCode, &percent2);
			    if (state != NO_ERROR)
			    {
					if (SetErrorVarF (&SPres[i].DrugAnswerCode, state, &SPres[i].DFatalErr))
					{
						flg = 1;
					}
					continue;
			    }

				// Percent1 is the calculated participation, based on drug_extension,
				// doctor specialty, etc.
			    state = GET_PARTICIPATING_PERCENT (SPres[i].RetPartCode, &percent1);
			    if (state != NO_ERROR)
			    {
					if (SetErrorVarF (&SPres[i].DrugAnswerCode, state, &SPres[i].DFatalErr))
					{
						flg = 1;
					}
					continue;
			    }

				// 20010726 special_prescs check 
				// have special confirmation and participating in {2,3} 
				// and answer participating = 4 
				// allow sell 100.00 % maccabi => patient not pay
				if ((MACCABI_PHARMACY)						&&
					(SPres[i].SpecPrescNum)					&&	
					(SPres[i].SpecPrescNumSource))
				{
					if (((SPres[i].BasePartCode == PARTI_CODE_TWO) ||
						 (SPres[i].BasePartCode == PARTI_CODE_THREE))	&&
						(SPres[i].RetPartCode == 4))
					{
						SPres[i].AdditionToPrice = 10000;
						continue;
					}
				}


				// Process Pharmacy Ishur, if any, for this drug line.
				LineNum = i + 1;

				// DonR 30Mar2006: Don't bother checking Pharmacy Ishur for Member ID zero!
				if (v_MemberIdentification > 0)
				{
					flg = test_pharmacy_ishur (v_PharmNum,
											   v_RecipeSource,
											   v_MemberIdentification,
											   v_IdentificationCode,
											   Member.maccabi_code,
											   v_MemberDiscountPercent,
											   Member.illness_bitmap,
											   v_TypeDoctorIdentification,
											   DocID,
											   &SPres[i],
											   PR_AsIf_Preferred_Largo[i],
											   v_SpecialConfNum,
											   v_SpecialConfSource,
											   v_RecipeIdentifier,
											   LineNum,
											   (short)2003,
											   PharmIshurProfession[i],
											   &FunctionError);

					SetErrorVar (&v_ErrorCode, FunctionError);


					// Test for deadlock or severe error.
					if (flg == 2) // deadlock or access conflict
					{
						reStart = MAC_TRUE;
						break;
					}
					else
					{
						if (flg == 1)	// Other severe error.
						{
							break;
						}

						// If we get here, the Pharmacy Ishur routine was happy.
					}
				}


				// DonR 14Jun2004: If "drug" being purchased is a device in the "gadgets"
				// table, retrieve the appropriate information and query the AS400 for
				// participation information.
				// DonR 19Aug2025 User Story #442308: If Nihul Tikrot calls are disabled, disable Meishar calls as well.
				if ((SPres[i].DL.in_gadget_table)					&&
					(v_RecipeSource != RECIP_SRC_NO_PRESC)			&&
					(v_RecipeSource != RECIP_SRC_EMERGENCY_NO_RX)	&&
					(TikrotRPC_Enabled)								&&	// If Nihul Tikrot calls are disabled, disable Meishar calls too.
					(MEISHAR_PHARMACY))	// DonR 01Jul2012: New macro conditionally enables Prati Plus pharmacies.
				{
					v_gadget_code	= SPres[i].LinkDrugToAddition;
					v_DrugCode		= SPres[i].DrugCode;

					// DonR 18Sep2011: Add get_ptn_from_as400 flag to gadgets table; if it's equal to zero,
					// pharmacy has chosen a "placeholder" service that is not actually supposed to get
					// its participation from AS/400.
					// DonR 27Nov2011: Switch from get_ptn_from_as400 to new separate enabled-status flags
					// for Maccabi member and IDF soldiers.
					// DonR 25Jul2016: Fixed WHERE clause (which had stuff grouped together wrong); also
					// suppress the "duplicate gadget" message to log, since it's not an indication of any
					// actual program or database problem.
					ExecSQL (	MAIN_DB, TR2003_5003_READ_gadgets,
								&v_service_code,		&v_service_number,
								&v_service_type,		&v_enabled_mac,
								&v_enabled_hova,		&v_enabled_keva,
								&enabled_pvt_pharm,
								&v_DrugCode,			&v_gadget_code,
								&v_gadget_code,			&Member.MemberMaccabi,
								&Member.MemberHova,		&Member.MemberKeva,
								END_OF_ARG_LIST									);

					Conflict_Test (reStart);

					if (SQLERR_code_cmp (SQLERR_too_many_result_rows) == MAC_TRUE)
					{
						SetErrorVar (&SPres[i].DrugAnswerCode,	ERR_DUPLICATE_GADGET_ROW);
						SetErrorVar (&v_ErrorCode,				ERR_DRUG_S_WRONG_IN_PRESC);
//						GerrLogReturn (GerrId, "Duplicate gadget! Largo %d, Gadget Code %d, err %d.",
//									   v_DrugCode, v_gadget_code, SQLCODE);
						break;
					}
					else
					{
						// WORKINGPOINT: NEED TO GIVE PHARMACY SOMETHING MORE INFORMATIVE THAN "DATABASE ERROR" IN THIS CASE!
						// (DonR 12OCT2008)
						if (SQLERR_error_test ())
						{
							GerrLogReturn (GerrId, "Get gadget: Largo %d, Gadget Code %d, err %d.",
										   v_DrugCode, v_gadget_code, SQLCODE);
							SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR);
							break;
						}
					}

					// DonR 18Sep2011: If get_ptn_from_as400 flag from gadgets table is equal to zero,
					// pharmacy has chosen a "placeholder" service that is not actually supposed to get
					// its participation from AS/400. Make the rest of the "gadgets" code conditional
					// on this flag.
					// DonR 27Nov2011: Switch from get_ptn_from_as400 to new separate enabled-status flags
					// for Maccabi member and IDF soldiers. A "placeholder" service will have zeroes for
					// all three member types.
					// DonR 20Jan2014: Add a condition for pharmacy type. Maccabi pharmacies can sell all
					// "meishar" items; private pharmacies can sell (or at least can get participation
					// from the "meishar" application) only those items with their "private pharmacy
					// enabled" flag set > 0.
					if (((MACCABI_PHARMACY) || (enabled_pvt_pharm > 0))			&&
						(((v_enabled_mac	> 0) && (Member.MemberMaccabi	> 0))	||
						 ((v_enabled_hova	> 0) && (Member.MemberHova		> 0))	||
						 ((v_enabled_keva	> 0) && (Member.MemberKeva		> 0))))
					{
						// DonR 25Mar2012: For members buying stuff at VAT-exempt pharmacies (i.e. in Eilat),
						// take VAT off the unit price sent to AS/400. The AS/400 "meishar" application is
						// supposed to send a response that will reflect this reduction, and so we won't
						// have to deduct VAT later on.
						// DonR 14Nov2012: We no longer want to send the reduced "Maccabi" price to Meishar;
						// instead, we just send the ordinary unit price, with VAT deducted for Eilat pharmacies.
						// DonR 20Jan2014: For private pharmacies, send the price from Price List 2 (normally used
						// for Maccabi pharmacies) to Meishar as the default unit price.
						if (PRIVATE_PHARMACY)
						{
							ExecSQL (	MAIN_DB, READ_GetStandardMaccabiPharmPriceForMeishar,
										&Yarpa_Price,	&v_DrugCode,	END_OF_ARG_LIST			);

							// No error trapping - just disable the price substitution.
							if (SQLCODE != 0)
								Yarpa_Price = 0;
						}
						else
						{
							// For Maccabi pharmacies, we never want (or need) to do this substitution.
							Yarpa_Price = 0;
						}

						// DonR 20Jan2014: Use Yarpa Price if we're at a private pharmacy and we read it
						// successfully from Price List 2; otherwise use RetOpDrugPrice, which is the
						// normal Yarpa Price.
						UnitPrice = (Yarpa_Price > 0) ? Yarpa_Price : SPres[i].RetOpDrugPrice;	// DonR 14Nov2012.
						if (vat_exempt != 0)
						{
							UnitPrice = (int)(((double)UnitPrice * no_vat_multiplier) + .5001);
						}


						// Store this gadget's information in array. If another gadget in the
						// same transaction has the same Service Code, Service Number, and
						// Service Type, abort and return a transaction-level error to
						// the pharmacy.
						gadget_svc_code		[i]		= v_service_code;
						gadget_svc_number	[i]		= v_service_number;
						gadget_svc_type		[i]		= v_service_type;

						for (j = 0; j < i; j++)
						{
							if ((gadget_svc_code	[j] == gadget_svc_code	[i]) &&
								(gadget_svc_number	[j] == gadget_svc_number[i]) &&
								(gadget_svc_type	[j] == gadget_svc_type	[i]))
							{
								SetErrorVar (&v_ErrorCode, ERR_DRUG_S_WRONG_IN_PRESC);
								SetErrorVar (&SPres[i].DrugAnswerCode, ERR_DUPLICATE_GADGET_REQ);
								SPres[i].DFatalErr = MAC_TRUE;
								break;	// This is a severe error!
							}
						}


						// One more test before talking to the AS400: Did the pharmacy send
						// a non-zero value for Units? If so, return a severe error at the
						// treatment level, as quantity for Gadgets should be in OP only.
						if (SPres[i].Units > 0)
						{
							SetErrorVar (&v_ErrorCode, ERR_DRUG_S_WRONG_IN_PRESC);
							SetErrorVar (&SPres[i].DrugAnswerCode, ERR_GADGET_REQ_WITH_UNITS);
							SPres[i].DFatalErr = MAC_TRUE;
							break;	// This is a severe error!
						}


						// If we get here, the gadget entry really does exist, and there are no
						// major problems with the gadget sale request.
						err = as400EligibilityTest (v_PharmNum,
													v_RecipeIdentifier,
													v_MemberIdentification,
													v_IdentificationCode,
													(int)v_service_code,
													(int)v_service_number,
													(int)v_service_type,
													v_DrugCode,
													(int)SPres[i].Op,
													(int)UnitPrice,
													(int)2,		// Retries
													&l_dummy1,	// plPrescId
													&l_dummy2,	// plIdNumber
													&i_dummy,	// pnIdCode
													&QuantityPermitted,
													&RequestNum,
													&PtnPrice,
													&Insurance);

						GerrLogFnameMini ("gadgets_log",
										   GerrId,
										   "2003: AS400 err = %d.",
										   (int)err);

						flg = 0;	// Set to 1 in switch below if we've hit a fatal error.

						switch (err)
						{
							case enCommFailed:	// Communications error.
								SetErrorVar (&SPres[i].DrugAnswerCode, ERR_GADGET_COMM_PROBLEM);
								break;


							case enHasEligibility:		// Member is eligible.
								if (QuantityPermitted < SPres[i].Op)
								{
									// DonR 13Dec2010: Reduce OP *only* if the Quantity Permitted by AS/400
									// was *less* than the quantity requested.
									SPres[i].Op = QuantityPermitted;

									switch (Insurance)
									{
										case 1:		SetErrorVar (&SPres[i].DrugAnswerCode,
																 ERR_GADGET_OP_REDUCED_BASIC);		break;

										case 2:		SetErrorVar (&SPres[i].DrugAnswerCode,
																 ERR_GADGET_OP_REDUCED_MAGEN);		break;

										case 3:		SetErrorVar (&SPres[i].DrugAnswerCode,
																 ERR_GADGET_OP_REDUCED_KEREN);		break;

										case 7:		SetErrorVar (&SPres[i].DrugAnswerCode,
																 ERR_GADGET_OP_REDUCED_YAHALOM);	break;

										default:	SetErrorVar (&SPres[i].DrugAnswerCode,
																 ERR_GADGET_OP_REDUCED_UNKNOWN);	break;
									}
								}
								else
								{
									switch (Insurance)
									{
										case 1:		SetErrorVar (&SPres[i].DrugAnswerCode,
																 ERR_GADGET_APPROVED_BASIC);		break;

										case 2:		SetErrorVar (&SPres[i].DrugAnswerCode,
																 ERR_GADGET_APPROVED_MAGEN);		break;

										case 3:		SetErrorVar (&SPres[i].DrugAnswerCode,
																 ERR_GADGET_APPROVED_KEREN);		break;

										case 7:		SetErrorVar (&SPres[i].DrugAnswerCode,
																 ERR_GADGET_APPROVED_YAHALOM);		break;

										default:	SetErrorVar (&SPres[i].DrugAnswerCode,
																 ERR_GADGET_APPROVED_UNKNOWN);		break;
									}
								}

								// DonR 17Jul2016: The Meishar ("gadget") application can override participation
								// from AS/400 ishurim. In this case, we need to zero out the ishur number, so
								// that we don't confuse other parts of the system.
								if (SPres[i].ret_part_source.table == FROM_SPEC_PRESCS_TBL)
								{
									SPres[i].SpecPrescNum		= 0;
									SPres[i].SpecPrescNumSource	= 0;
									SPres[i].IshurTikraType		= 0;
								}
								
	//							// DonR 20Jul2010: AS/400 sends per-unit participation, NOT total participation.
	//
								// DonR 02Sep2004: If more than one unit is being sold and authorized,
								// AS400 sends us the total participation, not the per-unit participation.
								// We have to change the participation to a per-unit basis, since this
								// is what the pharmacy expects.
								if (QuantityPermitted > 1)
									PtnPrice /= QuantityPermitted;

								SPres[i].RetPartCode					= 1;
								SPres[i].PriceSwap						= PtnPrice;
								SPres[i].ret_part_source.table			= FROM_GADGET_APP;


								// DonR 25Dec2007: Per Iris Shaya, if we're getting participation
								// from the AS400 "Shaban" application, turn member discounts off.
								SPres[i].in_health_pack					= 0;

								// DonR 09Jan2012: On the other hand (see above), if "Meishar" has authorized
								// participation of zero, we want to force in a 100% discount to clarify matters
								// for pharmacy systems and the AS/400.
								if (PtnPrice == 0)
									SPres[i].AdditionToPrice = 10000;


								switch (Insurance)
								{
									case 1:
									case 2:
									case 3:
									case 7:		SPres[i].ret_part_source.insurance_used	= Insurance;
												break;

									default:	SPres[i].ret_part_source.insurance_used	= BASIC_INS_USED;
												break;
								}

								break;


							case enAlreadyPurchased:	// Member has already bought one of these gadgets.
								SetErrorVar (&SPres[i].DrugAnswerCode, ERR_GADGET_ALREADY_BOUGHT);
								break;

							case enNoAppropriateInsurance:	// Member is ineligible.
								SetErrorVar (&SPres[i].DrugAnswerCode, ERR_GADGET_NO_INSURANCE);
								break;

							case enPharmacyNotExist:
							case enUnknownPharmacy:
							case enCalculationError:
							case IdNumberNotFound:
								SetErrorVar (&SPres[i].DrugAnswerCode, ERR_GADGET_ERROR);
								break;

							case enPay2MonthLate:
							case IdNumberIndebted:
								SetErrorVar (&SPres[i].DrugAnswerCode, ERR_GADGET_DEADBEAT_MEMBER);
								break;

							case enNoAccordance:	// This is a show-stopper!
								SetErrorVar (&v_ErrorCode, ERR_DRUG_S_WRONG_IN_PRESC);
								SetErrorVar (&SPres[i].DrugAnswerCode, ERR_GADGET_REQ_DISCREPANCY);
								SPres[i].DFatalErr = MAC_TRUE;
								flg = 1;	// This is a severe error!
								break;

							case enNotEligible:
								SetErrorVar (&SPres[i].DrugAnswerCode, ERR_GADGET_NOT_ELIGIBLE);
								break;

							case Rejected_TrafficAccident:
								SetErrorVar (&SPres[i].DrugAnswerCode, ERR_GADGET_TRAFFIC_ACCIDENT);
								break;

							default:	// Strange error.
								SetErrorVar (&SPres[i].DrugAnswerCode, ERR_GADGET_ERROR);
								break;
						}

						// If we got an AS400 return code indicating a fatal problem, break out
						// of dummy loop.
						if (flg != 0)
							break;

					}	// DonR 18Sep2011: Need to query AS400 for "gadget" eligibility.

				}	// Item in gadgets table being bought with prescription at Maccabi pharmacy.



				// Member discount percent calculation.
				//
				// Member discounts apply if:
				//
				// A) The drugs are being bought with a prescription.
				//
				// B) The member is entitled to a discount: Either by having Member Rights 7 or 17
				//    (entitled to a 100% discount) or with an explicit discount level set by
				//    Member Discount Percent.
				//    DonR 04Dec2014: Member can also get an automatic 100% discount if s/he has
				//    a match between his/her serious-illness bitmap and the drug's illness bitmap.
				//    This is implemented with the new macro GETS_100PCT_DISCOUNT.
				//
				// C) Drug is in Health Pack (i.e. "The Basket") - this can be set from drug_list,
				//    drug_extension, spclty_largo_prcnt, or an AS400 ishur.
				//    WORKINGPOINT: Should we be getting in_health_pack from the preferred drug
				//    when "participation swapping" is going on?
				//    DonR 03Jan2008: Yes, we should!
				//
				// D) The drug's Largo Type is equal to 'T' or 'M'.
				//
				// DonR 29Dec2014: In order to provide a message to pharmacies when a member with a
				// serious illness has to pay for prescribed drugs that are not relevant to that illness,
				// change the order of the logic a bit. The new macro MEMBER_GETS_DISCOUNTS tells us
				// that the member is entitled to discounts at least some of the time, but may not get
				// a discount on any particular medication.
				// DonR 11Feb2015: If we are actually selling a non-preferred drug under "preferred" conditions, the
				// preferred drug's in-health-basket flag will already have been copied to the regular in_health_pack
				// variable; and if we're *not* using the preferred drug's conditions (e.g. if we found a rule
				// of some sort that gives less than 100% participation for the non-preferred drug), we don't want
				// to use the preferred drug's in-health-basket flag. Accordingly, we want to test only the "regular"
				// health-basket flag for Condition C!
				// DonR 24Mar2015: For the moment, we're backing out the change from 11Feb2015.
				// DonR 30Jun2015: Restoring the change from 11Feb2015.
				// DonR 28Mar2016: Add capability for bypassing the health-basket requirement for particular illnesses.
				// DonR 13May2020 CR #31591: Use Largo Type lists from Sysparams to qualify discounts instead of
				// hard-coded values. For flexibility, use separate lists for illness-based and non-illness-based discounts,
				// as well as the new category of "ventilator" discounts. The comparision between a drug's Largo Type and these
				// lists is performed in read_drug() and stored in DL.IllnessDiscounts/VentilatorDiscounts/NonIllnessDiscounts.
				if ( (v_RecipeSource != RECIP_SRC_NO_PRESC)												&&	// Condition (A).
					 (v_RecipeSource != RECIP_SRC_EMERGENCY_NO_RX)										&&	// Condition (A).
					 (MEMBER_GETS_DISCOUNTS)															&&	// Condition (B).
					 ((SPres[i].in_health_pack != 0)	|| (IGNORE_HEALTH_BASKET (SPres[i].DL)))		&&	// Condition (C).
					 ((SPres[i].DL.IllnessDiscounts)	|| (SPres[i].DL.VentilatorDiscounts) || (SPres[i].DL.NonIllnessDiscounts)) )	// Condition (D) - revised by CR #31591.
				{
					// DonR 07Aug2003: Per Iris Shaya, deleted the condition that would skip the
					// discount for a drug with DFatalErr set TRUE. This was done because we want
					// to see discounts even if there's been an overdose/interaction detected.
					//
					// DonR 03Feb2004: If there is a Special Prescription, the drug's in_health_pack flag
					// has already been set based on the value in the special_prescs table. If the Special
					// Prescription specifies that the drug is not in the health basket, no discount
					// should be given.
					//
					// DonR 25Dec2007: Per Iris Shaya, member discounts are no longer conditioned
					// on any particular participation code.
					//
					//
					// DonR 11Feb2003: In all cases other than MemberRights = 7 or 17 or a bitmap match
					// (i.e. 100% discount), the level of discount will be stored in
					// Member Discount Percent.
					//
					// DonR 13May2020 CR #31591: Add a new category of "ventilator" discounts. These are
					// given for members with their VentilatorDiscount flag set non-zero (currently comes
					// from members/asaf_code, which should be renamed when we migrate to MS-SQL), for
					// all items with Largo Type matching a list - currently "B", "Y", or "D".
					if (((Member.VentilatorDiscount) && (SPres[i].DL.VentilatorDiscounts))	||
						(GETS_100PCT_DISCOUNT (SPres[i].DL)))
					{
						SPres[i].AdditionToPrice = 10000;
					}
					else	// No 100% discount but member *does* have a discount percentage set.
					{
						// Marianna 24Mar2022 Epic 232192: Darkonaim, Type 2 (Harel Foreign Workers)
						if ((Member.mem_id_extension == 9) && (Member.darkonai_type == 2))
						{
							Tmember_price_row	PriceRow;

							GET_MEMBER_PRICE_ROW (SPres[i].RetPartCode, &PriceRow);
						
							// Marianna 24Mar2022 Epic 232192: Type 2 Darkonaim get a 100% discount if the participation
							// percentage is less or equal to a configurable value - currently 15%.
							if ((PriceRow.member_price_prcnt) <= (DarkonaiMaxHishtatfutPct))
							{
								SPres[i].AdditionToPrice = 10000; // 100% discount
							} // Get 100% discount darkonai
						}
					
						// DonR 12Jan2004: Make sure we don't overwrite a big discount with a little one.
						// DonR 13May2020 CR #31591: For flexibility, make this discount conditional on
						// the drug's NonIllnessDiscounts flag, which is set by read_drug() based on whether
						// the drug's Largo Type matches the list set in sysparams/memb_disc_4_types.
						if ((SPres[i].DL.NonIllnessDiscounts) && (Member.discount_percent > SPres[i].AdditionToPrice))
							SPres[i].AdditionToPrice = Member.discount_percent;
					}	// else - member has a discount percentage set.

				}	// Member may be entitled to a discount.


				// If participation is 100% (without Fixed Price) after all our machinations, AND
				// better specialist participation is allowed for this drug and member's insurance
				// type, send a warning message to pharmacy.
				// DonR 16May2006: Kishon Divers (who are supposed to get their medications
				// direct from the government) pay 100% unconditionally. This means that
				// they will get participation code 1 (= 100%) even for drugs that everyone
				// else pays 15% for. We don't want to send warning messages in this case!
				//
				// DonR 01Apr2008: Per Iris Shaya, do not send this error code for purchases
				// of dentist- or home-visit-prescribed drugs.
				// DonR 09Jan2012: Replaced three conditions with macro FULL_PRICE_SALE.
				// DonR 12Jun2012: New macro FULL_PRICE_NO_ISHUR_ETC with some additional conditions.
				if ((FULL_PRICE_NO_ISHUR_ETC(i))												&&	// DonR 12Jun2012
					(SPres[i].DrugAnswerCode		== NO_ERROR)								&&
					((SPres[i].DL.specialist_drug	>  0) || (PR_AsIf_Preferred_SpDrug[i] > 0))	&&
					(v_RecipeSource					!= RECIP_SRC_DENTIST)						&&
					(v_RecipeSource					!= RECIP_SRC_HOME_VISIT))
				{
					ExecSQL (	MAIN_DB, READ_test_for_ERR_SPCLTY_LRG_WRN,
								&RowsFound,

								&SPres[i].DrugCode,				&PR_AsIf_Preferred_Largo[i],
								&Member.MemberMaccabi,			&Member.MemberHova,
								&Member.MemberKeva,				&ROW_NOT_DELETED,
								&Member.insurance_type,			&Member.current_vetek,
								&Member.prev_insurance_type,	&Member.prev_vetek,
								END_OF_ARG_LIST													);

					Conflict_Test (reStart);

					if (SQLERR_error_test ())
					{
						SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR);
						break;
					}
					else
					{
						if (RowsFound > 0)
						{
							SetErrorVar (&SPres[i].DrugAnswerCode, ERR_SPCLTY_LRG_WRN);
						}
					}
				}



				// DonR 28Mar2004: Per Iris Shaya, if member could get a discount by going to a Maccabi
				// pharmacy, send a warning message.
				// DonR 23Mar2011: Use the drug_list rule_status flag so we don't do a table lookup for
				// drugs that aren't in drug_extension.
				// DonR 09Jan2012: Replaced three conditions with macro FULL_PRICE_SALE.
				// DonR 12Jun2012: New macro FULL_PRICE_NO_ISHUR_ETC with some additional conditions.
				if ((FULL_PRICE_NO_ISHUR_ETC(i))											&&	// DonR 12Jun2012
					(SPres[i].DrugAnswerCode		== NO_ERROR)							&&
					((SPres[i].DL.rule_status		>  0) || (PR_AsIf_RuleStatus[i] > 0)))		// DonR 23Mar2011
				{
					// DonR 22Mar2011: In order to avoid checking the drug_extension table three times
					// when there are no rules applicable to this member, perform an extra lookup just
					// to see if there is something there. In theory at least, this should reduce the
					// average number of lookups.
					ExecSQL (	MAIN_DB, READ_test_for_SomeRuleApplies,
								&RowsFound,
								&SPres[i].DrugCode,		&PR_AsIf_Preferred_Largo[i],
								&Member.age_months,		&Member.age_months,
								&v_MemberGender,		&SysDate,
								&Member.MemberMaccabi,	&Member.MemberHova,
								&Member.MemberKeva,		END_OF_ARG_LIST					);

					// Don't bother with error-checking here; just say that if we got a valid response
					// from SQL *and* no rows were found, we set the flag FALSE, otherwise we continue
					// with the "real" table lookups, which do have error-checking.
					SomeRuleApplies = ((SQLCODE == 0) && (RowsFound == 0)) ? 0 : 1;
					

					if (SomeRuleApplies)
					{
						// Check for available Maccabi Pharmacy discount.
						ExecSQL (	MAIN_DB, READ_test_for_ERR_DISCOUNT_AT_MAC_PH_WARN,
									&RowsFound,

									&SPres[i].DrugCode,				&PR_AsIf_Preferred_Largo[i],
									&Member.age_months,				&Member.age_months,
									&v_MemberGender,				&Phrm_info.pharmacy_permission,
									&Phrm_info.pharmacy_permission,	&ROW_NOT_DELETED,
									&Member.MemberMaccabi,			&Member.MemberHova,
									&Member.MemberKeva,				&SysDate,
									&Member.insurance_type,			&Member.current_vetek,
									&Member.prev_insurance_type,	&Member.prev_vetek,
									END_OF_ARG_LIST														);

						Conflict_Test (reStart);

						if (SQLERR_error_test ())
						{
							SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR);
							break;
						}
						else
						{
							if (RowsFound > 0)
								SetErrorVar (&SPres[i].DrugAnswerCode, ERR_DISCOUNT_AT_MAC_PH_WARN);
						}
					}	// At least one drug_extension rule is applicable to this member.
				}	// Full-price sale, no errors, etc., and there is a "nohal" for this drug.

				// DonR 12Jun2012: If this sale is discounted, an AS/400 ishur is being used, the
				// member is a Kishon diver, a fatal error has occurred for this drug, or the drug
				// doesn't have any nohalim, set the flag SomeRuleApplies to zero so that we don't
				// have to make the same tests again.
				// DonR 09Aug2021 User Story #163882: Another condition that applies is where the
				// purchaser is a darkonai-plus who doesn't receive any discounts other than "shovarim".
				else
				{
					SomeRuleApplies = 0;	// We know no rules apply even without reading drug_extension.
				}



				// DonR 11May2004: Per Iris Shaya, if member could get a discount according to some
				// rule with Authorizing Authority between 1 and 49, send a warning message.
				// DonR 11Aug2005: Per Iris Shaya, change the confirm_authority select
				// to examine only rules for low-level authority (less than 25).
				// DonR 14May2007: If member has an Ishur with "tikra", we don't need to
				// give this message.
				//
				// DonR 01Apr2008: Per Iris Shaya, do not give this warning message
				// for purchases of dentist- or home-visit-prescribed drugs.
				// DonR 23Mar2011: Use the drug_list rule_status flag so we don't do a table lookup for
				// drugs that aren't in drug_extension.
				// DonR 27Dec2011: This message applies to all ishur possibilities - so widen the
				// confirm_authority check to include everything greater than zero.
				// DonR 09Jan2012: Replaced three conditions with macro FULL_PRICE_SALE (folded into SomeRuleApplies).
				// DonR 13Jun2012: Give different messages for different forms of available ishur.
				if ((SomeRuleApplies)														&&	// DonR 12Jun2012
					(v_RecipeSource					!= RECIP_SRC_DENTIST)					&&
					(v_RecipeSource					!= RECIP_SRC_HOME_VISIT))
				{
					ErrorCodeToAssign = 0;	// Default: no ishur discount possible.

					// First, see if there is a pharmacy-ishur-level rule available to the selling pharmacy,
					// based on its permission type.
					ExecSQL (	MAIN_DB, READ_test_for_discount_with_pharmacy_ishur,
								&RowsFound,

								&SPres[i].DrugCode,					&PR_AsIf_Preferred_Largo[i],
								&Member.age_months,					&Member.age_months,
								&v_MemberGender,					&SysDate,
								&ROW_NOT_DELETED,					&Member.MemberMaccabi,
								&Member.MemberHova,					&Member.MemberKeva,
								&Phrm_info.pharmacy_permission,		&Phrm_info.pharmacy_permission,
								&Phrm_info.pharmacy_permission,		&Member.insurance_type,
								&Member.current_vetek,				&Member.prev_insurance_type,
								&Member.prev_vetek,					END_OF_ARG_LIST						);

					Conflict_Test (reStart);

					if (SQLERR_error_test ())
					{
						SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR);
						break;
					}
					else
					{
						if (RowsFound > 0)	// Found a pharmacy-ishur rule for this pharmacy's permission type.
						{
							switch (Phrm_info.pharmacy_permission)
							{
								case 1:		ErrorCodeToAssign = ERR_DISCOUNT_WITH_PH_ISHUR_WRN;	break;
								case 6:		ErrorCodeToAssign = ERR_ISHUR_DISCNT_PRATI_PLUS;	break;
								default:	ErrorCodeToAssign = ERR_UNSPEC_ISHUR_DISCNT_AVAIL;	break;
							}

						}

						else
						// If no pharmacy-ishur rule was found for the selling pharmacy's permission type,
						// perform a more general check that includes all pharmacy types and all confirmation-
						// authority levels.
						{
							ExecSQL (	MAIN_DB, READ_test_for_ERR_UNSPEC_ISHUR_DISCNT_AVAIL,
										&RowsFound,

										&SPres[i].DrugCode,				&PR_AsIf_Preferred_Largo[i],
										&Member.age_months,				&Member.age_months,
										&v_MemberGender,				&SysDate,
										&ROW_NOT_DELETED,				&Member.MemberMaccabi,
										&Member.MemberHova,				&Member.MemberKeva,
										&Member.insurance_type,			&Member.current_vetek,
										&Member.prev_insurance_type,	&Member.prev_vetek,
										END_OF_ARG_LIST													);

							Conflict_Test (reStart);

							if (SQLERR_error_test ())
							{
								SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR);
								break;
							}
							else
							{
								if (RowsFound > 0)	// Found a rule.
								{
									ErrorCodeToAssign = ERR_UNSPEC_ISHUR_DISCNT_AVAIL;
								}
							}	// No database error looking for general non-automatic rules.
						}	// No pharmacy-ishur rule was found.

						// Finally, if we found something in either query, report the result to pharmacy.
						if (ErrorCodeToAssign > 0)
							SetErrorVar (&SPres[i].DrugAnswerCode, ErrorCodeToAssign);

					}	// No database error checking drug_extension table for pharmacy-ishur rules.
				}	// Some rule applies for the member/drug and prescription source is not dentist/home visit.



				// DonR 28Mar2004: Per Iris Shaya, if member could have gotten a discount by
				// signing up for Magen Kesef or Magen Zahav, send a warning message to that effect.
				// DonR 11Aug2005: Per Iris Shaya, change the confirm_authority select
				// to examine only rules for low-level authority (less than 25).
				// DonR 23Mar2011: Use the drug_list rule_status flag so we don't do a table lookup for
				// drugs that aren't in drug_extension.
				//
				// DonR 14Nov2011: Because Magen Kesef is being "turned off" (except for a few members who still
				// have Kesef and not Zahav), don't send the "discount if you weren't such a cheapskate" message
				// if the only rule that would give the discount is a Kesef rule.
				// DonR 20Dec2011: Per Iris Shaya, give this message only for "automatic" rules with
				// confirm_authority == 0.
				// DonR 09Jan2012: Replaced three conditions with macro FULL_PRICE_SALE (folded into SomeRuleApplies).
				// DonR 06Dec2012 "Yahalom": Changed SQL to reflect new "insurance_type" field; also, there is no
				// point in testing member's insurance status in SQL, since it's faster to check it in normal C code.
				// DonR 11Dec2012 "Yahalom": Add a new message (6149) if a "Yahalom" rule exists that would be
				// applicable. Only if no Yahalom rules are found will we check for Zahav rules.
				if ((SomeRuleApplies) && (Member.InsuranceType != 'Y'))
				{
					strcpy (InsuranceTypeToCheck, "Y");

					ExecSQL (	MAIN_DB, READ_test_for_Maccabi_Sheli_or_Zahav_rule,
								&RowsFound,				&PossibleInsuranceType,

								&SPres[i].DrugCode,		&PR_AsIf_Preferred_Largo[i],
								&Member.age_months,		&Member.age_months,
								&v_MemberGender,		&SysDate,
								&ROW_NOT_DELETED,		&Member.MemberMaccabi,
								&Member.MemberHova,		&Member.MemberKeva,
								&InsuranceTypeToCheck,	END_OF_ARG_LIST					);


					Conflict_Test (reStart);

					if (SQLERR_error_test ())
					{
						SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR);
						break;
					}

					// If we got here, we know that member doesn't have Yahalom. If (A) no Yahalom
					// rules were found, and (B) member doesn't have Zahav either, check for applicable
					// Zahav rules. Note that we don't need to check for previous Zahav insurance, since
					// the only members for whom previous Zahav insurance is relevant are those with Yahalom.
					if ((RowsFound < 1) && (Member.InsuranceType != 'Z'))
					{
						strcpy (InsuranceTypeToCheck, "Z");

						ExecSQL (	MAIN_DB, READ_test_for_Maccabi_Sheli_or_Zahav_rule,
									&RowsFound,				&PossibleInsuranceType,

									&SPres[i].DrugCode,		&PR_AsIf_Preferred_Largo[i],
									&Member.age_months,		&Member.age_months,
									&v_MemberGender,		&SysDate,
									&ROW_NOT_DELETED,		&Member.MemberMaccabi,
									&Member.MemberHova,		&Member.MemberKeva,
									&InsuranceTypeToCheck,	END_OF_ARG_LIST					);

						Conflict_Test (reStart);

						if (SQLERR_error_test ())
						{
							SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR);
							break;
						}
					}	// No Yahalom rules were found, and member doesn't have Zahav.

					// If at least one applicable rule was found, send the appropriate message.
					if (RowsFound > 0)
						SetErrorVar (&SPres[i].DrugAnswerCode,
									 (*PossibleInsuranceType == 'Y') ? ERR_DISCOUNT_IF_HAD_YAHALOM : ERR_DISCOUNT_IF_HAD_ZAHAV);

				}	// Some rule applies AND member doesn't have Yahalom.


				// 11Dec2003: Per Iris Shaya, substituting the Maccabi Price is a last resort.
				// Therefore, we should do it *after* checking for Pharmacy Ishur.

				// 25Mar2003: Per Iris Shaya, the logic for use of Maccabi Price is changing.
				// For normal participation calculation, we will always use the Yarpa
				// (i.e. regular) price; at the end, we'll use the Maccabi Price as
				// a fixed participation price if:
				//     A) A Maccabi Price exists for this drug in the pharmacy's default price list;
				//     B) Participation is 100%; and
				//     C) No fixed participation has been set from other sources, such as
				//        Drug-Extension.
				// Note that we're assuming that the Maccabi Price will be less than the
				// regular price!

				// 18Apr2004: This logic has to happen *before* the check for medicines that can
				// be sold only with a Special Prescription, since the Maccabi Price is a fixed
				// price that "turns off" the Special Prescription requirement check.
				// DonR 09Jan2012: Replaced three conditions with macro FULL_PRICE_SALE.
				// DonR 16Dec2013: If this is a sale of a "treatment"-type drug with a
				// prescription, at a Maccabi pharmacy, to a Maccabi member, Maccabi-price
				// discounts are disabled.
				// DonR 24Dec2014: We no longer disable Maccabi Price for prescription sales
				// to Maccabi members at Maccabi pharmacies; this backs out the change from
				// 16Dec2013.
				if ((SPres[i].MacabiDrugPrice > 0) && (FULL_PRICE_SALE(i)))
				{
					SPres[i].PriceSwap				= SPres[i].MacabiDrugPrice;
					SPres[i].MacabiPriceFlg			= MAC_TRUE;
					SPres[i].ret_part_source.table	= FROM_MACCABI_PRICE;
				}


				// DonR 28Jun2016: Add capability of getting a discounted price from the sale
				// tables sale/sale_bonus_recv.
				// DonR 11Jul2016: Re-enabled this feature, controlled by the new global flag
				// UseSaleTableDiscounts, set from sysparams/use_sale_tables.
				if ((MACCABI_PHARMACY)																	&&
					(UseSaleTableDiscounts)																&&
					(v_RecipeSource			!= RECIP_SRC_NO_PRESC)										&&
//					(v_RecipeSource			!= RECIP_SRC_EMERGENCY_NO_RX)								&&
					((FULL_PRICE_SALE(i))	|| (SPres[i].ret_part_source.table == FROM_MACCABI_PRICE)))
				{
					// Set up the current OP price as a default.
					Yarpa_Price		= (SPres[i].PriceSwap > 0) ? SPres[i].PriceSwap : SPres[i].RetOpDrugPrice;
					v_DrugCode		= SPres[i].DrugCode;

					ExecSQL (	MAIN_DB, READ_price_from_sale_bonus_recv,
								&Macabi_Price,
								&v_DrugCode,		&Yarpa_Price,
								&SysDate,			&SysDate,
								END_OF_ARG_LIST								);

					Conflict_Test (reStart);

					if (SQLCODE == 0)
					{
						SPres[i].PriceSwap				= Macabi_Price;
						SPres[i].MacabiPriceFlg			= MAC_TRUE;
						SPres[i].ret_part_source.table	= FROM_MACCABI_PRICE;
					}
				}


				// 21Dec2006: Per Iris Shaya, if after everything else the member is still
				// paying 100% with no fixed-price reduction, AND the sale is at a Maccabi
				// pharmacy, AND we aren't already working with the MaccabiCare price (i.e.
				// the price-list code we already read is *not* 800), AND a MaccabiCare price
				// exists, then use it.
				// 23Feb2008: Moved all the Price List 800 stuff down here. We use the
				// Maccabi Price from this price list if:
				// A) We haven't found any other discount;
				// B) The selling pharmacy and the drug are both in the Maccabicare program.
				// C) The sale is at a Maccabi pharmacy, OR it's a prescription sale (in
				//    which case the type of pharmacy is irrelevant).
				// In the special case of a non-prescription sale at a private pharmacy,
				// we also use the Maccabi price from Price List 800 - but in this case we
				// treat it as the "normal" drug price, and return Participation Code 97.
				// DonR 07Apr2008: To avoid unnecessary database lookups, added an
				// additional condition here. If a non-Maccabi member is purchasing
				// a drug without a prescription at a non-Maccabi pharmacy, all the
				// Maccabicare logic is irrelevant.
				//
				// DonR 02Aug2009: Per Iris Shaya, the Maccabicare logic now applies to both
				// Maccabi members and non-Maccabi members, at all private pharmacies.
				// DonR 09Jan2012: Replaced three conditions with macro FULL_PRICE_SALE.
				// DonR 16Dec2013: If this is a sale of a "treatment"-type drug with a
				// prescription, at a Maccabi pharmacy, to a Maccabi member, Maccabi-price
				// discounts are disabled.
				// DonR 24Dec2014: We no longer disable Maccabi Price for prescription sales
				// to Maccabi members at Maccabi pharmacies; this backs out the change from
				// 16Dec2013.
				// DonR 28Jun2016: Add global parameter to enable/disable Price List 800
				// lookup. For now at least, this flag will always be zero, but this could
				// change in the future.
				// DonR 11Jul2016: "Flipped" the global parameter - if UseSaleTableDiscounts is
				// set true, we *don't* want to look up Maccabi Price in Price List 800. The
				// parameter is now set from sysparams/use_sale_tables.
				// DonR 17Feb2019 CR #28228: In fact, the global UseSaleTableDiscounts parameter is irrelevant
				// to Price List 800 lookups. Also, read the Maccabi Price from Price List 800
				// only if it's less than the default price.
				if ((FULL_PRICE_SALE(i))													&&	// DonR 09Jan2012
					(maccabicare_pharm				!= 0)									&&
					(SPres[i].DL.maccabicare_flag	!= 0))
//				if ((!UseSaleTableDiscounts)												&&	// DonR 28Jun2016
//					(FULL_PRICE_SALE(i))													&&	// DonR 09Jan2012
//					(maccabicare_pharm				!= 0)									&&
//					(SPres[i].DL.maccabicare_flag	!= 0))
				{
					v_DrugCode	= SPres[i].DrugCode;
					Yarpa_Price	= SPres[i].RetOpDrugPrice;	// This gets overwritten if we read something, but that's harmless.

					ExecSQL (	MAIN_DB, READ_PriceList, READ_PriceList_800_conditional,
								&Yarpa_Price,	&Macabi_Price,		&Supplier_Price,
								&v_DrugCode,	&Yarpa_Price,		END_OF_ARG_LIST		);

					Conflict_Test (reStart);

					if (SQLCODE == 0)
					{
						SPres[i].YarpaDrugPrice			= Yarpa_Price;
						SPres[i].MacabiDrugPrice		= Macabi_Price;
						SPres[i].SupplierDrugPrice		= Supplier_Price;
						SPres[i].ret_part_source.table	= FROM_MACCABI_PRICE;
						// (Is the last line correct for private-pharmacy
						// non-prescription sales?

						if ((MACCABI_PHARMACY) ||
							((v_RecipeSource != RECIP_SRC_NO_PRESC) && (v_RecipeSource != RECIP_SRC_EMERGENCY_NO_RX)))
						{
							// "Conventional" situation: use Maccabicare price from
							// Price List 800 as a discounted fixed price.
							SPres[i].RetOpDrugPrice			= Yarpa_Price;
							SPres[i].PriceSwap				= Macabi_Price;
							SPres[i].MacabiPriceFlg			= MAC_TRUE;
							SPres[i].RetPartCode			= PARTI_CODE_MACCABICARE;	// = 17.
						}
						else
						{
							// Private pharmacy non-prescription Maccabicare sale.
							SPres[i].RetPartCode = PARTI_MACCABICARE_PVT;	// = 97.

							// DonR 05Aug2009: Bug fix - even though everyone gets Price Code 97
							// for these sales, only Maccabi members get the special Maccabi price.
							if (v_MemberBelongCode == MACABI_INSTITUTE)	// DonR 05Aug2009.
							{
								SPres[i].RetOpDrugPrice = Macabi_Price;
							}
						}
					}


					if (SQLERR_code_cmp (SQLERR_not_found) != MAC_TRUE)
					{
						if (SQLERR_error_test ())
						{
							SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR);
							break;
						}
					}
				}	// Need to check for Price-list 800 price.
				

				// DonR 04Dec2011: If the person making the purchase is a soldier and participation is 100%
				// (even if we're applying the Maccabi Price discount), send a warning message so pharmacist
				// will tell the soldier that s/he has to pay for the medication.
				// DonR 08Jan2012: If "Meishar" has given a fixed price of zero, that's a real
				// fixed price - so don't give this message.
				if ((v_RecipeSource					!= RECIP_SRC_NO_PRESC)								&&
					(Member.MemberTzahal			>  0)												&&
					(SPres[i].RetPartCode			== 1)												&&
					(SPres[i].ret_part_source.table	!= FROM_GADGET_APP)									&&	// DonR 08Jan2012
					((SPres[i].PriceSwap			== 0) || (SPres[i].ret_part_source.table == FROM_MACCABI_PRICE)))
				{
					SetErrorVar (&SPres[i].DrugAnswerCode, WARN_TZAHAL_NO_DISCOUNT);
				}


				// DonR 25Mar2004: Per Iris Shaya, if participation is 100% (without Fixed Price)
				// after all our machinations, AND no specialist participation exists for this drug,
				// AND NO RULES EXIST for low-level authorizing  authorities for this drug, AND one
				// or more rules exist which would allow some unspecified member to get a Special
				// Prescription for this drug, send an error message to pharmacy. This combination of
				// circumstances indicates that this drug is not available without an AS400 Ishur,
				// even if the member is willing to pay 100%.
				// DonR 11Aug2005: Per Iris Shaya, change the confirm_authority select
				// to examine only rules for low-level authority (less than 25).
				//
				// DonR 26Apr2007: Per Iris Shaya, changed conditions for error 6033. We
				// don't need to check drug_extension or spclty_largo_prcnt any more; we
				// just have to check the following conditions:
				// 1) 100% participation.
				// 2) No special price.
				// 3) No AS400 ishur with "tikra" flag set for this member/drug.
				// 4) Drug has "ishur required" flag set to 9.
				//
				// DonR 13Jul2011: Per Iris Shaya, changed criterion #3 (see above) so that an
				// AS/400 ishur without its "tikra" flag set *and* without any price reduction
				// can still enable the sale of a drug.
				// DonR 09Jan2012: Replaced three conditions with macro FULL_PRICE_SALE.
				if ((FULL_PRICE_SALE(i))										&&	// DonR 09Jan2012
					(SPres[i].ret_part_source.table	!= FROM_SPEC_PRESCS_TBL)	&&
					(SPres[i].DL.ishur_required		== 9))
				{
					SetErrorVar (&SPres[i].DrugAnswerCode, ERR_ISHUR_POSSIBLE_ERR);
				}

				// DonR 04Dec2013: Maccabi pharmacies are not allowed to sell "treatment" drugs
				// with presccription to Maccabi members at full price.
				// DonR 26Dec2013: If the sale is being rejected because the person buying drugs isn't a Maccabi
				// member, don't bother giving the "100% Forbidden" error.
				// DonR 24Dec2014: Give this error only for drugs that require a prescription.
				// DonR 09Aug2021 User Story #163882: Darkonai-Plus purchasers who are not supposed to get any discounts
				// (other than shovarim provided by their insurance company) are allowed to buy drugs at 100% from
				// Maccabi pharmacies.
				if ((MACCABI_PHARMACY)												&&
					(v_MemberBelongCode				== MACABI_INSTITUTE)			&&
					(v_ErrorCode					!= ERR_MEMBER_NOT_ELEGEBLE)		&&	// DonR 26Dec2013
					(v_RecipeSource					!= RECIP_SRC_NO_PRESC)			&&
					(v_RecipeSource					!= RECIP_SRC_EMERGENCY_NO_RX)	&&
					(SPres[i].DL.largo_type			== 'T')							&&
					(SPres[i].DL.no_presc_sale_flag	== MAC_FALS)					&&	// DonR 24Dec2014
					(SPres[i].DFatalErr				== 0)							&&
					(SPres[i].ret_part_source.table	!= FROM_SPEC_PRESCS_TBL)		&&
					(Member.force_100_percent_ptn	== 0)							&&	// DonR 09Aug2021 User Story #163882
					(FULL_PRICE_SALE(i)))
				{
				    SetErrorVarF (&SPres[i].DrugAnswerCode, ERR_MACCABI_100PCT_FORBIDDEN, &SPres[i].DFatalErr);
					flg = 1;	// So we'll prevent the whole sale from going through.
				}


				// DonR 29Jun2006: If necessary, check Drug Purchase Limits.
				// Note that if the member has an AS400 Special Prescription
				// with a quantity limit for this drug, that's enough - we
				// don't need to check the Drug Purchase Limit as well.
				//
				// First, mark all drugs which are being bought at some kind
				// of discount; this allows test_purchase_limits() to run
				// faster, since it doesn't have to make the same checks
				// multiple times.
				if (i == 0)	// Do this only once!
				{
					for (is = 0; is < v_NumOfDrugLinesRecs; is++)
					{
						// Just to be paranoid, repeat the default initialization.
						SPres[is].BoughtAtDiscount = MAC_FALS;

						// DonR 08Jan2012: If "Meishar" has given a fixed price of zero, that's a real
						// fixed price.
						// DonR 07Apr2013: Per Iris Shaya, drugs that got their participation from an AS/400
						// ishur are considered "discounted" even if they were bought at full price. (This
						// happens, for example, when they are subject to an "ishur with tikra".)
						if (((SPres[is].PriceSwap > 0) && (SPres[is].MacabiPriceFlg == 0))	||
							( SPres[is].ret_part_source.table == FROM_SPEC_PRESCS_TBL)		||	// DonR 07Apr2013
							( SPres[is].ret_part_source.table == FROM_GADGET_APP))				// DonR 08Jan2012
						{
							// Member got a fixed price other than Maccabi Price.
							SPres[is].BoughtAtDiscount = MAC_TRUE;
						}
						else
						{
							// If member paid 100% and didn't get a "real" discounted
							// fixed price, this drug was not bought at discount.
							switch (SPres[is].RetPartCode)
							{
								case  1:
								case 11:
								case 19:
								case 50:
								case PARTI_MACCABICARE_PVT:
								case 98:
											break;

								// Any other price code indicates some kind of discount.
								default:
											SPres[is].BoughtAtDiscount = MAC_TRUE;
											break;
							}	// Switch on member_price_code.
						}	// Member didn't get a discounted fixed price.
					}	// Loop through drugs to see which ones are being bought at a discount.
				}	// Execute inner loop only once.

				// If this drug needs to have Purchase Limit checking performed, go do it.
				// DonR 23Aug2010: Per Iris Shaya, Drug Purchase Limits should not be checked for those
				// non-Maccabi "members" in the membernotcheck table.
				// DonR 01Dec2016: Added macro to implement the option of exempting ill members from
				// certain drug purchase limits. This is passed to test_purchase_limits(), since
				// prescription-source validation applies even if  "real" quantity limits do not.
				// DonR 14Dec2021 User Story #205423: If the drug is being purchased without a
				// prescription, test purchase limits without regard for whether a discount is being
				// applied. This is because new Method 7/8 limits exist for safety rather than
				// economic purposes.
				// DonR 23Jul2025 User Story #427783: Now there is also a "check limits including
				// full-price purchases" option at the drug_purchase_lim level, to support new
				// safety restrictions on opioids and other dangerous drugs.
				if (( SPres[i].DL.purchase_limit_flg > 0)	&&
					( !SPres[i].HasIshurWithLimit)			&&
					( Member.check_od_interact)				&&	// DonR 24Jan2018 CR #13937
					(	(SPres[i].BoughtAtDiscount					)	||
						(SPres[i].PrescSource == RECIP_SRC_NO_PRESC	)	||
						(SPres[i].PurchaseLimitIncludeFullPrice		)	||	// DonR 23Jul2025 User Story #427783
						(SPres[i].DL.purchase_lim_without_discount	)	)	)
//					((SPres[i].BoughtAtDiscount) || (SPres[i].PrescSource == RECIP_SRC_NO_PRESC) || (SPres[i].DL.purchase_lim_without_discount)))
				{
					flg = test_purchase_limits (&SPres[i],
												&SPres[0],
												v_NumOfDrugLinesRecs,
												&Member,
												MACCABI_PHARMACY,
												OVERSEAS_MEMBER,
												EXEMPT_FROM_LIMIT,
												v_PharmNum,
												&FunctionError);
				}


				// DonR 05Mar2017: If necessary, reject this drug line based on its Permit Sales flag
				// in the prescr_source table, possibly modified by an applicable Method 2 limit from
				// drug_purchase_lim.
				if (SPres[i].PurchaseLimitSourceReject)
				{
//					SetErrorVarArr (&presdrugs->DrugAnswerCode, DRUG_RX_SOURCE_MEMBER_FORBIDDEN, presdrugs->DrugCode, LineNo, &presdrugs->DFatalErr, NULL);
					SetErrorVar (&SPres[i].DrugAnswerCode, DRUG_RX_SOURCE_MEMBER_FORBIDDEN);
				}


				// DonR 17Jan2006: Per Iris Shaya, check for early refills.
				// DonR 21Mar2006: If this drug is being sold according to an AS400 Ishur
				// with quantity limits, there is no need to check for early refill as well.
				// DonR 23Aug2010: Per Iris Shaya, don't check for early refills for those "members"
				// in the membernotcheck table.
				if ((MACCABI_PHARMACY)										&&
					(v_RecipeSource			!= RECIP_SRC_NO_PRESC)			&&
					(Member.check_od_interact)								&&	// DonR 24Jan2018 CR #13937
					(!SPres[i].HasIshurWithLimit)							&&	// DonR 21Mar2006.
					((SPres[i].DL.largo_type	== 'B')		||
					 (SPres[i].DL.largo_type	== 'M')		||
					 (SPres[i].DL.largo_type	== 'T')))
				{
					MaxRefillDate	= IncrementDate (SysDate, (0 - min_refill_days));

					// DonR 24Nov2024 User Story #366220: Select prior sales of generic equivalents
					// as well as sales of the drug currently being sold.
					ExecSQL (	MAIN_DB, READ_check_for_early_refill,
								&RowsFound,
								&v_MemberIdentification,		&SPres[i].DrugCode,
								&SPres[i].DL.economypri_group,	&MaxRefillDate,
								&v_IdentificationCode,			END_OF_ARG_LIST			);

					Conflict_Test (reStart);

					if (SQLERR_error_test ())
					{
						SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR);
						break;
					}
					else
					{
						if (RowsFound > 0)
						{
							// DonR 23Jun2024 User Story #318200: Decide whether this is an expensive drug based on
							// different prices for Maccabi doctor prescriptions versus all other prescription types.
							int	expensive_drug_prc;

							expensive_drug_prc = (SPres[i].PrescSource == RECIP_SRC_MACABI_DOCTOR) ? ExpensiveDrugMacDocRx : ExpensiveDrugNotMacDocRx;

							if (SPres[i].YarpaDrugPrice >= expensive_drug_prc)
							{
								SetErrorVar (&SPres[i].DrugAnswerCode, ERR_EARLY_REFILL_EXPENSIVE_WARN);
							}

							// DonR 21Sep2008: Per Iris Shaya, for one special pharmacy (which sells to
							// hospitals, etc. rather than "frontally"), we want to give the early-refill
							// warning even for inexpensive drugs.
							// DonR 19Apr2010: Changed #define from ...WARNING to ...CHEAP_WARNING.
							// DonR 26Mar2014: Pharmacy 997582 works the same as Pharmacy 995482.
							else
							{
								if ((v_PharmNum == 995482) || (v_PharmNum == 997582))
								{
									SetErrorVar (&SPres[i].DrugAnswerCode, ERR_EARLY_REFILL_CHEAP_WARNING);
								}
							}
						}	// Found recent purchase of same drug.
					}	// Succeeded in getting purchase-count.
				}	// Need to perform early-refill tests.

			}	// Loop through drugs prescribed.

			// If we've hit a serious error, break out of outer loop.
			if (flg == 1)
			{
				// Note that this won't override ERR_DATABASE_ERROR or other severity >= 10 errors.
				SetErrorVar (&v_ErrorCode, ERR_DRUG_S_WRONG_IN_PRESC);
				break;
			}
		}
		while (0);
		// End of Big Dummy Loop #2.


		// DonR 12Mar2012: For members who have lived overseas for two years or more
		// (maccabi_code == 22 or 23), there are restrictions on buying prescription
		// medications under certain conditions. Here we evaluate those conditions
		// and give appropriate messages to the pharmacy.
		// DonR 11Jul2012: Because we're now generating a warning/error message based on member's total daily
		// purchases, we want to compute the total non-discounted value of the current purchase even if the
		// member doesn't reside overseas, and even if the current prescription is from one of the "special"
		// prescription sources.
		if (v_RecipeSource != RECIP_SRC_NO_PRESC)
		{
			// Loop through drugs to calculate non-discounted purchase price for the quantity requested.
			for (i = FullPriceTotal = 0; i < v_NumOfDrugLinesRecs ; i++)
			{
				// DonR 12Mar2012: Add new logic to compute the total *non-discounted* price of the current sale.
				// This will be used below to issue a message to pharmacies if a member who has resided overseas
				// for two years or more (maccabi_code == 22 or 23) is attempting to purchase more than a set
				// amount of prescription drugs (defaulting to NIS 500).
				// Note that we treat preparation stuff the same as any other item for this purpose.
				OverseasWorkVar	=  (double)SPres[i].YarpaDrugPrice;
				OverseasWorkVar	*= (((double)SPres[i].Units / (double)SPres[i].DL.package_size) + (double)SPres[i].Op);
				FullPriceTotal	+= (int)(OverseasWorkVar + 0.5001);
			}

			if ((v_MemberBelongCode	== MACABI_INSTITUTE) && OVERSEAS_MEMBER)
			{
				// For certain prescription sources, we need to send the error without regard to the size of
				// the sale. For all other sources, we compare the full, non-discounted price of the sale
				// to the limit stored in the sysparams table.
				if ((v_RecipeSource == RECIP_SRC_HOSPITAL)			||
					(v_RecipeSource == RECIP_SRC_OLD_PEOPLE_HOUSE)	||
					(v_RecipeSource == RECIP_SRC_DENTIST)			||
					(v_RecipeSource == RECIP_SRC_PRIVATE)			||
					(FullPriceTotal >  OverseasMaxSaleAmt))
				{
					SetErrorVar (&v_ErrorCode, ((PRIVATE_PHARMACY) ? ERR_MEMBER_LIVES_OVERSEAS : WARN_MEMBER_LIVES_OVERSEAS));
				}
			}	// Drugs being sold to Maccabi member living overseas.

			// DonR 11Jul2012: Provide error/warning message if member is exceeding the daily limit on
			// buying prescription stuff, based on the pharmacy-type-specific limit set in sysparams.
			// Note that since we're looking only at current-day sales, we won't bother with old_member_id.
			if (FullPriceTotal < MaxDailyBuy [Phrm_info.pharmacy_permission])	// If we're already over limit, don't check DB.
			{
				ExecSQL (	MAIN_DB, READ_AlreadyBoughtToday,
							&AlreadyBoughtToday,
							&v_MemberIdentification,	&SysDate,
							&v_PharmNum,				&v_IdentificationCode,
							END_OF_ARG_LIST										);

				// Don't bother with "real" error-checking here - at least for now.
				if (SQLCODE != 0)
					AlreadyBoughtToday = 0;
			}
			else AlreadyBoughtToday = 0;

			if (((FullPriceTotal + AlreadyBoughtToday)			> MaxDailyBuy [Phrm_info.pharmacy_permission])	&&
				(MaxDailyBuy [Phrm_info.pharmacy_permission]	> 0))
			{
				// DonR 12Sep2012: Separate message for Maccabi pharmacies.
				SetErrorVar (&v_ErrorCode, MACCABI_PHARMACY ? DAILY_PURCH_LIM_EXCD_MACCABI : DAILY_PURCHASE_LIMIT_EXCEEDED);
			}
		}	// Current purchase is with prescription.


		// DonR 04Sep2003: Members pay via credit line (i.e. not through pharmacy) under the following conditions:
		//
		// 1) Drugs were prescribed by a doctor (not over-the-counter), and
		//
		// 2) Member has arranged a bank credit line with Maccabi, and
		//
		// 3) Member has chronic illness, no matter what pharmacy prescription is filled at.
		//
		// 4) Pharmacy has its "credit" flag set, no matter whether member is chronically ill or not.
		//
		// "Card passed" logic is irrelevant here.
		//
		// DonR 10Aug2004: Non-chronically-ill patients buying drugs at pharmacies that work
		// with credit lines will be given the option of paying through the credit line even
		// for non-prescription drugs. In this case we'll pass a 10 to the pharmacy, and the
		// pharmacy will pass back a 9 in Transaction 2005 to indicate that the member is
		// going to pay through his/her credit line.
		//
		// DonR 05Aug2009: Credit Type 9 is now sent to all pharmacies, Maccabi and private.
		if ((v_RecipeSource			!= RECIP_SRC_NO_PRESC)			&&	// NOT non-prescription
			((v_CreditYesNo			== 2) || (v_CreditYesNo == 4))	&&	// Member has Credit Line
			(v_insurance_status		== 1)							&&	// Chronic illness
			(((v_MacCent_cred_flg	!= 7) && (v_MacCent_cred_flg != 2)) || PRIVATE_PHARMACY))		// Per Iris Shaya 09Nov2005.
		{
			// DonR 10Apr2013: Add additional conditions for automatic credit-line payment. If these
			// are not met, send a message to the pharmacy and fall through to the next (optional
			// credit-line payment) section of code. Anyone otherwise eligible pays this way if
			// his/her magnetic card is used; otherwise, the purchase must be made at a Maccabi
			// pharmacy, and the member must be getting drugs through an old-age home with Credit Flag
			// equal to 1, 5, or 9.
			if (MEMBER_USED_MAG_CARD	||
				((MACCABI_PHARMACY && ((v_MacCent_cred_flg == 1) || (v_MacCent_cred_flg == 5) || (v_MacCent_cred_flg == 9)))))
			{
				v_CreditYesNo = 9;	// Payment is direct from bank to pharmacy.
			}
			else
			{
				SetErrorVar (&v_ErrorCode, ERR_AUTO_CREDIT_PMT_W_CARD_MSG);
			}
		}

		// DonR 10Apr2013: It is now possible that someone can meet most of the criteria for Credit Type 9
		// (automatic payment through credit line), but not have this option set because s/he didn't have
		// a magnetic card at the pharmacy. In this case, we want to fall through to the next section,
		// since the member may still qualify for optional credit-line payment. Accordingly, the next block
		// of code is no longer in an "else"; if v_CreditYesNo has been set to 9, the "if" will be false
		// in any case.

		// DonR 14Oct2004: If Client Location Code is greater than one (e.g. member
		// lives in an old-age home), offer payment by credit line only if that
		// location has a Credit Flag of 9 and the prescription is being filled
		// at a Maccabi pharmacy; in this case, we don't worry about
		// the member's Maccabi Card or the pharmacy's Credit Flag. If the Client
		// Location Code is zero or one, operate "normally" - that is, offer credit
		// if the card has been scanned.
		// DonR 10Jan2005: 1 is a valid card date!
		// DonR 30Oct2006: Per Iris Shaya, added Maccabi Centers Credit Flag of 5
		// to the condition (to join with 9) to pay by credit line w/o using member's card.
		if (((v_CreditYesNo			== 2) || (v_CreditYesNo == 4))	&&	// Member has Credit Line
			(v_CreditPharm			!= 0)							&&	// Pharmacy works with Credit Lines
			(((v_MacCent_cred_flg	!= 7) &&
			  (v_MacCent_cred_flg	!= 2))	 || (PRIVATE_PHARMACY))	&&	// Per Iris Shaya 09Nov2005.
			(MEMBER_USED_MAG_CARD ||						// Patient used magnetic card OR...
			 ((v_ClientLocationCode >  1) && ((v_MacCent_cred_flg == 5) ||
											  (v_MacCent_cred_flg == 9))	&& (MACCABI_PHARMACY))))
															// ...Magnetic card not required.
		{
			// 23Nov2005: Per Iris Shaya, Member Credit type 4 indicates that member
			// prefers to pay through multiple payments ("tashlumim"), even though
			// they are known to be a tool of the devil.
			v_CreditYesNo = (v_CreditYesNo == 4) ? 11 : 10;	// Pharmacy will give option to patient
															// to pay through Credit Line.


			// DonR 20Mar2006: Don't perform family purchase check for Member ID zero!
			// NOTE: As of 1 April 2011, credit_type_code will be populated in prescription_drugs;
			// this should mean that the prescriptions table can be dropped from this query.
			// DonR 03Apr2011: Revamped SELECT for better performance - we now need to use only
			// members and prescription_drugs.
			// DonR 07Apr2019: This SELECT needs the following changes: (A) Instead of looking at the
			// credit_type_code in prescription_drugs, we should be looking at prescriptions/credit_type_used,
			// which reflects the member's decision whether to pay by credit line; and (B) the value to look for
			// is 9 -  the only alternative is 0, which means the member paid by cash or credit card.
			if ((v_RecipeSource == RECIP_SRC_NO_PRESC) && (v_MemberIdentification > 0))
			{
				v_FirstOfMonth = SysDate + 1 - (SysDate % 100);

				ExecSQL (	MAIN_DB, READ_check_family_credit_used,
							&v_FamilyCreditUsed,
							&v_FamilyHeadTZ,	&v_FamilyHeadTZCode,
							&v_FirstOfMonth,	END_OF_ARG_LIST			);

				v_FamilyCreditInd = ColumnOutputLengths [1];

				if (SQLMD_is_null (v_FamilyCreditInd))
					v_FamilyCreditUsed = 0;

				// Compute anticipated total of the current sale.
				v_CreditToBeUsed = 0;
				for (i = 0; i < v_NumOfDrugLinesRecs; i++)
				{
					if (SPres[i].DFatalErr == MAC_TRUE )
						continue;

					// Compute participation per OP.
					// DonR 08Jan2012: If "Meishar" has given a fixed price of zero, that's a real
					// fixed price.
					if ((SPres[i].PriceSwap				== 0)	&&
						(SPres[i].ret_part_source.table	!= FROM_GADGET_APP))	// DonR 08Jan2012
					{
						// DonR 28Mar2010: Since we're only executing this block of code if the current
						// purchase is being made without a prescription - and non-prescription sales
						// are always charged 100% participation - the business about the 12-shekel
						// minimum when participation is 15% should really be irrelevant. This is worth
						// noting since I've just been told that the 12-skekel minimum has been changed
						// to 15 shekels.
						GET_PARTICIPATING_PERCENT (SPres[i].RetPartCode, &percent1);
						v_CreditThisDrug = (int) ((float)SPres[i].RetOpDrugPrice * (float)percent1 / 10000.0);
						if ((v_CreditThisDrug < 1200) && (SPres[i].RetPartCode == PARTI_CODE_TWO))
							v_CreditThisDrug = 1200;	// Minimum NIS 12.00 for "normal" 15% participation.
					}
					else
					{
						v_CreditThisDrug = SPres[i].PriceSwap;
					}

					// Multiply by packages requested.
					if (SPres[i].Op > 1)
						v_CreditThisDrug *= SPres[i].Op;

					// Compute member discount - although I don't really think there are any
					// discounts on non-prescription medications!
					if (SPres[i].AdditionToPrice  > 0)
						v_CreditThisDrug -= (int) ((float)v_CreditThisDrug * (float)SPres[i].AdditionToPrice / 10000.0);

					// Finally, add the current drug's net-to-member price to the total.
					v_CreditToBeUsed += v_CreditThisDrug;
				}	// Loop through drugs in current purchase request.


				// DonR 10Apr2019 CR #???: Maximum monthly family OTC credit is now a sysparams parameter:
				// sysparams/max_otc_credit_mon.
				if ((v_FamilyCreditUsed + v_CreditToBeUsed) > MaxFamilyOtcCreditPerMonth)
				{
					v_CreditYesNo = 0;	// No more use of credit line this month!
					SetErrorVar (&v_ErrorCode, ERR_NO_MORE_OTC_CREDIT);

					GerrLogFnameMini ("otc_credit_log",
									   GerrId,
									   "PrID %d prev %d, now %d, tot %d, err = %d - DENIED CREDIT!\t\t\t\t\t\t",
									   v_RecipeIdentifier,
									   v_FamilyCreditUsed,
									   v_CreditToBeUsed,
									   (v_FamilyCreditUsed + v_CreditToBeUsed),
									   SQLCODE);
				}	// Monthly OTC credit-line usage exceeded.
			}	// Current request is for OTC drugs.
		}	// Credit conditions met.

		else
		{
			// This is the "catch-all" default: if member is not an eligible chronic patient
			// and doesn't qualify for "normal" credit-line payment, s/he has to pay by
			// cash or credit card. Note that any value other than 2 or 4 in the member's
			// credit_type_code field should land him/her here!
			// DonR 10Apr2013: We can now get here if member qualified for automatic payment via
			// credit line (v_CreditYesNo == 9); accordingly, zero v_CreditYesNo conditionally.
			if (v_CreditYesNo != 9)
				v_CreditYesNo = 0;	// Payment is made to Pharmacy.
		}


		// DonR 19Jan2005: See if there's anything in pharmacy_message for the pharmacy to see.
		v_Message = MAC_FALS;

		ExecSQL (	MAIN_DB, READ_check_pharmacy_message_count,
					&RowsFound, &v_PharmNum, END_OF_ARG_LIST		);

		Conflict_Test (reStart);

		if ((SQLCODE == 0) && (RowsFound > 0))
			v_Message = MAC_TRUE;

		if (SQLERR_error_test ())
		{
			SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR);
		}


		// DonR 10Apr2013: If member is buying prescription drugs without passing a magnetic card AND the
		// prescriptions are not in the doctor-prescription table (and some other conditions are met), give
		// the pharmacy a message indicating that this is a purchase that should be permitted only with the
		// use of a magnetic card. The error is of severity 2, which means that the sale is permitted, but
		// the pharmacist must authorize it specifically. We neet to check doctor_presc only for the first
		// drug on the list, since pharmacies sell either against prescriptions sent via Transaction 2001
		// or against paper prescriptions, but never against a combination of both at the same time.
		// Note regarding the v_MacCent_cred_flg test: If this is an ordinary purchase by a member who does
		// not live in an old-age home, v_ClientLocationCode will be 1 and v_MacCent_cred_flg will be zero;
		// since only values of v_ClientLocationCode > 1 will have non-zero v_MacCent_cred_flg values, there
		// is no need to test v_ClientLocationCode here. A value of 1, 5, 7, or 9 in v_MacCent_cred_flg
		// *disables* this test.
		// DonR 24Dec2013: This test now applies to all pharmacies. Also, give different error codes
		// depending on Prescription Source. Also, added this logic to Transaction 2003 - it was previously
		// present only in Transaction 5003.
		// DonR 27Apr2015: Added capability of working with the new doctor_presc table instead of prescr_wr_new.
		if ((v_RecipeSource		!= RECIP_SRC_NO_PRESC)			&&
			(v_MacCent_cred_flg	!= 1)							&&
			(v_MacCent_cred_flg	!= 5)							&&
			(v_MacCent_cred_flg	!= 7)							&&
			(v_MacCent_cred_flg	!= 9)							&&
			(!MEMBER_USED_MAG_CARD))
		{
			RowsFound = 0;	// Default in case we don't need to do a lookup.

			// If the prescription is not from a Maccabi doctor, don't bother looking it up in doctor_presc - we
			// already know that it's not there.
			// DonR 05Nov2014: Changing the 799/800 boundary in economypri to 999/1000. This is temporary, as the
			// code is being expanded to six digits. When the next phase comes in, the boundary will
			// be at 19999/20000.
			// DonR 31Jan2018: Instead of selecting a list of related Largo Codes from economypri, get the list
			// from drug_list. This will give exactly the same result, but avoids duplicating logic that checks
			// the economypri system_code and del_flg fields. This way, the logic that deals with this table is
			// all contained in the "pipeline" programs As400UnixServer and As400UnixClient.
			if (v_RecipeSource == RECIP_SRC_MACABI_DOCTOR)
			{
				SET_ISOLATION_DIRTY;

				PW_Doc_Largo_Code	= PR_Original_Largo_Code	[0];
				PW_DoctorPrID		= PR_DoctorPrID				[0];

				ExecSQL (	MAIN_DB, READ_check_first_doctor_prescription_is_in_database,
							&RowsFound,
							&v_MemberIdentification,	&PW_DoctorPrID,
							&EarliestPrescrDate,		&LatestPrescrDate,
							&PW_Doc_Largo_Code,			&SPres[0].DL.economypri_group,
							&DocID,						&v_IdentificationCode,
							END_OF_ARG_LIST													);

				// If anything went wrong with the SQL query, just assume that we found something.
				if (SQLCODE != 0)
					RowsFound = 1;

				SET_ISOLATION_COMMITTED;
			}	// Prescriptions are from a Maccabi doctor, and thus should be found in doctor_presc.

			// If prescriptions are from a non-Maccabi doctor, or were not found in doctor_presc, notify
			// pharmacy that something is amiss.
			if (RowsFound == 0)
			{
				switch (v_RecipeSource)
				{
					case RECIP_SRC_MACABI_DOCTOR:		ErrorCodeToAssign = ERR_MUST_PASS_2_CARDS_TO_BUY;	break;

					case RECIP_SRC_HOSPITAL:
					case RECIP_SRC_OLD_PEOPLE_HOUSE:
					case RECIP_SRC_PRIVATE:				ErrorCodeToAssign = WHITE_PRESCRIPTION_PASS_2_CARDS;	break;

					default:							ErrorCodeToAssign = HAND_PRESCRIPTION_PASS_2_CARDS;	break;
				}	// Decide which error code to assign based on Prescription Source.

				SetErrorVar (&v_ErrorCode, ErrorCodeToAssign);
			}	// No electronic prescription found.
		}	// Prescription drug sale without member's magnetic card having been passed.


		// DonR 01Sep2013: In case we hit a fatal error but didn't assign Error 6089 (which
		// pharmacies use to indicate a "chasima" for a drug), make sure that we did assign it.
		// (SetErrorVarArr is smart enough not to set the same error twice, so we don't actually
		// need to check whether we've already assigned it.)
		for (i = 0; i < v_NumOfDrugLinesRecs ; i++)
		{
			if (SPres[i].DFatalErr)
			{
				SetErrorVar (&v_ErrorCode, ERR_DRUG_S_WRONG_IN_PRESC);
				break;
			}
		}


		// UPDATE TABLES.
		// Set up dummy loop to facilitate error-handling.
		do
		{
			// DonR 29Jul2003: If we've already hit a fatal database error, skip
			// past the record-writing stuff.
			if (v_ErrorCode == ERR_DATABASE_ERROR)
				break;

			// Insert into PRESCRIPTIONS table.
			ExecSQL (	MAIN_DB, TR2003_INS_prescriptions,
						&v_PharmNum,			&v_InstituteCode,
						&v_PriceListCode,		&v_MemberIdentification,
						&v_IdentificationCode,	&v_MemberBelongCode,
						&DocID,					&v_TypeDoctorIdentification,
						&v_RecipeSource,		&v_RecipeIdentifier,

						&v_SpecialConfNum,		&v_SpecialConfSource,
						&v_ConfirmationDate,	&v_ConfirmationHour,
						&v_ErrorCode,			&v_NumOfDrugLinesRecs,
						&v_CreditYesNo,			&v_MoveCard,
						&ShortZero,				&v_TerminalNum,

						&IntZero,				&IntZero,
						&IntZero,				&Member.maccabi_code,
						&IntZero,				&NOT_REPORTED_TO_AS400,
						&ShortZero,				&ShortZero,
						&ShortZero,				&v_ClientLocationCode,

						&v_DoctorDeviceCode,	&ShortZero,
						&v_PriceListCode,		&IntZero,
						&IntZero,				&v_MsgExistsFlg,
						&DRUG_NOT_DELIVERED,	&ROW_NOT_DELETED,
						&IntZero,				&ShortZero,

						&TrnID,					&ShortZero,
						&Member.insurance_type,	&Member.darkonai_type,
						END_OF_ARG_LIST											);

			Conflict_Test (reStart);

			if (SQLERR_error_test ())
			{
				SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR);
				break;
			}


			// DonR 02May2013: Write error code, if any, to prescription_msgs.
			if (v_ErrorCode > 0)
			{
				v_Severity = GET_ERROR_SEVERITY (v_ErrorCode);

				ExecSQL (	MAIN_DB, INS_prescription_msgs,
							&v_RecipeIdentifier,	&IntZero,				&ShortZero,
							&v_ErrorCode, 			&v_Severity,			&v_ConfirmationDate,
							&v_ConfirmationHour,	&DRUG_NOT_DELIVERED,	&NOT_REPORTED_TO_AS400,
							END_OF_ARG_LIST																);

				Conflict_Test (reStart);
				
				if (SQLERR_error_test ())
				{
					SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR);
					break;
				}
			}	// Non-zero error code at "rosh" level.


			// Insert into PRESCRIPTION_DRUGS table.
			for (i = 0; i < v_NumOfDrugLinesRecs ; i++)
			{
				// Prepare S.Q.L. variables.
				LineNum			= i + 1;
				PrsLinP			= &SPres[i];
				StopUseDate		= AddDays (SysDate, PrsLinP->Duration);
				StopBloodDate	= AddDays (SysDate, (PrsLinP->Duration + PrsLinP->DL.t_half));

				PW_Doc_Largo_Code	= PR_Original_Largo_Code	[i];
				PW_DoctorPrID		= PR_DoctorPrID				[i];
				PW_DosesPerDay		= PR_DosesPerDay			[i];
				PW_Pr_Date			= PR_Date					[i];


				// DonR 14Jan2009: We now write the substitution-permitted data using two variables:
				// first, a new flag explaining why a non-preferred drug was sold; and second, the
				// reason, if any, supplied by the pharmacy. The pharmacy-supplied value of 99
				// accordingly needs to be truncated to 9.
				PW_SubstPermitted = (SPres[i].WhyNonPreferredSold * 10) + (PR_WhyNonPreferredSold[i] % 10);

				// DonR 25Mar2012: For members buying stuff at VAT-exempt pharmacies (i.e. in Eilat),
				// take VAT off the fixed price, UNLESS it's a fixed price received from the AS/400 "meishar"
				// application. In the latter case, VAT should already have been taken off, so we don't want
				// to take it off twice.
				if ((vat_exempt != 0) && (SPres[i].ret_part_source.table != FROM_GADGET_APP))
				{
					// Note that casting the result of a floating-point computation to long results in
					// truncation rather than rounding - so adding .5001 beforehand won't cause member
					// to be charged one agora for a VAT-exempt freebie.
					SPres[i].PriceSwap = (int)(((double)SPres[i].PriceSwap * no_vat_multiplier) + .5001);

					// DonR 16Dec2013: If this is a prescription purchase receiving a "Maccabi Price"
					// discount, the price needs to be rounded to the nearest 10-agorot value.
					if ((v_RecipeSource					!= RECIP_SRC_NO_PRESC)		&&
						(SPres[i].PriceSwap				>  0)						&&
						(SPres[i].MacabiPriceFlg		== MAC_TRUE)				&&
						(SPres[i].ret_part_source.table	== FROM_MACCABI_PRICE))
					{
						Agorot = SPres[i].PriceSwap % 10;
						if (Agorot > 4)
							SPres[i].PriceSwap += 10;	// Round up.
						SPres[i].PriceSwap -= Agorot;	// Set the Agorot digit to zero.
					}
				}

				// DonR 31Aug2005: If this is a supplement that gets its participation
				// (i.e. RetPartCode) from the "major" drug, then its participation
				// method must be the same as well. This is not true for "gadgets" -
				// which sometimes use the Link to Addition parameter for a different purpose.
			    if ((SPres[i].LinkDrugToAddition	>  0	)	&&
					(SPres[i].DL.largo_type			!= 'X'	)	&&
					(SPres[i].DL.in_gadget_table	== 0	)	&&
					(MajorDrugIndex[i]				>= 0	))
			    {
					j = MajorDrugIndex[i];
				}
				else
				{
					j = i;	// Default.
				}
				// DonR 23Sep2004: Compute new participation source as a two-digit number, with
				// the "tens" digit indicating the insurance used and the "ones" digit
				// indicating the participation source.
				// DonR 31Aug2005: Use the subscript "j" defined above, so that we set
				// the participation method for supplements from the "major" drug
				// along with the participation itself.
				SPres[i].InsPlusPtnSource =   (10 * SPres[j].ret_part_source.insurance_used)
											+ SPres[j].ret_part_source.table;

				// DonR 14Jan2009: If we've assigned specialist participation (which can come
				// "directly" or through a Pharmacy Ishur), there should be a reason for doing
				// so in WhySpecialistPtnGiven. Store this value (which should be between 1 and 6)
				// as the "hundreds" digit in the participation-method field.
				if (((SPres[j].ret_part_source.table == FROM_DOC_SPECIALIST)	||
					 (SPres[j].ret_part_source.table == FROM_PHARMACY_ISHUR))		&&
					((SPres[j].WhySpecialistPtnGiven > 0) && (SPres[j].WhySpecialistPtnGiven < 10)))
						// The latter condition should always be true, but just in case...
				{
					SPres[i].InsPlusPtnSource += (100 * SPres[j].WhySpecialistPtnGiven);
				}


				// Try reading the doctor_presc row. If the read fails, try adding it
				// as a new row.
				// DonR 08Jun2005: No need to check doctor_presc for OTC prescriptions.
				// DonR 04Mar2015: Look up doctor prescriptions only for v_RecipeSource == RECIP_SRC_MACABI_DOCTOR.
				// Also, if the pharmacy's Doctor Prescription Date is incorrect, load it from the doctor-prescription
				// table.
				// DonR 27Apr2015: Add capability to use the new doctor_presc table instead of prescr_wr_new.

				if (v_RecipeSource == RECIP_SRC_MACABI_DOCTOR)	// Other sources won't be in the database.
				{
					// DonR 14Mar2005: Avoid unnecessary lock errors by downgrading "isolation"
					// for this lookup.
					// DonR 21May2013: Add fields to determine if this was a doctor prescription lacking
					// dosage, or a non-daily prescription.
					// DonR 21Jul2015: re-ordered WHERE clause to correspond with doctor_presc index.
					SET_ISOLATION_DIRTY;

					PW_ID = 0;	// Paranoid re-initialization of variable that we no longer use for anything anyway.

					ExecSQL (	MAIN_DB, TR2003_5003_READ_doctor_presc, TR2003_5003_READ_doctor_presc_by_date,
								&PW_status,					&PW_NumPerDose,		&PW_RealAmtPerDose,
								&PW_PrescriptionType,		&PW_AuthDate,		&DummyInt,
								&PW_Doc_Largo_Code,
								&v_MemberIdentification,	&PW_DoctorPrID,		&PW_Pr_Date,
								&PW_Doc_Largo_Code,			&DocID,				&v_IdentificationCode,
								END_OF_ARG_LIST																);

					// DonR 26Nov2003: Just in case the pharmacy sent a bogus Prescription Date, try
					// reading again with a reasonable date range.
					// DonR 21Jul2015: re-ordered WHERE clause to correspond with doctor_presc index.
					if (SQLERR_code_cmp (SQLERR_ok) != MAC_TRUE)
					{
						// NOTE: The FLOOR function is not supported in the 32-bit version of Informix.
						PW_ID = 0;	// Paranoid re-initialization of variable that we no longer use for anything anyway.

						ExecSQL (	MAIN_DB, TR2003_5003_READ_doctor_presc, TR2003_5003_READ_doctor_presc_by_date_range,
									&PW_status,					&PW_NumPerDose,			&PW_RealAmtPerDose,
									&PW_PrescriptionType,		&PW_AuthDate,			&PW_Pr_Date,
									&PW_Doc_Largo_Code,
									&v_MemberIdentification,	&PW_DoctorPrID,			&EarliestPrescrDate,
									&LatestPrescrDate,			&PW_Doc_Largo_Code,		&DocID,
									&v_IdentificationCode,		END_OF_ARG_LIST									);
					}	// Reading doctor_presc with exact date failed, so try with date range.


					// DonR 21May2013: If the drug being sold requires that the doctor specify quantity-per-dose
					// and this field is missing from the doctor's prescription, OR this is not a daily-use prescription,
					// send pharmacy an "instructions are missing" message. (Obviously, this is only assuming that we
					// actually did read a doctor_presc row.) Note that while this is a non-fatal error, it has to be
					// treated as a higher priority than other non-fatal errors - so we'll use the new macro
					// SetErrorVarBump, which will force the new error code in even if there was already an error of
					// equal severity.
					// DonR 27Apr2015: At least for now, if we're working with the new doctor_presc table and the doctor
					// prescribed a fractional amount amount-per-dose, we will *not* send pharmacy a "missing instructions"
					// error - but this may not be correct. If we do need to send an error in that case, we will have to
					// read both the "real" amount per dose and the FLOOR'ed version and compare them.
					if (SQLCODE == 0)
					{
						if (((*PW_PrescriptionType >= '1')	&& (*PW_PrescriptionType <= '4'))						||
							((PW_RealAmtPerDose - (double)PW_NumPerDose) > .001))
						{
							SetErrorVarBump (&SPres[i].DrugAnswerCode, ERR_MISSING_USAGE_INSTRUCTIONS);
						}	// Doctor's prescription was missing usage-instruction information.
					}	// Successfully read doctor prescription from doctor_presc.
					else
					{
						// Set bogus values for PRW-ID and AuthDate so we can be sure we don't match anything in the database.
						PW_ID = PW_AuthDate = -1;
					}


					// DonR 24Jul2013: Moved early-refill warning test here, since it now relies on knowing the
					// visit date from doctor_presc.
					// DonR 21Mar2006: If this drug is being sold according to an AS400 Ishur
					// with quantity limits, there is no need to check for early refill as well.
					// DonR 23Aug2010: Per Iris Shaya, don't check for early refills for those "members"
					// in the membernotcheck table.
					if ((MACCABI_PHARMACY)								&&
						(Member.check_od_interact)						&&	// DonR 24Jan2018 CR #13937
						(!SPres[i].HasIshurWithLimit)					&&	// DonR 21Mar2006.
						((SPres[i].DL.largo_type	== 'B')		||
						 (SPres[i].DL.largo_type	== 'M')		||
						 (SPres[i].DL.largo_type	== 'T'))			&&
						( SPres[i].DL.drug_type != 'O')					&&
						( SPres[i].DL.drug_type != 'Q'))
					{
						// DonR 19Apr2010: Add additional warning message for early refill, with slightly
						// different conditions than the other early-refill messages.
						// DonR 24Jul2013: Add criteria to avoid giving this message if the current sale
						// is against a prescription found in doctor_presc AND the previous sale was at
						// the same pharmacy with a prescription from the same doctor written on the same day.
						// DonR 27Apr2015: Create a separate version of this query to use the new pd_rx_link
						// table instead of prescr_wr_new.
						// DonR 01Jan2020: Patching in the same ODBC command as is used in Transaction 6003,
						// even though it has somewhat different logic for determining if earlier sales were
						// complete against their doctor prescriptions. This logic should work fine, since
						// all three drug-sale transactions are using the same routine to update doctor_presc
						// and pd_rx_link at sale-completion time.
						ShortOverlapDate	= IncrementDate (SysDate, MaxShortDurationOverlapDays);
						LongOverlapDate		= IncrementDate (SysDate, MaxLongDurationOverlapDays);

						// DonR 24Nov2024 User Story #366220: Select prior sales of generic equivalents
						// as well as sales of the drug currently being sold. There are a bunch of other
						// changes in the SQL logic - see the SQL code.
						ExecSQL (	MAIN_DB, READ_test_for_ERR_EARLY_REFILL_WARNING,
									&RowsFound,
									&v_MemberIdentification,	&SPres[i].DrugCode,		&SPres[i].DL.economypri_group,
									&v_IdentificationCode,		&SysDate,				&MaxShortDurationDays,
									&ShortOverlapDate,			&MaxShortDurationDays,	&LongOverlapDate,
									&SysDate,					END_OF_ARG_LIST											);

						Conflict_Test (reStart);

						if (SQLERR_error_test ())
						{
							SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR);
							break;
						}
						else
						{
							if (RowsFound > 0)
							{
								SetErrorVar (&SPres[i].DrugAnswerCode, ERR_EARLY_REFILL_WARNING);
							}	// Found recent purchase of same drug.
						}	// Succeeded in getting purchase-count.
					}	// Need to perform early-refill test.


					// DonR 14Mar2005: Restore "isolation" to normal level.
					SET_ISOLATION_COMMITTED;

				}	// This is not an OTC prescription.

				// DonR 21May2013: For quantity-per-dose, we want to write what the pharmacy sent, not what was in
				// doctor_presc - so the next line of code was moved here from the beginning of the drugs loop.
				PW_NumPerDose = PR_NumPerDose [i];

				// Remember the status of the doctor_presc row.
				// If the row had previously been marked with Status 3 (i.e. it had been
				// selected as an alternative prescription and had its Prescription Date
				// "stolen"), report it as Status 0 (unfilled). This is rather unlikely
				// ever to happen in the real world.
				// DonR 28Apr2015: Note that Status 3 is no longer in use, so I took out reference to it.
				PR_Status[i] = PW_status;


				// DonR 05May2004: If pharmacy sent a Special Prescription number AND
				// we didn't find an AS400 Special Prescription for this drug, copy the
				// pharmacy's number to the drug's variables - otherwise the number (used
				// for things like overriding interaction/overdose messages) is lost.
				if ((v_SpecialConfNum > 0) && (PrsLinP->SpecPrescNum == 0))
				{
					PrsLinP->SpecPrescNum		= v_SpecialConfNum;
					PrsLinP->SpecPrescNumSource	= v_SpecialConfSource;
				}


				// DonR 13Jan2009: If drugs are being sold without a prescription, swap in a value of 8
				// for the "in health basket" parameter. This doesn't change any logic in how the Unix
				// programs handle the sale, but is needed by the AS/400.
				// DonR 10Oct2016: For emergency-supply sales, force in zero for "in health basket".
				if (v_RecipeSource == RECIP_SRC_NO_PRESC)
					PrsLinP->in_health_pack = 8;
				else
				if (v_RecipeSource == RECIP_SRC_EMERGENCY_NO_RX)
					PrsLinP->in_health_pack = 0;


				// If participation isn't coming from a drug_extension rule, force rule_number to zero.
				if (PrsLinP->ret_part_source.table != FROM_DRUG_EXTENSION)
					PrsLinP->rule_number = 0;


				// DonR 09Jul2003: Switched storage of basic and computed participation codes,
				// so that the correct code for the actual purchase goes to the AS400. Now we
				// will store BasePartCode (normally from drug_list) in calc_member_price, and
				// RetPartCode (the participation code we actually send to the pharmacy) will
				// be in member_price_code. The latter field is the one sent to the AS400.
				// Note that this change will not affect pharmacy operations at all - just
				// a switch in which number is stored in which field, and which number is
				// sent to the AS400.
				ExecSQL (	MAIN_DB, TR2003_INS_prescription_drugs,
							&v_RecipeIdentifier,			&LineNum,
							&v_MemberIdentification,		&v_IdentificationCode,
							&PrsLinP->DrugCode,				&NOT_REPORTED_TO_AS400,
							&PrsLinP->Op,					&PrsLinP->Units,
							&PrsLinP->Duration,				&StopUseDate,

							&v_ConfirmationDate,			&v_ConfirmationHour,
							&ROW_NOT_DELETED,				&DRUG_NOT_DELIVERED,
							&PrsLinP->PriceSwap,			&PrsLinP->AdditionToPrice,
							&PrsLinP->LinkDrugToAddition,	&PrsLinP->RetOpDrugPrice,
							&PrsLinP->DrugAnswerCode,		&PrsLinP->RetPartCode,

							&PrsLinP->BasePartCode,			&v_RecipeSource,
							&PrsLinP->MacabiPriceFlg,		&v_PharmNum,
							&v_InstituteCode,				&PrsLinP->SupplierDrugPrice,
							&StopBloodDate,					&IntZero,
							&IntZero,						&IntZero,

							&IntZero,						&ShortZero,
							&PrsLinP->DL.t_half,			&DocID,
							&v_TypeDoctorIdentification,	&PW_DoctorPrID,
							&PW_Pr_Date,					&PW_Doc_Largo_Code,
							&PW_SubstPermitted,				&PrsLinP->Units,
													
							&PrsLinP->Op,					&PW_NumPerDose,
							&PW_DosesPerDay,				&PrsLinP->InsPlusPtnSource,
							&PW_ID,							&PrsLinP->in_health_pack,
							&PrsLinP->SpecPrescNum,			&PrsLinP->SpecPrescNumSource,
							&PrsLinP->rule_number,			&ShortZero,	// Action Type = 0 for pre-5000 trns.

							&PrsLinP->Doctor_Op,			&PrsLinP->Doctor_Units,
							&v_CreditYesNo,					&PrsLinP->why_future_sale_ok,
							&PrsLinP->qty_limit_chk_type,	&PrsLinP->qty_lim_ishur_num,
							&PrsLinP->vacation_ishur_num,	&PW_AuthDate,
							&v_DoctorDeviceCode,			END_OF_ARG_LIST					);

				Conflict_Test (reStart);
				
				if (SQLERR_error_test ())
				{
					SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR);
					break;
				}


				// DonR 02May2013: Write error code, if any, to prescription_msgs.
				if (PrsLinP->DrugAnswerCode > 0)
				{
					v_Severity = GET_ERROR_SEVERITY (PrsLinP->DrugAnswerCode);

					ExecSQL (	MAIN_DB, INS_prescription_msgs,
								&v_RecipeIdentifier,		&PrsLinP->DrugCode,		&LineNum,
								&PrsLinP->DrugAnswerCode, 	&v_Severity,			&v_ConfirmationDate,
								&v_ConfirmationHour,		&DRUG_NOT_DELIVERED,	&NOT_REPORTED_TO_AS400,
								END_OF_ARG_LIST																);

					Conflict_Test (reStart);
				
					if (SQLERR_error_test ())
					{
						SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR);
						break;
					}
				}	// Non-zero error code at drug level.

			}	// End of drugs-in-prescription loop.
		}
		while (0);	// Dummy loop should run only once.


		// Commit the transaction.
		if (reStart == MAC_FALS)	// No DB errors - OK to commit work.
		{
			CommitAllWork ();

			if (SQLERR_error_test ())	// Could not COMMIT
			{
				SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR);
			}
		}
		else
		{
			// We saw some kind of DB error - try to restart.
			if (TransactionRestart () != MAC_OK)
			{
				v_ErrorCode = ERR_DATABASE_ERROR;
				break;
			}
			
			// DonR 26Jul2011: Suppress log message until the fourth attempt fails.
			if (tries > 2)
				GerrLogReturn (GerrId, "Table is locked for the <%d> time", tries);
			
			sleep (ACCESS_CONFLICT_SLEEP_TIME);
		}	// DB error occurred.

	}	// End of Database Retries loop.
	
	
	if (reStart == MAC_TRUE)
	{
		GerrLogReturn (GerrId, "Locked for <%d> times", SQL_UPDATE_RETRIES);
		SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR);
	}


	// Prepare and send Response Message (Transaction 2004).
	WinHebToDosHeb ((unsigned char *) v_MemberFamilyName);
	WinHebToDosHeb ((unsigned char *) v_MemberFirstName);
	WinHebToDosHeb ((unsigned char *) v_DoctorFamilyName);
	WinHebToDosHeb ((unsigned char *) v_DoctorFirstName);

	nOut =  sprintf (OBuffer,		 "%0*d",  MSG_ID_LEN,				2004					);
	nOut += sprintf (OBuffer + nOut, "%0*d",  MSG_ERROR_CODE_LEN,		MAC_OK					);
	nOut += sprintf (OBuffer + nOut, "%0*d",  TPharmNum_len,			v_PharmNum				);
	nOut += sprintf (OBuffer + nOut, "%0*d",  TInstituteCode_len,		v_InstituteCode			);
	nOut += sprintf (OBuffer + nOut, "%0*d",  TTerminalNum_len,			v_TerminalNum			);
	nOut += sprintf (OBuffer + nOut, "%0*d",  TErrorCode_len,			v_ErrorCode				);
	nOut += sprintf	(OBuffer + nOut, "%0*d",  TGenericTZ_len,			v_MemberIdentification	);
	nOut += sprintf (OBuffer + nOut, "%0*d",  TIdentificationCode_len,	v_IdentificationCode	);

	nOut += sprintf (OBuffer + nOut, "%-*.*s", TMemberFamilyName_len,
										 	   TMemberFamilyName_len,	v_MemberFamilyName		);
    nOut += sprintf (OBuffer + nOut, "%-*.*s", TMemberFirstName_len,	
											   TMemberFirstName_len,	v_MemberFirstName		);

	nOut += sprintf (OBuffer + nOut, "%0*d",  TMemberDateOfBirth_len,	Member.date_of_bearth	);
						
	nOut += sprintf (OBuffer + nOut, "%-*.*s", TDoctorFamilyName_len,	
											   TDoctorFamilyName_len,	v_DoctorFamilyName		);
	nOut += sprintf (OBuffer + nOut, "%-*.*s", TDoctorFirstName_len,	
											   TDoctorFirstName_len,	v_DoctorFirstName		);

    nOut += sprintf (OBuffer + nOut, "%0*d",  TConfiramtionDate_len,	v_ConfirmationDate		);
	nOut += sprintf (OBuffer + nOut, "%0*d",  TConfirmationHour_len,	v_ConfirmationHour		);
	nOut += sprintf (OBuffer + nOut, "%0*d",  TMsgExistsFlg_len,		v_MsgExistsFlg			);
	// DonR 19Jan2005: Per Iris Shaya, replaced the Message Urgent Flag, which is currently
	// not in use, with the Pharmacy Message Flag - which was inadvertently left out of the
	// specification for Transaction 2005.
	nOut += sprintf (OBuffer + nOut, "%0*d",  TMsgExistsFlg_len,		v_Message				);
	nOut += sprintf (OBuffer + nOut, "%0*d",  TRecipeIdentifier_len,	v_RecipeIdentifier		);
	nOut += sprintf (OBuffer + nOut, "%0*d",  TCreditYesNo_len,			v_CreditYesNo			);
	nOut += sprintf (OBuffer + nOut, "%0*d",  TElectPR_LineNumLen,		v_NumOfDrugLinesRecs	);


	// Repeating portion of message.
	for (i = 0; i < v_NumOfDrugLinesRecs; i++)
	{
		// Line Number.
		nOut += sprintf (OBuffer + nOut, "%0*d",  TElectPR_LineNumLen,			(i + 1)						);
		nOut += sprintf (OBuffer + nOut, "%0*d",  TDrugCode_len,				SPres[i].DrugCode			);
		nOut += sprintf (OBuffer + nOut, "%0*d",  TDrugAnswerCode_len,			SPres[i].DrugAnswerCode		);
		nOut += sprintf (OBuffer + nOut, "%0*d",  1,							PR_Status[i]				);
		nOut += sprintf (OBuffer + nOut, "%0*d",  TUnits_len,					SPres[i].Units				);
		nOut += sprintf (OBuffer + nOut, "%0*d",  TOp_len,						SPres[i].Op					);
		nOut += sprintf (OBuffer + nOut, "%0*d",  TOpDrugPrice_len,				SPres[i].RetOpDrugPrice		);

		// 21NOV2002 Fee Per Service enh. begin.
		nOut += sprintf	(OBuffer + nOut, "%0*d",  TSupplierPrice_len,			SPres[i].SupplierDrugPrice	);
		// 21NOV2002 Fee Per Service enh. end.

		nOut += sprintf (OBuffer + nOut, "%0*d",  TMemberParticipatingType_len,	SPres[i].RetPartCode		);

		// Fixed Participation Price per Package.
		nOut += sprintf (OBuffer + nOut, "%0*d",  TOpDrugPrice_len,				SPres[i].PriceSwap			);

		nOut += sprintf (OBuffer + nOut, "%0*d",  TMemberDiscountPercent_len,	SPres[i].AdditionToPrice	);

		// DonR 15Jan2009: Because we now add a "hundreds" digit to this variable to indicate why
		// specialist participation was given, we need to ensure that only the last two digits are
		// sent to the pharmacy - thus we send the variable "MOD 100".
		nOut += sprintf (OBuffer + nOut, "%0*d",  2,							(SPres[i].InsPlusPtnSource % 100));

	}	// Output drug lines.


	// Return the size in Bytes of response message.
	*p_outputWritten			= nOut;
	*output_type_flg			= ANSWER_IN_BUFFER;
	ssmd_data_ptr->error_code	= v_ErrorCode;
	ssmd_data_ptr->pharmacy_num	= v_PharmNum;
	ssmd_data_ptr->terminal_num	= v_TerminalNum;


	RESTORE_ISOLATION;

	return  RC_SUCCESS;

}	// End of 2003/4 handler.



/*=======================================================================
||																		||
||			 HandlerToMsg_2005											||
||	Message handler for message 2005:									||
||        Electronic Prescription Drugs Delivery.						||
||																		||
||																		||
 =======================================================================*/

int HandlerToMsg_2005 (char			*IBuffer,
					   int			TotalInputLen,
					   char			*OBuffer,
					   int			*p_outputWritten,
					   int			*output_type_flg,
					   SSMD_DATA	*ssmd_data_ptr)

{
	// Local declarations
	ISOLATION_VAR;
	int				nOut;
	int				tries;
	int				i;
	int				j;
	int				is;
	int				reStart;
	int				ElyCount;
	int				flg;
	int				UnitsSellable;
	int				UnitsSold;
	TErrorCode		err;
	TErrorSevirity	Sever;
	TMonthLog		MonthLogCalc_next;
	TMonthLog		MonthLogCalc;
	short			NextMonthMinDay;
	ACTION_TYPE		AS400_Gadget_Action = enActWrite;

	// Message fields variables
	Msg1005Drugs				phDrgs					[MAX_REC_ELECTRONIC];
	short int					LineNumber				[MAX_REC_ELECTRONIC];
	short int					PR_SubstPermitted		[MAX_REC_ELECTRONIC];
	short int					PR_NumPerDose			[MAX_REC_ELECTRONIC];
	short int					PR_DosesPerDay			[MAX_REC_ELECTRONIC];
	short int					PR_HowParticDetermined	[MAX_REC_ELECTRONIC];
	short int					in_gadget_table			[MAX_REC_ELECTRONIC];
	short int					PRW_Op_Update			[MAX_REC_ELECTRONIC];
	short int					PrescSource				[MAX_REC_ELECTRONIC];
	int							PR_Date					[MAX_REC_ELECTRONIC];
	int							PR_Orig_Largo_Code		[MAX_REC_ELECTRONIC];
	int							PrevUnsoldUnits			[MAX_REC_ELECTRONIC];
	int							PrevUnsoldOP			[MAX_REC_ELECTRONIC];
	int							FixedParticipPrice		[MAX_REC_ELECTRONIC];
	int							PrescWrittenID			[MAX_REC_ELECTRONIC];
	TMemberDiscountPercent		v_MemberDiscPC			[MAX_REC_ELECTRONIC];
	TDoctorRecipeNum			v_DoctorRecipeNum		[MAX_REC_ELECTRONIC];
	TDate						StopBloodDate			[MAX_REC_ELECTRONIC];

	// DonR 12Feb2006: Ingredients-sold arrays.
	short int					ingr_code				[MAX_REC_ELECTRONIC] [3];
	double						ingr_quant				[MAX_REC_ELECTRONIC] [3];
	double						ingr_quant_std			[MAX_REC_ELECTRONIC] [3];
	double						ingr_quant_bot			[MAX_REC_ELECTRONIC] [3];
	double						ingr_quant_bot_std		[MAX_REC_ELECTRONIC] [3];
	double						per_quant				[MAX_REC_ELECTRONIC] [3];
	double						package_volume			[MAX_REC_ELECTRONIC];

	int							PW_ID;
	short int					PRW_OpUpdate;
	short int					PW_status;
	int							PD_discount_pc;

	int							v_ReasonForDiscount;
	int							v_ClientLocationCode;
	int							v_DocLocationCode;
	short int					v_ReasonToDispense;
	short int					v_in_gadget_table;
	int							v_TotalCouponDisc;

	// DonR 12Feb2006: Ingredients-sold fields.
	double						v_package_volume;
	short int					v_ingr_1_code;
	short int					v_ingr_2_code;
	short int					v_ingr_3_code;
	double						v_ingr_1_quant;
	double						v_ingr_2_quant;
	double						v_ingr_3_quant;
	double						v_ingr_1_quant_std;
	double						v_ingr_2_quant_std;
	double						v_ingr_3_quant_std;
	double						v_per_1_quant;
	double						v_per_2_quant;
	double						v_per_3_quant;

	PHARMACY_INFO				Phrm_info;
	TDrugListRow				DL;
	TPharmNum					v_PharmNum;
	TInstituteCode				v_InstituteCode;
	TTerminalNum				v_TerminalNum;
	TMemberBelongCode			v_MemberBelongCode;
	TGenericTZ					v_MemberID;
	TIdentificationCode			v_IDCode;
	TMoveCard					v_MoveCard;
	TTypeDoctorIdentification	v_DocIDType ;
	TGenericTZ					v_DoctorID;
	TDoctorIdInputType			v_DoctorIdInputType;
	TRecipeSource				v_RecipeSource;
	TInvoiceNum					v_InvoiceNum;
	TGenericTZ					v_UserIdentification;
	TPharmRecipeNum				v_PharmRecipeNum;
	TRecipeIdentifier			v_RecipeIdentifier;
	TMemberParticipatingType	v_MemberParticType;
	TDeliverConfirmationDate	v_DeliverConfirmationDate;
	TDeliverConfirmationHour	v_DeliverConfirmationHour;
	TMonthLog					v_MonthLog;
	TNumOfDrugLinesRecs			v_NumOfDrugLinesRecs;
	TCreditYesNo				v_CreditYesNo;
	short						paid_for	= 1;	// DonR 05Feb2023 User Story #232220 - always TRUE for 2005/5005.

	TNumofSpecialConfirmation		v_SpecialConfNum;
	TSpecialConfirmationNumSource	v_SpecialConfSource;

	// Prescription Lines structure
	Msg1005Drugs				*phDrgPtr;
	Msg1005Drugs				dbDrgs;

	// Gadgets/Gadget_spool tables.
	short int					v_action;
	int							v_service_code;
	int							v_gadget_code;
	short int					v_service_number;
	short int					v_service_type;
	TDrugCode					v_DrugCode;
	TOpDrugPrice				Unit_Price;
	TDoctorRecipeNum			v_DoctorPrID;
	int							v_DoctorPrDate;
	short int					PRW_OriginCode;
	TErrorCode					GadgetError;

	// Return-message variables
	TDate						SysDate;
	TDate						v_Date;
	THour						v_Hour;
	TErrorCode					v_ErrorCode;


	// Miscellaneous DB variables
	TPharmNum					sq_pharmnum;
	TInstituteCode				sq_institutecode;
	TTerminalNum				sq_terminalnum;
	TPriceListNum				sq_pricelistnum;
	TDoctorRecipeNum			sq_doctorprescid;
	TMemberBelongCode			sq_memberbelongcode;
	TGenericTZ					sq_MemberID;
	TIdentificationCode			sq_identificationcode;
	TTypeDoctorIdentification	sq_DocIDType;
	TGenericTZ					sq_doctoridentification;
	TRecipeSource				sq_recipesource;
	TNumOfDrugLinesRecs			sq_numofdruglinesrecs;
	TErrorCode					sq_errorcode;
	TErrorCode					prev_pr_errorcode;
	TTotalMemberParticipation	sq_TotalMemberPrice;
	TTotalDrugPrice				sq_TotalDrugPrice;
	TTotalDrugPrice				sq_TotSuppPrice;
	TMemberDiscountPercent		sq_member_discount;
	int							sq_PW_ID;
	int							sq_DrPrescNum;
	TDate						sq_stopusedate;
	TDate						sq_maxdrugdate;
	short						sq_PrescSource;
	int							AuthDate;
	int							PRW_PrescDate;
	TDate						max_drug_date;
	short int					sq_thalf;
	TDate						sq_DateDB;
	TMonthLog					sq_MonthLogDB;
	int							v_count_double_rec;
	short						v_MemberSpecPresc;
	TDate						sq_StopBloodDate;


	// Body of function.

	// Initialize variables.
	REMEMBER_ISOLATION;
	PosInBuff	= IBuffer;
	v_ErrorCode	= NO_ERROR;
	SysDate		= GetDate ();
	memset ((char *)&phDrgs [0],	0, sizeof (phDrgs));
	memset ((char *)&dbDrgs,		0, sizeof (dbDrgs));



	// Read message fields data into variables.
	v_PharmNum					= GetLong	(&PosInBuff, TPharmNum_len					); CHECK_ERROR();
	v_InstituteCode				= GetShort	(&PosInBuff, TInstituteCode_len				); CHECK_ERROR();
	v_TerminalNum				= GetShort	(&PosInBuff, TTerminalNum_len				); CHECK_ERROR();
	v_MemberID					= GetLong	(&PosInBuff, TMemberIdentification_len		); CHECK_ERROR();
	v_IDCode					= GetShort	(&PosInBuff, TIdentificationCode_len		); CHECK_ERROR();
	v_MemberBelongCode			= GetShort	(&PosInBuff, TMemberBelongCode_len			); CHECK_ERROR();	 
	v_MoveCard					= GetShort	(&PosInBuff, TMoveCard_len					); CHECK_ERROR();	 
	v_DocIDType					= GetShort	(&PosInBuff, TTypeDoctorIdentification_len	); CHECK_ERROR();	 
	v_DoctorID					= GetLong	(&PosInBuff, TDoctorIdentification_len		); CHECK_ERROR();	 
	v_DoctorIdInputType			= GetShort	(&PosInBuff, TDoctorIdInputType_len			); CHECK_ERROR();	 
	v_RecipeSource			  	= GetShort	(&PosInBuff, TRecipeSource_len				); CHECK_ERROR();	 
	v_UserIdentification		= GetLong	(&PosInBuff, TUserIdentification_len		); CHECK_ERROR();	 
	v_SpecialConfSource			= GetShort	(&PosInBuff, TSpecConfNumSrc_len			); CHECK_ERROR();
	v_SpecialConfNum			= GetLong	(&PosInBuff, TNumofSpecialConfirmation_len	); CHECK_ERROR();
	v_PharmRecipeNum			= GetLong	(&PosInBuff, TPharmRecipeNum_len			); CHECK_ERROR();
	v_RecipeIdentifier			= GetLong	(&PosInBuff, TRecipeIdentifier_len			); CHECK_ERROR();	 
	v_ReasonForDiscount			= GetLong	(&PosInBuff, TElectPR_DiscntReasonLen		); CHECK_ERROR();
	v_DeliverConfirmationDate	= GetLong	(&PosInBuff, TDeliverConfirmationDate_len	); CHECK_ERROR();
	v_DeliverConfirmationHour	= GetLong	(&PosInBuff, TDeliverConfirmationHour_len	); CHECK_ERROR();	 

	// Note: This will be the Credit Type Used. In some circumstances, it may be
	// different from the Credit Type that was sent to the pharmacy - for example,
	// the member may elect to pay in cash even though s/he has an automated
	// payment ("orat keva") set up. This value will be stored in the new database
	// field credit_type_used.
	v_CreditYesNo				= GetShort	(&PosInBuff, TCreditYesNo_len				); CHECK_ERROR();
	v_InvoiceNum			 	= GetLong	(&PosInBuff, TInvoiceNum_len				); CHECK_ERROR();	 
	v_ClientLocationCode		= GetLong	(&PosInBuff, TDeviceCode_len				); CHECK_ERROR();	 
	v_DocLocationCode			= GetLong	(&PosInBuff, TDeviceCode_len				); CHECK_ERROR();	 
	v_MonthLog					= GetShort	(&PosInBuff, TMonthLog_len					); CHECK_ERROR();
	v_ReasonToDispense			= GetShort	(&PosInBuff, TElectPR_ReasonToDispLen		); CHECK_ERROR();
	v_NumOfDrugLinesRecs		= GetShort	(&PosInBuff, TElectPR_LineNumLen			); CHECK_ERROR();


	// Get repeating portion of message
	for (i = 0; i < v_NumOfDrugLinesRecs; i++)
	{
		LineNumber			[i]		= GetShort	(&PosInBuff, TElectPR_LineNumLen			); CHECK_ERROR();
		v_DoctorRecipeNum	[i]		= GetLong	(&PosInBuff, TDoctorRecipeNum_len			); CHECK_ERROR();	 
		PR_Date				[i]		= GetLong	(&PosInBuff, TDate_len						); CHECK_ERROR();
		PR_Orig_Largo_Code	[i]		= GetLong	(&PosInBuff, TDrugCode_len					); CHECK_ERROR();
		phDrgs[i].DrugCode			= GetLong	(&PosInBuff, TDrugCode_len					); CHECK_ERROR();
		PR_SubstPermitted	[i]		= GetShort	(&PosInBuff, TElectPR_SubstPermLen			); CHECK_ERROR();
		PrevUnsoldUnits		[i]		= GetLong	(&PosInBuff, TUnits_len						); CHECK_ERROR();
		PrevUnsoldOP		[i]		= GetLong	(&PosInBuff, TUnits_len						); CHECK_ERROR();
		phDrgs[i].Units				= GetLong	(&PosInBuff, TUnits_len						); CHECK_ERROR();
		phDrgs[i].Op				= GetLong	(&PosInBuff, TOp_len						); CHECK_ERROR();
		PR_NumPerDose		[i]		= GetShort	(&PosInBuff, TUnits_len						); CHECK_ERROR();
		PR_DosesPerDay		[i]		= GetShort	(&PosInBuff, TUnits_len						); CHECK_ERROR();
		phDrgs[i].Duration			= GetShort	(&PosInBuff, TDuration_len					); CHECK_ERROR();

		phDrgs[i].LinkDrugToAddition= GetLong	(&PosInBuff, TLinkDrugToAddition_len		); CHECK_ERROR();

		phDrgs[i].OpDrugPrice		= GetLong	(&PosInBuff, TOpDrugPrice_len				); CHECK_ERROR();

		// 21NOV2002 Fee Per Service enh. begin.
		phDrgs[i].SupplierDrugPrice	= GetLong	(&PosInBuff, TSupplierPrice_len				); CHECK_ERROR();
		// 21NOV2002 Fee Per Service enh. end.

		v_MemberParticType			= GetShort	(&PosInBuff, TMemberParticipatingType_len	); CHECK_ERROR();	 
		FixedParticipPrice	[i]		= GetLong	(&PosInBuff, TOpDrugPrice_len				); CHECK_ERROR();
		v_MemberDiscPC		[i]		= GetShort	(&PosInBuff, TMemberDiscountPercent_len		); CHECK_ERROR();	 

		phDrgs[i].TotalDrugPrice	= GetLong	(&PosInBuff, TTotalDrugPrice_len			); CHECK_ERROR();

		phDrgs[i].TotalMemberParticipation
									= GetLong	(&PosInBuff, TTotalMemberParticipation_len	); CHECK_ERROR();

		// Normally, this can be ignored - since it's written to the DB by Trn. 2003.
		PR_HowParticDetermined [i]	= GetShort	(&PosInBuff, TElectPR_HowPartDetLen			); CHECK_ERROR();

		phDrgs[i].CurrentStockOp	= GetLong	(&PosInBuff, TElectPR_Stock_len				); CHECK_ERROR();

		phDrgs[i].CurrentStockInUnits
									= GetLong	(&PosInBuff, TElectPR_Stock_len				); CHECK_ERROR();


		// DonR 12Jul2005: Avoid DB errors due to ridiculously high OP numbers from pharmacy.
		PRW_Op_Update[i] = ((phDrgs[i].Op <= 32767) && (phDrgs[i].Op >= 0)) ? phDrgs[i].Op : 32767;

	}	// Drug-line reading loop.


	// Get amount of input not eaten
	ChckInputLen( TotalInputLen - (PosInBuff - IBuffer) );
	CHECK_ERROR();

	// Copy values to ssmd_data structure.
	ssmd_data_ptr->pharmacy_num		= v_PharmNum;
	ssmd_data_ptr->member_id		= v_MemberID;
	ssmd_data_ptr->member_id_ext	= v_IDCode;

	// Validate Date and Time.
	ChckDate (v_DeliverConfirmationDate);
	CHECK_ERROR();	 

	ChckTime (v_DeliverConfirmationHour);
	CHECK_ERROR();

	// DonR 27Jun2010: The subsidy amount, sent in the first four digits of "Reason for discount",
	// is now being written to a different field: Total Coupon Discount, which is passed to the
	// AS/400 field RK9021P/EAZOZR, replacing the old discount amount EADSCNTP.
	v_TotalCouponDisc = v_ReasonForDiscount / 10;	// The last digit goes to EACRCDE.
	v_TotalCouponDisc *= 100;						// The new field is in agorot, not shekels.
	v_ReasonForDiscount = v_ReasonForDiscount % 10;	// Keep only the last digit - so EADSCNTP will always be 0.

	
	reStart		= MAC_TRUE;


	// SQL Retry Loop.
	for (tries = 0; tries < SQL_UPDATE_RETRIES && reStart == MAC_TRUE; tries++)
	{
		// Initialize response variables.
		v_ErrorCode					= NO_ERROR;
		reStart						= MAC_FALS;
		SysDate						= GetDate ();
		v_Date						= GetDate ();
		v_Hour						= GetTime ();
		max_drug_date				= 0;

		memset ((char *)&dbDrgs, 0, sizeof (dbDrgs));


		// Dummy loop to avoid GOTO.
		do		// Exiting from LOOP will send the reply to pharmacy.
		{
			// Test pharmacy data.
			v_ErrorCode = IS_PHARMACY_OPEN (v_PharmNum, v_InstituteCode, &Phrm_info);

			if (v_ErrorCode != MAC_OK)
			{
				break;
			}


			// DonR 24Jul2013: Removed doctor-equals-member test from Trn. 2005 and 5005; this is already checked
			// in Trn. 2003/5003, and there's no reason to check it again.

			// DonR 25Jan2005:
			// Pharmacy Messages were being checked here - but we weren't returning
			// the answer, because the answer was left out of the specification.
			// Instead of adding a field to Message 2006, we moved the check to
			// Transaction 2003 and we send the result in a previously-unused field
			// in Message 2004 (which was Message Urgent Flag).

			// 20020109
			// if v_MonthLog different from MonthLog  now must check
			// if lasts days of the months =>   possible next month
			MonthLogCalc		= (v_Date / 100) % 10000;	// YYMM.
			MonthLogCalc_next	= MonthLogCalc;
			NextMonthMinDay = ((MonthLogCalc % 100) == 2) ? 23 : 25;

			if (v_MonthLog != MonthLogCalc)
			{
				if (v_Date % 100 > NextMonthMinDay)	// After 25th of current month (or 23rd if it's February).
				{
					if (MonthLogCalc % 100 == 12)	// It's December today.
					{
						MonthLogCalc_next = MonthLogCalc + 89;	// January of the next year.
					}
					else
					{
						MonthLogCalc_next = MonthLogCalc + 1;	// Advance one month in same year.
					}

					if (v_MonthLog != MonthLogCalc_next)
					{
						v_ErrorCode = ERR_PRESCR_PROBL_DIARY_MONTH;   // Date of sale problem.
					}
				}

				else
				{
					v_ErrorCode = ERR_PRESCR_PROBL_DIARY_MONTH;   // Date of sale problem.
				}
			}	// v_MonthLog != MonthLogCalc



			// Get Member data.
			if ((v_MemberID == 0) && (v_RecipeSource != RECIP_SRC_NO_PRESC))
			{
				v_ErrorCode = ERR_MEMBER_ID_CODE_WRONG;
				break;
			}

			// DonR 05Sep2006: Don't bother checking for non-Maccabi OTC purchases.
			if (v_MemberID > 0)
			{
				ExecSQL (	MAIN_DB, READ_members_MaxDrugDate_and_SpecPresc,
							&sq_maxdrugdate,	&v_MemberSpecPresc,
							&v_MemberID,		&v_IDCode,
							END_OF_ARG_LIST										);

				Conflict_Test (reStart);

				if ( SQLERR_code_cmp (SQLERR_not_found) == MAC_TRUE)
				{
					v_ErrorCode = ERR_MEMBER_ID_CODE_WRONG;
					break;
				}

				if (SQLERR_error_test ())
				{
					v_ErrorCode = ERR_DATABASE_ERROR;
					break;
				}
			}
			else
			{
				// Set values for non-Maccabi OTC purchase.
				sq_maxdrugdate = v_MemberSpecPresc = 0;
			}



			// User prescription global data.
			ExecSQL (	MAIN_DB, TR2005_READ_prescriptions,
						&sq_pharmnum,				&sq_institutecode,
						&sq_identificationcode,		&sq_MemberID,
						&sq_memberbelongcode,		&sq_doctoridentification,
						&sq_DocIDType,				&sq_recipesource,
						&sq_errorcode,				&sq_numofdruglinesrecs,
						&sq_terminalnum,			&sq_pricelistnum,
						&sq_member_discount,		&sq_DateDB,
						&sq_MonthLogDB,
						&v_RecipeIdentifier,		END_OF_ARG_LIST					);

			Conflict_Test (reStart);

			if ( SQLERR_code_cmp( SQLERR_not_found ) == MAC_TRUE )
			{
				v_ErrorCode = ERR_PRESCRIPTION_ID_NOT_FOUND;
				break;
			}

			if ( SQLERR_error_test() )
			{
				v_ErrorCode = ERR_DATABASE_ERROR;
				break;
			} 


			// DonR 24Jul2013: Remember the previous error code, so we don't overwrite it.
			prev_pr_errorcode = sq_errorcode;

			// Logical data tests.
			if (sq_errorcode != NO_ERROR)
			{
				Sever = GET_ERROR_SEVERITY (sq_errorcode);

				if (Sever >= FATAL_ERROR_LIMIT)
				{
					v_ErrorCode = ERR_PRESCRIPTIONID_NOT_APPROVED;
					break;
				}
			}


			ElyCount = 0;

			// Insert the following line after each line in if()
			// in order to catch the first true among all conditions :
			//    || ! ++ElyCount	
			//
			if ((sq_pharmnum				!= v_PharmNum			) || ! ++ElyCount ||
				(sq_institutecode			!= v_InstituteCode		) || ! ++ElyCount ||
				(sq_MemberID				!= v_MemberID			) || ! ++ElyCount ||
				(sq_identificationcode		!= v_IDCode				) || ! ++ElyCount ||
				(sq_memberbelongcode		!= v_MemberBelongCode	) || ! ++ElyCount ||
				(sq_doctoridentification	!= v_DoctorID			) || ! ++ElyCount ||
				(sq_DocIDType				!= v_DocIDType			) || ! ++ElyCount ||
				(sq_recipesource			!= v_RecipeSource		) || ! ++ElyCount ||
				(sq_numofdruglinesrecs		<  v_NumOfDrugLinesRecs	) || ! ++ElyCount ||
				(sq_terminalnum				!= v_TerminalNum		) || ! ++ElyCount
			   )
			{
				v_ErrorCode = ERR_PRESCRIPTION_ID_NOT_EQUAL;
				break;
			}


			// Test prescription drugs data.
			if (sq_DateDB != v_DeliverConfirmationDate )   //20020109
			{
				v_ErrorCode = ERR_PRESCRIPTION_PROBL_DATE;   //date of sell problem
				break;
			}

			// DonR 04 Mar 2003: Skip this test if the pharmacy hasn't given us a
			// Pharmacy Prescription Number.
			// DonR 23Jul2006: Don't do this test if the sale is coming from a
			// private pharmacy and the Member ID is zero.
			if ((v_RecipeIdentifier > 0)					&&
				(v_PharmRecipeNum > 0)						&&
				((MACCABI_PHARMACY) || (v_RecipeSource != RECIP_SRC_NO_PRESC)))
			{
				// Check for duplicate Pharmacy Prescription Number.
				// DonR 29Jul2007: Re-ordered selects to follow index structure; also added
				// a "dummy" select by member_price_code, just to make sure that Informix
				// uses the best applicable index.
				//
				// DonR 01Dec2009: This check doesn't need to be in committed mode, so we can set isolation
				// to "dirty" to avoid locked-record errors.
				//
				// DonR 29Jun2010: Got rid of dummy member_price_code select; I'll change the relevant index
				// to eliminate this irrelevant field as well. Also got rid of the select by date, which
				// hasn't been relevant for years.
				// DonR 08Feb2011: Change test on delivered_flg, since we're no longer using only 1 and 0 there.
				SET_ISOLATION_DIRTY;

				ExecSQL (	MAIN_DB, READ_count_duplicate_drug_sales,
							&v_count_double_rec,
							&v_PharmNum,			&v_MonthLog,			&v_PharmRecipeNum,
							&v_MemberBelongCode,	&DRUG_NOT_DELIVERED,	END_OF_ARG_LIST		);

				Conflict_Test (reStart);

				if (SQLERR_code_cmp (SQLERR_not_found) == MAC_TRUE)
				{
					v_ErrorCode = ERR_PRESCRIPTION_ID_NOT_FOUND;
					break;
				}

				if (SQLERR_error_test ())
				{
					v_ErrorCode = ERR_DATABASE_ERROR;
					break;
				} 

				if (v_count_double_rec > 0 )   //20020115
				{
					v_ErrorCode = ERR_PRESCRIPTION_DOUBLE_NUM;   //double presc_pharm number
					break;
				}

			}	// RecipeIdentifier > 0 and Pharmacy Prescr. ID > 0


			// Open Prescription Drugs cursor.
			// DonR 01Dec2009: Return to committed-read mode.
			SET_ISOLATION_COMMITTED;

			DeclareAndOpenCursor (	MAIN_DB, TR2005_presc_drug_cur,
									&v_RecipeIdentifier, &DRUG_NOT_DELIVERED, END_OF_ARG_LIST	);

			if (SQLERR_error_test ())
			{
				v_ErrorCode = ERR_DATABASE_ERROR;
				break;
			}

			// Set all drug lines from 2005 to "unmatched".
			for (i = 0; i < v_NumOfDrugLinesRecs; i++)
			{
				phDrgs[i].Flg = 0;
			}



			// Fetch and test prescription_drugs rows against drug lines in 2005.
			// Loop until we break out.
			do
			{
				FetchCursorInto (	MAIN_DB, TR2005_presc_drug_cur,
									&dbDrgs.DrugCode,			&dbDrgs.Op,
									&dbDrgs.Units,				&dbDrgs.LineNo,
									&dbDrgs.LinkDrugToAddition,	&dbDrgs.OpDrugPrice,
									&dbDrgs.SupplierDrugPrice,	&sq_errorcode,
									&sq_stopusedate,			&sq_thalf,
									&sq_PW_ID,					&sq_DrPrescNum,
									&dbDrgs.ParticipMethod,		&dbDrgs.MacabiPriceFlag,
									&sq_StopBloodDate,			&sq_PrescSource,
									END_OF_ARG_LIST											);

				Conflict_Test( reStart );

				if( SQLERR_code_cmp( SQLERR_end_of_fetch ) == MAC_TRUE )
				{
					break;
				}

				if (SQLERR_error_test ())
				{
					SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR);
					break;
				}


				// Find drug in buffer and test it against database values.
				for (i = 0; i < v_NumOfDrugLinesRecs; i++)
				{
					if ((dbDrgs.DrugCode != phDrgs[i].DrugCode) || (phDrgs[i].Flg   != 0))
					{
						continue;
					}

					phDrgs[i].LineNo			= dbDrgs.LineNo;
					phDrgs[i].ParticipMethod	= dbDrgs.ParticipMethod;
					phDrgs[i].MacabiPriceFlag	= dbDrgs.MacabiPriceFlag;
					phDrgs[i].Flg				= 1;
					phDrgs[i].DiscountPercent	= v_MemberDiscPC[i];
					PrescWrittenID	[i]			= sq_PW_ID;
					StopBloodDate	[i]			= sq_StopBloodDate;
					PrescSource		[i]			= sq_PrescSource;

					// DonR 09Feb2009: Per Iris Shaya, certain pharmacies (SuperPharm
					// in Sderot and Tamarim in Eilat) can change the Supplier Drug Price
					// for certain items; in this case, we want to update the drug-sale
					// row with the new Supplier Drug Price. Since other "Maccabi"
					// pharmacies won't change the price, doing the update won't
					// change anything. But if the conditions aren't met, we want
					// to make sure the existing database value is preserved.
					if ((dbDrgs.DrugCode < 90000) || (PRIVATE_PHARMACY))
					{
						phDrgs[i].SupplierDrugPrice = dbDrgs.SupplierDrugPrice;
					}

					// DonR 18Apr2005: If Doctor Prescription ID is different from what
					// pharmacy reported, just assume that the pharmacy value is bogus
					// and replace it.
					if (sq_DrPrescNum					!= v_DoctorRecipeNum[i])
					{
						v_DoctorRecipeNum[i] = sq_DrPrescNum;
					}

					// Test for same Link to Addition.
					if (phDrgs[i].LinkDrugToAddition	!= dbDrgs.LinkDrugToAddition)
					{
						v_ErrorCode = ERR_PRESCRIPTION_ID_NOT_EQUAL;
//						GerrLogReturn (GerrId,
//									   "Link-to-addition mismatch: 2003 = %d, 2005 = %d.",
//									   phDrgs[i].LinkDrugToAddition,
//									   dbDrgs.LinkDrugToAddition);
						break;
					}


					// See if any errors discovered so far are severe enough
					// to invalidate the drug.
					if (sq_errorcode != NO_ERROR)
					{
						Sever = GET_ERROR_SEVERITY (sq_errorcode);

						if (Sever > FATAL_ERROR_LIMIT)
						{
							v_ErrorCode = ERR_PRESCRIPTIONID_NOT_APPROVED;
							break;
						}
					}


					// Update max drug date for member
					len   = AddDays (sq_stopusedate, sq_thalf);

					if (max_drug_date < len)
					{
						max_drug_date = len;
					}

					break;

				}	// Loop through drugs to match lines in 2005 against
					// the current prescription_drugs row.

				// See if any errors discovered so far are severe enough
				// to invalidate the drug.
				if( v_ErrorCode != NO_ERROR )
				{
					Sever = GET_ERROR_SEVERITY (v_ErrorCode);

					if (Sever > FATAL_ERROR_LIMIT)
					{
						break;
					}
				}

			}
			while (1);	// End of fetch-and-test prescription_drugs rows loop.


			// Close Prescription Drugs cursor.
			CloseCursor (	MAIN_DB, TR2005_presc_drug_cur	);
			SQLERR_error_test ();


			// Test if any drugs in 2005 were not found in prescription_drugs table -
			// pharmacist is allowed to add drugs between 2001 and 2003, but
			// not between 2003 and 2005.
			//
			// While we're at it, check that drug is in the drug_list table,
			// load in its Largo Type and packaging details, and see if the
			// drug is also in the Gadgets table.

			// DonR 21Apr2005: Avoid unnecessary "table locked" errors.
			SET_ISOLATION_DIRTY;

			for (i = 0; i < v_NumOfDrugLinesRecs; i++)
			{
				in_gadget_table[i] = 0;

				if (phDrgs[i].Flg == 0)		// I.E. drug wasn't in DB select list.
				{
					v_ErrorCode = ERR_PRESCRIPTION_ID_NOT_EQUAL;
					break;
				}


				// Read drug_list data by largo code
				// DonR 14Apr2016: Switch to using the read_drug() routine, since that includes more sophisticated
				// data-massaging logic.
				if (read_drug (phDrgs[i].DrugCode,
							   99999999,
							   &Phrm_info,
							   false,	// Deleted drugs are "invisible".
							   &DL,
							   NULL))
				{
					phDrgs[i].LargoType			= DL.largo_type;
					phDrgs[i].package_size		= DL.package_size;
					phDrgs[i].openable_package	= DL.openable_pkg;
					v_in_gadget_table			= DL.in_gadget_table;
					v_ingr_1_code				= DL.Ingredient[0].code;
					v_ingr_2_code				= DL.Ingredient[1].code;
					v_ingr_3_code				= DL.Ingredient[2].code;
					v_ingr_1_quant				= DL.Ingredient[0].quantity;
					v_ingr_2_quant				= DL.Ingredient[1].quantity;
					v_ingr_3_quant				= DL.Ingredient[2].quantity;
					v_ingr_1_quant_std			= DL.Ingredient[0].quantity_std;
					v_ingr_2_quant_std			= DL.Ingredient[1].quantity_std;
					v_ingr_3_quant_std			= DL.Ingredient[2].quantity_std;
					v_per_1_quant				= DL.Ingredient[0].per_quant;
					v_per_2_quant				= DL.Ingredient[1].per_quant;
					v_per_3_quant				= DL.Ingredient[2].per_quant;
					v_package_volume			= DL.package_volume;
				}
				else
				{
					Conflict_Test (reStart);

					if (SQLERR_code_cmp (SQLERR_not_found) == MAC_TRUE)
					{
						v_ErrorCode = ERR_DRUG_CODE_NOT_FOUND;
						break;
					}

					if (SQLERR_error_test ())
					{
						v_ErrorCode = ERR_DATABASE_ERROR;
						break;
					}
				}

				// Save the drug's "in gadget table" status.
				in_gadget_table[i] = v_in_gadget_table;

				// Save generic-ingredient fields as well.
				package_volume	[i]    = v_package_volume;
				ingr_code		[i][0] = v_ingr_1_code;
				ingr_code		[i][1] = v_ingr_2_code;
				ingr_code		[i][2] = v_ingr_3_code;
				ingr_quant		[i][0] = v_ingr_1_quant;
				ingr_quant		[i][1] = v_ingr_2_quant;
				ingr_quant		[i][2] = v_ingr_3_quant;
				ingr_quant_std	[i][0] = v_ingr_1_quant_std;
				ingr_quant_std	[i][1] = v_ingr_2_quant_std;
				ingr_quant_std	[i][2] = v_ingr_3_quant_std;
				per_quant		[i][0] = v_per_1_quant;
				per_quant		[i][1] = v_per_2_quant;
				per_quant		[i][2] = v_per_3_quant;

			}	// Loop on drugs to get largo type, package size, and openability.


			// DonR 21Apr2005: Restore "normal" database isolation.
			SET_ISOLATION_COMMITTED;


			// If we broke out of the above loop prematurely, something went wrong!
			if (i < v_NumOfDrugLinesRecs)
				break;


			// End of dummy loop to prevent goto.
		}
		while (0);	// Loop should run only once.

		// DonR 01Dec2009: Just to be on the safe side, restore normal database isolation.
		// This should be necessary only if we hit a DB error while we were in "dirty read"
		// mode.
		SET_ISOLATION_COMMITTED;


		// Update tables.
		//
		// And now...
		// Yet another dummy loop to avoid goto!
		// Reply is sent to pharmacy when we exit from loop.

		// DonR 04Aug2010: If there is an error, write it to the prescriptions table so we know
		// why the sale wasn't completed.
		if (!SetErrorVar (&v_ErrorCode, v_ErrorCode))	// No problem so far
		{
			do
			{
				// Update PHARMACY_DAILY_SUM table.
				// DonR 05Aug2009: Don't add private-pharmacy non-prescription sales to pharmacy_daily_sum.
				if ((MACCABI_PHARMACY) || (v_RecipeSource != RECIP_SRC_NO_PRESC))
				{
					// Calculate prescription total
					sq_TotalMemberPrice	= 0;
					sq_TotalDrugPrice	= 0;
					sq_TotSuppPrice		= 0;

					for (i = 0; i < v_NumOfDrugLinesRecs; i++)
					{
						sq_TotalMemberPrice	+= phDrgs[i].TotalMemberParticipation;
						sq_TotalDrugPrice	+= phDrgs[i].TotalDrugPrice;

						sq_TotSuppPrice		+= phDrgs[i].SupplierDrugPrice * phDrgs[i].Op;

						if (phDrgs[i].Units > 0)
						{
							sq_TotSuppPrice	+= (int)(  (float)phDrgs[i].SupplierDrugPrice
													 * (float)phDrgs[i].Units
													 / (float)phDrgs[i].package_size);
						}
					}

					// DonR 21Mar2011: For better performance, try updating rather than inserting first.
					// The vast majority of the time, we'll be updating an existing row - so why waste
					// time trying to insert, failing, and only then updating?
					ExecSQL (	MAIN_DB, TR2005_UPD_pharmacy_daily_sum,
								&sq_TotalDrugPrice,		&sq_TotalMemberPrice,	&sq_TotSuppPrice,
								&v_NumOfDrugLinesRecs,
								&v_PharmNum,			&v_InstituteCode,		&v_MonthLog,
								&SysDate,				&v_TerminalNum,			END_OF_ARG_LIST		);

					// If there was nothing to update, try inserting a new row.
					if (SQLERR_code_cmp (SQLERR_no_rows_affected) == MAC_TRUE)
					{
						ExecSQL (	MAIN_DB, INS_pharmacy_daily_sum_19_columns,
									&v_PharmNum,		&v_InstituteCode,		&v_MonthLog,
									&SysDate,			&v_TerminalNum,			&sq_TotalDrugPrice,
									&IntOne,			&v_NumOfDrugLinesRecs,	&sq_TotalMemberPrice,
									&sq_TotSuppPrice,	&IntZero,				&IntZero,

									&IntZero,			&IntZero,				&IntZero,
									&IntZero,			&IntZero,				&IntZero,
									&IntZero,			END_OF_ARG_LIST								);
					}

					Conflict_Test (reStart);

					if (SQLERR_error_test ())
					{
						SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR);
						break;
					}
				}	// DonR 05Aug2009: Insert/update to pharmacy_daily_sum is now conditional.


				// Update PRESCRIPTION_DRUGS table.
				for (i = 0; i < v_NumOfDrugLinesRecs; i++)
				{
					phDrgPtr		= &phDrgs			[i];
					PW_ID			= PrescWrittenID	[i];
					PD_discount_pc	= v_MemberDiscPC	[i];
					sq_StopBloodDate= StopBloodDate		[i];

					// Update doctor_presc table to reflect delivery.
					//
					// DonR 04May2005: Set Reported-to-AS400 flag false only for "real" doctor
					// prescriptions, not those created by Pharmacy transactions.
					if (PrescSource[i] == RECIP_SRC_MACABI_DOCTOR)	// DonR 27Apr2015: Don't bother for non-Maccabi-doctor prescriptions.
					{
						update_doctor_presc (	0,		// Normal mode - NOT processing late-arriving doctor prescriptions.
												1,		// Only sales in 2005.
												v_MemberID,					v_IDCode,
												v_RecipeIdentifier,			phDrgPtr->LineNo,
												v_DoctorID,					v_DocIDType,					v_DoctorRecipeNum[i],
												PR_Orig_Largo_Code[i],		phDrgPtr->DrugCode,				NULL,	// No TDrugListRow structure.
												PR_Date[i],					SysDate,						v_Hour,
												phDrgPtr->Op,				phDrgPtr->Units,
												PrevUnsoldOP[i],			PrevUnsoldUnits[i],
												0 /*phDrgPtr->use_instr_template */ ,			// DonR 01Aug2024 Bug #334612: Get "ratzif" from pharmacy instead of from doctor Rx.
												0,		// Trn. 2005 doesn't handle deletions, so no DelPrescID.
												1,							NULL,	// Old-style single-prescription mode.
												&PR_Orig_Largo_Code[i],		&PR_Date[i],
												&phDrgs[i].NumRxLinks,
												NULL,	NULL,	NULL,	NULL		// Doctor visit & Rx arrival timestamps - relevant only for spool processing.
											);
					}	// Prescription Source == Maccabi Doctor.


					// DonR 14Jun2004: If "drug" being purchased is a device in the "gadgets"
					// table, retrieve the appropriate information and notify the AS400 of
					// the sale.
					if ((phDrgs[i].ParticipMethod % 10) == FROM_GADGET_APP)
					{
						v_gadget_code	= phDrgs[i].LinkDrugToAddition;
						v_DoctorPrID	= v_DoctorRecipeNum	[i];	// Unless these are bogus values 
						v_DoctorPrDate	= PR_Date			[i];	// inserted by pharmacy in Trn. 2003.
						v_DrugCode		= phDrgs[i].DrugCode;
						Unit_Price		= phDrgs[i].OpDrugPrice;	// Unless Maccabi Price is relevant.

						ExecSQL (	MAIN_DB, READ_Gadgets_for_sale_completion,
									&v_service_code,	&v_service_number,	&v_service_type,
									&v_DrugCode,		&v_gadget_code,		&v_gadget_code,
									END_OF_ARG_LIST												);

						Conflict_Test (reStart);

						if (SQLERR_error_test ())
						{
							SetErrorVar( &v_ErrorCode, ERR_DATABASE_ERROR );
							break;
						}

						phDrgs[i].MacabiPriceFlag = 0;	// Reset flag in prescription_drugs.

						// If we get here, the gadget entry really does exist.
						// DonR 19Aug2025 User Story #442308: If Nihul Tikrot calls are disabled, disable Meishar calls as well.
						if (TikrotRPC_Enabled)
						{
							GadgetError = as400SaveSaleRecord ((int)v_PharmNum,
															   (int)v_RecipeIdentifier,
															   (int)v_MemberID,
															   (int)v_IDCode,
															   (int)v_service_code,
															   (int)v_service_number,
															   (int)v_service_type,
															   (int)v_DrugCode,
															   (int)phDrgs[i].Op,
															   (int)Unit_Price,
															   (int)phDrgs[i].TotalMemberParticipation,
															   (int)SysDate,			// Event Date
															   (int)v_DoctorID,
															   (int)v_DoctorPrDate,	// Request Date
															   (int)v_DoctorPrID,		// Request Num.
															   (int)AS400_Gadget_Action,
															   (int)2					// Retries
															  );
							GerrLogFnameMini ("gadgets_log",
											   GerrId,
											   "2005 SaveSaleRecord: AS400 err = %d.",
											   (int)GadgetError);
						}
						else	// If Nihul Tikrot calls are disabled, disable Meishar calls too.
						{
							GadgetError = -1;
						}

						if (GadgetError != 0)
						{
							v_action = AS400_Gadget_Action;

							// Write to "holding" table!
							ExecSQL (	MAIN_DB, INS_gadget_spool,
										&v_PharmNum,							&v_RecipeIdentifier,
										&v_MemberID,							&v_IDCode,
										&v_DrugCode,							&v_service_code,
										&v_service_number,						&v_service_type,
										&phDrgPtr->Op,							&Unit_Price,

										&phDrgPtr->TotalMemberParticipation,	&SysDate,
										&v_DoctorID,							&v_DoctorPrDate,
										&v_DoctorPrID,							&v_action,
										&GadgetError,							&SysDate,
										&v_Hour,								&IntZero,
										END_OF_ARG_LIST													);

							if (SQLERR_error_test ())
							{
								GerrLogToFileName ("gadgets_log",
												   GerrId,
												   "2005: DB Error %d writing to gadget_spool.",
												   (int)SQLCODE);

								v_ErrorCode = ERR_DATABASE_ERROR;
								break;
							}
						}	// Error notifying AS400 of sale.
					}	// Item sold got its participation from AS400 "Gadget" application.

					else
					{
						// If the "drug" purchased is a "gadget" but we didn't get participation
						// from the AS400, zero out the Link-to-Addition variable so the AS400
						// won't get the wrong idea about where participation came from.
						if (in_gadget_table[i] != 0)
							phDrgs[i].LinkDrugToAddition = 0;
					}


					// Set up Sold Ingredients fields.
					for (j = 0; j < 3; j++)
					{
						if ((ingr_code	[i][j]		<  1)	||
							(ingr_quant	[i][j]		<= 0.0)	||
							(per_quant	[i][j]		<= 0.0)	||
							(phDrgs[i].package_size <  1))
						{
							// Invalid values - set this slot to zero.
							ingr_code			[i][j] = 0;
							ingr_quant_bot		[i][j] = 0.0;
							ingr_quant_bot_std	[i][j] = 0.0;
						}

						else
						{
							if (phDrgs[i].package_size == 1)
							{
								// Syrups and similar drugs.
								ingr_quant_bot[i][j] =		  (double)phDrgs	[i].Op
															* package_volume	[i]
															* ingr_quant		[i][j]
															/ per_quant			[i][j];

								ingr_quant_bot_std[i][j] =	  (double)phDrgs	[i].Op
															* package_volume	[i]
															* ingr_quant_std	[i][j]
															/ per_quant			[i][j];
							}

							else	// Package size > 1.
							{
								// Tablets, ampules, and similar drugs.
								// For these medications, ingredient is always given per unit.
								UnitsSold	=	  (phDrgs[i].Op		* phDrgs[i].package_size)
												+  phDrgs[i].Units;

								ingr_quant_bot		[i][j] = (double)UnitsSold * ingr_quant		[i][j];
								ingr_quant_bot_std	[i][j] = (double)UnitsSold * ingr_quant_std	[i][j];
							}
						}	// Valid quantity values available.
					}	// Loop through ingredients for this drug.

					// Load DB variables with computed values. Note that the Ingredient Code
					// may have been zeroed out, so it needs to be reloaded!
					v_ingr_1_code		= ingr_code			[i][0];
					v_ingr_2_code		= ingr_code			[i][1];
					v_ingr_3_code		= ingr_code			[i][2];
					v_ingr_1_quant		= ingr_quant_bot	[i][0];
					v_ingr_2_quant		= ingr_quant_bot	[i][1];
					v_ingr_3_quant		= ingr_quant_bot	[i][2];
					v_ingr_1_quant_std	= ingr_quant_bot_std[i][0];
					v_ingr_2_quant_std	= ingr_quant_bot_std[i][1];
					v_ingr_3_quant_std	= ingr_quant_bot_std[i][2];


					// Finally, update prescription_drugs. Note that this update has to happen
					// after the "gadget" logic, since the latter can change macabi_price_flg.
					// DonR Feb2023 User Story #232220: Add paid_for to columns UPDATEd.
					ExecSQL (	MAIN_DB, TR2005_UPD_prescription_drugs,
								&phDrgPtr->Op,					&phDrgPtr->Units,
								&DRUG_DELIVERED,				&NOT_REPORTED_TO_AS400,
								&phDrgPtr->CurrentStockOp,		&phDrgPtr->CurrentStockInUnits,
								&phDrgPtr->TotalDrugPrice,		&phDrgPtr->TotalMemberParticipation,
								&PD_discount_pc,				&phDrgPtr->SupplierDrugPrice,
								&v_Date,						&v_Hour,
								&phDrgPtr->MacabiPriceFlag,		&v_ingr_1_code,
								&v_ingr_2_code,					&v_ingr_3_code,
								&v_ingr_1_quant,				&v_ingr_2_quant,
								&v_ingr_3_quant,				&v_ingr_1_quant_std,
								&v_ingr_2_quant_std,			&v_ingr_3_quant_std,
								&PR_Orig_Largo_Code[i],			&PR_Date[i],
								&phDrgPtr->LinkDrugToAddition,	&phDrgPtr->NumRxLinks,
								&paid_for,

								&v_RecipeIdentifier,			&phDrgPtr->LineNo,
								END_OF_ARG_LIST															);

					Conflict_Test (reStart);

					if (SQLERR_error_test ())
					{
						SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR);
						break;
					}
				}	// Loop through drugs.

				// If any SQL errors were detected inside drug loop, break out of dummy loop.
				if (SQLERR_error_test ())
				{
					v_ErrorCode = ERR_DATABASE_ERROR;
					break;
				}


				// Update max_drug_date in members table.
				// DonR 05Sep2006: Don't do this for non-Maccabi OTC purchases.
				if ((max_drug_date > sq_maxdrugdate) && (v_MemberID > 0))
				{
					ExecSQL (	MAIN_DB, UPD_members_max_drug_date,
								&max_drug_date,		&v_MemberID,
								&v_IDCode,			END_OF_ARG_LIST		);

					Conflict_Test( reStart );

					if (SQLERR_error_test ())
					{
						SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR);
						break;
					}
				}


				// DonR 24Jul2013: Unless we hit a new error (like a database failure), we don't want to overwrite
				// a previously-stored error code in the prescriptions table; so use the previous value as our default.
				if (v_ErrorCode != 0)
					prev_pr_errorcode = v_ErrorCode;

				// Update PRESCRIPTIONS table.
				// DonR Feb2023 User Story #232220: Add paid_for to columns UPDATEd.
				ExecSQL (	MAIN_DB, TR2005_UPD_prescriptions,
							&v_InvoiceNum,			&v_UserIdentification,
							&v_PharmRecipeNum,		&v_DoctorIdInputType,
							&v_ClientLocationCode,	&v_DocLocationCode,
							&v_MonthLog,			&DRUG_DELIVERED,
							&NOT_REPORTED_TO_AS400,	&v_CreditYesNo,
							&v_Date,				&v_Hour,
							&prev_pr_errorcode,		&v_ReasonForDiscount,
							&v_ReasonToDispense,	&v_TotalCouponDisc,
							&paid_for,
							&v_RecipeIdentifier,	END_OF_ARG_LIST				);

				// DonR 22May2013: Also mark prescription_msgs and prescription_usage rows as delivered. Since the
				// delivery isn't complete unless we write to the database without any problems, we can piggy-back
				// on the same error handling. (Note that "no rows affected" doesn't throw an error in
				// SQLCODE/SQLERR_error_test.)
				if (SQLCODE == 0)
				{
					ExecSQL (	MAIN_DB, UPD_prescription_msgs_mark_delivered,
								&v_Date,				&v_Hour,				&DRUG_DELIVERED,
								&NOT_REPORTED_TO_AS400,	&v_RecipeIdentifier,	END_OF_ARG_LIST	);
				}

				if (SQLCODE == 0)
				{
					ExecSQL (	MAIN_DB, UPD_prescription_usage_mark_delivered,
								&v_Date,				&v_Hour,				&DRUG_DELIVERED,
								&NOT_REPORTED_TO_AS400,	&v_RecipeIdentifier,	END_OF_ARG_LIST	);
				}

				Conflict_Test (reStart);

				if (SQLERR_error_test ())
				{
					SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR);
					break;
				}

			}
			while (0);	// Dummy loop Should run only once.
			// End of dummy loop to prevent goto.
		}
		else	// DonR 04Aug2010: Some error occurred in previous processing - tag prescriptions row
				// with error code.
		{
			if (v_RecipeIdentifier > 0)
			{
				// Update PRESCRIPTIONS table. No error handling, as this is going to be a non-delivered
				// prescription and this update is informational only.
				ExecSQL (	MAIN_DB, UPD_prescriptions_set_error_code,
							&v_ErrorCode,	&v_RecipeIdentifier,	END_OF_ARG_LIST	);
			}
		}	// Serious error occurred - only DB update is to flag prescriptions row with error code.


		// Commit the transaction.
		if (reStart == MAC_FALS)	// No retry needed.
		{
			if (!SetErrorVar (&v_ErrorCode, v_ErrorCode)) // Transaction OK
			{
				CommitAllWork ();
			}
			else
			{
				// Error, so do rollback.
				RollbackAllWork ();
			}

			if (SQLERR_error_test ())	// Commit (or rollback) failed.
			{
				SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR);
			}
		}
		else
		{
			// Need to retry.
			if ( TransactionRestart () != MAC_OK)
			{
				v_ErrorCode = ERR_DATABASE_ERROR;
				break;
			}

			// DonR 26Jul2011: Suppress log message until the fourth attempt fails.
			if (tries > 2)
				GerrLogReturn (GerrId, "Table is locked for the <%d> time", tries);

			sleep (ACCESS_CONFLICT_SLEEP_TIME);
		}


	}	// End of retries loop.


	if (reStart == MAC_TRUE)
	{
		GerrLogReturn (GerrId, "Locked for <%d> times",SQL_UPDATE_RETRIES);

		SetErrorVar( &v_ErrorCode, ERR_DATABASE_ERROR );
	}


	// Don't send warnings - only fatal errors.
	if (v_ErrorCode != NO_ERROR)
	{
		Sever = GET_ERROR_SEVERITY (v_ErrorCode);

		if (Sever <= FATAL_ERROR_LIMIT)
		{
			v_ErrorCode = NO_ERROR;
		}
	}


	// Create Response Message
	nOut =  sprintf (OBuffer,		 "%0*d",	MSG_ID_LEN,					2006				);
	nOut += sprintf (OBuffer + nOut, "%0*d",	MSG_ERROR_CODE_LEN,			MAC_OK				);
	nOut += sprintf (OBuffer + nOut, "%0*d",	TPharmNum_len,				v_PharmNum			);
	nOut += sprintf (OBuffer + nOut, "%0*d",	TInstituteCode_len,			v_InstituteCode		);
	nOut += sprintf (OBuffer + nOut, "%0*d",	TTerminalNum_len,			v_TerminalNum		);
	nOut += sprintf (OBuffer + nOut, "%0*d",	TErrorCode_len,				v_ErrorCode			);
	nOut += sprintf (OBuffer + nOut, "%0*d",	TMemberIdentification_len,	v_MemberID			);
	nOut += sprintf (OBuffer + nOut, "%0*d",	TIdentificationCode_len,	v_IDCode			);
	nOut += sprintf (OBuffer + nOut, "%0*d",	TRecipeIdentifier_len,		v_RecipeIdentifier	);
	nOut += sprintf (OBuffer + nOut, "%0*d",	TDate_len,					v_Date				);
	nOut += sprintf (OBuffer + nOut, "%0*d",	THour_len,					v_Hour				);


	// Return the size in Bytes of respond message
	*p_outputWritten			= nOut;
	*output_type_flg			= ANSWER_IN_BUFFER;
	ssmd_data_ptr->error_code	= v_ErrorCode;
	ssmd_data_ptr->terminal_num	= v_TerminalNum;

	RESTORE_ISOLATION;

	return RC_SUCCESS;
}
/* End of real-time 2005/6 handler */



/*=======================================================================
||																		||
||			 HandlerToSpool_2005										||
||	Message handler for message 2005:									||
||        Electronic Prescription Drugs Delivery, spooled mode.			||
||																		||
 =======================================================================*/

int HandlerToSpool_2005 (char			*IBuffer,
						 int			TotalInputLen,
						 char			*OBuffer,
						 int			*p_outputWritten,
						 int			*output_type_flg,
						 SSMD_DATA		*ssmd_data_ptr,
						 short int		commfail_flag)

{
	// Local declarations
	ISOLATION_VAR;
	int				nOut;
	int				tries;
	int				retries;
	int				i;
	int				j;
	int				is;
	int				reStart;
	int				ElyCount;
	int				flg;
	int				UnitsSellable;
	int				UnitsSold;
	int				AddNewRows				= 0;
	TErrorCode		err;
	TErrorSevirity	Sever;
	ACTION_TYPE		AS400_Gadget_Action		= enActWrite;


	// Message fields variables
	Msg1005Drugs				phDrgs					[MAX_REC_ELECTRONIC];
	short int					LineNumber				[MAX_REC_ELECTRONIC];
	short int					PR_WhyNonPreferredSold	[MAX_REC_ELECTRONIC];
	short int					PR_NumPerDose			[MAX_REC_ELECTRONIC];
	short int					PR_DosesPerDay			[MAX_REC_ELECTRONIC];
	short int					PR_HowParticDetermined	[MAX_REC_ELECTRONIC];
	short int					v_in_gadget_table		[MAX_REC_ELECTRONIC];
	short int					PRW_Op_Update			[MAX_REC_ELECTRONIC];
	int							PR_Date					[MAX_REC_ELECTRONIC];
	int							PR_Orig_Largo_Code		[MAX_REC_ELECTRONIC];
	int							PrevUnsoldUnits			[MAX_REC_ELECTRONIC];
	int							PrevUnsoldOP			[MAX_REC_ELECTRONIC];
	int							FixedParticipPrice		[MAX_REC_ELECTRONIC];
	short						v_MemberDiscPC			[MAX_REC_ELECTRONIC];
	TDoctorRecipeNum			v_DoctorRecipeNum		[MAX_REC_ELECTRONIC];
	TMemberParticipatingType	v_MemberParticType		[MAX_REC_ELECTRONIC];

	// DonR 13Feb2006: Ingredients-sold arrays.
	short int					ingr_code				[MAX_REC_ELECTRONIC] [3];
	double						ingr_quant				[MAX_REC_ELECTRONIC] [3];
	double						ingr_quant_std			[MAX_REC_ELECTRONIC] [3];
	double						ingr_quant_bot			[MAX_REC_ELECTRONIC] [3];
	double						ingr_quant_bot_std		[MAX_REC_ELECTRONIC] [3];
	double						per_quant				[MAX_REC_ELECTRONIC] [3];
	double						package_volume			[MAX_REC_ELECTRONIC];

	int							PW_ID;
	int							PW_AuthDate;
	int							PRW_PrescDate;
	short int					PRW_OpUpdate;
	short int					PW_status;
	short int					PW_SubstPermitted;
	int							PD_discount_pc;

	int							v_ReasonForDiscount;
	int							v_ClientLocationCode;
	int							v_DocLocationCode;
	short int					v_ReasonToDispense;
	short int					LineNum;
	int							v_TotalCouponDisc;
		
	TFlg						same_pr_count;
	TFlg						report_as400_flag;
	TFlg						delete_flag;

	TDrugListRow				DL;
	PHARMACY_INFO				Phrm_info;
	TPharmNum					v_PharmNum;
	TPriceListNum				v_PriceListCode;
	TInstituteCode				v_InstituteCode;
	TTerminalNum				v_TerminalNum;
	TMemberBelongCode			v_MemberBelongCode;
	TGenericTZ					v_MemberID;
	TIdentificationCode			v_IDCode;
	TMoveCard					v_MoveCard;
	TTypeDoctorIdentification	v_DocIDType;
	TGenericTZ					v_DoctorID;
	TDoctorIdInputType			v_DoctorIdInputType;
	TRecipeSource				v_RecipeSource;
	TInvoiceNum					v_InvoiceNum;
	TGenericTZ					v_UserIdentification;
	TPharmRecipeNum				v_PharmRecipeNum;
	TRecipeIdentifier			v_RecipeIdentifier;
	TRecipeIdentifier			v_RecipeIdentifier1;
	TDeliverConfirmationDate	v_DeliverConfirmationDate;
	TDeliverConfirmationHour	v_DeliverConfirmationHour;
	TMonthLog					v_MonthLog;
	TNumOfDrugLinesRecs			v_NumOfDrugLinesRecs;
	TCreditYesNo				v_CreditYesNo;
	TCreditYesNo				v_MemberCreditFlag;
	short						paid_for	= 1;	// DonR 05Feb2023 User Story #232220 - always TRUE for 2005/5005.

	// DonR 29Jul2007: Add logic to decode temporary cards.
	TGenericTZ			      	v_TempMembIdentification;
	TIdentificationCode			v_TempIdentifCode;
	TDate						v_TempMemValidUntil;

	// DonR 13Feb2006: Ingredients-sold fields.
	double						v_package_volume;
	short int					v_ingr_1_code;
	short int					v_ingr_2_code;
	short int					v_ingr_3_code;
	double						v_ingr_1_quant;
	double						v_ingr_2_quant;
	double						v_ingr_3_quant;
	double						v_ingr_1_quant_std;
	double						v_ingr_2_quant_std;
	double						v_ingr_3_quant_std;
	double						v_per_1_quant;
	double						v_per_2_quant;
	double						v_per_3_quant;

	TNumofSpecialConfirmation		v_SpecialConfNum;
	TSpecialConfirmationNumSource	v_SpecialConfSource;

	// Prescription Lines structure
	Msg1005Drugs				*phDrgPtr;
	Msg1005Drugs				dbDrgs;

	// Gadgets/Gadget_spool tables.
	short int					v_action;
	int							v_service_code;
	int							v_gadget_code;
	short int					v_service_number;
	short int					v_service_type;
	TDrugCode					v_DrugCode;
	TOpDrugPrice				Unit_Price;
	TDoctorRecipeNum			v_DoctorPrID;
	int							v_DoctorPrDate;
	short int					PRW_OriginCode;
	TErrorCode					GadgetError;


	// Return-message variables
	TDate						SysDate;
	TDate						v_Date;
	THour						v_Hour;
	TErrorCode					v_ErrorCode;


	// Miscellaneous DB variables
	TPharmNum					sq_pharmnum;
	TInstituteCode				sq_institutecode;
	TTerminalNum				sq_terminalnum;
	TPriceListNum				sq_pricelistnum;
	TDoctorRecipeNum			sq_doctorprescid;
	TMemberBelongCode			sq_memberbelongcode;
	TGenericTZ					sq_MemberID;
	TIdentificationCode			sq_identificationcode;
	TTypeDoctorIdentification	sq_DocIDType;
	TGenericTZ					sq_doctoridentification;
	TRecipeSource				sq_recipesource;
	TNumOfDrugLinesRecs			sq_numofdruglinesrecs;
	TTotalMemberParticipation	sq_TotalMemberPrice;
	TTotalDrugPrice				sq_TotalDrugPrice;
	TTotalDrugPrice				sq_TotSuppPrice;
	TMemberDiscountPercent		sq_member_discount;
	int							sq_PW_ID;
	int							sq_DrPrescNum;
	TDate						StopUseDate;
	TDate						StopBloodDate;
	TDate						max_drug_date;
	TDate						sq_maxdrugdate;
	int							AuthDate;
	short int					sq_thalf;
	TDate						sq_DateDB;
	TMonthLog					sq_MonthLogDB;
	short						v_MemberSpecPresc;
	TErrorCode					v_Comm_Error;
	TSqlInd						v_CredInd;
	TRecipeIdentifier			v_prw_id;
	short						MyOriginCode	= 2005;


	// Body of function.

	// Initialize variables.
	REMEMBER_ISOLATION;
	SET_ISOLATION_DIRTY;
	PosInBuff		= IBuffer;
	v_ErrorCode		= NO_ERROR;
	SysDate			= GetDate ();
	v_Comm_Error	= commfail_flag;

	memset ((char *)&phDrgs [0],	0, sizeof (phDrgs));
	memset ((char *)&dbDrgs,		0, sizeof (dbDrgs));



	// Read message fields data into variables.
	v_PharmNum					= GetLong	(&PosInBuff, TPharmNum_len					); CHECK_ERROR_T("2005-Spool: PharmNum");
	v_InstituteCode				= GetShort	(&PosInBuff, TInstituteCode_len				); CHECK_ERROR_T("2005-Spool: InstituteCode");
	v_TerminalNum				= GetShort	(&PosInBuff, TTerminalNum_len				); CHECK_ERROR_T("2005-Spool: TerminalNum");
	v_MemberID					= GetLong	(&PosInBuff, TMemberIdentification_len		); CHECK_ERROR_T("2005-Spool: MemberID");
	v_IDCode					= GetShort	(&PosInBuff, TIdentificationCode_len		); CHECK_ERROR_T("2005-Spool: IDCode");
	v_MemberBelongCode			= GetShort	(&PosInBuff, TMemberBelongCode_len			); CHECK_ERROR_T("2005-Spool: MemberBelongCode");	 
	v_MoveCard					= GetShort	(&PosInBuff, TMoveCard_len					); CHECK_ERROR_T("2005-Spool: MoveCard");	 
	v_DocIDType					= GetShort	(&PosInBuff, TTypeDoctorIdentification_len	); CHECK_ERROR_T("2005-Spool: DocIDType");	 
	v_DoctorID					= GetLong	(&PosInBuff, TDoctorIdentification_len		); CHECK_ERROR_T("2005-Spool: DoctorID");	 
	v_DoctorIdInputType			= GetShort	(&PosInBuff, TDoctorIdInputType_len			); CHECK_ERROR_T("2005-Spool: DoctorIdInputType");	 
	v_RecipeSource			  	= GetShort	(&PosInBuff, TRecipeSource_len				); CHECK_ERROR_T("2005-Spool: RecipeSource");	 
	v_UserIdentification		= GetLong	(&PosInBuff, TUserIdentification_len		); CHECK_ERROR_T("2005-Spool: UserIdentification");	 
	v_SpecialConfSource			= GetShort	(&PosInBuff, TSpecConfNumSrc_len			); CHECK_ERROR_T("2005-Spool: SpecialConfSource");
	v_SpecialConfNum			= GetLong	(&PosInBuff, TNumofSpecialConfirmation_len	); CHECK_ERROR_T("2005-Spool: SpecialConfNum");
	v_PharmRecipeNum			= GetLong	(&PosInBuff, TPharmRecipeNum_len			); CHECK_ERROR_T("2005-Spool: PharmRecipeNum");
	v_RecipeIdentifier			= GetLong	(&PosInBuff, TRecipeIdentifier_len			); CHECK_ERROR_T("2005-Spool: RecipeIdentifier");	 
	v_ReasonForDiscount			= GetLong	(&PosInBuff, TElectPR_DiscntReasonLen		); CHECK_ERROR_T("2005-Spool: ReasonForDiscount");
	v_DeliverConfirmationDate	= GetLong	(&PosInBuff, TDeliverConfirmationDate_len	); CHECK_ERROR_T("2005-Spool: DeliverConfirmationDate");
	v_DeliverConfirmationHour	= GetLong	(&PosInBuff, TDeliverConfirmationHour_len	); CHECK_ERROR_T("2005-Spool: DeliverConfirmationHour");	 

	// Note: This will be the Credit Type Used. In some circumstances, it may be
	// different from the Credit Type that was sent to the pharmacy - for example,
	// the member may elect to pay in cash even though s/he has an automated
	// payment ("orat keva") set up. This value will be stored in the new database
	// field credit_type_used.
	v_CreditYesNo				= GetShort	(&PosInBuff, TCreditYesNo_len				); CHECK_ERROR_T("2005-Spool: CreditYesNo");
	v_InvoiceNum			 	= GetLong	(&PosInBuff, TInvoiceNum_len				); CHECK_ERROR_T("2005-Spool: InvoiceNum");	 
	v_ClientLocationCode		= GetLong	(&PosInBuff, TDeviceCode_len				); CHECK_ERROR_T("2005-Spool: ClientDeviceCode");	 
	v_DocLocationCode			= GetLong	(&PosInBuff, TDeviceCode_len				); CHECK_ERROR_T("2005-Spool: DocDeviceCode");	 
	v_MonthLog					= GetShort	(&PosInBuff, TMonthLog_len					); CHECK_ERROR_T("2005-Spool: MonthLog");
	v_ReasonToDispense			= GetShort	(&PosInBuff, TElectPR_ReasonToDispLen		); CHECK_ERROR_T("2005-Spool: ReasonToDispense");
	v_NumOfDrugLinesRecs		= GetShort	(&PosInBuff, TElectPR_LineNumLen			); CHECK_ERROR_T("2005-Spool: NumOfDrugLinesRecs");

	// Get repeating portion of message
	for (i = 0; i < v_NumOfDrugLinesRecs; i++)
	{
		LineNumber			[i]		= GetShort	(&PosInBuff, TElectPR_LineNumLen			); CHECK_ERROR_T("2005-Spool: LineNumber");
		v_DoctorRecipeNum	[i]		= GetLong	(&PosInBuff, TDoctorRecipeNum_len			); CHECK_ERROR_T("2005-Spool: DoctorRecipeNum");	 
		PR_Date				[i]		= GetLong	(&PosInBuff, TDate_len						); CHECK_ERROR_T("2005-Spool: PR_Date");
		PR_Orig_Largo_Code	[i]		= GetLong	(&PosInBuff, TDrugCode_len					); CHECK_ERROR_T("2005-Spool: Orig_Largo_Code");
		phDrgs[i].DrugCode			= GetLong	(&PosInBuff, TDrugCode_len					); CHECK_ERROR_T("2005-Spool: DrugCode");
		PR_WhyNonPreferredSold[i]	= GetShort	(&PosInBuff, TElectPR_SubstPermLen			); CHECK_ERROR_T("2005-Spool: WhyNonPreferredSold");
		PrevUnsoldUnits		[i]		= GetLong	(&PosInBuff, TUnits_len						); CHECK_ERROR_T("2005-Spool: PrevUnsoldUnits");
		PrevUnsoldOP		[i]		= GetLong	(&PosInBuff, TUnits_len						); CHECK_ERROR_T("2005-Spool: PrevUnsoldOP");
		phDrgs[i].Units				= GetLong	(&PosInBuff, TUnits_len						); CHECK_ERROR_T("2005-Spool: Units");
		phDrgs[i].Op				= GetLong	(&PosInBuff, TOp_len						); CHECK_ERROR_T("2005-Spool: Op");
		PR_NumPerDose		[i]		= GetShort	(&PosInBuff, TUnits_len						); CHECK_ERROR_T("2005-Spool: NumPerDose");
		PR_DosesPerDay		[i]		= GetShort	(&PosInBuff, TUnits_len						); CHECK_ERROR_T("2005-Spool: DosesPerDay");
		phDrgs[i].Duration			= GetShort	(&PosInBuff, TDuration_len					); CHECK_ERROR_T("2005-Spool: Duration");
		phDrgs[i].LinkDrugToAddition= GetLong	(&PosInBuff, TLinkDrugToAddition_len		); CHECK_ERROR_T("2005-Spool: LinkDrugToAddition");
		phDrgs[i].OpDrugPrice		= GetLong	(&PosInBuff, TOpDrugPrice_len				); CHECK_ERROR_T("2005-Spool: OpDrugPrice");

		// 21NOV2002 Fee Per Service enh. begin.
		phDrgs[i].SupplierDrugPrice	= GetLong	(&PosInBuff, TSupplierPrice_len				); CHECK_ERROR_T("2005-Spool: SupplierDrugPrice");
		// 21NOV2002 Fee Per Service enh. end.

		v_MemberParticType	[i]		= GetShort	(&PosInBuff, TMemberParticipatingType_len	); CHECK_ERROR_T("2005-Spool: MemberParticType");	 
		FixedParticipPrice	[i]		= GetLong	(&PosInBuff, TOpDrugPrice_len				); CHECK_ERROR_T("2005-Spool: FixedParticipPrice");
		v_MemberDiscPC		[i]		= GetShort	(&PosInBuff, TMemberDiscountPercent_len		); CHECK_ERROR_T("2005-Spool: MemberDiscPC");	 
		phDrgs[i].TotalDrugPrice	= GetLong	(&PosInBuff, TTotalDrugPrice_len			); CHECK_ERROR_T("2005-Spool: TotalDrugPrice");

		phDrgs[i].TotalMemberParticipation
									= GetLong	(&PosInBuff, TTotalMemberParticipation_len	); CHECK_ERROR_T("2005-Spool: TotalMemberParticipation");

		// Normally, this can be ignored - since it's written to the DB by Trn. 2003.
		PR_HowParticDetermined [i]	= GetShort	(&PosInBuff, TElectPR_HowPartDetLen			); CHECK_ERROR_T("2005-Spool: HowParticDetermined");

		phDrgs[i].CurrentStockOp	= GetLong	(&PosInBuff, TElectPR_Stock_len				); CHECK_ERROR_T("2005-Spool: CurrentStockOp");

		phDrgs[i].CurrentStockInUnits
									= GetLong	(&PosInBuff, TElectPR_Stock_len				); CHECK_ERROR_T("2005-Spool: CurrentStockInUnits");


		// DonR 12Jul2005: Avoid DB errors due to ridiculously high OP numbers from pharmacy.
		PRW_Op_Update[i] = ((phDrgs[i].Op <= 32767) && (phDrgs[i].Op >= 0)) ? phDrgs[i].Op : 32767;


		// DonR 16Nov2008: We don't get anything from pharmacy to indicate whether drugs should have their
		// health-basket flag set, so default value of 9 (= unknown).
		//
		// DonR 20Apr2009: Per Iris Shaya, the default value for the health-basket flag is being changed
		// from 9 to 8. This change affects only drug sales for which Transaction 2003 hasn't already
		// taken place - that is, when 2005-spool needs to do an INSERT rather than an UPDATE.
		phDrgs[i].in_health_pack = 8;

	}	// End of drug-line reading loop.


	// Get amount of input not eaten
	ChckInputLen( TotalInputLen - (PosInBuff - IBuffer) );
	CHECK_ERROR_T("2005-Spool: ChckInputLen");

	// Copy values to ssmd_data structure.
	ssmd_data_ptr->pharmacy_num		= v_PharmNum;
	ssmd_data_ptr->member_id		= v_MemberID;
	ssmd_data_ptr->member_id_ext	= v_IDCode;

	// Validate Date and Time.
	ChckDate (v_DeliverConfirmationDate);
	CHECK_ERROR_T("2005-Spool: Check Delivery Date");	 

	ChckTime (v_DeliverConfirmationHour);
	CHECK_ERROR_T("2005-Spool: Check Delivery Time");	 

	// DonR 27Jun2010: The subsidy amount, sent in the first four digits of "Reason for discount",
	// is now being written to a different field: Total Coupon Discount, which is passed to the
	// AS/400 field RK9021P/EAZOZR, replacing the old discount amount EADSCNTP.
	v_TotalCouponDisc = v_ReasonForDiscount / 10;	// The last digit goes to EACRCDE.
	v_TotalCouponDisc *= 100;						// The new field is in agorot, not shekels.
	v_ReasonForDiscount = v_ReasonForDiscount % 10;	// Keep only the last digit - so EADSCNTP will always be 0.

	
	reStart		= MAC_TRUE;


	// SQL Retry Loop.
	for (tries = 0; tries < SQL_UPDATE_RETRIES && reStart == MAC_TRUE; tries++)
	{
		// Initialize response variables.
		v_ErrorCode					= NO_ERROR;
		reStart						= MAC_FALS;
		SysDate						= GetDate ();
		v_Date						= GetDate ();
		v_Hour						= GetTime ();
		max_drug_date				= 0;
		v_MemberSpecPresc			= 0;

		memset ((char *)&dbDrgs, 0, sizeof (dbDrgs));

		// Get prescription id.
		state = GET_PRESCRIPTION_ID (&v_RecipeIdentifier1);

		if (state != NO_ERROR)
		{
			SetErrorVar( &v_ErrorCode, state );

			GerrLogReturn (GerrId,
						   "Can't get PRESCRIPTION_ID for 2005 spool - error %d",
						   state);
		}


		// Dummy loop to avoid GOTO.
		do		// Exiting from loop will send the reply to pharmacy.
		{
			// Load pharmacy data.
			v_ErrorCode = IS_PHARMACY_OPEN (v_PharmNum, v_InstituteCode, &Phrm_info);

			v_PriceListCode = (v_ErrorCode == MAC_OK) ? Phrm_info.price_list_num : 0;


			// Test temporary members.
		    if (v_IDCode == 7)
		    {
				ExecSQL (	MAIN_DB, READ_tempmemb,
							&v_TempIdentifCode,			&v_TempMembIdentification,	&v_TempMemValidUntil,
							&v_MemberID,				END_OF_ARG_LIST										);

				Conflict_Test (reStart);

				if (SQLERR_code_cmp (SQLERR_not_found) == MAC_TRUE)
				{
					// Since this is the spool processor, don't do anything - just
					// continue working.
				}

				else
				if (SQLERR_error_test ())
				{
					v_ErrorCode = ERR_DATABASE_ERROR;
				}

				else	// Use the values from tempmemb only if row was
						// actually read from the database.
				if (v_TempMemValidUntil >= SysDate)
				{
				    v_IDCode	= v_TempIdentifCode;
				    v_MemberID	= v_TempMembIdentification;
				}
			}	// Temporary Member test.

			
			
			// Test Maccabi members.
			if (v_MemberBelongCode == MACABI_INSTITUTE)
			{
				if (v_MemberID == 0)
				{
					v_ErrorCode				= ERR_MEMBER_ID_CODE_WRONG;

					v_MemberCreditFlag		= 0;
					Member.maccabi_code		= 0;
					Member.maccabi_until	= 0;
					v_MemberSpecPresc		= 0;
				}
				else
				{
					ExecSQL (	MAIN_DB, TR2005_5005spool_READ_members,
								&v_MemberCreditFlag,		&Member.maccabi_code,
								&Member.maccabi_until,		&v_MemberSpecPresc,
								&sq_maxdrugdate,			&Member.insurance_type,
								&Member.keren_mac_code,		&Member.mac_magen_code,
								&Member.yahalom_code,		&Member.keren_mac_from,
								&Member.keren_mac_until,	&Member.mac_magen_from,
								&Member.mac_magen_until,	&Member.yahalom_from,
								&Member.yahalom_until,		&Member.died_in_hospital,
								&v_MemberID,				&v_IDCode,
								END_OF_ARG_LIST										);

					Conflict_Test (reStart);

					if (SQLMD_is_null (v_CredInd))
						v_MemberCreditFlag = 0;


					// DonR 20Nov2007: If member is not found, continue - with spooled
					// sales, we can't just throw them away!
					if (SQLERR_code_cmp (SQLERR_not_found) == MAC_TRUE)
					{
						v_MemberCreditFlag		= 0;
						Member.maccabi_code		= 0;
						Member.maccabi_until	= 0;
						v_MemberSpecPresc		= 0;	// DonR 20Nov2007: Added another default.
					}
					else
					{
						// See if there were problems other than deleted-row or not-found.
						if (SQLERR_error_test ())
						{
							v_ErrorCode = ERR_DATABASE_ERROR;
							break;
						}

						// DonR 20Nov2007: Check for member rights only if we actually read
						// a row from the Members table.
						else
						{
							// Check member eligibility.
//							// DonR 25Nov2021 User Story #206812: Get died-in-hospital indicator from the Ishpuz system.
							// DonR 30Nov2022 User Story #408077: Use new macro MEMBER_INELIGIBLE (defined in
							// MsgHndlr.h) to decide if this member is eligible.
//							if ((Member.maccabi_code < 0) || (Member.maccabi_until < SysDate) || (Member.died_in_hospital))
							if (MEMBER_INELIGIBLE)
							{
								// DonR 29Jul2007: If we've failed to read the Member information,
								// don't overwrite the "bad member ID" error.
								if (v_ErrorCode != ERR_MEMBER_ID_CODE_WRONG)
								{
									v_ErrorCode = ERR_MEMBER_NOT_ELEGEBLE;
								}
							}	// Member is not eligible for service.
						}	// Good read of member.
					}	// We didn't get a member-not-found error.
				}	// Member Identification is non-zero.
			}	// Member belongs to Maccabi.


			// DonR 20Nov2007: For spooled sales to non-Maccabi members, provide
			// defaults for the values that would normally come from the Members
			// table.
			else
			{
				v_MemberCreditFlag		= 0;
				Member.maccabi_code		= 0;
				Member.maccabi_until	= 0;
				v_MemberSpecPresc		= 0;
			}


			// Check drugs against the drug_list table, and load in their Largo Type and packaging details.
			for (i = 0; i < v_NumOfDrugLinesRecs; i++)
			{
				// Read drug_list data by largo code.
				// DonR 14Apr2016: Switch to using the read_drug() routine, since that includes more sophisticated
				// data-massaging logic.
				if (read_drug (phDrgs[i].DrugCode,
							   99999999,
							   &Phrm_info,
							   false,	// Deleted drugs are "invisible".
							   &DL,
							   NULL))
				{
					phDrgs[i].LargoType			= DL.largo_type;
					phDrgs[i].package_size		= DL.package_size;
					phDrgs[i].openable_package	= DL.openable_pkg;
					phDrgs[i].THalf				= DL.t_half;
					v_in_gadget_table[i]		= DL.in_gadget_table;
					v_ingr_1_code				= DL.Ingredient[0].code;
					v_ingr_2_code				= DL.Ingredient[1].code;
					v_ingr_3_code				= DL.Ingredient[2].code;
					v_ingr_1_quant				= DL.Ingredient[0].quantity;
					v_ingr_2_quant				= DL.Ingredient[1].quantity;
					v_ingr_3_quant				= DL.Ingredient[2].quantity;
					v_ingr_1_quant_std			= DL.Ingredient[0].quantity_std;
					v_ingr_2_quant_std			= DL.Ingredient[1].quantity_std;
					v_ingr_3_quant_std			= DL.Ingredient[2].quantity_std;
					v_per_1_quant				= DL.Ingredient[0].per_quant;
					v_per_2_quant				= DL.Ingredient[1].per_quant;
					v_per_3_quant				= DL.Ingredient[2].per_quant;
					v_package_volume			= DL.package_volume;
				}
				else
				{
					Conflict_Test (reStart);

					if (SQLERR_code_cmp (SQLERR_not_found) == MAC_TRUE)
					{
						SetErrorVar (&v_ErrorCode, ERR_DRUG_CODE_NOT_FOUND);
						break;
					}

					if (SQLERR_error_test ())
					{
						SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR);
						break;
					}
				}

				// Save generic-ingredient fields as well.
				package_volume	[i]    = v_package_volume;
				ingr_code		[i][0] = v_ingr_1_code;
				ingr_code		[i][1] = v_ingr_2_code;
				ingr_code		[i][2] = v_ingr_3_code;
				ingr_quant		[i][0] = v_ingr_1_quant;
				ingr_quant		[i][1] = v_ingr_2_quant;
				ingr_quant		[i][2] = v_ingr_3_quant;
				ingr_quant_std	[i][0] = v_ingr_1_quant_std;
				ingr_quant_std	[i][1] = v_ingr_2_quant_std;
				ingr_quant_std	[i][2] = v_ingr_3_quant_std;
				per_quant		[i][0] = v_per_1_quant;
				per_quant		[i][1] = v_per_2_quant;
				per_quant		[i][2] = v_per_3_quant;

			}	// Loop on drugs.


			// DonR 30Apr2015: It appears that Yarpa (at least) has a bug of some sort: they sometimes send a Maccabi
			// Prescription ID that is incorrect. Until now, the spool version of Trn. 2005/5005 has not verified what
			// the pharmacy sends us - but this now appears to be a problem. Accordingly, we'll perform one simple
			// check to see if the pharmacy's Maccabi Prescription ID is bogus or not; if it looks like a mistake, we'll
			// force it to zero, so the transaction will be treated as a new sale rather than updating something irrelevant
			// that was already in the database. (Of course, the redunancy check below will still operate!)
			if (v_RecipeIdentifier != 0)
			{
				ExecSQL (	MAIN_DB, TR2005spool_READ_check_that_pharmacy_sent_correct_values,
							&sq_pharmnum,			&sq_MemberID,		&sq_DateDB,
							&v_RecipeIdentifier,	END_OF_ARG_LIST									);

				Conflict_Test (reStart);

				if ((SQLERR_code_cmp (SQLERR_not_found) == MAC_TRUE)	||
					(sq_pharmnum	!= v_PharmNum)						||
					(sq_MemberID	!= v_MemberID)						||
					(sq_DateDB		!= v_DeliverConfirmationDate))
				{
					// The Maccabi Prescription ID sent by the pharmacy was either not found in the database,
					// or else what was found for that ID didn't match the Pharmacy Code, Member ID, and
					// Sale Date in the database - so assume that the Prescription ID the pharmacy sent
					// is erroneous.
					GerrLogMini (GerrId, "2005-spool: data for sent for PrID %d does not match DB.\n     Member %d (%d in DB)\n     Pharmacy %d (%d in DB)\n     Date %d (%d in DB)",
								 v_RecipeIdentifier, v_MemberID, sq_MemberID, v_PharmNum, sq_pharmnum, v_DeliverConfirmationDate, sq_DateDB);
					v_RecipeIdentifier = 0;
				}
				else
				{	// Either a good read with matching values, or else a database error of some sort.
					if (SQLERR_error_test ())
					{
						v_ErrorCode = ERR_DATABASE_ERROR;
						break;
					}
				}
			}	// Pharmacy sent a non-zero Maccabi Prescription ID.


			// Check if same prescription already exists (in delivered form) in the database.
			// If so, mark the new version as deleted.
			// DonR 02Jan2018: As in Transaction 6005, we want to check for duplations even if Member ID is zero.
//			if (v_MemberID == 0)
//			{
//				same_pr_count = 0;
//			}
//			else 
//			{
				SET_ISOLATION_DIRTY;

				// DonR 04Jul2016: Pharmacies sometimes send a bunch of identical spooled transactions as
				// a single batch; to make sure the redundancy check works properly, we want to make sure
				// that we process only one spooled sale at a time for any given pharmacy. the new function
				// GetSpoolLock() ensures that only one instance of sqlserver.exe can be processing a spooled
				// transaction at any given moment.
				// DonR 28Jul2025: Adding Transaction Number / Spool Flag to GetSpoolLock(), for diagnostics
				// only - since I'm adding a call to this function in Transaction 2086 (which is apparently
				// getting a lock on pharmacy_daily_sum and causing deadlocks).
				GetSpoolLock (v_PharmNum, 2005, 1);

				// DonR 29Jun2010: Changed order of WHERE conditions to conform to index structure.
				// DonR 08Feb2011: Change test on delivered_flg, since we're no longer using only 1 and 0 there.
				// DonR 02Jan2018: Changed WHERE clause to be the same as in Trn. 5005 and 6005, except for
				// member_price_code which is not relevant in Trn. 2003/5. The new WHERE clause works even
				// when Member ID is zero.
				// DonR 08Jan2020: Used a standard SQL operation which *does* look at member_price_code;
				// since it's always zero for Transaction 2003/5003 sales, use ShortZero as our WHERE
				// criterion. (We can assume that a sale will be a duplicate only if it comes from the
				// same "family" of transactions.)
				ExecSQL (	MAIN_DB, READ_prescriptions_check_for_duplicate_spooled_sale,
							&same_pr_count,
							&v_PharmNum,			&v_MonthLog,		&v_PharmRecipeNum,
							&v_MemberBelongCode,	&ShortZero,			&DRUG_NOT_DELIVERED,
							END_OF_ARG_LIST														);

				Conflict_Test (reStart);

				if (SQLERR_error_test ())
				{
					SetErrorVar( &v_ErrorCode, ERR_DATABASE_ERROR );
					SET_ISOLATION_COMMITTED;
					break;
				}

				SET_ISOLATION_COMMITTED;
//			}


			// If the delivered prescription is already in the database, add the new version
			// with del_flg set to 2 (pre-deleted) and reported_to_as400 set to 1 (already
			// reported). Otherwise, add with these two fields set to zero - that is,
			// non-deleted and not yet reported to AS400.
			delete_flag			= (same_pr_count > 0) ? ROW_SYS_DELETED		: ROW_NOT_DELETED;

			report_as400_flag	= (same_pr_count > 0) ? REPORTED_TO_AS400	: NOT_REPORTED_TO_AS400;


			// Use the new Recipe ID (that is, insert new rows) if one of two conditions is true:
			// 1) Delivered prescription is already in the database, and the new version is being
			//    added for completeness (or whatever the reason is) only;
			// 2) The pharmacy sent the 2005 message with Recipe ID of zero - meaning that the
			//    2003/2004 transaction sequence was never completed, and so we need to add
			//    instead of updating.
			// The third possibility is that both conditions are false: that is, the pharmacy
			// reported a Recipe ID, but that recipe is currently marked as non-delivered. In this
			// case we'll do an update, just like a regular 2005 transaction.
			if ((delete_flag == ROW_SYS_DELETED) || (v_RecipeIdentifier == 0))
			{
				v_RecipeIdentifier = v_RecipeIdentifier1;
				AddNewRows = 1;	// DonR 03May2004 - so we remember that we've got to add new rows.
			}

			// End of dummy loop to prevent goto.
		}
		while (0);	// Loop should run only once.



		// Update tables.
		//
		// And now...
		// Yet another dummy loop to avoid goto!
		// Reply is sent to pharmacy when we exit from loop.
		if (reStart == MAC_FALS)	// No major database problem so far.
		{
			do
			{
				// Update Pharmacy Daily Sum ONLY IF we don't already have the delivered prescription
				// in the database.
				if (delete_flag != ROW_SYS_DELETED)
				{
					// Update PHARMACY_DAILY_SUM table.
					// DonR 15Apr2007: Pharmacy_daily_sum is written based on sale date, NOT
					// on system date!
					//
					// DonR 05Aug2009: Don't add private-pharmacy non-prescription sales to pharmacy_daily_sum.
					if ((MACCABI_PHARMACY) || (v_RecipeSource != RECIP_SRC_NO_PRESC))
					{
						// Calculate prescription total
						sq_TotalMemberPrice	= 0;
						sq_TotalDrugPrice	= 0;
						sq_TotSuppPrice		= 0;

						for (i = 0; i < v_NumOfDrugLinesRecs; i++)
						{
							sq_TotalMemberPrice	+= phDrgs[i].TotalMemberParticipation;
							sq_TotalDrugPrice	+= phDrgs[i].TotalDrugPrice;

							sq_TotSuppPrice		+= phDrgs[i].SupplierDrugPrice * phDrgs[i].Op;

							if (phDrgs[i].Units > 0)
							{
								sq_TotSuppPrice	+= (int)(  (float)phDrgs[i].SupplierDrugPrice
														 * (float)phDrgs[i].Units
														 / (float)phDrgs[i].package_size);
							}
						}

						// DonR 21Mar2011: For better performance, try updating rather than inserting first.
						// The vast majority of the time, we'll be updating an existing row - so why waste
						// time trying to insert, failing, and only then updating?
						ExecSQL (	MAIN_DB, TR2005_UPD_pharmacy_daily_sum,
									&sq_TotalDrugPrice,			&sq_TotalMemberPrice,	&sq_TotSuppPrice,
									&v_NumOfDrugLinesRecs,
									&v_PharmNum,				&v_InstituteCode,		&v_MonthLog,
									&v_DeliverConfirmationDate,	&v_TerminalNum,			END_OF_ARG_LIST		);

						// If there was nothing to update, try inserting a new row.
						// DonR 01Jan2020: The original version of this SQL puts sq_TotalMemberPrice
						// into net_fail_mem_part - but I think that's a bug, since (A) all the other
						// net_fail columns are set to zero, and (B) all the other transactions that
						// perform this INSERT put a zero there.
						if (SQLERR_code_cmp (SQLERR_no_rows_affected) == MAC_TRUE)
						{
							ExecSQL (	MAIN_DB, INS_pharmacy_daily_sum_19_columns,
										&v_PharmNum,					&v_InstituteCode,		&v_MonthLog,
										&v_DeliverConfirmationDate,		&v_TerminalNum,			&sq_TotalDrugPrice,
										&IntOne,						&v_NumOfDrugLinesRecs,	&sq_TotalMemberPrice,
										&sq_TotSuppPrice,				&IntZero,				&IntZero,

										&IntZero,						&IntZero,				&IntZero,
										&IntZero,						&IntZero,				&IntZero,
										&IntZero,						END_OF_ARG_LIST										);
						}

						Conflict_Test (reStart);

						if (SQLERR_error_test ())
						{
							SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR);
							break;
						}
					}	// DonR 05Aug2009: Insert/update to pharmacy_daily_sum is now conditional.
				}	// We are adding/updating the delivery "for real".


				// Write to DOCTOR_PRESC & PRESCRIPTION_DRUGS tables.
				for (i = 0; i < v_NumOfDrugLinesRecs; i++)
				{
					PW_ID				= 0;
					PW_AuthDate			= 0;
					phDrgPtr			= &phDrgs			[i];
					PD_discount_pc		= v_MemberDiscPC	[i];
					PRW_OpUpdate		= PRW_Op_Update		[i];
					LineNum				= i + 1;

					// DonR 14Jan2009: Change 99 ("kacha") to just 9, since the "tens" digit is now used
					// for additional information.
					PW_SubstPermitted	= PR_WhyNonPreferredSold[i] % 10;	// 10May2007: Per Iris, don't translate this.

					StopUseDate   = AddDays (v_DeliverConfirmationDate, phDrgPtr->Duration);
					StopBloodDate = AddDays (StopUseDate, phDrgPtr->THalf);

					if( StopBloodDate > max_drug_date )
					{
						max_drug_date = StopBloodDate;
					}
					

					// Update Prescriptions Written ONLY IF we don't already have the delivered
					// prescription in the database.
					// DonR 05Sep2006: Update Prescriptions Written only for "real" members.
					// DonR 28Apr2015: Update doctor_presc only if Prescription Source == Maccabi Doctor,
					// since other types of prescription won't be in our database anyway.
					if ((delete_flag != ROW_SYS_DELETED) && (v_MemberID > 0) && (v_RecipeSource == RECIP_SRC_MACABI_DOCTOR))
					{
						update_doctor_presc	(	0,		// Normal mode - NOT processing late-arriving doctor prescriptions.
												1,		// Only sales in 2005.
												v_MemberID,					v_IDCode,
												v_RecipeIdentifier,			LineNum,
												v_DoctorID,					v_DocIDType,					v_DoctorRecipeNum[i],
												PR_Orig_Largo_Code[i],		phDrgPtr->DrugCode,				NULL,	// No TDrugListRow structure.
												PR_Date[i],					v_Date,							v_Hour,
												PRW_OpUpdate,				phDrgPtr->Units,
												PrevUnsoldOP[i],			PrevUnsoldUnits[i],
												0 /*phDrgPtr->use_instr_template*/ ,			// DonR 01Aug2024 Bug #334612: Get "ratzif" from pharmacy instead of from doctor Rx.
												0,		// Trn. 2005 doesn't handle deletions, so no DelPrescID.
												1,							NULL,	// Old-style single-prescription mode.
												&PR_Orig_Largo_Code[i],		&PR_Date[i],
												&phDrgs[i].NumRxLinks,
												NULL,	NULL,	NULL,	NULL		// Doctor visit & Rx arrival timestamps - relevant only for spool processing.
											);
					}	// Need to update doctor_presc.


					// Set up Sold Ingredients fields.
					for (j = 0; j < 3; j++)
					{
						if ((ingr_code	[i][j]		<  1)	||
							(ingr_quant	[i][j]		<= 0.0)	||
							(per_quant	[i][j]		<= 0.0)	||
							(phDrgs[i].package_size <  1))
						{
							// Invalid values - set this slot to zero.
							ingr_code			[i][j] = 0;
							ingr_quant_bot		[i][j] = 0.0;
							ingr_quant_bot_std	[i][j] = 0.0;
						}

						else
						{
							if (phDrgs[i].package_size == 1)
							{
								// Syrups and similar drugs.
								ingr_quant_bot[i][j] =	  (double)phDrgs[i].Op
														* package_volume[i]
														* ingr_quant	[i][j]
														/ per_quant		[i][j];

								ingr_quant_bot_std[i][j] =	  (double)phDrgs[i].Op
															* package_volume[i]
															* ingr_quant_std[i][j]
															/ per_quant		[i][j];
							}

							else	// Package size > 1.
							{
								// Tablets, ampules, and similar drugs.
								// For these medications, ingredient is always given per unit.
								UnitsSold	=	  (phDrgs[i].Op		* phDrgs[i].package_size)
												+  phDrgs[i].Units;

								ingr_quant_bot		[i][j] = (double)UnitsSold * ingr_quant		[i][j];
								ingr_quant_bot_std	[i][j] = (double)UnitsSold * ingr_quant_std	[i][j];
							}
						}	// Valid quantity values available.
					}	// Loop through ingredients for this drug.

					// Load DB variables with computed values. Note that the Ingredient Code
					// may have been zeroed out, so it needs to be reloaded!
					v_ingr_1_code		= ingr_code				[i][0];
					v_ingr_2_code		= ingr_code				[i][1];
					v_ingr_3_code		= ingr_code				[i][2];
					v_ingr_1_quant		= ingr_quant_bot		[i][0];
					v_ingr_2_quant		= ingr_quant_bot		[i][1];
					v_ingr_3_quant		= ingr_quant_bot		[i][2];
					v_ingr_1_quant_std	= ingr_quant_bot_std	[i][0];
					v_ingr_2_quant_std	= ingr_quant_bot_std	[i][1];
					v_ingr_3_quant_std	= ingr_quant_bot_std	[i][2];


					// Insert or update prescription_drugs row.
					if (AddNewRows)
					{
						// DonR 22Nov2005: If the "drug" purchased is a "gadget" but we didn't
						// get participation from the AS400, zero out the Link-to-Addition
						// variable so the AS400 won't get the wrong idea about where
						// participation came from.
						if (((PR_HowParticDetermined [i] % 10)	!= FROM_GADGET_APP)	&&
							(v_in_gadget_table[i]				!= 0))
						{
							phDrgs[i].LinkDrugToAddition = 0;
						}

						// Insert new prescription_drugs row.
						// DonR Feb2023 User Story #232220: Add paid_for to columns INSERTed.
						ExecSQL (	MAIN_DB, TR2005spool_INS_prescription_drugs,
									&v_RecipeIdentifier,				&LineNum,
									&v_MemberID,						&v_IDCode,
									&phDrgPtr->DrugCode,				&report_as400_flag,
									&phDrgPtr->Op,						&phDrgPtr->Units,
									&phDrgPtr->Duration,				&StopUseDate,

									&v_DeliverConfirmationDate,			&v_DeliverConfirmationHour,
									&delete_flag,			 			&DRUG_DELIVERED,
									&FixedParticipPrice	[i],			&phDrgPtr->Discount,
									&phDrgPtr->LinkDrugToAddition,		&phDrgPtr->OpDrugPrice,
									&v_ErrorCode,						&v_MemberParticType [i],

									&v_MemberParticType	[i],			&v_RecipeSource,
									&ShortZero,							&v_PharmNum,
									&v_InstituteCode,					&phDrgPtr->SupplierDrugPrice,
									&StopBloodDate,						&phDrgPtr->TotalDrugPrice,
									&phDrgPtr->CurrentStockOp,			&phDrgPtr->CurrentStockInUnits,

									&phDrgPtr->TotalMemberParticipation,&v_Comm_Error,
									&phDrgPtr->THalf,					&v_DoctorID,
									&v_DocIDType,						&phDrgPtr->in_health_pack,
									&v_SpecialConfNum,					&v_SpecialConfSource,
									&v_DoctorRecipeNum	[i],			&PR_Date [i],

									&PR_Orig_Largo_Code	[i],			&PW_SubstPermitted,
									&PR_NumPerDose		[i],			&PR_DosesPerDay			[i],
									&v_MemberDiscPC		[i],			&PR_HowParticDetermined	[i],
									&PW_ID,								&v_ingr_1_code,
									&v_ingr_2_code,						&v_ingr_3_code,

									&v_ingr_1_quant,					&v_ingr_2_quant,
									&v_ingr_3_quant,					&v_ingr_1_quant_std,
									&v_ingr_2_quant_std,				&v_ingr_3_quant_std,
									&ShortZero,							&v_CreditYesNo,

									// DonR 25Jun2013: New "Bakarat Kamutit" fields are blank for sales
									// added through 2005/5005 spool. (But dr_visit_date is not!)
									&ShortZero,							&ShortZero,
									&IntZero,							&IntZero,
									&PW_AuthDate,						&v_DocLocationCode,
									&phDrgPtr->NumRxLinks,				&paid_for,
									END_OF_ARG_LIST															);

						Conflict_Test (reStart);

						if (SQLERR_error_test ())
						{
							SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR);
							break;
						}
					}	// Need to insert.

					else
					{
						// DonR 14Jun2004: If "drug" being purchased is a device in the "gadgets"
						// table, retrieve the appropriate information and notify the AS400 of
						// the sale.
						if ((PR_HowParticDetermined [i] % 10) == FROM_GADGET_APP)
						{
							v_gadget_code	= phDrgs[i].LinkDrugToAddition;
							v_DoctorPrID	= v_DoctorRecipeNum	[i];	// Unless these are bogus values 
							v_DoctorPrDate	= PR_Date			[i];	// inserted by pharmacy in Trn. 2003.
							v_DrugCode		= phDrgs[i].DrugCode;
							Unit_Price		= phDrgs[i].OpDrugPrice;	// Unless Maccabi Price is relevant.

							ExecSQL (	MAIN_DB, READ_Gadgets_for_sale_completion,
										&v_service_code,	&v_service_number,	&v_service_type,
										&v_DrugCode,		&v_gadget_code,		&v_gadget_code,
										END_OF_ARG_LIST												);

							Conflict_Test (reStart);

							if (SQLERR_error_test ())
							{
								SetErrorVar( &v_ErrorCode, ERR_DATABASE_ERROR );
								break;
							}

							// If we get here, the gadget entry really does exist.
							// DonR 19Aug2025 User Story #442308: If Nihul Tikrot calls are disabled, disable Meishar calls as well.
							if (TikrotRPC_Enabled)
							{
								GadgetError = as400SaveSaleRecord ((int)v_PharmNum,
																   (int)v_RecipeIdentifier,
																   (int)v_MemberID,
																   (int)v_IDCode,
																   (int)v_service_code,
																   (int)v_service_number,
																   (int)v_service_type,
																   (int)v_DrugCode,
																   (int)phDrgs[i].Op,
																   (int)Unit_Price,
																   (int)phDrgs[i].TotalMemberParticipation,
																   (int)v_DeliverConfirmationDate,	// Event Date
																   (int)v_DoctorID,
																   (int)v_DoctorPrDate,	// Request Date
																   (int)v_DoctorPrID,		// Request Num.
																   (int)AS400_Gadget_Action,
																   (int)2					// Retries
																  );
								GerrLogFnameMini ("gadgets_log",
												  GerrId,
												  "2005 Spool: AS400 err = %d.",
												  (int)GadgetError);
							}
							else	// If Nihul Tikrot calls are disabled, disable Meishar calls too.
							{
								GadgetError = -1;
							}

							if (GadgetError != 0)
							{
								v_action = AS400_Gadget_Action;

								// Write to "holding" table!
								ExecSQL (	MAIN_DB, INS_gadget_spool,
											&v_PharmNum,							&v_RecipeIdentifier,
											&v_MemberID,							&v_IDCode,
											&v_DrugCode,							&v_service_code,
											&v_service_number,						&v_service_type,
											&phDrgPtr->Op,							&Unit_Price,

											&phDrgPtr->TotalMemberParticipation,	&v_DeliverConfirmationDate,
											&v_DoctorID,							&v_DoctorPrDate,
											&v_DoctorPrID,							&v_action,
											&GadgetError,							&SysDate,
											&v_Hour,								&IntZero,
											END_OF_ARG_LIST															);

								if (SQLERR_error_test ())
								{
									GerrLogToFileName ("gadgets_log",
													   GerrId,
													   "2005 Spool: DB Error %d writing to gadget_spool.",
													   (int)SQLCODE);

									v_ErrorCode = ERR_DATABASE_ERROR;
									break;
								}
							}	// Error notifying AS400 of sale.
						}	// Item sold got its participation from AS400 "Gadget" application.

						else
						{
							// DonR 22Nov2005: If the "drug" purchased is a "gadget" but we didn't
							// get participation from the AS400, zero out the Link-to-Addition
							// variable so the AS400 won't get the wrong idea about where participation
							// came from.
							if (v_in_gadget_table[i] != 0)
								phDrgs[i].LinkDrugToAddition = 0;
						}


						// Update prescription_drugs row.
						// DonR 20Aug2008: Need to read Maccabi Price Flag, since we're now updating it below.
						ExecSQL (	MAIN_DB, TR2005_5005spool_READ_PD_macabi_price_flg,
									&LineNum,				&phDrgPtr->MacabiPriceFlag,
									&v_RecipeIdentifier,	&phDrgPtr->DrugCode,
									END_OF_ARG_LIST											);

						if (SQLCODE == 0)
						{
							// DonR Feb2023 User Story #232220: Add paid_for to columns UPDATEd.
							ExecSQL (	MAIN_DB, TR2005spool_UPD_prescription_drugs,
										&phDrgPtr->Op,							&phDrgPtr->Units,
										&DRUG_DELIVERED,						&NOT_REPORTED_TO_AS400,
										&v_Comm_Error,							&phDrgPtr->CurrentStockOp,
										&phDrgPtr->CurrentStockInUnits,			&phDrgPtr->TotalDrugPrice,
										&phDrgPtr->TotalMemberParticipation,	&v_MemberDiscPC [i],
										&v_SpecialConfNum,						&v_SpecialConfSource,
										&v_ingr_1_code,							&v_ingr_2_code,
										&v_ingr_3_code,							&v_ingr_1_quant,
										&v_ingr_2_quant,						&v_ingr_3_quant,
										&v_ingr_1_quant_std,					&v_ingr_2_quant_std,
										&v_ingr_3_quant_std,					&v_DeliverConfirmationDate,
										&v_DeliverConfirmationHour,				&phDrgPtr->MacabiPriceFlag,
										&PR_Orig_Largo_Code[i],					&PR_Date[i],
										&phDrgPtr->LinkDrugToAddition,			&phDrgPtr->NumRxLinks,
										&paid_for,

										&v_RecipeIdentifier,					&LineNum,
										END_OF_ARG_LIST															);
						}

						Conflict_Test (reStart);

						if (SQLERR_error_test ())
						{
							SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR);
							break;
						}

					}	// Update prescription_drugs row.
				}	// Loop through drugs.


				// If any SQL errors were detected inside drug loop, break out of dummy loop.
				if (SQLERR_error_test ())
				{
					v_ErrorCode = ERR_DATABASE_ERROR;
					break;
				}


				// Update max_drug_date in members table.
				// DonR 05Sep2006: Update only for "real" members.
				// DonR 20Nov2007: ...And not only "real", but also Maccabi - others aren't
				// present in the Members table.
				if ((delete_flag		!= ROW_SYS_DELETED)		&&
					(max_drug_date		>  sq_maxdrugdate)		&&
					(v_MemberID			>  0)					&&
					(v_MemberBelongCode	== MACABI_INSTITUTE))
				{
					ExecSQL (	MAIN_DB, UPD_members_max_drug_date,
								&max_drug_date,		&v_MemberID,
								&v_IDCode,			END_OF_ARG_LIST		);

					if (SQLERR_error_test ())
					{
						SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR);
						break;
					}
				}


				// Insert or update prescriptions row.
				if (AddNewRows)
				{
					// DonR Feb2023 User Story #232220: Add paid_for to columns INSERTed.
					ExecSQL (	MAIN_DB, TR2005spool_INS_prescriptions,
								&v_PharmNum,				&v_InstituteCode,
								&v_PriceListCode,			&v_MemberID,
								&v_IDCode,					&v_MemberBelongCode,
								&v_DoctorID,				&v_DocIDType,
								&v_RecipeSource,			&v_RecipeIdentifier,

								&v_SpecialConfNum,			&v_SpecialConfSource,
								&v_DeliverConfirmationDate,	&v_DeliverConfirmationHour,
								&v_ErrorCode,				&v_NumOfDrugLinesRecs,
								&v_CreditYesNo,				&v_MoveCard,
								&ShortZero,					&v_TerminalNum,

								&v_InvoiceNum,				&v_UserIdentification,
								&v_PharmRecipeNum,			&Member.maccabi_code,
								&IntZero,					&report_as400_flag,
								&v_Comm_Error,				&ShortZero,
								&v_DoctorIdInputType,		&v_ClientLocationCode,

								&v_DocLocationCode,			&v_MonthLog,
								&v_PriceListCode,			&IntZero,	// next_presc_start
								&IntZero,					&ShortZero,	// next_presc_stop & waiting_msg_flg
								&DRUG_DELIVERED,			&delete_flag,
								&v_ReasonForDiscount,		&v_ReasonToDispense,

								&v_CreditYesNo,				&MyOriginCode,
								&v_TotalCouponDisc,			&ShortZero,	// action_type
								&Member.insurance_type,		&paid_for,
								END_OF_ARG_LIST																	);

					Conflict_Test (reStart);

					if (SQLERR_error_test ())
					{
						SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR);
						break;
					}
				}

				else
				{
					// Update PRESCRIPTIONS table.
					// DonR 30Oct2006: Bug fix! Updates to prescription_drugs were setting
					// the Reported-to-AS400 flag to zero, but the update to prescriptions
					// was not. I added the additional field update below.
					// DonR Feb2023 User Story #232220: Add paid_for to columns UPDATEd.
					ExecSQL (	MAIN_DB, TR2005spool_UPD_prescriptions,
								&v_InvoiceNum,				&v_UserIdentification,
								&v_PharmRecipeNum,			&v_DoctorIdInputType,
								&v_ClientLocationCode,		&v_DocLocationCode,
								&v_MonthLog,				&DRUG_DELIVERED,
								&v_CreditYesNo,				&v_DeliverConfirmationDate,
								&v_DeliverConfirmationHour,	&v_ErrorCode,
								&v_ReasonForDiscount,		&v_ReasonToDispense,
								&v_Comm_Error,				&NOT_REPORTED_TO_AS400,
								&v_TotalCouponDisc,			&paid_for,

								&v_RecipeIdentifier,		END_OF_ARG_LIST					);

					// DonR 22May2013: Also mark prescription_msgs and prescription_usage rows as delivered. Since the
					// delivery isn't complete unless we write to the database without any problems, we can piggy-back
					// on the same error handling. (Note that "no rows affected" doesn't throw an error in
					// SQLCODE/SQLERR_error_test.)
					if (SQLCODE == 0)
					{
						ExecSQL (	MAIN_DB, UPD_prescription_msgs_mark_delivered,
									&v_DeliverConfirmationDate,	&v_DeliverConfirmationHour,	&DRUG_DELIVERED,
									&NOT_REPORTED_TO_AS400,		&v_RecipeIdentifier,		END_OF_ARG_LIST	);
					}

					if (SQLCODE == 0)
					{
						ExecSQL (	MAIN_DB, UPD_prescription_usage_mark_delivered,
									&v_DeliverConfirmationDate,	&v_DeliverConfirmationHour,	&DRUG_DELIVERED,
									&NOT_REPORTED_TO_AS400,		&v_RecipeIdentifier,		END_OF_ARG_LIST	);
					}

					Conflict_Test (reStart);

					if (SQLERR_error_test ())
					{
						SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR);
						break;
					}
				}	// Update prescriptions row.

			}
			while (0);	// Dummy loop Should run only once.
			// End of dummy loop to prevent goto.

		}	// ReStart is FALSE.


		// DonR 27Aug2025: Clear the Pharmacy Spool Lock we set at the beginning of the transaction.
		ClearSpoolLock (v_PharmNum);

		// Commit the transaction.
		if (reStart == MAC_FALS)	// No retry needed.
		{
			if (!SetErrorVar (&v_ErrorCode, v_ErrorCode)) // Transaction OK
			{
				CommitAllWork ();
			}
			else
			{
				// Error, so do rollback.
				RollbackAllWork ();
			}

			if (SQLERR_error_test ())	// Commit (or rollback) failed.
			{
				SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR);
			}
		}
		else
		{
			// Need to retry.
			if ( TransactionRestart () != MAC_OK)
			{
				v_ErrorCode = ERR_DATABASE_ERROR;
				break;
			}

			// DonR 26Jul2011: Suppress log message until the fourth attempt fails.
			if (tries > 2)
				GerrLogReturn (GerrId, "Table is locked for the <%d> time", tries);

			sleep (ACCESS_CONFLICT_SLEEP_TIME);
		}


	}	// End of retries loop.


	if( reStart == MAC_TRUE )
	{
		GerrLogReturn (GerrId, "Locked for <%d> times",SQL_UPDATE_RETRIES);

		SetErrorVar( &v_ErrorCode, ERR_DATABASE_ERROR );
	}


	// Don't send warnings - only fatal errors.
	if (v_ErrorCode != NO_ERROR)
	{
		Sever = GET_ERROR_SEVERITY (v_ErrorCode);

		if (Sever <= FATAL_ERROR_LIMIT)
		{
			v_ErrorCode = NO_ERROR;
		}
	}


	// Create Response Message
	nOut =  sprintf (OBuffer,		 "%0*d",	MSG_ID_LEN,					2006				);
	nOut += sprintf (OBuffer + nOut, "%0*d",	MSG_ERROR_CODE_LEN,			MAC_OK				);
	nOut += sprintf (OBuffer + nOut, "%0*d",	TPharmNum_len,				v_PharmNum			);
	nOut += sprintf (OBuffer + nOut, "%0*d",	TInstituteCode_len,			v_InstituteCode		);
	nOut += sprintf (OBuffer + nOut, "%0*d",	TTerminalNum_len,			v_TerminalNum		);
	nOut += sprintf (OBuffer + nOut, "%0*d",	TErrorCode_len,				v_ErrorCode			);
	nOut += sprintf (OBuffer + nOut, "%0*d",	TMemberIdentification_len,	v_MemberID			);
	nOut += sprintf (OBuffer + nOut, "%0*d",	TIdentificationCode_len,	v_IDCode			);
	nOut += sprintf (OBuffer + nOut, "%0*d",	TRecipeIdentifier_len,		v_RecipeIdentifier	);
	nOut += sprintf (OBuffer + nOut, "%0*d",	TDate_len,					v_Date				);
	nOut += sprintf (OBuffer + nOut, "%0*d",	THour_len,					v_Hour				);


	// Return the size in Bytes of respond message
	*p_outputWritten			= nOut;
	*output_type_flg			= ANSWER_IN_BUFFER;
	ssmd_data_ptr->error_code	= v_ErrorCode;
	ssmd_data_ptr->terminal_num	= v_TerminalNum;

	RESTORE_ISOLATION;

	return RC_SUCCESS;
}
/* End of spooled-mode 2005/6 handler */



/*=======================================================================
||																		||
||			 HandlerToMsg_5003											||
||	Message handler for message 5003:									||
||        Drugs Sale Request - "Nihul Tikrot" version					||
||																		||
 =======================================================================*/

int HandlerToMsg_5003 (char			*IBuffer,
					   int			TotalInputLen,
					   char			*OBuffer,
					   int			*p_outputWritten,
					   int			*output_type_flg,
					   SSMD_DATA	*ssmd_data_ptr)
{
	// Local declarations.
	int					TrnID = 5003;
	int					tries;
	int					retries;
	int					reStart;
	int					ErrOverflow;
	int					i;
	int					j;
	int					ingred;
	int					is;
	int					flg	= 0;
	int					UnitsSold;
	int					ErrorCodeToAssign		= 0;
	int					MaxFutureDate;			// DonR 11Sep2012.
	int					HoldLargo;
	int					HoldSpecialistDrug;
	int					HoldParentGroupCode;
	int					PtnOpPrice;
	int					PreparationLinkLargo;	// DonR 07Dec2010 for new "preparation" consistency check.
	int					TestLinkLargo;			// DonR 07Dec2010 for new "preparation" consistency check.
	int					FullPriceTotal	= 0;	// DonR 12Mar2012
	short				HoldRuleStatus;
	short				TypeCheckSpecPr;
	short				v_Message;
	short				DispenseAsWritten;
	short				MajorDrugIndex [MAX_REC_ELECTRONIC];
	short				err;
	short				FunctionError;
	short				percent1;
	short				percent2;
	short				percents;
	short				DeletionValid			= 1;
	short				DrugPriceMemHospTrigger	= 0;
	short				Agorot;					// DonR 16Dec2013;
	short				Permit100PercentSale;
	short				found_partially_sold_rx	= 0;
	short				rx_already_checked		= 0;
	short				DefaultSourceReject		= 0;	// DonR 27Dec2017 HOT FIX
	double				PtnWorkVar;
	double				OverseasWorkVar;
	char				filler [256];

	// "Nihul Tikrot" remote procedure call buffers and related fields.
	char				DrugTikraType		[MAX_REC_ELECTRONIC];
	char				DrugCoupon			[MAX_REC_ELECTRONIC];
	int					DrugRefundOffset	[MAX_REC_ELECTRONIC];
	char				TikrotHeader		[RPC_INP_HEADER_LEN];
	char				TikrotCurrentSale	[RPC_INP_CURRSALE_LEN];
	char				TikrotPriorSales	[RPC_INP_PRIORSALES_LEN];
	char				TikraType			[MAX_TIKRA_TIKROT_OUT];
	char				TikraPeriodDesc		[MAX_TIKRA_TIKROT_OUT] [7];
	short				TikraBasket			[MAX_TIKRA_TIKROT_OUT];
	short				TikraInsurance		[MAX_TIKRA_TIKROT_OUT];
	int					TikraAggPrevPtn		[MAX_TIKRA_TIKROT_OUT];
	int					TikraAggPrevWaived	[MAX_TIKRA_TIKROT_OUT];
	int					TikraCurrPtn		[MAX_TIKRA_TIKROT_OUT];
	int					TikraCurrWaived		[MAX_TIKRA_TIKROT_OUT];
	int					TikraCurrLevel		[MAX_TIKRA_TIKROT_OUT];
	short				TikraCouponCode		[MAX_TIKRA_COUPONS_OUT];
	int					TikraCouponAmt		[MAX_TIKRA_COUPONS_OUT];
	int					FamilyMemberTZ		[MAX_FAMILY_SIZE];
	short				FamilyMemberTZCode	[MAX_FAMILY_SIZE];
	short				FamilySize;
	short				FamilySalePrID_count;
	char				*HeaderRtn;
	char				*TikrotRtn;
	char				*CurrentSaleRtn;
	char				*CouponsRtn;
	short				NumTikrotLines;
	short				NumSaleLines;
	short				NumCouponLines;
	Tmember_price_row	PriceRow;
	char				*WritePtr;

	ISOLATION_VAR;

	// Variables for retrieving error count and array.
	int					ErrorCount;
	TErrorArray			*ErrorArray;

	// Package-opening variables.
	float				OpenPkgPercent = OPEN_PACKAGE_BELOW;

	// Return variables for AS400 Gadget participation check routine.
	int					i_dummy;
	int					l_dummy;
	int					UnitPrice;
	int					QuantityPermitted;
	int					RequestNum;
	int					PtnPrice;
	int					Insurance;
	int					MeisharInfoCode;


	// SQL variables.

	// Arrays to hold drugs prescribed data.
	TPresDrugs		SPres					[MAX_REC_ELECTRONIC];
	TDocRx			DocRx					[MAX_REC_ELECTRONIC];
	short			PR_WhyNonPreferredSold	[MAX_REC_ELECTRONIC];
	short			PR_NumPerDose			[MAX_REC_ELECTRONIC];
	short			PR_DosesPerDay			[MAX_REC_ELECTRONIC];
	short			PR_LineNumber			[MAX_REC_ELECTRONIC];
	short			PR_Status				[MAX_REC_ELECTRONIC];
	short			PR_ExtendAS400IshurDays	[MAX_REC_ELECTRONIC];
	short			preferred_mbr_prc_code	[MAX_REC_ELECTRONIC];
	short			gadget_svc_number		[MAX_REC_ELECTRONIC];
	short			gadget_svc_type			[MAX_REC_ELECTRONIC];
	short			PR_AsIf_Preferred_SpDrug[MAX_REC_ELECTRONIC];
	short			PR_AsIf_Preferred_Basket[MAX_REC_ELECTRONIC];
	short			PR_AsIf_RuleStatus		[MAX_REC_ELECTRONIC];
	int				gadget_svc_code			[MAX_REC_ELECTRONIC];
	int				PR_Date					[MAX_REC_ELECTRONIC];
	int				PR_Original_Largo_Code	[MAX_REC_ELECTRONIC];
	int				PR_DoctorPrID			[MAX_REC_ELECTRONIC];
	int				PR_AsIf_Preferred_Largo	[MAX_REC_ELECTRONIC];
	int				PR_AsIf_ParentGroupCode	[MAX_REC_ELECTRONIC];
	int				PharmIshurProfession	[MAX_REC_ELECTRONIC];
	int				MemberPtnAmount			[MAX_REC_ELECTRONIC];

	// Input message variables.
	PHARMACY_INFO	Phrm_info;
	int				v_PharmNum;
	short			v_InstituteCode;
	short			v_TerminalNum;
	short			v_ActionType;
	short			SALE_ACTION	= DRUG_SALE;
	int				v_MemberIdentification;
	short			v_IdentificationCode;
	short			v_IdentificationCode_s;
	short			v_MemberBelongCode;
	short		 	v_MoveCard;
	int				v_CardOwnerID;
	short			v_CardOwnerIDCode;
	short			v_in_hospital;	// DonR 02Aug2011 - Member Hospitalization Warning enhancement.
	short			v_TypeDoctorIdentification;
	int				DocID;
	short			DocChkInteractions;
	short			v_RecipeSource;
	int				v_DoctorLocationCode;
	int				v_DoctorLocationCodeMinus990000;
	int				v_ClientLocationCode;
	char			first_center_type [5];
	short			v_NumOfDrugLinesRecs;
	short			v_NumValidDrugLines;
	short			v_NumDrugLinesSent;
	short			v_MsgExistsFlg;
	short			v_MsgIsUrgentFlg;

	// "Nihul Tikrot" variables.
	short			DL_TikratMazonFlag;
	short			MemberHasTikra;
	short			MemberHasCoupon;
	short			MemberBuyingTikMazon;
	short			ThisIshurHasTikra;
	short			AnIshurHasTikra;
	short			TikrotRPC_Called;
	short			TikrotRPC_Error;
	short			TikrotStatus;
	int				FamilySalePrID;
	int				Yesterday;
	int				FamilyMemberID;
	short			FamilyMemberIDCode;
	char			v_DrugTikraType [2];
	short			v_DrugSubsidized;
	int				MemberPtnAmt;
	int				ChemicalPrice;
	int				tikra_discount	= 0;
	int				subsidy_amount	= 0;

	// Variables for deletion/refund of previous sales.
	int				v_DeletedPrID;
	int				v_DeletedPrDate;
	int				v_DeletedPrPharm;
	int				v_DeletedPrSubAmt;
	short			v_DeletedPrYYMM;
	int				v_DeletedPrPhID;
	short			v_DeletedPrDelFlg;
	short			v_DeletedPrTkMazon;
	short			v_DeletedPrPrcCode;
	int				v_DeletedPrOP;
	int				v_DeletedPrUnits;
	int				v_DeletedPrPtn;
	int				v_DeletedPrIshNum;
	short			v_DeletedPrIshSrc;
	short			v_DeletedPrIshTik;
	short			v_DeletedPrIshTkTp;
	short			v_DeletedPrPrtMeth;
	short			v_DeletedPrBasket;
	short			v_DeletedPrCredit;
	int				v_DeletedDocPrID;
	int				v_DeletedDocPrDate;
	int				v_DeletedDocLargo;
	int				v_DeletedPrMemPrc;
	int				v_DeletedPrSupPrc;
	int				v_DeletedPrDscPcnt;
	int				v_DeletedPrFixPrc;
	int				v_DeletedPrRuleNum;
	int				v_DeletedPrPRW_ID;
	int				DeletedPrPRW_ID			[MAX_REC_ELECTRONIC];

	// Variables for Generic Drug Substitution.
	int				EP_LargoCode;

	// Variables for Purchase Limit checking.
	int				PL_IshurNum = 0;
	int				PL_Units;
	short			PL_Months;
	short			PL_Type;
	int				PL_OpenDate;
	int				PL_CloseDate;
	int				PL_CloseDate_Min;

	int				AlreadyBoughtToday;

	// Pharmacy Special Confirmation. The source isn't transmitted in 2003.
	int				v_SpecialConfNum;
	short			v_SpecialConfSource;

	short			v_card_date;
	short			v_authorizealways;	/* from MEMBERS  *//*20001022*/

	// Pharmacy data.
	short			v_pharm_card		= 0;
	short			v_leumi_permission	= 0;
	short			v_CreditPharm		= 0;
	short			maccabicare_pharm	= 0;
	short			vat_exempt			= 0;

	// Maccabi Centers
	short			v_MacCent_cred_flg	= 0;

	// Individual Drugs prescribed
	TPresDrugs		*PrsLinP;
	int				v_DrugCode;
	int				v_Units;
	short			v_ArrayErrCode;		// DonR 11Apr2013 - to write to prescription_msgs.
	short			v_ArraySeverity;	// DonR 06Jun2013 - added new field to prescription_msgs.
	short			v_ArrayLineNo;		// DonR 10Jun2013 - added new field to prescription_msgs.
	short			ReqIncludesTreatment	= 0;

	// Return message header fields.
	int				v_FamilyHeadTZ;
	short			v_FamilyHeadTZCode;
	int				v_FamilyCreditUsed;
	int				v_CreditToBeUsed;
	int				v_CreditThisDrug;
	TSqlInd			v_FamilyCreditInd;
	int				v_FirstOfMonth;
	int				v_RecipeIdentifier;
	char			v_MemberFamilyName	[14 + 1];
	char			v_MemberFirstName	[ 8 + 1];
	short			v_MemberGender;
	char			v_DoctorFamilyName	[14 + 1];
	char			v_DoctorFirstName	[ 8 + 1];
	int				v_ConfirmationDate;
	short			v_ErrorCode;
	short			v_MemberDiscountPercent;
	int				v_ConfirmationHour;
	short			v_CreditYesNo;
	short			v_insurance_status; 
	short			v_MemberSpecPresc;
	int				v_MaxLargoVisible;

	// Drug lines
	char			PW_PrescriptionType [2];
	short			PW_NumPerDose;
	short			PW_DosesPerDay;
	short			PW_status_dummy;
	short			PW_Origin_Code;
	short			PW_SubstPermitted;
	int				PW_Pr_Date;
	int				PW_ValidFromDate;
	int				PW_Doc_Largo_Code;
	int				PW_Rx_Largo_Code;	// "Extra" variable for when we determine original Largo from doctor_presc.
	int				PW_DoctorPrID;
	short			PW_status	= 0;	// DonR 29Dec2009: Added initialization.
	int				PW_ID		= 0;	// DonR 29Dec2009: Added initialization.
	int				PW_AuthDate	= 0;	// DonR 24Jul2013.
	double			PW_RealAmtPerDose;
	int				UnitsToSell;
	int				OPToSell;
	int				PtnBeforeDiscount;

	// Gadgets table.
	int				v_service_code;
	int				v_gadget_code;
	short			v_service_number;
	short			v_service_type;
	short			v_enabled_status;
	short			v_enabled_mac;
	short			v_enabled_hova;
	short			v_enabled_keva;
	short			enabled_pvt_pharm;

	// MemberPharm table - member/pharmacy purchase restrictions
	short			v_MemPharm_Exists;
	int				v_MemPharm_PhCode;
	short			v_MemPharm_PhType;
	int				v_MemPharm_FromDt;
	int				v_MemPharm_ToDt;
	int				v_MemPharm_PhCode2;
	short			v_MemPharm_PhType2;
	int				v_MemPharm_FromDt2;
	int				v_MemPharm_ToDt2;
	short			v_MemPharm_ResType;
	short			v_MemPharm_PermittedOwner;
	short			MemPharm_Result;

	// General SQL variables.
	char			PossibleInsuranceType[2];
	int				max_drug_date;
	int				illness_bitmap;
	int				SysDate;
	int				StopUseDate;
	int				StopBloodDate;
	int				MaxRefillDate;
	int				ShortOverlapDate;	// DonR 19Apr2010 for new early-refill warning.
	int				LongOverlapDate;	// DonR 19Apr2010 for new early-refill warning.
	int				Yarpa_Price;
	int				Macabi_Price;
	int				Supplier_Price;	// 21NOV2002 Fee Per Service enh.
	short			v_PriceListCode;
	short			LineNum;
	int		      	v_TempMembIdentification;
	short			v_TempIdentifCode;
	int				v_TempMemValidUntil;
	short			RetPartCode;
	int				EarliestPrescrDate;
	int				LatestPrescrDate;
	int				RowsFound;
	int				high_rules_found;
	int				low_rules_found;
	int				v_prw_id;
	int				DL_economypri_grp;
	int				MatchingDiagnosis;
	short			SomeRuleApplies			= 0;	// DonR 22Mar2011
	char			InsuranceTypeToCheck	[ 1 + 1];
	short			OriginCode				= 5003;
	char			DummyPhone				[10 + 1];
	int				DummyRealDoctorTZ;
	int				DummyInt;

	// Member fields that aren't used in this transaction - added so we can
	// use a common read-member ODBC operation.
	int				MemberZipCode;
	char			MemberDefaultPhone		[10 + 1];
	char			MemberStreet			[12 + 1];
	char			MemberHouseNum			[ 4 + 1];
	char			MemberCity				[20 + 1];


	// Select cursor for family members OTHER than the member for whom drugs are being purchased.
	DeclareCursorInto (	MAIN_DB, READ_FamilyMembers_cur,
						&FamilyMemberID,			&FamilyMemberIDCode,
						&v_FamilyHeadTZ,			&v_FamilyHeadTZCode,
						&v_MemberIdentification,	END_OF_ARG_LIST			);


	// Body of function
 
	// Initialize variables.
	REMEMBER_ISOLATION;
	PosInBuff				= IBuffer;
	v_ErrorCode				= NO_ERROR;
	SysDate					= GetDate ();
	v_ConfirmationHour		= GetTime ();
	v_MsgExistsFlg			= 0;
	v_MsgIsUrgentFlg		= 0;
	v_NumOfDrugLinesRecs	= 0;
	EarliestPrescrDate		= AddDays (SysDate, (0 - PRESCR_VALID_DAYS));
	LatestPrescrDate		= AddDays (SysDate, 90);	// DonR 19Aug2015: changed from macro that resolved to 75 days.
	MaxFutureDate			= AddDays (SysDate, SaleOK_EarlyDays);
	v_DrugsBuf.max			= 0;	// DonR 16Feb2006: Initialize global buffer.
	FamilySize				= 0;
	tikra_discount			= 0;
	subsidy_amount			= 0;

	// Set "new style" error handling so (A) errors are stored in an array, and (B) drug-specific
	// errors do not stop processing for a given drug (and thus multiple errors can be reported
	// for each drug).
	SetErrorVarEnableArr ();

	// Vacation ishurim should be ignored for the last five days - so the member
	// can buy more drugs when s/he gets back from his/her trip.
	PL_CloseDate_Min = AddDays (SysDate, 5);

	// Initialize all elements of Prescription Data array.
	memset ((char *)SPres,		0, sizeof(SPres));
	memset ((char *)DocRx,		0, sizeof(DocRx));


	// Read message fields data into variables.
	v_PharmNum					= GetLong	(&PosInBuff, TPharmNum_len					); CHECK_ERROR ();	//  10,  7
	v_InstituteCode				= GetShort	(&PosInBuff, TInstituteCode_len				); CHECK_ERROR ();	//  17,  2
	v_TerminalNum				= GetShort	(&PosInBuff, TTerminalNum_len				); CHECK_ERROR ();	//  19,  2
	v_ActionType				= GetShort	(&PosInBuff, 2								); CHECK_ERROR ();	//  21,  2
	v_DeletedPrID				= GetLong	(&PosInBuff, 9								); CHECK_ERROR ();	//  23,  9
	v_DeletedPrPharm			= GetLong	(&PosInBuff, TPharmNum_len					); CHECK_ERROR ();	//  32,  7
	v_DeletedPrYYMM				= GetShort	(&PosInBuff, 4								); CHECK_ERROR ();	//  39,  4
	v_DeletedPrPhID				= GetLong	(&PosInBuff, 6								); CHECK_ERROR ();	//  43,  6
	v_MemberIdentification		= GetLong	(&PosInBuff, TGenericTZ_len					); CHECK_ERROR ();	//  49,  9
	v_IdentificationCode		= GetShort	(&PosInBuff, TIdentificationCode_len		); CHECK_ERROR ();	//  58,  1
	v_MemberBelongCode			= GetShort	(&PosInBuff, TMemberBelongCode_len			); CHECK_ERROR ();	//  59,  2
	v_MoveCard					= GetShort	(&PosInBuff, TMoveCard_len					); CHECK_ERROR ();	//  61,  4
	v_CardOwnerID				= GetLong	(&PosInBuff, TGenericTZ_len					); CHECK_ERROR ();	//  65,  9
	v_CardOwnerIDCode			= GetShort	(&PosInBuff, TIdentificationCode_len		); CHECK_ERROR ();	//  74,  1
	v_TypeDoctorIdentification	= GetShort	(&PosInBuff, TTypeDoctorIdentification_len	); CHECK_ERROR ();	//  75,  1
	DocID						= GetLong	(&PosInBuff, TGenericTZ_len					); CHECK_ERROR ();	//  76,  9
	v_RecipeSource				= GetShort	(&PosInBuff, TRecipeSource_len				); CHECK_ERROR ();	//  85,  2
	v_ClientLocationCode		= GetLong	(&PosInBuff, TDeviceCode_len				); CHECK_ERROR ();	//  87,  7
	v_DoctorLocationCode		= GetLong	(&PosInBuff, TDeviceCode_len				); CHECK_ERROR ();	//  94,  7
	v_SpecialConfSource			= GetShort	(&PosInBuff, TSpecConfNumSrc_len			); CHECK_ERROR ();	// 101,  1
	v_SpecialConfNum			= GetLong	(&PosInBuff, TNumofSpecialConfirmation_len	); CHECK_ERROR ();	// 102,  8
	ChemicalPrice				= GetLong	(&PosInBuff, 9								); CHECK_ERROR ();	// 110,  9

	// 11 characters reserved for future use.
	GetString								(&PosInBuff, filler, 11						); CHECK_ERROR ();	// 119, 11

	v_NumOfDrugLinesRecs		= GetShort	(&PosInBuff, TElectPR_LineNumLen			); CHECK_ERROR ();	// 130,  2 

	// Set identifying info for return message.
	ssmd_data_ptr->pharmacy_num		= v_PharmNum;
	ssmd_data_ptr->member_id		= Member.ID			= v_MemberIdentification;
	ssmd_data_ptr->member_id_ext	= Member.ID_code	= v_IdentificationCode_s	= v_IdentificationCode;

	
	// Validate Number of Drugs sold.
	if ((v_NumOfDrugLinesRecs	<  1) && (v_ActionType != SALE_DELETION))
	{
		SET_ERROR (ERR_FILE_TOO_SHORT);
		RESTORE_ISOLATION;
		return (ERR_FILE_TOO_SHORT);
	}

	if (v_NumOfDrugLinesRecs > MAX_REC_ELECTRONIC)
	{
		SET_ERROR (ERR_FILE_TOO_LONG);
		RESTORE_ISOLATION;
		return (ERR_FILE_TOO_LONG);
	}

	// Get repeating portion of message.
	// DonR 19Aug2010: If this is a sale deletion with 99 as the number of drug lines, we don't want
	// to read drug-line data from the incoming message; it's a request for full cancellation of the
	// sale without specifying what drugs were sold (perhaps because the pharmacy doesn't know).
	if ((v_ActionType != SALE_DELETION) || (v_NumOfDrugLinesRecs != 99))
	{
		for (i = 0; i < v_NumOfDrugLinesRecs; i++)	// Each drug line is 93 characters long.
		{
			PR_LineNumber			[i]	= GetShort	(&PosInBuff, TElectPR_LineNumLen	); CHECK_ERROR ();	// 132,  2
			PR_DoctorPrID			[i]	= GetLong	(&PosInBuff, TDoctorRecipeNum_len	); CHECK_ERROR ();	// 134,  6
			PR_Date					[i]	= GetLong	(&PosInBuff, TDate_len				); CHECK_ERROR ();	// 140,  8

			// DonR 02Mar2016: For now, Largo Codes are only five digits even though the message space for them
			// has expanded. Since six-digit Largo Codes blow up transmission of tables to AS/400 (and we've seen
			// this happen only for Doctor Largo Code), catch this problem right at the outset and reject the
			// transaction.
			// DonR 15Jul2024 User Story #349368: Largo Codes are being expanded to 6 digits on AS/400.
			// Accordingly, adjust the "sanity check" to allow one more digit.
			PR_Original_Largo_Code	[i]	= GetLong	(&PosInBuff, 9						); CHECK_ERROR ();	// 148,  9
			if (PR_Original_Largo_Code [i] > 999999)
				SET_ERROR (ERR_WRONG_FORMAT_FILE);

			PR_WhyNonPreferredSold	[i]	= GetShort	(&PosInBuff, TElectPR_SubstPermLen	); CHECK_ERROR ();	// 157,  2

			// Drug actually dispensed.
			SPres[i].DrugCode			= GetLong	(&PosInBuff, 9						); CHECK_ERROR ();	// 159,  9

			// Units/OP to be sold.
			SPres[i].Units				= GetLong	(&PosInBuff, TUnits_len				); CHECK_ERROR ();	// 168,  5
			SPres[i].Op					= GetLong	(&PosInBuff, TOp_len				); CHECK_ERROR ();	// 173,  5

			// DonR 12Jul2010: These numbers are what the doctor actually prescribed; they are
			// used for overdose checking, but for nothing else.
			SPres[i].Doctor_Units		= GetLong	(&PosInBuff, TUnits_len				); CHECK_ERROR ();	// 178,  5
			SPres[i].Doctor_Op			= GetLong	(&PosInBuff, TOp_len				); CHECK_ERROR ();	// 183,  5

			PR_NumPerDose		[i]		= GetShort	(&PosInBuff, TUnits_len				); CHECK_ERROR ();	// 188,  5
			PR_DosesPerDay		[i]		= GetShort	(&PosInBuff, TUnits_len				); CHECK_ERROR ();	// 193,  5
			SPres[i].Duration			= GetShort	(&PosInBuff, TDuration_len			); CHECK_ERROR ();	// 198,  3
			SPres[i].LinkDrugToAddition	= GetLong	(&PosInBuff, 9						); CHECK_ERROR ();	// 201,  9

			// 15 characters reserved for future use.
			GetString								(&PosInBuff, filler, 15				); CHECK_ERROR ();	// 210, 15

			// DonR 13Dec2016: Copy Prescription Source into sPres array - it's used in Quantity Limit checking now.
			// DonR 26Dec2016: Also copy Doctor ID and Doc ID Type, for interaction checking.
			SPres[i].PrescSource	= v_RecipeSource;
			SPres[i].DocIDType		= v_TypeDoctorIdentification;
			SPres[i].DocID			= DocID;

			// DonR 27Dec2016: Load up the new DocRx array with some doctor-prescription data, just for
			// compatibility with routines like CheckForInteractionIshur().
			if (PR_DoctorPrID [i] > 0)
			{
				SPres [i].NumDocRxes	= 1;
				SPres [i].FirstRx		= i;
				SPres [i].FirstDocPrID	= PR_DoctorPrID [i];	// DonR 27Mar2017 - for doc-saw-specialist-letter lookup.
				DocRx [i].PrID			= PR_DoctorPrID [i];
				DocRx [i].FromDate		= PR_Date		[i];
				DocRx [i].Units			= SPres [i].Units;
				DocRx [i].OP			= SPres [i].Op;
			}
			else
			{
				SPres [i].NumDocRxes = SPres [i].FirstRx = SPres [i].FirstDocPrID = 0;
			}
		}
	}	// NOT a delete-entire-sale request.

	// Get amount of input not eaten.
	ChckInputLen (TotalInputLen - (PosInBuff - IBuffer));
	CHECK_ERROR ();


	// SQL Transaction Retry Loop.
	reStart = MAC_TRUE;

	for (tries = 0; tries < SQL_UPDATE_RETRIES && reStart == MAC_TRUE; tries++)
	{
		// Initialize answer variables.
		v_ErrorCode				= NO_ERROR;
		reStart					= MAC_FALS;
		v_RecipeIdentifier		= 0;
		v_MemberGender			= 0;
		v_MemberDiscountPercent	= 0;
		MemberHasTikra			= 0;
		MemberHasCoupon			= 0;
		MemberBuyingTikMazon	= 0;
		ThisIshurHasTikra		= 0;
		AnIshurHasTikra			= 0;
		v_CreditYesNo			= 0;
		v_PriceListCode 		= 0;
		TypeCheckSpecPr			= 0;
		ReqIncludesTreatment	= 0;
		v_MemberFamilyName	[0]	= 0;
		v_MemberFirstName	[0]	= 0;
		v_DoctorFamilyName	[0]	= 0;
		v_DoctorFirstName	[0]	= 0;
		DocChkInteractions		= 1;	// Default is that all interactions *are* checked.
		v_ConfirmationDate		= GetDate ();
		SysDate					= GetDate ();
		strcpy (first_center_type,		"02");	// Default for Maccabi Center Type.

		// Initialize drug-level arrays, including those relating to Pharmacy Ishur.
		// DonR 28Mar2011: Moved this loop to execute unconditionally before the deletion section,
		// because otherwise DrugTikraType wasn't getting initialized and we got frequent database
		// errors (-391) trying to write to prescription_drugs as the column is set to NOT NULL.
		for (i = 0; i < v_NumOfDrugLinesRecs; i++)
		{
			SPres[i].DL.package_size				= 1;	// Set default to avoid division by zero.
			PR_AsIf_Preferred_Largo	[i]				= 0;
			PR_AsIf_Preferred_SpDrug[i]				= 0;
			PR_AsIf_Preferred_Basket[i]				= 0;
			PR_AsIf_ParentGroupCode	[i]				= 0;
			PR_AsIf_RuleStatus		[i]				= 0;
			preferred_mbr_prc_code	[i]				= 0;
			MajorDrugIndex			[i]				= -1;	// Default - no major drug found.
															// (Zero is a valid index value.)
			gadget_svc_number		[i]				= 0;
			gadget_svc_type			[i]				= 0;
			gadget_svc_code			[i]				= 0;
			DrugTikraType			[i]				= ' ';
			DrugCoupon				[i]				= '0';
			DrugRefundOffset		[i]				= 0;
			PR_ExtendAS400IshurDays	[i]				= 0;
			PharmIshurProfession	[i]				= 0;
			MemberPtnAmount			[i]				= 0;
			SPres[i].ret_part_source.insurance_used	= BASIC_INS_USED;
			SPres[i].DocChkInteractions				= 1;	// Interactions are checked *unless* a specific doctor has been flagged.
		}


		// Test temporary Maccabi cards.
		// DonR 29Sep2013: Moved this code up here, since if the current transaction is a
		// deletion request, we need to know the member's real T.Z. Number *before* checking
		// the original sale.
		if (v_IdentificationCode == 7)
		{
			SET_ISOLATION_DIRTY;

			ExecSQL (	MAIN_DB, READ_tempmemb,
						&v_TempIdentifCode,			&v_TempMembIdentification,	&v_TempMemValidUntil,
						&v_MemberIdentification,	END_OF_ARG_LIST										);

			Conflict_Test (reStart);

			if (SQLERR_code_cmp (SQLERR_not_found) == MAC_TRUE)
			{
				SetErrorVarArr (&v_ErrorCode, ERR_MEMBER_ID_CODE_WRONG, 0, 0, NULL, &ErrOverflow);
				err = MAC_TRUE;
			}
			else	// DonR 20Feb2005: Added "else" so we don't treat not-found
					// as a critical error.
			if (SQLERR_error_test ())
			{
				SetErrorVarArr (&v_ErrorCode, ERR_DATABASE_ERROR, 0, 0, NULL, &ErrOverflow);
				err = MAC_TRUE;
			}
			else	// DonR 20Feb2005: Added "else" so we use the values from
					// tempmemb only if row was actually read from the database.
					// DonR 20Oct2013: If the current action is a sale deletion, an expired
					// temporary card is still usable.
			if ((v_TempMemValidUntil >= SysDate) || (v_ActionType == SALE_DELETION))
			{
				v_IdentificationCode   = v_TempIdentifCode;
				v_MemberIdentification = v_TempMembIdentification;
			}
		}	// Temporary Member test.


		// If this is a deletion, find the sale to be deleted; otherwise make sure
		// Deleted Pr. ID is zero.
		// DonR 05Sep2010: If the sale to be deleted isn't found, force v_NumOfDrugLinesRecs to zero; this
		// will prevent bogus drug-line information from being returned to the pharmacy.
		if (v_ActionType == SALE_DELETION)
		{
			gettimeofday (&EventTime[EVENT_DELETE_START], 0);

			do	// Dummy loop for deletion-handling stuff.
			{
				SET_ISOLATION_DIRTY;

				DeletionValid = 1;	// Assume everything's OK until proven otherwise. 

				// If pharmacy didn't supply a Presciption ID to delete, try to identify the original
				// sale from the other parameters supplied.
				// DonR 30Jun2016: Added del_flg to WHERE clause, since there may be duplicate rows when
				// sales have arrived via spool.
				if (v_DeletedPrID == 0)
				{
					ExecSQL (	MAIN_DB, READ_Find_Prescription_ID_to_delete,
								&v_DeletedPrID,
								&v_MemberIdentification,	&v_DeletedPrPharm,
								&v_DeletedPrYYMM,			&v_DeletedPrPhID,
								&DRUG_DELIVERED,			&v_IdentificationCode,
								&v_MemberBelongCode,		END_OF_ARG_LIST				);

					Conflict_Test (reStart);

					if ((SQLERR_code_cmp (SQLERR_not_found) == MAC_TRUE) || (v_DeletedPrID < 1) || (v_DeletedPrID > 999999999))
					{
						v_DeletedPrID = 0;	// To get rid of NULL value.
						SetErrorVarArr (&v_ErrorCode, ERR_PRESCRIPTION_ID_NOT_FOUND, 0, 0, NULL, &ErrOverflow);
						DeletionValid = 0;	// So we don't contact AS/400 for "Shaban" or "Nihul Tikrot". 
						v_NumOfDrugLinesRecs = 0; // So we don't return bogus drug-line stuff to pharmacy.
						break;
					}

					if (SQLERR_error_test ())
					{
						SetErrorVarArr (&v_ErrorCode, ERR_DATABASE_ERROR, 0, 0, NULL, &ErrOverflow);
						DeletionValid = 0;	// So we don't contact AS/400 for "Shaban" or "Nihul Tikrot". 
						v_NumOfDrugLinesRecs = 0; // So we don't return bogus drug-line stuff to pharmacy.
						break;
					}

					// If we get here, a prior sale was found - although it may have already been deleted.
				}	// Pharmacy did not supply original Prescription ID.


				// Read in some relevant fields from the prescriptions row to be deleted.
				ExecSQL (	MAIN_DB, READ_Get_X003_deleted_prescription_row_values,
							&v_DeletedPrDate,		&v_DeletedPrSubAmt,
							&v_DeletedPrCredit,		&v_DeletedPrDelFlg,
							&v_DeletedPrID,			&DRUG_DELIVERED,
							END_OF_ARG_LIST												);

				if (SQLERR_code_cmp (SQLERR_not_found) == MAC_TRUE)
				{
					SetErrorVarArr (&v_ErrorCode, ERR_PRESCRIPTION_ID_NOT_FOUND, 0, 0, NULL, &ErrOverflow);
					v_NumOfDrugLinesRecs = 0; // So we don't return bogus drug-line stuff to pharmacy.
					DeletionValid = 0;	// So we don't contact AS/400 for "Shaban" or "Nihul Tikrot". 
					break;
				}
				else
				{
					if (SQLERR_error_test ())
					{
						SetErrorVarArr (&v_ErrorCode, ERR_DATABASE_ERROR, 0, 0, NULL, &ErrOverflow);
						DeletionValid = 0;	// So we don't contact AS/400 for "Shaban" or "Nihul Tikrot". 
						v_NumOfDrugLinesRecs = 0; // So we don't return bogus drug-line stuff to pharmacy.
						break;
					}
					else
					{
						// Successful read - but if this sale has already been deleted, throw an error.
						if (v_DeletedPrDelFlg == ROW_DELETED)
						{
							SetErrorVarArr (&v_ErrorCode, ERR_PRESC_ALREADY_DELETED, 0, 0, NULL, &ErrOverflow);
							DeletionValid = 0;	// So we don't contact AS/400 for "Shaban" or "Nihul Tikrot". 
							v_NumOfDrugLinesRecs = 0; // So we don't return bogus drug-line stuff to pharmacy.
							break;
						}
					}
				}	// Something other than not-found.

				// If we get here, we successfully read the prescriptions row to be deleted.


				// If pharmacy supplied a list of drugs to delete, read participation data from the
				// original drug sale. If quantity is different from the original sale, throw an error.
				if ((v_NumOfDrugLinesRecs > 0) && (v_NumOfDrugLinesRecs < 99))
				{
					for (i = 0; i < v_NumOfDrugLinesRecs; i++)
					{
						v_DrugCode = SPres[i].DrugCode;

						ExecSQL (	MAIN_DB, TR5003_READ_prescription_drugs_rows_to_delete,
									&v_DeletedPrOP,			&v_DeletedPrUnits,		&v_DeletedPrPtn,
									&v_DeletedPrTkMazon,	&v_DeletedPrPrcCode,	&v_DeletedPrDelFlg,
									&v_DeletedPrPrtMeth,	&v_DeletedPrIshNum,		&v_DeletedPrIshSrc,
									&v_DeletedPrIshTik,		&v_DeletedPrIshTkTp,	&v_DeletedPrMemPrc,
									&v_DeletedPrSupPrc,		&v_DeletedPrDscPcnt,	&v_DeletedPrFixPrc,
									&v_DeletedPrRuleNum,	&v_DeletedPrBasket,		&v_DeletedPrPRW_ID,
									&v_DeletedDocPrID,		&v_DeletedDocPrDate,	&v_DeletedDocLargo,
									&v_DeletedPrID,			&DRUG_DELIVERED,		&v_DrugCode,
									END_OF_ARG_LIST															);

						if (SQLERR_code_cmp (SQLERR_not_found) == MAC_TRUE)
						{
							SetErrorVarArr (&SPres[i].DrugAnswerCode,	ERR_DRUG_NOT_IN_ORIG_SALE,		v_DrugCode,	i + 1,	NULL, &ErrOverflow);
							SetErrorVarArr (&v_ErrorCode,				ERR_CANT_DEL_PRESC_OR_DRUGS,	0,			0,		NULL, &ErrOverflow);
							SPres[i].DFatalErr = MAC_TRUE;
							DeletionValid = 0;	// So we don't contact AS/400 for "Shaban" or "Nihul Tikrot". 
							break;
						}
						else
						{
							if (SQLERR_error_test ())
							{
								SetErrorVarArr (&v_ErrorCode, ERR_DATABASE_ERROR, 0, 0, NULL, &ErrOverflow);
								DeletionValid = 0;	// So we don't contact AS/400 for "Shaban" or "Nihul Tikrot". 
								break;
							}
							else
							{
								// Successful read - but if this drug has already been deleted, throw an error.
								// Question: Should this drug be removed from the current deletion request?
								if (v_DeletedPrDelFlg == ROW_DELETED)
								{
									SetErrorVarArr (&SPres[i].DrugAnswerCode,	ERR_DRUG_ALREADY_DELETED,		v_DrugCode,	i + 1,	NULL, &ErrOverflow);
									SetErrorVarArr (&v_ErrorCode,				ERR_CANT_DEL_PRESC_OR_DRUGS,	0,			0,		NULL, &ErrOverflow);
									DeletionValid = 0;	// So we don't contact AS/400 for "Shaban" or "Nihul Tikrot". 
									SPres[i].DFatalErr = MAC_TRUE;
								}

								// Compare quantities in deletion request with original sale; if there is
								// a discrepancy, throw an error.
								// DonR 30Jun2010: Pharmacy is supposed to send us negative units and OP -
								// so if they don't, it's an error.
								// DonR 05Jul2010: Pharmacy will send positive OP and Units; we will "flip"
								// them negative in a millisecond or so.
								if ((SPres[i].Op	!= v_DeletedPrOP)		||
									(SPres[i].Units	!= v_DeletedPrUnits))
								{
									SetErrorVarArr (&SPres[i].DrugAnswerCode,	ERR_QTY_DIFFERENT_IN_ORIG_SALE,	v_DrugCode,	i + 1,	NULL, &ErrOverflow);
									SetErrorVarArr (&v_ErrorCode,				ERR_CANT_DEL_PRESC_OR_DRUGS,	0,			0,		NULL, &ErrOverflow);
									DeletionValid = 0;	// So we don't contact AS/400 for "Shaban" or "Nihul Tikrot". 
									SPres[i].DFatalErr = MAC_TRUE;
								}

								// Copy original-sale values into arrays.
								// DonR 28Jun2010: Deletion participation is negative.
								// DonR 05Jul2010: According to Iris Shaya, pharmacy will send positive numbers
								// for OP and Units, and we will "flip" them until just before we send the
								// response back to the pharmacy; at that point we will "flip" them back. Yuck.
								SPres[i].DL.tikrat_mazon_flag	= v_DeletedPrTkMazon;
								SPres[i].RetPartCode			= v_DeletedPrPrcCode;
								SPres[i].SpecPrescNum			= v_DeletedPrIshNum;
								SPres[i].SpecPrescNumSource		= v_DeletedPrIshSrc;
								SPres[i].IshurWithTikra			= v_DeletedPrIshTik;
								SPres[i].IshurTikraType			= v_DeletedPrIshTkTp;
								SPres[i].InsPlusPtnSource		= v_DeletedPrPrtMeth;
								SPres[i].in_health_pack			= v_DeletedPrBasket;	// DonR 11Jul2010
								SPres[i].RetOpDrugPrice			= v_DeletedPrMemPrc;	// DonR 20Jul2010
								SPres[i].SupplierDrugPrice		= v_DeletedPrSupPrc;	// DonR 20Jul2010
								SPres[i].AdditionToPrice		= v_DeletedPrDscPcnt;	// DonR 20Jul2010
								SPres[i].PriceSwap				= v_DeletedPrFixPrc;	// DonR 20Jul2010
								SPres[i].rule_number			= v_DeletedPrRuleNum;	// DonR 20Jul2010
								SPres[i].Op						= 0 - v_DeletedPrOP;	// DonR 05Jul2010
								SPres[i].Units					= 0 - v_DeletedPrUnits;	// DonR 05Jul2010
								MemberPtnAmount			[i]		= 0 - v_DeletedPrPtn;	// DonR 28Jun2010
								DeletedPrPRW_ID			[i]		= v_DeletedPrPRW_ID;	// DonR 24Aug2010
								PR_DoctorPrID			[i]		= v_DeletedDocPrID;		// DonR 11Jun2015
								PR_Date					[i]		= v_DeletedDocPrDate;	// DonR 14Jun2015
								PR_Original_Largo_Code	[i]		= v_DeletedDocLargo;	// DonR 14Jun2015

								// Participation method needs to be "deconstructed".
								SPres[i].ret_part_source.insurance_used	= (v_DeletedPrPrtMeth % 100) / 10;
								SPres[i].ret_part_source.table			= (v_DeletedPrPrtMeth %  10);
							}	// Successful read.
						}	// Something other than not-found.
					}	// Loop through drugs sent by pharmacy.
				}	// Pharmacy has sent a list of drugs to delete.

				else
				{
					// If pharmacy did not supply a list of drugs to delete, read in all the drugs from
					// the original sale. Note that if pharmacy sends zero for the number of drugs to
					// delete, we do NOT automatically delete the whole prior sale - instead, we just
					// throw an error.
					if (v_NumOfDrugLinesRecs == 99)	// Pharmacy has requested full deletion of prior sale.
					{
						v_NumOfDrugLinesRecs = 0;	// Re-initialize number of drugs.

						// Do we need to copy additional fields from original sale?
						// DonR 10Apr2011: Declare cursor only if it's needed.
						DeclareCursorInto (	MAIN_DB, TR5003_deldrugs_cur,
											&v_DrugCode,			&v_DeletedPrOP,			&v_DeletedPrUnits,
											&v_DeletedPrPtn,		&v_DeletedPrTkMazon,	&v_DeletedPrPrcCode,
											&v_DeletedPrPrtMeth,	&v_DeletedPrIshNum,		&v_DeletedPrIshSrc,
											&v_DeletedPrIshTik,		&v_DeletedPrIshTkTp,	&v_DeletedPrMemPrc,
											&v_DeletedPrSupPrc,		&v_DeletedPrDscPcnt,	&v_DeletedPrFixPrc,
											&v_DeletedPrRuleNum,	&v_DeletedPrBasket,		&v_DeletedPrPRW_ID,
											&v_DeletedDocPrID,		&v_DeletedDocPrDate,	&v_DeletedDocLargo,
											&v_DeletedPrID,			&DRUG_DELIVERED,		&ROW_NOT_DELETED,
											END_OF_ARG_LIST															);

						OpenCursor (	MAIN_DB, TR5003_deldrugs_cur	);

						if (SQLERR_error_test ())
						{
							SetErrorVarArr (&v_ErrorCode, ERR_DATABASE_ERROR, 0, 0, NULL, &ErrOverflow);
							DeletionValid	= 0;	// So we don't contact AS/400 for "Shaban" or "Nihul Tikrot". 
							break;
						}

						do
						{
							FetchCursor (	MAIN_DB, TR5003_deldrugs_cur	);

							Conflict_Test (reStart);

							if (SQLCODE == 0)	// Good read.
							{
								// Copy original-sale values into arrays.
								// DonR 28Jun2010: Deletion participation is negative.
								// DonR 30Jun2010: So are OP and Units.
								SPres[v_NumOfDrugLinesRecs].DrugCode				= v_DrugCode;
								SPres[v_NumOfDrugLinesRecs].RetPartCode				= v_DeletedPrPrcCode;
								SPres[v_NumOfDrugLinesRecs].DL.tikrat_mazon_flag	= v_DeletedPrTkMazon;
								SPres[v_NumOfDrugLinesRecs].SpecPrescNum			= v_DeletedPrIshNum;
								SPres[v_NumOfDrugLinesRecs].SpecPrescNumSource		= v_DeletedPrIshSrc;
								SPres[v_NumOfDrugLinesRecs].IshurWithTikra			= v_DeletedPrIshTik;
								SPres[v_NumOfDrugLinesRecs].IshurTikraType			= v_DeletedPrIshTkTp;
								SPres[v_NumOfDrugLinesRecs].InsPlusPtnSource		= v_DeletedPrPrtMeth;
								SPres[v_NumOfDrugLinesRecs].in_health_pack			= v_DeletedPrBasket;	// DonR 11Jul2010
								SPres[v_NumOfDrugLinesRecs].RetOpDrugPrice			= v_DeletedPrMemPrc;	// DonR 20Jul2010
								SPres[v_NumOfDrugLinesRecs].SupplierDrugPrice		= v_DeletedPrSupPrc;	// DonR 20Jul2010
								SPres[v_NumOfDrugLinesRecs].AdditionToPrice			= v_DeletedPrDscPcnt;	// DonR 20Jul2010
								SPres[v_NumOfDrugLinesRecs].PriceSwap				= v_DeletedPrFixPrc;	// DonR 20Jul2010
								SPres[v_NumOfDrugLinesRecs].rule_number				= v_DeletedPrRuleNum;	// DonR 20Jul2010
								SPres[v_NumOfDrugLinesRecs].Op						= 0 - v_DeletedPrOP;	// DonR 30Jun2010
								SPres[v_NumOfDrugLinesRecs].Units					= 0 - v_DeletedPrUnits;	// DonR 30Jun2010
								MemberPtnAmount			[v_NumOfDrugLinesRecs]		= 0 - v_DeletedPrPtn;	// DonR 28Jun2010
								DeletedPrPRW_ID			[v_NumOfDrugLinesRecs]		= v_DeletedPrPRW_ID;	// DonR 24Aug2010
								PR_DoctorPrID			[v_NumOfDrugLinesRecs]		= v_DeletedDocPrID;		// DonR 11Jun2015
								PR_Date					[v_NumOfDrugLinesRecs]		= v_DeletedDocPrDate;	// DonR 14Jun2015
								PR_Original_Largo_Code	[v_NumOfDrugLinesRecs]		= v_DeletedDocLargo;	// DonR 14Jun2015

								// Participation method needs to be "deconstructed".
								SPres[v_NumOfDrugLinesRecs].ret_part_source.insurance_used	= (v_DeletedPrPrtMeth % 100) / 10;
								SPres[v_NumOfDrugLinesRecs].ret_part_source.table			= (v_DeletedPrPrtMeth %  10);

								v_NumOfDrugLinesRecs++;
							}	// Good read of drug-lines cursor.
							else
							{
								if (SQLERR_code_cmp (SQLERR_end_of_fetch) == MAC_TRUE)
								{
									break;	// No problem - we're finished reading drugs from original sale.
								}
								else
								{
									if (SQLERR_error_test ())
									{
										SetErrorVarArr (&v_ErrorCode, ERR_DATABASE_ERROR, 0, 0, NULL, &ErrOverflow);
										DeletionValid	= 0;	// So we don't contact AS/400 for "Shaban" or "Nihul Tikrot". 
										break;
									}
								}	// Something other than end-of-fetch.
							}	// Fetch failed.
						}
						while (1);	// End of cursor loop for reading prior-sale drugs.

						CloseCursor (	MAIN_DB, TR5003_deldrugs_cur	);
					}	// Pharmacy sent "99" to request full deletion of prior sale.
				}	// Pharmacy did not send a list of drugs to delete.

				// Validate Number of Drugs deleted.
				if (v_NumOfDrugLinesRecs < 1)
				{
					// This should never really happen, since if all drugs were already deleted the
					// prescriptions row should already be flagged as deleted - and we would have
					// detected the problem almost 200 lines of code ago.
					SetErrorVarArr (&v_ErrorCode, ERR_PRESC_ALREADY_DELETED, 0, 0, NULL, &ErrOverflow);
					DeletionValid = 0;	// So we don't contact AS/400 for "Shaban" or "Nihul Tikrot". 
					break;
				}
			}
			while (0);	// End of deletion-handling dummy loop.

			gettimeofday (&EventTime[EVENT_DELETE_END], 0);
		}	// Current transaction is a deletion request.


		// Process normal sale requests.
		else
		{
			v_DeletedPrID = v_DeletedPrDate = v_DeletedPrSubAmt = 0;

			// If pharmacy supplied a Pharmacy Ishur Number, check the sale request
			// against the Ishur. Initialize the PR_ExtendAS400IshurDays array
			// first, since check_sale_against_pharm_ishur() stores values there.
			err = check_sale_against_pharm_ishur (v_PharmNum,
												  v_MemberIdentification,
												  v_IdentificationCode,
												  SPres,
												  v_NumOfDrugLinesRecs,
												  v_SpecialConfNum,
												  v_SpecialConfSource,
												  PR_ExtendAS400IshurDays,
												  PharmIshurProfession,
												  &FunctionError);

			SetErrorVarArr (&v_ErrorCode, FunctionError, 0, 0, NULL, &ErrOverflow);

			if (err == 2)	// DB conflict.
			{
				reStart = MAC_TRUE;
			}
		}	// Normal sale request.


		// Get prescription id (for Prescriptions/Prescription Drugs tables).
		err = GET_PRESCRIPTION_ID (&v_RecipeIdentifier);

		if (err != NO_ERROR)
		{
		    SetErrorVarArr (&v_ErrorCode, err, 0, 0, NULL, &ErrOverflow);
		    GerrLogMini (GerrId, "Can't get PRESCRIPTION_ID error %d", err);
			continue;	// We're not in a dummy do-while loop here!
		}


		// DonR 27Apr2005: Avoid unnecessary lock errors by downgrading "isolation"
		// for this section.
		SET_ISOLATION_DIRTY;

		// DUMMY LOOP #1 TO PREVENT goto.
		do	// Exiting from LOOP jumps to the drug-handling stuff.
		{
			// Test member belong code. Assume no error to begin.
		    err = MAC_FALS;

		    switch (v_MemberBelongCode)
			{
				case MACABI_INSTITUTE:
				    break;

				default:
					// DonR 05Jun2005: To support MaccabiCare feature, permit non-Maccabi
					// members to purchase non-prescription drugs.
					if (v_RecipeSource != RECIP_SRC_NO_PRESC)
					{
						SetErrorVarArr (&v_ErrorCode, ERR_WORNG_MEMBERSHIP_CODE, 0, 0, &err, &ErrOverflow);
					}
				    break;
			}
		    if (err == MAC_TRUE)
				break;


			// Test pharmacy data.
		    err = IS_PHARMACY_OPEN (v_PharmNum, v_InstituteCode, &Phrm_info);

			// DonR 08Sep2009: Copy pharmacy data already read by IS_PHARMACY_OPEN() into local variables.
		    v_PriceListCode		= Phrm_info.price_list_num;
			v_pharm_card		= Phrm_info.pharm_card;
			v_leumi_permission	= Phrm_info.leumi_permission;
			v_CreditPharm		= Phrm_info.CreditPharm;
			maccabicare_pharm	= Phrm_info.maccabicare_pharm;
			vat_exempt			= Phrm_info.vat_exempt;

			// DonR 31Jul2013: Move error test to after we've loaded pharmacy values, since we still do things
			// like price-list lookups based on them even if the pharmacy hasn't done an open-shift.
			if (err != MAC_OK)
		    {
				SetErrorVarArr (&v_ErrorCode, err, 0, 0, NULL, &ErrOverflow);
				break;
			}


			// DonR 19Feb2008: Drugs with Largo Code >= 90000 are visible only
			// to Maccabi pharmacies.
			v_MaxLargoVisible = (MACCABI_PHARMACY) ? NO_MAX_LARGO : 89999;

			// Test prescription source.
		    err = MAC_FALS;

			// Private pharmacies with MaccabiCare flag non-zero *can* sell non-prescription drugs - 
			// but other private pharmacies cannot.
			if ((v_RecipeSource == RECIP_SRC_NO_PRESC) && (PRIVATE_PHARMACY) && (maccabicare_pharm == 0))
			{
				SetErrorVarArr (&v_ErrorCode, ERR_PHARM_CANT_SALE_PRECR_SRC, 0, 0, &err, &ErrOverflow);
			}

			// Check for unrecognized prescription source(s).
			if (!is_valid_presc_source (v_RecipeSource, PRIVATE_PHARMACY, &ErrorCodeToAssign, &DefaultSourceReject))
			{
				SetErrorVarArr (&v_ErrorCode, ErrorCodeToAssign, 0, 0, &err, &ErrOverflow);
			}

			if (err == MAC_TRUE)
				break;


			// Read and test Member information.
			// DonR 05Jun2005: For MaccabiCare, allow T.Z. of zero for non-Maccabi members.
		    if ((v_MemberIdentification == 0) && (v_MemberBelongCode == MACABI_INSTITUTE))
			{
				SetErrorVarArr (&v_ErrorCode, ERR_MEMBER_ID_CODE_WRONG, 0, 0, NULL, &ErrOverflow);
				break;
			}

			// Test member ID-CODE.
			// DonR 05Jun2005: For MaccabiCare, allow T.Z. of zero for non-Maccabi members.
			// Since we test for zero above, here we need test only for T.Z.'s between
			// 1 and 57 - which are invalid for anyone, Maccabi member or not.
		    if ((v_MemberIdentification > 0) & (v_MemberIdentification < 58))
		    {
				SetErrorVarArr (&v_ErrorCode, ERR_MEMBER_ID_CODE_WRONG, 0, 0, NULL, &ErrOverflow);
				err = MAC_TRUE;
			}

			// DonR 05Jun2005: If this is a case where Member ID can be zero, allow
			// Doctor ID to be zero as well - since we're dealing with non-prescription drugs.
			// DonR 10Jul2005: Just in case some non-Maccabi doctor has a license number
			// equal to the Teudat Zehut number of some patient, ignore that equality (see
			// third clause of the "if").
			// DonR 14Jul2010: Per Iris Shaya, suppress this error if the current transaction
			// is a sale deletion.
			if ((DocID						== v_MemberIdentification)	&&
				(v_MemberIdentification		>  0)						&&
				(v_TypeDoctorIdentification	!= 1)						&&
				(v_ActionType				!= SALE_DELETION))
			{
				SetErrorVarArr (&v_ErrorCode, ERR_DOCTOR_EQ_MEMBER, 0, 0, NULL, &ErrOverflow);
				err = MAC_TRUE;
			}


			// DonR 30Jun2005: If Member ID = 0, plug in appropriate values for a
			// non-Maccabi member.
			// DonR 05Sep2006: Moved this code up, so that we avoid looking up
			// non-existent data in the Members table.
			if (v_MemberIdentification == 0)
			{
				strcpy (v_MemberFamilyName,	"");
				strcpy (v_MemberFirstName,	"");
				v_CreditYesNo		= 0;
				v_MemberSpecPresc	= 0;	// DonR 24Jul2011: Prevent a meaningless special_prescs lookup for ID zero.
				Member.maccabi_until = Member.mac_magen_until = Member.keren_mac_until = Member.yahalom_until = 0;
				Member.date_of_bearth = SysDate;	// For lack of a more sensible default.
				strcpy (Member.insurance_type, "B");	// For lack of a more sensible default - or should we use "N"?

				// DonR 27Nov2011: For purposes of reading drug_extension and similar tables, set flags
				// for this non-member to enable reading of Maccabi rules but not IDF rules.
				Member.MemberMaccabi = 1;
				Member.MemberTzahal = Member.MemberHova = Member.MemberKeva = 0;
			}

			else
			{	// Look up (hopefully) "real" member.
				// DonR 15Feb2011: For performance, got rid of WHERE condition for del_flg; we never set
				// del_flg to anything other than 0 for this table and the field isn't in the index.
				// DonR 13May2020 CR #31591: Add new Member-on-Ventilator flag. This is
				// currently stored in the old column "asaf_code" (which was sent from
				// AS/400 but never used for anything); when we switch to MS-SQL, the
				// column should be renamed.
				ExecSQL (	MAIN_DB, READ_members_full,
							&v_MemberFamilyName,		&v_MemberFirstName,				&Member.date_of_bearth,
							&Member.maccabi_code,		&v_MemberSpecPresc,				&Member.maccabi_until,
							&Member.payer_tz,			&Member.payer_tz_code,			&v_MemberGender,
							&MemberDefaultPhone,		&MemberHouseNum,				&MemberStreet,
							&MemberCity,				&MemberZipCode,					&Member.insurance_type,

							&Member.keren_mac_code,		&Member.keren_mac_from,			&Member.keren_mac_until,
							&Member.mac_magen_code,		&Member.mac_magen_from,			&Member.mac_magen_until,
							&Member.yahalom_code,		&Member.yahalom_from,			&Member.yahalom_until,
							&Member.carry_over_vetek,	&Member.keren_wait_flag,		&illness_bitmap,
							&v_card_date,				&Member.update_date,			&Member.update_time,

							&v_authorizealways,			&Member.updated_by,				&Member.check_od_interact,
							&v_CreditYesNo,				&max_drug_date,					&v_MemberDiscountPercent,
							&v_insurance_status,		&v_FamilyHeadTZ,				&v_FamilyHeadTZCode,
							&MemberHasTikra,			&MemberHasCoupon,				&v_in_hospital,
							&Member.VentilatorDiscount,	&Member.darkonai_type,			&Member.force_100_percent_ptn,

							&Member.darkonai_no_card,	&Member.has_blocked_drugs,		&Member.died_in_hospital,
							&Member.mahoz,				&Member.dangerous_drug_status,

							&v_MemberIdentification,	&v_IdentificationCode,			END_OF_ARG_LIST					);

				Conflict_Test (reStart);
				
				// DonR 04Dec2022 User Story #408077: Use a new function to set some Member flags and values.
				SetMemberFlags (SQLCODE);
			
				if (SQLERR_code_cmp (SQLERR_not_found) == MAC_TRUE)
				{
					SetErrorVarArr (&v_ErrorCode, ERR_MEMBER_ID_CODE_WRONG, 0, 0, NULL, &ErrOverflow);
					err = MAC_TRUE;
				}
				else
				{
					if (SQLERR_error_test ())
					{
						SetErrorVarArr (&v_ErrorCode, ERR_DATABASE_ERROR, 0, 0, NULL, &ErrOverflow);
						err = MAC_TRUE;
					}
					else
					{
						// Successful read of member data.

						// DonR 30Mar2014: For convenience, store some member stuff in a structure - this will make function
						// arguments simpler.
						Member.ID				= v_MemberIdentification;
						Member.ID_code			= v_IdentificationCode;
						Member.sex				= v_MemberGender;
						Member.in_hospital		= v_in_hospital;
						Member.discount_percent	= v_MemberDiscountPercent;
						Member.illness_bitmap	= illness_bitmap;
					}	// Successful read of member data.
				}	// Something other than not-found: either success, or a DB problem.


				// Vacation ishurim and member-pharmacy restrictions aren't relevant to deletions.
				if (v_ActionType != SALE_DELETION)
				{
					// See if this member has an all-drug vacation limit-multiplier ishur.
					// DonR 15Feb2011: For performance, shuffled WHERE conditions to conform more closely to index.
					ExecSQL (	MAIN_DB, READ_Largo_99997_ishur, Largo_99997_ishur_standard_WHERE,
								&Member.PL_99997_IshurNum,		&Member.PL_99997_OpenDate,
								&v_MemberIdentification,		&v_IdentificationCode,
								&SysDate,						&SysDate,
								END_OF_ARG_LIST															);

					if (SQLCODE == 0)
					{
						Member.Has_PL_99997_Ishur = 1;
					}
					else
					{
						Member.Has_PL_99997_Ishur = Member.PL_99997_IshurNum = Member.PL_99997_OpenDate = 0;
					}


					// See if this member has a pharmacy purchase restriction.
					// Question: Does this apply to transactions other than drug sales?
					ExecSQL (	MAIN_DB, READ_MemberPharm,
								&v_MemPharm_PhCode,			&v_MemPharm_PhType,		&v_MemPharm_FromDt,
								&v_MemPharm_ToDt,			&v_MemPharm_PhCode2,	&v_MemPharm_PhType2,
								&v_MemPharm_FromDt2,		&v_MemPharm_ToDt2,		&v_MemPharm_ResType,
								&v_MemPharm_PermittedOwner,

								&v_MemberIdentification,	&v_IdentificationCode,	&ROW_NOT_DELETED,
								&SysDate,					&SysDate,				&SysDate,
								&SysDate,					END_OF_ARG_LIST									);

					v_MemPharm_Exists = (SQLERR_code_cmp (SQLERR_ok) == MAC_TRUE);

					// If member has a pharmacy purchase restriction, see how it
					// applies to this sale.
					//
					// ASSUMPTIONS:
					// 1) Restriction is still operative on its "to-date".
					// 2) If there is a match on Pharmacy Code, we don't have to look at
					//    the Pharmacy Type - assume that the people entering this data
					//    know what they're doing and there won't be inconsistency.
					// 3) There is no such thing as a blanket permission - that is, either
					//    there will be a specific Pharmacy Code OR a Pharmacy Type of 1
					//    (sale permitted at all Maccabi pharmacies).
					// 4) Member is allowed to buy non-prescription drugs anywhere.
					if (v_MemPharm_Exists)
					{
						// By default, assume that the sale is forbidden.
						MemPharm_Result = ERR_MEMBER_PHARM_SALE_FORBIDDEN;

						// Look at temporary fields first.
						if ((v_MemPharm_FromDt2 <= SysDate) && (v_MemPharm_ToDt2 >= SysDate))
						{
							if (((v_MemPharm_PhCode2 == v_PharmNum)	&& (v_PharmNum != 0)) ||
								((v_MemPharm_PhCode2 == 0) && (v_MemPharm_PhType2 == 1) && (MACCABI_PHARMACY)))
							{
								MemPharm_Result = ERR_MEMBER_PHARM_PERMITTED_TEMP;
							}
						}

						// Next look at "normal" restriction fields.
						if ((v_MemPharm_FromDt  <= SysDate) && (v_MemPharm_ToDt  >= SysDate))
						{
							if (((v_MemPharm_PhCode  == v_PharmNum)	&& (v_PharmNum != 0)) ||
								((v_MemPharm_PhCode  == 0) && (v_MemPharm_PhType  == 1) && (MACCABI_PHARMACY)))
							{
								MemPharm_Result = ERR_MEMBER_PHARM_PERMITTED;
							}
						}

						// If the restriction applies to all prescription sales, apply it
						// at the "header" level now.
						if ((v_MemPharm_ResType == 9) && (v_RecipeSource != RECIP_SRC_NO_PRESC))
						{
							SetErrorVarArr (&v_ErrorCode, MemPharm_Result, 0, 0, &err, &ErrOverflow);
						}
					}	// Member-pharmacy restriction exists.
				}	// NOT a deletion.


				// Build array of family members, NOT including the member for whom drugs
				// are being bought.
				FamilySize = 0;	// Just to be paranoid.

				OpenCursor (	MAIN_DB, READ_FamilyMembers_cur	);

				if (!SQLERR_error_test ())
				{
					// Fetch data and store in buffer.
					// For now at least, no real error checking; just quit when we hit anything.
					// We're already in "dirty read" mode, so we should be more or less OK.
					for ( ; ; )
					{
						FetchCursor (	MAIN_DB, READ_FamilyMembers_cur	);

						if (SQLCODE == 0)
						{
							FamilyMemberTZ		[FamilySize] = FamilyMemberID;
							FamilyMemberTZCode	[FamilySize] = FamilyMemberIDCode;
							if (++FamilySize >= MAX_FAMILY_SIZE)
								break;	// Buffer capacity reached.
						}
						else
						{
							break;
						}
					}
				}

				CloseCursor (	MAIN_DB, READ_FamilyMembers_cur	);
				// Done with family-member list-building.

			}	// Member ID is non-zero.


			// LOGICAL data tests.

			// Member eligibility
			// DonR 10Oct2011: Tzahal members are not in Meuhedet!
			// DonR 24Dec2015: If this is a sale deletion, treat member as valid even if s/he has left Maccabi - 
			// as long as the sale to be deleted took place within a configurable period (based on sysparams/del_valid_months).
			// Note that for these members, we see maccabi_code set negative - so the test for negative/zero
			// Member Rights has to be disabled for expired members who are deleting recent drug sales.
			//
			// DonR 27Jun2016: Per Iris, simplified the criteria, particularly for deletions.
			//    (A) For deletions, if the member is currently eligible, there is no restriction on the date of the sale being deleted.
			//    (B) For deletions of non-eligible members (who died or otherwise left Maccabi), we allow the transaction only if
			//        the sale being deleted took place within a reasonably recent period, defined in sysparams.
			//    (C) For everything *other* than deletions, Member Rights (members/maccabi_code) must be >= 0 and the Maccabi Until
			//        date must be at least today.
			//    (D) This change is based on the observation that we do *not* get a reliable Maccabi Until date for dead members -
			//        so we don't even want to look at this date for deletions. Of course, it *is* relevant for drug purchases,
			//        since only living, eligible members are supposed to be buying stuff.
			//
			// DonR 25Nov2021 User Story #206812: Get died-in-hospital indicator from the Ishpuz system. ==> NIU as of Dec2022!
			// DonR 30Nov2022 User Story #408077: Test for maccabi_code < 1, *not* < 0. Use new macro MEMBER_INELIGIBLE
			// as the test for eligibility to buy stuff; the macro is defined in MsgHndlr.h. Also, use a new macro
			// MEMBER_IN_MEUHEDET (defined in MsgHndlr.h) to decide if the person is a Meuhedet member.
			if (((v_ActionType			== SALE_DELETION)	&&										// This is a deletion request...
				 (Member.maccabi_code	<  1)				&&										// and the member is not eligible...
				 (v_DeletedPrDate		<  IncrementDate (SysDate, (0 - DeletionsValidDays))))	||	// and the sale is before the cutoff period.

				((v_ActionType			!= SALE_DELETION)	&& (MEMBER_INELIGIBLE))				||	// This is *not* a deletion request, and the member is not currently eligible.

				 (MEMBER_IN_MEUHEDET))																// Member is in Meuhedet.
			{
				// DonR 30Jun2005: If this is a MaccabiCare sale to a non-Maccabi member
				// (T.Z. = 0), don't worry about eligibility.
				if (v_MemberIdentification != 0)
				{
					SetErrorVarArr (&v_ErrorCode, ERR_MEMBER_NOT_ELEGEBLE, 0, 0, NULL, &ErrOverflow);
					err = MAC_TRUE;
				}
			}
			else
			{
				// DonR 27Jun2016: Changed the criteria for this message to test for a non-eligible member
				// based on members/maccabi_code ("Member Rights") and *not* on the Maccabi Until date.
				// DonR 30Nov2022 User Story #408077: Test for maccabi_code < 1, *not* < 0.
				if ((v_ActionType == SALE_DELETION) && (Member.maccabi_code	<  1))
				{
					SetErrorVarArr (&v_ErrorCode, DELETION_ALLOWED_FOR_EX_MEMBER, 0, 0, NULL, &ErrOverflow);
				}
				// DonR 30Nov2021: It's not yet clear if "Member died in hospital" is going to be a transaction-level
				// error, or if it will apply only to expensive items.
				else
				{
					if ((v_ActionType != SALE_DELETION) && (Member.died_in_hospital))
					{
						if (SetErrorVarArr (&v_ErrorCode, MEMBER_DIED_IN_HOSPITAL, 0, 0, NULL, &ErrOverflow))
							err = MAC_TRUE;
					}
				}
			}


			// If card had a "real" date that was different from the card-issue
			// date in the database, set error.
			if ((v_IdentificationCode_s	!= 7						)	&&	// Don't check temp. members
				(v_MoveCard				>  0						)	&&	// 10Jan2005: 1 is a valid card date!
				(v_MoveCard				<  9998						)	&&	// Card was used. 07Mar2006: 9998 = Temp. Card!
				((v_MoveCard != 9997) || (Member.MemberTzahal == 0)	)	&&	// 10Oct2011: Special card for soldiers.
				(v_MoveCard				!= v_card_date				)	&&	// Date on card doesn't match DB version.
				(v_card_date			>  0						)	&&  // Yulia 20030213: DB date is "for real".
				(v_MemberIdentification	!= 59						)	&&	// Special case - exempt real members
				(v_MemberIdentification	!= 83						))		// whose ID's have been used for testing.
			{
				// DonR 30Jul2013: Because a lot of new Maccabi Sheli cards were sent out by mail - and members'
				// data was updated with the new card dates before those cards had arrived - a new table,
				// membercard, has been added to list additional/alternative valid card dates for any given
				// member. Accordingly, check this table before sending a "card expired" error.
				ExecSQL (	MAIN_DB, READ_MemberCard,
							&v_card_date,
							&v_MemberIdentification,	&v_IdentificationCode,
							&v_MoveCard,				END_OF_ARG_LIST				);

				if (SQLCODE != 0)
				{
					SetErrorVarArr (&v_ErrorCode, ERR_CARD_EXPIRED, 0, 0, NULL, &ErrOverflow);
					err = MAC_TRUE;
				}
			}


			// leumit 20010101
			// If Member belongs to Leumit, and either (A) pharmacy isn't allowed to sell
			// Maccabi prescriptions to Leumit members, or (B) the prescription isn't from
			// a Maccabi doctor, reject the sale. If the pharmacy is permitted but the
			// prescription source is wrong, give a different error.
			// DonR 11Aug2008: Added new Maccabi Nurse prescription source.
			// DonR 04Apr2016: Per Iris Shaya, added new prescription source for Maccabi Dieticians.
			if (((!v_leumi_permission) ||
				((v_RecipeSource != RECIP_SRC_MACABI_DOCTOR)	&&
				 (v_RecipeSource != RECIP_SRC_MAC_DOC_BY_HAND)	&&
				 (v_RecipeSource != RECIP_SRC_MAC_DIETICIAN)	&&
				 (v_RecipeSource != RECIP_SRC_MACCABI_NURSE)))		&&
				(v_insurance_status == 2))
			{
				if (!v_leumi_permission)
					SetErrorVarArr (&v_ErrorCode, ERR_LEUMI_NOT_VALID,				0, 0, NULL, &ErrOverflow);
				else
					SetErrorVarArr (&v_ErrorCode, ERR_PHARM_CANT_SALE_PRECR_SRC,	0, 0, NULL, &ErrOverflow);

				err = MAC_TRUE;
			}
			// end leumit 20010101

			// DonR 10Apr2013: It looks like all the pharmacies currently have the "card" parameter
			// set to zero - which means that this block of code doesn't do anything.
			if ((v_pharm_card == 1) && (v_MemberIdentification != 0))	// only to pharmacy with flag ON
																		// DonR 30Jun2005: Doesn't apply
																		// to sales with T.Z. of zero.
			{
				if (v_IdentificationCode_s != 7) // 20010903 don't check tempmemb
				{
					if (v_card_date > 0) // member have card ,9999 read with ...
					{
						// If member didn't have card and is isn't set to "authorize always", set error. 
						if (!MEMBER_USED_MAG_CARD)
						{
							if (v_authorizealways)
							{
								// DonR 04Jun2015: If the Authorize Always flag was set because member requested it through
								// the Maccabi website or our mobile app, we don't want to zero it out - these requests expire
								// after a short time anyway. In fact, I'm not sure we should be zeroing-out AuthorizeAlways
								// in any case!
								if (Member.updated_by != 8888)
								{
									ExecSQL (	MAIN_DB, UPD_members_ClearAuthorizeAlways,
												&TrnID,					&v_MemberIdentification,
												&v_IdentificationCode,	END_OF_ARG_LIST				);

									SQLERR_error_test ();
								}	// Updated_by <> 8888, meaning that we're *not* dealing with a temporary user request.
							}	// ID entered by hand and authorize-always was set.
							else
							{
								SetErrorVarArr (&v_ErrorCode, ERR_PHARM_HAND_MADE, 0, 0, NULL, &ErrOverflow);
							}	// ID entered by hand and authorize-always was NOT set.
						}	// ID was entered by hand, not by passing magnetic card.
					}	// Member does have Maccabi card.
				}	// NOT a temporary member.
			}	// Pharmacy card flag == 1.


			// DonR 10Jul2012: Validate Doctor Location Code against Prescription Source.
			// Three circumstances can trigger an error:
			// (1) Prescription source does not require a Doctor Location Code, but one was supplied.
			// (2) Prescription source *does* require a Doctor Location Code, but one was *not* supplied.
			// (3) A Doctor Location Code (other than 999999) was supplied, but the location is invalid
			//     based on lookup in macabi_centers.
			// DonR 31Oct2012: Make this test conditional, so it executes only for drug sales. Deletions
			// do not have Doctor Location Codes, so the validation causes spurious rejections.
			if (v_ActionType == DRUG_SALE)	// DonR 31Oct2012.
			{
				ErrorCodeToAssign = 0;

				if (PrescrSource [v_RecipeSource].PrSrcDocLocnRequired == 0)
				{
					if (v_DoctorLocationCode != 0)
						ErrorCodeToAssign = ERR_DOC_LOCATION_INVALID;
				}
				else
				{	// Prescription source *does* require a valid Doctor Location Code.
					if ((v_DoctorLocationCode < 990001) || (v_DoctorLocationCode > 999999))
					{
						ErrorCodeToAssign = ERR_DOC_LOCATION_INVALID;
					}
					else
					{
						// Don't look up Code 9999.
						if (v_DoctorLocationCode < 999999)
						{
							v_DoctorLocationCodeMinus990000 = v_DoctorLocationCode - 990000;

							ExecSQL (	MAIN_DB, READ_MaccabiCenters_ValidateDocFacility,
										&RowsFound, &v_DoctorLocationCodeMinus990000, END_OF_ARG_LIST	);

							Conflict_Test (reStart);
						
							if (SQLERR_error_test ())
							{
								ErrorCodeToAssign = ERR_DATABASE_ERROR;
							}
							else
							{
								if (RowsFound < 1)
									ErrorCodeToAssign = ERR_DOC_LOCATION_INVALID;
							}	// Valid read.
						}	// Doctor Location Code needs to be looked up in macabi_centers.
					}	// Doctor Location Code is in correct range.
				}	// Prescription source *does* require a valid Doctor Location Code.
			
				// If we found something wrong with the Doctor Location Code, report it.
				if (ErrorCodeToAssign > 0)
				{
					if (SetErrorVarArr (&v_ErrorCode, ErrorCodeToAssign, 0, 0, NULL, &ErrOverflow))
					{
						err = MAC_TRUE;
						break;
					}
				}
			}	// Current action is a drug sale - DonR 31Oct2012 end.


			// If a Client Location Code was sent (e.g. member lives in an old-age home),
			// validate against the macabi_centers table. This will also allow us to
			// sell drugs against a credit line without using the member's Maccabi Card,
			// if the location has a Credit Flag of 9.
			// DonR 10Apr2013: Just to explain - v_ClientLocationCode = 1 for normal purchases
			// made by members (at least those not living in old-age homes) at pharmacies.
			// DonR 24Dec2013: If this is a private-pharmacy sale and a Client Location Code
			// was sent, return an error just as if a non-valid code had been sent.
			// DonR 13Oct2015: Give location-not-found error only for drug sales - NOT for deletions!
			if (v_ClientLocationCode > 1)
			{
				if (MACCABI_PHARMACY)
				{
					ExecSQL (	MAIN_DB, READ_MaccabiCenters_PatientFacility,
								&v_MacCent_cred_flg,	&first_center_type,
								&v_ClientLocationCode,	END_OF_ARG_LIST			);

					Conflict_Test (reStart);
				}
				else FORCE_NOT_FOUND;

				if (SQLERR_code_cmp (SQLERR_not_found) == MAC_TRUE)
				{
					if (v_ActionType == DRUG_SALE)	// DonR 13Oct2015.
					{
						SetErrorVarArr (&v_ErrorCode, ERR_LOCATION_CODE_WRONG, 0, 0, NULL, &ErrOverflow);
						err = MAC_TRUE;
						break;
					}
					else
					{
						// DonR 13Oct2015: If this is something other than a drug sale, ignore location-not-found.
						v_MacCent_cred_flg = 0;
						strcpy (first_center_type, "02");	// Default = most common value.
					}
				}
				else
				{
					if (SQLERR_error_test ())
					{
						SetErrorVarArr (&v_ErrorCode, ERR_DATABASE_ERROR, 0, 0, NULL, &ErrOverflow);
						break;
					}

					// DonR 04Jan2018 CR #13453: If this is an Al Tor order, check how many times this member
					// has ordered through Al Tor and failed to pick up his/her drugs.
					else	// Successful read of macabi_centers.
					{
						if ((v_ActionType == DRUG_SALE) && (!strncmp (first_center_type, "01", 2)))
						{
							ErrorCodeToAssign = MemberFailedToPickUpDrugs (&Member);

							if (ErrorCodeToAssign > 0)
							{
								if (SetErrorVarArr (&v_ErrorCode, ErrorCodeToAssign, 0, 0, NULL, &ErrOverflow))
								{
									err = MAC_TRUE;
									break;
								}
							}	// MemberFailedToPickUpDrugs() detected enough no-shows to throw an error.
						}	// Current action is an Al Tor sale.
					}	// Successful read of macabi_centers.
				}	// Something other than not-found for macabi_centers lookup.
			}	// Client Location Code > 1.


			// DonR 10Jul2005: Maccabi Doctor prescriptions *must* have the doctor's
			// Teudat Zehut number, *not* her license number.
			// DonR 21May2017: Don't report doctor-ID errors for sale deletions.
			if ((v_RecipeSource				== RECIP_SRC_MACABI_DOCTOR)	&&
				(v_TypeDoctorIdentification	== 1)						&&
				(v_ActionType				== DRUG_SALE))
			{
				SetErrorVarArr (&v_ErrorCode, ERR_DOCTOR_ID_CODE_WRONG, 0, 0, NULL, &ErrOverflow);
				err = MAC_TRUE;
				break;
			}

			// Test doctor's ID-CODE.
			// DonR 02Sep2004: Perform this lookup for Maccabi-by-hand prescriptions as well.
			//
			// DonR 10Jul2005: Perform this lookup for *any* prescription where the doctor-id
			// type is not set to 1. Prescriptions from doctors who are not in the doctors
			// table should have the doctor's licence number (ID Type 1), *not* the doctor's
			// Teudat Zehut number. However, if this is a non-prescription sale and the
			// Doctor ID is zero, don't check.
			if ((v_TypeDoctorIdentification	!= 1)	&&
				(((v_RecipeSource != RECIP_SRC_NO_PRESC) && (v_RecipeSource != RECIP_SRC_EMERGENCY_NO_RX)) || (DocID > 0)))
			{
				ExecSQL (	MAIN_DB, READ_doctors,
							&v_DoctorFirstName,		&v_DoctorFamilyName,
							&DummyPhone,			&DocChkInteractions,
							&DocID,					END_OF_ARG_LIST			);

				Conflict_Test (reStart);

				// DonR 21May2017: Don't report doctor-ID errors for sale deletions.
				if (SQLERR_code_cmp (SQLERR_not_found) == MAC_TRUE)
				{
					if (v_ActionType == DRUG_SALE)
					{
						SetErrorVarArr (&v_ErrorCode, ERR_DOCTOR_ID_CODE_WRONG, 0, 0, NULL, &ErrOverflow);
						err = MAC_TRUE;
						break;
					}
					else
					{
						// If this is a deletion and the doctor wasn't found, set relevant fields to blank/default.
						strcpy (v_DoctorFamilyName,	"");
						strcpy (v_DoctorFirstName,	"");
					}
				}
				else
				{
					if (SQLERR_error_test ())
					{
						SetErrorVarArr (&v_ErrorCode, ERR_DATABASE_ERROR, 0, 0, NULL, &ErrOverflow);
						break;
					}
					else
					{
						// We managed to read the doctor, so copy her interaction-check flag to the SPres structure.
						for (i = 0; i < v_NumOfDrugLinesRecs; i++)
						{
							SPres[i].DocChkInteractions = DocChkInteractions;
						}
					}	// Successfully read the doctors row.
				}
			}	// Prescription is from Maccabi doctor.

			// DonR 24Nov2011: For non-Maccabi doctors, perform the name lookup here so that we don't
			// have to access doctor_percents for every drug in the sale.
			// DonR 05Sep2016: Changed SELECT DISTINCT to SELECT FIRST 1 to avoid a cardinality violation.
			if ((v_TypeDoctorIdentification	== 1) && (DocID > 0))
			{
				ExecSQL (	MAIN_DB, READ_DoctorByLicenseNumber,
							&v_DoctorFirstName,		&v_DoctorFamilyName,
							&DummyPhone,			&DocChkInteractions,
							&DummyRealDoctorTZ,
							&DocID,					END_OF_ARG_LIST			);

				// If we didn't get a "hit", blank out the doctor-name variables.
				if (SQLERR_code_cmp (SQLERR_ok) != MAC_TRUE)
				{
					strcpy (v_DoctorFamilyName,	"");
					strcpy (v_DoctorFirstName,	"");

					if (SQLERR_code_cmp (SQLERR_not_found) != MAC_TRUE)
					{
						if (SQLERR_error_test ())
						{
							SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR);
							break;
						}
					}	// Something other than not-found.
				}	// Something other than successful read.

				else
				{
					// We managed to read the doctor, so copy her interaction-check flag to the SPres structure.
					for (i = 0; i < v_NumOfDrugLinesRecs; i++)
					{
						SPres[i].DocChkInteractions = DocChkInteractions;
					}
				}	// Successfully read the doctors row.
			}	// Non-zero Doctor License Number needs to be looked up.

		}
		while (0);
		// End of Big Dummy Loop #1.

		// DonR 27Apr2005: No need to restore "normal" database isolation here.


		// DUMMY LOOP #2 TO PREVENT goto.

		// DonR 21Apr2005: Avoid unnecessary lock errors by downgrading "isolation"
		// for this section. (Note that it should already be downgraded!)
		SET_ISOLATION_DIRTY;


		do	// Exiting from LOOP writes to database and sends reply.
		{
			// Drugs validation.
			// DonR 09Jun2010: Split the previous loop into two loops. The first one reads
			// all drugs from the drug_list table (using the new function read_drug)
			// and does some minimal processing of the data read; the second one deals with
			// Drug Purchase Limits, etc. This eliminates the ugly "read ahead" logic I added
			// on 06Jun2010, since now by the time we look at limits, all the drug_list data
			// for the current sale is already available.
			for (i = 0; i < v_NumOfDrugLinesRecs; i++)
			{
				v_DrugCode = SPres[i].DrugCode;

				// DonR 05Mar2017: Set default Prescription Source Reject flag (based on values
				// in the prescr_source table).
				SPres[i].PurchaseLimitSourceReject = DefaultSourceReject;

				// DonR 25Jul2013: If we found a vacation ishur in special_prescs, copy its ishur number into all drugs.
				if (Member.Has_PL_99997_Ishur)
					SPres[i].vacation_ishur_num = Member.PL_99997_IshurNum;

				// DonR 11Jul2011: Per Iris Shaya, check for duplicate drugs in a sale request.
				for (j = 0; j < i; j++)
				{
					if (SPres[j].DrugCode == v_DrugCode)
					{
						if (SetErrorVarArr (&SPres[j].DrugAnswerCode, ERR_DUPLICATE_DRUG_IN_SALE, v_DrugCode, j + 1, NULL, &ErrOverflow))
						{
							SetErrorVarArr (&v_ErrorCode, ERR_DRUG_S_WRONG_IN_PRESC, 0, 0, NULL, &ErrOverflow);
							SPres[i].DFatalErr = SPres[j].DFatalErr = MAC_TRUE;
							break;	// Out of the "for j..." loop - we still try to read the drug from drug_list.
						}
					}
				}
				// DonR 11Jul2011 end.

				if (!read_drug (v_DrugCode,
								v_MaxLargoVisible,
								&Phrm_info,
								(v_ActionType == SALE_DELETION),	// Deleted drugs are "visible" if we're deleting prior sales.
								NULL,
								&SPres[i]))
				{
					Conflict_Test (reStart);

					if (SQLERR_code_cmp (SQLERR_not_found) == MAC_TRUE)
					{
						// DonR 27Feb2008: Provide some defaults when drug isn't read.
						// DonR 11Jul2011: Although it shouldn't make any difference, moved the defaults outside
						// the "if drug-not-found is a fatal error" block - just in case someone decides to make
						// it a non-fatal error.
						SPres[i].BasePartCode		= 1;
						SPres[i].RetPartCode		= 1;
						SPres[i].part_for_discount	= 1;
						SPres[i].in_health_pack		= 0;

						if (SetErrorVarArr (&SPres[i].DrugAnswerCode, ERR_DRUG_CODE_NOT_FOUND, v_DrugCode, i + 1, &SPres[i].DFatalErr, &ErrOverflow))
						{
							SetErrorVarArr (&v_ErrorCode, ERR_DRUG_S_WRONG_IN_PRESC, 0, 0, NULL, &ErrOverflow);
							continue;
						}
					}	// Drug not found.
					else
					{
						if (SQLERR_error_test ())
						{
							SetErrorVarArr (&v_ErrorCode, ERR_DATABASE_ERROR, 0, 0, NULL, &ErrOverflow);
							break;
						}
					}
				}	// read_drug() returned zero (i.e. something went wrong).

				// If we get here, we have successfully read the drug_list row.

				// DonR 09Oct2016: If this is an "emergency supply" sale, hypnotics and narcotics are not allowed.
				if ((v_RecipeSource	== RECIP_SRC_EMERGENCY_NO_RX)	&&
					(v_ActionType	== DRUG_SALE)					&&
					((SPres[i].DL.drug_type == 'H') || (SPres[i].DL.drug_type == 'N')))
				{
					if (SetErrorVarArr (&SPres[i].DrugAnswerCode, NARCOTICS_HYPNOTICS_RX_ONLY_ERR, v_DrugCode, i + 1, &SPres[i].DFatalErr, &ErrOverflow))
					{
						SetErrorVarArr (&v_ErrorCode, ERR_DRUG_S_WRONG_IN_PRESC, 0, 0, NULL, &ErrOverflow);
						continue;
					}
				}

				// DonR 17Oct2021 User Story #196891: Check whether this drug is blocked for this user.
				// DonR 13Mar2025 User Story #384811: To support more detailed drug-blocking based on
				// different categories of Darkonaim (Maccabi, Harel Tourists, Harel Foreign Workers),
				// add Darkonai Type (3 times) to the READ_CheckDrugBlockedForMember parameter list.
				if (Member.has_blocked_drugs)	// Don't waste a DB lookup if we know we won't get results.
				{
					ExecSQL (	MAIN_DB, READ_CheckDrugBlockedForMember,
								&RowsFound,
								&Member.ID,
								&Member.darkonai_type,		&Member.darkonai_type,	&Member.darkonai_type,
								&SPres[i].DL.largo_code,	&Member.ID_code,		END_OF_ARG_LIST			);

					if ((SQLCODE == 0) && (RowsFound > 0))
					{
						// According to Iris, a single version of the error code is enough - we don't need
						// different error text for Maccabi versus private pharmacies.
						if (SetErrorVarArr (&SPres[i].DrugAnswerCode,
											MACCABI_PHARMACY ? LARGO_CODE_BLOCKED_FOR_MEMBER_MAC : LARGO_CODE_BLOCKED_FOR_MEMBER_PVT,
											v_DrugCode, i + 1, &SPres[i].DFatalErr, &ErrOverflow))
						{
							SetErrorVarArr (&v_ErrorCode, ERR_DRUG_S_WRONG_IN_PRESC, 0, 0, NULL, &ErrOverflow);
							continue;
						}
					}
					else
					{
						if (SQLERR_code_cmp (SQLERR_not_found) != MAC_TRUE)
						{
							SQLERR_error_test ();
						}
					}
				}

				// DonR 27Jan2016: Set a new flag if at least one drug in this sale request is a "treatment" type.
				// Note that we make this check before performing generic-drug substitution; it shouldn't make
				// any difference, since substitute drugs should always have the same Largo Type as their equivalents.
				if ((SPres[i].DL.largo_type	== 'B')		||
					(SPres[i].DL.largo_type	== 'M')		||
					(SPres[i].DL.largo_type	== 'T'))
				{
					ReqIncludesTreatment = 1;
				}

				// DonR 11Jul2010: Calling routine is now responsible for copying health-basket parameter
				// from the DL structure to the sPres structure. This should NOT be done for deletions,
				// since in that case we've already read the computed health-basket parameter from the
				// original sale.
				if (v_ActionType != SALE_DELETION)
				{
					SPres[i].in_health_pack		= SPres[i].DL.health_basket_new;

					// DonR 11Sep2012: Send pharmacy a warning message if the drug being sold is non-chronic
					// and the prescription date is too far in the future (based on the sale_ok_early_days
					// parameter in sysparams) OR the Duration parameter is greater than 45 days.
					// Note that the logic below allows for the possibility that this warning can later be
					// "upgraded" into a fatal error.
					// DonR 16Sep2012: Don't look at Chronic Flag; Chronic Months must be >= 3 for a
					// drug to be considered chronic.
					// DonR 07Jul2013: New "Bakara Kamutit" logic includes new controls on future sales.
					// Duration is no longer relevant.
					if ((v_ActionType == DRUG_SALE) && (PR_Date[i] > MaxFutureDate))
					{
						// Future sale can be permitted in several different ways; proceed from the most general
						// to the most specific. (Note that SPres[i].why_future_sale_ok has an initial value of zero.)
						// For now at least, future sales are permitted only at Maccabi pharmacies except in the case
						// of a vacation ishur - but this may change in the future for members who live overseas.
						// DonR 09Oct2018 CR #26700: Pharmacies are now authorized to sell future-dated prescriptions
						// based on a new flag, can_sell_future_rx, in the Pharmacy table.
						if (CAN_SELL_FUTURE_RX)
						{
							if ((SPres[i].DL.allow_future_sales == FUTURE_SALE_MAC_ONLY)	||
								(SPres[i].DL.allow_future_sales == FUTURE_SALE_ALL_PHARM))
							{
								SPres[i].why_future_sale_ok = (SPres[i].DL.allow_future_sales == FUTURE_SALE_ALL_PHARM) ?
																FUT_PERMITTED_ALL_PHARM : FUT_PERMITTED_MACCABI;
							}	// Future sale permitted by drug_list allow_future_sales flag.
							else
							{
								// Members who live overseas can fill future-dated prescriptions at Maccabi pharmacies.
								if (OVERSEAS_MEMBER)
									SPres[i].why_future_sale_ok = FUT_PERMITTED_OVRSEAS;
							}	// Future-dated sales of this drug are not normally permitted.
						}	// Sale at Maccabi pharmacy (or a private pharmacy with can_sell_future_rx set non-zero).

						// Vacation ishurim allow future sales at all pharmacies.
						if ((SPres[i].why_future_sale_ok == 0) && (Member.Has_PL_99997_Ishur))
							SPres[i].why_future_sale_ok = FUT_PERMITTED_ISHUR;

						if (SPres[i].why_future_sale_ok == 0)
						{
							if (SetErrorVarArr (&SPres[i].DrugAnswerCode, ERR_PR_DATE_IN_FUTURE, v_DrugCode, i + 1, &SPres[i].DFatalErr, &ErrOverflow))
							{
								SetErrorVarArr (&v_ErrorCode, ERR_DRUG_S_WRONG_IN_PRESC, 0, 0, NULL, &ErrOverflow);
							}	// Error is fatal.
						}	// Future sale is NOT approved!
					}	// Future drug sale.


					// DonR 21May2014: Add new warning message for Duration > 45 days.
					// DonR 16Jun2014: This warning doesn't apply if member has a vacation ishur.
					// DonR 09Oct2018 CR #26700: Pharmacies are now authorized to sell future-dated prescriptions
					// based on a new flag, can_sell_future_rx, in the Pharmacy table. It seems reasonable, then,
					// to send these pharmacies the same long-duration warning message that we send to Maccabi
					// pharmacies.
					if ((v_ActionType					== DRUG_SALE)						&&
						(v_RecipeSource					!= RECIP_SRC_NO_PRESC)				&&
						(SPres[i].Duration				>  45)								&&
						(SPres[i].DL.allow_future_sales	== FUTURE_SALE_FORBIDDEN)			&&
						(CAN_SELL_FUTURE_RX)												&&
						(!Member.Has_PL_99997_Ishur)										&&
						(!(OVERSEAS_MEMBER))												&&
						(!(SetErrorVarExists (ERR_PR_DATE_IN_FUTURE, SPres[i].DrugCode))))
					{
						if (SetErrorVarArr (&SPres[i].DrugAnswerCode, LONG_DURATION_WARNING, v_DrugCode, i + 1, &SPres[i].DFatalErr, &ErrOverflow))
						{
							SetErrorVarArr (&v_ErrorCode, ERR_DRUG_S_WRONG_IN_PRESC, 0, 0, NULL, &ErrOverflow);
						}
					}	// Applicable long-duration drug sale.

				}	// Current operation is not a deletion.


				// Compute ingredient usage for Ishur Limit checking - and possibly, eventually,
				// for ingredient-based Overdose/Interaction checking.
				for (ingred = 0; ingred < 3; ingred++)
				{
					if ((SPres[i].DL.Ingredient[ingred].code		<  1)	||
						(SPres[i].DL.Ingredient[ingred].quantity	<= 0.0)	||
						(SPres[i].DL.Ingredient[ingred].per_quant	<= 0.0)	||
						(SPres[i].DL.package_size					<  1))
					{
						// Invalid values - set this slot to zero.
						SPres[i].DL.Ingredient	[ingred].code		= 0;
						SPres[i].DL.Ingredient	[ingred].quantity	= 0.0;
						SPres[i].TotIngrQuantity[ingred]			= 0.0;
						SPres[i].IngrQuantityStd[ingred]			= 0.0;
					}

					else
					{
						if (SPres[i].DL.package_size == 1)
						{
							// Syrups and similar drugs.
							SPres[i].TotIngrQuantity[ingred] =	  (double)SPres[i].Op
																* SPres[i].DL.package_volume
																* SPres[i].DL.Ingredient[ingred].quantity
																/ SPres[i].DL.Ingredient[ingred].per_quant;
							SPres[i].IngrQuantityStd[ingred] =	  (double)SPres[i].Op
																* SPres[i].DL.package_volume
																* SPres[i].DL.Ingredient[ingred].quantity_std
																/ SPres[i].DL.Ingredient[ingred].per_quant;
						}

						else	// Package size > 1.
						{
							// Tablets, ampules, and similar drugs.
							// For these medications, ingredient is always given per unit.
							UnitsSold	=	  (SPres[i].Op		* SPres[i].DL.package_size)
											+  SPres[i].Units;

							SPres[i].TotIngrQuantity[ingred] =   SPres[i].DL.Ingredient[ingred].quantity
															   * (double)UnitsSold;
							SPres[i].IngrQuantityStd[ingred] =   SPres[i].DL.Ingredient[ingred].quantity_std
															   * (double)UnitsSold;
						}
					}	// Valid quantity values available.
				}	// Loop through ingredients for this drug.

				// Set flag for member if at least one item to be bought is "Mazon im Tikra".
				if (SPres[i].DL.tikrat_mazon_flag > 0)
					MemberBuyingTikMazon = 1;

		    }	// Drug reading/preliminary processing loop.


		    if ((reStart == MAC_TRUE) || (v_ErrorCode == ERR_DATABASE_ERROR))
		    {
				break;
			}


			// DonR 09Jun2010: Second drug loop, to handle Drug Purchase Limits and other processing.
			// This loop (along with the next ones, dealing with supplements and participation) is
			// relevant only for drug sales! For deletions, we skip approximately 1,900 lines of code
			// here!
			if (v_ActionType != SALE_DELETION)
			{
				for (i = 0; i < v_NumOfDrugLinesRecs; i++)
				{
					v_DrugCode = SPres[i].DrugCode;

					// DonR 12Dec2006: Check drug against applicable member-pharmacy restrictions.
					// We're interested here only in Hypnotics and Narcotics.
					if ((v_MemPharm_Exists) && (v_MemPharm_ResType == 1))
					{
						if ((SPres[i].DL.drug_type == 'H') || (SPres[i].DL.drug_type == 'N'))
						{
							// Remember that the "error code" may be just telling the pharmacy
							// that the sale is permitted!
							SetErrorVarArr (&SPres[i].DrugAnswerCode, MemPharm_Result, v_DrugCode, i + 1, &SPres[i].DFatalErr, &ErrOverflow);
						}
					}


					// DonR 29Jun2006: If this drug has a Drug Purchase Limit, attempt to read
					// the relevant fields from the drug_purchase_lim table and store them.
					if (SPres[i].DL.purchase_limit_flg > 0)
					{
						// DonR 26May2010 "Stickim": read previously-purchased drugs now, so we'll
						// be able to categorize member for new drug-purchase-limit logic based on
						// his/her previous drug purchases.
						if (get_drugs_in_blood (&Member,
												&v_DrugsBuf,
												GET_ALL_DRUGS_BOUGHT,
												&v_ErrorCode,
												&reStart)				== 1)
						{
							break;
						}	// high severity error OR deadlock occurred


						// DonR 01Dec2016: New function ReadDrugPurchaseLimitData() replaces separate code in Transactions
						// 2003, 5003, and 6003, and adds new functionality to support ingredient-based purchase limits.
						FunctionError = ReadDrugPurchaseLimitData (&Member, SPres, i, v_NumOfDrugLinesRecs, &v_DrugsBuf, NULL, &v_ErrorCode, &reStart);
					}	// Drug has a Drug Purchase Limit.


					// Logical tests on drug.
					// 1. Prescription drug sold w/o prescription.
					if ((v_RecipeSource					== RECIP_SRC_NO_PRESC) &&
						(SPres[i].DL.no_presc_sale_flag	== MAC_FALS))
					{
						if (SetErrorVarArr (&SPres[i].DrugAnswerCode, ERR_DRUG_REQUIRE_PRESCRIPTION, SPres[i].DrugCode, i + 1, &SPres[i].DFatalErr, &ErrOverflow))
						{
							SetErrorVarArr (&v_ErrorCode, ERR_DRUG_S_WRONG_IN_PRESC, 0, 0, NULL, &ErrOverflow);
//							continue;
						}
					}	// Drug Test 1 - presc. drug sold w/o prescription.

					// 2. DonR 09Jun2005: If this is a non-prescription purchase from a private pharmacy,
					// either the drug must have its MaccabiCare flag set true (meaning it's a
					// Maccabi-branded OTC drug) OR the pharmacy must have sent a Maccabi Sale Number
					// (which is sent and stored in the Doctor Prescription ID field). If neither of
					// these conditions is met, the pharmacy should never have sent us this drug-line.
					if ((v_RecipeSource					== RECIP_SRC_NO_PRESC)	&&
						(PRIVATE_PHARMACY)										&&
						(SPres[i].DL.maccabicare_flag	== 0)					&&
						(PR_DoctorPrID[i]				== 0))
					{
						if (SetErrorVarArr (&SPres[i].DrugAnswerCode, ERR_NOT_MACCABICARE_DRUG, SPres[i].DrugCode, i + 1, &SPres[i].DFatalErr, &ErrOverflow))
						{
							SetErrorVarArr (&v_ErrorCode, ERR_DRUG_S_WRONG_IN_PRESC, 0, 0, NULL, &ErrOverflow);
//							continue;
						}
					}


					// DonR 03Feb2004: Use separate function to find preferred (generic) drug.
					// DonR 14APR2003: Per Iris Shaya - if a non-preferred drug is being sold for
					// a reason (e.g. "dispense as written" or pharmacy is out of preferred drug),
					// member participates as if s/he were being given the preferred drug.
					// DonR 13Jan2005: Moved this block of code down to after drug_list is
					// read, so that we know beforehand whether the drug is in economypri.
					// Also, note that find_preferred_drug now looks up the Member Price Code
					// of the preferred drug, if any.
					find_preferred_drug (SPres[i].DrugCode,
										 SPres[i].DL.economypri_group,
										 SPres[i].DL.preferred_flg,
										 SPres[i].DL.preferred_largo,
										 0,	// No need to check purchase history for alternate preferred drugs.
										 0,	// Allow drugs with Preferred Status = 3 to be sold without requiring an explanation from pharmacy.
										 DocID,
										 v_MemberIdentification,
										 v_IdentificationCode,
										 &EP_LargoCode,
										 &PR_AsIf_Preferred_SpDrug	[i],
										 &PR_AsIf_ParentGroupCode	[i],
										 &preferred_mbr_prc_code	[i],
										 &DispenseAsWritten,
										 &PR_AsIf_Preferred_Basket	[i],
										 &PR_AsIf_RuleStatus		[i],
										 NULL,	// DL structure pointer
										 &FunctionError);

					SetErrorVarArr (&v_ErrorCode, FunctionError, 0, 0, NULL, &ErrOverflow);

					// Check if the drug to be sold is the one Maccabi wants to sell. If not, send
					// a warning message to the pharmacy.
					if (EP_LargoCode != SPres[i].DrugCode)
					{
						// If the pharmacist has a reason for selling a drug other
						// than the "preferred" one, store the preferred drug's Largo Code
						// for later use in checking member's participation level. Otherwise,
						// return a "Not Preferred Drug" warning code.
						//
						// DonR 28Mar2010: Changed handling of reason sent by pharmacy; instead of
						// excluding NO_REASON_GIVEN and DISPENSED_KACHA (= 0 and 99), now we
						// *include* only OUT_OF_STOCK_SUB_EQUIV and EXCESS_STOCK (= 2 and 3).
						// DOCTOR_NIXED_SUBSTITUTION (= 1) and any other strange value will *not*
						// allow sale of non-preferred drugs at preferred prices.
						if ((PR_WhyNonPreferredSold[i] == OUT_OF_STOCK_SUB_EQUIV)	||
							(PR_WhyNonPreferredSold[i] == EXCESS_STOCK			)	||
							(DispenseAsWritten))
						{
							// Non-substitution is legitimate.
							PR_AsIf_Preferred_Largo[i] = EP_LargoCode;

							// DonR 14Jan2009: Give the AS/400 a record of why we gave "as-if-preferred"
							// participation.
							// DonR 28Mar2010: Give a separate reason-code if participation was granted
							// based on a "tofes" that expired within the last 91 days.
							// DonR 20Dec2011: If a private pharmacy is selling a non-generic medicine because
							// the generic version is out of stock, send a warning message. Note that if the
							// doctor has nixed substitution anyway, there's no need to send this message - so
							// we execute this logic only for DispenseAsWritten == 0.
							switch (DispenseAsWritten)
							{
								case 0:		SPres[i].WhyNonPreferredSold = PHARM_AUTH_NON_GENERIC;

											if ((PRIVATE_PHARMACY)											&&
												(v_RecipeSource				!= RECIP_SRC_NO_PRESC		)	&&
												(PR_WhyNonPreferredSold[i]	== OUT_OF_STOCK_SUB_EQUIV	))
											{
												SetErrorVarArr (&SPres[i].DrugAnswerCode,
																ERR_OUT_OF_STOCK_WARN,
																SPres[i].DrugCode, i + 1, 
																NULL,
																&ErrOverflow);
											}
											break;

								case 1:		SPres[i].WhyNonPreferredSold = DOC_AUTH_NON_GENERIC;
											break;

								case 2:		SPres[i].WhyNonPreferredSold = DOC_AUTH_EXPIRED_TOFES;
											break;

								// Default condition should never really happen - but just in case...!
								default:	SPres[i].WhyNonPreferredSold = PHARM_AUTH_NON_GENERIC;	break;
							}
						}
						else
						{
							PR_AsIf_Preferred_Largo [i] = 0;	// Disable preferred-drug swapping - redundant, but paranoia is good!

							// If we're not going to use an "as-if-preferred" drug, zero
							// out the "as-if-preferred in-health-basket" parameter.
							PR_AsIf_Preferred_Basket[i] = 0;

							// DonR 14Jan2009: Give the AS/400 a record of the fact that a non-preferred
							// drug was sold without using "as-if-preferred" conditions.
							SPres[i].WhyNonPreferredSold = NON_GENERIC_SOLD_AS_IS;

							// 22Dec2003: Per Iris Shaya, non-preferred drug warning is given only if the
							// non-preferred drug is being dispensed without any indication that the
							// pharmacist is aware of the problem. If the pharmacist gives the Reason Code
							// 99 ("kacha"), no "acceptable" reason for the substitution is being offered,
							// but the pharmacist is already aware of the situation and doesn't need a message.
							// DonR 08Mar2006: Per Iris Shaya, if the drug's preferred_flg is 2, don't give
							// a warning.
							// DonR 28Mar2010: If pharmacy sent reason 1 (DOCTOR_NIXED_SUBSTITUTION) but
							// there is no valid doctor's ishur for dispensing the prescription as written,
							// give pharmacy a new, severe error message telling them so.
							// DonR 23Mar2015: Don't give an error if the drug being sold is, in fact, preferred.
							// This can happen when member bought Preferred Drug X last time but (for whatever
							// reason) is buying Preferred Drug Y this time.
							if (((PR_WhyNonPreferredSold[i]	== NO_REASON_GIVEN)				||
								 (PR_WhyNonPreferredSold[i]	== DOCTOR_NIXED_SUBSTITUTION))		&&
								(SPres[i].DL.preferred_flg	!= 1)								&&	// DonR 23Mar2015.
								(SPres[i].DL.preferred_flg	!= 2)								&&
								(v_RecipeSource				!= RECIP_SRC_NO_PRESC))
							{
								// DonR 09May2010 HOT FIX: Use new non-critical error code so that we
								// don't give up on this drug yet; it's possible member has an AS/400
								// ishur for the non-generic drug, in which case we'll use that and
								// suppress the non-critical error. If there isn't an AS/400 ishur
								// applicable, we'll replace the non-critical error with the "real"
								// critical error later on.
								SetErrorVarArr (&SPres[i].DrugAnswerCode,
												(PR_WhyNonPreferredSold[i] == DOCTOR_NIXED_SUBSTITUTION) ?
														ERR_NO_DOCTORS_ISHUR_WARN :
														ERR_NOT_PREFERRED_DRUG_WARNING,
												SPres[i].DrugCode, i + 1, 
												NULL,
												&ErrOverflow);
							}

						}	// No reason given for dispensing non-preferred drug.
					}	// Drug being sold isn't the preferred one.

					else
					// No preferred drug found - zero appropriate variables.
					{
						preferred_mbr_prc_code  [i]		= 0;
						PR_AsIf_Preferred_SpDrug[i]		= 0;
						PR_AsIf_Preferred_Basket[i]		= 0;
						PR_AsIf_ParentGroupCode	[i]		= 0;
						PR_AsIf_RuleStatus		[i]		= 0;
						PR_AsIf_Preferred_Largo [i]		= 0;	// Disable preferred-drug swapping.
						SPres[i].WhyNonPreferredSold	= 0;	// DonR 14Jan2009: shouldn't really be necessary.
					}
					// DonR 13Jan2005 end.


					// Set up pricing defaults.
					SPres[i].ret_part_source.table			= FROM_DRUG_LIST_TBL;
					SPres[i].ret_part_source.insurance_used	= BASIC_INS_USED;

					// Previous Member Price Code is not used for Electronic Prescriptions!
					// DonR 16May2006: Kishon Divers (who are supposed to get their medications
					// direct from the government) pay 100% unconditionally.
					// DonR 09Aug2021 User Story #163882: Treat darkonaim-plus people who always
					// pay 100% (minus shovarim they get from their insurance company) the same
					// as Kishon divers.
					if ((v_IdentificationCode == 3)		||	// Kishon divers.
						(Member.force_100_percent_ptn))		// Darkonaim-plus not getting any discounts.
					{
						SPres[i].BasePartCode				= PARTI_CODE_ONE;
						SPres[i].RetPartCode				= PARTI_CODE_ONE;
						SPres[i].part_for_discount			= PARTI_CODE_ONE;
					}
					else	// Normal members.
					{
						SPres[i].BasePartCode				= SPres[i].DL.member_price_code;
						SPres[i].RetPartCode				= SPres[i].DL.member_price_code;
						SPres[i].part_for_discount			= SPres[i].DL.member_price_code;
					}

					SPres[i].BasePartCodeFrom			= CURR_CODE;
					SPres[i].ret_part_source.state		= CURR_CODE;

				}	// Loop through drugs.


				if ((reStart == MAC_TRUE) || (v_ErrorCode == ERR_DATABASE_ERROR))
				{
					break;
				}


				// DonR 06Dec2010: Adding new "preparation" consistency test. Preparations should be handled
				// by a single Transaction 5003, and every item in the sale must be part of the preparation.
				// Keep track of the LinkDrugToAddition parameter for the first drug; everything else in
				// the list should agree with it. If the first drug is part of a preparation, everything
				// else should be as well; and if not, nothing else should be. If the Link field is being
				// used for a Gadget Code, treat it as zero for this purpose.
				PreparationLinkLargo = (SPres[0].DL.in_gadget_table > 0) ? 0 : SPres[0].LinkDrugToAddition;

				// DonR 24Dec2013: Additional "preparation" consistency checks. If this sale is a preparation,
				// there *must* be a non-zero Chemical Price; and if there is a non-zero Chemical Price the
				// sale *must* be a "preparation". Note (A) that negative Chemical Price is allowed, and
				// (B) we already check consistency of other items in the same sale. Also, if this is a
				// "preparation" sale, there *must* be more than one item in the sale. (Otherwise there isn't
				// anything to "prepare", after all...
				if (((PreparationLinkLargo != 0) && (ChemicalPrice == 0))			||
					((PreparationLinkLargo == 0) && (ChemicalPrice != 0))			||
					((PreparationLinkLargo != 0) && (v_NumOfDrugLinesRecs < 2)))
				{
					SetErrorVarArr (&v_ErrorCode, ERR_PREPARATION_INCONSISTENT, 0, 0, NULL, &ErrOverflow);
				}

				// Drugs SUPPLEMENTs test.
				for (i = 0; i < v_NumOfDrugLinesRecs; i++)
				{
					// DonR 06Dec2010: New "preparation" consistency test. Compare the link for each drug
					// against the link for the first drug; any inconsistency means the whole transaction
					// is invalid.
					TestLinkLargo = (SPres[i].DL.in_gadget_table > 0) ? 0 : SPres[i].LinkDrugToAddition;
					if (TestLinkLargo != PreparationLinkLargo)
					{
						SetErrorVarArr (&v_ErrorCode, ERR_PREPARATION_INCONSISTENT, 0, 0, NULL, &ErrOverflow);
					}

					// Skip this drug if one of the following is true:
					// 1. There's no supplement link - meaning that drug[i] isn't a supplement.
					// 2. It's a Package or Preparation type drug.
					// 3. We've already hit a severe error for this drug.

					// DonR 22Dec2003: Per Iris Shaya, bottles (Largo Type 'A') should be treated like other
					// parts of the preparation - that is, they get the same Participation Code as the
					// Preparation (Largo Type 'X', the 'major drug') itself.
					// DonR 23Aug2005: For "gadgets", LinkDrugToAddition is used optionally to indicate
					// the "gadget code" - e.g. in order to differentiate between knee braces sold for
					// use on the right knee as opposed to identical braces sold for the left knee.
					// Thus, if the "drug" is a gadget, skip the drug-supplement stuff.
					if ((SPres[i].LinkDrugToAddition	== 0)			||
						(SPres[i].DL.in_gadget_table	>  0)			||
						(SPres[i].DL.largo_type			== 'X')			||
						(SPres[i].DFatalErr				== MAC_TRUE))
					{
						continue;
					}

					flg = MAC_FALS;		// Major drug found flag.

					// Loop through the other drugs in the prescription, searching for the major
					// drug to which drug[i] is linked.
					for (is = 0; is < v_NumOfDrugLinesRecs; is++)
					{
						// Skip past drug[i] - we don't want to compare it to itself!
						if (is == i)
							continue;

						// If this drug isn't the one that drug[i] links to, skip it.
						if (SPres[is].DrugCode != SPres[i].LinkDrugToAddition)
							continue;

						flg = MAC_TRUE;	// Major drug found for supplemental drug[i].
						MajorDrugIndex[i] = is;

						// If the major drug is a Preparation (type 'X'), skip
						// the linkage and quantity tests.
						if (SPres[is].DL.largo_type != 'X')
						{
							// If drug[i] isn't recognized as a supplement to drug[is], set error.
							// DonR 10Jun2014: Per Iris Shaya, give an error only if the major drug's
							// type is neither X nor C; also, change the error given from 6013 to 6057.
							if ((SPres[i].DrugCode != SPres[is].DL.supplemental_1) &&
								(SPres[i].DrugCode != SPres[is].DL.supplemental_2) &&
								(SPres[i].DrugCode != SPres[is].DL.supplemental_3) &&
								(SPres[is].DL.largo_type != 'C'))
							{
								if (SetErrorVarArr (&SPres[i].DrugAnswerCode, ERR_PREPARATION_INCONSISTENT,
													SPres[i].DrugCode, i + 1, &SPres[i].DFatalErr, &ErrOverflow))
								{
									SetErrorVarArr (&v_ErrorCode, ERR_DRUG_S_WRONG_IN_PRESC, 0, 0, NULL, &ErrOverflow);
								}
							}	// Drug[i] isn't valid supplement to drug[is].

							else
							{
								// The supplement is the right stuff. Now match quantity
								// of supplement to that of the major drug.
								if (SPres[i].Op > (((SPres[is].Units > 0) ? 1 : 0 ) + SPres[is].Op))
								{
									// More packages of supplement than of major drug.
									if (SetErrorVarArr (&SPres[i].DrugAnswerCode, ERR_SUPPL_OP_NOT_MATCH_DRUG_OP,
														SPres[i].DrugCode, i + 1, &SPres[i].DFatalErr, &ErrOverflow))
									{
										SetErrorVarArr (&v_ErrorCode, ERR_DRUG_S_WRONG_IN_PRESC, 0, 0, NULL, &ErrOverflow);
									}
								}	// Too much of drug[i] to supplement quantity of drug[is].
							}	// Drug[i] is a valid supplement to drug[is].
						}	// The major drug[is] is not a Preparation (type 'X').


						// We've found the major drug and performed our tests - so we can quit
						// the inner loop.
						break;
					}	// Loop on [is] to find Major drug for supplemental drug[i].


					// If no major drug was found for supplemental drug[i], set error.
					// DonR 24Dec2013: If the reason that the major drug wasn't found was that there is only
					// one drug in the sale, we're now returning error 6057 (Preparation Inconsistent) - so
					// we don't have to return additional errors.
					// DonR 09Jun2014: Per Iris Shaya, changed the error code for this test from 6013 to 6057.
					if ((flg == MAC_FALS) && (v_NumOfDrugLinesRecs > 1))
					{
						if (SetErrorVarArr (&SPres[i].DrugAnswerCode, ERR_PREPARATION_INCONSISTENT,
											SPres[i].DrugCode, i + 1, &SPres[i].DFatalErr, &ErrOverflow))
						{
							SetErrorVarArr (&v_ErrorCode, ERR_DRUG_S_WRONG_IN_PRESC, 0, 0, NULL, &ErrOverflow);
							break;
						}
					}
				}	// Loop through drugs to perform supplement-drug tests.


				// Get Drug prices and check for Special Prescriptions in the DB.
				// DonR 06Nov2011: By default, trigger that enables the "Member in hospital"
				// warning message for pharmacies is set FALSE.
				// DonR 25Jan2015: DO NOT skip price- and participation-type checking for drugs
				// that have triggered a fatal error.
				DrugPriceMemHospTrigger	= 0;
				for (i = 0; i < v_NumOfDrugLinesRecs; i++)
				{
//					// Skip drugs for which we've already discovered a major error.
//					if (SPres[i].DFatalErr == MAC_TRUE)
//						continue;
//
					v_DrugCode = SPres[i].DrugCode;


					ExecSQL (	MAIN_DB, READ_PriceList_simple,
								&Yarpa_Price,	&Macabi_Price,		&Supplier_Price,
								&v_DrugCode,	&v_PriceListCode,	END_OF_ARG_LIST		);

					Conflict_Test (reStart);

					if (SQLERR_code_cmp (SQLERR_not_found) == MAC_TRUE)
					{
						if (SetErrorVarArr (&SPres[i].DrugAnswerCode, ERR_DRUG_PRICE_NOT_FOUND, v_DrugCode, i + 1, &SPres[i].DFatalErr, &ErrOverflow))
						{
							SetErrorVarArr (&v_ErrorCode, ERR_DRUG_S_WRONG_IN_PRESC, 0, 0, NULL, &ErrOverflow);
							continue;
						}
					}
					else
					{
						if (SQLERR_error_test ())
						{
							SetErrorVarArr (&v_ErrorCode, ERR_DATABASE_ERROR, 0, 0, NULL, &ErrOverflow);
							break;
						}
						else
						{
							// DonR 06Nov2011: If at least one drug has a non-discounted per-OP price of
							// more than NIS 500.00, set trigger that enables the "Member in hospital" warning
							// message for pharmacies. Note that this check must be made *before* taking VAT
							// off the price for Eilat pharmacies!
							if (Yarpa_Price > 50000)
								DrugPriceMemHospTrigger	= 1;
						}
					}


					// Store drug price in SPres array.
					// NOTE: The following call to GET_PARTICIPATING_PERCENT is ONLY to ensure that
					// the Participation Type is one we know something about. The retrieved
					// participation percentage is not used!
					state = GET_PARTICIPATING_PERCENT (SPres[i].BasePartCode,	&percents);
					if (state != NO_ERROR)
					{
						if (SetErrorVarArr (&v_ErrorCode, state, 0, 0, NULL, &ErrOverflow))
						{
							break;
						}
					}

					
					// 25Mar2003: Per Iris, the logic for use of Maccabi Price is changing.
					// For normal participation calculation, we will always use the Yarpa
					// (i.e. regular) price; at the end, we'll use the Maccabi Price as
					// a fixed participation price if:
					//     A) A Maccabi Price exists;
					//     B) Participation is 100%; and
					//     C) No fixed participation has been set from other sources, such as
					//        Drug-Extension.
					SPres[i].RetOpDrugPrice		= Yarpa_Price;
					SPres[i].YarpaDrugPrice	    = Yarpa_Price;
					SPres[i].MacabiDrugPrice    = Macabi_Price;
					SPres[i].SupplierDrugPrice  = Supplier_Price;

					// DonR 23Aug2011: For VAT-exempt pharmacies (e.g. in Eilat), deduct VAT from prices.
					// For now, adjust only the Yarpa price; but in future we may need to do the same
					// for the Maccabi (discounted) price.
					// DonR 19Feb2012: Added a separate price variable for calculating member participation, since Eilat
					// pharmacies are still supposed to see the VAT-inclusive price as the per-OP price.
					if (vat_exempt != 0)
					{
						SPres[i].PriceForPtnCalc = (int)(((double)Yarpa_Price * no_vat_multiplier) + .5001);
					}
					else
					{
						SPres[i].PriceForPtnCalc = Yarpa_Price;
					}


					// Test Special Prescription.
					// 20020122: If a member has a special prescription, it is
					// written in the members table (column spec_prescs). For such
					// members, we must check for a constant amount to pay.
					// DonR 21Jul2008: Per Iris Shaya, we should check Special Prescriptions only if drugs are
					// being sold based on some form of prescription. Non-prescription sales are not covered
					// by Special Prescriptions.
					if ((v_MemberSpecPresc /* from members table */)	&&
						(v_RecipeSource != RECIP_SRC_NO_PRESC)			&&
						(v_RecipeSource != RECIP_SRC_EMERGENCY_NO_RX))
					{
						TypeCheckSpecPr = 1;
					}

					// All the stuff about special special-prescription numbers
					// shouldn't apply to Electronic Prescriptions.
					if (TypeCheckSpecPr > 0)
					{
						// DonR 04Sep2003: If there was a Pharmacy Ishur to use an expired AS400
						// Special Prescription (and such a Special Prescription was found), the
						// number of days to extend it (currently always 31) will be stored in
						// PR_ExtendAS400IshurDays[]. Otherwise the array will have a zero, and
						// things will work as normal.
						flg = test_special_prescription (NULL,//&v_DummySpecialConfirmationNum,
														 NULL,//&v_DummySpecialConfirmationNumSource,
														 &Member,
														 OVERSEAS_MEMBER,
														 v_TypeDoctorIdentification,
														 DocID,
														 &SPres[i],
														 &SPres[0],
														 v_NumOfDrugLinesRecs,
														 PR_AsIf_Preferred_Largo[i],		// DonR 23Feb2004
														 &FunctionError,
														 TypeCheckSpecPr,
														 v_PharmNum,
														 Phrm_info.pharmacy_permission,		// DonR 14Aug2003
														 SPres[i].BasePartCode,
														 PR_ExtendAS400IshurDays[i],
														 &ThisIshurHasTikra);

						SetErrorVarArr (&v_ErrorCode, FunctionError, 0, 0, NULL, &ErrOverflow);

						// DonR 28Mar2007: If function returned OK, check whether
						// an ishur was found that had its "Tikra" flag set. If
						// so, remember that at least one such ishur was found
						// for the sale, so we can advise the member to pay by
						// Credit Line.
						// DonR 04Jan2012: Moved this stuff up above the "break" statements,
						// to make sure that it's executed unconditionally.
						if (ThisIshurHasTikra)
						{
							AnIshurHasTikra = SPres[i].IshurWithTikra = 1;
						}

						// DonR 09May2010 HOT FIX: If there is an AS/400 ishur for this member/drug,
						// suppress errors involving sale of non-generic medication without a valid
						// reason. The AS/400 ishur is a valid reason!
						// DonR 11Jul2011: Changed method of deleting error code, in order to work
						// with the "new" error-array system.
						if (SPres[i].ret_part_source.table == FROM_SPEC_PRESCS_TBL)
						{
							SetErrorVarDelete (ERR_NO_DOCTORS_ISHUR_WARN,		SPres[i].DrugCode);
							SetErrorVarDelete (ERR_NOT_PREFERRED_DRUG_WARNING,	SPres[i].DrugCode);
						}
						// DonR 04Jan2012 end.

						if (flg == 1)		/* high severity error occurred	*/
						{
							break;
						}
						else
						{
							if (flg == 2)	/* deadlock; access conflict	*/
							{
								reStart = MAC_TRUE;
								break;
							}
						}	// Something other than 1 (severe error) from test_special_prescription().
					}	// Need to test Special Prescription.
				}	// Get Drug Prices / Check Special Prescriptions loop.

				// DonR 04Jan2012: Add a separate "mini" loop to swap the real no-doctor's-ishur
				// error for the temporary version - since there is the possibility that the previous
				// loop will be exited prematurely, leaving the temporary version in place.
				for (i = 0; i < v_NumOfDrugLinesRecs; i++)
				{
					// DonR 09May2010 HOT FIX: Replace fictitious warning message with the real, critical-error
					// version.
					// DonR 11Jul2011: Since we're using the "new" error-array system, use a special swapping
					// function instead of just changing DrugAnswerCode.
					SetErrorVarSwap (&SPres[i].DrugAnswerCode,
									 ERR_NO_DOCTORS_ISHUR_WARN,
									 ERR_NO_DOCTORS_ISHUR,
									 SPres[i].DrugCode,
									 NULL);
				}	// "Mini-loop" for error-code substitution.

				// If we've hit a major problem, break out of big DUMMY loop.
				if ((reStart == MAC_TRUE) || (v_ErrorCode == ERR_DATABASE_ERROR))
				{
					break;
				}


				// DonR 02Aug2011: If member is currently a hospital inpatient and is trying to buy
				// prescription medications, provide a warning message to pharmacy.
				// DonR 06Nov2011: This message is now contingent on their being at least one expensive
				// (price/OP >= NIS 500.00) item being bought; purchase at Maccabi pharmacy; and member
				// either currently in hospital OR absent without official discharge ("on vacation").
				if (((v_in_hospital				== IN_HOSPITAL) || (v_in_hospital == ABSENT_NOT_DISCHARGED))	&&
					(DrugPriceMemHospTrigger	!= 0)															&&
					(v_RecipeSource				!= RECIP_SRC_NO_PRESC)											&&
					(MACCABI_PHARMACY))
				{
					SetErrorVarArr (&v_ErrorCode, WARN_MEMBER_IN_HOSPITAL, 0, 0, NULL, &ErrOverflow);
				}


				// Check for interactions and overdoses.
				// DonR 27Jul2003: Made these tests a separate function, since the same 1000+ lines of code
				// are present in at least three transactions.
				// DonR 24Jan2018: Moved member validation inside test_interaction_and_overdose(), so we can
				// now execute it unconditionally.
				test_interaction_and_overdose (&Member,
												max_drug_date,
												v_PharmNum,
												&Phrm_info,
												v_InstituteCode,
												v_TerminalNum,
												v_RecipeIdentifier,
												SPres,
												v_NumOfDrugLinesRecs,
												v_SpecialConfNum,
												v_SpecialConfSource,
												1,	// DonR 23Mar2004 - Error Mode = "new style"
												1,	// DonR 31Aug2005: Electronic Prescription Flag.
												&DocRx[0],
												&v_MsgExistsFlg,
												&v_MsgIsUrgentFlg,
												&FunctionError,
												&reStart);

				SetErrorVarArr (&v_ErrorCode, FunctionError, 0, 0, NULL, &ErrOverflow);


				// Member participating percent calculation.
				// Set Participation Code according to Prescription Source.
				// DonR 03Aug2003: Moved this section up, because we need to know the calculated
				// participation type *before* calculating Member Discount.
				flg = 0;

				// DonR 16May2006: Kishon Divers (who are supposed to get their medications
				// direct from the government) pay 100% unconditionally. This is implemented
				// (sneakily) by disabling the for-loop: Kishon divers have Identification Code
				// of 3, which makes the test condition FALSE for all values of i.
				// DonR 09Aug2021 User Story #163882: Add darkonai-plus people who don't get
				// any discounts (other than shovarim from their insurance company) to the
				// criteria for disabling this loop.
				for (i = 0;
					((i < v_NumOfDrugLinesRecs) && (v_IdentificationCode != 3) && (!Member.force_100_percent_ptn));
					i++)
				{
					// DonR 19May2016 HOT FIX: If we're getting *any* kind of participation from an AS/400 ishur,
					// don't do any further participation checking.
					if (SPres[i].ret_part_source.table == FROM_SPEC_PRESCS_TBL)
						continue;

					// DonR 14Apr2003: If we are treating a non-preferred drug as if it
					// were the preferred drug (for purposes of determining member participation),
					// swap in the preferred drug for checking participation levels.
					// DonR 04Dec2003: Use the preferred largo only if the participation for the
					// dispensed largo would be 100% with no fixed price.
					// DonR 09Jan2012: Replaced standard full-price conditions with macro FULL_PRICE_SALE.
					// DonR 17May2016: Per CR #4720, we now want to compute participation using the
					// generic "as if" drug first, and only if nothing better than 100% is found
					// will we fall back to the non-generic medication being sold.
					// This change is irrelevant except in situations where an "as if" drug exists - 
					// that is, a non-generic medication is being sold either because of a pharmacy
					// stock issue or because the doctor requested "dispense as written". Only
					// then will PR_AsIf_Preferred_Largo[i] have a non-zero value.
//					if ((PR_AsIf_Preferred_Largo[i]	> 0) && (FULL_PRICE_SALE(i)))
					if (PR_AsIf_Preferred_Largo[i] > 0)
					{
						// Switch pricing variables to preferred drug.
						SPres[i].BasePartCode				= preferred_mbr_prc_code[i];
						SPres[i].RetPartCode				= preferred_mbr_prc_code[i];
						SPres[i].part_for_discount			= preferred_mbr_prc_code[i];
						SPres[i].BasePartCodeFrom			= CURR_CODE;
						SPres[i].ret_part_source.state		= CURR_CODE;

						// Redo the participation-determination stuff with the preferred Largo Code.
						HoldLargo			= SPres[i].DrugCode;
						HoldSpecialistDrug	= SPres[i].DL.specialist_drug;
						HoldParentGroupCode	= SPres[i].DL.parent_group_code;
						HoldRuleStatus		= SPres[i].DL.rule_status;
						SPres[i].DrugCode				= PR_AsIf_Preferred_Largo	[i];
						SPres[i].DL.specialist_drug		= PR_AsIf_Preferred_SpDrug	[i];
						SPres[i].DL.parent_group_code	= PR_AsIf_ParentGroupCode	[i];
						SPres[i].DL.rule_status			= PR_AsIf_RuleStatus		[i];
						SPres[i].in_health_pack			= PR_AsIf_Preferred_Basket	[i];

						switch (v_RecipeSource)
						{
							case RECIP_SRC_DENTIST:

									flg = test_dentist_drugs_electronic (&SPres[i],
																		 &FunctionError);

									SetErrorVarArr (&v_ErrorCode, FunctionError, 0, 0, NULL, &ErrOverflow);
									break;


							// DonR 23Feb2008: Added new Prescription Source 11, for
							// "Home Visit" prescriptions.
							case RECIP_SRC_HOME_VISIT:

									flg = test_home_visit_drugs_electronic (&SPres[i],
																			&FunctionError);

									SetErrorVarArr (&v_ErrorCode, FunctionError, 0, 0, NULL, &ErrOverflow);
									break;

							default:							// DonR 29Aug2016 CR #8956.

									flg = test_mac_doctor_drugs_electronic (v_TypeDoctorIdentification,
																			DocID,
																			&SPres[i],
																			i + 1,
																			Member.age_months,
																			v_MemberGender,
																			Phrm_info.pharmacy_permission,
																			v_RecipeSource,
																			v_MemberIdentification,
																			v_IdentificationCode,
																			(v_ClientLocationCode > 1),
																			&PharmIshurProfession[i],
																			&FunctionError);

									SetErrorVarArr (&v_ErrorCode, FunctionError, 0, 0, NULL, &ErrOverflow);
									break;

						}	// End of switch on prescription source.


						// Swap back the drug code actually being sold.
						// DonR 15Jan2013 WORKINGPOINT: Should we also swap in the original Health Basket status
						// if participation is still 100%?
						SPres[i].DrugCode				= HoldLargo;
						SPres[i].DL.specialist_drug		= HoldSpecialistDrug;
						SPres[i].DL.parent_group_code	= HoldParentGroupCode;
						SPres[i].DL.rule_status			= HoldRuleStatus;
					}	// Need to compute participation with "as if" preferred Largo Code.


					// DonR 17May2016: Per CR #4720, the logic for the drug being sold now comes *after* the
					// logic for the "as if" generic drug. This change is irrelevant except in situations where
					// an "as if" drug exists - that is, a non-generic medication is being sold either because
					// of a pharmacy stock issue or because the doctor requested "dispense as written". Only
					// then will PR_AsIf_Preferred_Largo[i] have a non-zero value.
					if ((PR_AsIf_Preferred_Largo[i]	== 0) || (FULL_PRICE_SALE(i)))
					{
						// DonR 22Mar2015: If we *didn't* get participation for the preferred generic
						// drug, zero out the "tens" digit of the why-non-preferred-drug-was-sold flag.
						SPres[i].WhyNonPreferredSold	= 0;

						switch (v_RecipeSource)
						{
							case RECIP_SRC_DENTIST:

									flg = test_dentist_drugs_electronic (&SPres[i],
																		 &FunctionError);

									SetErrorVarArr (&v_ErrorCode, FunctionError, 0, 0, NULL, &ErrOverflow);
									break;


							// DonR 23Feb2008: Added new Prescription Source 11, for
							// "Home Visit" prescriptions.
							case RECIP_SRC_HOME_VISIT:

									flg = test_home_visit_drugs_electronic (&SPres[i],
																			&FunctionError);

									SetErrorVarArr (&v_ErrorCode, FunctionError, 0, 0, NULL, &ErrOverflow);
									break;

							default:							// DonR 29Aug2016 CR #8956.

									// DonR 05Aug2003: test_mac_doctor_drugs_electronic () now handles
									// retrieval of the name of non-Maccabi doctors (DocID = license number
									// rather than Teudat Zehut number) with entries in doctor_percents
									// and doctors tables.
									flg = test_mac_doctor_drugs_electronic (v_TypeDoctorIdentification,
																			DocID,
																			&SPres[i],
																			i + 1,
																			Member.age_months,
																			v_MemberGender,
																			Phrm_info.pharmacy_permission,
																			v_RecipeSource,
																			v_MemberIdentification,
																			v_IdentificationCode,
																			(v_ClientLocationCode > 1),
																			&PharmIshurProfession[i],
																			&FunctionError);

									SetErrorVarArr (&v_ErrorCode, FunctionError, 0, 0, NULL, &ErrOverflow);
									break;

						}	// End of switch on prescription source.
					}	// Either there is no "as if" generic drug, or there was no price reduction
						// for the "as if" drug.

				}	// Loop through drugs - yet again!



				// Test for deadlock or severe error.
				if (flg == 2) // deadlock or access conflict
				{
					reStart = MAC_TRUE;
				}

				// If we've hit a serious database error, break out of outer loop and restart.
				if ((reStart == MAC_TRUE) || (v_ErrorCode == ERR_DATABASE_ERROR) || (flg == 1))
					break;



				// Member participating percent validation.
				//
				// Compare Prescription participation with Drug participation.
				// DonR 25Jan2015: DO NOT skip price- and participation-type checking for drugs
				// that have triggered a fatal error.
				flg = 0;

				for (i = 0; i < v_NumOfDrugLinesRecs; i++)
				{
//					if (SPres[i].DFatalErr == MAC_TRUE )
//						continue;
//
					// DonR 07Aug2003: The supplement gets its participation (i.e. RetPartCode)
					// from the "major" drug.
					// DonR 23Aug2005: This is not true for "gadgets" - which sometimes use
					// the Link to Addition parameter for a different purpose.
					if ((SPres[i].LinkDrugToAddition	>  0	)	&&
						(SPres[i].DL.largo_type			!= 'X'	)	&&
						(SPres[i].DL.in_gadget_table	== 0	)	&&
						(MajorDrugIndex[i]				>= 0	))
					{
						SPres[i].RetPartCode = SPres[MajorDrugIndex[i]].RetPartCode;
						continue;
					}

					// Percent2 is the "default" participation for this drug.
					state =	GET_PARTICIPATING_PERCENT (SPres[i].BasePartCode, &percent2);
					if (state != NO_ERROR)
					{
						if (SetErrorVarArr (&SPres[i].DrugAnswerCode, state, SPres[i].DrugCode, i + 1, &SPres[i].DFatalErr, &ErrOverflow))
						{
							flg = 1;
						}
						continue;
					}

					// Percent1 is the calculated participation, based on drug_extension,
					// doctor specialty, etc.
					state = GET_PARTICIPATING_PERCENT (SPres[i].RetPartCode, &percent1);
					if (state != NO_ERROR)
					{
						if (SetErrorVarArr (&SPres[i].DrugAnswerCode, state, SPres[i].DrugCode, i + 1, &SPres[i].DFatalErr, &ErrOverflow))
						{
							flg = 1;
						}
						continue;
					}

					// 20010726 special_prescs check 
					// have special confirmation and participating in {2,3} 
					// and answer participating = 4 
					// allow sell 100.00 % maccabi => patient not pay
					if ((MACCABI_PHARMACY)						&&
						(SPres[i].SpecPrescNum)					&&	
						(SPres[i].SpecPrescNumSource))
					{
						if (((SPres[i].BasePartCode == PARTI_CODE_TWO) ||
							 (SPres[i].BasePartCode == PARTI_CODE_THREE))	&&
							(SPres[i].RetPartCode == 4))
						{
							SPres[i].AdditionToPrice = 10000;
							continue;
						}
					}


					// Process Pharmacy Ishur, if any, for this drug line.
					LineNum = i + 1;

					// DonR 30Mar2006: Don't bother checking Pharmacy Ishur for Member ID zero!
					if (v_MemberIdentification > 0)
					{
						flg = test_pharmacy_ishur (v_PharmNum,
												   v_RecipeSource,
												   v_MemberIdentification,
												   v_IdentificationCode,
												   Member.maccabi_code,
												   v_MemberDiscountPercent,
												   Member.illness_bitmap,
												   v_TypeDoctorIdentification,
												   DocID,
												   &SPres[i],
												   PR_AsIf_Preferred_Largo[i],
												   v_SpecialConfNum,
												   v_SpecialConfSource,
												   v_RecipeIdentifier,
												   LineNum,
												   (short)5003,
												   PharmIshurProfession[i],
												   &FunctionError);

						SetErrorVarArr (&v_ErrorCode, FunctionError, 0, 0, NULL, &ErrOverflow);


						// Test for deadlock or severe error.
						if (flg == 2) // deadlock or access conflict
						{
							reStart = MAC_TRUE;
							break;
						}
						else
						{
							if (flg == 1)	// Other severe error.
							{
								break;
							}

							// If we get here, the Pharmacy Ishur routine was happy.
						}
					}	// Member ID is non-zero.


					// DonR 14Jun2004: If "drug" being purchased is a device in the "gadgets"
					// table, retrieve the appropriate information and query the AS400 for
					// participation information.
					// DonR 19Aug2025 User Story #442308: If Nihul Tikrot calls are disabled, disable Meishar calls as well.
					if ((SPres[i].DL.in_gadget_table)					&&
						(v_RecipeSource != RECIP_SRC_NO_PRESC)			&&
						(v_RecipeSource != RECIP_SRC_EMERGENCY_NO_RX)	&&
						(TikrotRPC_Enabled)								&&	// If Nihul Tikrot calls are disabled, disable Meishar calls too.
						(MEISHAR_PHARMACY))	// DonR 01Jul2012: New macro conditionally enables Prati Plus pharmacies.
					{
						v_gadget_code	= SPres[i].LinkDrugToAddition;
						v_DrugCode		= SPres[i].DrugCode;

						// DonR 18Sep2011: Add get_ptn_from_as400 flag to gadgets table; if it's equal to zero,
						// pharmacy has chosen a "placeholder" service that is not actually supposed to get
						// its participation from AS/400.
						// DonR 27Nov2011: Switch from get_ptn_from_as400 to new separate enabled-status flags
						// for Maccabi member and IDF soldiers.
						// DonR 25Jul2016: Fixed WHERE clause (which had stuff grouped together wrong); also
						// suppress the "duplicate gadget" message to log, since it's not an indication of any
						// actual program or database problem.
						ExecSQL (	MAIN_DB, TR2003_5003_READ_gadgets,
									&v_service_code,		&v_service_number,
									&v_service_type,		&v_enabled_mac,
									&v_enabled_hova,		&v_enabled_keva,
									&enabled_pvt_pharm,
									&v_DrugCode,			&v_gadget_code,
									&v_gadget_code,			&Member.MemberMaccabi,
									&Member.MemberHova,		&Member.MemberKeva,
									END_OF_ARG_LIST									);

						Conflict_Test (reStart);

						if (SQLERR_code_cmp (SQLERR_too_many_result_rows) == MAC_TRUE)
						{
							SetErrorVarArr (&SPres[i].DrugAnswerCode,	ERR_DUPLICATE_GADGET_ROW,	v_DrugCode,	i + 1,	NULL, &ErrOverflow);
							SetErrorVarArr (&v_ErrorCode,				ERR_DRUG_S_WRONG_IN_PRESC,	0,			0,		NULL, &ErrOverflow);
//							GerrLogMini (GerrId, "Duplicate gadget! Largo %d, Gadget Code %d, err %d.",
//										 v_DrugCode, v_gadget_code, SQLCODE);
							break;
						}
						else
						{
							// WORKINGPOINT: NEED TO GIVE PHARMACY SOMETHING MORE INFORMATIVE THAN "DATABASE ERROR" IN THIS CASE!
							// (DonR 12OCT2008)
							// DonR 21Jul2011: Just to de-clutter the log, provide a shorter message for
							// row-not-found.
							if (SQLERR_code_cmp (SQLERR_not_found) == MAC_TRUE)
							{
								GerrLogReturn (GerrId, "Get gadget: No data for Largo %d, Gadget Code %d.",
											   v_DrugCode, v_gadget_code);
								SetErrorVarArr (&v_ErrorCode, ERR_DATABASE_ERROR, 0, 0, NULL, &ErrOverflow);
								break;
							}
							else
							{
								if (SQLERR_error_test ())
								{
									GerrLogReturn (GerrId, "Get gadget: Largo %d, Gadget Code %d, err %d.",
												   v_DrugCode, v_gadget_code, SQLCODE);
									SetErrorVarArr (&v_ErrorCode, ERR_DATABASE_ERROR, 0, 0, NULL, &ErrOverflow);
									break;
								}

								// If we get here, we read the row successfully.

							}	// Something other than row-not-found.
						}	// Not a duplicate gadget-row.

						// DonR 18Sep2011: If get_ptn_from_as400 flag from gadgets table is equal to zero,
						// pharmacy has chosen a "placeholder" service that is not actually supposed to get
						// its participation from AS/400. Make the rest of the "gadgets" code conditional
						// on this flag.
						// DonR 27Nov2011: Switch from get_ptn_from_as400 to new separate enabled-status flags
						// for Maccabi member and IDF soldiers. A "placeholder" service will have zeroes for
						// all three member types.
						// DonR 20Jan2014: Add a condition for pharmacy type. Maccabi pharmacies can sell all
						// "meishar" items; private pharmacies can sell (or at least can get participation
						// from the "meishar" application) only those items with their "private pharmacy
						// enabled" flag set > 0.
						if (((MACCABI_PHARMACY) || (enabled_pvt_pharm > 0))			&&
							(((v_enabled_mac	> 0) && (Member.MemberMaccabi	> 0))	||
							 ((v_enabled_hova	> 0) && (Member.MemberHova		> 0))	||
							 ((v_enabled_keva	> 0) && (Member.MemberKeva		> 0))))
						{
							// DonR 25Mar2012: For members buying stuff at VAT-exempt pharmacies (i.e. in Eilat),
							// take VAT off the unit price sent to AS/400. The AS/400 "meishar" application is
							// supposed to send a response that will reflect this reduction, and so we won't
							// have to deduct VAT later on.
							// DonR 14Nov2012: We no longer want to send the reduced "Maccabi" price to Meishar;
							// instead, we just send the ordinary unit price, with VAT deducted for Eilat pharmacies.
							// DonR 20Jan2014: For private pharmacies, send the price from Price List 2 (normally used
							// for Maccabi pharmacies) to Meishar as the default unit price.
							if (PRIVATE_PHARMACY)
							{
								ExecSQL (	MAIN_DB, READ_GetStandardMaccabiPharmPriceForMeishar,
											&Yarpa_Price,	&v_DrugCode,	END_OF_ARG_LIST			);

								// No error trapping - just disable the price substitution.
								if (SQLCODE != 0)
									Yarpa_Price = 0;
							}
							else
							{
								// For Maccabi pharmacies, we never want (or need) to do this substitution.
								Yarpa_Price = 0;
							}

							// DonR 20Jan2014: Use Yarpa Price if we're at a private pharmacy and we read it
							// successfully from Price List 2; otherwise use RetOpDrugPrice, which is the
							// normal Yarpa Price.
							UnitPrice = (Yarpa_Price > 0) ? Yarpa_Price : SPres[i].RetOpDrugPrice;	// DonR 14Nov2012.
							if (vat_exempt != 0)
							{
								UnitPrice = (int)(((double)UnitPrice * no_vat_multiplier) + .5001);
							}


							// Store this gadget's information in array. If another gadget in the
							// same transaction has the same Service Code, Service Number, and
							// Service Type, abort and return a transaction-level error to
							// the pharmacy.
							gadget_svc_code		[i]		= v_service_code;
							gadget_svc_number	[i]		= v_service_number;
							gadget_svc_type		[i]		= v_service_type;

							for (j = 0; j < i; j++)
							{
								if ((gadget_svc_code	[j] == gadget_svc_code	[i]) &&
									(gadget_svc_number	[j] == gadget_svc_number[i]) &&
									(gadget_svc_type	[j] == gadget_svc_type	[i]))
								{
									SetErrorVarArr (&v_ErrorCode, ERR_DRUG_S_WRONG_IN_PRESC, 0, 0, NULL, &ErrOverflow);
									SetErrorVarArr (&SPres[i].DrugAnswerCode, ERR_DUPLICATE_GADGET_REQ,
													v_DrugCode, i + 1, NULL, &ErrOverflow);
									SPres[i].DFatalErr = MAC_TRUE;
									break;	// This is a severe error!
								}
							}


							// One more test before talking to the AS400: Did the pharmacy send
							// a non-zero value for Units? If so, return a severe error at the
							// treatment level, as quantity for Gadgets should be in OP only.
							if (SPres[i].Units > 0)
							{
								SetErrorVarArr (&v_ErrorCode, ERR_DRUG_S_WRONG_IN_PRESC, 0, 0, NULL, &ErrOverflow);
								SetErrorVarArr (&SPres[i].DrugAnswerCode, ERR_GADGET_REQ_WITH_UNITS,
												v_DrugCode, i + 1, NULL, &ErrOverflow);
								SPres[i].DFatalErr = MAC_TRUE;
								break;	// This is a severe error!
							}


							// If we get here, the gadget entry really does exist, and there are no
							// major problems with the gadget sale request.
							gettimeofday (&EventTime[EVENT_GADGET_START], 0);

							err = as400EligibilityTest (v_PharmNum,
														v_RecipeIdentifier,
														v_MemberIdentification,
														v_IdentificationCode,
														(int)v_service_code,
														(int)v_service_number,
														(int)v_service_type,
														v_DrugCode,
														(int)SPres[i].Op,
														(int)UnitPrice,
														(int)2,				// Retries
														&MeisharInfoCode,	// DonR 23Dec2014. For now, should be either 0 or 88.
														&l_dummy,			// plIdNumber
														&i_dummy,			// pnIdCode
														&QuantityPermitted,
														&RequestNum,
														&PtnPrice,
														&Insurance);

							gettimeofday (&EventTime[EVENT_GADGET_END], 0);

							GerrLogFnameMini ("gadgets_log",
											   GerrId,
											   "5003: AS400 err = %d.",
											   (int)err);

							flg = 0;	// Set to 1 in switch below if we've hit a fatal error.

							switch (err)
							{
								case enCommFailed:	// Communications error.
									SetErrorVarArr (&SPres[i].DrugAnswerCode, ERR_GADGET_COMM_PROBLEM,
													v_DrugCode, i + 1, NULL, &ErrOverflow);
									break;


								case enHasEligibility:		// Member is eligible.
									if (QuantityPermitted < SPres[i].Op)
									{
										// DonR 13Dec2010: Reduce OP only if the Quantity Permitted by AS/400
										// was *less* than the quantity requested.
										SPres[i].Op = QuantityPermitted;

										switch (Insurance)
										{
											case 1:		SetErrorVarArr (&SPres[i].DrugAnswerCode,
																		ERR_GADGET_OP_REDUCED_BASIC,
																		v_DrugCode, i + 1,
																		NULL,
																		&ErrOverflow);		break;

											case 2:		SetErrorVarArr (&SPres[i].DrugAnswerCode,
																		ERR_GADGET_OP_REDUCED_MAGEN,
																		v_DrugCode, i + 1,
																		NULL,
																		&ErrOverflow);		break;

											case 3:		SetErrorVarArr (&SPres[i].DrugAnswerCode,
																		ERR_GADGET_OP_REDUCED_KEREN,
																		v_DrugCode, i + 1,
																		NULL,
																		&ErrOverflow);		break;

											case 7:		SetErrorVarArr (&SPres[i].DrugAnswerCode,
																		ERR_GADGET_OP_REDUCED_YAHALOM,
																		v_DrugCode, i + 1,
																		NULL,
																		&ErrOverflow);		break;

											default:	SetErrorVarArr (&SPres[i].DrugAnswerCode,
																		ERR_GADGET_OP_REDUCED_UNKNOWN,
																		v_DrugCode, i + 1,
																		NULL,
																		&ErrOverflow);	break;
										}
									}
									else
									{
										switch (Insurance)
										{
											case 1:		SetErrorVarArr (&SPres[i].DrugAnswerCode,
																		ERR_GADGET_APPROVED_BASIC,
																		v_DrugCode, i + 1,
																		NULL,
																		&ErrOverflow);		break;

											case 2:		SetErrorVarArr (&SPres[i].DrugAnswerCode,
																		ERR_GADGET_APPROVED_MAGEN,
																		v_DrugCode, i + 1,
																		NULL,
																		&ErrOverflow);		break;

											case 3:		SetErrorVarArr (&SPres[i].DrugAnswerCode,
																		ERR_GADGET_APPROVED_KEREN,
																		v_DrugCode, i + 1,
																		NULL,
																		&ErrOverflow);		break;

											case 7:		SetErrorVarArr (&SPres[i].DrugAnswerCode,
																		ERR_GADGET_APPROVED_YAHALOM,
																		v_DrugCode, i + 1,
																		NULL,
																		&ErrOverflow);		break;

											default:	SetErrorVarArr (&SPres[i].DrugAnswerCode,
																		ERR_GADGET_APPROVED_UNKNOWN,
																		v_DrugCode, i + 1,
																		NULL,
																		&ErrOverflow);		break;
										}
									}

									// DonR 15Dec2015: It's possible that "Meishar" is overriding participation granted according
									// to as AS/400 ishur. If this is the case, clear out any ishur-about-to-expire warnings,
									// since they are not relevant.
									// It's possible that other ishur-related warnings or errors should also be suppressed.
									// DonR 17Jul2016: The Meishar ("gadget") application can override participation
									// from AS/400 ishurim. In this case, we need to zero out the ishur number, so
									// that we don't confuse other parts of the system.
									if (SPres[i].ret_part_source.table == FROM_SPEC_PRESCS_TBL)
									{
										SetErrorVarDelete (ERR_ISHUR_EXPIRY_15_WARNING,		SPres[i].DrugCode);
										SetErrorVarDelete (ERR_ISHUR_EXPIRY_30_WARNING,		SPres[i].DrugCode);
										SetErrorVarDelete (ERR_ISHUR_EXPIRY_45_WARNING,		SPres[i].DrugCode);

										SPres[i].SpecPrescNum		= 0;
										SPres[i].SpecPrescNumSource	= 0;
										SPres[i].IshurTikraType		= 0;
									}

									// DonR 02Sep2004: If more than one unit is being sold and authorized,
									// AS400 sends us the total participation, not the per-unit participation.
									// We have to change the participation to a per-unit basis, since this
									// is what the pharmacy expects.
									if (QuantityPermitted > 1)
										PtnPrice /= QuantityPermitted;

									SPres[i].RetPartCode					= 1;
									SPres[i].PriceSwap						= PtnPrice;
									SPres[i].ret_part_source.table			= FROM_GADGET_APP;


									// DonR 25Dec2007: Per Iris Shaya, if we're getting participation
									// from the AS400 "Shaban" application, turn member discounts off.
									SPres[i].in_health_pack					= 0;

									// DonR 09Jan2012: On the other hand (see above), if "Meishar" has authorized
									// participation of zero, we want to force in a 100% discount to clarify matters
									// for pharmacy systems and the AS/400.
									if (PtnPrice == 0)
										SPres[i].AdditionToPrice = 10000;


									switch (Insurance)
									{
										case 1:
										case 2:
										case 3:
										case 7:		SPres[i].ret_part_source.insurance_used	= Insurance;
													break;

										default:	SPres[i].ret_part_source.insurance_used	= BASIC_INS_USED;
													break;
									}

									// DonR 23Dec2014: "Meishar" can now send an "error code" that is not really an error. 
									// In this case, the code will be sent by the "listener" program in the field that was
									// originally used to send back the Prescription ID (which we weren't paying any
									// attention to anyway). We will now use this code to supply extra messages to the
									// pharmacy.
									switch (MeisharInfoCode)
									{
										case Approved_WorkAccident:	SetErrorVarArr (&SPres[i].DrugAnswerCode,
																					GADGET_APPROVED_WORK_ACCIDENT,
																					v_DrugCode, i + 1,
																					NULL, &ErrOverflow);
																	break;

										default:					break;
									}

									

									break;	// case enHasEligibility - Member is eligible.


								case enAlreadyPurchased:	// Member has already bought one of these gadgets.
									SetErrorVarArr (&SPres[i].DrugAnswerCode, ERR_GADGET_ALREADY_BOUGHT,
													v_DrugCode, i + 1, NULL, &ErrOverflow);
									break;

								case enNoAppropriateInsurance:	// Member is ineligible.
									SetErrorVarArr (&SPres[i].DrugAnswerCode, ERR_GADGET_NO_INSURANCE,
													v_DrugCode, i + 1, NULL, &ErrOverflow);
									break;

								case enPharmacyNotExist:
								case enUnknownPharmacy:
								case enCalculationError:
								case IdNumberNotFound:
									SetErrorVarArr (&SPres[i].DrugAnswerCode, ERR_GADGET_ERROR,
													v_DrugCode, i + 1, NULL, &ErrOverflow);
									break;

								case enPay2MonthLate:
								case IdNumberIndebted:
									SetErrorVarArr (&SPres[i].DrugAnswerCode, ERR_GADGET_DEADBEAT_MEMBER,
													v_DrugCode, i + 1, NULL, &ErrOverflow);
									break;

								case enNoAccordance:	// This is a show-stopper!
									SetErrorVarArr (&v_ErrorCode, ERR_DRUG_S_WRONG_IN_PRESC, 0, 0, NULL, &ErrOverflow);
									SetErrorVarArr (&SPres[i].DrugAnswerCode, ERR_GADGET_REQ_DISCREPANCY,
													v_DrugCode, i + 1, NULL, &ErrOverflow);
									SPres[i].DFatalErr = MAC_TRUE;
									flg= 1;	// This is a severe error!
									break;

								case enNotEligible:
									SetErrorVarArr (&SPres[i].DrugAnswerCode, ERR_GADGET_NOT_ELIGIBLE,
													v_DrugCode, i + 1, NULL, &ErrOverflow);
									break;

								case Rejected_TrafficAccident:
									SetErrorVar (&SPres[i].DrugAnswerCode, ERR_GADGET_TRAFFIC_ACCIDENT);
									break;

								default:	// Strange error.
									SetErrorVarArr (&SPres[i].DrugAnswerCode, ERR_GADGET_ERROR,
													v_DrugCode, i + 1, NULL, &ErrOverflow);
									break;
							}

							// If we got an AS400 return code indicating a fatal problem, break out
							// of dummy loop.
							if (flg != 0)
								break;

						}	// DonR 18Sep2011: Need to query AS400 for "gadget" eligibility.

					}	// Item in gadgets table being bought with prescription at Maccabi pharmacy.



					// Member discount percent calculation.
					//
					// Member discounts apply if:
					//
					// A) The drugs are being bought with a prescription.
					//
					// B) The member is entitled to a discount: Either by having Member Rights 7 or 17
					//    (entitled to a 100% discount) or with an explicit discount level set by
					//    Member Discount Percent.
					//    DonR 04Dec2014: Member can also get an automatic 100% discount if s/he has
					//    a match between his/her serious-illness bitmap and the drug's illness bitmap.
					//    This is implemented with the new macro GETS_100PCT_DISCOUNT.
					//
					// C) Drug is in Health Pack (i.e. "The Basket") - this can be set from drug_list,
					//    drug_extension, spclty_largo_prcnt, or an AS400 ishur.
					//    WORKINGPOINT: Should we be getting in_health_pack from the preferred drug
					//    when "participation swapping" is going on?
					//    DonR 03Jan2008: Yes, we should!
					//
					// D) The drug's Largo Type is equal to 'T' or 'M'.
					//
					// DonR 29Dec2014: In order to provide a message to pharmacies when a member with a
					// serious illness has to pay for prescribed drugs that are not relevant to that illness,
					// change the order of the logic a bit. The new macro MEMBER_GETS_DISCOUNTS tells us
					// that the member is entitled to discounts at least some of the time, but may not get
					// a discount on any particular medication.
					// DonR 11Feb2015: If we are actually selling a non-preferred drug under "preferred" conditions, the
					// preferred drug's in-health-basket flag will already have been copied to the regular in_health_pack
					// variable; and if we're *not* using the preferred drug's conditions (e.g. if we found a rule
					// of some sort that gives less than 100% participation for the non-preferred drug), we don't want
					// to use the preferred drug's in-health-basket flag. Accordingly, we want to test only the "regular"
					// health-basket flag for Condition C!
					// DonR 24Mar2015: For the moment, we're backing out the change from 11Feb2015.
					// DonR 30Jun2015: Restoring the change from 11Feb2015.
					// DonR 28Mar2016: Add capability for bypassing the health-basket requirement for particular illnesses.
					// DonR 13May2020 CR #31591: Use Largo Type lists from Sysparams to qualify discounts instead of
					// hard-coded values. For flexibility, use separate lists for illness-based and non-illness-based discounts,
					// as well as the new category of "ventilator" discounts. The comparision between a drug's Largo Type and these
					// lists is performed in read_drug() and stored in DL.IllnessDiscounts/VentilatorDiscounts/NonIllnessDiscounts.
					if ( (v_RecipeSource != RECIP_SRC_NO_PRESC)												&&	// Condition (A).
						 (v_RecipeSource != RECIP_SRC_EMERGENCY_NO_RX)										&&	// Condition (A).
						 (MEMBER_GETS_DISCOUNTS)															&&	// Condition (B).
						 ((SPres[i].in_health_pack != 0)	|| (IGNORE_HEALTH_BASKET (SPres[i].DL)))		&&	// Condition (C).
						 ((SPres[i].DL.IllnessDiscounts)	|| (SPres[i].DL.VentilatorDiscounts) || (SPres[i].DL.NonIllnessDiscounts)) )	// Condition (D) - revised by CR #31591.
					{
						// DonR 07Aug2003: Per Iris Shaya, deleted the condition that would skip the
						// discount for a drug with DFatalErr set TRUE. This was done because we want
						// to see discounts even if there's been an overdose/interaction detected.
						//
						// DonR 03Feb2004: If there is a Special Prescription, the drug's in_health_pack flag
						// has already been set based on the value in the special_prescs table. If the Special
						// Prescription specifies that the drug is not in the health basket, no discount
						// should be given.
						//
						// DonR 25Dec2007: Per Iris Shaya, member discounts are no longer conditioned
						// on any particular participation code.
						//
						//
						// DonR 11Feb2003: In all cases other than MemberRights = 7 or 17 or a bitmap match
						// (i.e. 100% discount), the level of discount will be stored in
						// Member Discount Percent.
						// DonR 02Oct2018 CR #13262: In some cases (cancer, at least) we need to qualify
						// member/drug combination based on matching Diagnosis Codes in order to grant
						// a 100% discount.
						//
						// DonR 13May2020 CR #31591: Add a new category of "ventilator" discounts. These are
						// given for members with their VentilatorDiscount flag set non-zero (currently comes
						// from members/asaf_code, which should be renamed when we migrate to MS-SQL), for
						// all items with Largo Type matching a list - currently "B", "Y", or "D".
						if (((Member.VentilatorDiscount) && (SPres[i].DL.VentilatorDiscounts))	||
							(GETS_100PCT_DISCOUNT_WITHOUT_DIAGNOSIS (SPres[i].DL)))
						{
							SPres[i].AdditionToPrice	= 10000;
							SPres[i].member_diagnosis	= 0;	// Redundant re-initialization - pure paranoia.
						}
						else
						{
							if (GETS_100PCT_DISCOUNT_WITH_DIAGNOSIS (SPres[i].DL))
							{
								// If we get here, we need to see if one or more of the member's diagnoses
								// corresponds with a listed diagnosis for the drug being sold.
								MatchingDiagnosis = 0;

								ExecSQL (	MAIN_DB,
											READ_Find_member_diagnosis,
											Find_diagnosis_from_member_diagnoses,
											&MatchingDiagnosis,
											&SPres[i].DrugCode,		&v_MemberIdentification,
											&v_IdentificationCode,	END_OF_ARG_LIST				);

								// If we didn't get a positive count(*) and a valid SQL result code, check against
								// additional member diagnoses in the special_prescs table.
								if ((SQLCODE != 0) || (MatchingDiagnosis < 1))
								{
									ExecSQL (	MAIN_DB,
												READ_Find_member_diagnosis,
												Find_diagnosis_from_special_prescs,
												&MatchingDiagnosis,
												&SPres[i].DrugCode,		&v_MemberIdentification,
												&SysDate,				&SysDate,
												&v_IdentificationCode,	END_OF_ARG_LIST				);
								}


								if ((SQLCODE == 0) && (MatchingDiagnosis > 0))
								{
									SPres[i].AdditionToPrice	= 10000;
									SPres[i].member_diagnosis	= MatchingDiagnosis;
								}
							}	// 100% discount conditional on diagnosis-code match.
						}	// Did *not* grant a 100% discount based on non-diagnosis match. 

						if (SPres[i].AdditionToPrice != 10000)	// No 100% discount - does member have a discount percentage set?
						{
							// Marianna 24Mar2022 Epic 232192: Darkonaim, Type 2 (Harel Foreign Workers).
							if ((Member.mem_id_extension == 9) && (Member.darkonai_type == 2))
							{
								GET_MEMBER_PRICE_ROW (SPres[i].RetPartCode, &PriceRow);
								
								// Marianna 24Mar2022 Epic 232192: Type 2 Darkonaim get a 100% discount if the participation
								// percentage is less or equal to a configurable value - currently 15%.
								if ((PriceRow.member_price_prcnt) <= (DarkonaiMaxHishtatfutPct))
								{
									SPres[i].AdditionToPrice = 10000; // 100% discount
								} // Get 100% discount darkonai
							}
							
							// DonR 12Jan2004: Make sure we don't overwrite a big discount with a little one.
							// DonR 13May2020 CR #31591: For flexibility, make this discount conditional on
							// the drug's NonIllnessDiscounts flag, which is set by read_drug() based on whether
							// the drug's Largo Type matches the list set in sysparams/memb_disc_4_types.
							if ((SPres[i].DL.NonIllnessDiscounts) && (Member.discount_percent > SPres[i].AdditionToPrice))
								SPres[i].AdditionToPrice = Member.discount_percent;
						}	// else - member has a discount percentage set.

						// If member isn't getting a 100% discount (s/he may have a lower percentage, or zero)
						// AND there is participation for this drug, AND the sale is at a Maccabi pharmacy,
						// AND the member has a serious-illness code that would give 100% discount for some drugs,
						// send a message to indicate that member has to pay some "hishtatfut". This should
						// happen only when a member is seriously ill but buys a drug that is not relevant
						// to that illness. Since we don't want to send this message more than once per sale,
						// it's sent at the "header" level, with Largo Code = 0. (Note: only Maccabi pharmacies
						// get this message for now - this may change in future.)
						if ((Member.illness_bitmap)										&&	// Member has a serious illness
							(SPres[i].AdditionToPrice < 10000)							&&	// Not giving 100% discount
							(SPres[i].RetPartCode != 4)									&&	// Not giving 0% from Member Price Code
							(MACCABI_PHARMACY))
						{
							SetErrorVarArr (&v_ErrorCode, DRUG_DISEASE_MISMATCH_NO_DISCOUNT, 0, 0, NULL, &ErrOverflow);
						}
					}	// Member may be entitled to a discount.


					// If participation is 100% (without Fixed Price) after all our machinations, AND
					// better specialist participation is allowed for this drug and member's insurance
					// type, send a warning message to pharmacy.
					// DonR 16May2006: Kishon Divers (who are supposed to get their medications
					// direct from the government) pay 100% unconditionally. This means that
					// they will get participation code 1 (= 100%) even for drugs that everyone
					// else pays 15% for. We don't want to send warning messages in this case!
					//
					// DonR 01Apr2008: Per Iris Shaya, do not send this error code for purchases
					// of dentist- or home-visit-prescribed drugs.
					// DonR 10Jul2011: Since we now can report multiple errors for each drug, suppress
					// this test only if a fatal error has already occurred.
					// DonR 09Jan2012: Replaced three conditions with macro FULL_PRICE_SALE.
					// DonR 12Jun2012: New macro FULL_PRICE_NO_ISHUR_ETC with some additional conditions.
					if ((FULL_PRICE_NO_ISHUR_ETC(i))												&&	// DonR 09Jan2012
						((SPres[i].DL.specialist_drug	>  0) || (PR_AsIf_Preferred_SpDrug[i] > 0))	&&
						(v_RecipeSource					!= RECIP_SRC_DENTIST)						&&
						(v_RecipeSource					!= RECIP_SRC_HOME_VISIT))
					{
						ExecSQL (	MAIN_DB, READ_test_for_ERR_SPCLTY_LRG_WRN,
									&RowsFound,

									&SPres[i].DrugCode,				&PR_AsIf_Preferred_Largo[i],
									&Member.MemberMaccabi,			&Member.MemberHova,
									&Member.MemberKeva,				&ROW_NOT_DELETED,
									&Member.insurance_type,			&Member.current_vetek,
									&Member.prev_insurance_type,	&Member.prev_vetek,
									END_OF_ARG_LIST												);

						Conflict_Test (reStart);

						if (SQLERR_error_test ())
						{
							SetErrorVarArr (&v_ErrorCode, ERR_DATABASE_ERROR, 0, 0, NULL, &ErrOverflow);
							break;
						}
						else
						{
							if (RowsFound > 0)
							{
								SetErrorVarArr (&SPres[i].DrugAnswerCode, ERR_SPCLTY_LRG_WRN,
												SPres[i].DrugCode, i + 1, NULL, &ErrOverflow);
							}
						}
					}


					// DonR 28Mar2004: Per Iris Shaya, if member could get a discount by going to a Maccabi
					// pharmacy, send a warning message.
					// DonR 15Feb2011: Tuned WHERE clause for better performance.
					// DonR 23Mar2011: Use the drug_list rule_status flag so we don't do a table lookup for
					// drugs that aren't in drug_extension.
					// DonR 10Jul2011: Since we now can report multiple errors for each drug, suppress
					// this test only if a fatal error has already occurred.
					// DonR 09Jan2012: Replaced three conditions with macro FULL_PRICE_SALE.
					// DonR 12Jun2012: New macro FULL_PRICE_NO_ISHUR_ETC incorporating more "standard" conditions.
					if ((FULL_PRICE_NO_ISHUR_ETC(i))									&&	// DonR 12Jun2012
						((SPres[i].DL.rule_status > 0) || (PR_AsIf_RuleStatus[i] > 0)))		// DonR 23Mar2011
					{
						// DonR 22Mar2011: In order to avoid checking the drug_extension table three times
						// when there are no rules applicable to this member, perform an extra lookup just
						// to see if there is something there. In theory at least, this should reduce the
						// average number of lookups.
						ExecSQL (	MAIN_DB, READ_test_for_SomeRuleApplies,
									&RowsFound,
									&SPres[i].DrugCode,		&PR_AsIf_Preferred_Largo[i],
									&Member.age_months,		&Member.age_months,
									&v_MemberGender,		&SysDate,
									&Member.MemberMaccabi,	&Member.MemberHova,
									&Member.MemberKeva,		END_OF_ARG_LIST					);

						// Don't bother with error-checking here; just say that if we got a valid response
						// from SQL *and* no rows were found, we set the flag FALSE, otherwise we continue
						// with the "real" table lookups, which do have error-checking.
						SomeRuleApplies = ((SQLCODE == 0) && (RowsFound == 0)) ? 0 : 1;
						

						if (SomeRuleApplies)
						{
							// Check for discount available at Maccabi pharmacy.
							ExecSQL (	MAIN_DB, READ_test_for_ERR_DISCOUNT_AT_MAC_PH_WARN,
										&RowsFound,

										&SPres[i].DrugCode,				&PR_AsIf_Preferred_Largo[i],
										&Member.age_months,				&Member.age_months,
										&v_MemberGender,				&Phrm_info.pharmacy_permission,
										&Phrm_info.pharmacy_permission,	&ROW_NOT_DELETED,
										&Member.MemberMaccabi,			&Member.MemberHova,
										&Member.MemberKeva,				&SysDate,
										&Member.insurance_type,			&Member.current_vetek,
										&Member.prev_insurance_type,	&Member.prev_vetek,
										END_OF_ARG_LIST														);

							Conflict_Test (reStart);

							if (SQLERR_error_test ())
							{
								SetErrorVarArr (&v_ErrorCode, ERR_DATABASE_ERROR, 0, 0, NULL, &ErrOverflow);
								break;
							}
							else
							{
								if (RowsFound > 0)
									SetErrorVarArr (&SPres[i].DrugAnswerCode, ERR_DISCOUNT_AT_MAC_PH_WARN,
													SPres[i].DrugCode, i + 1, NULL, &ErrOverflow);
							}
						}	// At least one drug_extension rule is applicable to this member.
					}	// Full-price sale, no fatal errors, etc., and there is a "nohal" for this drug.

					// DonR 12Jun2012: If this sale is discounted, an AS/400 ishur is being used, the
					// member is a Kishon diver, a fatal error has occurred for this drug, or the drug
					// doesn't have any nohalim, set the flag SomeRuleApplies to zero so that we don't
					// have to make the same tests again.
					// DonR 09Aug2021 User Story #163882: Another condition that applies is where the
					// purchaser is a darkonai-plus who doesn't receive any discounts other than "shovarim".
					else
					{
						SomeRuleApplies = 0;	// We know no rules apply even without reading drug_extension.
					}


					// DonR 11May2004: Per Iris Shaya, if member could get a discount according to some
					// rule with Authorizing Authority between 1 and 49, send a warning message.
					// DonR 11Aug2005: Per Iris Shaya, change the confirm_authority select
					// to examine only rules for low-level authority (less than 25).
					// DonR 14May2007: If member has an Ishur with "tikra", we don't need to
					// give this message.
					//
					// DonR 01Apr2008: Per Iris Shaya, do not give this warning message
					// for purchases of dentist- or home-visit-prescribed drugs.
					// DonR 15Feb2011: Tuned WHERE clause for better performance.
					// DonR 23Mar2011: Use the drug_list rule_status flag so we don't do a table lookup for
					// drugs that aren't in drug_extension.
					// DonR 10Jul2011: Since we now can report multiple errors for each drug, suppress
					// this test only if a fatal error has already occurred.
					// DonR 27Dec2011: This message applies to all ishur possibilities - so widen the
					// confirm_authority check to include everything greater than zero.
					// DonR 09Jan2012: Replaced three conditions with macro FULL_PRICE_SALE.
					// DonR 12Jun2012: SomeRuleApplies now implies that a whole bunch of conditions have been
					// met: the sale is not discounted or subject to an AS/400 ishur, the member isn't a Kishon
					// diver, the drug has a "nohal", and so on; so there's no point in testing the same
					// variables again!
					// DonR 13Jun2012: Give different messages for different forms of available ishur.
					if ((SomeRuleApplies)											&&
						(v_RecipeSource					!= RECIP_SRC_DENTIST)		&&
						(v_RecipeSource					!= RECIP_SRC_HOME_VISIT))
					{
						ErrorCodeToAssign = 0;	// Default: no ishur discount possible.

						// First, see if there is a pharmacy-ishur-level rule available to the selling pharmacy,
						// based on its permission type.
						ExecSQL (	MAIN_DB, READ_test_for_discount_with_pharmacy_ishur,
									&RowsFound,

									&SPres[i].DrugCode,					&PR_AsIf_Preferred_Largo[i],
									&Member.age_months,					&Member.age_months,
									&v_MemberGender,					&SysDate,
									&ROW_NOT_DELETED,					&Member.MemberMaccabi,
									&Member.MemberHova,					&Member.MemberKeva,
									&Phrm_info.pharmacy_permission,		&Phrm_info.pharmacy_permission,
									&Phrm_info.pharmacy_permission,		&Member.insurance_type,
									&Member.current_vetek,				&Member.prev_insurance_type,
									&Member.prev_vetek,					END_OF_ARG_LIST						);

						Conflict_Test (reStart);

						if (SQLERR_error_test ())
						{
							SetErrorVarArr (&v_ErrorCode, ERR_DATABASE_ERROR, 0, 0, NULL, &ErrOverflow);
							break;
						}
						else
						{
							if (RowsFound > 0)	// Found a pharmacy-ishur rule for this pharmacy's permission type.
							{
								switch (Phrm_info.pharmacy_permission)
								{
									case 1:		ErrorCodeToAssign = ERR_DISCOUNT_WITH_PH_ISHUR_WRN;	break;
									case 6:		ErrorCodeToAssign = ERR_ISHUR_DISCNT_PRATI_PLUS;	break;
									default:	ErrorCodeToAssign = ERR_UNSPEC_ISHUR_DISCNT_AVAIL;	break;
								}

							}

							else
							// If no pharmacy-ishur rule was found for the selling pharmacy's permission type,
							// perform a more general check that includes all pharmacy types and all confirmation-
							// authority levels.
							{
								ExecSQL (	MAIN_DB, READ_test_for_ERR_UNSPEC_ISHUR_DISCNT_AVAIL,
											&RowsFound,

											&SPres[i].DrugCode,				&PR_AsIf_Preferred_Largo[i],
											&Member.age_months,				&Member.age_months,
											&v_MemberGender,				&SysDate,
											&ROW_NOT_DELETED,				&Member.MemberMaccabi,
											&Member.MemberHova,				&Member.MemberKeva,
											&Member.insurance_type,			&Member.current_vetek,
											&Member.prev_insurance_type,	&Member.prev_vetek,
											END_OF_ARG_LIST													);

								Conflict_Test (reStart);

								if (SQLERR_error_test ())
								{
									SetErrorVarArr (&v_ErrorCode, ERR_DATABASE_ERROR, 0, 0, NULL, &ErrOverflow);
									break;
								}
								else
								{
									if (RowsFound > 0)	// Found a rule.
									{
										ErrorCodeToAssign = ERR_UNSPEC_ISHUR_DISCNT_AVAIL;
									}
								}	// No database error looking for general non-automatic rules.
							}	// No pharmacy-ishur rule was found.

							// Finally, if we found something in either query, report the result to pharmacy.
							if (ErrorCodeToAssign > 0)
							{
								SetErrorVarArr (&SPres[i].DrugAnswerCode, ErrorCodeToAssign,
												SPres[i].DrugCode, i + 1, NULL, &ErrOverflow);
							}

						}	// No database error checking drug_extension table for pharmacy-ishur rules.
					}	// Some rule applies for the member/drug and prescription source is not dentist/home visit.



					// DonR 28Mar2004: Per Iris Shaya, if member could have gotten a discount by
					// signing up for Magen Kesef or Magen Zahav, send a warning message to that effect.
					// DonR 11Aug2005: Per Iris Shaya, change the confirm_authority select
					// to examine only rules for low-level authority (less than 25).
					// DonR 15Feb2011: Tuned WHERE clause for better performance.
					// DonR 23Mar2011: Use the drug_list rule_status flag so we don't do a table lookup for
					// drugs that aren't in drug_extension.
					// DonR 10Jul2011: Since we now can report multiple errors for each drug, suppress
					// this test only if a fatal error has already occurred.
					// DonR 14Nov2011: Because Magen Kesef is being "turned off" (except for a few members who still
					// have Kesef and not Zahav), suppress the "discount if you weren't such a cheapskate" message
					// if the only rule that would give the discount is a Kesef rule.
					// DonR 20Dec2011: Per Iris Shaya, give this message only for "automatic" rules with
					// confirm_authority == 0.
					// DonR 09Jan2012: Replaced three conditions with macro FULL_PRICE_SALE.
					// DonR 12Jun2012: SomeRuleApplies now implies that a whole bunch of conditions have been
					// met: the sale is not discounted or subject to an AS/400 ishur, the member isn't a Kishon
					// diver, the drug has a "nohal", and so on; so there's no point in testing the same
					// variables again!
					// DonR 06Dec2012 "Yahalom": Changed SQL to reflect new "insurance_type" field; also, there is no
					// point in testing member's insurance status in SQL, since it's faster to check it in normal C code.
					// DonR 11Dec2012 "Yahalom": Add a new message (6149) if a "Yahalom" rule exists that would be
					// applicable. Only if no Yahalom rules are found will we check for Zahav rules.
					// DonR 19Jan2015: In order to permit sales of "shaban" drugs at 100%, we need to identify not
					// only situations where member doesn't have "shaban" and would get a discount if s/he did;
					// we also need to identify situations where member *does* have "shaban", but the discount
					// wasn't granted - presumably because s/he signed up too recently for discounts to take effect.
					// in order to accomplish this, we will now test rules here even for members who do have
					// "shaban", and then apply a new, non-reported-to-pharmacy error code to indicate that the
					// member isn't getting a discount but *does* have "shaban".
					if (SomeRuleApplies)	// DonR 19Jan2015: Member may or may not have "shaban".
					{
						strcpy (InsuranceTypeToCheck, "Y");

						ExecSQL (	MAIN_DB, READ_test_for_Maccabi_Sheli_or_Zahav_rule,
									&RowsFound,				&PossibleInsuranceType,

									&SPres[i].DrugCode,		&PR_AsIf_Preferred_Largo[i],
									&Member.age_months,		&Member.age_months,
									&v_MemberGender,		&SysDate,
									&ROW_NOT_DELETED,		&Member.MemberMaccabi,
									&Member.MemberHova,		&Member.MemberKeva,
									&InsuranceTypeToCheck,	END_OF_ARG_LIST					);

						Conflict_Test (reStart);

						if (SQLERR_error_test ())
						{
							SetErrorVarArr (&v_ErrorCode, ERR_DATABASE_ERROR, 0, 0, NULL, &ErrOverflow);
							break;
						}

						// If we got here, we know that member doesn't have Yahalom. If (A) no Yahalom
						// rules were found, and (B) member doesn't have Zahav either, check for applicable
						// Zahav rules. Note that we don't need to check for previous Zahav insurance, since
						// the only members for whom previous Zahav insurance is relevant are those with Yahalom.
						if (RowsFound < 1)		// DonR 19Jan2015: Member may or may not have "shaban".
						{
							strcpy (InsuranceTypeToCheck, "Z");

							ExecSQL (	MAIN_DB, READ_test_for_Maccabi_Sheli_or_Zahav_rule,
										&RowsFound,				&PossibleInsuranceType,

										&SPres[i].DrugCode,		&PR_AsIf_Preferred_Largo[i],
										&Member.age_months,		&Member.age_months,
										&v_MemberGender,		&SysDate,
										&ROW_NOT_DELETED,		&Member.MemberMaccabi,
										&Member.MemberHova,		&Member.MemberKeva,
										&InsuranceTypeToCheck,	END_OF_ARG_LIST					);

							Conflict_Test (reStart);

							if (SQLERR_error_test ())
							{
								SetErrorVarArr (&v_ErrorCode, ERR_DATABASE_ERROR, 0, 0, NULL, &ErrOverflow);
								break;
							}
						}	// No Yahalom rules were found, and member doesn't have Zahav.

						// If at least one applicable rule was found, send the appropriate message.
						// DonR 19Jan2015: Add a third "error" to indicate that member *does* have appropriate
						// insurance but still didn't get a discount - presumably because the new insurance is
						// not yet in force.
						if (RowsFound > 0)
						{
							if ((*PossibleInsuranceType == 'Y') && (Member.InsuranceType != 'Y'))
							{
								SetErrorVarArr (&SPres[i].DrugAnswerCode, ERR_DISCOUNT_IF_HAD_YAHALOM, SPres[i].DrugCode, i + 1, NULL, &ErrOverflow);
							}
							else
							if ((*PossibleInsuranceType == 'Z') && (Member.InsuranceType != 'Z') && (Member.InsuranceType != 'Y'))
							{
								SetErrorVarArr (&SPres[i].DrugAnswerCode, ERR_DISCOUNT_IF_HAD_ZAHAV, SPres[i].DrugCode, i + 1, NULL, &ErrOverflow);
							}
							else
							{
								SetErrorVarArr (&SPres[i].DrugAnswerCode, SHABAN_NOT_YET_APPLICABLE, SPres[i].DrugCode, i + 1, NULL, &ErrOverflow);
							}
						}

					}	// Some rule applies AND member doesn't have Yahalom.


					// 11Dec2003: Per Iris Shaya, substituting the Maccabi Price is a last resort.
					// Therefore, we should do it *after* checking for Pharmacy Ishur.

					// 25Mar2003: Per Iris Shaya, the logic for use of Maccabi Price is changing.
					// For normal participation calculation, we will always use the Yarpa
					// (i.e. regular) price; at the end, we'll use the Maccabi Price as
					// a fixed participation price if:
					//     A) A Maccabi Price exists for this drug in the pharmacy's default price list;
					//     B) Participation is 100%; and
					//     C) No fixed participation has been set from other sources, such as
					//        Drug-Extension.
					// Note that we're assuming that the Maccabi Price will be less than the
					// regular price!

					// 18Apr2004: This logic has to happen *before* the check for medicines that can
					// be sold only with a Special Prescription, since the Maccabi Price is a fixed
					// price that "turns off" the Special Prescription requirement check.
					// DonR 09Jan2012: Replaced three conditions with macro FULL_PRICE_SALE.
					// DonR 16Dec2013: If this is a sale of a "treatment"-type drug with a
					// prescription, at a Maccabi pharmacy, to a Maccabi member, Maccabi-price
					// discounts are disabled.
					// DonR 24Dec2014: We no longer disable Maccabi Price for prescription sales
					// to Maccabi members at Maccabi pharmacies; this backs out the change from
					// 16Dec2013.
					if ((SPres[i].MacabiDrugPrice > 0) && (FULL_PRICE_SALE(i)))
					{
						SPres[i].PriceSwap				= SPres[i].MacabiDrugPrice;
						SPres[i].MacabiPriceFlg			= MAC_TRUE;
						SPres[i].ret_part_source.table	= FROM_MACCABI_PRICE;
					}


					// DonR 28Jun2016: Add capability of getting a discounted price from the sale
					// tables sale/sale_bonus_recv.
					// DonR 11Jul2016: Re-enabled this feature, controlled by the new global flag
					// UseSaleTableDiscounts, set from sysparams/use_sale_tables.
					if ((MACCABI_PHARMACY)																	&&
						(UseSaleTableDiscounts)																&&
						(v_RecipeSource			!= RECIP_SRC_NO_PRESC)										&&
//						(v_RecipeSource			!= RECIP_SRC_EMERGENCY_NO_RX)								&&
						((FULL_PRICE_SALE(i))	|| (SPres[i].ret_part_source.table == FROM_MACCABI_PRICE)))
					{
						// Set up the current OP price as a default.
						Yarpa_Price		= (SPres[i].PriceSwap > 0) ? SPres[i].PriceSwap : SPres[i].RetOpDrugPrice;
						v_DrugCode		= SPres[i].DrugCode;

						ExecSQL (	MAIN_DB, READ_price_from_sale_bonus_recv,
									&Macabi_Price,
									&v_DrugCode,		&Yarpa_Price,
									&SysDate,			&SysDate,
									END_OF_ARG_LIST								);

						Conflict_Test (reStart);

						if (SQLCODE == 0)
						{
							SPres[i].PriceSwap				= Macabi_Price;
							SPres[i].MacabiPriceFlg			= MAC_TRUE;
							SPres[i].ret_part_source.table	= FROM_MACCABI_PRICE;
						}
					}


					// 21Dec2006: Per Iris Shaya, if after everything else the member is still
					// paying 100% with no fixed-price reduction, AND the sale is at a Maccabi
					// pharmacy, AND we aren't already working with the MaccabiCare price (i.e.
					// the price-list code we already read is *not* 800), AND a MaccabiCare price
					// exists, then use it.
					// 23Feb2008: Moved all the Price List 800 stuff down here. We use the
					// Maccabi Price from this price list if:
					// A) We haven't found any other discount;
					// B) The selling pharmacy and the drug are both in the Maccabicare program.
					// C) The sale is at a Maccabi pharmacy, OR it's a prescription sale (in
					//    which case the type of pharmacy is irrelevant).
					// In the special case of a non-prescription sale at a private pharmacy,
					// we also use the Maccabi price from Price List 800 - but in this case we
					// treat it as the "normal" drug price, and return Participation Code 97.
					// DonR 07Apr2008: To avoid unnecessary database lookups, added an
					// additional condition here. If a non-Maccabi member is purchasing
					// a drug without a prescription at a non-Maccabi pharmacy, all the
					// Maccabicare logic is irrelevant.
					//
					// DonR 02Aug2009: Per Iris Shaya, the Maccabicare logic now applies to both
					// Maccabi members and non-Maccabi members, at all private pharmacies.
					// DonR 09Jan2012: Replaced three conditions with macro FULL_PRICE_SALE.
					// DonR 16Dec2013: If this is a sale of a "treatment"-type drug with a
					// prescription, at a Maccabi pharmacy, to a Maccabi member, Maccabi-price
					// discounts are disabled.
					// DonR 24Dec2014: We no longer disable Maccabi Price for prescription sales
					// to Maccabi members at Maccabi pharmacies; this backs out the change from
					// 16Dec2013.
					// DonR 28Jun2016: Add global parameter to enable/disable Price List 800
					// lookup. For now at least, this flag will always be zero, but this could
					// change in the future.
					// DonR 11Jul2016: "Flipped" the global parameter - if UseSaleTableDiscounts is
					// set true, we *don't* want to look up Maccabi Price in Price List 800. The
					// parameter is now set from sysparams/use_sale_tables.
					// DonR 31Dec2019: This version of the Price List 800 query didn't have the
					// logic to ignore situations where the Price List 800 Maccabi Price is equal
					// to or higher than the price we've already computed. Since both Trn. 2003
					// and Trn. 6003 *do* have that logic (added on 7 January 2019), I believe
					// this was a minor bug - so I'll go ahead and use the ODBC command that
					// includes that criterion.
					if ((!UseSaleTableDiscounts)												&&	// DonR 28Jun2016
						(FULL_PRICE_SALE(i))													&&	// DonR 09Jan2012
						(maccabicare_pharm				!= 0)									&&
						(SPres[i].DL.maccabicare_flag	!= 0))
					{
						v_DrugCode = SPres[i].DrugCode;

						ExecSQL (	MAIN_DB, READ_PriceList, READ_PriceList_800_conditional,
									&Yarpa_Price,	&Macabi_Price,		&Supplier_Price,
									&v_DrugCode,	&Yarpa_Price,		END_OF_ARG_LIST		);

						Conflict_Test (reStart);

						if (SQLCODE == 0)
						{
							// DonR 23Aug2011: For VAT-exempt pharmacies (e.g. in Eilat), deduct VAT from prices.
							// For now, adjust only the Yarpa price; but in future we may need to do the same
							// for the Maccabi (discounted) price.
							// DonR 19Feb2012: Added a separate price variable for calculating member participation, since Eilat
							// pharmacies are still supposed to see the VAT-inclusive price as the per-OP price.
							if (vat_exempt != 0)
							{
								SPres[i].PriceForPtnCalc = (int)(((double)Yarpa_Price * no_vat_multiplier) + .5001);
							}
							else
							{
								SPres[i].PriceForPtnCalc = Yarpa_Price;
							}

							SPres[i].YarpaDrugPrice			= Yarpa_Price;
							SPres[i].MacabiDrugPrice		= Macabi_Price;
							SPres[i].SupplierDrugPrice		= Supplier_Price;
							SPres[i].ret_part_source.table	= FROM_MACCABI_PRICE;
							// (Is the last line correct for private-pharmacy
							// non-prescription sales?

							if ((MACCABI_PHARMACY) ||
								((v_RecipeSource != RECIP_SRC_NO_PRESC) && (v_RecipeSource != RECIP_SRC_EMERGENCY_NO_RX)))
							{
								// "Conventional" situation: use Maccabicare price from
								// Price List 800 as a discounted fixed price.
								SPres[i].RetOpDrugPrice			= Yarpa_Price;
								SPres[i].PriceSwap				= Macabi_Price;
								SPres[i].MacabiPriceFlg			= MAC_TRUE;
								SPres[i].RetPartCode			= PARTI_CODE_MACCABICARE;	// = 17.
							}
							else
							{
								// Private pharmacy non-prescription Maccabicare sale.
								SPres[i].RetPartCode = PARTI_MACCABICARE_PVT;	// = 97.

								// DonR 05Aug2009: Bug fix - even though everyone gets Price Code 97
								// for these sales, only Maccabi members get the special Maccabi price.
								if (v_MemberBelongCode == MACABI_INSTITUTE)
									SPres[i].RetOpDrugPrice = Macabi_Price;
							}
						}


						if (SQLERR_code_cmp (SQLERR_not_found) != MAC_TRUE)
						{
							if (SQLERR_error_test ())
							{
								SetErrorVarArr (&v_ErrorCode, ERR_DATABASE_ERROR, 0, 0, NULL, &ErrOverflow);
								break;
							}
						}
					}	// Need to check for Price-list 800 price.


					// DonR 04Dec2011: If the person making the purchase is a soldier and participation is 100%
					// (even if we're applying the Maccabi Price discount), send a warning message so pharmacist
					// will tell the soldier that s/he has to pay for the medication.
					// DonR 08Jan2012: If "Meishar" has given a fixed price of zero, that's a real fixed price,
					// so don't give this message.
					if ((v_RecipeSource					!= RECIP_SRC_NO_PRESC)								&&
						(Member.MemberTzahal			>  0)												&&
						(SPres[i].RetPartCode			== 1)												&&
						(SPres[i].ret_part_source.table	!= FROM_GADGET_APP)									&&	// DonR 08Jan2012
						((SPres[i].PriceSwap			== 0) || (SPres[i].ret_part_source.table == FROM_MACCABI_PRICE)))
					{
						SetErrorVarArr (&SPres[i].DrugAnswerCode, WARN_TZAHAL_NO_DISCOUNT,
										SPres[i].DrugCode, i + 1, NULL, &ErrOverflow);
					}


					// DonR 25Mar2004: Per Iris Shaya, if participation is 100% (without Fixed Price)
					// after all our machinations, AND no specialist participation exists for this drug,
					// AND NO RULES EXIST for low-level authorizing  authorities for this drug, AND one
					// or more rules exist which would allow some unspecified member to get a Special
					// Prescription for this drug, send an error message to pharmacy. This combination of
					// circumstances indicates that this drug is not available without an AS400 Ishur,
					// even if the member is willing to pay 100%.
					// DonR 11Aug2005: Per Iris Shaya, change the confirm_authority select
					// to examine only rules for low-level authority (less than 25).
					//
					// DonR 26Apr2007: Per Iris Shaya, changed conditions for error 6033. We
					// don't need to check drug_extension or spclty_largo_prcnt any more; we
					// just have to check the following conditions:
					// 1) 100% participation.
					// 2) No special price.
					// 3) No AS400 ishur with "tikra" flag set for this member/drug.
					// 4) Drug has "ishur required" flag set to 9.
					//
					// DonR 13Jul2011: Per Iris Shaya, changed criterion #3 (see above) so that an
					// AS/400 ishur without its "tikra" flag set *and* without any price reduction
					// can still enable the sale of a drug.
					// DonR 09Jan2012: Replaced three conditions with macro FULL_PRICE_SALE.
					if ((FULL_PRICE_SALE(i))										&&	// DonR 09Jan2012
						(SPres[i].ret_part_source.table	!= FROM_SPEC_PRESCS_TBL)	&&
						(SPres[i].DL.ishur_required		== 9))
					{
						SetErrorVarArr (&SPres[i].DrugAnswerCode, ERR_ISHUR_POSSIBLE_ERR,
										SPres[i].DrugCode, i + 1, NULL, &ErrOverflow);
					}

					// DonR 04Dec2013: Maccabi pharmacies are not allowed to sell "treatment" drugs
					// with presccription to Maccabi members at full price.
					// DonR 26Dec2013: If the sale is being rejected because the person buying drugs isn't a Maccabi
					// member, don't bother giving the "100% Forbidden" error.
					// DonR 26Oct2014: take out the "flg = 1" bit, since we're adding new logic that will allow the sale
					// to go through under some conditions.
					// DonR 23Nov2014: Having a severe error code set for this drug prevented the program from
					// calling the Nihul Tikrot application; so instead, use a new "placeholder" error code, which
					// will either be converted to the real error, or else deleted.
					// DonR 24Dec2014: Give this error only for drugs that require a prescription. Error 6173
					// (COULD_BUY_WITHOUT_PRESC) is thus no longer relevant.
					// DonR 09Aug2021 User Story #163882: Darkonai-Plus purchasers who are not supposed to get any discounts
					// (other than shovarim provided by their insurance company) are allowed to buy drugs at 100% from
					// Maccabi pharmacies.
					if ((MACCABI_PHARMACY)												&&
						(v_MemberBelongCode				== MACABI_INSTITUTE)			&&
						(v_ErrorCode					!= ERR_MEMBER_NOT_ELEGEBLE)		&&	// DonR 26Dec2013
						(v_RecipeSource					!= RECIP_SRC_NO_PRESC)			&&
						(v_RecipeSource					!= RECIP_SRC_EMERGENCY_NO_RX)	&&
						(SPres[i].DL.largo_type			== 'T')							&&
						(SPres[i].DL.no_presc_sale_flag	== MAC_FALS)					&&	// DonR 24Dec2014
						(SPres[i].ret_part_source.table	!= FROM_SPEC_PRESCS_TBL)		&&
						(Member.force_100_percent_ptn	== 0)							&&	// DonR 09Aug2021 User Story #163882
						(FULL_PRICE_SALE(i)))
					{
						SetErrorVarArr (&SPres[i].DrugAnswerCode, ERR_MACCABI_100_PER_CENT,
										SPres[i].DrugCode, i + 1, &SPres[i].DFatalErr, &ErrOverflow);
					}


					// DonR 29Jun2006: If necessary, check Drug Purchase Limits.
					// Note that if the member has an AS400 Special Prescription
					// with a quantity limit for this drug, that's enough - we
					// don't need to check the Drug Purchase Limit as well.
					//
					// First, mark all drugs which are being bought at some kind
					// of discount; this allows test_purchase_limits() to run
					// faster, since it doesn't have to make the same checks
					// multiple times.
					if (i == 0)	// Do this only once!
					{
						for (is = 0; is < v_NumOfDrugLinesRecs; is++)
						{
							// Just to be paranoid, repeat the default initialization.
							SPres[is].BoughtAtDiscount = MAC_FALS;

							// DonR 08Jan2012: If "Meishar" has given a fixed price of zero, that's a real
							// fixed price - so don't look for further discount possibilities.
							// DonR 07Apr2013: Per Iris Shaya, drugs that got their participation from an AS/400
							// ishur are considered "discounted" even if they were bought at full price. (This
							// happens, for example, when they are subject to an "ishur with tikra".)
							if (((SPres[is].PriceSwap > 0) && (SPres[is].MacabiPriceFlg == 0))	||
								( SPres[is].ret_part_source.table == FROM_SPEC_PRESCS_TBL)		||	// DonR 07Apr2013
								( SPres[is].ret_part_source.table == FROM_GADGET_APP))				// DonR 08Jan2012
							{
								// Member got a fixed price other than Maccabi Price.
								SPres[is].BoughtAtDiscount = MAC_TRUE;
							}
							else
							{
								// If member paid 100% and didn't get a "real" discounted
								// fixed price, this drug was not bought at discount.
								switch (SPres[is].RetPartCode)
								{
									case  1:
									case 11:
									case 19:
									case 50:
									case PARTI_MACCABICARE_PVT:
									case 98:
												break;

									// Any other price code indicates some kind of discount.
									default:
												SPres[is].BoughtAtDiscount = MAC_TRUE;
												break;
								}	// Switch on member_price_code.
							}	// Member didn't get a discounted fixed price.
						}	// Loop through drugs to see which ones are being bought at a discount.
					}	// Execute inner loop only once.

					// If this drug needs to have Purchase Limit checking performed, go do it.
					// DonR 23Aug2010: Per Iris Shaya, Drug Purchase Limits should not be checked for those
					// non-Maccabi "members" in the membernotcheck table.
					// DonR 01Dec2016: Added macro to implement the option of exempting ill members from
					// certain drug purchase limits. This is passed to test_purchase_limits(), since
					// prescription-source validation applies even if  "real" quantity limits do not.
					// DonR 14Dec2021 User Story #205423: If the drug is being purchased without a
					// prescription, test purchase limits without regard for whether a discount is being
					// applied. This is because new Method 7/8 limits exist for safety rather than
					// economic purposes.
					// DonR 23Jul2025 User Story #427783: Now there is also a "check limits including
					// full-price purchases" option at the drug_purchase_lim level, to support new
					// safety restrictions on opioids and other dangerous drugs.
					if (( SPres[i].DL.purchase_limit_flg > 0)	&&
						( !SPres[i].HasIshurWithLimit)			&&
						( Member.check_od_interact)				&&	// DonR 24Jan2018 CR #13937
						(	(SPres[i].BoughtAtDiscount					)	||
							(SPres[i].PrescSource == RECIP_SRC_NO_PRESC	)	||
							(SPres[i].PurchaseLimitIncludeFullPrice		)	||	// DonR 23Jul2025 User Story #427783
							(SPres[i].DL.purchase_lim_without_discount	)	)	)
//						((SPres[i].BoughtAtDiscount) || (SPres[i].PrescSource == RECIP_SRC_NO_PRESC) || (SPres[i].DL.purchase_lim_without_discount)))
					{
						flg = test_purchase_limits (&SPres[i],
													&SPres[0],
													v_NumOfDrugLinesRecs,
													&Member,
													MACCABI_PHARMACY,
													OVERSEAS_MEMBER,
													EXEMPT_FROM_LIMIT,
													v_PharmNum,
													&FunctionError);
					}


					// DonR 05Mar2017: If necessary, reject this drug line based on its Permit Sales flag
					// in the prescr_source table, possibly modified by an applicable Method 2 "limit"
					// from drug_purchase_lim.
					if (SPres[i].PurchaseLimitSourceReject)
					{
						SetErrorVarArr (&SPres[i].DrugAnswerCode, DRUG_RX_SOURCE_MEMBER_FORBIDDEN, SPres[i].DrugCode, i + 1, &SPres[i].DFatalErr, &ErrOverflow);
					}


					// DonR 17Jan2006: Per Iris Shaya, check for early refills.
					// DonR 21Mar2006: If this drug is being sold according to an AS400 Ishur
					// with quantity limits, there is no need to check for early refill as well.
					// DonR 23Aug2010: Per Iris Shaya, don't check for early refills for those "members"
					// in the membernotcheck table.
					if ((MACCABI_PHARMACY)										&&
						(v_RecipeSource			!= RECIP_SRC_NO_PRESC)			&&
						(Member.check_od_interact)								&&	// DonR 24Jan2018 CR #13937
						(!SPres[i].HasIshurWithLimit)							&&	// DonR 21Mar2006.
						((SPres[i].DL.largo_type	== 'B')		||
						 (SPres[i].DL.largo_type	== 'M')		||
						 (SPres[i].DL.largo_type	== 'T')))
					{
						MaxRefillDate	= IncrementDate (SysDate, (0 - min_refill_days));

						// DonR 24Nov2024 User Story #366220: Select prior sales of generic equivalents
						// as well as sales of the drug currently being sold.
						ExecSQL (	MAIN_DB, READ_check_for_early_refill,
									&RowsFound,
									&v_MemberIdentification,		&SPres[i].DrugCode,
									&SPres[i].DL.economypri_group,	&MaxRefillDate,
									&v_IdentificationCode,			END_OF_ARG_LIST			);

						Conflict_Test (reStart);

						if (SQLERR_error_test ())
						{
							SetErrorVarArr (&v_ErrorCode, ERR_DATABASE_ERROR, 0, 0, NULL, &ErrOverflow);
							break;
						}
						else
						{
							if (RowsFound > 0)
							{
								// DonR 23Jun2024 User Story #318200: Decide whether this is an expensive drug based on
								// different prices for Maccabi doctor prescriptions versus all other prescription types.
								int	expensive_drug_prc;

								expensive_drug_prc = (SPres[i].PrescSource == RECIP_SRC_MACABI_DOCTOR) ? ExpensiveDrugMacDocRx : ExpensiveDrugNotMacDocRx;

								if (SPres[i].YarpaDrugPrice >= expensive_drug_prc)
								{
									SetErrorVarArr (&SPres[i].DrugAnswerCode, ERR_EARLY_REFILL_EXPENSIVE_WARN,
													SPres[i].DrugCode, i + 1, NULL, &ErrOverflow);
								}
								// DonR 21Sep2008: Per Iris Shaya, for one special pharmacy (which sells to
								// hospitals, etc. rather than "frontally"), we want to give the early-refill
								// warning even for inexpensive drugs.
								// DonR 19Apr2010: Changed #define from ...WARNING to ...CHEAP_WARNING.
								// DonR 26Mar2014: Pharmacy 997582 works the same as Pharmacy 995482.
								else
								{
									if ((v_PharmNum == 995482) || (v_PharmNum == 997582))
									{
										SetErrorVarArr (&SPres[i].DrugAnswerCode, ERR_EARLY_REFILL_CHEAP_WARNING,
														SPres[i].DrugCode, i + 1, NULL, &ErrOverflow);
									}
								}
							}	// Found recent purchase of same drug.
						}	// Succeeded in getting purchase-count.
					}	// Need to perform early-refill tests.

				}	// Loop through drugs prescribed.
			}	// Current action is a drug sale. (This "if" started about 1,900 lines ago!)



			// If we've hit a serious error, break out of outer loop.
			if (flg == 1)
			{
				// Note that this won't override ERR_DATABASE_ERROR or other severity >= 10 errors.
				SetErrorVarArr (&v_ErrorCode, ERR_DRUG_S_WRONG_IN_PRESC, 0, 0, NULL, &ErrOverflow);
				break;
			}

		}
		while (0);
		// End of Big Dummy Loop #2.


		// We don't need to calculate participation/insurance for deletions.
		if (v_ActionType != SALE_DELETION)
		{
			// Calculate actual member participation for each drug being sold.
			// Also, calculate insurance stuff here, so we'll already know it when we call
			// the "Nihul Tikrot" program on AS/400.
			// DonR 12Mar2012: Added (redundant and paranoid) initialization of FullPriceTotal to the
			// for-loop.
			for (i = FullPriceTotal = 0; i < v_NumOfDrugLinesRecs; i++)
			{
				// First, calculate participation per OP.
				//
				// Fixed prices still apply (in theory) to the main ingredient of a "preparation";
				// but for all other ingredients, participation is zero even if they would normally
				// have a fixed price.
				// DonR 26Jan2011: MaccabiCare discount prices (either at Maccabi pharmacies or with a prescription)
				// are also used in calculating participation in place of the default RetOpDrugPrice.
				// DonR 08Jan2012: If "Meishar" has given a fixed price of zero, that's a real
				// fixed price.
				if (((SPres[i].RetPartCode	== 1) || (SPres[i].RetPartCode				== PARTI_CODE_MACCABICARE))	&&
					((SPres[i].PriceSwap	!= 0) || (SPres[i].ret_part_source.table	== FROM_GADGET_APP)))
				{
					if ((PreparationLinkLargo == 0) || (SPres[i].DrugCode == PreparationLinkLargo))
					{
						// DonR 25Mar2012: For members buying stuff at VAT-exempt pharmacies (i.e. in Eilat),
						// take VAT off the fixed price, UNLESS it's a fixed price received from the AS/400 "meishar"
						// application. In the latter case, VAT should already have been taken off, so we don't want
						// to take it off twice.
						if ((vat_exempt != 0) && (SPres[i].ret_part_source.table != FROM_GADGET_APP))
						{
							// Note that casting the result of a floating-point computation to long results in
							// truncation rather than rounding - so adding .5001 beforehand won't cause member
							// to be charged one agora for a VAT-exempt freebie.
							SPres[i].PriceSwap = (int)(((double)SPres[i].PriceSwap * no_vat_multiplier) + .5001);

							// DonR 16Dec2013: If this is a prescription purchase receiving a "Maccabi Price"
							// discount, the price needs to be rounded to the nearest 10-agorot value.
							if ((v_RecipeSource					!= RECIP_SRC_NO_PRESC)		&&
								(SPres[i].PriceSwap				>  0)						&&
								(SPres[i].MacabiPriceFlg		== MAC_TRUE)				&&
								(SPres[i].ret_part_source.table	== FROM_MACCABI_PRICE))
							{
								Agorot = SPres[i].PriceSwap % 10;
								if (Agorot > 4)
									SPres[i].PriceSwap += 10;	// Round up.
								SPres[i].PriceSwap -= Agorot;	// Set the Agorot digit to zero.
							}
						}


						PtnWorkVar = (double)SPres[i].PriceSwap;
					}
					else
					{
						PtnWorkVar = 0.0;	// Non-main ingredient of "preparation".
					}
				}	// Some form of fixed-price participation.
				else
				{	// Something other than fixed-price participation.
					// If we're dealing with a "preparation", participation is based on the "chemical price" sent
					// by the pharmacy. This is used as the OP Price for the main ingredient, with all other items
					// in the sale priced at zero. If this is a conventional sale, we use the normal OP price.
					if (PreparationLinkLargo > 0)
					{
						PtnOpPrice = (SPres[i].DrugCode == PreparationLinkLargo) ? ChemicalPrice : 0.0;
					}
					else
					{
						// DonR 20Feb2012: I've made a fix for Eilat pharmacies. Since the per-package price
						// sent to pharmacies is supposed to include VAT even though these pharmacies are exempt
						// from VAT, I've set up a separate variable PriceForPtnCalc to hold the price to be used
						// in calculating member participation - with or without VAT, as appropriate. RetOpDrugPrice
						// will always have the per-package price inclusive of VAT.
						PtnOpPrice = SPres[i].PriceForPtnCalc;	// Was "= SPres[i].RetOpDrugPrice".
					}

					GET_MEMBER_PRICE_ROW (SPres[i].RetPartCode, &PriceRow);

					// If there is a minimum discounted price, use that if the package price is within range.
					// If price is below the minimum, there is no discount.
					if ((PriceRow.min_reduced_price > 0) && (PriceRow.max_pkg_price > 0))
					{
						if ((PtnOpPrice >= PriceRow.min_reduced_price) &&
							(PtnOpPrice <= PriceRow.max_pkg_price))	// Or just "<"?
						{
							PtnWorkVar = (double)PriceRow.min_reduced_price;
						}
						else
						{
							// If price is above the top of the range for the minimum discounted price,
							// apply percentage discount.
							if (PtnOpPrice > PriceRow.max_pkg_price)
							{
								PtnWorkVar =
									(double)PtnOpPrice * (double)PriceRow.member_price_prcnt / (double)10000;
							}
							else
							{
								PtnWorkVar =
									(double)PtnOpPrice;	// Cheap drug = no discount.
							}
						}
					}	// Participation type has a minumum per-package price.
					else
					{
						// For any other participation code, just apply the discount.
						PtnWorkVar =
							(double)PtnOpPrice * (double)PriceRow.member_price_prcnt / (double)10000;
					}
				}	// Not a fixed price.


				// Next, multiply by number of packages and units. To avoid size-of-variable limitations,
				// use a double-precision float variable for calculations.
				// DonR 08Dec2010: If this is a "preparation" sale, the price sent by the pharmacy represents
				// the full quantities being sold - so don't do the multiplication.
				if (PreparationLinkLargo == 0)
					PtnWorkVar *= (((double)SPres[i].Units / (double)SPres[i].DL.package_size) + (double)SPres[i].Op);


				// DonR 11Jun2017 CR #8425: Remember the computed participation amount before applying member discount.
				// If there's no member discount, we'll just assign this value to MemberPtnAmount[i] below.
				// We add a smidgin in case any precision issue causes us to lose an agora.
				PtnBeforeDiscount = (int)(PtnWorkVar + 0.5001);

				// Apply any member discount.
				if (SPres[i].AdditionToPrice > 0)
				{
					PtnWorkVar *= (((double)10000 - (double)SPres[i].AdditionToPrice) / (double)10000);

					// Assign float value back to integer variable - adding a smidgin in case any precision
					// issue causes us to lose an agora.
					MemberPtnAmount [i] = (int)(PtnWorkVar + 0.5001);

					// DonR 11Jun2017 CR #8425: If there is a member discount, store it in SPres[i].DiscountApplied.
					SPres[i].DiscountApplied = PtnBeforeDiscount - MemberPtnAmount [i];
				}
				else	// No discount - just copy PtnBeforeDiscount to MemberPtnAmount [i].
				{
					MemberPtnAmount [i] = PtnBeforeDiscount;
				}


				// DonR 12Mar2012: Add new logic to compute the total *non-discounted* price of the current sale.
				// This will be used below to issue a message to pharmacies if a member who has resided overseas
				// for two years or more (maccabi_code == 22 or 23) is attempting to purchase more than a set
				// amount of prescription drugs (defaulting to NIS 500).
				// Note that we treat preparation stuff the same as any other item for this purpose.
				OverseasWorkVar	=  (double)SPres[i].YarpaDrugPrice;
				OverseasWorkVar	*= (((double)SPres[i].Units / (double)SPres[i].DL.package_size) + (double)SPres[i].Op);
				FullPriceTotal	+= (int)(OverseasWorkVar + 0.5001);


				// Insurance/Participation Source processing.
				// DonR 31Aug2005: If this is a supplement that gets its participation
				// (i.e. RetPartCode) from the "major" drug, then its participation
				// method must be the same as well. This is not true for "gadgets" -
				// which sometimes use the Link to Addition parameter for a different purpose.
				// DonR 13May2010: Rather than just use the subscript of the major drug,
				// actually assign the major drug's participation-source data to the
				// supplement. This makes things simpler for the "Nihul Tikrot" RPC
				// routine below.
				if ((SPres[i].LinkDrugToAddition	>  0	)	&&
					(SPres[i].DL.largo_type			!= 'X'	)	&&
					(SPres[i].DL.in_gadget_table	== 0	)	&&
					(MajorDrugIndex[i]				>= 0	))
				{
					j = MajorDrugIndex[i];
					SPres[i].ret_part_source.insurance_used	= SPres[j].ret_part_source.insurance_used;
					SPres[i].ret_part_source.table			= SPres[j].ret_part_source.table;
					SPres[i].WhySpecialistPtnGiven			= SPres[j].WhySpecialistPtnGiven;
				}

				// DonR 23Sep2004: Compute new participation source as a two-digit number, with
				// the "tens" digit indicating the insurance used and the "ones" digit
				// indicating the participation source.
				SPres[i].InsPlusPtnSource =   (10 * SPres[i].ret_part_source.insurance_used)
												+	SPres[i].ret_part_source.table;

				// DonR 14Jan2009: If we've assigned specialist participation (which can come
				// "directly" or through a Pharmacy Ishur), there should be a reason for doing
				// so in WhySpecialistPtnGiven. Store this value (which should be between 1 and 6)
				// as the "hundreds" digit in the participation-method field.
				if (((SPres[i].ret_part_source.table == FROM_DOC_SPECIALIST)	||
					 (SPres[i].ret_part_source.table == FROM_PHARMACY_ISHUR))		&&
					((SPres[i].WhySpecialistPtnGiven > 0) && (SPres[i].WhySpecialistPtnGiven < 10)))
						// The latter condition should always be true, but just in case...
				{
					SPres[i].InsPlusPtnSource += (100 * SPres[i].WhySpecialistPtnGiven);
				}
			}	// Member participation / Insurance-plus-participation-source processing loop.


			// DonR 12Mar2012: For members who have lived overseas for two years or more
			// (maccabi_code == 22 or 23), there are restrictions on buying prescription
			// medications under certain conditions. Here we evaluate those conditions
			// and give appropriate messages to the pharmacy.
			if ((v_MemberBelongCode	== MACABI_INSTITUTE)			&&
				(v_RecipeSource		!= RECIP_SRC_NO_PRESC)			&&
				(OVERSEAS_MEMBER))
			{
				// For certain prescription sources, we need to send the error without regard to the size of
				// the sale. For all other sources, we compare the full, non-discounted price of the sale
				// to the limit stored in the sysparams table.
				if ((v_RecipeSource == RECIP_SRC_HOSPITAL)			||
					(v_RecipeSource == RECIP_SRC_OLD_PEOPLE_HOUSE)	||
					(v_RecipeSource == RECIP_SRC_DENTIST)			||
					(v_RecipeSource == RECIP_SRC_PRIVATE)			||
					(FullPriceTotal >  OverseasMaxSaleAmt))
				{
					SetErrorVarArr (&v_ErrorCode,
									((PRIVATE_PHARMACY) ? ERR_MEMBER_LIVES_OVERSEAS : WARN_MEMBER_LIVES_OVERSEAS),
									0, 0, NULL, &ErrOverflow);
				}
			}	// Prescription drugs being sold to Maccabi member living overseas.


			// DonR 11Jul2012: Provide error/warning message if member is exceeding the daily limit on
			// buying prescription stuff, based on the pharmacy-type-specific limit set in sysparams.
			// Note that since we're looking only at current-day sales, we won't bother with old_member_id.
			// DonR 09Jun2014: For efficiency in case this feature is disabled (by setting the limit equal
			// to zero), make the whole section of code conditional on a non-zero daily limit.
			if ((v_RecipeSource != RECIP_SRC_NO_PRESC) && (MaxDailyBuy [Phrm_info.pharmacy_permission] > 0))
			{
				if (FullPriceTotal < MaxDailyBuy [Phrm_info.pharmacy_permission])	// If we're already over limit, don't check DB.
				{
					ExecSQL (	MAIN_DB, READ_AlreadyBoughtToday,
								&AlreadyBoughtToday,
								&v_MemberIdentification,	&SysDate,
								&v_PharmNum,				&v_IdentificationCode,
								END_OF_ARG_LIST											);

					// Don't bother with "real" error-checking here - at least for now.
					if (SQLCODE != 0)
						AlreadyBoughtToday = 0;
				}
				else AlreadyBoughtToday = 0;

				if ((FullPriceTotal + AlreadyBoughtToday) > MaxDailyBuy [Phrm_info.pharmacy_permission])
				{
					// DonR 12Sep2012: Separate message for Maccabi pharmacies.
					SetErrorVarArr (&v_ErrorCode,
									MACCABI_PHARMACY ? DAILY_PURCH_LIM_EXCD_MACCABI : DAILY_PURCHASE_LIMIT_EXCEEDED,
									0, 0, NULL, &ErrOverflow);
				}
			}	// Not a non-prescription sale, so need to check daily purchase limit.

		}	// Current action is a drug sale.


		// DonR 04Aug2010: For Nihul Tikrot calls, we want to know how many drugs in the requested
		// sale are actually sellable - so let's count how many do NOT have fatal errors. No point
		// in executing this conditionally, since it will take almost no time to execute.
		for (i = v_NumValidDrugLines = 0;
			 (i < v_NumOfDrugLinesRecs) && (v_NumValidDrugLines < MAX_TIKRA_CURR_SALE);
			 i++)
		{
			if (!SPres[i].DFatalErr)
				v_NumValidDrugLines++;
		}


		// If necessary, call the AS/400 "Tikrot" application to get Tikrot/Coupon/Subsidy information.
		NumTikrotLines	= NumSaleLines		= NumCouponLines		= FamilySalePrID_count	= 0;
		TikrotStatus	= TikrotRPC_Called	= TikrotRPC_Error		= 0;
		*TikrotHeader	= *TikrotPriorSales	= *TikrotCurrentSale	= (char)0;

		gettimeofday (&EventTime[EVENT_START_TO_TIK], 0);

		// Contact AS/400 "Nihul Tikrot" application if:
		// (A) There hasn't been a fatal error for this sale/deletion.
		// (B) There is at least one valid drug line (i.e. with no fatal error) (DonR 04Aug2010)
		// (C) This is not a sale to an IDF soldier. (DonR 07Dec2011, "Tzahal" enhancement)
		// (D) We're not dealing with a non-prescription sale/deletion.
		// (E) Sale meets relevancy conditions OR current transaction is a valid sale deletion request.
		// (F) The global "Nihul Tikrot Enabled" flag is set TRUE.
		if ((!SetErrorVarArr (&v_ErrorCode, 0, 0, 0, NULL, &ErrOverflow))	&&							// (A)
			(v_NumValidDrugLines	>  0)									&&							// (B)
			(Member.MemberTzahal	== 0)									&&							// (C)
			(v_RecipeSource			!= RECIP_SRC_NO_PRESC)					&&							// (D)
			(v_RecipeSource			!= RECIP_SRC_EMERGENCY_NO_RX)			&&							// (D)
			((MemberHasTikra || MemberHasCoupon || MemberBuyingTikMazon || AnIshurHasTikra)	||			// (E1)
			 ((v_ActionType == SALE_DELETION) && (DeletionValid))))										// (E2)
		{
			if (TikrotRPC_Enabled)																		// (F)
			{
				TikrotRPC_Called = 1;

				// Build list of up to 25 recent sales (in the last two days) to family.
				WritePtr	= TikrotPriorSales;
				Yesterday	= IncrementDate (SysDate, -1);

				gettimeofday (&EventTime[EVENT_FAM_SALES_START], 0);

				// Select cursor for recent family sales (used for "Nihul Tikrot" RPC call).
				// DonR 10Apr2011: Declare cursor only if it's needed.
				// DonR 09Jan2012: If the member's head-of-family Teudat Zehut number is
				// zero, skip the family-prescriptions check.
				if (v_FamilyHeadTZ > 0)
				{
					DeclareAndOpenCursorInto (	MAIN_DB, READ_FamilySales_cur,
												&FamilySalePrID,
												&v_FamilyHeadTZ,	&v_FamilyHeadTZCode,
												&Yesterday,			&SysDate,
												END_OF_ARG_LIST								);

					if (!SQLERR_error_test ())
					{
						// Fetch data and store in buffer.
						// For now at least, no real error checking; just quit when we hit anything.
						// We're already in "dirty read" mode, so we should be more or less OK.
						for ( ; ; )
						{
							FetchCursor (	MAIN_DB, READ_FamilySales_cur	);

							if (SQLCODE == 0)
							{
								WritePtr += sprintf (WritePtr, "%0*d", 9,	FamilySalePrID);
								if (++FamilySalePrID_count >= MAX_TIKRA_PREV_SALES)
									break;	// Buffer capacity reached.
							}
							else
							{
								break;
							}
						}
					}

					CloseCursor (	MAIN_DB, READ_FamilySales_cur	);
				}	// Non-zero head-of-family T.Z. number.

				gettimeofday (&EventTime[EVENT_FAM_SALES_END], 0);
				// Done with recent-sales list-building.

				// Build request header.
				// DonR 04Aug2010: Send the number of valid drug lines rather than the total line count.
				WritePtr = TikrotHeader;
				WritePtr += sprintf (WritePtr, "%0*d"  ,  9,	v_RecipeIdentifier				);
				WritePtr += sprintf (WritePtr, "%0*d"  ,  9,	v_DeletedPrID					);
				WritePtr += sprintf (WritePtr, "%0*d"  ,  9,	v_MemberIdentification			);
				WritePtr += sprintf (WritePtr, "%0*d"  ,  1,	v_IdentificationCode			);
				WritePtr += sprintf (WritePtr, "%0*d"  ,  2,	v_ActionType					);
				WritePtr += sprintf (WritePtr, "%0*d"  ,  8,	v_DeletedPrDate					);
				WritePtr += sprintf (WritePtr, "%0*d"  ,  9,	v_FamilyHeadTZ					);
				WritePtr += sprintf (WritePtr, "%0*d"  ,  1,	v_FamilyHeadTZCode				);
				WritePtr += sprintf (WritePtr, "%c"    ,		ONE_OR_BLANK (MemberHasCoupon)	);
				WritePtr += sprintf (WritePtr, "%+0*d" , 10,	v_DeletedPrSubAmt				);
				WritePtr += sprintf (WritePtr, "%0*d"  ,  2,	v_NumValidDrugLines				);
				WritePtr += sprintf (WritePtr, "%0*d"  ,  2,	FamilySalePrID_count			);


				// Build list of drugs (up to 50) in the current sale.
				// DonR 28Jun2010: If a drug has a fatal error, DON'T send it to AS/400.
				// DonR 04Aug2010: Send up to 50 *valid* drug lines.
				// 34 characters per Drug Line.
				WritePtr = TikrotCurrentSale;
				for (i = v_NumDrugLinesSent = 0;
					 (i < v_NumOfDrugLinesRecs) && (v_NumDrugLinesSent < MAX_TIKRA_CURR_SALE);
					 i++)
				{
					if (!SPres[i].DFatalErr)
					{
						// DonR 30Jul2024 User Story #338533: Largo Code Length is now a Sysparams variable.
						WritePtr += sprintf (WritePtr, "%0*d"  ,  NihulTikrotLargoLen,	SPres[i].DrugCode			);
						WritePtr += sprintf (WritePtr, "%c "   ,		SPres[i].DL.largo_type						);
						WritePtr += sprintf (WritePtr, "%c"    ,		ONE_OR_BLANK (SPres[i].DL.tikrat_mazon_flag));
						WritePtr += sprintf (WritePtr, "%0*d"  ,  5,	SPres[i].DL.parent_group_code				);

						// DonR 08Mar2017 CR #11036: Send Ishur Source even if the ishur didn't involve Tikra.
						WritePtr += sprintf (WritePtr, "%c",			DIGIT_OR_BLANK (SPres[i].SpecPrescNumSource));
//						WritePtr += sprintf (WritePtr, "%c",
//											 DIGIT_OR_BLANK (SPres[i].SpecPrescNumSource * SPres[i].IshurWithTikra)	);

						WritePtr += sprintf (WritePtr, "%0*d"  ,  3,	SPres[i].IshurTikraType						);
						WritePtr += sprintf (WritePtr, "%+0*d" , 10,	MemberPtnAmount[i]							);
						WritePtr += sprintf (WritePtr, "%0*d"  ,  2,	SPres[i].RetPartCode						);
						WritePtr += sprintf (WritePtr, " "		/* Fixed Price Flag - NIU */						);
						WritePtr += sprintf (WritePtr, "%0*d"  ,  1,	SPres[i].in_health_pack						);
						WritePtr += sprintf (WritePtr, "%0*d"  ,  2,	SPres[i].ret_part_source.insurance_used		);
						WritePtr += sprintf (WritePtr, " "		/* Deletion Tikra Flag - NIU */						);

						v_NumDrugLinesSent++;
					}	// Drug doesn't already have a fatal error.
				}


				// Finally (well, not quite finally), call the RPC to invoke the AS/400 "Tikrot" program.
				TikrotRPC_Error = CallTikrotSP (TikrotHeader,	TikrotCurrentSale,	TikrotPriorSales,
												&HeaderRtn,		&TikrotRtn,			&CurrentSaleRtn,	&CouponsRtn);

				// DonR 03Aug2010: If the Tikrot RPC call failed, change "Tikrot Called" from 1 to 2.
				if (TikrotRPC_Error != 0)
				{
					TikrotRPC_Called = 2;
				}

				// Pull values from output header.
				PosInBuff = HeaderRtn + 9;	// Ignore Prescription ID - we know it already!

				TikrotStatus = GetShort (&PosInBuff, 4);

				// DonR 06Jun2010: Orly Spiegel changed the status returned. Now 1 means that everything's
				// OK but no reductions in price were granted; 2 means that reductions in price were
				// granted; and any other value means that something went wrong.
				if ((TikrotStatus == 1) || (TikrotStatus == 2))
				{
					NumTikrotLines	= GetShort (&PosInBuff, 2);
					NumSaleLines	= GetShort (&PosInBuff, 2);
					NumCouponLines	= GetShort (&PosInBuff, 2);
				}
				else
				{
					NumTikrotLines = NumSaleLines = NumCouponLines = 0;

					// DonR 25Jun2020: In order to diagnose an intermittent problem in communicating with
					// the "ODBC listener" program on AS/400, dump HeaderRtn to log if TikrotStatus has
					// an unrecognized value.
					GerrLogMini (GerrId, "PID %d: Got HeaderRtn {%s} for Prescription ID %d.",
								 (int)getpid(), HeaderRtn, v_RecipeIdentifier);
				}

				// Decode Tikrot Lines output.
				// 59 characters/line.
				PosInBuff = TikrotRtn;
				for (i = 0; i < NumTikrotLines; i++)
				{
					TikraType			[i] = GetChar	(&PosInBuff);
					TikraBasket			[i] = GetShort	(&PosInBuff,  1);
					TikraInsurance		[i] = GetShort	(&PosInBuff,  2);
					TikraAggPrevPtn		[i] = GetLong	(&PosInBuff, 10);
					TikraAggPrevWaived	[i] = GetLong	(&PosInBuff, 10);
					TikraCurrPtn		[i] = GetLong	(&PosInBuff, 10);
					TikraCurrWaived		[i] = GetLong	(&PosInBuff, 10);
					TikraCurrLevel		[i] = GetLong	(&PosInBuff,  9);
					GetString (&PosInBuff, TikraPeriodDesc [i],  6);

					// DonR 18Oct2010: Convert AS/400 Windows (1255) Hebrew to DOS (862) Hebrew - and
					// then reverse it, since AS/400 has strange ways of dealing with Hebrew.
					WinHebToDosHeb ((unsigned char *)TikraPeriodDesc [i]);	// DonR 18Oct2010
					buf_reverse ((unsigned char *)TikraPeriodDesc [i], 6);

					// DonR 09Aug2016: Keep track of total Tikra discount sent to pharmacy.
					tikra_discount += TikraCurrWaived [i];
				}

				// Decode Current Sale Lines output.
				// 17 characters/line.
				PosInBuff = CurrentSaleRtn;
				for (i = 0; i < NumSaleLines; i++)
				{
					// DonR 30Jul2024 User Story #338533: Largo Code Length is now a Sysparams variable.
					PosInBuff += NihulTikrotLargoLen;	// Ignore Largo Code - array order will be unchanged!
					DrugTikraType		[i] = GetChar	(&PosInBuff);
					DrugCoupon			[i] = GetChar	(&PosInBuff);
					DrugRefundOffset	[i] = GetLong	(&PosInBuff, 10);

					// DonR 13Jul2010: Drug Refund Offset needs to be used to reduce the Member
					// Participation Amount for sale deletions. Since this is sent from AS/400
					// as a negative number, we add it to the Participation Amount.
					// DonR 14Jul2010: Oops! Forgot that MemberPtnAmount is a negative number
					// for deletions; so we need to *subtract* the Refund Offset rather than add it.
					if (v_ActionType == SALE_DELETION)
						MemberPtnAmount [i] -= DrugRefundOffset [i];
				}

				// Decode Coupon Lines output.
				PosInBuff = CouponsRtn;
				for (i = 0; i < NumCouponLines; i++)
				{
					TikraCouponCode		[i] = GetShort	(&PosInBuff,  3);
					TikraCouponAmt		[i] = GetLong	(&PosInBuff, 10);

					// DonR 09Aug2016: Keep track of total subsidy amount sent to pharmacy.
					subsidy_amount += TikraCouponAmt [i];
				}
			}	// "Nihul Tikrot" calls are enabled.
			else
			{
				GerrLogMini (GerrId, "AS/400 not contacted - \"Nihul Tikrot\" calls are disabled!");
			}
		}	// Need to call AS/400 "Tikrot" program via ODBC RPC.


		// DonR 26Oct2014: "Maccabi Pharmacy Sale at 100% Participation Not Allowed" is now conditional: if
		// the member could receive a discount by signing up for a "shaban" service (Zahav or Maccabi Sheli)
		// or the member is getting a member-based discount for this drug (either because of a global percentage
		// discount or because of a serious illness/accident discount), we want to allow the sale to go through.
		if (v_ActionType != SALE_DELETION)
		{
			for (i = 0; i < v_NumOfDrugLinesRecs; i++)
			{
				// For each drug in the sale, see if we've previously assigned the relevant "you can't buy
				// this at full price" error.
				// DonR 23Nov2014: Note that we're now using a non-severe "placeholder" error code, which
				// will either be replaced by the real thing or else deleted.
				if (SetErrorVarExists (ERR_MACCABI_100_PER_CENT, SPres[i].DrugCode))
				{
					Permit100PercentSale = 0;	// Default: we will in fact prevent the sale.
					
					// DonR 03Dec2014: Per Iris Shaya, this change is no longer contingent on the member
					// receiving a "shovar" subsidy for the drug.
					// DonR 19Jan2015: Added a new non-reported error code for members who do have
					// "shaban" but didn't get a discount - presumably because they just signed up
					// and the 90-day waiting period hasn't been completed.
					// DonR 08Jun2016: If the member is getting a full or partial member-based discount
					// (based on an all-drugs percentage or on a match between serious illness and the
					// drug in question), permit the sale to go through. Also, combine multiple tests
					// into a single "if".
					// Note that the first condition will be true only for work accidents, where we sometimes
					// give a member discount even if the Health Basket parameter is zero. Otherwise we give
					// memer discounts only when participation is already less than 100%.
					if ((SPres[i].AdditionToPrice > 0)												||	// Member has a member/illness-based discount.
						(SetErrorVarExists (ERR_DISCOUNT_IF_HAD_ZAHAV,		SPres[i].DrugCode))		||	// Zahav discount possible.
						(SetErrorVarExists (ERR_DISCOUNT_IF_HAD_YAHALOM,	SPres[i].DrugCode))		||	// Maccabi Sheli discount possible.
						(SetErrorVarExists (SHABAN_NOT_YET_APPLICABLE,		SPres[i].DrugCode)))		// Member has signed up for "shaban" but it hasn't kicked in yet.
					{
						Permit100PercentSale = 1;	// Let the sale go through!
					}
					else

					// For the moment at least, we don't give a message for "Member could get a Specialist
					// discount with supplemental insurance" - so we need to check for this condition with
					// a separate SQL query. We already know that member *isn't* getting a specialist discount
					// on this sale, so we don't need to compare the spclty_largo_prcnt row's insurance with
					// the member's insurance - just that the spclty_largo_prcnt row's insurance type is
					// "Y" or "Z" (= Maccabi Sheli or Magen Zahav).
					{
						// This test applies to Maccabi prescribing physicians and specialist drugs only.
						if ((v_TypeDoctorIdentification != 1) &&
							((SPres[i].DL.specialist_drug > 0) || (PR_AsIf_Preferred_SpDrug[i] > 0)))
						{
							ExecSQL (	MAIN_DB, READ_check_for_specialist_discount_to_enable_full_price_sale,
										&RowsFound,
										&DocID,					&ROW_NOT_DELETED,
										&SPres[i].DrugCode,		&PR_AsIf_Preferred_Largo[i],
										&Member.MemberMaccabi,	&Member.MemberHova,
										&Member.MemberKeva,		&ROW_NOT_DELETED,
										END_OF_ARG_LIST											);

							Conflict_Test (reStart);

							if (SQLERR_error_test ())
							{
								SetErrorVarArr (&v_ErrorCode, ERR_DATABASE_ERROR, 0, 0, NULL, &ErrOverflow);
								break;
							}
							else
							{
								if (RowsFound > 0)
								{
									Permit100PercentSale = 1;	// Specialist discount possible - let the sale go through!
								}
							}
						}	// Maccabi Doctor and specialist drug.
					}	// Didn't find a "hit" on drug_extension or member/illness-based possibilities.


					// At this point, we have two possibilities: either Permit100PercentSale == 1, in which case we want
					// to suppress the "sale forbidden" messages; or Permit100PercentSale == 0, in which case we want to
					// go ahead and prevent the entire drug sale from going through.
					// DonR 24Dec2014: COULD_BUY_WITHOUT_PRESC is no longer relevant, since ERR_MACCABI_100_PER_CENT is
					// assigned only if the drug is a prescription-only one.
					if (Permit100PercentSale)
					{
						SetErrorVarDelete (ERR_MACCABI_100_PER_CENT,	SPres[i].DrugCode);
//						SetErrorVarDelete (COULD_BUY_WITHOUT_PRESC,		SPres[i].DrugCode);
					}
					else
					{
						// Swap in the real error code in place of the "placeholder".
						SetErrorVarSwap (&SPres[i].DrugAnswerCode,
										 ERR_MACCABI_100_PER_CENT,
										 ERR_MACCABI_100PCT_FORBIDDEN,
										 SPres[i].DrugCode,
										 NULL);

						// Flag the entire drug sale as invalid.
						SetErrorVarArr (&v_ErrorCode, ERR_DRUG_S_WRONG_IN_PRESC, 0, 0, NULL, &ErrOverflow);
					}

				}	// "Sale at 100%" placeholder error was previously assigned for this drug.
			}	// Loop through all drugs in sale.
		}	// Current action is a drug-sale request, so check for "100% forbidden" situations.
		// DonR 26Oct2014 end.


		// DonR 30Aug2015: Check if this member needs a Form 29-G for this drug; and whether a current
		// Form 29-G will expire soon.
// DonR 21Oct2015: TEMPORARY - CONDITIONALLY DISABLED, SO PROGRAM CAN BE PUT INTO PRODUCTION BEFORE THIS FEATURE IS TESTED.
// (Form29gMessagesEnabled will be TRUE if the member_drug_29g table is populated.)
// GerrLogMini (GerrId, "%s to check Form 29G because checking %s enabled.", (Form29gMessagesEnabled) ? "About" : "Not going", (Form29gMessagesEnabled) ? "is" : "is NOT");
if (Form29gMessagesEnabled)
{
		if (v_ActionType == DRUG_SALE)
		{
			for (i = 0; i < v_NumOfDrugLinesRecs; i++)
			{
				FunctionError = CheckMember29G (&Member, &SPres[i], &Phrm_info);

				if (FunctionError == ERR_DATABASE_ERROR)
				{
					SetErrorVarArr (&v_ErrorCode, ERR_DATABASE_ERROR, 0, 0, NULL, &ErrOverflow);
				}
				else
				{
					if (FunctionError > 0)
					{
						SetErrorVarArr (&SPres[i].DrugAnswerCode, FunctionError, SPres[i].DrugCode, i + 1, NULL, &ErrOverflow);
					}
					else
					{
						if (FunctionError == -1)
						{
							reStart = MAC_TRUE;
							break;
						}
					}
				}
			}	// Loop through drugs checking 29-G status.
		}	// This is a drug-sale transaction, so we need to check Form 29-G status.
}	// TEMPORARY!



		// DonR 04Sep2003: Members pay via credit line (i.e. not through pharmacy) under the following conditions:
		//
		// 1) Drugs were prescribed by a doctor (not over-the-counter), and
		//
		// 2) Member has arranged a bank credit line with Maccabi, and
		//
		// 3) Member has chronic illness, no matter what pharmacy prescription is filled at.
		//
		// 4) Pharmacy has its "credit" flag set, no matter whether member is chronically ill or not.
		//
		// "Card passed" logic is irrelevant here.
		//
		// DonR 10Aug2004: Non-chronically-ill patients buying drugs at pharmacies that work
		// with credit lines will be given the option of paying through the credit line even
		// for non-prescription drugs. In this case we'll pass a 10 to the pharmacy, and the
		// pharmacy will pass back a 9 in Transaction 2005 to indicate that the member is
		// going to pay through his/her credit line.
		//
		// DonR 05Aug2009: Credit Type 9 is now sent to all pharmacies, Maccabi and private.
		if ((v_RecipeSource			!= RECIP_SRC_NO_PRESC)										&&	// NOT non-prescription
			(v_RecipeSource			!= RECIP_SRC_EMERGENCY_NO_RX)								&&	// NOT emergency supply
			((v_CreditYesNo			== 2) || (v_CreditYesNo == 4))								&&	// Member has Credit Line
			(v_insurance_status		== 1)														&&	// Chronic illness
			(((v_MacCent_cred_flg	!= 7) && (v_MacCent_cred_flg != 2)) || PRIVATE_PHARMACY))		// Per Iris Shaya 09Nov2005.
		{
			// DonR 10Apr2013: Add additional conditions for automatic credit-line payment. If these
			// are not met, send a message to the pharmacy and fall through to the next (optional
			// credit-line payment) section of code. Anyone otherwise eligible pays this way if
			// his/her magnetic card is used; otherwise, the purchase must be made at a Maccabi
			// pharmacy, and the member must be getting drugs through an old-age home with Credit Flag
			// equal to 1, 5, or 9.
			if (MEMBER_USED_MAG_CARD	||
				((MACCABI_PHARMACY && ((v_MacCent_cred_flg == 1) || (v_MacCent_cred_flg == 5) || (v_MacCent_cred_flg == 9)))))
			{
				v_CreditYesNo = 9;	// Payment is direct from bank to pharmacy.
			}
			else
			{
				// DonR 27May2013: Send Error 6160 only if this is a drug sale.
				if (v_ActionType == DRUG_SALE)
					SetErrorVarArr (&v_ErrorCode, ERR_AUTO_CREDIT_PMT_W_CARD_MSG, 0, 0, NULL, &ErrOverflow);
			}
		}

		// DonR 10Apr2013: It is now possible that someone can meet most of the criteria for Credit Type 9
		// (automatic payment through credit line), but not have this option set because s/he didn't have
		// a magnetic card at the pharmacy. In this case, we want to fall through to the next section,
		// since the member may still qualify for optional credit-line payment. Accordingly, the next block
		// of code is no longer in an "else"; if v_CreditYesNo has been set to 9, the "if" will be false
		// in any case.

		// DonR 14Oct2004: If Client Location Code is greater than one (e.g. member
		// lives in an old-age home), offer payment by credit line only if that
		// location has a Credit Flag of 9 and the prescription is being filled
		// at a Maccabi pharmacy; in this case, we don't worry about
		// the member's Maccabi Card or the pharmacy's Credit Flag. If the Client
		// Location Code is zero or one, operate "normally" - that is, offer credit
		// if the card has been scanned.
		// DonR 10Jan2005: 1 is a valid card date!
		// DonR 30Oct2006: Per Iris Shaya, added Maccabi Centers Credit Flag of 5
		// to the condition (to join with 9) to pay by credit line w/o using member's card.
		if (((v_CreditYesNo			== 2) || (v_CreditYesNo == 4))	&&	// Member has Credit Line
			(v_CreditPharm			!= 0)							&&	// Pharmacy works with Credit Lines
			(((v_MacCent_cred_flg	!= 7) &&
				(v_MacCent_cred_flg	!= 2))	 || (PRIVATE_PHARMACY))	&&	// Per Iris Shaya 09Nov2005.
			(MEMBER_USED_MAG_CARD ||						// Patient used magnetic card OR...
				((v_ClientLocationCode >  1) && ((v_MacCent_cred_flg == 5) ||
												 (v_MacCent_cred_flg == 9))	&& (MACCABI_PHARMACY))))
															// ...Magnetic card not required.
		{
			// 23Nov2005: Per Iris Shaya, Member Credit type 4 indicates that member
			// prefers to pay through multiple payments ("tashlumim"), even though
			// they are known to be a tool of the devil.
			v_CreditYesNo = (v_CreditYesNo == 4) ? 11 : 10;	// Pharmacy will give option to patient
															// to pay through Credit Line.

			// DonR 20Mar2006: Don't perform family purchase check for Member ID zero!
			// Also, this check applies only to purchases, not to deletions.
			// NOTE: As of 1 April 2011, credit_type_code will be populated in prescription_drugs;
			// this should mean that the prescriptions table can be dropped from this query.
			// DonR 03Apr2011: Revamped SELECT for better performance - we now need to use only
			// members and prescription_drugs.
			// DonR 07Apr2019: This SELECT needs the following changes: (A) Instead of looking at the
			// credit_type_code in prescription_drugs, we should be looking at prescriptions/credit_type_used,
			// which reflects the member's decision whether to pay by credit line; and (B) the value to look for
			// is 9 -  the only alternative is 0, which means the member paid by cash or credit card.
			if ((v_RecipeSource == RECIP_SRC_NO_PRESC)	&&
				(v_MemberIdentification > 0)			&&
				(v_ActionType != SALE_DELETION))
			{
				v_FirstOfMonth = SysDate + 1 - (SysDate % 100);

				ExecSQL (	MAIN_DB, READ_check_family_credit_used,
							&v_FamilyCreditUsed,
							&v_FamilyHeadTZ,	&v_FamilyHeadTZCode,
							&v_FirstOfMonth,	END_OF_ARG_LIST			);

				v_FamilyCreditInd = ColumnOutputLengths [1];

				if (SQLMD_is_null (v_FamilyCreditInd))
					v_FamilyCreditUsed = 0;

				// Compute anticipated total of the current sale.
				v_CreditToBeUsed = 0;
				for (i = 0; i < v_NumOfDrugLinesRecs; i++)
				{
					if (SPres[i].DFatalErr == MAC_TRUE )
						continue;

					// Compute participation per OP.
					// DonR 08Jan2012: If "Meishar" has given a fixed price of zero, that's a real
					// fixed price.
					if ((SPres[i].PriceSwap == 0) && (SPres[i].ret_part_source.table != FROM_GADGET_APP))
					{
						// DonR 28Mar2010: Since we're only executing this block of code if the current
						// purchase is being made without a prescription - and non-prescription sales
						// are always charged 100% participation - the business about the 12-shekel
						// minimum when participation is 15% should really be irrelevant. This is worth
						// noting since I've just been told that the 12-skekel minimum has been changed
						// to 15 shekels.
						GET_PARTICIPATING_PERCENT (SPres[i].RetPartCode, &percent1);
						v_CreditThisDrug = (int) ((float)SPres[i].RetOpDrugPrice * (float)percent1 / 10000.0);
						if ((v_CreditThisDrug < 1200) && (SPres[i].RetPartCode == PARTI_CODE_TWO))
							v_CreditThisDrug = 1200;	// Minimum NIS 12.00 for "normal" 15% participation.
					}
					else
					{
						v_CreditThisDrug = SPres[i].PriceSwap;
					}

					// Multiply by packages requested.
					if (SPres[i].Op > 1)
						v_CreditThisDrug *= SPres[i].Op;

					// Compute member discount - although I don't really think there are any
					// discounts on non-prescription medications!
					if (SPres[i].AdditionToPrice  > 0)
						v_CreditThisDrug -= (int) ((float)v_CreditThisDrug * (float)SPres[i].AdditionToPrice / 10000.0);

					// Finally, add the current drug's net-to-member price to the total.
					v_CreditToBeUsed += v_CreditThisDrug;
				}	// Loop through drugs in current purchase request.


				// DonR 10Apr2019 CR #???: Maximum monthly family OTC credit is now a sysparams parameter:
				// sysparams/max_otc_credit_mon.
				if ((v_FamilyCreditUsed + v_CreditToBeUsed) > MaxFamilyOtcCreditPerMonth)
				{
					v_CreditYesNo = 0;	// No more use of credit line this month!
					SetErrorVarArr (&v_ErrorCode, ERR_NO_MORE_OTC_CREDIT, 0, 0, NULL, &ErrOverflow);

					GerrLogFnameMini ("otc_credit_log",
										GerrId,
										"PrID %d prev %d, now %d, tot %d, err = %d - DENIED CREDIT!\t\t\t\t\t\t",
										v_RecipeIdentifier,
										v_FamilyCreditUsed,
										v_CreditToBeUsed,
										(v_FamilyCreditUsed + v_CreditToBeUsed),
										SQLCODE);
				}	// Monthly OTC credit-line usage exceeded.
			}	// Current request is for OTC drugs.
		}	// Credit conditions met.

		else
		{
			// This is the "catch-all" default: if member is not an eligible chronic patient
			// and doesn't qualify for "normal" credit-line payment, s/he has to pay by
			// cash or credit card. Note that any value other than 2 or 4 in the member's
			// credit_type_code field should land him/her here!
			// DonR 10Apr2013: We can now get here if member qualified for automatic payment via
			// credit line (v_CreditYesNo == 9); accordingly, zero v_CreditYesNo conditionally.
			if (v_CreditYesNo != 9)
				v_CreditYesNo = 0;	// Payment is made to Pharmacy.
		}


		// DonR 19Jan2005: See if there's anything in pharmacy_message for the pharmacy to see.
		v_Message = MAC_FALS;

		ExecSQL (	MAIN_DB, READ_check_pharmacy_message_count,
					&RowsFound, &v_PharmNum, END_OF_ARG_LIST		);

		Conflict_Test (reStart);

		if ((SQLCODE == 0) && (RowsFound > 0))
			v_Message = MAC_TRUE;

		if (SQLERR_error_test ())
		{
			SetErrorVarArr (&v_ErrorCode, ERR_DATABASE_ERROR, 0, 0, NULL, &ErrOverflow);
		}


		// DonR 10Apr2013: If member is buying prescription drugs without passing a magnetic card AND the
		// prescriptions are not in the doctor-prescription table (and some other conditions are met), give
		// the pharmacy a message indicating that this is a purchase that should be permitted only with the
		// use of a magnetic card. The error is of severity 2, which means that the sale is permitted, but
		// the pharmacist must authorize it specifically. We neet to check doctor_presc only for the first
		// drug on the list, since pharmacies sell either against prescriptions sent via Transaction 2001
		// or against paper prescriptions, but never against a combination of both at the same time.
		// Note regarding the v_MacCent_cred_flg test: If this is an ordinary purchase by a member who does
		// not live in an old-age home, v_ClientLocationCode will be 1 and v_MacCent_cred_flg will be zero;
		// since only values of v_ClientLocationCode > 1 will have non-zero v_MacCent_cred_flg values, there
		// is no need to test v_ClientLocationCode here. A value of 1, 5, 7, or 9 in v_MacCent_cred_flg
		// *disables* this test.
		// DonR 24Dec2013: This test now applies to all pharmacies. Also, give different error codes
		// depending on Prescription Source.
		// DonR 26Dec2013: If the sale is being rejected because the person buying drugs isn't a Maccabi
		// member, don't bother checking the prescription status.
		// DonR 27Apr2015: Added capability of working with the new doctor_presc table instead of prescr_wr_new.
		// DonR 28Oct2015 CR #6245: This test now applies in two cases: either the purchase is at a Maccabi
		// pharmacy, OR the member has not used his/her magnetic card. For Maccabi pharmacies, we will send
		// a new error code with severity = 4, which requires two pharmacy workers to authorize the sale.
		if ((v_ActionType		== DRUG_SALE)					&&
			(v_ErrorCode		!= ERR_MEMBER_NOT_ELEGEBLE)		&&	// DonR 26Dec2013
			(v_RecipeSource		!= RECIP_SRC_NO_PRESC)			&&

			// DonR 28Oct2015 part 1 begin.
			(	(MACCABI_PHARMACY)						||

				(	(v_MacCent_cred_flg	!= 1)	&&
					(v_MacCent_cred_flg	!= 5)	&&
					(v_MacCent_cred_flg	!= 7)	&&
					(v_MacCent_cred_flg	!= 9)	&&
					(!MEMBER_USED_MAG_CARD)			)		)		)
			// DonR 28Oct2015 part 1 end.
		{
			RowsFound = 0;	// Default in case we don't need to do a lookup.

			// If the prescription is not from a Maccabi doctor, don't bother looking it up - we
			// already know that it's not there.
			// DonR 05Nov2014: Changing the 799/800 boundary in economypri to 999/1000. This is temporary, as the
			// code is being expanded to six digits. When the next phase comes in, the boundary will
			// be at 19999/20000.
			// DonR 31Jan2018: Instead of selecting a list of related Largo Codes from economypri, get the list
			// from drug_list. This will give exactly the same result, but avoids duplicating logic that checks
			// the economypri system_code and del_flg fields. This way, the logic that deals with this table is
			// all contained in the "pipeline" programs As400UnixServer and As400UnixClient.
			if (v_RecipeSource == RECIP_SRC_MACABI_DOCTOR)
			{
				SET_ISOLATION_DIRTY;

				PW_Doc_Largo_Code	= PR_Original_Largo_Code	[0];
				PW_DoctorPrID		= PR_DoctorPrID				[0];

				ExecSQL (	MAIN_DB, READ_check_first_doctor_prescription_is_in_database,
							&RowsFound,
							&v_MemberIdentification,	&PW_DoctorPrID,
							&EarliestPrescrDate,		&LatestPrescrDate,
							&PW_Doc_Largo_Code,			&SPres[0].DL.economypri_group,
							&DocID,						&v_IdentificationCode,
							END_OF_ARG_LIST													);

				// If anything went wrong with the SQL query, just assume that we found something.
				if (SQLCODE != 0)
					RowsFound = 1;

				SET_ISOLATION_COMMITTED;
			}	// Prescriptions are from a Maccabi doctor, and thus should be found in doctor_presc.

			// If prescriptions are from a non-Maccabi doctor, or were not found in doctor_presc, notify
			// pharmacy that something is amiss.
			// DonR 28Oct2015 CR #6245 part 2: This test now applies in two cases: either the purchase is
			// at a Maccabi pharmacy, OR the member has not used his/her magnetic card. For Maccabi
			// pharmacies, we will send a new error code with severity = 4, which requires two pharmacy
			// workers to authorize the sale.
			// DonR 31Dec2015: The new error code HAND_RX_MACCABI_NEED_2_CARDS is sent *in addition to* the old
			// error codes, not instead of them.
			// DonR 03Jan2016 HOT FIX: The "old" error codes are sent *only* if magnetic card wasn't passed,
			// even if the sale is to a Maccabi pharmacy.
			if (RowsFound == 0)
			{
				// 27Jan2016: Per Iris Shaya, add conditions for sending HAND_RX_MACCABI_NEED_2_CARDS: At least
				// one item in the sale request must be a "treatment", and the purchaser must be either an
				// ordinary member (i.e. not living in an old-age home or similar facility) or the member's
				// facility must have a "first center type" other than "02".
				// DonR 01Jun2021: At least for now, I have *not* added anything here to deal with the new
				// First Center Type "03" (= pickup).
				if ((MACCABI_PHARMACY)			&&
					(ReqIncludesTreatment)		&&
					((v_ClientLocationCode == 1) || (strncmp (first_center_type, "02", 2))))
				{
					SetErrorVarArr (&v_ErrorCode, HAND_RX_MACCABI_NEED_2_CARDS, 0, 0, NULL, &ErrOverflow);
				}

				if ((v_MacCent_cred_flg	!= 1)	&&
					(v_MacCent_cred_flg	!= 5)	&&
					(v_MacCent_cred_flg	!= 7)	&&
					(v_MacCent_cred_flg	!= 9)	&&
					(!MEMBER_USED_MAG_CARD))
				{
					switch (v_RecipeSource)
					{
						case RECIP_SRC_MACABI_DOCTOR:		ErrorCodeToAssign = ERR_MUST_PASS_2_CARDS_TO_BUY;		break;

						case RECIP_SRC_HOSPITAL:
						case RECIP_SRC_OLD_PEOPLE_HOUSE:
						case RECIP_SRC_PRIVATE:				ErrorCodeToAssign = WHITE_PRESCRIPTION_PASS_2_CARDS;	break;

						default:							ErrorCodeToAssign = HAND_PRESCRIPTION_PASS_2_CARDS;		break;
					}	// Decide which error code to assign based on Prescription Source.

					SetErrorVarArr (&v_ErrorCode, ErrorCodeToAssign, 0, 0, NULL, &ErrOverflow);
				}	// Magnetic card was not passed.
			}	// No electronic prescription found.

			else	// We *did* find a doctor prescription in the database. 
			{
				// DonR 04Jan2016, CR #6838: Give a message to pharmacy to suggest swiping member's magnetic card.
				if ((v_MacCent_cred_flg	!= 1)	&&
					(v_MacCent_cred_flg	!= 5)	&&
					(v_MacCent_cred_flg	!= 7)	&&
					(v_MacCent_cred_flg	!= 9)	&&
					(!MEMBER_USED_MAG_CARD))
				{
					SetErrorVarArr (&v_ErrorCode, RX_FOUND_BUT_CARD_NOT_PASSED, 0, 0, NULL, &ErrOverflow);
				}
			}	// Electronic prescription *was* found.

		}	// Prescription drug sale without member's magnetic card having been passed OR at a Maccabi pharmacy.


		// DonR 08Feb2016, CR 36237: If at least one doctor prescription in this sale request has already been
		// partially filled, send a message to the (Maccabi) pharmacy. If member's magnetic card was used, the
		// message will have severity 2 (requiring one pharmacist to swipe his/her card); if member's magnetic
		// card was not used, the message will be the same but will have severity 4, requiring two pharmacists'
		// cards to be swiped.
		if ((v_ActionType		== DRUG_SALE)					&&
			(v_RecipeSource		== RECIP_SRC_MACABI_DOCTOR)		&&
			(MACCABI_PHARMACY))
		{
			for (i = found_partially_sold_rx = 0; i < v_NumOfDrugLinesRecs ; i++)
			{
				// Don't check if Doctor Prescription ID is zero.
				if (PR_DoctorPrID [i] == 0)
					continue;

				// To save on database access time, check to see if the current prescription has already
				// been checked.
                for (j = rx_already_checked = 0; j < i; j++)
				{
					// (NOTE: For Transaction 6003, this "if" will need to look at Doctor ID as well as Prescription ID.)
					if (PR_DoctorPrID [i] == PR_DoctorPrID [j])
					{
						rx_already_checked = 1;
						break;
					}
				}	// Loop to check if the current doctor prescription has already been looked up.

				// If this is a repeat of a doctor prescription found earlier in the list, skip to
				// the next drug line.
				if (rx_already_checked)
					continue;

				// If we get here, the doctor prescription at position i is non-zero and has not been checked
				// for previous partial sales yet.
				found_partially_sold_rx = CheckForPartiallySoldDoctorPrescription (v_MemberIdentification,
																				   v_IdentificationCode,
																				   DocID,
																				   PR_DoctorPrID [i],
																				   EarliestPrescrDate,
																				   LatestPrescrDate);

				// If we've found one partially-sold prescription, there's no need to look for more.
				if (found_partially_sold_rx)
					break;
			}	// Loop through drugs in this sale to check for at least one partially-sold prescription.

			if (found_partially_sold_rx)
			{
				SetErrorVarArr (&v_ErrorCode, MEMBER_USED_MAG_CARD ? PREV_PARTIAL_SALE_NEED_1_CARD : PREV_PARTIAL_SALE_NEED_2_CARDS, 0, 0, NULL, &ErrOverflow);
			}

		}	// Need to test for presence of partially-sold prescription(s) in sale request.
		// DonR 08Feb2016, CR 36237 end.


		// DonR 01Sep2013: In case we hit a fatal error but didn't assign Error 6089 (which
		// pharmacies use to indicate a "chasima" for a drug), make sure that we did assign it.
		// (SetErrorVarArr is smart enough not to set the same error twice, so we don't actually
		// need to check whether we've already assigned it.)
		for (i = 0; i < v_NumOfDrugLinesRecs ; i++)
		{
			if (SPres[i].DFatalErr)
			{
				SetErrorVarArr (&v_ErrorCode, ERR_DRUG_S_WRONG_IN_PRESC, 0, 0, NULL, &ErrOverflow);
				break;
			}
		}

		// UPDATE TABLES.
		// Set up dummy loop to facilitate error-handling.
		do
		{
			// DonR 29Jul2003: If we've already hit a fatal database error, skip
			// past the record-writing stuff.
			if (v_ErrorCode == ERR_DATABASE_ERROR)
				break;

			// DonR 20Jul2010: For sale deletions, the Credit Type Used value from the original sale
			// overrides whatever we've calculated for the current transaction's credit type.
			if (v_ActionType == SALE_DELETION)
			{
				v_CreditYesNo = v_DeletedPrCredit;
			}

			// Insert into PRESCRIPTIONS table.
			// DonR Feb2023 User Story #232220: Add paid_for to columns INSERTed.
			// DonR 20Apr2023 User Story #432608: Add dummy value for Magento Order Number.
			ExecSQL (	MAIN_DB, INS_prescriptions,
						&v_PharmNum,			&v_InstituteCode,
						&v_PriceListCode,		&v_MemberIdentification,
						&v_IdentificationCode,	&v_MemberBelongCode,
						&DocID,					&v_TypeDoctorIdentification,
						&v_RecipeSource,		&v_RecipeIdentifier,

						&v_SpecialConfNum,		&v_SpecialConfSource,
						&v_ConfirmationDate,	&v_ConfirmationHour,
						&v_ErrorCode,			&v_NumOfDrugLinesRecs,
						&v_CreditYesNo,			&v_MoveCard,
						&ShortZero,				&v_TerminalNum,

						&IntZero,				&IntZero,
						&IntZero,				&Member.maccabi_code,
						&IntZero,				&NOT_REPORTED_TO_AS400,
						&ShortZero,				&ShortZero,
						&ShortZero,				&v_ClientLocationCode,

						&v_DoctorLocationCode,	&ShortZero,
						&v_PriceListCode,		&IntZero,
						&IntZero,				&v_MsgExistsFlg,
						&DRUG_NOT_DELIVERED,	&ROW_NOT_DELETED,
						&IntZero,				&ShortZero,

						&v_ActionType,			&v_DeletedPrID,
						&v_DeletedPrPharm,		&v_DeletedPrDate,
						&v_DeletedPrPhID,		&v_CardOwnerID,
						&v_CardOwnerIDCode,		&ShortZero,
						&ShortZero,				&ShortZero,

						&TikrotRPC_Called,		&TikrotRPC_Error,
						&tikra_discount,		&subsidy_amount,
						&OriginCode,			&ShortZero,
						&TikrotStatus,			&Member.insurance_type,
						&LongZero,				&ShortZero,

						&ShortZero,				&Member.darkonai_type,
						&ShortZero,				&LongZero,					// DonR Feb2023 User Story #232220 & 20Apr2023 User Story #432608.
						END_OF_ARG_LIST										);

			Conflict_Test (reStart);

			if (SQLERR_error_test ())
			{
//				GerrLogMini (GerrId, "Error %d inserting Presc ID %d to prescriptions table; SQLERRM = {%s}, DelPrId = %d, Action = %d.",
//							 SQLCODE, v_RecipeIdentifier, sqlca.sqlerrm, v_DeletedPrID, v_ActionType);
				if (SQLERR_code_cmp (SQLERR_not_unique) == MAC_TRUE)
					GerrLogMini (GerrId, "Duplicate key inserting %d to prescriptions.", v_RecipeIdentifier);
				SetErrorVarArr (&v_ErrorCode, ERR_DATABASE_ERROR, 0, 0, NULL, &ErrOverflow);
				break;
			}


			// Insert into PRESCRIPTION_DRUGS table.
			for (i = 0; i < v_NumOfDrugLinesRecs ; i++)
			{
				// Prepare S.Q.L. variables.
				LineNum			= i + 1;
				PrsLinP			= &SPres[i];
				StopUseDate		= AddDays (SysDate, PrsLinP->Duration);
				StopBloodDate	= AddDays (SysDate, (PrsLinP->Duration + PrsLinP->DL.t_half));

				PW_Doc_Largo_Code	= PR_Original_Largo_Code	[i];
				PW_DoctorPrID		= PR_DoctorPrID				[i];
				PW_DosesPerDay		= PR_DosesPerDay			[i];
				PW_Pr_Date			= PR_Date					[i];
				sprintf (v_DrugTikraType, "%c", DrugTikraType [i]);	// DonR 09Jul2020 BUG FIX: ODBC doesn't support single-character input columns, so tikra_type_code wasn't be written to DB!
				DL_TikratMazonFlag	= SPres[i].DL.tikrat_mazon_flag;
				DL_economypri_grp	= SPres[i].DL.economypri_group;
				MemberPtnAmt		= MemberPtnAmount			[i];
				v_DrugSubsidized	= (DrugCoupon[i] == '1') ? 1 : 0;

				// DonR 11Jul2011: In case an error has been set and subsequently deleted, reset drug_answer_code
				// with the most severe error found for this drug.
				SetErrorVarTop (&PrsLinP->DrugAnswerCode, PrsLinP->DrugCode, NULL);

				// DonR 14Jan2009: We now write the substitution-permitted data using two variables:
				// first, a new flag explaining why a non-preferred drug was sold; and second, the
				// reason, if any, supplied by the pharmacy. The pharmacy-supplied value of 99
				// accordingly needs to be truncated to 9.
				PW_SubstPermitted = (SPres[i].WhyNonPreferredSold * 10) + (PR_WhyNonPreferredSold[i] % 10);

				// DonR 24Aug2010: If this is a deletion, we've already read the original sale's
				// Prescriptions-Written ID - so don't retrieve it from the database.
				if (v_ActionType == SALE_DELETION)
				{
					PW_ID = DeletedPrPRW_ID	[i];
				}
				else
				{
					// If this was not an OTC sale, try reading the doctor_presc row to get various status fields.
					// DonR 04Mar2015: Look up doctor prescriptions only for v_RecipeSource == RECIP_SRC_MACABI_DOCTOR.
					// Also, if the pharmacy's Doctor Prescription Date is incorrect, load it from the doctor-prescription
					// table.
					if (v_RecipeSource == RECIP_SRC_MACABI_DOCTOR)	// Other sources won't be in the database.
					{
						// DonR 14Mar2005: Avoid unnecessary lock errors by downgrading "isolation" for this lookup.
						// DonR 21May2013: Add fields to determine if this was a doctor prescription
						// lacking dosage, or a non-daily prescription.
						// DonR 21Jul2015: re-ordered WHERE clause to conform to the doctor_presc unique index.
						SET_ISOLATION_DIRTY;

						// DonR 19Aug2015: If the pharmacy has sent a Doctor Prescription ID of zero, don't bother trying to read
						// from doctor_presc - it just creates extra work for the database server.
						if (PW_DoctorPrID < 1)
						{
							FORCE_NOT_FOUND;	// Force a "not found" result.
						}

						else
						{	// Pharmacy did supply a Doctor Prescription ID, so try the lookup...
							// ...except that if pharmacy didn't supply a possibly-valid Prescription Date, fall
							// through to the second SQL without bothering to look up by the specific date.
							if (PW_Pr_Date < 20150101)
							{
								FORCE_NOT_FOUND;	// Force a "not found" result.
							}
							else
							{
								PW_ID = 0;	// Paranoid re-initialization of variable that we no longer use for anything anyway.

								ExecSQL (	MAIN_DB,	TR2003_5003_READ_doctor_presc,
														TR2003_5003_READ_doctor_presc_by_date,
											&PW_status,					&PW_NumPerDose,		&PW_RealAmtPerDose,
											&PW_PrescriptionType,		&PW_AuthDate,		&DummyInt,
											&PW_Doc_Largo_Code,
											&v_MemberIdentification,	&PW_DoctorPrID,		&PW_Pr_Date,
											&PW_Doc_Largo_Code,			&DocID,				&v_IdentificationCode,
											END_OF_ARG_LIST																);
							} // Pharmacy did supply a possibly-valid Prescription Date.


							// DonR 26Nov2003: Just in case the pharmacy sent a bogus Prescription Date, try
							// reading again with a reasonable date range.
							// DonR 21Jul2015: re-ordered WHERE clause to conform to the doctor_presc unique index.
							if (SQLERR_code_cmp (SQLERR_not_found) == MAC_TRUE)
							{
								// NOTE: The FLOOR function is not supported in the 32-bit version of Informix.

								PW_ID = 0;	// Paranoid re-initialization of variable that we no longer use for anything anyway.

								ExecSQL (	MAIN_DB,	TR2003_5003_READ_doctor_presc,
														TR2003_5003_READ_doctor_presc_by_date_range,
											&PW_status,					&PW_NumPerDose,			&PW_RealAmtPerDose,
											&PW_PrescriptionType,		&PW_AuthDate,			&PW_ValidFromDate,
											&PW_Doc_Largo_Code,
											&v_MemberIdentification,	&PW_DoctorPrID,			&EarliestPrescrDate,
											&LatestPrescrDate,			&PW_Doc_Largo_Code,		&DocID,
											&v_IdentificationCode,		END_OF_ARG_LIST									);

								// Copy the Valid From Date only when we successfully read it based on a date range.
								if (SQLCODE == 0)
									PW_Pr_Date = PR_Date [i] = PW_ValidFromDate;
							}	// Reading doctor_presc with exact date failed, so try with date range.


							// DonR 19Aug2015: If we didn't find a matching doctor prescription even with a date range, it's possible
							// that the pharmacy sent an invalid Doctor Largo Code (due to a pharmacy-side bug, or because the
							// prescription wasn't delivered electronically and they had to scan a printed prescription sheet).
							// In this case, perform the same lookups, but using the economypri table to include all drugs in the
							// same substitution group. (It's possible that this will result in "cardinality" errors because a
							// doctor has prescribed two different drugs from the same group - but this should happen very
							// infrequently in real life.) Pre-qualify the Group Code so we don't do the lookup if we know we
							// won't find anything; also, perform the lookup only for treatment-type drugs (Largo Type B, M, and T).
							if ((SQLERR_code_cmp (SQLERR_not_found) == MAC_TRUE)															&&
								((SPres[i].DL.largo_type == 'B') || (SPres[i].DL.largo_type == 'M') || (SPres[i].DL.largo_type == 'T'))		&&
								(DL_economypri_grp > 0))		// DonR 04Feb2018: As400UnixServer is now responsible for deciding which economypri
																// rows are valid for substitution; any non-zero value in drug_list/economypri_group
																// is OK here.
							{
								// If pharmacy didn't supply a possibly-valid Prescription Date, fall through to the second
								// SQL without bothering to look up by the specific date.
								if (PW_Pr_Date < 20170101)
								{
									FORCE_NOT_FOUND;	// Force a "not found" result.
								}
								else
								{
									PW_ID = 0;	// Paranoid re-initialization of variable that we no longer use for anything anyway.

									ExecSQL (	MAIN_DB,	TR2003_5003_READ_doctor_presc,
															TR2003_5003_READ_doctor_presc_date_with_largo_subst,
												&PW_status,					&PW_NumPerDose,		&PW_RealAmtPerDose,
												&PW_PrescriptionType,		&PW_AuthDate,		&DummyInt,
												&PW_Rx_Largo_Code,
												&v_MemberIdentification,	&PW_DoctorPrID,		&PW_Pr_Date,
												&DL_economypri_grp,			&DocID,				&v_IdentificationCode,
												END_OF_ARG_LIST																);

									// Copy the Largo Prescribed only when we successfully read it based on the Economypri group.
									if (SQLCODE == 0)
										PW_Doc_Largo_Code = PR_Original_Largo_Code [i] = PW_Rx_Largo_Code;
								} // Pharmacy did supply a possibly-valid Prescription Date.


								// Just in case the pharmacy sent a bogus Prescription Date, try reading again with a
								// reasonable date range. Note that the SQL above (using Economypri with a single date)
								// can fail with "not found" OR with "more than one found"; in the latter case there is
								// no point in trying again with a date range, since we'll just hit the same error.
								//
								// DonR 21Jul2015: re-ordered WHERE clause to conform to the doctor_presc unique index.
								if (SQLERR_code_cmp (SQLERR_not_found) == MAC_TRUE)
								{
									// NOTE: The FLOOR function is not supported in the 32-bit version of Informix.
									PW_ID = 0;	// Paranoid re-initialization of variable that we no longer use for anything anyway.

									ExecSQL (	MAIN_DB,	TR2003_5003_READ_doctor_presc,
															TR2003_5003_READ_doctor_presc_date_range_with_largo_subst,
												&PW_status,					&PW_NumPerDose,			&PW_RealAmtPerDose,
												&PW_PrescriptionType,		&PW_AuthDate,			&PW_ValidFromDate,
												&PW_Rx_Largo_Code,
												&v_MemberIdentification,	&PW_DoctorPrID,			&EarliestPrescrDate,
												&LatestPrescrDate,			&DL_economypri_grp,		&DocID,
												&v_IdentificationCode,		END_OF_ARG_LIST									);

									// Copy the Valid From Date and Largo Prescribed only when we successfully read them based
									// on a date range and Economypri group.
									if (SQLCODE == 0)
									{
										PW_Pr_Date			= PR_Date					[i] = PW_ValidFromDate;
										PW_Doc_Largo_Code	= PR_Original_Largo_Code	[i] = PW_Rx_Largo_Code;
									}
								}	// Reading doctor_presc with exact date failed, so try with date range.
							}	// We didn't find a doctor_presc row for the Largo Prescribed sent by the pharmacy.
						}	// Pharmacy did supply a non-zero Doctor Prescription ID.

						// DonR 21May2013: If the drug being sold requires that the doctor specify quantity-per-dose
						// and this field is missing from the doctor's prescription, OR this is not a daily-use prescription,
						// send pharmacy an "instructions are missing" message. (Obviously, this is only assuming that we
						// actually did read a doctor_presc row.) Use the new macro SetErrorVarArrBump to ensure that the
						// new error takes priority over other severity-5 errors, although this isn't really important for
						// Trn. 5003 the way it is for Trn. 2003 since 5003 can send multiple errors for the same drug.
						if (SQLCODE == 0)
						{
							if (((*PW_PrescriptionType >= '1')	&& (*PW_PrescriptionType <= '4'))						||
								((PW_RealAmtPerDose - (double)PW_NumPerDose) > .001))
							{
								SetErrorVarArrBump (&PrsLinP->DrugAnswerCode, ERR_MISSING_USAGE_INSTRUCTIONS,
													PrsLinP->DrugCode, LineNum, NULL, &ErrOverflow);
							}	// Doctor's prescription was missing usage-instruction information.
						}	// Successfully read doctor prescription from doctor_presc.
						else
						{
							// Set bogus values for PRW-ID and AuthDate so we can be sure we don't match anything in the database.
							PW_ID = PW_AuthDate = -1;
						}


						// DonR 24Jul2013: Moved early-refill warning test here, since it now relies on knowing the
						// visit date from doctor_presc.
						// DonR 21Mar2006: If this drug is being sold according to an AS400 Ishur
						// with quantity limits, there is no need to check for early refill as well.
						// DonR 23Aug2010: Per Iris Shaya, don't check for early refills for those "members"
						// in the membernotcheck table.
						if ((MACCABI_PHARMACY)								&&
							(Member.check_od_interact)						&&	// DonR 24Jan2018 CR #13937
							(!SPres[i].HasIshurWithLimit)					&&	// DonR 21Mar2006.
							((SPres[i].DL.largo_type	== 'B')		||
							 (SPres[i].DL.largo_type	== 'M')		||
							 (SPres[i].DL.largo_type	== 'T'))			&&
							( SPres[i].DL.drug_type != 'O')					&&
							( SPres[i].DL.drug_type != 'Q'))
						{
							// DonR 19Apr2010: Add additional warning message for early refill, with slightly
							// different conditions than the other early-refill messages.
							// DonR 24Jul2013: Add criteria to avoid giving this message if the current sale
							// is against a prescription found in doctor_presc AND the previous sale was at
							// the same pharmacy with a prescription from the same doctor written on the
							// same day.
							// DonR 27Apr2015: Create a separate version of this query to use the new pd_rx_link
							// table instead of prescr_wr_new.
							// DonR 01Jan2020: Patching in the same ODBC command as is used in Transaction 6003,
							// even though it has somewhat different logic for determining if earlier sales were
							// complete against their doctor prescriptions. This logic should work fine, since
							// all three drug-sale transactions are using the same routine to update doctor_presc
							// and pd_rx_link at sale-completion time.
							ShortOverlapDate	= IncrementDate (SysDate, MaxShortDurationOverlapDays);
							LongOverlapDate		= IncrementDate (SysDate, MaxLongDurationOverlapDays);

							// DonR 24Nov2024 User Story #366220: Select prior sales of generic equivalents
							// as well as sales of the drug currently being sold. There are a bunch of other
							// changes in the SQL logic - see the SQL code.
							ExecSQL (	MAIN_DB, READ_test_for_ERR_EARLY_REFILL_WARNING,
										&RowsFound,
										&v_MemberIdentification,	&SPres[i].DrugCode,		&SPres[i].DL.economypri_group,
										&v_IdentificationCode,		&SysDate,				&MaxShortDurationDays,
										&ShortOverlapDate,			&MaxShortDurationDays,	&LongOverlapDate,
										&SysDate,					END_OF_ARG_LIST											);

							Conflict_Test (reStart);

							if (SQLERR_error_test ())
							{
								SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR);
								break;
							}
							else
							{
								if (RowsFound > 0)
								{
									SetErrorVarArr (&SPres[i].DrugAnswerCode, ERR_EARLY_REFILL_WARNING,
													SPres[i].DrugCode, i + 1, NULL, &ErrOverflow);
								}	// Found recent purchase of same drug.
							}	// Succeeded in getting purchase-count.
						}	// Need to perform early-refill test.


						// DonR 14Mar2005: Restore "isolation" to normal level.
						SET_ISOLATION_COMMITTED;

					}	// This is a Maccabi Doctor prescription.
				}	// This is not a deletion transaction.

				// DonR 21May2013: For quantity-per-dose, we want to write what the pharmacy sent, not what was in
				// doctor_presc - so the next line of code was moved here from the beginning of the drugs loop.
				PW_NumPerDose = PR_NumPerDose [i];

				// Remember the status of the doctor_presc row.
				// If the row had previously been marked with Status 3 (i.e. it had been
				// selected as an alternative prescription and had its Prescription Date
				// "stolen"), report it as Status 0 (unfilled). This is rather unlikely
				// ever to happen in the real world.
				// DonR 28Apr2015: Status 3 is no longer in use, so I simplified the line below.
				PR_Status[i] = PW_status;


				// DonR 05May2004: If pharmacy sent a Special Prescription number AND
				// we didn't find an AS400 Special Prescription for this drug, copy the
				// pharmacy's number to the drug's variables - otherwise the number (used
				// for things like overriding interaction/overdose messages) is lost.
				if ((v_SpecialConfNum > 0) && (PrsLinP->SpecPrescNum == 0))
				{
					PrsLinP->SpecPrescNum		= v_SpecialConfNum;
					PrsLinP->SpecPrescNumSource	= v_SpecialConfSource;
				}


				// DonR 13Jan2009: If drugs are being sold without a prescription, swap in a value of 8
				// for the "in health basket" parameter. This doesn't change any logic in how the Unix
				// programs handle the sale, but is needed by the AS/400.
				// DonR 10Oct2016: For emergency-supply sales, force in zero for "in health basket".
				if (v_RecipeSource == RECIP_SRC_NO_PRESC)
					PrsLinP->in_health_pack = 8;
				else
				if (v_RecipeSource == RECIP_SRC_EMERGENCY_NO_RX)
					PrsLinP->in_health_pack = 0;

				// If participation isn't coming from a drug_extension rule, force rule_number to zero.
				// DonR 20Jul2010: If this is a sale deletion, the Rule Number comes from the original
				// sale; accept it unconditionally.
				if ((PrsLinP->ret_part_source.table != FROM_DRUG_EXTENSION) && (v_ActionType != SALE_DELETION))
					PrsLinP->rule_number = 0;


				// DonR 09Jul2003: Switched storage of basic and computed participation codes,
				// so that the correct code for the actual purchase goes to the AS400. Now we
				// will store BasePartCode (normally from drug_list) in calc_member_price, and
				// RetPartCode (the participation code we actually send to the pharmacy) will
				// be in member_price_code. The latter field is the one sent to the AS400.
				// Note that this change will not affect pharmacy operations at all - just
				// a switch in which number is stored in which field, and which number is
				// sent to the AS400.
				ExecSQL (	MAIN_DB, TR5003_INS_prescription_drugs,
							&v_RecipeIdentifier,			&LineNum,
							&v_MemberIdentification,		&v_IdentificationCode,
							&PrsLinP->DrugCode,				&NOT_REPORTED_TO_AS400,
							&PrsLinP->Op,					&PrsLinP->Units,
							&PrsLinP->Duration,				&StopUseDate,

							&v_ConfirmationDate,			&v_ConfirmationHour,
							&ROW_NOT_DELETED,				&DRUG_NOT_DELIVERED,
							&PrsLinP->PriceSwap,			&PrsLinP->AdditionToPrice,
							&PrsLinP->LinkDrugToAddition,	&PrsLinP->RetOpDrugPrice,
							&v_ActionType,					&PrsLinP->DrugAnswerCode,

							&PrsLinP->RetPartCode,			&PrsLinP->BasePartCode,
							&v_RecipeSource,				&PrsLinP->MacabiPriceFlg,
							&v_PharmNum,					&v_InstituteCode,
							&PrsLinP->SupplierDrugPrice,	&StopBloodDate,
							&IntZero,						&IntZero,

							&IntZero,						&IntZero,
							&ShortZero,						&PrsLinP->DL.t_half,
							&DocID,							&v_TypeDoctorIdentification,
							&PW_DoctorPrID,					&PW_Pr_Date,
							&PW_Doc_Largo_Code,				&PW_SubstPermitted,

							&PrsLinP->Units,				&PrsLinP->Op,
							&PW_NumPerDose,					&PW_DosesPerDay,
							&PrsLinP->InsPlusPtnSource,		&PW_ID,
							&PrsLinP->in_health_pack,		&PrsLinP->SpecPrescNum,
							&PrsLinP->SpecPrescNumSource,	&PrsLinP->IshurWithTikra,

							&PrsLinP->IshurTikraType,		&PrsLinP->Doctor_Op,
							&PrsLinP->Doctor_Units,			&PrsLinP->rule_number,
							&v_DrugTikraType,				&v_DrugSubsidized,
							&DL_TikratMazonFlag,			&MemberPtnAmt,
							&ShortZero,						&v_CreditYesNo,

							&ShortZero,						&PrsLinP->why_future_sale_ok,
							&PrsLinP->qty_limit_chk_type,	&PrsLinP->qty_lim_ishur_num,
							&PrsLinP->vacation_ishur_num,	&PW_AuthDate,
							&v_DoctorLocationCode,			&PrsLinP->DiscountApplied,
							&PrsLinP->qty_limit_class_code,	END_OF_ARG_LIST					);

				Conflict_Test (reStart);
				
				if (SQLERR_error_test ())
				{
					SetErrorVarArr (&v_ErrorCode, ERR_DATABASE_ERROR, 0, 0, NULL, &ErrOverflow);
					break;
				}



				// DonR 12Sep2013: If we've performed actual-usage testing (for ampoules sold
				// against an AS/400 ishur with quantity limits), write to the new
				// prescription_usage table as well.
				if (PrsLinP->actual_usage_checked)
				{
					ExecSQL (	MAIN_DB, INS_prescription_usage,
								&v_RecipeIdentifier,			&LineNum,
								&PrsLinP->DrugCode,				&PrsLinP->course_len_days,
								&PrsLinP->course_treat_days,	&PrsLinP->Duration,
								&PrsLinP->calc_courses,			&PrsLinP->calc_treat_days,
								&PrsLinP->total_units,			&PrsLinP->fully_used_units,

								&PrsLinP->partly_used_units,	&PrsLinP->discarded_units,
								&PrsLinP->avg_partial_unit,		&PrsLinP->proportion_used,
								&PrsLinP->net_lim_ingr_used,	&v_ConfirmationDate,
								&v_ConfirmationHour,			&DRUG_NOT_DELIVERED,
								&NOT_REPORTED_TO_AS400,			END_OF_ARG_LIST					);

					Conflict_Test (reStart);
				
					if (SQLERR_error_test ())
					{
						SetErrorVarArr (&v_ErrorCode, ERR_DATABASE_ERROR, 0, 0, NULL, &ErrOverflow);
						break;
					}
				}	// Need to write to prescription_usage.

			}	// End of drugs-in-prescription loop.


			// DonR 08Apr2014: Call new routine to generate Health Alert Messages for this member.
			// (Note that we don't give Health Alerts for sale deletions.)
			// DonR 11Sep2024 Bug #350304: Add address of v_ErrorCode to the argument list for CheckHealthAlerts,
			// so that critical errors from the module will be written to prescriptions/error_code and sale
			// completion by the pharmacy will be prevented.
			if (v_ActionType == DRUG_SALE)
				CheckHealthAlerts	(	CHECK_MEMBER, &Member, SPres, v_NumOfDrugLinesRecs, NULL,
										&v_DrugsBuf, &Phrm_info, v_RecipeSource, &v_ErrorCode		);


			// DonR 11Apr2013: Write all error codes to prescription_msgs.
			// If we've hit a severe error, clear any "Mishur participation granted"
			// messages on any items in the sale, since they're no longer relevant.
			if ((SetErrorVarArr (&v_ErrorCode, 0, 0, 0, NULL, NULL)) || (reStart == MAC_TRUE))
			{
				SetErrorVarDelete (ERR_GADGET_OP_REDUCED_BASIC,		0);
				SetErrorVarDelete (ERR_GADGET_OP_REDUCED_MAGEN,		0);
				SetErrorVarDelete (ERR_GADGET_OP_REDUCED_KEREN,		0);
				SetErrorVarDelete (ERR_GADGET_OP_REDUCED_YAHALOM,	0);
				SetErrorVarDelete (ERR_GADGET_OP_REDUCED_UNKNOWN,	0);
				SetErrorVarDelete (ERR_GADGET_APPROVED_BASIC,		0);
				SetErrorVarDelete (ERR_GADGET_APPROVED_MAGEN,		0);
				SetErrorVarDelete (ERR_GADGET_APPROVED_KEREN,		0);
				SetErrorVarDelete (ERR_GADGET_APPROVED_YAHALOM,		0);
				SetErrorVarDelete (ERR_GADGET_APPROVED_UNKNOWN,		0);
			}
			else
			{	// Nothing terrible happened. Any additional messages to pharmacy get assigned here.
				if ((v_ActionType == DRUG_SALE) && (v_RecipeSource == RECIP_SRC_EMERGENCY_NO_RX))
				{
					SetErrorVarArr (&v_ErrorCode, EMERGENCY_SUPPLY_100_PERCENT, 0, 0, NULL, &ErrOverflow);
				}
			}

			ErrorCount = SetErrorVarTell (&ErrorArray, NULL);

			for (i = 0; i < ErrorCount; i++)
			{
				v_DrugCode		= ErrorArray [i].LargoCode;
				v_ArrayErrCode	= ErrorArray [i].Error;
				v_ArraySeverity	= ErrorArray [i].Severity;
				v_ArrayLineNo	= ErrorArray [i].LineNo;

				ExecSQL (	MAIN_DB, INS_prescription_msgs,
							&v_RecipeIdentifier,		&v_DrugCode,			&v_ArrayLineNo,
							&v_ArrayErrCode, 			&v_ArraySeverity,		&v_ConfirmationDate,
							&v_ConfirmationHour,		&DRUG_NOT_DELIVERED,	&NOT_REPORTED_TO_AS400,
							END_OF_ARG_LIST																);

				Conflict_Test (reStart);
				
				if (SQLERR_error_test ())
				{
					SetErrorVarArr (&v_ErrorCode, ERR_DATABASE_ERROR, 0, 0, NULL, &ErrOverflow);
					break;
				}
			}	// End of error-writing loop.

		}
		while (0);	// Dummy loop should run only once.


		// Commit the transaction.
		if (reStart == MAC_FALS)	// No DB errors - OK to commit work.
		{
			CommitAllWork ();

			if (SQLERR_error_test ())	// Could not COMMIT
			{
				SetErrorVarArr (&v_ErrorCode, ERR_DATABASE_ERROR, 0, 0, NULL, &ErrOverflow);
			}
		}
		else
		{
			// We saw some kind of DB error - try to restart.
			if (TransactionRestart () != MAC_OK)
			{
				v_ErrorCode = ERR_DATABASE_ERROR;
				break;
			}
			
			// DonR 26Jul2011: Suppress log message until the fourth attempt fails.
			if (tries > 2)
				GerrLogReturn (GerrId, "Table is locked for the <%d> time", tries);
			
			sleep (ACCESS_CONFLICT_SLEEP_TIME);
		}	// DB error occurred.

	}	// End of Database Retries loop.
	
	
	if (reStart == MAC_TRUE)
	{
		GerrLogReturn (GerrId, "Locked for <%d> times", SQL_UPDATE_RETRIES);
		SetErrorVarArr (&v_ErrorCode, ERR_DATABASE_ERROR, 0, 0, NULL, &ErrOverflow);
	}


	// Prepare and send Response Message (Message ID 5004).

	// DonR 02Mar2011: Per Iris Shaya, if we've hit a severe error, clear any
	// "Mishur participation granted" messages on any items in the sale,
	// since they're no longer relevant.
	// DonR 11Apr2013: Note that this code appears twice, and is going to be redundant
	// in most cases. However, it is possible that we'll hit a database error "davka" when
	// writing error codes to prescription_msgs (see above), so we have to execute this
	// code again after all the database stuff is over, just in case.
	if (SetErrorVarArr (&v_ErrorCode, 0, 0, 0, NULL, NULL))
	{
		SetErrorVarDelete (ERR_GADGET_OP_REDUCED_BASIC,		0);
		SetErrorVarDelete (ERR_GADGET_OP_REDUCED_MAGEN,		0);
		SetErrorVarDelete (ERR_GADGET_OP_REDUCED_KEREN,		0);
		SetErrorVarDelete (ERR_GADGET_OP_REDUCED_YAHALOM,	0);
		SetErrorVarDelete (ERR_GADGET_OP_REDUCED_UNKNOWN,	0);
		SetErrorVarDelete (ERR_GADGET_APPROVED_BASIC,		0);
		SetErrorVarDelete (ERR_GADGET_APPROVED_MAGEN,		0);
		SetErrorVarDelete (ERR_GADGET_APPROVED_KEREN,		0);
		SetErrorVarDelete (ERR_GADGET_APPROVED_YAHALOM,		0);
		SetErrorVarDelete (ERR_GADGET_APPROVED_UNKNOWN,		0);
	}

	WinHebToDosHeb ((unsigned char *) v_MemberFamilyName);
	WinHebToDosHeb ((unsigned char *) v_MemberFirstName);
	WinHebToDosHeb ((unsigned char *) v_DoctorFamilyName);
	WinHebToDosHeb ((unsigned char *) v_DoctorFirstName);

	nOut =  sprintf (OBuffer,		 "%0*d",  MSG_ID_LEN,				5004					);
	nOut += sprintf (OBuffer + nOut, "%0*d",  MSG_ERROR_CODE_LEN,		MAC_OK					);
	nOut += sprintf (OBuffer + nOut, "%0*d",  TPharmNum_len,			v_PharmNum				);
	nOut += sprintf (OBuffer + nOut, "%0*d",  TInstituteCode_len,		v_InstituteCode			);
	nOut += sprintf (OBuffer + nOut, "%0*d",  TTerminalNum_len,			v_TerminalNum			);
	nOut += sprintf	(OBuffer + nOut, "%0*d",  TGenericTZ_len,			v_MemberIdentification	);
	nOut += sprintf (OBuffer + nOut, "%0*d",  TIdentificationCode_len,	v_IdentificationCode	);

	nOut += sprintf (OBuffer + nOut, "%-*.*s", TMemberFamilyName_len,
											   TMemberFamilyName_len,	v_MemberFamilyName		);
	nOut += sprintf (OBuffer + nOut, "%-*.*s", TMemberFirstName_len,	
											   TMemberFirstName_len,	v_MemberFirstName		);

	nOut += sprintf (OBuffer + nOut, "%0*d",  TMemberDateOfBirth_len,	Member.date_of_bearth	);
						
	nOut += sprintf (OBuffer + nOut, "%-*.*s", TDoctorFamilyName_len,	
											   TDoctorFamilyName_len,	v_DoctorFamilyName		);
	nOut += sprintf (OBuffer + nOut, "%-*.*s", TDoctorFirstName_len,	
											   TDoctorFirstName_len,	v_DoctorFirstName		);

	nOut += sprintf (OBuffer + nOut, "%0*d",  TConfiramtionDate_len,	v_ConfirmationDate		);
	nOut += sprintf (OBuffer + nOut, "%0*d",  TConfirmationHour_len,	v_ConfirmationHour		);
	nOut += sprintf (OBuffer + nOut, "%0*d",  TMsgExistsFlg_len,		v_MsgExistsFlg			);
	// DonR 19Jan2005: Per Iris Shaya, replaced the Message Urgent Flag, which is currently
	// not in use, with the Pharmacy Message Flag - which was inadvertently left out of the
	// specification for Transaction 2005.
	nOut += sprintf (OBuffer + nOut, "%0*d",  TMsgExistsFlg_len,		v_Message				);
	nOut += sprintf (OBuffer + nOut, "%0*d",  TRecipeIdentifier_len,	v_RecipeIdentifier		);
	nOut += sprintf (OBuffer + nOut, "%0*d",  TCreditYesNo_len,			v_CreditYesNo			);

	// 20 characters reserved for future use.
	nOut += sprintf (OBuffer + nOut, "%*.*s", 20,	
											  20,						"00000000000000000000"  );

	nOut += sprintf (OBuffer + nOut, "%0*d",  TElectPR_LineNumLen,		v_NumOfDrugLinesRecs	);


	// Drug lines.
	for (i = 0; i < v_NumOfDrugLinesRecs; i++)
	{
		// DonR 05Jul2010: Per Iris Shaya, if this is a deletion we need to "flip" the OP/Units and
		// the total participation amount from negative back to positive before sending them to the
		// pharmacy.
		if (v_ActionType == SALE_DELETION)
		{
			SPres[i].Op			*= -1;
			SPres[i].Units		*= -1;
			MemberPtnAmount	[i]	*= -1;
		}

		// Line Number.
		nOut += sprintf (OBuffer + nOut, "%0*d",  TElectPR_LineNumLen,			(i + 1)						);
		nOut += sprintf (OBuffer + nOut, "%0*d",  9,							SPres[i].DrugCode			);
		nOut += sprintf (OBuffer + nOut, "%0*d",  1,							PR_Status[i]				);
		nOut += sprintf (OBuffer + nOut, "%0*d",  TUnits_len,					SPres[i].Units				);
		nOut += sprintf (OBuffer + nOut, "%0*d",  TOp_len,						SPres[i].Op					);
		nOut += sprintf (OBuffer + nOut, "%0*d",  TOpDrugPrice_len,				SPres[i].RetOpDrugPrice		);
		nOut += sprintf	(OBuffer + nOut, "%0*d",  TSupplierPrice_len,			SPres[i].SupplierDrugPrice	);
		nOut += sprintf (OBuffer + nOut, "%0*d",  TMemberParticipatingType_len,	SPres[i].RetPartCode		);
		nOut += sprintf (OBuffer + nOut, "%0*d",  TOpDrugPrice_len,				SPres[i].PriceSwap			);
		nOut += sprintf (OBuffer + nOut, "%0*d",  TMemberDiscountPercent_len,	SPres[i].AdditionToPrice	);

		// DonR 15Jan2009: Because we now add a "hundreds" digit to this variable to indicate why
		// specialist participation was given, we need to ensure that only the last two digits are
		// sent to the pharmacy - thus we send the variable "MOD 100".
		nOut += sprintf (OBuffer + nOut, "%0*d",  2,							(SPres[i].InsPlusPtnSource % 100));

		nOut += sprintf (OBuffer + nOut, "%0*d",  TOpDrugPrice_len,				MemberPtnAmount[i]			);

		// Isn't this redundant?
		nOut += sprintf (OBuffer + nOut, "%0*d",  2,							((SPres[i].InsPlusPtnSource % 100) / 10));

		nOut += sprintf (OBuffer + nOut, "%c",									DrugTikraType	[i]			);

		// AS/400 may give a space instead of a zero for "no coupon" - check if this is OK!
		nOut += sprintf (OBuffer + nOut, "%c",									DrugCoupon		[i]			);

		nOut += sprintf (OBuffer + nOut, "%0*d",  1,							SPres[i].in_health_pack		);

		// 15 characters reserved for future use.
		nOut += sprintf (OBuffer + nOut, "%*.*s", 15,	
												  15,						"00000000000000000000"  );
	}	// Output drug lines.


	// Output "Tikra" and "Coupon" lines (in same format).
	nOut += sprintf (OBuffer + nOut, "%0*d",  TElectPR_LineNumLen,		(NumTikrotLines + NumCouponLines)	);

	// "Tikra" lines.
	for (i = 0; i < NumTikrotLines; i++)
	{
		// Line Number.
		nOut += sprintf (OBuffer + nOut, "%0*d",  TElectPR_LineNumLen,			(i + 1)						);
		nOut += sprintf (OBuffer + nOut, "2"																);
		nOut += sprintf (OBuffer + nOut, "%c"  ,  								TikraType			[i]		);
		nOut += sprintf (OBuffer + nOut, "%0*d",  1,							TikraBasket			[i]		);
		nOut += sprintf (OBuffer + nOut, "%0*d",  2,							TikraInsurance		[i]		);
		nOut += sprintf (OBuffer + nOut, "%0*d",  TOpDrugPrice_len,				TikraAggPrevPtn		[i]		);
		nOut += sprintf (OBuffer + nOut, "%0*d",  TOpDrugPrice_len,				TikraAggPrevWaived	[i]		);
		nOut += sprintf (OBuffer + nOut, "%0*d",  TOpDrugPrice_len,				TikraCurrPtn		[i]		);
		nOut += sprintf (OBuffer + nOut, "%0*d",  TOpDrugPrice_len,				TikraCurrWaived		[i]		);
		nOut += sprintf (OBuffer + nOut, "%0*d",  TOpDrugPrice_len,				TikraCurrLevel		[i]		);

		// Check if this needs to be converted to DOS Hebrew.
		nOut += sprintf (OBuffer + nOut, "%-*.*s", 6, 6,						TikraPeriodDesc		[i]		);

		// Coupon fields - all zero for Tikra lines.
		nOut += sprintf (OBuffer + nOut, "%0*d",  3,							0							);
		nOut += sprintf (OBuffer + nOut, "%0*d",  TOpDrugPrice_len,				0							);
	}

	// Coupon lines.
	for (i = 0; i < NumCouponLines; i++)
	{
		// Line Number.
		nOut += sprintf (OBuffer + nOut, "%0*d" ,  TElectPR_LineNumLen,			(i + 1 + NumTikrotLines)	);
		nOut += sprintf (OBuffer + nOut, "1"																);

		// Tikra Line fields are blank or zero for Coupons.
		nOut += sprintf (OBuffer + nOut, " "									  /* Tikra Type			*/	);
		nOut += sprintf (OBuffer + nOut, "%0*d" ,  1,							0 /* TikraBasket		*/	);
		nOut += sprintf (OBuffer + nOut, "%0*d" ,  2,							0 /* TikraInsurance		*/	);
		nOut += sprintf (OBuffer + nOut, "%0*d",  TOpDrugPrice_len,				0 /* TikraAggPrevPtn	*/	);
		nOut += sprintf (OBuffer + nOut, "%0*d",  TOpDrugPrice_len,				0 /* TikraAggPrevWaived	*/	);
		nOut += sprintf (OBuffer + nOut, "%0*d",  TOpDrugPrice_len,				0 /* TikraCurrPtn		*/	);
		nOut += sprintf (OBuffer + nOut, "%0*d",  TOpDrugPrice_len,				0 /* TikraCurrWaived	*/	);
		nOut += sprintf (OBuffer + nOut, "%0*d",  TOpDrugPrice_len,				0 /* TikraCurrLevel		*/	);
		nOut += sprintf (OBuffer + nOut, "      "								  /* TikraPeriodDesc	*/	);

		// Actual Coupon fields.
		nOut += sprintf (OBuffer + nOut, "%0*d",  3,							TikraCouponCode	[i]			);
		nOut += sprintf (OBuffer + nOut, "%0*d",  TOpDrugPrice_len,				TikraCouponAmt	[i]			);
	}


	// Output family T.Z. numbers.
	// DonR 29Jun2010: TEMPORARY FOR PILOT: Force family size to zero, since pharmacy-side
	// software isn't ready to deal with this stuff.
	FamilySize = 0;

	nOut += sprintf (OBuffer + nOut, "%0*d",  TElectPR_LineNumLen,		FamilySize	);
	for (i = 0; i < FamilySize; i++)
	{
		nOut += sprintf (OBuffer + nOut, "%0*d",  TElectPR_LineNumLen,		(i + 1)					);
		nOut += sprintf	(OBuffer + nOut, "%0*d",  TGenericTZ_len,			FamilyMemberTZ		[i]	);
		nOut += sprintf (OBuffer + nOut, "%0*d",  TIdentificationCode_len,	FamilyMemberTZCode	[i]	);

		// 10 characters reserved for future use.
		nOut += sprintf (OBuffer + nOut, "%*.*s", 10,	
												  10,						"00000000000000000000"  );
	}


	// Output error codes with associated Largo Codes.
	// DonR 25Jul2013: Added new parameter to SetErrorVarTell to give us the number of reportable errors -
	// since we're now sometimes creating "errors" that we don't send to the pharmacy. We put this number
	// into the variable j, send it to the pharmacy, and then immediately re-use the variable for something
	// different.
	ErrorCount = SetErrorVarTell (&ErrorArray, &j);
	nOut += sprintf (OBuffer + nOut, "%0*d",  3, j);

	for (i = 0, j = 0; i < ErrorCount; i++)
	{
		// DonR 24Jul2013: Added new "ReportToPharm" element in TErrorArray structure; this allows us
		// to assign error codes that are never to be sent to pharmacies. Note that we have to use
		// j instead of i to send the error line number to the pharmacy, since we may be skipping
		// some elements of the array.
		if (ErrorArray [i].ReportToPharm)
		{
			j++;
			nOut += sprintf (OBuffer + nOut, "%0*d",  3,					j							);
			nOut += sprintf (OBuffer + nOut, "%0*d",  9,					ErrorArray [i].LargoCode	);
			nOut += sprintf (OBuffer + nOut, "%0*d",  4,					ErrorArray [i].Error		);
		}	// Error is one that is to be reported to pharmacy.
	}


	// Return the size in Bytes of response message.
	*p_outputWritten			= nOut;
	*output_type_flg			= ANSWER_IN_BUFFER;
	ssmd_data_ptr->error_code	= v_ErrorCode;
	ssmd_data_ptr->pharmacy_num	= v_PharmNum;
	ssmd_data_ptr->terminal_num	= v_TerminalNum;


	RESTORE_ISOLATION;

	return  RC_SUCCESS;

}	// End of 5003 handler.



/*=======================================================================
||																		||
||			 HandlerToMsg_5005											||
||	Message handler for message 5005:									||
||        Drugs Delivery - "Nihul Tikrot" version						||
||																		||
||																		||
 =======================================================================*/

int HandlerToMsg_5005 (char			*IBuffer,
					   int			TotalInputLen,
					   char			*OBuffer,
					   int			*p_outputWritten,
					   int			*output_type_flg,
					   SSMD_DATA	*ssmd_data_ptr)

{
	// Local declarations
	ISOLATION_VAR;
	int				nOut;
	int				tries;
	int				reStart;
	int				ElyCount;
	int				flg;
	int				UnitsSellable;
	int				UnitsSold;
	int				ComparePharmPtn;
	TErrorCode		err;
	TErrorSevirity	Sever;
	TMonthLog		MonthLogCalc_next;
	TMonthLog		MonthLogCalc;
	short			NextMonthMinDay;
	ACTION_TYPE		AS400_Gadget_Action;
	char			filler [256];

	// Message fields variables
	Msg1005Drugs				phDrgs					[MAX_REC_ELECTRONIC];
	short int					LineNumber				[MAX_REC_ELECTRONIC];
	short int					PR_SubstPermitted		[MAX_REC_ELECTRONIC];
	short int					PR_NumPerDose			[MAX_REC_ELECTRONIC];
	short int					PR_DosesPerDay			[MAX_REC_ELECTRONIC];
	short int					PR_HowParticDetermined	[MAX_REC_ELECTRONIC];
	short int					in_gadget_table			[MAX_REC_ELECTRONIC];
	short int					PRW_Op_Update			[MAX_REC_ELECTRONIC];
	int							PR_Date					[MAX_REC_ELECTRONIC];
	int							PR_Orig_Largo_Code		[MAX_REC_ELECTRONIC];
	int							PrevUnsoldUnits			[MAX_REC_ELECTRONIC];
	int							PrevUnsoldOP			[MAX_REC_ELECTRONIC];
	int							FixedParticipPrice		[MAX_REC_ELECTRONIC];
	int							PrescWrittenID			[MAX_REC_ELECTRONIC];
	TMemberDiscountPercent		v_MemberDiscPC			[MAX_REC_ELECTRONIC];
	TDoctorRecipeNum			v_DoctorRecipeNum		[MAX_REC_ELECTRONIC];
	TDate						StopBloodDate			[MAX_REC_ELECTRONIC];

	// DonR 12Feb2006: Ingredients-sold arrays.
	short int					ingr_code				[MAX_REC_ELECTRONIC] [3];
	double						ingr_quant				[MAX_REC_ELECTRONIC] [3];
	double						ingr_quant_std			[MAX_REC_ELECTRONIC] [3];
	double						ingr_quant_bot			[MAX_REC_ELECTRONIC] [3];
	double						ingr_quant_bot_std		[MAX_REC_ELECTRONIC] [3];
	double						per_quant				[MAX_REC_ELECTRONIC] [3];
	double						package_volume			[MAX_REC_ELECTRONIC];

	// New stuff for "Nihul Tikrot"
	short						v_ActionType;
	short						v_RetnReasonCode;
	short						v_5003CommError;
	TGenericTZ					v_CardOwnerID;
	TIdentificationCode			v_CardOwnerIDCode;
	short						v_CreditLinePmts;
	short						v_PaymentType;
	int							v_TotalTikraDisc;
	int							v_TotalCouponDisc;
	short						NumOtherSales;
	int							v_DrugDiscountAmt;
	short						v_DrugDiscountCode;
	short						v_PharmPtnCode;	// DonR 31Jan2011
	int							DrugDiscountAmt			[MAX_REC_ELECTRONIC];
	short						DrugDiscountCode		[MAX_REC_ELECTRONIC];
	short						InsuranceUsed			[MAX_REC_ELECTRONIC];
	char						DrugTikraType			[MAX_REC_ELECTRONIC];
	char						DrugSubsidized			[MAX_REC_ELECTRONIC];
	short						InHealthBasket			[MAX_REC_ELECTRONIC];
	int							OtherSaleID				[MAX_REC_ELECTRONIC];
	short						OtherSaleCredLine		[MAX_REC_ELECTRONIC];

	// Variables for deletion/refund of previous sales.
	int							v_DeletedPrID;
	int							sq_DeletedPrID;		// DonR 15Mar2011 - in case original sale came via spool.
	int							v_DeletedPrDate;
	int							v_DeletedPrPharm;
	short						v_DeletedPrYYMM;
	int							v_DeletedPrPhID;

	// Pharmacy Daily Sum.
	int							PDS_sum;
	int							PDS_prt_sum;
	int							PDS_lines;
	int							PDS_count;
	int							PDS_purch_sum;
	int							PDS_del_sum;
	int							PDS_del_prt_sum;
	int							PDS_del_lines;
	int							PDS_del_count;
	int							PDS_del_purch_sum;
	int							PDS_date;

	int							PW_ID;
	short						PRW_OpUpdate;
	short						PRW_UnitsUpdate;
	short						PW_status;
	int							PD_discount_pc;

	int							v_ClientLocationCode;
	int							v_DocLocationCode;
	short int					v_ReasonToDispense;
	short int					v_in_gadget_table;

	// DonR 12Feb2006: Ingredients-sold fields.
	double						v_package_volume;
	short int					v_ingr_1_code;
	short int					v_ingr_2_code;
	short int					v_ingr_3_code;
	double						v_ingr_1_quant;
	double						v_ingr_2_quant;
	double						v_ingr_3_quant;
	double						v_ingr_1_quant_std;
	double						v_ingr_2_quant_std;
	double						v_ingr_3_quant_std;
	double						v_per_1_quant;
	double						v_per_2_quant;
	double						v_per_3_quant;

	TDrugListRow				DL;
	PHARMACY_INFO				Phrm_info;
	TPharmNum					v_PharmNum;
	TInstituteCode				v_InstituteCode;
	TTerminalNum				v_TerminalNum;
	TMemberBelongCode			v_MemberBelongCode;
	TGenericTZ					v_MemberID;
	TIdentificationCode			v_IDCode;
	TMoveCard					v_MoveCard;
	TTypeDoctorIdentification	v_DocIDType ;
	TGenericTZ					v_DoctorID;
	short						v_ElectPrStatus;
	TRecipeSource				v_RecipeSource;
	TInvoiceNum					v_InvoiceNum;
	TGenericTZ					v_UserIdentification;
	TPharmRecipeNum				v_PharmRecipeNum;
	TRecipeIdentifier			v_RecipeIdentifier;
	TMemberParticipatingType	v_MemberParticType;
	TDeliverConfirmationDate	v_DeliverConfirmationDate;
	TDeliverConfirmationHour	v_DeliverConfirmationHour;
	TMonthLog					v_MonthLog;
	TNumOfDrugLinesRecs			v_NumOfDrugLinesRecs;
	TCreditYesNo				v_CreditYesNo;
	short						paid_for	= 1;	// DonR 05Feb2023 User Story #232220 - always TRUE for 2005/5005.

	TNumofSpecialConfirmation		v_SpecialConfNum;
	TSpecialConfirmationNumSource	v_SpecialConfSource;

	// Prescription Lines structure
	Msg1005Drugs				*phDrgPtr;
	Msg1005Drugs				dbDrgs;

	// Gadgets/Gadget_spool tables.
	short int					v_action;
	int							v_service_code;
	int							v_gadget_code;
	short int					v_service_number;
	short int					v_service_type;
	TDrugCode					v_DrugCode;
	TOpDrugPrice				Unit_Price;
	TDoctorRecipeNum			v_DoctorPrID;
	int							v_DoctorPrDate;
	short int					PRW_OriginCode;
	TErrorCode					GadgetError;
	TRecipeIdentifier			GadgetRecipeID;

	// Return-message variables
	TDate						SysDate;
	TDate						v_Date;
	THour						v_Hour;
	TErrorCode					v_ErrorCode;


	// Miscellaneous DB variables
	int							i;
	int							j;
	int							is;
	short						sq_ActionType;
	TPharmNum					sq_pharmnum;
	TInstituteCode				sq_institutecode;
	TTerminalNum				sq_terminalnum;
	TPriceListNum				sq_pricelistnum;
	TDoctorRecipeNum			sq_doctorprescid;
	TMemberBelongCode			sq_memberbelongcode;
	TGenericTZ					sq_MemberID;
	TIdentificationCode			sq_identificationcode;
	TTypeDoctorIdentification	sq_DocIDType;
	TGenericTZ					sq_doctoridentification;
	TRecipeSource				sq_recipesource;
	TNumOfDrugLinesRecs			sq_numofdruglinesrecs;
	TErrorCode					sq_errorcode;
	TErrorCode					prev_pr_errorcode;
	TTotalMemberParticipation	sq_TotalMemberPrice;
	TTotalDrugPrice				sq_TotalDrugPrice;
	TTotalDrugPrice				sq_TotSuppPrice;
	TMemberDiscountPercent		sq_member_discount;
	int							sq_PW_ID;
	int							sq_DrPrescNum;
	TDate						sq_stopusedate;
	TDate						sq_maxdrugdate;
	TDate						max_drug_date;
	short int					sq_thalf;
	TDate						sq_DateDB;
	TMonthLog					sq_MonthLogDB;
	int							v_count_double_rec;
	short						v_MemberSpecPresc;
	TDate						sq_StopBloodDate;
	int							sq_Op;
	int							sq_Units;
	int							sq_tikra_discount;
	int							sq_subsidy_amount;
	int							tr6003_date;
	int							tr6003_time;
	long						Dummy_online_order_num;	// So we can use the same SQL command as Trn. 6005.


	// Body of function.

	// Initialize variables.
	REMEMBER_ISOLATION;
	PosInBuff	= IBuffer;
	v_ErrorCode	= NO_ERROR;
	SysDate		= GetDate ();
	memset ((char *)&phDrgs [0],	0, sizeof (phDrgs));
	memset ((char *)&dbDrgs,		0, sizeof (dbDrgs));


	// Read message fields data into variables.
	v_PharmNum					= GetLong	(&PosInBuff, TPharmNum_len					); CHECK_ERROR();	//  10,  7
	v_InstituteCode				= GetShort	(&PosInBuff, TInstituteCode_len				); CHECK_ERROR();	//  17,  2
	v_TerminalNum				= GetShort	(&PosInBuff, TTerminalNum_len				); CHECK_ERROR();	//  19,  2
	v_ActionType				= GetShort	(&PosInBuff, 2								); CHECK_ERROR();	//  21,  2
	v_RetnReasonCode			= GetShort	(&PosInBuff, 2								); CHECK_ERROR();	//  23,  2
	v_5003CommError				= GetShort	(&PosInBuff, 4								); CHECK_ERROR();	//  25,  4
	v_DeletedPrID				= GetLong	(&PosInBuff, TRecipeIdentifier_len			); CHECK_ERROR();	//  29,  9
	v_DeletedPrPharm			= GetLong	(&PosInBuff, TPharmNum_len					); CHECK_ERROR();	//  38,  7
	v_DeletedPrYYMM				= GetShort	(&PosInBuff, 4								); CHECK_ERROR();	//  45,  4
	v_DeletedPrPhID				= GetLong	(&PosInBuff, TPharmRecipeNum_len			); CHECK_ERROR();	//  49,  6
	v_DeletedPrDate				= GetLong	(&PosInBuff, 8								); CHECK_ERROR();	//  55,  8
	v_MemberID					= GetLong	(&PosInBuff, TMemberIdentification_len		); CHECK_ERROR();	//  63,  9
	v_IDCode					= GetShort	(&PosInBuff, TIdentificationCode_len		); CHECK_ERROR();	//  72,  1
	v_MemberBelongCode			= GetShort	(&PosInBuff, TMemberBelongCode_len			); CHECK_ERROR();	//  73,  2
	v_CardOwnerID				= GetLong	(&PosInBuff, TMemberIdentification_len		); CHECK_ERROR();	//  75,  9
	v_CardOwnerIDCode			= GetShort	(&PosInBuff, TIdentificationCode_len		); CHECK_ERROR();	//  84,  1
	v_MoveCard					= GetShort	(&PosInBuff, TMoveCard_len					); CHECK_ERROR();	//  85,  4
	v_DocIDType					= GetShort	(&PosInBuff, TTypeDoctorIdentification_len	); CHECK_ERROR();	//  89,  1
	v_DoctorID					= GetLong	(&PosInBuff, TDoctorIdentification_len		); CHECK_ERROR();	//  90,  9
	v_ElectPrStatus				= GetShort	(&PosInBuff, 1								); CHECK_ERROR();	//  99,  1
	v_RecipeSource			  	= GetShort	(&PosInBuff, TRecipeSource_len				); CHECK_ERROR();	// 100,  2
	v_UserIdentification		= GetLong	(&PosInBuff, TUserIdentification_len		); CHECK_ERROR();	// 102,  9
	v_SpecialConfSource			= GetShort	(&PosInBuff, TSpecConfNumSrc_len			); CHECK_ERROR();	// 111,  1
	v_SpecialConfNum			= GetLong	(&PosInBuff, TNumofSpecialConfirmation_len	); CHECK_ERROR();	// 112,  8
	v_PharmRecipeNum			= GetLong	(&PosInBuff, TPharmRecipeNum_len			); CHECK_ERROR();	// 120,  6
	v_RecipeIdentifier			= GetLong	(&PosInBuff, TRecipeIdentifier_len			); CHECK_ERROR();	// 126,  9
	v_DeliverConfirmationDate	= GetLong	(&PosInBuff, TDeliverConfirmationDate_len	); CHECK_ERROR();	// 135,  8
	v_DeliverConfirmationHour	= GetLong	(&PosInBuff, TDeliverConfirmationHour_len	); CHECK_ERROR();	// 143,  6

	// Note: This will be the Credit Type Used. In some circumstances, it may be
	// different from the Credit Type that was sent to the pharmacy - for example,
	// the member may elect to pay in cash even though s/he has an automated
	// payment ("orat keva") set up. This value will be stored in the new database
	// field credit_type_used.
	v_CreditYesNo				= GetShort	(&PosInBuff, TCreditYesNo_len				); CHECK_ERROR();	// 149,  2
	v_CreditLinePmts			= GetShort	(&PosInBuff, 2								); CHECK_ERROR();	// 151,  2
	v_PaymentType				= GetShort	(&PosInBuff, 2								); CHECK_ERROR();	// 153,  2
	v_InvoiceNum			 	= GetLong	(&PosInBuff, TInvoiceNum_len				); CHECK_ERROR();	// 155,  7
	v_ClientLocationCode		= GetLong	(&PosInBuff, TDeviceCode_len				); CHECK_ERROR();	// 162,  7
	v_DocLocationCode			= GetLong	(&PosInBuff, TDeviceCode_len				); CHECK_ERROR();	// 169,  7
	v_MonthLog					= GetShort	(&PosInBuff, TMonthLog_len					); CHECK_ERROR();	// 176,  4
	v_ReasonToDispense			= GetShort	(&PosInBuff, TElectPR_ReasonToDispLen		); CHECK_ERROR();	// 180,  2
	v_TotalTikraDisc			= GetLong	(&PosInBuff, 9								); CHECK_ERROR();	// 182,  9
	v_TotalCouponDisc			= GetLong	(&PosInBuff, 9								); CHECK_ERROR();	// 191,  9
    v_PharmPtnCode				= GetShort	(&PosInBuff, 4								); CHECK_ERROR();	// 200,  4

	// 16 characters reserved for future use.
	GetString								(&PosInBuff, filler, 16						); CHECK_ERROR();	// 204, 16

	v_NumOfDrugLinesRecs		= GetShort	(&PosInBuff, TElectPR_LineNumLen			); CHECK_ERROR();	// 220,  2


	// Get repeating portion of message (180 characters per drug).
	for (i = 0; i < v_NumOfDrugLinesRecs; i++)
	{
		LineNumber			[i]		= GetShort	(&PosInBuff, TElectPR_LineNumLen			); CHECK_ERROR();	// 222,  2
		v_DoctorRecipeNum	[i]		= GetLong	(&PosInBuff, TDoctorRecipeNum_len			); CHECK_ERROR();	// 224,  6
		PR_Date				[i]		= GetLong	(&PosInBuff, TDate_len						); CHECK_ERROR();	// 230,  8

		// DonR 02Mar2016: For now, Largo Codes are only five digits even though the message space for them
		// has expanded. Since six-digit Largo Codes blow up transmission of tables to AS/400 (and we've seen
		// this happen only for Doctor Largo Code), catch this problem right at the outset and reject the
		// transaction.
		// DonR 15Jul2024 User Story #349368: Largo Codes are being expanded to 6 digits on AS/400.
		// Accordingly, adjust the "sanity check" to allow one more digit.
		PR_Orig_Largo_Code	[i]		= GetLong	(&PosInBuff, 9								); CHECK_ERROR();	// 238,  9
		if (PR_Orig_Largo_Code [i] > 999999)
			SET_ERROR (ERR_WRONG_FORMAT_FILE);

		phDrgs[i].DrugCode			= GetLong	(&PosInBuff, 9								); CHECK_ERROR();	// 247,  9
		PR_SubstPermitted	[i]		= GetShort	(&PosInBuff, TElectPR_SubstPermLen			); CHECK_ERROR();	// 256,  2

		// These are the numbers that were sent in Transaction 2001; comparing them with OP/Units Sold will
		// tell us if this is a full or a partial sale.
		PrevUnsoldUnits		[i]		= GetLong	(&PosInBuff, TUnits_len						); CHECK_ERROR();	// 258,  5
		PrevUnsoldOP		[i]		= GetLong	(&PosInBuff, TUnits_len						); CHECK_ERROR();	// 263,  5

		// Units/OP actually being sold:
		phDrgs[i].Units				= GetLong	(&PosInBuff, TUnits_len						); CHECK_ERROR();	// 268,  5
		phDrgs[i].Op				= GetLong	(&PosInBuff, TOp_len						); CHECK_ERROR();	// 273,  5

		PR_NumPerDose		[i]		= GetShort	(&PosInBuff, TUnits_len						); CHECK_ERROR();	// 278,  5
		PR_DosesPerDay		[i]		= GetShort	(&PosInBuff, TUnits_len						); CHECK_ERROR();	// 283,  5
		phDrgs[i].Duration			= GetShort	(&PosInBuff, TDuration_len					); CHECK_ERROR();	// 288,  3
		phDrgs[i].LinkDrugToAddition= GetLong	(&PosInBuff, 9								); CHECK_ERROR();	// 291,  9
		phDrgs[i].OpDrugPrice		= GetLong	(&PosInBuff, TOpDrugPrice_len				); CHECK_ERROR();	// 300,  9
		phDrgs[i].SupplierDrugPrice	= GetLong	(&PosInBuff, TSupplierPrice_len				); CHECK_ERROR();	// 309,  9
		v_MemberParticType			= GetShort	(&PosInBuff, TMemberParticipatingType_len	); CHECK_ERROR();	// 318,  4
		FixedParticipPrice	[i]		= GetLong	(&PosInBuff, TOpDrugPrice_len				); CHECK_ERROR();	// 322,  9
		v_MemberDiscPC		[i]		= GetShort	(&PosInBuff, TMemberDiscountPercent_len		); CHECK_ERROR();	// 331,  5
		phDrgs[i].TotalDrugPrice	= GetLong	(&PosInBuff, TTotalDrugPrice_len			); CHECK_ERROR();	// 336,  9

		phDrgs[i].TotalMemberParticipation
									= GetLong	(&PosInBuff, TTotalMemberParticipation_len	); CHECK_ERROR();	// 345,  9

		DrugDiscountAmt		[i]		= GetLong	(&PosInBuff, 9								); CHECK_ERROR();	// 354,  9

		// Normally, this can be ignored - since it's written to the DB by Trn. 2003.
		PR_HowParticDetermined [i]	= GetShort	(&PosInBuff, TElectPR_HowPartDetLen			); CHECK_ERROR();	// 363,  2

		phDrgs[i].CurrentStockOp	= GetLong	(&PosInBuff, TElectPR_Stock_len				); CHECK_ERROR();	// 365,  7

		phDrgs[i].CurrentStockInUnits
									= GetLong	(&PosInBuff, TElectPR_Stock_len				); CHECK_ERROR();	// 372,  7

		DrugDiscountCode	[i]		= GetShort	(&PosInBuff, 3								); CHECK_ERROR();	// 379,  3
		InsuranceUsed		[i]		= GetShort	(&PosInBuff, 2								); CHECK_ERROR();	// 382,  2
		DrugTikraType		[i]		= GetChar	(&PosInBuff									); CHECK_ERROR();	// 384,  1
		DrugSubsidized		[i]		= GetChar	(&PosInBuff									); CHECK_ERROR();	// 385,  1
		InHealthBasket		[i]		= GetShort	(&PosInBuff, 1								); CHECK_ERROR();	// 386,  1

		// 15 characters reserved for future use.
		GetString								(&PosInBuff, filler, 15						); CHECK_ERROR();	// 387, 15


		// DonR 12Jul2005: Avoid DB errors due to ridiculously high OP numbers from pharmacy.
		PRW_Op_Update[i] = ((phDrgs[i].Op <= 32767) && (phDrgs[i].Op >= 0)) ? phDrgs[i].Op : 32767;

	}	// Drug-line reading loop.


	// Read in additional drug sales being charged to the same Credit Line.
	// NIU (i.e. the number of lines should always be zero) as of 24May2010.
	NumOtherSales = GetShort (&PosInBuff, TElectPR_LineNumLen); CHECK_ERROR();

	for (i = 0; i < NumOtherSales; i++)
	{
		LineNumber			[i]		= GetShort	(&PosInBuff, TElectPR_LineNumLen			); CHECK_ERROR();
		OtherSaleID			[i]		= GetLong	(&PosInBuff, TRecipeIdentifier_len			); CHECK_ERROR();
		OtherSaleCredLine	[i]		= GetShort	(&PosInBuff, 1								); CHECK_ERROR();
		// 10 characters reserved for future use.
		GetString								(&PosInBuff, filler, 10						); CHECK_ERROR();
	}


	// Get amount of input not eaten
	ChckInputLen( TotalInputLen - (PosInBuff - IBuffer) );
	CHECK_ERROR();

	// Copy values to ssmd_data structure.
	ssmd_data_ptr->pharmacy_num		= v_PharmNum;
	ssmd_data_ptr->member_id		= v_MemberID;
	ssmd_data_ptr->member_id_ext	= v_IDCode;

	// Validate Date and Time.
	ChckDate (v_DeliverConfirmationDate);
	CHECK_ERROR();	 

	ChckTime (v_DeliverConfirmationHour);
	CHECK_ERROR();

	
	reStart = MAC_TRUE;

	// SQL Retry Loop.
	for (tries = 0; tries < SQL_UPDATE_RETRIES && reStart == MAC_TRUE; tries++)
	{
		// Initialize response variables.
		v_ErrorCode					= NO_ERROR;
		reStart						= MAC_FALS;
		SysDate						= GetDate ();
		v_Date						= GetDate ();
		v_Hour						= GetTime ();
		max_drug_date				= 0;

		memset ((char *)&dbDrgs, 0, sizeof (dbDrgs));


		// Dummy loop to avoid GOTO.
		do		// Exiting from LOOP will send the reply to pharmacy.
		{
			// Test pharmacy data.
			v_ErrorCode = IS_PHARMACY_OPEN (v_PharmNum, v_InstituteCode, &Phrm_info);

			if (v_ErrorCode != MAC_OK)
			{
				break;
			}

			// DonR 24Jul2013: Removed doctor-equals-member test from Trn. 2005 and 5005; this is already checked
			// in Trn. 2003/5003, and there's no reason to check it again.

			// DonR 25Jan2005:
			// Pharmacy Messages were being checked here - but we weren't returning
			// the answer, because the answer was left out of the specification.
			// Instead of adding a field to Message 2006, we moved the check to
			// Transaction 2003 and we send the result in a previously-unused field
			// in Message 2004 (which was Message Urgent Flag).

			// 20020109
			// if v_MonthLog different from MonthLog  now must check
			// if lasts days of the months =>   possible next month
			MonthLogCalc		= (v_Date / 100) % 10000;	// YYMM.
			MonthLogCalc_next	= MonthLogCalc;
			NextMonthMinDay = ((MonthLogCalc % 100) == 2) ? 23 : 25;

			if (v_MonthLog != MonthLogCalc)
			{
				if (v_Date % 100 > NextMonthMinDay)	// After 25th of current month (or 23rd if it's February).
				{
					if (MonthLogCalc % 100 == 12)	// It's December today.
					{
						MonthLogCalc_next = MonthLogCalc + 89;	// January of the next year.
					}
					else
					{
						MonthLogCalc_next = MonthLogCalc + 1;	// Advance one month in same year.
					}

					if (v_MonthLog != MonthLogCalc_next)
					{
						v_ErrorCode = ERR_PRESCR_PROBL_DIARY_MONTH;   // Date of sale problem.
					}
				}

				else
				{
					v_ErrorCode = ERR_PRESCR_PROBL_DIARY_MONTH;   // Date of sale problem.
				}
			}	// v_MonthLog != MonthLogCalc



			// Get Member data.
			if ((v_MemberID == 0) && (v_RecipeSource != RECIP_SRC_NO_PRESC))
			{
				v_ErrorCode = ERR_MEMBER_ID_CODE_WRONG;
				break;
			}

			// DonR 05Sep2006: Don't bother checking for non-Maccabi OTC purchases.
			if (v_MemberID > 0)
			{
				ExecSQL (	MAIN_DB, READ_members_MaxDrugDate_and_SpecPresc,
							&sq_maxdrugdate,	&v_MemberSpecPresc,
							&v_MemberID,		&v_IDCode,
							END_OF_ARG_LIST										);

				Conflict_Test (reStart);

				if (SQLERR_code_cmp (SQLERR_not_found) == MAC_TRUE)
				{
					v_ErrorCode = ERR_MEMBER_ID_CODE_WRONG;
					break;
				}

				if (SQLERR_error_test ())
				{
					v_ErrorCode = ERR_DATABASE_ERROR;
					break;
				}
			}
			else
			{
				// Set values for non-Maccabi OTC purchase.
				sq_maxdrugdate = v_MemberSpecPresc = 0;
			}


			// User prescription global data.
			ExecSQL (	MAIN_DB, READ_prescriptions_for_sale_completion,
						&sq_pharmnum,				&sq_institutecode,
						&sq_identificationcode,		&sq_MemberID,
						&sq_memberbelongcode,		&sq_doctoridentification,
						&sq_DocIDType,				&sq_recipesource,
						&sq_errorcode,				&sq_numofdruglinesrecs,

						&sq_terminalnum,			&sq_pricelistnum,
						&sq_member_discount,		&sq_DateDB,
						&sq_MonthLogDB,				&sq_ActionType,
						&sq_DeletedPrID,			&sq_tikra_discount,
						&sq_subsidy_amount,			&Dummy_online_order_num,
						&tr6003_date,				&tr6003_time,

						&v_RecipeIdentifier,		END_OF_ARG_LIST				);

			Conflict_Test (reStart);

			if (SQLERR_code_cmp (SQLERR_not_found) == MAC_TRUE)
			{
				v_ErrorCode = ERR_PRESCRIPTION_ID_NOT_FOUND;
				break;
			}

			if (SQLERR_error_test ())
			{
				v_ErrorCode = ERR_DATABASE_ERROR;
				break;
			} 

			// DonR 24Jul2013: Remember the previous error code, so we don't overwrite it.
			prev_pr_errorcode = sq_errorcode;


			// DonR 15Mar2011: If this is a deletion of a drug sale that arrived via spool, the pharmacy doesn't (yet)
			// know the Prescription ID of the original sale, and so v_DeletedPrID will be zero. However, in Trn. 5003
			// for the deletion, we will have already searched the database and found the original sale; so we can
			// replace the zero sent by the pharmacy with the Prescription ID we've already stored in the table.
			if ((v_ActionType == SALE_DELETION) && (v_DeletedPrID == 0))
			{
				v_DeletedPrID = sq_DeletedPrID;
			}


			// Logical data tests.
			if (sq_errorcode != NO_ERROR)
			{
				Sever = GET_ERROR_SEVERITY (sq_errorcode);

				if (Sever >= FATAL_ERROR_LIMIT)
				{
					v_ErrorCode = ERR_PRESCRIPTIONID_NOT_APPROVED;
					break;
				}
			}

			// DonR 11Aug2016: Additional checks on Total Tikra Discount and
			// Total Subsidy Discount. (Note that the actual rejection logic is below,
			// in the "ElyCount" section - this is just a diagnostic!)
//			if (v_TotalTikraDisc	!= sq_tikra_discount)
//			{
//				GerrLogMini (GerrId,
//							 "5005: %d: Tikra disc %d, pharm %d sent %d%s.",
//							 v_RecipeIdentifier, sq_tikra_discount, v_PharmNum, v_TotalTikraDisc, (TestSaleEquality) ? "" : " (test disabled)");
//			}
//
//			if (v_TotalCouponDisc	!= sq_subsidy_amount)
//			{
//				GerrLogMini (GerrId,
//							 "5005: %d: Subsidy %d, pharm %d sent %d%s.",
//							 v_RecipeIdentifier, sq_subsidy_amount, v_PharmNum, v_TotalCouponDisc, (TestSaleEquality) ? "" : " (test disabled)");
//			}


			ElyCount = 0;

			// Insert the following line after each line in if()
			// in order to catch the first true among all conditions :
			//    || ! ++ElyCount	
			//
			if ((sq_pharmnum				!= v_PharmNum			)							|| ! ++ElyCount ||
				(sq_institutecode			!= v_InstituteCode		)							|| ! ++ElyCount ||
				(sq_MemberID				!= v_MemberID			)							|| ! ++ElyCount ||
				(sq_identificationcode		!= v_IDCode				)							|| ! ++ElyCount ||
				(sq_memberbelongcode		!= v_MemberBelongCode	)							|| ! ++ElyCount ||
				(sq_doctoridentification	!= v_DoctorID			)							|| ! ++ElyCount ||
				(sq_DocIDType				!= v_DocIDType			)							|| ! ++ElyCount ||
				(sq_recipesource			!= v_RecipeSource		)							|| ! ++ElyCount ||
				(sq_numofdruglinesrecs		<  v_NumOfDrugLinesRecs	)							|| ! ++ElyCount ||
				(sq_ActionType				!= v_ActionType			)							|| ! ++ElyCount ||
				((sq_tikra_discount			!= v_TotalTikraDisc		) && (TestSaleEquality))	|| ! ++ElyCount ||
				((sq_subsidy_amount			!= v_TotalCouponDisc	) && (TestSaleEquality))	|| ! ++ElyCount ||
				(sq_terminalnum				!= v_TerminalNum		)							|| ! ++ElyCount
			   )
			{
				v_ErrorCode = ERR_PRESCRIPTION_ID_NOT_EQUAL;
				break;
			}


			// Test prescription drugs data.
			if (sq_DateDB != v_DeliverConfirmationDate )   //20020109
			{
				v_ErrorCode = ERR_PRESCRIPTION_PROBL_DATE;   // Date of sale problem.
				break;
			}

			// DonR 04 Mar 2003: Skip this test if the pharmacy hasn't given us a
			// Pharmacy Prescription Number.
			// DonR 23Jul2006: Don't do this test if the sale is coming from a
			// private pharmacy and the Member ID is zero.
			if ((v_RecipeIdentifier > 0)					&&
				(v_PharmRecipeNum > 0)						&&
				((MACCABI_PHARMACY) || (v_RecipeSource != RECIP_SRC_NO_PRESC)))
			{
				// Check for duplicate Pharmacy Prescription Number.
				// DonR 29Jul2007: Re-ordered selects to follow index structure; also added
				// a "dummy" select by member_price_code, just to make sure that Informix
				// uses the best applicable index.
				//
				// DonR 01Dec2009: This check doesn't need to be in committed mode, so we can set isolation
				// to "dirty" to avoid locked-record errors.
				//
				// DonR 29Jun2010: Got rid of dummy member_price_code select; I'll change the relevant index
				// to eliminate this irrelevant field as well. Also got rid of the select by date, which
				// hasn't been relevant for years.
				//
				// DonR 08Feb2011: Change test on delivered_flg, since we're no longer using only 1 and 0 there.
				SET_ISOLATION_DIRTY;

				ExecSQL (	MAIN_DB, READ_count_duplicate_drug_sales,
							&v_count_double_rec,
							&v_PharmNum,			&v_MonthLog,			&v_PharmRecipeNum,
							&v_MemberBelongCode,	&DRUG_NOT_DELIVERED,	END_OF_ARG_LIST		);

				Conflict_Test (reStart);

				if (SQLERR_code_cmp (SQLERR_not_found) == MAC_TRUE)
				{
					v_ErrorCode = ERR_PRESCRIPTION_ID_NOT_FOUND;
					break;
				}

				if (SQLERR_error_test ())
				{
					v_ErrorCode = ERR_DATABASE_ERROR;
					break;
				} 

				if (v_count_double_rec > 0 )   //20020115
				{
					v_ErrorCode = ERR_PRESCRIPTION_DOUBLE_NUM;   //double presc_pharm number
					break;
				}

			}	// RecipeIdentifier > 0 and Pharmacy Prescr. ID > 0


			// Open Prescription Drugs cursor.
			// DonR 01Dec2009: Return to committed-read mode.
			SET_ISOLATION_COMMITTED;

			// Select cursors.
			DeclareAndOpenCursorInto (	MAIN_DB, TR5005_presc_drug_cur,
										&dbDrgs.DrugCode,				&dbDrgs.Op,
										&dbDrgs.Units,					&dbDrgs.LineNo,
										&dbDrgs.LinkDrugToAddition,		&dbDrgs.OpDrugPrice,
										&dbDrgs.SupplierDrugPrice,		&sq_errorcode,
										&sq_stopusedate,				&sq_thalf,
										&sq_PW_ID,						&sq_DrPrescNum,
										&dbDrgs.ParticipMethod,			&dbDrgs.member_ptn_amt,
										&dbDrgs.MacabiPriceFlag,		&sq_StopBloodDate,
										&dbDrgs.dr_visit_date,			&dbDrgs.dr_presc_date,
										&dbDrgs.dr_largo_code,
										&v_RecipeIdentifier,			&DRUG_NOT_DELIVERED,
										END_OF_ARG_LIST												);

			if (SQLERR_error_test ())
			{
				v_ErrorCode = ERR_DATABASE_ERROR;
				break;
			}

			// Set all drug lines from 2005 to "unmatched".
			for (i = 0; i < v_NumOfDrugLinesRecs; i++)
			{
				phDrgs[i].Flg = 0;
			}


			// Fetch and test prescription_drugs rows against drug lines in 5005.
			// Loop until we break out.
			do
			{
				FetchCursor (	MAIN_DB, TR5005_presc_drug_cur	);

				Conflict_Test (reStart);

				if (SQLERR_code_cmp (SQLERR_end_of_fetch) == MAC_TRUE)
				{
					break;
				}

				if (SQLERR_error_test ())
				{
					SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR);
					break;
				}


				// Find drug in buffer and test it against database values.
				for (i = 0; i < v_NumOfDrugLinesRecs; i++)
				{
					if ((phDrgs[i].DrugCode != dbDrgs.DrugCode) || (phDrgs[i].Flg   != 0))
					{
						continue;
					}

					phDrgs[i].LineNo			= dbDrgs.LineNo;
					phDrgs[i].ParticipMethod	= dbDrgs.ParticipMethod;
					phDrgs[i].MacabiPriceFlag	= dbDrgs.MacabiPriceFlag;
					phDrgs[i].dr_visit_date		= dbDrgs.dr_visit_date;
					phDrgs[i].Flg				= 1;
					phDrgs[i].DiscountPercent	= v_MemberDiscPC[i];
					PrescWrittenID		[i]		= sq_PW_ID;
					StopBloodDate		[i]		= sq_StopBloodDate;
					PR_Date				[i]		= dbDrgs.dr_presc_date;
					PR_Orig_Largo_Code	[i]		= dbDrgs.dr_largo_code;

					// DonR 09Feb2009: Per Iris Shaya, certain pharmacies (SuperPharm
					// in Sderot and Tamarim in Eilat) can change the Supplier Drug Price
					// for certain items; in this case, we want to update the drug-sale
					// row with the new Supplier Drug Price. Since other "Maccabi"
					// pharmacies won't change the price, doing the update won't
					// change anything. But if the conditions aren't met, we want
					// to make sure the existing database value is preserved.
					if ((dbDrgs.DrugCode < 90000) || (PRIVATE_PHARMACY))
					{
						phDrgs[i].SupplierDrugPrice = dbDrgs.SupplierDrugPrice;
					}

					// DonR 18Apr2005: If Doctor Prescription ID is different from what
					// pharmacy reported, just assume that the pharmacy value is bogus
					// and replace it.
					if (sq_DrPrescNum != v_DoctorRecipeNum[i])
					{
						v_DoctorRecipeNum[i] = sq_DrPrescNum;
					}

					// Test for same Link to Addition.
					if (phDrgs[i].LinkDrugToAddition != dbDrgs.LinkDrugToAddition)
					{
						v_ErrorCode = ERR_PRESCRIPTION_ID_NOT_EQUAL;
//						GerrLogMini (GerrId,
//									 "Link-to-addition mismatch: 5003 = %d, 5005 = %d.",
//									 phDrgs[i].LinkDrugToAddition,
//									 dbDrgs.LinkDrugToAddition);
						break;
					}

					// DonR 11Aug2016: Pharmacy must send the same Member Participation Amount as they got
					// from Transaction 5003 - if not, reject the transaction. Note that if this a deletion,
					// we have to compare a "flipped" version of the participation amount, since the pharmacy
					// sends it as a positive number and the database already has it as a negative number.
					if ((v_RecipeSource != RECIP_SRC_NO_PRESC) && (TestSaleEquality))
					{
						ComparePharmPtn = (v_ActionType == SALE_DELETION) ? (0 - phDrgs[i].TotalMemberParticipation) : phDrgs[i].TotalMemberParticipation;

						if (((PRIVATE_PHARMACY) || (DrugDiscountCode[i] == 0)) && (ComparePharmPtn != dbDrgs.member_ptn_amt))
						{
							v_ErrorCode = ERR_PRESCRIPTION_ID_NOT_EQUAL;
//							GerrLogMini (GerrId,
//										 "5005: %d/%d: Ptn = %d, pharm %d sent %d.",
//										 v_RecipeIdentifier, phDrgs[i].LineNo, dbDrgs.member_ptn_amt, v_PharmNum, ComparePharmPtn);
						}
					}

					// See if any errors discovered so far are severe enough
					// to invalidate the drug.
					if (sq_errorcode != NO_ERROR)
					{
						Sever = GET_ERROR_SEVERITY (sq_errorcode);

						if (Sever > FATAL_ERROR_LIMIT)
						{
							v_ErrorCode = ERR_PRESCRIPTIONID_NOT_APPROVED;
							break;
						}
					}


					// Update max drug date for member
					len   = AddDays (sq_stopusedate, sq_thalf);

					if (max_drug_date < len)
					{
						max_drug_date = len;
					}

					break;

				}	// Loop through drugs to match lines in 2005 against
					// the current prescription_drugs row.

				// See if any errors discovered so far are severe enough
				// to invalidate the drug.
				if (v_ErrorCode != NO_ERROR)
				{
					Sever = GET_ERROR_SEVERITY (v_ErrorCode);

					if (Sever > FATAL_ERROR_LIMIT)
					{
						break;
					}
				}

			}
			while (1);	// End of fetch-and-test prescription_drugs rows loop.


			// Close Prescription Drugs cursor.
			CloseCursor (	MAIN_DB, TR5005_presc_drug_cur	);


			// Test if any drugs in 5005 were not found in prescription_drugs table -
			// pharmacist is allowed to add drugs between 2001 and 5003, but
			// not between 5003 and 5005.
			//
			// While we're at it, check that drug is in the drug_list table,
			// load in its Largo Type and packaging details, and see if the
			// drug is also in the Gadgets table.

			// DonR 21Apr2005: Avoid unnecessary "table locked" errors.
			SET_ISOLATION_DIRTY;

			for (i = 0; i < v_NumOfDrugLinesRecs; i++)
			{
				in_gadget_table[i] = 0;

				if (phDrgs[i].Flg == 0)		// I.E. drug wasn't in DB select list.
				{
					v_ErrorCode = ERR_PRESCRIPTION_ID_NOT_EQUAL;
					break;
				}


				// Read drug_list data by largo code.
				// DonR 14Apr2016: Switch to using the read_drug() routine, since that includes more sophisticated
				// data-massaging logic.
				if (read_drug (phDrgs[i].DrugCode,
							   99999999,
							   &Phrm_info,
							   (v_ActionType == SALE_DELETION),	// Deleted drugs are "visible" if we're deleting prior sales.
							   &DL,
							   NULL))
				{
					phDrgs[i].LargoType			= DL.largo_type;
					phDrgs[i].package_size		= DL.package_size;
					phDrgs[i].openable_package	= DL.openable_pkg;
					v_in_gadget_table			= DL.in_gadget_table;
					v_ingr_1_code				= DL.Ingredient[0].code;
					v_ingr_2_code				= DL.Ingredient[1].code;
					v_ingr_3_code				= DL.Ingredient[2].code;
					v_ingr_1_quant				= DL.Ingredient[0].quantity;
					v_ingr_2_quant				= DL.Ingredient[1].quantity;
					v_ingr_3_quant				= DL.Ingredient[2].quantity;
					v_ingr_1_quant_std			= DL.Ingredient[0].quantity_std;
					v_ingr_2_quant_std			= DL.Ingredient[1].quantity_std;
					v_ingr_3_quant_std			= DL.Ingredient[2].quantity_std;
					v_per_1_quant				= DL.Ingredient[0].per_quant;
					v_per_2_quant				= DL.Ingredient[1].per_quant;
					v_per_3_quant				= DL.Ingredient[2].per_quant;
					v_package_volume			= DL.package_volume;
				}
				else
				{
					Conflict_Test (reStart);

					if (SQLERR_code_cmp (SQLERR_not_found) == MAC_TRUE)
					{
						v_ErrorCode = ERR_DRUG_CODE_NOT_FOUND;
						break;
					}

					if (SQLERR_error_test ())
					{
						v_ErrorCode = ERR_DATABASE_ERROR;
						break;
					}
				}

				// Save the drug's "in gadget table" status.
				in_gadget_table[i] = v_in_gadget_table;

				// Save generic-ingredient fields as well.
				package_volume	[i]    = v_package_volume;
				ingr_code		[i][0] = v_ingr_1_code;
				ingr_code		[i][1] = v_ingr_2_code;
				ingr_code		[i][2] = v_ingr_3_code;
				ingr_quant		[i][0] = v_ingr_1_quant;
				ingr_quant		[i][1] = v_ingr_2_quant;
				ingr_quant		[i][2] = v_ingr_3_quant;
				ingr_quant_std	[i][0] = v_ingr_1_quant_std;
				ingr_quant_std	[i][1] = v_ingr_2_quant_std;
				ingr_quant_std	[i][2] = v_ingr_3_quant_std;
				per_quant		[i][0] = v_per_1_quant;
				per_quant		[i][1] = v_per_2_quant;
				per_quant		[i][2] = v_per_3_quant;

			}	// Loop on drugs to get largo type, package size, and openability.


			// DonR 21Apr2005: Restore "normal" database isolation.
			SET_ISOLATION_COMMITTED;


			// If we broke out of the above loop prematurely, something went wrong!
			if (i < v_NumOfDrugLinesRecs)
				break;


			// End of dummy loop to prevent goto.
		}
		while (0);	// Loop should run only once.

		// DonR 01Dec2009: Just to be on the safe side, restore normal database isolation.
		// This should be necessary only if we hit a DB error while we were in "dirty read"
		// mode.
		SET_ISOLATION_COMMITTED;

		// Update tables.
		//
		// And now...
		// Yet another dummy loop to avoid goto!
		// Reply is sent to pharmacy when we exit from loop.

		// DonR 04Aug2010: If there is an error, write it to the prescriptions table so we know
		// why the sale wasn't completed.
		if (!SetErrorVar (&v_ErrorCode, v_ErrorCode))	// No problem so far
		{
			do
			{
				// Update PHARMACY_DAILY_SUM table.
				// DonR 05Aug2009: Don't add private-pharmacy non-prescription sales to pharmacy_daily_sum.
				// DonR 05Jul2010: Note that for sale deletions, the pharmacy will report positive numbers
				// for OP/Units and Total Member Participation; we carry these numbers internally (in
				// prescriptions/prescription_drugs) as negative numbers.
				// DonR 23Aug2010: Per Iris Shaya, Total Tikra Discount and Total Coupon Discount need to be
				// subtracted from the participation amount for this sale.
				// DonR 11Jan2011: For sale deletions, AS/400 will report Tikra and Coupon reductions in
				// participation as negative numbers. This means that in calculating the member participation
				// written to pharmacy_daily_sum, these numbers need to be added, not subtracted. Accordingly,
				// this bit of math will be split into two versions and moved to the "if" based on the current
				// Action Type.
				if ((MACCABI_PHARMACY) || (v_RecipeSource != RECIP_SRC_NO_PRESC))
				{
					// Calculate prescription total
					sq_TotalMemberPrice	= 0;
					sq_TotalDrugPrice	= 0;
					sq_TotSuppPrice		= 0;

					for (i = 0; i < v_NumOfDrugLinesRecs; i++)
					{
						sq_TotalMemberPrice	+= phDrgs[i].TotalMemberParticipation;
						sq_TotalDrugPrice	+= phDrgs[i].TotalDrugPrice;

						sq_TotSuppPrice		+= phDrgs[i].SupplierDrugPrice * phDrgs[i].Op;

						if (phDrgs[i].Units != 0)
						{
							sq_TotSuppPrice	+= (int)(  (float)phDrgs[i].SupplierDrugPrice
													 * (float)phDrgs[i].Units
													 / (float)phDrgs[i].package_size);
						}
					}


					// Set up variables to insert/update Pharmacy Daily Sum data. There are three
					// possibilities:
					// 1) Drug sale.
					// 2) Conventional sale deletion.
					// 3) Deletion for prior month or different pharmacy; this gets recorded as a
					//    "negative sale" to the pharmacy performing the deletion.
					if (v_ActionType == DRUG_SALE)
					{
						sq_TotalMemberPrice	-= (v_TotalTikraDisc + v_TotalCouponDisc);	// DonR 23Aug2010 / 11Jan2011.
						PDS_sum				=  sq_TotalDrugPrice;
						PDS_prt_sum			=  sq_TotalMemberPrice;
						PDS_purch_sum		=  sq_TotSuppPrice;
						PDS_lines			=  v_NumOfDrugLinesRecs;
						PDS_count			=  1;
						PDS_del_sum			=  0;
						PDS_del_prt_sum		=  0;
						PDS_del_purch_sum	=  0;
						PDS_del_lines		=  0;
						PDS_del_count		=  0;
						PDS_date			=  SysDate;
					}
					else
					{	// The only "legal" alternative to DRUG_SALE for Trn. 5005 is SALE_DELETION.

						// DonR 11Jan2011: For deletions, discounts are sent by pharmacy as negative numbers.
						sq_TotalMemberPrice	+= (v_TotalTikraDisc + v_TotalCouponDisc);	// DonR 11Jan2011.

						if ((v_PharmNum == v_DeletedPrPharm) && (v_MonthLog == v_DeletedPrYYMM))
						{
							// Conventional sale deletion.
							PDS_sum				= 0;
							PDS_prt_sum			= 0;
							PDS_purch_sum		= 0;
							PDS_lines			= 0;
							PDS_count			= 0;
							PDS_del_sum			= sq_TotalDrugPrice;	// Should be positive.
							PDS_del_prt_sum		= sq_TotalMemberPrice;	// Should be positive.
							PDS_del_purch_sum	= sq_TotSuppPrice;		// Should be positive.
							PDS_del_lines		= v_NumOfDrugLinesRecs;
							PDS_del_count		= 1;

							// DonR 02Mar2011: For conventional sale deletions, read the original sale date,
							// since that's the date to be used for updating pharmacy_daily_sum.
							ExecSQL (	MAIN_DB, READ_deleted_sale_date,
										&PDS_date, &v_DeletedPrID, END_OF_ARG_LIST	);

							if (SQLCODE != 0)
								PDS_date = SysDate;
						}
						else
						{
							// Deletion for prior month and/or different pharmacy - treat as "negative sale".
							PDS_sum				= 0 - sq_TotalDrugPrice;	// Should be positive, so subtract.
							PDS_prt_sum			= 0 - sq_TotalMemberPrice;	// Should be positive, so subtract.
							PDS_purch_sum		= 0 - sq_TotSuppPrice;		// Should be positive, so subtract.
							PDS_lines			= v_NumOfDrugLinesRecs;
							PDS_count			= 1;
							PDS_del_sum			= 0;
							PDS_del_prt_sum		= 0;
							PDS_del_purch_sum	= 0;
							PDS_del_lines		= 0;
							PDS_del_count		= 0;
							PDS_date			= SysDate;
						}
					}

					// DonR 21Mar2011: For better performance, try updating rather than inserting first.
					// The vast majority of the time, we'll be updating an existing row - so why waste
					// time trying to insert, failing, and only then updating?
					// DonR 03May2015: Add a retry loop, since I'm seeing cases where the UPDATE fails but
					// then the INSERT fails too, with SQL duplicate key error.
					// DonR 13Jul2016: Added one more retry, and increased the pause before retry from five
					// to fifty milliseconds; this should, I hope, reduce the number of transactions that
					// fail with a DB error.
					// DonR 09Aug2016: Doubled sleep interval and added a third retry.
					for (i = 0; i < 4; i++)
					{
						if (i > 0)
							usleep (100000);	// Sleep 100 milliseconds before retry.

						ExecSQL (	MAIN_DB, UPD_pharmacy_daily_sum,
									&PDS_sum,			&PDS_prt_sum,			&PDS_purch_sum,
									&PDS_count,			&PDS_lines,				&PDS_del_sum,
									&PDS_del_prt_sum,	&PDS_del_lines,			&PDS_del_count,
									&PDS_del_purch_sum,
									&v_PharmNum,		&v_InstituteCode,		&v_MonthLog,
									&PDS_date,			&v_TerminalNum,			END_OF_ARG_LIST		);

						// DonR 31Dec2015: If the UPDATE succeeded, we need to break out of the retry loop so we don't 
						// perform the update twice! (In fact, the *only* situation in which we want to stay in the loop
						// is if both the UPDATE and the INSERT failed with "no rows affected" and "duplicate key",
						// respectively.)
						// DonR 05Jan2016 HOT FIX: If the update found nothing to update, SQLCODE will still be equal
						// to zero - so we have to look at "no rows affected" separately.
						if (DB_UPDATE_WORKED)
							break;	// We succeeded in the UPDATE, so abort the retry loop.

						// If there was nothing to update, try inserting a new row.
						if (SQLERR_code_cmp (SQLERR_no_rows_affected) == MAC_TRUE)
						{
							ExecSQL (	MAIN_DB, INS_pharmacy_daily_sum_19_columns,
										&v_PharmNum,		&v_InstituteCode,		&v_MonthLog,
										&PDS_date,			&v_TerminalNum,			&PDS_sum,
										&PDS_count,			&PDS_lines,				&PDS_prt_sum,
										&PDS_purch_sum,		&PDS_del_sum,			&PDS_del_prt_sum,

										&PDS_del_lines,		&PDS_del_count,			&PDS_del_purch_sum,
										&IntZero,			&IntZero,				&IntZero,
										&IntZero,			END_OF_ARG_LIST										);

							if (SQLERR_code_cmp (SQLERR_not_unique) == MAC_FALSE)
								break;	// Retry *only* on duplicate-key error.

						}	// No rows affected on UPDATE, so try INSERT.

						// DonR 31Dec2015: If we get here, either the UPDATE failed with something *other* than "no rows affected",
						// or else the UPDATE got "no rows affected" *AND* the INSERT failed with a duplicate-key error. This
						// situation, and *only* this situation, is when we want to re-attempt the UPDATE.

					}	// Retry loop.

					Conflict_Test (reStart);

					if (SQLERR_error_test ())
					{
						GerrLogMini (GerrId, "5005: DB error %d writing to pharmacy_daily_sum for %d/%d/%d/%d after %d attempts.",
									 SQLCODE, v_PharmNum, v_MonthLog, PDS_date, v_TerminalNum, i);
						SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR);
						break;
					}
				}	// DonR 05Aug2009: Insert/update to pharmacy_daily_sum is now conditional.


				// Update prescriptions-written table.
				for (i = 0; i < v_NumOfDrugLinesRecs; i++)
				{
					phDrgPtr			= &phDrgs			[i];
					PW_ID				= PrescWrittenID	[i];
					PD_discount_pc		= v_MemberDiscPC	[i];
					sq_StopBloodDate	= StopBloodDate		[i];
					v_DrugDiscountAmt	= DrugDiscountAmt	[i];
					v_DrugDiscountCode	= DrugDiscountCode	[i];
					v_DrugCode			= phDrgs[i].DrugCode;

					// Update tables (and AS/400 "Meishar" application) to reflect delivery.
					//
					// DonR 04May2005: Set Reported-to-AS400 flag false only for "real" doctor
					// prescriptions, not those created by Pharmacy transactions.
					if (v_RecipeSource == RECIP_SRC_MACABI_DOCTOR)	// DonR 27Apr2015: Don't bother for non-Maccabi-doctor prescriptions.
					{
						update_doctor_presc (	0,		// Normal mode - NOT processing late-arriving doctor prescriptions.
												v_ActionType,
												v_MemberID,					v_IDCode,
												v_RecipeIdentifier,			phDrgPtr->LineNo,
												v_DoctorID,					v_DocIDType,					v_DoctorRecipeNum[i],
												PR_Orig_Largo_Code[i],		phDrgPtr->DrugCode,				NULL,	// No TDrugListRow structure.
												PR_Date[i],					SysDate,						v_Hour,
												phDrgPtr->Op,				phDrgPtr->Units,
												PrevUnsoldOP[i],			PrevUnsoldUnits[i],
												0 /*phDrgPtr->use_instr_template*/ ,			// DonR 01Aug2024 Bug #334612: Get "ratzif" from pharmacy instead of from doctor Rx.
												v_DeletedPrID,
												1,							NULL,	// Old-style single-prescription mode.
												&PR_Orig_Largo_Code[i],		&PR_Date[i],
												&phDrgs[i].NumRxLinks,
												NULL,	NULL,	NULL,	NULL		// Doctor visit & Rx arrival timestamps - relevant only for spool processing.
											);
					}	// Prescription Source == Maccabi Doctor.


					// DonR 14Jun2004: If "drug" being purchased is a device in the "gadgets"
					// table, retrieve the appropriate information and notify the AS400 of
					// the sale.
					if ((phDrgs[i].ParticipMethod % 10) == FROM_GADGET_APP)
					{
						v_gadget_code	= phDrgs[i].LinkDrugToAddition;
						v_DoctorPrID	= v_DoctorRecipeNum	[i];	// Unless these are bogus values 
						v_DoctorPrDate	= PR_Date			[i];	// inserted by pharmacy in Trn. 5003.
						Unit_Price		= phDrgs[i].OpDrugPrice;	// Unless Maccabi Price is relevant.

						// DonR 23Jun2010: Gadget Action Code is now dependent on whether this is a
						// conventional drug sale or a deletion.
						AS400_Gadget_Action	= (v_ActionType == SALE_DELETION) ? enActSterNo : enActWrite;

						// DonR 09Dec2010: If the current action is a sale deletion, send AS/400 the
						// Prescription ID of the original sale, NOT the ID of the deletion itself.
						GadgetRecipeID = (v_ActionType == SALE_DELETION) ? v_DeletedPrID : v_RecipeIdentifier;

						ExecSQL (	MAIN_DB, READ_Gadgets_for_sale_completion,
									&v_service_code,	&v_service_number,	&v_service_type,
									&v_DrugCode,		&v_gadget_code,		&v_gadget_code,
									END_OF_ARG_LIST												);

						Conflict_Test (reStart);

						if (SQLERR_error_test ())
						{
							SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR);
							break;
						}

						// If we get here, the gadget entry really does exist.
						// DonR 19Aug2025 User Story #442308: If Nihul Tikrot calls are disabled, disable Meishar calls as well.
						if (TikrotRPC_Enabled)
						{
							GadgetError = as400SaveSaleRecord ((int)v_PharmNum,
															   (int)GadgetRecipeID,			// DonR 09Dec2010.
															   (int)v_MemberID,
															   (int)v_IDCode,
															   (int)v_service_code,
															   (int)v_service_number,
															   (int)v_service_type,
															   (int)v_DrugCode,
															   (int)phDrgs[i].Op,
															   (int)Unit_Price,
															   (int)phDrgs[i].TotalMemberParticipation,
															   (int)SysDate,			// Event Date
															   (int)v_DoctorID,
															   (int)v_DoctorPrDate,	// Request Date
															   (int)v_DoctorPrID,		// Request Num.
															   (int)AS400_Gadget_Action,
															   (int)2					// Retries
															  );
							GerrLogFnameMini ("gadgets_log",
											   GerrId,
											   "5005 SaveSaleRecord: AS400 err = %d.",
											   (int)GadgetError);
						}
						else	// If Nihul Tikrot calls are disabled, disable Meishar calls too.
						{
							GadgetError = -1;
						}

						if (GadgetError != 0)
						{
							v_action = AS400_Gadget_Action;

							// Write to "holding" table!
							// DonR 09Dec2010: If the current action is a sale deletion, send AS/400 the
							// Prescription ID of the original sale, NOT the ID of the deletion, in second parameter.
							ExecSQL (	MAIN_DB, INS_gadget_spool,
										&v_PharmNum,							&GadgetRecipeID,
										&v_MemberID,							&v_IDCode,
										&v_DrugCode,							&v_service_code,
										&v_service_number,						&v_service_type,
										&phDrgPtr->Op,							&Unit_Price,

										&phDrgPtr->TotalMemberParticipation,	&SysDate,
										&v_DoctorID,							&v_DoctorPrDate,
										&v_DoctorPrID,							&v_action,
										&GadgetError,							&SysDate,
										&v_Hour,								&IntZero,
										END_OF_ARG_LIST													);

							if (SQLERR_error_test ())
							{
								GerrLogToFileName ("gadgets_log",
												   GerrId,
												   "2005: DB Error %d writing to gadget_spool.",
												   (int)SQLCODE);

								v_ErrorCode = ERR_DATABASE_ERROR;
								break;
							}
						}	// Error notifying AS400 of sale.
					}	// Item sold got its participation from AS400 "Gadget" application.

					else
					{
						// If the "drug" purchased is a "gadget" but we didn't get participation
						// from the AS400, zero out the Link-to-Addition variable so the AS400
						// won't get the wrong idea about where participation came from.
						if (in_gadget_table[i] != 0)
							phDrgs[i].LinkDrugToAddition = 0;
					}


					// If this is a deletion, mark the original sold-drug row as deleted.
					// DonR 04Jul2016: Instead of repeating essentially the same code in eight places (i.e. two
					// tables, Transactions 5005 and 6005, online and spooled versions), created a single function
					// MarkDeleted() that handles the deletion-flag update. It returns zero if all went OK, and
					// anything else indicates a database error.
					if (v_ActionType == SALE_DELETION)
					{
						if (MarkDeleted ("5005  ", v_DeletedPrID, v_DrugCode, v_RecipeIdentifier) != 0)
						{
							SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR);
							break;
						}
					}	// Need to set deleted flag on original sale being deleted.


					// Prepare sold-ingredients variables.
					for (j = 0; j < 3; j++)
					{
						if ((ingr_code	[i][j]		<  1)	||
							(ingr_quant	[i][j]		<= 0.0)	||
							(per_quant	[i][j]		<= 0.0)	||
							(phDrgs[i].package_size <  1))
						{
							// Invalid values - set this slot to zero.
							ingr_code			[i][j] = 0;
							ingr_quant_bot		[i][j] = 0.0;
							ingr_quant_bot_std	[i][j] = 0.0;
						}

						else
						{
							if (phDrgs[i].package_size == 1)
							{
								// Syrups and similar drugs.
								ingr_quant_bot[i][j] =	  (double)phDrgs[i].Op
														* package_volume[i]
														* ingr_quant[i][j]
														/ per_quant	[i][j];

								ingr_quant_bot_std[i][j] =	  (double)phDrgs[i].Op
															* package_volume[i]
															* ingr_quant_std[i][j]
															/ per_quant		[i][j];
							}

							else	// Package size > 1.
							{
								// Tablets, ampules, and similar drugs.
								// For these medications, ingredient is always given per unit.
								UnitsSold	=	  (phDrgs[i].Op		* phDrgs[i].package_size)
												+  phDrgs[i].Units;

								ingr_quant_bot		[i][j] = (double)UnitsSold * ingr_quant		[i][j];
								ingr_quant_bot_std	[i][j] = (double)UnitsSold * ingr_quant_std	[i][j];
							}
						}	// Valid quantity values available.
					}	// Loop through ingredients for this drug.

					// Load DB variables with computed values. Note that the Ingredient Code
					// may have been zeroed out, so it needs to be reloaded!
					v_ingr_1_code		= ingr_code			[i][0];
					v_ingr_2_code		= ingr_code			[i][1];
					v_ingr_3_code		= ingr_code			[i][2];
					v_ingr_1_quant		= ingr_quant_bot	[i][0];
					v_ingr_2_quant		= ingr_quant_bot	[i][1];
					v_ingr_3_quant		= ingr_quant_bot	[i][2];
					v_ingr_1_quant_std	= ingr_quant_bot_std[i][0];
					v_ingr_2_quant_std	= ingr_quant_bot_std[i][1];
					v_ingr_3_quant_std	= ingr_quant_bot_std[i][2];

					// DonR 05Jul2010: Per Iris Shaya, pharmacies send positive numbers for OP/Units,
					// Total Member Participation, and Total (Supplier) Drug Price in sale-deletion
					// requests. We "flip" these numbers to negative when writing to the database.
					// DonR 03Jul2013: Instead of "flipping" the numbers stored in phDrgPtr, use a set
					// of simple variables. This way, when we hit a database lock and execute this block
					// of code more than once, the underlying "real" variables will remain positive and
					// the numbers we write to the database for deletions will be reliably negative.
					if (v_ActionType == SALE_DELETION)
					{
						sq_TotalMemberPrice	= phDrgPtr->TotalMemberParticipation	* -1;
						sq_TotalDrugPrice	= phDrgPtr->TotalDrugPrice				* -1;
						sq_Op				= phDrgPtr->Op							* -1;
						sq_Units			= phDrgPtr->Units						* -1;
					}
					else
					{
						// Ordinary drug sale - just copy the array/structure variables into the simple variables.
						sq_TotalMemberPrice	= phDrgPtr->TotalMemberParticipation;
						sq_TotalDrugPrice	= phDrgPtr->TotalDrugPrice;
						sq_Op				= phDrgPtr->Op;
						sq_Units			= phDrgPtr->Units;
					}

					// Finally, update prescription_drugs. Note that this update has to happen
					// after the "gadget" logic, since the latter can change macabi_price_flg.
					// Note also that delivered_flg gets the same value as Action Type; this allows
					// routines like get_drugs_in_blood() to select only drug sales without having
					// to add an additional element to the WHERE clause.
					// DonR Feb2023 User Story #232220: Add paid_for to columns UPDATEd.
					ExecSQL (	MAIN_DB, TR5005_UPD_prescription_drugs,
								&sq_Op,							&sq_Units,
								&v_ActionType,					&NOT_REPORTED_TO_AS400,
								&v_ElectPrStatus,				&phDrgPtr->CurrentStockOp,
								&phDrgPtr->CurrentStockInUnits,	&sq_TotalDrugPrice,
								&sq_TotalMemberPrice,			&PD_discount_pc,
								&phDrgPtr->SupplierDrugPrice,	&v_DrugDiscountAmt,
								&v_DrugDiscountCode,			&v_ingr_1_code,
								&v_ingr_2_code,					&v_ingr_3_code,
								&v_ingr_1_quant,				&v_ingr_2_quant,
								&v_ingr_3_quant,				&v_ingr_1_quant_std,
								&v_ingr_2_quant_std,			&v_ingr_3_quant_std,
								&v_Date,						&v_Hour,
								&phDrgPtr->MacabiPriceFlag,		&PR_Orig_Largo_Code[i],
								&PR_Date[i],					&phDrgPtr->LinkDrugToAddition,
								&phDrgPtr->NumRxLinks,			&paid_for,

								&v_RecipeIdentifier,			&phDrgPtr->LineNo,
								END_OF_ARG_LIST														);

					Conflict_Test (reStart);

					if (SQLERR_error_test ())
					{
						SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR);
						break;
					}
				}	// Loop through drugs.

				// If any SQL errors were detected inside drug loop, break out of dummy loop.
				// DonR 21Jul2011: If we've already logged an error, there's no need to log it again.
				if (v_ErrorCode == ERR_DATABASE_ERROR)
				{
					break;	// If we have already trapped a DB error, break out of loop.
				}
				else
				{
					if (SQLERR_error_test ())
					{
						SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR);
						break;
					}
				}


				// Update max_drug_date in members table.
				// DonR 05Sep2006: Don't do this for non-Maccabi OTC purchases.
				if ((max_drug_date > sq_maxdrugdate) && (v_MemberID > 0))
				{
					ExecSQL (	MAIN_DB, UPD_members_max_drug_date,
								&max_drug_date,		&v_MemberID,
								&v_IDCode,			END_OF_ARG_LIST		);

					Conflict_Test( reStart );

					if (SQLERR_error_test ())
					{
						SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR);
						break;
					}
				}

				// If this is a deletion of all drugs in a sale, mark the original
				// prescriptions row as deleted.
				// DonR 04Jul2016: Instead of repeating essentially the same code in eight places (i.e. two
				// tables, Transactions 5005 and 6005, online and spooled versions), created a single function
				// MarkDeleted() that handles the deletion-flag update. It returns zero if all went OK, and
				// anything else indicates a database error.
				if (v_ActionType == SALE_DELETION)
				{
					if (MarkDeleted ("5005  ", v_DeletedPrID, 0, v_RecipeIdentifier) != 0)
					{
						SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR);
						break;
					}
				}


				// DonR 24Jul2013: Unless we hit a new error (like a database failure), we don't want to overwrite
				// a previously-stored error code in the prescriptions table; so use the previous value as our default.
				if (v_ErrorCode != 0)
					prev_pr_errorcode = v_ErrorCode;

				// Update PRESCRIPTIONS table.
				// DonR Feb2023 User Story #232220: Add paid_for to columns UPDATEd.
				ExecSQL (	MAIN_DB, TR5005_UPD_prescriptions,
							&v_InvoiceNum,				&v_UserIdentification,
							&v_PharmRecipeNum,			&v_ElectPrStatus,
							&v_ClientLocationCode,		&v_DocLocationCode,
							&v_MonthLog,				&v_ActionType,
							&NOT_REPORTED_TO_AS400,		&v_CreditYesNo,
							&v_Date,					&v_Hour,
							&prev_pr_errorcode,			&IntZero,
							&v_RetnReasonCode,			&v_CreditLinePmts,
							&v_PaymentType,				&v_TotalTikraDisc,
							&v_TotalCouponDisc,			&v_ElectPrStatus,
							&v_ReasonToDispense,		&v_PharmPtnCode,
							&v_5003CommError,			&paid_for,

							&v_RecipeIdentifier,		END_OF_ARG_LIST				);

				// DonR 22May2013: Also mark prescription_msgs and prescription_usage rows as delivered. Since the
				// delivery isn't complete unless we write to the database without any problems, we can piggy-back
				// on the same error handling. (Note that "no rows affected" doesn't throw an error in
				// SQLCODE/SQLERR_error_test.)
				if (SQLCODE == 0)
				{
					ExecSQL (	MAIN_DB, UPD_prescription_msgs_mark_delivered,
								&v_Date,					&v_Hour,				&v_ActionType,
								&NOT_REPORTED_TO_AS400,		&v_RecipeIdentifier,	END_OF_ARG_LIST	);
				}

				if (SQLCODE == 0)
				{
					ExecSQL (	MAIN_DB, UPD_prescription_usage_mark_delivered,
								&v_Date,					&v_Hour,				&v_ActionType,
								&NOT_REPORTED_TO_AS400,		&v_RecipeIdentifier,	END_OF_ARG_LIST	);
				}

				Conflict_Test (reStart);

				if (SQLERR_error_test ())
				{
					SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR);
					break;
				}

			}
			while (0);	// Dummy loop Should run only once.
			// End of dummy loop to prevent goto.
		}
		else	// DonR 04Aug2010: Some error occurred in previous processing - tag prescriptions row
				// with error code.
		{
			if (v_RecipeIdentifier > 0)
			{
				// Update PRESCRIPTIONS table. No error handling, as this is going to be a non-delivered
				// prescription and this update is informational only.
				ExecSQL (	MAIN_DB, UPD_prescriptions_set_error_code,
							&v_ErrorCode,	&v_RecipeIdentifier,	END_OF_ARG_LIST	);
			}
		}	// Serious error occurred - only DB update is to flag prescriptions row with error code.


		// Commit the transaction.
		if (reStart == MAC_FALS)	// No retry needed.
		{
			if (!SetErrorVar (&v_ErrorCode, v_ErrorCode)) // Transaction OK
			{
				CommitAllWork ();
			}
			else
			{
				// Error, so do rollback.
				RollbackAllWork ();
			}

			if (SQLERR_error_test ())	// Commit (or rollback) failed.
			{
				SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR);
			}
		}
		else
		{
			// Need to retry.
			if (TransactionRestart () != MAC_OK)
			{
				v_ErrorCode = ERR_DATABASE_ERROR;
				break;
			}

			// DonR 26Jul2011: Suppress log message until the fourth attempt fails.
			if (tries > 2)
				GerrLogMini (GerrId, "Table is locked for the <%d> time", (tries + 1));

			sleep (ACCESS_CONFLICT_SLEEP_TIME);
		}


	}	// End of retries loop.


	if (reStart == MAC_TRUE)
	{
		GerrLogReturn (GerrId, "Locked for <%d> times",SQL_UPDATE_RETRIES);

		SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR);
	}

	// Don't send warnings - only fatal errors.
	if (v_ErrorCode != NO_ERROR)
	{
		Sever = GET_ERROR_SEVERITY (v_ErrorCode);

		if (Sever <= FATAL_ERROR_LIMIT)
		{
			v_ErrorCode = NO_ERROR;
		}
	}


	// Create Response Message
	nOut =  sprintf (OBuffer,		 "%0*d",	MSG_ID_LEN,					5006				);
	nOut += sprintf (OBuffer + nOut, "%0*d",	MSG_ERROR_CODE_LEN,			MAC_OK				);
	nOut += sprintf (OBuffer + nOut, "%0*d",	TPharmNum_len,				v_PharmNum			);
	nOut += sprintf (OBuffer + nOut, "%0*d",	TInstituteCode_len,			v_InstituteCode		);
	nOut += sprintf (OBuffer + nOut, "%0*d",	TTerminalNum_len,			v_TerminalNum		);
	nOut += sprintf (OBuffer + nOut, "%0*d",	TErrorCode_len,				v_ErrorCode			);
	nOut += sprintf (OBuffer + nOut, "%0*d",	TMemberIdentification_len,	v_MemberID			);
	nOut += sprintf (OBuffer + nOut, "%0*d",	TIdentificationCode_len,	v_IDCode			);
	nOut += sprintf (OBuffer + nOut, "%0*d",	TRecipeIdentifier_len,		v_RecipeIdentifier	);
	nOut += sprintf (OBuffer + nOut, "%0*d",	TDate_len,					v_Date				);
	nOut += sprintf (OBuffer + nOut, "%0*d",	THour_len,					v_Hour				);


	// Return the size in Bytes of respond message
	*p_outputWritten			= nOut;
	*output_type_flg			= ANSWER_IN_BUFFER;
	ssmd_data_ptr->error_code	= v_ErrorCode;
	ssmd_data_ptr->terminal_num	= v_TerminalNum;

	RESTORE_ISOLATION;

	return RC_SUCCESS;
}
// End of real-time 5005 handler.



/*=======================================================================
||																		||
||			 HandlerToSpool_5005										||
||	Message handler for message 5005:									||
||        Drugs Delivery "Nihul Tikrot" version - spooled mode.			||
||																		||
 =======================================================================*/

int HandlerToSpool_5005 (char			*IBuffer,
						 int			TotalInputLen,
						 char			*OBuffer,
						 int			*p_outputWritten,
						 int			*output_type_flg,
						 SSMD_DATA		*ssmd_data_ptr,
						 short int		commfail_flag)

{
	// Local declarations
	ISOLATION_VAR;
	int				nOut;
	int				tries;
	int				retries;
	int				i;
	int				j;
	int				is;
	int				reStart;
	int				ElyCount;
	int				flg;
	int				UnitsSellable;
	int				UnitsSold;
	int				AddNewRows				= 0;
	int				ComparePharmPtn;
	short			AmountMismatch			= 0;
	TErrorCode		err;
	TErrorSevirity	Sever;
	ACTION_TYPE		AS400_Gadget_Action;
	char			filler [256];


	// Message fields variables
	Msg1005Drugs				phDrgs					[MAX_REC_ELECTRONIC];
	short int					LineNumber				[MAX_REC_ELECTRONIC];
	short int					PR_WhyNonPreferredSold	[MAX_REC_ELECTRONIC];
	short int					PR_NumPerDose			[MAX_REC_ELECTRONIC];
	short int					PR_DosesPerDay			[MAX_REC_ELECTRONIC];
	short int					PR_HowParticDetermined	[MAX_REC_ELECTRONIC];
	short int					v_in_gadget_table		[MAX_REC_ELECTRONIC];
	short int					PRW_Op_Update			[MAX_REC_ELECTRONIC];
	int							PR_Date					[MAX_REC_ELECTRONIC];
	int							PR_Orig_Largo_Code		[MAX_REC_ELECTRONIC];
	int							PrevUnsoldUnits			[MAX_REC_ELECTRONIC];
	int							PrevUnsoldOP			[MAX_REC_ELECTRONIC];
	int							FixedParticipPrice		[MAX_REC_ELECTRONIC];
	short						v_MemberDiscPC			[MAX_REC_ELECTRONIC];
	TDoctorRecipeNum			v_DoctorRecipeNum		[MAX_REC_ELECTRONIC];
	TMemberParticipatingType	v_MemberParticType		[MAX_REC_ELECTRONIC];

	// DonR 13Feb2006: Ingredients-sold arrays.
	short int					ingr_code				[MAX_REC_ELECTRONIC] [3];
	double						ingr_quant				[MAX_REC_ELECTRONIC] [3];
	double						ingr_quant_std			[MAX_REC_ELECTRONIC] [3];
	double						ingr_quant_bot			[MAX_REC_ELECTRONIC] [3];
	double						ingr_quant_bot_std		[MAX_REC_ELECTRONIC] [3];
	double						per_quant				[MAX_REC_ELECTRONIC] [3];
	double						package_volume			[MAX_REC_ELECTRONIC];

	// New stuff for "Nihul Tikrot"
	short						v_ActionType;
	short						v_RetnReasonCode;
	short						v_5003CommError;
	short						PreviousErrorCode	= 0;
	TGenericTZ					v_CardOwnerID;
	TIdentificationCode			v_CardOwnerIDCode;
	short						v_CreditLinePmts;
	short						v_PaymentType;
	int							v_TotalTikraDisc;
	int							v_TotalCouponDisc;
	short						NumOtherSales;
	int							v_DrugDiscountAmt;
	short						v_DrugDiscountCode;
	short						v_PharmPtnCode;	// DonR 31Jan2011
	int							DrugDiscountAmt			[MAX_REC_ELECTRONIC];
	short						DrugDiscountCode		[MAX_REC_ELECTRONIC];
	short						InsuranceUsed			[MAX_REC_ELECTRONIC];
	char						DrugTikraType			[MAX_REC_ELECTRONIC];
	char						DrugSubsidized			[MAX_REC_ELECTRONIC];
	short						InHealthBasket			[MAX_REC_ELECTRONIC];
	int							OtherSaleID				[MAX_REC_ELECTRONIC];
	short						OtherSaleCredLine		[MAX_REC_ELECTRONIC];

	// Pharmacy Daily Sum.
	int							PDS_sum;
	int							PDS_prt_sum;
	int							PDS_lines;
	int							PDS_count;
	int							PDS_purch_sum;
	int							PDS_del_sum;
	int							PDS_del_prt_sum;
	int							PDS_del_lines;
	int							PDS_del_count;
	int							PDS_del_purch_sum;
	int							PW_ID;
	int							PW_AuthDate;
	short int					PRW_OpUpdate;
	short						PRW_UnitsUpdate;
	short int					PW_status;
	short int					PW_SubstPermitted;
	int							PD_discount_pc;
	int							PDS_date;

	int							v_ReasonForDiscount;
	int							v_ClientLocationCode;
	int							v_DocLocationCode;
	short int					v_ReasonToDispense;
	short int					LineNum;
		
	TFlg						same_pr_count;
	TFlg						report_as400_flag;
	TFlg						delete_flag;

	TDrugListRow				DL;
	PHARMACY_INFO				Phrm_info;
	TPharmNum					v_PharmNum;
	TPriceListNum				v_PriceListCode;
	TInstituteCode				v_InstituteCode;
	TTerminalNum				v_TerminalNum;
	TMemberBelongCode			v_MemberBelongCode;
	TGenericTZ					v_MemberID;
	TIdentificationCode			v_IDCode;
	TMoveCard					v_MoveCard;
	short						v_DocIDType;
	int							v_DoctorID;
	short						v_ElectPrStatus;
	TRecipeSource				v_RecipeSource;
	TInvoiceNum					v_InvoiceNum;
	TGenericTZ					v_UserIdentification;
	TPharmRecipeNum				v_PharmRecipeNum;
	TRecipeIdentifier			v_RecipeIdentifier;
	TRecipeIdentifier			v_RecipeIdentifier1;
	TDeliverConfirmationDate	v_DeliverConfirmationDate;
	TDeliverConfirmationHour	v_DeliverConfirmationHour;
	TMonthLog					v_MonthLog;
	TNumOfDrugLinesRecs			v_NumOfDrugLinesRecs;
	TCreditYesNo				v_CreditYesNo;
	TCreditYesNo				v_MemberCreditFlag;
	short						paid_for	= 1;	// DonR 05Feb2023 User Story #232220 - always TRUE for 2005/5005.

	// DonR 29Jul2007: Add logic to decode temporary cards.
	TGenericTZ			      	v_TempMembIdentification;
	TIdentificationCode			v_TempIdentifCode;
	TDate						v_TempMemValidUntil;

	// DonR 13Feb2006: Ingredients-sold fields.
	double						v_package_volume;
	short int					v_ingr_1_code;
	short int					v_ingr_2_code;
	short int					v_ingr_3_code;
	double						v_ingr_1_quant;
	double						v_ingr_2_quant;
	double						v_ingr_3_quant;
	double						v_ingr_1_quant_std;
	double						v_ingr_2_quant_std;
	double						v_ingr_3_quant_std;
	double						v_per_1_quant;
	double						v_per_2_quant;
	double						v_per_3_quant;

	TNumofSpecialConfirmation		v_SpecialConfNum;
	TSpecialConfirmationNumSource	v_SpecialConfSource;

	// Prescription Lines structure
	Msg1005Drugs				*phDrgPtr;
	Msg1005Drugs				dbDrgs;

	// Gadgets/Gadget_spool tables.
	short int					v_action;
	int							v_service_code;
	int							v_gadget_code;
	short int					v_service_number;
	short int					v_service_type;
	TDrugCode					v_DrugCode;
	TOpDrugPrice				Unit_Price;
	TDoctorRecipeNum			v_DoctorPrID;
	int							v_DoctorPrDate;
	short int					PRW_OriginCode;
	TErrorCode					GadgetError;
	TRecipeIdentifier			GadgetRecipeID;


	// Return-message variables
	TDate						SysDate;
	TDate						v_Date;
	THour						v_Hour;
	TErrorCode					v_ErrorCode;


	// Variables for deletion/refund of previous sales.
	int							v_DeletedPrID;
	int							v_DeletedPrDate;
	int							v_DeletedPrPharm;
	int							v_DeletedPrTikAmt;
	int							v_DeletedPrSubAmt;
	short						v_DeletedPrYYMM;
	short						v_DeletedPrDelFlg;
	short						v_DeletedPrSource;		// DonR 01Mar2012.
	short						v_DeletedDocIDType;		// DonR 01Mar2012.
	int							v_DeletedDoctorID;		// DonR 01Mar2012.
	int							v_DeletedClLocCode;		// DonR 01Mar2012.
	int							v_DeletedPrPhID;
	short						v_DeletedPrPrcCode;
	int							v_DeletedPrOP;
	int							v_DeletedPrUnits;
	int							v_DeletedPrPtn;
	short						v_DeletedPrPrtMeth;
	short						v_DeletedPrBasket;
	int							v_DeletedPrMemPrc;
	int							v_DeletedPrSupPrc;
	int							v_DeletedPrDscPcnt;
	int							v_DeletedPrFixPrc;
	int							v_DeletedPrTotPrc;
	int							v_DeletedPrDiscnt;
	int							v_DeletedPrDiscPt;
	short						v_DeletedPrDsCode;
	short						v_DeletedPrTkMazon;
	int							v_DeletedPrIshNum;
	short						v_DeletedPrIshSrc;
	short						v_DeletedPrIshTik;
	short						v_DeletedPrIshTkTp;
	short						v_DeletedPrCredit;
	int							v_DeletedPrRuleNum;
	int							v_DeletedPrDocPrID;
	int							v_DeletedPrPRW_ID;
	int							v_DeletedPrPrDate;
	int							v_DeletedPrPrLargo;
	int							DeletedPrPRW_ID			[MAX_REC_ELECTRONIC];

	// DonR 03Jul2013: Add variables set conditionally to give negative numbers
	// for deletions or returns for credit. This way we avoid "double-flipping"
	// if we recover from a database deadlock: the original variables remain
	// positive, so we won't re-flip a negative number back to positive.
	int							v_SignedTotPtn;
	int							v_SignedTotDrugPrc;
	int							v_SignedOP;
	int							v_SignedUnits;

	// Miscellaneous DB variables
	TPharmNum					sq_pharmnum;
	TInstituteCode				sq_institutecode;
	TTerminalNum				sq_terminalnum;
	TPriceListNum				sq_pricelistnum;
	TDoctorRecipeNum			sq_doctorprescid;
	TMemberBelongCode			sq_memberbelongcode;
	TGenericTZ					sq_MemberID;
	TIdentificationCode			sq_identificationcode;
	short						sq_DocIDType;
	TGenericTZ					sq_doctoridentification;
	TRecipeSource				sq_recipesource;
	TNumOfDrugLinesRecs			sq_numofdruglinesrecs;
	TErrorCode					sq_errorcode;
	TTotalMemberParticipation	sq_TotalMemberPrice;
	TTotalDrugPrice				sq_TotalDrugPrice;
	TTotalDrugPrice				sq_TotSuppPrice;
	TMemberDiscountPercent		sq_member_discount;
	int							sq_tikra_discount;
	int							sq_subsidy_amount;
	int							sq_member_ptn_amt;
	int							sq_PW_ID;
	int							sq_DrPrescNum;
	TDate						StopUseDate;
	TDate						StopBloodDate;
	TDate						max_drug_date;
	TDate						sq_maxdrugdate;
	short int					sq_thalf;
	TDate						sq_DateDB;
	TMonthLog					sq_MonthLogDB;
	short						v_MemberSpecPresc;
	TErrorCode					v_Comm_Error;
	TSqlInd						v_CredInd;
	TRecipeIdentifier			v_prw_id;
	long						Dummy_online_order_num;	// So we can use the same SQL command as Trn. 6005.
	short						OriginCode	= 5005;


	// Body of function.

	// Initialize variables.
	REMEMBER_ISOLATION;
	SET_ISOLATION_DIRTY;
	PosInBuff		= IBuffer;
	v_ErrorCode		= NO_ERROR;
	SysDate			= GetDate ();
	v_Comm_Error	= commfail_flag;

	memset ((char *)&phDrgs [0],	0, sizeof (phDrgs));
	memset ((char *)&dbDrgs,		0, sizeof (dbDrgs));



	// Read message fields data into variables.
	v_PharmNum					= GetLong	(&PosInBuff, TPharmNum_len					); CHECK_ERROR_T("5005-Spool: PharmNum");
	v_InstituteCode				= GetShort	(&PosInBuff, TInstituteCode_len				); CHECK_ERROR_T("5005-Spool: InstituteCode");
	v_TerminalNum				= GetShort	(&PosInBuff, TTerminalNum_len				); CHECK_ERROR_T("5005-Spool: TerminalNum");
	v_ActionType				= GetShort	(&PosInBuff, 2								); CHECK_ERROR_T("5005-Spool: ActionType");
	v_RetnReasonCode			= GetShort	(&PosInBuff, 2								); CHECK_ERROR_T("5005-Spool: RetnReasonCode");
	v_5003CommError				= GetShort	(&PosInBuff, 4								); CHECK_ERROR_T("5005-Spool: 5003CommError");
	v_DeletedPrID				= GetLong	(&PosInBuff, TRecipeIdentifier_len			); CHECK_ERROR_T("5005-Spool: DeletedPrID");
	v_DeletedPrPharm			= GetLong	(&PosInBuff, TPharmNum_len					); CHECK_ERROR_T("5005-Spool: DeletedPrPharm");
	v_DeletedPrYYMM				= GetShort	(&PosInBuff, 4								); CHECK_ERROR_T("5005-Spool: DeletedPrYYMM");
	v_DeletedPrPhID				= GetLong	(&PosInBuff, TPharmRecipeNum_len			); CHECK_ERROR_T("5005-Spool: DeletedPrPhID");
	v_DeletedPrDate				= GetLong	(&PosInBuff, 8								); CHECK_ERROR_T("5005-Spool: DeletedPrDate");
	v_MemberID					= GetLong	(&PosInBuff, TMemberIdentification_len		); CHECK_ERROR_T("5005-Spool: MemberID");
	v_IDCode					= GetShort	(&PosInBuff, TIdentificationCode_len		); CHECK_ERROR_T("5005-Spool: IDCode");
	v_MemberBelongCode			= GetShort	(&PosInBuff, TMemberBelongCode_len			); CHECK_ERROR_T("5005-Spool: MemberBelongCode");	 
	v_CardOwnerID				= GetLong	(&PosInBuff, TMemberIdentification_len		); CHECK_ERROR_T("5005-Spool: CardOwnerID");
	v_CardOwnerIDCode			= GetShort	(&PosInBuff, TIdentificationCode_len		); CHECK_ERROR_T("5005-Spool: CardOwnerIDCode");
	v_MoveCard					= GetShort	(&PosInBuff, TMoveCard_len					); CHECK_ERROR_T("5005-Spool: MoveCard");	 
	v_DocIDType					= GetShort	(&PosInBuff, TTypeDoctorIdentification_len	); CHECK_ERROR_T("5005-Spool: DocIDType");	 
	v_DoctorID					= GetLong	(&PosInBuff, TDoctorIdentification_len		); CHECK_ERROR_T("5005-Spool: DoctorID");	 
	v_ElectPrStatus				= GetShort	(&PosInBuff, 1								); CHECK_ERROR_T("5005-Spool: DoctorIdInputType");	 
	v_RecipeSource			  	= GetShort	(&PosInBuff, TRecipeSource_len				); CHECK_ERROR_T("5005-Spool: RecipeSource");	 
	v_UserIdentification		= GetLong	(&PosInBuff, TUserIdentification_len		); CHECK_ERROR_T("5005-Spool: UserIdentification");	 
	v_SpecialConfSource			= GetShort	(&PosInBuff, TSpecConfNumSrc_len			); CHECK_ERROR_T("5005-Spool: SpecialConfSource");
	v_SpecialConfNum			= GetLong	(&PosInBuff, TNumofSpecialConfirmation_len	); CHECK_ERROR_T("5005-Spool: SpecialConfNum");
	v_PharmRecipeNum			= GetLong	(&PosInBuff, TPharmRecipeNum_len			); CHECK_ERROR_T("5005-Spool: PharmRecipeNum");
	v_RecipeIdentifier			= GetLong	(&PosInBuff, TRecipeIdentifier_len			); CHECK_ERROR_T("5005-Spool: RecipeIdentifier");	 
	v_DeliverConfirmationDate	= GetLong	(&PosInBuff, TDeliverConfirmationDate_len	); CHECK_ERROR_T("5005-Spool: DeliverConfirmationDate");
	v_DeliverConfirmationHour	= GetLong	(&PosInBuff, TDeliverConfirmationHour_len	); CHECK_ERROR_T("5005-Spool: DeliverConfirmationHour");	 

	// Note: This will be the Credit Type Used. In some circumstances, it may be
	// different from the Credit Type that was sent to the pharmacy - for example,
	// the member may elect to pay in cash even though s/he has an automated
	// payment ("orat keva") set up. This value will be stored in the new database
	// field credit_type_used.
	v_CreditYesNo				= GetShort	(&PosInBuff, TCreditYesNo_len				); CHECK_ERROR_T("5005-Spool: CreditYesNo");
	v_CreditLinePmts			= GetShort	(&PosInBuff, 2								); CHECK_ERROR_T("5005-Spool: CreditLinePmts");
	v_PaymentType				= GetShort	(&PosInBuff, 2								); CHECK_ERROR_T("5005-Spool: PaymentType");
	v_InvoiceNum			 	= GetLong	(&PosInBuff, TInvoiceNum_len				); CHECK_ERROR_T("5005-Spool: InvoiceNum");	 
	v_ClientLocationCode		= GetLong	(&PosInBuff, TDeviceCode_len				); CHECK_ERROR_T("5005-Spool: ClientDeviceCode");	 
	v_DocLocationCode			= GetLong	(&PosInBuff, TDeviceCode_len				); CHECK_ERROR_T("5005-Spool: DocDeviceCode");	 
	v_MonthLog					= GetShort	(&PosInBuff, TMonthLog_len					); CHECK_ERROR_T("5005-Spool: MonthLog");
	v_ReasonToDispense			= GetShort	(&PosInBuff, TElectPR_ReasonToDispLen		); CHECK_ERROR_T("5005-Spool: ReasonToDispense");
	v_TotalTikraDisc			= GetLong	(&PosInBuff, 9								); CHECK_ERROR_T("5005-Spool: TotalTikraDisc");
	v_TotalCouponDisc			= GetLong	(&PosInBuff, 9								); CHECK_ERROR_T("5005-Spool: TotalCouponDisc");
    v_PharmPtnCode				= GetShort	(&PosInBuff, 4								); CHECK_ERROR_T("5005-Spool: PharmPtnCode");	// DonR 31Jan2011.

	// 16 characters reserved for future use.
	GetString								(&PosInBuff, filler, 16						); CHECK_ERROR_T("5005-Spool: HeaderFiller");

	v_NumOfDrugLinesRecs		= GetShort	(&PosInBuff, TElectPR_LineNumLen			); CHECK_ERROR_T("5005-Spool: NumOfDrugLinesRecs");


	// Get repeating portion of message
	// DonR 28Feb2011: If this is a sale deletion with 99 as the number of drug lines, we don't want
	// to read drug-line data from the incoming message; it's a request for full cancellation of the
	// sale without specifying what drugs were sold (perhaps because the pharmacy doesn't know).
	if ((v_ActionType != SALE_DELETION) || (v_NumOfDrugLinesRecs != 99))
	{
		for (i = 0; i < v_NumOfDrugLinesRecs; i++)
		{
			LineNumber			[i]		= GetShort	(&PosInBuff, TElectPR_LineNumLen			); CHECK_ERROR_T("5005-Spool: LineNumber");
			v_DoctorRecipeNum	[i]		= GetLong	(&PosInBuff, TDoctorRecipeNum_len			); CHECK_ERROR_T("5005-Spool: DoctorRecipeNum");	 
			PR_Date				[i]		= GetLong	(&PosInBuff, TDate_len						); CHECK_ERROR_T("5005-Spool: PR_Date");

			// DonR 02Mar2016: For now, Largo Codes are only five digits even though the message space for them
			// has expanded. Since six-digit Largo Codes blow up transmission of tables to AS/400 (and we've seen
			// this happen only for Doctor Largo Code), catch this problem right at the outset and reject the
			// transaction.
			// DonR 15Jul2024 User Story #349368: Largo Codes are being expanded to 6 digits on AS/400.
			// Accordingly, adjust the "sanity check" to allow one more digit.
			PR_Orig_Largo_Code	[i]		= GetLong	(&PosInBuff, 9								); CHECK_ERROR_T("5005-Spool: Orig_Largo_Code");
			if (PR_Orig_Largo_Code [i] > 999999)
				SET_ERROR (ERR_WRONG_FORMAT_FILE);

			phDrgs[i].DrugCode			= GetLong	(&PosInBuff, 9								); CHECK_ERROR_T("5005-Spool: DrugCode");
			PR_WhyNonPreferredSold[i]	= GetShort	(&PosInBuff, TElectPR_SubstPermLen			); CHECK_ERROR_T("5005-Spool: WhyNonPreferredSold");
			PrevUnsoldUnits		[i]		= GetLong	(&PosInBuff, TUnits_len						); CHECK_ERROR_T("5005-Spool: PrevUnsoldUnits");
			PrevUnsoldOP		[i]		= GetLong	(&PosInBuff, TUnits_len						); CHECK_ERROR_T("5005-Spool: PrevUnsoldOP");
			phDrgs[i].Units				= GetLong	(&PosInBuff, TUnits_len						); CHECK_ERROR_T("5005-Spool: Units");
			phDrgs[i].Op				= GetLong	(&PosInBuff, TOp_len						); CHECK_ERROR_T("5005-Spool: Op");
			PR_NumPerDose		[i]		= GetShort	(&PosInBuff, TUnits_len						); CHECK_ERROR_T("5005-Spool: NumPerDose");
			PR_DosesPerDay		[i]		= GetShort	(&PosInBuff, TUnits_len						); CHECK_ERROR_T("5005-Spool: DosesPerDay");
			phDrgs[i].Duration			= GetShort	(&PosInBuff, TDuration_len					); CHECK_ERROR_T("5005-Spool: Duration");
			phDrgs[i].LinkDrugToAddition= GetLong	(&PosInBuff, 9								); CHECK_ERROR_T("5005-Spool: LinkDrugToAddition");
			phDrgs[i].OpDrugPrice		= GetLong	(&PosInBuff, TOpDrugPrice_len				); CHECK_ERROR_T("5005-Spool: OpDrugPrice");
			phDrgs[i].SupplierDrugPrice	= GetLong	(&PosInBuff, TSupplierPrice_len				); CHECK_ERROR_T("2005-Spool: SupplierDrugPrice");
			v_MemberParticType	[i]		= GetShort	(&PosInBuff, TMemberParticipatingType_len	); CHECK_ERROR_T("5005-Spool: MemberParticType");	 
			FixedParticipPrice	[i]		= GetLong	(&PosInBuff, TOpDrugPrice_len				); CHECK_ERROR_T("5005-Spool: FixedParticipPrice");
			v_MemberDiscPC		[i]		= GetShort	(&PosInBuff, TMemberDiscountPercent_len		); CHECK_ERROR_T("5005-Spool: MemberDiscPC");	 
			phDrgs[i].TotalDrugPrice	= GetLong	(&PosInBuff, TTotalDrugPrice_len			); CHECK_ERROR_T("5005-Spool: TotalDrugPrice");

			phDrgs[i].TotalMemberParticipation
										= GetLong	(&PosInBuff, TTotalMemberParticipation_len	); CHECK_ERROR_T("5005-Spool: TotalMemberParticipation");

			DrugDiscountAmt		[i]		= GetLong	(&PosInBuff, 9								); CHECK_ERROR_T("5005-Spool: DrugDiscountAmt");

			// Normally, this can be ignored - since it's written to the DB by Trn. 2003.
			PR_HowParticDetermined [i]	= GetShort	(&PosInBuff, TElectPR_HowPartDetLen			); CHECK_ERROR_T("5005-Spool: HowParticDetermined");

			phDrgs[i].CurrentStockOp	= GetLong	(&PosInBuff, TElectPR_Stock_len				); CHECK_ERROR_T("5005-Spool: CurrentStockOp");

			phDrgs[i].CurrentStockInUnits
										= GetLong	(&PosInBuff, TElectPR_Stock_len				); CHECK_ERROR_T("5005-Spool: CurrentStockInUnits");

			DrugDiscountCode	[i]		= GetShort	(&PosInBuff, 3								); CHECK_ERROR_T("5005-Spool: DrugDiscountCode");
			InsuranceUsed		[i]		= GetShort	(&PosInBuff, 2								); CHECK_ERROR_T("5005-Spool: InsuranceUsed");
			DrugTikraType		[i]		= GetChar	(&PosInBuff									); CHECK_ERROR_T("5005-Spool: DrugTikraType");
			DrugSubsidized		[i]		= GetChar	(&PosInBuff									); CHECK_ERROR_T("5005-Spool: DrugSubsidized");
			InHealthBasket		[i]		= GetShort	(&PosInBuff, 1								); CHECK_ERROR_T("5005-Spool: InHealthBasket");

			// 15 characters reserved for future use.
			GetString								(&PosInBuff, filler, 15						); CHECK_ERROR_T("5005-Spool: DrugLineFiller");

			// DonR 12Jul2005: Avoid DB errors due to ridiculously high OP numbers from pharmacy.
			PRW_Op_Update[i] = ((phDrgs[i].Op <= 32767) && (phDrgs[i].Op >= 0)) ? phDrgs[i].Op : 32767;

			// DonR 03Mar2011: Copy AS/400 Ishur number/source from header to array.
			phDrgs[i].SpecPrescNum			= v_SpecialConfNum;
			phDrgs[i].SpecPrescNumSource	= v_SpecialConfSource;
		}	// End of drug-line reading loop.
	}	// NOT a deletion with 99 as number-of-drugs.


	// Read in additional drug sales being charged to the same Credit Line.
	// NIU (i.e. the number of lines should always be zero) as of 24May2010.
	NumOtherSales = GetShort (&PosInBuff, TElectPR_LineNumLen); CHECK_ERROR_T("5005-Spool: NumOtherSales");

	for (i = 0; i < NumOtherSales; i++)
	{
		LineNumber			[i]		= GetShort	(&PosInBuff, TElectPR_LineNumLen			); CHECK_ERROR_T("5005-Spool: OtherSaleNum");
		OtherSaleID			[i]		= GetLong	(&PosInBuff, TRecipeIdentifier_len			); CHECK_ERROR_T("5005-Spool: OtherSaleID");
		OtherSaleCredLine	[i]		= GetShort	(&PosInBuff, 1								); CHECK_ERROR_T("5005-Spool: OtherSaleCredLine");
		// 10 characters reserved for future use.
		GetString								(&PosInBuff, filler, 10						); CHECK_ERROR_T("5005-Spool: OtherSaleFiller");
	}


	// Get amount of input not eaten
	ChckInputLen( TotalInputLen - (PosInBuff - IBuffer) );
	CHECK_ERROR_T("5005-Spool: ChckInputLen");

	// Copy values to ssmd_data structure.
	ssmd_data_ptr->pharmacy_num		= v_PharmNum;
	ssmd_data_ptr->member_id		= v_MemberID;
	ssmd_data_ptr->member_id_ext	= v_IDCode;

	// Validate Date and Time.
	ChckDate (v_DeliverConfirmationDate);
	CHECK_ERROR_T("5005-Spool: Check Delivery Date");	 

	ChckTime (v_DeliverConfirmationHour);
	CHECK_ERROR_T("5005-Spool: Check Delivery Time");	 

	// DonR 24Aug2020: In 5005-spool, pharmacies sometimes send zero for the Sale Time. This
	// doesn't cause problems on Linux, but apparently some AS/400 queries don't work if
	// Sale Time is zero. So if we got a zero, add one second.
	if (v_DeliverConfirmationHour == 0)
		v_DeliverConfirmationHour = 1;	// = 00:00:01

	reStart		= MAC_TRUE;


	// SQL Retry Loop.
	for (tries = 0; tries < SQL_UPDATE_RETRIES && reStart == MAC_TRUE; tries++)
	{
		// Initialize response variables.
		v_ErrorCode					= NO_ERROR;
		reStart						= MAC_FALS;
		SysDate						= GetDate ();
		v_Date						= GetDate ();
		v_Hour						= GetTime ();
		max_drug_date				= 0;
		v_MemberSpecPresc			= 0;

		memset ((char *)&dbDrgs, 0, sizeof (dbDrgs));


		// Get prescription id.
		state = GET_PRESCRIPTION_ID (&v_RecipeIdentifier1);

		if (state != NO_ERROR)
		{
			SetErrorVar( &v_ErrorCode, state );

			GerrLogReturn (GerrId,
						   "Can't get PRESCRIPTION_ID for 5005 spool - error %d",
						   state);
		}


		// Dummy loop to avoid GOTO.
		do		// Exiting from loop will send the reply to pharmacy.
		{
			// Load pharmacy data.
			v_ErrorCode = IS_PHARMACY_OPEN (v_PharmNum, v_InstituteCode, &Phrm_info);

			v_PriceListCode = (v_ErrorCode == MAC_OK) ? Phrm_info.price_list_num : 0;


			// Test temporary members.
		    if (v_IDCode == 7)
		    {
				ExecSQL (	MAIN_DB, READ_tempmemb,
							&v_TempIdentifCode,			&v_TempMembIdentification,	&v_TempMemValidUntil,
							&v_MemberID,				END_OF_ARG_LIST										);

				Conflict_Test (reStart);

				if (SQLERR_code_cmp (SQLERR_not_found) == MAC_TRUE)
				{
					// Since this is the spool processor, don't do anything - just
					// continue working.
				}

				else
				if (SQLERR_error_test ())
				{
					v_ErrorCode = ERR_DATABASE_ERROR;
				}

				else	// Use the values from tempmemb only if row was
						// actually read from the database.
						// DonR 20Oct2013: If the current action is a sale deletion, an expired
						// temporary card is still usable.
				if ((v_TempMemValidUntil >= SysDate) || (v_ActionType == SALE_DELETION))
				{
				    v_IDCode	= v_TempIdentifCode;
				    v_MemberID	= v_TempMembIdentification;
				}
			}	// Temporary Member test.

			
			
			// Test Maccabi members.
			if (v_MemberBelongCode == MACABI_INSTITUTE)
			{
				if (v_MemberID == 0)
				{
					v_ErrorCode				= ERR_MEMBER_ID_CODE_WRONG;

					v_MemberCreditFlag		= 0;
					Member.maccabi_code		= 0;
					Member.maccabi_until	= 0;
					v_MemberSpecPresc		= 0;
				}
				else
				{
					ExecSQL (	MAIN_DB, TR2005_5005spool_READ_members,
								&v_MemberCreditFlag,		&Member.maccabi_code,
								&Member.maccabi_until,		&v_MemberSpecPresc,
								&sq_maxdrugdate,			&Member.insurance_type,
								&Member.keren_mac_code,		&Member.mac_magen_code,
								&Member.yahalom_code,		&Member.keren_mac_from,
								&Member.keren_mac_until,	&Member.mac_magen_from,
								&Member.mac_magen_until,	&Member.yahalom_from,
								&Member.yahalom_until,		&Member.died_in_hospital,
								&v_MemberID,				&v_IDCode,
								END_OF_ARG_LIST										);

					Conflict_Test (reStart);

					// DonR 04Dec2022 User Story #408077: Use a new function to set some Member flags and values.
					SetMemberFlags (SQLCODE);

					if (SQLMD_is_null (v_CredInd))
						v_MemberCreditFlag = 0;


					// DonR 20Nov2007: If member is not found, continue - with spooled
					// sales, we can't just throw them away!
					if (SQLERR_code_cmp (SQLERR_not_found) == MAC_TRUE)
					{
						v_MemberCreditFlag		= 0;
						Member.maccabi_code		= 0;
						Member.maccabi_until	= 0;
						v_MemberSpecPresc		= 0;	// DonR 20Nov2007: Added another default.
					}
					else
					{
						// See if there were problems other than deleted-row or not-found.
						if (SQLERR_error_test ())
						{
							v_ErrorCode = ERR_DATABASE_ERROR;
							break;
						}

						// DonR 20Nov2007: Check for member rights only if we actually read
						// a row from the Members table.
						else
						{
							// Check member eligibility.
//							// DonR 25Nov2021 User Story #206812: Get died-in-hospital indicator from the Ishpuz system.
							// DonR 30Nov2022 User Story #408077: Use new macro MEMBER_INELIGIBLE (defined in
							// MsgHndlr.h) to decide if this member is eligible.
//							if ((Member.maccabi_code < 1) || (Member.maccabi_until < SysDate) || (Member.died_in_hospital))
							if (MEMBER_INELIGIBLE)
							{
								// DonR 29Jul2007: If we've failed to read the Member information,
								// don't overwrite the "bad member ID" error.
								if (v_ErrorCode != ERR_MEMBER_ID_CODE_WRONG)
									v_ErrorCode = ERR_MEMBER_NOT_ELEGEBLE;
							}
						}	// Good read of member.
					}	// We didn't get a member-not-found error.
				}	// Member Identification is non-zero.
			}	// Member belongs to Maccabi.


			// DonR 20Nov2007: For spooled sales to non-Maccabi members, provide
			// defaults for the values that would normally come from the Members
			// table.
			else
			{
				v_MemberCreditFlag		= 0;
				Member.maccabi_code		= 0;
				Member.maccabi_until	= 0;
				v_MemberSpecPresc		= 0;
			}


			// Special processing for deletions.
			if (v_ActionType == SALE_DELETION)
			{
				// DonR 07Feb2011: If pharmacy didn't supply a Presciption ID to delete, try to identify
				// the original sale from the other parameters supplied.
				// DonR 30Jun2016: Added del_flg to WHERE clause, since there may be duplicate rows when
				// sales have arrived via spool.
				if (v_DeletedPrID == 0)
				{
					ExecSQL (	MAIN_DB, READ_Find_Prescription_ID_to_delete,
								&v_DeletedPrID,
								&v_MemberID,				&v_DeletedPrPharm,
								&v_DeletedPrYYMM,			&v_DeletedPrPhID,
								&DRUG_DELIVERED,			&v_IDCode,
								&v_MemberBelongCode,		END_OF_ARG_LIST				);

					// DonR 31Mar2011: If the prescription wasn't found, the SQL above doesn't necessarily
					// return an error - so if the MIN(prescription_id) value is in fact bogus, force an
					// SQL error by actually trying to read the prescriptions row.
					if (SQLCODE == 0)
					{
						bool	DeletedPrFoundInDatabase = false;

						ExecSQL (	MAIN_DB, READ_prescriptions_validate_deleted_pr_ID,
									&DeletedPrFoundInDatabase, &v_DeletedPrID, END_OF_ARG_LIST			);

						SQLERR_error_test ();
//						ExecSQL (	MAIN_DB, READ_prescriptions_validate_deleted_pr_ID,
//									&v_DeletedPrID, &v_DeletedPrID, END_OF_ARG_LIST			);

						if (!DeletedPrFoundInDatabase)
						{
GerrLogMini (GerrId, "Trn 5005-spool: Checked v_DeletedPrID %d from DB, found = %d, SQLCODE = %d.",
	v_DeletedPrID, DeletedPrFoundInDatabase, SQLCODE);
							v_DeletedPrID = 0;
						}
					}

					if (SQLCODE != 0)
						v_DeletedPrID = 0;
				}	// Need to get ID of prescription being deleted.


				// Read in Subsidy Amount from the prescriptions row to be deleted. (Do we need more fields?)
				// DonR 31Mar2011: Yes we do - added (total) Tikra Discount.
				// DonR 06Sep2011: Read in del_flg from prescription to be deleted, so we know if it's
				// already been deleted.
				// DonR 01Mar2012: Added Prescription Source, Doc ID Type, and Doctor ID to fields copied
				// from original sale.
				// DonR 27Mar2012: Added macabi_centers_num (= Client Location Code) as well.
				ExecSQL (	MAIN_DB, TR5005spool_READ_deleted_prescriptions_row_values,
							&v_DeletedPrSubAmt,		&v_DeletedPrTikAmt,
							&v_DeletedPrSource,		&v_DeletedDocIDType,
							&v_DeletedDoctorID,		&v_DeletedClLocCode,
							&v_DeletedPrDelFlg,
							&v_DeletedPrID,			END_OF_ARG_LIST			);

				if (SQLERR_code_cmp (SQLERR_not_found) == MAC_TRUE)
				{
					// Do (almost) nothing - we're in spool mode, so there's no point in throwing unnecessary errors.
					// On the other hand, make sure that Total Tikra Discount and Total Coupon Amount are set to zero.
					// DonR 06Sep2011: Zero out Deleted PR del_flg as well.
					v_DeletedPrDelFlg = v_DeletedPrSubAmt = v_DeletedPrTikAmt = v_DeletedClLocCode = 0;
				}
				else
				{
					if (SQLERR_error_test ())
					{
						v_ErrorCode = ERR_DATABASE_ERROR;
						break;
					}
					else
					{
						// Successful read.

						// Override what pharmacy sent only if pharmacy sent zero.
						// DonR 01Mar2012: Added three additional fields to be copied if necessary.
						// DonR 27Mar2012: Added Client Location Code (= macabi_centers_num); also changed
						// condition for copying Total Coupon Discount and Total Tikra Discount, which should
						// be copied based on the pharmacy's having sent '99' for the number of drug lines
						// and *not* based on what amounts the pharmacy sent in the deletion request.
						// DonR 20Apr2015: Total Tikra Discount and Total Coupon Discount should be negative
						// (if non-zero) for deletions - so we need to "flip" these numbers when we take them
						// from the original sale row.
						if (v_NumOfDrugLinesRecs	== 99)	// DonR 27Mar2012.
						{
							v_TotalCouponDisc	= 0 - v_DeletedPrSubAmt;
							v_TotalTikraDisc	= 0 - v_DeletedPrTikAmt;
						}

						if (v_RecipeSource			== 0)
							v_RecipeSource = v_DeletedPrSource;			// DonR 01Mar2012.

						if (v_DocIDType				== 0)
							v_DocIDType = v_DeletedDocIDType;			// DonR 01Mar2012.

						if (v_DoctorID				== 0)
							v_DoctorID = v_DeletedDoctorID;				// DonR 01Mar2012.

						if (v_ClientLocationCode	== 0)
							v_ClientLocationCode = v_DeletedClLocCode;	// DonR 27Mar2012.
					}
				}	// Something other than not-found.


				// If pharmacy did not supply a list of drugs to delete, read in all the drugs from
				// the original sale. Note that if pharmacy sends zero for the number of drugs to
				// delete, we do NOT automatically delete the whole prior sale - instead, we just
				// throw an error.
				if (v_NumOfDrugLinesRecs == 99)	// Pharmacy has requested full deletion of prior sale.
				{
					v_NumOfDrugLinesRecs = 0;	// List is actually empty - so far.


					// DonR 10Apr2011: Declare and open cursor only when it's needed.
					DeclareAndOpenCursorInto (	MAIN_DB, TR5005spool_deldrugs_cur,
												&v_DrugCode,			&v_DeletedPrOP,			&v_DeletedPrUnits,
												&v_DeletedPrPtn,		&v_DeletedPrPrcCode,	&v_DeletedPrDiscPt,
												&v_DeletedPrPrtMeth,	&v_DeletedPrMemPrc,		&v_DeletedPrSupPrc,

												&v_DeletedPrDscPcnt,	&v_DeletedPrFixPrc,		&v_DeletedPrBasket,
												&v_DeletedPrDate,		&v_DeletedPrTotPrc,		&v_DeletedPrDiscnt,
												&v_DeletedPrDsCode,		&v_DeletedPrTkMazon,	&v_DeletedPrIshNum,

												&v_DeletedPrIshSrc,		&v_DeletedPrIshTik,		&v_DeletedPrIshTkTp,
												&v_DeletedPrRuleNum,	&v_DeletedPrDocPrID,	&v_DeletedPrPrDate,
												&v_DeletedPrPrLargo,	&v_DeletedPrPRW_ID,
						
												&v_DeletedPrID,			&DRUG_DELIVERED,		&ROW_NOT_DELETED,
												END_OF_ARG_LIST															);

					if (SQLERR_error_test ())
					{
						v_ErrorCode = ERR_DATABASE_ERROR;
						break;
					}

					do
					{
						FetchCursor (	MAIN_DB, TR5005spool_deldrugs_cur	);

						Conflict_Test (reStart);

						if (SQLCODE == 0)	// Good read.
						{
							// Copy original-sale values into arrays.
							// Op, Units, Total Price, and Member Participation get "flipped" negative later on.
							phDrgs[v_NumOfDrugLinesRecs].DrugCode					= v_DrugCode;
							phDrgs[v_NumOfDrugLinesRecs].Op							= v_DeletedPrOP;
							phDrgs[v_NumOfDrugLinesRecs].Units						= v_DeletedPrUnits;
							phDrgs[v_NumOfDrugLinesRecs].TotalMemberParticipation	= v_DeletedPrPtn;
							phDrgs[v_NumOfDrugLinesRecs].OpDrugPrice				= v_DeletedPrMemPrc;
							phDrgs[v_NumOfDrugLinesRecs].SupplierDrugPrice			= v_DeletedPrSupPrc;
							phDrgs[v_NumOfDrugLinesRecs].TotalDrugPrice				= v_DeletedPrTotPrc;
							phDrgs[v_NumOfDrugLinesRecs].tikrat_mazon_flag			= v_DeletedPrTkMazon;
							phDrgs[v_NumOfDrugLinesRecs].SpecPrescNum				= v_DeletedPrIshNum;
							phDrgs[v_NumOfDrugLinesRecs].SpecPrescNumSource			= v_DeletedPrIshSrc;
							phDrgs[v_NumOfDrugLinesRecs].IshurWithTikra				= v_DeletedPrIshTik;
							phDrgs[v_NumOfDrugLinesRecs].IshurTikraType				= v_DeletedPrIshTkTp;
							phDrgs[v_NumOfDrugLinesRecs].rule_number				= v_DeletedPrRuleNum;
							v_DoctorRecipeNum		[v_NumOfDrugLinesRecs]			= v_DeletedPrDocPrID;	/* DonR 31Mar2011 */	 
							v_MemberDiscPC			[v_NumOfDrugLinesRecs]			= v_DeletedPrDiscPt;
							v_MemberDiscPC			[v_NumOfDrugLinesRecs]			= v_DeletedPrDscPcnt;
							PR_Date					[v_NumOfDrugLinesRecs]			= v_DeletedPrDate;
							DrugDiscountAmt			[v_NumOfDrugLinesRecs]			= v_DeletedPrDiscnt;
							DrugDiscountCode		[v_NumOfDrugLinesRecs]			= v_DeletedPrDsCode;
							v_MemberParticType		[v_NumOfDrugLinesRecs]			= v_DeletedPrPrcCode;
							PR_HowParticDetermined	[v_NumOfDrugLinesRecs]			= v_DeletedPrPrtMeth;
							InHealthBasket			[v_NumOfDrugLinesRecs]			= v_DeletedPrBasket;
							FixedParticipPrice		[v_NumOfDrugLinesRecs]			= v_DeletedPrFixPrc;
							DeletedPrPRW_ID			[v_NumOfDrugLinesRecs]			= v_DeletedPrPRW_ID;
							PR_Date					[v_NumOfDrugLinesRecs]			= v_DeletedPrPrDate;
							PR_Orig_Largo_Code		[v_NumOfDrugLinesRecs]			= v_DeletedPrPrLargo;

							// DonR 25May2016: We need to copy the OP value into the PRW_Op_Update array -
							// otherwise deletions where the original-sale values are read from the table
							// don't properly update the sold-status of the original doctor prescription.
							PRW_Op_Update			[v_NumOfDrugLinesRecs]			= v_DeletedPrOP;

							v_NumOfDrugLinesRecs++;
						}	// Good read of drug-lines cursor.
						else
						{
							if (SQLERR_code_cmp (SQLERR_end_of_fetch) == MAC_TRUE)
							{
								break;	// No problem - we're finished reading drugs from original sale.
							}
							else
							{
								if (SQLERR_error_test ())
								{
									v_ErrorCode		= ERR_DATABASE_ERROR;
									break;
								}
							}	// Something other than end-of-fetch.
						}	// Fetch failed.
					}
					while (1);	// End of cursor loop for reading prior-sale drugs.

					CloseCursor (	MAIN_DB, TR5005spool_deldrugs_cur	);
				}	// Pharmacy sent "99" to request full deletion of prior sale.

				else
				{
					// Pharmacy sent a list of drugs to delete - but we assume that the information
					// sent doesn't include all the values from the original sale, so we'll read
					// them and copy them in.
					for (i = 0; i < v_NumOfDrugLinesRecs; i++)
					{
						v_DrugCode = phDrgs[i].DrugCode;

						ExecSQL (	MAIN_DB, TR5005spool_READ_deleted_prescription_drugs_values,
									&v_DeletedPrOP,			&v_DeletedPrUnits,		&v_DeletedPrPtn,
									&v_DeletedPrPrcCode,	&v_DeletedPrDiscPt,		&v_DeletedPrPrtMeth,
									&v_DeletedPrMemPrc,		&v_DeletedPrSupPrc,		&v_DeletedPrDscPcnt,
									&v_DeletedPrFixPrc,		&v_DeletedPrBasket,		&v_DeletedPrDate,

									&v_DeletedPrTotPrc,		&v_DeletedPrDiscnt,		&v_DeletedPrDsCode,
									&v_DeletedPrTkMazon,	&v_DeletedPrIshNum,		&v_DeletedPrIshSrc,
									&v_DeletedPrIshTik,		&v_DeletedPrIshTkTp,	&v_DeletedPrRuleNum,
									&v_DeletedPrDocPrID,	&v_DeletedPrPrDate,		&v_DeletedPrPrLargo,
									&v_DeletedPrPRW_ID,

									&v_DeletedPrID,			&DRUG_DELIVERED,		&v_DrugCode,
									END_OF_ARG_LIST																);

						Conflict_Test (reStart);

						if (SQLCODE == 0)	// Good read.
						{
							// Copy original-sale values into arrays.
							// Op, Units, Total Price, and Member Participation get "flipped" negative later on.
							phDrgs[i].Op						= v_DeletedPrOP;
							phDrgs[i].Units						= v_DeletedPrUnits;
							phDrgs[i].TotalMemberParticipation	= v_DeletedPrPtn;
							phDrgs[i].OpDrugPrice				= v_DeletedPrMemPrc;
							phDrgs[i].SupplierDrugPrice			= v_DeletedPrSupPrc;
							phDrgs[i].TotalDrugPrice			= v_DeletedPrTotPrc;
							phDrgs[i].tikrat_mazon_flag			= v_DeletedPrTkMazon;
							phDrgs[i].SpecPrescNum				= v_DeletedPrIshNum;
							phDrgs[i].SpecPrescNumSource		= v_DeletedPrIshSrc;
							phDrgs[i].IshurWithTikra			= v_DeletedPrIshTik;
							phDrgs[i].IshurTikraType			= v_DeletedPrIshTkTp;
							phDrgs[i].rule_number				= v_DeletedPrRuleNum;
							v_DoctorRecipeNum		[i]			= v_DeletedPrDocPrID;	/* DonR 31Mar2011 */	 
							v_MemberDiscPC			[i]			= v_DeletedPrDiscPt;
							PR_Date					[i]			= v_DeletedPrDate;
							DrugDiscountAmt			[i]			= v_DeletedPrDiscnt;
							DrugDiscountCode		[i]			= v_DeletedPrDsCode;
							v_MemberParticType		[i]			= v_DeletedPrPrcCode;
							PR_HowParticDetermined	[i]			= v_DeletedPrPrtMeth;
							InHealthBasket			[i]			= v_DeletedPrBasket;
							v_MemberDiscPC			[i]			= v_DeletedPrDscPcnt;
							FixedParticipPrice		[i]			= v_DeletedPrFixPrc;
							DeletedPrPRW_ID			[i]			= v_DeletedPrPRW_ID;
							PR_Date					[i]			= v_DeletedPrPrDate;
							PR_Orig_Largo_Code		[i]			= v_DeletedPrPrLargo;
						}	// Good read of drug-lines cursor.
						else
						{
							if (SQLERR_error_test ())
							{
// ADD DIAGNOSTIC MESSAGE!
								v_ErrorCode = ERR_DATABASE_ERROR;
								break;
							}
						}
					}	// Loop through drug lines.
				}	// Pharmacy provided a list of drugs to be deleted from the original sale.
			}	// Deletion processing.


			// Check that drugs are in the drug_list table, and load in their Largo Type and packaging details.
			for (i = 0; i < v_NumOfDrugLinesRecs; i++)
			{
				// Read drug_list data by largo code.
				// DonR 14Apr2016: Switch to using the read_drug() routine, since that includes more sophisticated
				// data-massaging logic.
				// DonR 08Apr2018: Added new "see deleted drugs" parameter to read_drug(). Since spooled transactions
				// are often sent some time after the sale actually took place, we want to read deleted drugs even
				// if the current transaction is a drug sale - so set the parameter unconditionally TRUE.
				if (read_drug (phDrgs[i].DrugCode,
							   99999999,
							   &Phrm_info,
							   true,	// Deleted drugs are "visible" here for deletions AND sales.
							   &DL,
							   NULL))
				{
					phDrgs[i].LargoType			= DL.largo_type;
					phDrgs[i].package_size		= DL.package_size;
					phDrgs[i].openable_package	= DL.openable_pkg;
					phDrgs[i].THalf				= DL.t_half;
					v_in_gadget_table[i]		= DL.in_gadget_table;
					v_ingr_1_code				= DL.Ingredient[0].code;
					v_ingr_2_code				= DL.Ingredient[1].code;
					v_ingr_3_code				= DL.Ingredient[2].code;
					v_ingr_1_quant				= DL.Ingredient[0].quantity;
					v_ingr_2_quant				= DL.Ingredient[1].quantity;
					v_ingr_3_quant				= DL.Ingredient[2].quantity;
					v_ingr_1_quant_std			= DL.Ingredient[0].quantity_std;
					v_ingr_2_quant_std			= DL.Ingredient[1].quantity_std;
					v_ingr_3_quant_std			= DL.Ingredient[2].quantity_std;
					v_per_1_quant				= DL.Ingredient[0].per_quant;
					v_per_2_quant				= DL.Ingredient[1].per_quant;
					v_per_3_quant				= DL.Ingredient[2].per_quant;
					v_package_volume			= DL.package_volume;
				}
				else
				{
					Conflict_Test (reStart);

					if (SQLERR_code_cmp (SQLERR_not_found) == MAC_TRUE)
					{
						SetErrorVar (&v_ErrorCode, ERR_DRUG_CODE_NOT_FOUND);
						break;
					}

					if (SQLERR_error_test ())
					{
						SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR);
						break;
					}
				}

				// Save generic-ingredient fields as well.
				package_volume	[i]    = v_package_volume;
				ingr_code		[i][0] = v_ingr_1_code;
				ingr_code		[i][1] = v_ingr_2_code;
				ingr_code		[i][2] = v_ingr_3_code;
				ingr_quant		[i][0] = v_ingr_1_quant;
				ingr_quant		[i][1] = v_ingr_2_quant;
				ingr_quant		[i][2] = v_ingr_3_quant;
				ingr_quant_std	[i][0] = v_ingr_1_quant_std;
				ingr_quant_std	[i][1] = v_ingr_2_quant_std;
				ingr_quant_std	[i][2] = v_ingr_3_quant_std;
				per_quant		[i][0] = v_per_1_quant;
				per_quant		[i][1] = v_per_2_quant;
				per_quant		[i][2] = v_per_3_quant;

			}	// Loop on drugs.



			// DonR 30Apr2015: It appears that Yarpa (at least) has a bug of some sort: they sometimes send a Maccabi
			// Prescription ID that is incorrect. Until now, the spool version of Trn. 2005/5005 has not verified what
			// the pharmacy sends us - but this now appears to be a problem. Accordingly, we'll perform one simple
			// check to see if the pharmacy's Maccabi Prescription ID is bogus or not; if it looks like a mistake, we'll
			// force it to zero, so the transaction will be treated as a new sale rather than updating something irrelevant
			// that was already in the database. (Of course, the redunancy check below will still operate!)
			PreviousErrorCode = 0;	// Paranoid re-initialization.
			if (v_RecipeIdentifier != 0)
			{
				ExecSQL (	MAIN_DB, TR6005spool_READ_check_that_pharmacy_sent_correct_values,
							&sq_pharmnum,			&sq_MemberID,		&sq_DateDB,
							&sq_tikra_discount,		&sq_subsidy_amount,	&Dummy_online_order_num,
							&PreviousErrorCode,
							&v_RecipeIdentifier,	END_OF_ARG_LIST									);

				Conflict_Test (reStart);

				if ((SQLERR_code_cmp (SQLERR_not_found) == MAC_TRUE)	||
					((SQLCODE == 0) &&
					 ((sq_pharmnum	!= v_PharmNum)						||
					  (sq_MemberID	!= v_MemberID)						||
					  (sq_DateDB	!= v_DeliverConfirmationDate))))
				{
					// The Maccabi Prescription ID sent by the pharmacy was either not found in the database,
					// or else what was found for that ID didn't match the Pharmacy Code, Member ID, and
					// Sale Date in the database - so assume that the Prescription ID the pharmacy sent
					// is erroneous.
					GerrLogMini (GerrId, "5005-spool: data for sent for PrID %d does not match DB.\n     Member %d (%d in DB)\n"
										 "     Pharmacy %d (%d in DB)\n     Date %d (%d in DB)\n     SQLCODE = %d.",
								 v_RecipeIdentifier, v_MemberID, sq_MemberID,
								 v_PharmNum, sq_pharmnum, v_DeliverConfirmationDate, sq_DateDB, SQLCODE);

					v_RecipeIdentifier = 0;
				}
				else
				{	// Either a good read with matching values, or else a database error of some sort.
					if (SQLERR_error_test ())
					{
						v_ErrorCode = ERR_DATABASE_ERROR;
						break;
					}

					else
					{
						// Successful read of matching transaction. If "equality" test is enabled,
						// make sure that the pharmacy hasn't changed the Total Tikra Discount
						// or the Subsidy Amount from what was sent in the response to Trn. 6003.
						if (TestSaleEquality)
						{
							// Note that for diagnostic purposes, we want to test all the relevant "equality"
							// fields - so we don't quit once we've discovered a problem.
							if (v_TotalTikraDisc	!= sq_tikra_discount)
							{
								AmountMismatch = 1;

//								GerrLogMini (GerrId,
//											 "5005s: %d: Tikra %d, pharm %d sent %d%s.",
//											 v_RecipeIdentifier, sq_tikra_discount, v_PharmNum, v_TotalTikraDisc, (TestSaleEquality) ? "" : " (test disabled)");
							}

							if (v_TotalCouponDisc	!= sq_subsidy_amount)
							{
								AmountMismatch = 1;

//								GerrLogMini (GerrId,
//											 "5005s: %d: Subsidy %d, pharm %d sent %d%s.",
//											 v_RecipeIdentifier, sq_subsidy_amount, v_PharmNum, v_TotalCouponDisc, (TestSaleEquality) ? "" : " (test disabled)");
							}

							for (i = 0; i < v_NumOfDrugLinesRecs; i++)
							{
								// DonR 11Aug2016: Pharmacy must send the same Member Participation Amount as they got
								// from Transaction 6003 - if not, reject the transaction. Note that if this a deletion,
								// we have to compare a "flipped" version of the participation amount, since the pharmacy
								// sends it as a positive number and the database already has it as a negative number.
								if (v_RecipeSource != RECIP_SRC_NO_PRESC)
								{

									if ((PRIVATE_PHARMACY) || (DrugDiscountCode	[i] == 0))
									{
										ComparePharmPtn = (v_ActionType == SALE_DELETION) ?	(0 - phDrgs[i].TotalMemberParticipation) :
																							phDrgs[i].TotalMemberParticipation;

										ExecSQL (	MAIN_DB, READ_prescription_drugs_member_ptn_amt,
													&sq_member_ptn_amt,
													&v_RecipeIdentifier,	&phDrgs[i].DrugCode,	END_OF_ARG_LIST		);

										// No real error-handling is needed here.
										if ((SQLCODE == 0) && (ComparePharmPtn != sq_member_ptn_amt))
										{
//											GerrLogMini (GerrId,
//														 "5005s: %d/%d: Ptn = %d, pharm %d sent %d.",
//														 v_RecipeIdentifier, phDrgs[i].LineNo, sq_member_ptn_amt, v_PharmNum, ComparePharmPtn);

											AmountMismatch = 1;
										}	// Successful read of prescription-drugs row, but ptn. amounts don't match.
									}	// Sale is either at a private pharmacy, or does *not* have a Drug Discount Code for this drug.
								}	// Not an over-the-counter sale.
							}	// Loop through drugs to find participation-amount discrepancies.
						}	// "Equality" test is enabled.
					}	// Successful read of prescriptions row with matching Pharmacy Code, Member ID, and Sale Date.
				}	// Successful read of prescriptions row OR database error.
			}	// Pharmacy sent a non-zero Maccabi Prescription ID.


			// Check if same prescription already exists (in delivered form) in the database.
			// If so, mark the new version as deleted.
			// DonR 02Jan2018: As in Transaction 6005, we want to check for duplations even if Member ID is zero.
//			if (v_MemberID == 0)
//			{
//				same_pr_count = 0;
//			}
//			else
//			{
				// DonR 06Sep2011: If pharmacy has sent a deletion request for a sale that has already
				// been fully deleted, treat this request the same as a duplicate sale.
				if ((v_ActionType == SALE_DELETION)	&& ((v_DeletedPrDelFlg == 1) || (v_NumOfDrugLinesRecs == 0)))
				{
					same_pr_count = 1;	// Force "duplicate" status.
				}
				else
				{
					SET_ISOLATION_DIRTY;

					// DonR 04Jul2016: Pharmacies sometimes send a bunch of identical spooled transactions as
					// a single batch; to make sure the redundancy check works properly, we want to make sure
					// that we process only one spooled sale at a time for any given pharmacy. the new function
					// GetSpoolLock() ensures that only one instance of sqlserver.exe can be processing a spooled
					// transaction at any given moment.
					// DonR 28Jul2025: Adding Transaction Number / Spool Flag to GetSpoolLock(), for diagnostics
					// only - since I'm adding a call to this function in Transaction 2086 (which is apparently
					// getting a lock on pharmacy_daily_sum and causing deadlocks).
					GetSpoolLock (v_PharmNum, 5005, 1);

					// DonR 29Jun2010: Changed order of WHERE conditions to conform to index structure.
					// DonR 07Feb2011: Added action_type to WHERE conditions.
					// DonR 08Feb2011: Change test on delivered_flg, since we're no longer using only 1 and 0 there.
					// DonR 28Mar2011: Per Iris, removed member ID/Code from SELECT criteria; also added a criterion
					// for del_flg, since we don't want to consider the new transaction a duplicate unless the
					// previous transaction(s) were "for real". (In other words, we want to ignore transactions
					// with del_flg set to ROW_SYS_DELETED = 2.)
					ExecSQL (	MAIN_DB, READ_prescriptions_check_for_duplicate_spooled_sale,
								&same_pr_count,
								&v_PharmNum,			&v_MonthLog,		&v_PharmRecipeNum,
								&v_MemberBelongCode,	&v_PharmPtnCode,	&DRUG_NOT_DELIVERED,
								END_OF_ARG_LIST														);

					Conflict_Test (reStart);

					if (SQLERR_error_test ())
					{
						SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR);
						SET_ISOLATION_COMMITTED;
						break;
					}

					SET_ISOLATION_COMMITTED;
				}	// Need to check database for duplicate sales.
//			}	// Non-zero Member ID.


			// If the delivered prescription is already in the database, add the new version
			// with del_flg set to 2 (pre-deleted) and reported_to_as400 set to 1 (already
			// reported). Otherwise, add with these two fields set to zero - that is,
			// non-deleted and not yet reported to AS400.
			// DonR 05Sep2016: If we detected an amount discrepancy in Tikra Discount, Subsidy
			// Amount, or at least one drug participation amount, flag the row as "deleted by system"
			// and apply a new reported-to-AS/400 code of 3; this will make the relevant rows easy
			// to find in the database.
			if (same_pr_count > 0)
			{
				delete_flag			= ROW_SYS_DELETED;
				report_as400_flag	= REPORTED_TO_AS400;
			}
			else
			{
				if (AmountMismatch)
				{
					delete_flag			= ROW_SYS_DELETED;
					report_as400_flag	= SET_ASIDE_AMT_DISCREPANCY;
				}
				else
				{
					// DonR 13Jul2020 CR #32984 Part 4 (new version): Instead of checking the doctor_presc
					// table for previously-sold prescriptions, in spool mode we only want to check whether
					// we *already* detected this problem in the online transactions.
					switch (PreviousErrorCode)
					{
						case TR6005_RX_ALREADY_FULLY_SOLD:		delete_flag			= ROW_SYS_DELETED;
																report_as400_flag	= TR6005_RX_ALREADY_FULLY_SOLD;
																break;

						// Not part of the CR - but if there was a previous fatal error code
						// that should block the sale, we don't want to let it go through in
						// spool mode either! Note, though, that I don't see any actual cases
						// where this has happened in real life; apparently the pharmacy-side
						// systems prevent this kind of thing from happening.
						case ERR_PRESCRIPTIONID_NOT_APPROVED:	// From previous Trn. 6005-online.
						case ERR_DRUG_S_WRONG_IN_PRESC:			// From previous Trn. 6003.
																delete_flag			= ROW_SYS_DELETED;
																report_as400_flag	= PreviousErrorCode;
																break;

						default:								// If we get here, this is (or should be) a valid sale completion.
																delete_flag			= ROW_NOT_DELETED;
																report_as400_flag	= NOT_REPORTED_TO_AS400;
																break;
					}	// CR #32984 Part 4 (new version) end.
				}	// NOT an amount mismatch.
			}	// Not a redundant transaction.


			// Use the new Recipe ID (that is, insert new rows) if one of two conditions is true:
			// 1) Delivered prescription is already in the database, and the new version is being
			//    added for completeness (or whatever the reason is) only;
			// 2) The pharmacy sent the 5005 message with Recipe ID of zero - meaning that the
			//    2003/2004 transaction sequence was never completed, and so we need to add
			//    instead of updating.
			// The third possibility is that both conditions are false: that is, the pharmacy
			// reported a Recipe ID, but that recipe is currently marked as non-delivered. In this
			// case we'll do an update, just like a regular 5005 transaction.
			if ((delete_flag == ROW_SYS_DELETED) || (v_RecipeIdentifier == 0))
			{
				v_RecipeIdentifier = v_RecipeIdentifier1;
				AddNewRows = 1;	// DonR 03May2004 - so we remember that we've got to add new rows.
			}

			// End of dummy loop to prevent goto.
		}
		while (0);	// Loop should run only once.


		// Update tables.
		//
		// And now...
		// Yet another dummy loop to avoid goto!
		// Reply is sent to pharmacy when we exit from loop.
		if (reStart == MAC_FALS)	// No major database problem so far.
		{
			do
			{
				// Update Pharmacy Daily Sum ONLY IF we don't already have the delivered prescription
				// in the database.
				if (delete_flag != ROW_SYS_DELETED)
				{
					// Update PHARMACY_DAILY_SUM table.
					// DonR 15Apr2007: Pharmacy_daily_sum is written based on sale date, NOT
					// on system date!
					//
					// DonR 05Aug2009: Don't add private-pharmacy non-prescription sales to pharmacy_daily_sum.
					// DonR 23Aug2010: Per Iris Shaya, Total Tikra Discount and Total Coupon Discount need to be
					// subtracted from the participation amount for this sale.
					// DonR 11Jan2011: For sale deletions, AS/400 will report Tikra and Coupon reductions in
					// participation as negative numbers. This means that in calculating the member participation
					// written to pharmacy_daily_sum, these numbers need to be added, not subtracted. Accordingly,
					// this bit of math will be split into two versions and moved to the "if" based on the current
					// Action Type.
					if ((MACCABI_PHARMACY) || (v_RecipeSource != RECIP_SRC_NO_PRESC))
					{
						// Calculate prescription total
						sq_TotalMemberPrice	= 0;
						sq_TotalDrugPrice	= 0;
						sq_TotSuppPrice		= 0;

						for (i = 0; i < v_NumOfDrugLinesRecs; i++)
						{
							sq_TotalMemberPrice	+= phDrgs[i].TotalMemberParticipation;
							sq_TotalDrugPrice	+= phDrgs[i].TotalDrugPrice;

							sq_TotSuppPrice		+= phDrgs[i].SupplierDrugPrice * phDrgs[i].Op;

							if (phDrgs[i].Units > 0)
							{
								sq_TotSuppPrice	+= (int)(  (float)phDrgs[i].SupplierDrugPrice
														 * (float)phDrgs[i].Units
														 / (float)phDrgs[i].package_size);
							}
						}


						// Set up variables to insert/update Pharmacy Daily Sum data. There are five
						// possibilities:
						// 1) Drug sale.
						// 2) Conventional sale deletion.
						// 3) Deletion for prior month or different pharmacy; this gets recorded as a
						//    "negative sale" to the pharmacy performing the deletion.
						// 4) Return for credit ("zicui") - treated like (3).
						// 5) Cancellation of return - treated like (1).
						// Note that for any strange values of Action Type, we will assume that a drug
						// sale was intended.
						if ((v_ActionType != SALE_DELETION) && (v_ActionType != RETURN_FOR_CREDIT))
						{
							sq_TotalMemberPrice	-= (v_TotalTikraDisc + v_TotalCouponDisc);	// DonR 23Aug2010 / 11Jan2011.
							PDS_sum				=  sq_TotalDrugPrice;
							PDS_prt_sum			=  sq_TotalMemberPrice;
							PDS_purch_sum		=  sq_TotSuppPrice;
							PDS_lines			=  v_NumOfDrugLinesRecs;
							PDS_count			=  1;
							PDS_del_sum			=  0;
							PDS_del_prt_sum		=  0;
							PDS_del_purch_sum	=  0;
							PDS_del_lines		=  0;
							PDS_del_count		=  0;
							PDS_date			=  v_DeliverConfirmationDate;
						}
						else
						{	// If we get here, we are dealing with either a deletion (which may be "conventional"
							// or not) or a return for credit, which is treated like an "unconventional" deletion.

							// DonR 11Jan2011: For deletions, discounts are sent by AS/400 as negative numbers.
							sq_TotalMemberPrice	+= (v_TotalTikraDisc + v_TotalCouponDisc);	// DonR 11Jan2011.

							if ((v_ActionType	== SALE_DELETION)		&&
								(v_PharmNum		== v_DeletedPrPharm)	&&
								(v_MonthLog		== v_DeletedPrYYMM))
							{
								// Conventional sale deletion.
								PDS_sum				= 0;
								PDS_prt_sum			= 0;
								PDS_purch_sum		= 0;
								PDS_lines			= 0;
								PDS_count			= 0;
								PDS_del_sum			= sq_TotalDrugPrice;	// Should be positive.
								PDS_del_prt_sum		= sq_TotalMemberPrice;	// Should be positive.
								PDS_del_purch_sum	= sq_TotSuppPrice;		// Should be positive.
								PDS_del_lines		= v_NumOfDrugLinesRecs;
								PDS_del_count		= 1;

								// DonR 02Mar2011: For conventional sale deletions, read the original sale date,
								// since that's the date to be used for updating pharmacy_daily_sum.
								ExecSQL (	MAIN_DB, READ_deleted_sale_date,
											&PDS_date, &v_DeletedPrID, END_OF_ARG_LIST	);

								if (SQLCODE != 0)
									PDS_date = v_DeliverConfirmationDate;
							}
							else
							{
								// Deletion for prior month and/or different pharmacy, OR return for credit -
								// treat as "negative sale".
								PDS_sum				= 0 - sq_TotalDrugPrice;	// Should be positive, so subtract.
								PDS_prt_sum			= 0 - sq_TotalMemberPrice;	// Should be positive, so subtract.
								PDS_purch_sum		= 0 - sq_TotSuppPrice;		// Should be positive, so subtract.
								PDS_lines			= v_NumOfDrugLinesRecs;
								PDS_count			= 1;
								PDS_del_sum			= 0;
								PDS_del_prt_sum		= 0;
								PDS_del_purch_sum	= 0;
								PDS_del_lines		= 0;
								PDS_del_count		= 0;
								PDS_date			= v_DeliverConfirmationDate;
							}
						}

						// DonR 21Mar2011: For better performance, try updating rather than inserting first.
						// The vast majority of the time, we'll be updating an existing row - so why waste
						// time trying to insert, failing, and only then updating?
						// DonR 03May2015: Add a retry loop, since I'm seeing cases where the UPDATE fails but
						// then the INSERT fails too, with SQL duplicate key error.
						// DonR 13Jul2016: Added one more retry, and increased the pause before retry from five
						// to fifty milliseconds; this should, I hope, reduce the number of transactions that
						// fail with a DB error.
						// DonR 09Aug2016: Doubled sleep interval and added a third retry.
						for (i = 0; i < 4; i++)
						{
							if (i > 0)
								usleep (100000);	// Sleep 100 milliseconds before retry.

							ExecSQL (	MAIN_DB, UPD_pharmacy_daily_sum,
										&PDS_sum,			&PDS_prt_sum,			&PDS_purch_sum,
										&PDS_count,			&PDS_lines,				&PDS_del_sum,
										&PDS_del_prt_sum,	&PDS_del_lines,			&PDS_del_count,
										&PDS_del_purch_sum,
										&v_PharmNum,		&v_InstituteCode,		&v_MonthLog,
										&PDS_date,			&v_TerminalNum,			END_OF_ARG_LIST		);

							// DonR 31Dec2015: If the UPDATE succeeded, we need to break out of the retry loop so we don't 
							// perform the update twice! (In fact, the *only* situation in which we want to stay in the loop
							// is if both the UPDATE and the INSERT failed with "no rows affected" and "duplicate key",
							// respectively.)
							// DonR 05Jan2016 HOT FIX: If the update found nothing to update, SQLCODE will still be equal
							// to zero - so we have to look at "no rows affected" separately.
							if (DB_UPDATE_WORKED)
								break;	// We succeeded in the UPDATE, so abort the retry loop.

							// If there was nothing to update, try inserting a new row.
							if (SQLERR_code_cmp (SQLERR_no_rows_affected) == MAC_TRUE)
							{
								ExecSQL (	MAIN_DB, INS_pharmacy_daily_sum_19_columns,
											&v_PharmNum,		&v_InstituteCode,		&v_MonthLog,
											&PDS_date,			&v_TerminalNum,			&PDS_sum,
											&PDS_count,			&PDS_lines,				&PDS_prt_sum,
											&PDS_purch_sum,		&PDS_del_sum,			&PDS_del_prt_sum,

											&PDS_del_lines,		&PDS_del_count,			&PDS_del_purch_sum,
											&IntZero,			&IntZero,				&IntZero,
											&IntZero,			END_OF_ARG_LIST										);

								if (SQLERR_code_cmp (SQLERR_not_unique) == MAC_FALSE)
									break;	// Retry only on duplicate-key error.

							}	// No rows affected on UPDATE, so try INSERT.

							// DonR 31Dec2015: If we get here, either the UPDATE failed with something *other* than "no rows affected",
							// or else the UPDATE got "no rows affected" *AND* the INSERT failed with a duplicate-key error. This
							// situation, and *only* this situation, is when we want to re-attempt the UPDATE.

						}	// Retry loop.

						Conflict_Test (reStart);

						if (SQLERR_error_test ())
						{
							GerrLogMini (GerrId, "5005-spool: DB error %d writing to pharmacy_daily_sum for %d/%d/%d/%d after %d attempts.",
										 SQLCODE, v_PharmNum, v_MonthLog, PDS_date, v_TerminalNum, i);
							SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR);
							break;
						}
					}	// DonR 05Aug2009: Insert/update to pharmacy_daily_sum is now conditional.
				}	// We are adding/updating the delivery "for real".


				// Write to DOCTOR_PRESC & PRESCRIPTION_DRUGS tables.
				for (i = 0; i < v_NumOfDrugLinesRecs; i++)
				{
					PW_ID				= 0;	// Default value in case SQL doesn't find anything - shouldn't really be needed.
					PW_AuthDate			= 0;
					phDrgPtr			= &phDrgs			[i];
					PD_discount_pc		= v_MemberDiscPC	[i];
					PRW_OpUpdate		= PRW_Op_Update		[i];
					v_DrugDiscountAmt	= DrugDiscountAmt	[i];
					v_DrugDiscountCode	= DrugDiscountCode	[i];
					LineNum				= i + 1;

					// DonR 14Jan2009: Change 99 ("kacha") to just 9, since the "tens" digit is now used
					// for additional information.
					PW_SubstPermitted	= PR_WhyNonPreferredSold[i] % 10;	// 10May2007: Per Iris, don't translate this.

					StopUseDate   = AddDays (v_DeliverConfirmationDate, phDrgPtr->Duration);
					StopBloodDate = AddDays (StopUseDate, phDrgPtr->THalf);

					if (StopBloodDate > max_drug_date)
					{
						max_drug_date = StopBloodDate;
					}

					// Update Prescriptions Written ONLY IF we don't already have the delivered
					// prescription in the database.
					// DonR 05Sep2006: Update Prescriptions Written only for "real" members.
					// DonR 06Jul2010: Update Prescriptions Written only for sales and sale deletions.
					// DonR 28Apr2015: Update doctor_presc only if Prescription Source == Maccabi Doctor,
					// since other types of prescription won't be in our database anyway.
					if ((v_ActionType	!= RETURN_FOR_CREDIT)		&&
						(v_ActionType	!= RETURN_DELETION)			&&
						(delete_flag	!= ROW_SYS_DELETED)			&&
						(v_MemberID		>  0)						&&
						(v_RecipeSource == RECIP_SRC_MACABI_DOCTOR))
					{
						update_doctor_presc (	0,		// Normal mode - NOT processing late-arriving doctor prescriptions.
												v_ActionType,
												v_MemberID,					v_IDCode,
												v_RecipeIdentifier,			LineNum,
												v_DoctorID,					v_DocIDType,					v_DoctorRecipeNum[i],
												PR_Orig_Largo_Code[i],		phDrgPtr->DrugCode,				NULL,	// No TDrugListRow structure.
												PR_Date[i],					v_Date,							v_Hour,
												PRW_OpUpdate,				phDrgPtr->Units,
												PrevUnsoldOP[i],			PrevUnsoldUnits[i],
												0 /*phDrgPtr->use_instr_template*/ ,			// DonR 01Aug2024 Bug #334612: Get "ratzif" from pharmacy instead of from doctor Rx.
												v_DeletedPrID,
												1,							NULL,	// Old-style single-prescription mode.
												&PR_Orig_Largo_Code[i],		&PR_Date[i],
												&phDrgs[i].NumRxLinks,
												NULL,	NULL,	NULL,	NULL		// Doctor visit & Rx arrival timestamps - relevant only for spool processing.
											);
					}	// Need to update doctor_presc.


					// Set up Sold Ingredients fields.
					for (j = 0; j < 3; j++)
					{
						if ((ingr_code	[i][j]		<  1)	||
							(ingr_quant	[i][j]		<= 0.0)	||
							(per_quant	[i][j]		<= 0.0)	||
							(phDrgs[i].package_size <  1))
						{
							// Invalid values - set this slot to zero.
							ingr_code			[i][j] = 0;
							ingr_quant_bot		[i][j] = 0.0;
							ingr_quant_bot_std	[i][j] = 0.0;
						}

						else
						{
							if (phDrgs[i].package_size == 1)
							{
								// Syrups and similar drugs.
								ingr_quant_bot[i][j] =	  (double)phDrgs[i].Op
														* package_volume[i]
														* ingr_quant	[i][j]
														/ per_quant		[i][j];

								ingr_quant_bot_std[i][j] =	  (double)phDrgs[i].Op
															* package_volume[i]
															* ingr_quant_std[i][j]
															/ per_quant		[i][j];
							}

							else	// Package size > 1.
							{
								// Tablets, ampules, and similar drugs.
								// For these medications, ingredient is always given per unit.
								UnitsSold	=	  (phDrgs[i].Op		* phDrgs[i].package_size)
												+  phDrgs[i].Units;

								ingr_quant_bot		[i][j] = (double)UnitsSold * ingr_quant		[i][j];
								ingr_quant_bot_std	[i][j] = (double)UnitsSold * ingr_quant_std	[i][j];
							}
						}	// Valid quantity values available.
					}	// Loop through ingredients for this drug.

					// Load DB variables with computed values. Note that the Ingredient Code
					// may have been zeroed out, so it needs to be reloaded!
					v_ingr_1_code		= ingr_code			[i][0];
					v_ingr_2_code		= ingr_code			[i][1];
					v_ingr_3_code		= ingr_code			[i][2];
					v_ingr_1_quant		= ingr_quant_bot	[i][0];
					v_ingr_2_quant		= ingr_quant_bot	[i][1];
					v_ingr_3_quant		= ingr_quant_bot	[i][2];
					v_ingr_1_quant_std	= ingr_quant_bot_std[i][0];
					v_ingr_2_quant_std	= ingr_quant_bot_std[i][1];
					v_ingr_3_quant_std	= ingr_quant_bot_std[i][2];

					// DonR 05Jul2010: Per Iris Shaya, pharmacies send positive numbers for OP/Units,
					// Total Member Participation, and Total (Supplier) Drug Price in sale-deletion
					// requests. We "flip" these numbers to negative when writing to the database.
					// DonR 03Jul2013: Instead of "flipping" the numbers stored in phDrgPtr, use a set
					// of simple variables. This way, when we hit a database lock and execute this block
					// of code more than once, the underlying "real" variables will remain positive and
					// the numbers we write to the database for deletions will be reliably negative.
					if ((v_ActionType == SALE_DELETION) || (v_ActionType == RETURN_FOR_CREDIT))
					{
						v_SignedTotPtn		= phDrgPtr->TotalMemberParticipation	* -1;
						v_SignedTotDrugPrc	= phDrgPtr->TotalDrugPrice				* -1;
						v_SignedOP			= phDrgPtr->Op							* -1;
						v_SignedUnits		= phDrgPtr->Units						* -1;
					}
					else
					{
						// Ordinary drug sale or return-for-credit reversal - just copy the array/structure
						// variables into the simple variables without changing their sign.
						v_SignedTotPtn		= phDrgPtr->TotalMemberParticipation;
						v_SignedTotDrugPrc	= phDrgPtr->TotalDrugPrice;
						v_SignedOP			= phDrgPtr->Op;
						v_SignedUnits		= phDrgPtr->Units;
					}


					// If this is a deletion, mark the original sold-drug row as deleted.
					// DonR 31Jan2012: If this is a duplicate deletion (for whatever reason), DO NOT
					// update the original sale!
					// DonR 04Jul2016: Instead of repeating essentially the same code in eight places (i.e. two
					// tables, Transactions 5005 and 6005, online and spooled versions), created a single function
					// MarkDeleted() that handles the deletion-flag update. It returns zero if all went OK, and
					// anything else indicates a database error.
					if ((v_ActionType == SALE_DELETION) && (delete_flag	!= ROW_SYS_DELETED))
					{
						if (MarkDeleted ("5005sp", v_DeletedPrID, phDrgPtr->DrugCode, v_RecipeIdentifier) != 0)
						{
							SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR);
							break;
						}
					}


					// Insert or update prescription_drugs row.

//					// DonR 10Feb2020: For some bizarre reason, Member Discount Percent is an INTEGER in prescription_drugs, even though it's a
//					// SMALLINT in members and prescriptions. Accordingly, since ODBC does not automatically convert numeric types and behaves
//					// strangely (not to say abominably) when "incorrect" types are given, copy the short value in phDrgPtr->MemberDiscPC to an
//					// integer variable and use that to write to prescription_drugs. (Note that price_extension is also an INTEGER, so we need
//					// to make the fix for that column as well.)
//					MemberDiscPercentInt = v_MemberDiscPC [i];	// DonR 10Feb2020.
//
					if (AddNewRows)
					{
						// DonR 22Nov2005: If the "drug" purchased is a "gadget" but we didn't
						// get participation from the AS400, zero out the Link-to-Addition
						// variable so the AS400 won't get the wrong idea about where
						// participation came from.
						if (((PR_HowParticDetermined [i] % 10)	!= FROM_GADGET_APP)	&&
							(v_in_gadget_table[i]				!= 0))
						{
							phDrgs[i].LinkDrugToAddition = 0;
						}

						// Insert new prescription_drugs row.
						// DonR 07Jul2015: added value for elect_pr_status for compatibility with Mersham Digitali.
						// DonR 29May2019: Fixed assignment of price_extension; it was previously getting its value
						// from phDrgPtr->Discount, which was never assigned a value.
						// DonR Feb2023 User Story #232220: Add paid_for to columns INSERTed.
						ExecSQL (	MAIN_DB, TR5005spool_INS_prescription_drugs,
									&v_RecipeIdentifier,				&LineNum,
									&v_MemberID,						&v_IDCode,
									&phDrgPtr->DrugCode,				&report_as400_flag,
									&v_SignedOP,						&v_SignedUnits,
									&phDrgPtr->Duration,				&StopUseDate,

									&v_DeliverConfirmationDate,			&v_DeliverConfirmationHour,
									&delete_flag,			 			&v_ActionType,
									&FixedParticipPrice	[i],			&v_MemberDiscPC			[i],
									&phDrgPtr->LinkDrugToAddition,		&phDrgPtr->OpDrugPrice,
									&v_ActionType,						&v_ErrorCode,

									&v_MemberParticType	[i],			&v_MemberParticType	[i],
									&v_RecipeSource,					&ShortZero,
									&v_PharmNum,						&v_InstituteCode,
									&phDrgPtr->SupplierDrugPrice,		&StopBloodDate,
									&v_SignedTotDrugPrc,				&phDrgPtr->CurrentStockOp,

									&phDrgPtr->CurrentStockInUnits,		&v_SignedTotPtn,
									&v_SignedTotPtn,					&v_Comm_Error,
									&phDrgPtr->THalf,					&v_DoctorID,
									&v_DocIDType,						&InHealthBasket			[i],
									&phDrgPtr->SpecPrescNum,			&phDrgPtr->SpecPrescNumSource,

									&v_DoctorRecipeNum	[i],			&PR_Date[i],
									&PR_Orig_Largo_Code	[i],			&PW_SubstPermitted,
									&PR_NumPerDose		[i],			&PR_DosesPerDay			[i],
									&v_MemberDiscPC		[i],			&PR_HowParticDetermined	[i],
									&PW_ID,								&v_ingr_1_code,

									&v_ingr_2_code,						&v_ingr_3_code,
									&v_ingr_1_quant,					&v_ingr_2_quant,
									&v_ingr_3_quant,					&v_ingr_1_quant_std,
									&v_ingr_2_quant_std,				&v_ingr_3_quant_std,
									&v_DrugDiscountAmt,					&v_DrugDiscountCode,

									&phDrgPtr->tikrat_mazon_flag,		&phDrgPtr->IshurWithTikra,
									&phDrgPtr->IshurTikraType,			&phDrgPtr->rule_number,
									&v_CreditYesNo,						&v_ElectPrStatus,

									// DonR 25Jun2013: New "Bakarat Kamutit" fields are blank for sales
									// added through 2005/5005 spool. (But Doctor Visit Date isn't!)
									&ShortZero,							&ShortZero,
									&IntZero,							&IntZero,
									&PW_AuthDate,						&v_DocLocationCode,
									&phDrgPtr->NumRxLinks,				&paid_for,
									END_OF_ARG_LIST															);

						Conflict_Test (reStart);

						if (SQLERR_error_test ())
						{
							SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR);
							if (SQLERR_code_cmp (SQLERR_not_unique) == MAC_TRUE)
								GerrLogMini (GerrId, "Duplicate key inserting %d/%d to prescription_drugs.", v_RecipeIdentifier, LineNum);
							break;
						}
					}	// Need to insert.

					else
					{
						// DonR 14Jun2004: If "drug" being purchased is a device in the "gadgets"
						// table, retrieve the appropriate information and notify the AS400 of
						// the sale.
						if ((PR_HowParticDetermined [i] % 10) == FROM_GADGET_APP)
						{
							v_gadget_code	= phDrgs[i].LinkDrugToAddition;
							v_DoctorPrID	= v_DoctorRecipeNum	[i];	// Unless these are bogus values 
							v_DoctorPrDate	= PR_Date			[i];	// inserted by pharmacy in Trn. 2003.
							v_DrugCode		= phDrgs[i].DrugCode;
							Unit_Price		= phDrgs[i].OpDrugPrice;	// Unless Maccabi Price is relevant.

							// DonR 23Jun2010: Gadget Action Code is now dependent on whether this is a
							// conventional drug sale or a deletion.
							AS400_Gadget_Action	= (v_ActionType == SALE_DELETION) ? enActSterNo : enActWrite;

							// DonR 09Dec2010: If the current action is a sale deletion, send AS/400 the
							// Prescription ID of the original sale, NOT the ID of the deletion itself.
							GadgetRecipeID = (v_ActionType == SALE_DELETION) ? v_DeletedPrID : v_RecipeIdentifier;

							ExecSQL (	MAIN_DB, READ_Gadgets_for_sale_completion,
										&v_service_code,	&v_service_number,	&v_service_type,
										&v_DrugCode,		&v_gadget_code,		&v_gadget_code,
										END_OF_ARG_LIST												);

							Conflict_Test (reStart);

							if (SQLERR_error_test ())
							{
								SetErrorVar( &v_ErrorCode, ERR_DATABASE_ERROR );
								break;
							}

							// If we get here, the gadget entry really does exist.
							// DonR 09Dec2010: If the current action is a sale deletion, send AS/400 the
							// Prescription ID of the original sale, NOT the ID of the deletion, in second parameter.
							// DonR 19Aug2025 User Story #442308: If Nihul Tikrot calls are disabled, disable Meishar calls as well.
							if (TikrotRPC_Enabled)
							{
								GadgetError = as400SaveSaleRecord ((int)v_PharmNum,
																   (int)GadgetRecipeID,			// DonR 09Dec2010.
																   (int)v_MemberID,
																   (int)v_IDCode,
																   (int)v_service_code,
																   (int)v_service_number,
																   (int)v_service_type,
																   (int)v_DrugCode,
																   (int)v_SignedOP,
																   (int)Unit_Price,
																   (int)v_SignedTotPtn,
																   (int)v_DeliverConfirmationDate,	// Event Date
																   (int)v_DoctorID,
																   (int)v_DoctorPrDate,	// Request Date
																   (int)v_DoctorPrID,		// Request Num.
																   (int)AS400_Gadget_Action,
																   (int)2					// Retries
																  );
								GerrLogFnameMini ("gadgets_log",
												  GerrId,
												  "5005 Spool: AS400 err = %d.",
												  (int)GadgetError);
							}
							else	// If Nihul Tikrot calls are disabled, disable Meishar calls too.
							{
								GadgetError = -1;
							}

							if (GadgetError != 0)
							{
								v_action = AS400_Gadget_Action;

								// Write to "holding" table!
								ExecSQL (	MAIN_DB, INS_gadget_spool,
											&v_PharmNum,				&GadgetRecipeID,
											&v_MemberID,				&v_IDCode,
											&v_DrugCode,				&v_service_code,
											&v_service_number,			&v_service_type,
											&v_SignedOP,				&Unit_Price,

											&v_SignedTotPtn,			&v_DeliverConfirmationDate,
											&v_DoctorID,				&v_DoctorPrDate,
											&v_DoctorPrID,				&v_action,
											&GadgetError,				&SysDate,
											&v_Hour,					&IntZero,
											END_OF_ARG_LIST									);

								if (SQLERR_error_test ())
								{
									GerrLogToFileName ("gadgets_log",
													   GerrId,
													   "5005 Spool: DB Error %d writing to gadget_spool.",
													   (int)SQLCODE);

									v_ErrorCode = ERR_DATABASE_ERROR;
									break;
								}
							}	// Error notifying AS400 of sale.
						}	// Item sold got its participation from AS400 "Gadget" application.

						else
						{
							// DonR 22Nov2005: If the "drug" purchased is a "gadget" but we didn't
							// get participation from the AS400, zero out the Link-to-Addition
							// variable so the AS400 won't get the wrong idea about where participation
							// came from.
							if (v_in_gadget_table[i] != 0)
								phDrgs[i].LinkDrugToAddition = 0;
						}


						// Update prescription_drugs row.
						// DonR 20Aug2008: Need to read Maccabi Price Flag, since we're now updating it below.
						ExecSQL (	MAIN_DB, TR2005_5005spool_READ_PD_macabi_price_flg,
									&LineNum,				&phDrgPtr->MacabiPriceFlag,
									&v_RecipeIdentifier,	&phDrgPtr->DrugCode,
									END_OF_ARG_LIST											);

						// DonR 03Oct2021: Add separate error-handling for row-not-found, so we don't write
						// needlessly alarming messages to the log.
						if (SQLERR_code_cmp (SQLERR_not_found) == MAC_TRUE)
						{
							GerrLogMini (GerrId, "5005-spool: Row-not-found reading %d Largo %d (line #%d).",
										 v_RecipeIdentifier, phDrgPtr->DrugCode, phDrgPtr->LineNo);

							SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR);
							break;
						}
						else
						{
							if (SQLCODE == 0)
							{
								// DonR 20Aug2008: Added update of Maccabi Price Flag, so it can be reset to zero
								// for "gadget" sales. This was already happening in "normal" Trn. 2005, but the
								// field was missing from the SQL UPDATE statement below.
								// DonR 08Feb2011: Changed update of delivered_flg to use the variable v_ActionType
								// instead of hard-coded value of 1.
								// DonR Feb2023 User Story #232220: Add paid_for to columns UPDATEd.
								ExecSQL (	MAIN_DB, TR5005spool_UPD_prescription_drugs,
											&v_SignedOP,							&v_SignedUnits,
											&v_ActionType,							&NOT_REPORTED_TO_AS400,
											&v_Comm_Error,							&phDrgPtr->CurrentStockOp,
											&phDrgPtr->CurrentStockInUnits,			&v_SignedTotDrugPrc,
											&v_SignedTotPtn,						&v_MemberDiscPC [i],

											&phDrgPtr->SpecPrescNum,				&phDrgPtr->SpecPrescNumSource,
											&v_ingr_1_code,							&v_ingr_2_code,
											&v_ingr_3_code,							&v_ingr_1_quant,
											&v_ingr_2_quant,						&v_ingr_3_quant,
											&v_ingr_1_quant_std,					&v_ingr_2_quant_std,

											&v_ingr_3_quant_std,					&v_DeliverConfirmationDate,
											&v_DeliverConfirmationHour,				&phDrgPtr->MacabiPriceFlag,
											&PR_Orig_Largo_Code[i],					&PR_Date[i],
											&phDrgPtr->LinkDrugToAddition,			&phDrgPtr->NumRxLinks,
											&v_ElectPrStatus,						&phDrgPtr->SupplierDrugPrice,

											&v_DrugDiscountAmt,						&v_DrugDiscountCode,
											&paid_for,

											&v_RecipeIdentifier,					&LineNum,
											END_OF_ARG_LIST															);
							}
						}	// Something other than row-not-found reading prescription_drugs.

						// If we get here, either we did read the prescription_drugs row without any problem, or else
						// there was a problem other than row-not-found, and we need to write it to the log.
						Conflict_Test (reStart);

						if (SQLERR_error_test ())
						{
							SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR);
							break;
						}

					}	// Update prescription_drugs row.
				}	// Loop through drugs.


				// If any SQL errors were detected inside drug loop, break out of dummy loop.
				// DonR 03Oct2021: If we've already detected and logged an error, don't bother
				// logging the same error again - just break out of the loop.
				if (v_ErrorCode == ERR_DATABASE_ERROR)
				{
					break;
				}
				else
				{
					if (SQLERR_error_test ())
					{
						v_ErrorCode = ERR_DATABASE_ERROR;
						break;
					}
				}


				// Update max_drug_date in members table.
				// DonR 05Sep2006: Update only for "real" members.
				// DonR 20Nov2007: ...And not only "real", but also Maccabi - others aren't
				// present in the Members table.
				if ((v_ActionType		== DRUG_SALE)			&&
					(delete_flag		!= ROW_SYS_DELETED)		&&
					(max_drug_date		>  sq_maxdrugdate)		&&
					(v_MemberID			>  0)					&&
					(v_MemberBelongCode	== MACABI_INSTITUTE))
				{
					ExecSQL (	MAIN_DB, UPD_members_max_drug_date,
								&max_drug_date,		&v_MemberID,
								&v_IDCode,			END_OF_ARG_LIST		);

					if (SQLERR_error_test ())
					{
						SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR);
						break;
					}
				}


				// If this is a deletion of all drugs in a sale, mark the original
				// prescriptions row as deleted.
				// DonR 31Jan2012: If this is a duplicate deletion (for whatever reason), DO NOT
				// update the original sale!
				// DonR 04Jul2016: Instead of repeating essentially the same code in eight places (i.e. two
				// tables, Transactions 5005 and 6005, online and spooled versions), created a single function
				// MarkDeleted() that handles the deletion-flag update. It returns zero if all went OK, and
				// anything else indicates a database error.
				if ((v_ActionType == SALE_DELETION) && (delete_flag	!= ROW_SYS_DELETED))
				{
					if (MarkDeleted ("5005sp", v_DeletedPrID, 0, v_RecipeIdentifier) != 0)
					{
						SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR);
						break;
					}
				}

				// Insert or update prescriptions row.
				if (AddNewRows)
				{
					// DonR Feb2023 User Story #232220: Add paid_for to columns INSERTed.
					ExecSQL (	MAIN_DB, TR5005spool_INS_prescriptions,
								&v_PharmNum,				&v_InstituteCode,
								&v_PriceListCode,			&v_MemberID,
								&v_IDCode,					&v_MemberBelongCode,
								&v_DoctorID,				&v_DocIDType,
								&v_RecipeSource,			&v_RecipeIdentifier,

								&v_SpecialConfNum,			&v_SpecialConfSource,
								&v_DeliverConfirmationDate,	&v_DeliverConfirmationHour,
								&v_ErrorCode,				&v_NumOfDrugLinesRecs,
								&v_CreditYesNo,				&v_MoveCard,
								&ShortZero,					&v_TerminalNum,

								&v_InvoiceNum,				&v_UserIdentification,
								&v_PharmRecipeNum,			&Member.maccabi_code,
								&IntZero,					&report_as400_flag,
								&v_Comm_Error,				&v_PharmPtnCode,
								&v_ElectPrStatus,			&v_ClientLocationCode,

								&v_DocLocationCode,			&v_MonthLog,
								&v_PriceListCode,			&IntZero,				 // :v_SpecialRecipeStartDate
								&IntZero,					&ShortZero,				 // :v_SpecialRecipeStopDate, waiting_msg_flg
								&v_ActionType,				&delete_flag,
								&IntZero,					&v_ReasonToDispense,

								&v_ActionType,				&v_DeletedPrID,
								&v_DeletedPrPharm,			&v_DeletedPrDate,
								&v_DeletedPrPhID,			&v_CardOwnerID,
								&v_CardOwnerIDCode,			&ShortZero,
								&ShortZero,					&OriginCode,

								&v_CreditYesNo,				&v_RetnReasonCode,
								&v_CreditLinePmts,			&v_PaymentType,
								&v_TotalTikraDisc,			&v_TotalCouponDisc,
								&v_ElectPrStatus,			&ShortZero,
								&v_5003CommError,			&Member.insurance_type,
								&paid_for,					END_OF_ARG_LIST				);

					Conflict_Test (reStart);

					if (SQLERR_error_test ())
					{
						SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR);
						break;
					}
				}

				else
				{
					// Update PRESCRIPTIONS table.
					// DonR 30Oct2006: Bug fix! Updates to prescription_drugs were setting
					// the Reported-to-AS400 flag to zero, but the update to prescriptions
					// was not. I added the additional field update below.
					// DonR Feb2023 User Story #232220: Add paid_for to columns UPDATEd.
					ExecSQL (	MAIN_DB, TR5005spool_UPD_prescriptions,
								&v_InvoiceNum,				&v_UserIdentification,
								&v_PharmRecipeNum,			&v_ElectPrStatus,
								&v_ClientLocationCode,		&v_DocLocationCode,
								&v_MonthLog,				&v_ActionType,
								&v_CreditYesNo,				&v_DeliverConfirmationDate,
								&v_DeliverConfirmationHour,	&v_ErrorCode,
								&IntZero,					&v_ReasonToDispense,
								&v_Comm_Error,				&v_RetnReasonCode,
								&v_CreditLinePmts,			&v_PaymentType,
								&v_TotalTikraDisc,			&v_TotalCouponDisc,
								&v_ElectPrStatus,			&v_PharmPtnCode,
								&v_5003CommError,			&NOT_REPORTED_TO_AS400,
								&paid_for,

								&v_RecipeIdentifier,		END_OF_ARG_LIST					);

					// DonR 22May2013: Also mark prescription_msgs and prescription_usage rows as delivered. Since the
					// delivery isn't complete unless we write to the database without any problems, we can piggy-back
					// on the same error handling. (Note that "no rows affected" doesn't throw an error in
					// SQLCODE/SQLERR_error_test.)
					if (SQLCODE == 0)
					{
						ExecSQL (	MAIN_DB, UPD_prescription_msgs_mark_delivered,
									&v_DeliverConfirmationDate,	&v_DeliverConfirmationHour,	&v_ActionType,
									&NOT_REPORTED_TO_AS400,		&v_RecipeIdentifier,		END_OF_ARG_LIST	);
					}

					if (SQLCODE == 0)
					{
						ExecSQL (	MAIN_DB, UPD_prescription_usage_mark_delivered,
									&v_DeliverConfirmationDate,	&v_DeliverConfirmationHour,	&v_ActionType,
									&NOT_REPORTED_TO_AS400,		&v_RecipeIdentifier,		END_OF_ARG_LIST	);
					}

					Conflict_Test (reStart);

					if (SQLERR_error_test ())
					{
						SetErrorVar( &v_ErrorCode, ERR_DATABASE_ERROR );
						break;
					}
				}	// Update prescriptions row.

			}
			while (0);	// Dummy loop Should run only once.
			// End of dummy loop to prevent goto.

		}	// ReStart is FALSE.

		// DonR 27Aug2025: Clear the Pharmacy Spool Lock we set at the beginning of the transaction.
		ClearSpoolLock (v_PharmNum);

		// Commit the transaction.
		if (reStart == MAC_FALS)	// No retry needed.
		{
			// DonR 15Mar2011: Because we're dealing with spooled transactions here, there are a number of
			// ordinarily severe errors (such as drug-not-found or member-not-eligible) that may occur, but
			// which we do not want to prevent writing the transaction to the database. For example, when a
			// member has recently died, s/he will be considered "ineligible" - but refunds may still be
			// issued and cancelled. Accordingly, we want to abort the SQL transaction only if we've hit
			// a serious database error - not for any other error.
			if (v_ErrorCode != ERR_DATABASE_ERROR)
			{
				CommitAllWork ();
			}
			else
			{
				// Error, so do rollback.
				RollbackAllWork ();
			}

			if (SQLERR_error_test ())	// Commit (or rollback) failed.
			{
				SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR);
			}
		}
		else
		{
			// Need to retry.
			if (TransactionRestart () != MAC_OK)
			{
				v_ErrorCode = ERR_DATABASE_ERROR;
				break;
			}

			// DonR 26Jul2011: Suppress log message until the fourth attempt fails.
			if (tries > 2)
				GerrLogReturn (GerrId, "Table is locked for the <%d> time", tries);

			sleep (ACCESS_CONFLICT_SLEEP_TIME);
		}


	}	// End of retries loop.


	if (reStart == MAC_TRUE)
	{
		GerrLogReturn (GerrId, "Locked for <%d> times", SQL_UPDATE_RETRIES);

		SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR);
	}


	// Don't send warnings - only fatal errors.
	if (v_ErrorCode != NO_ERROR)
	{
		Sever = GET_ERROR_SEVERITY (v_ErrorCode);

		if (Sever <= FATAL_ERROR_LIMIT)
		{
			v_ErrorCode = NO_ERROR;
		}
	}


	// Create Response Message
	nOut =  sprintf (OBuffer,		 "%0*d",	MSG_ID_LEN,					5006				);
	nOut += sprintf (OBuffer + nOut, "%0*d",	MSG_ERROR_CODE_LEN,			MAC_OK				);
	nOut += sprintf (OBuffer + nOut, "%0*d",	TPharmNum_len,				v_PharmNum			);
	nOut += sprintf (OBuffer + nOut, "%0*d",	TInstituteCode_len,			v_InstituteCode		);
	nOut += sprintf (OBuffer + nOut, "%0*d",	TTerminalNum_len,			v_TerminalNum		);
	nOut += sprintf (OBuffer + nOut, "%0*d",	TErrorCode_len,				v_ErrorCode			);
	nOut += sprintf (OBuffer + nOut, "%0*d",	TMemberIdentification_len,	v_MemberID			);
	nOut += sprintf (OBuffer + nOut, "%0*d",	TIdentificationCode_len,	v_IDCode			);
	nOut += sprintf (OBuffer + nOut, "%0*d",	TRecipeIdentifier_len,		v_RecipeIdentifier	);
	nOut += sprintf (OBuffer + nOut, "%0*d",	TDate_len,					v_Date				);
	nOut += sprintf (OBuffer + nOut, "%0*d",	THour_len,					v_Hour				);


	// Return the size in Bytes of respond message
	*p_outputWritten			= nOut;
	*output_type_flg			= ANSWER_IN_BUFFER;
	ssmd_data_ptr->error_code	= v_ErrorCode;
	ssmd_data_ptr->terminal_num	= v_TerminalNum;

	RESTORE_ISOLATION;

	return RC_SUCCESS;
}
/* End of spooled-mode 5005 handler */



/*========================================================================
||																		||
||	GENERIC_HandlerToMsg_2033_6033										||
||	Electronic Prescriptions: Pharmacy Ishur Request					|| 
||																		||
||	Generic - works in "normal" or spool mode							||
||																		||
 =======================================================================*/

int GENERIC_HandlerToMsg_2033_6033 (	int			TrnNumber_in,
										char		*IBuffer,
										int			TotalInputLen,
										char		*OBuffer,
										int			*p_outputWritten,
										int			*output_type_flg,
										SSMD_DATA	*ssmd_data_ptr,
										short		commfail_flag,
										short		fromSpool)
{
	// Local variables.    
	int				nOut;
	int				reStart;
	int				DrugStartDate;
	short			dummy;
	ISOLATION_VAR;

	// SQL & message variables.
	
	// Incoming Message Header fields.
	TPharmNum					v_PharmNum;
	TInstituteCode				v_InstituteCode;
	TTerminalNum				v_TerminalNum;
	TGenericYYMM				v_DiaryMonth;
	TGenericYYYYMMDD			v_IshurDate;
	TGenericHHMMSS				v_IshurTime;
	TSpecialRecipeNumSource		v_PharmIshurSrc;
	TSpecialRecipeNum			v_PharmIshurNum;
	TMemberIdentification		v_MemberID;
	TIdentificationCode			v_IDCode;
	TAuthLevel					v_AuthLevel;
	TTypeDoctorIdentification	v_AuthIDType;
	TDoctorIdentification		v_AuthID;
	TDoctorFamilyName			v_AuthFamilyName;
	TDoctorFirstName			v_AuthFirstName;
	TSpecialRecipeNumSource		v_AS400IshurSrc;
	TSpecialRecipeNum			v_AS400IshurNum;
	TGenericText75				v_IshurFreeText;
	TNumOfDrugLinesRecs			v_NumLines;
	short						v_comm_error = commfail_flag;

	// Incoming Message Detail Line data.
	TPharmIshurRequest			DrugArray [MAX_REC_ELECTRONIC];
	TPharmIshurRequest			*ThisDrug;

	// Members table.
	char						rule_insurance_type[2];
	short						v_MemberSpecPresc;
	int							AdjustedMemberDateOfBirth;
	TMemberSex					v_MemberGender;

	// Temporary Members table.
	TGenericTZ			      	v_TempMembIdentification;
	TIdentificationCode			v_TempIdentifCode;
	TDate						v_TempMemValidUntil;

	// Drug Extension table.
	short						wait_months;
	short						price_code;
	int							fixed_price;
	TMemberSex					v_RuleGender;
	TAge						v_RuleFromAge;
	TAge						v_RuleToAge;
	short						v_extend_rule_days;
	short						v_NoPresc			= 0;	// Unless a drug_extension rule says otherwise.
	short						v_enabled_mac;
	short						v_enabled_hova;
	short						v_enabled_keva;
	short						needs_29_g			= 0;

	// Return-message and working variables.
	int							i;
	int							RowsFound;
	int							RecsUsed = 0;
	TAge						AdjustedMemberAge;
	TErrorCode					v_ErrorCode;
	TErrorCode					err;
	TDate						SysDate;
	THour						SysTime;
	TMemberParticipatingType	v_PrtCodeFound;
	PHARMACY_INFO				Phrm_info;
	short						v_LineNumber;
	short						ishur_unused			= ISHUR_UNUSED;
	short						ishur_used				= ISHUR_USED;
	short						v_treatment_category	= TREATMENT_TYPE_NOCHECK;
	short						v_in_health_pack		= 0;
	short						v_rule_in_health_pack;
	short						v_insurance_used		= BASIC_INS_USED;
	short						v_LowestPercent;
	short						v_magen_wait_months;
	short						v_keren_wait_months;
	short						v_preferred_flg;
	int							v_preferred_largo;
	short						OriginCode				= TrnNumber_in;	// DonR 13Aug2024 User Story #349368: Trn. Number is now variable.


	// Body of function.

	// Initialize variables.
	REMEMBER_ISOLATION;
	SET_ISOLATION_DIRTY;	// DonR 07Jul2016: Added because I saw a lock error reading member info.
    PosInBuff	= IBuffer;
	SysDate		= GetDate ();
	SysTime		= GetTime ();


	// Read incoming message into variables - Header first.
	v_PharmNum		= GetLong	(&PosInBuff, TPharmNum_len					); CHECK_ERROR ();	//  10,  7
	v_InstituteCode	= GetShort	(&PosInBuff, TInstituteCode_len				); CHECK_ERROR ();	//  17,  2
	v_TerminalNum	= GetShort	(&PosInBuff, TTerminalNum_len				); CHECK_ERROR ();	//  19,  2

	// DonR 13Aug2024 User Story #349368: Trn. 6033 includes a
	// Transaction Version Number to control the Largo Code length.
	if (TrnNumber_in == 6033)
	{
		VersionNumber	=	GetShort (&PosInBuff,	2);	CHECK_ERROR();
	}
	else
	{
		VersionNumber	=	1;
	}

	// DonR 13Aug2024 User Story #349368: Set Largo Code
	// length conditionally on Transaction Version Number.
	VersionLargoLen = (VersionNumber > 1) ? 9 : 5;

	v_DiaryMonth	= GetShort	(&PosInBuff, TGenericYYMM_len				); CHECK_ERROR ();	//  21,  4
	v_IshurDate		= GetLong	(&PosInBuff, TGenericYYYYMMDD_len			); CHECK_ERROR ();	//  25,  8
	v_IshurTime		= GetLong	(&PosInBuff, TGenericHHMMSS_len				); CHECK_ERROR ();	//  33,  6
	v_PharmIshurSrc	= GetShort	(&PosInBuff, TSpecialRecipeNumSource_len	); CHECK_ERROR ();	//  39,  1
	v_PharmIshurNum	= GetLong	(&PosInBuff, TSpecialRecipeNum_len			); CHECK_ERROR ();	//  40,  8
	v_MemberID		= GetLong	(&PosInBuff, TGenericTZ_len					); CHECK_ERROR ();	//  48,  9
	v_IDCode		= GetShort	(&PosInBuff, TIdentificationCode_len		); CHECK_ERROR ();	//  57,  1
	v_AuthLevel		= GetShort	(&PosInBuff, TAuthLevel_len					); CHECK_ERROR ();	//  58,  2
	v_AuthIDType	= GetShort	(&PosInBuff, TTypeDoctorIdentification_len	); CHECK_ERROR ();	//  60,  1
	v_AuthID		= GetLong	(&PosInBuff, TGenericTZ_len					); CHECK_ERROR ();	//  61,  9

	GetString					(&PosInBuff, v_AuthFamilyName,	TDoctorFamilyName_len	); CHECK_ERROR ();	//  70, 14
	GetString					(&PosInBuff, v_AuthFirstName,	TDoctorFirstName_len	); CHECK_ERROR ();	//  84,  8

	v_AS400IshurSrc	= GetShort	(&PosInBuff, TSpecialRecipeNumSource_len	); CHECK_ERROR ();	//  92,  1
	v_AS400IshurNum	= GetLong	(&PosInBuff, TSpecialRecipeNum_len			); CHECK_ERROR ();	//  93,  8

	GetString					(&PosInBuff, v_IshurFreeText,	75						); CHECK_ERROR ();	// 101, 75

    v_NumLines		= GetShort	(&PosInBuff, TElectPR_LineNumLen			); CHECK_ERROR ();	// 176, 2


	// Read Detail Lines. Note that for now (at least) we don't do anything with the
	// first field, Detail Line Number.
	// Each drug line is 34 bytes long.
	for (i = 0; i < v_NumLines; i++)
	{
		dummy								= GetShort	(&PosInBuff, TElectPR_LineNumLen			); CHECK_ERROR ();	// 178,  2

		// DonR 13Aug2024 User Story #349368: Largo Code Length is now variable, based on
		// Transaction Version Number. Offsets reflect the new field length of 9 digits.
		DrugArray[i].v_DrugCode				= GetLong	(&PosInBuff, VersionLargoLen				); CHECK_ERROR ();	// 180,  9

		DrugArray[i].v_PrtCodeRequested		= GetShort	(&PosInBuff, TMemberParticipatingType_len	); CHECK_ERROR ();	// 189,  4
		DrugArray[i].v_PrtAmtRequested		= GetLong	(&PosInBuff, TTotalMemberParticipation_len	); CHECK_ERROR ();	// 193,  9
		DrugArray[i].v_RuleNum				= GetLong	(&PosInBuff, TRuleNum_len					); CHECK_ERROR ();	// 202,  9
		DrugArray[i].v_Profession			= GetLong	(&PosInBuff, TElectPR_Profession_len		); CHECK_ERROR ();	// 211,  5

		// As long as we're looping, initialize the detail line's status and other data.
		DrugArray[i].v_Status				= 0;	// Assume no error.
		DrugArray[i].v_PackageSize			= 0;	// Default in case DB has NULL value.
		DrugArray[i].v_ExtendAs400IshurDays	= 0;	// Default in case this ever becomes a variable.
		DrugArray[i].EconomypriGroup		= 0;	// Default.
		DrugArray[i].SpecialistDrug			= 0;	// Default.
		DrugArray[i].PrefSpecialistDrug		= 0;	// Default.

		DrugArray[i].v_PreferredDrug		= DrugArray[i].v_DrugCode;	// Assume sold drug is the preferred drug.
	}

	// Check that message was correct length.
    ChckInputLen (TotalInputLen - (PosInBuff - IBuffer)); CHECK_ERROR ();

	// Check validity of Date and Time.
	ChckDate (v_IshurDate); CHECK_ERROR ();
	ChckTime (v_IshurTime); CHECK_ERROR ();

	// Assign values to ssmd structure.
	ssmd_data_ptr->pharmacy_num		= v_PharmNum;
	ssmd_data_ptr->member_id		= v_MemberID;
	ssmd_data_ptr->member_id_ext	= v_IDCode;


	// Initialize variables and begin SQL Retries loop.
	reStart		= MAC_TRUE;
	v_ErrorCode	= NO_ERROR;

	if (v_NumLines < 1)
	{
		SetErrorVar (&v_ErrorCode, ERR_FILE_TOO_SHORT);
		reStart = MAC_FALS;	// Skip all the SQL stuff if the incoming message wasn't OK.
	}


	for (tries = 0; (tries < SQL_UPDATE_RETRIES) && (reStart == MAC_TRUE); tries++)
	{
		// (Re-)initialize response variables.
		reStart = MAC_FALS;


		// Test pharmacy data.
		err = IS_PHARMACY_OPEN (v_PharmNum, v_InstituteCode, &Phrm_info);
		SetErrorVar (&v_ErrorCode, err);

		if ((v_ErrorCode == ERR_PHARMACY_NOT_FOUND)	||
			((v_ErrorCode	== ERR_PHARMACY_NOT_OPEN) && (fromSpool == 0)))
		{
			// No point in restarting - just jump out of the loop!
			break;
		}
		else
		{
			// Reset v_ErrorCode if we hit something that we don't really care about.
			v_ErrorCode = NO_ERROR;
		}

		
		// Dummy loop to avoid goto. Exiting this loop will send reply to Pharmacy.
		do
		{
			// Read and test Member information.
		    if (v_MemberID < 58)	// Yulia 27.11.2000
			{
				v_ErrorCode = ERR_MEMBER_ID_CODE_WRONG;
				break;
			}

			// Test temporary members.
		    if (v_IDCode == 7)
		    {
				ExecSQL (	MAIN_DB, READ_tempmemb,
							&v_TempIdentifCode,			&v_TempMembIdentification,	&v_TempMemValidUntil,
							&v_MemberID,				END_OF_ARG_LIST										);

				Conflict_Test (reStart);

				if (SQLERR_code_cmp (SQLERR_not_found) == MAC_TRUE)
				{
				    v_ErrorCode = ERR_MEMBER_ID_CODE_WRONG;
					break;
				}

				if (SQLERR_error_test ())
				{
				    v_ErrorCode = ERR_DATABASE_ERROR;
				    break;
				}

				if (v_TempMemValidUntil >= SysDate)
				{
				    v_IDCode   = v_TempIdentifCode;
				    v_MemberID = v_TempMembIdentification;
				}
			}	// Temporary Member test.

			// DonR 29Aug2021: Add force_100_percent_ptn to list of columns read. Note that
			// we should really replace a lot of "simple" variable with elements of the Member
			// structure, just to conform to what's being done in more recent code.
			ExecSQL (	MAIN_DB, TR2033_READ_members,
						&Member.maccabi_code,			&Member.maccabi_until,
						&Member.mac_magen_code,			&Member.mac_magen_from,		&Member.mac_magen_until,
						&Member.keren_mac_code,			&Member.keren_mac_from,		&Member.keren_mac_until,
						&Member.keren_wait_flag,		&v_MemberSpecPresc,			&Member.date_of_bearth,
						&v_MemberGender,				&Member.insurance_type,		&Member.carry_over_vetek,
						&Member.yahalom_code,			&Member.yahalom_from,		&Member.yahalom_until,
						&Member.force_100_percent_ptn,	&Member.died_in_hospital,

						&v_MemberID,					&v_IDCode,					END_OF_ARG_LIST			);

			Conflict_Test (reStart);
			
			// DonR 04Dec2022 User Story #408077: Use a new function to set some Member flags and values.
			SetMemberFlags (SQLCODE);
			
			if (SQLERR_code_cmp (SQLERR_not_found) == MAC_TRUE)
			{
				v_ErrorCode = ERR_MEMBER_ID_CODE_WRONG;
				break;
			}
			
			if (SQLERR_error_test ())
			{
				v_ErrorCode = ERR_DATABASE_ERROR;
				break;
			}

			// DonR 29Aug2021: If this person is a Darkonai-plus "member" who automatically pays
			// 100% for everything, pharmacy ishurim are not allowed - reject the transaction
			// with a new error code.
			if (Member.force_100_percent_ptn)
			{
				v_ErrorCode = NO_PHARM_ISHUR_FOR_DARKONAI_PLUS;
				break;
			}


			// Check Member eligibility.
//			// DonR 25Nov2021 User Story #206812: Get died-in-hospital indicator from the Ishpuz system. (NIU)
			// DonR 30Nov2022 User Story #408077: Use new macros defined in MsgHndlr.h to determine
			// eligibility.
			if ((MEMBER_INELIGIBLE) || (MEMBER_IN_MEUHEDET))
//				(Member.died_in_hospital)	||
			{
				v_ErrorCode = ERR_MEMBER_NOT_ELEGEBLE;
				break;
			}


			// Check if this Pharmacy Ishur already exists with any used lines. If so,
			// return an error.
			ExecSQL (	MAIN_DB, TR2033_READ_check_if_ishur_already_used,
						&RecsUsed,
						&v_PharmNum,		&v_PharmIshurNum,
						&v_DiaryMonth,		&v_MemberID,
						&ishur_used,		END_OF_ARG_LIST						);

			Conflict_Test (reStart);
			
			if (SQLERR_error_test ())
			{
				v_ErrorCode = ERR_DATABASE_ERROR;
				break;
			}

			// DonR 22Oct2003: If the pharmacy sends a Pharmacy Ishur Number of zero, reject it.
			// (Note that Iris may want a separate error code, which would require a fancier fix.)
			if ((RecsUsed > 0) || (v_PharmIshurNum == 0))
			{
				v_ErrorCode = ERR_PHARM_ISHUR_ALREADY_USED;
				break;
			}

			// Delete any pre-existing version of this ishur.
			ExecSQL (	MAIN_DB, TR2033_DEL_previous_ishur_version,
						&v_PharmNum,		&v_PharmIshurNum,
						&v_DiaryMonth,		&v_MemberID,
						END_OF_ARG_LIST									);

			// DonR 05Jul2005: Try eliminating (or reducing) table-locked errors.
			SET_ISOLATION_DIRTY;

			// Loop through drugs in request.
			for (i = 0; i < v_NumLines; i++)
			{
				// Check that the drug exists in the database.
				ExecSQL (	MAIN_DB, TR2033_READ_drug_list,
							&DrugArray[i].v_PackageSize,		&v_magen_wait_months,
							&v_keren_wait_months,				&DrugArray[i].EconomypriGroup,
							&DrugArray[i].SpecialistDrug,		&v_preferred_flg,
							&v_preferred_largo,
							&DrugArray[i].v_DrugCode,			&drug_deleted,
							&Phrm_info.pharm_category,			&pharmacy_assuta,
							&assuta_drug_active,				&assuta_drug_past,
							END_OF_ARG_LIST															);

				Conflict_Test (reStart);

				if (SQLERR_code_cmp (SQLERR_not_found) == MAC_TRUE)
				{
					v_ErrorCode = ERR_DRUG_CODE_NOT_FOUND;

					// DonR 17Jun2020: Don't log a "missing Largo Code" error unless the Largo Code is non-zero.
					if (DrugArray[i].v_DrugCode > 0)
						GerrLogMini (GerrId, "Trn. %d: Largo Code %d not found in drug_list.", TrnNumber_in, DrugArray[i].v_DrugCode);

					break;	// This is a fatal error for the whole transaction.
				}
				else
				{
					if (SQLERR_error_test ())
					{
						v_ErrorCode = ERR_DATABASE_ERROR;
						break;
					}
				}

				// DonR 12Aug2003: If by some chance package_size is zero, change it to
				// 1 to avoid division-by-zero error.
				if (DrugArray[i].v_PackageSize < 1)
				{
					DrugArray[i].v_PackageSize = 1;
				}


				// DonR 21Dec2003: Check if this drug is non-preferred, and if so see what the preferred
				// (generic) drug is. This drug's Largo Code will be stored in the Pharmacy Ishur table
				// and used for rule/speciality checking, so that if the non-preferred drug is being sold
				// for a valid reason, the member will be treated as if s/he had bought the preferred drug.
				find_preferred_drug (DrugArray[i].v_DrugCode,
									 DrugArray[i].EconomypriGroup,
									 v_preferred_flg,
									 v_preferred_largo,
									 0,			// No need to check purchase history for alternate preferred drugs.
									 0,			// Allow drugs with Preferred Status = 3 to be sold without requiring an explanation from pharmacy.
									 0,			// Doctor ID
									 0,			// Member ID
									 0,			// Member ID Code
									 &DrugArray[i].v_PreferredDrug,
									 &DrugArray[i].PrefSpecialistDrug,
									 NULL,		// Parent Group Code pointer
									 NULL,		// Preferred Drug Member Price pointer
									 NULL,		// DispenseAsWritten flag pointer
									 NULL,		// In-health-basket pointer
									 NULL,		// Rule-status pointer
									 NULL,		// DL structure pointer
									 &err);
				SetErrorVar (&v_ErrorCode, err);

				// Validate each drug's requested Participation based on its associated Rule Number.
				switch (DrugArray[i].v_RuleNum)
				{
//					// DonR 07Jul2013: As part of "Bakara Kamutit" enhancement, disabled extension of AS/400 ishurim.
					case LETTER_IN_HAND:
					case TREATMENT_COMMITTEE:
//					case EXPIRED_AS400_ISHUR:	// DonR 07Jul2013. IS IT CORRECT THAT THIS IS REMARKED OUT???

						// Since this feature is no longer in use, just give an error and continue.
						DrugArray[i].v_Status = ERR_RULE_NOT_FOUND_WARNING;
						break;


					// DonR 10Dec2009: "Treat as specialist prescription" request is now checked
					// only to confirm that either the sold drug or its generic equivalent is
					// a specialist drug.
					case NON_MAC_SPECIALIST:
					case MAC_SPECIALIST_NOTE:
						// DonR 16Jan2005: If neither the drug nor its "preferred" equivalent are
						// specialist drugs, don't bother checking spclty_largo_prcnt. This is
						// just to save time and ease the burden on the network and database.
						// DonR 10Dec2009: Since we're not determining the actual specialist
						// participation here, we need to check the spclty_largo_prnct table
						// to make sure that specialist participation is available to this member
						// based on his/her insurance status.
						RowsFound = 0;
						ThisDrug = &DrugArray[i];

						if ((ThisDrug->SpecialistDrug != 0) || (ThisDrug->PrefSpecialistDrug != 0))
						{
							ExecSQL (	MAIN_DB, TR2033_READ_check_spclty_largo_prcnt,
										&RowsFound,

										&ThisDrug->v_DrugCode,			&ThisDrug->v_PreferredDrug,
										&ThisDrug->v_PreferredDrug,		&Member.MemberMaccabi,
										&Member.MemberHova,				&Member.MemberKeva,
										&Member.insurance_type,			&Member.current_vetek,
										&Member.prev_insurance_type,	&Member.prev_vetek,
										END_OF_ARG_LIST													);

							if (SQLERR_error_test ())
							{
								v_ErrorCode = ERR_DATABASE_ERROR;
								RowsFound = 0;	// Just to be sure SQL didn't put something funky there!
							}
						}	// Bought drug or generic equivalent is a specialist drug.

						if (RowsFound < 1)
						{
							ThisDrug->v_Status = ERR_PHARM_ISHUR_NO_SLP_FOUND;
						}

						// DonR 10Dec2009: Instead of finding the optimal specialist participation in
						// Transaction 2033/6033, we will simply record the Speciality Code sent by
						// the pharmacy and use the pharmacy ishur to authorize specialist participation
						// in test_mac_doctor_drugs_electronic(), called by Transaction 2003. To indicate
						// that the pharmacy has not specified a speciality, we will store a -1 in the
						// ishur's Speciality Code field.
						if (ThisDrug->v_Profession == 0)
							ThisDrug->v_Profession = -1;

						break;

//					// DonR 07Jul2013: As part of "Bakara Kamutit" enhancement, disabled extension of AS/400 ishurim.
					case EXPIRED_AS400_ISHUR:

						RowsFound = 0;

						// DonR 17Jan2006: Don't allow extension if abrupt-close flag is set.
						//
						// DonR 11Nov2009: Add a new condition to prevent pharmacy from extending ishurim issued
						// on the basis of Magen Zahav insurance. Also, combine the nominal-drug and preferred-drug
						// criteria into a single SELECT statement, and provide a new error code to indicate when
						// a recently-expired ishur exists but cannot be extended by the pharmacy. (This also means
						// applying the test for automatic ishurim - source = 5 - here.)
						// DonR 12Sep2012: Ishurim with Member Price Code of zero are invalid - ignore them!
						ExecSQL (	MAIN_DB, TR2033_READ_check_extendable_as400_ishur,
									&RowsFound,
									&v_MemberID,				&v_IDCode,
									&DrugArray[i].v_DrugCode,	&DrugArray[i].v_PreferredDrug,
									END_OF_ARG_LIST													);

						Conflict_Test (reStart);

						if (RowsFound < 1)
						{
							// DonR 11Nov2009: See if there is at least one expired ishur that pharmacy doesn't
							// have the ability to extend; if so, give a different error.
							ExecSQL (	MAIN_DB, TR2033_READ_check_nonextendable_as400_ishur,
										&RowsFound,
										&v_MemberID,				&v_IDCode,
										&DrugArray[i].v_DrugCode,	&DrugArray[i].v_PreferredDrug,
										END_OF_ARG_LIST													);
							
							Conflict_Test (reStart);

							if (RowsFound > 0)
							{
								DrugArray[i].v_Status = ERR_PHARM_CANT_SALE_SPCL_PRES;
							}
							else
							{
								DrugArray[i].v_Status = ERR_NO_ISHUR_TO_EXTEND;
							}
						}

						else
						// We found an ishur to extend.
						{
							DrugArray[i].v_ExtendAs400IshurDays	= MAX_DAYS_EXPIRED;
						}

						break;
	

					// Default: look up rule in drug_extension table and validate accordingly.
					default:

						// Clear fixed price - in case Yarpa mistakenly puts something there.
						DrugArray[i].v_PrtAmtRequested = 0;

						// DonR 25Apr2023 User Story #432608: If this pharmacy is not authorized to use
						// pharmacy "nohalim", report a transaction-level error.
						if (!PHARM_NOHAL_ENABLED)
						{
							v_ErrorCode = NOT_AUTHORIZED_FOR_PHARM_NOHALIM;
							break;	// This is a fatal error for the whole transaction.
						}

						// First, see if there is such a rule.
						// DonR 11Aug2005: Per Iris Shaya, change the confirm_authority select
						// to examine only rules for low-level authority (less than 25).
						// DonR 04Dec2011: Read in enabled flags so we can give a better error message if the rule
						// exists but isn't for the right member type.
						// DonR 06Dec2012: Use a single SELECT for regular and preferred generic drug.
						// DonR 27Apr2025 User Story #390071: Make the maximum confirm_authority value
						// for pharmacy ishurim a sysparams parameter.
						ExecSQL (	MAIN_DB, TR2033_READ_drug_extension,
									&v_treatment_category,			&v_rule_in_health_pack,			&wait_months,
									&rule_insurance_type,			&price_code,					&fixed_price,
									&v_RuleFromAge,					&v_RuleToAge,					&needs_29_g,
									&v_enabled_mac,					&v_enabled_hova,				&v_enabled_keva,
									&v_RuleGender,					&v_NoPresc,						&v_extend_rule_days,

									&DrugArray[i].v_DrugCode,		&DrugArray[i].v_PreferredDrug,	&DrugArray[i].v_RuleNum,
									&Phrm_info.pharmacy_permission,	&Phrm_info.pharmacy_permission,	&SysDate,
									&v_AuthLevel,					&MaxPharmIshurConfirmAuthority,	END_OF_ARG_LIST				);

						Conflict_Test (reStart);

						if (SQLERR_code_cmp (SQLERR_not_found) == MAC_TRUE)
						{
							DrugArray[i].v_Status = ERR_PHARM_ISHUR_RULE_NOT_FOUND;
						}
						else
						{	// Something other than not-found.
							if (SQLERR_error_test ())
							{
								v_ErrorCode = ERR_DATABASE_ERROR;
							}	// Error other than not-found.

							// We found the rule!
							else
							{
								// DonR 04Dec2011: If the rule exists but is for the wrong type of member,
								// give a "rule doesn't fit" error message. Since this is the logical equivalent
								// of a "not found" error, skip the rest of this block of code if this is the case.
								// DonR 20Dec2011: Added additional conditions to give a "rule doesn't fit" error
								// when there is no match based on insurance type. Previously we didn't give any
								// message in this case - we just treated it as a rule giving 100% participation.
								// DonR 09Dec2012 "Yahalom": New insurance-type and "vetek" criteria. */
								if ((((v_enabled_mac		== 0) || (Member.MemberMaccabi	== 0))	&&
									 ((v_enabled_hova		== 0) || (Member.MemberHova		== 0))	&&
									 ((v_enabled_keva		== 0) || (Member.MemberKeva		== 0)))				||

									( (*rule_insurance_type != 'B')																		&&
									 ((*rule_insurance_type != Member.InsuranceType)		|| (Member.current_vetek	< wait_months))	&&
									 ((*rule_insurance_type != Member.PrevInsuranceType)	|| (Member.prev_vetek		< wait_months))))
								{
									DrugArray[i].v_Status = ERR_PHARM_ISHUR_RULE_NOT_FIT;
								}

								else
								{
									// DonR 21Feb2005: We now have an optional "age tolerance" in Drug
									// Extension, to allow for continuing treatment for a period of time
									// beyond the normal expiration of the member's eligibility.
									if ((v_RuleToAge < Member.age_months) && (v_extend_rule_days > 0))
									{
										AdjustedMemberDateOfBirth = AddDays (Member.date_of_bearth,
																			 (0 - v_extend_rule_days));

										// Compute Member Age in months.
										// DonR 28Dec2015: Change age logic to be more intuitive. A newborn baby's age
										// in months will now be zero until s/he is in fact a month old; in all cases,
										// Member Age will be the number of *completed* months of life. GenMonsDif()
										// already returns the correct value - so all we have to do is stop adding 1 to it.
										AdjustedMemberAge = GetMonsDif (SysDate, AdjustedMemberDateOfBirth);

										// Set a special warning message for "extended" rules.
										DrugArray[i].v_Status = ERR_PHARM_ISHUR_RULE_EXTENDED;
									}
									else
									{
										AdjustedMemberAge = Member.age_months;
									}

									// DonR 22Mar2004: If member doesn't qualify for this rule, give a warning
									// message.
									if ((v_RuleFromAge	>  Member.age_months)	||
										(v_RuleToAge	<  AdjustedMemberAge)	||
										((v_RuleGender	!= v_MemberGender)	&& (v_RuleGender != 0)))
									{
										DrugArray[i].v_Status = ERR_PHARM_ISHUR_RULE_NOT_FIT;
									}

									else
									{
										// Rule really is there and really is applicable to this member.
										DrugArray[i].v_PrtCodeRequested = price_code;

										if ((price_code == 1) && (fixed_price > 0))
										{
											DrugArray[i].v_PrtAmtRequested  = fixed_price; 
										}

										// DonR 29Nov2007: Per Iris Shaya, the health-basket status
										// from drug_extension unconditionally overrides the default
										// health-basket status read from drug_list.
										v_in_health_pack = v_rule_in_health_pack;

										switch (*rule_insurance_type)
										{
											case 'B':	v_insurance_used = BASIC_INS_USED;		break;
											case 'K':	v_insurance_used = KESEF_INS_USED;		break;
											case 'Z':	v_insurance_used = ZAHAV_INS_USED;		break;
											case 'Y':	v_insurance_used = YAHALOM_INS_USED;	break;
											default:	v_insurance_used = NO_INS_USED; // Shouldn't ever happen.
										}

									}	// Rule is applicable for member.
								}	// Rule is for right member type.
							}	// Rule was found.
						}	// Rule was found OR another DB error occurred.

						break;

				}	// Switch on Rule Number.


				// If we've hit a major error (i.e. within the switch), break out of the loop.
				if (SetErrorVar (&v_ErrorCode, v_ErrorCode))
					break;


				// Write a row to pharmacy_ishur for this drug.
				v_LineNumber = i + 1;

				ExecSQL (	MAIN_DB, TR2033_INS_pharmacy_ishur,
							&v_PharmNum,						&v_InstituteCode,
							&v_TerminalNum,						&v_DiaryMonth,
							&v_IshurDate,						&v_IshurTime,
							&v_PharmIshurSrc,					&v_PharmIshurNum,
							&v_MemberID,						&v_IDCode,
							&v_AuthLevel,						&v_AuthIDType,
							&v_AuthID,							&v_AuthFirstName,
							&v_AuthFamilyName,					&v_AS400IshurSrc,
							&v_AS400IshurNum,					&DrugArray[i].v_ExtendAs400IshurDays,
							&v_LineNumber,						&DrugArray[i].v_DrugCode,
							&DrugArray[i].v_PreferredDrug,		&DrugArray[i].v_PrtCodeRequested,
							&DrugArray[i].v_PrtAmtRequested,	&DrugArray[i].v_RuleNum,
							&DrugArray[i].v_Profession,			&v_treatment_category,
							&v_in_health_pack,					&v_IshurFreeText,
							&DrugArray[i].v_Status,				&ishur_unused,
							&IntZero,							&ShortZero,
							&SysDate,							&SysTime,
							&OriginCode,						&NOT_REPORTED_TO_AS400,
							&v_comm_error,						&v_insurance_used,
							&v_NoPresc,							&needs_29_g,
							END_OF_ARG_LIST																	);

				Conflict_Test (reStart);

				// DonR 17Jul2024 (minor) bug fix: Use the SQLERR_code_cmp() function instead of looking
				// at the old Informix error codes to detect a duplicate row on INSERT.
//				if ((SQLCODE == INF_NOT_UNIQUE_INDEX)		||
//					(SQLCODE == INF_NOT_UNIQUE_CONSTRAINT))
				if (SQLERR_code_cmp (SQLERR_not_unique) == MAC_TRUE)
				{
					v_ErrorCode = ERR_PHARM_ISHUR_DUP_LARGO;
					break;
				}

				else
				if (SQLERR_error_test ())
				{
					v_ErrorCode = ERR_DATABASE_ERROR;
					break;
				}
			
			}	// Loop through drugs in Ishur Request.
		}
		while (0);
		// End of dummy loop.


		// Commit the transaction.
		if (reStart == MAC_FALS)	// No retry needed.
		{
			if (!SetErrorVar (&v_ErrorCode, v_ErrorCode)) // Transaction OK
			{
				CommitAllWork ();
			}
			else
			{
				// Error, so do rollback.
				RollbackAllWork ();
			}

			if (SQLERR_error_test ())	// Commit (or rollback) failed.
			{
				SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR);
			}
		}
		else
		{
			// Need to retry.
			if ( TransactionRestart () != MAC_OK)
			{
				v_ErrorCode = ERR_DATABASE_ERROR;
				break;
			}

			// DonR 26Jul2011: Suppress log message until the fourth attempt fails.
			if (tries > 2)
				GerrLogReturn (GerrId, "Table is locked for the <%d> time", tries);

			sleep (ACCESS_CONFLICT_SLEEP_TIME);
		}


	}	// End of DB retries loop.

	
	if (reStart == MAC_TRUE)
	{
		GerrLogReturn (GerrId, "Locked for <%d> times", SQL_UPDATE_RETRIES);

		SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR);
	}


	
	// Write response message.
	// DonR 13Aug2024 User Story #349368: Support Transaction Number 6033 as well as 2033.
	nOut  = sprintf (OBuffer,		 "%0*d", MSG_ID_LEN,					TrnNumber_in + 1	);
	nOut +=	sprintf (OBuffer + nOut, "%0*d", MSG_ERROR_CODE_LEN,			MAC_OK				);
	nOut += sprintf (OBuffer + nOut, "%0*d", TPharmNum_len,					v_PharmNum			);
	nOut += sprintf (OBuffer + nOut, "%0*d", TInstituteCode_len,			v_InstituteCode		);
	nOut += sprintf (OBuffer + nOut, "%0*d", TTerminalNum_len,				v_TerminalNum		);
	nOut += sprintf (OBuffer + nOut, "%0*d", TErrorCode_len,				v_ErrorCode			);
	nOut += sprintf (OBuffer + nOut, "%0*d", TGenericTZ_len,				v_MemberID			);
	nOut += sprintf (OBuffer + nOut, "%0*d", TIdentificationCode_len,		v_IDCode			);
	nOut += sprintf (OBuffer + nOut, "%0*d", TSpecialRecipeNumSource_len,	v_PharmIshurSrc		);
	nOut += sprintf (OBuffer + nOut, "%0*d", TSpecialRecipeNum_len,			v_PharmIshurNum		);
	nOut += sprintf (OBuffer + nOut, "%0*d", TGenericYYYYMMDD_len,			v_IshurDate			);
	nOut += sprintf (OBuffer + nOut, "%0*d", TGenericHHMMSS_len,			v_IshurTime			);
	nOut += sprintf (OBuffer + nOut, "%0*d", TElectPR_LineNumLen,			v_NumLines			);


	// Return the size of response message.
	*p_outputWritten			= nOut;
	*output_type_flg			= ANSWER_IN_BUFFER;
	ssmd_data_ptr->error_code	= v_ErrorCode;

	RESTORE_ISOLATION;

	return RC_SUCCESS;

} /* End of 2033/6033 GENERIC handler */



/*=======================================================================
||																		||
||		  REALTIME mode of GENERIC_HandlerToMsg_2033_6033				||
||																		||
 =======================================================================*/
int   HandlerToMsg_2033_6033	(	int			TrnNumber_in,
									char		*IBuffer,
									int			TotalInputLen,
									char		*OBuffer,
									int			*p_outputWritten,
									int			*output_type_flg,
									SSMD_DATA	*ssmd_data_ptr		)
{
    return GENERIC_HandlerToMsg_2033_6033	(	TrnNumber_in,
												IBuffer,
												TotalInputLen,
												OBuffer,
												p_outputWritten,
												output_type_flg,
												ssmd_data_ptr,
												0,			/* comm_fail_flg = 0 */
												0 );		/*-> RT mode         */
}



/*=======================================================================
||																		||
||		  Spooled mode of GENERIC_HandlerToMsg_2033_6033				||
||																		||
 =======================================================================*/
int   HandlerToSpool_2033_6033	(	int			TrnNumber_in,
									char		*IBuffer,
									int			TotalInputLen,
									char		*OBuffer,
									int			*p_outputWritten,
									int			*output_type_flg,
									SSMD_DATA	*ssmd_data_ptr,
									short int	commfail_flag		)
{
    return GENERIC_HandlerToMsg_2033_6033	(	TrnNumber_in,
												IBuffer,
												TotalInputLen,
												OBuffer,
												p_outputWritten,
												output_type_flg,
												ssmd_data_ptr,
												commfail_flag,
												1 );	// Spooled mode.
}



/*===========================================================================
||																			||
||			 HandlerToMsg_2060												||
||	    Message handler for message 2060 -  how_to_take download.			||
||												(downloads whole table)		||
||																			||
===========================================================================*/
int HandlerToMsg_2060 (char			*IBuffer,
					   int			TotalInputLen,
					   char			*OBuffer,
					   int			*p_outputWritten,
					   int			*output_type_flg,
					   SSMD_DATA	*ssmd_data_ptr)
{
	// Local declarations
	int				tries;
	int				reStart;
	TErrorCode		err;
	PHARMACY_INFO	Phrm_info;
	ISOLATION_VAR;

	// SQL variables

	// INPUT MESSAGE
	TPharmNum			v_PharmNum;
	TInstituteCode		v_InstituteCode;
	TTerminalNum		v_TerminalNum;

	// RETURN MESSAGE
	// header
	TGenericYYYYMMDD	SysDate;
	TGenericHHMMSS		SysTime;
	TErrorCode			v_ErrorCode			= NO_ERROR;
	short				v_MaxNumToUpdate	= 0;
	char				NumRecsBuf	[4];
	char				*NumRecsInsertPoint	= OBuffer;

	// Database fields.
	short				v_code;
	char				v_desc	[41];

	
	// Init answer header variables
	REMEMBER_ISOLATION;
	PosInBuff			= IBuffer;
	SysDate				= GetDate ();
	SysTime				= GetTime ();


	// Read request message fields.
	v_PharmNum			= GetLong	(&PosInBuff, TPharmNum_len			); CHECK_ERROR();
	v_InstituteCode		= GetShort	(&PosInBuff, TInstituteCode_len		); CHECK_ERROR();
	v_TerminalNum		= GetShort	(&PosInBuff, TTerminalNum_len		); CHECK_ERROR();

	// Get amount of input not eaten
	ChckInputLen (TotalInputLen - (PosInBuff - IBuffer));
	CHECK_ERROR ();


	// DonR 14Jul2005: Avoid unnecessary lock errors by downgrading "isolation"
	// for this transaction.
	SET_ISOLATION_DIRTY;

	// Retries loop.
	for (tries = 0, reStart = MAC_TRUE, v_ErrorCode = NO_ERROR;	(tries < SQL_UPDATE_RETRIES) && (reStart == MAC_TRUE); tries++)
	{
		reStart = MAC_FALS;

		// One-time loop to avoid GOTO's. Response is sent upon exit from this "loop".
		do
		{
			// Test if pharmacy has been OPENed.
			err = IS_PHARMACY_OPEN (v_PharmNum, v_InstituteCode, &Phrm_info);
			SetErrorVar (&v_ErrorCode, err);
			if (v_ErrorCode != MAC_OK)
			{
				break;
			}

			// Open database cursor.
			DeclareAndOpenCursorInto (	MAIN_DB, TR2060_how_to_take_cur,
										&v_code, &v_desc, END_OF_ARG_LIST	);

			if (SQLERR_error_test ())
			{
				SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR);
				break;
			}

			// Create the header with data so far.
			nOut =  sprintf (OBuffer,		 "%0*d",	MSG_ID_LEN,				2061			);
			nOut += sprintf (OBuffer + nOut, "%0*d",	MSG_ERROR_CODE_LEN,		MAC_OK			);
			nOut += sprintf (OBuffer + nOut, "%0*d",	TPharmNum_len,			v_PharmNum		);
			nOut += sprintf (OBuffer + nOut, "%0*d",	TInstituteCode_len,		v_InstituteCode	);
			nOut += sprintf (OBuffer + nOut, "%0*d",	TTerminalNum_len,		v_TerminalNum	);
			nOut += sprintf (OBuffer + nOut, "%0*d",	TErrorCode_len,			v_ErrorCode		);
			nOut += sprintf (OBuffer + nOut, "%0*d",	TGenericYYYYMMDD_len,	SysDate			);
			nOut += sprintf (OBuffer + nOut, "%0*d",	TGenericHHMMSS_len,		SysTime			);

			// Save location to write number-of-records value, since it will be updated at the end.
			NumRecsInsertPoint = OBuffer + nOut;

			nOut += sprintf (OBuffer + nOut, "%0*d",	2,						v_MaxNumToUpdate);


			// Write the data to file - loop until we hit end-of-fetch.
			for (;;)
			{
				// Fetch and test DB errors
				FetchCursor (	MAIN_DB, TR2060_how_to_take_cur	);

				Conflict_Test (reStart);

				if (SQLERR_code_cmp (SQLERR_end_of_fetch) == MAC_TRUE)
				{
					break;
				}
				else
				{
					if (SQLERR_error_test ())
					{
						SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR);
						break;
					}
				}

				// Write the how_to_take row we fetched.
				v_MaxNumToUpdate++;

//				WinHebToDosHeb ((unsigned char *) v_desc);

				nOut += sprintf (OBuffer + nOut, "%0*d",	 3,		v_code		);
				nOut += sprintf (OBuffer + nOut, "%-*.*s",	40, 40,	v_desc		);

			}	// DB fetch loop.

			CloseCursor (	MAIN_DB, TR2060_how_to_take_cur	);

			SQLERR_error_test ();
		}
		while (0);	// One-time loop to avoid GOTO's.

		// If we've hit a DB error, pause briefly and retry.
		if (reStart != MAC_FALS)	// We've hit a DB error.
		{
			// DonR 26Jul2011: Suppress log message until the fourth attempt fails.
			if (tries > 2)
				GerrLogReturn (GerrId, "Table is locked for the <%d> time", tries);

			sleep (ACCESS_CONFLICT_SLEEP_TIME);
		}	// Pause before retrying.
	}	// End of retries loop.


	// Update record counter in header section of buffer.
	sprintf (NumRecsBuf, "%0*d", 2, v_MaxNumToUpdate);
	memcpy (NumRecsInsertPoint, NumRecsBuf, 2);

	// Return the size in bytes of response message.
	*p_outputWritten	= nOut;
	*output_type_flg	= ANSWER_IN_BUFFER;

	ssmd_data_ptr->error_code	= v_ErrorCode;
	ssmd_data_ptr->pharmacy_num	= v_PharmNum;
	ssmd_data_ptr->terminal_num	= v_TerminalNum;

	RESTORE_ISOLATION;

	return (RC_SUCCESS);
}
// End of 2060 handler.



/*===========================================================================
||																			||
||			 HandlerToMsg_2062												||
||	    Message handler for message 2062 -  drug_shape download.			||
||												(downloads whole table)		||
||																			||
===========================================================================*/
int HandlerToMsg_2062 (char			*IBuffer,
					   int			TotalInputLen,
					   char			*OBuffer,
					   int			*p_outputWritten,
					   int			*output_type_flg,
					   SSMD_DATA	*ssmd_data_ptr)
{
	// Local declarations
	int				tries;
	int				reStart;
	TErrorCode		err;
	PHARMACY_INFO	Phrm_info;
	ISOLATION_VAR;

	// SQL variables

	// INPUT MESSAGE
	TPharmNum			v_PharmNum;
	TInstituteCode		v_InstituteCode;
	TTerminalNum		v_TerminalNum;

	// RETURN MESSAGE
	// header
	TGenericYYYYMMDD	SysDate;
	TGenericHHMMSS		SysTime;
	TErrorCode			v_ErrorCode			= NO_ERROR;
	int					v_MaxNumToUpdate	= 0;
	char				NumRecsBuf	[4];
	char				*NumRecsInsertPoint	= OBuffer;

	// Database fields.
	short				v_code;
	short				ShapeCodeLen;
	char				v_desc	[5];

	
	// Init answer header variables
	REMEMBER_ISOLATION;
	PosInBuff			= IBuffer;
	SysDate				= GetDate ();
	SysTime				= GetTime ();


	// Read request message fields.
	v_PharmNum			= GetLong	(&PosInBuff, TPharmNum_len			); CHECK_ERROR();	// 10, 7
	v_InstituteCode		= GetShort	(&PosInBuff, TInstituteCode_len		); CHECK_ERROR();	// 17, 2
	v_TerminalNum		= GetShort	(&PosInBuff, TTerminalNum_len		); CHECK_ERROR();	// 19, 2

	// DonR 22Nov2020 CR #30978: If the pharmacy has sent a Version Number, read it from the
	// input message; but if we don't have at least 2 characters left to read, assume that
	// the pharmacy is still using Version 1 of the transaction, which doesn't have a Version
	// Number included in the request.
	if ((TotalInputLen - (PosInBuff - IBuffer)) > 1)
	{
		VersionNumber	= GetShort	(&PosInBuff,  2					); CHECK_ERROR ();		// 21, 2
	}
	else
	{
		VersionNumber	= 1;
	}

	// Get amount of input not eaten
	ChckInputLen (TotalInputLen - (PosInBuff - IBuffer));
	CHECK_ERROR ();

	// DonR 22Nov2020 CR #30978: For Version Number > 1, send 3 bytes of Shape Code rather than 2.
	ShapeCodeLen = (VersionNumber > 1) ? 3 : 2;


	// DonR 14Jul2005: Avoid unnecessary lock errors by downgrading "isolation"
	// for this transaction.
	SET_ISOLATION_DIRTY;

	// Retries loop.
	for (tries = 0, reStart = MAC_TRUE, v_ErrorCode = NO_ERROR;	(tries < SQL_UPDATE_RETRIES) && (reStart == MAC_TRUE); tries++)
	{
		reStart = MAC_FALS;

		// One-time loop to avoid GOTO's. Response is sent upon exit from this "loop".
		do
		{
			// Test if pharmacy has been OPENed.
			err = IS_PHARMACY_OPEN (v_PharmNum, v_InstituteCode, &Phrm_info);
			SetErrorVar (&v_ErrorCode, err);
			if (v_ErrorCode != MAC_OK)
			{
				break;
			}

			// Open database cursor.
			DeclareAndOpenCursorInto (	MAIN_DB, TR2062_drug_shape_cur,
										&v_code, &v_desc, END_OF_ARG_LIST	);

			if (SQLERR_error_test ())
			{
				SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR);
				break;
			}

			// Create the header with data so far.
			nOut =  sprintf (OBuffer,		 "%0*d",	MSG_ID_LEN,				2063			);
			nOut += sprintf (OBuffer + nOut, "%0*d",	MSG_ERROR_CODE_LEN,		MAC_OK			);
			nOut += sprintf (OBuffer + nOut, "%0*d",	TPharmNum_len,			v_PharmNum		);
			nOut += sprintf (OBuffer + nOut, "%0*d",	TInstituteCode_len,		v_InstituteCode	);
			nOut += sprintf (OBuffer + nOut, "%0*d",	TTerminalNum_len,		v_TerminalNum	);
			nOut += sprintf (OBuffer + nOut, "%0*d",	TErrorCode_len,			v_ErrorCode		);
			nOut += sprintf (OBuffer + nOut, "%0*d",	TGenericYYYYMMDD_len,	SysDate			);
			nOut += sprintf (OBuffer + nOut, "%0*d",	TGenericHHMMSS_len,		SysTime			);

			// Save location to write number-of-records value, since it will be updated at the end.
			NumRecsInsertPoint = OBuffer + nOut;

			// DonR 19Oct2021: For Version Number > 1, the number of rows sent needs to be Length 3!
			// We can use ShapeCodeLen for this, since Shape Code is a numeric field and thus the
			// number of rows sent can't be greater than 999.
			nOut += sprintf (OBuffer + nOut, "%0*d",	ShapeCodeLen,	v_MaxNumToUpdate);


			// Write the data to file - loop until we hit end-of-fetch.
			for (;;)
			{
				// Fetch and test DB errors
				FetchCursor (	MAIN_DB, TR2062_drug_shape_cur	);

				Conflict_Test (reStart);

				if (SQLERR_code_cmp (SQLERR_end_of_fetch) == MAC_TRUE)
				{
					break;
				}
				else
				{
					if (SQLERR_error_test ())
					{
						SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR);
						break;
					}
				}

				// Write the how_to_take row we fetched.
				v_MaxNumToUpdate++;

//				WinHebToDosHeb ((unsigned char *) v_desc);

				nOut += sprintf (OBuffer + nOut, "%0*d",	 ShapeCodeLen,			v_code		);	// DonR 22Nov2020 CR #30978: Variable output length.
				nOut += sprintf (OBuffer + nOut, "%-*.*s",	 4,				  4,	v_desc		);

				// DonR 31Dec2020: If Version Number > 1, add a "reserved" field.
				if (VersionNumber > 1)
				{
					nOut += sprintf (OBuffer + nOut, "%-*.*s",	 20,				  20,	""	);
				}
			}	// DB fetch loop.

			CloseCursor (	MAIN_DB, TR2062_drug_shape_cur	);

			SQLERR_error_test ();
		}
		while (0);	// One-time loop to avoid GOTO's.

		// If we've hit a DB error, pause briefly and retry.
		if (reStart != MAC_FALS)	// We've hit a DB error.
		{
			// DonR 26Jul2011: Suppress log message until the fourth attempt fails.
			if (tries > 2)
				GerrLogReturn (GerrId, "Table is locked for the <%d> time", tries);

			sleep (ACCESS_CONFLICT_SLEEP_TIME);
		}	// Pause before retrying.
	}	// End of retries loop.


	// Update record counter in header section of buffer.
	// DonR 19Oct2021: For Version Number > 1, the number of rows sent needs to be Length 3!
	// We can use ShapeCodeLen for this, since Shape Code is a numeric field and thus the
	// number of rows sent can't be greater than 999.
	sprintf (NumRecsBuf, "%0*d", ShapeCodeLen, v_MaxNumToUpdate);
	memcpy (NumRecsInsertPoint, NumRecsBuf, ShapeCodeLen);

	// Return the size in bytes of response message.
	*p_outputWritten	= nOut;
	*output_type_flg	= ANSWER_IN_BUFFER;

	ssmd_data_ptr->error_code	= v_ErrorCode;
	ssmd_data_ptr->pharmacy_num	= v_PharmNum;
	ssmd_data_ptr->terminal_num	= v_TerminalNum;

	RESTORE_ISOLATION;

	return (RC_SUCCESS);
}
// End of 2062 handler.



/*===========================================================================
||																			||
||			 HandlerToMsg_2064												||
||	    Message handler for message 2064 -  unit_of_measure download.		||
||												(downloads whole table)		||
||																			||
===========================================================================*/
int HandlerToMsg_2064 (char			*IBuffer,
					   int			TotalInputLen,
					   char			*OBuffer,
					   int			*p_outputWritten,
					   int			*output_type_flg,
					   SSMD_DATA	*ssmd_data_ptr)
{
	// Local declarations
	int				tries;
	int				reStart;
	TErrorCode		err;
	PHARMACY_INFO	Phrm_info;
	ISOLATION_VAR;

	// SQL variables

	// INPUT MESSAGE
	TPharmNum			v_PharmNum;
	TInstituteCode		v_InstituteCode;
	TTerminalNum		v_TerminalNum;

	// RETURN MESSAGE
	// header
	TGenericYYYYMMDD	SysDate;
	TGenericHHMMSS		SysTime;
	TErrorCode			v_ErrorCode			= NO_ERROR;
	int					v_MaxNumToUpdate	= 0;
	char				NumRecsBuf	[4];
	char				*NumRecsInsertPoint	= OBuffer;

	// Database fields.
	char				v_abbrev	[4];
	char				v_short_eng	[9];
	char				v_short_heb	[9];

	
	// Init answer header variables
	REMEMBER_ISOLATION;
	PosInBuff			= IBuffer;
	SysDate				= GetDate ();
	SysTime				= GetTime ();


	// Read request message fields.
	v_PharmNum			= GetLong	(&PosInBuff, TPharmNum_len			); CHECK_ERROR();
	v_InstituteCode		= GetShort	(&PosInBuff, TInstituteCode_len		); CHECK_ERROR();
	v_TerminalNum		= GetShort	(&PosInBuff, TTerminalNum_len		); CHECK_ERROR();

	// Get amount of input not eaten
	ChckInputLen (TotalInputLen - (PosInBuff - IBuffer));
	CHECK_ERROR ();


	// DonR 14Jul2005: Avoid unnecessary lock errors by downgrading "isolation"
	// for this transaction.
	SET_ISOLATION_DIRTY;

	// Retries loop.
	for (tries = 0, reStart = MAC_TRUE, v_ErrorCode = NO_ERROR;	(tries < SQL_UPDATE_RETRIES) && (reStart == MAC_TRUE); tries++)
	{
		reStart = MAC_FALS;

		// One-time loop to avoid GOTO's. Response is sent upon exit from this "loop".
		do
		{
			// Test if pharmacy has been OPENed.
			err = IS_PHARMACY_OPEN (v_PharmNum, v_InstituteCode, &Phrm_info);
			SetErrorVar (&v_ErrorCode, err);
			if (v_ErrorCode != MAC_OK)
			{
				break;
			}

			// Open database cursor.
			// DonR 28Jun2017 CR #12222: Added a new send_to_pharmacy flag to the unit_of_measure table;
			// for now at least, only one unit ("F") is disabled.
			DeclareAndOpenCursorInto (	MAIN_DB, TR2064_unit_of_measure_cur,
										&v_abbrev,	&v_short_eng,	&v_short_heb,	END_OF_ARG_LIST	);

			if (SQLERR_error_test ())
			{
				SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR);
				break;
			}

			// Create the header with data so far.
			nOut =  sprintf (OBuffer,		 "%0*d",	MSG_ID_LEN,				2065			);
			nOut += sprintf (OBuffer + nOut, "%0*d",	MSG_ERROR_CODE_LEN,		MAC_OK			);
			nOut += sprintf (OBuffer + nOut, "%0*d",	TPharmNum_len,			v_PharmNum		);
			nOut += sprintf (OBuffer + nOut, "%0*d",	TInstituteCode_len,		v_InstituteCode	);
			nOut += sprintf (OBuffer + nOut, "%0*d",	TTerminalNum_len,		v_TerminalNum	);
			nOut += sprintf (OBuffer + nOut, "%0*d",	TErrorCode_len,			v_ErrorCode		);
			nOut += sprintf (OBuffer + nOut, "%0*d",	TGenericYYYYMMDD_len,	SysDate			);
			nOut += sprintf (OBuffer + nOut, "%0*d",	TGenericHHMMSS_len,		SysTime			);

			// Save location to write number-of-records value, since it will be updated at the end.
			NumRecsInsertPoint = OBuffer + nOut;

			nOut += sprintf (OBuffer + nOut, "%0*d",	TElectPR_LineNumLen,	v_MaxNumToUpdate);


			// Write the data to file - loop until we hit end-of-fetch.
			for (;;)
			{
				// Fetch and test DB errors
				FetchCursor (	MAIN_DB, TR2064_unit_of_measure_cur	);

				Conflict_Test (reStart);

				if (SQLERR_code_cmp (SQLERR_end_of_fetch) == MAC_TRUE)
				{
					break;
				}
				else
				{
					if (SQLERR_error_test ())
					{
						SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR);
						break;
					}
				}

				// Write the how_to_take row we fetched.
				v_MaxNumToUpdate++;

				nOut += sprintf (OBuffer + nOut, "%-*.*s",	 3,  3,	v_abbrev	);
				nOut += sprintf (OBuffer + nOut, "%-*.*s",	 8,  8,	v_short_eng	);
				nOut += sprintf (OBuffer + nOut, "%-*.*s",	 8,  8,	v_short_heb	);

			}	// DB fetch loop.

			CloseCursor (	MAIN_DB, TR2064_unit_of_measure_cur	);

			SQLERR_error_test ();
		}
		while (0);	// One-time loop to avoid GOTO's.

		// If we've hit a DB error, pause briefly and retry.
		if (reStart != MAC_FALS)	// We've hit a DB error.
		{
			// DonR 26Jul2011: Suppress log message until the fourth attempt fails.
			if (tries > 2)
				GerrLogReturn (GerrId, "Table is locked for the <%d> time", tries);

			sleep (ACCESS_CONFLICT_SLEEP_TIME);
		}	// Pause before retrying.
	}	// End of retries loop.


	// Update record counter in header section of buffer.
	sprintf (NumRecsBuf, "%0*d", TElectPR_LineNumLen, v_MaxNumToUpdate);
	memcpy (NumRecsInsertPoint, NumRecsBuf, TElectPR_LineNumLen);

	// Return the size in bytes of response message.
	*p_outputWritten	= nOut;
	*output_type_flg	= ANSWER_IN_BUFFER;

	ssmd_data_ptr->error_code	= v_ErrorCode;
	ssmd_data_ptr->pharmacy_num	= v_PharmNum;
	ssmd_data_ptr->terminal_num	= v_TerminalNum;

	RESTORE_ISOLATION;

	return (RC_SUCCESS);
}
// End of 2064 handler.



/*===========================================================================
||																			||
||			 HandlerToMsg_2066												||
||	    Message handler for message 2066 -  reason_not_sold download.		||
||												(downloads whole table)		||
||																			||
===========================================================================*/
int HandlerToMsg_2066 (char			*IBuffer,
					   int			TotalInputLen,
					   char			*OBuffer,
					   int			*p_outputWritten,
					   int			*output_type_flg,
					   SSMD_DATA	*ssmd_data_ptr)
{
	// Local declarations
	int				tries;
	int				reStart;
	TErrorCode		err;
	PHARMACY_INFO	Phrm_info;
	ISOLATION_VAR;

	// SQL variables

	// INPUT MESSAGE
	TPharmNum			v_PharmNum;
	TInstituteCode		v_InstituteCode;
	TTerminalNum		v_TerminalNum;

	// RETURN MESSAGE
	// header
	TGenericYYYYMMDD	SysDate;
	TGenericHHMMSS		SysTime;
	TErrorCode			v_ErrorCode			= NO_ERROR;
	int					v_MaxNumToUpdate	= 0;
	char				NumRecsBuf	[4];
	char				*NumRecsInsertPoint	= OBuffer;

	// Database fields.
	short				v_code;
	char				v_desc	[51];

	
	// Init answer header variables
	REMEMBER_ISOLATION;
	PosInBuff			= IBuffer;
	SysDate				= GetDate ();
	SysTime				= GetTime ();


	// Read request message fields.
	v_PharmNum			= GetLong	(&PosInBuff, TPharmNum_len			); CHECK_ERROR();
	v_InstituteCode		= GetShort	(&PosInBuff, TInstituteCode_len		); CHECK_ERROR();
	v_TerminalNum		= GetShort	(&PosInBuff, TTerminalNum_len		); CHECK_ERROR();

	// Get amount of input not eaten
	ChckInputLen (TotalInputLen - (PosInBuff - IBuffer));
	CHECK_ERROR ();


	// DonR 14Jul2005: Avoid unnecessary lock errors by downgrading "isolation"
	// for this transaction.
	SET_ISOLATION_DIRTY;

	// Retries loop.
	for (tries = 0, reStart = MAC_TRUE, v_ErrorCode = NO_ERROR;	(tries < SQL_UPDATE_RETRIES) && (reStart == MAC_TRUE); tries++)
	{
		reStart = MAC_FALS;

		// One-time loop to avoid GOTO's. Response is sent upon exit from this "loop".
		do
		{
			// Test if pharmacy has been OPENed.
			err = IS_PHARMACY_OPEN (v_PharmNum, v_InstituteCode, &Phrm_info);
			SetErrorVar (&v_ErrorCode, err);
			if (v_ErrorCode != MAC_OK)
			{
				break;
			}

			// Open database cursor.
			DeclareAndOpenCursorInto (	MAIN_DB, TR2066_reason_not_sold_cur,
										&v_code, &v_desc, END_OF_ARG_LIST		);

			if (SQLERR_error_test ())
			{
				SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR);
				break;
			}

			// Create the header with data so far.
			nOut =  sprintf (OBuffer,		 "%0*d",	MSG_ID_LEN,				2067			);
			nOut += sprintf (OBuffer + nOut, "%0*d",	MSG_ERROR_CODE_LEN,		MAC_OK			);
			nOut += sprintf (OBuffer + nOut, "%0*d",	TPharmNum_len,			v_PharmNum		);
			nOut += sprintf (OBuffer + nOut, "%0*d",	TInstituteCode_len,		v_InstituteCode	);
			nOut += sprintf (OBuffer + nOut, "%0*d",	TTerminalNum_len,		v_TerminalNum	);
			nOut += sprintf (OBuffer + nOut, "%0*d",	TErrorCode_len,			v_ErrorCode		);
			nOut += sprintf (OBuffer + nOut, "%0*d",	TGenericYYYYMMDD_len,	SysDate			);
			nOut += sprintf (OBuffer + nOut, "%0*d",	TGenericHHMMSS_len,		SysTime			);

			// Save location to write number-of-records value, since it will be updated at the end.
			NumRecsInsertPoint = OBuffer + nOut;

			nOut += sprintf (OBuffer + nOut, "%0*d",	TElectPR_LineNumLen,	v_MaxNumToUpdate);


			// Write the data to file - loop until we hit end-of-fetch.
			for (;;)
			{
				// Fetch and test DB errors
				FetchCursor (	MAIN_DB, TR2066_reason_not_sold_cur	);

				Conflict_Test (reStart);

				if (SQLERR_code_cmp (SQLERR_end_of_fetch) == MAC_TRUE)
				{
					break;
				}
				else
				{
					if (SQLERR_error_test ())
					{
						SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR);
						break;
					}
				}

				// Write the how_to_take row we fetched.
				v_MaxNumToUpdate++;

//				WinHebToDosHeb ((unsigned char *) v_desc);

				nOut += sprintf (OBuffer + nOut, "%0*d",	 2,		v_code		);
				nOut += sprintf (OBuffer + nOut, "%-*.*s",	50, 50,	v_desc		);

			}	// DB fetch loop.

			CloseCursor (	MAIN_DB, TR2066_reason_not_sold_cur	);

			SQLERR_error_test ();
		}
		while (0);	// One-time loop to avoid GOTO's.

		// If we've hit a DB error, pause briefly and retry.
		if (reStart != MAC_FALS)	// We've hit a DB error.
		{
			// DonR 26Jul2011: Suppress log message until the fourth attempt fails.
			if (tries > 2)
				GerrLogReturn (GerrId, "Table is locked for the <%d> time", tries);

			sleep (ACCESS_CONFLICT_SLEEP_TIME);
		}	// Pause before retrying.
	}	// End of retries loop.


	// Update record counter in header section of buffer.
	sprintf (NumRecsBuf, "%0*d", TElectPR_LineNumLen, v_MaxNumToUpdate);
	memcpy (NumRecsInsertPoint, NumRecsBuf, TElectPR_LineNumLen);

	// Return the size in bytes of response message.
	*p_outputWritten	= nOut;
	*output_type_flg	= ANSWER_IN_BUFFER;

	ssmd_data_ptr->error_code	= v_ErrorCode;
	ssmd_data_ptr->pharmacy_num	= v_PharmNum;
	ssmd_data_ptr->terminal_num	= v_TerminalNum;

	RESTORE_ISOLATION;

	return (RC_SUCCESS);
}
// End of 2066 handler.



/*===========================================================================
||																			||
||			 HandlerToMsg_2068												||
||	    Message handler for message 2068 -  discount_remarks download.		||
||												(downloads whole table)		||
||																			||
===========================================================================*/
int HandlerToMsg_2068 (char			*IBuffer,
					   int			TotalInputLen,
					   char			*OBuffer,
					   int			*p_outputWritten,
					   int			*output_type_flg,
					   SSMD_DATA	*ssmd_data_ptr)
{
	// Local declarations
	int				tries;
	int				reStart;
	TErrorCode		err;
	PHARMACY_INFO	Phrm_info;
	ISOLATION_VAR;

	// SQL variables

	// INPUT MESSAGE
	TPharmNum			v_PharmNum;
	TInstituteCode		v_InstituteCode;
	TTerminalNum		v_TerminalNum;

	// RETURN MESSAGE
	// header
	TGenericYYYYMMDD	SysDate;
	TGenericHHMMSS		SysTime;
	TErrorCode			v_ErrorCode			= NO_ERROR;
	int					v_MaxNumToUpdate	= 0;
	char				NumRecsBuf	[4];
	char				*NumRecsInsertPoint	= OBuffer;

	// Database fields.
	short				v_code;
	char				v_desc	[26];


	// Init answer header variables
	REMEMBER_ISOLATION;
	PosInBuff			= IBuffer;
	SysDate				= GetDate ();
	SysTime				= GetTime ();


	// Read request message fields.
	v_PharmNum			= GetLong	(&PosInBuff, TPharmNum_len			); CHECK_ERROR();
	v_InstituteCode		= GetShort	(&PosInBuff, TInstituteCode_len		); CHECK_ERROR();
	v_TerminalNum		= GetShort	(&PosInBuff, TTerminalNum_len		); CHECK_ERROR();

	// Get amount of input not eaten
	ChckInputLen (TotalInputLen - (PosInBuff - IBuffer));
	CHECK_ERROR ();


	// DonR 14Jul2005: Avoid unnecessary lock errors by downgrading "isolation"
	// for this transaction.
	SET_ISOLATION_DIRTY;

	// Retries loop.
	for (tries = 0, reStart = MAC_TRUE, v_ErrorCode = NO_ERROR;	(tries < SQL_UPDATE_RETRIES) && (reStart == MAC_TRUE); tries++)
	{
		reStart = MAC_FALS;

		// One-time loop to avoid GOTO's. Response is sent upon exit from this "loop".
		do
		{
			// Test if pharmacy has been OPENed.
			err = IS_PHARMACY_OPEN (v_PharmNum, v_InstituteCode, &Phrm_info);
			SetErrorVar (&v_ErrorCode, err);
			if (v_ErrorCode != MAC_OK)
			{
				break;
			}

			// Open select cursor.
			DeclareAndOpenCursorInto (	MAIN_DB, TR2068_discount_remarks_cur,
										&v_code, &v_desc, END_OF_ARG_LIST		);

			if (SQLERR_error_test ())
			{
				SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR);
				break;
			}

			// Create the header with data so far.
			nOut =  sprintf (OBuffer,		 "%0*d",	MSG_ID_LEN,				2069			);
			nOut += sprintf (OBuffer + nOut, "%0*d",	MSG_ERROR_CODE_LEN,		MAC_OK			);
			nOut += sprintf (OBuffer + nOut, "%0*d",	TPharmNum_len,			v_PharmNum		);
			nOut += sprintf (OBuffer + nOut, "%0*d",	TInstituteCode_len,		v_InstituteCode	);
			nOut += sprintf (OBuffer + nOut, "%0*d",	TTerminalNum_len,		v_TerminalNum	);
			nOut += sprintf (OBuffer + nOut, "%0*d",	TErrorCode_len,			v_ErrorCode		);
			nOut += sprintf (OBuffer + nOut, "%0*d",	TGenericYYYYMMDD_len,	SysDate			);
			nOut += sprintf (OBuffer + nOut, "%0*d",	TGenericHHMMSS_len,		SysTime			);

			// Save location to write number-of-records value, since it will be updated at the end.
			NumRecsInsertPoint = OBuffer + nOut;

			nOut += sprintf (OBuffer + nOut, "%0*d",	TElectPR_LineNumLen,	v_MaxNumToUpdate);


			// Write the data to file - loop until we hit end-of-fetch.
			for (;;)
			{
				// Fetch and test DB errors
				FetchCursor (	MAIN_DB, TR2068_discount_remarks_cur	);

				Conflict_Test (reStart);

				if (SQLERR_code_cmp (SQLERR_end_of_fetch) == MAC_TRUE)
				{
					break;
				}
				else
				{
					if (SQLERR_error_test ())
					{
						SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR);
						break;
					}
				}

				// Write the how_to_take row we fetched.
				v_MaxNumToUpdate++;

//				WinHebToDosHeb ((unsigned char *) v_desc);

				nOut += sprintf (OBuffer + nOut, "%0*d",	 2,		v_code		);
				nOut += sprintf (OBuffer + nOut, "%-*.*s",	25, 25,	v_desc		);

			}	// DB fetch loop.

			CloseCursor (	MAIN_DB, TR2068_discount_remarks_cur	);

			SQLERR_error_test ();
		}
		while (0);	// One-time loop to avoid GOTO's.

		// If we've hit a DB error, pause briefly and retry.
		if (reStart != MAC_FALS)	// We've hit a DB error.
		{
			// DonR 26Jul2011: Suppress log message until the fourth attempt fails.
			if (tries > 2)
				GerrLogReturn (GerrId, "Table is locked for the <%d> time", tries);

			sleep (ACCESS_CONFLICT_SLEEP_TIME);
		}	// Pause before retrying.
	}	// End of retries loop.


	// Update record counter in header section of buffer.
	sprintf (NumRecsBuf, "%0*d", TElectPR_LineNumLen, v_MaxNumToUpdate);
	memcpy (NumRecsInsertPoint, NumRecsBuf, TElectPR_LineNumLen);

	// Return the size in bytes of response message.
	*p_outputWritten	= nOut;
	*output_type_flg	= ANSWER_IN_BUFFER;

	ssmd_data_ptr->error_code	= v_ErrorCode;
	ssmd_data_ptr->pharmacy_num	= v_PharmNum;
	ssmd_data_ptr->terminal_num	= v_TerminalNum;

	RESTORE_ISOLATION;

	return (RC_SUCCESS);
}
// End of 2068 handler.



/*=======================================================================
||																		||
||			 HandlerToMsg_2070_2092										||
||	    Message handler for message 2070/2092 -  EconomyPri update.		||
||																		||
=======================================================================*/
int HandlerToMsg_2070_2092 (int			TrNum,
							char		*IBuffer,
						    int			TotalInputLen,
							char		*OBuffer,
							int			*p_outputWritten,
							int			*output_type_flg,
							SSMD_DATA	*ssmd_data_ptr)
{
	// Local declarations
	char			fileName[256];
	int				tries;
	int				reStart;
	short			GroupCodeOutLen;
	TErrorCode		err;
	FILE			*outFP;
	ISOLATION_VAR;

	// SQL variables

	// INPUT MESSAGE
	PHARMACY_INFO					Phrm_info;
	TPharmNum						v_PharmNum;
	TInstituteCode					v_InstituteCode;
	TTerminalNum					v_TerminalNum;
	TGenericYYYYMMDD				v_LastUpdateDate;
	TGenericHHMMSS					v_LastUpdateTime;

	// RETURN MESSAGE
	// header
	TErrorCode						v_ErrorCode;
	TGenericYYYYMMDD				v_FileUpdateDate;
	TGenericHHMMSS					v_FileUpdateTime;
	int								v_MaxNumToUpdate;
	int								v_RcvDate;
	int								v_RcvTime;
	int								v_RcvSeq;

	// lines
	TDrugCode						v_DrugCode;
	TGenericCharFlag				v_DelFlag;
	TGenericFlag					v_ActionFlag;
	int								v_GroupCode;
	TGroupNbrShort					v_GroupNbr;
	TItemSeq						v_ItemSeq;
	char							TableName [33]	= "EconomyPri";	// DonR 19Aug2020: Change from char * to array, since ODBC doesn't like string constants.

	
	// Init answer header variables
	REMEMBER_ISOLATION;
	PosInBuff			= IBuffer;
	v_ErrorCode			= NO_ERROR;
	v_FileUpdateDate	= 0;
	v_FileUpdateTime	= 0;
	v_MaxNumToUpdate	= 0;
	GroupCodeOutLen		= (TrNum == 2070) ? 3 : 5;


	// Read request message fields.
	v_PharmNum			= GetLong	(&PosInBuff, TPharmNum_len			); CHECK_ERROR();
	v_InstituteCode		= GetShort	(&PosInBuff, TInstituteCode_len		); CHECK_ERROR();
	v_TerminalNum		= GetShort	(&PosInBuff, TTerminalNum_len		); CHECK_ERROR();
	v_LastUpdateDate	= GetLong	(&PosInBuff, TGenericYYYYMMDD_len	); CHECK_ERROR();
	v_LastUpdateTime	= GetLong	(&PosInBuff, TGenericHHMMSS_len		); CHECK_ERROR();

	ChckDate (v_LastUpdateDate); CHECK_ERROR();
	ChckTime (v_LastUpdateTime); CHECK_ERROR();

	GET_TRN_VERSION_IF_SENT	// DonR 13Aug2024 User Story #349368.

	// Get amount of input not eaten
	ChckInputLen (TotalInputLen - (PosInBuff - IBuffer));
	CHECK_ERROR();

	// DonR 13Aug2024 User Story #349368: Set Largo Code
	// length conditionally on Transaction Version Number.
	VersionLargoLen = (VersionNumber > 1) ? 9 : 5;

	// Get ready to process the request.
	reStart = MAC_TRUE;

	sprintf (fileName,
			 "%s/econpr_%0*d_%0*d_%0*d.snd",
			 MsgDir,
			 TPharmNum_len,
			 v_PharmNum,
			 TInstituteCode_len,
			 v_InstituteCode,
			 TTerminalNum_len,
			 v_TerminalNum);


	// Retries loop.
	for (tries = 0;	(tries < SQL_UPDATE_RETRIES) && (reStart == MAC_TRUE); tries++)
	{
		// Create new file for response message.
		outFP = fopen( fileName, "w" );

		if( outFP == NULL )
		{
			int	err = errno;

			GerrLogReturn (GerrId,
						   "\tCannot open output file '%s'\n\tError (%d) %s\n",
						   fileName,
						   err,
						   strerror(err));

			RESTORE_ISOLATION;
			return( ERR_UNABLE_OPEN_OUTPUT_FILE );
		}

		// Init answer header variables
		v_ErrorCode			= NO_ERROR;
		v_FileUpdateDate	= 0;
		v_FileUpdateTime	= 0;
		v_MaxNumToUpdate	= 0;
		reStart				= MAC_FALS;

		// DonR 14Jul2005: Avoid unnecessary lock errors by downgrading "isolation"
		// for this transaction.
		SET_ISOLATION_DIRTY;

		// One-time loop to avoid GOTO's. Response is sent upon exit from this "loop".
		do
		{
			// Test if pharmacy has been OPENed.
			err = IS_PHARMACY_OPEN (v_PharmNum, v_InstituteCode, &Phrm_info);
			SetErrorVar (&v_ErrorCode, err);
			if (v_ErrorCode != MAC_OK)
			{
				break;
			}

			// Get the modification date.
			ExecSQL (	MAIN_DB, READ_TablesUpdate,
						&v_FileUpdateDate,	&v_FileUpdateTime,
						&TableName,			END_OF_ARG_LIST		);

			Conflict_Test (reStart);

			if (SQLERR_code_cmp (SQLERR_not_found) == MAC_TRUE)
			{
				v_FileUpdateDate = 0;
				v_FileUpdateTime = 0;
			}
			else
			{
				if (SQLERR_error_test ())
				{
					if (SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR))
					{
						break;
					}
				}
			}

			// Select cursor.
			// DonR 31Jan2018: Use new send_and_use_pharm flag instead of system_code or group_code to decide what
			// to send. This allows us to locate all the fiddly table-specific logic in the "pipeline" programs
			// As400UnixServer and As400UnixClient.
			DeclareAndOpenCursor (	MAIN_DB, TR2070_economypri_cur,
									&v_LastUpdateDate,	&v_LastUpdateTime,
									&v_LastUpdateDate,	END_OF_ARG_LIST		);
			
			Conflict_Test (reStart);

			if (SQLERR_error_test ())
			{
				SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR);
				break;
			}


			// Create the header with data so far.
			fprintf (outFP, "%0*d",		MSG_ID_LEN,				TrNum + 1		);	// DonR 19Feb2015: Now a variable since there are 2 versions.
			fprintf (outFP, "%0*d",		MSG_ERROR_CODE_LEN,		MAC_OK			);
			fprintf (outFP, "%0*d",		TPharmNum_len,			v_PharmNum		);
			fprintf (outFP, "%0*d",		TInstituteCode_len,		v_InstituteCode	);
			fprintf (outFP, "%0*d",		TTerminalNum_len,		v_TerminalNum	);
			fprintf (outFP, "%0*d",		TErrorCode_len,			v_ErrorCode		);
			fprintf (outFP, "%0*d",		TGenericYYYYMMDD_len,	v_FileUpdateDate);
			fprintf (outFP, "%0*d",		TGenericHHMMSS_len,		v_FileUpdateTime);
			fprintf (outFP, "%0*d",		TEconPriNumUpdated_len,	v_MaxNumToUpdate);


			// Write the economypri data to file.
			// Loop until we hit end-of-fetch.
			for (;;)
			{
				// Fetch and test DB errors
				FetchCursorInto (	MAIN_DB, TR2070_economypri_cur,
									&v_GroupCode,	&v_GroupNbr,	&v_ItemSeq,
									&v_DrugCode,	&v_DelFlag,		&v_RcvDate,
									&v_RcvTime,		&v_RcvSeq,		END_OF_ARG_LIST	);

				Conflict_Test( reStart );

				if (SQLERR_code_cmp (SQLERR_end_of_fetch) == MAC_TRUE)
				{
					break;
				}
				else
				{
					if (SQLERR_error_test ())
					{
						SetErrorVar( &v_ErrorCode, ERR_DATABASE_ERROR );
						break;
					}
				}


				// DonR 19Feb2015: If we're in "old" mode (i.e. Trn. 2070), do not send rows with
				// Group Code > 999 to pharmacies, since they don't fit into the old format.
				if ((v_GroupCode > 999) && (TrNum == 2070))
					continue;

				// Set Action Flag based on whether this is a deletion or an insert/update.
				v_ActionFlag = ((*v_DelFlag == 'D') || (*v_DelFlag == 'd')) ? 1 : 0;


				// Write the EconomyPri row we fetched.
				v_MaxNumToUpdate++;

				fprintf (outFP, "%0*d",		VersionLargoLen,	v_DrugCode	);	// DonR 13Aug2024 User Story #349368: Variable Largo Code length.
				fprintf (outFP, "%0*d",		1,					v_ActionFlag);
				fprintf (outFP, "%0*d",		GroupCodeOutLen,	v_GroupCode	);
				fprintf (outFP, "%0*d",		3,					v_ItemSeq	);
				fprintf (outFP, "%0*d",		3,					v_GroupNbr	);

			}	// Fetch economypri loop.

			CloseCursor (	MAIN_DB, TR2070_economypri_cur	);

			SQLERR_error_test ();

		}
		while (0);	// One-time loop to avoid GOTO's.


		// DonR 12Jan2020: There's nothing to commit here, so there's no point in trying.
		// Commit the transaction.
		if (reStart == MAC_FALS)	/* Transaction OK then COMMIT	*/
		{
			// No DB updates here, so there's no point in committing anything!
		}
		else
		{
			if (TransactionRestart () != MAC_OK)
			{
				v_ErrorCode = ERR_DATABASE_ERROR;
				break;
			}

			// DonR 26Jul2011: Suppress log message until the fourth attempt fails.
			if (tries > 2)
				GerrLogReturn (GerrId, "Table is locked for the <%d> time", tries);

			sleep (ACCESS_CONFLICT_SLEEP_TIME);

			fclose (outFP);
		}

	}	/* End of retries loop */


	// REWRITE  MESSAGE HEADER.
	rewind (outFP);

	fprintf (outFP, "%0*d",		MSG_ID_LEN,				TrNum + 1		);	// DonR 19Feb2015: Now a variable since there are 2 versions.
	fprintf (outFP, "%0*d",		MSG_ERROR_CODE_LEN,		MAC_OK			);
	fprintf (outFP, "%0*d",		TPharmNum_len,			v_PharmNum		);
	fprintf (outFP, "%0*d",		TInstituteCode_len,		v_InstituteCode	);
	fprintf (outFP, "%0*d",		TTerminalNum_len,		v_TerminalNum	);
	fprintf (outFP, "%0*d",		TErrorCode_len,			v_ErrorCode		);
	fprintf (outFP, "%0*d",		TGenericYYYYMMDD_len,	v_FileUpdateDate);
	fprintf (outFP, "%0*d",		TGenericHHMMSS_len,		v_FileUpdateTime);
	fprintf (outFP, "%0*d",		TEconPriNumUpdated_len,	v_MaxNumToUpdate);

	fclose (outFP);


	// Write the name of output-file on string for T-SWITCH.
	*p_outputWritten = sprintf (OBuffer, "%s", fileName);
	*output_type_flg = ANSWER_IN_FILE;

	ssmd_data_ptr->error_code	= v_ErrorCode;
	ssmd_data_ptr->pharmacy_num	= v_PharmNum;
	ssmd_data_ptr->terminal_num	= v_TerminalNum;

	RESTORE_ISOLATION;

	return (RC_SUCCESS);
}
// End of 2070/2092 handler.



/*===========================================================================
||																			||
||			 HandlerToMsg_2072												||
||	    Message handler for message 2072 -  Prescription Sources download.	||
||												(downloads whole table)		||
||																			||
===========================================================================*/
int HandlerToMsg_2072 (char			*IBuffer,
					   int			TotalInputLen,
					   char			*OBuffer,
					   int			*p_outputWritten,
					   int			*output_type_flg,
					   SSMD_DATA	*ssmd_data_ptr)
{
	// Local declarations
	char			fileName[256];
	int				tries;
	int				reStart;
	TErrorCode		err;
	PHARMACY_INFO	Phrm_info;
	FILE			*outFP;
	ISOLATION_VAR;

	// SQL variables

	// INPUT MESSAGE
	TPharmNum						v_PharmNum;
	TInstituteCode					v_InstituteCode;
	TTerminalNum					v_TerminalNum;

	// RETURN MESSAGE
	// header
	TErrorCode						v_ErrorCode;
	TGenericYYYYMMDD				SysDate;
	TGenericHHMMSS					SysTime;
	int								v_MaxNumToUpdate;

	// Prescription Source fields.
	short int						v_pr_src_code;
	char							v_pr_src_desc[26];
	short int						v_pr_src_docid_type;
	short int						v_pr_src_doc_device;
	short int						v_pr_src_priv_pharm;

	
	// Init answer header variables
	REMEMBER_ISOLATION;
	PosInBuff			= IBuffer;
	v_ErrorCode			= NO_ERROR;
	v_MaxNumToUpdate	= 0;
	SysDate				= GetDate ();
	SysTime				= GetTime ();


	// Read request message fields.
	v_PharmNum			= GetLong	(&PosInBuff, TPharmNum_len			); CHECK_ERROR();
	v_InstituteCode		= GetShort	(&PosInBuff, TInstituteCode_len		); CHECK_ERROR();
	v_TerminalNum		= GetShort	(&PosInBuff, TTerminalNum_len		); CHECK_ERROR();

	// Get amount of input not eaten
	ChckInputLen (TotalInputLen - (PosInBuff - IBuffer));
	CHECK_ERROR();


	// Get ready to process the request.
	reStart = MAC_TRUE;

	sprintf (fileName,
			 "%s/prescr_source_%0*d_%0*d_%0*d.snd",
			 MsgDir,
			 TPharmNum_len,
			 v_PharmNum,
			 TInstituteCode_len,
			 v_InstituteCode,
			 TTerminalNum_len,
			 v_TerminalNum);


	// Retries loop.
	for (tries = 0;	(tries < SQL_UPDATE_RETRIES) && (reStart == MAC_TRUE); tries++)
	{
		// Create new file for response message.
		outFP = fopen( fileName, "w" );

		if( outFP == NULL )
		{
			int	err = errno;

			GerrLogReturn (GerrId,
						   "\tCannot open output file '%s'\n\tError (%d) %s\n",
						   fileName,
						   err,
						   strerror(err));

			RESTORE_ISOLATION;
			return( ERR_UNABLE_OPEN_OUTPUT_FILE );
		}

		// Init answer header variables
		v_ErrorCode			= NO_ERROR;
		v_MaxNumToUpdate	= 0;
		reStart				= MAC_FALS;

		// DonR 14Jul2005: Avoid unnecessary lock errors by downgrading "isolation"
		// for this transaction.
		SET_ISOLATION_DIRTY;

		// One-time loop to avoid GOTO's. Response is sent upon exit from this "loop".
		do
		{
			// Test if pharmacy has been OPENed.
			err = IS_PHARMACY_OPEN (v_PharmNum, v_InstituteCode, &Phrm_info);
			SetErrorVar (&v_ErrorCode, err);
			if( v_ErrorCode != MAC_OK )
			{
				break;
			}

			// Open Prescription Sources cursor.
			DeclareAndOpenCursor (	MAIN_DB, TR2072_prescr_source_cur	);

			if (SQLERR_error_test ())
			{
				SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR);
				break;
			}


			// Create the header with data so far.
			fprintf (outFP, "%0*d",		MSG_ID_LEN,				2073			);
			fprintf (outFP, "%0*d",		MSG_ERROR_CODE_LEN,		MAC_OK			);
			fprintf (outFP, "%0*d",		TPharmNum_len,			v_PharmNum		);
			fprintf (outFP, "%0*d",		TInstituteCode_len,		v_InstituteCode	);
			fprintf (outFP, "%0*d",		TTerminalNum_len,		v_TerminalNum	);
			fprintf (outFP, "%0*d",		TErrorCode_len,			v_ErrorCode		);
			fprintf (outFP, "%0*d",		TGenericYYYYMMDD_len,	SysDate			);
			fprintf (outFP, "%0*d",		TGenericHHMMSS_len,		SysTime			);
			fprintf (outFP, "%0*d",		TElectPR_LineNumLen,	v_MaxNumToUpdate);


			// Write the contracts data to file.
			// Loop until we hit end-of-fetch.
			for (;;)
			{
				// Fetch and test DB errors
				FetchCursorInto (	MAIN_DB, TR2072_prescr_source_cur,
									&v_pr_src_code,			&v_pr_src_desc,
									&v_pr_src_docid_type,	&v_pr_src_doc_device,
									&v_pr_src_priv_pharm,	END_OF_ARG_LIST			);

				Conflict_Test (reStart);

				if (SQLERR_code_cmp (SQLERR_end_of_fetch) == MAC_TRUE)
				{
					break;
				}
				else
				{
					if (SQLERR_error_test ())
					{
						SetErrorVar( &v_ErrorCode, ERR_DATABASE_ERROR );
						break;
					}
				}


				// Write the Pharmacy Contracts row we fetched.
				v_MaxNumToUpdate++;

				WinHebToDosHeb ((unsigned char *) v_pr_src_desc);

				fprintf (outFP, "%0*d",		 2,		v_pr_src_code		);
				fprintf (outFP, "%-*.*s",	15, 15,	v_pr_src_desc		);
				fprintf (outFP, "%0*d",		 1,		v_pr_src_docid_type	);
				fprintf (outFP, "%0*d",		 1,		v_pr_src_doc_device	);
				fprintf (outFP, "%0*d",		 1,		v_pr_src_priv_pharm	);

			}	// Fetch Prescription Sources loop.

			CloseCursor (	MAIN_DB, TR2072_prescr_source_cur	);

			SQLERR_error_test();

		}
		while (0);	// One-time loop to avoid GOTO's.


		// Commit the transaction.
		if( reStart == MAC_FALS )	/* Transaction OK then COMMIT	*/
		{
			// DonR 12Jan2020: No point in committing when there isn't anything to commit.
		}
		else
		{
			if (TransactionRestart () != MAC_OK)
			{
				v_ErrorCode = ERR_DATABASE_ERROR;
				break;
			}

			// DonR 26Jul2011: Suppress log message until the fourth attempt fails.
			if (tries > 2)
				GerrLogReturn (GerrId, "Table is locked for the <%d> time", tries);

			sleep (ACCESS_CONFLICT_SLEEP_TIME);

			fclose (outFP);
		}

	}	/* End of retries loop */


	// REWRITE  MESSAGE HEADER.
	rewind (outFP);

	fprintf (outFP, "%0*d",		MSG_ID_LEN,				2073			);
	fprintf (outFP, "%0*d",		MSG_ERROR_CODE_LEN,		MAC_OK			);
	fprintf (outFP, "%0*d",		TPharmNum_len,			v_PharmNum		);
	fprintf (outFP, "%0*d",		TInstituteCode_len,		v_InstituteCode	);
	fprintf (outFP, "%0*d",		TTerminalNum_len,		v_TerminalNum	);
	fprintf (outFP, "%0*d",		TErrorCode_len,			v_ErrorCode		);
	fprintf (outFP, "%0*d",		TGenericYYYYMMDD_len,	SysDate			);
	fprintf (outFP, "%0*d",		TGenericHHMMSS_len,		SysTime			);
	fprintf (outFP, "%0*d",		TElectPR_LineNumLen,	v_MaxNumToUpdate);

	fclose (outFP);


	// Write the name of output-file on string for T-SWITCH.
	*p_outputWritten = sprintf( OBuffer, "%s", fileName );
	*output_type_flg = ANSWER_IN_FILE;

	ssmd_data_ptr->error_code	= v_ErrorCode;
	ssmd_data_ptr->pharmacy_num	= v_PharmNum;
	ssmd_data_ptr->terminal_num	= v_TerminalNum;

	RESTORE_ISOLATION;

	return (RC_SUCCESS);
}
// End of 2072 handler.



/*=======================================================================
||																		||
||			 HandlerToMsg_2074											||
||	    Message handler for message 2074 -								||
||		  Drug_extension (simplified) update.							||
||																		||
=======================================================================*/
int HandlerToMsg_2074 (char			*IBuffer,
					   int			TotalInputLen,
					   char			*OBuffer,
					   int			*p_outputWritten,
					   int			*output_type_flg,
					   SSMD_DATA	*ssmd_data_ptr)
{
	// Local declarations
	char			fileName[256];
	int				tries;
	int				reStart;
	TErrorCode		err;
	PHARMACY_INFO	Phrm_info;
	FILE			*outFP;
	ISOLATION_VAR;

	// SQL variables

	// INPUT MESSAGE
	TPharmNum						v_PharmNum;
	TInstituteCode					v_InstituteCode;
	TTerminalNum					v_TerminalNum;
	TGenericYYYYMMDD				v_LastUpdateDate;
	TGenericHHMMSS					v_LastUpdateTime;

	// RETURN MESSAGE
	// header
	TErrorCode						v_ErrorCode;
	TGenericYYYYMMDD				v_FileUpdateDate;
	TGenericHHMMSS					v_FileUpdateTime;
	int								v_MaxNumToUpdate;
	int								v_RcvDate;
	int								v_RcvTime;
	int								v_RcvSeq;

	// lines
	TDrugCode						v_DrugCode;
	int								v_RuleNumber;
	short int						v_DelFlag;
	TGenericYYYYMMDD				v_EffDate;
	TGenericText75					v_RuleName;
	short int						v_ConfirmAuth;
	short int						v_InBasket;
	short int						v_basic_price_code;
	short int						v_zahav_price_code;
	short int						v_kesef_price_code;
	int								v_basic_fixed_price;
	int								v_kesef_fixed_price;
	short int						v_ParticipationCode;
	int								v_FixedParticipation;
	char							TableName [33]	= "Drug_extension";	// DonR 19Aug2020: Change from char * to array, since ODBC doesn't like string constants.

	
	// Init answer header variables
	REMEMBER_ISOLATION;
	PosInBuff			= IBuffer;
	v_ErrorCode			= NO_ERROR;
	v_FileUpdateDate	= 0;
	v_FileUpdateTime	= 0;
	v_MaxNumToUpdate	= 0;


	// Read request message fields.
	v_PharmNum			= GetLong	(&PosInBuff, TPharmNum_len			); CHECK_ERROR();
	v_InstituteCode		= GetShort	(&PosInBuff, TInstituteCode_len		); CHECK_ERROR();
	v_TerminalNum		= GetShort	(&PosInBuff, TTerminalNum_len		); CHECK_ERROR();
	v_LastUpdateDate	= GetLong	(&PosInBuff, TGenericYYYYMMDD_len	); CHECK_ERROR();
	v_LastUpdateTime	= GetLong	(&PosInBuff, TGenericHHMMSS_len		); CHECK_ERROR();

	// DonR 25Aug2004: If pharmacy sends date/time of 99999999/999999, select all
	// rows in table; otherwise select incrementally based on new columns
	// changed_date and changed_time.
	if (v_LastUpdateDate == 99999999)
		v_LastUpdateDate = -1;	// Select all rows.
	else
		ChckDate (v_LastUpdateDate); CHECK_ERROR();

	if (v_LastUpdateTime != 999999)
		ChckTime (v_LastUpdateTime); CHECK_ERROR();

	GET_TRN_VERSION_IF_SENT	// DonR 13Aug2024 User Story #349368.

	// Get amount of input not eaten
	ChckInputLen (TotalInputLen - (PosInBuff - IBuffer));
	CHECK_ERROR();

	// DonR 13Aug2024 User Story #349368: Set Largo Code
	// length conditionally on Transaction Version Number.
	VersionLargoLen = (VersionNumber > 1) ? 9 : 5;

	// Get ready to process the request.
	reStart = MAC_TRUE;

	sprintf (fileName,
			 "%s/drug_extn%0*d_%0*d_%0*d.snd",
			 MsgDir,
			 TPharmNum_len,
			 v_PharmNum,
			 TInstituteCode_len,
			 v_InstituteCode,
			 TTerminalNum_len,
			 v_TerminalNum);


	// Retries loop.
	for (tries = 0;	(tries < SQL_UPDATE_RETRIES) && (reStart == MAC_TRUE); tries++)
	{
		// Create new file for response message.
		outFP = fopen (fileName, "w");

		if (outFP == NULL)
		{
			int	err = errno;

			GerrLogReturn (GerrId,
						   "\tCannot open output file '%s'\n\tError (%d) %s\n",
						   fileName,
						   err,
						   strerror(err));

			RESTORE_ISOLATION;
			return (ERR_UNABLE_OPEN_OUTPUT_FILE);
		}

		// Init answer header variables
		v_ErrorCode			= NO_ERROR;
		v_FileUpdateDate	= 0;
		v_FileUpdateTime	= 0;
		v_MaxNumToUpdate	= 0;
		reStart				= MAC_FALS;

		// DonR 14Jul2005: Avoid unnecessary lock errors by downgrading "isolation"
		// for this transaction.
		SET_ISOLATION_DIRTY;

		// One-time loop to avoid GOTO's. Response is sent upon exit from this "loop".
		do
		{
			// Test if pharmacy has been OPENed.
			err = IS_PHARMACY_OPEN (v_PharmNum, v_InstituteCode, &Phrm_info);
			SetErrorVar (&v_ErrorCode, err);
			if (v_ErrorCode != MAC_OK)
			{
				break;
			}

			// Get the modification date.
			ExecSQL (	MAIN_DB, READ_TablesUpdate,
						&v_FileUpdateDate,	&v_FileUpdateTime,
						&TableName,			END_OF_ARG_LIST		);

			Conflict_Test (reStart);

			if (SQLERR_code_cmp (SQLERR_not_found) == MAC_TRUE)
			{
				v_FileUpdateDate = 0;
				v_FileUpdateTime = 0;
			}
			else
			{
				if (SQLERR_error_test ())
				{
					if (SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR))
					{
						break;
					}
				}
			}


			// Open Drug_extension cursor.
			// DonR 30Jul2023 User Story #468171: For private pharmacies, we want to send only rows
			// with no_presc_sale_flag = 0. Add one input parameter: the private-pharmacy flag.
			DeclareAndOpenCursor (	MAIN_DB, TR2074_drug_extension_cur,
									&v_LastUpdateDate,	&v_LastUpdateTime,
									&v_LastUpdateDate,	&Phrm_info.private_pharmacy,
									END_OF_ARG_LIST										);

			if (SQLERR_error_test ())
			{
				SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR);
				break;
			}


			// Create the header with data so far.
			fprintf (outFP, "%0*d",		MSG_ID_LEN,				2075			);
			fprintf (outFP, "%0*d",		MSG_ERROR_CODE_LEN,		MAC_OK			);
			fprintf (outFP, "%0*d",		TPharmNum_len,			v_PharmNum		);
			fprintf (outFP, "%0*d",		TInstituteCode_len,		v_InstituteCode	);
			fprintf (outFP, "%0*d",		TTerminalNum_len,		v_TerminalNum	);
			fprintf (outFP, "%0*d",		TErrorCode_len,			v_ErrorCode		);
			fprintf (outFP, "%0*d",		TGenericYYYYMMDD_len,	v_FileUpdateDate);
			fprintf (outFP, "%0*d",		TGenericHHMMSS_len,		v_FileUpdateTime);
			fprintf (outFP, "%0*d",		5,						v_MaxNumToUpdate);


			// Write the drugs data to file.
			// Loop until we hit end-of-fetch.
			for (;;)
			{
				// Fetch and test DB errors
				FetchCursorInto (	MAIN_DB, TR2074_drug_extension_cur,
									&v_DrugCode,			&v_RuleNumber,
									&v_EffDate,				&v_RuleName,
									&v_ConfirmAuth,			&v_basic_price_code,
									&v_kesef_price_code,	&v_zahav_price_code,
									&v_basic_fixed_price,	&v_kesef_fixed_price,
									&v_DelFlag,				&v_InBasket,
									END_OF_ARG_LIST									);

				Conflict_Test (reStart);

				if (SQLERR_code_cmp (SQLERR_end_of_fetch) == MAC_TRUE)
				{
					break;
				}
				else
				{
					if (SQLERR_error_test ())
					{
						SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR);
						break;
					}
				}

				// Set single Participation Code and Fixed Price variables.
				v_ParticipationCode =	(v_basic_price_code		> 0)	?	v_basic_price_code	:
										(v_kesef_price_code		> 0)	?	v_kesef_price_code	:
																			v_zahav_price_code;

				v_FixedParticipation =	(v_basic_fixed_price > 0) ? v_basic_fixed_price : v_kesef_fixed_price;



				// Write the Drug_extension row we fetched.
				v_MaxNumToUpdate++;

				WinHebToDosHeb ((unsigned char *) v_RuleName);

				fprintf (outFP, "%0*d",		9,						v_RuleNumber		);
				fprintf (outFP, "%0*d",		TGenericFlag_len,		v_DelFlag			);
				fprintf (outFP, "%0*d",		TGenericYYYYMMDD_len,	v_EffDate			);
				fprintf (outFP, "%0*d",		VersionLargoLen,		v_DrugCode			);	// DonR 13Aug2024 User Story #349368.
				fprintf (outFP, "%-*.*s",	75, 75,					v_RuleName			);
				fprintf (outFP, "%0*d",		2,						v_ConfirmAuth		);
				fprintf (outFP, "%0*d",		TGenericFlag_len,		v_InBasket			);
				fprintf (outFP, "%0*d",		4,						v_ParticipationCode	);
				fprintf (outFP, "%0*d",		9,						v_FixedParticipation);

			}	// Fetch drug_extension loop.

			CloseCursor (	MAIN_DB, TR2074_drug_extension_cur	);

			SQLERR_error_test ();

		}
		while (0);	// One-time loop to avoid GOTO's.


		// Commit the transaction.
		if (reStart == MAC_FALS)	/* Transaction OK then COMMIT	*/
		{
			// DonR 12Jan2020: No point in a commit when there's nothing to commit!
		}
		else
		{
			if (TransactionRestart () != MAC_OK)
			{
				v_ErrorCode = ERR_DATABASE_ERROR;
				break;
			}

			// DonR 26Jul2011: Suppress log message until the fourth attempt fails.
			if (tries > 2)
				GerrLogReturn (GerrId, "Table is locked for the <%d> time", tries);

			sleep (ACCESS_CONFLICT_SLEEP_TIME);

			fclose (outFP);
		}

	}	/* End of retries loop */


	// REWRITE  MESSAGE HEADER.
	rewind (outFP);

	fprintf (outFP, "%0*d",		MSG_ID_LEN,				2075			);
	fprintf (outFP, "%0*d",		MSG_ERROR_CODE_LEN,		MAC_OK			);
	fprintf (outFP, "%0*d",		TPharmNum_len,			v_PharmNum		);
	fprintf (outFP, "%0*d",		TInstituteCode_len,		v_InstituteCode	);
	fprintf (outFP, "%0*d",		TTerminalNum_len,		v_TerminalNum	);
	fprintf (outFP, "%0*d",		TErrorCode_len,			v_ErrorCode		);
	fprintf (outFP, "%0*d",		TGenericYYYYMMDD_len,	v_FileUpdateDate);
	fprintf (outFP, "%0*d",		TGenericHHMMSS_len,		v_FileUpdateTime);
	fprintf (outFP, "%0*d",		5,						v_MaxNumToUpdate);

	fclose (outFP);


	// Write the name of output-file on string for T-SWITCH.
	*p_outputWritten = sprintf( OBuffer, "%s", fileName );
	*output_type_flg = ANSWER_IN_FILE;

	ssmd_data_ptr->error_code	= v_ErrorCode;
	ssmd_data_ptr->pharmacy_num	= v_PharmNum;
	ssmd_data_ptr->terminal_num	= v_TerminalNum;

	RESTORE_ISOLATION;

	return (RC_SUCCESS);
}
// End of 2074 handler.



/*===========================================================================
||																			||
||			 HandlerToMsg_2076												||
||	  Message handler for message 2076 -  Authorizing Authorities download.	||
||												(downloads whole table)		||
||																			||
===========================================================================*/
int HandlerToMsg_2076 (char			*IBuffer,
					   int			TotalInputLen,
					   char			*OBuffer,
					   int			*p_outputWritten,
					   int			*output_type_flg,
					   SSMD_DATA	*ssmd_data_ptr)
{
	// Local declarations
	char			fileName[256];
	int				tries;
	int				reStart;
	TErrorCode		err;
	PHARMACY_INFO	Phrm_info;
	FILE			*outFP;
	ISOLATION_VAR;

	// SQL variables

	// INPUT MESSAGE
	TPharmNum						v_PharmNum;
	TInstituteCode					v_InstituteCode;
	TTerminalNum					v_TerminalNum;

	// RETURN MESSAGE
	// header
	TErrorCode						v_ErrorCode;
	TGenericYYYYMMDD				SysDate;
	TGenericHHMMSS					SysTime;
	int								v_MaxNumToUpdate;

	// Confirm Authority fields.
	short int						v_authority_code;
	char							v_authority_desc[26];

	
	// Init answer header variables
	REMEMBER_ISOLATION;
	PosInBuff			= IBuffer;
	v_ErrorCode			= NO_ERROR;
	v_MaxNumToUpdate	= 0;
	SysDate				= GetDate ();
	SysTime				= GetTime ();


	// Read request message fields.
	v_PharmNum			= GetLong	(&PosInBuff, TPharmNum_len			); CHECK_ERROR();
	v_InstituteCode		= GetShort	(&PosInBuff, TInstituteCode_len		); CHECK_ERROR();
	v_TerminalNum		= GetShort	(&PosInBuff, TTerminalNum_len		); CHECK_ERROR();

	// Get amount of input not eaten
	ChckInputLen (TotalInputLen - (PosInBuff - IBuffer));
	CHECK_ERROR();


	// Get ready to process the request.
	reStart = MAC_TRUE;

	sprintf (fileName,
			 "%s/confirm_authority_%0*d_%0*d_%0*d.snd",
			 MsgDir,
			 TPharmNum_len,
			 v_PharmNum,
			 TInstituteCode_len,
			 v_InstituteCode,
			 TTerminalNum_len,
			 v_TerminalNum);


	// Retries loop.
	for (tries = 0;	(tries < SQL_UPDATE_RETRIES) && (reStart == MAC_TRUE); tries++)
	{
		// Create new file for response message.
		outFP = fopen (fileName, "w");

		if (outFP == NULL)
		{
			int	err = errno;

			GerrLogReturn (GerrId,
						   "\tCannot open output file '%s'\n\tError (%d) %s\n",
						   fileName, err, strerror (err));

			RESTORE_ISOLATION;
			return (ERR_UNABLE_OPEN_OUTPUT_FILE);
		}

		// Init answer header variables
		v_ErrorCode			= NO_ERROR;
		v_MaxNumToUpdate	= 0;
		reStart				= MAC_FALS;

		// DonR 14Jul2005: Avoid unnecessary lock errors by downgrading "isolation"
		// for this transaction.
		SET_ISOLATION_DIRTY;

		// One-time loop to avoid GOTO's. Response is sent upon exit from this "loop".
		do
		{
			// Test if pharmacy has been OPENed.
			err = IS_PHARMACY_OPEN (v_PharmNum, v_InstituteCode, &Phrm_info);
			SetErrorVar (&v_ErrorCode, err);
			if (v_ErrorCode != MAC_OK)
			{
				break;
			}

			// Open Authorizing Authority cursor.
			DeclareAndOpenCursor (	MAIN_DB, TR2076_confirm_authority_cur	);

			if (SQLERR_error_test ())
			{
				SetErrorVar( &v_ErrorCode, ERR_DATABASE_ERROR );
				break;
			}


			// Create the header with data so far.
			fprintf (outFP, "%0*d",		MSG_ID_LEN,				2077			);
			fprintf (outFP, "%0*d",		MSG_ERROR_CODE_LEN,		MAC_OK			);
			fprintf (outFP, "%0*d",		TPharmNum_len,			v_PharmNum		);
			fprintf (outFP, "%0*d",		TInstituteCode_len,		v_InstituteCode	);
			fprintf (outFP, "%0*d",		TTerminalNum_len,		v_TerminalNum	);
			fprintf (outFP, "%0*d",		TErrorCode_len,			v_ErrorCode		);
			fprintf (outFP, "%0*d",		TGenericYYYYMMDD_len,	SysDate			);
			fprintf (outFP, "%0*d",		TGenericHHMMSS_len,		SysTime			);
			fprintf (outFP, "%0*d",		TElectPR_LineNumLen,	v_MaxNumToUpdate);


			// Write the Authorizing Authority data to file.
			// Loop until we hit end-of-fetch.
			for (;;)
			{
				// Fetch and test DB errors
				FetchCursorInto (	MAIN_DB, TR2076_confirm_authority_cur,
									&v_authority_code, &v_authority_desc, END_OF_ARG_LIST	);

				Conflict_Test (reStart);

				if (SQLERR_code_cmp (SQLERR_end_of_fetch) == MAC_TRUE)
				{
					break;
				}
				else
				{
					if (SQLERR_error_test ())
					{
						SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR);
						break;
					}
				}


				// Write the Pharmacy Contracts row we fetched.
				v_MaxNumToUpdate++;
				
				WinHebToDosHeb ((unsigned char *) v_authority_desc);

				fprintf (outFP, "%0*d",		 2,		v_authority_code);
				fprintf (outFP, "%-*.*s",	15, 15,	v_authority_desc);

			}	// Fetch Authorizing Authority lines loop.

			CloseCursor (	MAIN_DB, TR2076_confirm_authority_cur	);

			SQLERR_error_test ();

		}
		while (0);	// One-time loop to avoid GOTO's.


		// Commit the transaction.
		if( reStart == MAC_FALS )	/* Transaction OK then COMMIT	*/
		{
			// DonR 12Jan2020: No point in a commit when no DB updates have been made!
		}
		else
		{
			if (TransactionRestart () != MAC_OK)
			{
				v_ErrorCode = ERR_DATABASE_ERROR;
				break;
			}

			// DonR 26Jul2011: Suppress log message until the fourth attempt fails.
			if (tries > 2)
				GerrLogReturn (GerrId, "Table is locked for the <%d> time", tries);

			sleep (ACCESS_CONFLICT_SLEEP_TIME);

			fclose (outFP);
		}

	}	/* End of retries loop */


	// REWRITE  MESSAGE HEADER.
	rewind (outFP);

	fprintf (outFP, "%0*d",		MSG_ID_LEN,				2077			);
	fprintf (outFP, "%0*d",		MSG_ERROR_CODE_LEN,		MAC_OK			);
	fprintf (outFP, "%0*d",		TPharmNum_len,			v_PharmNum		);
	fprintf (outFP, "%0*d",		TInstituteCode_len,		v_InstituteCode	);
	fprintf (outFP, "%0*d",		TTerminalNum_len,		v_TerminalNum	);
	fprintf (outFP, "%0*d",		TErrorCode_len,			v_ErrorCode		);
	fprintf (outFP, "%0*d",		TGenericYYYYMMDD_len,	SysDate			);
	fprintf (outFP, "%0*d",		TGenericHHMMSS_len,		SysTime			);
	fprintf (outFP, "%0*d",		TElectPR_LineNumLen,	v_MaxNumToUpdate);

	fclose (outFP);


	// Write the name of output-file on string for T-SWITCH.
	*p_outputWritten = sprintf (OBuffer, "%s", fileName);
	*output_type_flg = ANSWER_IN_FILE;

	ssmd_data_ptr->error_code	= v_ErrorCode;
	ssmd_data_ptr->pharmacy_num	= v_PharmNum;
	ssmd_data_ptr->terminal_num	= v_TerminalNum;

	RESTORE_ISOLATION;

	return (RC_SUCCESS);
}
// End of 2076 handler.



/*===========================================================================
||																			||
||			 HandlerToMsg_2078												||
||	 Message handler for message 2078 -  Sale (+ associated tables) update.	||
||																			||
===========================================================================*/
int HandlerToMsg_2078 (char			*IBuffer,
					   int			TotalInputLen,
					   char			*OBuffer,
					   int			*p_outputWritten,
					   int			*output_type_flg,
					   SSMD_DATA	*ssmd_data_ptr)
{
	// Local declarations
	char			fileName [256];
	int				tries;
	int				reStart;
	TErrorCode		err;
	PHARMACY_INFO	Phrm_info;
	FILE			*outFP;
	ISOLATION_VAR;

	// SQL variables

	// INPUT MESSAGE
	TPharmNum						v_PharmNum;
	TInstituteCode					v_InstituteCode;
	TTerminalNum					v_TerminalNum;
	TGenericYYYYMMDD				v_LastUpdateDate;
	TGenericHHMMSS					v_LastUpdateTime;

	// RETURN MESSAGE
	// header
	TErrorCode						v_ErrorCode;
	TGenericYYYYMMDD				SysDate;
	TGenericHHMMSS					SysTime;
	int								v_NumSaleRows		= 0;
	int								v_NumSaleFpRows		= 0;
	int								v_NumSaleBonusRows	= 0;
	int								v_NumSaleTargetRows	= 0;

	// Sale fields.
	int								v_sale_number;
	char							v_sale_name [76];
	int								v_start_date;
	int								v_end_date;
	short							v_sale_audience;
	short							v_sale_type;
	int								v_min_op_to_buy;
	int								v_op_to_receive;
	int								v_min_purchase_amt;
	short							v_purchase_disc;
	int								v_tav_knia_amt;

	// Sale_Fixed_Price, Sale_Bonus_Recv, and Sale_Target fields.
	int								v_largo_code;
	int								v_sale_price;
	short							v_discount_percent;
	short							v_pharmacy_type;
	short							v_pharmacy_size;
	int								v_target_op;

	
	// Init answer header variables
	REMEMBER_ISOLATION;
	PosInBuff			= IBuffer;
	v_ErrorCode			= NO_ERROR;
	SysDate				= GetDate ();
	SysTime				= GetTime ();


	// Read request message fields.
	v_PharmNum			= GetLong	(&PosInBuff, TPharmNum_len			); CHECK_ERROR();	// 10, 7
	v_InstituteCode		= GetShort	(&PosInBuff, TInstituteCode_len		); CHECK_ERROR();	// 17, 2
	v_TerminalNum		= GetShort	(&PosInBuff, TTerminalNum_len		); CHECK_ERROR();	// 19, 2
	v_LastUpdateDate	= GetLong	(&PosInBuff, TGenericYYYYMMDD_len	); CHECK_ERROR();	// 21, 8
	v_LastUpdateTime	= GetLong	(&PosInBuff, TGenericHHMMSS_len		); CHECK_ERROR();	// 29, 6

	// If pharmacy sends date/time of 99999999/999999, select all rows in table;
	// otherwise select incrementally based on update_date and update_time.
	if (v_LastUpdateDate == 99999999)
		v_LastUpdateDate = -1;	// Select all rows.
	else
		ChckDate (v_LastUpdateDate); CHECK_ERROR();

	if (v_LastUpdateTime != 999999)
		ChckTime (v_LastUpdateTime); CHECK_ERROR();

	GET_TRN_VERSION_IF_SENT	// DonR 13Aug2024 User Story #349368.

	// Get amount of input not eaten
	ChckInputLen (TotalInputLen - (PosInBuff - IBuffer));
	CHECK_ERROR();

	// DonR 13Aug2024 User Story #349368: Set Largo Code
	// length conditionally on Transaction Version Number.
	VersionLargoLen = (VersionNumber > 1) ? 9 : 5;

	// Get ready to process the request.
	reStart = MAC_TRUE;

	sprintf (fileName,
			 "%s/sale_tables%0*d_%0*d_%0*d.snd",
			 MsgDir,
			 TPharmNum_len,
			 v_PharmNum,
			 TInstituteCode_len,
			 v_InstituteCode,
			 TTerminalNum_len,
			 v_TerminalNum);


	// Retries loop.
	for (tries = 0;	(tries < SQL_UPDATE_RETRIES) && (reStart == MAC_TRUE); tries++)
	{
		// Create new file for response message.
		outFP = fopen (fileName, "w");

		if (outFP == NULL)
		{
			int	err = errno;

			GerrLogReturn (GerrId,
						   "\tCannot open output file '%s'\n\tError (%d) %s\n",
						   fileName,
						   err,
						   strerror(err));

			RESTORE_ISOLATION;
			return (ERR_UNABLE_OPEN_OUTPUT_FILE);
		}

		// Init answer header variables
		v_ErrorCode			= NO_ERROR;
		v_NumSaleRows		= 0;
		v_NumSaleFpRows		= 0;
		v_NumSaleBonusRows	= 0;
		v_NumSaleTargetRows	= 0;
		reStart				= MAC_FALS;

		// DonR 14Jul2005: Avoid unnecessary lock errors by downgrading "isolation"
		// for this transaction.
		SET_ISOLATION_DIRTY;

		// One-time loop to avoid GOTO's. Response is sent upon exit from this "loop".
		do
		{
			// Test if pharmacy has been opened.
			err = IS_PHARMACY_OPEN (v_PharmNum, v_InstituteCode, &Phrm_info);
			SetErrorVar (&v_ErrorCode, err);
			if (v_ErrorCode != MAC_OK)
			{
				break;
			}

			// Create the header with data so far.
			fprintf (outFP, "%0*d",		MSG_ID_LEN,				2079				);
			fprintf (outFP, "%0*d",		MSG_ERROR_CODE_LEN,		MAC_OK				);
			fprintf (outFP, "%0*d",		TPharmNum_len,			v_PharmNum			);
			fprintf (outFP, "%0*d",		TInstituteCode_len,		v_InstituteCode		);
			fprintf (outFP, "%0*d",		TTerminalNum_len,		v_TerminalNum		);
			fprintf (outFP, "%0*d",		TErrorCode_len,			v_ErrorCode			);
			fprintf (outFP, "%0*d",		TGenericYYYYMMDD_len,	SysDate				);
			fprintf (outFP, "%0*d",		TGenericHHMMSS_len,		SysTime				);
			fprintf (outFP, "%0*d",		9,						v_NumSaleRows		);
			fprintf (outFP, "%0*d",		9,						v_NumSaleFpRows		);
			fprintf (outFP, "%0*d",		9,						v_NumSaleBonusRows	);
			fprintf (outFP, "%0*d",		9,						v_NumSaleTargetRows	);


			// 1) Open Sale cursor.
			// DonR 14Jan2015: Per Iris Shaya, send information for *all* pharmacies - so v_PharmNum
			// is no longer part of the WHERE clause.
			DeclareAndOpenCursorInto (	MAIN_DB, TR2078_sale_cur,
										&v_sale_number,			&v_sale_name,			&v_start_date,
										&v_end_date,			&v_sale_audience,		&v_sale_type,
										&v_min_op_to_buy,		&v_op_to_receive,		&v_min_purchase_amt,
										&v_purchase_disc,		&v_tav_knia_amt,
										&v_LastUpdateDate,		&v_LastUpdateTime,		&v_LastUpdateDate,
										END_OF_ARG_LIST															);

			if (SQLERR_error_test ())
			{
				SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR);
				break;
			}

			// Write the Sale data to file.
			// Loop until we hit end-of-fetch.
			for (;;)
			{
				// Fetch and test DB errors
				FetchCursor (	MAIN_DB, TR2078_sale_cur	);

				Conflict_Test (reStart);

				if (SQLERR_code_cmp (SQLERR_end_of_fetch) == MAC_TRUE)
				{
					break;
				}
				else
				{
					if (SQLERR_error_test ())
					{
						SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR);
						break;
					}
				}


				// Write the Sale row we fetched.
				v_NumSaleRows++;
				
				WinHebToDosHeb ((unsigned char *) v_sale_name);

				fprintf (outFP, "%0*d",		 6,		v_sale_number		);
				fprintf (outFP, "%-*.*s",	75, 75,	v_sale_name			);
				fprintf (outFP, "%0*d",		 8,		v_start_date		);
				fprintf (outFP, "%0*d",		 8,		v_end_date			);
				fprintf (outFP, "%0*d",		 1,		v_sale_audience		);
				fprintf (outFP, "%0*d",		 2,		v_sale_type			);
				fprintf (outFP, "%0*d",		 5,		v_min_op_to_buy		);
				fprintf (outFP, "%0*d",		 5,		v_op_to_receive		);
				fprintf (outFP, "%0*d",		 9,		v_min_purchase_amt	);
				fprintf (outFP, "%0*d",		 5,		v_purchase_disc		);
				fprintf (outFP, "%0*d",		 9,		v_tav_knia_amt		);

			}	// Fetch Sale rows loop.

			CloseCursor (	MAIN_DB, TR2078_sale_cur	);

			SQLERR_error_test ();


			// 2) Open Sale_Fixed_Price cursor.
			// DonR 14Jan2015: Per Iris Shaya, send information for *all* pharmacies - so v_PharmNum
			// is no longer part of the WHERE clause.
			DeclareAndOpenCursorInto (	MAIN_DB, TR2078_sale_fixed_price_cur,
										&v_sale_number,		&v_largo_code,		&v_sale_price,
										&v_LastUpdateDate,	&v_LastUpdateTime,	&v_LastUpdateDate,
										END_OF_ARG_LIST													);

			if (SQLERR_error_test ())
			{
				SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR);
				break;
			}

			// Write the Sale_Fixed_Price data to file.
			// Loop until we hit end-of-fetch.
			for (;;)
			{
				// Fetch and test DB errors
				FetchCursor (	MAIN_DB, TR2078_sale_fixed_price_cur	);

				Conflict_Test (reStart);

				if (SQLERR_code_cmp (SQLERR_end_of_fetch) == MAC_TRUE)
				{
					break;
				}
				else
				{
					if (SQLERR_error_test ())
					{
						SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR);
						break;
					}
				}


				// Write the Sale_Fixed_Price row we fetched.
				v_NumSaleFpRows++;
				
				fprintf (outFP, "%0*d",	 6,					v_sale_number	);
				fprintf (outFP, "%0*d",	 VersionLargoLen,	v_largo_code	);	// DonR 13Aug2024 User Story #349368: Variable Largo Code length.
				fprintf (outFP, "%0*d",	 9,					v_sale_price	);

			}	// Fetch Sale_Fixed_Price rows loop.

			CloseCursor (	MAIN_DB, TR2078_sale_fixed_price_cur	);

			SQLERR_error_test ();


			// DonR 15Apr2015: TEMPORARY FIX because of a Yarpa bug: stuff from sale_bonus_recv
			// for Sale Type 14 needs to be sent as if it were coming from sale_fixed_price; so
			// we need an additional cursor for Sale Type 14.
			// DonR 29Apr2015: Temporarily disable this cursor by changing sale_type value from 14 to 9914.
			// DonR 07Jul2015: Per Iris Shay, re-enabled the 15Apr2015 fix by changing 9914 back to 14.
			DeclareAndOpenCursorInto (	MAIN_DB, TR2078_sale_bonus_recv_sale_type_14_cur,
										&v_sale_number,		&v_largo_code,		&v_sale_price,
										&v_LastUpdateDate,	&v_LastUpdateTime,	&v_LastUpdateDate,
										END_OF_ARG_LIST													);

			if (SQLERR_error_test ())
			{
				SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR);
				break;
			}

			// Write the sale_bonus_recv data to file.
			// Loop until we hit end-of-fetch.
			for (;;)
			{
				// Fetch and test DB errors
				FetchCursor (	MAIN_DB, TR2078_sale_bonus_recv_sale_type_14_cur	);

				Conflict_Test (reStart);

				if (SQLERR_code_cmp (SQLERR_end_of_fetch) == MAC_TRUE)
				{
					break;
				}
				else
				{
					if (SQLERR_error_test ())
					{
						SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR);
						break;
					}
				}


				// Write the Sale_Fixed_Price row we fetched.
				v_NumSaleFpRows++;
				
				fprintf (outFP, "%0*d",	 6,					v_sale_number	);
				fprintf (outFP, "%0*d",	 VersionLargoLen,	v_largo_code	);	// DonR 13Aug2024 User Story #349368: Variable Largo Code length.
				fprintf (outFP, "%0*d",	 9,					v_sale_price	);

			}	// Fetch Sale_Fixed_Price rows loop.

			CloseCursor (	MAIN_DB, TR2078_sale_bonus_recv_sale_type_14_cur	);

			SQLERR_error_test ();
			// DonR 15Apr2015 end.


			// 3) Open Sale_Bonus_Recv cursor.
			// DonR 14Jan2015: Per Iris Shaya, send information for *all* pharmacies - so v_PharmNum
			// is no longer part of the WHERE clause.
			// DonR 15Apr2015: TEMPORARY FIX because of a Yarpa bug: stuff from sale_bonus_recv
			// for Sale Type 14 needs to be sent as if it were coming from sale_fixed_price; so
			// we need to disable this cursor for Sale Type 14.
			// DonR 29Apr2015: Temporarily disable the temporary fix by changing sale_type value from 14 to 9914.
			// DonR 07Jul2015: Per Iris Shay, re-enabled the 15Apr2015 fix by changing 9914 back to 14.
			DeclareAndOpenCursorInto (	MAIN_DB, TR2078_sale_bonus_recv_sale_NOT_type_14_cur,
										&v_sale_number,			&v_largo_code,
										&v_discount_percent,	&v_sale_price,
										&v_LastUpdateDate,		&v_LastUpdateTime,
										&v_LastUpdateDate,		END_OF_ARG_LIST		);

			if (SQLERR_error_test ())
			{
				SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR);
				break;
			}

			// Write the Sale_Bonus_Recv data to file.
			// Loop until we hit end-of-fetch.
			for (;;)
			{
				// Fetch and test DB errors
				FetchCursor (	MAIN_DB, TR2078_sale_bonus_recv_sale_NOT_type_14_cur	);

				Conflict_Test (reStart);

				if (SQLERR_code_cmp (SQLERR_end_of_fetch) == MAC_TRUE)
				{
					break;
				}
				else
				{
					if (SQLERR_error_test ())
					{
						SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR);
						break;
					}
				}


				// Write the Sale_Bonus_Recv row we fetched.
				v_NumSaleBonusRows++;
				
				fprintf (outFP, "%0*d",	 6,					v_sale_number		);
				fprintf (outFP, "%0*d",	 VersionLargoLen,	v_largo_code		);	// DonR 13Aug2024 User Story #349368: Variable Largo Code length.
				fprintf (outFP, "%0*d",	 5,					v_discount_percent	);
				fprintf (outFP, "%0*d",	 9,					v_sale_price		);

			}	// Fetch Sale_Bonus_Recv rows loop.

			CloseCursor (	MAIN_DB, TR2078_sale_bonus_recv_sale_NOT_type_14_cur	);

			SQLERR_error_test ();


			// 4) Open Sale_Target cursor.
			// DonR 14Jan2015: Per Iris Shaya, send information for *all* pharmacies - so v_PharmNum
			// is no longer part of the WHERE clause.
			DeclareAndOpenCursorInto (	MAIN_DB, TR2078_sale_target_cur,
										&v_sale_number,			&v_pharmacy_type,
										&v_pharmacy_size,		&v_target_op,
										&v_LastUpdateDate,		&v_LastUpdateTime,
										&v_LastUpdateDate,		END_OF_ARG_LIST		);

			if (SQLERR_error_test ())
			{
				SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR);
				break;
			}

			// Write the Sale_Target data to file.
			// Loop until we hit end-of-fetch.
			for (;;)
			{
				// Fetch and test DB errors
				FetchCursor (	MAIN_DB, TR2078_sale_target_cur	);

				Conflict_Test (reStart);

				if (SQLERR_code_cmp (SQLERR_end_of_fetch) == MAC_TRUE)
				{
					break;
				}
				else
				{
					if (SQLERR_error_test ())
					{
						SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR);
						break;
					}
				}


				// Write the Sale_Target row we fetched.
				v_NumSaleTargetRows++;
				
				fprintf (outFP, "%0*d",		 6,		v_sale_number		);
				fprintf (outFP, "%0*d",		 2,		v_pharmacy_type		);
				fprintf (outFP, "%0*d",		 2,		v_pharmacy_size		);
				fprintf (outFP, "%0*d",		 9,		v_target_op			);

			}	// Fetch Sale_Target rows loop.

			CloseCursor (	MAIN_DB, TR2078_sale_target_cur	);

			SQLERR_error_test ();

		}
		while (0);	// One-time loop to avoid GOTO's.


		// Commit the transaction.
		if (reStart == MAC_FALS)	/* Transaction OK then COMMIT	*/
		{
			// DonR 13Jan2020: No point in a commit when we haven't updated anything!
		}
		else
		{
			if (TransactionRestart () != MAC_OK)
			{
				v_ErrorCode = ERR_DATABASE_ERROR;
				break;
			}

			// DonR 26Jul2011: Suppress log message until the fourth attempt fails.
			if (tries > 2)
				GerrLogReturn (GerrId, "Table is locked for the <%d> time", tries);

			sleep (ACCESS_CONFLICT_SLEEP_TIME);

			fclose (outFP);
		}

	}	/* End of retries loop */


	// REWRITE  MESSAGE HEADER.
	rewind (outFP);

	fprintf (outFP, "%0*d",		MSG_ID_LEN,				2079				);
	fprintf (outFP, "%0*d",		MSG_ERROR_CODE_LEN,		MAC_OK				);
	fprintf (outFP, "%0*d",		TPharmNum_len,			v_PharmNum			);
	fprintf (outFP, "%0*d",		TInstituteCode_len,		v_InstituteCode		);
	fprintf (outFP, "%0*d",		TTerminalNum_len,		v_TerminalNum		);
	fprintf (outFP, "%0*d",		TErrorCode_len,			v_ErrorCode			);
	fprintf (outFP, "%0*d",		TGenericYYYYMMDD_len,	SysDate				);
	fprintf (outFP, "%0*d",		TGenericHHMMSS_len,		SysTime				);
	fprintf (outFP, "%0*d",		9,						v_NumSaleRows		);
	fprintf (outFP, "%0*d",		9,						v_NumSaleFpRows		);
	fprintf (outFP, "%0*d",		9,						v_NumSaleBonusRows	);
	fprintf (outFP, "%0*d",		9,						v_NumSaleTargetRows	);

	fclose (outFP);


	// Write the name of output-file on string for T-SWITCH.
	*p_outputWritten = sprintf( OBuffer, "%s", fileName );
	*output_type_flg = ANSWER_IN_FILE;

	ssmd_data_ptr->error_code	= v_ErrorCode;
	ssmd_data_ptr->pharmacy_num	= v_PharmNum;
	ssmd_data_ptr->terminal_num	= v_TerminalNum;

	RESTORE_ISOLATION;

	return (RC_SUCCESS);
}
// End of 2078 handler.



/*===========================================================================
||																			||
||			 HandlerToMsg_2080												||
||	  Message handler for message 2080 -  Gadgets download.					||
||											(downloads whole table)			||
||																			||
===========================================================================*/
int HandlerToMsg_2080 (char			*IBuffer,
					   int			TotalInputLen,
					   char			*OBuffer,
					   int			*p_outputWritten,
					   int			*output_type_flg,
					   SSMD_DATA	*ssmd_data_ptr)
{
	// Local declarations
	char			fileName [256];
	int				tries;
	int				reStart;
	TErrorCode		err;
	PHARMACY_INFO	Phrm_info;
	FILE			*outFP;
	ISOLATION_VAR;

	// SQL variables

	// INPUT MESSAGE
	TPharmNum						v_PharmNum;
	TInstituteCode					v_InstituteCode;
	TTerminalNum					v_TerminalNum;

	// RETURN MESSAGE
	// header
	TErrorCode						v_ErrorCode;
	TGenericYYYYMMDD				SysDate;
	TGenericHHMMSS					SysTime;
	int								v_MaxNumToUpdate;

	// Gadget fields.
	int		v_gadget_code;
	int		v_largo_code;
	int		v_service_code;
	short	v_service_number;
	short	v_service_type;
	char	v_service_desc [26];

	
	// Init answer header variables
	REMEMBER_ISOLATION;
	PosInBuff			= IBuffer;
	v_ErrorCode			= NO_ERROR;
	v_MaxNumToUpdate	= 0;
	SysDate				= GetDate ();
	SysTime				= GetTime ();


	// Read request message fields.
	v_PharmNum			= GetLong	(&PosInBuff, TPharmNum_len			); CHECK_ERROR();
	v_InstituteCode		= GetShort	(&PosInBuff, TInstituteCode_len		); CHECK_ERROR();
	v_TerminalNum		= GetShort	(&PosInBuff, TTerminalNum_len		); CHECK_ERROR();

	GET_TRN_VERSION_IF_SENT	// DonR 13Aug2024 User Story #349368.

	// Get amount of input not eaten
	ChckInputLen (TotalInputLen - (PosInBuff - IBuffer));
	CHECK_ERROR ();

	// DonR 13Aug2024 User Story #349368: Set Largo Code
	// length conditionally on Transaction Version Number.
	VersionLargoLen = (VersionNumber > 1) ? 9 : 5;

	// Get ready to process the request.
	reStart = MAC_TRUE;

	sprintf (fileName,
			 "%s/gadgets_%0*d_%0*d_%0*d.snd",
			 MsgDir,
			 TPharmNum_len,
			 v_PharmNum,
			 TInstituteCode_len,
			 v_InstituteCode,
			 TTerminalNum_len,
			 v_TerminalNum);


	// Retries loop.
	for (tries = 0;	(tries < SQL_UPDATE_RETRIES) && (reStart == MAC_TRUE); tries++)
	{
		// Create new file for response message.
		outFP = fopen (fileName, "w");

		if (outFP == NULL)
		{
			int	err = errno;

			GerrLogReturn (GerrId,
						   "\tCannot open output file '%s'\n\tError (%d) %s\n",
						   fileName,
						   err,
						   strerror(err));

			RESTORE_ISOLATION;
			return (ERR_UNABLE_OPEN_OUTPUT_FILE);
		}

		// Init answer header variables
		v_ErrorCode			= NO_ERROR;
		v_MaxNumToUpdate	= 0;
		reStart				= MAC_FALS;

		// Avoid unnecessary lock errors by downgrading "isolation"
		// for this transaction.
		SET_ISOLATION_DIRTY;

		// One-time loop to avoid GOTO's. Response is sent upon exit from this "loop".
		do
		{
			// Test if pharmacy has been OPENed.
			err = IS_PHARMACY_OPEN (v_PharmNum, v_InstituteCode, &Phrm_info);
			SetErrorVar (&v_ErrorCode, err);
			if (v_ErrorCode != MAC_OK)
			{
				break;
			}

			// Open Gadgets cursor.
			DeclareAndOpenCursor (	MAIN_DB, TR2080_gadgets_cur	);

			if (SQLERR_error_test ())
			{
				SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR);
				break;
			}


			// Create the header with data so far.
			fprintf (outFP, "%0*d",		MSG_ID_LEN,				2081			);
			fprintf (outFP, "%0*d",		MSG_ERROR_CODE_LEN,		MAC_OK			);
			fprintf (outFP, "%0*d",		TPharmNum_len,			v_PharmNum		);
			fprintf (outFP, "%0*d",		TInstituteCode_len,		v_InstituteCode	);
			fprintf (outFP, "%0*d",		TTerminalNum_len,		v_TerminalNum	);
			fprintf (outFP, "%0*d",		TErrorCode_len,			v_ErrorCode		);
			fprintf (outFP, "%0*d",		TGenericYYYYMMDD_len,	SysDate			);
			fprintf (outFP, "%0*d",		TGenericHHMMSS_len,		SysTime			);
			fprintf (outFP, "%0*d",		9,						v_MaxNumToUpdate);


			// Write the Gadgets data to file.
			// Loop until we hit end-of-fetch.
			for (;;)
			{
				// Fetch and test DB errors
				FetchCursorInto (	MAIN_DB, TR2080_gadgets_cur,
									&v_gadget_code,		&v_service_desc,
									&v_largo_code,		&v_service_code,
									&v_service_number,	&v_service_type,
									END_OF_ARG_LIST							);

				Conflict_Test (reStart);

				if (SQLERR_code_cmp (SQLERR_end_of_fetch) == MAC_TRUE)
				{
					break;
				}
				else
				{
					if (SQLERR_error_test ())
					{
						SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR);
						break;
					}
				}


				// Write the Gadgets row we fetched.
				v_MaxNumToUpdate++;
				
				WinHebToDosHeb ((unsigned char *) v_service_desc);

				fprintf (outFP, "%0*d",		 VersionLargoLen,	v_largo_code);	// DonR 13Aug2024 User Story #349368: Variable Largo Code length.
				fprintf (outFP, "%0*d",		 5,					v_service_code);
				fprintf (outFP, "%0*d",		 2,					v_service_number);
				fprintf (outFP, "%0*d",		 2,					v_service_type);
				fprintf (outFP, "%-*.*s",	25, 25,				v_service_desc);
				fprintf (outFP, "%0*d",		 5,					v_gadget_code);

			}	// Fetch Gadget lines loop.

			CloseCursor (	MAIN_DB, TR2080_gadgets_cur	);

			SQLERR_error_test ();

		}
		while (0);	// One-time loop to avoid GOTO's.


		// Commit the transaction.
		if (reStart == MAC_FALS)	/* Transaction OK then COMMIT	*/
		{
			// DonR 13Jan2020: No point in a commit when we haven't updated anything!
		}
		else
		{
			if (TransactionRestart () != MAC_OK)
			{
				v_ErrorCode = ERR_DATABASE_ERROR;
				break;
			}

			// DonR 26Jul2011: Suppress log message until the fourth attempt fails.
			if (tries > 2)
				GerrLogReturn (GerrId, "Table is locked for the <%d> time", tries);

			sleep (ACCESS_CONFLICT_SLEEP_TIME);

			fclose (outFP);
		}

	}	/* End of retries loop */


	// REWRITE  MESSAGE HEADER.
	rewind (outFP);

	fprintf (outFP, "%0*d",		MSG_ID_LEN,				2081			);
	fprintf (outFP, "%0*d",		MSG_ERROR_CODE_LEN,		MAC_OK			);
	fprintf (outFP, "%0*d",		TPharmNum_len,			v_PharmNum		);
	fprintf (outFP, "%0*d",		TInstituteCode_len,		v_InstituteCode	);
	fprintf (outFP, "%0*d",		TTerminalNum_len,		v_TerminalNum	);
	fprintf (outFP, "%0*d",		TErrorCode_len,			v_ErrorCode		);
	fprintf (outFP, "%0*d",		TGenericYYYYMMDD_len,	SysDate			);
	fprintf (outFP, "%0*d",		TGenericHHMMSS_len,		SysTime			);
	fprintf (outFP, "%0*d",		9,						v_MaxNumToUpdate);

	fclose (outFP);

	// Write the name of output-file on string for T-SWITCH.
	*p_outputWritten = sprintf (OBuffer, "%s", fileName);
	*output_type_flg = ANSWER_IN_FILE;

	ssmd_data_ptr->error_code	= v_ErrorCode;
	ssmd_data_ptr->pharmacy_num	= v_PharmNum;
	ssmd_data_ptr->terminal_num	= v_TerminalNum;

	RESTORE_ISOLATION;

	return (RC_SUCCESS);
}
// End of 2080 handler.



/*=======================================================================
||																		||
||			 HandlerToMsg_2082											||
||	    Message handler for message 2082 -  PharmDrugNotes update.		||
||																		||
=======================================================================*/
int HandlerToMsg_2082 (char			*IBuffer,
					   int			TotalInputLen,
					   char			*OBuffer,
					   int			*p_outputWritten,
					   int			*output_type_flg,
					   SSMD_DATA	*ssmd_data_ptr)
{
	// Local declarations
	char			fileName [256];
	int				tries;
	int				reStart;
	TGenericFlag	v_ActionFlag;
	TErrorCode		err;
	PHARMACY_INFO	Phrm_info;
	FILE			*outFP;
	ISOLATION_VAR;

	// SQL variables

	// INPUT MESSAGE
	TPharmNum						v_PharmNum;
	TInstituteCode					v_InstituteCode;
	TTerminalNum					v_TerminalNum;
	TGenericYYYYMMDD				v_LastUpdateDate;
	TGenericHHMMSS					v_LastUpdateTime;

	// RETURN MESSAGE
	// header
	TErrorCode						v_ErrorCode;
	TGenericYYYYMMDD				v_FileUpdateDate;
	TGenericHHMMSS					v_FileUpdateTime;
	int								v_MaxNumToUpdate;

	// lines
	int								v_gnrldrugnotecode;
	TGenericCharFlag				v_del_flg;
	short							v_gdn_category;
	short							v_gdn_connect_type;
	char							v_gdn_connect_desc	[26];
	char							v_gnrldrugnotetype	[ 2];
	short							v_gdn_seq_num;
	short							v_gdn_severity;
	char							v_gnrldrugnote		[51];
	int								v_update_date;
	int								v_update_time;
	char							TableName			[33] = "PharmDrugNotes";	// DonR 19Aug2020: Change from char * to array, since ODBC doesn't like string constants.


	// Init answer header variables
	REMEMBER_ISOLATION;
	PosInBuff			= IBuffer;
	v_ErrorCode			= NO_ERROR;
	v_FileUpdateDate	= 0;
	v_FileUpdateTime	= 0;
	v_MaxNumToUpdate	= 0;


	// Read request message fields.
	v_PharmNum			= GetLong	(&PosInBuff, TPharmNum_len			); CHECK_ERROR();
	v_InstituteCode		= GetShort	(&PosInBuff, TInstituteCode_len		); CHECK_ERROR();
	v_TerminalNum		= GetShort	(&PosInBuff, TTerminalNum_len		); CHECK_ERROR();
	v_LastUpdateDate	= GetLong	(&PosInBuff, TGenericYYYYMMDD_len	); CHECK_ERROR();
	v_LastUpdateTime	= GetLong	(&PosInBuff, TGenericHHMMSS_len		); CHECK_ERROR();

	ChckDate (v_LastUpdateDate); CHECK_ERROR();
	ChckTime (v_LastUpdateTime); CHECK_ERROR();

	// Get amount of input not eaten
	ChckInputLen (TotalInputLen - (PosInBuff - IBuffer));
	CHECK_ERROR ();


	// Get ready to process the request.
	reStart = MAC_TRUE;

	sprintf (fileName,
			 "%s/gdn_%0*d_%0*d_%0*d.snd",
			 MsgDir,
			 TPharmNum_len,
			 v_PharmNum,
			 TInstituteCode_len,
			 v_InstituteCode,
			 TTerminalNum_len,
			 v_TerminalNum);


	// Retries loop.
	for (tries = 0;	(tries < SQL_UPDATE_RETRIES) && (reStart == MAC_TRUE); tries++)
	{
		// Create new file for response message.
		outFP = fopen (fileName, "w");

		if (outFP == NULL)
		{
			int	err = errno;

			GerrLogReturn (GerrId,
						   "\tCannot open output file '%s'\n\tError (%d) %s\n",
						   fileName,
						   err,
						   strerror(err));

			RESTORE_ISOLATION;
			return (ERR_UNABLE_OPEN_OUTPUT_FILE);
		}

		// Init answer header variables
		v_ErrorCode			= NO_ERROR;
		v_FileUpdateDate	= 0;
		v_FileUpdateTime	= 0;
		v_MaxNumToUpdate	= 0;
		reStart				= MAC_FALS;

		// Avoid unnecessary lock errors by downgrading "isolation"
		// for this transaction.
		SET_ISOLATION_DIRTY;

		// One-time loop to avoid GOTO's. Response is sent upon exit from this "loop".
		do
		{
			// Test if pharmacy has been OPENed.
			err = IS_PHARMACY_OPEN (v_PharmNum, v_InstituteCode, &Phrm_info);
			SetErrorVar (&v_ErrorCode, err);
			if (v_ErrorCode != MAC_OK)
			{
				break;
			}

			// Get the modification date.
			ExecSQL (	MAIN_DB, READ_TablesUpdate,
						&v_FileUpdateDate,	&v_FileUpdateTime,
						&TableName,			END_OF_ARG_LIST		);

			Conflict_Test (reStart);

			if (SQLERR_code_cmp (SQLERR_not_found) == MAC_TRUE)
			{
				v_FileUpdateDate = 0;
				v_FileUpdateTime = 0;
			}
			else
			{
				if (SQLERR_error_test ())
				{
					if (SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR))
					{
						break;
					}
				}
			}

			// Select cursor.
			DeclareAndOpenCursor (	MAIN_DB, TR2082_pharmdrugnotes_cur,
									&v_LastUpdateDate,	&v_LastUpdateTime,
									&v_LastUpdateDate,	END_OF_ARG_LIST		);

			Conflict_Test (reStart);

			if (SQLERR_error_test ())
			{
				SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR);
				break;
			}


			// Create the header with data so far.
			fprintf (outFP, "%0*d",		MSG_ID_LEN,				2083			);
			fprintf (outFP, "%0*d",		MSG_ERROR_CODE_LEN,		MAC_OK			);
			fprintf (outFP, "%0*d",		TPharmNum_len,			v_PharmNum		);
			fprintf (outFP, "%0*d",		TInstituteCode_len,		v_InstituteCode	);
			fprintf (outFP, "%0*d",		TTerminalNum_len,		v_TerminalNum	);
			fprintf (outFP, "%0*d",		TErrorCode_len,			v_ErrorCode		);
			fprintf (outFP, "%0*d",		TGenericYYYYMMDD_len,	v_FileUpdateDate);
			fprintf (outFP, "%0*d",		TGenericHHMMSS_len,		v_FileUpdateTime);
			fprintf (outFP, "%0*d",		TEconPriNumUpdated_len,	v_MaxNumToUpdate);


			// Write the PharmDrugNotes data to file.
			// Loop until we hit end-of-fetch.
			for (;;)
			{
				// Fetch and test DB errors
				FetchCursorInto (	MAIN_DB, TR2082_pharmdrugnotes_cur,
									&v_gnrldrugnotecode,	&v_del_flg,
									&v_gdn_category,		&v_gdn_connect_type,
									&v_gdn_connect_desc,	&v_gnrldrugnotetype,
									&v_gdn_seq_num,			&v_gdn_severity,
									&v_gnrldrugnote,		&v_update_date,
									&v_update_time,			END_OF_ARG_LIST			);

				Conflict_Test (reStart);

				if (SQLERR_code_cmp (SQLERR_end_of_fetch) == MAC_TRUE)
				{
					break;
				}
				else
				{
					if (SQLERR_error_test ())
					{
						SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR);
						break;
					}
				}


				// Set Action Flag based on whether this is a deletion or an insert/update.
				v_ActionFlag = ((*v_del_flg == 'D') || (*v_del_flg == 'd')) ? 1 : 0;

				// DonR 26Nov2025 User Story #251264: Support more flexible Hebrew
				// encoding output.
//				WinHebToDosHeb ((unsigned char *)(v_gdn_connect_desc));
//				WinHebToDosHeb ((unsigned char *)(v_gnrldrugnote));
				EncodeHebrew	('W', Phrm_info.hebrew_encoding, sizeof (v_gdn_connect_desc),	v_gdn_connect_desc);
				EncodeHebrew	('W', Phrm_info.hebrew_encoding, sizeof (v_gnrldrugnote),		v_gnrldrugnote);

				// Write the PharmDrugNotes row we fetched.
				v_MaxNumToUpdate++;

				fprintf (outFP, "%0*d",		5,					v_gnrldrugnotecode	);
				fprintf (outFP, "%0*d",		TGenericFlag_len,	v_ActionFlag		);
				fprintf (outFP, "%0*d",		1,					v_gdn_category		);
				fprintf (outFP, "%0*d",		3,					v_gdn_connect_type	);
				fprintf (outFP, "%-*.*s",	25, 25,				v_gdn_connect_desc	);
				fprintf (outFP, "%-*.*s",	1,  1,				v_gnrldrugnotetype	);
				fprintf (outFP, "%0*d",		2,					v_gdn_seq_num		);
				fprintf (outFP, "%0*d",		1,					v_gdn_severity		);
				fprintf (outFP, "%-*.*s",	50, 50,				v_gnrldrugnote		);

			}	// Fetch PharmDrugNotes loop.

			CloseCursor (	MAIN_DB, TR2082_pharmdrugnotes_cur	);

			SQLERR_error_test ();

		}
		while (0);	// One-time loop to avoid GOTO's.


		// Commit the transaction.
		if (reStart == MAC_FALS)	/* Transaction OK then COMMIT	*/
		{
			// DonR 13Jan2020: No need for a commit when we haven't updated anything!
		}
		else
		{
			if (TransactionRestart () != MAC_OK)
			{
				v_ErrorCode = ERR_DATABASE_ERROR;
				break;
			}

			// DonR 26Jul2011: Suppress log message until the fourth attempt fails.
			if (tries > 2)
				GerrLogReturn (GerrId, "Table is locked for the <%d> time", tries);

			sleep (ACCESS_CONFLICT_SLEEP_TIME);

			fclose (outFP);
		}

	}	/* End of retries loop */


	// REWRITE MESSAGE HEADER.
	rewind (outFP);

	fprintf (outFP, "%0*d",		MSG_ID_LEN,				2083			);
	fprintf (outFP, "%0*d",		MSG_ERROR_CODE_LEN,		MAC_OK			);
	fprintf (outFP, "%0*d",		TPharmNum_len,			v_PharmNum		);
	fprintf (outFP, "%0*d",		TInstituteCode_len,		v_InstituteCode	);
	fprintf (outFP, "%0*d",		TTerminalNum_len,		v_TerminalNum	);
	fprintf (outFP, "%0*d",		TErrorCode_len,			v_ErrorCode		);
	fprintf (outFP, "%0*d",		TGenericYYYYMMDD_len,	v_FileUpdateDate);
	fprintf (outFP, "%0*d",		TGenericHHMMSS_len,		v_FileUpdateTime);
	fprintf (outFP, "%0*d",		TEconPriNumUpdated_len,	v_MaxNumToUpdate);

	fclose (outFP);


	// Write the name of output-file on string for T-SWITCH.
	*p_outputWritten = sprintf (OBuffer, "%s", fileName);
	*output_type_flg = ANSWER_IN_FILE;

	ssmd_data_ptr->error_code	= v_ErrorCode;
	ssmd_data_ptr->pharmacy_num	= v_PharmNum;
	ssmd_data_ptr->terminal_num	= v_TerminalNum;

	RESTORE_ISOLATION;

	return (RC_SUCCESS);
}
// End of 2082 handler.



/*=======================================================================
||																		||
||			 HandlerToMsg_2084											||
||	    Message handler for message 2084 -	DrugNotes update.			||
||																		||
=======================================================================*/
int HandlerToMsg_2084 (char			*IBuffer,
					   int			TotalInputLen,
					   char			*OBuffer,
					   int			*p_outputWritten,
					   int			*output_type_flg,
					   SSMD_DATA	*ssmd_data_ptr)
{
	// Local declarations
	char			fileName [256];
	int				tries;
	int				reStart;
	TGenericFlag	v_ActionFlag;
	TErrorCode		err;
	PHARMACY_INFO	Phrm_info;
	FILE			*outFP;
	ISOLATION_VAR;

	// SQL variables

	// INPUT MESSAGE
	TPharmNum						v_PharmNum;
	TInstituteCode					v_InstituteCode;
	TTerminalNum					v_TerminalNum;
	TGenericYYYYMMDD				v_LastUpdateDate;
	TGenericHHMMSS					v_LastUpdateTime;

	// RETURN MESSAGE
	// header
	TErrorCode						v_ErrorCode;
	TGenericYYYYMMDD				v_FileUpdateDate;
	TGenericHHMMSS					v_FileUpdateTime;
	int								v_MaxNumToUpdate;

	// lines
	int								v_largo_code;
	int								v_gnrldrugnotecode;
	TGenericCharFlag				v_del_flg;
	int								v_update_date;
	int								v_update_time;
	char							TableName [33] = "DrugNotes";	// DonR 19Aug2020: Change from char * to array, since ODBC doesn't like string constants.


	// Init answer header variables
	REMEMBER_ISOLATION;
	PosInBuff			= IBuffer;
	v_ErrorCode			= NO_ERROR;
	v_FileUpdateDate	= 0;
	v_FileUpdateTime	= 0;
	v_MaxNumToUpdate	= 0;


	// Read request message fields.
	v_PharmNum			= GetLong	(&PosInBuff, TPharmNum_len			); CHECK_ERROR();
	v_InstituteCode		= GetShort	(&PosInBuff, TInstituteCode_len		); CHECK_ERROR();
	v_TerminalNum		= GetShort	(&PosInBuff, TTerminalNum_len		); CHECK_ERROR();
	v_LastUpdateDate	= GetLong	(&PosInBuff, TGenericYYYYMMDD_len	); CHECK_ERROR();
	v_LastUpdateTime	= GetLong	(&PosInBuff, TGenericHHMMSS_len		); CHECK_ERROR();

	ChckDate (v_LastUpdateDate); CHECK_ERROR();
	ChckTime (v_LastUpdateTime); CHECK_ERROR();

	GET_TRN_VERSION_IF_SENT	// DonR 13Aug2024 User Story #349368.

	// Get amount of input not eaten
	ChckInputLen (TotalInputLen - (PosInBuff - IBuffer));
	CHECK_ERROR ();

	// DonR 13Aug2024 User Story #349368: Set Largo Code
	// length conditionally on Transaction Version Number.
	VersionLargoLen = (VersionNumber > 1) ? 9 : 5;

	// Get ready to process the request.
	reStart = MAC_TRUE;

	sprintf (fileName,
			 "%s/drugnotes_%0*d_%0*d_%0*d.snd",
			 MsgDir,
			 TPharmNum_len,
			 v_PharmNum,
			 TInstituteCode_len,
			 v_InstituteCode,
			 TTerminalNum_len,
			 v_TerminalNum);


	// Retries loop.
	for (tries = 0;	(tries < SQL_UPDATE_RETRIES) && (reStart == MAC_TRUE); tries++)
	{
		// Create new file for response message.
		outFP = fopen (fileName, "w");

		if (outFP == NULL)
		{
			int	err = errno;

			GerrLogReturn (GerrId,
						   "\tCannot open output file '%s'\n\tError (%d) %s\n",
						   fileName,
						   err,
						   strerror(err));

			RESTORE_ISOLATION;
			return (ERR_UNABLE_OPEN_OUTPUT_FILE);
		}

		// Init answer header variables
		v_ErrorCode			= NO_ERROR;
		v_FileUpdateDate	= 0;
		v_FileUpdateTime	= 0;
		v_MaxNumToUpdate	= 0;
		reStart				= MAC_FALS;

		// Avoid unnecessary lock errors by downgrading "isolation"
		// for this transaction.
		SET_ISOLATION_DIRTY;

		// One-time loop to avoid GOTO's. Response is sent upon exit from this "loop".
		do
		{
			// Test if pharmacy has been OPENed.
			err = IS_PHARMACY_OPEN (v_PharmNum, v_InstituteCode, &Phrm_info);
			SetErrorVar (&v_ErrorCode, err);
			if (v_ErrorCode != MAC_OK)
			{
				break;
			}

			// Get the modification date.
			ExecSQL (	MAIN_DB, READ_TablesUpdate,
						&v_FileUpdateDate,	&v_FileUpdateTime,
						&TableName,			END_OF_ARG_LIST		);

			Conflict_Test (reStart);

			if (SQLERR_code_cmp (SQLERR_not_found) == MAC_TRUE)
			{
				v_FileUpdateDate = 0;
				v_FileUpdateTime = 0;
			}
			else
			{
				if (SQLERR_error_test ())
				{
					if (SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR))
					{
						break;
					}
				}
			}


			// Declare and open DrugNotes cursor.
			// DonR 13Jul2020: Use new combined operation to avoid splitting
			// the DECLARE and OPEN statements.
			DeclareAndOpenCursor (	MAIN_DB, TR2084_drugnotes_cur,
									&v_LastUpdateDate,	&v_LastUpdateTime,
									&v_LastUpdateDate,	END_OF_ARG_LIST		);

			if (SQLERR_error_test ())
			{
				SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR);
				break;
			}


			// Create the header with data so far.
			fprintf (outFP, "%0*d",		MSG_ID_LEN,				2085			);
			fprintf (outFP, "%0*d",		MSG_ERROR_CODE_LEN,		MAC_OK			);
			fprintf (outFP, "%0*d",		TPharmNum_len,			v_PharmNum		);
			fprintf (outFP, "%0*d",		TInstituteCode_len,		v_InstituteCode	);
			fprintf (outFP, "%0*d",		TTerminalNum_len,		v_TerminalNum	);
			fprintf (outFP, "%0*d",		TErrorCode_len,			v_ErrorCode		);
			fprintf (outFP, "%0*d",		TGenericYYYYMMDD_len,	v_FileUpdateDate);
			fprintf (outFP, "%0*d",		TGenericHHMMSS_len,		v_FileUpdateTime);
			fprintf (outFP, "%0*d",		TEconPriNumUpdated_len,	v_MaxNumToUpdate);


			// Write the DrugNotes data to file.
			// Loop until we hit end-of-fetch.
			for (;;)
			{
				// Fetch and test DB errors
				FetchCursorInto (	MAIN_DB, TR2084_drugnotes_cur,
									&v_largo_code,		&v_gnrldrugnotecode,	&v_del_flg,
									&v_update_date,		&v_update_time,			END_OF_ARG_LIST	);

				Conflict_Test (reStart);

				if (SQLERR_code_cmp (SQLERR_end_of_fetch) == MAC_TRUE)
				{
					break;
				}
				else
				{
					if (SQLERR_error_test ())
					{
						SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR);
						break;
					}
				}


				// Set Action Flag based on whether this is a deletion or an insert/update.
				v_ActionFlag = ((*v_del_flg == 'D') || (*v_del_flg == 'd')) ? 1 : 0;


				// Write the DrugNotes row we fetched.
				v_MaxNumToUpdate++;

				fprintf (outFP, "%0*d",		VersionLargoLen,	v_largo_code		);	// DonR 13Aug2024 User Story #349368: Variable Largo Code length.
				fprintf (outFP, "%0*d",		TGenericFlag_len,	v_ActionFlag		);
				fprintf (outFP, "%0*d",		5,					v_gnrldrugnotecode	);

			}	// Fetch DrugNotes loop.

			CloseCursor (	MAIN_DB, TR2084_drugnotes_cur	);

			SQLERR_error_test ();

		}
		while (0);	// One-time loop to avoid GOTO's.


		// Commit the transaction.
		if (reStart == MAC_FALS)	/* Transaction OK then COMMIT	*/
		{
			// DonR 13Jan2020: No point in a commit when we haven't modified anything!
		}
		else
		{
			if (TransactionRestart () != MAC_OK)
			{
				v_ErrorCode = ERR_DATABASE_ERROR;
				break;
			}

			// DonR 26Jul2011: Suppress log message until the fourth attempt fails.
			if (tries > 2)
				GerrLogReturn (GerrId, "Table is locked for the <%d> time", tries);

			sleep (ACCESS_CONFLICT_SLEEP_TIME);

			fclose (outFP);
		}

	}	/* End of retries loop */


	// REWRITE MESSAGE HEADER.
	rewind (outFP);

	fprintf (outFP, "%0*d",		MSG_ID_LEN,				2085			);
	fprintf (outFP, "%0*d",		MSG_ERROR_CODE_LEN,		MAC_OK			);
	fprintf (outFP, "%0*d",		TPharmNum_len,			v_PharmNum		);
	fprintf (outFP, "%0*d",		TInstituteCode_len,		v_InstituteCode	);
	fprintf (outFP, "%0*d",		TTerminalNum_len,		v_TerminalNum	);
	fprintf (outFP, "%0*d",		TErrorCode_len,			v_ErrorCode		);
	fprintf (outFP, "%0*d",		TGenericYYYYMMDD_len,	v_FileUpdateDate);
	fprintf (outFP, "%0*d",		TGenericHHMMSS_len,		v_FileUpdateTime);
	fprintf (outFP, "%0*d",		TEconPriNumUpdated_len,	v_MaxNumToUpdate);

	fclose (outFP);


	// Write the name of output-file on string for T-SWITCH.
	*p_outputWritten = sprintf (OBuffer, "%s", fileName);
	*output_type_flg = ANSWER_IN_FILE;

	ssmd_data_ptr->error_code	= v_ErrorCode;
	ssmd_data_ptr->pharmacy_num	= v_PharmNum;
	ssmd_data_ptr->terminal_num	= v_TerminalNum;

	RESTORE_ISOLATION;

	return (RC_SUCCESS);
}
// End of 2084 handler.



/*=======================================================================
||																		||
||			 HandlerToMsg_2086											||
||	    Message handler for message 2086 -	Pharmacy_daily_sum			||
||											reconciliation message		||
||																		||
=======================================================================*/
int HandlerToMsg_2086 (char			*IBuffer,
					   int			TotalInputLen,
					   char			*OBuffer,
					   int			*p_outputWritten,
					   int			*output_type_flg,
					   SSMD_DATA	*ssmd_data_ptr)
{
	// Local declarations
	char			fileName [256];
	int				tries;
	int				reStart;
	TErrorCode		err;
	PHARMACY_INFO	Phrm_info;
	FILE			*outFP;
	ISOLATION_VAR;

	// SQL variables

	// INPUT MESSAGE
	TPharmNum					v_PharmNum;
	TInstituteCode				v_InstituteCode;
	TTerminalNum				v_TerminalNum;
	short						v_DiaryMonth;

	// RETURN MESSAGE
	// header
	TErrorCode					v_ErrorCode;
	int							v_MaxNumToUpdate;

	// lines
	TGenericYYYYMMDD			v_SaleDate;
	int							v_PrescCount;
	int							v_LinesCount;
	int							v_AmtSold;
	int							v_MemberPtn;
	int							v_PurchaseAmt;
	int							v_DeletedSales;
	int							v_DeleteLines;
	int							v_DeleteSum;
	int							v_DeletePtn;
	int							v_DelPurchaseAmt;


	// Init answer header variables
	REMEMBER_ISOLATION;
	PosInBuff			= IBuffer;
	v_ErrorCode			= NO_ERROR;
	v_MaxNumToUpdate	= 0;


	// Read request message fields.
	v_PharmNum			= GetLong	(&PosInBuff, TPharmNum_len			); CHECK_ERROR();
	v_InstituteCode		= GetShort	(&PosInBuff, TInstituteCode_len		); CHECK_ERROR();
	v_TerminalNum		= GetShort	(&PosInBuff, TTerminalNum_len		); CHECK_ERROR();
	v_DiaryMonth		= GetShort	(&PosInBuff, TGenericYYMM_len		); CHECK_ERROR();


	// Get amount of input not eaten
	ChckInputLen (TotalInputLen - (PosInBuff - IBuffer));
	CHECK_ERROR();


	// Get ready to process the request.
	reStart = MAC_TRUE;

	sprintf (fileName,
			 "%s/trn2086_%0*d_%0*d_%0*d.snd",
			 MsgDir,
			 TPharmNum_len,			v_PharmNum,
			 TInstituteCode_len,	v_InstituteCode,
			 TTerminalNum_len,		v_TerminalNum);


	// Retries loop.
	for (tries = 0;	(tries < SQL_UPDATE_RETRIES) && (reStart == MAC_TRUE); tries++)
	{
		// Create new file for response message.
		outFP = fopen (fileName, "w");

		if (outFP == NULL)
		{
			int	err = errno;

			GerrLogReturn (GerrId,
						   "\tCannot open output file '%s'\n\tError (%d) %s\n",
						   fileName,
						   err,
						   strerror(err));

			RESTORE_ISOLATION;
			return (ERR_UNABLE_OPEN_OUTPUT_FILE);
		}

		// Init answer header variables
		v_ErrorCode			= NO_ERROR;
		v_MaxNumToUpdate	= 0;
		reStart				= MAC_FALS;

		// Avoid unnecessary lock errors by downgrading "isolation"
		// for this transaction.
		SET_ISOLATION_DIRTY;

		// One-time loop to avoid GOTO's. Response is sent upon exit from this "loop".
		do
		{
			// Test if pharmacy has been OPENed.
			err = IS_PHARMACY_OPEN (v_PharmNum, v_InstituteCode, &Phrm_info);
			SetErrorVar (&v_ErrorCode, err);
			if (v_ErrorCode != MAC_OK)
			{
				break;
			}

			// DonR 28Jul2025: Adding a call to GetSpoolLock() in Transaction 2086, since
			// this transaction is apparently getting a lock on pharmacy_daily_sum and causing
			// deadlocks. I added Transaction Number and Spool Flag to GetSpoolLock() so
			// we'll get better diagnostics on which transaction is being delayed.
			GetSpoolLock (v_PharmNum, 2086, 0);

			// Open dates cursor.
			DeclareAndOpenCursorInto (	MAIN_DB, TR2086_pharmacy_daily_sum_date_cur,
										&v_SaleDate,
										&v_PharmNum,		&v_InstituteCode,
										&v_DiaryMonth,		END_OF_ARG_LIST		);

			if (SQLERR_error_test ())
			{
				SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR);
				break;
			}


			// Create the header with data so far.
			fprintf (outFP, "%0*d",		MSG_ID_LEN,				2087			);
			fprintf (outFP, "%0*d",		MSG_ERROR_CODE_LEN,		MAC_OK			);
			fprintf (outFP, "%0*d",		TPharmNum_len,			v_PharmNum		);
			fprintf (outFP, "%0*d",		TInstituteCode_len,		v_InstituteCode	);
			fprintf (outFP, "%0*d",		TTerminalNum_len,		v_TerminalNum	);
			fprintf (outFP, "%0*d",		TErrorCode_len,			v_ErrorCode		);
			fprintf (outFP, "%0*d",		TEconPriNumUpdated_len,	v_MaxNumToUpdate);


			// Write the pharmacy_daily_sum data to file.
			// Loop until we hit end-of-fetch on the dates available for the
			// requested Diary Month.
			for (;;)
			{
				// Fetch and test DB errors
				FetchCursor (	MAIN_DB, TR2086_pharmacy_daily_sum_date_cur	);

				Conflict_Test (reStart);

				if (SQLERR_code_cmp (SQLERR_end_of_fetch) == MAC_TRUE)
				{
					break;
				}
				else
				{
					if (SQLERR_error_test ())
					{
						SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR);
						break;
					}
				}

				// Get summary numbers for this date.
				ExecSQL (	MAIN_DB, TR2086_READ_pharmacy_daily_sum_one_day_totals,
							&v_PrescCount,		&v_LinesCount,		&v_AmtSold,
							&v_MemberPtn,		&v_PurchaseAmt,		&v_DeletedSales,
							&v_DeleteLines,		&v_DeleteSum,		&v_DeletePtn,
							&v_DelPurchaseAmt,
							&v_PharmNum,		&v_InstituteCode,	&v_DiaryMonth,
							&v_SaleDate,		END_OF_ARG_LIST							);

				if (SQLERR_error_test ())
				{
					SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR);
					break;
				}


				// Write the pharmacy_daily_sum row we fetched.
				v_MaxNumToUpdate++;

				fprintf (outFP, "%0*d",	TGenericYYYYMMDD_len,	v_SaleDate		);
				fprintf (outFP, "%0*d",	9,						v_PrescCount	);
				fprintf (outFP, "%0*d",	9,						v_LinesCount	);
				fprintf (outFP, "%0*d",	9,						v_AmtSold		);
				fprintf (outFP, "%0*d",	9,						v_MemberPtn		);
				fprintf (outFP, "%0*d",	9,						v_PurchaseAmt	);
				fprintf (outFP, "%0*d",	9,						v_DeletedSales	);
				fprintf (outFP, "%0*d",	9,						v_DeleteLines	);
				fprintf (outFP, "%0*d",	9,						v_DeleteSum		);
				fprintf (outFP, "%0*d",	9,						v_DeletePtn		);
				fprintf (outFP, "%0*d",	9,						v_DelPurchaseAmt);

			}	// Fetch dates loop.

			CloseCursor (	MAIN_DB, TR2086_pharmacy_daily_sum_date_cur	);

			SQLERR_error_test ();

		}
		while (0);	// One-time loop to avoid GOTO's.


		// DonR 27Aug2025: Clear the Pharmacy Spool Lock we set at the beginning of the transaction.
		ClearSpoolLock (v_PharmNum);

		// Commit the transaction.
		if (reStart == MAC_FALS)	/* Transaction OK then COMMIT	*/
		{
			// DonR 13Jan2020: No point in a commit when we haven't changed anything!
		}
		else
		{
			if (TransactionRestart () != MAC_OK)
			{
				v_ErrorCode = ERR_DATABASE_ERROR;
				break;
			}

			// DonR 26Jul2011: Suppress log message until the fourth attempt fails.
			if (tries > 2)
				GerrLogReturn (GerrId, "Table is locked for the <%d> time", tries);

			sleep (ACCESS_CONFLICT_SLEEP_TIME);

			fclose (outFP);
		}

	}	/* End of retries loop */


	// REWRITE MESSAGE HEADER.
	rewind (outFP);

	fprintf (outFP, "%0*d",		MSG_ID_LEN,				2087			);
	fprintf (outFP, "%0*d",		MSG_ERROR_CODE_LEN,		MAC_OK			);
	fprintf (outFP, "%0*d",		TPharmNum_len,			v_PharmNum		);
	fprintf (outFP, "%0*d",		TInstituteCode_len,		v_InstituteCode	);
	fprintf (outFP, "%0*d",		TTerminalNum_len,		v_TerminalNum	);
	fprintf (outFP, "%0*d",		TErrorCode_len,			v_ErrorCode		);
	fprintf (outFP, "%0*d",		TEconPriNumUpdated_len,	v_MaxNumToUpdate);

	fclose (outFP);


	// Write the name of output-file on string for T-SWITCH.
	*p_outputWritten = sprintf (OBuffer, "%s", fileName);
	*output_type_flg = ANSWER_IN_FILE;

	ssmd_data_ptr->error_code	= v_ErrorCode;
	ssmd_data_ptr->pharmacy_num	= v_PharmNum;
	ssmd_data_ptr->terminal_num	= v_TerminalNum;

	RESTORE_ISOLATION;

	return (RC_SUCCESS);
}
// End of 2086 handler.



/*=======================================================================
||																		||
||			 HandlerToMsg_2088											||
||	    Message handler for message 2088 -	List of pharmacy drug sales	||
||																		||
=======================================================================*/
int HandlerToMsg_2088 (char			*IBuffer,
					   int			TotalInputLen,
					   char			*OBuffer,
					   int			*p_outputWritten,
					   int			*output_type_flg,
					   SSMD_DATA	*ssmd_data_ptr)
{
	// Local declarations
	static int		mypid				= -1;
	int				NumSales;
	int				NumDrugs;
	int				TotSalesRead		= 0;
	int				TotDrugsRead		= 0;
	short			DeletionMode;

	char			fileName [256];
	int				tries;
	int				reStart;
	int				sale;
	int				drug;
	short			err;
	PHARMACY_INFO	Phrm_info;
	FILE			*outFP;
//	char			DiagBuffer [2048];
//	char			*DiagBufPtr;
	ISOLATION_VAR;

	// SQL variables

	// INPUT MESSAGE
	int								v_PharmNum;
	short							v_InstituteCode;
	short							v_TerminalNum;
	short							v_DiaryMonth;
	int								v_SaleDate;
	int								v_StartPrID;

	// RETURN MESSAGE
	// header
	short							v_ErrorCode			= 0;
	short							v_MoreToCome		= 0;
	int								v_LastPrID			= 0;
	int								v_MaxNumToUpdate	= 0;

	// Sales structure
	typedef struct
	{
		int							v_Date;
		int							v_MemberID;
		short						v_MemberIdCode;
		short						v_MemberInst;
		int							v_PrescriptionID;
		short						v_CreditType;
		short						v_MemberPriceCode;
		int							v_PharmPrescID;
		short						v_CheckDiaryMonth;
		short						v_CheckDelivered;
		short						v_CheckDeleted;
		int							v_TotalTikraDisc;
		int							v_TotalCouponDisc;
	}
	SALE_t;

	SALE_t SALE [5000];
	SALE_t *ThisSale;


	// Drug-line structure
	typedef struct
	{
		int							v_DrugPrID;
		int							v_LargoCode;
		int							v_AmtSold;
		int							v_MemberPtn;
		int							v_PurchaseAmt;
		short						v_Deleted;
		short						v_CheckDelivered_D;
		int							v_op;
		int							v_units;
	}
	DRUG_t;

	DRUG_t DRUG [20000];
	DRUG_t *ThisDrug;

	// Sale & Drug-line simple variables
	int								v_Date;
	int								v_MemberID;
	short							v_MemberIdCode;
	short							v_MemberInst;
	int								v_PrescriptionID;
	short							v_CreditType;
	short							v_MemberPriceCode;
	int								v_PharmPrescID;
	short							v_CheckDiaryMonth;
	int								v_DelPrID;
	int								v_DelPrPharm;
	short							v_DelPrDiaryMonth;
	short							v_CheckDelivered;
	short							v_CheckDeleted;
	short							v_RecipeSource;

	int								v_DrugPrID;
	int								v_LargoCode;
	int								v_AmtSold;
	int								v_MemberPtn;
	int								v_PurchaseAmt;
	short							v_Deleted;
	short							v_CheckDelivered_D;
	int								v_op;
	int								v_units;
	int								v_TotalTikraDisc;
	int								v_TotalCouponDisc;

	// Sale & drug-lines derived values
	short							v_LinesCount;
	int								TotDeleteLines;		// DonR 27Feb2020: Was a short, but it's mapped as an int in MacODBC_MyOperators.c.
	int								TotDeletions;		// DonR 27Feb2020: Was a short, but COUNT(*) is mapped as an int in MacODBC_MyOperators.c.
	int								TotDeletedPrice;
	int								TotDeletedPtn;
	int								TotDeletedPurchaseAmt;
	int								TotAmtSold;
	int								TotMemberPtn;
	int								ThisPurchaseAmt;
	int								TotPurchaseAmt;

	int								ThisMember;
	int								ThisPrID;

	// Dynamic cursor creation.
	char							DrugSelect[10001];
	int								DrugSelectLen;


	// Init answer header variables
	REMEMBER_ISOLATION;
	PosInBuff			= IBuffer;
	v_ErrorCode			= NO_ERROR;
	v_MaxNumToUpdate	= 0;

	if (mypid < 0)
		mypid = getpid ();

	// Avoid unnecessary lock errors by downgrading "isolation"
	// for this transaction.
	SET_ISOLATION_DIRTY;

	// Read request message fields.
	v_PharmNum			= GetLong	(&PosInBuff, TPharmNum_len			); CHECK_ERROR();
	v_InstituteCode		= GetShort	(&PosInBuff, TInstituteCode_len		); CHECK_ERROR();
	v_TerminalNum		= GetShort	(&PosInBuff, TTerminalNum_len		); CHECK_ERROR();
	v_DiaryMonth		= GetShort	(&PosInBuff, TGenericYYMM_len		); CHECK_ERROR();
	v_SaleDate			= GetLong	(&PosInBuff, TGenericYYYYMMDD_len	); CHECK_ERROR();
	v_StartPrID			= GetLong	(&PosInBuff, TRecipeIdentifier_len	); CHECK_ERROR();

	// Get amount of input not eaten
	ChckInputLen (TotalInputLen - (PosInBuff - IBuffer));
	CHECK_ERROR ();

//	GerrLogMini (GerrId, "\n\n2088: Pharmacy %d, Month %d, Date %d.", v_PharmNum, v_DiaryMonth, v_SaleDate);

	// Get ready to process the request.
	reStart = MAC_TRUE;

	sprintf (fileName,
			 "%s/trn2088_%0*d_%0*d_%0*d_%0*d.snd",
			 MsgDir,
			 TPharmNum_len,			v_PharmNum,
			 TInstituteCode_len,	v_InstituteCode,
			 TTerminalNum_len,		v_TerminalNum,
			 TRecipeIdentifier_len,	v_StartPrID);


	// Retries loop.
	for (tries = 0;	(tries < SQL_UPDATE_RETRIES) && (reStart == MAC_TRUE); tries++)
	{
		// Create new file for response message.
		outFP = fopen (fileName, "w");

		if (outFP == NULL)
		{
			int	err = errno;

			GerrLogReturn (GerrId,
						   "\tCannot open output file '%s'\n\tError (%d) %s\n",
						   fileName,
						   err,
						   strerror(err));

			RESTORE_ISOLATION;
			return (ERR_UNABLE_OPEN_OUTPUT_FILE);
		}

		// Init answer header variables
		v_ErrorCode			= NO_ERROR;
		v_MaxNumToUpdate	= 0;
		reStart				= MAC_FALS;


		// One-time loop to avoid GOTO's. Response is sent upon exit from this "loop".
		do
		{
			// Test if pharmacy has been OPENed.
			err = IS_PHARMACY_OPEN (v_PharmNum, v_InstituteCode, &Phrm_info);
			SetErrorVar (&v_ErrorCode, err);
			if (v_ErrorCode != MAC_OK)
			{
				break;
			}


			// Open sales and drugs cursors and read in all relevant data.
			// Sales: length of data returned = 30 bytes.
			// DonR 15Dec2011: Added fields to check if deletions are for sales by the same pharmacy
			// in the same month.
			// DonR 01Jun2022: Checked this SQL, and it conforms to one of the indexes.
			DeclareAndOpenCursorInto (	MAIN_DB, TR2088_prescriptions_cur,
										&v_Date,			&v_MemberID,		&v_MemberIdCode,
										&v_MemberInst,		&v_PrescriptionID,	&v_CreditType,
										&v_MemberPriceCode,	&v_PharmPrescID,	&v_CheckDiaryMonth,
										&v_CheckDelivered,	&v_RecipeSource,	&v_TotalTikraDisc,
										&v_TotalCouponDisc,	&v_DelPrID,			&v_DelPrPharm,
										&v_CheckDeleted,
										&v_PharmNum,		&v_SaleDate,		END_OF_ARG_LIST			);
//GerrLogMini (GerrId, "2088: Opened TR2088_prescriptions_cur, SQLCODE = %d.", SQLCODE);

			if (SQLERR_error_test ())
			{
				GerrLogFnameMini ("2088_log",
								  GerrId,
								  "Error %d opening sales cursor.", SQLCODE);

				SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR);
				break;
			}


			// Read all qualifying sales into array. Note that this loop terminates
			// without giving an error if it reaches 5000 sales - which is something
			// like five times what I've actually seen happen at one pharmacy in
			// one day.
			for (ThisSale = &SALE[0], NumSales = 0; NumSales < 5000; )
			{
				// Fetch next prescriptions row and test DB errors
				FetchCursor (	MAIN_DB, TR2088_prescriptions_cur	);
//GerrLogMini (GerrId, "2088: Fetched TR2088_prescriptions_cur, SQLCODE = %d.", SQLCODE);

				if (SQLCODE != 0)
				{
					Conflict_Test (reStart);

					if (SQLERR_code_cmp (SQLERR_end_of_fetch) == MAC_TRUE)
					{
						break;
					}
					else
					{
						if (SQLERR_error_test ())
						{
							SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR);
							break;
						}
					}
				}

				TotSalesRead++;

				// DonR 15Dec2011: If the row we just read is a deletion, retrieve the Diary Month of the
				// drug sale it deleted. If the deletion was for a sale in a different month, we want to
				// go ahead and report it to the pharmacy (see below for changes in the "if" statement).
				if (v_CheckDelivered == SALE_DELETION)
				{
					ExecSQL (	MAIN_DB, READ_get_deleted_sale_diary_month,
								&v_DelPrDiaryMonth,	&v_DelPrID,	END_OF_ARG_LIST		);

					if (SQLCODE != 0)
						v_DelPrDiaryMonth = 0;
				}


				// Since Informix is too stupid to accomodate all our selection requirements
				// without taking forever to open the cursor, do a "post-select select".
				// DonR 21Jun2006: Ignore duplicate spooled sales.
				// DonR 02Aug2007: Manually skip sales with PrID below target, so cursor
				// doesn't have to include PrID.
				// DonR 05Aug2009: For private pharmacies, skip non-prescription sales.
				// DonR 07Feb2011: Change delivery-flag check so that Action Types 3 and 4
				// (in addition to Action Type 1) will be sent to pharmacy.
				// DonR 15Dec2011: Suppress reporting of deletions only if the sale happened in the same month
				// as the deletion and was done at the same pharmacy. Deletions of prior-month sales and sales
				// at different pharmacies are reported as independent items.
				// DonR 01Jun2022: For (hopefully) better performance under MS-SQL, added "AND delivered_flg <> 0"
				// to the WHERE clause in TR2088_prescriptions_cur and deleted it from the "post-SELECT" logic.
				if ((v_PrescriptionID	<= v_StartPrID)								||
					((v_CheckDiaryMonth	!= v_DiaryMonth) && (v_DiaryMonth != 0))	||
//					(v_CheckDelivered	== DRUG_NOT_DELIVERED)						||	// DonR 07Feb2011

					((v_CheckDelivered	== SALE_DELETION)		&&
					 (v_DelPrDiaryMonth == v_CheckDiaryMonth)	&&
					 (v_DelPrPharm		== v_PharmNum))								||	// DonR 15Dec2011

					(v_CheckDeleted		== 2)										||	// Duplicate spooled sale.
					((PRIVATE_PHARMACY) && (v_RecipeSource == RECIP_SRC_NO_PRESC))		// DonR 05Aug2009.
				   )
				{
					continue;
				}

				// Store prescriptions data in structure array.
				ThisSale->v_Date			= v_Date;
				ThisSale->v_MemberID		= v_MemberID;
				ThisSale->v_MemberIdCode	= v_MemberIdCode;
				ThisSale->v_MemberInst		= v_MemberInst;
				ThisSale->v_PrescriptionID	= v_PrescriptionID;
				ThisSale->v_CreditType		= v_CreditType;
				ThisSale->v_MemberPriceCode	= v_MemberPriceCode;
				ThisSale->v_PharmPrescID	= v_PharmPrescID;
				ThisSale->v_CheckDiaryMonth	= v_CheckDiaryMonth;
				ThisSale->v_CheckDelivered	= v_CheckDelivered;
				ThisSale->v_TotalTikraDisc	= v_TotalTikraDisc;
				ThisSale->v_TotalCouponDisc	= v_TotalCouponDisc;
				ThisSale->v_CheckDeleted	= v_CheckDeleted;

				NumSales++;	// This sale qualifies, so increment counter.
				ThisSale++;
			}	// Sale-reading loop

			CloseCursor (	MAIN_DB, TR2088_prescriptions_cur	);

			SQLERR_error_test();

//
//			GerrLogMini (GerrId, "2088: NumSales = %d.", NumSales);

			// If anything so far has thrown a DB error, break out of one-time loop.
			if (v_ErrorCode == ERR_DATABASE_ERROR)
				break;

			qsort (SALE, (size_t)NumSales, sizeof (SALE_t), SALE_compare);


			// Create dynamic Drug cursor.
			//	SELECT	prescription_id,	largo_code,			total_drug_price,
			//			total_member_price,	price_for_op_ph,	del_flg,
			//			delivered_flg,		op,					units
			//
			//	FROM	prescription_drugs...
			// DonR 01Jun2022: Use a single sprintf, just to make the code prettier.
			DrugSelectLen  = sprintf (	DrugSelect,
										"SELECT	prescription_id,	largo_code,			total_drug_price,	"
										"		total_member_price,	price_for_op_ph,	del_flg,			"
										"		delivered_flg,		op, units								"
										"FROM	prescription_drugs											"
										"WHERE	prescription_id IN (										"	);

			// DonR 16Sep2007: If no sales were selected, produce a "dummy select" for
			// drugs.
			// DonR 28Dec2016 WORKINGPOINT: The business of always appending a comma and then
			// deleting it at the end is stupid. Instead, we should use a ternary operator
			// (sale > ) ? "," : "" to put the necessary commas *before* number 2-n.
			if (NumSales > 0)
			{
				for (sale = 0; (sale < NumSales) && (sale < 310); sale++)
				{
					DrugSelectLen += sprintf (DrugSelect + DrugSelectLen,
											  "%d,",
											  SALE [sale].v_PrescriptionID);
				}
			}
			else
			{
				DrugSelectLen += sprintf (DrugSelect + DrugSelectLen, "-1,");
			}

			// "Backspace" once character to get rid of the extra comma.
			// DonR 01Jun2022: Added "AND delivered_flg <> 0" - this should speed
			// things up a little, now that we're running under MS-SQL (which is
			// presumably smarter than Informix was about optimizing queries).
			DrugSelectLen += sprintf (DrugSelect + DrugSelectLen - 1, ") AND delivered_flg <> 0");

			// DonR 01Jun2022: Changed this to a DeclareAndOpenCursorInto, with a simple FetchCursor - I think
			// it will run at least a tiny bit faster.
//			DeclareAndOpenCursor (	MAIN_DB, TR2088_prescription_drugs_cur, &DrugSelect, END_OF_ARG_LIST	);
			DeclareAndOpenCursorInto (	MAIN_DB, TR2088_prescription_drugs_cur,		&DrugSelect,
										&v_DrugPrID,			&v_LargoCode,		&v_AmtSold,
										&v_MemberPtn,			&v_PurchaseAmt,		&v_Deleted,
										&v_CheckDelivered_D,	&v_op,				&v_units,
										END_OF_ARG_LIST												);
//GerrLogMini (GerrId, "2088: Declared TR2088_prescription_drugs_cur from {%s}, SQLCODE = %d.", DrugSelect, SQLCODE);

			if (SQLERR_error_test ())
			{
				GerrLogFnameMini ("2088_log", GerrId, "Error %d opening drugs cursor.", SQLCODE);

				SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR);
				break;
			}

			for (ThisDrug = &DRUG[0], NumDrugs = 0; NumDrugs < 20000; )
			{
				// Fetch next prescription_drugs row and test DB errors
				FetchCursor (	MAIN_DB, TR2088_prescription_drugs_cur	);
//GerrLogMini (GerrId, "2088: Fetched TR2088_prescription_drugs_cur, SQLCODE = %d.", SQLCODE);

				if (SQLCODE != 0)
				{
					Conflict_Test (reStart);

					if (SQLERR_code_cmp (SQLERR_end_of_fetch) == MAC_TRUE)
					{
						break;
					}
					else
					{
						if (SQLERR_error_test ())
						{
							SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR);
							break;
						}
					}
				}

				TotDrugsRead++;

				// Since Informix is too stupid to accomodate all our selection requirements
				// without taking forever to open the cursor, do a "post-select select".
				// DonR 07Feb2011: Send "delivered" drugs for Action Types other than Drug Sales.
				// DonR 01Jun2022: Added "AND delivered_flg <> 0" to the WHERE clause - this should
				// speed things up a little, now that we're running under MS-SQL (which is presumably
				// smarter than Informix was about optimizing queries). Accordingly, we don't need
				// "if (v_CheckDelivered_D != DRUG_NOT_DELIVERED)" here!
//				if (v_CheckDelivered_D != DRUG_NOT_DELIVERED)	// DonR 07Feb2011.
//				{
				// Store drug-line data in structure array.
				ThisDrug->v_DrugPrID			= v_DrugPrID;
				ThisDrug->v_LargoCode			= v_LargoCode;
				ThisDrug->v_AmtSold				= v_AmtSold;
				ThisDrug->v_MemberPtn			= v_MemberPtn;
				ThisDrug->v_PurchaseAmt			= v_PurchaseAmt;
				ThisDrug->v_Deleted				= v_Deleted;
				ThisDrug->v_CheckDelivered_D	= v_CheckDelivered_D;
				ThisDrug->v_op					= v_op;
				ThisDrug->v_units				= v_units;

				NumDrugs++;
				ThisDrug++;
//				}	// Drug was delivered.

			}	// Drug-reading loop


			CloseCursor (	MAIN_DB, TR2088_prescription_drugs_cur	);

			SQLERR_error_test();

			// If anything so far has thrown a DB error, break out of one-time loop.
			if (v_ErrorCode == ERR_DATABASE_ERROR)
				break;

			// Sort array of drugs.
			qsort (DRUG, (size_t)NumDrugs, sizeof (DRUG_t), DRUG_compare);


			// Create the header with data so far.
			fprintf (outFP, "%0*d",		MSG_ID_LEN,				2089			);
			fprintf (outFP, "%0*d",		MSG_ERROR_CODE_LEN,		MAC_OK			);
			fprintf (outFP, "%0*d",		TPharmNum_len,			v_PharmNum		);
			fprintf (outFP, "%0*d",		TInstituteCode_len,		v_InstituteCode	);
			fprintf (outFP, "%0*d",		TTerminalNum_len,		v_TerminalNum	);
			fprintf (outFP, "%0*d",		TErrorCode_len,			v_ErrorCode		);
			fprintf (outFP, "%0*d",		TGenericFlag_len,		v_MoreToCome	);
			fprintf (outFP, "%0*d",		TRecipeIdentifier_len,	v_LastPrID		);
			fprintf (outFP, "%0*d",		TEconPriNumUpdated_len,	v_MaxNumToUpdate);


			// Write the sales/drugs data to file.
			// Loop until we hit end-of-fetch on the sales available for the
			// requested Diary Month and Sale Date.
			for (sale = 0, drug = 0; sale < NumSales; sale++)
			{
				ThisSale = &SALE [sale];
				ThisDrug = &DRUG [drug];

				ThisMember	= ThisSale->v_MemberID;
				ThisPrID	= ThisSale->v_PrescriptionID;

				// Initialize drug totals for this sale.
				v_LinesCount			= 0;
				TotAmtSold				= 0;
				TotMemberPtn			= 0 - (ThisSale->v_TotalTikraDisc + ThisSale->v_TotalCouponDisc);	// DonR 11Jan2011.
				TotPurchaseAmt			= 0;
				TotDeletedPrice			= 0;
				TotDeletedPtn			= 0;
				TotDeletedPurchaseAmt	= 0;
				TotDeleteLines			= 0;
				TotDeletions			= 0;
				DeletionMode			= 0;	// DonR 03Oct2011: No deletion detected yet.

				// Find matching rows from prescription_drugs table.
				// A value of -1 for v_DrugPrID means that we've reached
				// the end of the prescription_drugs cursor.
				while ((drug < NumDrugs) && (DRUG [drug].v_DrugPrID <= ThisSale->v_PrescriptionID))
				{
					ThisDrug = &DRUG [drug];

					// If the last-fetched row matches the sale AND it was actually,
					// delivered, add it to totals.
					if (ThisDrug->v_DrugPrID == ThisSale->v_PrescriptionID)
					{
						// DonR 31Jul2007: Package size now comes from an array rather
						// than from the sold-drugs cursor.
						ThisPurchaseAmt	=  (int)(((float)ThisDrug->v_op
												+ ((float)ThisDrug->v_units / (float)pkg_size [ThisDrug->v_LargoCode])) * (float)ThisDrug->v_PurchaseAmt);

						TotAmtSold		+= ThisDrug->v_AmtSold;
						TotMemberPtn	+= ThisDrug->v_MemberPtn;
						TotPurchaseAmt	+= ThisPurchaseAmt;
						v_LinesCount	++;

						// DonR 16Apr2019 CR #28230: Because we want to tell the pharmacy only about what that
						// pharmacy did (and not about deletions that were performed elsewhere), we want to take
						// TotDeleteLines from the actual deletion transactions (which are now filtered by Pharmacy
						// Code) and *not* from the number of deleted drugs in the sale itself.
						if (ThisDrug->v_Deleted	> 0)
						{
//							TotDeleteLines++;	DonR 16Apr2019 CR #28230
//
							// Presc_delete is mostly obsolete - but as of early 2019, it's still in (very)
							// occasional use.
							// DonR 16Apr2019 CR #28230: Added Pharmacy Code to the WHERE clause, since we're supposed
							// to report only what the current requesting pharmacy did, rather than deletions
							// that were performed at other pharmacies.
							// DonR 01Jun2022: Eliminate the SELECT from presc_delete - that table is well
							// and truly dead.

							// DonR 19Jan2011: If we didn't see anything in the old presc_delete table,
							// try again using the new "Nihul Tikrot"-style deletions, which are stored
							// in the prescriptions/prescription_drugs tables. Note that because the
							// del_presc_id column in prescriptions isn't indexed, we want to select
							// by member_id (which is indexed) so that we don't kill performance.
							// DonR 06Sep2011: Don't count non-"delivered" deletions!
							// DonR 03Aug2014: Added new index on del_presc_id and changed the WHERE sequence
							// to reflect this. The change was needed because of the Maccabi pharmacy in Maccabim,
							// which sells to non-Maccabi members; this means that when deletions occur, they are
							// sometimes for Member ID 0, and these deletions were slowing down Trn. 2088 to the
							// point where it was timing out. Note that Informix was running the SELECT slowly
							// even with the new index in place; the solution was to take out the member_id
							// criterion entirely.
							// DonR 06Mar2019 CR #28230: Restrict this SELECT to retrieve only deletion transactions that
							// took place at the requesting pharmacy. This way each pharmacy sees numbers that reflect only
							// what happened at that pharmacy; if a sale was deleted at a different pharmacy, the deletion
							// data will show up when that pharmacy runs Transaction 2088, but will *not* show up for the
							// pharmacy that performed the original sale.
							if ((TotDeletions == 0) && (DeletionMode == 0))
							{
								ExecSQL (	MAIN_DB, TR2088_READ_sum_of_deletions_of_a_sale,
											&TotDeletions,		&v_TotalTikraDisc,
											&v_TotalCouponDisc,	&TotDeleteLines,
											&ThisPrID,			&v_PharmNum,
											END_OF_ARG_LIST										);

								// Because the aggregate function COUNT(*) returns a value even if nothing is found, we can't
								// assume that nothing-found will generate an SQL error; so a successful read requires SQLCODE
								// to be zero *and* TotDeletions > 0.
								if ((SQLCODE == 0) && (TotDeletions > 0))
								{
									// No old-style deletions, so we're dealing with new-style deletion.
									DeletionMode = 2;

									// DonR 03Oct2011: For new-style sale deletions, we need to get price and participation data from the
									// deletions themselves (stored in prescriptions/prescription_drugs) rather than from the original sale.
									// DonR 03Aug2014: Added new index on del_presc_id and changed the WHERE sequence to reflect this. The
									// change was needed because of the Maccabi pharmacy in Maccabim, which sells to non-Maccabi members;
									// this means that when deletions occur, they are sometimes for Member ID 0, and these deletions were
									// slowing down Trn. 2088 to the point where it was timing out. Note that Informix was running the
									// SELECT slowly even with this index in place; the solution was to take out the member_id criterion
									// entirely.
									// DonR 06Mar2019: Restrict this SELECT to retrieve only deletion transactions that took place at
									// the requesting pharmacy. This way each pharmacy sees numbers that reflect only what happened at
									// that pharmacy; if a sale was deleted at a different pharmacy, the deletion data will show up when that
									// pharmacy runs Transaction 2088, but will *not* show up for the pharmacy that performed the original sale.
									// (Note that if performance for this cursor degrades seriously, the new criterion could be changed to
									// a "post-SELECT select".)
									DeclareAndOpenCursorInto (	MAIN_DB, TR2088_sale_deletions_cur,
																&v_AmtSold,			&v_MemberPtn,		&v_PurchaseAmt,
																&v_LargoCode,		&v_op,				&v_units,
																&ThisPrID,			&v_PharmNum,		END_OF_ARG_LIST		);

									if (SQLERR_error_test ())
									{
										SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR);
										break;
									}
//GerrLogMini (GerrId, "2088: Opened TR2088_sale_deletions_cur, SQLCODE = %d.", SQLCODE);

									do
									{
										FetchCursor (	MAIN_DB, TR2088_sale_deletions_cur	);

										if (SQLCODE != 0)
										{
											Conflict_Test (reStart);

											if (SQLERR_code_cmp (SQLERR_end_of_fetch) == MAC_TRUE)
											{
												break;
											}
											else
											{
												if (SQLERR_error_test ())
												{
													SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR);
													break;
												}
											}
										}

										ThisPurchaseAmt = (int)(((float)v_op + ((float)v_units / (float)pkg_size [v_LargoCode]))
																* (float)v_PurchaseAmt);

										TotDeletedPrice			-= v_AmtSold;
										TotDeletedPtn			-= v_MemberPtn;
										TotDeletedPurchaseAmt	-= ThisPurchaseAmt;
									}
									while (1);

									CloseCursor (	MAIN_DB, TR2088_sale_deletions_cur	);

									// Adjust deleted participation to reflect discounts reported in the deletions.
									// NOTE: Because we're inside an "if" that includes the condition "&& (DeletionMode == 0)",
									// we don't have to worry about retrieving this stuff or adding it to the total more than once.
									TotDeletedPtn += (v_TotalTikraDisc + v_TotalCouponDisc);

								}	// Successful read from prescriptions table.
								else
								{
									// If we didn't read anything, set zeroes - since we're sometimes getting bogus values.
									TotDeletions		= 0;
									v_TotalTikraDisc	= 0;
									v_TotalCouponDisc	= 0;
									TotDeleteLines		= 0;
								}
							}	// Nothing found from old presc_delete table pertaining to the current sale.
						}	// This drug line was deleted.
					}	// This drug line belongs to the current sale and was delivered.

					// Fetch next prescriptions row and test DB errors
					drug++;

				}	// Loop on drug lines.


				// Write the sales/drugs rows we fetched.
				v_LastPrID = ThisSale->v_PrescriptionID;

				fprintf (outFP, "%0*d",		TPharmRecipeNum_len,	ThisSale->v_PharmPrescID	);
				fprintf (outFP, "%0*d",		TGenericYYYYMMDD_len,	ThisSale->v_Date			);
				fprintf (outFP, "%0*d",		TMemberBelongCode_len,	ThisSale->v_MemberInst		);
				fprintf (outFP, "%0*d",		4,						ThisSale->v_MemberPriceCode	);
				fprintf (outFP, "%0*d",		TRecipeIdentifier_len,	ThisSale->v_PrescriptionID	);
				fprintf (outFP, "%0*d",		9,						ThisSale->v_MemberID		);
				fprintf (outFP, "%0*d",		1,						ThisSale->v_MemberIdCode	);
				fprintf (outFP, "%0*d",		9,						TotAmtSold					);
				fprintf (outFP, "%0*d",		9,						TotMemberPtn				);
				fprintf (outFP, "%0*d",		9,						TotPurchaseAmt				);
				fprintf (outFP, "%0*d",		2,						v_LinesCount				);
				fprintf (outFP, "%0*d",		2,						ThisSale->v_CreditType		);
				fprintf (outFP, "%0*d",		2,						TotDeletions				);
				fprintf (outFP, "%0*d",		2,						TotDeleteLines			);
				fprintf (outFP, "%0*d",		9,						TotDeletedPrice			);
				fprintf (outFP, "%0*d",		9,						TotDeletedPtn			);
				fprintf (outFP, "%0*d",		9,						TotDeletedPurchaseAmt	);

//				DiagBufPtr = DiagBuffer;
//
//				DiagBufPtr += sprintf (DiagBufPtr, "%0*d",		TPharmRecipeNum_len,	ThisSale->v_PharmPrescID	);
//				DiagBufPtr += sprintf (DiagBufPtr, "%0*d",		TGenericYYYYMMDD_len,	ThisSale->v_Date			);
//				DiagBufPtr += sprintf (DiagBufPtr, "%0*d",		TMemberBelongCode_len,	ThisSale->v_MemberInst		);
//				DiagBufPtr += sprintf (DiagBufPtr, "%0*d",		4,						ThisSale->v_MemberPriceCode	);
//				DiagBufPtr += sprintf (DiagBufPtr, "%0*d",		TRecipeIdentifier_len,	ThisSale->v_PrescriptionID	);
//				DiagBufPtr += sprintf (DiagBufPtr, "%0*d",		9,						ThisSale->v_MemberID		);
//				DiagBufPtr += sprintf (DiagBufPtr, "%0*d",		1,						ThisSale->v_MemberIdCode	);
//				DiagBufPtr += sprintf (DiagBufPtr, "%0*d",		9,						TotAmtSold					);
//				DiagBufPtr += sprintf (DiagBufPtr, "%0*d",		9,						TotMemberPtn				);
//				DiagBufPtr += sprintf (DiagBufPtr, "%0*d",		9,						TotPurchaseAmt				);
//				DiagBufPtr += sprintf (DiagBufPtr, "%0*d",		2,						v_LinesCount				);
//				DiagBufPtr += sprintf (DiagBufPtr, "%0*d",		2,						ThisSale->v_CreditType		);
//				DiagBufPtr += sprintf (DiagBufPtr, "%0*d",		2,						TotDeletions				);
//				DiagBufPtr += sprintf (DiagBufPtr, "%0*d",		2,						TotDeleteLines			);
//				DiagBufPtr += sprintf (DiagBufPtr, "%0*d",		9,						TotDeletedPrice			);
//				DiagBufPtr += sprintf (DiagBufPtr, "%0*d",		9,						TotDeletedPtn			);
//				DiagBufPtr += sprintf (DiagBufPtr, "%0*d",		9,						TotDeletedPurchaseAmt	);
//
//				GerrLogMini (GerrId, "2088: Len = %d: %s\n", strlen (DiagBuffer), DiagBuffer);


				// If we've prepared a full batch, set flag and exit loop.
				if (++v_MaxNumToUpdate > 309)	// 310 records is almost 32,000 bytes.
				{
					v_MoreToCome = 1;
					break;
				}

			}	// Loop on sales.
		}
		while (0);	// One-time loop to avoid GOTO's.


		// If reStart is TRUE, we hit a DB lock and we need to retry. Otherwise there's
		// nothing to do - this transaction is read-only, so there's nothing to COMMIT.
		if (reStart)
		{
			// DonR 26Jul2011: Suppress log message until the fourth attempt fails.
			if (tries > 2)
				GerrLogReturn (GerrId, "Table is locked for the <%d> time", tries);

			sleep (ACCESS_CONFLICT_SLEEP_TIME);

			fclose (outFP);
		}

	}	/* End of retries loop */



	// REWRITE MESSAGE HEADER.
	rewind (outFP);

	fprintf (outFP, "%0*d",		MSG_ID_LEN,				2089			);
	fprintf (outFP, "%0*d",		MSG_ERROR_CODE_LEN,		MAC_OK			);
	fprintf (outFP, "%0*d",		TPharmNum_len,			v_PharmNum		);
	fprintf (outFP, "%0*d",		TInstituteCode_len,		v_InstituteCode	);
	fprintf (outFP, "%0*d",		TTerminalNum_len,		v_TerminalNum	);
	fprintf (outFP, "%0*d",		TErrorCode_len,			v_ErrorCode		);
	fprintf (outFP, "%0*d",		TGenericFlag_len,		v_MoreToCome	);
	fprintf (outFP, "%0*d"	,	TRecipeIdentifier_len,	v_LastPrID		);
	fprintf (outFP, "%0*d",		TEconPriNumUpdated_len,	v_MaxNumToUpdate);

	fclose (outFP);


	// Write the name of output-file on string for T-SWITCH.
	*p_outputWritten = sprintf (OBuffer, "%s", fileName);
	*output_type_flg = ANSWER_IN_FILE;

	ssmd_data_ptr->error_code	= v_ErrorCode;
	ssmd_data_ptr->pharmacy_num	= v_PharmNum;
	ssmd_data_ptr->terminal_num	= v_TerminalNum;

	RESTORE_ISOLATION;

	return (RC_SUCCESS);
}
// End of 2088 handler.



int SALE_compare (const void *data1, const void *data2)
{
	// Sales structure
	typedef struct
	{
		TGenericYYYYMMDD			v_Date;
		int							v_MemberID;
		short						v_MemberIdCode;
		short						v_MemberInst;
		int							v_PrescriptionID;
		short						v_CreditType;
		short						v_MemberPriceCode;
		int							v_PharmPrescID;
		short						v_CheckDiaryMonth;
		short						v_CheckDelivered;
		short						v_CheckDeleted;
	}
	SALE_t;

	SALE_t *Sale_1 = (SALE_t *)data1;
	SALE_t *Sale_2 = (SALE_t *)data2;


	return (Sale_1->v_PrescriptionID - Sale_2->v_PrescriptionID);
}


int DRUG_compare (const void *data1, const void *data2)
{
	// Drug-line structure
	typedef struct
	{
		int							v_DrugPrID;
		int							v_LargoCode;
		int							v_AmtSold;
		int							v_MemberPtn;
		int							v_PurchaseAmt;
		short						v_Deleted;
		short						v_CheckDelivered_D;
		int							v_op;
		int							v_units;
	}
	DRUG_t;

	DRUG_t *Drug_1 = (DRUG_t *)data1;
	DRUG_t *Drug_2 = (DRUG_t *)data2;


	return (Drug_1->v_DrugPrID - Drug_2->v_DrugPrID);
}



/*=======================================================================
||																		||
||			 init_package_size_array									||
||	    (Re-)initialize array of drug package sizes - used by Trn. 2088	||
||																		||
=======================================================================*/
int init_package_size_array (void)
{
	// Local declarations
	int				i;
	int				tries;
	int				reStart;
//	static int		Interval	= 14400;
	static int		Interval	= 300;	// Initial default - try every 5 minutes until DB query works.
	static bool		Initialized	= 0;
	TErrorCode		err			= 0;
	ISOLATION_VAR;

	// SQL variables
	int						v_PackageSize;
	int						v_LargoCode;


	// Init answer header variables
	REMEMBER_ISOLATION;

	// Avoid unnecessary lock errors by downgrading "isolation"
	// for this transaction.
	SET_ISOLATION_DIRTY;


	// The first time we run this routine, initialize array to all 1's - just to avoid division by zero.
	if (!Initialized)
	{
		for (i = 0; i < 100000; i++)
		{
			pkg_size [i] = 1;	// Default, mainly to avoid division by zero.
		}

		Initialized = 1;
	}

	// Get ready to process the request.
	reStart = MAC_TRUE;


	// Retries loop.
	for (tries = 0;	(tries < SQL_UPDATE_RETRIES) && (reStart == MAC_TRUE); tries++)
	{
		reStart = MAC_FALS;

		do
		{
			// DonR 31Jul2007: If necessary, (re-)initialize Package Size array.
			// The update will take place if more than four hours have passed
			// since the last initialization.
			// DonR 23Dec2021: It would be a good idea to set the interval conditionally: If the
			// array was read successfully, leave it alone for four hours, but if there was a
			// database problem, try again in something like five minutes.
			if (diff_from_now (pkg_size_asof_date, pkg_size_asof_time) >= Interval)
			{
				pkg_size_asof_date	= GetDate ();
				pkg_size_asof_time	= GetTime ();

				// Cursor for (re-)initializing Package Size array.
				// DonR 10Apr2011: Declare cursor only when needed.
				DeclareAndOpenCursorInto (	MAIN_DB, init_package_size_array_cur,
											&v_LargoCode,	&v_PackageSize,		END_OF_ARG_LIST		);

				if (SQLERR_error_test ())
				{
					GerrLogFnameMini ("2088_log",
									  GerrId,
									  "Error %d opening package_size cursor.", SQLCODE);

					err				= ERR_DATABASE_ERROR;

					// DonR 23Dec2021: If we failed to open the cursor, set up a retry in 5 minutes.
					Interval = 300;

					break;
				}

				// DonR 23Dec2021: If we get here, we succeeded in opening the package-size cursor.
				// However, we don't know yet if we'll hit some kind of error (contention, most likely)
				// reading the cursor - so for now, set a very short retry interval so that we won't
				// spend much time with a mix of actual table values and default values.
				Interval = 60;

				// Now read through the cursor and load package-size values to the array.
				for (;;)
				{
					FetchCursor (	MAIN_DB, init_package_size_array_cur	);

					Conflict_Test (reStart);

					if (SQLERR_code_cmp (SQLERR_end_of_fetch) == MAC_TRUE)
					{
						// DonR 23Dec2021: If we get here, we succeeded in opening the package-size cursor
						// and reading it until the end. Accordingly, set the timing for the next refresh
						// of the in-memory table to four hours from now.
						Interval = 14400;

						break;
					}
					else
					{
						if (SQLERR_error_test ())
						{
							err				= ERR_DATABASE_ERROR;
							break;
						}
					}

					// Package_size is a long integer in the database, but its
					// maximum value is in fact less than 32,767.
					// DonR 26Dec2021: Make sure we don't put values less than 1 into the array.
					pkg_size [v_LargoCode] = (v_PackageSize > 0) ? (short)v_PackageSize : 1;
				}

				CloseCursor (	MAIN_DB, init_package_size_array_cur	);

			}	// It's time to (re-)initialize the array.

		}
		while (0);	// One-time loop to avoid GOTO's.


		// See if we need to retry.
		if (reStart == MAC_FALS)
		{
			// No need to take any action.
		}
		else
		{
			// DonR 26Jul2011: Suppress log message until the fourth attempt fails.
			if (tries > 2)
				GerrLogReturn (GerrId, "Table is locked for the <%d> time", tries);

			sleep (ACCESS_CONFLICT_SLEEP_TIME);
		}

	}	/* End of retries loop */


	RESTORE_ISOLATION;

	return (err);
}
// End of init_package_size_array.



/*===============================================================================
||																				||
||			 HandlerToMsg_X090													||
||	    Message handler for message 2090 -	Single pharmacy drug sale details	||
||																				||
||	DonR 23Aug2010: Now also handles Trn. 5090, enhanced for "Nihul Tikrot".	||
||																				||
===============================================================================*/
int HandlerToMsg_X090 (char			*IBuffer,
					   int			TotalInputLen,
					   char			*OBuffer,
					   int			*p_outputWritten,
					   int			*output_type_flg,
					   SSMD_DATA	*ssmd_data_ptr,
					   int			transaction_num)
{
	// Local declarations
	char			fileName [256];
	int				tries;
	int				reStart;
	TErrorCode		err;
	PHARMACY_INFO	Phrm_info;
	FILE			*outFP;
	ISOLATION_VAR;

	// SQL variables

	// INPUT MESSAGE
	TPharmNum						v_PharmNum;
	TInstituteCode					v_InstituteCode;
	TTerminalNum					v_TerminalNum;
	short							v_DiaryMonth;
	short							v_MemberInst;
	short							v_MemberPriceCode;
	int								v_PharmPrescID;
	int								v_PrescID;

	// RETURN MESSAGE
	// Header
	TErrorCode						v_ErrorCode			= 0;
	int								v_MemberId;
	short							v_MemberIdCode;
	short							v_DocIdType;
	int								v_DocId;
	short							v_PrescSource;
	TGenericYYYYMMDD				v_SaleDate;
	TGenericHHMMSS					v_SaleTime;
	short							v_CreditTypeUsed;
	int								v_ReceiptNum;
	short							v_SaleTerminal;
	int								v_ClientLocation;
	int								v_DoctorLocation;
	short							v_SaleDiaryMonth;
	short							v_NumLines;
	int								v_tikra_discount;
	int								v_subsidy_amount;
	int								v_del_presc_id;
	int								v_del_pharm_code;
	int								v_del_ph_presc_id;
	int								v_del_sale_date;
	short							v_DelDiaryMonth;
	short							v_ActionType;

	// Lines
	short							v_LineNum;
	int								v_LargoCode;
	int								v_Units;
	int								v_Op;
	int								v_LinkToDrug;
	int								v_SalePricePerOp;
	int								v_PharmPricePerOp;
	short							v_ParticipCode;
	int								v_FixedPricePerOp;
	int								v_MemberDiscount;
	int								v_TotalPrice;
	int								v_MemberPtn;
	short							v_Deleted;
	short							MyDrugCodeLength;

	// Presc_delete fields
	int								v_DeletionId;
	TGenericYYYYMMDD				v_DeleteDate;
	TGenericHHMMSS					v_DeleteTime;
	int								v_DeleteReceiptNum;
	short							v_DeleteTerminalId;


	// Init answer header variables
	REMEMBER_ISOLATION;
	PosInBuff			= IBuffer;
	v_ErrorCode			= NO_ERROR;


	// Read request message fields.
	v_PharmNum			= GetLong	(&PosInBuff, TPharmNum_len			); CHECK_ERROR();
	v_InstituteCode		= GetShort	(&PosInBuff, TInstituteCode_len		); CHECK_ERROR();
	v_TerminalNum		= GetShort	(&PosInBuff, TTerminalNum_len		); CHECK_ERROR();
	v_DiaryMonth		= GetShort	(&PosInBuff, TGenericYYMM_len		); CHECK_ERROR();
	v_MemberInst		= GetShort	(&PosInBuff, TInstituteCode_len		); CHECK_ERROR();
	v_MemberPriceCode	= GetShort	(&PosInBuff, 4						); CHECK_ERROR();
	v_PharmPrescID		= GetLong	(&PosInBuff, TPharmRecipeNum_len	); CHECK_ERROR();
	v_PrescID			= GetLong	(&PosInBuff, TRecipeIdentifier_len	); CHECK_ERROR();

	// Get amount of input not eaten
	ChckInputLen (TotalInputLen - (PosInBuff - IBuffer));
	CHECK_ERROR();


	// Get ready to process the request.
	reStart = MAC_TRUE;

	sprintf (fileName,
			 "%s/Trn2090_%0*d_%0*d_%0*d_%0*d.snd",
			 MsgDir,
			 TPharmNum_len,			v_PharmNum,
			 TInstituteCode_len,	v_InstituteCode,
			 TTerminalNum_len,		v_TerminalNum,
			 TRecipeIdentifier_len,	v_PrescID);


	// Retries loop.
	for (tries = 0;	(tries < SQL_UPDATE_RETRIES) && (reStart == MAC_TRUE); tries++)
	{
		// Create new file for response message.
		outFP = fopen (fileName, "w");

		if (outFP == NULL)
		{
			RESTORE_ISOLATION;
			return (ERR_UNABLE_OPEN_OUTPUT_FILE);
		}

		// Init answer header variables
		v_ErrorCode			= NO_ERROR;
		reStart				= MAC_FALS;

		// Avoid unnecessary lock errors by downgrading "isolation"
		// for this transaction.
		SET_ISOLATION_DIRTY;

		// One-time loop to avoid GOTO's. Response is sent upon exit from this "loop".
		do
		{
			// Test if pharmacy has been OPENed.
			err = IS_PHARMACY_OPEN (v_PharmNum, v_InstituteCode, &Phrm_info);
			SetErrorVar (&v_ErrorCode, err);
			if (v_ErrorCode != MAC_OK)
			{
				break;
			}

			// Read the sale row and open drugs cursor.

			// Read prescriptions table by Prescription ID if possible - otherwise,
			// use various horrible pharmacy-related fields.
			// DonR 08Feb2011: Change test on delivered_flg, since we're no longer using only 1 and 0 there.
			if (v_PrescID > 0)
			{
				ExecSQL (	MAIN_DB,	TR2090_READ_prescriptions,
										TR2090_READ_prescriptions_by_prescription_id,
							&v_MemberId,		&v_MemberIdCode,		&v_MemberInst,
							&v_DocIdType,		&v_DocId,				&v_PrescSource,
							&v_PharmPrescID,	&v_PrescID,				&v_SaleDate,
							&v_SaleTime,		&v_CreditTypeUsed,		&v_ReceiptNum,

							&v_SaleTerminal,	&v_ClientLocation,		&v_DoctorLocation,
							&v_SaleDiaryMonth,	&v_tikra_discount,		&v_subsidy_amount,
							&v_del_presc_id,	&v_del_pharm_code,		&v_del_ph_presc_id,
							&v_del_sale_date,	&v_ActionType,			&v_NumLines,
					
							&v_PrescID,			&v_PharmNum,			&DRUG_NOT_DELIVERED,
							END_OF_ARG_LIST														);
			}
			else
			{
				ExecSQL (	MAIN_DB,	TR2090_READ_prescriptions,
										TR2090_READ_prescriptions_by_pharmacy_fields,
							&v_MemberId,		&v_MemberIdCode,		&v_MemberInst,
							&v_DocIdType,		&v_DocId,				&v_PrescSource,
							&v_PharmPrescID,	&v_PrescID,				&v_SaleDate,
							&v_SaleTime,		&v_CreditTypeUsed,		&v_ReceiptNum,

							&v_SaleTerminal,	&v_ClientLocation,		&v_DoctorLocation,
							&v_SaleDiaryMonth,	&v_tikra_discount,		&v_subsidy_amount,
							&v_del_presc_id,	&v_del_pharm_code,		&v_del_ph_presc_id,
							&v_del_sale_date,	&v_ActionType,			&v_NumLines,
					
							&v_PharmNum,		&v_DiaryMonth,			&v_PharmPrescID,
							&v_MemberInst,		&v_MemberPriceCode,		&DRUG_NOT_DELIVERED,
							END_OF_ARG_LIST														);
			}

			// DonR 30Jun2020: Give the full database diagnostic only if the error is
			// something other than not-found.
			if (SQLCODE != 0)
			{
				GerrLogMini (GerrId, "Trn. 2090 couldn't get prescriptions data for PrID %d (pharm %d month %d pharm pr ID %d member inst %d price code %d - SQLCODE = %d.",
							 v_PrescID, v_PharmNum, v_DiaryMonth, v_PharmPrescID, v_MemberInst, v_MemberPriceCode, SQLCODE);
				SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR);

				if (SQLERR_code_cmp (SQLERR_not_found) == MAC_FALSE)
				{
					SQLERR_error_test ();
				}

				break;
			}

			// DonR 23Aug2010: For "Nihul Tikrot" version, if this "sale" is actually a deletion, retrieve
			// the original sale's Diary Month - which is otherwise equal to zero.
			v_DelDiaryMonth = 0;
			if ((transaction_num == 5090) && (v_del_presc_id > 0))
			{
				ExecSQL (	MAIN_DB, READ_get_deleted_sale_diary_month,
							&v_DelDiaryMonth,	&v_del_presc_id,	END_OF_ARG_LIST		);

				if (SQLCODE != 0)
				{
					v_DelDiaryMonth = 0;
				}
			}	// Need to get Diary Month for deleted sale.


			// Select cursor for prescription_drugs.
			DeclareAndOpenCursorInto (	MAIN_DB, TR2090_prescription_drugs_cur,
										&v_LineNum,				&v_LargoCode,			&v_Units,
										&v_Op,					&v_LinkToDrug,			&v_SalePricePerOp,
										&v_PharmPricePerOp,		&v_ParticipCode,		&v_FixedPricePerOp,
										&v_MemberDiscount,		&v_TotalPrice,			&v_MemberPtn,
										&v_Deleted,
										&v_PrescID,				END_OF_ARG_LIST									);

			if (SQLERR_error_test ())
			{
				GerrLogToFileName (	"2090_log",
									GerrId,
									"Trn%d: Failed to open prescription_drugs cursor - SQLCODE = %d.",
									transaction_num, (int)SQLCODE);

				SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR);
				break;
			}


			// Create the header with data so far.
			fprintf (outFP, "%0*d",		MSG_ID_LEN,					transaction_num + 1);	// DonR 23Aug2010.
			fprintf (outFP, "%0*d",		MSG_ERROR_CODE_LEN,			MAC_OK			);
			fprintf (outFP, "%0*d",		TPharmNum_len,				v_PharmNum		);
			fprintf (outFP, "%0*d",		TInstituteCode_len,			v_InstituteCode	);
			fprintf (outFP, "%0*d",		TTerminalNum_len,			v_TerminalNum	);
			fprintf (outFP, "%0*d",		TErrorCode_len,				v_ErrorCode		);
			fprintf (outFP, "%0*d",		TGenericTZ_len,				v_MemberId		);
			fprintf (outFP, "%0*d",		TIdentificationCode_len,	v_MemberIdCode	);
			fprintf (outFP, "%0*d",		TInstituteCode_len,			v_MemberInst	);
			fprintf (outFP, "%0*d",		TIdentificationCode_len,	v_DocIdType		);
			fprintf (outFP, "%0*d",		TGenericTZ_len,				v_DocId			);
			fprintf (outFP, "%0*d",		TRecipeSource_len,			v_PrescSource	);
			fprintf (outFP, "%0*d",		TPharmRecipeNum_len,		v_PharmPrescID	);
			fprintf (outFP, "%0*d",		TRecipeIdentifier_len,		v_PrescID		);
			fprintf (outFP, "%0*d",		TGenericYYYYMMDD_len,		v_SaleDate		);
			fprintf (outFP, "%0*d",		TGenericHHMMSS_len,			v_SaleTime		);
			fprintf (outFP, "%0*d",		TCreditYesNo_len,			v_CreditTypeUsed);
			fprintf (outFP, "%0*d",		TInvoiceNum_len,			v_ReceiptNum	);
			fprintf (outFP, "%0*d",		TTerminalNum_len,			v_SaleTerminal	);
			fprintf (outFP, "%0*d",		TDeviceCode_len,			v_ClientLocation);
			fprintf (outFP, "%0*d",		TDeviceCode_len,			v_DoctorLocation);
			fprintf (outFP, "%0*d",		TGenericYYMM_len,			v_SaleDiaryMonth);

			// New fields for Trn. 5090 only.
			if (transaction_num == 5090)
			{
				fprintf (outFP, "%0*d",	2,							v_ActionType		);
				fprintf (outFP, "%0*d",	9,							v_tikra_discount	);
				fprintf (outFP, "%0*d",	9,							v_subsidy_amount	);
				fprintf (outFP, "%0*d",	9,							v_del_presc_id		);
				fprintf (outFP, "%0*d",	7,							v_del_pharm_code	);
				fprintf (outFP, "%0*d",	4,							v_DelDiaryMonth		);
				fprintf (outFP, "%0*d",	6,							v_del_ph_presc_id	);
				fprintf (outFP, "%0*d",	8,							v_del_sale_date		);

				// 20 bytes reserved for expansion.
				fprintf (outFP, "00000000000000000000"	);
			}

			fprintf (outFP, "%0*d",		TElectPR_LineNumLen,		v_NumLines		);



			// Write the sales/drugs data to file.
			// Loop until we hit end-of-fetch on the sales available for the
			// requested Diary Month and Sale Date.
			// DonR 23Aug2010: Set Drug Code length conditionally.
			MyDrugCodeLength = (transaction_num == 5090) ? 9 : 5;


			for (;;)
			{
				// Fetch next prescriptions row and test DB errors
				FetchCursor (	MAIN_DB, TR2090_prescription_drugs_cur	);

				Conflict_Test (reStart);

				if (SQLERR_code_cmp (SQLERR_end_of_fetch) == MAC_TRUE)
				{
					break;
				}
				else
				{
					if (SQLERR_error_test ())
					{
						SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR);
						break;
					}
				}


				// If this drug sale was deleted, fetch the deletion row. Note that since
				// there can be (and sometimes is!) more than one deletion row per sale,
				// the SQL has to reference del_presc_drugs to make sure we're looking
				// at the right deletion.
				if (v_Deleted != 0)
				{
					ExecSQL (	MAIN_DB, TR2090_READ_old_deletion_tables,
								&v_DeletionId,			&v_DeleteDate,
								&v_DeleteTime,			&v_DeleteReceiptNum,
								&v_DeleteTerminalId,
								&v_PrescID,				&v_PrescID,
								&v_LargoCode,			END_OF_ARG_LIST			);
				}

				if ((v_Deleted == 0) || (SQLCODE != 0))
				{
					v_DeletionId = v_DeleteDate = v_DeleteTime = v_DeleteReceiptNum = 0;
					v_DeleteTerminalId = 0;
				}


				fprintf (outFP, "%0*d",		TElectPR_LineNumLen,			v_LineNum			);
				fprintf (outFP, "%0*d",		MyDrugCodeLength,				v_LargoCode			);
				fprintf (outFP, "%0*d",		TUnits_len,						v_Units				);
				fprintf (outFP, "%0*d",		TOp_len,						v_Op				);
				fprintf (outFP, "%0*d",		MyDrugCodeLength,				v_LinkToDrug		);
				fprintf (outFP, "%0*d",		TGenericAmount_len,				v_SalePricePerOp	);
				fprintf (outFP, "%0*d",		TGenericAmount_len,				v_PharmPricePerOp	);
				fprintf (outFP, "%0*d",		TMemberParticipatingType_len,	v_ParticipCode		);
				fprintf (outFP, "%0*d",		TGenericAmount_len,				v_FixedPricePerOp	);
				fprintf (outFP, "%0*d",		TMemberDiscountPercent_len,		v_MemberDiscount	);
				fprintf (outFP, "%0*d",		TGenericAmount_len,				v_TotalPrice		);
				fprintf (outFP, "%0*d",		TGenericAmount_len,				v_MemberPtn			);
				fprintf (outFP, "%0*d",		TGenericFlag_len,				v_Deleted			);

				fprintf (outFP, "%0*d",		TRecipeIdentifier_len,			v_DeletionId		);
				fprintf (outFP, "%0*d",		TGenericYYYYMMDD_len,			v_DeleteDate		);
				fprintf (outFP, "%0*d",		TGenericHHMMSS_len,				v_DeleteTime		);
				fprintf (outFP, "%0*d",		TCancleInvoiceNum_len,			v_DeleteReceiptNum	);
				fprintf (outFP, "%0*d",		TTerminalNum_len,				v_DeleteTerminalId	);

				// New fields for Trn. 5090 only.
				if (transaction_num == 5090)
				{
					// WORKINGPOINT: This should be "Deletion Number" - need to check where it should
					// come from.
					fprintf (outFP, "000000"		);
					// 15 bytes reserved for expansion.
					fprintf (outFP, "000000000000000"	);
				}

			}	// Fetch drugs loop.

			CloseCursor (	MAIN_DB, TR2090_prescription_drugs_cur	);

			SQLERR_error_test ();

		}
		while (0);	// One-time loop to avoid GOTO's.


		// Commit the transaction.
		if (reStart == MAC_FALS)	/* Transaction OK then COMMIT	*/
		{
			// DonR 14Jan2020: No point in a commit when we haven't changed anything!
		}
		else
		{
			if (TransactionRestart () != MAC_OK)
			{
				v_ErrorCode = ERR_DATABASE_ERROR;
				break;
			}

			// DonR 26Jul2011: Suppress log message until the fourth attempt fails.
			if (tries > 2)
				GerrLogReturn (GerrId, "Table is locked for the <%d> time", tries);

			sleep (ACCESS_CONFLICT_SLEEP_TIME);

			fclose (outFP);
		}

	}	/* End of retries loop */


	fclose (outFP);


	// Write the name of output-file on string for T-SWITCH.
	*p_outputWritten = sprintf (OBuffer, "%s", fileName);
	*output_type_flg = ANSWER_IN_FILE;

	ssmd_data_ptr->error_code	= v_ErrorCode;
	ssmd_data_ptr->pharmacy_num	= v_PharmNum;
	ssmd_data_ptr->terminal_num	= v_TerminalNum;

	RESTORE_ISOLATION;

	return (RC_SUCCESS);
}
// End of 2090/5090 handler.



/*======================================================================================
||																			            ||
||			 HandlerToMsg_2096															||
||	    Message handler for message 2096 -  usage_instruct download.					||
||												(downloads whole table)					||
||		Marianna 15Oct2020 CR #32620													||
=======================================================================================*/
int HandlerToMsg_2096 (char			*IBuffer,
					   int			TotalInputLen,
					   char			*OBuffer,
					   int			*p_outputWritten,
					   int			*output_type_flg,
					   SSMD_DATA	*ssmd_data_ptr)
{
	// Local declarations
	int				tries;
	int				reStart;
	TErrorCode		err;
	PHARMACY_INFO	Phrm_info;
	ISOLATION_VAR;

	// SQL variables

	// INPUT MESSAGE
	TPharmNum			v_PharmNum;
	TInstituteCode		v_InstituteCode;
	TTerminalNum		v_TerminalNum;

	// RETURN MESSAGE
	// header
	TGenericYYYYMMDD	SysDate;
	TGenericHHMMSS		SysTime;
	TErrorCode			v_ErrorCode			= NO_ERROR;
	int					v_MaxNumToUpdate	= 0;
	char				NumRecsBuf	[9 + 1];	// DonR 04Dec2023 Bug #483305: Change length of number-of-rows from 2 (which is too short) to 9.
	char				*NumRecsInsertPoint	= OBuffer;

	// Database fields.
	short	shape_num;
	char	shape_code			[ 4 + 1];
	char	shape_desc			[25 + 1];
	short	inst_code;
	char	inst_msg			[40 + 1];
	short	inst_seq;
	short	calc_op_flag;
	char	unit_code			[ 3 + 1];
	short	unit_seq;
	char	unit_name			[ 8 + 1];
	char	unit_desc			[25 + 1];
	short	open_od_window;
	short	concentration_flag;
	char	total_unit_name		[10 + 1];
	short	round_units_flag;
	int		update_date;
	int		update_time;
	int		updatedby;
	char	del_flg				[ 1 + 1];


	// Init answer header variables
	REMEMBER_ISOLATION;
	PosInBuff			= IBuffer;
	SysDate				= GetDate ();
	SysTime				= GetTime ();


	// Read request message fields.
	v_PharmNum			= GetLong	(&PosInBuff, TPharmNum_len			); CHECK_ERROR();
	v_InstituteCode		= GetShort	(&PosInBuff, TInstituteCode_len		); CHECK_ERROR();
	v_TerminalNum		= GetShort	(&PosInBuff, TTerminalNum_len		); CHECK_ERROR();

	// Get amount of input not eaten
	ChckInputLen (TotalInputLen - (PosInBuff - IBuffer));
	CHECK_ERROR ();


	// DonR 14Jul2005: Avoid unnecessary lock errors by downgrading "isolation"
	// for this transaction.
	SET_ISOLATION_DIRTY;

	// Retries loop.
	for (tries = 0, reStart = MAC_TRUE, v_ErrorCode = NO_ERROR;	(tries < SQL_UPDATE_RETRIES) && (reStart == MAC_TRUE); tries++)
	{
		reStart = MAC_FALS;

		// One-time loop to avoid GOTO's. Response is sent upon exit from this "loop".
		do
		{
			// Test if pharmacy has been OPENed.
			err = IS_PHARMACY_OPEN (v_PharmNum, v_InstituteCode, &Phrm_info);
			SetErrorVar (&v_ErrorCode, err);
			if (v_ErrorCode != MAC_OK)
			{
				break;
			}

			// Open database cursor.
			DeclareAndOpenCursorInto (	MAIN_DB, TR2096_usage_instruct_cur,
										&shape_num,				&shape_code,		&shape_desc,
										&inst_code,				&inst_msg,			&inst_seq,
										&calc_op_flag,			&unit_code,			&unit_seq,
										&unit_name,				&unit_desc,			&open_od_window,
										&concentration_flag,	&total_unit_name,	&round_units_flag,
										&update_date,			&update_time,		&updatedby,
										&del_flg,				END_OF_ARG_LIST							);

			if (SQLERR_error_test ())
			{
				SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR);
				break;
			}

			// Create the header with data so far.
			nOut =  sprintf (OBuffer,		 "%0*d",	MSG_ID_LEN,				2096			);
			nOut += sprintf (OBuffer + nOut, "%0*d",	MSG_ERROR_CODE_LEN,		MAC_OK			);
			nOut += sprintf (OBuffer + nOut, "%0*d",	TPharmNum_len,			v_PharmNum		);
			nOut += sprintf (OBuffer + nOut, "%0*d",	TInstituteCode_len,		v_InstituteCode	);
			nOut += sprintf (OBuffer + nOut, "%0*d",	TTerminalNum_len,		v_TerminalNum	);
			nOut += sprintf (OBuffer + nOut, "%0*d",	TErrorCode_len,			v_ErrorCode		);
			nOut += sprintf (OBuffer + nOut, "%0*d",	TGenericYYYYMMDD_len,	SysDate			);
			nOut += sprintf (OBuffer + nOut, "%0*d",	TGenericHHMMSS_len,		SysTime			);

			// Save location to write number-of-records value, since it will be updated at the end.
			NumRecsInsertPoint = OBuffer + nOut;

			// DonR 26Sep2023 Bug #483305: Change length of number-of-rows from 2 (which is too short) to 9.
//			nOut += sprintf (OBuffer + nOut, "%0*d",	TElectPR_LineNumLen,	v_MaxNumToUpdate);
			nOut += sprintf (OBuffer + nOut, "%0*d",	9,						v_MaxNumToUpdate);


			// Write the data to file - loop until we hit end-of-fetch.
			for (;;)
			{
				// Fetch and test DB errors
				FetchCursor (	MAIN_DB, TR2096_usage_instruct_cur	);

				Conflict_Test (reStart);

				if (SQLERR_code_cmp (SQLERR_end_of_fetch) == MAC_TRUE)
				{
					break;
				}
				else
				{
					if (SQLERR_error_test ())
					{
						SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR);
						break;
					}
				}

				// Write the how_to_take row we fetched.
				v_MaxNumToUpdate++;

				nOut += sprintf (OBuffer + nOut, "%0*d",	 3,		shape_num		);
				nOut += sprintf (OBuffer + nOut, "%0*d",	 3,		inst_code		);
				nOut += sprintf (OBuffer + nOut, "%-*.*s",	 3,  3,	unit_code		);
				nOut += sprintf (OBuffer + nOut, "%-*.*s",	20, 20,	" "				);	// Reserved for future use.


				// The fields below are not part of the requirement for Trn. 2096 - but they can be enabled if necessary.
//				nOut += sprintf (OBuffer + nOut, "%-*.*s",	 4,  4,	shape_code			);
//				nOut += sprintf (OBuffer + nOut, "%-*.*s",	25, 25,	shape_desc			);
//				nOut += sprintf (OBuffer + nOut, "%-*.*s",	40, 40,	inst_msg			);
//				nOut += sprintf (OBuffer + nOut, "%0*d",	 1,		inst_seq			);
//				nOut += sprintf (OBuffer + nOut, "%0*d",	 1,		calc_op_flag		);
//				nOut += sprintf (OBuffer + nOut, "%0*d",	 1,		unit_seq			);
//				nOut += sprintf (OBuffer + nOut, "%-*.*s",	 3,  3,	unit_name			);
//				nOut += sprintf (OBuffer + nOut, "%-*.*s",	25, 25,	unit_desc			);
//				nOut += sprintf (OBuffer + nOut, "%0*d",	 1,		open_od_window		);
//				nOut += sprintf (OBuffer + nOut, "%0*d",	 1,		concentration_flag	);
//				nOut += sprintf (OBuffer + nOut, "%-*.*s",	10, 10,	total_unit_name		);
//				nOut += sprintf (OBuffer + nOut, "%0*d",	 1,		round_units_flag	);
//				nOut += sprintf (OBuffer + nOut, "%0*d",	???,	update_date			); // Marianna Do we need this here? Don: NO.
//				nOut += sprintf (OBuffer + nOut, "%0*d",	???,	update_time			); // Marianna Do we need this here? Don: NO.
//				nOut += sprintf (OBuffer + nOut, "%0*d",	???,	updatedby			); // Marianna Do we need this here? Don: NO.
//				nOut += sprintf (OBuffer + nOut, "%-*.*s",	 1,  1,	del_flg				);



			}	// DB fetch loop.

			CloseCursor (	MAIN_DB,  TR2096_usage_instruct_cur	);

			SQLERR_error_test ();
		}
		while (0);	// One-time loop to avoid GOTO's.

		// If we've hit a DB error, pause briefly and retry.
		if (reStart != MAC_FALS)	// We've hit a DB error.
		{
			// DonR 26Jul2011: Suppress log message until the fourth attempt fails.
			if (tries > 2)
				GerrLogReturn (GerrId, "Table is locked for the <%d> time", tries);

			sleep (ACCESS_CONFLICT_SLEEP_TIME);
		}	// Pause before retrying.
	}	// End of retries loop.


	// Update record counter in header section of buffer.
	// DonR 04Dec2023 Bug #483305 update: Change length of number-of-rows from 2 (which is too short) to 9.
	sprintf (NumRecsBuf, "%0*d", 9, v_MaxNumToUpdate);
	memcpy (NumRecsInsertPoint, NumRecsBuf, 9);

	// Return the size in bytes of response message.
	*p_outputWritten	= nOut;
	*output_type_flg	= ANSWER_IN_BUFFER;

	ssmd_data_ptr->error_code	= v_ErrorCode;
	ssmd_data_ptr->pharmacy_num	= v_PharmNum;
	ssmd_data_ptr->terminal_num	= v_TerminalNum;

	RESTORE_ISOLATION;

	return (RC_SUCCESS);
}
// End of 2096 handler.



/*======================================================================================
||																			            ||
||			 HandlerToMsg_2101															||
||	    Message handler for message 2101 -  usage_instr_reason_changed download.		||
||												(downloads whole table)					||
||		Marianna 15Oct2020 CR #32620													||
=======================================================================================*/
int HandlerToMsg_2101 (char			*IBuffer,
					   int			TotalInputLen,
					   char			*OBuffer,
					   int			*p_outputWritten,
					   int			*output_type_flg,
					   SSMD_DATA	*ssmd_data_ptr)
{
	// Local declarations
	int				tries;
	int				reStart;
	TErrorCode		err;
	PHARMACY_INFO	Phrm_info;
	ISOLATION_VAR;

	// SQL variables

	// INPUT MESSAGE
	TPharmNum			v_PharmNum;
	TInstituteCode		v_InstituteCode;
	TTerminalNum		v_TerminalNum;

	// RETURN MESSAGE
	// header
	TGenericYYYYMMDD	SysDate;
	TGenericHHMMSS		SysTime;
	TErrorCode			v_ErrorCode			= NO_ERROR;
	int					v_MaxNumToUpdate	= 0;
	char				NumRecsBuf	[4];
	char				*NumRecsInsertPoint	= OBuffer;

	// Database fields.
	short	reason_changed_code;
	char	reason_short_desc		[30 + 1];
	char	reason_long_desc		[50 + 1];
	int 	update_date;
	int		update_time;

	// Init answer header variables
	REMEMBER_ISOLATION;
	PosInBuff			= IBuffer;
	SysDate				= GetDate ();
	SysTime				= GetTime ();


	// Read request message fields.
	v_PharmNum			= GetLong	(&PosInBuff, TPharmNum_len			); CHECK_ERROR();
	v_InstituteCode		= GetShort	(&PosInBuff, TInstituteCode_len		); CHECK_ERROR();
	v_TerminalNum		= GetShort	(&PosInBuff, TTerminalNum_len		); CHECK_ERROR();

	// Get amount of input not eaten
	ChckInputLen (TotalInputLen - (PosInBuff - IBuffer));
	CHECK_ERROR ();


	// DonR 14Jul2005: Avoid unnecessary lock errors by downgrading "isolation"
	// for this transaction.
	SET_ISOLATION_DIRTY;

	// Retries loop.
	for (tries = 0, reStart = MAC_TRUE, v_ErrorCode = NO_ERROR;	(tries < SQL_UPDATE_RETRIES) && (reStart == MAC_TRUE); tries++)
	{
		reStart = MAC_FALS;

		// One-time loop to avoid GOTO's. Response is sent upon exit from this "loop".
		do
		{
			// Test if pharmacy has been OPENed.
			err = IS_PHARMACY_OPEN (v_PharmNum, v_InstituteCode, &Phrm_info);
			SetErrorVar (&v_ErrorCode, err);
			if (v_ErrorCode != MAC_OK)
			{
				break;
			}

			// Open database cursor.
			DeclareAndOpenCursorInto (	MAIN_DB, TR2101_usage_instr_reason_changed_cur,
										&reason_changed_code,	&reason_short_desc,		&reason_long_desc,		
										&update_date,			&update_time,			END_OF_ARG_LIST		);

			if (SQLERR_error_test ())
			{
				SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR);
				break;
			}

			// Create the header with data so far.
			nOut =  sprintf (OBuffer,		 "%0*d",	MSG_ID_LEN,				2101			);
			nOut += sprintf (OBuffer + nOut, "%0*d",	MSG_ERROR_CODE_LEN,		MAC_OK			);
			nOut += sprintf (OBuffer + nOut, "%0*d",	TPharmNum_len,			v_PharmNum		);
			nOut += sprintf (OBuffer + nOut, "%0*d",	TInstituteCode_len,		v_InstituteCode	);
			nOut += sprintf (OBuffer + nOut, "%0*d",	TTerminalNum_len,		v_TerminalNum	);
			nOut += sprintf (OBuffer + nOut, "%0*d",	TErrorCode_len,			v_ErrorCode		);
			nOut += sprintf (OBuffer + nOut, "%0*d",	TGenericYYYYMMDD_len,	SysDate			);
			nOut += sprintf (OBuffer + nOut, "%0*d",	TGenericHHMMSS_len,		SysTime			);

			// Save location to write number-of-records value, since it will be updated at the end.
			NumRecsInsertPoint = OBuffer + nOut;

			nOut += sprintf (OBuffer + nOut, "%0*d",	TElectPR_LineNumLen,	v_MaxNumToUpdate);


			// Write the data to file - loop until we hit end-of-fetch.
			for (;;)
			{
				// Fetch and test DB errors
				FetchCursor (	MAIN_DB, TR2101_usage_instr_reason_changed_cur	);

				Conflict_Test (reStart);

				if (SQLERR_code_cmp (SQLERR_end_of_fetch) == MAC_TRUE)
				{
					break;
				}
				else
				{
					if (SQLERR_error_test ())
					{
						SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR);
						break;
					}
				}

				// Write the how_to_take row we fetched.
				v_MaxNumToUpdate++;

//				WinHebToDosHeb ((unsigned char *) reason_short_desc); Not part of output, at least for now.
				WinHebToDosHeb ((unsigned char *) reason_long_desc);
				
				nOut += sprintf (OBuffer + nOut, "%0*d",	 3,		reason_changed_code	);
				nOut += sprintf (OBuffer + nOut, "%-*.*s",	50, 50,	reason_long_desc	);
				nOut += sprintf (OBuffer + nOut, "%-*.*s",	20, 20,	" "					);	// Reserved for future use.

//				nOut += sprintf (OBuffer + nOut, "%-*.*s",	30, 30,	reason_short_desc		);
//				nOut += sprintf (OBuffer + nOut, "%0*d",	???,	update_date		); // Marianna Do we need this here?  Don: NO.
//				nOut += sprintf (OBuffer + nOut, "%0*d",	???,	update_time		); // Marianna Do we need this here?  Don: NO.

			}	// DB fetch loop.

			CloseCursor (	MAIN_DB,  TR2101_usage_instr_reason_changed_cur	);

			SQLERR_error_test ();
		}
		while (0);	// One-time loop to avoid GOTO's.

		// If we've hit a DB error, pause briefly and retry.
		if (reStart != MAC_FALS)	// We've hit a DB error.
		{
			// DonR 26Jul2011: Suppress log message until the fourth attempt fails.
			if (tries > 2)
				GerrLogReturn (GerrId, "Table is locked for the <%d> time", tries);

			sleep (ACCESS_CONFLICT_SLEEP_TIME);
		}	// Pause before retrying.
	}	// End of retries loop.


	// Update record counter in header section of buffer.
	sprintf (NumRecsBuf, "%0*d", TElectPR_LineNumLen, v_MaxNumToUpdate);
	memcpy (NumRecsInsertPoint, NumRecsBuf, TElectPR_LineNumLen);

	// Return the size in bytes of response message.
	*p_outputWritten	= nOut;
	*output_type_flg	= ANSWER_IN_BUFFER;

	ssmd_data_ptr->error_code	= v_ErrorCode;
	ssmd_data_ptr->pharmacy_num	= v_PharmNum;
	ssmd_data_ptr->terminal_num	= v_TerminalNum;

	RESTORE_ISOLATION;

	return (RC_SUCCESS);
}
// End of 2101 handler.



/*=======================================================================
||																		||
||			 HandlerToMsg_5051											||
||	    Message handler for message 5051 -								||
||			credit_reasons full-table download							||
||																		||
=======================================================================*/
int HandlerToMsg_5051 (char			*IBuffer,
					   int			TotalInputLen,
					   char			*OBuffer,
					   int			*p_outputWritten,
					   int			*output_type_flg,
					   SSMD_DATA	*ssmd_data_ptr)
{
	// Local declarations
	char			fileName [256];
	int				tries;
	int				reStart;
	TErrorCode		err;
	PHARMACY_INFO	Phrm_info;
	FILE			*outFP;
	ISOLATION_VAR;

	// SQL variables

	// INPUT MESSAGE
	TPharmNum						v_PharmNum;
	TInstituteCode					v_InstituteCode;
	TTerminalNum					v_TerminalNum;

	// RETURN MESSAGE
	// header
	TErrorCode						v_ErrorCode;
	TGenericYYYYMMDD				v_FileUpdateDate;
	TGenericHHMMSS					v_FileUpdateTime;
	int								v_MaxNumToUpdate;

	// lines
	short int						v_reason_code;
	char							v_short_desc	[31];	// DonR 09May2017: renamed variable.
	int								v_update_date;
	int								v_update_time;
	char							TableName		[33] = "CreditReasons";	// DonR 19Aug2020: Change from char * to array, since ODBC doesn't like string constants.


	// Select cursor.
	// DonR 09May2017: RK9078P descriptions have been changed from 15/30 characters
	// to 30/50 characters; as a result, we're now sending the short description
	// instead of the long one. The transaction format isn't changing, but variable
	// names are.
	DeclareCursorInto (	MAIN_DB, TR5051_credit_reasons_cur,
						&v_reason_code,		&v_short_desc,
						&v_update_date,		&v_update_time,
						END_OF_ARG_LIST							);


	// Init answer header variables
	REMEMBER_ISOLATION;
	PosInBuff			= IBuffer;
	v_ErrorCode			= NO_ERROR;
	v_FileUpdateDate	= 0;
	v_FileUpdateTime	= 0;
	v_MaxNumToUpdate	= 0;


	// Read request message fields.
	v_PharmNum			= GetLong	(&PosInBuff, TPharmNum_len			); CHECK_ERROR();
	v_InstituteCode		= GetShort	(&PosInBuff, TInstituteCode_len		); CHECK_ERROR();
	v_TerminalNum		= GetShort	(&PosInBuff, TTerminalNum_len		); CHECK_ERROR();

	// Get amount of input not eaten
	ChckInputLen (TotalInputLen - (PosInBuff - IBuffer));
	CHECK_ERROR();


	// Get ready to process the request.
	reStart = MAC_TRUE;

	sprintf (fileName,
			 "%s/credit_reasons_%0*d_%0*d_%0*d.snd",
			 MsgDir,
			 TPharmNum_len,			v_PharmNum,
			 TInstituteCode_len,	v_InstituteCode,
			 TTerminalNum_len,		v_TerminalNum);


	// Retries loop.
	for (tries = 0;	(tries < SQL_UPDATE_RETRIES) && (reStart == MAC_TRUE); tries++)
	{
		// Create new file for response message.
		outFP = fopen (fileName, "w");

		if (outFP == NULL)
		{
			int	err = errno;

			GerrLogReturn (GerrId,
						   "\tCannot open output file '%s'\n\tError (%d) %s\n",
						   fileName,
						   err,
						   strerror(err));

			RESTORE_ISOLATION;
			return (ERR_UNABLE_OPEN_OUTPUT_FILE);
		}

		// Init answer header variables
		v_ErrorCode			= NO_ERROR;
		v_FileUpdateDate	= 0;
		v_FileUpdateTime	= 0;
		v_MaxNumToUpdate	= 0;
		reStart				= MAC_FALS;

		// Avoid unnecessary lock errors by downgrading "isolation"
		// for this transaction.
		SET_ISOLATION_DIRTY;

		// One-time loop to avoid GOTO's. Response is sent upon exit from this "loop".
		do
		{
			// Test if pharmacy has been OPENed.
			err = IS_PHARMACY_OPEN (v_PharmNum, v_InstituteCode, &Phrm_info);
			SetErrorVar (&v_ErrorCode, err);
			if (v_ErrorCode != MAC_OK)
			{
				break;
			}

			// Get the modification date.
			ExecSQL (	MAIN_DB, READ_TablesUpdate,
						&v_FileUpdateDate,	&v_FileUpdateTime,
						&TableName,			END_OF_ARG_LIST		);

			Conflict_Test (reStart);

			if (SQLERR_code_cmp (SQLERR_not_found) == MAC_TRUE)
			{
				v_FileUpdateDate = 0;
				v_FileUpdateTime = 0;
			}
			else
			{
				if (SQLERR_error_test ())
				{
					if (SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR))
					{
						break;
					}
				}
			}


			// Open credit_reasons cursor.
			OpenCursor (	MAIN_DB, TR5051_credit_reasons_cur	);

			if (SQLERR_error_test ())
			{
				SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR);
				break;
			}


			// Create the header with data so far.
			fprintf (outFP, "%0*d",		MSG_ID_LEN,				5052			);
			fprintf (outFP, "%0*d",		MSG_ERROR_CODE_LEN,		MAC_OK			);
			fprintf (outFP, "%0*d",		TPharmNum_len,			v_PharmNum		);
			fprintf (outFP, "%0*d",		TInstituteCode_len,		v_InstituteCode	);
			fprintf (outFP, "%0*d",		TTerminalNum_len,		v_TerminalNum	);
			fprintf (outFP, "%0*d",		TErrorCode_len,			v_ErrorCode		);
			fprintf (outFP, "%0*d",		TGenericYYYYMMDD_len,	v_FileUpdateDate);
			fprintf (outFP, "%0*d",		TGenericHHMMSS_len,		v_FileUpdateTime);
			fprintf (outFP, "%0*d",		TEconPriNumUpdated_len,	v_MaxNumToUpdate);


			// Write the credit_reasons data to file.
			// Loop until we hit end-of-fetch.
			for (;;)
			{
				// Fetch and test DB errors
				FetchCursor (	MAIN_DB, TR5051_credit_reasons_cur	);

				Conflict_Test (reStart);

				if (SQLERR_code_cmp (SQLERR_end_of_fetch) == MAC_TRUE)
				{
					break;
				}
				else
				{
					if (SQLERR_error_test ())
					{
						SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR);
						break;
					}
				}


				// Translate Hebrew text from Windows to DOS.
				WinHebToDosHeb ((unsigned char *)(v_short_desc));


				// Write the credit_reasons row we fetched.
				v_MaxNumToUpdate++;

				fprintf (outFP, "%0*d",		 2,					v_reason_code				);
				fprintf (outFP, "%-*.*s",	30, 30,				v_short_desc				);
				fprintf (outFP, "%0*d",		 2,					0 /* Viewing Sequence */	);
				fprintf (outFP, "%0*d",		 1,					0 /* Severity/NIU */		);
				fprintf (outFP, "%-*.*s",	10, 10,				"0000000000"				);	// Reserved.

			}	// Fetch credit_reasons loop.

			CloseCursor (	MAIN_DB, TR5051_credit_reasons_cur	);

			SQLERR_error_test ();

		}
		while (0);	// One-time loop to avoid GOTO's.


		// Commit the transaction.
		if (reStart == MAC_FALS)	/* Transaction OK then COMMIT	*/
		{
			// DonR 14Jan2020: No point in a commit, since we haven't changed anything!
		}
		else
		{
			if (TransactionRestart () != MAC_OK)
			{
				v_ErrorCode = ERR_DATABASE_ERROR;
				break;
			}

			// DonR 26Jul2011: Suppress log message until the fourth attempt fails.
			if (tries > 2)
				GerrLogReturn (GerrId, "Table is locked for the <%d> time", tries);

			sleep (ACCESS_CONFLICT_SLEEP_TIME);

			fclose (outFP);
		}

	}	/* End of retries loop */


	// REWRITE MESSAGE HEADER.
	rewind (outFP);

	fprintf (outFP, "%0*d",		MSG_ID_LEN,				5052			);
	fprintf (outFP, "%0*d",		MSG_ERROR_CODE_LEN,		MAC_OK			);
	fprintf (outFP, "%0*d",		TPharmNum_len,			v_PharmNum		);
	fprintf (outFP, "%0*d",		TInstituteCode_len,		v_InstituteCode	);
	fprintf (outFP, "%0*d",		TTerminalNum_len,		v_TerminalNum	);
	fprintf (outFP, "%0*d",		TErrorCode_len,			v_ErrorCode		);
	fprintf (outFP, "%0*d",		TGenericYYYYMMDD_len,	v_FileUpdateDate);
	fprintf (outFP, "%0*d",		TGenericHHMMSS_len,		v_FileUpdateTime);
	fprintf (outFP, "%0*d",		TEconPriNumUpdated_len,	v_MaxNumToUpdate);

	fclose (outFP);


	// Write the name of output-file on string for T-SWITCH.
	*p_outputWritten = sprintf (OBuffer, "%s", fileName);
	*output_type_flg = ANSWER_IN_FILE;

	ssmd_data_ptr->error_code	= v_ErrorCode;
	ssmd_data_ptr->pharmacy_num	= v_PharmNum;
	ssmd_data_ptr->terminal_num	= v_TerminalNum;

	RESTORE_ISOLATION;

	return (RC_SUCCESS);
}
// End of 5051 handler.



/*=======================================================================
||																		||
||			 HandlerToMsg_5053											||
||	    Message handler for message 5053 -								||
||			subsidy_messages full-table download						||
||																		||
=======================================================================*/
int HandlerToMsg_5053 (char			*IBuffer,
					   int			TotalInputLen,
					   char			*OBuffer,
					   int			*p_outputWritten,
					   int			*output_type_flg,
					   SSMD_DATA	*ssmd_data_ptr)
{
	// Local declarations
	char			fileName [256];
	int				tries;
	int				reStart;
	TErrorCode		err;
	PHARMACY_INFO	Phrm_info;
	FILE			*outFP;
	ISOLATION_VAR;

	// SQL variables

	// INPUT MESSAGE
	TPharmNum						v_PharmNum;
	TInstituteCode					v_InstituteCode;
	TTerminalNum					v_TerminalNum;

	// RETURN MESSAGE
	// header
	TErrorCode						v_ErrorCode;
	TGenericYYYYMMDD				v_FileUpdateDate;
	TGenericHHMMSS					v_FileUpdateTime;
	int								v_MaxNumToUpdate;

	// lines
	short int						v_subsidy_code;
	char							v_short_desc	[11];
	int								v_update_date;
	int								v_update_time;
	char							TableName		[33] = "SubsidyMessages";	// DonR 19Aug2020: Change from char * to array, since ODBC doesn't like string constants.


	// Init answer header variables
	REMEMBER_ISOLATION;
	PosInBuff			= IBuffer;
	v_ErrorCode			= NO_ERROR;
	v_FileUpdateDate	= 0;
	v_FileUpdateTime	= 0;
	v_MaxNumToUpdate	= 0;


	// Read request message fields.
	v_PharmNum			= GetLong	(&PosInBuff, TPharmNum_len			); CHECK_ERROR();
	v_InstituteCode		= GetShort	(&PosInBuff, TInstituteCode_len		); CHECK_ERROR();
	v_TerminalNum		= GetShort	(&PosInBuff, TTerminalNum_len		); CHECK_ERROR();

	// Get amount of input not eaten
	ChckInputLen (TotalInputLen - (PosInBuff - IBuffer));
	CHECK_ERROR ();


	// Get ready to process the request.
	reStart = MAC_TRUE;

	sprintf (fileName,
			 "%s/subsidy_messages_%0*d_%0*d_%0*d.snd",
			 MsgDir,
			 TPharmNum_len,			v_PharmNum,
			 TInstituteCode_len,	v_InstituteCode,
			 TTerminalNum_len,		v_TerminalNum);


	// Retries loop.
	for (tries = 0;	(tries < SQL_UPDATE_RETRIES) && (reStart == MAC_TRUE); tries++)
	{
		// Create new file for response message.
		outFP = fopen (fileName, "w");

		if (outFP == NULL)
		{
			int	err = errno;

			GerrLogReturn (GerrId,
						   "\tCannot open output file '%s'\n\tError (%d) %s\n",
						   fileName,
						   err,
						   strerror(err));

			RESTORE_ISOLATION;
			return (ERR_UNABLE_OPEN_OUTPUT_FILE);
		}

		// Init answer header variables
		v_ErrorCode			= NO_ERROR;
		v_FileUpdateDate	= 0;
		v_FileUpdateTime	= 0;
		v_MaxNumToUpdate	= 0;
		reStart				= MAC_FALS;

		// Avoid unnecessary lock errors by downgrading "isolation"
		// for this transaction.
		SET_ISOLATION_DIRTY;

		// One-time loop to avoid GOTO's. Response is sent upon exit from this "loop".
		do
		{
			// Test if pharmacy has been OPENed.
			err = IS_PHARMACY_OPEN (v_PharmNum, v_InstituteCode, &Phrm_info);
			SetErrorVar (&v_ErrorCode, err);
			if (v_ErrorCode != MAC_OK)
			{
				break;
			}

			// Get the modification date.
			ExecSQL (	MAIN_DB, READ_TablesUpdate,
						&v_FileUpdateDate,	&v_FileUpdateTime,
						&TableName,			END_OF_ARG_LIST		);

			Conflict_Test (reStart);

			if (SQLERR_code_cmp (SQLERR_not_found) == MAC_TRUE)
			{
				v_FileUpdateDate = 0;
				v_FileUpdateTime = 0;
			}
			else
			{
				if (SQLERR_error_test ())
				{
					if (SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR))
					{
						break;
					}
				}
			}


			// Open subsidy_messages cursor.
			DeclareAndOpenCursorInto (	MAIN_DB, TR5053_subsidy_messages_cur,
										&v_subsidy_code,	&v_short_desc,
										&v_update_date,		&v_update_time,
										END_OF_ARG_LIST							);

			if (SQLERR_error_test ())
			{
				SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR);
				break;
			}


			// Create the header with data so far.
			fprintf (outFP, "%0*d",		MSG_ID_LEN,				5054			);
			fprintf (outFP, "%0*d",		MSG_ERROR_CODE_LEN,		MAC_OK			);
			fprintf (outFP, "%0*d",		TPharmNum_len,			v_PharmNum		);
			fprintf (outFP, "%0*d",		TInstituteCode_len,		v_InstituteCode	);
			fprintf (outFP, "%0*d",		TTerminalNum_len,		v_TerminalNum	);
			fprintf (outFP, "%0*d",		TErrorCode_len,			v_ErrorCode		);
			fprintf (outFP, "%0*d",		TGenericYYYYMMDD_len,	v_FileUpdateDate);
			fprintf (outFP, "%0*d",		TGenericHHMMSS_len,		v_FileUpdateTime);
			fprintf (outFP, "%0*d",		TEconPriNumUpdated_len,	v_MaxNumToUpdate);


			// Write the subsidy_messages data to file.
			// Loop until we hit end-of-fetch.
			for (;;)
			{
				// Fetch and test DB errors
				FetchCursor (	MAIN_DB, TR5053_subsidy_messages_cur	);

				Conflict_Test (reStart);

				if (SQLERR_code_cmp (SQLERR_end_of_fetch) == MAC_TRUE)
				{
					break;
				}
				else
				{
					if (SQLERR_error_test ())
					{
						SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR);
						break;
					}
				}


				// Translate Hebrew text from Windows to DOS.
				WinHebToDosHeb ((unsigned char *)(v_short_desc));


				// Write the subsidy_messages row we fetched.
				v_MaxNumToUpdate++;

				fprintf (outFP, "%0*d",		 3,					v_subsidy_code				);
				fprintf (outFP, "%-*.*s",	10, 10,				v_short_desc				);
				fprintf (outFP, "%-*.*s",	10, 10,				"0000000000"				);	// Reserved.

			}	// Fetch subsidy_messages loop.

			CloseCursor (	MAIN_DB, TR5053_subsidy_messages_cur	);

			SQLERR_error_test ();

		}
		while (0);	// One-time loop to avoid GOTO's.


		// Commit the transaction.
		if (reStart == MAC_FALS)	/* Transaction OK then COMMIT	*/
		{
			// DonR 14Jan2020: No point in a commit, since we haven't changed anything!
		}
		else
		{
			if (TransactionRestart () != MAC_OK)
			{
				v_ErrorCode = ERR_DATABASE_ERROR;
				break;
			}

			// DonR 26Jul2011: Suppress log message until the fourth attempt fails.
			if (tries > 2)
				GerrLogReturn (GerrId, "Table is locked for the <%d> time", tries);

			sleep (ACCESS_CONFLICT_SLEEP_TIME);

			fclose (outFP);
		}

	}	/* End of retries loop */


	// REWRITE MESSAGE HEADER.
	rewind (outFP);

	fprintf (outFP, "%0*d",		MSG_ID_LEN,				5054			);
	fprintf (outFP, "%0*d",		MSG_ERROR_CODE_LEN,		MAC_OK			);
	fprintf (outFP, "%0*d",		TPharmNum_len,			v_PharmNum		);
	fprintf (outFP, "%0*d",		TInstituteCode_len,		v_InstituteCode	);
	fprintf (outFP, "%0*d",		TTerminalNum_len,		v_TerminalNum	);
	fprintf (outFP, "%0*d",		TErrorCode_len,			v_ErrorCode		);
	fprintf (outFP, "%0*d",		TGenericYYYYMMDD_len,	v_FileUpdateDate);
	fprintf (outFP, "%0*d",		TGenericHHMMSS_len,		v_FileUpdateTime);
	fprintf (outFP, "%0*d",		TEconPriNumUpdated_len,	v_MaxNumToUpdate);

	fclose (outFP);


	// Write the name of output-file on string for T-SWITCH.
	*p_outputWritten = sprintf (OBuffer, "%s", fileName);
	*output_type_flg = ANSWER_IN_FILE;

	ssmd_data_ptr->error_code	= v_ErrorCode;
	ssmd_data_ptr->pharmacy_num	= v_PharmNum;
	ssmd_data_ptr->terminal_num	= v_TerminalNum;

	RESTORE_ISOLATION;

	return (RC_SUCCESS);
}
// End of 5053 handler.



/*=======================================================================
||																		||
||			 HandlerToMsg_5055											||
||	    Message handler for message 5055 -								||
||			tikra_type full-table download								||
||																		||
=======================================================================*/
int HandlerToMsg_5055 (char			*IBuffer,
					   int			TotalInputLen,
					   char			*OBuffer,
					   int			*p_outputWritten,
					   int			*output_type_flg,
					   SSMD_DATA	*ssmd_data_ptr)
{
	// Local declarations
	char			fileName [256];
	int				tries;
	int				reStart;
	TErrorCode		err;
	PHARMACY_INFO	Phrm_info;
	FILE			*outFP;
	ISOLATION_VAR;

	// SQL variables

	// INPUT MESSAGE
	TPharmNum						v_PharmNum;
	TInstituteCode					v_InstituteCode;
	TTerminalNum					v_TerminalNum;

	// RETURN MESSAGE
	// header
	TErrorCode						v_ErrorCode;
	TGenericYYYYMMDD				v_FileUpdateDate;
	TGenericHHMMSS					v_FileUpdateTime;
	int								v_MaxNumToUpdate;

	// lines
	char							v_tikra_code;
	char							v_short_desc	[16];
	int								v_update_date;
	int								v_update_time;
	char							TableName		[33] = "TikraType";	// DonR 19Aug2020: Change from char * to array, since ODBC doesn't like string constants.


	// Init answer header variables
	REMEMBER_ISOLATION;
	PosInBuff			= IBuffer;
	v_ErrorCode			= NO_ERROR;
	v_FileUpdateDate	= 0;
	v_FileUpdateTime	= 0;
	v_MaxNumToUpdate	= 0;


	// Read request message fields.
	v_PharmNum			= GetLong	(&PosInBuff, TPharmNum_len			); CHECK_ERROR();
	v_InstituteCode		= GetShort	(&PosInBuff, TInstituteCode_len		); CHECK_ERROR();
	v_TerminalNum		= GetShort	(&PosInBuff, TTerminalNum_len		); CHECK_ERROR();

	// Get amount of input not eaten
	ChckInputLen (TotalInputLen - (PosInBuff - IBuffer));
	CHECK_ERROR ();


	// Get ready to process the request.
	reStart = MAC_TRUE;

	sprintf (fileName,
			 "%s/tikra_type_%0*d_%0*d_%0*d.snd",
			 MsgDir,
			 TPharmNum_len,			v_PharmNum,
			 TInstituteCode_len,	v_InstituteCode,
			 TTerminalNum_len,		v_TerminalNum);


	// Retries loop.
	for (tries = 0;	(tries < SQL_UPDATE_RETRIES) && (reStart == MAC_TRUE); tries++)
	{
		// Create new file for response message.
		outFP = fopen (fileName, "w");

		if (outFP == NULL)
		{
			int	err = errno;

			GerrLogReturn (GerrId,
						   "\tCannot open output file '%s'\n\tError (%d) %s\n",
						   fileName,
						   err,
						   strerror(err));

			RESTORE_ISOLATION;
			return (ERR_UNABLE_OPEN_OUTPUT_FILE);
		}

		// Init answer header variables
		v_ErrorCode			= NO_ERROR;
		v_FileUpdateDate	= 0;
		v_FileUpdateTime	= 0;
		v_MaxNumToUpdate	= 0;
		reStart				= MAC_FALS;

		// Avoid unnecessary lock errors by downgrading "isolation"
		// for this transaction.
		SET_ISOLATION_DIRTY;

		// One-time loop to avoid GOTO's. Response is sent upon exit from this "loop".
		do
		{
			// Test if pharmacy has been OPENed.
			err = IS_PHARMACY_OPEN (v_PharmNum, v_InstituteCode, &Phrm_info);
			SetErrorVar (&v_ErrorCode, err);
			if (v_ErrorCode != MAC_OK)
			{
				break;
			}

			// Get the modification date.
			ExecSQL (	MAIN_DB, READ_TablesUpdate,
						&v_FileUpdateDate,	&v_FileUpdateTime,
						&TableName,			END_OF_ARG_LIST		);

			Conflict_Test (reStart);

			if (SQLERR_code_cmp (SQLERR_not_found) == MAC_TRUE)
			{
				v_FileUpdateDate = 0;
				v_FileUpdateTime = 0;
			}
			else
			{
				if (SQLERR_error_test ())
				{
					if (SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR))
					{
						break;
					}
				}
			}


			// Open tikra_type cursor.
			DeclareAndOpenCursorInto (	MAIN_DB, TR5055_tikra_type_cur,
										&v_tikra_code,		&v_short_desc,
										&v_update_date,		&v_update_time,
										END_OF_ARG_LIST							);

			if (SQLERR_error_test ())
			{
				SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR);
				break;
			}


			// Create the header with data so far.
			fprintf (outFP, "%0*d",		MSG_ID_LEN,				5056			);
			fprintf (outFP, "%0*d",		MSG_ERROR_CODE_LEN,		MAC_OK			);
			fprintf (outFP, "%0*d",		TPharmNum_len,			v_PharmNum		);
			fprintf (outFP, "%0*d",		TInstituteCode_len,		v_InstituteCode	);
			fprintf (outFP, "%0*d",		TTerminalNum_len,		v_TerminalNum	);
			fprintf (outFP, "%0*d",		TErrorCode_len,			v_ErrorCode		);
			fprintf (outFP, "%0*d",		TGenericYYYYMMDD_len,	v_FileUpdateDate);
			fprintf (outFP, "%0*d",		TGenericHHMMSS_len,		v_FileUpdateTime);
			fprintf (outFP, "%0*d",		TEconPriNumUpdated_len,	v_MaxNumToUpdate);


			// Write the tikra_type data to file.
			// Loop until we hit end-of-fetch.
			for (;;)
			{
				// Fetch and test DB errors
				FetchCursor (	MAIN_DB, TR5055_tikra_type_cur	);

				Conflict_Test (reStart);

				if (SQLERR_code_cmp (SQLERR_end_of_fetch) == MAC_TRUE)
				{
					break;
				}
				else
				{
					if (SQLERR_error_test ())
					{
						SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR);
						break;
					}
				}


				// Translate Hebrew text from Windows to DOS.
				WinHebToDosHeb ((unsigned char *)(v_short_desc));


				// Write the tikra_type row we fetched.
				v_MaxNumToUpdate++;

				fprintf (outFP, "%c",							v_tikra_code);
				fprintf (outFP, "%-*.*s",	15, 15,				v_short_desc);
				fprintf (outFP, "%-*.*s",	10, 10,				"0000000000"				);	// Reserved.

			}	// Fetch tikra_type loop.

			CloseCursor (	MAIN_DB, TR5055_tikra_type_cur	);

			SQLERR_error_test ();

		}
		while (0);	// One-time loop to avoid GOTO's.


		// Commit the transaction.
		if (reStart == MAC_FALS)	/* Transaction OK then COMMIT	*/
		{
			// DonR 14Jan2020: No point in a commit, since we haven't changed anything!
		}
		else
		{
			if (TransactionRestart () != MAC_OK)
			{
				v_ErrorCode = ERR_DATABASE_ERROR;
				break;
			}

			// DonR 26Jul2011: Suppress log message until the fourth attempt fails.
			if (tries > 2)
				GerrLogReturn (GerrId, "Table is locked for the <%d> time", tries);

			sleep (ACCESS_CONFLICT_SLEEP_TIME);

			fclose (outFP);
		}

	}	/* End of retries loop */


	// REWRITE MESSAGE HEADER.
	rewind (outFP);

	fprintf (outFP, "%0*d",		MSG_ID_LEN,				5056			);
	fprintf (outFP, "%0*d",		MSG_ERROR_CODE_LEN,		MAC_OK			);
	fprintf (outFP, "%0*d",		TPharmNum_len,			v_PharmNum		);
	fprintf (outFP, "%0*d",		TInstituteCode_len,		v_InstituteCode	);
	fprintf (outFP, "%0*d",		TTerminalNum_len,		v_TerminalNum	);
	fprintf (outFP, "%0*d",		TErrorCode_len,			v_ErrorCode		);
	fprintf (outFP, "%0*d",		TGenericYYYYMMDD_len,	v_FileUpdateDate);
	fprintf (outFP, "%0*d",		TGenericHHMMSS_len,		v_FileUpdateTime);
	fprintf (outFP, "%0*d",		TEconPriNumUpdated_len,	v_MaxNumToUpdate);

	fclose (outFP);


	// Write the name of output-file on string for T-SWITCH.
	*p_outputWritten = sprintf (OBuffer, "%s", fileName);
	*output_type_flg = ANSWER_IN_FILE;

	ssmd_data_ptr->error_code	= v_ErrorCode;
	ssmd_data_ptr->pharmacy_num	= v_PharmNum;
	ssmd_data_ptr->terminal_num	= v_TerminalNum;

	RESTORE_ISOLATION;

	return (RC_SUCCESS);
}
// End of 5055 handler.



/*=======================================================================
||																		||
||			 HandlerToMsg_5057											||
||	    Message handler for message 5057 -								||
||			hmo_membership full-table download							||
||																		||
=======================================================================*/
int HandlerToMsg_5057 (char			*IBuffer,
					   int			TotalInputLen,
					   char			*OBuffer,
					   int			*p_outputWritten,
					   int			*output_type_flg,
					   SSMD_DATA	*ssmd_data_ptr)
{
	// Local declarations
	char			fileName [256];
	int				tries;
	int				reStart;
	TErrorCode		err;
	PHARMACY_INFO	Phrm_info;
	FILE			*outFP;
	ISOLATION_VAR;

	// SQL variables

	// INPUT MESSAGE
	TPharmNum						v_PharmNum;
	TInstituteCode					v_InstituteCode;
	TTerminalNum					v_TerminalNum;

	// RETURN MESSAGE
	// header
	TErrorCode						v_ErrorCode;
	TGenericYYYYMMDD				v_FileUpdateDate;
	TGenericHHMMSS					v_FileUpdateTime;
	int								v_MaxNumToUpdate;

	// lines
	short int						v_organization;
	char							v_description	[16];
	int								v_update_date;
	int								v_update_time;
	char							TableName		[33] = "HMO_Membership";	// DonR 19Aug2020: Change from char * to array, since ODBC doesn't like string constants.


	// Init answer header variables
	REMEMBER_ISOLATION;
	PosInBuff			= IBuffer;
	v_ErrorCode			= NO_ERROR;
	v_FileUpdateDate	= 0;
	v_FileUpdateTime	= 0;
	v_MaxNumToUpdate	= 0;


	// Read request message fields.
	v_PharmNum			= GetLong	(&PosInBuff, TPharmNum_len			); CHECK_ERROR();
	v_InstituteCode		= GetShort	(&PosInBuff, TInstituteCode_len		); CHECK_ERROR();
	v_TerminalNum		= GetShort	(&PosInBuff, TTerminalNum_len		); CHECK_ERROR();

	// Get amount of input not eaten
	ChckInputLen (TotalInputLen - (PosInBuff - IBuffer));
	CHECK_ERROR();


	// Get ready to process the request.
	reStart = MAC_TRUE;

	sprintf (fileName,
			 "%s/hmo_membership_%0*d_%0*d_%0*d.snd",
			 MsgDir,
			 TPharmNum_len,			v_PharmNum,
			 TInstituteCode_len,	v_InstituteCode,
			 TTerminalNum_len,		v_TerminalNum);


	// Retries loop.
	for (tries = 0;	(tries < SQL_UPDATE_RETRIES) && (reStart == MAC_TRUE); tries++)
	{
		// Create new file for response message.
		outFP = fopen (fileName, "w");

		if (outFP == NULL)
		{
			int	err = errno;

			GerrLogReturn (GerrId,
						   "\tCannot open output file '%s'\n\tError (%d) %s\n",
						   fileName,
						   err,
						   strerror(err));

			RESTORE_ISOLATION;
			return (ERR_UNABLE_OPEN_OUTPUT_FILE);
		}

		// Init answer header variables
		v_ErrorCode			= NO_ERROR;
		v_FileUpdateDate	= 0;
		v_FileUpdateTime	= 0;
		v_MaxNumToUpdate	= 0;
		reStart				= MAC_FALS;

		// Avoid unnecessary lock errors by downgrading "isolation"
		// for this transaction.
		SET_ISOLATION_DIRTY;

		// One-time loop to avoid GOTO's. Response is sent upon exit from this "loop".
		do
		{
			// Test if pharmacy has been OPENed.
			err = IS_PHARMACY_OPEN (v_PharmNum, v_InstituteCode, &Phrm_info);
			SetErrorVar (&v_ErrorCode, err);
			if (v_ErrorCode != MAC_OK)
			{
				break;
			}

			// Get the modification date.
			ExecSQL (	MAIN_DB, READ_TablesUpdate,
						&v_FileUpdateDate,	&v_FileUpdateTime,
						&TableName,			END_OF_ARG_LIST		);

			Conflict_Test (reStart);

			if (SQLERR_code_cmp (SQLERR_not_found) == MAC_TRUE)
			{
				v_FileUpdateDate = 0;
				v_FileUpdateTime = 0;
			}
			else
			{
				if (SQLERR_error_test ())
				{
					if (SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR))
					{
						break;
					}
				}
			}


			// Open hmo_membership cursor.
			DeclareAndOpenCursorInto (	MAIN_DB, TR5057_hmo_membership_cur,
										&v_organization,	&v_description,
										&v_update_date,		&v_update_time,
										END_OF_ARG_LIST							);

			if (SQLERR_error_test ())
			{
				SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR);
				break;
			}


			// Create the header with data so far.
			fprintf (outFP, "%0*d",		MSG_ID_LEN,				5058			);
			fprintf (outFP, "%0*d",		MSG_ERROR_CODE_LEN,		MAC_OK			);
			fprintf (outFP, "%0*d",		TPharmNum_len,			v_PharmNum		);
			fprintf (outFP, "%0*d",		TInstituteCode_len,		v_InstituteCode	);
			fprintf (outFP, "%0*d",		TTerminalNum_len,		v_TerminalNum	);
			fprintf (outFP, "%0*d",		TErrorCode_len,			v_ErrorCode		);
			fprintf (outFP, "%0*d",		TGenericYYYYMMDD_len,	v_FileUpdateDate);
			fprintf (outFP, "%0*d",		TGenericHHMMSS_len,		v_FileUpdateTime);
			fprintf (outFP, "%0*d",		TEconPriNumUpdated_len,	v_MaxNumToUpdate);


			// Write the hmo_membership data to file.
			// Loop until we hit end-of-fetch.
			for (;;)
			{
				// Fetch and test DB errors
				FetchCursor (	MAIN_DB, TR5057_hmo_membership_cur	);

				Conflict_Test (reStart);

				if (SQLERR_code_cmp (SQLERR_end_of_fetch) == MAC_TRUE)
				{
					break;
				}
				else
				{
					if (SQLERR_error_test ())
					{
						SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR);
						break;
					}
				}


				// Translate Hebrew text from Windows to DOS.
				WinHebToDosHeb ((unsigned char *)(v_description));


				// Write the hmo_membership row we fetched.
				v_MaxNumToUpdate++;

				fprintf (outFP, "%0*d",		 2,					v_organization				);
				fprintf (outFP, "%-*.*s",	15, 15,				v_description				);
				fprintf (outFP, "%-*.*s",	10, 10,				"0000000000"				);	// Reserved.

			}	// Fetch hmo_membership loop.

			CloseCursor (	MAIN_DB, TR5057_hmo_membership_cur	);

			SQLERR_error_test ();

		}
		while (0);	// One-time loop to avoid GOTO's.


		// Commit the transaction.
		if (reStart == MAC_FALS)	/* Transaction OK then COMMIT	*/
		{
			// DonR 14Jan2020: No point in a commit, since we haven't changed anything!
		}
		else
		{
			if (TransactionRestart () != MAC_OK)
			{
				v_ErrorCode = ERR_DATABASE_ERROR;
				break;
			}

			// DonR 26Jul2011: Suppress log message until the fourth attempt fails.
			if (tries > 2)
				GerrLogReturn (GerrId, "Table is locked for the <%d> time", tries);

			sleep (ACCESS_CONFLICT_SLEEP_TIME);

			fclose (outFP);
		}

	}	/* End of retries loop */


	// REWRITE MESSAGE HEADER.
	rewind (outFP);

	fprintf (outFP, "%0*d",		MSG_ID_LEN,				5058			);
	fprintf (outFP, "%0*d",		MSG_ERROR_CODE_LEN,		MAC_OK			);
	fprintf (outFP, "%0*d",		TPharmNum_len,			v_PharmNum		);
	fprintf (outFP, "%0*d",		TInstituteCode_len,		v_InstituteCode	);
	fprintf (outFP, "%0*d",		TTerminalNum_len,		v_TerminalNum	);
	fprintf (outFP, "%0*d",		TErrorCode_len,			v_ErrorCode		);
	fprintf (outFP, "%0*d",		TGenericYYYYMMDD_len,	v_FileUpdateDate);
	fprintf (outFP, "%0*d",		TGenericHHMMSS_len,		v_FileUpdateTime);
	fprintf (outFP, "%0*d",		TEconPriNumUpdated_len,	v_MaxNumToUpdate);

	fclose (outFP);


	// Write the name of output-file on string for T-SWITCH.
	*p_outputWritten = sprintf (OBuffer, "%s", fileName);
	*output_type_flg = ANSWER_IN_FILE;

	ssmd_data_ptr->error_code	= v_ErrorCode;
	ssmd_data_ptr->pharmacy_num	= v_PharmNum;
	ssmd_data_ptr->terminal_num	= v_TerminalNum;

	RESTORE_ISOLATION;

	return (RC_SUCCESS);
}
// End of 5057 handler.


/*=======================================================================
||																		||
||			 HandlerToMsg_5059											||
||	    Message handler for message 5059 -								||
||			virtual_store_reason_texts full-table download				||
||																		||
=======================================================================*/
int HandlerToMsg_5059 (char			*IBuffer,
					   int			TotalInputLen,
					   char			*OBuffer,
					   int			*p_outputWritten,
					   int			*output_type_flg,
					   SSMD_DATA	*ssmd_data_ptr)
{
	// Local declarations
	char				fileName [256];
	int					tries;
	int					reStart;
	TErrorCode			err;
	PHARMACY_INFO		Phrm_info;
	FILE				*outFP;
	ISOLATION_VAR;

	// SQL variables

	// INPUT MESSAGE
	TPharmNum			v_PharmNum;
	TInstituteCode		v_InstituteCode;
	TTerminalNum		v_TerminalNum;

	// RETURN MESSAGE
	// header
	TErrorCode			v_ErrorCode;
	TGenericYYYYMMDD	v_FileUpdateDate;
	TGenericHHMMSS		v_FileUpdateTime;
	int					v_MaxNumToUpdate;

	// lines
	short				v_reason_code;
	char 				v_reason_text		[100];
	short				v_displaytToMember;
	char				v_pharmacyText		[100];
	short				v_allowOnlineOrder;
	short				v_allowPickup;
	short				v_allowDelivery;
	char				TableName			[ 33] = "virtual_store_reason_texts";	// DonR 19Aug2020: Change from char * to array, since ODBC doesn't like string constants.


	// Init answer header variables
	REMEMBER_ISOLATION;
	PosInBuff			= IBuffer;
	v_ErrorCode			= NO_ERROR;
	v_FileUpdateDate	= 0;
	v_FileUpdateTime	= 0;
	v_MaxNumToUpdate	= 0;


	// Read request message fields.
	v_PharmNum			= GetLong	(&PosInBuff, TPharmNum_len			); CHECK_ERROR();
	v_InstituteCode		= GetShort	(&PosInBuff, TInstituteCode_len		); CHECK_ERROR();
	v_TerminalNum		= GetShort	(&PosInBuff, TTerminalNum_len		); CHECK_ERROR();

	// Get amount of input not eaten
	ChckInputLen (TotalInputLen - (PosInBuff - IBuffer));
	CHECK_ERROR ();


	// Get ready to process the request.
	reStart = MAC_TRUE;

	sprintf (fileName,
			 "%s/virtual_store_reason_texts_%0*d_%0*d_%0*d.snd",
			 MsgDir,
			 TPharmNum_len,			v_PharmNum,
			 TInstituteCode_len,	v_InstituteCode,
			 TTerminalNum_len,		v_TerminalNum);


	// Retries loop.
	for (tries = 0;	(tries < SQL_UPDATE_RETRIES) && (reStart == MAC_TRUE); tries++)
	{
		// Create new file for response message.
		outFP = fopen (fileName, "w");

		if (outFP == NULL)
		{
			int	err = errno;

			GerrLogReturn (GerrId,
						   "\tCannot open output file '%s'\n\tError (%d) %s\n",
						   fileName,
						   err,
						   strerror(err));

			RESTORE_ISOLATION;
			return (ERR_UNABLE_OPEN_OUTPUT_FILE);
		}

		// Init answer header variables
		v_ErrorCode			= NO_ERROR;
		v_FileUpdateDate	= 0;
		v_FileUpdateTime	= 0;
		v_MaxNumToUpdate	= 0;
		reStart				= MAC_FALS;

		// Avoid unnecessary lock errors by downgrading "isolation"
		// for this transaction.
		SET_ISOLATION_DIRTY;

		// One-time loop to avoid GOTO's. Response is sent upon exit from this "loop".
		do
		{
			// Test if pharmacy has been OPENed.
			err = IS_PHARMACY_OPEN (v_PharmNum, v_InstituteCode, &Phrm_info);
			SetErrorVar (&v_ErrorCode, err);
			if (v_ErrorCode != MAC_OK)
			{
				break;
			}

			// Get the modification date.
			ExecSQL (	MAIN_DB, READ_TablesUpdate,
						&v_FileUpdateDate,	&v_FileUpdateTime,
						&TableName,			END_OF_ARG_LIST		);

			Conflict_Test (reStart);

			if (SQLERR_code_cmp (SQLERR_not_found) == MAC_TRUE)
			{
				v_FileUpdateDate = GetDate(); //Marianna 08Nov2021 change from 0 to current date
				v_FileUpdateTime = GetTime(); //Marianna 08Nov2021 change from 0 to current time
			}
			else
			{
				if (SQLERR_error_test ())
				{
					if (SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR))
					{
						break;
					}
				}
			}


			// Open virtual_store_reason_texts cursor.
			DeclareAndOpenCursorInto (	MAIN_DB, TR5059_virtual_store_reason_texts_cur,
										&v_reason_code		,	&v_reason_text,
										&v_displaytToMember	,	&v_pharmacyText,
										&v_allowOnlineOrder	, 	&v_allowPickup,
										&v_allowDelivery	, 	END_OF_ARG_LIST				);

			if (SQLERR_error_test ())
			{
				SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR);
				break;
			}


			// Create the header with data so far.
			fprintf (outFP, "%0*d",		MSG_ID_LEN,				5060			);
			fprintf (outFP, "%0*d",		MSG_ERROR_CODE_LEN,		MAC_OK			);
			fprintf (outFP, "%0*d",		TPharmNum_len,			v_PharmNum		);
			fprintf (outFP, "%0*d",		TInstituteCode_len,		v_InstituteCode	);
			fprintf (outFP, "%0*d",		TTerminalNum_len,		v_TerminalNum	);
			fprintf (outFP, "%0*d",		TErrorCode_len,			v_ErrorCode		);
			fprintf (outFP, "%0*d",		TGenericYYYYMMDD_len,	v_FileUpdateDate);
			fprintf (outFP, "%0*d",		TGenericHHMMSS_len,		v_FileUpdateTime);
			fprintf (outFP, "%0*d",		3,						v_MaxNumToUpdate);


			// Write the virtual_store_reason_texts data to file.
			// Loop until we hit end-of-fetch.
			for (;;)
			{
				// Fetch and test DB errors
				FetchCursor (	MAIN_DB, TR5059_virtual_store_reason_texts_cur	);

				Conflict_Test (reStart);

				if (SQLERR_code_cmp (SQLERR_end_of_fetch) == MAC_TRUE)
				{
					break;
				}
				else
				{
					if (SQLERR_error_test ())
					{
						SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR);
						break;
					}
				}


//				// Translate Hebrew text from Windows to DOS.
//				WinHebToDosHeb ((unsigned char *)(v_reason_text));				
//				WinHebToDosHeb ((unsigned char *)(v_pharmacyText));
//
//
				// Write the virtual_store_reason_texts row we fetched.
				v_MaxNumToUpdate++;

				fprintf (outFP, "%0*d"	,	3,				v_reason_code				);
				//fprintf (outFP, "%-*.*s",	100, 100,		v_reason_text				);	// DonR 18Nov2021 not part of the output, at least for now.
				//fprintf (outFP, "%0*d"	,	???,			v_displaytToMember			); //Marianna 10Nov2021 not part of the output, at least for now.
				fprintf (outFP, "%-*.*s",	100, 100,		v_pharmacyText				);
				//fprintf (outFP, "%0*d"	,	???,			v_allowOnlineOrder			); //Marianna 10Nov2021 not part of the output, at least for now.
				//fprintf (outFP, "%0*d"	,	???,			v_allowPickup				); //Marianna 10Nov2021 not part of the output, at least for now.
				//fprintf (outFP, "%0*d"	,	???,			v_allowDelivery				); //Marianna 10Nov2021 not part of the output, at least for now.
				fprintf (outFP, "%-*.*s",	20, 20,			""							);	// Reserved.  


			}	// Fetch virtual_store_reason_texts loop.

			CloseCursor (	MAIN_DB, TR5059_virtual_store_reason_texts_cur	);

			SQLERR_error_test ();

		}
		while (0);	// One-time loop to avoid GOTO's.


		// Retry if we hit a DB lock (which should never happen here).
		if (reStart)
		{
			// DonR 26Jul2011: Suppress log message until the fourth attempt fails.
			if (tries > 2)
				GerrLogReturn (GerrId, "Table is locked for the <%d> time", tries);

			sleep (ACCESS_CONFLICT_SLEEP_TIME);

			fclose (outFP);
		}

	}	/* End of retries loop */


	// REWRITE MESSAGE HEADER.
	rewind (outFP);

	fprintf (outFP, "%0*d",		MSG_ID_LEN,				5060			);
	fprintf (outFP, "%0*d",		MSG_ERROR_CODE_LEN,		MAC_OK			);
	fprintf (outFP, "%0*d",		TPharmNum_len,			v_PharmNum		);
	fprintf (outFP, "%0*d",		TInstituteCode_len,		v_InstituteCode	);
	fprintf (outFP, "%0*d",		TTerminalNum_len,		v_TerminalNum	);
	fprintf (outFP, "%0*d",		TErrorCode_len,			v_ErrorCode		);
	fprintf (outFP, "%0*d",		TGenericYYYYMMDD_len,	v_FileUpdateDate);
	fprintf (outFP, "%0*d",		TGenericHHMMSS_len,		v_FileUpdateTime);
	fprintf (outFP, "%0*d",		3,						v_MaxNumToUpdate);

	fclose (outFP);


	// Write the name of output-file on string for T-SWITCH.
	*p_outputWritten = sprintf (OBuffer, "%s", fileName);
	*output_type_flg = ANSWER_IN_FILE;

	ssmd_data_ptr->error_code	= v_ErrorCode;
	ssmd_data_ptr->pharmacy_num	= v_PharmNum;
	ssmd_data_ptr->terminal_num	= v_TerminalNum;

	RESTORE_ISOLATION;

	return (RC_SUCCESS);
}
// End of 5059 handler.


/*=======================================================================
||																		||
||			 HandlerToMsg_5061											||
||	    Message handler for message 5061 -								||
||			druggencomponents incremental download						||
||	  Added 20Nov2025 by DonR - User Story #434188.						||
||																		||
=======================================================================*/
int HandlerToMsg_5061 (	int			TransactionID_in,
						char		*IBuffer,
						int			TotalInputLen,
						cJSON		*JSON_MaccabiRequest,
						char		*OBuffer,
						int			*p_outputWritten,
						int			*output_type_flg,
						SSMD_DATA	*ssmd_data_ptr,
						int			*NumProcessed_out		)
{
	// Local declarations
	char			fileName [256];
	int				tries;
	int				reStart;
	TGenericFlag	v_ActionFlag;
	TErrorCode		err;
	PHARMACY_INFO	Phrm_info;
	FILE			*outFP;
	bool			JSON_Mode				= 0;
	ISOLATION_VAR;

	// SQL variables

	// INPUT MESSAGE
	TPharmNum			v_PharmNum;
	TInstituteCode		v_InstituteCode		= 0;
	TTerminalNum		v_TerminalNum;
	short				VersionNumber;
	TGenericYYYYMMDD	v_LastUpdateDate;
	TGenericHHMMSS		v_LastUpdateTime;

	// RETURN MESSAGE
	// header
	TErrorCode			v_ErrorCode;
	TGenericYYYYMMDD	v_FileUpdateDate;
	TGenericHHMMSS		v_FileUpdateTime;
	int					v_MaxNumToUpdate;

	// JSON output objects
	cJSON				*JSON_OutputObject			= NULL;
	cJSON				*JSON_OutputHeader			= NULL;
	cJSON				*JSON_OutputData			= NULL;
	cJSON				*JSON_OutputGenCompArray	= NULL;
	cJSON				*JSON_OutputGenComponent	= NULL;

	// lines
	int					largo_code;
	short				gen_comp_code;
	int					update_date;
	int					update_time;
	char				del_flg					[ 1 + 1];
	char				ingr_description		[40 + 1];
	char				ingredient_type			[ 1 + 1];
	double				package_size;
	double				ingredient_qty;
	char				ingredient_units		[ 3 + 1];
	double				ingredient_per_qty;
	char				ingredient_per_units	[ 3 + 1];
	double				ingredient_converted;
	char				ingredient_conv_units	[ 3 + 1];
	int					Precision				= 9;					// Number of decimal places for float outputs in non-JSON format.
	char				TableName [33]			= "DrugGenComponents";	// Remember that ODBC doesn't like string constants!


	// Init answer header variables
	REMEMBER_ISOLATION;
	PosInBuff			= IBuffer;
	v_ErrorCode			= NO_ERROR;
	v_FileUpdateDate	= 0;
	v_FileUpdateTime	= 0;
	v_MaxNumToUpdate	= 0;
	JSON_Mode			= (JSON_MaccabiRequest != NULL);


	// Read request message fields.
	if (JSON_Mode)
	{
		glbErrorCode = 0;	// Start off assuming no errors.

		v_InstituteCode	= 0;								// Already defaulted - just being paranoid. (It should really always be = 2; we
															// read it from the pharmacy table later on, in IS_PHARMACY_OPEN().)

		cJSON_GetIntByName		(JSON_MaccabiRequest, "PharmacyID",			MANDATORY,	&v_PharmNum			);	CHECK_JSON_ERROR ();
		cJSON_GetShortByName	(JSON_MaccabiRequest, "InstitutionCode",	OPTIONAL,	&v_InstituteCode	);	CHECK_JSON_ERROR ();
		cJSON_GetShortByName	(JSON_MaccabiRequest, "TerminalNum",		MANDATORY,	&v_TerminalNum		);	CHECK_JSON_ERROR ();
		cJSON_GetShortByName	(JSON_MaccabiRequest, "VersionNumber",		MANDATORY,	&VersionNumber		);	CHECK_JSON_ERROR ();
		cJSON_GetIntByName		(JSON_MaccabiRequest, "LastUpdateDate",		MANDATORY,	&v_LastUpdateDate	);	CHECK_JSON_ERROR ();
		cJSON_GetIntByName		(JSON_MaccabiRequest, "LastUpdateTime",		MANDATORY,	&v_LastUpdateTime	);	CHECK_JSON_ERROR ();

		if (glbErrorCode)
			SetErrorVar (&v_ErrorCode, ERR_WRONG_FORMAT_FILE);
	}
	else
	{
		v_PharmNum			= GetLong	(&PosInBuff, TPharmNum_len			); CHECK_ERROR();
		v_InstituteCode		= GetShort	(&PosInBuff, TInstituteCode_len		); CHECK_ERROR();
		v_TerminalNum		= GetShort	(&PosInBuff, TTerminalNum_len		); CHECK_ERROR();
		VersionNumber		= GetShort	(&PosInBuff, 2						); CHECK_ERROR();
		v_LastUpdateDate	= GetLong	(&PosInBuff, TGenericYYYYMMDD_len	); CHECK_ERROR();
		v_LastUpdateTime	= GetLong	(&PosInBuff, TGenericHHMMSS_len		); CHECK_ERROR();

		// Get amount of input not eaten
		ChckInputLen (TotalInputLen - (PosInBuff - IBuffer));
		CHECK_ERROR ();
	}

	ChckDate (v_LastUpdateDate); CHECK_ERROR();
	ChckTime (v_LastUpdateTime); CHECK_ERROR();


	// Get ready to process the request.
	reStart = MAC_TRUE;

	sprintf (fileName,
			 "%s/druggencomponents_%0*d_%0*d_%0*d.snd",
			 MsgDir,
			 TPharmNum_len,
			 v_PharmNum,
			 TInstituteCode_len,
			 v_InstituteCode,
			 TTerminalNum_len,
			 v_TerminalNum);


	// Retries loop.
	for (tries = 0;	(tries < SQL_UPDATE_RETRIES) && (reStart == MAC_TRUE); tries++)
	{
		// Create new file for response message.
		outFP = fopen (fileName, "w");


		if (outFP == NULL)
		{
			int	err = errno;

			GerrLogReturn (GerrId,
						   "\tCannot open output file '%s'\n\tError (%d) %s\n",
						   fileName,
						   err,
						   strerror(err));

			RESTORE_ISOLATION;
			return (ERR_UNABLE_OPEN_OUTPUT_FILE);
		}

		// Init answer header variables
		v_ErrorCode			= NO_ERROR;
		v_FileUpdateDate	= 0;
		v_FileUpdateTime	= 0;
		v_MaxNumToUpdate	= 0;
		reStart				= MAC_FALS;

		// Avoid unnecessary lock errors by downgrading "isolation"
		// for this transaction.
		SET_ISOLATION_DIRTY;

		// One-time loop to avoid GOTO's. Response is sent upon exit from this "loop".
		do
		{
			// Test if pharmacy has been OPENed.
			err = IS_PHARMACY_OPEN (v_PharmNum, v_InstituteCode, &Phrm_info);
			SetErrorVar (&v_ErrorCode, err);
			if (v_ErrorCode != MAC_OK)
			{
				break;
			}

			// Get the modification date.
			ExecSQL (	MAIN_DB, READ_TablesUpdate,
						&v_FileUpdateDate,	&v_FileUpdateTime,
						&TableName,			END_OF_ARG_LIST		);

			Conflict_Test (reStart);

			if (SQLERR_code_cmp (SQLERR_not_found) == MAC_TRUE)
			{
				v_FileUpdateDate = 0;
				v_FileUpdateTime = 0;
			}
			else
			{
				if (SQLERR_error_test ())
				{
					if (SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR))
					{
						break;
					}
				}
			}


			// Declare and open DrugNotes cursor.
			// DonR 13Jul2020: Use new combined operation to avoid splitting
			// the DECLARE and OPEN statements.
			DeclareAndOpenCursor (	MAIN_DB, TR5061_DrugGenComponents_cur,
									&v_LastUpdateDate,	&v_LastUpdateTime,
									&v_LastUpdateDate,	END_OF_ARG_LIST		);

			if (SQLERR_error_test ())
			{
				SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR);
				break;
			}


			// Create the header with data so far.
			if (JSON_Mode)
			{
				JSON_OutputObject = cJSON_CreateObject ();

				if (JSON_OutputObject == NULL)
				{
					GerrLogMini (GerrId, "Couldn't create JSON_OutputObject!");
					// We should obviously do something more than log an error here -
					// but I'm not yet sure just what. In any case, creating the object
					// should never fail in real life - I hope!
				}
				else
				{
					// Create and populate Response Header object, as well as the
					// "container" objects for table row data.
					JSON_OutputHeader = cJSON_AddObjectToObject (JSON_OutputObject, "Header");

					cJSON_AddAnyNumberToObject	(JSON_OutputHeader, "ResponseTransactionID",	5062			);	// Is this really necessary?
					cJSON_AddAnyNumberToObject	(JSON_OutputHeader, "CommError",				MAC_OK			);	// Is this really necessary?
					cJSON_AddAnyNumberToObject	(JSON_OutputHeader, "PharmacyID",				v_PharmNum		);
					cJSON_AddAnyNumberToObject	(JSON_OutputHeader, "InstituteCode",			v_InstituteCode	);
					cJSON_AddAnyNumberToObject	(JSON_OutputHeader, "TerminalNum",				v_TerminalNum	);

					// Error Code and Table Update Date/Time will be added after the table-reading loop.

					// Set up objects for table-data output.
					JSON_OutputData			= cJSON_AddObjectToObject	(JSON_OutputObject, "Data");

					JSON_OutputGenCompArray	= cJSON_AddArrayToObject	(JSON_OutputData, "DrugComponents");
				}	// Created output object successfully.
			}	// Create "starter" JSON output header.
			else
			{	// Create "Legacy" output header.
				fprintf (outFP, "%0*d",		MSG_ID_LEN,				5062			);
				fprintf (outFP, "%0*d",		MSG_ERROR_CODE_LEN,		MAC_OK			);
				fprintf (outFP, "%0*d",		TPharmNum_len,			v_PharmNum		);
				fprintf (outFP, "%0*d",		TInstituteCode_len,		v_InstituteCode	);
				fprintf (outFP, "%0*d",		TTerminalNum_len,		v_TerminalNum	);
				fprintf (outFP, "%0*d",		TErrorCode_len,			v_ErrorCode		);
				fprintf (outFP, "%0*d",		TGenericYYYYMMDD_len,	v_FileUpdateDate);
				fprintf (outFP, "%0*d",		TGenericHHMMSS_len,		v_FileUpdateTime);
				fprintf (outFP, "%0*d",		TEconPriNumUpdated_len,	v_MaxNumToUpdate);
			}


			// Write the DrugGenComponents data to file.
			// Loop until we hit end-of-fetch.
			for (;;)
			{
				// Fetch and test DB errors
				FetchCursorInto (	MAIN_DB, TR5061_DrugGenComponents_cur,
									&largo_code,			&gen_comp_code,		&del_flg,
									&ingr_description,		&ingredient_type,	&package_size,
									&ingredient_qty,		&ingredient_units,	&ingredient_per_qty,
									&ingredient_per_units,	&update_date,		&update_time,
									END_OF_ARG_LIST														);

				Conflict_Test (reStart);

				if (SQLERR_code_cmp (SQLERR_end_of_fetch) == MAC_TRUE)
				{
					break;
				}
				else
				{
					if (SQLERR_error_test ())
					{
						SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR);
						break;
					}
				}


				// Set Action Flag based on whether this is a deletion or an insert/update.
				v_ActionFlag = ((*del_flg == 'D') || (*del_flg == 'd')) ? 1 : 0;

				// For ingredients in milligrams/micrograms, create a separate amount in grams
				// (since that's the unit the Ministry of Health wants in reports).
				ingredient_converted = ingredient_qty;				// Default = use what we already have.
				strcpy (ingredient_conv_units, ingredient_units);	// Default = use what we already have.

				if (!strcasecmp (ingredient_units, "MIU"))
				{
					// Convert milli-IU to IU.
					ingredient_converted /= 1000.0;
					strcpy (ingredient_conv_units, "IU");
				}

				else
				if (!strcasecmp (ingredient_units, "mcg"))
				{
					// Convert micrograms to grams.
					ingredient_converted /= 1000000.0;
					strcpy (ingredient_conv_units, "g");
				}

				else
				if (	(!strcasecmp (ingredient_units, "mg " ))	||
						(!strcasecmp (ingredient_units, "mg"  ))		)
				{
					// Convert milligrams to grams.
					ingredient_converted /= 1000.0;
					strcpy (ingredient_conv_units, "g");
				}


				// Write the DrugNotes row we fetched.
				v_MaxNumToUpdate++;

				if (JSON_Mode)
				{
					JSON_OutputGenComponent = cJSON_AddNewObjectToArray (JSON_OutputGenCompArray);

					// No point in sending padded strings in JSON!
					StripAllSpaces (ingr_description		);
					StripAllSpaces (ingredient_type			);
					StripAllSpaces (ingredient_units		);
					StripAllSpaces (ingredient_per_units	);

					cJSON_AddAnyNumberToObject	(JSON_OutputGenComponent, "LargoCode",			largo_code				);	//  8 in specification.
					cJSON_AddAnyNumberToObject	(JSON_OutputGenComponent, "IngredientCode",		gen_comp_code			);	//  9
					cJSON_AddAnyNumberToObject	(JSON_OutputGenComponent, "ActionCode",			v_ActionFlag			);	// 10
					cJSON_AddStringToObject		(JSON_OutputGenComponent, "IngredientName",		ingr_description		);	// 11
					cJSON_AddStringToObject		(JSON_OutputGenComponent, "IngredientType",		ingredient_type			);	// 12
					cJSON_AddAnyNumberToObject	(JSON_OutputGenComponent, "PackageSize",		package_size			);	// 13
					cJSON_AddStringToObject		(JSON_OutputGenComponent, "IngredientUnits",	ingredient_units		);	// 14
					cJSON_AddAnyNumberToObject	(JSON_OutputGenComponent, "IngredientQty",		ingredient_qty			);	// 15
					cJSON_AddStringToObject		(JSON_OutputGenComponent, "DoseUnits",			ingredient_per_units	);	// 16
					cJSON_AddAnyNumberToObject	(JSON_OutputGenComponent, "DoseSize",			ingredient_per_qty		);	// 17
					cJSON_AddAnyNumberToObject	(JSON_OutputGenComponent, "IngredientQtyStd",	ingredient_converted	);	// 18
					cJSON_AddStringToObject		(JSON_OutputGenComponent, "IngredientUnitsStd",	ingredient_conv_units	);	// 19
				}
				else
				{
					fprintf (outFP, "%0*d",		 9,				largo_code				);	//  8 in specification.
					fprintf (outFP, "%0*d",		 4,				gen_comp_code			);	//  9
					fprintf (outFP, "%0*d",		 1,				v_ActionFlag			);	// 10
					fprintf (outFP, "%-*.*s",	40, 40,			ingr_description		);	// 11
					fprintf (outFP, "%-*.*s",	 1,  1,			ingredient_type			);	// 12

					// Determine the number of decimal places to output Package Size.
					Precision = 9;	// Default to 01.123456789
					do
					{
						if (package_size >=         10.0) Precision--; else break;
						if (package_size >=        100.0) Precision--; else break;
						if (package_size >=       1000.0) Precision--; else break;
						if (package_size >=      10000.0) Precision--; else break;
						if (package_size >=     100000.0) Precision--; else break;
						if (package_size >=    1000000.0) Precision--; else break;
						if (package_size >=   10000000.0) Precision--; else break;
						if (package_size >=  100000000.0) Precision--; else break;
						if (package_size >= 1000000000.0) Precision--; else break;	// All the way down to zero decimal places!

						// We shouldn't really see numbers this big - but if we ever do, we don't want
						// to break the output format by trying to report them.
						if (package_size >= 9999999999.0)
						{
							package_size = 0.0;	// Suppress numbers that won't fit.
						}
					}
					while (0);

					fprintf (outFP, "%0*.*f",	10, Precision,	package_size			);	// 13

					fprintf (outFP, "%-*.*s",	 3,  3,			ingredient_units		);	// 14

					// Determine the number of decimal places to output Ingredient Quantity.
					Precision = 9;	// Default to 01.123456789
					do
					{
						if (ingredient_qty >=         10.0) Precision--; else break;
						if (ingredient_qty >=        100.0) Precision--; else break;
						if (ingredient_qty >=       1000.0) Precision--; else break;
						if (ingredient_qty >=      10000.0) Precision--; else break;
						if (ingredient_qty >=     100000.0) Precision--; else break;
						if (ingredient_qty >=    1000000.0) Precision--; else break;
						if (ingredient_qty >=   10000000.0) Precision--; else break;
						if (ingredient_qty >=  100000000.0) Precision--; else break;
						if (ingredient_qty >= 1000000000.0) Precision--; else break;	// All the way down to zero decimal places!

						// We shouldn't really see numbers this big - but if we ever do, we don't want
						// to break the output format by trying to report them.
						if (ingredient_qty >= 9999999999.0)
						{
							ingredient_qty = 0.0;	// Suppress numbers that won't fit.
						}
					}
					while (0);

					fprintf (outFP, "%0*.*f",	10, Precision,	ingredient_qty			);	// 15

					fprintf (outFP, "%-*.*s",	 3,  3,			ingredient_per_units	);	// 16

					// Determine the number of decimal places to output Ingredient-Per Quantity.
					Precision = 9;	// Default to 01.123456789
					do
					{
						if (ingredient_per_qty >=         10.0) Precision--; else break;
						if (ingredient_per_qty >=        100.0) Precision--; else break;
						if (ingredient_per_qty >=       1000.0) Precision--; else break;
						if (ingredient_per_qty >=      10000.0) Precision--; else break;
						if (ingredient_per_qty >=     100000.0) Precision--; else break;
						if (ingredient_per_qty >=    1000000.0) Precision--; else break;
						if (ingredient_per_qty >=   10000000.0) Precision--; else break;
						if (ingredient_per_qty >=  100000000.0) Precision--; else break;
						if (ingredient_per_qty >= 1000000000.0) Precision--; else break;	// All the way down to zero decimal places!

						// We shouldn't really see numbers this big - but if we ever do, we don't want
						// to break the output format by trying to report them.
						if (ingredient_per_qty >= 9999999999.0)
						{
							ingredient_per_qty = 0.0;	// Suppress numbers that won't fit.
						}
					}
					while (0);

					fprintf (outFP, "%0*.*f",	10, Precision,	ingredient_per_qty		);	// 17

					// Determine the number of decimal places to output Standard-Unit Ingredient Quantity.
					Precision = 9;	// Default to 01.123456789
					do
					{
						if (ingredient_converted >=         10.0) Precision--; else break;
						if (ingredient_converted >=        100.0) Precision--; else break;
						if (ingredient_converted >=       1000.0) Precision--; else break;
						if (ingredient_converted >=      10000.0) Precision--; else break;
						if (ingredient_converted >=     100000.0) Precision--; else break;
						if (ingredient_converted >=    1000000.0) Precision--; else break;
						if (ingredient_converted >=   10000000.0) Precision--; else break;
						if (ingredient_converted >=  100000000.0) Precision--; else break;
						if (ingredient_converted >= 1000000000.0) Precision--; else break;	// All the way down to zero decimal places!

						// We shouldn't really see numbers this big - but if we ever do, we don't want
						// to break the output format by trying to report them.
						if (ingredient_converted >= 9999999999.0)
						{
							ingredient_converted = 0.0;	// Suppress numbers that won't fit.
						}
					}
					while (0);

					fprintf (outFP, "%0*.*f",	10, Precision,	ingredient_converted	);	// 18
					fprintf (outFP, "%-*.*s",	 3,  3,			ingredient_conv_units	);	// 19
				}	// "Legacy" output.
			}	// Fetch DrugGenComponents loop.

			CloseCursor (	MAIN_DB, TR5061_DrugGenComponents_cur	);

			SQLERR_error_test ();

		}
		while (0);	// One-time loop to avoid GOTO's.


		// Commit the transaction (not that there's actually any need for a COMMIT here).
		if (reStart == MAC_FALS)	/* Transaction OK then COMMIT	*/
		{
			// DonR 13Jan2020: No point in a commit when we haven't modified anything!
		}
		else
		{
			if (TransactionRestart () != MAC_OK)
			{
				v_ErrorCode = ERR_DATABASE_ERROR;
				break;
			}

			// DonR 26Jul2011: Suppress log message until the fourth attempt fails.
			if (tries > 2)
				GerrLogReturn (GerrId, "Table is locked for the <%d> time", tries);

			sleep (ACCESS_CONFLICT_SLEEP_TIME);

			fclose (outFP);
		}

	}	/* End of retries loop */


	// For JSON output, populate the "missing" JSON header values and output to file.
	if (JSON_Mode)
	{
		cJSON_AddAnyNumberToObject	(JSON_OutputHeader, "ErrorCode",		v_ErrorCode			);
		cJSON_AddAnyNumberToObject	(JSON_OutputHeader, "MaxUpdateDate",	v_FileUpdateDate	);
		cJSON_AddAnyNumberToObject	(JSON_OutputHeader, "MaxUpdateTime",	v_FileUpdateTime	);

		// Send the output JSON hierarchy to file and delete it from memory.
		cJSON_PrintToFP (JSON_OutputObject, outFP);
//		cJSON_PrintUnformattedToFP (JSON_OutputObject, outFP);
		cJSON_Delete (JSON_OutputObject);
	}	// Complete JSON output header and write entire output object to file.
	else
	{
		// REWRITE MESSAGE HEADER.
		rewind (outFP);

		fprintf (outFP, "%0*d",		MSG_ID_LEN,				5062			);
		fprintf (outFP, "%0*d",		MSG_ERROR_CODE_LEN,		MAC_OK			);
		fprintf (outFP, "%0*d",		TPharmNum_len,			v_PharmNum		);
		fprintf (outFP, "%0*d",		TInstituteCode_len,		v_InstituteCode	);
		fprintf (outFP, "%0*d",		TTerminalNum_len,		v_TerminalNum	);
		fprintf (outFP, "%0*d",		TErrorCode_len,			v_ErrorCode		);
		fprintf (outFP, "%0*d",		TGenericYYYYMMDD_len,	v_FileUpdateDate);
		fprintf (outFP, "%0*d",		TGenericHHMMSS_len,		v_FileUpdateTime);
		fprintf (outFP, "%0*d",		TEconPriNumUpdated_len,	v_MaxNumToUpdate);

		fclose (outFP);
	}	// Finish "legacy" output.


	// Write the name of output-file on string for T-SWITCH.
	*p_outputWritten = sprintf (OBuffer, "%s", fileName);
	*output_type_flg = ANSWER_IN_FILE;

	ssmd_data_ptr->error_code	= v_ErrorCode;
	ssmd_data_ptr->pharmacy_num	= v_PharmNum;
	ssmd_data_ptr->terminal_num	= v_TerminalNum;

	RESTORE_ISOLATION;

	return (RC_SUCCESS);
}
// End of 5061 handler.



int process_spools (void)
{
	static int	initialized		= 0;
	static int	check_interval	= 120;	// DonR 04Mar2021: Changed from 30 seconds to 2 minutes.
	static int	update_interval	= 0;
	static int	LastDate		= 0;
	static int	LastTime		= 0;
	int			error_count		= 0;
	int			num_sent		= 0;
	ISOLATION_VAR;

	// SQL variables

	// Gadget Spool.
	int			v_pharmacy_code;
	int			v_prescription_id;
	int			v_member_id;
	short		v_mem_id_extension;
	int			v_largo_code;
	int			v_service_code;
	int			v_service_number;
	int			v_service_type;
	int			v_op;
	int			v_op_price;
	int			v_total_ptn;
	int			v_event_date;
	int			v_doctor_id;
	int			v_request_date;
	int			v_request_num;
	short		v_action_code;
	short		v_as400_error_code;
	int			v_update_date;
	int			v_update_time;
	short		v_reported_to_as400;

	// Gadget Spool Timer.
	short		v_interval;
	int			v_last_run_date;
	int			v_last_run_time;

	// Miscellaneous.
	int			SysDate;
	int			SysTime;

	// Server Audit cleanup.
	int			KeepDate;


	// Initialization.
	REMEMBER_ISOLATION;
	SET_ISOLATION_COMMITTED;
	SysDate = GetDate ();
	SysTime = GetTime ();

	// DonR 04Mar2021: Bug fix - the routine was supposed to re-initialize every five
	// minutes, but what should have been a logical OR was an AND - so it wasn't
	// re-reading the gadget_spool_timer table after the very first time. Fixed that,
	// plus changed the re-initialization interval from five minutes to one hour.
	if ((!initialized) ||
		((SysDate > LastDate) || ((SysDate == LastDate) && ((SysTime - LastTime) > 10000))))
	{
		LastDate = SysDate;
		LastTime = SysTime;

		ExecSQL (	MAIN_DB, process_spools_READ_gadget_spool_run_interval,
					&v_interval, END_OF_ARG_LIST								);

		if (SQLCODE == 0)
		{
			initialized		= 1;
			update_interval	= (int)v_interval;
		}	// Good read of Gadget Spool Timer.


		RESTORE_ISOLATION;
		return ((SQLCODE == 0) ? 0 : -1);
	}	// Uninitialized and either first time or five minutes have passed since last
		// failed attempt to read the interval from gadget_spool_timer.

	else
	{
		// If a previous initialization attempt failed and less than five minutes has
		// passed, quit now with a return code of -2.
		if (!initialized)
		{
			RESTORE_ISOLATION;
			return (-2);
		}
	}


	// If we get here, the function has previously been initialized.
	// Now check if enough time has passed to be worth checking the
	// gadget_spool_timer table. I've arbitrarily decided that the checking
	// interval is 30 seconds - this seems reasonable, in that it ensures
	// frequent checks but doesn't burden the database too much.
	if (diff_from_then (LastDate,	LastTime,
						SysDate,	SysTime)	>= check_interval)
	{
		LastDate = SysDate;
		LastTime = SysTime;

		// Check database to see if it's time to process the spool. This is
		// done in a rather sneaky way - by performing an update query
		// on the gadget_spool_timer table. Only if enough time has passed
		// will the update query work; and as soon as it does work, all
		// instances of this process will be prevented from processing the
		// gadget spool until the time interval has passed yet again. In other
		// words, if the interval is set to 60 seconds, the update query
		// will succeed at most once per minute; and thus the spool will be
		// processed once each minute or so, by a random instance of the process.
		v_last_run_date = SysDate;
		v_last_run_time = IncrementTime (SysTime, (0 - update_interval), &v_last_run_date);

		ExecSQL (	MAIN_DB, process_spools_UPD_gadget_spool_timer,
					&SysDate,			&SysTime,
					&v_last_run_date,	&v_last_run_date,
					&v_last_run_time,	END_OF_ARG_LIST		);

		// DonR 22Oct2020: Add a COMMIT to make sure we don't leave the table locked.
		if ((SQLCODE != 0) || (sqlca.sqlerrd[2] < 1))
		{
			CommitAllWork ();
			RESTORE_ISOLATION;
			return (0);	// The update failed - which should normally mean that some instance
						// of the process has processed the gadget spool within the
						// configured time interval.
		}
		else
		{
			CommitAllWork ();
		}


		// The update succeeded, so it's time to process the gadget spool.
		// DonR 19Aug2025 User Story #442308: If Nihul Tikrot calls are disabled, disable Meishar
		// calls as well. In this case we still update the spool timer, since we don't need
		// to keep retrying constantly until communications with AS/400 are re-enabled.
		if (TikrotRPC_Enabled)
		{
			DeclareAndOpenCursor (	MAIN_DB, process_spools_gadget_spool_cur,
									&NOT_REPORTED_TO_AS400, END_OF_ARG_LIST		);

			if (SQLCODE != 0)
			{
				CloseCursor (	MAIN_DB, process_spools_gadget_spool_cur	);

				CommitAllWork ();

				RESTORE_ISOLATION;
				return (-1);	// No need for error-handling here - just give up.
			}

			do
			{
				FetchCursorInto (	MAIN_DB, process_spools_gadget_spool_cur,
									&v_pharmacy_code,		&v_prescription_id,
									&v_member_id,			&v_mem_id_extension,
									&v_largo_code,			&v_service_code,
									&v_service_number,		&v_service_type,
									&v_op,					&v_op_price,
									&v_total_ptn,			&v_event_date,
									&v_doctor_id,			&v_request_date,
									&v_request_num,			&v_action_code,
									END_OF_ARG_LIST									);

				if ((SQLCODE != 0) || (num_sent >= 50))
				{
					CloseCursor (	MAIN_DB, process_spools_gadget_spool_cur	);

					CommitAllWork ();

					break;	// No need for error-handling here - just give up.
							// Note that this handles normal end-of-cursor as
							// well as "real" errors.
				}

				// Good fetch - tell AS400 about gadget sale/cancellation.
				v_as400_error_code = as400SaveSaleRecord ((int)v_pharmacy_code,
														  (int)v_prescription_id,
														  (int)v_member_id,
														  (int)v_mem_id_extension,
														  (int)v_service_code,
														  (int)v_service_number,
														  (int)v_service_type,
														  (int)v_largo_code,
														  (int)v_op,
														  (int)v_op_price,
														  (int)v_total_ptn,
														  (int)v_event_date,
														  (int)v_doctor_id,
														  (int)v_request_date,		// Doctor Presc. Date
														  (int)v_request_num,		// Doctor Presc. ID
														  (int)v_action_code,
														  (int)2					// Retries
														 );
				GerrLogFnameMini ("gadgets_log",
								  GerrId,
								  "Process spool: PID %d, SaveSale returned %d for Ph %d, Mem %d, Svc %d/%d/%d.",
								  getpid(),
								  v_as400_error_code,
								  v_pharmacy_code,
								  v_member_id,
								  (int)v_service_code,
								  (int)v_service_number,
								  (int)v_service_type);

				if (v_as400_error_code != -1)
				{
					error_count = 0;	// Reset counter, since AS400 server is evidently working.
					num_sent++;

					// If we hit an error other than communications failure, we'll update
					// the spool row but still consider it as "unreported to AS400".
					v_reported_to_as400 = (v_as400_error_code == 0) ? 1 : 0;

					// Update "holding" table!
					ExecSQL (	MAIN_DB, process_spools_UPD_gadget_spool,
								&v_as400_error_code,	&SysDate,
								&SysTime,				&v_reported_to_as400,
								END_OF_ARG_LIST									);

					// Note: No error-handling needed here, I think.

				}	// Got a response from AS400 other than communications failure.
				else
				{
					error_count++;
					if (error_count > 10)
					{
						GerrLogFnameMini ("gadgets_log",
										  GerrId,"Process spool: PID %d quitting after 10 failures in a row.\n", getpid());
						CloseCursor (	MAIN_DB, process_spools_gadget_spool_cur	);

						CommitAllWork ();

						break;
					}
				}
			}
			while (1);
		}	// "Nihul Tikrot Enabled" switch is ON - DonR 19Aug2025 emergency fix.


		// DonR 12Dec2012: Add purging of old stuff from server-audit log tables.
		if ((SysTime >= 1000) && (SysTime <= 1500))	// Only between 00:10 and 00:15.
		{
			KeepDate = AddDays (SysDate, (0 - 5));

			ExecSQL (	MAIN_DB, process_spools_DEL_purge_sql_srv_audit,
						&KeepDate, END_OF_ARG_LIST							);

			ExecSQL (	MAIN_DB, process_spools_DEL_purge_sql_srv_audit_doc,
						&KeepDate, END_OF_ARG_LIST							);
		}

		// DonR 16Dec2019: PrescSource is automatically reloaded every hour (in SqlServer.ec);
		// I don't think we need to re-read an essentially static table more frequently than that.
//		// DonR 05Mar2017: Add periodic reloading of in-memory Prescription Source table.
//		LoadPrescrSource ();


		// DonR 19Oct2017: Add processing of late-arriving doctor prescriptions.
		ProcessRxLateArrivalQueue (0, 0, 0, 0);	// All arguments are zero in "normal" queue-processing mode.

	}	// Enough time has passed to attempt processing the gadget spool.

	else	// Not enough time has passed - so do nothing.
	{
		RESTORE_ISOLATION;
		return (0);
	}
}



// NIU as of 16Nov2025 (and since I don't know when!)
#if 0
short FindDrugPurchLimit (int			DrugCode_in,
						  short			Gender_in,
						  int			MemberBirthday_in,
						  TDrugsBuf		*DrugsBufPtr_in,
						  TPresDrugs	*CurrSaleDrugsArrayPtr_in,
						  int			CurrSaleNumDrugs_in,
						  int			*Units_out,
						  short			*Months_out,
						  int			*HistStartDate_out,
						  TErrorCode	*ErrCode_out,
						  int			*Restart_out)
{
	ISOLATION_VAR;
	int		SysDate;
	short	i;
	short	FoundSomething			= 0;
	short	MadeQualifyingPurchase	= 0;

	short	Gender					= Gender_in;
	int		DrugCode				= DrugCode_in;
	short	MemberAgeYears;
	int		Units;
	short	Months;
	short	ClassCode;
	short	ClassType;
	int		ClassHistoryDays;
	int		MinimumPurchDate;
	int		HistStartDate;


	DeclareCursorInto (	MAIN_DB, FindDrugPurchLimit_drug_purchase_lim_cur,
						&Units,				&Months,
						&HistStartDate,		&ClassCode,
						&ClassType,			&ClassHistoryDays,
						&DrugCode,			&Gender,
						&MemberAgeYears,	&MemberAgeYears,
						END_OF_ARG_LIST											);


	REMEMBER_ISOLATION;
	SET_ISOLATION_DIRTY;

	SysDate			= GetDate ();

	MemberAgeYears	= (SysDate / 10000) - (MemberBirthday_in / 10000);

	// DonR 28Dec2015: Member's age in years ticks up *on* his/her birthday - so the comparison
	// below needs to change from "<=" to "<". Note that (at least for now) we are not worrying
	// about members born on 29 February buying drugs on 28 February of a non-leap year. As
	// Gilbert and Sullivan would tell us, this is a paradox!
	if ((SysDate % 10000) < (MemberBirthday_in % 10000))
		MemberAgeYears--;	// Member hasn't reached his/her birthday this year.


	OpenCursor (	MAIN_DB, FindDrugPurchLimit_drug_purchase_lim_cur	);

	if (SQLERR_error_test ())
	{
		*ErrCode_out = ERR_DATABASE_ERROR;
		FoundSomething	= -1;
	}

	else
	{
		// Fetch data and store in buffer.
		for ( ; ; )
		{
			FetchCursor (	MAIN_DB, FindDrugPurchLimit_drug_purchase_lim_cur	);

			if (SQLERR_code_cmp (SQLERR_access_conflict) == MAC_TRUE)
			{
				SQLERR_error_test ();
				FoundSomething = -1;
				*Restart_out = MAC_TRUE;
				break;
			}

			if (SQLERR_code_cmp (SQLERR_end_of_fetch) == MAC_TRUE)
			{
				FoundSomething = 0;	// We got to the end of the cursor without finding a match.
				break;
			}

			if (SQLERR_error_test ())
			{
				*ErrCode_out = ERR_DATABASE_ERROR;
				FoundSomething	= -1;
				break;
			}

			// If we get here, we've managed to read a purchase limit row for the drug being bought
			// that conforms to any relevant demographic criteria. However, if the purchase limit
			// class for this limit is of type 2, we also have to qualify the limit based on whether
			// the member has bought drugs of the same class as the purchase limit (e.g. insulin
			// for purchase limits on blood-sugar diagnostic strips).
			if (ClassType == 2)
			{
				MadeQualifyingPurchase = 0;
				MinimumPurchDate = IncrementDate (GetDate (), (0 - ClassHistoryDays));

				// If someone creates a Type 2 class with Class Code of zero, it will be ignored!
				for (i = 0; ((ClassCode > 0) && (i < DrugsBufPtr_in->max)); i++)
				{
					if ((DrugsBufPtr_in->Durgs[i].ClassCode	== ClassCode)			&&
						(DrugsBufPtr_in->Durgs[i].StartDate	>= MinimumPurchDate))
					{
						MadeQualifyingPurchase = 1;
						break;
					}
				}

				// DonR 06Jun2010: We need to check current sale for qualifying purchase, not just past sales.
				for (i = 0; ((ClassCode > 0) && (!MadeQualifyingPurchase) && (i < CurrSaleNumDrugs_in)); i++)
				{
					if (CurrSaleDrugsArrayPtr_in[i].DL.class_code == ClassCode)
					{
						MadeQualifyingPurchase = 1;
						break;
					}
				}

				// If member has not purchased a qualifying drug for this Purchase Limit Class, skip
				// to the next row in the cursor.
				if (!MadeQualifyingPurchase)
				{
					continue;
				}
			}	// ClassType == 2

			// If we get here, we've actually managed to find a combination of Drug Purchase Limit
			// and Purchase Limit Class that fits this member and drug.
			FoundSomething		= 1;
			*Units_out			= Units;
			*Months_out			= Months;
			*HistStartDate_out	= HistStartDate;
			break;
		}	// Cursor-reading loop.
	}	// Cursor was opened successfully.

	CloseCursor (	MAIN_DB, FindDrugPurchLimit_drug_purchase_lim_cur	);

	RESTORE_ISOLATION;

	return (FoundSomething);
}
#endif


// Read a member's data into the global Member structure. This function doesn't
// do any real error-handling; if it returns zero, the calling logic is responsible
// for deciding what to do about row-not-found or any other database error.
int read_member (	int			MemberID_in,
					short		MemberID_code_in	)
{
	// Clear out the global member structure unconditionally, so if the SELECT fails
	// we won't have any residual data from previously-read members.
	memset ((char *)&Member, 0, sizeof (TMemberData));

	Member.ID		= MemberID_in;
	Member.ID_code	= MemberID_code_in;

	// DonR 13May2020 CR #31591: Add new Member-on-Ventilator flag. This is
	// currently stored in the old column "asaf_code" (which was sent from
	// AS/400 but never used for anything); when we switch to MS-SQL, the
	// column should be renamed.
	ExecSQL (	MAIN_DB, READ_members_full,
				&Member.last_name,			&Member.first_name,				&Member.date_of_bearth,
				&Member.maccabi_code,		&Member.spec_presc,				&Member.maccabi_until,
				&Member.payer_tz,			&Member.payer_tz_code,			&Member.sex,
				&Member.phone,				&Member.house_num,				&Member.street,
				&Member.city,				&Member.zip_code,				&Member.insurance_type,

				&Member.keren_mac_code,		&Member.keren_mac_from,			&Member.keren_mac_until,
				&Member.mac_magen_code,		&Member.mac_magen_from,			&Member.mac_magen_until,
				&Member.yahalom_code,		&Member.yahalom_from,			&Member.yahalom_until,
				&Member.carry_over_vetek,	&Member.keren_wait_flag,		&Member.illness_bitmap,
				&Member.card_date,			&Member.update_date,			&Member.update_time,

				&Member.authorizealways,	&Member.updated_by,				&Member.check_od_interact,
				&Member.credit_type_code,	&Member.max_drug_date,			&Member.discount_percent,
				&Member.insurance_status,	&Member.idnumber_main,			&Member.idcode_main,
				&Member.has_tikra,			&Member.has_coupon,				&Member.in_hospital,
				&Member.VentilatorDiscount,	&Member.darkonai_type,			&Member.force_100_percent_ptn,

				&Member.darkonai_no_card,	&Member.has_blocked_drugs,		&Member.died_in_hospital,
				&Member.mahoz,				&Member.dangerous_drug_status,

				&Member.ID,					&Member.ID_code,				END_OF_ARG_LIST						);

	// Strip leading and trailing spaces from address strings, and construct a single address string.
	strip_spaces (Member.street);
	strip_spaces (Member.house_num);
	strip_spaces (Member.city);
	sprintf (Member.address, "%s %s, %s %d", Member.street, Member.house_num, Member.city, Member.zip_code);

	// DonR 04Dec2022 User Story #408077: Use a new function to set some Member flags and values.
	SetMemberFlags (SQLCODE);

	// Return TRUE if the read was successful; any failure will return 0. Normal error-handling should
	// follow any call to this function if it returns zero.
	return (SQLCODE == 0);
}



// Read an entire drug_list row into a TDrugListRow structure and/or a TPresDrugs structure.
// This function returns TRUE if successful; in the event of any failure, it returns FALSE.
// The calling function is responsible for error handling in that case, and also for controlling
// database isolation.

int read_drug (int				DrugCode_in,
			   int				MaxVisibleDrug_in,
			   PHARMACY_INFO	*PharmInfo_in,
			   bool				SeeDeletedDrugs_in,
			   TDrugListRow		*DL_out,
			   TPresDrugs		*PresDrugs_out)
{
	int				DrugCode		= DrugCode_in;
	short			PharmCategory;
	bool			MaccabiPharmacy;
	TDrugListRow	DL;


	memset ((char *)&DL, 0, sizeof (DL));
	DL.largo_code = DrugCode_in;

	// DonR 04Dec2011: If a Pharmacy Info structure wasn't supplied, assume that we're not dealing with Assuta -
	// so we won't read any deleted drugs.
	if (PharmInfo_in != NULL)
	{
		PharmCategory	= PharmInfo_in->pharm_category;
		MaccabiPharmacy	= PharmInfo_in->maccabi_pharmacy;
	}
	else
	{
		PharmCategory = -999;
		MaccabiPharmacy	= 0;
	}

	// DonR 30Mar2011: To simplify the SELECT, convert DrugCode to a bogus value if the requested
	// Largo Code exceeds the maximum "visible" Largo Code.
	if (DrugCode > MaxVisibleDrug_in)
		DrugCode = -999999;


	// DonR 30Mar2011: In addition to the change above, move the criteria for del_flg and Assuta Drug Status
	// to a "post-SELECT select"; this should make the SQL run faster.
	// DonR 27Nov2011: Reformatted SELECT to three columns to save space and improve readability.
	// DonR 24Sep2019: Another conditional read: convert all values of chronic_flag other than 1 to 0.
	// DonR 23Jul2023 User Story 448931/458942: Added two new "flag" columns.
	// DonR 22Jul2025 User Story #427521/417800: Added two new "flags" for narcotics/psychotherapy
	// drugs, plus a new preparation_type column.
	ExecSQL (	MAIN_DB, read_drug_READ_drug_list,
				&DL.long_name,						&DL.short_name,							&DL.name_25,
				&DL.preferred_flg,					&DL.preferred_largo,					&DL.multi_pref_largo,
				&DL.del_flg,						&DL.del_flg_cont,						&DL.is_out_of_stock,
				&DL.parent_group_code,				&DL.parent_group_name,					&DL.economypri_group,
				&DL.specialist_drug,				&DL.has_shaban_rules,					&DL.in_doctor_percents,

				&DL.class_code,						&DL.drug_book_flg,						&DL.maccabicare_flag,
				&DL.assuta_drug_status,				&DL.t_half,								&DL.chronic_flag,
				&DL.chronic_months,					&DL.additional_price,					&DL.package,
				&DL.package_size,					&DL.package_volume,						&DL.openable_pkg,
				&DL.calc_op_by_volume,				&DL.expiry_date_flag,					&DL.shape_code,
				
				&DL.shape_code_new,					&DL.form_name,							&DL.split_pill_flag,
				&DL.supplemental_1,					&DL.supplemental_2,						&DL.supplemental_3,
				&DL.supplier_code,					&DL.computersoft_code,					&DL.bar_code_value,
				&DL.no_presc_sale_flag,				&DL.home_delivery_ok,					&DL.largo_type,
				&DL.in_gadget_table,				&DL.treatment_typ_cod,					&DL.in_health_pack,
				
				&DL.health_basket_new,				&DL.pharm_group_code,					&DL.member_price_code,
				&DL.price_prcnt,					&DL.must_prescribe_qty,					&DL.price_prcnt_magen,
				&DL.price_prcnt_keren,				&DL.enabled_status,						&DL.enabled_mac,
				&DL.enabled_hova,					&DL.enabled_keva,						&DL.drug_type,
				&DL.priv_pharm_sale_ok,				&DL.pharm_sale_new,						&DL.pharm_sale_test,

				&DL.status_send,					&DL.sale_price,							&DL.sale_price_strudel,
				&DL.interact_flg,					&DL.in_drug_interact,					&DL.in_overdose_table,
				&DL.magen_wait_months,				&DL.keren_wait_months,					&DL.drug_book_doct_flg,
				&DL.ishur_required,					&DL.Ingredient [0].code,				&DL.Ingredient [1].code,
				&DL.Ingredient [2].code,			&DL.Ingredient [0].quantity,			&DL.Ingredient [1].quantity,
				
				&DL.Ingredient [2].quantity,		&DL.Ingredient [0].units,				&DL.Ingredient [1].units,
				&DL.Ingredient [2].units,			&DL.Ingredient [0].per_quant,			&DL.Ingredient [1].per_quant,
				&DL.Ingredient [2].per_quant,		&DL.Ingredient [0].per_units,			&DL.Ingredient [1].per_units,
				&DL.Ingredient [2].per_units,		&DL.purchase_limit_flg,					&DL.qty_lim_grp_code,
				&DL.ishur_qty_lim_flg,				&DL.ishur_qty_lim_ingr,					&DL.rule_status,
				
				&DL.sensitivity_flag,				&DL.sensitivity_code,					&DL.sensitivity_severe,
				&DL.needs_29_g,						&DL.print_generic_flg,					&DL.copies_to_print,
				&DL.substitute_drug,				&DL.fps_group_code,						&DL.tikrat_mazon_flag,
				&DL.allow_future_sales,				&DL.Ingredient [0].quantity_std,		&DL.Ingredient [1].quantity_std,
				&DL.Ingredient [2].quantity_std,	&DL.illness_bitmap,						&DL.cont_treatment,
				
				&DL.how_to_take_code,				&DL.time_of_day_taken,					&DL.treatment_days,
				&DL.unit_abbreviation,				&DL.has_diagnoses,						&DL.tight_ishur_limits,
				&DL.qualify_future_sales,			&DL.maccabi_profit_rating,				&DL.is_orthopedic_device,
				&DL.needs_preparation,				&DL.needs_patient_measurement,			&DL.needs_patient_instruction,
				&DL.needs_refrigeration,			&DL.online_order_pickup_ok,				&DL.delivery_ok_per_shape_code,

				&DL.needs_dilution,					&DL.purchase_lim_without_discount,		&DL.ConsignmentItem,
				&DL.has_member_type_exclusion,		&DL.bypass_member_pharm_restriction,	&DL.compute_duration,
				&DL.tikrat_piryon_pharm_type,		&DL.is_narcotic,						&DL.psychotherapy_drug,
				&DL.preparation_type,				&DL.chronic_days,

				&DL.refresh_date,					&DL.refresh_time,						&DL.changed_date,
				&DL.changed_time,					&DL.pharm_update_date,					&DL.pharm_update_time,
				&DL.doc_update_date,				&DL.doc_update_time,					&DL.as400_batch_date,
				&DL.as400_batch_time,				&DL.update_date,						&DL.update_time,
				&DL.update_date_d,					&DL.update_time_d,						&DL.pharm_diff,
		
				&DrugCode,							END_OF_ARG_LIST																);
//GerrLogMini(GerrId, "read_drug: Largo %d gave SQLCODE %d. Pref Largo = %d, Price Code %d.",
//	DrugCode, SQLCODE, DL.preferred_largo, DL.member_price_code);

	if (SQLCODE == 0)
	{
		// Good read; now do the "post-SELECT select" on del_flg and assuta_drug_status.
		// DonR 08Apr2018: Added the new flag SeeDeletedDrugs_in. This lets us "see" deleted drugs
		// if the current operation is a sale deletion; we don't want to be unable to delete a
		// drug sale all because the drug was logically deleted after the sale took place!
		if ((DL.del_flg != drug_deleted)	||
			(SeeDeletedDrugs_in)			||
				((PharmCategory == pharmacy_assuta) &&
				 ((DL.assuta_drug_status == assuta_drug_active) || (DL.assuta_drug_status == assuta_drug_past))))
		{
			// This drug is OK - just drop through.
		}
		else
		{
			// This drug is deleted, isn't an Assuta drug for an Assuta pharmacy, and the current
			// transaction is not a sale deletion - so we want to treat the drug as "not found".
			FORCE_NOT_FOUND;
		}
	}

	// DonR 12Aug2003: Avoid division-by-zero problems by ensuring that package
	// size has some real value. (I don't know if there's any real chance of
	// problems, but paranoia is sometimes a good thing.)
	if (DL.package_size < 1)
	{
		DL.package_size = 1;
	}

	// DonR 05Feb2003: If the package size is 1, we must assume that the
	// package is non-openable EVEN IF OPENABLE_PKG IS NON-ZERO. This
	// assumption is necessary for now, because many drugs in the database
	// have openable_pkg set true even though they are not in fact openable.
	if (DL.package_size == 1)
	{
		DL.openable_pkg = 0;
	}

	// DonR 18Mar2018 CR #14502: Calculate "effective package size" for things like syrups
	// and creams where we need to calculate OP according to package volume.
	// DonR 24Apr2018: Remove condition on Package Size, since that's now one of the
	// criteria for setting calc_op_by_volume non-zero in the first place.
	if ((DL.calc_op_by_volume	>  0)		&&
//		(DL.package_size		== 1)		&&
		(DL.package_volume		>  1.0))
	{
		DL.effective_package_size = (int)(DL.package_volume + .000001);	// Add a smidgin to avoid rounding errors.
	}
	else
	{
		DL.effective_package_size = DL.package_size;
	}

	// DonR 07Nov2023 User Story #473527: Add monthly_package_size, which will be equal to an
	// even multiple of 30 for all "monthly" packages, and will hold the original package-size
	// value for all non-"monthly" packages. This is being done so that "monthly" prescriptions
	// and "monthly" packages can be compared on an equal basis.
	if ((DL.effective_package_size % 28) == 0)
		DL.monthly_package_size = 30 * (DL.effective_package_size / 28);
	else

	// Package sizes of 29 and 31 don't seem to exist in the real world, but just in case...
	if ((DL.effective_package_size % 29) == 0)
		DL.monthly_package_size = 30 * (DL.effective_package_size / 29);
	else

	// Package sizes of 29 and 31 don't seem to exist in the real world, but just in case...
	if ((DL.effective_package_size % 31) == 0)
		DL.monthly_package_size = 30 * (DL.effective_package_size / 31);
	else
		DL.monthly_package_size = DL.effective_package_size;	// Default to the original value.
	// DonR 07Nov2023 User Story #473527 end.

	// Another sanity check.
	if (DL.t_half < 0)
	{
		DL.t_half = 0;
	}

	// Sanity check on wait times:
	if (DL.magen_wait_months < -9)
	{
		DL.magen_wait_months = -9;	// Just to avoid bugs - negative wait times are unlikely!
	}
	if (DL.keren_wait_months < -9)
	{
		DL.keren_wait_months = -9;	// Just to avoid bugs - negative wait times are unlikely!
	}

	// DonR 27Nov2011 "Tzahal" enhancement: If this drug's relevant member-type-enabled flag
	// is set FALSE, any discount from drug_list is disabled; and the next bit of logic will
	// automatically disable the drug_list-based member discount as well, by setting the
	// in-health-basket flag FALSE.
	if (((DL.enabled_mac	<= 0) || (Member.MemberMaccabi	<= 0))	&&	// Non-Maccabi member OR non-Maccabi discount...
		((DL.enabled_hova	<= 0) || (Member.MemberHova		<= 0))	&&	// AND non-hova member OR non-hova discount...
		((DL.enabled_keva	<= 0) || (Member.MemberKeva		<= 0)))		// AND non-keva member OR non-keva discount.
	{
		DL.member_price_code = PARTI_CODE_ONE;
	}

	// DonR 22May2025 User Story #394813: We need to keep an "unprocessed" copy of the
	// in-health-basket flag, since in some cases (for Type 1 darkonaim) we need to
	// pass this value to Nihul Tikrot even though participation is set to 100%.
	DL.health_basket_new_unprocessed = DL.health_basket_new;

	// DonR 25Dec2007: Per Iris Shaya, if drug_list specifies 100%
	// participation, force Health Basket status false.
	if (DL.member_price_code == PARTI_CODE_ONE)
	{
		DL.health_basket_new = DL.in_health_pack = 0;
	}

	// DonR 13May2020 CR #31591: Set three boolean indicators based on whether this drug's
	// Largo Type matches the Sysparams strings for illness-based and non-illness-based
	// member discounts. (I set a value of 1 rather than -1 just in case someone does a
	// "> 0" comparison somewhere!)
	DL.IllnessDiscounts		= (strchr (LargoTypesForIllnessDiscounts,		DL.largo_type) != NULL)	? 1 : 0;
	DL.VentilatorDiscounts	= (strchr (LargoTypesForVentilatorDiscounts,	DL.largo_type) != NULL)	? 1 : 0;
	DL.NonIllnessDiscounts	= (strchr (LargoTypesForNonIllnessDiscounts,	DL.largo_type) != NULL)	? 1 : 0;

	// DonR 13Feb2022 TEMPORARY HACK: For two specific Largo Codes (Libre sensors),
	// force a value of 1 for purchase_lim_without_discount. This code will be
	// deleted once we get the value from AS/400 RK9001P.
	// DonR 01Jun2022: Added Largo 72753, which is replacing Largo 65158.
	if ((DrugCode == 65158) || (DrugCode == 65429) || (DrugCode == 72753))
		DL.purchase_lim_without_discount = 1;

	// DonR 13Feb2022: If the current transaction is coming from a non-Maccabi
	// pharmacy, force DL.purchase_lim_without_discount FALSE. If people want
	// to buy stuff at full price from non-Maccabi pharmacies, that's their
	// business. It takes all types...
	if (!MaccabiPharmacy)
		DL.purchase_lim_without_discount = 0;

	// DonR 06Feb2024 User Story #540234: Add a new flag to suppress all future sales
	// for cannabis - even if the member lives overseas.
	DL.PreventFutureCannabisSales = ((DL.drug_type == 'K') && (!CannabisAllowFutureSales));

	// Copy results to whatever structure pointers have been passed.
	if (DL_out != NULL)
	{
		*DL_out = DL;
	}

	// DonR 11Jul2010: The DL structure now has all the ingredients data, and thus the sPres structure
	// no longer has an Ingredients array. Also, the calling function is responsible for copying the
	// drug's health-basket parameter into the sPres array, since in some cases (e.g. Trn. 5003 deletions)
	// doing so is incorrect.
	if (PresDrugs_out != NULL)
		PresDrugs_out->DL = DL;


	// Return TRUE if the read was successful; any failure will return 0. Normal error-handling should
	// follow any call to this function if it returns zero.
	return (SQLCODE == 0);
}


/*				   -- EOF --				     */
