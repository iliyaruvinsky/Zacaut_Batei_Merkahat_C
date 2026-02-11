/*=======================================================================
||																		||
||								FatherProcess.c							||
||																		||
||======================================================================||
||																		||
||	Description:														||
||	----------- 														||
||	Server for starting the MACABI system and keeping all servers up.	||
||	Servers to start up and parameters are read from database.			||
||																		||
||																		||
||	Usage:																||
||	-----																||
||	FatherProcess.exe													||
||																		||
||======================================================================||
||	WRITTEN BY	: Ely Levy ( Reshuma )									||
||	DATE		: 30.05.1996											||
=======================================================================*/


#define MAIN
#define FATHER
#define NO_PRN

static	 char	*GerrSource = __FILE__;

#include <MacODBC.h>
#include "MsgHndlr.h"


#ifndef NO_PRN    ////////////////// GAL  19/10/2003 ////////////////
 #define prn( msg ) \
   printf("{pid=%d, ppid=%d}, %s\n", getpid(), getppid(), msg); \
   fflush(stdout);

 #define prn_1( msg , d )	\
 {							\
	sprintf( buf ,msg ,d );	\
	prn( buf );				\
 }

 #define prn_2( msg , d , e )	\
 {								\
	sprintf( buf ,msg ,d ,e );  \
	prn( buf );					\
 }
#else	// DonR 25Apr2022: Added NO_PRN stubs for diagnostics.
 #define prn( msg ) \
   fflush(stdout);

 #define prn_1( msg , d )	\
 {							\
	sprintf( buf ,msg ,d );	\
	prn( buf );				\
 }

 #define prn_2( msg , d , e )	\
 {								\
	sprintf( buf ,msg ,d ,e );  \
	prn( buf );					\
 }

#endif			  ////////////////// GAL  19/10/2003 ////////////////

#define Conflict_Test(r)										\
	if (SQLERR_code_cmp (SQLERR_access_conflict) == MAC_TRUE) 	\
{																\
	r = MAC_TRUE;												\
	break;														\
}

#define PROG_KEY_LEN			31
#define PARAM_NAM_LEN			31
#define PARAM_VAL_LEN			31
			     
TABLE		*parm_tablep,
//			*parm_tablep2,
			*proc_tablep,
			*updt_tablep,
			*stat_tablep,
			*tstt_tablep,
			*dstt_tablep,
//			*sevr_tablep,
//			*pcnt_tablep,
//			*dcdr_tablep,
//			*intr_tablep,
			*prsc_tablep,
//			*prw_tablep,
			*msg_recid_tablep,	// DonR 03Jul2005
//			*delpr_id_tablep,	// DonR 04Jul2005
//			*drex_tablep,		// uncommented by GAL 20/10/2003
			*dipr_tablep;
//			*phrm_tablep;

TABLE	**table_ptrs[] =
{
		&parm_tablep,/* 			PARM_TABLE				*/
		&proc_tablep,/* 			PROC_TABLE				*/
		&stat_tablep,/* 			STAT_TABLE				*/
		&tstt_tablep,/* 			TSTT_TABLE				*/
		&dstt_tablep,/* 			DSTT_TABLE				*/
		&updt_tablep,/* 			UPDT_TABLE				*/
//		&phrm_tablep,/* 			PHRM_TABLE				*/
//		&sevr_tablep,/* 			SEVR_TABLE				*/
//		&pcnt_tablep,/* 			PCNT_TABLE				*/
//		&dcdr_tablep,/* 			DCDR_TABLE				*/
//		&intr_tablep,/* 			INTR_TABLE				*/
		&msg_recid_tablep,	/* Message Rec_id table create BEFORE presc! */
//		&delpr_id_tablep,	/* Message Rec_id table create BEFORE presc! */
		&prsc_tablep,/* 			PRSC_TABLE				*/
//		&prw_tablep, /* 			PRW_TABLE				*/
//		&drex_tablep,/* 			DREX_TABLE				*/ // uncommented by GAL 20/10/2003
		&dipr_tablep,/* 			DIPR_TABLE				*/
		NULL,		 /* 			NULL					*/
};

// DonR 09Aug2022: Add new array to store instance-control information. This
// is to support microservices, where we may have several multiple-instance
// programs running as part of the application.
TInstanceControl	InstanceControl [MAX_PROC_TYPE_USED + 1];

int		run_system			(int sys_flgs);
int		GetProgParm			(char *prog_name, char *param_name, char *param_value);
void	TerminateHandler	(int signo);

static int		running_system;

int				caught_signal			= 0;	// DonR 10Apr2022: This is now in actual use for SIGTERM (Signal 15, "soft" system shutdown").
int				TikrotProductionMode	= 1;	// To avoid "unresolved external" compile errors.
int				UseODBC = 1;

// DonR 10Apr2022: Add capability to perform an orderly shutdown
// on receipt of Signal 15 ("soft" terminate).
struct sigaction	sig_act_terminate;


/*=======================================================================
||																		||
||																		||
||								main()									||
||																		||
||																		||
=======================================================================*/
int main (int argc, char *argv[])
{
	char		term_mesg		[2048],
				CoreName		[ 128],
				buf				[ 512],
				save_log_file	[ 512],
				*message,
				*system_env,
				*system_for_log;
	time_t		cur_time,
				curr_time		[TOTAL_INTERVALS],
				doc_curr_time	[TOTAL_DOC_INTERVALS];
	struct tm	*tmptr;
	pid_t		son_pid;
	int 		status,
				restart,
				running_count,
				nonbusy_count,
				NewInstancesToOpen,
				pharm_proc_count,
				doc_proc_count,
				shut_down,
				proc_found,
				state,
				counter,
				message_idx,
				term_len,
				*status_ptr,
				i,
				j,
				k,
				cnt,
				days,
				hours,
				mins,
				secs,
				sons, 
				system,
				err,
				died_pr_flg,
				match,
				log_shutdown_msg,
				ProgramType;
	
	short		relevant_status;

	TABLE_DATA			table_data;
	PROC_DATA			proc_data;
	DIPR_DATA			dipr_data;
	STAT_DATA			stat_data;
	TSTT_DATA			tstt_data;
	DSTT_DATA			dstt_data;
	UPDT_DATA			updt_data;
	TInstanceControl	*IC;	
	extern TABLE_DATA	TableTab[];
	static time_t		last_wait_time = (time_t)0;	// DonR 11May2022: Added initialization.

	// DonR 10Apr2022: Add signal processing to permit handling
	// of "soft shutdown" signal 15 (SIGTERM).
	sigset_t	NullSigset;

	// File pointer for manually appending to system_log.
	FILE		*SystemLog = NULL;

	/*=======================================================================
	||																		||
	||								INITIALIZATION							||
	||																		||
	=======================================================================*/
	
	prn ("FatherProcess starting...");  // GAL  19/10/2003
	
	// Be sure that 0-2 file descriptors remain open across exec
	if( fcntl( 0, F_SETFD, 0 ) == -1 ||
		fcntl( 1, F_SETFD, 0 ) == -1 ||
		fcntl( 2, F_SETFD, 0 ) == -1 )
	{
		GerrLogMini (GerrId, "fcntl failed " GerrErr, GerrStr);
	}

	// Set to session leader
	ABORT_ON_ERR (setsid ());

	// Initialize environment buffers
	ABORT_ON_ERR (InitEnv (GerrId));

	// Hash environment into buffers
	ABORT_ON_ERR (HashEnv (GerrId));

	// Get current program short name
	GetCurrProgName (argv[0], CurrProgName);


	// DonR 10Apr2022: Set up signal handler for Signal 15 (SIGTERM),
	// which will trigger a "soft" system shutdown.
	memset ((char *)&NullSigset, 0, sizeof(sigset_t));

	// DonR 11Aug2022: Initialize new InstanceControl array.
	memset ((char *)&InstanceControl, 0, sizeof(InstanceControl));

    sig_act_terminate.sa_handler	= TerminateHandler;
    sig_act_terminate.sa_mask		= NullSigset;
    sig_act_terminate.sa_flags		= 0;

	if (sigaction (SIGTERM, &sig_act_terminate, NULL) != 0)
	{
		GerrLogMini (GerrId, "FatherProcess: can't install signal handler for SIGTERM" GerrErr, GerrStr);
	}

	// NOTE: As of 18May2022, MacODBC automatically installs a segmentation-fault
	// handler to trap and diagnose bad-parameter errors.


	// Connect to database.
	// DonR 15Jan2020: SQLMD_connect (which resolves to INF_CONNECT) now
	// handles ODBC connections as well as the old Informix DB connection.
	do
	{
		SQLMD_connect ();
		if (!MAIN_DB->Connected)
		{
			sleep (10);
			GerrLogMini (GerrId, "\n\n\nRetrying %s ODBC connect.", MAIN_DB->Name);
		}
	}
	while (!MAIN_DB->Connected);


	GerrLogMini (GerrId, "\nMACABI WATCHDOG SERVER STARTUP");

	// Get initial parameters from setup table.
	ABORT_ON_ERR (SqlGetParamsByName (FatherParams, PAR_LOAD));
// GerrLogMini(GerrId, "Back from SqlGetParamsByName.");

	// Lock pages in primary memory
	if (LockPagesMode == 1)
	{
#ifdef _LINUX_
		if (mlockall (MCL_CURRENT))	// GAL 2/01/05 Linux
#else
		if (plock (PROCLOCK))
#endif
		{
			GerrLogMini (GerrId, "Can't lock pages in primary memory - error (%d) %s.", GerrStr);
		}
	}
	
	// Return to unprivileged ("macabi") userid.
	if (setuid (atoi (MacUid)))
	{
		GerrLogMini (GerrId, "Can't setuid to %d - error (%d) %s.", atoi (MacUid), GerrStr);
	}
	
	// Get current program named pipe and listen on it.
	GetCurrNamedPipe (CurrNamedPipe);
	ABORT_ON_ERR (ListenSocketNamed (CurrNamedPipe));

	// Create Maccabi system semaphore.
	ABORT_ON_ERR (CreateSemaphore ());
	
	// Init shared memory
	ABORT_ON_ERR (InitFirstExtent (SharedSize));
	
	// Create & Refresh all needed tables in shared memory
	prn ("FatherProcess: Create & Refresh all needed tables in shared memory.\n");  // GAL  19/10/2003
// GerrLogMini (GerrId, "Starting CreateTable loop...");
	for (len = 0; TableTab[len].table_name[0]; len++)
	{
//		GerrLogMini (GerrId, "Creating TableTab[%d] = %s.", len, TableTab[len].table_name);
		ABORT_ON_ERR (CreateTable (TableTab[len].table_name, table_ptrs[len]));

		if (TableTab[len].refresh_func != NULL)
		{
			// If the memory table's refresh function pointer is non-NULL, call it
			// with the array subscript as its one argument.
			ABORT_ON_ERR ((*TableTab[len].refresh_func)(len));
			
			// Create and insert table-updated data for this shared-memory table.
			strcpy (updt_data.table_name, TableTab[len].table_name);
			updt_data.update_date = GetDate();
			updt_data.update_time = GetTime();
			state = AddItem (updt_tablep, (ROW_DATA)&updt_data);
			
			if (ERR_STATE (state))
			{
				GerrLogMini (GerrId, "Can't insert record for table '%s' into shm", updt_data.table_name);
			}
		}	// Shared-memory table has a non-NULL refresh function pointer.
	}	// Loop through shared-memory table array.


	// Update system status in shared memory.
	stat_data.status		= GOING_UP;
	stat_data.pharm_status	= GOING_UP;
	stat_data.doctor_status	= GOING_UP;
	stat_data.param_rev		= 0;
	stat_data.tsw_flnum		= 0;
	stat_data.sons_count	= 0;
	
	ABORT_ON_ERR (AddItem (stat_tablep, (ROW_DATA)&stat_data));
	
	// Load parameters from db into shm table.
	sql_dbparam_into_shm (parm_tablep, stat_tablep, PAR_LOAD);
	
	// Add myself to the shared-memory processes table.
	ABORT_ON_ERR (AddCurrProc (0, FATHERPROC_TYPE, 0, PHARM_SYS | DOCTOR_SYS));
	
	// Create and insert pharmacy statistics row in shared memory.
	// DonR 01May2022: As I note below, this feature is not in real use
	// any longer, and should probably just be deleted from the system.
	bzero ((char*)&tstt_data, sizeof(tstt_data));
	
	tstt_data.start_time = curr_time [SECOND_INTERVAL] = time (NULL);
	
	curr_time [MINUTE_INTERVAL]	= curr_time [SECOND_INTERVAL] / 60;
	curr_time [HOUR_INTERVAL]	= curr_time [SECOND_INTERVAL] / 3600;
	curr_time [DAY_INTERVAL]	= curr_time [SECOND_INTERVAL] / 86400;
	
	// DonR 01May2022: Note that as of today, the PharmMessages list is way, way
	// out of date - the last message code there is 2070, meaning that it's missing
	// all the current drug-sale transactions as well as a whole lot of other stuff.
	// Given that (as far as I know) nobody is actually accessing these statistics
	// stored in shared memory, this is harmless. At some point, we should probably
	// just stop putting this stuff into memory since nobody is using the feature
	// anymore.
	for (message_idx = 0; message_idx < TOTAL_MSGS; message_idx++)
	{
		tstt_data.last_time  [message_idx] [SECOND_INTERVAL]	= curr_time [SECOND_INTERVAL];
		tstt_data.last_time  [message_idx] [MINUTE_INTERVAL]	= curr_time [MINUTE_INTERVAL];
		tstt_data.last_time  [message_idx] [HOUR_INTERVAL]		= curr_time [HOUR_INTERVAL];
		tstt_data.last_time  [message_idx] [DAY_INTERVAL]		= curr_time [DAY_INTERVAL];
		
		tstt_data.time_count [message_idx] [MSGNUM_ENTRY] = PharmMessages [message_idx];
	}
	
	ABORT_ON_ERR (AddItem (tstt_tablep, (ROW_DATA)&tstt_data));
	
	
	// Create and insert doctor-system statistics row in shared memory.
	// DonR 01May2022: As I note above, this feature is not in real use
	// any longer, and should probably just be deleted from the system.
	bzero ((char*)&dstt_data, sizeof(dstt_data));
	
	dstt_data.start_time = doc_curr_time [SECOND_INTERVAL] = time( NULL );
	
	doc_curr_time [MINUTE_INTERVAL]	= doc_curr_time [SECOND_INTERVAL] / 60;
	doc_curr_time [HOUR_INTERVAL]	= doc_curr_time [SECOND_INTERVAL] / 3600;
	doc_curr_time [DAY_INTERVAL]	= doc_curr_time [SECOND_INTERVAL] / 86400;
	
	for (message_idx = 0; message_idx < TOTAL_DOC_MSGS; message_idx++)
	{
		dstt_data.last_time  [message_idx] [SECOND_INTERVAL]	= doc_curr_time [SECOND_INTERVAL];
		dstt_data.last_time  [message_idx] [MINUTE_INTERVAL]	= doc_curr_time [MINUTE_INTERVAL];
		dstt_data.last_time  [message_idx] [HOUR_INTERVAL]		= doc_curr_time [HOUR_INTERVAL];
		dstt_data.last_time  [message_idx] [DAY_INTERVAL]		= doc_curr_time [DAY_INTERVAL];
		
		dstt_data.time_count [message_idx] [MSGNUM_ENTRY] = DoctorMessages [message_idx];
	}
	
	ABORT_ON_ERR (AddItem (dstt_tablep, (ROW_DATA)&dstt_data));


	// RUN PROCESSES FOR BOTH SYSTEMS.
	system_env = getenv ("MAC_SYS");

	// If we can't find "MAC_SYS" in the environment, assume that we're
	// running a combined doctor/pharmacy system. (Note that this seems
	// to be how we're working in Production, even though we do not in
	// fact run both systems on the same server - we just delete the
	// programs that are not applicable.)
	// Note that running_system is a bitmap - and thus we construct it
	// using bitwise-OR operators ("|").
	if (system_env == NULL)
	{
		running_system	= PHARM_SYS | DOCTOR_SYS;
		system_for_log	= "combined pharmacy/doctor (defaulted)";
	}
	else if (system_env[0] == (char)0)
	{
		running_system	= PHARM_SYS | DOCTOR_SYS;
		system_for_log	= "combined pharmacy/doctor (defaulted)";
	}
	else
	{
		// Build the running_system bitmap based on the contents of system_env.
		running_system	=	(strstr( system_env, "pharm"		) ? PHARM_SYS		: 0 )	|
							(strstr( system_env, "doctor"		) ? DOCTOR_SYS		: 0 )	|
							(strstr( system_env, "doc_tcp_only"	) ? DOCTOR_SYS_TCP	: 0 );		// GAL 19/10/2003 added DOCTOR_SYS_TCP.

		system_for_log	=	system_env;
	}

	prn_2 ("FatherProcess running system %s (bitmap %d).\n", system_for_log, running_system);

	GerrLogMini (GerrId, "\nFatherProcess PID %d running %s system.", getpid(), system_for_log);

	// For some reason, we're passing the "system" argument to run_system() as an
	// "all systems" constant, even though we just went through the logic of building
	// the running_system bitmap. Maybe this should be changed...
    run_system ( PHARM_SYS | DOCTOR_SYS | DOCTOR_SYS_TCP );  // GAL 19/10/2003 added DOCTOR_SYS_TCP.

//	GerrLogMini (GerrId, "FatherProcess %d back from run_system().", getpid());

	// At this point, we have (hopefully) started all child processes that
	// should be running.


	/*=======================================================================
	||																		||
	||							WATCH THE SYSTEM.							||
	||																		||
	||				CHECK IF NEED TO DO ONE OF THE FOLLOWING:				||
	||																		||
	||				1) RE-RUN A SERVER THAT ENDED.							||
	||				2) SHUT DOWN ALL SERVERS.								||
	||				3) RELOAD PARAMETERS FROM DB INTO SHARED MEMORY 		||
	||																		||
	=======================================================================*/
	
	// This is the main "watchdog" loop - it keeps running until the
	// Maccabi application needs to be shut down.
	//
	// DonR 11Apr2022: Added (!caught_signal) as a condition - if we catch
	// a signal (typically 15 == SIGTERM) we want to terminate the entire
	// application.
	for (shut_down = MAC_FALS; (shut_down == MAC_FALS) && (!caught_signal); )
	{
#ifndef NO_PRN
		// Print in-memory table statistics to stdio - disable if NO_PRN is defined.
		do
		{
			time_t		curr_time		= time (NULL);
			static int	to_print_flg	= 1;
			
			if ((curr_time % 3600) == 0)
			{
				if (to_print_flg)
				{
					ListTables ();
					to_print_flg = 0;	// So we print these diagnostics only once per hour.
				}
			}
			else
			{
				to_print_flg = 1;	// Set to print diagnostics next hour.
			}
		}
		while (0);
#endif
		
		// Analyze system state.
		
		// Get count of child processes from the in-memory statistics table.
		GetSonsCount (&sons);
//		GerrLogMini (GerrId, "At %d, sons = %d.", GetTime(), sons);
		
		// Get system status from shared memory table.
		get_sys_status (&stat_data);
		
//		printf ("System status %d, pharm stat %d, doctor stat %d.\n",
//				stat_data.status, stat_data.pharm_status, stat_data.doctor_status ); fflush(stdout);

		// Handle status of the whole system.
		switch (stat_data.status)
		{
			case GOING_UP:		// Count running child servers.
								// NOTE: Despite the previous comment, at present this loop does NOT
								// count child processes - it just scans the list to look for an
								// instance of "control.exe". Since control.exe may not even be used
								// for anything nowadays, this is probably not the best way to establish
								// that the system is up - maybe change the logic so we actually *do*
								// check for a minimum number of child processes? Or maybe look for one
								// or more processes *other* than control.exe? Or even make things
								// really simple and just look at the "sons" value we just retrieved
								// from the "statistics" table?
								// DonR 11Aug2022: Going ahead based on the previous comment. This *should*
								// make control.exe a completely optional (and, as far as I can tell, a
								// completely useless) program!
								if (sons > 0)
								{
									set_sys_status (SYSTEM_UP, 0, 0);
								}

								break;

			
			case SYSTEM_UP:		// If the system is already up, nothing needs to be done here.
								break;

			
			case GOING_DOWN:	// If the "sons" value we just retrieved from the "statistics"
								// table is greater than one, the system is not yet fully down.
								// Only if it's 1 or less do we want to change the status to
								// SYSTEM_DOWN.
								// DonR 11Aug2022: I *think* that if the system is fully down,
								// "sons" will be zero rather than one. I'll try making the
								// change and see if "kill_sys" still brings the system down
								// as it's supposed to.
								if (sons < 1)
								{
									// No child process is running - now is the time to shut down.
									set_sys_status (SYSTEM_DOWN, 0, 0);
								}
			
								break;


			case SYSTEM_DOWN:	// If the "sons" value is zero, then the application really has
								// been fully shut down, and we can exit the main "watchdog" loop.
								if (sons < 1)
								{
									// Set shut_down TRUE to exit "watchdog" loop.
									shut_down = MAC_TRUE;
									continue;
								}

								break;	// I.e. there is still at least one child process running.

			
			default:			// Unknown system state - log it!
								GerrLogMini (GerrId, "FatherProcess: unknown system state %d.", stat_data.status);
								break;

		}	// Switch on whole-system state.
		

		// Check for running doctor-system and pharmacy-system child processes.
		// DonR 04May2022: Use a single loop to count both doctor and pharmacy
		// processes - this makes the code a little more compact.
		RewindTable (proc_tablep);

		for (pharm_proc_count = doc_proc_count = 0; ActItems (proc_tablep, 0, GetItem, (OPER_ARGS)&proc_data) == MAC_OK; )
		{
			// Check for a doctor-system process.
			if (proc_data.system == DOCTOR_SYS)
			{
				doc_proc_count++;
			}
			else
			// Check for a pharmacy-system process.
			if (proc_data.system == PHARM_SYS)
			{
				pharm_proc_count++;
			}

			// If we've found at least one child process for each application,
			// we can quit the loop.
			if ((doc_proc_count > 0) && (pharm_proc_count))
			{
				break;
			}
		}	// Loop through processes table to find doctor-system and pharmacy-system child processes.
		
		// Handle status of pharmacy and doctor system. We just ran get_sys_status(), but
		// let's do it again just to make sure stat_data is current.
		get_sys_status (&stat_data);
		
		switch (stat_data.pharm_status)
		{
			case GOING_UP:		// If at least one pharmacy-system child process is running,
								// mark the pharmacy system as "up and running".
								if (pharm_proc_count > 0)
								{
									set_sys_status (0, SYSTEM_UP, 0);
								}
								break;

			case GOING_DOWN:	// If there are no pharmacy-system child processes running,
								// mark the pharmacy system as "down".
								if (pharm_proc_count < 1)
								{
									set_sys_status (0, SYSTEM_DOWN, 0);
								}
								break;
			
			case SYSTEM_DOWN:
			case SYSTEM_UP:		// If the pharmacy-system status is already either "up
								// and running" or "down", there's nothing to do here.
								break;
			
			default:			// Unknown value for pharmacy-system state - log it!
								GerrLogMini (GerrId, "FatherProcess: unknown pharmacy-system state %d.", stat_data.pharm_status);
								break;

		}	// Switch on pharmacy-system state.

		// Now do the same for the doctor system.
		switch (stat_data.doctor_status)
		{
			case GOING_UP:		// If at least one doctor-system child process is running,
								// mark the doctor system as "up and running".
								if (doc_proc_count > 0)
								{
									set_sys_status (0, 0, SYSTEM_UP);
								}
								break;

			case GOING_DOWN:	// If there are no doctor-system child processes running,
								// mark the doctor system as "down".
								if (doc_proc_count < 1)
								{
									set_sys_status (0, 0, SYSTEM_DOWN);
								}
								break;
			
			case SYSTEM_DOWN:
			case SYSTEM_UP:		// If the doctor-system status is already either "up
								// and running" or "down", there's nothing to do here.
								break;
			
			default:			// Unknown value for doctor-system state - log it!
								GerrLogMini (GerrId, "FatherProcess: unknown doctor-system state %d.", stat_data.doctor_status);
								break;

		}	// Switch on doctor-system state.


		/*=======================================================================
		||																		||
		||				CHECK FOR DEAD / STOPPED CHILD PROCESSES				||
		||																		||
		=======================================================================*/
		
		died_pr_flg = 0;	// Start with the assumption that no child processes have died.
		
		// Collect status of all child processes. Setting the first argument to (pid_t) -1 means
		// that we're requesting the status of *any* child process that has terminated.
		son_pid = waitpid	(	(pid_t) -1,		// Tell me about any child process that terminated.
								&status,		// Exit status of the terminated child process.
								WNOHANG		);	// Don't wait.

		// If no child process terminated, check the shared-memory died-processes table.
		if (son_pid == 0)
		{
			state = ActItems (dipr_tablep, 1, GetItem, (OPER_ARGS)&dipr_data);
			
			if (state == MAC_OK)	// I.e. we found something in the died-processes table.
			{
				// Get the same values (son_pid and status) that we would have
				// gotten from the waitpid() call above.
				son_pid	= dipr_data.pid;
				status	= dipr_data.status;
				
				// Clear the dead child process from the dies-processes table.
				state = DeleteItem (dipr_tablep, (ROW_DATA)&dipr_data);

				if (ERR_STATE (state))
				{
					GerrLogMini (GerrId, "Can't delete row from died process table");
				}
				
				died_pr_flg = 1;	// We found a dead child process.
			}
			
			/*=======================================================================
			||																		||
			||				CHECK FOR SYSTEM PROCESSES ( NOT SONS ) PHANTOMS		||
			||																		||
			=======================================================================*/
			
			else
			// Nothing was found by waitpid() or in the died-processes table.
			{
				// If the "died processes" table is empty, check for "orphan" processes
				// in the shared-memory "processes" table. We perform this check every
				// five seconds.
				if ((state == NO_MORE_ROWS) && ((time(NULL) - last_wait_time) > 5))
				{
					last_wait_time = time(NULL);	// DonR 11May2022: Moved this here, to make sure that once
													// we've done the 5-second check, we don't do it again until
													// another 5 seconds have passed.

					// We're interested in searching for "orphan" processes only if the system
					// is being partially or entirely brought down.
					get_sys_status (& stat_data);
					
					if (	(stat_data.status			== GOING_DOWN)		||
							(stat_data.doctor_status	== GOING_DOWN)		||
							(stat_data.pharm_status		== GOING_DOWN)	)
					{
						// Rewind "processes" table and loop through it.
						RewindTable (proc_tablep);
						
						for (; ActItems (proc_tablep, 0, GetItem, (OPER_ARGS)&proc_data) == MAC_OK; )
						{
							// Check only processes of the system that's going down - so waitpid() isnt interrupted. (DonR: I don't understand that last part.)
							if (	((stat_data.doctor_status	== GOING_DOWN) && (proc_data.system != DOCTOR_SYS))		||
									((stat_data.pharm_status	== GOING_DOWN) && (proc_data.system != PHARM_SYS)))
							{
								continue;
							}
							
							// Check for a phantom son process. We're assuming that a return of -1 from
							// getpgid() (= Get Process Group ID) means that the process in question has
							// terminated - other reasons for a return of -1 should not be relevant here.
							//
							if (getpgid (proc_data.pid) == -1)
							{
								son_pid = proc_data.pid;
								
								break;
							}
						}	// Loop through shared-memory process table.
						
					} // System is going down.
					
				} // NO_MORE_ROWS in "died processes" table and 5 seconds have passed since the last check.
			}	// Failed to read anything from the shared-memory "died processes" table.
				
		}	// Waitpid() did *not* find a terminated child process.
		
		else
		{
			if( son_pid < 0 )
			{
				if( errno == ECHILD )				/* No child proccess	*/
				{
					shut_down = MAC_TRUE;
					continue;
				}
				
				GerrLogMini (GerrId, "Error while waiting for son process...Error ( %d ) %s", errno, GerrStr);
			}
			
			else
			{
				last_wait_time = time(NULL);
			}
		}


		// Child process died (however we detected it) - log it.
		if (son_pid > 0)
		{
			// 1) set time
			time (&cur_time);
			tmptr = localtime (&cur_time);

			term_len = 0;
				
			// 2) Update son processes count
			AddToSonsCount (-1);

			// 3) Get process details
			proc_data.pid	= son_pid;
			proc_found		= ActItems (proc_tablep, 1, GetProc, (OPER_ARGS)&proc_data);
//GerrLogMini (GerrId, "Looked for dead process %d - proc_found = %d.", son_pid, proc_found);

			// If process was found in the table, delete it from shared memory and do some other cleanup.
			if (proc_found == MAC_OK)
			{
				// 4) Remove son process from shared memory
				DeleteItem (proc_tablep, (ROW_DATA)&proc_data);

				// 5) Remove named pipe file of son process
				if (!access (proc_data.named_pipe, F_OK))
				{
					if (unlink (proc_data.named_pipe))
					{
						GerrLogMini (GerrId, "Can't remove son's named pipe '%s'...error( %d ) %s",
									 proc_data.named_pipe, GerrStr);
					}
				}


				// 6) Update sql child process count
				if ((proc_data.proc_type > 0) && (proc_data.proc_type <= MAX_PROC_TYPE_USED))
				{
					InstanceControl [proc_data.proc_type].instances_running--;
				}


				// 7) Set up the log message.
				secs   = cur_time - proc_data.start_time;
				days   = secs / 86400;
				secs  -= days * 86400;

				hours	=  secs / 3600;
				secs	-= hours * 3600;

				mins	=  secs / 60;
				secs	-= mins * 60;

				term_len += sprintf (	term_mesg + term_len,	"\n\tCommand. . . . . . . : %s"
																"\n\tChild pid. . . . . . : %d"
																"\n\tElapsed. . . . . . . :",
										proc_data.proc_name, proc_data.pid							);

				if (days)	term_len += sprintf (term_mesg + term_len, " %d days",		days);
				if (hours)	term_len += sprintf (term_mesg + term_len, " %d hours",		hours);
				if (mins)	term_len += sprintf (term_mesg + term_len, " %d minutes",	mins);
				if (secs)	term_len += sprintf (term_mesg + term_len, " %d seconds",	secs);

				if ((days + hours + mins + secs) == 0) term_len += sprintf (term_mesg + term_len, " Nothing (immediate).");
			}	// Found dead process in shared-memory processes table.

			else
			{
				term_len += sprintf (	term_mesg + term_len,	"process not found in processes table."
																"\n\tChild pid. . . . . . : %d status(%s)",
										son_pid, (died_pr_flg) ? "grand-child" : "child"					);
			}
			
			term_len += sprintf (term_mesg + term_len, "\n\tMessage. . . . . . . : ");
//prn(term_mesg);
//GerrLogMini(GerrId, "FatherProcess %d: term_mesg = %s.", getpid(), term_mesg);

			// Check cause of death of the child process.
			log_shutdown_msg = 1;	// By default, log any reason child process shut down.
			
			if (WIFEXITED (status))	// Process exited with exit() command.
			{
				switch (WEXITSTATUS (status))
				{
					case MAC_SERV_SHUT_DOWN:	message				= "SHUT-DOWN";
												restart				= MAC_FALS;
												log_shutdown_msg	= 0;	// Don't bother logging a "normal" exit.
												break;

					// MAC_SYST_SHUT_DOWN does not appear to be used anywhere.
					case MAC_SYST_SHUT_DOWN:	message				= "MAC SYSTEM SHUT-DOWN";
												restart				= MAC_FALS;
												set_sys_status (GOING_DOWN, 0, 0);
												break;
					
					case MAC_EXIT_NO_MEM:		message				= "MEMORY ERROR";
												restart				= MAC_TRUE;
												break;
					
					// MAC_EXIT_SQL_ERR does not appear to be used anywhere.
					case MAC_EXIT_SQL_ERR:		message				= "SQL ERROR";
												restart				= MAC_TRUE;
												break;
					
					// MAC_EXIT_SQL_CONNECT does not appear to be used anywhere.
					case MAC_EXIT_SQL_CONNECT:	message				= "ERROR AT CONNECT TO SQL";
												restart				= MAC_TRUE;
												break;
					
					// MAC_EXIT_SELECT does not appear to be used anywhere.
					case MAC_EXIT_SELECT:		message				= "SYSTEM ERROR";
												restart				= MAC_TRUE;
												break;
					
					// MAC_EXIT_TCP does not appear to be used anywhere.
					case MAC_EXIT_TCP:			message				= "NETWORK ERROR";
												restart				= MAC_TRUE;
												break;

					// DonR 25Aug2005: Treat a signal (such as 15, "soft" program terminate)
					// trapped by the child process the same as we do an untrapped signal
					// (like 9, "hard" program terminate).
					// DonR 21Jul2022: Should we *not* decrement proc_data.retrys for a "soft"
					// program termination (Signal 15)?
					case MAC_EXIT_SIGNAL:		if (proc_found == MAC_OK)
												{
													proc_data.retrys--;
												}
												
												message				= "CHILD WAS KILLED DUE TO TRAPPED SIGNAL";
												restart				= MAC_TRUE;

#if 0
												// DonR 21Jul2022: Is there any real reason we should keep this code? In 21
												// years at Maccabi, I've yet to see anyone do anything with a core-dump
												// file other than delete it!
												if (WCOREDUMP (status))	// SAVE CORE FILE
												{
													sprintf (	buf, "%s/%s.%d.%d.%d_%d:%02d:%02d.core",
																CoreDir,
																(proc_found == MAC_OK) ? proc_data.proc_name : "noname",
																tmptr->tm_year + 1900,
																tmptr->tm_mon  + 1,
																tmptr->tm_mday,
																tmptr->tm_hour,
																tmptr->tm_min,
																tmptr->tm_sec												);

#ifdef _LINUX_
													sprintf (CoreName, "core.%d", proc_data.pid);
#else
													sprintf (CoreName, "core");
#endif
													if (!link (CoreName, buf))
													{
														unlink (CoreName);
														term_len += sprintf (	term_mesg + term_len,
																				"\n\t\t\t\t *** core file SAVED ***");
													}
													else
													{
														GerrLogReturn (	GerrId, "Can't move core file > core dir - %s...error %d/%s", CoreDir, GerrStr);
													}
												}	// Process core dump.
#endif

												break;


					case MAC_CHILD_NOT_STARTED:	// Raise child count instead of child itself -
												// because child count is being decreased anyway.
												// DonR 21Jul2022: I don't understand the previous comment either!
												AddToSonsCount (1);
												message				= "CHILD PROCESS NOT STARTED";
												restart				= MAC_FALS;
												break;
					
					case MAC_SERV_RESET:		// Child process detected an error that made it think that
												// it needs to be restarted.
												message				= "SERVER RESET";
												restart				= MAC_TRUE;

												if (proc_found == MAC_OK)
												{
													proc_data.retrys--;
												}

												break;

				}	// Switch on child-process exit status.
				
				term_len += sprintf (term_mesg + term_len, message);
//GerrLogMini(GerrId, "FatherProcess %d: ending message = %s.", getpid(), message);
			}	// Child process exited with an exit() command.
			
			else if (WIFSTOPPED (status))	// Process was stopped by a signal - do *not* restart it.
			{
				term_len += sprintf (term_mesg + term_len, "CHILD STOPPED BY SIGNAL %d.", WSTOPSIG (status));
				restart = MAC_FALS;
			}
			
			else if (WIFSIGNALED (status))	// Process was killed by a signal (presumably Signal 9).
			{
				if (proc_found == MAC_OK)
				{
					proc_data.retrys--;
				}
				
				term_len += sprintf (term_mesg + term_len, "CHILD WAS KILLED BY SIGNAL %d", WTERMSIG (status));
				
				restart = MAC_TRUE;

				// DonR 21Jul2022: I'm deleting the core-dump code here - it was already disabled, and
				// I can't think of any reason anyone would want to re-enable it, particularly when a
				// child process was deliberately terminated by a "kill" signal. If someone wants to
				// restore it, just copy it from the code above (which is also disabled).
			}

			else
			{
				term_len += sprintf (term_mesg + term_len, "PROCESS TERMINATED - CAUSE UNKNOWN");
			}

			
			/*=======================================================================
			||																		||
			||						RESTART CHILD PROCESS	 						||
			||																		||
			=======================================================================*/
			
			// Determine restart status
			get_sys_status (&stat_data);

			// DonR 21Jul2022: To make the "if" below a little simpler, set a new variable
			// relevant_status equal to the system status relevant to the dead child process.
			relevant_status = (proc_found == MAC_OK) ?	(proc_data.system == PHARM_SYS)		?	stat_data.pharm_status	:
														(proc_data.system == DOCTOR_SYS)	?	stat_data.doctor_status	:
																								stat_data.status
														: 0;
			// Note that the last condition (proc_type != TSX25WORKPROC_TYPE) should be
			// unnecessary, as we stopped running X.25 communications many years ago.
			if (	proc_found				== MAC_OK				&&
					restart					== MAC_TRUE 			&&
					stat_data.status 		== SYSTEM_UP			&&
					relevant_status 		== SYSTEM_UP			&&
					proc_data.proc_type		!= TSX25WORKPROC_TYPE		)
			{
				// If a sufficient amount of time has passed, reset the number of retries
				// for this process. The "interval" value comes from the setup table:
				// program_name = "FatherProcess.exe", parameter_name = "interval".
				// The current value is 3600 = one hour, which seems reasonable enough.
//GerrLogMini (GerrId, "FatherProcess restarting %s - time passed = %d, interval = %d.", proc_data.proc_name, (cur_time - proc_data.start_time), interval);
				if ((cur_time - proc_data.start_time) > interval)
				{
					proc_data.retrys = 0;
				}

				// ProcRetrys comes from the setup table: program_name = "All",
				// parameter_name = "retrys". (Note that it could probably be overridden
				// with a specific value for program_name = "FatherProcess.exe", but
				// there doesn't seem to be any pressing need for that.)
				if (proc_data.retrys < ProcRetrys)
				{
					GerrLogMini (GerrId, "FatherProcess %d re-running %s.", getpid(), proc_data.proc_name);

					state = Run_server (&proc_data);
//GerrLogMini (GerrId, "FatherProcess re-ran %s, state = %d.", proc_data.proc_name, state);
					
					if (state == MAC_OK)
					{
						if ((proc_data.proc_type > 0) && (proc_data.proc_type <= MAX_PROC_TYPE_USED))
						{
							InstanceControl [proc_data.proc_type].instances_running++;
						}
					}
					else
					{
						GerrLogMini (GerrId, "FatherProcess %d tried to re-run %s but got error %d from Run_server().",
									 getpid(), proc_data.proc_name, state);
					}
					
					term_len += sprintf (term_mesg + term_len, "\n\t\t\t*** Re-running child process ***");
				}
				else
				{
					term_len += sprintf (term_mesg + term_len, "\n\t\t\t*** Too many retries - STOPPING ***");
				}
			}	// Process was found and the relevant system is up and running.

			else	// No retry attempt - write to log to explain why.
			{
				term_len += sprintf (	term_mesg + term_len,
										"\n\t*** Not retrying: proc_found = %d, restart = %d, type = %d  ***",
										proc_found, restart, proc_data.proc_type								);
			}

			// This "if" shouldn't really be necessary anymore - since we haven't been
			// using X.25 communications in many years, proc_type is never going to
			// be TSX25WORKPROC_TYPE and thus the "if" will always be TRUE.
			// DonR 18Aug2022: Got rid of NIU variable LOG_X25_CONNECTIONS.
			if (proc_data.proc_type != TSX25WORKPROC_TYPE)
			{
				strcpy (save_log_file, LogFile);
				if (proc_found == MAC_OK)
				{
					strcpy (LogFile, proc_data.log_file);
				}

				// DonR 05Mar2003: Suppress the normal shutdown message for Pharm. TCP Worker -
				// just to make the log smaller.
				// NOTE: This is another "if" that will always be TRUE, since PharmTcpWorker.exe
				// and DocTcpWorker.exe no longer exist.
				if ((log_shutdown_msg == 1) ||
					((strcmp (proc_data.proc_name, "PharmTcpWorker.exe")) &&
					 (strcmp (proc_data.proc_name, "DocTcpWorker.exe"))))
				{
					GerrLogMini (GerrId, "CHILD PROCESS IS GOING DOWN - %s", term_mesg);
				}
				
				strcpy( LogFile, save_log_file );
			}	// Log child process shutdown.
			

			// Having found at least one dead child process, jump to the next iteration
			// of the main "watchdog" loop.
			continue;
		}	// son_pid > 0 - in other words, a child process died.

		
		/*=======================================================================
		||																		||
		||		CHECK FOR INCOMING MESSAGES FROM CHILDREN / PC CONTROLLER		||
		||																		||
		=======================================================================*/
		
		// DonR 03May2022: The delay in the GetSocketMessage() call appears to be the only
		// delaying factor in the whole system-monitoring loop. Currently, SelectWait
		// (which is configured in the setup table) is set to 200000 (= 1/5 second). I don't
		// see any reason that we need to run the whole monitoring loop five times per
		// second; and while FatherProcess isn't eating up a whole lot of CPU time anyway,
		// it would be a bit more efficient to have the loop iterate on a more leisurely
		// schedule. (At the same time, we don't want it to be *too* relaxed - when a son
		// process dies or is brought down, we want the new copy to be restarted without
		// any major delay!) For the moment, let's try 500000 (= 1/2 second) - this should
		// significantly cut the CPU load for FatherProcess, while still restarting dead
		// son processes with an average delay of only around 1/4 second.
//		state = GetSocketMessage (SelectWait, buf, sizeof (buf), &len, CLOSE_SOCKET);
		state = GetSocketMessage (500000, buf, sizeof (buf), &len, CLOSE_SOCKET);

		if ((state == MAC_OK) && (len > 0))	// Message arrived.
		{
			switch ((match = ListNMatch (buf, PcMessages)))
			{
				case -1:			// Unknown message.
									GerrLogMini (GerrId, "FatherProcess got unknown ipc message '%s'", buf);
									break;

				case LOAD_PAR:		// Load parameters from DB to shared memory.
GerrLogMini (GerrId, "FatherProcess got LOAD_PAR message '%s'", buf);
									BREAK_ON_ERR (SqlGetParamsByName (FatherParams, PAR_RELOAD));
									BREAK_ON_ERR (sql_dbparam_into_shm (parm_tablep, stat_tablep, PAR_RELOAD));
									break;

				case STRT_PH_ONLY:	// Start up pharmacy system.
				case STRT_DC_ONLY:	// Start up doctor system.
									get_sys_status (&stat_data);
GerrLogMini (GerrId, "FatherProcess got start-system message '%s'", buf);

									// Start up only when system is currently down.
									if (((match == STRT_PH_ONLY) && (stat_data.pharm_status		!= SYSTEM_DOWN))	||
										((match == STRT_DC_ONLY) && (stat_data.doctor_status	!= SYSTEM_DOWN)))
									{
										// Start-system commands jump to the next iteration of the main
										// watchdog loop, whether the system is already up or not. (This
										// may be a minor bug, but it's probably irrelevant since I don't
										// think we're getting any of these remote-control commands
										// anyway.)
										continue;
									}

									run_system ((match == STRT_PH_ONLY) ? PHARM_SYS : DOCTOR_SYS);

									// Set new system state in shared memory.
									set_sys_status (0, (match == STRT_PH_ONLY) ? GOING_UP : 0, (match == STRT_DC_ONLY) ? GOING_UP : 0);

									// Start-system commands jump to the next iteration of the main
									// watchdog loop.
									continue;
				
				case STDN_PH_ONLY:	// Gracefully shut down pharmacy system.
				case STDN_DC_ONLY:	// Gracefully shut down doctor system.
GerrLogMini (GerrId, "FatherProcess got system shutdown message '%s'", buf);

									// Set new system state in shared memory.
									set_sys_status (0, (match == STDN_PH_ONLY) ? GOING_DOWN : 0, (match == STDN_DC_ONLY) ? GOING_DOWN : 0);

									// For system shutdown commands, we jump to the next iteration of the
									// main watchdog loop.
									continue;
				
				case SHUT_DWN:		// Gracefully shut down the whole system.
GerrLogMini (GerrId, "FatherProcess got full-system shutdown message '%s'", buf);

									// Set new system state in shared memory.
									set_sys_status (GOING_DOWN, 0, 0);
									
									// For system shutdown commands, we jump to the next iteration of the
									// main watchdog loop.
									continue;
				
				case STDN_IMM:		// Immediate full-system shutdown.
GerrLogMini (GerrId, "FatherProcess got immediate system shutdown message '%s'", buf);
				
									// Set new system state in shared memory.
									set_sys_status (SYSTEM_DOWN, 0, 0);
				
									shut_down = MAC_TRUE;
									
									// For system shutdown commands, we jump to the next iteration of the
									// main watchdog loop.
									continue;
			}	// Switch on message received.
		}	// Got a socket message.


		// Manage multiple-instance child programs.

		// First, if the system isn't running, there's no point in
		// monitoring/fixing the number of servers up and running.
		get_sys_status (&stat_data);
		if (stat_data.status != SYSTEM_UP)
		{
			continue;	// To next iteration of main "watchdog" loop.
		}

		// Loop through Program Types looking for programs that require
		// multi-instance control.
		for (ProgramType = 0; ProgramType <= MAX_PROC_TYPE_USED; ProgramType++)
		{
			// Use a shorter pointer variable for more compact code.
			IC = &InstanceControl [ProgramType];

			// Get the status of the program's subsystem.
			relevant_status =	(IC->program_system == PHARM_SYS)	?	stat_data.pharm_status	:
								(IC->program_system == DOCTOR_SYS)	?	stat_data.doctor_status	:
																		stat_data.status;

			// First, find every possible reason not to bother checking
			// a given child program's status.
			if ((IC->program_type		!= ProgramType		)	||	// Array element wasn't initialized from DB.
				(IC->instance_control	== 0				)	||	// Instance control isnt enabled for this program.
				(IC->max_instances		<  2				)	||	// Multiple instances not enabled.
				(IC->min_free_instances	<  1				)	||	// No minimum free instances was set.
				(relevant_status		!= SYSTEM_UP		))
			{
				continue;
			}

			// Find total and busy instances of the program.
			RewindTable (proc_tablep);

			for (running_count = nonbusy_count = 0; ActItems (proc_tablep, 0, GetItem, (OPER_ARGS)&proc_data) == MAC_OK;)
			{
				if (proc_data.proc_type == IC->program_type)
				{
					running_count++;

					if (proc_data.busy_flg == MAC_FALS)
					{
						nonbusy_count++;
					}
				}
			}

			// The total number of running instances should be the same as recorded in
			// the InstanceControl array - but if it isn't, write a message to the log!
			if (running_count != IC->instances_running)
			{
				GerrLogMini (GerrId, "%s: Should have %d instances running, but found %d (%d available).",
							 IC->ProgramName, IC->instances_running, running_count, nonbusy_count);

				// Do we want to correct the number of running instances stored in the array?
			}

			// If the number of available instances is below the minimum *and* the total
			// number running is below the maximum, we need to start some new instances.
			if ((nonbusy_count < IC->min_free_instances) && (running_count < IC->max_instances))
			{
				NewInstancesToOpen = IC->min_free_instances - nonbusy_count;

				if ((running_count + NewInstancesToOpen) > IC->max_instances)
				{
					NewInstancesToOpen = IC->max_instances - running_count;
				}

				// Don't log "Only X free instances" if the program isn't running at all - it
				// just craps up the log file.
				if (IC->instances_running > 0)
				{
					if (nonbusy_count > 0)
					{
						GerrLogMini (GerrId, "\nOnly %d free instance%s of %s (%d required) - starting %d more%s."
											 "\n(%d running, %d busy, maximum instances = %d.)",
											 nonbusy_count, ((nonbusy_count > 1) ? "s" : ""),
											 IC->ProgramName, IC->min_free_instances, NewInstancesToOpen,
											 ((running_count + NewInstancesToOpen) < IC->max_instances) ? "" : " (up to maximum)",
											 running_count, (running_count - nonbusy_count), IC->max_instances);
					}
					else
					{
						GerrLogMini (GerrId, "\nNo free instances of %s (%d required) - starting %d more%s."
											 "\n(%d running, %d busy, maximum instances = %d.)",
											 IC->ProgramName, IC->min_free_instances, NewInstancesToOpen,
											 ((running_count + NewInstancesToOpen) < IC->max_instances) ? "" : " (up to maximum)",
											 running_count, (running_count - nonbusy_count), IC->max_instances);
					}
				}

				strcpy (proc_data.proc_name, IC->ProgramName);

				for (i = 0; i < NewInstancesToOpen; i++)
				{
					proc_data.retrys = 0;

					err = Run_server (&proc_data);

					if (err == MAC_OK)
					{
						IC->instances_running++;
					}
					else
					{
						// If the program we're trying to run doesn't exist at all, there's
						// no point in looping more than once. Also, note that we don't
						// bother logging errors here - Run_server() has sufficient error
						// logging of its own.
						if (err == MAC_PROGRAM_NOT_FOUND)
							break;

						GerrLogMini (GerrId, "FatherProcess %d tried to run a new instance of %s but got error %d from Run_server().",
									 getpid(), proc_data.proc_name, err);
					}
				}	// New-instance loop.
			}	// It's both necessary and possible to start new program instances.
		}	// Loop through Program Types looking for multi-instance programs.
	}	// End of main "watchdog" loop.


	/*=======================================================================
	||																		||
	||						  SHUT DOWN THE SYSTEM							||
	||																		||
	||						KILL ALL RUNNING CHILD PROCESSES	 			||
	=======================================================================*/
	
//	SystemLog = fopen ("/pharm/log/shutdown_log", "a");
	SystemLog = NULL;	// Redundant - disable special "manual" logging.

	// Get count of child processes.
	err = GetSonsCount (&sons);

	if (SystemLog != NULL)
		fprintf (SystemLog, "\n\n\nShutting down at %d - found %d child processes in table (err = %d).\n", GetTime(), sons, err);

	if (sons > 0)
	{
		int		SonCounter	= 0;
		int		KillSignal	= 9;

		// Loop through shared-memory processes table.
		RewindTable (proc_tablep);
		
		while (ActItems (proc_tablep, 0, GetItem, (OPER_ARGS)&proc_data) == MAC_OK)
		{
			if (SystemLog != NULL)
				fprintf (SystemLog, "Child %d Type %d PID %d = %s.\n", SonCounter, proc_data.proc_type, proc_data.pid, proc_data.proc_name);

			SonCounter++;

			// Skip my own entry in the table.
			if (proc_data.proc_type == FATHERPROC_TYPE)
			{
				continue;
			}
		
			// Send terminate signal to child process.
			// DonR 11Apr2022: Send SIGTERM (user-requested shutdown) to those child
			// processes that are prepared to act on it. Otherwise send Signal 9,
			// which is an abrubt, un-trappable "hard kill" signal.
			switch (proc_data.proc_type)
			{
				case SQLPROC_TYPE:			// SqlServer.exe
				case AS400TOUNIX_TYPE:		// As400UnixServer.exe
				case DOCSQLPROC_TYPE:		// DocSqlServer.exe
				case PURCHASE_HIST_TYPE:	// PurchaseHistoryServer

							KillSignal = 15;	// SIGTERM == "soft" kill.
							break;

				default:	KillSignal = 9;		// "Hard" kill.
							break;
			}

			kill (proc_data.pid, KillSignal);
			
			// Remove child process's named pipe.
			if (!access (proc_data.named_pipe, F_OK))
			{
				if (unlink (proc_data.named_pipe))
				{
					GerrLogMini (GerrId, "Can't remove son's named pipe '%s'...error (%d) %s", proc_data.named_pipe, GerrStr);
				}
			}
			
			// Set up diagnostic message text.
			time (&cur_time);
			
			secs   = cur_time - proc_data.start_time;
			
			days   = secs / 86400;
			secs  -= days * 86400;
			
			hours  = secs / 3600;
			secs  -= hours * 3600;
			
			mins   = secs / 60;
			secs  -= mins * 60;
			
			len  = sprintf (buf,		"\n\tSon pid %d (%s)", proc_data.pid, proc_data.proc_name);

			len += sprintf (buf + len,	"\n\tElapsed. . . . . . . :");
			
			if (days	> 0)
				len += sprintf (buf + len,	" %d days", days);

			if (hours	> 0)
				len += sprintf (buf + len,	" %d hours", hours);

			if (mins	> 0)
				len += sprintf (buf + len,	" %d minutes", mins);

			if (secs	> 0)
				len += sprintf (buf + len,	" %d seconds", secs);

			if ((days + hours + mins + secs) == 0)
				len += sprintf (buf + len,	" Nothing( immediate ).");

			len += sprintf (buf + len,	"\n\tMessage. . . . . . . : STOPPED (MACABI SYSTEM shutdown)");

			// GerrLogMini (GerrId, "Child process %d/%s is being shut down.", proc_data.pid, proc_data.proc_name);
		}
	}

	GerrLogMini (GerrId, "FatherProcess: All child processes should be shut down now.");


	/*=======================================================================
	||																		||
	||						 SHUT DOWN ALL OTHER SERVICES					||
	||																		||
	 =======================================================================*/

	// Disconnect from database
	// DonR 15Jan2020: SQLMD_disconnect (which resolves to INF_DISCONNECT) now handles
	// disconnecting from ODBC sources as well as the old Informix data source.
	SQLMD_disconnect ();

	// Close sockets & dispose named pipe file
	DisposeSockets ();

	// Remove shared memory extents
	KillAllExtents ();

	// Delete semaphore
	// DonR 11Apr2022: To avoid errors as other processes shut down, wait a bit
	// (let's try 1 second) before deleting the semaphore.
	usleep (1 * 1000000);
	DeleteSemaphore ();

	// Exit with "OK" status.
	if (SystemLog != NULL)
	{
		fprintf (SystemLog, "FatherProcess.exe terminating at %d.\n", GetTime());
		fclose (SystemLog);
	}

	GerrLogMini (GerrId, "\nFatherProcess: Maccabi system is down!");

	exit	(MAC_OK);
	_exit	(MAC_OK);	// This should be redundant, since the "normal" exit() call should
						// do the job. _exit() is a fallback, since it doesn't perform
						// cleanup like closing files or any atexit/on_exit tasks.

}	// End of main().



/*=======================================================================
||																		||
||					 sql_dbparam_into_shm()								||
||																		||
||				Load parameters from db table into shared memory		||
||				Also take processes to run into buffer					||
||																		||
 =======================================================================*/
int 	sql_dbparam_into_shm (	TABLE	*parm_tablep,	/* Table pointer*/
								TABLE	*stat_tablep,	/* Table pointer*/
								int		reload_flg		/* reload flag	*/	)
{
	PARM_DATA		parm_data;

	static short	first_time = MAC_TRUE;
	int				RowsRead;

	static char		prog_name[256];
	static char		param_name[256];
	static char		param_value[256];
	static short	val_ind;

	// Delete all params shm table if reload
	if (reload_flg == PAR_RELOAD)
	{
		DeleteTable (parm_tablep);
	}

	// LOAD PARAMETERS INTO SHARED MEMORY
	// Declare cursor for retrieving setup table data.
	if (first_time == MAC_TRUE)
	{
		DeclareCursorInto (	MAIN_DB, FP_setup_cur,
							&prog_name,		&param_name,
							&param_value,	END_OF_ARG_LIST		);
	}

	first_time = RowsRead = MAC_FALS;

	for (restart = MAC_TRUE, tries = 0; (tries < SQL_UPDATE_RETRIES) && (restart == MAC_TRUE); tries++)
	{
		restart = MAC_FALS;

		do
		{
			OpenCursor (	MAIN_DB, FP_setup_cur	);

			Conflict_Test_Cur (restart);

			BREAK_ON_ERR (SQLERR_error_test ());

			// Retrieve all table into shared memory
			while (1)
			{
				FetchCursor (	MAIN_DB, FP_setup_cur	);
				val_ind = ColumnOutputLengths [3];

				Conflict_Test (restart);

				// Exit loop on error / end of fetch
				BREAK_ON_TRUE	(SQLERR_code_cmp (SQLERR_end_of_fetch));
				BREAK_ON_ERR	(SQLERR_error_test ());

				RowsRead++;

				// If parameter_value got a NULL value, treat it as a zero-length string.
				// (Note that this *should* be redundant.)
				if (SQLMD_is_null (val_ind))
				{
					param_value[0] = 0;
				}

				// Shared-memory parameter key = program_name.param_name, trimmed as necessary.
				sprintf (parm_data.par_key, "%.*s.%.*s", PROG_KEY_LEN, prog_name, PARAM_NAM_LEN, param_name);

				sprintf (parm_data.par_val, "%.*s", PARAM_VAL_LEN, param_value);
				
				state = AddItem (parm_tablep, (ROW_DATA)&parm_data);
			}	// Loop through setup table.

			CloseCursor (	MAIN_DB, FP_setup_cur	);

			SQLERR_error_test();	// No real point in this - CloseCursor() doesn't throw interesting errors.

//GerrLogMini (GerrId, "FatherProcess loaded setup data, %d rows read.", RowsRead);

		}
		while (0);	// Setup-table dummy loop.

		if (restart == MAC_TRUE)	// Reading setup failed - pause before retry.
		{
			sleep (ACCESS_CONFLICT_SLEEP_TIME);
		}

	}	// Retry loop for setup table.

	if (restart == MAC_TRUE)
	{
		GerrLogMini (GerrId, "sql_dbparam_into_shm: Table 'setup' locked... failed after '%d' retries.", SQL_UPDATE_RETRIES);
		return (INF_ERR);
	}

	// Add UID, username, password, and db to shared memory
	sprintf	(parm_data.par_key, "%s.%s", ALL_PREFIX, "mac_uid");
	strcpy	(parm_data.par_val, MacUid);
	RETURN_ON_ERR (AddItem (parm_tablep, (ROW_DATA)&parm_data));

	sprintf	(parm_data.par_key, "%s.%s", ALL_PREFIX, "mac_user");
	strcpy	(parm_data.par_val, MacUser);
	RETURN_ON_ERR (AddItem (parm_tablep, (ROW_DATA)&parm_data));

	sprintf	(parm_data.par_key, "%s.%s", ALL_PREFIX, "mac_pass");
	strcpy	(parm_data.par_val, MacPass);
	RETURN_ON_ERR (AddItem (parm_tablep, (ROW_DATA)&parm_data));

	sprintf	(parm_data.par_key, "%s.%s", ALL_PREFIX, "mac_db");
	strcpy	(parm_data.par_val, MacDb);
	RETURN_ON_ERR (AddItem (parm_tablep, (ROW_DATA)&parm_data));

	// Increment parameters revision.
	state = ActItems (stat_tablep, 1, IncrementRevision, NULL);
	
	// Return status
	return (state);
}



/*=======================================================================
||																		||
||						SqlGetParamsByName()							||
||																		||
||					Get parameters from database by name				||
||																		||
 =======================================================================*/
int SqlGetParamsByName (	ParamTab	*prog_params,	/* params descr struct	*/
							int			reload_flg		/* is load or reload	*/	)
{
	int					i;
	int					RowsRead;
	char				db_prog_name	[32 + 1];
	char				db_all_name		[32 + 1];
	char				db_param_name	[256];
	char				db_param_val	[256];
	TInstanceControl	IC;

	// Declare cursor for parameters
	strcpy (db_prog_name,	CurrProgName);
	strcpy (db_all_name,	ALL_PREFIX);

	DeclareCursor (	MAIN_DB, FP_params_cur,
					&db_prog_name,	&db_all_name,	END_OF_ARG_LIST	);

	// Zero touched flag for all params
	for (i = 0; prog_params[i].param_val != NULL; i++)
	{
		prog_params[i].touched_flg = MAC_FALS;
	}

	// Get data
	for (restart = MAC_TRUE, tries = RowsRead = 0; tries < SQL_UPDATE_RETRIES && restart == MAC_TRUE; tries++)
	{
		restart = MAC_FALS;

		// Open cursor for parameters
		OpenCursor (	MAIN_DB, FP_params_cur	);

		Conflict_Test_Cur (restart);

		if (restart == MAC_FALS)
		{
			state = SQLERR_error_test ();

			// Fetch params from db
			while (state == MAC_OK)
			{
				FetchCursorInto (	MAIN_DB, FP_params_cur,
									&db_param_name,				&db_param_val,
									&IC.program_type,			&IC.instance_control,
									&IC.startup_instances,		&IC.max_instances,
									&IC.min_free_instances,		END_OF_ARG_LIST			);
				Conflict_Test (restart);

				// Exit loop on error / end of fetch
				if (SQLERR_code_cmp (SQLERR_end_of_fetch) == MAC_TRUE)
					break;

				if (SQLERR_error_test ())
					break;

				RowsRead++;

				// DonR 09Aug2022: If this is a "Program..." parameter and its Program
				// Type value is valid (i.e. from 1 to MAX_PROC_TYPE_USED), store its
				// instance-control parameters.
				if ((IC.program_type >  0)									&&
					(IC.program_type <= MAX_PROC_TYPE_USED)					&&
					(strstr (db_param_name, PROG_PREFIX) == db_param_name))
				{
					strcpy (InstanceControl [IC.program_type].ProgramName, db_param_val);
					InstanceControl [IC.program_type].program_type			= IC.program_type;
					InstanceControl [IC.program_type].instance_control		= IC.instance_control;
					InstanceControl [IC.program_type].startup_instances		= IC.startup_instances;
					InstanceControl [IC.program_type].max_instances			= IC.max_instances;
					InstanceControl [IC.program_type].min_free_instances	= IC.min_free_instances;
															
					// Initialize current instances running *only* on initial load - otherwise
					// preserve the existing value.
					if (reload_flg == PAR_LOAD)
						InstanceControl [IC.program_type].instances_running		= 0;
				}	// The row fetched is a valid program-to-run descriptor.

				// Look in params descr buffer if param exists and update allowed
				for (i = 0; prog_params[i].param_val != NULL; i++)
				{
					if (!strcmp (prog_params[i].param_name, db_param_name))
					{
						if ((reload_flg == PAR_LOAD) || ((reload_flg == PAR_RELOAD) && (prog_params[i].reload_flg == PAR_RELOAD)))
						{
							switch (prog_params[i].param_type)
							{
								case PAR_INT:		*(int *)	prog_params[i].param_val = atoi (db_param_val);
													break;

								case PAR_LONG:		*(long *)	prog_params[i].param_val = atol (db_param_val);
													break;

								case PAR_DOUBLE:	*(double *)	prog_params[i].param_val = atof (db_param_val);
													break;

								case PAR_CHAR:		strcpy (prog_params[i].param_val, db_param_val);
													break;

								default:			GerrLogReturn (GerrId, "Got unknown parameter type from setup table: %d", prog_params[i].param_type);
													break;

							} // switch( prog_params[i].param_type )

						} // if (reload_flg == ... )

						prog_params[i].touched_flg = MAC_TRUE;
						break;	// param was found

					}	// if (!strcmp ... )

				}	// for (i = 0 ... )

			}	// while (state == MAC_OK)

			// Close cursor for parameters resources
			CloseCursor (	MAIN_DB, FP_params_cur	);

		} /* if( restart == ... ) */

		if (restart == MAC_TRUE)	// Reading setup failed - pause before retry.
		{
			sleep (ACCESS_CONFLICT_SLEEP_TIME);
		}

	}	// Retry loop for setup table.

	if (restart == MAC_TRUE)
	{
		GerrLogMini (GerrId, "SqlGetParamsByName: Table 'setup' locked... failed after '%d' retries.", SQL_UPDATE_RETRIES);
		return (INF_ERR);
	}

	//	Check all params values.
//GerrLogMini (GerrId, "FatherProcess %d: checking params values.", getpid());
	for (i = 0; prog_params[i].param_val != NULL; i++)
	{
		if ((reload_flg == PAR_LOAD) || ((reload_flg == PAR_RELOAD) && (prog_params[i].reload_flg == PAR_RELOAD)))
		{
			if (prog_params[i].touched_flg == MAC_FALS)
			{
				GerrLogMini (GerrId, "parameter '%s' not %sloaded", prog_params[i].param_name, (reload_flg == PAR_RELOAD) ? "re" : "");
				state = SQL_PARAM_MISS;
			}
		}
	}

	// Return status.
//GerrLogMini(GerrId, "SqlGetParamsByName	returning %d.", state);
	return (state);
}



/*=======================================================================
||																		||
||		RUN PROCESSES FROM PROCESSES BUFFER OF SPECIFIED SYSTEM 		||
||																		||
 =======================================================================*/
int    run_system (int sys_flgs)
{
	char				data			[512],
						curr_prog		[512];
	PROC_DATA			proc_data;
	PARM_DATA			parm_data;
	int 				i;
	int 				system;
	int 				port_count;
	int 				tcp_server_started; 
	int					err;
	int					CurrProgLen;
	TABLE				*parm_tablep;
	short				RunProgramType;
	TInstanceControl	IC;


	sprintf (curr_prog,		"%s.%s", CurrProgName, PROG_PREFIX);					// "Program"
	CurrProgLen = strlen (curr_prog);

	/*=======================================================================
	||						Open parameters shm table						||
	 =======================================================================*/
	RETURN_ON_ERR (OpenTable (PARM_TABLE, &parm_tablep));

	tcp_server_started = MAC_FALS;

	// Loop on params shared-memory table.
	while (ActItems (parm_tablep, 0, GetItem, (OPER_ARGS)&parm_data) == MAC_OK)
	{
		// 1. Fetch programs only
		if (strncmp (curr_prog, parm_data.par_key, CurrProgLen))
		{
			continue;
		}

		// 2. Fetch only programs of the requested system
		strcpy (proc_data.proc_name, parm_data.par_val);
		GetProgParm (proc_data.proc_name, "system", data);

		system = (strstr (data, "pharm") ? PHARM_SYS : 0) | (strstr (data, "doctor") ? DOCTOR_SYS : 0) | (strstr (data , "doc_tcp_only") ? DOCTOR_SYS_TCP : 0);

		if (!system)
		{
			GerrLogMini (GerrId, "No 'system' parameter for program '%s'", proc_data.proc_name);
			continue;
		}

		// 3. Startup only processes for the requested system
		// Shared processes are started only on initial startup
		if (((system & sys_flgs) != system) || ((system & running_system) == 0))
		{
			continue;
		}

//GerrLogMini (GerrId, "\nFound program \"%s\" in shared memory - searching array for it.", proc_data.proc_name);
		// 4. DonR 10Aug2022: Find the program name in the InstanceControl array.
		// We will use the parameters in this array to control the number of
		// instances for each program we start.
		// NOTE: Having to do a string match like this is kind of clunky - but
		// since I don't want to start changing shared-memory structures if I
		// don't have to, this seems like the best approach to take.
		memset ((char *)&IC, (char)0, sizeof (TInstanceControl));	// Default = all zeroes = single-instance program.

		for (RunProgramType = 0; RunProgramType <= MAX_PROC_TYPE_USED; RunProgramType++)
		{
			if (!strcmp (InstanceControl [RunProgramType].ProgramName, proc_data.proc_name))
			{
				InstanceControl [RunProgramType].program_system = system;
				IC = InstanceControl [RunProgramType];
				break;
			}
		}

		// If this is a single-instance program, or if the startup_intances
		// value wasn't set, force startup_instances to 1.
		if ((!IC.instance_control) || (IC.startup_instances < 1))
		{
			IC.startup_instances = 1;
		}

		GerrLogMini (GerrId, "Starting %d cop%s of %s (Program Type %d); instance control = %d, max instances = %d, min free instances = %d.",
					 IC.startup_instances, ((IC.startup_instances > 1) ? "ies" : "y"), proc_data.proc_name, IC.program_type, IC.instance_control, IC.max_instances, IC.min_free_instances);

		// 5. Start up as many instances of the program as required.
		for (i = 0; i < IC.startup_instances; i++)
		{
			// Initialize anything in the proc_data structure that needs initialization.
			proc_data.retrys		= 0;
			proc_data.eicon_port	= 0;

			// Run the process instance.
			err = Run_server (&proc_data);

			// DonR 11Apr2022: If the program to be run wasn't found at all,
			// don't keep trying to open multiple copies of it - just give up.
			if (err == MAC_PROGRAM_NOT_FOUND)
				break;

			// If we have a valid Process Type (which we always should have),
			// increment the active-instances counter in the InstanceControl array.
			if (err == MAC_OK)
			{
				InstanceControl [RunProgramType].instances_running++;
			}
			else
			{
				// Run_server() (in Memory.c) already logs errors - so we don't
				// need to do anything special here.
			}

		}	// Loop to open one or more instances of child process.
	}	// Loop on params shared-memory table.

//GerrLogMini (GerrId, "FatherProcess %d: run_system() closing parm_tablep and returning.", getpid());
	CloseTable (parm_tablep);

	return MAC_OK;
}



/*=======================================================================
||																		||
||						Find parameter of program						||
||																		||
 =======================================================================*/
int GetProgParm (char *prog_name, char *param_name, char *param_value)
{
	TABLE		*parm_tablep;
	char		curr_prog[512];
	PARM_DATA	parm_data;
	int			CompLen;

	// Open parameters shm table
	param_value[0] = 0;
	RETURN_ON_ERR (OpenTable (PARM_TABLE, &parm_tablep));

	sprintf (curr_prog, "%s.%s", prog_name, param_name);
	CompLen = strlen (curr_prog);

	// Loop on params shm table.
	while (ActItems (parm_tablep, 0, GetItem, (OPER_ARGS)&parm_data) == MAC_OK)
	{
		// Fetch the parameter of program only.
		// DonR 10Aug2022: For performance (and elegance), move strlen() call
		// outside the loop.
		if (strncmp (curr_prog, parm_data.par_key, CompLen))
		{
			continue;
		}

		strcpy (param_value, parm_data.par_val);
		break;	// DonR 10Aug2022: No point in looping once we've got a match.
	}

	// Close parameters shm table
	CloseTable (parm_tablep);
	
	return MAC_OK;
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


	// Reset signal handling for the caught signal.
	sigaction (signo, &sig_act_terminate, NULL);

	// We don't want to copy the signal caught into caught_signal if it was a "deliberate"
	// segmentation fault trapped when we tested ODBC parameter pointers - so I moved
	// these two lines after the pointer-validation block.
	caught_signal	= signo;

	// Produce a friendly and informative message in the SQL Server logfile.
	switch (signo)
	{
		case SIGFPE:
			sig_description = "floating-point error - probably division by zero";
			break;

		case SIGSEGV:
			sig_description = "segmentation error";
			break;

		case SIGTERM:
			sig_description = "user-requested shutdown";
			break;

		default:
			sig_description = "check manual page for SIGNAL";
			break;
	}
	
	GerrLogMini (GerrId,
				 "\n\nFatherProcess got signal %d (%s). Shutting down Maccabi System.", signo, sig_description);
}
