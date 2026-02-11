/*=======================================================================
||																		||
||				MessageFuncs.c											||
||																		||
||======================================================================||
||																		||
||  PURPOSE:															||
||																		||
||  General function for messages comeing from T-SWITCH.				||
||																		||
||----------------------------------------------------------------------||
|| PROJECT:	SqlServer. (for macabi)										||
||----------------------------------------------------------------------||
|| PROGRAMMER:	Gilad Haimov.											||
|| DATE:	Aug 1996.													||
|| COMPANY:	Reshuma Ltd.												||
|| WORK DONE:	Basic functions.										||
||----------------------------------------------------------------------||
|| PROGRAMMER:	Boaz Shnapp.											||
|| DATE:	Nov 1996.													||
|| COMPANY:	Reshuma Ltd.												||
|| WORK DONE:	More functions.											||
||----------------------------------------------------------------------||
|| PROGRAMMER:	Ely Levy.												||
|| DATE:	Nov 1996.													||
|| COMPANY:	Reshuma Ltd.												||
|| WORK DONE:	More functions.											||
||----------------------------------------------------------------------||
||				Include files.											||
 =======================================================================*/

#include <MacODBC.h>
#include "PharmacyErrors.h"
#include "MsgHndlr.h"
#include "DBFields.h"
#include "MessageFuncs.h"		// DonR 26Aug2003: Function prototypes.
#include "TikrotRPC.h"

/*=======================================================================
||			    Local declarations				||
 =======================================================================*/
static char *GerrSource = __FILE__;

extern	TDrugsBuf	v_DrugsBuf;	// DonR 16Feb2006 - made drugs-in-blood global.

char	*curr_pos_in_mesg;

#define	SET_POS(pos)			\
	curr_pos_in_mesg = (pos);

#define LOG_CONFLICT()			\
	SQLERR_error_test();

// DonR 03Mar2021: Folding the error codes for SQLERR_deadlocked into
// SQLERR_access_conflict, so there's no longer any reason to check
// for both. (I'll keep SQLERR_deadlocked available in case any routine
// needs to know more specifically what went wrong.)
#define Conflict_Test(r)										\
if (SQLERR_code_cmp (SQLERR_access_conflict)	== MAC_TRUE)	\
{																\
	SQLERR_error_test ();										\
	r = MAC_TRUE;												\
	sleep (ACCESS_CONFLICT_SLEEP_TIME);							\
	break;														\
}

#define	RIDICULOUS_HIGH_PRICE			1073676288

// DonR 03Feb2019 CR #27974: Expand purchase-limit history checking from 90 days
// to 750 days. Note that up to now, we retained only 365 days' history; I've changed
// the relevant values in the shrinkdoctor table to retain two years' worth.
// DonR 07Feb2019: The actual number of 30-day months to check is now set in
// sysparams/bakarakamutit_mons - but PURCHASE_LIMIT_HISTORY_MONTHS is still used
// to dimension the relevant arrays.
#define PURCHASE_LIMIT_HISTORY_MONTHS	37

// DonR 03Jan2018 CR #13453 - Al Tor no-show enhancement.
extern short	no_show_reject_num;
extern short	no_show_hist_days;
extern short	no_show_warn_code;
extern short	no_show_err_code;

// DonR 23Apr2025 User Story #390071: Add a new sysparams parameter to control the
// lookup of doctor-chosen "nohalim".
// DonR 13May2025 User Story #388766: And another one for special drug-shortage
// "nohalim".
extern short	MaxDoctorRuleConfirmAuthority;
extern short	DrugShortageRuleConfAuthority;

int	SUPPLY_compare (const void *data1, const void *data2);


/* ========================================================================= *
 *                                                                           *
 *                            AsciiToBin                                     *
 *           Translate an ascii number( "127" ) into binary( 127 )           *
 *                                                                           *
 * ========================================================================= *
 */
void AsciiToBin (char *buff, int len, int *p_val)
{
	char          tmp[64];
	static char	*pos_in_buff;


	memcpy (tmp, buff, len);
	*(tmp + len) =  0;
	*p_val = atoi (tmp);
}
/* End of AsciiToBin() */


/* ========================================================================= *
 *                                                                           *
 *                           WriteErrAndexit                                 *
 *           		     Termination of program:                         *
 *                                                                           *
 * ========================================================================= *
 */
void WriteErrAndexit(
		     int       errcode ,
		     int       msg_index
		     )
{
  fprintf( stderr ,"\nReceived message %d was not processed due to"
	   " error number %d at line %d ." ,
	   msg_index, errcode, __LINE__ ) ;
  exit( errcode ) ;
}
/* End of WriteErrAndexit() */


/* ========================================================================= *
 *                                                                           *
 *                         get_message_header                                *
 *           	Get message index & length from input file:                  *
 *                                                                           *
 * ========================================================================= *
 */
int	get_message_header (char *input_buf, int *pMsgID, int *pMsgEC)
{
	AsciiToBin (input_buf, MSG_ID_LEN, pMsgID);

	AsciiToBin (input_buf + MSG_ID_LEN, MSG_ERROR_CODE_LEN, pMsgEC);

	return (RC_SUCCESS);
}
/* End of get_message_header() */



/* ========================================================================= *
 *                                                                           *
 *                              verify_numeric                               *
 *			       Test numeric data									     *
 *                                                                           *
 * ========================================================================= *
 */
short int verify_numeric( char *ptr, int ascii_len )
{
  int	i;
  bool	FoundError = false;


  for( i = 0 ; i < ascii_len ;  i++ )
    {
      switch( ptr[i] )
	{
	case '0':
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	case '8':
	case '9':
	  continue;
	case '-':
	case '+':
	  if( i == 0 ) continue;
	default:	  
	  SET_ERROR( ERR_WRONG_FORMAT_FILE );
	  FoundError = true;
	  break;
	case '\0':
	  SET_ERROR( ERR_FILE_TOO_SHORT );
	  FoundError = true;
	  break;
	}
      return( -1 );
    }

  // DonR 13Aug2024: If we're returning an error, also provide a more
  // informative message to the program log.
  if (FoundError)
	  GerrLogMini (GerrId, "verify_numeric returning invalid-value code %d on {%s}.", glbErrorCode, ptr);

  return( 0 );
}
/* End of verify_numeric() */



/* ========================================================================= *
 *                                                                           *
 *                              GetChar                                     *
 *        Get a single (non-terminated) character field from buffer			*
 *			& promote buffer position										*
 *                                                                           *
 * ========================================================================= *
 */
char GetChar (char **p_buffer)
{
	char	tmp;

	tmp = **p_buffer;

	SET_POS (*p_buffer);

	*p_buffer += 1;	// promote buffer position

	return (tmp);	// return char value
}
/* End of GetChar() */



/* ========================================================================= *
 *                                                                           *
 *                              GetShort                                     *
 *        Get a short field from buffer & promote buffer position            *
 *                                                                           *
 * ========================================================================= *
 */
short int GetShort( char **p_buffer, int ascii_len )
{
  char			tmp[1024] ;
  int			rc;

  memcpy( tmp, *p_buffer, ascii_len );

  tmp[ascii_len] = 0;		/*  null terminate string    */

  SET_POS( *p_buffer );

  *p_buffer  +=  ascii_len ;    /*  promote buffer position  */

  rc = verify_numeric( tmp, ascii_len );

  if( rc )
    return( rc );

  return (short)atoi( tmp ) ;          /*  return short value       */
}
/* End of GetShort() */



/* ========================================================================= *
 *                                                                           *
 *                              GetInt                                       *
 *        Get an integer field from buffer & promote buffer position         *
 *                                                                           *
 * ========================================================================= *
 */
int GetInt( char **p_buffer, int ascii_len )
{
  char 			tmp[1024] ;
  int			rc;
  
  memcpy( tmp , *p_buffer, ascii_len );
  
  tmp[ascii_len] = 0;		/*  null terminate string    */

  SET_POS( *p_buffer );

  *p_buffer += ascii_len;       /*  promote buffer position  */

  rc = verify_numeric( tmp, ascii_len );

  if( rc )
    return( rc );

  return atoi( tmp );           /*  return long value       */
}
/* End of GetInt() */



/* ========================================================================= *
 *                                                                           *
 *                              GetLong                                      *
 *        Get a long field from buffer & promote buffer position             *
 *                                                                           *
 * ========================================================================= *
 */
long int GetLong( char **p_buffer, int ascii_len )
{
  char 			tmp[1024] ;
  int			rc;
  
  memcpy( tmp , *p_buffer, ascii_len );
  
  tmp[ascii_len] = 0;		/*  null terminate string    */

  SET_POS( *p_buffer );

  *p_buffer += ascii_len;       /*  promote buffer position  */

  rc = verify_numeric( tmp, ascii_len );

  if( rc )
    return( rc );

  return atol( tmp );           /*  return long value       */
}
/* End of GetLong() */



/* ========================================================================= *
 *                                                                           *
 *                              GetString                                    *
 *        Get a string field from buffer & promote buffer position           *
 *                                                                           *
 * ========================================================================= *
 */
void GetString (
		char		** p_buffer ,
		char		*str,
		int		ascii_len
		)
{
  memcpy( str, *p_buffer , ascii_len ) ;

  str[ascii_len]  =  0 ;		/*  null terminate string   */

  SET_POS( *p_buffer );

  *p_buffer  +=  ascii_len ;		/*  promote buffer position */
}
/* End of GetString() */



/* ========================================================================= *
 *                                                                           *
 *                              GetNothing                                   *
 *        Bypass stuff from buffer & promote buffer position		         *
 *                                                                           *
 * ========================================================================= *
 */
void GetNothing (char ** p_buffer,	int ascii_len)
{
  SET_POS( *p_buffer );

  *p_buffer  +=  ascii_len ;		/*  promote buffer position */
}
/* End of GetNothing() */


// CHECK_JSON_ERROR - Added 20Apr2021 for JSON support.
void CHECK_JSON_ERROR ()
{
	char	ErrorText	[500];

	switch (Global_JSON_Error)
	{
		// DonR 06Jun2021: Added new status JSON_NO_DATA_NOT_MANDATORY (= 1); if this status is returned, the tag was
		// not found but we do *not* want to return an error.
		case JSON_NO_ERROR:
		case JSON_NO_DATA_NOT_MANDATORY:	return;

		case JSON_PARENT_NULL:				sprintf (ErrorText, "NULL parent JSON object for tag '%s'.", Global_JSON_Tag);
											break;

		case JSON_TAG_NOT_SUPPLIED:			sprintf (ErrorText, "JSON tag %s.", (Global_JSON_Tag == NULL) ? "is NULL" : "has length = 0");
											break;


		case JSON_MISSING_VALUE_POINTER:	sprintf (ErrorText, "Missing destination pointer for JSON tag '%s'.", Global_JSON_Tag);
											break;


		case JSON_MAX_LENGTH_NEGATIVE:		sprintf (ErrorText, "Negative maximum length given for JSON tag '%s'.", Global_JSON_Tag);
											break;


		case JSON_TAG_NOT_FOUND:			sprintf (ErrorText, "JSON tag '%s' was not found in parent object.", Global_JSON_Tag);
											break;


		case JSON_VALUE_TYPE_MISMATCH:		sprintf (ErrorText, "Unexpected value type in JSON tag '%s'.", Global_JSON_Tag);
											break;


		default:							sprintf (ErrorText, "Unrecognized JSON error %d for tag '%s'.", Global_JSON_Error, Global_JSON_Tag);
											break;
	}

	// Save any non-zero error code in glbErrorCode - which should be initialized to zero.
	glbErrorCode = Global_JSON_Error;

	GerrLogMini (GerrId, ErrorText);

	return;
}



/* ========================================================================= *
 *                                                                           *
 *                              PrintErr                                     *
 *         Close open files & Print error code  into output file             *
 *                                                                           *
 * ========================================================================= *
 */
int PrintErr(
	      char		*output_buf,
	      int		errcode,
	      int		msgcode
	      )
{
    char	temp[4096];
    int		len;

    len = sprintf(
		  temp,
		  "%0*d%0*d",
		  MSG_ID_LEN,
		  msgcode,
		  MSG_ERROR_CODE_LEN,
		  errcode
		  );

    memcpy(
	   output_buf,
	   temp,
	   len
	   );

    return( len );

}
/* End of PrintErr() */



/* ========================================================================= *
 *                                                                           *
 *                              ChckInputLen                                 *
 *        Verify data file does not contain garbage or is too small          *
 *                                                                           *
 * ========================================================================= */
void ChckInputLen (int diff     /* = [total-input] - [processed input] */)
{
	if (diff  <  0)
	{
		SET_ERROR (ERR_FILE_TOO_SHORT) ;
	}
	else if (diff  >  0)
	{
		SET_ERROR (ERR_FILE_TOO_LONG) ;
	}
}
/* End of ChckInputLen() */


/* ========================================================================= *
 *                                                                           *
 *                              ChckInputLenAllowTrailingSpaces              *
 *        Verify data file does not contain garbage or is too small          *
 *        Note that this version does *not* throw an error if the buffer	 *
 *        is over-length but has only spaces in it - which makes it easier	 *
 *        to "replay" transactions.											 *
 *                                                                           *
 * ========================================================================= */
void ChckInputLenAllowTrailingSpaces (char *buff_in, int diff /* = [total-input] - [processed input] */ )
{
	if (diff  <  0)
	{
		SET_ERROR (ERR_FILE_TOO_SHORT);
	}
	else
	{
		if (diff  >  0)
		{
			// If all the remaining buffer consists of spaces, strspn() will return
			// a value equal to the left-over buffer length - and in this case we
			// will *not* return an error.
			if (diff > strspn (buff_in, " "))
			{
				SET_ERROR (ERR_FILE_TOO_LONG);
			}
		}
	}
}
// End of ChckInputLenAllowTrailingSpaces()


/* ========================================================================= *
 *                                                                           *
 *                                 ChckDate                                  *
 *                          Verify a legal date exp.                         *
 *                                                                           *
 * ========================================================================= *
 */
void ChckDate (int		date	/*  format:  YYYYMMDD  */	)
{
	bool	FoundError	= false;
	int		day			= date % 100 ;
	int		month		= ( date / 100 ) % 100 ;
  

	// DonR 01Jan2010 HOT FIX: Changed illegal date boundary from 2010 to 2090.
	// DonR 06May2021 User Story #157425: Change minimum legal date from 19900000
	// to 19800000.
//	if (date	< 19900000	||
	if (date	< 19800000	||	// DonR 06May2021
		date	> 20900000	||
		day		> 31		||
		month	> 12			)
	{
		SET_ERROR (ERR_WRONG_FORMAT_FILE);
		FoundError = true;
	}
	else
	{
		/* date OK */
	}

  // DonR 13Aug2024: If we're returning an error, also provide a more
  // informative message to the program log.
  if (FoundError)
	  GerrLogMini (GerrId, "ChckDate returning invalid-value code %d on %d.", glbErrorCode, date);

}
/* End of ChckDate() */



/* ========================================================================= *
 *                                                                           *
 *                                 ChckTime                                  *
 *                          Verify a legal time exp.                         *
 *                                                                           *
 * ========================================================================= *
 */
void ChckTime (
	       int		time	/*  format:  HHNNSS  */
	       )
{
  bool	FoundError	= false;
  int	sec			= time % 100 ;
  int	min			= ( time / 100 ) % 100 ;
  
  
  if( time < 0  ||  time > 240000  ||  sec > 60  ||  min > 60 )
    {
		SET_ERROR( ERR_WRONG_FORMAT_FILE ) ;
		FoundError = true;
    }

  // DonR 13Aug2024: If we're returning an error, also provide a more
  // informative message to the program log.
  if (FoundError)
	  GerrLogMini (GerrId, "ChckTime returning invalid-value code %d on %d.", glbErrorCode, time);

}
/* End of ChckTime() */



/*=======================================================================
||																		||
||			    GetTblUpFlg()											||
||																		||
||		 Test if pharmacy needs to refresh a table						||
||																		||
|| Returns:																||
|| -------																||
|| MAC_TRUE = if pharmecy need to refresh table.						||
|| MAC_FALS = if pharmecy need not to refresh table.					||
 =======================================================================*/
int	GetTblUpFlg ( TDate	PharmecyDate,
				  char	*TableName		)
{
/*=======================================================================
||			    Local declarations										||
 =======================================================================*/
	ISOLATION_VAR;
	int	err_rtn = MAC_FALS;

	int		GTUFDataBaseModDate;
	int		GTUFDataBaseModTime;	// Just so we can use a standard ODBC procedure that already exists.
	char	TableNameArray [33];	// DonR 19Aug2020: Change from char * to array, since ODBC doesn't like string constants.


	REMEMBER_ISOLATION;
	SET_ISOLATION_DIRTY;

	/*=======================================================================
	||			    Test the table.											||
	 =======================================================================*/
	strcpy (TableNameArray, TableName);	// DonR 19Aug2020: Change from char * to array, since ODBC doesn't like string constants.

	ExecSQL (	MAIN_DB, READ_TablesUpdate, &GTUFDataBaseModDate, &GTUFDataBaseModTime, &TableNameArray, END_OF_ARG_LIST	);
    
	if (SQLERR_code_cmp (SQLERR_not_found) != MAC_TRUE)
	{
		if (!SQLERR_error_test ())
		{
			if (GTUFDataBaseModDate >= PharmecyDate)
			{
				err_rtn = MAC_TRUE;
			}
		}
	}

	RESTORE_ISOLATION;
	return (err_rtn);

} /* End of GetTblUpdFlage() */




/*=======================================================================
||																		||
||			      SetErrorVar_x()										||
||																		||
|| Set returned error code (error with highest severity), and returns a	||
|| flage if the current worst error is a severe one.					||
||																		||
|| DonR 11May2010: New version: handles error arrays for new			||
|| "Nihul Tikrot" transactions. This function should not be called		||
|| directly; instead, use the macros SetErrorVarInit, SetErrorVar		||
|| (which works like the old version), SetErrorVarArr,					||
|| SetErrorVarDelete, and SetErrorVarTell.								||
||																		||
||																		||
|| Returns:																||
|| -------																||
|| MAC_TRUE = if fatal error.											||
|| MAC_FALS = if not fatal error.										||
 =======================================================================*/

int	SetErrorVar_x (int			ActionCode_in,
				   short		*ErrorCode_Var,
				   short		Old_ErrorCode,
				   short		New_ErrorCode,
				   int			LargoCode_in,
				   short		LineNo_in,
				   short		BumpFlag_in,
				   TErrorArray	**ArrayAddr_out,
				   short		*FatalFlag_out,
				   int			*OverflowFlag_out)
{
	// Local declarations
    short			Prev_Sever;
    short			New_Sever	= 0;
    short			Test_Sever;
	int				retn_value	= 0;
	int				i;
	int				redundant;
	int				swap_made;
	int				ReportableErrors;

	// Array to hold all errors encountered.
	static TErrorArray	err_array [SET_ERR_MAX_ERRORS];
	static int			num_errors			= 0;
	static int			new_style_enabled	= 0;	// If non-zero, work in new "array mode".



	// If "new-style" error handling is not enabled, ensure that we'll work in the old style
	// even if the calling routine is using the "new-style" function call and supplying a
	// Largo Code associated with the error. Also, if "new-style" error handling IS enabled
	// and some sloppy programmer (== DonR <g>) has left in an old-style function call,
	// ensure that the error is added to the array (with Largo Code of zero).
	if ((ActionCode_in == SET_ERR_SET_NEW) && (!new_style_enabled))
	{
		ActionCode_in = SET_ERR_SET_OLD;
	}
	else
	{
		if ((ActionCode_in == SET_ERR_SET_OLD) && (new_style_enabled))
		{
			ActionCode_in	= SET_ERR_SET_NEW;
			LargoCode_in	= 0;
		}
	}


	// Switch on Action Code to see what we need to do.
	switch (ActionCode_in)
	{
		case SET_ERR_INIT:		num_errors = 0;			// No errors yet.
								new_style_enabled = 0;	// Default: "old-style" error processing.
								retn_value = 0;
								break;


		case SET_ERR_MODE_NEW:	// Note that this does NOT set the error count to zero - a call to
								// SetErrorVarInit (i.e. ActionCode_in = SET_ERR_INIT) is still
								// required.
								new_style_enabled = 1;	// Turn on "new-style" error processing.
								retn_value = 0;
								break;


		case SET_ERR_TELL:		if (ArrayAddr_out != NULL)
								{
									// It's possible, at least in theory, that someone might want
									// to know how many errors are stored without supplying an
									// array address to retrieve the actual list of errors.
									*ArrayAddr_out = err_array;
								}

								if (OverflowFlag_out != NULL)
								{
									for (i = ReportableErrors = 0; i < num_errors; i++)
									{
										if (err_array [i].ReportToPharm)
											ReportableErrors++;
									}

									*OverflowFlag_out = ReportableErrors;
								}

								retn_value = num_errors;
								break;


		case SET_ERR_TELL_MAX:	retn_value = SET_ERR_MAX_ERRORS;
								break;


		case SET_ERR_SET_NEW:	if (OverflowFlag_out != NULL)
									*OverflowFlag_out = MAC_FALS;	// Default = no array overflow.

								// If the new error code is zero, don't store it.
								if (New_ErrorCode != 0)
								{
									for (i = redundant = 0; i < num_errors; i++)
									{
										if ((err_array [i].LargoCode	== LargoCode_in) &&
											(err_array [i].Error		== New_ErrorCode))
										{
											redundant = 1;	// Error is already present for this Largo Code.
											New_Sever = err_array [i].Severity;
											break;
										}
									}	// Loop through existing errors.

									if (!redundant)	// Error is not already in array.
									{
										if (num_errors >= SET_ERR_MAX_ERRORS) // Pure paranoia - "=" should be enough.
										{
											if (OverflowFlag_out != NULL)
												*OverflowFlag_out = MAC_TRUE;
										}
										else
										{
											// Error is new, and there's room in the array.
											// DonR 24Jul2013: Add new ReportToPharm flag - always TRUE except for Severity 3,
											// which will be used to store "errors" that are only of interest inside Maccabi and
											// will not be sent to pharmacies.
											// DonR 25Feb2015: Add capability to send a given error code with severity taken
											// from a different error code. This can be useful, for example, when a new error
											// is initially in don't-report-to-pharmacy mode, and we then want to start
											// reporting it to pharmacies only in certain cases; so for those cases, we'll
											// send it with severity "stolen" from another error code, while leaving its
											// default severity unchanged. We accomplish this by assigning a value greater than
											// zero to Old_ErrorCode, which was not previously used in this section; if it does
											// have such a value, we'll use it to retrieve the severity level. The relevant
											// (new) macro is SetErrorVarArrAsIf.
											// DonR 16Mar2015: Add yet another wrinkle: A new macro, SetErrorVarArrSpecSeverity,
											// will allow the calling routine to specify a severity to override the severity
											// in the error table. To distinguish this from an "as if" severity set by
											// SetErrorVarArrAsIf, SetErrorVarArrSpecSeverity will pass the severity level
											// as a negative number which, obviously, must be "flipped" positive.
											err_array [num_errors].LargoCode		= LargoCode_in;
											err_array [num_errors].LineNo			= LineNo_in;
											err_array [num_errors].Error			= New_ErrorCode;

											err_array [num_errors].Severity			= New_Sever =
												(Old_ErrorCode < 0)	? (0 - Old_ErrorCode)	// Flip negative severity to positive.
																	: GET_ERROR_SEVERITY ((Old_ErrorCode > 0) ? Old_ErrorCode : New_ErrorCode);	// DonR 25Feb2015

											err_array [num_errors].ReportToPharm	= (New_Sever != 3);
											num_errors++;
										}
									}	// Error code/Largo Code combination not found in array.
								}	// New error code is non-zero.
								else
								{
									New_Sever = 0;
								}

								// NO BREAK! New-style error processing includes the functionality of
								// old-style error processing (with one important difference - see below),
								// so we just "fall through" to the next "case".


		case SET_ERR_SET_OLD:	// Get severity of previous and and new error. Test_Sever will have either
								// the old or the new severity, whichever is greater.
								// DonR 11Jul2011: Avoid null-pointer (segmentation) violations.
								if (ErrorCode_Var == NULL)	// Should never really happen!
								{
									retn_value = -1;
								}
								else
								{
									Test_Sever = Prev_Sever	= GET_ERROR_SEVERITY (*ErrorCode_Var);

									// DonR 06Jun2013: If we've already stored the new severity in our array, don't bother
									// looking it up again.
									if (ActionCode_in == SET_ERR_SET_OLD)
										New_Sever = GET_ERROR_SEVERITY (New_ErrorCode);

									// DonR 21Oct2007: We are now using Severity 0 for some non-critical
									// informational messages, such as telling the pharmacy that a
									// member's ishur will expire in the near future. In these cases, we
									// need to know that a severity level of zero *with* an error code
									// takes priority over the initial state of grace in which both
									// severity and error code are zero.
									// DonR 19May2013: Added a new feature: "BumpFlag". For non-array use (i.e.
									// Pharmacy Transaction 2003), this allows an error to "bump" a previous
									// error even though they are of equal severity.
									if (( New_Sever			>  Prev_Sever)													||
										((New_Sever			== Prev_Sever)	&& (New_ErrorCode > 0)	&& (BumpFlag_in	!= 0))	||
										((*ErrorCode_Var	== 0)			&& (New_ErrorCode > 0)	&& (Prev_Sever	== 0)))
									{
										// DonR 24Jul2013: New feature (or hack, if you prefer): Severity 3 is for special
										// "errors" that are of interest only to Maccabi, and are never to be sent to
										// pharmacies. Accordingly, they are stored in err_array[] and writtent to
										// prescription_msgs, but are otherwise ignored.
										if (New_Sever != 3)
										{
											*ErrorCode_Var	= New_ErrorCode;
										}	// New severity is not equal to 3.

										Test_Sever = New_Sever;	// Used in the bit of logic below.
									}

									if (Test_Sever >= FATAL_ERROR_LIMIT)
									{
										retn_value = MAC_TRUE;

										// DonR 18Jul2011: If calling routine has provided a fatal-flag address
										// and the new error is fatal, set the flag TRUE.
										if (FatalFlag_out != NULL)
											*FatalFlag_out = retn_value;
									}
									else
									{
										retn_value = MAC_FALS;
									}
								}	// Valid error-code address.

								break;


								// DonR 28Feb2011: If no Largo Code was specified, delete the
								// error for all drugs (and for non-drug-specific errors).
								// Since the error may occur more than once, embed the for loop
								// inside a do-while loop.
		case SET_ERR_DELETE:	do
								{
									for (i = retn_value = 0; i < num_errors; i++)
									{
										if (((err_array [i].LargoCode	== LargoCode_in) || (LargoCode_in == 0)) &&
											( err_array [i].Error		== Old_ErrorCode))
										{
											retn_value = 1;	// Error is present for this Largo Code.
											num_errors--;	// Array will shrink by one.
										}

										// If error was found and it wasn't the last thing in the array,
										// "bubble" later stuff forward. (Note that num_errors in the
										// "if" below has already been reduced by one!
										if ((retn_value != 0) && (i < num_errors))
										{
											err_array [i].LargoCode		= err_array [i + 1].LargoCode;
											err_array [i].LineNo		= err_array [i + 1].LineNo;
											err_array [i].Error			= err_array [i + 1].Error;
											err_array [i].Severity		= err_array [i + 1].Severity;
											err_array [i].ReportToPharm	= err_array [i + 1].ReportToPharm;
										}
									}	// Loop through existing errors.
								}
								while ((retn_value != 0) && (LargoCode_in == 0));	// Normally drops through first time.

								break;


								// DonR 11Jul2011: Add Substitute capability. Note that if the "new" error
								// code is the same as the old one, this amounts to a test to see if a
								// particular error code has been set for a particular drug.
		case SET_ERR_SWAP:		for (i = swap_made = 0; i < num_errors; i++)
								{
									if (((err_array [i].LargoCode	== LargoCode_in) || (LargoCode_in == 0)) &&
										( err_array [i].Error		== Old_ErrorCode))
									{
										swap_made = 1;	// At least one substitution was made (or at least the error was found).

										if (New_ErrorCode != Old_ErrorCode)	// If we're actually making a substitution.
										{
											err_array [i].Error			= New_ErrorCode;
											err_array [i].Severity		= New_Sever = GET_ERROR_SEVERITY (New_ErrorCode);
											err_array [i].ReportToPharm	= (New_Sever != 3);
										}
										else	// If all  we're doing is checking for an error code's existence, stop
												// looking once we've found a single instance.
										{
											break;
										}
									}
								}	// Loop through existing errors.

								// DonR 23Apr2014: If all we're doing is testing for the existence of an error code,
								// (A) we don't need to worry about all the severity stuff; and (B) we want to return
								// a 1 or a 0 based on the existence of the error, *not* based on severity.
								if  ((New_ErrorCode == Old_ErrorCode)	&&
									 (ErrorCode_Var == NULL)			&&
									 (FatalFlag_out == NULL))
								{
									retn_value = swap_made;
									break;
								}

								// If the new error is more severe than the previous worst error, swap it into
								// the error variable.
								if (ErrorCode_Var == NULL)	// Should never really happen!
								{
									retn_value = -1;
								}
								else
								{
									Test_Sever = Prev_Sever	= GET_ERROR_SEVERITY (*ErrorCode_Var);

									// If no swap was made, then there is no "new" error - so treat
									// the situation as if the new error and its severity were both zero.
									// This way we don't store anything new in the return error-code slot.
									if (!swap_made)
									{
										New_Sever = New_ErrorCode = 0;
									}
//									else
//									{
//										New_Sever = err_array [i].Severity;
//									}

									// DonR 21Oct2007: We are now using Severity 0 for some non-critical
									// informational messages, such as telling the pharmacy that a
									// member's ishur will expire in the near future. In these cases, we
									// need to know that a severity level of zero *with* an error code
									// takes priority over the initial state of grace in which both
									// severity and error code are zero.
									if ((New_Sever > Prev_Sever) ||
										((*ErrorCode_Var == 0) && (Prev_Sever == 0) && (New_ErrorCode > 0)))
									{
										if (New_Sever != 3)
											*ErrorCode_Var	= New_ErrorCode;	// Severity 3 is not reported to pharmacies or stored in error_code variable.
										Test_Sever		= New_Sever;	// Used in the bit of logic below.
									}

									if (Test_Sever >= FATAL_ERROR_LIMIT)
									{
										retn_value = MAC_TRUE;

										// DonR 18Jul2011: If calling routine has provided a fatal-flag address
										// and the new error is fatal, set the flag TRUE.
										if (FatalFlag_out != NULL)
											*FatalFlag_out = retn_value;
									}
									else
									{
										retn_value = MAC_FALS;
									}
								}

								break;


		case SET_ERR_FINDTOP:	// For a given Largo Code, or for all Largo Codes, find the most severe error
								// and store it in an integer pointer. Like the "set error" functions, returns
								// TRUE or FALSE based on whether a "fatal" error has been found.
								// (Note that this assumes we've been using the array-version when setting errors;
								// if someone calls SetErrorVarTop() after using only the non-array version, this
								// routine will zero-out the error variable even if an error had been set!)
								if (ErrorCode_Var == NULL)	// Should never really happen.
								{
									retn_value = -1;
								}
								else
								{
									Prev_Sever		= -1;	// So even a zero-severity error will be more severe.
									*ErrorCode_Var	= 0;	// In case we don't find any errors.

									for (i = retn_value = 0; i < num_errors; i++)
									{
										if ((err_array [i].LargoCode == LargoCode_in) || (LargoCode_in == 0))
										{
											// Severity 3 is not reported to pharmacies or stored in error_code variable.
	 										Test_Sever = (err_array [i].Severity == 3) ? -1 : err_array [i].Severity;											

											if (Test_Sever > Prev_Sever)
											{
												*ErrorCode_Var	= err_array [i].Error;
												Prev_Sever		= Test_Sever;

												if (Test_Sever >= FATAL_ERROR_LIMIT)
												{
													retn_value = MAC_TRUE;
												}
											}	// Current severity is greater than previous severity.
										}	// Current array position is relevant.
									}	// Loop through error array.

									// DonR 18Jul2011: If calling routine has provided a fatal-flag address,
									// store the fatal-flag value in it. (Note that in this case, unlike
									// setting the error variable, we want to store the flag value even if
									// it's FALSE!)
									if (FatalFlag_out != NULL)
										*FatalFlag_out = retn_value;

								}	// Return-value pointer is non-NULL.

								break;


		default:				retn_value = -1;
								break;	// Should never happen!

	}	// switch (ActionCode_in)


	return (retn_value);
}	// End of SetErrorVar_x ().


/*===========================================================================
||																			||
||			   IS_PHARMACY_OPEN_X() - new version							||
||																			||
|| Test if pharmacy is open (and authorized for the current transaction)	||
|| & returns info.															||
||																			||
|| This version reads from the database table rather than from shared		||
|| memory, and gets a fuller set of information than the old version.		||
||																			||
 ===========================================================================*/
TErrorCode IS_PHARMACY_OPEN_X (int				v_PharmNum,
							   short			v_InstituteCode_in,
							   PHARMACY_INFO	*Phrm_info_out,
							   short			TransactionNumber_in,
							   short			Spool_in)
{
	// Local variables
	TErrorCode				err		= MAC_OK;
	int						reStart	= MAC_TRUE;
	int						tries;
	short					i;
	PHARM_TRN_PERMISSIONS	TrnPermission;
	short					v_InstituteCode		= v_InstituteCode_in;	// Need to keep the input value because we compare it to what's in the table.
	PHARMACY_INFO			Phrm_info;
	ISOLATION_VAR;



	// Body of function
	REMEMBER_ISOLATION;
	SET_ISOLATION_DIRTY;
	memset (&Phrm_info,		0, sizeof (Phrm_info));
	memset (&TrnPermission,	0, sizeof (TrnPermission));

	// DonR 18Jan2022: Just so we have a place we always know we can find the Pharmacy Code,
	// added it to the pharmacy_info structure.
	Phrm_info.pharmacy_code = v_PharmNum;

//	GerrLogMini (GerrId, "IS_PHARMACY_OPEN_X: Pharm %d, Inst %d, Trn_num %d, Spool %d.", v_PharmNum, v_InstituteCode_in, TransactionNumber_in, Spool_in);
//
//	if ((TransactionNumber_in != CurrentTransactionNumber) || (Spool_in != CurrentSpoolFlag))
//	{
//		TransactionNumber_in	= CurrentTransactionNumber;
//		Spool_in				= CurrentSpoolFlag;
//		GerrLogMini (GerrId, "IS_PHARMACY_OPEN_X: Reset Transaction Number / Spool to %d/%d!", TransactionNumber_in, Spool_in);
//	}
//

	// DonR 28Jan2019: Find the relevant pharmacy permissions for the current Transaction Code.
	for (i = 0; i < 200; i++)
	{
		if ((TrnPermissions[i].transaction_num == TransactionNumber_in) && (TrnPermissions[i].spool == Spool_in))
		{
			TrnPermission = TrnPermissions[i];
			break;
		}
		else
		{
			if (TrnPermissions[i].transaction_num < 1)	// In other words, we got to the end of the array without a match.
			{
				// If we didn't find the current transaction in the array, assume it's enabled for all pharmacy types.
				// DonR 14Aug2024 User Story #349368: Also disable Transcaction Version Number control for a "missing"
				// transaction.
				TrnPermission.permitted[0]			=
				TrnPermission.permitted[1]			=
				TrnPermission.permitted[2]			= 1;	// Default = all permitted.
				TrnPermission.MinVersionPermitted	= 0;	// Default = no minimum Transaction Version Number.
				break;
			}	// End of array with no match.
		}	// Not a match.
	}	// Loop through permissions array.
//	GerrLogMini (GerrId, "Last value of i = %d, found transaction %d while looking for %d spool %d.", i, TrnPermission.transaction_num, TransactionNumber_in, Spool_in);


	for (tries = 0; tries < SQL_UPDATE_RETRIES && reStart == MAC_TRUE; tries++)
	{
		reStart = MAC_FALS;	// So anything other than a deadlock error will end the loop.

		do
		{
			// DonR 10Jan2019 "Mersham Pituach": Add logic to WHERE clause to exclude non-hesder
			// pharmacies for those transactions that should be accessible only to pharmacies
			// with "hesder Maccabi".
			// DonR 09May2021: Since institued_code is not part of the unique index (and in fact is always
			// equal to 2), deleted it from the WHERE criteria and added it to the list of columns
			// SELECTed. This way we can check whether a pharmacy sent an incorrect value.
			// DonR 29Jan2024 User Story #540234: Read CannabisPharmacy/can_sell_cannabis flag.
			ExecSQL (	MAIN_DB, IsPharmOpen_READ_pharmacy,
						&Phrm_info.pharmacy_permission	,	&Phrm_info.price_list_num		,
						&Phrm_info.vat_exempt			,	&Phrm_info.open_close_flg		,
						&Phrm_info.pharm_category		,	&Phrm_info.pharm_card			,
						&Phrm_info.leumi_permission		,	&Phrm_info.CreditPharm			,
						&Phrm_info.maccabicare_pharm	,	&Phrm_info.meishar_enabled		,

						&Phrm_info.order_originator		,	&Phrm_info.order_fulfiller		,
						&Phrm_info.can_sell_future_rx	,	&Phrm_info.hesder_category		,
						&Phrm_info.web_pharmacy_code	,	&Phrm_info.owner				,
						&v_InstituteCode				,	&Phrm_info.ConsignmentPharmacy	,
						&Phrm_info.PharmNohalEnabled	,	&Phrm_info.can_sell_cannabis	,

						&Phrm_info.mahoz				,	&Phrm_info.software_house		,

						&v_PharmNum						,	&ROW_NOT_DELETED				,
						END_OF_ARG_LIST															);
//GerrLogMini (GerrId, "IsPharmOpen_READ_pharmacy: Pharmacy %d/Inst. %d, Permission %d, hesder %d, owner %d, web pharm %d, SQLCODE = %d.",
//	v_PharmNum, v_InstituteCode, Phrm_info.pharmacy_permission, Phrm_info.hesder_category, Phrm_info.owner, Phrm_info.web_pharmacy_code, SQLCODE);

			Conflict_Test (reStart); // Note that this version of Conflict_Test() includes the wait before retrying!

			// DonR 09May2021: Since we took institued_code out of the WHERE clause, simulate a
			// not-found error if the pharmacy supplied a non-zero value that's different from
			// what we found in the pharmacy table.
			if ((SQLERR_code_cmp (SQLERR_not_found) == MAC_TRUE)	||
				((v_InstituteCode_in > 0) && (v_InstituteCode_in != v_InstituteCode)))
			{
				err = ERR_PHARMACY_NOT_FOUND;
				break;
			}

			// Note that because some calling routines look specifically for ERR_PHARMACY_NOT_FOUND as an
			// indication of a problem, we return this code even if there was some other form of database
			// error. Since it's very unlikely that we'll hit other errors for a table that (A) we're
			// reading in "dirty" mode, and (B) is seldom updated, this shouldn't cause any problems.
			if (SQLERR_error_test ())
			{
				GerrLogMini (GerrId, "IS_PHARMACY_OPEN: DB error %d reading Pharmacy #%d.", SQLCODE, v_PharmNum);
				err = ERR_PHARMACY_NOT_FOUND;
				break;
			}

			else
			// Successful read - process what we read from database.
			{
				// DonR 09May2021 "Chanut Virtualit": If we're in a transaction where the pharmacy doesn't
				// supply an Institution Code, we want to use what we read from the database - so copy
				// it into a new slot in the pharmacy_info structure.
				Phrm_info.institute_code = v_InstituteCode;

				// "Predigest" permission data - just to save a few clock cycles.
				//
				// DonR 20Apr2023 User Story #432608: Change criteria for pharmacy types, and add a new
				// pharmacy type for "Maccabi (Hishtatfut) Type". The changes are as follows:
				// 1) Maccabi_pharmacy is now based on "owner == 0" instead of Permission Type - so it
				//    includes only "genuine" Maccabi pharmacies.
				// 2) Private_pharmacy is now based on "owner != 0" instead of Permission Type.
				// 3) As a result of these two changes, non-hesder pharmacies are now "officially"
				//    private as opposed to Maccabi - but this should be irrelevant, since these pharmacies
				//    are allowed to sell stuff only via x005-spooled transactions, with no calculation of
				//    participation or all the other normal logic.
				// 4) There is a new ConsignmentPharmacy flag that's read directly from the database.
				// 5) There is a new MaccabiPtnPharmacy flag that's set based on "permission == 1"; it's
				//    basically the same as maccabi_pharmacy used to be, but it will include the "consignatzia"
				//    pharmacies which will also have permission == 1. This flag will be used for "hishtatfut"
				//    and anything else where we're interested in the "hishtatfut" and related calculations,
				//    rather than the legal/administrative differences between Maccabi and private pharmacies.
				//
				// Note that "consignatzia" pharmacies (mostly or entirely SuperPharm) are specially designated
				// for areas where there isn't a local Maccabi pharmacy, and thus they get to sell "consignment"
				// items that are normally available only at Maccabi pharmacies, and in fact still belong to
				// Maccabi (and thus the "consignment"). As of the new changes, these pharmacies will sell all
				// items at "Maccabi" participation; but on non-consignment items, they can still sell
				// prescription medications at 100% participation. The exception to this is actual consignment
				// items, since in this case the pharmacy is actually selling on behalf of Maccabi and is subject
				// to the same regulations that apply to Maccabi pharmacies.
//				Phrm_info.maccabi_pharmacy		= (Phrm_info.pharmacy_permission	== PERMISSION_TYPE_MACABI);
//				Phrm_info.private_pharmacy		= (Phrm_info.pharmacy_permission	!= PERMISSION_TYPE_MACABI);	    
				Phrm_info.maccabi_pharmacy		= (Phrm_info.owner					== 0);
				Phrm_info.private_pharmacy		= (Phrm_info.owner					!= 0);

				Phrm_info.prati_plus_pharmacy	= (Phrm_info.pharmacy_permission	== PERMISSION_TYPE_PRATI_PLUS);
				Phrm_info.MaccabiPtnPharmacy	= (Phrm_info.pharmacy_permission	== PERMISSION_TYPE_MACABI);

				Phrm_info.has_hesder_maccabi	= (Phrm_info.hesder_category		!= NON_HESDER_PHARMACY);


				// If this pharmacy does not have "hesder Maccabi", the future-sale-permitted flag
				// should already be FALSE - but just in case, force in a FALSE value.
				if (!Phrm_info.has_hesder_maccabi)
					Phrm_info.can_sell_future_rx = 0;

				// DonR 28Jan2019 CR #27234: In case hesder_category has a strange value, we default to 1
				// (private w/hesder, the most common value) for pharmacy code < 100000, and 0 (Maccabi
				// pharmacy) for pharmacies from 100000 and up. This shouldn't ever be necessary, but this
				// way we avoid subscripting errors if AS/400 sends us bad values for hesder_category.
				if ((Phrm_info.hesder_category < 0) || (Phrm_info.hesder_category > 2))
					Phrm_info.hesder_category = (v_PharmNum > 99999) ? 0 : 1;

				// DonR 28Jan2019 CR #27234: If this pharmacy is not authorized to use the current
				// transaction, give an error.
				if (!TrnPermission.permitted[Phrm_info.hesder_category])
				{
					// DonR 01Jul2020: Moved this message to a separate log file.
					GerrLogFnameMini ("hesder_log", GerrId, "Pharmacy %d hesder_category = %d, permitted[%d] = for trn %d = %d.",
									  v_PharmNum, Phrm_info.hesder_category, Phrm_info.hesder_category, TransactionNumber_in, TrnPermission.permitted[Phrm_info.hesder_category]);
					err = ERR_INCONSISTENT_TRANSACTIONS;
					break;
//					FORCE_NOT_FOUND;	// Or else maybe Error 6129/ERR_INCONSISTENT_TRANSACTIONS?
				}
				else
				{
					// DonR 14Aug2024 User Story #349368: If Transaction Version Number validation has
					// been enabled for the current transaction, verify that the version number sent by
					// the pharmacy is at least equal to the minimum version permitted.
					if (VersionNumber < TrnPermission.MinVersionPermitted)
					{
GerrLogMini (GerrId, "Is_pharmacy_open: Transaction %d version %d is less than minimum %d.",
	TransactionNumber_in, VersionNumber, TrnPermission.MinVersionPermitted);
						err = ERR_OBSOLETE_TRANSACTION_VERSION;
					}
					else
					{
						// DonR 19Feb2019: If we get here, the pharmacy was found in the database and is
						// authorized to execute the current transaction code - even if it hasn't yet performed
						// an open-shift. Accordingly, set the new found_and_legal flag; if this flag remains
						// FALSE, the pharmacy isn't authorized to run the transaction at all, and shouldn't
						// even see additional error messages (e.g. for Transaction 6003, which can report
						// multiple errors).
						Phrm_info.found_and_legal = 1;
					}	// No problem with pharmacy authorization for transaction OR with Transaction Version Number.
				}	// No problem with pharmacy authorization for transaction.


				if (Phrm_info.open_close_flg != PHARM_OPEN)
				{
					err = ERR_PHARMACY_NOT_OPEN;
				}

				// DonR 29Jan2024 User Story #540234: Process CannabisPharmacy/can_sell_cannabis flag.
				// If the sysparams-fed parameter CannabisPermitPerPharmacy is TRUE, we just use the
				// value we got from the pharmacy table; if CannabisPermitPerPharmacy is FALSE, we
				// force in a can_sell_cannabis value of FALSE for Maccabi pharmacies, and TRUE for
				// all private pharmacies. (NOTE: When we set can_sell_cannabis here, we're using a
				// boolean TRUE value based on inverting the "is a Maccabi pharmacy" flag. So code
				// should just use this as a boolean, *not* test for specific or positive values!)
				if (!CannabisPermitPerPharmacy)
				{
					Phrm_info.can_sell_cannabis = !Phrm_info.maccabi_pharmacy;
				}
				// Else just leave the variable with whatever value we read from the table.

				// DonR 09Sep2020: Read additional parameters from the new pharmacy_type_params table.
				// DonR 19Jun2025 User Story #417785 (new transaction 6011): Added 3 new parameters to
				// control prior-sales/ishurim download to pharmacies.
				// DonR 06Jan2026: Added 3 new type-specific date ranges for opioids, ADHD drugs, and
				// everything else ("ordinary" items). Prior_sale_download_days is now a derived value,
				// the maximum of these 3 new columns.
				ExecSQL (	MAIN_DB, IsPharmOpen_READ_pharmacy_type_params,
							&Phrm_info.max_order_sit_days						,	&Phrm_info.max_order_work_hrs					,
							&Phrm_info.maxdailybuy								,	&Phrm_info.kill_nocard_svc_minutes				,
							&Phrm_info.pharm_type_meishar_enabled				,	&Phrm_info.allow_order_narcotic					,
							&Phrm_info.allow_order_preparation					,	&Phrm_info.allow_full_price_order				,
							&Phrm_info.allow_refrigerated_delivery				,	&Phrm_info.max_price_for_pickup					,

							&Phrm_info.select_late_rx_days_buying_in_person		,	&Phrm_info.select_late_rx_days_ordering_online	,
							&Phrm_info.select_late_rx_days_filling_online_order	,	&Phrm_info.allow_online_order_days_before_expiry,
							&Phrm_info.usage_memory_mons						,	&Phrm_info.prior_sale_download_type				,
							&Phrm_info.enable_ishur_download					,	&Phrm_info.prior_sale_opioid_days				,
							&Phrm_info.prior_sale_ADHD_days						,	&Phrm_info.prior_sale_ordinary_days				,

							&Phrm_info.pharmacy_permission						,	&Phrm_info.owner								,
							&Phrm_info.web_pharmacy_code						,	END_OF_ARG_LIST										);

				if (SQLCODE != 0)
				{
					if (SQLERR_code_cmp (SQLERR_not_found) == MAC_TRUE)
					{
						GerrLogMini (GerrId, "Nothing found in pharmacy_type_params for Pharmacy #%d:\nPermission Type %d / Owner %d / Web Pharmacy Code %d - using defaults.",
									 v_PharmNum, Phrm_info.pharmacy_permission, Phrm_info.owner, Phrm_info.web_pharmacy_code);
					}
					else
					{
						GerrLogMini (GerrId, "Error %d reading pharmacy_type_params for Pharmacy #%d:\nPermission Type %d / Owner %d / Web Pharmacy Code %d - using defaults.",
									 SQLCODE, v_PharmNum, Phrm_info.pharmacy_permission, Phrm_info.owner, Phrm_info.web_pharmacy_code);
						SQLERR_error_test ();
					}

					switch (Phrm_info.pharmacy_permission)
					{
						case 0:		// Ordinary private pharmacies.
									Phrm_info.max_order_sit_days			= OnlineOrderWaitDays;		// Global default from sysparams.
									Phrm_info.max_order_work_hrs			= OnlineOrderMaxWorkHours;	// Global default from sysparams.
									Phrm_info.maxdailybuy					=  0;
									Phrm_info.kill_nocard_svc_minutes		= 15;
									Phrm_info.pharm_type_meishar_enabled	=  0;
									break;

						case 1:		// For some reason, private non-hesder pharmacies are being given
									// Permission Type 1, which is supposed to be for Maccabi pharmacies.
									Phrm_info.max_order_sit_days			= OnlineOrderWaitDays;		// Global default from sysparams.
									Phrm_info.max_order_work_hrs			= OnlineOrderMaxWorkHours;	// Global default from sysparams.
									Phrm_info.maxdailybuy					= (Phrm_info.owner == 0) ? 1000000 : 0;
									Phrm_info.kill_nocard_svc_minutes		= (Phrm_info.owner == 0) ? -1 : 15;
									Phrm_info.pharm_type_meishar_enabled	= (Phrm_info.owner == 0) ?  1 :  0;
									break;

						case 6:		// Prati Plus pharmacies.
									Phrm_info.max_order_sit_days			= OnlineOrderWaitDays;		// Global default from sysparams.
									Phrm_info.max_order_work_hrs			= OnlineOrderMaxWorkHours;	// Global default from sysparams.
									Phrm_info.maxdailybuy					=  0;
									Phrm_info.kill_nocard_svc_minutes		= 15;
									Phrm_info.pharm_type_meishar_enabled	=  1;
									break;

						default:	// Should never happen - but if it does, use the private-pharmacy defaults.
									Phrm_info.max_order_sit_days			= OnlineOrderWaitDays;		// Global default from sysparams.
									Phrm_info.max_order_work_hrs			= OnlineOrderMaxWorkHours;	// Global default from sysparams.
									Phrm_info.maxdailybuy					=  0;
									Phrm_info.kill_nocard_svc_minutes		= 15;
									Phrm_info.pharm_type_meishar_enabled	=  0;
									break;
					}	// Switch to provide defaults because we couldn't read from pharmacy_type_params.
				}	// Read from pharmacy_type_params failed.


				// DonR 26Nov2025 User Story #251264: Get required Hebrew encoding,
				// if any, from new table transaction_hebrew_encoding. Possible
				// encoding values are:
				//		D =	DOS Hebrew
				//		U =	UTF-8 Hebrew
				//		W =	Win-1255 Hebrew
				//		N =	No translation - send as-is. Set this as the default
				//			if nothing is found in the table.
				// Note that the default implies that we will send Windows-1255 Hebrew in
				// any transaction where the new logic is implemented, since that's normally
				// what we get from AS/400; if we want to send anything different, we must
				// add the relevant rows to transaction_hebrew_encoding!
				Phrm_info.hebrew_encoding = 'N';

				ExecSQL (	MAIN_DB, IsPharmOpen_READ_hebrew_encoding,
							&Phrm_info.hebrew_encoding	,
							&TransactionNumber_in		,	&Phrm_info.software_house,
							END_OF_ARG_LIST													);

				if (SQLCODE != 0)
				{
					if (SQLERR_code_cmp (SQLERR_not_found) == MAC_TRUE)
					{
						Phrm_info.hebrew_encoding = 'N';	// Shouldn't really be necessary.
					}
					else
					{
						GerrLogMini (	GerrId, "Error %d reading transaction_hebrew_encoding for Transaction %d Software House %d (Pharmacy %d).",
										SQLCODE, TransactionNumber_in, Phrm_info.software_house, v_PharmNum);
						SQLERR_error_test ();
					}
				}	// Any error code reading from transaction_hebrew_encoding.

			}	// Successful read of pharmacy table.

		}
		while (0);	// One-time loop to avoid GOTO's.

	}	// Retry loop.

	if (reStart == MAC_TRUE)	// Maximum number of retries exceeded - throw an error.
	{
		GerrLogMini (GerrId, "IS_PHARMACY_OPEN: %d retries reading Pharmacy #%d - giving up!", tries, v_PharmNum);
		err = ERR_PHARMACY_NOT_FOUND;
	}


	RESTORE_ISOLATION;

	if (Phrm_info_out != NULL)
		*Phrm_info_out = Phrm_info;

	return (err);
}
// End of IS_PHARMACY_OPEN_X ()



/*=======================================================================
||																		||
||			   IS_DRUG_INTERACAT()										||
||																		||
|| Test if two drugs have interaction.									||
||																		||
|| Returns:																||
|| 0 - interaction code need not to be change.							||
|| 1 - if drugs have interaction & interaction > previus code			||
|| 2 = deadlock, caller should restart transaction.						||
|| 3 = unpredictable error occured - return  error to caller.			||
 =======================================================================*/
int	IS_DRUG_INTERACAT (TDrugCode		drugA,
					   TDrugCode		drugB,
					   TInteractionType	*interaction,
					   TErrorCode		*ErrorCode )
{
	/*=======================================================================
	||			    Local declarations				||
	 =======================================================================*/
	ISOLATION_VAR;

	short					inter_type;
	TErrorCode				err;
	int						FirstLargo;
	int						SecondLargo;

	/*=======================================================================
	||				Body of function										||
	 =======================================================================*/
//GerrLogMini (GerrId, "IS_DRUG_INTERACAT: drugA = %d, drugB = %d.", drugA, drugB);


	// Avoid DB deadlocks reading interaction table.
	REMEMBER_ISOLATION;
	SET_ISOLATION_DIRTY;


	// Decide which largo code comes first in the SELECT.
	if (drugA < drugB)
	{
		FirstLargo	= drugA;
		SecondLargo	= drugB;
	}

	else
	{
		FirstLargo	= drugB;
		SecondLargo	= drugA;
    }

	// DonR 16Jul2015: For some reason, since around 16 June we seem to be "losing" interactions.
	// I'm not sure why this is, but as a possible solution, let's try doing a "real" database
	// lookup if we didn't find anything in shared memory.
	// DonR 19Jul2015: Turns out that GET_INTERACATION() wasn't returning any error at all - 
	// but it also wasn't finding interactions. Instead of trying to fix it, I've decided
	// to just work from the database. Later, I'll add logic to avoid the lookup if the drug
	// in question isn't in drug_interactions at all.
	inter_type = 0;	// DonR 20Jul2015: So we don't get bogus values.
					// (Should be redundant, since we reset it to zero if there's a DB error.)

	ExecSQL (	MAIN_DB, IsDrugInteract_READ_drug_interaction, &inter_type, &FirstLargo, &SecondLargo, END_OF_ARG_LIST	);

	if (SQLCODE == 0)
	{
		err = NO_ERROR;	// We found an interaction from the table even though we didn't see it in shared memory.
	}
	else
	{
		inter_type = 0;				// DonR 20Jul2015: So we don't get bogus values.

		if (SQLERR_code_cmp (SQLERR_not_found) == MAC_TRUE)
		{
			err = ERR_KEY_NOT_FOUND;	// Not-found is an OK result.
		}
		else
		{
			err = ERR_DATABASE_ERROR;	// Something other than OK or not-found.
		}
	}	// SQLCODE is non-zero.
//GerrLogMini (GerrId, "IS_DRUG_INTERACAT: SQLCODE = %d, err = %d, inter_type = %d.", SQLCODE, err, inter_type);

	// Restore default (committed read) isolation.
	RESTORE_ISOLATION;

    if ((err != NO_ERROR) && (err != ERR_KEY_NOT_FOUND))
    {
		*ErrorCode = err;
		return (1);
    }

    *interaction = inter_type;

    return (0);
}
/* End of IS_DRUG_INTERACAT() */



// DonR 26Dec2016: Add function to check for Doctor Interaction Ishurim.
int CheckForInteractionIshur (int				MemberTZ_in,
							  short				MemberTzCode_in,
							  TPresDrugs		*SaleDrug_1_in,
							  TPresDrugs		*SaleDrug_2_in,
							  TDocRx			*DocRxArray_in,
							  DURDrugsInBlood	*BloodDrug,
							  short				*DocApproved_out,
							  short				*InteractionType_in_out)
{
	ISOLATION_VAR;

	short		Rx;
	int			SelStrLen;

	int				MemberID		= MemberTZ_in;
	int				MemberIDCode	= MemberTzCode_in;
	int				FirstLargo		= SaleDrug_1_in->DrugCode;
	int				SecondLargo;
	int				DocID_1			= SaleDrug_1_in->DocID;
	int				DocID_2;
	int				EP_Group_1		= SaleDrug_1_in->DL.economypri_group;
	int				EP_Group_2		= 0;	// No real default value.
	char			DocPrIDs_1		[2000];
	char			DocPrIDs_2		[2000];
	int				RowsFound;
	char			SelectString	[5500];
//	char			DiagBuf	[6000];


	// This routine should be fully supported for Transactions 2003, 5003, and 6003. Of these,
	// 2003 is mostly out of use as of the end of 2016; 5003 is the principle transaction
	// being used in the real world; and 6003 is in use in parts of the country, and will
	// supplant 5003 over time. Transaction 1063 still exists, but is no longer really
	// supported. Rather than amend the transaction to work properly with this routine,
	// we will simply return if DocRxArray_in is NULL.
	if (DocRxArray_in == NULL)
		return(0);


	// Calling routine should supply the first drug in the interaction, which is the "primary"
	// drug for the test, and the second drug which can be either another drug in the current
	// sale request, or a previously purchased drug. Which one to look at is controlled by
	// which of the two pointers (SaleDrug_2_in or BloodDrug) is NULL. Further, we want to
	// check for an interaction ishur based on *either* drug in current sale request; but an
	// ishur written for a drug that was previously purchased is *not* relevant.

	// Note that the Doctor Prescription ID logic may be changed to work with Visit Date.
	// Because Visit Date is *not* one of the values that the pharmacy sends in Transaction
	// 6003 (and thus is not part of the TDocRx structure), we will need to populate the
	// relevant list(s) by executing a SELECT DISTINCT query on the doctor_presc table - 
	// not the end of the world, but a bit of additional coding. The retrieved values will
	// be stored in renamed versions of DocPrIDs_1 and DocPrIDs_2, and then we just have
	// to adjust the WHERE logic to test against visit_date rather than doc_pr_id. (We
	// may also want to fiddle with doc_interact_ishur indexes...)

	// Build lists of relevant Doctor Prescription ID's to be incorporated in SELECT string.
	// First, create a default "find nothing" version. This will be overwritten if there are any
	// real Doctor Prescription ID's to look for.
	strcpy (DocPrIDs_1, "-999999");
	strcpy (DocPrIDs_2, "-999999");

	// Note that for Transactions 2003 and 5003, NumDocRxes will be either zero or one; only
	// with Transaction 6003 did we add the ability to combine multiple doctor prescription
	// ID's in a single sale request drug-line. If there are no doctor prescription ID's
	// associated with a drug in the sale request, the "for Rx" loop won't do anything and
	// the default "select nothing" value of -999999 will remain.
	for (Rx = SelStrLen = 0; Rx < SaleDrug_1_in->NumDocRxes; Rx++)
	{
		// Don't add bogus Doctor Prescription ID's to the list - only values > 0 are of interest.
//		SelStrLen += sprintf (DocPrIDs_1 + SelStrLen, "%s%d", (Rx > 0) ? "," : "", DocRxArray_in [SaleDrug_1_in->FirstRx + Rx].PrID);
		if (DocRxArray_in [SaleDrug_1_in->FirstRx + Rx].PrID > 0)
			SelStrLen += sprintf (DocPrIDs_1 + SelStrLen, "%s%d", (SelStrLen > 0) ? "," : "", DocRxArray_in [SaleDrug_1_in->FirstRx + Rx].PrID);
	}

	// Array of Doctor Prescription ID's for the second drug is relevant only if this is
	// a drug in the current sale request; otherwise (or if there are no doctor prescription
	// ID's for this drug line) we leave the default value of -999999 in place.
	// DonR 19Apr2017: We need to search for ishurim based not only on the actual Largo Codes
	// sold, but also on drugs in the same Economypri groups.
	if (SaleDrug_2_in != NULL)	// Second drug in interaction is part of the current sale request.
	{
		SecondLargo	= SaleDrug_2_in->DrugCode;
		DocID_2		= SaleDrug_2_in->DocID;	// Since one sale request in Trn. 6003 can involve multiple doctors.
		EP_Group_2	= SaleDrug_2_in->DL.economypri_group;

		for (Rx = SelStrLen = 0; Rx < SaleDrug_2_in->NumDocRxes; Rx++)
		{
			// Don't add bogus Doctor Prescription ID's to the list - only values > 0 are of interest.
//			SelStrLen += sprintf (DocPrIDs_2 + SelStrLen, "%s%d", (Rx > 0) ? "," : "", DocRxArray_in [SaleDrug_2_in->FirstRx + Rx].PrID);
			if (DocRxArray_in [SaleDrug_2_in->FirstRx + Rx].PrID > 0)
				SelStrLen += sprintf (DocPrIDs_2 + SelStrLen, "%s%d", (SelStrLen > 0) ? "," : "", DocRxArray_in [SaleDrug_2_in->FirstRx + Rx].PrID);
		}
	}
	else	// Second drug in interaction is from a previous drug sale.
	{
		if (BloodDrug != NULL)
			SecondLargo = BloodDrug->Code;
		else
			SecondLargo = -999;

		// Ishurim from doctors in previous sales are *not* relevant. This means that we
		// don't have to worry about finding the previously-sold drug's Economypri Group Code,
		// since the lookup (with negative Doctor ID) will fail anyway.
		DocID_2 = EP_Group_2 = -999999;
	}	// Second drug in interaction is from a previous drug sale.

	// DonR 19Apr2017: If either drug sold has an Economypri Group Code of zero, set the
	// variable to a value that's guaranteed not to retrieve anything.
	if (EP_Group_1 < 1)
		EP_Group_1 = -999;
	if (EP_Group_2 < 1)
		EP_Group_2 = -999;

	// Now build the SELECT string.
	// NOTE: It would be nice, in theory, to parameterize this query, PREPARE it
	// once, and then simply invoke it with EXECUTE... USING every time we need it.
	// The problem is that we don't know how many Doctor Prescription ID's will be
	// in the two "IN" lists, and there is no elegant way to PREPARE in advance
	// without knowing this piece of information. Hmph.
	//
	// DonR 13Feb2017: Revised version of the Interaction Ishur lookup. This version
	// works by Visit Date rather than directly by Prescription ID; the difference is
	// that with the new version, an ishur will work if it was issued on the same day
	// as one of the relevant prescriptions, even if the actual Doctor Prescription ID
	// is different. Because the pharmacy doesn't send Visit Date, we need to do a
	// subquery lookup to get the list of relevant Visit Dates from the Doctor
	// Prescriptions table.
	sprintf (SelectString,
			 "SELECT	COUNT(*)					"
			 "FROM		doc_interact_ishur			"
			 "WHERE		member_id		=  %d		"

			 "  AND	(		(	(contractor		=   %d)		AND					"
			 "					((largo_code_1	=   %d) OR (largo_code_1 IN (SELECT largo_code from drug_list WHERE economypri_group = %d)))	AND	"
			 "					((largo_code_2	=   %d) OR (largo_code_2 IN (SELECT largo_code from drug_list WHERE economypri_group = %d)))	AND	"
			 "					(visit_date		IN								"
			 "						(SELECT DISTINCT visit_date					"
			 "						 FROM			 doctor_presc				"
			 "						 WHERE			 member_id			=  %d	"
			 "						   AND			 doctor_presc_id	IN (%s)	"
			 "						   AND			 doctor_id			=  %d	"
			 "						   AND			 member_id_code		=  %d	"
			 "						)											"
			 "					)												"
			 "				)													"

			 "			OR	(	(contractor		=   %d)		AND					"
			 "					((largo_code_1	=   %d) OR (largo_code_1 IN (SELECT largo_code from drug_list WHERE economypri_group = %d)))	AND	"
			 "					((largo_code_2	=   %d) OR (largo_code_2 IN (SELECT largo_code from drug_list WHERE economypri_group = %d)))	AND	"
			 "					(visit_date		IN								"
			 "						(SELECT DISTINCT visit_date					"
			 "						 FROM			 doctor_presc				"
			 "						 WHERE			 member_id			=  %d	"
			 "						   AND			 doctor_presc_id	IN (%s)	"
			 "						   AND			 doctor_id			=  %d	"
			 "						   AND			 member_id_code		=  %d	"
			 "						)											"
			 "					)												"
			 "				)													"
			 "		)															"

			 "  AND	member_id_code	=  %d										",

			 MemberID,

			 DocID_1,
			 FirstLargo,
			 EP_Group_1,
			 SecondLargo,
			 EP_Group_2,
			 MemberID,
			 DocPrIDs_1,
			 DocID_1,
			 MemberIDCode,

			 DocID_2,
			 SecondLargo,
			 EP_Group_2,
			 FirstLargo,
			 EP_Group_1,
			 MemberID,
 			 DocPrIDs_2,
			 DocID_2,
			 MemberIDCode,

			 MemberIDCode);

	// FOR DIAGNOSIS ONLY!
//	sprintf (DiagBuf,
//			 "SELECT COUNT(*) "
//			 "FROM	doc_interact_ishur "
//			 "WHERE	... AND			 doctor_presc_id IN (%s)	"
//			 "		... AND			 doctor_presc_id IN (%s) ...",
//			 DocPrIDs_1, DocPrIDs_2);

	REMEMBER_ISOLATION;
	SET_ISOLATION_DIRTY;

	ExecSQL (	MAIN_DB, CheckForInteractionIshur_READ_IshurCount, &SelectString, &RowsFound, END_OF_ARG_LIST	);

//if (!TikrotProductionMode) GerrLogMini (GerrId, "CheckForInteractionIshur: PREPARE of [%s] gave SQLCODE %d", (SQLCODE == 0) ? DiagBuf : SelectString, SQLCODE);

//if (!TikrotProductionMode) GerrLogMini (GerrId, "CheckForInteractionIshur: EXECUTE gave RowsFound = %d, SQLCODE %d", RowsFound, SQLCODE);

	// Interpret results.
	if (SQLCODE == 0)
	{
		// The Doctor Approved Interaction flag will be used to send a special message to pharmacy.
		if (DocApproved_out != NULL)
			*DocApproved_out = (RowsFound > 0) ? 1 : 0;

		// DonR 15Mar2017: At least for now, a Doctor Interaction Ishur will *not* downgrade an interaction's severity.
		// If the doctor approved the interaction *and* it was initially flagged as severe, downgrade it.
		InteractionType_in_out = NULL; // DonR 15Mar2017 - just a rather clumsy way of disabling the interaction-severity downgrade.
		if (InteractionType_in_out != NULL)
		{
			if ((*InteractionType_in_out == SEVER_INTERACTION) && (RowsFound > 0))
				*InteractionType_in_out = NOT_SEVER_INTERACTION;
		}
	}
	else
	{
		// DonR 12Mar2020: If something went wrong, print the diagnostics but don't abort anything -
		// just assume that the interaction was *not* authorized and proceed.
		SQLERR_error_test ();

		if (DocApproved_out != NULL)
			*DocApproved_out = 0;
	}


	RESTORE_ISOLATION;

	return(0);

}



/*=======================================================================
||																		||
||			   TransactionRestart()										||
||																		||
|| Begin rollback SQL transaction & write to log.						||
||																		||
 =======================================================================*/
int	TransactionRestart( void )
{
	/*=======================================================================
	||				Body of function			||
	=======================================================================*/
	RollbackAllWork ();

	return (SQLERR_error_test ());
}
/* End of TransactionRestart() */



/*=======================================================================
||																		||
||			   get_drugs_in_blood()										||
||																		||
|| Get the drugs in members blood.										||
|| RETURNs:																||
|| 0 = OK.																||
|| 1 = high severity error occured OR deadlock.							||
========================================================================*/
int	get_drugs_in_blood (TMemberData				*Member,
						TDrugsBuf				*drugbuf,
						short					CallType_in,
						TErrorCode				*ErrorCode,
						int						*Restart_out)
{
	/*===================================================================
	||			    Local declarations									||
	====================================================================*/
	ISOLATION_VAR;
	int				err_rtn;
	int				time_minutes;

	// Static variables to detect whether we're calling to retrieve data that's already
	// in the buffer.
	static int		last_member_id	= 0;
	static short	last_id_code	= 0;
	static short	last_call_type	= -1;
	static int		last_date		= 0;
	static int		last_time_min	= 0;

	int							v_member_id		= Member->ID;
	short						v_id_code		= Member->ID_code;
	int							v_drug;
	int							DL_drug			= -1;
	int							v_StopBloodDate;
	int							v_stop_date;
	int							v_StartDate;
	int							v_PurchaseTime;
	int							v_PharmacyCode;
	int							v_Units;
	int							v_op;
	int							v_Duration;	// DonR 05Apr2020: Made an int to agree with the database, even though it really should be a short (as it was before).
	short						v_Source;
	int							v_doctor;
	short						v_doctortype;
	int							v_presc_id;
	short int					v_line_no;
	short int					v_t_half;
	short						v_del_flg;
	int							v_price_replace;
	short int					v_maccabi_price;
	short int					v_mem_price_code;
	short int					v_particip_method;
	int							v_SpecialPrescNum;	// DonR: Added 25Nov2012.
	short int					v_GetAllDrugsBot;
	int							v_SysDate;
	int							v_SelectDate;
	short						DELIVERED = DRUG_DELIVERED;
	short						v_mem_id_code_read;

	// DonR 12Feb2006: Ingredients-sold fields.
	short int					v_ingr_1_code;
	short int					v_ingr_2_code;
	short int					v_ingr_3_code;
	double						v_ingr_1_quant;
	double						v_ingr_2_quant;
	double						v_ingr_3_quant;
	double						v_ingr_1_quant_std;
	double						v_ingr_2_quant_std;
	double						v_ingr_3_quant_std;

	// DonR 29Jun2006: Fields from drug_list for purchase-limit
	// checking.
	int							v_package_size;
	short						v_purch_limit_flg;
	int							v_qty_lim_grp_code;
	short						v_class_code;
	short						PharmacologyGroup;
	short						TreatmentType;
	int							EconomypriGroup;
	char						LargoType [1 + 1];
	short						chronic_flag;
	short						ShapeCode;
	int							ParentGroupCode;
	short						in_drug_interact;



	// NOTE: the declare-cursor-only-once logic caused this function to fail (on Linux 4 only,
	// for some reason!); so don't re-implement it unless you have some bright idea as to why
	// it failed here in the first place.  -DonR 07Apr2011


	/*===================================================================
	||				Body of function									||
	 ===================================================================*/
	REMEMBER_ISOLATION;

	// DonR 16Feb2006: If we need to retrieve all drugs purchased (to test
	// Special Prescription limits), set flag to disable stop_blood_date
	// select.
	if (CallType_in == GET_ALL_DRUGS_BOUGHT)
		v_GetAllDrugsBot = 1;
	else
		v_GetAllDrugsBot = 0;

	// DonR 24Jul2011: We don't want to read purchase history for Member ID of zero; in order to keep things
	// simple, suppress the SELECT by setting Member ID to the bogus value of -999.
	if (v_member_id == 0)
		v_member_id = -999;


	do	// Dummy loop to avoid multiple exit points.
	{
		// Alloc buffer if needed
		// DonR 15Feb2006: Allocate 20 slots at a time, not 10.
		// DonR 25May2010: Make it 50 - we're retaining more data these days!
		// DonR 05Apr2020: Up it to 100 - retention is even longer nowadays.
		if (drugbuf->size < 1)
		{
			drugbuf->Durgs = (DURDrugsInBlood *)
							 MemAllocReturn (GerrId,
											 sizeof(DURDrugsInBlood) * 100,
											 "Drugs in member blood");

			if (drugbuf->Durgs == (DURDrugsInBlood*)NULL)
			{
				*ErrorCode = ERR_NO_MEMORY;
				err_rtn = 1;
				break;
			}  

			drugbuf->size = 100;
			drugbuf->max  = 0;
		}


		// Get Data - first initialize a few things.
		v_SysDate		= GetDate ();
		time_minutes	= GetTime () / 100;	// Strip off seconds.

		// DonR 07Feb2011: Another trick to speed up the SELECT - use a date that's conditionally
		// set to zero rather than testing CallType in the SELECT statement.
		v_SelectDate	= (CallType_in == GET_ALL_DRUGS_BOUGHT) ? 0 : v_SysDate;

		// See if this is a redundant function call - if so, just return, and if not, make sure
		// the buffer is logically empty (by setting its current size to zero).
		// Note that we don't test for equality in the "get all drugs bought" flag; if the
		// current call is to retrieve just drugs in blood and we've already retrieved all drugs,
		// then we can use the buffer we've already loaded.
		if ((drugbuf->max		>  0				) &&
			(v_member_id		== last_member_id	) &&
			(v_id_code			== last_id_code		) &&
			(v_GetAllDrugsBot	<= last_call_type	) &&
			(v_SysDate			== last_date		) &&
			(time_minutes		== last_time_min	))
		{
			// Buffer is already loaded with the relevant information - just return!
			err_rtn = 0;
			break;
		}

		// If we get here, this is not a repeat call - so we actually have to do something.
		// Remember the new input parameters (and time and date), and logically clear the drug buffer.
		last_member_id	= v_member_id;
		last_id_code	= v_id_code;
		last_call_type	= v_GetAllDrugsBot;
		last_date		= v_SysDate;
		last_time_min	= time_minutes;
		drugbuf->max	= 0;

		gettimeofday (&EventTime[EVENT_HIST_START], 0);

		// We're actually going to do something, so set isolation "dirty".
		SET_ISOLATION_DIRTY;

		// Declare cursor only once we know we actually want to open it. Also, read drug_list separately
		// to improve performance.
		// DonR 21May2014: change SELECT criteria (and index) to improve performance.
		DeclareAndOpenCursorInto (	MAIN_DB, GetDrugsInBlood_blood_drugs_cur,
									&v_drug,					&v_stop_date,					&v_Duration,
									&v_Units,					&v_op,							&v_StartDate,
									&v_PurchaseTime,			&v_PharmacyCode,				&v_Source,
									&v_doctor,					&v_doctortype,					&v_presc_id,

									&v_line_no,					&v_t_half,						&v_StopBloodDate,
									&v_price_replace,			&v_maccabi_price,				&v_mem_price_code,
									&v_particip_method,			&v_ingr_1_code,					&v_ingr_2_code,
									&v_ingr_3_code,				&v_ingr_1_quant,				&v_ingr_2_quant,

									&v_ingr_3_quant,			&v_ingr_1_quant_std,			&v_ingr_2_quant_std,
									&v_ingr_3_quant_std,		&v_SpecialPrescNum,				&v_mem_id_code_read,
									&v_del_flg,

									&v_member_id,				&DELIVERED,						&v_SelectDate,
									END_OF_ARG_LIST																		);

		if (SQLERR_error_test ())
		{
			*ErrorCode = ERR_DATABASE_ERROR;
			CloseCursor (	MAIN_DB, GetDrugsInBlood_blood_drugs_cur	);
			err_rtn			= 1;
			break;
		}

		// Fetch data and store in buffer.
		for ( ; ; )
		{
			FetchCursor (	MAIN_DB, GetDrugsInBlood_blood_drugs_cur	);

			if (SQLERR_code_cmp (SQLERR_access_conflict) == MAC_TRUE)
			{
				LOG_CONFLICT ();
				err_rtn			= 1;		// DonR 01Sep2005: Indicate access conflict by
				*Restart_out	= MAC_TRUE;	// setting Restart variable, not by using separate
											// return code.
				break;
			}

			if (SQLERR_code_cmp (SQLERR_end_of_fetch) == MAC_TRUE)
			{
				err_rtn = 0;
				break;
			}

			else
			{
				if (SQLERR_error_test ())
				{
					*ErrorCode = ERR_DATABASE_ERROR;
					err_rtn			= 1;
					break;
				}
			}

			// DonR 17Aug2010: For better performance, moved the selection by Delivered Flag and Deleted Flag
			// to after the SQL SELECT, since they're not part of the relevant index. At least in theory,
			// this should enable the routine to run faster even though more prescription_drugs rows will
			// actually be read. (Note that about 1 in 6 prescription_drugs rows are normally non-delivered,
			// and about 1 in 1000 are deleted.)
			// DonR 21May2014: Moved selection by Delivered Flag to the actual SQL SELECT statement (and
			// changed index on prescription_drugs to support this), and moved the selection by
			// Member ID Code here.
			// DonR 19Dec2019: Got rid of the Old Member ID logic - it hasn't been used basically forever.
			if ((v_del_flg			!= ROW_NOT_DELETED) || (v_mem_id_code_read != v_id_code))
			{
				continue;
			}


			// Reallocate buffer if needed
			if (drugbuf->max >= drugbuf->size)
			{
				drugbuf->Durgs = (DURDrugsInBlood *)
								 MemReallocReturn (GerrId,
												   (void*)drugbuf->Durgs,
												   sizeof(DURDrugsInBlood) * (drugbuf->size += 50),
												   "Drugs in member blood" );

				if (drugbuf->Durgs == (DURDrugsInBlood*)NULL)
				{
					*ErrorCode = ERR_NO_MEMORY;
					err_rtn = 1;
					break;
				}
			}

			// DonR 07Apr2011: If necessary, read in fields from drug_list. Prescription_drugs rows
			// are now ORDER'ed BY largo_code in order to minimize drug_list access and speed things up.
			// DonR 02Nov2014: Added Economypri Group to drugs-in-blood structure.
			// DonR 24Sep2019: To make code simpler elsewhere, store chronic_flag as 1 or 0;
			// if the value in drug_list is 2 (or anything else), consider that a zero for
			// all intents and purposes.
			if (v_drug != DL_drug)
			{
				DL_drug = v_drug;

				ExecSQL (	MAIN_DB, GetDrugsInBlood_READ_drug_list,
							&v_package_size,		&v_purch_limit_flg,		&v_qty_lim_grp_code,
							&v_class_code,			&PharmacologyGroup,		&EconomypriGroup,
							&LargoType,				&TreatmentType,			&in_drug_interact,
							&chronic_flag,			&ShapeCode,				&ParentGroupCode,
							&v_drug,				END_OF_ARG_LIST									);

				// No real error-trapping - if the drug wasn't found, just skip this drug-sale.
				if (SQLCODE != 0)
					continue;
			}

			// Put data in buffer
			// 1) Prescription Drugs and drug_list.
			drugbuf->Durgs[drugbuf->max].Code				= v_drug;
			drugbuf->Durgs[drugbuf->max].StopDate			= v_stop_date;
			drugbuf->Durgs[drugbuf->max].StartDate			= v_StartDate;
			drugbuf->Durgs[drugbuf->max].PurchaseTime		= v_PurchaseTime;
			drugbuf->Durgs[drugbuf->max].Duration			= v_Duration;
			drugbuf->Durgs[drugbuf->max].Units				= v_Units;
			drugbuf->Durgs[drugbuf->max].Op					= v_op;
			drugbuf->Durgs[drugbuf->max].Source				= v_Source;
			drugbuf->Durgs[drugbuf->max].DoctorId			= v_doctor;
			drugbuf->Durgs[drugbuf->max].DoctorIdType		= v_doctortype;
			drugbuf->Durgs[drugbuf->max].PrescriptionID		= v_presc_id;
			drugbuf->Durgs[drugbuf->max].LineNo				= v_line_no;
			drugbuf->Durgs[drugbuf->max].THalf				= v_t_half;
			drugbuf->Durgs[drugbuf->max].StopBloodDate		= v_StopBloodDate;
			drugbuf->Durgs[drugbuf->max].SpecialPrescNum	= v_SpecialPrescNum;	// DonR: Added 25Nov2012.
			drugbuf->Durgs[drugbuf->max].PharmacologyGroup	= PharmacologyGroup;	// DonR: Added 30Mar2014.
			drugbuf->Durgs[drugbuf->max].TreatmentType		= TreatmentType;		// DonR: Added 06Apr2014.
			drugbuf->Durgs[drugbuf->max].EconomypriGroup	= EconomypriGroup;		// DonR: Added 02Nov2014.
			drugbuf->Durgs[drugbuf->max].PharmacyCode		= v_PharmacyCode;		// DonR: Added 10Nov2014.
			drugbuf->Durgs[drugbuf->max].chronic_flag		= chronic_flag;			// DonR: Added 16Sep2019.
			drugbuf->Durgs[drugbuf->max].ShapeCode			= ShapeCode;			// DonR: Added 04Dec2023.
			drugbuf->Durgs[drugbuf->max].parent_group_code	= ParentGroupCode;		// DonR: Added 28Feb2024.
			drugbuf->Durgs[drugbuf->max].in_drug_interact	= in_drug_interact;		// DonR: Added 25Mar2020.
			strcpy (drugbuf->Durgs[drugbuf->max].LargoType,	  LargoType);			// DonR: Added 02Nov2014.


			// 2) Figure out if this drug was bought at discount, so we
			// know if it counts for purchase limit checking.
			// DonR 07Apr2013: Per Iris Shaya, drugs that got their participation from an AS/400 ishur are considered
			// "discounted" even if they were bought at full price. (This happens, for example, when they are subject
			// to an "ishur with tikra".)
			if (((v_price_replace > 0) && (v_maccabi_price == 0))		||
				((v_particip_method % 10) == FROM_SPEC_PRESCS_TBL))	// DonR 07Apr2013
			{
				// Member got a fixed price other than Maccabi Price.
				drugbuf->Durgs[drugbuf->max].BoughtAtDiscount = 1;
			}
			else
			{
				// If member paid 100% and didn't get a "real" discounted
				// fixed price, this drug was not bought at discount.
				// DonR 30Jan2022 WORKINGPOINT: Maybe rather than hard-coding a list of full-price
				// price codes, we should reference a new flag in the member_price table and do a
				// table lookup here? That would help avoid bugs as the list of price codes changes
				// over time.
				switch (v_mem_price_code)
				{
					case  1:
					case 11:
					case 19:
					case 50:
					case 97:
					case 98:
								drugbuf->Durgs[drugbuf->max].BoughtAtDiscount = 0;
								break;

					// Any other price code indicates some kind of discount.
					default:
								drugbuf->Durgs[drugbuf->max].BoughtAtDiscount = 1;
								break;
				}	// Switch on member_price_code.
			}	// Member didn't get a discounted fixed price or participation from an AS/400 ishur.

			// 3) Sold Ingredients.
			// DonR 07Jul2013: The new "standardized" ingredient-usage fields (ingr_N_quant_std) will contain the
			// same information as the old fields (ingr_N_quant_bot), except that grams and micrograms will be
			// converted to milligrams, and milli-IU will be converted to IU. Existing drug-sale information does
			// not have the new fields filled in; while this will be done by manual SQL for those ingredients
			// that need to be converted to grams/IU, for the remainder we'll just use the old field as a default.
			// Once all relevant history has the new fields filled (say in mid-2014), we can start just using the
			// new fields and ignore the old ones.
			drugbuf->Durgs[drugbuf->max].Ingredient[0].code		= v_ingr_1_code;
			drugbuf->Durgs[drugbuf->max].Ingredient[1].code		= v_ingr_2_code;
			drugbuf->Durgs[drugbuf->max].Ingredient[2].code		= v_ingr_3_code;
			drugbuf->Durgs[drugbuf->max].Ingredient[0].quantity	= (v_ingr_1_quant_std > 0.0) ? v_ingr_1_quant_std : v_ingr_1_quant;
			drugbuf->Durgs[drugbuf->max].Ingredient[1].quantity	= (v_ingr_2_quant_std > 0.0) ? v_ingr_2_quant_std : v_ingr_2_quant;
			drugbuf->Durgs[drugbuf->max].Ingredient[2].quantity	= (v_ingr_3_quant_std > 0.0) ? v_ingr_3_quant_std : v_ingr_3_quant;

			// 4) Drug List.
			drugbuf->Durgs[drugbuf->max].TotalUnits				=   (v_op * v_package_size)
																  + v_Units;
			drugbuf->Durgs[drugbuf->max].PurchaseLimitFlag		= v_purch_limit_flg;
			drugbuf->Durgs[drugbuf->max].PurchaseLimitGroupCode	= v_qty_lim_grp_code;
			drugbuf->Durgs[drugbuf->max].ClassCode				= v_class_code;		// DonR 26May2010: "Stickim".

			drugbuf->max++;
		}	// End of cursor-reading loop.

		CloseCursor (	MAIN_DB, GetDrugsInBlood_blood_drugs_cur	);

		if (SQLERR_error_test ())
		{
			*ErrorCode = ERR_DATABASE_ERROR;
			err_rtn = 1;
			break;
		}
	}
	while (0);


	RESTORE_ISOLATION;

	// Note that if we do a drugs-in-blood-only call followed by a get-all-drugs call
	// for the same sale request, the time taken by the first call will be overwritten.
	gettimeofday (&EventTime[EVENT_HIST_END], 0);

	return (err_rtn);
}
// End of get_drugs_in_blood ()



/*=======================================================================
||																		||
||			test_special_prescription ()								||
||																		||
|| Test special prescription drugs & quantities / period.				||
|| RETURNs:																||
|| 0 = OK.																||
|| 1 = high severity error occurred.									||
|| 2 = deadlock, caller should restart transaction.						||
 =======================================================================*/
/* - if dont have number parametrs this is type = 1 must receive number
   - const sum to pay exist > 0 must do calculation
   - other way must have participating from special prescriptions
   - for all drug must take in_helth_pack flag for decision in 1003
     */

// NOTE: Once Trn. 1063/1065 is removed, the only possible value for TypeCheckSpecPr
// is 1 - which means (A) that it can be removed as a parameter, and (B) that all
// code below that deals with any value other than 1 can be deleted.

int	test_special_prescription (TNumofSpecialConfirmation		*confnum,
							   TSpecialConfirmationNumSource	*confnumsrc,
							   TMemberData						*Member,
							   int								overseas_member_flag,
							   TTypeDoctorIdentification		DocIdType_in,
							   TDoctorIdentification			DocId_in,
							   TPresDrugs						*presdrugs,
							   TPresDrugs						*DrugArrayPtr,
							   int								NumDrugs,
							   TDrugCode						AlternateLargo_in,
							   TErrorCode						*ErrorCode,
							   short							TypeCheckSpecPr,
							   TPharmNum						v_PharmNum,
							   TPermissionType					v_MaccabiPharmacy,
							   TParticipatingType				v_ParticipatingType,
							   int								MaxExpiredDays_in,
							   short							*IshurWithTikra_out)
{
	// Local declarations.
	int				dose_in_units;
	int				cur_interval;
	int				total_units_allowed;
	int				prv_interval;
	int				UnitsBought;
	int				UnitsPerRefill;
	int				FullPurchasePrice;
	int				IshurFixedPrice;
	int				FlgNotFound = 0; //20020301
	int				SqlResult;
	int				ReStart;
	int				DrugNum;
	int				Ingr;
	int				CycleNum;
	int				CycleStartDate;
	int				IshurPharmError		= 0;
	short			GaveQtyLimitError	= 0;	// DonR 15Sep2020 CR #30106.
	ISOLATION_VAR;

	int				DrugCode			= presdrugs->DrugCode;
	int				AlternateLargo		= AlternateLargo_in;
	int				SelectDrugCode;
	int				MemberId			= Member->ID;
	short			IdExt				= Member->ID_code;
	int				StopUseDate;
	int				SystemDate;
	int				TreatmentStart;
	int				TestDate;
	short			MemberPart;
	int				ConfNum;
	short			ConfNumSrc;
	short			ishur_source_send;	// DonR 01May2012
	int				IshurPharmNum;
	short			SP_TreatmentType;
	short			v_DocIdType			= DocIdType_in;
	int				v_DocID				= DocId_in;
	short			v_insurance_used;
	short			v_in_health_pack_sp	= 0;
	short			AbruptCloseFlg;
	short			PrivPharmSaleOk;
	int				ConstSumToPay;
	int				rows_found;
	short			v_tikra_flag		= 0;
	short			v_IshurTikraType	= 0;
	short			LineNo;
	short			needs_29_g;

	// DonR 19Feb2006: New Quantity Limits for Ishurim.
	short			v_QtyLim_ingr;
	short			v_QtyLim_flg;
	short			v_QtyLim_course_len;
	double			v_QtyLim_all_ishur;
	double			v_QtyLim_all_warn;
	double			v_QtyLim_all_err;
	double			v_QtyLim_course_lim;
	double			v_QtyLim_course_wrn;
	double			v_QtyLim_course_err;
	double			v_QtyLim_per_day;
	double			QLM_all_ishur_std;
	double			QLM_all_warn_std;
	double			QLM_all_err_std;
	double			QLM_course_lim_std;
	double			QLM_course_wrn_std;
	double			QLM_course_err_std;
	double			QLM_per_day_std;
	short			QLM_treat_days;
	int				v_QtyLim_hist_start;
	double			QtyLimAllBought		= 0.0;
	double			AdjQtyLimAllBought	= 0.0;
	double			NumCourses			= 1.0;
	double			v_AdjLimIngrUsage;
	int				v_LookupPrID;
//	int				ParentGroupCode;				// DonR 31Mar2020: May need to re-enable for Production.
	short			v_LookupLineNo;
	int				FirstFewDays;
	int				SixCyclesStart;
	int				ThreeMonthsEnabled		= 0;
	int				AllIshur100Pct			= 0;
	int				AllIshur115Pct			= 0;
	int				AllIshur130Pct			= 0;
	int				CurrentCourse100Pct		= 0;
	int				AnyCourse100Pct			= 0;
	int				AnyCourse115Pct			= 0;
	int				AnyCourse130Pct			= 0;
	int				AdjAllIshur100Pct		= 0;
	int				AdjAllIshur115Pct		= 0;
	int				AdjAllIshur130Pct		= 0;
	int				AdjCurrentCourse100Pct	= 0;
	int				AdjAnyCourse100Pct		= 0;
	int				AdjAnyCourse115Pct		= 0;
	int				AdjAnyCourse130Pct		= 0;
//	short			ForceAdjQLErrors		= 0;	// DonR 31Mar2020: May need to re-enable for Production.
	double			QtyLimCourseBought		[5];	// Marianna 06Aug2020 CR #27955 add Cycle 4 for monthly limit.
	double			AdjQtyLimCourseBought	[5];	// Marianna 06Aug2020 CR #27955 add Cycle 4 for monthly limit.
	double			TestLimit				[5];	// Marianna 06Aug2020 CR #27955 add Cycle 4 for monthly limit.
	double			ManotPerCycle			[4];	// NOTE: ManotPerCycle is *not* used for Cycle 4 (the new monthly limit),
														// so we don't need to change the size of this array.
	int				CourseStart				[5];	// Marianna 06Aug2020 CR #27955 add Cycle 4 for monthly limit.

	// Marianna 06Aug2020 CR #27955
	short 			monthly_qlm_flag;
	double			monthly_dosage;
	double			monthly_dosage_std;
	short			monthly_duration;
	int				FirstFewDaysMonthly;			// DonR 05Apr2021 Feature #128927.
	double			NumCoursesMonthly		= 1.0;	// DonR 05Apr2021 Feature #128927.
	int				MonthlyLimit			= 0;
	int				AdjMonthlyLimit			= 0;
			
	// Marianna 30Jul2020 CR #28605
	int 			PrevIshurTreatmentStartDate;	
	short 			cont_approval_flag;
	int 			orig_confirm_num;
	short			ExceptionalIshur;


	// Define cursor for selecting most current Special Prescription for
	// a given Member and Drug.
	// DonR 13Nov2008: Switch to new health-basket field.
	// DonR 07Jul2011: Added logic to test stop_use_date, in order to ensure that we "see" a current ishur
	// for the generic drug even if there is an expired ishur for the non-generic drug. (That is, we want
	// the expired, non-extendable ishur to be rejected by the WHERE clause.)
	// DonR 12Sep2012: Ishurim with Member Price Code of zero are invalid - ignore them!
	// DonR 25Nov2012: Added nominal per-course limit to DB read, so we can compute fictitious "all-ishur"
	// limits for "forever" ishurim.
	// DonR 07Jul2013: Read new Quantity Limit History Start Date, as well as "standardized" Quantity
	// Limit values which will override the non-standardized ones.


	// Initialization.
	REMEMBER_ISOLATION;
	SET_ISOLATION_DIRTY;

	// DonR 07Jul2004: Clear out error code, since the variable may previously have
	// been used for who knows what.
	*ErrorCode	= NO_ERROR;
	ReStart		= 0;

	// DonR 10Jun2013: Array error handler now stores LineNo, to be written to prescription_msgs. Rather than add a parameter to
	// this function, just compute LineNo based on the two pointers that are already passed.
	LineNo = 1 + (presdrugs - DrugArrayPtr);

	// DonR 21Mar2011: Use values from input variables only if they're non-NULL.
	ConfNum		= (confnum		!= NULL) ? *confnum		: 0;
	ConfNumSrc	= (confnumsrc	!= NULL) ? *confnumsrc	: 0;

	// DonR 28Mar2007: Initialize output "Tikra" flag to zero.
    if (IshurWithTikra_out != NULL)
		*IshurWithTikra_out = 0;

	// Get Prescription data.
	if (presdrugs->DFatalErr == MAC_TRUE)
	{
		RESTORE_ISOLATION;
		return (0);	// DonR 21Mar2006: Not a show-stopper - just skip to next drug
					// in calling-routine loop.
	}

    SystemDate = GetDate ();


	// TypeCheckSpecPr should never actually be zero here -
	// so this code should probably be removed! (Kill it when Trn. 1063 is killed.)
	// DonR 13Nov2008: Switch to new health-basket field.
	// DonR 12Sep2012: Ishurim with Member Price Code of zero are invalid - ignore them!
	if (TypeCheckSpecPr == 0)  //old version
	{
		*ErrorCode = ERR_SPCL_PRES_NOT_FOUND;
		RESTORE_ISOLATION;
		return (1);
	}	// Special Prescription Check Type = zero.

	
	else    // 1 or 2 or 3
	{
		SelectDrugCode	= DrugCode;
		SqlResult		= -1;	// Default - nothing found or error.

		// DonR 01Sep2003: If the calling routine has specified a non-zero expiry tolerance,
		// adjust the expiration date test accordingly.
		// 21Oct2004: Per Iris Shaya, do NOT extend ishurim if their source is 5 - these
		// are "automatic" ishurim, and selling the drug would trigger an unwanted
		// automatic extension.
		// DonR 07Jul2011: Moved this computation up here, since TestDate is now used in the WHERE clause
		// of best_sp_pr_cur.
		if ((MaxExpiredDays_in > 0) && (ConfNumSrc != 5))
			TestDate = AddDays (SystemDate, (0 - MaxExpiredDays_in));
		else
			TestDate = SystemDate;

		ExecSQL (	MAIN_DB, TestSpecialPresc_READ_special_prescs,
					&IshurPharmNum,			&DrugCode,				&MemberPart,
					&PrivPharmSaleOk,		&TreatmentStart,		&StopUseDate,
					&ConstSumToPay,			&ConfNumSrc,			&v_QtyLim_course_lim,
					&ConfNum,				&v_in_health_pack_sp,	&SP_TreatmentType,
					&v_insurance_used,		&AbruptCloseFlg,		&v_QtyLim_ingr,
					&v_QtyLim_flg,			&v_QtyLim_course_len,	&v_QtyLim_all_ishur,

					&v_QtyLim_all_warn,		&v_QtyLim_all_err,		&v_QtyLim_course_wrn,
					&v_QtyLim_course_err,	&v_tikra_flag,			&v_IshurTikraType,
					&ishur_source_send,		&v_QtyLim_hist_start,	&QLM_all_ishur_std,
					&QLM_all_warn_std,		&QLM_all_err_std,		&QLM_course_lim_std,
					&QLM_course_wrn_std,	&QLM_course_err_std,	&v_QtyLim_per_day,
					&QLM_per_day_std,		&QLM_treat_days,		&needs_29_g,
					
					&monthly_qlm_flag,		&monthly_dosage,		&monthly_dosage_std,	// Marianna 06Aug2020 CR #28605/27955: added 6 columns.
					&monthly_duration,		&cont_approval_flag,	&orig_confirm_num,		// Marianna 06Aug2020 CR #28605/27955: added 6 columns.
					&ExceptionalIshur,

					&MemberId,				&IdExt,					&SelectDrugCode,
					&SystemDate,			&TestDate,				&SystemDate,
					END_OF_ARG_LIST															);

		if ((SQLERR_code_cmp (SQLERR_access_conflict)			== MAC_TRUE)	||
			(SQLERR_code_cmp (SQLERR_access_conflict_cursor)	== MAC_TRUE))
		{
			GerrLogMini (GerrId, "PID %d: Access conflict in TestSpecialPresc_READ_special_prescs.", (int)getpid());
			RESTORE_ISOLATION;
			return (2);
		}

		if ((SQLERR_code_cmp (SQLERR_not_found)		!= MAC_TRUE)	&&
			(SQLERR_code_cmp (SQLERR_end_of_fetch)	!= MAC_TRUE))
		{
			if (SQLERR_error_test ())
			{
				*ErrorCode		= ERR_DATABASE_ERROR;
				RESTORE_ISOLATION;
				return (1);
			}
		}

		SqlResult = SQLCODE;	// If we get here, it's either 0 (= OK) or a "not found" error.

		// If not found for the drug being sold, check the generic equivalent.
		if ((SqlResult != 0) && (AlternateLargo > 0))
		{
			SelectDrugCode	= AlternateLargo;

			ExecSQL (	MAIN_DB, TestSpecialPresc_READ_special_prescs,
						&IshurPharmNum,			&DrugCode,				&MemberPart,
						&PrivPharmSaleOk,		&TreatmentStart,		&StopUseDate,
						&ConstSumToPay,			&ConfNumSrc,			&v_QtyLim_course_lim,
						&ConfNum,				&v_in_health_pack_sp,	&SP_TreatmentType,
						&v_insurance_used,		&AbruptCloseFlg,		&v_QtyLim_ingr,
						&v_QtyLim_flg,			&v_QtyLim_course_len,	&v_QtyLim_all_ishur,

						&v_QtyLim_all_warn,		&v_QtyLim_all_err,		&v_QtyLim_course_wrn,
						&v_QtyLim_course_err,	&v_tikra_flag,			&v_IshurTikraType,
						&ishur_source_send,		&v_QtyLim_hist_start,	&QLM_all_ishur_std,
						&QLM_all_warn_std,		&QLM_all_err_std,		&QLM_course_lim_std,
						&QLM_course_wrn_std,	&QLM_course_err_std,	&v_QtyLim_per_day,
						&QLM_per_day_std,		&QLM_treat_days,		&needs_29_g,
					
						&monthly_qlm_flag,		&monthly_dosage,		&monthly_dosage_std,	// Marianna 06Aug2020 CR #28605/27955: added 6 columns.
						&monthly_duration,		&cont_approval_flag,	&orig_confirm_num,		// Marianna 06Aug2020 CR #28605/27955: added 6 columns.
						&ExceptionalIshur,

						&MemberId,				&IdExt,					&SelectDrugCode,
						&SystemDate,			&TestDate,				&SystemDate,
						END_OF_ARG_LIST															);

			if ((SQLERR_code_cmp (SQLERR_access_conflict)			== MAC_TRUE)	||
				(SQLERR_code_cmp (SQLERR_access_conflict_cursor)	== MAC_TRUE))
			{
				LOG_CONFLICT ();
				RESTORE_ISOLATION;
				return (2);
			}

			if ((SQLERR_code_cmp (SQLERR_not_found)		!= MAC_TRUE)	&&
				(SQLERR_code_cmp (SQLERR_end_of_fetch)	!= MAC_TRUE))
			{
				if (SQLERR_error_test ())
				{
					*ErrorCode = ERR_DATABASE_ERROR;
					RESTORE_ISOLATION;
					return (1);
				}
			}

			SqlResult = SQLCODE;	// If we get here, it's either 0 (= OK) or a "not found" error.
		}

		// Test database errors
		if (SqlResult == 100)		// Row not found.
		{
			if (TypeCheckSpecPr == 2) // really number
			{
				*ErrorCode = ERR_SPCL_PRES_NOT_FOUND;
				RESTORE_ISOLATION;
				return (1);
			}
			else
			{
				if (TypeCheckSpecPr == 1 ||  TypeCheckSpecPr == 3) // have no number or fictive
				{
					FlgNotFound = 1;
					RESTORE_ISOLATION;
					return (0);
				}
			}

		}
		else
		{
			// This shouldn't be necessary, as we've already checked for severe errors above.
			if (SQLERR_error_test ())
			{
				*ErrorCode = ERR_DATABASE_ERROR;
				RESTORE_ISOLATION;
				return (1);
			}
		}


		// Test date of ishur.
		// DonR 12Apr2010: Add information message to tell pharmacy that we're selling based on
		// an expired AS/400 ishur that we've extended based on their Pharmacy Ishur.
		if (StopUseDate < SystemDate)
		{
			// The ishur was supposed to expire before today.
			SetErrorVarArr (&presdrugs->DrugAnswerCode, ERR_USING_EXPIRED_ISHUR_WARN, DrugCode, LineNo, NULL, NULL);
		}

		else	// The ishur is still "b'tokef".
		{
			// DonR 29Feb2024 User Story #276372 fix: If this is an "ishur harig" (which is valid
			// only for a very short time) do *not* send "impending ishur expiration" warnings.
			if (!ExceptionalIshur)
			{
				// DonR 20Jan2004: Provide warning messages for soon-to-expire Special Prescriptions.
				TestDate = AddDays (SystemDate, 16);
				if (StopUseDate < TestDate)
				{
					SetErrorVarArr (&presdrugs->DrugAnswerCode, ERR_ISHUR_EXPIRY_15_WARNING, DrugCode, LineNo, NULL, NULL);
				}
				else
				{
					TestDate = AddDays (SystemDate, 31);
					if (StopUseDate < TestDate)
					{
						SetErrorVarArr (&presdrugs->DrugAnswerCode, ERR_ISHUR_EXPIRY_30_WARNING, DrugCode, LineNo, NULL, NULL);
					}
					else
					{
						TestDate = AddDays (SystemDate, 46);
						if (StopUseDate < TestDate)
							SetErrorVarArr (&presdrugs->DrugAnswerCode, ERR_ISHUR_EXPIRY_45_WARNING, DrugCode, LineNo, NULL, NULL);
					}	// Ishur expires at least 31 days from now.
				}	// Ishur expires at least 16 days from now.
			}	// This is *not* an "ishur harig", so expiration warnings are enabled.
		}	// Ishur expiry is today or beyond.


		// DonR 14Aug2003: Per Iris Shaya, Maccabi pharmacies can sell against Special Prescriptions
		// that were written for other pharmacies. They still get a warning, though.
		// DonR 31Jan2005: Pharmacy 995482 is a special case, as it's a fictitious pharmacy code
		// used for drugs delivered directly to hospitals. Special prescriptions that specify
		// this pharmacy code can be used *only* for purchases through this "pharmacy".
		// DonR 26Mar2014: Pharmacy 997582 works the same as Pharmacy 995482.
		//
		// DonR 12Jun2012: New version of Pharmacy Permision / Pharmacy Code checking. Now we perform
		// the Permission Type check in all cases, and look at specific-pharmacy matching only if the
		// pharmacy selling against the ishur is in the right category.
		IshurPharmError = 0;	// Re-initialize just to be paranoid.

		if (((PrivPharmSaleOk == 1) && (v_MaccabiPharmacy != PERMISSION_TYPE_MACABI))	||
			((PrivPharmSaleOk == 6) && (v_MaccabiPharmacy == PERMISSION_TYPE_PRIVATE)))
		{
			// Pharmacy does not have adequate permission to sell against this ishur.
			IshurPharmError = ERR_PHARM_CANT_SALE_SPCL_PRES;
		}

		else	// Permission Type is OK - now check for specific Pharmacy Code mismatch.
		{
			if ((IshurPharmNum != 0) && (IshurPharmNum != v_PharmNum))
			{
				// If the ishur is for the special pharmacy that supplies hospitals, give a different
				// error, regardless of the Pharmacy Type of the pharmacy attempting the sale.
				// DonR 12Sep2012: Prati Plus pharmacies now get NO error message in this case; only
				// Maccabi pharmacies get ERR_SPEC_PR_FOR_OTHER_PHARM_WRN (6081).
				// DonR 11Jul2016: Per CR #8469, the two special pharmacies 995482 and 997582 are now
				// treated as a single unit - that is, if the ishur specifies one of them, the other
				// one can also sell against the ishur. Any other pharmacy is still locked out.
				IshurPharmError =
					(((IshurPharmNum == 995482) && (v_PharmNum != 997582)) || ((IshurPharmNum == 997582) && (v_PharmNum != 995482)))
						? ERR_PHARM_995482_IN_ISHUR
						: ((v_MaccabiPharmacy == PERMISSION_TYPE_PRIVATE)
								? ERR_PHARM_CANT_SALE_SPCL_PRES
								: ((v_MaccabiPharmacy == PERMISSION_TYPE_MACABI) ? ERR_SPEC_PR_FOR_OTHER_PHARM_WRN : 0));
			}	// Non-zero Ishur Pharmacy Code is different from selling Pharmacy Code.
		}

		if (IshurPharmError > 0)
		{
			if (SetErrorVarArr (&presdrugs->DrugAnswerCode, IshurPharmError, DrugCode, LineNo, &presdrugs->DFatalErr, NULL))
			{
				RESTORE_ISOLATION;
				return (1);
			}
		}


		// DonR 26Aug2003: If the Special Prescription specifies a Treatment Category (such as
		// in-vitro fertilization or dialysis), check that the prescribing doctor is allowed
		// to prescribe this drug for this purpose. We test this by seeing if an appropriate
		// row exists in doctor_percents.
		// DonR 15Feb2011: Tuned WHERE clause for better performance.
		if ((SP_TreatmentType > TREATMENT_TYPE_NOCHECK) && (SP_TreatmentType < TREATMENT_TYPE_MAX))
		{
			ExecSQL (	MAIN_DB, CheckDoctorPercentsTreatmentCategory,
						&rows_found,
						&DrugCode,		&SP_TreatmentType,	&v_DocIdType,		&v_DocID,
						&v_DocIdType,	&v_DocID,			&ROW_NOT_DELETED,	END_OF_ARG_LIST	);

			if (SQLERR_error_test ())
			{
				*ErrorCode = ERR_DATABASE_ERROR;
				RESTORE_ISOLATION;
				return (1);
			}

			if (rows_found < 1)
			{
				// Doctor can't prescribe this drug for this treatment category.
				// DonR 20Dec2011: Per Iris Shaya, don't give the same error twice - just give it at
				// the drug level.
				if (SetErrorVarArr (&presdrugs->DrugAnswerCode, ERR_BAD_TREAT_CATEGORY_FOR_DOC,
									DrugCode, LineNo, &presdrugs->DFatalErr, NULL))
				{
					RESTORE_ISOLATION;
					return (1);
				}
			}
		}	// Need to validate Special Prescription & prescribing doctor against Doctor Percents.

	}	// TypeCheckSpecPr is non-zero.

	if (FlgNotFound)
	{
		RESTORE_ISOLATION;
		return (0);
	}


	// Validate call type and pharmacy-supplied ishur number (NIU, I think).
	if (TypeCheckSpecPr == 0 ||  TypeCheckSpecPr == 2) // old version or really number
	{
		if (confnum != NULL)
		{
			if (ConfNum != *confnum) //find largo but other spec_num
			{
				*ErrorCode = ERR_SPCL_PRES_NOT_FOUND;
				RESTORE_ISOLATION;
				return (1);
			}
		}
	}

    if (IdExt != Member->ID_code || MemberId != Member->ID)
    {
		if (SetErrorVarArr (ErrorCode, ERR_SPCL_PRES_MEMB_ID_WRONG, 0, 0, NULL, NULL))
		{
			RESTORE_ISOLATION;
			return (1);
		}
    }


	// Marianna 30Jul2020 CR #28605: If this ishur is a replacement for an earlier one
	// and we're supposed to look at purchases for both of them, read the Treatment
	// Start Date from the earlier ishur.
	// DonR 13Aug2020: In the case of replacement ishurim, we need to know the Start
	// Date of the previous ishur even if (as will usually be the case) the ishur itself
	// does *not* have qty_lim_flg set TRUE. Accordingly, this block of code needs to
	// execute whether or not there is an "internal" bakara kamutit - and I moved it
	// outside the "if" below.
	if (cont_approval_flag)
	{
		ExecSQL (	MAIN_DB, TestSpecialPresc_READ_PreviousIshurDate,
					&PrevIshurTreatmentStartDate,
					&MemberId,			&DrugCode,
					&orig_confirm_num,  &IdExt,
					END_OF_ARG_LIST										);

		if (SQLCODE == 0)
		{
			if (PrevIshurTreatmentStartDate < TreatmentStart)
				v_QtyLim_hist_start = PrevIshurTreatmentStartDate;
			else
				v_QtyLim_hist_start = TreatmentStart;
		}
		else
		{
			// Previous ishur was not found.
			v_QtyLim_hist_start	= TreatmentStart;
			orig_confirm_num	= -9999999;          // Assign a value we're sure won't match anything.
			cont_approval_flag	= 0;
		}		
			
	}	// ishur needs to check sales from a previous version of the ishur.

	// DonR 08Mar2023 User Story #276372 BUG FIX/WORKAROUND: If this is an "exceptional" ishur, AS/400
	// really should not give us a value for Original Ishur Number (orig_confirm_num) because the
	// exceptional ishur is *not* supposed to test drug purchases made under the normal ishur. However,
	// in testing we see that while the "Continued Ishur Flag" (cont_approval_flag) is FALSE, we still
	// get the "normal" ishur number in orig_confirm_num, and this causes the quantity-limit logic to
	// see purchases under the normal ishur instead of ignoring them. Accordingly, if cont_approval_flag
	// is FALSE, we want to force orig_confirm_num to a nonsense value to get around the problem.
	// (It *may* also be a good idea to suppress orig_confirm_num based on a TRUE value in ExceptionalIshur,
	// but I'm not going to bother doing so at this point since going by cont_approval_flag should get
	// the job done.)
	else
	{
		orig_confirm_num	= -9999999;	// Assign a value we're sure won't match anything.
	}
		//Marianna CR#28605 end.	


	// DonR 19Feb2006: New Ishur Limit checking.
	// DonR 07Jul2013: The new "standardized" limit fields will not necessarily be filled in at
	// first - so use them conditionally for now. Once all active ishurim have the standardized
	// limit fields filled in, the code can be simplified to use them unconditionally and not
	// even bother reading the non-standardized limits. (We actually should be able to get by
	// with a single "if" here - I'm being a little paranoid.)
	if (QLM_all_ishur_std	> 0.0)	v_QtyLim_all_ishur	= QLM_all_ishur_std;
	if (QLM_all_warn_std	> 0.0)	v_QtyLim_all_warn	= QLM_all_warn_std;
	if (QLM_all_err_std		> 0.0)	v_QtyLim_all_err	= QLM_all_err_std;
	if (QLM_course_lim_std	> 0.0)	v_QtyLim_course_lim	= QLM_course_lim_std;
	if (QLM_course_wrn_std	> 0.0)	v_QtyLim_course_wrn	= QLM_course_wrn_std;
	if (QLM_course_err_std	> 0.0)	v_QtyLim_course_err	= QLM_course_err_std;
	if (QLM_per_day_std		> 0.0)	v_QtyLim_per_day	= QLM_per_day_std;


	// DonR 22Feb2006: Don't bother checking if all the limits are zero.
	if (( v_QtyLim_flg								> 0)	 &&
		( v_QtyLim_ingr								> 0)	 &&
		((   v_QtyLim_all_err
		   + v_QtyLim_course_err
		   + v_QtyLim_all_warn
		   + v_QtyLim_course_wrn
		   + v_QtyLim_all_ishur
		   + v_QtyLim_course_lim)					> 0.0))
	{
		presdrugs->HasIshurWithLimit = MAC_TRUE;	// DonR 21Mar2006: So we know not
													// to give "early refill" warnings.

		// Set Quantity Limit Check Type for this drug.
		presdrugs->qty_limit_chk_type = SP_PR_PURCHASE_LIMIT;

		// DonR 07Jul2013: The new column qlm_history_start is now used to control how far back we look
		// to determine drug-ingredient usage, since limits may change during the life of an ishur.
		// However, at least until all current ishurim are updated, there will be ishurim without a
		// value in this field - so provide a default to use the start-date of the ishur itself, as
		// we did previously.
		if (v_QtyLim_hist_start == 0)
			v_QtyLim_hist_start = TreatmentStart;

		// DonR 16Feb2006: If this ishur has the new limit-checking values,
		// get drugs previously purchased by member. If the array has already
		// been loaded, we don't have to do it again.
		// DonR 25May2010: get_drugs_in_blood() now handles old and new member ID, and is also smart enough
		// to decide whether it has to reload to drugs-in-blood buffer - so we can call it once, unconditionally.
		if (get_drugs_in_blood (Member,
								&v_DrugsBuf,
								GET_ALL_DRUGS_BOUGHT,
								ErrorCode,
								&ReStart)				== 1)
		{
			if (ReStart != 0)
			{
				LOG_CONFLICT ();
				RESTORE_ISOLATION;
				return (2);
			}
			else
			{
				*ErrorCode = ERR_DATABASE_ERROR;
				RESTORE_ISOLATION;
				return (1);
			}
		}	// high severity error OR deadlock occurred

		// DonR 07Jul2013: New cycle-control logic required by "bakara kamutit" enhancement.
		// Note that FUTURE_SALE_ALL_PHARM doesn't actually permit future-dated sales at private pharmacies -
		// but that's taken care of elsewhere. A member could come to a private pharmacy with a current-dated
		// 90-day prescription and get it filled if the drug has this flag set.
		// Several different conditions will allow the higher level of purchase limits.
		// DonR 29Jan2024 QUESTION: I note that this logic still goes by "permission type" rather than
		// pharmacy ownership. Is that correct? Is a SuperPharm "consignatzia" pharmacy allowed to sell
		// future-dated prescriptions as if it were a Maccabi pharmacy?
		// DonR 29Jan2024 User Story #540234: Make ThreeMonthsEnabled conditional on either the item being
		// sold *not* being cannabis, or the global variable CannabisAllowFutureSales (from the sysparams
		// table) being TRUE.
		ThreeMonthsEnabled =	((presdrugs->DL.drug_type != 'K') || (CannabisAllowFutureSales))
								&&
								(	( presdrugs->DL.allow_future_sales == FUTURE_SALE_ALL_PHARM)														||
									((presdrugs->DL.allow_future_sales == FUTURE_SALE_MAC_ONLY) && (v_MaccabiPharmacy == PERMISSION_TYPE_MACABI))	||
									( presdrugs->vacation_ishur_num > 0)																				||
									( overseas_member_flag)
								);

		if (ThreeMonthsEnabled)
		{
			// Allow 4/5/6 cycles in last 1/2/3 months.
			// 15Jan2014: If we're in "three month" mode, the single-transaction limit depends
			// on the pharmacy type and the drug's Future Sales flag. Maccabi pharmacies can
			// sell all drugs on a "3/4/5/6" basis, while private pharmacies are on either a
			// "1/4/5/6" basis or a "3/4/5/6" basis depending on the Future Sales flag.
			ManotPerCycle [0] = ((v_MaccabiPharmacy					== PERMISSION_TYPE_MACABI) ||
								 (presdrugs->DL.allow_future_sales	== FUTURE_SALE_ALL_PHARM))		? 3.0 : 1.0;
			ManotPerCycle [1] = 4.0;
			ManotPerCycle [2] = 5.0;
			ManotPerCycle [3] = 6.0;
		}
		else	// "Three months" mode is *not* enabled.
		{
			// Allow 2/3/4 cycles in last 1/2/3 months.
			// 15Jan2014: If we're not in "three month" mode, even Maccabi pharmacies can sell only one
			// month's worth in a single transaction.
			ManotPerCycle [0] = 1.0;
			ManotPerCycle [1] = 2.0;
			ManotPerCycle [2] = 3.0;
			ManotPerCycle [3] = 4.0;
		}

		// These variables were already initialized, but paranoia is a good thing in a computer programmer.
		// Note that NumCourses is used only for the "old style" course-limit check.
		NumCourses			= 1.0;
		NumCoursesMonthly	= 1.0;	// DonR 05Apr2021 Feature #128927.
		QtyLimAllBought		= 0.0;
		AdjQtyLimAllBought	= 0.0;


		// Set CourseStart equal to the day *before* the current treatment
		// cycle started. If course length is less than 21, use a time
		// span of 30 days. AS400 should already have adjusted the course
		// limits accordingly.
		// DonR 10Oct2007: Per Iris Shaya, if we're dealing with a fixed
		// 30-day period (for short-course treatments), we still have to
		// look at purchases in the first few days of the 30-day period
		// to see if we want to allow purchase of two months' worth of
		// medicine.
		// DonR 25Nov2012: Per Iris Shaya, change the start-of-period time
		// from five days to seven days.
		// DonR 07Jul2013: Instead of hard-coded cycle-overlap time, use
		// a parameter read from sysparams (column = ishur_cycle_ovrlap).
		for (CycleNum = 0, CycleStartDate = SystemDate; CycleNum < 4; CycleNum++)
		{
			QtyLimCourseBought		[CycleNum] = 0.0;
			AdjQtyLimCourseBought	[CycleNum] = 0.0;
			CourseStart				[CycleNum] = CycleStartDate;	// First cycle consists of current purchase only.

			// Note that each iteration of the loop prepares CycleStartDate for the *next* cycle.
			if (v_QtyLim_course_len >= 21)
			{
				CycleStartDate	= IncrementDate (CycleStartDate, (0 - v_QtyLim_course_len));
			}
			else
			{
				// For short treatment cycles, the "first few days" logic works assuming a one-month span.
				CycleStartDate	= IncrementDate (CycleStartDate, -30);
			}
		}

		FirstFewDays = IncrementDate (CourseStart[1], IshurCycleOverlap);
		
		// Marianna 10Aug2020 CR #27955: Added a new "monthly" limit, which operates
		// independently of all the other limits.
		if (monthly_qlm_flag)
		{  
			QtyLimCourseBought     [4]	= 0.0;
			AdjQtyLimCourseBought  [4]	= 0.0;
			CourseStart            [4]  = IncrementDate (SystemDate, ( -30 * monthly_duration));
			FirstFewDaysMonthly			= IncrementDate (CourseStart[4], IshurCycleOverlap);
		}

		// DonR 25Nov2012: If this ishur is "forever" (i.e. its Stop Use Date
		// is 31 December 2039 or later) then the per-ishur limits sent by
		// AS/400 are meaningless - since there is no set number of cycles.
		// In this case, we have two possibilities: First, if the ishur's
		// Treatment Start Date is far enough in the past to allow at least six
		// courses of treatment, we will construct "fictitious limits" based
		// on six courses (allowing purchase of seven courses' worth); and
		// second, if less time has passed since the Treatment Start Date, we
		// will simply ignore per-ishur limits and check only against per-course
		// limits.
		// DonR 04Sep2022: It appears that this logic, which allows the member
		// to buy 7 courses of treatment over 6 courses of time, is overly strict -
		// at least in cases where the member has a vacation ishur. We should get
		// a change request to add some flexibility in this case. Entered as a
		// bug report (#448756) in ADO on 10May2023. (NOTE: It might also be
		// possible to leave this logic enabled, but change the limit multiplier
		// from 7.0 to 9.0 for people with vacation_ishur_num > 0. However, I
		// don't think it's really necessary, and simply disabling the logic for
		// these situations seems safer to me.)
		if ((StopUseDate					>= 20391231)	&&		// I.e. "forever".
			(presdrugs->vacation_ishur_num	== 0)			&&		// DonR 10May2023 Bug #448756
			(!cont_approval_flag))									// Marianna 06Aug2020 CR#28605 suppress this logic if
																	// we're in "continued ishur" mode.
		{
			SixCyclesStart = IncrementDate (SystemDate, (0 - (6 * v_QtyLim_course_len)));

			if (v_QtyLim_hist_start <= SixCyclesStart)
			{
				// At least six cycles have passed - construct fictitious all-ishur limits.
				v_QtyLim_hist_start = SixCyclesStart;
				v_QtyLim_all_err = v_QtyLim_all_warn = v_QtyLim_all_ishur = 7.0 * v_QtyLim_course_lim;
			}
			else
			{
				// Fewer than six cycles have passed - disable all-ishur limits.
				v_QtyLim_all_err = v_QtyLim_all_warn = v_QtyLim_all_ishur = 0.0;
			}
		}


		// DonR 12Sep2013: If we're working with single-use units (including pills, capsules,
		// ampoules, etc.) determine how much of the quantity-limit ingredient the member will
		// actually be using. Note that the list below is of shape codes for which we do NOT
		// want to calculate actual usage - for these, we just go by what is sold.
		if ((!presdrugs->actual_usage_checked)		&&		// We haven't already done the check for this drug.
			( presdrugs->DL.shape_code_new != 16)	&&		// Penfill
			( presdrugs->DL.shape_code_new != 22)	&&		// Elixir
			( presdrugs->DL.shape_code_new != 45)	&&		// Novolet
			( presdrugs->DL.shape_code_new != 47)	&&		// Cartridge
			( presdrugs->DL.shape_code_new != 51)	&&		// Pen - added 01Oct2013.
			( presdrugs->DL.shape_code_new != 66)	&&		// Syrup
			( presdrugs->DL.shape_code_new != 82)	&&		// Suspension
			( presdrugs->DL.shape_code_new != 84)	&&		// Syrup
			( presdrugs->DL.shape_code_new != 88)	&&		// Oral solution
			( presdrugs->DL.shape_code_new != 96))			// Cartridge
		{
			// Copy a couple of fields to prescription_usage just to make it easier to
			// see why we calculated as we did.
			presdrugs->course_len			= v_QtyLim_course_len;
			presdrugs->course_treat_days	= QLM_treat_days;

			// Calculate how many courses of treatment and treatment days medicine
			// is being supplied for. Note that we are assuming that the pharmacy is going
			// to send correct "duration" numbers (e.g. 28 for four treatment courses of 7
			// days each). As of 15 September 2013, this is very much not the case - for
			// example, around 30% of the time pharmacies will send a duration of 4 (i.e.
			// four treatment days) when they should send 28 (four treatment *courses*).
			// According to Iris, the solution will be for pharmacies to change their
			// procedures, not for this program to try to second-guess them.

			// Calculate number of full courses using integer division - so that if
			// course length is 7 and pharmacy sends duration of 12 days, we consider
			// that to be one course.
			presdrugs->calc_courses	= presdrugs->Duration / v_QtyLim_course_len;
			if (presdrugs->calc_courses < 1)
				presdrugs->calc_courses = 1;	// Assume that drugs are supplied for at least one course.

			if (QLM_treat_days < 1)
				QLM_treat_days = 1;			// Assume at least one treatment day per course.
			presdrugs->calc_treat_days = presdrugs->calc_courses * QLM_treat_days;


			determine_actual_usage	(DrugArrayPtr,
									 NumDrugs,
									 (LineNo - 1),
									 presdrugs->calc_treat_days,
									 v_QtyLim_ingr,
									 v_QtyLim_per_day,
									 QLM_treat_days);
		}


		// Scan through previous drug purchases to find purchases of the
		// ishur's limit ingredient.
		for (DrugNum = 0; DrugNum < v_DrugsBuf.max; DrugNum++)
		{
			// Skip this drug if its purchase date is out of range, OR if it was not
			// purchased under the same ishur as the drug being sold. (DonR 25Nov2012)
			// DonR 07Jul2013: Use new Quantity Limit Start Date instead of Treatment
			// Start Date. (They will be the same unless someone has changed Quantity
			// Limits during the lifetime of the ishur.)
			// Marianna 11Aug2020 CR#28605: If this ishur is a replacement for an earlier one
			// and we're supposed to look at purchases for both of them, we need to include
			// *both* ishur numbers in this "if". (We already set v_QtyLim_hist_start to the
			// Start Date of the previous ishur, if we found it.)
			if ((v_DrugsBuf.Durgs[DrugNum].StartDate		<  v_QtyLim_hist_start)			||
				(v_DrugsBuf.Durgs[DrugNum].StartDate		>  SystemDate)					||
				((v_DrugsBuf.Durgs[DrugNum].SpecialPrescNum	!= ConfNum)				&&
				 (v_DrugsBuf.Durgs[DrugNum].SpecialPrescNum	!= orig_confirm_num)))
			{
				continue;
			}

			// Loop through generic ingredients for this historical drug purchase.
			for (Ingr = 0; Ingr < 3; Ingr++)
			{
				// Skip anything that isn't the ingredient we're testing for.
				// Note that we're assuming that the "target" ingredient will be present
				// only once for any given drug - if the AS/400 feeds us strange
				// data in drug_list, we'll get strange results!
				if (v_DrugsBuf.Durgs[DrugNum].Ingredient[Ingr].code != v_QtyLim_ingr)
					continue;


				// Check prescription_usage to see if there's an adjusted-usage figure available.
				// If not, just copy the "regular" usage number.
				v_LookupPrID	= v_DrugsBuf.Durgs[DrugNum].PrescriptionID;
				v_LookupLineNo	= v_DrugsBuf.Durgs[DrugNum].LineNo;

				ExecSQL (	MAIN_DB, TestSpecialPresc_READ_prescription_usage,
							&v_AdjLimIngrUsage,	&v_LookupPrID, &v_LookupLineNo, END_OF_ARG_LIST	);

				if (SQLCODE == 0)
					v_DrugsBuf.Durgs[DrugNum].AdjLimIngrUsage = v_AdjLimIngrUsage;
				else
					v_DrugsBuf.Durgs[DrugNum].AdjLimIngrUsage = v_DrugsBuf.Durgs[DrugNum].Ingredient[Ingr].quantity;


				QtyLimAllBought		+= v_DrugsBuf.Durgs[DrugNum].Ingredient[Ingr].quantity;
				AdjQtyLimAllBought	+= v_DrugsBuf.Durgs[DrugNum].AdjLimIngrUsage;

				// Loop through last three treatment cycles (or months, for short cycles).
				// Marianna 10Aug2020 CR #27955: Added the new optional "monthly" cycle to the loop.
				for (CycleNum = 1; CycleNum < 5; CycleNum++)	// Marianna 10Aug2020 Added Cycle 4.
				{
					if (v_DrugsBuf.Durgs[DrugNum].StartDate > CourseStart[CycleNum])
					{
						// Note that the quantity retrieved by get_drugs_in_blood() is, by default,
						// in standardized units.
						QtyLimCourseBought		[CycleNum]	+= v_DrugsBuf.Durgs[DrugNum].Ingredient[Ingr].quantity;
						AdjQtyLimCourseBought	[CycleNum]	+= v_DrugsBuf.Durgs[DrugNum].AdjLimIngrUsage;

						// DonR 15Mar2006: If something with this ingredient was bought
						// within the first few days of the most recent treatment cycle,
						// assume that the member is buying for the next cycle - so we'll
						// test against double the limits.
						if ((CycleNum == 1) && (v_DrugsBuf.Durgs[DrugNum].StartDate <= FirstFewDays))
							NumCourses = 2.0;

						// DonR 05Apr2021 Feature #128927: If something with this ingredient was bought
						// within the first few days of the most recent *monthly* cycle, assume that
						// the member is buying for the next cycle - so we'll test against double the limits.
						if (monthly_qlm_flag && (CycleNum == 4) && (v_DrugsBuf.Durgs[DrugNum].StartDate <= FirstFewDaysMonthly))
							NumCoursesMonthly = 2.0;
					}	// This purchase was made during the relevant treatment cycle.
				}	// Loop through past treatment cycles.
			}	// Loop through ingredients.
		}	// Loop through previously-purchased drugs.


		// Scan through the current drug purchase request to find purchases
		// of the ishur's limit ingredient.
		for (DrugNum = 0; DrugNum < NumDrugs; DrugNum++)
		{
			// Loop through generic ingredients for line of the current drug purchase.
			for (Ingr = 0; Ingr < 3; Ingr++)
			{
				// Skip anything that isn't the ingredient we're testing for.
				if (DrugArrayPtr[DrugNum].DL.Ingredient[Ingr].code == v_QtyLim_ingr)
				{
					// DonR 07Aug2013: Standardized ingredient usage is used if available (which it
					// always should be).
					if (DrugArrayPtr[DrugNum].IngrQuantityStd[Ingr] > 0.0)
					{
						QtyLimAllBought		+= DrugArrayPtr[DrugNum].IngrQuantityStd[Ingr];
						AdjQtyLimAllBought	+= DrugArrayPtr[DrugNum].net_lim_ingr_used;

						// Current purchase gets added into all cycles.
						// Marianna 10Aug2020 CR #27955: Added the new optional "monthly" cycle to the loop.
						for (CycleNum = 0; CycleNum < 5; CycleNum++)	// Marianna 10Aug2020 CR #27955 change CycleNum < from 4 to 5 
						{
							QtyLimCourseBought		[CycleNum]	+= DrugArrayPtr[DrugNum].IngrQuantityStd[Ingr];
							AdjQtyLimCourseBought	[CycleNum]	+= DrugArrayPtr[DrugNum].net_lim_ingr_used;
						}
					}
					else
					{
						QtyLimAllBought		+= DrugArrayPtr[DrugNum].TotIngrQuantity[Ingr];
						AdjQtyLimAllBought	+= DrugArrayPtr[DrugNum].net_lim_ingr_used;

						// Current purchase gets added into all cycles.
						// Marianna 10Aug2020 CR #27955: Added the new optional "monthly" cycle to the loop.
						for (CycleNum = 0; CycleNum < 5; CycleNum++)	// Marianna 10Aug2020 CR #27955 change CycleNum < from 4 to 5 
						{
							QtyLimCourseBought		[CycleNum] += DrugArrayPtr[DrugNum].TotIngrQuantity[Ingr];
							AdjQtyLimCourseBought	[CycleNum] += DrugArrayPtr[DrugNum].net_lim_ingr_used;
						}
					}

				}	// Found matching ingredient in current purchase.
			}	// Loop through ingredients.
		}	// Loop through drugs in the current purchase request.


		// DonR 15Mar2015: For certain drugs (identified by Parent Group Code in table force_new_ql_codes),
		// we want to send pharmacies the new "adjusted" Quantity Limit error codes. To do so, we'll
		// check the table and set the ForceAdjQLErrors flag.
		// DonR 16Jan2020 CR #31688: Instead of getting a separate flag from force_new_ql_codes, we now
		// have a per-drug flag in drug_list: tight_ishur_limits. This gets its value from
		// RK9001P/C0AJSS.
		// DonR 31Mar2020: May need to re-enable if Raya Karol isn't ready when the ODBC pilot begins.
//		ParentGroupCode = presdrugs->DL.parent_group_code;
//
//		ExecSQL (	MAIN_DB, TestSpecialPresc_READ_force_new_ql_codes, &ForceAdjQLErrors, &ParentGroupCode, END_OF_ARG_LIST	);
//
//		// No "real" error-checking; just assume that any non-OK result is equivalent to "not found".
//		if (SQLCODE != 0)
//			ForceAdjQLErrors = 0;



		// Compare combined historical and current purchases with AS400 ishur limits
		// and assign warning/error codes as necessary.
		// DonR 07Jul2013: To simplify code, set flag variables to indicate what problems were found. At the end, we'll
		// use these codes to figure out what errors/warnings to assign.
		// DonR 16Aug2020: Change limit comparisions from ">" to a ratio test. This lets us avoid rounding
		// errors without adding "fudge factors", and should give a more accurate result.
		AllIshur100Pct			= ((v_QtyLim_all_ishur	> 0.0) && ((QtyLimAllBought		/ v_QtyLim_all_ishur	)	> 1.000001));
		AllIshur115Pct			= ((v_QtyLim_all_warn	> 0.0) && ((QtyLimAllBought		/ v_QtyLim_all_warn		)	> 1.000001));
		//AllIshur130Pct		= ((v_QtyLim_all_err	> 0.0) && (QtyLimAllBought		> v_QtyLim_all_err		)) DISABLED AS OF JULY 2013.

		AdjAllIshur100Pct		= ((v_QtyLim_all_ishur	> 0.0) && ((AdjQtyLimAllBought	/ v_QtyLim_all_ishur	)	> 1.000001));
		AdjAllIshur115Pct		= ((v_QtyLim_all_warn	> 0.0) && ((AdjQtyLimAllBought	/ v_QtyLim_all_warn		)	> 1.000001));
		AdjAllIshur130Pct		= ((v_QtyLim_all_err	> 0.0) && ((AdjQtyLimAllBought	/ v_QtyLim_all_err		)	> 1.000001));	// Enabled 16Mar2015.

		// If we're operating in "three month mode", disable the old single-course logic.
		if (!ThreeMonthsEnabled)
		{
			CurrentCourse100Pct		= ((v_QtyLim_course_lim	> 0.0) && ((QtyLimCourseBought[1]		/ (v_QtyLim_course_lim * NumCourses))	> 1.000001));
			AnyCourse115Pct			= ((v_QtyLim_course_wrn > 0.0) && ((QtyLimCourseBought[1]		/ (v_QtyLim_course_wrn * NumCourses))	> 1.000001));
			//AnyCourse130Pct		= ((v_QtyLim_course_err > 0.0) && QtyLimCourseBought[1]	> (v_QtyLim_course_err * NumCourses))); DISABLED AS OF JULY 2013.

			AdjCurrentCourse100Pct	= ((v_QtyLim_course_lim	> 0.0) && ((AdjQtyLimCourseBought[1]	/ (v_QtyLim_course_lim * NumCourses))	> 1.000001));
			AdjAnyCourse115Pct		= ((v_QtyLim_course_wrn > 0.0) && ((AdjQtyLimCourseBought[1]	/ (v_QtyLim_course_wrn * NumCourses))	> 1.000001));
			AdjAnyCourse130Pct		= ((v_QtyLim_course_err > 0.0) && ((AdjQtyLimCourseBought[1]	/ (v_QtyLim_course_err * NumCourses))	> 1.000001));	// Enabled 16Mar2015.
		}	// Not operating in "three month" mode.


		// Look through all four treatment cycles (Cycle 0 being the current purchase) to assign the new course-limit
		// errors (which, for now, are not sent to the pharmacy but will be examined internally) and also, if we're
		// in "three month" mode, to assign the "traditional" course-limit warning.
		// DonR 25Jul2013: Per Iris, assign the "traditional" course-limit warning even if we're not in
		// "three month" mode.
		for (CycleNum = 0; CycleNum < 4; CycleNum++)
		{
			//AnyCourse130Pct	+= ((v_QtyLim_course_err > 0.0) && (QtyLimCourseBought[CycleNum]	> (v_QtyLim_course_err * ManotPerCycle[CycleNum]))); DISABLED
			AnyCourse100Pct		+= ((v_QtyLim_course_lim > 0.0) && ((QtyLimCourseBought[CycleNum]		/ (v_QtyLim_course_lim * ManotPerCycle[CycleNum]))	> 1.000001));
			AnyCourse115Pct		+= ((v_QtyLim_course_wrn > 0.0) && ((QtyLimCourseBought[CycleNum]		/ (v_QtyLim_course_wrn * ManotPerCycle[CycleNum]))	> 1.000001));

			AdjAnyCourse100Pct	+= ((v_QtyLim_course_lim > 0.0) && ((AdjQtyLimCourseBought[CycleNum]	/ (v_QtyLim_course_lim * ManotPerCycle[CycleNum]))	> 1.000001));
			AdjAnyCourse115Pct	+= ((v_QtyLim_course_wrn > 0.0) && ((AdjQtyLimCourseBought[CycleNum]	/ (v_QtyLim_course_wrn * ManotPerCycle[CycleNum]))	> 1.000001));
			AdjAnyCourse130Pct	+= ((v_QtyLim_course_err > 0.0) && ((AdjQtyLimCourseBought[CycleNum]	/ (v_QtyLim_course_err * ManotPerCycle[CycleNum]))	> 1.000001)); // Enabled 16Mar2015.
		}	// Loop through treatment cycles (including current purchase).
		
		// Marianna 10Aug2020 CR #27955: Add check for new "monthly" cycle, if it's
		// enabled for this ishur.
		// DonR 05Apr2021 Feature #128927: If member bought something at the beginning of the monthly cycle, assume
		// that the current purchase is for the *next* cycle - and thus double the applicable limit.
		if (monthly_qlm_flag)    
		{  
			MonthlyLimit 	= ((monthly_dosage_std > 0.0) && ((QtyLimCourseBought	[4]	/ (monthly_dosage_std * NumCoursesMonthly))	> 1.000001));
			AdjMonthlyLimit = ((monthly_dosage_std > 0.0) && ((AdjQtyLimCourseBought[4]	/ (monthly_dosage_std * NumCoursesMonthly))	> 1.000001));
		}	// Marianna 10Aug2020 CR #27955 end.


//GerrLogMini (GerrId, "AnyCourse100Pct = %d, v_QtyLim_course_lim = %f, Cycle 1 start %d, Cycle 1 manot %f, Cycle 1 lim %f, Cycle 1 usage %f.",
//			          AnyCourse100Pct,      v_QtyLim_course_lim,      CourseStart[1],    ManotPerCycle[1],
//					  (v_QtyLim_course_lim * ManotPerCycle[1]),  QtyLimCourseBought[1]);


		// Finally, assign any errors/warnings that have been identified.
		// First, deal with the new codes that (for now) are not sent to pharmacy.
		// These are assigned unconditionally, since they're used only for internal analysis.
		// DonR 16Mar2015: For the moment at least, we *will* send thee new codes to pharmacies
		// for specific drugs whose Parent Group Codes are found in force_new_ql_codes.

		// DonR 16Mar2015: For a list of drugs identified by Parent Group Code, send the new "adjusted"
		// error codes to pharmacies, with "real" severity levels "borrowed" from the corresponding old
		// non-"adjusted" error codes. For these drugs (at least), we will send the full-severity error
		// codes, *not* the warning or needs-pharmacist-authorization codes, at the 100%-of-limit level!
		// DonR 16Jan2020 CR #31688: Instead of getting a separate flag from force_new_ql_codes, we now
		// have a per-drug flag in drug_list: tight_ishur_limits. This gets its value from
		// RK9001P/C0AJSS.
		// DonR 31Mar2020: May need to re-enable the old version if Raya Karol isn't ready when the ODBC pilot begins.
		if (presdrugs->DL.tight_ishur_limits)
//		if (ForceAdjQLErrors > 0)
		{
			if (AdjAllIshur100Pct)
			{
				GaveQtyLimitError = 1;
				if (SetErrorVarArrAsIf (&presdrugs->DrugAnswerCode,
										ADJ_ISHUR_INGR_TOTAL_ERR, ERR_ISHUR_INGR_TOTAL_ERR,	// "Borrowed" severity  = 10.
										DrugCode, LineNo, &presdrugs->DFatalErr, NULL))
				{
					SetErrorVarArr (ErrorCode, ERR_DRUG_S_WRONG_IN_PRESC, 0, 0, NULL, NULL);
				}	// Error is severe (which it should be), so exit routine.
			}	// Violated all-ishur 100% limit.
			else
			{
				if ((AdjCurrentCourse100Pct) || (AdjAnyCourse100Pct))	// Enabled 16Mar2015.
				{
					GaveQtyLimitError = 1;
					if (SetErrorVarArrAsIf (&presdrugs->DrugAnswerCode,
											ADJ_ISHUR_INGR_COURSE_ERR, ERR_ISHUR_INGR_COURSE_ERR,	// "Borrowed" severity = 10.
											DrugCode, LineNo, &presdrugs->DFatalErr, NULL))
					{
						SetErrorVarArr (ErrorCode, ERR_DRUG_S_WRONG_IN_PRESC, 0, 0, NULL, NULL);
					}	// Error is severe (which it should be), so exit routine.
				}	// Violated current-course or any-course 100% limit.
			}	// Did *not* violate the all-ishur 100% limit.
		}	// Drug sold IS flagged to get the new, high-severity "adjusted" messages.

		else	// Default for drugs not flagged to get the high-severity "adjusted" error codes.
		{
			if (AnyCourse100Pct)
			{
				GaveQtyLimitError = 1;
				SetErrorVarArr (&presdrugs->DrugAnswerCode,
								(ThreeMonthsEnabled) ? ISHUR_INGR_COURSE_456_MSG : ISHUR_INGR_COURSE_234_MSG,
								DrugCode, LineNo, &presdrugs->DFatalErr, NULL);
			}

			if (CurrentCourse100Pct)
			{
				GaveQtyLimitError = 1;
				SetErrorVarArr (&presdrugs->DrugAnswerCode, ISHUR_INGR_COURSE_100_MSG, DrugCode, LineNo, &presdrugs->DFatalErr, NULL);
			}

			if (AdjAnyCourse100Pct)
			{
				GaveQtyLimitError = 1;
				SetErrorVarArr (&presdrugs->DrugAnswerCode,
								(ThreeMonthsEnabled) ? ADJ_ISHUR_COURSE_456_MSG : ADJ_ISHUR_COURSE_234_MSG,
								DrugCode, LineNo, &presdrugs->DFatalErr, NULL);
			}

			if (AdjCurrentCourse100Pct)
			{
				GaveQtyLimitError = 1;
				SetErrorVarArr (&presdrugs->DrugAnswerCode, ADJ_ISHUR_COURSE_100_MSG, DrugCode, LineNo, &presdrugs->DFatalErr, NULL);
			}


			// Assign more investigational error/warning codes. These are assigned hierarchically.
			if (AdjAllIshur130Pct)	// Enabled 16Mar2015.
			{
				GaveQtyLimitError = 1;
				SetErrorVarArr (&presdrugs->DrugAnswerCode, ADJ_ISHUR_INGR_TOTAL_ERR, DrugCode, LineNo, &presdrugs->DFatalErr, NULL);
			}
			else
			{
				if (AdjAnyCourse130Pct)	// Enabled 16Mar2015.
				{
					GaveQtyLimitError = 1;
					SetErrorVarArr (&presdrugs->DrugAnswerCode, ADJ_ISHUR_INGR_COURSE_ERR, DrugCode, LineNo, &presdrugs->DFatalErr, NULL);
				}
				else
				{
					if (AdjAllIshur115Pct)
					{
						GaveQtyLimitError = 1;
						SetErrorVarArr (&presdrugs->DrugAnswerCode, ADJ_ISHUR_INGR_TOTAL_WARN, DrugCode, LineNo, &presdrugs->DFatalErr, NULL);
					}
					else
					{
						if (AdjAnyCourse115Pct)
						{
							GaveQtyLimitError = 1;
							SetErrorVarArr (&presdrugs->DrugAnswerCode, ADJ_ISHUR_INGR_COURSE_WARN, DrugCode, LineNo, &presdrugs->DFatalErr, NULL);
						}
					}	// Not assigning all-ishur warning.
				}	// Not assigning single-course error ("fatal" version).
			}	// NOT assigning ADJ_ISHUR_INGR_TOTAL_ERR ("fatal" version).


			// Now deal with the error/warning codes that *are* sent to pharmacy. These are assigned
			// hierarchically; note that as of July 2013, AllIshur130Pct and AnyCourse130Pct are not
			// actually in use. The warning messages that are in use aren't actually fatal, so the
			// return(0) statements for those will never execute unless someone changes their severity.
			if (AllIshur130Pct)	// (Not used as of July 2013.)
			{
				GaveQtyLimitError = 1;
				if (SetErrorVarArr (&presdrugs->DrugAnswerCode, ERR_ISHUR_INGR_TOTAL_ERR, DrugCode, LineNo, &presdrugs->DFatalErr, NULL))
				{
					SetErrorVarArr (ErrorCode, ERR_DRUG_S_WRONG_IN_PRESC, 0, 0, NULL, NULL);
				}
			}
			else
			{
				if (AnyCourse130Pct)	// (Not used as of July 2013.)
				{
					GaveQtyLimitError = 1;
					if (SetErrorVarArr (&presdrugs->DrugAnswerCode, ERR_ISHUR_INGR_COURSE_ERR, DrugCode, LineNo, &presdrugs->DFatalErr, NULL))
					{
						SetErrorVarArr (ErrorCode, ERR_DRUG_S_WRONG_IN_PRESC, 0, 0, NULL, NULL);
					}
				}
				else
				{
					if (AllIshur115Pct)
					{
						GaveQtyLimitError = 1;
						if (SetErrorVarArr (&presdrugs->DrugAnswerCode, ERR_ISHUR_INGR_TOTAL_WARN, DrugCode, LineNo, &presdrugs->DFatalErr, NULL))
						{
							SetErrorVarArr (ErrorCode, ERR_DRUG_S_WRONG_IN_PRESC, 0, 0, NULL, NULL);
						}
					}
					else
					{
						if (AnyCourse115Pct)
						{
							GaveQtyLimitError = 1;
							if (SetErrorVarArr (&presdrugs->DrugAnswerCode, ERR_ISHUR_INGR_COURSE_WARN, DrugCode, LineNo, &presdrugs->DFatalErr, NULL))
							{
								SetErrorVarArr (ErrorCode, ERR_DRUG_S_WRONG_IN_PRESC, 0, 0, NULL, NULL);
							}
						}
					}	// Not assigning all-ishur warning.
				}	// Not assigning single-course error ("fatal" version).
			}	// NOT assigning ERR_ISHUR_INGR_TOTAL_ERR ("fatal" version).
		}	// Drug sold is NOT in the list of Parent Group Codes that get the new "adjusted" messages.

	}	// Need to check new Ishur Limits.


	// Marianna 10Aug2020 CR #27955: Treat the new, optional "monthly" limit separately
	// from all the other limits. It's not clear (yet) whether we should send two separate
	// messages based on whether the limit violation is based on adjusted usage or not, so
	// we'll send one or the other but not both.
	if (AdjMonthlyLimit)
	{
		GaveQtyLimitError = 1;
		if (SetErrorVarArr (&presdrugs->DrugAnswerCode, ADJ_ISHUR_MONTHLY_LIMIT_ERR, DrugCode, LineNo, &presdrugs->DFatalErr, NULL))
		{
			SetErrorVarArr (ErrorCode, ERR_DRUG_S_WRONG_IN_PRESC, 0, 0, NULL, NULL);
		}
	}
	else
	{
		if (MonthlyLimit)
		{
			GaveQtyLimitError = 1;
			if (SetErrorVarArr (&presdrugs->DrugAnswerCode, ISHUR_MONTHLY_LIMIT_ERR, DrugCode, LineNo, &presdrugs->DFatalErr, NULL))
			{
				SetErrorVarArr (ErrorCode, ERR_DRUG_S_WRONG_IN_PRESC, 0, 0, NULL, NULL);
			}
		}
	}
	// Marianna 10Aug2020 CR #27955 end.


	// DonR 15Sep2020 CR #30106: If we found *any* quantity-limit problem, *and* this drug
	// requires a previous purchase to authorize 3-month sales, *and* we didn't find such
	// a previous qualifying purchase, send a special message to the pharmacy to tell them
	// why only one month's worth could be bought.
	if ((GaveQtyLimitError) && (presdrugs->DL.allow_future_sales == FUTURE_SALE_NOT_YET_QUALIFIED))
	{
		if (SetErrorVarArr (&presdrugs->DrugAnswerCode, ERR_FUTURE_SALE_NOT_YET_QUALIFIED, DrugCode, LineNo, &presdrugs->DFatalErr, NULL))
		{
			SetErrorVarArr (ErrorCode, ERR_DRUG_S_WRONG_IN_PRESC, 0, 0, NULL, NULL);
		}
	}


	// If we get here, we *didn't* hit a fatal error - so we're actually using the ishur.
	presdrugs->RetPartCode						= MemberPart;
    presdrugs->PrticSet							= MAC_TRUE;
    presdrugs->ret_part_source.table			= FROM_SPEC_PRESCS_TBL;
    presdrugs->ret_part_source.insurance_used	= (v_insurance_used) ? v_insurance_used : BASIC_INS_USED;
    presdrugs->in_health_pack					= v_in_health_pack_sp;
    presdrugs->IshurTikraType					= v_IshurTikraType;		// DonR 08Jun2010.
	presdrugs->rule_needs_29g					= needs_29_g;

	// DonR 19May2016: Whether we assign a fixed-price discount should depend on the Member Price
	// Code from the ishur, *not* from the default price code! (In reality, the default should
	// always be 1 and ConstSumToPay should have a value only if MemberPart == 1, but still...)
	// DonR 18May2025 User Story #388766: Moved this bit of code up a bit, since we may need to
	// overwrite the normal ishur fixed price with a special drug-shortage rule. In any case, it
	// makes more sense to have it here than where it was before.
	if ((ConstSumToPay > 0) && (MemberPart == 1))
	{
		// DonR 03May2004: Got rid of all the old Fixed Price logic for Special Prescriptions.
		// Instead of calculating a discount, just put the Fixed Price into the "Price Swap"
		// slot.
		presdrugs->PriceSwap = ConstSumToPay;
	}


	// DonR 18May2025 User Story #388766: If this is an ishur with tikra and the current
	// purchase is against a prescription from the previous calendar month that was partially
	// filled, we need to check for a special drug-shortage rule. If such a rule exists for
	// this drug (compatible with the member, pharmacy, and so on), we will use the participation
	// values from this rule (normally 0%) in place of whatever the AS/400 ishur normally gives.
	// Minor note: I'm assuming that the needs-29g value should still come from the ishur.
	if ((presdrugs->WasPossibleDrugShortage) && (v_tikra_flag > 0))
	{
		short	SelectTreatmentCategory;
		int		ShortageRuleNumber			= 0;
		short	ShortageRulePriceCode		= 0;
		int		ShortageRuleFixedPrice		= 0;
		char	ShortageRuleInsuranceType	[1 + 1];
		short	ShortageRuleHealthBasket	= 0;

		// If (and only if) the ishur has a valid non-zero Treatment Category,
		// use that value to select appropriate drug-shortage rules; otherwise,
		// default to 1 (= "general" treatment).
		SelectTreatmentCategory = ((SP_TreatmentType > TREATMENT_TYPE_NOCHECK) && (SP_TreatmentType < TREATMENT_TYPE_MAX)) ?
									SP_TreatmentType : TREATMENT_TYPE_GENERAL;

		// FindSpecialDrugShortageRule is basically a simplified version of FIND_DRUG_EXTENSION_row,
		// customized for this specific situation.
		ExecSQL	(	MAIN_DB, FindSpecialDrugShortageRule,
					&ShortageRuleNumber,		&ShortageRulePriceCode,			&ShortageRuleFixedPrice,
					&ShortageRuleInsuranceType,	&ShortageRuleHealthBasket,

					&DrugCode,					&Member->age_months,			&Member->age_months,
					&Member->sex,				&v_MaccabiPharmacy,				&v_MaccabiPharmacy,
					&SelectTreatmentCategory,	&DrugShortageRuleConfAuthority,	&Member->MemberMaccabi,
					&Member->MemberHova,		&Member->MemberKeva,			&Member->insurance_type,
					&Member->current_vetek,		&Member->prev_insurance_type,	&Member->prev_vetek,
					END_OF_ARG_LIST																				);

		// If we found a special drug-shortage rule, use it to override the
		// normal values from the ishur.
		if (SQLCODE == 0)
		{
			presdrugs->ret_part_source.table			= DRUG_SHORTAGE_RULE;
			presdrugs->rule_number						= ShortageRuleNumber;
			presdrugs->RetPartCode						= ShortageRulePriceCode;

			// Assign fixed-price participation only for Price Code 1 (= 100%)
			// and there is in fact a fixed price for the special shortage rule.
			// Note that if the special drug-shortage rule does *not* assign a
			// fixed price, we need to explicitly set the drug's fixed price to
			// zero, since the ishur may have set a fixed price that the rule
			// overrides.
			if ((ShortageRulePriceCode == 1) && (ShortageRuleFixedPrice > 0))
			{
				presdrugs->PriceSwap = ShortageRuleFixedPrice;
			}
			else
			{
				presdrugs->PriceSwap = 0;	// In case the ishur had a fixed price that we need to override.
			}

			// Assign the numeric insurance-used value from the alpha code in the "nohal".
			switch (*ShortageRuleInsuranceType)
			{
				case 'B':	presdrugs->ret_part_source.insurance_used = BASIC_INS_USED;		break;
				case 'K':	presdrugs->ret_part_source.insurance_used = KESEF_INS_USED;		break;
				case 'Z':	presdrugs->ret_part_source.insurance_used = ZAHAV_INS_USED;		break;
				case 'Y':	presdrugs->ret_part_source.insurance_used = YAHALOM_INS_USED;	break;
				default:	presdrugs->ret_part_source.insurance_used = BASIC_INS_USED;
			}

			presdrugs->in_health_pack					= ShortageRuleHealthBasket;

			// Reminder: At least for now, we're leaving the 29g value alone here.
//			presdrugs->rule_needs_29g					= ?????;

		}	// We found a special drug-shortage rule to apply.

		else 
		// Row-not-found is perfectly legitimate here - but we do want to log other database errors.
		// (However, at least for now, we won't do anything other than write such errors to the log.)
		if (!SQLERR_code_cmp (SQLERR_not_found))
		{
			SQLERR_error_test ();
		}

	}	// We have an ishur with tikra, and the current drug purchase is against a
		// previous-calendar-month partially-filled prescription, so we needed to
		// check for a special drug-shortage rule.

	// DonR 18May2025 User Story #388766 end.


	// DonR 11Dec2017 CR #12458: Save the Ishur Start Date so the "regular" bakara
	// kamutit routine has access to it for Limit Types 3 and 4.
	// Marianna 13Aug2020 CR #28605: If the current ishur is a continuation of a previous
	// ishur, save the starting date of the previous ishur instead of the current one.
	presdrugs->IshurStartDate = (cont_approval_flag) ? v_QtyLim_hist_start : TreatmentStart;

	// DonR 28Mar2007: If an ishur was found with "Tikra Flag" set, tell calling routine.
    if (IshurWithTikra_out != NULL)
		*IshurWithTikra_out = (v_tikra_flag > 0) ? 1 : 0;


	if ((TypeCheckSpecPr > 0) && (confnum != NULL))  //by memberid or not really number return real values if found
	{
		if ( (	(*confnum ==  1)		||
				(*confnum ==  2)		||
				(*confnum ==  3)		||
				(*confnum ==  4)		||
				(*confnum ==  5)		||
				(*confnum ==  6)		||
				(*confnum ==  8)		||
				(*confnum ==  9)		||
				(*confnum == 11)		||
				(*confnum == 12)		||
				(*confnum == 13)		||
				(*confnum == 14)		||
				(*confnum == 15)		||
				(*confnum == 16)		||
				(*confnum == 77777777)	||
				(*confnum == 99999999)		) &&
			 (TypeCheckSpecPr == 3) )
		{
			//			printf ("\n 1003  [%d]t[%d]",*confnum,TypeCheckSpecPr);fflush(stdout);
		}
		else
		{
		 *confnum		= ConfNum;				//20020120
		 *confnumsrc	= ishur_source_send;	// DonR 01May2012: Use processed version, equal to 1 or 3
												// depending on Confirming Authority.
		}
	}

	presdrugs->SpecPrescNum			= ConfNum;				//20020428
	presdrugs->SpecPrescNumSource	= ishur_source_send;	// DonR 01May2012: Use processed version, equal to 1 or 3
															// depending on Confirming Authority.


	RESTORE_ISOLATION;
    return 0;
}
// End of test_special_prescription ()



int	determine_actual_usage	(	TPresDrugs						*DrugArrayPtr,
								int								NumDrugs,
								short							ThisDrugSubscript,
								short							NumTreatmentDays,
								short							IngredientCode,
								double							IngrAmountPerDay,
								short							TreatmentDaysPerCycle
							)
{
	// Local declarations.
	int			DrugNum;
	int			Day;
	int			Ingr;
	int			UnitsSold;
	int			NumRelevantDrugs;
	short		DidSomething;
	double		TotalBought;
	double		TotalUsed;
	double		TotalDiscarded;
	TPresDrugs	*DRUG;

	typedef struct
	{
		int		DrugArraySubscript;
		long	total_units;
		long	fully_used_units;
		long	partly_used_units;
		long	available_units;
		double	lim_ingr_per_unit;
		double	partial_unit_used;
		double	lim_ingr_discarded;
	}
	SUPPLY_t;

	SUPPLY_t	SUPPLY	[MAX_REC_ELECTRONIC];


	typedef struct
	{
		double	daily_ingr_dose;
		double	dose_still_unsupplied;
	}
	DEMAND_t;

	DEMAND_t	DEMAND	[365];	// Asssume that we'll never be buying for more than a year;
								// in reality it should never be more than 90 days. 


	// Initialize and then populate DEMAND and SUPPLY arrays.
	memset ((char *)DEMAND, 0, sizeof(DEMAND));
	memset ((char *)SUPPLY, 0, sizeof(SUPPLY));
	NumRelevantDrugs = 0;

	// Prevent array overflow - although this really shouldn't be necessary.
	if (NumTreatmentDays > 365)
		NumTreatmentDays = 365;

	for (Day = 0; Day < NumTreatmentDays; Day++)
	{
		DEMAND[Day].daily_ingr_dose = DEMAND[Day].dose_still_unsupplied = IngrAmountPerDay;
	}

	// This routine will be called only once for any given limit ingredient; and we'll start
	// scanning the drug array from the point where that ingredient first appears.
	for (DrugNum = ThisDrugSubscript; DrugNum < NumDrugs; DrugNum++)
	{
		for (Ingr = 0; Ingr < 3; Ingr++)
		{
			// Skip anything that isn't the ingredient we're testing for.
			if (DrugArrayPtr[DrugNum].DL.Ingredient[Ingr].code == IngredientCode)
			{
				// DonR 30Sep2013: The calling routine calculates and copies values for course_len, course_treat_days,
				// calc_courses, and calc_treat_days - but only for the first drug found relevant to a particular
				// ishur. We can (I hope!) safely assume that the values from the ishur will be the same for all
				// relevant drugs, but the duration may be different. Thus we need to copy what's copy-able, and
				// calculate calc_courses and calc_treat_days, for subsequent drugs.
				if (DrugNum > ThisDrugSubscript)
				{
					DrugArrayPtr[DrugNum].course_len_days	= DrugArrayPtr[ThisDrugSubscript].course_len;
					DrugArrayPtr[DrugNum].course_treat_days	= DrugArrayPtr[ThisDrugSubscript].course_treat_days;

					DrugArrayPtr[DrugNum].calc_courses		= DrugArrayPtr[DrugNum].Duration / DrugArrayPtr[DrugNum].course_len_days;
					if (DrugArrayPtr[DrugNum].calc_courses	< 1)
						DrugArrayPtr[DrugNum].calc_courses	= 1;	// Assume that drugs are supplied for at least one course.

					// Note that the calling routine has already set treatment days per cycle to one if it's zero
					// in the ishur. However, I'll leave the logic here as well, just for good (i.e. paranoid)
					// programming practice.
					if (TreatmentDaysPerCycle < 1)
						TreatmentDaysPerCycle = 1;	// Assume at least one treatment day per course.
					DrugArrayPtr[DrugNum].calc_treat_days	= DrugArrayPtr[DrugNum].calc_courses * TreatmentDaysPerCycle;
				}	// Not the first drug relevant to this ishur, so we need to copy and calculate stuff.

				// Units Sold and Amount Per Unit are calculated differently for drugs with
				// a Package Size of 1.
				if (DrugArrayPtr[DrugNum].DL.package_size > 1)
				{
					UnitsSold = (DrugArrayPtr[DrugNum].Op * DrugArrayPtr[DrugNum].DL.package_size) + DrugArrayPtr[DrugNum].Units;

					SUPPLY [NumRelevantDrugs].lim_ingr_per_unit	= DrugArrayPtr[DrugNum].DL.Ingredient[Ingr].quantity_std;
				}
				else
				{
					UnitsSold = DrugArrayPtr[DrugNum].Op;

					SUPPLY [NumRelevantDrugs].lim_ingr_per_unit	= DrugArrayPtr[DrugNum].DL.package_volume
																* DrugArrayPtr[DrugNum].DL.Ingredient[Ingr].quantity_std
																/ DrugArrayPtr[DrugNum].DL.Ingredient[Ingr].per_quant;
				}

				SUPPLY [NumRelevantDrugs].DrugArraySubscript	= DrugNum;
				SUPPLY [NumRelevantDrugs].total_units			= UnitsSold;
				SUPPLY [NumRelevantDrugs].available_units		= UnitsSold;
				SUPPLY [NumRelevantDrugs].fully_used_units		= 0;
				SUPPLY [NumRelevantDrugs].partly_used_units		= 0;
				SUPPLY [NumRelevantDrugs].lim_ingr_discarded	= 0.0;

				// We've now populated an element of the SUPPLY array, so increment the array-size counter.
				NumRelevantDrugs++;

				break;	// Load SUPPLY array only once for any given drug purchased; this breaks
						// out of the ingredient loop so we move on to the next drug purchased.

			}	// Found the ingredient we're looking for.
		}	// Loop through ingredients for drug purchased.
	}	// Loop through drugs purchased.

	// Just in case something very, very strange has happened, exit immediately if there are
	// no relevant drugs.
	if (NumRelevantDrugs < 1)
		return (-1);

	// Sort the SUPPLY array by size - largest units first.
	qsort (SUPPLY, (size_t)NumRelevantDrugs, sizeof (SUPPLY_t), SUPPLY_compare);


	// At this point, we have a DEMAND array with no demand supplied, and a SUPPLY array, sorted
	// with the largest units first, waiting to be allocated to fill the demand. Onwards!

	// Note that this ampoule-allocating algorithm does NOT guarantee maximal usage efficiency
	// in all cases. For example, let's say that the daily dosage is 150 mg., and ampoules of
	// 100 mg. and 75 mg. are being bought. The most efficient way to deliver the required dose
	// would be to use two 75-mg. ampoules per day; but because we allocate full ampoules first,
	// working from larger to smaller dosages, we would supply each day's dose as one 100-mg.
	// ampoule plus 2/3 of a 75-mg. ampoule, discarding 25 mg.-worth. In real life, this is
	// unlikely to present any problems; and an algorithm smart enough to minimize ingredient
	// wastage in all cases would also be smart enough to take over the world and enslave us all.


	// DonR 30Sep2013: First, try to fill each day's demand with the smallest available unit that
	// will completely fill that demand. This should (almost) ensure that the least possible amount
	// of medication is designated as to-be-discarded.
	for (Day = DidSomething = 0; Day < NumTreatmentDays; Day++)
	{
		for (DrugNum = NumRelevantDrugs - 1; DrugNum >= 0 ; DrugNum--)
		{
			if ((SUPPLY[DrugNum].available_units	< 1)								||
				(SUPPLY[DrugNum].lim_ingr_per_unit	< DEMAND [Day].daily_ingr_dose))
			{
				continue;	// A single unit of this drug is not available/adequate to meet this day's full demand.
			}

			// Allocate either a full or a partial unit to fill this day's demand.
			if (SUPPLY [DrugNum].lim_ingr_per_unit > DEMAND [Day].daily_ingr_dose)
			{
				// This is the smallest available unit that supplies MORE than a full daily dose.
				SUPPLY [DrugNum].partly_used_units++;
				SUPPLY [DrugNum].lim_ingr_discarded	+= (SUPPLY [DrugNum].lim_ingr_per_unit - DEMAND [Day].daily_ingr_dose);
				SUPPLY [DrugNum].partial_unit_used	+= (DEMAND [Day].daily_ingr_dose / SUPPLY [DrugNum].lim_ingr_per_unit);
			}
			else
			{
				// This unit supplies EXACTLY a full daily dose.
				SUPPLY [DrugNum].fully_used_units++;
			}

			DidSomething = 1;							// We filled this day's demand, so we need to check additional days.
			DEMAND [Day].dose_still_unsupplied = 0.0;	// Demand is fully satisfied for this day.
			SUPPLY [DrugNum].available_units--;
			break;										// We've fully met this day's demand - onward to the next day!

		}	// Loop through drugs in ascending unit size to find single units adequate to meet a full day's demand.

		// If we didn't find any single unit to meet a day's demand, there's no point in checking additional days;
		// just move on to the next loops to assign multiple units. (Note that this check is done just to improve
		// execution time by a few machine cycles - particularly by avoiding unnecessary floating-point operations.)
		if (!DidSomething)
			break;

	}	// Loop through treatment days.


	// Second, allocate full units, starting with the largest. Note that in order for this loop
	// to do anything, there must be unmet demand for at least one treatment day that can't be
	// supplied by a single unit - otherwise this demand would have been supplied in the
	// previous loop. Thus we will supply at least one day's demand using more than one unit -
	// either two or more full units or a combination of full and partial units.
	for (DrugNum = 0; DrugNum < NumRelevantDrugs; DrugNum++)
	{
		// We're done with this drug when we run out of units, or when we run out of "space" to
		// use a unit. In either case, DidSomething will be 0 at the end of the loop on
		// treatment days.
		do
		{
			for (Day = DidSomething = 0;
				 (Day < NumTreatmentDays) && (SUPPLY[DrugNum].available_units > 0);
				 Day++)
			{
				if (DEMAND [Day].dose_still_unsupplied >= SUPPLY [DrugNum].lim_ingr_per_unit)
				{
					// There is "room" for a full unit of this drug on this day.
					SUPPLY [DrugNum].fully_used_units++;
					SUPPLY [DrugNum].available_units--;
					DEMAND [Day].dose_still_unsupplied -= SUPPLY [DrugNum].lim_ingr_per_unit;
					DidSomething = 1;
				}	// Allocated a full ampoule to a treatment day.
			}	// Loop through treatment days.

			// When we get here, we've completed one pass through the treatment days. If we
			// allocated at least one full ampoule to at least one treatment day, DidSomething
			// will be non-zero - so we'll execute the Day loop again.
		}
		while (DidSomething);

		// When we get here, we've finished allocating full units of the current drug; so
		// it's time to proceed with the next smaller-dosage drug.
	}	// Loop through drugs in descending limit-ingredient dosage.


	// Third, allocate partial units, working "backwards" from smallest to largest dosage.
	// Again, this loop does anything ONLY IF there is demand that can't be fully met by
	// using a single unit or multiple full units; that is, at least one day's demand  has
	// to be met by using at least one full unit plus one partial unit.
	for (DrugNum = NumRelevantDrugs - 1; DrugNum >= 0 ; DrugNum--)
	{
		// We don't need the "while" loop here as we did in the full-unit allocation section,
		// since it is impossible to need more than one partial ampoule on any given treatment
		// day.
		for (Day = 0;
			 (Day < NumTreatmentDays) && (SUPPLY[DrugNum].available_units > 0);
			 Day++)
		{
			if (DEMAND [Day].dose_still_unsupplied > 0.0)
			{
				// There is "room" for a partial unit of this drug on this day. Note that
				// we know there isn't room for a full unit, because that would have been
				// taken care of in the full-units loop above.
				SUPPLY [DrugNum].partly_used_units++;
				SUPPLY [DrugNum].available_units--;

				// The amount discarded from this unit is the size of the unit MINUS what
				// is actually going to be used. Partial_unit_used will be used to compute the
				// average fractional usage of partial units.
				SUPPLY [DrugNum].lim_ingr_discarded	+= (SUPPLY [DrugNum].lim_ingr_per_unit - DEMAND [Day].dose_still_unsupplied);
				SUPPLY [DrugNum].partial_unit_used	+= (DEMAND [Day].dose_still_unsupplied / SUPPLY [DrugNum].lim_ingr_per_unit);

				DEMAND [Day].dose_still_unsupplied = 0.0;
			}	// Allocated a partial unit to a treatment day.
		}	// Loop through treatment days.
	}	// Loop through drugs in ascending limit-ingredient dosage.


	// When we get here, we have allocated full and partial units reasonably efficiently (but
	// not necessarily optimally - see comment above!) to the required daily doses. Now we can
	// calculate how much of each drug will actually be discarded versus how much will be used.
	for (DrugNum = 0; DrugNum < NumRelevantDrugs; DrugNum++)
	{
		DRUG = &DrugArrayPtr [SUPPLY [DrugNum].DrugArraySubscript];

		DRUG->actual_usage_checked	= 1;
		DRUG->total_units			= SUPPLY [DrugNum].total_units;
		DRUG->fully_used_units		= SUPPLY [DrugNum].fully_used_units;
		DRUG->partly_used_units		= SUPPLY [DrugNum].partly_used_units;

		// If this drug comes in an openable package, then the pharmacy is supposed to sell only
		// the exact number of units required. On the other hand, if the package is not
		// openable, we assume that the member is responsible for giving leftover medicine to
		// his/her dog.
		// Note that if a package is completely superfluous (i.e. none of its contents are required)
		// we don't want to assume that the member is buying it just to throw it all out!
		// DonR 30Sep2013: If package size is 1, don't discard any units.
		if ((DRUG->DL.openable_pkg)											||
			((DRUG->fully_used_units + DRUG->partly_used_units) == 0)		||
			(DRUG->DL.package_size == 1))
		{
			DRUG->discarded_units = 0;
		}
		else
		{
			// DonR 30Sep2013: If package size is greater than one, don't discard any full
			// packages - just the leftover units from a single opened package. This is done
			// by using the "modulo" (%) operator.
			DRUG->discarded_units = SUPPLY [DrugNum].available_units % DRUG->DL.package_size;
		}

		// DRUG->avg_partial_unit will have the actual fractional usage of partly-used ampoules.
		if (SUPPLY [DrugNum].partly_used_units > 0)
			DRUG->avg_partial_unit		= SUPPLY [DrugNum].partial_unit_used / SUPPLY [DrugNum].partly_used_units;
		else
			DRUG->avg_partial_unit		= 0.0;

		// Compute the total fractional usage of the drug being bought, net of partially-used
		// ampoules and discarded ampoules.
		TotalBought					=		((double)SUPPLY [DrugNum].total_units	* SUPPLY [DrugNum].lim_ingr_per_unit);

		TotalDiscarded				=		((double)DRUG->discarded_units			* SUPPLY [DrugNum].lim_ingr_per_unit)
										+	SUPPLY [DrugNum].lim_ingr_discarded;

		TotalUsed					=		TotalBought - TotalDiscarded;

		DRUG->proportion_used		=		(TotalBought > 0) ? TotalUsed / TotalBought : 0.0;
		DRUG->net_lim_ingr_used		=		TotalUsed;
	}

	return (0);
}


int SUPPLY_compare (const void *data1, const void *data2)
{
	// SUPPLY structure
	typedef struct
	{
		int		DrugArraySubscript;
		long	total_units;
		long	fully_used_units;
		long	partly_used_units;
		long	available_units;
		double	lim_ingr_per_unit;
		double	partial_unit_used;
		double	lim_ingr_discarded;
	}
	SUPPLY_t;

	SUPPLY_t *Supply_1 = (SUPPLY_t *)data1;
	SUPPLY_t *Supply_2 = (SUPPLY_t *)data2;


	// Because we want to sort by DESCENDING unit size, we want to return a negative value
	// (meaning that Element 1 gets sorted before Element 2) if the first of the two array
	// elements has a higher per-unit dosage.
	return (Supply_2->lim_ingr_per_unit - Supply_1->lim_ingr_per_unit);
}



/*=======================================================================
||																		||
||			ReadDrugPurchaseLimData_x ()								||
||																		||
|| Read drug purchase limits - "regular" or from ishur.					||
|| Returns:																||
|| 0 = OK.																||
|| 1 = high severity error occurred.									||
||																		||
||	NOTE: This function is called via two different macros:				||
|||	ReadDrugPurchaseLimitData() is the "normal" call, and				||
||	ReadNonIshurPurchaseLimitMethods() reads only limits that do not	||
||	involve ishurim (i.e. 0/1, 5/6, 7/8)								||
 =======================================================================*/
TErrorCode	ReadDrugPurchaseLimData_x (TMemberData			*Member_in,
									   TPresDrugs			*CurrSaleDrugArray_in,
									   short				CurrSaleDrugLineNumber_in,
									   short				CurrSaleDrugLineCount_in,
									   TDrugsBuf			*BloodDrugsBuf_in,
									   short				RegularMethodsOnly,
									   short				ReadRegularLimitsForOTC,
									   DRUG_PURCHASE_LIMIT	*PurchaseLimit_out,
									   TErrorCode			*ErrorCode_out,
									   int					*reStart_out)
{
	TMemberData				Member			= *Member_in;
	TPresDrugs				*ThisDrug		= &CurrSaleDrugArray_in	[CurrSaleDrugLineNumber_in];
	int						Largo			= CurrSaleDrugArray_in[CurrSaleDrugLineNumber_in].DrugCode;
	short					PrescSource		= CurrSaleDrugArray_in[CurrSaleDrugLineNumber_in].PrescSource;
	DRUG_PURCHASE_LIMIT		PurchaseLimit;
	DRUG_PURCHASE_LIMIT		Ishur;
	int						i;
	int						SysDate;
	int						MinimumPurchDate;
	int						PL_CloseDate_Min;
	short					MemberAgeYears;
	short					ThisIngredient;
	short					FoundSomething			= 0;	// NOT a boolean - we also use -1 as an error value.
	bool					FoundMethodTwo			= 0;
	short OTC_sale									= 0;	// Default = *with* prescription : Marianna 14Sep2023 BUG FIX #480141
	ISOLATION_VAR;


	// DonR 11Dec2017: Documentation of Purchase Limit Methods (drug_purchase_lim.limit_method,
	// also stored in SPres.PurchaseLimitMethod).
	//
	// 0 = "Old" method: test limits by units purchased.
	// 1 = "New" method: test limits by ingredient usage.
	// 2 = Prescription Source permission control. (No such rules exist in Production as of 11Dec2017.)
	// 3 = Fertility drugs using start/end dates taken from AS/400 Ishur - test limits by units bought.
	// 4 = Fertility drugs using start/end dates taken from AS/400 Ishur - test limits by ingredient usage.
	// 5 = Test limits by units purchased, looking *only* at a specific Prescription Source.
	// 6 = Test limits by ingredient usage, looking *only* at a specific Prescription Source.
	// 7 = Test by units purchased at the current pharmacy with a specific Prescription Source; this
	//     method uses duration_months as a number of *days* - normally 1 for today's purchases.
	// 8 = Test by ingredient amount purchased at the current pharmacy with a specific Prescription Source;
	//     this method uses duration_months as a number of *days* - normally 1 for today's purchases.


	// Initialization.
	REMEMBER_ISOLATION;
	SET_ISOLATION_DIRTY;

	memset (&PurchaseLimit,	0, sizeof(PurchaseLimit));
	memset (&Ishur,			0, sizeof(Ishur));

	SysDate				= GetDate ();
	PL_CloseDate_Min	= AddDays (SysDate, 5);

	// DonR 28Dec2015: Member's age in years ticks up *on* his/her birthday - so the comparison
	// below needs to change from "<=" to "<". Note that (at least for now) we are not worrying
	// about members born on 29 February buying drugs on 28 February of a non-leap year. As
	// Gilbert and Sullivan would tell us, this is a paradox!
	MemberAgeYears	= (SysDate / 10000) - (Member.BirthDate / 10000);
	if ((SysDate % 10000) < (Member.BirthDate % 10000))
		MemberAgeYears--;	// Member hasn't reached his/her birthday this year.

	// Marianna 14Sep2023 BUG FIX #480141
	// Limit methods 0,1,3,4  should be invisible for sale without a prescription,
	// because these limits apply to prescription sales and there is no need to check them.
	// DonR 11Feb2024 BUG FIX #480141 revision: In some cases, someone (by mistake?) creates
	// a "nohal" that gives discounts even on OTC sales. In this case, we *do* want to check
	// purchase limits. Since at the time ReadDrugPurchaseLimitData() is called in "normal"
	// mode we don't yet know whether the purchase is going to be at full price, we go ahead
	// and suppress Limit Methods 0, 1, 3, and 4; but then in test_purchase_limits(), if we
	// see that an OTC sale *is* being made at a discount, we'll come back with a call to
	// ReadPurchaseLimitsAllMethods() that will set ReadRegularLimitsForOTC TRUE and thus
	// read the "traditional" limits even for the OTC purchase.
	if ((PrescSource == RECIP_SRC_NO_PRESC || PrescSource == RECIP_SRC_EMERGENCY_NO_RX) && !ReadRegularLimitsForOTC)
	{
		OTC_sale = 1; // *without* prescription 
	}

	// First, see if there is a "standard" drug purchase limit applicable to this member/drug.
	// DonR 13Dec2016: We want to look at both Method 2 and Method 0/1 rows - Method 2 tells us
	// whether the sale is authorized for this member/drug/prescription source regardless of
	// quantities, while Methods 0 and 1 are "conventional" quantity limits.
	// DonR 07Aug2017 CR #12458: Add Purchase Limit Methods 3 & 4 (fertility drugs using start/end dates
	// taken from AS/400 Ishur) and change the ORDER BY to use a CASE construction.
	// DonR 24May2018 CR#13948: Add new Purchase Limit Methods 5 and 6, for special limits that apply only
	// to a specific Prescription Source. These will serve a double purpose: (A) they'll authorize the
	// sale in the same way a Method 2 limit can; and (B) they'll provide a quantity limit the way
	// Method 0/1 limits do. Because they're more specific, they'll sort ahead of Method 0/1.
	// 09Dec2021 NOTE: It looks like in fact Method 0/1 sorts *ahead* of Method 5/6. I'm not sure
	// this is a real problem - as far as I know, nobody is using both types of limit on the same
	// Largo Codes. At some point this should be discussed - is there some control in place to prevent
	// people from configuring multiple, contradictory limits on the same items?
	// DonR 05Dec2021: Add new Purchase Limit Methods 7 and 8. These work similarly to 5 and 6, but
	// the limit will apply only to purchases at the current pharmacy, and the time span is in days
	// (normally just today's purchases) rather than months.
	// DonR 16Nov2025 User Story #453336: Get 3 new columns for Libre sensor logic.
	DeclareAndOpenCursorInto (	MAIN_DB, ReadDrugPurchaseLim_FindDrugPurchLimit_cur,
								&PurchaseLimit.largo_code,					&PurchaseLimit.class_code,
								&PurchaseLimit.qty_lim_grp_code,			&PurchaseLimit.limit_method,
								&PurchaseLimit.max_units,					&PurchaseLimit.ingr_qty_std,
								&PurchaseLimit.presc_source,				&PurchaseLimit.permit_source,

								&PurchaseLimit.warning_only,				&PurchaseLimit.ingredient_code,
								&PurchaseLimit.exempt_diseases,				&PurchaseLimit.duration_months,
								&PurchaseLimit.history_start_date,			&PurchaseLimit.MonthIs28Days,
								&PurchaseLimit.class_type,					&PurchaseLimit.class_history_days,

								&PurchaseLimit.NumQualifyingPurchases,		&PurchaseLimit.CustomErrorCode,
								&PurchaseLimit.include_full_price_sales,	&PurchaseLimit.no_ishur_for_pharmacology,
								&PurchaseLimit.no_ishur_for_treatment_type,	&PurchaseLimit.prev_sales_span_days,

								&Largo,										&PrescSource,
								&RegularMethodsOnly,						&Member.sex,
								&MemberAgeYears,							&MemberAgeYears,
								&OTC_sale,																		// Marianna 14Sep2023 BUG FIX #480141
								END_OF_ARG_LIST																	);

	if (SQLERR_error_test ())
	{
		*ErrorCode_out = ERR_DATABASE_ERROR;
		FoundSomething	= -1;
	}

	else	// Successful cursor open.
	{
		// Fetch data and store in buffer.
		for (FoundSomething = FoundMethodTwo = 0; ; )
		{
			FetchCursor (	MAIN_DB, ReadDrugPurchaseLim_FindDrugPurchLimit_cur	);

			if (SQLERR_code_cmp (SQLERR_end_of_fetch) == MAC_TRUE)
			{
				FoundSomething = 0;	// We got to the end of the cursor without finding a match.
			}
			else
			{	// Anything other than end-of-fetch.
				if (SQLERR_code_cmp (SQLERR_access_conflict) == MAC_TRUE)
				{
					SQLERR_error_test ();
					FoundSomething = -1;
					*reStart_out = MAC_TRUE;
				}
				else
				{	// Either some other database error, or else a successful fetch.
					if (SQLERR_error_test ())
					{
						*ErrorCode_out = ERR_DATABASE_ERROR;
						FoundSomething	= -1;
					}
				}	// Not an access conflict.
			}	// Not end-of-fetch.

			// Any non-zero SQLCODE means we did *not* succeed in FETCHing a row - so break out of the reading loop.
			if (SQLCODE != 0)
				break;

			// If we get here, the FETCH succeeded - although we don't yet know if the row FETCHed is actually relevant.

			// Sanity check: The minimum value for PurchaseLimit.NumQualifyingPurchases is 1,
			// since there's no point in having a prior-purchase requirement of fewer than
			// one sale date. And we don't want to throw run-time errors trying to dimension
			// an array of length < 1!
			// DonR 08Feb2022: Added sanity checking for PurchaseLimit.CustomErrorCode as well.
			// For this purpose, any 4-digit number is acceptable.
			if (PurchaseLimit.NumQualifyingPurchases < 1)
				PurchaseLimit.NumQualifyingPurchases = 1;

			if ((PurchaseLimit.CustomErrorCode < 1000) || (PurchaseLimit.CustomErrorCode > 9999))
				PurchaseLimit.CustomErrorCode = 0;

			// DonR 13Dec2016: If we've already found an applicable Method 2 limit, skip any
			// remaining Method 2 limits in the SELECT list. Note that an applicable Method 5/6
			// or 7/8 limit also authorizes the sale (subject to quantities), and will set
			// FoundMethodTwo TRUE.
			if ((FoundMethodTwo) && (PurchaseLimit.limit_method == 2))
				continue;

			// If we get here, we've managed to read a purchase limit row for the drug being bought
			// that conforms to any relevant demographic criteria. However, if the purchase limit
			// class for this limit is of type 2, we also have to qualify the limit based on whether
			// the member has bought drugs of the same (non-zero) class as the purchase limit (e.g.
			// insulin for purchase limits on blood-sugar diagnostic strips).
			// Note that if someone creates a Type 2 class with Class Code of zero, it will be ignored!
			// DonR 07Feb2022: Added a new "number of qualifying purchases required" column to drug_purchase_limit.
			// We count the number of unique purchase dates of qualifying purchases (including today), and only if
			// the number is at least equal to PurchaseLimit.NumQualifyingPurchases do we qualify the member for
			// this Purchase Limit Class.
			// DonR 17Nov2025 User Story #453336: Additional functionality for Libre blood-sugar sensors.
			// We can now test for a minimum date range between first historical purchase and most recent
			// purchase of a qualifying drug; and we can also test for the NON-existence of an ishur for
			// an alternative product (e.g. a continuous blood-sugar monitoring device).
			if ((PurchaseLimit.class_type == 2) && (PurchaseLimit.class_code > 0))
			{
				int		QualifyingPurchaseDate [PurchaseLimit.NumQualifyingPurchases];	// OK in C, but maybe not in C++. I can live with that...
				int		NumQualifyingPurchasesFound			= 0;
				int		UniqueDate;
				int		FirstPurchaseDate					= 99999999;
				int		LastPurchaseDate					= 20220101;
				bool	QualifyingPurchaseAlreadyMadeToday	= false;
				bool	DisqualifyingIshurExists			= false;

				MinimumPurchDate = IncrementDate (SysDate, (0 - PurchaseLimit.class_history_days));

				for (i = 0; i < BloodDrugsBuf_in->max; i++)
				{
					if ((BloodDrugsBuf_in->Durgs[i].ClassCode	== PurchaseLimit.class_code)	&&
						(BloodDrugsBuf_in->Durgs[i].StartDate	>= MinimumPurchDate))
					{
						// DonR 17Nov2025 User Story #453336: Update the first-sale and
						// most-recent-sale dates as necessary.
						if (BloodDrugsBuf_in->Durgs[i].StartDate < FirstPurchaseDate)
						{
							FirstPurchaseDate = BloodDrugsBuf_in->Durgs[i].StartDate;
						}

						// Ignore bogus future dates - which shouldn't happen anyway.
						if ((BloodDrugsBuf_in->Durgs[i].StartDate <= SysDate)	&&
							(BloodDrugsBuf_in->Durgs[i].StartDate > LastPurchaseDate))
						{
							LastPurchaseDate = BloodDrugsBuf_in->Durgs[i].StartDate;
						}

						// If we see a qualifying past purchase that took place today, we don't
						// need to bother checking the current sale request, since we're counting
						// unique purchase dates rather than just counting drug sale transactions.
						if (BloodDrugsBuf_in->Durgs[i].StartDate == SysDate)
						{
							QualifyingPurchaseAlreadyMadeToday = true;
						}

						// DonR 07Feb2022: Add new logic that can require more than one past purchase
						// to qualify a member for a particular limit. To avoid chicanery, count the
						// number of unique purchase *dates* rather than just *purchases*.
						// First, loop through the Unique Purchase Date array to see if we already
						// found something for that date.
						// DonR 24Dec2025 BUG FIX: We don't want to count unique purchase dates
						// beyond what we need - especially since the QualifyingPurchaseDate array
						// is dimensioned only to the minimum size we're interested in, and with
						// the latest changes we sometimes keep looping through the member's
						// complete purchase history to find the minimum and maximum qualifying
						// purchase dates. This was causing us to write beyond the array boundary,
						// and when the excess was big enough (around 8 "extra" dates) the program
						// crashed with a segmentation fault.
						if (NumQualifyingPurchasesFound < PurchaseLimit.NumQualifyingPurchases)
						{
							// See if this purchase date is one we already know about.
							for (UniqueDate = 0; UniqueDate < NumQualifyingPurchasesFound; UniqueDate++)
							{
								if (QualifyingPurchaseDate [UniqueDate] == BloodDrugsBuf_in->Durgs[i].StartDate)
									break;
							}

							// If we got to the end of the previous loop with no matches, store this date
							// and increment our dates-found counter.
							if (UniqueDate == NumQualifyingPurchasesFound)
							{
								QualifyingPurchaseDate [NumQualifyingPurchasesFound++] = BloodDrugsBuf_in->Durgs[i].StartDate;
							}
						}	// We have not yet found the minimum number of unique previous-purchase dates. (DonR 24Dec2025 BUG FIX)

						// Keep looping until the end, unless we've already seen enough unique
						// purchase dates to qualify the member for this Purchase Limit Class.
						// DonR 17Nov2025 User Story #453336: Do *not* terminate the loop even
						// if we've found enough qualifying purchases, for situations where
						// we're also looking at the date range of past purchases.
						// DonR 23Dec2025 BUG FIX: The condition for breaking out of the historical-purchase
						// loop early should be that PurchaseLimit.prev_sales_span_days is EQUAL to zero
						// (or less than 1), NOT that it's greater than zero! If it's greater than zero,
						// we do need to look at the member's full purchase history to make sure we're getting
						// an accurate minimum time span between the earliest and latest qualifying purchase.
						if ((NumQualifyingPurchasesFound		>= PurchaseLimit.NumQualifyingPurchases)	&&
							(PurchaseLimit.prev_sales_span_days <  1))
						{
							break;
						}
					}	// Found a qualifying past sale.
				}	// Loop through array of past purchases.

				// DonR 06Jun2010: We need to check current sale for qualifying purchase, not just past sales.
				// DonR 07Feb2022: No point in checking the current purchase unless (A) we haven't
				// already found a qualifying purchase made today, and (B) we're exactly one purchase
				// date shy of qualifying for this Purchase Limit Class.
				if ((NumQualifyingPurchasesFound == (PurchaseLimit.NumQualifyingPurchases - 1)) && (!QualifyingPurchaseAlreadyMadeToday))
				{
					for (i = 0; i < CurrSaleDrugLineCount_in; i++)
					{
						if (CurrSaleDrugArray_in[i].DL.class_code == PurchaseLimit.class_code)
						{
							NumQualifyingPurchasesFound++;
							LastPurchaseDate = SysDate;
							break;
						}
					}
				}	// Not enough qualifying purchases from drugs-in-blood, so we need to check current sale.

//if (PurchaseLimit.NumQualifyingPurchases > 0)
//GerrLogMini (GerrId, "%d/%d qualifying purchases found, from %d to %d. Minimum span = %d, actual span = %d.",
//					NumQualifyingPurchasesFound, PurchaseLimit.NumQualifyingPurchases,
//					FirstPurchaseDate, LastPurchaseDate,
//					PurchaseLimit.prev_sales_span_days,
//					GetDaysDiff (LastPurchaseDate, FirstPurchaseDate) );
//
				// If member has not purchased a qualifying drug for this Purchase Limit Class, skip
				// to the next row in the cursor.
				if (NumQualifyingPurchasesFound < PurchaseLimit.NumQualifyingPurchases)
				{
					continue;	// Meaning that the line below setting FoundSomething and PurchaseLimit.stat will not execute.
				}

				// DonR 17Nov2025 User Story #453336: There are now two optional additional tests:
				// 1) Did a minimum of PurchaseLimit.prev_sales_span_days elapse between the earliest
				//    and latest sales of the qualifying drug(s)?
				// 2) Is there an active ishur for an alternative device/drug - e.g. a continuous
				//    blood-sugar monitoring device?
				// Note that if we get here, we already know that an adequate number of prior sales
				// were found - so FirstPurchaseDate and LastPurchaseDate have meaningful values.
				if ((PurchaseLimit.prev_sales_span_days					> 0)	&&
					(GetDaysDiff (LastPurchaseDate, FirstPurchaseDate)	< PurchaseLimit.prev_sales_span_days))
				{
					continue;
				}

				if (PurchaseLimit.no_ishur_for_pharmacology > 0)
				{
					ExecSql (	MAIN_DB, ReadDrugPurchaseLim_CheckForDisqualifyingIshur,
								&DisqualifyingIshurExists,
								&Member.ID,									&Member.ID_code,
								&SysDate,									&SysDate,
								&PurchaseLimit.no_ishur_for_pharmacology,	&PurchaseLimit.no_ishur_for_treatment_type,
								END_OF_ARG_LIST																				);

//GerrLogMini (GerrId, "ReadDrugPurchaseLim_CheckForDisqualifyingIshur SQLCODE = %d, "
//					"Member %d, Pharmacology %d Treat Type %d, DisqualifyingIshurExists = %d.",
//				SQLCODE, Member.ID, PurchaseLimit.no_ishur_for_pharmacology, 
//				PurchaseLimit.no_ishur_for_treatment_type, DisqualifyingIshurExists);
//
					// No real error-handling, at least for now - but log any messages,
					// in case the SQL is faulty.
					SQLERR_error_test ();

					if (DisqualifyingIshurExists)
					{
						continue;
					}
				}	// Need to check for a disqualifying ishur.

			}	// ClassType == 2 - applicability depends on one or more qualifying purchases.


			// If we get here, we've found something applicable: either the first applicable Method 2
			// limit or the first applicable Method 0/1/3/4/5/6 limit. If what we've found is a Method 2
			// limit, we're not done - we still need to look for "normal" purchase limits. Note that
			// the value we store in ThisDrug->PurchaseLimitSourceReject is the inverse of what we
			// get in the drug_purchase_lim table - this is because we initialize structures to
			// all zeroes, and we want the default to be that a sale is permitted.
			// DonR 24May2018 CR#13948: Add new Purchase Limit Methods 5 and 6, for quantity limits
			// that apply only to a specific Prescription Source.
			// DonR 05Dec2021 User Story #205423: Add new Purchase Limit Methods 7 and 8, for daily (OTC)
			// per-pharmacy limits.
			if ((PurchaseLimit.limit_method == 2)	||
				(PurchaseLimit.limit_method == 5)	||
				(PurchaseLimit.limit_method == 6)	||
				(PurchaseLimit.limit_method == 7)	||
				(PurchaseLimit.limit_method == 8))
			{
				FoundMethodTwo = 1;		// So we'll skip any remaining Method 2 limits in the SELECT list.

				if (PurchaseLimit.limit_method == 2)
				{
					// Method 2 limits can authorize or de-authorize prescription sources, depending on the permit_source column.
					ThisDrug->PurchaseLimitSourceReject = (PurchaseLimit.permit_source > 0) ? 0 : 1;
					continue;				// Keep looping to find "normal" drug purchase limits.
				}
				else
				{
					// The existence of a Method 5, 6, 7, or 8 limit implies that the prescription source *is* authorized.
					ThisDrug->PurchaseLimitSourceReject = 0;
					// ...And *don't* jump to the next limit in the SELECT cursor - we're not done with this one!
				}
			}


			// If we get here, we've actually managed to find a combination of "normal" (Method 0/1, 3/4, 5/6,
			// or 7/8) Drug Purchase Limit and Purchase Limit Class that fits this member and drug. Once we've
			// found an applicable limit, we stop searching; this means that a Method 3/4 limit makes Method 0/1,
			// 5/6, and 7/8 limits invisible, and a Method 0/1 limit makes Method 5/6 and 7/8 limits invisible.
			// In real life, we're not supposed to have multiple limit methods in place for the same Largo Code,
			// but as long as the ORDER BY logic in the cursor corresponds to the actual order of preference
			// dictated by business logic, the routine should function properly even if someone puts multiple
			// limit methods in place.
			FoundSomething = PurchaseLimit.status = 1;

			break;
		}	// Cursor-reading loop.
	}	// Cursor was opened successfully.

	CloseCursor (	MAIN_DB, ReadDrugPurchaseLim_FindDrugPurchLimit_cur	);


	// Next, if there was no applicable limit from the drug_purchase_lim table, see if we can
	// read in a non-applicable row just to get a couple of default values (like Limit Method
	// and Warning Only) that aren't stored in purchase_lim_ishur.
	// DonR 11Jun2018 CR #13948: add Method 5/6 limits to the SELECT, and ORDER BY limit_method
	// so that a Method 5/6 limit will be used to supply the "missing" fields only if there are
	// no other alternatives. Note that this should never happen in practice, since there shouldn't
	// be any quantity-limit ishurim for an item without a Method 0/1 or 3/4 limit in place.
	// DonR 05Dec2021 User Story #205423: Added new OTC one-day limit methods 7 and 8; again, there
	// should be no ishurim for these limits. Method 7/8 sorts after Method 5/6.
	if (FoundSomething < 1)
	{
		ExecSQL (	MAIN_DB, ReadDrugPurchaseLim_READ_Find_DPL_default_values,
					&PurchaseLimit.limit_method,		&PurchaseLimit.warning_only,
					&PurchaseLimit.ingredient_code,		&PurchaseLimit.exempt_diseases,
					&PurchaseLimit.qty_lim_grp_code,	&PurchaseLimit.MonthIs28Days,
					&Largo,								END_OF_ARG_LIST						);

		// No real error-trapping here - just supply some defaults if we didn't find anything.
		if (SQLCODE != 0)
		{
			PurchaseLimit.limit_method		= 0;
			PurchaseLimit.warning_only		= 0;
			PurchaseLimit.exempt_diseases	= 0;
			PurchaseLimit.ingredient_code	= ThisDrug->DL.Ingredient[0].code;
		}
	}	// We did *not* find a "real" limit for either Method 3/4 or the various "normal" quantity-based methods.


	// Whether or not we found an applicable limit from drug_purchase_lim, we need to see if
	// this member has a purchase limit ishur. Note that if the member has an all-drug
	// vacation ishur (from a special_prescs row with fictitious Largo Code 99997), we
	// ignore all single-drug vacation ishurim.
	ExecSQL (	MAIN_DB, ReadDrugPurchaseLim_READ_purchase_lim_ishur,
				&Ishur.max_units,			&Ishur.ingr_qty_std,	&Ishur.duration_months,
				&Ishur.ishur_open_date,		&Ishur.ishur_num,		&Ishur.ishur_type,
				&Member.ID,					&Largo,					&Member.ID_code,
				&Member.Has_PL_99997_Ishur,	&SysDate,				&SysDate,
				&PL_CloseDate_Min,			END_OF_ARG_LIST										);

	if (SQLERR_code_cmp (SQLERR_access_conflict) == MAC_TRUE)
	{
		SQLERR_error_test ();
		FoundSomething = -1;
		*reStart_out = MAC_TRUE;
	}
	else
	{
		if (SQLCODE == 0)
		{
			// Swap ishur values into the target purchase limit structure.
			PurchaseLimit.max_units			= Ishur.max_units;
			PurchaseLimit.ingr_qty_std		= Ishur.ingr_qty_std;
			PurchaseLimit.duration_months	= Ishur.duration_months;
			PurchaseLimit.ishur_open_date	= Ishur.ishur_open_date;
			PurchaseLimit.ishur_num			= Ishur.ishur_num;
			PurchaseLimit.ishur_type		= Ishur.ishur_type;
			PurchaseLimit.status			= 2;	// 1 = normal limit, 2 == purchase limit ishur.
			FoundSomething					= 1;
		}
		else
		{
			// If the SQL error is just "nothing found", that's OK - it just means that the member
			// doesn't have a Purchase Limit Ishur for this drug.
			if (SQLERR_code_cmp (SQLERR_not_found) != MAC_TRUE)
			{
				SQLERR_error_test ();
				FoundSomething = -1;
				*ErrorCode_out = ERR_DATABASE_ERROR;
			}
		}	// Not a successful read;
	}	// Not an access conflict.


	// If we're using a "regular" drug purchase limit (and not an ishur), we need to check for
	// expired ishurim. Drug purchases that took place while an ishur existed can't be counted
	// against a "regular" limit, since typically they would prevent the member from buying
	// drugs that s/he needs. (Ishur limits are generally higher than default limits.)
	// DonR 17Dec2017: For now at least, we want to apply this logic only to "normal" Method 0/1
	// limits. Method 3/4 limits get their time-span info from an applicable AS/400 ishur, and
	// we are assuming that these ishurim will have the correct dates since they're approved
	// on an individual basis.
	// DonR 05Dec2021 User Story #205423: Note that the new Type 7/8 limits are not discount-related,
	// so they shouldn't have any connection to purchase limit ishurim. Accordingly, I am *not*
	// including them in the logic below, at least for now.
	if ((PurchaseLimit.status == 1) &&
		((PurchaseLimit.limit_method == 0) || (PurchaseLimit.limit_method == 1) || (PurchaseLimit.limit_method == 5) || (PurchaseLimit.limit_method == 6)))
	{
		ExecSQL (	MAIN_DB, ReadDrugPurchaseLim_READ_ishur_MaxCloseDate,
					&Ishur.ishur_close_date,
					&Member.ID,					&Largo,
					&Member.ID_code,			&Member.Has_PL_99997_Ishur,
					&SysDate,					&SysDate,
					END_OF_ARG_LIST												);

		// No need to bother with "real" error-checking here.
		if (SQLCODE == 0)
		{
			// If we found a closed purchase limit ishur AND the ishur's close date
			// was after the history-start date of the "regular" purchase limit, use
			// the date from the closed ishur as the starting point for checking 
			// purchase history. Note that since the ishur was still valid on its
			// close date, the starting date for checking purchase history is the day
			// *after* the ishur's close date.
			if (Ishur.ishur_close_date > PurchaseLimit.history_start_date)
			{
				PurchaseLimit.history_start_date = Ishur.ishur_close_date + 1;
				// We don't care if this gives us an invalid date, since the test in
				// test_purchase_limits() is for purchases on a date >= the start-history
				// date. If the ishur closed on 31 October, we set the start-history date
				// to 32 October; purchases on 31 October will thus be ignored, and purchases
				// on 1 November will be taken into account.
			}
		}	// Found a closed purchase limit ishur.
	}	// Drug purchase limit coming from default table, so we need to check for expired ishurim.


	// At this point we've either found an applicable limit (from drug_purchase_lim or from
	// purchase_lim_ishur), or we've decided that no limit is applicable. All that remains
	// is to set up output variables.
	switch (PurchaseLimit.status)
	{
		case 1:		// Conventional drug purchase limit from drug_purchase_lim.
					ThisDrug->PurchaseLimitFrom			= 'D';
					ThisDrug->PurchaseLimitStartHist	= PurchaseLimit.history_start_date;
					ThisDrug->PurchaseLimitOpenDt		= 0;
					ThisDrug->qty_lim_ishur_num			= 0;

					// DonR 03Nov2022: To improve the ability to answer questions about why a particular sale was or
					// was not blocked, we are now writing the Purchase Limit Class Code to the prescription_drugs
					// table, in the new column qty_limit_class_code. Note that we *do not* do this for Purchase
					// Limit ishurim, since they do not rely on fitting the member into a particular Purchase Limit
					// Class - so we store the Class Code only when PurchaseLimit.status == 1.
					ThisDrug->qty_limit_class_code		= PurchaseLimit.class_code;

					break;

		case 2:		// Purchase limit from purchase_lim_ishur.
					//																		"Regular" Ishur					  Vacation Ishur
					ThisDrug->PurchaseLimitFrom			= (PurchaseLimit.ishur_type == 6) ? 'R'								: 'V';
					ThisDrug->PurchaseLimitStartHist	= (PurchaseLimit.ishur_type == 6) ? PurchaseLimit.ishur_open_date	: 0;
					ThisDrug->PurchaseLimitOpenDt		= (PurchaseLimit.ishur_type == 6) ? 0								: PurchaseLimit.ishur_open_date;
					ThisDrug->qty_lim_ishur_num			= PurchaseLimit.ishur_num;
					break;

		default:	// No applicable limit found - so disable purchase-limit checking for this drug line.
					ThisDrug->DL.purchase_limit_flg	= 0;
	}

	// DonR 01Mar2017: Instead of copying a single Limit Ingredient, we need to support Limit Groups
	// that include several ingredients. (This means we're adding together milligrams of different
	// ingredients, which isn't really valid - but for now at least, we don't have a choice.)
	ThisDrug->PurchaseLimitIngrCode [0]	= PurchaseLimit.ingredient_code;	// Default to "simple" case.
	ThisDrug->PurchaseLimitIngrCount	= 1;								// Default to "simple" case.

	// No point in executing the SQL below unless it's going to be relevant.
	// DonR 16Apr2018: We need to build the ingredient list for Method 4 limits as well as Method 1 limits.
	// DonR 11Jun2018: Method 6 limits use ingredients as well.
	// DonR 05Dec2021 User Story #205423: Method 8 limits also use ingredients.
	if ((PurchaseLimit.status > 0) &&
		((PurchaseLimit.limit_method == 1) || (PurchaseLimit.limit_method == 4) || (PurchaseLimit.limit_method == 6) || (PurchaseLimit.limit_method == 8)))
	{
		// DonR 12Jul2020: Moved the DeclareCursor inside the "if", since otherwise when the "if"
		// was FALSE the cursor was left DECLARE'd. Now it's DECLARE'd only if it's going to be
		// used (and closed).
		// DonR 05Dec2021: QUESTION: Is there some way to set up an ingredient limit such that it doesn't
		// need a unique Group Code and we don't have to perform this query? A zero value in qty_lim_grp_code
		// might be a good indicator...
		DeclareAndOpenCursor (	MAIN_DB, ReadDrugPurchaseLim_FindGroupIngreds_cur, &PurchaseLimit.qty_lim_grp_code, END_OF_ARG_LIST	);

		if (SQLCODE == 0)	// No real error-trapping here.
		{
			// Reset the counter to zero; if we don't succeed in reading anything (which
			// should never happen), we'll set it back to one afterwards.
			ThisDrug->PurchaseLimitIngrCount = 0;

			do
			{
				FetchCursorInto (	MAIN_DB, ReadDrugPurchaseLim_FindGroupIngreds_cur, &ThisIngredient, END_OF_ARG_LIST	);

				if ((SQLCODE != 0) || (ThisDrug->PurchaseLimitIngrCount >= LIMIT_GROUP_MAX_INGRED))
					break;	// Again, no real error-trapping. LIMIT_GROUP_MAX_INGRED is 25 for now,
							// which should be plenty high.

				// If we get here, we actually read an ingredient. (Most of the time, we'll
				// see only one!)
				ThisDrug->PurchaseLimitIngrCode [ThisDrug->PurchaseLimitIngrCount] = ThisIngredient;
				ThisDrug->PurchaseLimitIngrCount++;
			}
			while (1);
		}	// Successful OPEN of ReadDrugPurchaseLim_FindGroupIngreds_cur.

		CloseCursor (	MAIN_DB, ReadDrugPurchaseLim_FindGroupIngreds_cur	);

		// If we didn't read anything, restore the default situation.
		if (ThisDrug->PurchaseLimitIngrCount < 1)
			ThisDrug->PurchaseLimitIngrCount = 1;

	}	// Need to read in the list of ingredients for this Limit Group.


	ThisDrug->PurchaseLimitUnits					= PurchaseLimit.max_units;
	ThisDrug->PurchaseLimitIngredLim				= PurchaseLimit.ingr_qty_std;
	ThisDrug->PurchaseLimitMethod					= PurchaseLimit.limit_method;
	ThisDrug->PurchaseLimitWarningOnly				= PurchaseLimit.warning_only;
	ThisDrug->PurchaseLimitExemptDisease			= PurchaseLimit.exempt_diseases;
	ThisDrug->PurchaseLimitMonthIs28Days			= PurchaseLimit.MonthIs28Days;
	ThisDrug->PurchaseLimitCustomErrorCode			= PurchaseLimit.CustomErrorCode;
	ThisDrug->PurchaseLimitIncludeFullPrice			= PurchaseLimit.include_full_price_sales;

	// If member has an all-drug vacation ishur (using fictional Largo Code 99997), this
	// suppresses visibility of ordinary single-drug vacation ishurim, and also overrides
	// a couple of values. (Note that "99997" ishurim are from the special_prescs table,
	// *not* from purchase_lim_ishur!)
	// DonR 12Dec2021 QUESTION: Should we disable this bit of logic if the Limit Method is 7 or 8?
	if (Member.Has_PL_99997_Ishur)
	{
		ThisDrug->PurchaseLimitFrom		= 'V';
		ThisDrug->PurchaseLimitMonths	= 1;	// DonR 07Jul2013: 3 cycles of 1 month each.
		ThisDrug->PurchaseLimitOpenDt	= Member.PL_99997_OpenDate;
	}
	else
	{
		ThisDrug->PurchaseLimitMonths	= PurchaseLimit.duration_months;
	}

	// If calling routine wants a copy of the Purchase Limit structure, copy it into the provided pointer.
	if (PurchaseLimit_out != NULL)
		*PurchaseLimit_out = PurchaseLimit;

	RESTORE_ISOLATION;

	return (FoundSomething);
}	// End of ReadDrugPurchaseLimitData().



/*=======================================================================
||																		||
||			test_purchase_limits ()										||
||																		||
|| Test sale against drug purchase limits.								||
|| RETURNs:																||
|| 0 = OK.																||
|| 1 = high severity error occurred.									||
 =======================================================================*/
TErrorCode	test_purchase_limits (TPresDrugs				*DrugToCheck,
								  TPresDrugs				*DrugArrayPtr,
								  int						NumDrugs,
								  TMemberData				*Member,
								  int						maccabi_pharmacy_flag,
								  int						overseas_member_flag,
								  short						ExemptFromQtyLimit,
								  int						PharmacyCode,
								  TErrorCode				*ErrorCode)
{
	// Local declarations.
	int				ReStart;
	int				ErrorToReport	= 0;
	int				CycleNum;
	int				NumCycles;
	int				SystemDate;
	int				CycleStartDate;
	int				GoBackDays;
	int				DrugNum;
	int				ThisDrugUnits;
	int				FutureMonths;
	int				FirstMonthOfCycleEnd;		// DonR 23Jun2024 User Story #319203.
	int				NextCycleUnits		= 0;	// DonR 23Jun2024 User Story #319203.
	double			NextCycleIngredient	= 0.0;	// DonR 23Jun2024 User Story #319203.
	int				TestDate		[PURCHASE_LIMIT_HISTORY_MONTHS + 1];	// CR #27974.
	int				TotalUnits		[PURCHASE_LIMIT_HISTORY_MONTHS + 1];	// CR #27974.
	int				UnitLimit		[PURCHASE_LIMIT_HISTORY_MONTHS + 1];	// CR #27974.
	int				ManotPerCycle	[PURCHASE_LIMIT_HISTORY_MONTHS + 1];	// CR #27974.
	double			TotalIngredUsed	[PURCHASE_LIMIT_HISTORY_MONTHS + 1];	// CR #27974.
	double			IngredLimit		[PURCHASE_LIMIT_HISTORY_MONTHS + 1];	// CR #27974.
	short			Ingr;
	short			LimGroupIngred;
	short			FoundIngredUsage;
	short			LineNo;
	bool			UnderLimitUpToNow			= true;
	bool			CurrentSaleMoreThanMinimum	= false;
	bool			ThisDrugMoreThanMinimum		= false;
	bool			IgnorePharmacyMatch			= true;		// By default, sales at all pharmacies are relevant.
	bool			IgnorePrescriptionSource	= true;		// By default, all prescription sources are relevant.
	bool			IgnoreWhetherDiscounted		= false;	// By default, only discounted sales are relevant.
	bool			CompareUnitQuantity			= false;	// Whether the Limit Method calls for a unit-based quantity test.
	bool			CompareIngredientQuantity	= false;	// Whether the Limit Method calls for an ingredient-based quantity test.
	bool			LongNonDrugLimit			= false;	// DonR 23Jun2024 User Story #319203 - whether the limit
															// covers intervals of at least one quarter (3 months).
	DURDrugsInBlood	*PastDrug;								// DonR 12Dec2021: Just to make some code more compact.
	TPresDrugs		*CurrDrug;								// DonR 12Dec2021: Just to make some code more compact.
	ISOLATION_VAR;


	// DonR 11Dec2017: Documentation of Purchase Limit Methods (drug_purchase_lim.limit_method,
	// also stored in SPres.PurchaseLimitMethod).
	//
	// 0 = "Old" method: test limits by units purchased.
	// 1 = "New" method: test limits by ingredient usage.
	// 2 = Prescription Source permission control, with no quantity tests.
	// 3 = Fertility drugs using start/end dates taken from AS/400 Ishur - test limits by units bought.
	// 4 = Fertility drugs using start/end dates taken from AS/400 Ishur - test limits by ingredient usage.
	// 5 = Test limits by units purchased, looking *only* at a specific Prescription Source.
	// 6 = Test limits by ingredient usage, looking *only* at a specific Prescription Source.
	// 7 = Test by units purchased at the current pharmacy with a specific Prescription Source; this
	//     method uses duration_months as a number of *days* - normally 1 for today's purchases.
	// 8 = Test by ingredient amount purchased at the current pharmacy with a specific Prescription Source;
	//     this method uses duration_months as a number of *days* - normally 1 for today's purchases.


	// Skip drugs that already have a severe error.
	// DonR 01Mar2017: If this member has a serious illness and is buying a drug that's relevant to
	// that illness, we don't want to check "normal" quantity limits (but we still have to check
	// for valid Prescription Source). In that case, since we just checked the Prescription Source
	// in the calling "transaction mainline" routine, we can just exit the function at this point.
	if ((DrugToCheck->DFatalErr == MAC_TRUE) || (ExemptFromQtyLimit))
	{
		return (0);	// Not a show-stopper - just skip to next drug in calling-routine loop.
	}


	// Initialization.
	SystemDate = GetDate ();

	// DonR 10Jun2013: Array error handler now stores LineNo, to be written to prescription_msgs. Rather than add a parameter to
	// this function, just compute LineNo based on the two pointers that are already passed.
	LineNo = 1 + (DrugToCheck - DrugArrayPtr);

	// DonR 13Feb2022: If the current drug has its Purchase Limits Without Discount
	// flag set TRUE, ignore whether previous *or* current purchases were made at
	// 100% participation.
	// DonR 23Jul2025 User Story #427783: Now there is also a "check limits including
	// full-price purchases" option at the drug_purchase_lim level, to support new
	// safety restrictions on opioids and other dangerous drugs.
	if (	(DrugToCheck->DL.purchase_lim_without_discount)	||
			(DrugToCheck->PurchaseLimitIncludeFullPrice)		)
	{
		IgnoreWhetherDiscounted = true;
	}

	// DonR 11Dec2017 CR #12458: If the member isn't buying the drug in question on the basis of
	// an AS/400 ishur, the drug's IshurStartDate value will be zero. In this case - or if the ishur
	// didn't have a reasonable start date - check again to see if there's an applicable "normal"
	// limit (= Limit Method 0 or 1). If not, just exit, as there's no applicable limit to check.
	if (	(DrugToCheck->PurchaseLimitMethod == 3 || DrugToCheck->PurchaseLimitMethod == 4)	&&
			(DrugToCheck->IshurStartDate < 20000101))
	{
		if (ReadNonIshurPurchaseLimitMethods (	Member,
												DrugArrayPtr,
												LineNo - 1,
												NumDrugs,
												&v_DrugsBuf,
												NULL,
												NULL,
												&ReStart		)	== 0)	// 0 = Nothing found.
		{
			return (0);	// Not a show-stopper - just skip to next drug in calling-routine loop.
		}

	}	// Limit Method 3 or 4 is not applicable if member isn't buying this drug with an AS/400 ishur.

// ELSE?

	// DonR 11Feb2024 BUG FIX #480141 Revised: Normally, all OTC sales (presc_source == 0) are at 100% hishtatfut,
	// and thus ordinary drug purchase limits (methods 0, 1, 3, and 4) don't apply. However, sometimes (by mistake?)
	// someone does create a "nohal" that gives discounts even without a prescriptions - apparently the AS/400 user
	// interface allows it, and the logic on the Linux side allows these discounts as well. When this happens, we
	// need to *disable* the suppression of limit-checking, since a "nohal" discount is at Maccbi's expense. Since
	// we normally read drug_purchase_lim data before calculating hishtatfut, the best approach is to test for
	// discounted OTC sales here, and if the sale is in fact OTC but with less than 100% hishtatfut, re-read the
	// limits with all Limit Methods included.
	if	(	(DrugToCheck->PrescSource == RECIP_SRC_NO_PRESC || DrugToCheck->PrescSource == RECIP_SRC_EMERGENCY_NO_RX)		&&
			(DrugToCheck->BoughtAtDiscount)
		)
	{
		ReadPurchaseLimitsAllMethods (	Member,
										DrugArrayPtr,
										LineNo - 1,
										NumDrugs,
										&v_DrugsBuf,
										NULL,
										NULL,
										&ReStart		);
	}


	// DonR 10Feb2019: Add a "sanity check" on the number of cycles set in sysparams - it can't be
	// less than zero, and it can't be more than PURCHASE_LIMIT_HISTORY_MONTHS since that would
	// blow through the size of the arrays we've allocated. (The arrays are currently allocated
	// to allow a full three years of history, which is more than we're ever likely to use.)
	if (BakaraKamutitHistoryMonths < 0)
		BakaraKamutitHistoryMonths = 0;

	if (BakaraKamutitHistoryMonths > PURCHASE_LIMIT_HISTORY_MONTHS)
		BakaraKamutitHistoryMonths = PURCHASE_LIMIT_HISTORY_MONTHS;

	// DonR 23Jun2024 User Story #319203: Also add a sanity check on the duration (whether in months or
	// days), since we don't want to see division-by-zero errors. Limit Method 2 is exempt from this,
	// since it doesn't set up an actual quantity check.
	if ((DrugToCheck->PurchaseLimitMonths < 1) && (DrugToCheck->PurchaseLimitMethod != 2))
	{
		DrugToCheck->PurchaseLimitMonths = 1;
	}

	// DonR 06Dec2021: Just for convenience, set up two boolean variables to indicate whether
	// the applicable Limit Method calls for a unit-based or an ingredient-based quantity test.
	// DonR 21Nov2021 User Story #205423: Added Limit Methods 7 and 8.
	switch (DrugToCheck->PurchaseLimitMethod)
	{
		case  0:	// "Old" method: test limits by units purchased.
		case  3:	// Fertility drugs using start/end dates taken from AS/400 Ishur - test limits by units bought.
		case  5:	// Test limits by units purchased, looking *only* at a specific Prescription Source.
		case  7:	// Test by units purchased at the current pharmacy, normally used for current-day purchases.
					CompareUnitQuantity			= 1;
					CompareIngredientQuantity	= 0;
					break;

		case  1:	// "New" method: test limits by ingredient usage.
		case  4:	// Fertility drugs using start/end dates taken from AS/400 Ishur - test limits by ingredient usage.
		case  6:	// Test limits by ingredient usage, looking *only* at a specific Prescription Source.
		case  8:	// Test by ingredient quantity purchased at the current pharmacy, normally used for current-day purchases.
					CompareUnitQuantity			= 0;
					CompareIngredientQuantity	= 1;
					break;

		case  2:	// Prescription Source permission control, with no quantity tests. In reality, this should be
					// irrelevant, since Method 2 limits are handled in the "mainline" calling routine, and "default"
					// shouldn't happen either. Still, it's good practice to handle all possibilities, even the
					// impossible ones!
		default:
					CompareUnitQuantity			= 0;
					CompareIngredientQuantity	= 0;
					break;
	}

	// DonR 11Dec2017 CR #12458: If we're using a Purchase Limit with Method = 3 or 4, we test only
	// a single "cycle", with the start date taken from the member's relevant AS/400 Ishur and the
	// end date equal to today. Note that for these Limit Methods, Cycle 0 testing is redundant:
	// since the limit for Cycle 1 (from the AS/400 Ishur Start Date until today) is the same as
	// the limit for Cycle 0, any Cycle 0 limit violation would also violate the limit for Cycle 1.
	if ((DrugToCheck->PurchaseLimitMethod == 3) || (DrugToCheck->PurchaseLimitMethod == 4))
	{
		NumCycles		= 2;	// Current purchase & lifespan of ishur.
		ManotPerCycle	[0]	= ManotPerCycle		[1]	= 1;
		TotalUnits		[0] = TotalUnits		[1] = 0;
		TotalIngredUsed	[0] = TotalIngredUsed	[1] = 0.0;
		UnitLimit		[0] = UnitLimit			[1] = DrugToCheck->PurchaseLimitUnits;
		IngredLimit		[0] = IngredLimit		[1] = DrugToCheck->PurchaseLimitIngredLim;	// DonR 16Aug2020: Change comparisons from > to ratio.
		TestDate		[0] = SystemDate;
		TestDate		[1] = DrugToCheck->IshurStartDate;
	}

	// DonR 06Dec2021 User Story #205423: The new Method 7/8 limits are intended to control OTC purchases
	// at a particular pharmacy on a particular day. Accordingly, they use DrugToCheck->PurchaseLimitMonths
	// as a day count rather than a month count - and it's expected that the value of PurchaseLimitMonths
	// will normally be 1.
	else
	if ((DrugToCheck->PurchaseLimitMethod == 7) || (DrugToCheck->PurchaseLimitMethod == 8))
	{
		IgnorePharmacyMatch			= 0;	// Method 7/8 limits apply to purchases at a single pharmacy.
		IgnorePrescriptionSource	= 0;	// Method 7/8 limits apply to a single prescription source - normally 0 for OTC purchases.
		IgnoreWhetherDiscounted		= true;	// Method 7/8 limits are about safety rather than economics - so full-price purchases are relevant.
		NumCycles					= 2;	// Current purchase & recent history - normally only today's previous purchases.
		ManotPerCycle	[0]	= ManotPerCycle		[1]	= 1;
		TotalUnits		[0] = TotalUnits		[1] = 0;
		TotalIngredUsed	[0] = TotalIngredUsed	[1] = 0.0;
		UnitLimit		[0] = UnitLimit			[1] = DrugToCheck->PurchaseLimitUnits;
		IngredLimit		[0] = IngredLimit		[1] = DrugToCheck->PurchaseLimitIngredLim;	// DonR 16Aug2020: Change comparisons from > to ratio.
		TestDate		[0] = SystemDate;
		TestDate		[1] = (DrugToCheck->PurchaseLimitMonths > 1) ? AddDays (SystemDate, (1 - DrugToCheck->PurchaseLimitMonths)) : SystemDate;
	}

	else	// Conventional limits or prescription-source-based limits (Method = 0/1 or 5/6),
			// NOT using dates from AS/400 Ishur.
	{
		// DonR 23Jun2024 User Story #319203: If this is a limit with the time span set to three
		// or more months, set the LongNonDrugLimit flag TRUE. In this case we will disable future
		// sales, and allow sales for the next cycle in the last month of the current cycle.
		// DonR 31Jul2024 User Story #337030: Apply the new logic for everything *except*
		// "treatment"-type drugs. Changed the name of the flag to avoid confusion.
//		LongNonDrugLimit = (DrugToCheck->PurchaseLimitMonths >= 3);
		LongNonDrugLimit = ((DrugToCheck->DL.largo_type != 'T') && (DrugToCheck->PurchaseLimitMonths >= 3));


		// DonR 06Dec2021: Keep "if" statement below simple (or at least simple-ish) by using some
		// boolean variables to control the various Limit Method options.
		if ((DrugToCheck->PurchaseLimitMethod == 5) || (DrugToCheck->PurchaseLimitMethod == 6))
		{
			IgnorePrescriptionSource = 0;	// Method 5/6 limits apply to a single prescription source.
		}

		// DonR 07Jul2013: New cycle-control logic required by "bakara kamutit" enhancement.
		// Note that FUTURE_SALE_ALL_PHARM doesn't actually permit future-dated sales at private pharmacies -
		// but that's taken care of elsewhere. A member could come to a private pharmacy with a current-dated
		// 90-day prescription and get it filled if the drug has this flag set.
		// Several different conditions will allow the higher level of purchase limits.
		// DonR 03Feb2019 CR #27974: Expand time horizon for history-checking from 90 days to 750 days,
		// parameterized by PURCHASE_LIMIT_HISTORY_MONTHS (which, at least for now, is a hard-coded macro
		// rather than a sysparams parameter).
		// DonR 07Feb2019: The actual number of 30-day months to check is now set by sysparams/bakarakamutit_mons.
		// DonR 29Jan2024 User Story #540234: Make "three months mode" conditional on either the item being
		// sold *not* being cannabis, or the global variable CannabisAllowFutureSales (from the sysparams
		// table) being TRUE.
		// DonR 23Jun2024 User Story #319203: Disable "future" mode for long-term (quarterly and up) limits.
		if	(
				(	( DrugToCheck->DL.drug_type != 'K') || (CannabisAllowFutureSales)	)
				&&
				(	!LongNonDrugLimit	)	// DonR 23Jun2024 User Story #319203.
				&&
				(	( DrugToCheck->DL.allow_future_sales == FUTURE_SALE_ALL_PHARM)								||
					((DrugToCheck->DL.allow_future_sales == FUTURE_SALE_MAC_ONLY ) && (maccabi_pharmacy_flag)) 	||
					(DrugToCheck->vacation_ishur_num > 0)														||
					(overseas_member_flag)
				)
			)
		{
			// Allow 4/5/6 cycles in last 1/2/3 months.
			// 15Jan2014: If we're in "three month" mode, the single-transaction limit depends
			// on the pharmacy type and the drug's Future Sales flag. Maccabi pharmacies can
			// sell all drugs on a "3/4/5/6" basis, while private pharmacies are on either a
			// "1/4/5/6" basis or a "3/4/5/6" basis depending on the Future Sales flag.
			ManotPerCycle [0]	= ((maccabi_pharmacy_flag) || (DrugToCheck->DL.allow_future_sales == FUTURE_SALE_ALL_PHARM))	? 3 : 1;
			FutureMonths		= 3;	// CR #27974.
		}
		else	// "Three months" mode is *not* enabled.
		{
			// Allow 2/3/4 cycles in last 1/2/3 months.
			// 15Jan2014: If we're not in "three month" mode, even Maccabi pharmacies can sell only one
			// month's worth in a single transaction.
			// DonR 25Jun2024 User Story #319203: For long-term (quarterly and up) limits, we want to set
			// FutureMonths to zero by default, since the member is not supposed to be able to buy stuff
			// for a three-month cycle that hasn't started yet. The exception is that if the current cycle
			// is in its last month (indicated by at least one sale that took place in the *first* month
			// of the current cycle), we will allow the member to buy the item for the next cycle. That
			// part of the logic is implemented below, when we scan through past purchases.
			ManotPerCycle [0]	= 1;
			FutureMonths		= (LongNonDrugLimit) ? 0 : 1;	// DonR 25Jun2024 User Story #319203.
		}

		// DonR 23Jun2024 User Story #319203: We should set NumCycles based on the length of each
		// cycle - there's no point in looking at irrelevant long-ago history! (As of June 2024,
		// BakaraKamutitHistoryMonths = 18; so for quarterly limits, we'll look at 6 past cycles.)
// 		NumCycles = BakaraKamutitHistoryMonths + 1;	// CR #27974, 07Feb2019.
		NumCycles = (BakaraKamutitHistoryMonths / DrugToCheck->PurchaseLimitMonths) + 1;

		// DonR 06Dec2021: We now support the option of 28-day "months" for certain items - in particular,
		// sensors that come in packages of 2 where each sensor is used for exactly 14 days. We are not
		// yet getting the "28-day flag" from AS/400, but that will happen in a few weeks.
		// DonR 25Jun2024 User Story #319203: Do a more accurate computation of cycle length that works
		// for cycles of more than one month. Note that if the limit specifies a 28-day month, we want
		// to use that month length regardless of the actual calendar.
		if (DrugToCheck->PurchaseLimitMonthIs28Days)
		{
			GoBackDays = DrugToCheck->PurchaseLimitMonths * 28;	// DonR 05Dec2021: Support 28-day "monthly" limits.
		}
		else
		{
			// DonR 25Jun2024 User Story #319203: For "real" months, compute the cycle length based on a 365-day
			// year. (Multiply before dividing to make sure that we don't lose a day that we don't want to lose.)
			// For ordinary one-month limits, this means we'll simply divide 365 by 12 to get GoBackDays = 30; but
			// for three-month limits, we'll divide (365 * 3 = 1095) by 12 to get 91.
			GoBackDays = (365 * DrugToCheck->PurchaseLimitMonths) / 12;
		}

		// DonR 25Jun2024 User Story #319203: Set trigger date for a previous drug sale to permit buying the
		// next multiple-month cycle when the current cycle is in its last month. If LongNonDrugLimit is
		// FALSE, we'll set FirstMonthOfCycleEnd to a nonsense value so it won't do anything. Note that if
		// GoBackDays = 91 (for a quarterly limit), FirstMonthOfCycleEnd is today - 60 days, and a qualifying
		// purchase needs to be >= the starting date for the cycle and *less than* FirstMonthOfCycleEnd.
		FirstMonthOfCycleEnd = (LongNonDrugLimit) ? AddDays (SystemDate, (31 - GoBackDays)) : 0;

		// DonR 03Feb2019 CR #27974: Instead of just 30/60/90 days, we're now going back 540 days - so
		// assign manot-per-cycle in a loop rather than individually. Note that we start the loop at 1,
		// since Cycle 0 has special logic for pharmacies/drugs that allow (or don't allow) future sales.
		// (DonR 25Jun2024 User Story #319203: Remember that for quarterly limits, FutureMonths = 0!)
		for (CycleNum = 1; CycleNum < NumCycles; CycleNum++)
		{
			ManotPerCycle [CycleNum] = CycleNum + FutureMonths;
		}


		// DonR 07Jul2013: According to the new "Bakara Kamutit" logic, we always look at
		// purchases over the last three months, even if the member has a vacation ishur.
		// Note that according to the new specification, PurchaseLimitMonths should always
		// equal 1.
		// DonR 05Dec2021: To prepare for the option of 28-day "months" for certain items
		// (e.g. sensors that come in packages of two and are used for exactly 14 days each),
		// make the month length a variable with default = 30 rather than a hard-coded number.
		// This gives us the option of overriding it with a "28-day flag".
		CycleStartDate	= SystemDate;
//		GoBackDays		= DrugToCheck->PurchaseLimitMonths * MonthLength;	// DonR 05Dec2021: Either 28 or 30 days/month.

		// DonR 21May2017: In some cases (with "ishur caspi" limits), we saw spurious
		// quantity-limit errors, apparently because of an internal rounding error in
		// the processor's math operations. Assuming that the same thing could happen
		// here, expand the ingredient-based limit by a tiny factor - not enough to
		// account for any actual additional purchases, but more than the rounding error.
		for (CycleNum = 0; CycleNum < NumCycles; CycleNum++)
		{
			TotalUnits		[CycleNum] = 0;
			TotalIngredUsed	[CycleNum] = 0.0;		// DonR 11Dec2017 - OOPS! This was never initialized - a potential bug!

			TestDate	[CycleNum] = CycleStartDate;	// First cycle consists of current purchase only.
			UnitLimit	[CycleNum] = ManotPerCycle [CycleNum] * DrugToCheck->PurchaseLimitUnits;
			IngredLimit	[CycleNum] = ManotPerCycle [CycleNum] * DrugToCheck->PurchaseLimitIngredLim;	// DonR 16Aug2020: Change comparisons from > to ratio.

			// Decrement date for next cycle.
			if (CycleNum < (NumCycles - 1))	// Don't bother doing this for the last iteration.
				CycleStartDate = AddDays (CycleStartDate, (0 - GoBackDays));
		}
	}	// Limit Method is NOT 3/4 or 7/8.


	// WORKINGPOINT: Do we want to suppress "early refill" warnings when we
	// check Drug Purchase Limits?

	// Get drugs previously purchased by member. If the array has already
	// been loaded, we don't have to do it again.
	// DonR 25May2010: get_drugs_in_blood() now handles old and new member ID, and is also smart enough
	// to decide whether it has to reload to drugs-in-blood buffer - so we can call it once, unconditionally.
	if (get_drugs_in_blood (Member,
							&v_DrugsBuf,
							GET_ALL_DRUGS_BOUGHT,
							ErrorCode,
							&ReStart)				== 1)
	{
		if (ReStart != 0)
		{
			LOG_CONFLICT ();
			return (2);
		}
		else
		{
			*ErrorCode = ERR_DATABASE_ERROR;
			return (1);
		}
	}	// high severity error OR deadlock occurred

	// DonR 25Jun2024 User Story #319203: Redundant, paranoid, re-initialization. These
	// two variables will get values only if we're dealing with a quarterly limit and we
	// find a relevant purchase in the first month of the current cycle. They are added
	// to the limits (for all cycles) so we know to allow the member to buy stuff for the
	// next cycle once we're in the last month of the current cycle.
	NextCycleUnits		= 0;
	NextCycleIngredient	= 0.0;

	// Scan through previous drug purchases to find purchases of drugs
	// in the same limit-group as the current drug.
	for (DrugNum = 0; DrugNum < v_DrugsBuf.max; DrugNum++)
	{
		// DonR 12Dec2021: Just to make code a bit more compact,
		// set up a simple "Durgs" pointer for past purchases.
		PastDrug = &v_DrugsBuf.Durgs[DrugNum];

		// Skip this drug if its purchase date is out of range OR
		// if it isn't part of the same purchase limit group OR
		// if it wasn't bought at a discount.
		//
		// DonR 07Nov2010: Because some members may have bought stuff before quantity limits
		// were imposed, in quantities greater than the limits allow, set a minimum date on
		// history to be considered relevant. This will normally be the date on which a set
		// of quantity limits was put into effect.

		// DonR 03Jun2018 CR#13948: If we're using a prescription-source-specific limit (Method 5/6)
		// AND the member doesn't have a Quantity Limit Ishur, ignore historical purchases from other
		// prescription sources.
		// DonR 05Dec2021: Add conditions to support new Method 7/8 limits.
		if (( PastDrug->StartDate				<= SystemDate)																&&
			( PastDrug->StartDate				>= DrugToCheck->PurchaseLimitStartHist)										&&
			( PastDrug->PurchaseLimitFlag		>  0)																		&&
			( PastDrug->PurchaseLimitGroupCode	== DrugToCheck->DL.qty_lim_grp_code)										&&
			((PastDrug->BoughtAtDiscount)												|| (IgnoreWhetherDiscounted))		&&
			((PastDrug->PharmacyCode			== PharmacyCode)						|| (IgnorePharmacyMatch))			&&
			((PastDrug->Source					== DrugToCheck->PrescSource)			|| (DrugToCheck->qty_lim_ishur_num > 0)
																						|| (IgnorePrescriptionSource)))
		{
			// Loop through cycles, testing date individually and adding units as required.
			// Note that for everything except vacation ishurim, we start the loop with the
			// second cycle; the first cycle is for the current purchase only, and doesn't
			// include any previous purchases even if they were done earlier today. Vacation
			// ishurim work differently: we test only one time span, and include all purchases
			// made from the ishur's Open Date through the current purchase.
			// DonR 07Jul2013: Always start from second cycle. Vacation ishurim no longer
			// change the basic way we work - they are just one of the triggers for setting
			// higher values in ManotPerCycle[].
			// DonR 01Dec2016: Add capability of looking at ingredient usage instead of
			// OP/Units bought. We will now produce totals for both, and only at the end
			// decide which numbers to compare.
			for (CycleNum = 1; CycleNum < NumCycles; CycleNum++)
			{
				if (PastDrug->StartDate >= TestDate [CycleNum])
				{
					TotalUnits [CycleNum] += PastDrug->TotalUnits;

					// DonR 25Jun2024 User Story #319203: If we've found a qualifying beginning-of-cycle
					// purchase, set up the next-cycle units variable. (If this is an ingredient-based
					// limit, we'll ignore units - so there's no harm in putting a value into the variable
					// even if there's no ingredient match.)
					if ((CycleNum == 1) && (PastDrug->StartDate < FirstMonthOfCycleEnd))
					{
						NextCycleUnits = DrugToCheck->PurchaseLimitUnits;
					}

					// DonR 01Mar2017:
					// 1) No point in totaling ingredients if that's not how this purchase limit works.
					// 2) Add support for multiple ingredients in a single Limit Group.
					if (CompareIngredientQuantity)	// DonR 06Dec2021: Use a single boolean to indicate that we're doing ingredient-usage testing.
					{
						for (Ingr = FoundIngredUsage = 0; Ingr < 3; Ingr++)
						{
							// Don't bother matching zeroes!
							if (PastDrug->Ingredient[Ingr].code < 1)
								continue;

							for (LimGroupIngred = 0; LimGroupIngred < DrugToCheck->PurchaseLimitIngrCount; LimGroupIngred++)
							{
								if (PastDrug->Ingredient[Ingr].code == DrugToCheck->PurchaseLimitIngrCode[LimGroupIngred])
								{
									TotalIngredUsed [CycleNum] += PastDrug->Ingredient[Ingr].quantity;
									FoundIngredUsage = 1;

									// DonR 25Jun2024 User Story #319203: If this is an "ingredient" limit,
									// see if we've found a qualifying beginning-of-cycle purchase. If so,
									// copy the monthly ingredient limit to NextCycleIngredient.
									if ((CycleNum == 1) && (PastDrug->StartDate < FirstMonthOfCycleEnd))
									{
										NextCycleIngredient = DrugToCheck->PurchaseLimitIngredLim;
									}

									break;	// We can assume that if the same Ingredient Code appears twice for the same drug, it's an error.
								}	// Found relevant ingredient usage.
							}	// Loop through Limit Group ingredients.

							// Once we found our ingredient usage, we don't need to continue with other ingredient slots for this drug.
							if (FoundIngredUsage > 0)
								break;
						}	// Loop through past-drug ingredients.
					}	// We're dealing with an ingredient-based Purchase Limit.
				}	// This drug was bought at the right time to be added to this past cycle.
			}	// Loop through past cycles.
		}	// Previously-purchased drug is relevant to quantity-limit checking.
	}	// Loop through previously-purchased drugs.


	// DonR 01Jul2018: If we're testing a Type 3/4 limit, see if we've still got something left to sell after all the past
	// purchases are added up. If so, the member is allowed to buy one unit (or OP) of every relevant drug in the current sale,
	// even if that would put him/her over the limit. Note that the comparison between usage here is ">=" rather than ">":
	// if the limit has been exactly met, we *don't* want to allow any further purchases.
	// DonR 16Aug2020: Changed ingredient-used calculation from ">=" to ratio test for better accuracy.
	if ((DrugToCheck->PurchaseLimitMethod == 3) || (DrugToCheck->PurchaseLimitMethod == 4))
	{
		for (CycleNum = 0, UnderLimitUpToNow = 1; (CycleNum < NumCycles) && UnderLimitUpToNow; CycleNum++)
		{
			if (((DrugToCheck->PurchaseLimitMethod == 3) && (TotalUnits		[CycleNum] >= UnitLimit [CycleNum]))	||
				((DrugToCheck->PurchaseLimitMethod == 4) && (IngredLimit	[CycleNum] >  0.0) && ((TotalIngredUsed [CycleNum] / IngredLimit [CycleNum]) > 0.9999999)))
			{
				UnderLimitUpToNow = 0;
			}	// Limit exceeded for this cycle.
		}	// Loop through cycles.
	}	// Working with a Method 3 or Method 4 limit.


	// Scan through the current drug purchase request to find purchases
	// of drugs in the relevant purchase limit group.
	// DonR 01Dec2016: Add capability of looking at ingredient usage instead of
	// OP/Units bought. We will now produce totals for both, and only at the end
	// decide which numbers to compare.
	for (DrugNum = CurrentSaleMoreThanMinimum = 0; DrugNum < NumDrugs; DrugNum++)
	{
		// DonR 12Dec2021: Just to make code a bit more compact, set up
		// a simple "Durgs" pointer for drugs in the current sale request.
		// Also, not that we don't need to check Pharmacy Code here - all
		// current drugs are, by definition, being sold at the same pharmacy.
		CurrDrug = &DrugArrayPtr[DrugNum];

		// DonR 06Dec2021: Use new boolean variables to support new Limit Methods 7 & 8.
		if (( CurrDrug->DL.purchase_limit_flg	>  0)																		&&
			( CurrDrug->DL.qty_lim_grp_code		== DrugToCheck->DL.qty_lim_grp_code)										&&
			((CurrDrug->BoughtAtDiscount)											|| (IgnoreWhetherDiscounted))			&&
			((CurrDrug->PrescSource				== DrugToCheck->PrescSource)		|| (DrugToCheck->qty_lim_ishur_num > 0)
																					|| (IgnorePrescriptionSource)))
		{
			ThisDrugUnits =   CurrDrug->Units
							+ (CurrDrug->DL.package_size * CurrDrug->Op);

			// For Method 3/4 limits, check whether this drug line involves more than the minimum quantity.
			// Note that at least for now, we're assuming that a single package (OP = 1) is "minimal"; we
			// may need to change this and look at whether the openable_pkg flag is set true before
			// considering 1 OP as "minimal".
			// DonR 26Jul2018: Tightened up the criterion for "minimal" purchase. Now the purchase is "more
			// than minimal" if (OP + Units) > 1 OR (OP > 0 and the package has more than one unit and is
			// openable). Previously we didn't force pharmacy to open the package to meet this test.
			ThisDrugMoreThanMinimum = (((CurrDrug->Units + CurrDrug->Op) > 1)			||
									   ((CurrDrug->Op > 0) && (CurrDrug->DL.package_size > 1) && (CurrDrug->DL.openable_pkg != 0)));

			// Current purchase gets added into all cycles.
			for (CycleNum = 0; CycleNum < NumCycles; CycleNum++)
			{
				TotalUnits [CycleNum] += ThisDrugUnits;

				// If we're using Method 3 (single cycle based on Ishur date, units rather than milligrams),
				// update the "current sale more than minimum" flag.
				if ((DrugToCheck->PurchaseLimitMethod == 3) && (ThisDrugMoreThanMinimum))
					CurrentSaleMoreThanMinimum = 1;

				// DonR 01Mar2017:
				// 1) No point in totaling ingredients if that's not how this
				//    purchase limit works.
				// 2) Add support for multiple ingredients in a single Limit Group.
				if (CompareIngredientQuantity)	// DonR 06Dec2021: Use a single boolean to indicate that we're doing ingredient-usage testing.
				{
					for (Ingr = FoundIngredUsage = 0; Ingr < 3; Ingr++)
					{
						// Don't bother matching zeroes!
						if (CurrDrug->DL.Ingredient[Ingr].code < 1)
							continue;

						for (LimGroupIngred = 0; LimGroupIngred < DrugToCheck->PurchaseLimitIngrCount; LimGroupIngred++)
						{
							if (CurrDrug->DL.Ingredient[Ingr].code == DrugToCheck->PurchaseLimitIngrCode[LimGroupIngred])
							{
									TotalIngredUsed [CycleNum] += CurrDrug->IngrQuantityStd[Ingr];
									FoundIngredUsage = 1;

									// If we're using Method 4 (single cycle based on Ishur date, measuring by ingredient usage),
									// update the "current sale more than minimum" flag.
									if ((DrugToCheck->PurchaseLimitMethod == 4) && (ThisDrugMoreThanMinimum))
										CurrentSaleMoreThanMinimum = 1;

									break;	// We can assume that if the same Ingredient Code appears twice for the same drug, it's an error.
							}	// Found relevant ingredient usage.
						}	// Loop through Limit Group ingredients.

						// Once we found our ingredient usage, we don't need to continue with other ingredient slots for this drug.
						if (FoundIngredUsage > 0)
							break;
					}	// Loop through current drug ingredients.
				}	// We're dealing with an ingredient-based Purchase Limit.
			}	// Loop through cycles.
		}	// Drug is relevant to the current purchase-limit check.
	}	// Loop through drugs in the current purchase request.


	// Compare combined historical and current purchases with Purchase Limits
	// and assign warning/error codes as necessary.
	//
	// Perform test for all relevant cycles.
	// DonR 01Dec2016: Test can now be made in two different ways: either comparing total units,
	// or else using ingredient usage.
	// DonR 11Dec2017 CR #12458: Added support for new Limit Methods 3 & 4, which apply to AS/400
	// Ishurim rather than to multiple calendar-month cycles.
	// DonR 11Jun2018 CR #13948: Added support for Method 5/6 limits.
	// DonR 25Jun2024 User Story #319203: Add NextCycleUnits/NextCycleIngredient to the limit amounts
	// we compare total purchases to. In most cases, these variables will have zero values; but if
	// we're testing a quarterly (or longer) limit *and* a purchase was made in the first month of
	// the current cycle, they will have values that permit the member to buy the next cycle's worth.
	for (CycleNum = ErrorToReport = 0; CycleNum < NumCycles; CycleNum++)
	{
		if ((CompareUnitQuantity		&& (TotalUnits	[CycleNum] > (UnitLimit [CycleNum] + NextCycleUnits)))		||
			(CompareIngredientQuantity	&& (IngredLimit	[CycleNum] > 0.0) && ((TotalIngredUsed [CycleNum] / (IngredLimit [CycleNum] + NextCycleIngredient)) > 1.000001)))
		{
			// DonR 02Jul2018: For Method 3/4 (single cycle based on AS/400 Ishur date), we need to set up some
			// different error codes IF there was some limit "left over" before the current sale.
			if (((DrugToCheck->PurchaseLimitMethod == 3) || (DrugToCheck->PurchaseLimitMethod == 4)) && (UnderLimitUpToNow))
			{
				if (CurrentSaleMoreThanMinimum)
				{
					if (DrugToCheck->PurchaseLimitWarningOnly > 0)
					{
						ErrorToReport = (DrugToCheck->qty_lim_ishur_num == 0) ?	WARN_METHOD_3_4_LIMIT_EXCEEDED :
																				WARN_METHOD_3_4_ISHUR_EXCEEDED;
					}
					else
					{
						ErrorToReport = (DrugToCheck->qty_lim_ishur_num == 0) ?	ERR_METHOD_3_4_LIMIT_EXCEEDED :
																				ERR_METHOD_3_4_ISHUR_EXCEEDED;
					}
				}	// Current sale involves more than one unit/OP of at least one relevant drug.
				else
				{
					// We are allowing this sale even though it exceeds the limit - send a warning
					// message to this effect.
					ErrorToReport = (DrugToCheck->qty_lim_ishur_num == 0) ?	WARN_METHOD_3_4_LAST_ALLOWED :
																			WARN_METHOD_3_4_ISHUR_LAST_ALLOWED;
				}	// Current sale involves minimum quantities (1 unit or 1 OP) of all relevant drugs.
			}	// Method 3/4.

			else
			// DonR 06Dec2021 User Story #205423: Add support for Limit Methods 7 and 8 for
			// single-pharmacy (and normally single-day) OTC purchase limits.
			if ((DrugToCheck->PurchaseLimitMethod == 7) || (DrugToCheck->PurchaseLimitMethod == 8))
			{
				if (DrugToCheck->PurchaseLimitWarningOnly > 0)
				{
					ErrorToReport = OVER_DAILY_OTC_PER_PHARM_LIMIT_WARN;
				}
				else
				{
					ErrorToReport = OVER_DAILY_OTC_PER_PHARM_LIMIT_ERR;
				}
			}

			else
			{
				// DonR 08Feb2022: Added new Custom Error Code feature. If we've got an error and the limit
				// we're using is a standard one rather than a limit modified by a Purchase Limit Ishur,
				// AND there is a non-zero value in DrugToCheck->PurchaseLimitCustomErrorCode, use that as
				// the returned error instead of WARN_DRUG_PURCH_LIMIT_EXCEEDED/ERR_DRUG_PURCH_LIMIT_EXCEEDED.
				// WORKINGPOINT: If the member has qualified for a non-zero limit buy buying a particular type
				// of drug enough times, we should probably send the regular error/warning message rather than
				// the custom message. This is because the custom message is basically intended to tell the
				// member why s/he can't buy the item *at all* rather than that s/he has already bought too many.
				if (DrugToCheck->PurchaseLimitWarningOnly > 0)
				{
					ErrorToReport = (DrugToCheck->qty_lim_ishur_num == 0) ?	(DrugToCheck->PurchaseLimitCustomErrorCode > 0) ? DrugToCheck->PurchaseLimitCustomErrorCode : WARN_DRUG_PURCH_LIMIT_EXCEEDED :
																			WARN_DRUG_PURCH_ISHUR_EXCEEDED;
				}
				else
				{
					ErrorToReport = (DrugToCheck->qty_lim_ishur_num == 0) ?	(DrugToCheck->PurchaseLimitCustomErrorCode > 0) ? DrugToCheck->PurchaseLimitCustomErrorCode : ERR_DRUG_PURCH_LIMIT_EXCEEDED :
																			ERR_DRUG_PURCH_ISHUR_EXCEEDED;
				}
			}	// Not Method 3/4 AND not Method 7/8.

			if (ErrorToReport)
				break;

		}	// Limit exceeded for this cycle.
	}	// Loop through cycles.

	// DonR 22Feb2012: Moved the SetErrorVarArr() call after the loop and made it conditional; this ensures
	// that only one error (or none) will be reported for any given drug. Previously it was possible for
	// both a warning and a severe error to be reported.
	if (ErrorToReport > 0)
	{
		SetErrorVarArr (&DrugToCheck->DrugAnswerCode, ErrorToReport, DrugToCheck->DrugCode, LineNo, &DrugToCheck->DFatalErr, NULL);

		// DonR 15Sep2020 CR #30106: If we found *any* quantity-limit problem, *and* this drug
		// requires a previous purchase to authorize 3-month sales, *and* we didn't find such
		// a previous qualifying purchase, send a special message to the pharmacy to tell them
		// why only one month's worth could be bought.
		if (DrugToCheck->DL.allow_future_sales == FUTURE_SALE_NOT_YET_QUALIFIED)
		{
			SetErrorVarArr (&DrugToCheck->DrugAnswerCode, ERR_FUTURE_SALE_NOT_YET_QUALIFIED, DrugToCheck->DrugCode, LineNo, &DrugToCheck->DFatalErr, NULL);
		}
	}

	// Set Quantity Limit Check Type for this drug.
	// DonR 16Jul2018: Added a new indicator (RX_SOURCE_PURCHASE_LIM = 4) for prescription-source-based
	// limits. I decided to use a conventional if-then-else to avoid nested ternary operators.
	// DonR 09Dec2021 User Story #205423: Added new Limit Methods 7 and 8, and changed from an "if" to
	// a "switch" for clarity.
	if (DrugToCheck->qty_lim_ishur_num == 0)
	{
		switch (DrugToCheck->PurchaseLimitMethod)
		{
			case 5:
			case 6:
			case 7:
			case 8:
						DrugToCheck->qty_limit_chk_type = RX_SOURCE_PURCHASE_LIM;
						break;

			default:
						DrugToCheck->qty_limit_chk_type = STD_PURCHASE_LIMIT;
						break;
		}
	}
	else
	{
		DrugToCheck->qty_limit_chk_type = PURCHASE_LIMIT_ISHUR;
	}


    return 0;
}
// End of test_purchase_limits ()



/*=======================================================================
||																		||
||			test_pharmacy_ishur ()										||
||																		||
|| RETURNs:																||
|| 0 = OK.																||
|| 1 = high severity error occurred.									||
|| 2 = deadlock, caller should restart transaction.						||
 =======================================================================*/
TErrorCode	test_pharmacy_ishur (int						   	PharmIn,
								 TRecipeSource					RecipeSourceIn,
								 TMemberIdentification			MemIdIn,
								 TIdentificationCode			IdExtIn,
								 TMemberRights					MemberRightsIn,
								 TMemberDiscountPercent			MemberDiscountPercentIn,
								 int							MemberIllnessBitmapIn,
								 TTypeDoctorIdentification		DocIdType_in,
								 TDoctorIdentification			DocId_in,
								 TPresDrugs						*DrugPtrIn,
								 int							AsIf_Preferred_Largo_in,
								 TNumofSpecialConfirmation		PharmIshurNumIn,
								 TSpecialConfirmationNumSource	PharmIshurSrcIn,
								 TRecipeIdentifier				PrescriptionIdIn,
								 short int						LineNumIn,
								 short int						TrnNumIn,
								 int							PharmIshurProfessionIn,
								 TErrorCode						*ErrorCodeOut)
{
	ISOLATION_VAR;

	// SQL versions of function arguments.
	TPharmNum						v_PharmNum		= PharmIn;
	TMemberIdentification			v_MemberId		= MemIdIn;
	TIdentificationCode				v_MemIdExt		= IdExtIn;
	TTypeDoctorIdentification		v_DocIdType		= DocIdType_in;
	TDoctorIdentification			v_DocID			= DocId_in;
	TPresDrugs						*v_DrugPtr		= DrugPtrIn;
	TNumofSpecialConfirmation		v_PharmIshurNum	= PharmIshurNumIn;
	TSpecialConfirmationNumSource	v_PharmIshurSrc	= PharmIshurSrcIn;
	int								v_PrefLargo		= AsIf_Preferred_Largo_in;

	// From pharmacy_ishur table.
	TMemberParticipatingType		v_PhIshurPrtCode;
	short							v_PhIshurInsuranceUsed;
	int								v_PhIshurFixedPtn;
	TRuleNum						v_RuleNumber;
	TErrorCode						v_PhIshurErrorCode;
	TParticipatingPrecent			v_PhIshurPtnPercent;
	float							v_PhIshurPtnAmount;
	TRecipeIdentifier				v_PhIshurUsedPrescId	= PrescriptionIdIn;
	short int						v_PhIshurUsedLineNum	= LineNumIn;
	short int						v_PhIshurUpdTrnNum		= TrnNumIn;
	short int						v_ishur_unused			= ISHUR_UNUSED;
	short int						v_ishur_used			= ISHUR_USED;
	short int						v_TreatmentCategory;
	short int						v_in_health_pack;
	short int						v_no_presc_sale_flag;
	short							needs_29_g;
	int								v_PhIshurRrn;
	int								v_Profession			= PharmIshurProfessionIn;

	// From drug-line structure.
	TParticipatingPrecent			v_CurrentPtnPercent;
	float							v_CurrentPtnAmount;

	// Miscellaneous.
	TDate							v_SysDate;
	THour							v_SysTime;
	TErrorCode						RtnValue	= 0;
	TErrorCode						err;
	int								rows_found	= 0;
	short							UsingAS400Ishur;


	// Initialize variable(s).
	REMEMBER_ISOLATION;
	v_SysDate = GetDate ();
	v_SysTime = GetTime ();
	*ErrorCodeOut = NO_ERROR;	// Assume error-out argument needs initialization.

	// Dummy loop to avoid GOTO's and multiple exit points.
	do
	{
		// Test for instant-exit conditions.
		// DonR 01Aug2019: Disabled the instant-exit condition when we already know we're getting participation
		// from an AS/400 "ishur kaspi". This was a bug, because we still need to mark the ishur as "used"!
		if ((v_DrugPtr->DFatalErr		== MAC_TRUE)					||	// Don't bother if drug already has serious problems.
//			(v_DrugPtr->SpecPrescNum	>  0)							||	// AS400 ishur overrides all, so don't bother.
			(RecipeSourceIn				== RECIP_SRC_EMERGENCY_NO_RX)	||	// Emergency supply is forced to 100% participation,
																			// so pharmacy ishurim are irrelevant.
			(v_PharmIshurNum			<= 0))								// No pharmacy ishur, so there's nothing to do.
		{
			RtnValue = 0;
			break;
		}

		// DonR 01Aug2019: Set a variable to indicate whether we're getting participation from an AS/400 ishur.
		// If we are, the only Pharmacy Ishur we're interested in is one that was used to extend the AS/400 ishur;
		// any other Pharmacy Ishurim are irrelevant, since they weren't used to grant participation to the member.
		UsingAS400Ishur = (v_DrugPtr->SpecPrescNum > 0) ? 1 : 0;


		// See if there really is a pharmacy ishur for this drug. If not, no problem - just exit.
		// DonR 05Jul2005: Split the select into two (primary and fallback) to eliminate a
		// cardinality violation. The violation is still possible if there is no ishur for the
		// drug being sold, but there are two or more ishurim with other largo codes as the
		// primary drug and the preferred largo as the secondary drug. I don't think this will
		// happen much (if at all) in real life.
		// DonR 11Feb2011: Tuned WHERE clause for better performance.
		// DonR 01Aug2019: If we're getting participation from an AS/400 ishur, *only* Rule #3
		// (AS/400 Ishur Extension) Pharmacy Ishurim are visible to this SELECT; and if we're
		// not getting participation from an AS/400 Ishur, Rule #3 Pharmacy Ishurim are invisible
		// (since we didn't find an AS/400 ishur to extend).
		SET_ISOLATION_COMMITTED;

		ExecSQL (	MAIN_DB,	TestPharmacyIshur_READ_pharmacy_ishur,
								TestPharmacyIshur_MainLargo,
					&v_PhIshurPrtCode,			&v_PhIshurFixedPtn,
					&v_PhIshurInsuranceUsed,	&v_RuleNumber,
					&v_PhIshurErrorCode,		&v_TreatmentCategory,
					&v_PhIshurRrn,				&v_in_health_pack,
					&v_no_presc_sale_flag,		&needs_29_g,
					&v_PharmNum,				&v_MemberId,
					&v_MemIdExt,				&v_PharmIshurNum,
					&v_DrugPtr->DrugCode,		&v_SysDate,
					&UsingAS400Ishur,			&UsingAS400Ishur,
					&v_ishur_unused,			END_OF_ARG_LIST				);

		// If there was no pharmacy ishur for the drug being sold, try the preferred
		// alternative drug. If still not found, no problem - just exit.
		// DonR 01Aug2019: If we're getting participation from an AS/400 ishur, *only* Rule #3
		// (AS/400 Ishur Extension) Pharmacy Ishurim are visible to this SELECT; and if we're
		// not getting participation from an AS/400 Ishur, Rule #3 Pharmacy Ishurim are invisible
		// (since we didn't find an AS/400 ishur to extend).
		if (SQLERR_code_cmp (SQLERR_not_found) == MAC_TRUE)
		{
			ExecSQL (	MAIN_DB,	TestPharmacyIshur_READ_pharmacy_ishur,
									TestPharmacyIshur_PreferredLargo,
						&v_PhIshurPrtCode,			&v_PhIshurFixedPtn,
						&v_PhIshurInsuranceUsed,	&v_RuleNumber,
						&v_PhIshurErrorCode,		&v_TreatmentCategory,
						&v_PhIshurRrn,				&v_in_health_pack,
						&v_no_presc_sale_flag,		&needs_29_g,
						&v_PharmNum,				&v_MemberId,
						&v_MemIdExt,				&v_PharmIshurNum,
						&v_DrugPtr->DrugCode,		&v_SysDate,
						&UsingAS400Ishur,			&UsingAS400Ishur,
						&v_ishur_unused,			&v_PrefLargo,
						END_OF_ARG_LIST										);
		}

		if (SQLERR_code_cmp (SQLERR_not_found) == MAC_TRUE)
		{
			// No ishur found is OK - just return 0.
			RtnValue = 0;
			break;
		}

		if (SQLERR_code_cmp (SQLERR_access_conflict) == MAC_TRUE)
		{
			LOG_CONFLICT ();
			RtnValue = 2;
			break;
		}

		if (SQLERR_error_test ())
		{
			*ErrorCodeOut = ERR_DATABASE_ERROR;
			RtnValue = 1;
			break;
		}

		// If we get here, we've found a pharmacy ishur that *may* be applicable.

		// DonR 11Sep2012: Set up a general default indicating that the pharmacy ishur gives
		// 200% participation; this will prevent assigning participation based on the ishur
		// unless it turns out that the ishur actually does provide participation lower than
		// 100%.
		// DonR 23Sep2012: Bug fix: we don't want to zero out v_PhIshurFixedPtn here, since it may have a
		// valid fixed price from drug_extension!
		v_PhIshurPtnPercent	= 20000;


		switch (v_RuleNumber)
		{
			// First, handle specialist-participation ishurim.
			case NON_MAC_SPECIALIST:
			case MAC_SPECIALIST_NOTE:

				// DonR 01Apr2008: Per Iris Shaya, disable specialist participation set by
				// Pharmacy Ishur if the drug being sold was prescribed by a dentist or
				// a home-visit. In either of these cases, any applicable specialist participation
				// will be used automatically, and specialist participation that might be
				// available for other professions is irrelevant.
				//
				// DonR 10Dec2009: When a Pharmacy Ishur authorizes specialist participation, we
				// now use the "normal" logic in test_mac_doctor_drugs_electronic() to determine
				// exactly what participation to use. Accordingly, all we want to do here is the
				// minimum necessary to assign the ishur number to the drug sale, and update the
				// pharmacy ishur appropriately.
				//
				// DonR 11Sep2012: Moved this code into the switch statement.
				if ((RecipeSourceIn						!= RECIP_SRC_DENTIST)		&&
					(RecipeSourceIn						!= RECIP_SRC_HOME_VISIT)	&&
					(v_DrugPtr->ret_part_source.table	== FROM_PHARMACY_ISHUR)		&&
					(v_DrugPtr->WhySpecialistPtnGiven	== PHARMACY_ISHUR))
				{
					// Copy Pharmacy Ishur Number/Source to drug sale structure.
					v_DrugPtr->SpecPrescNum						= v_PharmIshurNum;
					v_DrugPtr->SpecPrescNumSource				= v_PharmIshurSrc;

					// Update Pharmacy Ishur row.
					ExecSQL (	MAIN_DB, TestPharmacyIshur_UPD_pharmacy_ishur_specialist,
								&v_ishur_used,				&v_PhIshurUsedPrescId,
								&v_PhIshurUsedLineNum,		&v_DrugPtr->RetPartCode,
								&v_DrugPtr->PriceSwap,		&v_DrugPtr->ret_part_source.insurance_used,
								&v_DrugPtr->in_health_pack,	&v_Profession,
								&v_SysDate,					&v_SysTime,
								&v_PhIshurUpdTrnNum,		&NOT_REPORTED_TO_AS400,
								&v_PhIshurRrn,				END_OF_ARG_LIST									);

					if (SQLERR_code_cmp (SQLERR_access_conflict) == MAC_TRUE)
					{
						LOG_CONFLICT ();
						RtnValue = 2;
					}
					else
					if (SQLERR_error_test ())
					{
						*ErrorCodeOut = ERR_DATABASE_ERROR;
						RtnValue = 1;
					}

				}	// Specialist participation was granted because of Pharmacy Ishur.

				// DonR 08Feb2018: If this Pharmacy Ishur is dealing with specialist participation, the *only*
				// place where participation decisions should be made is in the regular participation-determination
				// routines - not here. In order to prevent stuff (like the why-specialist-participation value)
				// being overwritten, set the fixed-price value for the ishur to zero. (The percentage value already
				// has a value of 200%.)
				v_PhIshurFixedPtn = 0;	// I.e. disabled; the participation percent already defaults to 200%.

//				// DonR 23Sep2012: Just to be paranoid, make sure we don't use this ishur to set participation.
//				else
//				{
//					v_PhIshurFixedPtn = 0;	// I.e. disabled; the participation percent already defaults to 200%.
//				}
//
				break;


			// DonR 04Sep2003: If the Pharmacy Ishur is a request to use an AS400 Special Prescription
			// past its expiry date, no participation information will be stored in the Pharmacy Ishur
			// row. One of three things will be true: either the AS400 Special Prescription was found, in
			// which case either it was OK and SpecPrescNum will be set for this drug (and we'll already
			// have exited this function); or the AS400 Special Prescription wasn't found and we need to
			// retrieve the error from the Pharmacy Ishur and return it to the calling routine; or the
			// AS400 Special Prescription was found and couldn't be used for some reason. To make this
			// last case work properly, we've already made sure that the Pharmacy Ishur specifies 200%
			// participation so that whatever values have already been computed for the drug will be
			// left alone.
			case EXPIRED_AS400_ISHUR:
			case LETTER_IN_HAND:
			case TREATMENT_COMMITTEE:

				v_PhIshurFixedPtn = 0;	// I.e. disabled; the participation percent already defaults to 200%.
				break;


			// Handle drug_extension-based ishurim.
			default:
				// DonR 25Dec2007: Moved discount calculation below, after the point
				// where we actually decide to use the Pharmacy Ishur.

				// DonR 07Mar2011: Test whether pharmacy issued an ishur based on a prescription-only
				// rule, when the current sale is without a prescription.
				if ((v_no_presc_sale_flag == 0) && (RecipeSourceIn == RECIP_SRC_NO_PRESC))
				{
					SetErrorVarArr (&v_DrugPtr->DrugAnswerCode,
									ERR_PHARM_ISHUR_RULE_PRESC_ONLY,
									v_DrugPtr->DrugCode, LineNumIn,
									NULL,
									NULL);
					RESTORE_ISOLATION;
					return (0);
				}

				// If this ishur specifies a rule from the drug_extension table AND the rule specifies
				// a treatment category (such as fertility or dialysis), make sure the prescribing
				// doctor is allowed to prescribe this drug for this purpose. We test this by seeing
				// if an appropriate row exists in doctor_percents.
				// DonR 15Feb2011: Tuned WHERE clause for better performance.
				if ((v_TreatmentCategory > TREATMENT_TYPE_GENERAL) && (v_TreatmentCategory < TREATMENT_TYPE_MAX))
				{
					SET_ISOLATION_DIRTY;

					ExecSQL (	MAIN_DB, CheckDoctorPercentsTreatmentCategory,
								&rows_found,
								&v_DrugPtr->DrugCode,	&v_TreatmentCategory,
								&v_DocIdType,			&v_DocID,
								&v_DocIdType,			&v_DocID,
								&ROW_NOT_DELETED,		END_OF_ARG_LIST			);

					if (SQLERR_error_test ())
					{
						*ErrorCodeOut = ERR_DATABASE_ERROR;
						RESTORE_ISOLATION;
						return (1);
					}

					if (rows_found < 1)
					{
						// Doctor can't prescribe this drug for this treatment category. Set drug's error
						// (unless we've already seen a worse error for this drug) and return to calling
						// routine.
						SetErrorVarArr (&v_DrugPtr->DrugAnswerCode,
										ERR_BAD_TREAT_CATEGORY_FOR_DOC,
										v_DrugPtr->DrugCode, LineNumIn,
										NULL,
										NULL);

						RESTORE_ISOLATION;
						return (0);
					}	// Doctor not authorized to prescribe for this Treatment Category.
				}	// Need to validate Drug Extension rule & prescribing doctor against Doctor Percents.

				// If we get here, we have an applicable drug_extension-based pharmacy ishur. Now validate
				// its Price Code.
				err = GET_PARTICIPATING_PERCENT (v_PhIshurPrtCode, &v_PhIshurPtnPercent);

				if (err != NO_ERROR)
				{
					// This shouldn't really happen, since the participation codes are tested
					// in Transaction 2033. But just in case, restore the bogus default value.
					// DonR 10Sep2012: Use 200% to make sure we don't assign an invalid participation code.
					v_PhIshurPtnPercent = 20000;
				}
		}	// Switch statement.


// What about drug_extension quantity limits?


		// OK, we read a Pharmacy Ishur. Now see if it's useful to us.

		// The drug's default participation has already been checked and re-checked -
		// so we don't have to worry about error-trapping here. I hope.
		GET_PARTICIPATING_PERCENT (v_DrugPtr->RetPartCode, &v_CurrentPtnPercent);


		// Decide what values to use.
		if (v_PhIshurErrorCode == NO_ERROR)	// I.e. no error was stored in the pharmacy_ishur row.
		{
			// No errors detected, so compare the levels of participation.
			v_PhIshurPtnAmount =
				(v_PhIshurFixedPtn > 0)		? (float)v_PhIshurFixedPtn
											: ((float)v_DrugPtr->RetOpDrugPrice * (float)v_PhIshurPtnPercent / 10000.0);

			v_CurrentPtnAmount =
				(v_DrugPtr->PriceSwap > 0)	? (float)v_DrugPtr->PriceSwap
											: ((float)v_DrugPtr->RetOpDrugPrice * (float)v_CurrentPtnPercent / 10000.0);


			if (v_PhIshurPtnAmount <= v_CurrentPtnAmount)	// Pharm. Ishur gives lower participation.
			{
				// DonR 25Dec2007: Moved discount logic from up above, since there's no reason
				// to assign member discounts based on the Pharmacy Ishur until we actually
				// know that we're going to use the Pharmacy Ishur.
				switch (v_RuleNumber)
				{
					case EXPIRED_AS400_ISHUR:
					case LETTER_IN_HAND:
					case TREATMENT_COMMITTEE:
					case NON_MAC_SPECIALIST:	// Shouldn't get here!
					case MAC_SPECIALIST_NOTE:	// Shouldn't get here!

						break;


					default:	// I.e. a drug_extension rule.
// WORKINGPOINT: Is this discount calculation still necessary? At first glance, I think it's redundant.
						// See if we need to grant patient a discount based on whether the rule
						// or specialist participation specifies that it is part of the health basket.
						if ((v_in_health_pack != 0) && (v_DrugPtr->in_health_pack == 0))
						{
							// The logic below is copied from Transaction 2003. It has to be
							// duplicated because this routine is called after the point where
							// discounts are normally awarded - and at that point, the program
							// doesn't yet know that there is a rule specifying that this
							// treatment is in the health basket under these conditions.
							//
							// DonR 25Dec2007: Per Iris Shaya, member discounts are no longer
							// conditioned on any particular participation code.
							if ( (RecipeSourceIn != RECIP_SRC_NO_PRESC)													&&
								 ((MemberRightsIn	== 7) || (MemberRightsIn == 17) || (MemberDiscountPercentIn > 0))	&&
								 ((v_DrugPtr->DL.largo_type == 'T') || (v_DrugPtr->DL.largo_type	== 'M')) )
							{
								if (((MemberRightsIn == 7)		|| (MemberRightsIn == 17))	&&
									((!MemberIllnessBitmapIn)	|| (MemberIllnessBitmapIn & v_DrugPtr->DL.illness_bitmap)))
								{
									v_DrugPtr->AdditionToPrice = 10000;
								}
								else
								{
									// DonR 12Jan2004: Make sure we don't overwrite a big
									// discount with a little one.
									if (MemberDiscountPercentIn > v_DrugPtr->AdditionToPrice)
										v_DrugPtr->AdditionToPrice = MemberDiscountPercentIn;
								}	// else - not equal to 7 or 17.
							}	// Eligible prescription sale AND member is entitled to a discount.
						}	// Health Basket status changed from 0 to 1.

						// DonR 25Dec2007: Added ELSE to turn discount off when the Pharmacy
						// Ishur changes Health Basket status from 1 to 0.
						else

						{
							if ((v_in_health_pack == 0) && (v_DrugPtr->in_health_pack != 0))
							{
								v_DrugPtr->AdditionToPrice = 0;
							}
						}

						// Now that we've (re-)set the discount stuff, assign the actual
						// Health Basket status to the drug sale.
						// DonR 09Aug2017: Also store the "Needs 29-G" value from the drug_extension rule.
						v_DrugPtr->in_health_pack	= v_in_health_pack;
						v_DrugPtr->rule_needs_29g	= needs_29_g;

				}	// End of switch statement.


				// Pharmacy Ishur gives lower (or, worst case, equal) participation - as it should.
				if (v_PhIshurFixedPtn > 0)
				{
					v_DrugPtr->PriceSwap	= v_PhIshurFixedPtn;
					v_DrugPtr->RetPartCode	= 1;	// DonR 16Dec2003: Ptn. is always 100% with fixed price.
				}
				else
				{
					v_DrugPtr->RetPartCode	= v_PhIshurPrtCode;
					v_DrugPtr->PriceSwap	= 0;	// DonR 16Dec2003: Fixed price is always zero with ptn. < 100%.
				}

				v_DrugPtr->PrticSet							= MAC_TRUE;
				v_DrugPtr->ret_part_source.table			= FROM_PHARMACY_ISHUR;
				v_DrugPtr->ret_part_source.insurance_used	= v_PhIshurInsuranceUsed;
				v_DrugPtr->SpecPrescNum						= v_PharmIshurNum;
				v_DrugPtr->SpecPrescNumSource				= v_PharmIshurSrc;
				v_DrugPtr->rule_number						= v_RuleNumber;		// DonR 11Feb2018. Note that we should get here only if the pharmacy
																				// ishur is using a drug_extension rule and actually setting the participation.

				// If we're getting participation from the Pharmacy Ishur but the Ishur involves
				// something other than a specialist prescription, zero the "why" flag so we don't
				// confuse the AS/400.
				v_DrugPtr->WhySpecialistPtnGiven = 0;

			}	// Pharm. Ishur gives lower participation than we would have otherwise.
			else
			{
				// Pharmacy Ishur gives higher participation than "normal" - something's strange!
				// (Unless the Pharmacy Ishur wasn't intended to specify a participation level -
				// thus the error code is conditional. DonR 04Sep2003)
				// DonR 10Sep2012: Suppress this message for all non-"nohal" pharmacy ishurim.
				switch (v_RuleNumber)
				{
					case EXPIRED_AS400_ISHUR:
					case LETTER_IN_HAND:
					case TREATMENT_COMMITTEE:
					case NON_MAC_SPECIALIST:
					case MAC_SPECIALIST_NOTE:

						break;


					default:	// I.e. a drug_extension rule.
						SetErrorVarArr (&v_DrugPtr->DrugAnswerCode,
										ERR_PHARM_ISHUR_PRICE_HIGHER,
										v_DrugPtr->DrugCode, LineNumIn,
										NULL,
										NULL);
				}
			}	// Pharmacy Ishur participation is higher than what we would get otherwise.


			// It may be necessary to add code here to suppress certain DrugAnswerCode
			// values from "normal" processing - in particular, those dealing with
			// minor participation errors.

		}	// No error was stored in the pharmacy_ishur row.

		else
		// Some error was found when the Pharmacy Ishur was sent in Transaction 2033 - so report that
		// error now.
		{
			SetErrorVarArr (&v_DrugPtr->DrugAnswerCode,
							v_PhIshurErrorCode,
							v_DrugPtr->DrugCode, LineNumIn,
							NULL,
							NULL);

		}	// Pharmacy Ishur has error code (set by Trn. 2033) for this drug.



		// Mark Pharmacy Ishur row as Used.
		ExecSQL (	MAIN_DB, TestPharmacyIshur_UPD_pharmacy_ishur_MarkUsedOnly,
					&v_ishur_used,			&v_PhIshurUsedPrescId,
					&v_PhIshurUsedLineNum,	&v_SysDate,
					&v_SysTime,				&v_PhIshurUpdTrnNum,
					&NOT_REPORTED_TO_AS400,	&v_PhIshurRrn,
					END_OF_ARG_LIST													);

		if (SQLERR_code_cmp (SQLERR_access_conflict) == MAC_TRUE)
		{
			LOG_CONFLICT ();
			RtnValue = 2;
			break;
		}

		if (SQLERR_error_test ())
		{
			*ErrorCodeOut = ERR_DATABASE_ERROR;
			RtnValue = 1;
			break;
		}

	}
	while (0);	// End of dummy loop.


	RESTORE_ISOLATION;
	return (RtnValue);

}
// End of test_pharmacy_ishur ()



/*=======================================================================
||																		||
||			check_sale_against_pharm_ishur ()							||
||																		||
|| RETURNs:																||
|| 0 = OK.																||
|| 1 = high severity error occurred, ErrorCodeOut set.					||
|| 2 = deadlock, caller should restart transaction.						||
 =======================================================================*/
TErrorCode	check_sale_against_pharm_ishur (TPharmNum					   	PharmIn,
											TMemberIdentification			MemIdIn,
											TIdentificationCode				IdExtIn,
											TPresDrugs						*DrugPtrIn,
											int								NumDrugsIn,
											TNumofSpecialConfirmation		PharmIshurNumIn,
											TSpecialConfirmationNumSource	PharmIshurSrcIn,
											short							ExtendAS400IshurDaysOut[],
											int								PharmIshurProfessionOut[],
											TErrorCode						*ErrorCodeOut)
{
	ISOLATION_VAR;

	// SQL versions of function arguments.
	TPharmNum						v_PharmNum		= PharmIn;
	TMemberIdentification			v_MemberId		= MemIdIn;
	TIdentificationCode				v_MemIdExt		= IdExtIn;
	TPresDrugs						*v_DrugPtr		= DrugPtrIn;
	TNumofSpecialConfirmation		v_PharmIshurNum	= PharmIshurNumIn;
	TSpecialConfirmationNumSource	v_PharmIshurSrc	= PharmIshurSrcIn;

	// From pharmacy_ishur table.
	TDrugCode						v_PhIshurLargo;
	short int						v_PhIshurUsedFlg;
	short int						v_PhIshurExtendDays;
	int								v_SpecialityCode;

	// Miscellaneous.
	TDate							v_SysDate;
	TErrorCode						RtnValue = 0;
	int								found;
	int								i;
	int								PharmIshurRowCount		= 0;
	int								PharmIshurUnsoldDrugs	= 0;
	int								PharmIshurAlreadyUsed	= 0;
	int								CursorDeclared			= 0;


	// Initialize variable(s).
	REMEMBER_ISOLATION;
	v_SysDate = GetDate ();
	*ErrorCodeOut = NO_ERROR;	// Assume error-out argument needs initialization.


	// Dummy loop to avoid GOTO's and multiple exit points.
	do
	{
		// Test for instant-exit conditions.
		if ((NumDrugsIn <= 0) || (v_PharmIshurNum <= 0) || (v_PharmIshurSrc != 2))
		{
			RtnValue = 0;
			break;
		}

		SET_ISOLATION_DIRTY;

		// DonR 01Nov2006: Added system-date select to cursor, so we don't generate
		// duplicate-ishur errors from previous-month ishurim.
		// DonR 15Feb2011: For performance, shuffled WHERE criteria to conform to index structure.
		// DonR 25Mar2020: Declare cursor only once we know that the pharmacy is selling subject to
		// a pharmacy ishur; and declare it only the first time in the retry loop.
		if (!CursorDeclared)
		{
			DeclareCursorInto (	MAIN_DB, check_sale_against_pharm_ishur_cur,
								&v_PhIshurLargo,		&v_PhIshurUsedFlg,
								&v_SpecialityCode,		&v_PhIshurExtendDays,
								&v_PharmNum,			&v_MemberId,
								&v_MemIdExt,			&v_PharmIshurNum,
								&v_SysDate,				END_OF_ARG_LIST			);
			CursorDeclared = 1;
		}

		OpenCursor (	MAIN_DB, check_sale_against_pharm_ishur_cur	);

		if (SQLERR_code_cmp (SQLERR_access_conflict) == MAC_TRUE)
		{
			LOG_CONFLICT ();
			*ErrorCodeOut = ERR_DATABASE_ERROR;
			RtnValue = 2;
			break;
		}

		if (SQLERR_error_test ())
		{
			*ErrorCodeOut = ERR_DATABASE_ERROR;
			RtnValue = 1;
			break;
		}


		do
		{
			FetchCursor (	MAIN_DB, check_sale_against_pharm_ishur_cur	);

			if (SQLERR_code_cmp (SQLERR_end_of_fetch) == MAC_TRUE)
			{
				break;
			}

			if (SQLERR_code_cmp (SQLERR_access_conflict) == MAC_TRUE)
			{
				LOG_CONFLICT ();
				*ErrorCodeOut = ERR_DATABASE_ERROR;
				RtnValue = 2;
				break;
			}

			if (SQLERR_error_test ())
			{
				*ErrorCodeOut = ERR_DATABASE_ERROR;
				RtnValue = 1;
				break;
			}


			// We've read something!
			PharmIshurRowCount++;

			if (v_PhIshurUsedFlg != ISHUR_UNUSED)
				PharmIshurAlreadyUsed++;

			// Each drug in the Pharmacy Ishur MUST be in the Sale Request as well!	
			found = MAC_FALS;

			for (i = 0; i < NumDrugsIn; i++)
			{
				if (v_DrugPtr[i].DrugCode == v_PhIshurLargo)
				{
					found = MAC_TRUE;

					// Store the value of as400_ishur_extend so that the calling routine will know
					// if this Pharmacy Ishur row called for the extension of an expired AS400
					// Special Prescription. If the Pharmacy Ishur row is something other than
					// a Special Prescription extension, as400_ishur_extend will be zero. We have
					// to store this information now, since we'll need to send it to
					// test_special_prescription(), which is called *before* test_pharmacy_ishur().
					ExtendAS400IshurDaysOut[i] = v_PhIshurExtendDays;

					// DonR 07Dec2009: Store the Profession Code in an array as well. There are
					// three possibilities: If the Pharmacy Ishur has a zero here, the Ishur is
					// for something other than granting specialist participation; if it has a
					// positive number, that number is a Profession Code which will be used
					// (in test_mac_doctor_drugs_electronic() ) to determine specialist participation;
					// and if the Ishur has a -1 here, the ishur is for specialist participation
					// but the pharmacy has not specified which type of specialist is involved. In
					// the latter case, test_mac_doctor_drugs_electronic() will apply the same logic
					// used for residents of old-age homes.
					PharmIshurProfessionOut[i] = v_SpecialityCode;

					break;
				}
			}

			if (!found)
				PharmIshurUnsoldDrugs++;

		}
		while (1);
		// Cursor loop.


		if (CursorDeclared)
			CloseCursor (	MAIN_DB, check_sale_against_pharm_ishur_cur	);


		// If we've hit a severe error in the inner loop, skip analysis of fetch results.
		if (RtnValue != 0)
			break;

		// Assign error code in order of increasing severity (more or less).
		if (PharmIshurUnsoldDrugs > 0)
		{
			*ErrorCodeOut = ERR_PHARM_ISHUR_NOT_ALL_USED;
			RtnValue = 1;
		}

		if (PharmIshurAlreadyUsed > 0)
		{
			*ErrorCodeOut = ERR_PHARM_ISHUR_ALREADY_USED;
			RtnValue = 1;
		}

		if (PharmIshurRowCount == 0)
		{
			*ErrorCodeOut = ERR_PHARM_ISHUR_NOT_FOUND;
			RtnValue = 1;
		}
	}
	while (0);
	// Dummy loop.


	RESTORE_ISOLATION;
	return (RtnValue);
}
// End of check_sale_against_pharm_ishur ()



// DonR 27Feb2008: test_dentist_drugs() has been replaced
// by the generic version, test_special_drugs().

/*=======================================================================
||																		||
||			   test_special_drugs()										||
||																		||
|| Test if drugs allowed to dentist or home-visit.						||
||																		||
|| This function is macro'd as either test_dentist_drugs				||
|| or test_home_visit_drugs.											||
||																		||
|| RETURNs:																||
|| 0 = OK.																||
|| 1 = high severity error occurred.									||
|| 2 = deadlock, caller should restart transaction.						||
 =======================================================================*/
int	test_special_drugs (TPresDrugs					*presdrugs,
						TNumOfDrugLinesRecs			drugcnt,
						TMemberParticipatingType	presParti,
						int							RuleStart_in,
						TErrorCode					*ErrorCode)
{
    int				i;
    TErrorCode		err = NO_ERROR;


	// DonR 27Feb2008: Implement this function as a looping call to
	// test_special_drugs_electronic(), which does exactly the same
	// thing as this function did, but to one drug at a time.
    for (i = 0; i < drugcnt; i++)
	{
		if (presdrugs[i].DFatalErr == MAC_TRUE)
			continue;

		err = test_special_drugs_electronic (&presdrugs[i], RuleStart_in, ErrorCode);

		// As in the old version, as soon as we hit a serious error, break
		// out of the loop.
		if (err != 0)
			break;
	}

	return (err);
}
// End of test_special_drugs()



// DonR 23Feb2008: test_dentist_drugs_electronic() has been replaced
// by the generic version, test_special_drugs_electronic().

/*=======================================================================
||																		||
||			   test_special_drugs_electronic()							||
||																		||
|| Test if drugs allowed to dentist or home-visit.						||
||																		||
|| This function is macro'd as either test_dentist_drugs_electronic		||
|| or test_home_visit_drugs_electronic.									||
||																		||
|| RETURNs:																||
|| 0 = OK.																||
|| 1 = high severity error occurred.									||
|| 2 = deadlock, caller should restart transaction.						||
 =======================================================================*/
int	test_special_drugs_electronic (TPresDrugs					*presdrug,
								   int							RuleStart_in,
								   TErrorCode					*ErrorCode)
{
	// Local declarations.
    TErrorCode		err;
    TErrorCode		rtn_err;
	ISOLATION_VAR;

	// Database variables.
	TDrugCode				v_drug		= presdrug->DrugCode;
	int						RuleStart	= RuleStart_in;
	int						RuleEnd		= RuleStart_in + 999;
	short					price_code;
	short					health_basket;
	int						fixed_price;
	char					insurance_type[2];

	
	// Assume error-out argument needs initialization.
	*ErrorCode = rtn_err = NO_ERROR;


	// DonR 27Feb2008: For dentist and home-visit prescriptions, the
	// default participation is 100% even if drug_list specifies
	// something less. Only an entry in spclty_largo_prcnt will give
	// the member less than 100% participation.
	// DonR 01Apr2008: Default conditions include not being in the health basket.
	presdrug->BasePartCode			= presdrug->RetPartCode = 1;	// 100%
	presdrug->ret_part_source.table	= FROM_NO_TABLE;	// DonR 20Feb2008.
	presdrug->ret_part_source.state	= CURR_CODE;
	presdrug->in_health_pack		= 0;				// DonR 01Apr2008.


	// DonR 16Jan2005: Don't bother looking in database if we already
	// know this isn't a specialist drug.
	//
	// DonR 22Nov2011: Because Magen Kesef and Magen Zahav are being "separated", there will now be
	// separate rows in spclty_largo_prcnt for the two supplemental insurance types - as well as
	// possibly a separate row for basic insurance. (However, we are guaranteed to have only one set
	// of rows for any given Tzahal status: Maccabi Member, Tzahal Hova, or Tzahal Keva.) Accordingly,
	// we need to read this table using a cursor rather than a singleton SELECT.
	if (presdrug->DL.specialist_drug > 0)
	{
		REMEMBER_ISOLATION;
		SET_ISOLATION_DIRTY;

		DeclareAndOpenCursorInto (	MAIN_DB, TestSpecialDrugs_SLP_cur,
									&price_code,			&fixed_price,
									&insurance_type,		&health_basket,

									&v_drug,				&ROW_NOT_DELETED,
									&Member.MemberMaccabi,	&Member.MemberHova,
									&Member.MemberKeva,		&RuleStart,
									&RuleEnd,				&Member.insurance_type,
									&Member.current_vetek,	&Member.prev_insurance_type,
									&Member.prev_vetek,		END_OF_ARG_LIST					);

		if (SQLCODE == 0)
		{
			FetchCursor (	MAIN_DB, TestSpecialDrugs_SLP_cur	);
		}

		// Test database errors
		if (SQLERR_code_cmp (SQLERR_access_conflict) == MAC_TRUE)
		{
			LOG_CONFLICT ();
			rtn_err = 2;
		}

		else
		{
			// No access conflict.
			//
			// If nothing was found, there's no error - just use the default of
			// 100% / "no table".
			if ((SQLERR_code_cmp (SQLERR_end_of_fetch)	!= MAC_TRUE)	&&
				(SQLERR_code_cmp (SQLERR_not_found)		!= MAC_TRUE))	// Row not found.
			{
				// Any other database error represents a problem of some sort.
				if (SQLERR_error_test())
				{
					*ErrorCode = ERR_DATABASE_ERROR;
					rtn_err = 1;
				}
				else	// We actually found something - apply it!
				{
					// We found something in Specialty Largo Percent - find correct price code.
					err = SimpleFindLowestPtn (price_code, fixed_price, insurance_type, health_basket, presdrug, 1);

					if (err == ERR_CODE_NOT_FOUND)
					{
						rtn_err = 1;	// This is not a "real" error.
					}
					else
					{
						if (err == NO_ERROR)
						{
							// Success! Hooray!
							presdrug->ret_part_source.table	= FROM_DOC_SPECIALIST;
							presdrug->WhySpecialistPtnGiven	= DENTIST_ETC;	// DonR 14Jan2009
						}
						else
						{	// This really shouldn't ever happen either!
							SetErrorVarArr (ErrorCode, err, 0, 0, &rtn_err, NULL);
						}
					}
				}	// Successful read of spclty_largo_prcnt.
			}	// We got some SQL return other than not-found or end-of-fetch.
		}	// No access conflict.

		CloseCursor (	MAIN_DB, TestSpecialDrugs_SLP_cur	);

		// Note that with a single exit point, we need only one call to
		// RESTORE_ISOLATION - and that's only needed if we've actually
		// tried to read from the database.
		RESTORE_ISOLATION;

	}	// The drug being sold is a specialist drug.


	return (rtn_err);
}
// End of test_special_drugs_electronic()



/*=======================================================================
||																		||
||			   test_mac_doctor_drugs_electronic()						||
||																		||
|| Test Participating precent for macabi doctor.						||
||	(new version for Electronic Prescriptions)							||
|| RETURNs:																||
|| 0 = OK.																||
|| 1 = high severity error occured.										||
|| 2 = Deadlock.														||
 =======================================================================*/

// DonR 10Dec2009: Macros for reasons to search for optimal specialist
// participation.
#define NO_REASON_APPLICABLE	0
#define OLD_AGE_HOME			1
#define PHARMACY_ISHUR_REQ		2
#define SPEC_LETTER_SEEN		3
#define PURCHASE_HISTORY		4

TErrorCode test_mac_doctor_drugs_electronic (TTypeDoctorIdentification	DocIdType_in,
											 TDoctorIdentification		docid,
											 TPresDrugs					*ThisDrug_in,
											 short						LineNo_in,
											 TAge						memberAge,
											 TMemberSex					memberGender,
											 short int					phrm_perm,
											 TRecipeSource				RecipeSource_in,
											 TMemberIdentification		MemberId_in,
											 TIdentificationCode		IdType_in,
											 short int					SpecialtySearchFlag,
											 int						*PharmIshurProfessionInOut,
											 TErrorCode					*ErrorCode)
{
	// Local declarations
	TErrorCode				err;
	TParticipatingPrecent	test_part_prcn;
	TParticipatingPrecent	presParti;
	DrugExtension			ext;
	TDoctorBelong			doc_belong;
	int						SumUnits;
	int						ExtUnits;
	int						ReasonForSpecPtn;
	short					err_flg;
	ISOLATION_VAR;


	TTypeDoctorIdentification	v_DocIdType		= DocIdType_in;
	TDoctorIdentification		v_DocID			= docid;
	Tlargo_code					v_Largo			= ThisDrug_in->DrugCode;
	short						v_chronic_flag	= ThisDrug_in->DL.chronic_flag;
	int							v_EPGroup		= ThisDrug_in->DL.economypri_group;
	int							v_ParentGroup	= ThisDrug_in->DL.parent_group_code;
	long						VisitNumber		= ThisDrug_in->VisitNumber;
	int							FirstPrId		= ThisDrug_in->FirstDocPrID;
	TTreatmentType				TreatmentType	= TREATMENT_TYPE_GENERAL;
	TPresDrugs					*ThisDrug		= ThisDrug_in;
	short						v_LowestPercent;
	TMemberIdentification		v_MemberID		= MemberId_in;
	TIdentificationCode			v_IdType		= IdType_in;
	int							SystemDate;
	int							TwoMonthsDate;
	int							SixMonthsDate;
	int							PastPurchases	= 0;
	int							SpecLetters		= 0;
	short						price_code;
	short						health_basket;
	int							fixed_price;
	char						insurance_type[2];

	// DonR 08Dec2009: Variables for new specialist-optimization logic.
	int							v_Profession		= *PharmIshurProfessionInOut;	// = 0 if no Pharm. Ishur.
	int							v_ProfessionFound	= 0;



	// Assume error-out argument needs initialization.
	REMEMBER_ISOLATION;
	*ErrorCode = NO_ERROR;

	// DonR 21Aug2003: Handle non-prescription drugs. These default to 100% participation.
	if ((RecipeSource_in == RECIP_SRC_NO_PRESC) || (RecipeSource_in == RECIP_SRC_EMERGENCY_NO_RX))
	{
		ThisDrug->BasePartCode = ThisDrug->RetPartCode = 1; // Maccabi 100%.
		ThisDrug->ret_part_source.table  = FROM_NO_TABLE;
	}

	// DonR 23Dec2012: Since we now look at drug_extension rules even if we found specialist participation,
	// make life simpler by quitting right away if we've already set participation from an AS/400 ishur.
	if ((ThisDrug->PrticSet == MAC_TRUE) && (ThisDrug->ret_part_source.table == FROM_SPEC_PRESCS_TBL))
		return (0);

	// Default participation now comes from Drug table, not from doctor's prescription.
	presParti = ThisDrug->BasePartCode;


	// DonR 14Jul2005: Avoid unnecessary record-locked errors.
	SET_ISOLATION_DIRTY;



	// Test for specialist participation.
	do	// Dummy loop to avoid nasty GOTO's.
	{
		// DonR 23Dec2012: Added code above to exit if participation has been set
		// by an AS/400 ishur - so we can get rid of the ThisDrug->PrticSet test here.
		// If we've already hit a fatal error for this drug, OR if this is a
		// non-prescription sale, quit the first dummy loop.
		if ((ThisDrug->DFatalErr	== MAC_TRUE)					||
			(RecipeSource_in		== RECIP_SRC_NO_PRESC)			||
			(RecipeSource_in		== RECIP_SRC_EMERGENCY_NO_RX))
			break;

		err_flg = 0;

		// Try to grant specialist participation if this is a specialist drug.
		if (ThisDrug->DL.specialist_drug == 0)
			break;	// Jump down to drug_extension test.

		// FIRST, if the drug was prescribed by a Maccabi doctor, try for a match based
		// on the prescribing doctor's speciality(s) and the Speciality Largo Percent table.
		if (v_DocIdType != 1)	// Skip this section if the prescribing physician is non-Maccabi.
		{
			// Declare cursor for retrieving percent codes from tables doctor_speciality,
			// spclty_largo_prcnt joined on speciality_code.
			// DonR 01Jan2010: Fixed "ambiguous column" error for speciality_code in WHERE clause.
			// DonR 15Feb2011: Re-ordered WHERE clause for better performance.
			// DonR 10Apr2011: Declare cursor only when necessary.
			DeclareAndOpenCursorInto (	MAIN_DB, TestMacDoctorDrugsElectronic_SpecialistPtn_1_cur,
										&price_code,			&fixed_price,
										&insurance_type,		&health_basket,

										&v_DocID,				&ROW_NOT_DELETED,
										&v_Largo,				&Member.MemberMaccabi,
										&Member.MemberHova,		&Member.MemberKeva,
										&ROW_NOT_DELETED,		&Member.insurance_type,
										&Member.current_vetek,	&Member.prev_insurance_type,
										&Member.prev_vetek,		END_OF_ARG_LIST						);
			
			if (SQLERR_error_test ())
			{
				err = ERR_DATABASE_ERROR;
				SetErrorVarArr (ErrorCode, err, 0, 0, &err_flg, NULL);
				break;
			}	// Failed to open cursor.

			// Loop through rows selected to find the best participation type.
			do
			{
				FetchCursor (	MAIN_DB, TestMacDoctorDrugsElectronic_SpecialistPtn_1_cur	);
				
				if (SQLERR_code_cmp (SQLERR_end_of_fetch) == MAC_TRUE)	// Normal exit from loop.
				{
					break;
				}
				
				if (SQLERR_error_test ())
				{
					err = ERR_DATABASE_ERROR;
					SetErrorVarArr (ErrorCode, err, 0, 0, &err_flg, NULL);
					break;
				}	// Error (other than end-of-fetch) reading cursor.


				// Successfully read a cursor row.
				// See if we've found a better participation code than the default.
				err = SimpleFindLowestPtn (price_code, fixed_price, insurance_type, health_basket, ThisDrug, 1);
				if ((err != NO_ERROR) && (err != ERR_CODE_NOT_FOUND))	// Invalid Participation Code found.
				{
					SetErrorVarArr (ErrorCode, err, 0, 0, &err_flg, NULL);
					break;
				}

				// Note that, unlike the old version, we don't quit as soon as we've found something
				// better than the default. We keep looping, to make sure we find the optimal
				// Participation Code.
				//
				// DonR 18Dec2007: Per Iris Shaya, if we're setting participation from
				// spclty_largo_prcnt, the "in health basket" value from spclty_largo_prcnt
				// should override the value from drug_list.
				// DonR 23Nov2011: Since we're now sorting spclty_largo_prcnt rows by increasing participation
				// percentage (with 100%/fixed price treated as 0.01%), we're guaranteed that the first match
				// is the best match. Therefore, once we've found a match, break out of the loop.
				if (err == NO_ERROR)
				{
					ThisDrug->ret_part_source.table = FROM_DOC_SPECIALIST;
					ThisDrug->WhySpecialistPtnGiven	= PRESCRIBED_BY_SPECIALIST;	// DonR 14Jan2009
					break;	// DonR 23Nov2011
				}	// GetParticipatingCode did not return an error.

			}	// Cursor-reading loop.
			while (1);

			CloseCursor (	MAIN_DB, TestMacDoctorDrugsElectronic_SpecialistPtn_1_cur	);
			SQLERR_error_test ();

			// If we've hit a severe error, exit the routine now.
			if (err_flg == 1)
			{
				// DonR 14Jul2005: Restore default isolation.
				RESTORE_ISOLATION;

				return (1);
			}

			// If we've found a match, we're finished with this section and we can go check
			// drug_extension stuff.
			if (ThisDrug->PrticSet == MAC_TRUE)
				break;
		}	// Prescribing doctor is a  Maccabi physician.


		// SECOND, if there was a Pharmacy Ishur for specialist participation in which
		// the pharmacy specified the Speciality Code involved, try reading the relevant
		// spclty_largo_prcnt row and see if it works for us.
		//
		// DonR 22Nov2011: Because Magen Kesef and Magen Zahav are being "separated", there will now be
		// separate rows in spclty_largo_prcnt for the two supplemental insurance types - as well as
		// possibly a separate row for basic insurance. However, we are guaranteed to have only one set
		// of rows for any given Tzahal status: Maccabi Member, Tzahal Hova, or Tzahal Keva. Accordingly,
		// we need to read this table using a cursor rather than a singleton SELECT.
		if (v_Profession > 0)
		{
			DeclareAndOpenCursorInto (	MAIN_DB, TestMacDoctorDrugsElectronic_SpecialistPtn_3_cur,
										&price_code,					&fixed_price,
										&insurance_type,				&health_basket,

										&v_Profession,					&v_Largo,
										&Member.MemberMaccabi,			&Member.MemberHova,
										&Member.MemberKeva,				&ROW_NOT_DELETED,
										&Member.insurance_type,			&Member.current_vetek,
										&Member.prev_insurance_type,	&Member.prev_vetek,
										END_OF_ARG_LIST												);

			if (SQLCODE == 0)
			{
				FetchCursor (	MAIN_DB, TestMacDoctorDrugsElectronic_SpecialistPtn_3_cur	);
			}

			if ((SQLERR_code_cmp (SQLERR_end_of_fetch)	!= MAC_TRUE)	&&
				(SQLERR_code_cmp (SQLERR_not_found)		!= MAC_TRUE))	// Row not found.
			{
				if (SQLERR_error_test ())
				{
					err = ERR_DATABASE_ERROR;
					SetErrorVarArr (ErrorCode, err, 0, 0, &err_flg, NULL);
					break;
				}	// Error (other than row-not-found) reading spclty_largo_prcnt.

				else
				// Successful read of spclty_largo_prcnt.
				{
					// See if we've found a better participation code than the default.
					err = SimpleFindLowestPtn (price_code, fixed_price, insurance_type, health_basket, ThisDrug, 1);
					if ((err != NO_ERROR) && (err != ERR_CODE_NOT_FOUND))	// Invalid Participation Code found.
					{
						SetErrorVarArr (ErrorCode, err, 0, 0, &err_flg, NULL);
						break;
					}

					if (err == NO_ERROR)	// Valid data found.
					{
						// DonR 14Jan2009: If a Pharmacy Ishur was used to grant specialist participation,
						// the prescription_drugs row will need to be flagged to indicate that specialist
						// participation was granted by the pharmacy, even though its participation source
						// is "from pharmacy ishur" rather than "from specialist". If you understand this
						// comment, congratulations - you're a nerd.
						ThisDrug->ret_part_source.table = FROM_PHARMACY_ISHUR;
						ThisDrug->WhySpecialistPtnGiven	= PHARMACY_ISHUR;
					}	// GetParticipatingCode did not return an error.
				}	// Good read of spclty_largo_prcnt.
			}	// Something other than row-not-found reading spclty_largo_prcnt.
				

			CloseCursor (	MAIN_DB, TestMacDoctorDrugsElectronic_SpecialistPtn_3_cur	);

			// If we've found a match, we're finished with this section and we can go check
			// drug_extension stuff.
			if (ThisDrug->PrticSet == MAC_TRUE)
				break;
		}	// Pharmacy Ishur supplied a Speciality Code.


		// THIRD, see if it's possible to grant specialist participation without regard to
		// the prescribing doctor or a pharmacy-specified Speciality Code. This can happen
		// for four different reasons:
		//
		// 1) Prescription is from an old-age home with the appropriate authorization level.
		// 2) Pharmacy has issued a Pharmacy Ishur for specialist participation, but has
		//    not specified a Speciality Code.
		// 3) The current prescribing doctor has checked the box in Clicks to confirm
		//    that s/he has seen a specialist's letter authorizing the drug or a related drug;
		//    this will result in doctor_presc/specialist_ishur > 0.
		// 4) The member has previously bought this drug or a related drug with specialist
		//    participation.
		//
		// If any of these conditions apply, we will scroll through the relevant spclty_largo_prcnt
		// rows to find the best available specialist participation. We test the conditions in
		// order of increasing database burden - starting with simple integer comparisons, and
		// building up to the drug-purchase history scan.
		ReasonForSpecPtn = NO_REASON_APPLICABLE;

		if ((RecipeSource_in == RECIP_SRC_OLD_PEOPLE_HOUSE) && (SpecialtySearchFlag))
			ReasonForSpecPtn = OLD_AGE_HOME;
		else
		if (v_Profession == -1)
			ReasonForSpecPtn = PHARMACY_ISHUR_REQ;
		else
		{
			// Look for applicable spec_letter_seen rows.
			// Question: Should we be excluding dentist and home-visit Speciality Codes
			// from this lookup, or would that be unnecessarily paranoid?
			SpecLetters = 0;

			// DonR 13Dec2016: The spec_letter_seen table is no longer being populated; but we now get a "flag"
			// field in doctor_presc to indicate what should be the same thing. So if nothing relevant was
			// found in spec_letter_seen, search doctor_presc in the same manner. (Note that we may want to
			// tighten up the WHERE clause parameters at some point - for now, the idea is to duplicate the
			// functionality of the old spec_letter_seen lookup.)
			// The WHERE clause includes some "dummy" criteria to conform to the index structure of
			// doctor_presc, and thus (I hope!) give faster results.
			// DonR 31May2018: When reading from spclty_drug_grp, a match by Parent Group qualifies whether
			// Economypri ("Adifuyot") Group is zero or not - so get rid of "group_code = 0" on that line.
			// DonR 03Jun2018: spclty_drug_grp is now built with -999 in place of zero for group_code and
			// parent_group_code; this means that we no longer have to put in special WHERE conditions to
			// accept only positive values, since -999's are guaranteed not to match anything.
			ExecSQL (	MAIN_DB, TestMacDoctorDrugsElectronic_READ_SpecLetters,
						&SpecLetters,
						&v_MemberID,		&v_Largo,			&v_EPGroup,
						&v_ParentGroup,		&v_DocID,			&v_IdType,
						&v_MemberID,		&FirstPrId,			&FirstPrId,
						&v_DocID,			&v_IdType,			&VisitNumber,
						&VisitNumber,		END_OF_ARG_LIST						);

			if ((SQLCODE == 0) && (SpecLetters > 0))
				ReasonForSpecPtn = SPEC_LETTER_SEEN;
		}	// Need to search spec_letter_seen table.


		if (ReasonForSpecPtn == NO_REASON_APPLICABLE)	// I.e. nothing found yet.
		{
			// Set up dates for purchase-history check.
			SystemDate		= GetDate ();
			TwoMonthsDate	= IncrementDate (SystemDate, -61);
			SixMonthsDate	= IncrementDate (SystemDate, -183);

			// DonR 30Oct2006: Per Iris Shaya / Raya Kroll, qualify previous purchases
			// based on Member Price Code of 2 or three, NOT on Participation Source.
			//
			// DonR 06Dec2009: Per Iris Shaya, we're going back to testing by
			// Participation Source, in conjunction with member's having received
			// some form of discount (including fixed-price discounts).
			// Note that this change is being made along with a change in the logic
			// that builds the spclty_drug_grp table - now past sales under specialist
			// terms are included even if the drug sold is not "officially" a
			// specialist drug. (See As400UnixClient changes for 06Dec2009.)
			//
			// DonR 29Jun2010: I'm about to add a new index to prescription_drugs by member_id
			// and largo_code; so I'm moving the largo_code criterion to take advantage of the
			// new index. At least in theory, this should boost performance.
			// DonR 15Feb2011: Performance tuning on WHERE clause criteria.
			// DonR 31May2018: When reading from spclty_drug_grp, a match by Parent Group qualifies whether
			// Economypri ("Adifuyot") Group is zero or not - so get rid of "group_code = 0" on that line.
			// DonR 03Jun2018: spclty_drug_grp is now built with -999 in place of zero for group_code and
			// parent_group_code; this means that we no longer have to put in special WHERE conditions to
			// accept only positive values, since -999's are guaranteed not to match anything.
			ExecSQL (	MAIN_DB, TestMacDoctorDrugsElectronic_READ_PastPurchases,
						&PastPurchases,
						&v_MemberID,		&SixMonthsDate,		&v_Largo,
						&v_EPGroup,			&v_ParentGroup,
						&v_IdType,			&TwoMonthsDate,		END_OF_ARG_LIST	);

			if ((SQLCODE == 0) && (PastPurchases > 0))
				ReasonForSpecPtn = PURCHASE_HISTORY;
		}	// Need to perform purchase-history check.


		// If we found a reason to search for the best available specialist participation, go
		// ahead and perform the search.
		if (ReasonForSpecPtn != NO_REASON_APPLICABLE)
		{
			// Declare cursor for retrieving percent codes from table
			// spclty_largo_prcnt with no join - for use only when the patient's
			// purchase history shows a previous discounted purchase of this drug
			// or a related drug, or when a doctor has filled out a "tofes"
			// certifying that s/he has seen a specialist letter for this drug
			// or a related drug, or there is some other reason to search for
			// the best available specialist participation.
			// DonR 10Apr2011: Declare cursor only when necessary.
			DeclareAndOpenCursorInto (	MAIN_DB, TestMacDoctorDrugsElectronic_SpecialistPtn_2_cur,
										&price_code,			&fixed_price,					&insurance_type,
										&health_basket,			&v_ProfessionFound,

										&v_Largo,				&Member.MemberMaccabi,			&Member.MemberHova,
										&Member.MemberKeva,		&ROW_NOT_DELETED,				&Member.insurance_type,
										&Member.current_vetek,	&Member.prev_insurance_type,	&Member.prev_vetek,
										END_OF_ARG_LIST																	);
			
			if (SQLERR_error_test ())
			{
				err = ERR_DATABASE_ERROR;
				SetErrorVarArr (ErrorCode, err, 0, 0, &err_flg, NULL);
				break;
			}	// Failed to open cursor.

			// Loop through rows selected to find best participation type.
			do
			{
				FetchCursor (	MAIN_DB, TestMacDoctorDrugsElectronic_SpecialistPtn_2_cur	);
				
				if (SQLERR_code_cmp (SQLERR_end_of_fetch) == MAC_TRUE)	// Normal exit from loop.
				{
					break;
				}
				
				if (SQLERR_error_test ())
				{
					err = ERR_DATABASE_ERROR;
					SetErrorVarArr (ErrorCode, err, 0, 0, &err_flg, NULL);
					break;
				}	// Error (other than end-of-fetch) reading cursor.

				// Successfully read a cursor row.
				// See if we've found a better participation type than the default.
				err = SimpleFindLowestPtn (price_code, fixed_price, insurance_type, health_basket, ThisDrug, 1);
				if ((err != NO_ERROR) && (err != ERR_CODE_NOT_FOUND))	// Invalid Participation Code found.
				{
					SetErrorVarArr (ErrorCode, err, 0, 0, &err_flg, NULL);
					break;
				}

				// If pharmacy sent a specialist-participation ishur without a speciality code,
				// store the retrieved speciality code so the ishur can be updated accordingly.
				if (err == NO_ERROR)	// Valid, better participation found.
				{
					if (*PharmIshurProfessionInOut == -1)
						*PharmIshurProfessionInOut = v_ProfessionFound;
				}	// GetParticipatingCode found better participation.

				// DonR 23Nov2011: Since we're now sorting spclty_largo_prcnt rows by increasing participation
				// percentage (with 100%/fixed price treated as 0.01%), we're guaranteed that the first match
				// is the best match. Therefore, once we've found a match, break out of the loop.
			}	// Cursor-reading loop for mac_doc_elpr_2.
			while (0);	// DonR 23Nov2011 - was "while (1)", but now we want to execute the "loop" only once.


			CloseCursor (	MAIN_DB, TestMacDoctorDrugsElectronic_SpecialistPtn_2_cur	);
			SQLERR_error_test ();

			// If we've hit a severe error, exit the routine now.
			if (err_flg == 1)
			{
				// DonR 14Jul2005: Restore default isolation.
				RESTORE_ISOLATION;
				return (1);
			}


			// If we've completed the scan of spclty_largo_prcnt and found a specialist
			// participation better than the default, set up the drug-purchase structure
			// values according - based on the reason we performed the scan in the first
			// place.
			if (ThisDrug->PrticSet == MAC_TRUE)
			{
				switch (ReasonForSpecPtn)
				{
					case OLD_AGE_HOME:
										ThisDrug->ret_part_source.table = FROM_DOC_SPECIALIST;
										ThisDrug->WhySpecialistPtnGiven	= OLD_PEOPLES_HOME;
										break;

					case PHARMACY_ISHUR_REQ:
										ThisDrug->ret_part_source.table = FROM_PHARMACY_ISHUR;
										ThisDrug->WhySpecialistPtnGiven	= PHARMACY_ISHUR;
										break;

					case SPEC_LETTER_SEEN:
										ThisDrug->ret_part_source.table = FROM_DOC_SPECIALIST;
										ThisDrug->WhySpecialistPtnGiven	= DOCTOR_SAW_SPEC_LETTER;
										break;

					case PURCHASE_HISTORY:
										ThisDrug->ret_part_source.table = FROM_DOC_SPECIALIST;
										ThisDrug->WhySpecialistPtnGiven	= PURCHASE_HISTORY;
										break;

					default:
										break;	// (This should never happen!)
				}
			}	// We found the best available specialist participation for this drug.
		}	// There is some reason to perform an all-specialities scan of spclty_largo_prcnt.
	}
	while (0);	// End of first dummy loop (spclty_largo_prcnt checking).



	// Test drug extensions (a.k.a. "nohalim ragilim").
	do	// Yet another dummy loop.
	{
		// If we've already hit a fatal error for this drug, OR there are no drug_extension
		// rules for this drug, skip drug_extension.
		// DonR 19Dec2012: We no longer want to skip this section if we've already found something
		// in spclty_largo_prcnt - it's possible that a drug_extension rule will give better
		// participation.
		if ((ThisDrug->DFatalErr		== MAC_TRUE)					||
			(RecipeSource_in			== RECIP_SRC_EMERGENCY_NO_RX)	||
			(ThisDrug->DL.rule_status	== 0))
		{
			break;
		}

		// GET DRUG EXTENSION DATA
		err = FIND_DRUG_EXTENSION (	ThisDrug->DrugCode,
									memberAge,
									memberGender,
									phrm_perm,
									(RecipeSource_in == RECIP_SRC_NO_PRESC) ? NO_PRESC_SRC : PRESC_SRC,
									TREATMENT_TYPE_GENERAL,
									ThisDrug->DoctorRuleNumber,
//									ThisDrug->WasPossibleDrugShortage,	DonR 18May2025 Reverting to previous version - but leaving the code here just in case.
									&ext	);

		if (err != NO_ERROR)
		{
			if (err == ERR_KEY_NOT_FOUND)
				break;

			if (SetErrorVarArr (ErrorCode, err, 0, 0, NULL, NULL))
			{
				// DonR 14Jul2005: Restore default isolation.
				RESTORE_ISOLATION;

				return (1);
			}
		}

		// Check if the Drug Extension row gives better participation terms than the default.
		// DonR 01Jan2010: GetParticipatingCode() handles fixed-price substitution for
		// drug_extension rules as well as for other tables now - so set the "fixed-price
		// flag" equal to 1.
		err = SimpleFindLowestPtn (ext.price_code, ext.fixed_price, ext.insurance_type, ext.prc.in_health_basket, ThisDrug, 1);

		if ((err != NO_ERROR) && (err != ERR_CODE_NOT_FOUND))
		{
			if (SetErrorVarArr (ErrorCode, err, 0, 0, NULL, NULL))
			{
				// DonR 14Jul2005: Restore default isolation.
				RESTORE_ISOLATION;

				return (1);
			}
		}

		if (err == NO_ERROR)
		{
			// DonR 24Apr2025 User Story #390071: If the rule we found has confirm_authority > 0,
			// that means it's a rule that was selected by the doctor.
			// DonR 13May2025 User Story #388766: We can now also retrieve special drug-shortage
			// rules, with a special confirm_authority value (sysparams/DrugShortageRuleConfAuthority).
			// Accordingly, I'm changing this assignment to a switch statement, just because it's
			// easier to read than a double-tertiary or multi-part "if".
			// DonR 18May2025: Special drug-shortage rules will no longer be retrieved by
			// FIND_DRUG_EXTENSION() because they apply only when the member has an AS/400 ishur
			// with tikra. However, I'm leaving this logic in place just in case someone changes
			// their mind. DrugShortageRuleConfAuthority = 17, and doctors (like pharmacies) use
			// "gorem me'asher" up to 10, so there shouldn't be any overlap.
			switch (ext.confirm_authority)
			{
				case 0:		// Automatic "nohalim".
							ThisDrug->ret_part_source.table = FROM_DRUG_EXTENSION;
							break;

				default:	// Either a special drug_shortage nohal or a doctor-chosen nohal.
							ThisDrug->ret_part_source.table =
								(ext.confirm_authority == DrugShortageRuleConfAuthority) ?
									DRUG_SHORTAGE_RULE : FROM_DOCTOR_NOHAL;
			}
//			ThisDrug->ret_part_source.table = (ext.confirm_authority > 0) ? FROM_DOCTOR_NOHAL : FROM_DRUG_EXTENSION;

			ThisDrug->rule_number			= ext.rule_number;
			ThisDrug->rule_needs_29g		= ext.needs_29_g;	// DonR 01Sep2015.
		}

		// DonR 01Jan2010: GetParticipatingCode() now assigns fixed-price participation
		// for drug_extension rules (as it should always have done!), so the code that
		// did the same thing here has been removed.


		// Test if quantity test is needed
		if ((ext.duration == 0) || ((ext.op + ext.units) == 0))
		{
			continue;
		}

		// Test drug extension quantity limits
		err = SumMemberDrugsUsed (ThisDrug,
								  MemberId_in,
								  IdType_in,
								  ext.duration,
								  &SumUnits);

		if (err != NO_ERROR)
		{
			if (SetErrorVarArr (ErrorCode, ERR_DATABASE_ERROR, 0, 0, NULL, NULL))
			{
				// DonR 14Jul2005: Restore default isolation.
				RESTORE_ISOLATION;

				return (err);
			}
		}

		// DonR 12Aug2003: Avoid division-by-zero problems by ensuring that package
		// size has some real value. (I don't know if there's any real chance of
		// problems, but paranoia is sometimes a good thing.)
		if (ThisDrug->DL.package_size < 1)
		{
			ThisDrug->DL.package_size = 1;
		}

		ExtUnits = ext.op * ThisDrug->DL.package_size + ext.units;

		if( SumUnits >= ExtUnits )
		{
			SetErrorVarArr (&ThisDrug->DrugAnswerCode, ERR_QTY_REDUCE_PRC_QTY_OVER_LIM, ThisDrug->DrugCode, LineNo_in, &ThisDrug->DFatalErr, NULL);
			break;
		}

		ExtUnits -= SumUnits;	// Remaining units that can be bought based on the rule.
		SumUnits =  ThisDrug->Op * ThisDrug->DL.package_size + ThisDrug->Units;

		if (SumUnits > ExtUnits)
		{
			SumUnits = ExtUnits;
			SetErrorVarArr (&ThisDrug->DrugAnswerCode, ERR_QTY_REDUCE_PRC_QTY_LIM_EXT, ThisDrug->DrugCode, LineNo_in, &ThisDrug->DFatalErr, NULL);
			break;
		}	// Units to be purchased would exceed amount available according to rule.

	}
	while (0);	// end second dummy loop.

	// DonR 14Jul2005: Restore default isolation.
	RESTORE_ISOLATION;

	return (0);
}
// End of test_mac_doctor_drugs_electronic()



/*=======================================================================
||																		||
||			   predict_member_participation()							||
||																		||
|| Predict member participation for prescription-request transactions.	||
||																		||
|| RETURNs:																||
|| 0 = OK.																||
|| 1 = high severity error occured.										||
|| 2 = Deadlock.														||
 =======================================================================*/
TErrorCode predict_member_participation (	TMemberData		*Member_in,
											TDrugListRow	*DL_in,
											PHARMACY_INFO	*PhrmInfo_in,
											short			DocIdType_in,
											int				DocId_in,
											int				VisitDate_in,
											int				DoctorRuleNumber_in,	// DonR 28Apr2025 User Story #390071.
											short			*PriceCode_out,
											short			*PricePercent_out,
											short			*FixedPriceFlag_out,
											int				*FixedPrice_out,
											short			*PermissionType_out,
											short			*InsuranceUsed_out,
											short			*HealthBasket_out,
											short			*ErrorCode_out			)
{
	// Local declarations
	TErrorCode				err					= NO_ERROR;
	DrugExtension			ext;
	int						ReasonForSpecPtn;
	short					NewPriceCode		= 0;
	short					NewPricePercent		= 0;
	int						NewFixedPrice		= 0;
	short					NewFixedPriceExists	= 0;
	short					NewInsuranceUsed	= 0;
	short					NewReasonCode		= 0;
	short					NewTable			= 0;
	ISOLATION_VAR;

	TMemberData					*M				= Member_in;
	TDrugListRow				*DL				= DL_in;
	TTypeDoctorIdentification	v_DocIdType		= DocIdType_in;
	TDoctorIdentification		v_DocID			= DocId_in;
	int							VisitDate		= VisitDate_in;


	TTreatmentType				TreatmentType	= TREATMENT_TYPE_GENERAL;
	int							SystemDate;
	int							TwoMonthsDate;
	int							SixMonthsDate;
	int							PastPurchases	= 0;
	int							SpecLetters		= 0;
	short						price_code;
	int							fixed_price;
	char						insurance_type[2];
	short						HealthBasket;
	short						HealthBasketNew;
	int							IgnoreProfessionFound;


	// Avoid unnecessary record-locked errors.
	REMEMBER_ISOLATION;
	SET_ISOLATION_DIRTY;

	// Assume error-out argument needs initialization.
	if (ErrorCode_out != NULL)
		*ErrorCode_out = NO_ERROR;


	// 1) Start with default participation from drug_list.
	NewTable			= FROM_DRUG_LIST_TBL;
	NewInsuranceUsed	= BASIC_INS_USED;
	NewPriceCode		= DL->member_price_code;
	HealthBasketNew		= DL->health_basket_new;
// GerrLogMini (GerrId, "Default Health Basket value = %d.", HealthBasketNew);
// GerrLogMini (GerrId, "\npredict_member_participation: Member %d, Largo %d, specialist %d, rule_status %d.", M->ID, DL->largo_code, DL->specialist_drug, DL->rule_status);
// GerrLogMini (GerrId, "predict_member_participation: specialist_drug = %d, rule_status = %d", DL->specialist_drug, DL->rule_status);

	// 2) Test for specialist participation.
	do	// Dummy loop to avoid nasty GOTO's.
	{
		// Don't try to grant specialist participation if this is not a specialist drug.
		if (DL->specialist_drug == 0)
			break;	// Jump down to drug_extension test.

		// FIRST, if the drug was prescribed by a Maccabi doctor, try for a match based
		// on the prescribing doctor's speciality(s) and the Speciality Largo Percent table.
		if ((v_DocID > 0) && (v_DocIdType != 1))	// Skip this section if the prescribing physician is non-Maccabi.
		{
			// Declare cursor for retrieving percent codes from tables doctor_speciality,
			// spclty_largo_prcnt joined on speciality_code.
			// DonR 01Jan2010: Fixed "ambiguous column" error for speciality_code in WHERE clause.
			// DonR 15Feb2011: Re-ordered WHERE clause for better performance.
			// DonR 10Apr2011: Declare cursor only when necessary.
			// DonR 23Dec2019: Since this cursor is identical to the one used in test_mac_doctor_drugs_electronic(),
			// there's no point in creating a different one - just use a dummy variable to hold the Health Basket
			// value, which isn't used by this routine.
			DeclareAndOpenCursorInto (	MAIN_DB, TestMacDoctorDrugsElectronic_SpecialistPtn_1_cur,
										&price_code,		&fixed_price,				&insurance_type,	&HealthBasket,
										&v_DocID,			&ROW_NOT_DELETED,			&DL->largo_code,	&M->MemberMaccabi,
										&M->MemberHova,		&M->MemberKeva,				&ROW_NOT_DELETED,	&M->insurance_type,
										&M->current_vetek,	&M->prev_insurance_type,	&M->prev_vetek,		END_OF_ARG_LIST			);

			if (SQLERR_error_test ())
			{
				err = ERR_DATABASE_ERROR;
				break;
			}	// Failed to open cursor.

			// Read first row selected to find the best participation type. (Note that if the first row
			// gives us an error in FindLowestPtnForPrescRequest() - which is unlikely - we will proceed
			// with the next row.)
			do
			{
				FetchCursor (	MAIN_DB, TestMacDoctorDrugsElectronic_SpecialistPtn_1_cur	);
				
				if (SQLERR_code_cmp (SQLERR_end_of_fetch) == MAC_TRUE)	// Normal exit from loop.
				{
					break;
				}

				// DonR 01Jul2020: If we get a "conflict test" error, don't log it, at least for now.
				if (SQLCODE != 0)
				{
					if (SQLERR_code_cmp (SQLERR_access_conflict) == MAC_FALSE)
					{
						SQLERR_error_test ();
					}

					err = ERR_DATABASE_ERROR;
					break;
				}	// Error (other than end-of-fetch) reading cursor.


				// Successfully read a cursor row.
				// See if we've found a better participation code than the default.
				err = FindLowestPtnForPrescRequest	(price_code,		fixed_price,		NewPriceCode,		NewFixedPrice,			insurance_type,
													 &NewPriceCode,		&NewPricePercent,	&NewFixedPrice,		&NewFixedPriceExists,	&NewInsuranceUsed);

				// DonR 23Nov2011: Since we're now sorting spclty_largo_prcnt rows by increasing participation
				// percentage (with 100%/fixed price treated as 0.01%), we're guaranteed that the first match
				// is the best match. Therefore, once we've found a match, break out of the loop.
				if (err == NO_ERROR)
				{
					NewTable		= FROM_DOC_SPECIALIST;
					NewReasonCode	= PRESCRIBED_BY_SPECIALIST;
					HealthBasketNew	= HealthBasket;
// GerrLogMini (GerrId, "Found specialist ptn %d FP %d, Health Basket %d.", NewPriceCode, NewFixedPrice, HealthBasketNew);
					break;	// We've found something, so there's no reason to continue reading from the cursor.
				}	// GetParticipatingCode did not return an error.

			}	// Cursor-reading loop.
			while (1);

			CloseCursor (	MAIN_DB, TestMacDoctorDrugsElectronic_SpecialistPtn_1_cur	);


			// If we've found a match, we're finished with this section and we can go check
			// drug_extension stuff.
			if (NewTable == FROM_DOC_SPECIALIST)
				break;
		}	// Prescribing doctor is a  Maccabi physician.


		// SECOND, see if it's possible to grant specialist participation without regard to
		// the prescribing doctor. This can happen for two different reasons:
		//
		// 1) A doctor (not necessarily the prescribing physician) has sent an ishur confirming
		//    that s/he has seen a specialist's letter authorizing the drug or a related drug;
		//    this ishur is stored in the spec_letter_seen table.
		// 2) The member has previously bought this drug or a related drug with specialist
		//    participation.
		//
		// (Note that logic relating to old-age homes and pharmacy ishurim doesn't apply to
		// prescription-request transactions - so only two of the four reasons relevant to
		// test_mac_doctor_drugs_electronic() are relevant here.)
		//
		// If either of these conditions apply, we will scroll through the relevant spclty_largo_prcnt
		// rows to find the best available specialist participation. We test the conditions in
		// order of increasing database burden - starting with simple integer comparisons, and
		// building up to the drug-purchase history scan.
		ReasonForSpecPtn = NO_REASON_APPLICABLE;

		// Look for applicable spec_letter_seen rows.
		// Question: Should we be excluding dentist and home-visit Speciality Codes
		// from this lookup, or would that be unnecessarily paranoid?
		// DonR 15Feb2011: Re-ordered WHERE clause for better performance.
		// DonR 31May2018: When reading from spclty_drug_grp, a match by Parent Group qualifies whether
		// Economypri ("Adifuyot") Group is zero or not - so get rid of "group_code = 0" on that line.
		// DonR 03Jun2018: spclty_drug_grp is now built with -999 in place of zero for group_code and
		// parent_group_code; this means that we no longer have to put in special WHERE conditions to
		// accept only positive values, since -999's are guaranteed not to match anything.
		SpecLetters		= 0;

		ExecSQL (	MAIN_DB, predict_member_participation__READ_SpecLetters,
					&SpecLetters,
					&M->ID,					&DL->largo_code,	&DL->economypri_group,
					&DL->parent_group_code,	&v_DocID,			&M->ID_code,
					&VisitDate,				END_OF_ARG_LIST								);

		if ((SQLCODE == 0) && (SpecLetters > 0))
			ReasonForSpecPtn = SPEC_LETTER_SEEN;


		if (ReasonForSpecPtn == NO_REASON_APPLICABLE)	// I.e. nothing found yet.
		{
			// Set up dates for purchase-history check.
			SystemDate		= GetDate ();
			TwoMonthsDate	= IncrementDate (SystemDate, -61);
			SixMonthsDate	= IncrementDate (SystemDate, -183);

			// DonR 30Oct2006: Per Iris Shaya / Raya Kroll, qualify previous purchases
			// based on Member Price Code of 2 or three, NOT on Participation Source.
			//
			// DonR 06Dec2009: Per Iris Shaya, we're going back to testing by
			// Participation Source, in conjunction with member's having received
			// some form of discount (including fixed-price discounts).
			// Note that this change is being made along with a change in the logic
			// that builds the spclty_drug_grp table - now past sales under specialist
			// terms are included even if the drug sold is not "officially" a
			// specialist drug. (See As400UnixClient changes for 06Dec2009.)
			//
			// DonR 29Jun2010: I'm about to add a new index to prescription_drugs by member_id
			// and largo_code; so I'm moving the largo_code criterion to take advantage of the
			// new index. At least in theory, this should boost performance.
			// DonR 15Feb2011: Performance tuning on WHERE clause criteria.
			// DonR 31May2018: When reading from spclty_drug_grp, a match by Parent Group qualifies whether
			// Economypri ("Adifuyot") Group is zero or not - so get rid of "group_code = 0" on that line.
			// DonR 03Jun2018: spclty_drug_grp is now built with -999 in place of zero for group_code and
			// parent_group_code; this means that we no longer have to put in special WHERE conditions to
			// accept only positive values, since -999's are guaranteed not to match anything.
			// DonR 23Dec2019: Since this SQL is identical to the version used in
			// test_mac_doctor_drugs_electronic(), there's no point in creating a different one.
			ExecSQL (	MAIN_DB, TestMacDoctorDrugsElectronic_READ_PastPurchases,
						&PastPurchases,
						&M->ID,					&SixMonthsDate,				&DL->largo_code,
						&DL->economypri_group,	&DL->parent_group_code,
						&M->ID_code,			&TwoMonthsDate,				END_OF_ARG_LIST		);

			if ((SQLCODE == 0) && (PastPurchases > 0))
				ReasonForSpecPtn = PURCHASE_HISTORY;
		}	// Need to perform purchase-history check.


		// If we found a reason to search for the best available specialist participation, go
		// ahead and perform the search.
		if (ReasonForSpecPtn != NO_REASON_APPLICABLE)
		{
			// Declare cursor for retrieving percent codes from table
			// spclty_largo_prcnt with no join - for use only when the patient's
			// purchase history shows a previous discounted purchase of this drug
			// or a related drug, or when a doctor has filled out a "tofes"
			// certifying that s/he has seen a specialist letter for this drug
			// or a related drug, or there is some other reason to search for
			// the best available specialist participation.
			// DonR 10Apr2011: Declare cursor only when necessary.
			// DonR 23Dec2019: Since this cursor is identical to the one used in test_mac_doctor_drugs_electronic(),
			// there's no point in creating a different one - just use dummy variables to hold the Health Basket
			// and Profession Found values, which aren't used by this routine.
			DeclareAndOpenCursorInto (	MAIN_DB, TestMacDoctorDrugsElectronic_SpecialistPtn_2_cur,
										&price_code,			&fixed_price,				&insurance_type,
										&HealthBasket,			&IgnoreProfessionFound,
										&DL->largo_code,		&M->MemberMaccabi,			&M->MemberHova,
										&M->MemberKeva,			&ROW_NOT_DELETED,			&M->insurance_type,
										&M->current_vetek,		&M->prev_insurance_type,	&M->prev_vetek,
										END_OF_ARG_LIST																);
			
			if (SQLERR_error_test ())
			{
				err = ERR_DATABASE_ERROR;
				break;
			}	// Failed to open cursor.

			// Loop through rows selected to find best participation type.
			do
			{
				FetchCursor (	MAIN_DB, TestMacDoctorDrugsElectronic_SpecialistPtn_2_cur	);
				
				if (SQLERR_code_cmp (SQLERR_end_of_fetch) == MAC_TRUE)	// Normal "nothing found" exit from loop.
				{
					break;
				}
				
				if (SQLERR_error_test ())
				{
					err = ERR_DATABASE_ERROR;
					break;
				}	// Error (other than end-of-fetch) reading cursor.

				// Successfully read a cursor row.
				// See if we've found a better participation type than the default.
				err = FindLowestPtnForPrescRequest	(price_code,		fixed_price,		NewPriceCode,		NewFixedPrice,			insurance_type,
													 &NewPriceCode,		&NewPricePercent,	&NewFixedPrice,		&NewFixedPriceExists,	&NewInsuranceUsed);

				if (err == NO_ERROR)
				{
					NewTable		= FROM_DOC_SPECIALIST;
					HealthBasketNew	= HealthBasket;

					switch (ReasonForSpecPtn)
					{
						case SPEC_LETTER_SEEN:
											NewReasonCode	= DOCTOR_SAW_SPEC_LETTER;
											break;

						case PURCHASE_HISTORY:
											NewReasonCode	= PURCHASE_HISTORY;
											break;

						default:
											continue;	// (Jump to next item in cursor - this should never happen!)
					}

					break;	// Success - jump out of loop.
				}

				// DonR 23Nov2011: Since we're now sorting spclty_largo_prcnt rows by increasing participation
				// percentage (with 100%/fixed price treated as 0.01%), we're guaranteed that the first match
				// is the best match. Therefore, we will continue looping only in the unlikely event that
				// FindLowestPtnForPrescRequest() returned an error.

			}	// Cursor-reading loop for predict_ptn_2.
			while (1);


			CloseCursor (	MAIN_DB, TestMacDoctorDrugsElectronic_SpecialistPtn_2_cur	);
		}	// There is some reason to perform an all-specialities scan of spclty_largo_prcnt.
	}
	while (0);	// End of first dummy loop (spclty_largo_prcnt checking).


	// 3. Test drug extensions. If we've already hit a severe database error or if this drug doesn't
	// have any "nohalim", don't bother.
	if ((err != ERR_DATABASE_ERROR) && (DL->rule_status != 0))
	{
		do	// Yet another dummy loop.
		{
			// GET DRUG EXTENSION DATA
//GerrLogMini (GerrId, "Predict Ptn: Largo %d, Rule Status %d, Member Age %d months, Member Sex %d, Pharm Perm %d, Default ptn %d/%d.",
//DL->largo_code, DL->rule_status, Member_in->age_months, Member_in->sex, PhrmInfo_in->pharmacy_permission, NewPriceCode, NewFixedPrice);
			err = FIND_DRUG_EXTENSION (	DL->largo_code,
										Member_in->age_months,
										Member_in->sex,
										PhrmInfo_in->pharmacy_permission,
										PRESC_SRC,
										TREATMENT_TYPE_GENERAL,
										DoctorRuleNumber_in,	// DonR 28Apr2025 User Story #390071.
//										false,					DonR 18May2025 Reverting to previous version - but leaving the shortage-rule code here just in case.
										&ext	);
// GerrLogMini (GerrId, "FIND_DRUG_EXTENSION returned %d, ext.price_code %d.", err, ext.price_code);

			// Check if the Drug Extension row gives better participation terms than the default.
			// DonR 01Jan2010: GetParticipatingCode() handles fixed-price substitution for
			// drug_extension rules as well as for other tables now - so set the "fixed-price
			// flag" equal to 1.
			if (err == NO_ERROR)
			{
//GerrLogMini (GerrId, "Calling FindLowestPtnForPrescRequest: Default %d/%d, ext %d/%d, insurance %s.", NewPriceCode, NewFixedPrice, ext.price_code, ext.fixed_price, ext.insurance_type);
				err = FindLowestPtnForPrescRequest	(ext.price_code,	ext.fixed_price,		NewPriceCode,
													 NewFixedPrice,		ext.insurance_type,		&NewPriceCode,		&NewPricePercent,
													 &NewFixedPrice,	&NewFixedPriceExists,	&NewInsuranceUsed);
//GerrLogMini (GerrId, "Predict Ptn: FindLowestPtnForPrescRequest returns err %d, ptn %d/%d.", err, NewPriceCode, NewFixedPrice);

				if (err == NO_ERROR)
				{
					// DonR 28Apr2025 User Story #390071 functionality note: In Transaction 6003, we
					// differentiate between "ordinary" drug-extension discounts and discounts due to
					// a "nohal" chosen by the doctor; a different Participation Source value is sent
					// to AS/400. In 6001/6101, however, we don't tell the pharmacy which table gave
					// us our participation, so we can just set NewTable to FROM_DRUG_EXTENSION and
					// not worry too much.
					NewTable		= FROM_DRUG_EXTENSION;
					HealthBasketNew	= ext.prc.in_health_basket;
// GerrLogMini (GerrId, "Got Health Basket %d from drug_extension.", HealthBasketNew);
				}
			}
		}
		while (0);	// end second dummy loop.
	}	// No previous severe database error.


	// WORKINGPOINT: CHECK FOR POSSIBLE DISCOUNTS AND ASSIGN "DISCOUNT AVAILABLE" MESSAGE CODE.


	// WORKINGPOINT: SET MACCABI PRICE IF NO OTHER DISCOUNTS AVAILABLE.


	// DonR 14Jul2005: Restore default isolation.
	RESTORE_ISOLATION;


	// Set up output variables.
	if (PriceCode_out		!= NULL)	*PriceCode_out		= NewPriceCode;
	if (PricePercent_out	!= NULL)	*PricePercent_out	= NewPricePercent;
	if (FixedPriceFlag_out	!= NULL)	*FixedPriceFlag_out	= NewFixedPriceExists;
	if (FixedPrice_out		!= NULL)	*FixedPrice_out		= NewFixedPrice;
	if (InsuranceUsed_out	!= NULL)	*InsuranceUsed_out	= NewInsuranceUsed;
	if (HealthBasket_out	!= NULL)	*HealthBasket_out	= HealthBasketNew;
	if (ErrorCode_out		!= NULL)	*ErrorCode_out		= err;
	if (PermissionType_out	!= NULL)	*PermissionType_out	= (NewTable == FROM_DRUG_EXTENSION) ? ext.permission_type : 0;
// GerrLogMini (GerrId, "Returning price code %d, FP exists %d, err %d.", NewPriceCode, NewFixedPriceExists, err);
	// If we've hit a severe database error, let the calling function know; anything else can be ignored.
	return ((err == ERR_DATABASE_ERROR) ? err : 0);
}
// End of predict_member_participation()



/*=======================================================================
||																		||
||			   test_no_presc_drugs ()									||
||																		||
|| Test Participating precent for non-prescription drugs.				||
|| RETURNs:																||
|| 0 = OK.																||
|| 1 = high-severity error occurred.									||
|| 2 = Deadlock.														||
 =======================================================================*/
TErrorCode test_no_presc_drugs (TPresDrugs					*presdrugs,
								TNumOfDrugLinesRecs			drugcnt,
								TMemberParticipatingType	presParti,
								short int					memberAge,
								TMemberSex					memberGender,
								short int					phrm_perm,
								TMemberIdentification		MemberId,
								TIdentificationCode			IdType,
								TErrorCode					*ErrorCode)
{
/*=======================================================================
||			    Local declarations				||
 =======================================================================*/
    int				i;
    TErrorCode			err;
    TParticipatingPrecent	test_part_prcn;
    TMemberParticipatingType	ParticipatingCode = 1;	// No point in looking it up!
    TParticTst			prt;
    DrugExtension		ext;
    int				SumUnits,
				ExtUnits;
/*=======================================================================
||	      	Set participating percent to 100 percent.		||
 =======================================================================*/
    for( i=0 ; i < drugcnt ; i++ )
    {
		presdrugs[i].RetPartCode	    = ParticipatingCode;

		presdrugs[i].ret_part_source.table  = FROM_NO_TABLE;
    }

/*=======================================================================
||			Test drug extension data.			||
 =======================================================================*/
	// DonR 21Nov2011: Make sure FIND_DRUG_EXTENSION doesn't see "Tzahal" rules, since
	// this function is used only by the old Transaction 1063.
	Member.MemberMaccabi	= 1;
	Member.MemberTzahal	= Member.MemberHova = Member.MemberKeva = 0;

    for( i=0; i < drugcnt; i++ )
    {
		if (presdrugs[i].DFatalErr == MAC_TRUE)
		{
			continue;
		}

		err = FIND_DRUG_EXTENSION (	presdrugs[i].DrugCode,
									memberAge,
									memberGender,
									phrm_perm,
									NO_PRESC_SRC,
									TREATMENT_TYPE_GENERAL,
									0,		// Doctor Rule Number - not relevant for OTC sales.
//									false,	DonR 18May2025 Reverting to previous version - but leaving the shortage-rule code here just in case.
									&ext	);

		if( err != NO_ERROR )
		{
			//
			// Stay with 100% on no extension
			//
			if( err == ERR_KEY_NOT_FOUND ) continue;

			if (SetErrorVarArr (ErrorCode, err, 0, 0, NULL, NULL))
			{
			return( 1 );
			}
		}

		// Stay with 100% on no_presc flag down in extensions
		if( ! ext.no_presc_sale_flag )
		{
			continue;
		}

		if( ext.prc.basic_part == 0 )
		{
			continue;
		}

		err = GET_PARTICIPATING_PERCENT( ext.prc.basic_part, &test_part_prcn);
		if( err != NO_ERROR )
		{
			if (SetErrorVarArr (ErrorCode, err, 0, 0, NULL, NULL))
			{
			return( 1 );
			}
		}

		// Get correct Participating code
		// DonR 01Jan2010: GetParticipatingCode() handles fixed-price substitution for
		// drug_extension rules as well as for other tables now - so set the "fixed-price
		// flag" equal to 1.
		err = GetParticipatingCode (presParti, &ext.prc, &presdrugs[i], 1);

		if( err != ERR_CODE_NOT_FOUND )
		{
			presdrugs[i].ret_part_source.table	= FROM_DRUG_EXTENSION;
			presdrugs[i].rule_number			= ext.rule_number;
			presdrugs[i].rule_needs_29g			= ext.needs_29_g;	// DonR 01Sep2015.
		}

		/*
		 * Test if quantity test is needed
		 * -------------------------------
		 */
		if( ext.duration       == 0   ||
			ext.op + ext.units == 0   )
		{
			continue;
		}

		/*
		 * Test drug extension quantity limits
		 * ----------------------------------
		 */
		err = SumMemberDrugsUsed( &presdrugs[i], 
					  MemberId,
					  IdType,
					  ext.duration,
					  &SumUnits );

		if( err != NO_ERROR )
		{
			SetErrorVarArr (ErrorCode, ERR_DATABASE_ERROR, 0, 0, NULL, NULL);
			return( err );
		}

		// DonR 12Aug2003: Avoid division-by-zero problems by ensuring that package
		// size has some real value. (I don't know if there's any real chance of
		// problems, but paranoia is sometimes a good thing.)
		if (presdrugs[i].DL.package_size < 1)
		{
			presdrugs[i].DL.package_size = 1;
		}

		ExtUnits = ext.op * presdrugs[i].DL.package_size + ext.units;

		if (SumUnits >= ExtUnits)
		{
			SetErrorVarArr (&presdrugs[i].DrugAnswerCode, ERR_QTY_REDUCE_PRC_QTY_OVER_LIM, presdrugs[i].DrugCode, i + 1, &presdrugs[i].DFatalErr, NULL);
			continue;
		}

		ExtUnits -= SumUnits;	// Remaining units that can be bought based on the rule.

		SumUnits = presdrugs[i].Op * presdrugs[i].DL.package_size + presdrugs[i].Units;

		if (SumUnits > ExtUnits)
		{
			SumUnits = ExtUnits;
			SetErrorVarArr (&presdrugs[i].DrugAnswerCode, ERR_QTY_REDUCE_PRC_QTY_LIM_EXT, presdrugs[i].DrugCode, i + 1, &presdrugs[i].DFatalErr, NULL);
			continue;	// Skip fixed price computation - at least for now.
		}

		// DonR 01Jan2010: GetParticipatingCode() now assigns fixed-price participation
		// for drug_extension rules (as it should always have done!), so the code that
		// did the same thing here has been removed.

    }
    return( 0 );
}
/* End of test_no_presc_drugs() */



// DonR 01Jan2010: To enable handling of fixed-price participation from
// drug_extension within GetParticipatingCode(), "normalized" the structure
// loading for FIND_DRUG_EXTENSION() to load relevant data into the
// participation-test substructure "prc".

/*=======================================================================
||																		||
||			FIND_DRUG_EXTENSION    ()  20020423 Yulia					||
||																		||
||		Get drug extension from DB table								||
||																		||
 =======================================================================*/
TErrorCode FIND_DRUG_EXTENSION (TDrugCode		LargoCode_in,
								TAge			member_age,
								TMemberSex		member_gender,
								TPermissionType	permission_type_in,
								TFlg			no_presc_flg,
								TTreatmentType	treatment_type,
								int				DoctorRuleNumber,			// DonR 24Apr2025 User Story #390071.
//								bool			WasPossibleDrugShortage,	DonR 18May2025 Reverting to previous version - but leaving the code here just in case.
								DrugExtension	*drug_ext_info)
{
	TErrorCode		ErrRtn;
	ISOLATION_VAR;

	DRUG_EXTENSION_ROW	DE;
	short				v_sort_sequence;
	TDrugCode			largo_code		= LargoCode_in;


	REMEMBER_ISOLATION;

	DE.largo_code					= largo_code;
	DE.from_age						= member_age;
	DE.to_age						= member_age;
	DE.member_gender				= member_gender;
	DE.info.permission_type			= permission_type_in;
	DE.treatment_type				= treatment_type;
	DE.info.no_presc_sale_flag		= no_presc_flg;
	DE.info.op						= -1;
	DE.info.prc.in_health_basket	= 0;
	DE.info.confirm_authority		= 0;
//GerrLogMini (GerrId, "\nFind DE: Member age %d Presc %d Ins Type %s Maccabi Member %d Hova %d Keva %d.\n",
//member_age, no_presc_flg, Member.insurance_type, Member.MemberMaccabi, Member.MemberHova, Member.MemberKeva);

	// DonR 13Jul2005: Get rid of unnecessary table-locked errors.
	SET_ISOLATION_DIRTY;

	ExecSQL (	MAIN_DB, FIND_DRUG_EXTENSION_row,
				&DE.largo_code,					&DE.from_age,					&DE.to_age,
				&DE.member_gender,				&DE.info.permission_type,		&DE.treatment_type,
				&DE.info.op,					&DE.info.units,					&DE.info.duration,
				&DE.info.no_presc_sale_flag,	&DE.info.rule_number,			&DE.info.price_code,
				&DE.info.fixed_price,			&DE.info.insurance_type,		&DE.info.needs_29_g,

				&DE.info.prc.basic_part,		&DE.info.prc.kesef_part,		&DE.info.prc.zahav_part,
				&DE.info.prc.basic_fixed_price,	&DE.info.prc.kesef_fixed_price,	&DE.info.prc.zahav_fixed_price,
				&DE.info.prc.in_health_basket,	&v_sort_sequence,				&DE.info.confirm_authority,

				&DE.largo_code,					&DE.from_age,					&DE.to_age,
				&DE.member_gender,				&DE.info.permission_type,		&DE.info.permission_type,
				&DE.info.no_presc_sale_flag,	&DE.treatment_type,
				&DoctorRuleNumber,				&MaxDoctorRuleConfirmAuthority,	// DonR 24Apr2025 User Story #390071.
//				&WasPossibleDrugShortage,		&DrugShortageRuleConfAuthority,	DonR 18May2025 Reverting to previous version - but leaving the code here just in case.
				&Member.MemberMaccabi,			&Member.MemberHova,				&Member.MemberKeva,
				&Member.insurance_type,			&Member.current_vetek,
				&Member.prev_insurance_type,	&Member.prev_vetek,				END_OF_ARG_LIST						);

	if ((SQLERR_code_cmp (SQLERR_end_of_fetch)	== MAC_TRUE)	||
		(SQLERR_code_cmp (SQLERR_not_found)		== MAC_TRUE))	// Row not found.
	{
		ErrRtn = ERR_KEY_NOT_FOUND;
	}
	else
	{	// Found a row or hit a database error other than not-found.

		if (SQLERR_error_test ())
		{
			ErrRtn = ERR_DATABASE_ERROR;
		}
		else
		{	// Successful read!
			*drug_ext_info = DE.info;
			ErrRtn = NO_ERROR;
		}	// Successful read!
	}	// Found a row or hit a database error other than not-found.


	// DonR 13Jul2005: Restore normal isolation level.
	RESTORE_ISOLATION;

	return (ErrRtn);
}


/*=======================================================================
||																		||
||			GetParticipatingCode()										||
||																		||
||																		||
|| Find participating code with lowest percent.							||
||																		||
 =======================================================================*/
TErrorCode GetParticipatingCode (TMemberParticipatingType	presParti,
								 TParticTst					*prt,
								 TPresDrugs					*PresLinP,
								 int						FixedPriceEnabled)
{
	/*=======================================================================
	||			    Local declarations										||
	=======================================================================*/
	TParticipatingPrecent	PrevPercent;
	TParticipatingPrecent	NewPercent;
	TErrorCode				err;
	int						RetCode = -1;

	/*=======================================================================
	||				Body of function										||
	=======================================================================*/

	// Find the current percentage for the participation code stored in the
	// prescription-drug structure, and store it in PrevPercent.
    err = GET_PARTICIPATING_PERCENT (PresLinP->RetPartCode, &PrevPercent);

    if (err != NO_ERROR)
    {
		return (err);
    }


	// Test the various participation codes in the Participation Test structure
	// against the existing participation for this drug.
	//
	// DonR 01Jan2010: This routine now sets in-health-basket when it sets other
	// participation-related values.
	//
	// DonR 05Jan2010: Deleted code dealing with previous-Basic, previous-Kesef,
	// and previous-Zahav participation codes - these are no longer in use, so
	// there's no point in wasting time checking them.

	// Check Magen Zahav (formerly Maccabi Magen).
	if (prt->zahav_part != 0)
	{
		if ((PresLinP->mac_magen_code == NO_MACBI_MAGE) && (prt->zahav_part == presParti))
		{
			PresLinP->no_magen_flg = MAC_TRUE;
		}
		else
		{
			if (PresLinP->mac_magen_code != NO_MACBI_MAGE)
			{
				err = GET_PARTICIPATING_PERCENT (prt->zahav_part, &NewPercent);

				if (err != NO_ERROR)
				{
					return (1);
				}

				// DonR 10Nov2003: Per Iris Shaya, Participation Code 5 (which doesn't involve
				// the usual 12-sheckel minimum participation) trumps Participation Code 2.
				// DonR 12Dec2004: Enable handling of fixed price.
				// DonR 11Mar2008: PrticSet should be set TRUE unconditionally!
				if ( (NewPercent		<  PrevPercent)	||
					 (prt->zahav_part	== presParti)	||
					 ((prt->zahav_part	== PARTI_CODE_ONE) && (prt->zahav_fixed_price > 0) && (FixedPriceEnabled))	||
					 ((prt->zahav_part	== PARTI_CODE_FIVE) && (PresLinP->RetPartCode == PARTI_CODE_TWO)) )
				{
					PrevPercent									= NewPercent;
					PresLinP->RetPartCode						= prt->zahav_part;
					PresLinP->ret_part_source.insurance_used	= ZAHAV_INS_USED;
					PresLinP->ret_part_source.state				= CURR_CODE;
					PresLinP->PrticSet							= MAC_TRUE;					// DonR 11Mar2008.
					PresLinP->in_health_pack					= prt->in_health_basket;	// DonR 31Dec2009.

					// If we're dealing with a fixed price, set the appropriate drug value.
					if ((prt->zahav_part		== PARTI_CODE_ONE)	&&
						(prt->zahav_fixed_price	>  0)				&&
						(FixedPriceEnabled))
					{
						PresLinP->PriceSwap  = prt->zahav_fixed_price;
					}

					RetCode = NO_ERROR;
				}
			}	// Member has Magen.
		}	// Member has Magen OR presParti <> magen participation.
	}


	// Check Magen Kesef (formerly Keren Maccabi).
	if( prt->kesef_part != 0 )
	{
		if ((PresLinP->keren_mac_code == NO_KEREN_MACBI) && (prt->kesef_part == presParti))
		{
			PresLinP->no_keren_flg = MAC_TRUE;
		}
		else
		{
			if( PresLinP->keren_mac_code != NO_KEREN_MACBI )
			{
				err = GET_PARTICIPATING_PERCENT( prt->kesef_part, &NewPercent );

				if( err != NO_ERROR )
				{
					return( 1 );
				}

				// DonR 10Nov2003: Per Iris Shaya, Participation Code 5 (which doesn't involve
				// the usual 12-sheckel minimum participation) trumps Participation Code 2.
				// DonR 12Dec2004: Enable handling of fixed price.
				// DonR 11Mar2008: PrticSet should be set TRUE unconditionally!
				if ( (NewPercent		<  PrevPercent)	||
					 (prt->kesef_part	== presParti)	||
					 ((prt->kesef_part	== PARTI_CODE_ONE) && (prt->kesef_fixed_price > 0) && (FixedPriceEnabled))	||
					 ((prt->kesef_part	== PARTI_CODE_FIVE) && (PresLinP->RetPartCode == PARTI_CODE_TWO)) )
				{
					PrevPercent									= NewPercent;
					PresLinP->RetPartCode						= prt->kesef_part;
					PresLinP->ret_part_source.insurance_used	= KESEF_INS_USED;
					PresLinP->ret_part_source.state				= CURR_CODE;
					PresLinP->PrticSet							= MAC_TRUE;					// DonR 11Mar2008.
					PresLinP->in_health_pack					= prt->in_health_basket;	// DonR 31Dec2009.

					// If we're dealing with a fixed price, set the appropriate drug value.
					if ((prt->kesef_part		== PARTI_CODE_ONE)	&&
						(prt->kesef_fixed_price	>  0)				&&
						(FixedPriceEnabled))
					{
						PresLinP->PriceSwap  = prt->kesef_fixed_price;
					}

					RetCode = NO_ERROR;
				}
			}	// Member has Keren.
		}	// Member has Keren OR presParti <> Keren participation.
	}


	// Check basic insurance.
	if (prt->basic_part != 0)
	{
		err = GET_PARTICIPATING_PERCENT (prt->basic_part, &NewPercent);

		if (err != NO_ERROR)
		{
			return (1);
		}

		// DonR 10Nov2003: Per Iris Shaya, Participation Code 5 (which doesn't involve
		// the usual 12-sheckel minimum participation) trumps Participation Code 2.
		// DonR 12Dec2004: Enable handling of fixed price.
		// DonR 11Mar2008: PrticSet should be set TRUE unconditionally!
		if ( (NewPercent		<  PrevPercent)	||
			 (prt->basic_part	== presParti)	||
			 ((prt->basic_part	== PARTI_CODE_ONE) && (prt->basic_fixed_price > 0) && (FixedPriceEnabled))	||
			 ((prt->basic_part	== PARTI_CODE_FIVE) && (PresLinP->RetPartCode == PARTI_CODE_TWO)) )
		{
			PrevPercent									= NewPercent;
			PresLinP->RetPartCode						= prt->basic_part;
			PresLinP->ret_part_source.insurance_used	= BASIC_INS_USED;
			PresLinP->ret_part_source.state				= CURR_CODE;
			PresLinP->PrticSet							= MAC_TRUE;					// DonR 11Mar2008.
			PresLinP->in_health_pack					= prt->in_health_basket;	// DonR 31Dec2009.

			// If we're dealing with a fixed price, set the appropriate drug value.
			if ((prt->basic_part		== PARTI_CODE_ONE)	&&
				(prt->basic_fixed_price	>  0)				&&
				(FixedPriceEnabled))
			{
				PresLinP->PriceSwap  = prt->basic_fixed_price;
			}

			RetCode = NO_ERROR;
		}
	}


	if (RetCode == NO_ERROR)
		return (RetCode);
	else
		return (ERR_CODE_NOT_FOUND);
}
// End of GetParticipatingCode ()



/*=======================================================================
||																		||
||			SimpleFindLowestPtn()										||
||																		||
||																		||
|| Find participating code with lowest percent - new version 09Dec2012.	||
||																		||
|| Assumes that all validation has already been done, and all that		||
|| remains is to determine if the new participation is lower than		||
|| whatever was previously found.										||
||																		||
 =======================================================================*/
TErrorCode SimpleFindLowestPtn	(short			price_code_in,
								 int			fixed_price_in,
								 char			*insurance_type_in,
								 short			HealthBasket_in,
								 TPresDrugs		*PresLinP,
								 int			FixedPriceEnabled)
{
	// Local declarations.
	short			PrevCode		= PresLinP->RetPartCode;
	int				PrevFixedPrice	= PresLinP->PriceSwap;
	short			PrevPercent;
	short			NewPercent;
	short			err;
	short			insurance_used;
	int				RetCode = -1;


	// Find the current percentage for the participation code stored in the
	// prescription-drug structure, and store it in PrevPercent.
    err = GET_PARTICIPATING_PERCENT (PrevCode, &PrevPercent);

    if (err != NO_ERROR)
    {
		return (err);
    }

	// Now get the new percentage.
    err = GET_PARTICIPATING_PERCENT (price_code_in, &NewPercent);

    if (err != NO_ERROR)
    {
		return (err);
    }

	// Set up numeric insurance-used variable - although we're not sure yet that we'll need it.
	switch (*insurance_type_in)
	{
		case 'B':	insurance_used = BASIC_INS_USED;	break;
		case 'K':	insurance_used = KESEF_INS_USED;	break;
		case 'Z':	insurance_used = ZAHAV_INS_USED;	break;
		case 'Y':	insurance_used = YAHALOM_INS_USED;	break;
		default:	insurance_used = NO_INS_USED;
	}

	// If the previous participation is 100% with no reductions, PrevCode will be 1 with PrevFixedPrice of zero.
	// In this case, force in an unrealistically high PrevFixedPrice so that a new "real" fixed price will be
	// accepted as better than the old price. (Note that this still leaves the possibility that someone will
	// set a fixed price higher than the drug's retail price!)
	if  ((PrevCode == 1) && (PrevFixedPrice == 0) && (!PresLinP->PrticSet))
		PrevFixedPrice = RIDICULOUS_HIGH_PRICE;	// Close to maximum 4-byte integer - over NIS 10 million should be unrealistic enough!

	// Test the new participation code against the existing participation for this drug.
	// DonR 10Nov2003: Per Iris Shaya, Participation Code 5 (which doesn't involve
	// the usual 12-sheckel minimum participation) trumps Participation Code 2.
	// Note that (A) we will assign the new participation if the percentage is equal to the old one
	// (unless both are 100%); (B) any fixed price (if fixed-price-setting is enabled) will override
	// any non-fixed-price percentage; and (C) if both old and new versions are fixed-price, the new
	// one will replace the old one UNLESS the old one was cheaper. (The latter is a new feature; it's
	// doubtful if it will be useful in real life, but who knows?)
	// DonR 27May2015: Added logic to the first line of the "if" below such that a percentage discount
	// will not override a genuine fixed-price discount. Note that a real fixed price of zero (which
	// is possible if it comes from the "Meishar" application) will not be set to RIDICULOUS_HIGH_PRICE,
	// so it will survive a test against any percentage reduction or positive fixed price.
	if (insurance_used > 0)
	{
		if ( ((NewPercent		<=  PrevPercent) && (NewPercent < 10000) && ((PrevPercent < 10000) || (PrevFixedPrice == RIDICULOUS_HIGH_PRICE)))	||
			 ((price_code_in	== 1)	&& (fixed_price_in > 0)	&& (FixedPriceEnabled) && ((PrevCode != 1) || (fixed_price_in <= PrevFixedPrice)))	||
			 ((price_code_in	== 5)	&& (PrevCode == 2)) )
		{
			PresLinP->RetPartCode						= price_code_in;
			PresLinP->ret_part_source.insurance_used	= insurance_used;
			PresLinP->ret_part_source.state				= CURR_CODE;		// Is this still needed for anything?
			PresLinP->PrticSet							= MAC_TRUE;
			PresLinP->in_health_pack					= HealthBasket_in;

			// If we're dealing with a fixed price, set the appropriate drug value.
			// (Note that PriceSwap should already be zero if the previous Price Code was anything other than 1,
			// so we shouldn't have to worry about clearing it.)
			if (price_code_in == 1)
			{
				PresLinP->PriceSwap  = fixed_price_in;
			}

			RetCode = NO_ERROR;
		}	// New participation qualifies to replace previous participation.
	}	// Input variables included a valid insurance type.

	// ERR_CODE_NOT_FOUND is not a "real" error - it just means that for whatever reason, we didn't make a
	// change in the participation variables.
	if (RetCode != NO_ERROR)
		RetCode = ERR_CODE_NOT_FOUND;

	return (RetCode);
}
// End of SimpleFindLowestPtn ()



/*=======================================================================
||																		||
||			FindLowestPtnForPrescRequest()								||
||																		||
||																		||
|| Find participating code with lowest percent - for Trn 6001.			||
||																		||
|| Assumes that all validation has already been done, and all that		||
|| remains is to determine if the new participation is lower than		||
|| whatever was previously found.										||
||																		||
 =======================================================================*/
TErrorCode FindLowestPtnForPrescRequest	(	short	NewPriceCode_in,
											int		NewFixedPrice_in,
											short	OldPriceCode_in,
											int		OldFixedPrice_in,
											char	*insurance_type_in,
											short	*AssignedPriceCode_out,
											short	*AssignedPricePercent_out,
											int		*AssignedFixedPrice_out,
											short	*AssignedFixedPriceExists_out,
											short	*AssignedInsuranceUsed_out		)
{
	// Local declarations.
	short			PrevPercent;
	short			NewPercent;
	short			err;
	short			insurance_used;
	int				RetCode = -1;


	// Find the current percentage for the participation code stored in the
	// prescription-drug structure, and store it in PrevPercent.
    err = GET_PARTICIPATING_PERCENT (OldPriceCode_in, &PrevPercent);

    if (err != NO_ERROR)
    {
		GerrLogMini (GerrId, "FindLowestPtnForPrescRequest: Error %d looking up old price code %d.", err, OldPriceCode_in);
		return (err);
    }

	// Now get the new percentage.
    err = GET_PARTICIPATING_PERCENT (NewPriceCode_in, &NewPercent);

    if (err != NO_ERROR)
    {
		GerrLogMini (GerrId, "FindLowestPtnForPrescRequest: Error %d looking up new price code %d.", err, OldPriceCode_in);
		return (err);
    }

	// Set up numeric insurance-used variable - although we're not sure yet that we'll need it.
	switch (*insurance_type_in)
	{
		case 'B':	insurance_used = BASIC_INS_USED;	break;
		case 'K':	insurance_used = KESEF_INS_USED;	break;
		case 'Z':	insurance_used = ZAHAV_INS_USED;	break;
		case 'Y':	insurance_used = YAHALOM_INS_USED;	break;
		default:	insurance_used = NO_INS_USED;
	}

	// If the previous participation is 100% with no reductions, PrevCode will be 1 with PrevFixedPrice of zero.
	// In this case, force in an unrealistically high PrevFixedPrice so that a new "real" fixed price will be
	// accepted as better than the old price. (Note that this still leaves the possibility that someone will
	// set a fixed price higher than the drug's retail price!)
	// DonR 27May2015: Removed a condition. In *any* situation where a fixed price has not been detected
	// (that is, including cases where we've got a percentage reduction) we want to set the previous fixed
	// price to an unrealistically high number.
	if  (OldFixedPrice_in == 0)
		OldFixedPrice_in = RIDICULOUS_HIGH_PRICE;	// Close to maximum 4-byte integer - over NIS 10 million should be unrealistic enough!

	// Test the new participation code against the existing participation for this drug.
	// DonR 10Nov2003: Per Iris Shaya, Participation Code 5 (which doesn't involve
	// the usual 12-sheckel minimum participation) trumps Participation Code 2.
	// Note that (A) we will assign the new participation if the percentage is equal to the old one
	// (unless both are 100%); (B) any fixed price (if fixed-price-setting is enabled) will override
	// any non-fixed-price percentage; and (C) if both old and new versions are fixed-price, the new
	// one will replace the old one UNLESS the old one was cheaper. (The latter is a new feature; it's
	// doubtful if it will be useful in real life, but who knows?)
	// DonR 12Nov2014: Changed the first line of the "if" below such that a percentage reduction will
	// not override a fixed-price reduction. Should this change also be made to SimpleFindLowestPtn()?
	// It looks like a bug to me, but nobody has complained and it's been running for years...
	// DonR 27May2015: Fixed the first line of the "if" below - since we're forcing in a high value for
	// the previous fixed price if in fact there wasn't a previous fixed price, we have to test for that
	// same high value here - *not* for zero!
//GerrLogMini (GerrId, "FindLowestPtnForPrescRequest: Insurance used = %d, Prev %d/%d, New %d/%d.", insurance_used, PrevPercent, OldFixedPrice_in, NewPercent, NewFixedPrice_in);
	if (insurance_used > 0)
	{
		if ( ((NewPercent <= PrevPercent) && (NewPercent < 10000) && (OldFixedPrice_in == RIDICULOUS_HIGH_PRICE))						||
			 ((NewPriceCode_in == 1) && (NewFixedPrice_in > 0) && ((OldPriceCode_in != 1) || (NewFixedPrice_in <= OldFixedPrice_in)))	||
			 ((NewPriceCode_in == 5) && (OldPriceCode_in == 2)) )
		{
			*AssignedPriceCode_out		= NewPriceCode_in;
			*AssignedInsuranceUsed_out	= insurance_used;
			*AssignedPricePercent_out	= NewPercent;

			// If we're dealing with a fixed price, set the appropriate drug value.
			// (Note that PriceSwap should already be zero if the previous Price Code was anything other than 1,
			// so we shouldn't have to worry about clearing it.)
			if (NewPriceCode_in == 1)
			{
				*AssignedFixedPrice_out			= NewFixedPrice_in;
				*AssignedFixedPriceExists_out	= 1;
			}
			else
			{
				*AssignedFixedPrice_out			= 0;
				*AssignedFixedPriceExists_out	= 0;
			}

			RetCode = NO_ERROR;
		}	// New participation qualifies to replace previous participation.
	}	// Input variables included a valid insurance type.

	// ERR_CODE_NOT_FOUND is not a "real" error - it just means that for whatever reason, we didn't make a
	// change in the participation variables.
	if (RetCode != NO_ERROR)
		RetCode = ERR_CODE_NOT_FOUND;

	return (RetCode);
}
// End of FindLowestPtnForPrescRequest ()



/*=======================================================================
||																		||
||			SumMemberDrugsUsed ()										||
||																		||
||																		||
|| Sum total drugs bought by member at reduced cost.					||
||																		||
 =======================================================================*/
TErrorCode SumMemberDrugsUsed (TPresDrugs				*pDrug,
							   TMemberIdentification	membId,
							   TIdentificationCode		idType,
							   TDuration				duration,
							   int						*SumUnits)
{
	ISOLATION_VAR;

	TPresDrugs				*PDrug;
	TMemberIdentification	MembId;
	TIdentificationCode		IdType;
	int						StartDate;
	int						SystmDate;
	int						op;
	int						units;
	TSqlInd					oind;
	TSqlInd					uind;


	// Sum Quantity used.
	REMEMBER_ISOLATION;
	PDrug     = pDrug;
	MembId    = membId;
	IdType    = idType;
	SystmDate = GetDate ();
	StartDate = AddDays (SystmDate, (duration * -1));
	*SumUnits = 0;

	// DonR 06Mar2005: Avoid unnecessary record-locked errors.
	SET_ISOLATION_DIRTY;

	ExecSQL (	MAIN_DB, SumMemberDrugsUsed_READ_PD_totals,
				&units,		&op,
				&MembId,	&PDrug->DrugCode,	&StartDate,
				&IdType,	&DRUG_DELIVERED,	&ROW_NOT_DELETED,
				END_OF_ARG_LIST										);

				uind	= ColumnOutputLengths [1];
				oind	= ColumnOutputLengths [2];

	if (SQLERR_code_cmp (SQLERR_not_found) == MAC_TRUE)
	{
		RESTORE_ISOLATION;	/* DonR 06Mar2005 */
		return (NO_ERROR);
	}
	else
	{
		if (SQLERR_code_cmp (SQLERR_access_conflict) == MAC_TRUE)
		{
			RESTORE_ISOLATION;	/* DonR 06Mar2005 */
			LOG_CONFLICT ();
			return (2);
		}
		else
		{
			if (SQLERR_error_test ())
			{
				RESTORE_ISOLATION;	/* DonR 06Mar2005 */
				return (1);
			}
		}
	}

    if (SQLMD_is_null (uind))
		units = 0;
    
    if (SQLMD_is_null (oind))
		op = 0;

	// DonR 12Aug2003: Avoid division-by-zero problems by ensuring that package
	// size has some real value. (I don't know if there's any real chance of
	// problems, but paranoia is sometimes a good thing.)
	if (PDrug->DL.package_size < 1)
	{
		PDrug->DL.package_size = 1;
	}

    *SumUnits = units + op * PDrug->DL.package_size;

	RESTORE_ISOLATION;	/* DonR 06Mar2005 */

    return (NO_ERROR);

}
/* End of SumMemberDrugsUsed() */


// DonR 30Mar2014: New functions for checking Health Alert Rules.

#define HAR_TEST_MODE_STANDARD		1
#define HAR_TEST_MODE_COMPLIANCE	2
#define HAR_TEST_MODE_NEW_DRUG		3
#define HAR_TEST_MODE_INGR_COUNT	4
#define HAR_TEST_MODE_ALL_TESTS		5	// DonR 01Apr2024 User Story #548221

/*=======================================================================
||																		||
||			CheckHealthAlerts ()										||
||																		||
||																		||
|| Check for member-specific health alerts based on health_alert_rules	||
||																		||
 =======================================================================*/
int CheckHealthAlerts	(	short			ActionType,
							TMemberData		*Member_in,
							TPresDrugs		*SPres,
							short			NumCurrDrugs,
							TDocRx			*DocRx,
							TDrugsBuf		*drugbuf,
							PHARMACY_INFO	*Phrm_info_in,
							short			PrescSource_in,
							short			*v_ErrorCode_out	// Added by DonR 11Sep2024 Bug #350304.
						)
{
	static THealthAlertRules	*Rules			= NULL;
	static char					*Ingredients	= NULL;
	static short				NumRules		= 0;
	static short				RuleArraySize	= -1;	// Force allocation first time even if rules table is empty.
	static short				IngrArraySize	= -1;	// Force allocation first time.
	static int					RefreshDate		= 0;
	static int					RefreshTime		= 0;

	int							fn_restart;
	short						fn_error;
	short						RuleNumber;
	short						IngredientNumber;
	short						DrugTestNum;
	short						ProfTestNum;
	short						DrugNum;
	short						FoundRelevantDrug			= 0;
	short						NeedToCheckHistory			= 0;
	short						PurchaseHistoryLoaded		= 0;
	short						RxFoundInCurrentSale		= 0;
	short						MemberQualifiesByIllness	= 0;
	short						i;
	short						Hist_MM;
	short						Hist_DD;
	int							NumFoundInCurrentSale			= 0;
	int							NumFoundInPastSales				= 0;
	int							IngredientsInBlood				= 0;
	int							PercentCompliance				= 0;
	int							DoseDaysPrescribed				= 0;
	int							DoseDaysPurchased				= 0;
	int							TaggedDrugLargoPurchased		= 0;	// DonR 18Nov2025 User Story #461176: Renamed variable.
	int							TaggedDrugLineNumber			= 0;	// DonR 18Nov2025 User Story #461176: Renamed variable.
	char						PastPurchaseDatesBought [13][32];
	char						DrugTestsPassed			[6];	// DonR 01Apr2024 User Story #548221
	T_HAR_DrugTest				*TEST;
	TDrugListRow				*DL;
	DURDrugsInBlood				*HIST;
	ISOLATION_VAR;

	int					SysDate;
	int					SysTime;
	TMemberData			*Member			= Member_in;
	int					MemberIllnessBitmap;
	THealthAlertRules	*ThisRule;
	int					RuleCount			= 0;
	int					LabResCount			= 0;
	int					DocVisitCount		= 0;
	int					RashamCount			= 0;
	int					DiagnosisCount		= 0;
	int					WarnFreqCount		= 0;	// Marianna 20May2024 Epic #178023
	int					LastWarnDate		= 0;	// Marianna 28May2024 Epic #178023 
	int					WarningPeriodDate	= 0;	// Marianna 28May2024 Epic #178023
	int					ThresholdDate		= 0;	// Marianna 28May2024 Epic #178023
	int					ExplanationCount	= 0;	// Marianna 18Aug2024 Epic #178023
	bool				pharmacy_mahoz_flag;		// Marianna 18Aug2024
	bool				pharmacy_code_flag;			// Marianna 18Aug2024
	bool				member_mahoz_flag;			// Marianna 18Aug2024
	int					LastLabTestDate		= 0;
	int					LastLabTestTime		= 0;
	int					FirstDrugPurchDate	= 0;
	int					MaxIngredient		= 0;
	int					MinAgeBasedDate		= 0;
	int					MaxAgeBasedDate		= 0;
	int					MinLabTestDate		= 0;
	int					MaxLabTestDate		= 0;
	int					MinDocVisitDate		= 0;
	int					MinDocVisitTime		= 0;
	int					MinRxValidFrom		= 0;
	int					MaxRxValidFrom		= 0;
	int					MinRxValidUntil		= 0;
	int					MaxRxValidUntil		= 0;
	short				IgnoreRxSoldStatus	= 0;
	int					Rx_Largo;
	int					Rx_Presc_ID;
	int					Rx_ValidFrom;
	int					Rx_Doctor_ID;
	short				Rx_pharmacology;
	short				Rx_treatment_type;
	short				Rx_ingr_1_code;
	short				Rx_ingr_2_code;
	short				Rx_ingr_3_code;
	short				Rx_ShapeCode;
	int					Rx_ParentGroup;
	short				Rx_SoldStatus;
	short				Rx_OP_prescribed;
	short				Rx_UnitsPrescribed;
	short				Rx_QtyMethod;
	short				Rx_TreatmentLength;
	short				Rx_OP_sold;
	short				Rx_ChronicFlag;
	int					Rx_UnitsSold;
	float				Rx_PackageVolume;
	

	// Initialization.
	SysDate = GetDate ();
	SysTime = GetTime ();
	REMEMBER_ISOLATION;
	SET_ISOLATION_DIRTY;


//GerrLogFnameMini ("HAR_log",
//					GerrId,
//					"CheckHealthAlerts: RefreshDate = %d, RefreshTime = %d, RuleArraySize = %d.",
//					(int)RefreshDate, RefreshTime, RuleArraySize);

	// If necessary, initialize/load the Health Alert Rules array.
	// For now at least, perform hourly reloads.
	if ((Rules		== NULL)		||
		(SysDate	!= RefreshDate)	||
		((SysDate	== RefreshDate) && ((SysTime - RefreshTime) > 9999)))
	{
		RefreshDate = SysDate;
		RefreshTime = SysTime;

		// Get the required size of the Health Alert Rules array.
		ExecSQL (	MAIN_DB, CheckHealthAlerts_READ_RuleCount,
					&RuleCount,	&SysDate,	&SysDate,	END_OF_ARG_LIST	);

//GerrLogFnameMini ("HAR_log",
//					GerrId,
//					"CheckHealthAlerts: RuleCount = %d.",
//					(int)RuleCount);

		// Get the required size of ingredients-prescribed array. Note that as of 22Sep2019,
		// the maximum Ingredient Code is 2082 - and we don't expect this list to expand
		// much, or quickly.
		ExecSQL (	MAIN_DB, CheckHealthAlerts_READ_MaxIngredientCode, &MaxIngredient, END_OF_ARG_LIST	);

		if (SQLCODE == 0)
			MaxIngredient += 200;	// Provide some room for expansion if a bunch of ingredients are added at the same time.
		else
			MaxIngredient =  3000;	// Seems like an adequate default given the current maximum of 2082.


		// Allocate/reallocate the Health Alert Rules array.
		if (RuleCount > RuleArraySize)
		{
			RuleArraySize = RuleCount + 5;	// 5 extra spaces before a realloc is required - should be more than enough.

			if (Rules == NULL)	// First time.
			{
				Rules = (THealthAlertRules *)
						MemAllocReturn (GerrId,
										sizeof (THealthAlertRules) * RuleArraySize,
										"Member Health Alert Rules");
			}
			else
			{
				Rules = (THealthAlertRules *)
						MemReallocReturn (GerrId,
										  (void *)Rules,
										  sizeof (THealthAlertRules) * RuleArraySize,
										  "Member Health Alert Rules");
			}
		}	// Need to (re-)allocate rules array.

//GerrLogFnameMini ("HAR_log",
//					GerrId,
//					"Allocated %d bytes to rules array, dim = %d.",
//					sizeof (THealthAlertRules) * RuleArraySize, RuleArraySize);

		// Allocate/reallocate the Ingredients In Blood array.
		if (MaxIngredient > IngrArraySize)
		{
			IngrArraySize = MaxIngredient + 1;	// MaxIngredient already has "padding" built in.

			if (Ingredients == NULL)	// First time.
			{
				Ingredients = (char *) MemAllocReturn (GerrId, IngrArraySize, "Member Health Alert Ingredients");
			}
			else
			{
				Ingredients = (char *) MemReallocReturn (GerrId, (void *)Ingredients, IngrArraySize, "Member Health Alert Ingredients");
			}
		}	// Need to (re-)allocate ingredients array.


		// Note that we use a separate NumRules variable to count rules actually loaded; in
		// reality, NumRules should always be equal to RuleCount.
		NumRules = 0;

		// DonR 14Mar2019 CR #16045: Added fields for Rasham and Diagnosis checking.
		// DonR 24Sep2019: Added a bunch more fields to support new rule types.
		DeclareAndOpenCursor (	MAIN_DB, CheckHealthAlerts_HAR_cursor, &SysDate, &SysDate, END_OF_ARG_LIST	);

		do
		{
			ThisRule = &Rules[NumRules];

//GerrLogFnameMini ("HAR_log",
//					GerrId,
//					"Last SQLCODE = %d, fetching rule #%d", SQLCODE, NumRules);

			// DonR 14Mar2019 CR #16045: Added fields for Rasham and Diagnosis checking.
			// DonR 24Sep2019: Added a bunch more fields to support new rule types.
			// DonR 21Apr2020 CR #32576: Added timing control (Current/History/Both) to drug rules.
			// DonR 04Dec2023 User Story #529100: Added Shape Code to drug-selection criteria.
			FetchCursorInto (	MAIN_DB, CheckHealthAlerts_HAR_cursor,
								&ThisRule->rule_number,					&ThisRule->rule_desc1,					&ThisRule->rule_desc2,
								&ThisRule->rule_enabled,				&ThisRule->rule_start_date,				&ThisRule->rule_end_date,
								&ThisRule->msg_code_assigned,			&ThisRule->member_age_min,				&ThisRule->member_age_max,
								&ThisRule->member_sex,					&ThisRule->drug_test_mode,				&ThisRule->minimum_count,
								&ThisRule->maximum_count,
					
								&ThisRule->DRG[0].largo_code,			&ThisRule->DRG[0].pharmacology,			&ThisRule->DRG[0].treatment_type,
								&ThisRule->DRG[0].min_treatment_type,	&ThisRule->DRG[0].max_treatment_type,	&ThisRule->DRG[0].chronic_flag,
								&ThisRule->DRG[0].ingredient,			&ThisRule->DRG[0].shape_code,			&ThisRule->DRG[0].parent_group_code,
								&ThisRule->DRG[0].timing,

								&ThisRule->DRG[1].largo_code,			&ThisRule->DRG[1].pharmacology,			&ThisRule->DRG[1].treatment_type,
								&ThisRule->DRG[1].min_treatment_type,	&ThisRule->DRG[1].max_treatment_type,	&ThisRule->DRG[1].chronic_flag,
								&ThisRule->DRG[1].ingredient,			&ThisRule->DRG[1].shape_code,			&ThisRule->DRG[1].parent_group_code,
								&ThisRule->DRG[1].timing,

								&ThisRule->DRG[2].largo_code,			&ThisRule->DRG[2].pharmacology,			&ThisRule->DRG[2].treatment_type,
								&ThisRule->DRG[2].min_treatment_type,	&ThisRule->DRG[2].max_treatment_type,	&ThisRule->DRG[2].chronic_flag,
								&ThisRule->DRG[2].ingredient,			&ThisRule->DRG[2].shape_code,			&ThisRule->DRG[2].parent_group_code,
								&ThisRule->DRG[2].timing,

								&ThisRule->DRG[3].largo_code,			&ThisRule->DRG[3].pharmacology,			&ThisRule->DRG[3].treatment_type,
								&ThisRule->DRG[3].min_treatment_type,	&ThisRule->DRG[3].max_treatment_type,	&ThisRule->DRG[3].chronic_flag,
								&ThisRule->DRG[3].ingredient,			&ThisRule->DRG[3].shape_code,			&ThisRule->DRG[3].parent_group_code,
								&ThisRule->DRG[3].timing,

								&ThisRule->DRG[4].largo_code,			&ThisRule->DRG[4].pharmacology,			&ThisRule->DRG[4].treatment_type,
								&ThisRule->DRG[4].min_treatment_type,	&ThisRule->DRG[4].max_treatment_type,	&ThisRule->DRG[4].chronic_flag,
								&ThisRule->DRG[4].ingredient,			&ThisRule->DRG[4].shape_code,			&ThisRule->DRG[4].parent_group_code,
								&ThisRule->DRG[4].timing,
						
								&ThisRule->purchase_hist_days,			&ThisRule->presc_sale_only,				&ThisRule->lab_check_type,
								&ThisRule->lab_req_code[0],				&ThisRule->lab_req_code[1],				&ThisRule->lab_req_code[2],
								&ThisRule->lab_req_code[3],				&ThisRule->lab_req_code[4],				&ThisRule->lab_req_code[5],
								&ThisRule->lab_result_low,				&ThisRule->lab_result_high,				&ThisRule->new_drug_wait_days,
								&ThisRule->lab_check_days,				&ThisRule->lab_min_age_mm,				&ThisRule->lab_max_age_mm,

								&ThisRule->ok_if_seen_prof[0],			&ThisRule->ok_if_seen_prof[1],			&ThisRule->ok_if_seen_prof[2],
								&ThisRule->ok_if_seen_prof[3],			&ThisRule->ok_if_seen_prof[4],			&ThisRule->ok_if_seen_prof[5],
								&ThisRule->ok_if_seen_prof[6],			&ThisRule->ok_if_seen_prof[7],			&ThisRule->ok_if_seen_prof[8],
								&ThisRule->ok_if_seen_prof[9],			&ThisRule->ok_if_seen_within,
						
								&ThisRule->invert_rasham,				&ThisRule->rasham_code   [0],			&ThisRule->rasham_code   [1],
								&ThisRule->rasham_code   [2],			&ThisRule->rasham_code   [3],			&ThisRule->rasham_code   [4],

								&ThisRule->invert_diagnosis,			&ThisRule->diagnosis_code[0],			&ThisRule->diagnosis_code[1],
								&ThisRule->diagnosis_code[2],			&ThisRule->diagnosis_code[3],			&ThisRule->diagnosis_code[4],

								&ThisRule->invert_illnesses,			&ThisRule->illness_code	 [0],			&ThisRule->illness_code	 [1],
								&ThisRule->illness_code	 [2],

								&ThisRule->days_between_warnings,		&ThisRule->max_num_of_warnings,			&ThisRule->warning_period_length,		
								&ThisRule->explanation_largo_code,

								&ThisRule->pharmacy_mahoz[0],			&ThisRule->pharmacy_mahoz[1], 			&ThisRule->pharmacy_mahoz[2],			
								&ThisRule->pharmacy_mahoz[3],			&ThisRule->pharmacy_mahoz[4], 
								
								&ThisRule->pharmacy_code [0],			&ThisRule->pharmacy_code [1],			&ThisRule->pharmacy_code [2],
								&ThisRule->pharmacy_code [3],			&ThisRule->pharmacy_code [4],			
								
								&ThisRule->member_mahoz  [0],			&ThisRule->member_mahoz  [1],			&ThisRule->member_mahoz	 [2],
								&ThisRule->member_mahoz  [3],			&ThisRule->member_mahoz  [4],			
								END_OF_ARG_LIST																										);

//GerrLogFnameMini ("HAR_log",
//					GerrId,
//					"Loaded Rule %d, Lab_low = %d, Lab_high = %d, drug_test_mode %d, SQLCODE = %d.",
//					(int)ThisRule->rule_number, ThisRule->lab_result_low, ThisRule->lab_result_high, ThisRule->drug_test_mode, SQLCODE);
//
			if (SQLCODE == 0)
			{
// if (ThisRule->rule_number == 802) GerrLogMini (GerrId, "Loaded Rule 802.");
				for (DrugTestNum = ThisRule->drug_tests_enabled = ThisRule->force_history_test = 0; DrugTestNum < 5; DrugTestNum++)
				{
					// For each of five drug-group tests, see if there is in fact something to test. Note that zero *is*
					// a valid test value for Pharmacology Group Code and Treatment Type Code, so a negative value is used
					// to disable testing on these fields. Largo Code and Ingredient Code are more "conventional".
					// DonR 04Dec2023 User Story #529100: Add Shape Code to list of criteria.
					// DonR 28Feb2024: Add Parent Group Code to list of criteria.
					ThisRule->DRG[DrugTestNum].enabled = ((ThisRule->DRG[DrugTestNum].largo_code			>  0)	||
														  (ThisRule->DRG[DrugTestNum].pharmacology			> -1)	||
														  (ThisRule->DRG[DrugTestNum].treatment_type		> -1)	||
														  (ThisRule->DRG[DrugTestNum].min_treatment_type	> -1)	||
														  (ThisRule->DRG[DrugTestNum].max_treatment_type	> -1)	||
														  (ThisRule->DRG[DrugTestNum].chronic_flag			> -1)	||
														  (ThisRule->DRG[DrugTestNum].shape_code			> -1)	||
														  (ThisRule->DRG[DrugTestNum].parent_group_code		> -1)	||
														  (ThisRule->DRG[DrugTestNum].ingredient			>  0));
//if (ThisRule->rule_number == 802) GerrLogMini (GerrId, "Rule 802 DRG[%d].enabled = %d.", DrugTestNum, ThisRule->DRG[DrugTestNum].enabled);

					// Keep a separate status to see if any of these drug tests exist for this rule.
					if (ThisRule->DRG[DrugTestNum].enabled)
					{
						ThisRule->drug_tests_enabled = 1;

						// DonR 19Aug2025 User Story #441834: Add support for "starter" drugs by allowing "not found"
						// to be considered TRUE. We do this by translating 'X', 'Y', and 'Z' to their corresponding
						// "normal" test-type values, and setting a new "invert the result" flag.
						switch (ThisRule->DRG[DrugTestNum].timing)
						{
							case 'X':	ThisRule->DRG[DrugTestNum].timing = 'C';	ThisRule->DRG[DrugTestNum].InvertResult = true;	break;
							case 'Y':	ThisRule->DRG[DrugTestNum].timing = 'H';	ThisRule->DRG[DrugTestNum].InvertResult = true;	break;
							case 'Z':	ThisRule->DRG[DrugTestNum].timing = 'B';	ThisRule->DRG[DrugTestNum].InvertResult = true;	break;
							default:	break;
						}

						// DonR 21Apr2020 CR #32576: If the timing control (Current/History/Both) is unrecognized (or is
						// already 'B'), default to 'B' (= Both current sale and History).
						if ((ThisRule->DRG[DrugTestNum].timing != 'C')		&&
							(ThisRule->DRG[DrugTestNum].timing != 'H'))
						{
							ThisRule->DRG[DrugTestNum].timing = 'B';
						}
//if (ThisRule->rule_number == 802) GerrLogMini (GerrId, "Rule 802 DRG[%d].timing = %c.", DrugTestNum, ThisRule->DRG[DrugTestNum].timing);
					}
					else
					{
						// DonR 21Apr2020 CR #32576: For disabled drug rules, set the timing flag to either 'N'
						// (if this is the first drug rule) or whatever the previous value was. This lets us
						// be smart about setting the force_history_test flag, even if whoever set up the Health
						// Alert Rule did things a little strangely.
						ThisRule->DRG[DrugTestNum].timing = (DrugTestNum == 0) ? 'N' : ThisRule->DRG[DrugTestNum - 1].timing;
					}

					// DonR 21Apr2020 CR #32576: If there is more than one timing type for this set
					// of drug tests, set the force_history_test flag TRUE. This should happen when
					// we want to send alerts for a change from one drug purchased in the past to
					// another drug being purchased now. Note that once it's been set TRUE, we don't
					// want to set it back FALSE! Note also that if the previous timing flag is 'N',
					// the current drug rule is the first enabled one - so we haven't yet established
					// that there is more than one timing type in effect. The result should be that
					// ThisRule->force_history_test will be set TRUE if and only if there is more
					// than one timing type for *enabled* drug rules.
					if ((DrugTestNum > 0)						&&
						(ThisRule->DRG[DrugTestNum].enabled)	&&
						(!ThisRule->force_history_test))
					{
						// Keep this "if" here, so we don't have to trust the C compiler to not try looking
						// at ThisRule->DRG[DrugTestNum - 1] before we know that DrugTestNum is greater than zero.
						if (ThisRule->DRG[DrugTestNum - 1].timing != 'N')
						{
							ThisRule->force_history_test = (ThisRule->DRG[DrugTestNum].timing != ThisRule->DRG[DrugTestNum - 1].timing);
						}
					}

				}	// Loop through drug tests for this rule.


				// Set up dates for history lookups, so we don't have to do the calculations over again
				// for each member. IMPORTANT: Note that a value of 1 means we look at one day, INCLUDING
				// TODAY; a value of 31 means we look at a full month including today, and so on.
				if (ThisRule->purchase_hist_days	> 0)
					ThisRule->MinPurchaseDate	= IncrementDate (SysDate, (1 - ThisRule->purchase_hist_days));
				else
					ThisRule->MinPurchaseDate	= 99999999;

				if (ThisRule->lab_check_days		> 0)
					ThisRule->MinLabTestDate	= IncrementDate (SysDate, (1 - ThisRule->lab_check_days));
				else
					ThisRule->MinLabTestDate	= -99999999;	// If no lab date range is specified, we check all the history we have.

				if (ThisRule->ok_if_seen_within		> 0)
					ThisRule->MinDocVisitDate	= IncrementDate (SysDate, (1 - ThisRule->ok_if_seen_within));
				else
					ThisRule->MinDocVisitDate	= 99999999;

				// Set up non-filled-out Profession Group values as -99, so we don't get spurious matches.
				// For doctor-visit logic to be enabled, at least one Profession Group needs to be non-zero.
				ThisRule->ok_if_seen_enabled = 0;

				for (ProfTestNum = 0; ProfTestNum < 10; ProfTestNum++)
				{
					if (ThisRule->ok_if_seen_prof [ProfTestNum]  < 1)
						ThisRule->ok_if_seen_prof [ProfTestNum]  = -99;
					else
						ThisRule->ok_if_seen_enabled = 1;
				}

				// DonR 13Mar2019 CR #16045: See if Rasham and Diagnosis criteria have meaningful values;
				// also validate illness codes and build illness bitmap to compare against member's
				// illness bitmap.
				ThisRule->rasham_enabled = ThisRule->diagnoses_enabled = ThisRule->illnesses_enabled = ThisRule->illness_bitmap = MemberQualifiesByIllness = 0;

				for (i = 0; i < 5; i++)
				{
					if (ThisRule->rasham_code [i] < 1)
						ThisRule->rasham_code [i] = -99;
					else
						ThisRule->rasham_enabled = 1;

					if (ThisRule->diagnosis_code [i] < 1)
						ThisRule->diagnosis_code [i] = -99;
					else
						ThisRule->diagnoses_enabled = 1;

					if (i < 3)
					{
						if ((ThisRule->illness_code [i] < 1) || (ThisRule->illness_code [i] > 30))
						{
							ThisRule->illness_code [i] = -99;
						}
						else
						{
							ThisRule->illnesses_enabled = 1;
							ThisRule->illness_bitmap |= (1 << (ThisRule->illness_code [i]	- 1));
							//
							// for_cystic_fibro		 1
							// for_tuberculosis		 2
							// for_gaucher			 3
							// for_aids				 4
							// for_dialysis			 5	
							// for_thalassemia		 6
							// for_hemophilia		 7
							// for_cancer			 8
							// for_reserved_1		 9
							// for_reserved_2		10
							// for_reserved_3		11
							// for_work_accident	28
							// for_car_accident		29
							// for_reserved_99		30
							//
						}	// Illness code is within valid range - set illnesses "enabled" and add to bitmap.
					}	// Only three illness slots, so skip for i > 2.
				}	// Loop through five Rasham/Diagnosis slots.

//GerrLogFnameMini ("HAR_log",
//					GerrId,
//					"Rule %d: ok_if_seen_within = %d, MinDocVisitDate = %d, ok_if_seen_enabled = %d.",
//					(int)ThisRule->rule_number, ThisRule->ok_if_seen_within, ThisRule->MinDocVisitDate, ThisRule->ok_if_seen_enabled);
//
//				// If we're inverting the member-illness test, implement the inversion by
//				// changing the illness bitmap we just built to its ones' complement.
//				if ((ThisRule->invert_illnesses[0] == 'Y') || (ThisRule->invert_illnesses[0] == 'y'))
//				{
//					ThisRule->illness_bitmap = ~ThisRule->illness_bitmap;
//				}

				// Marianna 19May2024 Epic #178023: See if warning with frequency have meaningful values.	
				ThisRule->frequency_enabled = (	(ThisRule->days_between_warnings > 0) || 
												((ThisRule->max_num_of_warnings > 0) && (ThisRule->warning_period_length > 0)));

				// Marianna 14Aug2024 Epic #178023: See if pharmacy_mahoz, pharmacy_code, member_mahoz criteria have meaningful values.
				for (i = 0; i < 5; i++)
				{
					if (ThisRule->pharmacy_mahoz[i] < 1)
						ThisRule->pharmacy_mahoz[i] = -99;
					else
						ThisRule->pharmacy_mahoz_enabled = true;

					if (ThisRule->pharmacy_code[i] < 1)
						ThisRule->pharmacy_code[i] = -99;
					else
						ThisRule->pharmacy_code_enabled = true;

					if (ThisRule->member_mahoz[i] < 1)
						ThisRule->member_mahoz[i] = -99;
					else
						ThisRule->member_mahoz_enabled = true;
				}

				// Marianna 14Aug2024 Epic #178023: See if Explanation Largo Code has a meaningful value.
				ThisRule->explanation_enabled = ((ThisRule->explanation_largo_code > 0) && (ThisRule->warning_period_length > 0));

				NumRules++;
			}	// SQLCODE == 0 (i.e. we successfully read a Health Alert Rule)
		}
		while (SQLCODE == 0);

		CloseCursor (	MAIN_DB, CheckHealthAlerts_HAR_cursor	);
	}	// End of Health Alert Rules array initialization/loading.

	// If this was only an initialization call (without an actual member to check), exit now.
	// DonR 14Aug2016: If the member isn't a real one - that is, if his/her data wasn't read
	// from the members table - exit now without checking for health alerts.
	if ((ActionType == INITIALIZE_ONLY) || (!Member->RowExists))
	{
		RESTORE_ISOLATION;
		return (0);
	}

	// DonR 15Sep2019: Create local copy of the member's Illness Bitmap. If the member is *not*
	// flagged as having a serious illness (Maccabi Code <> 7 or 17), then force the bitmap
	// to zero (which is why we need a local copy).
	MemberIllnessBitmap = (( Member->maccabi_code == 7) || (Member->maccabi_code == 17)) ? Member->illness_bitmap : 0;

	// If we get here, we're actually checking health alerts for a member.
	for (RuleNumber = 0, ThisRule = &Rules[0]; RuleNumber < NumRules; RuleNumber++, ThisRule++)
	{
		// Initialize anything that needs to be initialized on a per-rule basis.
		FirstDrugPurchDate	= SysDate;	// Assuming a qualified drug is being bought now; this will be
										// overwritten if we find any qualifying historical purchases.

		// Just to make the code below a little simpler, check the member's illness
		// qualification first and store it in a nice simple variable. Note that
		// a rule bitmap of zero means that all members qualify (unless, of course,
		// the "invert" option is in effect).
		MemberQualifiesByIllness = (ThisRule->illness_bitmap == 0) ? 1 : (ThisRule->illness_bitmap & MemberIllnessBitmap);
		if ((ThisRule->invert_illnesses[0] == 'Y') || (ThisRule->invert_illnesses[0] == 'y'))
			MemberQualifiesByIllness = !MemberQualifiesByIllness;


		// First, ignore rules that don't apply because of member's age or gender, as well
		// as rules that would set an error that has already been assigned.
		// DonR 10Apr2016: Add capability to have Health Alert Rules that apply only to Maccabi
		// pharmacies. These are designated by a rule_enabled value of 2, as opposed to the
		// "normal" enabled value of 1. Also implement the presc_sale_only flag, in case
		// anyone wants to use it.
		// DonR 15Sep2019: Add test for member illnesses. The test, if enabled, can be "normal" or "inverted".
		// The "normal" version means that only a member who has a matching illness will qualify for this
		// Health Alert Rule; if the test is "inverted", then only a member who does *not* have any of the
		// illnesses (up to 3 Illness Codes are allowed) will qualify. Note that the "inverted" version is
		// implemented by changing the rule's illness bitmap to its ones' complement, i.e. inverting every bit.
		// Marianna 20Aug2024 Epic #343262 : add a new value of 3 enabled only at private pharmacies.
		if ((Member->age_months < ThisRule->member_age_min)												||
			(Member->age_months > ThisRule->member_age_max)												||
			((ThisRule->member_sex != 0)		&& (Member->sex != ThisRule->member_sex))				||
//			((ThisRule->illnesses_enabled)		&& (!(ThisRule->illness_bitmap & MemberIllnessBitmap)))	||
			((ThisRule->rule_enabled	== 2)	&& (!Phrm_info_in->maccabi_pharmacy))					||
			((ThisRule->rule_enabled	== 3)	&& (!Phrm_info_in->private_pharmacy))					||
			((ThisRule->presc_sale_only	>  0)	&& (PrescSource_in == RECIP_SRC_NO_PRESC))				||
			( !MemberQualifiesByIllness)																||
			( SetErrorVarExists (ThisRule->msg_code_assigned, 0)))
		{
//if (ThisRule->rule_number == 802)
//GerrLogFnameMini ("HAR_log",
//					GerrId,
//					"Rule %d does not apply to Member %d.",
//					(int)ThisRule->rule_number, Member->ID);
			continue;	// This rule is not relevant to this member.
		}

//
//		// DonR 10Apr2016: Add capability to have Health Alert Rules that apply only to Maccabi
//		// pharmacies. These are designated by a rule_enabled value of 2, as opposed to the
//		// "normal" enabled value of 1. Also implement the presc_sale_only flag, in case
//		// anyone wants to use it.
//		if (((ThisRule->rule_enabled	== 2) && (!Phrm_info_in->maccabi_pharmacy))			||
//			((ThisRule->presc_sale_only >  0) && (PrescSource_in == RECIP_SRC_NO_PRESC)))
//			continue;

		// If we get here, the rule is enabled and applicable to this member, pharmacy, and
		// sale request.

		// First, check "compliance" rules designed to check the extent to which a member
		// has filled his/her qualifying doctor prescriptions.
		// DonR 06Oct2019: We also need to check unfilled/partially filled doctor prescriptions
		// for "polypharmacy" patients - since we need to know the sum of chronic-drug ingredients
		// that *could* be bought along with those that have already been bought.
		if ((ThisRule->drug_test_mode == HAR_TEST_MODE_COMPLIANCE)		||
			(ThisRule->drug_test_mode == HAR_TEST_MODE_INGR_COUNT))
		{
			DoseDaysPrescribed = DoseDaysPurchased = IngredientsInBlood = 0;

			// Set date range depending on the type of rule we're processing.
			// For "compliance", we don't care about the Valid From date at all,
			// and we want to ignore prescriptions that are too old *or* that
			// remain valid after today.
			if (ThisRule->drug_test_mode == HAR_TEST_MODE_COMPLIANCE)
			{
				MinRxValidFrom		= -1;
				MaxRxValidFrom		= 99999999;
				MinRxValidUntil		= ThisRule->MinPurchaseDate;
				MaxRxValidUntil		= SysDate;
				IgnoreRxSoldStatus	= -9999;	// For compliance, we look at all valid Sold Status values.
			}
			// For "ingredient count" rules, we're interested in prescriptions
			// that are unfilled or partially filled, are *not* future-dated,
			// and are still "b'tokef".
			else
			{
				MinRxValidFrom		= -1;
				MaxRxValidFrom		= SysDate;
				MinRxValidUntil		= SysDate;
				MaxRxValidUntil		= 99999999;
				IgnoreRxSoldStatus	= 2;		// For ingredient-count, we ignore fully-sold prescriptions.

				// If we're going to be looking at ingredients, start off with a blank slate.
				memset (Ingredients, 0, IngrArraySize);
			}

			// Cursor for all non-deleted doctor prescriptions. Note that we want to
			// convert any value of drug_list.chronic_flag other than 1 to 0; it makes
			// code elsewhere more simple if all non-chronic drugs have a zero there.
			// Note also that I've excluded all prescriptions that remain valid after the
			// current date, whether they're filled or unfilled. It's rational enough to
			// exclude unfilled prescriptions that still have time left on their "tokef";
			// but I'm not 100% sure if fully-filled prescriptions should be counted or not
			// for purposes of measuring the "compliance percentage". Applying a
			// consistent standard for filled and unfilled prescriptions seems the most
			// sensible approach to me.
			// DonR 06Oct2019: Revised WHERE logic so the cursor will work gracefully for
			// both "compliance" and "ingredient count" rules.
			// DonR 04Dec2023 User Story #529100: Add Shape Code to list of drug-test criteria.
			// (If it's zero or greater, it matches against drug_list/shape_code_new.)
			DeclareAndOpenCursorInto (	MAIN_DB, CheckHealthAlerts_HAR_doctor_rx,
										&Rx_Largo,					&Rx_Presc_ID,			&Rx_ValidFrom,			&Rx_Doctor_ID,
										&Rx_SoldStatus,				&Rx_OP_prescribed,		&Rx_UnitsPrescribed,	&Rx_QtyMethod,
										&Rx_TreatmentLength,		&Rx_UnitsSold,			&Rx_OP_sold,
										&Rx_pharmacology,			&Rx_treatment_type,		&Rx_PackageVolume,
										&Rx_ChronicFlag,			&Rx_ingr_1_code,		&Rx_ingr_2_code,		&Rx_ingr_3_code,
										&Rx_ShapeCode,				&Rx_ParentGroup,

										&Member->ID,				&MinRxValidUntil,		&MaxRxValidUntil,
										&MinRxValidFrom,			&MaxRxValidFrom,		&IgnoreRxSoldStatus,	&Member->ID_code,
										END_OF_ARG_LIST																					);

//if (ThisRule->drug_test_mode == HAR_TEST_MODE_COMPLIANCE)
//GerrLogFnameMini ("\nHAR_log",
//					GerrId,
//					"Opened HAR_doctor_rx, SQLCODE = %d.", SQLCODE);

			do
			{
				FetchCursor (	MAIN_DB, CheckHealthAlerts_HAR_doctor_rx	);
//if (ThisRule->drug_test_mode == HAR_TEST_MODE_COMPLIANCE)
//GerrLogFnameMini ("HAR_log",
//					GerrId,
//					"Fetched HAR_doctor_rx, SQLCODE = %d, Rx %d, Largo %d, Pharmacology %d, Chronic %d.", 
//					SQLCODE, Rx_Presc_ID, Rx_Largo, Rx_pharmacology, Rx_ChronicFlag);

				if (SQLCODE != 0)
					break;

//				// If this Health Alert Rule specifies either chronic or non-chronic drugs, skip
//				// prescriptions for drugs that aren't in the right category. Note that this could
//				// be implemented as part of the above SELECT, but it might impact DB server
//				// performance; given that there aren't all that many current prescriptions for
//				// any given member, it won't cost much to implement this as a "post-SELECT select".
//				// Note also that in drug_list, 0 *or* 2 indicates a non-chronic drug.
//				if (((ThisRule->chronic_flag == 0) && (Rx_ChronicFlag == 1))	||
//					((ThisRule->chronic_flag == 1) && (Rx_ChronicFlag != 1)))
//				{
//					continue;
//				}
//
				// If we get here, we've fetched a doctor prescription that "matured" during the
				// relevant period. Now qualify it based on any drug criteria that were entered
				// for this rule. Note that it's possible for there to be a rule without any
				// drug-specific qualifications, in which case *any* prescription will be used
				// to compute the unfilled percentage and possibly trigger an alert.
				// DonR 21Apr2020 CR #32576: Note that the new timing flag for drug tests is
				// NOT relevant here - at least for now, it applies only to standard drug-purhase
				// checking, not to prescription compliance or ingredient-count checking.
				if (ThisRule->drug_tests_enabled)
				{
					for (DrugTestNum = FoundRelevantDrug = 0; DrugTestNum < 5; DrugTestNum++)
					{
						// If this drug-qualifying criteria set doesn't have any meaningful values,
						// skip to the next one. Keep track of whether at least one drug test was
						// applicable; if none of them were, we consider this prescription relevant.
						TEST = &ThisRule->DRG[DrugTestNum];
						if (!TEST->enabled)
						{
							continue;
						}

						// DonR 04Dec2023 User Story #529100: Add Shape Code to list of drug-test criteria. (If it's
						// zero or greater, it matches against drug_list/shape_code_new.)
						// DonR 28Feb2024: Add Parent Group Code to list of drug-test criteria.
						if (((TEST->largo_code			< 1) || (Rx_Largo			== TEST->largo_code			))	&&
							((TEST->pharmacology		< 0) || (Rx_pharmacology	== TEST->pharmacology		))	&&
							((TEST->treatment_type		< 0) || (Rx_treatment_type	== TEST->treatment_type		))	&&
							((TEST->min_treatment_type	< 0) ||	(Rx_treatment_type	>= TEST->min_treatment_type	))	&&
							((TEST->max_treatment_type	< 0) ||	(Rx_treatment_type	<= TEST->max_treatment_type	))	&&
							((TEST->shape_code			< 0) ||	(Rx_ShapeCode		== TEST->shape_code			))	&&
							((TEST->parent_group_code	< 0) ||	(Rx_ParentGroup		== TEST->parent_group_code	))	&&
							((TEST->chronic_flag		< 0) ||	(Rx_ChronicFlag		== TEST->chronic_flag		))	&&
							((TEST->ingredient			< 1) ||	(Rx_ingr_1_code		== TEST->ingredient)	||
																(Rx_ingr_2_code		== TEST->ingredient)	||
																(Rx_ingr_3_code		== TEST->ingredient			)))
						{
							FoundRelevantDrug = 1;
//if (ThisRule->drug_test_mode == HAR_TEST_MODE_COMPLIANCE)
//GerrLogFnameMini ("HAR_log",
//					GerrId,
//					"Test %d passed!", DrugTestNum);
							break;	// No need to pass more than one test.
						}	// Prescribed drug matched drug-test criteria.
					}	// Loop through five drug-criteria tests.
				}	// There is at least one applicable drug test.
				else
				{
					// If there are no drug tests for this rule, *any* drug qualifies!
					FoundRelevantDrug = 1;
				}	// There aren't any operative drug tests for this Health Alert Rule.

				// If there were no applicable drug tests, then *any* prescription will be used
				// to calculate the unfilled percentage, and may help trigger an alert; and
				// if we did find at least one applicable drug test *and* the prescription met its
				// criteria, that also goes into the calculation that can trigger an alert.
				if (FoundRelevantDrug)
				{
					if (ThisRule->drug_test_mode == HAR_TEST_MODE_COMPLIANCE)
					{
						// For each prescription found, check if it's in the current sale request.
						// If so, take it off the list even if it's not being fully sold.
						// Note that as of 22Sep2019, I'm copying the number of doctor prescriptions (+ 1)
						// into the "OP" element of Prescription Zero - which is otherwise a blank record
						// in the array. This saves having to pass the size of the array as another
						// function parameter. NOTE that the starting point for looping through this
						// array is element 1, not the usual element 0!
						RxFoundInCurrentSale = 0;

						if (DocRx != NULL)
						{
							for (DrugNum = 1; DrugNum < DocRx[0].OP; DrugNum++)
							{
								if ((DocRx [DrugNum].LargoPrescribed	== Rx_Largo		)	&&
									(DocRx [DrugNum].DocID				== Rx_Doctor_ID	)	&&
									(DocRx [DrugNum].PrID				== Rx_Presc_ID	)	&&
									(DocRx [DrugNum].FromDate			== Rx_ValidFrom	))
								{
									RxFoundInCurrentSale = 1;
									break;
								}	// The prescription is part of the current sale request.
							}	// Loop through prescriptions in the current sale request.
						}	// Calling routine passed a valid Doctor Prescriptions array pointer.

//GerrLogFnameMini ("HAR_log",
//					GerrId,
//					"RxFoundInCurrentSale = %d.", RxFoundInCurrentSale);
						// If we found a qualifying doctor prescription and it's *not* in the current
						// sale request, add to total dose-days prescribed and total dose-days sold.
						if (!RxFoundInCurrentSale)
						{
							// Add this prescription to total dose-days prescribed.
							DoseDaysPrescribed += Rx_TreatmentLength;

//GerrLogFnameMini ("HAR_log",
//					GerrId,
//					"DoseDaysPrescribed = %d, Sold Status = %d.", DoseDaysPrescribed, Rx_SoldStatus);
							switch (Rx_SoldStatus)
							{
								case 1:		// If Quantity Method = 1, we want to use *only* the OP value from Clicks.
											// Otherwise we want to use *only* the Total Units from Clicks.
											if (Rx_QtyMethod == 1)
											{
												// For Quantity Method 1, we get only OP from Clicks - convert to Total Units.
												Rx_UnitsPrescribed	= (int) ((float)Rx_OP_prescribed * Rx_PackageVolume);
											}

											// Get the total amount sold in units.
											Rx_UnitsSold += (Rx_OP_sold * Rx_PackageVolume);

											// Units Prescribed (after converting OP above) should never be zero - but just in case...
											if (Rx_UnitsPrescribed == 0)
												Rx_UnitsPrescribed = Rx_UnitsSold;

											// Multiply Treatment Length by the proportion of the total amount prescribed that's been sold so far.
//											DoseDaysPurchased += (int) ((float)Rx_TreatmentLength * (((float)Rx_OP_sold * Rx_PackageVolume) + (float)Rx_UnitsSold) / (float)Rx_UnitsPrescribed);
											DoseDaysPurchased += (int) ((float)Rx_TreatmentLength * ((float)Rx_UnitsSold) / (float)Rx_UnitsPrescribed);
										
											break;

								case 2:		DoseDaysPurchased += Rx_TreatmentLength;	// Simple - completely sold, so all days are accounted for.
											break;

								default:	break;	// Anything other than fully or partially sold is considered unsold.
							}

						}	// This prescription is *not* in the current sale request.
					}	// This rule is a "compliance" test.

					// Process ingredients for "ingredient count" rules.
					else
					{
					// Don't do anything unless there's an actual ingredient code present!
						if (Rx_ingr_1_code > 0)
						{
							// If this is the first time we've seen this ingredient, increment the
							// ingredients-in-blood counter.
							if (Ingredients [Rx_ingr_1_code] == (char)0)
							{
								IngredientsInBlood++;
							}

							// Increment the number-of-occurrences counter for this ingredient. Note that
							// since we're using a charactuer array, we don't want to increment too high -
							// let's keep it to 127, since it's a signed array by default. (In reality,
							// it's unlikely that anyone's going to take anything close to that many drugs
							// with the same ingredient at the same time!)
							if (Ingredients [Rx_ingr_1_code] < (char)127)
							{
								Ingredients [Rx_ingr_1_code]++;
							}
						}	// Rx_ingr_1_code > 0

						if (Rx_ingr_2_code > 0)
						{
							// If this is the first time we've seen this ingredient, increment the
							// ingredients-in-blood counter.
							if (Ingredients [Rx_ingr_2_code] == (char)0)
							{
								IngredientsInBlood++;
							}

							// Increment the number-of-occurrences counter for this ingredient. Note that
							// since we're using a charactuer array, we don't want to increment too high -
							// let's keep it to 127, since it's a signed array by default. (In reality,
							// it's unlikely that anyone's going to take anything close to that many drugs
							// with the same ingredient at the same time!)
							if (Ingredients [Rx_ingr_2_code] < (char)127)
							{
								Ingredients [Rx_ingr_2_code]++;
							}
						}	// Rx_ingr_2_code > 0

						if (Rx_ingr_3_code > 0)
						{
							// If this is the first time we've seen this ingredient, increment the
							// ingredients-in-blood counter.
							if (Ingredients [Rx_ingr_3_code] == (char)0)
							{
								IngredientsInBlood++;
							}

							// Increment the number-of-occurrences counter for this ingredient. Note that
							// since we're using a charactuer array, we don't want to increment too high -
							// let's keep it to 127, since it's a signed array by default. (In reality,
							// it's unlikely that anyone's going to take anything close to that many drugs
							// with the same ingredient at the same time!)
							if (Ingredients [Rx_ingr_3_code] < (char)127)
							{
								Ingredients [Rx_ingr_3_code]++;
							}
						}	// Rx_ingr_3_code > 0

					}	// This rule is an "ingredient count" test.

				}	// Either the prescribed drug qualified, or there were no applicable drug tests for it to pass.
			}	// Cursor through applicable doctor prescriptions.
			while (SQLCODE == 0);

			CloseCursor (	MAIN_DB, CheckHealthAlerts_HAR_doctor_rx	);

			if (ThisRule->drug_test_mode == HAR_TEST_MODE_COMPLIANCE)
			{
				// If we didn't find any relevant prescriptions, don't try to divide by zero!
				// Just assume that if there were no prescriptions, compliance was 100% by definition.
				PercentCompliance = (DoseDaysPrescribed > 0) ? (100 * DoseDaysPurchased) / DoseDaysPrescribed : 100;

				if ((PercentCompliance >= ThisRule->minimum_count) && (PercentCompliance <= ThisRule->maximum_count))
				{
					// The percentage of dose-days prescribed that were purchased is between the minimum
					// and the maximum. In this case, we don't have to do anything here - just continue
					// in the outer "rule loop", and we'll send a message to the pharmacy.
				}
				else
				{
					// The percentage of dose-days prescribed that were purchased is outside the specified
					// range. In this case, we need to abort this iteration of the rules loop so that we
					// *don't* send a message to the pharmacy.
					continue;
				}
			}	// This is a "compliance" rule.

		}	// This is a "compliance" rule OR an "ingredient count" rule, so we need to look
			// at doctor prescriptions.


		// DonR 06Oct2019: We want to test current and past drug purchases for all rule types
		// *except* "compliance" rules. Since "ingredient count" rules look at doctor prescriptions
		// as well as drug sales, I can't use an "else" here anymore.
		if (ThisRule->drug_test_mode != HAR_TEST_MODE_COMPLIANCE)
		{
			// Set default need-to-check-history flag. In "standard" mode, we need to check history
			// unless we find a qualifying drug in the current sale request; in "ingredient" mode
			// we always need to check history; but in "new drug" mode we need to check history only
			// if there is a qualifying drug in the current sale request, so we start with the flag
			// set FALSE.
			// DonR 01Apr2024 User Story #548221: Add new drug-test mode HAR_TEST_MODE_ALL_TESTS.
			// For this mode, all drug tests that actually test something must be passed.
			NeedToCheckHistory = (	(ThisRule->drug_test_mode == HAR_TEST_MODE_STANDARD)	||
									(ThisRule->drug_test_mode == HAR_TEST_MODE_INGR_COUNT)	||
									(ThisRule->drug_test_mode == HAR_TEST_MODE_ALL_TESTS));

			// Initialize counters.
			FoundRelevantDrug = NumFoundInCurrentSale = NumFoundInPastSales = 0;

			// DonR 01Apr2024 User Story #548221: Initialize tests-passed array to all-FALSE.
			// DonR 20Aug2025 User Story #441834: Because we now have the option of "inverted"
			// drug tests (i.e. the test is true if a qualifying drug was NOT found), we need
			// to initialize the Tests Passed array differently: For each "inverted" test, we
			// start off with the assumption that its result is TRUE, and if we find a matching
			// drug in the current request or in purchase history we will set it FALSE.
//			strcpy (DrugTestsPassed, "FFFFF");
			if (ThisRule->drug_tests_enabled)
			{
				for (DrugTestNum = 0, DrugTestsPassed[5] = (char)0; DrugTestNum < 5; DrugTestNum++)
				{
					DrugTestsPassed [DrugTestNum] = (ThisRule->DRG[DrugTestNum].InvertResult) ? 'T' : 'F';
				}
			}
			else
			{
				// If there are no applicable drug tests, apply the old all-FALSE initialization.
				// (It may not be necessary, but it can't hurt.)
				strcpy (DrugTestsPassed, "FFFFF");
			}

			// Loop through drugs in current sale request.
			for (DrugNum = 0; DrugNum < NumCurrDrugs; DrugNum++)
			{
				// Set up a shortcut pointer to the drug_list information for this drug line.
				DL = &SPres[DrugNum].DL;

//				// If this Health Alert Rule specifies either chronic or non-chronic drugs, skip
//				// drugs that aren't in the right category. Note that in drug_list, 0 *or* 2
//				// indicates a non-chronic drug.
//				if (((ThisRule->chronic_flag == 0) && (DL->chronic_flag == 1))	||
//					((ThisRule->chronic_flag == 1) && (DL->chronic_flag != 1)))
//				{
//					continue;
//				}
//
				if (ThisRule->drug_tests_enabled)
				{
					// Since ThisRule->drug_tests_enabled is non-zero, we know that there's at least
					// one applicable drug test.
					for (DrugTestNum = FoundRelevantDrug = 0; DrugTestNum < 5; DrugTestNum++)
					{
						// If this drug-qualifying criteria set doesn't have any meaningful values, skip to the next one.
						// DonR 31Jul2016: Keep track of whether at least one drug test was applicable; if none of them
						// were, we consider this rule as being applicable (so far).
						// DonR 21Apr2020 CR #32576: If this drug test is relevant for past purchase only,
						// skip it for drugs in the current purchase request.
						TEST = &ThisRule->DRG[DrugTestNum];

						// DonR 01Apr2024 User Story #548221: If this drug test isn't enabled, mark it as "passed".
						if (!TEST->enabled)
						{
							DrugTestsPassed [DrugTestNum] = 'T';
						}

						// If this drug test isn't enabled, OR if it doesn't include the current purchase
						// request, skip it for now since we're in the current-request loop.
						if ((!TEST->enabled) || (TEST->timing == 'H'))
						{
							continue;
						}

						// DonR 04Dec2023 User Story #529100: Add Shape Code to list of drug-test criteria. (If it's
						// zero or greater, it matches against drug_list/shape_code_new.)
						// DonR 28Feb2024: Add Parent Group Code to list of drug-test criteria.
						if (((TEST->largo_code			< 1) || (DL->largo_code			== TEST->largo_code			))	&&
							((TEST->pharmacology		< 0) || (DL->pharm_group_code	== TEST->pharmacology		))	&&
							((TEST->treatment_type		< 0) || (DL->treatment_typ_cod	== TEST->treatment_type		))	&&
							((TEST->min_treatment_type	< 0) ||	(DL->treatment_typ_cod	>= TEST->min_treatment_type	))	&&
							((TEST->max_treatment_type	< 0) ||	(DL->treatment_typ_cod	<= TEST->max_treatment_type	))	&&
							((TEST->shape_code			< 0) ||	(DL->shape_code_new		== TEST->shape_code			))	&&
							((TEST->parent_group_code	< 0) ||	(DL->parent_group_code	== TEST->parent_group_code	))	&&
							((TEST->chronic_flag		< 0) ||	(DL->chronic_flag		== TEST->chronic_flag		))	&&
							((TEST->ingredient			< 1) ||	(DL->Ingredient[0].code	== TEST->ingredient)	||
																(DL->Ingredient[1].code	== TEST->ingredient)	||
																(DL->Ingredient[2].code	== TEST->ingredient			)))
						{
							// All tests were either passed or not specified. Note that in "standard" mode, we're
							// not interested in how many of the drug tests we pass - one's enough. But in the new
							// "all tests" mode, we need to scan through all the tests since "failing" any one of
							// them disables the Health Alert.

							// DonR 20Aug2025 User Story #441834: If the new "invert test" flag is set TRUE, a
							// matching drug sets the test-passed flag to 'F' (= false) rather than 'T'. In this
							// case we don't want to set the other "affirmative" variables!
							if (TEST->InvertResult)
							{
								// DonR 01Apr2024 User Story #548221: We're in inverted mode - so a
								// matching drug means that we *failed* the test! We don't stop looping
								// in this case, since some other drug test may give a positive result.
								DrugTestsPassed [DrugTestNum] = 'F';	//  We've failed this test!
							}
							else
							{
								// We're in normal mode - so a matching drug means that we
								// passed the test.
								DrugTestsPassed [DrugTestNum] = 'T';	// DonR 01Apr2024 User Story #548221
								FoundRelevantDrug = 1;

								// DonR 18Nov2025 User Story #461176: If this drug test is for the current
								// sale only, and we're in "normal" drug-testing mode, remember the Largo
								// Code and Line Number of the drug - assuming we actually send the pharmacy
								// an error/warning code, we'll attach it to that drug rather than reporting
								// it only at the "header" level.
								// DonR 30Nov2025 User Story #441834: Generalize the 18Nov2025 change to deal
								// with "all tests mode" and "inverted" tests. In this case, we will attach
								// the alert to the first NON-inverted current-purchace test we passed.
								if (	(	(ThisRule->drug_test_mode == HAR_TEST_MODE_STANDARD)	||
											(ThisRule->drug_test_mode == HAR_TEST_MODE_ALL_TESTS)		)	&&
											(TEST->timing == 'C')											&&
											(TaggedDrugLargoPurchased < 1)											)
								{
									TaggedDrugLargoPurchased	= DL->largo_code;
									TaggedDrugLineNumber		= DrugNum;
								}

								// If we've passed the test AND passing one test is enough, break out of the loop.
								// DonR 01Apr2024 User Story #548221: Don't break out of the loop
								// if we're in the new "all tests" mode.
								if (ThisRule->drug_test_mode != HAR_TEST_MODE_ALL_TESTS)
									break;

							}	// Normal situation - test is *NOT* inverted.

						}	// The current drug in the sale request met test criteria.

					}	// Loop through drug tests for this drug line.

// GerrLogMini (GerrId, "Rule #%d: after current Largo %d, DrugTestsPassed = %s.", ThisRule->rule_number, DL->largo_code, DrugTestsPassed);
				}	// There is at least one operative drug test for this Health Alert Rule.
				else
				{
					// If there are no drug tests for this rule, *any* drug qualifies!
					FoundRelevantDrug = 1;
				}	// There aren't any operative drug tests for this Health Alert Rule.
			
				// If there was at least one operative drug test and the current drug line
				// did *not* qualify, skip to the next drug line.
				if (!FoundRelevantDrug)
					continue;

				// If we get here, we've found a drug in the current sale request that satisfies
				// at least one drug test in the Health Alert Rule. Now decide what to do about it.
				NumFoundInCurrentSale++;

				// In "standard" mode, all we're looking for is at least one qualifying drug;
				// once we've found it in the current sale request, we don't need to continue
				// looping or check prior sales.
				// DonR 21Apr2020 CR #32576: If this rule involves *both* past and present
				// drugs (i.e. it's a rule that sends a message when a member is switched
				// from one drug to a different one) we need to check history even though
				// we found something relevant in the current purchase.
				// DonR 01Apr2024 User Story #548221: "All tests" mode uses the same basic
				// need-to-check-history logic as "standard" mode - except that it needs to
				// loop through all the drugs in the current sale request.
				if (	(ThisRule->drug_test_mode == HAR_TEST_MODE_STANDARD)	||
						(ThisRule->drug_test_mode == HAR_TEST_MODE_ALL_TESTS))
				{
					NeedToCheckHistory = ThisRule->force_history_test;

					// In "standard" mode, we're done looping through drugs; but in "all tests"
					// mode, we need to keep looping unless all tests have already been passed.
					if (	(ThisRule->drug_test_mode == HAR_TEST_MODE_STANDARD)	||
							(!strcmp (DrugTestsPassed, "TTTTT"))	)
					{
						break;
					}
					else
					{	// We're in "all tests" mode, and we have *not* yet passed all the drug tests...
						continue;	// ...so keep looping through drugs.
					}
				}

				// In "new drug" mode, one qualifying drug in the current sale request is enough
				// to tell us that we *do* have to check history for prior qualifying purchases -
				// so jump out of this loop, but *enable* the history search. While we're at it,
				// this is a reasonable point to initialize the array of past purchase dates,
				// since the "new drug" mode is the only one that needs to keep track of distinct
				// purchase dates.
				if (ThisRule->drug_test_mode == HAR_TEST_MODE_NEW_DRUG)
				{
					NeedToCheckHistory			= 1;
					TaggedDrugLargoPurchased	= DL->largo_code;
					TaggedDrugLineNumber		= DrugNum;
					memset (PastPurchaseDatesBought, 0, (13 * 32));
					break;
				}

				// In "ingredient" mode, we need to continue looping through both the current sale
				// request *and* history to count the distinct active ingredients from all qualifying
				// current drugs.
				if (ThisRule->drug_test_mode == HAR_TEST_MODE_INGR_COUNT)
				{
					NeedToCheckHistory = 1;	// Already the default for this mode, but set it again to be paranoid.

					// Loop through active ingredients for this qualifying drug.
					for (IngredientNumber = 0; IngredientNumber < 3; IngredientNumber++)
					{
						// Don't do anything unless there's an actual ingredient code present!
						if (DL->Ingredient[IngredientNumber].code < 1)
							continue;
					
						// If this is the first time we've seen this ingredient, increment the
						// ingredients-in-blood counter.
						if (Ingredients [DL->Ingredient[IngredientNumber].code] == (char)0)
						{
							IngredientsInBlood++;
						}

						// Increment the number-of-occurrences counter for this ingredient. Note that
						// since we're using a charactuer array, we don't want to increment too high -
						// let's keep it to 127, since it's a signed array by default. (In reality,
						// it's unlikely that anyone's going to take anything close to that many drugs
						// with the same ingredient at the same time!)
						if (Ingredients [DL->Ingredient[IngredientNumber].code] < (char)127)
						{
							Ingredients [DL->Ingredient[IngredientNumber].code]++;
						}
					}	// Loop through ingredients for this qualifying drug in current purchase request.
				}	// "Ingredient" testing mode.

				// Just in case someone puts in an unrecognized testing mode, break out of the current-purchase loop.
				// This should never happen in real life!
				else
				{
					// We should never get here, since we've accounted for all "legal" HAR test modes.
					break;
				}

			}	// Loop through drugs in current sale request.


			// DonR 03Dec2025 User Story #441834: For "inverted" drug tests, we don't really know
			// if the test has been passed until *after* we're done looping through all the drugs
			// in the current sale request. But now that we've done that, we can check whether a
			// current-purchase inverted drug test is still showing as "passed" after all the drugs
			// have been compared to the test criteria. (Note that we don't have to do this bit
			// for HAR_TEST_MODE_ALL_TESTS - for that mode, we just test everything and look for
			// and all-TRUE result at the end.)
			if ((ThisRule->drug_test_mode == HAR_TEST_MODE_STANDARD) && (!FoundRelevantDrug))
			{
				for (DrugTestNum = 0; DrugTestNum < 5; DrugTestNum++)
				{
					TEST = &ThisRule->DRG[DrugTestNum];

					// We're looking for an enabled, inverted, current-sale-related
					// test that has *not* been failed.
					if (	(TEST->enabled)								&&
							(TEST->InvertResult)						&&
							(TEST->timing					== 'C')		&&
							(DrugTestsPassed [DrugTestNum]	== 'T')				)
					{
						NumFoundInCurrentSale++;
						NeedToCheckHistory = ThisRule->force_history_test;
						break;
					}
				}	// Loop through drug tests.
			}	// Need to check for "inverted" current-sale tests that have been passed.


			// If required, loop through drugs in past purchases.
			if ((NeedToCheckHistory) && (ThisRule->purchase_hist_days > 0))
			{
				// If (and only if) we get here, purchase history is relevant for health-alert checking.
				// Call get_drugs_in_blood() (which may already have been called for other purposes) once
				// per customer.
				if (!PurchaseHistoryLoaded)
				{
					PurchaseHistoryLoaded = 1;
					fn_error = get_drugs_in_blood (Member,
												   drugbuf,
												   GET_ALL_DRUGS_BOUGHT,
												   &fn_error,
												   &fn_restart);
				}	// Need to call get_drugs_in_blood(); 

				for (DrugNum = 0; DrugNum < drugbuf->max; DrugNum++)
				{
					HIST = &drugbuf->Durgs[DrugNum];

					// If we're in "ingredient" mode, ignore drugs that the member should already have
					// finished taking. Note in this regard that StopBloodDate (which is always the same
					// as StopDate, FYI) is the first day that the drug is *no longer* being taken - so
					// if it's equal to SysDate, the drug is already non-relevant. Also note that for
					// "new drug" mode, we don't want to look at "historical" drug purchases that took
					// place today - we're interested only in actual history.
					if (((ThisRule->drug_test_mode	== HAR_TEST_MODE_INGR_COUNT	) &&	(HIST->StopBloodDate	<= SysDate))	||
						((ThisRule->drug_test_mode	== HAR_TEST_MODE_NEW_DRUG	) &&	(HIST->StartDate		>= SysDate))	||
					/*	  ThisRule->drug_test_mode	== anything at all			) && */	(HIST->StartDate		<  ThisRule->MinPurchaseDate))
					{
						continue;
					}

					if (ThisRule->drug_tests_enabled)
					{
						// Since ThisRule->drug_tests_enabled is non-zero, we know that there's at least
						// one applicable drug test.
						for (DrugTestNum = FoundRelevantDrug = 0; DrugTestNum < 5; DrugTestNum++)
						{
							// If this drug-qualifying criteria set doesn't have any meaningful values, skip to the next one.
							// DonR 31Jul2016: Keep track of whether at least one drug test was applicable; if none of them were, we consider
							// this rule as being applicable (so far).
							// DonR 21Apr2020 CR #32576: If this drug test is relevant for the current purchase only,
							// skip it for drugs previously purchased.
							TEST = &ThisRule->DRG[DrugTestNum];
							if ((!TEST->enabled) || (TEST->timing == 'C'))
							{
								continue;
							}

							// DonR 04Dec2023 User Story #529100: Add Shape Code to list of drug-test criteria. (If it's
							// zero or greater, it matches against drug_list/shape_code_new.)
							// DonR 28Feb2024: Add Parent Group Code to list of drug-test criteria.
							if (((TEST->largo_code			< 1) ||	(HIST->Code					== TEST->largo_code				))	&&
								((TEST->pharmacology		< 0) ||	(HIST->PharmacologyGroup	== TEST->pharmacology			))	&&
								((TEST->treatment_type		< 0) ||	(HIST->TreatmentType		== TEST->treatment_type			))	&&
								((TEST->min_treatment_type	< 0) ||	(HIST->TreatmentType		>= TEST->min_treatment_type		))	&&
								((TEST->max_treatment_type	< 0) ||	(HIST->TreatmentType		<= TEST->max_treatment_type		))	&&
								((TEST->shape_code			< 0) ||	(HIST->ShapeCode			== TEST->shape_code				))	&&
								((TEST->parent_group_code	< 0) ||	(HIST->parent_group_code	== TEST->parent_group_code		))	&&
								((TEST->chronic_flag		< 0) ||	(HIST->chronic_flag			== TEST->chronic_flag			))	&&
								((TEST->ingredient			< 1) ||	(HIST->Ingredient[0].code	== TEST->ingredient)	||
																	(HIST->Ingredient[1].code	== TEST->ingredient)	||
																	(HIST->Ingredient[2].code	== TEST->ingredient				)))
							{
								// All tests were either passed or not specified. Note that in "standard" mode,
								// we're not interested in how many of the drug tests we pass - one's enough.
								// But in the new "all tests" mode, we need to scan through all the tests since
								// "failing" any one of them disables the Health Alert.

								// DonR 20Aug2025 User Story #441834: If the new "invert test" flag is
								// set TRUE, a matching drug sets the test-passed flag to 'F' (= false)
								// rather than 'T'. In this case we don't want to set the other
								// "affirmative" variables!
								if (TEST->InvertResult)
								{
									// DonR 01Apr2024 User Story #548221: We're in inverted mode - so a
									// matching drug means that we *failed* the test! We don't stop looping
									// in this case, since some other drug test may give a positive result.
									DrugTestsPassed [DrugTestNum] = 'F';	//  We've failed this test!
								}
								else
								{
									// We're in normal mode - so a matching drug means that we
									// passed the test.
									FoundRelevantDrug = 1;
									DrugTestsPassed [DrugTestNum] = 'T';	// DonR 01Apr2024 User Story #548221


									// DonR 01Apr2024 User Story #548221: Don't break out of the loop
									// if we're in the new "all tests" mode.
									if (ThisRule->drug_test_mode != HAR_TEST_MODE_ALL_TESTS)
										break;

								}	// Normal situation - test is *NOT* inverted.

							}	// Previously purchased drug matches a test.

						}	// Loop through drug tests for this previously purchased drug.

// GerrLogMini (GerrId, "Rule #%d: after history Largo %d, DrugTestsPassed = %s.", ThisRule->rule_number, HIST->Code, DrugTestsPassed);
					}	// There is at least one operative drug test for this Health Alert Rule.
					else
					{
						// If there are no drug tests for this rule, *any* drug qualifies!
						FoundRelevantDrug = 1;
					}	// There aren't any operative drug tests for this Health Alert Rule.

					// If there was at least one operative drug test and the current drug line
					// did *not* qualify, skip to the next drug line.
					if (!FoundRelevantDrug)
						continue;

					// If we get here, we've found a drug in the current sale request that satisfies
					// any drug tests in the Health Alert Rule. Now decide what to do about it.

					// In "standard" mode, all we're looking for is at least one qualifying drug;
					// once we've found it, we don't need to continue looping.
					// DonR 01Apr2024 User Story #548221: Treat the new "all tests" mode the same as
					// "standard" mode, except that we keep looping through history until all drug
					// tests are passed or we run out of history.
					if (	(ThisRule->drug_test_mode == HAR_TEST_MODE_STANDARD)	||
							(ThisRule->drug_test_mode == HAR_TEST_MODE_ALL_TESTS))
					{
						NumFoundInPastSales++;

						// DonR 10May2020 CR #32662: If ThisRule->new_drug_wait_days has a value, we
						// need to look at *all* past purchases to find the first one that qualifies.
						if (ThisRule->new_drug_wait_days > 0)
						{
							// HIST->StartDate comes from prescription_drugs/date, and should always have
							// a sane value - but qualify it > 0 just to be paranoid.
							if ((HIST->StartDate > 0) && (HIST->StartDate < FirstDrugPurchDate))
							{
								FirstDrugPurchDate = HIST->StartDate;
							}

							continue;	// Do NOT break if we're searching for the first qualifying historical drug purchase!
						}
						else
						{
							// DonR 01Apr2024 User Story #548221: Break out of the history loop if we're in "standard"
							// mode *or* if we're in "all tests" mode and all tests have been passed.
							if (	(ThisRule->drug_test_mode == HAR_TEST_MODE_STANDARD)	||
									(!strcmp (DrugTestsPassed, "TTTTT"))	)
							{
								break;		// ThisRule->new_drug_wait_days doesn't have a value,
											// so any historical purchase is enough.
							}
						}
					}

					// In "new drug" mode, we need to know how many qualifying drugs were taken before
					// the current drug sale - so we need to continue looping until the end.
					if (ThisRule->drug_test_mode == HAR_TEST_MODE_NEW_DRUG)
					{
						// We're actually interested in the number of distinct *days* on which
						// qualifying drugs were bought - not the number of drugs or even the
						// number of purchase transactions. We can assume that we're not interested
						// in more than one year's history (in fact we're interested in considerably
						// less), so we'll just work in terms of month and day.
						Hist_MM = (HIST->StartDate % 10000) / 100;
						Hist_DD = HIST->StartDate % 100;

						// Note that for the moment at least, we won't bother incrementing the
						// counter for any given date beyond 1. If it turns out that someone
						// needs functionality that would require us to know how many qualifying
						// drugs were purchased on any given day, we can add that logic at that point.
						if (PastPurchaseDatesBought [Hist_MM] [Hist_DD] == (char)0)
						{
							PastPurchaseDatesBought [Hist_MM] [Hist_DD]++;
							NumFoundInPastSales++;
						}

						continue;	// In other words, don't do anything special - just keep looping.
					}

					// In "ingredient" mode, we need to continue looping through history to count the
					// distinct active ingredients from all qualifying current drugs.
					if (ThisRule->drug_test_mode == HAR_TEST_MODE_INGR_COUNT)
					{
						// Loop through active ingredients for this qualifying drug.
						for (IngredientNumber = 0; IngredientNumber < 3; IngredientNumber++)
						{
							// Don't do anything unless there's an actual ingredient code present!
							if (HIST->Ingredient[IngredientNumber].code < 1)
								continue;
					
							// If this is the first time we've seen this ingredient, increment the
							// ingredients-in-blood counter.
							if (Ingredients [HIST->Ingredient[IngredientNumber].code] == (char)0)
							{
								IngredientsInBlood++;
							}

							// Increment the number-of-occurrences counter for this ingredient. Note that
							// since we're using a charactuer array, we don't want to increment too high -
							// let's keep it to 127, since it's a signed array by default. (In reality,
							// it's unlikely that anyone's going to take anything close to that many drugs
							// with the same ingredient at the same time!) It's also worth noting that as
							// of September 2019, nobody had asked for any functionality that would require
							// more than a yes/no value for any given ingredient - so this is kind of overkill.
							if (Ingredients [HIST->Ingredient[IngredientNumber].code] < (char)127)
							{
								Ingredients [HIST->Ingredient[IngredientNumber].code]++;
							}
						}	// Loop through ingredients for this qualifying drug in purchase history.
					}	// "Ingredient" testing mode.

					// Just in case someone puts in an unrecognized testing mode, break out of the past-purchase loop.
					// This should never happen in real life!
					else
					{
						// We should never get here, since we've accounted for all "legal" HAR test modes.
						break;
					}

				}	// Loop through drugs purchased in the past.
// GerrLogMini (GerrId, "Rule #%d: after history tests, DrugTestsPassed = %s.", ThisRule->rule_number, DrugTestsPassed);


				// DonR 03Dec2025 User Story #441834: For "inverted" drug tests, we don't really
				// know if the test has been passed until *after* we're done looping through all
				// the drugs in past sales. But now that we've done that, we can check whether a
				// past-purchase inverted drug test is still showing as "passed" after all the
				// drugs have been compared to the test criteria. (Note that we don't have to do
				// this bit for HAR_TEST_MODE_ALL_TESTS - for that mode, we just test everything
				// and look for and all-TRUE result at the end.)
				if ((ThisRule->drug_test_mode == HAR_TEST_MODE_STANDARD) && (!FoundRelevantDrug))
				{
					for (DrugTestNum = 0; DrugTestNum < 5; DrugTestNum++)
					{
						TEST = &ThisRule->DRG[DrugTestNum];

						// We're looking for an enabled, inverted, current-sale-related
						// test that has *not* been failed.
						if (	(TEST->enabled)								&&
								(TEST->InvertResult)						&&
								(TEST->timing					!= 'C')		&&
								(DrugTestsPassed [DrugTestNum]	== 'T')				)
						{
							NumFoundInPastSales++;
							break;
						}
					}	// Loop through drug tests.
				}	// Need to check for "inverted" past-sale tests that have been passed.

			}	// Need to check past drug purchases.


			// We've finished checking current and past purchases. Now, based on which testing mode
			// this Health Alert Rule specifies, decide whether to continue to the lab-result tests,
			// or just give up and go on to the next rule.

			// DonR 21Apr2020 CR #32576: In "standard mode", if the current Health Alert Rule
			// is testing for a switch from one set of drugs to a different set of drugs, we
			// want to trigger the alert only if we got a "hit" in both the current sale *and*
			// in purchase history...
			// DonR 01Apr2024 User Story #548221: ...But in "all tests" mode, we work according
			// to a results string that represents the status of all drug tests performed, and
			// measure "success" by all test results being TRUE.
			if (	(ThisRule->drug_test_mode == HAR_TEST_MODE_STANDARD)	||
					(ThisRule->drug_test_mode == HAR_TEST_MODE_ALL_TESTS))
			{
				// DonR 01Apr2024 User Story #548221: For "all tests" mode, success requires
				// that all drug tests were passed.
				if (ThisRule->drug_test_mode == HAR_TEST_MODE_ALL_TESTS)
				{
					if (strcmp (DrugTestsPassed, "TTTTT"))
					{
						// Result string is *not* all TRUE, so the drug tests have failed.
						continue;
					}
				}

				else	// "Standard" drug-test mode.
				{
					if (ThisRule->force_history_test)
					{
						// Both past *and* current purchases must have been found to trigger the alert.
						if ((NumFoundInCurrentSale < 1) || (NumFoundInPastSales < 1))
							continue;
					}
					else
					{
						// Either a current *or* a past purchase will trigger the alert.
						if ((NumFoundInCurrentSale < 1) && (NumFoundInPastSales < 1))	// Could also just look at FoundRelevantDrug,
							continue;													// but this is a little safer.
					}
				}	// "Standard" drug-test mode.

				// If we get here, the drug tests have been passed successfully.

				// DonR 10May2020 CR #32662: If ThisRule->new_drug_wait_days has a value and
				// we found a qualifying drug purchase, set ThisRule->MinLabTestDate based on
				// the first purchase date we found incremented by ThisRule->new_drug_wait_days.
				// Note that in this case we ignore any previous value of ThisRule->MinLabTestDate!
				if (ThisRule->new_drug_wait_days > 0)
				{
					ThisRule->MinLabTestDate = IncrementDate (FirstDrugPurchDate, ThisRule->new_drug_wait_days);
				}
			}	// "Standard" or "all test" drug-testing mode.

			// In "new drug" mode, the question is how many times qualifying drugs were
			// bought before the current drug sale - so compare our answer to the parameters
			// set in the Health Alert Rule. Note that the minimum and maximum counts
			// are for the number of *past* sales - so if we want to trigger an alert
			// if this is the first or second sale of a drug type (e.g. opiates), we
			// want a minimum of 0 and a maximum of 1. Note also that we're actually
			// counting days on which drugs were bought, rather than the actual number of
			// drugs or the number of purchase transactions. This is because the whole point
			// of the "new drug" alert is to make sure that the pharmacist has discussed
			// the new drug with the patient, and we're not going to assume that multiple
			// conversations took place on the same day.
			if (ThisRule->drug_test_mode == HAR_TEST_MODE_NEW_DRUG)
			{
				if (( NumFoundInCurrentSale	< 1)						||
					( NumFoundInPastSales	< ThisRule->minimum_count)	||
					((NumFoundInPastSales	> ThisRule->maximum_count) && (ThisRule->maximum_count > -1)))
				{
					continue;
				}
			}

			if (ThisRule->drug_test_mode == HAR_TEST_MODE_INGR_COUNT)
			{
				if (( IngredientsInBlood < ThisRule->minimum_count)	||
					((IngredientsInBlood > ThisRule->maximum_count) && (ThisRule->maximum_count > -1)))
				{
					continue;
				}
			}

		}	// Health Alert Rule test mode is anything other than "compliance".


		// If a lab test is relevant to this rule, see if a qualifying lab result exists.
		// There are six types of test on lab results, coded in ThisRule->lab_check_type:
		// 1 = test was performed;
		// 2 = test was *not* performed;
		// 3 = result was above the upper limit;
		// 4 = result was below the lower limit;
		// 5 = result was outside the limits in either direction;
		// 6 = result was within limits (*including* results equal to one of the limits).
		//
		// DonR 08Aug2016: Added two new parameters to the rule table to permit qualifying
		// lab results based on the member's age at the time the test was taken. This was needed
		// because a new rule recommends a blood count for kids in their second year of life,
		// unless such a test was already given when they were nine months old or older. A
		// blood count taken before nine months of age is not relevant, and the rule should
		// still trigger in that case.
		MinAgeBasedDate	= (ThisRule->lab_min_age_mm > -1) ? AddMonths (Member->BirthDate, ThisRule->lab_min_age_mm) : 0;
		MaxAgeBasedDate	= (ThisRule->lab_max_age_mm > -1) ? AddMonths (Member->BirthDate, ThisRule->lab_max_age_mm) : 99999999;

//GerrLogFnameMini ("HAR_log",
//					GerrId,
//					"MinAgeBasedDate = %d, MaxAgeBasedDate = %d.",
//					MinAgeBasedDate, MaxAgeBasedDate);

		// Set date range using the larger of the two possible minimum dates, and a maximum date that can't
		// be later than today (just in case there's bogus data in the table).
		// DonR 10May2020 CR #32662: Note that there is no change here, meaning that *if* someone has
		// specified both new_drug_wait_days *and* age-based lab-test criteria, the system will use the
		// smallest date range that satisfies both conditions. I don't think this is likely to happen in
		// real life, since age-based criteria and drug-purchase-based criteria are meant to solve two
		// very different problems.
		MinLabTestDate	= (ThisRule->MinLabTestDate	> MinAgeBasedDate)	? ThisRule->MinLabTestDate	: MinAgeBasedDate;
		MaxLabTestDate	= (SysDate					< MaxAgeBasedDate)	? SysDate					: MaxAgeBasedDate;

		// DonR 14May2020 CR #32662: If the new "wait X days after drug purchase
		// before performing the lab test" interval would push the minimum date
		// to receive test results past today, don't give the warning.
		if ((ThisRule->new_drug_wait_days > 0) && (ThisRule->MinLabTestDate > SysDate) && (ThisRule->lab_check_type == 2))
		{
			continue;
		}


		// Note that if we're doing anything with lab results, the first Request Code field must
		// have a real value. The remaining "alternate" fields are optional.
		if ((ThisRule->lab_check_type > 0) && (ThisRule->lab_req_code[0] > 0))
		{
			ExecSQL (	MAIN_DB, CheckHealthAlerts_READ_LabLastTestDate,
						&LastLabTestDate,				&LabResCount,
						&Member->ID,					&Member->ID_code,
						&ThisRule->lab_req_code[0],		&ThisRule->lab_req_code[1],		&ThisRule->lab_req_code[2],
						&ThisRule->lab_req_code[3],		&ThisRule->lab_req_code[4],		&ThisRule->lab_req_code[5],
						&MinLabTestDate,				&MaxLabTestDate,
						&ThisRule->lab_result_high,		&ThisRule->lab_check_type,
						&ThisRule->lab_result_low,		&ThisRule->lab_check_type,
						&ThisRule->lab_result_high,		&ThisRule->lab_result_low,		&ThisRule->lab_check_type,
						&ThisRule->lab_result_low,		&ThisRule->lab_result_high,		&ThisRule->lab_check_type,
						END_OF_ARG_LIST																					);

//GerrLogFnameMini ("HAR_log",
//					GerrId,
//					"For Rule %d, LastLabTestDate = %d, SQLCODE = %d.",
//					(int)ThisRule->rule_number, (int)LastLabTestDate, (int)SQLCODE);

			// Make sure we really did see a qualifying lab result.
			if ((SQLCODE != 0) || (LabResCount < 1) || (LastLabTestDate < 19900101))
				LastLabTestDate = 0;

			// If no relevant lab result was found (or if we're checking for the absense of a test
			// and a relevant result *was* found), give up on this Health Alert Rule.
			if (((ThisRule->lab_check_type != 2) && (LastLabTestDate == 0))		||
				((ThisRule->lab_check_type == 2) && (LastLabTestDate != 0)))
			{
				continue;
			}

			// If we did find a relevant lab result, retrieve its time - in case the patient saw a doctor
			// on the same day the result was released, we need to know if s/he saw the doctor before or
			// after the result was available.
			if (ThisRule->lab_check_type != 2)
			{
				ExecSQL (	MAIN_DB, CheckHealthAlerts_READ_LabLastTestTime,
							&LastLabTestTime,
							&Member->ID,				&Member->ID_code,
							&ThisRule->lab_req_code[0],	&ThisRule->lab_req_code[1],	&ThisRule->lab_req_code[2],
							&ThisRule->lab_req_code[3],	&ThisRule->lab_req_code[4],	&ThisRule->lab_req_code[5],
							&ThisRule->MinLabTestDate,	&LastLabTestDate,
							&ThisRule->lab_result_high,	&ThisRule->lab_check_type,
							&ThisRule->lab_result_low,	&ThisRule->lab_check_type,
							&ThisRule->lab_result_high,	&ThisRule->lab_result_low,	&ThisRule->lab_check_type,
							END_OF_ARG_LIST																			);

//GerrLogFnameMini ("HAR_log",
//					GerrId,
//					"For Rule %d, LastLabTestTime = %d, SQLCODE = %d.",
//					(int)ThisRule->rule_number, (int)LastLabTestTime, (int)SQLCODE);

			}	// Looking for tests that actually were performed (i.e. not Type 2).
		}	// This rule involves some type of lab-test check.


		// If a recent visit to a doctor with a particular Profession Group Code will turn off the
		// Health Alert, check auth and authprev to see if such a visit has taken place.
		// In most cases, this logic should only execute if there is a lab-test criterion for this
		// health-alert rule; however, it's theoretically possible that someone could set up a rule
		// based on doctor visits independent of lab tests, so this logic will be executed even if
		// there are no lab-test criteria.
		if (ThisRule->ok_if_seen_enabled)
		{
			if (ThisRule->MinDocVisitDate == 99999999)
			{
				// If there is no explicit setting for doctor-visit date range, we're looking for a doctor
				// visit after a relevant lab-test result. Obviously, if this rule isn't looking at lab-test
				// results, a specific doctor-visit date range should be set up.
				MinDocVisitDate = LastLabTestDate;
				MinDocVisitTime = LastLabTestTime;
			}
			else
			{
				// An explicit doctor-visit date range was set - so just set the time variable to zero.
				// Remember to set the simple MinDocVisitDate variable from the structure variable!
				MinDocVisitDate = ThisRule->MinDocVisitDate;
				MinDocVisitTime = 0;
			}


			// DonR 30Mar2017: Instead of the "dead" tables auth and visitnew, use the new
			// table member_prof_visit. (Note that as of today, the new table isn't being
			// populated - but neither are the old ones, mostly!) Since there are still
			// some rows being written to auth and visitnew, I'll leave the SQL for those
			// tables in place for now. Once member_prof_visit is in full use, I'll delete
			// the code for the old tables.
			ExecSQL (	MAIN_DB, CheckHealthAlerts_READ_DocVisitCount,
						&DocVisitCount,
						&Member->ID,
						&ThisRule->ok_if_seen_prof[0],		&ThisRule->ok_if_seen_prof[1],
						&ThisRule->ok_if_seen_prof[2],		&ThisRule->ok_if_seen_prof[3],
						&ThisRule->ok_if_seen_prof[4],		&ThisRule->ok_if_seen_prof[5],
						&ThisRule->ok_if_seen_prof[6],		&ThisRule->ok_if_seen_prof[7],
						&ThisRule->ok_if_seen_prof[8],		&ThisRule->ok_if_seen_prof[9],
						&Member->ID_code,					&MinDocVisitDate,
						&MinDocVisitTime,					&MinDocVisitDate,
						END_OF_ARG_LIST														);

			if (SQLCODE != 0)
				DocVisitCount = 0;


			// If the member has had at least one qualifying doctor visit, we do *not* have to send a message
			// to the pharmacy.

//GerrLogFnameMini ("HAR_log",
//					GerrId,
//					"For Rule %d, DocVisitCount = %d.",
//					(int)ThisRule->rule_number, (int)DocVisitCount);

			if (DocVisitCount > 0)
				continue;
		}	// This rule requires doctor-visit checking.



		// DonR 14Mar2019 CR #16045: Add Rasham checking.
		if (ThisRule->rasham_enabled)
		{
			ExecSQL (	MAIN_DB, CheckHealthAlerts_READ_RashamCount,
						&RashamCount,
						&Member->ID,
						&ThisRule->rasham_code[0],	&ThisRule->rasham_code[1],
						&ThisRule->rasham_code[2],	&ThisRule->rasham_code[3],
						&ThisRule->rasham_code[4],	&Member->ID_code,
						END_OF_ARG_LIST											);

			// If the SELECT failed, assume that this rule is non-applicable.
			if (SQLCODE != 0)
			{
				GerrLogFnameMini ("HAR_log", GerrId, "For Rule %d, get Rasham count SQLCODE = %d.", SQLCODE);
				continue;
			}

			if (((RashamCount == 0) && (strchr ("Yy", ThisRule->invert_rasham[0]) == NULL))	||
				((RashamCount >  0) && (strchr ("Yy", ThisRule->invert_rasham[0]) != NULL)))
			{
				continue;
			}
		}	// This rule requires rasham checking.


		// DonR 14Mar2019: Member Diagnosis checking. Note that as of now, nobody has actually
		// asked for this feature - but since it works pretty much the same as Rasham checking,
		// it made sense to implement it in case someone eventually does ask for it.
		if (ThisRule->diagnoses_enabled)
		{
			ExecSQL (	MAIN_DB, CheckHealthAlerts_READ_DiagnosisCount,
						&DiagnosisCount,
						&Member->ID,
						&ThisRule->diagnosis_code[0],	&ThisRule->diagnosis_code[1],
						&ThisRule->diagnosis_code[2],	&ThisRule->diagnosis_code[3],
						&ThisRule->diagnosis_code[4],	&Member->ID_code,
						END_OF_ARG_LIST													);

			// Since we can also get diagnosis codes from AS/400 ishurim, we need to check there as well.
			if ((SQLCODE != 0) || (DiagnosisCount < 1))
			{
				ExecSQL (	MAIN_DB, CheckHealthAlerts_READ_DiagnosisCountFromIshurim,
							&DiagnosisCount,
							&Member->ID,					&SysDate,
							&SysDate,						&Member->ID_code,
							&ThisRule->diagnosis_code[0],	&ThisRule->diagnosis_code[1],
							&ThisRule->diagnosis_code[2],	&ThisRule->diagnosis_code[3],
							&ThisRule->diagnosis_code[4],	END_OF_ARG_LIST					);
			}	// Need to check special_prescs because we didn't find anything in member_diagnoses.

			// If the SELECT failed, assume that this rule is non-applicable.
			if (SQLCODE != 0)
			{
				GerrLogFnameMini ("HAR_log", GerrId, "For Rule %d, get diagnosis count SQLCODE = %d.", SQLCODE);
				continue;
			}

			if (((DiagnosisCount == 0) && (strchr ("Yy", ThisRule->invert_diagnosis[0]) == NULL))	||
				((DiagnosisCount >  0) && (strchr ("Yy", ThisRule->invert_diagnosis[0]) != NULL)))
			{
				continue;
			}
		}	// This rule requires diagnosis checking.


		// Marianna 21May2024 Epic #178023 :
		// Set up dates for period length
		// IMPORTANT: Note that a value of 1 means we look at one day, INCLUDING
		// TODAY; a value of 31 means we look at a full month including today, and so on.
		if (ThisRule->frequency_enabled)
		{
			// Set the time span for checking the database for previously-sent warnings.
			WarningPeriodDate = IncrementDate (	SysDate,
												1 - ((ThisRule->warning_period_length > ThisRule->days_between_warnings) ?
														ThisRule->warning_period_length : ThisRule->days_between_warnings));

			// If there is a minimum time to wait for sending repeated warnings, set
			// the last permitted date - if the warning was given later than that, we
			// don't want to send it again (yet).
			if (ThisRule->days_between_warnings > 0)
				ThresholdDate = IncrementDate(SysDate, (0 - ThisRule->days_between_warnings));
			else
				ThresholdDate = 99999999;
			
			ExecSQL	(	MAIN_DB, CheckHealthAlerts_READ_CheckWarnings,
						&WarnFreqCount,						&LastWarnDate,						
						&Member->ID,						&WarningPeriodDate,
						&Member->mem_id_extension,			&ThisRule->msg_code_assigned,
						END_OF_ARG_LIST														);
				
			// If the SELECT failed, assume that this rule is non-applicable.
			if (SQLCODE != 0)
			{
				SQLERR_error_test();
				continue;
			}
			
			// SELECT not failed, check the answer.
			// If the member has had message more that max, we do *not* have to send a message
			// to the pharmacy.
			if ((ThisRule->warning_period_length	>  0)	&&
				(ThisRule->max_num_of_warnings		>  0)	&&
				(WarnFreqCount						>= ThisRule->max_num_of_warnings))
			{
				continue;
			}
			
			// ...And if the last warning was too recent, we also want to suppress the new warning.
			if (LastWarnDate > ThresholdDate)
			{
				continue;
			}
			
		}	// This rule requires warning frequency checking.
		// Marianna 21May2024 Epic #178023 end.
	
		// Marianna 14Aug2024 Epic #178023:test type - Member Mahoz, Pharmacy Mahoz, Pharmacy Code
		// We check in separate blocks, if any of the three tests are enabled, 
		// and set a flag true if we find a match.
		if (ThisRule->pharmacy_mahoz_enabled)
		{
			for (i = 0, pharmacy_mahoz_flag = false; i < 5; i++)
			{
				if (ThisRule->pharmacy_mahoz[i] == Phrm_info_in->mahoz)
					pharmacy_mahoz_flag = true;
			}

			// We do *not* send a message to the pharmacy if pharmacy mahoz flag is still false
			if (!pharmacy_mahoz_flag)
			{
				continue;
			}
		}	// Pharmacy Mahoz test is enabled.

		if (ThisRule->pharmacy_code_enabled)
		{
			for (i = 0, pharmacy_code_flag = false; i < 5; i++)
			{
				if (ThisRule->pharmacy_code[i] == Phrm_info_in->pharmacy_code)
					pharmacy_code_flag = true;
			}

			// We do *not* send a message to the pharmacy if pharmacy code flag is still false
			if (!pharmacy_code_flag)
			{
				continue;
			}
		}	// Pharmacy Code test is enabled.

		if (ThisRule->member_mahoz_enabled)
		{
			for (i = 0, member_mahoz_flag = false; i < 5; i++)
			{
				if (ThisRule->member_mahoz[i] == Member->mahoz)
					member_mahoz_flag = true;
			}

			// We do *not* send a message to the pharmacy if member mahoz flag is still false
			if (!member_mahoz_flag)
			{
				continue;
			}
		}	// Member Mahoz test is enabled.

		// Check if the member has had at least one "sale" of fictitious explanation largo.
		if (ThisRule->explanation_enabled)
		{
			ExecSQL(MAIN_DB, CheckHealthAlerts_READ_PrescDrugs,
				&ExplanationCount,
				&Member->ID, &ThisRule->explanation_largo_code, &WarningPeriodDate,
				&Member->ID_code, END_OF_ARG_LIST);

			//if (SQLCODE != 0)
			//	ExplanationCount = 0;

			// If the member has had one fictive sale of largo, we do *not* have to send a message to the pharmacy.
			// Pharmacy sold fictive largo code that indicate that a specific explanation was given to the member, 
			// about specific health alert.
			if (ExplanationCount > 0)
				continue;
		}
		

		// If we get here, we actually do have to send an alert message to the pharmacy.
		// Note that these Health Alert Messages are only for information, and will never
		// prevent a drug sale - so we don't have to worry about whether the error is a
		// severe one (it isn't).
		// DonR 03Oct2019: If we're in "new drug" mode, set the warning to be connected to the first
		// qualifying drug in the current purchase, rather than at the "header" level.
		// DonR 30Sep2020: If we're giving a drug-specific message, test whether it's a fatal
		// one - and if it is, set the global "abort sale" error code as well.
		// DonR 11Sep2024 Bug #350304: Added a new function argument, v_ErrorCode_out, which will be
		// the address of the calling routine's v_ErrorCode. This will allow v_ErrorCode to be set by
		// the Health Alerts module, which is important in those cases where Health Alerts assigns a
		// "chasima-level" error. Before this fix, a pharmacy could complete the sale even if Health
		// Alerts assigned a critical error.
		// DonR 18Nov2025 User Story #461176: We are now going to assign Health Alerts to specific
		// drugs when an alert is triggered by a current-sale-only "conventional" drug test (i.e.
		// HAR_TEST_MODE_STANDARD and a "Timing Type C" test is passed). Accordingly, tag a specific
		// drug based on a non-zero value for TaggedDrugLargoPurchased rather than the Drug Test Mode.
//		if (ThisRule->drug_test_mode == HAR_TEST_MODE_NEW_DRUG)
		if (TaggedDrugLargoPurchased > 0)
		{
			if (SetErrorVarArr (	&SPres[TaggedDrugLineNumber].DrugAnswerCode, ThisRule->msg_code_assigned,
									TaggedDrugLargoPurchased, TaggedDrugLineNumber + 1, NULL, NULL)	)
			{
				SetErrorVarArr (v_ErrorCode_out, ERR_DRUG_S_WRONG_IN_PRESC, 0, 0, NULL, NULL);
			}
		}
		else
		{
			SetErrorVarArr (v_ErrorCode_out, ThisRule->msg_code_assigned, 0, 0, NULL, NULL);
		}

	}	// Loop through Health Alert Rules.


	// All done!
	RESTORE_ISOLATION;
	return(0);
}



// DonR 30Mar2015: New function to handle table updates when a drug
// sale (or a drug-sale deletion) is completed.
// DonR 04Mar2025: Got rid of a bunch of duplication in variables, since
// we no longer need to copy every function parameter to a declared SQL
// "host" variable.
int update_doctor_presc (	short			LateArrivalMode_in,		// 0 = normal, 1 = process late-arriving doctor prescription
							short			ActionCode,
							int				v_MemberID,
							short			v_IDCode,
							int				v_RecipeIdentifier,
							short			LineNumber,
							int				DocID,
							short			DocIDType,
							int				DocPrID,
							int				LargoPrescribed,
							int				LargoSold_in,
							TDrugListRow	*DL_in,					// Always NULL in late-arrival mode.
							int				ValidFromDate,
							int				SoldDate,
							int				SoldTime,
							short			OP_sold_in,
							short			UnitsSold_in,
							short			PrevUnsoldOP,
							short			PrevUnsoldUnits,
							short			PrescriptionType_in,	// DonR 01Aug2024 Bug #334612.
							int				DeletedPrescID,
							bool			ThisIsTheLastRx_in,
							int				*CarryForwardUnits_in_out,
							int				*LargoPrescribed_out,
							int				*ValidFromDate_out,
							short			*RxLinkCounter_out,
							int				*VisitDate_out,
							int				*VisitTime_out,
							int				*Rx_Added_Date_out,
							int				*Rx_Added_Time_out
						)
{
	int		tries;
	int		EffectiveRxUnits;
	int		UnitsNeededToClose;
	int		read_drug_ok;
	short	ForceRxFullySold;	// DonR 21Nov2017 CR #13341.
	short	ForceArrivalQProcessing			= 0;
	bool	UseReportedUnsoldOpUnits;
	ISOLATION_VAR;

	// DonR 04Mar2025: Since we are no longer using embedded (Informix) SQL, we
	// no longer have to worry about copying function arguments into local "host"
	// variables - so I got rid of 15 duplicate variables by just moving the
	// "real" variable to the argument list instead of having an "..._in" variable
	// and copying it. The remaining "..._in" variables are ones where making this
	// change would have been somewhat more involved - so I'm leaving them alone
	// at least for the time being.
	int				LargoSold				= LargoSold_in;
	short			OP_sold					= OP_sold_in;
	short			pharm_OP_sold			= OP_sold_in;
	short			UnitsSold				= UnitsSold_in;
	short			pharm_Units_sold		= UnitsSold_in;
	int				PrevUnsoldOP_i			= PrevUnsoldOP;
	int				PrevUnsoldUnits_i		= PrevUnsoldUnits;
	int				SysDate;
	int				SysTime;
	long			CurrentYearToSecond;
	int				EarliestPrescrDate;
	int				LatestPrescrDate;
	int				DL_economypri_grp;
	int				ApplicablePkgSize;
	short			DRX_SoldOP				= 0;
	short			DRX_SoldUnits			= 0;
	short			DRX_Rx_OP				= 0;
	short			DRX_Rx_TotUnits			= 0;
	short			DRX_QtyMethod			= 0;
	short			DRX_TreatLength			= 0;
	short			DRX_PrescriptionType	= 0;	// DonR 01Aug2024 Bug #334612: No longer using this to store in pd_rx_link.
	short			Rx_OpUpdate				= 0;
	short			Rx_UnitsUpdate			= 0;
	int				Rx_OpUpdate_i			= 0;
	int				Rx_UnitsUpdate_i		= 0;
	short			SoldDrug_OP				= 0;
	short			SoldDrug_Units			= 0;
	int				SoldDrug_OP_i			= 0;
	int				SoldDrug_Units_i		= 0;
	short			Rx_status				= 0;
	short			Rounding_status			= 0;

	// DonR 04Mar2025 Bug #396579: Internal variables that get a value from multiplying
	// OP by package size should be integers, *not* shorts! It's possible for pharmacies
	// (at least non-Yarpa ones) to sell ridiculously large quantities by mistake, and
	// we saw at least one case where the total computed units was over 32,767 and
	// resulted in negative numbers in pd_rx_link.
	int				UnitsPrescribed			= 0;
	int				UnitsAlreadySold		= 0;
	int				UnitsSellable			= 0;
	int				UnitsSoldNow			= 0;	// This is the most important one to change from short to int!
	int				NewTotUnitsSold			= 0;
	int				UnitsRemaining			= 0;

	short			ClosedByCarryForward	= 0;
	short			rx_origin				= 0;	// DonR 11Dec2024 User Story #373619.
	short			RxMonthLen;
	int				OriginalCarryForwardUnits;
	int				READ_RX_SQLCODE;
	int				UPD_RX_SQLCODE;
	int				INS_PDL_SQLCODE;
	int				INS_RXSTAT_SQLCODE;
	int				RowsFound;
	short			FlagValueZero			= 0;
	short			FlagValueOne			= 1;
	TDrugListRow	DL;		// Drug being sold.
	TDrugListRow	DP;		// Drug originally prescribed.

	// Fields read from doctor_presc for updating CDS.
	int		clicks_patient_id				= 0;
	long	visit_number					= 0;
	int		visit_date						= 0;
	int		visit_time						= 0;
	int		Rx_ValidFromDate				= 0;
	int		Rx_LargoPrescribed				= 0;
	int		row_added_date					= 0;
	int		row_added_time					= 0;
	short	prev_sold_status				= 0;
	short	visit_line_number				= 0;


	// Initialization.
	SysDate				= GetDate ();
	SysTime				= GetTime ();
	EarliestPrescrDate	= AddDays (SysDate, (0 - 75));	// WORKINGPOINT: EXPAND TO 90-ISH?
	LatestPrescrDate	= AddDays (SysDate, 90);
	CurrentYearToSecond	= GetCurrentYearToSecond ();

	UseReportedUnsoldOpUnits = ((PrevUnsoldOP != 0) || (PrevUnsoldUnits != 0));

	if (CarryForwardUnits_in_out != NULL)
	{
		OriginalCarryForwardUnits	= *CarryForwardUnits_in_out;

//		ADD CARRY-FORWARD TO UNITS SOLD!

		*CarryForwardUnits_in_out	= 0;
	}
	else
	{
		OriginalCarryForwardUnits	= 0;
	}


	// In order to put CDS-friendly values into the "pass-through" table rx_status_changes,
	// read some fields from doctor_presc.
	// DonR 09Jul2015: Moved this code up a bit, and added some fields so that we can more
	// accurately determine whether the prescription has been fully sold.
	REMEMBER_ISOLATION;
	SET_ISOLATION_DIRTY;


	// DonR 07Jul2015: If necessary, retrieve the drug_list row for the drug being
	// sold from the database.
	// DonR 08Apr2018: Added new "see deleted drugs" parameter to read_drug(). Since we're not
	// validating drugs at this point (that's the responsibility of the calling routine), here
	// we just set the parameter TRUE.
	if (DL_in == NULL)
	{
		read_drug_ok = read_drug (LargoSold_in, 999999999, NULL, true, &DL, NULL);
		DL_in = &DL;

		// DonR 01Nov2020: Add logging of any unexpected DB error reading drug_list.
		if (!read_drug_ok)
		{
			if (SQLERR_code_cmp (SQLERR_not_found) != MAC_TRUE)
			{
				SQLERR_error_test ();
			}
		}
	}
	else
	{
		memcpy (&DL, DL_in, sizeof (DL));
	}

	// If the drug being sold (or deleted) is not the same as the drug that was originally
	// prescribed, read the original drug's package size from the database. This way we
	// can be sure that the values for OP/Units Sold are accurate in terms of the drug
	// prescribed, even if something with a different package size was sold. (Note that
	// at least for now, we aren't worrying about different dosages!)
	// DonR 08Apr2018: Added new "see deleted drugs" parameter to read_drug(). Since we're not
	// validating drugs at this point (that's the responsibility of the calling routine), here
	// we just set the parameter TRUE.
	if ((LargoSold != LargoPrescribed) && (LargoPrescribed > 0))
	{
		// Just in case we couldn't read the prescribed drug, default to using the same drug info.
		if (!read_drug (LargoPrescribed, 999999999, NULL, true, &DP, NULL))
		{
			DP = DL;

			// DonR 01Nov2020: Add logging of any unexpected DB error reading drug_list.
			if (SQLERR_code_cmp (SQLERR_not_found) != MAC_TRUE)
			{
				SQLERR_error_test ();
			}
		}
	}
	else
	{
		// Drug sold is the one the doctor prescribed - copy the structure for convenience.
		DP = DL;
	}

	// Copy the Economypri Group Code of the SOLD drug to a simple variable for SQL below.
	// (Why the sold drug and not the supposedly-prescribed drug? Because we need this
	// variable only if we believe that the pharmacy sent a bogus Original Largo Code.
	DL_economypri_grp	= DL.economypri_group;


	// DonR 19Aug2015: When drug sales are processed normally through Transaction 5003 or 6003,
	// the relevant routine already performs flexible lookups to match the drug being sold with
	// the correct doctor prescription. However, the same kind of logic also needs to be applied
	// to transactions arriving via 5005/6005 spool, since pharmacies do not always provide us
	// with accurate Prescription Date or Original Largo Code data. (We still count on
	// receiving an accurate Doctor Prescription ID, however - there is no practical alternative
	// to this.) In fact, we should perform flexible lookups even after Trn. 5003/6003, since
	// sometimes the doctor prescription arrives from CDS just after the member has appeared
	// at the pharmacy; it can happen that a "missing" prescription at 5003/6003-time is no longer
	// missing at 5005/6005-time.
	//
	// DonR 19Aug2015: If the pharmacy has sent a Doctor Prescription ID of zero, don't bother
	// trying to read from doctor_presc - it just creates extra work for the database server.
	//
	// DonR 12Dec2018 CR #15277: Add logic such that (A) if the pharmacy supplied a Doctor License
	// Number instead of a Doctor TZ, that part of the WHERE clause will be disabled; and (B) we
	// now read the actual Doctor TZ from doctor_presc. Because of the slightly less specific
	// WHERE clause in this case, I changed the SELECT to a SELECT FIRST 1, with an ORDER BY
	// that will give us the most recently issued prescription in the unlikely case that more
	// than one match the other selection criteria.
	// DonR 01Aug2023 User Story #469361: Read prescription_type from doctor_presc and add it
	// to pd_rx_link.
	// DonR 11Dec2024 User Story #373619: Add rx_origin to both queries that read doctor_presc.
	if (DocPrID < 1)
	{
		FORCE_NOT_FOUND;	// Force a "not found" result.
		if (DocPrID < 0)	// I.e. a really strange value - zero is too common to bother logging.
			GerrLogMini (GerrId, "update_doctor_presc: Invalid DocPrID = %d for PrID %d/%d.", DocPrID, v_RecipeIdentifier, LineNumber);
	}

	else
	{	// Pharmacy did supply a Doctor Prescription ID, so try the lookup...
		// ...except that if pharmacy didn't supply a possibly-valid Prescription Date, fall
		// through to the second SQL without bothering to look up by the specific date.
		if (ValidFromDate < 20150101)
		{
			FORCE_NOT_FOUND;	// Force a "not found" result.
//			GerrLogMini (GerrId, "update_doctor_presc: ValidFromDate = %d, disabling first DB lookup, SQLCODE = %d.", ValidFromDate, SQLCODE);
		}
		else
		{
			ExecSQL (	MAIN_DB, UpdateDoctorPresc_READ_Rx_by_Largo_Prescribed,
						&clicks_patient_id,		&prev_sold_status,		&visit_number,		&visit_date,
						&visit_time,			&row_added_date,		&row_added_time,	&visit_line_number,
						&Rx_ValidFromDate,		&Rx_LargoPrescribed,	&DRX_SoldUnits,		&DRX_SoldOP,
						&DRX_QtyMethod,			&DRX_Rx_OP,				&DRX_Rx_TotUnits,	&DRX_TreatLength,
						&DocID,					&DRX_PrescriptionType,	&rx_origin,

						&v_MemberID,			&DocPrID,				&LargoPrescribed,	&ValidFromDate,
						&ValidFromDate,			&DocID,					&DocIDType,			&v_IDCode,
						&ActionCode,			&ActionCode,			END_OF_ARG_LIST								);

//			if (SQLCODE != 0)
//				GerrLogMini (GerrId, "update_doctor_presc: First lookup failed - SQLCODE = %d.", SQLCODE);
		} // Pharmacy did supply a possibly-valid Prescription Date.

		// DonR 19Aug2015: Add logic to try reading the original doctor prescription if
		// the pharmacy has sent bogus data for Prescription Date and/or Original Largo Code.
		if (SQLERR_code_cmp (SQLERR_not_found) == MAC_TRUE)
		{
			ExecSQL (	MAIN_DB, UpdateDoctorPresc_READ_Rx_by_Largo_Prescribed,
						&clicks_patient_id,		&prev_sold_status,		&visit_number,			&visit_date,
						&visit_time,			&row_added_date,		&row_added_time,		&visit_line_number,
						&Rx_ValidFromDate,		&Rx_LargoPrescribed,	&DRX_SoldUnits,			&DRX_SoldOP,
						&DRX_QtyMethod,			&DRX_Rx_OP,				&DRX_Rx_TotUnits,		&DRX_TreatLength,
						&DocID,					&DRX_PrescriptionType,	&rx_origin,

						&v_MemberID,			&DocPrID,				&LargoPrescribed,		&EarliestPrescrDate,
						&LatestPrescrDate,		&DocID,					&DocIDType,				&v_IDCode,
						&ActionCode,			&ActionCode,			END_OF_ARG_LIST									);

//			if (SQLCODE != 0)
//				GerrLogMini (GerrId, "update_doctor_presc: Second lookup failed - SQLCODE = %d.", SQLCODE);
		}	// Initial lookup by specific Prescription Date and Largo Prescribed failed.


		// DonR 19Aug2015: If we didn't find a matching doctor prescription even with a date range, it's possible
		// that the pharmacy sent an invalid Doctor Largo Code (due to a pharmacy-side bug, or because the
		// prescription wasn't delivered electronically and they had to scan a printed prescription sheet).
		// In this case, perform the same lookups, but using the economypri table to include all drugs in the
		// same substitution group. (It's possible that this will result in "cardinality" errors because a
		// doctor has prescribed two different drugs from the same group - but this should happen very
		// infrequently in real life.) Pre-qualify the Group Code so we don't do the lookup if we know we
		// won't find anything; also, perform the lookup only for treatment-type drugs (Largo Type B, M, and T).
		if ((SQLERR_code_cmp (SQLERR_not_found) == MAC_TRUE)									&&
			((DL.largo_type == 'B') || (DL.largo_type == 'M') || (DL.largo_type == 'T'))		&&
			(DL_economypri_grp >= 1) && (DL_economypri_grp <= 999))		// DonR 05Nov2014 - 800->1000
		{
			// If pharmacy didn't supply a possibly-valid Prescription Date, fall through to the second
			// SQL without bothering to look up by the specific date.
			if (ValidFromDate < 20150101)
			{
				FORCE_NOT_FOUND;	// Force a "not found" result.
//				GerrLogMini (GerrId, "update_doctor_presc: ValidFromDate = %d, disabling third DB lookup, SQLCODE = %d.", ValidFromDate, SQLCODE);
			}
			else
			{
				ExecSQL (	MAIN_DB, UpdateDoctorPresc_READ_Rx_by_Generic_Group,
							&clicks_patient_id,		&prev_sold_status,		&visit_number,			&visit_date,
							&visit_time,			&row_added_date,		&row_added_time,		&visit_line_number,
							&Rx_ValidFromDate,		&Rx_LargoPrescribed,	&DRX_SoldUnits,			&DRX_SoldOP,
							&DRX_QtyMethod,			&DRX_Rx_OP,				&DRX_Rx_TotUnits,		&DRX_TreatLength,
							&DocID,					&DRX_PrescriptionType,	&rx_origin,

							&v_MemberID,			&DocPrID,				&ValidFromDate,			&ValidFromDate,
							&DL_economypri_grp,		&DocID,					&DocIDType,				&v_IDCode,
							&ActionCode,			&ActionCode,			END_OF_ARG_LIST									);

//				if (SQLCODE != 0)
//					GerrLogMini (GerrId, "update_doctor_presc: Third lookup failed - SQLCODE = %d.", SQLCODE);
			} // Pharmacy did supply a possibly-valid Prescription Date.

			// If broadening the Largo Prescribed to include other drugs in the Economypri group wasn't
			// enough to find the prescription, try broadening the Prescription Date as well.
			// Note that the SQL above (using Economypri with a single date) can fail with "not found"
			// OR with "more than one found"; in the latter case there is no point in trying again with
			// a date range, since we'll just hit the same error.
			if (SQLERR_code_cmp (SQLERR_not_found) == MAC_TRUE)
			{
				ExecSQL (	MAIN_DB, UpdateDoctorPresc_READ_Rx_by_Generic_Group,
							&clicks_patient_id,		&prev_sold_status,		&visit_number,			&visit_date,
							&visit_time,			&row_added_date,		&row_added_time,		&visit_line_number,
							&Rx_ValidFromDate,		&Rx_LargoPrescribed,	&DRX_SoldUnits,			&DRX_SoldOP,
							&DRX_QtyMethod,			&DRX_Rx_OP,				&DRX_Rx_TotUnits,		&DRX_TreatLength,
							&DocID,					&DRX_PrescriptionType,	&rx_origin,

							&v_MemberID,			&DocPrID,				&EarliestPrescrDate,	&LatestPrescrDate,
							&DL_economypri_grp,		&DocID,					&DocIDType,				&v_IDCode,
							&ActionCode,			&ActionCode,			END_OF_ARG_LIST									);

//				if (SQLCODE != 0)
//					GerrLogMini (GerrId, "update_doctor_presc: Fourth lookup failed - SQLCODE = %d.", SQLCODE);
			}	// Reading doctor_presc with exact date failed, so try with date range AND Economypri group.
		}	// Lookups by specific Largo Prescribed failed, so try based on Economypri group.
	} // Pharmacy supplied a non-zero Doctor Prescription ID.


	// No real error-handling for this SELECT - but (at least in spool mode) we
	// gave it a good try!
	READ_RX_SQLCODE			= SQLCODE;
	ForceArrivalQProcessing	= 0;	// Re-initialize just to be paranoid.

//	if (READ_RX_SQLCODE != 0)
//		GerrLogMini (GerrId, "update_doctor_presc: All lookups failed - READ_RX_SQLCODE = %d.", READ_RX_SQLCODE);


	// DonR 18Oct2017: If the current transaction is a deletion *and* we read a doctor prescription, check
	// to see whether the original sale is on the Doctor Prescription Late Arrival Queue. It's possible
	// for the prescription to arrive between the sale and the deletion; and it's important that the updates
	// for these two events be performed in the proper sequence: sale first, then deletion. If the sale
	// is on the queue, force an update of that particular queue entry before proceeding to update the
	// prescription with the deletion information. Note, BTW, that this logic operates only if
	// LateArrivalMode_in is zero - so we're not in any danger of falling into recursion loops!
	if ((ActionCode == 2) && (READ_RX_SQLCODE == 0) && (!LateArrivalMode_in))
	{
		// See if the relevant item is on the queue. The final bit of the WHERE clause (selecting by
		// Doctor ID) is almost certainly unnecessary, but it's harmless.
//		// DonR 26Dec2017: Changed the SELECT to read quantity info, not just get a row count.
//		// Because we read the doctor prescription in a state that did *not* reflect the sale
//		// that is now being deleted, we need to adjust the prescription's TOT_OP_SOLD and
//		// TOT_UNITS_SOLD values based on the Sold OP/Units number is the rx_late_arrival_q
//		// row. Note that although there really shouldn't be more than one applicable unprocessed
//		// row in rx_late_arrival_q, we use a SELECT FIRST 1 with an ORDER BY just to make sure
//		// we don't get cardinality violations.
		ExecSQL (	MAIN_DB, UpdateDoctorPresc_READ_CountLateArrivalQ,
					&RowsFound,
					&DeletedPrescID, &LargoPrescribed, &DocPrID, &DocID, END_OF_ARG_LIST	);

		// For now at least, don't bother with any real error-handling on this SELECT.
		// Usually, nothing will be found (meaning that the doctor prescription has
		// already been updated based on the sale that's now being deleted). If we
		// did find something on the queue, we'll put the deletion information on the
		// queue as well, then force immediate queue processing. This approach should
		// (I hope!) guarantee correct quantity and status updating.
		if ((SQLCODE == 0) && (RowsFound > 0))
		{
			READ_RX_SQLCODE			= SQLERR_NO_MORE_DATA_OR_NOT_FOUND;
			ForceArrivalQProcessing	= 1;
		}	// There was a late-arrival queue item to process.

//		{
//			// If processing the late-arrival queue item for the original drug sale fails for
//			// some reason, suppress the update of the doctor prescription for now, and add
//			// the deletion information to the late-arrival queue (in the relevant logic
//			// down below). Note that 1 = success, -1 = some kind of error, and 0 = nothing processed.
//			if (ProcessRxLateArrivalQueue (DeletedPrescID, LargoPrescribed, DocPrID, DocID) < 1)
//			{
//				READ_RX_SQLCODE = SQLERR_NO_MORE_DATA_OR_NOT_FOUND;
//			}	// ProcessRxLateArrivalQueue() was unable to process the queue item.
//
//			else
//			{
//				DRX_SoldUnits	+= Q_SoldUnits;
//				DRX_SoldOP		+= Q_SoldOP;
//			}	// ProcessRxLateArrivalQueue() succeeded in updating the doctor prescription
//				// with the queued drug-sale information.

	}	// Need to check if the sale to be deleted occurred before the doctor prescription arrived.


	// If we didn't find a doctor prescription, clear variables and write to queue in "normal"
	// mode, or just exit in late-arrival mode.
	if (READ_RX_SQLCODE != 0)
	{
		// DonR 01Oct2017: If we're in late-arrival mode and we still don't see a doctor prescription
		// matching the sale, there's nothing left to do - just exit with a return code of -1 to
		// indicate to the calling routine that we didn't do anything and the calling parameters
		// should be left in the queue table (rx_late_arrival_q). OTOH, if we're in "normal" mode
		// we want to add this function's input parameters to the queue table so that late-arriving
		// doctor prescriptions can be marked as "sold" shortly after they appear in the database.
		if (LateArrivalMode_in)
		{
			// Late-arrival mode - just return a "nothing found" result.
			RESTORE_ISOLATION;
			return (-1);
		}

		else
		{	// "Normal" mode: clear variables, write input parameters to late-arrival queue,
			// and continue processing.

			// Make sure we don't have bogus values for DB fields.
			clicks_patient_id		=
			prev_sold_status		=
			visit_number			=
			visit_date				=
			visit_time				=
			row_added_date			=
			row_added_time			=
			visit_line_number		=
			Rx_ValidFromDate		=
			Rx_LargoPrescribed		=
			UnitsSellable			=
			UnitsAlreadySold		=
			UnitsRemaining			=
			NewTotUnitsSold			=
			rx_origin				=
			DRX_SoldUnits			=
			DRX_SoldOP				=
			DRX_QtyMethod			=
			DRX_Rx_OP				=
			DRX_TreatLength			=
			DRX_Rx_TotUnits			=
			DRX_PrescriptionType	=	0;

			// Write input parameters to rx_late_arrival_q, then continue with the rest of the function.
			// If the input Doctor Prescription ID is less than 1, don't bother - since it will never
			// arrive by definition, there's no point waiting for it!
			if (DocPrID > 0)
			{
				for (tries = 0; tries < 3; tries++)
				{
					if (tries > 0)
						usleep (500000);	// Wait for 500 milliseconds. Note that this means a total potential
											// wait time of one second - but this shouldn't happen in real life.
					ExecSQL (	MAIN_DB, UpdateDoctorPresc_INS_LateArrivalQ,
								&ActionCode,			&v_MemberID,			&v_IDCode,
								&v_RecipeIdentifier,	&LineNumber,
								&DocID,					&DocIDType,
								&DocPrID,				&LargoPrescribed,		&LargoSold,
								&ValidFromDate,			&SoldDate,				&SoldTime,
								&OP_sold,				&UnitsSold,				&PrevUnsoldOP,
								&PrevUnsoldUnits,		&CurrentYearToSecond,	&CurrentYearToSecond,
								END_OF_ARG_LIST															);

					// If there was an access-conflict error, keep looping; otherwise (i.e. success or
					// "hard failure") break out of the loop.
					if (SQLERR_code_cmp (SQLERR_access_conflict) != MAC_TRUE)
					{
						tries++;	// For human-friendly error logging.
						break;
					}
				}	// Retry loop for rx_late_arrival_q INSERT.

				// Other than making two retry attempts, we're not reacting to errors on the SQL INSERT except
				// for writing a message to the log. If we see a lot of errors in real life, this will have to
				// be revisited - but it's very, very unlikely to happen.
				if (SQLCODE != 0)
				{
					GerrLogMini (GerrId, "update_doctor_presc: INSERT to rx_late_arrival_q failed - SQLCODE = %d after %d attempts.", SQLCODE, tries);
				}
				else
				{
					if (tries > 1)
						GerrLogMini (GerrId, "update_doctor_presc: INSERT to rx_late_arrival_q succeeded after %d attempts.", tries);
				}
			}	// Doctor Prescription ID is greater than zero, so the prescription is worth looking for.

		}	// "Normal" mode - need to INSERT function parameters into rx_late_arrival_q so
			// we can check for late-arriving doctor prescriptions.

	}	// We were NOT able to read a doctor prescription for this sale.


	// If we retrieved the correct Valid-From Date from doctor_presc, store it in the "real" variable.
	if (Rx_ValidFromDate > 0)
		ValidFromDate = Rx_ValidFromDate;


	// If we get here and Rx_LargoPrescribed has a non-zero value, this is the correct Largo Prescribed
	// (which may be different from what the pharmacy reported). In this case, read the "new" prescribed
	// drug so we know its package size, etc.
	if (Rx_LargoPrescribed > 0)
	{
		if (Rx_LargoPrescribed == LargoSold)
		{
			DP = DL;
		}
		else
		{
			if (Rx_LargoPrescribed != DP.largo_code)
			{
				// Just in case we couldn't read the prescribed drug, default to using the sold drug's info.
				// DonR 08Apr2018: Added new "see deleted drugs" parameter to read_drug(). Since we're not
				// validating drugs at this point (that's the responsibility of the calling routine), here
				// we just set the parameter TRUE.
				if (!read_drug (Rx_LargoPrescribed, 999999999, NULL, true, &DP, NULL))
				{
					DP = DL;

					// DonR 01Nov2020: Add logging of any unexpected DB error reading drug_list.
					if (SQLERR_code_cmp (SQLERR_not_found) != MAC_TRUE)
					{
						SQLERR_error_test ();
					}
				}
			}
		}	// Rx_LargoPrescribed != LargoSold

		LargoPrescribed = Rx_LargoPrescribed;	// Needed for updating doctor_presc.
	}	// We retrieved the correct Prescribed Largo from doctor_presc.

	// GerrLogMini (GerrId, "Prescribed Largo %d, calc OP by volume %d; sold Largo %d, calc OP %d.", DP.largo_code, DP.calc_op_by_volume, DL.largo_code, DL.calc_op_by_volume);

	// DonR 21Nov2017 CR #13341: If this is a sale of a different Largo Code than what the doctor prescribed
	// that is *not* in the same Generic Substitution Group, we will mark the prescription as fully sold
	// *regardless* of the actual quantity that was sold. This is because a decision was made not to try to
	// convert pills of different dosages to a matching quantity of pills of the original dose.
	ForceRxFullySold =	((LargoSold				!= LargoPrescribed)										&&
						 ((DL.economypri_group	!= DP.economypri_group) || (DL.economypri_group < 1)));
	// (I.e. if the drug sold is exactly what was prescribed OR the two drugs are in the same non-zero
	// substitution group, ForceRxFullySold will be zero and we'll work normally.)


	// If (and only if) the calling routine isn't giving us reliable numbers for
	// OP/Units Already Sold, recalculate these numbers from the Doctor Prescriptions
	// table. If we didn't find a doctor prescription to update, then don't bother.
	// DonR 05May2016: Since we're now reading the doctor_presc table unconditionally,
	// there's no longer any good reason to use the "trust previous sold amounts"
	// option - so I've eliminated the parameter from this function, which now
	// (quite properly) trusts nothing and nobody.
	// DonR 18Mar2018 CR #14502: Use the new "effective package volume" value instead
	// of the original package size. Normally, this is the same; but for syrups, creams,
	// and similar things where we need to calculate OP by volume, the effective package
	// size will be the integer equivalent of the package volume. (The "normal" package
	// size for these items is 1 - one bottle, one tube, etc.)
	if (READ_RX_SQLCODE == 0)
	{
		// Recalculate quantities available for sale from this prescription.
		// First, process the original prescription; we need to use *either* prescribed Total Units
		// or prescribed OP, but *not* both - depending on the Quantity Method used.
		// DonR 22Mar2018 CR #14502: In Production, essentially all prescriptions for syrups
		// and creams have Quantity Method = 1; but in QA we waw a prescription where this
		// was not the case. Just to make sure, force prescriptions to work in OP when
		// the Largo Prescribed has "Calculate OP by Volume" set TRUE.
		if ((DRX_QtyMethod == 1) || (DP.calc_op_by_volume > 0))
			DRX_Rx_TotUnits	= 0;	// Doctor specified OP - so ignore units.
		else
			DRX_Rx_OP		= 0;	// Doctor did not specify OP - so use units and ignore OP.

		UnitsPrescribed		= (DRX_Rx_OP	* DP.effective_package_size) + DRX_Rx_TotUnits;

		// Next, compute the units already sold, and calculate how much remains sellable.
		UnitsAlreadySold	= (DRX_SoldOP	* DP.effective_package_size) + DRX_SoldUnits;

		UnitsSellable		= (UnitsAlreadySold > UnitsPrescribed) ? 0 : UnitsPrescribed - UnitsAlreadySold;
		// GerrLogMini (GerrId, "UnitsPrescribed = %d, UnitsAlreadySold = %d, UnitsSellable = %d.", UnitsPrescribed, UnitsAlreadySold, UnitsSellable);

		// Note that the calculations above are all in terms of the drug prescribed (stored in
		// structure DP). But the sellable units/OP sent to pharmacy are in terms of the drug
		// to be *sold*, which may have a different package size - so in the next calculation,
		// we need to use the drug-sold structure DL.
		// DonR 10Apr2018: If the pharmacy reported at least one non-zero number for previously-unsold OP/Units,
		// we want to save those to pd_rx_link and RK9122ACP.
		if (!UseReportedUnsoldOpUnits)
		{
			PrevUnsoldOP	= UnitsSellable / DL.effective_package_size;
			PrevUnsoldUnits	= UnitsSellable % DL.effective_package_size;
			// GerrLogMini (GerrId, "At first: PrevUnsoldOP = %d, PrevUnsoldUnits = %d.", PrevUnsoldOP, PrevUnsoldUnits);

			// DonR 22Mar2018 CR #14502: If we're calculating OP by volume, even a small
			// remainder means we need to sell an additional bottle/tube.
			if ((DL.calc_op_by_volume > 0) && (PrevUnsoldUnits > 0))
			{
				PrevUnsoldOP++;
				PrevUnsoldUnits = 0;
				// GerrLogMini (GerrId, "Corrected: PrevUnsoldOP = %d, PrevUnsoldUnits = %d.", PrevUnsoldOP, PrevUnsoldUnits);
			}
		}	// Pharmacy did *not* report non-zero previously-unsold OP/Units.
	}	// We did manage to read the doctor prescription.


	RESTORE_ISOLATION;


	// DonR 01Nov2018 WORKING NOTE: We need a better system to trigger "parsimonious prescription
	// closing". The main "round to close" logic is OK, but it's still sensitive to the allocations
	// of units/OP sent by pharmacy. In the case of one drug line for multiple presriptions, this
	// can be problematic.
	// Part of the solution will be to "look ahead": before calling this routine to update individual
	// prescriptions, the calling routine should check the total amount prescribed for all relevant
	// prescriptions (and the total amount unsold) against the total amount being sold now. If the
	// current sale is going to be less than required to supply the entire amount prescribed, that
	// means that we *may* want to use "parsimonious" logic. 
	// Condition 1: Fewer units will be sold than the theoretical full amount prescribed.
	// Condition 2: Package size sold == 28.
	// Condition 3: Only full packages are being sold - no units.
	// If these general conditions are met, we can look at each prescription individually. Is
	// it monthly? Were any previously-sold amounts monthly? If so, then we can work with the
	// prescription in "monthly mode".
	//
	// Possibly add to doctor_presc table: unit_months_in_rx / unit_months_sold, both SMALLINT.
	// These would be used to provide an "alternate math", where we just add and subtract
	// unit-months and don't worry about actual units. The logic would be a lot simpler...
	//
	// 1) Create a new routine to analyze prescriptions. If the prescription is new (sold_status = 0)
	//    AND it's monthly (based on duration divisible by 28/29/30/31) AND the total units MOD
	//    month length = 0, set unit_months_in_rx to the correct number; otherwise set to -1 to
	//    indicate that this is *not* a monthly prescription, or has had non-monthly partial sales.
	//    This routine would also return the nominal number of unsold units.
	// 2) Before calling update_doctor_presc(), the calling routine would loop through prescriptions,
	//    determine their monthly status, and get a total nominal amount unsold.
	// 3) IF the total nominal amount unsold is greater than the total units being sold now, AND
	//    the current sale consists only of full OP's, then we go into "parsimonious mode" and
	//    do our computations on a unit-month basis rather than on a unit basis.


	// DonR 09Apr2018: If we're in multiple-prescription mode AND rounding conditions apply AND
	// the drug being sold comes in packages of 28 pills, we need to recalculate the quantity
	// of the drug being sold that is allocated to the current prescription we're updating.
	// If the drug sold comes in packages of 30 (there don't seem to be any drugs that come
	// in packages of 29 or 31 pills), we can just use the numbers sent by the pharmacy.
	if (
		(RoundPackagesToCloseRx)														&&	// From sysparams/round_pkgs_sold.
		(DL.openable_pkg)																&&	// No rounding takes place for non-openable packages.
		(ActionCode == DRUG_SALE)														&&	// This is a drug sale rather than a deletion.
		(!ThisIsTheLastRx_in)															&&	// NOT the last prescription in the list.
		(!ForceRxFullySold)																&&	// DonR 21Nov2017 CR #13341: If we're forcing a full sale, there's no point in all this logic.
		(DL.package_size	== 28)														&&	// Drug comes in a "monthly" package of 28 units.
		(DL.largo_type		== 'T')														&&	// This is a "treatment" (i.e. a normal drug).

		// Check whether Treatment Length is a monthly multiple.
		(DRX_TreatLength > 0)															&&	// Since zero MOD anything is zero!
		(	((DRX_TreatLength % 28) == 0) ||
			((DRX_TreatLength % 29) == 0) ||
			((DRX_TreatLength % 30) == 0) ||
			((DRX_TreatLength % 31) == 0)		)										&&	// Prescription is for one or more months.

		// Check whether the amount originally prescribed comes out to a "monthly" number of units.
		(DRX_Rx_TotUnits > 0)															&&	// Again, since zero MOD anything is zero.
		(	((DRX_Rx_TotUnits % 28) == 0) ||
			((DRX_Rx_TotUnits % 29) == 0) ||
			((DRX_Rx_TotUnits % 30) == 0) ||
			((DRX_Rx_TotUnits % 31) == 0)		)										&&	// Original amount prescribed doesn't require opening a package.

		(CarryForwardUnits_in_out	!= NULL)
	   )
	{
		EffectiveRxUnits	= DRX_Rx_TotUnits - (DRX_Rx_TotUnits % 28);			// Minimum units needed assuming 28 pills/month.
		UnitsNeededToClose	= EffectiveRxUnits - UnitsAlreadySold;				// ...net of what's already been sold in the past.
		UnitsSoldNow		= (OP_sold_in * DL.package_size) + UnitsSold_in;	// What pharmacy sent.

		// If the pharmacy allocated more than enough units to close this prescription, adjust
		// the allocation and tell the calling routine that there are available units to add
		// to the next prescription in the list.
		if (UnitsSoldNow > UnitsNeededToClose)
		{
			*CarryForwardUnits_in_out	= UnitsSoldNow - UnitsNeededToClose;
			OP_sold_in					= UnitsNeededToClose / DL.package_size;
			UnitsSold_in				= UnitsNeededToClose % DL.package_size;
		}
		else
		{
			*CarryForwardUnits_in_out = 0;
		}
	}	// Need to adjust allocation of drugs sold to the current prescription.
	else
	{
		// Just to be paranoid, set CarryForwardUits_out to zero - even though we did it at the
		// beginning of the function.
		if (CarryForwardUnits_in_out != NULL)
			*CarryForwardUnits_in_out = 0;
	}


	// Determine the new sold-status of the doctor prescription.
	// DonR 10May2017: Store the applicable package size of the incoming OP/Units sold numbers
	// in a variable to be used later. This is required because for deletions, the calling
	// routine is *already* getting the OP/Units to delete for this prescription in terms of
	// the prescribed drug, and we were double-translating - leading to "over-deleted" prescriptions.
	if (ActionCode == SALE_DELETION)
	{
		Rx_OpUpdate			= 0 - OP_sold_in;				// Pharmacy gives positive numbers only.
		Rx_UnitsUpdate		= 0 - UnitsSold_in;				// Pharmacy gives positive numbers only.
		ApplicablePkgSize	= DP.effective_package_size;	// Deletions will have OP/Units deleted *already*
															// in terms of original Largo prescribed.
	}
	else
	{
		// This is a drug sale.
		Rx_OpUpdate			= OP_sold_in;
		Rx_UnitsUpdate		= UnitsSold_in;
		ApplicablePkgSize	= DL.effective_package_size;	// Sales will have OP/Units sold in terms
															// of Largo being sold.
// GerrLogMini (GerrId, "Rx_OpUpdate = %d, Rx_UnitsUpdate = %d, ApplicablePkgSize = %d.", Rx_OpUpdate, Rx_UnitsUpdate, ApplicablePkgSize);
	}


	// Set doctor_presc status: 1 = partially filled, 2 = completely filled.
	// First calculate amount that was sellable and amount that was
	// actually sold. Note that the "Previously Unsold" values are what was
	// sent in Transaction 2001/6001, and thus should reflect the package size
	// to be sold, *not* necessarily the package size of the drug prescribed.
	// DonR 09Jul2015: In Transaction 6001, we are now automatically rounding
	// to even OP for monthly prescriptions. This means that for the 6000-series
	// of transactions, we don't have to do any even-unit rounding here - since
	// the "previously-unsold" figure *already* represents the number of even OP
	// needed to complete the sale based on "rounding".
	// DonR 18May2017: In some cases, pharmacies do in fact open packages even
	// though the drug in question is flagged as non-openable. In this case we
	// need to accept the fact that the package was opened in order to do accurate
	// accounting.
	// DonR 22Mar2018 CR #14502: Treat "calculate OP by volume" drugs as if they
	// were in openable packages, since we need to calculate in volume units.
	// DonR 01May2019 BUG FIX: In certain cases - e.g. the prescribed drug has
	// package size 30 and the sold/deleted drug has package size 10 - the
	// "effective open package" logic didn't work and we discarded units that
	// we should not have discarded. Using ApplicablePkgSize should fix this.
// GerrLogMini (GerrId, "DL.openable_pkg = %d,	DL.calc_op_by_volume = %d.", DL.openable_pkg, DL.calc_op_by_volume);
	if ((DL.openable_pkg)				||
		(DL.calc_op_by_volume > 0)		||
		((Rx_UnitsUpdate % ApplicablePkgSize) != 0))	// Package is openable in theory OR was opened in practice.
//		((Rx_UnitsUpdate % DL.package_size) != 0))	// Package is openable in theory
//													// OR was opened in practice.
	{
		UnitsSoldNow	= (Rx_OpUpdate		* ApplicablePkgSize)	+  Rx_UnitsUpdate;
		UnitsRemaining	= UnitsSellable		- UnitsSoldNow;
		NewTotUnitsSold	= UnitsAlreadySold	+ UnitsSoldNow;
	}
	else
	{
		UnitsSoldNow	= Rx_OpUpdate	* ApplicablePkgSize;
		UnitsRemaining	= NewTotUnitsSold = 0;	// No rounding is needed for non-openable,
												// non-opened packages.
	}
// GerrLogMini (GerrId, "UnitsSoldNow = %d, UnitsRemaining = %d, NewTotUnitsSold = %d.", UnitsSoldNow, UnitsRemaining, NewTotUnitsSold);


	// DonR 04May2016: If the doctor prescription meets the criteria to be considered
	// "monthly" and the remaining sellable units would normally be rounded down such
	// that a subsequent Transaction 6001 response would tell the pharmacy to sell
	// zero OP, consider the sale complete.
	// DonR 21Oct2018: At Iris Shaya's request, making some revisions to the prescription-closing
	// logic:
	// A) Don't bother checking if the drug sold comes in an openable package.
	// B) Support 2- and 3-month packages (~60 and ~90 units/OP).
	// C) The maximum package portion is now a parameter rather than hard-coded at half an OP.
	// D) The maximum average-number-of-pills discrepancy is now also a parameter, instead of being
	//    hard-coded at 3.0 per 28 pills.
	// E) Enable this logic for Largo Type 'M' as well as 'T'.
	// F) Add a "closed based on rounding" status.
	// G) Perform the rounding logic even if the global RoundPackagesToCloseRx parameter is FALSE.
	//    In this case, we will set the "closed based on rounding" status TRUE, but set the "sold"
	//    status according to the regular logic. This will allow testing of the rounding logic
	//    without actually changing how prescriptions are handled in Production.

	Rounding_status = 0;	// Paranoid re-initialization.
	RxMonthLen		= ((DRX_TreatLength > 0) && ((DRX_TreatLength % 31) == 0)) ? 31 : 30;	// Almost always 30.

	// DonR 24Oct2018: Instead of looking at the remainder in terms of package size,
	// just look at the total prescribed units versus the total supplied units. If
	// the total units supplied is going to be at least 28/30 of the amount prescribed,
	// that means we're meeting the "28-days-per-month" test and we can "round to close"
	// the prescription.
	if (
		// 1) General conditions.
//		(RoundPackagesToCloseRx)														&&	// DISABLED HERE - moved below. (From sysparams/round_pkgs_sold.)
//		(DL.openable_pkg)																&&	// DISABLED (No rounding takes place for non-openable packages.)
//		(MACCABI_PHARMACY)																&&	// DISABLED (14Apr2016: This rounding method is for Maccabi pharmacies only.)
		(ActionCode		== DRUG_SALE)													&&	// This is a drug sale rather than a deletion.
		((UnitsAlreadySold + UnitsSoldNow) <  UnitsPrescribed)							&&	// No point in rounding up if we already know we're selling all that was prescribed.
		((UnitsAlreadySold + UnitsSoldNow) >= ((UnitsPrescribed * 28) / RxMonthLen))	&&	// 24Oct2018: Look at total amount sold instead of package portion remaining.
//		(UnitsSoldNow	<  UnitsSellable)												&&	// No point in rounding up if we already know we're selling all that was prescribed.
//		((UnitsSellable - UnitsSoldNow) < DL.package_size)								&&	// Amount remaining to sell is less than a full package.
//		(UnitsRemaining <  (DL.package_size * MaxUnsoldOpPercent / 100))				&&	// Remaining units to sell are less than the maximum portion of a package.
//		(UnitsRemaining < (DL.package_size / 2))										&&	// OLD VERSION (Remaining units to sell are less than half a package.)
		(!ForceRxFullySold)																&&	// DonR 21Nov2017 CR #13341: If we're forcing a full sale, there's no point in all this logic.

		// 2) Check whether the drug being sold is appropriate for "rounding".
		//    DonR 21Oct2018: Added bimonthly and trimonthly package logic.
		(((DL.package_size	>= 28) && (DL.package_size	<= 31))	||							// Drug comes in a "monthly" package, or...
		 ((DL.package_size	>= 56) && (DL.package_size	<= 62))	||							// Drug comes in a "bimonthly" package, or...
		 ((DL.package_size	>= 84) && (DL.package_size	<= 93)))						&&	// Drug comes in a "trimonthly" package.
//		(DL.package_size	>= 28)														&&	// Drug comes in a "monthly" package.
//		(DL.package_size	<= 31)														&&
		((DL.largo_type		== 'T') || (DL.largo_type == 'M'))							&&	// This is a "treatment" (i.e. a normal drug). (Added 'M' 21Oct2018.)

		// 3) Check whether the prescription has a "monthly-type" duration.
		(DRX_TreatLength > 0)															&&	// Since zero MOD anything is zero!
		(	((DRX_TreatLength % 28) == 0) ||
			((DRX_TreatLength % 29) == 0) ||
			((DRX_TreatLength % 30) == 0) ||
			((DRX_TreatLength % 31) == 0)		)										&&	// Prescription is for one or more months.

		// 4) Check whether the amount originally prescribed comes out to a "monthly" number of units.
		(DRX_Rx_TotUnits > 0)															&&	// Again, since zero MOD anything is zero.
		(	((DRX_Rx_TotUnits % 28) == 0) ||
			((DRX_Rx_TotUnits % 29) == 0) ||
			((DRX_Rx_TotUnits % 30) == 0) ||
			((DRX_Rx_TotUnits % 31) == 0)		)										&&	// Original amount prescribed doesn't require opening a package.

		// 5) Check whether the new total amount sold comes out to a "monthly" number of units.
		//    (Note that in this case we don't have to worry about testing for a non-zero value!)
		//    If our modulo checks don't work (e.g. if member has previously bought one package
		//    with 29 pills and one with 30 pills), we also check based on a floating-point
		//    calculation; this shouldn't execute if NewTotUnitsSold is zero (since the first
		//    modulo test will return TRUE) but I've put in an extra .001 anyway, so division
		//    by zero can't happen even in theory.
		(	((NewTotUnitsSold % 28) == 0) ||
			((NewTotUnitsSold % 29) == 0) ||
			((NewTotUnitsSold % 30) == 0) ||
			((NewTotUnitsSold % 31) == 0) ||
			((		 (float)(NewTotUnitsSold % 28)
				/	((float)(NewTotUnitsSold / 28) + .001)) < MaxRemainderPerOp)	)	)	// Total quantity sold is also "monthly'.
	{
		// If we get here, we've met the conditions for "rounding". Whether we actually close
		// the prescription or not is controlled by RoundPackagesToCloseRx; but even if "rounding"
		// is disabled, we still want to set the "eligible for rounding" status.
		Rounding_status	= 1;

		if (RoundPackagesToCloseRx)	// From sysparams/round_pkgs_sold.
		{
			// If we get here, we actually do want to perform "rounding".
			UnitsRemaining = 0;
			Rx_status = 2;	// Force complete sale even if there is a small unsold "remainder".
		}
		else
		{
			// If we get here, rounding *would* close the prescription but that option is
			// disabled. In this case, we fall back to the "standard" logic to determine
			// the new prescription-sold status. Since we already tested (UnitsSoldNow < UnitsSellable)
			// in the general conditions for rounding, we know that if we get here the
			// current sale is not enough to mark the prescription as fully sold - so we
			// don't need a conditional assignment here.
			Rx_status = 1;
		}
	} // Rounding conditions *did* apply.

	else
	{	// Rounding conditions did not apply - so use the old, simple logic
		// to decide if this is a full or a partial sale.
		// DonR 21May2017: Revamped the new-status-calculation logic a bit. This version
		// deals with deletions and sales separately, to make things a little simpler;
		// and because for deletions it pre-calculates what will be written to
		// tot_op_sold and tot_units_sold, it should always give a status of zero
		// for prescriptions where all sales have been deleted.
		if (ActionCode == SALE_DELETION)
		{
			// We can (or should be able to) assume that after a deletion, the prescription
			// will not have status 2 (entirely sold). So the only question should be whether
			// there is some remaining sold amount after the deletion; if everything is going
			// to be zero, then the prescription's status will be zero as well.
			Rx_status = (((DRX_SoldOP + Rx_OpUpdate) == 0) && ((DRX_SoldUnits + Rx_UnitsUpdate) == 0)) ? 0 : 1;
		}
		else	// Drug sale.
		{
			// DonR 21Nov2017 CR #13341: Implement the "force full sale" logic for cross-group substitutions.
			Rx_status = ((UnitsSoldNow >= UnitsSellable) || (ForceRxFullySold)) ? 2 : 1;
		}
	}	// Rounding conditions did not apply.
	// DonR 04May2016 end.


	// DonR 07Jul2015: If necessary (i.e. if the package sizes are different), convert
	// the OP/Units sold into equivalent OP/Units of the drug prescribed. We want to
	// do all our "accounting" based on the originally-prescribed drug!
	// DonR 10May2017: Perform this conversion for drug sales *only* - deletions come
	// to this routine with their OP/Units *already* in terms of the prescribed drug!
	if ((ActionCode != SALE_DELETION) && (DL.effective_package_size != DP.effective_package_size))
	{
		Rx_OpUpdate		= UnitsSoldNow / DP.effective_package_size;
		Rx_UnitsUpdate	= UnitsSoldNow % DP.effective_package_size;
// GerrLogMini (GerrId, "Translated to prescribed pkg size: Rx_OpUpdate = %d, Rx_UnitsUpdate = %d.", Rx_OpUpdate, Rx_UnitsUpdate);
	}


	// DonR 09Apr2018: AS/400 wants RK9122ACP to be populated based on the package size of
	// the drug sold, *not* of the drug prescribed. This doesn't make a lot of difference
	// for pills, but it does for things like syrups where units should always be zero.
	// Rather than change existing logic, I've added two new columns to pd_rx_link:
	// sold_drug_op and sold_drug_units. These will be used *only* to populate RK9122ACP.
	SoldDrug_OP		= UnitsSoldNow / DL.effective_package_size;
	SoldDrug_Units	= UnitsSoldNow % DL.effective_package_size;


	// Update doctor_presc.
	// Note: some of these status fields (linux_update_flag/date/time, reported_to_cds)
	// may turn out to be unnecessary. Note also that we perform this update unconditionally -
	// if the prescription isn't found, the update will fail with no horrible consequences.
	// Sometimes the prescription may have come in between the time we tried reading it (above)
	// and the time we try to update it.
	// DonR 20Jul2015: No need to set reported_to_as400 false when we sell drugs, since we are no
	// longer updating rows in RK9049CEP to reflect drug sales.
	// DonR 05Jul2016: Add a little retry loop to cope with DB contention.
	// DonR 18Jul2016: Add queuing (to new table drx_status_queue if UPDATE attempts fail.
	// DonR 02Oct2017: If we didn't succeed in reading a doctor prescription, don't try
	// to update a doctor prescription.
	if (READ_RX_SQLCODE == 0)	// DonR 02Oct2017
	{
		// Insert to the "pass-through" table used to update CDS. Note that if we were unable to
		// read the doctor_presc row to get required fields, there is no point in inserting
		// bogus values - just force a failure code of -1 into INS_RXSTAT_SQLCODE.
		// DonR 04Jan2016: At Eylon Rahav's request, added Largo Prescribed to rx_status_changes.
		if ((visit_number		< 1)	||
			(visit_line_number	< 1)	||
			(clicks_patient_id	> 99999999))
			GerrLogMini (GerrId, "update_doctor_presc: About to write strange data to rx_status_changes!");

		ExecSQL (	MAIN_DB, UpdateDoctorPresc_INS_rx_status_changes,
					&clicks_patient_id,		&DocPrID,			&visit_number,
					&visit_line_number,		&LargoPrescribed,	&prev_sold_status,
					&Rx_status,				&SysDate,			&SysTime,
					END_OF_ARG_LIST													);
// -11031 = invalid cursor state.

		// Rather than handle errors individually, just remember what code we got for
		// this INSERT; we'll decide what to do about it later.
		INS_RXSTAT_SQLCODE = SQLCODE;


		// Updating doctor_presc is prone to access conflicts - so we use a retry loop.
		for (tries = 0; tries < 3; tries++)
		{
			if (tries > 0)
				usleep (200000);	// Wait for 200 milliseconds. Note that this means a total wait time
									// of 1/4 second per loop iteration.

			ExecSQL (	MAIN_DB, UpdateDoctorPresc_UPD_Rx_std_keys,
						&Rx_status,			&Rounding_status,	&SoldDate,
						&SoldTime,			&Rx_UnitsUpdate,	&Rx_OpUpdate,
						&SysDate,			&SysTime,
						&v_MemberID,		&DocPrID,			&LargoPrescribed,
						&ValidFromDate,		&DocID,				&visit_line_number,
						&v_IDCode,			END_OF_ARG_LIST								);

			// DonR 04Jan2016: Every once in a while, the UPDATE above fails with SQLCODE == -245
			// (cannot position via index). In this case, try again using the other unique index.
			// (A regular retry might work just as well.) Don't even try unless we know that we did
			// manage to find the doctor prescription - no point in trying to update something that
			// doesn't exist! (Note that as of 02Oct2017, this whole block of code executes conditionally
			// on whether the read was successful - so we don't need to test READ_RX_SQLCODE in the "if" below.
			// DonR 31May2016: Revised the WHERE clause to reflect the new index without Clicks Patient ID.
			if (SQLERR_code_cmp (SQLERR_access_conflict))
			{
				usleep (50000);	// Wait for 50 milliseconds. Note that this means a total wait time
								// of 1/4 second per loop iteration.

				ExecSQL (	MAIN_DB, UpdateDoctorPresc_UPD_Rx_CDS_keys,
							&Rx_status,			&Rounding_status,	&SoldDate,
							&SoldTime,			&Rx_UnitsUpdate,	&Rx_OpUpdate,
							&SysDate,			&SysTime,
							&visit_number,		&DocPrID,			&visit_line_number,
							END_OF_ARG_LIST													);

			}	// Initial UPDATE failed with some form of access-conflict error.


			// DonR 18Jul2016: Instead of just failing, write the prescription status
			// update information to the new drx_status_queue table. Note that we
			// don't add a delay here, as we're accessing a different table where
			// we shouldn't have contention issues.
			// DonR 02Oct2017: For better performance, don't use the backup method
			// (writing to the delayed-update queue) on the first attempt.
			if ((SQLERR_code_cmp (SQLERR_access_conflict)) && (tries > 0))
			{
				ExecSQL (	MAIN_DB, UpdateDoctorPresc_INS_drx_status_queue,
							&v_MemberID,		&v_IDCode,			&DocID,				&visit_number,
							&DocPrID,			&visit_line_number,	&LargoPrescribed,	&ValidFromDate,
							&Rx_status,			&Rounding_status,	&Rx_UnitsUpdate,	&Rx_OpUpdate,
							&SoldDate,			&SoldTime,			&FlagValueOne,		&SysDate,
							&SysTime,			&SysDate,			&SysTime,			&FlagValueOne,
							END_OF_ARG_LIST																	);

				if (SQLERR_code_cmp (SQLERR_not_unique) == MAC_TRUE)	// Row was already present - shouldn't happen much in real life.
				{
					ExecSQL (	MAIN_DB, UpdateDoctorPresc_UPD_drx_status_queue,
								&Rx_status,			&Rounding_status,	&SoldDate,
								&SoldTime,			&Rx_UnitsUpdate,	&Rx_OpUpdate,
								&SysDate,			&SysTime,
								&visit_number,		&DocPrID,			&visit_line_number,
								END_OF_ARG_LIST													);

					// Note that an UPDATE can "succeed" even if nothing was found to update. In
					// this case, we DON'T want to think we succeeded! Force in -271, which is an
					// access-conflict error we don't encounter updating doctor_presc - this way
					// we get a meaningful diagnostic.
					if (NO_ROWS_AFFECTED)
						sqlca.sqlcode = SQLCODE = -271;

				}	// Drx_status_queue row was already present, so update it.

				// Suppress queued-successfully message in regular program log.
				if (SQLCODE != 0)
				{
					GerrLogMini		(							GerrId, "        Queue Rx %ld/%6d/%3d for update, SQLCODE %4d.",
																		visit_number, DocPrID, visit_line_number, SQLCODE);
				}

				GerrLogFnameMini	("drx_status_queue_log",	GerrId, "        Queue Rx %ld/%6d/%3d for update, SQLCODE %4d.",
																		visit_number, DocPrID, visit_line_number, SQLCODE);

			}	// Second UPDATE also failed with some form of access-conflict error, and we're on at least the second attempt.


			// If there was an access-conflict error, keep looping; otherwise (success or
			// "hard failure") break out of the loop.
			if (SQLERR_code_cmp (SQLERR_access_conflict) != MAC_TRUE)
			{
				tries++;	// For human-friendly error logging.
				break;
			}
		}	// Retry loop for doctor_presc update.
	}	// DonR 02Oct2017 update doctor_presc and rx_status_changes only if we actually read a prescription to update.
	else
	{
		// If we didn't read a doctor_presc row to update, make sure we don't report any bogus errors.
		SQLCODE	= tries	= 0;
		INS_RXSTAT_SQLCODE = SQLERR_NO_MORE_DATA_OR_NOT_FOUND;	// = Not found (which is not normally given for INSERTs).
	}


	// Rather than handle errors individually, just remember what code we got for
	// this UPDATE; we'll decide what to do about it later.
	UPD_RX_SQLCODE = SQLCODE;	// (Zero by definition if we never read a doctor prescription.)
	if (SQLCODE != 0)
	{
		GerrLogMini (GerrId, "update_doctor_presc: doctor_presc update failed - SQLCODE = %d after %d attempts.", SQLCODE, tries);
	}
	else
	{
		// DonR 03Dec2020: Increase the logging threshold from 2 to 3 tries; this should mean
		// that only in unusual situations will we see any logged messages about this update
		// unless it actually fails or there is an unusual database traffic jam.
		if (tries > 2)
			GerrLogMini (GerrId, "update_doctor_presc: RX update OK after %d attempts.", tries);
	}


	// Insert to prescription_drugs-to-doctor_presc linkage table.
	// DonR 02Oct2017: If we're in late-arrival mode, the pd_rx_link row should already exist.
	// OTOH it's probably missing its visit information - so update that.
	if (LateArrivalMode_in)
	{
		INS_PDL_SQLCODE = 0;	// Just so we don't think anything went wrong with the INSERT we're not doing.

		// No error handling here, as the update is really pretty much optional.
		// DonR 01Aug2024 Bug #334612: Keep the value sent by pharmacy (use_instr_template)
		// when we inserted the pd_rx_link row instead of updating it from the doctor prescription.
		// DonR 11Dec2024 User Story #373619: Add rx_origin to pd_rx_link columns updated.
		ExecSQL (	MAIN_DB, UpdateDoctorPresc_UPD_pd_rx_link,
					&visit_number,			&visit_line_number,		&visit_date,
					&rx_origin,				&SysDate,				&SysTime,

					&v_RecipeIdentifier,	&LineNumber,			&DocPrID,
					&LargoPrescribed,		&ValidFromDate,			END_OF_ARG_LIST			);
	}	// Late-arrival mode: update pd_rx_link row with visit fields if they're missing (which they probably are).

	else
	{	// "Normal" mode: INSERT new row to pd_rx_link.
		// DonR 17May2020: Copy short integers to regular integer variables, since that's
		// how the Informix database is set up. We seem to be getting errors for sale
		// deletions when we rely on implicity type conversions.
		// DonR 25Aug2021: We are INSERTing to pd_rx_link even if the pharmacy sent a zero
		// value for Doctor Prescription ID. I don't think this causes any real harm, and
		// the somewhat-bogus row does have some diagnostic usefulness - so I'll leave this
		// unchanged.
		Rx_OpUpdate_i			= Rx_OpUpdate;
		Rx_UnitsUpdate_i		= Rx_UnitsUpdate;
		SoldDrug_OP_i			= SoldDrug_OP;
		SoldDrug_Units_i		= SoldDrug_Units;
		PrevUnsoldOP_i			= PrevUnsoldOP;
		PrevUnsoldUnits_i		= PrevUnsoldUnits;

		// DonR 01Aug2023 User Story #469361: Added Economypri Group, Prescription Type,
		// and Deleted Flag to pd_rx_link.
		// DonR 01Aug2024 Bug #334612: Use the value sent by pharmacy (use_instr_template)
		// to update Prescription Type instead of copying it from the doctor prescription.
		// DonR 11Dec2024 User Story #373619: Add rx_origin to pd_rx_link columns inserted.
		ExecSQL (	MAIN_DB, UpdateDoctorPresc_INS_pd_rx_link,
					&v_RecipeIdentifier,	&LineNumber,			&v_MemberID,
					&v_IDCode,				&DocID,					&DocPrID,
					&LargoPrescribed,		&LargoSold,				&ValidFromDate,
					&Rx_OpUpdate_i,			&Rx_UnitsUpdate_i,		&Rounding_status,

					&SoldDrug_OP_i,			&SoldDrug_Units_i,		&Rx_status,
					&SoldDate,				&PrevUnsoldOP_i,		&PrevUnsoldUnits_i,
					&visit_number,			&visit_line_number,		&visit_date,
					&SysDate,				&SysTime,				&FlagValueZero,

					&DL_economypri_grp,		&PrescriptionType_in,	&FlagValueZero,
					&rx_origin,				END_OF_ARG_LIST									);

		// Rather than handle errors individually, just remember what code we got for
		// this UPDATE; we'll decide what to do about it later.
		INS_PDL_SQLCODE = SQLCODE;
if (SQLCODE) { SQLERR_error_test (); GerrLogMini (GerrId, "%d/%d: Member %d, Rx %d, Largo %d/%d, From %d, Op/Units %d/%d/%d/%d.", v_RecipeIdentifier, LineNumber, v_MemberID, DocPrID, LargoPrescribed,
	LargoSold, ValidFromDate, Rx_OpUpdate, Rx_UnitsUpdate, SoldDrug_OP, SoldDrug_Units);    }


		// DonR 11Jun2017 CR #8425: We now keep track of how many doctor prescriptions each drug line links to.
		if ((RxLinkCounter_out != NULL) && (INS_PDL_SQLCODE == 0))
			*RxLinkCounter_out = *RxLinkCounter_out + 1;
	}	// *Not* in late-arrival mode.



// Note: send diagnostic to log only if an insert/update failed - *not* if reading doctor_presc failed.
if ((UPD_RX_SQLCODE != 0) || (INS_PDL_SQLCODE != 0) || ((INS_RXSTAT_SQLCODE != 0) && (INS_RXSTAT_SQLCODE != SQLERR_NO_MORE_DATA_OR_NOT_FOUND)))
GerrLogMini (GerrId, "update_doctor_presc SQL results: READ_RX_SQLCODE %d, UPD_RX_SQLCODE %d, INS_PDL_SQLCODE %d, INS_RXSTAT_SQLCODE %d.",
			 READ_RX_SQLCODE, UPD_RX_SQLCODE, INS_PDL_SQLCODE, INS_RXSTAT_SQLCODE);



	// If we had to retrieve the correct Valid-From Date and/or Largo Prescribed from
	// doctor_presc AND the calling routine has given us valid pointer(s) to store
	// these values, send them back to the calling routine. Note that if we *didn't*
	// need to get these values from doctor_presc (or we tried and failed to do so),
	// we will leave the return variable(s) alone, since they may be variables used
	// to update tables.
	if ((Rx_ValidFromDate	> 0) && (ValidFromDate_out		!= NULL))
		*ValidFromDate_out		= Rx_ValidFromDate;

	if ((Rx_LargoPrescribed	> 0) && (LargoPrescribed_out	!= NULL))
		*LargoPrescribed_out	= Rx_LargoPrescribed;

	// The visit/prescription arrival timestamps are relevant only in spool mode, and
	// in spool mode we'll get here only if we successfully read the doctor prescription.
	// So all we need to test is that the return pointers are not NULL.
	if (VisitDate_out		!= NULL) *VisitDate_out		= visit_date;
	if (VisitTime_out		!= NULL) *VisitTime_out		= visit_time;
	if (Rx_Added_Date_out	!= NULL) *Rx_Added_Date_out	= row_added_date;
	if (Rx_Added_Time_out	!= NULL) *Rx_Added_Time_out	= row_added_time;


	RESTORE_ISOLATION;

	// If necessary (i.e. if this was a deletion and the relevant drug sale was already on the
	// late-arrival queue), force an immediate processing of the late-arriving prescription queue.
	if (ForceArrivalQProcessing)
		ProcessRxLateArrivalQueue (0, 0, 0, 0);	// All arguments are zero in "normal" queue-processing mode.


	// Return zero if everything was OK; return -1 (TRUE) if one or more table-writes failed
	// and we had to add to the spool table.
	return (UPD_RX_SQLCODE || INS_PDL_SQLCODE || (INS_RXSTAT_SQLCODE && (INS_RXSTAT_SQLCODE != SQLERR_NO_MORE_DATA_OR_NOT_FOUND)));
}



short ProcessRxLateArrivalQueue (int PrescID_in, int LargoPrescribed_in, int DocPrID_in, int DocID_in)
{
	int		NumProcessed	= 0;

	// (Potential) input variables.
	int		PrescID_sel		= PrescID_in;
	int		LargoPrescribed	= LargoPrescribed_in;
	int		DocPrID			= DocPrID_in;
	int		DocID			= DocID_in;

	// Variables read from rx_late_arrival_q
	short	ActionCode;
	int		v_MemberID;
	short	v_IDCode;
	short	DocIDType;
	int		PrescriptionID;
	short	LineNumber;
	int		LargoSold;
	int		ValidFromDate;
	int		SoldDate;
	int		SoldTime;
	short	OP_sold;
	short	UnitsSold;
	short	PrevUnsoldOP;
	short	PrevUnsoldUnits;

	// Number of minutes to wait before next retry of a particular queue item.
	// Note that we do our figuring-out using ordinary integers, then put the
	// result into a string which then gets converted to an SQL INTEGER variable
	// by the Informix function incvasc(). It would be nice if the conversion
	// from integer to interval could be handled by SQL code - but when I tried
	// it, I got compilation errors.
	int		MinutesBeforeRetry;
	int		MinutesUntil_0100;
//	char	RetryIntervalStr [6];
//	INTERVAL MINUTE(4) TO MINUTE	RetryInterval;

	// Additional variables for date logic.
	int		SysDate;
	int		SysTime;
	int		OneWeekAgo;
	long	CurrentYearToSecond;
	long	NextRetryYearToSecond;
	int		CurrTimeInMinutes;
	int		MinDate_NoExtraDelay;
	int		MinDate_FiveMinuteDelay;
	int		MinDate_OneHourDelay;

	// Doctor Prescription timestamp values set by update_doctor_presc();
	int		VisitDate;
	int		VisitTime;
	int		Rx_Added_Date;
	int		Rx_Added_Time;


	SysDate				= GetDate ();
	SysTime				= GetTime ();
	OneWeekAgo			= IncrementDate (SysDate,  -7);
	CurrentYearToSecond	= GetCurrentYearToSecond ();

	// Depending on whether this is a call to process a specific queue item or not, set up
	// SELECT specifying variables.
	if (PrescID_sel <= 0)
	{
		// "Normal" mode, controlled by date/time variables.
		LargoPrescribed	= DocPrID = DocID = 0;

		// Set up criteria for specifying interval before next retry of prescriptions that
		// haven't yet arrived.
		CurrTimeInMinutes = (60 * (SysTime / 10000)) + ((SysTime / 100) % 100);
		MinutesUntil_0100 = (25 * 60) - CurrTimeInMinutes; // NEXT calendar day at 01:00.

		MinDate_NoExtraDelay		= IncrementDate (SysDate,  -4);
		MinDate_FiveMinuteDelay		= IncrementDate (SysDate, -10);
		MinDate_OneHourDelay		= IncrementDate (SysDate, -30);

	}	// Function was called for "normal" queue processing, not a specific queue item.


	// DonR 06May2021: In order to reduce (or, hopefully, avoid completely) database contention
	// problems, I now restrict the cursor to SELECT only the first 50 rows (meaning the oldest
	// rows that haven't already been marked with a future next_proc_time). I'll back out the
	// previous attempt to fix the contention issue, since it appears to have caused even worse
	// problems with database locks left open.
	DeclareAndOpenCursorInto (	MAIN_DB, ProcessRxLateArrivalQ_ProcRxArrivals_cur,
								&ActionCode,		&v_MemberID,			&v_IDCode,
								&PrescriptionID,	&LineNumber,			&DocID,	
								&DocIDType,			&DocPrID,				&LargoPrescribed,
								&LargoSold,			&ValidFromDate,			&SoldDate,		
								&SoldTime,			&OP_sold,				&UnitsSold,		
								&PrevUnsoldOP,		&PrevUnsoldUnits,
								&OneWeekAgo,
								&PrescID_sel,		&CurrentYearToSecond,
								&PrescID_sel,		&PrescID_sel,
								&LargoPrescribed,	&LargoPrescribed,
								&DocPrID,			&DocPrID,
								&DocID,				&DocID,					END_OF_ARG_LIST		);

	if (SQLERR_error_test ())
	{
		return (-1);
	}

	NumProcessed = 0; // Redundant, but paranoia is a good thing!

	do
	{
		FetchCursor (	MAIN_DB, ProcessRxLateArrivalQ_ProcRxArrivals_cur	);

		if (SQLERR_code_cmp (SQLERR_end_of_fetch) == MAC_TRUE)
			break;

		if (SQLERR_error_test ())
		{
			NumProcessed = -1;
			break;
		}

		// Note that update_doctor_presc() returns zero if everything went OK.
		if (update_doctor_presc (	1,					// = Late Arrival Mode.
									ActionCode,
									v_MemberID,
									v_IDCode,
									PrescriptionID,
									LineNumber,
									DocID,
									DocIDType,
									DocPrID,
									LargoPrescribed,
									LargoSold,
									NULL,				// Drug List row - always NULL in late-arrival mode.
									ValidFromDate,
									SoldDate,
									SoldTime,
									OP_sold,
									UnitsSold,
									PrevUnsoldOP,
									PrevUnsoldUnits,
									0,					// DonR 01Aug2024 Bug #334612: Prescription_type is not relevant in late-arrival mode.
									0,					// Deleted Prescription ID is not relevant in late-arrival mode.
									1,					// Single-prescription mode.
									NULL,				// Single-prescription mode.
									NULL,				// Largo Prescribed output variable is irrelevant here.
									NULL,				// Valid From Date output variable is irrelevant here.
									NULL,				// Rx Link Counter output variable is irrelevant here.
									&VisitDate,
									&VisitTime,
									&Rx_Added_Date,
									&Rx_Added_Time		)	== 0)
		{
			// update_doctor_presc() succeeded in reading the relevant doctor prescription and performing
			// all the required updates - so we want to update the rx_late_arrival_q row to indicate this.
			ExecSQL (	MAIN_DB, ProcessRxLateArrivalQ_UPD_rx_late_arrival_q,
						&VisitDate,				&VisitTime,			&Rx_Added_Date,		&Rx_Added_Time,
						&CurrentYearToSecond,
						&PrescriptionID,		&LineNumber,		&LargoPrescribed,	&DocPrID,
						&DocID,					&DocIDType,			&ValidFromDate,		END_OF_ARG_LIST		);

			if (SQLCODE == 0)
				NumProcessed++;
			else
				GerrLogMini(GerrId, "ProcessRxLateArrivalQueue: UPDATE (success) to rx_late_arrival_q failed, SQLCODE = %d.", SQLCODE);

		}	// update_doctor_presc() was successful.

		// If update_doctor_presc() didn't find the doctor prescription and update it,
		// mark this queue item for later processing. The amount of time we wait between
		// retries depends on how "fresh" the sale is; we don't want to keep the database
		// busy with frequent retries of ancient stuff.
		else
		{
			if (PrescID_sel > 0)	// Single-queue-item mode.
			{
				// This is a brand-new transaction (or at least the deletion request is),
				// so we want frequent retries.
				MinutesBeforeRetry = 0;
			}
			else
			{
				// "Normal" queue processing: set the retry interval based on how long ago
				// the pharmacy transaction took place.
				MinutesBeforeRetry	= MinutesUntil_0100;	// Default: process really old stuff once a day at 1:00 AM.

				if (SoldDate >= MinDate_OneHourDelay)		MinutesBeforeRetry = 60;
				if (SoldDate >= MinDate_FiveMinuteDelay)	MinutesBeforeRetry =  5;
				if (SoldDate >= MinDate_NoExtraDelay)		MinutesBeforeRetry =  0;
			}

			// Convert the Minutes Before Retry we've calculated to an SQL INTERVAL variable.
//			sprintf (RetryIntervalStr, "%d", MinutesBeforeRetry);
//			incvasc (RetryIntervalStr, &RetryInterval);
			NextRetryYearToSecond = GetIncrementedYearToSecond (MinutesBeforeRetry * 60);

			ExecSQL (	MAIN_DB, ProcessRxLateArrivalQ_UPD_SetForNextRetry,
						&NextRetryYearToSecond,		&CurrentYearToSecond,
						&PrescriptionID,			&LineNumber,			&LargoPrescribed,
						&DocPrID,					&DocID,					&ValidFromDate,
						END_OF_ARG_LIST															);

			if (SQLCODE != 0)
				GerrLogMini(GerrId, "ProcessRxLateArrivalQueue: UPDATE (failure) to rx_late_arrival_q failed, SQLCODE = %d, MinutesBeforeRetry = %d.",
							SQLCODE, MinutesBeforeRetry);
		}	// update_doctor_presc() did *not* succeed in finding and updating a doctor prescription.


		// If this routine was called for a particular queue item, there should be
		// only one row in the cursor - but let's force a stop after the first row anyway.
		if (PrescID_sel > 0)
		{
			// In "normal" mode, we don't care how many rows we processed - zero is going to be
			// the case more often than not. But in single-row mode, we should pretty much always
			// process a row, since this mode is called only when a deletion detected that the
			// doctor prescription arrived after the sale took place. So we want to print a log
			// message if things didn't go as expected.
			if (NumProcessed < 1)
				GerrLogMini (GerrId, "ProcessRxLateArrivalQueue: Failed to process queue item for %d Largo prescribed %d DocPrID %d.",
					            PrescriptionID, LargoPrescribed, DocPrID);

			break;
		}

		// DonR 05Apr2021: If we get here, we are doing "normal" queue processing, not just
		// updating a single row. In order to avoid database deadlocks, add a COMMIT for
		// each loop iteration - I'm not sure if we're actually getting deadlocks from this
		// routine, but I suspect that we are. Note that to support in-loop COMMITs, I added
		// ODBC_PRESERVE_CURSORS = 1 to SqlServer.c's startup database parameters.
		//
		// DonR 06May2021: My previous attempt at getting rid of database-contention errors appears to
		// have caused more (and worse) problems than it solved - database locks were being left open,
		// and after a few hours the whole application would grind to a halt. Instead, I've backed out
		// the change, switched ODBC_PRESERVE_CURSORS back to 0 for this program, and added a "{FIRST} 50"
		// to the ProcessRxLateArrivalQ_ProcRxArrivals_cur SELECT to limit the number of late-arriving
		// prescriptions we'll look for in any one call to this routine.
//		CommitAllWork ();

	}
	while (1);	// End of fetch Rx Late Arrival Queue loop.


	CloseCursor (	MAIN_DB, ProcessRxLateArrivalQ_ProcRxArrivals_cur	);

	CommitAllWork ();

	return (NumProcessed);
}


#if 0
short CheckForServiceWithoutCard_Old (	TMemberData		*Member,
										PHARMACY_INFO	Phrm_info)
{
	short		Authorized	= 0;	// Default = not authorized.
	short		ValidDays;
	time_t		Now;
	time_t		NowMinus;
	struct tm	MaxTm;
	int			SysDate;
	int			MaxDateForServiceWithoutCard;
	ISOLATION_VAR;

	int		NumSales;
	int		MaxDate;
	int		MaxTime;
	int		ReqDate			= Member->update_date;
	int		ReqTime			= Member->update_time;
	int		Member_ID		= Member->ID;
	int		Member_ID_code	= Member->ID_code;

// if (Member->authorizealways) GerrLogFnameMini ("SvcWithoutCard_log", GerrId, "CheckForServiceWithoutCard: Member %d, Pharm Permission %d, AuthAlways %d, UpdatedBy %d.",
//													Member_ID, Phrm_info.pharmacy_permission, Member->authorizealways, Member->updated_by);
	// Use a do-while(0) "loop" to allow a single function exit point.
	do
	{
		// 1) If the Authorize Always flag isn't set, then service without magnetic card isn't authorized
		//    and there's no point in checking anything else.
		if (Member->authorizealways < 1)
			break;


		// 2) If the member's "updated by" field is *not* set to 8888, we're dealing with an Authorize
		//    Always flag that was set by "Haverut" and is not subject to time limits; on the other
		//    hand, in this case the Authorize Always flag is good for service *only* at Maccabi
		//    pharmacies.
		//
		// DonR 13Feb2018: In this situation, we *do* want to impose a time limit - the tolerance (in days)
		// set in sysparams/svc_w_o_card_days.
		// DonR 14Feb2018: Just to be nice, allow service without card to continue indefinitely if there
		// is no Card Date in the database for the member.
		// DonR 19Oct2023 User Story #487690: Added capability of disabling long-term service without card by setting
		// the "valid days" parameter < 1. Also added a separate parameter to enable (and control) long-term service
		// without card at private pharmacies.
		if (Member->updated_by != 8888)
		{
			SysDate = GetDate ();

			// Choose which "valid days" parameter to use.
			ValidDays = (Phrm_info.maccabi_pharmacy) ? SvcWithoutCardValidDays : SvcWithoutCardValidDaysPvt;

//			if (Phrm_info.maccabi_pharmacy)
//			{
			// (If the "valid days" parameter is less than or equal to zero, long-term service without card
			// is disabled for this pharmacy category.) HOWEVER, I'm *not* disabling service-without-card
			// for members with card_date == 0 at Maccabi pharmacies.
			MaxDateForServiceWithoutCard = (ValidDays > 0) ? AddDays (Member->update_date, (ValidDays - 1)) : 0;	// I.e. 1 day means it's valid only on Member->update_date.

			// NOTE: We may need to add special logic for Member->card_date == 1111 and/or 8888.
			// As of 14Feb2018, I'm still waiting for some clarification regarding situations
			// where the Service Without Card flag is set by Haverut but I still have a normal
			// Member->card_date in the database.
			// DonR 19Oct2023 User Story #487690: We're now adding the capability of enabling long-term
			// service without card at private pharmacies. Note that for now, the "forever" logic below
			// still works only at MaccabiPharm - but it appears that there are zero members affected by
			// this, so we don't really have to worry about whether it should also apply at private pharmacies.
			if ((MaxDateForServiceWithoutCard >= SysDate) || ((Member->card_date == 0) && (Phrm_info.maccabi_pharmacy)))
				Authorized = 2;	// Service without card is authorized!
//			}

			break;	// Nothing more to check.
		}


		// 3) If we get here, the member's Authorize Always flag is set *and* we're dealing with a
		//    member-initiated "I left my card at home" request (i.e. updated_by == 8888). First,
		//    check whether the overall maximum timeout has passed.
		if (diff_from_now (Member->update_date, Member->update_time) > NoCardSvc_Max_Validity)
		{
// GerrLogFnameMini ("SvcWithoutCard_log", GerrId, "CheckForServiceWithoutCard: Request from %d/%d has expired.", Member->update_date, Member->update_time);
			break;	// The request has timed out - no need to check for drug purchases.
		}


		// 4) A completed drug purchase at a private pharmacy will close the time window for the
		//    service-without-card request after a configurable interval - currently set to
		//    fifteen minutes.
		if (NoCardSvc_After_Pvt_Purchase >= 0)
		{
			REMEMBER_ISOLATION;
			SET_ISOLATION_DIRTY;

			MaxDate	= GetDate ();
			MaxTime	= GetTime ();
			MaxTime	= IncrementTime (MaxTime, (0 - NoCardSvc_After_Pvt_Purchase), &MaxDate);


			ExecSQL (	MAIN_DB,
						CheckForServiceWithoutCard_READ_CountPharmSales,
						CheckForServiceWithoutCard_READ_PrivatePharmacy,
						&NumSales,
						&Member_ID,			&Member_ID_code,
						&ReqDate,			&ReqDate,			&ReqTime,
						&MaxDate,			&MaxDate,			&MaxTime,
						END_OF_ARG_LIST													);

// GerrLogFnameMini ("SvcWithoutCard_log", GerrId, "CheckForServiceWithoutCard: %d disqualifying private-pharmacy sales found, SQLCODE = %d.", NumSales, SQLCODE);
			// No need for error-handling here (I think).
			if ((SQLCODE == 0) && (NumSales > 0))
			{
				RESTORE_ISOLATION;
				break;
			}
			else
				RESTORE_ISOLATION;
		}	// We need to test whether a private-pharmacy purchase has terminated the member's service-without-card request.


		// 5) A completed drug purchase at a Maccabi pharmacy will *not* close the time window for the
		//    service-without-card request; but let's assume that someone will change their mind about this,
		//    and support the feature just in case.
		if (NoCardSvc_After_Mac_Purchase >= 0)
		{
			REMEMBER_ISOLATION;
			SET_ISOLATION_DIRTY;

			MaxDate	= GetDate ();
			MaxTime	= GetTime ();
			MaxTime	= IncrementTime (MaxTime, (0 - NoCardSvc_After_Mac_Purchase), &MaxDate);


			ExecSQL (	MAIN_DB,
						CheckForServiceWithoutCard_READ_CountPharmSales,
						CheckForServiceWithoutCard_READ_MaccabiPharmacy,
						&NumSales,
						&Member_ID,			&Member_ID_code,
						&ReqDate,			&ReqDate,			&ReqTime,
						&MaxDate,			&MaxDate,			&MaxTime,
						END_OF_ARG_LIST													);

// GerrLogFnameMini ("SvcWithoutCard_log", GerrId, "CheckForServiceWithoutCard: %d disqualifying Maccabi-pharmacy sales found, SQLCODE = %d.", NumSales, SQLCODE);
			// No need for error-handling here (I think).
			if ((SQLCODE == 0) && (NumSales > 0))
			{
				RESTORE_ISOLATION;
				break;
			}
			else
				RESTORE_ISOLATION;
		}	// We need to test whether a Maccabi-pharmacy purchase has terminated the member's service-without-card request.


		// 6) If we get here, we've passed all applicable tests and the member's request for service without card is approved.
		Authorized = 2;	// Service without card is authorized!

	}
	while (0);

// if (Member->authorizealways) GerrLogFnameMini ("SvcWithoutCard_log", GerrId, "CheckForServiceWithoutCard returning %d.", Authorized);

	return (Authorized);
}
#endif


// Marianna 30Apr2025 User Story #391697 NEW Function 
short CheckForServiceWithoutCard (	TMemberData		*Member,
									PHARMACY_INFO	Phrm_info	)
{
	short	Authorized = 0;	// Default = not authorized.
	int		SysDate;
	int 	SysTime;
	int		NumSales;
	int		MaxDate;
	int		MaxTime;
	int 	EligibilityHttpStatus;
	int 	eligibility_err;

	TEligibilityData EligibilityData;
	ISOLATION_VAR;


	SysDate = GetDate();
	SysTime = GetTime();

	// Get eligibility data without card
	eligibility_err = EligibilityWithoutCard(Member->ID, Member->ID_code, &EligibilityData, &EligibilityHttpStatus);

	if (eligibility_err)  // If there was an error retrieving eligibility data
	{
		GerrLogMini (	GerrId,
						"CheckForServiceWithoutCard: Could not retrieve eligibility data for "
						"Member ID %d; EligibilityWithoutCard() returned %d/HTTP response %d.",
						Member->ID, eligibility_err, EligibilityHttpStatus);
	}

	else // If eligibility data is successfully retrieved for the member without card
	{
		// Long-term service without card
		if (EligibilityData.approval_type_code == 1)
		{
			// NOTE: Because we're now getting both valid-from and valid-until datetime values from the new
			// REST service, we no longer need to calculate the end of the long-term service-without-card
			// period - so the sysparams parameters SvcWithoutCardValidDays and SvcWithoutCardValidDaysPvt
			// are not relevant.

			//  Check if the current date is within the valid range (from and until)
			if	(	(	(SysDate >  EligibilityData.valid_from_date)	||
						(SysDate == EligibilityData.valid_from_date && SysTime >= EligibilityData.valid_from_time)	)
					&&
					(	(SysDate <	EligibilityData.valid_until_date)	||
						(SysDate == EligibilityData.valid_until_date && SysTime <= EligibilityData.valid_until_time)	)
				)
			{
				// If both date and time are within the valid range, authorize the service
				Authorized = 2; // Service without card is authorized!
			}

			// If the request was out of range, log it.
			else
			{
				GerrLogMini	(	GerrId,
								"CheckForServiceWithoutCard: Long-term date/time is out of "
								"range for Member ID %d. Valid from %d/%d until %d/%d.",
								Member->ID,
								EligibilityData.valid_from_date,	EligibilityData.valid_from_time,
								EligibilityData.valid_until_date,	EligibilityData.valid_until_time);
			}

		} // End of long-term service validation

		else
		if (EligibilityData.approval_type_code == 2)	// Short-term service without card
		{
			// Use a do-while(0) "loop" to allow a single function exit point.
			do
			{
				// We're dealing with a member-initiated "I left my card at home" request.
				// First, check whether the overall maximum timeout has passed.
				if (diff_from_now (EligibilityData.valid_from_date, EligibilityData.valid_from_time) > NoCardSvc_Max_Validity)
				{
					break;	// The request has timed out - no need to check for drug purchases.
				}

				// A completed drug purchase at a private pharmacy will close the time window for the
				// service-without-card request after a configurable interval - currently set to
				// fifteen minutes.
				if (NoCardSvc_After_Pvt_Purchase >= 0)
				{
					REMEMBER_ISOLATION;
					SET_ISOLATION_DIRTY;

					MaxDate = SysDate;
					MaxTime = SysTime;
					MaxTime = IncrementTime(MaxTime, (0 - NoCardSvc_After_Pvt_Purchase), &MaxDate);

					ExecSQL	(	MAIN_DB,
								CheckForServiceWithoutCard_READ_CountPharmSales,
								CheckForServiceWithoutCard_READ_PrivatePharmacy,

								&NumSales,

								&Member->ID,						&Member->ID_code,
								&EligibilityData.valid_from_date,	&EligibilityData.valid_from_date,
								&EligibilityData.valid_from_time,
								&MaxDate,							&MaxDate,
								&MaxTime,							END_OF_ARG_LIST						);

					// No need for error-handling here (I think).
					if ((SQLCODE == 0) && (NumSales > 0))
					{
						RESTORE_ISOLATION;
						break;
					}
					else
					{
						RESTORE_ISOLATION;
					}

				}	// We need to test whether a private-pharmacy purchase has terminated the member's service-without-card request.


				// A completed drug purchase at a Maccabi pharmacy will *not* close the time window for the
				// service-without-card request; but let's assume that someone will change their mind about this,
				// and support the feature just in case.
				if (NoCardSvc_After_Mac_Purchase >= 0)
				{
					REMEMBER_ISOLATION;
					SET_ISOLATION_DIRTY;

					MaxDate = SysDate;
					MaxTime = SysTime;
					MaxTime = IncrementTime(MaxTime, (0 - NoCardSvc_After_Mac_Purchase), &MaxDate);

					ExecSQL	(	MAIN_DB,
								CheckForServiceWithoutCard_READ_CountPharmSales,
								CheckForServiceWithoutCard_READ_MaccabiPharmacy,

								&NumSales,

								&Member->ID,						&Member->ID_code,
								&EligibilityData.valid_from_date,	&EligibilityData.valid_from_date,
								&EligibilityData.valid_from_time,
								&MaxDate,							&MaxDate,
								&MaxTime,							END_OF_ARG_LIST						);

					// No need for error-handling here (I think).
					if ((SQLCODE == 0) && (NumSales > 0))
					{
						RESTORE_ISOLATION;
						break;
					}
					else
					{
						RESTORE_ISOLATION;
					}

				}	// We need to test whether a Maccabi-pharmacy purchase has terminated the member's service-without-card request.


				// If we get here, we've passed all applicable tests and the member's request for service without card is approved.
				Authorized = 2;	// Service without card is authorized!

			} while (0);
		} // End of Short-term service validation

		else
		{
			if (EligibilityData.approval_type_code != 0)
				GerrLogMini	(	GerrId,
								"CheckForServiceWithoutCard: Unexpected authorization result %d for Member ID %d.",
								Authorized, Member->ID);
		}	// Log authorization type <> 0, 1, or 2.

	}	// Got some kind of non-error response from the REST service-without-card service.


	return (Authorized);
}	// End of new CheckForServiceWithoutCard() function.


int CheckMember29G (TMemberData		*Member_in,
					TPresDrugs		*DrugLine_in,
					PHARMACY_INFO	*PhrmInfo_in)
{
	int	ErrReturned	= 0;
	ISOLATION_VAR;

	int		MemberID		= Member_in->ID;
	short	MemberIDCode	= Member_in->ID_code;
	int		LargoCode		= DrugLine_in->DL.largo_code;
	int		TestDate;
	int		TestDate2;
	int		F29G_EndDate;
	int		F29G_PharmacyCode;
	short	F29G_FormType;
	short	needs_29_g;


	// If the drug being sold does not require a 29-G, do nothing and return the default (non-) error code of zero.
	// Note that the "needs 29-G status" can be true in either drug_list or the drug_extension "nohal" being used,
	// or in both.
	// Also, do nothing if the calling routine hasn't provided a non-zero Member ID and Largo Code.
	switch (DrugLine_in->ret_part_source.table)
	{
		case FROM_SPEC_PRESCS_TBL:
		case FROM_PHARMACY_ISHUR:
		case FROM_DRUG_EXTENSION:		needs_29_g = DrugLine_in->rule_needs_29g;	break;

		default:						needs_29_g = DrugLine_in->DL.needs_29_g;	break;
	}

//	if ((( DrugLine_in->DL.needs_29_g	>  0)		||
//		 ((DrugLine_in->rule_needs_29g	>  0) && (DrugLine_in->ret_part_source.table == FROM_DRUG_EXTENSION)))		&&
	if ((needs_29_g		> 0)		&&
		(MemberID		> 0)		&&
		(LargoCode		> 0))
	{
		TestDate = TestDate2 = GetDate ();
		SET_ISOLATION_DIRTY;

		// See if there is a current Form 29-G row in our database.
		// Note that for now, we are not doing anything with Pharmacy Code or Form Type.
		ExecSQL (	MAIN_DB, CheckMember29G_READ_member_drug_29g,
					&F29G_EndDate,	&F29G_PharmacyCode,	&F29G_FormType,
					&MemberID,		&LargoCode,			&MemberIDCode,
					&TestDate,		&TestDate2,			END_OF_ARG_LIST	);

// GerrLogMini(GerrId, "Looked up 29G for %d/%d, SQLCODE = %d.", MemberID, LargoCode, SQLCODE);
		if (SQLERR_code_cmp (SQLERR_not_found) == MAC_TRUE)
		{
			ErrReturned = FORM_29G_NOT_FOUND;
		}
		else
		{
			// DonR 09Jun2020: Sporadic ODBC loss-of-PREPAREdness is treated as a
			// conflict error in order to force a retry.
			// DonR 03Mar2021: Folding the error codes for SQLERR_deadlocked into
			// SQLERR_access_conflict, so there's no longer any reason to check
			// for both. (I'll keep SQLERR_deadlocked available in case any routine
			// needs to know more specifically what went wrong.)
			if (SQLERR_code_cmp (SQLERR_access_conflict) == MAC_TRUE)
			{
				ErrReturned = -1;	// Calling routines will need to know that this means "restart".
			}
			else
			{
				if (SQLERR_error_test ())
				{
					ErrReturned = ERR_DATABASE_ERROR;
				}
				else
				{
					// We successfully read a current Form 29-G for this member/drug.
					// Now let's see if it's going to expire soon - and if so, send the
					// pharmacy a warning message.
					do
					{
						TestDate = IncrementDate (TestDate, 15);
						if (F29G_EndDate <= TestDate)
						{
							ErrReturned = FORM_29G_EXP_IN_15_DAYS;
							break;
						}

						TestDate = IncrementDate (TestDate, 15);
						if (F29G_EndDate <= TestDate)
						{
							ErrReturned = FORM_29G_EXP_IN_30_DAYS;
							break;
						}

						TestDate = IncrementDate (TestDate, 15);
						if (F29G_EndDate <= TestDate)
						{
							ErrReturned = FORM_29G_EXP_IN_45_DAYS;
							break;
						}

						TestDate = IncrementDate (TestDate, 15);
						if (F29G_EndDate <= TestDate)
						{
							ErrReturned = FORM_29G_EXP_IN_60_DAYS;
							break;
						}
					}
					while (0);
					// Check for soon-to-expire Form 29-G.

					// If we get here, either everything is OK and ErrReturned still has its default
					// value of zero; or else it has a warning about a soon-to-expire Form 29-G. 

				}	// Successful read of member's Form 29-G data.
			}	// Something other than conflict error.
		}	// Something other than not-found.

		RESTORE_ISOLATION;
	}	// There is some reason to check the database for Form 29-G data.

	return (ErrReturned);
}	// End of CheckMember29G().



short CheckForPartiallySoldDoctorPrescription (int		MemberId_in,
											   short	MemberIdCode_in,
											   int		DocId_in,
											   int		DoctorPrId_in,
											   int		EarliestUntilDate_in,
											   int		LatestUntilDate_in)
{
	ISOLATION_VAR;

	short	RxWasPartiallySold	= 0;

	int		MemberId			= MemberId_in;
	short	MemberIdCode		= MemberIdCode_in;
	int		DocId				= DocId_in;
	int		DoctorRxId			= DoctorPrId_in;
	int		EarliestPrescrDate	= EarliestUntilDate_in;
	int		LatestPrescrDate	= LatestUntilDate_in;
	short	SoldStatus;


	SET_ISOLATION_DIRTY;

	DeclareAndOpenCursorInto (	MAIN_DB, CheckForPartlySoldDocRx_partly_sold_cur,
								&SoldStatus,
								&MemberId,			&DoctorRxId,			&DocId,
								&MemberIdCode,		&EarliestPrescrDate,	&LatestPrescrDate,
								END_OF_ARG_LIST														);

	if (SQLCODE == 0)	// Don't bother with error-trapping.
	{
		FetchCursor (	MAIN_DB, CheckForPartlySoldDocRx_partly_sold_cur	);

		if (SQLCODE == 0)	// Don't bother with error trapping.
		{
			switch (SoldStatus)
			{
				case 0:		// If the first status is zero (= unsold), we need to see if there are any other statuses
							// found for this prescription. *Any* additional status (1 or 2) means the prescription
							// has been partially sold; we don't even have to look at the value, since we used
							// SELECT DISTINCT.
							FetchCursor (	MAIN_DB, CheckForPartlySoldDocRx_partly_sold_cur	);

							if (SQLCODE == 0)
								RxWasPartiallySold = 1;

							break;


				case 1:		RxWasPartiallySold = 1;	// If the first status is 1 (partially sold) then by definition
													// this is a partially-sold prescription.
							break;


				case 2:		break;	// If the first status is 2, then the prescription was completely sold. We shouldn't
									// see this in real life, since pharmacy shouldn't be sending us sale requests for
									// fully-sold prescriptions.


				default:	break;	// This shouldn't really happen, since 0, 1, and 2 are the only valid values for sold_status.
			}	// Switch on first status read.
		}	// Initial cursor fetch was successful.
	}	// Cursor open was successful.

	CloseCursor (	MAIN_DB, CheckForPartlySoldDocRx_partly_sold_cur	);

	return (RxWasPartiallySold);
}



short MarkDeleted (char	*Caller_in,
				   int	v_DeletedPrID,
				   int	v_DrugCode,
				   int	DeletionPrescId_in)
{
	short		DB_error_encountered = 0;
	short		tries;
	int			RowsFound;
	static int	LastPrID = -999;



	if (v_DrugCode != 0)	// Deleting a drug line.
	{
		ExecSQL (	MAIN_DB, MarkDrugSaleDeleted_UPD_prescription_drugs,
					&v_DeletedPrID,	&v_DrugCode,	END_OF_ARG_LIST			);

		// DonR 01Aug2023 User Story #469361: Added del_flg to pd_rx_link, so we can tell if a row in
		// that table is no longer "alive" without needing a join to prescription_drugs.
		if (SQLCODE == 0)
		{
			ExecSQL (	MAIN_DB, MarkDrugSaleDeleted_UPD_pd_rx_link,
						&v_DeletedPrID,	&v_DrugCode,	END_OF_ARG_LIST		);
		}

		// DonR 19Mar2024 User Story #540234: Also update pd_cannabis_products/del_flg.
		if (SQLCODE == 0)
		{
			ExecSQL (	MAIN_DB, MarkDrugSaleDeleted_UPD_pd_cannabis_products,
						&v_DeletedPrID,	&v_DrugCode,	END_OF_ARG_LIST		);
		}

		if (SQLCODE != 0)
		{
			// Don't bother logging the usual DB-contention error.
			if (SQLERR_code_cmp (SQLERR_access_conflict) == MAC_FALSE)
			{
				GerrLogMini (GerrId, "%s%s%s Mark  %d/%5d deleted,      SQLCODE %4d.",
							 ((v_DeletedPrID != LastPrID) ? "\n" : ""),
							 ((v_DeletedPrID != LastPrID) ? Caller_in : "      "),
							 ((v_DeletedPrID != LastPrID) ? ":" : " "),
							 v_DeletedPrID, v_DrugCode, SQLCODE);
			}
else GerrLogMini (GerrId, "MarkDeleted %d: MarkDrugSaleDeleted_UPD_prescription_drugs for %d/%d SQLCODE = %d.", DeletionPrescId_in, v_DeletedPrID, v_DrugCode, SQLCODE);

			LastPrID = v_DeletedPrID;
		}
	}

	else					// v_DrugCode == 0 means we're deleting the sale itself.
	{
//		// DonR 19Jun2022: In an attempt to fix the handling of deletions where we hit a DB contention
//		// error updating the prescriptions row and then somehow lose the update of the prescription_drugs
//		// row, split the prescriptions UPDATE into two separate operations: first count any non-deleted
//		// drug lines, and if that operations returns a COUNT of zero perform a simple, unconditional
//		// UPDATE on prescriptions.
//		ExecSQL (	MAIN_DB, CountNonDeletedDrugLines, &RowsFound,	&v_DeletedPrID,	END_OF_ARG_LIST	);
//
//		// Execute the UPDATE only if the SELECT COUNT was successful and returned RowsFound = 0.
//		if ((SQLCODE == 0) && (RowsFound == 0))
//		{
		ExecSQL (	MAIN_DB, MarkDrugSaleDeleted_UPD_prescriptions,
					&v_DeletedPrID,	&v_DeletedPrID,	END_OF_ARG_LIST		);
//		}

		if (SQLCODE != 0)
		{
			// Don't bother logging the usual DB-contention error.
			if (SQLERR_code_cmp (SQLERR_access_conflict) == MAC_FALSE)
			{
				GerrLogMini (GerrId, "%s%s%s Mark  %d       deleted,      SQLCODE %4d.",
							 ((v_DeletedPrID != LastPrID) ? "\n" : ""),
							 ((v_DeletedPrID != LastPrID) ? Caller_in : "      "),
							 ((v_DeletedPrID != LastPrID) ? ":" : " "),
							 v_DeletedPrID, SQLCODE);
			}
else GerrLogMini (GerrId, "MarkDeleted %d: MarkDrugSaleDeleted_UPD_prescriptions for %d SQLCODE = %d.", DeletionPrescId_in, v_DeletedPrID, SQLCODE);

			LastPrID = v_DeletedPrID;
		}
	}


	// DonR 30Jun2016: Instead of just failing, write the deleted prescription ID/Largo Code
	// to the new drugsale_del_queue table.
	if (SQLERR_code_cmp (SQLERR_access_conflict) == MAC_TRUE)
	{
		// In real life, we aren't likely to hit DB contention writing to drugsale_del_queue -
		// but just in case, add a couple of retries.
		for (tries = 0; tries < 4; tries++)
		{
			if (tries > 0)
			{
				GerrLogMini			(							GerrId, "        Queue %d/%5d for deletion, SQLCODE %4d, retrying...",
																		v_DeletedPrID, v_DrugCode, SQLCODE);
				GerrLogFnameMini	("drugsale_del_queue_log",	GerrId, "        Queue %d/%5d for deletion, SQLCODE %4d, retrying...",
																		v_DeletedPrID, v_DrugCode, SQLCODE);
				usleep (100000); // Wait for 1/10 second.
			}

			ExecSQL (	MAIN_DB, MarkDrugSaleDeleted_INS_drugsale_del_queue,
						&v_DeletedPrID,	&v_DrugCode,	END_OF_ARG_LIST			);
GerrLogMini (GerrId, "MarkDeleted %d: MarkDrugSaleDeleted_INS_drugsale_del_queue for %d/%d SQLCODE = %d.", DeletionPrescId_in, v_DeletedPrID, v_DrugCode, SQLCODE);

			// Either success or "real" failure breaks out of the retry loop - only
			// DB contention keeps us inside for more retries.
			if (SQLERR_code_cmp (SQLERR_access_conflict) != MAC_TRUE)
				break;
		}

		if (SQLCODE != 0)	// Success is boring, so don't clutter the log with it.
		{
			if (v_DrugCode != 0)	// Deleting a drug line.
			{
				GerrLogMini			(							GerrId, "        Queue %d/%5d for deletion, SQLCODE %4d.",
																		v_DeletedPrID, v_DrugCode, SQLCODE);
				GerrLogFnameMini	("drugsale_del_queue_log",	GerrId, "        Queue %d/%5d for deletion, SQLCODE %4d.",
																		v_DeletedPrID, v_DrugCode, SQLCODE);
			}
			else					// Deleting a prescriptions row.
			{
				GerrLogMini			(							GerrId, "        Queue %d       for deletion, SQLCODE %4d.",
																		v_DeletedPrID, SQLCODE);
				GerrLogFnameMini	("drugsale_del_queue_log",	GerrId, "        Queue %d       for deletion, SQLCODE %4d.",
																		v_DeletedPrID, SQLCODE);
			}
		}

		// If we couldn't even write to the deletion queue, give up and tell the calling routine
		// we hit a serious DB error.
		if (SQLCODE != 0)
		{
			DB_error_encountered = 1;
		}
	}	// Access conflict updating prescriptions/prescription_drugs row.

	else
	{
		if (SQLERR_error_test ())	// I.e. some error other than an access conflict.
		{
			DB_error_encountered = 1;
		}
	}	// Something (including, possibly, success!) other than an access-conflict error.


	return (DB_error_encountered);
}



short GetSpoolLock (int PharmacyCode_in, int TransactionID_in, short SpoolFlg_in)
{
	short	tries;
	int		prev_SQLCODE;
	int		v_PID;
	int		v_SysDate;
	int		v_SysTime;

			// NOTE: If the maximum number of seconds = X, the actual average lock timeout
			// will be around X - 0.5; so a value of 3 means the lock will time out between
			// around 2.1 and 3.0 seconds after it was set. In general, locks should be
			// cleared by the locking transaction long before that.
	int		MaxSpoolLockSeconds	= 3;	// Maybe replace with a SysParams variable in future?
	ISOLATION_VAR;


	// DonR 22May2025: In the last couple of weeks, we've been seeing database deadlocks
	// on spooled Transaction 6005s - mostly in the early morning. I checked the logs,
	// and I don't see *any* cases where GetSpoolLock actually did anything - so I've
	// added code to set "repeatable" isolation, in the hope that this will prevent
	// simultaneous processing of spooled transactions from the same pharmacy.
	// DonR 27Aug2025: Because I've added columns to pharm_spool_lock and logic to the
	// UPDATE query to make locking a function of "business logic" rather than relying
	// on the database server to throw a DB-contention error, we now want to work in
	// "dirty" mode to make sure we're seeing the most up-to-date pharm_spool_lock row
	// even (and especially!) if it hasn't been committed yet.
	REMEMBER_ISOLATION;
//	SET_ISOLATION_REPEATABLE;
	SET_ISOLATION_DIRTY;

	v_PID		= getpid ();
	v_SysDate	= GetDate ();

	// Retry for 2.5 seconds maximum. This may need tuning...
	for (tries = 0; tries < 25; tries++)
	{
		if (tries > 0)
			usleep (100000); // Wait for 1/10 second.

		v_SysTime = GetTime ();

		// DonR 27Aug2025: Because I'm tightening up the WHERE clause for the UPDATE statement,
		// I'm going to execute the INSERT unconditionally - it will almost always fail, but
		// we don't actually care - particularly since the volume of calls to GetSpoolLock()
		// is pretty low.
		ExecSQL (	MAIN_DB, GetSpoolLock_INS_pharm_spool_lock,
					&PharmacyCode_in,	END_OF_ARG_LIST				);

		// DonR 18Sep2025 HOT FIX: Force the program to ignore expected
		// errors on the INSERT.
		// 
		// FIX: If the INSERT returned a duplicate-key error (which will happen
		// essentially all the time), reassure the system that this is not a real
		// error. 21Sep2025: Add SQLCODE == 100 to the "suppress error" logic.
		if ((SQLERR_code_cmp (SQLERR_not_unique) == MAC_TRUE) || (SQLCODE == 100))
		{
			FORCE_SQL_OK;
		}

		// DonR 28Aug2025: Enhanced the pharm_spool_lock table and this UPDATE query so that we
		// no longer rely on the database server to throw a contention error; instead, the new
		// version of the query will update the row only if pharm_spool_lock/locked = 0 OR at
		// least 2 seconds have passed since the last lock was set. (The latter condition avoids
		// the "dead hand" situation where an old lock prevents any new activity.)
		ExecSQL (	MAIN_DB, GetSpoolLock_UPD_pharm_spool_lock,
					&v_PID,				&v_SysDate,				&v_SysTime,
					&PharmacyCode_in,	&MaxSpoolLockSeconds,	END_OF_ARG_LIST		);

		// DonR 27Aug2025: If no rows were affected, force a "not found" error
		// so that we know we failed to update the pharm_spool_lock row.
		if (SQLERR_code_cmp (SQLERR_no_rows_affected) == MAC_TRUE)
		{
			FORCE_NOT_FOUND;
		}

		// If we succeeded, this instance of sqlserver.exe has the lock - and will keep it until
		// the current DB transaction is committed (or rolled back).
		if (SQLCODE == 0)
		{
			tries++;	// Just to keep the diagnostic clear.
			break;
		}
		else
		{
			prev_SQLCODE = SQLCODE;
		}
	}

	RESTORE_ISOLATION;

	// Normally, we succeed on the first try - so don't bother with a diagnostic in that case.
	if (tries > 1)
		GerrLogMini (GerrId,	"GetSpoolLock: Pharm %d Trn %d%s, SQLCODE %d after %d "
								"attempts (%d msec delay). Previous SQLCODE was %d",
								PharmacyCode_in, TransactionID_in, ((SpoolFlg_in) ? "-spool" : ""),
								SQLCODE, tries, ((tries - 1) * 100), prev_SQLCODE);

	return (0);
}



int ClearSpoolLock (int PharmacyCode_in)
{
	// This function is basically just an "alias" for the SQL UPDATE query; we don't
	// want or need any serious error-handling, diagnostics, or anything else.
	ExecSQL (	MAIN_DB, CLEAR_pharm_spool_lock, &PharmacyCode_in,	END_OF_ARG_LIST	);

	if (SQLCODE)
	{
		SQLERR_error_test ();
	}

	return (SQLCODE);
}



short is_valid_presc_source (short PrescSource_in, short PrivatePharmacy_in, int *ErrorCode_out, short *DefaultSourceReject_out)

{
	short	IsValid		= 0;
	ISOLATION_VAR;

	short	PrescSource	= PrescSource_in;
	short	NumFound	= 0;
	short	PrivPharm	= 0;


	// DonR 05Mar2017: Instead of a "real" table lookup, use the in-memory version
	// stored in the PrescrSource array. Note that if the Prescription Source is
	// "found", the Prescription Source Code in the array will be equal to the
	// subscript; anything else there means that we did not read a valid row for
	// that Prescription Source.
	if ((PrescrSource[PrescSource].pr_src_code != PrescSource) || (PrivatePharmacy_in && (!PrescrSource[PrescSource].pr_src_priv_pharm)))
	{
		IsValid = 0;

		if (ErrorCode_out != NULL)
			*ErrorCode_out = ERR_PRESCRIPTION_SOURCE_REJECT;

		// No need to reject the same Prescription Source in two different ways - so
		// just set this flag to zero.
		if (DefaultSourceReject_out != NULL)
			*DefaultSourceReject_out = 0;
	}
	else
	{
		IsValid = 1;

		// Note that we assign a meaningful value to the Reject Source output flag only for
		// otherwise valid sources; we don't want to produce two different "rejected source"
		// errors!
		if (DefaultSourceReject_out != NULL)
			*DefaultSourceReject_out = (PrescrSource[PrescSource].allowed_by_default) ? 0 : 1;
	}

	return (IsValid);
}


int find_preferred_drug (int			SoldDrug_in,
						 int			EconomypriGroup_in,
						 short			PreferredFlag_in,
						 int			PreferredLargo_in,
						 short			NeedHistoryCheck_in,
						 short			SwapOutType_3_Drugs_in,
						 int			TreatmentContractor_in,
						 int			MemberId_in,
						 short			MemberIdCode_in,
						 int			*PrefDrug_out,
						 short			*SpecialistDrug_out,
						 int			*ParentGroupCode_out,
						 short			*PrefDrugMemberPrice_out,
						 short			*DispenseAsWritten_out,
						 short			*InHealthBasket_out,
						 short			*RuleStatus_out,
						 TDrugListRow	*DL_out,
						 TErrorCode		*ErrorCode_out)

{
	int				ErrorCode		= 0;
	ISOLATION_VAR;

	// Variables for Generic Drug Substitution.
	int				EP_LargoCode;
	short			EP_GroupCode;
	short			EP_GroupNumber;
	short			EP_MinGroupNbr;
	short			EP_MinItemSeq;
	TDrugListRow	DL;
	int				EP_RcvDate;
	int				EP_RcvTime;
	int				DNP_Count;
	int				DNP_TreatmentContr	= TreatmentContractor_in;
	int				DNP_MemberId		= MemberId_in;
	short			DNP_MemberIdCode	= MemberIdCode_in;
	int				DNP_LargoCode		= SoldDrug_in;
	int				DNP_ParentGroup;
	int				DF_ValidFrom;
	int				DF_ValidUntil;
	int				SysDate;
	int				ThreeMonthsAgo;
	int				FourMonthsAgo;
	int				LastPrefDrugBought;
	int				PD_EP_GroupCode		= EconomypriGroup_in;
//	long			DefaultPreferred	= PreferredLargo_in;



	// Default - drug sold/prescribed is already the preferred (generic) drug.
	// Note: No sanity check on PrefDrug_out not being NULL!
	EP_LargoCode = *PrefDrug_out = SoldDrug_in;

	// More initializations.
	EP_GroupCode			= EP_GroupNumber = 0;
	*ErrorCode_out			= 0;	// No sanity check!

	if (DispenseAsWritten_out != NULL)
		*DispenseAsWritten_out	= 0;	// Assume doctor has not sent a Form 90 ishur.


	// DonR 13Jan2005: Provide a quick exit if the drug code isn't in economypri.
	// DonR 11Mar2015: We no longer exit immediately if the default drug is preferred -
	// it's possible that the member has recently bought a different preferred drug,
	// and in that case we'll make the substitution even over another preferred drug.
	// DonR 29Jan2018: Added new parameter SwapOutType_3_Drugs_in to permit generic
	// substitution on drugs with Preferred Flag = 3. This flag is TRUE for prescription-request
	// transactions (2001, 6001) but FALSE everywhere else - meaning that these drugs can
	// be sold under their own conditions with no explanation from the pharmacy.
	if ((EconomypriGroup_in	<  1)								||
		(PreferredLargo_in	<  1)								||	// No preferred drug, or else the default drug is already the only preferred drug.
		((PreferredFlag_in	== 1) && (!NeedHistoryCheck_in))	||	// Default drug is already "preferred", and we aren't interested in alternates.
		((PreferredFlag_in	== 3) && (!SwapOutType_3_Drugs_in))	||	// Default drug is not preferred but can be sold under its own conditions.
		(PreferredFlag_in	== 4)								||	// 25Mar2007: Default drug is not preferred but has same conditions as preferred.
		(PreferredFlag_in	== 8)								||	// Default drug is not in e.p. table - but pharmacy can substitute.
		(PreferredFlag_in	== 9))									// Default drug is not in economypri table.
	{
		return (ErrorCode);	// Which is zero at this point.
	}

	// DonR 30Mar2011: No point in worrying about isolation if we're going to exit before doing anything
	// involving the database - so moved the REMEMBER call here, and got rid of the RESTORE call on the first
	// return() call just above.
	REMEMBER_ISOLATION;

	// More initialization.
	SysDate			= GetDate ();

	// Dummy loop to avoid GOTO's.
	do
	{
		SET_ISOLATION_DIRTY;

		// DonR 11Mar2015: Check member's purchase history to see if there is a different preferred
		// drug that s/he has bought recently. In this case, we'll opt for continuity.
		// DonR 24Mar2015: This history check is now conditional on the new flag NeedHistoryCheck_in,
		// which is equal to the drug_list field multi_pref_largo for Transactions 2001/6001, and is
		// always zero for transactions like 2003/5003/6003 that don't care about alternate preferred
		// drugs. This means we have to check history only at prescription-request time, and only for
		// drugs in economypri groups with more than one preferred drug.
		// DonR 18Sep2017: If member's most recent purchase was of the default preferred drug, that
		// *is* relevant for history checking - otherwise if the doctor has prescribed preferred drug
		// A and member has bought the default preferred drug B, we'll send A to the pharmacy where
		// we should send B.
		LastPrefDrugBought = 0;

		if (NeedHistoryCheck_in)
		{
			FourMonthsAgo	= IncrementDate (SysDate, -122);

			ExecSQL (	MAIN_DB, find_preferred_drug_READ_LastPrefDrugBought,
						&LastPrefDrugBought,
						&DNP_MemberId,			&PD_EP_GroupCode,	&FourMonthsAgo,
						&DNP_MemberIdCode,		END_OF_ARG_LIST							);

			// DonR 24Sep2017: If we're choosing a drug to be sent to pharmacy in Transaction 2001/6001, we
			// want to send the "most preferred" drug if there is no member history of using a different
			// preferred drug. The easiest way to do this is to treat a "not found" result as if the member
			// had previously bought the "most preferred" drug.
			if (SQLERR_code_cmp (SQLERR_not_found) == MAC_TRUE)
			{
				SQLCODE				= 0;					// So the "k'ilu" result will be accepted below.
				LastPrefDrugBought	= PreferredLargo_in;	// We'll send this in 2001/6001 even if doc
															// prescribed a different preferred drug.
			}
		}	// Need to check purchase history for alternate preferred drug.
		else
		//	History check isn't needed.
		{
			// Not really necessary, but let's be tidy!
			SQLCODE = LastPrefDrugBought = 0;
		}


		// DonR 10Apr2011: Instead of reading from economypri, we now need only to take the preferred largo
		// code that was sent to this function; the economypri logic is handled in advance by
		// As400UnixClient.
		// DonR 11Mar2015: If we found a preferred drug that the member has bought, different from
		// the default preferred drug, use that instead of the default.
		// DonR 27Feb2017: If the drug the doctor prescribed is already preferred (PreferredFlag_in	== 1)
		// and there's no relevant purchase history, don't substitute the default preferred drug for the
		// drug prescribed - just let the doctor's choice stand.
		EP_LargoCode = ((SQLCODE == 0) && (LastPrefDrugBought > 0)) ?	LastPrefDrugBought :
																		((PreferredFlag_in	== 1) ? SoldDrug_in : PreferredLargo_in);

// GerrLogMini (GerrId, "find_pref: Default %d, Pref %d, Last pref bot %d, SQLCODE %d, choosing %d.",
//			 SoldDrug_in, PreferredLargo_in, LastPrefDrugBought, SQLCODE, EP_LargoCode);

		// DonR Dec2011: Use standard read_drug() function instead of separate local logic to read drug_list.
		// DonR 08Apr2018: Added new "see deleted drugs" parameter to read_drug(). Since deleted drugs should
		// never be preferred, here we just set the parameter FALSE.
		if (!read_drug (EP_LargoCode,
						9999999,
						NULL,
						false,	// Deleted drugs are "invisible".
						&DL,
						NULL))
		{
			if (SQLERR_code_cmp (SQLERR_not_found) == MAC_TRUE)
			{
				GerrLogToFileName ("economypri_problems",
								   GerrId,
								   "find_preferred_drug: Preferred drug %d not found/deleted in drug_list!",
								   (int)EP_LargoCode);
				ErrorCode		= 1;
				break;
			}
			else
			{
				// DonR 01Nov2020: Add logging of any unexpected DB error reading drug_list.
				SQLERR_error_test ();
			}
		}	// read_drug() returned zero to indicate a problem.

		// If we get here, the preferred drug actually exists.
		*PrefDrug_out = EP_LargoCode;

		if (SpecialistDrug_out != NULL)
			*SpecialistDrug_out = DL.specialist_drug;

		if (ParentGroupCode_out != NULL)
			*ParentGroupCode_out = DL.parent_group_code;

		if (PrefDrugMemberPrice_out != NULL)
			*PrefDrugMemberPrice_out = DL.member_price_code;

		if (RuleStatus_out != NULL)
			*RuleStatus_out = DL.rule_status;

		// If drug_list specifies 100% participation, force Health
		// Basket status false.
		if (DL.member_price_code == PARTI_CODE_ONE)
			DL.health_basket_new = 0;
		if (InHealthBasket_out != NULL)
			*InHealthBasket_out = DL.health_basket_new;

		// DonR 30Oct2014: Added a new output variable for the drug_list structure, to prevent having
		// to read the same drug_list row twice.
		if (DL_out != NULL)
			*DL_out = DL;


		// DonR 13Jul2005: Check to see if the doctor has sent a dispense-as-written
		// ishur (Form 90); if so, set the return flag to indicate that the non-generic
		// drug should be treated as if it were the preferred drug.
		// DonR 24Jul2011: We don't want to read drugnotprior for Member ID of zero - so skip this
		// whole block of code.
		if ((DispenseAsWritten_out != NULL) && (DNP_MemberId > 0))
		{
			// DonR 25Mar2010: Get the valid date range for doctors' ishurim for this drug.
			// Note that the MAX function in the SELECT below shouldn't be necessary,
			// as there is never supposed to be more than one drug_forms entry for a given
			// drug for Forms 90 and 92; but just in case, the aggregate function will
			// avoid cardinality errors. We want to find the most current tofes, so we
			// select the latest dates. We're assuming that the "t'fasim" will be sequential;
			// if someone does strange things with dates, it's still possible to get strange
			// results here, but it should never happen in real life. Note that if
			// Tofes 90 is replaced by Tofes 92 (or vice versa), doctors' ishurim issued
			// through the previous tofes become invalid on the effective date of the switch.
			ThreeMonthsAgo = IncrementDate (SysDate, -91);

			// DonR 10May2010 HOT FIX: Ignore deletion flag in drug_forms; deleted rows (which must be
			// sent to Clicks as deletions) are still potentially meaningful, as they may have been
			// closed within the last 90 days.
			DeclareAndOpenCursorInto (	MAIN_DB, find_preferred_drug_drugforms_cur,
										&DF_ValidFrom,		&DF_ValidUntil,
										&DNP_LargoCode,		&SysDate,
										&ThreeMonthsAgo,	END_OF_ARG_LIST				);

			if (SQLCODE == 0)
			{
				// We're interested only in the first row fetched.
				FetchCursor (	MAIN_DB, find_preferred_drug_drugforms_cur	);
			}

			// DonR 25Mar2010: If we didnt' succeed in reading anything (for whatever reason),
			// set the date range to nonsense values (i.e. from 0 to minus 1) to disable doctors' ishurim.
			if (SQLCODE != 0)
			{
				DF_ValidFrom	= 0;
				DF_ValidUntil	= -1;
			}

			CloseCursor (	MAIN_DB, find_preferred_drug_drugforms_cur	);

			DNP_Count = 0;

			// DonR 23May2016: Per Raya Karol (CR #8077), Form 90 requests now work for all doctors and
			// for all dosages of the same medicine (defined by Parent Group Code). Treatmentcontr is
			// thus no longer part of the WHERE clause, and the Largo Code criterion includes a double
			// subquery to include Form 90 requests for different dosages of the same drug being sold.
			// DonR 31May2016: For now, revert to the previous version of this SELECT - we're still
			// waiting for a formal Change Request.
			// DonR 29Dec2019: For now, convert only the old (and still current) version to ODBC. Since
			// DrugNotPrior is more or less a dead (or slowly dying) table anyway, there isn't much
			// point to worrying about a "new" lookup method that was never put into actual use in
			// 3 1/2 years!
			ExecSQL (	MAIN_DB, find_preferred_drug_READ_drugnotprior,
						&DNP_Count,
						&DNP_MemberId,		&DNP_TreatmentContr,	&DNP_LargoCode,
						&DNP_MemberIdCode,	&DF_ValidFrom,			&DF_ValidUntil,
						END_OF_ARG_LIST													);

			// DonR 28Mar2010: If we are granting generic-drug conditions based on a tofes
			// that expired within the last 91 days (including today), return a different
			// status than we do if the tofes is still current.
			if ((SQLCODE == 0) && (DNP_Count > 0))
				*DispenseAsWritten_out = (DF_ValidUntil >= SysDate) ? 1 : 2;

			if (SQLERR_error_test ())
			{
				GerrLogToFileName ("elpresc_log",
								   GerrId,
								   "find_preferred_drug: DB error in Generic Substitution (5).");

				*ErrorCode_out	= ERR_DATABASE_ERROR;
				ErrorCode		= 1;
				break;
			}
		}	// Non-NULL dispense-as-written output pointer AND non-zero Member ID.
	}
	while (0);
	// End of Dummy Loop.


	RESTORE_ISOLATION;

	return (ErrorCode);
}



short MemberFailedToPickUpDrugs (TMemberData *Member_in)
{
	short	error_code_out	= 0;
	ISOLATION_VAR;

	int		MemberID		= Member_in->ID;
	short	MemberID_Code	= Member_in->ID_code;
	int		SysDate;
	int		StartDate;
	int		DecrementDays;
	int		NoShowCount		= 0;

	// Initialization.
	DecrementDays	= (no_show_hist_days > 0)? (1 - no_show_hist_days) : 0;	// I.e. a negative number or zero, with today counting as one day of history.
	SysDate			= GetDate ();
	StartDate		= AddDays (SysDate, DecrementDays);

	SET_ISOLATION_DIRTY;

	// DonR 01Jun2021: For now, I'm not changing anything here - but at some point, we may want
	// to add the new First Center Type value of "03" (= pickup) to the query!

	ExecSQL (	MAIN_DB, MemberFailedToPickUpDrugs_READ_NoShowCount,
				&NoShowCount,
				&MemberID,			&StartDate,			&SysDate,
				&MemberID_Code,		END_OF_ARG_LIST						);

	// No sophisticated error-trapping is needed here - if the SELECT failed, just don't do anything.
	if (SQLCODE == 0)
	{
		// Don't assign error codes that haven't been set up in sysparams. This means that we could,
		// in theory, assign the warning code even if the higher threshold has been reached, assuming
		// that the error code wasn't set > 0 in sysparams.
		if ((NoShowCount >= no_show_reject_num		) && (no_show_err_code	> 0))
		{
			error_code_out = no_show_err_code;
		}

		else
		if ((NoShowCount >= (no_show_reject_num - 1)) && (no_show_warn_code	> 0))
		{
			error_code_out = no_show_warn_code;
		}

		else error_code_out = 0;	// Redundant, since it was initialized at declaration - but paranoia is our friend!
	}	// Successful read of the number of days when member failed to pick up drugs s/he had ordered.

	// On the other hand, if there's something wrong with the SQL, log the error.
	// (Operational errors shouldn't normally happen, since we're running this
	// in "dirty read" mode.)
	else
	{
		SQLERR_error_test ();
		error_code_out = -1;
	}


	RESTORE_ISOLATION;

	return (error_code_out);
}


// Read Member Price data. This function maintains its own cache, and does not
// rely on shared-memory functions. It returns the same error values as the
// old GET_PARTICIPATING_PERCENT function, and copies the appropriate structure
// instance into the PriceRow_out parameter. If PriceCode_in is out of
// range (< 0 or > 99), the function will copy an all-zero structure, except
// for the percentage which will be set to 100%. 
int read_member_price (short				PriceCode_in,
					   Tmember_price_row	*PriceRow_out,
					   short				*Percent_out)
{
	static int				first_time = 1;
	static struct timeval	LastReadTime;
	struct timeval			TimeNow;
	int						rc;
	ISOLATION_VAR;

	static Tmember_price_row	PriceArray	[100];
	static Tmember_price_row	Price;		// DonR 25Mar2020: Does this really need to be static?


	if (first_time)
		first_time = LastReadTime.tv_sec = LastReadTime.tv_usec = 0;

	gettimeofday (&TimeNow, 0);

	// Assume that we need to re-read the member_price table every 15 minutes.
	if ((TimeNow.tv_sec - LastReadTime.tv_sec) > 900L)
	{
		// Start with cleaned-out array.
		memset ((char *)PriceArray, 0, sizeof(PriceArray));

		// Open cursor.
		REMEMBER_ISOLATION;
		SET_ISOLATION_DIRTY;

		DeclareAndOpenCursorInto (	MAIN_DB, READ_member_price_cur,
									&Price.participation_name,		&Price.calc_type_code,		&Price.member_institued,
									&Price.yarpa_part_code,			&Price.tax,					&Price.member_price_code,
									&Price.update_date,				&Price.update_time,			&Price.del_flg,
									&Price.member_price_prcnt,		&Price.max_pkg_price,		END_OF_ARG_LIST				);

		do
		{
			FetchCursor (	MAIN_DB, READ_member_price_cur	);

			if (SQLERR_code_cmp	(SQLERR_end_of_fetch))
				LastReadTime = TimeNow;	// Array was loaded OK, so we're set for 15 minutes.

			if (SQLCODE != 0)
				break;	// Done reading table, successfully or otherwise.

			// If we get here, we read something - so copy it to array position.
			// For our purposes, anything from 0 to 99 is valid - although 0 really shouldn't be.
			if ((Price.member_price_code >= 0) && (Price.member_price_code <= 99))
			{
				Price.min_reduced_price = Price.tax;	// Since "tax" is confusing.
				PriceArray[Price.member_price_code] = Price;
			}
		}
		while (1);

		// Close cursor.
		CloseCursor (	MAIN_DB, READ_member_price_cur	);

		RESTORE_ISOLATION;
	}	// Buffer needs to be loaded/refreshed.


	// At this point, we should have PriceArray loaded with all valid, current data
	// from member_price.
	if ((PriceCode_in >= 0) && (PriceCode_in <= 99))
	{
		if (PriceArray[PriceCode_in].member_price_code != PriceCode_in)	// I.e. we didn't load this row.
		{
			rc = ERR_PARTICI_CODE_NOT_FOUND;
			PriceArray[PriceCode_in].member_price_prcnt	= 10000;	// Assume 100%.

			GerrLogMini (GerrId,
						 "Read_member_price: Price Code %d not found in table - assuming 100 percent!",
						 PriceCode_in);
		}
		else	// Valid data found.
		{
			rc = NO_ERROR;
		}

		if (PriceRow_out != NULL)
			*PriceRow_out = PriceArray[PriceCode_in];

		if (Percent_out != NULL)
			*Percent_out = PriceArray[PriceCode_in].member_price_prcnt;
    }
	else	// Requested price code is out of range!
	{
		// Send a blank structure with 100% participation, and write an error to log.
		memset ((char *)&Price, 0, sizeof(Price));
		Price.member_price_prcnt = 10000;	// Assume 100%.
		rc = ERR_PARTICI_CODE_NOT_FOUND;

		GerrLogMini (GerrId,
					 "Read_member_price: Price Code %d out of range - assuming 100 percent!",
					 PriceCode_in);

		if (PriceRow_out != NULL)
			*PriceRow_out = Price;

		if (Percent_out != NULL)
			*Percent_out = Price.member_price_prcnt;
	}

	return (rc);
}


// DonR 04Dec2022 User Story #408077: Set up flags in global TMemberData structure. We
// assume that the TMemberData structure has already been initialized to all-NULL (except
// for insurance_type, which should be initialized to "N" to avoid database errors).
// Note that at least for now, we're not performing every possible test on TMemberData
// fields, since not all transactions are loading values into all structure members; this
// function *does* expect values in maccabi_code, maccabi_until, and date_of_bearth.
int SetMemberFlags (int SQLCODE_in)
{
	if (SQLCODE_in == 0)
	{
		Member.RowExists			= 1;
		Member.MemberEligible		= ((Member.maccabi_code >   0) && (Member.maccabi_until >= GlobalSysDate));
		Member.MemberLivesOverseas	= ((Member.maccabi_code == 22) || (Member.maccabi_code == 23));

		// DonR 28Dec2015: Change age logic to be more intuitive. A newborn baby's age in months will now be zero
		// until s/he is in fact a month old; in all cases, Member Age will be the number of *completed* months
		// of life. GenMonsDif() already returns the correct value - so all we have to do is stop adding 1 to it.
	    Member.age_months			= GetMonsDif (GlobalSysDate, Member.date_of_bearth);	// Should really get a spelling fix in DB and code!

		// Set Tzahal, Maccabi-member, and Meuheded-member flags.
		switch (Member.maccabi_code)
		{
			case 33:	Member.MemberHova		= Member.MemberTzahal	= 1;	break;

			case 34:	Member.MemberKeva		= Member.MemberTzahal	= 1;	break;

			default:	Member.MemberMaccabi	= 1;
						Member.MemberInMeuhedet	= ((Member.maccabi_code >= 30) && (Member.maccabi_code  <= 60));
						break;
		}

		// Set insurance type based on dates. This is required just in case a member's supplemental
		// insurance has expired but we haven't yet received an update from AS/400.
		if (	((Member.InsuranceType == 'Y') &&	((Member.yahalom_from		> GlobalSysDate) ||
													 (Member.yahalom_until		< GlobalSysDate) ||
													 (Member.yahalom_code		< 0)))					||
				((Member.InsuranceType == 'Z') &&	((Member.mac_magen_from		> GlobalSysDate) ||
													 (Member.mac_magen_until	< GlobalSysDate) ||
													 (Member.mac_magen_code		< 0)))					||
				((Member.InsuranceType == 'K') &&	((Member.keren_mac_from		> GlobalSysDate) ||
													 (Member.keren_mac_until	< GlobalSysDate) ||
													 (Member.keren_mac_code		< 0)))							)
		{
			// Expired "shaban" - treat member as Basic.
			Member.InsuranceType = 'B';
			Member.yahalom_code = Member.mac_magen_code = Member.keren_mac_code = -1;
		}
		else
		{
			// We should never have an unknown Insurance Type, since the value is set by As400UnixServer.
			// But just in case, substitute "B" (= basic insurance) for any weird value.
			if ((Member.InsuranceType < 'B') || (Member.InsuranceType > 'Z'))
			{
				Member.InsuranceType = 'B';
			}
		}

		// Set "vetek" and previous-insurance values.
		// By default, previous insurance and previous-insurance "vetek" are set to disabled values.
		Member.prev_vetek = -999;
		strcpy (Member.prev_insurance_type,	"~");

		switch (Member.InsuranceType)
		{
			case 'Y':	// Yahalom (actually "Maccabi Sheli", but they didn't tell us that
						// until all this stuff had already been coded).
						Member.current_vetek = GetMonsDif (GlobalSysDate, Member.yahalom_from);

						// If (and only if) member previously had Kesef or Zahav, set up Zahav
						// as the previous insurance, with appropriate "vetek".
						if (Member.carry_over_vetek > 0)
						{
							strcpy (Member.prev_insurance_type,	"Z");
							Member.prev_vetek = GetMonsDif (GlobalSysDate, Member.carry_over_vetek);
						}

						break;

			case 'Z':	// Zahav.
						Member.current_vetek = GetMonsDif (GlobalSysDate, Member.mac_magen_from);
						break;

			case 'K':	// Kesef.
						Member.current_vetek = GetMonsDif (GlobalSysDate, Member.keren_mac_from);
						break;

			default:	// Only basic insurance - no "vetek"!
						Member.current_vetek = 0;
						break;
		}	// Switch on current member insurance type.

		// If this member is exempt from waiting for "shaban" benefits, override computed "veteks".
		// Note that to qualify for anything, member needs vetek *and* current or previous supplemental
		// insurance - so we can safely override the veteks without checking the insurance flags.
		if ((Member.keren_wait_flag == 0) || (Member.keren_wait_flag == 2))
		{
			Member.current_vetek = Member.prev_vetek = 1440;	// I.e. "ad me'ah v'esrim".
		}

	}
	else
	{
		// Pretty much everything can be left with its zero/NULL default if nothing was
		// read from the database - but MemberMaccabi should probably get a value, and
		// we can re-initialize insurance_type to "N" even though it should already have
		// that value.
		Member.MemberMaccabi	= 1;
		Member.InsuranceType	= 'N';	// InsuranceType == insurance_type[0].
		Member.prev_vetek		= -999;
		strcpy (Member.prev_insurance_type,	"~");
	}

	// DonR 26Oct2025 User Story #429086: Add two new flags (one of which isn't really used
	// so far) to give a 0/1 indication of members' home-hospice and narcotic-limit-exemption
	// status. I give 0/1 rather than true/false values so that a 0/1 flag can be sent to
	// pharmacies without further "massaging".
	Member.InHomeHospice	= ((Member.dangerous_drug_status == 1) || (Member.dangerous_drug_status == 2)) ? 1 : 0;
	Member.NarcoLimitExempt	= ((Member.dangerous_drug_status == 1) || (Member.dangerous_drug_status == 3)) ? 1 : 0;
	Member.NarcoMaxDuration	= Member.NarcoLimitExempt ? exempt_narco_max_duration	: default_narco_max_duration;
	Member.NarcoMaxValidity	= Member.NarcoLimitExempt ? exempt_narco_max_validity	: default_narco_max_validity;


	return (0);
}


int TranslateTechnicalID (int Technical_ID_in, int *TZ_out, short *TZ_code_out, TMemberData *Member_out, int *HTTP_ResponseCode_out)
{
	// Static variables to support low-overhead repeat use of the REST service.
	static SERVICE	my_service;
	static bool		service_initialized		= false;
	static char		*my_service_name		= "TranslateTechnicalID";	// Maximum length = 40 characters.
	static cJSON	*service_JSON_output	= NULL;						// MUST be static, since we store its address only when we initialize my_service!

	// Other "generic" REST service-consumption variables.
	int				err						= 0;

	// Variables specific to this function.
	cJSON			*JSON_TZ				= NULL;
	cJSON			*JSON_TZ_Code			= NULL;
	int				TZ						= 0;
	short			TZ_code					= 0;
	bool			Got_TZ					= false;
	bool			Got_TZ_Code				= false;


	// This part of the code is "generic", except for filling in the operational URL from
	// the URL template and the text of the "JSON structure is NULL" log message.

	// The first time the function is called, set up the REST service.
	if (!service_initialized)
	{
		err = set_up_consumed_REST_service (my_service_name, &my_service, &service_JSON_output);

		// Keep the first-time flag TRUE unless set_up_consumed_REST_service() succeeds.
		if (err)
		{
			GerrLogMini (GerrId, "set_up_consumed_REST_service returned %d.", err);
		}
		else
		{
			service_initialized = 1;
		}
	}	// Service needs to be initialized.

	if (service_initialized)
	{
		// Create the actual "operational" URL to call.
		sprintf (my_service.url, my_service.url_template, Technical_ID_in);		// NON-GENERIC: Create the URL with Technical ID filled in.

		// Perform the REST service call. Static variable service_JSON_output will receive the parsed JSON results.
		err = consume_REST_service (&my_service);

		// Process results of the service call.
		if (service_JSON_output == NULL)
		{
			GerrLogMini (GerrId, "Translate Technical ID: TZ output JSON structure is NULL!");	// NON-GENERIC log message.
			err = -3;
		}
		else
		{	// We got valid JSON from the service - so let's see if it has what we want. 
			err				= -2;	// So far, we have *not* found good data in the HTTP response.

			// The "generic" code ends here - the rest (until the "return err") is function-specific.

			JSON_TZ			= cJSON_GetObjectItemCaseSensitive (service_JSON_output, "member_id");
			JSON_TZ_Code	= cJSON_GetObjectItemCaseSensitive (service_JSON_output, "member_id_code");

			if (JSON_TZ != NULL)
			{
				TZ = atoi (JSON_TZ->valuestring);
				Got_TZ = true;

				if (JSON_TZ_Code != NULL)
				{
					TZ_code = (short)atoi (JSON_TZ_Code->valuestring);
					Got_TZ_Code = true;

					err = 0;	// If we get here, we got both a Member TZ and a Member TZ Code.
				}	// Found the member_id_code tag.

				// GerrLogMini (GerrId, "TranslateTechnicalID: %d ==> TZ %d/%d.", Technical_ID_in, *TZ_out, *TZ_code_out);

			}	// Found the member_id tag.
			else
			{
				GerrLogMini (GerrId, "TranslateTechnicalID: Couldn't find member_id in JSON output - value = %s.",
								(JSON_TZ == NULL) ? "NULL" : JSON_TZ->valuestring);
			}
//	
//			cJSON_Delete (service_JSON_output);
		}	// service_JSON_output is non-NULL - successful parse of Technical ID translation service JSON output string.


		// OUTPUT TO OPTIONAL POINTERS.
		// DonR 21Nov2021: Added optional output of Member structure data; both the structure pointer and
		// the "simple" variable pointers for TZ and TZ Code are now optional (i.e. they can be NULL).

		// If a non-NULL member-structure pointer was passed in, populate it with results.
		if (Member_out != NULL)
		{
			// DonR 21Nov2021: Unconditionally copy the Technical ID into the Member structure.
			Member_out->TechnicalID = Technical_ID_in;

			if (Got_TZ)
				Member_out->ID		= Member_out->member_id			= TZ;

			if (Got_TZ_Code)
				Member_out->ID_code	= Member_out->mem_id_extension	= TZ_code;
		}

		// Output TZ Number if we found it and a non-NULL pointer was supplied.
		if ((TZ_out != NULL) && (Got_TZ))
		{
			*TZ_out = TZ;
		}

		// Output TZ Code if we found it and a non-NULL pointer was supplied.
		if ((TZ_code_out != NULL) && (Got_TZ_Code))
		{
			*TZ_code_out = TZ_code;
		}
	}	// service is initialized

	// Output HTTP Response Code if a non-NULL pointer was supplied.
	if (HTTP_ResponseCode_out != NULL)
		*HTTP_ResponseCode_out = (int)my_service.HTTP_ResponseCode;

	// Known values for curl_result are 0 = OK, -1 = curl init error, -2 = data not found in HTTP response.
	// Note that this does not take into account errors thrown by curl_easy_perform()!
	return err;
}


// Marianna 22Apr2025: User Story #391697
int EligibilityWithoutCard (	int					Member_id_in,
								short				Member_id_code_in,
								TEligibilityData	*EligibilityData_out,
								int					*HTTP_ResponseCode_out	)
{
	// Static variables to support low-overhead repeat use of the REST service.
	static SERVICE	my_service;
	static bool		service_initialized		= false;
	static char		*my_service_name		= "EligibilityWithoutCard";    // Maximum length = 40 characters.
	static cJSON	*service_JSON_output	= NULL;                        // MUST be static, since we store its address only when we initialize my_service!

	// Other "generic" REST service-consumption variables.
	int				err = 0;

	// Variables specific to this function.
	cJSON			*JSON_array					= NULL;
	cJSON			*JSON_element				= NULL;
	cJSON			*JSON_approval_type_code	= NULL;
	// cJSON        *JSON_approval_type_name	= NULL;		// Not currently used in business logic but retained for completeness
	cJSON			*JSON_valid_from			= NULL;
	cJSON			*JSON_valid_until			= NULL;
	bool			Got_EligibilityData			= false;	// To track if we find valid eligibility data

	// Marianna 05Apr2025: Check EligibilityData_out is not NULL before proceeding. If NULL abort.
	if (EligibilityData_out == NULL) 
	{
		GerrLogMini(GerrId, "EligibilityWithoutCard: NULL pointer received for EligibilityData_out — aborting.");
		err = -4; 
	}

	// This part of the code is "generic", except for filling in the operational URL from
	// the URL template and the text of the "JSON structure is NULL" log message.

	// The first time the function is called, set up the REST service.
	if ((!service_initialized) && (err >= 0))
	{
		err = set_up_consumed_REST_service(my_service_name, &my_service, &service_JSON_output);

		// Keep the first-time flag TRUE unless set_up_consumed_REST_service() succeeds.
		if (err)
		{
			GerrLogMini(GerrId, "set_up_consumed_REST_service returned %d.", err);
		}
		else
		{
			service_initialized = 1;
		}
	}    // Service needs to be initialized.

	if (service_initialized && err >= 0)
	{
		// Create the actual "operational" URL to call.
		// URL format: https://api.test.maccabi.dpx.mac.org.il:9443/esbapi/mac/v1/members/{Member_id_code_in}/{Member_id_in}/eligibilities/cardless/valid
		sprintf(my_service.url, my_service.url_template, Member_id_code_in, Member_id_in);
		
		// Marianna 27Apr2025 : Diagnostic
		//GerrLogMini(GerrId, "Eligibility Without Card: Full URL being used: %s", my_service.url);

		// Perform the REST service call. Static variable service_JSON_output will receive the parsed JSON results.
		err = consume_REST_service(&my_service);

		// Marianna 27Apr2025 : Diagnostic
		//if (my_service.curl_output_MEMORY.REST_output_buf != NULL)
		//{
		//	GerrLogMini(GerrId, "Eligibility Without Card: HTTP Response: %s", my_service.curl_output_MEMORY.REST_output_buf);
		//}
		//else 
		//{
		//	GerrLogMini(GerrId, "Eligibility Without Card: HTTP response buffer is NULL.");
		//}

		// Process results of the service call.
		if (service_JSON_output == NULL)
		{
			GerrLogMini(GerrId, "Eligibility Without Card: Eligibility output JSON structure is NULL!");
			err = -3;
		}
		else
		{    // We got valid JSON from the service - so let's see if it has what we want.
			err = -2;    // So far, we have *not* found good data in the HTTP response.

			// Check if we have an array.
//			if ((cJSON_IsArray(service_JSON_output)) && (cJSON_GetArraySize(service_JSON_output) > 0))
			if (cJSON_IsArray (service_JSON_output))
			{
				// Check if we have an array with at least one element.
				if (cJSON_GetArraySize (service_JSON_output) > 0)
				{
					Got_EligibilityData = false;

					JSON_element = cJSON_GetArrayItem (service_JSON_output, 0);

					if (JSON_element != NULL)
					{
						JSON_approval_type_code = cJSON_GetObjectItemCaseSensitive(JSON_element, "approval_type_code");
						// JSON_approval_type_name = cJSON_GetObjectItemCaseSensitive(JSON_element, "approval_type_name");	// Not used in business logic currently
						JSON_valid_from = cJSON_GetObjectItemCaseSensitive(JSON_element, "valid_from");
						JSON_valid_until = cJSON_GetObjectItemCaseSensitive(JSON_element, "valid_until");

						// Check if we have all the required fields
						// For now, we don't check: && JSON_approval_type_name != NULL
						if (JSON_approval_type_code != NULL &&
							JSON_valid_from != NULL && JSON_valid_until != NULL)
						{
							// OUTPUT 
							// Fill the output structure

							EligibilityData_out->approval_type_code = JSON_approval_type_code->valueint;

							// Copy strings with safety checks to prevent buffer overflow
							// strncpy(Eligibility_out->approval_type_name, JSON_approval_type_name->valuestring, 
							//		sizeof(Eligibility_out->approval_type_name) - 1);
							// Eligibility_out->approval_type_name[sizeof(Eligibility_out->approval_type_name) - 1] = '\0';

							// Marianna 06May2025: The string fields (valid_from, valid_until) are not currently in active use 
							// but have been retained for potential future use or debugging if needed.
							strncpy(EligibilityData_out->valid_from, JSON_valid_from->valuestring,
								sizeof(EligibilityData_out->valid_from) - 1);
							EligibilityData_out->valid_from[sizeof(EligibilityData_out->valid_from) - 1] = '\0';

							strncpy(EligibilityData_out->valid_until, JSON_valid_until->valuestring,
								sizeof(EligibilityData_out->valid_until) - 1);
							EligibilityData_out->valid_until[sizeof(EligibilityData_out->valid_until) - 1] = '\0';

							// Parse the from/until date and time
							if ((ParseISODateTime(JSON_valid_from->valuestring, &EligibilityData_out->valid_from_date, &EligibilityData_out->valid_from_time) == 0) &&
								(ParseISODateTime(JSON_valid_until->valuestring, &EligibilityData_out->valid_until_date, &EligibilityData_out->valid_until_time) == 0))
							{
								Got_EligibilityData = true;
								err = 0;    // Success!
								
								// Marianna 27Apr2025 : Diagnostic
								//GerrLogMini(GerrId, "EligibilityWithoutCard: ParseISODateTime Valid From date: %d, Valid Until date: %d", EligibilityData_out->valid_from_date, EligibilityData_out->valid_until_date);
								//GerrLogMini(GerrId, "EligibilityWithoutCard: ParseISODateTime Valid From time: %d, Valid Until time: %d", EligibilityData_out->valid_from_time, EligibilityData_out->valid_until_time);
							}
							else
							{
								GerrLogMini(GerrId, "EligibilityWithoutCard: Failed to parse valid_from/valid_until date: %s / %s",
									JSON_valid_from->valuestring, JSON_valid_until->valuestring);
							}
						}	// We got non-NULL values for Approvat Type and valid from/to dates.
						else
						{
							GerrLogMini(GerrId, "EligibilityWithoutCard: Missing required fields in JSON data.");
						}
					}	// JSON array element is non-NULL.
					else
					{
						GerrLogMini(GerrId, "EligibilityWithoutCard: Array element 0 is NULL.");
					}

					if (!Got_EligibilityData)
					{
						GerrLogMini(GerrId, "EligibilityWithoutCard: No valid eligibility data found.");
						err = -1;  // No valid entry found		
					}
				}	// JSON array has at least one element.

				else	// Got a valid array, but it's empty - NOT an error!
				{
					err = 0;	// Not an error.

					// Zero out the Approval Type Code. We shouldn't need to worry about the rest
					// of the EligibilityData structure.
					EligibilityData_out->approval_type_code = 0;	// No service-without-card approved.
				}

			}	// Got a valid JSON array.

			else	// Did NOT get a valid JSON array - this is a genuine error.
			{
				GerrLogMini(GerrId, "EligibilityWithoutCard: Response is not a JSON array.");
				err = -1;
			}

			// No need to delete JSON output as it's used as a static variable
			// cJSON_Delete(service_JSON_output);
		}   // service_JSON_output is non-NULL
	}	//service is initialized 
	
	// Output HTTP Response Code if a non-NULL pointer was supplied.
	if (HTTP_ResponseCode_out != NULL)
	{
		*HTTP_ResponseCode_out = (int)my_service.HTTP_ResponseCode;
	}

	return err;
}


// Generic functions to support consuming REST services.

int set_up_consumed_REST_service (char *ServiceNameIn, SERVICE *service, cJSON **JSON_output_ptr)
{
	int		return_value			= 0;
	short	template_length			= 0;
	char	service_desc			[ 100 + 1];
	char	scheme					[  10 + 1];
	char	username				[ 100 + 1];
	char	password				[ 100 + 1];
	char	host					[ 100 + 1];
	char	path					[ 500 + 1];
	char	method					[  10 + 1];
	char	header_name				[  40 + 1];
	char	header_value			[ 255 + 1];
	char	header_combined			[ 300 + 1];	// Actually 297, but I'm feeling generous.
	char	template_buf			[1200 + 1];
	short	num_additional_URL_bytes	= 0;
	int		GrowOutputBufferIncrement	= 0;
	int		port						= 0;

	do
	{
		// Initialize the service structure to all-NULLs. Note that we haven't
		// yet validated that the pointer is non-NULL. The reason I'm putting
		// this before the NULL check is so that if a NULL service name is
		// passed to this function, we'll still initialize the service structure
		// to all zeroes.
		if (service != NULL)
		{
			memset (service, 0, sizeof(SERVICE));
		}

		// This should never happen - but make sure nobody's calling the function
		// with a NULL structure pointer or Service Name.
		if ((ServiceNameIn == NULL) || (service == NULL))
		{
			GerrLogMini (GerrId, "set_up_consumed_REST_service: ServiceNameIn = %s, service pointer %s NULL.", ServiceNameIn, (service == NULL) ? "is" : "is not");

			return_value = -1;
			break;
		}


		// The calling routine can provide the service_url_components key as a string constant -
		// so we need to make sure it's in "variable space" before using it in a database query.
		// DonR 15Feb2022: Change from a simple strcpy() to strncpy() - just in case someone
		// tries to set up an over-long service name.
//		strcpy (service->service_name, ServiceNameIn);
		strncpy (service->service_name, ServiceNameIn, sizeof (service->service_name));
		service->service_name [sizeof (service->service_name)] = (char)0;	// Just in case the input was over-long.

		ExecSQL (	MAIN_DB, READ_service_url_components,
					&service_desc,				&scheme,
					&username,					&password,
					&host,						&port,
					&path,						&method,
					&num_additional_URL_bytes,	&GrowOutputBufferIncrement,

					&service->service_name,		END_OF_ARG_LIST				);

		if (SQLCODE != 0)
		{
			if (SQLERR_code_cmp (SQLERR_not_found) == MAC_TRUE)
			{
				GerrLogMini (GerrId, "set_up_consumed_REST_service couldn't find service \"%s\".", ServiceNameIn);
			}
			else
			{
				SQLERR_error_test ();
			}

			return_value = -1;
			break;
		}

		// Read was successful, so build the URL template out of its component parts.
		StripAllSpaces (service_desc);
		StripAllSpaces (scheme);
		StripAllSpaces (username);
		StripAllSpaces (password);
		StripAllSpaces (host);
		StripAllSpaces (path);
		StripAllSpaces (method);

		// DonR 22Dec2025: Copy Service Description into service structure so we'll get proper diagnostics.
		strcpy (service->service_desc, service_desc);

		// Scheme (normally http/https) is mandatory.
		template_length  = sprintf (template_buf, "%s:", scheme);

		// Authority has several components, and is (theoretically) optional. If there is any
		// Authority segment, the host parameter is mandatory.
		if (*host)
		{
			template_length += sprintf ((template_buf + template_length), "//");
			
			// Userinfo is optional, and may or may not include a password (which is deprecated, but still legal).
			if (*username)
			{
				template_length += sprintf (	(template_buf + template_length),
												"%s%s%s@",
												username,
												(*password) ? ":" : "",
												password							);
			}

			template_length += sprintf ((template_buf + template_length), "%s", host);

			// Port is also optional.
			if (port > 0)
			{
				template_length += sprintf ((template_buf + template_length), ":%d", port);
			}
		}	// End of Authority component.

		// The path is also optional; if present, it always begins with a slash. Note that the path
		// read from the service_url_components table includes any query and "fragment" components,
		// with any variables to be inserted by the calling routine formatted as C sprintf masks.
		if (*path)
		{
			if (*path != '/')
			{
				template_length += sprintf ((template_buf + template_length), "/");
			}
			
			template_length += sprintf ((template_buf + template_length), "%s", path);
		}

		// At this point, the URL template should be complete and ready for use. Allocate URL
		// buffers and copy the assembled URL template into service.url_template. (The calling
		// routine will use this to sprintf to service.url itself.)
		service->url_template	= MemAllocReturn (	GerrId, template_length + 1,
													"Consumed REST service URL template");

		service->url			= MemAllocReturn (	GerrId, template_length + num_additional_URL_bytes + 1,
													"Consumed REST service URL");

		if ((service->url_template == NULL) || (service->url == NULL))
		{
			return_value = -1;
			break;
		}

		strcpy (service->url_template, template_buf);


		// Translate the http/https method to a single-character code (just to keep code
		// simple at execution time). For now at least, we support only Get and Post, and
		// we're not even looking for a full-word match. If it becomes necessary to support
		// other methods - particularly PUT or PATCH - we'll need to change this logic.
		switch (*method)
		{
			case 'P':
			case 'p':	service->http_method = 'P';	break;

			case 'G':
			case 'g':
			default:	service->http_method = 'G';	break;
		}


		// DonR 22Apr2025 User Story #391697: Add capability of loading custom HTTP headers
		// from the new table service_header_values.
		DeclareAndOpenCursorInto (	MAIN_DB, READ_service_http_headers,
									&header_name,				&header_value,
									&service->service_name,		END_OF_ARG_LIST		);

		if (SQLCODE != 0)
		{
			SQLERR_error_test ();
			return_value = -1;
			break;
		}

		// Loop through any HTTP header rows we find for this service, and add
		// them to service.HeaderList.
		while (1)
		{
			// Fetch an HTTP header row.
			FetchCursor (	MAIN_DB, READ_service_http_headers	);

			if (SQLERR_code_cmp (SQLERR_end_of_fetch) == MAC_TRUE)
			{
				break;
			}

			else
			if (SQLERR_error_test () == MAC_TRUE)
			{
				return_value = -1;
				break;
			}

			else	// Successful read of an HTTP header row.
			{
				StripAllSpaces (header_name);
				StripAllSpaces (header_value);
				
				// Marianna 23Apr2025 Temporary diagnostic
				//if (!TikrotProductionMode) GerrLogMini(GerrId, "Loading HTTP headers: After stripping spaces - name: '%s', value: '%s'", header_name, header_value);

				if (*header_name)	// There shouldn't be any blank header names - but if there are, ignore them.
				{
					// Note that there is an option to send "header_name:" (with a colon and
					// without a "payload" value) to delete a default HTTP header. At least
					// for now, we are *not* supporting this option, as it's unlikely to be
					// needed for our purposes. We do, however, support "empty" header values.

					if (*header_value)	// Normal headers are sent in the form "name: value".
						sprintf (header_combined, "%s: %s", header_name, header_value);
					else				// Headers without a value are sent in the form "name;" (with a semicolon).
						sprintf (header_combined, "%s;", header_name);

					// Marianna 23Apr2025 Temporary diagnostic
					//GerrLogMini(GerrId, "Loading HTTP headers: Creating header with value: '%s'", header_combined);

					 service->HeaderList = curl_slist_append (service->HeaderList, header_combined);
				}	// Non-blank HTTP header name.
			}	// Successful read of service_header_values row.
		}	// Loop through service_header_values rows for this service.

		CloseCursor (	MAIN_DB, READ_service_http_headers	);
		// At this point service.HeaderList should be loaded with any HTTP header values we need to send.


		// Set up the service's output buffer increment size.
		service->curl_output_MEMORY.GrowBufferIncrement = GrowOutputBufferIncrement;

//		// If a non-NULL JSON pointer was passed in, store it as the JSON result address.
//		if (JSON_output_ptr != NULL)
		// Store the JSON pointer that was passed in, even if it's NULL - that will
		// just prevent consume_REST_service() from trying to parse what it gets
		// as JSON output.
		service->JSON_output = JSON_output_ptr;

	} while (0);

// GerrLogMini (GerrId, "%s resolves to URL template \"%s\", method = %s, return_value %d.", service->service_name, service->url_template, (service->http_method == 'P') ? "POST" : "GET", return_value);
	return (return_value);
}


int consume_REST_service (SERVICE *service)
{
	int				curl_result		= 0;
	int				NewBufferSize	= 0;
	bool			ClearBuffer		= true;
	static char		curl_errbuf		[CURL_ERROR_SIZE + 1];
	char			*JSON_ErrPtr;


	do
	{
		// Make at least some attempt to check that the incoming SERVICE structure
		// has been properly initialized before trying to use it.
		if ((service->service_name	[0] == (char)0)		||
			(service->url			[0] == (char)0))
		{
			GerrLogMini (GerrId, "consume_REST_service: SERVICE structure is not properly initialized.");
				curl_result = -1;
				break;
		}

		// The first time this function is called for a given SERVICE instance,
		// perform the required initializations.
		if (service->curl_instance == NULL)
		{
			service->curl_instance = curl_easy_init ();

			if (service->curl_instance)
			{
				// These options can be set once, the first time we establish the connection.
				curl_result = curl_easy_setopt	(service->curl_instance, CURLOPT_ERRORBUFFER,	curl_errbuf);
				curl_result = curl_easy_setopt	(service->curl_instance, CURLOPT_WRITEFUNCTION,	REST_CurlMemoryCallback);
				curl_result = curl_easy_setopt	(service->curl_instance, CURLOPT_WRITEDATA,		(void *)&service->curl_output_MEMORY);

				// For now, we're supporting only POST and GET.
				if (service->http_method == 'P')
				{
					// If the calling routine hasn't set up a minimum starting buffer-size
					// value, assume a "reasonable" value of 1000 bytes. Note that whatever
					// the starting value is, it will get updated if we see a JSON string
					// with a greater length.
					if (service->JSON_print_buffer_length <= 0)
					{
						service->JSON_print_buffer_length = 1000;
					}

					// All the other POST stuff is dependent on the actual data that
					// is passed in - so it takes place outside the initialization
					// section.
				}	// Initialization for POST.
				else
				{
					// Curl already defaults to GET, but set it explicitly just to be thorough.
					curl_result = curl_easy_setopt	(service->curl_instance, CURLOPT_HTTPGET, 1L);
				}	// Initialization for GET.

			}	// Successful creation of curl instance.
			else
			{
				GerrLogMini (GerrId, "consume_REST_service: curl_easy_init() returned NULL!");
				curl_result = -1;
				break;
			}
		}	// Need to create and set up curl instance.


		// If we get here, there should be a valid curl instance.

		// For now, we're supporting only POST and GET. If we're doing a GET,
		// we don't have to do anything special after the first initialization.
		// On the other hand, most of the POST stuff happens here.
		if (service->http_method == 'P')
		{
			// Data to be posted comes in two flavors: JSON and string. If the calling
			// function has specified JSON data, we need to print it out into string
			// format before posting it.
			if (service->JSON_post_data != NULL)
			{
				// To minimize memory use, we'll create this output without pretty formatting.
				service->char_post_data = cJSON_PrintBuffered (service->JSON_post_data, service->JSON_print_buffer_length, 0, &NewBufferSize);

				// If cJSON had to expand the buffer, remember the new buffer size for next time.
				if (NewBufferSize > service->JSON_print_buffer_length)
				{
					service->JSON_print_buffer_length = NewBufferSize;
				}

				// This should be redundant if cJSON_PrintBuffered() worked properly - but it's
				// harmless and one should never trust that anything is properly initialized!
				ClearBuffer = (service->char_post_data != NULL);	// So we don't try to deallocate a non-allocated value.
			}	// There is JSON data to POST.

			else	// If we are POSTing non-JSON data, we don't want to clear the
					// buffer - it's the responsibility of whoever allocated it.
			{
				ClearBuffer = false;
			}

			// Just in case there was a problem creating the text version of the
			// JSON object, log a diagnostic and set the buffer pointer to an
			// empty (constant) string. (This could also be true if the calling
			// routine simply failed to provide either a non-NULL JSON pointer
			// or a non-NULL char pointer to POST.)
			if (service->char_post_data == NULL)
			{
				GerrLogMini (	GerrId, "consume_REST_service for %s: char_post_data%s is NULL!",
								service->service_desc, (service->JSON_post_data != NULL) ? " from JSON object" : "");

				service->char_post_data	= "";
				ClearBuffer				= false;	// Redundant, but harmless.
			}

			curl_result = curl_easy_setopt (service->curl_instance, CURLOPT_POSTFIELDS, service->char_post_data);
		}	// Http Method == POST.


		// If the URL has any variable information, the calling routine is responsible for creating
		// the ready-to-use URL using sprintf (or whatever else it wants). If this has not been done,
		// assume that we're using a fixed URL specified in service->url_template.
		if (service->url[0] == (char)0)
		{
			strcpy (service->url, service->url_template);
		}

		// Finally, notify curl of the URL and execute the call!
		// DonR 22Apr2025 User Story #391697: Add the ability to send custom HTTP headers,
		// configured in the new table service_header_values.
		REST_CurlMemoryClear (&service->curl_output_MEMORY);				// Clear the output buffer for the new service call.

		curl_result = curl_easy_setopt	(	service->curl_instance,
											CURLOPT_URL,
											service->url				);	// Notify curl that this will be the URL to use.

		curl_result = curl_easy_setopt	(	service->curl_instance,
											CURLOPT_HTTPHEADER,
											service->HeaderList			);	// Include any custom HTTP headers.

		curl_result = curl_easy_perform	(	service->curl_instance		);	// Perform the HTTP call.

		if (curl_result)
		{
			GerrLogMini (	GerrId, "consume_REST_service for %s: curl_easy_perform returned %d / %s, errbuf = [%s].",
							service->service_desc, curl_result, curl_easy_strerror(curl_result), curl_errbuf);
		}
		else
		{
			curl_easy_getinfo (service->curl_instance, CURLINFO_RESPONSE_CODE, &service->HTTP_ResponseCode);
		}

		if (service->JSON_output != NULL)
		{
			// DonR 15Feb2022: Delete any existing version of the JSON output before
			// parsing new data. This way the calling routine doesn't have to worry
			// about performing the cJSON_Delete(), as long as it keeps using the
			// same (static) cJSON pointer variable for its JSON data.
			if (*service->JSON_output != NULL)
				cJSON_Delete (*service->JSON_output);

			*service->JSON_output = cJSON_Parse (service->curl_output_MEMORY.REST_output_buf);

			if (*service->JSON_output == NULL)
			{
				JSON_ErrPtr = (char *)cJSON_GetErrorPtr ();
				GerrLogMini (GerrId, "consume_REST_service for %s: cJSON_Parse() error before %s,\nHTTP output = %s.",
							 service->service_desc, (JSON_ErrPtr == NULL) ? "NULL" : JSON_ErrPtr, service->curl_output_MEMORY.REST_output_buf);
			}
		}

	} while (0);

	// Known values for curl_result are 0 = OK, -1 = curl init error, -2 = data not found in HTTP response.
	// Note that this does not take into account errors thrown by curl_easy_perform()!
	return curl_result;
}


static size_t REST_CurlMemoryCallback (void *contents, size_t size, size_t nmemb, void *userp)
{
	REST_MEMORY	*MEM		= (REST_MEMORY *)userp;
	size_t		AddSize		= size * nmemb;
	size_t		NewLength;
	bool		FirstTime	= (MEM->REST_output_buf == NULL);
	
	// Calling routine should set up MEM->grow_buf_increment based on the expected
	// size range of the data to be retrieved from the REST service - but just in
	// case somebody forgets to do so, force in a value of 1000 bytes.
	if (MEM->GrowBufferIncrement < 1)
	{
		MEM->GrowBufferIncrement = 1000;
	}

	// NewLength is the actual resulting "strlen length" of the REST data after we append the new stuff.
	NewLength = MEM->LengthUsed + AddSize;

	// If the buffer needs to be expanded, do it now.
	if (NewLength >= MEM->SizeAllocated)
	{
		// Set up the new buffer size.
		while (MEM->SizeAllocated < NewLength)
			MEM->SizeAllocated += MEM->GrowBufferIncrement;

		// Note that realloc() should work the same as malloc() when the
		// initial value of MEM->REST_output_buf is NULL.
		MEM->REST_output_buf = realloc (MEM->REST_output_buf, MEM->SizeAllocated + 1);

		// DonR 22May2025: Just to avoid excess log messages, write the "added N bytes"
		// message only if this is *not* the first allocation of this memory buffer.
		if (!FirstTime)
			GerrLogMini (GerrId, "REST_CurlMemoryCallback: Added %d bytes, %d/%d bytes used.", AddSize, NewLength, MEM->SizeAllocated);
	}

	// Now append the new stuff to the buffer and then update LengthUsed.
	memcpy (MEM->REST_output_buf + MEM->LengthUsed, contents, AddSize);
	MEM->LengthUsed = NewLength;
	MEM->REST_output_buf [MEM->LengthUsed] = (char)0;

	return AddSize;
}


// Clear the current contents of a REST output memory structure, leaving the buffer allocated.
static void REST_CurlMemoryClear (REST_MEMORY *MEM)
{
	// If the buffer has yet to be initialized, there's no need to do anything.
	if (MEM->REST_output_buf != NULL)
	{
		MEM->LengthUsed				= 0;
		MEM->REST_output_buf [0]	= (char)0;
	}

	return;
}


/*=======================================================================
||				  -- EOF --												||
 =======================================================================*/
