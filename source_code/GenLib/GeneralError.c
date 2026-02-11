/*=======================================================================
||									||
||				GeneralError.c				||
||									||
||----------------------------------------------------------------------||
||									||
|| Date : 02/05/96							||
|| Written By : Ely Levy ( reshuma )					||
||									||
||----------------------------------------------------------------------||
||									||
||  Purpose:								||
||  --------								||
||  	library routines for error handling.				||
||	Error handling should be done only				||
||	with these library routines.					||
||									||
||----------------------------------------------------------------------||
||									||
|| 				Include files				||
||									||
 =======================================================================*/

#ifdef _LINUX_
	#include "../Include/Global.h"
#else
	#include "Global.h"
#endif

static char	*GerrSource = __FILE__;

char * GetDbTimestamp ();

// DonR 24Sep2024: Add a local function prototype to avoid compiler warning on new Dev server.
int create_log_directory (char *filename_in);


/*=======================================================================
||                                                                      ||
||                      Private parameters                              ||
||                                                                      ||
||      CAUTION : DO NOT !!! CHANGE THIS PARAGRAPH -- IMPORTANT !!!     ||
||                                                                      ||
 =======================================================================*/

// DonR December 2002: Revamped error format. The new version is smaller and "sleeker".
static	char
	*ErrorFmt =			/* Format of error messages     */
	"\nMESSAGE:\n"
	"   PID  %d\tUser: %s\tWorking dir: %s\n"
	"   From %s Line %d at %s"
	"   Message:  %s\n"
	"---------------------------------------------------------------\n"
	,
       	*TitleFmt =			/* Format of titled messages    */
       	"\n\n*******************<<< START OF MESSAGE >>>********************\n"
       	"\t    TITLE : %s\n"
       	"\t    %.*s\n"
       	"\tFrom pid . . . . . . : %d\n"
       	"\tRequest from username: %s\n"
       	"\tWorking directory  . : %s\n"
       	"\tTime . . . . . . . . : %s\n"
       	"\tCurrent source file. : %s\n"
       	"\tLine number. . . . . : %d\n"
       	"\tMessage. . . . . . . : %s\n"
       	"********************<<< END OF MESSAGE >>>*********************\n"
       	,
	*minuses =
	"-------------------------------------"
	"-------------------------------------"
	"-------------------------------------"
	"-------------------------------------"
	"-------------------------------------"
	"-------------------------------------"
	"-------------------------------------"
	"-------------------------------------"
	;
 
/*=======================================================================
||									||
||			     GerrLogExit()				||
||									||
||  Print error message on standard error and exit with error status	||
||									||
 =======================================================================*/

// NOTE: This function may be causing segmentation faults. At some point,
// the code should be checked to see if there is in fact a bug here. (It's
// not urgent, since GerrLogExit() should never really be called under
// normal circumstances.)

void GerrLogExit(
		 char *source,		/* Source name from  (GERR_ID)	*/
		 int	 line,		/* Line in source from  -"-	*/
		 int  status,		/* Exit status for exit()	*/
		 char *printf_fmt,	/* Format for printf		*/
		 ...			/* arguments for printf		*/
		 )
{

/*
 * Local variables
 * ---------------
 */
    char	curr_dir[256];
    va_list	printf_args;
//    time_t	err_time;
    FILE	*fp;

/*
 * Get time & other variables
 * --------------------------
 */
//    time( &err_time );
    va_start( printf_args, printf_fmt );
    vsprintf( buf, printf_fmt, printf_args );

/*
 * Open log file for append
 * ------------------------
 */
    fp = open_log_file();

/*
 * Put error message into stderr
 * -----------------------------
 */
    // DonR December 2002: Revamped error format. The new version is smaller and "sleeker".
    fprintf(
	    fp,
	    ErrorFmt,
	    getpid(),
	    getlogin(),
	    getcwd( curr_dir, 256 ),
	    source,
	    line,
//	    ctime( &err_time ),
	    GetDbTimestamp (),
	    buf
	    );

/*
 * Close log file
 * --------------
 */
    close_log_file( fp );

/*
 * Exit with status
 * ----------------
 */
    va_end( printf_args );
    exit( status );
    _exit( status );

}

/*=======================================================================
||									||
||			     GerrLogReturn()				||
||									||
||	Print error message on standard error and return		||
||									||
 =======================================================================*/
void GerrLogReturn(
		   char *source,	/* Source name from  (GERR_ID)	*/
		   int	 line,		/* Line in source from  -"-	*/
		   char *printf_fmt,	/* Format for printf		*/
		   ...			/* arguments for printf		*/
		   )
{

/*
 * Local variables
 * ---------------
 */
    char	curr_dir[256];
    va_list	printf_args;
//    time_t	err_time;
    FILE	*fp;

/*
 * Get time & other variables
 * --------------------------
 */
//    time( &err_time );
    va_start( printf_args, printf_fmt );
    vsprintf( buf, printf_fmt, printf_args );

/*
 * Open log file for append
 * ------------------------
 */
    fp = open_log_file();

/*
 * Put error message into log file
 * -------------------------------
 */
    // DonR December 2002: Revamped error format. The new version is smaller and "sleeker".
    fprintf(
	    fp,
	    ErrorFmt,
	    getpid(),
	    getlogin(),
	    getcwd( curr_dir, 256 ),
	    source,
	    line,
//	    ctime( &err_time ),
	    GetDbTimestamp (),
	    buf
	    );

/*
 * Close log file
 * --------------
 */
    close_log_file( fp );

/*
 * return
 * ------
 */
    va_end( printf_args );

	// DonR 06May2021: Since these errors are normally "fatal" (and should occur almost never),
	// add a 1/4-second delay before returning to the calling routine. This will prevent "doom
	// loop" situations from filling up the whole disk drive with endless error messages - four
	// per second is still plenty if we're in an endless loop, but it won't push the logfile
	// to multiple gigabytes in any hurry. And 1/4 second isn't long enough to cause timeouts
	// in situations where we hit a serious error but we're *not* in a "doom loop".
	usleep (250000);	// Sleep 250 milliseconds before returning, so if we're in some kind of
						// "doom loop" we won't fill up the whole disk with endless log messages.

    return;

}



/*=======================================================================
||																		||
||			     GerrLogMini ()											||
||																		||
||	Print message (with minimal frills) on standard error and return	||
||																		||
 =======================================================================*/
void GerrLogMini (char	*source,		// For compatibility only.
				  int	line,			// For compatibility only.
				  char	*printf_fmt,	// Format for printf.
				  ...					// Arguments for printf.
				 )
{
	va_list	printf_args;
	time_t	err_time;
	char	timebuf [50];
	FILE	*fp;

	// Initialization.
	time (&err_time);
	sprintf (timebuf, ctime (&err_time));
//	timebuf [strlen(timebuf) - 6] = (char)0;	// Get rid of year.
	timebuf [strlen(timebuf) - 15] = (char)0;	// Get rid of year AND time.
	
	va_start (printf_args, printf_fmt);
	vsprintf (buf, printf_fmt, printf_args);


	// Open log file for append, write message, and close.
	fp = open_log_file ();
	fprintf (fp, "%s - %s %s\n", buf, timebuf + 4, GetDbTimestamp () ); // Start after day-of-week.
	close_log_file (fp);

	// All done!
	va_end (printf_args);
	return;
}



/*=======================================================================
||																		||
||			     GerrLogAddBlankLines ()								||
||																		||
||	Add N blank lines to standard error and return						||
||																		||
 =======================================================================*/
void GerrLogAddBlankLines ( int NumLines_in )
{
	FILE	*fp;
	int		i;

	// Open log file for append, write message, and close.
	fp = open_log_file ();

	for (i = 0; i < NumLines_in; i++)
		fprintf (fp, "\n");

	close_log_file (fp);

	// All done!
	return;
}



/*=======================================================================
||																		||
||			     GerrLogFnameMini ()									||
||																		||
||	Print message (with minimal frills) to specified file and return	||
||																		||
 =======================================================================*/
void GerrLogFnameMini (char *FileName,
					   char	*source,		// For compatibility only.
					   int	line,			// For compatibility only.
					   char	*printf_fmt,	// Format for printf.
					   ...					// Arguments for printf.
					   )
{
	va_list	printf_args;
	time_t	err_time;
	char	timebuf [50];
	FILE	*fp;

	// Initialization.
	time (&err_time);
	sprintf (timebuf, ctime (&err_time));
//	timebuf [strlen(timebuf) - 6] = (char)0;	// Get rid of year.
	timebuf [strlen(timebuf) - 15] = (char)0;	// Get rid of year AND time.
	
	va_start (printf_args, printf_fmt);
	vsprintf (buf, printf_fmt, printf_args);

	// Open specified file for append, write message, and close.
	fp = open_other_file (FileName, "a");
	fprintf (fp, "%s - %s %s\n", buf, timebuf + 4, GetDbTimestamp () ); // Start after day-of-week.

	close_log_file (fp);

	// All done!
	va_end (printf_args);
	return;
}



/*=======================================================================
||																		||
||			     GerrLogFnameClear ()									||
||																		||
||	Clear specified file (with short message) and return				||
||																		||
 =======================================================================*/
void GerrLogFnameClear (char *FileName)
{
	time_t	err_time;
	FILE	*fp;

	// Initialization.
	time (&err_time);

	// Open specified file for output, write message, and close.
	fp = open_other_file (FileName, "w");
	fprintf (fp, "File cleared at %s\n", ctime (&err_time));

	close_log_file (fp);

	// All done!
	return;
}



/*=======================================================================
||																		||
||			     GerrLogToFileName ()									||
||																		||
||	Print error message to specified file and return					||
||																		||
 =======================================================================*/
void GerrLogToFileName	(	char *FileName,
							char *source,		/* Source name from  (GERR_ID)	*/
							int	 line,			/* Line in source from  -"-	*/
							char *printf_fmt,	/* Format for printf		*/
							...					/* arguments for printf		*/
						)
{
	// Local variables
	char	curr_dir[256];
	va_list	printf_args;
//	time_t	err_time;
	FILE	*fp;

	// Get time & other variables
//	time (&err_time);

	va_start (printf_args, printf_fmt);
	vsprintf (buf, printf_fmt, printf_args);

	// Open specified file for append
	fp = open_other_file (FileName, "a");

	// Put message into file
	fprintf (fp,
			 ErrorFmt,
			 getpid		(),
			 getlogin	(),
			 getcwd		(curr_dir, 256),
			 source,
			 line,
//			 ctime( &err_time ),
			 GetDbTimestamp (),
			 buf
			);

	// Close file
	close_log_file (fp);

	// Return
	va_end (printf_args);

	// DonR 06May2021: Since these errors are normally "fatal" (and should occur almost never),
	// add a 1/4-second delay before returning to the calling routine. This will prevent "doom
	// loop" situations from filling up the whole disk drive with endless error messages - four
	// per second is still plenty if we're in an endless loop, but it won't push the logfile
	// to multiple gigabytes in any hurry. And 1/4 second isn't long enough to cause timeouts
	// in situations where we hit a serious error but we're *not* in a "doom loop".
	usleep (250000);	// Sleep 250 milliseconds before returning, so if we're in some kind of
						// "doom loop" we won't fill up the whole disk with endless log messages.

	return;
}



/*=======================================================================
||									||
||			     GmsgLogExit()				||
||									||
||	Print message with title on standard error and exit		||
|| 	 with error status					        ||
||									||
 =======================================================================*/
void GmsgLogExit(
		 char *source,		/* Source name from  (GERR_ID)	*/
		 int	 line,		/* Line in source from  -"-	*/
		 int  status,		/* Exit status for exit()	*/
		 char *title,		/* Title for message		*/
		 char *printf_fmt,	/* Format for printf		*/
		 ...			/* arguments for printf		*/
		 )
{

/*
 * Local variables
 * ---------------
 */
    char	curr_dir[512];
    va_list	printf_args;
//    time_t	err_time;
    FILE	*fp;

/*
 * Get time & other variables
 * --------------------------
 */
//    time( &err_time );
    va_start( printf_args, printf_fmt );
    vsprintf( buf, printf_fmt, printf_args );

/*
 * Open log file for append
 * ------------------------
 */
    fp = open_log_file();

/*
 * Put error message into lof file
 * -------------------------------
 */
    fprintf(
	    fp,
	    TitleFmt,
	    title,
	    8 + strlen(title),
	    minuses,
	    getpid(),
	    getlogin(),
	    getcwd( curr_dir, 256 ),
//		ctime( &err_time ),
		GetDbTimestamp (),
	    source,
	    line,
	    buf
	    );

/*
 * Close log file
 * --------------
 */
    close_log_file( fp );

/*
 * Exit with status
 * ----------------
 */
    va_end( printf_args );
    exit( status );
    _exit( status );

}

/*=======================================================================
||									||
||			     GmsgLogReturn()				||
||									||
||	Print message with title on standard error and return		||
||									||
 =======================================================================*/
void GmsgLogReturn(
		   char *source,	/* Source name from  (GERR_ID)	*/
		   int	 line,		/* Line in source from  -"-	*/
		   char *title,		/* Title of message		*/
		   char *printf_fmt,	/* Format for printf		*/
		   ...			/* arguments for printf		*/
		   )
{

/*
 * Local variables
 * ---------------
 */
    char	curr_dir[256];
    va_list	printf_args;
//    time_t	err_time;
    FILE	*fp;

/*
 * Get time & other variables
 * --------------------------
 */
//    time( &err_time );
    va_start( printf_args, printf_fmt );
    vsprintf( buf, printf_fmt, printf_args );

/*
 * Open log file for append
 * ------------------------
 */
    fp = open_log_file();

/*
 * Put error message into log file
 * -------------------------------
 */
    fprintf(
	    fp,
	    TitleFmt,
	    title,
	    8 + strlen(title),
	    minuses,
	    getpid(),
	    getlogin(),
	    getcwd( curr_dir, 256 ),
		GetDbTimestamp (),
	    source,
	    line,
		buf
	    );

/*
 * Close log file
 * --------------
 */
    close_log_file( fp );

/*
 * return
 * ------
 */
    va_end( printf_args );
    return;

}

/*=======================================================================
||									||
||				open_log_file()				||
||									||
||			Open log file for append			||
||									||
 =======================================================================*/
FILE	*open_log_file( void )
{

/*
 * Local variables
 * ---------------
 */
    char	file_name[512];
    int		ret;
    FILE	*fp;

/*
 * Reset user mask
 * ---------------
 */
    ret = umask( 0 );

#ifdef ALPHA

    if( ret < 0 )
    {
	printf(
	       "Can't clear user mask.Error (%d) %s",
	       GERR_str
	       );

	exit( -1 );
    }

#endif

/*
 * Open log file for append
 * ------------------------
 */
    /*
     * Return stderr if either log dir / log file
     * aren't defined
     */
    if( ! LogDir[0] ||
	! LogFile[0] )
	return( stderr );

    sprintf(
	    file_name,
	    "%s/%s",
	    LogDir,
	    LogFile
	    );

    if( (fp = fopen( file_name, "a" )) == NULL )
    {
	printf(
	       "Can't open log file %s.Error (%d) %s",
	       LogFile,
	       GerrStr
	       );

	exit( -1 );
    }

/*
 * Return file pointer
 * -------------------
 */
    return( fp );

}

/*=======================================================================
||									||
||				close_log_file()			||
||									||
||				Close log file				||
||									||
 =======================================================================*/
void	close_log_file(
		       FILE *fp			/* Log file descriptor	*/
		       )
{

/*
 * Close log file for append
 * -------------------------
 */
    fflush( fp );

    if( fp != stderr )
	fclose( fp );

}



/*=======================================================================
||																		||
||				open_other_file()										||
||																		||
||			Open a file other than the "normal" log file for append		||
||																		||
 =======================================================================*/
FILE	*open_other_file (char *fname, char *open_mode)
{
	// Local variables
	char	file_name[512];
	int		ret;
	FILE	*fp;

	// Reset user mask
	ret = umask (0);

#ifdef ALPHA
	if (ret < 0)
	{
		printf ("Can't clear user mask.Error (%d) %s", GERR_str);
		exit( -1 );
	}
#endif

	// Open file for append
	// Return stderr if log dir or file name aren't defined.
	if ((!LogDir [0]) || (fname == NULL) || (!fname [0]))
		return (stderr);

	sprintf (file_name, "%s/%s", LogDir, fname);

	if ((fp = fopen (file_name, open_mode)) == NULL)
	{
		// Before giving up, try creating the directory path and make
		// another attempt to open the log file.
		ret = create_log_directory (file_name);
		if (ret == 0)
		{
			fp = fopen (file_name, open_mode);
		}

		if (fp == NULL)
		{
			printf ("Can't open specified file %s in %s. Error (%d) %s",
					fname, LogDir, GerrStr);
			exit (-1);
		}
	}

	// Return file pointer
	return (fp);
}



// create_log_directory() - Create appropriate directory path for logfile.

int create_log_directory (char *filename_in)
{
	char	dirpath		[300];
	char	system_cmd	[400];
	char	*last_slash;
	int		result;
	int		len;


	last_slash	= strrchr (filename_in, '/');
	if (last_slash == NULL)
		return (-1);

	len			= last_slash - filename_in;
	if ((len < 1) || (len > 299))
		return (-1);

	strncpy (dirpath, filename_in, len);
	dirpath [len] = (char)0;
	sprintf (system_cmd, "mkdir -m777 -p %s", dirpath);

	result = system (system_cmd);

	return (result);
}
