/*=======================================================================
||																		||
||				GenSql.h												||
||																		||
||======================================================================||
||																		||
||	Purpose : General definitions for working with databases.			||
||		  For the time being support is for Oracle , Informix.			||
||																		||
||======================================================================||
||																		||
||  Written by : Boaz Shnapp ( reshuma ) - oracle.						||
||  Updated by : Ely Levy ( reshuma ) - informix.						||
||  25/12/1994															||
||  Revison 01															||
||																		||
 =======================================================================*/


#ifndef  SQL_ERR_H
#define  SQL_ERR_H

#include "Global.h"

/*Marianna 24.05.2020*/
//#ifdef _LINUX_
//	#include <MacODBC.h>		//Marianna
//	//#include </opt/IBM/informix/incl/dmi/sqlca.h>	//Marianna
//	//#include </home/informix/incl/dmi/sqlca.h>	//Marianna - prev
//#else
//	#include </usr/informix/incl/dmi/sqlca.h>		//Marianna
//#endif
/*End Marianna 24.05.2020*/

/*From MacODBC.h - Marianna 24.05.2020*/

#ifndef sqlca			//Marianna
#ifndef SQLCA_INCL		//Marianna
typedef struct sqlca_s
{
	int		sqlcode;
	char	sqlerrm		[601]; // error message parameters
	char	sqlerrp		[8];
	int		sqlerrd		[6];
	// 0 - estimated number of rows returned
	// 1 - serial value after insert or  ISAM error code
	// 2 - number of rows processed
	// 3 - estimated cost
	// 4 - offset of the error into the SQL statement
	// 5 - rowid after insert
	char	sqlawarn	[8];
//	char sqlwarn0; /* = W if any of sqlwarn[1-7] = W */
//	char sqlwarn1; /* = W if any truncation occurred or
//				database has transactions or
//			        no privileges revoked */
//	char sqlwarn2; /* = W if a null value returned or
//				ANSI database */
//	char sqlwarn3; /* = W if no. in select list != no. in into list or
//				turbo backend or no privileges granted */
//	char sqlwarn4; /* = W if no where clause on prepared update, delete or
//				incompatible float format */
//	char sqlwarn5; /* = W if non-ANSI statement */
//	char sqlwarn6; /* = W if server is in data replication secondary mode */
//	char sqlwarn7; /* = W if database locale is different from proc_locale
//                         = W if backend XPS and if explain avoid_execute is set
//                             (for select, insert, delete and update only)
//			*/
}	sqlca_t;

// Global database "communications area" structure.
sqlca_t	sqlca;
int		SQLCODE;
#endif		//Marianna
#endif		//Marianna
/*End Marianna 24.05.2020*/



static char	*minuses =
	"-------------------------------------------------------------------------------------------------------";



// Provide global default SQL command text to avoid compilation/execution errors.
static const char *sqlcmdtxt[] =
{
	"Original SQL statement not available.",
	0
};

//// DonR 16Dec2019: Global pointer to last ODBC SQL command text.
//extern char *ODBC_LastSQLStatementText;

// DonR 14Jan2021: Pull in global "Production Mode" flag so the get-timestamp
// routine knows if we're running on a test system. Note that this means all
// mainlines need to define this variable!
extern int		TikrotProductionMode;	// So operating mode is visible to calling routine.


/*
 **
 *** Database of project
 *** -------------------
 **
 */

#define INFORMIX_MODE

/*=======================================================================
||		    Define ORACLE moduls interface.			||
 =======================================================================*/
#ifdef	ORACLE_MODE

#define SQLERR_code_cmp( typ )	ORAERR_code_cmp( &sqlca, typ )

#define SQLERR_error_test()	ORAERR_error_test(&sqlca, GERR_ID, sqlstm.stmt)

#define SQLMD_connect()		ORACLE_connect( ORACLE_user_pass )

#define SQLMD_disconnect()	ORACLE_disconnect()

#define SQLMD_begin()		ORACLE_begin()

#define SQLMD_commit()		ORACLE_commit()

#define SQLMD_rollback()	ORACLE_rollback()

#define	SQLMD_db_to_em_date	ORACLE_db_to_em_date

#define	SQLMD_em_date_to_db	ORACLE_em_date_to_db

#define	SQLMD_current_date	ORACLE_current_date

#define	SQLMD_prep_sql		OCI_prep_sql

#define	SQLMD_5_min_insert	OCI_5_min_insert

#define	SQLMD_1_hour_insert	OCI_1_hour_insert

#define	SQLMD_1_day_insert	OCI_1_day_insert

#define	SQLMD_update_crnt	OCI_update_crnt

#define	SQLMD_update_avg	OCI_update_avg

#define SQLMD_null		-1

#define SQLMD_notnull		0

/*=======================================================================
||			Function definitions.				||
 =======================================================================*/

int     ORAERR_code_cmp();
void 	ORAERR_error_test();
void	ORACLE_connect();
void	ORACLE_begin();
void	ORACLE_disconnect();
void	ORACLE_commit();
void	ORACLE_rollback();
void	ORACLE_db_to_em_date(
			     char *,
			     API_DATETIME *
			     );

void	ORACLE_em_date_to_db(API_DATETIME *, char *);
void    ORACLE_current_date(char *);
void	OCI_prep_sql();
void	OCI_5_min_insert();
void	OCI_1_hour_insert();
void    OCI_1_day_insert();
void    OCI_update_crnt();

/*=======================================================================
||			Informix database moduls interface.		||
 =======================================================================*/

#elif defined( INFORMIX_MODE )

/*=======================================================================
||		    Define Informix moduls interface.			||
 =======================================================================*/

#define SQLERR_code_cmp( type )	INF_CODE_CMP( &sqlca, type )

#ifdef _LINUX_
	#define SQLERR_error_test()     LINUX_INF_ERROR_TEST( &sqlca,	GerrId,	NULL, (char **)sqlcmdtxt ) 
#else
	#define SQLERR_error_test()     INF_ERROR_TEST( &sqlca,	GerrId,	NULL ) 
#endif

#define	SQLERR_error_clear()		INF_ERROR_CLEAR(&sqlca)

#define SQLMD_connect()		INF_CONNECT( MacUser, MacPass, MacDb )

#define SQLMD_disconnect()	INF_DISCONNECT()

#define SQLMD_begin()		INF_BEGIN()

#define SQLMD_commit()		INF_COMMIT()

#define SQLMD_rollback()	INF_ROLLBACK()

#define SQLMD_connect_id( conn )	\
				INF_CONNECT_ID( MacUser, MacPass, MacDb, conn )

#define SQLMD_disconnect_id( conn )	\
				INF_DISCONNECT_ID( conn )

#define SQLMD_set_connection( conn )	\
				INF_SET_CONNECTION( conn )

#define SQLMD_is_null( ind )	( (ind == -1) ? MAC_TRUE : MAC_FALS ) 

#define Conflict_Test_Cur( r )					\
if( SQLERR_code_cmp( SQLERR_access_conflict_cursor ) == MAC_TRUE )\
{								\
    r = MAC_TRUE;						\
}
/*=======================================================================
||			Database error codes.										||
 =======================================================================*/
#define INF_OK								0
#define SQL_OK								0
#define FORCE_SQL_OK	sqlca.sqlcode=SQLCODE=0;strcpy(ODBC_ErrorBuffer,"");	// DonR 27Nov2025: Clear error buffer as well as SQLCODE.

#define INF_DEADLOCK						-143
#define SQL_DEADLOCK						1205	// DonR 02Mar2021 MS-SQL equivalent of Informix -143.
#define INF_CANNOT_READ_SYS_CATALOG			-211
#define INF_COLUMN_NOT_FOUND_IN_TABLES		-217
#define INF_NOT_UNIQUE_INDEX				-239
#define SQL_NOT_UNIQUE_INDEX				2601
#define SQL_NOT_UNIQUE_PRIMARY_KEY			2627	// DonR 08Feb2021: MS-SQL generates a different error code for primary keys than for unique indexes.
#define SQLERR_CANNOT_POSITION_WITHIN_TABL	-243	// DonR 08Nov2020: Note that we're generating this code for all databases, not just Informix.
#define INF_ACCESS_CONFLICT					-244
#define INF_CANNOT_POSITION_VIA_INDEX		-245
#define INF_CANNOT_GET_NEXT_ROW				-246
#define INF_NOT_UNIQUE_CONSTRAINT			-268	// DonR 08Nov2020: Is there an MS-SQL version of this one?
#define INF_TABLE_IS_LOCKED_FOR_INSERT		-271
#define SQLERR_TOO_MANY_RESULT_ROWS			-284	// DonR 08Nov2020: Note that we're generating this code for all databases, not just Informix.
#define	INF_DB_NOT_SELECTED					-349
#define INF_CURSOR_NOT_AVAILABLE			-404
#define INF_REFERENCED_VALUE_NOT_FOUND		-691
#define INF_REFERENCED_VALUE_IN_USE			-692
#define INF_CONNECT_ATTEMPT_FAILED			-908
#define INF_CONNECTION_NOT_EXIST			-1803
#define INF_IMPLICIT_NOT_ALLOWED			-1811
#define SQL_TRN_CLOSED_BY_OTHER_SESSION		3926	// DonR 30Jul2023 Bug #469281 - happens when there's a network glitch in DB infrastructure.
#define DB_TCP_PROVIDER_ERROR				10060
#define DB_CONNECTION_BROKEN				-25582
#define TABLE_SCHEMA_CHANGED				16943	// DonR 27Jul2025 Bug #435292 - caused by an automated nightly DB maintenance routine.
#define DB_CONNECTION_RESTORED				-25999	// DonR 06Sep2023: This is a made-up error code to force a retry after DB is reconnected.

#define SQLERR_NO_MORE_DATA_OR_NOT_FOUND	100		// DonR 08Nov2020: Note that this code applies to all databases, not just Informix.
#define FORCE_NOT_FOUND	sqlca.sqlcode=SQLCODE=100;	// DonR 25Aug2021: Moved this here and replaced SQLERR_NO_MORE_DATA_OR_NOT_FOUND
													// with its actual numeric value, since I'm not sure the "double macro" thing was
													// working properly. Also added a semicolon to the end, just in case someone leaves
													// it out.

#define	SQLERR_no_rows_affected				200
#define INF_TABLE_NOT_FOUND					-206
#define SQL_TABLE_NOT_FOUND					15225
#define SQL_INTEGER_PRECISION_EXCEEDED		-1215	// DonR 17Nov2020.
#define SQL_UNICODE_CONVERSION_FAILED		11		// DonR 22Mar2021.

// DonR 04Oct2021: SQL Server native errors for common programmer errors.
#define SQL_NATIVE_BAD_SYNTAX				102
#define SQL_NATIVE_FUNCTION_ARGS_WRONG		174
#define SQL_NATIVE_FUNCTION_NAME_WRONG		195
#define SQL_NATIVE_BAD_COLUMN_NAME			207
#define SQL_NATIVE_BAD_TABLE_NAME			208

// DonR 18May2022: Add a "synthetic" error code for MS-SQL
// "silent INSERTion failure". This is when the database server
// fails to return an error code, but also fails to actually
// INSERT a row; so far, I've seen this happen only with one
// INSERT command in Transaction 6005-spool, but others on the
// internet have encountered similar problems so I'm assuming
// that there's some bug somewhere in the DB server.
#define SQL_SILENT_INSERT_FAILURE		-29000

// Macros for quick execution.
#define NO_SQL_ERROR        (sqlca.sqlcode == 0)
#define SQL_ERROR_RETURNED  (sqlca.sqlcode != 0)
#define DB_UPDATE_WORKED    ((sqlca.sqlcode == 0) && (sqlca.sqlerrd[2] != 0))
#define NO_ROWS_AFFECTED    ((sqlca.sqlcode == 0) && (sqlca.sqlerrd[2] == 0))


/*=======================================================================
||			Function declarations.										||
 =======================================================================*/

int	INF_ERROR_TEST(
		       struct	sqlca_s  *code,	/* sqlca from SQL stmt	*/
		       char	*source_name,	/* source name of  prog	*/
		       int	line_no,	/* error line number 	*/
		       char	*sttm_str	/* Statement string	*/
		       );

int	LINUX_INF_ERROR_TEST (struct sqlca_s	*code,			/* sqlca from SQL stmt		*/
						  char				*source_name,	/* source name of  prog		*/
						  int				line_no,		/* error line number		*/
						  char				*sttm_str,		/* "special message" string	*/
						  char				**SQL_stmt);	/* SQL statement executed	*/

int	INF_CODE_CMP( 
		     struct	sqlca_s *code_strct,/* sqlca structr ptr*/
		     int	error_type	/* error type		*/
		     );

void	CURRENT_DATE(
		     char	*dbt		/* dotted string date	*/
		     );

void	STRING_DATE_TO_TM(
			  char	*dbt,		/* dotted string date	*/
			  struct tm *tm_strct	/* struct tm to get date*/
			  );

int	INF_CONNECT(
		    char	*username,	/* username		*/
		    char	*password,	/* password		*/
		    char	*dbname		/* database name	*/
		    );

int	INF_DISCONNECT();

int	INF_COMMIT();

int	INF_ROLLBACK();

char * GetDbTimestamp ();

char * StripAllSpaces (unsigned char * source);

// DonR 24Sep2024: Add some missing function prototypes.
void	date_to_tm	(int inp, struct tm *TM);
void	fix_char	(unsigned char *source);
void	time_to_tm	(int inp, struct tm *TM);
int		tm_to_date	(struct tm *tms);
int		tm_to_time	(struct tm *tms);

#endif		/* For database mode					*/

/*=======================================================================
||			General Sql error code types.			||
 =======================================================================*/

#define SQLERR_ok							 0
#define SQLERR_not_found					 1
#define SQLERR_deadlocked					 2
#define SQLERR_access_conflict				 3
#define SQLERR_not_unique					 4
#define SQLERR_referenced_value_in_use		 5
#define SQLERR_referenced_value_not_found	 6
#define SQLERR_too_many_result_rows			 7
#define SQLERR_end_of_fetch					 8
#define SQLERR_access_conflict_cursor		 9
#define SQLERR_column_not_found_in_tables	10
#define SQLERR_table_not_found				11
#define SQLERR_data_conversion_problem		12


// DonR 17Jul2005: DB Isolation Level stuff.
#define _DIRTY_READ_		0
#define	_COMMITTED_READ_	1
#define _CURSOR_STABILITY	2
#define _REPEATABLE_READ_	3

#define ISOLATION_VAR	short _saved_isolation_

// DonR 30Aug2020: I've been unable to get _set_db_isolation_ working once
// the program has started doing anything with the database - even just
// reading stuff. I've already effectively disabled _set_db_isolation(),
// but for performance improvement, let's disable the relevant macros
// entirely - there's no point in calling functions that don't do anything!
// DonR 14Jan2021: Re-enabled isolation control for MS-SQL - so the #defines
// need to be re-enabled as well.
// DonR 22Feb2021: Leaving these disabled now, to improve performance. If we
// see significant deadlock problems once we switch Production to MS-SQL,
// it'll be time to revisit this.
#if 0
	#define SET_ISOLATION_DIRTY			_set_db_isolation(_DIRTY_READ_);
	#define SET_ISOLATION_COMMITTED		_set_db_isolation(_COMMITTED_READ_);
	#define SET_ISOLATION_STABLE		_set_db_isolation(_CURSOR_STABILITY);
	#define SET_ISOLATION_REPEATABLE	_set_db_isolation(_REPEATABLE_READ_);

	#define REMEMBER_ISOLATION			_get_db_isolation(&_saved_isolation_);
	#define RESTORE_ISOLATION			_set_db_isolation(_saved_isolation_);
#else
	#define SET_ISOLATION_DIRTY			;
	#define SET_ISOLATION_COMMITTED		;
	#define SET_ISOLATION_STABLE		;
	#define SET_ISOLATION_REPEATABLE	;

	#define REMEMBER_ISOLATION			;
	#define RESTORE_ISOLATION			;
#endif

int _set_db_isolation (short new_isolation_mode_in);
int _get_db_isolation (short *current_isolation_mode_out);
// DonR 17Jul2005: DB Isolation Level stuff end.

// More function prototypes.
int WinHebToUTF8 (unsigned char *Win1255_in, unsigned char **UTF8_out, size_t *length_out);

double ConvertUnitAmount (double amount_in, char *unit_in);

int ParseISODateTime(const char* datetime_in, int* date_out, int* time_out);

//because it use in 4 files :DBFields.h;global_1.h;
//As400UnixClient.ec;macsql.ec Yulia 06.2000
// DonR 20Jan2020: Changed from long to int, since we're now working in 64-bit Linux
// where int is 4 bytes and long is 8 bytes (a database BIGINT).
typedef int TCCharRowid20;  

#endif		/* For #ifndef at start of file				*/

// DonR 26Nov2023: Add some "missing" function declarations.
int	diff_from_then (int	date0_in,
					int	time0_in,
					int	date1_in,
					int	time1_in);

int	diff_calendar_months (int date0_in, int date1_in);

short GET_MSG_REC_ID (int *message_recid);

// DonR 26Nov2025 User Story #251264: Add support for more flexible Hebrew output encoding.
int EncodeHebrew	(	char			input_encoding_in,
						char			output_encoding_in,
						size_t			buffer_size_in,	// Includes terminating NULL character!
						unsigned char	*text_in_out			);

void	WinHebToDosHeb( unsigned char * source );

void  strip_spaces (unsigned char * source);

void  strip_trailing_spaces (unsigned char * source);












