/*=======================================================================
||																		||
||				global.h												||
||																		||
|| NOTE: Run DELCR on this file BEFORE CHECKING IT IN!                  ||
||----------------------------------------------------------------------||
||																		||
|| NOTE:                                                                ||
|| ----                                                                 ||
|| In order to get the source name of a program in error messages		||
||	the user must include the following lines in his source code		||
||																		||
|| #include "global.h"													||
|| static  char *GerrSource = __FILE__;									||
||																		||
||----------------------------------------------------------------------||
||																		||
|| Date : 02/05/96														||
|| Written by : Ely Levy ( reshuma )									||
||																		||
||----------------------------------------------------------------------||
||																		||
|| Date : 19/11/98														||
|| Updated by : Boris Evsikov ( reshuma )								||
||																		||
||----------------------------------------------------------------------||
||																		||
|| Purpose :															||
||	This is an include file for all macabi project sources.				|| 
||																		||
||----------------------------------------------------------------------||
||																		||
|| 			Include files												||
||																		||
 =======================================================================*/

#ifndef	GLOBAL_H
#define GLOBAL_H

#ifdef _LINUX_
	#include "../Include/mac_def.h"
#else
	#include "mac_def.h"
#endif

#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>

#ifndef _LINUX_
	#include <prototypes.h>
#endif

#include <sys/shm.h>
#include <time.h>
#include <sys/types.h>
#include <sys/times.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <malloc.h>
#include <stdarg.h>
#include <signal.h>
#include <netdb.h>

#ifdef _LINUX_
	#include <semaphore.h>
#else
	#include <sys/semaphore.h>
#endif

#include <sys/socket.h>
#include <sys/select.h>

#ifndef _LINUX_
	#include <sys/itimer.h>
	#include <sys/netinet/in.h>
#endif

#include <sys/un.h>

#ifdef _LINUX_
	#include <sys/mman.h>
#else
	#include <sys/lock.h>
#endif

#include <netinet/in.h>
#include <arpa/inet.h>

/*=======================================================================
||									||
||			Global debug levels				||
||									||
 =======================================================================*/

/*
 * Put message id on trace file
 */
//#define MESSAGE_DEBUG

/*=======================================================================
||									||
||			Global macros for project			||
||									||
 =======================================================================*/

#define	NOT_FOUND		-1		/* itm not found in list*/

#define	OK_MESS			"0"		/* ok message to pc	*/

#define	TOO_MANY_CONN		"-1"		/* too many connections	*/

#define	SHAKE_ERR		"-2"		/* handshaking error	*/

#define	UNKNOWN_MESS		"-3"		/* unknown message of pc*/

#define	INTERNAL_ERR		"-4"		/* internal error	*/

#define	SYS_GOING_DOWN		"-5"		/* system is going down	*/

#define	LOG_FILE_KEY 		"log_file"	/* log file key in setup*/

#define	PARM_TABLE		"parm_table"	/* params shared mem tab*/

#define	PROC_TABLE		"proc_table"	/* procs  shared mem tab*/

#define	UPDT_TABLE		"updt_table"	/* update shared mem tab*/

#define	STAT_TABLE		"stat_table"	/* statis shared mem tab*/

#define	TSTT_TABLE		"tstt_table"	/* time s shared mem tab*/

#define	DSTT_TABLE		"dstt_table"	/* time s shared mem tab*/

#define	PRSC_TABLE		"prsc_table"	/* prescription shmm tab*/

#define	MSG_RECID_TABLE		"msg_recid_table"		/* message rec_id shmm tab*/

#define	DIPR_TABLE		"dipr_table"	/* died processes shm tb*/

#define	PROG_PREFIX		"Program"	/* Programs prefix in db*/

#define	ALL_PREFIX		"All"		/* All Programs prefix	*/

#define	NAMEDP_PREFIX		"_namedp_"	/* Named pipes prefix	*/

#define	SQL_POSTFIX		"Sql"		/* Program postfix in db*/

#define	DOC_SQL_POSTFIX		"DocSql"	/* Program postfix in db*/

#define	TSS_POSTFIX		"Tss"		/* Program postfix in db*/

#define	ARG_POSTFIX		"Arg"		/* param  postfix in db	*/

#define	NOT_CONTROL		1		/* All procs but control*/

#define	NO_RETRYS		1		/* No retrys flag	*/

#define	ALL_PROCS		2		/* All processes	*/

#define	JUST_CONTROL		3		/* Just control process	*/

#define FIRST_SQL		4		/* first sql only	*/

#define	GOING_UP		10		/* System is going up	*/

#define	SYSTEM_UP		20		/* System is up		*/

#define	GOING_DOWN		30		/* System is going down	*/

#define	SYSTEM_DOWN		40		/* System is down	*/

#define DAY_INTERVAL		0		/* day interval		*/

#define HOUR_INTERVAL		1		/* hour interval	*/

#define MINUTE_INTERVAL		2		/* minute interval	*/

#define SECOND_INTERVAL		3		/* second interval	*/

#define	DAY_ENTRY		10		/* day entry in times	*/

#define	HOUR_ENTRY		20		/* hour entry in times	*/

#define	MINUTE_ENTRY		30		/* minute entry in times*/

#define	SECOND_ENTRY		40		/* second entry in times*/

#define	MSGNUM_ENTRY		0		/* message number entry	*/
#define	MSGCNT_ENTRY		1		/* message count entry	*/
#define	MSGWAIT_ENTRY		2		/* message wait entry	*/
#define	MSGWORK_ENTRY		3		/* message work entry	*/

#define	ALL_MSGS		-1		/* total for all messags*/
#define	LOAD_PAR		 0		/* Load params db > shm	*/
#define	SHUT_DWN		 1		/* Shutdown all system	*/
#define	STDN_IMM		 2		/* Shutdown immediately	*/
#define	PROC_STT		 3		/* pharm process table	*/
#define	SYST_STT		 4		/* Is system up		*/
#define	PROC_MSG		 5		/* Process Message	*/
#define	MSGS_STT		 6		/* message statistics	*/
#define	TIME_STT		 7		/* time  statistics	*/
#define	LAST_MES		 8		/* last messages	*/
#define	DOC_PROC_STT	 9		/* doctor process table	*/
#define	STDN_PH_ONLY	10		/* shutdown pharm system*/
#define	STDN_DC_ONLY	11		/* shutdown doctors syst*/
#define	STRT_PH_ONLY	12		/* start pharm system  	*/
#define	STRT_DC_ONLY	13		/* start doctors system	*/
#define	DOC_MSGS_STT	14		/* doctor messsage stat	*/
#define	DOC_TIME_STT	15		/* doctor time statistic*/
#define	DOC_LAST_MES	16		/* doctor last messages	*/

#define	FATHERPROC_TYPE			 1		/* Father process type	*/
#define	SQLPROC_TYPE			 2		/* Sql server process ty*/
#define	CONTROLPROC_TYPE		 3		/* Monitor process type	*/
#define	UNIXTOAS400_TYPE		 4		/* Communication pr type*/
#define	AS400TOUNIX_TYPE		 5		/* As/400 process type	*/
#define	TSX25SRVPROC_TYPE		 6		/* Tswitch X25 Listener */
#define	TSX25WORKPROC_TYPE		 7		/* Tswitch X25 Worker   */
#define	SHMREFRESHPROC_TYPE		 8		/* shm ref process type	*/
#define	AS400MONTHLY_TYPE		 9		/* As/400 process type	*/
#define	DOCUNIXTOAS400_TYPE		10		/* Communication pr type*/
#define	DOCAS400TOUNIX_TYPE		11		/* As/400 process type	*/
#define	DOCSQLPROC_TYPE			12		/* Sql server process ty*/
#define	TSTCPIPSRVPROC_TYPE		13		// DocTCPServer
#define	WEBSERVERPROC_TYPE		14		/* Web Server process type */
#define PURCHASE_HIST_TYPE		15		/* Member Purchase History Service process type */
#define	DOC2UNIXTOAS400_TYPE	16		// As400UnixDoc2Client - this #define is not (yet?) in the actual
										// program source. The program was last compiled in 2014 and will
										// be thrown out with the rest of the old Doctors system, so I don't
										// think there's any point in making the change.
#define	DOC2AS400TOUNIX_TYPE	17		// As400UnixDoc2Server (NEW - this program was previously using the
										// same value as its "sister" program As400UnixDocServer, and this
										// led to problems in restarting after an error.
#define PHARM_TCP_SERVER_TYPE	18		// PharmTcpServer (NEW - this program was previously using the same
										// value as DocTcpServer.
#define MAX_PROC_TYPE_USED		18		// Increment as necessary!

#define NO_VERBOSE		1		/* don't show (recv)clos*/
#define VERBOSE			2		/* show (recv)close conn*/

#define	BUFFER_SIZE		128000		/* Global buffer size	*/

#define	GerrStr	(errno), (strerror(errno))	/* Global params for err*/

#define	GerrErr	" error %d (%s)"		/* Global params for err*/

#define GerrId        (GerrSource), (__LINE__)	/* Global error id	*/

#ifdef	AIX_OP_SYS	//
#define WCOREDUMP(status)	1
#endif //

#define MAC_PROCESS_RUNING	1	/* child process started	*/

#define MAC_TRUE		1

#define MAC_FALS		0
#define MAC_FALSE		0

#define	FILE_MESG		1

#define	DATA_MESG		2

#define	NOT_SORTED		1

#define	SORT_REGULAR		2

#define	SORT_BTREE		3

#define	REGULAR_MSG		1

#define	SPOOL_MSG		2

#define	OPER_FORWARD		0

#define	OPER_STOP		1

#define	OPER_BACKWARD		2

#define MAC_SERV_SHUT_DOWN		 0	/* server stopped normally	*/
#define MAC_EXIT_NO_MEM			 1	/* alloc error			*/
#define MAC_EXIT_SQL_ERR		 3	/* sql error			*/
#define MAC_EXIT_SQL_CONNECT	 4	/* error at connect to SQL	*/
#define MAC_EXIT_SELECT			 5	/* select() system call error	*/
#define MAC_EXIT_TCP			 6	/* Start TCP/IP error		*/
#define MAC_SYST_SHUT_DOWN		 7	/* server stopped sys go down	*/
#define MAC_EXIT_SIGNAL			 8	/* Server stopped due to trapped signal */
#define MAC_CHILD_NOT_STARTED	20	/* child process not started	*/
#define MAC_SERV_RESET			21	/* server stopped for reset	*/
#define MAC_PROGRAM_NOT_FOUND	22	// Program couldn't be found, so don't even try to start it!

// Transaction 1055 dynamic variable types.
// DonR 11Mar2024 Bug #297605: Added PAR_SHORT and PAR_NON_ZERO_SHORT so we
// can read SMALLINT values from the database into short integers.
#define PAR_INT				1	/* parameter is integer		*/
#define PAR_ASC				2   // parameter is a string.
#define PAR_DAT				3   // date parameter - always an integer.
#define PAR_IS_NON_ZERO		4	// DonR 04Jan2017. This lets us enable or disable a Trn. 1055 output
								// line depending on an underlying DB variable having a non-zero value.
#define PAR_SHORT			5	// DonR 11Mar2024: Added so that we can keep database SMALLINTs as short integers.
#define PAR_NON_ZERO_SHORT	6	// DonR 11Mar2024: Added short version of PAR_IS_NON_ZERO.

#define PAR_LOAD		1	/* load parameter in start only */
#define PAR_RELOAD		2	/* parameter is reloadable	*/

#define PAR_LONG		2	// parameter is long. NOT used in Transaction 1055!
#define PAR_CHAR		3	// parameter is string. NOT used in Transaction 1055!
#define PAR_DOUBLE		4	// parameter is double. NOT used in Transaction 1055!

#define LEAVE_OPEN_SOCK		1	/* leave socket open after retrn*/
#define CLOSE_SOCKET		2	/* close socket  before return	*/

#define PHARM_SYS			0x01	/* pharmacy system code		*/
#define DOCTOR_SYS			0x02	/* doctors system code		*/
#define DOCTOR_SYS_TCP		0x04	/* doctors system code	for Tcp/Ip only	*/

#define	ANSWER_IN_FILE		1
#define	ANSWER_IN_BUFFER	2


#ifdef	MAIN //

#define DEF_TYPE
#define DEF_INIT( a )		=\
				a

#else //

#define DEF_TYPE		extern
#define DEF_INIT( a )

#endif //	/* MAIN */

/*
 **
 *** Common Used Exit Macros
 *** -----------------------
 **
 */

#define	ERR_STATE( a )		\
    ( a < MAC_OK )

#define	ABORT_ON_ERR( a )	\
    ret = (a) ;			\
    if( ret < MAC_OK )		\
    {				\
	exit( -1 );		\
    }

#define	ABORT_ON_NULL( a )	\
    ptr = (char*)(a) ;		\
    if( ptr == NULL )		\
    {				\
	exit( -1 );		\
    }

#define	RETURN_ON_ERR( a )	\
    ret = a ;			\
    if( ret < MAC_OK )		\
    {				\
	printf("err ( %d ) in pid ( %d ).\n",\
	       ret,getpid());	\
	       fflush(stdout);	\
	return( ret );		\
    }

#define	BREAK_ON_ERR( a )	\
    ret = a ;			\
    if( ret < MAC_OK )		\
    {				\
	break;			\
    }

#define	BREAK_ON_TRUE( a )	\
    ret = a ;			\
    if( ret == MAC_TRUE )	\
    {				\
	break;			\
    }

#define	CONT_ON_ERR( a )	\
    ret = a ;			\
    if( ret < MAC_OK )		\
    {				\
	continue;		\
    }

#define	RETURN_ON_NULL( a )	\
    ptr = (char*)a ;		\
    if( ptr == NULL )		\
    {				\
	return( MEM_ALLOC_ERR );\
    }

#define	MAX_SOCK( a, b )	\
    ((a) > (b)) ? (a) : (b)

#ifdef _LINUX_
	#define MIN( x , y  ) \
		( ( ( x ) < ( y ) ) ?  ( x ) : ( y ) )
#endif

/*
 *  Is string empty ?
 *  -----------------
 */
#define NOT_EMPTY_STRING( s )  \
    strlen( (s) + strspn( (s), " \t" ) )

/*
 **
 *** Statistics arrays length
 *** ------------------------
 **
 */

#define TOTAL_MSGS	32			/* DonR 04Mar2003 */

#define TOTAL_INTERVALS	4	

#define TOTAL_ENTRIES	25

#define TOTAL_TIMES	4

#define TOTAL_LAST	20

//#define TOTAL_DOC_MSGS		52	/* DonR 04Mar2003 */
//#define TOTAL_DOC_MSGS		60	/* Yulia 20030518 (41,70,72,74) * 2 */
//#define TOTAL_DOC_MSGS		62	/* Yulia real number */
//#define TOTAL_DOC_MSGS		62	/* Yulia real number */
//#define TOTAL_DOC_MSGS		70	/* Yulia real number 20030708*/
#define TOTAL_DOC_MSGS		70	// DonR 07Apr2013: Added 53.

#define TOTAL_DOC_INTERVALS	4	

#define TOTAL_DOC_ENTRIES	25

#define TOTAL_DOC_TIMES		4

#define TOTAL_DOC_LAST		20

/*=======================================================================
||									||
||			Global types for project			||
||									||
 =======================================================================*/

typedef	key_t	semaphore_key_type;		/* semaphore key type	*/

typedef	sem_t	semaphore_type;			/* semaphore type	*/

typedef	key_t	sharedmem_key_type;		/* shared mem key type	*/

typedef	size_t	size_type;			/* size type		*/

typedef	union univ_addr
{
    struct sockaddr_in	internet_addr;
    struct sockaddr_un	unix_addr;
} UNIV_ADDR;

/*
 * Message header format ( internet domain )
 * -----------------------------------------
 */
typedef struct
{
    char		messageType[8];	/* numeric ascii, null terminate*/
    char		messageLen[8];	/* numeric ascii, null terminate*/
    char		flags[16];	/* 16 1-byte ascii flags	*/
} MESSAGE_HEADER;

/*
 * Parameters description buffer ( load from database )
 * ----------------------------------------------------
 */
typedef struct				/* --< PARAMETER DESC TABLE >-- */
{
    void		*param_val;	/* parameter value address	*/
    char		*param_name;	/* parameter name in database	*/
    int			param_type,	/* parameter data storage type	*/
    			reload_flg,	/* parameter is reloadable ?	*/
   			touched_flg;	/* flag if param value changed	*/
} ParamTab;

/*
 * Parameters description buffer ( load from environment )
 * -------------------------------------------------------
 */
typedef struct				/* --< PARAMETER DESC TABLE >-- */
{
    char		*param_value,	/* parameter environment name 	*/
    			*param_name;	/* parameter value address	*/
} EnvTab;


/*
 **
 ***
 *** SHARED MEMORY DEFINITIONS
 *** =========================
 ***
 **
 */

/*
 * Data in row of parameters table
 * -------------------------------
 */
typedef struct parm_data		/*<params table row data descr> */
{
    char		par_key[64],	/* parameter key		*/
			par_val[32];	/* parameter value		*/
} PARM_DATA;

/*
 * Data in row of table-to-update table
 * ------------------------------------
 */
typedef struct updt_data		/*<tables-to-update table row > */
{
    char		table_name[64];	/* table name to update		*/
    int			update_date;
    int			update_time;
} UPDT_DATA;


// DonR 04Aug2022: New structure for controlling the number of instances
// of a "child" process - replacing the hard-coded logic in FatherProcess
// for SqlServer and DocSqlServer with generic logic so that new "micro-
// service" servers can be easily added to the system. I would have made
// the structure variables short integers, but since we're overlaying this
// structure onto a 4-byte integer (eicon_port - see the definition of
// PROC_DATA below) it's better to keep the structure length at 4 bytes.
typedef struct instance_control_data
{
	char	ProgramName	[33];		// From setup/parameter_value.
	short	program_type;			// See the program-type #define's above.
	short	program_system;			// Pharmacy / Doctor / Doctor TCP bitmapped.
	short	instance_control;		// 0 = FALSE, 1 = TRUE.
	short	startup_instances;		// Minimum instances at system startup.
	short	max_instances;			// Maximum instances that can be running.
	short	min_free_instances;		// Minimum available instances to maintain.
	short	instances_running;		// The current number of instances running.
} TInstanceControl;

// Data in row of shared-memory processes table
typedef struct proc_data		/* < proc table row data descr> */
{
    char		proc_name[32],	/* process name			*/
				log_file[80],	/* log file of process		*/
				named_pipe[80];	/* process named pipe to connect*/
    short int	proc_type,	/* process type			*/
				busy_flg;	/* is process busy ?		*/
    pid_t		pid,		/* process system id		*/
				father_pid;	/* father process id		*/
    time_t		start_time;	/* process start time		*/
    short int	s_year,		/* process start year		*/
				s_mon,		/* process start month		*/
				s_day,		/* process start day		*/
				s_hour,		/* process start hour		*/
				s_min,		/* process start minute		*/
				s_sec;		/* process start second		*/
    short int	l_year,		/* process last access year	*/
				l_mon,		/* process last access month	*/
				l_day,		/* process last access day	*/
				l_hour,		/* process last access hour	*/
				l_min,		/* process last access minute	*/
				l_sec;		/* process last access second	*/
    int			retrys;		/* Number of retrys		*/
    int			eicon_port;	// Eicon port for X25 srvr/worker (should be NIU as of 2022).
    short int	system;		/* or of pharmacy = 0x01 ,	*/
							/* doctors = 0x02		*/
} PROC_DATA;


// Child process status & pid
typedef struct
{
    int	status,
	pid;
} DIPR_DATA;

// Data in row of processes table
typedef struct sqlp_data		/* < proc table row data descr> */
{
    short int		proc_busy;	/* process is busy ?		*/
    char		named_pipe[64];	/*process named pipe to connect	*/
} SQLP_DATA;

// Data in row of status table
typedef struct stat_data		/* < stat table row data descr> */
{
    short int		status;		/* system status		*/
    int			pharm_status;	/* pharmacy system status	*/
    int			doctor_status;	/* doctor system status		*/
    int			param_rev;	/* parameters revision		*/
    int			tsw_flnum;	/* tswitch temporary file numbrs*/
    int			sons_count;	/* father process sons count	*/
} STAT_DATA;

// Some statistic message data
typedef struct ssmd_data		/* < ssmd table row data descr> */
{
	short			terminal_num;
	short			spool_flg;
	short			ok_flg;
	short			member_id_ext;
	char			start_time[12];	/* null terminated time string	*/
	struct timeval	proc_time;	/* 2 long's in struct trimeval	*/
	struct timeval	start_proc;	/* 2 long's in struct trimeval	*/
	int				pharmacy_num;
	int				member_id;
	short			msg_idx;
	short			error_code;
} SSMD_DATA;

// Data in row of time statistics table
typedef struct tstt_data		/* < tstt table row data descr> */
{
    int			msg_count	/* message counters		*/
			[TOTAL_MSGS]	/* for all message types	*/
			[TOTAL_INTERVALS]/* time intervals		*/
			[TOTAL_ENTRIES];/* last entries			*/
    int			time_count	/* time counters		*/
			[TOTAL_MSGS]	/* all message types		*/
			[TOTAL_TIMES];	/* for all time samples		*/
    time_t		last_time
			[TOTAL_MSGS]	/* for all message types	*/
			[TOTAL_INTERVALS];/* last update times		*/
    short int		last_time_mod
			[TOTAL_MSGS]	/* for all message types	*/
			[TOTAL_INTERVALS];/* last times mod entries	*/
    SSMD_DATA		last_mesg
			[TOTAL_LAST];	/* last messages data		*/
    int			total_msg_count;/* total messages count		*/
    time_t		start_time;	/* start statistics time	*/
} TSTT_DATA;

/*
 * Some doctors statistic message data
 * -----------------------------------
 */
typedef struct dsmd_data		/* < dsmd table row data descr> */
{
    int			terminal_id;
    short int		spool_flg;
    short int		ok_flg;
    int			member_id;
    short int		member_id_ext;
    short int		msg_idx;
    int			contractor;
    char		start_time[12];	/* null terminated time string	*/
    struct timeval	proc_time;	/* 2 long's in struct trimeval	*/
    struct timeval	start_proc;	/* 2 long's in struct trimeval	*/
    int			doctor_id;
    short int		error_code;
    short int		contract_type;

} DSMD_DATA;

/*
 * Data in row of doctors time statistics table
 * --------------------------------------------
 */
typedef struct dstt_data		/* < dstt table row data descr> */
{
    int			msg_count	/* message counters		*/
			[TOTAL_DOC_MSGS]/* for all message types	*/
			[TOTAL_DOC_INTERVALS]/* time intervals		*/
			[TOTAL_DOC_ENTRIES];/* last entries		*/
    int			time_count	/* time counters		*/
			[TOTAL_DOC_MSGS]/* all message types		*/
			[TOTAL_DOC_TIMES];/* for all time samples	*/
    int			last_time
			[TOTAL_DOC_MSGS]/* for all message types	*/
			[TOTAL_DOC_INTERVALS];/* last update times	*/
    short int		last_time_mod
			[TOTAL_DOC_MSGS]/* for all message types	*/
			[TOTAL_DOC_INTERVALS];/* last times mod entries	*/
    DSMD_DATA		last_mesg
			[TOTAL_DOC_LAST];/* last messages data		*/
    int			total_msg_count;/* total messages count		*/
    time_t		start_time;	/* start statistics time	*/
} DSTT_DATA;

/*
 * Data in any row of table
 * ------------------------
 */
typedef char *ROW_DATA;			/* data in row table		*/

/*
 * Compare function type
 * ---------------------
 */
typedef
    int	(*ROW_COMP)(
		    const void *,
		    const void *
		    );

typedef
    int	(*REF_FUNC)(
		    int
		    );

/*
 * Data of table
 * -------------
 */
typedef struct table_data		/* -< table data header descr > */
{
    int			record_size;	/* table record  size		*/
    int			data_size;	/* table data  size		*/
    short int		sort_flg;	/* table is sorted flag		*/
    REF_FUNC		refresh_func;	/* refresh function		*/
    ROW_COMP		row_comp;	/* table record compare function*/
    char		table_name[32];	/* table name			*/
    char		updating_tables[320];/* table names to update	*/
} TABLE_DATA;

/*
 * Pointer inside an extent
 * ------------------------
 */
typedef struct extent_ptr
{
    int			ext_no;		/* extent no			*/
    off_t		offset;		/* offset inside an extent	*/
} EXTENT_PTR;

/*
 * Table row
 * ---------
 */
typedef struct table_row		/* --< table row data descr >-- */
{
    EXTENT_PTR		back_row,	/* back row in table		*/
			next_row;	/* next row in table		*/
    EXTENT_PTR		row_data;	/* table row data	 	*/
} TABLE_ROW;

// Header of table
typedef struct table_header		/* --<  table header descr  >-- */
{
    TABLE_DATA		table_data;	/* table data ( previous struct)*/
    int			record_count;	/* total record count for table	*/ 
    int			free_count;	/* total free record countC	*/
    EXTENT_PTR		back_table,	/* back table header		*/
			next_table;	/* next table header		*/
    EXTENT_PTR		first_row,	/* first row in table		*/
			last_row,	/* last row in table		*/
			first_free,	/* first row in free rows list	*/
			last_free;	/* last row in free rows list	*/
} TABLE_HEADER;

// Tables connection buffer
typedef struct	table			/* ----< Connection struct >--- */
{
    short	int	conn_id,	/* connection serial number	*/
			busy_flg;	/* busy flag			*/
    struct table	*curr_addr;	/* Validation field		*/
    EXTENT_PTR		table_header;	/* Table header			*/
    EXTENT_PTR		curr_row;	/* Current row in table		*/
} TABLE;

// Header of shared memory extent
typedef struct extent_header		/* --< extent header descr  >-- */
{
    int			free_space,	/* free space in shared extent	*/
			extent_size,	/* extent size in bytes		*/
			next_extent_id;	/* next extent id		*/
    off_t		free_addr_off;	/* start address of free space	*/
    EXTENT_PTR		next_extent,	/* next extent header		*/
			back_extent;	/* back extent header		*/
} EXTENT_HEADER;

// Master header of shared memory extents
typedef struct master_shm_header	/* --< extent header descr  >-- */
{
    int			extent_count;	/* global extent count		*/
    EXTENT_PTR		first_table,	/* first table header		*/
			last_table;	/* last table header		*/
    EXTENT_PTR		first_extent,	/* first extent header		*/
			free_extent;	/* first free extent header	*/
} MASTER_SHM_HEADER;

typedef char	*OPER_ARGS;		/* arguments for row operation	*/

typedef int	(*OPERATION)(
			     TABLE*,
			     OPER_ARGS
			     );		/* operation on rows of tables	*/


// Structure describes key value for searching in shared memory table
typedef struct __finditemparams
{
    void   *pKeyLower;	// Lower value of the key
    void   *pKeyUpper;  // Upper value of the key
    short  sStartKey;   // Start position of the key in record
    short  sKeyLength;  // Key length    
} FINDITEMPARAMS, *PFINDITEMPARAMS;


/*
 **
 ***
 *** CONTROL PROCESS DEFINITIONS
 *** =========================
 ***
 **
 */ 
typedef struct __repstate
{
    char cMyNameAndLine[32];
    short sMyState;
    short sMyType;
    char cHisNameAndLine[32];
    short sHisState;
    short sHisType;
} REPSTATE, *PREPSTATE;


/*=======================================================================
||									||
||			Global variables for project			||
||									||
 =======================================================================*/

static	char
	buf[BUFFER_SIZE],		/* Global buffer for general use*/
	*ptr;				/* Global pointer for gen use	*/

static	int
	len,				/* Global length for general use*/
	state,				/* state - to know if continue	*/
	retrys = NO_RETRYS,		/* retrys after fail		*/
	eicon_port = 0,			/* eicon port for tsservers	*/
	ret;				/* Global return code variable	*/

DEF_TYPE char
	MacUid[512],			/* macabi database userid	*/
	MacUser[512],			/* macabi database username	*/
	MacPass[512],			/* macabi database password	*/
	MacDb[512],				/* macabi database name		*/
	CurrProgName[256]		/* Current program file name	*/
	DEF_INIT( "" )
	,
	CurrNamedPipe[256]		/* Current program named pipe 	*/
	DEF_INIT( "" )
	,
	BinDir[256],			/* Location of executetbles	*/
	CoreDir[256]			/* Where to move core files	*/
	DEF_INIT( "" )
	,
	NamedpDir[256]			/* Where to put named pipes	*/
	DEF_INIT( "" )
	,
	LogDir[256]			/* log file directory		*/
	DEF_INIT( "" )
	,
	MsgDir[256]			/* msg file directory		*/
	DEF_INIT( "" )
	,
	LogFile[256]			/* log file name		*/
	DEF_INIT( "" )
	;

DEF_TYPE int
	NamedPipeSocket			/* Named pipe socket of process	*/
	DEF_INIT( -1 )
	,
	SharedMemUpdateInterval		/* interval between shm updates	*/
	DEF_INIT( 10 )
	,
	GoDownCheckInterval		/* interval between godown check*/
	DEF_INIT( 10 )
	,
	interval,			/* Time interval for retrys	*/
	ReadTimeout			/* Timeout for read in seconds	*/
	DEF_INIT( 10 )
	,
	WriteTimeout			/* Timeout for write in seconds	*/
	DEF_INIT( 10 )
	,
	SelectWait,			/* Wait in select(micro-seconds)*/
	FATAL_ERROR_LIMIT		/* least severity for errors	*/
	DEF_INIT(5),
	DEFAULT_SEVERITY		/* default severity when not fnd*/
	DEF_INIT(10),
	SQL_UPDATE_RETRIES		/* sql access conflict retrys	*/
	DEF_INIT(10),
	SHM_UPDATE_INTERVAL		/* shared memory update interval*/
	DEF_INIT(10),
	SPCL_PRESC_PRE_SALE_DAYS	/* special prescs per sale days	*/
	DEF_INIT(10),
	ACCESS_CONFLICT_SLEEP_TIME	/* access conflict retries wait */
	DEF_INIT(1),
	LockPagesMode			/* Lock pgs in primary mem mode	*/
	DEF_INIT(1),
	ProcRetrys,			/* Num of retries after	fail	*/
	MacTcpPort			/* Tcp/Ip port for listen	*/
	DEF_INIT(9996),		
	LogCommErr			/* Log communication errors mode*/
	DEF_INIT(1),		
	LogMesgErr			/* Log message errors mode	*/
	DEF_INIT(1),		
	X25TimeOut			/* x25 timeout in seconds	*/
	DEF_INIT(1),
	SYSTEM_MAX_LOAD			/* system max load for work	*/
	DEF_INIT(40),
	SLEEP_BETWEEN_TABLES		/* sleep between tables to as400*/
	DEF_INIT(60),
	TRANSFER_INTERVAL		/* interval not to update recs	*/
	DEF_INIT(1800),
	tries,
	restart;

DEF_TYPE size_type
	SharedSize;			/* Shared memory size		*/

/*=======================================================================
||									||
||		Global functions definition for project			||
||									||
 =======================================================================*/

/* EPH: Allow use by C++ */
#ifdef __cplusplus //
extern "C" {
#endif //

/*
 **
 *** S E M A P H O R E S
 *** -------------------
 **
 */
					/* Creat semaphore		*/
int	CreateSemaphore(
			void
			);

					/* Delete semaphore		*/
int	DeleteSemaphore(
			void
			);

					/* Open semaphore		*/
int	OpenSemaphore(
		      void
		      );

					/* Close semaphore		*/
int	CloseSemaphore(
		       void
		       );

					/* Wait on semaphore		*/
int	WaitForResource(
			void
			);

					/* Release semaphore		*/
int	ReleaseResource(
			void
			);

/*
 **
 *** E R R O R   H A N D L I N G
 *** ---------------------------
 **
 */

void 	GerrLogExit(
		    char	*source,/* Source name from  (GERR_ID)  */
		    int		line,	/* Line in source from  -"-     */
		    int		status,	/* Exit status for exit()       */
		    char	*printf_fmt,/* Format for printf        */
		    ...			/* arguments for printf         */
		    );

void 	GerrLogReturn(
		      char	*source,/* Source name from  (GERR_ID)  */
		      int	line,	/* Line in source from  -"-     */
		      char	*printf_fmt,/* Format for printf        */
		      ...		/* arguments for printf         */
		      );

void 	GerrLogMini(
		      char	*source,/* Source name from  (GERR_ID)  */
		      int	line,	/* Line in source from  -"-     */
		      char	*printf_fmt,/* Format for printf        */
		      ...		/* arguments for printf         */
		      );

void	GerrLogToFileName	(	char *FileName,
								char *source,		/* Source name from  (GERR_ID)	*/
								int	 line,			/* Line in source from  -"-	*/
								char *printf_fmt,	/* Format for printf		*/
								...					/* arguments for printf		*/
							);

void	GerrLogFnameMini	(	char *FileName,
								char *source,		/* Source name from  (GERR_ID)	*/
								int	 line,			/* Line in source from  -"-	*/
								char *printf_fmt,	/* Format for printf		*/
								...					/* arguments for printf		*/
							);

void GerrLogAddBlankLines ( int NumLines_in );

void 	GmsgLogExit(
		    char	*source,/* Source name from  (GERR_ID)  */
		    int		line,	/* Line in source from  -"-     */
		    int		status,	/* Exit status for exit()       */
		    char	*title,	/* Title for message		*/
		    char	*printf_fmt,/* Format for printf        */
		    ...			/* arguments for printf         */
		    );

void 	GmsgLogReturn(
		      char	*source,/* Source name from  (GERR_ID)  */
		      int	line,	/* Line in source from  -"-     */
		      char	*title,	/* Title for message		*/
		      char	*printf_fmt,/* Format for printf        */
		      ...		/* arguments for printf         */
		      );

static	FILE	*open_log_file(
			       void
			       );	/* open log file for append	*/

FILE	*open_other_file (char *fname, char *OpenMode);	// Open specified file for specified mode.

void	close_log_file(
			       FILE *fp
			       );	/* close log file.		*/

/*
 **
 *** S H A R E D  M E M O R Y
 *** ------------------------
 **
 */

					/* allocate & init 1st sherd mem*/
int	InitFirstExtent(
			size_type	shm_size
			);

					/* allocate new extent , init	*/
					/* and insert into extent chain	*/
static int	AddNextExtent(
			      size_type	shm_size
			      );

					/* kill all alocated shared	*/
					/* extents			*/
int	KillAllExtents(
		       void
		       );

					/* open first shared extent	*/
int	OpenFirstExtent(
			void
			);

					/* attach all newly alloc extnts*/
static int	AttachAllExtents(
				 void
				 );


					/* detach all attached extnts	*/
int	DetachAllExtents(
			 void
			 );


					/* attach to a shared memory    */
					/* region ( segment )		*/
static void	*AttachShMem(
			     int shm_id
			     );

					/* create table header in memory*/
int	CreateTable(
		    const char	*table_name,	/* table name		*/
		    TABLE	**tablep	/* table pointer	*/
		    );

					/* create table header in memory*/
int	OpenTable(
		  char		*table_name,	/* table name		*/
		  TABLE		**tablep	/* table pointer	*/
		  );

					/* open table for acees items	*/
int	CloseTable(
		   TABLE	*tablep		/* table pointer	*/
		   );

int	DeleteTable(
		    TABLE	*tablep		/* table pointer	*/
		    );

int	set_sys_status(
                        int	status,
                        int	pharm_status,
                        int	doctor_status
			);

					/* Add item record to table	*/
int	AddItem(
		TABLE		*tablep,	/* table pointer	*/
		ROW_DATA	row_data	/* table row data	*/
		);

					/* Delete item record from table*/
int	DeleteItem(
		   TABLE	*tablep,	/* table pointer	*/
		   ROW_DATA	row_data	/* table row data	*/
		   );

int	ActItems(
		 TABLE		*tablep,	/* table pointer	*/
		 int		rewind_flg,	/* rewind table flag	*/
		 OPERATION	func,		/* operation on a row	*/
		 OPER_ARGS	args		/* arguments 4 operation*/
		 );

					/* list all table names		*/
void	ListTables(
		   void
		   );

					/* Compare two table rows : parm*/
int	ParmComp(
		 const void *	row_data1,	/* one table row	*/
		 const void *	row_data2	/* another table row	*/
		 );

					/* Compare two table rows : proc*/
int	ProcComp(
		 const void *	row_data1,	/* one table row	*/
		 const void *	row_data2	/* another table row	*/
		 );

int	UpdtComp(
		 const void *	row_data1,	/* one table row	*/
		 const void *	row_data2	/* another table row	*/
		 );

int	DiprComp(
		 const void *	row_data1,
		 const void *	row_data2
		 );

					/* Rewind table ( go to 1'st row*/
int	RewindTable(
		    TABLE	*tablep		/* table pointer	*/
		    );

					/* List all rows of table	*/
int	GetItem(
		 TABLE		*tablep,	/* table pointer	*/
		 OPER_ARGS	args		/* argument to get data	*/
		 );

int	SetItem(
		 TABLE		*tablep,	/* table pointer	*/
		 OPER_ARGS	args		/* argument to get data	*/
		 );


int     SeqFindItem( 
		 TABLE	*tablep,	// table pointer	
		 OPER_ARGS              // stucture describes key value 
		 );

int	GetProc(
		TABLE		*tablep,	/* table pointer	*/
		OPER_ARGS	args		/* argument to get data	*/
		);

int	get_sys_status(
		  STAT_DATA	*stat_data	/* status record	*/
		  );

int	UpdateTimeStat(
		       int	system,		/* pharmacy / doctors	*/
		       void	*data		/* some stat mesg data*/
		       );

int	IncrementCounters(
			  TABLE		*tablep,/* table pointer	*/
			  OPER_ARGS	args	/* argument to get data	*/
			  );

int	IncrementDoctorCounters(
			  TABLE		*tablep,/* table pointer	*/
			  OPER_ARGS	args	/* argument to get data	*/
			  );

					/* Add current process into tabl*/
int	AddCurrProc(
		    int		retrys,		/* retrys for process	*/
		    short int	proc_type,	/* process type		*/
		    int		eicon_port,	/* eicon port for tsserv*/
		    short int	system		/* pharmacy or doctors	*/
		    );

					/* Get parameters from shm	*/
int	ShmGetParamsByName(
		       	   ParamTab *prog_params,/* params descr struct	*/
		       	   int	    reload_flg	/* is load or reload	*/
		       	   );

int	IncrementRevision(
			  TABLE		*tablep,/* table pointer	*/
			  OPER_ARGS	args	/* argument to get data	*/
			  );

int	UpdateShmParams(
			void
			);

int	GetFreeSqlServer(
			 int	*pid,		/* pid of sql server	*/
			 char	*named_pipe,	/* named pipe of sql srv*/
			 short int proc_type	/* SqlServer or		*/
						/* DocSqlServer		*/
			 );

int	LockSqlServer(
		      TABLE		*tablep,/* table pointer	*/
		      OPER_ARGS		args	/* argument to get data	*/
		      );

int	SetFreeSqlServer(
			 int	pid		/* pid of sql server	*/
			 );

int	ReleaseSqlServer(
			 TABLE		*tablep,/* table pointer	*/
			 OPER_ARGS	args	/* argument to get data	*/
			 );

int	SetRecordSize(
		      TABLE	*tablep,	/* table pointer	*/
		      int	size		/* new record size	*/
		      );

int	GetSonsCount(
		     int		*sons	/* son proccesses count	*/
		     );

int	AddToSonsCount(
			int		addition/* addition to son count*/
			);

int	RescaleTimeStat(
		       int	system		/* pharmacy / doctors	*/
		       );

int	RescaleCounters(
			  TABLE		*tablep,/* table pointer	*/
			  OPER_ARGS	args	/* argument to get data	*/
			  );

int	RescaleDoctorCounters(
			  TABLE		*tablep,/* table pointer	*/
			  OPER_ARGS	args	/* argument to get data	*/
			  );

int	GetTabNumByName(
			const char	*table_name
			);

int	UpdateTable(
		    TABLE	*tablep,		/* table pointer*/
		    OPER_ARGS	args			/* arguments	*/
		    );

int	GetTable(
		 TABLE		*tablep,		/* table pointer*/
		 OPER_ARGS	args			/* arguments	*/
		 );

/*
 **
 *** S O C K E T S
 *** -------------
 **
 */

int     ListenSocketNamed(
			  char	*socket_name
			  );			/* socket name		*/

int     ConnectSocketNamed(
			   char *socket_name
			   );			/* socket name		*/

int     ListenSocketUnnamed(
			    int port
			    );

int     ConnectSocketUnnamed(
			     int port_num,	/* remote port number   */
			     u_long address	/* remote address       */
			     );

int     AcceptConnection(
			 int	listen_sock,	/* listen socket	*/
			 struct sockaddr *client_addr,/* client addres	*/
			 int *addr_len		/* address lengt	*/
			 );

int
	CloseSocket(
		    int socket_num
		    );				/* socket number	*/

int     ReadSocket(
		   int		socket,		/* socket to read from	*/
		   char		*buffer,	/* buffer to recive data*/
		   int		max_len,	/* length of data       */
		   int		*type,
		   int		*size,		/* type of message	*/
		   int		verbose_flg	/* verbose flag		*/
		   );

int     WriteSocket(
		    int		socket,		/* socket to write thru	*/
		    const char	*buffer,	/* buffer to recive data*/
		    int		buf_len,	/* length of data       */
		    int		type		/* type of message      */
		    );

int	WriteSocketHead(
			int		socket,	/* socket to write thru	*/
			short int	status,	/* system status	*/
			char	*buffer,	/* buffer to recive data*/
			int		buf_len,/* length of data 	*/
			int		type	/* type of message	*/
			);

int
	read_socket_data(
			 int	socket_num,	/* socket number        */
			 char	*buffer,	/* buffer to receive    */
			 int	size,		/* size of data to read */
			 int	verbose_flg	/* verbose flag		*/
			 );

int
	write_socket_data(
			  int	socket_num,	/* socket number        */
			  const char	*buffer,/* buffer to send data  */
			  int	size		/* size of data to send */
			  );

static int
	get_header_details(
			   int	socket_num,	/* socket number        */
			   MESSAGE_HEADER *headerp,/*general message header*/
			   int	*typep,		/* type pointer         */
			   int	*lenp,		/* length pointer       */
			   int	verbose_flg	/* verbose flag		*/
			   );

static int
	header_details(
			   int	socket_num,	/* socket number        */
			   int	type,		/* message type         */
			   int	len		/* message length       */
			   );

void    GetCurrNamedPipe(
			 char	*buffer		/* buffer to get data	*/
			 );

int	DisposeSockets(
		       void
		       );

char	*GetPeerAddr(
		     int socket_num		/* socket number	*/
		     );

int	GetSocketMessage(
			 int	micro_sec,	/* micro seconds to wait*/
			 char	*buffer,	/* buffer to get data	*/
			 int	max_len,	/* max length to read	*/
			 int	*size,		/* actual size read	*/
			 int	close_flg	/* socket close flag	*/
			 );

void	SigPipeHandler(
		       int	sig_num		/* signal number	*/
		       );

void	SigAlrmHandler(
		       int	sig_num		/* signal number	*/
		       );

int	GetInterProcMesg(
			 char	*in_buf,	/* input buffer		*/
			 int	*length,	/* length of data	*/
			 char	*out_buf	/* output buffer	*/
			 );

int	SetInterProcMesg(
			 char	*in_buf,	/* input buffer		*/
			 int	length,		/* length of data	*/
			 int	mesg_type,	/* direct / via file	*/
			 char	*out_buf,	/* output buffer	*/
			 int	*out_len	/* output buffer length	*/
			 );


/*
 **
 *** F A T H E R  P R O C E S S
 *** --------------------------
 **
 */

					/* Load params from db into shm	*/
int	sql_dbparam_into_shm(
			     TABLE	*parm_tablep,	/* Table pointer*/
			     TABLE	*stat_tablep,	/* Table pointer*/
			     int	reload_flg	/* reload flag	*/
			     );

					/* Load initial params from db	*/
int	SqlGetParamsByName(
		       	   ParamTab	*prog_params,/*params desc strct*/
		       	   int		reload_flg/* is load or reload	*/
		       	   );

int	update_sys_stat(
			TABLE	*stat_tablep,	/* status table pointer	*/
			STAT_DATA	*stat_data/* status record	*/
			);

/*
 **
 *** M E M O R Y
 *** -----------
 **
 */

void	*MemAllocExit(
		      char	*source_name,	/* source name		*/
		      int	line_no,	/* line number		*/
		      size_t	size,		/* mem size to allocate	*/
		      char	*Memfor		/* err text if err	*/
		      );

void	*MemReallocExit(
			char	*source_name,	/* source name		*/
			int	line_no,	/* line number		*/
			void	*old_ptr,	/* original mem pointer	*/
			size_t	new_size,	/* new size to allocate	*/
			char	*mem_for	/* err text if err	*/
			);

void	*MemAllocReturn(
			char	*source_name,	/* source name		*/
			int	line_no,	/* line number		*/
			size_t	size,		/* mem size to allocate	*/
			char	*mem_for	/* err text if err	*/
			);

void	*MemReallocReturn(
			  char	*source_name,	/* source name		*/
			  int	line_no,	/* line number		*/
			  void	*old_ptr,	/* original mem pointer	*/
			  size_t new_size,	/* new size to allocate	*/
			  char	*mem_for	/* err text if err	*/
			  );

void	MemFree(
		void	*ptr			/* mem pointer to free	*/
		);

int	ListMatch(
		  char	*key,			/* key to match in list	*/
		  char	**list			/* string list pointer	*/
		  );

int	ListNMatch(
		  char *key,			/* key to match in list	*/
		  char **list			/* string list pointer	*/
		  );

void	StringToupper(
		      char	*str		/* string to upper-case	*/
		      );

void	BufConvert(
		   char	*source,		/* buffer to convert	*/
		   int	size			/* convert 'size' bytes	*/
		   );

void	GetCurrProgName(
			char	*path_name,	/* path name of file	*/
			char	*file_name	/* file name without '.'*/
			);

int	InitEnv( 
		char	*source_name,		/* source name		*/
		int	line_no			/* line number		*/
		);

int	HashEnv(
	        char	*source_name,		/* source name		*/
	  	int	line_no			/* line number		*/
		);

int	InitSonProcess(
		       char	*proc_pathname,	/* process pathname	*/
		       int	proc_type,	/* process type		*/
		       int	retrys,		/* num of retrys	*/
		       int	eicon_port,	/* num of retrys	*/
		       short int system		/* pharmacy or doctors	*/
		       );

int	ExitSonProcess(
		       int	status		/* exit status of proc	*/
		       );

int	BroadcastSonProcs(
			  int	proc_flg,	/* which procs shutdown	*/
			  TABLE	*proc_tablep,	/* proc table pointer	*/
			  int	mess_type	/* message type		*/
			  );

int	GetCommonPars(
		      int	argc,		/* num of args		*/
		      char	**argv,		/* arguments		*/
		      int	*retrys		/* num of retrys	*/
		      );

int	GetFatherNamedPipe(
    char*	father_namedpipe		/* father proc named pip*/
    );

int	Run_server	(
			 PROC_DATA	*proc_data /* process data	*/
			 );

int	ToGoDown	(
			 int	system		/* system of process	*/
			 );

int	GetSystemLoad	(
			 int interval
			 );


/*
 ** SQL SERVER
 **
 */ 

int	MakeAndSendReturnMessage(
    char	*input_buf,			/* input message buffer	*/
    int		input_size			/* input buffer size	*/
    );						/* process message	*/

/*
 ** AS400 SERVER
 **
 */ 

int	ProcessAs400Message(
			    void
			    );			/* process message	*/

/*
 ** HANDLERS & UTILS
 **
 */

void	alarm_handler(
		      int	sig		/* signal caught	*/
		      );

int	GetTime(				/* get current time	*/
		void
		);

int	GetDate(				/* get current date	*/
		void
		);

// DonR 29Dec2019: Added functions to substitute for Informix database timestamps.
long GetCurrentYearToSecond		(void);
long GetIncrementedYearToSecond	(int IncrementInSeconds_in);


int	GetDayOfWeek (void);	// Sunday = 0, Saturday = 6.

// DonR 11Sep2012: IncrementDate() is now implemented as a macro, resolving to AddDays().
// int IncrementDate (int db_date_in, int change_date_in);
#define IncrementDate(db_date_in,change_date_in) AddDays(db_date_in,change_date_in)

int AddHours (int db_date_in,
			  int db_time_in,
			  int add_hours_in,
			  int *db_date_out,
			  int *db_time_out);

int IncrementTime (int db_time_in, int change_time_in, int *db_date_in_out);

int	GetMessageIndex(
			int system,		/* pharmacy / doctors	*/
			int message_num		/* message number	*/
			);

int	RefreshPrescriptionTable(
				 int	entry
				 );

char *GetComputerShortName (void);

int	UpdateLastAccessTime(
			     void
			     );

int	UpdateLastAccessTimeInternal(
				     TABLE		*proc_tablep,
				     OPER_ARGS		args
				     );

int	GetPrescriptionId(
			  TABLE		*dcdr_tablep,
			  OPER_ARGS	args
			  );

int	GetMessageRecId(
			  TABLE		*dcdr_tablep,
			  OPER_ARGS	args
			  );

void	my_nap( int miliseconds );

int	run_cmd( char * command, char * answer );

char *	GetWord( char * cptr, char * word );

void	buf_reverse( unsigned char *, int );

double ConvertUnitAmount (double amount_in, char *unit_in);

int ParseISODateTime(const char* datetime_in, int* date_out, int* time_out);

/* EPH: Allow use by C++ */
#ifdef __cplusplus //
};
#endif //

/*=======================================================================
||									||
||		Structures description for all programs			||
||									||
 =======================================================================*/

DEF_TYPE char
	*SysState[] 
#ifdef MAIN //
=
{
  (char *)"System is going up",
  (char *)"System is up",
  (char *)"System is going down",
  (char *)"System is down"
}
#endif //
;

DEF_TYPE char
	*PcMessages[]
#ifdef MAIN //
=
{
  (char *)"Reload params from db",
  (char *)"Shutdown all system cleanly",
  (char *)"Shutdown all system immediately",
  (char *)"Send me processes state",
  (char *)"Is the system up",
  (char *)"Process message",
  (char *)"Send me message statistics",
  (char *)"Send me time statistics",
  (char *)"Send me last messages",
  (char *)"Get doctor process table",
  (char *)"Shutdown pharm system only",
  (char *)"Shutdown doctors system only",
  (char *)"Startup pharm system only",
  (char *)"Startup doctors system only",
  (char *)"Send me doctors message statistics",
  (char *)"Send me doctors time statistics",
  (char *)"Send me doctors last messages",
  NULL
}
#endif //
;


// WARNING: Whenever you extend the list below, increment TOTAL_MSGS accordingly!!!
DEF_TYPE int
	PharmMessages[]
#ifdef MAIN //
=
{
  ALL_MSGS,	/* virtual message to sum all data	*/
  1001,		/* pharmacy messages			*/
  1003,		/* NIU - pharmacies now all use 1063. */
  1005,		/* NIU - pharmacies now all use 1065. */
  1007,
  1009,
  1011,
  1013,
  1014,
  1080,	// Pharmacy Contract Update.
  1022,
  1026,
  1028,
  2033,	// DonR 19Aug2003: Changed from 1032, which was not in use.
  2072,
  1043,
  1047,
  1049,
  1051,
  1053,
  1055,
  1057,
  2074,
  2076,
  1063,/*Yulia 20021031*/
  1065,
  2001,
  2003,
  2005,
  2007,
  2070,
  0			/* end of message numbers list	*/
}
#endif //
;

/* Note : addition of new items to DoctorMessages below forces 'side effects' e.g. 
   in module DocSqlServer. Workarround : to change not used value for new actual one 
   06/06/2002
*/
// WARNING: Whenever you extend the list below, increment TOTAL_DOC_MSGS accordingly!!!
DEF_TYPE int
	DoctorMessages[]
#ifdef MAIN //
=
{
  ALL_MSGS,	/* virtual message to sum all data	*/
  1,		/* doctors messages			*/   /* 0*/
  111,//Y20080729
  112,//20030710
  182,//20030710
  104,/*20020502*/
  11,
  114,/*20010909*/                           /* 5*/
  117,/*20010306*/
  12,
  120,/*20010306*/
  124, //Y0621 134,/*20020610*/                     
  13,										/*10*/
  14,
  15,//Y0621  16,
  17,                                        
  18,
  2,										/*15*/
  20,
  21,/*20080701 Yulia*/
  23,/*Y20041129*/
  24,	/* DonR 17Oct2004: Patient's Special Prescriptions, Spec. Pr's, Drugs in Blood.*/
  25,/*20080626 Yulia*/
  26,                                       
  28,
  29,
  3,
  32,/*rentgen 05.2000 */                   /*23*/
  33,	/* DonR 03Dec2012: Same as 23 but includes "Yahalom" fields. */
  37,
  38,
  4,
  41,/*20030518*/
  43,/*20030518*/                           /*28*/
  48,/*20010201*/
  49,/*20010201*/
  50,                                       /*31*/
  52,/*new term params 07.2000 */
  53,	/* DonR 07Apr2013: Same as 33 but with some additional fields. */
  54,
  58,/*new shift open 20050621 */
  62,/*20040624* Yulia new 22*/
  68,/*20080626 Yulia*/
  70,/*20030518*/                           
  72,/*20030518*/
  74,/*20030518*/                           /*38*/
  75,
  76,
  77,
  78,
  79,                                       /*43*/
  8,
  80,
  81,
  82,
  83,                                       /*48*/
  84,
  85,
  86,
  88,
  89,                                       /*53*/
  9,
  90,
  91,/*Cl85 20050621*/
  93,
  94,
  95,/*Cl85 20050621*/
  96,	/* DonR 03Dec2012: Same as 82 but includes "Yahalom" fields. */
  9999,
  0			/* end of message numbers list	*/
}
#endif //
;

DEF_TYPE char
	*serve_shake
DEF_INIT(
  (char *)"hello world, it's macabi user, please serve me:"
);

DEF_TYPE EnvTab				/* Environment buffer		*/
	EnvTabPar[]
#ifdef MAIN //
=
{
  { LogDir,	(char *)"MAC_LOG" 	},
  { LogFile,	(char *)"MAC_LGFILE" 	},
  { MacUser,	(char *)"MAC_USER" 	},
  { MacUid,	(char *)"MAC_UID" 	},
  { MacPass,	(char *)"MAC_PASS" 	},
  { MacDb,	(char *)"MAC_DB" 	},
  { NamedpDir,	(char *)"MAC_PIP"	},
  { NULL,	NULL	  	}
}
#endif //
;

DEF_TYPE ParamTab			/* parameters from database	*/
	FatherParams[]
#ifdef MAIN //
=
{
  { (void*)&ProcRetrys,	(char *)"retrys",	PAR_INT,
    PAR_RELOAD,	MAC_TRUE },

  { (void*)&interval,	(char *)"interval",	PAR_INT,
    PAR_RELOAD, MAC_TRUE },

  { (void*)&SharedSize,	(char *)"shared_size",	PAR_INT,
    PAR_LOAD,	MAC_TRUE },

  { (void*)BinDir,	(char *)"bin_dir",	PAR_CHAR,
    PAR_RELOAD,	MAC_TRUE },

  { (void*)CoreDir,	(char *)"core_dir",	PAR_CHAR,
    PAR_RELOAD,	MAC_TRUE },

  { (void*)LogDir,	(char *)"log_dir",	PAR_CHAR,
    PAR_RELOAD,	MAC_TRUE },

  { (void*)MsgDir,	(char *)"msg_dir",	PAR_CHAR,
    PAR_RELOAD,	MAC_TRUE },

  { (void*)NamedpDir,	(char *)"namedp_dir",	PAR_CHAR,
    PAR_RELOAD,	MAC_TRUE },

  { (void*)LogFile,	(char *)LOG_FILE_KEY,	PAR_CHAR,
    PAR_RELOAD,	MAC_TRUE },

//  { (void*)EiconPorts,	(char *)"eicon_ports",	PAR_CHAR,
//    PAR_RELOAD,	MAC_TRUE },
//
//  { (void*)DocEiconPorts,(char *)"doc_eicon_ports",	PAR_CHAR,
//    PAR_RELOAD,	MAC_TRUE },
//
//  { (void*)DocTcpipPorts,(char *)"doc_tcpip_ports",	PAR_CHAR,
//    PAR_RELOAD,	MAC_TRUE },
//
//  { (void*)&InitSqlP,	(char *)"init_sql",	PAR_INT,
//    PAR_RELOAD,	MAC_TRUE },
//
//  { (void*)&InitDocSqlP,	(char *)"init_doc_sql",	PAR_INT,
//    PAR_RELOAD,	MAC_TRUE },
//
//  { (void*)&InitTssP,	(char *)"init_tss",	PAR_INT,
//    PAR_RELOAD,	MAC_TRUE },
//
//  { (void*)&InitTssListen,	(char *)"init_listen",	PAR_INT,
//    PAR_RELOAD,	MAC_TRUE },
//
//  { (void*)&InitTssPort,	(char *)"port_count",	PAR_INT,
//    PAR_RELOAD,	MAC_TRUE },
//
//  { (void*)&SpareSqlP,	(char *)"spare_sql",	PAR_INT,
//    PAR_RELOAD,	MAC_TRUE },
//
//  { (void*)&MaxSqlP,	(char *)"max_sql",	PAR_INT,
//    PAR_RELOAD,	MAC_TRUE },
//
//  { (void*)&SpareDocSqlP,	(char *)"spare_doc_sql",	PAR_INT,
//    PAR_RELOAD,	MAC_TRUE },
//
//  { (void*)&MaxDocSqlP,	(char *)"max_doc_sql",	PAR_INT,
//    PAR_RELOAD,	MAC_TRUE },
//
  { (void*)&ReadTimeout,(char *)"read_timeout",	PAR_INT,
    PAR_RELOAD,	MAC_TRUE },

  { (void*)&WriteTimeout,(char *)"write_timeout",PAR_INT,
    PAR_RELOAD,	MAC_TRUE },

  { (void*)&SelectWait,	(char *)"select_wait",	PAR_INT,
    PAR_RELOAD,	MAC_TRUE },

  { (void*)&FATAL_ERROR_LIMIT,	(char *)"fatal_error_limit",	PAR_INT,
    PAR_RELOAD,	MAC_TRUE },

  { (void*)&DEFAULT_SEVERITY,	(char *)"default_severity",	PAR_INT,
    PAR_RELOAD,	MAC_TRUE },

  { (void*)&SQL_UPDATE_RETRIES,	(char *)"sql_update_retries",	PAR_INT,
    PAR_RELOAD,	MAC_TRUE },

  { (void*)&SHM_UPDATE_INTERVAL,(char *)"shm_update_interval",	PAR_INT,
    PAR_RELOAD,	MAC_TRUE },

  { (void*)&SPCL_PRESC_PRE_SALE_DAYS,	(char *)"spcl_presc_pre_sale_days",PAR_INT,
    PAR_RELOAD,	MAC_TRUE },

  { (void*)&ACCESS_CONFLICT_SLEEP_TIME,	(char *)"access_conflict_sleep_time",PAR_INT,
    PAR_RELOAD,	MAC_TRUE },

  { (void*)&LockPagesMode,	(char *)"lock_pages_mode",	PAR_INT,
    PAR_RELOAD,	MAC_TRUE },

  { (void*)&LogCommErr,		(char *)"log_comm_err",		PAR_INT,
    PAR_RELOAD,	MAC_TRUE },

  { (void*)&LogMesgErr,		(char *)"log_mesg_err",		PAR_INT,
    PAR_RELOAD,	MAC_TRUE },

//  { (void*)TswProgName,		(char *)"tsw_prog_name",	PAR_CHAR,
//    PAR_RELOAD,	MAC_TRUE },
//
  { (void*)&X25TimeOut,		(char *)"x25_time_out",		PAR_INT,
    PAR_RELOAD,	MAC_TRUE },

//  { (void*)&SAR_INTERVAL,	(char *)"sar_interval",		PAR_INT,
//    PAR_RELOAD,	MAC_TRUE },
//
  { (void*)&SYSTEM_MAX_LOAD,	(char *)"system_max_load",	PAR_INT,
    PAR_RELOAD,	MAC_TRUE },

  { (void*)&SharedMemUpdateInterval,	(char *)"shm_update_proc",	PAR_INT,
  PAR_RELOAD,	MAC_TRUE },

  { (void*)&GoDownCheckInterval,	(char *)"godown_check_interval",PAR_INT,
    PAR_RELOAD,	MAC_TRUE },

  { (void*)&TRANSFER_INTERVAL,	(char *)"transfer_interval",	PAR_INT,
    PAR_RELOAD,	MAC_TRUE },

  { (void*)&SLEEP_BETWEEN_TABLES,(char *)"sleep_between_tables",PAR_INT,
    PAR_RELOAD,	MAC_TRUE },

//  { (void*)&LOG_X25_CONNECTIONS,(char *)"log_x25_connections",PAR_INT,
//    PAR_RELOAD,	MAC_TRUE },
//
  { NULL,		NULL,		0,
    0,		MAC_TRUE }
}
#endif //
;


//***********************************************************************
// 
//  All the parameters are defined below will be attemptd to initialize
//  from every son process; so, in setup table in db the program_name
//  must be set to 'All', otherwise some children will not find the 
//  parameters.
//
//***********************************************************************
DEF_TYPE ParamTab			/* parameters from shared memory*/
	SonProcParams[]
#ifdef MAIN //
=
{
  { (void*)&ProcRetrys,	(char *)"retrys",			PAR_INT,
    PAR_RELOAD,	MAC_TRUE },

  { (void*)NamedpDir,	(char *)"namedp_dir",			PAR_CHAR,
    PAR_RELOAD,	MAC_TRUE },

  { (void*)MsgDir,	(char *)"msg_dir",			PAR_CHAR,
    PAR_RELOAD,	MAC_TRUE },

  { (void*)LogFile,	(char *)LOG_FILE_KEY,			PAR_CHAR,
    PAR_RELOAD,	MAC_TRUE },

  { (void*)MacUid,	(char *)"mac_uid",			PAR_CHAR,
    PAR_RELOAD,	MAC_TRUE },

  { (void*)MacUser,	(char *)"mac_user",			PAR_CHAR,
    PAR_RELOAD,	MAC_TRUE },

  { (void*)MacPass,	(char *)"mac_pass",			PAR_CHAR,
    PAR_RELOAD,	MAC_TRUE },

  { (void*)MacDb,	(char *)"mac_db",			PAR_CHAR,
    PAR_RELOAD,	MAC_TRUE },

  { (void*)&ReadTimeout,(char *)"read_timeout",			PAR_INT,
    PAR_RELOAD,	MAC_TRUE },

  { (void*)&WriteTimeout,(char *)"write_timeout",		PAR_INT,
    PAR_RELOAD,	MAC_TRUE },

  { (void*)&SelectWait,	(char *)"select_wait",			PAR_INT,
    PAR_RELOAD,	MAC_TRUE },

  { (void*)&SharedSize,	(char *)"shared_size",			PAR_INT,
    PAR_LOAD,	MAC_TRUE },

  { (void*)&FATAL_ERROR_LIMIT,	(char *)"fatal_error_limit",	PAR_INT,
    PAR_RELOAD,	MAC_TRUE },

  { (void*)&DEFAULT_SEVERITY,	(char *)"default_severity",	PAR_INT,
    PAR_RELOAD,	MAC_TRUE },

  { (void*)&SQL_UPDATE_RETRIES,	(char *)"sql_update_retries",	PAR_INT,
    PAR_RELOAD,	MAC_TRUE },

  { (void*)&SHM_UPDATE_INTERVAL,(char *)"shm_update_interval",	PAR_INT,
    PAR_RELOAD,	MAC_TRUE },

  { (void*)&SPCL_PRESC_PRE_SALE_DAYS,	(char *)"spcl_presc_pre_sale_days",PAR_INT,
    PAR_RELOAD,	MAC_TRUE },

  { (void*)&ACCESS_CONFLICT_SLEEP_TIME,	(char *)"access_conflict_sleep_time",PAR_INT,
    PAR_RELOAD,	MAC_TRUE },

  { (void*)&LockPagesMode,	(char *)"lock_pages_mode",	PAR_INT,
    PAR_RELOAD,	MAC_TRUE },

  { (void*)&LogCommErr,		(char *)"log_comm_err",		PAR_INT,
    PAR_RELOAD,	MAC_TRUE },

  { (void*)&LogMesgErr,		(char *)"log_mesg_err",		PAR_INT,
    PAR_RELOAD,	MAC_TRUE },

//  { (void*)TswProgName,		(char *)"tsw_prog_name",	PAR_CHAR,
//    PAR_RELOAD,	MAC_TRUE },
//
  { (void*)BinDir,		(char *)"bin_dir",		PAR_CHAR,
    PAR_RELOAD,	MAC_TRUE },

  { (void*)&X25TimeOut,		(char *)"x25_time_out",		PAR_INT,
    PAR_RELOAD,	MAC_TRUE },

//  { (void*)&SAR_INTERVAL,	(char *)"sar_interval",		PAR_INT,
//    PAR_RELOAD,	MAC_TRUE },
//
  { (void*)&SYSTEM_MAX_LOAD,	(char *)"system_max_load",	PAR_INT,
    PAR_RELOAD,	MAC_TRUE },

  { (void*)&TRANSFER_INTERVAL,	(char *)"transfer_interval",	PAR_INT,
    PAR_RELOAD,	MAC_TRUE },

  { (void*)&SLEEP_BETWEEN_TABLES,(char *)"sleep_between_tables",PAR_INT,
    PAR_RELOAD,	MAC_TRUE },

//  { (void*)&LOG_X25_CONNECTIONS,(char *)"log_x25_connections",PAR_INT,
//    PAR_RELOAD,	MAC_TRUE },
//
//  { (void*)&InitTssListen,	(char *)"init_listen",	PAR_INT,
//    PAR_RELOAD,	MAC_TRUE },
//
//  { (void*)&InitTssPort,	(char *)"port_count",	PAR_INT,
//    PAR_RELOAD,	MAC_TRUE },
//
  { (void*)&SharedMemUpdateInterval,	(char *)"shm_update_proc",	PAR_INT,
  PAR_RELOAD,	MAC_TRUE },

  { (void*)&GoDownCheckInterval,	(char *)"godown_check_interval",PAR_INT,
    PAR_RELOAD,	MAC_TRUE },

//  { (void*)ReplicPartner,	(char *)"replication_partner",	PAR_CHAR,
//    PAR_RELOAD,	MAC_TRUE },
//
  { NULL,		NULL,		0,
    0,		MAC_TRUE }
}
#endif //
;

#if defined(MAIN) && defined(NON_SQL) && 0//
TABLE_DATA	TableTab[] =
{
  { 0, 0,			NOT_SORTED,		NULL,
    NULL,		"",			"",	}
};
#endif //
/*=======================================================================
||									||
||		Global error codes for project				||
||									||
 =======================================================================*/
/*
 * Global error codes
 */
#define	MAC_OK		0	/* Everything o.k.			*/
#define	SIGACTION_ERR	-9998	/* Error in setting signal handler	*/
#define	NOT_IMPLEMENTED	-9999	/* Function currently not implemented	*/

/*
 * Semaphore error codes
 */
#define	SEM_CREAT_ERR	-100	/* Semaphore creation error		*/
#define	SEM_DELETE_ERR	-101	/* Semaphore deletion error		*/
#define	SEM_ALREADY_OPN	-102	/* Semaphore already open		*/
#define	SEM_OPEN_ERR	-103	/* Semaphore open error			*/
#define	SEM_CLOSE_ERR	-104	/* Semaphore close error		*/
#define	SEM_NOT_OPEN	-105	/* Semaphore not open			*/
#define	SEM_WAIT_ERR	-106	/* Semaphore wait error			*/
#define	SEM_RELIS_ERR	-107	/* Semaphore release error		*/
#define	SEM_IN_WAIT	-108	/* Semaphore already waiting on semaphor*/
#define	SEM_RELISED	-109	/* Semaphore already released 		*/

/*
 * Shared memory error codes
 */
#define	SHM_REMOVE_ERR	-200	/* Shared memory removal error		*/
#define	SHM_GET_ERR	-201	/* Shared memory creation error		*/
#define	SHM_ATT_ERR	-202	/* Shared memory attach error		*/
#define	SHM_DET_ERR	-203	/* Shared memory detach error		*/
#define	SHM_NO_INIT	-204	/* Shared memory not initialized	*/
#define	SHM_KILL_ERR	-205	/* Shared memory deletion error		*/
#define	SHM_TAB_EXISTS	-206	/* Shared memory table exists already	*/
#define	SHM_TAB_NOT_EX	-207	/* Shared memory table not exists 	*/
#define	CONN_MAX_EXCEED	-208	/* Shared memory table conn max exceeded*/
#define	TAB_PTR_INVALID	-209	/* Shared memory table pointer invalid	*/
#define	DUP_TAB_KEY_ERR	-210	/* Shared memory table duplicate key	*/
#define	NULL_KEY_PTR	-211	/* Shared memory table key pointer null	*/
#define	ROW_NOT_FOUND	-212	/* Shared memory table row not found	*/
#define	DATA_LEN_ERR	-213	/* Shared memory table record length err*/
#define	COMP_FUN_ERR	-214	/* Shared memory table record compar err*/
#define	NO_MORE_ROWS	-215	/* Shared memory table empty or end of -*/
#define	NO_COMP_FUN	-216	/* Shared memory table record compar nul*/
#define	NO_FIRST_EXT	-217	/* Shared memory first extent not attach*/
#define	TAB_NAM_ERR	-218	/* Shared memory table name error	*/
#define	MSG_NUM_ERR	-219	/* Message number is not in array	*/

/*
 * Sockets error codes
 */
#define	ACCEPT_ERR	-300	/* Accept new connection failed		*/
#define	COM_END		-301	/* Communication closed by other side	*/
#define	READ_ERR	-302	/* Error while reading socket	 	*/
#define	WRITE_ERR	-303	/* Error while writing socket	 	*/
#define	BAD_ADDR	-304	/* Bad address - not known to inet_addr	*/
#define	CONN_END	-306	/* Connection reset by remote side	*/
#define	HDR_ERR		-307	/* Header of message is wrong		*/
#define	NO_SOCKET	-308	/* Can't obtain a socket		*/
#define	BIND_ERR	-309	/* Can't bind a socket to a name	*/
#define	LISTEN_ERR	-310	/* Can't listen on socket		*/
#define	CONNECT_ERR	-311	/* Can't connect to socket		*/
#define	SOCK_OPT_ERR	-312	/* Can't set socket option		*/
#define	DATA_DISCARDED	-313	/* Some of data received - discarded	*/
#define	NAMEDP_DEL_ERR	-314	/* Can't remove named pipe		*/
#define	NO_LISTEN_SOCK	-315	/* No listen socket			*/
#define	NO_CONNECT_SOCK	-316	/* No connect socket			*/
#define	SELECT_ERR	-317	/* Select error				*/
#define	TIMEOUT_READ	-318	/* Timeout at read			*/
#define	WRITE_NOWHERE	-319	/* Write to nowhere			*/
#define	NO_FILE		-320	/* File doesn't exist			*/
#define	TIMEOUT_WRITE	-321	/* Timeout at write			*/

/*
 * Memory error codes
 */
#define	MEM_ALLOC_ERR	-400	/* memory allocation error		*/
#define	ENV_PAR_MISS	-401	/* environment variable missing		*/
#define	TOO_FEW_ARGS	-402	/* too few arguments on command line	*/
#define	NO_FATHER_PIP	-403	/* no father process named pipe in shm	*/

/*
 * SQL error codes
 */
#define	SQL_PARAM_MISS	-500	/* parameter to load missing in table	*/
#define	INF_ERR		-501	/* informix error			*/



/*=======================================================================
||									||
||		End of include file global.h				||
||									||
 =======================================================================*/

#endif // /* GLOBAL_H	*/

