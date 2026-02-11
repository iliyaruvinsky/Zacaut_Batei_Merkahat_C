/*=======================================================================
||																		||
||				Gensql.c												||
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
#include "GenSql.h"					//Marianna

extern int TikrotProductionMode;

#define GEN_SQL

#include  "PharmacyErrors.h"				//Marianna
#include "MsgHndlr.h"						//Marianna
#include <MacODBC.h>

#define  SZENV_SYSTEM   "MAC_SYS"   // GAL 23/06/05

#ifdef _LINUX_
	#define	MAX_NAME_LEN	256
#else
	#define	MAX_NAME_LEN	MAXHOSTNAMELEN
	/* MAXHOSTNAMELEN = 256 in <sys/socket.h> GAL 15/06/05 */
#endif


// DonR 03Mar2021: Folding the error codes for SQLERR_deadlocked into
// SQLERR_access_conflict, so there's no longer any reason to check
// for both. (I'll keep SQLERR_deadlocked available in case any routine
// needs to know more specifically what went wrong.)
#define Conflict_Test(r)									\
if (SQLERR_code_cmp (SQLERR_access_conflict) == MAC_TRUE)	\
{															\
    SQLERR_error_test ();									\
    r = MAC_TRUE;											\
    break;													\
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
//  { sizeof(PHARMACY_ROW), sizeof(PHARMACY_ROW),	NOT_SORTED,	RefreshPharmacyTable,
//    NULL,		PHRM_TABLE,		"Pharmacy"	},
//  { sizeof(SEVERITY_ROW), sizeof(SEVERITY_ROW),	NOT_SORTED,	RefreshSeverityTable,
//    NULL,		SEVR_TABLE,		"Pc_error_message"	},
//  { sizeof(PERCENT_ROW), sizeof(PERCENT_ROW),	NOT_SORTED,	RefreshPercentTable,
//    NULL,		PCNT_TABLE,		"Member_price"	},
//  { sizeof(DOCTOR_DRUG_ROW), sizeof(DOCTOR_DRUG_ROW),	NOT_SORTED,	RefreshDoctorDrugTable,
//    DocDruComp,		DCDR_TABLE,		"Doctor_Percents,Doctor_Speciality,Spclty_largo_prcnt"	},
//  { sizeof(INTERACTION_ROW), sizeof(INTERACTION_ROW),	NOT_SORTED,	RefreshInteractionTable,
//    NULL,		INTR_TABLE,		"Drug_interaction"	},
  { sizeof(MESSAGE_RECID_ROW), sizeof(MESSAGE_RECID_ROW),	NOT_SORTED,	NULL,
    NULL,		MSG_RECID_TABLE,		""	}, // MUST BE BEFORE PRESCRIPTION ID TABLE!!
//  { sizeof(DELPR_RECID_ROW), sizeof(DELPR_RECID_ROW),	NOT_SORTED,	NULL,
//    NULL,		DELPR_ID_TABLE,		""	}, // MUST BE BEFORE PRESCRIPTION ID TABLE!!
  { sizeof(PRESCRIPTION_ROW), sizeof(PRESCRIPTION_ROW),	NOT_SORTED,	RefreshPrescriptionTable,
    NULL,		PRSC_TABLE,		""	},
//  { sizeof(PRESCR_WRITTEN_ROW), sizeof(PRESCR_WRITTEN_ROW),	NOT_SORTED,	RefreshPrescrWrittenTable,
//    NULL,		PRW_TABLE,		""	},
//  { sizeof(DRUG_EXTENSION_ROW), sizeof(DRUG_EXTENSION_ROW),	NOT_SORTED,	RefreshDrugExtensionTable,
//    NULL,		DREX_TABLE,		"Drug_extension"	},
  { sizeof(DIPR_DATA), sizeof(DIPR_DATA),		NOT_SORTED,	NULL,
    DiprComp,		DIPR_TABLE,		""	},
  { 0, 0,			0,			NULL,
    NULL,		"",			""	},
};

// Variable to store the current database isolation level.
//static short	_current_db_isolation_ = _COMMITTED_READ_;	// Default for non-ANSI DB.
static short	_current_db_isolation_ = _DIRTY_READ_;		// Default for ODBC, set at connection time.

// Variable to store the current database connection status.
//static short	_db_connected_ = 0;

// Global variable to control whether we automatically pause after a failed database auto-reconnect.
// DonR 07Oct2021: Change the default for this to TRUE, so we don't write huge volumes to log files
// while a DB connection is down.
short			_PauseAfterFailedReconnect = 1;


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
	int			exception_count;
	char		message [255];
	char		my_sqlerrm [601];
	char		MessageHeader [101];
	int			messlen;
	int			i;
	int			errm_len;
	static int	allow_recursion = 1;	// DonR 23Jan2007: Permits reconnecting to DB without risk of endless loop.


	// If there's no error, quit now.
	if (code->sqlcode == INF_OK)
	{
		return (MAC_OK);
	}

	// Log the error.
	strcpy (buf,		"");
	strcpy (stmt_buf,	"");
	strcpy (temp,		"");

	sprintf (MessageHeader, "%s - fatal error for PID %d", (MAIN_DB->Name == NULL) ? "Unknown DB" : MAIN_DB->Name, getpid());

	// 0. Get the current value of sqlca.sqlerrm.
	// DonR 15Apr2015: I suspect that copying into sqlerrm was causing segmentation faults;
	// accordingly, I've made two changes: First, I lengthened sqlerrm from 255 to 512; and
	// second, I've switched from a "normal" strcpy to strncpy, and added an ASCII zero
	// at the end just in case the source string is longer than 511 characters.
	if (code->sqlerrm == NULL)
	{
		strcpy (my_sqlerrm, "");
	}
	else
	{
		errm_len = strlen(code->sqlerrm);
		if (errm_len > 600)
			errm_len = 600;
		strncpy (my_sqlerrm, code->sqlerrm, errm_len);
		my_sqlerrm[errm_len] = (char)0;
	}

	// 1. Get the SQL diagnostics.
	// DonR 06Jan2020: If ODBC_SQLCODE == SQLCODE, assume that the current error
	// came from the ODBC routines - so skip the GET DIAGNOSTICS stuff and just
	// use what the ODBC routines have already provided.
	strcpy (buf, ODBC_ErrorBuffer);		//Marianna


	// 2. Get the SQL statement
	// DonR 28Jul2025: Getting rid of the "SQLCODE == ODBC_SQLCODE" condition for
	// this bit - I don't think there's any reason for it anymore, and it's possible
	// that in some cases we're seeing the wrong SQL statement text because of it.
//	if ((ODBC_LastSQLStatementText != NULL) && (SQLCODE == ODBC_SQLCODE))
	if (ODBC_LastSQLStatementText != NULL)
	{
//GerrLogMini (GerrId, "INF_ERROR_TEST: length of ODBC_LastSQLStatementText = %d.", strlen (ODBC_LastSQLStatementText));
		if (strlen (ODBC_LastSQLStatementText) < 6000)
			len = sprintf (stmt_buf, ODBC_LastSQLStatementText);
	}

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
	// DonR 27Jul2011: If the error is a table- or row-locked one (which is usually the case),
	// substitute a more concise message.
//GerrLogMini (GerrId, "Before printing error, code->sqlcode = %d, sttm_str %s NULL.", code->sqlcode, (sttm_str == NULL) ? "IS" : "IS NOT");
	if (SQLERR_code_cmp (SQLERR_access_conflict) == MAC_TRUE)
	{
		// DonR 07Sep2023 Bug #469281: If SQLERR_code_cmp() already restored the database connection,
		// we've already got log messages to that effect - so don't log DB_CONNECTION_RESTORED as an
		// access-conflict error here.
		if (code->sqlcode != DB_CONNECTION_RESTORED)
			GerrLogMini (GerrId, "Access conflict error %d in %s at line %d.", code->sqlcode, source_name, line_no);
	}
	else	// Something other than table-locked.
	{
		if (sttm_str == NULL)
		{
//GerrLogMini(GerrId, "\nCalling GmsgLogReturn with sttm_str NULL:\nmessage {%s}\nstatement {%s}\nsqlerrm {%s}\nDatabase = %s", buf,stmt_buf,my_sqlerrm, MAIN_DB->Name);
			GmsgLogReturn (source_name,
						   line_no,
						   MessageHeader,
						   "\n\tSQLCODE. . . : %d\n"
						   "\tDB message . . : %s\n"
						   "\tDB statement   :\n%s\n"
						   "\tDB sqlerrm . . : %s\n",
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
						   MessageHeader,
						   "\n\tDB code. . . . . . : %d\n"
						   "\tDB mess. . . . . . . : %s\n"
						   "\tDB statement . . . . : \n%s\n"
						   "\tDB special message . : %s"
						   "\tDB sqlerrm . . . . . : %s\n",
						   code->sqlcode,
						   buf,
						   stmt_buf,
						   sttm_str,
						   my_sqlerrm);
		}
	}	// Error other than some kind of access conflict.
//GerrLogMini(GerrId, "Back from GmsgLogReturn.");


	// DonR 23Jan2007: Attempt autmatic recovery from DB-disconnected error.
	// DonR 16Jun2008: Added INF_IMPLICIT_NOT_ALLOWED (-1811) and DB_CONNECTION_BROKEN (-25582)
	// to the list of DB errors that will trigger an automatic reconnect attempt.
	// DonR 10Oct2021: Made fixes (here and in MacODBC.h) to support automatic
	// database reconnection for ODBC.
	// DonR 06Sep2023: If the reconnection is successful, change the SQLCODE value to a new,
	// made-up DB_CONNECTION_RESTORED (-25999) value that we can treat as a "contention" error
	// to trigger an automatic retry of the current transaction.
	// NOTE: We also do an auto-reconnect (if necessary) every time we call "SQLERR_code_cmp (SQLERR_access_conflict)",
	// so the version here should be necessary only if (A) some code fails to check for accesss conflicts before checking
	// for other errors, or (B) the maximum number of access-conflict retries was reached and the auto-reconnect still
	// failed. The auto-connect in SQLERR_code_cmp has a pause of only a few seconds after a failed reconnect attempt;
	// here we pause for a full minute, to try to cope with longer-term database connection problems.
	if (((code->sqlcode == INF_DB_NOT_SELECTED)			||
		 (code->sqlcode == INF_CONNECTION_NOT_EXIST)	||
		 (code->sqlcode == INF_CURSOR_NOT_AVAILABLE)	||
		 (code->sqlcode == INF_IMPLICIT_NOT_ALLOWED)	||
		 (code->sqlcode == DB_CONNECTION_BROKEN)		||	// (This is the only error code that's relevant for ODBC, at least under MS-SQL.)
		 (code->sqlcode == INF_CONNECT_ATTEMPT_FAILED))			&&
		(allow_recursion > 0))
	{
		allow_recursion = 0;	// Disable any further recursion.

		GerrLogMini (GerrId, "\nPID %d attempting database disconnect-reconnect.", getpid());

		SQLMD_disconnect ();	// To make sure all databases are disconnected before calling
								// SQLMD_connect(), which assumes that it's starting from zero.

		SQLMD_connect ();		// Try reconnecting to DB - if there is an error,
								// this routine will be called again - but because
								// the recursion flag has been set to zero, we
								// won't loop any deeper!

		GerrLogMini (	GerrId,
						"PID %d auto-reconnect to %s %s successful.%s",
						getpid(),
						(MAIN_DB->Name == NULL) ? "Unknown DB" : MAIN_DB->Name,
						(MAIN_DB->Connected) ? "was" : "was NOT",
						(MAIN_DB->Connected || (!_PauseAfterFailedReconnect)) ? "" : " Sleeping for a minute before retry...");
				
		GerrLogAddBlankLines (1);	// DonR 18Sep2023 (just for pretty).

		allow_recursion = 1;	// Reset recursion flag.

		// DonR 14May2015: If an automatic reconnect attempt failed, pause for a minute - this will
		// prevent filling the disk with gigabytes of error messages, particularly from As400UnixClient.
		if (MAIN_DB->Connected)
		{
			SQLCODE = code->sqlcode = DB_CONNECTION_RESTORED;
		}
		else
		{
			if (_PauseAfterFailedReconnect)
				sleep (60);
		}
	}	// Some form of database-disconnected error.


	// DonR 30Jun2015: I still seem to be seeing some cases (in Test, at least) where As400UnixClient
	// goes into some kind of loop and fills the disk with logged messages. Since this is the only
	// program that sets _PauseAfterFailedReconnect non-zero, it seems reasonable to add a small delay
	// after *any* database error when this flag is set.
	if (_PauseAfterFailedReconnect)
		usleep (250000);	// 1/4 second delay.

	// Exit function with error code.
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
int	INF_CODE_CMP (	struct	sqlca_s *code_strct,	/*sqlca structur ptr*/
					int		error_type				/* error type		*/	)
{
	static int	allow_recursion = 1;	// DonR 23Jan2007: Permits reconnecting to DB without risk of endless loop.


	/*======================================================================
	||		Test error code in structure is of error type					||
	 ======================================================================*/

    switch (error_type)
    {
		case SQLERR_ok:
            return	( (code_strct->sqlcode != INF_OK)				? MAC_FALS : MAC_TRUE );

		case SQLERR_end_of_fetch:
		case SQLERR_not_found:
            return	( (	code_strct->sqlcode != SQLERR_NO_MORE_DATA_OR_NOT_FOUND)
																	? MAC_FALS : MAC_TRUE );

		case SQLERR_no_rows_affected:
            return	( (	code_strct->sqlerrd[2] != 0 )				? MAC_FALS : MAC_TRUE );

		// DonR 03Mar2021: Folding the error codes for SQLERR_deadlocked into
		// SQLERR_access_conflict, so there's no longer any reason to check
		// for both. (I'll keep SQLERR_deadlocked available in case any routine
		// needs to know more specifically what went wrong.)
		case SQLERR_deadlocked:
            return	( (	code_strct->sqlcode != INF_DEADLOCK							&&
						code_strct->sqlcode != SQL_DEADLOCK								)
																	? MAC_FALS : MAC_TRUE );

		// DonR 03Mar2021: Folding the error codes for SQLERR_deadlocked into
		// SQLERR_access_conflict, so there's no longer any reason to check
		// for both. (I'll keep SQLERR_deadlocked available in case any routine
		// needs to know more specifically what went wrong.)
		// DonR 06Sep2023 Bug #469281: Added DB_CONNECTION_RESTORED, which we'll get when
		// there's a glitch in the network DB infrastructure and the database connection has
		// been closed and successfully reopened.
		case SQLERR_access_conflict:

			// DonR 07Sep2023 Bug #469281: Copied database auto-reconnect logic here, so that we can
			// recover from a brief network-connection disruption *within* the normal DB-contention
			// retry loop, without even throwing a critical error at the calling application code.
			// I'm leaving in the list of Informix error codes to trigger the reconnect, although
			// I'm pretty sure that MS-SQL will throw completely different errors. DB_CONNECTION_BROKEN
			// is a "synthetic" error that MacODBC.h substitutes for MS-SQL error 3926, "Transaction in
			// this session has been committed or aborted by another session."

			// DonR 23Jan2007: Attempt autmatic recovery from DB-disconnected error.
			// DonR 16Jun2008: Added INF_IMPLICIT_NOT_ALLOWED (-1811) and DB_CONNECTION_BROKEN (-25582)
			// to the list of DB errors that will trigger an automatic reconnect attempt.
			// DonR 10Oct2021: Made fixes (here and in MacODBC.h) to support automatic
			// database reconnection for ODBC.
			// DonR 06Sep2023: If the reconnection is successful, change the SQLCODE value to a new,
			// made-up DB_CONNECTION_RESTORED (-25999) value so we know that we don't have to reconnect
			// to the database yet again.
			if	(	(	(code_strct->sqlcode == INF_DB_NOT_SELECTED)			||
						(code_strct->sqlcode == INF_CONNECTION_NOT_EXIST)		||
						(code_strct->sqlcode == INF_CURSOR_NOT_AVAILABLE)		||
						(code_strct->sqlcode == INF_IMPLICIT_NOT_ALLOWED)		||
						(code_strct->sqlcode == DB_CONNECTION_BROKEN)			||	// (This is the only error code that's relevant for ODBC, at least under MS-SQL.)
						(code_strct->sqlcode == INF_CONNECT_ATTEMPT_FAILED)			)		&&
					(allow_recursion > 0)
				)
			{
				allow_recursion = 0;	// Disable any further recursion.

				SQLMD_disconnect ();	// To make sure all databases are disconnected before calling
										// SQLMD_connect(), which assumes that it's starting from zero.

				SQLMD_connect ();		// Try reconnecting to DB - if there is an error,
										// this routine will be called again - but because
										// the recursion flag has been set to zero, we
										// won't loop any deeper!

				GerrLogMini (	GerrId,
								"PID %d auto-reconnect to %s %s",
								getpid(),
								(MAIN_DB->Name == NULL)	? "Unknown DB"	: MAIN_DB->Name,
								(MAIN_DB->Connected)	? "succeeded."	: "failed. Sleeping for 3 seconds before retry...");

				GerrLogAddBlankLines (1);	// DonR 18Sep2023 (just for pretty).

				allow_recursion = 1;	// Reset recursion flag.

				// DonR 14May2015: If an automatic reconnect attempt failed, pause for a minute - this will
				// prevent filling the disk with gigabytes of error messages, particularly from As400UnixClient.
				if (MAIN_DB->Connected)
				{
					SQLCODE = code_strct->sqlcode = DB_CONNECTION_RESTORED;
				}
				else
				{
					sleep (3);
				}
			}	// Try to reconnect after some form of database-disconnected error.

            return	( (	code_strct->sqlcode != INF_ACCESS_CONFLICT					&&
						code_strct->sqlcode != SQLERR_CANNOT_POSITION_WITHIN_TABL	&&
						code_strct->sqlcode != INF_CANNOT_POSITION_VIA_INDEX		&&
						code_strct->sqlcode != INF_CANNOT_GET_NEXT_ROW				&&
						code_strct->sqlcode != INF_DEADLOCK							&&
						code_strct->sqlcode != SQL_DEADLOCK							&&
						code_strct->sqlcode != DB_CONNECTION_BROKEN					&&	// DonR 07Sep2023 - TRUE if DB auto-reconnect (above) failed.
						code_strct->sqlcode != DB_CONNECTION_RESTORED				&&	// DonR 06Sep2023.
						code_strct->sqlcode != INF_TABLE_IS_LOCKED_FOR_INSERT			)

																	? MAC_FALS : MAC_TRUE );

		// DonR 08Nov2020: Probably need to add MS-SQL error codes.
		// Another question: do we really need to differentiate between SQLERR_access_conflict
		// and SQLERR_access_conflict_cursor? They both include SQLERR_CANNOT_POSITION_WITHIN_TABL...
		case SQLERR_access_conflict_cursor:
            return	( (	code_strct->sqlcode != INF_CANNOT_READ_SYS_CATALOG			&&
						code_strct->sqlcode != SQLERR_CANNOT_POSITION_WITHIN_TABL		)
																	? MAC_FALS : MAC_TRUE );

		case SQLERR_not_unique:
            return ( (	code_strct->sqlcode != INF_NOT_UNIQUE_INDEX			&&
						code_strct->sqlcode != INF_NOT_UNIQUE_CONSTRAINT	&&	// DonR 08Nov2020: Is there an MS-SQL version of this one?
						code_strct->sqlcode != SQL_NOT_UNIQUE_PRIMARY_KEY	&&	// DonR 08Feb2021: MS-SQL gives a different error code for primary keys.
						code_strct->sqlcode != SQL_NOT_UNIQUE_INDEX				)
																	? MAC_FALS : MAC_TRUE );

		// DonR 08Nov2020: This one doesn't appear to be in use anywhere in our code;
		// I'm not sure what it even means. (Of course, I could look it up - but that
		// would be too simple. <g>)
		case SQLERR_referenced_value_in_use:
			return ( (	code_strct->sqlcode != INF_REFERENCED_VALUE_IN_USE)
																	? MAC_FALS : MAC_TRUE );

		// DonR 08Nov2020: This one doesn't appear to be in use anywhere in our code;
		// I'm not sure what it even means. (Of course, I could look it up - but that
		// would be too simple. <g>)
		case SQLERR_referenced_value_not_found:
			return ( (	code_strct->sqlcode != INF_REFERENCED_VALUE_NOT_FOUND)
																	? MAC_FALS : MAC_TRUE );

		// DonR 08Nov2020: For ODBC, this error is generated by our own code rather than
		// the database server; accordingly, we don't need a separate error code for MS-SQL
		// or any other database server we may use in future.
		case SQLERR_too_many_result_rows:
			return ( (	code_strct->sqlcode != SQLERR_TOO_MANY_RESULT_ROWS)
																	? MAC_FALS : MAC_TRUE );

		// DonR 08Nov2020: This one shouldn't really be needed here, as it will come up only
		// when there's an actual error in coding SQL commands, rather than in normal run-time
		// situations. I don't see any calls to check for SQLERR_column_not_found_in_tables
		// anywhere in our source code.
		case SQLERR_column_not_found_in_tables:
			return ( (	code_strct->sqlcode != INF_COLUMN_NOT_FOUND_IN_TABLES)
																	? MAC_FALS : MAC_TRUE );

		// DonR 04Nov2020: Added Table Not Found codes.
		case SQLERR_table_not_found:
				return ( (	code_strct->sqlcode != INF_TABLE_NOT_FOUND			&&
							code_strct->sqlcode != SQL_TABLE_NOT_FOUND				)	? MAC_FALS : MAC_TRUE );

		// DonR 22Mar2021: Added Unicode Conversion Failed handling - as a rule, this should NOT
		// be treated as a fatal error, I think.
		case SQLERR_data_conversion_problem:
			return ( (	code_strct->sqlcode != SQL_UNICODE_CONVERSION_FAILED)
																	? MAC_FALS : MAC_TRUE );

    }

    return (MAC_FALS);				/* default --- NO MATCH */
}

/*=======================================================================
||                                                                      ||
||			CURRENT_DATE()												||
||                                                                      ||
||	Returns current date & time in dotted formatted string.				||
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
||																		||
||				diff_from_now()											||
||																		||
||		Returns diffrence in seconds from now							||
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
||																		||
||				diff_calendar_months ()									||
||																		||
||		Returns difference in calendar months *without* regard to		||
||		days - so 31 May to 01 June = 1 month.							||
 =======================================================================*/
int	diff_calendar_months (int date0_in, int date1_in)
{
	int			year0;
	int			year1;
	int			month0;
	int			month1;
	int			MonthsSince1900_0;
	int			MonthsSince1900_1;

	// NOTE: This function assumes that the incoming arguments are valid,
	// so it does *not* check for dates that are not in YYYYMMDD format.
	year0	= date0_in / 10000;
	year1	= date1_in / 10000;

	month0	= (date0_in % 10000) / 100;
	month1	= (date1_in % 10000) / 100;

	MonthsSince1900_0 = ((year0 - 1900) * 12) + month0;
	MonthsSince1900_1 = ((year1 - 1900) * 12) + month1;

    return (MonthsSince1900_1 - MonthsSince1900_0);
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


/*=======================================================================
||																		||
||				INF_CONNECT()											||
||																		||
||			Connect to ODBC databases									||
||																		||
 =======================================================================*/
int	INF_CONNECT (char *username, char *password, char *dbname)
{
	int	sql_result;

	char			*ODBC_DSN;
	char			*ODBC_UserName;
	char			*ODBC_Password;
	char			*ODBC_DB_name;
	char			*ODBC_DefaultDB			= NULL;
	char			*ODBC_MirroringEnabled	= NULL;
	char			*ODBC_ConnectTimeout	= NULL;
	short			MS_SQL_Configured		= 0;
	short			Informix_Configured		= 0;

	// Marianna deleted the old embedded-SQL code.

	MS_DB.Connected = INF_DB.Connected = ODBC_MIRRORING_ENABLED = 0;

	// DonR 26Aug2020: Add a new environment parameter to override the default DB
	// connect timeout value.
	ODBC_ConnectTimeout	= getenv ("ODBC_CONNECT_TIMEOUT");

	// DonR 04Dec2019: Open ODBC database(s).
	ODBC_DSN			= getenv ("MS_SQL_ODBC_DSN");
	ODBC_UserName		= getenv ("MS_SQL_ODBC_USER");
	ODBC_Password		= getenv ("MS_SQL_ODBC_PASS");
	ODBC_DB_name		= getenv ("MS_SQL_ODBC_DB");

	if ((ODBC_DSN		!= NULL)	&&
		(ODBC_UserName	!= NULL)	&&
		(ODBC_Password	!= NULL)	&&
		(ODBC_DB_name	!= NULL))
	{
		MS_SQL_Configured = 1;
		sql_result = ODBC_CONNECT (&MS_DB, ODBC_DSN, ODBC_UserName, ODBC_Password, ODBC_DB_name, ODBC_ConnectTimeout);

		if (sql_result)
		{
			GerrLogMini (GerrId, "ODBC connect to MS-SQL %s/%s/%s/%s returns %d, connected = %d.",
						 ODBC_DSN, ODBC_UserName, ODBC_Password, ODBC_DB_name, sql_result, MS_DB.Connected);
		}
	}
//else GerrLogMini (GerrId, "MS-SQL: DSN %s, User %s, Psw %s, DB %s.", ODBC_DSN, ODBC_UserName, ODBC_Password, ODBC_DB_name);

	ODBC_DSN		= getenv ("INF_ODBC_DSN");
	ODBC_UserName	= getenv ("INF_ODBC_USER");
	ODBC_Password	= getenv ("INF_ODBC_PASS");
	ODBC_DB_name	= NULL;

	if ((ODBC_DSN		!= NULL)	&&
		(ODBC_UserName	!= NULL)	&&
		(ODBC_Password	!= NULL))
	{
		Informix_Configured = 1;
		sql_result = ODBC_CONNECT (&INF_DB, ODBC_DSN, ODBC_UserName, ODBC_Password, ODBC_DB_name, ODBC_ConnectTimeout);

		if (sql_result)
		{
			GerrLogMini (GerrId, "ODBC connect to Informix %s/%s/%s/%s returns %d, connected = %d.",
						 ODBC_DSN, ODBC_UserName, ODBC_Password, ODBC_DB_name, sql_result, INF_DB.Connected);
		}
//else GerrLogMini (GerrId, "ODBC connect to Informix %s/%s/%s/%s returns %d, connected = %d.",
//					ODBC_DSN, ODBC_UserName, ODBC_Password, ODBC_DB_name, sql_result, INF_DB.Connected);
	}
//else GerrLogMini (GerrId, "Informix environment variables NOT OK: %s/%s/%s.", ODBC_DSN, ODBC_UserName, ODBC_Password);


	// DonR 29Jul2020: Set new MAIN_DB pointer based on which databases were configured.
	// Note that this is *not* contingent on successful connection - only on how the
	// environment variables are set up. We do *not* want an automatic fall-back to a
	// different database if we failed to connect to the intended one!
	MAIN_DB = &INF_DB;	// For now, start with Informix as the default.

	if (MS_SQL_Configured)
	{
		// If both MS-SQL *and* Informix are configured (meaning that we're connecting
		// to both databases), check the new environment variable ODBC_DEFAULT_DATABASE.
		// If (and only if) we recognize a database name, use that to set MAIN_DB to
		// the desired database.
		if (Informix_Configured)
		{
			ODBC_DefaultDB = getenv ("ODBC_DEFAULT_DATABASE");

			// See if we got a recognizable value for ODBC_DEFAULT_DATABASE
			// and set MAIN_DB accordingly.
			if (ODBC_DefaultDB != NULL)
			{
				if (!strcasecmp (ODBC_DefaultDB, "Informix"))
				{
					MAIN_DB = &INF_DB;	// Informix is the chosen DB.
				}
				else
				{
					if ((!strcasecmp (ODBC_DefaultDB, "MS-SQL")) || (!strcasecmp (ODBC_DefaultDB, "MSSQL")))
					{
						MAIN_DB = &MS_DB;	// MS-SQL is configured, so change the default to MS-SQL.
					}
					// Else nothing - leave the default value of MAIN_DB in place.
				}	// ODBC_DEFAULT_DATABASE <> "Informix".
			}	// ODBC_DEFAULT_DATABASE has a value.


			ODBC_MirroringEnabled = getenv ("ODBC_MIRRORING_ENABLED");

			if (ODBC_MirroringEnabled != NULL)
			{
				if ((!strcasecmp (ODBC_MirroringEnabled, "Yes"		))	||
					(!strcasecmp (ODBC_MirroringEnabled, "On"		))	||
					(!strcasecmp (ODBC_MirroringEnabled, "Enabled"	))	||
					(!strcasecmp (ODBC_MirroringEnabled, "True"		)))
				{
					ODBC_MIRRORING_ENABLED = 1;
				}
			}	// ODBC_MIRRORING_ENABLED has a value.

		}	// Informix *and* MS-SQL are configured by environment variables.

		else	// *Only* MS-SQL is configured, so set MAIN_DB without checking ODBC_DEFAULT_DATABASE.
		{
			MAIN_DB = &MS_DB;	// Only MS-SQL is configured, so change the default to MS-SQL.
		}
	}	// MS-SQL is configured by environment variables.

	// We already set Informix as an unconditional default, but let's set it again
	// on the basis that if's should have else's!
	else
	{
		MAIN_DB = &INF_DB;	// Informix is the default since MS-SQL wasn't configured.
	}

	// DonR 23Aug2020: Set up ALT_DB as a secondary database pointer. If only one database
	// is connected, ALT_DB will have the same value as MAIN_DB; if two databases are
	// connected, it will be whichever one is *not* MAIN_DB.
	if (MAIN_DB == &INF_DB)
	{
		ALT_DB = (MS_DB.Connected) ? &MS_DB : MAIN_DB;
	}
	else
	{
		ALT_DB = (INF_DB.Connected) ? &INF_DB : MAIN_DB;
	}

GerrLogMini (GerrId, "Informix_Configured = %d, MS_SQL_Configured = %d, ODBC_DefaultDB = %s,\n     MAIN_DB = %s, ALT_DB = %s, Mirroring %sabled.",
			 Informix_Configured, MS_SQL_Configured,
			 (ODBC_DefaultDB == NULL) ? "NULL" : ODBC_DefaultDB,
			 (MAIN_DB == &INF_DB) ? "Informix" : (MAIN_DB == &MS_DB) ? "MS-SQL" : "(none)",
			 ( ALT_DB == &INF_DB) ? "Informix" : ( ALT_DB == &MS_DB) ? "MS-SQL" : "(none)",
			 (ODBC_MIRRORING_ENABLED) ? "en" : "dis");


    return (sql_result);

}


/*=======================================================================
||																		||
||				INF_DISCONNECT()										||
||																		||
||			Disconnect from all databases								||
||																		||
 =======================================================================*/
int	INF_DISCONNECT ()
{
	CleanupODBC (&INF_DB);
	CleanupODBC (&MS_DB);
	
//	GerrLogMini (GerrId, "INF_DISCONNECT: Disconnected from all databases.");

	// Marianna deleted old embedded-SQL code.

	return (0);
}

/*=======================================================================
||																		||
||				INF_BEGIN()												||
||																		||
||			Begin transaction in Informix database						||
||																		||
 =======================================================================*/
		// Marianna - disabled all the code here, since ODBC doesn't use BEGIN WORK or
		// any equivalent; transactions are started automatically.
int	INF_BEGIN()			//Marianna	25052020
{
		return (0);		//Marianna 25052020
}

/*=======================================================================
||									||
||				INF_COMMIT()				||
||									||
||			Commit Informix transaction			||
||									||
 =======================================================================*/
			// Marianna cleaned up old code.
int	INF_COMMIT()			//Marianna 25052020
{
		return (0);
}

/*=======================================================================
||									||
||				INF_ROLLBACK()				||
||									||
||			Rollback Informix transaction			||
||									||
 =======================================================================*/
		// Marianna cleaned up old code.
int	INF_ROLLBACK()		//Marianna 25052020
{
		return (0);
}

/*=======================================================================
||																		||
||				INF_CONNECT_ID()										||
||																		||
||			Connect to Informix database with id						||
||																		||
 =======================================================================*/
		// Marianna cleaned up old code.
int	INF_CONNECT_ID(
		       char	*username,	/* username		*/
		       char	*password,	/* password		*/
		       char	*dbname,	/* database name	*/
		       char	*conn_id	/* connection id	*/
		       )										//Marianna 25052020
{
		return (0);
}

/*=======================================================================
||																		||
||				INF_DISCONNECT_ID()										||
||																		||
||			Disconnect from Informix database							||
||																		||
 =======================================================================*/
		// Marianna cleaned up old code.
int	INF_DISCONNECT_ID (char	*conn_id	/* connection id	*/ )
		//Marianna 25052020
{
		return (0);
}

/*=======================================================================
||									||
||				INF_SET_CONNECTION()			||
||									||
||			Switch to Informix database connection		||
||									||
 =======================================================================*/
		// Marianna cleaned up old code.
int	INF_SET_CONNECTION (char	*conn_id	/* connection id	*/ )
		//Marianna 25052020
{
		return (0);
}


#if 0
/*=======================================================================
||									||
||				RefreshPharmacyTable()			||
||									||
||			Refresh pharmacy shared memory table		||
||									||
 =======================================================================*/
int	RefreshPharmacyTable (int	entry)
{

	// Return -- ok
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
	return( MAC_OK );
}


/*=======================================================================
||									||
||				RefreshPercentTable()			||
||									||
||			Refresh percent shared memory table		||
||									||
 =======================================================================*/
int	RefreshPercentTable (int	entry)
{

	//  Return -- ok
    return( MAC_OK );
}


/*=======================================================================
||																		||
||			RefreshDoctorDrugTable()									||
||																		||
||			Refresh doctor drug shared memory table						||
||																		||
 =======================================================================*/
int	RefreshDoctorDrugTable (int	entry)
{
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
	//  Return -- ok
    return (MAC_OK);
}
#endif


/*=======================================================================
||																		||
||			RefreshPrescriptionTable()									||
||																		||
||			Refresh prescription shared memory table					||
||																		||
||          New Variant GAL 14/06/05									||
 =======================================================================*/

// DonR 12Apr2022: Tightened up log messages and disabled stuff for presc_delete, since
// the old-style prescription deletion (with its associated tables) is no longer in use.

int	RefreshPrescriptionTable( int entry )
{
    static PRESCRIPTION_ROW		prescription_row;
    static MESSAGE_RECID_ROW	msg_recid_row;

    TSqlInd		presc_ind;

	PRESCRIPTION_ROW	*prescription_row_ptr;
	MESSAGE_RECID_ROW	*msg_recid_row_ptr;

	int		h_nUpperLimit;
  	int		h_nLowLimit;
	int		recid_UpperLimit;
  	int		recid_LowLimit;
	int		delpr_UpperLimit;	// Still read from presc_per_host, even though presc_delete is no longer used.
	int		delpr_LowLimit;		// Still read from presc_per_host, even though presc_delete is no longer used.
	char	h_szHostName[ 256 + 1 ];

    RAW_DATA_HEADER	raw_data_header , *raw_data_header_ptr;

    TABLE		*prsc_tablep;
    TABLE		*recid_tablep;

	char		buf[ 1024 ];
    int         nMacSys = PHARM_SYS;
	ISOLATION_VAR;


	REMEMBER_ISOLATION;


	if (!gethostname (h_szHostName, MAX_NAME_LEN))
	{
		GerrLogMini (GerrId , "Host : \"%s\"" , h_szHostName );

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

			ExecSQL (	MAIN_DB, READ_presc_per_host,
						&h_nLowLimit,		&h_nUpperLimit,		&recid_LowLimit,
						&recid_UpperLimit,	&delpr_LowLimit,	&delpr_UpperLimit,
						&h_szHostName,		END_OF_ARG_LIST								);

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
			GerrLogMini (GerrId, "Table '%s' locked.. failed after '%d' retries", "presc_per_host", SQL_UPDATE_RETRIES);

			RESTORE_ISOLATION;
			return INF_ERR;
		}

		GerrLogMini (GerrId,	"Prescription_id range: %d -> %d,\n"
								"Message ID range:      %d -> %d.  ",
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

   				// Get max presc id in given range
				// DonR 26Jun2005: changed the selection criteria from BETWEEN
				// to LowLimit <= max < UpperLimit. This means that if we set
				// the limit to between 30,000,000 and 40,000,000 the actual
				// maximum value we will look for will be 39,999,999. This
				// avoids a potential major bug, since people may enter criteria
				// into presc_per_host such that the same (round) number is used
				// for the top of one range and the bottom of another.
				ExecSQL (	MAIN_DB, READ_max_prescripton_id,
							&prescription_row.prescription_id,
							&h_nLowLimit,
							&h_nUpperLimit,
							END_OF_ARG_LIST							);

				presc_ind = ColumnOutputLengths [1];

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
				ExecSQL (	MAIN_DB, READ_max_messages_details_rec_id,
							&msg_recid_row.message_rec_id,
							&recid_LowLimit,
							&recid_UpperLimit,
							END_OF_ARG_LIST									);

				presc_ind = ColumnOutputLengths [1];

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


				if ( restart == MAC_TRUE )	// transaction faild -- try again
				{
				     sleep( ACCESS_CONFLICT_SLEEP_TIME );
			    }

		   } // for( restart = ... )

		   if ( restart == MAC_TRUE )
		   {
				GerrLogMini (GerrId, "Table '%s' locked.. failed after '%d' retries", "prescriptions", SQL_UPDATE_RETRIES);

				RESTORE_ISOLATION;
				return INF_ERR;
			}
		}	// This is a Pharmacy (or combination) system.

	} // get host name successfully
	else
	{
			GerrLogMini (GerrId, "Function <gethostname> failed , Error #%d \"%s\"", errno , strerror (errno));

			RESTORE_ISOLATION;
			return -1;  // ?!?
	}

    GerrLogMini (GerrId , "Next Presc ID = %d, next Message ID = %d.", prescription_row.prescription_id, msg_recid_row.message_rec_id);


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

	char	v_short_name [3];
	char	h_szHostName [256 + 1 ];


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
				ExecSQL (	MAIN_DB, READ_GetComputerShortName,
							&v_short_name, &h_szHostName, END_OF_ARG_LIST	);

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


#if 0
/*=======================================================================
||																		||
||			RefreshPrescrWrittenTable()									||
||																		||
||			Initializa prescriptions-written shared memory table		||
||																		||
 =======================================================================*/
int	RefreshPrescrWrittenTable (int entry)
{
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
#endif


/*=======================================================================
||																		||
||			GET_ERROR_SEVERITY()										||
||																		||
||		Get error severity by error code								||
||																		||
 =======================================================================*/
short GET_ERROR_SEVERITY (short error_code_in)
{
	int				severity_found		= 0;
	short			DbErrorCode;
	short			severity_level;
	short			CacheOffset;

	short			NeedToClearCache	= 0;
	short			NeedToLoadCache		= 0;

	static int		LastClearedDate		= 0;
	static short	CacheIsLoaded		= 0;
	static char		SeverityCache		[600];

	ISOLATION_VAR;


	REMEMBER_ISOLATION;
	SET_ISOLATION_DIRTY;


	// Decide what we need to do. There are three basic considerations:
	// 1) We want to use cached severity values as much as possible for
	//    frequently used error codes, in order to maximize performance.
	// 2) We want to avoid using stale cached values, so at a minimum we
	//    want to reload the cache once every 24 hours or so. (The good
	//    news is that pc_error_message, which is the source of the cached
	//    data, is mostly static.)
	// 3) We don't want to load (or reload) the whole cache while we're in
	//    the middle of handling a pharmacy transaction - doing so probably
	//    wouldn't cause serious problems, but we want to keep real-time
	//    transactions running as fast as possible. (Looking up error
	//    severities one code at a time shouldn't slow things down very
	//    much, and doing so will gradually build up the cache even if we
	//    never got around to loading the whole pc_error_message table.)
	// Note that passing -1 to this routine will load/refresh the cache
	// conditionally, only if the cache is empty or stale; -2 (or any other
	// negative number) will load/refresh the cache unconditionally.
	NeedToLoadCache =	 (error_code_in <  -1) ||
						((error_code_in == -1) && ((LastClearedDate != GlobalSysDate) || (!CacheIsLoaded)));

	NeedToClearCache = NeedToLoadCache || (LastClearedDate != GlobalSysDate);


	// Since real error severities can go from 0 to 10, use 127 as
	// our "unitialized" value. If the incoming error code is less
	// than zero, this is a request to (re-)load the cache - so
	// we want to start out with the whole cache set to 127. Note
	// that if the cache is stale (meaning that it hasn't been
	// (re-)loaded today, we clear it but do *not* automatically
	// reload the whole thing - we don't want to do that while the
	// program is in the middle of handling a pharmacy request!
	// Normally the cache should be reloaded at night, so it will
	// be ready when needed.
	if (NeedToClearCache)
	{
		LastClearedDate	= GlobalSysDate;
		CacheIsLoaded	= 0;
		memset ((char *)SeverityCache, (char)127, sizeof (SeverityCache));
	}

	if (NeedToLoadCache)
	{
		DeclareAndOpenCursorInto (	MAIN_DB, RefreshSeverityTable_cur,
									&DbErrorCode,	&severity_level,	END_OF_ARG_LIST	);

		if (SQLCODE != 0)
			SQLERR_error_test ();

		while (SQLCODE == 0)
		{
			FetchCursor (	MAIN_DB, RefreshSeverityTable_cur	);

			// Exit loop on end of fetch.
			if (SQLERR_code_cmp (SQLERR_end_of_fetch))
			{
				CacheIsLoaded = 1;
				break;
			}

			// Exit loop on error - *without* setting the "cache loaded" flag TRUE.
			BREAK_ON_ERR (SQLERR_error_test ());

			// Severity values from the pc_error_message table should always
			// be between 0 and 10. If we read something different, just skip
			// past it for now - meaning that we'll force a "live" read from
			// the table and output a diagnostic at that point.
			if ((severity_level < 0) || (severity_level > 10))
				continue;

			// If we get here, we successfully read a row. Now, assuming it's
			// one of the rows we actually want to cache, store its value in
			// the appropriate slot. Note that in order to provide a space for
			// Error Code 0 in the cache, we are *not* leaving room for Error
			// Code 100 - it's not in the table anyway, so there's not much
			// point in leaving a space for it.
			if ( DbErrorCode == 0)
				SeverityCache [DbErrorCode      ] = (char)severity_level;	// Cache slot 0
			else
			if ((DbErrorCode >= 101) && (DbErrorCode < 250))
				SeverityCache [DbErrorCode - 100] = (char)severity_level;	// Cache slots 1-149
			else
			if ((DbErrorCode >= 5000) && (DbErrorCode < 5050))
				SeverityCache [DbErrorCode - 4850] = (char)severity_level;	// Cache slots 150-199
			else
			if ((DbErrorCode >= 6000) && (DbErrorCode < 6300))
				SeverityCache [DbErrorCode - 5800] = (char)severity_level;	// Cache slots 200-499
			else
			if ((DbErrorCode >= 8000) && (DbErrorCode < 8100))
				SeverityCache [DbErrorCode - 7500] = (char)severity_level;	// Cache slots 500-599
			// Else just ignore the row and continue.
		}	// while SQLCODE == 0

		CloseCursor (	MAIN_DB, RefreshSeverityTable_cur	);
	}	// NeedToLoadCache is TRUE.


	// Now that we've handled initialization/reload requests, deal with "real" requests.
	if (error_code_in >= 0)
	{
		if ( error_code_in == 0)
			CacheOffset = error_code_in      ;	// Cache slot 0
		else
		if ((error_code_in >= 101) && (error_code_in < 250))
			CacheOffset = error_code_in - 100;	// Cache slots 1-149
		else
		if ((error_code_in >= 5000) && (error_code_in < 5050))
			CacheOffset = error_code_in - 4850;	// Cache slots 150-199
		else
		if ((error_code_in >= 6000) && (error_code_in < 6300))
			CacheOffset = error_code_in - 5800;	// Cache slots 200-499
		else
		if ((error_code_in >= 8000) && (error_code_in < 8100))
			CacheOffset = error_code_in - 7500;	// Cache slots 500-599
		else CacheOffset = -1;

		// If the requested error code is one that's supposed to be cached,
		// retrieve its severity from the cache; otherwise set a value of
		// 127 to indicate that we need to read the severity value from the
		// pc_error_message table. Note that the cached value may be 127,
		// meaning that the cache has not yet been loaded for this error
		// code; in this case we still have to read from the database table.
		if (CacheOffset >= 0)
			severity_level = SeverityCache [CacheOffset];
		else
			severity_level = 127;

		// DonR 05Jan2004: As a fallback, try looking at the "real" database table if
		// we didn't find anything in the cache. This means that we don't have to bring
		// the whole system down every time we add a new error code to the table.
		if ((severity_level < 0) || (severity_level > 10))	// The only other possible value *should* be 127.
		{
			ExecSQL (	MAIN_DB, READ_GET_ERROR_SEVERITY,
						&severity_level, &error_code_in, END_OF_ARG_LIST	);

//GerrLogMini (GerrId, "Pid %d error_code_in %d got severity %d from DB, SQLCODE = %d.", getpid(), error_code_in, severity_level, SQLCODE);
			if (SQLERR_code_cmp (SQLERR_ok) == MAC_TRUE)
			{
				severity_found = 1;

				// If we're working with an error code that's supposed to be cached, *and*
				// we got a valid value from the pc_error_message table, store that value
				// in the cache.
				if (CacheOffset > -1)
				{
					if ((severity_level >= 0) && (severity_level <= 10))
					{
						SeverityCache [CacheOffset] = (char)severity_level;
					}
					else
					{
						GerrLogMini (GerrId, "GET_ERROR_SEVERITY read out-of-range severity %d for Error Code %d; not caching it!",
									 severity_level, error_code_in);
					}
				}
				else
				{
					// Log non-standard severities for non-cached error codes.
					if ((severity_level < 0) || (severity_level > 10))
						GerrLogMini (GerrId, "GET_ERROR_SEVERITY read out-of-range severity %d for non-cached Error Code %d.",
									 severity_level, error_code_in);
				}
			}	// Good read of pc_error_message table.
			else
			{
				if (SQLERR_code_cmp (SQLERR_not_found) == MAC_TRUE)
				{
					GerrLogMini (GerrId, "GET_ERROR_SEVERITY failed to read severity for Error Code %d - SQLCODE = %d.",
								 error_code_in, SQLCODE);
				}
				else
				{
					SQLERR_error_test ();
				}
			}	// Could *not* read row from pc_error_message table.
		}	// No valid cached severity level was found.
		else
		{
			// We did find a cached, valid severity level.
			severity_found = 1;
		}


		if (!severity_found)
		{
			GerrLogMini (GerrId,
						 "No severity level found for error code %d in pc_error_message table.",
						 error_code_in);
		}
	}	// Error_code_in was 0 or more (in other words, this was *not* a load-cache command).

	RESTORE_ISOLATION;

	// Note that for load-cache calls (error_code_in < 0), the return
	// value is meaningless.
	return (severity_level);
}


/*=======================================================================
||																		||
||			GET_PRESCRIPTION_ID()										||
||																		||
||		Get free prescription id from shared memory						||
||																		||
 =======================================================================*/
TErrorCode GET_PRESCRIPTION_ID (TRecipeIdentifier *recipe_identifier)
{
    TABLE		*prsc_tablep;

    PRESCRIPTION_ROW	prescription_row;

    int			state;
	int			actitems_state;


    state = OpenTable (PRSC_TABLE, &prsc_tablep);

    if (state < MAC_OK)
    {
		return (ERR_SHARED_MEM_ERROR);
    }

    actitems_state = ActItems (prsc_tablep, 1, GetPrescriptionId, (OPER_ARGS)&prescription_row);

    state = CloseTable (prsc_tablep);

    if( state < MAC_OK )
    {
		return (ERR_SHARED_MEM_ERROR);
    }

    *recipe_identifier = prescription_row.prescription_id;

//	printf("New prescription_id = %d\n" , prescription_row.prescription_id );fflush(stdout);

	return (NO_ERROR);

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


#if 0
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
#endif


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

	char DB_TS_buffer		[ 8 + 1];
	static char TS_buffer	[50 + 1];


	// Save previous SQL error status.
	// DonR 02May2024 BUG FIX: Moved the saving and restoration of previous SQLCODE
	// and number-of-rows values outside the "if", so we won't ever lose the previous
	// values. This was a problem in test environments, because the "if" always forced
	// an SQL "failure" in getting the timestamp in non-Production environments.
	PrevSQLCODE			= SQLCODE;
	PrevNumRows			= sqlca.sqlerrd[2];
//	PrevStatementText	= ODBC_LastSQLStatementText;

	// DonR 15Jan2020: Don't bother trying to get the DB timestamp if
	// we already know we're not connected to the database.
	// DonR 13Feb2020: Added global variable ODBC_AvoidRecursion. If
	// this is set TRUE, the calling routine is ODBC_Exec - and we don't
	// want to execute READ_GetCurrentDatabaseTime, since we're already
	// in the middle of some kind of database operation.
	if ((MAIN_DB != NULL) && (MAIN_DB->Connected > 0) && (!ODBC_AvoidRecursion) && (TikrotProductionMode))
	{
		// DonR 26Jan2020: It would appear that the ODBC driver (at least for Informix)
		// doesn't like static variables - maybe it thinks they're supposed to be constants -
		// and the program crashes unceremoniously at this call. It seems to work fine if I
		// pass in the *non*-static buffer, so it would appear that that's the best fix.
		ExecSQL (	MAIN_DB, READ_GetCurrentDatabaseTime, &DB_TS_buffer, END_OF_ARG_LIST	);
		strcpy (TS_buffer, DB_TS_buffer);
	}
	else
	{
		SQLCODE = -1;
	}

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
	// DonR 02May2024 BUG FIX: Moved the saving and restoration of previous SQLCODE
	// and number-of-rows values outside the "if".
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
||		           StripAllSpaces()										||
||																		||
||	Strip leading and trailing spaces, down to length = 0				||
||																		||
 =======================================================================*/
char * StripAllSpaces (unsigned char * source)
{
	int		L;
	int		i;
	int		n;
	int		d;

	// DonR 26Jan2022: Changed the function type from void to char *, to make it easy to embed
	// in strcmp() calls. To support this more cleanly, added an outer "do...while(0)" loop;
	// this lets us have a single exit point from the function instead of multiple "return"
	// statements.
	do
	{
		// First, strip trailing spaces.
		L = strlen((char *)source);

		// Sanity check: if string is less than 1 characters or more than 5000 characters, do nothing.
		if ((L < 1) || (L > 5000))
			break;

		while ((L > 0) && (source[L - 1] == ' '))
		{
			L--;
		}
		source[L] = (char)0;

		// If we're down to length zero, we're done.
		if (L < 1)
			break;

		// Find first non-space character. If we get here, the string must have
		// something other than spaces in it.
		n = 0;
		while (source[n] == ' ')
			n++;

		// Copy "real" text to beginning of buffer.
		for (i = 0; i <= (L - n); i++)
			source[i] = source[i + n];
	}
	while (0);

	// DonR 26Jan2022: For convenience, return the input char * as output. This way we can
	// easily embed StripAllSpaces() in a strcmp() call.
	return (source);
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

		// Reverse Hebrew text.
		buf_reverse (source + pos, to_pos - pos);

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

			// Re-reverse number inside Hebrew text
			buf_reverse (&source[pos], to_num_pos - pos);
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
	buf_reverse (source, L);

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
					buf_reverse (start_segment, Reverse_N_Chars);
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
				buf_reverse (start_segment, segment_length);
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

	// DonR 25Aug2020: Let's try enabling DB isolation control. For now, at least,
	// try to do it for all open ODBC databases (i.e. Informix and MS-SQL).

	// DonR 20Aug2020: For some reason, setting DB isolation seems to be failing
	// once we start doing anything - even just reading from the database. Rather
	// than keep trying to solve this, let's just leave this functionality disabled
	// and revisit it when and if we have to. Rather than remark out the code, just
	// force the current-isolation value equal to the requested value; this will
	// prevent execution of the block of code that actually tries to change the
	// isolation.
	// DonR 14Jan2021: Enabling isolation-control functionality for MS-SQL *only*.
	// We can assume that any use of the alternate DB will be less speed-sensitive,
	// so we're going to change isolation for the main database *only*.
	if (MAIN_DB == &INF_DB)
		_current_db_isolation_ = new_isolation_mode_in;

	// If the requested isolation level is the same as the current level,
	// there's no need to do anything - just return an OK status.
	if (new_isolation_mode_in != _current_db_isolation_)
	{
//GerrLogMini (GerrId, "Switching isolation to %s.", (new_isolation_mode_in == _DIRTY_READ_) ? "DIRTY" : "COMMITTED");
		switch (new_isolation_mode_in)
		{
			case _DIRTY_READ_:		ExecSQL (	MAIN_DB, SET_isolation_dirty, END_OF_ARG_LIST	);
if (SQLCODE) GerrLogMini (GerrId, "SET_isolation_dirty SQLCODE = %d", SQLCODE);
//									err_rtn = SetDirtyRead (MAIN_DB);
//									if ((ALT_DB != MAIN_DB) && (ALTERNATE_DB_USED))
//										SetDirtyRead (ALT_DB);
//									if (ALTERNATE_DB_USED)
//										ExecSQL (	ALT_DB, SET_isolation_dirty, END_OF_ARG_LIST	);
									break;

			case _COMMITTED_READ_:	ExecSQL (	MAIN_DB, SET_isolation_committed, END_OF_ARG_LIST	);
if (SQLCODE) GerrLogMini (GerrId, "SET_isolation_committed SQLCODE = %d", SQLCODE);
//									err_rtn = SetCommittedRead (MAIN_DB);
//									if (ALT_DB != MAIN_DB)
//										SetCommittedRead (ALT_DB);
//									if (ALTERNATE_DB_USED)
//										ExecSQL (	ALT_DB, SET_isolation_committed, END_OF_ARG_LIST	);
									break;

			// DonR 25Aug2020: I don't think we've ever used Cursor Stability; for now
			// at least, just treat this as if it were a synonym for Repeatable Read.
			case _CURSOR_STABILITY:
			case _REPEATABLE_READ_:	ExecSQL (	MAIN_DB, SET_isolation_repeatable, END_OF_ARG_LIST	);
if (SQLCODE) GerrLogMini (GerrId, "SET_isolation_repeatable SQLCODE = %d", SQLCODE);
//									err_rtn = SetRepeatableRead (MAIN_DB);
//									if (ALT_DB != MAIN_DB)
//										SetRepeatableRead (ALT_DB);
//									if (ALTERNATE_DB_USED)
//										ExecSQL (	ALT_DB, SET_isolation_repeatable, END_OF_ARG_LIST	);
									break;

			default:				err_rtn = -1;	// Unrecognized mode requested.
									break;
		}

//		if (err_rtn == NO_ERROR)
		if (SQLCODE == 0)
			_current_db_isolation_ = new_isolation_mode_in;
	}	// Isolation is changing.

	return (SQLCODE);
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
