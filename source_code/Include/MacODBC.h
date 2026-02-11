/*============================================================================
||																			||
||				MacODBC.h													||
||																			||
||==========================================================================||
||																			||
||  PURPOSE:																||
||																			||
||  Header file for Maccabi ODBC "infrastructure" functions.				||
||																			||
||--------------------------------------------------------------------------||
|| PROJECT:	ODBC database conversion										||
||--------------------------------------------------------------------------||
|| PROGRAMMER:	Don Radlauer.												||
|| DATE:		December 2019.												||
||--------------------------------------------------------------------------||
||																			||
|| This file should be in the standard include directory.					||
||																			||
 ===========================================================================*/



// Having both "#pragma once" and "ifndef" should be redundant, but harmless.
#pragma once
#ifndef MacODBC_H
#define MacODBC_H


// ODBC includes.
#include "/usr/local/include/sql.h"
#include "/usr/local/include/sqlext.h"
#include <errno.h>
#include <locale.h>
#include <string.h>
#include <setjmp.h>
#include "GenSql.h"		// Marianna 26May2020

// includes the entire collection of Maccabi SQL command IDs, including
// numeric and text versions - the text version is used for diagnostics.
#include <MacODBC_MyOperatorIDs.h>

// Global variable to handle signal-trapping.
extern int	caught_signal;


// DonR 23Feb2020: "Missing" #defines for setting preserve-cursor behavior for MS-SQL.
#define SQL_COPT_SS_PRESERVE_CURSORS 1204
#define SQL_PC_ON 1L
#define SQL_PC_OFF 0L

// DonR 21Jun2020: "Missing" #defines for Informix.
#define SQL_INFX_ATTR_AUTO_FREE                  2263


// DonR 12Jul2020: Add #define'd parameter to enable/disable pointer checking.
// I want to see if disabling this feature has any impact on the gradual
// performance degradation we're seeing.
// ADDENDUM: I found and fixed an entirely different problem, so for now at least,
// I'm going to set this back to enabled. It can be set back to zero to try to
// increase performance by a smidgin.
#define ENABLE_POINTER_VALIDATION			1
#define CHECK_VALIDITY_ONLY					0
#define CHECK_POINTER_IS_WRITABLE			1

// DonR 14Jul2020: Add another #define'd parameter to control validation that
// "sticky" operations are still in PREPARE'd state and ready for use. It
// appears that the previous problems in this regard were the result of a
// bug in the handling of *non*-sticky statements: sometimes they were
// DECLARE'd and never opened/fetched/closed, and then the next time they
// were used they were DECLARE'd all over again. This has now been fixed -
// see changes made on 11Jul2020 and 12Jul2020. (I've also fixed calling
// routines to avoid the problem where I've seen it come up.) So now we have
// the option of turning off the extra validation step, and (hopefully)
// improving performance a bit.
#define ENABLE_PREPARED_STATE_VALIDATION	1


// Define the maxiumum number of "sticky" statements that can be DECLARE'd
// and left open for ongoing use. 100 seems like a reasonable maxiumum, and
// I believe that as of July 2020 we're using fewer than 50 at once.
// DonR 26Oct2020: I'm seeing some "maxed out" messages in the log. I'm not
// 100% sure if this is because there are in fact more than 100 sticky
// statements now, or if somehow the number is creeping up because of a bug
// somewhere. I'll try increasing the maximum to 150; if there's a bug, this
// won't help, but if it's legitimate that we need a bit more than 100, then
// 150 will probably be more than adequate.
// DonR 29Oct2020: I think I see what's going wrong - revert to 100.
// DonR 10Jul2025: Just out of paranoia, I'm going to increase to 120 "stickies".
#define ODBC_MAX_STICKY_STATEMENTS	120
//#define ODBC_MAX_STICKY_STATEMENTS	100
//#define ODBC_MAX_STICKY_STATEMENTS	150


// Define the maxiumum number of parameters we expect to be able to pass to
// and from an SQL operation. 300 seems reasonable, given that the most
// complicated table in the database as of July 2020 (drug_list) has 145
// columns. Note that this number needs to accommodate both input *and*
// output parameters, since their pointers are stored consecutively in
// an array!
#define ODBC_MAX_PARAMETERS	300


enum ODBC_DatabaseProvider
{
	ODBC_NoProvider,	// Just so the first real provider is non-zero.
	ODBC_Informix,
	ODBC_DB2,
	ODBC_MS_SqlServer,
	ODBC_Oracle
};

// Define macros for various types of SQL commands.
// DonR 13Jul2020: Just for convenience, added DECLARE_AND_OPEN_CURSOR
// and DECLARE_AND_OPEN_CURSOR_INTO options, to enable combining the
// DECLARE and OPEN operations in a single statement.
enum ODBC_CommandType
{
	DECLARE_CURSOR,
	DECLARE_CURSOR_INTO,
	DECLARE_AND_OPEN_CURSOR,
	DECLARE_AND_OPEN_CURSOR_INTO,
	DEFERRED_INPUT_CURSOR,
	DEFERRED_INPUT_CURSOR_INTO,
	OPEN_CURSOR,
	OPEN_CURSOR_USING,
	FETCH_CURSOR,
	FETCH_CURSOR_INTO,
	CLOSE_CURSOR,
	FREE_STATEMENT,
	SINGLETON_SQL_CALL,
	COMMIT_WORK,
	ROLLBACK_WORK,
	GET_LENGTHS_READ,
	SET_DIRTY_READ,
	SET_COMMITTED_READ,
	SET_REPEATABLE_READ,
	SET_CUSTOM_SIGSEGV_HANDLER
};

// DonR 10Jul2025: Add the definition of "bool" here, so we can get its size
// and properly support boolean variables.
#ifndef BOOL_DEFINED
	#define BOOL_DEFINED
	typedef enum tag_bool
	{
		false = 0,
		true
	}
	bool;                  /* simple boolean typedef */
#endif

// DonR 22Jan2020: For some reason, something was getting screwed up when we
// needed a retry for the ODBC connect to Informix. Moving the HDBC part of
// the ODBC_DB_HEADER structure to the end seemed to help. Just in case some
// ODBC driver function is overflowing the bounds of HDBC, I've added an
// overflow buffer - it really shouldn't be needed, and I should play with
// this stuff later to make sure I've got a proper fix!
// DonR 06Aug2020: A few days ago I took out the overflow buffer, and this
// version is running - and appears to be stable - in Production. I'll leave
// in place, remarked out, just in case a problem comes up when we start
// playing with MS-SQL.
typedef struct ODBC_DB_HEADER		{ int Provider; int Connected; SQLHDBC HDBC; char Name[21]; /* char HDBC_OverflowBuffer[1000]; */ }		ODBC_DB_HEADER;
typedef struct ODBC_ColumnParams	{ int type; int length;}																				ODBC_ColumnParams;


// Declare global variables.
#ifdef MAIN
//	static int			ForceDbErrorTenthTime		= 0;	// TEMPORARY FOR TESTING "3926" RECOVERY.
	static SQLHENV		ODBC_henv;							// The ODBC environment is used only at connect-time and exit-time.
	ODBC_DB_HEADER		MS_DB;
	ODBC_DB_HEADER		INF_DB;
	ODBC_DB_HEADER		*MAIN_DB					= NULL;	// DonR 14Jul2020: Added so that calling routine can set a "generic" DB pointer value to whatever DB is desired.
	ODBC_DB_HEADER		*ALT_DB						= NULL;	// DonR 23Aug2020: If two DB's are connected, this will be the non-primary one - otherwise it'll be equal to MAIN_DB.
	static short		ODBC_PRESERVE_CURSORS		= 0;	// Set TRUE *BEFORE* connecting to database if we want cursors to behave as if they were declared
															// "WITH HOLD" - otherwise, by default ODBC closes all cursors when there is a COMMIT.
	static short		ODBC_henv_allocated			= 0;	// DonR 06Oct2021 - instead of using "first time called" to decide whether to allocate the environment.
	static int			NUM_ODBC_DBS_CONNECTED		= 0;
	static int			ALTERNATE_DB_USED			= 0;	// DonR 31Dec2020: So we can optimize CommitAllWork and (hopefully) improve performance.
	int					ODBC_MIRRORING_ENABLED;				// DonR 21Feb2021: ODBC "mirroring" enhancement.
	int					MIRROR_SQLCODE				= 0;	// DonR 21Feb2021: ODBC "mirroring" enhancement.
	SQLLEN				MIRROR_ROWS_AFFECTED		= 0;	// DonR 21Feb2021: ODBC "mirroring" enhancement.
	int					LOCK_TIMEOUT				= 500;	// DonR 03Mar2021: Query lock timeout in milliseconds.
	int					DEADLOCK_PRIORITY			= 0;	// DonR 03Mar2021: 0 = normal, -10 to 10 = low to high priority.
	int					ODBC_SQLCODE				= 0;	// So error routines can see if SQLCODE came from ODBC.
	char				ODBC_ErrorBuffer		[5000];

	short				ODBC_AvoidRecursion			= 0;	// So logging routines know not to call ODBC functions if they're called *from* an ODBC function.

	short				ODBC_ValidatingPointers		= 0;	// Global indicator that we're validating pointers sent to ODBC routine; this lets us know
															// that if we hit a segmentation fault, that's the reason why - and we'll handle the error
															// gracefully.
	short				ODBC_PointerIsValid;				// Global indicator for pointer validity; it will be set TRUE before read/write checking a
															// column/parameter pointer, and will be set FALSE by the segmentation-fault trapping routine.

	short				Connect_ODBC_Only			= 0;	// Global indicator so a given mainline can suppress all calls to embedded SQL.

	struct sigaction	sig_act_ODBC_SegFault;
	sigjmp_buf			BeforePointerTest;
	sigjmp_buf			AfterPointerTest;

	char				*CurrentMacroName			= "";	// Made global for easy diagnostics.

	// DonR 01Jul2020: Add operation counter and diagnostic threshold to improve "re-PREPARE" diagnostics.
	static long			ODBC_ActivityCount			= 0L;
	static long			PrintDiagnosticsAfter		= -1L;

	// Useful little global variables so we can do INSERT's without
	// hard-coded VALUES.
	int				IntZero						= 0;
	int				IntOne						= 1;
	int				IntTwo						= 2;
	short			ShortZero					= 0;
	short			ShortOne					= 1;
	short			ShortTwo					= 2;
	long			LongZero					= 0L;

	SQLLEN			*ColumnOutputLengths;
	char			ODBC_LastSQLStatementText [6000];
	char			*END_OF_ARG_LIST			= "End of argument list!";

	// DonR 24May2020: Moved some static status variables from function scope to module scope,
	// since sometimes it seems like function-scope static variables are less static than one
	// would want them to be.
	static int			ODBC_Exec_FirstTimeCalled	= 1;
	static int			NumStickyHandlesUsed		= 0;

	// DonR 29Jun2020: Moved these to module scope, so the error-handling routine
	// can clear them when necessary.
	// DonR 08Jul2020: For simpler code, made all these arrays conventional
	// pre-dimensioned ones rather than pointers to be malloc'ed. Since they're
	// never re-sized, there's no point in using malloc - and simpler code
	// is a good thing!
	static short		StatementPrepared		[ODBC_MAXIMUM_OPERATION];
	static short		StatementOpened			[ODBC_MAXIMUM_OPERATION];
	static short		StatementIsSticky		[ODBC_MAXIMUM_OPERATION];

	// DonR 10Jul2025: Add a static variable to allow proper handling of boolean variables,
	// no matter what integer type the compiler assigns to the "bool" enum.
	static int			BoolType = SQL_C_SLONG;	// That seems to be the current correct value, so we'll use it as our default.

#else	// External declarations for variables available to other source modules.
	extern ODBC_DB_HEADER	MS_DB;
	extern ODBC_DB_HEADER	INF_DB;
	extern ODBC_DB_HEADER	*MAIN_DB;				// DonR 14Jul2020: Added so that calling routine can set a "generic" DB pointer value to whatever DB is desired.
	extern ODBC_DB_HEADER	*ALT_DB;				// DonR 23Aug2020: If two DB's are connected, this will be the non-primary one - otherwise it'll be equal to MAIN_DB.
	extern int				NUM_ODBC_DBS_CONNECTED;
	extern int				ODBC_MIRRORING_ENABLED;	// DonR 21Feb2021: ODBC "mirroring" enhancement.
	extern int				MIRROR_SQLCODE;			// DonR 21Feb2021: ODBC "mirroring" enhancement.
	extern SQLLEN			MIRROR_ROWS_AFFECTED;	// DonR 21Feb2021: ODBC "mirroring" enhancement.
	extern int				LOCK_TIMEOUT;			// DonR 03Mar2021: Query lock timeout in milliseconds.
	extern int				DEADLOCK_PRIORITY;		// DonR 03Mar2021: 0 = normal, -10 to 10 = low to high priority.
	extern int				ODBC_SQLCODE;			// So error routines can see if SQLCODE came from ODBC.
	extern short			ODBC_PRESERVE_CURSORS;	// Set TRUE *BEFORE* connecting to database if we want cursors to behave as if they were declared "WITH HOLD".
	extern char				ODBC_ErrorBuffer [];
	extern int				IntZero;
	extern int				IntOne;
	extern int				IntTwo;
	extern short			ShortZero;
	extern short			ShortOne;
	extern short			ShortTwo;
	extern long				LongZero;
	extern SQLLEN			*ColumnOutputLengths;
	extern char				ODBC_LastSQLStatementText [];
	extern char				*END_OF_ARG_LIST;
	extern short			ODBC_AvoidRecursion;
	extern short			ODBC_ValidatingPointers;
	extern short			ODBC_PointerIsValid;
	extern short			Connect_ODBC_Only;
	extern struct sigaction	sig_act_ODBC_SegFault;
	extern sigjmp_buf		BeforePointerTest;
	extern sigjmp_buf		AfterPointerTest;
#endif


// Simple macros to test for SQL success or failure.
#define SQL_WORKED(R)	((R == SQL_SUCCESS) || (R == SQL_SUCCESS_WITH_INFO))
#define SQL_FAILED(R)	((R != SQL_SUCCESS) && (R != SQL_SUCCESS_WITH_INFO))


// Macros for the various ODBC interface "functions".
//
// OpenCursor, CloseCursor, and FetchCursor don't need any variable arguments; in GNU C,
// the ## token eliminates the preceding comma in macro expansion and thus avoids having
// to decide between compiler errors and adding a meaningless NULL parameter at the end
// of each macro call. Note that I tried using __VA_OPT__ instead, but the compiler
// didn't seem to like it. (I added this token to ExecSQL as well, for statements like
// CREATE TABLE that have neither input nor output columns.)
// NOTE: I gave two different spellings of the singleton command: ExecSQL and ExecSql. It
// seemed easier to have both recognized than to stop myself from typing it the wrong way!
// ...And I did the same for the Rollback commands - they'll work for Rollback... and RollBack.
//
// The order of arguments to the macros is as follows:
// 1) Pointer to the database header structure (type ODBC_DB_HEADER) if relevant.
// 2) The SQL Operation ID (defined as enum in MacODBC_MyOperatorIDs.h) if relevant.
// (Everything from here on is part of the variable-arguments list - always supplied as pointers.)
// 3) IF the SQL Operation is defined in the local copy of MacODBC_MyOperators.c as NULL,
//    the address of the SQL command text to PREPARE (for Declare and ExecSQL commands).
// 4) IF the SQL Operation requires a custom WHERE clause, the WHERE clause identifier.
//    (This is relevant only for Declare and ExecSQL commands.)
// 5) IF there is a custom WHERE clause and its WhereClauseText is defined as NULL,
//    the address of the WHERE clause text to PREPARE (for Declare and ExecSQL commands).
// 6) Output variable pointers, where relevant.
// 7) Input variable pointers, where relevant.
//
// DonR 25Dec2019: Added a new safety feature: If you add the optional parameter END_OF_ARG_LIST
// to any of these ODBC_Exec calls (as the last argument, obviously) you'll get an error message
// if your parameter list is shorter than it should be. The error message will tell you what the
// next argument was supposed to be, so it should be easy to diagnose what went wrong.
// NOTE: quite some time ago, I made END_OF_ARG_LIST *non*-optional!
#define DeclareCursor(DB,OP_ID,...)					ODBC_Exec (DB,		DECLARE_CURSOR,					OP_ID,	NULL,		##__VA_ARGS__)
#define DeclareCursorInto(DB,OP_ID,...)				ODBC_Exec (DB,		DECLARE_CURSOR_INTO,			OP_ID,	NULL,		  __VA_ARGS__)
#define DeclareAndOpenCursor(DB,OP_ID,...)			ODBC_Exec (DB,		DECLARE_AND_OPEN_CURSOR,		OP_ID,	NULL,		##__VA_ARGS__)
#define DeclareAndOpenCursorInto(DB,OP_ID,...)		ODBC_Exec (DB,		DECLARE_AND_OPEN_CURSOR_INTO,	OP_ID,	NULL,		  __VA_ARGS__)
#define DeclareDeferredCursor(DB,OP_ID,...)			ODBC_Exec (DB,		DEFERRED_INPUT_CURSOR,			OP_ID,	NULL,		##__VA_ARGS__)
#define DeclareDeferredCursorInto(DB,OP_ID,...)		ODBC_Exec (DB,		DEFERRED_INPUT_CURSOR_INTO,		OP_ID,	NULL,		  __VA_ARGS__)
#define OpenCursor(DB,OP_ID,...)					ODBC_Exec (DB,		OPEN_CURSOR,					OP_ID,	NULL,		##__VA_ARGS__)
#define OpenCursorUsing(DB,OP_ID,...)				ODBC_Exec (DB,		OPEN_CURSOR_USING,				OP_ID,	NULL,		##__VA_ARGS__)
#define FetchCursor(DB,OP_ID,...)					ODBC_Exec (DB,		FETCH_CURSOR,					OP_ID,	NULL,		##__VA_ARGS__)
#define FetchCursorInto(DB,OP_ID,...)				ODBC_Exec (DB,		FETCH_CURSOR_INTO,				OP_ID,	NULL,		  __VA_ARGS__)
#define CloseCursor(DB,OP_ID,...)					ODBC_Exec (DB,		CLOSE_CURSOR,					OP_ID,	NULL,		##__VA_ARGS__)
#define FreeStatement(DB,OP_ID,...)					ODBC_Exec (DB,		FREE_STATEMENT,					OP_ID,	NULL,		##__VA_ARGS__)
#define ExecSQL(DB,OP_ID,...)						ODBC_Exec (DB,		SINGLETON_SQL_CALL,				OP_ID,	NULL,		##__VA_ARGS__)
#define ExecSql(DB,OP_ID,...)						ODBC_Exec (DB,		SINGLETON_SQL_CALL,				OP_ID,	NULL,		##__VA_ARGS__)
#define CommitWork(DB,...)							ODBC_Exec (DB,		COMMIT_WORK,					0,		NULL,		##__VA_ARGS__)
#define RollbackWork(DB,...)						ODBC_Exec (DB,		ROLLBACK_WORK,					0,		NULL,		##__VA_ARGS__)
#define RollBackWork(DB,...)						ODBC_Exec (DB,		ROLLBACK_WORK,					0,		NULL,		##__VA_ARGS__)
#define CommitAllWork(...)							ODBC_Exec (NULL,	COMMIT_WORK,					0,		NULL,		##__VA_ARGS__)
#define RollbackAllWork(...)						ODBC_Exec (NULL,	ROLLBACK_WORK,					0,		NULL,		##__VA_ARGS__)
#define RollBackAllWork(...)						ODBC_Exec (NULL,	ROLLBACK_WORK,					0,		NULL,		##__VA_ARGS__)
#define SetDirtyRead(DB,...)						ODBC_Exec (DB,		SET_DIRTY_READ,					0,		NULL,		##__VA_ARGS__)
#define SetCommittedRead(DB,...)					ODBC_Exec (DB,		SET_COMMITTED_READ,				0,		NULL,		##__VA_ARGS__)
#define SetRepeatableRead(DB,...)					ODBC_Exec (DB,		SET_REPEATABLE_READ,			0,		NULL,		##__VA_ARGS__)
#define GetLengthsRead(LEN_OUT,...)					ODBC_Exec (NULL,	GET_LENGTHS_READ,				0,		LEN_OUT,	##__VA_ARGS__)
#define SetCustomSegmentationFaultHandler(...)		ODBC_Exec (NULL,	SET_CUSTOM_SIGSEGV_HANDLER,		0,		NULL,		  __VA_ARGS__)


// Macros for "generic" ODBC error-handling function.
enum ODBC_ErrorCategory
{
	ODBC_ENVIRONMENT_ERR,
	ODBC_DB_HANDLE_ERR,
	ODBC_STATEMENT_ERR
};

#define ODBC_EnvironmentError(EnvPtr)							ODBC_ErrorHandler (ODBC_ENVIRONMENT_ERR,	EnvPtr,	NULL,	NULL,			0)
#define ODBC_DB_ConnectionError(DB_Ptr)							ODBC_ErrorHandler (ODBC_DB_HANDLE_ERR,		NULL,	DB_Ptr, NULL,			0)
#define ODBC_StatementError(StatementPtr,OperationIdentifier)	ODBC_ErrorHandler (ODBC_STATEMENT_ERR,		NULL,	NULL,	StatementPtr,	OperationIdentifier)


// Function declarations.
int ODBC_Exec						(	ODBC_DB_HEADER		*Database,
										int					CommandType_in,
										int					OperationIdentifier_in,
										SQLLEN				**LengthReadArrayPtr_in,
										...											);

int SQL_GetMainOperationParameters	(	ODBC_DB_HEADER		*Database,
										int					OperationIdentifier_in,
										char				**SQL_CommandText_out,
										int					*NumOutputColumns_out,
										ODBC_ColumnParams	**OutputColumns_out,
										int					*NumInputColumns_out,
										ODBC_ColumnParams	**InputColumns_out,
										int					*NeedsWhereClauseIdentifier_out,
										int					*GenerateVALUES_out,
										short				*StatementIsSticky_out,
										short				*Convert_NF_to_zero_out,
										short				*CommandIsMirrored_out,
										short				*SuppressDiagnostics_out,
										char				**CursorName_out,
										char				**ErrorDescription_out	);

int SQL_GetWhereClauseParameters	(	long				WhereClauseIdentifier_in,
										char				**WhereClauseText_out,
										int					*NumInputColumns_out,
										ODBC_ColumnParams	**InputColumns_out,
										char				**ErrorDescription_out	);


int SQL_CustomizePerDB				(	ODBC_DB_HEADER		*Database,
										char				*SQL_Command_in,
										int					*CommandLength_out		);

int ParseColumnList					(	char				*SpecStringIn,
										int					NumColumnsIn,
										ODBC_ColumnParams	*ParamsOut		);

int find_FOR_UPDATE_or_GEN_VALUES	(	int		OperationIdentifier,
										char	*SQL_command_in,
										int		*FoundForUpdate_out,
										short	*FoundInsert_out,
										int		*FoundSelect_out,
										int		*FoundCustomWhereInsertionPoint_out	);

int ODBC_CONNECT					 (	ODBC_DB_HEADER	*Database,
										char			*DSN,
										char			*username,
										char			*password,
										char			*dbname,
										char			*timeout_str	);

int CleanupODBC						(	ODBC_DB_HEADER	*Database					);

int ODBC_ErrorHandler				(	int			ErrorCategory,
										SQLHENV		*Environment,
										SQLHDBC		*Database,
										SQLHSTMT	*Statement,
										int			OperationIdentifier				);

int ODBC_IsValidPointer				(	void *Pointer_in, int CheckPointerWritable_in, int ColumnType_in, char **ErrDesc_out	);

// DonR 18May2022: I'm going to have ODBC_Exec() automatically set up a handler
// for segmentation faults, so mainlines don't have to bother unless they need
// to use a "custom" handler. In order to enable all this, we need a function-
// pointer variable that defaults to the local handler function, but can be set
// to something else when necessary.
void macODBC_SegmentationFaultCatcher	(int signo);
static void (* SIGSEGV_Handler)(int) = macODBC_SegmentationFaultCatcher;

// The actual function code should compile only once, in the mainline source module.
#ifdef MAIN

// Finally, here are the actual ODBC infrastructure functions.

// C type identifier	ODBC C typedef			C type
//
// SQL_C_CHAR			SQL_CHAR				unsigned char *
// SQL_C_CHAR			SQL_VARCHAR				unsigned char *
// SQL_C_UTINYINT		SQL_C_UTINYINT			char			(For single-character variables - NOT TESTED AS OF 11DEC2019)
// SQL_C_WCHAR			SQL_WCHAR *				wchar_t *		(NOT SET UP YET)
// SQL_C_SSHORT			SQL_SMALLINT			short int
// SQL_C_USHORT			SQL_USMALLINT			unsigned short int
// SQL_C_SLONG			SQL_INTEGER				int
// SQL_C_ULONG			SQL_UINTEGER			unsigned int
// SQL_C_FLOAT			SQL_REAL				float
// SQL_C_DOUBLE			SQL_DOUBLE, SQL_FLOAT	double
// SQL_C_BIT			SQLCHAR					unsigned char
// SQL_C_STINYINT		SQLSCHAR				signed char
// SQL_C_UTINYINT		SQLCHAR					unsigned char
// SQL_C_SBIGINT		SQLBIGINT				long
// SQL_C_UBIGINT		SQLUBIGINT				unsigned long	(NOT SET UP YET)
// SQL_C_BINARY			SQLCHAR *				unsigned char *	(NOT SET UP YET)


int ODBC_Exec (	ODBC_DB_HEADER	*Database,
				int				CommandType_in,
				int				OperationIdentifier_in,
				SQLLEN			**LengthReadArrayPtr_in,
				...	)
{
	va_list				ArgList;
	static char			SQL_CmdBuffer				[6000];
	static SQLLEN		FieldLengthsRead			[ODBC_MAX_PARAMETERS];
	static void			*ColumnParameterPointers	[ODBC_MAX_PARAMETERS];

	// DonR 08Jul2020: For simpler code, made all these arrays conventional
	// pre-dimensioned ones rather than pointers to be malloc'ed. Since they're
	// never re-sized, there's no point in using malloc - and simpler code
	// is a good thing!
	static SQLHSTMT		StatementCache				[ODBC_MAXIMUM_OPERATION];
	static SQLHSTMT		MirrorStatementCache		[ODBC_MAXIMUM_OPERATION];	// DonR 21Feb2021: ODBC "mirroring" enhancement.
	static short		Statement_DB_Provider		[ODBC_MAXIMUM_OPERATION];
	static short		Convert_NF_to_zero			[ODBC_MAXIMUM_OPERATION];
	static short		CommandIsMirrored			[ODBC_MAXIMUM_OPERATION];	// DonR 21Feb2021: ODBC "mirroring" enhancement.
	static short		IsInsertCommand				[ODBC_MAXIMUM_OPERATION];	// DonR 18May2022: To detect MS-SQL "silent insert failure".
	static char			*SavedCommandTextPtr		[ODBC_MAXIMUM_OPERATION];

	short				ColumnParameterCount		= 0;
	short				CaughtParameterProblem		= 0;
	short				FoundFetchError				= 0;	// DonR 22Mar2021.
	ODBC_DB_HEADER		*MIRROR_DB					= NULL;	// DonR 21Feb2021: ODBC "mirroring" enhancement.

	int					ReturnCode					= 0;
	int					result;
	int					i;
	int					CmdLen;
	SQLLEN				RowsAffected				= 0;
	SQLHSTMT			*ThisStatementPtr;
	SQLHSTMT			*MirrorStatementPtr;	// DonR 21Feb2021: ODBC "mirroring" enhancement.

	// Flags to indicate which operations are necessary for a particular command type.
	// By default, everything is turned off.
	// DonR 26Jul2020: Added SuppressDiagnostics flag. This is intended mostly for
	// DROP TABLE commands where the table being dropped probably doesn't exist;
	// in this case SQLPrepare will fail, but we don't want to log the error.
	short				NeedToInterpretOp		= 0;
	short				InterpretOutputOnly		= 0;
	short				NeedToPrepare			= 0;
	short				NeedToBindInput			= 0;
	short				NeedToBindOutput		= 0;
	short				NeedToExecute			= 0;
	short				NeedToFetch				= 0;
	short				NeedToCheckCardinality	= 0;
	short				NeedToClose				= 0;
	short				NeedToFreeStatement		= 0;
	short				NeedToCommit			= 0;
	short				NeedToRollBack			= 0;
	short				NeedToSetDirty			= 0;
	short				NeedToSetCommitted		= 0;
	short				NeedToSetRepeatable		= 0;
	short				NeedValidOperationId	= 0;
	short				SuppressDiagnostics		= 0;


	// Variables to store output of SQL_GetMainOperationParameters() and SQL_GetWhereClauseParameters().
	char				*SQL_CommandText;
	long				WhereClauseIdentifier_in;
	int					NumOutputColumns				= 0;
	int					NumInputColumns					= 0;
	int					NumWhereClauseInputColumns		= 0;
	int					NeedsWhereClauseID				= 0;
	int					GenerateVALUES					= 0;
	short				MirrorThisCommand				= 0;	// DonR 21Feb2021: ODBC "mirroring" enhancement.

	int					FoundForUpdate					= 0;
	int					FoundSelect						= 0;
	int					FoundCustomWhereInsertionPoint	= 0;
	ODBC_ColumnParams	*OutputColumns;
	ODBC_ColumnParams	*InputColumns;
	ODBC_ColumnParams	*WhereClauseInputColumns;
	char				*WhereClauseText;
	char				*CursorName;
	char				*ErrorDescription;

	void				*VoidStar;			// Pointer variable to be loaded from va_args and used for binding.
	SQLLEN				BindLength;			// Length for binding output columns.
	int					SQL_ParamType;		// For binding input parameters; set based on parameter's C type.
	short				NumParams;			// Used to validate that a sticky statement is in fact still in PREPAREd state.
	sigset_t			NullSigset;
	char				*BadParameterDesc;


//	// The first time this function is called, do some initialization.
//	if (ODBC_Exec_FirstTimeCalled)
//	{
//		// Allocate statement cache and statement-prepared array based on the total
//		// number of SQL statements we know about - even though most of them aren't
//		// "sticky" and thus most of these array elements won't really be used. This
//		// way we avoid having to search through an array of "sticky" statements to
//		// find the one we're looking to (re-)use.
//		// DonR 08Jul2020: For simpler code, made all these arrays conventional
//		// pre-dimensioned ones rather than pointers to be malloc'ed. Since they're
//		// never re-sized, there's no point in using malloc - and simpler code
//		// is a good thing!
//		memset ((char *)StatementCache,					(char)0, (ODBC_MAXIMUM_OPERATION * sizeof(SQLHSTMT)));
//		memset ((char *)MirrorStatementCache,			(char)0, (ODBC_MAXIMUM_OPERATION * sizeof(SQLHSTMT)));
//		memset ((char *)Statement_DB_Provider,			(char)0, (ODBC_MAXIMUM_OPERATION * sizeof(short)));
//		memset ((char *)StatementPrepared,				(char)0, (ODBC_MAXIMUM_OPERATION * sizeof(short)));
//		memset ((char *)StatementOpened,				(char)0, (ODBC_MAXIMUM_OPERATION * sizeof(short)));
//		memset ((char *)StatementIsSticky,				(char)0, (ODBC_MAXIMUM_OPERATION * sizeof(short)));
//		memset ((char *)Convert_NF_to_zero,				(char)0, (ODBC_MAXIMUM_OPERATION * sizeof(short)));
//		memset ((char *)CommandIsMirrored,				(char)0, (ODBC_MAXIMUM_OPERATION * sizeof(short)));
//		memset ((char *)IsInsertCommand,				(char)0, (ODBC_MAXIMUM_OPERATION * sizeof(short)));
//		memset ((char *)SavedCommandTextPtr,			(char)0, (ODBC_MAXIMUM_OPERATION * sizeof(char *)));
//		memset ((char *)SQL_CmdBuffer,					(char)0, sizeof(SQL_CmdBuffer));
//	
//		ODBC_Exec_FirstTimeCalled			= 0;	// So next time we don't initialize this stuff.
//		NumStickyHandlesUsed				= 0;	// Redundant, but I'm feeling paranoid.
//		ALTERNATE_DB_USED					= 0;
//		MIRROR_SQLCODE						= 0;
//		MIRROR_ROWS_AFFECTED				= 0;
//		ODBC_ActivityCount					= 0L;	// Redundant, but harmless.
//
//		strcpy (ODBC_ErrorBuffer,			"");
//		strcpy (ODBC_LastSQLStatementText,	"");
//	}	// First-execution initializations.
//
	// Initialize the variable argument list.
	va_start (ArgList, LengthReadArrayPtr_in);

	// Initialize column/parameter arrays. Note that these initializations
	// are performed every time ODBC_Exec() is called.
	memset ((char *)FieldLengthsRead,				(char)0, (ODBC_MAX_PARAMETERS * sizeof(SQLLEN)));
	memset ((char *)ColumnParameterPointers,		(char)0, (ODBC_MAX_PARAMETERS * sizeof(void *)));

	// Set the "Avoid Recursion" flag TRUE - so any calls to error-logging routines made from *within*
	// ODBC_Exec will not result in further (recursive) calls to ODBC_Exec.
	ODBC_AvoidRecursion = 1;

	// DonR 16Nov2021: Add initialization of the global rows-affected variable to zero.
	sqlca.sqlerrd[2] = 0;

	// DonR 24May2020: In order to simplify code - and possibly improve reliability - always
	// use the StatementCache element corresponding to OperationIdentifier_in, instead of the
	// previous logic where we used subscript 0 for non-sticky singleton SQL operations. This
	// means that ThisStatementPtr is set only here, unconditionally.
	ThisStatementPtr	= &StatementCache		[OperationIdentifier_in];
	MirrorStatementPtr	= &MirrorStatementCache	[OperationIdentifier_in];


	// Determine what operations we need to perform based on what
	// type of SQL operation type was requested. Just to be paranoid,
	// re-initialize the operation flags to FALSE.
	NeedToInterpretOp		= 0;
	InterpretOutputOnly		= 0;
	NeedToPrepare			= 0;
	NeedToBindInput			= 0;
	NeedToBindOutput		= 0;
	NeedToExecute			= 0;
	NeedToFetch				= 0;
	NeedToCheckCardinality	= 0;
	NeedToClose				= 0;
	NeedToFreeStatement		= 0;
	NeedToCommit			= 0;
	NeedToRollBack			= 0;
	NeedToSetDirty			= 0;
	NeedToSetCommitted		= 0;
	NeedToSetRepeatable		= 0;
	NeedValidOperationId	= 0;
	ReturnCode				= 0;	// Paranoid re-initialization.
	SuppressDiagnostics		= 0;	// Paranoid re-initialization.

	// Now Decide what we're going to have to do in this function call.
	//
	// DonR 18Dec2019: Added three new variations:
	//    A) DeclareDeferredCursor (DEFERRED_INPUT_CURSOR): PREPAREs the
	//       cursor but does *not* bind input parameters.
	//    B) DeclareDeferredCursorInto (DEFERRED_INPUT_CURSOR_INTO):
	//       PREPAREs the cursor and binds output, but does *not* bind
	//       input parameters.
	//    C) OpenCursorUsing (OPEN_CURSOR_USING) *must* be used after
	//       DeclareDeferredCursor or DeclareDeferredCursorInto (assuming
	//       there are input parameters); it binds input parameters before
	//       executing the SELECT.
	// Note that as of 18Dec2019, these variations have *not* been tested.
	//
	// DonR 13Jul2020: Just for convenience, added DECLARE_AND_OPEN_CURSOR
	// and DECLARE_AND_OPEN_CURSOR_INTO options, to enable combining the
	// DECLARE and OPEN operations in a single statement.

	CurrentMacroName = "";	// Paranoid re-initialization.

	switch (CommandType_in)
	{
		case	DECLARE_CURSOR:					NeedToInterpretOp		= 1;
												NeedToPrepare			= 1;
												NeedToBindInput			= 1;
												NeedValidOperationId	= 1;
												CurrentMacroName		= "DeclareCursor";
												break;

		case	DECLARE_CURSOR_INTO:			NeedToInterpretOp		= 1;
												NeedToPrepare			= 1;
												NeedToBindInput			= 1;
												NeedToBindOutput		= 1;
												NeedValidOperationId	= 1;
												CurrentMacroName		= "DeclareCursorInto";
												break;

		case	DECLARE_AND_OPEN_CURSOR:		NeedToInterpretOp		= 1;
												NeedToPrepare			= 1;
												NeedToBindInput			= 1;
												NeedToExecute			= 1;
												NeedValidOperationId	= 1;
												CurrentMacroName		= "DeclareAndOpenCursor";
												break;

		case	DECLARE_AND_OPEN_CURSOR_INTO:	NeedToInterpretOp		= 1;
												NeedToPrepare			= 1;
												NeedToBindInput			= 1;
												NeedToBindOutput		= 1;
												NeedToExecute			= 1;
												NeedValidOperationId	= 1;
												CurrentMacroName		= "DeclareAndOpenCursorInto";
												break;

		case	DEFERRED_INPUT_CURSOR:			NeedToInterpretOp		= 1;
												NeedToPrepare			= 1;
												NeedValidOperationId	= 1;
												CurrentMacroName		= "DeclareDeferredCursor";
												break;

		case	DEFERRED_INPUT_CURSOR_INTO:		NeedToInterpretOp		= 1;
												NeedToPrepare			= 1;
												NeedToBindOutput		= 1;
												NeedValidOperationId	= 1;
												CurrentMacroName		= "DeclareDeferredCursorInto";
												break;

		case	OPEN_CURSOR:					NeedToExecute			= 1;
												NeedValidOperationId	= 1;
												CurrentMacroName		= "OpenCursor";
												break;

		case	OPEN_CURSOR_USING:				NeedToInterpretOp		= 1;
												NeedToBindInput			= 1;
												NeedToExecute			= 1;
												NeedValidOperationId	= 1;
												CurrentMacroName		= "OpenCursorUsing";
												break;

		case	FETCH_CURSOR:					NeedToFetch				= 1;
												NeedValidOperationId	= 1;
												CurrentMacroName		= "FetchCursor";
												break;

		case	FETCH_CURSOR_INTO:				NeedToInterpretOp		= 1;
												InterpretOutputOnly		= 1;
												NeedToBindOutput		= 1;
												NeedToFetch				= 1;
												NeedValidOperationId	= 1;
												CurrentMacroName		= "FetchCursorInto";
												break;

		case	CLOSE_CURSOR:					NeedToClose				= 1;
												NeedValidOperationId	= 1;
												CurrentMacroName		= "CloseCursor";
												break;

		case	FREE_STATEMENT:					NeedToFreeStatement		= 1;
												NeedValidOperationId	= 1;
												CurrentMacroName		= "FreeStatement";
												break;

		case	SINGLETON_SQL_CALL:				NeedToInterpretOp		= 1;
												NeedToPrepare			= 1;
												NeedToBindInput			= 1;
												NeedToBindOutput		= 1;
												NeedToExecute			= 1;
												NeedToFetch				= 1;
												NeedToCheckCardinality	= 1;
												NeedToClose				= 1;
												NeedValidOperationId	= 1;
												CurrentMacroName		= "ExecSQL";
												break;

		case	COMMIT_WORK:					NeedToCommit			= 1;
												CurrentMacroName		= "CommitWork";
												break;

		case	ROLLBACK_WORK:					NeedToRollBack			= 1;
												CurrentMacroName		= "RollbackWork";
												break;

		case	GET_LENGTHS_READ:				// All GET_LENGTHS_READ does is return the address of the internal lengths-read array.
												CurrentMacroName		= "GetLengthsRead";
												break;

		case	SET_DIRTY_READ:					NeedToSetDirty			= 1;
												CurrentMacroName		= "SetDirtyRead";
												break;

		case	SET_COMMITTED_READ:				NeedToSetCommitted		= 1;
												CurrentMacroName		= "SetCommittedRead";
												break;

		case	SET_REPEATABLE_READ:			NeedToSetRepeatable		= 1;
												CurrentMacroName		= "SetRepeatableRead";
												break;

		case	SET_CUSTOM_SIGSEGV_HANDLER:		SIGSEGV_Handler			= va_arg (ArgList, void *);
												CurrentMacroName		= "SetCustomSegmentationFaultHandler";
												break;

												// Add standard error logging!
		default:								GerrLogMini (GerrId,
															 "ODBC_Exec got unrecognized operation type %d for operation %d!",
															 CommandType_in, OperationIdentifier_in);
												CurrentMacroName		= "Unrecognized operation!";
												ODBC_AvoidRecursion = 0;
												return (-1);
	}

	// DonR 25May2020: Add a sanity check for OperationIdentifier_in - it shouldn't really
	// be necessary, but a little paranoia is a good thing!
	if (((OperationIdentifier_in < 1) || (OperationIdentifier_in >= ODBC_MAXIMUM_OPERATION)) && (NeedValidOperationId))
	{
		GerrLogMini (GerrId, "ODBC_Exec got invalid Operation Identifier %d for %s.", OperationIdentifier_in, CurrentMacroName);
		ODBC_AvoidRecursion = 0;
		return (-1);
	}


	// DonR 18May2022: Moved the first-time initializations to *after* we interpret
	// CommandType_in; this lets a calling program use SetCustomSegmentationFaultHandler()
	// to override the default SIGSEGV handler *before* ODBC_Exec() installs it.
	//
	// The first time this function is called, do some initialization.
	if (ODBC_Exec_FirstTimeCalled)
	{
		// Allocate statement cache and statement-prepared array based on the total
		// number of SQL statements we know about - even though most of them aren't
		// "sticky" and thus most of these array elements won't really be used. This
		// way we avoid having to search through an array of "sticky" statements to
		// find the one we're looking to (re-)use.
		// DonR 08Jul2020: For simpler code, made all these arrays conventional
		// pre-dimensioned ones rather than pointers to be malloc'ed. Since they're
		// never re-sized, there's no point in using malloc - and simpler code
		// is a good thing!
		memset ((char *)StatementCache,					(char)0, (ODBC_MAXIMUM_OPERATION * sizeof(SQLHSTMT)));
		memset ((char *)MirrorStatementCache,			(char)0, (ODBC_MAXIMUM_OPERATION * sizeof(SQLHSTMT)));
		memset ((char *)Statement_DB_Provider,			(char)0, (ODBC_MAXIMUM_OPERATION * sizeof(short)));
		memset ((char *)StatementPrepared,				(char)0, (ODBC_MAXIMUM_OPERATION * sizeof(short)));
		memset ((char *)StatementOpened,				(char)0, (ODBC_MAXIMUM_OPERATION * sizeof(short)));
		memset ((char *)StatementIsSticky,				(char)0, (ODBC_MAXIMUM_OPERATION * sizeof(short)));
		memset ((char *)Convert_NF_to_zero,				(char)0, (ODBC_MAXIMUM_OPERATION * sizeof(short)));
		memset ((char *)CommandIsMirrored,				(char)0, (ODBC_MAXIMUM_OPERATION * sizeof(short)));
		memset ((char *)IsInsertCommand,				(char)0, (ODBC_MAXIMUM_OPERATION * sizeof(short)));
		memset ((char *)SavedCommandTextPtr,			(char)0, (ODBC_MAXIMUM_OPERATION * sizeof(char *)));
		memset ((char *)SQL_CmdBuffer,					(char)0, sizeof(SQL_CmdBuffer));
	
		ODBC_Exec_FirstTimeCalled			= 0;	// So next time we don't initialize this stuff.
		NumStickyHandlesUsed				= 0;	// Redundant, but I'm feeling paranoid.
		ALTERNATE_DB_USED					= 0;
		MIRROR_SQLCODE						= 0;
		MIRROR_ROWS_AFFECTED				= 0;
		ODBC_ActivityCount					= 0L;	// Redundant, but harmless.

		strcpy (ODBC_ErrorBuffer,			"");
		strcpy (ODBC_LastSQLStatementText,	"");

		// DonR 18May2022: Instead of relying on the calling mainline to set up and install
		// a segmentation-fault handler, do it here. If the calling program wants to use a
		// custom handler instead of macODBC_SegmentationFaultCatcher(), it can use
		// SetCustomSegmentationFaultHandler() to tell MacODBC to install the custom handler.
		memset ((char *)&NullSigset, 0, sizeof(sigset_t));

		sig_act_ODBC_SegFault.sa_handler	= SIGSEGV_Handler;
		sig_act_ODBC_SegFault.sa_mask		= NullSigset;
		sig_act_ODBC_SegFault.sa_flags		= 0;

		if (sigaction (SIGSEGV, &sig_act_ODBC_SegFault, NULL) != 0)
		{
			GerrLogReturn (	GerrId,
							"ODBC_Exec() couldn't install %s signal handler for SIGSEGV -" GerrErr ".",
							(SIGSEGV_Handler == macODBC_SegmentationFaultCatcher) ? "default" : "custom",
							GerrStr	);
		}
//else GerrLogMini (GerrId, "ODBC_Exec() successfully installed %s SIGSEGV handler.",
//					(SIGSEGV_Handler == macODBC_SegmentationFaultCatcher) ? "default" : "custom");

		// DonR 10Jul2025: Check what size "bool" variables are. The default is that they're regular
		// integers, but the C compiler reserves its right to decide on something different, and the
		// only way to be 100% sure is to call sizeof().
		if (sizeof(bool) == sizeof(int))
			BoolType = SQL_C_SLONG;		// Which is our default.
		else
		if (sizeof(bool) == sizeof(short))
			BoolType = SQL_C_SSHORT;	// Which would make sense, but at the moment the C compiler doesn't agree.
		else
		if (sizeof(bool) == sizeof(long))
			BoolType = SQL_C_SBIGINT;	// Unlikely, but who knows what some strange future compiler might think?
		else
			BoolType = SQL_C_SLONG;		// We should never get to this case - but it's bad form to leave out the final "else"!

	}	// First-execution initializations.


	// DonR 30Jun2020: Keep a counter of pharmacy transactions, just for diagnostics.
	if (OperationIdentifier_in == INS_messages_details)
		ODBC_ActivityCount++;


	// DonR 11Jul2020: If this is a non-sticky operation and it's either a "singleton" SQL call
	// or a DECLARE call, it should *not* already be declared and opened at this point. If it
	// is, print a diagnostic - and if we see that we are in fact getting log messages telling
	// us that an operation is being called without having been properly cleaned up the last
	// time it exectuted, we should either find and fix the problem or else force a CLOSE/FREE
	// before re-executing it.
	// DonR 12Jul2020: I found one cursor that was being DECLARE'd unconditionally, but sometimes
	// was never OPEN'ed, read, or CLOSE'd - and it appears that this may have been causing all
	// manner of problems, including possibly the mysterious un-PREPARing of sticky statements.
	// Accordingly, I'm adding a (recursive) FreeStatement call here, which should correct the
	// problem if it comes up again.
	if ((!StatementIsSticky	[OperationIdentifier_in])													&&
		(NeedToPrepare)																					&&
		((StatementPrepared	[OperationIdentifier_in]) || (StatementOpened [OperationIdentifier_in])))
	{
		GerrLogMini (GerrId, "PID %d: %s %s already prepared - calling FreeStatement().",
					 (int)getpid(), CurrentMacroName, ODBC_Operation_Name [OperationIdentifier_in]);

		FreeStatement (Database, OperationIdentifier_in);
	}

	// DonR 28Jun2020: Give an initial value to ODBC_LastSQLStatementText, so we'll have something
	// meaningful there even if we're not PREPARing anything. Note that we do *not* want to do this
	// for READ_GetCurrentDatabaseTime, since that's used to get the timestamp for errors caused
	// by other operations and we don't want to lose the original value!
	if (OperationIdentifier_in != READ_GetCurrentDatabaseTime)
	{
		sprintf (ODBC_LastSQLStatementText, "%s %s", CurrentMacroName, ODBC_Operation_Name [OperationIdentifier_in]);
	}


	// Master dummy loop to facilitate error reporting - once we've hit a fatal
	// error, we don't want to keep processing.
	do
	{
		// If we're dealing with something simple like a Fetch (with INTO predefined)
		// ora Close, there's no need to look at the operation parameters.
		if (NeedToInterpretOp)
		{
			// Interpret the SQL operation requested.
			// Note that even if this is a "sticky" statement that we've already
			// prepared, we still have to go through this to get to the right
			// place in our variable argument list and get the right input/output
			// variables properly bound!
			result = SQL_GetMainOperationParameters (	Database,
														OperationIdentifier_in,
														&SQL_CommandText,
														&NumOutputColumns,
														&OutputColumns,
														&NumInputColumns,
														&InputColumns,
														&NeedsWhereClauseID,
														&GenerateVALUES,
														&StatementIsSticky	[OperationIdentifier_in],
														&Convert_NF_to_zero	[OperationIdentifier_in],
														&MirrorThisCommand,
														&SuppressDiagnostics,
														&CursorName,
														&ErrorDescription	);

			// Enable mirroring for this SQL command only if mirroring is enabled globally - it
			// needs to be set TRUE with the environment variable ODBC_MIRRORING_ENABLED *and*
			// we need to have two different connected databases.
			CommandIsMirrored [OperationIdentifier_in] = (ODBC_MIRRORING_ENABLED) ? MirrorThisCommand : 0;

			// DonR 27Jan2020: Save the SQL command-text pointer. This is needed only because
			// we give the option for a calling routine to supply the SQL command text if it
			// needs to be built dynamically. Subsequent calls (like FetchCursorInto) may or
			// may not include the "extra" argument with this command text; if they do, we
			// need to be able to recognize it so we can automatically skip past it.
			if (SQL_CommandText != NULL)
			{
				SavedCommandTextPtr [OperationIdentifier_in] = SQL_CommandText;
			}

			// For now, no sophisticated error handling. A negative return code should mean
			// something like an invalid Operation Identifier - i.e. a program bug, not a
			// database issue.
			if (result < 0)
			{
				// Add standard error logging!
				GerrLogMini (GerrId, "SQL_GetMainOperationParameters for %s %s returned error %d, err text\n%s\n",
							 CurrentMacroName, ODBC_Operation_Name [OperationIdentifier_in], result, ErrorDescription);
				ODBC_AvoidRecursion = 0;
				return (result);
			}

			// If the operation specification in MacODBC_MyOperators.c gives SQL_CommandText ==  NULL,
			// take the SQL command text from the variable-argument list. This is used when a complex
			// SQL command has to be defined dynamically - e.g. where WHERE...IN() lists require a
			// number of possible values that cannot be known in advance.
			// DonR 27Jan2020: If the current operation is a FETCH or some other operation performed
			// *after* an initial PREPARE, don't worry about retrieving the SQL command text again.
			// (Of course, this means that if you *do* provide the SQL command text again, it'll be
			// interpreted as a parameter to BIND!)
			// DonR 27Jan2020: As noted above, we also need to save the externally-prepared SQL command
			// buffer "on the side", so we can skip past it if it's included in later calls like
			// FetchCursorInto (which may or may not include it).
			if ((SQL_CommandText == NULL) && (NeedToPrepare))
			{
				SavedCommandTextPtr [OperationIdentifier_in] = SQL_CommandText = va_arg (ArgList, void *);
//GerrLogMini (GerrId, "Default SQL command is NULL - using \n%s\n supplied by calling routine.", SQL_CommandText);

				if (SQL_CommandText == END_OF_ARG_LIST)
				{
					GerrLogMini (GerrId, "Premature end of argument list for operation %s %s - expected SQL command text.",
								 CurrentMacroName, ODBC_Operation_Name [OperationIdentifier_in]);
					ODBC_AvoidRecursion = 0;
					return (ODBC_SQLCODE = SQLCODE = -1);
				}
			}

			// DonR 02Feb2020: Before automatically adding VALUES or FOR UPDATE clauses
			// to the SQL command string, make sure they're actually appropriate. Also
			// check to see if there's a "%s" in the command - if not, we'll force
			// NeedsWhereClauseID false.
			// DonR 18May2022: Load the "found INSERT" result into a static array, so we
			// remember which SQL commands are INSERTs. This is useful because when we
			// execute the statement, we want to detect MS-SQL "silent INSERT failures" -
			// and to do that, we need to know when we're INSERTing.
			find_FOR_UPDATE_or_GEN_VALUES (	OperationIdentifier_in,
											SQL_CommandText,
											&FoundForUpdate,
											&IsInsertCommand [OperationIdentifier_in],
											&FoundSelect,
											&FoundCustomWhereInsertionPoint	);

			if ((!FoundCustomWhereInsertionPoint) && (NeedsWhereClauseID))
			{
				GerrLogMini (GerrId, "Suppressed erroneous NeedsWhereClauseID for %s %s - there's no insertion point!",
							 CurrentMacroName, ODBC_Operation_Name [OperationIdentifier_in]);
				NeedsWhereClauseID = 0;
			}

			// DonR 21Feb2021: SQL command "mirroring" should apply only to write operations:
			// INSERT, UPDATE, DELETE, and possibly things like CREATE/DROP TABLE. If the
			// current command is a SELECT, force CommandIsMirrored FALSE even if it was
			// set TRUE in MacODBC_MyOperators. If the current operation is *not* a SELECT
			// and mirroring is enabled, set the Alternate Database Used flag TRUE.
			if (FoundSelect)
			{
				CommandIsMirrored [OperationIdentifier_in] = 0;
			}
			else
			{
				// Whichever database the calling routine specified, the database to mirror
				// is the other one.
				MIRROR_DB = (Database == MAIN_DB) ? ALT_DB : MAIN_DB;
			}

			// If the request specified requires a "custom" WHERE clause, that should be
			// the first argument on the variable-argument list.
			// DonR 25Dec2019: Added InterpretOutputOnly control variable, set TRUE for
			// FetchCursorInto commands. This is intended to let this function work
			// properly for SQL operations with a "custom" WHERE clause whether or not
			// the FetchCursorInto command included the WHERE clause identifier. The
			// idea is that in this case we skip reading the WHERE clause identifier,
			// and then when we read the first pointer to bind an output variable, we'll
			// check whether what we got was really a pointer, or just an integer that
			// would be a valid WHERE clause ID. If the latter, we'll just pull the next
			// value off the va_arg list. Simple, assuming that we don't have problems
			// dealing with va_arg values of different sizes!
			// NOTE: At least for now, do NOT assume that custom WHERE clauses and
			// externally-built dynamic SQL commands will work perfectly together!
			if ((NeedsWhereClauseID) && (!InterpretOutputOnly))
			{
				WhereClauseIdentifier_in = va_arg (ArgList, long);
				if (WhereClauseIdentifier_in == (long)END_OF_ARG_LIST)
				{
					GerrLogMini (GerrId, "Premature end of argument list for %s %s - expected custom WHERE clause identifier.",
								 CurrentMacroName, ODBC_Operation_Name [OperationIdentifier_in]);
					ODBC_AvoidRecursion = 0;
					return (ODBC_SQLCODE = SQLCODE = -1);
				}

				result = SQL_GetWhereClauseParameters	(	WhereClauseIdentifier_in,
															&WhereClauseText,
															&NumWhereClauseInputColumns,
															&WhereClauseInputColumns,
															&ErrorDescription				);

				// For now, no sophisticated error handling. A negative return code should mean
				// something like an invalid Operation Identifier - i.e. a program bug, not a
				// database issue.
				if (result < 0)
				{
					GerrLogMini (GerrId, "SQL_GetWhereClauseParameters returned error %d for %s %s.",
								 result, CurrentMacroName, ODBC_Operation_Name [OperationIdentifier_in]);
					ODBC_AvoidRecursion = 0;
					return (result);
				}


				// If the custom WHERE clause in MacODBC_MyCustomWhereClauses.c gives
				// SQL_CommandText ==  NULL, take the WHERE clause text from the variable-argument
				// list. This can be used when a complex SQL command has to be defined dynamically -
				// e.g. where WHERE...IN() lists require a number of possible values that cannot be
				// known in advance.
				if (WhereClauseText == NULL)
				{
					WhereClauseText = va_arg (ArgList, void *);
//GerrLogMini (GerrId, "Default WHERE clause text is NULL - using \n%s\n supplied by calling routine.", WhereClauseText);
					if (WhereClauseText == END_OF_ARG_LIST)
					{
						GerrLogMini (GerrId, "Premature end of argument list for %s %s - expected WHERE clause text.",
									 CurrentMacroName, ODBC_Operation_Name [OperationIdentifier_in]);
						ODBC_AvoidRecursion = 0;
						return (ODBC_SQLCODE = SQLCODE = -1);
					}
				}
	
			}	// SQL command requires a "custom" WHERE clause.
			else
			{
				NumWhereClauseInputColumns = 0;	// Pure paranoia.
			}

		}	// We need to interpret the operation requested.


		// If this statement was designated as "sticky", we want to prepare it only
		// the first time, to minimize network traffic and work for the DB server.
		// Input/output binding is still done "on the fly", since (A) it doesn't
		// involve network traffic or the DB server, and (B) it's dangerous to
		// assume old bindings are still valid. For the moment, I've defined
		// ODBC_MAX_STICKY_STATEMENTS as 100; I have no idea if any particular
		// database driver supports that many open statements. We could probably
		// survive quite happily with far fewer if we had to.
		// Note that if we already know we don't need to prepare the statement
		// (e.g. for a Fetch or a Close), we need to find the correct statement
		// handle to use but other than that no statement-preparation is necessary.
		if (NeedToPrepare)
		{
			do
			{
				// In real situations, it's unlikely that anyone would want to perform
				// the same SQL operation against two different databases - although
				// it might come up as part of a switchover from one database to
				// another. In any case, it's a good idea to provide automatic support
				// for this even if it'll seldom be used.
				if (Database->Provider != Statement_DB_Provider[OperationIdentifier_in])
				{
					// If this statement was already used for another provider, free it
					// and clear up all associated flags so we start fresh.
					if (Statement_DB_Provider[OperationIdentifier_in] > ODBC_NoProvider)
					{
						// This SQLFreeHandle call may well fail - it's not especially important if it does.
						// DonR 03May2020: Stop using UseStatementSubscript[] - just do the Free Handle for
						// the non-zero array element, since an extra Free Handle won't (or shouldn't) do any
						// damage. If the statement was a non-sticky singleton call, it should already have
						// been freed after the last time it execute.
						// DonR 21Feb2021: Also free the Mirror Statement Cache if applicable.
						if (CommandIsMirrored [OperationIdentifier_in])
						{
							result = SQLFreeHandle (SQL_HANDLE_STMT, MirrorStatementCache [OperationIdentifier_in]);
						}

						result = SQLFreeHandle (SQL_HANDLE_STMT, StatementCache [OperationIdentifier_in]);
GerrLogMini (GerrId, "Switch DB Provider for %s %s from %d to %d: SQLFreeHandle returns %d.",
			 CurrentMacroName, ODBC_Operation_Name [OperationIdentifier_in],
			 Statement_DB_Provider[OperationIdentifier_in], Database->Provider, result);

						// DonR 29Oct2020: Any time we un-prepare a sticky statement, we also have to
						// decrement the Sticky Handles Used counter.
						if ((StatementIsSticky [OperationIdentifier_in]) && (StatementPrepared	[OperationIdentifier_in]))
							NumStickyHandlesUsed--;

						// Reset StatementPrepared so we know we need to set this statement up again next time.
						// DonR 29Mar2020: Use simple subscript rather than UseStatementSubscript[].
						StatementPrepared	[OperationIdentifier_in] = 0;
						StatementOpened		[OperationIdentifier_in] = 0;
					}	// The previous DB provider for this statement is an actual database.
			
					// And now we store the new DB provider.
					Statement_DB_Provider[OperationIdentifier_in] = Database->Provider;
				}	// The current DB provider for this statement is not the same as the
					// previous one (which may be a real provider or no provider).


				// The current statement is one that normally requires preparation -
				// but it may be a "sticky" statement that has already been prepared.

				// There is finite capacity to keep open statements; so if a statement
				// has been designated "sticky" but the maximum number of "sticky"
				// statements has already been reached, turn off its "stickiness" and
				// create a diagnostic.
				if ((StatementIsSticky [OperationIdentifier_in]) && (!StatementPrepared [OperationIdentifier_in]))
				{
//GerrLogFnameMini ("sticky_log", GerrId, "PID %d: %s %s is sticky and unPrepared.  NumStickyHandlesUsed = %d, max %d",
//	(int)getpid(), CurrentMacroName, ODBC_Operation_Name [OperationIdentifier_in], NumStickyHandlesUsed, ODBC_MAX_STICKY_STATEMENTS);
					if (NumStickyHandlesUsed >= ODBC_MAX_STICKY_STATEMENTS)
					{
						StatementIsSticky [OperationIdentifier_in] = 0;

						// This should probably be replaced by some more sophisticated error handling.
						// Add standard error logging (although this is NOT fatal)!
						GerrLogMini (GerrId, "Rejected sticky statement %s %s because %d statements are already sticky.",
									 CurrentMacroName, ODBC_Operation_Name [OperationIdentifier_in], NumStickyHandlesUsed);
//GerrLogFnameMini ("sticky_log", GerrId, "Rejected sticky statement %s %s because %d statements are already sticky.",
//									 CurrentMacroName, ODBC_Operation_Name [OperationIdentifier_in], NumStickyHandlesUsed);
					}
					else
					{
						NumStickyHandlesUsed++;
//GerrLogFnameMini ("sticky_log", GerrId, "Incremented NumStickyHandlesUsed to %d.", NumStickyHandlesUsed);
					}
				}	// "Sticky" statement hasn't yet been prepared, so check if we've reached Peak Stickiness.


				// If the statement is sticky and has already been prepared, we're done
				// with the preparation section - it's already prepared and ready for use.
				if ((StatementIsSticky [OperationIdentifier_in])	&&
					(StatementPrepared [OperationIdentifier_in]))
				{
					// DonR 07May2020: At times, we've seen sticky statements fail the second time around -
					// somehow they got either freed or corrupted. Until and unless I can figure out what
					// causes this and prevent it from happening, add a validation step to ensure that we
					// will re-do the PREPARE if a sticky statement is not in fact still accessible.
					// DonR 14Jul2020: Added #define'd parameter ENABLE_PREPARED_STATE_VALIDATION
					// to enable/disable this check, since it appears that the bug that was
					// causing loss-of-PREPARation problems has been fixed. Disabling the check
					// should improve performance - although it's not clear whether the improvement
					// will be big enough to be measurable. (Note that to keep the code simple, I didn't
					// change the program flow - I just force a "successful" result of 0 instead of
					// actually calling SQLNumParams.)
					// DonR 21Feb2021: Note that at least for now, we're performing this test *only* on the
					// primary database for the current operation - *not* on the mirrored database.
					result = (ENABLE_PREPARED_STATE_VALIDATION) ? SQLNumParams (*ThisStatementPtr, &NumParams) : 0;

					if (SQL_FAILED (result))
					{
						// DonR 30Jun2020: Don't bother with the diagnostic when the program is
						// first getting "warmed up". The threshold can be increased as we get
						// more confident - but should be lowered again for MS-SQL testing!
						if (ODBC_ActivityCount > PrintDiagnosticsAfter)
						{
							GerrLogMini (GerrId, "PID %d re-PREPARE %s %s after %ld transactions.",
										 (int)getpid (), CurrentMacroName, ODBC_Operation_Name [OperationIdentifier_in], ODBC_ActivityCount);
						}

						result = SQLFreeHandle (SQL_HANDLE_STMT, *ThisStatementPtr);

						StatementPrepared	[OperationIdentifier_in] = 0;
						StatementOpened		[OperationIdentifier_in] = 0;

						// DonR 29Oct2020: Any time we un-prepare a sticky statement, we also have to
						// decrement the Sticky Handles Used counter.
						NumStickyHandlesUsed--;
					}
					else	// SQLNumParams succeeded, so we can go ahead and re-use the sticky statement. 
					{
						break;	// Skip the rest of the PREPARation steps.
					}
				}	// "Sticky" statement should already be prepared and ready for binding.

				// Finally, it's time to actually do something!

				// Now allocate the statement.
				result = SQLAllocStmt (Database->HDBC, ThisStatementPtr);
				if (SQL_FAILED (result))
				{
					GerrLogMini (GerrId, "PID %d: SQLAllocStmt for %s %s failed, returning %d.",
								 getpid(), CurrentMacroName, ODBC_Operation_Name [OperationIdentifier_in], result);
					break;
				}

				// DonR 21Feb2021: Add statement allocation for alternate-database mirroring.
				if (CommandIsMirrored [OperationIdentifier_in])
				{
					result = SQLAllocStmt (MIRROR_DB->HDBC, MirrorStatementPtr);
					if (SQL_FAILED (result))
					{
						GerrLogMini (GerrId, "SQLAllocStmt for mirroring %s %s failed, returning %d.",
									 CurrentMacroName, ODBC_Operation_Name [OperationIdentifier_in], result);
						break;
					}
				}

//				// DonR 21Jun2020: For Informix, try setting AUTOFREE off for sticky statements.
//				// (Note that this doesn't seem to actually accomplish anything. Hmph.)
//				if ((Database->Provider == ODBC_Informix) && (StatementIsSticky [OperationIdentifier_in]))
//				{
//					result = SQLSetStmtAttr (*ThisStatementPtr, SQL_INFX_ATTR_AUTO_FREE, (SQLPOINTER)SQL_FALSE, SQL_IS_USMALLINT);
//					if (SQL_FAILED (result))
//					{
//						GerrLogMini (GerrId, "Disable AUTOFREE for %s: result = %d.", ODBC_Operation_Name [OperationIdentifier_in], result);
//					}
//				}
//
				// Now build the SQL command string. This should consist of 1-3 elements:
				//
				// 1) The basic command.
				//
				// 2) Optionally, a variable WHERE clause, in cases where we want to perform
				//    the same SELECT, UPDATE, or DELETE with different sets of WHERE criteria.
				//    In this case, there must be a " %s " in the basic command where the WHERE
				//    clause should go; it's best if the keyword "WHERE" itself is part of the
				//    main command, and the variable portion is just the actual criteria. The
				//    code, however, does not enforce this - the variable WHERE stuff can include
				//    the "WHERE" keyword, and alternatively constitute only a subset of the
				//    desired selection criteria, with the rest left in the basic command.
				//
				// 3) Optionally, a constructed VALUES clause - since this is basically just
				//    a bunch of question marks and commas, it's easier to have the function
				//    build it based on the number of input parameters needed and not have to
				//    worry about counting  question marks. Since VALUES always goes at the
				//    end of an INSERT, there's no need for a "%s" in this case.
				//
				// NOTE: At least for the moment, mirroring assumes that the SQL command string
				// does *not* include any database-specific code, so that the exact same text
				// can be PREPAREd for either database with equal success. If we need to mirror
				// an operation that *does* involve different SQL code for different databases,
				// the mirroring will have to be done in application code using different SQL
				// operations.
				if (NeedsWhereClauseID)
				{
					CmdLen = sprintf (SQL_CmdBuffer, SQL_CommandText, WhereClauseText);
				}
				else
				{
					strcpy (SQL_CmdBuffer, SQL_CommandText);
					CmdLen = strlen (SQL_CmdBuffer);
				}

				// Note that at least for now, VALUES generation assumes that there is at least
				// one input parameter - an INSERT with Generate Values set TRUE and no input
				// parameters doesn't make a whole hell of a lot of sense. It would be easy
				// enough to add the condition "NumInputColumns > 0" to the "if" below...
				// DonR 02Feb2020: Generate a VALUES clause only if this is really an INSERT!
				if ((GenerateVALUES) && (IsInsertCommand [OperationIdentifier_in]))
				{
					CmdLen += sprintf (SQL_CmdBuffer + CmdLen, " VALUES ( ");

					for (i = 0; i < NumInputColumns; i++)
					{
						// Print a leading comma for everything after the first iteration.
						CmdLen += sprintf (SQL_CmdBuffer + CmdLen, "%s?", (i > 0) ? "," : " ");
					}

					CmdLen += sprintf (SQL_CmdBuffer + CmdLen, " ) ");
				}

				// Before preparing the statement, insert any required provider-specific terms.
				// For example, the SQL_CustomizePerDB routine will look for either %FIRST% or
				// %TOP% (case insensitive) and replace the token with the DB-appropriate term.
				SQL_CustomizePerDB (Database, SQL_CmdBuffer, &CmdLen);

				// At this point, we should have a well-formed SQL command string ready for
				// preparation and binding - kind of a software akeidah.

				// If we need a cursor in "FOR UPDATE" mode, this is where to set it updatable.
				// We don't need a separate indicator variable for this, since the only reason
				// to give a cursor a name is to make it updatable (and making it updatable
				// without assigning a name wouldn't work anyway).
				// DonR 02Feb2020: Cursor Name is relevant only for SELECT statements.
				// DonR 10Mar2021: Note that even if we aren't using a cursor to perform any
				// updates, it needs to have a cursor name if it's going to retain its state
				// through intermediate COMMITs. Otherwise it may get closed or reset back to
				// the first row selected - *not* what we normally want!
				if ((CursorName != NULL) && (FoundSelect))
				{
					result = SQLSetStmtAttr (*ThisStatementPtr, SQL_ATTR_CONCURRENCY, (SQLPOINTER)SQL_CONCUR_LOCK, SQL_IS_INTEGER);

					if (SQL_FAILED (result))
					{
						ReturnCode = ODBC_StatementError (ThisStatementPtr, OperationIdentifier_in);
						GerrLogMini (GerrId, "PID %d: SQLSetStmtAttr (set cursor updatable) failed for %s %s, result = %d.",
									 getpid(), CurrentMacroName, ODBC_Operation_Name [OperationIdentifier_in], result);
						break;
					}

					// DonR 26Jan2020: At least in Informix, the DB server wants a "FOR UPDATE"
					// even if we've run a SQLSetStmtAttr call. Rather than (God forbid) go
					// through all my SQL commands and add "FOR UPDATE" where necessary, let's
					// make it automatic!
					// DonR 02Feb2020: The call to check for a FOR UPDATE clause has been moved
					// up a bit, since it now also checks whether the SQL command is an INSERT.
					if (!FoundForUpdate)
					{
						CmdLen += sprintf (SQL_CmdBuffer + CmdLen, " FOR UPDATE ");
					}
				}	// CursorName != NULL

				// WORKINGPOINT: Any other SQLSetStmtAttr() commands should go here.


				// Now go ahead and PREPARE the SQL statement.
				// DonR 12Feb2020: Since error-logging routines use READ_GetCurrentDatabaseTime to get a common
				// timestamp, don't copy it into ODBC_LastSQLStatementText.
				// DonR 22Jun2020: Instead of just copying the char * value, do an actual strcpy. This should
				// make life simpler, since it means we'll always have a copy of the last SQL we PREPAREd.
				// This also gives us the option of playing with the SQL text before writing it to error logs,
				// if we want - for example, we could substitute spaces for tabs or do other formatting.
				if (OperationIdentifier_in != READ_GetCurrentDatabaseTime)
				{
					sprintf (ODBC_LastSQLStatementText, "%s %s:\n%s",
							 CurrentMacroName, ODBC_Operation_Name [OperationIdentifier_in], SQL_CmdBuffer);
				}

				// DonR 29Mar2020: Use simple statement pointer rather than UseStatementSubscript[].
				result = SQLPrepare (*ThisStatementPtr, SQL_CmdBuffer, CmdLen);

				if (SQL_FAILED (result))
				{
					ReturnCode = ODBC_StatementError (ThisStatementPtr, OperationIdentifier_in);

					if (!SuppressDiagnostics)
					{
						// DonR 12Sep2021 resiliency enhancement:
						// We should really never get errors on PREPARE in Production - even when I deliberately
						// used SQL with syntax errors, bad table names, and so on, I found that the errors were
						// reported at EXECUTE time rather than at PREPARE time. Accordingly, we should see errors
						// here only when there is some kind of network or database server problem. In order to enable
						// automatic reconnection to the database, we need to re-initialize everything when this
						// happens, and force a connection-broken error. (Note that in future, if we see error codes
						// here that are *not* the result of infrastructure problems, we should deal with them without
						// re-initializing the database connection.) Because there are all kinds of errors at EXECUTE
						// time, it's best to detect a broken database connection here, where we don't normally see
						// any errors.

						// In order to avoid going to the database for the log-message timestamp, we need to
						// disconnect from the database *before* logging anything.
						SQLMD_disconnect ();

						// Reset the local first-time-called variable, so that after we reconnect we'll start with
						// a clean slate. (OK, we'll start with clean arrays, counters, etc. - no actual slate is
						// involved! <g>)
						ODBC_Exec_FirstTimeCalled = 1;

						GerrLogMini (GerrId,	"\nPID %d: PREPARE %s\nfor %s %s failed, ReturnCode = %d.\nError = %s.\n"
												"Closing database connection(s) and returning DB_CONNECTION_BROKEN.",
									 getpid(), SQL_CmdBuffer, CurrentMacroName, ODBC_Operation_Name [OperationIdentifier_in],
									 ReturnCode, sqlca.sqlerrm);

						// Force in a connection-broken error code, which will trigger automatic database reconnect
						// the next time a database operation is attempted.
						ReturnCode = DB_CONNECTION_BROKEN;
					}

					break;
				}
				else
				{
					StatementPrepared		[OperationIdentifier_in] = 1;	// Success!
				}

				// DonR 21Feb2021: PREPARE the mirrored version of the statement as well.
				if (CommandIsMirrored [OperationIdentifier_in])
				{
					result = SQLPrepare (*MirrorStatementPtr, SQL_CmdBuffer, CmdLen);

					if (SQL_FAILED (result))
					{
						if (!SuppressDiagnostics)
						{
							GerrLogMini (GerrId, "PREPARE %s\nfor mirrored %s %s failed, result = %d!",
										 SQL_CmdBuffer, CurrentMacroName, ODBC_Operation_Name [OperationIdentifier_in], result);
						}

						ReturnCode = ODBC_StatementError (MirrorStatementPtr, OperationIdentifier_in);
						break;
					}
				}	// Statement preparation for mirrored SQL operation.

				// If this operation specifies a non-NULL cursor name (which is needed for
				// UPDATE/DELETE operations with WHERE CURRENT OF xxx), set the name now.
				if (CursorName != NULL)
				{
					result = SQLSetCursorName (*ThisStatementPtr, CursorName, SQL_NTS);
//GerrLogMini (GerrId, "Set cursor name to %s - result = %d.", CursorName, result);

					if (SQL_FAILED (result))
					{
						ReturnCode = ODBC_StatementError (ThisStatementPtr, OperationIdentifier_in);;
						GerrLogMini (GerrId, "SQLSetCursorName failed for %s %s, result = %d.",
									 CurrentMacroName, ODBC_Operation_Name [OperationIdentifier_in], result);
						break;
					}
				}

			} while (0);
		}	// NeedToPrepare is TRUE.

		// Since the NeedtoPrepare stuff is within its own do-while "loop", we need extra
		// logic to break out of the main "loop" if we hit an error.
		if (ReturnCode)
		{
//			if (!SuppressDiagnostics)
//			{
//				GerrLogMini (GerrId, "\nPID %d: After PREPARE section, ReturnCode = %d - breaking out of do-loop.", (int)getpid(), ReturnCode);
//			}
			break;
		}


		// DonR 12Feb2020: Instead of pulling arguments directly from va_args and immediately binding
		// them, load them all into a buffer first. This way we can make at least some attempt to
		// ensure that the pointers are valid (e.g. no missing ampersands) *before* we do anything
		// with them - and thus avoid segmentation errors.
		ColumnParameterCount	= 0;	// Paranoid re-initialization.
		CaughtParameterProblem	= 0;	// Yet more paranoia.
		ODBC_ValidatingPointers	= 1;	// Until we turn this OFF, segmentation faults will be caught and
										// used to detect bad column/parameter pointers - but will *not*
										// crash the program.

		if (NeedToBindOutput)
		{
			for (i = 0; i < NumOutputColumns; i++)
			{
				VoidStar = va_arg (ArgList, void *);

				if (VoidStar == END_OF_ARG_LIST)
				{
					sprintf (sqlca.sqlerrm, "Premature end of argument list for %s %s - expected output column %d.",
							 CurrentMacroName, ODBC_Operation_Name [OperationIdentifier_in], i + 1);
					ReturnCode = result = -1;	// Force "syntax" error.
					CaughtParameterProblem = 1;
					break;
				}

				// Skip past optional parameters like Custom WHERE Clause ID and dynamically-created
				// SQL command text. We want to do this only at the beginning of the column list, and
				// only when these parameters were not in fact required. (If they were, we should have
				// already taken them from va_args!)
				if ((i == 0) && (InterpretOutputOnly))
				{
					// DonR 25Dec2019: If the first "pointer" we pulled off the va_arg list looks like a
					// custom WHERE clause ID, *and* we're performing an operation (i.e. FetchCursorInto)
					// where that parameter is optional, skip past it and pull the next argument off the
					// va_arg list. No actual address should be a number between 1 and 35 or so!
					if (((long)VoidStar > 0) && ((long)VoidStar < ODBC_MAXIMUM_WHERE_CLAUSE))
					{
						WhereClauseIdentifier_in = (long)VoidStar;	// Just for nicer diagnostic messages.
// GerrLogMini (GerrId, "Skipping past apparent custom WHERE clause ID %ld.", WhereClauseIdentifier_in);
						VoidStar = va_arg (ArgList, void *);
						if (VoidStar == END_OF_ARG_LIST)
						{
							sprintf (sqlca.sqlerrm, "Premature end of argument list for %s %s - expected output column %d after WHERE clause ID %ld.",
									 CurrentMacroName, ODBC_Operation_Name [OperationIdentifier_in], i + 1, WhereClauseIdentifier_in);
							ReturnCode = result = -1;	// Force "syntax" error.
							CaughtParameterProblem = 1;
							break;
						}
//GerrLogMini (GerrId, "Replacement first output pointer = %ld.", (long)VoidStar);
					}

					// DonR 27Jan2020: Another similar gimmick: When the calling routine has supplied
					// a dynamically-created SQL command instead of using a "canned" one, subsequent
					// calls (like FetchCursorInto) may or may not include the custom SQL command as
					// a parameter. If it was supplied, we want to skip past it automatically.
					// NOT YET TESTED!!!
					if ((char *)VoidStar == SavedCommandTextPtr [OperationIdentifier_in])
					{
//GerrLogMini (GerrId, "%s %s: Skipping past apparent custom dynamic SQL command text.", CurrentMacroName, ODBC_Operation_Name [OperationIdentifier_in]);
						VoidStar = va_arg (ArgList, void *);
						if (VoidStar == END_OF_ARG_LIST)
						{
							sprintf (sqlca.sqlerrm, "Premature end of argument list for %s %s - expected output column %d after dynamic SQL command text.",
									 CurrentMacroName, ODBC_Operation_Name [OperationIdentifier_in], i + 1);
							ReturnCode = result = -1;	// Force "syntax" error.
							CaughtParameterProblem = 1;
							break;
						}
//GerrLogMini (GerrId, "Replacement first output pointer = %ld.", (long)VoidStar);
					}
				}	// First column parameter and we're in interpret-output-only mode.

				// Check that the output pointer is actually valid, to avoid program
				// hang-ups and crashes.
				if (!ODBC_IsValidPointer (VoidStar, CHECK_POINTER_IS_WRITABLE, OutputColumns[i].type, &BadParameterDesc))
				{
					sprintf (sqlca.sqlerrm, "Output parameter %d for %s %s is invalid - %s!",
							 i + 1, CurrentMacroName, ODBC_Operation_Name [OperationIdentifier_in], BadParameterDesc);
					ReturnCode = result = -1;	// Force "syntax" error.
					CaughtParameterProblem = 1;
					break;
				}

				// If we get here, we expect to have a valid output column pointer - so store it.
				ColumnParameterPointers [ColumnParameterCount++] = VoidStar;

			}	// Loop through output columns and store them in ColumnParameterPointers array.

			if (CaughtParameterProblem)
			{
				GerrLogMini (GerrId, "%s", sqlca.sqlerrm);
			}
		}	// This operation type normally requires binding output columns.


		if ((NeedToBindInput) && (!CaughtParameterProblem))
		{
			for (i = 0; i < NumInputColumns; i++)
			{
				VoidStar = va_arg (ArgList, void *);
				if (VoidStar == END_OF_ARG_LIST)
				{
					sprintf (sqlca.sqlerrm, "Premature end of argument list for %s %s - expected input column %d.",
							 CurrentMacroName, ODBC_Operation_Name [OperationIdentifier_in], i + 1);
					ReturnCode = result = -1;	// Force "syntax" error.
					CaughtParameterProblem = 1;
					break;
				}

				// Test pointer for validity by reading a byte and writing it back.
				if (!ODBC_IsValidPointer (VoidStar, CHECK_VALIDITY_ONLY, InputColumns[i].type, &BadParameterDesc))
				{
					sprintf (sqlca.sqlerrm, "Input parameter %d for %s %s is invalid - %s!",
							 i + 1, CurrentMacroName, ODBC_Operation_Name [OperationIdentifier_in], BadParameterDesc);
					ReturnCode = result = -1;	// Force "syntax" error.
					CaughtParameterProblem = 1;
					break;
				}

				// If we get here, we expect to have a valid input column pointer - so store it.
				ColumnParameterPointers [ColumnParameterCount++] = VoidStar;
			}	// Loop through input parameters and store them in ColumnParameterPointers array.

			if (CaughtParameterProblem)
			{
				GerrLogMini (GerrId, "%s", sqlca.sqlerrm);
			}
		}	// This operation type normally requires binding input parameters.


		if ((NeedsWhereClauseID) && (NeedToBindInput) && (NumWhereClauseInputColumns > 0) && (!CaughtParameterProblem))
		{
			for (i = 0; i < NumWhereClauseInputColumns; i++)
			{
				VoidStar = va_arg (ArgList, void *);
				if (VoidStar == END_OF_ARG_LIST)
				{
					sprintf (sqlca.sqlerrm, "Premature end of argument list for %s %s - expected custom WHERE clause %ld column %d.",
							 CurrentMacroName, ODBC_Operation_Name [OperationIdentifier_in], (long)WhereClauseIdentifier_in, i + 1);
					ReturnCode = result = -1;	// Force "syntax" error.
					CaughtParameterProblem = 1;
					break;
				}

				// Test pointer for validity by reading a byte and writing it back.
				if (!ODBC_IsValidPointer (VoidStar, CHECK_VALIDITY_ONLY, WhereClauseInputColumns[i].type, &BadParameterDesc))
				{
					sprintf (sqlca.sqlerrm, "Custom WHERE clause parameter %d for %s %s WHERE clause ID %d is invalid - %s!",
							 i + 1, CurrentMacroName, ODBC_Operation_Name [OperationIdentifier_in],
							 WhereClauseIdentifier_in, BadParameterDesc);
					ReturnCode = result = -1;	// Force "syntax" error.
					CaughtParameterProblem = 1;
					break;
				}

				// If we get here, we expect to have a valid input column pointer - so store it.
				ColumnParameterPointers [ColumnParameterCount++] = VoidStar;
			}	// Loop through custom WHERE clause input parameters and store them in ColumnParameterPointers array.

			if (CaughtParameterProblem)
			{
				GerrLogMini (GerrId, "%s", sqlca.sqlerrm);
			}
		}	// This operation type requires binding custom WHERE clause parameters.

		// Reset the switch so that any further segmentation-fault errors will crash the program, as
		// we would normally expect them to do.
#if 1
		ODBC_ValidatingPointers = 0;
#else
		// Alternative NIU version: if there are any other segmentation faults
		// *inside* MacODBC, trap them and treat them as a database error.
		ODBC_ValidatingPointers = 2;
		if (sigsetjmp (AfterPointerTest, 1))
		{
			GerrLogMini (GerrId, "PID %d: MacODBC caught a segmentation fault - ODBC_ValidatingPointers = %d.", getpid(), ODBC_ValidatingPointers);
			sqlca.sqlcode = SQLCODE = ODBC_SQLCODE = -2;
			ODBC_ValidatingPointers = 0;
			return (SQLCODE);
		}
#endif


		// DonR 12Feb2020: If the current command involves input OR output columns, the programmer
		// should put END_OF_ARG_LIST at the end of the argument list. By checking for this special
		// argument, we should be able to reduce bugs caused by programmer sloppiness. Since some
		// of these bugs can be show-stoppers, I'm going to make this a fatal error even though
		// END_OF_ARG_LIST was optional until now.
		if ((ColumnParameterCount > 0) && (!CaughtParameterProblem))
		{
			VoidStar = va_arg (ArgList, void *);

			if (VoidStar != END_OF_ARG_LIST)
			{
				sprintf (sqlca.sqlerrm, "Missing END_OF_ARG_LIST for %s %s - check parameter list!",
						 CurrentMacroName, ODBC_Operation_Name [OperationIdentifier_in]);
				ReturnCode = result = -1;	// Force "syntax" error.
				CaughtParameterProblem = 1;
				GerrLogMini (GerrId, "%s", sqlca.sqlerrm);
				break;
			}
		}	// Current command has at least one input or output column parameter.

		// If we caught any parameter problems (which may have happened inside a loop), it's time to give up now!
		if (CaughtParameterProblem)
		{
//GerrLogMini(GerrId, "CaughtParameterProblem = %d for %s %s.", CaughtParameterProblem, CurrentMacroName, ODBC_Operation_Name [OperationIdentifier_in]);
			break;
		}


		// If we get here, we should have exactly the right number of (valid, we hope!) input and output parameters.
		// Since we've already checked this, we don't have to check again in the binding routines below.

		// DonR 28Apr2020: Instead of using a separate pointer variable and incrementing it, just re-use
		// the subscript variable ColumnParameterCount.
		// We'll pull values out of this array the same as if we were pulling them from va_args.
		ColumnParameterCount = 0;	// Rewind to the beginning of the list of stored column pointers.


		// Now that the SQL statement has been prepared, it's time to bind columns to it.
		// First bind output columns, then input columns. (Why output before input? Just
		// so it's easier to convert embedded-SQL code to the new format.)
		// NOTE: SQL command mirroring is supported ONLY for write statements like INSERT,
		// UPDATE, DELETE, and possibly things like CREATE/DROP TABLE. Accordingly, there
		// is *no* mirroring of output-column binding, since that applies only to SELECTs
		// which are not mirrored.
		if (NeedToBindOutput)
		{
			for (i = 0; i < NumOutputColumns; i++)
			{
				// Diagnostic only - will be removed later, and we'll rely on the ODBC driver to throw
				// an error if the type is bad. (Of course, we'll need to check that it does so...)
				switch (OutputColumns[i].type)
				{
					// For output columns, VARCHAR is the same as CHAR - the only difference is
					// how the database stores the data internally.
					case		SQL_VARCHAR:		OutputColumns[i].type = SQL_C_CHAR;
					case		SQL_C_UTINYINT:		
					case		SQL_C_SSHORT:
					case		SQL_C_USHORT:
					case		SQL_C_SLONG:
					case		SQL_C_ULONG:
					case		SQL_C_SBIGINT:
					case		SQL_C_DOUBLE:
					case		SQL_C_FLOAT:
					case		SQL_C_CHAR:			break;

					default:						// Add normal error logging!
													GerrLogMini (GerrId, "Unrecognized output parameter type %d for column #%d of %s %s!",
															OutputColumns[i].type, (i + 1), CurrentMacroName, ODBC_Operation_Name [OperationIdentifier_in]);
													break;
				}

				// NOTE that for now, we're assuming that the only variable-length output field type
				// is CHAR/VARCHAR - we're not worrying about other variable-length types.
				VoidStar = ColumnParameterPointers [ColumnParameterCount++];

				if (OutputColumns[i].type == SQL_C_CHAR)
				{
					// The minimum length for a character output column is 2 - one character "payload"
					// and one character for the trailing NULL. If this is a single-character NON-pointer
					// output column, OutputColumns[i].length will be zero - so we have to force in a
					// bind length of 2. In this case we also need to save the "real" output address and
					// substitute the address of our internal buffer, since otherwise the trailing NULL
					// from the database variable would overwrite something in the calling routine.
					// DonR 06Jul2020: Instead of getting all fancy with the single-character output columns,
					// just use a nice, simple, pre-allocated array OneCharFetchBuffer [ODBC_MAX_PARAMETERS][2]
					// and a similar destination pointer array OneCharDestinationPointers. This loses nothing
					// in efficiency, and makes the code less prone to bugs.
					//
					// DonR 02Aug2020: The previous way of binding single-character fields was buggy, since
					// (after I simplified it to get rid of malloc's) I was relying on variables and arrays
					// that didn't retain their values from operation to operation. Rather than going back
					// to special allocated intermediate buffers to handle these fields, I came up with a
					// simpler method: SELECT the ASCII value of the field in the SQL statement, then bind it
					// as a 1-byte "tiny integer". This is slightly inelegant as it requres changing the
					// SELECT clause of the ODBC statement a bit, but its implementation is nice and simple:
					// no extra variables, no extra buffers, no extra anything - just a type switch to
					// SQL_C_UTINYINT. Note that when we switch to MS-SQL, we should check if simply binding
					// one byte of a character field will do the job without all the ASCII/TinyInt business!
					if (OutputColumns[i].length < 1)
					{
						OutputColumns[i].type = SQL_C_UTINYINT;
						BindLength = 0;
					}
					else
					{
						BindLength	= OutputColumns[i].length + 1;
					}
				}	// Output column type is CHAR.
				else
				{
					// For everything other than CHAR output types (i.e. various numeric types),
					// Bind Length is zero.
					BindLength = 0;
				}

				// NOTE: The Field Length Read is stored according to the column number passed
				// to ODBC - in other words, we start counting at 1, *not* zero!
				result = SQLBindCol (	*ThisStatementPtr,
										(unsigned short)(i + 1),	// DonR 02Apr2020: Typecast should be unnecessary, but it's harmless at worst.
										OutputColumns[i].type,
										VoidStar,
										BindLength,
										&FieldLengthsRead[i + 1]	);

				if (SQL_FAILED (result))
				{
					ReturnCode = ODBC_StatementError (ThisStatementPtr, OperationIdentifier_in);;
					GerrLogMini(GerrId, "PID %d: BIND failed for output column %d of SQL operation %s %s "
								"after %ld transactions! Result = %d, ReturnCode = %d, ODBC error = %s.",
								(int)getpid(), i + 1, CurrentMacroName, ODBC_Operation_Name [OperationIdentifier_in],
								ODBC_ActivityCount, result, ReturnCode, ODBC_ErrorBuffer);

					if (StatementIsSticky [OperationIdentifier_in])
					{
						// DonR 29Oct2020: Any time we un-prepare a sticky statement, we also have to
						// decrement the Sticky Handles Used counter.
						if (StatementPrepared [OperationIdentifier_in])
							NumStickyHandlesUsed--;

						SQLFreeHandle (SQL_HANDLE_STMT, *ThisStatementPtr);
						StatementPrepared	[OperationIdentifier_in] = 0;
						StatementOpened		[OperationIdentifier_in] = 0;
					}

					break;
				}

			}	// Loop through output columns and SQLBindCol them.
		}	// This operation type normally requires binding output columns.


		// Now bind input columns.
		if (NeedToBindInput)
		{
			for (i = 0; i < NumInputColumns; i++)
			{
				// DonR 17Nov2021: Moved VoidStar assignment to before the switch statement,
				// so that VoidStar is available for string-length checking.
				VoidStar = ColumnParameterPointers [ColumnParameterCount++];

				// Note that SQL_VARCHAR is permitted as an input column type, but not for output columns.
				// We need to make sure the ODBC BindODBCParam() function returns an error of some sort
				// when it's given an invalid parameter type.
				switch (InputColumns[i].type)
				{
					case		SQL_C_SSHORT:		SQL_ParamType = SQL_SMALLINT;	break;
					case		SQL_C_USHORT:		SQL_ParamType = SQL_SMALLINT;	break;
					case		SQL_C_SLONG:		SQL_ParamType = SQL_INTEGER;	break;
					case		SQL_C_ULONG:		SQL_ParamType = SQL_INTEGER;	break;
					case		SQL_C_SBIGINT:		SQL_ParamType = SQL_BIGINT;		break;
					case		SQL_C_FLOAT:
					case		SQL_C_DOUBLE:		SQL_ParamType = SQL_FLOAT;		break;
					case		SQL_C_UTINYINT:		SQL_ParamType = SQL_CHAR;		break;	// NOT TESTED AS OF 11DEC2019!

					// DonR 17Nov2021: In at least some cases, if an over-length string is passed to the ODBC
					// routines on an INSERT or UPDATE, the operation will fail to write to the database even
					// though no error code is returned. In order to avoid hard-to-find bugs, I'm adding logic
					// to correct the problem automatically, while at the same time logging the problem so that
					// the calling routine can be fixed - since the correction takes more processor cycles than
					// getting things right to begin with.
					case		SQL_C_CHAR:
					case		SQL_VARCHAR:
													SQL_ParamType			= (InputColumns[i].type == SQL_VARCHAR) ? SQL_VARCHAR : SQL_CHAR;
													InputColumns[i].type	= SQL_C_CHAR;	// Treat VARCHAR as CHAR.

													// First check the exact length, since this doesn't require a loop.
													// Check strlen() as a fallback.
													if (((char *)VoidStar) [InputColumns[i].length] != (char)0)
													{
														// First test failed, so now try strlen() - maybe the input is shorter
														// than the maximum allowed length.
														int	SourceStringLength = strlen((char *)VoidStar);

														if (SourceStringLength > InputColumns[i].length)
														{
															// String is in fact over its permitted length. Fix the problem, but
															// log a message so we'll know we have to fix the calling routine.
															((char *)VoidStar) [InputColumns[i].length] = (char)0;

															GerrLogMini (	GerrId, "%s parameter %d was truncated from %d to %d bytes.",
																			ODBC_Operation_Name [OperationIdentifier_in], (i + 1),
																			SourceStringLength, InputColumns[i].length);
														}
													}

													break;
					// DonR 17Nov2021 END.

//					case		SQL_VARCHAR:		SQL_ParamType			= SQL_VARCHAR;
//													InputColumns[i].type	= SQL_C_CHAR;		break;
//
//					case		SQL_C_CHAR:			SQL_ParamType = SQL_CHAR;		break;


					default:						// Add normal error logging!
													SQL_ParamType = 0;
													GerrLogMini (GerrId, "Unrecognized input parameter type %d for column #%d of %s %s!",
															InputColumns[i].type, (i + 1), CurrentMacroName, ODBC_Operation_Name [OperationIdentifier_in]);
													break;
				}


				result = SQLBindParameter (	*ThisStatementPtr,
											(unsigned short)(i + 1),	// DonR 02Apr2020: Typecast should be unnecessary, but it's harmless at worst.
											SQL_PARAM_INPUT,
											InputColumns[i].type,
											SQL_ParamType,
											((SQL_ParamType == SQL_CHAR) || (SQL_ParamType == SQL_VARCHAR)) ? InputColumns[i].length : 0,
											0,
											VoidStar,
											0,
											NULL	);
				if (SQL_FAILED (result))
				{
					ReturnCode = ODBC_StatementError (ThisStatementPtr, OperationIdentifier_in);
					GerrLogMini (GerrId, "PID %d: Error binding input parameter #%d for %s %s, type = %d, "
								 "result = %d, ptr = %ld, ODBC error = %s.",
								 getpid(), i + 1, CurrentMacroName, ODBC_Operation_Name [OperationIdentifier_in], InputColumns[i].type,
								 result, (long)VoidStar, ODBC_ErrorBuffer);
					break;
				}

				// DonR 21Feb2021: Add mirroring of INPUT columns.
				if (CommandIsMirrored [OperationIdentifier_in])
				{
					result = SQLBindParameter (	*MirrorStatementPtr,
												(unsigned short)(i + 1),	// DonR 02Apr2020: Typecast should be unnecessary, but it's harmless at worst.
												SQL_PARAM_INPUT,
												InputColumns[i].type,
												SQL_ParamType,
												((SQL_ParamType == SQL_CHAR) || (SQL_ParamType == SQL_VARCHAR)) ? InputColumns[i].length : 0,
												0,
												VoidStar,
												0,
												NULL	);
					if (SQL_FAILED (result))
					{
						ReturnCode = ODBC_StatementError (MirrorStatementPtr, OperationIdentifier_in);
						GerrLogMini (GerrId, "Error binding input parameter #%d for mirrored %s %s, type = %d, result = %d, ptr = %ld, ODBC error = %s.",
									 i + 1, CurrentMacroName, ODBC_Operation_Name [OperationIdentifier_in], InputColumns[i].type, result, (long)VoidStar, ODBC_ErrorBuffer);
						break;
					}
				}	// Command is mirrored, so bind INPUT columns to mirrored statement.
			}	// Loop through input parameters and SQLBindParameter them.


			// If there are any additional parameters in a "custom" WHERE clause, bind those as well.
			// (Note that we're assuming that these always come after "normal" input parameters -
			// I *think* that's correct!)
			if ((NeedsWhereClauseID) && (NumWhereClauseInputColumns > 0))
			{
				for (i = 0; i < NumWhereClauseInputColumns; i++)
				{
					// Note that SQL_VARCHAR is permitted as an input column type, but not for output columns.
					// We need to make sure the ODBC BindODBCParam() function returns an error of some sort
					// when it's given an invalid parameter type.
					switch (WhereClauseInputColumns[i].type)
					{
						case		SQL_C_SSHORT:		SQL_ParamType = SQL_SMALLINT;	break;
						case		SQL_C_USHORT:		SQL_ParamType = SQL_SMALLINT;	break;
						case		SQL_C_SLONG:		SQL_ParamType = SQL_INTEGER;	break;
						case		SQL_C_ULONG:		SQL_ParamType = SQL_INTEGER;	break;
						case		SQL_C_SBIGINT:		SQL_ParamType = SQL_BIGINT;		break;
						case		SQL_C_FLOAT:
						case		SQL_C_DOUBLE:		SQL_ParamType = SQL_FLOAT;		break;
						case		SQL_C_CHAR:			SQL_ParamType = SQL_CHAR;		break;
						case		SQL_C_UTINYINT:		SQL_ParamType = SQL_CHAR;		break;	// NOT TESTED AS OF 11DEC2019!
						case		SQL_VARCHAR:		SQL_ParamType = SQL_VARCHAR;	WhereClauseInputColumns[i].type = SQL_C_CHAR;		break;

						default:						// Add normal error logging!
														SQL_ParamType = 0;
														GerrLogMini (GerrId, "Unrecognized where-clause parameter type %d for column #%d of %s %s!",
																	 WhereClauseInputColumns[i].type, (i + 1), CurrentMacroName, ODBC_Operation_Name [OperationIdentifier_in]);
														break;
					}

					VoidStar = ColumnParameterPointers [ColumnParameterCount++];

					// Note that we add NumInputColumns to the parameter number, since this list comes
					// after the "normal" input parameter list.
					result = SQLBindParameter (	*ThisStatementPtr,
												(unsigned short)(i + NumInputColumns + 1),	// DonR 02Apr2020: Typecast should be unnecessary, but it's harmless at worst.
												SQL_PARAM_INPUT,
												WhereClauseInputColumns[i].type,
												SQL_ParamType,
												((SQL_ParamType == SQL_CHAR) || (SQL_ParamType == SQL_VARCHAR)) ? WhereClauseInputColumns[i].length : 0,
												0,
												VoidStar,
												0,
												NULL	);
					if (SQL_FAILED (result))
					{
						ReturnCode = ODBC_StatementError (ThisStatementPtr, OperationIdentifier_in);
						GerrLogMini (GerrId, "Error binding custom WHERE clause parameter #%d for %s %s, type = %d, result = %d.\n",
									 i + NumInputColumns + 1, CurrentMacroName, ODBC_Operation_Name [OperationIdentifier_in], WhereClauseInputColumns[i].type, result);
						break;
					}

					// DonR 21Feb2021: Add mirroring of WHERE-clause columns.
					if (CommandIsMirrored [OperationIdentifier_in])
					{
						// Note that we add NumInputColumns to the parameter number, since this list comes
						// after the "normal" input parameter list.
						result = SQLBindParameter (	*MirrorStatementPtr,
													(unsigned short)(i + NumInputColumns + 1),	// DonR 02Apr2020: Typecast should be unnecessary, but it's harmless at worst.
													SQL_PARAM_INPUT,
													WhereClauseInputColumns[i].type,
													SQL_ParamType,
													((SQL_ParamType == SQL_CHAR) || (SQL_ParamType == SQL_VARCHAR)) ? WhereClauseInputColumns[i].length : 0,
													0,
													VoidStar,
													0,
													NULL	);
						if (SQL_FAILED (result))
						{
							ReturnCode = ODBC_StatementError (MirrorStatementPtr, OperationIdentifier_in);
							GerrLogMini (GerrId, "Error binding custom WHERE clause parameter #%d for mirrored %s %s, type = %d, result = %d.\n",
										 i + NumInputColumns + 1, CurrentMacroName, ODBC_Operation_Name [OperationIdentifier_in], WhereClauseInputColumns[i].type, result);
							break;
						}
					}	// Command is mirrored, so bind "custom" WHERE columns to mirrored statement.
				}	// Loop through "custom" WHERE clause input parameters and SQLBindParameter them.
			}	// There is a "custom" WHERE clause with at least one input parameter.
		}	// This operation type normally requires binding input columns.


		// Now that we're all bound up, try to execute the SQL statement. (This is done
		// for open-cursor commands and for singleton operations.)
		if (NeedToExecute)
		{
			// If we haven't done a PREPARE (e.g. if we do an OpenCursor without DeclareCursor),
			// throw an error and don't even try to perform the SQLExecute.
			// DonR 29Mar2020: Use simple subscript rather than UseStatementSubscript[].
			if (!StatementPrepared [OperationIdentifier_in])
			{
				// DonR 07Oct2021: If we lost connection to the database, we want to return DB_CONNECTION_BROKEN
				// so that a reconnect gets triggered.
				if (Database->Connected == 0)
				{
					ReturnCode = result = DB_CONNECTION_BROKEN;	// Triggers automatic reconnect.
					strcpy (sqlca.sqlerrm, "Attempted to EXECUTE SQL statement on non-opened database.");
//GerrLogMini (GerrId, "PID %d: Attempted to EXECUTE %s %s on closed database - returning ReturnCode %d.",
//			 (int)getpid(), CurrentMacroName, ODBC_Operation_Name [OperationIdentifier_in], ReturnCode);

					// Note that we don't worry about stickiness here - if the database was closed,
					// all the sticky status stuff was already cleared.
				}
				else
				{
					ReturnCode = result = -400;	// Invalid Cursor State.
					strcpy (sqlca.sqlerrm, "Attempted to EXECUTE/OPEN before SQL statement was PREPARED/DECLARED.");
GerrLogMini (GerrId, "PID %d: Attempted to EXECUTE/OPEN %s %s before SQL statement was PREPARED/DECLARED - returning ReturnCode %d.",
			 (int)getpid(), CurrentMacroName, ODBC_Operation_Name [OperationIdentifier_in], ReturnCode);

					// DonR 20Jun2020: If this is a sticky statement, do some cleanup just to be safe.
					if (StatementIsSticky [OperationIdentifier_in])
					{
						SQLFreeHandle (SQL_HANDLE_STMT, *ThisStatementPtr);
						StatementOpened		[OperationIdentifier_in] = 0;

						// DonR 29Oct2020: Note that since StatementPrepared is already FALSE for
						// this operation, we shouldn't have to decrement NumStickyHandlesUsed
						// in this case.
					}
				}	// Database is still connected.

				break;
			}	// StatementPrepared [OperationIdentifier_in] is FALSE.


			// DonR 21Feb2021: If mirroring is on for this SQL operation, execute the mirrored version
			// first (for no super-important reason). For the moment at least, we want to treat errors
			// from the mirrored execution somewhat less seriously than for the main database - if
			// both give errors, the one reported to the calling routine will be from the main database.
			if (CommandIsMirrored [OperationIdentifier_in])
			{
				MIRROR_SQLCODE = result = SQLExecute (*MirrorStatementPtr);

				if (SQL_FAILED (result))
				{
					MIRROR_SQLCODE = ReturnCode = ODBC_StatementError (MirrorStatementPtr, OperationIdentifier_in);

					// If we hit an error in SQLExecute for the mirrored operation, it really should be
					// reported through "normal" channels - but just in case (particularly if there's
					// an error for the main database as well) write a log message here.
					//
					// DonR 05Jul2022: Previously, we didn't log an error on the mirrored database if
					// it was something "ordinary" like contention or a primary-key violation. However,
					// today we had a Production problem where I wasted considerable time (mine and
					// the DBA team's) because I didn't realize that errors were coming from mirrored
					// Informix updates rather than from MS-SQL. Accordingly, I'm changing the code
					// here to log pretty much *all* errors on the mirrored database - they don't happen
					// often (and there aren't a lot of mirrored SQL operations in any case) and it's
					// important to know when they happen so we know exactly where to look for the problem.
					switch (MIRROR_SQLCODE)
					{
						// If the error was something that's considered "normal" - i.e. database contention,
						// duplicate key on INSERT, or something else that's the result of a temporary
						// circumstance and not a programming error or a result of some kind of ODBC
						// corruption - it's enough to return the error code to the calling routine. Only
						// in the case of something that we *don't* consider normal do we want to print a
						// special diagnostic and free the "sticky" statement that failed to execute.
						// DonR 05Jul2022: See my comment above. I'm disabling almost all of the "case"
						// statements here, so that we *will* specifically log errors when writing to the
						// mirrored database.
//						case INF_NOT_UNIQUE_INDEX:
//						case SQL_NOT_UNIQUE_INDEX:
//						case SQL_NOT_UNIQUE_PRIMARY_KEY:
//						case INF_NOT_UNIQUE_CONSTRAINT:
//						case SQLERR_CANNOT_POSITION_WITHIN_TABL:
//						case INF_ACCESS_CONFLICT:
//						case INF_CANNOT_POSITION_VIA_INDEX:
//						case INF_CANNOT_GET_NEXT_ROW:
//						case INF_TABLE_IS_LOCKED_FOR_INSERT:
//						case INF_DEADLOCK:							// DonR 17Mar2021 added to "normal" DB contention list.
//						case SQL_DEADLOCK:							// DonR 17Mar2021 added to "normal" DB contention list.
//						case SQL_INTEGER_PRECISION_EXCEEDED:
						case SQL_UNICODE_CONVERSION_FAILED:			// DonR 22Mar2021 added Unicode Conversion Failure to the list
																	// of "normal" non-fatal events - although I think we see this
																	// error only on FETCH, not on EXECUTE.
																	break;

						default:
									GerrLogMini (GerrId, "PID %d mirrored %s %s returning result %d after SQLExecute.\nError = {%s}.",
										(int)getpid(), CurrentMacroName, ODBC_Operation_Name [OperationIdentifier_in], result, sqlca.sqlerrm);
									break;
					}	// Switch on MIRROR_SQLCODE.
				}	// Execution failed for mirrored SQL operation.
				else
				{	// Mirrored SQL operation succeeded.
					// DonR 31Dec2020: Set ALTERNATE_DB_USED true, so that CommitAllWork will actually
					// affect all databases. If we get to COMMIT time and ALTERNATE_DB_USED is false, we will
					// treat CommitAllWork() the same as CommitWork(MAIN_DB), which may execute faster. (Note
					// that NeedToCommit and NeedToRollBack are false for all operations other than COMMIT and
					// ROLLBACK, so if they're both false we know we're doing something else.)
					// DonR 21Feb2021: Moved the code to set down to the "execute" section.
					ALTERNATE_DB_USED	= 1;
					MIRROR_SQLCODE		= 0;

					result = SQLRowCount (*MirrorStatementPtr, &MIRROR_ROWS_AFFECTED);
//GerrLogMini (GerrId, "PID %d mirrored %s %s returning successful result %d after SQLExecute. %d rows affected.",
//	(int)getpid(), CurrentMacroName, ODBC_Operation_Name [OperationIdentifier_in], result, MIRROR_ROWS_AFFECTED);
				}	// Successful execution of mirrored SQL operation.
			}	// SQL execution for mirrored database.


			// Regular execution for whichever database the calling routine requested.
			ReturnCode = result = SQLExecute (*ThisStatementPtr);

//			// TEMPORARY TESTING CODE!
//			if (OperationIdentifier_in == IsPharmOpen_READ_pharmacy)
//			{
//				ForceDbErrorTenthTime++;
//
//				if (ForceDbErrorTenthTime == 10)
//				{
//					ReturnCode = result = SQL_TRN_CLOSED_BY_OTHER_SESSION;
//				}
//			}
//
			if (SQL_FAILED (result))
			{
				ReturnCode = ODBC_StatementError (ThisStatementPtr, OperationIdentifier_in);

				// DonR 20Jun2020: If this is a sticky statement, do some cleanup just to be safe.
				// DonR 11Nov2020: Don't print special diagnostics or free the statement for ordinary,
				// innocuous stuff like access-conflict errors. (Note that for "synthetic" access-
				// conflict errors generated in the case of loss-of-preparation, the error-handling
				// routine will take care of freeing the statement handle, etc.) Note that we'll
				// probably have to add some MS-SQL-specific codes to this switch statement once we
				// know what kind of access-conflict errors we actually see in MS-SQL real-life situations.
				// DonR 05Sep2023: Also don't print special diagnostics or free the statement for errors
				// that are going to force us to close down the whole database connection and re-open it.
				// The only example of an error like this - so far! - is 3926 = SQL_TRN_CLOSED_BY_OTHER_SESSION.
				// DonR 10Sep2023: Got another error code that triggers auto-reconnect: 10060, which I've
				// assigned to the new macro DB_TCP_PROVIDER_ERROR.
				// DonR 27Jul2025: Adding another error code to the auto-reconnect list: TABLE_SCHEMA_CHANGED
				// (= 16943), which is caused by some overnight database-maintenance routine.
				if (StatementIsSticky [OperationIdentifier_in])
				{
					switch (ReturnCode)
					{
						// If the error was something that's considered "normal" - i.e. database contention,
						// duplicate key on INSERT, or something else that's the result of a temporary
						// circumstance and not a programming error or a result of some kind of ODBC
						// corruption - it's enough to return the error code to the calling routine. Only
						// in the case of something that we *don't* consider normal do we want to print a
						// special diagnostic and free the "sticky" statement that failed to execute.
						case INF_NOT_UNIQUE_INDEX:
						case SQL_NOT_UNIQUE_INDEX:
						case SQL_NOT_UNIQUE_PRIMARY_KEY:
						case INF_NOT_UNIQUE_CONSTRAINT:
						case SQLERR_CANNOT_POSITION_WITHIN_TABL:
						case INF_ACCESS_CONFLICT:
						case INF_CANNOT_POSITION_VIA_INDEX:
						case INF_CANNOT_GET_NEXT_ROW:
						case INF_TABLE_IS_LOCKED_FOR_INSERT:
						case INF_DEADLOCK:							// DonR 17Mar2021 added to "normal" DB contention list.
						case SQL_DEADLOCK:							// DonR 17Mar2021 added to "normal" DB contention list.
						case SQL_TRN_CLOSED_BY_OTHER_SESSION:		// DonR 03Sep2023 Bug #469281: Added to "normal" list - will force disconnect-reconnect.
						case DB_TCP_PROVIDER_ERROR:					// DonR 10Sep2023 Bug #469281: Added to "normal" list - will force disconnect-reconnect.
						case DB_CONNECTION_BROKEN:					// DonR 06Sep2023 Bug #469281: Added to "normal" list - will force disconnect-reconnect.
						case TABLE_SCHEMA_CHANGED:					// DonR 27Jul2025 Bug #435292: Added to "normal" list - will force disconnect-reconnect.
						case SQL_INTEGER_PRECISION_EXCEEDED:
						case SQL_UNICODE_CONVERSION_FAILED:			// DonR 22Mar2021 added Unicode Conversion Failure to the list
																	// of "normal" non-fatal events - although I think we see this
																	// error only on FETCH, not on EXECUTE.

//GerrLogMini (GerrId, "SQLExecute returned %d, ReturnCode = %d.", result, ReturnCode);
																	break;
						default:

//GerrLogFnameMini ("sticky_log", GerrId, "PID %d %s %s returning result %d after SQLExecute - setting StatementPrepared FALSE.\nError = {%s}.",
//	(int)getpid(), CurrentMacroName, ODBC_Operation_Name [OperationIdentifier_in], result, sqlca.sqlerrm);
// Temporary diagnostic for MS-SQL testing:
GerrLogMini (GerrId, "PID %d %s %s returning result %d / ReturnCode %d after SQLExecute - setting StatementPrepared FALSE.\nError = {%s}.",
	(int)getpid(), CurrentMacroName, ODBC_Operation_Name [OperationIdentifier_in], result, ReturnCode, sqlca.sqlerrm);

																	// DonR 29Oct2020: Any time we un-prepare a sticky statement, we also have to
																	// decrement the Sticky Handles Used counter. Note that if this is a "synthetic"
																	// -243 error set up by ODBC_StatementError(), StatementPrepared will already
																	// have been set to zero.
																	if (StatementPrepared [OperationIdentifier_in])
																		NumStickyHandlesUsed--;

																	SQLFreeHandle (SQL_HANDLE_STMT, *ThisStatementPtr);
																	StatementPrepared	[OperationIdentifier_in] = 0;	// DonR 06Jul2020.
																	StatementOpened		[OperationIdentifier_in] = 0;

																	break;
					}	// Switch to avoid overreacting to innocuous DB errors.
				}	// Statement that failed to execute was "sticky".

				break;	// From outer "do loop", so we don't try to fetch anything.

			}	// Statement execution failed.
			else
			{	// Statement executed OK.

				// DonR 21Feb2021: Set ALTERNATE_DB_USED here instead of up above. Check whether
				// there are any other conditions that should be set!
				if (Database != MAIN_DB)
				{
					// DonR 31Dec2020: Set ALTERNATE_DB_USED true, so that CommitAllWork will actually
					// affect all databases. If we get to COMMIT time and ALTERNATE_DB_USED is false, we will
					// treat CommitAllWork() the same as CommitWork(MAIN_DB), which may execute faster. (Note
					// that NeedToCommit and NeedToRollBack are false for all operations other than COMMIT and
					// ROLLBACK, so if they're both false we know we're doing something else.)
					// DonR 21Feb2021: Moved the code to set down to the "execute" section.
					ALTERNATE_DB_USED = 1;
				}

				// DonR 29Mar2020: Use simple subscript rather than UseStatementSubscript[].
				StatementOpened [OperationIdentifier_in] = 1;

				// DonR 15Nov2021: Instead of getting SQLRowCount directly into sqlerrd[2] (which
				// may not be the same type as SQLRowCount stores) read into RowsAffected (defined
				// as SQLLEN) and then copy it to sqlerrd[2]. I don't think this was the actual
				// reason we were getting -1 in sqlerrd[2], but the fix is harmless at worst.
				result = SQLRowCount (*ThisStatementPtr, &RowsAffected);
				sqlca.sqlerrd[2] = (int)RowsAffected;

				// DonR 16Nov2021: For now, log anything other than SQL_SUCCESS, even if it's innocuous.
				// If SQLRowCount() actually failed, maybe we need to return an actual error to the
				// calling routine!
				if (result != SQL_SUCCESS)
				{
					int	RowCountReturnCode;	// So we don't overwrite ReturnCode.

					RowCountReturnCode = ODBC_StatementError (ThisStatementPtr, OperationIdentifier_in);

					GerrLogMini (	GerrId, "SQLRowCount result = %d. ReturnCode %d, row count %d, sqlerrm =\n%s",
									result, RowCountReturnCode, sqlca.sqlerrd[2], sqlca.sqlerrm);

					// if SQL_FAILED (result)...
				}

				// DonR 16Nov2021: If this was a "singleton" INSERT/UPDATE/DELETE request and rows-affected
				// was less than zero, print a diagnostic and force rows-affected to zero.
				if ((CommandType_in == SINGLETON_SQL_CALL) && (sqlca.sqlerrd[2] < 0))
				{
					if (NumOutputColumns < 1)
					{
						GerrLogMini (GerrId, "Changing rows-affected from %d to zero.", sqlca.sqlerrd[2]);
					}

					sqlca.sqlerrd[2] = 0;
				}

				// DonR 18May2022: For some reason, MS-SQL sometimes fails to INSERT a row even though
				// SQLExecute() did not return an error code. (So far, I've seen this happen only for
				// one INSERT in Transaction 6005-spool - but it doesn't look like there's anything
				// wrong with the application code, and an internet search showed that other people
				// have had the same problem.) Rather than try to fix MS-SQL or find an apparently non-
				// existent bug in the application code, it seems sensible to check the rows-affected
				// value after every apparently-successful INSERT, and force a "synthetic" SQL error
				// if no row was in fact INSERTed. This way, we don't have to put explicit "no rows
				// affected" checks after every INSERT in the system, because the logic below will
				// set an error that the regular SQL error test will report.
				if ((IsInsertCommand [OperationIdentifier_in]) && (sqlca.sqlerrd[2] < 1))
				{
					ReturnCode = result = SQL_SILENT_INSERT_FAILURE;

					// Note that once we see that this feature is working, we can delete this diagnostic;
					// the regular SQL error test will log the error.
					GerrLogMini (	GerrId,
									"\"Silent\" INSERT failure in %s - forcing SQL_SILENT_INSERT_FAILURE error.",
									ODBC_Operation_Name [OperationIdentifier_in]	);
				}

				// DonR 22Feb2021: If we're in mirroring mode for this operation, we want the
				// return status information to reflect the "worse case" between the two
				// databases we wrote to.
				if (CommandIsMirrored [OperationIdentifier_in])
				{
					// Store the *lower* value of SQLRowCount between the two databases - so if
					// a row we want to UPDATE is present in one database but not in the other,
					// we'll return "no rows affected" and the application code will know that
					// it has to INSERT the value.
					// DonR 16Nov2021: Don't swap in MIRROR_ROWS_AFFECTED if it's a negative (= error) value.
					if ((MIRROR_ROWS_AFFECTED < sqlca.sqlerrd[2]) && (MIRROR_ROWS_AFFECTED >= 0))
					{
						sqlca.sqlerrd[2] = MIRROR_ROWS_AFFECTED;
					}

					// If the mirror-database threw an error, swap that in place of the successful
					// return code we got from the primary database.
					if (MIRROR_SQLCODE != 0)
					{
						ReturnCode = MIRROR_SQLCODE;
					}
				}	// Need to combine return values for mirrored and primary databases.

			}	// SQLExecute succeeded.

		}	// This operation type requires executing an SQL statement.


		// ...And fetch the results!
		// If there are no output columns, we force NeedToFetch FALSE even if this is an
		// operation type that otherwise would require a Fetch. This will happen all the
		// time for singleton operations like INSERT or UPDATE. (But for any kind of FETCH,
		// we obviously want to fetch - NumOutputColumns will be zero for a FETCH after
		// a SELECT INTO.)
		// Note that mirroring is *not* supported for SELECT statements, so the FETCH
		// code doesn't deal with mirrored operations.
		if ((CommandType_in == SINGLETON_SQL_CALL) && (NumOutputColumns < 1))
			NeedToFetch = 0;

		if (NeedToFetch)
		{
			// If we haven't done an OPEN (e.g. if we forgot to perform an OpenCursor),
			// throw an error and don't even try to perform the FETCH.
			// DonR 29Mar2020: Use simple subscript rather than UseStatementSubscript[].
			if (!StatementOpened [OperationIdentifier_in])
			{
				ReturnCode = result = -400;	// Invalid Cursor State.
				sprintf (sqlca.sqlerrm, "PID %d: Attempted to FETCH before SQL statement was EXECUTED/OPENED.", (int)getpid());

				// DonR 20Jun2020: If this is a sticky statement, do some cleanup just to be safe.
				if (StatementIsSticky [OperationIdentifier_in])
				{
					// DonR 29Oct2020: Any time we un-prepare a sticky statement, we also have to
					// decrement the Sticky Handles Used counter.
					if (StatementPrepared [OperationIdentifier_in])
						NumStickyHandlesUsed--;

					SQLFreeHandle (SQL_HANDLE_STMT, *ThisStatementPtr);
					StatementPrepared	[OperationIdentifier_in] = 0;
				}

//GerrLogMini (GerrId, "Attempted to FETCH before SQL statement was EXECUTED/OPENED - returning ReturnCode %d.\n", ReturnCode);
				break;
			}

			ReturnCode = result = SQLFetchScroll (*ThisStatementPtr, SQL_FETCH_NEXT, 0);

			// DonR 21Jun2020: Added new Convert_NF_to_zero flag. If this is set TRUE
			// (which should be only for SELECT's returning only a COUNT value) we do
			// *not* want to generate errors for SQLCODE 100 (= not found).
			if ((ReturnCode == 100) && (Convert_NF_to_zero [OperationIdentifier_in]))
			{
				*(int *)ColumnParameterPointers [0] = ReturnCode = result = 0;	// Override error result.
//GerrLogMini (GerrId, "%s %s: Converted not-found to COUNT = 0.", CurrentMacroName, ODBC_Operation_Name [OperationIdentifier_in]);
			}

			if ((result != 100) && (SQL_FAILED (result)))
			{
				FoundFetchError = 0;	// We haven't established yet whether the FETCH error is serious.

				ReturnCode = ODBC_StatementError (ThisStatementPtr, OperationIdentifier_in);

				// DonR 20Jun2020: If this is a sticky statement, do some cleanup just to be safe.
				switch (ReturnCode)
				{
						case SQL_UNICODE_CONVERSION_FAILED:		// DonR 22Mar2021 added Unicode Conversion Failure to the list of "normal" non-fatal events.
																FoundFetchError = 0;	// Should be redundant, since it's defaulted above. Twice.
																break;

						default:								// Anything not on the "ignore" list is treated as serious.
																FoundFetchError = 1;

																if (StatementIsSticky [OperationIdentifier_in])
																{
																	// DonR 29Oct2020: Any time we un-prepare a sticky statement, we also have to
																	// decrement the Sticky Handles Used counter.
																	if (StatementPrepared [OperationIdentifier_in])
																		NumStickyHandlesUsed--;

																	SQLFreeHandle (SQL_HANDLE_STMT, *ThisStatementPtr);
																	StatementPrepared	[OperationIdentifier_in] = 0;	// DonR 06Jul2020.
																	StatementOpened		[OperationIdentifier_in] = 0;
																}

																break;
				}	// Switch on FETCH ReturnCode.

				// Break only if the FETCH error is something serious.
				if (FoundFetchError)
					break;
			}

			// Database servers are not required to return the number of rows SELECTed, either
			// after an EXECUTE or after a FETCH. It appears that MS-SQL does return a value,
			// but Informix does not. Thus for Informix there's no point in even calling
			// SQLRowCount.
			// DonR 31Aug2020: It now appears that even MS-SQL does not return a meaningful
			// value from SQLRowCount(), and according to the documentation I've found online
			// it's not a guaranteed-to-work approach. Accordingly, I'm going to use the
			// SQLFetchScroll() method unconditionally for all databases.
			// DonR 08Nov2020: Note that we're returning the Informix cardinality-violation
			// code (SQLERR_TOO_MANY_RESULT_ROWS = -284) regardless of which database we're using.
			if ((NeedToCheckCardinality) && ((ReturnCode == 0) || (ReturnCode == 1)))
			{
				result = SQLFetchScroll (*ThisStatementPtr, SQL_FETCH_NEXT, 0);
				if (SQL_WORKED (result))
				{
					ReturnCode = SQLERR_TOO_MANY_RESULT_ROWS;
				}
			}	// Need to check cardinality.
		}	// There is at least one output field, so we need to fetch something.


		if (NeedToCommit)
		{
			// If the incoming database pointer is NULL, the calling function wants to commit
			// ALL databases - so we execute the SQLEndTran() at the environment level.
			// DonR 31Dec2020: If only the main database has been accessed, interpret
			// CommitAllWork() as if the calling function had called CommitWork(MAIN_DB).
			if (Database == NULL)
			{
//GerrLogMini(GerrId, "CommitAllWork: ALTERNATE_DB_USED = %d.", ALTERNATE_DB_USED);
				if (ALTERNATE_DB_USED)
				{
					ALTERNATE_DB_USED = 0;	// Reset back to false.

					// Commit *all* databases we've connected to.
					result = SQLEndTran (SQL_HANDLE_ENV, ODBC_henv, SQL_COMMIT);
					if (SQL_FAILED (result))
					{
						ReturnCode = ODBC_EnvironmentError (&ODBC_henv);
						break;
					}
				}
				else
				{
					// Commit the main database handle because the alternate handle was never used.
					result = SQLEndTran (SQL_HANDLE_DBC, MAIN_DB->HDBC, SQL_COMMIT);
					if (SQL_FAILED (result))
					{
						ReturnCode = ODBC_DB_ConnectionError (&MAIN_DB->HDBC);
						break;
					}
				}
			}
			else
			{
//GerrLogMini(GerrId, "CommitWork: ALTERNATE_DB_USED = %d, database %s MAIN_DB.", ALTERNATE_DB_USED, (Database == MAIN_DB) ? "is" : "is NOT");
				// If the calling function is COMMITting the alternate database, clear
				// the alternate-database-used indicator.
				if (Database != MAIN_DB)
				{
					ALTERNATE_DB_USED = 0;
				}

				// Commit the database handle passed in by the calling function.
				result = SQLEndTran (SQL_HANDLE_DBC, Database->HDBC, SQL_COMMIT);
				if (SQL_FAILED (result))
				{
					ReturnCode = ODBC_DB_ConnectionError (&Database->HDBC);
					break;
				}
			}
		}	// NeedToCommit.


		if (NeedToRollBack)
		{
//GerrLogMini (GerrId, "SQL_ROLLBACK was called - Database %s NULL.", (Database == NULL) ? "is" : "is not");
			// If the incoming database pointer is NULL, the calling function wants to roll
			// back ALL databases - so we execute the SQLEndTran() at the environment level.
			// DonR 31Dec2020: Since ROLLBACKs are exceptional events, we're not going to bother
			// optimizing RollbackAllWork(). But we still have to selectively reset the
			// alternate-database-used indicator.
//			GerrLogMini (GerrId, "PID %d got rollback request for %s.",
//						 getpid(), (Database == NULL) ? "all databases" : ((Database == MAIN_DB) ? "main database" : "alternate database"));

			if (Database == NULL)
			{
				// Roll back *all* databases we've connected to.
				ALTERNATE_DB_USED = 0;
				result = SQLEndTran (SQL_HANDLE_ENV, ODBC_henv, SQL_ROLLBACK);
//				GerrLogMini (GerrId, "Rollback all DBs - result = %d.", result);
				if (SQL_FAILED (result))
				{
					ReturnCode = ODBC_EnvironmentError (&ODBC_henv);
					GerrLogMini (GerrId, "Rollback all DBs failed - result = %d, ReturnCode %d.", result, ReturnCode);
					break;
				}
			}
			else
			{
				// If the calling function is rolling back the alternate database,
				// clear the alternate-database-used indicator.
				if (Database != MAIN_DB)
					ALTERNATE_DB_USED = 0;

				// Roll back the database handle passed in by the calling function.
				result = SQLEndTran (SQL_HANDLE_DBC, Database->HDBC, SQL_ROLLBACK);
//				GerrLogMini (GerrId, "Rollback specified DB - result = %d.", result);
				if (SQL_FAILED (result))
				{
					ReturnCode = ODBC_DB_ConnectionError (&Database->HDBC);
					GerrLogMini (GerrId, "Rollback single DB failed - result = %d, ReturnCode %d.", result, ReturnCode);
					break;
				}
			}
		}	// NeedToRollBack.


		// DonR 25Aug2020: Reconnected the isolation control routine _set_db_isolation() - but
		// the SQLSetConnectAttr() call fails consistently, at least for dirty mode. Accordingly,
		// I'm going to disable these SQLSetConnectAttr() calls for Informix.

		if (NeedToSetDirty)
		{
			result = (Database == &INF_DB) ? 0 : SQLSetConnectAttr (Database->HDBC, SQL_ATTR_TXN_ISOLATION, (SQLPOINTER)SQL_TXN_READ_UNCOMMITTED, SQL_IS_UINTEGER);
			if (SQL_FAILED (result))
			{
				GerrLogMini (GerrId, "Set %s isolation DIRTY failed (result = %d).", Database->Name, result);
				ReturnCode = ODBC_DB_ConnectionError (&Database->HDBC);
			}
else GerrLogMini (GerrId, "Set %s isolation DIRTY succeeded (result = %d).", Database->Name, result);
		}


		if (NeedToSetCommitted)
		{
			result = (Database == &INF_DB) ? 0 : SQLSetConnectAttr (Database->HDBC, SQL_ATTR_TXN_ISOLATION, (SQLPOINTER)SQL_TXN_READ_COMMITTED, SQL_IS_UINTEGER);
			if (SQL_FAILED (result))
			{
				GerrLogMini (GerrId, "Set %s isolation COMMITTED failed (result = %d).", Database->Name, result);
				ReturnCode = ODBC_DB_ConnectionError (&Database->HDBC);
			}
else GerrLogMini (GerrId, "Set %s isolation COMMITTED succeeded (result = %d).", Database->Name, result);
		}


		if (NeedToSetRepeatable)
		{
			result = (Database == &INF_DB) ? 0 : SQLSetConnectAttr (Database->HDBC, SQL_ATTR_TXN_ISOLATION, (SQLPOINTER)SQL_TXN_REPEATABLE_READ, SQL_IS_UINTEGER);
			if (SQL_FAILED (result))
			{
				GerrLogMini (GerrId, "Set %s isolation REPEATABLE failed (result = %d).", Database->Name, result);
				ReturnCode = ODBC_DB_ConnectionError (&Database->HDBC);
			}
		}

	} while (0);	// End of big dummy loop.



	// If ReturnCode = 1 (OK with info), just return 0 (OK) in SQLCODE.
	sqlca.sqlcode = SQLCODE = ODBC_SQLCODE = (ReturnCode == 1) ? 0 : ReturnCode;


	// NeedToClose will be true if the current command is CLOSE_CURSOR
	// or SINGLETON_SQL_CALL. If the command is set up as non-"sticky",
	// we need to do a "hard close" by calling SQLFreeHandle. If the
	// command is "sticky", we want to do a "soft close", closing the
	// cursor and losing any pending results, but keeping all our
	// preparation and binding. And if the command was a singleton
	// statement with no output columns, we can skip even this step,
	// since any SELECT command will have output columns and other
	// commands don't create a cursor. (This may have to be revisited
	// if someone starts using INSERT cursors, but I don't think there
	// are any of those in the system at present and I don't see any
	// likely need for one.)
	//
	// It's worth noting that the model implemented here is one that
	// has good "hygiene", in that it doesn't leave PREPARED statements
	// sitting around unless they're "sticky". On the other hand, we
	// may see reduced performance from this - I'm not certain, but it's
	// possible that embedded SQL was doing the PREPARE only once for
	// each statement (or perhaps only once for each cursor), and forcing
	// the system to perform a lot more PREPARE's would gum up the works.
	// If we see this happening, the first point of attack is to be more
	// aggressive in designating statements as "sticky", and *not*
	// executing FreeStatement on them in high-volume transactions. We
	// may even want to increase ODBC_MAX_STICKY_STATEMENTS to give
	// ourselves room for more "sticky" statements.
	if (NeedToClose)
	{
		if (StatementIsSticky [OperationIdentifier_in])
		{
			// If this is a singleton command and it's something that doesn't involve
			// output columns (like an INSERT or an UPDATE, for example) don't bother
			// doing anything. Note that it might be just as sensible to execute the
			// SQLFreeStmt even in those cases - it shouldn't have much overhead.
			if (( CommandType_in == CLOSE_CURSOR) ||
				((CommandType_in == SINGLETON_SQL_CALL) && (NumOutputColumns > 0)))
			{
				// DonR 20Jun2020: While SQLFreeStmt (HSTMT, SQL_CLOSE) is supposed to be the
				// exact equivalent of SQLCloseCursor (HSTMT), apparently Informix has a poorly
				// documented "feature": if the database is in "autofree" mode AND no results
				// were returned from SELECT, SQLFreeStmt will unprepare the statement
				// automatically. See this URL for more information:
				// https://www.ibm.com/support/knowledgecenter/SSGU8G_11.70.0/com.ibm.odbc.doc/ids_odbc_222.htm
				// Note that if this fix doesn't work, the fallback will be to add a "tracker"
				// array to keep track of whether a given ODBC operation has actually retrieved
				// any data; until this happens, don't execute SQLCloseCursor() for the operation.
				// DonR 21Jun2020: It appears that changing to SQLCloseCursor() doesn't help - and
				// not closing cursors doesn't work either. So let's try another workaround: if
				// we're in Informix mode and we haven't yet seen a successful FETCH, set the statement
				// to un-PREPARED so at least we'll anticipate the problem rather than having to 
				// detect and repair it.
//				result = SQLFreeStmt (*ThisStatementPtr, SQL_CLOSE);
				result = SQLCloseCursor (*ThisStatementPtr);
				StatementOpened [OperationIdentifier_in] = 0;

				// If necessary, also close the statement for the mirrored operation.
				if (CommandIsMirrored [OperationIdentifier_in])
				{
					result = SQLCloseCursor (*MirrorStatementPtr);
				}
			}
		
		}	// "Sticky" statements get a "soft" close-cursor, if necessary.
		else
		{
			// "Non-sticky" statements need to be completely freed after use.
			NeedToFreeStatement = 1;
		}
	}	// The current command is one that may require some kind of close - either "hard" or "soft".


	if (NeedToFreeStatement)
	{
		result = SQLFreeHandle (SQL_HANDLE_STMT, *ThisStatementPtr);
		// if (result) printf("Normal SQLFreeHandle for Op %d returns %d.\n", OperationIdentifier_in, result);

		// If necessary, also free the statement for the mirrored operation.
		if (CommandIsMirrored [OperationIdentifier_in])
		{
			result = SQLFreeHandle (SQL_HANDLE_STMT, *MirrorStatementPtr);
		}

		// If the calling routine is forcibly freeing a sticky statement, decrement the
		// number-of-open-sticky-statements counter.
		if ((StatementIsSticky [OperationIdentifier_in]) && (StatementPrepared [OperationIdentifier_in]))
		{
//			StatementIsSticky	[OperationIdentifier_in] = 0; NOT NECESSARY!
			NumStickyHandlesUsed--;
		}

		// Reset StatementPrepared (and StatementOpened) so we know we need to set this statement up again next time.
		// DonR 29Mar2020: Use simple subscript rather than UseStatementSubscript[].
		StatementPrepared	[OperationIdentifier_in] = 0;
		StatementOpened		[OperationIdentifier_in] = 0;

		// DonR 06Aug2020: If I'm remembering correctly, the following three statements are
		// supposed to be the more "modern" equivalent of SQLFreeHandle. We might want to
		// try playing with them in MS-SQL - or maybe just leave well enough alone.
//		result = SQLFreeStmt(*Statement, SQL_CLOSE);  
//		result = SQLFreeStmt(*Statement, SQL_UNBIND);  
//		result = SQLFreeStmt(*Statement, SQL_RESET_PARAMS);  
	}	// Statement needs a "hard close", either because it's non-sticky or because the calling routine asked for it.


	// Clean up and go home!
	va_end (ArgList);

	// If the caller has sent a non-null address to store it, send back
	// the location of the lengths-read array. While we're at it, also
	// copy that location into a global variable, ColumnOutputLengths,
	// so calling functions can be lazy and not bother using GetLengthsRead().
	ColumnOutputLengths = &FieldLengthsRead[0];
	if (LengthReadArrayPtr_in != NULL)
		*LengthReadArrayPtr_in = &FieldLengthsRead[0];

	ODBC_AvoidRecursion		= 0;
	ODBC_ValidatingPointers	= 0;

	return (SQLCODE);
}	// ODBC_Exec() end.



int SQL_GetMainOperationParameters (	ODBC_DB_HEADER		*Database,
										int					OperationIdentifier_in,
										char				**SQL_CommandText_out,
										int					*NumOutputColumns_out,
										ODBC_ColumnParams	**OutputColumns_out,
										int					*NumInputColumns_out,
										ODBC_ColumnParams	**InputColumns_out,
										int					*NeedsWhereClauseIdentifier_out,
										int					*GenerateVALUES_out,
										short				*StatementIsSticky_out,
										short				*Convert_NF_to_zero_out,
										short				*CommandIsMirrored_out,
										short				*SuppressDiagnostics_out,
										char				**CursorName_out,
										char				**ErrorDescription_out	)
{
	// Output variables.
	static char					*SQL_CommandText;
	static int					NumOutputColumns			= 0;
	static int					NumInputColumns				= 0;
	static int					NeedsWhereClauseID			= 0;
	static int					GenerateVALUES				= 0;
	static int					StatementIsSticky			= 0;
	static int					Convert_NF_to_zero			= 0;
	static int					CommandIsMirrored			= 0;
	static int					LastOperationID				= -1;
	static ODBC_DB_HEADER		*LastDbHeader				= NULL;
	static short				SuppressDiagnostics			= 0;
	static char					*CursorName					= NULL;
	static ODBC_ColumnParams	OutputColumns	[ODBC_MAX_PARAMETERS];
	static ODBC_ColumnParams	InputColumns	[ODBC_MAX_PARAMETERS];
	static char					ErrorBuffer		[500];
	char						*OutputColumnSpec;
	char						*InputColumnSpec;
	int							RetVal						= 0;
	int							SameOpAsLastTime;


	// Before we do anything, assume we have no error to report.
	RetVal	= 0;
	strcpy (ErrorBuffer, "");

	// Check whether this is the same operation as the last one requested. If so,
	// we don't have to re-parse the input/output column specifications.
	// DonR 02Jun2020: We've been testing with this feature disabled - and the
	// disabled version is currently being pilot-tested in Production. Now it's
	// time to re-enable the feature and see (in Test!) if it works OK. The
	// actual impact on performance is unknown at this point.
	// DonR 31Aug2020: This shouldn't happen in real life, but in testing it's
	// possible that we'll use both databases for an operation that has different
	// SQL for different databases (like getting the DB timestamp). To prevent
	// problems, treat a change of database the same as a change of Operation ID.
	SameOpAsLastTime = ((OperationIdentifier_in == LastOperationID) && (Database == LastDbHeader));
//	SameOpAsLastTime = 0;	// TEMPORARY FOR TESTING


	// If this function is called multiple times for the same operation ID (which happens,
	// for example, with cursor-reading loops that use FETCH INTO), there's no need to
	// repeat the analysis - everything relevant is stored in static variables, so we
	// don't have to re-assign anything. This should give us a little performance boost.
	if (!SameOpAsLastTime)
	{
		LastOperationID = OperationIdentifier_in;
		LastDbHeader	= Database;

		// Set some additional default values.
		// By default, the Cursor Name output variable gets a value of NULL.
		// Only cursors that will have FOR CURRENT OF operations performed
		// on them need an explicitly defined cursor name.
		NumOutputColumns			=	0;
		NumInputColumns				=	0;
		NeedsWhereClauseID			=	0;
		GenerateVALUES				=	0;
		StatementIsSticky			=	0;
		Convert_NF_to_zero			=	0;
		CommandIsMirrored			=	0;
		SuppressDiagnostics			=	0;
		OutputColumnSpec			=	NULL;
		InputColumnSpec				=	NULL;
		CursorName					=	NULL;
		memset ((char *)OutputColumns, 0, ODBC_MAX_PARAMETERS * sizeof(ODBC_ColumnParams));
		memset ((char *)InputColumns,  0, ODBC_MAX_PARAMETERS * sizeof(ODBC_ColumnParams));

		// Set variables based on the SQL operation specified.
		switch (OperationIdentifier_in)
		{
			#include "MacODBC_MyOperators.c"

			default:
						RetVal	= -1;
						GerrLogMini (GerrId, "SQL_GetMainOperationParameters: Unrecognized SQL operation %d (max = %d).",
									 OperationIdentifier_in, (ODBC_MAXIMUM_OPERATION - 1));
						break;

		}	// Switch (OperationIdentifier_in)

		// Interpret input/output column specifications.
		if ((RetVal == 0) && (NumOutputColumns > 0) && (!SameOpAsLastTime))
		{
			RetVal = ParseColumnList (OutputColumnSpec, NumOutputColumns, OutputColumns);
		}

		if ((RetVal == 0) && (NumInputColumns > 0) && (!SameOpAsLastTime))
		{
			RetVal = ParseColumnList (InputColumnSpec, NumInputColumns, InputColumns);
		}

	}	// The current operation is NOT the same as the last one.

	// DonR 21Jun2020: Added the new (optional) Convert_NF_to_zero flag. if this is set TRUE,
	// a return code of 100 (= not found) will be converted to a return code of 0 (= OK)
	// with the returned column set to zero. This should be used ONLY when the entire output
	// of a SELECT is a COUNT function - but just in case, let's force it FALSE if that
	// condition isn't met.
	if (Convert_NF_to_zero)
	{
		if ((NumOutputColumns != 1) || (OutputColumns[0].type != SQL_C_SLONG))
			Convert_NF_to_zero = 0;
	}

	// Move results to output variables.
	if (RetVal == 0)
	{
		*SQL_CommandText_out			= SQL_CommandText;
		*NumOutputColumns_out			= NumOutputColumns;
		*NumInputColumns_out			= NumInputColumns;
		*NeedsWhereClauseIdentifier_out	= NeedsWhereClauseID;
		*GenerateVALUES_out				= GenerateVALUES;
		*StatementIsSticky_out			= StatementIsSticky;
		*Convert_NF_to_zero_out			= Convert_NF_to_zero;
		*CommandIsMirrored_out			= CommandIsMirrored;
		*SuppressDiagnostics_out		= SuppressDiagnostics;
		*CursorName_out					= CursorName;
		*ErrorDescription_out			= ErrorBuffer;
		*OutputColumns_out				= (NumOutputColumns	> 0) ? OutputColumns	: NULL;
		*InputColumns_out				= (NumInputColumns	> 0) ? InputColumns		: NULL;
	}
	else
	{
		*SQL_CommandText_out			= "";
		*NumOutputColumns_out			= 0;
		*NumInputColumns_out			= 0;
		*NeedsWhereClauseIdentifier_out	= 0;
		*GenerateVALUES_out				= 0;
		*StatementIsSticky_out			= StatementIsSticky;
		*Convert_NF_to_zero_out			= Convert_NF_to_zero;
		*CommandIsMirrored_out			= CommandIsMirrored;
		*SuppressDiagnostics_out		= SuppressDiagnostics;
		*CursorName_out					= NULL;
		*ErrorDescription_out			= ErrorBuffer;
		*OutputColumns_out				= NULL;
		*InputColumns_out				= NULL;
	}


	return (RetVal);
}	// SQL_GetMainOperationParameters() end.



int SQL_GetWhereClauseParameters	(	long				WhereClauseIdentifier_in,
										char				**WhereClauseText_out,
										int					*NumInputColumns_out,
										ODBC_ColumnParams	**InputColumns_out,
										char				**ErrorDescription_out	)
{
	// Output variables.
	static char					*WhereClauseText;
	static int					NumInputColumns			= 0;
	static int					LastOperationID			= -1;
	static ODBC_ColumnParams	InputColumns	[100];
	static char					ErrorBuffer		[500];
	char						*InputColumnSpec;
	int							RetVal;
	int							SameOpAsLastTime;

	// Before we do anything, assume we have no error to report.
	RetVal	= 0;
	strcpy (ErrorBuffer, "");

	// Check whether this is the same operation as the last one requested. If so,
	// we don't have to re-parse the input/output column specifications.
	SameOpAsLastTime = (WhereClauseIdentifier_in == LastOperationID);


	// If this function is called multiple times for the same operation ID (which happens,
	// for example, with cursor-reading loops that use FETCH INTO), there's no need to
	// repeat the analysis - everything relevant is stored in static variables, so we
	// don't have to re-assign anything. This should give us a little performance boost.
	if (!SameOpAsLastTime)
	{
		LastOperationID = WhereClauseIdentifier_in;

		// Set some additional default values.
		NumInputColumns				=	0;
		InputColumnSpec				=	NULL;
		memset ((char *)InputColumns,  0, 100 * sizeof(ODBC_ColumnParams));

		// WORKINGPOINT: Would it be a nice idea to count question marks in the SQL command
		// text, so NumInputColumns could be set automatically? Or would it just slow things
		// down for no really good reason?

		// Set variables based on the SQL operation specified.
		switch (WhereClauseIdentifier_in)
		{
		#include "MacODBC_MyCustomWhereClauses.c"

		default:
					RetVal	= -1;
					GerrLogMini (GerrId, "Unrecognized WHERE clause identifier %d (max = %d).", WhereClauseIdentifier_in, (ODBC_MAXIMUM_WHERE_CLAUSE - 1));
					break;

		}	// Switch (OperationIdentifier_in)


		// Interpret input column specifications.
		if ((RetVal == 0) && (NumInputColumns > 0) && (!SameOpAsLastTime))
		{
			RetVal = ParseColumnList (InputColumnSpec, NumInputColumns, InputColumns);
		}

	}	// The current operation is NOT the same as the last one.


	// Move results to output variables.
	if (RetVal == 0)
	{
		*WhereClauseText_out			= WhereClauseText;
		*NumInputColumns_out			= NumInputColumns;
		*InputColumns_out				= (NumInputColumns	> 0) ? InputColumns		: NULL;
		*ErrorDescription_out			= ErrorBuffer;
	}
	else
	{
		*WhereClauseText_out			= "";
		*NumInputColumns_out			= 0;
		*InputColumns_out				= NULL;
		*ErrorDescription_out			= ErrorBuffer;
	}

	return (RetVal);
}	// SQL_GetWhereClauseParameters() end.



int SQL_CustomizePerDB (ODBC_DB_HEADER	*Database, char *SQL_Command_in, int *CommandLength_out)
{
	char	MyBuffer	[6000];
	char	*FirstSearchTerms[]			= {"{FIRST}", "{TOP}"};
	int		NumFirstSearchTerms			= 2;
//	char	*RowidSearchTerms[]			= {"{ROWID}", "{ROW_ID}", "{ROW-ID}"};
//	int		NumRowidSearchTerms			= 3;
	char	*TransactionSearchTerms[]	= {"{TRANSACTION}"};
	int		NumTransactionSearchTerms	= 1;
	char	*GroupSearchTerms[]			= {"{GROUP}"};
	int		NumGroupSearchTerms			= 1;
	int		SearchTerm;
	int		len;
	int		MadeSubstitution		= 0;
	char	*FoundSearchTerm;

	// DonR 03Nov2020: Add support for "generic modulo" operator.
	char	*ModuloSearchTerms[]		= {"{MODULO}", "{MOD}"};
	int		NumModuloSearchTerms		= 2;
	char	*BufPtr;
	char	*Token;
	char	*Delim = " \t,()";
	char	Dividend		[65];
	char	Divisor			[65];


	// COMPILATION NOTE: strcasestr() is not part of standard C, so for this to
	// compile properly we need to add -D_GNU_SOURCE to the compile command.
	// (Alternatively, "#define _GNU_SOURCE" could be added to the source code.
	// Or both.)
	// DonR 07Jul2020: I note that for now, the strcasestr version is disabled -
	// and I don't recall why. Maybe let's revive that option for the MS-SQL
	// version, since we'll be using new makefiles in any case.
	// DonR 06Aug2020: Re-enabling the strcasestr version - let's see if it
	// compiles and works.


	// DonR 03Nov2020: Parse {MODULO} text and construct a version that works for the
	// required database. For Informix, {MODULO} A B translates to "MOD(A,B)"; for
	// MS-SQL, it translates to "(A % B)". Note that at least for now, the "default"
	// translation (which should never really happen) uses the MOD(A,B) syntax, since
	// that appears to be more common than the MS-SQL style.
	for (SearchTerm = 0; SearchTerm < NumModuloSearchTerms; SearchTerm++)
	{
		while ((FoundSearchTerm = strcasestr (SQL_Command_in, ModuloSearchTerms[SearchTerm])) != NULL)
		{
			memset	(MyBuffer, (char)0, sizeof (MyBuffer));
			len = FoundSearchTerm - SQL_Command_in;
			strncpy	(MyBuffer, SQL_Command_in, len);

			// Strip the next 2 tokens after {MODULO}/{MOD} - these are the
			// dividend and the divisor, respectively. Note that we use
			// strtok_r() here so that afterwards we'll have a pointer to
			// the remainder of the SQL command buffer. Note also that in
			// the unlikely event that there aren't enough tokens left in
			// the SQL command line, we add values that are pretty much
			// guaranteed to create a command parsing error.
			BufPtr = FoundSearchTerm + strlen (ModuloSearchTerms[SearchTerm]);	// = first byte after {MODULO}/{MOD}.
			Token = strtok_r (BufPtr, Delim, &BufPtr);
			strcpy (Dividend,	(Token != NULL) ? Token : "MISSING DIVIDEND");
			Token = strtok_r (BufPtr, Delim, &BufPtr);
			strcpy (Divisor,	(Token != NULL) ? Token : "MISSING DIVISOR");

			switch (Database->Provider)
			{
				case	ODBC_Informix:		sprintf (MyBuffer + len, "MOD (%s, %s) %s",	Dividend, Divisor, BufPtr);
											break;

				case	ODBC_MS_SqlServer:	sprintf (MyBuffer + len, "(%s %% %s) %s",	Dividend, Divisor, BufPtr);
											break;
			
				default:					sprintf (MyBuffer + len, "MOD (%s, %s) %s",	Dividend, Divisor, BufPtr);
											break;
			}

			strcpy (SQL_Command_in, MyBuffer);
// GerrLogMini (GerrId, "Modulo substitution result = {%s}.", SQL_Command_in);

			MadeSubstitution++;
		}
	}


	// Convert {TOP} and {FIRST} to the version that works for the database we're using.
	for (SearchTerm = 0; SearchTerm < NumFirstSearchTerms; SearchTerm++)
	{
		while ((FoundSearchTerm = strcasestr (SQL_Command_in, FirstSearchTerms[SearchTerm])) != NULL)
		{
			memset	(MyBuffer, (char)0, sizeof (MyBuffer));
			len = FoundSearchTerm - SQL_Command_in;
			strncpy	(MyBuffer, SQL_Command_in, len);

			switch (Database->Provider)
			{
				case	ODBC_Informix:		strcpy (MyBuffer + len, "FIRST");
											len += 5;
											break;

				case	ODBC_MS_SqlServer:	strcpy (MyBuffer + len, "TOP");
											len += 3;
											break;
			
				default:					strcpy (MyBuffer + len, "TOP");
											len += 3;
											break;
			}

			strcpy (MyBuffer + len, FoundSearchTerm + strlen (FirstSearchTerms[SearchTerm]));

			strcpy (SQL_Command_in, MyBuffer);

			MadeSubstitution++;
		}
	}

	// DonR 06Sep2020: Eliminating support for ROWID, since MS-SQL doesn't support it and
	// use of physical row-ID's is not considered good SQL practice even in databases
	// that support it.
//	for (SearchTerm = 0; SearchTerm < NumRowidSearchTerms; SearchTerm++)
//	{
//		while ((FoundSearchTerm = strcasestr (SQL_Command_in, RowidSearchTerms[SearchTerm])) != NULL)
//		while ((FoundSearchTerm = strstr (SQL_Command_in, RowidSearchTerms[SearchTerm])) != NULL)
//		{
//			memset	(MyBuffer, (char)0, sizeof (MyBuffer));
//			len = FoundSearchTerm - SQL_Command_in;
//			strncpy	(MyBuffer, SQL_Command_in, len);
//
//			switch (Database->Provider)
//			{
//				case	ODBC_Informix:		strcpy (MyBuffer + len, "ROWID");
//											len += 5;
//											break;
//
//				case	ODBC_MS_SqlServer:	strcpy (MyBuffer + len, "(CAST (%%PHYSLOC%% AS BIGINT))");
//											len += 30;
//											break;
//			
//				default:					strcpy (MyBuffer + len, "(CAST (%%PHYSLOC%% AS BIGINT))");
//											len += 30;
//											break;
//			}
//
//			strcpy (MyBuffer + len, FoundSearchTerm + strlen (RowidSearchTerms[SearchTerm]));
//
//			strcpy (SQL_Command_in, MyBuffer);
//GerrLogMini (GerrId, "Converted {rowid} command to:\n%s", SQL_Command_in);
//
//			MadeSubstitution++;
//		}
//	}

	// WORKINGPOINT: The "TRANSACTION" and "GROUP" logic work the same way - maybe they
	// should be combined into a single "generic" substitution? Or maybe it's not worth
	// the trouble...

	// Informix allows columns named "TRANSACTION", but MS-SQL treats that as a keyword;
	// accordingly, for MS-SQL we need to add double-quotes around the word. (Informix
	// doesn't like the double-quotes, so this has to be a "curly-bracket substitution".)
	for (SearchTerm = 0; SearchTerm < NumTransactionSearchTerms; SearchTerm++)
	{
		while ((FoundSearchTerm = strcasestr (SQL_Command_in, TransactionSearchTerms[SearchTerm])) != NULL)
		{
			memset	(MyBuffer, (char)0, sizeof (MyBuffer));
			len = FoundSearchTerm - SQL_Command_in;
			strncpy	(MyBuffer, SQL_Command_in, len);

			switch (Database->Provider)
			{
				case	ODBC_Informix:		strcpy (MyBuffer + len, "TRANSACTION");
											len += 11;
											break;

				case	ODBC_MS_SqlServer:	strcpy (MyBuffer + len, "\"TRANSACTION\"");
											len += 13;
											break;
			
				default:					strcpy (MyBuffer + len, "\"TRANSACTION\"");
											len += 13;
											break;
			}

			strcpy (MyBuffer + len, FoundSearchTerm + strlen (TransactionSearchTerms[SearchTerm]));

			strcpy (SQL_Command_in, MyBuffer);

			MadeSubstitution++;
		}
	}

	// Informix allows columns named "GROUP", but MS-SQL treats that as a keyword;
	// accordingly, for MS-SQL we need to add double-quotes around the word. (Informix
	// doesn't like the double-quotes, so this has to be a "curly-bracket substitution".)
	for (SearchTerm = 0; SearchTerm < NumGroupSearchTerms; SearchTerm++)
	{
		while ((FoundSearchTerm = strcasestr (SQL_Command_in, GroupSearchTerms[SearchTerm])) != NULL)
		{
			memset	(MyBuffer, (char)0, sizeof (MyBuffer));
			len = FoundSearchTerm - SQL_Command_in;
			strncpy	(MyBuffer, SQL_Command_in, len);

			switch (Database->Provider)
			{
				case	ODBC_Informix:		strcpy (MyBuffer + len, "GROUP");
											len += 5;
											break;

				case	ODBC_MS_SqlServer:	strcpy (MyBuffer + len, "\"GROUP\"");
											len += 7;
											break;
			
				default:					strcpy (MyBuffer + len, "\"GROUP\"");
											len += 7;
											break;
			}

			strcpy (MyBuffer + len, FoundSearchTerm + strlen (GroupSearchTerms[SearchTerm]));

			strcpy (SQL_Command_in, MyBuffer);

			MadeSubstitution++;
		}
	}

	// If and only if we actually did something, update the command length.
	if ((MadeSubstitution > 0) && (CommandLength_out != NULL))
	{
		*CommandLength_out = strlen (SQL_Command_in);
	}

	return (0);
}	// SQL_CustomizePerDB() end.



int ParseColumnList (	char				*SpecStringIn,
						int					NumColumnsIn,
						ODBC_ColumnParams	*ParamsOut		)
{
	static char	buffer [4000];
	char		*BufPtr;
	char		*Token;
	char		*Delim = " \t,[]()";
	int			ParametersDefined		= 0;

	// Start the tokenizing and get the first token.
	strcpy (buffer, SpecStringIn);
	BufPtr = &buffer[0];
	Token = strtok (BufPtr, Delim);
	BufPtr = NULL;

	while (ParametersDefined < NumColumnsIn)
	{
		if (Token != NULL)
		{
			do
			{
				// NOTE: I've put these strcasecmp's in order more or less by how frequently
				// they're likely to be used - with the idea that the fewer string comparisons
				// we have to do, the faster this thing will run.

				if ((!strcasecmp (Token, "INT")) || (!strcasecmp (Token, "INTEGER")))
				{
					ParamsOut [ParametersDefined].type		= SQL_C_SLONG;
					ParamsOut [ParametersDefined].length	= 0;
					ParametersDefined++;
					break;
				}

				if ((!strcasecmp (Token, "SHORT")) || (!strcasecmp (Token, "SMALLINT")))
				{
					ParamsOut [ParametersDefined].type		= SQL_C_SSHORT;
					ParamsOut [ParametersDefined].length	= 0;
					ParametersDefined++;
					break;
				}

				if (!strcasecmp (Token, "CHAR"))
				{
					ParamsOut [ParametersDefined].type		= SQL_C_CHAR;

					Token = strtok (BufPtr, Delim);
					if (Token != NULL)
					{
						ParamsOut [ParametersDefined].length = atoi (Token);

						// For single-character SELECTed fields that are to be mapped to a simple character
						// variable (i.e. "char VARNAME" as opposed to "char VARNAME[2]"), specify the field
						// as CHAR(0). (Alternately, use "SINGLE_CHAR" or "ONECHAR".) In this case, we'll
						// bind the output to an internal buffer, and then copy just the "payload" character
						// to the actual output variable.
						// DonR 06Jul2020: We don't need to count the single-character output columns anymore -
						// the main routine just looks for character fields of length 0.
						ParametersDefined++;
					}
					break;
				}

				// Note: VarChar is necessary only for input parameters (and I'm
				// not sure it's really all that necessary even then).
				if (!strcasecmp (Token, "VARCHAR"))
				{
					ParamsOut [ParametersDefined].type		= SQL_VARCHAR;

					Token = strtok (BufPtr, Delim);
					if (Token != NULL)
					{
						ParamsOut [ParametersDefined].length = atoi (Token);
						ParametersDefined++;
					}
					break;
				}

				// Almost all our decimal numbers are double-precision - so SQL FLOAT
				// gets mapped to SQL_C_DOUBLE. For those few cases where we have a
				// non-double decimal number, use REAL, which maps to SQL_C_FLOAT.
				if ((!strcasecmp (Token, "DOUBLE")) ||(!strcasecmp (Token, "FLOAT")))
				{
					ParamsOut [ParametersDefined].type		= SQL_C_DOUBLE;
					ParamsOut [ParametersDefined].length	= 0;
					ParametersDefined++;
					break;
				}

				if ((!strcasecmp (Token, "LONG")) || (!strcasecmp (Token, "BIGINT")))
				{
					ParamsOut [ParametersDefined].type		= SQL_C_SBIGINT;
					ParamsOut [ParametersDefined].length	= 0;
					ParametersDefined++;
					break;
				}

				// DonR 10Jul2025: Added support for booleans. Bool is defined as an "enum", which
				// means that the compiler determines the size of the variable. To avoid problems,
				// when the ODBC stuff initializes we'll test the size of "bool", and set a variable
				// up to give us the correct integer format for booleans.
				if ((!strcasecmp (Token, "BOOL")) || (!strcasecmp (Token, "BOOLEAN")))
				{
					ParamsOut [ParametersDefined].type		= BoolType;
					ParamsOut [ParametersDefined].length	= 0;
					ParametersDefined++;
					break;
				}

				if (!strcasecmp (Token, "UNSIGNED"))
				{
					ParamsOut [ParametersDefined].type		= SQL_C_ULONG;
					ParamsOut [ParametersDefined].length	= 0;
					ParametersDefined++;
					break;
				}

				if (!strcasecmp (Token, "USHORT"))
				{
					ParamsOut [ParametersDefined].type		= SQL_C_USHORT;
					ParamsOut [ParametersDefined].length	= 0;
					ParametersDefined++;
					break;
				}

				// NOTE: Because a database "float" is normally a double-precision number
				// in our C code, use "REAL" to designate an ordinary float number. (There
				// are hardly any in our code.)
				if (!strcasecmp (Token, "REAL"))
				{
					ParamsOut [ParametersDefined].type		= SQL_C_FLOAT;
					ParamsOut [ParametersDefined].length	= 0;
					ParametersDefined++;
					break;
				}

				if ((!strcasecmp (Token, "SINGLE-CHAR"))	|| (!strcasecmp (Token, "SINGLE_CHAR")) || (!strcasecmp (Token, "SINGLECHAR"))		||
					(!strcasecmp (Token, "ONE-CHAR"))		|| (!strcasecmp (Token, "ONE_CHAR"))	|| (!strcasecmp (Token, "ONECHAR")))
				{
					// For single-character SELECTed fields that are to be mapped to a simple character
					// variable (i.e. "char VARNAME" as opposed to "char VARNAME[2]"), specify the field
					// as CHAR(0). (Alternately, use "SINGLE-CHAR" or "ONECHAR", with some punctuation
					// variations permitted.) In this case, we'll bind the output to an internal buffer,
					// and then copy just the "payload" character to the actual output variable.
					// DonR 06Jul2020: We don't need to count the single-character output columns anymore -
					// the main routine just looks for character fields of length 0.
					ParamsOut [ParametersDefined].type		= SQL_C_CHAR;
					ParamsOut [ParametersDefined].length	= 0;
					ParametersDefined++;
					break;
				}


				// SQL_C_WCHAR			SQL_WCHAR *				wchar_t *		(NOT SET UP YET)
				// SQL_C_BIT			SQLCHAR					unsigned char
				// SQL_C_STINYINT		SQLSCHAR				signed char
				// SQL_C_UTINYINT		SQLCHAR					unsigned char
				// SQL_C_UBIGINT		SQLUBIGINT				unsigned long	(NOT SET UP YET)
				// SQL_C_BINARY			SQLCHAR *				unsigned char *	(NOT SET UP YET)
				//
				// DATETIME / INTERVAL

				GerrLogMini (GerrId, "Unrecognized field-spec token {%s} for parameter %d!\n", Token, ParametersDefined + 1);
				break;
			}
			while (0);

			if (Token == NULL)
			{
				GerrLogMini (GerrId, "Field specification missing string length for parameter %d.\n", ParametersDefined + 1);
				break;
			}

		}	// Token != NULL

		else
		{
			GerrLogMini (GerrId, "Unexpected end of field specification - expected %d params, got only %d.\n", NumColumnsIn, ParametersDefined);
			break;
		}
	
		// Get next token.
		Token = strtok (BufPtr, Delim);

	}	// while (ParametersDefined < NumColumnsIn)

	if (ParametersDefined != NumColumnsIn)
		GerrLogMini (GerrId, "ParseColumnList: ParametersDefined = %d, NumColumnsIn = %d - mismatch!", ParametersDefined, NumColumnsIn);

	return (ParametersDefined != NumColumnsIn);
}	// ParseColumnList() end.



int find_FOR_UPDATE_or_GEN_VALUES	(	int		OperationIdentifier,
										char	*SQL_command_in,
										int		*FoundForUpdate_out,
										short	*FoundInsert_out,
										int		*FoundSelect_out,
										int		*FoundCustomWhereInsertionPoint_out	)
{
	static char	buffer [4000];
	char		*BufPtr;
	char		*Token;
	char		*Delim = " \t)";	// DonR 05Apr2020: Added close-paren to the delimiter list; it should be relevant only
									// for the custom WHERE insertion point, which can sometimes look like "... %s)".
	int			FirstToken						= 1;
	int			FoundForUpdate					= 0;
	int			FoundSelect						= 0;
	int			FoundFor						= 0;
	int			FoundCustomWhereInsertionPoint	= 0;
	short		FoundInsert						= 0;


	// DonR 16Mar2020: In some cases (like a FETCH INTO when the cursor was
	// DECLARED using a mainline-created SQL string) the SQL command string
	// passed to this function was NULL. That, needless to say, caused
	// problems! The solution is simply to make most of find_FOR_UPDATE_or_GEN_VALUES
	// operate only on a string that is non-NULL.
	if (SQL_command_in != NULL)
	{
		// Start the tokenizing and get the first token.
		strcpy (buffer, SQL_command_in);
		BufPtr = &buffer[0];
		Token = strtok (BufPtr, Delim);
		BufPtr = NULL;

		// We perform a break when (and if) we find the "FOR" - so we don't
		// need an additional condition for the while-loop.
		while (Token != NULL)
		{
			// If this is an INSERT statement, INSERT must be the first token in
			// the command string; if we're past that point, don't bother looking.
			if (FirstToken)
			{
				FirstToken = 0;

				if (!strcasecmp (Token, "INSERT"))
				{
					FoundInsert = 1;
				}

				if (!strcasecmp (Token, "SELECT"))
				{
					FoundSelect = 1;
				}
			}	// First token.


			// Check for custom WHERE clause insertion point - if there isn't one, we'll force
			// NeedsWhereClauseID false. (Note that this test *is* case-sensitive, since %S
			// isn't valid.)
			if (!strcmp (Token, "%s"))
				FoundCustomWhereInsertionPoint = 1;

			// If we're past the first token and this is *not* a SELECT statement, there's
			// no point in looking for a FOR UPDATE clause - it's irrelevant and shouldn't
			// be there at all. (Really, if this routine is going to clean up after all
			// possible programmer sloppiness, it should automatically *delete* an erroneous
			// FOR UPDATE clause - but I don't think we need to get that fancy.)
			// DonR 05Apr2020: Because I added ")" to the delimiter list, in theory this routine
			// could get fooled if someone wrote "FOR) UPDATE"; but in that case we'd get a syntax
			// error at execution time, so the bug would be discovered pretty easily.
			if ((FoundSelect) && (!FoundFor))
			{
				if (!strcasecmp (Token, "FOR"))
				{
					FoundFor = 1;	// So multiple "FOR" tokens won't be accepted - only the first one will work.

					// We found the "FOR" - now check if the next token is "UPDATE".
					Token = strtok (BufPtr, Delim);
					if (Token != NULL)
					{
						if (!strcasecmp (Token, "UPDATE"))
							FoundForUpdate = 1;
					}
				}	// We found a "FOR" token.
			}	// This is a SELECT statement.

			// The current token is not "FOR", so keep looking.
			Token = strtok (BufPtr, Delim);
		}	// While there are still more tokens to parse through.
//GerrLogMini(GerrId, "Found UPDATE %d, found INSERT %d, found SELECT %d.", FoundForUpdate, FoundInsert, FoundSelect);

	}	// if (SQL_command_in != NULL)

	if (FoundForUpdate_out != NULL)
		*FoundForUpdate_out = FoundForUpdate;

	if (FoundInsert_out != NULL)
		*FoundInsert_out = FoundInsert;

	if (FoundSelect_out != NULL)
		*FoundSelect_out = FoundSelect;

	if (FoundCustomWhereInsertionPoint_out != NULL)
		*FoundCustomWhereInsertionPoint_out = FoundCustomWhereInsertionPoint;

	return (0);
}



int ODBC_CONNECT (	ODBC_DB_HEADER	*Database,
					char			*DSN,
					char			*username,
					char			*password,
					char			*dbname,
					char			*timeout_str)
{
	static int	FirstTimeCalled	= 1;
	int			result			= 0;
	int			NativeError		= 0;
	int			AutoFreeDefault	= 0;
	long		ConnectTimeout	= 300;	// Seconds.
	char		StatementBuffer	[200];

	char		*ConnectResult;
	SQLHSTMT	ConnectToODBC_hstmt;		// Local statement handle for "USE" command.
	SQLHSTMT	SetTimeout_hstmt;			// Local statement handle for "SET LOCK_TIMEOUT" command.
	SQLHSTMT	SetDeadlockPriority_hstmt;	// Local statement handle for "SET DEADLOCK_PRIORITY" command.

	// Variables for use when and if we use SQLDriverConnect().
	//char		LoginInfo		[100];
	//char		ConnectInfoOut	[4096];
	//short		OutBufLength;


	if (!ODBC_henv_allocated)
//	if (FirstTimeCalled)
	{
//		if (!FirstTimeCalled)
//			GerrLogMini (GerrId, "Reconnect - re-allocating ODBC environment handle.");
//
		FirstTimeCalled			= 0;

		MS_DB.Connected			= 0;
		MS_DB.Provider			= ODBC_MS_SqlServer;
		strcpy (MS_DB.Name, "MS-SQL");

		INF_DB.Provider			= ODBC_Informix;
		INF_DB.Connected		= 0;
		strcpy (INF_DB.Name, "Informix");

		NUM_ODBC_DBS_CONNECTED	= 0;

		// Make sure this program uses the system locale - otherwise
		// Hebrew text doesn't work.
		setlocale (LC_ALL, "");

		// Allocate the global ODBC environment handle.
		// NOTE: I've made a couple of attempts to use SQLAllocHandle instead of SQLAllocEnv
		// and SQLAllocConnect - but allocating the connection always fails, even though
		// allocating the environment returns no errors.
		result = SQLAllocEnv (&ODBC_henv);
//		result = SQLAllocHandle (SQL_HANDLE_ENV, SQL_NULL_HANDLE, &ODBC_henv);
		if (SQL_FAILED (result))
		{
			NativeError = ODBC_EnvironmentError (&ODBC_henv);
			GerrLogMini (GerrId, "PID %d unable to allocate environment handle (result = %d).", getpid(), result);
			return (NativeError);
		}
		else
		{
			ODBC_henv_allocated = 1;
		}
	}	// First time called initializations.


	do
	{
//GerrLogMini (GerrId, "ODBC_CONNECT called for %s.", (Database->Provider == ODBC_MS_SqlServer) ? "MS-SQL" : "Informix");
		// Allocate a connection handle.
		result = SQLAllocConnect (ODBC_henv, &Database->HDBC);
//		result = SQLAllocHandle (SQL_HANDLE_DBC, ODBC_henv, &Database->HDBC);
		if (SQL_FAILED (result))
		{
			ODBC_DB_ConnectionError (&Database->HDBC);
			GerrLogMini (GerrId, "PID %d unable to allocate connection handle (result = %d).", getpid(), result);
			CleanupODBC (Database);
			break;
		}

		// DonR 26Aug2020: Add a new environment parameter to override the default DB
		// connect timeout value. (Note that there's no sanity-checking here!)
		if (timeout_str != NULL)
		{
			ConnectTimeout = atol (timeout_str);
		}

		// Before connecting to the database server, set some pre-connection parameters.
		// Specify a ridiculously long timeout interval, since the QA Informix database seems
		// to have a problem that causes it to respond very slowly to connection requests.
		// DonR 26Aug2020: See above - the timeout interval is now configurable.
		result = SQLSetConnectAttr (Database->HDBC, SQL_ATTR_LOGIN_TIMEOUT,			(SQLPOINTER)ConnectTimeout,				SQL_IS_UINTEGER);
		result = SQLSetConnectAttr (Database->HDBC, SQL_ATTR_CONNECTION_TIMEOUT,	(SQLPOINTER)ConnectTimeout,				SQL_IS_UINTEGER);
		result = SQLSetConnectAttr (Database->HDBC, SQL_ATTR_TXN_ISOLATION,			(SQLPOINTER)SQL_TXN_READ_UNCOMMITTED,	SQL_IS_UINTEGER);

		// DonR 23Feb2020: Try setting the connection to keep cursors open after a COMMIT - the equivalent of
		// having all cursors set to WITH HOLD. This setting is conditional, since it's needed only for situations
		// where we may have update cursors with lots of rows and periodic COMMIT's; the only place where this
		// normally happens is As400UnixClient. NOTE that Informix does not appear to have this capability.
		if ((Database->Provider == ODBC_MS_SqlServer) && (ODBC_PRESERVE_CURSORS))
		{
			result = SQLSetConnectAttr (Database->HDBC, SQL_COPT_SS_PRESERVE_CURSORS,	(SQLPOINTER)SQL_PC_ON,					SQL_IS_UINTEGER);
			if (result)
				GerrLogMini (GerrId, "PID %d: SQLSetConnectAttr SQL_COPT_SS_PRESERVE_CURSORS to SQL_PC_ON returns %d.", getpid(), result);
		}

		// Now execute the actual CONNECT.
//GerrLogMini (GerrId, "Calling SQLConnect to %s %s/%s/%s...", Database->Name, DSN, username, password);
//		result = SQLConnect (Database->HDBC, DSN, SQL_NTS, username, SQL_NTS, password, SQL_NTS);
		result = SQLConnect (Database->HDBC, DSN, (short)strlen(DSN), username, (short)strlen(username), password, (short)strlen(password));
//GerrLogMini(GerrId, "SQLConnect returned %d.", result);

		// Log connection parameters and result.
		switch (result)
		{
			case SQL_SUCCESS:			ConnectResult = "OK";			break;
			case SQL_SUCCESS_WITH_INFO:	ConnectResult = "OK w/info";	break;
			default:					ConnectResult = "FAILED";		break;
		}
// GerrLogMini (GerrId, "ConnectResult for %s = %s, SQL_FAILED (result) = %d.", Database->Name, ConnectResult, SQL_FAILED (result));

		if (SQL_FAILED (result))
		{
			// GerrLogMini (GerrId, "Connect to DSN %s as %s/%s: %s (result = %d).", DSN, username, password, ConnectResult, result);
			// GerrLogMini (GerrId, "Connection to %s failed with result %d; &Database->Connected = %ld.", Database->Name, result, (long)(&Database->Connected));
			NativeError = ODBC_DB_ConnectionError (&Database->HDBC);
			Database->Connected = 0;
			GerrLogMini(GerrId, "PID %d: Connection to %s failed with result %d, ODBC error:\n%s.", getpid(), Database->Name, result, ODBC_ErrorBuffer);


//			// Second chance: try SQLDriverConnect().
//			sprintf (LoginInfo, "DSN=%s;UID=%s;PWD=%s;DRIVER=/opt/microsoft/msodbcsql17/lib64/libmsodbcsql-17.3.so.1.1;", DB, USER, PASS);
//			sprintf (LoginInfo, "DSN=%s;UID=%s;PWD=%s;DRIVER=SQL Server;", DB, USER, PASS);
//			result = SQLDriverConnect (	hdbc,		// Connection handle.
//										NULL,		// Dialogue window (none).
//										LoginInfo,
//										strlen (LoginInfo),
//										ConnectInfoOut,
//										4000,
//										&OutBufLength,
//										SQL_DRIVER_NOPROMPT);
//
//
//			result = SQLConnect (hdbc, DSN, SQL_NTS, DSN_userid, SQL_NTS, DSN_password, SQL_NTS);
//
//			// Log connection parameters and result.
//			switch (result)
//			{
//				case SQL_SUCCESS:			ConnectResult = "OK";			break;
//				case SQL_SUCCESS_WITH_INFO:	ConnectResult = "OK w/info";	break;
//				default:					ConnectResult = "FAILED";		break;
//			}
//
//			printf ("Second-chance connect to %s: %s (result = %d).\n", LoginInfo, ConnectResult, result);
//			ODBC_DB_ConnectionError (&Database->HDBC);
		}
		else
		{
//GerrLogMini(GerrId, "ODBC Connect to %s succeeded; &Database->Connected = %ld.", Database->Name, (long)(&Database->Connected));
			Database->Connected = 1;
			NUM_ODBC_DBS_CONNECTED++;
GerrLogMini (GerrId, "PID %d: %s connected = %d, num DBs connected = %d.", getpid(), Database->Name, Database->Connected, NUM_ODBC_DBS_CONNECTED);
		}

		if (SQL_FAILED (result))
		{
			CleanupODBC (Database);
			break;
		}

		// If we get here, we *should* be connected and ready to do stuff.


		// MS-SQL requires a "USE" to select the specific database we'll be working with;
		// Informix does not.
		if (Database->Provider == ODBC_MS_SqlServer)
		{
			// Allocate a statement handle - works only after connecting to database.
			result = SQLAllocStmt (Database->HDBC, &ConnectToODBC_hstmt);
			if (SQL_FAILED (result))
			{
				GerrLogMini (GerrId, "PID %d unable to allocate statement handle (result = %d).", getpid(), result);
				CleanupODBC (Database);
				break;
			}

			// Select the specific database.
			sprintf (StatementBuffer, "USE %s", dbname);
			result = SQLPrepare (ConnectToODBC_hstmt, StatementBuffer, strlen (StatementBuffer));
			if (SQL_FAILED (result))
			{
				GerrLogMini (GerrId, "PID %d unable to prepare %s (result = %d).", getpid(), StatementBuffer, result);
				CleanupODBC (Database);
				break;
			}

			result = SQLExecute (ConnectToODBC_hstmt);	// USE command.
			if (SQL_FAILED (result))
			{
				NativeError = ODBC_StatementError (&ConnectToODBC_hstmt, 0);
				GerrLogMini (GerrId, "PID %d: %s failed (result = %d). Native error %d, error desc:\n%s.",
							 getpid(), StatementBuffer, result, NativeError, sqlca.sqlerrm);

				// If we couldn't USE the database, the CONNECT was presumably to a database that was
				// set offline or otherwise not usable. In this case, treat it as a failed CONNECT.
				// For the moment, I'm adding this CleanupODBC() call just for the USE statement; if
				// the USE works, the SET LOCK_TIMEOUT and SET DEADLOCK_PRIORITY calls should work
				// as well.
				CleanupODBC (Database);
				break;
			}
			
			SQLFreeStmt (ConnectToODBC_hstmt, SQL_CLOSE);
			// USE database done.


			// DonR 03Mar2021: Add setting of Lock Timeout and Deadlock Priority for MS-SQL.
			// Allocate a statement handle - works only after connecting to database.
			result = SQLAllocStmt (Database->HDBC, &SetTimeout_hstmt);
			if (SQL_FAILED (result))
			{
				GerrLogMini (GerrId, "PID %d unable to allocate statement handle for LOCK_TIMEOUT (result = %d).", getpid(), result);
				CleanupODBC (Database);
				break;
			}

			// Set up the command.
			sprintf (StatementBuffer, "SET LOCK_TIMEOUT %d", LOCK_TIMEOUT);
			result = SQLPrepare (SetTimeout_hstmt, StatementBuffer, strlen (StatementBuffer));
			if (SQL_FAILED (result))
			{
				GerrLogMini (GerrId, "PID %d unable to prepare %s (result = %d).", getpid(), StatementBuffer, result);
				CleanupODBC (Database);
				break;
			}

			result = SQLExecute (SetTimeout_hstmt);	// USE command.
			if (SQL_FAILED (result))
			{
				NativeError = ODBC_StatementError (&SetTimeout_hstmt, 0);
				GerrLogMini (GerrId, "PID %d: %s failed (result = %d). Native error %d, error desc:\n%s.",
							 getpid(), StatementBuffer, result, NativeError, sqlca.sqlerrm);
				break;
			}

			SQLFreeStmt (SetTimeout_hstmt, SQL_CLOSE);
			// SET TIMEOUT done.


			// Set Deadlock Priority.
			result = SQLAllocStmt (Database->HDBC, &SetDeadlockPriority_hstmt);
			if (SQL_FAILED (result))
			{
				GerrLogMini (GerrId, "PID %d unable to allocate statement handle for DEADLOCK_PRIORITY (result = %d).", getpid(), result);
				CleanupODBC (Database);
				break;
			}

			// Set up the command.
			sprintf (StatementBuffer, "SET DEADLOCK_PRIORITY %d", DEADLOCK_PRIORITY);
			result = SQLPrepare (SetDeadlockPriority_hstmt, StatementBuffer, strlen (StatementBuffer));
			if (SQL_FAILED (result))
			{
				GerrLogMini (GerrId, "PID %d unable to prepare %s (result = %d).", getpid(), StatementBuffer, result);
				CleanupODBC (Database);
				break;
			}

			result = SQLExecute (SetDeadlockPriority_hstmt);	// USE command.
			if (SQL_FAILED (result))
			{
				NativeError = ODBC_StatementError (&SetDeadlockPriority_hstmt, 0);
				GerrLogMini (GerrId, "PID %d: %s failed (result = %d). Native error %d, error desc:\n%s.",
							 getpid(), StatementBuffer, result, NativeError, sqlca.sqlerrm);
				break;
			}
			
			SQLFreeStmt (SetDeadlockPriority_hstmt, SQL_CLOSE);
			// SET DEADLOCK_PRIORITY done.

		}	// Database->Provider == ODBC_MS_SqlServer


		// We want all our database connections to be in "transaction" mode so they
		// can be rolled back if we hit a major error - so turn AUTOCOMMIT off.
//GerrLogMini (GerrId, "Calling SQLSetConnectAttr for SQL_AUTOCOMMIT_OFF...");
		result = SQLSetConnectAttr (Database->HDBC, SQL_ATTR_AUTOCOMMIT, (SQLPOINTER)SQL_AUTOCOMMIT_OFF, SQL_IS_UINTEGER);
		if (SQL_FAILED (result))
		{
			GerrLogMini (GerrId, "PID %d: Set AUTOCOMMIT off failed (result = %d).", getpid(), result);
			NativeError = ODBC_DB_ConnectionError (&Database->HDBC);
		}
//else GerrLogMini (GerrId, "Set SQL_AUTOCOMMIT_OFF succeeded.");

		// We also want to default our DB isolation level to "dirty read" to avoid
		// deadlocks. Note that ODBC does *not* allow us to change the isolation
		// level once a transaction has begun - so any SET_ISOLATION_COMMITTED
		// calls have to be executed before there's any uncommitted data.
//GerrLogMini (GerrId, "Calling SQLSetConnectAttr for SQL_TXN_READ_UNCOMMITTED...");
		result = SQLSetConnectAttr (Database->HDBC, SQL_ATTR_TXN_ISOLATION, (SQLPOINTER)SQL_TXN_READ_UNCOMMITTED, SQL_IS_UINTEGER);
		if (SQL_FAILED (result))
		{
			GerrLogMini (GerrId, "PID %d: Set initial DIRTY READ failed (result = %d).", getpid(), result);
			NativeError = ODBC_DB_ConnectionError (&Database->HDBC);
		}
//else GerrLogMini (GerrId, "Set SQL_TXN_READ_UNCOMMITTED succeeded.");

		// DonR 21Jun2020: Yet another attempt to figure out why Informix sometimes un-PREPAREs
		// sticky operations, and maybe even fix the problem.
		if (Database->Provider == ODBC_Informix)
		{
			result = SQLGetConnectAttr (Database->HDBC, SQL_INFX_ATTR_AUTO_FREE, (SQLPOINTER)&AutoFreeDefault, sizeof(int), NULL);
//			GerrLogMini (GerrId, "Default SQL_INFX_ATTR_AUTO_FREE = %d, result = %d.", AutoFreeDefault, result);
			result = SQLSetConnectAttr (Database->HDBC, SQL_INFX_ATTR_AUTO_FREE, (SQLPOINTER)SQL_FALSE, SQL_IS_USMALLINT);
			if (SQL_FAILED (result))
			{
				GerrLogMini (GerrId, "PID %d: Disable AUTOFREE for connection: result = %d.", getpid(), result);
			}
		}


//		// DonR 02Apr2020: Turn off Informix ODBC parsing for better performance.
//		// (Disabled, since I can't find a value for SQL_INFX_ATTR_SKIP_PARSING anywhere.)
//		if (Database->Provider == ODBC_Informix)
//		{
//			result = SQLSetConnectAttr (Database->HDBC, SQL_INFX_ATTR_SKIP_PARSING, (SQLPOINTER)SQL_TRUE, SQL_IS_USMALLINT);
//			if (SQL_FAILED (result))
//			{
//				GerrLogMini (GerrId, "Disable Informix ODBC parsing failed (result = %d).", result);
//				ODBC_DB_ConnectionError (&Database->HDBC);
//			}
//			else GerrLogMini (GerrId, "Disable Informix ODBC parsing succeeded.");
//		}	// Disable Informix ODBC parsing.

//		// Temporary - get some driver information.
//		SQLGetInfo (hdbc, SQL_MAX_ASYNC_CONCURRENT_STATEMENTS, &result, 4, &ErrorLength);
//		printf ("ODBC max concurrent statements = %d.\n", result);
//		SQLGetInfo (hdbc, SQL_MAX_CONCURRENT_ACTIVITIES, &result, 4, &ErrorLength);
//		printf ("ODBC max concurrent activities = %d.\n", result);
//		SQLGetInfo (hdbc, SQL_MAX_DRIVER_CONNECTIONS, &result, 4, &ErrorLength);
//		printf ("ODBC max driver connections = %d.\n", result);

	}
	while (0);

	// DonR 07Oct2021: If Native Error has a non-zero value, return that; otherwise, if there was
	// an error of some kind, return whatever's in "result" - normally -1.
	if ((NativeError == 0) && (SQL_FAILED (result)))
		NativeError = result;

	return (NativeError);
}	// ODBC_CONNECT() end.



int CleanupODBC (ODBC_DB_HEADER	*Database)
{
//	GerrLogMini (GerrId, "CleanupODBC for %s: Connected = %d, DBs connected = %d.",
//				 Database->Name, Database->Connected, NUM_ODBC_DBS_CONNECTED);
//
	// Disconnect from the database and free all handles
	if (Database->Connected)
	{
		SQLDisconnect	(Database->HDBC);
		SQLFreeConnect	(Database->HDBC);
		Database->Connected = 0;

		if (--NUM_ODBC_DBS_CONNECTED < 1)
		{
			SQLFreeEnv	(ODBC_henv);
			ODBC_henv_allocated		= 0;
			NUM_ODBC_DBS_CONNECTED	= 0;	// Shouldn't ever go below zero, but just in case!
		}
	}

	return (0);
}	// CleanupODBC() end.



int ODBC_ErrorHandler				(	int			ErrorCategory,
										SQLHENV		*Environment,
										SQLHDBC		*Database,
										SQLHSTMT	*Statement,
										int			OperationIdentifier		)
{
	char		*OriginalSQLCommand = NULL;
	char		ODBC_StateBuffer	[100];
	char		ODBC_ErrorText		[4096];
	char		*ErrCategoryDesc;
	int			ODBC_NativeError;
	short		ODBC_ErrorLength;
	int			errm_len;

	// Dummy parameters for retrieving original SQL command text.
	int					DummyInteger;
	short				DummyShort;
	ODBC_ColumnParams	*DummyColumnPointer;
	char				*DummyString;

	switch (ErrorCategory)
	{
	case ODBC_ENVIRONMENT_ERR:
				ErrCategoryDesc = "Environment error";
				SQLGetDiagRec (SQL_HANDLE_ENV, *Environment, 1, ODBC_StateBuffer, &ODBC_NativeError, ODBC_ErrorText, 4095, &ODBC_ErrorLength);
				break;


	case ODBC_DB_HANDLE_ERR:
				ErrCategoryDesc = "Database handle error";
				SQLGetDiagRec (SQL_HANDLE_DBC, *Database, 1, ODBC_StateBuffer, &ODBC_NativeError, ODBC_ErrorText, 4095, &ODBC_ErrorLength);
				break;


	case ODBC_STATEMENT_ERR:	// (macro call is ODBC_StatementError.)
				ErrCategoryDesc = "Statement error";
				if (OperationIdentifier > 0)
				{
					// DonR 25Aug2020: Added a new database-pointer argument at the beginning of the argument list
					// for SQL_GetMainOperationParameters. This is actually relevant for only one SQL operation -
					// READ_GetCurrentDatabaseTime, which gets a timestamp value from the database and has to use
					// completely different SQL for MS-SQL than it uses for Informix. here in the error-handling
					// routine, we don't actually have to worry about which DB-structure pointer we pass to
					// SQL_GetMainOperationParameters(), so just send it MAIN_DB.
					SQL_GetMainOperationParameters (	MAIN_DB,
														OperationIdentifier,	// SQL Operation ID.
														&OriginalSQLCommand,	// Original SQL command text - the only output we're interested in!
														&DummyInteger,			// Number of output columns.
														&DummyColumnPointer,	// Output column pointer.
														&DummyInteger,			// Number of input columns.
														&DummyColumnPointer,	// Input column pointer.
														&DummyInteger,			// Needs custom WHERE clause.
														&DummyInteger,			// Auto-build VALUES.
														&DummyShort,			// Statement is "sticky".
														&DummyShort,			// Convert not-found to zero for COUNT operations.
														&DummyShort,			// Mirror this statement to alternate database.
														&DummyShort,			// Suppress diagnostics (e.g. for DROP TABLE commands).
														&DummyString,			// Cursor Name.
														&DummyString	);		// Error string.
				}

				SQLGetDiagRec (SQL_HANDLE_STMT, *Statement, 1, ODBC_StateBuffer, &ODBC_NativeError, ODBC_ErrorText, 4095, &ODBC_ErrorLength);
//				// TEMPORARY TESTING CODE!
//				if (OperationIdentifier == IsPharmOpen_READ_pharmacy)
//				{
//					if (ForceDbErrorTenthTime == 10)
//					{
//						ODBC_NativeError = SQL_TRN_CLOSED_BY_OTHER_SESSION;
//					}
//				}

				// DonR 02Jun2020: The SQLNumParams check should catch problems relating to "sticky" statements
				// that have somehow gotten un-PREPAREd; but I've seen at least one case where it didn't work,
				// presumably because of the small gap in timing between the SQLNumParams check and the actual
				// execution and fetch of the statement. I'm hoping to figure out what's causing the loss of
				// PREPARE-ation, but in the meantime let's add another layer of resilience by trapping the
				// relevant error and converting it into a pseudo access conflict (which will normally trigger
				// an automatic retry at the application level).
				// DonR 05Jul2020: Added -211 (system catalog error) to the list of native codes that we'll
				// force to be treated as a conflict error.
				if (((ODBC_NativeError == -11103) && (!strcmp (ODBC_StateBuffer, "S1002")))	||	// Invalid Descriptor Index.
					( ODBC_NativeError == -11031)											||	// Invalid cursor state.
					( ODBC_NativeError == -211))
				{
					// DonR 30Jun2020: Don't bother with the diagnostic when the program is
					// first getting "warmed up". The threshold can be increased as we get
					// more confident - but should be lowered again for MS-SQL testing!
					if (ODBC_ActivityCount > PrintDiagnosticsAfter)
					{
						GerrLogMini (GerrId, "PID %d forcing access-conflict error for %s %s after %ld transactions. Native error %d, State Buffer %s.",
									 (int)getpid(), CurrentMacroName, ODBC_Operation_Name [OperationIdentifier], ODBC_ActivityCount, ODBC_NativeError, ODBC_StateBuffer);
					}

					ODBC_NativeError = SQLERR_CANNOT_POSITION_WITHIN_TABL;	// Force return of SQLERR_CANNOT_POSITION_WITHIN_TABL (-243).
					SQLFreeHandle (SQL_HANDLE_STMT, *Statement);
					StatementPrepared	[OperationIdentifier] = 0;
					StatementOpened		[OperationIdentifier] = 0;

					// DonR 11Nov2020: Make sure to decrement the Sticky Statement Counter!
					if (StatementIsSticky [OperationIdentifier])
					{
						NumStickyHandlesUsed--;
					}
				}	// Create "synthetic" access-conflict error to force retry after a loss-of-preparation error.


				// DonR 04Sep2023 Bug #469281: There is at least one database error (and surely there are really
				// a bunch of them) where the only recovery method that (I think) will work is to disconnect from
				// the database and reconnect. In order to do this, we'll shut the old connection here, log a
				// diagnostic, and change the error code to DB_CONNECTION_BROKEN - which will, in turn, force
				// a reconnection when the result is logged by INF_ERROR_TEST().
				// DonR 10Sep2023: Added another need-to-reconnect error DB_TCP_PROVIDER_ERROR / 10060.
				// DonR 27Jul2025 Bug #435292: Added another need-to-reconnect error TABLE_SCHEMA_CHANGED / 16943.
				switch (ODBC_NativeError)
				{
					case SQL_TRN_CLOSED_BY_OTHER_SESSION:
					case DB_TCP_PROVIDER_ERROR:
					case TABLE_SCHEMA_CHANGED:	// DonR 27Jul2025 Bug #435292
					case DB_CONNECTION_BROKEN:	// (Really an Informix error code - won't be thrown by MS-SQL.)

						// In order to avoid going to the database for the log-message timestamp, we need to
						// disconnect from the database *before* logging anything.
						SQLMD_disconnect ();

						GerrLogMini (	GerrId,
										"\nPID %d: Converting Native Error %d to DB_CONNECTION_BROKEN "
										"to force database reconnect.",
										(int)getpid(), ODBC_NativeError	);

						// Reset the local first-time-called variable, so that after we reconnect we'll start with
						// a clean slate. (OK, we'll start with clean arrays, counters, etc. - no actual slate is
						// involved! <g>)
						ODBC_Exec_FirstTimeCalled = 1;

						// Force in a connection-broken error code, which will trigger automatic database reconnect
						// the next time a database operation is attempted.
						ODBC_NativeError = DB_CONNECTION_BROKEN;	// Force DB reconnect.
						break;

					default:
						break;
				}

//				// DonR 30Jul2023 Bug #469281: TEMPORARY - add a diagnostic when we see Native Error 3926 (transaction
//				// closed by another session). This error is now treated as a "regular" contention error, so we won't
//				// normally see a logged error for it, but until we know that the fix works, I want to see when it occurs.
//				if ((ODBC_NativeError == SQL_TRN_CLOSED_BY_OTHER_SESSION) || (ODBC_NativeError == (0 - SQL_TRN_CLOSED_BY_OTHER_SESSION)))
//				{
//					GerrLogMini (GerrId, "Caught native SQL error %d in %s.",
//								 ODBC_NativeError, (OriginalSQLCommand == NULL) ? "N/A" : OriginalSQLCommand);
//				}
				break;
	
	default:	strcpy	(ODBC_StateBuffer, "");
				ErrCategoryDesc = "Unrecognized error category";
				sprintf	(ODBC_ErrorText, "Unrecognized error category %d.", ErrorCategory);
				ODBC_NativeError = 0;
				break;
	}

	// Set up "legacy" error reporting fields.
	if (strlen (ODBC_ErrorText) > ODBC_ErrorLength)
	{
		ODBC_ErrorText [ODBC_ErrorLength] = (char)0;
	}

	errm_len = sprintf (ODBC_ErrorBuffer, "%s: state %s, native err %d, error text {%s}",
						ErrCategoryDesc, ODBC_StateBuffer, ODBC_NativeError, ODBC_ErrorText);
//GerrLogMini(GerrId, ODBC_ErrorBuffer);
	if (errm_len >= sizeof(sqlca.sqlerrm))
		errm_len = sizeof(sqlca.sqlerrm) - 1;
	strncpy (sqlca.sqlerrm, ODBC_ErrorBuffer, errm_len);
	sqlca.sqlerrm [errm_len] = (char)0;

//GerrLogMini (GerrId, "Error category %d: SQL state %s, native err %d, error text {%s}\nOriginal SQL command:\n%s\n",
//			 ErrorCategory, ODBC_StateBuffer, ODBC_NativeError, ODBC_ErrorText,
//			 (OriginalSQLCommand == NULL) ? "N/A" : OriginalSQLCommand);

	// For now, return the ODBC SQL Native Error - but we should probably do some
	// translation here, to return familiar error codes to calling functions.
	return (ODBC_NativeError);
}	// ODBC_ErrorHandler() end.


// DonR 30May2022: Note that for now (at least), the parameter ColumnType_in
// is not being used for anything. I tried several methods of validating output
// columns to try to detect when the pointer sent to MacODBC was in fact pointing
// to "constant space" instead of to writable "variable space" memory - but nothing
// I tried worked reliably. Accordingly, I'm giving up for the moment, until (and
// if) I can think of something else to try.
int ODBC_IsValidPointer (void *Pointer_in, int CheckPointerWritable_in, int ColumnType_in, char **ErrDesc_out)
{
	char	PointerTestChar;
	char	*ErrDescription			= NULL;
	short	ODBC_PointerIsReadable	= 0;	// Default = pointer was *not* readable; this is
											// relevant only if ODBC_PointerIsValid is set FALSE.

	ODBC_PointerIsValid = 1;	// Default = pointer is OK.

// DonR 12July2020: Add #define'd parameter to enable/disable pointer checking.
// I want to see if disabling this feature has any impact on the gradual
// performance degradation we're seeing. (DonR 30May2022: No, the degradation
// was because of something else entirely.)
#if ENABLE_POINTER_VALIDATION
	if (!sigsetjmp (BeforePointerTest, 1))
	{
		// Try reading one byte from the pointer, and then writing the same byte back again. If
		// the address is both readable and writeable without a segmentation fault, we can pretty
		// much assume that it's valid.
		PointerTestChar			= *(char *)Pointer_in;
		ODBC_PointerIsReadable	= 1;	// If we get here, we were (at least) able to read from the pointer.

		// DonR 29May2022: For input values, it should be enough that the pointer is readable, so we
		// should be able to skip the writability test. However, this should be tested to make sure
		// the ODBC driver is in fact able to cope with read-only pointers for input parameters. 
		if (CheckPointerWritable_in)
		{
			// This should fail if the pointer is pointing to a constant value.
			*(char *)Pointer_in		= PointerTestChar;
		}	// Need to check whether (output) variable points to a writable memory address.

	}
	else
	{
		// If we get here, sigsetjmp "returned" a non-zero value - which means
		// there was a siglongjmp from macODBC_SegmentationFaultCatcher() (or
		// from a "custom" segmentation-fault handler set up by the mainline),
		// and ODBC_PointerIsValid has been set FALSE. We don't have to do
		// anything special in this case - just return from the function.
	}
#endif

	// DonR 18May2022: In addition to returning a flag to indicate whether the tested
	// pointer is valid, set a text variable to a description of the type of problem,
	// if there was one.
	if (ErrDesc_out != NULL)
	{
		if (ErrDescription == NULL)	// In case we add logic above that sets a different
									// value, leave that value in place.
		{
			ErrDescription = (ODBC_PointerIsValid) ? "" : ((ODBC_PointerIsReadable) ? "not writable" : "not readable");
		}

		*ErrDesc_out = ErrDescription;
	}

	return (ODBC_PointerIsValid);
}


void macODBC_SegmentationFaultCatcher (int signo)
{
	caught_signal = signo;


// DonR 12July2020: Add #define'd parameter to enable/disable pointer checking.
// I want to see if disabling this feature has any impact on the gradual
// performance degradation we're seeing.
#if ENABLE_POINTER_VALIDATION
	// ODBC_ValidatingPointers indicates that we are in fact checking ODBC column/parameter
	// pointers, so we want to recover gracefully from a segmentation fault. At any other
	// point, a segmentation fault is a fatal error and should terminate the program as it
	// normally does.
	if (ODBC_ValidatingPointers)
	{
		// Segmentation fault means that the pointer we're checking is *not* OK.
		ODBC_PointerIsValid = 0;

		// Reset signal handling for the segmentation fault signal.
		sigaction (SIGSEGV, &sig_act_ODBC_SegFault, NULL);

		// Go back to the point just before we forced the segmentation-fault error.
		siglongjmp (BeforePointerTest, 1);
	}
	else
	{
		// "Ordinary" fatal segmentation fault - exit program.
		GerrLogMini (GerrId, "PID %d: Segmentation fault - terminating program!", getpid());
		RollbackAllWork ();
		SQLMD_disconnect ();

		// Exit child process.
		ExitSonProcess ((signo == 0) ? MAC_SERV_SHUT_DOWN : MAC_EXIT_SIGNAL);
	}

#else	// ENABLE_POINTER_VALIDATION is FALSE - just do regular segmentation-fault trapping. 
	// "Ordinary" fatal segmentation fault - exit program.
	GerrLogMini (GerrId, "PID %d: Segmentation fault - terminating program!", getpid());
	RollbackAllWork ();
	SQLMD_disconnect ();

	// Exit child process.
	ExitSonProcess ((signo == 0) ? MAC_SERV_SHUT_DOWN : MAC_EXIT_SIGNAL);
#endif

}

#endif	// MAIN
#endif	// MacODBC_H defined