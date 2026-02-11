/* << -------------------------------------------------------- >> */
/*                                                           	  */
/*  			  As400UnixClient.h                       */
/*                                                           	  */
/*  -----------------------------------------------------------   */
/*                                                           	  */
/*  Date: 	 Oct-Nov 1996					  */
/*  Written by:  Gilad Haimov ( Reshuma Ltd. )			  */
/*                                                           	  */
/*  -----------------------------------------------------------   */
/*                                                           	  */
/*  Purpose :							  */
/*       Header file for client-process in AS400 <-> Unix   	  */
/*       communication system.       				  */
/*  -----------------------------------------------------------   */
/*                                                           	  */
/*  Comments:                                                     */
/*                                                           	  */
/* << -------------------------------------------------------- >> */

/* << -------------------------------------------------------- >> */
/*                          #include                              */
/* << -------------------------------------------------------- >> */
#include <time.h>
#include <assert.h>
#include "As400UnixGlob.h"

#include "Global.h"
#include "DB.h"
#include "DBFields.h"

/* << -------------------------------------------------------- >> */
/*                       user defined types                       */
/* << -------------------------------------------------------- >> */
typedef int ( *TFuncPtr )( void );

typedef struct sockaddr_in Tsockaddr_in;

typedef struct timeval Ttimeval;
/* << -------------------------------------------------------- >> */
/*                          #defines                              */
/* << -------------------------------------------------------- >> */
#define MAIN

#define _IN_MAIN_SRC_FILE_   /* avoiding "extern" before global vars */

/*#define MAX_RECORD_LEN			1024 yulia 20020619*/
/*define MAX_RECORD_LEN			2048 20040615*/
#define MAX_RECORD_LEN			7148


#define Fatal_error_at_function		!


#define MSG_QUEUE_CAPACITY		1024
#define NO_ITEMS_IN_QUEUE		-1
#define CLIENT_CONNECT_STRING		"AS400<->Unix client process"
#define SLEEP_TIME_IN_SECONDS		5
#define AS400_MESSAGE_LENGTH		4

#define MH_SUCCESS			10
#define MH_RESTART_CONNECTION		11
#define MH_FATAL_ERROR			12
#define MH_SYSTEM_IS_BUSY		13
#define MH_MAX_ERROR_WAS_EXCEEDED	14
#define MH_FATHER_GOING_DOWN		15
#define LAST_DRUG_TO_SEND		16
#define ERR_NO_MEMORY				17

#define SB_UNABLE_TO_SEND_RECORD	30
#define SB_LOCAL_DB_ERROR		31
#define SB_SYSTEM_IS_BUSY		32
#define SB_BATCH_SUCCESSFULLY_PASSED	33
#define SB_FATAL_ERROR			34
#define SB_RESTART_CONNECTION		35
#define SB_END_OF_FETCH			36
#define SB_DATA_ACCESS_CONFLICT		37
#define SB_END_OF_FETCH_NOCOMMIT 	38

#define RR_UNABLE_TO_ROLLBACK		91
#define RR_ROLLBACK_DONE		92	

#define COMMIT_DONE						60
#define COMMIT_FAILED_ROLLBACK_DONE		61
#define COMMIT_FAILED_ROLLBACK_FAILED	62
#define COMMIT_DISCREPANCY_ROLLEDBACK	63
// DonR December 2002: Added new status for discrepancy handling.

#define CM_NO_MESSAGE_PASSED		-17
#define CM_RESTART_CONNECTION		-18

#define MAX_ERRORS_FOR_SINGLE_TABLE	3
// DonR December 2002: Changed max errors from one to three - to allow more graceful
// recovery from DB contention errors.

// #define MAX_SELECT_WAIT_TIME_IN_SECS	1200
#define MAX_SELECT_WAIT_TIME_IN_SECS	120

#define NO_WAIT_MODE			0
#define NO_FLAGS			0

#define RC_RESTART_CONNECTION		120
#define RC_FATAL_ERROR_OCCURED		121
#define RC_FATHER_GOING_DOWN		122

/* todo */
#define MAX_SEND_RETRIES		3

/* todo */
#define MAX_CONNECT_RETRIES		6

/* todo */
#define MAX_ALLOWED_ERRORS_MSG_2502	10

/* todo */
#define MAX_ALLOWED_ERRORS_MSG_2513	10

#define CL_DIMENSION_OF_BATCH           50

#define CL_DIMENSION_OF_BATCH_DOC       100

DEF_TYPE	
    int SLEEP_BETWEEN_TABLES_NIGTH 	/* sleep between tables to as400 night*/
    DEF_INIT(5),
    SLEEP_BETWEEN_TABLES_DAY 	/* sleep between tables to as400 day*/
    DEF_INIT(30);


/*
 *  the two macro's bellow will immediatley return from function with
 *  an error-code which will cause program to terminate in case of failure
 *  to do rollback:
 */
#define RETURN_ON_FATAL_ROLLBACK_ERR( p ) \
    if( (p) != RR_ROLLBACK_DONE ) return MH_FATAL_ERROR;
	  
#define RETURN_ON_FATAL_COMMIT_ERR( p ) \
    if( (p) == COMMIT_FAILED_ROLLBACK_FAILED ) return MH_FATAL_ERROR;

/* << -------------------------------------------------------- >> */
/*		      ROW_COMPARE( a, b )			  */
/*								  */
/*		if a > b   -  return 1 				  */
/*		if a < b   -  return -1 			  */
/*		if a == b  -  return 0 				  */
/*								  */
/* << -------------------------------------------------------- >> */
#define ROW_COMPARE( a, b ) ( (a) > (b) ? 1 : (a) < (b) ? -1 : 0 )
/* << -------------------------------------------------------- >> */
/*				globals                           */
/* << -------------------------------------------------------- >> */
char		glbMsgBuffer[MAX_RECORD_LEN];

#ifdef _LINUX_
	fd_set	glbFDBitMask;
#else
	Tfd_set	glbFDBitMask;
#endif

TSocket		glbSocket;
TABLE		*glbStateTable;
int             glbInTransaction;


typedef struct
{
	Tpharmacy_code			v_pharmacy_code;
	Tinstitued_code			v_institued_code;
	Tterminal_id			v_terminal_id;
	Tprice_list_code		v_price_list_code;
	Tmember_institued		v_member_institued;
	Tmember_id				v_member_id;
	Tmem_id_extension		v_mem_id_extension;
	Tcard_date				v_card_date;
	Tdoctor_id_type			v_doctor_id_type;
	Tdoctor_id				v_doctor_id;
	Tdoctor_insert_mode		v_doctor_insert_mode;
	Tpresc_source			v_presc_source;
	Treceipt_num			v_receipt_num;
	Tuser_ident				v_user_ident;
	Tdoctor_presc_id		v_doctor_presc_id;
	Tpresc_pharmacy_id		v_presc_pharmacy_id;
	Tprescription_id		v_prescription_id;
	Tprice_code				v_member_price_code;
	Tmember_discount_pt		v_member_discount_pt;
	Tcredit_type_code		v_credit_type_code;
	Tcredit_type_code		v_credit_type_used;
	Tmacabi_code			v_macabi_code;
	Tmacabi_centers_num		v_macabi_centers_num;
	Tmacabi_centers_num		v_doc_device_code;
	int						v_reason_for_discnt;	// Add typedef (or not) eventually.
	short int				v_reason_to_disp;		// Add typedef (or not) eventually.
	Tdate					v_date;
	Ttime					v_time;
	Tspecial_presc_num		v_special_presc_num;
	Tspec_presc_num_sors	v_spec_pres_num_sors;
	Tdiary_month_1			v_diary_month;
	Terror_code				v_error_code;
	Tcomm_error_code		v_comm_error_code;
	Tprescription_lines		v_prescription_lines;
	long					v_rowid;	// DonR 03Sep2020 MS-SQL compatibility. I don't think this rowid is actually used anywhere.
	short					action_type;	// This and following fields for "Nihul Tikrot", 17Jun2010.
	int						del_presc_id;
	int						del_sale_date;
	int						card_owner_id;
	short					card_owner_id_code;
	short					num_payments;
	short					payment_method;
	short					credit_reason_code;
	short					tikra_called_flag;
	short					tikra_status_code;
	int						tikra_discount;
	int						subsidy_amount;
	short					sale_req_error;			// DonR 03Apr2011.
	char					insurance_type[2];		// DonR 17Dec2012 "Yahalom".
	short					member_insurance;		// DonR 17Dec2012 "Yahalom" - translated version of insurance_type for AS/400.
	short					OriginCode;				// DonR 09May2013.
	long					online_order_num;		// DonR 01Jun2021 User Story #144324 - pass Online Order Number to RK9021P/EAZZZR.
	short					darkonai_plus_sale;		// DonR 09Aug2021 User Story #163882
	short					paid_for;				// DonR 27Apr2023 User Story #432608: Not sent to AS/400, at least for now.
	long					MagentoOrderNum;		// DonR 27Apr2023 User Story #432608
}
TMsg2502Record;	/* table PRESCRIPTIONS */

typedef struct
{
	Tpharmacy_code			v_pharmacy_code;
	Tinstitued_code			v_institued_code;
	Tprescription_id		v_prescription_id;
	Tlargo_code				v_largo_code;
	int						v_op;
	int						v_units;
	Tduration				v_duration;
	Tprice_for_op			v_price_for_op;
	Top_stock				v_op_stock;
	Tunits_stock			v_units_stock;
	Ttotal_member_price		v_total_member_price;
	Tline_no				v_line_no;
	Tcomm_error_code		v_comm_error_code;
	Tdrug_answer_code		v_drug_answer_code;
	Ttotal_drug_price		v_total_drug_price;
	Tstop_use_date			v_stop_use_date; 
	Tstop_blood_date		v_stop_blood_date;
	Tdel_flg				v_del_flg;
	Tprice_replace			v_price_replace;
	Tprice_extension		v_price_extension;
	Tlink_ext_to_drug		v_link_ext_to_drug;
	Tmacabi_price_flg		v_macabi_price_flg;
	Tcalc_member_price		v_calc_member_price;
	Tdate					v_date;
	Ttime					v_time;
	int						v_dr_presc_id;
	int						v_dr_presc_date;
	short					IsDigital;
	short					v_particip_meth;
	int						v_prw_rrn;
	int						v_dr_largo_code;
	short					v_subst_permitted;
	int						v_units_unsold;
	int						v_op_unsold;
	int						v_units_per_dose;
	int						v_doses_per_day;
	int						v_member_discount_pt;
	int						member_discount_amt;
	long					v_rowid;		// DonR 03Sep2020 MS-SQL compatibility. I don't think this rowid is actually used anywhere.
	Tspecial_presc_num		v_special_presc_num;
	Tspec_presc_num_sors	v_spec_pres_num_sors;
	Tsupplier_price			v_supplier_price;
	int						v_updated_by;	// DonR 25Dec2008: This is where the health-basket status is stored.
	int						rule_number;		// This and following fields for "Nihul Tikrot", 17Jun2010.
	char					tikra_type_code;
	short					subsidized;
	short					discount_code;
	short					why_future_sale_ok;		// DonR 18Jun2013.
	short					qty_limit_chk_type;		// DonR 18Jun2013.
	int						doctor_id;
	short					doctor_id_type;
	short					presc_source;
	int						doctor_facility;
	short					elect_pr_status;
	short					use_instr_template;
	short					how_to_take_code;
	char					unit_code			[ 4];
	short					course_treat_days;
	short					course_length;
	char					course_units		[11];
	char					days_of_week		[21];
	char					times_of_day		[201];	// DonR 21Mar2023 User Story #432608: changed length from 40 to 200.
	char					side_of_body		[11];
	char					UsageInstrGiven		[101];	// DonR 21Mar2023 User Story #432608.
	short					use_instr_changed;
	short					num_linked_rxes;
	short					BarcodeScanned;
	int						member_diagnosis;		// DonR 03Oct2018 CR #13262.
	int						ph_OTC_unit_price;		// DonR 16Oct2018.
	int						voucher_amount_used;	// DonR 18Aug2021.
	short					qty_limit_class_code;	// DonR 03Nov2022.
	int						MaccabiOtcPrice;		// DonR 21Mar2023 User Story #432608.
	int						SalePkgPrice;			// DonR 21Mar2023 User Story #432608.
	int						SaleNum4Price;			// DonR 21Mar2023 User Story #432608.
	int						SaleNumBuy1Get1;		// DonR 21Mar2023 User Story #432608.
	int						Buy1Get1Savings;		// DonR 21Mar2023 User Story #432608.
	int						ByHandReduction;		// DonR 21Mar2023 User Story #432608.
	short					IsConsignment;			// DonR 21Mar2023 User Story #432608.
	short					NumCourses;				// DonR 21Mar2023 User Story #432608.
	int						alternate_yarpa_price;	// Marianna 29Jun2023 User Story #461368.
	int						blood_start_date;		// Marianna 10Aug2023 User Story #469361.
	short					blood_duration;			// Marianna 10Aug2023 User Story #469361.
	int						blood_last_date;		// Marianna 10Aug2023 User Story #469361.
	short					blood_data_calculated;	// Marianna 10Aug2023 User Story #469361.
	int						NumCannabisProducts;	// Marianna 19Feb2024 User Story #540234.
}
TMsg2506Record;	/* table PRESCRIPTION_DRUGS TDate->Tdate */	



/* << -------------------------------------------------------- >> */
/*                     function prototypes                        */
/* << -------------------------------------------------------- >> */
static int		send_table_sequence_to_remote_DB(
							 void
							 );
static int		pass_record_batch_2001(
					       void
					       );

static int		rollback_remote_and_local_DBs(
						      void
						      );

static int		commit_remote_and_local_DBs(
						    void
						    );

static TSuccessStat	send_a_msg_to_server(
					     int	msg
					     );

static TSuccessStat	send_msg_and_get_confirmation(
						      int	msg,
						      int	confirmation
						      );

static void		sleep_remaining_time(
					     time_t   start_time
					     );

static int		get_msg_from_server(
					    int seconds_to_wait
					    );

static TSuccessStat	initialize_connection_to_server(
							void
							);

static int		call_message_handler(
					     int	msgID
					     );

static int		pass_table_to_server(
					     TFuncPtr  pass_a_record_batch
					     );

static int		get_dimension_of_batch(
					       int msgID
					       );

static void		connect_to_local_database(
						  void
						  );

static void		terminate_communication_and_DB_connections(
								   void
								   );

static TSuccessStat	send_buffer_to_server(
					      TSocket	sock,
					      const char	*buf,
					      int		len
					      );

static int		get_message_by_index(
					     int index
					     );

static void		get_system_status(
					 bool *a,
					 bool *b
					 );

static int		init_son_process(
					 int		l_argc,
					 char	*l_argv[],
					 int		*pRetrys,
					 int		typ,
					 TABLE	**stat_tablepPtr,
					 TABLE	**proc_tablepPtr
					 );

static bool		a_legal_message_ID(
					   int msgID
					   );

static bool		is_a_legal_longint(
					   long	num,
					   int	len
					   );

static bool		is_a_legal_shortint(
					    short	num,
					    int		len
					    );
					    
static int get_server_response (int wait_time,
								int MsgLen,
								int *Param1,
								int *Param2,
								int *Param3,
								int *Param4);

static int ProcessQueuedRxUpdates (void);


/* << -------------------------------------------------------- >> */
/*                     msg handler prototypes			  */
/* << -------------------------------------------------------- >> */

#ifdef DOCTORS_SRC
static int		pass_record_batch_3501( void);
static int		pass_record_batch_3502( void);
static int		pass_record_batch_3503( void);
static int		pass_record_batch_3504( void);
static int		pass_record_batch_3505( void);
static int		pass_record_batch_3506( void);
static int		pass_record_batch_3507( void);
static int		pass_record_batch_3508( void);
static int		pass_record_batch_3509( void);
static int		pass_record_batch_3510( void);
static int		pass_record_batch_3511( void);
static int		pass_record_batch_3512( void);
static int		pass_record_batch_3513( void);
static int		pass_record_batch_3514( void);
static int		pass_record_batch_3515( void);
static int		pass_record_batch_3516( void);
static int		pass_record_batch_3517( void);
static int		pass_record_batch_3518( void);
#else //DOCTORS_SRC

static int		pass_record_batch_2501( void );
static int		pass_record_batch_2503( void );
static int		pass_record_batch_2502_2506( void );
static int		pass_a_single_record_2502(TMsg2502Record * );
static int		pass_a_single_record_2506(TMsg2506Record * );
//static TSuccessStat	pass_eq_key_records_2506(
//				/*oracle		 const TMsg2506Record *, */
//						 TMsg2506Record *,
//						 Tprescription_id,
//						 TDate,
//						 Ttime
//						 );
static int		pass_record_batch_2504( void );
static int		pass_record_batch_2505( void );
static int		pass_record_batch_2507( void );
static int		pass_record_batch_2508( void );
static int		pass_record_batch_2509( void );
static int		pass_record_batch_2512( void );
static int		pass_record_batch_2513_2514( void );
//static int		pass_a_single_record_2513( const TMsg2502Record * );
//static int		pass_eq_key_records_2514(
/*	oracle               const TMsg2514Record *,*/
//						 TMsg2506Record *,
//						 Tprescription_id,
//						 TDate,
//						 Ttime
//						 );
static int		pass_record_batch_2515( void );
static int		pass_record_batch_2516( void );
static int		pass_record_batch_2517( void );

#endif //DOCTORS_SRC
