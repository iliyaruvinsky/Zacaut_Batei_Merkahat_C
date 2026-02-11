/*=======================================================================
||																		||
||				SqlServer.c												||
||																		||
||======================================================================||
||																		||
||  Description:                                                        ||
||  -----------                                                         ||
||  Sql server process.													||
||  When MACABI system is going up , the father process runs few copies	||
||    of this process and keep minimum non-busy copies during			||
||    system is up														||
||  This process opens a connection to db when waking up and wait for	||
||    sql requests from PC.												||
||  On request , this process executes the request and returns to wait	||
||  On MACABI system shutdown , this process closes connection to db	||
||																		||
||======================================================================||
||  WRITTEN BY	: Ely Levy ( Reshuma )									||
||  DATE	: 30.05.1996												||
||  REVISION	: 01													||
 =======================================================================*/

#define __IN_PROCESS_MESSAGE__
#define MAIN

static char *GerrSource = __FILE__;

#include <MacODBC.h>
#include "MsgHndlr.h"
#include "PharmacyErrors.h"
#include "TikrotRPC.h"
#include "MessageFuncs.h"
#include <cJSON.c>

extern char	*curr_pos_in_mesg;

#define	GET_POS()	(curr_pos_in_mesg)

#define Conflict_Test(r)									\
if (SQLERR_code_cmp (SQLERR_access_conflict) == MAC_TRUE)	\
{															\
    SQLERR_error_test ();									\
    r = MAC_TRUE;											\
    break;													\
}

// Global variables.
int				accept_sock				= -1;
static int		mypid;
static int		message_count			= 0;
int				caught_signal;
int				NeedToUpdateSysparams	= 0;
double			no_vat_multiplier		= 0.85470085;	// Default (@ 17% VAT) until re-calculated from sysparams VAT percent.
short			Form29gMessagesEnabled	= 9999;			// DonR 21Oct2015 TEMPORARY.

static char		MyShortName [3];
int				ExpensiveDrugMacDocRx	= 50000;		// DonR 23Jun2024 User Story #318200. "Real" value comes from sysparams/ExpensiveDrugMacDocRx.
int				ExpensiveDrugNotMacDocRx= 50000;		// DonR 23Jun2024 User Story #318200. "Real" value comes from sysparams/ExpensiveDrugNotMacDocRx.
short			min_refill_days			= 14;
int				OverseasMaxSaleAmt		= 50000;		// DonR 12Mar2012.
short			SaleOK_EarlyDays;						// DonR 01Nov2017 - Digital Prescriptions.
short			SaleOK_LateDays;						// DonR 25Feb2015 - Digital Prescriptions.
short			svc_w_o_card_days;						// DonR 13Feb2018 - fix to service-without-card processing.
short			svc_w_o_card_days_pvt;					// DonR 19Oct2023 User Story #487690 - add long-term svc-without-card for private pharmacies.
short			no_show_reject_num;						// DonR 03Jan2018 CR #13453.
short			no_show_hist_days;						// DonR 03Jan2018 CR #13453.
short			no_show_warn_code;						// DonR 03Jan2018 CR #13453.
short			no_show_err_code;						// DonR 03Jan2018 CR #13453.
float			vat_percent				= 16.0;			// DonR 14Nov2011: Fixed default.
double			IngrSubstMaxRatio		=  2.0;			// DonR 12Jul2018  "special" ingredient-based substitution outside EconomyPri group.
short			usage_memory_mons;						// DonR 21Jun2018: Online Orders (SuperPharm).
short			max_order_sit_days;						// DonR 21Jun2018: Online Orders (SuperPharm).
short			max_order_work_hrs;						// DonR 21Jun2018: Online Orders (SuperPharm).
short			NihulTikrotLargoLen;					// DonR 30Jul2024 User Story #338533.
char			SickDscLargoTypes		[21];			// DonR 13May2020 CR #31591.
char			VentDscLargoTypes		[21];			// DonR 13May2020 CR #31591.
char			MembDscLargoTypes		[21];			// DonR 13May2020 CR #31591.
int				MinimumNormalMemberTZ;					// DonR 29Jun2020 CR #28453.
short			MaxShortDurationDays			= 25;	// DonR 25Nov2024 User Story #366220.
short			MaxShortDurationOverlapDays		=  5;	// DonR 25Nov2024 User Story #366220.
short			MaxLongDurationOverlapDays		= 15;	// DonR 25Nov2024 User Story #366220.
short			MaxPharmIshurConfirmAuthority	= 24;	// DonR 27Apr2025 User Story #390071.
short			MaxDoctorRuleConfirmAuthority	= 24;	// DonR 23Apr2025 User Story #390071.
short			DrugShortageRuleConfAuthority	= 17;	// DonR 13May2025 User Story #388766.


typedef enum
{
    BEFORE_UPDATE,
    AFTER_UPDATE,
    CONN_HANGUP
}
SEND_STATES;

int	SpoolErr[] =			/* Errors to spool messages	*/
{
    ERR_NO_MEMORY,
    ERR_DATABASE_ERROR,
    ERR_SHARED_MEM_ERROR,
    0
};

SEND_STATES	send_state;

#define	NORMAL_EXIT		0
#define	ENDLESS_LOOP	1

void		ClosedPipeHandler				(int	signo);
void		TerminateHandler				(int	signo);
void		NihulTikrotEnableHandler		(int	signo);
void		NihulTikrotDisableHandler		(int	signo);
int			init_package_size_array			(void);
int			TerminateProcess				(int	signo, int in_loop);
int			process_spools					(void);
int			LoadTrnPermissions				(void);
struct sigaction	sig_act_pipe;
struct sigaction	sig_act_terminate;
struct sigaction	sig_act_NihulTikrotEnable;
struct sigaction	sig_act_NihulTikrotDisable;

/*=======================================================================
||																		||
||																		||
||				main()													||
||																		||
||																		||
 =======================================================================*/
int	main (int argc, char *argv[])
{
	// Local variables.
	char		buffer[131072];
	sigset_t	NullSigset;
	static int	LastDate = 0;
	static int	LastTime = 0;
	int			rc;

	ISOLATION_VAR;


	short	TikrotEnabled;
	short	EnablePPlusMeishar;
	short	PrSrcNum;
	short	PrSrcDocLocnReq;
	short	UseSaleTables;
	short	test_sale_equality;
	short	check_card_date;
	short	ddi_pilot_enabled;
	int		MaxDailyBuy_Pvt;
	int		MaxDailyBuy_PPlus;
	int		MaxDailyBuy_Mac;
	int		MaxOtcCreditPerMonth;
	int		CycleOverlap;
	int		nocard_ph_maxvalid;
	int		nocard_ph_pvt_kill;
	int		nocard_ph_mac_kill;
	short	round_pkgs_2_pharm;
	short	round_pkgs_sold;
	short	max_unsold_op_pct;
	float	max_units_op_off;
	short	del_valid_months;
	short	illness_bit_number;
	short	override_basket;
	short	is_actual_disease;
	short	use_diagnoses;
	short	bakarakamutit_mons;
	int		MinNormalMemberTZ;
	int		RowsInTable;	// DonR 21Oct2015: TEMPORARY!

//	// TEMPORARY FOR TESTING.
//	short	MinErrorCode;
//	short	MaxErrorCode;
//	char	ErrorLine	[101];
//	char	DB_Time1	[ 21];
//	char	DB_Time2	[ 21];
//	int		SysDate;
//	int		SysTime;
//	int		SQLCODE0;
//

	/*=======================================================================
	||																		||
	||			   GET READY TO WORK										||
	||																		||
	||======================================================================||
	||			Get command line arguments									||
	=======================================================================*/
	GetCommonPars (argc, argv, &retrys);

	// Initialize global drugs-in-blood buffer.
	v_DrugsBuf.Durgs	= (DURDrugsInBlood*)NULL;
	v_DrugsBuf.max		= 0;
	v_DrugsBuf.size		= 0;

	// DonR 26Mar2020: Set global parameter to suppress all calls to the old Informix
	// database connection.
	Connect_ODBC_Only	= 1;

	// DonR 22Nov2021: Set global "translate Windows Hebrew to UTF-8" switch ON.
	Global_JSON_TranslateWin1255ToUTF8 = 1;

	// Initialize "last sign of life" message buffer to length zero.
	LastSignOfLife[0] = (char)0;

	// Set global System Date variable.
	GlobalSysDate = GetDate ();	// Avoid extra GetDate() calls by storing the value in a global variable.

	/*=======================================================================
	||			INITIALIZE SON PROCESS										||
	=======================================================================*/
	RETURN_ON_ERR (InitSonProcess (argv[0], SQLPROC_TYPE, retrys, 0, PHARM_SYS));

	// Install signal handlers for Closed Pipe, Software Terminate (signal 15),
	// Floating Point Error (div. by zero, signal 8) and Segmentation Error
	// (signal 11).
	caught_signal = 0;
	memset ((char *)&NullSigset, 0, sizeof(sigset_t));

    sig_act_pipe.sa_handler					= ClosedPipeHandler;
    sig_act_pipe.sa_mask					= NullSigset;
    sig_act_pipe.sa_flags					= 0;

    sig_act_terminate.sa_handler			= TerminateHandler;
    sig_act_terminate.sa_mask				= NullSigset;
    sig_act_terminate.sa_flags				= 0;

    sig_act_NihulTikrotEnable.sa_handler	= NihulTikrotEnableHandler;
    sig_act_NihulTikrotEnable.sa_mask		= NullSigset;
    sig_act_NihulTikrotEnable.sa_flags		= 0;

    sig_act_NihulTikrotDisable.sa_handler	= NihulTikrotDisableHandler;
    sig_act_NihulTikrotDisable.sa_mask		= NullSigset;
    sig_act_NihulTikrotDisable.sa_flags		= 0;


	if (sigaction (SIGPIPE, &sig_act_pipe, NULL) != 0)
	{
		GerrLogReturn (GerrId,
					   "Can't install signal handler for SIGPIPE" GerrErr,
					   GerrStr);
	}

	if (sigaction (SIGTERM, &sig_act_terminate, NULL) != 0)
	{
		GerrLogReturn (GerrId,
					   "Can't install signal handler for SIGTERM" GerrErr,
					   GerrStr);
	}

	if (sigaction (SIGFPE, &sig_act_terminate, NULL) != 0)
	{
		GerrLogReturn (GerrId,
					   "Can't install signal handler for SIGFPE" GerrErr,
					   GerrStr);
	}

	// DonR 26May2022: Instead of setting the Segmentation Fault handler here,
	// notify MacODBC that this is the handler to use instead of its default.
	SetCustomSegmentationFaultHandler (TerminateHandler);
//	if (sigaction (SIGSEGV, &sig_act_terminate, NULL) != 0)
//	{
//		GerrLogReturn (GerrId,
//					   "Can't install signal handler for SIGSEGV" GerrErr,
//					   GerrStr);
//	}

	if (sigaction (SIGUSR1, &sig_act_NihulTikrotEnable, NULL) != 0)
	{
		GerrLogReturn (GerrId,
					   "Can't install signal handler for SIGUSR1 (enable Nihul Tikrot)" GerrErr,
					   GerrStr);
	}

	if (sigaction (SIGUSR2, &sig_act_NihulTikrotDisable, NULL) != 0)
	{
		GerrLogReturn (GerrId,
					   "Can't install signal handler for SIGUSR2 (disable Nihul Tikrot)" GerrErr,
					   GerrStr);
	}


	/*=======================================================================
	||			CONNECT TO DATABASE											||
	=======================================================================*/

	mypid = getpid ();

#ifdef DEBUG
	printf ("here is macuser '%s' macpass '%s' macdb '%s'\n", MacUser, MacPass, MacDb);
	fflush (stdout);
#endif

	// DonR 03Mar2021: Set MS-SQL attributes for query timeout and deadlock priority.
	// For the Pharmacy Server program, we want a shortish timeout (since we have
	// retries set up for pretty much everything) and a high deadlock priority (since
	// we want to privilege getting responses to pharmacy over things like sending
	// updates to AS/400).
	//
	// DonR 05Apr2021: Set ODBC_PRESERVE_CURSORS TRUE, so that COMMITs can be performed
	// without having to re-open cursors. This is in support of adding an in-loop
	// COMMIT to ProcessRxLateArrivalQueue() - which may help avoid deadlocking issues.
	//
	// DonR 06May2021: My previous attempt at getting rid of database-contention errors appears to have
	// caused more (and worse) problems than it solved - database locks were being left open, and after
	// a few hours the whole application would grind to a halt. Instead, I've backed out the in-loop
	// CommitAllWork() in ProcessRxLateArrivalQueue(), switched ODBC_PRESERVE_CURSORS back to 0 (= FALSE)
	// for this program, and added a "{FIRST} 50" to the ProcessRxLateArrivalQ_ProcRxArrivals_cur SELECT
	// to limit the number of late-arriving prescriptions we'll look for in any one call to that routine.
	// Basically, it looks like ODBC_PRESERVE_CURSORS should be set TRUE only for relatively simple
	// programs like ShrinkPharm or As400UnixClient, where it's easy to make sure that all cursors are
	// explicitly closed when necessary; SqlServer is so complex that this would be difficult to implement
	// reliably, and the benefit isn't worth the cost.
	LOCK_TIMEOUT			= 250;	// Milliseconds.
	DEADLOCK_PRIORITY		= 8;	// 0 = normal, -10 to 10 = low to high priority.
//	ODBC_PRESERVE_CURSORS	= 1;	// So all cursors will be preserved after COMMITs. DISABLED.
	ODBC_PRESERVE_CURSORS	= 0;	// Re-affirm the default: close all cursors after COMMITs.

	// DonR 15Jan2020: The standard connect routine SQLMD_connect() (which actually resolves
	// to INF_CONNECT() ) now handles connections to the "old" Informix connection, plus ODBC
	// connections to Informix and MS-SQL, assuming the relevant environment variables are set.
	do
	{
		SQLMD_connect ();
		if (!MAIN_DB->Connected)
		{
			sleep (10);
			GerrLogMini (GerrId, "\n\nRetrying %s ODBC connect.", MAIN_DB->Name);
		}
	}
	while (!MAIN_DB->Connected);

	if (SQLERR_error_test() != MAC_OK)
	{
		ExitSonProcess (MAC_SERV_RESET);
	}


	// DonR 30Aug2020: This COMMIT shouldn't really be necessary, but maybe it
	// will prevent some errors when setting isolation.
	CommitAllWork ();

	// DonR 02Aug2007: Initialize package-size array used by Trn. 2088.
	init_package_size_array ();

	// DonR 11Jul2012: Maximum Daily Purchase Limit array defaults to all-zero.
	memset ((char *)MaxDailyBuy, 0, sizeof(MaxDailyBuy));

	// DonR 30Jun2005: Get two-character short name for this computer.
	strcpy (MyShortName, GetComputerShortName());

//
//	// TEMPORARY FOR TESTING.
//
//	// 1) Compare 3 timestamps.
//	SysTime = GetTime ();
//	ExecSQL (	MAIN_DB, READ_GetCurrentDatabaseTime, &DB_Time1, END_OF_ARG_LIST	);
//	SQLCODE0 = SQLCODE;
//	ExecSQL (	ALT_DB, READ_GetCurrentDatabaseTime, &DB_Time2, END_OF_ARG_LIST	);
//	GerrLogMini (GerrId, "PID %d: Linux time %d, Main %s time %s, Alt %s time %s, SQLCODE0 = %d, SQLCODE = %d.", mypid, SysTime, MAIN_DB->Name, DB_Time1, ALT_DB->Name, DB_Time2, SQLCODE0, SQLCODE);
//
//	// 2) Force cardinality violation in both databases (and, incidentally, test using
//	//    the same SQL operation on both databases).
//	MinErrorCode = 6230;
//	MaxErrorCode = 6231;
//	ExecSQL (	MAIN_DB, TestReadPcErrorMessage, &ErrorLine, &MinErrorCode, &MaxErrorCode, END_OF_ARG_LIST	);
//	GerrLogMini (GerrId, "PID %d: Main DB (%s)   cardinality test - SQLCODE = %d.", mypid, MAIN_DB->Name, SQLCODE);
//	ExecSQL (	ALT_DB, TestReadPcErrorMessage, &ErrorLine, &MinErrorCode, &MaxErrorCode, END_OF_ARG_LIST	);
//	GerrLogMini (GerrId, "PID %d:  Alt DB (%s) cardinality test - SQLCODE = %d.", mypid, ALT_DB->Name, SQLCODE);
//
//	// 3) Hebrew test.
//	MinErrorCode = 6230;
//	MaxErrorCode = 6230;
//	ExecSQL (	MAIN_DB, TestReadPcErrorMessage, &ErrorLine, &MinErrorCode, &MaxErrorCode, END_OF_ARG_LIST	);
//	GerrLogMini (GerrId, "PID %d: Main DB (%s)   Hebrew test {%s},   SQLCODE = %d.", mypid, MAIN_DB->Name, ErrorLine, SQLCODE);
//	ExecSQL (	ALT_DB, TestReadPcErrorMessage, &ErrorLine, &MinErrorCode, &MaxErrorCode, END_OF_ARG_LIST	);
//	GerrLogMini (GerrId, "PID %d:  Alt DB (%s) Hebrew test {%s}, SQLCODE = %d.", mypid, ALT_DB->Name, ErrorLine, SQLCODE);
//
//
//	char *BadAddress	= NULL;
//	char Constant[]		= "ERR_PRESCRIPTION_ID_NOT_FOUND";
//	char GoodString		[65];
//	char ErrorLine		[101];
//
//	strcpy (GoodString, "TR6001_PH995482_IN_ISHUR");
//	strcpy (ErrorLine, "");
//
//	ExecSQL (	MAIN_DB, TestReadPcErrorMessage, &ErrorLine, NULL, END_OF_ARG_LIST	);
//GerrLogMini (GerrId, "\nBad input address:      ErrorLine = \"%s\", SQLCODE = %d.", ErrorLine, SQLCODE);
//
//	ExecSQL (	MAIN_DB, TestReadPcErrorMessage, &ErrorLine, &Constant, END_OF_ARG_LIST	);
//GerrLogMini (GerrId,   "Constant input address: ErrorLine = \"%s\", SQLCODE = %d.", ErrorLine, SQLCODE);
//
//	ExecSQL (	MAIN_DB, TestReadPcErrorMessage, &ErrorLine, "ERR_PRESCRIPTION_ID_NOT_FOUND", END_OF_ARG_LIST	);
//GerrLogMini (GerrId,   "Naked input string:     ErrorLine = \"%s\", SQLCODE = %d.", ErrorLine, SQLCODE);
//
//	ExecSQL (	MAIN_DB, TestReadPcErrorMessage, &ErrorLine, &GoodString, END_OF_ARG_LIST	);
//GerrLogMini (GerrId,   "Good input string:      ErrorLine = \"%s\", SQLCODE = %d.", ErrorLine, SQLCODE);
//
//	ExecSQL (	MAIN_DB, TestReadPcErrorMessage, NULL, &GoodString, END_OF_ARG_LIST	);
//GerrLogMini (GerrId, "\nNULL destination:       ErrorLine = \"%s\", SQLCODE = %d.", Constant, SQLCODE);
//
//	ExecSQL (	MAIN_DB, TestReadPcErrorMessage, "Hi there!", &GoodString, END_OF_ARG_LIST	);
//GerrLogMini (GerrId,   "Const destination:      ErrorLine = \"%s\", SQLCODE = %d.", Constant, SQLCODE);
//
//GerrLogMini (GerrId, "\nBefore: Constant addr %ld, value %ld.", (long)(&Constant), (long)Constant);
//	ExecSQL (	MAIN_DB, TestReadPcErrorMessage, &Constant, &GoodString, END_OF_ARG_LIST	);
//GerrLogMini (GerrId, "After:  Constant addr %ld, value %ld, SQLCODE = %d.", (long)(&Constant), (long)Constant, SQLCODE);
//GerrLogMini (GerrId, "Const destination aliased: ErrorLine = \"%s\", SQLCODE = %d.", Constant, SQLCODE);


	// DonR 28Mar2016: Load bitmap of illnesses for which member discounts can be granted
	// even if the drug's health-basket parameter is zero.
	// DonR 01Dec2016: Added a second bitmap to indicate which illnesses are actual diseases,
	// as opposed to work accidents and traffic accidents.
	// DonR 20Sep2018: Added two new bitmaps for handling diseases with and without
	// specific diagnosis codes.
	BypassHealthBasketBitmap		=
	ActualDiseaseBitmap				=
	DiseasesWithoutDiagnosesBitmap	=
	DiseasesWithDiagnosesBitmap		= 0;	// Already initialized, but let's be paranoid.

	DeclareAndOpenCursorInto (	MAIN_DB, bypass_basket_cur,
								&illness_bit_number, &override_basket, &is_actual_disease, &use_diagnoses, END_OF_ARG_LIST	);

	while (1)
	{
		FetchCursor (MAIN_DB, bypass_basket_cur);

		if (SQLERR_code_cmp (SQLERR_end_of_fetch) == MAC_TRUE)
		{
			break;
		}
		else
		{
			// Log errors to file but otherwise ignore them.
			if (SQLERR_error_test ())
			{
				continue;
			}
			else
			{
				if (override_basket		> 0)
					BypassHealthBasketBitmap	|= (1 << (illness_bit_number - 1));

				if (is_actual_disease	> 0)
					ActualDiseaseBitmap			|= (1 << (illness_bit_number - 1));

				// DonR 20Sep2018 CR #13262: Load "positive" bitmap for determining
				// which diseases involve specific diagnoses. After this loop, we'll
				// also create an inverse "negative" bitmap for those diseases that
				// do *not* involve specific diagnoses.
				if (use_diagnoses > 0)
					DiseasesWithDiagnosesBitmap	|= (1 << (illness_bit_number - 1));

			}	// Found a qualifying row.
		}	// Something other than end-of-fetch.
	}	// Loop through relevant rows in illness_codes table.

	CloseCursor (MAIN_DB, bypass_basket_cur);

	// DonR 20Sep2018 CR #13262: As promised above, create the "negative" bitmap for
	// all the diseases that do *not* involve specific diagnosis codes.
	DiseasesWithoutDiagnosesBitmap = ~DiseasesWithDiagnosesBitmap;


	// DonR 17Jan2006: Get configuration parameters from sysparams table.
	// DonR 28Jun2016: Added new configuration flag to enable/disable lookup of
	// Maccabi Price from Price List 800. For now, this flag always gets a
	// value of zero; if anyone decides that we want to revive the feature,
	// just add an actual column to sysparams and read it in instead of the
	// hard-coded zero.
//	// DonR 29Jun2016: Temporarily changed the hard-coded value for EnablePriceList800
//	// to 1, so that I can put a hot-fix version into Production with the Price List
//	// 800 lookup still operational.
	// DonR 13May2020 CR #31591: Added Largo Types eligible for illness-based,
	// ventilator-based, and non-illness-based member discounts.
	// DonR 29Jun2023 User Story #461368: Add 3 new columns to support time-limited alternate price list lookup.
	// Marianna 09Jul2023 User Story #463302: Add alt_price_list_only_if_cheaper.
	// DonR 03Aug2023 User Story #469361: Add AdjustCalculateDurationPercent.
	// DonR 21Aug2023: Got rid of max_future_presc, which was redundant - all we need is sale_ok_early_days.
	// Instead of "bubbling" up all the other parameters, moved AdjustCalculateDurationPercent from the end
	// to the tenth position where max_future_presc used to be.
	// DonR 28Jan2024 User Story #540234: Added five new parameters for Cannabis sale options.
	// DonR 23Jun2024 User Story #318200: Added two new "expensive drug" columns. The old expensive_drug_prc
	// column (and variable) are now obsolete.
	ExecSQL (	MAIN_DB,	READ_sysparams,
				&min_refill_days,					&NihulTikrotLargoLen,			&TikrotEnabled,
				&vat_percent,						&OverseasMaxSaleAmt,			&EnablePPlusMeishar,
				&MaxDailyBuy_Pvt,					&MaxDailyBuy_PPlus,				&MaxDailyBuy_Mac,
				&AdjustCalculateDurationPercent,	&CycleOverlap,					&SaleOK_EarlyDays,
				&SaleOK_LateDays,					&nocard_ph_maxvalid,			&nocard_ph_pvt_kill,
		
				&nocard_ph_mac_kill,				&round_pkgs_2_pharm,			&round_pkgs_sold,
				&del_valid_months,					&UseSaleTables,					&test_sale_equality,
				&check_card_date,					&ddi_pilot_enabled,				&no_show_reject_num,
				&no_show_hist_days,					&no_show_warn_code,				&no_show_err_code,
				&svc_w_o_card_days,					&usage_memory_mons,				&max_order_sit_days,

				&max_order_work_hrs,				&IngrSubstMaxRatio,				&max_unsold_op_pct,
				&max_units_op_off,					&bakarakamutit_mons,			&MaxOtcCreditPerMonth,
				&SickDscLargoTypes,					&VentDscLargoTypes,				&MembDscLargoTypes,
				&MinNormalMemberTZ,					&pkg_price_for_sms,				&FutureSalesMaxHist,
				&FutureSalesMinHist,				&MacPharmMaxOTC_OP_PerDay,		&MacPharmMaxGSL_OP_PerDay,
		
				&EnableSecretInvestigatorLogic,		&DarkonaiMaxHishtatfutPct,		&SaleCompletionMaxDelay,
				&alt_price_list_first_date,			&alt_price_list_last_date,		&alt_price_list_code,
				&alt_price_list_only_if_cheaper,	&svc_w_o_card_days_pvt,			&CannabisSaleEarlyDays,
				&CannabisAllowFutureSales,			&CannabisAllowDiscounts,		&CannabisCallNihulTikrot,
				&CannabisPermitPerPharmacy,			&ExpensiveDrugMacDocRx,			&ExpensiveDrugNotMacDocRx,

				&MaxShortDurationDays,				&MaxShortDurationOverlapDays,	&MaxLongDurationOverlapDays,
				&MaxDoctorRuleConfirmAuthority,		&MaxPharmIshurConfirmAuthority,	&DrugShortageRuleConfAuthority,
				&default_narco_max_duration,		&default_narco_max_validity,	&exempt_narco_max_duration,
				&exempt_narco_max_validity,

				END_OF_ARG_LIST																							);

	if (SQLCODE == 0)
	{
		TikrotRPC_Enabled				= TikrotEnabled;
		PPlusMeisharEnabled				= EnablePPlusMeishar;
		IshurCycleOverlap				= CycleOverlap;
		SendRoundedPackagesToPharm		= round_pkgs_2_pharm;
		RoundPackagesToCloseRx			= round_pkgs_sold;
		MaxUnsoldOpPercent				= max_unsold_op_pct;
		MaxRemainderPerOp				= max_units_op_off;
		UseSaleTableDiscounts			= UseSaleTables;
		TestSaleEquality				= test_sale_equality;
		CheckCardDate					= check_card_date;
		DDI_Pilot_Enabled				= ddi_pilot_enabled;
		SvcWithoutCardValidDays			= svc_w_o_card_days;
		SvcWithoutCardValidDaysPvt		= svc_w_o_card_days_pvt;// DonR 19Oct2023 User Story #487690.
		OnlineOrderRememberUsageMonths	= usage_memory_mons;	// DonR 21Jun2018: Online Orders (SuperPharm).
		OnlineOrderWaitDays				= max_order_sit_days;	// DonR 21Jun2018: Online Orders (SuperPharm).
		OnlineOrderMaxWorkHours			= max_order_work_hrs;	// DonR 21Jun2018: Online Orders (SuperPharm).
		BakaraKamutitHistoryMonths		= bakarakamutit_mons;	// DonR 07Feb2019 CR #27974.
		MaxFamilyOtcCreditPerMonth		= MaxOtcCreditPerMonth;	// DonR 10Apr2019 CR #???.
		MinimumNormalMemberTZ			= MinNormalMemberTZ;	// DonR 29Jun2020 CR #28453.

		// DonR 23Aug2011: Calculate factor to remove VAT from prices for pharmacies in Eilat.
		no_vat_multiplier = (double)1.0 / ((double)1.0 + ((double)vat_percent / (double)100.0));

		// DonR 11Jul2012: Load Daily Purchase Limits into array (subscripted by Permission Type).
		MaxDailyBuy [0] = MaxDailyBuy_Pvt;
		MaxDailyBuy [1] = MaxDailyBuy_Mac;
		MaxDailyBuy [6] = MaxDailyBuy_PPlus;

		// DonR 01Jun2015: Translate service-without-card limits from minutes to seconds. Note
		// that we want to do this *only* for positive values! After translating, move the values
		// into global variables.
		if (nocard_ph_maxvalid > 0) nocard_ph_maxvalid *= 60;
		if (nocard_ph_pvt_kill > 0) nocard_ph_pvt_kill *= 60;
		if (nocard_ph_mac_kill > 0) nocard_ph_mac_kill *= 60;

		NoCardSvc_Max_Validity			= nocard_ph_maxvalid;
		NoCardSvc_After_Pvt_Purchase	= nocard_ph_pvt_kill;
		NoCardSvc_After_Mac_Purchase	= nocard_ph_mac_kill;

		// DonR 24Dec2015: Added new sysparams parameter del_valid_months to indicate how long after
		// a member leaves Maccabi s/he can still delete prior drug sales. Convert this parameter
		// into days by multiplying by 30.5.
		DeletionsValidDays = (del_valid_months * 30) + (del_valid_months / 2);

		// DonR 13May2020 CR #31591: Set Largo Types eligible for illness-based
		// and non-illness-based discounts.
		strcpy (LargoTypesForIllnessDiscounts,		SickDscLargoTypes);
		strcpy (LargoTypesForVentilatorDiscounts,	VentDscLargoTypes);
		strcpy (LargoTypesForNonIllnessDiscounts,	MembDscLargoTypes);
	}
	else
	{
		TestSaleEquality			= 1;	// By default, validate participation/tikrot amts in 5005/6005.
		CheckCardDate				= 1;	// By default, member card dates *are* checked in Trn. 6001/6003.
		DDI_Pilot_Enabled			= 0;	// By default, the DDI pilot is *disabled* and all doctors will have interactions checked.
		SvcWithoutCardValidDays		= 9999;	// By default, don't time out service-without-card requests from Haverut.
		SvcWithoutCardValidDaysPvt	= 9999;	// By default, don't time out service-without-card requests from Haverut.
		MinimumNormalMemberTZ		= 100;	// DonR 29Jun2020 CR #28453: Default is to give warnings for TZ's of 1 or 2 digits.

		UseSaleTableDiscounts	= 0;	// Default to disabled.
		TikrotRPC_Enabled		= 1;	// Default to enabled.

		// Service without card: default to 3 hours max, 15 minutes after private-pharmacy purchase,
		// and purchase at Maccabi pharmacy doesn't affect the time limit.
		NoCardSvc_Max_Validity			= 60 * 180;
		NoCardSvc_After_Pvt_Purchase	= 60 *  15;
		NoCardSvc_After_Mac_Purchase	= -1;
		IngrSubstMaxRatio				= 2.0;

		MaxFamilyOtcCreditPerMonth		= 100000;

		// DonR 13May2020 CR #31591: Set default Largo Types eligible for
		// illness-based and non-illness-based discounts.
		strcpy (LargoTypesForIllnessDiscounts,		"TM");
		strcpy (LargoTypesForVentilatorDiscounts,	"BYD");
		strcpy (LargoTypesForNonIllnessDiscounts,	"TM");

		// DonR 28Nov2021 User Story #205423: Set default to 3 OP/day for MaccabiPharm OTC purchases.
		MacPharmMaxOTC_OP_PerDay		= 3;
	}

	GerrLogMini (GerrId,
				 "PID %d System Time = %d. Refill Days %d, Expensive %d/%d,\n"
				 "          Tikrot Enabled %d, Prati+ Meishar Enabled %d,\n"
				 "          VAT = %f%%, no-VAT = %f,\n"
				 "          Max Daily Purchase Pvt/PPlus/Mac %d/%d/%d,\n"
				 "          Bypass Basket Bitmap = %d, UseSaleTableDiscounts = %d,\n"
				 "          Pkg price for SMS %d, SaleOK_EarlyDays %d, SQLCODE %d.",
				 mypid, GetTime(), min_refill_days, ExpensiveDrugMacDocRx, ExpensiveDrugNotMacDocRx, TikrotEnabled, EnablePPlusMeishar, vat_percent, no_vat_multiplier,
				 MaxDailyBuy_Pvt, MaxDailyBuy_PPlus, MaxDailyBuy_Mac, BypassHealthBasketBitmap, UseSaleTableDiscounts, pkg_price_for_sms, SaleOK_EarlyDays, SQLCODE);


	// DonR 28Jan2019: Load array with Transaction Permission information (to avoid looking it up
	// every time a transaction request comes in).
	LoadTrnPermissions ();

	// DonR 10Jul2012: Load array with Prescription Source information (to avoid having to look it up
	// while processing drug sales).
	LoadPrescrSource ();

	// DonR 24Dec2012: Restore default (committed read) isolation.
	RESTORE_ISOLATION;

	// Set up ODBC connection to AS/400 for "Nihul Tikrot" RPC call.
	InitTikrotSP ();


	/*=======================================================================
	||																		||
	||				MASTER LOOP												||
	||																		||
	=======================================================================*/
	while (1)
	{
		/*=======================================================================
		||			UPDATE SHARED MEMORY IF NEEDED								||
		=======================================================================*/

		RETURN_ON_ERR (UpdateShmParams ());

		/*=======================================================================
		||			GO DOWN WITH SYSTEM											||
		=======================================================================*/

		if (ToGoDown (PHARM_SYS))
		{
			GerrLogReturn (GerrId,
						   "Pharmacy System is going down, and thus so am I.");
			break;
		}


		// DonR 26Aug2003: Trap signals such as Software Termination (Signal 15) gracefully.
		// DonR 05Jul2010: Signals 10 and 12 are used to switch "Nihul Tikrot" RPC calls
		// on and off - they shouldn't trigger program termination.
		if ((caught_signal == SIGUSR1) || (caught_signal == SIGUSR2))
		{
			caught_signal = 0;
			continue;
		}
		else
		{
			if (caught_signal > 0)
			{
				break;
			}
		}

		/*=======================================================================
		||			GET SOCKET MESSAGES											||
		=======================================================================*/

		memset (buffer, 0, sizeof(buffer));

		// DonR 28Apr2022: SelectWait is currently set to 200,000 microseconds (equal
		// to 1/5 second). This means that an instance of SqlServer will perform all
		// the "extra" stuff (checking for spools to process, etc.) five times each
		// second, which is a *lot* more than necessary. Instead, I'm going to try
		// hard-coding a value of 5,000,000 microseconds (= 5 seconds); this should
		// mean that instances of the program that handle requests will perform at
		// least as well as before, but idle instances should have a lot less useless
		// work to do and will eat up less CPU time.
		// DonR 26May2024: If GetSocketMessage() returns -306 ("Connection closed by
		// client"), print a diagnostic but do *not* terminate the program. There are
		// probably other error codes we should treat the same way, but in real life
		// even -306 happens very, very rarely.
//		accept_sock = GetSocketMessage (SelectWait,
		accept_sock = GetSocketMessage (5000000,
										buffer,
										sizeof(buffer),
										&len,
										LEAVE_OPEN_SOCK);

		ret = accept_sock;
	    if (ret < MAC_OK)
		{
			if (caught_signal == 0)
			{
				GerrLogMini (	GerrId,
								"PID %d got %d from GetSocketMessage - %sterminating!",
								mypid, accept_sock, ((accept_sock == CONN_END) ? "NOT " : "")	);
			}

			if ((caught_signal == SIGUSR1) || (caught_signal == SIGUSR2))
			{
				caught_signal = 0;
				continue;
			}
			else
			{
				if (accept_sock == CONN_END)
					continue;
				else
					break;
			}
		}
 
		if (len > 0)	// message arrived
		{
			if (MakeAndSendReturnMessage (buffer, len) !=  RC_SUCCESS)
			{
				//  error has occurred
			}

			/*=======================================================================
			||		UPDATE LAST ACCESS TIME FOR PROCESS								||
			=======================================================================*/
			state = UpdateLastAccessTime ();

			if (ERR_STATE (state))
			{
				GerrLogReturn (GerrId,
							   "Error while updating last access time of process");
			}

		}	// Got a message.


		// DonR 13Jul2004: Send any spooled Gadget Sale/Cancellation Requests to AS400.
		// Note that process_spools() has its own interval check, so calling it frequently
		// doesn't have any huge effect on CPU usage.
//		sprintf (LastSignOfLife, "About to call process_spools()...");
		process_spools ();


		// DonR 07Jul2010: Every five minutes, check to see if Nihul Tikrot Enabled status
		// has been changed in the sysparams table; if so, reset the program's internal
		// switch.
		// DonR 21Oct2015: Since in practice we aren't making sysparams changes, change
		// timing from every five minutes to every hour.
		// DonR 18Jan2023: Set GlobalSysDate so that cache-initialization routines can use
		// it to check whether their data is fresh.
		if (diff_from_then (LastDate,	LastTime,
							GetDate (),	GetTime ())	>= 3600)
		{
			GlobalSysDate = LastDate = GetDate ();
			LastTime	= GetTime ();

			// DonR 02Aug2007: Re-initialize package-size array used by Trn. 2088.
			// DonR 28Apr2022: Moved this inside the "if diff_from_then" block, so
			// we don't try to re-initialize the package size array constantly.
//			sprintf (LastSignOfLife, "About to call init_package_size_array()...");
			init_package_size_array ();

			// DonR 17Jan2023: Re-initialize error-severity cache.
			// Note that passing -1 means that the cache will be loaded only
			// if necessary; a value of -2 (or less) would reload the cache
			// unconditionally. Note also that for reload commands, the
			// function does *not* return a meaningful value.
			GET_ERROR_SEVERITY (-1);

			if (NeedToUpdateSysparams)
			{
				TikrotEnabled = TikrotRPC_Enabled;

				// Update switch in sysparams so (A) all copies of sqlserver.exe on all Unix/Linux servers
				// know to disable themselves, and (B) restarting the server will not lose the current state.
				ExecSql (	MAIN_DB, UPD_sysparams_TikrotEnabled, &TikrotEnabled, &TikrotEnabled, END_OF_ARG_LIST	);

				CommitAllWork ();	// Make sure we're not leaving open transactions around!

				NeedToUpdateSysparams = 0;
			}
			else
			{
				// DonR 28Dec2017: Added refresh of all relevant Sysparams parameters, rather than just those
				// dealing with Prati-Plus Meishar and Nihul Tikrot. The extra overhead is negligible, and it
				// saves having to restart servers every time there's a non-urgent parameter change.
				// DonR 13May2020 CR #31591: Added Largo Types eligible for illness-based,
				// ventilator-based, and non-illness-based member discounts.
				// DonR 29Jun2023 User Story #461368: Add 3 new columns to support time-limited alternate price list lookup.
				// Marianna 09Jul2023 User Story #463302: Add alt_price_list_only_if_cheaper.
				// DonR 03Aug2023 User Story #469361: Add AdjustCalculateDurationPercent.
				// DonR 21Aug2023: Got rid of max_future_presc, which was redundant - all we need is sale_ok_early_days.
				// Instead of "bubbling" up all the other parameters, moved AdjustCalculateDurationPercent from the end
				// to the tenth position where max_future_presc used to be.
				// DonR 28Jan2024 User Story #540234: Added five new parameters for Cannabis sale options.
				// DonR 23Jun2024 User Story #318200: Added two new "expensive drug" columns. The old expensive_drug_prc
				// column (and variable) are now obsolete.
				ExecSQL (	MAIN_DB,	READ_sysparams,
							&min_refill_days,					&NihulTikrotLargoLen,			&TikrotEnabled,
							&vat_percent,						&OverseasMaxSaleAmt,			&EnablePPlusMeishar,
							&MaxDailyBuy_Pvt,					&MaxDailyBuy_PPlus,				&MaxDailyBuy_Mac,
							&AdjustCalculateDurationPercent,	&CycleOverlap,					&SaleOK_EarlyDays,
							&SaleOK_LateDays,					&nocard_ph_maxvalid,			&nocard_ph_pvt_kill,
		
							&nocard_ph_mac_kill,				&round_pkgs_2_pharm,			&round_pkgs_sold,
							&del_valid_months,					&UseSaleTables,					&test_sale_equality,
							&check_card_date,					&ddi_pilot_enabled,				&no_show_reject_num,
							&no_show_hist_days,					&no_show_warn_code,				&no_show_err_code,
							&svc_w_o_card_days,					&usage_memory_mons,				&max_order_sit_days,

							&max_order_work_hrs,				&IngrSubstMaxRatio,				&max_unsold_op_pct,
							&max_units_op_off,					&bakarakamutit_mons,			&MaxOtcCreditPerMonth,
							&SickDscLargoTypes,					&VentDscLargoTypes,				&MembDscLargoTypes,
							&MinNormalMemberTZ,					&pkg_price_for_sms,				&FutureSalesMaxHist,
							&FutureSalesMinHist,				&MacPharmMaxOTC_OP_PerDay,		&MacPharmMaxGSL_OP_PerDay,
		
							&EnableSecretInvestigatorLogic,		&DarkonaiMaxHishtatfutPct,		&SaleCompletionMaxDelay,
							&alt_price_list_first_date,			&alt_price_list_last_date,		&alt_price_list_code,
							&alt_price_list_only_if_cheaper,	&svc_w_o_card_days_pvt,			&CannabisSaleEarlyDays,
							&CannabisAllowFutureSales,			&CannabisAllowDiscounts,		&CannabisCallNihulTikrot,
							&CannabisPermitPerPharmacy,			&ExpensiveDrugMacDocRx,			&ExpensiveDrugNotMacDocRx,

							&MaxShortDurationDays,				&MaxShortDurationOverlapDays,	&MaxLongDurationOverlapDays,
							&MaxDoctorRuleConfirmAuthority,		&MaxPharmIshurConfirmAuthority,	&DrugShortageRuleConfAuthority,
							&default_narco_max_duration,		&default_narco_max_validity,	&exempt_narco_max_duration,
							&exempt_narco_max_validity,

							END_OF_ARG_LIST																							);

				if (SQLCODE == 0)
				{
					if (EnablePPlusMeishar != PPlusMeisharEnabled)
					{
						PPlusMeisharEnabled	= EnablePPlusMeishar;
						GerrLogMini (GerrId, "Changed Prati Plus Meishar Flag to %d.", PPlusMeisharEnabled);
					}

					if (TikrotEnabled != TikrotRPC_Enabled)
					{
						// Depending on which way the switch has changed, enable or disable Nihul Tikrot calls.
						if (TikrotEnabled == 0)
							NihulTikrotDisableHandler (0);	// Called without a signal.
						else
							NihulTikrotEnableHandler (0);	// Called without a signal.

						TikrotRPC_Enabled		= TikrotEnabled;
					}

					IshurCycleOverlap				= CycleOverlap;
					SendRoundedPackagesToPharm		= round_pkgs_2_pharm;
					RoundPackagesToCloseRx			= round_pkgs_sold;
					MaxUnsoldOpPercent				= max_unsold_op_pct;
					MaxRemainderPerOp				= max_units_op_off;
					UseSaleTableDiscounts			= UseSaleTables;
					TestSaleEquality				= test_sale_equality;
					CheckCardDate					= check_card_date;
					DDI_Pilot_Enabled				= ddi_pilot_enabled;
					SvcWithoutCardValidDays			= svc_w_o_card_days;
					SvcWithoutCardValidDaysPvt		= svc_w_o_card_days_pvt;// DonR 19Oct2023 User Story #487690.
					OnlineOrderRememberUsageMonths	= usage_memory_mons;	// DonR 21Jun2018: Online Orders (SuperPharm).
					OnlineOrderWaitDays				= max_order_sit_days;	// DonR 21Jun2018: Online Orders (SuperPharm).
					OnlineOrderMaxWorkHours			= max_order_work_hrs;	// DonR 21Jun2018: Online Orders (SuperPharm).
					BakaraKamutitHistoryMonths		= bakarakamutit_mons;	// DonR 07Feb2019 CR #27974.
					MinimumNormalMemberTZ			= MinNormalMemberTZ;	// DonR 29Jun2020 CR #28453.

					// DonR 23Aug2011: Calculate factor to remove VAT from prices for pharmacies in Eilat.
					no_vat_multiplier = (double)1.0 / ((double)1.0 + ((double)vat_percent / (double)100.0));

					// DonR 11Jul2012: Load Daily Purchase Limits into array (subscripted by Permission Type).
					MaxDailyBuy [0] = MaxDailyBuy_Pvt;
					MaxDailyBuy [1] = MaxDailyBuy_Mac;
					MaxDailyBuy [6] = MaxDailyBuy_PPlus;

					// DonR 01Jun2015: Translate service-without-card limits from minutes to seconds. Note
					// that we want to do this *only* for positive values! After translating, move the values
					// into global variables.
					if (nocard_ph_maxvalid > 0) nocard_ph_maxvalid *= 60;
					if (nocard_ph_pvt_kill > 0) nocard_ph_pvt_kill *= 60;
					if (nocard_ph_mac_kill > 0) nocard_ph_mac_kill *= 60;

					NoCardSvc_Max_Validity			= nocard_ph_maxvalid;
					NoCardSvc_After_Pvt_Purchase	= nocard_ph_pvt_kill;
					NoCardSvc_After_Mac_Purchase	= nocard_ph_mac_kill;

					// DonR 24Dec2015: Added new sysparams parameter del_valid_months to indicate how long after
					// a member leaves Maccabi s/he can still delete prior drug sales. Convert this parameter
					// into days by multiplying by 30.5.
					DeletionsValidDays = (del_valid_months * 30) + (del_valid_months / 2);

					// DonR 13May2020 CR #31591: Set Largo Types eligible for illness-based
					// and non-illness-based discounts.
					strcpy (LargoTypesForIllnessDiscounts,		SickDscLargoTypes);
					strcpy (LargoTypesForVentilatorDiscounts,	VentDscLargoTypes);
					strcpy (LargoTypesForNonIllnessDiscounts,	MembDscLargoTypes);

				}	// Good read of sysparams.


				// DonR 21Oct2015: TEMPORARY FEATURE: Check the member_drug_29g table to see if it has any
				// contents. If it does, we will enable Form 29-G checking and send the relevant warning
				// messages to pharmacies. If the table is empty, assume that the feature is not yet in
				// real-world use and disable it.
				ExecSQL (	MAIN_DB, READ_29G_TableSize, &RowsInTable, END_OF_ARG_LIST	);

				// DonR 05Jul2016: Log a message about Form29gMessagesEnabled only when the program starts up OR
				// the value is different from the last time we checked.
				if (Form29gMessagesEnabled != ((SQLCODE == 0) && (RowsInTable > 0)))
				{
					Form29gMessagesEnabled = ((SQLCODE == 0) && (RowsInTable > 0));
					GerrLogMini (GerrId, "Form29gMessagesEnabled = %d.", Form29gMessagesEnabled);
				}

				// DonR 28Jan2019: Load array with Transaction Permission information (to avoid looking it up
				// every time a transaction request comes in). Refresh the list hourly.
				LoadTrnPermissions ();

				// DonR 10Jul2012: Load array with Prescription Source information (to avoid having to look it up
				// while processing drug sales).
				// DonR 21Mar2018: Copied this line here, so the Prescription Source array will also
				// refresh hourly.
				LoadPrescrSource ();

			}	// Not updating sysparams nihul-tikrot flag with changed value from signal trap.
		}	// At least one hour has passed since the last time we checked this stuff.


		/*=======================================================================
		||			MARK MYSELF AS FREE											||
		=======================================================================*/

		// Normally, an instance of sqlserver will be marked "busy" only if there
		// is actually an incoming request for it to process - so calling
		// SetFreeSqlServer() when GetSocketMessage() timed out with no request
		// is unnecessary. However, the CPU burden of SetFreeSqlServer() is
		// minimal (and there is no network or DB usage), and setting the server
		// free could be necessary if someone has marked it as busy and then sent
		// a zero-length "request". TLDR Version: DO NOT MAKE THIS CONDITIONAL.
		state = SetFreeSqlServer (mypid);

		if (ERR_STATE (state))
		{
			GerrLogReturn (GerrId,
						   "Can't set myself as free sqlserver");
		}

	}	/* WHILE*/


	// Terminate process in "normal" (i.e. not in eternal loop) mode.
	TerminateProcess (caught_signal, NORMAL_EXIT);
}



/*=======================================================================
||																		||
||			     MakeAndSendReturnMessage()								||
||																		||
 =======================================================================*/
int	 MakeAndSendReturnMessage (	char	*input_buf,			/* input message buffer	*/
								int		input_size			/* input buffer size	*/	)
{
    
/*=======================================================================
||			    Local variables.										||
 =======================================================================*/
	char		output_buf [65536];
	int			i;
	int			output_size;
	int			output_type_flg;
	int			retval;
	int			reStart;
	int			tries;
	int			MsgIndex;
	int			MsgErrorCode;
	SSMD_DATA	ssmd_data;
	struct tm	*cur_tm;
	int			rec_id;
	TErrorCode	rec_id_err;

	// DonR 15Feb2011: Timing variables for Nihul Tikrot diagnostics.
	int			purchase_hist		= 0;
	int			pre_tikrot			= 0;
	int			family_sales		= 0;
	int			rpc_execute			= 0;
	int			trn_total			= 0;
	int			gadget				= 0;

	int			sq_pharmacy_num;
	short		sq_terminal_num;
	short		sq_msg_idx;
	int			sq_member_id;
	short		sq_member_id_ext;
	short		sq_spool_flg;
	short		sq_ok_flg;
	int			sq_time;
	int			sq_date;
	int			sq_proc_time;
	int			sq_num_processed;
	short		sq_error_code;
	int			sq_pid = mypid;
	int			sq_msg_len = input_size - MSG_HEADER_SIZE;
	int			sq_msg_count;
	char		sq_message_raw [2500];	// DonR 24Nov2014: Increased length from 255 to 2500.

	// DonR 14Apr2021: Add support for incoming JSON requests.
	cJSON		*JSON_request			= NULL;
	cJSON		*JSON_MaccabiHeader		= NULL;
	cJSON		*JSON_MaccabiRequest	= NULL;
	cJSON		*JSON_TrnId				= NULL;
	char		*JSON_ErrPtr;
	int			JSON_Mode				= 0;


	// Get exact time for message statistics
    memset ((void *)&ssmd_data, 0, sizeof(ssmd_data));
    gettimeofday (&ssmd_data.start_proc, 0);
    ssmd_data.proc_time = ssmd_data.start_proc;	// Initially, no time has passed.
    cur_tm = localtime (&ssmd_data.start_proc.tv_sec);
    sprintf (ssmd_data.start_time, "%02d:%02d:%02d", cur_tm->tm_hour, cur_tm->tm_min, cur_tm->tm_sec);


	// Initializations
    memset (output_buf, 0, sizeof (output_buf));	// enable EOF detection

	// DonR 04Dec2022 User Story #408077: Add a global TMemberData structure to replace local ones. We'll
	// perform this initialization unconditionally, even though non-member-related transactions don't need
	// it - the cost is minimal, especially since the non-member-related transactions are all much lower
	// volume than the member-related ones for selling drugs.
	memset ((char *)&Member, 0, sizeof (TMemberData));
	Member.InsuranceType	= 'N';			// Start with a non-NULL value to avoid DB errors.
	Member.MemberMaccabi	= 1;			// DonR 26Mar2024: Also default to Member Type "Maccabi", which is
											// true for everyone in our database now. (The Tzahal feature hasn't
											// been in use for many years already.)

	GlobalSysDate			= GetDate ();	// Avoid extra GetDate() calls by storing the value in a global variable.

	// DonR 14Aug2024 User Story #349368: Initialize new global variables
	// VersionNumber and VersionLargoLen.
	VersionNumber			= 1;	// Just so we'll have some reasonable default value.
	VersionLargoLen			= 5;	// The standard Largo Code length before User Story #349368.


	// DonR 14Apr2021: Intercept JSON messages.
	if (*input_buf == '{')
	{
		// Set defaults.
		MsgIndex		= 0;
		MsgErrorCode	= 1;
		JSON_Mode		= 1;

		JSON_request = cJSON_Parse (input_buf);
		if (JSON_request == NULL)
		{
			JSON_ErrPtr = (char *)cJSON_GetErrorPtr ();
			GerrLogMini (GerrId, "cJSON_Parse() error before %s.", (JSON_ErrPtr == NULL) ? "NULL" : JSON_ErrPtr);
		}
		else
		{
			JSON_MaccabiRequest = cJSON_GetObjectItemCaseSensitive (JSON_request, "PharmServerRequest");
			if (JSON_MaccabiRequest == NULL)
			{
				GerrLogMini (GerrId, "Couldn't find 'PharmServerRequest' in JSON request.");
			}

			JSON_MaccabiHeader = cJSON_GetObjectItemCaseSensitive (JSON_request, "HTTP_Headers");
			if (JSON_MaccabiHeader == NULL)
			{
				GerrLogMini (GerrId, "Couldn't find 'HTTP_Headers' in JSON request.");
			}

			// DonR 14Jun2021: Look for the Transaction ID in the header object first, since
			// that's where PharmWebServer.exe puts it for POSTed transactions, and those
			// transactions typically have fairly large "Maccabi Requests" to parse through.
			// DonR 16Jul2025 User Story #417785: In fact, the Transaction ID is *not* being
			// put in the header object - so we're just wasting time by looking there first.
			// So I'm switching to look for it in JSON_MaccabiRequest first, and only if that
			// fails will we look in JSON_MaccabiHeader.
//			JSON_TrnId = cJSON_GetObjectItemCaseSensitive (JSON_MaccabiHeader, "request_number");
			JSON_TrnId = cJSON_GetObjectItemCaseSensitive (JSON_MaccabiRequest, "request_number");

			if (cJSON_IsNumber (JSON_TrnId))
			{
				MsgIndex = JSON_TrnId->valueint;
// GerrLogMini (GerrId, "Got JSON TransactionId = %d.", MsgIndex);
			}
			else
			{
				// Fallback: If we didn't find a numeric request_number in the
				// "PharmServerRequest" object, look in HTTP_Headers.
//				JSON_TrnId = cJSON_GetObjectItemCaseSensitive (JSON_MaccabiRequest, "request_number");
				JSON_TrnId = cJSON_GetObjectItemCaseSensitive (JSON_MaccabiHeader, "request_number");
				if (cJSON_IsNumber (JSON_TrnId))
				{
					MsgIndex = JSON_TrnId->valueint;
				}
				else
				{
					GerrLogMini (GerrId, "Couldn't find request_number in JSON request - value = %s.", (JSON_TrnId == NULL) ? "NULL" : JSON_TrnId->valuestring);
				}
			}
		}
	}
	else
	{
		// Get Non-JSON message id & error code
		get_message_header (input_buf, &MsgIndex, &MsgErrorCode);
	}


	// DonR 31Dec2020: For a little more timing accuracy, moved the "get request" log message
	// here, right at the beginning of the function.
	if (!TikrotProductionMode)
		GerrLogMini (GerrId, "SqlServer %05d got %s%s %d request...", mypid, (MsgErrorCode == RC_SUCCESS) ? "regular" : "spooled", (*input_buf == '{') ? " JSON" : "", MsgIndex);


	// DonR 28Jan2019: Store the transaction code in ssmd_data *before* calling the
	// transaction handler - this way we can use this value as a "pseudo-global" variable
	// to decide (i.e. in the is_pharmacy_open() routine) which transactions are valid
	// for which pharmacies.
    ssmd_data.msg_idx	= MsgIndex;
	ssmd_data.spool_flg	= (MsgErrorCode == RC_SUCCESS) ? 0 : 1;


#ifdef MESSAGE_DEBUG
    printf( "message_index == (%d)\n", MsgIndex );
    fflush(stdout);
#endif


    sq_msg_idx		= (short)MsgIndex;

    sq_time			= cur_tm->tm_hour * 10000 +
					  cur_tm->tm_min  * 100 +
					  cur_tm->tm_sec;

    sq_date			= (cur_tm->tm_year + 1900) * 10000 +
					  (cur_tm->tm_mon  + 1) * 100 +
					  cur_tm->tm_mday;

	// DonR 24Nov2014: Increased stored request-message length from 255 to 2500.
    len = (input_size > 2499) ? 2499 : input_size;

    memcpy (sq_message_raw, input_buf, len);
    sq_message_raw [len] = 0;

	sq_msg_count = ++message_count;	// DonR 28Jul2003

    reStart = MAC_TRUE;

	// Retry loop for pre-transaction-processing log-table inserts.
    for (tries = 0; tries < SQL_UPDATE_RETRIES && reStart == MAC_TRUE; tries++)
    {
		reStart = MAC_FALS;

		// Get unique ID for Message log
		do				// transaction start
		{
			rec_id_err = GET_MSG_REC_ID (&rec_id);

			if (rec_id_err != NO_ERROR)
			{
				break;
			}

			// Insert message details into log
			// DonR 07Jul2016: Consolidated messages_incoming with messages_details. Since the
			// table's fields are defined NOT NULL but do not have defaults, put in dummy values
			// for the fields that don't have real values yet.
			// Note that I have also changed the values of the Spool Flag from 1/2 to 0/1 - given
			// the name of the variable, it's easier to remember that 0 is *not* spooled
			// and 1 *is* spooled.
			// DonR 30Jan2019 CR #27234: Set ssmd_data.spool_flg *before* calling transaction-
			// processing functions, so subroutines will know that they're dealing with a spooled transaction.
		    sq_spool_flg = (MsgErrorCode == RC_SUCCESS) ? 0 : 1;

			ExecSQL (	MAIN_DB, INS_messages_details, &sq_msg_idx, &sq_time, &sq_date, &sq_message_raw, &rec_id, &sq_spool_flg, &MyShortName, END_OF_ARG_LIST	);

		    Conflict_Test (reStart);

			// DonR 16Apr2023: Add a separate diagnostic for duplicate-key errors. These are non-fatal, but
			// they indicate that someone needs to update the rec_id range in the presc_per_host table.
			if (SQLERR_code_cmp (SQLERR_not_unique) == MAC_TRUE)
			{
				GerrLogMini (GerrId, "INS_messages_details: non-fatal duplicate-key err %d for %s rec_id %d.\n"
							 "Check presc_per_host values for msg_low_limit and msg_upr_limit!",
							 SQLCODE, MyShortName, rec_id);
			}
			else
			{
				if (SQLERR_error_test ())
				{
					GerrLogMini (GerrId, "INS_messages_details failed with SQLCODE %d for %d/%d/%d/%d/%d/%s.",
								 SQLCODE, sq_msg_idx, sq_date, sq_time, rec_id, sq_spool_flg, MyShortName);
				}
			}


			// DonR 28Jul2003: Write a row to table sql_srv_audit.

			// DonR 30Jul2003: If we're dealing with a spooled transaction, invert the
			// transaction number for sql_srv_audit. This is instead of adding a
			// separate field for Spooled Status - which might not be a bad thing
			// to do, later.
			if (MsgErrorCode != RC_SUCCESS)
			{
				sq_msg_idx *= -1;
			}

//			sprintf (LastSignOfLife,
//					 "About to write pre-transaction-processing log data for Transaction %d%s.",
//					 MsgIndex, (MsgErrorCode == RC_SUCCESS) ? "" : " (spool)");

			// DonR 22Mar2011: Since we're updating much more often than we're inserting, try the update first.
			ExecSQL (	MAIN_DB, UPD_sql_srv_audit_InProgress,
						&sq_date,		&sq_time,		&MyShortName,	&sq_msg_idx,
						&rec_id,		&sq_msg_len,	&sq_msg_count,	&sq_pid,
						END_OF_ARG_LIST													);

			if (SQLERR_code_cmp (SQLERR_no_rows_affected) == MAC_TRUE)
			{
				ExecSQL (	MAIN_DB, INS_sql_srv_audit,
							&sq_pid,		&MyShortName,		&sq_date,		&sq_time,
							&sq_msg_idx,	&rec_id,			&sq_msg_len,	&sq_msg_count,
							END_OF_ARG_LIST														);
			}	// UPDATE didn't find anything to update.

			Conflict_Test (reStart);
			SQLERR_error_test ();
		}
		while (0);			// transaction stop

		if (reStart == MAC_FALS)	/* Transaction OK then COMMIT	*/
		{
			// DonR 31Dec2020: Commit just the main database - this may be faster than CommitAllWork(),
			// and we've written to only the two tables messages_details and sql_srv_audit at this point.
			// DonR 21Feb2021: Revert to CommitAllWork() - I'm using these tables as a test-bed for the
			// new SQL command mirroring capability, and I want to make sure I commit whatever there is
			// to commit. In any case, CommitAllWork() now reverts to CommitWork() if we haven't done
			// anything with the alternate database, so performance should be the same.
			CommitAllWork ();
		}
		else				/* Transaction ERR then ROLLBACK	*/
		{
			RollbackAllWork ();

			sleep (2);
		}
    }			/* end of retries loop */

    if (reStart != MAC_FALS)	// Error writing to log tables - no minimum threshold for diagnostics.
    {
		GerrLogMini (GerrId, "Table 'messages_details' is locked after <%d> tries", tries);
    }


	/*=======================================================================
	||																		||
	||                Process incoming message & write output message:      ||
	||																		||
	 =======================================================================*/
    retval				= RC_SUCCESS;
    glbErrorCode		= NO_ERROR;
    send_state			= BEFORE_UPDATE;
	sq_num_processed	= 0;

    input_buf	+= MSG_HEADER_SIZE;
    input_size	-= MSG_HEADER_SIZE;

	SetErrorVarInit ();	// DonR 11May2010: Ensures that mode is reset to default of "old-style" error handling.

//	sprintf (LastSignOfLife,
//			 "About to call handler for Transaction %d%s.",
//			 MsgIndex, (MsgErrorCode == RC_SUCCESS) ? "" : " (spool)");

// ADD sq_num_processed TO "MAJOR" TRANSACTIONS.

// DonR 31Dec2020: Moved this log message to the beginning of the function.
//if (!TikrotProductionMode) GerrLogMini (GerrId, "SqlServer %d got %s %d request...", mypid, (MsgErrorCode == RC_SUCCESS) ? "regular" : "spooled", MsgIndex);

//	CurrentTransactionNumber	= MsgIndex;
//	CurrentSpoolFlag			= (MsgErrorCode == RC_SUCCESS) ? 0 : 1;
//

    switch (MsgIndex)
    {
	case 1001:
	    if (MsgErrorCode == RC_SUCCESS)
			retval = HandlerToMsg_1001		(input_buf, input_size, output_buf, &output_size, &output_type_flg, &ssmd_data);
	    else
			retval = HandlerToSpool_1001	(input_buf, input_size, output_buf, &output_size, &output_type_flg, &ssmd_data, MsgErrorCode);

		break;

		// DonR 02Dec2021: Disable transactions 1007 (old-style sale deletion) and 1009 (function unclear).
//	case 1007:
//	    if (MsgErrorCode == RC_SUCCESS)
//			retval = HandlerToMsg_1007		(input_buf, input_size, output_buf, &output_size, &output_type_flg, &ssmd_data);
//	    else
//			retval = HandlerToSpool_1007	(input_buf, input_size, output_buf, &output_size, &output_type_flg, &ssmd_data, MsgErrorCode);
//	    
//		break;
//
//
//	case 1009:
//		    retval = HandlerToMsg_1009		(input_buf, input_size, output_buf, &output_size, &output_type_flg, &ssmd_data);
//		    break;
//
//
	case 1011:
		    retval = DownloadDrugList		(input_buf, input_size, output_buf, &output_size, &output_type_flg, &ssmd_data, MsgIndex);
		    break;


	case 1013:
	    if (MsgErrorCode == RC_SUCCESS)
			retval = HandlerToMsg_1013		(input_buf, input_size, output_buf, &output_size, &output_type_flg, &ssmd_data);
		else
			retval = HandlerToSpool_1013	(input_buf, input_size, output_buf, &output_size, &output_type_flg, &ssmd_data, MsgErrorCode);
		
		break;


	case 1014:
			retval = HandlerToMsg_1014		(input_buf, input_size, output_buf, &output_size, &output_type_flg, &ssmd_data);
		    break;


	// DonR 13Aug2024 User Story #349368: 6022 is a new version of 1022, plus Transaction Version Number.
	case 1022:
	case 6022:
	    if (MsgErrorCode == RC_SUCCESS)
			retval = HandlerToMsg_1022_6022		(MsgIndex, input_buf, input_size, output_buf, &output_size, &output_type_flg, &ssmd_data);
	    else
			retval = HandlerToSpool_1022_6022	(MsgIndex, input_buf, input_size, output_buf, &output_size, &output_type_flg, &ssmd_data, MsgErrorCode);

	    break;


	case 1026:
	    retval = HandlerToMsg_1026			(input_buf, input_size, output_buf, &output_size, &output_type_flg, &ssmd_data);
	    break;


	case 1028:
	    retval = HandlerToMsg_1028			(input_buf, input_size, output_buf, &output_size, &output_type_flg, &ssmd_data);
	    break;


	case 1043:
	    retval = HandlerToMsg_1043			(input_buf, input_size, output_buf, &output_size, &output_type_flg, &ssmd_data);
	    break;


	case 1047:
	    retval = HandlerToMsg_1047			(input_buf, input_size, output_buf, &output_size, &output_type_flg, &ssmd_data);
	    break;


	case 1049:
	    retval = HandlerToMsg_1049			(input_buf, input_size, output_buf, &output_size, &output_type_flg, &ssmd_data);
	    break;


	case 1051:
	    retval = HandlerToMsg_1051			(input_buf, input_size, output_buf, &output_size, &output_type_flg, &ssmd_data);
	    break;


	case 1053:
	    if (MsgErrorCode == RC_SUCCESS)
			retval = HandlerToMsg_1053		(input_buf, input_size, output_buf, &output_size, &output_type_flg, &ssmd_data);
	    else
			retval = HandlerToSpool_1053	(input_buf, input_size, output_buf, &output_size, &output_type_flg, &ssmd_data, MsgErrorCode);

		break;


	case 1055:
	    retval = HandlerToMsg_1055			(input_buf, input_size, output_buf, &output_size, &output_type_flg, &ssmd_data);
	    break;


	// DonR 02Dec2021: Disable transactions 1063 and 1065 (ancient first-generation drug sale request/completion).
//	case 1063:
//	    retval = HandlerToMsg_1003_1063		(input_buf, input_size, output_buf, &output_size, &output_type_flg, &ssmd_data, 1063);
//	    break;
//
//
//	case 1065:
//	    if (MsgErrorCode == RC_SUCCESS)
//			retval = HandlerToMsg_1005_1065	(input_buf, input_size, output_buf, &output_size, &output_type_flg, &ssmd_data, 1065);
//	    else
//			retval = HandlerToSpool_1005_1065	(input_buf, input_size, output_buf, &output_size, &output_type_flg, &ssmd_data, MsgErrorCode, 1065);
//
//	    break;
//
//
	case 1080:	// Request to Update Pharmacy's Contract data
	    retval = HandlerToMsg_1080			(input_buf, input_size, output_buf, &output_size, &output_type_flg, &ssmd_data);
	    break;


	// Electronic Prescriptions transactions.

	case 2001:	// Request for Prescriptions to Fill
	    retval = HandlerToMsg_2001			(input_buf, input_size, output_buf, &output_size, &output_type_flg, &ssmd_data);
	    break;


	case 2003:	// Sale Request
	    retval = HandlerToMsg_2003			(input_buf, input_size, output_buf, &output_size, &output_type_flg, &ssmd_data);
	    break;


	case 2005:	// Delivery
	    if (MsgErrorCode == RC_SUCCESS)
			retval = HandlerToMsg_2005		(input_buf, input_size, output_buf, &output_size, &output_type_flg, &ssmd_data);
	    else
			retval = HandlerToSpool_2005	(input_buf, input_size, output_buf, &output_size, &output_type_flg, &ssmd_data, MsgErrorCode);
	    break;


//	case 2007:	// Old version Cancellation of Sale - DISABLED.
//	    if (MsgErrorCode == RC_SUCCESS)
//			retval = HandlerToMsg_2007		(input_buf, input_size, output_buf, &output_size, &output_type_flg, &ssmd_data);
//	    else
//			retval = HandlerToSpool_2007	(input_buf, input_size, output_buf, &output_size, &output_type_flg, &ssmd_data, MsgErrorCode);
//
//	    break;
//
//
	case 2033:	// Pharmacy Ishur.
	case 6033:	// Pharmacy Ishur - new version with Transaction Version Number.
	    if (MsgErrorCode == RC_SUCCESS)
			retval = HandlerToMsg_2033_6033		(MsgIndex, input_buf, input_size, output_buf, &output_size, &output_type_flg, &ssmd_data);
	    else
			retval = HandlerToSpool_2033_6033	(MsgIndex, input_buf, input_size, output_buf, &output_size, &output_type_flg, &ssmd_data, MsgErrorCode);

		break;


	case 2060:	// how_to_take full-table download.
	    retval = HandlerToMsg_2060			(input_buf, input_size, output_buf, &output_size, &output_type_flg, &ssmd_data);
	    break;


	case 2062:	// drug_shape full-table download.
	    retval = HandlerToMsg_2062			(input_buf, input_size, output_buf, &output_size, &output_type_flg, &ssmd_data);
	    break;


	case 2064:	// unit_of_measure full-table download.
	    retval = HandlerToMsg_2064			(input_buf, input_size, output_buf, &output_size, &output_type_flg, &ssmd_data);
	    break;


	case 2066:	// reason_not_sold full-table download.
	    retval = HandlerToMsg_2066			(input_buf, input_size, output_buf, &output_size, &output_type_flg, &ssmd_data);
	    break;


	case 2068:	// discount_remarks full-table download.
	    retval = HandlerToMsg_2068			(input_buf, input_size, output_buf, &output_size, &output_type_flg, &ssmd_data);
	    break;


	case 2070:	// Request to Update Pharmacy's EconomyPri data (for generic substitution)
	case 2092:	// EconomyPri (new version, supports Group Code up to 99999)
	    retval = HandlerToMsg_2070_2092		(MsgIndex, input_buf, input_size, output_buf, &output_size, &output_type_flg, &ssmd_data);
	    break;


	case 2072:	// Request to Update Pharmacy's Prescription Source data.
	    retval = HandlerToMsg_2072			(input_buf, input_size, output_buf, &output_size, &output_type_flg, &ssmd_data);
	    break;


	case 2074:	// Request to Update Pharmacy's Drug_extension (simplified) data.
	    retval = HandlerToMsg_2074			(input_buf, input_size, output_buf, &output_size, &output_type_flg, &ssmd_data);
	    break;


	case 2076:	// Request to Update Pharmacy's Confirm_authority data.
	    retval = HandlerToMsg_2076			(input_buf, input_size, output_buf, &output_size, &output_type_flg, &ssmd_data);
	    break;


	case 2078:	// Request to Update Pharmacy's Sale data (for MaccabiCare).
	    retval = HandlerToMsg_2078			(input_buf, input_size, output_buf, &output_size, &output_type_flg, &ssmd_data);
	    break;


	case 2080:	// Request to download Gadgets data.
	    retval = HandlerToMsg_2080			(input_buf, input_size, output_buf, &output_size, &output_type_flg, &ssmd_data);
	    break;


	case 2082:	// Request to download PharmDrugNotes data.
	    retval = HandlerToMsg_2082			(input_buf, input_size, output_buf, &output_size, &output_type_flg, &ssmd_data);
	    break;


	case 2084:	// Request to download DrugNotes data.
	    retval = HandlerToMsg_2084			(input_buf, input_size, output_buf, &output_size, &output_type_flg, &ssmd_data);
	    break;


	case 2086:	// Request for pharmacy_daily_sum data by diary month.
	    retval = HandlerToMsg_2086			(input_buf, input_size, output_buf, &output_size, &output_type_flg, &ssmd_data);
	    break;


	case 2088:	// Request for sales data by diary month/sale date.
	    retval = HandlerToMsg_2088			(input_buf, input_size, output_buf, &output_size, &output_type_flg, &ssmd_data);
	    break;


	case 2090:	// Request for single-sale details.
	    retval = HandlerToMsg_X090			(input_buf, input_size, output_buf, &output_size, &output_type_flg, &ssmd_data, 2090);
	    break;


	// DonR 05Nov2017: Added separate transaction for drug-list supplemental fields.
	case 2094:
		retval = DownloadDrugList		(input_buf, input_size, output_buf, &output_size, &output_type_flg, &ssmd_data, MsgIndex);
		break;

	case 2096:	// Marianna 15Oct2020 Feature #97279/CR #32620: Usage_instruct full-table download.
	    retval = HandlerToMsg_2096			(input_buf, input_size, output_buf, &output_size, &output_type_flg, &ssmd_data);
		break;

	case 2101:	// Marianna 15Oct2020 Feature #97279/CR #32620: Usage_instr_reason_changed full-table download.
	    retval = HandlerToMsg_2101			(input_buf, input_size, output_buf, &output_size, &output_type_flg, &ssmd_data);
		break;


	case 5003:	// "Nihul Tikrot" Sale/Credit/Deletion request.
//		gettimeofday (&EventTime[EVENT_TRN_START], 0);
//
//		// By default, everything has taken zero milliseconds until proven otherwise.
//		for (i = EVENT_DELETE_START; i < 20; i++)
//			EventTime[i] = EventTime[EVENT_TRN_START];
//
	    retval = HandlerToMsg_5003			(input_buf, input_size, output_buf, &output_size, &output_type_flg, &ssmd_data);

//		gettimeofday (&EventTime[EVENT_TRN_END], 0);
//
//		// Print timing diagnostic if we've called AS/400 Tikrot application.
//		if (DiffTime (EventTime[EVENT_TRN_START], EventTime[EVENT_CALL_RPC_RETRY]) > 0)
//		{
//			purchase_hist		= DiffTime (EventTime[EVENT_HIST_START],		EventTime[EVENT_HIST_END]);
////			pre_tikrot			= DiffTime (EventTime[EVENT_TRN_START],			EventTime[EVENT_START_TO_TIK]);
////			family_sales		= DiffTime (EventTime[EVENT_FAM_SALES_START],	EventTime[EVENT_FAM_SALES_END]);
//			rpc_execute			= DiffTime (EventTime[EVENT_CALL_RPC_INIT],		EventTime[EVENT_CALL_RPC_EXEC]);
//			trn_total			= DiffTime (EventTime[EVENT_TRN_START],			EventTime[EVENT_TRN_END]);
//			gadget				= DiffTime (EventTime[EVENT_GADGET_START],		EventTime[EVENT_GADGET_END]);
//
////			GerrLogMini (GerrId,
////						 "PID %d, Hist % 4d, Del %d, Pre-Tik % 4d, RPC Init % 4d,"
////						 " RPC Exec % 5d, Init2 %d, Exec2 %d, RPC Tot % 5d, Trn Tot % 5d.",
//			GerrLogFnameMini ("nihul_tikrot_log", GerrId,
////			GerrLogMini (GerrId,
////						 "\n        Hist  Prep   Fam    RPC Gadget    Tot    Unidentified\n"
//						 "\n        Hist  Unix    RPC Gadget    Tot\n"
//						 "Timing, % 4d, % 4d, % 5d, % 5d, % 5d,",
//						 "Timing, % 4d, % 4d, % 4d, % 5d, % 5d, % 5d, % 5d,",
//
//						 mypid,
//						 purchase_hist,
//						 (trn_total - (rpc_execute + gadget)),
//						 DiffTime (EventTime[EVENT_DELETE_START],		EventTime[EVENT_DELETE_END]),
//						 (pre_tikrot - (purchase_hist + gadget)),
//						 family_sales,
//						 DiffTime (EventTime[EVENT_CALL_RPC_START],		EventTime[EVENT_CALL_RPC_INIT]),
//						 rpc_execute,
//						 gadget,
//						 DiffTime (EventTime[EVENT_CALL_RPC_EXEC],		EventTime[EVENT_CALL_RPC_RE_INIT]),
//						 DiffTime (EventTime[EVENT_CALL_RPC_RE_INIT],	EventTime[EVENT_CALL_RPC_RETRY]),
//						 DiffTime (EventTime[EVENT_CALL_RPC_START],		EventTime[EVENT_CALL_RPC_RETRY]),
//						 trn_total);
//						 (trn_total - (purchase_hist + family_sales + gadget + rpc_execute)));
//		}
//
//		if (DiffTime (EventTime[EVENT_GADGET_START], EventTime[EVENT_GADGET_END]) > 0)
//		{
//			GerrLogFnameMini ("nihul_tikrot_log", GerrId,
//						 "PID %d, Gadget %d.",
//
//						 mypid,
//						 (int)DiffTime (EventTime[EVENT_GADGET_START],		EventTime[EVENT_GADGET_END]));
//		}
//

	    break;


	case 5005:	// "Nihul Tikrot" Delivery Confirmation.
	    if (MsgErrorCode == RC_SUCCESS)
			retval = HandlerToMsg_5005		(input_buf, input_size, output_buf, &output_size, &output_type_flg, &ssmd_data);
	    else
			retval = HandlerToSpool_5005	(input_buf, input_size, output_buf, &output_size, &output_type_flg, &ssmd_data, MsgErrorCode);

		break;


	case 5051:	// Full download of credit_reasons table.
	    retval = HandlerToMsg_5051			(input_buf, input_size, output_buf, &output_size, &output_type_flg, &ssmd_data);
	    break;


	case 5053:	// Full download of subsidy_messages table.
	    retval = HandlerToMsg_5053			(input_buf, input_size, output_buf, &output_size, &output_type_flg, &ssmd_data);
	    break;


	case 5055:	// Full download of tikra_type table.
	    retval = HandlerToMsg_5055			(input_buf, input_size, output_buf, &output_size, &output_type_flg, &ssmd_data);
	    break;


	case 5057:	// Full download of hmo_membership table.
	    retval = HandlerToMsg_5057			(input_buf, input_size, output_buf, &output_size, &output_type_flg, &ssmd_data);
	    break;


	case 5059:	// Full download of virtual_store_reason_texts table.
	    retval = HandlerToMsg_5059			(input_buf, input_size, output_buf, &output_size, &output_type_flg, &ssmd_data);
	    break;


	case 5061:	// DrugGenComponents incremental download.
	    retval = HandlerToMsg_5061		(MsgIndex, input_buf, input_size, JSON_MaccabiRequest, output_buf, &output_size, &output_type_flg, &ssmd_data, &sq_num_processed);
	    break;


	case 5090:	// Request for single-sale details - new version for Nihul Tikrot.
	    retval = HandlerToMsg_X090			(input_buf, input_size, output_buf, &output_size, &output_type_flg, &ssmd_data, 5090);
	    break;


	case 6001:	// Request for Prescriptions to Fill - "Digital Prescription" version.
	case 6101:	// Request for Prescriptions to Fill - "Chanut Virtualit" version.
	    retval = HandlerToMsg_6001_6101		(MsgIndex, input_buf, input_size, JSON_MaccabiRequest, output_buf, &output_size, &output_type_flg, &ssmd_data, &sq_num_processed);
	    break;


	case 6003:	// "Digital Prescription" Sale/Credit/Deletion request.
		retval = HandlerToMsg_6003			(MsgIndex, input_buf, input_size, output_buf, &output_size, &output_type_flg, &ssmd_data, &sq_num_processed);
	    break;


	case 6005:	// "Digital Prescription" Delivery Confirmation.
//GerrLogMini(GerrId, "About to handle %s 6005 request.", (MsgErrorCode == RC_SUCCESS) ? "regular" : "spooled");
		if (MsgErrorCode == RC_SUCCESS)
			retval = HandlerToMsg_6005		(input_buf, input_size, output_buf, &output_size, &output_type_flg, &ssmd_data, &sq_num_processed);
	    else
			retval = HandlerToSpool_6005	(input_buf, input_size, output_buf, &output_size, &output_type_flg, &ssmd_data, MsgErrorCode, &sq_num_processed);
	    break;


	case 6011:	// Member drug history / ishurim download.
	    retval = HandlerToMsg_6011		(MsgIndex, input_buf, input_size, JSON_MaccabiRequest, output_buf, &output_size, &output_type_flg, &ssmd_data, &sq_num_processed);
	    break;


	case 6102:	// Prescription counts for a group of members - "Chanut Virtualit".
	    retval = HandlerToMsg_6102		(MsgIndex, input_buf, input_size, JSON_MaccabiRequest, output_buf, &output_size, &output_type_flg, &ssmd_data, &sq_num_processed);
	    break;


	case 6103:	// Predicted participation for a group of drugs, including Meishar and Nihul Tikrot reductions - "Chanut Virtualit".
	    retval = HandlerToMsg_6103		(MsgIndex, input_buf, input_size, JSON_MaccabiRequest, output_buf, &output_size, &output_type_flg, &ssmd_data, &sq_num_processed);
	    break;


	default :
	    if (LogMesgErr == 1)
	    {
		    sq_message_raw [100] = 0;
			GerrLogMini (GerrId, "Illegal message code: %d, message starts '%s'.", MsgIndex, sq_message_raw);
	    }

	    retval = ERR_ILLEGAL_MESSAGE_CODE;
	    break;
    }	// Switch on Message Index

// DonR 31Dec2020: Moved this log message to the very end of the function.
//if (!TikrotProductionMode) GerrLogMini (GerrId, "Got retval %d from %s %d request from pharmacy %d.",
//	retval, (MsgErrorCode == RC_SUCCESS) ? "regular" : "spooled", MsgIndex, ssmd_data.pharmacy_num);

// TEMPORARY: Put out a log message even in Production mode.
//else GerrLogMini (GerrId, "SqlServer %d got retval %d from %s %d request...",
//	getpid(), retval, (MsgErrorCode == RC_SUCCESS) ? "regular" : "spooled", MsgIndex);
//	sprintf (LastSignOfLife,
//			 "Handler for Transaction %d%s returned %d.",
//			 MsgIndex, (MsgErrorCode == RC_SUCCESS) ? "" : " (spool)", retval);

    input_buf -= MSG_HEADER_SIZE;

    input_size += MSG_HEADER_SIZE;

	/*=======================================================================
	||																		||
	||		INSERT MESSAGE STATISTICS IN SHARED MEMORY						||
	||																		||
	 =======================================================================*/
    ssmd_data.ok_flg	= retval;

    for (len = 0; (SpoolErr[len] != 0) && (SpoolErr[len] != ssmd_data.error_code); len++);

    if (SpoolErr [len] != 0)
    {
		retval = ssmd_data.error_code;
    }
    else
	{
		if (retval != RC_SUCCESS)
		{
			ssmd_data.error_code = retval;
		}
	}

	if (retval != ERR_ILLEGAL_MESSAGE_CODE)
	{
//		sprintf (LastSignOfLife,
//				 "Updating time statistics after Transaction %d%s returned %d.",
//				 MsgIndex, (MsgErrorCode == RC_SUCCESS) ? "" : " (spool)", retval);
//
		state = UpdateTimeStat (PHARM_SYS, &ssmd_data);

		// DonR 30Jul2007: Suppress error message for new transactions, at least
		// temporarily (and hopefully forever).
		if ((ERR_STATE (state)) && (MsgIndex <= 2033))
		{
			GerrLogMini (GerrId, "Error while updating shared memory for msg (%d)", MsgIndex);
		}
	}

	/*=======================================================================
	||																		||
	||		INSERT MESSAGE STATISTICS IN DATABASE							||
	||																		||
	 =======================================================================*/

    sq_pharmacy_num		= ssmd_data.pharmacy_num;
    sq_terminal_num		= ssmd_data.terminal_num;
    sq_member_id		= ssmd_data.member_id;
    sq_member_id_ext	= ssmd_data.member_id_ext;
    sq_spool_flg		= ssmd_data.spool_flg;
    sq_ok_flg			= ssmd_data.ok_flg;
    sq_error_code		= ssmd_data.error_code;

	/*=======================================================================
	||			RETRIES LOOP FOR INSERT										||
	 =======================================================================*/
//	sprintf (LastSignOfLife,
//			 "Writing to messages_details after Transaction %d%s returned %d.",
//			 MsgIndex, (MsgErrorCode == RC_SUCCESS) ? "" : " (spool)", retval);
//
    reStart = MAC_TRUE;

    for (tries = 0; tries < SQL_UPDATE_RETRIES && reStart == MAC_TRUE; tries++)
    {
		reStart = MAC_FALS;

		do				// transaction start
		{
			// DonR 31Dec2020: For more accurate transaction timing, update sql_srv_audit
			// *before* getting ssmd_data.proc_time and writing the time data to messages_details.
			ExecSQL (	MAIN_DB, UPD_sql_srv_audit_NotInProgress, &sq_error_code, &sq_pid, END_OF_ARG_LIST	);

		    Conflict_Test (reStart);

		    SQLERR_error_test ();


			// DonR 30Jul2007: Get the transaction-finished time here, so that
			// we'll have accurate timing even for transactions that aren't stored
			// in the shared-memory area.
			gettimeofday (&ssmd_data.proc_time, 0);

			// DonR 31Dec2020: Try a more streamlined millisecond calculation.
			ssmd_data.proc_time.tv_usec += (((ssmd_data.proc_time.tv_sec - ssmd_data.start_proc.tv_sec) * 1000000) - ssmd_data.start_proc.tv_usec);

			// DonR 30Jul2007: Correct timing calculation to give accurate time in milliseconds.
			sq_proc_time = ssmd_data.proc_time.tv_usec / 1000;


			// DonR 07Jul2016: Consolidated messages_incoming with messages_details. Since the
			// INSERT has already taken place (the INSERT that used to be for messages_incoming),
			// all we need to do now is an unconditional update.
			ExecSQL (	MAIN_DB, UPD_messages_details,
						&sq_pharmacy_num,	&sq_terminal_num,	&sq_member_id,		&sq_member_id_ext,	&sq_spool_flg,
						&sq_ok_flg,			&sq_proc_time,		&sq_num_processed,	&sq_error_code,		&rec_id,
						END_OF_ARG_LIST																					);

		    Conflict_Test (reStart);

		    SQLERR_error_test ();
		}
		while (0);			// transaction end

		if (reStart == MAC_FALS)	/* Transaction OK then COMMIT*/
		{
			CommitAllWork ();
		}
		else				/* Transaction ERR then ROLLBACK	*/
		{
			RollbackAllWork ();

		    sleep (1);
		}
    }			/* end of retries loop */

    if (reStart != MAC_FALS)		/* Error inserting row to log	*/
    {
		GerrLogReturn (GerrId,
					   "Table 'messages_details' is locked.");
    }

//	sprintf (LastSignOfLife,
//			 "Wrote to messages_details and sql_srv_audit after Transaction %d%s returned %d.",
//			 MsgIndex, (MsgErrorCode == RC_SUCCESS) ? "" : " (spool)", retval);
//
	/*=======================================================================
	||			Check if hung-up by worker									||
	 =======================================================================*/
    if (send_state == CONN_HANGUP)	/* Got hung up by tsworker	*/
    {
		GerrLogMini (GerrId,
					 "Got hung up by tsworker.");

		CloseSocket (accept_sock);

		accept_sock = -1;
	
		return (CONN_END);
    }

    send_state = AFTER_UPDATE;


	/*=======================================================================
	||																		||
	||		WRITE OUTPUT TYPE & OUTPUT MESSAGE TO PHARMACY					||
	||																		||
	 =======================================================================*/
	// Set header of answer message in case of error.
	if (retval != RC_SUCCESS)
    {
		output_type_flg = ANSWER_IN_BUFFER;

		switch (retval)
		{
			// If this was a JSON call with a bad/missing Transaction ID, produce a JSON version of the
			// error output. (If there was a valid Transaction ID, the normal handler routine is
			// responsible for creating proper JSON output even if there was an error.)
			case ERR_ILLEGAL_MESSAGE_CODE:
				if (JSON_Mode)
				{
					sprintf (output_buf, "{\"HTTP_response_status\": 404, \"request_number\": %d}", MsgIndex);
					break;
				}
				// else do nothing and fall through to the default error output.

			case ERR_WRONG_FORMAT_FILE:
				output_size = PrintErr (output_buf,
										retval,						/* error code		*/
										GET_POS() - input_buf + 1	/* input offset   */	);
				break;

			default:
				output_size = PrintErr (output_buf,
										retval,						/* error code		*/
										9999						/* Dummy message number */	);
				break;
		}
    }	// retval != RC_SUCCESS


	// Write output type to socket.
    len = sprintf (buf, "%d", output_type_flg);
	state = WriteSocket (accept_sock, buf, len, 0);

	// If that worked, send the rest of the output to socket.
	if (!ERR_STATE (state))
	{
		// Write actual transaction response to socket.
		state = WriteSocket (accept_sock, output_buf, output_size, 0);
    }


    CloseSocket (accept_sock);

    accept_sock = -1;
    
    if (ERR_STATE (state))
    {
		if (LogCommErr == 1)
		{
			GerrLogMini (GerrId, "COMM ERROR - can't send return message to tswitch.");
		}
    }

	// DonR 31Dec2020: Moved the end-of-transaction log message here, to the
	// very end of the function, just for more accurate timing.
	if (!TikrotProductionMode)
		GerrLogMini (GerrId, "Got retval %d from %s %d request from pharmacy %d.",
					 retval, (MsgErrorCode == RC_SUCCESS) ? "regular" : "spooled", MsgIndex, ssmd_data.pharmacy_num);

	// If we got a JSON request, de-allocate the cJSON instances that were allocated to it.
	if (JSON_request != NULL)
		cJSON_Delete (JSON_request);

    return (retval);
}



/*=======================================================================
||																		||
||			     ClosedPipeHandler ()									||
||																		||
 =======================================================================*/
void	 ClosedPipeHandler (int signo)
{
	sigaction (SIGPIPE, &sig_act_pipe, NULL);

	// DonR 25Nov2003: On a trial basis, try *not* terminating the program when we get a closed pipe error.
	//caught_signal = signo;
	caught_signal = 0;


	if (accept_sock != -1)
	{
		CloseSocket (accept_sock);

		accept_sock == -1;

		// Reset process if already updated SHM & DB
		if (send_state == AFTER_UPDATE)
		{
//			GerrLogMini (GerrId, "\nPID %d got hung-up by tsworker.", mypid);
		}
		else
		{
			GerrLogReturn (GerrId, "Got hung-up by tsworker -- update SHM & DB -- reset process");

			send_state = CONN_HANGUP;
		}

	}
	else
	{
		GerrLogReturn (GerrId, "Got hung-up but no connection was active -- strange & spooky");
	}
}



/*=======================================================================
||																		||
||			     TerminateHandler ()									||
||																		||
|| Note: This routine should handle any terminating/fatal signal        ||
||       that doesn't require special processing.                       ||
 =======================================================================*/
void	 TerminateHandler (int signo)
{
	char		*sig_description;
	int			time_now;
	static int	last_signo	= 0;
	static int	last_called	= 0;


	// Reset signal handling for the caught signal.
	sigaction (signo, &sig_act_terminate, NULL);


	// DonR 13Feb2020: Add special handling for segmentation faults caused by invalid
	// column/parameter pointers passed to ODBC database-interface routines. In this
	// case we just set a status variable to indicate an invalid pointer, then perform
	// a siglongjump to go back to the pointer-testing function ODBC_IsValidPointer in
	// macODBC.h.
	if ((signo == SIGSEGV) && (ODBC_ValidatingPointers))
	{
		// DonR 30May2022: For now, the only TRUE value of ODBC_ValidatingPointers
		// is 1. We may, in future, use other non-zero values to find any segmentation
		// errors we encounter inside MacODBC.
		if (ODBC_ValidatingPointers == 1)
		{
			// Segmentation fault means that the pointer we're checking is *not* OK.
			ODBC_PointerIsValid = 0;

			// Signal handling for the segmentation fault signal was already reset at
			// the beginning of this function.

			// Go back to the point just before we forced the segmentation-fault error.
			siglongjmp (BeforePointerTest, 1);
		}
		else
		{
			// Print a diagnosis and exit more-or-less gracefully from MacODBC.
			// DonR 30May2022: As noted above, this possibility does not yet exist!
			siglongjmp (AfterPointerTest, 1);
		}
	}

	// If we get here, we're dealing with something *other* than an ODBC
	// pointer-validation error.

	// We don't want to copy the signal caught into caught_signal if it was a "deliberate"
	// segmentation fault trapped when we tested ODBC parameter pointers - so I moved
	// these two lines after the pointer-validation block.
	caught_signal	= signo;
	time_now		= GetTime();


	// DonR 04Jan2004: This morning, two instances of this process went into an endless loop
	// of division-by-zero errors - presumably because Signal 8 was trapped inside a loop
	// somewhere that wasn't smart enough to break out of itself or to avoid the problem
	// entirely. To prevent future occurrences of this type, detect endless loops and
	// terminate the process "manually".
	if ((signo == last_signo) && (time_now == last_called))
	{
		GerrLogMini (GerrId,
					 "SQL Server aborting endless loop (signal %d). Terminating process.",
					 signo);

		TerminateProcess (signo, ENDLESS_LOOP);
	}
	else
	{
		last_signo	= signo;
		last_called	= time_now;
	}


	// Produce a friendly and informative message in the SQL Server logfile.
	switch (signo)
	{
		case SIGFPE:
			RollbackAllWork ();	// Any pending work is incomplete and needs to be undone.
			sig_description = "floating-point error - probably division by zero";
			break;

		case SIGSEGV:
			RollbackAllWork ();	// Any pending work is incomplete and needs to be undone.
			sig_description = "segmentation error";
			break;

		case SIGTERM:
			sig_description = "user-requested program shutdown";
			break;

		default:
			sig_description = "check manual page for SIGNAL";
			break;
	}
	
	GerrLogMini (GerrId,
				 "\nSQL Server received Signal %d (%s). Terminating process.",
				 signo,
				 sig_description);
GerrLogMini (GerrId, "ODBC_ValidatingPointers = %d.", ODBC_ValidatingPointers);

	// DonR 14Apr2015: Added a new global message buffer to help track down segmentation faults and
	// other mysterious, fatal errors.
//	if (strlen (LastSignOfLife) > 0)	// TEST VERSION!
	if ((signo != SIGTERM) && (strlen (LastSignOfLife) > 0))
	{
		GerrLogMini (GerrId,
						"\nLast sign of life:\n===============================\n%s\n===============================\n",
						LastSignOfLife);
	}
}



/*=======================================================================
||																		||
||			     NihulTikrotEnableHandler ()							||
||																		||
 =======================================================================*/
void	 NihulTikrotEnableHandler (int signo)
{
	caught_signal = signo;

	// Enable calls to AS/400 "Nihul Tikrot" program, or give a message that they are
	// already enabled.
	if (TikrotRPC_Enabled)
	{
		if (signo > 0)
		{
			GerrLogMini (GerrId, "PID %d got \"enable\" signal, but Tikrot was already enabled.", (int)getpid()	);
		}
		else
		{
			GerrLogMini (GerrId, "PID %d got internal \"enable\" call, but Tikrot was already enabled.", (int)getpid()	);
		}
	}
	else
	{
		if (signo > 0)
		{
			NeedToUpdateSysparams = 1;
			GerrLogMini (GerrId, "PID %d got \"enable\" signal - Nihul Tikrot calls enabled!", (int)getpid()	);
		}
		else
		{
			GerrLogMini (GerrId, "PID %d got internal \"enable\" call - Nihul Tikrot calls enabled!", (int)getpid()	);
		}
	}

	TikrotRPC_Enabled = 1;

	// Reset signal handling for the caught signal - if there was one.
	// We may want to call this routine based on some change in the database,
	// in which case there won't be a signal.
	if (signo > 0)
		sigaction (signo, &sig_act_NihulTikrotEnable, NULL);
}



/*=======================================================================
||																		||
||			     NihulTikrotDisableHandler ()							||
||																		||
 =======================================================================*/
void	 NihulTikrotDisableHandler (int signo)
{
	caught_signal = signo;

	// Enable calls to AS/400 "Nihul Tikrot" program, or give a message that they are
	// already disabled.
	if (!TikrotRPC_Enabled)
	{
		if (signo > 0)
		{
			GerrLogMini (GerrId, "PID %d got \"disable\" signal, but Tikrot was already disabled.", (int)getpid()	);
		}
		else
		{
			GerrLogMini (GerrId, "PID %d got internal \"disable\" call, but Tikrot was already disabled.", (int)getpid()	);
		}
	}
	else
	{
		if (signo > 0)
		{
			NeedToUpdateSysparams = 1;
			GerrLogMini (GerrId, "PID %d got \"disable\" signal - Nihul Tikrot calls disabled!", (int)getpid()	);
		}
		else
		{
			GerrLogMini (GerrId, "PID %d got internal \"disable\" call - Nihul Tikrot calls disabled!", (int)getpid()	);
		}
	}

	TikrotRPC_Enabled = 0;

	// Reset signal handling for the caught signal - if there was one.
	// We may want to call this routine based on some change in the database,
	// in which case there won't be a signal.
	if (signo > 0)
		sigaction (signo, &sig_act_NihulTikrotDisable, NULL);
}



int TerminateProcess (int signo, int in_loop)

{
	int	sq_time;
	int	sq_date;
	int	sq_pid;
	int	sq_in_progress;


	// Indicate a graceful exit in SQL Server Audit table.
	sq_pid			= mypid;
	sq_time			= GetTime ();
	sq_date			= GetDate ();
	sq_in_progress	= (in_loop == ENDLESS_LOOP) ? (9800 + signo) : (9900 + signo);

	ExecSQL (	MAIN_DB, UPD_sql_srv_audit_ProgramEnd,
				&sq_date, &sq_time, &sq_in_progress, &sq_pid, END_OF_ARG_LIST	);

	CommitAllWork ();	// Make sure we're not leaving open transactions around!

	// Disconnect from database.
	// DonR 15Jan2020: SQLMD_disconnect (which resolves to INF_DISCONNECT)
	// now handles the exit from ODBC connections as well as the old-style
	// Informix connection.
	SQLMD_disconnect ();
	SQLERR_error_test ();

	// DonR 06Jul2011: In case of segmentation fault, dump called-function diagnostics to file.
	if (in_loop == ENDLESS_LOOP)
		TRACE_DUMP ();

	// Exit child process.
	ExitSonProcess ((signo == 0) ? MAC_SERV_SHUT_DOWN : MAC_EXIT_SIGNAL);
}


long DiffTime (struct timeval tvBegin, struct timeval tvEnd)
{
	long lDiffSec		= tvEnd.tv_sec - tvBegin.tv_sec;
	long lDiffMilliSec	= ( tvEnd.tv_usec - tvBegin.tv_usec ) / 1000;

	return (lDiffSec * 1000) + lDiffMilliSec;
}



short LoadPrescrSource ()

{
	static short Initialized	= 0;
	ISOLATION_VAR;

	PRESCR_SOURCE	PrescrSourceRow;


	REMEMBER_ISOLATION;
	SET_ISOLATION_DIRTY;

	DeclareAndOpenCursorInto	(	MAIN_DB, LoadPrescSourceCur,
									&PrescrSourceRow.pr_src_code,			&PrescrSourceRow.pr_src_docid_type,		&PrescrSourceRow.id_type_accepted,
									&PrescrSourceRow.PrSrcDocLocnRequired,	&PrescrSourceRow.pr_src_priv_pharm,		&PrescrSourceRow.allowed_by_default,
									END_OF_ARG_LIST																											);

	// No real error-trapping - just exit if the OPEN didn't work.
	if (SQLCODE != 0)
	{
		// If this is the first time AND the OPEN failed, force initialization. Note that
		// if we've already initialized the array once AND the OPEN fails, we'll retain
		// the array's current values.
		if (!Initialized)
			memset ((char *)PrescrSource,	0, sizeof(PrescrSource));

		return (-1);
	}


	memset ((char *)PrescrSource,	0, sizeof(PrescrSource));

	while (1)
	{
		FetchCursor (	MAIN_DB, LoadPrescSourceCur	);

		if (SQLERR_code_cmp (SQLERR_end_of_fetch) == MAC_TRUE)
		{
			break;
		}
		else
		{
			// Log errors to file but otherwise ignore them.
			if (SQLERR_error_test ())
			{
				Initialized = 0;	// Assume values are invalid, so force re-initialization.
				continue;
			}
			else
			{
				// Successful read!
				PrescrSource	[PrescrSourceRow.pr_src_code] = PrescrSourceRow;
			}	// Successful cursor fetch.
		}	// Not end-of-fetch.
	}	// Cursor-reading loop for prescr_source.

	CloseCursor (	MAIN_DB, LoadPrescSourceCur	);

	Initialized = 1;	// We've successfully loaded the array, so values should be valid.
	RESTORE_ISOLATION;

	return (0);
}


int LoadTrnPermissions ()
{
	static short	Initialized	= 0;
	short			RowCount	= 0;
	ISOLATION_VAR;

	short	transaction_num;
	short	spool;
	short	ok_maccabi;
	short	ok_pvt_hesder;
	short	ok_non_hesder;
	short	MinVersionPermitted;


	REMEMBER_ISOLATION;
	SET_ISOLATION_DIRTY;

	memset ((char *)TrnPermissions, 0, sizeof(TrnPermissions));

	DeclareAndOpenCursorInto	(	MAIN_DB, LoadTrnPermitCur,
									&transaction_num,	&spool,			&ok_maccabi,
									&ok_pvt_hesder,		&ok_non_hesder, &MinVersionPermitted,
									END_OF_ARG_LIST												);

	// No real error-trapping - just exit if the OPEN didn't work.
	if (SQLCODE != 0)
	{
		// If this is the first time AND the OPEN failed, force initialization. Note that
		// if we've already initialized the array once AND the OPEN fails, we'll retain
		// the array's current values.
		Initialized	= 0;
		return (-1);
	}

	while (RowCount < 200)
	{
		FetchCursor	(	MAIN_DB, LoadTrnPermitCur	);

		if (SQLERR_code_cmp (SQLERR_end_of_fetch) == MAC_TRUE)
		{
			break;
		}
		else
		{
			// Skip past any errors. Note that any "missing" transaction will be assumed to be enabled
			// for all pharmacy types.
			if (SQLERR_error_test ())
			{
		 		Initialized	= 0;
				break;
			}
			else
			{
				// Successful read!

				// Sanity check: The minimum value for MinVersionPermitted is zero (= no minimum version).
				if (MinVersionPermitted < 0)
					MinVersionPermitted = 0;

				TrnPermissions [RowCount].transaction_num		= transaction_num;
				TrnPermissions [RowCount].spool					= spool;
				TrnPermissions [RowCount].permitted[0]			= ok_maccabi;
				TrnPermissions [RowCount].permitted[1]			= ok_pvt_hesder;
				TrnPermissions [RowCount].permitted[2]			= ok_non_hesder;
				TrnPermissions [RowCount].MinVersionPermitted	= MinVersionPermitted;

				RowCount++;
			}	// Successful cursor fetch.
		}	// Not end-of-fetch.
	}	// Cursor-reading loop for pharm_trn_permit.

	CloseCursor	(	MAIN_DB, LoadTrnPermitCur	);

	Initialized = 1;	// We've successfully loaded the array, so values should be valid.
	RESTORE_ISOLATION;

	return (0);
}
