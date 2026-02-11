/*=======================================================================
||				MsgHndlr.h												||
||																		||
||======================================================================||
||																		||
||  PURPOSE:															||
||																		||
||  Define all data-types and sizes of data comeing from T-SWITCH.		||
||																		||
||----------------------------------------------------------------------||
|| PROJECT:	SqlServer. (for macabi)										||
||----------------------------------------------------------------------||
|| PROGRAMMER:	Gilad Haimov.											||
|| DATE:	Aug 1996.													||
|| COMPANY:	Reshuma Ltd.												||
|| WORK DONE:	Reciving message and geting the data (t-switch simulate)||
||----------------------------------------------------------------------||
|| PROGRAMMER:	Boaz Shnapp.											||
|| DATE:	Nov 1996.													||
|| COMPANY:	Reshuma Ltd.												||
|| WORK DONE:	S.Q.L transactions.										||
||----------------------------------------------------------------------||
||		  Make sure that file dont include 2 times.						||
 =======================================================================*/
#ifndef MSG_HANDLER_H
#define MSG_HANDLER_H

#include "TreatmentTypes.h"
#include "traceback.h"
#include "string.h"
#include "cJSON.h"
#include <curl/curl.h>

/*=======================================================================
||			   General definitions										||
 =======================================================================*/
#ifndef NON_SQL
	typedef struct	// In-memory version of prescr_source table, minus description.
	{
		short		pr_src_code;
		short		pr_src_docid_type;
		short		id_type_accepted;
		short		PrSrcDocLocnRequired;
		short		pr_src_priv_pharm;
		short		allowed_by_default;
	} PRESCR_SOURCE;


	#ifndef BOOL_DEFINED
		#define BOOL_DEFINED
		typedef enum tag_bool
		{
		  false = 0,
		  true
		}
		bool;                  /* simple boolean typedef */
	#endif
#endif

typedef struct pharm_trn_permissions
{
	short	transaction_num;
	short	spool;
	short	permitted[3];
	short	MinVersionPermitted;
} PHARM_TRN_PERMISSIONS;



// If not in main module, define all global variables as "extern".
#ifdef	MAIN
	int				PPlusMeisharEnabled		= 0;	// Default to disabled mode.
	int				IshurCycleOverlap		= 10;
	short			SaleOK_EarlyDays		= 0;	// DonR 21Aug2023: Changed variable name and default to match current values.
	short			UseSaleTableDiscounts	= 0;	// Default to disabled mode.
	short			CheckCardDate			= 1;	// CR #11541 - if zero, Trn. 6001 will not worry
													// about expired cards.

//	short			PrSrcNumber				[100];
//	short			PrSrcDocLocnRequired	[100];
	PRESCR_SOURCE			PrescrSource	[100];
	PHARM_TRN_PERMISSIONS	TrnPermissions	[200];

	// DonR 14Aug2024 User Story #349368: Expand Largo Code from 5 to 9 (really 6)
	// digits, and add support for setting a minimum valid Transaction Version Number
	// in the pharm_trn_permit table. For convenience, create new global variables
	// VersionNumber and VersionLargoLen.
	short			VersionNumber;
	short			VersionLargoLen;

	short			BakaraKamutitHistoryMonths;

	int				MaxDailyBuy				[10];
	int				MaxFamilyOtcCreditPerMonth;

	char			LastSignOfLife			[512];

	int				NoCardSvc_Max_Validity;
	int				NoCardSvc_After_Pvt_Purchase;
	int				NoCardSvc_After_Mac_Purchase;
	short			SvcWithoutCardValidDays;	// DonR 13Feb2018: For service w/o card set by Haverut or "by hand".
	short			SvcWithoutCardValidDaysPvt;	// DonR 19Oct2023 User Story #487690: For service w/o card set by Haverut (or by hand) at private pharmacies.

	short			SendRoundedPackagesToPharm	= 0;
	short			RoundPackagesToCloseRx		= 0;
	short			MaxUnsoldOpPercent			= 50;
	float			MaxRemainderPerOp			= 3.0;

	short			DeletionsValidDays			= 91;

	int				BypassHealthBasketBitmap		= 0;
	int				ActualDiseaseBitmap				= 0;
	int				DiseasesWithoutDiagnosesBitmap	= 0;
	int				DiseasesWithDiagnosesBitmap		= 0;

	short			TestSaleEquality			= 1;
	short			DDI_Pilot_Enabled			= 0;

	short			OnlineOrderRememberUsageMonths	=  3;
	short			OnlineOrderWaitDays				= 10;
	short			OnlineOrderMaxWorkHours			= 48;

	// DonR 14Sep2020 CR #30106.
	short			FutureSalesMaxHist				= 183;
	short			FutureSalesMinHist				=  20;

	// DonR 13May2020 CR #31591: Add global strings to determine which Largo Types are
	// eligible for illness-based and non-illness-based member discounts.
	// Just for clarity, here are the definitions of the Largo Types we're interested in
	// as of May 2020:
	// T = Treatments
	// M = Vitamins, herbal supplements
	// B = Food supplements (e.g. Ensure, baby formula)
	// Y = Equipment
	// D = Diagnostic Equipment
	char			LargoTypesForIllnessDiscounts		[21];	// Default = "TM".
	char			LargoTypesForVentilatorDiscounts	[21];	// Default = "BYD".
	char			LargoTypesForNonIllnessDiscounts	[21];	// Default = "TM".

	int				pkg_price_for_sms	= 300000;		// DonR 06Aug2020 CR #33003.

	// DonR 28Nov2021 User Story #205423.
	// DonR 25Jan2024 User Story #545773: Added MacPharmMaxGSL_OP_PerDay after its "twin"
	// parameter MacPharmMaxOTC_OP_PerDay.
	short			MacPharmMaxOTC_OP_PerDay		= 0;	// Default - feature disabled.
	short			MacPharmMaxGSL_OP_PerDay		= 0;	// Default - feature disabled.

	// DonR 23Feb2022 User Story #227221.
	short			EnableSecretInvestigatorLogic	= 1;	// Default - feature ENABLED.
	
	// Marianna 27Mar2022 Epic #232192.
	short			DarkonaiMaxHishtatfutPct; 

	// DonR 26Dec2022
	short			SaleCompletionMaxDelay			= 0;	// Default - feature DISABLED.

	// DonR 29Jun2023 User Story #461368
	int				alt_price_list_first_date;
	int				alt_price_list_last_date;
	short			alt_price_list_code;
	
	// Marianna 09Jul2023 User Story #463302.
	short 			alt_price_list_only_if_cheaper;

	// DonR 03Aug2023 User Story #469361.
	short			AdjustCalculateDurationPercent;

	// DonR 28Jan2024 User Story #540234: Cannabis-sale control parameters.
	short			CannabisSaleEarlyDays;
	short			CannabisAllowFutureSales;
	short			CannabisAllowDiscounts;
	short			CannabisCallNihulTikrot;
	short			CannabisPermitPerPharmacy;

	short			default_narco_max_duration		=  5;	// DonR 28Oct2025 User Story #429086.
	short			default_narco_max_validity		=  5;	// DonR 28Oct2025 User Story #429086.
	short			exempt_narco_max_duration		= 31;	// DonR 28Oct2025 User Story #429086.
	short			exempt_narco_max_validity		= 15;	// DonR 28Oct2025 User Story #429086.

#else
	extern int		PPlusMeisharEnabled;
	extern int		IshurCycleOverlap;
	extern short	SaleOK_EarlyDays;
	extern short	UseSaleTableDiscounts;
	extern short	CheckCardDate;	// CR #11541 - if zero, Trn. 6001 will not worry
									// about expired cards.

//	extern short	PrSrcNumber				[];
//	extern short	PrSrcDocLocnRequired	[];

#ifndef NON_SQL
	extern PRESCR_SOURCE	PrescrSource	[];
#endif

	extern			PHARM_TRN_PERMISSIONS	TrnPermissions	[];

	// DonR 14Aug2024 User Story #349368: Expand Largo Code from 5 to 9 (really 6)
	// digits, and add support for setting a minimum valid Transaction Version Number
	// in the pharm_trn_permit table. For convenience, create new global variables
	// VersionNumber and VersionLargoLen.
	extern short	VersionNumber;
	extern short	VersionLargoLen;

	extern short	BakaraKamutitHistoryMonths;

	extern int		MaxDailyBuy				[];
	extern int		MaxFamilyOtcCreditPerMonth;

	extern char		LastSignOfLife			[];

	extern int		NoCardSvc_Max_Validity;
	extern int		NoCardSvc_After_Pvt_Purchase;
	extern int		NoCardSvc_After_Mac_Purchase;
	extern short	SvcWithoutCardValidDays;	// DonR 13Feb2018: For service w/o card set by Haverut or "by hand".
	extern short	SvcWithoutCardValidDaysPvt;	// DonR 19Oct2023 User Story #487690: For service w/o card set by Haverut (or by hand) at private pharmacies.

	extern short	SendRoundedPackagesToPharm;
	extern short	RoundPackagesToCloseRx;
	extern short	MaxUnsoldOpPercent;
	extern float	MaxRemainderPerOp;

	extern short	DeletionsValidDays;

	extern int		BypassHealthBasketBitmap;
	extern int		ActualDiseaseBitmap;
	extern int		DiseasesWithoutDiagnosesBitmap;
	extern int		DiseasesWithDiagnosesBitmap;

	extern short	TestSaleEquality;
	extern short	DDI_Pilot_Enabled;

	extern short	OnlineOrderRememberUsageMonths;
	extern short	OnlineOrderWaitDays;
	extern short	OnlineOrderMaxWorkHours;
	extern double	IngrSubstMaxRatio;		// DonR 12Jul2018  "special" ingredient-based substitution outside EconomyPri group.

	// DonR 14Sep2020 CR #30106.
	extern short	FutureSalesMaxHist;
	extern short	FutureSalesMinHist;

	// DonR 13May2020 CR #31591: Add global strings to determine which Largo Types are
	// eligible for illness-based and non-illness-based member discounts.
	extern char			LargoTypesForIllnessDiscounts		[];
	extern char			LargoTypesForVentilatorDiscounts	[];
	extern char			LargoTypesForNonIllnessDiscounts	[];

	extern int			pkg_price_for_sms;				// DonR 06Aug2020 CR #33003.

	// DonR 28Nov2021 User Story #205423.
	// DonR 25Jan2024 User Story #545773: Added MacPharmMaxGSL_OP_PerDay after its "twin"
	// parameter MacPharmMaxOTC_OP_PerDay.
	extern short		MacPharmMaxOTC_OP_PerDay;
	extern short		MacPharmMaxGSL_OP_PerDay;

	// DonR 23Feb2022 User Story #227221.
	extern short		EnableSecretInvestigatorLogic;
	
	// Marianna 27Mar2022 Epic #232192.
	extern short		DarkonaiMaxHishtatfutPct;

	// DonR 26Dec2022
	extern short		SaleCompletionMaxDelay;

	// DonR 29Jun2023 User Story #461368
	extern int			alt_price_list_first_date;
	extern int			alt_price_list_last_date;
	extern short		alt_price_list_code;
	
	// Marianna 09Jul2023 User Story #463302.
	extern short 		alt_price_list_only_if_cheaper;		

	// DonR 03Aug2023 User Story #469361.
	extern short		AdjustCalculateDurationPercent;

	// DonR 28Jan2024 User Story #540234: Cannabis-sale control parameters.
	extern short		CannabisSaleEarlyDays;
	extern short		CannabisAllowFutureSales;
	extern short		CannabisAllowDiscounts;
	extern short		CannabisCallNihulTikrot;
	extern short		CannabisPermitPerPharmacy;

	extern short		default_narco_max_duration;	// DonR 28Oct2025 User Story #429086.
	extern short		default_narco_max_validity;	// DonR 28Oct2025 User Story #429086.
	extern short		exempt_narco_max_duration;	// DonR 28Oct2025 User Story #429086.
	extern short		exempt_narco_max_validity;	// DonR 28Oct2025 User Story #429086.

#endif

// Logic macros.
#define SECRET_INVESTIGATOR	(EnableSecretInvestigatorLogic && (!Member.check_od_interact))

// Return values:
#define RC_SUCCESS			1
#define RC_FAILURE			0
#define	EXTRA_MEMORY		20000

// Action Types for Trn. 5003/5005/6003/6005.
#define DRUG_SALE				 1
#define SALE_DELETION			 2
#define RETURN_FOR_CREDIT		 3
#define RETURN_DELETION			 4
#define MARK_AS_PAID			90

// Array dimensions for "Nihul Tikrot"
#define MAX_TIKRA_CURR_SALE		50
#define MAX_TIKRA_PREV_SALES	50
#define MAX_FAMILY_SIZE			25
#define MAX_TIKRA_TIKROT_OUT	50
#define MAX_TIKRA_COUPONS_OUT	10

#define ONE_OR_BLANK(i)		(((i) == 0) ? ' ' : '1')
//#define ONE_TWO_BLANK(i)	((((i) == 0) || ((i) < 0) || ((i) > 2)) ? ' ' : ((char)(i) + '0'))
#define ONE_TWO_BLANK(i)	((((i) == 1) || ((i) == 2)) ? ((char)(i) + '0') : ' ')
#define DIGIT_OR_BLANK(i)	((((i) == 0) || ((i) < 0) || ((i) > 9)) ? ' ' : ((char)(i) + '0'))

// Permission type ( table PHARMACY ) :
#define PERMISSION_TYPE_PRIVATE		0
#define PERMISSION_TYPE_MACABI		1
#define PERMISSION_TYPE_ANY			2
#define PERMISSION_TYPE_PRATI_PLUS	6

// Hesder Types
#define MACCABI_PHARMACY_HESDER		0
#define PRIVATE_WITH_HESDER			1
#define NON_HESDER_PHARMACY			2


// DonR 26Oct2008: New #define's to handle enhancement of in-health-basket logic.
#define	HEALTH_BASKET_NO		0
#define	HEALTH_BASKET_YES		1
#define	HEALTH_BASKET_KE_ILU	4
#define	HEALTH_BASKET_NEW_YES	5


// DonR 20Apr2021 for JSON.
#define MANDATORY				1
#define OPTIONAL				0
#define NON_MANDATORY			0


// DonR 09Sep2009: Pharmacy Categories ("shaichut")
#define PHARMACY_ASSUTA			20

// DonR 09Sep2009: Assuta Drug Status values (and the standard drug-deleted value)
#define ASSUTA_DRUG_NOT			0
#define ASSUTA_DRUG_ACTIVE		1
#define ASSUTA_DRUG_PAST		2
#define ASSUTA_DRUG_CANCELLED	3
#define DRUG_DELETED			9

// DonR 06Nov2011: Macros for member-hospitalization status:
#define NOT_HOSPITALIZED		0
#define IN_HOSPITAL				1
#define DISCHARGED				2
#define ABSENT_NOT_DISCHARGED	3


// TRUE value for field no_presc_flg ( table drug_extension ):
#define NO_PRESC_SRC		1
#define PRESC_SRC			0


// Interaction severity
#define SEVER_INTERACTION		1
#define	NOT_SEVER_INTERACTION	2


// ALL PHARM PERMISSIONS
#define ALL_PHARM			2


// DonR 07Jul2013: New "Bakara Kamutit" constants.
#define FUTURE_SALE_FORBIDDEN			0
#define FUTURE_SALE_NOT_YET_QUALIFIED	1	// DonR 15Sep2020 CR #30106.
#define FUTURE_SALE_MAC_ONLY			2
#define FUTURE_SALE_ALL_PHARM			3

#define NO_PURCHASE_LIMIT		0
#define STD_PURCHASE_LIMIT		1
#define SP_PR_PURCHASE_LIMIT	2
#define PURCHASE_LIMIT_ISHUR	3
#define RX_SOURCE_PURCHASE_LIM	4
#define MEISHAR_PURCHASE_LIMIT	5

#define FUT_PERMITTED_ALL_PHARM	1
#define FUT_PERMITTED_MACCABI	2
#define FUT_PERMITTED_OVRSEAS	3
#define FUT_PERMITTED_ISHUR		4

// DonR 11Dec2018 CR #15277: Values for prescr_source/id_type_accepted.
#define	DOC_TZ_ONLY				1
#define	DOC_LICENSE_ONLY		2
#define	DOC_LICENSE_OR_TZ		3
#define	DOC_LICENSE_IRRELEVANT	9

// Pharmacy Ishur defines - for Electronic Prescriptions.
#define	ISHUR_UNUSED		0
#define	ISHUR_USED			1

// Action Type defines for Health Alert Rule initializing/loading.
#define INITIALIZE_ONLY		0
#define CHECK_MEMBER		1

// DonR 01Mar2017: Add support for multiple ingredient codes in the same Drug Purchase Limit group.
#define LIMIT_GROUP_MAX_INGRED	25

// DonR 04Jan2022: Add some constants for new drug_list flags.
#define NEEDS_REFRIGERATION		1
#define NEEDS_FREEZER			2


// Test macros
void log_serious_error_text	(int err, char *msg);
void log_serious_error_int	(int err, int err_identifier);

#define CHECK_ERROR()					\
	if (glbErrorCode != NO_ERROR)		\
	{									\
		RESTORE_ISOLATION;				\
	    return glbErrorCode;			\
	}

#define CHECK_ERROR_T(msg)							\
	if (glbErrorCode != NO_ERROR)					\
	{												\
		log_serious_error_text (glbErrorCode, msg);	\
		RESTORE_ISOLATION;							\
	    return glbErrorCode;						\
	}

#define CHECK_ERROR_I(identifier)							\
	if (glbErrorCode != NO_ERROR)							\
	{														\
		log_serious_error_int (glbErrorCode, identifier);	\
		RESTORE_ISOLATION;									\
	    return glbErrorCode;								\
	}

#define SET_ERROR( err_code )                   \
        glbErrorCode = (err_code)


// DonR 13Aug2024 User Story #349368: Macro to get an optional
// Transaction Version Number at the end of a pharmacy request.
#define GET_TRN_VERSION_IF_SENT		if ((TotalInputLen - (PosInBuff - IBuffer)) > 1)					\
									{	VersionNumber = GetShort (&PosInBuff, 2); CHECK_ERROR ();	}	\
									else																\
									{	VersionNumber = 1;											}


/*
 *  Message buffers sizes
 *  ---------------------
 */
#define IBUFFER_SIZE				8000	/* input buffer size	*/
#define MAX_MSG_DIM					8000
#define OBUFFER_SIZE				8000	/* output buffer size	*/
#define MAX_FIELD_LEN				256
#define MAX_REC						10		/* Maximum sub records	*/
#define MAX_REC_ELECTRONIC			99		// DonR 05Jan2015 - Changed back from 999 to 99 for Digital Prescriptions.
#define MAX_DOC_RX					1000	// Assumes a maximum of 10 doctor prescriptions per drug line - which is a lot!
#define MAX_TRN6001_DRUGS			999
#define MAX_TRN6011_LARGOS_TO_SELL	99		// Same value as MAX_REC_ELECTRONIC - no need to make it bigger than that.
#define MAX_TRN6011_PRIOR_SALES		999
#define MAX_TRN6011_ISHURIM			149		// As of July 2025, the real-world maximum is 36.
#define MAX_1002_MSG				5	/* Maximum number of return message	*/
#define MAX_CANNABIS_PRODUCTS		100		// DonR 01Feb2024 User Story #540234.

/*=======================================================================
||		       Global message buffersvariables			||
 =======================================================================*/
static char		IBuffer[IBUFFER_SIZE] ;

static char		OBuffer[OBUFFER_SIZE] ;

static char		glbBuffer[8000] ;

/*static*/ int		glbErrorCode;
		/* Global error flag	*/
static int		out_msg_index;

static char		*get_mem;
static FILE         *in;
			/* input file handle	*/
static FILE 		*out;
			/* output file handle	*/

/*=======================================================================
||			    Messages constants.										||
 =======================================================================*/
#define RECIP_SRC_NO_PRESC				0
#define RECIP_SRC_MACABI_DOCTOR			1
#define RECIP_SRC_HOSPITAL				2
#define RECIP_SRC_OLD_PEOPLE_HOUSE		3
#define RECIP_SRC_DENTIST				4
#define RECIP_SRC_PRIVATE				5
#define RECIP_SRC_MACABI_STAMP			6
#define RECIP_SRC_MAC_DOC_BY_HAND		7
#define RECIP_SRC_MOKED_MACCABI			8
#define RECIP_SRC_MEUCHEDET				9
#define RECIP_SRC_LEUMIT				10
#define RECIP_SRC_HOME_VISIT			11
#define RECIP_SRC_MACCABI_NURSE			12
#define RECIP_SRC_AFFILIATED_CLINIC		13
#define RECIP_SRC_PHARMACIST_EQUIP		15
#define RECIP_SRC_MAC_DIETICIAN			16
#define RECIP_SRC_MAC_PHYS_THERAPIST	17
#define RECIP_SRC_EMERGENCY_NO_RX		18
#define RECIP_SRC_CONTINUED_PRESC		90
#define RECIP_SRC_MIXED_SOURCES			99

#define NO_MACBI_MAGE				-1
#define NO_KEREN_MACBI				-1

#define MACABI_INSTITUTE			2
#define SUM_MAC_PRICE				1

#define NO_INTERACTION				0


#define FROM_UNKNOWN				0	// unknown source

// Note that there are three macros (one of them no longer in use) that resolve
// to 1. At some point we may change things so that we can use values with more
// than one digit; in that case, maybe we'll give DRUG_SHORTAGE_RULE a new value
// (although really, it's conceptually very similar to FROM_DOCTOR_NOHAL since
// both involve applying a "noahal" automatically even though its "gorem me'asher"
// code is greater than zero).
//#define FROM_DOC_AUTHORIZED		1	// doctor_percents - NIU since October 2004.
#define FROM_DOCTOR_NOHAL			1	// DonR 24Apr2025 User Story #390071.
#define DRUG_SHORTAGE_RULE			1	// DonR 13May2025 User Story #388766.

#define FROM_DOC_SPECIALIST			2	// includes dentists!
#define FROM_DRUG_LIST_TBL			3	// drug_list (default)
#define FROM_DRUG_EXTENSION			4
#define	FROM_MACCABI_PRICE			5
#define FROM_SPEC_PRESCS_TBL		6
#define FROM_NO_TABLE				7	// No doctor prescription (?)
#define	FROM_PHARMACY_ISHUR			8
#define	FROM_GADGET_APP				9

// DonR 21Sep2004: Values for new InsuranceUsed flag in TPresDrugs structure.
#define NO_INS_USED			0
#define BASIC_INS_USED		1
#define ZAHAV_INS_USED		2
#define KESEF_INS_USED		3
#define YAHALOM_INS_USED	7

#define CURR_CODE				2


// DonR 14Jan2009: Reasons for assigning specialist participation.
#define PRESCRIBED_BY_SPECIALIST	1
#define DOCTOR_SAW_SPEC_LETTER		2
#define PHARMACY_ISHUR				3
#define PURCHASE_HISTORY			4
#define	OLD_PEOPLES_HOME			5
#define DENTIST_ETC					6


// DonR 14Jan2009: Explanation for sale of non-preferred drug.
//
// DonR 26Apr2009: Per Iris Shaya, NON_GENERIC_SOLD_AS_IS is now
// equal to 0 instead of 3. This means that the only time there
// will be a non-zero value in this slot is when a non-generic
// drug is being sold under generic-drug conditions.
// DonR 28Mar2010: Added new reason code for doctor's ishur based
// on a "tofes" that expired within the last 91 days.
#define NON_GENERIC_SOLD_AS_IS	0
#define DOC_AUTH_NON_GENERIC	1
#define PHARM_AUTH_NON_GENERIC	2
#define DOC_AUTH_EXPIRED_TOFES	3
#define INGR_BASED_SUBSTITUTION	4


// Reasons to dispense non-preferred drugs:
#define NO_REASON_GIVEN				 0
#define DOCTOR_NIXED_SUBSTITUTION	 1
#define OUT_OF_STOCK_SUB_EQUIV		 2
#define EXCESS_STOCK				 3
#define	OUT_OF_STOCK_SUPPLIER		 7	// NIU as of 23Nov2017
#define	OUT_OF_STOCK_SUB_OTHER		 8
#define DISPENSED_KACHA				99


/*===================================================
	We must define to way for
	1)host_variables
	2)"defines" for using for arrays definition
	3) "NAMES" that use in both 1),2)
	2000.05.23
====================================================*/
#define	PRINT			 1
#define	NO_PRINT		 0
#define	MAX_DYN_VAR	   100
//yulia 2000.05.23#define PAR_INT				 1
//yulia 2000.05.23#define PAR_ASC				 2
//yulia 2000.05.23#define  PAR_DAT			 3
/*end 2000.05.03*/



//temp comment
//#define DUR_CHECK			0
/*=======================================================================
||			    Database constants.				||
 =======================================================================*/


/*============================================================================
 * change all define's to host variables with static "type" = const options  *
 * add to ORACLE translation                                       2000.04.27*
 *===========================================================================*/


#ifdef	MAIN

	short   DRUG_NOT_DELIVERED	= 0;
	short   DRUG_DELIVERED		= 1;

	short   ROW_NOT_DELETED		= 0;
	short   ROW_DELETED			= 1;
	short   ROW_SYS_DELETED		= 2;
	short   ROW_ALREADY_DELETED	= 3;

	short   PHARM_CLOSE_SQL			= 0;
	short   PHARM_OPEN_SQL			= 1;

	short   NOT_REPORTED_TO_AS400			= 0;
	short   REPORTED_TO_AS400		 	    = 1;
	short   NO_NEED_TO_REPORT_TO_AS400		= 2;
	short   SET_ASIDE_AMT_DISCREPANCY		= 3;

	int		DUR_CHECK			= 0;
	int		OVER_DOSE_CHECK	= 1;
	short	PHARMACIST		= 0;
	short	DOCTOR			= 1;

#else
	extern short	DRUG_NOT_DELIVERED;
	extern short	DRUG_DELIVERED;
	extern short	ROW_NOT_DELETED;
	extern short	ROW_DELETED;
	extern short	ROW_SYS_DELETED;
	extern short	ROW_ALREADY_DELETED;
	extern short	NOT_REPORTED_TO_AS400;
	extern short	REPORTED_TO_AS400;
	extern short	NO_NEED_TO_REPORT_TO_AS400;
	extern short	SET_ASIDE_AMT_DISCREPANCY;
	extern short	PHARM_CLOSE_SQL;
	extern short	PHARM_OPEN_SQL;
	extern int		DUR_CHECK;
	extern int		OVER_DOSE_CHECK;
	extern short	PHARMACIST;
	extern short	DOCTOR;
#endif


#define PHARM_OPEN		1
#define PHARM_CLOSE		0


/*=======================================================================
||			Length of (ascii) data types			||
 =======================================================================*/

/* <-----------------------------------------------------------	*
 *  Warning:							*
 *      In order to change the length of a STRING field		*
 *      you must also change the length in section		*
 *     "STRING TYPES" below.					*
 * ----------------------------------------------------------->	*/

#define MSG_ID_LEN				5 /* len of message ID	*/
#define MSG_ERROR_CODE_LEN			4 /* len of message dim	*/
#define MSG_HEADER_SIZE		MSG_ID_LEN + MSG_ERROR_CODE_LEN

// DonR 07 Jan 2003 - Add some generic type fields - since many messages have standard data elements.
#define TGenericFlag_len				1
#define TGenericYYYYMMDD_len			8
#define TGenericYYMM_len				4
#define TGenericHHMMSS_len				6
#define TGenericTZ_len					9
#define TGenericAmount_len				9

#define TAllowedProcList_len		       50
#define TAllowedBelongings_len		       50
#define TYarpaParticipationCode_len		2
#define TCalcTypeCode_len			4
#define TPriceForOP_len				9
#define TParticipationName_len		       25
#define TPharmacistIDExtension_len		1
#define TCreditYesNoFlg_len			1
#define TConfirmationReason_len			2
#define THospitalCode_len			5
#define TConfirmationType_len			2
#define Tdiary_month_len			4
#define Tdiary_new_month_len			6
#define TMedNumOfMessageLines_len		4
#define TClosingCode_len  			1
#define TInstituteCode_len  			2
#define TTerminalNum_len  			2
#define TPriceListNum_len  			3
#define TPharmacyType_len  			1
#define TNet_len  				1
#define TCommunicationType_len  		1
#define TFreeMemory_len  			4
#define TMessage_len  				1
#define TGlobalPricelistUpdate_len  		1
#define TKabalaNum_len				9
#define TMedNumOfDrugLines_len			4
#define TMaccabiPricelistUpdate_len  		1
#define TErrorCodeListUpdate_len  		1
#define TPurchasePricelistUpdate_len  		1
#define TDrugBookUpdate_len  			1
#define TDiscountListUpdate_len  		1
#define TSuppliersListUpdate_len  		1
#define TInstituteListUpdate_len  		1
#define TReserve1_len				4
#define TReserve2_len				4
#define TOpenMessageLenghtInLines_len		1
#define TErrorCode_len  			4
#define TIdentificationCode_len  		1
#define TMemberRights_len  			3
#define TMemberRightsCode_len			3
#define TDocTerminalNum_len			7
#define TNumOfDrugLinesRecs_len  		1
#define TMemberParticipatingType_len  		4
#define TDrugAnswerCode_len  			4
#define TBilNum_len				10
#define TNumDrugsUpdated_len			9
#define TMemberBelongCode_len			2
#define TMoveCard_len				4
#define TTypeDoctorIdentification_len		1
#define TDoctorIdInputType_len  		1
#define TRecipeSource_len  			2
#define TPaymentParticipatingType_len		2
#define TParticipatingType_len  		4
#define TParticipatingPrecent_len  		5
#define TParticipatingAmount_len  		5
#define TSpecConfNumSrc_len			  	1
#define TMonthLog_len				4
#define TDuration_len				3
#define TCancelAuthNum_len  		9
#define TInteractionType_len  			4
#define TSeverityLevel_len  			1
#define TUpdateCode_len  			1
#define TAnotherParticipatingTypeExist_len  	1
#define TRegisteredInDrugNote_len  		1
#define TMaccabiPriceListNum_len  		3
#define TNewMaccabiPriceListNum_len  		3
#define TOrderRecordsNum_len  			5
#define TUrgencyCode_len  			1
#define TPackageUnits_len  			5
#define PurchaseOrderConfirmationCode_len  	9
#define PurchaseOrderConfirmRecordNum_len  	5
#define TActionType_len  			2
#define TNumOfDrugLines_len  			4
#define TAmountType_len  			1
#define TAnotherMessage_len  			1
#define TAcceptedLinesNum_len			5
#define TRealAcceptedRecordsNum_len  		4
#define TDoseRenewalFrequencyInDays_len  	3
#define TDoseNum_len  				2
#define TSourceValidSpecialRecipeNum_len  	1
#define TMaccabiKerenCode_len  			3
#define TMaccabiMagenCode_len  			3
#define TSpecialRecipeNumSource_len  		1
#define TSpecialRecipeNumCode_len  		1
#define TCanDeliverRecipeInPrivPharm_len  	1
#define TErrorCodeToUpdateNum_len  		4
#define TErrorSevirity_len  			2
#define TNumOfLinesToUpdate_len  		4
#define TSpecialConfirmMessageFlag_len		1
#define TActionCode_len  			1
#define TCreditYesNo_len  			2
#define TAuthorizarionNum2_len  		3
#define TRecordNum_len  			4
#define TStationNum_len  			2
#define TPaymentType_len  			2
#define TSalesActionNum_len  			4
#define TRefundActionNum_len  			4
#define TPharmNum_len				7
#define TUserIdentification_len  		9
#define THardwareVersion_len  			9
#define TAvailableDisk_len  			9
#define TFlushingFileSize_len			9
#define TLastBackupDate_len  			8
#define TDate_len  				8
#define THour_len  				6
#define TPriceListUpdateDate_len  		8
#define TPriceListUpdateHour_len  		6
#define TLastPriceListUpdateDate_len  		8
#define TLastPriceListUpdateHour_len  		6
#define TMaccabiPriceListUpdateDate_len  	8
#define TPurchasePriceListUpdateDate_len  	8
#define TDrugBookUpdateDate_len  		8
#define TDiscountListUpdateDate_len  		8
#define TSuppliersListUpdateDate_len  		8
#define TInstituteListUpdateDate_len  		8
#define TErrorCodeListUpdateDate_len  		8
#define TRecipeIdentifier_len			9
#define TMemberIdentification_len  		9
#define TMemberDateOfBirth_len  		8
#define TConfiramtionDate_len			8
#define TConfirmationHour_len			6
#define TLineNum_len				4
#define TDrugCode_len				5
#define TUnits_len  				5
#define TOp_len  				5
#define TDrugCodeInInteraction_len  		5
#define TOpDrugPrice_len  			9
#define TDoctorIdentification_len  		9
#define TInvoiceNum_len  			7
#define TDoctorRecipeNum_len  			6
#define TPharmRecipeNum_len  			6
#define TDeviceCode_len  			7
#define TDeviceNum_len  			7
#define TDeliverConfirmationDate_len  		8
#define TDeliverConfirmationHour_len  		6
#define TNumofSpecialConfirmation_len 		8
#define TCurrentStockOp_len  			9
#define TCurrentStockInUnits_len  		9
#define TTotalMemberParticipation_len		9
#define TTotalDrugPrice_len  			9
#define TLinkDrugToAddition_len  		5
#define TRecipeNum_len  			6
#define TSpecialRecipeStartDate_len  		8
#define TCancleRecipeNum_len  			6
#define TCancleInvoiceNum_len			7
#define TSpecialRecipeStopDate_len  		8
#define TLastDrugUpdateDate_len  		8
#define TLastDrugUpdateHour_len  		6
#define TDrugFileUpdateDate_len  		8
#define TDrugFileUpdateHour_len  		6
#define TNumOfDrugsToUpdate_len  		9
#define TPackageSize_len  			5
#define TSupplierCode_len  			5
#define TCompuSoftDrgCode_len  			6	// DonR 17Oct2017 5 -> 6.
#define TDrugAddition1Code_len  		5
#define TDrugAddition3Code_len  		5
#define TDrugAddition2Code_len  		5
#define TDrugPriceForUpdateNum_len  		2
#define TNewMaccabiPrice_len  			9
#define TPurchasePrice_len  			9
#define TYarppaDrugCode_len  			7
#define TCurrentStockLeftOver_len  		9
#define TOrderAmount_len  			9
#define TManufactorCode_len  			5
#define TInternalOrderNum_len			8
#define TAuthorizationNum_len			7
#define TSuppDevPharmNum_len  			7
#define TDiscountAmount_len  			9
#define TDiscountPrecent_len  			5
#define TTotalAfterDiscount_len  		9
#define TTotalAfterVat_len  			9
#define TOpAmout_len  				5
#define TUnitsAmout_len  			5
#define TUnitPriceFor1Op_len  			9
#define TTotalForItemLine_len			9
#define TFullOpStock_len  			7
#define TCurrentUnitStock_len			7
#define TMinOpStock_len  			5
#define TEnterDate_len  			8
#define TEnterHour_len  			6
#define TFromDate_len				8
#define TToDate_len  				8
#define TTotalFinanceAccumulate_len  		9
#define TMemberParticipatingAccumulate_len  	9
#define TNumOfSoldRecipe_len  			9
#define TNumOfSoldDrugLines_len  		9
#define TCancleFinanceAccumulate_len  		7
#define TNumOfCancledRecipeLines_len  		9
#define TDeliverFailedCommFinanceAccum_len  	9
#define TDeliverFailedCommLinesNum_len  	9
#define TMessageDate_len  			8
#define TMessageHour_len  			6
#define TNumOfMessageLines_len  		2
#define TMessageLine_len  			60
#define TSupervisorPharmecistIdent_len  	9
#define TSupervisorPharmecistCardValid_len  	4
#define TDoseDoseageInOp_len  			5
#define TDoseDoseageInUnits_len  		5
#define TTreatmentStartDate_len  		8
#define TTreatmentStopDate_len  		8
#define TValidSpecialRecipeNum_len  		8
#define TPreviousOpDeliverAmount_len  		5
#define TPreviousUnitsDeliverAmount_len  	5
#define TPreviousDeliverDate_len  		8
#define TSpecialRecipeNum_len			8
#define TUpdateDate_len  			8
#define TUpdateHour_len  			6
#define TLastUpdateDate_len  			8
#define TLastUpdateHour_len  			6
#define TSupplierNum_len  			5
#define TZipCode_len  				5
#define TCommunicationSupplier_len  		5
#define TReportDate_len  			8
#define TReportHour_len  			6
#define TAuthorizarionNum3_len  		9
#define TCardNum_len  				7
#define TSalesMoney_len  			9
#define TRefundsMoney_len  			9
#define TSoftwareVersion_len  			9
#define TPhone1_len  				10
#define TPhone2_len  				10
#define TPhone3_len  				10
#define TOpenShiftMessage_len			60
#define TMemberFamilyName_len			14
#define TMemberFirstName_len  			8
#define TSaleWithoutPrescription_len		1
#define TDoctorFamilyName_len			14
#define TDoctorFirstName_len  			8
#define TGlobalError_len  			60
#define TInteractionDescLine_len  		40
#define TDrugRespondLine_len  			40
#define TDrugDescription_len  			30
#define TUserNum_len  				9
#define TErrorDecription_len  			50
#define TSupplierName_len  			25
#define TAddress_len  				20
#define TCity_len  				20
#define TCity15_len  				15
#define TFaxNum_len  				10
#define TMainDeviceType_len  			2
#define TSemiDeviceType_len  			2
#define TDeviceName_len				40
#define TMemberDiscountPercent_len  		5
#define TPriceSwap_len  			6
#define TAdditionToPrice_len  			7
#define TSupplierTableCod_len			2
#define TSupplierType_len			2
#define TEmployeeID_len				9
#define TEmployeePassword_len			9
#define TCheckType_len				1
#define TMsgExistsFlg_len			1
#define TPrintFlg_len				1
#define TPermissionType_len			2

#define TVatAmount_len				9
#define TBilAmount_len				9
#define TSupplSendNum_len			10
#define TBilAmountWithVat_len			9
#define TBilAmountBeforeVat_len			9
#define TBilConstr_len				1
#define TBilInvDiff_len			9
#define TBasePriceFor1Op_len			9

// DonR 05SEP2002
#define TElectPR_Stock_len  			7
#define TElectPR_DiscntReasonLen		5
#define TElectPR_ReasonToDispLen		2
#define TElectPR_SubstPermLen			2
#define TElectPR_HowPartDetLen			2
#define TElectPR_LineNumLen				2

// DonR 06Feb2003
#define TGroupCode_len		 			3
#define TGroupNbr_len		 			3
#define TItemSeq_len		 			3
#define TEconPriNumUpdated_len			9

#define TRuleNum_len					9
#define TElectPR_Profession_len			5
#define TAuthLevel_len					2


/*   Synonims:
 */
#define Tmaccabi_code_len	TMemberRightsCode_len
#define Tkeren_mac_code_len	TMaccabiKerenCode_len
#define Tmac_magen_code_len	TMaccabiMagenCode_len

#define TSupplierPrice_len  			9  /*Yulia 20021031*/

/*=======================================================================
||			SHORT INTEGER TYPES				||
 =======================================================================*/

/*
 * This paragraph is for disabling
 * SQL commands for non-SQL files
 */
//#ifdef NON_SQL //
//#define EXEC
//#define SQL
//#define begin	long int ____e_l_y______
//#define end	long int ____l_e_v_y____
//#define declare
//#define section
//#endif //

typedef short int TFlg;
typedef short int TPrintFlg;
typedef short int TYarpaParticipationCode;
typedef short int TCalcTypeCode;
typedef short int TSpecialConfirmMessageFlag;
typedef short int TDoseDoseageInUnits;
typedef short int TDoseDoseageInOp;
typedef short int TPharmacistIDExtension;
typedef short int TConfirmationReason;
typedef short int TConfirmationType;
typedef short int Tdiary_month;
typedef short int TSqlInd;
typedef short int TDeleteFlage;
typedef short int TFlage;
typedef short int TAge;
typedef short int TMessageLineID;
typedef short int TMedNumOfMessageLines;
typedef short int TClosingCode;
typedef short int TInstituteCode;
typedef short int TTerminalNum;
typedef short int TMedNumOfDrugLines;
typedef short int TPriceListNum;
typedef short int TPharmacyType;
typedef short int TNet;
typedef short int TCommunicationType;
typedef short int TMemberDiscountPercent;
typedef short int TFreeMemory;
typedef short int TMessage;
typedef short int TGlobalPricelistUpdate;
typedef short int TMaccabiPricelistUpdate;
typedef short int TPurchasePricelistUpdate;
typedef short int TDrugBookUpdate;
typedef short int TDiscountListUpdate;
typedef short int TSuppliersListUpdate;
typedef short int TInstituteListUpdate;
typedef short int TErrorCodeListUpdate;
typedef short int TSaleWithoutPrescription;
typedef short int TReserve1;
typedef short int TReserve2;
typedef short int TOpenMessageLenghtInLines;
typedef short int TErrorCode;
typedef short int TIdentificationCode;
typedef short int TMemberRights;
typedef short int TMemberRightsCode;
typedef short int TNumOfDrugLinesRecs;
typedef short int TMemberParticipatingType;
typedef short int TDrugAnswerCode;
typedef short int TMemberBelongCode;
typedef short int TMoveCard;
typedef short int TTypeDoctorIdentification;
typedef short int TDoctorIdInputType;
typedef short int TRecipeSource;
typedef short int TPaymentParticipatingType;
typedef short int TParticipatingType;
typedef short int TParticipatingPrecent;
typedef short int TParticipatingAmount;
typedef short int TSpecialConfirmationNumSource;
typedef short int TMonthLog;
typedef short int TDuration;
typedef short int TCancleAuthorizationNum;
typedef short int TSeverityLevel;
typedef short int TUpdateCode;
typedef short int TAnotherParticipatingTypeExist;
typedef short int TRegisteredInDrugNote;
typedef short int TMaccabiPriceListNum;
typedef short int TNewMaccabiPriceListNum;
typedef short int TOrderRecordsNum;
typedef short int TUrgencyCode;
typedef short int TPackageUnits;
typedef short int PurchaseOrderConfirmationCode;
typedef short int PurchaseOrderConfirmRecordNum;
typedef short int TActionType;
typedef short int TNumOfDrugLines;
typedef short int TAmountType;
typedef short int TAnotherMessage;
typedef short int TAcceptedLinesNum;
typedef short int TRealAcceptedRecordsNum;
typedef short int TDoseRenewalFrequencyInDays;
typedef short int TDoseNum;
typedef short int TSourceValidSpecialRecipeNum;
typedef short int TMaccabiKerenCode;
typedef short int TMaccabiMagenCode;
typedef short int TSpecialRecipeNumSource;
typedef short int TSpecialRecipeNumCode;
typedef short int TCanDeliverRecipeInPrivPharm;
typedef short int TErrorCodeToUpdateNum;
typedef short int TErrorSevirity;
typedef short int TNumOfLinesToUpdate;
typedef short int TActionCode;
typedef short int TCreditYesNo;
typedef short int TCreditYesNoFlg;
typedef short int TAuthorizarionNum2;
typedef short int TRecordNum;
typedef short int TStationNum;
typedef short int TPaymentType;
typedef short int TSalesActionNum;
typedef short int TRefundActionNum;
typedef short int TSupplierTableCod;
typedef short int TCheckType;
typedef short int TInteractionType;
typedef short int TOpenCloseFlag;
typedef short int TPermissionType;
typedef short int TMacabiPriceFlg;
typedef short int TMsgExistsFlg;
typedef short int TDoctorBelong;
typedef short int TBilConstr;
typedef short int TLineNum;
typedef short int TMemberSex;
typedef short int TGroupCodeShort;
typedef short int TGroupNbrShort;
typedef short int TItemSeq;
typedef short int TStatus;
typedef short int TAuthLevel;
typedef short int	TTreatmentType;

/*=======================================================================
||			 INTEGER TYPES												||
 =======================================================================*/
typedef   int THospitalCode;
typedef   int TStopBloodDate;
typedef   int TDeletedRecordUniqeID;
typedef   int TMessageID;
typedef   int TRefundsMoney;
typedef   int TSalesMoney;
typedef   int TPharmNum;
typedef   int TUserIdentification;
typedef   int THardwareVersion;
typedef   int TAvailableDisk;
typedef   int TFlushingFileSize;
typedef   int TMaxNumOfDrugsToUpdate;
typedef   int TLastBackupDate;
typedef   int TDate;
typedef   int THour;
typedef   int TPriceListUpdateDate;
typedef   int TPriceSwap;
typedef   int TAdditionToPrice;
typedef   int TDocTerminalNum;
typedef   int TPriceListUpdateHour;
typedef   int TLastPriceListUpdateDate;
typedef   int TLastPriceListUpdateHour;
typedef   int TMaccabiPriceListUpdateDate;
typedef   int TPurchasePriceListUpdateDate;
typedef   int TDrugBookUpdateDate;
typedef   int TDiscountListUpdateDate;
typedef   int TSuppliersListUpdateDate;
typedef   int TInstituteListUpdateDate;
typedef   int TErrorCodeListUpdateDate;
typedef   int TRecipeIdentifier;
typedef   int TMemberIdentification;
typedef   int TMemberDateOfBirth;
typedef   int TConfiramtionDate;
typedef   int TConfirmationHour;
typedef   int TDrugCode;
typedef   int TUnits;
typedef   int TOp;
typedef   int TDrugCodeInInteraction;
typedef   int TOpDrugPrice;
typedef   int TDoctorIdentification;
typedef   int TInvoiceNum;
typedef   int TDoctorRecipeNum;
typedef   int TPharmRecipeNum;
typedef   int TDeviceCode;
typedef   int TDeviceNum;
typedef   int TDeliverConfirmationDate;
typedef   int TDeliverConfirmationHour;
typedef   int TNumofSpecialConfirmation;
typedef   int TCurrentStockOp;
typedef   int TCurrentStockInUnits;
typedef   int TTotalMemberParticipation;
typedef   int TTotalDrugPrice;
typedef   int TLinkDrugToAddition;
typedef   int TRecipeNum;
typedef   int TSpecialRecipeStartDate;
typedef   int TCancleRecipeNum;
typedef   int TCancleInvoiceNum;
typedef   int TSpecialRecipeStopDate;
typedef   int TLastDrugUpdateDate;
typedef   int TLastDrugUpdateHour;
typedef   int TDrugFileUpdateDate;
typedef   int TDrugFileUpdateHour;
typedef   int TNumOfDrugsToUpdate;
typedef   int TPackageSize;
typedef   int TSupplierCode;
typedef   int TComputersoftDrugCode;
typedef   int TDrugAddition1Code;
typedef   int TDrugAddition3Code;
typedef   int TDrugAddition2Code;
typedef   int TDrugPriceForUpdateNum;
typedef   int TNewMaccabiPrice;
typedef   int TPurchasePrice;
typedef   int TYarppaDrugCode;
typedef   int TCurrentStockLeftOver;
typedef   int TOrderAmount;
typedef   int TManufactorCode;
typedef   int TInternalOrderNum;
typedef   int TAuthorizationNum;
typedef   int TSuppDevPharmNum;
typedef   int TDiscountAmount;
typedef   int TDiscountPrecent;
typedef   int TBilAmount;
typedef   int TVatAmount;
typedef   int TOpAmout;
typedef   int TUnitsAmout;
typedef   int TUnitPriceFor1Op;
typedef   int TBasePriceFor1Op;
typedef   int TTotalForItemLine;
typedef   int TFullOpStock;
typedef   int TCurrentUnitStock;
typedef   int TMinOpStock;
typedef   int TEnterDate;
typedef   int TEnterHour;
typedef   int TFromDate;
typedef   int TToDate;
typedef   int TTotalFinanceAccumulate;
typedef   int TMemberParticipatingAccumulate;
typedef   int TNumOfSoldRecipe;
typedef   int TNumOfSoldDrugLines;
typedef   int TKabalaNum;
typedef   int TCancleFinanceAccumulate;
typedef   int TNumOfCancledRecipeLines;
typedef   int TDeliverFailedCommFinanceAccum;
typedef   int TDeliverFailedCommLinesNum;
typedef   int TMessageDate;
typedef   int TMessageHour;
typedef   int TNumOfMessageLines;
typedef   int TSupervisorPharmecistIdent;
typedef   int TSupervisorPharmecistCardValid;
typedef   int TTreatmentStartDate;
typedef   int TTreatmentStopDate;
typedef   int TValidSpecialRecipeNum;
typedef   int TPreviousOpDeliverAmount;
typedef   int TPreviousUnitsDeliverAmount;
typedef   int TPreviousDeliverDate;
typedef   int TSpecialRecipeNum;
typedef   int TUpdateDate;
typedef   int TUpdateHour;
typedef   int TLastUpdateDate;
typedef   int TLastUpdateHour;
typedef   int TSupplierNum;
typedef   int TPriceForOP;
typedef   int TZipCode;
typedef   int TCommunicationSupplier;
typedef   int TReportDate;
typedef   int TReportHour;
typedef   int TAuthorizarionNum3;
typedef   int TCardNum;
typedef   int TEmployeeID;
typedef   int TEmployeePassword;
typedef   int TBilAmountWithVat;
typedef   int TBilAmountBeforeVat;
typedef   int TBilInvDiff;
typedef   int Tdiary_monthNew;
typedef   int Tfee;
typedef   int TRuleNum;
typedef   int TProfessionx;

/*=======================================================================
||				STRING TYPES				||
 =======================================================================*/
typedef char TAllowedProcList[50 + 1];
typedef char TAllowedBelongings[50 + 1];
typedef char TParticipationName[25 + 1];
typedef char TPhone[10 + 1];
typedef char TPhone1[10 + 1];
typedef char TPhone2[10 + 1];
typedef char TPhone3[10 + 1];
typedef char TMessageLine[60 + 1];
typedef char TSoftwareVersion[9 + 1];
typedef char TOpenShiftMessage[60 + 1];
typedef char TMemberFamilyName[14 + 1];
typedef char TMemberFirstName[8 + 1];
typedef char TDoctorFamilyName[14 + 1];
typedef char TDoctorFirstName[8 + 1];
typedef char TGlobalError[60 + 1];
typedef char TInteractionDescLine[40 + 1];
typedef char TDrugRespondLine[40 + 1];
typedef char TDrugDescription[30 + 1];
typedef char TUserNum[9 + 1];
typedef char TErrorDecription[50 + 1];
typedef char TSupplierName[25 + 1];
typedef char TAddress[20 + 1];
typedef char TCity[20 + 1];
typedef char TCity15[15 + 1];
typedef char TFaxNum[10 + 1];
typedef char TMainDeviceType[2 + 1];
typedef char TSemiDeviceType[2 + 1];
typedef char TDeviceName[40 + 1];
typedef char TSupplierType[2 + 1];
typedef char TBilNum[10+1];
typedef char TSupplSendNum[10+1];

/*=======================================================================
||				GENERIC TYPEDEFS (DonR 05Feb2003						||
 =======================================================================*/

// DonR 05 Feb 2003 - Add some generic type fields - since many messages have
// standard data elements.
typedef short	TGenericFlag;
typedef int		TGenericYYYYMMDD;
typedef short	TGenericYYMM;
typedef int		TGenericHHMMSS;
typedef int		TGenericTZ;
typedef char	TGenericCharFlag	[ 1 + 1];
typedef char	TGenericText75		[75 + 1];


/*=======================================================================
||			      Special buffers				||
 =======================================================================*/

typedef struct raw_data_header
{
	int		actual_rows;
	int		alloc_rows;
} RAW_DATA_HEADER;


// DonR 15Feb2006: Add generic-ingredient data to drugs-in-blood data.
typedef struct
{
	short		code;
	double		quantity;
	double		quantity_std;
	double		per_quant;
	char		units		[ 3 + 1];
	char		per_units	[ 3 + 1];
}	TIngredient;

typedef struct				/* -----< DRUGS IN BLOOD >----- */
{
	TDrugCode					Code;
	TTreatmentStopDate			StopDate;
	TTreatmentStopDate			StopBloodDate;
	TTreatmentStartDate			StartDate;
	int							PurchaseTime;
	TDoctorIdentification		DoctorId;
	TTypeDoctorIdentification	DoctorIdType;
	TUnits						Units;
	TOp							Op;
	TIngredient					Ingredient[3];
	double						AdjLimIngrUsage;	// DonR 16Sep2013. For now, not loaded automatically by get_drugs_in_blood().
	TDuration					Duration;
	short int					THalf;
	int							PrescriptionID;
	short int					LineNo;
	TRecipeSource				Source;
	int							TotalUnits;
	short						PurchaseLimitFlag;
	int							PurchaseLimitGroupCode;
	short						BoughtAtDiscount;
	short						ClassCode;	// DonR 26May2010: "Stickim".
	int							SpecialPrescNum;
	short						PharmacologyGroup;
	int							EconomypriGroup;
	char						LargoType [1 + 1];
	short						TreatmentType;
	short						chronic_flag;
	short						ShapeCode;
	short						in_drug_interact;
	int							parent_group_code;
	int							PharmacyCode;
} DURDrugsInBlood;

typedef struct				/* -< DRUGS IN BLOOD BUFFER  >- */
{
	DURDrugsInBlood	*Durgs;
	int				max;
	int				size;
} TDrugsBuf;


typedef struct pharmacy_info	// PHARMACY INFO FROM DATABASE (more complete info than old version).
{
	int		pharmacy_code;				// DonR 18Jan2022 just so we have a place we know to look for this!
	short	institute_code;				// DonR 09May2021 "Chanut Virtualit".
	short	pharmacy_permission;
	short	price_list_num;
	short	found_and_legal;
	short	open_close_flg;
	short	pharm_category;
	short	software_house;				// DonR 26Nov2025 User Story #251264.
	char	hebrew_encoding;			// DonR 26Nov2025 User Story #251264: read from transaction_hebrew_encoding table.
	short	pharm_card;
	short	leumi_permission;
	short	CreditPharm;
	short	maccabicare_pharm;
	short	maccabi_pharmacy;			// DonR 20Apr2023 User Story #432608 - includes only "genuine" Maccabi pharmacies with Owner == 0.
//	short	GenuineMaccabiPharmacy;		// DonR 19Apr2023 User Story #432608 - excludes SuperPharm "consignatzia" pharmacies.
	short	private_pharmacy;			// DonR 20Apr2023 User Story #432608 - includes all pharmacies with Owner <> 0.
	short	prati_plus_pharmacy;
	short	MaccabiPtnPharmacy;			// DonR 20Apr2023 User Story #432608 - includes all pharmacies with permission_type == 1.
	short	ConsignmentPharmacy;		// DonR 19Apr2023 User Story #432608
	short	PharmNohalEnabled;			// DonR 19Apr2023 User Story #432608
	short	hesder_category;
	short	has_hesder_maccabi;
	short	vat_exempt;
	short	mahoz;						// Marianna 22May2024 User Story #314887
	short	meishar_enabled;
	short	order_originator;
	short	order_fulfiller;
	short	can_sell_future_rx;
	int		web_pharmacy_code;			// DonR 09Sep2020 Online Orders.
	short	owner;						// DonR 09Sep2020 Online Orders.
	short	max_order_sit_days;			// DonR 09Sep2020 Online Orders.
	short	max_order_work_hrs;			// DonR 09Sep2020 Online Orders.
	int		maxdailybuy;				// DonR 09Sep2020 Online Orders.
	int		kill_nocard_svc_minutes;	// DonR 09Sep2020 Online Orders.
	short	pharm_type_meishar_enabled;	// DonR 09Sep2020 Online Orders.
//	short	extend_valid_until_date;	// DonR 10Jan2022 Chanut Virtualit.
	short	select_late_rx_days_buying_in_person;		// DonR 13Jan2022 moving global params to pharmacy_type_params for greater flexibility.
	short	select_late_rx_days_ordering_online;		// DonR 13Jan2022 moving global params to pharmacy_type_params for greater flexibility.
	short	select_late_rx_days_filling_online_order;	// DonR 13Jan2022 moving global params to pharmacy_type_params for greater flexibility.
	short	allow_online_order_days_before_expiry;		// DonR 13Jan2022 moving global params to pharmacy_type_params for greater flexibility.
	short	usage_memory_mons;							// DonR 13Jan2022 moving global params to pharmacy_type_params for greater flexibility.
	short	allow_order_narcotic;
	short	allow_order_preparation;
	short	allow_full_price_order;
	short	allow_refrigerated_delivery;
	short	can_sell_cannabis;
//	short	min_days_before_rx_until_date;
	int		max_price_for_pickup;
	short	prior_sale_download_type;	// DonR 19Jun2025 User Story #417785 (new transaction 6011).
	short	prior_sale_opioid_days;		// DonR 06Jan2026 User Story #417785 (new transaction 6011).
	short	prior_sale_ADHD_days;		// DonR 06Jan2026 User Story #417785 (new transaction 6011).
	short	prior_sale_ordinary_days;	// DonR 06Jan2026 User Story #417785 (new transaction 6011).
	bool	enable_ishur_download;		// DonR 19Jun2025 User Story #417785 (new transaction 6011).
	short	default_narco_max_duration;	// DonR 28Oct2025 User Story #429086.
	short	default_narco_max_validity;	// DonR 28Oct2025 User Story #429086.
	short	exempt_narco_max_duration;	// DonR 28Oct2025 User Story #429086.
	short	exempt_narco_max_validity;	// DonR 28Oct2025 User Story #429086.
} PHARMACY_INFO;


typedef struct pharmacy_row		/* -<   PHARMACY KEY         >- */
{
	int				pharmacy_num;
	short			institute_code;
	PHARMACY_INFO	info;
} PHARMACY_ROW;


typedef struct severity_row		/* -< SEVERITY ROW IN BUFFER >- */
{
	short	error_code;
	short	severity_level;
} SEVERITY_ROW;


typedef struct percent_row		/* -< PERCENT ROW IN BUFFER >- */
{
	TParticipatingType		percent_code;
	TParticipatingPrecent	percent;
} PERCENT_ROW;

typedef struct
{
	char	participation_name	[25 + 1];
	short	calc_type_code;
	short	member_institued;
	short	yarpa_part_code;
	int		tax;
	short	member_price_code;
	int		update_date;
	int		update_time;
	short	del_flg;
	short	member_price_prcnt;
	int		max_pkg_price;
	int		min_reduced_price;	// Copy of "tax", for clarity.
}	Tmember_price_row;


// DonR 22Nov2011: Revamped structure to reflect current naming conventions (Basic/Kesef/Zahav)
// and permit aggregate SELECTS by separating out the health-basket flags for the three
// insurance categories.
typedef struct				/* -< DRUGS PARTICIPATING TST >-*/
{
	TMemberParticipatingType	basic_part;
	TMemberParticipatingType	kesef_part;
	TMemberParticipatingType	zahav_part;
	int							basic_fixed_price;
	int							kesef_fixed_price;
	int							zahav_fixed_price;
	short						in_health_basket;	// DonR 18Dec2007.
} TParticTst;

typedef struct				/* -< DRUGS DISCOUNT SOURCE >-*/
{
	short int			table;
	short int 			insurance_used;
	short int	    	state;
} TRetPartSource;

typedef struct tag_DUR_Data		/* -<DRUG-INTERACTION-LINE>- */
{
	TRecipeIdentifier	interact_presc_id;
	short int			line_no;
	short int			interact_line_no;
	TDrugCode			largo_code;
	TDrugCode			interact_largo_cod;
	TTreatmentStopDate	StopDate;
	TTreatmentStopDate	interact_StopDate;
	TStopBloodDate		StopBloodDate;
	TStopBloodDate		interact_StopBloodDate;
} DUR_Data;


typedef struct doctor_drug_row		/* -<DOCTOR-DRUG KEY IN BUF>- */
{
	TDoctorIdentification	doctor_id;
	TDrugCode				drug_code;
	TParticTst				info;
	TDoctorBelong			doctor_belong;
} DOCTOR_DRUG_ROW;

typedef struct doctor_drug_row_arg		/* -<DRUG argument for search>- */
{
	TDoctorIdentification	doctor_id;
	TDrugCode				drug_code;
	TParticTst				info;
	TDoctorBelong			doctor_belong;
	DOCTOR_DRUG_ROW			*last_found_drug;
} DOCTOR_DRUG_ROW_ARG;


typedef struct interaction_row		/* -<INTERACTION KEY IN BUF>- */
{
	TDrugCode			first_drug_code;
	TDrugCode			second_drug_code;
	TInteractionType	interaction_type;
} INTERACTION_ROW;


typedef struct prescription_row		/* -<PRESCRIPTION KEY IN BUF>-*/
{
	TRecipeIdentifier	prescription_id;
} PRESCRIPTION_ROW;


typedef struct message_recid_row		/* -<MESSAGE ID IN BUF>-*/
{
	TRecipeIdentifier	message_rec_id;
} MESSAGE_RECID_ROW;


typedef struct delpr_recid_row		/* -<DELETED PR ID IN BUF>-*/
{
	TRecipeIdentifier	delpr_rec_id;
} DELPR_RECID_ROW;


typedef struct prescr_written_row		/* -<PRESCRIPTION-WRITTEN KEY IN BUF>-*/
{
	TRecipeIdentifier	this_id;
	TRecipeIdentifier	next_id;
	TRecipeIdentifier	last_reserved_id;
	short				error_code;
} PRESCR_WRITTEN_ROW;


// DonR 09Jun2010: "Generic" structure to hold drug_list data.
typedef struct
{
	int		largo_code;
	char	long_name			[30 + 1];
	char	short_name			[17 + 1];
	char	name_25				[25 + 1];
	short	preferred_flg;
	int		preferred_largo;
	short	multi_pref_largo;
	short	del_flg;
	char	del_flg_cont;
	short	is_out_of_stock;
	int		parent_group_code;
	char	parent_group_name	[25 + 1];
	int		economypri_group;
	short	specialist_drug;
	short	has_shaban_rules;
	short	in_doctor_percents;
	short	class_code;
	short	drug_book_flg;
	short	maccabicare_flag;
	short	assuta_drug_status;
	short	t_half;
	short	chronic_flag;
	short	chronic_months;
	int		chronic_days;
	short	allow_future_sales;
	short	qualify_future_sales;
	short	PreventFutureCannabisSales;
	short	additional_price;
	char	package				[10 + 1];
	int		package_size;
	double	package_volume;
	int		effective_package_size;
	int		monthly_package_size;	// DonR 07Nov2023 User Story #473527.
	short	openable_pkg;
	short	calc_op_by_volume;
	short	expiry_date_flag;
	short	shape_code;
	short	shape_code_new;
	char	form_name			[25 + 1];
	char	unit_abbreviation	[ 3 + 1];
	short	must_prescribe_qty;
	short	split_pill_flag;
	int		supplemental_1;
	int		supplemental_2;
	int		supplemental_3;
	int		supplier_code;
	int		computersoft_code;
	char	bar_code_value		[14 + 1];
	short	no_presc_sale_flag;
	short	home_delivery_ok;
	char	largo_type;
	short	in_gadget_table;
	short	treatment_typ_cod;
	short	in_health_pack;
	short	health_basket_new;
	short	health_basket_new_unprocessed;
	short	pharm_group_code;
	short	member_price_code;
	int		price_prcnt;
	int		price_prcnt_magen;
	int		price_prcnt_keren;
	short	enabled_status;
	short	enabled_mac;
	short	enabled_hova;
	short	enabled_keva;
	char	drug_type;
	short	is_narcotic;				// DonR 22Jul2025 User Story #427521
	short	psychotherapy_drug;			// DonR 22Jul2025 User Story #427521
	short	preparation_type;			// DonR 22Jul2025 User Story #417800
	short	priv_pharm_sale_ok;
	short	tikrat_piryon_pharm_type;	// DonR 11Feb2025 User Story #376480
	short	pharm_sale_new;
	char	pharm_sale_test;
	short	status_send;
	char	sale_price			[ 5 + 1];
	char	sale_price_strudel	[ 5 + 1];
	char	interact_flg;
	short	in_drug_interact;
	short	in_overdose_table;
	short	magen_wait_months;
	short	keren_wait_months;
	short	drug_book_doct_flg;
	short	ishur_required;
	TIngredient		Ingredient[3];
	short	purchase_limit_flg;
	short	purchase_lim_without_discount;
	int		qty_lim_grp_code;
	short	ishur_qty_lim_flg;
	short	tight_ishur_limits;
	short	ishur_qty_lim_ingr;
	short	rule_status;
	short	sensitivity_flag;
	int		sensitivity_code;
	short	sensitivity_severe;
	short	needs_29_g;
	short	print_generic_flg;
	short	copies_to_print;
	int		substitute_drug;
	int		fps_group_code;
	short	tikrat_mazon_flag;
	short	cont_treatment;
	short	how_to_take_code;
	short	time_of_day_taken;
	short	treatment_days;
	int		illness_bitmap;
	short	has_diagnoses;
	short	IllnessDiscounts;		// DonR 13May2020 CR #31591
	short	VentilatorDiscounts;	// DonR 13May2020 CR #31591
	short	NonIllnessDiscounts;	// DonR 13May2020 CR #31591
	short	ConsignmentItem;		// DonR 16Apr2023 User Story #432608
	int		refresh_date;
	int		refresh_time;
	int		changed_date;
	int		changed_time;
	int		pharm_update_date;
	int		pharm_update_time;
	int		doc_update_date;
	int		doc_update_time;
	int		as400_batch_date;
	int		as400_batch_time;
	int		update_date;
	int		update_time;
	int		update_date_d;
	int		update_time_d;
	char	pharm_diff			[50 + 1];
	short	maccabi_profit_rating;				// DonR 27Apr2021 User Story #149963
	short	is_orthopedic_device;				// DonR 03May2021 Chanut Virtualit, User Story #146798
	short	needs_preparation;					// DonR 03May2021 Chanut Virtualit, User Story #146798
	short	needs_patient_measurement;			// DonR 03May2021 Chanut Virtualit, User Story #146798
	short	needs_patient_instruction;			// DonR 03May2021 Chanut Virtualit, User Story #146798
	short	needs_refrigeration;				// DonR 03May2021 Chanut Virtualit, User Story #146798
	short	online_order_pickup_ok;				// DonR 03May2021 Chanut Virtualit, User Story #146798
	short	delivery_ok_per_shape_code;			// DonR 23Aug2021 Chanut Virtualit, User Story #146798
	short	needs_dilution;						// DonR 23Aug2021 Chanut Virtualit, User Story #146798
	short	has_member_type_exclusion;			// DonR 23Jul2023 User Story 448931/458942
	short	bypass_member_pharm_restriction;	// DonR 23Jul2023 User Story 448931/458942
	short	compute_duration;					// DonR 31Jul2023 User Story #469361
}	TDrugListRow;


typedef struct	// Doctor_presc table structure - DonR 28Oct2014.
{
	int		member_id;
	short	member_id_code;
	int		clicks_patient_id;
	int		doctor_id;
	int		doctor_presc_id;
	int		largo_prescribed;
	int		valid_from_date;
	int		valid_until_date;
	long	order_number;
	short	sold_status;
	short	ordered_status;
	short	deleted_status;
	long	visit_number;
	int		visit_date;
	int		visit_time;
	char	clinic_address		[255 + 1];
	short	line_number;
	char	speciality_desc		[ 25 + 1];
	short	specialist_ishur;
	int		rule_number;
	short	prescription_type;
	short	digital_presc_flag;
	short	dose_number;
	short	op;
	short	total_units;
	short	qty_method;
	short	original_op;			// DonR 15Feb2024 User Story #473527 fix.
	short	original_total_units;	// DonR 18Feb2024 User Story #473527 fix.
	short	original_qty_method;	// DonR 18Feb2024 User Story #473527 fix.
	short	adjusted_monthly_units;	// DonR 07Nov2023 User Story #473527.
	short	usage_method_code;
	char	usage_instructions	[100 + 1];
	double	amt_per_dose;
	char	unit_abbreviation	[  3 + 1];
	short	doses_per_day;
	short	treatment_length;
	short	course_treat_days;
	short	course_len;
	char	course_len_units	[  6 + 1];
	short	course_len_days;		// DonR 22Oct2023 User Story #487170.
	short	num_courses;
	char	days_per_week		[ 20 + 1];
	char	morning_evening		[200 + 1];
	char	treatment_side		[ 10 + 1];
	char	member_phone		[ 10 + 1];
	char	clinic_phone		[ 10 + 1];
	int		row_added_date;
	int		row_added_time;
	int		last_sold_date;
	int		last_sold_time;
	short	tot_units_sold;
	short	tot_op_sold;
	short	linux_update_flag;
	int		linux_update_date;
	int		linux_update_time;
	short	extern_update_flag;
	int		extern_update_date;
	int		extern_update_time;
	short	reported_to_as400;
	short	reported_to_cds;
	short	effective_unit_len;

	// DonR 07Nov2024 User Story #357209: New columns for prescriptions from external sources.
	short	rx_origin;	// 0 = Maccabi Doctor, 1 = MaccabiDent, 2 = hospital, 3 = other.
	char	external_doc_first_name	[  8 + 1];
	char	external_doc_last_name	[ 14 + 1];
	int		external_doc_license_num;
	short	external_doc_license_type;	// 1 = Maccabi Doctor, ...
	char	internal_comments		[500 + 1];
	short	ProfCodeToPharmacy;						// DonR 08Dec2025 User Story #441076.
	char	ProfDescription			[ 25 + 1];		// DonR 08Dec2025 User Story #441076.
	char	PharmacistAuthorization	[ 20 + 1];		// DonR 08Dec2025 User Story #441076.
}	TDoctorPrescRow;


typedef struct	// Previous-purchase data to support Trn. 6001/6101 "reason" codes - DonR 25May2022.
{
	// From database.
	int		PrevLargoBought;
	int 	PrevDuration;
	int 	PrevUnitsPerDose;		// As stored in prescription_drugs - e.g. 1.5 pills = 150.
	double	PrevAmtPerDose;			// Converted to the format used in doctor_presc - 1.5 pills = 1.5.
	int 	PrevDosesPerDay;
	short	PrevUseInstrTemplate;
	short	PrevShapeCodeNew;
	short 	PrevCourseTreatDays;
	short 	PrevCourseLength;
	short	PrevCourseUnitLength;	// DonR 22Oct2023 User Story #487170.
	short 	PrevCourseLengthDays;	// DonR 22Oct2023 User Story #487170.
	short 	PrevHowToTakeCode;
	char 	PrevUnitCode	[  3 + 1];
	char 	PrevCourseUnits	[ 10 + 1];
	char 	PrevDaysOfWeek	[ 20 + 1];
	char 	PrevTimesOfDay	[200 + 1];	// DonR 29Oct2023 BUG FIX: We read 200 characters from the database, so we need 200 characters here.
	char 	PrevSideOfBody	[ 10 + 1];

	// Derived booleans.
	bool	FoundPreviousPurchase;
	bool	PrevPurchaseWithSameLargo;
	bool	PrevPurchaseWithSameShape;
//	bool	PrescriptionTypeMismatch;	APPLIES TO SPECIFIC DOCTOR PRESCRIPTION
//	bool 	UsageInstructionChanged;	APPLIES TO SPECIFIC DOCTOR PRESCRIPTION
}	TPrevPurchaseData;


typedef struct	// Trn. 6001 prescription-data structure - DonR 28Oct2014.
{
	short				IsExtraLargo;
	bool				NeedToFindBestPreferredPackageSize;		// DonR 12Sep2023 User Story #473527
	bool				BestPreferredPackageSizeChosen;			// DonR 12Sep2023 User Story #473527
	bool				EvenPackagesToFill;						// DonR 07Nov2023 User Story #473527.
	bool				SameParentGroupAsRx;					// DonR 07Nov2023 User Story #473527.
	short				NumWholePackages;						// DonR 13Nov2023 User Story #473527.
	TDoctorPrescRow		Rx;
	TDrugListRow		DL;
	TDrugListRow		DL_prescribed;
	TPrevPurchaseData	PPD;
	TDoctorFirstName	DoctorFirstName;
	TDoctorFamilyName	DoctorFamilyName;
	char				DoctorPhone			[10 + 1];
	char				DocContactPhone		[40 + 1];
	char				ShapeName			[25 + 1];
	int					DoctorLicense;
	short				PriceCode;
	short				PricePercent;
	short				FixedPriceFlag;
	short				InsuranceUsed;
	short				InHealthBasket;
	int					ReducedPrice;
	int					AdditionToPrice;	// DonR 09Jan2022 - enable member discounts.
	int					DiscountApplied;	// DonR 09Jan2022 - enable member discounts.
	int					TotalMemberPtn;
	int					MemberPtnPerPackage;
	int					YarpaPrice;
	int					MaccabiPrice;
	bool				FoundAs400Ishur;
	short				SpecPrescNumSource;
	short				IshurTikraType;
	short				PriceMessageCode;
	short				PtnRequiresPratiPlus;
	short				PrDateValidForSale;
	short				SuppressOutputToPharmacy;
	short				OPToSell;
	short				UnitsToSell;
	short				TotalUnitsToSell;
	short				TotalUnitsPrescribed;	// DonR 18Jan2022 for Chanut Virtualit Trn. 6101.
	short				UnitsAlreadySold;
	int					dose_times_100;
	short				NumRxInVisit;
	short				DrugAnswerCode;
	short				DFatalErr;
	short				OtherValidRxExists;
	short				OnlineSalePermitted;
	short				PickupPermitted;
	short				SuperPharmDeliveryPermitted;	// DonR 17Aug2021 "Chanut Virtualit": This is the "old logic" flag for Trn. 6001 output.
	short				DeliveryPermitted;				// DonR 17Aug2021 "Chanut Virtualit": This is the "new logic" flag for Trn. 6101 output.
	short				DeliverWithoutConsultation;		// DonR 20Jan2022 "Chanut Virtualit": Pulled from drug_shape/home_delivery_ok.
	short				ReasonForbiddenCode;
	short				DisplayReasonForbidden;
	char				ReasonForbiddenDesc		[100 + 1];
	short				ConversationRequired;
	short				ConversationReasonCode;
	short				DisplayConversationReason;
	char				ConversationReasonDesc	[100 + 1];
	short				FirstOccurrenceOfLargoSubscript;	// DonR 28Oct2021 User Story #202451 - send each Largo Code only once to Nihul Tikrot.
	int					AggregatePtnForThisLargo;			// DonR 28Oct2021 User Story #202451 - send each Largo Code only once to Nihul Tikrot.
	short				LargoBlockedForMember;				// 21Nov2021 Epic #183134 / User Story #196891
	short				TikratMazonFlagSend;				// DonR 12Feb2025 User Story #376480.
	short				IsCannabis;							// DonR 08Dec2025 User Story #441076.
	short				IsPreparation;						// DonR 08Dec2025 User Story #441076.
	short				NumMessageLines;					// DonR 08Dec2025 User Story #441076.
	short				ProfCodeToPharmacy;					// DonR 08Dec2025 User Story #441076.
	char				ProfDescription			[ 25 + 1];	// DonR 08Dec2025 User Story #441076.
}	T6001_Drugs;


typedef struct	// Prior sale data for Transaction 6001 prescribed drugs.
{
	int					EconomypriGroup;
	int					LargoCode;
	int					LastSaleDate;
	int					LastSaleTime;
	short				OP;
	short				Units;
}	T6001_LastRelevantSale;


typedef struct		// Drug sale "lines"
{
	TDrugListRow					DL;		// DonR 09Jun2010: Will eventually replace many fields below.
	TDrugCode						DrugCode;
	TOp								Op;
	TUnits							Units;
	TOp								Doctor_Op;
	TUnits							Doctor_Units;
	TDuration						Duration;
	TOpDrugPrice					OpDrugPrice;
	TLinkDrugToAddition				LinkDrugToAddition;
	TOpDrugPrice					RetOpDrugPrice;
	TOpDrugPrice					YarpaDrugPrice;
	TOpDrugPrice					AlternateYarpaPrice;	// DonR 29Jun2023 User Story #461368.
	TOpDrugPrice					PriceForPtnCalc;		// DonR 19Feb2012: For Eilat pharmacies, this will be price minus VAT.
	TOpDrugPrice					MacabiDrugPrice;
	TMacabiPriceFlg					MacabiPriceFlg;
	TMemberParticipatingType		RetPartCode;
	TMemberParticipatingType		BasePartCode;
	TDrugAnswerCode					DrugAnswerCode;
	TInteractionDescLine			InteractionDescLine;
	TDrugCodeInInteraction			DrugInInteraction;
	TDrugCodeInInteraction			RetDrugInteraction;
	TPriceSwap						PriceSwap;
	TAdditionToPrice				AdditionToPrice;
	int								DiscountApplied;
	TInteractionType				interaction_type;
	int								StopUseDate;
	int								StopBloodDate;
	short							DFatalErr;
	short							ATestOverDosed;
	short							PrticSet;
	TUnits							over_dose_limit;
	short							no_magen_flg;
	short							no_keren_flg;
	TRetPartSource					ret_part_source;
	TParticipatingPrecent			part_for_discount;
	short							BasePartCodeFrom;
	short							in_health_pack;
	TNumofSpecialConfirmation		SpecPrescNum;			//20020430
	TSpecialConfirmationNumSource	SpecPrescNumSource;
	TOpDrugPrice					SupplierDrugPrice;		//20021031 Yulia
	TMemberParticipatingType		mac_magen_code;			// DonR 25Nov2003
	TMemberParticipatingType		keren_mac_code;			// DonR 25Nov2003
	short							InsPlusPtnSource;		// DonR 21Sep2004
	double							TotIngrQuantity[3];		// DonR 11Jul2010
	double							IngrQuantityStd[3];		// DonR 20Jun2013
	short							HasIshurWithLimit;
	short							HasPrescFixedPrice;		// DonR 13Jun2006
	char							PurchaseLimitFrom;			// DonR 29Jun2006
	int								PurchaseLimitUnits;			// DonR 29Jun2006
	double							PurchaseLimitIngredLim;		// DonR 30Nov2016
	short							PurchaseLimitMonths;		// DonR 29Jun2006
	int								PurchaseLimitOpenDt;		// DonR 20Jul2006
	int								PurchaseLimitStartHist;		// DonR 07Nov2010
	short							PurchaseLimitMethod;		// DonR 30Nov2016
	short							PurchaseLimitWarningOnly;	// DonR 30Nov2016
	short							PurchaseLimitIngrCode [LIMIT_GROUP_MAX_INGRED];		// DonR 01Mar2017 - made into an array.
	short							PurchaseLimitIngrCount;								// DonR 01Mar2017 - support for multiple ingredients in group.
	short							PurchaseLimitExemptDisease;		// DonR 30Nov2016
	short							PurchaseLimitSourceReject;		// DonR 13Dec2016
	short							PurchaseLimitMonthIs28Days;		// DonR 06Dec2021
	short							PurchaseLimitCustomErrorCode;	// DonR 08Feb2022
	bool							PurchaseLimitIncludeFullPrice;	// DonR 22Jul2025 User Story #427783
	short							BoughtAtDiscount;				// DonR 02Jul2006
	short							IshurWithTikra;					// DonR 26Apr2007
	short							IshurTikraType;					// DonR 08Jun2010
	int								IshurStartDate;					// DonR 11Dec2017 CR #12458
	short							WhySpecialistPtnGiven;			// DonR 14Jan2009
	short							WhyNonPreferredSold;			// DonR 14Jan2009
	int								rule_number;					// DonR 10Jun2010
	short							rule_needs_29g;					// DonR 01Sep2015
	int								DoctorRuleNumber;				// DonR 23Apr2025 User Story #390071.
	short							why_future_sale_ok;				// DonR 25Jun2013
	short							qty_limit_chk_type;				// DonR 25Jun2013
	short							qty_limit_class_code;			// DonR 03Nov2022.
	int								qty_lim_ishur_num;				// DonR 25Jun2013
	int								vacation_ishur_num;				// DonR 25Jun2013
	short							actual_usage_checked;			// DonR 10Sep2013
	short							course_len;						// DonR 03Feb2015 - could be in days, weeks, or months.
	char							course_len_units[  6 + 1];		// DonR 03Feb2015
	short							course_len_days;				// DonR 10Sep2013, renamed 03Feb2015.
	short							course_treat_days;				// DonR 10Sep2013
	short							calc_courses;					// DonR 10Sep2013
	short							calc_treat_days;				// DonR 10Sep2013
	int								total_units;					// DonR 10Sep2013
	int								fully_used_units;				// DonR 10Sep2013
	int								partly_used_units;				// DonR 10Sep2013
	int								discarded_units;				// DonR 10Sep2013
	double							avg_partial_unit;				// DonR 10Sep2013
	double							proportion_used;				// DonR 10Sep2013
	double							net_lim_ingr_used;				// DonR 10Sep2013
	short							DocIDType;						// 06Jan2015 Digital Prescriptions
	int								DocID;							// 06Jan2015 Digital Prescriptions
	int								DocID_FromLookup;				// 11Dec2018 CR #15277.
	int								DocFacility;					// 06Jan2015 Digital Prescriptions
	short							DocChkInteractions;				// DonR 15May2017
	short							PrescSource;					// 06Jan2015 Digital Prescriptions
	long							VisitNumber;					// 06Jan2015 Digital Prescriptions
	int								VisitDate;						// 27Mar2017
	int								FirstDocPrID;					// 27Mar2017
	short							IsDigital;						// 06Jan2015 Digital Prescriptions
	short							FirstRx;						// 06Jan2015 Digital Prescriptions
	short							NumDocRxes;						// 06Jan2015 Digital Prescriptions
	int								MaxPrDate;						// 07Jan2015 Digital Prescriptions
	short							BarcodeScanned;					// 20Aug2018 CR #15804
	int								ph_OTC_unit_price;				// 20Aug2018 Meishar for OTC
	int								CannabisParticipation;			// 08Apr2024 User Story #540234 (Cannabis)
	int								member_diagnosis;				// 02Oct2018 CR #13262.
	short							LargoBlockedForMember;			// 18Nov2021 Epic #183134 / User Story #196891
	short							IsConsignment;					// DonR 27Mar2023 User Story #432608.
	short							ValidConsignmentSale;			// DonR 30Jul2023 User Story #468171.
	short							TikratMazonFlagSend;			// DonR 11Feb2025 User Story #376480.
	bool							WasPossibleDrugShortage;		// DonR 13May2025 User Story #388766;
 } TPresDrugs;


typedef struct		// Doctor-prescription "trailer" data for Digital Prescription transactions.
{
	int		LargoPrescribed;	// DonR: added 22Sep2019 to simplify Health Alerts code.
	int		DocID;				// DonR: added 20Nov2016 to simplify previous-partial-sale lookup logic.
	int		clicks_patient_id;
	long	VisitNumber;
	short	line_number;
	int		PrID;
	int		FromDate;
	int		Units;
	int		OP;
	int		UnitsUnsold;
	int		OP_Unsold;
	short	prev_sold_status;
} TDocRx;


typedef struct		// Unfilled/partially-filled doctor-prescription "trailer" data for Digital Prescription transactions.
{
	long	VisitNumber;
	int		PrID;
	int		FromDate;
	int		Doctor_Largo;	
	short	Filled_Status;
	short	Reason_Code;
} TUnfilledRx;


// DonR 02Jun2021 User Story #163882.
typedef struct
{
	long	voucher_num;
	char	voucher_type	[15 + 1];
	int		voucher_discount_given;
	int		voucher_amount_remaining;
	int		original_voucher_amount;
} TVoucherUsed;


// DonR 29Jan2024 User Story ##540234.
typedef struct
{
	short	DrugLineSubscript;
	int		cannabis_product_code;
	long	cannabis_product_barcode;
	short	op;
	short	units;
	int		price_per_op;
	int		product_sale_amount;
	int		product_ptn_amount;
}	TCannabisProducts;

typedef struct	// Trn. 6001 prescription-data structure - DonR 28Oct2014.
{
	short				DocIDType;
	int					DocID;
	TDoctorFirstName	DoctorFirstName;
	TDoctorFamilyName	DoctorFamilyName;
	char				DoctorPhone	[10 + 1];
	short				check_interactions;
}	TDoctorInfo;


typedef struct				/* -------< 1005 DRUGs >------- */
{
    TDrugCode						DrugCode;
    TOp								Op;
	TUnits							Units;
    TDuration						Duration;
    TOpDrugPrice					OpDrugPrice;
    TCurrentStockOp					CurrentStockOp;
    TCurrentStockInUnits			CurrentStockInUnits;
    TTotalMemberParticipation		TotalMemberParticipation;
	int								member_ptn_amt;
    TTotalDrugPrice					TotalDrugPrice;
    TLinkDrugToAddition				LinkDrugToAddition;
    short int						LineNo;
    TPriceSwap						PriceSwap;
    TAdditionToPrice				AdditionToPrice;
    short int						THalf;
    short int						Flg;
    char							LargoType;
    TMemberDiscountPercent			Discount;
    TMemberDiscountPercent			DiscountPercent;	// DonR: 28AUG2002 (Electronic Prescr.)
    int								package_size;		// DonR: 11SEP2002 (Electronic Prescr.)
    short							openable_package;	// DonR: 11SEP2002 (Electronic Prescr.)
    TOpDrugPrice					SupplierDrugPrice;	//20021031 Yulia
	TNumofSpecialConfirmation		SpecPrescNum;		// DonR 03Feb2004
	TSpecialConfirmationNumSource	SpecPrescNumSource;	// DonR 03Feb2004
	short int						ParticipMethod;		// DonR 27Jun2004
	short int						MacabiPriceFlag;	// DonR 29Jun2004
	short int						in_health_pack;		// DonR 16Nov2008
	short							tikrat_mazon_flag;	// DonR 01Mar2011
	short							IshurWithTikra;		// DonR 01Mar2011
	short							IshurTikraType;		// DonR 01Mar2011
	int								rule_number;		// DonR 01Mar2011
	int								dr_visit_date;		// DonR 04Mar2015
	int								dr_presc_date;		// DonR 14Jun2015
	int								dr_largo_code;		// DonR 14Jun2015
	short							NumRxLinks;			// 11Jun2017 CR #8425 - Number of pd_rx_link rows written for this drug line.
} Msg1005Drugs;


typedef struct				/* -------< 6005 DRUGs >------- */
{
	TDrugListRow	DL;		// DonR 02Aug2023: Should eventually replace many fields below.
	int				DrugCode;
	int				Op;
	int				Units;
    short			Duration;
	int				OpDrugPrice;
	int				StockOP;
	int				StockUnits;
	int				TotalPtn;
	int				TotalDrugPrice;
	int				LinkToAddition;
    short			LineNo;
 	int				AdditionToPrice;
    short			THalf;
    short			Flg;
    char			LargoType;
//	short			Discount;	DonR 23May2019 CR #29169: This variable doesn't get assigned any values, and was used in error to set price_extension on 6005-Spool INSERTS.
    int				package_size;
    short			openable_package;
 	int				SupplierDrugPrice;
	int				SpecPrescNum;
	short			SpecPrescNumSource;
	short			ParticipMethod;
	short			MemberPtnCode;
	int				member_ptn_amt;
	short			MacabiPriceFlag;
	short			in_health_pack;
	short			tikrat_mazon_flag;
	short			IshurWithTikra;
	short			IshurTikraType;
	int				rule_number;
	short			LineNumber;
	short			SubstPermitted;
	int				NumPerDose;
	short			DosesPerDay;
	short			in_gadget_table;
	int				SaleDate;
	int				Orig_Largo_Code;
	int				PrevUnsoldUnits;
	int				PrevUnsoldOP;
	int				FixedPrice;
	int				ph_otc_unit_price;
	int				PrescWrittenID;
	short			MemberDiscPC;
	int				DoctorRecipeNum;
	int				StopBloodDate;
	short			ingr_code			[3];
	double			ingr_quant			[3];
	double			ingr_quant_std		[3];
	double			ingr_quant_bot		[3];
	double			ingr_quant_bot_std	[3];
	double			per_quant			[3];
	double			package_volume;
	int				DrugDiscountAmt;
	short			DrugDiscountCode;
	int				VoucherAmtUsed;	// DonR 07Jun2021 User Story #163882.
	short			InsuranceUsed;
	char			DrugTikraType;
	char			DrugSubsidized;
	short			InHealthBasket;
	short			use_instr_template;
	short			how_to_take_code;
	char			unit_code			[4];
	short			course_treat_days;
	short			course_len;
	char			course_len_units	[11];
	short			course_len_days;
	char			days_of_week		[21];
	char			times_of_day		[201];
	char			side_of_body		[11];
	char			UsageInstrGiven		[101];	// DonR 21Mar2023 User Story #432608.
	short			ElectPrStatus;
	short			use_instr_changed;
	short			DocIDType;
	int				DocID;
	int				DocFacility;
	short			PrescSource;
	short			IsDigital;				// 22Aug2018
	long			VisitNumber;
	short			BarcodeScanned;			// 20Aug2018 CR #15804
	int				MaccabiOtcPrice;		// DonR 21Mar2023 User Story #432608.
	int				SalePkgPrice;			// DonR 21Mar2023 User Story #432608.
	int				SaleNum4Price;			// DonR 21Mar2023 User Story #432608.
	int				SaleNumBuy1Get1;		// DonR 21Mar2023 User Story #432608.
	int				Buy1Get1Savings;		// DonR 21Mar2023 User Story #432608.
	int				ByHandReduction;		// DonR 21Mar2023 User Story #432608.
	short			IsConsignment;			// DonR 21Mar2023 User Story #432608.
	short			NumCourses;				// DonR 21Mar2023 User Story #432608.
	short			FirstRx;
	short			NumDocRxes;
	short			NumRxLinks;		// 11Jun2017 CR #8425emergency - Number of pd_rx_link rows written for this drug line.
	int				blood_start_date;				// DonR 31Jul2023 User Story #469361.
	short			blood_duration;					// DonR 31Jul2023 User Story #469361.
	int				blood_last_date;				// DonR 31Jul2023 User Story #469361.
	short			blood_data_calculated;			// DonR 31Jul2023 User Story #469361.
	short			NumCannabisProducts;			// DonR 01Feb2024 User Story #540234.
} Msg6005Drugs;


typedef struct
{
	int		date;
	int		largo_code;
	int		economypri_group;					// From drug_list - added 06Nov2025.
	int		op;
	int		units;
	int		pharmacy_code;						// Send to MaccabiPharm only.
	char	pharmacy_name			[ 30 + 1];	// From pharmacy - send to MaccabiPharm only.
	short	sold_by_us;							// 0/1 - was sale made by the current pharmacy? Relevant for private pharmacies.
	short	presc_source;						// From prescription_drugs.
	char	pr_src_desc				[ 15 + 1];	// From prescr_source/source_category_desc.
	short	doctor_id_type;						// So we know whether to look up by license number.
	int		doctor_id;							// For doctor_id_type == 1, this is the license number.
	char	doc_first_name			[  8 + 1];	// From doctors - what about external doc name from doctor_presc for dentists etc?
	char	doc_last_name			[ 14 + 1];	// From doctors - what about external doc name from doctor_presc for dentists etc?
	char	speciality_desc			[ 25 + 1];	// From doctor_presc.
	int		rule_number;						// Iris removed this from the Trn. 6011 specification, but I'm leaving it "alive" just in case.
	int		special_presc_num;					// Make sure this has a value only when participation source == ishur kaspi! {MOD} (particip_method, 10) = 6.
	short	digital_rx;							// (Convert all non-zero values to 1.)
	short	use_instr_template;
	short	how_to_take_code;
	int		units_per_dose;
	char	unit_code				[  3 + 1];
	int		doses_per_day;						// Should really be a short, but it's defined as an integer in the database.
	int		duration;							// Should also really be a short.
	short	course_length;
	char	course_units			[ 10 + 1];	// (Truncate to len = 6, which is the maximum "real" length.)
	short	course_treat_days;
	short	NumCourses;
	char	days_of_week			[ 20 + 1];
	char	times_of_day			[200 + 1];
	char	side_of_body			[ 10 + 1];
	char	UsageInstrGiven			[100 + 1];

	// Fields for reporting ishur quantity limit usage.
	short	qty_limit_chk_type;					// From prescription_drugs. 2 == ishur purchase limit; anything else is irrelevant.
	short	ingr_1_code_bot;					// From prescription_drugs.
	short	ingr_2_code_bot;					// From prescription_drugs.
	short	ingr_3_code_bot;					// From prescription_drugs.
	double	ingr_1_quant_bot;					// From prescription_drugs.
	double	ingr_2_quant_bot;					// From prescription_drugs.
	double	ingr_3_quant_bot;					// From prescription_drugs.
	double	ingr_quant_to_report;				// Will be the correct ingr_X_quant_bot, or zero if not applicable.
	short	qty_lim_ingr;						// From special_prescs.
	char	qty_lim_units			[ 3 + 1];	// From special_prescs.
	char	unit_desc_heb			[ 8 + 1];	// From unit_of_measure (based on special_prescs/qty_lim_units).
} T6011_Drugs;


typedef struct
{
	int		main_largo_code;
	int		special_presc_num;
	int		treatment_start;
	int		stop_use_date;
	short	qty_lim_flg;
	double	qty_lim_per_day;
	char	unit_desc_heb			[ 8 + 1];	// From unit_of_measure (based on special_prescs/qty_lim_units).
	short	qty_lim_treat_days;
	short	qty_lim_course_len;
	short	qty_lim_courses;
	short	num_largos_in_ishur;
	int		largo_code;
	int		LargoCodeList	[250];				// As of July 2025, the real-world maximum is 91.
} T6011_Ishurim;


// DonR 01Jan2010: Fixed-price and in-health-basket data are already in
// the TParticTst substructure - so deleted them from the DrugExtension
// "parent" structure.
typedef struct				/* -----< DRUG EXTENSION >----- */
{
	TOp			op;
	TUnits		units;
	TDuration	duration;
	short		no_presc_sale_flag;
	int			rule_number;		// DonR 10Jun2010
	TParticTst	prc;
	short		price_code;
	int			fixed_price;
	char		insurance_type[2];
	short		needs_29_g;
	short		permission_type;	// DonR 12Jun2018
	short		confirm_authority;	// DonR 24Apr2025 User Story #390071.
} DrugExtension;


typedef struct drug_extension_row
{
	TDrugCode			largo_code;
	TAge				from_age;
	TAge				to_age;
	TMemberSex			member_gender;		// DonR 30Oct2003
//	TPermissionType		permission_type;	// DonR 12Jun2018 - moved this to the DrugExtension structure ("info").
	TTreatmentType		treatment_type;
	DrugExtension		info;
} DRUG_EXTENSION_ROW;


typedef struct tag_TStockReportRec
{
	TLineNum			v_LineNum;
	TDrugCode			v_DrugCode;
	TOpAmout			v_OpAmout;
	TUnitsAmout			v_UnitsAmout;
	TAmountType			v_AmountType;
	TBasePriceFor1Op	v_BasePriceFor1Op;
	TUnitPriceFor1Op	v_UnitPriceFor1Op;
	TTotalForItemLine	v_TotalForItemLine;
	TFullOpStock		v_FullOpStock;
	TCurrentUnitStock	v_CurrentUnitStock;
	TMinOpStock			v_MinOpStock;
} TStockReportRec;


typedef struct tag_TPrescDrugRec
{
	TDrugCode				v_DrugCode;
	TCurrentStockOp			v_CurrentStockOp;
	TCurrentStockInUnits	v_CurrentStockInUnits;
	TSqlInd					v_Flg;
} TPrescDrugRec;


typedef struct tag_TMoneyEmptyLine_Rec
{
	TStationNum				v_StationNum;
	TUserIdentification		v_UserIdentification;
	TCardNum				v_CardNum;
	TPaymentType			v_PaymentType;
	TSalesMoney				v_SalesMoney;
	TSalesActionNum			v_SalesActionNum;
	TRefundsMoney			v_RefundsMoney;
	TRefundActionNum		v_RefundActionNum;
} TMoneyEmpLineRec;


typedef struct tag_TPharmIshurRequest
{
    TDrugCode					v_DrugCode;
	TDrugCode					v_PreferredDrug;
	TMemberParticipatingType	v_PrtCodeRequested;
	TTotalMemberParticipation	v_PrtAmtRequested;
	TRuleNum					v_RuleNum;
	TProfessionx				v_Profession;
	TPackageSize				v_PackageSize;
	TStatus						v_Status;
	short						v_ExtendAs400IshurDays;
	TMemberParticipatingType	keren_mac_code;
	TMemberParticipatingType	mac_magen_code;
	int							EconomypriGroup;		// DonR 13Jan2005
	short int					SpecialistDrug;			// DonR 16Jan2005
	short int					PrefSpecialistDrug;		// DonR 16Jan2005
} TPharmIshurRequest;


// DonR 27Mar2014: Health Alert Rules.
typedef struct
{
	int		largo_code;
	short	pharmacology;
	short	treatment_type;
	short	min_treatment_type;
	short	max_treatment_type;
	short	chronic_flag;
	short	ingredient;
	short	shape_code;			// DonR 04Dec2023 User Story #529100.
	int		parent_group_code;	// DonR 28Feb2024.
	char	timing;				// 21Apr2020 CR #32576.
	short	enabled;			// Set by program rather than read from table.
	bool	InvertResult;		// DonR 19Aug2025 User Story #xxxx support for "starter" drugs.
} T_HAR_DrugTest;


typedef struct
{
	short			rule_number;
	char			rule_desc1	[81];
	char			rule_desc2	[81];
	short			rule_enabled;
	int				rule_start_date;
	int				rule_end_date;
	short			msg_code_assigned;
	short			member_age_min;
	short			member_age_max;
	short			member_sex;
	short			drug_test_mode;
	short			minimum_count;
	short			maximum_count;
	T_HAR_DrugTest	DRG [5];
	short			purchase_hist_days;
	short			presc_sale_only;
	short			lab_check_type;
	int				lab_req_code		[6];
	int				lab_result_low;
	int				lab_result_high;
	short			lab_check_days;
	short			new_drug_wait_days;
	short			lab_min_age_mm;
	short			lab_max_age_mm;
	int				ok_if_seen_prof [10];
	short			ok_if_seen_within;
	short			ok_if_seen_enabled;
	int				MinPurchaseDate;
	int				MinLabTestDate;
	int				MinDocVisitDate;
	char			invert_rasham		[2];
	short			rasham_code			[5];
	char			invert_diagnosis	[2];
	int				diagnosis_code		[5];
	char			invert_illnesses	[2];
	short			illness_code		[3];
	short			rasham_enabled;
	short			diagnoses_enabled;
	short			illnesses_enabled;
	short			drug_tests_enabled;
	short			force_history_test;	// 21Apr2020 CR #32576.
	int				illness_bitmap;
	short			days_between_warnings;		// Marianna 12May2024 Epic #178023
	short			max_num_of_warnings;		// Marianna 12May2024 Epic #178023
	short			warning_period_length;		// Marianna 12May2024 Epic #178023
	short			frequency_enabled;			// Marianna 12May2024 Epic #178023
	int				explanation_largo_code;		// Marianna 15Aug2024 Epic #178023
	bool			explanation_enabled;
	short			pharmacy_mahoz		[5];	// Marianna 14Aug2024 Epic #178023
	bool			pharmacy_mahoz_enabled;
	int				pharmacy_code		[5];	// Marianna 14Aug2024 Epic #178023
	bool			pharmacy_code_enabled;
	short			member_mahoz		[5];	// Marianna 14Aug2024 Epic #178023
	bool			member_mahoz_enabled;
} THealthAlertRules;


// Member data.

// DonR 28Nov2022 User Story 408077: Add new macros to test member eligibility. Note that
// Member.InsuranceType is calculated when As400UnixServer receives updates from AS/400, and
// should always be set to 'N' when (and only when) Member.maccabi_code is zero or negative.
// (I don't see any examples where maccabi_code is zero for non-darkonaim, although I'm told
// that it could happen in rare cases.) Since InsuranceType depends on maccabi_code, testing
// by both of them is redundant; on the other hand, we recently saw cases (of darkonaim) where
// we got zero for maccabi_code as our only indication that a person's commercial health
// insurance (and thus their "membership") had terminated, so testing only for negative values
// of maccabi_code isn't good enough. Mila Weisbich is fixing this on AS/400 so that we should
// stop getting zero for maccabi_code, but at the same time it makes sense to tighten up the
// test - there's no point in saying that a zero value is OK for eligibility while at the same
// time saying that it's *not* a recognized value for assigning InsuranceType!
// Note that at some point we *may* want to add Member.died_in_hospital to this macro.
//
// DonR 05Dec2022: The new function SetMemberFlags() sets up flag variables in the TMemberData
// structure that already reflect all the logic required for these macros. I'll leave the
// macros in place, but now they simply refer to the pre-computed flag variables.
#define	MEMBER_INELIGIBLE	(!Member.MemberEligible)
#define	MEMBER_ELIGIBLE		(Member.MemberEligible)

#define MEMBER_IN_MEUHEDET	(Member.MemberInMeuhedet)

// DonR 07Jul2013: Macro to identify members normally living overseas.
// DonR 04Jan2014: Changed macro to look at structure member rather than local variable.
// DonR 30Nov2022: Moved macro from individual source modules to MsgHndlr.h.
#define OVERSEAS_MEMBER		(Member.MemberLivesOverseas)

// And here's the member structure itself!
typedef struct
{
	int		TechnicalID;			// DonR 10Jun2021.
	short	current_vetek;
	short	prev_vetek;
	short	Has_PL_99997_Ishur;		// DonR 29Nov2016
	int		PL_99997_OpenDate;		// DonR 29Nov2016
	int		PL_99997_IshurNum;		// DonR 29Nov2016
	int		clicks_patient_id;		// DonR 01Mar2015: Added this, but I'm not sure when/if it will actually be use.
	short	VentilatorDiscount;		// DonR 13May2020 CR #31591. Currently "asaf_code" in database.
//	short	asaf_code;				// Should get changed to "ventilator_discount" in DB.

	// Derived flags - not read directly from the table.
	short	RowExists;				// DonR 14Aug2016: Whether we actually read a row from the members table.
	short	MemberMaccabi;
	short	MemberTzahal;
	short	MemberHova;
	short	MemberKeva;
	short	MemberEligible;			// DonR 04Dec2022: Add a couple more statuses as table members.
	short	MemberInMeuhedet;		// DonR 04Dec2022: Add a couple more statuses as table members.
	short	MemberLivesOverseas;	// DonR 04Dec2022: Add a couple more statuses as table members.
	short	age_months;

	// DonR 27Jun2021: Add fields to accept the full members row structure, with names the same
	// as the table columns. Functions that use non-structure variables or older structure
	// variables with different names should eventually be altered to use the new structure fields.
	// (Note that for fields that were already in the structure, I copied them here with their
	// existing comments.)
	short	card_date;			// DonR 30Jan2018: Added just to enable some code cleanup.
	bool	UsingTempMembId;
	char	father_name			[ 8 + 1];
	char	first_name			[ 8 + 1];
	char	last_name			[14 + 1];
	char	street				[12 + 1];
	char	house_num			[ 4 + 1];
	char	city				[20 + 1];
	int		zip_code;
	char	address				[50 + 1];
	short	mahoz;
	short	sex;
	short	marital_status;
	short	yahalom_code;		// "Yahalom" should really get renamed to "maccabi_sheli" in DB and code!
	int		yahalom_from;
	int		yahalom_until;
	short	mac_magen_code;
	int		mac_magen_from;
	int		mac_magen_until;
	short	keren_mac_code;		// Moved from "physical" position to logical position in structure.
	int		keren_mac_from;
	int		keren_mac_until;
	short	keren_wait_flag;
	int		carry_over_vetek;
	short	insurance_status;
	int		max_drug_date;
	int		update_date;		// DonR 01Jun2015: Support for service-without-card requests.
	int		update_time;		// DonR 01Jun2015: Support for service-without-card requests.
	int		data_update_date;
	int		data_update_time;
	short	del_flg;			// Not really in use - drop?
	short	credit_type_code;

	// DonR 09Jan2022: To avoid (or sidestep) confusion and bugs, set up the old discount_percent
	// element and the new member_discount_pt element as a union, so that they are in effect
	// synonyms.
	union
	{
		short	member_discount_pt;	// This is the name of the column in the database.
		short	discount_percent;	// This is a synonym - a lot of code still references it by this name.
	};

	// DonR 22Nov2022: Just to allow some simplified code, create a union between the old
	// string insurance_type value and a new, simple character value InsuranceType.
	union
	{
		char	insurance_type		[1 + 1];
		char	InsuranceType;
	};

	union
	{
		char	prev_insurance_type	[1 + 1];
		char	PrevInsuranceType;
	};

	// DonR 26Apr2022: We also need to create unions for Member TZ and Member TZ Code, since
	// they exist in two different versions in the structure and that causes horrible bugs.
	// While I'm at it, I'll also fix the Date of Birth field, although it's not as likely
	// to cause problems.
	union
	{
		int		ID;
		int		member_id;
	};

	union
	{
		short	ID_code;
		short	mem_id_extension;
	};

	union
	{
		int		BirthDate;			// DonR 08Aug2016: Added to support health-alert checking. date_of_bearth in DB (and new structure
		int		date_of_bearth;		// variable) should really get a spelling fix in DB and code!
	};

	int		doctor_status;		// Not clear if we really need a full integer for this (but not important, really).
	char	phone				[10 + 1];
	short	maccabi_code;
	int		maccabi_until;
	short	authorizealways;	// DonR 01Jun2015: Support for service-without-card requests.
	short	urgentonlygroup;
	int		updated_by;			// DonR 01Jun2015: Support for service-without-card requests.
	short	spec_presc;
	short	has_tikra;
	short	has_coupon;
	short	check_od_interact;	// DonR 23Jan2018 CR #13937 - new flag from members table to replace SELECT from MemberNotCheck.
	int		updateadr;
	int		idnumber_main;		// Head of family.
	short	idcode_main;		// Head of family.
	int		payer_tz;			// Can be someone *other* than head of family.
	short	payer_tz_code;		// Can be someone *other* than head of family.
	bool	payer_is_in_family;
	char	first_english		[15 + 1];
	char	last_english		[15 + 1];
	int		mess;
	int		snif;				// Not sure if this is still being used for anything.
	short	illness_1;			// We don't normally bother reading individual Illness Flags from the table - just the computed bitmap.
	short	illness_2;
	short	illness_3;
	short	illness_4;
	short	illness_5;
	short	illness_6;
	short	illness_7;
	short	illness_8;
	short	illness_9;
	short	illness_10;
	int		illness_bitmap;
	short	darkonai_type;			// DonR 16Aug2021: This is what we get in APPLIB/MEMBERS/DRK, with "bad" values changed to 0.
	short	force_100_percent_ptn;
	short	darkonai_no_card;
	short	has_blocked_drugs;		// DonR 17Oct2021 User Story #196891.
	short	in_hospital;			// DonR 28Oct2025: Moved this, just for better readability.
	short	died_in_hospital;		// DonR 25Nov2021 User Story #206812.
	short	dangerous_drug_status;	// DonR 29Jul2025 User Story #435969.
	short	InHomeHospice;			// DonR 28Oct2025 User Story #429086.
	short	NarcoLimitExempt;		// DonR 28Oct2025 User Story #429086.
	short	NarcoMaxDuration;		// DonR 28Oct2025 User Story #429086.
	short	NarcoMaxValidity;		// DonR 28Oct2025 User Story #429086.
	// DonR 27Jun2021 end.
} TMemberData;

// Marianna 22Apr2025: User Story #391697
// Structure to hold the eligibility data
typedef struct
{
	int     approval_type_code;
	char    approval_type_name[100];
	char    valid_from[30];		// YYYY-MM-DDThh:mm:ss
	char    valid_until[30];	// YYYY-MM-DDThh:mm:ss
	int     valid_from_date;    // YYYYMMDD
	int     valid_until_date;   // YYYYMMDD
	int     valid_from_time;    // HHMMSS
	int     valid_until_time;   // HHMMSS
} TEligibilityData;

// DonR 04Dec2022 User Story #408077: Create a global TMemberData structure to replace local ones.
#ifdef	MAIN
	TMemberData Member;
	int			GlobalSysDate;
#else
	extern TMemberData	Member;
	extern int			GlobalSysDate;
#endif

#define GLOBALSYSDATE_DEFINED


/*=======================================================================
||			1055 datatypes & defines			||
 =======================================================================*/

typedef struct
{
	char	var_name[61];
	int		var_type;	/* DUR / OVERDOSE	*/
	int		var_code;	/* key for VAR_DATA	*/
	int		var_length;
} DYN_VAR;

typedef struct
{
	char	*var_data;
	int		var_type;		/* ASCII / binary */
} VAR_DATA;

// DonR 11Mar2024 Bug #297605: Change integer types to reflect what we actually
// retrieve from the database; reading a short integer into an integer variable
// can cause errors, since we're leaving bytes 3-n of the integer with whatever
// value they had before.
typedef struct
{
	int						pharmacy_code;
	short					institued_code;
	short					terminal_id;
	int						prescription_id;
	short					line_no;
	int						largo_code;
	int						largo_code_inter;
	short					interaction_type;
	int						date;
	int						time;
	short					del_flg;
	int						presc_id_inter;
	short					line_no_inter;
	int						member_id;
	short					mem_id_extension;
	int						doctor_id;
	short					doctor_id_type;
	int						sec_doctor_id;
	short					sec_doctor_id_type;
	short					error_code;
	short					duration;
	int						op;
	int						units;
	int						stop_blood_date;
	short					check_type;
	short					destination;
	short					print_flg;
	short					doc_approved_flag;
} PRESC_ERR;

typedef struct
{
	TDrugDescription		drug_desc;
	TDrugDescription		drug_inter_desc;
	int						interaction_sever;
	TInteractionDescLine	interaction_desc;
	TInteractionDescLine	interaction_desc_add;
	TDoctorFirstName		doctor_first_name;
	TDoctorFamilyName		doctor_last_name;
	TPhone					doctor_phone;
	TDate					stop_blood_date;
	TDoctorFirstName		sec_doctor_first_n;
	TDoctorFamilyName		sec_doctor_last_n;
	TMemberFirstName		member_first_name;
	TMemberFamilyName		member_last_name;
} DUR_DATA;

typedef struct
{
	TDrugDescription	drug_desc;
	TDate				purchase_date;
	int					duration;
	TOp					op;
	TUnits				units;
	TDoctorFirstName	doc_first_name;
	TDoctorFamilyName	doc_last_name;
	TDoctorFirstName	curr_doc_f_name;
	TDoctorFamilyName	curr_doc_l_name;
	TPhone				curr_doc_phone;
	TPhone				doc_phone;
} OVD_DATA;

typedef struct
{
	char				msg_row[81];
	char				msg_alternate[81];	// DonR 01Mar2010: Used for "backup" text in the event that "normal"
											// message line has variables but they're all empty.
	short int			msg_sub_cycle;
} MESS_ROW;

typedef struct
{
	int					msg_row_max;
	int					msg_row_alloc;
	MESS_ROW			*msg_rows;
} MESS_DATA;

// DonR 24Nov2016: Add structure for Drug Purchase Limit data.
typedef struct
{
	int		largo_code;
	short	class_code;
	int		qty_lim_grp_code;
	short	limit_method;
	int		max_units;
	double	ingr_qty_std;
	short	presc_source;
	short	permit_source;
	short	warning_only;
	short	ingredient_code;
	short	exempt_diseases;
	short	duration_months;
	int		history_start_date;
	short	class_type;
	int		class_history_days;
	int		ishur_num;
	short	ishur_type;
	int		ishur_open_date;
	int		ishur_close_date;
	short	status;
	short	MonthIs28Days;
	short	NumQualifyingPurchases;			// DonR 07Feb2022
	short	CustomErrorCode;				// DonR 08Feb2022
	bool	include_full_price_sales;		// DonR 22Jul2025 User Story #427783
	short	no_ishur_for_pharmacology;		// DonR 16Nov2025 User Story #453336
	short	no_ishur_for_treatment_type;	// DonR 16Nov2025 User Story #453336
	int		prev_sales_span_days;			// DonR 16Nov2025 User Story #453336
} DRUG_PURCHASE_LIMIT;


typedef struct REST_Output_Struct
{
	char	*REST_output_buf;
	size_t	SizeAllocated;
	size_t	LengthUsed;
	size_t	GrowBufferIncrement;
} REST_MEMORY;

typedef struct REST_service_control
{
	// The first few elements are set up by set_up_consumed_REST_service().
	char				service_name		[ 40 + 1];
	char				service_desc		[100 + 1];	// varchar(100) in DB.
	char				*url_template;					// Allocate as much as is needed.
	char				*url;							// Allocate strlen(url_template) + allowance for request parameters.
	char				http_method;					// = 'G' or 'P' for Get/Post.
	struct curl_slist	*HeaderList;					// DonR 22Apr2025 User Story #391697: Add support for additional HTTP headers.

	// If the method is POST, calling routine should put the JSON object
	// pointer of the data to POST here.
	cJSON		*JSON_post_data;

	// If we're going to POST JSON data, we want to give the CJSON routines
	// an initial guess as to how big a "print" buffer we're going to need;
	// this avoids some extra buffer reallocation overhead. To be extra
	// clever (a.k.a. sneaky), we will automatically update this value when
	// the JSON output was in fact longer than the last value - so that the
	// next time we POST for this service, we'll start off with a guess
	// that's equal to the longest buffer the current program instance has
	// seen so far.
	int			JSON_print_buffer_length;

	// Alternatively, if the data to POST is just text, the calling routine
	// should leave JSON_post_data = NULL and put the text data here.
	char		*char_post_data;

	// Memory structure to receive the output of the service called.
	REST_MEMORY	curl_output_MEMORY;

	// These values are set by consume_REST_service().
	cJSON		**JSON_output;		// JSON output can have whatever scope is needed.
	CURL		*curl_instance;
//	int			curl_result;
	long		HTTP_ResponseCode;
} SERVICE;


/*
 * This paragraph is for disabling
 * SQL commands for non-SQL files
 */
//#ifdef NON_SQL				//

//#undef EXEC					//
//#undef SQL					//
//#undef begin				//
//#undef end					//
//#undef declare				//
//#undef section				//

//#endif						//

/*=======================================================================
||			    Function prototypes				||
 =======================================================================*/
int PrintErr		(	char	*output_buf,
				int	errcode,
				int     msgindex			);

void  close_files	(	FILE	*f_1,
				FILE	*f_2				);

void ChckInputLen (int diff     /* = [total-input] - [processed input] */);

void ChckInputLenAllowTrailingSpaces (char *buff_in, int diff     /* = [total-input] - [processed input] */);

int get_file_names	(	char	*in_file,
				char	*out_file,
				char	*cmdline_str			);

void WriteErrAndexit	(	int	errcode,
				int	msg_index			);

void AsciiToBin		(	char	*buff,
				int	len,
				int	*p_val				);

int get_message_header	(	char	*input_buf,
				int	*pMsgID,
				int	*pMsgEC				);

char GetChar (char **p_buffer);

short	GetShort	(char **p_buffer, int ascii_len);
int		GetInt		(char **p_buffer, int ascii_len);
long	GetLong		(char **p_buffer, int ascii_len);

void	  GetString	(	char	**p_buffer,
				char	*str,
				int	ascii_len			);

void GetNothing (char ** p_buffer,	int ascii_len);

int   GeToSpoolIndex	(	char	*in_file			);

//void  AddHeader		(	int	msg_index,
//				int	err_msg				);
//
int	diff_from_now	(	int	date,
				int	time				);

short rx_source_found (TPresDrugs *DrugLine, short DrugCount, short SourceTest, short SecondSourceTest, short Mode);

/*
 * Handlers to messages
 * --------------------
 */

int   HandlerToMsg_1001 (
    char    *input_buf,
    int     input_size,
    char    *output_buf,
    int	    *output_size,
    int	    *output_type_flg,
    SSMD_DATA	*ssmd_data_ptr
    );

int   HandlerToMsg_1003_1063	(
    char    *input_buf,
    int     input_size,
    char    *output_buf,
    int	    *output_size,
    int	    *output_type_flg,
    SSMD_DATA	*ssmd_data_ptr,
	int		transaction_num
    );

int   HandlerToMsg_1005_1065	(
    char    *input_buf,
    int     input_size,
    char    *output_buf,
    int	    *output_size,
    int	    *output_type_flg,
    SSMD_DATA	*ssmd_data_ptr,
	int		transaction_num
    );

int   DownloadDrugList	(	char		*input_buf,
							int			input_size,
							char		*output_buf,
							int			*output_size,
							int		    *output_type_flg,
							SSMD_DATA	*ssmd_data_ptr,
							int			MessageIndex_in	);

int   HandlerToMsg_1013	(
    char    *input_buf,
    int     input_size,
    char    *output_buf,
    int	    *output_size,
    int	    *output_type_flg,
    SSMD_DATA	*ssmd_data_ptr
    );

int   HandlerToMsg_1022_6022 (	int			TrnNumber_in,
								char		*input_buf,
								int			input_size,
								char		*output_buf,
								int			*output_size,
								int			*output_type_flg,
								SSMD_DATA	*ssmd_data_ptr		);

int   HandlerToMsg_1026 (
    char    *input_buf,
    int     input_size,
    char    *output_buf,
    int	    *output_size,
    int	    *output_type_flg,
    SSMD_DATA	*ssmd_data_ptr
    );

int   HandlerToMsg_1028( char		*IBuffer,
			 char		TotalInputLen,
			 char		*OBuffer,
			 int		*p_outputWritten,
			 int		*output_type_flg,
			 SSMD_DATA	*ssmd_data_ptr );

int   HandlerToMsg_1043 (
    char    *input_buf,
    int     input_size,
    char    *output_buf,
    int	    *output_size,
    int	    *output_type_flg,
    SSMD_DATA	*ssmd_data_ptr
    );

int   HandlerToMsg_1047 (
    char    *input_buf,
    int     input_size,
    char    *output_buf,
    int	    *output_size,
    int	    *output_type_flg,
    SSMD_DATA	*ssmd_data_ptr
    );

int   HandlerToMsg_1049 (
    char    *input_buf,
    int     input_size,
    char    *output_buf,
    int	    *output_size,
    int	    *output_type_flg,
    SSMD_DATA	*ssmd_data_ptr
    );

int   HandlerToMsg_1014 (
    char    *input_buf,
    int     input_size,
    char    *output_buf,
    int	    *output_size,
    int	    *output_type_flg,
    SSMD_DATA	*ssmd_data_ptr
    );

int   HandlerToMsg_1051 (
    char    *input_buf,
    int     input_size,
    char    *output_buf,
    int	    *output_size,
    int	    *output_type_flg,
    SSMD_DATA	*ssmd_data_ptr
    );

int   HandlerToMsg_1053 (
    char    *input_buf,
    int     input_size,
    char    *output_buf,
    int	    *output_size,
    int	    *output_type_flg,
    SSMD_DATA	*ssmd_data_ptr
    );

int   HandlerToMsg_1055 (
    char    *input_buf,
    int     input_size,
    char    *output_buf,
    int	    *output_size,
    int	    *output_type_flg,
    SSMD_DATA	*ssmd_data_ptr
    );

int HandlerToMsg_1080 (char			*IBuffer,
					   int			TotalInputLen,
					   char			*OBuffer,
					   int			*p_outputWritten,
					   int			*output_type_flg,
					   SSMD_DATA	*ssmd_data_ptr);

int HandlerToMsg_2001 (char			*IBuffer,
					   int			TotalInputLen,
					   char			*OBuffer,
					   int			*p_outputWritten,
					   int			*output_type_flg,
					   SSMD_DATA	*ssmd_data_ptr);

int HandlerToMsg_2003 (char			*IBuffer,
					   int			TotalInputLen,
					   char			*OBuffer,
					   int			*p_outputWritten,
					   int			*output_type_flg,
					   SSMD_DATA	*ssmd_data_ptr);

int HandlerToMsg_2005 (char			*IBuffer,
					   int			TotalInputLen,
					   char			*OBuffer,
					   int			*p_outputWritten,
					   int			*output_type_flg,
					   SSMD_DATA	*ssmd_data_ptr);

int   HandlerToMsg_2033_6033	(	int			TrnNumber_in,
									char		*IBuffer,
									int			TotalInputLen,
									char		*OBuffer,
									int			*p_outputWritten,
									int			*output_type_flg,
									SSMD_DATA	*ssmd_data_ptr		);

int HandlerToMsg_2060 (char			*IBuffer,
					   int			TotalInputLen,
					   char			*OBuffer,
					   int			*p_outputWritten,
					   int			*output_type_flg,
					   SSMD_DATA	*ssmd_data_ptr);

int HandlerToMsg_2062 (char			*IBuffer,
					   int			TotalInputLen,
					   char			*OBuffer,
					   int			*p_outputWritten,
					   int			*output_type_flg,
					   SSMD_DATA	*ssmd_data_ptr);

int HandlerToMsg_2064 (char			*IBuffer,
					   int			TotalInputLen,
					   char			*OBuffer,
					   int			*p_outputWritten,
					   int			*output_type_flg,
					   SSMD_DATA	*ssmd_data_ptr);

int HandlerToMsg_2066 (char			*IBuffer,
					   int			TotalInputLen,
					   char			*OBuffer,
					   int			*p_outputWritten,
					   int			*output_type_flg,
					   SSMD_DATA	*ssmd_data_ptr);

int HandlerToMsg_2068 (char			*IBuffer,
					   int			TotalInputLen,
					   char			*OBuffer,
					   int			*p_outputWritten,
					   int			*output_type_flg,
					   SSMD_DATA	*ssmd_data_ptr);

int HandlerToMsg_2070_2092 (int			TrNum,
							char		*IBuffer,
						    int			TotalInputLen,
							char		*OBuffer,
							int			*p_outputWritten,
							int			*output_type_flg,
							SSMD_DATA	*ssmd_data_ptr);

int HandlerToMsg_2072 (char			*IBuffer,
					   int			TotalInputLen,
					   char			*OBuffer,
					   int			*p_outputWritten,
					   int			*output_type_flg,
					   SSMD_DATA	*ssmd_data_ptr);

int HandlerToMsg_2074 (char			*IBuffer,
					   int			TotalInputLen,
					   char			*OBuffer,
					   int			*p_outputWritten,
					   int			*output_type_flg,
					   SSMD_DATA	*ssmd_data_ptr);

int HandlerToMsg_2076 (char			*IBuffer,
					   int			TotalInputLen,
					   char			*OBuffer,
					   int			*p_outputWritten,
					   int			*output_type_flg,
					   SSMD_DATA	*ssmd_data_ptr);

int HandlerToMsg_2078 (char			*IBuffer,
					   int			TotalInputLen,
					   char			*OBuffer,
					   int			*p_outputWritten,
					   int			*output_type_flg,
					   SSMD_DATA	*ssmd_data_ptr);

int HandlerToMsg_2080 (char			*IBuffer,
					   int			TotalInputLen,
					   char			*OBuffer,
					   int			*p_outputWritten,
					   int			*output_type_flg,
					   SSMD_DATA	*ssmd_data_ptr);

int HandlerToMsg_2082 (char			*IBuffer,
					   int			TotalInputLen,
					   char			*OBuffer,
					   int			*p_outputWritten,
					   int			*output_type_flg,
					   SSMD_DATA	*ssmd_data_ptr);

int HandlerToMsg_2084 (char			*IBuffer,
					   int			TotalInputLen,
					   char			*OBuffer,
					   int			*p_outputWritten,
					   int			*output_type_flg,
					   SSMD_DATA	*ssmd_data_ptr);

int HandlerToMsg_2086 (char			*IBuffer,
					   int			TotalInputLen,
					   char			*OBuffer,
					   int			*p_outputWritten,
					   int			*output_type_flg,
					   SSMD_DATA	*ssmd_data_ptr);

int HandlerToMsg_2088 (char			*IBuffer,
					   int			TotalInputLen,
					   char			*OBuffer,
					   int			*p_outputWritten,
					   int			*output_type_flg,
					   SSMD_DATA	*ssmd_data_ptr);

int HandlerToMsg_X090 (char			*IBuffer,
					   int			TotalInputLen,
					   char			*OBuffer,
					   int			*p_outputWritten,
					   int			*output_type_flg,
					   SSMD_DATA	*ssmd_data_ptr,
					   int			transaction_num);

int HandlerToMsg_2096 (char			*IBuffer,
					   int			TotalInputLen,
					   char			*OBuffer,
					   int			*p_outputWritten,
					   int			*output_type_flg,
					   SSMD_DATA	*ssmd_data_ptr);

int HandlerToMsg_2101 (char			*IBuffer,
					   int			TotalInputLen,
					   char			*OBuffer,
					   int			*p_outputWritten,
					   int			*output_type_flg,
					   SSMD_DATA	*ssmd_data_ptr);

int HandlerToMsg_5003 (char			*IBuffer,
					   int			TotalInputLen,
					   char			*OBuffer,
					   int			*p_outputWritten,
					   int			*output_type_flg,
					   SSMD_DATA	*ssmd_data_ptr);

int HandlerToMsg_5005 (char			*IBuffer,
					   int			TotalInputLen,
					   char			*OBuffer,
					   int			*p_outputWritten,
					   int			*output_type_flg,
					   SSMD_DATA	*ssmd_data_ptr);

int HandlerToMsg_5051 (char			*IBuffer,
					   int			TotalInputLen,
					   char			*OBuffer,
					   int			*p_outputWritten,
					   int			*output_type_flg,
					   SSMD_DATA	*ssmd_data_ptr);

int HandlerToMsg_5053 (char			*IBuffer,
					   int			TotalInputLen,
					   char			*OBuffer,
					   int			*p_outputWritten,
					   int			*output_type_flg,
					   SSMD_DATA	*ssmd_data_ptr);

int HandlerToMsg_5055 (char			*IBuffer,
					   int			TotalInputLen,
					   char			*OBuffer,
					   int			*p_outputWritten,
					   int			*output_type_flg,
					   SSMD_DATA	*ssmd_data_ptr);

int HandlerToMsg_5057 (char			*IBuffer,
					   int			TotalInputLen,
					   char			*OBuffer,
					   int			*p_outputWritten,
					   int			*output_type_flg,
					   SSMD_DATA	*ssmd_data_ptr);

int HandlerToMsg_5059 (char			*IBuffer,
					   int			TotalInputLen,
					   char			*OBuffer,
					   int			*p_outputWritten,
					   int			*output_type_flg,
					   SSMD_DATA	*ssmd_data_ptr);

int HandlerToMsg_5061 (	int			TransactionID_in,
						char		*IBuffer,
						int			TotalInputLen,
						cJSON		*JSON_MaccabiRequest,
						char		*OBuffer,
						int			*p_outputWritten,
						int			*output_type_flg,
						SSMD_DATA	*ssmd_data_ptr,
						int			*NumProcessed_out	);

int HandlerToMsg_6001_6101 (	int			TransactionID_in,
								char		*IBuffer,
								int			TotalInputLen,
								cJSON		*JSON_MaccabiRequest,
								char		*OBuffer,
								int			*p_outputWritten,
								int			*output_type_flg,
								SSMD_DATA	*ssmd_data_ptr,
								int			*NumProcessed_out	);

int HandlerToMsg_6003 (	int			TrNum,
						char		*IBuffer,
						int			TotalInputLen,
						char		*OBuffer,
						int			*p_outputWritten,
						int			*output_type_flg,
						SSMD_DATA	*ssmd_data_ptr,
						int			*NumProcessed_out);

int HandlerToMsg_6005 (	char		*IBuffer,
						int			TotalInputLen,
						char		*OBuffer,
						int			*p_outputWritten,
						int			*output_type_flg,
						SSMD_DATA	*ssmd_data_ptr,
						int			*NumProcessed_out);

int HandlerToSpool_6005 (	char		*IBuffer,
							int			TotalInputLen,
							char		*OBuffer,
							int			*p_outputWritten,
							int			*output_type_flg,
							SSMD_DATA	*ssmd_data_ptr,
							short int	commfail_flag,
							int			*NumProcessed_out);

int HandlerToMsg_6011 (	int			TransactionID_in,
						char		*IBuffer,
						int			TotalInputLen,
						cJSON		*JSON_MaccabiRequest,
						char		*OBuffer,
						int			*p_outputWritten,
						int			*output_type_flg,
						SSMD_DATA	*ssmd_data_ptr,
						int			*NumProcessed_out	);

int HandlerToMsg_6102 (	int			TransactionID_in,
						char		*IBuffer,
						int			TotalInputLen,
						cJSON		*JSON_MaccabiRequest,
						char		*OBuffer,
						int			*p_outputWritten,
						int			*output_type_flg,
						SSMD_DATA	*ssmd_data_ptr,
						int			*NumProcessed_out	);

int HandlerToMsg_6103 (	int			TransactionID_in,
						char		*IBuffer,
						int			TotalInputLen,
						cJSON		*JSON_MaccabiRequest,
						char		*OBuffer,
						int			*p_outputWritten,
						int			*output_type_flg,
						SSMD_DATA	*ssmd_data_ptr,
						int			*NumProcessed_out	);

/*
 * Generic functions( -> called in both SPOOL & REALTIME ) modes:
 * --------------------------------------------------------------
 */
int   GENERIC_HandlerToMsg_1022_6022	(	int			TrnNumber_in,
											char		*IBuffer,
											int			TotalInputLen,
											char		*OBuffer,
											int			*p_outputWritten,
											int			*output_type_flg,
											SSMD_DATA	*ssmd_data_ptr,
											short		commfail_flag,
											int			fromSpool			);

int   GENERIC_HandlerToMsg_1053( char		*IBuffer,
				 int		TotalInputLen,
				 char		*OBuffer,
				 int		*p_outputWritten,
				 int		*output_type_flg,
				 SSMD_DATA	*ssmd_data_ptr,
				 short int	commfail_flag,
				 int		fromSpool );

int   GenHandlerToMsg_1001( char		*IBuffer,
			     int		TotalInputLen,
			     char		*OBuffer,
			     int		*p_outputWritten,
			     int		*output_type_flg,
			     SSMD_DATA		*ssmd_data_ptr,
			     int		fromspool,
			     int		commerr_flag);

int   GenHandlerToMsg_1013( char		*IBuffer,
			     int		TotalInputLen,
			     char		*OBuffer,
			     int		*p_outputWritten,
			     int		*output_type_flg,
			     SSMD_DATA		*ssmd_data_ptr,
			     int		fromspool,
			     int		commerr_flag);

int GENERIC_HandlerToMsg_2033_6003	(	int			TrnNumber_in,
										char		*IBuffer,
										int			TotalInputLen,
										char		*OBuffer,
										int			*p_outputWritten,
										int			*output_type_flg,
										SSMD_DATA	*ssmd_data_ptr,
										short		commfail_flag,
										short		fromSpool			);

/*
 * Handlers to spool
 * -----------------
 */
int   HandlerToSpool_1001( char		*IBuffer,
			   int		TotalInputLen,
			   char		*OBuffer,
			   int		*p_outputWritten,
			   int		*output_type_flg,
			   SSMD_DATA	*ssmd_data_ptr,
			   short int	commfail_flag );

int   HandlerToSpool_1005( char		*IBuffer,
			   int		TotalInputLen,
			   char		*OBuffer,
			   int		*p_outputWritten,
			   int		*output_type_flg,
			   SSMD_DATA	*ssmd_data_ptr,
			   short int	commfail_flag );

int   HandlerToSpool_1013( char		*IBuffer,
			   int		TotalInputLen,
			   char		*OBuffer,
			   int		*p_outputWritten,
			   int		*output_type_flg,
			   SSMD_DATA	*ssmd_data_ptr,
			   short int	commfail_flag );

int   HandlerToSpool_1022_6022	(	int			TrnNumber_in,
									char		*IBuffer,
									int			TotalInputLen,
									char		*OBuffer,
									int			*p_outputWritten,
									int			*output_type_flg,
									SSMD_DATA	*ssmd_data_ptr,
									short		commfail_flag		);

int   HandlerToSpool_1053( char		*IBuffer,
			   int		TotalInputLen,
			   char		*OBuffer,
			   int		*p_outputWritten,
			   int		*output_type_flg,
			   SSMD_DATA	*ssmd_data_ptr,
			   short int	commfail_flag );

int   HandlerToSpool_1061( char    *input_buf,
			   int     input_size,
			   char    *output_buf,
			   int	    *output_size,
			   int	    *output_type_flg,
			   SSMD_DATA	*ssmd_data_ptr,
			   short int	commfail_flag );

int HandlerToSpool_2005 (char			*IBuffer,
						 int			TotalInputLen,
						 char			*OBuffer,
						 int			*p_outputWritten,
						 int			*output_type_flg,
						 SSMD_DATA		*ssmd_data_ptr,
						 short int		commfail_flag);

int   HandlerToSpool_2033_6033	(	int			TrnNumber_in,
									char		*IBuffer,
									int			TotalInputLen,
									char		*OBuffer,
									int			*p_outputWritten,
									int			*output_type_flg,
									SSMD_DATA	*ssmd_data_ptr,
									short int	commfail_flag		);

int HandlerToSpool_5005 (char			*IBuffer,
						 int			TotalInputLen,
						 char			*OBuffer,
						 int			*p_outputWritten,
						 int			*output_type_flg,
						 SSMD_DATA		*ssmd_data_ptr,
						 short int		commfail_flag);

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
						  int			*Restart_out);


int test_interaction_and_overdose (TMemberData						*Member,
								   TDate							MemMaxDrugDate_in,
//								   TGenericTZ						DocID_in,
//								   TTypeDoctorIdentification		DocIdType_in,
								   TPharmNum						PharmNum_in,
								   PHARMACY_INFO					*PharmInfoPtr_in,
								   TInstituteCode					InstCode_in,
								   TTerminalNum						TerminalNum_in,
								   TRecipeIdentifier				RecipeID_in,
								   TPresDrugs						*DrugArray_in,
								   TNumOfDrugLinesRecs				NumLines_in,
								   TNumofSpecialConfirmation		SpecConfNum_in,
								   TSpecialConfirmationNumSource	SpecConfSource_in,
								   short							ErrorMode_in,
								   short							ElectPR_flag_in,
								   TDocRx							*DocRxArray_in,
								   TMsgExistsFlg					*MsgFlag_out,
								   TMsgExistsFlg					*MsgIsUrgent_out,
								   TErrorCode						*ErrorCode_out,
								   int								*ReStart_out);

int write_interaction_and_overdose (
									TPharmNum						PharmNum_in,
									TInstituteCode					InstCode_in,
									TTerminalNum					TerminalNum_in,
									TGenericTZ						MemberID_in,
									TIdentificationCode				MemIdCode_in,
									int								dur_od_err_in,
									int								detail_err_in,
									short int						PrintFlag_in,
									short int						ErrorMode_in,
									short int						ElectronicPr_in,
									TGenericTZ						DocID_in,
									TTypeDoctorIdentification		DocIdType_in,
									TRecipeIdentifier				RecipeID_in,
									TDrugCode						Largo1_in,
									short							doc_approved_flag_in,

									TInteractionType				interaction_type_in,
									int								CheckType_in,

									short int						Subscript1_in,

									short int						Subscript2_in,
									DURDrugsInBlood					*BloodDrug_in,
									TPresDrugs						*PresDrug_in,
									int								StopBloodDate_in);

int find_preferred_drug (TDrugCode		SoldDrug_in,
						 int			EconomypriGroup_in,
						 short			PreferredFlag_in,
						 int			PreferredLargo_in,
						 short			NeedHistoryCheck_in,
						 short			SwapOutType_3_Drugs_in,
						 int			TreatmentContractor_in,
						 int			MemberId_in,
						 short			MemberIdCode_in,
						 TDrugCode		*PrefDrug_out,
						 short			*SpecialistDrug_out,
						 int			*ParentGroupCode_out,
						 short			*PrefDrugMemberPrice_out,
						 short			*DispenseAsWritten_out,
						 short			*InHealthBasket_out,
						 short			*RuleStatus_out,
						 TDrugListRow	*DL_out,
						 TErrorCode	*ErrorCode_out);

short MemberFailedToPickUpDrugs (TMemberData *Member_in);

void ChckDate		(	int	date /* format: YYYYMMDD */	);

void ChckTime		(	int	time /* format: HHNNSS   */	);

void free_ptr		(	void					);

int  tm_to_date		(	struct	tm *tms				);

void date_to_tm		(	int	inp,
				struct	tm *TM				);

void time_to_tm		(	int	inp,
				struct	tm *TM				);

int  GetMonsDif		(	int	base_date,
				int	sub_date			);

int  GetDaysDiff	(	int	base_day,
				int	sub_day				);

int  AddDays		(	int	base_day,
				int	add_day				);

int WinHebToUTF8 (unsigned char *Win1255_in, unsigned char **UTF8_out, size_t *length_out);

//TErrorCode
//	GET_PARTICIPATING_PERCENT(
//				  TParticipatingType	percent_code,
//				  TParticipatingPrecent	*percent
//				  );
//
//
//TErrorCode
//	GET_PARTICIPATING_CODE(
//			       TParticipatingPrecent	percent,
//			       TParticipatingType	*percent_code
//			       );

#define GET_PARTICIPATING_PERCENT(percent_code,percent) read_member_price(percent_code,NULL,percent)

#define GET_MEMBER_PRICE_ROW(percent_code,PriceRow) read_member_price(percent_code,PriceRow,NULL)

int read_member_price (short				PriceCode_in,
					   Tmember_price_row	*PriceRow_out,
					   short				*Percent_out);


TErrorSevirity
	GET_ERROR_SEVERITY(
			   TErrorCode	error_code
			   );


TErrorCode
	GET_PRESCRIPTION_ID(
			    TRecipeIdentifier	*recipe_identifier
			    );


TErrorCode
	FIND_DRUG_EXTENSION	(
							TDrugCode			LargoCode_in,
							TAge				member_age,
							TMemberSex			member_gender,
							TPermissionType		permission_type,
							TFlg				no_presc_flg,
							TTreatmentType		treatment_type,
							int					DoctorRuleNumber,
//							bool				WasPossibleDrugShortage,	DonR 18May2025 Reverting to previous version - but leaving the code here just in case.
							DrugExtension		*drug_ext_info
						);


TErrorCode GetParticipatingCode (TMemberParticipatingType	presParti,
								 TParticTst					*prt,
								 TPresDrugs					*PresLinP,
								 int						FixedPriceEnabled);

TErrorCode SumMemberDrugsUsed(
			      TPresDrugs		*pDrug,
			      TMemberIdentification	membId,
			      TIdentificationCode	idType,
			      TDuration			duration,
			      int			*SumUnits
			      );

int read_member (	int			MemberID_in,
					short		MemberID_code_in	);

int read_drug (int				DrugCode_in,
			   int				MaxVisibleDrug_in,
			   PHARMACY_INFO	*PharmInfo_in,
			   bool				SeeDeletedDrugs_in,
			   TDrugListRow		*DL_out,
			   TPresDrugs		*PresDrugs_out);

TErrorCode predict_member_participation (	TMemberData		*Member_in,
											TDrugListRow	*DL_in,
											PHARMACY_INFO	*PhrmInfo_in,
											short			DocIdType_in,
											int				DocId_in,
											int				VisitDate_in,
											int				DoctorRuleNumber_in,
											short			*PriceCode_out,
											short			*PricePercent_out,
											short			*FixedPriceFlag_out,
											int				*FixedPrice_out,
											short			*PermissionType_out,
											short			*InsuranceUsed_out,
											short			*HealthBasket_out,
											short			*ErrorCode_out			);

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

short CheckForServiceWithoutCard (	TMemberData		*Member,
									PHARMACY_INFO	Phrm_info);


// DonR 18Dec2017: Added a new "mode" parameter and renamed ReadDrugPurchaseLimitData() to ReadDrugPurchaseLimData_x().
// The new version of the function is called via two macros: ReadDrugPurchaseLimitData() is #defined so that the function
// works in "normal" mode, reading Drug Purchase Limits for all limit methods; while ReadNonIshurPurchaseLimitMethods() works
// in "fallback" mode, reading Method 0 and Method 1 limits only. This is used in test_purchase_limits() when a Method 3/4
// limit was found, but there is no applicable AS/400 ishur to provide a date for the start of limit checking.
// DonR 11Feb2024 BUG FIX #480141 revision: In some cases, someone (by mistake?) creates a "nohal" that gives discounts even
// on OTC sales. In this case, we *do* want to check purchase limits. Since at the time ReadDrugPurchaseLimitData() is called
// in "normal" mode we don't yet know whether the purchase is going to be at full price, we go ahead and suppress Limit
// Methods 0, 1, 3, and 4; but then in test_purchase_limits(), if we see that an OTC sale *is* being made at a discount, we'll
// come back with a call to ReadPurchaseLimitsAllMethods() that will set ReadRegularLimitsForOTC TRUE and thus read the
// "traditional" limits even for the OTC purchase.
TErrorCode	ReadDrugPurchaseLimData_x (TMemberData			*Member_in,
									   TPresDrugs			*CurrSaleDrugArray_in,
									   short				CurrSaleDrugLineNumber_in,
									   short				CurrSaleDrugLineCount_in,
									   TDrugsBuf			*BloodDrugsBuf_in,
									   short				RegularMethodsOnly,
									   short				ReadRegularLimitsForOTC,
									   DRUG_PURCHASE_LIMIT	*PurchaseLimit_out,
									   TErrorCode			*ErrorCode_out,
									   int					*reStart_out);

#define	ReadDrugPurchaseLimitData(	Member_in,						\
									CurrSaleDrugArray_in,			\
									CurrSaleDrugLineNumber_in,		\
									CurrSaleDrugLineCount_in,		\
									BloodDrugsBuf_in,				\
									PurchaseLimit_out,				\
									ErrorCode_out,					\
									reStart_out	)					\
		ReadDrugPurchaseLimData_x (	Member_in,						\
									CurrSaleDrugArray_in,			\
									CurrSaleDrugLineNumber_in,		\
									CurrSaleDrugLineCount_in,		\
									BloodDrugsBuf_in,				\
									0, /* Read all limit methods */	\
									0, /* Normal OTC mode		 */	\
									PurchaseLimit_out,				\
									ErrorCode_out,					\
									reStart_out	)

#define	ReadNonIshurPurchaseLimitMethods(	Member_in,						\
											CurrSaleDrugArray_in,			\
											CurrSaleDrugLineNumber_in,		\
											CurrSaleDrugLineCount_in,		\
											BloodDrugsBuf_in,				\
											PurchaseLimit_out,				\
											ErrorCode_out,					\
											reStart_out	)					\
																		\
		ReadDrugPurchaseLimData_x	(	Member_in,						\
										CurrSaleDrugArray_in,			\
										CurrSaleDrugLineNumber_in,		\
										CurrSaleDrugLineCount_in,		\
										BloodDrugsBuf_in,				\
										1, /* Ignore Methods 2,3,4 */	\
										0, /* Normal OTC mode	   */	\
										PurchaseLimit_out,				\
										ErrorCode_out,					\
										reStart_out	)

// DonR 11Feb2024 BUG FIX #480141 revision: New macro to disable suppression of "traditional" limits for OTC purchases.
#define	ReadPurchaseLimitsAllMethods(		Member_in,							\
											CurrSaleDrugArray_in,				\
											CurrSaleDrugLineNumber_in,			\
											CurrSaleDrugLineCount_in,			\
											BloodDrugsBuf_in,					\
											PurchaseLimit_out,					\
											ErrorCode_out,						\
											reStart_out	)						\
																				\
		ReadDrugPurchaseLimData_x		(	Member_in,							\
											CurrSaleDrugArray_in,				\
											CurrSaleDrugLineNumber_in,			\
											CurrSaleDrugLineCount_in,			\
											BloodDrugsBuf_in,					\
											0, /* Read all limit methods    */	\
											1, /* Force reading all methods */	\
											PurchaseLimit_out,					\
											ErrorCode_out,						\
											reStart_out	)
// DonR 18Dec2017 end.


// DonR 14Feb2022: Prototypes for functions to consume REST services.

// Local prototypes - these two functions are used only by set_up_consumed_REST_service()
// and consume_REST_service().
static size_t	REST_CurlMemoryCallback (void *contents, size_t size, size_t nmemb, void *userp);
static void		REST_CurlMemoryClear (REST_MEMORY *MEM);

// Global prototypes.
int set_up_consumed_REST_service (char *ServiceNameIn, SERVICE *service, cJSON **JSON_output_ptr);
int consume_REST_service (SERVICE *service);

// DonR 26Nov2023: Supply missing function prototype(s).
int SetMemberFlags (int SQLCODE_in);

#endif /* MSG_HANDLER_H */ //

/*=======================================================================
||			      End Of MsgHndlr.h				||
 =======================================================================*/
