/*=======================================================================
||																		||
||				Gensql.ec												||
||																		||
||======================================================================||
||																		||
||	Purpose:															||
||	This file contains interface functions for  INFORMIX				||
||	like : error tests, connect , disconnect database.					||
||																		||
||======================================================================||
||																		||
||  Written by : Boaz Shnapp ( reshuma ) - oracle.25/12/1994			||
||  Updated by : Ely Levy ( reshuma ) - Informix.11/06/96				||
||																		||
||  Revision 2															||
 =======================================================================*/

static  char	*GerrSource = __FILE__;

char	**glbsqltxtarr;

#include <iconv.h>

//EXEC SQL INCLUDE sqlca;			//Marianna

// EXEC SQL INCLUDE sqltypes;

//EXEC SQL INCLUDE "GenSql.h";		//Marianna
#include "GenSql.h"					//Marianna


#define GEN_SQL

//EXEC SQL INCLUDE "PharmacyErrors.h";		//Marianna
#include  "PharmacyErrors.h"				//Marianna


//EXEC SQL INCLUDE "MsgHndlr.h";			//Marianna
#include "MsgHndlr.h"						//Marianna

#include <MacODBC.h>

#define  SZENV_SYSTEM   "MAC_SYS"   // GAL 23/06/05
	
#ifdef _LINUX_
	#define	MAX_NAME_LEN	256		
#else 
	#define	MAX_NAME_LEN	MAXHOSTNAMELEN
	/* MAXHOSTNAMELEN = 256 in <sys/socket.h> GAL 15/06/05 */
#endif

#define Conflict_Test( r )					\
if( SQLERR_code_cmp( SQLERR_access_conflict ) == MAC_TRUE ||	\
    SQLERR_code_cmp( SQLERR_deadlocked ) == MAC_TRUE )		\
{								\
    SQLERR_error_test();					\
    r = MAC_TRUE;						\
    break;							\
}

// Extended define Hebrew for DOS and WINDOWS
#define ishebrew( c )									\
(														\
( (unsigned char)( c ) >= 128 && (unsigned char)( c ) <= 154 ) ||	\
( (unsigned char)( c ) >= 224 && (unsigned char)( c ) <= 250 ) )

#define isenglish( c )											\
(																\
( (unsigned char)(c) >= 65 && (unsigned char)(c) <= 90 ) ||		\
( (unsigned char)(c) >= 97 && (unsigned char)(c) <= 122 ) )

#define isnumber( c )											\
( (unsigned char)(c) >= 48 && (unsigned char)(c) <= 57 )

/*=======================================================================
||																		||
||			Private parameters											||
||																		||
||	CAUTION : DO NOT !!! CHANGE THIS PARAGRAPH -- IMPORTANT !!!			||
||																		||
 =======================================================================*/

#define	PHRM_RET_ON_ERR( state )			\
	if( state < MAC_OK )					\
	{										\
	  return( ERR_SHARED_MEM_ERROR );		\
	}



TABLE_DATA
	TableTab[]
=
{
  { sizeof(PARM_DATA), sizeof(PARM_DATA),	NOT_SORTED,		NULL,
    ParmComp,		PARM_TABLE,		""	},
  { sizeof(PROC_DATA), sizeof(PROC_DATA),	NOT_SORTED,		NULL,
    ProcComp,		PROC_TABLE,		""	},
  { sizeof(STAT_DATA), sizeof(STAT_DATA),	NOT_SORTED,		NULL,
    NULL,		STAT_TABLE,		""	},
  { sizeof(TSTT_DATA), sizeof(TSTT_DATA),	NOT_SORTED,		NULL,
    NULL,		TSTT_TABLE,		""	},
  { sizeof(DSTT_DATA), sizeof(DSTT_DATA),	NOT_SORTED,		NULL,
    NULL,		DSTT_TABLE,		""	},
  { sizeof(UPDT_DATA), sizeof(UPDT_DATA),	NOT_SORTED,		NULL,
    UpdtComp,		UPDT_TABLE,		""	},
  { sizeof(PHARMACY_ROW), sizeof(PHARMACY_ROW),	NOT_SORTED,	RefreshPharmacyTable,
    NULL,		PHRM_TABLE,		"Pharmacy"	},
  { sizeof(SEVERITY_ROW), sizeof(SEVERITY_ROW),	NOT_SORTED,	RefreshSeverityTable,
    NULL,		SEVR_TABLE,		"Pc_error_message"	},
  { sizeof(PERCENT_ROW), sizeof(PERCENT_ROW),	NOT_SORTED,	RefreshPercentTable,
    NULL,		PCNT_TABLE,		"Member_price"	},
  { sizeof(DOCTOR_DRUG_ROW), sizeof(DOCTOR_DRUG_ROW),	NOT_SORTED,	RefreshDoctorDrugTable,
    DocDruComp,		DCDR_TABLE,		"Doctor_Percents,Doctor_Speciality,Spclty_largo_prcnt"	},
  { sizeof(INTERACTION_ROW), sizeof(INTERACTION_ROW),	NOT_SORTED,	RefreshInteractionTable,
    NULL,		INTR_TABLE,		"Drug_interaction"	},
  { sizeof(MESSAGE_RECID_ROW), sizeof(MESSAGE_RECID_ROW),	NOT_SORTED,	NULL,
    NULL,		MSG_RECID_TABLE,		""	}, // MUST BE BEFORE PRESCRIPTION ID TABLE!!
  { sizeof(DELPR_RECID_ROW), sizeof(DELPR_RECID_ROW),	NOT_SORTED,	NULL,
    NULL,		DELPR_ID_TABLE,		""	}, // MUST BE BEFORE PRESCRIPTION ID TABLE!!
  { sizeof(PRESCRIPTION_ROW), sizeof(PRESCRIPTION_ROW),	NOT_SORTED,	RefreshPrescriptionTable,
    NULL,		PRSC_TABLE,		""	},
  { sizeof(PRESCR_WRITTEN_ROW), sizeof(PRESCR_WRITTEN_ROW),	NOT_SORTED,	RefreshPrescrWrittenTable,
    NULL,		PRW_TABLE,		""	},
  { sizeof(DRUG_EXTENSION_ROW), sizeof(DRUG_EXTENSION_ROW),	NOT_SORTED,	RefreshDrugExtensionTable,
    NULL,		DREX_TABLE,		"Drug_extension"	},
  { sizeof(DIPR_DATA), sizeof(DIPR_DATA),		NOT_SORTED,	NULL,
    DiprComp,		DIPR_TABLE,		""	},
  { 0, 0,			0,			NULL,
    NULL,		"",			""	},
};

// Variable to store the current database isolation level.
static short	_current_db_isolation_ = _COMMITTED_READ_;	// Default for non-ANSI DB.

// Variable to store the current database connection status.
static short	_db_connected_ = 0;

// Global variable to control whether we automatically pause after a failed database auto-reconnect.
short			_PauseAfterFailedReconnect = 0;


/*=======================================================================
||																		||
||				tm_to_date()											||
||																		||
 =======================================================================*/
int	tm_to_date( struct tm *tms )
{
    return(  tms->tm_mday +
	  (tms->tm_mon  +    1) * 100 +
	  (tms->tm_year + 1900) * 10000 );
}
/*=======================================================================
||									||
||				tm_to_time()				||
||									||
 =======================================================================*/
int	tm_to_time( struct tm *tms )
{
    return( tms->tm_sec          +
	    tms->tm_min  * 100   +
	    tms->tm_hour * 10000  );
}


/*=======================================================================
||																		||
||				GetDate ()												||
||																		||
||		get current date in format YYMMDD								||
 =======================================================================*/
int	GetDate (void)
{
	time_t		curr_time;
	struct tm	*tm_strct;

	curr_time = time (NULL);

	tm_strct  = localtime (&curr_time);

	return (tm_to_date (tm_strct));
}

// Return the equivalent of Informix CURRENT YEAR TO SECOND as YYYYMMDDHHMMSS.
long GetCurrentYearToSecond (void)
{
	time_t		curr_time;
	struct tm	*tms;

	curr_time	= time (NULL);
	tms			= localtime (&curr_time);

	return	(		(	(tms->tm_year + 1900)	* 10000L	* 1000000L	)
				+	(	(tms->tm_mon  +    1)	*   100L	* 1000000L	)
				+	(	 tms->tm_mday						* 1000000L	)
				+	(	 tms->tm_hour			* 10000L				)
				+	(	 tms->tm_min			*   100L				)
				+	(	 tms->tm_sec									));
}


long GetIncrementedYearToSecond (int IncrementInSeconds_in)
{
	int	WorkDate;
	int	WorkTime;

	WorkDate = GetDate ();
	WorkTime = GetTime ();

	WorkTime = IncrementTime (WorkTime, IncrementInSeconds_in, &WorkDate);

	return (((long)WorkDate * 1000000L) + (long)WorkTime);
}


/*=======================================================================
||																		||
||				GetDayOfWeek ()											||
||																		||
||		get current day of week - Sunday = 0, Saturday = 6				||
 =======================================================================*/
int	GetDayOfWeek (void)
{
	time_t		curr_time;
	struct tm	*tm_strct;

	curr_time = time (NULL);

	tm_strct  = localtime (&curr_time);

	return (tm_strct->tm_wday);
}


/*=======================================================================
||																		||
||				GetTime ()												||
||																		||
||		get current time in format HHMMSS								||
 =======================================================================*/
int	GetTime (void)
{
	time_t		curr_time;
	struct tm	*tm_strct;

	curr_time = time (NULL);

	tm_strct  = localtime (&curr_time);

	return (  10000 * tm_strct->tm_hour
			+	100 * tm_strct->tm_min
			+	tm_strct->tm_sec);
}
/* End of GetTime () */


// DonR 11Sep2012: IncrementDate() is, in effect, a duplicate of AddDays(); so
// there is no point in having both functions as part of our code. Accordingly,
// I'm changing IncrementDate() into a macro resolving to AddDays().
#if 0
//========================================================================
//																		||
//		IncrementDate()													||
//																		||
//		Increment a YYYYMMDD date by N days								||
//========================================================================
int IncrementDate (int db_date_in, int change_date_in)
{
	int	year;
	int month;
	int day;
	int month_len[13] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};


	year	= db_date_in / 10000;
	month	= (db_date_in % 10000) / 100;
	day		= db_date_in % 100;

	// Limited sanity checking of input date.
	if ((year < 1000)	||
		(year > 8000)	||
		(month < 1)		||
		(month > 12))
	{
		return (-1);
	}

	// Reset February length if we're starting with a leap year.
	if (((year % 4) == 0) &&
		(((year % 100) != 0) || ((year % 400) == 0)))
	{
		month_len[2] = 29;
	}

	day = day + change_date_in;

	// Positive increments.
	while (day > month_len [month])
	{
		day -= month_len [month];

		month++;

		// Check for year rollover.
		if (month > 12)
		{
			month = 1;
			year++;

			// Reset February length.
			if (((year % 4) == 0) &&
				(((year % 100) != 0) || ((year % 400) == 0)))
			{
				month_len[2] = 29;
			}
			else
			{
				month_len[2] = 28;
			}
		}
	}


	// Negative increments.	
	while (day < 1)
	{
		month--;

		// Check for year roll-under.
		if (month < 1)
		{
			month = 12;
			year--;

			// Reset February length.
			if (((year % 4) == 0) &&
				(((year % 100) != 0) || ((year % 400) == 0)))
			{
				month_len[2] = 29;
			}
			else
			{
				month_len[2] = 28;
			}
		}

		day += month_len [month];
	}
	

	return ((year * 10000) + (month * 100) + day);
}
#endif


int AddHours (int db_date_in,
			  int db_time_in,
			  int add_hours_in,
			  int *db_date_out,
			  int *db_time_out)
{
	int	AddSeconds	= add_hours_in * 3600;
	int	WorkTime	= db_time_in;
	int WorkDate	= db_date_in;

	WorkTime = IncrementTime (WorkTime, AddSeconds, &WorkDate);

	if (db_date_out != NULL)
		*db_date_out = WorkDate;

	if (db_time_out != NULL)
		*db_time_out = WorkTime;

	return (0);
}


//========================================================================
//																		||
//		IncrementTime()													||
//																		||
//		Increment an HHMMSS time by N seconds							||
//========================================================================
int IncrementTime (int db_time_in, int change_time_in, int *db_date_in_out)
{
	int	hour;
	int minute;
	int second;
	int	date_increment = 0;


	hour	= db_time_in / 10000;
	minute	= (db_time_in % 10000) / 100;
	second	= db_time_in % 100;

	// Limited sanity checking of input time.
	if ((hour < 0 )		||
		(hour > 24)		||
		(minute < 0)	||
		(minute > 60))
	{
		return (-1);
	}

	second = second + change_time_in;

	// Fix seconds - positive increments.
	// DonR 31Oct2005: For better performance on big intervals, jump an
	// hour at a time rather than minute-by-minute.
	while (second > 3599)
	{
		second -= 3600;
		hour++;
	}
	while (second > 59)
	{
		second -= 60;
		minute++;
	}

	// Fix seconds - negative increments.
	while (second < -3599)
	{
		second += 3600;
		hour--;
	}
	while (second < 0)
	{
		second += 60;
		minute--;
	}

	// Fix minutes - positive increments.
	while (minute > 59)
	{
		minute -= 60;
		hour++;
	}

	// Fix minutes - negative increments.
	while (minute < 0)
	{
		minute += 60;
		hour--;
	}

	// Fix hours - positive increments.
	while (hour > 23)
	{
		hour -= 24;
		date_increment++;
	}

	// Fix hours - negative increments.
	while (hour < 0)
	{
		hour += 24;
		date_increment--;
	}


	// If we got a non-NULL date pointer, adjust the date as necessary.
	if ((date_increment != 0) && (db_date_in_out != NULL))
	{
		*db_date_in_out = IncrementDate (*db_date_in_out, date_increment);
	}
	

	return ((hour * 10000) + (minute * 100) + second);
}



#ifdef _LINUX_
/*=======================================================================
||																		||
||                           LINUX_INF_ERROR_TEST()						||
||																		||
||	Shell for Informix error reporting on Linux							||
 =======================================================================*/
int	LINUX_INF_ERROR_TEST (struct sqlca_s	*code,			// sqlca from SQL stmt
						  char				*source_name,	// source name of prog - first part of GerrId.
						  int				line_no,		// error line number - second part of GerrId.
						  char				*sttm_str,		// "special message" string - NULL by #definition.
						  char				**SQL_stmt)		// SQL statement executed - #define'd to a meaningless constant value.
{
//	if (SQL_stmt != NULL)
//		glbsqltxtarr = SQL_stmt;
//	else
//		glbsqltxtarr = NULL;

	return (INF_ERROR_TEST (code, source_name, line_no, sttm_str));
}
#endif



/*=======================================================================
||																		||
||                              INF_ERROR_TEST()						||
||																		||
||	Test informix sqlca & SQLCODE and react accordingly.				||
 =======================================================================*/
int	INF_ERROR_TEST (struct sqlca_s	*code,			// sqlca from SQL stmt
					char			*source_name,	// source name of prog - first part of GerrId.
					int				line_no,		// error line number - second part of GerrId.
					char			*sttm_str)		// "special message" string - always NULL if called by LINUX_INF_ERROR_TEST.
{
	char		buf			[8192];
	char		stmt_buf	[8192];
	char		temp		[8192];
	int			position;
	int			end_line;
	int			column;
	static int	allow_recursion = 1;	// DonR 23Jan2007: Permits reconnecting to DB
										// without risk of endless loop.

	//EXEC SQL BEGIN DECLARE SECTION;	//Marianna
		int		exception_count;
		char	message [255];
		char	my_sqlerrm [601];
		int		messlen;
		int		i;
		int		errm_len;
	//EXEC SQL END DECLARE SECTION;		//Marianna

	// If there's no error, quit now.
	if (code->sqlcode == INF_OK)
	{
		return (MAC_OK);
	}
//GerrLogMini(GerrId, "INF_ERROR_TEST received sqlcode %d.", code->sqlcode);
//GerrLogMini(GerrId, "sttm_str = {%s}", sttm_str);
//GerrLogMini(GerrId, "code->sqlerrm = {%s}, len = %d.", code->sqlerrm, strlen(code->sqlerrm));
//GerrLogMini(GerrId, "SQLCODE = %d, ODBC_SQLCODE = %d.", SQLCODE, ODBC_SQLCODE);
//GerrLogMini(GerrId, "ODBC_ErrorBuffer = {%s}", ODBC_ErrorBuffer);
//GerrLogMini(GerrId, "ODBC_LastSQLStatementText = {%s}", ODBC_LastSQLStatementText);

	// Log the error.
	strcpy (buf,		"");
	strcpy (stmt_buf,	"");
	strcpy (temp,		"");

	// 0. Get the current value of sqlca.sqlerrm.
	// DonR 15Apr2015: I suspect that copying into sqlerrm was causing segmentation faults;
	// accordingly, I've made two changes: First, I lengthened sqlerrm from 255 to 512; and
	// second, I've switched from a "normal" strcpy to strncpy, and added an ASCII zero
	// at the end just in case the source string is longer than 511 characters.
//	sprintf (LastSignOfLife,
//				"INF_ERROR_TEST: SQLCODE = %d, sqlca.sqlerrm %s NULL.",
//				code->sqlcode, (code->sqlerrm == NULL) ? "is" : "is not");

	if (code->sqlerrm == NULL)
	{
		strcpy (my_sqlerrm, "");
	}
	else
	{
//		sprintf (LastSignOfLife,
//					"INF_ERROR_TEST: SQLCODE = %d, sqlca.sqlerrm %s NULL; len = %d, sizeof = %d, {%s}.",
//					code->sqlcode, (code->sqlerrm == NULL) ? "is" : "is not", strlen(code->sqlerrm), sizeof(code->sqlerrm), code->sqlerrm);

//		errm_len = sizeof(code->sqlerrm);
		errm_len = strlen(code->sqlerrm);
		if (errm_len > 600)
			errm_len = 600;
		strncpy (my_sqlerrm, code->sqlerrm, errm_len);
		my_sqlerrm[errm_len] = (char)0;

//		sprintf (LastSignOfLife,
//					"INF_ERROR_TEST: SQLCODE = %d, my_sqlerrm = {%s}.",
//					code->sqlcode, my_sqlerrm);
	}

	// 1. Get the SQL diagnostics.
	// DonR 06Jan2020: If ODBC_SQLCODE == SQLCODE, assume that the current error
	// came from the ODBC routines - so skip the GET DIAGNOSTICS stuff and just
	// use what the ODBC routines have already provided.
			//Marianna
//	if (SQLCODE != ODBC_SQLCODE)
//	{
//		EXEC SQL
//			GET DIAGNOSTICS	:exception_count = NUMBER;
//
//		for (buf[0] = 0, len = 0, i = 1; i <= exception_count; i++)
//		{
//			EXEC SQL
//				GET DIAGNOSTICS	exception :i
//								:message = MESSAGE_TEXT,
//								:messlen = MESSAGE_LENGTH;
//
//			message [messlen - 1] = '\0';
//
//			if (exception_count > 1)
//			{
//				len += sprintf (buf + len, "\n\tINFORMIX MESSAGE %d . : ", i);
//			}
//
//			len += sprintf (buf + len, message);
//		}
//	}	// Current error does NOT appear to have come from ODBC routines.
//	else
//	{
//		strcpy (buf, ODBC_ErrorBuffer);
//	}

	strcpy (buf, ODBC_ErrorBuffer);		//Marianna


	// 2. Get the SQL statement
	if ((ODBC_LastSQLStatementText != NULL) && (SQLCODE == ODBC_SQLCODE))
	{
//GerrLogMini (GerrId, "INF_ERROR_TEST: length of ODBC_LastSQLStatementText = %d.", strlen (ODBC_LastSQLStatementText));
		if (strlen (ODBC_LastSQLStatementText) < 6000)
			len = sprintf (stmt_buf, ODBC_LastSQLStatementText);
	}
//	else
//	{
//		for (temp[0] = i = len = 0;
//			 glbsqltxtarr != NULL && glbsqltxtarr[i] != NULL;	// Vladimir: avoid Linux crashes.
//			 i++)
//		{
//			len += sprintf (temp + len, glbsqltxtarr[i]);
//		}
//	}

	// 3. Get position of column in statement.
	// DonR 06Jan2020: If ODBC_SQLCODE == SQLCODE, assume that the current error
	// came from the ODBC routines - so skip the GET DIAGNOSTICS stuff and just
	// use what the ODBC routines have already provided.
	if (SQLCODE != ODBC_SQLCODE)
	{
		position = code->sqlerrd[4] - 3;
//GerrLogMini (GerrId, "position = %d.", position);

		if (position <= 0)
			position = 0;

		while ((position > 0) && (temp [position] != ' '))
		{
			position--;
		}

		position++;

		end_line = position + 79 - (position % 80);
		column = position % 80;
		len = strlen (temp);

		if (position == 1)
		{
			sprintf (stmt_buf, "%s\n", temp);
		}
		else
		{
			if (len > end_line)
			{
				sprintf (stmt_buf,
						 "%.*s\n%.*s^\n%s",
						 end_line,	temp,
						 column,	minuses,
						 temp + end_line);
			}
			else
			{
				sprintf (stmt_buf,
						 "%s\n%.*s^",
						 temp,
						 column, minuses);
			}
		}	// position != 1
	}	// Current error does NOT appear to have come from ODBC routines.


	// 4. Insert error message into log file.
	// DonR 27Jul2011: If the error is a table-locked one (which is usually the case), substitute a
	// more concise message.
//GerrLogMini (GerrId, "Before printing error, code->sqlcode = %d, sttm_str %s NULL.", code->sqlcode, (sttm_str == NULL) ? "IS" : "IS NOT");
	if (code->sqlcode == -244)
	{
		GerrLogMini (GerrId, "Record-locked error in %s at line %d.", source_name, line_no);
	}
	else	// Something other than table-locked.
	{
		if (sttm_str == NULL)
		{
//GerrLogMini(GerrId, "Calling GmsgLogReturn with sttm_str NULL:\nmessage {%s}\nstatement {%s}\nsqlerrm {%s}", buf,stmt_buf,my_sqlerrm);
			GmsgLogReturn (source_name,
						   line_no,
						   "INFORMIX FATAL ERROR",
						   "\n\tINFORMIX code. . . . : %d\n"
						   "\tINFORMIX message . . : %s\n"
						   "\tINFORMIX statement . :\n%s\n"
						   "\tINFORMIX sqlerrm . . : %s\n",
						   code->sqlcode,
						   buf,
						   stmt_buf,
						   my_sqlerrm);
		}
		else
		{
//GerrLogMini(GerrId, "Calling GmsgLogReturn with sttm_str NOT NULL.");
			GmsgLogReturn (source_name,
						   line_no,
						   "INFORMIX FATAL ERROR for PID %d",
						   "\n\tINFORMIX code. . . . . . : %d\n"
						   "\tINFORMIX mess. . . . . . . : %s\n"
						   "\tINFORMIX statement . . . . : \n%s\n"
						   "\tINFORMIX special message . : %s"
						   "\tINFORMIX sqlerrm . . . . . : %s\n",
						   getpid(),
						   code->sqlcode,
						   buf,
						   stmt_buf,
						   sttm_str,
						   my_sqlerrm);
		}
	}	// Error other than -244.
//GerrLogMini(GerrId, "Back from GmsgLogReturn.");


	// DonR 23Jan2007: Attempt autmatic recovery from DB-disconnected error.
	// DonR 16Jun2008: Added INF_IMPLICIT_NOT_ALLOWED (-1811) and INF_CONNECTION_BROKEN (-25582)
	// to the list of DB errors that will trigger an automatic reconnect attempt.
	if (((code->sqlcode == INF_DB_NOT_SELECTED)			||
		 (code->sqlcode == INF_CONNECTION_NOT_EXIST)	||
		 (code->sqlcode == INF_CURSOR_NOT_AVAILABLE)	||
		 (code->sqlcode == INF_IMPLICIT_NOT_ALLOWED)	||
		 (code->sqlcode == INF_CONNECTION_BROKEN)		||
		 (code->sqlcode == INF_CONNECT_ATTEMPT_FAILED))			&&
		(allow_recursion > 0))
	{
		allow_recursion = 0;	// Disable any further recursion.

		SQLMD_connect();		// Try reconnecting to DB - if there is an error,
								// this routine will be called again - but because
								// the recursion flag has been set to zero, we
								// won't loop any deeper!

		GmsgLogReturn (source_name,
					   line_no,
					   "AUTOMATIC RECONNECT TO DATABASE",
					   "Auto reconnect %s successful - SQLCODE = %d.%s\n",
					   (_db_connected_) ? "was" : "was NOT ",
					   SQLCODE,
					   (_db_connected_ || (!_PauseAfterFailedReconnect)) ? "" : " Sleeping for a minute before retry...");

		allow_recursion = 1;	// Reset recursion flag.

		// DonR 14May2015: If an automatic reconnect attempt failed, pause for a minute - this will
		// prevent filling the disk with gigabytes of error messages, particularly from As400UnixClient.
		if ((!_db_connected_) && (_PauseAfterFailedReconnect))
			sleep (60);
	}


	// DonR 30Jun2015: I still seem to be seeing some cases (in Test, at least) where As400UnixClient
	// goes into some kind of loop and fills the disk with logged messages. Since this is the only
	// program that sets _PauseAfterFailedReconnect non-zero, it seems reasonable to add a small delay
	// after *any* database error when this flag is set.
	if (_PauseAfterFailedReconnect)
		usleep (250000);	// 1/4 second delay.

	// Exit program with error code.
	return (INF_ERR);

}



/*===========================================================================
||																			||
||                              INF_ERROR_CLEAR()							||
||																			||
||	Clear informix sqlca & SQLCODE of any previous error, unconditionally.	||
||		(Does NOT clear sqlerrd array - so "no rows affected" will still	||
||			be detected.)													||
 ===========================================================================*/
int	INF_ERROR_CLEAR (struct sqlca_s	*code)
{
	code->sqlcode = SQLCODE = INF_OK;

	return (MAC_OK);
}



/*=======================================================================
||																		||
||				INF_CODE_CMP()											||
||																		||
||				Check return status										||
||			Return 1 if value match, 0 otherwise						||
 =======================================================================*/
int	INF_CODE_CMP( 
		     struct	sqlca_s *code_strct,/*sqlca structur ptr*/
		     int	error_type	/* error type		*/
		     )
{

/*======================================================================
||		Test error code in structure is of error type		||
 ======================================================================*/

    switch( error_type )
    {
        case SQLERR_ok:
            return( (code_strct->sqlcode != INF_OK)
		    ? MAC_FALS : MAC_TRUE );

		case SQLERR_end_of_fetch:
        case SQLERR_not_found:
            return( (code_strct->sqlcode != INF_NO_MORE_DATA_OR_NOT_FOUND )
		    ? MAC_FALS : MAC_TRUE );

        case SQLERR_no_rows_affected:
            return( (code_strct->sqlerrd[2] != 0 )
		    ? MAC_FALS : MAC_TRUE );

        case SQLERR_deadlocked:
            return( (code_strct->sqlcode != INF_DEADLOCK)
		    ? MAC_FALS : MAC_TRUE );

        case SQLERR_access_conflict:
            return( (code_strct->sqlcode != INF_ACCESS_CONFLICT &&
		    code_strct->sqlcode != INF_CANNOT_POSITION_WITHIN_TABL &&
		    code_strct->sqlcode != INF_CANNOT_POSITION_VIA_INDEX &&
		    code_strct->sqlcode != INF_CANNOT_GET_NEXT_ROW &&
		    code_strct->sqlcode != INF_TABLE_IS_LOCKED_FOR_INSERT )
		    ? MAC_FALS : MAC_TRUE );

        case SQLERR_access_conflict_cursor:
            return( ( code_strct->sqlcode != INF_CANNOT_READ_SYS_CATALOG &&
		    code_strct->sqlcode != INF_CANNOT_POSITION_WITHIN_TABL )
		    ? MAC_FALS : MAC_TRUE );

        case SQLERR_not_unique: 
            return( (code_strct->sqlcode != INF_NOT_UNIQUE_INDEX &&
		     code_strct->sqlcode != INF_NOT_UNIQUE_CONSTRAINT)
		    ? MAC_FALS : MAC_TRUE );

	case SQLERR_referenced_value_in_use:
            return ( (code_strct->sqlcode != INF_REFERENCED_VALUE_IN_USE )
		     ? MAC_FALS : MAC_TRUE );

	case SQLERR_referenced_value_not_found:
            return ( (code_strct->sqlcode != INF_REFERENCED_VALUE_NOT_FOUND)
		     ? MAC_FALS : MAC_TRUE );

	case SQLERR_too_many_result_rows:
            return ( (code_strct->sqlcode != INF_TOO_MANY_RESULT_ROWS)
		     ? MAC_FALS : MAC_TRUE );

	case SQLERR_column_not_found_in_tables:
            return ( (code_strct->sqlcode != INF_COLUMN_NOT_FOUND_IN_TABLES)
		     ? MAC_FALS : MAC_TRUE );
	    
    }

    return( MAC_FALS );				/* default --- NO MATCH */
}

/*=======================================================================
||                                                                      ||
||			CURRENT_STRING_DATE()				||
||                                                                      ||
||	Returns current date & time in dotted formatted string.		||
||                                                                      ||
 =======================================================================*/
void	CURRENT_DATE(
		     char	*dbt		/* dotted string date	*/
		     )
{
    time_t     current_time;
    struct tm  *cur_tm;

    time( &current_time );
    cur_tm = localtime( &current_time );

    sprintf( dbt, "%04d.%02d.%02d.%02d.%02d.%02d",
		    cur_tm->tm_year + 1900,
		    cur_tm->tm_mon  + 1,
		    cur_tm->tm_mday,
		    cur_tm->tm_hour,
		    cur_tm->tm_min,
		    cur_tm->tm_sec );

}

/*=======================================================================
||									||
||				diff_from_now()				||
||									||
||		Returns diffrence in seconds from now			||
 =======================================================================*/
int	diff_from_now(
		      int	inp_date,
		      int	inp_time
		      )
{
	return (diff_from_then (inp_date, inp_time, GetDate(), GetTime()));
}

/*=======================================================================
||																		||
||				diff_from_then ()										||
||																		||
||		Returns difference in seconds from T0 to T1						||
 =======================================================================*/
int	diff_from_then (int	date0_in,
					int	time0_in,
					int	date1_in,
					int	time1_in)
{
	time_t		time0;
	time_t		time1;
	struct tm	tm_str0;
	struct tm	tm_str1;
	
	date_to_tm (date0_in, &tm_str0);
    time_to_tm (time0_in, &tm_str0);
	date_to_tm (date1_in, &tm_str1);
    time_to_tm (time1_in, &tm_str1);

    time0 = mktime (&tm_str0);
    time1 = mktime (&tm_str1);

    return (abs (time1 - time0));
}

/*=======================================================================
||									||
||				STRING_DATE_TO_TM()			||
||									||
||	Convert date from dotted string format to struct tm.		||
||									||
 =======================================================================*/
void	STRING_DATE_TO_TM(
			  char		*dbt,	/* dotted string date	*/
			  struct tm	*tm_strct/*struct tm to get date*/
			  )
{
    tm_strct->tm_year	=	atoi(&dbt[0]) - 1900;
    tm_strct->tm_mon	=	atoi(&dbt[5]) - 1;
    tm_strct->tm_mday	=	atoi(&dbt[8]);
    tm_strct->tm_hour	=	atoi(&dbt[11]);
    tm_strct->tm_min	=	atoi(&dbt[14]);
    tm_strct->tm_sec	=	atoi(&dbt[17]);
}


#if 0
/*=======================================================================
||																		||
||				DATABASE_CONNECTED()									||
||																		||
||			Return DB connection status									||
||																		||
 =======================================================================*/
int DATABASE_CONNECTED ()
{
	return (_db_connected_ == 1);
}
#endif



/*=======================================================================
||																		||
||				INF_CONNECT()											||
||																		||
||			Connect to Informix database								||
||																		||
 =======================================================================*/
int	INF_CONNECT(
		    char	*username,	/* username		*/
		    char	*password,	/* password		*/
		    char	*dbname		/* database name	*/
		    )
{
	int	sql_result;

	char			*ODBC_DSN;
	char			*ODBC_UserName;
	char			*ODBC_Password;
	char			*ODBC_DB_name;

	//Marianna
//	if (!Connect_ODBC_Only)
//	{
//		EXEC SQL BEGIN DECLARE SECTION;
//			char	*user;
//			char	*pass;
//			char	*db;
//		EXEC SQL END DECLARE SECTION;
//
//		user	=	username;
//		pass	=	password;
//		db		=	dbname;
//
//		// Connect to database - if we're retrying after an ODBC connect
//		// failure and we're already connected the old way, don't attempt
//		// to reconnect.
//		if (!_db_connected_)
//		{
//			EXEC SQL
//				CONNECT TO	:db
//				USER		:user
//				USING		:pass;
//
//			// Test error
//			sql_result = SQLERR_error_test();
//
//			_db_connected_ = (sql_result == INF_OK) ? 1 : 0;
//
////			if (sql_result != 0)
//				GerrLogMini (GerrId, "CONNECT (old style) to %s/%s/%s returns %d.", db, user, pass, sql_result);
//		}
//	}
//	else
//	{
//		_db_connected_ = 0;
//		GerrLogMini (GerrId, "Old-style DB CONNECT disabled.");
//	}


	MS_DB.Connected = INF_DB.Connected = 0;

	// DonR 04Dec2019: Open ODBC database(s).
	ODBC_DSN		= getenv ("MS_SQL_ODBC_DSN");
	ODBC_UserName	= getenv ("MS_SQL_ODBC_USER");
	ODBC_Password	= getenv ("MS_SQL_ODBC_PASS");
	ODBC_DB_name	= getenv ("MS_SQL_ODBC_DB");

	if ((ODBC_DSN		!= NULL)	&&
		(ODBC_UserName	!= NULL)	&&
		(ODBC_Password	!= NULL)	&&
		(ODBC_DB_name	!= NULL))
	{
		sql_result = ODBC_CONNECT (&MS_DB, ODBC_DSN, ODBC_UserName, ODBC_Password, ODBC_DB_name);
		GerrLogMini (GerrId, "ODBC connect to MS-SQL %s/%s/%s/%s returns %d, connected = %d.",
					 ODBC_DSN, ODBC_UserName, ODBC_Password, ODBC_DB_name, sql_result, MS_DB.Connected);
	}

	ODBC_DSN		= getenv ("INF_ODBC_DSN");
	ODBC_UserName	= getenv ("INF_ODBC_USER");
	ODBC_Password	= getenv ("INF_ODBC_PASS");
	ODBC_DB_name	= NULL;

	if ((ODBC_DSN		!= NULL)	&&
		(ODBC_UserName	!= NULL)	&&
		(ODBC_Password	!= NULL))
	{
		sql_result = ODBC_CONNECT (&INF_DB, ODBC_DSN, ODBC_UserName, ODBC_Password, ODBC_DB_name);

		if (sql_result)
		{
			GerrLogMini (GerrId, "ODBC connect to Informix %s/%s/%s/%s returns %d, connected = %d.",
						 ODBC_DSN, ODBC_UserName, ODBC_Password, ODBC_DB_name, sql_result, INF_DB.Connected);
		}
//else GerrLogMini (GerrId, "ODBC connect to Informix %s/%s/%s/%s returns %d, connected = %d.",
//					ODBC_DSN, ODBC_UserName, ODBC_Password, ODBC_DB_name, sql_result, INF_DB.Connected);
	}
//else GerrLogMini (GerrId, "Informix environment variables NOT OK: %s/%s/%s.", ODBC_DSN, ODBC_UserName, ODBC_Password);

    return (sql_result);
    
}

/*=======================================================================
||																		||
||				INF_DISCONNECT()										||
||																		||
||			Disconnect from Informix database							||
||																		||
 =======================================================================*/
int	INF_DISCONNECT ()
{
	CleanupODBC (&INF_DB);
	CleanupODBC (&MS_DB);

	_db_connected_ = 0;

		//Marianna
//	if (!Connect_ODBC_Only)
//	{
//		// Disconnect from Informix database (old-style connection).
//		EXEC SQL
//			DISCONNECT CURRENT;
//
//		// Test error
//		return (SQLERR_error_test ());
//	}
//	else
//	{
		return (0);
//	}
}

/*=======================================================================
||																		||
||				INF_BEGIN()												||
||																		||
||			Begin transaction in Informix database						||
||																		||
 =======================================================================*/
		//Marianna
int	INF_BEGIN()			//Marianna	25052020
{
//	if (!Connect_ODBC_Only)
//	{
//		// Begin work
//		// TEMPORARY - Just to avoid stupid "Already in transaction" errors!
//		EXEC SQL COMMIT WORK;
//	    EXEC SQL BEGIN WORK;
//
//		// Test error
//		if (SQLCODE == -535)
//		{
//			GerrLogMini (GerrId, "INF_BEGIN: Already in transaction.");
//		}
//		else
//		{
//			SQLERR_error_test();
//		}
//
//	    return ((SQLCODE == 0) ? 0 : INF_ERR);
//	}
//	else
//	{
		return (0);		//Marianna 25052020
//	}
}

/*=======================================================================
||									||
||				INF_COMMIT()				||
||									||
||			Commit Informix transaction			||
||									||
 =======================================================================*/
			//Marianna
int	INF_COMMIT()			//Marianna 25052020
{
//	if (!Connect_ODBC_Only)
//	{
//		// Commit work on Informix database
//	    EXEC SQL COMMIT WORK;
//
//		// Test error
//		return( SQLERR_error_test() );
//	}
//	else
//	{
		return (0);
//	}
}

/*=======================================================================
||									||
||				INF_ROLLBACK()				||
||									||
||			Rollback Informix transaction			||
||									||
 =======================================================================*/
		//Marianna
int	INF_ROLLBACK()		//Marianna 25052020
{
//	if (!Connect_ODBC_Only)
//	{
//		// Rollback work on Informix database
//	    EXEC SQL ROLLBACK WORK;
//
//		// Test error
//		// DonR 22Apr2013: If we haven't opened a DB transaction, write a short log message
//		// but don't return an error.
//		if (SQLCODE == -255)
//		{
//			GerrLogMini (GerrId, "Rollback requested, but not in transaction.");
//			return (0);
//		}
//		else
//		{
//			return (SQLERR_error_test ());
//		}
//	}
//	else
//	{
		return (0);
//	}
}

/*=======================================================================
||																		||
||				INF_CONNECT_ID()										||
||																		||
||			Connect to Informix database with id						||
||																		||
 =======================================================================*/
		//Marianna
int	INF_CONNECT_ID(
		       char	*username,	/* username		*/
		       char	*password,	/* password		*/
		       char	*dbname,	/* database name	*/
		       char	*conn_id	/* connection id	*/
		       )										//Marianna 25052020
{
//	if (!Connect_ODBC_Only)
//	{
//		int	sql_result;
//
//		EXEC SQL BEGIN DECLARE SECTION;
//			char	*user;
//			char	*pass;
//			char	*db;
//			char	*c_id;
//		EXEC SQL END DECLARE SECTION;
//
//		user	=	username;
//		pass	=	password;
//		db		=	dbname;
//		c_id	=	conn_id;
//
//		// Connect to database
//		EXEC SQL
//			CONNECT TO	:db
//			AS			:c_id
//			USER		:user
//			USING		:pass
//			WITH CONCURRENT TRANSACTION;
//
//		// Test error
//		sql_result = SQLERR_error_test();
//
//		_db_connected_ = (sql_result == INF_OK) ? 1 : 0;
//
//		return (sql_result);
//	}
//	else
//	{
		return (0);
//	}
}

/*=======================================================================
||																		||
||				INF_DISCONNECT_ID()										||
||																		||
||			Disconnect from Informix database							||
||																		||
 =======================================================================*/
		//Marianna
int	INF_DISCONNECT_ID(
			  char	*conn_id	/* connection id	*/
			  )										//Marianna 25052020
{
//	if (!Connect_ODBC_Only)
//	{
//		EXEC SQL BEGIN DECLARE SECTION;
//			char	*c_id;
//		EXEC SQL END DECLARE SECTION;
//
//		// Disconnect from Informix database
//		c_id	=	conn_id;
//
//		EXEC SQL
//		  DISCONNECT :c_id;
//
//		_db_connected_ = 0;
//
//		// Test error
//		return( SQLERR_error_test() );
//	}
//	else
//	{
		return (0);
//	}
}

/*=======================================================================
||									||
||				INF_SET_CONNECTION()			||
||									||
||			Switch to Informix database connection		||
||									||
 =======================================================================*/
		//Marianna
int	INF_SET_CONNECTION(
			   char	*conn_id	/* connection id	*/
			   )				//Marianna 25052020
{
//	if (!Connect_ODBC_Only)
//	{
//		EXEC SQL BEGIN DECLARE SECTION;
//			char	*c_id;
//		EXEC SQL END DECLARE SECTION;
//
//		// Disconnect from Informix database
//	    c_id	=	conn_id;
//
//	    EXEC SQL
//		  SET CONNECTION :c_id;
//
//		// Test error
//	    return( SQLERR_error_test() );
//	}
//	else
//	{
		return (0);
//	}
}


/*=======================================================================
||									||
||				RefreshPharmacyTable()			||
||									||
||			Refresh pharmacy shared memory table		||
||									||
 =======================================================================*/
int	RefreshPharmacyTable(
			     int	entry
			     )
{
#if 0

//    EXEC SQL BEGIN DECLARE SECTION;
//
//    static PHARMACY_ROW	pharmacy_row;
//
//    PHARMACY_ROW	*pharmacy_row_ptr;
//   
//    EXEC SQL END DECLARE SECTION;
//
//    long		
//			curr_num,
//			temp;
//
//    static char		*buffer;
//
//    static short int	first_visit = MAC_TRUE;
//    static long int	alloc_num = 700;	/* may be taken from db */
//
//    RAW_DATA_HEADER	raw_data_header,
//			*raw_data_header_ptr;
//
//    TABLE		*phrm_tablep;
//	ISOLATION_VAR;
//
/*
 *  Declare cursor for select
 *  all table pharmacy & allocate memory
 *  ------------------------------------
 */
//	REMEMBER_ISOLATION;
//
//    if( first_visit == MAC_TRUE  )
//    {
//	alloc_num = 500;
//
//	buffer =
//	  MemAllocReturn(
//			 GerrId,
//			 sizeof(RAW_DATA_HEADER) +
//			 sizeof(PHARMACY_ROW) * alloc_num,
//			 "memory for pharmacy table raw data"
//			 );
//
//	if( buffer == NULL )
//	{
//		RESTORE_ISOLATION;
//	    return( MEM_ALLOC_ERR );
//	}
//
//	EXEC SQL
//	  DECLARE	pharm_cur CURSOR FOR
//	  SELECT	pharmacy_code,
//			institued_code,
//			permission_type,
//			price_list_code,
//			open_close_flag
//	
//	  INTO		:pharmacy_row.pharmacy_num,
//			:pharmacy_row.institute_code,
//			:pharmacy_row.info.pharmacy_permission,
//			:pharmacy_row.info.price_list_num,
//			:pharmacy_row.info.open_close_flg
//	
//	  FROM		pharmacy
//	  WHERE		del_flg	= :ROW_NOT_DELETED;
//    }
//
//    first_visit = MAC_FALS;
//    
//    for(
//	restart = MAC_TRUE,
//	tries = 0;
//	tries < SQL_UPDATE_RETRIES && restart == MAC_TRUE;
//	tries++
//	)
//    {
//	restart = MAC_FALS;
//
	/*
	 * Begin work
	 * ----------
	 */
//	SQLMD_begin();
//
//	if( state = SQLERR_error_test() )
//	{
//	    break;
//	}
//
	/*
	 *  Open cursor
	 *  -----------
	 */
//	SET_ISOLATION_DIRTY;
//
//	EXEC SQL
//	  OPEN  pharm_cur;
//
//	Conflict_Test_Cur( restart );
//
//	state = SQLERR_error_test();
//
//	if( state == MAC_OK	&&
//	    restart == MAC_FALS )
//	{
		/*
	     *  select all pharmacy table
	     *  -------------------------
	     */
//	    for(
//		pharmacy_row_ptr =
//		  (PHARMACY_ROW*)(buffer + sizeof(RAW_DATA_HEADER)),
//		curr_num = 0;
//		/* ever */;
//		curr_num++,
//		pharmacy_row_ptr++
//		)
//	    {
		/*
		 * Check for free space
		 * --------------------
		 */
//		if( curr_num	>= alloc_num - 1 )
//		{
//		    alloc_num +=
//		      500;
//		    
//		    buffer =
//		      MemReallocReturn(
//				       GerrId,
//				       buffer,
//				       sizeof(RAW_DATA_HEADER) +
//				       sizeof(PHARMACY_ROW) * alloc_num,
//				       "memory for pharmacy table raw data"
//				       );
//
//		    if( buffer == NULL )
//		    {
//				RESTORE_ISOLATION;
//				return( MEM_ALLOC_ERR );
//		    }
//
//		    pharmacy_row_ptr	=
//		      (PHARMACY_ROW*)(
//				      buffer +
//				      sizeof(RAW_DATA_HEADER) +
//				      sizeof(PHARMACY_ROW) * curr_num
//				      );
//		} /* if( curr_num ... ) */
//
//		/*
//		 * Fetch row from table
//		 * --------------------
//		 */
//		EXEC SQL
//		  FETCH pharm_cur;
//		
//		Conflict_Test( restart );
//
		/*
		 * Exit loop on error / end of fetch
		 */
//		BREAK_ON_TRUE( SQLERR_code_cmp( SQLERR_end_of_fetch ) );
//
//		BREAK_ON_ERR( SQLERR_error_test() );
//
//		*pharmacy_row_ptr = pharmacy_row;
//
//	    } /* for( curr_num .. ) */
//
	    /*
	     * Close pharmacy cursor
	     * ---------------------
	     */
//	    EXEC SQL
//	      CLOSE pharm_cur;
//
//	    SQLERR_error_test();
//
//	} /* if( restart ... ) */
//
//	if( restart == MAC_TRUE )	/* transaction faild -- try again */
//	{
//	    SQLMD_rollback();
//
//	    BREAK_ON_ERR( SQLERR_error_test() );
//
//	    sleep( ACCESS_CONFLICT_SLEEP_TIME );
//	}
//	else				/* transaction succed -- finish   */
//	{
//	    SQLMD_commit();
//
//	    SQLERR_error_test();
//	}

//    } /* for( restart = ... ) */

    if( restart == MAC_TRUE )
    {
	GerrLogReturn(
		      GerrId,
		      "Table '%s' locked.. failed after '%d' retries",
		      "pharmacy",
		      SQL_UPDATE_RETRIES
		      );

	RESTORE_ISOLATION;
	return( INF_ERR );
    }

/*
 * Set buffer header
 * -----------------
 */
    raw_data_header.actual_rows	= curr_num;
    
    raw_data_header.alloc_rows	= alloc_num;

    raw_data_header_ptr =
      (RAW_DATA_HEADER*)buffer;

    *raw_data_header_ptr = raw_data_header;

/*
 * Sort buffer by pharmacy_code,institue_code
 * ------------------------------------------
 */
    qsort(
	  buffer +
	  sizeof(RAW_DATA_HEADER),
	  curr_num,
	  sizeof(PHARMACY_ROW),
	  PhrmCompare
	  );
      
/*
 * replace buffer in pharmacy table in shm
 * ---------------------------------------
 */
	RESTORE_ISOLATION;

    RETURN_ON_ERR(
		  OpenTable(
			    PHRM_TABLE,
			    &phrm_tablep
			    )
		  );

    RETURN_ON_ERR(
		  SetRecordSize(
				phrm_tablep,
				sizeof(RAW_DATA_HEADER) +
				sizeof(PHARMACY_ROW) * curr_num
				)
		  );

    RETURN_ON_ERR(
		  AddItem(
			  phrm_tablep,
			  buffer
			  )
		  );

    RETURN_ON_ERR(
		  CloseTable(
			     phrm_tablep
			     )
		  );
#endif

/*
 *  Return -- ok
 *  ------------
 */
    return( MAC_OK );
}


/*=======================================================================
||																		||
||				RefreshSeverityTable()									||
||																		||
||			Refresh severity shared memory table						||
||																		||
 =======================================================================*/
int	RefreshSeverityTable (int	entry)
{
	static short		first_visit = MAC_TRUE;
	static int			alloc_num = 200;	/* may be taken from db */
	static char			*buffer;
	static SEVERITY_ROW	severity_row;
	SEVERITY_ROW		*severity_row_ptr;
	int					curr_num;
	int					temp;
	RAW_DATA_HEADER		raw_data_header;
	RAW_DATA_HEADER		*raw_data_header_ptr;
	TABLE				*sevr_tablep;
	ISOLATION_VAR;


	REMEMBER_ISOLATION;

	if (first_visit == MAC_TRUE)
	{
		alloc_num = 100;

		buffer = MemAllocReturn (GerrId,
								 sizeof(RAW_DATA_HEADER) + sizeof(SEVERITY_ROW) * alloc_num,
								 "memory for severity table raw data");
	
		if (buffer == NULL)
		{
			RESTORE_ISOLATION;
			return (MEM_ALLOC_ERR);
		}

//		EXEC SQL
//		  DECLARE	sever_cur CURSOR FOR
//		  SELECT	error_code,
//				severity_level
//	
//		  INTO		:severity_row.error_code,
//				:severity_row.severity_level
//	
//		  FROM		pc_error_message;
	}

	first_visit = MAC_FALS;
    
	for (restart = MAC_TRUE, tries = 0; tries < SQL_UPDATE_RETRIES && restart == MAC_TRUE; tries++)
	{
		restart = MAC_FALS;

		if (state = SQLERR_error_test ())
		{
			break;
		}

		DeclareCursorInto (	&INF_DB, RefreshSeverityTable_cur,
							&severity_row.error_code,	&severity_row.severity_level,	END_OF_ARG_LIST		);

		SET_ISOLATION_DIRTY;

		OpenCursor (	&INF_DB, RefreshSeverityTable_cur	);
//		EXEC SQL
//		  OPEN  sever_cur;

		Conflict_Test_Cur (restart);

		state = SQLERR_error_test();

		if (state == MAC_OK	&& restart == MAC_FALS)
		{
			for (severity_row_ptr = (SEVERITY_ROW *)(buffer + sizeof(RAW_DATA_HEADER)), curr_num = 0;
				 /* No exit condition */;
				 curr_num++, severity_row_ptr++
				)
			{
				// Check for free space
				if (curr_num >= alloc_num - 1)
				{
					alloc_num += 100;
		    
					buffer = MemReallocReturn (GerrId,
											   buffer,
											   sizeof(RAW_DATA_HEADER) + sizeof(SEVERITY_ROW) * alloc_num,
											   "memory for severity table raw data");

					if (buffer == NULL)
					{
						RESTORE_ISOLATION;
						return (MEM_ALLOC_ERR);
					}

					severity_row_ptr = (SEVERITY_ROW *)(buffer + sizeof(RAW_DATA_HEADER) + sizeof(SEVERITY_ROW) * curr_num);
				}

				FetchCursor (	&INF_DB, RefreshSeverityTable_cur	);
//				EXEC SQL
//					FETCH sever_cur;
		
				Conflict_Test (restart);

				// Exit loop on error / end of fetch
				BREAK_ON_TRUE (SQLERR_code_cmp (SQLERR_end_of_fetch));

				BREAK_ON_ERR (SQLERR_error_test ());

				*severity_row_ptr = severity_row;

			} /* for( curr_num = ... ) */

			CloseCursor (	&INF_DB, RefreshSeverityTable_cur	);
//			EXEC SQL
//				CLOSE sever_cur;
//
//			SQLERR_error_test();

		} /* if( restart == ... ) */

		if (restart == MAC_TRUE)	// transaction failed -- try again
		{
			sleep (ACCESS_CONFLICT_SLEEP_TIME);
		}

	} /* for( restart = ... ) */

	if (restart == MAC_TRUE)
	{
		GerrLogMini (GerrId, "Table pc_error_message locked...failed after %d retries", SQL_UPDATE_RETRIES);
		return (INF_ERR);
	}

	// Set buffer header
	raw_data_header.actual_rows	= curr_num;
    
	raw_data_header.alloc_rows	= alloc_num;

	raw_data_header_ptr = (RAW_DATA_HEADER*)buffer;

	*raw_data_header_ptr = raw_data_header;

	// Sort buffer by error_code, severity_level
	qsort (buffer + sizeof(RAW_DATA_HEADER), curr_num, sizeof(SEVERITY_ROW), SevrCompare);

	// replace buffer in severity table in shm
	RESTORE_ISOLATION;

	RETURN_ON_ERR ( OpenTable (SEVR_TABLE, &sevr_tablep) );

	RETURN_ON_ERR ( SetRecordSize (sevr_tablep, sizeof(RAW_DATA_HEADER) + sizeof(SEVERITY_ROW) * curr_num) );

	RETURN_ON_ERR ( AddItem (sevr_tablep, buffer) );

	RETURN_ON_ERR ( CloseTable (sevr_tablep ) );

	return( MAC_OK );
}


/*=======================================================================
||									||
||				RefreshPercentTable()			||
||									||
||			Refresh percent shared memory table		||
||									||
 =======================================================================*/
int	RefreshPercentTable(
			    int	entry
			    )
{
#if 0

//    EXEC SQL BEGIN DECLARE SECTION;

    static PERCENT_ROW	percent_row;

    PERCENT_ROW	*percent_row_ptr;

//    EXEC SQL END DECLARE SECTION;

    long
			curr_num,
			temp;

    static char		*buffer;

    static short int	first_visit = MAC_TRUE;
    static long int	alloc_num = 80000;	/* may be taken from db */

    RAW_DATA_HEADER	raw_data_header,
			*raw_data_header_ptr;

    TABLE		*pcnt_tablep;
	ISOLATION_VAR;

/*
 *  Declare cursor for select
 *  all table percent
 *  -------------------------
 */
	REMEMBER_ISOLATION;

    if( first_visit == MAC_TRUE )
    {
	alloc_num = 100;

	buffer =
	  MemAllocReturn(
			 GerrId,
			 sizeof(RAW_DATA_HEADER) +
			 sizeof(PERCENT_ROW) * alloc_num,
			 "memory for percent table raw data"
			 );

	if( buffer == NULL )
	{
		RESTORE_ISOLATION;
	    return( MEM_ALLOC_ERR );
	}

//	EXEC SQL
//	  DECLARE	prcnt_cur CURSOR FOR
//	  SELECT	member_price_code,
//			member_price_prcnt
//
//	  INTO		:percent_row.percent_code,
//			:percent_row.percent
//
//	  FROM		member_price;
    }

    first_visit = MAC_FALS;

    for(
	restart = MAC_TRUE,
	tries = 0;
	tries < SQL_UPDATE_RETRIES && restart == MAC_TRUE;
	tries++
	)
    {
	restart = MAC_FALS;

	/*
	 * Begin work
	 * ----------
	 */
//	SQLMD_begin();

	if( state = SQLERR_error_test() )
	{
	    break;
	}

	/*
	 *  Open cursor
	 *  -----------
	 */
	SET_ISOLATION_DIRTY;

//	EXEC SQL
//	  OPEN  prcnt_cur;

	Conflict_Test_Cur( restart );

	state = SQLERR_error_test();

	if( state == MAC_OK	&&
	    restart == MAC_FALS )
	{
	    /*
	     *  select all percent table
	     *  -------------------------
	     */
	    for(
		percent_row_ptr	=
		  (PERCENT_ROW*)(buffer + sizeof(RAW_DATA_HEADER)),
		curr_num = 0;
		/* ever */;
		curr_num++,
		percent_row_ptr++
		)
	    {

		/*
		 * Check for free space
		 * --------------------
		 */
		if( curr_num	>= alloc_num - 1)
		{
		    alloc_num +=
		      100;

		    buffer =
		      MemReallocReturn(
				       GerrId,
				       buffer,
				       sizeof(RAW_DATA_HEADER) +
				       sizeof(PERCENT_ROW) * alloc_num,
				       "memory for percent table raw data"
				       );

		    if( buffer == NULL )
		    {
				RESTORE_ISOLATION;
				return( MEM_ALLOC_ERR );
		    }

		    percent_row_ptr	=
		      (PERCENT_ROW*)(
				      buffer +
				      sizeof(RAW_DATA_HEADER) +
				      sizeof(PERCENT_ROW) * curr_num
				      );
		}

		/*
		 * Fetch row from table
		 * --------------------
		 */
//		EXEC SQL
//		  FETCH prcnt_cur;

		Conflict_Test( restart );

		/*
		 * Exit loop on error / end of fetch
		 */
		BREAK_ON_TRUE( SQLERR_code_cmp( SQLERR_end_of_fetch ) );

		BREAK_ON_ERR( SQLERR_error_test() );

		*percent_row_ptr = percent_row;

	    } /* for( curr_num = ... ) */

	    /*
	     * Close & free cursor
	     * -------------------
	     */
//	    EXEC SQL
//	      CLOSE prcnt_cur;

	    SQLERR_error_test();

	} /* if( restart = ... ) */

	if( restart == MAC_TRUE )	/* transaction faild -- try again */
	{
//	    SQLMD_rollback();

	    BREAK_ON_ERR( SQLERR_error_test() );

	    sleep( ACCESS_CONFLICT_SLEEP_TIME );
	}
	else				/* transaction succed -- finish   */
	{
//	    SQLMD_commit();

	    SQLERR_error_test();
	}

    } /* for( restart = ... ) */

    if( restart == MAC_TRUE )
    {
		GerrLogReturn(
				  GerrId,
				  "Table '%s' locked.. failed after '%d' retries",
				  "member_price",
				  SQL_UPDATE_RETRIES
				  );

		RESTORE_ISOLATION;
		return( INF_ERR );
    }

/*
 * Set buffer header
 * -----------------
 */
    raw_data_header.actual_rows	= curr_num;

    raw_data_header.alloc_rows	= alloc_num;

    raw_data_header_ptr =
      (RAW_DATA_HEADER*)buffer;

    *raw_data_header_ptr = raw_data_header;

/*
 * Sort buffer by percent_code
 * ---------------------------
 */
    qsort(
	  buffer +
	  sizeof(RAW_DATA_HEADER),
	  curr_num,
	  sizeof(PERCENT_ROW),
	  PcntCompareByCode
	  );

/*
 * replace buffer in percent table in shm
 * ---------------------------------------
 */
	RESTORE_ISOLATION;

    RETURN_ON_ERR(
		  OpenTable(
			    PCNT_TABLE,
			    &pcnt_tablep
			    )
		  );

    RETURN_ON_ERR(
		  SetRecordSize(
				pcnt_tablep,
				sizeof(RAW_DATA_HEADER) +
				sizeof(PERCENT_ROW) * curr_num
				)
		  );

    RETURN_ON_ERR(
		  AddItem(
			  pcnt_tablep,
			  buffer
			  )
		  );

    RETURN_ON_ERR(
		  CloseTable(
			     pcnt_tablep
			     )
		  );

#endif

/*
 *  Return -- ok
 *  ------------
 */
    return( MAC_OK );
}


/*=======================================================================
||									||
||			GET_PARTICIPATING_PERCENT()			||
||									||
||		Get participating percent by percent type		||
||									||
 =======================================================================*/
//TErrorCode
//	GET_PARTICIPATING_PERCENT(
//				  TParticipatingType	percent_code,
//				  TParticipatingPrecent	*percent
//				  )
//{
//    TABLE		*pcnt_tablep;
//
//    PERCENT_ROW		percent_row;
//
//    int			state;
//    
//    PHRM_RET_ON_ERR(
//		    OpenTable(
//			      PCNT_TABLE,
//			      &pcnt_tablep
//			      )
//		    );
//
//    percent_row.percent_code =
//      percent_code;
//    
//    state =
//      ActItems(
//	       pcnt_tablep,
//	       1,
//	       GetPercentByCode,
//	       (OPER_ARGS)&percent_row
//	       );
//
//    PHRM_RET_ON_ERR(
//		    CloseTable(
//			       pcnt_tablep
//			       )
//		    );
//
//    if(
//       state == NO_MORE_ROWS		||
//       percent_row.percent == -1
//       )
//    {
//	GerrLogReturn(
//		      GerrId,
//		      "No percent for percent_code '%d' in member_price table - assuming 100 percent!",
//		      (int)percent_code
//		      );
//	*percent = 10000;	// DonR 26Mar2003: Put this in to avoid crashes.
//
//	return( ERR_PARTICI_CODE_NOT_FOUND );
//    }
//
//    PHRM_RET_ON_ERR( state );
//    
//    *percent = percent_row.percent;
//
//    return( NO_ERROR );
//}
//
//
/*=======================================================================
||									||
||			GET_PARTICIPATING_CODE()			||
||									||
||		Compare participating lines by : percent		||
||									||
 =======================================================================*/
//TErrorCode
//	GET_PARTICIPATING_CODE(
//			       TParticipatingPrecent	percent,
//			       TParticipatingType	*percent_code
//			       )
//{
//   TABLE		*pcnt_tablep;
//
//    PERCENT_ROW		percent_row;
//
//    int			state;
//    
//    PHRM_RET_ON_ERR(
//		    OpenTable(
//			      PCNT_TABLE,
//			      &pcnt_tablep
//			      )
//		    );
//
//    percent_row.percent =
//      percent;
//    
//    state =
//      ActItems(
//	       pcnt_tablep,
//	       1,
//	       GetCodeByPercent,
//	       (OPER_ARGS)&percent_row
//	       );
//
//    PHRM_RET_ON_ERR(
//		    CloseTable(
//			       pcnt_tablep
//			       )
//		    );
//
//    if(
//       state == NO_MORE_ROWS		||
//       percent_row.percent_code == -1
//       )
//    {
//	GerrLogReturn(
//		      GerrId,
//		      "No percent_code for percent '%d' in member_price table",
//		      percent
//		      );
//
//	return( ERR_PARTICIPATING_PERCENT_WRONG );
//    }
//
//    PHRM_RET_ON_ERR( state );
//    
//    *percent_code = percent_row.percent_code;
//
//    return( NO_ERROR );
//}



/*=======================================================================
||																		||
||			RefreshDoctorDrugTable()									||
||																		||
||			Refresh doctor drug shared memory table						||
||																		||
 =======================================================================*/
int	RefreshDoctorDrugTable (int	entry)
{
#if 0

//	EXEC SQL BEGIN DECLARE SECTION;
		static DOCTOR_DRUG_ROW	doctor_drug_row;

		DOCTOR_DRUG_ROW			*doctor_drug_row_ptr;
		DOCTOR_DRUG_ROW			*temp_drug_row_ptr;

		TTreatmentType			TreatmentType;
//	EXEC SQL END DECLARE SECTION;

	long				curr_num;
	long				temp;

	static char			*buffer;

	static short int	first_visit	= MAC_TRUE;
	static long int		alloc_num	= 80000;	/* may be taken from db */

	RAW_DATA_HEADER		raw_data_header;
	RAW_DATA_HEADER		*raw_data_header_ptr;

	TABLE				*dcdr_tablep;
	ISOLATION_VAR;


	/*=======================================================================
	||																		||
	||		GET ROWS OF DOCTOR, LARGO, INSURANCES AS						||
	||	UNION OF DOCTOR_PERCENTS AND (JOIN OF DOCTOR SPECIALITY AND			||
	||	SPECIALITY LARGO PERCENT).											||
	||																		||
	=======================================================================*/
	REMEMBER_ISOLATION;

	TreatmentType = TREATMENT_TYPE_GENERAL;

	// First time routine is called, allocate storage in shared memory.
	if (first_visit == MAC_TRUE)
	{
		alloc_num = 80000;

		buffer = MemAllocReturn (GerrId,
								 sizeof (RAW_DATA_HEADER) + sizeof (DOCTOR_DRUG_ROW) * alloc_num,
								 "memory for doctor_drug table raw data");

		if (buffer == NULL)
		{
			RESTORE_ISOLATION;
			return (MEM_ALLOC_ERR);
		}


		// Declare cursor for select all rows from table doctor_percents.
//		EXEC SQL
//			DECLARE	doc_drug_cur CURSOR FOR
//
//			SELECT	doctor_id,
//					largo_code,
//					basic_price_code,
//					zahav_price_code,
//					kesef_price_code
//
//			INTO	:doctor_drug_row.doctor_id,
//					:doctor_drug_row.drug_code,
//					:doctor_drug_row.info.basic_part,
//					:doctor_drug_row.info.zahav_part,
//					:doctor_drug_row.info.kesef_part
//
//			FROM	doctor_percents
//
//			WHERE	treatment_category = :TreatmentType;


		// Declare cursor for select doctor, drug code, percent codes from tables
		// doctor_speciality, spclty_largo_prcnt joined on speciality_code.
//		EXEC SQL
//			DECLARE	doc_drug_cur_2 CURSOR FOR
//
//			SELECT	doctor_id,
//					largo_code,
//					basic_price_code,
//					zahav_price_code,
//					kesef_price_code
//
//			INTO	:doctor_drug_row.doctor_id,
//					:doctor_drug_row.drug_code,
//					:doctor_drug_row.info.basic_part,
//					:doctor_drug_row.info.zahav_part,
//					:doctor_drug_row.info.kesef_part
//
//			FROM	doctor_speciality,
//					spclty_largo_prcnt
//
//			WHERE	doctor_speciality.speciality_code = spclty_largo_prcnt.speciality_code
//			  AND	enabled_mac	> 0;
	}

	first_visit = MAC_FALS;


	for (restart = MAC_TRUE, tries = 0; tries < SQL_UPDATE_RETRIES && restart == MAC_TRUE; tries++)
	{
		restart = MAC_FALS;

		// Begin work.
//		BREAK_ON_ERR (SQLMD_begin ());

		// Open cursor doc_drug_cur.
		SET_ISOLATION_DIRTY;

//		EXEC SQL
//			OPEN doc_drug_cur;

		Conflict_Test_Cur (restart);

		if (ERR_STATE (SQLERR_error_test ()) || restart == MAC_TRUE)
		{
			RESTORE_ISOLATION;
			return (INF_ERR);
		}


		// Read doctor_percents table into memory.
		for (doctor_drug_row_ptr = (DOCTOR_DRUG_ROW *)(buffer + sizeof(RAW_DATA_HEADER)), curr_num = 0;
			 /* ever */;
			 curr_num++, doctor_drug_row_ptr++)
		{
			// Check for free space and allocate more memory if needed.
			if (curr_num >= alloc_num - 1)
			{
				alloc_num += 50000;

				buffer = MemReallocReturn (GerrId,
										   buffer,
										   sizeof(RAW_DATA_HEADER) + sizeof(DOCTOR_DRUG_ROW) * alloc_num,
										   "memory for doctor_drug table raw data");

				if (buffer == NULL)
				{
					RESTORE_ISOLATION;
					return (MEM_ALLOC_ERR);
				}

				doctor_drug_row_ptr	= (DOCTOR_DRUG_ROW *)
										(  buffer
										 + sizeof(RAW_DATA_HEADER)
										 + (sizeof(DOCTOR_DRUG_ROW) * curr_num));
			}	// Need more space.


			// Fetch row from table.
//			EXEC SQL
//				FETCH doc_drug_cur;

			Conflict_Test (restart);

			// Exit loop on error / end of fetch.
			BREAK_ON_TRUE	(SQLERR_code_cmp	(SQLERR_end_of_fetch));
			BREAK_ON_ERR	(SQLERR_error_test	());

			// Identify shared-memory row as coming from doctor_percents.
			doctor_drug_row.doctor_belong = FROM_DOC_AUTHORIZED;

			// Store data.
			*doctor_drug_row_ptr = doctor_drug_row;
		}	// Loop to read doctor_percents.

		// Close doctor_percents cursor.
//		EXEC SQL
//			CLOSE doc_drug_cur;

		if (restart == MAC_TRUE)	/* transaction failed -- try again */
		{
//			BREAK_ON_ERR (SQLMD_rollback ());
			sleep (ACCESS_CONFLICT_SLEEP_TIME);
			continue;
		}

		BREAK_ON_ERR (SQLERR_error_test ());



		// Open join cursor for Doctor Speciality percents.
//		EXEC SQL
//			OPEN doc_drug_cur_2;

		Conflict_Test_Cur (restart);

		if (ERR_STATE (SQLERR_error_test ()) || restart == MAC_TRUE)
		{
			RESTORE_ISOLATION;
			return (INF_ERR);
		}


		// Select all lines from join cursor and update to codes for least percents or insert new.
		for (/* no initialization */	;
			 /* for ever */				;
			 curr_num++, doctor_drug_row_ptr++)
		{
			// Check for free space and allocate more memory if needed.
			if (curr_num >= alloc_num - 1)
			{
				alloc_num += 50000;
				buffer = MemReallocReturn (GerrId,
										   buffer,
										   sizeof (RAW_DATA_HEADER) + (sizeof (DOCTOR_DRUG_ROW) * alloc_num),
										   "memory for doctor_drug table raw data");

				if (buffer == NULL)
				{
					RESTORE_ISOLATION;
					return (MEM_ALLOC_ERR);
				}

				doctor_drug_row_ptr	= (DOCTOR_DRUG_ROW*)(  buffer
														 + sizeof (RAW_DATA_HEADER)
														 + (sizeof (DOCTOR_DRUG_ROW) * curr_num));
			}


			// Fetch row from join cursor.
//			EXEC SQL
//				FETCH doc_drug_cur_2;

			Conflict_Test (restart);

			// Exit loop on error / end of fetch.
			BREAK_ON_TRUE	(SQLERR_code_cmp	(SQLERR_end_of_fetch));
			BREAK_ON_ERR	(SQLERR_error_test	());

			// Identify shared-memory row as coming from doctor_speciality / spclty_largo_prcnt join.
			doctor_drug_row.doctor_belong = FROM_DOC_SPECIALIST;

			// Store data.
			*doctor_drug_row_ptr = doctor_drug_row;
		}	// Loop to read join cursor.


		// Close join cursor.
//		EXEC SQL
//			CLOSE doc_drug_cur_2;

		if (restart == MAC_TRUE)	// transaction failed -- try again.
		{
//			BREAK_ON_ERR (SQLMD_rollback ());
			sleep (ACCESS_CONFLICT_SLEEP_TIME);
			continue;
		}
		else	// transaction succeeded -- commit.
		{
//			BREAK_ON_ERR (SQLMD_commit ());
		}

	}	// for( restart = ... )


	if (restart == MAC_TRUE)
	{
		GerrLogReturn (GerrId,
					   "Table '%s' may be locked.. failed after '%d' retries",
					   "doctor_speciality or spclty_largo_prcnt or "
					   "doctor_percents",
					   SQL_UPDATE_RETRIES);
		RESTORE_ISOLATION;
		return (INF_ERR);
	}


	// Set up buffer header
	raw_data_header.actual_rows	= curr_num;
	raw_data_header.alloc_rows	= alloc_num;
	raw_data_header_ptr			= (RAW_DATA_HEADER*)buffer;
	*raw_data_header_ptr		= raw_data_header;

	// Sort buffer by error_code, doctor_drug_level.
	qsort (buffer + sizeof(RAW_DATA_HEADER),
		   curr_num,
		   sizeof(DOCTOR_DRUG_ROW),
		   TableTab[entry].row_comp);

	// Replace buffer in doctor_drug table in shared memory.
	RESTORE_ISOLATION;
	RETURN_ON_ERR (OpenTable		(TableTab[entry].table_name, &dcdr_tablep));
	RETURN_ON_ERR (SetRecordSize	(dcdr_tablep, sizeof(RAW_DATA_HEADER) + (sizeof(DOCTOR_DRUG_ROW) * curr_num)));
	RETURN_ON_ERR (AddItem			(dcdr_tablep, buffer));
	RETURN_ON_ERR (CloseTable		(dcdr_tablep));
#endif

	// All done!
	return (MAC_OK);
}


/*=======================================================================
||									||
||			RefreshInteractionTable()			||
||									||
||			Refresh interaction shared memory table		||
||									||
 =======================================================================*/
int	RefreshInteractionTable(
				int	entry
				)
{
#if 0

//    EXEC SQL BEGIN DECLARE SECTION;

    static INTERACTION_ROW	interaction_row;

    INTERACTION_ROW	*interaction_row_ptr,
			*temp_inter_row_ptr;

//    EXEC SQL END DECLARE SECTION;

    int
			curr_num,
//			temp,
			counter,
			redundant;

    static char		*buffer;

    static short int	first_visit = MAC_TRUE;
    static int	alloc_num = 1000;	/* may be taken from db */

    RAW_DATA_HEADER	raw_data_header,
			*raw_data_header_ptr;

    TABLE		*intr_tablep;
	ISOLATION_VAR;


/*
 *  Declare cursor for select
 *  all table interaction
 *  -------------------------
 */
	REMEMBER_ISOLATION;

    if( first_visit == MAC_TRUE )
    {
	alloc_num = 25000;

	buffer =
	  MemAllocReturn(
			 GerrId,
			 sizeof(RAW_DATA_HEADER) +
			 sizeof(INTERACTION_ROW) * alloc_num,
			 "memory for interaction table raw data"
			 );

	if( buffer == NULL )
	{
		RESTORE_ISOLATION;
	    return( MEM_ALLOC_ERR );
	}

//	EXEC SQL
//	  DECLARE	inter_cur CURSOR FOR
//	  SELECT	first_largo_code,
//			second_largo_code,
//			interaction_type
//
//	  INTO		:interaction_row.first_drug_code,
//			:interaction_row.second_drug_code,
//			:interaction_row.interaction_type
//
//	  FROM		drug_interaction;
    }

    first_visit = MAC_FALS;

    for(
	restart = MAC_TRUE,
	tries = 0;
	tries < SQL_UPDATE_RETRIES && restart == MAC_TRUE;
	tries++
	)
    {
	restart = MAC_FALS;

	/*
	 * Begin work
	 * ----------
	 */
//	SQLMD_begin();

	if( state = SQLERR_error_test() )
	{
	    break;
	}

	/*
	 *  Open cursor inter_cur
	 *  ---------------------
	 */
	SET_ISOLATION_DIRTY;

//	EXEC SQL
//	  OPEN  inter_cur;

	Conflict_Test_Cur( restart );

	state = SQLERR_error_test();

	if( state == MAC_OK	&&
	    restart == MAC_FALS )
	{
	    /*
	     *  select all interaction table
	     *  ----------------------------
	     */
	    for(
		interaction_row_ptr	=
		  (INTERACTION_ROW*)(buffer + sizeof(RAW_DATA_HEADER)),
		curr_num = 0;
		/* ever */;
		curr_num++,
		interaction_row_ptr++
		)
	    {

		/*
		 * Check for free space
		 * --------------------
		 */
		if( curr_num	>= alloc_num - 1)
		{
		    alloc_num +=
		      5000;

		    buffer =
		      MemReallocReturn(
				       GerrId,
				       buffer,
				       sizeof(RAW_DATA_HEADER) +
				       sizeof(INTERACTION_ROW) * alloc_num,
				       "memory for interaction table raw data"
				       );

		    if( buffer == NULL )
		    {
				RESTORE_ISOLATION;
				return( MEM_ALLOC_ERR );
		    }

		    interaction_row_ptr	=
		      (INTERACTION_ROW*)(
					 buffer +
					 sizeof(RAW_DATA_HEADER) +
					 sizeof(INTERACTION_ROW) * curr_num
					 );
		}

		/*
		 * Fetch row from table
		 * --------------------
		 */
//		EXEC SQL
//		  FETCH inter_cur;

		Conflict_Test( restart );

		/*
		 * Exit loop on error / end of fetch
		 */
		BREAK_ON_TRUE( SQLERR_code_cmp( SQLERR_end_of_fetch ) );

		BREAK_ON_ERR( SQLERR_error_test() );

		*interaction_row_ptr = interaction_row;

		if(
		   interaction_row_ptr->first_drug_code >
		   interaction_row_ptr->second_drug_code
		   )
		{
		    interaction_row.first_drug_code =	/* first > temp */
		      interaction_row_ptr->first_drug_code;

		    interaction_row_ptr->first_drug_code =/* second > first */
		      interaction_row_ptr->second_drug_code;

		    interaction_row_ptr->second_drug_code =/* temp > second */
		      interaction_row.first_drug_code;
		}

	    } /* for( curr_num .. ) */

	    /*
	     * Close cursor inter_cur
	     * ----------------------
	     */
//	    EXEC SQL
//	      CLOSE inter_cur;

	    SQLERR_error_test();

	} /* if( restart ... ) */

	if( restart == MAC_TRUE )	/* transaction faild -- try again */
	{
//	    SQLMD_rollback();

	    BREAK_ON_ERR( SQLERR_error_test() );

	    sleep( ACCESS_CONFLICT_SLEEP_TIME );
	}
	else				/* transaction succed -- finish   */
	{
//	    SQLMD_commit();

	    SQLERR_error_test();
	}

    } /* for( restart = ... ) */

    if( restart == MAC_TRUE )
    {
		GerrLogReturn(
				  GerrId,
				  "Table '%s' locked.. failed after '%d' retries",
				  "drug_interaction",
				  SQL_UPDATE_RETRIES
				  );

		RESTORE_ISOLATION;
		return( INF_ERR );
    }

    if( state != MAC_OK )
    {
		RESTORE_ISOLATION;
		return( state );
    }

/*
 * Sort buffer by first_drug_code, second_drug_code
 * ------------------------------------------------
 */
    qsort(
	  buffer +
	  sizeof(RAW_DATA_HEADER),
	  curr_num,
	  sizeof(INTERACTION_ROW),
	  IntrCompare
	  );

/*
 * Drop repeating lines in buffer
 * ------------------------------
 */
    for(
	redundant = 0,
	  temp_inter_row_ptr =
	      (INTERACTION_ROW*)(buffer + sizeof(RAW_DATA_HEADER)),
	  interaction_row_ptr =
	      temp_inter_row_ptr + 1,
	  counter = 1;

	counter < curr_num;

	counter++,
	  interaction_row_ptr++
	)
    {
	if(
	   IntrCompare(
		       (const void *)interaction_row_ptr,
		       (const void *)temp_inter_row_ptr
		       )
	   )
	{
	    *++temp_inter_row_ptr =
	      *interaction_row_ptr;

	    continue;
	}

	redundant++;
    }

    curr_num -= redundant;

/*
 * Set buffer header
 * -----------------
 */
    raw_data_header.actual_rows	= curr_num;

    raw_data_header.alloc_rows	= alloc_num;

    raw_data_header_ptr =
      (RAW_DATA_HEADER*)buffer;

    *raw_data_header_ptr = raw_data_header;

/*
 * replace buffer in interaction table in shm
 * ---------------------------------------
 */
	RESTORE_ISOLATION;

    RETURN_ON_ERR(
		  OpenTable(
			    INTR_TABLE,
			    &intr_tablep
			    )
		  );

    RETURN_ON_ERR(
		  SetRecordSize(
				intr_tablep,
				sizeof(RAW_DATA_HEADER) +
				sizeof(INTERACTION_ROW) * curr_num
				)
		  );

    RETURN_ON_ERR(
		  AddItem(
			  intr_tablep,
			  buffer
			  )
		  );

    RETURN_ON_ERR(
		  CloseTable(
			     intr_tablep
			     )
		  );
#endif

/*
 *  Return -- ok
 *  ------------
 */
    return( MAC_OK );

}


/*=======================================================================
||																		||
||			RefreshPrescriptionTable()									||
||																		||
||			Refresh prescription shared memory table					||
||																		||
||          New Variant GAL 14/06/05									||
 =======================================================================*/
int	RefreshPrescriptionTable( int entry )
{

  //EXEC SQL BEGIN DECLARE SECTION;			//Marianna

    static PRESCRIPTION_ROW		prescription_row;
    static MESSAGE_RECID_ROW	msg_recid_row;
    static DELPR_RECID_ROW		delpr_id_row;

    TSqlInd		presc_ind;

	PRESCRIPTION_ROW	*prescription_row_ptr;
	MESSAGE_RECID_ROW	*msg_recid_row_ptr;
	DELPR_RECID_ROW		*delpr_id_row_ptr;

	int	h_nUpperLimit;
  	int	h_nLowLimit;
	int	recid_UpperLimit;
  	int	recid_LowLimit;
	int	delpr_UpperLimit;
  	int	delpr_LowLimit;
	char	h_szHostName[ 256 + 1 ];

  //EXEC SQL END DECLARE SECTION;			//Marianna


    RAW_DATA_HEADER	raw_data_header , *raw_data_header_ptr;

    TABLE		*prsc_tablep;
    TABLE		*recid_tablep;
    TABLE		*delpr_tablep;

	char		buf[ 1024 ];
    int         nMacSys = PHARM_SYS;
	ISOLATION_VAR;


	REMEMBER_ISOLATION;


	if (!gethostname (h_szHostName, MAX_NAME_LEN))
	{
		GerrLogReturn( GerrId , "Host : \"%s\"" , h_szHostName );

		for (restart = MAC_TRUE, tries = 0;
			 tries < SQL_UPDATE_RETRIES && restart == MAC_TRUE;
			 tries++)
		{
			restart = MAC_FALS;

			// Get data from table presc_per_host
			// DonR 01Apr2008: Since we're interested only in servers running the
			// Pharmacy system, add a WHERE criterion for "active <> 0". This approach
			// seems a little more elegant than working with the MAC_SYS environment
			// variable, which is left empty for a dual-purpose server.
			SET_ISOLATION_DIRTY;

			ExecSQL (	&INF_DB, READ_presc_per_host,
						&h_nLowLimit,		&h_nUpperLimit,		&recid_LowLimit,
						&recid_UpperLimit,	&delpr_LowLimit,	&delpr_UpperLimit,
						&h_szHostName,		END_OF_ARG_LIST								);
//			EXEC SQL
//				SELECT	low_limit,
//						upper_limit,
//						msg_low_limit,
//						msg_upr_limit,
//						delpr_low_limit,
//						delpr_upr_limit
//
//				INTO	:h_nLowLimit,
//						:h_nUpperLimit,
//						:recid_LowLimit,
//						:recid_UpperLimit,
//						:delpr_LowLimit,
//						:delpr_UpperLimit
//
//				FROM	presc_per_host
//
//				WHERE	host_name	=  :h_szHostName
//				  AND	active		<> 0;

			Conflict_Test (restart);

// GerrLogMini (GerrId, "Looked up %s in presc_per_host: PrID min/max = %d/%d, SQLCODE = %d.", h_nLowLimit, h_nUpperLimit, SQLCODE);

			// DonR 01Apr2008: Instead of worrying about the MAC_SYS environment
			// variable, just assume that if a presc_per_host row with "active"
			// set non-zero is what defines a Pharmacy system. So if we didn't
			// manage to read an "active" row from presc_per_host, we can
			// safely assume we're working with a non-Pharmacy server.
			if (SQLERR_code_cmp (SQLERR_not_found) == MAC_TRUE)
			{
				nMacSys				=
				h_nLowLimit			=
				h_nUpperLimit		=
				recid_LowLimit		=
				recid_UpperLimit	=
				delpr_LowLimit		=
				delpr_UpperLimit	= 0;
			}
			else
			{
				// Exit loop on error
				BREAK_ON_ERR (SQLERR_error_test ());
			}

			if (restart == MAC_TRUE)	// transaction faild -- try again 
			{
				sleep (ACCESS_CONFLICT_SLEEP_TIME); // 10 seconds ?!?
			}

		} // for( restart = ... ) 


		if ( restart == MAC_TRUE )
		{
			GerrLogReturn( GerrId,
				"Table '%s' locked.. failed after '%d' retries",
				"presc_per_host",
				SQL_UPDATE_RETRIES );

			RESTORE_ISOLATION;
			return INF_ERR;
		}

		GerrLogReturn( GerrId , "LowLimit = %d , UpperLimit =%d, MsgRecId %d -> %d" , 
						h_nLowLimit , h_nUpperLimit, recid_LowLimit, recid_UpperLimit );

		// DonR 01Apr2008: Don't bother checking prescriptions table if this
		// is a Doctors-only server.
		if (nMacSys)
		{
			for( restart = MAC_TRUE , tries = 0;
				tries < SQL_UPDATE_RETRIES && restart == MAC_TRUE;
				tries++)
			{
				restart = MAC_FALS;

//			    // Begin work
//				SQLMD_begin();
//		 
//				if ( state = SQLERR_error_test() )
//			    {
//					break;
//				}
//
   				// Get max presc id in given range
				// DonR 26Jun2005: changed the selection criteria from BETWEEN
				// to LowLimit <= max < UpperLimit. This means that if we set
				// the limit to between 30,000,000 and 40,000,000 the actual
				// maximum value we will look for will be 39,999,999. This
				// avoids a potential major bug, since people may enter criteria
				// into presc_per_host such that the same (round) number is used
				// for the top of one range and the bottom of another.
				ExecSQL (	&INF_DB, READ_max_prescripton_id,
							&prescription_row.prescription_id,
							&h_nLowLimit,
							&h_nUpperLimit,
							END_OF_ARG_LIST							);

				presc_ind = ColumnOutputLengths [1];
//				EXEC SQL
//					SELECT	MAX( prescription_id )
//					INTO	:prescription_row.prescription_id:presc_ind
//					FROM	prescriptions 
//					WHERE	prescription_id >= :h_nLowLimit
//					  AND	prescription_id <  :h_nUpperLimit;
	    
				Conflict_Test( restart );

				// Exit loop on error
				BREAK_ON_ERR( SQLERR_error_test() );

				if ( SQLMD_is_null( presc_ind ) == MAC_TRUE )
				{    // !?!
					 prescription_row.prescription_id = h_nLowLimit;
				}
				else
				{
					 prescription_row.prescription_id ++;
				}


				// DonR 30Jun2005: Check for maximum message rec_id as well.
				ExecSQL (	&INF_DB, READ_max_messages_details_rec_id,
							&msg_recid_row.message_rec_id,
							&recid_LowLimit,
							&recid_UpperLimit,
							END_OF_ARG_LIST									);

				presc_ind = ColumnOutputLengths [1];
//				EXEC SQL
//					SELECT	MAX(rec_id)
//					INTO	:msg_recid_row.message_rec_id:presc_ind
//					FROM	messages_details 
//					WHERE	rec_id >= :recid_LowLimit
//					  AND	rec_id <  :recid_UpperLimit;
	    
				Conflict_Test( restart );

				// Exit loop on error
				BREAK_ON_ERR( SQLERR_error_test() );

				if ( SQLMD_is_null( presc_ind ) == MAC_TRUE )
				{    // !?!
					 msg_recid_row.message_rec_id = recid_LowLimit;
				}
				else
				{
					 msg_recid_row.message_rec_id++;
				}

				// DonR 04Jul2005: Check for maximum presc_delete id as well.
				ExecSQL (	&INF_DB, READ_max_messages_details_rec_id,
							&delpr_id_row.delpr_rec_id,
							&delpr_LowLimit,
							&delpr_UpperLimit,
							END_OF_ARG_LIST									);

				presc_ind = ColumnOutputLengths [1];
//				EXEC SQL
//					SELECT	MAX(deletion_id)
//					INTO	:delpr_id_row.delpr_rec_id:presc_ind
//					FROM	presc_delete 
//					WHERE	deletion_id >= :delpr_LowLimit
//					  AND	deletion_id <  :delpr_UpperLimit;
	    
				Conflict_Test(restart);

				// Exit loop on error
				BREAK_ON_ERR (SQLERR_error_test ());

				if ( SQLMD_is_null( presc_ind ) == MAC_TRUE )
				{    // !?!
					 delpr_id_row.delpr_rec_id = delpr_LowLimit;
				}
				else
				{
					 delpr_id_row.delpr_rec_id++;
				}

				if ( restart == MAC_TRUE )	// transaction faild -- try again 
				{
				     sleep( ACCESS_CONFLICT_SLEEP_TIME );
			    }

		   } // for( restart = ... ) 

		   if ( restart == MAC_TRUE )
		   {
				GerrLogReturn( GerrId,
					"Table '%s' locked.. failed after '%d' retries",
					"prescriptions",
					SQL_UPDATE_RETRIES );

				RESTORE_ISOLATION;
				return INF_ERR;
			}
		}	// This is a Pharmacy (or combination) system.
	
	} // get host name successfully
	else
	{
			GerrLogReturn( GerrId,
					"Function <gethostname> failed , Error #%d \"%s\"",
					errno , strerror( errno ) );
			
			RESTORE_ISOLATION;
			return -1;  // ?!?
	}

    GerrLogReturn( GerrId , "PrescID %d, Rec_id %d." , 
							prescription_row.prescription_id,msg_recid_row.message_rec_id);


	// Set buffer header
    raw_data_header.actual_rows	= 1;
    
    raw_data_header.alloc_rows	= 1;

    raw_data_header_ptr = (RAW_DATA_HEADER *)buf;

    *raw_data_header_ptr = raw_data_header;


	// Set up and store Prescription ID.
    prescription_row_ptr = (PRESCRIPTION_ROW *)(buf + sizeof(RAW_DATA_HEADER));

    *prescription_row_ptr = prescription_row;

    // replace buffer in Prescription ID table in shm
	RESTORE_ISOLATION;

    RETURN_ON_ERR (OpenTable (PRSC_TABLE, &prsc_tablep));

    RETURN_ON_ERR (SetRecordSize (prsc_tablep,
								  sizeof(RAW_DATA_HEADER) + sizeof(PRESCRIPTION_ROW)));

    RETURN_ON_ERR (AddItem (prsc_tablep, buf));

    RETURN_ON_ERR (CloseTable (prsc_tablep));


	// Set up and store Message Rec_ID.
    msg_recid_row_ptr = (MESSAGE_RECID_ROW *)(buf + sizeof(RAW_DATA_HEADER));

    *msg_recid_row_ptr = msg_recid_row;

    // replace buffer in Message Rec_ID table in shm
    RETURN_ON_ERR (OpenTable (MSG_RECID_TABLE, &recid_tablep));

    RETURN_ON_ERR (SetRecordSize (recid_tablep,
								  sizeof(RAW_DATA_HEADER) + sizeof(MESSAGE_RECID_ROW)));

    RETURN_ON_ERR (AddItem (recid_tablep, buf));

    RETURN_ON_ERR (CloseTable (recid_tablep));


	// Set up and store Deleted Prescription ID.
    delpr_id_row_ptr = (DELPR_RECID_ROW *)(buf + sizeof(RAW_DATA_HEADER));

    *delpr_id_row_ptr = delpr_id_row;

    // replace buffer in Deleted Prescription ID table in shm
    RETURN_ON_ERR (OpenTable (DELPR_ID_TABLE, &delpr_tablep));

    RETURN_ON_ERR (SetRecordSize (delpr_tablep,
								  sizeof(RAW_DATA_HEADER) + sizeof(DELPR_RECID_ROW)));

    RETURN_ON_ERR (AddItem (delpr_tablep, buf));

    RETURN_ON_ERR (CloseTable (delpr_tablep));

	//  Return -- ok
    return( MAC_OK );
  
} // RefreshPrescriptionTable



/*=======================================================================
||																		||
||			GetComputerShortName()										||
||																		||
||			Get short name of computer									||
||																		||
 =======================================================================*/
char *GetComputerShortName ()
{
	static char		MyShortName [3];
	static short	Initialized = 0;
	ISOLATION_VAR;

	//EXEC SQL BEGIN DECLARE SECTION;		//Marianna
		char	v_short_name [3];
		char	h_szHostName [256 + 1 ];
	//EXEC SQL END DECLARE SECTION;		//Marianna


	REMEMBER_ISOLATION;

	if (!Initialized)
	{
		strcpy (v_short_name,	"");
		strcpy (MyShortName,	"");

		if (!gethostname (h_szHostName, MAX_NAME_LEN))
		{
			for (restart = MAC_TRUE, tries = 0;
				 tries < SQL_UPDATE_RETRIES && restart == MAC_TRUE;
				 tries++
				)
			{
				SET_ISOLATION_DIRTY;
				restart = MAC_FALS;

				// Get data from table presc_per_host
				ExecSQL (	&INF_DB, READ_GetComputerShortName,
							&v_short_name, &h_szHostName, END_OF_ARG_LIST	);
//				EXEC SQL
//					SELECT	short_name
//					INTO	:v_short_name
//					FROM	presc_per_host 
//					WHERE	host_name = :h_szHostName;

				Conflict_Test (restart);

				// Exit loop on error
				BREAK_ON_ERR (SQLERR_error_test ());

				if (restart == MAC_TRUE)	// transaction failed -- try again 
				{
					sleep (ACCESS_CONFLICT_SLEEP_TIME); // 10 seconds ?!?
				}
				else
				{
					// Read succeeded.
					Initialized = 1;
					strcpy (MyShortName, v_short_name);
				}

			} // for( restart = ... ) 

			if (restart == MAC_TRUE)
			{
				GerrLogReturn (GerrId,
							   "Table '%s' locked.. failed after '%d' retries",
							   "presc_per_host",
							   SQL_UPDATE_RETRIES);
			}

		} // get host name successfully

		else
		{
			GerrLogReturn (GerrId,
						   "Function <gethostname> failed , Error #%d \"%s\"",
						   errno , strerror (errno));
		}

	}	// Not already initialized.


	//  Return -- ok
	RESTORE_ISOLATION;
	return (&MyShortName[0]);

} // GetComputerShortName


/*=======================================================================
||																		||
||			RefreshPrescrWrittenTable()									||
||																		||
||			Initializa prescriptions-written shared memory table		||
||																		||
 =======================================================================*/
int	RefreshPrescrWrittenTable (int entry)
{
#if 0
	RAW_DATA_HEADER				raw_data_header;
	RAW_DATA_HEADER				*raw_data_header_ptr;
    static PRESCR_WRITTEN_ROW	prw_row;
    PRESCR_WRITTEN_ROW			*prw_row_ptr;
	TABLE						*prw_tablep;
	char						buf [1024];


	// Set up Prescriptions Written ID structure so that the first time
	// we need to get an ID, the routine will go to the database to
	// "grab" a batch of ID numbers.
	prw_row.this_id				= 0;
	prw_row.next_id				= 1;	// I.e. next_id > last_reserved_id.
	prw_row.last_reserved_id	= 0;

	// Set buffer header
	raw_data_header.actual_rows	= 1;
	raw_data_header.alloc_rows	= 1;

	// Copy raw_data_header structure into buffer.
	raw_data_header_ptr		= (RAW_DATA_HEADER *)buf;
	*raw_data_header_ptr	= raw_data_header;

	// Copy prescriptions-written ID structure to buffer after raw_data_header.
	prw_row_ptr				= (PRESCR_WRITTEN_ROW *) (buf + sizeof(RAW_DATA_HEADER));
	*prw_row_ptr			= prw_row;

	// Move buffer to shared memory.
	RETURN_ON_ERR (OpenTable		(PRW_TABLE, &prw_tablep));

	RETURN_ON_ERR (SetRecordSize	(prw_tablep, sizeof(RAW_DATA_HEADER) + sizeof(PRESCR_WRITTEN_ROW)));

	RETURN_ON_ERR (AddItem			(prw_tablep, buf));

	RETURN_ON_ERR (CloseTable		(prw_tablep));
#endif

	return (MAC_OK);
}


/*=======================================================================
||																		||
||			RefreshDrugExtensionTable()		STUB VERSION				||
||																		||
||			Refresh drug extension shared memory table					||
||																		||
|| NOTE: This version does nothing, and exists only for compatibility.	||
|| Drug_extension data is no longer retrieved from shared memory.		||
||																		||
 =======================================================================*/
int	RefreshDrugExtensionTable (int	entry)
{
	return( MAC_OK );
}



/*=======================================================================
||																		||
||			GET_ERROR_SEVERITY()										||
||																		||
||		Get error severity by error code								||
||																		||
 =======================================================================*/
TErrorSevirity GET_ERROR_SEVERITY (TErrorCode	error_code)

{
	TABLE			*sevr_tablep;
	SEVERITY_ROW	severity_row;
	int				state;
	int				actitems_state;
	int				severity_level = DEFAULT_SEVERITY;
	int				severity_found = 0;
	ISOLATION_VAR;

	//EXEC SQL BEGIN DECLARE SECTION;		//Marianna
		short	v_DbSeverityLevel;
		short	v_DbErrorCode;
	//EXEC SQL END DECLARE SECTION;		//Marianna


	REMEMBER_ISOLATION;

	// Dummy loop to avoid GOTO's.
	do
	{
		state =	OpenTable (SEVR_TABLE, &sevr_tablep);

		if (state < MAC_OK)
			break;

		severity_row.error_code		= error_code;
		severity_row.severity_level	= -1;

		actitems_state = ActItems (sevr_tablep,
								   1,
								   GetSeverityByCode,
								   (OPER_ARGS)&severity_row);

		state = CloseTable (sevr_tablep);

		if ((state							<  MAC_OK)			||
			(actitems_state					== NO_MORE_ROWS)	||
			(actitems_state					<  MAC_OK)			||
			(severity_row.severity_level	== -1))
		{
			break;
		}

		// If we got here, we found something in the shared-memory table.
		severity_found = 1;
		severity_level = severity_row.severity_level;
	}
	while (0);


	// DonR 05Jan2004: As a fallback, try looking at the "real" database table if
	// we didn't find anything in the shared-memory table. This means that we don't
	// have to bring the whole system down every time we add a new error code.
	if (!severity_found)
	{
		v_DbErrorCode = error_code;
		SET_ISOLATION_DIRTY;

		ExecSQL (	&INF_DB, READ_GET_ERROR_SEVERITY,
					&v_DbSeverityLevel, &v_DbErrorCode, END_OF_ARG_LIST	);
//		EXEC SQL
//			SELECT	severity_level
//			INTO	:v_DbSeverityLevel
//			FROM	pc_error_message
//			WHERE	error_code = :v_DbErrorCode;

		if (SQLERR_code_cmp (SQLERR_ok) == MAC_TRUE)
		{
			severity_found = 1;
			severity_level = v_DbSeverityLevel;

			//GerrLogReturn (GerrId,
			//			   "Got severity level %d from DB for error code %d.",
			//			   severity_level,
			//			   error_code);
		}
		else
		{
			if (SQLERR_code_cmp (SQLERR_not_found) != MAC_TRUE)
			{
				SQLERR_error_test ();
			}
		}
	}


	if (!severity_found)
	{
		GerrLogReturn (GerrId,
					   "No severity level for error code %d in severity table.",
					   error_code);
	}


	RESTORE_ISOLATION;

	return (severity_level);
}


/*=======================================================================
||																		||
||			GET_PRESCRIPTION_ID()										||
||																		||
||		Get free prescription id from shared memory						||
||																		||
 =======================================================================*/
TErrorCode
	GET_PRESCRIPTION_ID(
			    TRecipeIdentifier	*recipe_identifier
			    )
{
    TABLE		*prsc_tablep;

    PRESCRIPTION_ROW	prescription_row;

    int			state,
			actitems_state;
    
    state =
      OpenTable(
		PRSC_TABLE,
		&prsc_tablep
		);

    if( state < MAC_OK )
    {
	return( ERR_SHARED_MEM_ERROR );
    }

    actitems_state =
      ActItems(
	       prsc_tablep,
	       1,
	       GetPrescriptionId,
	       (OPER_ARGS)&prescription_row
	       );

    state =
      CloseTable(
		 prsc_tablep
		 );

    if( state < MAC_OK )
    {
	return( ERR_SHARED_MEM_ERROR );
    }

    *recipe_identifier =
      prescription_row.prescription_id;
	
//	printf("New prescription_id = %d\n" , prescription_row.prescription_id );fflush(stdout);
    
	return( NO_ERROR );
  
}



/*=======================================================================
||																		||
||			GET_MSG_REC_ID()											||
||																		||
||		Get free message rec_id from shared memory						||
||																		||
 =======================================================================*/
TErrorCode GET_MSG_REC_ID (int *message_recid)
{
	TABLE				*recid_tablep;
	MESSAGE_RECID_ROW	msg_recid_row;
	int					state;
	int					actitems_state;


	state = OpenTable (MSG_RECID_TABLE, &recid_tablep);

	if (state < MAC_OK)
	{
		return (ERR_SHARED_MEM_ERROR);
	}

	actitems_state = ActItems (recid_tablep,
							   1,
							   GetMessageRecId,
							   (OPER_ARGS)&msg_recid_row);

	state =	CloseTable (recid_tablep);

	if (state < MAC_OK)
	{
		return (ERR_SHARED_MEM_ERROR);
	}

	*message_recid = msg_recid_row.message_rec_id;

//	printf("New message_recid = %d\n" , msg_recid_row.message_rec_id);fflush(stdout);

	return (NO_ERROR);

}



/*=======================================================================
||																		||
||			GET_DELPR_ID()												||
||																		||
||		Get free delete prescription id from shared memory				||
||																		||
 =======================================================================*/
TErrorCode GET_DELPR_ID (TRecipeIdentifier *delpr_id)
{
	TABLE				*recid_tablep;
	DELPR_RECID_ROW		recid_row;
	int					state;
	int					actitems_state;


	state = OpenTable (DELPR_ID_TABLE, &recid_tablep);

	if (state < MAC_OK)
	{
		return (ERR_SHARED_MEM_ERROR);
	}

	actitems_state = ActItems (recid_tablep,
							   1,
							   GetDelprId,
							   (OPER_ARGS)&recid_row);

	state =	CloseTable (recid_tablep);

	if (state < MAC_OK)
	{
		return (ERR_SHARED_MEM_ERROR);
	}

	*delpr_id = recid_row.delpr_rec_id;

//	GerrLogToFileName ("1007_log",
//	GerrId,
//	"GET_DELPR_ID: Got new ID %d.", *delpr_id);

	return (NO_ERROR);

}


#if 0
/*=======================================================================
||																		||
||			GET_PRW_ID()												||
||																		||
||		Get free prescription-written id from shared memory				||
||																		||
 =======================================================================*/
#define PRW_ID_BATCH_SIZE	2000

TErrorCode GET_PRW_ID (TRecipeIdentifier	*prw_identifier)
{
	TABLE						*prw_tablep;
	static PRESCR_WRITTEN_ROW	prw_row;
    PRESCR_WRITTEN_ROW			*prw_row_ptr;
	RAW_DATA_HEADER				raw_data_header;
	RAW_DATA_HEADER				*raw_data_header_ptr;
	int							state;
	int							actitems_state;
	int							tries;
	int							succeeded;
	int							return_value = NO_ERROR;
	char						buf [1024];
	ISOLATION_VAR;

	//EXEC SQL BEGIN DECLARE SECTION;		//Marianna
		long			v_prw_nextid;
		long			v_prw_testid;
		long			v_date;
		long			v_time;
	//EXEC SQL END   DECLARE SECTION;		//Marianna


	REMEMBER_ISOLATION;

	for (tries = 0; tries < 3; tries++)
	{
		succeeded = MAC_FALSE;

		state = OpenTable (PRW_TABLE, &prw_tablep);
		if (state != MAC_OK)
		{
			return_value = ERR_SHARED_MEM_ERROR;
			continue;
		}

		actitems_state = ActItems (prw_tablep, 1, GetPrescrWrittenId, (OPER_ARGS)&prw_row);

		state = CloseTable (prw_tablep);
		if (state != MAC_OK)
		{
			return_value = ERR_SHARED_MEM_ERROR;
			continue;
		}

		if (prw_row.error_code == 0)
		{
			succeeded = MAC_TRUE;
			return_value = NO_ERROR;
			break;
		}


		if (prw_row.error_code == -1)	// No more available reserved ID's - get more from database.
		{
			do
			{
				SET_ISOLATION_REPEATABLE;

				EXEC SQL LOCK TABLE prw_nextid IN EXCLUSIVE MODE;

				if (SQLERR_error_test ())
				{
					GerrLogReturn (GerrId,"Error %d locking PRW_nextid.",(int)SQLCODE);
					return_value = ERR_DATABASE_ERROR;
					break;
				}


				EXEC SQL
					SELECT	prw_nextid
					INTO	:v_prw_nextid
					FROM	prw_nextid;

				if (SQLERR_error_test ())
				{
					GerrLogReturn (GerrId,"Error %d reading PRW_nextid.",(int)SQLCODE);
					return_value = ERR_DATABASE_ERROR;
					break;
				}


				// Set up Prescriptions Written ID structure based upon the value
				// of prw_nextid we retrieved from the database.
				v_prw_testid				=  v_prw_nextid;		// To test batch with dummy add.
				prw_row.this_id				=  v_prw_nextid + 1;	// Allow room for test add.
				prw_row.next_id				=  v_prw_nextid + 2;
				prw_row.last_reserved_id	=  v_prw_nextid + PRW_ID_BATCH_SIZE - 1;

				v_prw_nextid				+= PRW_ID_BATCH_SIZE;

				EXEC SQL
					UPDATE	prw_nextid
					SET		prw_nextid = :v_prw_nextid;

				if (SQLERR_error_test ())
				{
					GerrLogReturn (GerrId, "Error %d updating PRW_nextid with value %d.",
									(int)SQLCODE, (int)v_prw_nextid);
					return_value = ERR_DATABASE_ERROR;
					break;
				}


				// DonR 23Dec2004: Just to make sure everything's OK before we
				// start using the new batch of PRW ID's, try adding a dummy
				// row with the first ID number retrieved. If the add doesn't
				// succeed, somehow we got a duplicate batch - so just try again.
				// Note that this test is done *after* updating the prw_nextid
				// table, so if there's a problem writing to prescr_wr_new we'll
				// advance to the next batch of PRW ID's.
				v_date = GetDate ();
				v_time = GetTime ();

				EXEC SQL
					INSERT INTO	prescr_wr_new	(
													prw_id,
													treatmentcontr,
													origin_code,
													prescr_added_date,
													prescr_added_time
												)

					VALUES						(
													:v_prw_testid,
													:v_prw_testid,
													9999,
													:v_date,
													:v_time
												);

				if (SQLERR_error_test ())
				{
					GerrLogReturn (GerrId,"Error %d writing test PRW_ID %d.",(int)SQLCODE,(int)v_prw_testid);
					return_value = ERR_DATABASE_ERROR;
					break;
				}


				state = OpenTable (PRW_TABLE, &prw_tablep);
				if (state < MAC_OK)
				{
					return_value = ERR_SHARED_MEM_ERROR;
					break;
				}

				actitems_state = ActItems (prw_tablep, 1, PutPrescrWrittenId, (OPER_ARGS)&prw_row);

				state = CloseTable (prw_tablep);
				if (state < MAC_OK)
				{
					return_value = ERR_SHARED_MEM_ERROR;
					break;
				}

				succeeded		= MAC_TRUE;
				return_value	= NO_ERROR;
			}
			while (0);

			if (succeeded)
			{
				break;	// No need for more iterations.
			}
			else
			{
				// DonR 13Mar2005: Clear error flags - otherwise neither this process
				// nor any other process on this machine will attempt a recovery!
				OpenTable (PRW_TABLE, &prw_tablep);
				ActItems (prw_tablep, 1, ClearPrescrWrittenIdFlag, (OPER_ARGS)&prw_row);
				CloseTable (prw_tablep);
//				GerrLogReturn (GerrId,"Cleared PRW-ID error flag.");
			}

		}	// Need to get new ID batch from database.

		// If the error code wasn't -1, it should be -3 - meaning that another process
		// is getting the new batch. In this case, just pause and try again.
		sleep (ACCESS_CONFLICT_SLEEP_TIME);

	}	// End of retry loop.

	if ((!succeeded) || (return_value != NO_ERROR))
	{
		GerrLogReturn (GerrId,"GET_PRW_ID failed: Succeeded = %d, return_value %d.",
						  (int)succeeded, (int)return_value);
	}


	RESTORE_ISOLATION;

	if (!succeeded)
	{
		if (return_value == NO_ERROR)
			return_value = ERR_DATABASE_ERROR;

		return (return_value);
	}

	*prw_identifier = prw_row.this_id;

	if (prw_row.error_code != MAC_OK)
		return (ERR_SHARED_MEM_ERROR);
	else
		return (NO_ERROR);
}


/*=======================================================================
||																		||
||			PRW_ID_IN_USE()												||
||																		||
||		See if prescription-written id from shared memory is really		||
||		available, and if not, get a new batch of ID's.
||																		||
 =======================================================================*/
TErrorCode PRW_ID_IN_USE (TRecipeIdentifier	prw_identifier_in)
{
	TErrorCode			return_value = 0;
	TABLE				*prw_tablep;
	PRESCR_WRITTEN_ROW	prw_row;
	ISOLATION_VAR;

	//EXEC SQL BEGIN DECLARE SECTION;		//Marianna
		long			v_prw_testid;
		int				v_count;
	//EXEC SQL END   DECLARE SECTION;		//Marianna


	REMEMBER_ISOLATION;

	// See if a row already exists for the PRW ID to be tested.
	v_prw_testid	= prw_identifier_in;
	v_count			= 0;

	SET_ISOLATION_COMMITTED;

	EXEC SQL
		SELECT	COUNT(*)
		INTO	:v_count
		FROM	prescr_wr_new
		WHERE	prw_id = :v_prw_testid;

	if (v_count > 0)
	{
		return_value = 1;

		// Set up Prescriptions Written ID structure so that when
		// we get an ID with GET_PRW_ID(), the routine will go to
		// the database to "grab" a batch of ID numbers.
		prw_row.this_id				= 0;
		prw_row.next_id				= 1;	// I.e. next_id > last_reserved_id.
		prw_row.last_reserved_id	= 0;

		OpenTable (PRW_TABLE, &prw_tablep);
		ActItems (prw_tablep, 1, PutPrescrWrittenId, (OPER_ARGS)&prw_row);
		CloseTable (prw_tablep);
	}


	RESTORE_ISOLATION;

	return (return_value);
}
#endif



/*=======================================================================
||									||
||				DocDruComp()				||
||									||
 =======================================================================*/
int	DocDruComp(
		   const void	*data1,
		   const void	*data2
		   )
{
    DOCTOR_DRUG_ROW
	*row1,
	*row2;

    row1 =
	(DOCTOR_DRUG_ROW*)data1;

    row2 =
	(DOCTOR_DRUG_ROW*)data2;

/*
 * Compare by drug code
 */
  
    if( row1->drug_code > row2->drug_code )
    {
	return( 1 );
    }
    if( row1->drug_code < row2->drug_code )
    {
	return( -1 );
    }

/*
 * Compare by doctor id
 */
    if( row1->doctor_id > row2->doctor_id )
    {
	return( 1 );
    }
    if( row1->doctor_id < row2->doctor_id )
    {
	return( -1 );
    }

/*
 * Compare by doctor belong
 */
    if( row1->doctor_belong > row2->doctor_belong )
    {
	return( 1 );
    }
    if( row1->doctor_belong < row2->doctor_belong )
    {
	return( -1 );
    }

    return( 0 );
}

/*=======================================================================
||									||
||				date_to_tm()				||
||									||
|| Break date into structure tm.					||
||									||
 =======================================================================*/
void    date_to_tm( int inp,  struct tm *TM )
{
    memset( (char*)TM, 0, sizeof(struct tm) );
    TM->tm_mday  =   inp % 100;
    TM->tm_mon   = ( inp / 100 ) % 100 - 1;
    TM->tm_year  = ( inp / 10000 ) - 1900;
}
/*=======================================================================
||									||
||				time_to_tm()				||
||									||
|| Break time into structure tm.					||
||									||
 =======================================================================*/
void    time_to_tm( int inp,  struct tm *TM )
{
    TM->tm_sec  =   inp % 100;
    TM->tm_min  = ( inp / 100 ) % 100;
    TM->tm_hour = inp / 10000;
}
/*=======================================================================
||									||
||				datetime_to_unixtime()			||
||									||
|| Converts db date & time in format 'YYYYMMDD' and 'HHMMSS		||
|| to UNIX system time							||
||									||
 =======================================================================*/
time_t    datetime_to_unixtime( int db_date, int db_time )
{
    struct tm dbt;

    date_to_tm( db_date, &dbt );
    time_to_tm( db_time, &dbt );

    return ( mktime(&dbt) );
}
/*=======================================================================
||									||
||				unixtime_to_datetime()			||
||									||
|| Converts db date & time in format 'YYYYMMDD' and 'HHMMSS		||
|| to UNIX system time							||
||									||
 =======================================================================*/
#ifndef MMdxTpmConnectPassword // if global_1.h included
typedef struct
{
	int	DbDate;
	int	DbTime;
} TDbDateTime;
#endif

TDbDateTime	unixtime_to_datetime( time_t *dt )
{
	TDbDateTime DateTime;

	DateTime.DbDate = tm_to_date( localtime(dt) );
	DateTime.DbTime = tm_to_time( localtime(dt) );

	return( DateTime );
}



// DonR 20Jul2016: New function to retrieve the current database timestamp
// in HH:MM:SS (string) format. This will be used by logging functions to
// provide consistent timestamps across different Linux servers - which
// is useful in evaluating timeliness of queued table updates to things
// like prescription status.
char * GetDbTimestamp ()
{
	time_t	err_time;
	int		PrevSQLCODE;
	int		PrevNumRows;
//	char	*PrevStatementText;

	//EXEC SQL BEGIN DECLARE SECTION;		//Marianna
		char DB_TS_buffer		[ 8 + 1];
		static char TS_buffer	[50 + 1];
	//EXEC SQL END   DECLARE SECTION;		//Marianna


	// Save previous SQL error status.
	PrevSQLCODE			= SQLCODE;
	PrevNumRows			= sqlca.sqlerrd[2];
//	PrevStatementText	= ODBC_LastSQLStatementText;

	// DonR 15Jan2020: Don't bother trying to get the DB timestamp if
	// we already know we're not connected to the database.
	// DonR 13Feb2020: Added global variable ODBC_AvoidRecursion. If
	// this is set TRUE, the calling routine is ODBC_Exec - and we don't
	// want to execute READ_GetCurrentDatabaseTime, since we're already
	// in the middle of some kind of database operation.
	if ((INF_DB.Connected > 0) && (!ODBC_AvoidRecursion))
//	if (0)
	{
		// DonR 26Jan2020: It would appear that the ODBC driver (at least for Informix)
		// doesn't like static variables - maybe it thinks they're supposed to be constants -
		// and the program crashes unceremoniously at this call. It seems to work fine if I
		// pass in the *non*-static buffer, so it would appear that that's the best fix.
		ExecSQL (	&INF_DB, READ_GetCurrentDatabaseTime, &DB_TS_buffer, END_OF_ARG_LIST	);
		strcpy (TS_buffer, DB_TS_buffer);
	}
	else
	{
		SQLCODE = -1;
	}
//	EXEC SQL
//		SELECT	CURRENT HOUR TO SECOND
//		INTO	:TS_buffer
//		FROM	sysparams;

	// The SQL should never fail - but if it does, put the "normal" HHMMSS timestamp into the buffer.
	if (SQLCODE != 0)
	{
		time	(&err_time);
		sprintf	(TS_buffer, ctime(&err_time));
		TS_buffer [strlen(TS_buffer) - 6] = (char)0;

		// DonR 22Jun2020: Get rid of the redundant "Day MMM DD " at the beginning of ctime() output.
		strcpy (DB_TS_buffer, &TS_buffer[11]);
		strcpy (TS_buffer, DB_TS_buffer);
	}
//	else
//	{
//		TS_buffer [8] = (char)0;
//	}

	// Restore previous SQL error status.
	sqlca.sqlcode				= SQLCODE	= ODBC_SQLCODE	= PrevSQLCODE;
	sqlca.sqlerrd[2]										= PrevNumRows;
//	ODBC_LastSQLStatementText								= PrevStatementText;

	return (char *)TS_buffer;
}




// DonR 11Jun2009 begin.
/*=======================================================================
||																		||
||		           strip_spaces()										||
||																		||
||	Strip trailing spaces (but leave string at least					||
||	one character long).												||
||																		||
 =======================================================================*/
void  strip_spaces (unsigned char * source)
{
	int		L;
	int		i;
	int		n;
	int		d;

	// First, strip trailing spaces.
	L = strlen((char *)source);

	// Sanity check: if string is less than 2 characters or more than 1000 characters, do nothing.
	if ((L < 2) || (L > 1000))
		return;

	while ((L > 1) && (source[L - 1] == ' '))
	{
		L--;
	}
	source[L] = (char)0;

	// If we're down to one character, we're done.
	if (L < 2)
		return;

	// Find first non-space character. If we get here, the string must have
	// something other than spaces in it.
	n = 0;
	while (source[n] == ' ')
		n++;

	// Copy "real" text to beginning of buffer.
	for (i = 0; i <= (L - n); i++)
		source[i] = source[i + n];

}
// DonR 11Jun2009 end.



/*=======================================================================
||																		||
||		           strip_trailing_spaces()								||
||																		||
||	Strip trailing (but not leading) spaces.							||
||  Will strip string down to length zero!								||
||																		||
 =======================================================================*/
void  strip_trailing_spaces (unsigned char * source)
{
	int		L;


	// First, strip trailing spaces.
	L = strlen((char *)source);

	// Sanity check: if string is less than 2 characters or more than 2000 characters, do nothing.
	if ((L < 1) || (L > 2000))
		return;

	while ((L > 0) && (source[L - 1] == ' '))
	{
		L--;
	}
	source[L] = (char)0;

	return;
}


/*=======================================================================
||																		||
||		           switch_to_win_heb()									||
||																		||
 =======================================================================*/
void  switch_to_win_heb (unsigned char * source)
{
	register int	    pos, to_pos, to_num_pos, sign_count;
	static char    *signs = ":,.+-/$";

	for (pos = 0; source[pos]; pos++)
	{
		// When non hebrew continue
		if (!ishebrew(source[pos]))
		{
			continue;
		}

		// If we get here, source[pos] is a Hebrew character.
		// Find length of non english text & fix parentheses.
		for (to_pos = pos + 1;
			 (source[to_pos] != 0) && (!isenglish(source[to_pos])) && (strchr("\n\f", source[to_pos]) == NULL);
			 to_pos++)
		{
			fix_char (source + to_pos);	// Switch open-paren to close-paren and vice versa.
		}
		// At this point we've fixed parentheses inside Hebrew/neutral text. To_pos is set
		// to the first character other than Hebrew/neutral text - either the end of the
		// buffer, the first English letter, or a newline/'/f'.

		// Back up to the last character that isn't a number or a Hebrew letter.
		for (;
			 source[to_pos-1] < 48 ||
			 (source[to_pos-1] > 57  && source[to_pos-1] < 128 ) ||
			 (source[to_pos-1] > 154 && source[to_pos-1] < 224 ) ||
			 source[to_pos-1] > 250;
			 to_pos--)
		{
			// No action here - just re-positioning.
		}
		// At this point, the character at to_pos is the last digit or Hebrew letter.

		// Convert (reverse) hebrew text.
		buf_convert (source + pos, to_pos - pos);

		// Convert numbers inside hebrew text
		for (; pos < to_pos ; pos++)
		{
			if (source[pos] < 48 || source[pos] > 57)
				continue;

			// Find number length
			for (to_num_pos = pos + 1, sign_count = 0; to_num_pos < to_pos; to_num_pos++)
			{
				// Digits - continue
				if (source[to_num_pos] > 47 && source[to_num_pos] < 58)
				{
					sign_count = 0;
					continue;
				}

				// Signs - continue only if one sign after digits
				if (strchr (signs, source[to_num_pos]) != NULL && !sign_count++)
				{
					continue;
				}

				break;
			}			/* end of loop on nums inside hebrew txt*/

			// Convert (reverse) number inside hebrew text
			buf_convert (&source[pos], to_num_pos - pos);
			pos = to_num_pos - 1;
			continue;
		}			/* end of loop on hebrew text		*/

		pos = to_pos - 1;
		continue;
	}
}


/*=======================================================================
||																		||
||		           switch_to_win_heb_new()								||
||																		||
 =======================================================================*/
#define UNDETERMINED	0
#define ENGLISH			1
#define HEBREW			2
#define NUMBER			3

void  switch_to_win_heb_new (unsigned char * source)
{
	// These variables are not in use yet.
	int				sign_count;
	static char		*signs		= ":,.+-/$";
	static char		*neutrals	= " -_";

	int				L;
	register int	P;
	unsigned char *	start_segment = source;
	unsigned char *	end_segment;
	int				segment_length;
	int				state			= UNDETERMINED;
	int				prev_state		= UNDETERMINED;
	int				hebrew_seen		= 0;
	int				neutral_chars	= 0;
	int				extend_back		= 0;
	int				Reverse_N_Chars	= 0;
	int				OpenParens		= 0;
	int				OpenBrackets	= 0;
	int				OpenBraces		= 0;


	// Strip trailing spaces before reversal (but leave string at least one character long).
	L = strlen((char *)source);
	while ((L > 1) && (source[L - 1] == ' '))
	{
		L--;
	}
	source[L] = (char)0;

	// Reverse the whole buffer.
	buf_convert (source, L);

	// Strip trailing spaces after reversal (but leave string at least one character long).
	while ((L > 1) && (source[L - 1] == ' '))
	{
		L--;
	}
	source[L] = (char)0;


	// Reverse English and numeric text.
	for (P = Reverse_N_Chars = 0, start_segment = source; P <= L; P++)
	{
		// AS/400 displays strictly left-to-right, so we handle parens, brackets,
		// and braces without regard to language. Note that we're not even going
		// to try to handle nested parentheses (or brackets or braces)!
		if ((source[P] == '(') || (source[P] == ')'))
		{
			if (OpenParens)
			{
				source[P]	= ')';
				OpenParens	= 0;
			}
			else
			{
				source[P]	= '(';
				OpenParens	= 1;
			}
		}

		if ((source[P] == '[') || (source[P] == ']'))
		{
			if (OpenBrackets)
			{
				source[P]	= ']';
				OpenBrackets	= 0;
			}
			else
			{
				source[P]	= '[';
				OpenBrackets	= 1;
			}
		}

		if ((source[P] == '{') || (source[P] == '}'))
		{
			if (OpenBraces)
			{
				source[P]	= '}';
				OpenBraces	= 0;
			}
			else
			{
				source[P]	= '{';
				OpenBraces	= 1;
			}
		}


		if ((!isenglish (source[P])) && (!isnumber (source[P])))
		{
			if (Reverse_N_Chars > 0)
			{
				// If two English words are separated by a space, hyphen, or underscore, treat
				// them as a single block of English text. Any other divider (including
				// periods, commas, or whatever) will result in individual words spelled
				// correctly, but *possibly* not in the order intended. 
				if ((isenglish (source[P + 1])) && (strchr (neutrals, source[P]) != NULL))

				{
					Reverse_N_Chars++;
				}
				else
				{
					buf_convert (start_segment, Reverse_N_Chars);
					Reverse_N_Chars = 0;
				}
			}
		}
		else
		{
			if (Reverse_N_Chars == 0)
			{
				start_segment = source + P;
			}

			Reverse_N_Chars++;
		}
	}


#if 0
	for (P = 0; P < L; P++)
	{
		// Determine what type of text the current character is.
		if ((ishebrew	(source[P])) ||
			((source[P] == ' ') && (ishebrew(source[P + 1])) && hebrew_seen))
		{
			state = HEBREW;
			hebrew_seen = 1;
		}

		if ((isenglish	(source[P])) && ((prev_state == HEBREW) || (prev_state == UNDETERMINED)))
		{
			state = ENGLISH;
			extend_back = neutral_chars;
		}

		if ((isnumber	(source[P])) && ((prev_state == HEBREW) || (prev_state == UNDETERMINED)))
		{
			state = NUMBER;
		}


		// Reverse parentheses (and similar characters) in Hebrew.
		if ((source[P] == ')') && (state == HEBREW) && (ishebrew (source[P + 1])))
			source[P] = '(';

		else
		if ((source[P] == '(') && (state == HEBREW) && (ishebrew (source[P - 1])))
			source[P] = ')';


		if ((state != prev_state) || (P == (L - 1)))	// Language change or last segment in buffer.
		{
			if ((prev_state == ENGLISH) || (prev_state == NUMBER))
			{
				// Reverse English or numeric text.
				end_segment = source + P;
				if (P == (L - 1))
					end_segment++;				// Include final character in final segment.

				start_segment -= extend_back;	// Include preceding punctuation marks.
				segment_length = end_segment - start_segment;
				buf_convert (start_segment, segment_length);
				extend_back = 0;
			}

			prev_state = state;
			start_segment = source + P;
		}

		if ((ishebrew (source[P])) || (isenglish (source[P])) || (isnumber (source[P])))
			neutral_chars = 0;
		else
			neutral_chars++;

	}	// Loop through buffer.
#endif
}



// DonR 17Jul2005: DB Isolation Level functions.

// Note that this function does not test the database error return.
int _set_db_isolation (short new_isolation_mode_in)
{
	int	err_rtn = NO_ERROR;

	// DonR 12Mar2020: For now at least, there is no obvious way in ODBC to change the
	// isolation level once we're connected to the database. If we eventually figure
	// out a way to do it, re-enable this code block but change the embedded-SQL commands
	// to whatever ODBC call(s) we need to use instead.

			//Marianna
//	if (!Connect_ODBC_Only)
//	{
//		// If the requested isolation level is the same as the current level,
//		// there's no need to do anything - just return an OK status.
//		if (new_isolation_mode_in != _current_db_isolation_)
//		{
//			switch (new_isolation_mode_in)
//			{
//				case _DIRTY_READ_:		EXEC SQL SET ISOLATION TO DIRTY READ;
//										break;
//
//				case _COMMITTED_READ_:	EXEC SQL SET ISOLATION TO COMMITTED READ;
//										break;
//
//				case _CURSOR_STABILITY:	EXEC SQL SET ISOLATION TO CURSOR STABILITY;
//										break;
//
//				case _REPEATABLE_READ_:	EXEC SQL SET ISOLATION TO REPEATABLE READ;
//										break;
//
//				default:				err_rtn = -1;	// Unrecognized mode requested.
//										break;
//			}
//
//			if (err_rtn == NO_ERROR)
//				_current_db_isolation_ = new_isolation_mode_in;
//		}	// Isolation is changing.
//	}

	return (err_rtn);
}



int _get_db_isolation (short *current_isolation_mode_out)
{
	int	err_rtn = NO_ERROR;

	if (current_isolation_mode_out != NULL)
		*current_isolation_mode_out = _current_db_isolation_;
	else
		err_rtn = -1;	// NULL pointer passed - error!

	return (err_rtn);
}
// DonR 17Jul2005: DB Isolation Level stuff end.
