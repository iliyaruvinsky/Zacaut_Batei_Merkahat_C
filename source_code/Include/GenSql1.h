/*=======================================================================
||									||
||				GenSql.h				||
||									||
||======================================================================||
||									||
||	Purpose : General definitions for working with databases.	||
||		  For the time being support is for Oracle , Informix.	||
||									||
||======================================================================||
||									||
||  Written by : Boaz Shnapp ( reshuma ) - oracle.			||
||  Updated by : Ely Levy ( reshuma ) - informix.			||
||  25/12/1994								||
||  Revison 01								||
||									||
 =======================================================================*/


#ifndef  SQL_ERR_H
#define  SQL_ERR_H

#include "../Include/Global.h"

static char	*minuses =
	"-------------------------------------------------------------------------------------------------------";

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

#define SQLERR_error_test()     INF_ERROR_TEST( &sqlca,	GerrId,	NULL ) 

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
||			Informix error codes.				||
 =======================================================================*/

#define INF_OK				0
#define INF_ACCESS_CONFLICT		-244
#define INF_TABLE_IS_LOCKED_FOR_INSERT	-271
#define INF_DEADLOCK			-143
#define INF_NOT_UNIQUE_INDEX		-239
#define INF_NOT_UNIQUE_CONSTRAINT	-268
#define INF_REFERENCED_VALUE_NOT_FOUND	-691
#define INF_REFERENCED_VALUE_IN_USE	-692
#define INF_TOO_MANY_RESULT_ROWS	-284
#define INF_CANNOT_READ_SYS_CATALOG	-211
#define INF_COLUMN_NOT_FOUND_IN_TABLES	-217
#define INF_CANNOT_POSITION_WITHIN_TABL	-243
#define INF_CANNOT_POSITION_VIA_INDEX	-245
#define INF_NO_MORE_DATA_OR_NOT_FOUND	100
#define	SQLERR_no_rows_affected		200

/*=======================================================================
||			Function definitions.				||
 =======================================================================*/

int	INF_ERROR_TEST(
		       struct	sqlca_s  *code,	/* sqlca from SQL stmt	*/
		       char	*source_name,	/* source name of  prog	*/
		       int	line_no,	/* error line number 	*/
		       char	*sttm_str	/* Statement string	*/
		       );

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

#endif		/* For database mode					*/

/*=======================================================================
||			General Sql error code types.			||
 =======================================================================*/

#define SQLERR_ok				0
#define SQLERR_not_found			1
#define SQLERR_deadlocked			2
#define SQLERR_access_conflict			3
#define SQLERR_not_unique			4
#define SQLERR_referenced_value_in_use		5
#define SQLERR_referenced_value_not_found	6
#define SQLERR_too_many_result_rows		7
#define SQLERR_end_of_fetch			8
#define SQLERR_access_conflict_cursor		9
#define SQLERR_column_not_found_in_tables	10



//because it use in 4 files :DBFields.h;global_1.h;
//As400UnixClient.ec;macsql.ec Yulia 06.2000
typedef 	int	TCCharRowid20;  

#endif		/* For #ifndef at start of file				*/

















