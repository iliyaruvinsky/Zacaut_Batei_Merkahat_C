/*=======================================================================
||									||
||				Memory.c				||
||									||
||----------------------------------------------------------------------||
||									||
|| Date : 19/05/96							||
|| Written by : Ely Levy ( reshuma )					||
||									||
||----------------------------------------------------------------------||
||									||
|| Purpose :								||
||	Library routines for handling memory buffers. 			||
||	Memory buffers handling should					||
||	be only with this library routines.				||
||									||
||----------------------------------------------------------------------||
||									||
|| 			Include files					||
||									||
 =======================================================================*/

#ifdef _LINUX_
	#include "../Include/Global.h"
	#include "../Include/GenSql.h"	// DonR: Added 24Sep2024 to avoid compiler warnings on new Dev server.
#else
	#include "Global.h"
#endif

static char	*GerrSource = __FILE__;

#define	DEBUG2

/*=======================================================================
||                                                                      ||
||                      Private parameters                              ||
||                                                                      ||
||      CAUTION : DO NOT !!! CHANGE THIS PARAGRAPH -- IMPORTANT !!!     ||
||                                                                      ||
 =======================================================================*/

static	char
	**EnvNames = NULL,		/* environment names buffer	*/
	**EnvValues = NULL;		/* environment values buffer	*/

static	int
	EnvCount = 0,			/* environment variables count	*/
	EnvSize = 20;			/* environment buffer size	*/

/*=======================================================================
||									||
||			      MemAllocExit().				||
||									||
|| 		Allocate requested amount of memory.			||
|| 		On error - put message in log file and exit.		||
 =======================================================================*/
void	*MemAllocExit(
		      char *source_name,	/* source name		*/
		      int  line_no,		/* line number		*/
		      size_t size,		/* size to allocate	*/
		      char *Memfor		/* memory description	*/
		      )
{
    void  *ptr;

    if( (ptr = malloc( size )) == NULL )
    {
	GerrLogExit(
		    source_name,
		    line_no,
		    -1,
		    "Can't get memory for : %s. (size=%d)",
		    Memfor,
		    size
		    );
    }
    return( ptr );

}

/*=======================================================================
||									||
||			      MemReallocExit().				||
||									||
|| 		Reallocate requested amount of memory.			||
|| 		On error - put message in log file and exit.		||
 =======================================================================*/
void	*MemReallocExit(
			char *source_name,	/* source name		*/
			int  line_no,		/* line number		*/
			void *old_ptr,		/* original memory ptr	*/
			size_t new_size,	/* new size to allocate	*/
			char *mem_for		/* memory description	*/
			)
{
    void  *ptr;

    if( (ptr = realloc( old_ptr,
			new_size
			)) == NULL )
    {
	GerrLogReturn(
		    source_name,
		    line_no,
		    "Can't realloc memory for : %s. (size=%d)",
		    mem_for,
		    new_size
		    );
    }
    return( ptr );

}

/*=======================================================================
||									||
||			      MemAllocReturn().				||
||									||
|| 		Allocate requested amount of memory.			||
|| 		On error - put message in log file and return.		||
 =======================================================================*/
void	*MemAllocReturn(
			char *source_name,	/* source name		*/
			int  line_no,		/* line number		*/
			size_t size,		/* size to allocate	*/
			char *mem_for		/* memory description	*/
			)
{
    void  *ptr;

    if( (ptr = malloc( size )) == NULL )
    {
	GerrLogReturn(
		      source_name,
		      line_no,
		      "Can't get memory for %s. (size=%d)",
		      mem_for,
		      size
		      );
    }
    return( ptr );

}

/*=======================================================================
||									||
||			      MemReallocReturn()			||
||									||
|| 		Reallocate requested amount of memory.			||
|| 		On error - put message in log file and return.		||
 =======================================================================*/
void	*MemReallocReturn(
			  char *source_name,	/* source name		*/
			  int  line_no,		/* line number		*/
			  void *old_ptr,	/* original memory ptr	*/
			  size_t new_size,	/* new size to allocate	*/
			  char *mem_for		/* memory description	*/
			  )
{
    void  *ptr;

    if( (ptr = realloc( old_ptr, new_size )) == NULL )
    {
	GerrLogReturn( 
		      source_name,
		      line_no,
		      "Can't realloc memory for %s. (size=%d)",
		      mem_for,
		      new_size
		      );
    }
    return( ptr );

}

/*=======================================================================
||									||
||			      MemFree()					||
||									||
|| 			Free allocted memory.				||
 =======================================================================*/
void	MemFree(
		void *ptr			/* memory pointer	*/
		)
{
    if( ! ptr )
    {
	GerrLogReturn( GerrId, "attempt to free a null pointer" );
    }

    free( ptr );

}

/*=======================================================================
||									||
||				ListMatch()				||
||									||
|| 		Find a matching string in a list of strings.		||
 =======================================================================*/
int	ListMatch(
		  char *key,			/* key to match in list	*/
		  char **list			/* string list pointer	*/
		  )
{

/*
 * Local variables
 * ---------------
 */
    int	num;

/*
 * Test key string to match
 * ------------------------
 */
    if( key == NULL || ! key[0] )
	return( -1 );

/*
 * Loop on list items
 * ------------------
 */
    for(
	num = 0;

	list[num] != NULL	&&
	    strcmp(
		key,
		list[num]
		);

	num++
	);

/*
 * Return item number or -1 for not-found
 * --------------------------------------
 */
    if( list[num] != NULL )
	return( num );

    return -1;
}

/*=======================================================================
||									||
||				ListNMatch()				||
||									||
|| 		Find a matching string in a list of strings.		||
 =======================================================================*/
int	ListNMatch(
		  char *key,			/* key to match in list	*/
		  char **list			/* string list pointer	*/
		  )
{

/*
 * Local variables
 * ---------------
 */
    int	num;

/*
 * Test key string to match
 * ------------------------
 */
    if( key == NULL || ! key[0] )
	return( -1 );

/*
 * Loop on list items
 * ------------------
 */
    for(
	num = 0;

	list[num] != NULL	&&
	    strncmp(
		key,
		list[num],
		strlen(list[num])
		);

	num++
	);

/*
 * Return item number or -1 for not-found
 * --------------------------------------
 */
    if( list[num] != NULL )
	return( num );

    return -1;
}

/*=======================================================================
||									||
||				StringToupper()				||
||									||
 =======================================================================*/
void	StringToupper(
		      char *str			/* string to upper-case	*/
		      )
{
    if( str )
    {
	while( *str )
	{
	    *str = toupper( *str );
	    str++;
	}
        /*  while( *str++ = toupper( *str++ ) ) ;  */
    }
}

/*=======================================================================
||									||
||				BufConvert()				||
||									||
 =======================================================================*/
void	BufConvert(
		   char *source,		/* string to convert	*/
		   int size			/* size of convertion	*/
		   )
{

/*
 * Local variables
 * ---------------
 */
    register int	bot, top;
    char		temp;

/*
 * Convert buffer
 * --------------
 */
    for( bot = 0, top = size - 1; bot < top ; top--, bot++ )
    {
	temp = source[top];
	source[top] = source[bot];
	source[bot] = temp;
    }

}

/*=======================================================================
||									||
||				GetCurrProgName()			||
||									||
 =======================================================================*/
void	GetCurrProgName(
			char *path_name,
			char *file_name
			)
{

/*
 * Local variables
 * ---------------
 */
    char	*cp;

/*
 * Get filename from pathname & cut suffix
 * ---------------------------------------
 */
    cp = strrchr( path_name, '/' );
    if( cp != NULL )
    {
	++cp;
    }
    else
    {
	cp = path_name;
    }
    strcpy( file_name, cp );

}

/*=======================================================================
||									||
||				InitEnv()				||
||									||
||		Initialize hash environment lists & counters		||
||									||
 =======================================================================*/
int	InitEnv(
		char *source_name,		/* source name		*/
		int  line_no			/* line number		*/
		)
{

/*
 * Alloc initial memory for env buffers
 * ------------------------------------
 */
    EnvNames = (char **)MemAllocReturn( 
				       source_name,
				       line_no,
				       EnvSize * sizeof(char *),
				       "Memory for environment"
				       );

    EnvValues = (char **)MemAllocReturn(
				        source_name,
				        line_no,
				        EnvSize * sizeof(char *),
				        "Memory for environment"
				        );

    if( EnvNames == NULL ||
	EnvValues == NULL )
    {
	return( MEM_ALLOC_ERR );
    }

/*
 * Initialize parameter lists
 * --------------------------
 */
    EnvNames[0] = NULL;
    EnvValues[0] = NULL;
    
/*
 * Return -- ok
 * ------------
 */
    return( MAC_OK );

}

/*=======================================================================
||									||
||				HashEnv()				||
||									||
||	 Get environment value from hash list / environment & return	||
||									||
 =======================================================================*/
int	HashEnv(
		char *source_name,
		int  line_no
		)
{

/*
 * Local variables
 * ---------------
 */
    char	*cp;
    int		match,
		no_match;
    EnvTab	*env_param;

/*
 * Loop on env tab params
 * ----------------------
 */
    no_match	= 0;
    for(
	env_param		= EnvTabPar;
	env_param->param_name 	!= NULL;
	env_param++
	)
    {
/*
 * Fetch in hash list environment name
 * -----------------------------------
 */
	match = ListMatch( env_param->param_name, EnvNames );
	if( match != -1 )
	{
	    strcpy(
		   env_param->param_value,
		   EnvValues[match]
		   );
	    continue;
	}
/*
 * Get environment value by name
 * -----------------------------
 */
	cp = getenv( env_param->param_name );
	if( cp != NULL )
	{
	    strcpy( env_param->param_value, cp );
	    if( EnvCount > EnvSize - 1 )
	    {
		EnvSize += 20;
		EnvNames = (char**)MemReallocReturn(
						    source_name,
						    line_no,
						    EnvNames,
						    EnvSize * sizeof(char *),
						    "Memory for environment"
						    );

		EnvValues = (char**)MemReallocReturn(
						     source_name,
						     line_no,
						     EnvValues,
						     EnvSize * sizeof(char *),
						     "Memory for environment"
						     );

		if( EnvNames == NULL ||
		    EnvValues == NULL )
		{
		    return( MEM_ALLOC_ERR );
		}
	    }
	    EnvNames[EnvCount] = strdup( env_param->param_name );
	    EnvValues[EnvCount++] = strdup( env_param->param_value );
	    EnvNames[EnvCount] = EnvValues[EnvCount] = NULL;
	    continue;
	}

	no_match++;
	GerrLogReturn( source_name,
		       line_no,
		       "Missing environment variable : %s.",
		       env_param->param_name
		       );
    }

/*
 * Return
 * ------
 */
    if( no_match )
    {
	return( ENV_PAR_MISS );
    }

    return( MAC_OK );

}

/*=======================================================================
||																		||
||				InitSonProcess()										||
||																		||
||	 Init son process by:												||
||		1) Attaching shared memory.										||
||		2) Load parameters from shared memory.							||
||		3) Create listen named pipe for process.						||
||		4) Add record for process in shared memory.						||
||		5) Open processes & status tables for access.					||
||																		||
 =======================================================================*/
int	InitSonProcess	(	char		*proc_pathname,		// process name
						int			proc_type,			// process type
						int			retrys,				// num of retries
						int			eicon_port,			// eicon port for tsserv
						short int	system				// pharmacy or doctors
					)
{
	// Initialize environment buffers
	ABORT_ON_ERR (InitEnv (GerrId));

	// Hash environment into buffers
	ABORT_ON_ERR (HashEnv (GerrId));
    
	// Open semaphore
	RETURN_ON_ERR (OpenSemaphore ());

	// Attach to shared memory
	RETURN_ON_ERR (OpenFirstExtent ());

	// Get file name of process
	GetCurrProgName (proc_pathname, CurrProgName);

	// Load parameters from shared memory table
	RETURN_ON_ERR (ShmGetParamsByName (SonProcParams, PAR_LOAD));

	// Lock pages in primary memory
	if (LockPagesMode == 1)
	{

#ifdef _LINUX_
		if (mlockall (MCL_CURRENT)) // GAL 2/01/05 Linux
#else
		if (plock (PROCLOCK))
#endif
		{ 
			GerrLogReturn (GerrId, "Can't lock pages in primary memory - error %d %s.", GerrStr);
		}
	}
    
	// Return to unprivileged userid
	if (setuid (atoi (MacUid)))
	{
//		GerrLogReturn (GerrId, "Can't setuid to %d - error %d %s.", atoi (MacUid), GerrStr);
	}
    
	// Get pathname for named pipe
	GetCurrNamedPipe (CurrNamedPipe);

	// Get listen socket on the named pipe
	RETURN_ON_ERR (ListenSocketNamed (CurrNamedPipe));

	// Add record of process to shared memory table
	RETURN_ON_ERR (AddCurrProc (retrys, proc_type, eicon_port, system));
// GerrLogMini (GerrId, "Added %s PID %d type %d system %d retrys %d - ret = %d.", CurrProgName, getpid(), proc_type, system, retrys, ret); 

	// Add to "son processes" count.
	RETURN_ON_ERR (AddToSonsCount (1));

	// All done!
	return (MAC_OK);
}


/*=======================================================================
||									||
||				ExitSonProcess()			||
||									||
||	 Exit son process by :						||
||		1) Detaching shared memory.				||
||		2) Drop listen named pipe for process & close connect.	||
||		3) Close semaphore.					||
||									||
 =======================================================================*/
int	ExitSonProcess(
		       int	status		/* exit status of proc	*/
		       )
{
/*
 * Close connection & dispose named pipe file
 * ------------------------------------------
 */
    DisposeSockets();

/*
 * Detach all shared memory extents
 * --------------------------------
 */
    DetachAllExtents();

/*
 * Close semaphore
 * ---------------
 */
    CloseSemaphore();

/*
 * Exit process -- normal shutdown
 * -------------------------------
 */
    exit( status );

    _exit( status );

}

/*=======================================================================
||									||
||				BroadcastSonProcs()			||
||									||
||		Broadcast a message to all son processes		||
||									||
 =======================================================================*/
int	BroadcastSonProcs(
			  int	proc_flg,	/* which proc shutdown	*/
			  TABLE	*proc_tablep,	/* proc table pointer	*/
			  int	mess_type	/* message type		*/
			  )
{

/*
 * Local variables
 */
    PROC_DATA	proc_data;
    struct sigaction sig_act;
    int		cnt_socket;
    int		first_row = 1;

/*=======================================================================
||									||
||		Broadcast a message to all son processes		||
||									||
 =======================================================================*/

    sig_act.sa_handler	= alarm_handler;

#ifdef _LINUX_
    sig_act.sa_mask	= *( sigset_t* ) NULL;	//  GAL 2/01/05 Linux
#else
    sig_act.sa_mask = 0;
#endif

    sig_act.sa_flags	= 0;

    RewindTable( proc_tablep );

    while(
	  ActItems(
		   proc_tablep,
		   0,
		   GetItem,
		   (OPER_ARGS)&proc_data
		   )
	  == MAC_OK
	  )
    {
	/********************** DEBUG *************************/
	if( proc_flg == FIRST_SQL )
	{
	    if( proc_data.proc_type != SQLPROC_TYPE )
	    {
		continue;
	    }
	    if( ! first_row )
	    {
		continue;
	    }
	    first_row = 0;
	}

	switch( proc_data.proc_type )
	{
	    case FATHERPROC_TYPE:		/* DON'T CALL MYSELF	*/

		continue;

	    case CONTROLPROC_TYPE:		/* CONTROL PROCESS	*/

		if( proc_flg == NOT_CONTROL )
		{
		    continue;
		};
		break;


	    default:				/* OTHER PROCESS	*/

		if( proc_flg == JUST_CONTROL )
		{
		    continue;
		};

		break;
	}

#ifdef DEBUG2
	printf(
	       "FATHER : connecting proc '%s' ...\n",
	       proc_data.proc_name
	       );
	fflush( stdout );
#endif
	if(
	   sigaction(
		     SIGALRM,
		     &sig_act,
		     NULL
		     )
	   == -1
	   )
	{
	    GerrLogReturn(
			  GerrId,
			  "Error while in sigaction .. error (%d) %s",
			  GerrStr
			  );
	    break;
	}

	/*
	 * Set alarm
	 */
	alarm( 1 );

	cnt_socket =
	  ConnectSocketNamed(
			     proc_data.named_pipe
			     );

	CONT_ON_ERR( cnt_socket );

	/*
	 * Reset alarm
	 */
	alarm( 0 );

#ifdef DEBUG2
	printf(
	       "FATHER : writing proc '%s' ...\n",
	       proc_data.proc_name
	       );
	fflush( stdout );
#endif

	WriteSocket(
		    cnt_socket,
		    PcMessages[mess_type],
		    strlen(PcMessages[mess_type]),
		    0
		    );

	CloseSocket(
		    cnt_socket
		    );

#ifdef DEBUG2
	printf(
	       "FATHER : finished writing proc '%s' ...\n",
	       proc_data.proc_name
	       );
	fflush( stdout );
#endif

    }

/*
 * Return -- ok
 */
    return( MAC_OK );

}

/*=======================================================================
||									||
||				alarm_handler()				||
||									||
||			alarm handler -- get out of system call		||
||									||
 =======================================================================*/
void	alarm_handler(
		      int	sig		/* signal caught	*/
		      )
{

/*
 * Do nothing -- just get out of system call
 */
    return;
}

/*=======================================================================
||									||
||				GetCommonPars()				||
||									||
||		Get common parameters from command line			||
||									||
 =======================================================================*/
int	GetCommonPars(
		      int	argc,		/* num of args		*/
		      char	**argv,		/* arguments		*/
		      int	*retrys		/* num of retrys	*/
		      )
{
    int		i;
  
    *retrys =					/* Zero arguments	*/
      NO_RETRYS;
    
    for(
	i = 0;
	argv[i] != NULL;
	i++
	)
    {
	if(
	   argv[i][0] != '-'
	   )
	{
	    continue;
	}

	switch( argv[i][1] )
	{
	    case 'r':
	    case 'R':
	      
		if(
		   argv[i][2]
		   )
		{
		    *retrys =
		      atoi(
			   &argv[i][2]
			   );
		}
		else
		{
		    *retrys =
		      atoi(
			   argv[++i]
			   );
		}
		continue;

	    default:

	        continue;
	}
    }

    return( MAC_OK );
    
}

/*=======================================================================
||									||
||				GetFatherNamedPipe()			||
||									||
 =======================================================================*/
int	GetFatherNamedPipe( char*	father_namedpipe )
{

    TABLE	* proc_tablep;
    PROC_DATA	proc_data;
    
    father_namedpipe[0] = 0;

    RETURN_ON_ERR( OpenTable( PROC_TABLE, & proc_tablep ) );

    while(
	  ActItems(
		   proc_tablep,
		   0,
		   GetItem,
		   (OPER_ARGS)&proc_data
		   )
	  != NO_MORE_ROWS
	  )
    {
	if( proc_data.proc_type == FATHERPROC_TYPE )
	{
	    strcpy( father_namedpipe, proc_data.named_pipe );

	    break;
	}
    }

    CloseTable( proc_tablep );

    if( ! father_namedpipe[0] )
    {
	GerrLogReturn( GerrId,
		    "Father process named pipe not found in shared mem"
		    );
	return( NO_FATHER_PIP );
    }

    return( MAC_OK );
    
}

/*=======================================================================
||									||
||				TssCanGoDown()				||
||									||
 =======================================================================*/
int	TssCanGoDown( int	system )
{

    TABLE	* proc_tablep;
//    TABLE	* dipr_tablep;
    PROC_DATA	proc_data;
//    PROC_DATA	dipr_data;
    int		pharm_count = 0,
		doctor_count = 0;
    static time_t last_time = 0;

    if( time(NULL) - last_time < GoDownCheckInterval )
    {
	return 0;
    }

    last_time = time(NULL);

    
    //
    // 1) Check for running workers
    //
    RETURN_ON_ERR( OpenTable( PROC_TABLE, & proc_tablep ) );

    while(
	  ActItems(
		   proc_tablep,
		   0,
		   GetItem,
		   (OPER_ARGS)&proc_data
		   )
	  != NO_MORE_ROWS
	  )
    {
	{
	    if( proc_data.proc_type == TSX25WORKPROC_TYPE )
	    {
		pharm_count ++;
	    }

	    // and doctors worker for - doctor_count ++;
	}
    }

    CloseTable( proc_tablep );




    return( ( system == PHARM_SYS && ! pharm_count ) ||
	    ( system == DOCTOR_SYS && ! doctor_count ) );
    
}



/*=======================================================================
||																		||
||			     Run_server()											||
||																		||
||	Runs server son process and store values in buffer					||
||	Command in proc_data is may contain arguments saparated by			||
||	  whitespaces and are coming first. next arguments come from		||
||	  params shared memory table.										||
||																		||
 =======================================================================*/
int	Run_server (PROC_DATA *proc_data)
{
    TABLE		*parm_tablep;

    pid_t		son_pid;
    int			i,
				args_count,
				curr_prog_len,
				args_size;
    char		exe_file_name	[512],
				command			[256],
				curr_prog		[512],
				log_file_key	[512],
				new_log_file	[512],
				save_log_file	[512],
				full_command	[1024],
				*ptr,
				*sprtor = " \t\n\r\f",
				**args;
    PARM_DATA	parm_data;
	FILE		*ReadExeFile	= NULL;

	/*===================================================
	||													||
	||			GET CHILD PROCESS ARGUMENTS				||
	||													||
	 ===================================================*/
	// get pathname of command & first arguments
    ptr = strtok (proc_data->proc_name, sprtor);
    strcpy (command, ptr);

    sprintf (log_file_key, "%s.%s", command, LOG_FILE_KEY);

    sprintf (exe_file_name, "%s/%s", BinDir, ptr);

	// DonR 11Apr2022: Check whether the program to run exists
	// before trying to run it.
	ReadExeFile = fopen (exe_file_name, "r");
	if (ReadExeFile == NULL)
	{
		GerrLogMini (GerrId, "Run_server: Couldn't find %s!", exe_file_name);
		return (MAC_PROGRAM_NOT_FOUND);
	}
	else
	{
		fclose (ReadExeFile);
	}

	// Allocate space for arguments list
    args_size = 10;
    args = (char **)MemAllocReturn (GerrId, args_size * sizeof(char *), "Memory for process arguments");
    RETURN_ON_NULL (args);

	// Get more arguments from command itself.
    args[0]	= exe_file_name;

    args_count = 0;

    while ((ptr = strtok (NULL, sprtor)) != NULL)
    {
		if (args_count > args_size - 3)
		{
		    args_size += 10;
		    args =  (char **)MemReallocReturn (GerrId, args, args_size * sizeof(char *), "Process arguments");
		    RETURN_ON_NULL (args);
		}

		args_count++;
	
		args[args_count] = strdup (ptr);
    }
    
	// Prepare row for finding command arguments
    curr_prog_len = sprintf (curr_prog, "%s.%s", command, ARG_POSTFIX);
    
	// Fetch arguments from shm table into list
    sprintf (full_command, "-r%d", proc_data->retrys);

    args_count++;
	
    args[args_count] = strdup (full_command);

	// Loop on params shm table
    RETURN_ON_ERR (OpenTable (PARM_TABLE, &parm_tablep));

    new_log_file[0] = 0;

    while (1)
    {
        state = ActItems (parm_tablep, 0, GetItem, (OPER_ARGS)&parm_data);

		if (state == NO_MORE_ROWS)
		{
			break;
		}

		RETURN_ON_ERR (state);

		// Get log filename for current program
		if (!strcmp (parm_data.par_key, log_file_key))
		{
		    strcpy (new_log_file, parm_data.par_val);
		}

		if (strncmp (curr_prog, parm_data.par_key, curr_prog_len))
		{
		    continue;
		}

		if (args_count > (args_size - 3))
		{
		    args_size += 10;
			args = (char **)MemReallocReturn (GerrId, args, (args_size * sizeof(char *)), "Process arguments");
			RETURN_ON_NULL (args);
		}

		args_count++;
	
		args[args_count] = strdup (parm_data.par_val);

#ifdef DEBUG
		printf ("Got  argument  : %s .\n", args[args_count]);
		fflush (stdout);
#endif

    }	// While (1)

    CloseTable (parm_tablep);

	// Add eicon port argument to tsserver processes
    if (proc_data->eicon_port)
    {
		sprintf (buf, "-p%d", proc_data->eicon_port);
		args_count++;
		args[args_count] = strdup (buf);
    }

	// Add MODE parameter for TCP/IP Listener
	// DonR 14Aug2022 NOTE: This argument is *NOT* needed for
	// PharmTcpServer; I looked on the old linux01-dev machine
	// (in Vladimir Galinkin's source directory) and it doesn't
	// look like it's used by DocTcpServer either.
    if (proc_data->proc_type == TSTCPIPSRVPROC_TYPE)
    {
		args[++args_count] = strdup ("-mTCPIP");
    }

    args_count++;
    
    args[args_count] = NULL;

	// FORK NEW PROCESS AND EXECUTE
    for (len = i = 0; args[i] != NULL; i++)
    {
		len += sprintf (full_command + len, "%s%s", (i ? " " : ""), args[i]);
    }

    if (fcntl (0, F_GETFD, 0) == -1)
    {
		puts ("failed at point 1");
		fflush (stdout);
    }

    switch ((son_pid = fork ()))
    {
		case -1:	// Error return
					GerrLogReturn (GerrId, "Can't create new child process" GerrErr, GerrStr);
					return (MAC_FALS);

		default:	// We're still in the parent process
					// Set stamps for new process
					// NOTE: This "if" should always be true, since we are no
					// longer running X25 communications.
					// DonR 18Aug2022: Got rid of NIU LOG_X25_CONNECTIONS variable.
//				    if ((proc_data->proc_type != TSX25WORKPROC_TYPE) || LOG_X25_CONNECTIONS)
				    if (proc_data->proc_type != TSX25WORKPROC_TYPE)
				    {
						strcpy (save_log_file, LogFile);

						if (new_log_file[0])
						{
						    strcpy (LogFile, new_log_file);
						}

						GerrLogMini (GerrId, "PID %d starting %s (PID %d).", getpid (), full_command, son_pid);

						strcpy (LogFile, save_log_file);
				    }

					break;

		case 0:		// We're now in the child process.
				    if (fcntl (0, F_GETFD, 0) == -1)
				    {
						puts ("failed at point 2");
						fflush(stdout);
				    }

				    if (proc_data->system == DOCTOR_SYS)
					{
						putenv ("DOCTOR_SYS=1");
					}

					// GerrLogMini (GerrId, "Forked PID %d starting %s.", getpid(), full_command);

				    //  Run the command
					execv (args[0], args);

					// DonR 11Apr2022: Since we now check whether the executable is present
					// before we try to run it, we should really never get here! (The exception
					// would be if the program file is present and readable, but *not*
					// executable - but this shouldn't happen in real life.)
					GerrLogMini (GerrId, "PID %d failed to launch %s - errno = %d.", getpid(), args[0], errno);

					// DonR 11Apr2022: Added an exit here, since the GerrLogExit() call
					// seems to cause segmentation faults and I don't feel like trying
					// to fix it right now. (Sorry sorry sorry.)
					exit (MAC_CHILD_NOT_STARTED);


				    GerrLogExit (GerrId, MAC_CHILD_NOT_STARTED,
//								 "Can't execv (\"%s\")\n\terror: (%d) %s.\n",
								 "Can't execv (\"%s\")\n\terror: %s.\n",
								 full_command, GerrStr);
				   break;
    }	// Switch on fork() return code.

	//  Return -- ok
    return (MAC_OK);
}



/*=======================================================================
||									||
||			     GetSystemLoad()				||
||									||
 =======================================================================*/
int	GetSystemLoad( int interval )
{
    int		count;
    int		load;
    char	command[256];
    char	buffer[256];
    FILE	*fp;

    sprintf( command, "sar %d", interval );

    fp = popen( command, "r" );

    if( fp == NULL )
    {
	GerrLogReturn(
		      GerrId,
		      "Can't execute command '%s' "
		      GerrErr,
		      command,
		      GerrStr
		      );
	return -1;
    }

    for( count = 0;
	 count < 5 && buf == fgets( buf, 80, fp );
	 count++
	 );

    load = -1;

    if( count == 5 )
    {
	len = strlen( buf );
	for( len -= 2; len > 0 && buf[len-1] != ' '; len -- );
	load = 100 - atoi( buf + len );
    }

    pclose( fp );

    return load;
}

/*=======================================================================
||									||
||			     	my_nap()				||
||									||
 =======================================================================*/
void	my_nap( int miliseconds )
//
// The following block emulates nap ...
//
{
    struct timeval t;
    fd_set	f;

    t.tv_sec = miliseconds / 1000;
    t.tv_usec = (miliseconds % 1000) * 1000;	// microseconds

    FD_ZERO( & f );
    //FD_SET( 0, & f );

    select( 1, NULL, &f, NULL, & t );
}

/*=======================================================================
||									||
||				run_cmd()				||
||									||
 =======================================================================*/
int	run_cmd( char * command, char * answer )
{
    //
    // Run command and get handle to its answer
    //
    FILE	*fp = popen( command, "r" );
    char	line_buf[256];
    struct sigaction sig_act;

    answer[0] = 0;

    if( fp == NULL )
    {
	GerrLogReturn(
		      GerrId,
		      "Can't open a pipe to command '%s'"
		      GerrErr,
		      command,
		      GerrStr
		      );
	return -1;
    }

    //
    // Read lines of answer
    //
    line_buf[0] = 0;

    sig_act.sa_handler	= alarm_handler;

#ifdef _LINUX_
    sig_act.sa_mask	= *( sigset_t* ) NULL;	//  GAL 2/01/05 Linux
#else
    sig_act.sa_mask = 0;
#endif

    sig_act.sa_flags	= 0;

    if(
       sigaction(
		 SIGALRM,
		 &sig_act,
		 NULL
		 )
       == -1
       )
    {
	GerrLogReturn(
		      GerrId,
		      "Error while in sigaction .. error (%d) %s",
		      GerrStr
		      );
    }

    alarm( 5 );

    while( line_buf == fgets(line_buf, sizeof(line_buf), fp) )
    {
	answer += sprintf( answer, line_buf );
    }

    alarm( 0 );

    return pclose( fp );

}

/*=======================================================================
||									||
||				GetWord()				||
||									||
 =======================================================================*/
char *	GetWord( char * cptr, char * word )
{
    int		count;

    if( cptr != NULL )
    {
	cptr += strspn( cptr, "\t\n\r " );

	count = strcspn( cptr, "\t\n\r " );

	strncpy( word, cptr, count );
	word[count] = 0;

	cptr += count;
    }
    else
    {
	word[0] = 0;
    }

    return cptr;
}

/*=======================================================================
||									||
||				ToGoDown()				||
||									||
||	Return values :							||
||	0 -> nothing	else -> go down					||
 =======================================================================*/
int	ToGoDown(int	system)
{
    STAT_DATA	stat_data;
    int		ret = 0;
    static time_t last_time = 0;

    if( time(NULL) - last_time < GoDownCheckInterval )
    {
	return 0;
    }

    last_time = time(NULL);

    //
    // Get status record
    //
    state = get_sys_status( & stat_data );

    //
    // Determine if to go down -
    // either when whole system going down
    //  or when sub-system is going down
    //
    ret = ( stat_data.status == GOING_DOWN )	||
	( system == PHARM_SYS && stat_data.pharm_status == GOING_DOWN ) ||
	( system == DOCTOR_SYS && stat_data.doctor_status == GOING_DOWN );
	
    return ret;
}

/*=======================================================================
||																		||
||				buf_reverse()											||
||																		||
||		         Reverse a text buffer but do NOT change it otherwise	||
||				(DonR: Renamed for accuracy 17Jul2023.					||
||																		||
 =======================================================================*/
void	buf_reverse( unsigned char *source, int size )
{
	register int	i;
	char			buffer[512];

	if (size < 2)
		return;

	memcpy (buffer, source, size);
	for (i = 0; size ; /**/ )
	{
		source[i++] = buffer[--size];
	}
}
/* End of buf_reverse() */

/*=======================================================================
||																		||
||				fix_char()												||
||																		||
||																		||
 =======================================================================*/
void fix_char( unsigned char *source )
{
    static char	*signs_to_fix = "()",		/* from signs		*/
		*fixed_signs  = ")(",		/* to signs		*/
		*cp;
    /*
     * Test if need fix
     */
    if( (cp = strchr( signs_to_fix, *source )) == NULL )
    {
	return;
    } 

    /*
     * Fix char
     */
    *source = fixed_signs[cp - signs_to_fix];
}



/*=======================================================================
||																		||
||				EncodeHebrew ()											||
||																		||
||	Flexible routine to send pharmacies Hebrew in whatever				||
||	encoding they need. Note that if we start using UTF-8 in			||
||	"legacy" transactions, the relevant text variables and				||
||	output formats need to be big enough to accomodate it!				||
||																		||
||	Added 26Nov2025 by DonR, User Story #251264.						||
||																		||
 =======================================================================*/
int EncodeHebrew	(	char			input_encoding_in,
						char			output_encoding_in,
						size_t			buffer_size_in,	// Includes terminating NULL character!
						unsigned char	*text_in_out			)
{
	// D = DOS Hebrew
	// U = UTF-8 Hebrew
	// W = Win-1255 Hebrew
	// N = No translation - send as-is

	// For now at least, the only source we have is Windows-1255 Hebrew;
	// so we're not actually doing anything with input_encoding_in other
	// than comparing it to the desired output encoding to see if we
	// actually have to do anything.

	// First, see if we actually need to do anything.
	if ((output_encoding_in	== 'N')	|| (output_encoding_in == input_encoding_in))
	{
		// Don't log anything - just return silently.
		return (0);
	}

	// Second, log an error if the input encoding is something other
	// than Windows-1255, since (at this point, at least) we're not
	// set up to handle any other source data.
	if ((input_encoding_in != 'W') || ((output_encoding_in != 'D') && (output_encoding_in != 'U')))
	{
		char	*InputEncodingDesc;
		char	*OutputEncodingDesc;

		switch (input_encoding_in)
		{
			case	'D':	InputEncodingDesc	= "DOS Hebrew";				break;
			case	'U':	InputEncodingDesc	= "UTF-8";					break;
			case	'W':	InputEncodingDesc	= "Windows-1255 Hebrew";	break;
			default:		InputEncodingDesc	= "unknown encoding";		break;
		}

		switch (output_encoding_in)
		{
			case	'D':	OutputEncodingDesc	= "DOS Hebrew";				break;
			case	'U':	OutputEncodingDesc	= "UTF-8";					break;
			case	'W':	OutputEncodingDesc	= "Windows-1255 Hebrew";	break;
			default:		OutputEncodingDesc	= "unknown encoding";		break;
		}


		GerrLogMini (	GerrId, "EncodeHebrew: Can't translate %c (%s) to %c (%s)!",
						input_encoding_in, InputEncodingDesc, output_encoding_in, OutputEncodingDesc	);

		return (-1);
	}

	// If we get here, we're converting "W" (Windows-1255 Hebrew)
	// to "D" (DOS Hebrew) or "U" (UTF-8).

	// Handle Windows-1255-to-DOS conversion - just invoke
	// WinHebToDosHeb rather than copy all its logic.
	if (output_encoding_in == 'D')
	{
		WinHebToDosHeb (text_in_out);
	}

	// Handle Windows-1255-to-UTF-8 conversion. This needs a little extra
	// logic, since UTF-8 is a multi-byte format and the result will be
	// longer than the single-byte-character input.
	else
	{
	    size_t			UTF8_length	= 0;
	 	unsigned char *	UTF8_string;

		WinHebToUTF8 ((unsigned char *)text_in_out, &UTF8_string, &UTF8_length);

		// Copy the UTF-8 text into our input-output string and put
		// a terminator character at the end. Note that UTF8_length
		// does *not* include the terminating NULL character!
		if (UTF8_length >= buffer_size_in)
		{
			memcpy (text_in_out, UTF8_string, buffer_size_in);
			text_in_out [buffer_size_in] = (char)0;

			// If the buffer didn't have room for all of the UTF-8 output,
			// write a message to log.
			GerrLogMini (	GerrId, "EncodeHebrew: Truncating %d-byte UTF-8 string to fit %d-byte buffer.",
							UTF8_length, (buffer_size_in - 1)												);
		}
		else
		{
			memcpy (text_in_out, UTF8_string, UTF8_length);
			text_in_out [UTF8_length] = (char)0;
		}

	}

	return (0);
}



/*=======================================================================
||																		||
||				WinHebToDosHeb ()										||
||																		||
 =======================================================================*/
void	WinHebToDosHeb( unsigned char * source )
{
    register int	    pos, to_pos, to_num_pos, sign_count;
    static char    *signs = ":,.+-/$";

    for( pos = 0; source[pos]; pos++ )
    {
	/*
	 * When non hebrew continue
	 */
	if( source[pos] < 128 || ( source[pos] > 154 && source[pos] < 224 ) ||
	    source[pos] > 250 )
	{
	    continue;
	}

	/*
	 * Find length of non english text & fix chars
	 */
	for( to_pos = pos + 1;
		source[to_pos]	&&
		( source[to_pos] < 65  ||
		( source[to_pos] > 90 && source[to_pos] < 97 ) ||
		source[to_pos] > 122 ) &&
		strchr( "\n\f", source[to_pos] ) == NULL ; to_pos++ )
	{
	   fix_char( source + to_pos );
	}

	/*
	 * Return the last non - (hebrew or digits)
	 */
	for( ; source[to_pos-1] < 48 ||
		( source[to_pos-1] > 57  && source[to_pos-1] < 128 ) ||
		( source[to_pos-1] > 154 && source[to_pos-1] < 224 ) ||
		source[to_pos-1] > 250; to_pos-- )
	{
	}

	/*
	 * Reverse Hebrew text
	 */
	buf_reverse( source + pos, to_pos - pos );

	/*
	 * Convert numbers inside hebrew text
	 */
	for( ; pos < to_pos ; pos++ )
	{
	    if( source[pos] < 48 || source[pos] > 57 )
	      continue;

	    /*
	     * Find number length
	     */
	    for( to_num_pos = pos + 1, sign_count = 0; to_num_pos < to_pos;
		to_num_pos++ )
	    {
		/*
		 * Digits - continue
		 */
		if( source[to_num_pos] > 47 && source[to_num_pos] < 58 )
		{
		    sign_count = 0;
		    continue;
		}
		/*
		 * Signes - continue only if one sign after digits
		 */
		if( strchr( signs, source[to_num_pos] ) != NULL &&
		    ! sign_count++ )
		{
		    continue;
		}

		break;
	    }			/* end of loop on nums inside hebrew txt*/
	    /*
	     * Re-reverse number inside Hebrew text
	     */
	    buf_reverse( &source[pos], to_num_pos - pos );
	    pos = to_num_pos - 1;
	    continue;
	}			/* end of loop on hebrew text		*/
	pos = to_pos - 1;
	continue;
    }				/* end of loop on text			*/


/*
 *  Gilad( 2.1997 ): replace WinHeb with DosHeb:
 */
   for( pos = 0; source[pos]; pos++ )
   {
      if( source[pos] >= 224 && source[pos] <= 250 )
      {
	 source[pos] -= 96;
      }
   }
}

/* End of WinHebToDosHeb */



int WinHebToUTF8 (unsigned char *Win1255_in, unsigned char **UTF8_out, size_t *length_out)
{
	static short			Initialized	= 0;
	static unsigned char	UTF8	[256] [4];
	static unsigned char	*OutBuf					= NULL;	// So realloc() will work like malloc() the first time it's called.
	static int				OutBuf_needed			= 6000;	// Assume 2000 char input with everything requiring 3 UTF bytes.
	static int				OutBuf_allocated		= 0;	// Nothing allocated yet.

	// Win-1255 -> UTF-8 translation array sourced from https://pretagteam.com/question/converting-windows1255-to-utf8-in-php-5 .
	// Hopefully the values are all correct! I'm leaving the source text as-is and translating it to UTF-8 bytes when the
	// function runs the first time.
	static const char		*TranslationSource []	=
		{
			"xe2x82xac",	"xefxbfxbd",	"xe2x80x9a",	"xc6x92",		"xe2x80x9e",	"xe2x80xa6",	"xe2x80xa0",	"xe2x80xa1",	// 128 - 135
			"xcbx86",		"xe2x80xb0",	"xefxbfxbd",	"xe2x80xb9",	"xefxbfxbd",	"xefxbfxbd",	"xefxbfxbd",	"xefxbfxbd",	// 136 - 143
			"xefxbfxbd",	"xe2x80x98",	"xe2x80x99",	"xe2x80x9c",	"xe2x80x9d",	"xe2x80xa2",	"xe2x80x93",	"xe2x80x94",	// 144 - 151
			"xcbx9c",		"xe2x84xa2",	"xefxbfxbd",	"xe2x80xba",	"xefxbfxbd",	"xefxbfxbd",	"xefxbfxbd",	"xefxbfxbd",	// 152 - 159
			"xc2xa0",		"xc2xa1",		"xc2xa2",		"xc2xa3",		"xe2x82xaa",	"xc2xa5",		"xc2xa6",		"xc2xa7",		// 160 - 167
			"xc2xa8",		"xc2xa9",		"xc3x97",		"xc2xab",		"xc2xac",		"xc2xad",		"xc2xae",		"xc2xaf",		// 168 - 175
			"xc2xb0",		"xc2xb1",		"xc2xb2",		"xc2xb3",		"xc2xb4",		"xc2xb5",		"xc2xb6",		"xc2xb7",		// 176 - 183
			"xc2xb8",		"xc2xb9",		"xc3xb7",		"xc2xbb",		"xc2xbc",		"xc2xbd",		"xc2xbe",		"xc2xbf",		// 184 - 191
			"xd6xb0",		"xd6xb1",		"xd6xb2",		"xd6xb3",		"xd6xb4",		"xd6xb5",		"xd6xb6",		"xd6xb7",		// 192 - 199
			"xd6xb8",		"xd6xb9",		"xefxbfxbd",	"xd6xbb",		"xd6xbc",		"xd6xbd",		"xd6xbe",		"xd6xbf",		// 200 - 207
			"xd7x80",		"xd7x81",		"xd7x82",		"xd7x83",		"xd7xb0",		"xd7xb1",		"xd7xb2",		"xd7xb3",		// 208 - 215
			"xd7xb4",		"xefxbfxbd",	"xefxbfxbd",	"xefxbfxbd",	"xefxbfxbd",	"xefxbfxbd",	"xefxbfxbd",	"xefxbfxbd",	// 216 - 223
			"xd7x90",		"xd7x91",		"xd7x92",		"xd7x93",		"xd7x94",		"xd7x95",		"xd7x96",		"xd7x97",		// 224 - 231
			"xd7x98",		"xd7x99",		"xd7x9a",		"xd7x9b",		"xd7x9c",		"xd7x9d",		"xd7x9e",		"xd7x9f",		// 232 - 239
			"xd7xa0",		"xd7xa1",		"xd7xa2",		"xd7xa3",		"xd7xa4",		"xd7xa5",		"xd7xa6",		"xd7xa7",		// 240 - 247
			"xd7xa8",		"xd7xa9",		"xd7xaa",		"xefxbfxbd",	"xefxbfxbd",	"xe2x80x8e",	"xe2x80x8f",	"xefxbfxbd"		// 248 - 255
		};

	size_t					InputLen			= 0;
	size_t					OutputLen			= 0;
	int						i;
	unsigned char			*UTF8_translation;

	// The first time the function is executed, initialize everything.
	if (!Initialized)
	{
		int			j;
		int			UTF_CharLen;
		char		FirstChar;
		char		SecondChar;
		short		FirstNum		= 0;
		short		SecondNum		= 0;

		Initialized = 1;	// So we run this block of code only once.

		// Clear the translation array.
		memset ((char *)UTF8, (char)0, sizeof (UTF8));

		// Character 1 through 127 is the same in UTF-8 as in regular text.
		for (i = 1; i < 128; i++)
		{
			UTF8 [i] [0] = (char)i;	// Length is 1 by definition.
		}

		// Characters 128-255 translate to 2- or 3-byte UTF-8 equivalents.
		// Note that I'm not super-worried about efficiency here, since this
		// setup code runs only once. Still, it should run reasonably fast.
		for (i = 128; i < 256; i++)
		{
			// In the translation data I downloaded, each output byte is in the
			// form "x##", with # representing a hexadecimal digit 0-9, a-f.
			UTF_CharLen = strlen (TranslationSource [i - 128]) / 3;

			for (j = 0; j < UTF_CharLen; j++)
			{
				FirstChar	= TranslationSource [i - 128] [1 + (j * 3)];
				SecondChar	= TranslationSource [i - 128] [2 + (j * 3)];

				if ((FirstChar	>= '0') && (FirstChar	<= '9'))
					FirstNum = (short)(FirstChar - '0');
					else
				if ((FirstChar	>= 'a') && (FirstChar	<= 'f'))
					FirstNum = 10 + (short)(FirstChar - 'a');
					else
				if ((FirstChar	>= 'A') && (FirstChar	<= 'F'))
					FirstNum = 10 + (short)(FirstChar - 'A');
					else FirstNum = 0;


				if ((SecondChar	>= '0') && (SecondChar	<= '9'))
					SecondNum = (short)(SecondChar - '0');
					else
				if ((SecondChar	>= 'a') && (SecondChar	<= 'f'))
					SecondNum = 10 + (short)(SecondChar - 'a');
					else
				if ((SecondChar	>= 'A') && (SecondChar	<= 'F'))
					SecondNum = 10 + (short)(SecondChar - 'A');
					else SecondNum = 0;

				UTF8 [i] [j] = (char)((FirstNum * 16) + SecondNum);
			}	// Loop through a single UTF-8 character translation string.
		}	// Loop through all 128 UTF-8 character translation strings.
	}	// Need to initialize the function.


	// See if the input string is long enough to require re-allocating the output string.
	// Assume that all input characters will need 3 UTF-8 bytes; and if we're extending
	// the output buffer, add an extra 3000 bytes (= 1000 bytes of input).
	InputLen = strlen (Win1255_in);

	if ((InputLen * 3) > OutBuf_needed)
		OutBuf_needed = 3000 + (InputLen * 3);

	// This condition will always be true the first time we execute.
	if (OutBuf_needed > OutBuf_allocated)
	{
		// The first time, OutBuf = NULL so realloc() works like malloc().
		OutBuf = realloc (OutBuf, (1 + OutBuf_needed));

		if (OutBuf != NULL)
		{
			OutBuf_allocated = OutBuf_needed;
		}
		else
		{
			// If for some reason we couldn't (re-)allocate the output buffer, return
			// with an error value of -1.
			GerrLogMini (GerrId, "WinHebToUTF8() couldn't allocate output buffer (length %d).", (1 + OutBuf_needed));
			return (-1);
		}
	}	// Need to (re-)allocate the output buffer.


	// Now it's time to do the actual translation from Win-1255 to UTF-8!
	// For now (at least), use strcat() to build the output buffer. If this turns out to be
	// too slow, we can replace strcat() with an inline equivalent. Note that we're using
	// the original character value as a subscript to the translation array UTF8 - this way
	// we don't need an "if" to differentiate between ordinary characters 1-127 and special
	// (including Hebrew) characters 128-255. Doing it this way keeps the code super-simple
	// at the cost of all those strcat() calls.
	// DonR 22Nov2021: I went ahead and built a strcat-free version. To make things a little
	// simpler (?) and mmmmaybe a little faster, added a simple char * variable, UTF8_translation,
	// to replace multiple references to UTF8 [Win1255_in [i]].
	for (i = OutputLen = 0; i < InputLen; i++)
	{
		// Since the UTF-8 translation result is referenced multiple times,
		// save it to a simple pointer variable.
		UTF8_translation = UTF8 [Win1255_in [i]];

		// Copy the UTF-8 translation of the input byte to the output buffer.
		// We know that at least one byte will be copied, so we use "do...while"
		// instead of "while...do".
		do
		{
			// Copy one byte, then increment both the output-buffer subscript
			// *and* the UTF-8 translation pointer by one. Note that the final
			// NULL byte of the translation is *not* copied, so at the end of
			// the loop we have to terminate the output string "by hand"!
			OutBuf [OutputLen++] = *UTF8_translation++;
		}
		while (*UTF8_translation);
	}	// Buffer-translating loop.

	// Don't forget to terminate the output string! Since we're now building the output
	// buffer byte-by-byte, it doesn't get terminated automatically.
	OutBuf [OutputLen] = (char)0;

//	GerrLogMini (GerrId, "WinHebToUTF8: Input length %d, output length %d. Converted\n{%s}\nto\n{%s}", InputLen, OutputLen, Win1255_in, OutBuf);

	// The calling function is responsible for copying the translated UTF-8 output
	// to its own storage.
	if (UTF8_out != NULL)
		*UTF8_out = OutBuf;

	if (length_out != NULL)
		*length_out	= OutputLen;

	return (0);
}	// End of WinHebToUTF8 ().


/*=======================================================================
||																		||
||				GetMonsDif()											||
||																		||
|| dif = YYYY/MM/DD - YYYY/MM/DD										||
|| returns diff in COMPLETED months.									||
|| NOTE: base_date is the "to" date, and sub_date is the "from" date!	||
 =======================================================================*/
int	GetMonsDif( int base_date,    int sub_date )
{
	int dif;
	int	to_year;
	int	to_month;
	int	to_day;
	int	*month_len;
	int regular_year	[13] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
	int leap_year		[13] = {0, 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

	// DonR 29Nov2012: Sanity check.
	if (base_date < 19000101)
		return (-1);

	to_year		= base_date / 10000;
	to_month	= (base_date / 100) % 100;
	to_day		= base_date % 100;

	dif  = (to_year		- (sub_date / 10000)) * 12;		// Diff in years * 12.
	dif += (to_month	- ((sub_date / 100) % 100 ));	// Add diff in months.

	// DonR 03Mar2004: Add a bit of precision by looking at days as well
	// as months and years.
	// DonR 29Nov2012: Refine this logic to take month length into account.
	// Basically, if the "to date" is the last day of the month, assume
	// that it's the 31st for purposes of comparison.
	if ((to_year % 4 == 0) && ((to_year % 100 != 0) || (to_year % 400 == 0)))
		month_len = leap_year;
	else
		month_len = regular_year;

	if (to_day >= month_len [to_month])
		to_day = 31;	// We may be at the end of a month with < 31 days.

	if (to_day < (sub_date % 100))
		dif--;

	return (dif);
}



/*=======================================================================
||																		||
||				GetDaysDiff()											||
||																		||
|| dif = YYYY/MM/DD - YYYY/MM/DD										||
|| returns diff in days.												||
||																		||
|| Note that the first argument is normally the *later* day!			||
||																		||
 =======================================================================*/
int    GetDaysDiff( int base_day,    int sub_day )
{
/*=======================================================================
||			    Local declarations										||
 =======================================================================*/
    struct tm   time_str;
    time_t      vac;
/*=======================================================================
||				Body of function										||
 =======================================================================*/
    /*
     * Get base date unit time
     * -----------------------
     */
    date_to_tm( base_day, &time_str );
    vac = mktime( &time_str );
    /*
     *  Get substract date unit time
     *  ----------------------------
     */
    date_to_tm( sub_day, &time_str );
    vac -= mktime( &time_str );
    /*
     *  Set difference in days
     *  ----------------------
     */
    return( vac / 86400 );
}



/*=======================================================================
||																		||
||				  AddDays()												||
||																		||
|| date = YYYY/MM/DD + days												||
|| returns date + days.													||
|| Note that this function has an "effective range" of 68 years!		||
 =======================================================================*/
int    AddDays( int base_day,    int add_day )
{
/*=======================================================================
||			    Local declarations				||
 =======================================================================*/
    struct tm   time_str;
    time_t      vac;
/*=======================================================================
||				Body of function			||
 =======================================================================*/
    date_to_tm( base_day, &time_str );
//20020201 add zero in all fields because other way that not work 
	time_str.tm_sec   = 0;
	time_str.tm_min   = 0;
	time_str.tm_hour  = 0;
	time_str.tm_wday  = 0;
	time_str.tm_yday  = 0;
	time_str.tm_isdst = 0;
#ifndef _LINUX_
	time_str.tm_tzadj = (long)0;
	time_str.tm_name[ 0 ] = '\0';
#endif


    vac = mktime( &time_str ) + add_day * 86400;
    return( tm_to_date( localtime( &vac )));
}



/*=======================================================================
||																		||
||				AddMonths()												||
||																		||
 =======================================================================*/
int	AddMonths (int base_date, int add_months)
{
	int dif;
	int	to_year;
	int	to_month;
	int	to_day;
	int	*month_len;
	int regular_year	[13] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
	int leap_year		[13] = {0, 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

	// Sanity check.
	if (base_date < 19000101)
		return (-1);

	// First, decode the starting date.
	to_year		= base_date / 10000;
	to_month	= (base_date / 100) % 100;
	to_day		= base_date % 100;

	// Adjust the month, then correct if the new value is less than one or more than 12.
	// Note that this could be done without the iterative while loops - but since these
	// simple integer operations work very fast anyway, and we're generally dealing with
	// smallish numbers, it seemed best to keep the logic simple and iterative.
	to_month += add_months;
	while (to_month < 1)
	{
		to_year--;
		to_month += 12;
	}
	while (to_month > 12)
	{
		to_year++;
		to_month -= 12;
	}

	// We need to adjust the day only if the base-date day doesn't exist in the target
	// month - e.g. if we incremented 29, 30, or 31 January (or 31 March) by one month.
	// Since we're using this routine to determine "month-birthdays" (i.e. what was the
	// date when someone turned X months old), we want to deal with generic end-of-month-X
	// to end-of-month-Y rather than count days.
	if ((to_year % 4 == 0) && ((to_year % 100 != 0) || (to_year % 400 == 0)))
		month_len = leap_year;
	else
		month_len = regular_year;

	if (to_day > month_len [to_month])
		to_day = month_len [to_month];

	return ((to_year * 10000) + (to_month * 100) + to_day);
}




/*=======================================================================
||																		||
||				  AddSeconds()											||
||																		||
|| date = YYYY/MM/DD + days												||
|| returns date + days.													||
 =======================================================================*/
int    AddSeconds( int base_time,    int add_sec )
{
/*=======================================================================
||			    Local declarations				||
 =======================================================================*/
    struct tm   *time_str;
    time_t      vac;
/*=======================================================================
||				Body of function			||
 =======================================================================*/
  vac = time( NULL );
  
  time_str  = localtime( &vac );

  time_to_tm( base_time, time_str );

  vac = mktime( time_str ) + add_sec;
  return( tm_to_time( localtime( &vac )));
}



/*=======================================================================
||																		||
||				  ConvertUnitAmount ()									||
||																		||
|| Converts amounts in grams or micrograms to milligrams;				||
|| converts amount in MIU (milli-IU) to IU;								||
|| leaves amounts in other units alone.									||
 =======================================================================*/
double ConvertUnitAmount (double amount_in, char *unit_in)
{
	double	factor	= 1.0;	// Unless the source unit is MIU, mcg, or g, we simply return the input as the output.

	if (	(!strcasecmp (unit_in, "MIU"))	||
			(!strcasecmp (unit_in, "mcg"))		)
	{
		factor = 0.001;
	}
	else
	{
		// DonR 18Mar2024 User Story #299818: Make sure we recognize "g" the same
		// as "g  ", since in at least one case we're stripping spaces from the
		// Unit Code.
		if (	(!strcasecmp (unit_in, "g  "))	||
				(!strcasecmp (unit_in, "g"  ))		)
		{
			factor = 1000.0;
		}
	}

	return (amount_in * factor);
}



/*=======================================================================
||																		||
||				  ParseISODateTime ()									||
||																		||
|| Converts datetime format (YYYY-MM-DDThh:mm:ss)						||
|| to YYYYMMDD and HHMMSS												||
|| Marianna 20250422													||
 =======================================================================*/
int ParseISODateTime (const char* datetime_in, int* date_out, int* time_out)
{
	int year, month, day, hour, minute, second;

	// Check if any of the input are NULL
	if (datetime_in == NULL || date_out == NULL || time_out == NULL)
	{
		return -1;
	}

	// Parse ISO format: "YYYY-MM-DDThh:mm:ss"
	if (sscanf(datetime_in, "%d-%d-%dT%d:%d:%d", &year, &month, &day, &hour, &minute, &second) != 6)
	{
		return -1; // Return error if format doesn't match
	}

	// Check validate ranges 
	if (year < 0 || year > 9999 || month < 1  || month > 12  || day < 1    || day > 31 ||
		hour < 0 || hour > 23   || minute < 0 || minute > 59 || second < 0 || second > 59)
	{
		return -1; // Return error if values are out of valid ranges
	}

	// Convert to YYYYMMDD and HHMMSS formats
	*date_out = (year * 10000) + (month * 100) + day;
	*time_out = (hour * 10000) + (minute * 100) + second;

	return 0;
}