/* << -------------------------------------------------------- >> */
/*                                                           	  */
/*			   As400UnixServer.h                      */
/*                                                           	  */
/*  -----------------------------------------------------------   */
/*                                                           	  */
/*  Date: 	Oct-Nov 1996					  */
/*  Written by: Gilad Haimov ( reshuma )			  */
/*                                                           	  */
/*  -----------------------------------------------------------   */
/*                                                           	  */
/*  Purpose :							  */
/*         Header file for As400UnixServer.ec => server-process   */
/*         in AS400 <-> Unix communication model.       	  */
/*  -----------------------------------------------------------   */
/*                                                           	  */
/*  Comments:      							  */
/*                                                           	  */
/* << -------------------------------------------------------- >> */

#if ! defined( _UNIX_AS400_COMM_SERVER_ )
#define _UNIX_AS400_COMM_SERVER_

/* << -------------------------------------------------------- >> */
/*  			   #include files                         */
/* << -------------------------------------------------------- >> */
#include <assert.h>
#include "As400UnixGlob.h"


#ifndef BOOL_DEFINED
	#define BOOL_DEFINED
	typedef enum tag_bool
	{
		false = 0,
		true
	}
	bool;                  /* simple boolean typedef */
#endif

#ifndef GLOBALSYSDATE_DEFINED
	#define GLOBALSYSDATE_DEFINED
	#ifdef	MAIN
		int			GlobalSysDate;
	#else
		extern int	GlobalSysDate;
	#endif
#endif

/* << -------------------------------------------------------- >> */
/*  			  #defining Constants                     */
/* << -------------------------------------------------------- >> */
#define	AS400_UNIX_PORT_BATCH	9002
#define WITH_ROLLBACK			1
#define WITHOUT_ROLLBACK		0
#define ERROR_ON_RECEIVE		-1
#define BAD_DATA_RECEIVED		-2
#define MSG_HEADER_LEN			4	/*  length of message-header	*/
#define DATA_MSG_TYPE_LEN		1	/*  length of data message type	*/
#define NO_FREE_CONNECTIONS		NULL	/*  no free client left		*/
#define MAX_NUM_OF_CLIENTS		30	/*  max connections		*/
#define DATA_PARSING_ERR		65	/* error while parsing message	*/
#define ALL_RETRIES_WERE_USED	66	/* data conflict error		*/
#define SQL_ERROR_OCCURED		67	/* SQL err while processing msg */
#define MSG_HANDLER_SUCCEEDED	1	/* message handler success code */
#define UNKNOWN_COMMAND			-3

#define COMMUNICATIONS_MESSAGE	0
#define BEGIN_TABLE_COMMAND		1
#define TABLE_DATA				2
#define	END_TABLE_COMMAND		3
#define GET_TABLE_SIZE_RANGE	4
#define CLIENT_LOG_MESSAGE      5

int  UPDATED_BY_AS400  = 7;

typedef enum tag_TServerState
{
  CONNECTED_NOT_IN_TRANSACTION = 100,
  CONNECTED_IN_TRANSACTION,
  NOT_CONNECTED
}
TServerState ;    	/* possible states of Server process	*/

typedef enum tagTShmUpdate 
{ 
	NONE = 0, 
	DRUG_LIST, 
	PRICE_LIST, 
	MEM_PRICE, 
	PHARMACY,
	DOCTOR_PERCENTS,
	DOCTOR_SPECIALITY,
	SPCLTY_LARGO_PRCNT,
	DRUG_INTERACTION,
	DRUG_EXTENSION,
	SUPPLIERS, 
	MAC_CENTERS,
	ERR_MSG,	
	/* 8 new fields for new transaction 2047-2056*/
	DRUGSUBST, /* NIU */        
	GENCOMPONENTS,
	DRUGGENCOMPONENTS,
	GENERYINTERACTION,
	GNRLDRUGNOTES,
	DRUGNOTES,
	DRUGDOCTORPROF,
	ECONOMYPRI,
	PHARMACOLOGY,
	GENINTERNOTE,
	DRUG_LIST_DOCTOR,	/*20020401 Yulia*/
	PHARMACY_CONTRACT,	/* DonR 21Oct2003 */
	PRESC_SOURCE,		/* DonR 12Nov2003 */
	CONFIRM_AUTHORITY,	/* DonR 12Nov2003 */
	GADGETS,			/* DonR 27May2004 */
	CLICKS_DISCOUNT,	/* DonR 06Jun2004 */
	DRUG_FORMS,			/* DonR 06Jun2004 */
	USAGE_INSTRUCTIONS,	/* DonR 06Jun2004 */
	TREATMENT_GROUP,	/* DonR 06Jun2004 */
	PRESCR_PERIOD,		/* DonR 06Jun2004 */
	DRUG_EXTN_DOC,		/* DonR 06Jun2004 */
	SALE,				// DonR 21Jun2005 MaccabiCare
	SALE_FIXED_PRICE,	// DonR 21Jun2005 MaccabiCare
	SALE_BONUS_RECV,	// DonR 21Jun2005 MaccabiCare
	SALE_TARGET,		// DonR 21Jun2005 MaccabiCare
	SALE_PHARMACY,		// DonR 21Jun2005 MaccabiCare
	PHARMDRUGNOTES,		// DonR 09Feb2006
	PURCHASE_LIM_ISHUR,	// DonR 27Jun2006
	DRUG_PURCHASE_LIM,	// DonR 27Jun2006
	MEMBER_PHARM,		// DonR 22Nov2006
	COUPONS,			// DonR 21Mar2010
	CREDIT_REASONS,		// DonR 21Mar2010
	SUBSIDY_MESSAGES,	// DonR 21Mar2010
	TIKRA_TYPE,			// DonR 21Mar2010
	HMO_MEMBERSHIP,		// DonR 21Mar2010
	PURCH_LIM_CLASS		// DonR 25May2010


 
#ifdef DOCTORS_SRC
,
	LABNOTES,
	LABRESULTS,
	TEMPMEMB,
	AUTH,
	CONTRACT,
	CONTERMDEL,
	CONMEMB,
	HOSPITALMSG1,
	HOSPITALMSG2,
	SPEECHCLIENT,
	RASHAM,
	MEMB_MESS,
	CLICKSTBLUPD,
	CONTRACTNEW,	//Yulia 20131110
	CONTERMNEW,		//Yulia 20131110
	TERMINALNEW,	//Yulia 20131110
	SUBSTNEW		//Yulia 20131110

#endif 
} TShmUpdate; 

typedef struct
{
  TSocket	socket;
  TServerState  server_state;
  short int	in_tran_flag;
  TIP_Address	ip_address;
  bool		sys_is_busy;
  int		sql_err_flg;
  time_t	last_access;
  char		db_connection_id[32];
}
TConnection;		/* details of a single client-server connection */


/*
 *  Order of message-handlers DML commands:
 */
#define ATTEMPT_INSERT_FIRST    true    /*   insert & then update	*/
#define ATTEMPT_UPDATE_FIRST    false   /*   the other way around	*/


/*
 *  Sleep interval between retries:
 */
#define SLEEP_TIME_BETWEEN_RETRIES   1 /* one second */ 


/* << -------------------------------------------------------- >> */
/*			Global variables                          */
/* << -------------------------------------------------------- >> */

//char	glbMsgBuffer[1024] ; /*  global client-message buffer	*/ yulia 20020620
//char	glbMsgBuffer[2048] ; /*  global client-message buffer	*/ 
char	glbMsgBuffer[4096] ; /*  global client-message buffer	*/ 
TSocket glbListenSocket ;    /*  listen socket                  */
bool    glbErrorFlg;	     /*  error on parsing received buf	*/
bool    PriceArrayInitilized;/*  true after glbPriceListUpdateArray*/
			     /*  initilized                     */

/*
 *  Global array containing all connection-structures:
 */
TConnection glbConnectionsArray[MAX_NUM_OF_CLIENTS] ;

/*
 *  Global array of date & time values to be used in shared-memory
 *          refreshment routine for table "Price_list" only:
 */
long int     glbPriceListUpdateArray[4] = {0, 0, 0, 0};

/* << -------------------------------------------------------- >> */
/* 			   Other Macros                           */
/* << -------------------------------------------------------- >> */
#define Fatal_error_at_function !

/*
 * all comm. messages are > 9000:
 */
#define NOT_A_COMMUNICATION_MSG( msgID ) (msgID) < 9000

/*
 * Is message ID in AS400 message domain ?
 */
#define LEGAL_MESSAGE_ID( id )	\
		   ( (id) > 1999  &&  (id) < 10000 )


/*
 * zero global error flag before "dangarous" code is entered:
 */
#define INIT_ERROR_DETECTION_MECHANISM  glbErrorFlg = false


/*
 * if global error flag was raised - exit message handler with failure code:
 */
#define RETURN_ON_ERROR		\
if( glbErrorFlg == true  )	\
{				\
	GerrLogReturn( GerrId, "DATA_PARSING_ERR on the line" ); \
	return DATA_PARSING_ERR;	\
}

/*
 * if illegal sign was passed -> return witfh data-parsing error code:
 */
#define RETURN_ON_ILLEGAL_SIGN( s )	\
   switch( *s )				\
   {					\
      case ' ' :			\
      case '+' :			\
      case '-' :			\
	 break; /* -> OK */		\
					\
      default:				\
         return DATA_PARSING_ERR;	\
   }


/*
 * Write data access conflicts to log file:
 */
#define WRITE_ACCESS_CONFLICT_TO_LOG( msgID ) 				\
  GerrLogReturn(											\
		GerrId,												\
		"Failure at handler to message %s due to data "		\
		"access conflict (err = %d).",						\
		msgID, SQLCODE										\
		);

// Handle SQL code after a deletion attempt within a retry-loop.
#define HANDLE_DELETE_SQL_CODE( ID, c, retcode_ptr, db_changed_flg_ptr )	\
	if (SQLERR_code_cmp (SQLERR_access_conflict) == MAC_TRUE)				\
	{																		\
		*retcode_ptr = ALL_RETRIES_WERE_USED;								\
		continue; /* retry */												\
	}																		\
	if (SQLERR_code_cmp (SQLERR_no_rows_affected) == MAC_TRUE)				\
	{																		\
			*retcode_ptr = MSG_HANDLER_SUCCEEDED;							\
	}																		\
	else																	\
	{																		\
		if (SQLERR_error_test () == MAC_OK)									\
		{ 																	\
			*db_changed_flg_ptr = 1;  										\
			*retcode_ptr = MSG_HANDLER_SUCCEEDED;							\
		}																	\
		else																\
		{																	\
			c->sql_err_flg = 1;												\
			*retcode_ptr = SQL_ERROR_OCCURED;								\
			break;															\
		}																	\
	}

// Handle SQL code after an update attempt within a retry-loop.
#define HANDLE_UPDATE_SQL_CODE( ID, c, insert_flg_ptr, rc_ptr, stop_ptr )	\
	if (SQLERR_code_cmp (SQLERR_access_conflict) == MAC_TRUE)				\
	{																		\
		*rc_ptr = ALL_RETRIES_WERE_USED;									\
		break;																\
	}																		\
	if (SQLERR_code_cmp (SQLERR_no_rows_affected) == MAC_TRUE)				\
	{																		\
		*insert_flg_ptr = ATTEMPT_INSERT_FIRST;								\
		continue;	/* try INSERT */										\
	}																		\
	if (SQLERR_error_test () == MAC_OK) 									\
	{																		\
		*rc_ptr = MSG_HANDLER_SUCCEEDED;									\
	}																		\
	else																	\
	{																		\
		*rc_ptr = SQL_ERROR_OCCURED;										\
		c->sql_err_flg = 1;													\
	}																		\
	*stop_ptr = true;														\
	break;


// Handle SQL code after an insert attempt within a retry-loop.
#define HANDLE_INSERT_SQL_CODE( ID, c, insert_flg_ptr, rc_ptr, stop_ptr )	\
	if (SQLERR_code_cmp (SQLERR_access_conflict) == MAC_TRUE)				\
	{																		\
		*rc_ptr = ALL_RETRIES_WERE_USED;									\
		break;																\
	}																		\
	if (SQLERR_code_cmp (SQLERR_not_unique) == MAC_TRUE)					\
	{																		\
		*insert_flg_ptr = ATTEMPT_UPDATE_FIRST;								\
		continue;	/* try UPDATE */										\
	}																		\
	if (SQLERR_error_test () == MAC_OK)										\
	{																		\
		*rc_ptr = MSG_HANDLER_SUCCEEDED;									\
	}																		\
	else																	\
	{																		\
		*rc_ptr = SQL_ERROR_OCCURED;										\
		c->sql_err_flg = 1;													\
	}																		\
	*stop_ptr = true;														\
	break;


/* << -------------------------------------------------------- >> */
/*			function headers                          */
/* << -------------------------------------------------------- >> */
static void 		UpdateSharedMem( 
					TShmUpdate   tbl_to_update
					); 

static void		close_socket(
				     TSocket   sock
				     ) ;

static void		set_connection_to_not_in_transaction(
							     TConnection * c
							     ) ;

static void		set_connection_to_in_transaction_state(
							       TConnection *
							       ) ;

static int              pipe_is_broken(
					TSocket  sock,
					int milisec
				       );

int	find_free_connection(
			     void
			     ) ;

static void		initialize_struct_to_not_connected(
							   TConnection *
							   ) ;

static TSuccessStat	create_a_new_connection(
						void
						) ;

static void		initialize_connection_array(
						    void
						    ) ;

static TSocket		make_listen_socket(
					   void
					   ) ;

static bool		connection_is_in_transaction (TConnection *);

static bool		connection_is_not_in_transaction (TConnection *);

static TSystemMsg	get_message_ID(
				       TConnection *  c
				       ) ;

static TSystemMsg	get_data_message_type (TConnection *c, TSystemMsg msg);

static TSuccessStat	receive_from_socket(
					    TConnection *,
					    char *,
					    int
					    ) ;

static int		call_msg_handler_pharm (
										 TSystemMsg	   msg,
										 TSystemMsg	   msg_type,
										 TConnection   *p_connect,
										 int	       msg_len
									   ) ;

static int		call_msg_handler(
					 TSystemMsg	   msg,
					 TConnection *     p_connect,
					 int	           msg_len
					 ) ;

static TSuccessStat	inform_client(
				  TConnection	*connect,
				  TSystemMsg   	msg,
				  char			text_buffer[]
				  ) ;

static void		terminate_a_connection(
					       TConnection *	c,
					       int		use_rollback
					       ) ;

static TSuccessStat	DO_ROLLBACK(
				    TConnection  *c
				    ) ;

static int		get_struct_len(
				       TSystemMsg msg
				       ) ;

static int		commit_handler(
					   TConnection *  p_connect,
					   TShmUpdate    shm_table_to_update
					   ) ;

static int		answer_to_all_client_requests(
						      fd_set *
						      );

static int		get_sysem_status(
					   bool	*pSysDown,
					   bool	*pSysBusy
					   )  ;

static int		init_son_process(
					 int		l_argc,
					 char	*l_argv[],
					 int		*pRetrys,
					 int		typ,
					 TABLE	**stat_tablepPtr,
					 TABLE	**proc_tablepPtr
					 );

static short		get_short(
				  char	**p_buffer ,
				  int	ascii_len
				  );

static int		get_long(
				 char	**p_buffer ,
				 int	ascii_len
				 );

static double	get_double(
				 char	**p_buffer ,
				 int	ascii_len
				 );

#ifdef DOCTORS_SRC
static long		get_intv(
				 char	**p_buffer ,
				 int	ascii_len
				 );
#endif //DOCTORS_SRC

static void		get_string(
				   char	** p_buffer ,
				   char	*str,
				   int	ascii_len
				   );

static void		get_str_h(
				   char	** p_buffer ,
				   char	*str,
				   int	ascii_len,
				   short	WinHebFlag
				   );

static bool		legal_digit(
				    const char	*buf,
				    int		pos
				    );

void                    initGlbPriceListArray( void );


    #endif  /* _UNIX_AS400_COMM_SERVER_ */


    /* << -------------------------------------------------------- >> */
    /* 		             E O F                                */
    /* << -------------------------------------------------------- >> */


    /*=======================================================================
    ||									||
    ||			function headers				||	
    ||									||
     =======================================================================*/
    void ascii_to_ebcdic_dic(  
			      char * buf, 
			      int    len 
			     );

    void  switch_to_win_heb( 
			     unsigned char * source 
			    );

    char * Ebcdic_Mem_ToAscii(
			      char		*buffer,
			      unsigned	len
			      );

    char * Ebcdic_Str_ToAscii(
			      char * str
			      ) ;

    char * Ascii_Mem_ToEbcdic(
			      char		*buffer,
			      unsigned	len
			      );

    char * Ascii_Str_ToEbcdic(
			      char *  str
			      );



static int message_2005_handler (TConnection	*p_connect,
								 int			data_len,
								 bool			do_insert,
								 TShmUpdate		*shm_table_to_update,
								 int			*FlgDoctor,
								 int			SetTimestamp_in);


/* << -------------------------------------------------------- >> */
/* 		             E O F                                */
/* << -------------------------------------------------------- >> */
