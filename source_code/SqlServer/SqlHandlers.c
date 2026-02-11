/*=======================================================================
||																		||
||				SqlHandlers.c											||
||																		||
||======================================================================||
||																		||
||  PURPOSE:															||
||																		||
||  Get messages coming from T-SWITCH, analyze them, do the				||
||  actual work that has to be done, then send a corrresponding answer	||
||  thru the T-SWITCH server.											||
||																		||
||----------------------------------------------------------------------||
|| PROJECT:	SqlServer. (for macabi)										||
||----------------------------------------------------------------------||
|| ORIGINAL PROGRAMMERS:	Ely Levy, Gilad Haimov, Boaz Shnapp			||
|| DATE:	Aug 1996 - Jun 1997.										||
|| COMPANY:	Reshuma Ltd.												||
||----------------------------------------------------------------------||
 =======================================================================*/



// Include files.
#include <MacODBC.h>
#include "MsgHndlr.h"
#include "PharmacyErrors.h"
#include "TikrotRPC.h"
#include "MessageFuncs.h"
#include "As400UnixMediator.h"


static char *GerrSource = __FILE__;

extern	TDrugsBuf	v_DrugsBuf;	// DonR 16Feb2006 - made drugs-in-blood global.

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

#define MACCABI_PHARMACY	(Phrm_info.pharmacy_permission == PERMISSION_TYPE_MACABI)
#define PRIVATE_PHARMACY	(Phrm_info.pharmacy_permission != PERMISSION_TYPE_MACABI)
#define	CAN_SELL_FUTURE_RX	(Phrm_info.can_sell_future_rx)

#define IS_NEED_DUR_OD_TEST										\
(	(MACCABI_PHARMACY &&										\
		( (	v_SpecialConfirmationNum != 9 &&					\
			v_SpecialConfirmationNum != 8 )	||					\
			v_SpecialConfirmationNumSource != 1 ) )		||		\
	(	PRIVATE_PHARMACY &&										\
		(	v_SpecialConfirmationNum != 77777777 ||				\
			v_SpecialConfirmationNumSource != 1 ) ) )

#define IS_NEED_PARTICI_TEST						\
(  (MACCABI_PHARMACY  &&							\
      ( (v_SpecialConfirmationNum != 1	&&			\
         v_SpecialConfirmationNum != 2	&&			\
         v_SpecialConfirmationNum != 3	&&			\
         v_SpecialConfirmationNum != 4	&&			\
         v_SpecialConfirmationNum != 5  &&			\
         v_SpecialConfirmationNum != 6) || 			\
      v_SpecialConfirmationNumSource != 1) ) ||		\
   ( PRIVATE_PHARMACY &&			\
      (v_SpecialConfirmationNum != 2 ||				\
       v_SpecialConfirmationNumSource != 1) ) )	

#define  IS_NEED_ANY_TEST						\
( ( MACCABI_PHARMACY  &&						\
     ( ( v_SpecialConfirmationNum != 11	&&		\
         v_SpecialConfirmationNum != 12	&&		\
	 v_SpecialConfirmationNum != 13	&&			\
	 v_SpecialConfirmationNum != 14	&&			\
	 v_SpecialConfirmationNum != 15 &&			\
	 v_SpecialConfirmationNum != 16)  ||		\
	v_SpecialConfirmationNumSource != 1))	||	\
  ( PRIVATE_PHARMACY &&							\
      (v_SpecialConfirmationNum != 99999999 ||	\
       v_SpecialConfirmationNumSource != 1) ) )


#define T1063_CREDIT_PHARM(Pharm) (((Pharm == 700182	) ||	\
								    (Pharm == 700582	) ||	\
								    (Pharm == 701582	) ||	\
								    (Pharm == 702182	) ||	\
								    (Pharm == 702282	) ||	\
								    (Pharm == 702382	) ||	\
								    (Pharm == 703782	) ||	\
								    (Pharm == 709582	)) ? 1 : 0)


char *PosInBuff;
static int  nOut;


/*
 * Macros for 1055 messages rows array
 * ===================================
 */
#define	SEVER_NUM	2
#define	PART_NUM	4
#define	DEST_NUM	4
#define	TYPE_NUM	2

#define LEN_BY_ATTR( type, dest, part, sever )		\
	( SEVER_NUM * PART_NUM * DEST_NUM * (type) +	\
	SEVER_NUM * PART_NUM * (dest) +			\
	SEVER_NUM * (part) +				\
	(sever) )

#define TYPE_BY_LEN( len )				\
	( (len) / ( SEVER_NUM * PART_NUM * DEST_NUM ) );

#define DEST_BY_LEN( len )				\
	( ((len) / ( SEVER_NUM * PART_NUM ) ) % DEST_NUM );

#define PART_BY_LEN( len )				\
	( ((len) / SEVER_NUM ) % PART_NUM );

#define SEVER_BY_LEN( len )				\
	( (len) % SEVER_NUM );
// hold dur/overdose error
// for table presc_drug_inter


// Global SQL variables for identifying "visible" drugs.
static short int	drug_deleted		= DRUG_DELETED;
static short int	pharmacy_assuta	= PHARMACY_ASSUTA;
static short int	assuta_drug_active	= ASSUTA_DRUG_ACTIVE;
static short int	assuta_drug_past	= ASSUTA_DRUG_PAST;
//extern char		MemberPrevInsType [2];	// DonR 29Nov2012: Yahalom enhancement.
//extern short	MemberCurrVetek;		// DonR 29Nov2012: Yahalom enhancement.
//extern short	MemberPrevVetek;		// DonR 29Nov2012: Yahalom enhancement.


#define	NO_ERROR 			0
#define	ERR_DATABASE_ERROR 		3010

//
// D.U.R. decision table (drug interactions)
//
#define OLD_STYLE		0
#define	ELECTRONIC_PR	1
#define	SEVER			0
#define	NOT_SEVER		1
#define	TWO_DOCTORS		0
#define	SAME_DOCTOR		1
#define	PRESC_ERR_CODE	0
#define	DRUG_ERR_CODE	1
#define	PRINT_FLAG		2
#define MESSAGE_FLAG	3
#define DRUG_ERR_DB_FLG	4
#define DUR_DETAIL_ERR	5	// Written to presc_drug_interact for statistics.

int	DurDecisionTable[2][2][2][6] =
{
	{
		// "Old style" - 
		{
			// PRESC_ERR_CODE				DRUG_ERR_CODE		PRINT	MSG		DRUG_ERR_DB_FLG		DUR_DETAIL_ERR
			// severe + two doctors
			{ ERR_DRUG_S_WRONG_IN_PRESC,	ERR_DUR_ERROR,		PRINT,	1,		ERR_DUR_ERROR,		ERR_SEVERE_INTERACTION_WARN },
			// severe + same doctor
			{ 0,							ERR_DUR_WARRNING,	PRINT,	1,		ERR_DUR_WARRNING,	ERR_SEVERE_INTERACTION_SAMEDOC }
		},
		{
			// not severe + two doctors
			{ 0,							ERR_DUR_WARRNING,	PRINT, 1, ERR_DUR_WARRNING,	ERR_DUR_WARRNING },
			// not severe + same doctor
			{ 0,							0,					0,	   0, 0,				0 }
		}
	},

	{
		// "New style" - Electronic Prescription.
		{
			// PRESC_ERR_CODE				DRUG_ERR_CODE					PRINT	MSG		DRUG_ERR_DB_FLG					DUR_DETAIL_ERR
			// severe + two doctors
			{ 0,							ERR_SEVERE_INTERACTION_WARN,	PRINT,	1,		ERR_OD_INTERACT_NEEDS_REVIEW,	ERR_SEVERE_INTERACTION_WARN },
			// severe + same doctor
			{ 0,							ERR_DUR_WARRNING,				PRINT,	1,		ERR_DUR_WARRNING,				ERR_SEVERE_INTERACTION_SAMEDOC }
		},
		{
			// not severe + two doctors
			{ 0,							ERR_DUR_WARRNING,				PRINT,	1,		ERR_DUR_WARRNING,				ERR_DUR_WARRNING },
			// not severe + same doctor
			{ 0,							0,								0,		0,		0,								0 }
		}
	}
};


// OD decision table (drug overdoses)
#define OD_PRESC_ERR	0
#define OD_DRUG_ERR		1
#define OD_1055_ERR		2	// Written to presc_drug_interact for Transaction 1055 use.
#define	OD_PRINT_FLAG	3
#define OD_DETAIL_ERR	4	// Written to presc_drug_interact for statistics.

int	OD_DecisionTable[2][4][5] =
{
	// "Old style" - 
	{
		// Overall error			Drug error					Interact error (1055)			Print		Detail error.
		//
		// Limit <= 0.
		{ERR_DRUG_S_WRONG_IN_PRESC,	ERR_OVER_DOSE_ERROR,		ERR_OVER_DOSE_ERROR,			NO_PRINT,	ERR_ZERO_OVERDOSE_WARN},
		// O/D based on Average Dose.
		{ERR_DRUG_S_WRONG_IN_PRESC,	ERR_OVER_DOSE_ERROR,		ERR_OVER_DOSE_ERROR,			NO_PRINT,	ERR_AVG_OVER_DOSE_WARN},
		// O/D based on Daily Dose.
		{0,							ERR_OVER_DOSE_WARRNING,		ERR_OVER_DOSE_WARRNING,			NO_PRINT,	ERR_OVER_DOSE_WARRNING},
		// O/D based on Daily Dose double the limit.
		{ERR_DRUG_S_WRONG_IN_PRESC,	ERR_OVER_DOSE_ERROR_DAYS,	ERR_OVER_DOSE_WARRNING,			PRINT,		ERR_DOUBLE_DAILY_OD_WARN}
	},

	// Electronic Prescription.
	{
		// Overall error			Drug error					Interact error (1055)			Print		Detail error.
		//
		// Limit <= 0.
		{0,							ERR_AVG_OVER_DOSE_WARN,		ERR_OD_INTERACT_NEEDS_REVIEW,	PRINT,		ERR_ZERO_OVERDOSE_WARN},
		// O/D based on Average Dose.
		{0,							ERR_AVG_OVER_DOSE_WARN,		ERR_OD_INTERACT_NEEDS_REVIEW,	PRINT,		ERR_AVG_OVER_DOSE_WARN},
		// O/D based on Daily Dose.
		{0,							ERR_OVER_DOSE_WARRNING,		ERR_OVER_DOSE_WARRNING,			NO_PRINT,	ERR_OVER_DOSE_WARRNING},
		// O/D based on Daily Dose double the limit.
		{0,							ERR_DOUBLE_DAILY_OD_WARN,	ERR_OD_INTERACT_NEEDS_REVIEW,	PRINT,		ERR_DOUBLE_DAILY_OD_WARN}
	}
};


static int T2003_total		= 0;
static int T2003_OK			= 0;
static int T2003_added		= 0;
static int T2003_on_time	= 0;
static int T2003_late		= 0;
static int T2003_otc		= 0;
static int T2003_bad_PrID	= 0;
static int T2003_zero_PrID	= 0;

#define REPORT_STATISTICS	0
#define MATCHED_ON_TIME		1
#define	MATCHED_LATE		2
#define LATE				3
#define LATE_BAD_PR_ID		4

#define GADGET_NONE		0
#define GADGET_RIGHT	1
#define GADGET_LEFT		2


/*=======================================================================
||																		||
||				presc_err_comp()										||
||																		||
 =======================================================================*/
int	presc_err_comp (const void *p1, const void *p2)
{
	long		d1;
	long		d2;
	int			s1;
	int			s2;
	long		l1;	// Largo Codes - DonR 22Feb2010.
	long		l2;	// Largo Codes - DonR 22Feb2010.
    PRESC_ERR	*pe1 = (PRESC_ERR*)p1;
    PRESC_ERR	*pe2 = (PRESC_ERR*)p2;


    // 1. sort by check_type : D.U.R - OVERDOSE
    if (pe1->check_type > pe2->check_type)
    {
		return 1;
    }

    if (pe1->check_type < pe2->check_type)
    {
		return -1;
    }


	// DonR 11Jun2009: Sorting requirements are different for overdose: it's not relevant
	// which doctor prescribed the medication; and we want to print the current sale first,
	// followed by previous sales in chronological order from oldest to newest.
	if (pe1->check_type == DUR_CHECK)
	{
		// 2. sort by error severity
		s1 = ((GET_ERROR_SEVERITY (pe1->error_code)	>= FATAL_ERROR_LIMIT ) &&
			  (pe1->doctor_id						!= pe1->sec_doctor_id))			? 0 : 1;

		s2 = ((GET_ERROR_SEVERITY (pe2->error_code)	>= FATAL_ERROR_LIMIT ) &&
			  (pe2->doctor_id						!= pe2->sec_doctor_id))			? 0 : 1;

		if (s1 > s2)
		{
			return 1;
		}

		if (s1 < s2)
		{
			return -1;
		}

		// 3. sort by destination - same doctor or not
		s1 = pe1->doctor_id != pe1->sec_doctor_id ? 0 : 1;
		s2 = pe2->doctor_id != pe2->sec_doctor_id ? 0 : 1;

		if (s1 > s2)
		{
			return 1;
		}

		if (s1 < s2)
		{
			return -1;
		}

		// 4. Sort by sale date, DESCENDING. This allows us to print only the most recent sale
		// of any particular medication, since - unlike for overdoses - there is no reason to
		// print repeat sales of the same interacting medication. (DonR 16Jun2009)
		if (pe1->date < pe2->date)	// Note "backwards" comparison for descending sort!
		{
			return 1;
		}

		if (pe1->date > pe2->date)	// Note "backwards" comparison for descending sort!
		{
			return -1;
		}

	}	// Sorting for Interactions.


	else
	// DonR 11Jun2009: New sorting method for Overdose.
	// DonR 22Feb2010: Add sort by Largo Code, so each drug in current sale comes at the head of
	// a list of other sales of the same drug.
	{
		// 2. Sort by error severity - this shouldn't actually do anything for Overdose.
		s1 = (GET_ERROR_SEVERITY (pe1->error_code) >= FATAL_ERROR_LIMIT) ? 0 : 1;
		s2 = (GET_ERROR_SEVERITY (pe2->error_code) >= FATAL_ERROR_LIMIT) ? 0 : 1;

		if (s1 > s2)
		{
			return 1;
		}

		if (s1 < s2)
		{
			return -1;
		}

		// 3. Sort by Largo Code (added 22Feb2010).
		l1 = pe1->largo_code;
		l2 = pe2->largo_code;

		if (l1 > l2)
		{
			return 1;
		}

		if (l1 < l2)
		{
			return -1;
		}

		// 4. Sort by date of sale.
		// If the two prescription ID's in a presc_drug_inter row are the same, the row
		// corresponds to the current sale, and should be printed first. Thus, set its
		// date (for sorting purposes) to zero. Otherwise the row represents a previous
		// sale, and its date will be left alone. Thus even a previous sale earlier on
		// the same day as the current sale will be printed after the current sale.
		d1 = (pe1->prescription_id == pe1->presc_id_inter) ? 0 : pe1->date;
		d2 = (pe2->prescription_id == pe2->presc_id_inter) ? 0 : pe2->date;

		if (d1 > d2)
		{
			return 1;
		}

		if (d1 < d2)
		{
			return -1;
		}

	}

    return 0;
}



/*=======================================================================
||																		||
||		           HandlerToMsg_1001									||
||		Message handler for message 1001 open shift.					||
||																		||
 =======================================================================*/
int   HandlerToMsg_1001( char		*IBuffer,
			 int		TotalInputLen,
			 char		*OBuffer,
			 int		*p_outputWritten,
			 int		*output_type_flg,
			 SSMD_DATA	*ssmd_data_ptr )
{
    return GenHandlerToMsg_1001( IBuffer,
				 TotalInputLen,
				 OBuffer,
				 p_outputWritten,
				 output_type_flg,
				 ssmd_data_ptr,
				 0,
				 0);
}

/*=======================================================================
||									||
||		           HandlerToMsg_1001				||
||		Message handler for message 1001 open shift.		||
||									||
 =======================================================================*/
int   HandlerToSpool_1001( char		*IBuffer,
			 int		TotalInputLen,
			 char		*OBuffer,
			 int		*p_outputWritten,
			 int		*output_type_flg,
			 SSMD_DATA	*ssmd_data_ptr,
			 short int	commerr_flag)
{
    return GenHandlerToMsg_1001( IBuffer,
				 TotalInputLen,
				 OBuffer,
				 p_outputWritten,
				 output_type_flg,
				 ssmd_data_ptr,
				 1,
				 commerr_flag);
}



/*=======================================================================
||																		||
||		           HandlerToMsg_1001									||
||		Message handler for message 1001 open shift.					||
||			      REALTIME mode											||
||																		||
 =======================================================================*/
int   GenHandlerToMsg_1001( char		*IBuffer,
			 int		TotalInputLen,
			 char		*OBuffer,
			 int		*p_outputWritten,
			 int		*output_type_flg,
			 SSMD_DATA	*ssmd_data_ptr,
			 int		fromspool,
			 int		commerr_flag)
{
	/*=======================================================================
	||			    Local declarations										||
	 =======================================================================*/
    int				tries;
	int				reStart;
	int				i;
    PHARMACY_INFO	Phrm_info;
    char			*non_empty_msg_lines[5];
	ISOLATION_VAR;

	/*=======================================================================
	||			Message fields variables									||
	 =======================================================================*/
	int								v_PharmNum;
	short							v_InstituteCode;
	short							v_InstituteCodeSql;
	short							v_TerminalNum;
	short							v_PriceListNum;
	short							v_PriceListNumSql;
	int								v_PriceListUpdateDate;
	int								v_MaccabiPriceListUpdateDate;
	int								v_PurchasePriceListUpdateDate;
	int								v_DrugBookUpdateDate;
	int								v_UserIdentification;
	char							v_SoftwareVersion_pc	[ 9 + 1];
	int								v_HardwareVersion;
	short							hesder_category_in;
	short							hesder_category_db;
	short							v_CommunicationType;
	int								v_DiscountListUpdateDate;
	int								v_SuppliersListUpdateDate;
	int								v_InstituteListUpdateDate;
	int								v_ErrorCodeListUpdateDate;
	int								v_AvailableDisk;
	short							v_Net;
	int								v_FlushingFileSize;
	int								v_LastBackupDate;
	short							v_FreeMemory;
	int								v_Date;
	int								v_Hour;
	int								v_fps_fee_level;	// DonR 14APR2003 Fee-Per-Service.
	int								v_fps_fee_lower;	// DonR 14APR2003 Fee-Per-Service.
	int								v_fps_fee_upper;	// DonR 14APR2003 Fee-Per-Service.
	short							v_maccabicare_flag;	// DonR 12Jun2005 MaccabiCare

	// Declarations of variables appearing in return message ONLY:
	short							v_ErrorCode;
	short							v_Comm_Error;
	char							v_Phone1		[10 + 1];
	char							v_Phone2		[10 + 1];
	char							v_Phone3		[10 + 1];
	int								v_DateSql;
	int								v_HourSql;
	short							v_Message;
	short							v_GlobalPricelistUpdate;
	short							v_MaccabiPricelistUpdate;
	short							v_DrugBookUpdate;
	short							v_DiscountListUpdate;
	short							v_SuppliersListUpdate;
	short							v_InstituteListUpdate;
	short							v_ErrorCodeListUpdate;
	short			       			v_Reserve1;
	short							v_OpenMessageLenghtInLines;
	char					 		v_OpenShiftMessage_1	[60 + 1];
	char							v_OpenShiftMessage_2	[60 + 1];
	char							v_OpenShiftMessage_3	[60 + 1];
	char							v_OpenShiftMessage_4	[60 + 1];
	char						 	v_OpenShiftMessage_5	[60 + 1];
	TSqlInd							v_Msgind1;
	TSqlInd							v_Msgind2;
	TSqlInd							v_Msgind3;
	TSqlInd							v_Msgind4;
	TSqlInd							v_Msgind5;
	char							v_SoftwareVersion_need	[ 9 + 1];
	short							v_DeleteFlage;
	int								v_MessageNum;
	short							v_PermissionType;
	short							comm_err_code;	// DonR 09Mar2020: Was an int, but it's a short in the DB and the SQL operation.
	short							from_spool;		// DonR 09Mar2020: Was an int, but it's a short in the DB and the SQL operation.


	/*=======================================================================
	||																		||
	||				Body of function										||
	||																		||
	||======================================================================||
	||			  Init answer variables										||
	=======================================================================*/
	REMEMBER_ISOLATION;
    PosInBuff					= IBuffer;
    v_ErrorCode					= NO_ERROR;

    v_Phone1[0]					= 0;
    v_Phone2[0]					= 0;
    v_Phone3[0]					= 0;
    v_DateSql					= GetDate();
    v_HourSql					= GetTime();
    v_Message					= MAC_FALS;
    v_GlobalPricelistUpdate		= MAC_FALS;
    v_MaccabiPricelistUpdate	= MAC_FALS;
    v_DrugBookUpdate			= MAC_FALS;
    v_DiscountListUpdate		= MAC_FALS;
    v_SuppliersListUpdate		= MAC_FALS;
    v_InstituteListUpdate		= MAC_FALS;
    v_ErrorCodeListUpdate		= MAC_FALS;
    v_Reserve1					= 0;
    v_OpenMessageLenghtInLines	= 0;
    v_OpenShiftMessage_1[0]		= 0;
    v_OpenShiftMessage_2[0]		= 0;
    v_OpenShiftMessage_3[0]		= 0;
    v_OpenShiftMessage_4[0]		= 0;
    v_OpenShiftMessage_5[0]		= 0;
    v_SoftwareVersion_need[0]	= 0;
    
	/*=======================================================================
	||																		||
	||		      Get message fields data into variables					||
	||																		||
	 =======================================================================*/
	v_PharmNum						= GetLong	(&PosInBuff, TPharmNum_len						);	CHECK_ERROR();	//  10,  7
	v_InstituteCode					= GetShort	(&PosInBuff, TInstituteCode_len					);	CHECK_ERROR();	//  17,  2
	v_TerminalNum					= GetShort	(&PosInBuff, TTerminalNum_len					);	CHECK_ERROR();	//  19,  2
	v_PriceListNum					= GetShort	(&PosInBuff, TPriceListNum_len					);	CHECK_ERROR();	//  21,  3
	v_PriceListUpdateDate			= GetLong	(&PosInBuff, TPriceListUpdateDate_len			);	CHECK_ERROR();	//  24,  8
	v_MaccabiPriceListUpdateDate	= GetLong	(&PosInBuff, TMaccabiPriceListUpdateDate_len	);	CHECK_ERROR();	//  32,  8
	v_PurchasePriceListUpdateDate	= GetLong	(&PosInBuff, TPurchasePriceListUpdateDate_len	);	CHECK_ERROR();	//  40,  8
	v_DrugBookUpdateDate			= GetLong	(&PosInBuff, TDrugBookUpdateDate_len			);	CHECK_ERROR();	//  48,  8
	v_UserIdentification			= GetLong	(&PosInBuff, TUserIdentification_len			);	CHECK_ERROR();	//  56,  9

	GetString (&PosInBuff, v_SoftwareVersion_pc,	TSoftwareVersion_len);	CHECK_ERROR();							//  65,  9

	v_HardwareVersion				= GetLong	(&PosInBuff, THardwareVersion_len				);	CHECK_ERROR();	//  74,  9
	hesder_category_in				= GetShort	(&PosInBuff, 1									);	CHECK_ERROR();	//  83,  1
	v_CommunicationType				= GetShort	(&PosInBuff, TCommunicationType_len				);	CHECK_ERROR();
	v_DiscountListUpdateDate		= GetLong	(&PosInBuff, TDiscountListUpdateDate_len		);	CHECK_ERROR();
	v_SuppliersListUpdateDate		= GetLong	(&PosInBuff, TSuppliersListUpdateDate_len		);	CHECK_ERROR();
	v_InstituteListUpdateDate		= GetLong	(&PosInBuff, TInstituteListUpdateDate_len		);	CHECK_ERROR();
	v_ErrorCodeListUpdateDate		= GetLong	(&PosInBuff, TErrorCodeListUpdateDate_len		);	CHECK_ERROR();
	v_AvailableDisk					= GetLong	(&PosInBuff, TAvailableDisk_len					);	CHECK_ERROR();
	v_Net							= GetShort	(&PosInBuff, TNet_len							);	CHECK_ERROR();
	v_FlushingFileSize				= GetLong	(&PosInBuff, TFlushingFileSize_len				);	CHECK_ERROR();
	v_LastBackupDate				= GetLong	(&PosInBuff, TLastBackupDate_len				);	CHECK_ERROR();
	v_FreeMemory					= GetShort	(&PosInBuff, TFreeMemory_len					);	CHECK_ERROR();
	v_Date							= GetLong	(&PosInBuff, TDate_len							);	CHECK_ERROR();
	v_Hour							= GetLong	(&PosInBuff, THour_len							);	CHECK_ERROR();

	// Get amount of input not eaten
    ChckInputLen( TotalInputLen - (PosInBuff - IBuffer) );
    CHECK_ERROR();

	ChckDate (v_PriceListUpdateDate			);	CHECK_ERROR();
	ChckDate (v_MaccabiPriceListUpdateDate	);	CHECK_ERROR();
	ChckDate (v_DrugBookUpdateDate			);	CHECK_ERROR();
	ChckDate (v_DiscountListUpdateDate		);	CHECK_ERROR();
	ChckDate (v_SuppliersListUpdateDate		);	CHECK_ERROR();
	ChckDate (v_InstituteListUpdateDate		);	CHECK_ERROR();
	ChckDate (v_ErrorCodeListUpdateDate		);	CHECK_ERROR();
	ChckDate (v_Date						);	CHECK_ERROR();
	ChckTime (v_Hour						);	CHECK_ERROR();
 
	ssmd_data_ptr->pharmacy_num = v_PharmNum;

	/*=======================================================================
	||																		||
	||			    SQL TRANSACTIONs.										||
	||																		||
	||======================================================================||
	||			     Retries loop											||
	 =======================================================================*/

    reStart = MAC_TRUE;

    for (tries = 0; tries < SQL_UPDATE_RETRIES && reStart == MAC_TRUE; tries++)
    {
		/*=======================================================================
		||			  Init answer variables										||
		 =======================================================================*/
		reStart						= MAC_FALS;
		v_ErrorCode					= NO_ERROR;
		v_Phone1[0]					= 0;
		v_Phone2[0]					= 0;
		v_Phone3[0]					= 0;
		v_DateSql					= GetDate();
		v_HourSql					= GetTime();
		v_Message					= MAC_FALS;
		v_GlobalPricelistUpdate		= MAC_FALS;
		v_MaccabiPricelistUpdate	= MAC_FALS;
		v_DrugBookUpdate			= MAC_FALS;
		v_DiscountListUpdate		= MAC_FALS;
		v_SuppliersListUpdate		= MAC_FALS;
		v_InstituteListUpdate		= MAC_FALS;
		v_ErrorCodeListUpdate		= MAC_FALS;
		v_Reserve1					= 4; //25.07.2000
		v_OpenShiftMessage_1[0]		= 0;
		v_OpenShiftMessage_2[0]		= 0;
		v_OpenShiftMessage_3[0]		= 0;
		v_OpenShiftMessage_4[0]		= 0;
		v_OpenShiftMessage_5[0]		= 0;
		v_SoftwareVersion_need[0]	= 0;
		hesder_category_db			= 0;


		/*=======================================================================
		||																		||
		||		    TESTING INPUT AND PREPARE OUTPUT DATA						||
		||																		||
		||======================================================================||
		||		    Testing pharmacy code, institute code						||
		 =======================================================================*/

		// DonR 14Jul2005: Avoid unnecessary locks reading from static tables.
		SET_ISOLATION_DIRTY;

		do				/* Exiting from LOOP will send	*/
		{				/* the reply to phrmacy	*/
			ExecSQL (	MAIN_DB, TR1001_READ_Pharmacy,
						&v_Phone1,				&v_Phone2,				&v_Phone3,					&v_InstituteCodeSql,		&v_PriceListNumSql,
						&hesder_category_db,	&v_PermissionType,		&v_SoftwareVersion_need,	&v_OpenShiftMessage_1,		&v_OpenShiftMessage_2,
						&v_OpenShiftMessage_3,	&v_OpenShiftMessage_4,	&v_OpenShiftMessage_5,		&v_fps_fee_level,			&v_fps_fee_lower,
						&v_fps_fee_upper,		&v_maccabicare_flag,
						&v_PharmNum,			&ROW_NOT_DELETED,		END_OF_ARG_LIST																	);

			// Copy length-read values into indicator variables for Open Shift Messages, which allow NULL values.
			v_Msgind1 = ColumnOutputLengths [ 9];
			v_Msgind2 = ColumnOutputLengths [10];
			v_Msgind3 = ColumnOutputLengths [11];
			v_Msgind4 = ColumnOutputLengths [12];
			v_Msgind5 = ColumnOutputLengths [13];

			Conflict_Test( reStart );

			if( SQLERR_code_cmp( SQLERR_not_found ) == MAC_TRUE )
			{
				strcpy(v_Phone1, "-" );
				strcpy(v_Phone2, "-" );
				strcpy(v_Phone3, "-" );
				strcpy(v_SoftwareVersion_need, "-" );
				hesder_category_db = 0;

				if( SetErrorVar( &v_ErrorCode, ERR_PHARMACY_NOT_FOUND ) )
				{
					break;
				}
			}
			else if( SQLERR_error_test() )
			{
				if( SetErrorVar( &v_ErrorCode, ERR_DATABASE_ERROR ) )
				{
					break;
				}
			}

			if( v_InstituteCodeSql != v_InstituteCode )
			{
				if( SetErrorVar( &v_ErrorCode, ERR_INSTITUTE_CODE_WRONG ) )
				{
					break;
				}
			}
			
			if( v_PriceListNumSql  != v_PriceListNum )
			{
				v_MaccabiPricelistUpdate = MAC_TRUE;

				if( SetErrorVar( &v_ErrorCode, ERR_PRICE_CODE_WRONG ) )
				{
					break; 
				}
			}
			
			if (hesder_category_in != hesder_category_db)
			{
				if (SetErrorVar (&v_ErrorCode, ERR_PHARMACY_TYPE_WRONG))
				{
					break; 
				}
			}
		
			// Get non-empty message lines:
			i = 0;
		
			if ((!SQLMD_is_null (v_Msgind1)) && (NOT_EMPTY_STRING (v_OpenShiftMessage_1)))
			{
				non_empty_msg_lines[i++] = v_OpenShiftMessage_1;
			}
			if ((!SQLMD_is_null (v_Msgind2)) && (NOT_EMPTY_STRING (v_OpenShiftMessage_2)))
			{
				non_empty_msg_lines[i++] = v_OpenShiftMessage_2;
			}
			if ((!SQLMD_is_null (v_Msgind3)) && (NOT_EMPTY_STRING (v_OpenShiftMessage_3)))
			{
				non_empty_msg_lines[i++] = v_OpenShiftMessage_3;
			}
			if ((!SQLMD_is_null (v_Msgind4)) && (NOT_EMPTY_STRING (v_OpenShiftMessage_4)))
			{
				non_empty_msg_lines[i++] = v_OpenShiftMessage_4;
			}
			if ((!SQLMD_is_null (v_Msgind5)) && (NOT_EMPTY_STRING (v_OpenShiftMessage_5)))
			{
				non_empty_msg_lines[i++] = v_OpenShiftMessage_5;
			}
			
			v_OpenMessageLenghtInLines = i; /* == num of non-empty lines */
	    
			/*=======================================================================
			||			Testing pharmacy messages									||
			 =======================================================================*/

			v_Message = MAC_TRUE;

			ExecSQL (	MAIN_DB, READ_pharmacy_message_undelivered, &v_MessageNum, &v_PharmNum, END_OF_ARG_LIST	);

			Conflict_Test (reStart);

			if (SQLERR_code_cmp (SQLERR_not_found) == MAC_TRUE)
			{
				v_Message = MAC_FALS;
			}
			else
			{
				if ((SQLERR_code_cmp (SQLERR_too_many_result_rows) == MAC_FALS) && (SQLERR_error_test ()))
				{
					v_Message = MAC_FALS;
					if (SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR))
					{
						break;
					}
				}
			}

			/*=======================================================================
			||		      Prepare tables update flages.								||
			 =======================================================================*/
			v_GlobalPricelistUpdate  = GetTblUpFlg (v_PriceListUpdateDate, "Price_list");

			if (v_MaccabiPricelistUpdate != MAC_TRUE)
			{
				v_MaccabiPricelistUpdate =	GetTblUpFlg (v_MaccabiPriceListUpdateDate,
														 "Price_list_macabi")
											||	/* Logically OR-ing the two flags */
											GetTblUpFlg (v_PurchasePriceListUpdateDate,
														 "Price_list_macabi");
			}

			v_DrugBookUpdate	     = GetTblUpFlg (v_DrugBookUpdateDate,		"Drug_list");
			v_DiscountListUpdate     = GetTblUpFlg (v_DiscountListUpdateDate,	"Member_price");
			v_SuppliersListUpdate    = GetTblUpFlg (v_SuppliersListUpdateDate,	"Suppliers");
			v_InstituteListUpdate    = GetTblUpFlg (v_InstituteListUpdateDate,	"Macabi_centers");
			v_ErrorCodeListUpdate    = GetTblUpFlg (v_ErrorCodeListUpdateDate,	"Pc_error_message");

			/*=======================================================================
			||																		||
			||			    UPDATE TABLES.											||
			||																		||
			||======================================================================||
			||			 Update PHARMACY table.										||
			 =======================================================================*/

			// DonR 14Jul2005: Restore default DB isolation.
			SET_ISOLATION_COMMITTED;

			ExecSQL (	MAIN_DB, UPD_SetPharmacyOpenStatus, &PHARM_OPEN_SQL, &v_PharmNum, &ROW_NOT_DELETED, END_OF_ARG_LIST	);

			Conflict_Test (reStart);

			if (SQLERR_error_test ())
			{
				SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR);
			}

		} while (0);		// Loop should run only once.


		/*=======================================================================
		||		    Insert into PHARMACY_LOG table.								||
		 =======================================================================*/
		if (reStart == MAC_FALS)	/* Dont write retries to log 	*/
		{
			do
			{
				from_spool = fromspool ? 2 : 1;
				comm_err_code = commerr_flag;

				ExecSQL (	MAIN_DB, TR1001_INS_pharmacy_log,
							&v_PharmNum,				&v_InstituteCode,			&v_TerminalNum,					&v_ErrorCode,					&PHARM_OPEN_SQL,
							&v_PriceListNum,			&v_PriceListUpdateDate,		&v_MaccabiPriceListUpdateDate,	&v_PurchasePriceListUpdateDate,	&v_DrugBookUpdateDate,
							&v_SoftwareVersion_pc,		&v_HardwareVersion,			&hesder_category_db,			&v_CommunicationType,			&v_Phone1,
							&v_Phone2,					&v_Phone3,					&v_Message,						&v_GlobalPricelistUpdate,		&v_MaccabiPricelistUpdate,

							&v_DrugBookUpdate,			&v_SoftwareVersion_need,	&v_DiscountListUpdateDate,		&v_SuppliersListUpdateDate,		&v_InstituteListUpdateDate,
							&v_ErrorCodeListUpdateDate,	&v_AvailableDisk,			&v_Net,							&v_FlushingFileSize,			&v_LastBackupDate,
							&v_FreeMemory,				&v_Date,					&v_Hour,						&v_DateSql,						&v_HourSql,
							&v_UserIdentification,		&v_ErrorCodeListUpdate,		&v_DiscountListUpdate,			&v_SuppliersListUpdate,			&v_InstituteListUpdate,

							&from_spool,				&comm_err_code,				&NOT_REPORTED_TO_AS400,			END_OF_ARG_LIST												);

				Conflict_Test (reStart);

				if (SQLERR_error_test ())
				{
					SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR);
				}

			} while (0);	// Loop should run only once.

		}	// Not on a retry.

		/*=======================================================================
		||			  Commit the transaction.									||
		 =======================================================================*/
		if( reStart == MAC_FALS )	/* Tranaction OK then COMMIT	*/
		{
			CommitAllWork ();

			if( SQLERR_error_test() )	/* Could not COMMIT		*/
			{
			SetErrorVar( &v_ErrorCode, ERR_DATABASE_ERROR );
			}

			if( ! SetErrorVar( &v_ErrorCode, v_ErrorCode ) )	// No fatal error so far.
			{
				Phrm_info.pharmacy_permission = v_PermissionType;
				Phrm_info.price_list_num      = v_PriceListNumSql;
				Phrm_info.open_close_flg      = MAC_TRUE;
			}
		}
		else
		{
			if( TransactionRestart() != MAC_OK )
			{
			v_ErrorCode = ERR_DATABASE_ERROR;
			break;
			}

			// DonR 28Dec2020: Increased threshold for logging this message from > 0 to > 2.
			if (tries > 2)
				GerrLogReturn (GerrId, "Table is locked for the <%d> time", tries);

			sleep( ACCESS_CONFLICT_SLEEP_TIME );
		}

    }	/* End of retries loop */

    if( reStart == MAC_TRUE )
    {
		GerrLogReturn(
				  GerrId,
				  "Locked for <%d> times",
				  SQL_UPDATE_RETRIES
				  );

		SetErrorVar( &v_ErrorCode, ERR_DATABASE_ERROR );
    }


	/*=======================================================================
	||																		||
	||			  WRITE RESPONSE MESSAGE.									||
	||																		||
	 =======================================================================*/
    nOut =  sprintf (OBuffer,			"%0*d",		MSG_ID_LEN,				1002			);
    nOut += sprintf (OBuffer + nOut,	"%0*d",		MSG_ERROR_CODE_LEN,		MAC_OK			);
    nOut += sprintf (OBuffer + nOut,	"%0*d",		TPharmNum_len,			v_PharmNum		);
    nOut += sprintf (OBuffer + nOut,	"%0*d",		TInstituteCode_len,		v_InstituteCode	);
    nOut += sprintf (OBuffer + nOut,	"%0*d",		TTerminalNum_len,		v_TerminalNum	);
    nOut += sprintf (OBuffer + nOut,	"%0*d",		TErrorCode_len,			v_ErrorCode		);
    nOut += sprintf (OBuffer + nOut,	"%*d",		TDate_len,				v_DateSql		);
    nOut += sprintf (OBuffer + nOut,	"%*d" ,		THour_len,				v_HourSql		);

	// DonR 14APR2003 Fee-Per-Service Begin:
	// Note that these three values are being put in place of the phone numbers -
	// and that's why I've kept the phone-number length parameters (== 10).
    nOut += sprintf (OBuffer + nOut,	"%0*d",		TPhone1_len,			v_fps_fee_level	);
    nOut += sprintf (OBuffer + nOut,	"%0*d",		TPhone2_len,			v_fps_fee_lower	);
    nOut += sprintf (OBuffer + nOut,	"%0*d",		TPhone3_len,			v_fps_fee_upper	);
	// DonR 14APR2003 Fee-Per-Service End.

    nOut += sprintf (OBuffer + nOut,	"%0*d",		TMessage_len,			v_Message		);

    nOut += sprintf (OBuffer + nOut,	"%0*d",		TGlobalPricelistUpdate_len,
																			v_GlobalPricelistUpdate);

    nOut += sprintf (OBuffer + nOut,	"%0*d",		TMaccabiPricelistUpdate_len,
																			v_MaccabiPricelistUpdate);

	nOut += sprintf (OBuffer + nOut,	"%0*d",		TErrorCodeListUpdate_len,
																			v_ErrorCodeListUpdate);

	nOut += sprintf (OBuffer + nOut,	"%0*d",		TDrugBookUpdate_len,	v_DrugBookUpdate);

	nOut += sprintf (OBuffer + nOut,	"%-*.*s",	TSoftwareVersion_len,
													TSoftwareVersion_len,	v_SoftwareVersion_need);

	nOut += sprintf (OBuffer + nOut,	"%0*d",		TDiscountListUpdate_len,v_DiscountListUpdate);

	nOut += sprintf (OBuffer + nOut,	"%0*d",		TSuppliersListUpdate_len,
																			v_SuppliersListUpdate);

	nOut += sprintf (OBuffer + nOut,	"%0*d",		TInstituteListUpdate_len,
																			v_InstituteListUpdate);

	nOut += sprintf (OBuffer + nOut,	"%0*d",		TReserve1_len,			v_Reserve1		);
	nOut += sprintf (OBuffer + nOut,	"%0*d",		2,						hesder_category_in);	// DonR 09Jan2019: Renamed Owner to hesder_category (for Mersham Pituach)
	nOut += sprintf (OBuffer + nOut,	"%0*d",		2,						v_maccabicare_flag);	// DonR 12Jun2005

	nOut += sprintf (OBuffer + nOut,	"%0*d",		TOpenMessageLenghtInLines_len,
																			v_OpenMessageLenghtInLines);

    /*
     *  write non-empty message lines into OBuffer: 
     */
    for (i = 0; i < v_OpenMessageLenghtInLines; i++)
    {
		WinHebToDosHeb ((unsigned char *)non_empty_msg_lines[i]);
	
		nOut += sprintf( OBuffer + nOut, "%-*.*s",
				 TOpenShiftMessage_len,	TOpenShiftMessage_len,
				 non_empty_msg_lines[i] );
    }
    
    /*
     * Return the size in Bytes of respond message
     * -------------------------------------------
     */
    *p_outputWritten = nOut;
    *output_type_flg = ANSWER_IN_BUFFER;
    ssmd_data_ptr->error_code = v_ErrorCode;
    ssmd_data_ptr->pharmacy_num = v_PharmNum;
    ssmd_data_ptr->terminal_num = v_TerminalNum;

	RESTORE_ISOLATION;

    return  RC_SUCCESS;

}
/* End of 1001 handler */



/*=======================================================================
||																		||
||			 DownloadDrugList											||
||	    Message handler for message 1011 drug-list update.				||
||		DonR 05Nov2017: Added Trn. 2094 drug-list supplemental fields.	||
||																		||
||																		||
=======================================================================*/
int DownloadDrugList	(	char		*IBuffer,
							int			TotalInputLen,
							char		*OBuffer,
							int			*p_outputWritten,
							int			*output_type_flg,
							SSMD_DATA	*ssmd_data_ptr,
							int			MessageIndex_in)
{
	// Local declarations
	char			fileName[265];
	int				tries;
	int				reStart;
	FILE			*outFP;
	time_t			StartTime = time(NULL);
	ISOLATION_VAR;

//	// Unremark stuff below to enable diagnostics.
//	#define PrintTime(x) //{ printf("%d : %s\n", time(NULL) - StartTime, x ); fflush(stdout); }


	// SQL variables

	// INPUT MESSAGE
	PHARMACY_INFO					Phrm_info;
	TPharmNum						v_PharmNum;
	TInstituteCode					v_InstituteCode;
	TTerminalNum					v_TerminalNum;
	TLastDrugUpdateDate				v_LastDrugUpdateDate;
	TLastDrugUpdateHour				v_LastDrugUpdateHour;

	// RETURN MESSAGE
	// header
	TErrorCode						v_ErrorCode;
	TDrugFileUpdateDate				v_DrugFileUpdateDate;
	TDrugFileUpdateHour				v_DrugFileUpdateHour;
	TDrugFileUpdateDate				v_ThisUpdateDate;
	TDrugFileUpdateHour				v_ThisUpdateHour;
	TMaxNumOfDrugsToUpdate			v_MaxNumOfDrugsToUpdate;

	// lines
	TDrugCode						v_DrugCode;
	TUpdateCode						v_UpdateCode;
	short							v_DelFlg;
	short							v_AssutaDrugStatus;
	TDrugDescription				v_DrugDescription;
	TPackageSize					v_PackageSize;
	short							calc_op_by_volume;	// DonR 05Mar2018 CR #14502.
	double							package_volume;
	long							pkg_volume_int;
	TParticipatingType				v_ParticipatingType;
	TComputersoftDrugCode			v_ComputersoftDrugCode;
	TAnotherParticipatingTypeExist	v_OtherParticTypExists;
	TSqlInd							v_AnoInd;
	TRegisteredInDrugNote			v_RegisteredInDrugNote;
	TSqlInd							v_DruInd;
	TSaleWithoutPrescription		v_SaleWithoutPresc;
//	TDrugAddition1Code				v_DrugAddition1Code;
	TDrugAddition3Code				v_FPS_Group_Code;
	short							v_ExpiryDateFlag;
	short							v_preferred_flg;
	short							v_openable_pkg;
	short							v_maccabicare_flag;
	short							v_in_gadget_table;
	short							v_filler = 0;
	short							ShapeCode;
	short							ShapeCodeLen;
	char							v_largo_type [2];
	short							how_to_take_code;
	char							unit_abbreviation	[ 3 + 1];
	char							per_1_units			[ 3 + 1];	// DonR 05Mar2018 CR #14502.
	char							bar_code_value		[14 + 1];
	short							ingr_1_code;
	double							ingr_1_quant;
	char							ingr_1_units		[ 3 + 1];
	double							per_1_quant;
	long							ingr_1_quant_int;
	long							per_1_quant_int;
	char							drug_type			[ 1 + 1];	// DonR 05May2019 CR #14710.
	short							needs_refrigeration;			// Marianna 05Jan2022 : Feature 217431.
	short							needs_dilution;					// Marianna 05Jan2022 : Feature 217431.
	short							ConsignmentItem;				// DonR 16Apr2023 User Story #432608.
	short							is_narcotic;					// DonR 06Nov2025 from drug_list.
	short							psychotherapy_drug;				// DonR 06Nov2025 from drug_list.
	short							preparation_type;				// DonR 22Dec2025 from drug_list.
	short							is_opioid;						// DonR 06Nov2025 based on drug_type == 'N'.
	short							is_ADHD;						// DonR 06Nov2025 based on drug_type == 'T'.
	short							is_preparation;					// DonR 22Dec2025 based on preparation_type	>  0.

	// Misc.
	int								NowDate;
	int								NowTime;


	REMEMBER_ISOLATION;

//	PrintTime("Handler Entry");

	NowDate = GetDate ();
	NowTime = GetTime ();


	// Body of function

	// Init answer header variables
	PosInBuff				= IBuffer;
	v_ErrorCode				= NO_ERROR;
	v_DrugFileUpdateDate	= 0;
	v_DrugFileUpdateHour	= 0;
	v_MaxNumOfDrugsToUpdate	= 0;


	// Read request message fields.
	v_PharmNum				= GetLong	(&PosInBuff, TPharmNum_len			); CHECK_ERROR();	// 10, 7
	v_InstituteCode			= GetShort	(&PosInBuff, TInstituteCode_len		); CHECK_ERROR();	// 17, 2
	v_TerminalNum			= GetShort	(&PosInBuff, TTerminalNum_len		); CHECK_ERROR();	// 19, 2
	v_LastDrugUpdateDate	= GetLong	(&PosInBuff, TLastDrugUpdateDate_len); CHECK_ERROR();	// 21, 8
	v_LastDrugUpdateHour	= GetLong	(&PosInBuff, TLastDrugUpdateHour_len); CHECK_ERROR();	// 29, 6

	ChckDate (v_LastDrugUpdateDate); CHECK_ERROR();
//	ChckTime (v_LastDrugUpdateHour); CHECK_ERROR();	DonR 05Jul2004: Causes problems, as we
//													generate fake times when getting data
//													from the AS400. Some of these fake times
//													are "illegal".

	// DonR 22Nov2020 CR #30978: If the pharmacy has sent a Version Number, read it from the
	// input message; but if we don't have at least 2 characters left to read, assume that
	// the pharmacy is still using Version 1 of the transaction, which doesn't have a Version
	// Number included in the request.
	if ((TotalInputLen - (PosInBuff - IBuffer)) > 1)
	{
		VersionNumber		= GetShort	(&PosInBuff,  2					); CHECK_ERROR ();		// 35, 2
	}
	else
	{
		VersionNumber		= 1;
	}

	// Get amount of input not eaten
	ChckInputLen (TotalInputLen - (PosInBuff - IBuffer));
	CHECK_ERROR();



	// Get ready to process the request.
	reStart = MAC_TRUE;

	// DonR 22Nov2020 CR #30978: For Version Number > 1, send 3 bytes of Shape Code rather than 2.
	ShapeCodeLen	= (VersionNumber > 1) ? 3 : 2;

	// DonR 13Aug2024 User Story #349368: Set Largo Code
	// length conditionally on Transaction Version Number.
	VersionLargoLen	= (VersionNumber > 2) ? 9 : 5;

	sprintf (fileName,
			 "%s/drugs%d_%0*d_%0*d_%0*d.snd",
			 MsgDir,
			 MessageIndex_in,
			 TPharmNum_len,
			 v_PharmNum,
			 TInstituteCode_len,
			 v_InstituteCode,
			 TTerminalNum_len,
			 v_TerminalNum);

//	PrintTime("Before retries loop");


	// Retries loop.
	for (tries = 0;	(tries < SQL_UPDATE_RETRIES) && (reStart == MAC_TRUE); tries++)
	{
		// Create new file for response message.
		outFP = fopen( fileName, "w" );

		if( outFP == NULL )
		{
			int	err = errno;

			GerrLogReturn (GerrId,
						   "\tCant open output file '%s'\n\tError (%d) %s\n",
						   fileName,
						   err,
						   strerror(err));

			RESTORE_ISOLATION;
			return (ERR_UNABLE_OPEN_OUTPUT_FILE);
		}

		// Init answer header variables
		v_ErrorCode				= NO_ERROR;
		v_DrugFileUpdateDate	= v_LastDrugUpdateDate;	// DonR 17Aug2005: Default = no new data.
		v_DrugFileUpdateHour	= v_LastDrugUpdateHour;	// DonR 17Aug2005: Default = no new data.
		v_MaxNumOfDrugsToUpdate	= 0;
		reStart					= MAC_FALS;


		// One-time loop to avoid GOTO's. Response is sent upon exit from this "loop".
		do
		{
			// DonR 12Jul2005: Avoid unnecessary table-locked errors.
			SET_ISOLATION_DIRTY;

			// Test if pharmacy has been OPENed.
//			PrintTime("Before IS_PHARMACY_OPEN");

			v_ErrorCode = IS_PHARMACY_OPEN (v_PharmNum, v_InstituteCode, &Phrm_info);
			if (v_ErrorCode != MAC_OK)
			{
				break;
			}


			// Open drugs cursor.
//			PrintTime("Before open Cursor_1011_name");

			// DonR 09Sep2009: Made changes for Assuta Pharmacies and FPS Groups.
			// DonR 13Jul2020: Switched to new combined DECLARE and OPEN operation,
			// to avoid situations where the DECLARE is executed but the OPEN/FETCH/CLOSE
			// is not.
			DeclareAndOpenCursor (	MAIN_DB, TR1011_2094_DrugListDownload_cur,
									&v_LastDrugUpdateDate,	&v_LastDrugUpdateDate,	&v_LastDrugUpdateHour,
									&NowDate,				&NowDate,				&NowTime,
									END_OF_ARG_LIST																);

//			PrintTime("After open Cursor_1011_name");

			if (SQLERR_error_test ())
			{
				SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR);
				break;
			}


			// Create the header with data so far.
			fprintf (outFP, "%0*d",		MSG_ID_LEN,					(MessageIndex_in + 1)	);
			fprintf (outFP, "%0*d",		MSG_ERROR_CODE_LEN,			MAC_OK					);
			fprintf (outFP, "%0*d",		TPharmNum_len,				v_PharmNum				);	//  1
			fprintf (outFP, "%0*d",		TInstituteCode_len,			v_InstituteCode			);	//  2
			fprintf (outFP, "%0*d",		TTerminalNum_len,			v_TerminalNum			);	//  3
			fprintf (outFP, "%0*d",		TErrorCode_len,				v_ErrorCode				);	//  4
			fprintf (outFP, "%0*d",		TDrugFileUpdateDate_len,	v_DrugFileUpdateDate	);	//  5
			fprintf (outFP, "%0*d",		TDrugFileUpdateHour_len,	v_DrugFileUpdateHour	);	//  6
			fprintf (outFP, "%0*d",		TNumDrugsUpdated_len,		v_MaxNumOfDrugsToUpdate	);	//  7


			// Write the drugs data to file.
//			PrintTime("Before Loop on Cursor_1011_name");

			// Loop until we hit end-of-fetch.
			for (;;)
			{
				// Fetch and test DB errors
//				PrintTime("Before Fetch from Cursor_1011_name");

				FetchCursorInto (	MAIN_DB, TR1011_2094_DrugListDownload_cur,
									&v_DrugCode,				&v_UpdateCode,			&v_AssutaDrugStatus,		&v_DrugDescription,
									&v_PackageSize,				&v_ParticipatingType,	&v_ComputersoftDrugCode,	&v_OtherParticTypExists,
									&v_RegisteredInDrugNote,	&v_SaleWithoutPresc,	&drug_type,					&v_FPS_Group_Code,
									&v_preferred_flg,			&v_openable_pkg,		&calc_op_by_volume,			&per_1_units,

									&package_volume,			&v_maccabicare_flag,	&v_in_gadget_table,			&v_largo_type,
									&ShapeCode,					&how_to_take_code,		&unit_abbreviation,			&bar_code_value,
									&ingr_1_code,				&ingr_1_quant,			&ingr_1_units,				&per_1_quant,
									&needs_refrigeration,		&needs_dilution,		&ConsignmentItem,			&is_narcotic,	// DonR 06Nov2025 add is_narcotic/psychotherapy_drug.
									&psychotherapy_drug,		&preparation_type,		&v_ThisUpdateDate,			&v_ThisUpdateHour,
									END_OF_ARG_LIST																								);

//				PrintTime("After Fetch from Cursor_1011_name");

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

				// DonR 17Aug2005: Set the returned as-of timestamp conditionally.
				if (v_ThisUpdateDate > v_DrugFileUpdateDate)
				{
					v_DrugFileUpdateDate = v_ThisUpdateDate;
					v_DrugFileUpdateHour = v_ThisUpdateHour;
				}
				else
				{
					if ((v_ThisUpdateDate == v_DrugFileUpdateDate)	&&
						(v_ThisUpdateHour >  v_DrugFileUpdateHour))
					{
						v_DrugFileUpdateHour = v_ThisUpdateHour;
					}
				}

				// DonR 09Sep2009: Depending on whether this is an Assuta pharmacy or not, we
				// use different criteria for determining if the drug is "deleted" (i.e. invisible)
				// or not. Assuta pharmacies can sell some OTC items (e.g. soaps, etc.) that are not
				// visible to other pharmacies.
				switch (Phrm_info.pharm_category)
				{
					case PHARMACY_ASSUTA:
						if (v_UpdateCode == DRUG_DELETED)
						{
							if ((v_AssutaDrugStatus == ASSUTA_DRUG_ACTIVE)	||
								(v_AssutaDrugStatus == ASSUTA_DRUG_PAST))
							{
								v_DelFlg = 0;	// This drug is visible only to Assuta pharmacies.
							}
							else
							{
								v_DelFlg = 1;	// This drug is invisible to all pharmacies.
							}
						}
						else
						{
							v_DelFlg = 0;	// Ordinary, non-deleted item.
						}

						break;

					default:
						// Standard logic - ignore Assuta Drug Status and just go by del_flg value.
						v_DelFlg = ((v_UpdateCode == DRUG_DELETED) ? 1 : 0);
						break;
				}

				// DonR 06Nov2025/22Dec2025: Derive is_opioid, is_ADHD, and is_preparation flags based on Drug Type.
				is_opioid		= (*drug_type		== 'N'	) ? 1 : 0;
				is_ADHD			= (*drug_type		== 'T'	) ? 1 : 0;
				is_preparation	= (preparation_type	>  0	) ? 1 : 0;

				// Write the drug row we fetched.
				v_MaxNumOfDrugsToUpdate++;

				// DonR 13Aug2024 User Story #349368: Largo Code length is now variable based on Version Number.
				fprintf (outFP, "%0*d",		VersionLargoLen,			v_DrugCode				);	//  8. Applies to both 1011 and 2094 downloads.

				// DonR 05Nov2017: This function now supports two different transactions: 1011, the "traditional" drug-list
				// download, and 2094, a new download for addtional fields.
				if (MessageIndex_in == 1011)
				{
					fprintf (outFP, "%0*d",		TUpdateCode_len,		v_DelFlg				);

					WinHebToDosHeb ((unsigned char *)(v_DrugDescription));
					fprintf (outFP, "%-*.*s",	TDrugDescription_len,
												TDrugDescription_len,	v_DrugDescription		);

					fprintf (outFP, "%0*d",		TPackageSize_len,		v_PackageSize			);
					fprintf (outFP, "%0*d",		TParticipatingType_len,	v_ParticipatingType		);
					fprintf (outFP, "%0*d",		1,						v_preferred_flg			);
					fprintf (outFP, "%0*d",		1,						v_openable_pkg			);
					fprintf (outFP, "%0*d",		1,						v_maccabicare_flag		);
					fprintf (outFP, "%0*d",		1,						v_in_gadget_table		);
//					fprintf (outFP, "%0*d",		1,						v_AssutaDrugStatus		);	// DonR 09Sep2009 AssutaPharm. 17Oct2017 DISABLED.
					fprintf (outFP, "%0*d",		TCompuSoftDrgCode_len,	v_ComputersoftDrugCode	);  // DonR 17Oct2017 expanded to length 6 by "stealing" from NIU Assuta Drug Status.
					fprintf (outFP, "%0*d",		TGenericFlag_len,		v_OtherParticTypExists	);
					fprintf (outFP, "%0*d",		TGenericFlag_len,		v_UpdateCode			);
					fprintf (outFP, "%0*d",		TGenericFlag_len,		v_SaleWithoutPresc		);
					fprintf (outFP, "%0*d",		1,						v_filler				);	// DonR 17Oct2017 - move v_AssutaDrugStatus here?
					fprintf (outFP, "%0*d",		ShapeCodeLen,			ShapeCode				);	// DonR 22Nov2020 CR #30978: Variable output length.
					fprintf (outFP, "%0*d",		3,						how_to_take_code		);	// DonR 12May2015.
					fprintf (outFP, "%-*.*s",	3, 3,					unit_abbreviation		);	// DonR 12May2015.
//					fprintf (outFP, "%0*d",		5,						v_DrugAddition1Code		);
//					fprintf (outFP, "%0*d",		4,						v_filler				);	// DonR 09Sep2009 AssutaPharm.
//					fprintf (outFP, "%0*d",		1,						v_ExpiryDateFlag		);	// DonR 09Sep2009 AssutaPharm.
					fprintf (outFP, "%c",								v_largo_type[0]			);	// DonR 21May2014 replaces Expiry Date Flag.
					fprintf (outFP, "%0*d",		5,						v_FPS_Group_Code		);	// DonR 09Sep2009 FPS.

					// 17Dec2009: Iris discussed making the following changes in the order of the last four fields. (Actually,
					// the change doesn't include the last field, but only the three before it.) In the end, she decided to
					// leave things as they are - but I'll leave the commented-out change in place, just in case the idea
					// gets revived.  -DonR
					//				fprintf (outFP, "%0*d",		1,						v_ExpiryDateFlag		);	// DonR 09Sep2009 AssutaPharm.
					//				fprintf (outFP, "%0*d",		4,						v_filler				);	// DonR 09Sep2009 AssutaPharm.
					//				fprintf (outFP, "%0*d",		5,						v_filler				);	// DonR 17Dec2009 AssutaPharm.
					//				fprintf (outFP, "%0*d",		5,						v_FPS_Group_Code		);	// DonR 09Sep2009 FPS.

				}	// "Traditional" Transaction 1011 drug-list download.

				else
				{
					// Convert package volume from float to integer - multiply by 100,000 to capture decimals.
					// Add a smidgin to the float number just to avoid weird precision errors.
					pkg_volume_int		= (long) ((package_volume	* 100) + .0000000001);
					ingr_1_quant_int	= (long) ((ingr_1_quant		* 100) + .0000000001);
					per_1_quant_int		= (long) ((per_1_quant		* 100) + .0000000001);

					// DonR 05Nov2017: Transaction 2094 supplemental-fields download.
					fprintf (outFP, "%0*d",		  4,						ingr_1_code				);	//  9
					fprintf (outFP, "%-*.*s",	 13,	 13,				bar_code_value			);	// 10

					// DonR 05Mar2018 CR #14502: Added three fields for OP adjustment by bottle volume.
					fprintf (outFP, "%0*d",		  1,						calc_op_by_volume		);	// 11
					fprintf (outFP, "%0*d",		  9,						pkg_volume_int			);	// 12
					fprintf (outFP, "%-*.*s",	  3,	  3,				per_1_units				);	// 13

					// DonR 28Jun2018: Added fields for main active ingredient.
					fprintf (outFP, "%0*d",		  9,						ingr_1_quant_int		);	// 14
					fprintf (outFP, "%-*.*s",	  3,	  3,				ingr_1_units			);	// 15
					fprintf (outFP, "%0*d",		  9,						per_1_quant_int			);	// 16
					fprintf (outFP, "%-*.*s",	  1,	  1,				drug_type				);	// 17. DonR 05May2019 CR #14710 (and reduced filler from 66 to 65 chars.)
					fprintf (outFP, "%0*d",		  1,						needs_refrigeration		);	// 18. Marianna 05Jan2022 : Feature 217431
					fprintf (outFP, "%0*d",		  1,						needs_dilution			);	// 19. Marianna 05Jan2022 : Feature 217431
					fprintf (outFP, "%0*d",		  1,						ConsignmentItem			);	// 20. DonR 16Apr2023 User Story #432608.
					fprintf (outFP, "%0*d",		  1,						is_narcotic				);	// 21. DonR 06Nov2025 User Story #441076.
					fprintf (outFP, "%0*d",		  1,						is_opioid				);	// 22. DonR 06Nov2025 User Story #441076.
					fprintf (outFP, "%0*d",		  1,						is_ADHD					);	// 23. DonR 06Nov2025 User Story #441076.
					fprintf (outFP, "%0*d",		  1,						psychotherapy_drug		);	// 24. DonR 06Nov2025 User Story #441076.
					fprintf (outFP, "%0*d",		  1,						is_preparation			);	// 25. DonR 22Dec2025 User Story #441076.
					fprintf (outFP, "%-*.*s",	 57,	 57,				""						);	// Filler.
				}

			}	// Fetch drugs loop.


//			PrintTime("Before CLOSE Cursor_1011_name");
			CloseCursor (	MAIN_DB, TR1011_2094_DrugListDownload_cur	);

			SQLERR_error_test();

		}
		while (0);	// One-time loop to avoid GOTO's.


		// Commit the transaction (although there shouldn't really be anything to commit).
		if( reStart == MAC_FALS )	/* Tranaction OK then COMMIT	*/
		{
			CommitAllWork ();

			SQLERR_error_test();
		}
		else
		{
			if( TransactionRestart() != MAC_OK )
			{
				v_ErrorCode = ERR_DATABASE_ERROR;
				break;
			}

			GerrLogReturn (GerrId, "Table is locked for the <%d> time", tries);

			sleep (ACCESS_CONFLICT_SLEEP_TIME);

			fclose (outFP);
		}

//		PrintTime("Before end retries loop");
	}	/* End of retries loop */


	// REWRITE  MESSAGE HEADER.
	rewind( outFP );

	fprintf (outFP, "%0*d",		MSG_ID_LEN,				(MessageIndex_in + 1)	);
	fprintf (outFP, "%0*d",		MSG_ERROR_CODE_LEN,		MAC_OK					);
	fprintf (outFP, "%0*d",		TPharmNum_len,			v_PharmNum				);
	fprintf (outFP, "%0*d",		TInstituteCode_len,		v_InstituteCode			);
	fprintf (outFP, "%0*d",		TTerminalNum_len,		v_TerminalNum			);
	fprintf (outFP, "%0*d",		TErrorCode_len,			v_ErrorCode				);
	fprintf (outFP, "%0*d",		TGenericYYYYMMDD_len,	v_DrugFileUpdateDate	);
	fprintf (outFP, "%0*d",		TGenericHHMMSS_len,		v_DrugFileUpdateHour	);
	fprintf (outFP, "%0*d",		TNumDrugsUpdated_len,	v_MaxNumOfDrugsToUpdate	);

	fclose( outFP );


	// Write the name of output-file on string for T-SWITCH.
	*p_outputWritten = sprintf (OBuffer, "%s", fileName);
	*output_type_flg = ANSWER_IN_FILE;

	ssmd_data_ptr->error_code	= v_ErrorCode;
	ssmd_data_ptr->pharmacy_num	= v_PharmNum;
	ssmd_data_ptr->terminal_num	= v_TerminalNum;

//	PrintTime("Before Handler Exit");

	RESTORE_ISOLATION;

	return (RC_SUCCESS);
}
// End of drug-list download handler.



/*=======================================================================
||																		||
||		           HandlerToMsg_1013									||
||		Message handler for message 1013 close shift.					||
||																		||
 =======================================================================*/
int   HandlerToMsg_1013( char		*IBuffer,
			 int		TotalInputLen,
			 char		*OBuffer,
			 int		*p_outputWritten,
			 int		*output_type_flg,
			 SSMD_DATA	*ssmd_data_ptr )
{
    return GenHandlerToMsg_1013( IBuffer,
				 TotalInputLen,
				 OBuffer,
				 p_outputWritten,
				 output_type_flg,
				 ssmd_data_ptr,
				 0,
				 0);
}



/*=======================================================================
||																		||
||		           HandlerToMsg_1013									||
||		Message handler for message 1013 close shift.					||
||																		||
 =======================================================================*/
int   HandlerToSpool_1013( char		*IBuffer,
			 int		TotalInputLen,
			 char		*OBuffer,
			 int		*p_outputWritten,
			 int		*output_type_flg,
			 SSMD_DATA	*ssmd_data_ptr,
			 short int	commerr_flag)
{
    return GenHandlerToMsg_1013( IBuffer,
				 TotalInputLen,
				 OBuffer,
				 p_outputWritten,
				 output_type_flg,
				 ssmd_data_ptr,
				 1,
				 commerr_flag);
}



/*=======================================================================
||																		||
||			 HandlerToMsg_1013											||
||	      Message handler for message 1013 close shift.					||
||				GENERIC													||
||																		||
 =======================================================================*/
int   GenHandlerToMsg_1013( char		*IBuffer,
			    int		TotalInputLen,
			    char		*OBuffer,
			    int		*p_outputWritten,
			    int		*output_type_flg,
			    SSMD_DATA	*ssmd_data_ptr,
			    int		fromspool,
			    int		commerr_flag)
{
	/*=======================================================================
	||			    Local declarations				||
	 =======================================================================*/
    int				tries,
					reStart;
    PHARMACY_INFO	Phrm_info;
	ISOLATION_VAR;
	/*=======================================================================
	||			Message fields variables			||
	 =======================================================================*/
    TPharmNum		v_PharmNum;
    TInstituteCode	v_InstituteCode;
    TTerminalNum	v_TerminalNum;
    TClosingCode	v_ClosingCode;
    TDate		v_Date;
    THour		v_Hour;
    /*
     * Declerations of variables appearing in return message ONLY
     * ----------------------------------------------------------
     */
    TErrorCode		v_ErrorCode;
    TErrorCode		err;
    TDate		v_DateSql;
    THour		v_HourSql;
    TDate		SysDate;
    THour		SysTime;
    int			comm_err_code;

	/*=======================================================================
	||									||
	||				Body of function			||
	||									||
	||======================================================================||
	||			  Init answer variables				||
	=======================================================================*/
	REMEMBER_ISOLATION;
    PosInBuff	= IBuffer;
    v_ErrorCode	= NO_ERROR;
    v_DateSql	= GetDate();
    v_HourSql	= GetTime();
	/*=======================================================================
	||									||
	||		      Get message fields data into variables		||
	||									||
	 =======================================================================*/
    v_PharmNum      = GetLong( &PosInBuff,  TPharmNum_len );
    ssmd_data_ptr->pharmacy_num = v_PharmNum;
    CHECK_ERROR();

    v_InstituteCode = GetShort( &PosInBuff, TInstituteCode_len );
    CHECK_ERROR();

    v_TerminalNum   = GetShort( &PosInBuff, TTerminalNum_len );
    CHECK_ERROR();

    v_ClosingCode   = GetShort( &PosInBuff, TClosingCode_len );
    CHECK_ERROR();

    v_Date	    = GetLong( &PosInBuff,  TDate_len );
    CHECK_ERROR();
    ChckDate( v_Date );
    CHECK_ERROR();

    v_Hour	    = GetLong( &PosInBuff, THour_len );
    CHECK_ERROR();
    ChckTime( v_Hour );
    CHECK_ERROR();
    /*
     *  Get amount of input not eaten
     *  -----------------------------
     */
    ChckInputLen( TotalInputLen - (PosInBuff - IBuffer) );
    CHECK_ERROR();
	/*=======================================================================
	||									||
	||			    SQL TRANSACTIONs.				||
	||									||
	||======================================================================||
	||			     Retries loop				||
	=======================================================================*/

    reStart = MAC_TRUE;

    for( tries=0; tries < SQL_UPDATE_RETRIES && reStart == MAC_TRUE; tries++ )
    {
	/*=======================================================================
	||			  Init answer variables				||
	 =======================================================================*/
	reStart		= MAC_FALS;
	v_ErrorCode	= NO_ERROR;
	SysDate		= GetDate();
	SysTime		= GetTime();

//	if( TransactionBegin() != MAC_OK )
//	{
//	    v_ErrorCode = ERR_DATABASE_ERROR;
//	    break;
//	}
//
	SET_ISOLATION_COMMITTED;

	/*=======================================================================
	||			 Test if pharmacy OPENed.			||
	 =======================================================================*/
	do				/* Exiting from LOOP will send	*/
	{				/* the reply to phrmacy	*/

	    v_ErrorCode = IS_PHARMACY_OPEN (v_PharmNum, v_InstituteCode, &Phrm_info);

	    if( v_ErrorCode == ERR_PHARMACY_NOT_FOUND )
	    {
		break;
	    }

		/*=======================================================================
		||			 Update PHARMACY table.				||
		 =======================================================================*/

	    if( ! fromspool )		/* Don't close spooled closings	*/
	    {
		ExecSQL (	MAIN_DB, UPD_SetPharmacyOpenStatus, &PHARM_CLOSE_SQL, &v_PharmNum, &ROW_NOT_DELETED, END_OF_ARG_LIST	);

		Conflict_Test( reStart );

		if( SQLERR_error_test() )
		{
		    SetErrorVar( &v_ErrorCode, ERR_DATABASE_ERROR );
		}
	    }

	} while( 0 );			/* DO loop shuld run only once	*/
	/*=======================================================================
	||		    Insert into PHARMACY_LOG table.			||
	 =======================================================================*/
	if( reStart == MAC_FALS )	/* Dont write retries to log 	*/
	{
	    do
	    {
		comm_err_code = commerr_flag;

		ExecSQL (	MAIN_DB, TR1013_INS_pharmacy_log,
					&v_ClosingCode,		&comm_err_code,		&SysDate,		&SysTime,		&v_ErrorCode,		&v_InstituteCode,
					&PHARM_CLOSE_SQL,	&v_Date,			&v_Hour,		&v_PharmNum,	&v_TerminalNum,		&NOT_REPORTED_TO_AS400,
					END_OF_ARG_LIST																											);

		Conflict_Test( reStart );

		if( SQLERR_error_test() )
		{
		    SetErrorVar( &v_ErrorCode, ERR_DATABASE_ERROR );
		}

	    } while( 0 );		/* DO loop shuld run only once	*/

	}
	/*=======================================================================
	||			  Commit the transaction.			||
	 =======================================================================*/
	if( reStart == MAC_FALS )	/* Tranaction OK then COMMIT	*/
	{
		CommitAllWork ();

	    if( SQLERR_error_test() )	/* Could not COMMIT		*/
	    {
		SetErrorVar( &v_ErrorCode, ERR_DATABASE_ERROR );
	    }
	    else
	    if( ! fromspool )		/* Don't close spooled closings	*/
	    {
		Phrm_info.pharmacy_permission = 0;
		Phrm_info.price_list_num      = 0;
		Phrm_info.open_close_flg      = MAC_FALS;
//		err = UPDATE_PHARMACY_IN_SHERED_MEM( v_PharmNum,
//						   v_InstituteCode,
//						   &Phrm_info );
//		if( err != MAC_OK )
//		{
//		    SetErrorVar( &v_ErrorCode, err );
//		}
	    }
	}
	else
	{
	    if( TransactionRestart() != MAC_OK )
	    {
		v_ErrorCode = ERR_DATABASE_ERROR;
		break;
	    }

	    GerrLogReturn(
			  GerrId,
			  "Table is locked for the <%d> time",
			  tries
			  );

	    sleep( ACCESS_CONFLICT_SLEEP_TIME );
	}

    }	/* End of retries loop */

    if( reStart == MAC_TRUE )
    {
	GerrLogReturn(
		      GerrId,
		      "Locked for <%d> times",
		      SQL_UPDATE_RETRIES
		      );

	SetErrorVar( &v_ErrorCode, ERR_DATABASE_ERROR );
    }
	/*=======================================================================
	||									||
	||			  WRITE RESPOND MESSAGE.			||
	||									||
	 =======================================================================*/
    nOut =  sprintf( OBuffer, "%0*d",
		     MSG_ID_LEN,		1042 );

    nOut += sprintf( OBuffer + nOut, "%0*d",
		     MSG_ERROR_CODE_LEN,	MAC_OK );

    nOut += sprintf( OBuffer + nOut, "%0*d",
		     TPharmNum_len,		v_PharmNum );

    nOut += sprintf( OBuffer + nOut, "%0*d",
		     TInstituteCode_len,	v_InstituteCode );

    nOut += sprintf( OBuffer + nOut, "%0*d",
		     TTerminalNum_len,		v_TerminalNum );

    nOut += sprintf( OBuffer + nOut, "%0*d",
		     TErrorCode_len,		v_ErrorCode );

    nOut += sprintf( OBuffer + nOut, "%*d",
		     TDate_len,			v_DateSql );

    nOut += sprintf( OBuffer + nOut, "%*d" ,
		     THour_len,			v_HourSql );

    /*
     * Return the size in Bytes of respond message (INCLUDING header)
     * --------------------------------------------------------------
     */
    *p_outputWritten = nOut;
    *output_type_flg = ANSWER_IN_BUFFER;
    ssmd_data_ptr->error_code = v_ErrorCode;
    ssmd_data_ptr->pharmacy_num = v_PharmNum;
    ssmd_data_ptr->terminal_num = v_TerminalNum;

	RESTORE_ISOLATION;

    return  RC_SUCCESS;

}
/* End of 1013 handler */



/*=======================================================================
||																		||
||			 HandlerToMsg_1014											||
||	    Message handler for message 1014 price-list update.				||
||																		||
||																		||
 =======================================================================*/
int   HandlerToMsg_1014( char   *IBuffer,         
			 int	TotalInputLen,    
			 char   *OBuffer,         
			 int    *p_outputWritten, 
			 int    *output_type_flg,
			 SSMD_DATA	*ssmd_data_ptr
			 )
{
	/*=======================================================================
	||			    Local declarations				||
	 =======================================================================*/
    char		fileName[256];
    int			tries,
			reStart;
    int			Number_cursor; //Yulia for choice Cursor_1014_name
	int		CustomWhereClauseID;
    FILE		*outFP;
	ISOLATION_VAR;
	/*=======================================================================
	||			Message fields variables			||
	 =======================================================================*/
    PHARMACY_INFO	Phrm_info;
    /*
     * INPUT MESSAGE
     * =============
     */
    TPharmNum			v_PharmNum;
    TInstituteCode		v_InstituteCode;
    TTerminalNum		v_TerminalNum;
    TMaccabiPriceListNum	v_MaccabiPriceListNum;
    TMaccabiPriceListNum        v_PriceListNum;
    TLastPriceListUpdateDate	v_LastPriceListUpdateDate;
    TLastPriceListUpdateHour	v_LastPriceListUpdateHour;
	short int					v_maccabicare_flag	= 0;

    /*
     * RETURN MESSAGE ONLY
     * ===================
     * header
     * ------
     */
    TErrorCode			v_ErrorCode;
    TPriceListUpdateDate	v_PriceListUpdateDate;
    TPriceListUpdateHour	v_PriceListUpdateHour;
	char					TableName[33] = "Price_list_macabi";	// DonR 19Aug2020: Change from char * to array, since ODBC doesn't like string constants.
    TNumOfDrugsToUpdate		v_NumOfDrugsToUpdate;
    /*
     * lines
     * -----
     */
    TDrugCode 	      		v_DrugCode;
    TNewMaccabiPrice   		v_NewMaccabiPrice;
    TPurchasePrice	      	v_PurchasePrice;
    TDeleteFlage		v_DeleteFlage;
    /*
     * General SQL variables.
     * ----------------------
     */
	char str_sql[800];
    
	char str_sql_mac_pr[800];
	char str_sql_mac_new[800];
	char str_sql_mac_largo[800];

	/*=======================================================================
	||			    Select cursors.				||
	 =======================================================================*/

	sprintf (str_sql_mac_pr,"%s","    SELECT del_flg,largo_code,macabi_price,supplier_price FROM price_list ");
	strcat	(str_sql_mac_pr,	 "    WHERE  price_list_code = ? ");
	strcat	(str_sql_mac_pr,     "      AND  ((date_macabi  = ? AND time_macabi >= ? ) OR (date_macabi  > ?)) ");

	sprintf (str_sql_mac_new,"%s","   SELECT del_flg,largo_code,macabi_price,supplier_price FROM price_list ");
	strcat	(str_sql_mac_new,	 "    WHERE  price_list_code = ? ");

	sprintf (str_sql_mac_largo,"%s"," SELECT del_flg,largo_code,macabi_price,supplier_price FROM price_list ");
	strcat	(str_sql_mac_largo,	 "    WHERE  price_list_code = 1 AND largo_code >90000 ");

	/*=======================================================================
	||									||
	||			    Body of function				||
	||									||
	||======================================================================||
	||		       Init answer header variables			||
	=======================================================================*/
    PosInBuff			= IBuffer;
    v_ErrorCode			= NO_ERROR;
    v_PriceListUpdateDate	= 0;
    v_PriceListUpdateHour	= 0;
    v_NumOfDrugsToUpdate	= 0;
	REMEMBER_ISOLATION;

	/*=======================================================================
	||									||
	||		      Get message fields data into variables		||
	||									||
	 =======================================================================*/
	v_PharmNum					= GetLong	(	&PosInBuff, TPharmNum_len					);					// 10, 7
	ssmd_data_ptr->pharmacy_num = v_PharmNum;
	CHECK_ERROR();

	v_InstituteCode				= GetShort	(	&PosInBuff, TInstituteCode_len				); CHECK_ERROR ();	// 17, 2
	v_TerminalNum				= GetShort	(	&PosInBuff, TTerminalNum_len				); CHECK_ERROR ();	// 19, 2
	v_MaccabiPriceListNum		= GetShort	(	&PosInBuff, TMaccabiPriceListNum_len		); CHECK_ERROR ();	// 21, 3

	v_LastPriceListUpdateDate	= GetLong	(	&PosInBuff, TLastPriceListUpdateDate_len	); CHECK_ERROR ();	// 24, 8
	ChckDate ( v_LastPriceListUpdateDate ); CHECK_ERROR();

	v_LastPriceListUpdateHour	= GetLong	(	&PosInBuff, TLastPriceListUpdateHour_len	); CHECK_ERROR ();	// 32, 6
	ChckTime ( v_LastPriceListUpdateHour ); CHECK_ERROR();

	GET_TRN_VERSION_IF_SENT	// DonR 13Aug2024 User Story #349368.												   38, 2

    /*
     * Get amount of input not eaten
     * -----------------------------
     */
    ChckInputLen( TotalInputLen - (PosInBuff - IBuffer) );
    CHECK_ERROR();

	// DonR 13Aug2024 User Story #349368: Set Largo Code
	// length conditionally on Transaction Version Number.
	VersionLargoLen = (VersionNumber > 1) ? 9 : 5;

	/*=======================================================================
	||									||
	||			    SQL TRANSACTIONs.				||
	||									||
	||======================================================================||
	||			     Retries loop				||
	=======================================================================*/

    reStart = MAC_TRUE;

    sprintf( fileName, "%s/prices_%0*d_%0*d_%0*d.snd",
	     MsgDir,
	     TPharmNum_len,      v_PharmNum,
	     TInstituteCode_len, v_InstituteCode,
	     TTerminalNum_len,   v_TerminalNum );

    for( tries=0; tries < SQL_UPDATE_RETRIES && reStart == MAC_TRUE; tries++ )
    {
	/*=======================================================================
	||		  Create new file for respond message.			||
	 =======================================================================*/
	outFP = fopen( fileName, "w" );

	if( outFP == NULL )
	{
	    int	err = errno;
	    GmsgLogReturn( GerrSource, __LINE__,
			   "\tCant open output file '%s'\n"
			   "\tError (%d) %s\n",
			   fileName, err, strerror(err));
		RESTORE_ISOLATION;
	    return( ERR_UNABLE_OPEN_OUTPUT_FILE );
	}
	/*=======================================================================
	||		       Init answer header variables			||
	 =======================================================================*/
	reStart			= MAC_FALS;
	v_ErrorCode		= NO_ERROR;
	v_PriceListUpdateDate	= 0;
	v_PriceListUpdateHour	= 0;
	v_NumOfDrugsToUpdate	= 0;

	// DonR 14Jul2005: Get rid of unnecessary table-locked errors.
	SET_ISOLATION_DIRTY;

	/*=======================================================================
	||			 Test if pharmacy code.				||
	 =======================================================================*/
	do				/* Exiting from LOOP will send	*/
	{				/* the reply to phrmacy	*/
	    /*
	     * Test if opened
	     * --------------
	     */
	    v_ErrorCode = IS_PHARMACY_OPEN (v_PharmNum, v_InstituteCode, &Phrm_info);
	    if( v_ErrorCode != MAC_OK )
	    {
		break;
	    }
		/*=======================================================================
		||			 Get the modification date.			||
		 =======================================================================*/
//		TableName = "Price_list_macabi";
		ExecSQL (	MAIN_DB, READ_TablesUpdate, &v_PriceListUpdateDate, &v_PriceListUpdateHour, &TableName, END_OF_ARG_LIST	);
	      
	    Conflict_Test( reStart );

	    if( SQLERR_code_cmp( SQLERR_not_found ) == MAC_TRUE )
	    {
			v_PriceListUpdateDate = 0;
			v_PriceListUpdateHour = 0;
	    }
	    else if( SQLERR_error_test() )
	    {
			if( SetErrorVar( &v_ErrorCode, ERR_DATABASE_ERROR ) )
			{
				break;
			}
	    }

	// Get Pharmacy data.
	ExecSQL (	MAIN_DB, TR1014_READ_GetPharmMaccabicareFlag, &v_maccabicare_flag, &v_PharmNum, &ROW_NOT_DELETED, END_OF_ARG_LIST	);

	    Conflict_Test( reStart );

	    if( SQLERR_code_cmp( SQLERR_not_found ) == MAC_TRUE )
	    {
			v_maccabicare_flag = 0;
	    }
	    else if( SQLERR_error_test() )
	    {
			if( SetErrorVar( &v_ErrorCode, ERR_DATABASE_ERROR ) )
			{
				break;
			}
	    }


		/*=======================================================================
		||          If pharmacy is private -> answer will contain only header    ||
		 =======================================================================*/
	    if( PRIVATE_PHARMACY &&
		Phrm_info.price_list_num ==1 && v_MaccabiPriceListNum == 1)
	    {
		break;
	    }

	/*=======================================================================
	||	  Open the proper cursor acording to type of pharmacy.				||
	=======================================================================*/
	if (Phrm_info.price_list_num != v_MaccabiPriceListNum)
	{
		if (((MACCABI_PHARMACY) || ((PRIVATE_PHARMACY) && (v_MaccabiPriceListNum == 800))) &&
		    (v_MaccabiPriceListNum == 999 || v_MaccabiPriceListNum == 111 || v_MaccabiPriceListNum == 800) ) 
		{
			v_PriceListNum = v_MaccabiPriceListNum;

			CustomWhereClauseID = TR1014_PriceListCodeAndDate;
//			Number_cursor = 1;
//			strcpy (str_sql,str_sql_mac_pr);
		}
		else
		{
			v_PriceListNum = Phrm_info.price_list_num;

			CustomWhereClauseID = TR1014_PriceListCodeOnly;
//			Number_cursor = 2;
//			strcpy (str_sql,str_sql_mac_new);
		}
	}

	else	// Price List requested == pharmacy's "normal" price list.
	{
		if(MACCABI_PHARMACY && v_MaccabiPriceListNum == 1)
		{
			// v_PriceListNum is irrelevant for this version.
			CustomWhereClauseID = TR1014_PriceListLargoGT90000;
//			Number_cursor = 3;
//			strcpy (str_sql,str_sql_mac_largo);
		}
		else
		{
			v_PriceListNum = v_MaccabiPriceListNum;

			CustomWhereClauseID = TR1014_PriceListCodeAndDate;
//			Number_cursor = 4;
//			strcpy (str_sql,str_sql_mac_pr);
		}
	}

		// With custom WHERE clauses, we can just use the full input-parameter list
		// for the one with the most input parameters; versions that need fewer
		// parameters simply won't take them off the list.
//		DeclareCursor (	MAIN_DB, TR1014_PriceList_cur, CustomWhereClauseID,
//						&v_PriceListNum,			&v_LastPriceListUpdateDate,
//						&v_LastPriceListUpdateHour,	&v_LastPriceListUpdateDate	);
		DeclareDeferredCursor (	MAIN_DB, TR1014_PriceList_cur, CustomWhereClauseID, END_OF_ARG_LIST	);

		if (SQLCODE != 0)
			GerrLogMini (GerrId, "DECLAREd TR1014_PriceList_cur, WHERE clause #%d, SQLCODE = %d.", CustomWhereClauseID, SQLCODE);
	    if (SQLERR_error_test ())
	    {
			SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR);
			break;
	    }

		switch (CustomWhereClauseID)
		{
		case TR1014_PriceListCodeAndDate:	OpenCursorUsing	(	MAIN_DB, TR1014_PriceList_cur, TR1014_PriceListCodeAndDate,
																&v_PriceListNum,			&v_LastPriceListUpdateDate,
																&v_LastPriceListUpdateHour,	&v_LastPriceListUpdateDate,
																END_OF_ARG_LIST												);
											break;

		case TR1014_PriceListCodeOnly:		OpenCursorUsing	(	MAIN_DB, TR1014_PriceList_cur, TR1014_PriceListCodeOnly,
																&v_PriceListNum, END_OF_ARG_LIST							);
											break;

		case TR1014_PriceListLargoGT90000:	OpenCursor		(	MAIN_DB, TR1014_PriceList_cur	);
											break;
		}

		if (SQLCODE != 0)
			GerrLogMini (GerrId, "OPENed TR1014_PriceList_cur, WHERE clause #%d, SQLCODE = %d.", CustomWhereClauseID, SQLCODE);
	    if (SQLERR_error_test ())
	    {
			SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR);
			break;
	    }

	    if( SQLERR_error_test() )
	    {
			SetErrorVar( &v_ErrorCode, ERR_DATABASE_ERROR );
			break;
	    }

		/*=======================================================================
		||		   Create the header with data so far.			||
		 =======================================================================*/
	    fprintf( outFP, "%0*d",
		     MSG_ID_LEN,		 1015 );
	    fprintf( outFP, "%0*d",
		     MSG_ERROR_CODE_LEN,	 MAC_OK );
	    fprintf( outFP, "%0*d",
		     TPharmNum_len,		 v_PharmNum );
	    fprintf( outFP, "%0*d",
		     TInstituteCode_len,	 v_InstituteCode );
	    fprintf( outFP, "%0*d",
		     TTerminalNum_len,		 v_TerminalNum );
	    fprintf( outFP, "%0*d",
		     TErrorCode_len,		 v_ErrorCode );
	    fprintf( outFP,"%0*d",
			TNewMaccabiPriceListNum_len, ((v_PriceListNum == 800) ? v_PriceListNum
																	: Phrm_info.price_list_num) );
	    fprintf( outFP, "%0*d",
		     TPriceListUpdateDate_len,	 v_PriceListUpdateDate );
	    fprintf( outFP, "%0*d",
		     TPriceListUpdateHour_len,	 v_PriceListUpdateHour );
	    fprintf( outFP, "%0*d",
		     TNumOfDrugsToUpdate_len,	 v_NumOfDrugsToUpdate );
		/*=======================================================================
		||		     Write the price data to file.			||
		 =======================================================================*/
	    for( ; /* for ever */ ; )
	    {
		/*
		 * Fetch and test DB errors
		 * ------------------------
		 */
		FetchCursorInto (	MAIN_DB, TR1014_PriceList_cur,
							&v_DeleteFlage,		&v_DrugCode,	&v_NewMaccabiPrice,
							&v_PurchasePrice,	END_OF_ARG_LIST						);

		if ((SQLCODE != 0) && (SQLERR_code_cmp (SQLERR_not_found) == MAC_FALSE))
			GerrLogMini (GerrId, "FETCHed TR1014_PriceList_cur, WHERE clause #%d, SQLCODE = %d.", CustomWhereClauseID, SQLCODE);

		Conflict_Test( reStart );

		if( SQLERR_code_cmp( SQLERR_end_of_fetch ) == MAC_TRUE )
		{
		    break;
		}
		else if( SQLERR_error_test() )
		{
		    SetErrorVar( &v_ErrorCode, ERR_DATABASE_ERROR );
		    break;
		}

		
		/*
		 * Write the drug rows
		 * -------------------
		 */
		v_NumOfDrugsToUpdate++;

		fprintf( outFP, "%0*d",
			 VersionLargoLen,        v_DrugCode );	// DonR 13Aug2024 User Story #349368: Variable length based on trn. version.
		fprintf( outFP, "%0*d",
			 TNewMaccabiPrice_len, v_NewMaccabiPrice );
		fprintf( outFP, "%0*d",
			 TPurchasePrice_len,   v_PurchasePrice );
	    } /* for(;; ) */

		CloseCursor (	MAIN_DB, TR1014_PriceList_cur	);
	    SQLERR_error_test();

	} while( 0 );		/* DO loop Shuld run only once	*/
	/*=======================================================================
	||			  Commit the transaction.			||
	 =======================================================================*/

	if( reStart == MAC_FALS )	/* Tranaction OK then COMMIT	*/
	{
		CommitAllWork ();

	    SQLERR_error_test();
	}
	else
	{
	    if( TransactionRestart() != MAC_OK )
	    {
		v_ErrorCode = ERR_DATABASE_ERROR;
		break;
	    }

	    GerrLogReturn(
			  GerrId,
			  "Table is locked for the <%d> time",
			  tries
			  );

	    sleep( ACCESS_CONFLICT_SLEEP_TIME );
	    fclose( outFP );
	}
    }	/* End of retries loop */

    if( reStart == MAC_TRUE )
    {
	GerrLogReturn(
		      GerrId,
		      "Locked for <%d> times",
		      SQL_UPDATE_RETRIES
		      );

	SetErrorVar( &v_ErrorCode, ERR_DATABASE_ERROR );
    }
	/*=======================================================================
	||									||
	||			REWRITE  MESSAGE HEADER.			||
	||									||
	 =======================================================================*/

    rewind( outFP );

    fprintf( outFP, "%0*d",
	     MSG_ID_LEN,		 1015 );
    fprintf( outFP, "%0*d",
	     MSG_ERROR_CODE_LEN,	 MAC_OK );
    fprintf( outFP, "%0*d",
	     TPharmNum_len,		 v_PharmNum );
    fprintf( outFP, "%0*d",
	     TInstituteCode_len,	 v_InstituteCode );
    fprintf( outFP, "%0*d",
	     TTerminalNum_len,		 v_TerminalNum );
    fprintf( outFP, "%0*d",
	     TErrorCode_len,		 v_ErrorCode );
    fprintf( outFP,"%0*d",
	     TNewMaccabiPriceListNum_len, ((v_PriceListNum == 800) ? v_PriceListNum
																	: Phrm_info.price_list_num) );
    fprintf( outFP, "%0*d",
	     TPriceListUpdateDate_len,	 v_PriceListUpdateDate );
    fprintf( outFP, "%0*d",
	     TPriceListUpdateHour_len,	 v_PriceListUpdateHour );
    fprintf( outFP, "%0*d",
	     TNumOfDrugsToUpdate_len,	 v_NumOfDrugsToUpdate );

    fclose( outFP );

	/*
	 * Write the name of output-file on string for T-SWITCH
	 * ----------------------------------------------------
	 */

    *p_outputWritten = sprintf( OBuffer, "%s", fileName );
    *output_type_flg = ANSWER_IN_FILE;
    ssmd_data_ptr->error_code = v_ErrorCode;
    ssmd_data_ptr->pharmacy_num = v_PharmNum;
    ssmd_data_ptr->terminal_num = v_TerminalNum;

	RESTORE_ISOLATION;

    return( RC_SUCCESS );

}
/* end of 1014 handler */



/*=======================================================================
||																		||
||			    HandlerToMsg_1022_6022									||
||		       A drug amount is entering stock							||
||		Message handler for message 1022/6022 - insert into				||
||		         tables STOCK & STOCK_REPORT							||
||																		||
||			      GENERIC mode											||
||																		||
 =======================================================================*/
int   GENERIC_HandlerToMsg_1022_6022	(	int			TrnNumber_in,
											char		*IBuffer,
											int			TotalInputLen,
											char		*OBuffer,
											int			*p_outputWritten,
											int			*output_type_flg,
											SSMD_DATA	*ssmd_data_ptr,
											short		commfail_flag,
											int			fromSpool			)
{
	/*=======================================================================
	||			    Local declarations				||
	 =======================================================================*/
    int				nOut;
    int				i,
					reStart;
    PHARMACY_INFO	Phrm_info;
	ISOLATION_VAR;
	/*=======================================================================
	||			Message fields variables			||
	 =======================================================================*/

    /*
     * Buffer to hold STOCK_REPORT records
     * -----------------------------------
     */
	static TStockReportRec		*StockRepRecs;
	static long int			stock_alloc;
	static int			first_time = 1;
    
	/*
		*  Include message field variables file:
		*/
	TPharmNum	    		v_PharmNum;
	TInstituteCode			v_InstituteCode;
	TTerminalNum			v_TerminalNum;
	TUserIdentification		v_UserIdentification;
	TActionType	    		v_ActionType;
	TAuthorizationNum		v_AuthorizationNum;
	TDate		    		v_Date;
	THour		    		v_Hour;
	TSuppDevPharmNum		v_SuppDevPharmNum;
	TBilNum		        	v_BillNum;
	TDiscountAmount			v_DiscountAmount;
		
	TVatAmount			v_VatAmount;
	TBilAmount			v_BilAmount;
	TDate				v_SupplBillDate;
	TSupplSendNum			v_SupplSendNum;
	TBilAmountWithVat		v_BilAmountWithVat;
	TBilAmountBeforeVat		v_BilAmountBeforeVat;
	TBilConstr			v_BillConstr;
	TBilInvDiff			v_BillInvDiff;

	Tdiary_monthNew			v_diary_month;
	TMedNumOfDrugLines		v_MedNumOfDrugLines;

		
	/*
		*   Declerations of variables appearing in return message ONLY:
		*/
	TErrorCode			v_ErrorCode;
	TEnterDate			v_EnterDate;
	TEnterHour			v_EnterHour;
	TMedNumOfDrugLines		v_AcceptedLinesNum;

	TStockReportRec			*stock_rep_ptr;

	short int			comm_error_code = commfail_flag;

	TFlg				delete_flag;
	TFlg				reported_to_as400_flag;

	long int			same_count;

	/*=======================================================================
	||									||
	||				Body of function			||
	||									||
	||======================================================================||
	||			  Init answer variables				||
	 =======================================================================*/
	REMEMBER_ISOLATION;
    PosInBuff = IBuffer;
    
    if( first_time )
    {
	stock_alloc = 10;

	StockRepRecs =
	    (TStockReportRec*)
	    MemAllocReturn(
			   GerrId,
			   stock_alloc * sizeof(TStockReportRec),
			   "memory for stock report lines"
			   );

	if( StockRepRecs == NULL )
	{
		RESTORE_ISOLATION;
	    return( ERR_NO_MEMORY );
	}

	first_time = 0;
    }

	/*=======================================================================
	||									||
	||		      Get message fields data into variables		||
	||									||
	 =======================================================================*/

     v_PharmNum		=	GetLong( &PosInBuff, TPharmNum_len );
     ssmd_data_ptr->pharmacy_num = v_PharmNum;
     CHECK_ERROR(); 
     v_InstituteCode	=	GetShort( &PosInBuff, 
					      TInstituteCode_len );
     CHECK_ERROR(); 
     v_TerminalNum		=	GetShort( &PosInBuff, 
					      TTerminalNum_len );
     CHECK_ERROR(); 

	 // DonR 13Aug2024 User Story #349368: Trn. 6022 includes a
	 // Transaction Version Number to control the Largo Code length.
	 if (TrnNumber_in == 6022)
	 {
		 VersionNumber	=	GetShort (&PosInBuff,	2);	CHECK_ERROR();
	 }
	 else
	 {
		 VersionNumber	=	1;
	 }

     v_UserIdentification   =	GetLong( &PosInBuff, 
					     TUserIdentification_len );
     CHECK_ERROR(); 
     v_ActionType		=	GetShort( &PosInBuff, 
					      TActionType_len );
     CHECK_ERROR(); 
     v_AuthorizationNum	=	GetLong( &PosInBuff, 
					     TAuthorizationNum_len );
     CHECK_ERROR(); 
     v_Date			=	GetLong( &PosInBuff, TDate_len );
     CHECK_ERROR(); 
     ChckDate( v_Date );

     CHECK_ERROR(); 
     v_Hour			=	GetLong( &PosInBuff, THour_len );
     CHECK_ERROR(); 
     ChckTime( v_Hour );

     CHECK_ERROR(); 
     v_SuppDevPharmNum 	=	GetLong( &PosInBuff, 
					     TSuppDevPharmNum_len );
     CHECK_ERROR(); 

     GetString( &PosInBuff, v_BillNum, TBilNum_len );
     CHECK_ERROR(); 

     v_DiscountAmount	=	GetLong( &PosInBuff, 
					     TDiscountAmount_len );
     CHECK_ERROR(); 


     v_VatAmount		=	GetLong( &PosInBuff, 
 						TVatAmount_len );
     CHECK_ERROR(); 

     v_BilAmount		=	GetLong( &PosInBuff, 
					     TBilAmount_len );
     CHECK_ERROR(); 

     v_SupplBillDate	=	GetLong( &PosInBuff, TDate_len );
     CHECK_ERROR(); 
     ChckDate( v_SupplBillDate );

     GetString( &PosInBuff, v_SupplSendNum, TSupplSendNum_len );
     CHECK_ERROR(); 
   
     v_BilAmountWithVat	=	GetLong( &PosInBuff, 
					     TBilAmountWithVat_len );
     CHECK_ERROR(); 

     v_BilAmountBeforeVat =	GetLong( &PosInBuff, 
					     TBilAmountBeforeVat_len );
     CHECK_ERROR(); 

     v_BillConstr        =       GetShort( &PosInBuff, 
					      TBilConstr_len );
     CHECK_ERROR(); 
	
     v_BillInvDiff	=	GetLong( &PosInBuff, 
					     TBilInvDiff_len );
     CHECK_ERROR(); 


     v_diary_month   	=	GetLong( &PosInBuff, 
					      Tdiary_new_month_len );
     CHECK_ERROR(); 
     v_MedNumOfDrugLines =	GetShort( &PosInBuff, 
					      TNumOfDrugLines_len );
     CHECK_ERROR();

     if( v_MedNumOfDrugLines > stock_alloc )
     {
	stock_alloc = v_MedNumOfDrugLines + 10;

	StockRepRecs =
	    (TStockReportRec*)
	    MemReallocReturn(
			     GerrId,
			     StockRepRecs,
			     stock_alloc * sizeof(TStockReportRec),
			     "memory for stock report lines"
			     );

	if( StockRepRecs == NULL )
	{
		RESTORE_ISOLATION;
	    return( ERR_NO_MEMORY );
	}

     }

	 // DonR 13Aug2024 User Story #349368: Set Largo Code
	 // length conditionally on Transaction Version Number.
	 VersionLargoLen = (VersionNumber > 1) ? 9 : 5;

     for( i = 0  ;  i < v_MedNumOfDrugLines ;  i++ )
     {
	 StockRepRecs[i].v_LineNum=  GetShort( &PosInBuff, 
						     TLineNum_len );
	 CHECK_ERROR();

	 StockRepRecs[i].v_DrugCode = GetLong( &PosInBuff, VersionLargoLen );	// DonR 13Aug2024 User Story #349368
	 CHECK_ERROR(); 

	 StockRepRecs[i].v_OpAmout	       	=  GetLong( &PosInBuff, 
						     TOpAmout_len );
	 CHECK_ERROR(); 
	 StockRepRecs[i].v_UnitsAmout       =  GetLong( &PosInBuff, 
						     TUnitsAmout_len );
	 CHECK_ERROR(); 
	 StockRepRecs[i].v_AmountType	=  GetShort( &PosInBuff, 
						      TAmountType_len );
	 CHECK_ERROR();
	 StockRepRecs[i].v_BasePriceFor1Op=  GetLong( &PosInBuff, 
					      TBasePriceFor1Op_len);
	 CHECK_ERROR();
	 StockRepRecs[i].v_UnitPriceFor1Op	=  GetLong( &PosInBuff,
						   TUnitPriceFor1Op_len );
	 CHECK_ERROR(); 
	 StockRepRecs[i].v_TotalForItemLine	=  GetLong( &PosInBuff, 
						   TTotalForItemLine_len );
	 CHECK_ERROR(); 
	 StockRepRecs[i].v_FullOpStock	=  GetLong( &PosInBuff, 
						     TFullOpStock_len );
	 CHECK_ERROR(); 
	 StockRepRecs[i].v_CurrentUnitStock	=  GetLong( &PosInBuff, 
						  TCurrentUnitStock_len );
	 CHECK_ERROR(); 
	 StockRepRecs[i].v_MinOpStock	=  GetLong( &PosInBuff, 
						     TMinOpStock_len );
	 CHECK_ERROR(); 
     }

    /*
     *  Get amount of input not eaten
     *  -----------------------------
     */
    ChckInputLen( TotalInputLen - (PosInBuff - IBuffer) );
    CHECK_ERROR();
	/*=======================================================================
	||									||
	||			    SQL TRANSACTIONs.				||
	||									||
	||======================================================================||
	||			     Retries loop				||
	 =======================================================================*/

    reStart = MAC_TRUE;

    for( tries=0; tries < SQL_UPDATE_RETRIES && reStart == MAC_TRUE; tries++ )
    {
	/*=======================================================================
	||			  Init answer variables				||
	 =======================================================================*/
	reStart	    = MAC_FALS;
	SET_ISOLATION_COMMITTED;
	v_EnterDate = GetDate();
	v_EnterHour = GetTime();
    

	v_ErrorCode = IS_PHARMACY_OPEN (v_PharmNum, v_InstituteCode, &Phrm_info);

	if( v_ErrorCode == ERR_PHARMACY_NOT_FOUND )
	{
	    break;
	}

	//
	// 24-06-98 Ely Levy -
	// Make sure that same spooled transaction dont
	//  pass to as400
	//
	same_count = 0;

	if( fromSpool )
	do
	{
		ExecSql (	MAIN_DB, TR1022_READ_CheckDuplicateStockRow,
					&same_count,			&v_PharmNum,		&v_Date,
					&v_SuppDevPharmNum,		&v_TerminalNum,		&v_ActionType,
					&v_AuthorizationNum,	&v_diary_month,		END_OF_ARG_LIST		);
	    
	    Conflict_Test( reStart );

	    if( SQLERR_error_test() )
	    {
		SetErrorVar( &v_ErrorCode, ERR_DATABASE_ERROR );
		break;
	    }

	} while( 0 );

	
	delete_flag = same_count ?
			ROW_SYS_DELETED : ROW_NOT_DELETED;

	reported_to_as400_flag = same_count ?
			REPORTED_TO_AS400 : NOT_REPORTED_TO_AS400;


	//if (fromSpool)
	//{
	//	GerrLogFnameMini ("spooled_stuff",
	//					  GerrId,
	//					  "\nspool_1022: CommErr %d, same_count %d, fromSpool %d,"
	//					  "\nreported_to_as400 %d, del_flg %d, Pharmacy %d,"
	//					  "\nSerial # %d, Action %d, DiaryMonth %d.",
	//					  (int)comm_error_code,
	//					  (int)same_count,
	//					  (int)fromSpool,
	//					  (int)reported_to_as400_flag,
	//					  (int)delete_flag,
	//					  v_PharmNum,
	//					  (int)v_AuthorizationNum,
	//					  (int)v_ActionType,
	//					  v_diary_month);
	//}


	/*=======================================================================
	||									||
	||	       INSERT DATA INTO TABLES STOCK & STOCK_REPORT 		||
	||									||
	 =======================================================================*/
	
	if( reStart == MAC_FALS )
	do
	{
		ExecSQL (	MAIN_DB, TR1022_INS_Stock,
					&v_PharmNum,				&v_InstituteCode,		&v_TerminalNum,
					&v_UserIdentification,		&v_ActionType,			&v_AuthorizationNum,
					&v_Date,					&v_Hour,				&v_SuppDevPharmNum,
					&v_BillNum,					&v_DiscountAmount,		&v_diary_month,
					&v_MedNumOfDrugLines,		&v_EnterDate,			&v_EnterHour,

					&v_ErrorCode,				&comm_error_code,		&delete_flag,
					&reported_to_as400_flag,	&v_VatAmount,			&v_BilAmount,
					&v_SupplBillDate,			&v_SupplSendNum,		&v_BilAmountWithVat,
					&v_BilAmountBeforeVat,		&v_BillConstr,			&v_BillInvDiff,
					END_OF_ARG_LIST																);
	    
	    Conflict_Test( reStart );

	    if( SQLERR_error_test() )
	    {
			SetErrorVar( &v_ErrorCode, ERR_DATABASE_ERROR );
			break;
	    }

	    stock_rep_ptr = StockRepRecs;
  
	    for( i = 0; i < v_MedNumOfDrugLines; i++ )
	    {
			ExecSQL (	MAIN_DB, TR1022_INS_StockReport,
						&stock_rep_ptr->v_LineNum,			&stock_rep_ptr->v_DrugCode,		&stock_rep_ptr->v_OpAmout,
						&stock_rep_ptr->v_UnitsAmout,		&stock_rep_ptr->v_AmountType,	&stock_rep_ptr->v_UnitPriceFor1Op,
						&stock_rep_ptr->v_TotalForItemLine,	&stock_rep_ptr->v_FullOpStock,	&stock_rep_ptr->v_CurrentUnitStock,
						&stock_rep_ptr->v_MinOpStock,		&comm_error_code,				&v_EnterDate,

						&v_EnterHour,						&v_ActionType,					&v_diary_month,
						&v_PharmNum,						&v_AuthorizationNum,			&v_TerminalNum,
						&delete_flag,						&reported_to_as400_flag,		&stock_rep_ptr->v_BasePriceFor1Op,
						END_OF_ARG_LIST																								);

		Conflict_Test( reStart );
	
		if( SQLERR_error_test() )
		{
		    SetErrorVar( &v_ErrorCode, ERR_DATABASE_ERROR );
		    break;
		}
		
		stock_rep_ptr++;
	    }
	    
	}
	while( 0 );
	
	/*=======================================================================
	||			  Commit the transaction.			||
	 =======================================================================*/
	if( reStart == MAC_FALS )	/* Tranaction OK then COMMIT	*/
	{
	    // 13-5-98 Ely :
	    // Let spooled transactions in even when shift 
	    // is closed.
	    //
	    if( ! SetErrorVar( &v_ErrorCode, v_ErrorCode ) ||
		( fromSpool && v_ErrorCode == ERR_PHARMACY_NOT_OPEN ) )
	    {
			CommitAllWork ();
	    }
	    else
	    {
			RollbackAllWork ();
	    }
	    if( SQLERR_error_test() )		/* Could not COMMIT	*/
	    {
		SetErrorVar( &v_ErrorCode, ERR_DATABASE_ERROR );
	    }
	}
	else
	{
	    if( TransactionRestart() != MAC_OK ) /* -> sleep & rollback */
	    {
		v_ErrorCode = ERR_DATABASE_ERROR;
		break;
	    }

	    GerrLogReturn(
			  GerrId,
			  "Table is locked for the <%d> time",
			  tries
			  );

	    sleep( ACCESS_CONFLICT_SLEEP_TIME );
	}
    }	/* End of retries loop */

    if( reStart == MAC_TRUE )
    {
	GerrLogReturn(
		      GerrId,
		      "Locked for <%d> times",
		      SQL_UPDATE_RETRIES
		      );

	SetErrorVar( &v_ErrorCode, ERR_DATABASE_ERROR );
    }

	/*=======================================================================
	||									||
	||			  WRITE RESPOND MESSAGE.			||
	||									||
	 =======================================================================*/
    
    nOut =  sprintf( OBuffer, "%0*d",
				 MSG_ID_LEN,		TrnNumber_in + 1 );	// DonR 13Aug2024 User Story #349368: Support 6022 as well as 1022.

    nOut += sprintf( OBuffer + nOut, "%0*d",
				 MSG_ERROR_CODE_LEN,	MAC_OK );
    
    nOut +=  sprintf( OBuffer + nOut, "%0*d",
				  TPharmNum_len,
				  v_PharmNum );
    
    nOut +=  sprintf( OBuffer + nOut, "%0*d",
				  TInstituteCode_len,
				  v_InstituteCode );
    
    nOut +=  sprintf( OBuffer + nOut, "%0*d",
				  TTerminalNum_len,
				  v_TerminalNum );
    
    nOut +=  sprintf( OBuffer + nOut, "%0*d",
				  TErrorCode_len, v_ErrorCode );
    
    nOut +=  sprintf( OBuffer + nOut, "%0*d",
				  TAuthorizationNum_len ,
				  v_AuthorizationNum );
    
    nOut +=  sprintf( OBuffer + nOut, "%0*d",
				  TActionType_len ,
				  v_ActionType );
    
    nOut +=  sprintf( OBuffer + nOut, "%0*d",
				  TEnterDate_len, v_EnterDate );
    
    nOut +=  sprintf( OBuffer + nOut, "%0*d",
				  TEnterHour_len, v_EnterHour );
    
    nOut +=  sprintf( OBuffer + nOut, "%0*d",
				  TMedNumOfDrugLines_len ,
				  v_MedNumOfDrugLines );
    
    /*
     * Return the size in Bytes of respond message
     * -------------------------------------------
     */
    *p_outputWritten = nOut;
    *output_type_flg = ANSWER_IN_BUFFER;
    ssmd_data_ptr->error_code = v_ErrorCode;

	RESTORE_ISOLATION;

    return RC_SUCCESS;
} /* End of GENERIC_1022 handler */



/*=======================================================================
||																		||
||		  REALTIME mode of GENERIC_HandlerToMsg_1022_6022				||
||																		||
 =======================================================================*/
int   HandlerToMsg_1022_6022	(	int			TrnNumber_in,
									char		*IBuffer,
									int			TotalInputLen,
									char		*OBuffer,
									int			*p_outputWritten,
									int			*output_type_flg,
									SSMD_DATA	*ssmd_data_ptr		)
{
    return GENERIC_HandlerToMsg_1022_6022 (	TrnNumber_in,
											IBuffer,
											TotalInputLen,
											OBuffer,
											p_outputWritten,
											output_type_flg,
											ssmd_data_ptr,
											(short)0,		/* comm_fail_flg = 0 */
											0 );		/*-> RT mode         */
}



/*=======================================================================
||									||
||			  HandlerToMsg_1026				||
||	   Message handler for message 1026 get-text-message.		||
||									||
||									||
 =======================================================================*/
int   HandlerToMsg_1026( char   *IBuffer,         
			 int	TotalInputLen,    
			 char   *OBuffer,         
			 int    *p_outputWritten, 
			 int    *output_type_flg,
			 SSMD_DATA	*ssmd_data_ptr
			 )
{
	/*=======================================================================
	||			    Local declarations				||
	 =======================================================================*/
    char			fileName[256];
    int				tries,
					reStart,
					LineLen,
					StartIn;
    PHARMACY_INFO	Phrm_info;
    FILE			*outFP;
    static char		*signs = "?!.,";
    char			*signPtr;
	ISOLATION_VAR;
	/*=======================================================================
	||		       Message fields variables				||
	 =======================================================================*/
    /*
     * INPUT MESSAGE
     * =============
     */
    TPharmNum			v_PharmNum;
    TInstituteCode		v_InstituteCode;
    TTerminalNum		v_TerminalNum;
    /*
     * RETURN MESSAGE ONLY
     * ===================
     * header
     * ------
     */
    TErrorCode			v_ErrorCode;
    TMessageDate		v_MessageDate;
    TMessageHour		v_MessageHour;
    TAnotherMessage		v_AnotherMessage;
    TMedNumOfMessageLines	v_MedNumOfMessageLines;
    /*
     * lines
     * -----
     */
    TMessageLine		v_MessageLine, TmpLine;
    char			*TmpLinePtr;
    /*
     * General SQL
     * -----------
     */
    TMessageID			v_MessageID;
    TMessageLineID		v_MessageLineID;
    TMessageDate		v_DeliveryDate;
    TMessageHour		v_DeliveryHour;


	/*=======================================================================
	||									||
	||			    Body of function				||
	||									||
	||======================================================================||
	||		       Init answer header variables			||
	=======================================================================*/
	REMEMBER_ISOLATION;
    PosInBuff			= IBuffer;
    v_ErrorCode			= NO_ERROR;
    v_MessageDate		= 0;
    v_MessageHour		= 0;
    v_AnotherMessage		= MAC_FALS;
    v_MedNumOfMessageLines	= 0;
    v_DeliveryDate		= GetDate();
    v_DeliveryHour		= GetTime();
	/*=======================================================================
	||									||
	||		      Get message fields data into variables		||
	||									||
	 =======================================================================*/
    v_PharmNum	    = GetLong( &PosInBuff,  TPharmNum_len );
    ssmd_data_ptr->pharmacy_num = v_PharmNum;
    CHECK_ERROR();
    v_InstituteCode = GetShort( &PosInBuff, TInstituteCode_len );
    CHECK_ERROR();
    v_TerminalNum   = GetShort( &PosInBuff, TTerminalNum_len );
    CHECK_ERROR();
    /*
     * Get amount of input not eaten
     * -----------------------------
     */
    ChckInputLen( TotalInputLen - (PosInBuff - IBuffer) );
    CHECK_ERROR();
	/*=======================================================================
	||									||
	||			    SQL TRANSACTIONs.				||
	||									||
	||======================================================================||
	||			     Retries loop				||
	=======================================================================*/

    reStart = MAC_TRUE;

    sprintf( fileName, "%s/message_%0*d_%0*d_%0*d.snd",
	     MsgDir,
	     TPharmNum_len,      v_PharmNum,
	     TInstituteCode_len, v_InstituteCode,
	     TTerminalNum_len,   v_TerminalNum );

    for(
	tries=0;
	tries < SQL_UPDATE_RETRIES && reStart == MAC_TRUE;
	tries++
	)
    {
	/*=======================================================================
	||		  Create new file for respond message.			||
	 =======================================================================*/
	outFP = fopen( fileName, "w" );

	if( outFP == NULL )
	{
	    int	err = errno;
	    GmsgLogReturn( GerrSource, __LINE__,
			   "\tCant open output file '%s'\n"
			   "\tError (%d) %s\n",
			   fileName, err, strerror(err));
		RESTORE_ISOLATION;
	    return( ERR_UNABLE_OPEN_OUTPUT_FILE );
	}
	/*=======================================================================
	||		       Init answer header variables			||
	 =======================================================================*/
	reStart			= MAC_FALS;
	v_ErrorCode		= NO_ERROR;
	v_MessageDate		= 0;
	v_MessageHour		= 0;
	v_AnotherMessage	= MAC_FALS;
	v_MedNumOfMessageLines	= 0;
	v_DeliveryDate		= GetDate();
	v_DeliveryHour		= GetTime();

//	if( TransactionBegin() != MAC_OK )
//	{
//	    v_ErrorCode = ERR_DATABASE_ERROR;
//	    break;
//	}
	/*=======================================================================
	||			   Test pharmacy code.				||
	 =======================================================================*/
	do				/* Exiting from LOOP will send	*/
	{				/* the reply to phrmacy	*/
		SET_ISOLATION_DIRTY;

	    /*
	     * Test if opened
	     * --------------
	     */
	    v_ErrorCode = IS_PHARMACY_OPEN (v_PharmNum, v_InstituteCode, &Phrm_info);

	    if( v_ErrorCode != MAC_OK )
	    {
		break;
	    }
		/*=======================================================================
		||			   Open the cursor.				||
		 =======================================================================*/
		DeclareAndOpenCursor (	MAIN_DB, TR1026_message_id_cur,	&v_PharmNum, END_OF_ARG_LIST	);

	    if (SQLERR_error_test ())
	    {
			SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR);
			break;
	    }
		/*=======================================================================
		||			 Get the message date & id.			||
		 =======================================================================*/
	    /*
	     * GET MESSAGE ID
	     * --------------
	     */
		FetchCursorInto (	MAIN_DB, TR1026_message_id_cur,
							&v_MessageDate, &v_MessageHour, &v_MessageID,	END_OF_ARG_LIST	);

	    Conflict_Test( reStart );

	    if( SQLERR_code_cmp( SQLERR_end_of_fetch ) == MAC_TRUE )
	    {
			v_ErrorCode = ERR_NO_OUT_STENDING_MESSAGE;
			break;
	    }
	    else
	    if( SQLERR_error_test() )
	    {
			v_ErrorCode = ERR_DATABASE_ERROR;
			break;
	    }

	    /*
	     * CHECK FOR MORE MESSAGES
	     * -----------------------
	     */
		FetchCursorInto (	MAIN_DB, TR1026_message_id_cur,
							&v_MessageDate, &v_MessageHour, &v_MessageID,	END_OF_ARG_LIST	);

	    Conflict_Test( reStart );

	    v_AnotherMessage = MAC_TRUE;

	    if( SQLERR_code_cmp( SQLERR_end_of_fetch ) == MAC_TRUE )
	    {
			v_AnotherMessage = MAC_FALS;
	    }
	    else
	    if( SQLERR_error_test() )
	    {
			v_AnotherMessage = MAC_FALS;
			v_ErrorCode = ERR_DATABASE_ERROR;
			break;
	    }

		/*=======================================================================
		||			UPDATE THE DELIVERY DATE & TIME			||
		 =======================================================================*/
		ExecSQL (	MAIN_DB, TR1026_UPD_pharmacy_message,
					&v_DeliveryDate, &v_DeliveryHour, &v_MessageID, &v_PharmNum, END_OF_ARG_LIST	);
	    
	    Conflict_Test( reStart );

	    if( SQLERR_error_test() )
	    {
			v_ErrorCode = ERR_DATABASE_ERROR;
			break;
	    }
		/*=======================================================================
		||			   Open the cursor.				||
		 =======================================================================*/
		DeclareAndOpenCursor (	MAIN_DB, TR1026_messags_text_cur,
								&v_MessageID, &ROW_NOT_DELETED, END_OF_ARG_LIST		);

	    if (SQLERR_error_test ())
	    {
			SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR);
			break;
	    }
		/*=======================================================================
		||		   Create the header with data so far.			||
		 =======================================================================*/
	    fprintf( outFP, "%0*d", MSG_ID_LEN,		1027 );

	    fprintf( outFP, "%0*d", MSG_ERROR_CODE_LEN, MAC_OK );

	    fprintf( outFP, "%0*d", TPharmNum_len,	v_PharmNum );

	    fprintf( outFP, "%0*d", TInstituteCode_len,	v_InstituteCode );

	    fprintf( outFP, "%0*d", TTerminalNum_len,	v_TerminalNum );

	    fprintf( outFP, "%0*d", TErrorCode_len,	v_ErrorCode );

	    fprintf( outFP, "%0*d", TMessageDate_len,	v_MessageDate );

	    fprintf( outFP, "%0*d", TMessageHour_len,	v_MessageHour );

	    fprintf( outFP, "%0*d", TAnotherMessage_len,	v_AnotherMessage );

	    fprintf( outFP, "%0*d",TMedNumOfMessageLines_len,
						 v_MedNumOfMessageLines );
		/*=======================================================================
		||		     Write the messages data to file.			||
		 =======================================================================*/
	    for( ; /* for ever */ ; )
	    {
		/*
		 * Fetch and test DB errors
		 * ------------------------
		 */
			FetchCursorInto (	MAIN_DB, TR1026_messags_text_cur,
								&v_MessageID,	&v_MessageLineID, &v_MessageLine,	END_OF_ARG_LIST	);

		Conflict_Test( reStart );

		if( SQLERR_code_cmp( SQLERR_end_of_fetch ) == MAC_TRUE )
		{
		    break;
		}
		else
		if( SQLERR_error_test() )
		{
		    SetErrorVar( &v_ErrorCode, ERR_DATABASE_ERROR );
		    break;
		}
		/*
		 * Write the error-message rows
		 * ----------------------------
		 */
		v_MedNumOfMessageLines++;

		for( LineLen = TMessageLine_len; LineLen > 0 && 
		               v_MessageLine[LineLen - 1] == ' '; LineLen-- );

		if( LineLen != 0 )
		{
		    
		    strcpy( TmpLine, v_MessageLine ); 

		    for( TmpLinePtr = TmpLine, StartIn = 0; *TmpLinePtr &&
			                *TmpLinePtr == ' '; TmpLinePtr++, StartIn++ );

		    memset( v_MessageLine , ' ', TMessageLine_len );

		    strncpy( v_MessageLine + (TMessageLine_len - LineLen), 
			     TmpLine + StartIn,
			     LineLen - StartIn);
			
		    v_MessageLine[TMessageLine_len+1] = 0;

		    for( LineLen = TMessageLine_len; LineLen > 0 && 
		               v_MessageLine[LineLen - 1] == ' '; LineLen-- );

		    for( TmpLinePtr = v_MessageLine, StartIn = 0; *TmpLinePtr &&
			                *TmpLinePtr == ' '; TmpLinePtr++, StartIn++ );

		    for( ; signPtr = strchr( signs, v_MessageLine[LineLen-1] ); )
		    {
			for( TmpLinePtr = v_MessageLine + LineLen - 2; 
			             TmpLinePtr >= v_MessageLine ; TmpLinePtr--)
			{
			    TmpLinePtr[1] = TmpLinePtr[0];
			}

			v_MessageLine[StartIn] = *signPtr;
		    }
		}

		WinHebToDosHeb( (unsigned char *)(v_MessageLine) );

		fprintf( outFP, "%-*.*s",
			 TMessageLine_len, TMessageLine_len, v_MessageLine );

	    } /* for( ; ; ) */

	} while( 0 );		/* DO loop Shuld run only once	*/

	// DonR 04Jan2021: Moved the CLOSE CURSOR statements out of the "while" loop,
	// to make sure they always execute.
	/*=======================================================================
	||			   Close the cursor.				||
	 =======================================================================*/
	CloseCursor (	MAIN_DB, TR1026_message_id_cur	);

	CloseCursor (	MAIN_DB, TR1026_messags_text_cur	);

	/*=======================================================================
	||			  Commit the transaction.			||
	 =======================================================================*/
	if( reStart == MAC_FALS )	/* Tranaction OK then COMMIT	*/
	{
		CommitAllWork ();

	    SQLERR_error_test();
	}
	else
	{
	    if( TransactionRestart() != MAC_OK )
	    {
		v_ErrorCode = ERR_DATABASE_ERROR;
		break;
	    }

	    GerrLogReturn(
			  GerrId,
			  "Table is locked for the <%d> time",
			  tries
			  );

	    sleep( ACCESS_CONFLICT_SLEEP_TIME );

	    fclose( outFP );
	}

    }	/* End of retries loop */

    if( reStart == MAC_TRUE )
    {
	GerrLogReturn(
		      GerrId,
		      "Locked for <%d> times",
		      SQL_UPDATE_RETRIES
		      );

	SetErrorVar( &v_ErrorCode, ERR_DATABASE_ERROR );
    }

	/*=======================================================================
	||									||
	||			REWRITE  MESSAGE HEADER.			||
	||									||
	 =======================================================================*/

    rewind( outFP );

    fprintf( outFP, "%0*d", MSG_ID_LEN,		1027 );

    fprintf( outFP, "%0*d", MSG_ERROR_CODE_LEN, MAC_OK );

    fprintf( outFP, "%0*d", TPharmNum_len,	v_PharmNum );

    fprintf( outFP, "%0*d", TInstituteCode_len,	v_InstituteCode );

    fprintf( outFP, "%0*d", TTerminalNum_len,	v_TerminalNum );

    fprintf( outFP, "%0*d", TErrorCode_len,	v_ErrorCode );

    fprintf( outFP, "%0*d", TMessageDate_len,	v_MessageDate );

    fprintf( outFP, "%0*d", TMessageHour_len,	v_MessageHour );

    fprintf( outFP, "%0*d",TAnotherMessage_len,	v_AnotherMessage );

    fprintf( outFP, "%0*d",TMedNumOfMessageLines_len,
	     v_MedNumOfMessageLines );

    fclose( outFP );

	/*
	 * Write the name of output-file on string for T-SWITCH
	 * ----------------------------------------------------
	 */

    *p_outputWritten = sprintf( OBuffer, "%s", fileName );
    *output_type_flg = ANSWER_IN_FILE;
    ssmd_data_ptr->error_code = v_ErrorCode;
    ssmd_data_ptr->pharmacy_num = v_PharmNum;
    ssmd_data_ptr->terminal_num = v_TerminalNum;

	RESTORE_ISOLATION;

    return( RC_SUCCESS );

} /* end of handler */



/*=======================================================================
||									||
||			     HandlerToMsg_1028				||
||	      Answer to a Financial-Status query from pharmacy		||
||									||
 =======================================================================*/
int   HandlerToMsg_1028( char		*IBuffer,
			 char		TotalInputLen,
			 char		*OBuffer,
			 int		*p_outputWritten,
			 int		*output_type_flg,
			 SSMD_DATA	*ssmd_data_ptr )
{
    
	/*=======================================================================
	||			    Local declarations				||
	 =======================================================================*/
    int				nOut;
    int				i,
					reStart;
    PHARMACY_INFO	Phrm_info;
	ISOLATION_VAR;
	/*=======================================================================
	||			Message fields variables			||
	 =======================================================================*/

    /*
     * Buffer to hold STOCK_REPORT records:
     * --------------------------------------
     */
	/*
		*  Include message field variables file:
		*/
	TPharmNum			v_PharmNum;
	TInstituteCode			v_InstituteCode;
	TTerminalNum			v_TerminalNum;
	TFromDate			v_FromDate;
	TToDate				v_ToDate;
	Tdiary_month			v_diary_month;

	/*
		*   Declerations of variables appearing in return message ONLY:
		*/
	TErrorCode			v_ErrorCode;
	TTotalFinanceAccumulate		v_TotalFinanceAccumulate;
	TMemberParticipatingAccumulate	v_MemberParticipatingAccumulate;
	TNumOfSoldRecipe		v_NumOfSoldRecipe;
	TNumOfSoldDrugLines		v_NumOfSoldDrugLines;
	TCancleFinanceAccumulate	v_CancleFinanceAccumulate;
	TNumOfCancledRecipeLines	v_NumOfCancledRecipeLines;
	TDeliverFailedCommFinanceAccum	v_DeliverFailedCommFinanceAccum;
	TDeliverFailedCommLinesNum	v_DeliverFailedCommLinesNum;
	TDate				v_Date;
	THour				v_Hour;
	TSqlInd				v_Ind;

	/*=======================================================================
	||									||
	||				Body of function			||
	||									||
	||======================================================================||
	||			  Init answer variables				||
	 =======================================================================*/
	REMEMBER_ISOLATION;
    PosInBuff   = IBuffer;
    
	/*=======================================================================
	||									||
	||		      Get message fields data into variables		||
	||									||
	 =======================================================================*/
	/*
	 *  Insert message fields into local variables:
	 */
    v_PharmNum	    = GetLong( &PosInBuff, TPharmNum_len );
    ssmd_data_ptr->pharmacy_num = v_PharmNum;
    CHECK_ERROR();
    v_InstituteCode = GetShort( &PosInBuff, TInstituteCode_len );
    CHECK_ERROR();
    v_TerminalNum   = GetShort( &PosInBuff, TTerminalNum_len );
    CHECK_ERROR();
    v_FromDate	    = GetLong( &PosInBuff, TFromDate_len );
    CHECK_ERROR();
    ChckDate( v_FromDate );
    
    CHECK_ERROR();
    v_ToDate	    = GetLong( &PosInBuff, TToDate_len );
    CHECK_ERROR();
    ChckDate( v_ToDate );
    CHECK_ERROR();
    v_diary_month   = GetShort( &PosInBuff, Tdiary_month_len );
    CHECK_ERROR(); 
	 
    /*
     *  Get amount of input not eaten
     *  -----------------------------
     */
    ChckInputLen( TotalInputLen - (PosInBuff - IBuffer) );
    CHECK_ERROR();
	/*=======================================================================
	||									||
	||			    SQL TRANSACTIONs.				||
	||									||
	||======================================================================||
	||			     Retries loop				||
	 =======================================================================*/

    reStart = MAC_TRUE;
	SET_ISOLATION_DIRTY;

    for( tries=0; tries < SQL_UPDATE_RETRIES && reStart == MAC_TRUE; tries++ )
    {
	/*=======================================================================
	||			  Init answer variables				||
	 =======================================================================*/
	reStart				= MAC_FALS;
	v_TotalFinanceAccumulate	= 0;
	v_MemberParticipatingAccumulate = 0;
	v_NumOfSoldRecipe		= 0;
	v_NumOfSoldDrugLines		= 0;
	v_CancleFinanceAccumulate	= 0;
	v_NumOfCancledRecipeLines	= 0;
	v_DeliverFailedCommFinanceAccum	= 0;
	v_DeliverFailedCommLinesNum	= 0;
	v_Date				= GetDate();
	v_Hour				= GetTime();

	/*=======================================================================
	||			    Test pharmacy data.				||
	 =======================================================================*/
	v_ErrorCode = IS_PHARMACY_OPEN (v_PharmNum, v_InstituteCode, &Phrm_info);
	if( v_ErrorCode != MAC_OK )
	{
	    break;
	}
	v_ErrorCode = NO_ERROR;
	
//	if( TransactionBegin() != MAC_OK )  /* BEGIN WORK & SET LOCK MODE */
//	{
//	    v_ErrorCode = ERR_DATABASE_ERROR;
//	    break;
//	}
//	

	do
	{
		ExecSQL (	MAIN_DB, TR1028_READ_PharmacyDailySum,
					&v_TotalFinanceAccumulate,			&v_MemberParticipatingAccumulate,
					&v_NumOfSoldRecipe,					&v_NumOfSoldDrugLines,
					&v_CancleFinanceAccumulate,			&v_NumOfCancledRecipeLines,
					&v_DeliverFailedCommFinanceAccum,	&v_DeliverFailedCommLinesNum,
					&v_PharmNum,						&v_InstituteCode,
					&v_diary_month,						&v_FromDate,
					&v_ToDate,							END_OF_ARG_LIST						);

		v_Ind = ColumnOutputLengths [1];
	    
	    Conflict_Test( reStart );

	    if( SQLERR_error_test() )
	    {
			SetErrorVar( &v_ErrorCode, ERR_DATABASE_ERROR );
			break;
	    }
	    
	    if( SQLMD_is_null( v_Ind ) )
	    {
		v_TotalFinanceAccumulate		= 0;
		v_MemberParticipatingAccumulate		= 0;
		v_NumOfSoldRecipe			= 0;
		v_NumOfSoldDrugLines			= 0;
		v_CancleFinanceAccumulate		= 0;
		v_NumOfCancledRecipeLines		= 0;
		v_DeliverFailedCommFinanceAccum		= 0;
		v_DeliverFailedCommLinesNum		= 0;
	    }

	    v_ErrorCode = NO_ERROR;

	}
	while( 0 );
	
	
	/*=======================================================================
	||			  Commit the transaction.			||
	 =======================================================================*/
	if( reStart == MAC_FALS )	/* Tranaction OK then COMMIT	*/
	{
	    if( ! SetErrorVar( &v_ErrorCode, v_ErrorCode ) )	// Nothing fatal.
	    {
			CommitAllWork ();
	    }
	    else
	    {
			RollbackAllWork ();
	    }
	    if( SQLERR_error_test() )		/* Could not COMMIT	*/
	    {
			SetErrorVar( &v_ErrorCode, ERR_DATABASE_ERROR );
	    }
	}
	else
	{
	    if( TransactionRestart() != MAC_OK ) /* -> sleep & rollback */
	    {
			v_ErrorCode = ERR_DATABASE_ERROR;
			break;
	    }

	    GerrLogReturn(
			  GerrId,
			  "Table is locked for the <%d> time",
			  tries
			  );

	    sleep( ACCESS_CONFLICT_SLEEP_TIME );
	}

    } /* endfor() */

    if( reStart == MAC_TRUE )
    {
		GerrLogReturn(
				  GerrId,
				  "Locked for <%d> times",
				  SQL_UPDATE_RETRIES
				  );

		SetErrorVar( &v_ErrorCode, ERR_DATABASE_ERROR );
    }
    
	/*=======================================================================
	||									||
	||			  WRITE RESPOND MESSAGE.			||
	||									||
	 =======================================================================*/
    
    nOut =  sprintf( OBuffer, "%0*d",
				 MSG_ID_LEN,		1029 );
    
    nOut += sprintf( OBuffer + nOut, "%0*d",
			 MSG_ERROR_CODE_LEN,	MAC_OK );
    
    nOut += sprintf( OBuffer + nOut, "%0*d",
				  TPharmNum_len, v_PharmNum );
    
    nOut += sprintf( OBuffer + nOut, "%0*d",
				  TInstituteCode_len,
				  v_InstituteCode );
    
    nOut += sprintf( OBuffer + nOut, "%0*d",
				  TTerminalNum_len,
				  v_TerminalNum );
    
    nOut += sprintf( OBuffer + nOut, "%0*d",
				  TErrorCode_len,
				  v_ErrorCode );
    
    nOut += sprintf( OBuffer + nOut, "%0*d",
				  TFromDate_len, v_FromDate );
    
    nOut += sprintf( OBuffer + nOut, "%0*d",
				  TToDate_len, v_ToDate );
    
    nOut += sprintf( OBuffer + nOut, "%0*d",
				  TTotalFinanceAccumulate_len,
				  v_TotalFinanceAccumulate );
    
    nOut += sprintf( OBuffer + nOut, "%0*d",
				  TMemberParticipatingAccumulate_len,
				  v_MemberParticipatingAccumulate );
    
    nOut += sprintf( OBuffer + nOut, "%0*d",
				  TNumOfSoldRecipe_len,
				  v_NumOfSoldRecipe );
    
    nOut += sprintf( OBuffer + nOut, "%0*d",
				  TNumOfSoldDrugLines_len,
				  v_NumOfSoldDrugLines );
    
    nOut += sprintf( OBuffer + nOut, "%0*d",
				  TCancleFinanceAccumulate_len,
				  v_CancleFinanceAccumulate );
    
    nOut += sprintf( OBuffer + nOut, "%0*d",
				  TNumOfCancledRecipeLines_len,
				  v_NumOfCancledRecipeLines );
    
    nOut += sprintf( OBuffer + nOut, "%0*d",
				  TDeliverFailedCommFinanceAccum_len,
				  v_DeliverFailedCommFinanceAccum );
    
    nOut += sprintf( OBuffer + nOut, "%0*d",
				  TDeliverFailedCommLinesNum_len,
				  v_DeliverFailedCommLinesNum );
    
    nOut += sprintf( OBuffer + nOut, "%0*d",
				  TDate_len, v_Date );
    
    nOut += sprintf( OBuffer + nOut, "%0*d",
				  THour_len, v_Hour );

    
    /*
     * Return the size in Bytes of respond message
     * -------------------------------------------
     */
    *p_outputWritten = nOut;
    *output_type_flg = ANSWER_IN_BUFFER;
    ssmd_data_ptr->error_code = v_ErrorCode;

	RESTORE_ISOLATION;

    return  RC_SUCCESS;
    
}
/* End of 1028 handler */



/*=======================================================================
||																		||
||			  HandlerToMsg_1043											||
||	   Message handler for message 1043 error-messages update.			||
||																		||
||																		||
 =======================================================================*/
int   HandlerToMsg_1043( char   *IBuffer,         
			 int	TotalInputLen,    
			 char   *OBuffer,         
			 int    *p_outputWritten, 
			 int    *output_type_flg,
			 SSMD_DATA	*ssmd_data_ptr
			 )
{
	/*=======================================================================
	||			    Local declarations				||
	 =======================================================================*/
    char			fileName[256];
    int				tries,
					reStart;
    PHARMACY_INFO	Phrm_info;
    FILE			*outFP;
	ISOLATION_VAR;
	/*=======================================================================
	||		       Message fields variables				||
	 =======================================================================*/
    /*
     * INPUT MESSAGE
     * =============
     */
    int					v_PharmNum;
    short				v_InstituteCode;
    short				v_TerminalNum;
    /*
     * RETURN MESSAGE ONLY
     * ===================
     * header
     * ------
     */
 	char				TableName [33] = "Pc_error_message";	// DonR 19Aug2020: Change from char * to array, since ODBC doesn't like string constants.
    short				v_ErrorCode;
    int					v_UpdateDate;
    int					v_UpdateHour;
    short				v_ErrorCodeToUpdateNum;
    /*
     * lines
     * -----
     */
    short				v_ErrorCode_m;
    char				v_ErrorDecription	[60 + 1];
    short				v_ErrorSevirity;

	/*=======================================================================
	||									||
	||			    Body of function				||
	||									||
	||======================================================================||
	||		       Init answer header variables			||
	=======================================================================*/
	REMEMBER_ISOLATION;
    PosInBuff			= IBuffer;
    v_ErrorCode			= NO_ERROR;
    v_UpdateDate		= 0;
    v_UpdateHour		= 0;
    v_ErrorCodeToUpdateNum	= 0;
	/*=======================================================================
	||									||
	||		      Get message fields data into variables		||
	||									||
	 =======================================================================*/
    v_PharmNum	    = GetLong( &PosInBuff,  TPharmNum_len );
    ssmd_data_ptr->pharmacy_num = v_PharmNum;
    CHECK_ERROR();
    v_InstituteCode = GetShort( &PosInBuff, TInstituteCode_len );
    CHECK_ERROR();
    v_TerminalNum   = GetShort( &PosInBuff, TTerminalNum_len );
    CHECK_ERROR();
    /*
     * Get amount of input not eaten
     * -----------------------------
     */
    ChckInputLen( TotalInputLen - (PosInBuff - IBuffer) );
    CHECK_ERROR();
	/*=======================================================================
	||									||
	||			    SQL TRANSACTIONs.				||
	||									||
	||======================================================================||
	||			     Retries loop				||
	=======================================================================*/

    reStart = MAC_TRUE;

    sprintf( fileName, "%s/errmsg_%0*d_%0*d_%0*d.snd",
	     MsgDir,
	     TPharmNum_len,      v_PharmNum,
	     TInstituteCode_len, v_InstituteCode,
	     TTerminalNum_len,   v_TerminalNum );

    for(
	tries=0;
	tries < SQL_UPDATE_RETRIES && reStart == MAC_TRUE;
	tries++
	)
    {
	/*=======================================================================
	||		  Create new file for respond message.			||
	 =======================================================================*/
	outFP = fopen( fileName, "w" );

	if( outFP == NULL )
	{
	    int	err = errno;
	    GmsgLogReturn( GerrSource, __LINE__,
			   "\tCant open output file '%s'\n"
			   "\tError (%d) %s\n",
			   fileName, err, strerror(err));
		RESTORE_ISOLATION;
	    return( ERR_UNABLE_OPEN_OUTPUT_FILE );
	}
	/*=======================================================================
	||		       Init answer header variables			||
	 =======================================================================*/
	reStart			= MAC_FALS;
	v_ErrorCode		= NO_ERROR;
	v_UpdateDate		= 0;
	v_UpdateHour		= 0;
	v_ErrorCodeToUpdateNum	= 0;

	/*=======================================================================
	||			   Test pharmacy code.				||
	 =======================================================================*/
	do				/* Exiting from LOOP will send	*/
	{				/* the reply to phrmacy	*/
		SET_ISOLATION_DIRTY;

	    /*
	     * Test if opened
	     * --------------
	     */
	    v_ErrorCode = IS_PHARMACY_OPEN (v_PharmNum, v_InstituteCode, &Phrm_info);

	    if( v_ErrorCode != MAC_OK )
	    {
		break;
	    }
		/*=======================================================================
		||			 Get the modification date.			||
		 =======================================================================*/
		ExecSQL (	MAIN_DB, READ_TablesUpdate, &v_UpdateDate, &v_UpdateHour, &TableName, END_OF_ARG_LIST	);
	      
	    Conflict_Test( reStart );

	    if( SQLERR_code_cmp( SQLERR_not_found ) == MAC_TRUE )
	    {
			v_UpdateDate = 0;
			v_UpdateHour = 0;
	    }
	    else if( SQLERR_error_test() )
	    {
		if( SetErrorVar( &v_ErrorCode, ERR_DATABASE_ERROR ) )
		{
		    break;
		}
	    }
		/*=======================================================================
		||			   Open the cursor.				||
		 =======================================================================*/
		DeclareAndOpenCursor (	MAIN_DB, TR1043_error_messags_cur, END_OF_ARG_LIST	);

	    if( SQLERR_error_test() )
	    {
			SetErrorVar( &v_ErrorCode, ERR_DATABASE_ERROR );
			break;
	    }
		/*=======================================================================
		||		   Create the header with data so far.			||
		 =======================================================================*/
	    fprintf( outFP, "%0*d", MSG_ID_LEN,		1044 );

	    fprintf( outFP, "%0*d", MSG_ERROR_CODE_LEN, MAC_OK );

	    fprintf( outFP, "%0*d",TPharmNum_len,	v_PharmNum );

	    fprintf( outFP, "%0*d", TInstituteCode_len,	v_InstituteCode );

	    fprintf( outFP, "%0*d", TTerminalNum_len,	v_TerminalNum );

	    fprintf( outFP, "%0*d", TErrorCode_len,	v_ErrorCode );

	    fprintf( outFP, "%0*d",TUpdateDate_len,	v_UpdateDate );

	    fprintf( outFP, "%0*d",TUpdateHour_len,	v_UpdateHour );

	    fprintf( outFP, "%0*d", TErrorCodeToUpdateNum_len,
		     v_ErrorCodeToUpdateNum );
		/*=======================================================================
		||		     Write the messages data to file.			||
		 =======================================================================*/
	    for(;; )
	    {
		/*
		 * Fetch and test DB errors
		 * ------------------------
		 */
			FetchCursorInto (	MAIN_DB, TR1043_error_messags_cur,
								&v_ErrorCode_m, &v_ErrorDecription, &v_ErrorSevirity,	END_OF_ARG_LIST	);

		Conflict_Test( reStart );

		if( SQLERR_code_cmp( SQLERR_end_of_fetch ) == MAC_TRUE )
		{
		    break;
		}

		else if( SQLERR_error_test() )
		{
		    SetErrorVar( &v_ErrorCode, ERR_DATABASE_ERROR );
		    break;
		}
		/*
		 * Write the error-message rows
		 * ----------------------------
		 */
		v_ErrorCodeToUpdateNum++;

		fprintf( outFP, "%0*d",
			 TErrorCode_len,	v_ErrorCode_m );

		// DonR 26Nov2025 User Story #251264: Support more flexible Hebrew encoding output.
//		WinHebToDosHeb( ( unsigned char * )( v_ErrorDecription ) );
		EncodeHebrew	('W', Phrm_info.hebrew_encoding, sizeof (v_ErrorDecription), v_ErrorDecription);
		
		fprintf( outFP, "%-*.*s",
			 TErrorDecription_len,	TErrorDecription_len,
			 v_ErrorDecription );
		fprintf( outFP, "%0*d",
			 TErrorSevirity_len,	v_ErrorSevirity );

	    } /* for(;; ) */

		CloseCursor (	MAIN_DB, TR1043_error_messags_cur	);
	    SQLERR_error_test();
	    
	} while( 0 );		/* DO loop Shuld run only once	*/
	/*=======================================================================
	||			  Commit the transaction.			||
	 =======================================================================*/
	if( reStart == MAC_FALS )	/* Tranaction OK then COMMIT	*/
	{
		CommitAllWork ();
	    SQLERR_error_test();
	}
	else
	{
	    if( TransactionRestart() != MAC_OK )
	    {
			v_ErrorCode = ERR_DATABASE_ERROR;
			break;
	    }

	    GerrLogReturn(
			  GerrId,
			  "Table is locked for the <%d> time",
			  tries
			  );

	    sleep( ACCESS_CONFLICT_SLEEP_TIME );
	    fclose( outFP );
	}

    }	/* End of retries loop */

    if( reStart == MAC_TRUE )
    {
	GerrLogReturn(
		      GerrId,
		      "Locked for <%d> times",
		      SQL_UPDATE_RETRIES
		      );

	SetErrorVar( &v_ErrorCode, ERR_DATABASE_ERROR );
    }
    
	/*=======================================================================
	||									||
	||			REWRITE  MESSAGE HEADER.			||
	||									||
	 =======================================================================*/

    rewind( outFP );

    fprintf( outFP, "%0*d", MSG_ID_LEN,		1044 );

    fprintf( outFP, "%0*d", MSG_ERROR_CODE_LEN, MAC_OK );

    fprintf( outFP, "%0*d",TPharmNum_len,	v_PharmNum );

    fprintf( outFP, "%0*d", TInstituteCode_len,	v_InstituteCode );

    fprintf( outFP, "%0*d", TTerminalNum_len,	v_TerminalNum );

    fprintf( outFP, "%0*d", TErrorCode_len,	v_ErrorCode );

    fprintf( outFP, "%0*d",TUpdateDate_len,	v_UpdateDate );

    fprintf( outFP, "%0*d",TUpdateHour_len,	v_UpdateHour );

    fprintf( outFP, "%0*d", TErrorCodeToUpdateNum_len,
	     v_ErrorCodeToUpdateNum );

    fclose( outFP );

	/*
	 * Write the name of output-file on string for T-SWITCH
	 * ----------------------------------------------------
	 */

    *p_outputWritten = sprintf( OBuffer, "%s", fileName );
    *output_type_flg = ANSWER_IN_FILE;
    ssmd_data_ptr->error_code = v_ErrorCode;
    ssmd_data_ptr->pharmacy_num = v_PharmNum;
    ssmd_data_ptr->terminal_num = v_TerminalNum;

	RESTORE_ISOLATION;

    return( RC_SUCCESS );

}
/* end of 1043 handler */



/*=======================================================================
||									||
||			  HandlerToMsg_1047				||
||	   Message handler for message 1047 discount-table update.	||
||									||
||									||
 =======================================================================*/
int   HandlerToMsg_1047( char   *IBuffer,         
			 int	TotalInputLen,    
			 char   *OBuffer,         
			 int    *p_outputWritten, 
			 int    *output_type_flg,
			 SSMD_DATA	*ssmd_data_ptr
			 )
{
	/*=======================================================================
	||			    Local declarations				||
	 =======================================================================*/
    char			fileName[256];
    int				tries,
					reStart;
    PHARMACY_INFO	Phrm_info;
    FILE			*outFP;
	ISOLATION_VAR;
	/*=======================================================================
	||		       Message fields variables				||
	 =======================================================================*/
    /*
     * INPUT MESSAGE
     * =============
     */
    short			v_ErrorCode;
    int				v_PharmNum;
    short			v_InstituteCode;
    short			v_TerminalNum;
    /*
     * RETURN MESSAGE ONLY
     * ===================
     * header
     * ------
     */
	char				TableName [33] = "Member_price";	// DonR 19Aug2020: Change from char * to array, since ODBC doesn't like string constants.
    int					v_UpdateDate;
    int					v_UpdateHour;
    short				v_NumOfLinesToUpdate;
    /*
     * lines
     * -----
     */
    short				v_ParticipatingType;
    short				v_ParticipatingPrecent;
    int					v_ParticipatingAmount;		// DonR 26Feb2020: Was typedef'ed as a SHORT - but it's an integer in the database.
    char				v_ParticipationName	[25 + 1];
    short				v_CalcTypeCode;
    short				v_MemberBelongCode;
    short				v_YarpaParticipationCode;
	int					v_max_pkg_price;


	/*=======================================================================
	||									||
	||			    Body of function				||
	||									||
	||======================================================================||
	||		       Init answer header variables			||
	=======================================================================*/
	REMEMBER_ISOLATION;
    PosInBuff		 = IBuffer;
    v_ErrorCode		 = NO_ERROR;
    v_UpdateDate	 = 0;
    v_UpdateHour	 = 0;
    v_NumOfLinesToUpdate = 0;

	/*=======================================================================
	||									||
	||		      Get message fields data into variables		||
	||									||
	 =======================================================================*/
    v_PharmNum	    = GetLong( &PosInBuff,  TPharmNum_len );
    ssmd_data_ptr->pharmacy_num = v_PharmNum;
    CHECK_ERROR();
    v_InstituteCode = GetShort( &PosInBuff, TInstituteCode_len );
    CHECK_ERROR();
    v_TerminalNum   = GetShort( &PosInBuff, TTerminalNum_len );
    CHECK_ERROR();
    /*
     * Get amount of input not eaten
     * -----------------------------
     */
    ChckInputLen( TotalInputLen - (PosInBuff - IBuffer) );
    CHECK_ERROR();
	/*=======================================================================
	||									||
	||			    SQL TRANSACTIONs.				||
	||									||
	||======================================================================||
	||			     Retries loop				||
	=======================================================================*/

    reStart = MAC_TRUE;

    sprintf( fileName, "%s/member_price_%0*d_%0*d_%0*d.snd",
	     MsgDir,
	     TPharmNum_len,      v_PharmNum,
	     TInstituteCode_len, v_InstituteCode,
	     TTerminalNum_len,   v_TerminalNum );

    for(
	tries=0;
	tries < SQL_UPDATE_RETRIES && reStart == MAC_TRUE;
	tries++
	)
    {
	/*=======================================================================
	||		  Create new file for respond message.			||
	 =======================================================================*/
	outFP = fopen( fileName, "w" );

	if( outFP == NULL )
	{
	    int	err = errno;
	    GmsgLogReturn( GerrSource, __LINE__,
			   "\tCant open output file '%s'\n"
			   "\tError (%d) %s\n",
			   fileName, err, strerror(err));
		RESTORE_ISOLATION;
	    return( ERR_UNABLE_OPEN_OUTPUT_FILE );
	}
	/*=======================================================================
	||		       Init answer header variables			||
	 =======================================================================*/
	reStart			= MAC_FALS;
	v_ErrorCode		= NO_ERROR;
	v_UpdateDate		= 0;
	v_UpdateHour		= 0;
	v_NumOfLinesToUpdate	= 0;

//	if( TransactionBegin() != MAC_OK )
//	{
//	    v_ErrorCode = ERR_DATABASE_ERROR;
//	    break;
//	}
//
	/*=======================================================================
	||			 Test if pharmacy open.				||
	 =======================================================================*/
	do				/* Exiting from LOOP will send	*/
	{				/* the reply to phrmacy	*/
		SET_ISOLATION_DIRTY;

	    /*
	     * Test if opened
	     * --------------
	     */
	    v_ErrorCode = IS_PHARMACY_OPEN (v_PharmNum, v_InstituteCode, &Phrm_info);

	    if( v_ErrorCode != MAC_OK )
	    {
			break;
	    }
		/*=======================================================================
		||			 Get the modification date.			||
		 =======================================================================*/
		ExecSQL (	MAIN_DB, READ_TablesUpdate, &v_UpdateDate, &v_UpdateHour, &TableName, END_OF_ARG_LIST	);
	      
	    Conflict_Test( reStart );

	    if( SQLERR_code_cmp( SQLERR_not_found ) == MAC_TRUE )
	    {
			v_UpdateDate = 0;
			v_UpdateHour = 0;
	    }
	    else if( SQLERR_error_test() )
	    {
			if( SetErrorVar( &v_ErrorCode, ERR_DATABASE_ERROR ) )
			{
				break;
			}
	    }
		/*=======================================================================
		||			   Open the cursor.				||
		 =======================================================================*/
		DeclareAndOpenCursor (	MAIN_DB, TR1047_member_pric_up_cur, END_OF_ARG_LIST	);

	    if( SQLERR_error_test() )
	    {
			SetErrorVar( &v_ErrorCode, ERR_DATABASE_ERROR );
			break;
	    }

		/*=======================================================================
		||		   Create the header with data so far.			||
		 =======================================================================*/
	    fprintf( outFP, "%0*d", MSG_ID_LEN,		1048 );

	    fprintf( outFP, "%0*d", MSG_ERROR_CODE_LEN, MAC_OK );

	    fprintf( outFP, "%0*d",TPharmNum_len,	v_PharmNum );

	    fprintf( outFP, "%0*d", TInstituteCode_len,	v_InstituteCode );

	    fprintf( outFP, "%0*d", TTerminalNum_len,	v_TerminalNum );

	    fprintf( outFP, "%0*d", TErrorCode_len,	v_ErrorCode );

	    fprintf( outFP, "%0*d",TUpdateDate_len,	v_UpdateDate );

	    fprintf( outFP, "%0*d",TUpdateHour_len,	v_UpdateHour );

	    fprintf( outFP, "%0*d", TNumOfLinesToUpdate_len,
		     v_NumOfLinesToUpdate );

		/*=======================================================================
		||		     Write the messages data to file.			||
		 =======================================================================*/
	    for( ; /* for ever */ ; )
	    {
		/*
		 * Fetch and test DB errors
		 * ------------------------
		 */
			FetchCursorInto (	MAIN_DB, TR1047_member_pric_up_cur,
								&v_ParticipatingType,		&v_ParticipatingPrecent,	&v_ParticipatingAmount,
								&v_ParticipationName,		&v_CalcTypeCode,			&v_MemberBelongCode,
								&v_YarpaParticipationCode,	&v_max_pkg_price,			END_OF_ARG_LIST				);

		Conflict_Test( reStart );

		if( SQLERR_code_cmp( SQLERR_end_of_fetch ) == MAC_TRUE )
		{
		    break;
		}
		else if( SQLERR_error_test() )
		{
		    SetErrorVar( &v_ErrorCode, ERR_DATABASE_ERROR );
		    break;
		}

		/*
		 * Write the discount rows
		 * -----------------------
		 */
		v_NumOfLinesToUpdate++;

		fprintf( outFP, "%0*d",
			 TParticipatingType_len,
			 v_ParticipatingType );
		
		fprintf( outFP, "%0*d",
			 TParticipatingPrecent_len,
			 v_ParticipatingPrecent );
			 
		fprintf( outFP, "%0*d",
			 TParticipatingAmount_len,
			 v_ParticipatingAmount );
		
		WinHebToDosHeb( ( unsigned char * )( v_ParticipationName ) );

		// DonR 16Oct2006: Per Iris Shaya, truncate Participation Type Name
		// from 25 to 20 characters.
		fprintf( outFP, "%0-*.*s",
			 TParticipationName_len - 5,
			 TParticipationName_len - 5,
			 v_ParticipationName );

		// DonR 16Oct2006: Per Iris Shaya, send 9-character Maximum Package
		// Price instead of last five characters of the Ptn. Type Name and
		// the four-character (and not used) Calculation Type Code.
		fprintf( outFP, "%0*d",
			 9,
			 v_max_pkg_price );

		fprintf( outFP, "%0*d",
			 TMemberBelongCode_len,
			 v_MemberBelongCode );
		
		fprintf( outFP, "%0*d",
			 TYarpaParticipationCode_len,
			 v_YarpaParticipationCode );
		
	    } /* for( ; ; ) */

		CloseCursor (	MAIN_DB, TR1047_member_pric_up_cur	);

	    SQLERR_error_test();
	    
	}
	while (0);		/* Loop should run only once	*/

	/*=======================================================================
	||			  Commit the transaction.			||
	 =======================================================================*/
	if( reStart == MAC_FALS )	/* Tranaction OK then COMMIT	*/
	{
		CommitAllWork ();

	    SQLERR_error_test();
	}
	else				/* Transaction ERR then ROLLBACK*/
	{
	    if( TransactionRestart() != MAC_OK )
	    {
			v_ErrorCode = ERR_DATABASE_ERROR;
			break;
	    }

	    GerrLogReturn(
			  GerrId,
			  "Table is locked for the <%d> time",
			  tries
			  );

	    sleep( ACCESS_CONFLICT_SLEEP_TIME );

	    fclose( outFP );
	}

    }	/* End of retries loop */

    if( reStart == MAC_TRUE )
    {
	GerrLogReturn(
		      GerrId,
		      "Locked for <%d> times",
		      SQL_UPDATE_RETRIES
		      );

	SetErrorVar( &v_ErrorCode, ERR_DATABASE_ERROR );
    }
    
	/*=======================================================================
	||									||
	||			REWRITE  MESSAGE HEADER.			||
	||									||
	 =======================================================================*/

    rewind( outFP );

    fprintf( outFP, "%0*d", MSG_ID_LEN,		1048 );

    fprintf( outFP, "%0*d", MSG_ERROR_CODE_LEN, MAC_OK );

    fprintf( outFP, "%0*d",TPharmNum_len,	v_PharmNum );

    fprintf( outFP, "%0*d", TInstituteCode_len,	v_InstituteCode );

    fprintf( outFP, "%0*d", TTerminalNum_len,	v_TerminalNum );

    fprintf( outFP, "%0*d", TErrorCode_len,	v_ErrorCode );

    fprintf( outFP, "%0*d",TUpdateDate_len,	v_UpdateDate );

    fprintf( outFP, "%0*d",TUpdateHour_len,	v_UpdateHour );

    fprintf( outFP, "%0*d", TNumOfLinesToUpdate_len,
	     v_NumOfLinesToUpdate );
    fclose( outFP );

	/*
	 * Write the name of output-file on string for T-SWITCH
	 * ----------------------------------------------------
	 */
    *p_outputWritten = sprintf( OBuffer, "%s", fileName );
    *output_type_flg = ANSWER_IN_FILE;
    ssmd_data_ptr->error_code = v_ErrorCode;
    ssmd_data_ptr->pharmacy_num = v_PharmNum;
    ssmd_data_ptr->terminal_num = v_TerminalNum;

	RESTORE_ISOLATION;

    return( RC_SUCCESS );

}
/* end of handler */



/*=======================================================================
||									||
||			  Handle.rToMsg_1049				||
||	   Message handler for message 1049 supplier table update.	||
||									||
||									||
 =======================================================================*/
int   HandlerToMsg_1049( char   *IBuffer,         
			 int	TotalInputLen,    
			 char   *OBuffer,         
			 int    *p_outputWritten, 
			 int    *output_type_flg,
			 SSMD_DATA	*ssmd_data_ptr
			 )
{
	/*=======================================================================
	||			    Local declarations				||
	 =======================================================================*/
    char			fileName[256];
    int				tries,
					reStart;
    PHARMACY_INFO	Phrm_info;
    FILE			*outFP;
	ISOLATION_VAR;
	/*=======================================================================
	||		       Message fields variables				||
	 =======================================================================*/
    /*
     * INPUT MESSAGE
     * =============
     */
    int					v_PharmNum;
    short				v_InstituteCode;
    short				v_TerminalNum;
	char				TableName [33] = "Suppliers";	// DonR 19Aug2020: Change from char * to array, since ODBC doesn't like string constants.
    int					v_LastUpdateDate;
    int					v_LastUpdateHour;
    /*
     * RETURN MESSAGE ONLY
     * ===================
     * header
     * ------
     */
    short				v_ErrorCode;
    int					v_UpdateDate;
    int					v_UpdateHour;
    int					v_LastDate;
    int					v_LastHour;
    short				v_NumOfLinesToUpdate;
    /*
     * lines
     * -----
     */
    short				v_SupplierTableCod;
    short				v_ActionCode;
    int					v_SupplierNum;
    char				v_SupplierName		[25 + 1];
    char				v_SupplierType		[ 2 + 1];
    char				v_Address			[20 + 1];
    char				v_City				[20 + 1];	// Output is 15 characters, but it's 20 characters in the database.
    int					v_ZipCode;
    char				v_Phone1			[10 + 1];
    char				v_Phone2			[10 + 1];
    char				v_FaxNum			[10 + 1];
    int					v_CommunicationSupplier;
    int					v_EmployeeID;
    char				v_AllowedProcList	[50 + 1];
    int					v_EmployeePassword;
    short				v_CheckType;


	// DonR 05May2019 CR #14710: There are four new "worker" fields in RK9019. Because
	// we are using "old" slots in Transaction 1049 to send these values to pharmacies,
	// we're just swapping them into the regular variables in CL.EC and storing them
	// in the "old" database columns in Linux. Here are the mappings:
	//		D4M0UB		worker_job_desc		==> street_and_no	(instead of D4ADDR)
	//		D4MSPPHARM	worker_pharm_code	==> phone_1			(instead of D4TEL)
	//		D4Y3KS		worker_card_num		==> fax_num			(instead of D4FAX)
	//		D4YPKS		worker_job_code		==> check_type		(instead of D4PRMTCDE)
	// Note that while I cleaned up some of the code here visually, there are no functional
	// changes in Transaction 1049; all the changes are on the AS/400 side in CL.EC.


	/*=======================================================================
	||									||
	||			    Body of function				||
	||									||
	||======================================================================||
	||		       Init answer header variables			||
	=======================================================================*/
	REMEMBER_ISOLATION;
    PosInBuff		 = IBuffer;
    v_ErrorCode		 = NO_ERROR;
    v_UpdateDate	 = 0;
    v_UpdateHour	 = 0;
    v_NumOfLinesToUpdate = 0;

	/*=======================================================================
	||									||
	||		      Get message fields data into variables		||
	||									||
	 =======================================================================*/
    v_PharmNum	     = GetLong( &PosInBuff,  TPharmNum_len );
    ssmd_data_ptr->pharmacy_num = v_PharmNum;
    CHECK_ERROR();
    v_InstituteCode  = GetShort( &PosInBuff, TInstituteCode_len );
    CHECK_ERROR();
    v_TerminalNum    = GetShort( &PosInBuff, TTerminalNum_len );
    CHECK_ERROR();
    v_LastUpdateDate = GetLong( &PosInBuff,  TLastUpdateDate_len );
    CHECK_ERROR();
    ChckDate( v_LastUpdateDate );
    CHECK_ERROR();
    v_LastUpdateHour = GetLong( &PosInBuff,  TLastUpdateHour_len );
    CHECK_ERROR();
    ChckTime( v_LastUpdateHour );
    CHECK_ERROR();
    /*
     * Get amount of input not eaten
     * -----------------------------
     */
    ChckInputLen( TotalInputLen - (PosInBuff - IBuffer) );
    CHECK_ERROR();
	/*=======================================================================
	||									||
	||			    SQL TRANSACTIONs.				||
	||									||
	||======================================================================||
	||			     Retries loop				||
	=======================================================================*/

    reStart = MAC_TRUE;

    sprintf( fileName, "%s/suppliers_%0*d_%0*d_%0*d.snd",
	     MsgDir,
	     TPharmNum_len,      v_PharmNum,
	     TInstituteCode_len, v_InstituteCode,
	     TTerminalNum_len,   v_TerminalNum );

    for(
	tries=0;
	tries < SQL_UPDATE_RETRIES && reStart == MAC_TRUE;
	tries++
	)
    {
	/*=======================================================================
	||		  Create new file for respond message.			||
	 =======================================================================*/
	outFP = fopen( fileName, "w" );

	if( outFP == NULL )
	{
	    int	err = errno;
	    GmsgLogReturn( GerrSource, __LINE__,
			   "\tCant open output file '%s'\n"
			   "\tError (%d) %s\n",
			   fileName, err, strerror(err));
		RESTORE_ISOLATION;
	    return( ERR_UNABLE_OPEN_OUTPUT_FILE );
	}
	/*=======================================================================
	||		       Init answer header variables			||
	 =======================================================================*/
	reStart			= MAC_FALS;
	v_ErrorCode		= NO_ERROR;
	v_UpdateDate		= 0;
	v_UpdateHour		= 0;
	v_NumOfLinesToUpdate	= 0;

//	if( TransactionBegin() != MAC_OK )
//	{
//	    v_ErrorCode = ERR_DATABASE_ERROR;
//	    break;
//	}
	/*=======================================================================
	||			 Test if pharmacy open.				||
	 =======================================================================*/
	do				/* Exiting from LOOP will send	*/
	{				/* the reply to phrmacy	*/
		SET_ISOLATION_DIRTY;

	    /*
	     * Test if opened
	     * --------------
	     */
	    v_ErrorCode = IS_PHARMACY_OPEN (v_PharmNum, v_InstituteCode, &Phrm_info);
	    if( v_ErrorCode != MAC_OK )
	    {
		break;
	    }
	    /*
	     * Test pharmacy type (private/macabi)
	     * ----------------------------------
	     */
	    if( PRIVATE_PHARMACY )
	    {
			if( SetErrorVar(&v_ErrorCode, ERR_PHARMACY_HAS_NO_PERMISSION ))
			{
				break;
			}
	    }

		/*=======================================================================
		||			 Get the modification date.			||
		 =======================================================================*/
		ExecSQL (	MAIN_DB, READ_TablesUpdate, &v_UpdateDate, &v_UpdateHour, &TableName, END_OF_ARG_LIST	);
	      
	    Conflict_Test( reStart );

	    if( SQLERR_code_cmp( SQLERR_not_found ) == MAC_TRUE )
	    {
			v_UpdateDate = 0;
			v_UpdateHour = 0;
	    }
	    else if( SQLERR_error_test() )
	    {
			if( SetErrorVar( &v_ErrorCode, ERR_DATABASE_ERROR ) )
			{
				break;
			}
	    }

		/*=======================================================================
		||			   Open the cursor.				||
		 =======================================================================*/
		DeclareAndOpenCursorInto (	MAIN_DB, TR1049_supplier_up_cur, ChooseNewlyUpdatedRows,
									&v_ActionCode,		&v_SupplierTableCod,	&v_SupplierNum,
									&v_SupplierName,	&v_SupplierType,		&v_Address,
									&v_City,			&v_ZipCode,				&v_Phone1,

									&v_Phone2,			&v_FaxNum,				&v_CommunicationSupplier,
									&v_EmployeeID,		&v_AllowedProcList,		&v_EmployeePassword,
									&v_CheckType,		&v_LastDate,			&v_LastHour,

									&v_LastUpdateDate,	&v_LastUpdateHour,		&v_LastUpdateDate,
									END_OF_ARG_LIST															);

	    if( SQLERR_error_test() )
	    {
			SetErrorVar( &v_ErrorCode, ERR_DATABASE_ERROR );
			break;
	    }

		// Create the header with data so far.
		fprintf (outFP, "%0*d", MSG_ID_LEN,				1050 );
		fprintf (outFP, "%0*d", MSG_ERROR_CODE_LEN,		MAC_OK );
		fprintf (outFP, "%0*d", TPharmNum_len,			v_PharmNum );
		fprintf (outFP, "%0*d", TInstituteCode_len,		v_InstituteCode );
		fprintf (outFP, "%0*d", TTerminalNum_len,		v_TerminalNum );
		fprintf (outFP, "%0*d", TErrorCode_len,			v_ErrorCode );
		fprintf (outFP, "%0*d", TUpdateDate_len,		v_UpdateDate );
		fprintf (outFP, "%0*d", TUpdateHour_len,		v_UpdateHour );
		fprintf (outFP, "%0*d", 4,						v_NumOfLinesToUpdate );

		// DonR 01Sep2010: Since it's entirely possible that we won't find any updated
		// rows, set up a default value for v_LastDate and v_LastHour; this will prevent
		// sending garbage values to pharmacies in the "last update date/time" fields.
		v_LastDate = v_UpdateDate;
		v_LastHour = v_UpdateHour;

		/*=======================================================================
		||		     Write the messages data to file.			||
		 =======================================================================*/
	    for( ; /* for ever */ ; )
	    {
			// Fetch and test DB errors
			FetchCursor (	MAIN_DB, TR1049_supplier_up_cur	);

			Conflict_Test( reStart );

			if ((SQLERR_code_cmp (SQLERR_end_of_fetch) == MAC_TRUE)	||
				(v_NumOfLinesToUpdate > 9999))
			{
				break;
			}
			else if( SQLERR_error_test() )
			{
				SetErrorVar( &v_ErrorCode, ERR_DATABASE_ERROR );
				break;
			}

			// Write the output rows
			v_NumOfLinesToUpdate++;

			WinHebToDosHeb( ( unsigned char * )( v_SupplierName ) );
			WinHebToDosHeb( ( unsigned char * )( v_Address ) );
			WinHebToDosHeb( ( unsigned char * )( v_City ) );

			fprintf (outFP, "%0*d",		TSupplierTableCod_len,							v_SupplierTableCod );		//  1, L = 2. 2 == workers.
			fprintf (outFP, "%0*d",		TActionCode_len,								v_ActionCode );				//  2
			fprintf (outFP, "%0*d",		TSupplierNum_len,								v_SupplierNum );			//  3
			fprintf (outFP, "%-*.*s",	TSupplierName_len,		TSupplierName_len,		v_SupplierName );			//  4
			fprintf (outFP, "%-*.*s",	TSupplierType_len,		TSupplierType_len,		v_SupplierType );			//  5
			fprintf (outFP, "%-*.*s",	TAddress_len,			TAddress_len,			v_Address );				//  6
			fprintf (outFP, "%-*.*s",	TCity15_len,			TCity_len,				v_City );					//  7
			fprintf (outFP, "%0*d",		TZipCode_len,									v_ZipCode );				//  8
			fprintf (outFP, "%-*.*s",	TPhone1_len,			TPhone1_len,			v_Phone1 );					//  9
			fprintf (outFP, "%-*.*s",	TPhone2_len,			TPhone2_len,			v_Phone2 );					// 10
			fprintf (outFP, "%-*.*s",	TFaxNum_len,			TFaxNum_len,			v_FaxNum );					// 11
			fprintf (outFP, "%0*d",		5,												v_CommunicationSupplier );	// 12
			fprintf (outFP, "%0*d",		TEmployeeID_len,								v_EmployeeID );				// 13
			fprintf (outFP, "%-*.*s",	TAllowedProcList_len,	TAllowedProcList_len,	v_AllowedProcList );		// 14
			fprintf (outFP, "%0*d",		TEmployeePassword_len,							v_EmployeePassword );		// 15
			fprintf (outFP, "%0*d",		TCheckType_len,									v_CheckType );				// 16

	    } /* for( ; ; ) */

		CloseCursor (	MAIN_DB, TR1049_supplier_up_cur	);
	    SQLERR_error_test();
	    
	} while( 0 );		/* DO loop should run only once	*/
	/*=======================================================================
	||			  Commit the transaction.			||
	 =======================================================================*/
	if( reStart == MAC_FALS )	/* Tranaction OK then COMMIT	*/
	{
		CommitAllWork ();

	    SQLERR_error_test();
	}
	else
	{
	    if( TransactionRestart() != MAC_OK )
	    {
			v_ErrorCode = ERR_DATABASE_ERROR;
			break;
	    }

	    GerrLogReturn(
			  GerrId,
			  "Table is locked for the <%d> time",
			  tries
			  );

	    sleep( ACCESS_CONFLICT_SLEEP_TIME );
	    fclose( outFP );
	}
    }	/* End of retries loop */

    if( reStart == MAC_TRUE )
    {
	GerrLogReturn(
		      GerrId,
		      "Locked for <%d> times",
		      SQL_UPDATE_RETRIES
		      );

	SetErrorVar( &v_ErrorCode, ERR_DATABASE_ERROR );
    }
    
	/*=======================================================================
	||									||
	||			REWRITE  MESSAGE HEADER.			||
	||									||
	 =======================================================================*/

    rewind( outFP );

	fprintf (outFP, "%0*d", MSG_ID_LEN,				1050 );
	fprintf (outFP, "%0*d", MSG_ERROR_CODE_LEN,		MAC_OK );
	fprintf (outFP, "%0*d", TPharmNum_len,			v_PharmNum );
	fprintf (outFP, "%0*d", TInstituteCode_len,		v_InstituteCode );
	fprintf (outFP, "%0*d", TTerminalNum_len,		v_TerminalNum );
	fprintf (outFP, "%0*d", TErrorCode_len,			v_ErrorCode );
	fprintf (outFP, "%0*d", TUpdateDate_len,		v_LastDate );
	fprintf (outFP, "%0*d", TUpdateHour_len,		v_LastHour );
	fprintf (outFP, "%0*d", 4,						v_NumOfLinesToUpdate );

    fclose( outFP );

	/*
	 * Write the name of output-file on string for T-SWITCH
	 * ----------------------------------------------------
	 */
    *p_outputWritten = sprintf( OBuffer, "%s", fileName );
    *output_type_flg = ANSWER_IN_FILE;
    ssmd_data_ptr->error_code = v_ErrorCode;
    ssmd_data_ptr->pharmacy_num = v_PharmNum;
    ssmd_data_ptr->terminal_num = v_TerminalNum;

	RESTORE_ISOLATION;

    return( RC_SUCCESS );

}
/* end of handler */



/*=======================================================================
||									||
||			  HandlerToMsg_1051				||
||	   Message handler for message 1051 centers table update.	||
||									||
||									||
 =======================================================================*/
int   HandlerToMsg_1051( char   *IBuffer,         
			 int	TotalInputLen,    
			 char   *OBuffer,         
			 int    *p_outputWritten, 
			 int    *output_type_flg,
			 SSMD_DATA	*ssmd_data_ptr
			 )
{
	/*=======================================================================
	||			    Local declarations				||
	 =======================================================================*/
    char			fileName[256];
    int				tries,
					reStart;
    PHARMACY_INFO	Phrm_info;
    FILE			*outFP;
	ISOLATION_VAR;
	/*=======================================================================
	||		       Message fields variables				||
	 =======================================================================*/
    /*
     * INPUT MESSAGE
     * =============
     */
    int					v_PharmNum;
    short				v_InstituteCode;
    short				v_TerminalNum;
    int					v_LastUpdateDate;
    int					v_LastUpdateHour;
    /*
     * RETURN MESSAGE ONLY
     * ===================
     * header
     * ------
     */
    short				v_ErrorCode;
    int					v_UpdateDate;
    int					v_UpdateHour;
	char				TableName [33] = "Macabi_centers";	// DonR 19Aug2020: Change from char * to array, since ODBC doesn't like string constants.
    short				v_NumOfLinesToUpdate;
    /*
     * lines
     * -----
     */
    short				v_ActionCode;					// DonR 09Feb2020: Was int, but maps to del_flg, which is a SMALLINT. Possible source of ODBC problems!
    int					v_DeviceNum;
    char				v_MainDeviceType	[ 2 + 1];
    char				v_SemiDeviceType	[ 2 + 1];
    char				v_DeviceName		[40 + 1];
    char				v_Address			[20 + 1];
    char				v_City				[20 + 1];
    int					v_ZipCode;
    char				v_Phone1			[10 + 1];
    char				v_AssutaCardNumber	[10 + 1];	// DonR 09Sep2009: Replaces Phone2 (Assuta Pharmacy)
    char				v_FaxNum			[10 + 1];
    short				v_DiscountPrecent;				// DonR 09Feb2020: Was typedef'ed int - maybe the source of ODBC problems!
    short				v_CreditYesNoFlg;
    char				v_AllowedProcList	[50 + 1];
    char				v_AllowedBelongings	[50 + 1];

	/*=======================================================================
	||									||
	||			    Body of function				||
	||									||
	||======================================================================||
	||		       Init answer header variables			||
	=======================================================================*/
	REMEMBER_ISOLATION;
    PosInBuff		 = IBuffer;
    v_ErrorCode		 = NO_ERROR;
    v_UpdateDate	 = 0;
    v_UpdateHour	 = 0;
    v_NumOfLinesToUpdate = 0;
	/*=======================================================================
	||									||
	||		      Get message fields data into variables		||
	||									||
	 =======================================================================*/
    v_PharmNum	     = GetLong( &PosInBuff,  TPharmNum_len );
    ssmd_data_ptr->pharmacy_num = v_PharmNum;
    CHECK_ERROR();
    v_InstituteCode  = GetShort( &PosInBuff, TInstituteCode_len );
    CHECK_ERROR();
    v_TerminalNum    = GetShort( &PosInBuff, TTerminalNum_len );
    CHECK_ERROR();
    v_LastUpdateDate = GetLong( &PosInBuff,  TLastUpdateDate_len );
    CHECK_ERROR();
    ChckDate( v_LastUpdateDate );
    CHECK_ERROR();
    v_LastUpdateHour = GetLong( &PosInBuff,  TLastUpdateHour_len );
    CHECK_ERROR();
    ChckTime( v_LastUpdateHour );
    CHECK_ERROR();
    /*
     * Get amount of input not eaten
     * -----------------------------
     */
    ChckInputLen( TotalInputLen - (PosInBuff - IBuffer) );
    CHECK_ERROR();
	/*=======================================================================
	||									||
	||			    SQL TRANSACTIONs.				||
	||									||
	||======================================================================||
	||			     Retries loop				||
	=======================================================================*/

    reStart = MAC_TRUE;

    sprintf( fileName, "%s/centers_%0*d_%0*d_%0*d.snd",
	     MsgDir,
	     TPharmNum_len,      v_PharmNum,
	     TInstituteCode_len, v_InstituteCode,
	     TTerminalNum_len,   v_TerminalNum );

    for(
	tries=0;
	tries < SQL_UPDATE_RETRIES && reStart == MAC_TRUE;
	tries++
	)
    {
	/*=======================================================================
	||		  Create new file for respond message.			||
	 =======================================================================*/
	outFP = fopen( fileName, "w" );

	if( outFP == NULL )
	{
	    int	err = errno;
	    GmsgLogReturn( GerrSource, __LINE__,
			   "\tCant open output file '%s'\n"
			   "\tError (%d) %s\n",
			   fileName, err, strerror(err));
		RESTORE_ISOLATION;
	    return( ERR_UNABLE_OPEN_OUTPUT_FILE );
	}
	/*=======================================================================
	||		       Init answer header variables			||
	 =======================================================================*/
	reStart			= MAC_FALS;
	v_ErrorCode		= NO_ERROR;
	v_UpdateDate		= 0;        
	v_UpdateHour		= 0;
	v_NumOfLinesToUpdate	= 0;

	/*=======================================================================
	||			 Test if pharmacy open.				||
	 =======================================================================*/
	do				/* Exiting from LOOP will send	*/
	{				/* the reply to pharmacy	*/
		SET_ISOLATION_DIRTY;

	    /*
	     * Test if opened
	     * --------------
	     */
	    v_ErrorCode = IS_PHARMACY_OPEN (v_PharmNum, v_InstituteCode, &Phrm_info);

	    if( v_ErrorCode != MAC_OK )
	    {
		break;
	    }
	    /*
	     * Test pharmacy type (private/macabi)
	     * ----------------------------------
	     */
	    if( PRIVATE_PHARMACY )
	    {
		if( SetErrorVar(&v_ErrorCode, ERR_PHARMACY_HAS_NO_PERMISSION ))
		{
		    break;
		}
	    }
		/*=======================================================================
		||			 Get the modification date.			||
		 =======================================================================*/
		ExecSQL (	MAIN_DB, READ_TablesUpdate, &v_UpdateDate, &v_UpdateHour, &TableName, END_OF_ARG_LIST	);
	      
	    Conflict_Test( reStart );

	    if( SQLERR_code_cmp( SQLERR_not_found ) == MAC_TRUE )
	    {
		v_UpdateDate = 0;
		v_UpdateHour = 0;
	    }
	    else if( SQLERR_error_test() )
	    {
		if( SetErrorVar( &v_ErrorCode, ERR_DATABASE_ERROR ) )
		{
		    break;
		}
	    }
		/*=======================================================================
		||			   Open the cursor.				||
		 =======================================================================*/
		DeclareAndOpenCursor (	MAIN_DB, TR1051_centers_up_cur, ChooseNewlyUpdatedRows,
								&v_LastUpdateDate, &v_LastUpdateHour, &v_LastUpdateDate, END_OF_ARG_LIST	);
if (SQLCODE != 0) GerrLogMini(GerrId, "Opened TR1051_centers_up_cur, Last Update = %d/%d, SQLCODE = %d.", v_LastUpdateDate, v_LastUpdateHour, SQLCODE);

	    if (SQLERR_error_test ())
	    {
			SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR);
			break;
	    }

		/*=======================================================================
		||		   Create the header with data so far.			||
		 =======================================================================*/
	    fprintf( outFP, "%0*d", MSG_ID_LEN,		1052 );

	    fprintf( outFP, "%0*d", MSG_ERROR_CODE_LEN, MAC_OK );

	    fprintf( outFP, "%0*d",TPharmNum_len,	v_PharmNum );

	    fprintf( outFP, "%0*d", TInstituteCode_len,	v_InstituteCode );

	    fprintf( outFP, "%0*d", TTerminalNum_len,	v_TerminalNum );

	    fprintf( outFP, "%0*d", TErrorCode_len,	v_ErrorCode );

	    fprintf( outFP, "%0*d",TUpdateDate_len,	v_UpdateDate );

	    fprintf( outFP, "%0*d",TUpdateHour_len,	v_UpdateHour );

	    fprintf( outFP, "%0*d", TNumOfLinesToUpdate_len,
		     v_NumOfLinesToUpdate );
		/*=======================================================================
		||		     Write the messages data to file.			||
		 =======================================================================*/
	    for( ; /* for ever */ ; )
	    {
			// Fetch and test DB errors
			// DonR 12Mar2020: FETCH commands don't need the custom-WHERE-clause identifier,
			// but the ODBC routine is supposed to be smart enough to skip past it if it
			// is supplied. For now at least, include this parameter to see if that logic
			// is working properly.
			FetchCursorInto (	MAIN_DB, TR1051_centers_up_cur, ChooseNewlyUpdatedRows,
//			FetchCursorInto (	MAIN_DB, TR1051_centers_up_cur,
								&v_ActionCode,			&v_DeviceNum,		&v_MainDeviceType,
								&v_SemiDeviceType,		&v_DeviceName,		&v_Address,
								&v_City,				&v_ZipCode,			&v_Phone1,
								&v_AssutaCardNumber,	&v_FaxNum,			&v_DiscountPrecent,
								&v_CreditYesNoFlg,		&v_AllowedProcList,	&v_AllowedBelongings,
								END_OF_ARG_LIST														);
// GerrLogMini (GerrId, "Fetched Maccabi Center #%d. ZIP = %d, Phone1 = {%s}, Fax = {%s}, SQLCODE %d.",
//	v_DeviceNum, v_ZipCode, v_Phone1, v_FaxNum, SQLCODE);

		Conflict_Test( reStart );

		if( SQLERR_code_cmp( SQLERR_end_of_fetch ) == MAC_TRUE )
		{
		    break;
		}
		else if( SQLERR_error_test() )
		{
		    SetErrorVar( &v_ErrorCode, ERR_DATABASE_ERROR );
		    break;
		}
		/*
		 * Write the error-message rows
		 * ----------------------------
		 */
		v_NumOfLinesToUpdate++;

		// DonR 25Jul2013: Per Iris Shaya, switched the two Location ("device") Type fields, so the "main" one is sent second.
		fprintf( outFP, "%0*d",
			 TActionCode_len,	v_ActionCode ); 
		fprintf( outFP, "%0*d",
			 TDeviceNum_len,	v_DeviceNum ); 
		fprintf( outFP, "%-*.*s",
			 TSemiDeviceType_len,	TSemiDeviceType_len,
			 v_SemiDeviceType );
		fprintf( outFP, "%-*.*s",
			 TMainDeviceType_len,	TMainDeviceType_len,
			 v_MainDeviceType );

		WinHebToDosHeb( ( unsigned char * )( v_DeviceName ) );
		
		fprintf( outFP, "%-*.*s",
			 TDeviceName_len,	TDeviceName_len,
			 v_DeviceName );

		WinHebToDosHeb( ( unsigned char * )( v_Address ) );
		
		fprintf( outFP, "%-*.*s",
			 TAddress_len,		TAddress_len,
			 v_Address );

		WinHebToDosHeb( ( unsigned char * )( v_City ) );
		
		fprintf( outFP, "%-*.*s",
			 TCity15_len,		TCity15_len,
			 v_City );
		fprintf( outFP, "%0*d",
			 TZipCode_len,		v_ZipCode ); 
		fprintf( outFP, "%-*.*s",
			 TPhone1_len,		TPhone1_len,
			 v_Phone1 );
		fprintf( outFP, "%-*.*s",
			 10,		10,
			 v_AssutaCardNumber );	// DonR 09Sep2009: Replaces Phone2 (Assuta Pharmacy)
		fprintf( outFP, "%-*.*s",
			 TFaxNum_len,		TFaxNum_len,
			 v_FaxNum );
		fprintf( outFP, "%0*d",
			 TDiscountPrecent_len,	v_DiscountPrecent );
		fprintf( outFP, "%0*d",
			 TCreditYesNoFlg_len,	v_CreditYesNoFlg );
		
		WinHebToDosHeb( ( unsigned char * )( v_AllowedProcList ) );
		
		fprintf( outFP, "%-*.*s",
			 TAllowedProcList_len,   TAllowedProcList_len,
			 v_AllowedProcList );
		
		WinHebToDosHeb( ( unsigned char * )( v_AllowedBelongings ) );
		
		fprintf( outFP, "%-*.*s",
			 TAllowedBelongings_len,   TAllowedBelongings_len,
			 v_AllowedBelongings );
		
	    } /* for( ; ; ) */

		CloseCursor (	MAIN_DB, TR1051_centers_up_cur	);
	    SQLERR_error_test();

	} while( 0 );		/* DO loop Shuld run only once	*/
	/*=======================================================================
	||			  Commit the transaction.			||
	 =======================================================================*/
	if( reStart == MAC_FALS )	/* Tranaction OK then COMMIT	*/
	{
		CommitAllWork ();

	    SQLERR_error_test();
	}
	else
	{
	    if( TransactionRestart() != MAC_OK )
	    {
			v_ErrorCode = ERR_DATABASE_ERROR;
			break;
	    }

	    GerrLogReturn(
			  GerrId,
			  "Table is locked for the <%d> time",
			  tries
			  );

	    sleep( ACCESS_CONFLICT_SLEEP_TIME );
	    fclose( outFP );
	}
    }	/* End of retries loop */

    if( reStart == MAC_TRUE )
    {
	GerrLogReturn(
		      GerrId,
		      "Locked for <%d> times",
		      SQL_UPDATE_RETRIES
		      );

	SetErrorVar( &v_ErrorCode, ERR_DATABASE_ERROR );
    }
    
	/*=======================================================================
	||									||
	||			REWRITE  MESSAGE HEADER.			||
	||									||
	 =======================================================================*/

    rewind( outFP );

    fprintf( outFP, "%0*d", MSG_ID_LEN,		1052 );

    fprintf( outFP, "%0*d", MSG_ERROR_CODE_LEN, MAC_OK );

    fprintf( outFP, "%0*d",TPharmNum_len,	v_PharmNum );

    fprintf( outFP, "%0*d", TInstituteCode_len,	v_InstituteCode );

    fprintf( outFP, "%0*d", TTerminalNum_len,	v_TerminalNum );

    fprintf( outFP, "%0*d", TErrorCode_len,	v_ErrorCode );

    fprintf( outFP, "%0*d",TUpdateDate_len,	v_UpdateDate );

    fprintf( outFP, "%0*d",TUpdateHour_len,	v_UpdateHour );

    fprintf( outFP, "%0*d", TNumOfLinesToUpdate_len,
	     v_NumOfLinesToUpdate );
    fclose( outFP );

	/*
	 * Write the name of output-file on string for T-SWITCH
	 * ----------------------------------------------------
	 */
    *p_outputWritten = sprintf( OBuffer, "%s", fileName );
    *output_type_flg = ANSWER_IN_FILE;
    ssmd_data_ptr->error_code = v_ErrorCode;
    ssmd_data_ptr->pharmacy_num = v_PharmNum;
    ssmd_data_ptr->terminal_num = v_TerminalNum;

	RESTORE_ISOLATION;

    return( RC_SUCCESS );

}
/* end of handler */



/*=======================================================================
||																		||
||			     HandlerToMsg_1053										||
||		Message handler for message 1053 - insert into 					||
||		   tables MONEY_EMPTY & MONEY_EMP_LINES							||
||		               Generic mode										||
||																		||
 =======================================================================*/
int   GENERIC_HandlerToMsg_1053( char		*IBuffer,
				 int		TotalInputLen,
				 char		*OBuffer,
				 int		*p_outputWritten,
				 int		*output_type_flg,
				 SSMD_DATA	*ssmd_data_ptr,
				 short int	commfail_flag,
				 int		fromSpool )
{
	/*=======================================================================
	||			    Local declarations				||
	 =======================================================================*/
    int				nOut;
    int				i,
					reStart;
    PHARMACY_INFO	Phrm_info;
	ISOLATION_VAR;
	/*=======================================================================
	||			Message fields variables			||
	 =======================================================================*/
	/*
		* Buffer to hold table MONEY_EMP_LINES records:
		* ----------------------------------------------
		*/
	static int		first_time = 1;
	static int		line_rec_alloc;
		static TMoneyEmpLineRec	*line_rec_err;
	TMoneyEmpLineRec	*line_arr_iter;
		
	/*
		*  Include message field variables file:
		*/
	TPharmNum		v_PharmNum;
	TInstituteCode		v_InstituteCode;
	TTerminalNum		v_TerminalNum;
	TActionType		v_ActionType;
	Tdiary_month		v_diary_month;
	TReportDate		v_ReportDate;
	TReportHour		v_ReportHour;
	TInvoiceNum		v_InvoiceNum;
	TAuthorizarionNum2	v_AuthorizarionNum2;
	TAuthorizarionNum3	v_AuthorizarionNum3;
	TRecordNum		v_RecordNum;

	TDate			current_Date;
	THour			current_Hour;

	/*
		*   Declarations of variables appearing in return message ONLY:
		*/
	TErrorCode		v_ErrorCode;
	TRealAcceptedRecordsNum	v_RealAcceptedRecordsNum;

	short int		comm_error_code = commfail_flag;

	/*=======================================================================
	||									||
	||				Body of function			||
	||									||
	||======================================================================||
	||			  Init answer variables				||
	 =======================================================================*/
	REMEMBER_ISOLATION;
	PosInBuff = IBuffer;
    
	if( first_time )
	{
	    line_rec_err =
		(TMoneyEmpLineRec*)
		MemAllocReturn(
			       GerrId,
			       (line_rec_alloc = 10)*sizeof(TMoneyEmpLineRec),
			       "memory for money empty lines"
			       );

	    if( line_rec_err == NULL )
	    {
			RESTORE_ISOLATION;
			return( ERR_NO_MEMORY );
	    }

	    first_time = 0;
	}

	/*=======================================================================
	||									||
	||		      Get message fields data into variables		||
	||									||
	 =======================================================================*/
	 
	CHECK_ERROR();
	v_PharmNum		=	GetLong( &PosInBuff, 
						 TPharmNum_len );
	ssmd_data_ptr->pharmacy_num = v_PharmNum;
	CHECK_ERROR();
	v_InstituteCode		=	GetShort( &PosInBuff, 
						  TInstituteCode_len );
	CHECK_ERROR();
	v_TerminalNum		=	GetShort( &PosInBuff, 
						  TTerminalNum_len );
	CHECK_ERROR();
	v_ActionType		=	GetShort( &PosInBuff, 
						  TActionType_len );
	CHECK_ERROR();

	v_ReportDate		=	GetLong( &PosInBuff, 
						 TReportDate_len );
	CHECK_ERROR();
	ChckDate( v_ReportDate );

	CHECK_ERROR();
	v_ReportHour		=	GetLong( &PosInBuff, 
						 TReportHour_len );
	CHECK_ERROR();
	ChckTime( v_ReportHour );

	CHECK_ERROR();
	v_InvoiceNum 		=	GetLong( &PosInBuff, 
						 TInvoiceNum_len );
	CHECK_ERROR();
	v_AuthorizarionNum2     =	GetShort( &PosInBuff, 
						  TAuthorizarionNum2_len );
	CHECK_ERROR();
	v_AuthorizarionNum3     =	GetLong( &PosInBuff, 
						 TAuthorizarionNum3_len );
	
	v_diary_month   	=	GetShort( &PosInBuff, 
						  Tdiary_month_len );
	CHECK_ERROR();
	v_RecordNum		=	GetShort( &PosInBuff, 
						  TRecordNum_len );
	CHECK_ERROR();
	
	if( v_RecordNum >= line_rec_alloc - 1 )
	{
	    line_rec_err =
		(TMoneyEmpLineRec*)
		MemReallocReturn(
			         GerrId,
				 line_rec_err,
			         (line_rec_alloc = v_RecordNum + 10)
				 *sizeof(TMoneyEmpLineRec),
			         "memory for money empty lines"
			         );

	    if( line_rec_err == NULL )
	    {
			RESTORE_ISOLATION;
			return( ERR_NO_MEMORY );
	    }
	}

	for( i = 0; i < v_RecordNum; i++ )
	{
	    line_rec_err[i].v_StationNum	= GetShort( &PosInBuff, 
						     TStationNum_len );
	    CHECK_ERROR();
	    line_rec_err[i].v_UserIdentification= GetLong( &PosInBuff, 
						     TUserIdentification_len );
	    CHECK_ERROR();
	    line_rec_err[i].v_CardNum		= GetLong( &PosInBuff, 
						      TCardNum_len );
	    CHECK_ERROR();
	    line_rec_err[i].v_PaymentType	= GetShort( &PosInBuff, 
						      TPaymentType_len );
	    CHECK_ERROR();
	    line_rec_err[i].v_SalesMoney	= GetLong( &PosInBuff, 
						      TSalesMoney_len );
	    CHECK_ERROR();
	    line_rec_err[i].v_SalesActionNum	= GetShort( &PosInBuff, 
						      TSalesActionNum_len );
	    CHECK_ERROR();
	    line_rec_err[i].v_RefundsMoney	= GetLong( &PosInBuff, 
						      TRefundsMoney_len );
	    CHECK_ERROR();
	    line_rec_err[i].v_RefundActionNum	= GetShort( &PosInBuff, 
						      TRefundActionNum_len );
	    CHECK_ERROR();
	}

    /*
     *  Get amount of input not eaten
     *  -----------------------------
     */
    ChckInputLen( TotalInputLen - (PosInBuff - IBuffer) );
    CHECK_ERROR();
	/*=======================================================================
	||									||
	||			    SQL TRANSACTIONs.				||
	||									||
	||======================================================================||
	||			     Retries loop				||
	 =======================================================================*/

    reStart = MAC_TRUE;
	SET_ISOLATION_COMMITTED;

    for( tries=0; tries < SQL_UPDATE_RETRIES && reStart == MAC_TRUE; tries++ )
    {
	/*=======================================================================
	||			  Init answer variables				||
	 =======================================================================*/
	reStart	    = MAC_FALS;
	
	v_ErrorCode = IS_PHARMACY_OPEN (v_PharmNum, v_InstituteCode, &Phrm_info);
	if( v_ErrorCode == ERR_PHARMACY_NOT_FOUND )
	{
	    break;
	}

	/*
	 * Test pharmacy type (private/macabi)
	 * ----------------------------------
	 */
	if( PRIVATE_PHARMACY )
	{
	    if( SetErrorVar(&v_ErrorCode, ERR_PHARMACY_HAS_NO_PERMISSION ))
	    {
		break;
	    }
	}
	
//	if( TransactionBegin() != MAC_OK )
//	{
//	    v_ErrorCode = ERR_DATABASE_ERROR;
//	    break;
//	}
	/*=======================================================================
	||									||
	|| Insert into tables MONEY_EMPTY (master) & MONEY_EMP_LINES (detailes) ||
	||									||
	 =======================================================================*/

	current_Date = GetDate();
	current_Hour = GetTime();

	do
	{
		ExecSQL (	MAIN_DB, TR1053_INS_money_empty,
					&v_PharmNum,			&v_InstituteCode,		&v_TerminalNum,			&v_ActionType,
					&v_diary_month,			&v_ReportDate,			&v_ReportHour,			&current_Date,
					&current_Hour,			&v_InvoiceNum,			&v_AuthorizarionNum2,	&v_AuthorizarionNum3,
					&v_RecordNum,			&ROW_NOT_DELETED,		&comm_error_code,		&v_ErrorCode,
					&NOT_REPORTED_TO_AS400,	END_OF_ARG_LIST															);

		GerrLogFnameMini ("money_empty_log", GerrId,
						  "\nPharm %d, Action %d, Diary Month %d, ReportDate/Time %d/%d, SQLCODE %d.\n",
						  v_PharmNum,
						  (int)v_ActionType,
						  (int)v_diary_month,
						  v_ReportDate,
						  v_ReportHour,
						  (int)SQLCODE);
	    
	    Conflict_Test( reStart );

	    if( SQLERR_error_test() )
	    {
		SetErrorVar( &v_ErrorCode, ERR_DATABASE_ERROR );
		break;
	    }

	    line_arr_iter = line_rec_err;

	    for( i = 0; i < v_RecordNum; i++ )
	    {
			ExecSQL (	MAIN_DB, TR1053_INS_money_emp_lines,
						&v_diary_month,					&v_PharmNum,						&v_ActionType,
						&v_InvoiceNum,					&v_TerminalNum,						&line_arr_iter->v_UserIdentification,
						&line_arr_iter->v_CardNum,		&ROW_NOT_DELETED,					&line_arr_iter->v_StationNum,
						&line_arr_iter->v_PaymentType,	&line_arr_iter->v_SalesMoney,		&line_arr_iter->v_SalesActionNum,
						&line_arr_iter->v_RefundsMoney,	&line_arr_iter->v_RefundActionNum,	&current_Date,
						&current_Hour,					&NOT_REPORTED_TO_AS400,				END_OF_ARG_LIST							);
		
		Conflict_Test( reStart );

		if( SQLERR_error_test() )
		{
		    SetErrorVar( &v_ErrorCode, ERR_DATABASE_ERROR );
		    break;
		}

		line_arr_iter++; /* promote MONEY_EMP_LINES iterator */

	    }

	    Conflict_Test( reStart );
	}
	while( 0 );

	/*=======================================================================
	||			  Commit the transaction.			||
	 =======================================================================*/
	if( reStart == MAC_FALS )	/* Tranaction OK then COMMIT	*/
	{
	    // 13-5-98 Ely :
	    // Let spooled transactions in even when shift 
	    // is closed.
	    //
	    if( ! SetErrorVar( &v_ErrorCode, v_ErrorCode ) ||
		( fromSpool && v_ErrorCode == ERR_PHARMACY_NOT_OPEN ) )
	    {
			CommitAllWork ();
	    }
	    else
	    {
			RollbackAllWork ();
	    }
	    if( SQLERR_error_test() )		/* Could not COMMIT	*/
	    {
			SetErrorVar( &v_ErrorCode, ERR_DATABASE_ERROR );
	    }
	}
	else
	{
	    if( TransactionRestart() != MAC_OK ) /* -> sleep & rollback */
	    {
			v_ErrorCode = ERR_DATABASE_ERROR;
			break;
	    }

	    GerrLogReturn(
			  GerrId,
			  "Table is locked for the <%d> time",
			  tries
			  );

	    sleep( ACCESS_CONFLICT_SLEEP_TIME );
	}
    }	/* End of retries loop */

    if( reStart == MAC_TRUE )
    {
	GerrLogReturn(
		      GerrId,
		      "Locked for <%d> times",
		      SQL_UPDATE_RETRIES
		      );

	SetErrorVar( &v_ErrorCode, ERR_DATABASE_ERROR );
    }

	/*=======================================================================
	||									||
	||			  WRITE RESPOND MESSAGE.			||
	||									||
	 =======================================================================*/
    
    nOut  = sprintf( OBuffer, "%0*d",
				 MSG_ID_LEN,		1054 );

    nOut += sprintf( OBuffer + nOut, "%0*d",
				  MSG_ERROR_CODE_LEN,	MAC_OK );

    nOut += sprintf( OBuffer + nOut, "%0*d",
				  TPharmNum_len, 
				  v_PharmNum );
    
    nOut += sprintf( OBuffer + nOut, "%0*d",
				  TInstituteCode_len, 
				  v_InstituteCode );
    
    nOut += sprintf( OBuffer + nOut, "%0*d",
				  TTerminalNum_len, 
				  v_TerminalNum );
    
    nOut += sprintf( OBuffer + nOut, "%0*d",
				  TErrorCode_len, 
				  v_ErrorCode );
    
    nOut += sprintf( OBuffer + nOut, "%0*d",
				  TRecordNum_len, 
				  v_RecordNum );
    
    /*
     * Return the size in Bytes of respond message
     * -------------------------------------------
     */
    *p_outputWritten = nOut;
    *output_type_flg = ANSWER_IN_BUFFER;
    ssmd_data_ptr->error_code = v_ErrorCode;

	RESTORE_ISOLATION;

    return RC_SUCCESS;
}
/* End of GENERIC_1053 handler */



/*=======================================================================
||																		||
||			     HandlerToMsg_1055										||
||			Message handler for message 1055							||
||				get dur/od messages 									||
||																		||
 =======================================================================*/
int   HandlerToMsg_1055 (char		*IBuffer,
						 int		TotalInputLen,
						 char		*OBuffer,
						 int		*p_outputWritten,
						 int		*output_type_flg,
						 SSMD_DATA	*ssmd_data_ptr)
{
#define SEVER_DUR( presc_err )					\
    ( (presc_err).doctor_id		!=				\
	(presc_err).sec_doctor_id 	&&				\
	(presc_err).check_type	== DUR_CHECK &&		\
	(presc_err).error_code 	== ERR_DUR_ERROR )

	/*=======================================================================
	||			    Local declarations										||
	 =======================================================================*/
    int				nOut;
    int				i,
					j,
					k,
					code,
					date,
					year,
					presc_err_cnt,
					next_err_cnt,
					prev_err_cnt,
					last_msg_type,
					last_msg_sever,
					last_msg_dest,
					next_msg_dest,
					next_msg_type,
					next_msg_sever,
					month,
					day,
					match,
					dyn_var_max,
					reStart;
	int				first_presc_err_in_msg;
	int				elpr_must_review_flag;
    PHARMACY_INFO	Phrm_info;
    char			fileName[256];
    FILE			*outFP;
    static int		first_time = 1;
    char			*from;
    char			*to,
					*next,
					to_buf[512];
    time_t			long_time;
    struct tm		*cur_time;
	short			LineHasVariables;
	short			VariablesHaveValues;
	short			FullDiagnosticsEnabled	= 0;
	ISOLATION_VAR;


//
// The following define is the number of entries
//  in templates array -- equals to multiple of
//  destinations , parts, severities, types
//
#define 		Messages_Total 64

	/*=======================================================================
	||			Message fields variables			||
	 =======================================================================*/
	TPharmNum			v_PharmNum;
	TInstituteCode		v_InstituteCode;
	TTerminalNum		v_TerminalNum;

	TDrugCode		drugA;
	TDrugCode		drugB;
	TSqlInd			v_Ind;
	TSqlInd			v_Ind1;

	static TDate	current_Date;
	static THour	current_Hour;

	int				v_MemberIdentification;
	short			v_IdentificationCode;
	int				v_RecipeIdentifier;
	short int		sq_msg_dest;
	short int		sq_msg_type;
	short int		sq_msg_part;
	short int		sq_msg_sub_cycle;
	short int		sq_msg_sever;
	char			sq_msg_row[81];
	char			sq_msg_row_alt[81];	// DonR 01Mar2010.
	int				sq_msg_row_num;
	short int		title_printed;
	short int		row_in_message;
	short int		row_in_section;		// DonR 03Mar2010.
	short int		skip_older_sale;	// DonR 03Mar2010.
	short			DummyCheckInteractions;

	// Declarations of variables appearing in return message ONLY:
	TErrorCode		v_ErrorCode;
	short			v_AnotherMessage	= 0;
	short			v_MedNumOfMessageLines;
	short			v_PrintFlg;

	int				presc_err_max;
	static int		presc_err_alloc;

	static PRESC_ERR	*presc_err_array;
	static PRESC_ERR	presc_err;
	static DUR_DATA		dur_data;
	static OVD_DATA		ovd_data;

	// dynamic variables array & iterations pointer
	static DYN_VAR		dyn_vars[MAX_DYN_VAR];
	static DYN_VAR		dyn_var;

	// all messages rows arrays by type,destination
	static MESS_DATA	all_messages[Messages_Total];

	// array holding data from presc_drug_inter
	// DonR 11Mar2024 Bug #297605: Added short-integer variable types to make
	// the variables compatible with what we actually read from the database.
	static VAR_DATA presc_vars[] =
		{
			{ (char*)&current_Date,					PAR_DAT				},
			{ (char*)dur_data.member_first_name,	PAR_ASC				},
			{ (char*)dur_data.member_last_name,		PAR_ASC				},
			{ (char*)&presc_err.member_id,			PAR_INT				},
			{ (char*)&presc_err.prescription_id,	PAR_INT				},
			{ (char*)&presc_err.pharmacy_code,		PAR_INT				},
			{ (char*)&presc_err.institued_code,		PAR_SHORT			},
			{ (char*)&presc_err.terminal_id,		PAR_SHORT			},
			{ (char*)&presc_err.line_no,			PAR_SHORT			},
			{ (char*)&presc_err.largo_code,			PAR_INT				},
			{ (char*)&presc_err.largo_code_inter,	PAR_INT				},
			{ (char*)&presc_err.interaction_type,	PAR_SHORT			},
			{ (char*)&presc_err.date,				PAR_INT				},
			{ (char*)&presc_err.time,				PAR_INT				},
			{ (char*)&presc_err.del_flg,			PAR_SHORT			},
			{ (char*)&presc_err.presc_id_inter,		PAR_INT				},
			{ (char*)&presc_err.line_no_inter,		PAR_SHORT			},
			{ (char*)&presc_err.mem_id_extension,	PAR_SHORT			},
			{ (char*)&presc_err.doctor_id,			PAR_INT				},
			{ (char*)&presc_err.doctor_id_type,		PAR_SHORT			},
			{ (char*)&presc_err.sec_doctor_id,		PAR_INT				},
			{ (char*)&presc_err.sec_doctor_id_type,	PAR_SHORT			},
			{ (char*)&presc_err.error_code,			PAR_SHORT			},
			{ (char*)&presc_err.duration,			PAR_SHORT			},
			{ (char*)&presc_err.op,					PAR_INT				},
			{ (char*)&presc_err.units,				PAR_INT				},
			{ (char*)&presc_err.stop_blood_date,	PAR_INT				},
			{ (char*)&presc_err.check_type,			PAR_SHORT			},
			{ (char*)&presc_err.destination,		PAR_SHORT			},
			{ (char*)&presc_err.print_flg,			PAR_SHORT			},
			{ (char*)&presc_err.doc_approved_flag,	PAR_NON_ZERO_SHORT	},
		};

	// array holding data for dur (drug interaction) messages
	static VAR_DATA dur_vars[] =
		{
			{ (char*)dur_data.drug_desc, 			PAR_ASC },
			{ (char*)dur_data.drug_inter_desc, 		PAR_ASC },
			{ (char*)&dur_data.interaction_sever, 	PAR_INT },
			{ (char*)dur_data.interaction_desc, 	PAR_ASC },
			{ (char*)dur_data.interaction_desc_add,	PAR_ASC },
			{ (char*)dur_data.doctor_first_name, 	PAR_ASC },
			{ (char*)dur_data.doctor_last_name, 	PAR_ASC },
			{ (char*)dur_data.doctor_phone,			PAR_ASC },
			{ (char*)&dur_data.stop_blood_date,		PAR_DAT },
			{ (char*)dur_data.sec_doctor_first_n,	PAR_ASC },
			{ (char*)dur_data.sec_doctor_last_n,	PAR_ASC },
			{ (char*)dur_data.member_first_name,	PAR_ASC },
			{ (char*)dur_data.member_last_name,		PAR_ASC },
			{ (char*)ovd_data.doc_phone,			PAR_ASC },
		};

	// array holding data for overdose messages
	static VAR_DATA ovd_vars[] =
		{
			{ (char*)ovd_data.drug_desc,		PAR_ASC	},
			{ (char*)&ovd_data.purchase_date,	PAR_DAT	},
			{ (char*)&ovd_data.duration,		PAR_INT	},
			{ (char*)&ovd_data.op,				PAR_INT	},
			{ (char*)&ovd_data.units,			PAR_INT	},
			{ (char*)ovd_data.doc_first_name,	PAR_ASC	},
			{ (char*)ovd_data.doc_last_name,	PAR_ASC	},
			{ (char*)ovd_data.curr_doc_f_name,	PAR_ASC	},
			{ (char*)ovd_data.curr_doc_l_name,	PAR_ASC	},
			{ (char*)ovd_data.curr_doc_phone,	PAR_ASC	},
			{ (char*)ovd_data.doc_phone,		PAR_ASC	},
		};

	static VAR_DATA		*var_dat_ptr;


	/*=======================================================================
	||		Declare dynamic variables select cursor							||
	 =======================================================================*/
	DeclareCursor (	MAIN_DB, TR1055_dyn_var_cur, END_OF_ARG_LIST	);

	/*=======================================================================
	||		Declare dur / od message rows data cursor						||
	 =======================================================================*/
	DeclareCursor (	MAIN_DB, TR1055_presc_mes_cur, &v_RecipeIdentifier, &ROW_NOT_DELETED, END_OF_ARG_LIST	);


	/*=======================================================================
	||																		||
	||				Body of function										||
	||																		||
	||======================================================================||
	||			  Init answer variables										||
	 =======================================================================*/
	REMEMBER_ISOLATION;
    PosInBuff = IBuffer;

	// First-time initialization.
    if (first_time)
    {
		for (len = 0; len < Messages_Total; len++)
		{
			all_messages[len].msg_row_alloc = 0;
		}

		presc_err_alloc = first_time = 0;
    }
    
	// Get message fields data into variables.
    CHECK_ERROR();
    v_PharmNum				= GetLong  (&PosInBuff, TPharmNum_len);				CHECK_ERROR();
    v_InstituteCode			= GetShort (&PosInBuff, TInstituteCode_len);		CHECK_ERROR();
    v_TerminalNum			= GetShort (&PosInBuff, TTerminalNum_len);
    v_MemberIdentification	= GetLong  (&PosInBuff, TMemberIdentification_len);	CHECK_ERROR();
    v_IdentificationCode	= GetShort (&PosInBuff, TIdentificationCode_len);	CHECK_ERROR();
    v_RecipeIdentifier		= GetLong  (&PosInBuff, TRecipeIdentifier_len);		CHECK_ERROR();

	ssmd_data_ptr->pharmacy_num = v_PharmNum;
	ssmd_data_ptr->member_id = v_MemberIdentification;
	ssmd_data_ptr->member_id_ext = v_IdentificationCode;

    // Get amount of input not eaten
    ChckInputLen( TotalInputLen - (PosInBuff - IBuffer) );
    CHECK_ERROR();


    sprintf( fileName, "%s/dur_od_mes_%0*d_%0*d_%0*d.snd",
	     MsgDir,
	     TPharmNum_len,      v_PharmNum,
	     TInstituteCode_len, v_InstituteCode,
	     TTerminalNum_len,   v_TerminalNum );


	// DonR 28Jul2011: Moved this line, since the review flag should be initialized to zero every time
	// the transaction is run, not just the first time!
	elpr_must_review_flag = 0;


	// Retries loop
    reStart = MAC_TRUE;
    for (tries = 0; tries < SQL_UPDATE_RETRIES && reStart == MAC_TRUE; tries++)
    {
		// Create new file for response message.
		outFP = fopen( fileName, "w" );

		if (outFP == NULL)
		{
			int	err = errno;

			GerrLogReturn (GerrId, "\tCant open output file '%s'\n\tError (%d) %s\n",
						   fileName, err, strerror(err));

			RESTORE_ISOLATION;
			return (ERR_UNABLE_OPEN_OUTPUT_FILE);
		}

		// Initialize response variables.
		reStart	    			= MAC_FALS;
		v_ErrorCode				= NO_ERROR;
		v_AnotherMessage		= MAC_FALS;
		v_MedNumOfMessageLines	= 0;
		current_Date 			= GetDate();
		current_Hour 			= GetTime();

//		// We don't need to open and COMMIT a database transaction,
//		// since Transaction 1055 doesn't write anything to the database.
//		if (TransactionBegin () != MAC_OK)
//		{
//			v_ErrorCode = ERR_DATABASE_ERROR;
//			break;
//		}
//	
		// DonR 13Jul2005: Get rid of unnecessary table-locked errors.
		SET_ISOLATION_DIRTY;


		do	// Exiting from LOOP will send the reply to pharmacy.
		{
			// Check that pharmacy exists and has performed open-shift.
			v_ErrorCode = IS_PHARMACY_OPEN (v_PharmNum, v_InstituteCode, &Phrm_info);

			if (v_ErrorCode != MAC_OK)
			{
				break;
			}
	    
			// Get all dynamic variables from database.
			OpenCursor (	MAIN_DB, TR1055_dyn_var_cur	);

			if (SQLERR_error_test ())
			{
				SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR);
				break;
			}

		    dyn_var_max = 0;

			while (1)
			{
				FetchCursorInto (	MAIN_DB, TR1055_dyn_var_cur,
									&dyn_var.var_code, &dyn_var.var_type, &dyn_var.var_name,	END_OF_ARG_LIST	);

				Conflict_Test (reStart);

				if (SQLERR_code_cmp (SQLERR_end_of_fetch ) == MAC_TRUE)
				{
				    break;
				}
				else if (SQLERR_error_test())
				{
					if (SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR))
					{
						break;
					}
				}

				// Match = the maximum subscript in the type-specific array.
				// DonR 17May2009: Added third variable type for the presc_vars array, and accordingly
				// changed ternary operator to a switch statement.
				switch (dyn_var.var_type)
				{
					case 0:  match = sizeof(dur_vars)   / sizeof(VAR_DATA) - 1;	break;
					case 1:  match = sizeof(ovd_vars)   / sizeof(VAR_DATA) - 1;	break;
					case 4:  match = sizeof(presc_vars) / sizeof(VAR_DATA) - 1;	break;	// New! DonR 17May2009.
					default: match = -1;	// Force error for unrecognized variable type.
				}

				if ((dyn_var.var_code < 0) || (dyn_var.var_code > match))
				{
					GerrLogReturn	(	GerrId, "Found dynamic var '%s' code '%d' type '%d' outside allowed range 0...%d",
										dyn_var.var_name,
										dyn_var.var_code,
										dyn_var.var_type,
										match
									);

					if (SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR))
					{
						break;
					}
				}	// Invalid dynamic variable specification read from database.

				if (dyn_var_max >= MAX_DYN_VAR)	// MAX_DYN_VAR = 100.
				{
					GerrLogReturn (GerrId, "Too many dynamic vars in table - '%d' can have only '%d'",
								   dyn_var_max, MAX_DYN_VAR);

					if (SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR))
					{
						break;
					}
				}	// Too many dynamic variables specified.

				// If we get here, we've found a valid dynamic variable specification.

				// Strip trailing spaces from dynamic variable name (so we can match it properly
				// in message-specification lines).
				for (len = strlen (dyn_var.var_name); (len > 0) && (dyn_var.var_name[len-1] == ' '); len --);

				// At end of loop, len is the valid string length of the variable name.
				dyn_var.var_name[len] = 0;
				dyn_var.var_length = len;

				// Store the variable specification we've read into the Dynamic Variables array.
				dyn_vars[dyn_var_max++] = dyn_var;

			}	// End of fetch loop for dynamic variable specifications table.

			// No need for error-trapping on cursor-close.
			CloseCursor (	MAIN_DB, TR1055_dyn_var_cur	);

			// If we've hit a problem, break out of the do-loop (and either retry or give up).
			if ((reStart == MAC_TRUE) || (v_ErrorCode != NO_ERROR))
			{
				break;
			}


			// Get all message templates from database.
			for (len = 0; len < Messages_Total; len++)	// Reminder: Messages_Total is #define'd to be 64.
			{
				// Allocate memory for template lines. Initially, we allocate space for ten
				// message-template lines for each of 64 possible message categories.
				if (!all_messages[len].msg_row_alloc)
				{
					all_messages[len].msg_rows = (MESS_ROW *) MemAllocReturn (GerrId,
																			  (all_messages[len].msg_row_alloc = 10) * sizeof (MESS_ROW),
																			  "memory for messages templates");

					if (all_messages[len].msg_rows == NULL)	// Memory allocation failed.
					{
						SetErrorVar (&v_ErrorCode, ERR_NO_MEMORY);
						break;
					}
				}	// Space for message-template lines has not yet been allocated.

				all_messages[len].msg_row_max = 0;

				// #define	SEVER_NUM	2
				// #define	PART_NUM	4
				// #define	DEST_NUM	4
				// #define	TYPE_NUM	2

				// Set attributes by position in array.
				sq_msg_type		= TYPE_BY_LEN	(len);	// = 0 or 1.
				sq_msg_dest		= DEST_BY_LEN	(len);	// = 0, 1, 2, or 3 - but only 0 and 3 are populated.
				sq_msg_part		= PART_BY_LEN	(len);	// = 0, 1, 2, or 3.
				sq_msg_sever	= SEVER_BY_LEN	(len);	// = 0 or 1.


				// Definitions of type, destination, part, and severity.
				// Severity:	0 = severe,
				//				1 = warning only.
				//
				// Type:		0 = interaction,
				//				1 = overdose.
				//
				// Destination:	0 = pharmacist only,
				//				3 = doctor + pharmacist. (Or other non-zero values - but 0 and 3 are what we have now.)


				// Each cursor open is for a specific value of len - so we're opening, reading, and closing
				// this cursor 64 times!
				DeclareAndOpenCursor (	MAIN_DB, TR1055_msg_fmt_cur,
										&sq_msg_type, &sq_msg_dest, &sq_msg_part, &sq_msg_sever, END_OF_ARG_LIST	);

				if (SQLERR_error_test ())
				{
					SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR);
					break;
				}

				// Loop on message rows
				while (1)
				{
					FetchCursorInto (	MAIN_DB, TR1055_msg_fmt_cur,
										&sq_msg_row, &sq_msg_row_alt, &sq_msg_row_num, &sq_msg_sub_cycle,	END_OF_ARG_LIST	);

					Conflict_Test (reStart);

					if (SQLERR_code_cmp (SQLERR_end_of_fetch) == MAC_TRUE)
					{
						break;	// Not a problem - just move on.
					}
					else if (SQLERR_error_test ())
					{
						if (SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR))
						{
							break;
						}
					}

					// If we get here, the FETCH was successful.
					// Check for enough space and, if necessary, reallocate to make more room.
					if (all_messages[len].msg_row_max >= all_messages[len].msg_row_alloc - 1)
					{
						all_messages[len].msg_rows = (MESS_ROW *) MemReallocReturn (GerrId,
																					all_messages[len].msg_rows,
																					(all_messages[len].msg_row_alloc += 10) * sizeof (MESS_ROW),
																					"memory for messages templates");

						if (all_messages[len].msg_rows == NULL)
						{
							SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR);
							break;
						}
					}	// Need to allocate more memory for this message-template category.

					// Process the row - sub-cycle, message text, and alternate text.
					// DonR 04Jun2009: For each message line, we now have an associated message sub-cycle.
					// This indicates whether a particular line of text is to be printed:
					// in all cases								(sub-cycle 0);
					// only for the first database row read		(sub-cycle 1);
					// only for the second row					(sub-cycle 2);
					// or for all rows *after* the first		(sub-cycle 3).
					// This is intended to apply to Part 1 of the message, which deals with the
					// details of current and prior drug sales.
					all_messages[len].msg_rows[all_messages[len].msg_row_max].msg_sub_cycle = sq_msg_sub_cycle;
					sq_msg_row[80] = sq_msg_row_alt[80] = 0;	// Just in case terminator is missing.
					strcpy (all_messages[len].msg_rows[all_messages[len].msg_row_max].msg_alternate,
							sq_msg_row_alt);	// Note that we don't increment msg_row_max here!
					strcpy (all_messages[len].msg_rows[all_messages[len].msg_row_max++].msg_row,
							sq_msg_row);

				}	// FETCH loop on message template rows for one combination of
					// type, severity, destination, and part.

				// Don't bother checking errors on CLOSE.
				CloseCursor (	MAIN_DB, TR1055_msg_fmt_cur	);

				if ((reStart == MAC_TRUE) || (v_ErrorCode != NO_ERROR))
				{
					break;
				}

			}	// Loop on len (from 0 to 63).

			// See if we've hit a problem reading all our message-template information.
			if ((reStart == MAC_TRUE) || (v_ErrorCode != NO_ERROR))
			{
				break;
			}


			// We've now (finally!) read in all the configuration stuff; create the output
			// header, then start reading actual transaction information and producing
			// output message(s).

			// Write message header to file.
			fprintf (outFP, "%0*d",	 5,	1056					);
			fprintf (outFP, "%0*d",	 4,	MAC_OK					);
			fprintf (outFP, "%0*d",	 7,	v_PharmNum				);
			fprintf (outFP, "%0*d",	 2,	v_InstituteCode			);
			fprintf (outFP, "%0*d",	 2,	v_TerminalNum			);
			fprintf (outFP, "%0*d",	 4,	v_ErrorCode				);
			fprintf (outFP, "%0*d",	 9,	v_MemberIdentification	);
			fprintf (outFP, "%0*d",	 1,	v_IdentificationCode	);
			fprintf (outFP, "%0*d",	 9,	v_RecipeIdentifier		);
			fprintf (outFP, "%0*d",	 1,	v_AnotherMessage		);	// Initially FALSE.
			fprintf (outFP, "%0*d",	 4,	v_MedNumOfMessageLines	);	// Initially zero.


			// Get all interaction/overdose data for this prescription_id into buffer.

			// DonR 13Jul2005: Use normal isolation level to read member's interactions.
			SET_ISOLATION_COMMITTED;

			// SELECT all presc_drug_inter rows for the prescription_id the pharmacy sent.
			OpenCursor (	MAIN_DB, TR1055_presc_mes_cur	);

			if (SQLERR_error_test ())
			{
				SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR);
				break;
			}

			// First time through, allocate array for ten presc_drug_inter rows.
			if (!presc_err_alloc)
			{
				presc_err_array = (PRESC_ERR *) MemAllocReturn (GerrId,
																(presc_err_alloc = 10) * sizeof (PRESC_ERR),
																"memory for messages dur/overdose data");

				if (presc_err_array == NULL)
				{
					SetErrorVar (&v_ErrorCode, ERR_NO_MEMORY);
					break;
				}
			}	// Initial memory allocation for presc_drug_inter rows.

			// No rows read so far.
			presc_err_max = 0;

			// FETCH rows into array (via a single-copy structure variable).
			while (1)
			{
				FetchCursorInto (	MAIN_DB, TR1055_presc_mes_cur,
									&presc_err.pharmacy_code,		&presc_err.institued_code,		&presc_err.terminal_id,
									&presc_err.prescription_id,		&presc_err.line_no,				&presc_err.largo_code,
									&presc_err.largo_code_inter,	&presc_err.interaction_type,	&presc_err.date,
									&presc_err.time,				&presc_err.del_flg,				&presc_err.presc_id_inter,
									&presc_err.line_no_inter,		&presc_err.member_id,			&presc_err.mem_id_extension,

									&presc_err.doctor_id,			&presc_err.doctor_id_type,		&presc_err.sec_doctor_id,
									&presc_err.sec_doctor_id_type,	&presc_err.error_code,			&presc_err.duration,
									&presc_err.op,					&presc_err.units,				&presc_err.stop_blood_date,
									&presc_err.check_type,			&presc_err.destination,			&presc_err.print_flg,
									&presc_err.doc_approved_flag,	END_OF_ARG_LIST													);

				Conflict_Test (reStart);

				if (SQLERR_code_cmp (SQLERR_end_of_fetch) == MAC_TRUE)
				{
					break;	// No problem - we're just finished FETCHing.
				}
				else if (SQLERR_error_test ())
				{
					if (SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR))
					{
						break;
					}
				}

				// If we got here, the FETCH succeeded.

				// If necessary, allocate more space for presc_drug_inter rows.
				if (presc_err_max >= (presc_err_alloc - 1))
				{
					presc_err_array = (PRESC_ERR *) MemReallocReturn (GerrId,
																	  presc_err_array,
																	  (presc_err_alloc += 10) * sizeof (PRESC_ERR),
																	  "memory for messages dur/overdose data");

					if (presc_err_array == NULL)
					{
						SetErrorVar (&v_ErrorCode, ERR_NO_MEMORY);
						break;
					}
				}	// Need to allocate more space for presc_drug_inter rows.

				// DonR 28Mar2004: If the interaction/overdose was detected by Transaction 2003
				// (Electronic Prescription), see if this is an interaction that must be
				// reviewed and OK'ed by the pharmacist.
				if (presc_err.error_code == ERR_OD_INTERACT_NEEDS_REVIEW)
				{
					elpr_must_review_flag = 1;
				}

				// Store this row in the array.
				presc_err_array[presc_err_max++] = presc_err;

			}	// End of presc_drug_inter FETCH loop.

			// Don't bother with error-trapping on CLOSE.
			CloseCursor (	MAIN_DB, TR1055_presc_mes_cur	);


			// If we hit an error, retry or just give up.
			if ((reStart == MAC_TRUE) || (v_ErrorCode != NO_ERROR))
			{
				break;
			}

			// Sort all the rows we read, according to the following criteria:
			// 1) Interactions before overdose.
			// 2) For interactions:
			//    A) Severe + different doctors come first.
			//    B) Then different doctors before same doctor.
			//    C) Then most to least recent. (Should we sort the current sale as date = 0, the way
			//       we do for overdose?)
			// 3) For overdose:
			//    A) Decreasing severity (shouldn't really do anything, as currently all O/D messages are severe).
			//    B) Largo Code.
			//    C) Current sale first, then newest to oldest.
		    qsort (presc_err_array, presc_err_max, sizeof (PRESC_ERR), presc_err_comp);

			// DonR 13Jul2005: Return to "dirty read" mode to read static tables.
			SET_ISOLATION_DIRTY;


			// Loop over all dur / overdose messages data in buffer to produce output file.
			for (presc_err_cnt = title_printed = row_in_message = first_presc_err_in_msg = row_in_section = 0, prev_err_cnt = -1, next_err_cnt = 1;
				 presc_err_cnt < presc_err_max;
				 presc_err_cnt++, row_in_message++, row_in_section++, prev_err_cnt++, next_err_cnt++)
			{
				presc_err = presc_err_array [presc_err_cnt];	// I.e. a simple structure variable with the current row.

				// Load dynamic variable data.
				ovd_data.duration			= presc_err.duration;
				ovd_data.op					= presc_err.op;
				ovd_data.units				= presc_err.units;
				ovd_data.purchase_date		= presc_err.date;
				dur_data.stop_blood_date	= presc_err.stop_blood_date;

				// First drug description. Read into interaction variable; afterwards, copy to overdose variable.
				ExecSQL (	MAIN_DB, TR1055_READ_GetDL_LongName, &dur_data.drug_desc, &presc_err.largo_code, END_OF_ARG_LIST	);

				Conflict_Test (reStart);

				if (SQLERR_code_cmp (SQLERR_not_found) == MAC_TRUE)
				{
					dur_data.drug_desc[0] = 0;
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

				strip_spaces (dur_data.drug_desc);	// DonR 11Jun2009.
				strcpy (ovd_data.drug_desc, dur_data.drug_desc);


				// Second drug description - relevant only for interactions.
				ExecSQL (	MAIN_DB, TR1055_READ_GetDL_LongName, &dur_data.drug_inter_desc, &presc_err.largo_code_inter, END_OF_ARG_LIST	);

				Conflict_Test (reStart);

				if (SQLERR_code_cmp (SQLERR_not_found) == MAC_TRUE)
				{
					dur_data.drug_inter_desc[0] = 0;
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

				strip_spaces (dur_data.drug_inter_desc);	// DonR 11Jun2009.


				// Interaction type. Note that the interaction_type table has values only
				// for types 1 and 2 - so 0, like any other non-recognized value, will be
				// not found (and give a resulting severity of 0). Note also that the
				// destination variable, dur_data.interaction_sever, is kind of misnamed:
				// 1 or 2 do refer to different severities of interaction, but this field
				// is not used to determine the *logical* severity (i.e. system behavior) -
				// that goes by the severity code of the error code assigned.
				ExecSQL (	MAIN_DB, TR1055_READ_interaction_type, &dur_data.interaction_sever, &presc_err.interaction_type, END_OF_ARG_LIST	);

				Conflict_Test (reStart);

				if (SQLERR_code_cmp (SQLERR_not_found) == MAC_TRUE)
				{
					dur_data.interaction_sever 	= 0;
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


				// Interaction description. The drug_interaction table always lists the two interacting
				// drugs in order of ascending Largo Code.
				if (presc_err.largo_code < presc_err.largo_code_inter)
				{
					drugA = presc_err.largo_code;
					drugB = presc_err.largo_code_inter;
				}
				else
				{
					drugA = presc_err.largo_code_inter;
					drugB = presc_err.largo_code;
				}

				ExecSQL (	MAIN_DB, TR1055_READ_GetDrugInteractionNotes,
							&dur_data.interaction_desc,	&dur_data.interaction_desc_add,
							&drugA,						&drugB,
							END_OF_ARG_LIST													);

				v_Ind	= ColumnOutputLengths [1];
				v_Ind1	= ColumnOutputLengths [2];

				Conflict_Test (reStart);

				if (SQLERR_code_cmp (SQLERR_not_found) == MAC_TRUE)
				{
					dur_data.interaction_desc [0] = dur_data.interaction_desc_add [0] = (char)0;
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

				// If we got here, the drug_interaction SELECT worked.
				if (SQLMD_is_null (v_Ind))
				{
					dur_data.interaction_desc[0] = (char)0;
				}

				if (SQLMD_is_null (v_Ind1))
				{
					dur_data.interaction_desc_add[0] = (char)0;
				}


				// doctor first,last name & phone
				ExecSQL (	MAIN_DB, READ_doctors,
							&dur_data.doctor_first_name,	&dur_data.doctor_last_name,
							&dur_data.doctor_phone,			&DummyCheckInteractions,
							&presc_err.doctor_id,			END_OF_ARG_LIST	);

				Conflict_Test (reStart);

				if (SQLERR_code_cmp (SQLERR_not_found) == MAC_TRUE)
				{
					dur_data.doctor_first_name[0] = dur_data.doctor_last_name[0] = dur_data.doctor_phone[0] = (char)0;
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

				// Strip spaces from doctor name and copy from interaction variables to overdose variables.
				strip_spaces (dur_data.doctor_first_name);	// DonR 11Jun2009.
				strip_spaces (dur_data.doctor_last_name);	// DonR 11Jun2009.
				strip_spaces (dur_data.doctor_phone);		// DonR 11Jun2009.

				strcpy (ovd_data.curr_doc_f_name,	dur_data.doctor_first_name);
				strcpy (ovd_data.curr_doc_l_name,	dur_data.doctor_last_name);
				strcpy (ovd_data.curr_doc_phone,	dur_data.doctor_phone);


				// second drug doctor first,last name & phone
				// Why are we using the overdose doctor-phone variable rather than having two phone
				// numbers in the interaction variable list?
				ExecSQL (	MAIN_DB, READ_doctors,
							&dur_data.sec_doctor_first_n,	&dur_data.sec_doctor_last_n,
							&ovd_data.doc_phone,			&DummyCheckInteractions,
							&presc_err.sec_doctor_id,		END_OF_ARG_LIST	);

				Conflict_Test (reStart);

				if (SQLERR_code_cmp (SQLERR_not_found) == MAC_TRUE)
				{
					dur_data.sec_doctor_first_n[0] = dur_data.sec_doctor_last_n[0] = ovd_data.doc_phone[0] = (char)0;
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

				// Strip spaces from doctor name and copy from interaction variables to overdose variables.
				strip_spaces (dur_data.sec_doctor_first_n);	// DonR 11Jun2009.
				strip_spaces (dur_data.sec_doctor_last_n);	// DonR 11Jun2009.
				strip_spaces (ovd_data.doc_phone);			// DonR 11Jun2009.

				strcpy (ovd_data.doc_first_name,	dur_data.sec_doctor_first_n);
				strcpy (ovd_data.doc_last_name,		dur_data.sec_doctor_last_n);


				// member first & last name
				ExecSQL (	MAIN_DB, TR1055_READ_members,
							&dur_data.member_first_name,	&dur_data.member_last_name,
							&presc_err.member_id,			&presc_err.mem_id_extension,
							END_OF_ARG_LIST													);

				Conflict_Test (reStart);

				if (SQLERR_code_cmp (SQLERR_not_found) == MAC_TRUE)
				{
					dur_data.member_first_name[0] = dur_data.member_last_name[0] = (char)0;
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

				// Strip spaces from Member Name.
				strip_spaces (dur_data.member_first_name);		// DonR 11Jun2009.
				strip_spaces (dur_data.member_last_name);		// DonR 11Jun2009.


				// We've now read all our tables - time to do stuff with the data!

				/*=======================================================================
				|| 		Combine actual dynamic values with template						||
				 =======================================================================*/

				// Get attributes of required message

				sq_msg_type  = presc_err.check_type;	// Interaction or overdose.
				sq_msg_part  = 1;	// body of message, representing actual database rows from presc_drug_inter.

//				sq_msg_sever = GET_ERROR_SEVERITY (presc_err.error_code);	// severity
				sq_msg_sever = (GET_ERROR_SEVERITY (presc_err.error_code) >= FATAL_ERROR_LIMIT) ? 0 : 1;

				// Fetch the appropriate message to send back.
				//   follow the following precedence list --
				// 1) Non-severe interaction for same doctor: try 3.
				// 2) Anything else (including no layout for 3): try 0.
				// 3) If no layout for 0, try 2 - which will always fail based on how dur_msgs is populated.
				// 4) If 2 fails (which it always will): give up and throw an error.

				sq_msg_dest  = 0;	// Default: pharmacist (screen only?)

				// Is this a non-severe same-doctor interaction?
				if ((sq_msg_type != OVER_DOSE_CHECK) && (presc_err.doctor_id == presc_err.sec_doctor_id))
				{
					sq_msg_sever = 1;			// not sever
					sq_msg_dest  = 3;			// same doc not sever
				}

				state 	= sq_msg_dest;			// save destination

				// If there is no message format defined for "part 1" (actual data rows) for the
				// default destination, try a different destination. Note that Destination 2
				// doesn't appear to exist, so Destination 2 will always continue to -1.
				while (sq_msg_dest > -1)
				{
					// Index of message format.
					len = LEN_BY_ATTR (sq_msg_type, sq_msg_dest, sq_msg_part, sq_msg_sever);

					// Is there at least one output line for this destination?
					if (all_messages[len].msg_row_max == 0)	// I.e. no - this is an empty slot.
					{
						// No output format found - try the next-best alternative.
						switch (sq_msg_dest)
						{
							case 3:	// pharmacist -- same doctor not sever
									sq_msg_dest = 0;
									continue;

							case 0:	// pharmacist
									sq_msg_dest = 2;
									continue;

							case 2:	// pharmacist + doctor (??? - appears to be not in use!)
									sq_msg_dest = -1;
									continue;

							default:
									GerrLogReturn (GerrId,
												   "Got wrong destination value %d!",
												   sq_msg_dest);
									break;
						}		// end of switch
					}			// end of if

					break;
				}	// end of while

				// DonR 26Mar2017: After the above switch statement, we may need to recalculate len - since
				// sq_msg_dest may have changed.
				len = LEN_BY_ATTR (sq_msg_type, sq_msg_dest, sq_msg_part, sq_msg_sever);

// if (!TikrotProductionMode) GerrLogMini (GerrId, "Before Part loop: Type = %d, severe = %d, dest = %d, Part 1 len = %d, Part 1 format count = %d.", sq_msg_type, sq_msg_sever, sq_msg_dest, len, all_messages[len].msg_row_max);
				// If after the above logic we still haven't found any output format for this message,
				// write an error to log.
				if (all_messages[len].msg_row_max == 0)
				{
					GerrLogReturn (GerrId,
								   "No message body template for type : %s dest : %s severity : %s",
								   sq_msg_type	? "OVERDOSE"	: "D.U.R.",
								   state		? "DOCTOR"		: "PHARMACIST",
								   sq_msg_sever	? "WARNING"		: "SEVER");

					if( SetErrorVar( &v_ErrorCode, ERR_DATABASE_ERROR ) )
					{
						break;
					}
				}

				// Send message title bef new type/sever/dest
				// Send template message with data
				// Send message bottom title bef new type/sever/dest or end
				for (sq_msg_part = 0; sq_msg_part < 4; sq_msg_part++)
				{
// if (!TikrotProductionMode) GerrLogMini (GerrId, "Top of loop for buffer row %d - sq_msg_part = %d, .", presc_err_cnt, sq_msg_part);
					// Skip messages for non-severe interactions among drugs that same doctor wrote.
					// PROBLEM: If this is the only thing we're reporting on, there will be no output
					// and Trn. 1055 will return Error 6015 (= no message available).
					// SOLUTION: test_interaction_and_overdose() shouldn't set the flag to indicate
					// to the pharmacy that they need to run Trn. 1055 if the only problem detected
					// was a non-severe same-doctor interaction.
					if (((sq_msg_part == 0) || (sq_msg_part == 1))				&&
						(presc_err.doctor_id		== presc_err.sec_doctor_id)	&&
						(presc_err.check_type		== DUR_CHECK)				&&
						(presc_err.interaction_type	== NOT_SEVER_INTERACTION))
					{
// if (!TikrotProductionMode) GerrLogMini (GerrId, "Part %d - skipping section because same doc non-severe interaction.", sq_msg_part);
						continue;	// On to next section.
					}

					// Handle message title.
					if (sq_msg_part == 0)
					{
						if (!title_printed)
						{
							// No previous title was output - so we know we have to produce the title this time.
							last_msg_dest = last_msg_sever = last_msg_type = -100;
						}
						else
						{
							// Previous destination:
							last_msg_dest = ((sq_msg_type != OVER_DOSE_CHECK)	&&
											 (presc_err_array [prev_err_cnt].doctor_id == presc_err_array [prev_err_cnt].sec_doctor_id)) ? 3 : 0;

							// Previous severity:
							last_msg_sever = ((GET_ERROR_SEVERITY (presc_err_array [prev_err_cnt].error_code) >= FATAL_ERROR_LIMIT)	&&
											  (last_msg_dest != 3))	? 0 : 1;

							// Previous message type:
							last_msg_type  = presc_err_array [prev_err_cnt].check_type;
						}

//						last_msg_dest = title_printed ?
//											(((sq_msg_type != OVER_DOSE_CHECK) &&
//											  (presc_err_array[presc_err_cnt-1].doctor_id == presc_err_array[presc_err_cnt-1].sec_doctor_id)) ?
//											 3 : 0) : -100;
//
//						// severity
//						last_msg_sever = title_printed ?
//											(((GET_ERROR_SEVERITY(presc_err_array[presc_err_cnt-1].error_code) >= FATAL_ERROR_LIMIT) &&
//											  (last_msg_dest != 3))	?
//											  0 : 1 ): -100;
//
//						last_msg_type  = title_printed ?
//											presc_err_array[presc_err_cnt-1].check_type : -100;

						// If we've already printed the relevant title lines for this message, don't print them again.
						// DonR 22Feb2010: Added an additional condition so that if this is an Overdose message and
						// we're starting the list of relevant sales for a new drug, we'll print the title and first-row
						// stuff. (The Overdose data is now sorted by Largo Code, so the desired output consists of each
						// overdosing Largo Code in the current sale followed by a list of previous sales of the same
						// Largo Code.
						if ((sq_msg_sever			== last_msg_sever)	&&
							(sq_msg_type			== last_msg_type)	&&
							(sq_msg_dest			== last_msg_dest)	&&
							((presc_err.check_type	== DUR_CHECK) || (presc_err.prescription_id != presc_err.presc_id_inter)))
						{
							continue;	// That is, the current database row is just being added to a message
										// that we've already started to write.
						}
						else
						{
							// DonR 07Jun2009: We now track the row number of each row that's part of the current
							// message being printed, so we can differentiate between output lines that should be
							// printed only for the first row, only for the second row, for all rows after the
							// first row, or for all rows. In conformance with standard C style, we begin counting
							// with row 0 rather than row 1.
							title_printed			= 1;
							row_in_message			= 0;
							first_presc_err_in_msg	= presc_err_cnt;

							// DonR 03Mar2010: See if we need to print titles (part = 0) for the beginning
							// of a section (= subcycle 1) or a later subcycle.
							if ((sq_msg_type	!= last_msg_type)	||
								(sq_msg_dest	!= last_msg_dest))
							{
								row_in_section = 0;
							}
						}
					}	// Message title (Part = 0) control logic.

					// Handle message bottom title or
					// depended bottom title(printed at evening or Saturday).
					if (presc_err_cnt >= (presc_err_max - 1))
					{
						// We're on the last presc_drug_inter row in the list, so there is no next row!
						next_msg_dest = next_msg_sever = next_msg_type = -100;
					}
					else
					{
						// There is at least one presc_drug_inter row in the array after the current one.
						// Next destination:
						next_msg_dest  = ((sq_msg_type != OVER_DOSE_CHECK)	&&
										  (presc_err_array [next_err_cnt].doctor_id == presc_err_array [next_err_cnt].sec_doctor_id)) ? 3 : 0;

						// Next severity:
						next_msg_sever = ((GET_ERROR_SEVERITY (presc_err_array [next_err_cnt].error_code) >= FATAL_ERROR_LIMIT)	&&
										  (next_msg_dest != 3)) ? 0 : 1;

						// Next message type:
						next_msg_type  = presc_err_array [next_err_cnt].check_type;
					}

					// If the current database row is not the last one in the message being created, don't
					// generate signature/weekend lines yet.
					if (((sq_msg_part == 2) || (sq_msg_part == 3))	&&
						(((sq_msg_sever == next_msg_sever)			&&
						  (sq_msg_type  == next_msg_type)			&&
						  (sq_msg_dest  == next_msg_dest))				|| !title_printed))
					{
						continue;
					}

					// Handle special weekend bottom text (Part = 3) only in the evening or on Saturday.
					if (sq_msg_part == 3)
					{
						long_time = time (NULL);
						cur_time = localtime (&long_time);
						if ((cur_time->tm_wday  < 5 && cur_time->tm_hour < 19 && cur_time->tm_hour > 7) ||
							(cur_time->tm_wday == 5 && cur_time->tm_hour < 14 && cur_time->tm_hour > 7))
						{
							continue; 
						}
					}


					// Get message index in format array again, since parameters like part, severity, and
					// and destination may have changed.
					len = LEN_BY_ATTR (sq_msg_type, sq_msg_dest, sq_msg_part, sq_msg_sever);
// if (!TikrotProductionMode) GerrLogMini (GerrId, "Before format-row loop: Part = %d, type = %d, severe = %d, dest = %d, len = %d, format count = %d.", sq_msg_part, sq_msg_type, sq_msg_sever, sq_msg_dest, len, all_messages[len].msg_row_max);

					// DonR 16Jun2009: For interaction messages, we want to print only the most recent
					// sale for any particular drug. We have already sorted the array by descending
					// date; now we have to test each drug against what's already been printed in the
					// current message, to see if it's a "new" largo code.
					// DonR 07Mar2010: The check to see if an interaction is redundant needs to look at
					// both of the interacting largo codes.
					skip_older_sale = 0;
					if ((sq_msg_type != OVER_DOSE_CHECK) && (presc_err_cnt > first_presc_err_in_msg))
					{
						for (j = first_presc_err_in_msg; j < presc_err_cnt; j++)
						{
							if ((presc_err_array[j].largo_code_inter == presc_err_array[presc_err_cnt].largo_code_inter)	&&
								(presc_err_array[j].largo_code		 == presc_err_array[presc_err_cnt].largo_code))
							{
								skip_older_sale = 1;
								break;	// No need for further iterations.
							}
						}
					}

					// Loop on message format rows.
					for (i = 0; i < all_messages[len].msg_row_max; i++)
					{
						// DonR 07Jun2009: Add new "subcycle" control, to enable the suppression of output
						// lines depending on which of the relevant current or prior drug sales is being
						// printed. The subcycles (stored in dur_msgs) are as follows:
						//
						//		0 (default)	- prints for all rows.
						//		1			- prints only for the first drug sale in a message.
						//		2			- prints only for the second drug sale - that is, it
						//					  works as a title for the list of previous sales
						//		3			- prints for all rows except the first one - that is,
						//					  for all previous drug sales.
						//		4 (future?)	- may be used in future for output only on the last
						//					  drug sale in the message.
						//
						// Note that at least for now, this logic is implemented for all message parts, even
						// though it should really be relevant only for Part 1, which lists current and
						// previous drug sales.
						// DonR 28Feb2010: I'm about to add sub-cycles to the dur_msgs table for Part 0 in
						// Overdose messages - so ignore the "relevance" comment immediately above!

						sq_msg_sub_cycle = all_messages[len].msg_rows[i].msg_sub_cycle;	// Just for convenience.

						// DonR 02Mar2010: For title lines, we need to look at the actual database row
						// number we're printing - since we reset row_in_message to re-trigger title printing,
						// but we still want to know that we're in the middle of a message.
						// DonR 03Mar2010: Added new control variable row_in_section.
						if (sq_msg_part == 0)
						{
							if (((sq_msg_sub_cycle == 1) && (row_in_section != 0))	||
								((sq_msg_sub_cycle == 2) && (row_in_section != 1))	||
								((sq_msg_sub_cycle == 3) && (row_in_section == 0)))
							{
// if (!TikrotProductionMode) GerrLogMini (GerrId, "Part %d line %02d - skipping because row_in_section = %d and sub_cycle = %d.", sq_msg_part,i,row_in_section,sq_msg_sub_cycle);
								continue;
							}
						}
						else	// We're printing a part of the message *other* than the titles.
						{
							// DonR 04Mar2010: Added a condition to skip printing of "excess" older
							// sales of drugs in interaction (for "detail" cycle only).
							if (((sq_msg_sub_cycle == 1) && (row_in_message != 0))	||
								((sq_msg_sub_cycle == 2) && (row_in_message != 1))	||
								((sq_msg_sub_cycle == 3) && (row_in_message == 0))	||
								((sq_msg_part == 1)	&& (skip_older_sale)))
							{
// if (!TikrotProductionMode) GerrLogMini (GerrId, "Part %d line %02d - skipping because row_in_section = %d and sub_cycle = %d OR skip_older_sale = %d.", sq_msg_part,i,row_in_section,sq_msg_sub_cycle,skip_older_sale);
								continue;
							}
						}

						// Default (for now at least) is that everything goes to screen *and* printer.
						// This is modified below!
						v_PrintFlg = 1;

						// DonR 01Mar2010: Add outer loop to enable "fallback" text when normal line
						// contains dynamic variables but none of them have any value. (This is needed
						// specifically for doctors who are not in our database, so that their names
						// and telephone numbers are blank.)
						for (j = 0; j < 2; j++)
						{

							// Replace each dynamic var with data.
							// DonR 01Mar2010: If the "normal" row contained variables but all were empty,
							// AND alternate text exists, try again with the alternate text.
							from	= (j == 0) ?	all_messages[len].msg_rows[i].msg_row :
													all_messages[len].msg_rows[i].msg_alternate;
							to		= to_buf;

							// Yulia 20010712: Interaction description text is shown on-screen only.
							if (strstr (from, "תאור האינטראקציה"))
							{
								v_PrintFlg = 0;
							}


							// DonR 01Mar2010: Set up to determine if the output line (A) has at least one
							// dynamic variable, and (B) if so, if at least one dynamic variable has a value.
							LineHasVariables = VariablesHaveValues = 0;

							while (*from)
							{
								// Copy until chr(169) ("@") or end, skip chr(254) (= "©")
								for (; (*from) && (*from != (char)169); from++)
								{
									if (*from != (char)254)
									{
										*to++ = *from;
									}
								}

								// Break when end
								if (!(*from))
								{
									break;
								}

								from++;			// skip chr(169) ("©")
								state = strcspn (from, " \t_");	// capture dyn var


								// Find dynamic variable in array
								// DonR 17May2009: Added third variable type for the presc_vars array, representing
								// "generic" variables that apply to either interaction or overdose messages.
								// Accordingly, these variables (var_type == 4) are always enabled.
								for (k = 0;
									 k < dyn_var_max	&&
											(strncmp(from, dyn_vars[k].var_name, state) ||
											(dyn_vars[k].var_type != presc_err.check_type && dyn_vars[k].var_type != 4));
									 k++);

								// Dynamic variable found.
								if (k < dyn_var_max)
								{
									code				= dyn_vars[k].var_code;
									LineHasVariables	= 1;	// DonR 01Mar2010.

									// DonR 17May2009: Added third variable type for the presc_vars array, and accordingly
									// changed ternary operator to a switch statement.
									switch (dyn_vars[k].var_type)
									{
										case 0:  var_dat_ptr = dur_vars   + code;	break;
										case 1:  var_dat_ptr = ovd_vars   + code;	break;
										case 4:  var_dat_ptr = presc_vars + code;	break;	// New! DonR 17May2009.
									}


									// Suppress printout of interaction description lines - at least, I think
									// that's what this logic does! (Comment by DonR 16Jun2009.)
									if ((dyn_vars[k].var_type == 0)	&&
										(dyn_vars[k].var_code == 3 || dyn_vars[k].var_code == 4))
									{
										v_PrintFlg = 0;
									}

									// DonR 01Mar2010: Set VariablesHaveValues TRUE if a variable has a value.
									// Note that for numeric variables, even a zero constitutes a value; only
									// string variables (type = PAR_ASC) can be truly empty.
									// DonR 11Mar2024 Bug #297605: Added two variable types for short integers.
									switch (var_dat_ptr->var_type)
									{
										case PAR_INT:
											to += sprintf (to, "%d ", *(int*)var_dat_ptr->var_data);
											VariablesHaveValues = 1;
											break;

										case PAR_SHORT:	// DonR 11Mar2024.
											to += sprintf (to, "%d ", *(short *)var_dat_ptr->var_data);
											VariablesHaveValues = 1;
											break;

										case PAR_ASC:
											to += sprintf (to, "%s ", (char*)var_dat_ptr->var_data);

											if (strlen ((char*)var_dat_ptr->var_data) > 0)
												VariablesHaveValues = 1;

											break;

										case PAR_DAT:
											date	= *(int*)var_dat_ptr->var_data;
											year	= date / 10000;
											month	= (date / 100) % 100;
											day		= date % 100;
											to += sprintf( to, "%02d/%02d/%04d ", day, month, year);
											VariablesHaveValues = 1;
											break;

										// DonR 04Jan2017: Add capability of enabling or disabling an output line based on a logical
										// test of an integer variable, *without* actually adding the variable value to the message line.
										// (Note that if we wanted to do this with non-integer variables, we'd need some additional
										// flexibility here.)
										case PAR_IS_NON_ZERO:
											VariablesHaveValues = ((*(int *)var_dat_ptr->var_data) != 0) ? 1 : 0;
											break;

										case PAR_NON_ZERO_SHORT:	// DonR 11Mar2024.
											VariablesHaveValues = ((*(short *)var_dat_ptr->var_data) != 0) ? 1 : 0;
											break;

										default:
											GerrLogReturn (GerrId, "Unknown type '%d'", var_dat_ptr->var_type);
											break;
									}
								}
								else
								{
									GerrLogReturn (GerrId, "Unknown dynamic var <%.*s>", state, from);
								}

								// Skip dynamic variable & undecscores
								from += state;
								from += strspn( from, "_" );

							}	// while (*from ...

							*to = 0;

							// DonR 07Jun2009: The line below forces a maximum line length of 41.
							// This is because the printers at pharmacies can't handle more
							// than 41 characters per line.
							to_buf[41] = 0;  //30.11.2000 yulia change from to_buf[40] = 0

if ((!TikrotProductionMode) && (FullDiagnosticsEnabled)) GerrLogMini (GerrId, "Part %d line %02d, [%s] %s", sq_msg_part, i, to_buf, (j > 0) ? "(ALT)" : "");

							strip_spaces (to_buf);	// DonR 11Jun2009.
							WinHebToDosHeb( (unsigned char *)to_buf );

							// DonR 01Mar2010: If this is the firt ("normal") iteration of the loop, see
							// if there is some reason to try again with the alternate text. To do so, the
							// following conditions must be true:
							//
							// 1) The "normal" line has dynamic variables AND all are empty.
							// 2) There is an alternate line with non-zero length.
							if ((j == 0)				&&
								((!LineHasVariables)	||	// "Normal" line is free of variables.
								 (VariablesHaveValues)	||	// At least one variable has a value.
								 (strlen ((char *)all_messages[len].msg_rows[i].msg_alternate) == 0)))	// No alternate line.
							{
								break;	// Don't use alternate text!
							}

						}	// For j...


						// DonR 01Mar2010: If, after all the above, the resulting line of text begins with
						// "[NO-OUTPUT]", then don't output anything - not even a blank line. This is mainly
						// intended to work with the new alternate-text feature, so that if the "normal" line
						// has variables but all are blank, the line can be suppressed entirely (instead of
						// simply having substitute text printed out).
						if (!strncasecmp (to_buf, "[NO-OUTPUT]", 11))
						{
// if (!TikrotProductionMode) GerrLogMini (GerrId, "Part %d line %02d, [%s] - skipping!", sq_msg_part, i, to_buf);
							continue;
						}


						// Write message to output file.

						// Don't print special weekend bottom title (part == 3) - it's on-screen only.
						// Suppression of printing for interaction descriptions was handled above, so
						// at this point v_PrintFlg is already zero regardless of severity.
						// If the message is just a warning, we go by the print flag that was set when
						// the presc_drug_inter row was written. Note that if the message *is* severe,
						// we're not changing anything - the print flag is already 1, so the reassignment
						// of v_PrintFlg is redundant in this case.
						// (And yes, it's confusing that 0 means the message *is* severe!)
						if (v_PrintFlg)
						{
							v_PrintFlg =  (sq_msg_part == 3)	?	0 :
																	sq_msg_sever ? presc_err.print_flg : 1;
						}

						fprintf (outFP, "%0*d", TPrintFlg_len, v_PrintFlg);

						fprintf (outFP, "%-*.*s", TMessageLine_len, TMessageLine_len, to_buf);

						v_MedNumOfMessageLines++;

					}	// Loop on message rows within part.

				}	// Loop on message parts (title, data rows, weekend text, signature lines).

			}	// Loop over all dur / overdose messages data in buffer.

		}
		while (0);
		/*=======================================================================
		||			  End of dummy loop											||
		 =======================================================================*/

		// DonR 21Mar2017: Transaction 1055 performs no database writes, so there is no
		// reason to open a database transaction and worry about COMMITs and ROLLBACKs.
		// So I've remarked out the unnecessary code.
		if (reStart)
		{
		    GerrLogMini (GerrId, "Table is locked for the <%d> time", tries);
		    sleep (ACCESS_CONFLICT_SLEEP_TIME);
		}

	}	// End of retries loop.


	if (reStart == MAC_TRUE)
	{
		GerrLogMini (GerrId, "Locked for <%d> times", SQL_UPDATE_RETRIES);
		SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR);
	}


	// REWRITE  MESSAGE HEADER.
	rewind (outFP);

	// Assign error codes before exiting.
	if (v_MedNumOfMessageLines < 1)
	{
		SetErrorVar (&v_ErrorCode, ERR_NO_OUT_STENDING_MESSAGE);
	}
	else
	{
		if (elpr_must_review_flag)
		{
			SetErrorVar (&v_ErrorCode, ERR_OD_INTERACT_NEEDS_REVIEW);
		}
	}

	fprintf (outFP, "%0*d",	 5, 1056					);
	fprintf (outFP, "%0*d",	 4,	MAC_OK					);
	fprintf (outFP, "%0*d",	 7, v_PharmNum				);
	fprintf (outFP, "%0*d",	 2, v_InstituteCode			);
	fprintf (outFP, "%0*d",	 2, v_TerminalNum			);
	fprintf (outFP, "%0*d",	 4, v_ErrorCode				);
	fprintf (outFP, "%0*d",	 9, v_MemberIdentification	);
	fprintf (outFP, "%0*d",	 1, v_IdentificationCode	);
	fprintf (outFP, "%0*d",	 9, v_RecipeIdentifier		);
	fprintf (outFP, "%0*d",	 1, v_AnotherMessage		);	// Should always be zero - there's nothing here that sets the flag TRUE!
	fprintf (outFP, "%0*d",	 4, v_MedNumOfMessageLines	);

	fclose (outFP);


	// Write the name of output file to string for T-SWITCH.
	*p_outputWritten = sprintf (OBuffer, "%s", fileName);
	*output_type_flg = ANSWER_IN_FILE;
	ssmd_data_ptr->error_code = v_ErrorCode;
	ssmd_data_ptr->pharmacy_num = v_PharmNum;
	ssmd_data_ptr->terminal_num = v_TerminalNum;

//	// The Message Format cursor is OPENed multiple times, so it needs to be "sticky" -
//	// but that means we need to FREE it at the end of the transaction.
//	// DonR 14Oct2020: Made the statement non-sticky - but let's leave the
//	// FreeStatement in place, since it's harmless and I might change my mind.
//	FreeStatement (	MAIN_DB, TR1055_msg_fmt_cur	);
//
	RESTORE_ISOLATION;

	return (RC_SUCCESS);

}
// End of 1055 handler.



/*=======================================================================
||									||
||		  REALTIME mode of GENERIC_HandlerToMsg_1053		||
||									||
 =======================================================================*/
int   HandlerToMsg_1053( char		*IBuffer,
			 int		TotalInputLen,
			 char		*OBuffer,
			 int		*p_outputWritten,
			 int		*output_type_flg,
			 SSMD_DATA	*ssmd_data_ptr )
{
    return GENERIC_HandlerToMsg_1053( IBuffer,
				      TotalInputLen,
				      OBuffer,
				      p_outputWritten,
				      output_type_flg,
				      ssmd_data_ptr,
				      0,		/* comm_fail_flg = 0 */
				      0 );		/*-> RT mode         */
}



/*=======================================================================
||																		||
||			 HandlerToMsg_1080											||
||	    Message handler for message 1080 -  Pharmacy Contract update.	||
||																		||
=======================================================================*/
int HandlerToMsg_1080 (char			*IBuffer,
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
	PHARMACY_INFO	Phrm_info;
	FILE			*outFP;
	ISOLATION_VAR;

	// SQL variables

	// INPUT MESSAGE
	TPharmNum				v_PharmNum;
	TInstituteCode			v_InstituteCode;
	TTerminalNum			v_TerminalNum;

	// RETURN MESSAGE
	// header
	TErrorCode				v_ErrorCode;
	TGenericYYYYMMDD		SysDate;
	TGenericHHMMSS			SysTime;
	long int				v_MaxNumToUpdate;

	// Pharmacy fields
	short					v_ContractType;
	short					v_ContractTax;
	int						v_ContractCode;
	int						v_ContractStart;
	int						v_Ph_Reserved_1 = 0L;
	int						v_Ph_Reserved_2 = 0L;
	int						v_Ph_Reserved_3 = 0L;

	// lines
	int						v_MinPrice;
	int						v_MaxPrice;
	int						v_Fee;
	int						v_Cont_Reserved = 0L;
	int						v_FPS_Group_Code;
	char					v_FPS_Group_Name [16];

	
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

	//GerrLogToFileName ("1080_log",
	//				   GerrId,
	//				   "Trn1080: Pharm. %d.",
	//				   (int)v_PharmNum);


	// Get ready to process the request.
	reStart = MAC_TRUE;

	sprintf (fileName,
			 "%s/pharm_contract_%0*d_%0*d_%0*d.snd",
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
						   "\tCant open output file '%s'\n\tError (%d) %s\n",
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

		// DonR 14Jul2005: This is an output-only transaction, so no need
		// for committed reads.
		SET_ISOLATION_DIRTY;

		// One-time loop to avoid GOTO's. Response is sent upon exit from this "loop".
		do
		{
			// Test if pharmacy has been OPENed.
			v_ErrorCode = IS_PHARMACY_OPEN (v_PharmNum, v_InstituteCode, &Phrm_info);
			if( v_ErrorCode != MAC_OK )
			{
				break;
			}

			// Get necessary data from Pharmacy table.
			ExecSQL (	MAIN_DB, TR1080_READ_pharmacy,
						&v_ContractType,	&v_ContractTax,		&v_ContractCode,	&v_ContractStart,
						&v_PharmNum,		&ROW_NOT_DELETED,	END_OF_ARG_LIST							);

			Conflict_Test( reStart );

			if( SQLERR_error_test() )
			{
				if (SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR))
				{
					break;
				}
			}

			// Declare and open Pharmacy Contract cursor.
			// DonR 13Jul2020: Try new DeclareAndOpen macro instead of separate operations.
			DeclareAndOpenCursor (	MAIN_DB, TR1080_pharmacy_contract_cur,
									&v_ContractCode, &SysDate, &SysDate, &ROW_NOT_DELETED, END_OF_ARG_LIST	);

			if (SQLERR_error_test ())
			{
				SetErrorVar (&v_ErrorCode, ERR_DATABASE_ERROR);
				break;
			}

			// Create the header with data so far.
			fprintf (outFP, "%0*d",		MSG_ID_LEN,				1081			);
			fprintf (outFP, "%0*d",		MSG_ERROR_CODE_LEN,		MAC_OK			);
			fprintf (outFP, "%0*d",		TPharmNum_len,			v_PharmNum		);
			fprintf (outFP, "%0*d",		TInstituteCode_len,		v_InstituteCode	);
			fprintf (outFP, "%0*d",		TTerminalNum_len,		v_TerminalNum	);
			fprintf (outFP, "%0*d",		TErrorCode_len,			v_ErrorCode		);
			fprintf (outFP, "%0*d",		TGenericYYYYMMDD_len,	SysDate			);
			fprintf (outFP, "%0*d",		TGenericHHMMSS_len,		SysTime			);
			fprintf (outFP, "%0*d",		1,						v_ContractType	);
			fprintf (outFP, "%0*d",		5,						v_ContractCode	);
			fprintf (outFP, "%0*d",		8,						v_ContractStart	);
			fprintf (outFP, "%0*d",		1,						v_ContractTax	);
			fprintf (outFP, "%0*d",		9,						v_Ph_Reserved_1	);
			fprintf (outFP, "%0*d",		9,						v_Ph_Reserved_2	);
			fprintf (outFP, "%0*d",		9,						v_Ph_Reserved_3	);
			fprintf (outFP, "%0*d",		TElectPR_LineNumLen,	v_MaxNumToUpdate);


			// Write the contracts data to file.
			// Loop until we hit end-of-fetch.
			for (;;)
			{
				// Fetch and test DB errors
				FetchCursorInto (	MAIN_DB, TR1080_pharmacy_contract_cur,
									&v_MinPrice,		&v_MaxPrice,		&v_Fee,
									&v_FPS_Group_Code,	&v_FPS_Group_Name,	END_OF_ARG_LIST	);

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

				// DonR 15Nov2021: Pharmacy is getting gibberish - so let's try disabling the
				// call to WinHebToDosHeb() and see if that helps.
				// DonR 30Nov2021: It turns out that the change that made SuperPharm happy made
				// everyone else unhappy - so I'm backing it out and SuperPharm will have to fix
				// the problem on their side.
				WinHebToDosHeb ((unsigned char *)v_FPS_Group_Name);

				// Write the Pharmacy Contracts row we fetched.
				v_MaxNumToUpdate++;

				fprintf (outFP, "%0*d",	TElectPR_LineNumLen,	v_MaxNumToUpdate	);
				fprintf (outFP, "%0*d",		9,						v_MinPrice			);
				fprintf (outFP, "%0*d",		9,						v_MaxPrice			);
				fprintf (outFP, "%0*d",		9,						v_Fee				);
				fprintf (outFP, "%0*d",		5,						v_FPS_Group_Code	);
				fprintf (outFP, "%-*.*s",	15, 15,					v_FPS_Group_Name	);
				fprintf (outFP, "%0*d",		7,						v_Cont_Reserved		);

			}	// Fetch contract lines loop.

			CloseCursor (	MAIN_DB, TR1080_pharmacy_contract_cur	);

			SQLERR_error_test();

		}
		while (0);	// One-time loop to avoid GOTO's.


		// Commit the transaction.
		if( reStart == MAC_FALS )	/* Tranaction OK then COMMIT	*/
		{
			CommitAllWork ();

			SQLERR_error_test();
		}
		else
		{
			if( TransactionRestart() != MAC_OK )
			{
				v_ErrorCode = ERR_DATABASE_ERROR;
				break;
			}

			GerrLogReturn (GerrId, "Table is locked for the <%d> time", tries);

			sleep( ACCESS_CONFLICT_SLEEP_TIME );

			fclose( outFP );
		}

	}	/* End of retries loop */

	//GerrLogToFileName ("1080_log",
	//				   GerrId,
	//				   "Wrote %d rows to file %s.",
	//				   (int)v_MaxNumToUpdate,
	//				   fileName);


	// REWRITE  MESSAGE HEADER.
	rewind( outFP );

	fprintf (outFP, "%0*d",		MSG_ID_LEN,				1081			);
	fprintf (outFP, "%0*d",		MSG_ERROR_CODE_LEN,		MAC_OK			);
	fprintf (outFP, "%0*d",		TPharmNum_len,			v_PharmNum		);
	fprintf (outFP, "%0*d",		TInstituteCode_len,		v_InstituteCode	);
	fprintf (outFP, "%0*d",		TTerminalNum_len,		v_TerminalNum	);
	fprintf (outFP, "%0*d",		TErrorCode_len,			v_ErrorCode		);
	fprintf (outFP, "%0*d",		TGenericYYYYMMDD_len,	SysDate			);
	fprintf (outFP, "%0*d",		TGenericHHMMSS_len,		SysTime			);
	fprintf (outFP, "%0*d",		1,						v_ContractType	);
	fprintf (outFP, "%0*d",		5,						v_ContractCode	);
	fprintf (outFP, "%0*d",		8,						v_ContractStart	);
	fprintf (outFP, "%0*d",		1,						v_ContractTax	);
	fprintf (outFP, "%0*d",		9,						v_Ph_Reserved_1	);
	fprintf (outFP, "%0*d",		9,						v_Ph_Reserved_2	);
	fprintf (outFP, "%0*d",		9,						v_Ph_Reserved_3	);
	fprintf (outFP, "%0*d",		TElectPR_LineNumLen,	v_MaxNumToUpdate);

	fclose( outFP );


	// Write the name of output-file on string for T-SWITCH.
	*p_outputWritten = sprintf( OBuffer, "%s", fileName );
	*output_type_flg = ANSWER_IN_FILE;

	ssmd_data_ptr->error_code	= v_ErrorCode;
	ssmd_data_ptr->pharmacy_num	= v_PharmNum;
	ssmd_data_ptr->terminal_num	= v_TerminalNum;

	RESTORE_ISOLATION;

	return( RC_SUCCESS );
}
// End of 1080 handler.



/*=======================================================================
||																		||
||			   SPOOL HANDLERS :											||
||																		||
 =======================================================================*/

 
/*=======================================================================
||																		||
||		 SPOOL mode of GENERIC_HandlerToMsg_1022_6022					||
||																		||
 =======================================================================*/
int   HandlerToSpool_1022_6022	(	int			TrnNumber_in,
									char		*IBuffer,
									int			TotalInputLen,
									char		*OBuffer,
									int			*p_outputWritten,
									int			*output_type_flg,
									SSMD_DATA	*ssmd_data_ptr,
									short		commfail_flag		)
{
    return GENERIC_HandlerToMsg_1022_6022	(	TrnNumber_in,
												IBuffer,
												TotalInputLen,
												OBuffer,
												p_outputWritten,
												output_type_flg,
												ssmd_data_ptr,
												commfail_flag,
												1 );		/*-> SPOOL mode */
}



/*=======================================================================
||									||
||		 SPOOL mode of GENERIC_HandlerToMsg_1053		||
||									||
 =======================================================================*/
int   HandlerToSpool_1053( char		*IBuffer,
			   int		TotalInputLen,
			   char		*OBuffer,
			   int		*p_outputWritten,
			   int		*output_type_flg,
			   SSMD_DATA	*ssmd_data_ptr,
			   short int	commfail_flag )
{
    return GENERIC_HandlerToMsg_1053( IBuffer,
				      TotalInputLen,
				      OBuffer,
				      p_outputWritten,
				      output_type_flg,
				      ssmd_data_ptr,
				      commfail_flag,
				      1 );		/*-> SPOOL mode */
}



// Macro to shorten calls to write_interaction_and_overdose().
#define INTERACT_STD_VARS	PharmNum_in,			InstCode_in,			TerminalNum_in,		\
							v_MemberIdentification, v_IdentificationCode,	dur_od_err,			\
							detail_err,				sq_printflg,			ErrorMode_in,		\
							ElectPR_flag_in,		SPres[i].DocID,			SPres[i].DocIDType,	\
							RecipeID_in,			SPres[i].DrugCode,		doc_approved_flag

/*=======================================================================
||																		||
||			 test_interaction_and_overdose ()							||
||																		||
||	    Generic Interaction/Overdose testing routine					||
||																		||
=======================================================================*/
int test_interaction_and_overdose (TMemberData						*Member,
								   TDate							MemMaxDrugDate_in,
//								   TGenericTZ						DocID,
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
								   short							*MsgFlag_out,
								   short							*MsgIsUrgent_out,
								   TErrorCode						*ErrorCode_out,
								   int								*ReStart_out)


{
	// Non-SQL variables.
	ISOLATION_VAR;
	int								i;
	int								is;
	int								id;
	int								doc_flg;
	int								reStart							= 0;
	int								severity_flg;
	int								divisor;
//	short							Declared_max_units_cur			= 0;
	TDate							SysDate;
	TDate							MinDate;
	TDate							MaxDate;
	TUnits							v_Units;
	float							v_Units_f;
	TErrorCode						v_ErrorCode						= 0;
	TMsgExistsFlg					v_MsgExistsFlg					= 0;
	TMsgExistsFlg					v_MsgIsUrgentFlg				= 0;
	TNumOfDrugLinesRecs				v_NumOfDrugLinesRecs			= NumLines_in;
	TNumofSpecialConfirmation		v_SpecialConfirmationNum		= SpecConfNum_in;
	TSpecialConfirmationNumSource	v_SpecialConfirmationNumSource	= SpecConfSource_in;

	PHARMACY_INFO				Phrm_info					= *PharmInfoPtr_in;
	TGenericTZ					v_MemberIdentification		= Member->ID;
	TIdentificationCode			v_IdentificationCode		= Member->ID_code;
	TDate						max_drug_date				= MemMaxDrugDate_in;
	TAge						MemberAge					= Member->age_months;
	TPresDrugs					*SPres						= DrugArray_in;


	short int				from_age;
	short int				DrugErr;
	short int				sq_printflg;
	short					doc_approved_flag;
	int						dur_od_err;
	int						detail_err;
	TDrugCode				v_DrugCode;
	TUnits					over_dose_limit;
	TInteractionType		interaction_type;
	TDate					sq_stopblooddate;
								   

	// DonR 16Jun2009: Per Iris Shaya, interaction/overdose messages from Transaction 1063 should be
	// handled the same as they are for Transaction 2003. As a TEMPORARY, quick-and-dirty way to
	// accomplish this, hard-code the incoming Error Mode paramater to 1 (= "new" mode); once the early
	// stages of testing are complete, this should be changed to eliminate the extra level in the
	// relevant arrays, and the ErrorMode paramater to this function should be eliminated.
	ErrorMode_in = 1;

	// Initialize variables.
	SysDate				= GetDate ();
	*ErrorCode_out		= NO_ERROR;

	// DonR 24Jan2018 CR #13937: Rather than having this logic duplicated in various transactions that
	// call this routine, do the member-validity test here. (QUESTION: Why is the value of the TZ-Code relevant?)
	// If the member is non-valid, OR is a Maccabi investigator (Member->check_od_interact == 0),
	// set everything as OK and return without checking for interactions/overdoses.
	if (( Member->ID		<  1)		||
		((Member->ID_code	== 0)	&& (!Member->check_od_interact)))
	{
		*MsgFlag_out		= 0;
		*MsgIsUrgent_out	= 0;
		*ErrorCode_out		= NO_ERROR;
		*ReStart_out		= 0;

		return (0);
	}
	// If we get here, we have an actual member to check!

	REMEMBER_ISOLATION;

	SET_ISOLATION_DIRTY;


	// Dummy loop to avoid GOTO.
	do
	{
		// Get drugs in member blood.
		// DonR 25May2010: get_drugs_in_blood() now handles old and new member ID, and is also smart enough
		// to decide whether it has to reload to drugs-in-blood buffer - so we can call it once, unconditionally.
		if (max_drug_date >= SysDate)
		{
			if (get_drugs_in_blood (Member,
									&v_DrugsBuf,
									GET_DRUGS_IN_BLOOD,
									&v_ErrorCode,
									&reStart)				== 1)
			{
				break;
			}	// high severity error OR deadlock occurred

		}	// Max_drug_date >= Sysdate.


		// Interaction tests.
		// Note that the "if" below will exclude only a few members from the tests for
		// interaction/overdose.
		if (IS_NEED_DUR_OD_TEST && IS_NEED_ANY_TEST)
		{
			for (i = 0; i < v_NumOfDrugLinesRecs; i++)
			{
				// Skip drugs with major errors already detected.
				// DonR 15May2017 DDI Pilot: Also skip drugs whose prescribing doctor is part of the pilot -
				// indicated by the check_interactions column in the doctors table, which is copied to SPres.
				// Note that drugs prescribed by a doctor participating in the DDI pilot are entirely "invisible"
				// if they are part of the current sale request; but they *are* visible if they have already been
				// purchased.
//				if (SPres[i].DFatalErr == MAC_TRUE)
				if ((SPres[i].DFatalErr == MAC_TRUE) || ((DDI_Pilot_Enabled) && (SPres[i].DocChkInteractions == 0)))
					continue;
		
				// test each drug in the prescription list against all drugs coming
				// after it in the list - so we don't test the same combination twice.
				for (is = i + 1; is < v_NumOfDrugLinesRecs; is++)
				{
					// DonR 15May2017 DDI Pilot: Skip drugs whose prescribing doctor is part of the pilot -
					// indicated by the check_interactions column in the doctors table, which is copied to SPres.
					// Note that drugs prescribed by a doctor participating in the DDI pilot are entirely "invisible"
					// if they are part of the current sale request; but they *are* visible if they have already been
					// purchased.
					if ((DDI_Pilot_Enabled) && (SPres[is].DocChkInteractions == 0))
						continue;

					interaction_type = 0;	// Reset default to non-interacting.

					// DonR 25Mar2020: There's no point in checking the database for an interaction unless we already
					// know that both drugs are in the drug_interaction table.
					if ((SPres[i].DL.in_drug_interact) && (SPres[is].DL.in_drug_interact))
					{
						if (IS_DRUG_INTERACAT (SPres[i].DrugCode,
											   SPres[is].DrugCode,
											   &interaction_type,
											   &v_ErrorCode)		== 1)
						{
							break;
						}	// high severity error occurred
					}
					// No need for an else here, since we already set interaction_type to zero as default.


					if (interaction_type)	// drugs have interaction
					{
						// DonR 26Dec2016: Check for Doctor Interaction Ishur.
						CheckForInteractionIshur (Member->ID,
												  Member->ID_code,
												  &SPres[i],			// Drug #1 in current sale.
												  &SPres[is],			// Drug #2 in current sale.
												  DocRxArray_in,		// Pass-through so routine knows doctor-prescription info.
												  NULL,					// Previously bought drug - not relevant here.
												  &doc_approved_flag,	// Will be written to presc_drug_inter/doc_approved_flag.
												  &interaction_type);	// Will be downgraded if severe but doctor approved it.

						// Calculate stop blood date
						sq_stopblooddate = AddDays (SysDate,
													(SPres[is].Duration + SPres[is].DL.t_half));

						// Set print flag + presc error + drug error + message flag
						// by severity + (not)same doctor
						severity_flg = (interaction_type == SEVER_INTERACTION) ?	SEVER :
																					NOT_SEVER;

						// DonR 26Dec2016: Because Digital Prescription transactions allow multiple doctors in
						// the same sale request, we can no longer assume that the intra-transaction test always
						// means the two drugs were prescribed by the same doctor.
						doc_flg = (SPres[is].DocID != SPres[i].DocID) ?	TWO_DOCTORS : SAME_DOCTOR;

						sq_printflg	= DurDecisionTable [ErrorMode_in] [severity_flg] [doc_flg] [PRINT_FLAG];

						DrugErr		= DurDecisionTable [ErrorMode_in] [severity_flg] [doc_flg] [DRUG_ERR_CODE];

						// DonR 28Mar2004: Separate error code to be stored in database, so that
						// we can force review in Trn. 1055 for Electronic Prescriptions.
						dur_od_err	= DurDecisionTable [ErrorMode_in] [severity_flg] [doc_flg] [DRUG_ERR_DB_FLG];
						detail_err	= DurDecisionTable [ErrorMode_in] [severity_flg] [doc_flg] [DUR_DETAIL_ERR];

						SetErrorVarArr (&SPres[i].DrugAnswerCode, DrugErr, SPres[i].DrugCode, i + 1, &SPres[i].DFatalErr, NULL);

						SetErrorVarArr (&v_ErrorCode,
										DurDecisionTable [ErrorMode_in] [severity_flg] [doc_flg] [PRESC_ERR_CODE],
										0, 0, NULL, NULL);

						if (!v_MsgExistsFlg)
						{
							v_MsgExistsFlg = DurDecisionTable [ErrorMode_in] [severity_flg] [doc_flg] [MESSAGE_FLAG];
						}

						write_interaction_and_overdose (INTERACT_STD_VARS,
														interaction_type, DUR_CHECK,
														i, is, NULL, &SPres[is], sq_stopblooddate);
						Conflict_Test (reStart);

						if (SQLERR_error_test ())
						{
							GerrLogToFileName ("elpresc_log", GerrId,
											   "Test_Interaction: DB error %d writing Interaction (1).",
											   SQLCODE);

							v_ErrorCode = ERR_DATABASE_ERROR;
							break;
						}

						if (SetErrorVarArr (&SPres[i].DrugAnswerCode, SPres[i].DrugAnswerCode, SPres[i].DrugCode, i + 1, NULL, NULL))
						{
							break;
						}

					}	// drugs have interaction.
				}	// Inner loop of drug-interaction test between drugs in prescription.

				if ((reStart == MAC_TRUE) || (v_ErrorCode != NO_ERROR))
					break;


				// Test drug[i] in prescription for interaction with drugs in BLOOD
				for (is = 0; is < v_DrugsBuf.max; is++)
				{
					// DonR 16Feb2006: Skip drugs that are no longer relevant. (They
					// may be in buffer, since it's now used for Special Prescription
					// Limit checking as well as Interaction/Overdose checking.
					if (v_DrugsBuf.Durgs[is].StopBloodDate < SysDate)
						continue;	// Skip to next drug in buffer.

					interaction_type = 0;	// Reset default to non-interacting.

					// DonR 25Mar2020: There's no point in checking the database for an interaction unless we already
					// know that both drugs are in the drug_interaction table.
					if ((SPres[i].DL.in_drug_interact) && (v_DrugsBuf.Durgs[is].in_drug_interact))
					{
						if (IS_DRUG_INTERACAT (SPres[i].DrugCode,
											   v_DrugsBuf.Durgs[is].Code,
											   &interaction_type,
											   &v_ErrorCode)				== 1)
						{
							break;
						}	// high severity error occurred
					}
					// No need for an else here, since we already set interaction_type to zero as default.


					// Exclude private source drugs
					if ((interaction_type != 0)	&&
						(v_DrugsBuf.Durgs[is].Source != RECIP_SRC_PRIVATE))
					{
						// DonR 26Dec2016: Check for Doctor Interaction Ishur.
						CheckForInteractionIshur (Member->ID,
												  Member->ID_code,
												  &SPres[i],				// Drug #1 in current sale.
												  NULL,						// Drug #2 in current sale - not relevant here.
  												  DocRxArray_in,			// Pass-through so routine knows doctor-prescription info.
												  &v_DrugsBuf.Durgs[is],	// Previously bought drug.
												  &doc_approved_flag,		// Will be written to presc_drug_inter/doc_approved_flag.
												  &interaction_type);		// Will be downgraded if severe but doctor approved it.

						// DonR 03Mar2010: Bug fix: we already know the correct stop-blood-date for
						// historical drug sales - and we certainly don't want to recalculate them
						// based on today's date when (in most cases) they were bought before today!
						sq_stopblooddate = v_DrugsBuf.Durgs[is].StopBloodDate;

						// Set print flag + presc error + drug error + message flag
						// by severity + (not)same doctor
						severity_flg	= (interaction_type == SEVER_INTERACTION) ?	SEVER :
																					NOT_SEVER;

						doc_flg = (v_DrugsBuf.Durgs[is].DoctorId != SPres[i].DocID ) ?	TWO_DOCTORS : SAME_DOCTOR;

						sq_printflg	= DurDecisionTable [ErrorMode_in] [severity_flg] [doc_flg] [PRINT_FLAG];

						DrugErr		= DurDecisionTable [ErrorMode_in] [severity_flg] [doc_flg] [DRUG_ERR_CODE];

						// DonR 28Mar2004: Separate error code to be stored in database, so that
						// we can force review in Trn. 1055 for Electronic Prescriptions.
						dur_od_err	= DurDecisionTable [ErrorMode_in] [severity_flg] [doc_flg] [DRUG_ERR_DB_FLG];
						detail_err	= DurDecisionTable [ErrorMode_in] [severity_flg] [doc_flg] [DUR_DETAIL_ERR];

						SetErrorVarArr (&SPres[i].DrugAnswerCode, DrugErr, SPres[i].DrugCode, i + 1, &SPres[i].DFatalErr, NULL);

						SetErrorVarArr (&v_ErrorCode,
										DurDecisionTable [ErrorMode_in] [severity_flg] [doc_flg] [PRESC_ERR_CODE],
										0, 0, NULL, NULL);

						if (!v_MsgExistsFlg)
						{
							v_MsgExistsFlg = DurDecisionTable [ErrorMode_in] [severity_flg] [doc_flg] [MESSAGE_FLAG];
						}

						write_interaction_and_overdose (INTERACT_STD_VARS,
														interaction_type, DUR_CHECK,
														i, is, &v_DrugsBuf.Durgs[is], NULL,
														sq_stopblooddate);
						Conflict_Test (reStart);

						if (SQLERR_error_test ())
						{
							GerrLogToFileName ("elpresc_log", GerrId,
											   "Test_Interaction: DB error %d writing Interaction (2).",
											   SQLCODE);

							v_ErrorCode = ERR_DATABASE_ERROR;
							break;
						}

						if (SetErrorVarArr (&SPres[i].DrugAnswerCode, SPres[i].DrugAnswerCode, SPres[i].DrugCode, i + 1, NULL, NULL))
						{
							break;
						}

					} /* interaction & source != private */
				} 	// Inner loop of drug-interaction test between drug in prescription and drugs in blood.
			}	// Outer loop for drug-interaction testing.
		}	// Drug-interaction testing is required.


		// Quit if we've hit a major error.
		if (reStart == MAC_TRUE)
			break;


		// OVER-DOSE test A (average daily dose test).
		if (IS_NEED_DUR_OD_TEST && IS_NEED_ANY_TEST)
		{
			for (i = 0; i < v_NumOfDrugLinesRecs; i++)
			{
					// Skip drugs with major errors already detected.
				if ((SPres[i].DFatalErr == MAC_TRUE)		||

					// DonR 29Mar2011: Skip overdose check for drugs that we know aren't in the over_dose table.
					(SPres[i].DL.in_overdose_table == 0)	||

					// Skip overdose check for non-prescription drugs.
					(SPres[i].DL.no_presc_sale_flag == MAC_TRUE))
				{
					continue;
				}

				// Get the over-dose limit.
				v_DrugCode = SPres[i].DrugCode;

				ExecSQL (	MAIN_DB, IntOD_max_units,
							&over_dose_limit,	&from_age,
							&v_DrugCode,		&MemberAge,	END_OF_ARG_LIST	);

				Conflict_Test (reStart);

				// Skip to next drug if no O.D. limit was found.
				if (SQLERR_code_cmp (SQLERR_not_found) == MAC_TRUE)
				{
					continue;
				}

				if (SQLERR_error_test ())
				{
					GerrLogToFileName ("elpresc_log", GerrId,
									   "Test_Interaction: DB error %d fetching max_units_cur.",
									   SQLCODE);

					v_ErrorCode = ERR_DATABASE_ERROR;
					break;
				}

				// Init test values
				SPres[i].over_dose_limit = over_dose_limit;

				if (over_dose_limit <= 0)
				{
					SetErrorVarArr (&v_ErrorCode,
									OD_DecisionTable [ErrorMode_in] [0] [OD_PRESC_ERR], 0, 0, NULL, NULL);

					SetErrorVarArr (&SPres[i].DrugAnswerCode,
									OD_DecisionTable [ErrorMode_in] [0] [OD_DRUG_ERR],
									SPres[i].DrugCode, i + 1, &SPres[i].DFatalErr, NULL);

					dur_od_err	= OD_DecisionTable [ErrorMode_in] [0] [OD_1055_ERR];
					detail_err	= OD_DecisionTable [ErrorMode_in] [0] [OD_DETAIL_ERR];
					sq_printflg	= OD_DecisionTable [ErrorMode_in] [0] [OD_PRINT_FLAG];

					doc_approved_flag = 0;	// No doctor approvals for overdoses!

					SPres[i].ATestOverDosed = MAC_TRUE;

					// Insert overdose details into table.
					write_interaction_and_overdose (INTERACT_STD_VARS,
													0 /* interaction_type */,
													OVER_DOSE_CHECK,
													i, i, NULL, &SPres[i],
													0 /* sq_stopblooddate */);
					Conflict_Test (reStart);

					if (SQLERR_error_test ())
					{
						GerrLogToFileName ("elpresc_log", GerrId,
										   "Test_Interaction: DB error %d writing Overdose (3).",
										   SQLCODE);

						v_ErrorCode = ERR_DATABASE_ERROR;
						break;
					}

					v_MsgExistsFlg = 1;
					continue;
				}	// Overdose Limit <= 0


				MinDate	= SysDate;
				MaxDate	= AddDays (SysDate, SPres[i].Duration);

				// Sum drugs in current sale.
				v_Units	= 0;
				for (is = 0; is < v_NumOfDrugLinesRecs; is++)
				{
					// At each iteration of the outer loop (for i...) we're looking at one
					// drug in the prescription. So the inner loop is going to sum up
					// only those prescriptions (and previously dispensed drugs still in
					// blood) with matching Largo Code.
					if (SPres[i].DrugCode != SPres[is].DrugCode)
						continue;

					// DonR 26Jan2011: At least for the moment, we're reverting to the previous logic:
					// that is, we compute overdose based only on the amount being sold, not on what
					// the doctor prescribed. This is because the new version has problems when the
					// pharmacy fills a prescription using a substitute with different dosage per OP.
					//
//					// DonR 12Jul2010: Overdose is now computed with reference to OP/Units as prescribed,
//					// which may be more than what's currently being sold. (For transactions 1063 and 2003,
//					// the numbers are the same.)
//					v_Units += SPres[is].Doctor_Units + (SPres[is].Doctor_Op * SPres[is].DL.package_size);
					v_Units += SPres[is].Units + (SPres[is].Op * SPres[is].DL.package_size);

					// Adjust testing period as required.
					if (SPres[is].Duration > SPres[i].Duration)
					{
						MaxDate = AddDays (SysDate, SPres[is].Duration);
					}
				}	// Inner loop to add up same-drug dosage prescribed.

				// Sum drugs in BLOOD.
				for (is = 0; is < v_DrugsBuf.max; is++)
				{
					// DonR 16Feb2006: Skip drugs that are no longer relevant. (They
					// may be in buffer, since it's now used for Special Prescription
					// Limit checking as well as Interaction/Overdose checking.
					if (v_DrugsBuf.Durgs[is].StopBloodDate < SysDate)
						continue;	// Skip to next drug in buffer.

					if (SPres[i].DrugCode != v_DrugsBuf.Durgs[is].Code)
						continue;

					// If the previous drug treatment should already be over, assume
					// that it's irrelevant to overdose-testing.
					// DonR 29Sep2005: StopDate is actually the first date AFTER all the
					// drugs have been taken - so we need to subtract one day from it
					// to get the last date when the previously-bought drugs were actually
					// taken.
					if (AddDays (v_DrugsBuf.Durgs[is].StopDate, -1) < SysDate)
						continue;

					v_Units +=   v_DrugsBuf.Durgs[is].Units
							   + (v_DrugsBuf.Durgs[is].Op * SPres[i].DL.package_size);

					// Adjust testing period as required.
					if (v_DrugsBuf.Durgs[is].StartDate < MinDate)
					{
						MinDate = v_DrugsBuf.Durgs[is].StartDate;
					}

					if (v_DrugsBuf.Durgs[is].StopDate > MaxDate)
					{
						MaxDate = v_DrugsBuf.Durgs[is].StopDate;
					}
				}	// Inner loop to add drugs in blood.


				// Now that amounts and dates have been computed, test for overdose.
				// DonR 29Sep2005: MaxDate is actually the first date AFTER all the
				// drugs have been taken - so we don't need to add a day to the
				// time interval to get the correct span.
				// DonR 06Oct2005: For better accuracy (and fewer false alarms), do the
				// computations and comparison using floats rather than integers. The
				// "rounding factor" is now smaller - instead of adding 1 to units, we
				// compute units accurately and add a smidgin to the limit to avoid
				// false overdose errors.
				divisor = GetDaysDiff (MaxDate, MinDate);
				if (divisor > 0)
					v_Units_f = (float)v_Units / (float)divisor;	// Total Units per day.
				else
					v_Units_f = (float)v_Units;


				if (v_Units_f > ((float)SPres[i].over_dose_limit + .0001))
				{
					// Overdose detected!
					SetErrorVarArr (&v_ErrorCode,
									OD_DecisionTable [ErrorMode_in] [1] [OD_PRESC_ERR], 0, 0, NULL, NULL);

					SetErrorVarArr (&SPres[i].DrugAnswerCode,
									OD_DecisionTable [ErrorMode_in] [1] [OD_DRUG_ERR],
									SPres[i].DrugCode, i + 1, &SPres[i].DFatalErr, NULL);

					dur_od_err	= OD_DecisionTable [ErrorMode_in] [1] [OD_1055_ERR];
					detail_err	= OD_DecisionTable [ErrorMode_in] [1] [OD_DETAIL_ERR];
					sq_printflg	= OD_DecisionTable [ErrorMode_in] [1] [OD_PRINT_FLAG];

					doc_approved_flag = 0;	// No doctor approvals for overdoses!

					SPres[i].ATestOverDosed = MAC_TRUE;

					// Insert same largo drugs details (for prescribed drugs) into
					// dur/overdose table.
					for (is = 0; is < v_NumOfDrugLinesRecs; is++)
					{
						// We're looking at only one Largo Code at a time.
						if (SPres[i].DrugCode != SPres[is].DrugCode)
							continue;

						write_interaction_and_overdose (INTERACT_STD_VARS,
														0 /* interaction_type */,
														OVER_DOSE_CHECK,
														i, is, NULL, &SPres[is],
														0 /* sq_stopblooddate */);
						Conflict_Test (reStart);

						if (SQLERR_error_test ())
						{
							GerrLogToFileName ("elpresc_log", GerrId,
											   "Test_Interaction: DB error %d writing Overdose (4).",
											   SQLCODE);

							v_ErrorCode = ERR_DATABASE_ERROR;
							break;
						}

						v_MsgExistsFlg = 1;
					}	// Loop on drugs prescribed to post overdoses to database.


					// Insert same largo drugs details (for drugs in blood) into
					// dur/overdose table.
					for (is = 0; is < v_DrugsBuf.max; is++)
					{
						// DonR 16Feb2006: Skip drugs that are no longer relevant. (They
						// may be in buffer, since it's now used for Special Prescription
						// Limit checking as well as Interaction/Overdose checking.
						if (v_DrugsBuf.Durgs[is].StopBloodDate < SysDate)
							continue;	// Skip to next drug in buffer.

						// We're looking at only one Largo Code at a time.
						if (SPres[i].DrugCode != v_DrugsBuf.Durgs[is].Code)
							continue;

						// DonR 11Jun2009: The lines below suppress printing of previous drug
						// sales if they were prescribed by the same doctor - but this logic
						// does not apply to overdoses! For overdoses, all previous sales that
						// are still "in the blood" are relevant.
//						sq_printflg =	(v_DrugsBuf.Durgs[is].DoctorId != SPres[i].DocID)	||
//										(interaction_type == SEVER_INTERACTION);

						write_interaction_and_overdose (INTERACT_STD_VARS,
														0 /* interaction_type */,
														OVER_DOSE_CHECK,
														i, is,
														&v_DrugsBuf.Durgs[is], NULL,
														0 /* sq_stopblooddate */);
						Conflict_Test (reStart);

						if (SQLERR_error_test ())
						{
							GerrLogToFileName ("elpresc_log", GerrId,
											   "Test_Interaction: DB error %d writing Overdose (5).",
											   SQLCODE);

							v_ErrorCode = ERR_DATABASE_ERROR;
							break;
						}

						v_MsgExistsFlg = 1;
					}	// Write Overdose data to DB for drugs in blood.
				}	// Overdose on average daily combined dose.
			}	// Outer loop on drugs to be tested for overdose.
		}	// Overdose testing is required.


		// If we've hit a severe database error, break out of dummy loop.
		if ((reStart == MAC_TRUE) || (v_ErrorCode == ERR_DATABASE_ERROR))
		{
			break;
		}



		// OVER-DOSE test B (daily dose test).
		if (IS_NEED_DUR_OD_TEST && IS_NEED_ANY_TEST)
		{
			for (i = 0; i < v_NumOfDrugLinesRecs; i++)
			{
				// If we've already found a major error for this drug, skip to next one.
				if ((SPres[i].DFatalErr == MAC_TRUE)		||

				// Skip drugs with negative/zero overdose limit.
					(SPres[i].over_dose_limit <= 0)			||

				// Skip drugs for which we've already detected an overdose.
					(SPres[i].ATestOverDosed == MAC_TRUE)	||

				// DonR 29Mar2011: Skip overdose check for drugs that we know aren't in the over_dose table.
					(SPres[i].DL.in_overdose_table == 0)	||

				// Skip testing on non-prescription drugs.
					(SPres[i].DL.no_presc_sale_flag == MAC_TRUE))
					continue;


				// Loop through days this drug will be taken.
				for (id = 0; id < SPres[i].Duration; id++)
				{
					MinDate = AddDays (SysDate, id);

					v_Units = 0;

					// Sum up amount of this drug in prescription.
					for (is = 0; is < v_NumOfDrugLinesRecs; is++)
					{
						if (SPres[i].DrugCode != SPres[is].DrugCode)
							continue;

						// If drug[is] is already finished by day [id], then ignore it.
						if (id >= SPres[is].Duration)
							continue;

						// Add average daily dose of drug[is] to total daily dose of
						// the drug.
						// DonR 07Aug2005: Fixed bug introduced in Version 81 (11Aug2003). We
						// need to divide by duration *before* adding the current purchase
						// to the total!



						// DonR 31Jan2011: At least for the moment, we're reverting to the previous logic:
						// that is, we compute overdose based only on the amount being sold, not on what
						// the doctor prescribed. This is because the new version has problems when the
						// pharmacy fills a prescription using a substitute with different dosage per OP.
						//
//						// DonR 12Jul2010: Overdose is not computed with reference to OP/Units as prescribed,
//						// which may be more than what's currently being sold. (For transactions 1063 and 2003,
//						// the numbers are the same.)
//						v_Units +=	((SPres[is].Doctor_Units + (SPres[is].Doctor_Op * SPres[is].DL.package_size)) /
						v_Units +=	((SPres[is].Units + (SPres[is].Op * SPres[is].DL.package_size)) /
									 ((SPres[is].Duration > 0) ? SPres[is].Duration : 1));
					}	// Loop through drugs prescribed.

					// Add previously-prescribed drugs in BLOOD to the daily total.
					for (is = 0; is < v_DrugsBuf.max; is++)
					{
						// DonR 16Feb2006: Skip drugs that are no longer relevant. (They
						// may be in buffer, since it's now used for Special Prescription
						// Limit checking as well as Interaction/Overdose checking.
						if (v_DrugsBuf.Durgs[is].StopBloodDate < SysDate)
							continue;	// Skip to next drug in buffer.

						if (SPres[i].DrugCode != v_DrugsBuf.Durgs[is].Code)
							continue;

						// If previously-prescribed drug treatment has been completed
						// before day [id], skip it.
						// DonR 29Sep2005: StopDate is actually the first date AFTER all the
						// drugs have been taken - so we need to subtract one day from it
						// to get the last date when the previously-bought drugs were actually
						// taken.
						if (AddDays (v_DrugsBuf.Durgs[is].StopDate, -1) < MinDate)
							continue;

						// Add average daily dose of drug[is] to total daily dose of
						// the drug.
						// DonR 07Aug2005: Fixed bug introduced in Version 81 (11Aug2003). We
						// need to divide by duration *before* adding the old purchase to the total!
						v_Units +=	((v_DrugsBuf.Durgs[is].Units + (v_DrugsBuf.Durgs[is].Op * SPres[i].DL.package_size)) /
									 ((v_DrugsBuf.Durgs[is].Duration > 0) ? v_DrugsBuf.Durgs[is].Duration : 1));
					}	// Loop through drugs in blood.


					// Test computed daily total against overdose limit.
					if (v_Units > SPres[i].over_dose_limit)
					{
						// DonR 11Sep2005: Per Raya Kroll, eliminate the warning message
						// for daily overdoses. If we detect an overdose, jump straight
						// to the "hard error" stage.

						SetErrorVarArr (&v_ErrorCode,
										OD_DecisionTable [ErrorMode_in] [3] [OD_PRESC_ERR], 0, 0, NULL, NULL);

						SetErrorVarArr (&SPres[i].DrugAnswerCode,
										OD_DecisionTable [ErrorMode_in] [3] [OD_DRUG_ERR],
										SPres[i].DrugCode, i + 1, &SPres[i].DFatalErr, NULL);

						dur_od_err	= OD_DecisionTable [ErrorMode_in] [3] [OD_1055_ERR];
						detail_err	= OD_DecisionTable [ErrorMode_in] [3] [OD_DETAIL_ERR];
						sq_printflg	= OD_DecisionTable [ErrorMode_in] [3] [OD_PRINT_FLAG];

						doc_approved_flag = 0;	// No doctor approvals for overdoses!

						// Insert same largo drugs details (from prescribed drugs)
						// into dur/overdose table.
						for (is = 0; is < v_NumOfDrugLinesRecs; is++)
						{
							// Skip other drugs.
							if( SPres[i].DrugCode != SPres[is].DrugCode)
								continue;

							write_interaction_and_overdose (INTERACT_STD_VARS,
															0 /* interaction_type */,
															OVER_DOSE_CHECK,
															i, is, NULL, &SPres[is],
															0 /* sq_stopblooddate */);
							Conflict_Test (reStart);

							if (SQLERR_error_test ())
							{
								GerrLogToFileName ("elpresc_log", GerrId,
												   "Test_Interaction: DB error %d writing Overdose (6).",
												   SQLCODE);

								v_ErrorCode = ERR_DATABASE_ERROR;
								break;
							}

							v_MsgExistsFlg = 1;
						}	// Add relevant prescribed drugs to Overdose table.


						// Insert same largo drugs details (from drugs in blood)
						// into dur/overdose table.
						for (is = 0; is < v_DrugsBuf.max; is++)
						{
							// DonR 16Feb2006: Skip drugs that are no longer relevant. (They
							// may be in buffer, since it's now used for Special Prescription
							// Limit checking as well as Interaction/Overdose checking.
							if (v_DrugsBuf.Durgs[is].StopBloodDate < SysDate)
								continue;	// Skip to next drug in buffer.

							// Skip other drugs.
							if (SPres[i].DrugCode != v_DrugsBuf.Durgs[is].Code)
								continue;

							// DonR 11Jun2009: The lines below are irrelevant, since printing
							// of previous sales for Overdose has nothing to do with which
							// doctor prescribed the drug.
							// Under certain circumstances, force print flag true even
							// if it otherwise wouldn't be.
//							if ((v_DrugsBuf.Durgs[is].DoctorId	!= SPres[i].DocID)				||
//								(interaction_type				== SEVER_INTERACTION))
//							{
//								sq_printflg = PRINT;
//							}

							write_interaction_and_overdose (INTERACT_STD_VARS,
															0 /* interaction_type */,
															OVER_DOSE_CHECK,
															i, is,
															&v_DrugsBuf.Durgs[is], NULL,
															0 /* sq_stopblooddate */);
							Conflict_Test (reStart);

							if (SQLERR_error_test ())
							{
								GerrLogToFileName ("elpresc_log", GerrId, 
												   "Test_Interaction: DB error %d writing Overdose (7).",
												   SQLCODE);

								v_ErrorCode = ERR_DATABASE_ERROR;
								break;
							}

							v_MsgExistsFlg = 1;
						}	// Add relevant drugs in blood to Overdose table.


						// Once we've seen an overdose for one day for this drug, we
						// don't have to keep checking further days of treatment.
						break;

					}	// Daily overdose detected.
				}	// Loop through days of treatment with drug[i].
			}	// Outer loop through drugs for daily O/D testing.
		}	// Need to perform daily overdose testing.

	}
	while (0);
	// End of big dummy loop.


	*MsgFlag_out		= v_MsgExistsFlg;
	*MsgIsUrgent_out	= v_MsgIsUrgentFlg;
	*ErrorCode_out		= v_ErrorCode;
	*ReStart_out		= reStart;

//	// IntOD_max_units_cur is "sticky" since it's called multiple times;
//	// so it needs to be freed explicitly.
//	FreeStatement (	MAIN_DB, IntOD_max_units_cur	);
//
	RESTORE_ISOLATION;

	return (0);
}	// End of test_interaction_and_overdose ().



/*=======================================================================
||																		||
||			 write_interaction_and_overdose ()							||
||																		||
||	    Generic Interaction/Overdose write-to-database routine			||
|| NOTE: All error handling is done by the calling routine!				||
||																		||
=======================================================================*/
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
									int								StopBloodDate_in)


{
	TPharmNum					v_PharmNum					= PharmNum_in;
	TInstituteCode				v_InstituteCode				= InstCode_in;
	TTerminalNum				v_TerminalNum				= TerminalNum_in;
	TInteractionType			interaction_type			= interaction_type_in;
	TGenericTZ					v_MemberIdentification		= MemberID_in;
	TIdentificationCode			v_IdentificationCode		= MemIdCode_in;
	int							dur_od_err					= dur_od_err_in;
	int							detail_err					= detail_err_in;
	short						CheckType					= CheckType_in;	// DonR 09Feb2020: The DB column is a SMALLINT, so this needs to be a short integer.
	TGenericTZ					DocID						= DocID_in;
	TTypeDoctorIdentification	DocIdType					= DocIdType_in;
	short int					PrintFlag					= PrintFlag_in;
	short int					ErrorMode					= ErrorMode_in;
	short int					ElectronicPr				= ElectronicPr_in;

	TRecipeIdentifier			v_RecipeIdentifier			= RecipeID_in;
	TDrugCode					Largo1						= Largo1_in;
	short						doc_approved_flag			= doc_approved_flag_in;
	short int					LineNum						= Subscript1_in + 1;

	short int					LineNum2					= Subscript2_in + 1;
	int							StopBloodDate				= StopBloodDate_in;

	TDate					SysDate;
	THour					SysTime;
	DURDrugsInBlood			SecondDrug;
	short					ShortZero	= 0;
	int						IntZero		= 0;


	SysDate = GetDate ();
	SysTime = GetTime ();

	// Paranoia time: If this is an overdose rather than an interaction, force doc_approved_flag
	// to zero. (This may change if we ever add the ability for doctors to approve overdoses - which
	// is probably a rather dangerous idea!)
	if (CheckType != DUR_CHECK)
		doc_approved_flag = 0;

	// Depending on which type of structure was passed for the second
	// drug, store the appropriate values in SecondDrug.
	if (BloodDrug_in != NULL)
	{
		// For overdoses, second drug code is zero.
		if (CheckType == DUR_CHECK)
			SecondDrug.Code = BloodDrug_in->Code;
		else
			SecondDrug.Code = 0;

		SecondDrug.PrescriptionID	= BloodDrug_in->PrescriptionID;
		SecondDrug.LineNo			= BloodDrug_in->LineNo;
		SecondDrug.StartDate		= BloodDrug_in->StartDate;
		SecondDrug.DoctorId			= BloodDrug_in->DoctorId;
		SecondDrug.DoctorIdType		= BloodDrug_in->DoctorIdType;
		SecondDrug.Units			= BloodDrug_in->Units;
		SecondDrug.Op				= BloodDrug_in->Op;
		SecondDrug.Duration			= BloodDrug_in->Duration;
	}
	else
	if (PresDrug_in != NULL)
	{
		// For overdoses, second drug code is zero.
		// DonR 12Jul2010: Overdose is not computed with reference to OP/Units as prescribed,
		// which may be more than what's currently being sold. (For transactions 1063 and 2003,
		// the numbers are the same.)
		// DonR 31Jan2011: At least for the moment, we're reverting to the previous logic:
		// that is, we compute overdose based only on the amount being sold, not on what
		// the doctor prescribed. This is because the new version has problems when the
		// pharmacy fills a prescription using a substitute with different dosage per OP.
		if (CheckType == DUR_CHECK)
			SecondDrug.Code = PresDrug_in->DrugCode;
		else
			SecondDrug.Code = 0;

		SecondDrug.PrescriptionID	= RecipeID_in;
		SecondDrug.LineNo			= LineNum2;
		SecondDrug.StartDate		= SysDate;
		SecondDrug.DoctorId			= PresDrug_in->DocID;			// DonR 04Apr2017: Take from structure, since Trn. 6003 can have multiple docs!
		SecondDrug.DoctorIdType		= PresDrug_in->DocIDType;		// DonR 04Apr2017: Take from structure, since Trn. 6003 can have multiple docs!
//		SecondDrug.Units			= PresDrug_in->Doctor_Units;	// DonR 12Jul2010.
//		SecondDrug.Op				= PresDrug_in->Doctor_Op;		// DonR 12Jul2010.
		SecondDrug.Units			= PresDrug_in->Units;			// DonR 31Jan2011.
		SecondDrug.Op				= PresDrug_in->Op;				// DonR 31Jan2011.
		SecondDrug.Duration			= PresDrug_in->Duration;
															/* Drug 2 columns */
	}
	else	// Both pointers are NULL - set (almost) everything to zero.
	{
		SecondDrug.Code = 0;

		SecondDrug.PrescriptionID	= RecipeID_in;
		SecondDrug.LineNo			= LineNum2;
		SecondDrug.StartDate		= SysDate;
		SecondDrug.DoctorId			= DocID_in;
		SecondDrug.DoctorIdType		= DocIdType_in;
		SecondDrug.Units			= 0;
		SecondDrug.Op				= 0;
		SecondDrug.Duration			= 0;
	}


	ExecSQL (	MAIN_DB, IntOD_INS_presc_drug_inter,

				/* Variable header columns */
				&v_PharmNum,				&v_InstituteCode,
				&v_TerminalNum,				&SysDate,
				&SysTime,					&v_MemberIdentification,
				&v_IdentificationCode,		&dur_od_err,
				&detail_err,				&PrintFlag,
				&ElectronicPr,				&ErrorMode,
				&interaction_type,			&CheckType,

				/* Constant header columns */
				&IntZero,	/* spec PR # */	&ShortZero,	/* spec PR src */
				&ROW_NOT_DELETED,			&NOT_REPORTED_TO_AS400,
				&PHARMACIST,				&doc_approved_flag,

				/* Drug 1 columns */
				&v_RecipeIdentifier,		&LineNum,
				&Largo1,					&DocID,
				&DocIdType,

				/* Drug 2 columns */
				&SecondDrug.Code,			&SecondDrug.PrescriptionID,
				&SecondDrug.LineNo,			&SecondDrug.DoctorId,
				&SecondDrug.DoctorIdType,	&SecondDrug.Duration,
				&SecondDrug.Op,				&SecondDrug.Units,
				&SecondDrug.StartDate,		&StopBloodDate,
				END_OF_ARG_LIST												);

	return (0);
}



void log_serious_error_text	(int err, char *msg)
{
	GerrLogReturn (GerrId,
				   "SqlServer: Serious error %d - message = \"%s\".",
				   err, msg);
}


void log_serious_error_int	(int err, int err_identifier)
{
	GerrLogReturn (GerrId,
				   "SqlServer: Serious error %d - identifier = %d.",
				   err, err_identifier);
}

/*				   -- EOF --				     */
