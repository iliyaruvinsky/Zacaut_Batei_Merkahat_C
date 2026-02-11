#ifndef _TIKROTRPC_H_
#define _TIKROTRPC_H_

#include <sys/time.h>

	// Public definitions first.
	#ifndef NULL
		#define NULL	0
	#endif

	#define RPC_INP_DUMMY_LEN		   7
	#define RPC_INP_HEADER_LEN		  64	// = 63, with one byte extra.
	#define RPC_INP_CURRSALE_LEN	1751	// = 35 * 50, with one byte extra (DonR 30Jul2024 User Story #338533: added 1 byte/line).
	#define RPC_INP_PRIORSALES_LEN	 451	// =  9 * 50, with one byte extra.
	#define RPC_OUT_HEADER_LEN		  20	// = 19, with one byte extra.
	#define RPC_OUT_TIKROT_LEN		2951	// = 59 * 50, with one byte extra. Nihul Tikrot actually gives only up to 10 lines of output.
	#define RPC_OUT_CURRSALE_LEN	 901	// = 18 * 50, with one byte extra.
	#define RPC_OUT_COUPONS_LEN		 131	// = 13 * 10, with one byte extra. Nihul Tikrot actually gives only up to 5 lines of output.

	// Defines for event-timing array subscripts.
	#define EVENT_TRN_START			 0
	#define EVENT_DELETE_START		 1
	#define EVENT_DELETE_END		 2
	#define EVENT_HIST_START		 3
	#define EVENT_HIST_END			 4
	#define EVENT_GADGET_START		 5
	#define EVENT_GADGET_END		 6
	#define EVENT_START_TO_TIK		 7
	#define EVENT_FAM_SALES_START	 8
	#define EVENT_FAM_SALES_END		 9
	#define EVENT_CALL_RPC_START	10
	#define EVENT_CALL_RPC_INIT		11
	#define EVENT_CALL_RPC_EXEC		12
	#define EVENT_CALL_RPC_RE_INIT	13
	#define EVENT_CALL_RPC_RETRY	14
	#define EVENT_TRN_END			19


	int CallTikrotSP (char *Header_in,
					  char *CurrentSale_in,
					  char *PriorSales_in,
					  char **Header_out,
					  char **Tikrot_out,
					  char **CurrentSale_out,
					  char **Coupons_out);

	int InitTikrotSP (void);

	long DiffTime (struct timeval tvBegin, struct timeval tvEnd);

	// DonR 14Nov2010: Define event-time array for main module; reference it for everything else.
	#ifdef MAIN
		struct timeval	EventTime	[20];
	#else
		extern struct timeval	EventTime[];
	#endif

	// Private stuff.
	#ifdef _TIKROTRPC_C_

		static int		RPC_Allocated	= 0;
		static int		RPC_Connected	= 0;
		static int		RPC_Recursed	= 0;

		static SQLHENV	RPC_henv;
		static SQLHDBC	RPC_hdbc;
		static SQLHSTMT	RPC_hstmt;	// StatementHandle

		static SQLCHAR	*RPC_DSN;	// Set conditionally based on /pharm/bin/NihulTikrot.cfg
		static SQLCHAR	RPC_DSN_userid[]			= "UNIXTIKROT";
		static SQLCHAR	RPC_DSN_password[]			= "ALEX110743";

		int				TikrotProductionMode	= 0;	// Default to test mode.
		int				TikrotRPC_Enabled		= 1;	// Default to enabled mode.

		int		RPC_Initialize	(void);
		void	RPC_Cleanup		(void);

	#else
		extern int		TikrotProductionMode;	// So operating mode is visible to calling routine.
		extern int		TikrotRPC_Enabled;		// So RPC-enabled mode is visible to calling routine.
	#endif // _TIKROTPRC_C_


	// If we're compiling on Unix, compile an empty function stub to avoid unreferenced-symbol errors.
	#ifndef _LINUX_
		#ifdef MAIN
			int		TikrotProductionMode	= 1;	// Stubbed-out version is in "production mode" to eliminate
													// unnecessary computation.

			int		TikrotRPC_Enabled		= 0;	// Disabled for SCO Unix.

			int CallTikrotSP (char *Header_in,
							  char *CurrentSale_in,
							  char *PriorSales_in,
							  char **Header_out,
							  char **Tikrot_out,
							  char **CurrentSale_out,
							  char **Coupons_out)
			{
				return(0);
			}

			int InitTikrotSP (void)
			{
				return(0);
			}

		#endif
	#endif

#endif // _TIKROTRPC_H_
