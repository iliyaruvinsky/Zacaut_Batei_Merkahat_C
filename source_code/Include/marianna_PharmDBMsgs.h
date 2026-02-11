/* << -------------------------------------------------------- >> */
/*			      PharmDBMsgs.h  		                          */
/*                                                           	  */
/* Include AFTER DBFields.h; gives message structures for         */
/* As400UnixClient and As400UnixServer                            */
/*                                                           	  */
/* << -------------------------------------------------------- >> */


#if !defined( __PHARMDBMSGS__H_ )
#define __PHARMDBMSGS__H_

#ifndef DOCTORS_SRC

// M E S S A G E     R E C O R D S
/*        Declaring data structurs used in FETCH command          */
/*		     in messages 2502 & 2504			  */
/* << -------------------------------------------------------- >> */

#if 0
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
	TCCharRowid20			v_rowid;   //ORACLE 2000.05.22
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
	int						v_rowid;
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
	char					times_of_day		[41];
	char					side_of_body		[11];
	short					use_instr_changed;
	short					num_linked_rxes;
	short					BarcodeScanned;
	int						member_diagnosis;		// DonR 03Oct2018 CR #13262.
	int						ph_OTC_unit_price;		// DonR 16Oct2018.
}
TMsg2506Record;	/* table PRESCRIPTION_DRUGS TDate->Tdate */	
#endif


typedef struct
{
	Tprescription_id		v_prescription_id;
	Tlargo_code				v_largo_code;
	Tline_no				v_line_no;
	Tdrug_answer_code		v_error_code;
	short					v_severity_level;
}
TMsg2522Record;	// Table prescription_msgs.

typedef struct
{
	Tprescription_id		prescription_id;
	Tline_no				line_no;
	long					visit_number;
	short					clicks_line_number;
	int						doctor_presc_id;
	Tlargo_code				largo_prescribed;
	Tlargo_code				largo_sold;
	int						valid_from_date;
	int						prev_unsold_op;
	int						prev_unsold_units;
	int						op_sold;
	int						units_sold;
	int						sold_drug_op;
	int						sold_drug_units;
	short					close_by_rounding;
	short					rx_sold_status;
}
TMsg2523Record;	// Table pd_rx_link.


/* << -------------------------------------------------------- >> */
/*                                                           	  */
/*    Section:		Length of record			  */
/*                                                           	  */
/* << -------------------------------------------------------- >> */

#define RECORD_LEN_2001				\
     (Tpharmacy_code_len +  			\
      Tinstitued_code_len + 			\
      Tpharmacy_name_len + 			\
      Tpharmacist_last_na_len + 		\
      Tpharmacist_first_n_len + 		\
      Tstreet_and_no_len + 			\
      Tcity_len + 				\
      Tphone_len + 				\
      Towner_len + 				\
      Tcomm_type_len + 				\
      Tphone_1_len + 				\
      Tphone_2_len + 				\
      Tphone_3_len + 				\
      Tpermission_type_len + 			\
      Tprice_list_code_len	+	\
	  1                     +	\
	  Tfee_len              +	\
	  Tfee_len              +	\
	  Tfee_len				+	\
	  Tpharm_credit_flag_len	+	\
		1					+	\
		1					+	\
		5					+	\
		8					+	\
		1					+ /* MaccabiCare flag */		\
		2					+ /* Pharmacy Category */		\
		1					+ /* Future Sales Enabled */	\
		1					  /* Meishar Enabled */			\
	  ) /* end of record 2001 */
// DonR 16Sep2003 - added Credit Flag to Pharmacy download.

#define RECORD_LEN_2002				\
     (Tpharmacy_code_len)

#define RECORD_LEN_2003				\
     (Tpharmacy_code_len +			\
      Tinstitued_code_len +			\
      Tmember_id_len +			 	\
      Tmem_id_extension_len +			\
      Tdoctor_id_type_len +			\
      Tdoctor_id_len +				\
      Tspecial_presc_num_len +			\
      Tspecial_presc_ext_len +			\
      Tprice_code_len +				\
      Tlargo_code_len +				\
      Top_in_dose_len + 			\
      Tunits_in_dose_len +			\
      Tstart_use_date_len +			\
      Tstop_use_date_len +			\
      Tauthority_id_len +			\
      Tconfirm_authority__len +			\
      Tpresc_source_len +			\
      Tconfirm_reason_len +			\
      Tdose_num_len +				\
      Tdose_renew_freq_len +			\
      Tpriv_pharm_sale_ok_len +			\
      Tconfirmation_type_len +			\
      Thospital_code_len	+ \
	  Tconst_sum_to_pay_len +\
      Tin_health_pack_len	+	\
	  Ttreatment_category_len	+	\
	  Tcountry_center_len		+	\
      Tmess_code_len			+	\
	  Treason_mess_len			+	\
	  Tmess_text_len			+	\
      Tmade_date_len			+	\
	  TGenericFlag_len			+	/* doc_has_seen_ishur	*/	\
	  TGenericFlag_len			+	/* abrupt_close_flg		*/	\
	  4							+	/* v_QtyLim_ingr		*/	\
	  TGenericFlag_len			+	/* v_QtyLim_flg			*/	\
	  3							+	/* v_QtyLim_units		*/	\
	  20						+	/* v_QtyLim_qty_each	*/	\
	  20						+	/* v_QtyLim_per_day		*/	\
	  2							+	/* v_QtyLim_treat_days	*/	\
	  3							+	/* v_QtyLim_course_len	*/	\
	  2							+	/* v_QtyLim_courses		*/	\
	  20						+	/* v_QtyLim_all_ishur	*/	\
	  20						+	/* v_QtyLim_all_warn	*/	\
	  20						+	/* v_QtyLim_all_err		*/	\
	  20						+	/* v_QtyLim_per_course	*/	\
	  20						+	/* v_QtyLim_course_wrn	*/	\
	  20						+	/* v_QtyLim_course_err	*/	\
	   3						+	/* v_form_number		*/	\
	   1						+	/* v_tikra_flag			*/	\
	   3						+	/* v_tikra_type_code	*/	\
	   8						+	/* v_portal_update_dt	*/	\
	   8						+	/* qlm_history_start	*/	\
	   2						+	/* needs_29_g			*/	\
	   6						+	/* member_diagnosis		*/	\
	/* Marianna 14Jul2020 CR #27955 */							\
	   1						+	/* monthly_qlm_flag		*/	\
	  20						+	/* monthly_dosage		*/	\
	   2						+	/* monthly_duration		*/	\
	/* Marianna 04Aug2020 CR #28605	*/							\
	   1						+	/* cont_approval_flag	*/	\
	   8							/* orig_confirm_num		*/	\
	 )	/* end of rec 2003 */
  

#define RECORD_LEN_2004				\
     (Tmember_id_len +				\
      Tmem_id_extension_len +			\
      Tlargo_code_len +				\
      Tspecial_presc_num_len +			\
      Tspecial_presc_ext_len)  /* end of rec 2004 */

#define RECORD_LEN_4005				\
     (Tlargo_code_len +				\
      Tpriv_pharm_sale_ok_len +		\
      Tlargo_type_len +				\
      Tlong_name_len +				\
      Tpackage_len +				\
      Tpackage_size_len +			\
      Tdrug_book_flg_len +			\
      Tprice_code_len	/* Basic */			+	\
      Tprice_code_len	/* Prv. Basic */	+	\
      Tt_half_len +					\
      Tcronic_months_len +			\
      Tsupplemental_1_len +			\
      Tsupplemental_2_len +			\
      Tsupplemental_3_len +			\
      Tsupplier_code_len +			\
      Tcomputersoft_code_len +		\
      Tadditional_price_len +		\
      Tno_presc_sale_flag_len +     \
      Tdel_flg_len +                \
      Tin_health_pack_len+			\
	  Tpharm_group_code_len+		\
	  Tshort_name_len +				\
      Tform_name_len  +				\
	  Tprice_prcnt_len+				\
	  Tprice_prcnt_len+				\
	  Tprice_prcnt_len+				\
      Tdrug_type_len				+	\
      Tpharm_sale_new_len			+	\
	  Tstatus_send_len				+	\
      Tsale_price_len				+	\
	  Tinteract_flg_len				+	\
	  Twait_months_len				+	\
	  Twait_months_len				+	\
	  Tdrug_book_flg_len			+	\
	  Topenable_package_len			+	\
	  Tishur_required_len			+	\
	  12 /* ingr code x 3 */		+	\
	  60 /* ingr quant x 3 */		+	\
	  9  /* ingr units x 3 */		+	\
	  60 /* per quant x 3 */		+	\
	  9  /* per units x 3 */		+	\
	  1  /* pharm_sale_test */		+	\
	  1  /* v_rule_status */		+	\
	  1  /* v_sensitivity_flag */	+	\
	  5  /* v_sensitivity_code */	+	\
	  1  /* v_sensitivity_severe */	+	\
	  1  /* v_needs_29_g */			+	\
	  1  /* v_chronic_flag */		+	\
	  3  /* v_shape_code */			+	\
	  3  /* v_shape_code_new */		+	\
	  1  /* v_split_pill_flag */	+	\
	  2  /* v_treatment_typ_cod */	+	\
	  1  /* v_print_generic_flg */	+	\
	  5  /* v_substitute_drug */	+	\
	  20 /* package_volume */		+	\
	  2					/* copies_to_print */	+	\
	  TGenericFlag_len	/* preferred_flg */		+	\
	  5					/* parent_group_code */	+	\
	  25				/* parent_group_name */	+	\
	  1					/* v_pharm_real_time */	+	\
      Tsale_price_len	/* Strudels */			+	\
	  TGenericFlag_len	/* v_IshurQtyLimFlg	*/	+	\
	  1					/* tight_ishur_limits	*/	+	\
	  4					/* v_IshurQtyLimIngr */	+	\
	  1					/* MaccabiCare flag */	+	\
	  Tname_25_len		/* name_25			*/	+	\
	  5					/* FPS Group Code   */	+	\
	  1					/* Assuta Drug Status */	+	\
	  1					/* Pkg. Expiry Dt. Flag */	+	\
	  13				/* Bar-Code Value */		+	\
	  1					/* Tikrat Mazon Flag */		+	\
	  5					/* Class Code */			+	\
	  2					/* Enabled Flag */			+	\
	  Tprice_prcnt_len	/* Yahalom % */				+	\
	  Twait_months_len	/* Yahalom Wait MM */		+	\
	  2					/* allow_future_sales */	+	\
	  1					/* cont_treatment     */	+	\
	  1					/* time_of_day_taken  */	+	\
	  3					/* treatment_days	  */	+	\
	  2					/* for_cystic_fibro	  */	+	\
	  2					/* for_tuberculosis	  */	+	\
	  2					/* for_gaucher		  */	+	\
	  2					/* for_aids			  */	+	\
	  2					/* for_dialysis		  */	+	\
	  2					/* for_thalassemia	  */	+	\
	  2					/* for_hemophilia	  */	+	\
	  2					/* for_cancer		  */	+	\
	  2					/* for_car_accident   */	+	\
	  2					/* for_reserved_1	  */	+	\
	  2					/* for_work_accident  */	+	\
	  2					/* for_reserved_3	  */	+	\
	  2					/* for_reserved_99	  */	+	\
	  3					/* how_to_take_code	  */	+	\
	  3					/* unit_abbreviation  */		\
	) /* end of rec 2005 */


#define RECORD_LEN_2006				\
     (Tlargo_code_len) 


#define RECORD_LEN_2007				\
     (Tlargo_code_len +				\
      Tfrom_age_len +				\
      Tmax_units_per_day_len) /* end of rec 2007 */
  
#define RECORD_LEN_2008				\
     (Tlargo_code_len) 

#define RECORD_LEN_2009				\
     (Tinteraction_type_len +			\
      Tdur_message_len +			\
      Tinteraction_level_len)  /* end of rec 2009 */
  
#define RECORD_LEN_2010				\
     (Tinteraction_type_len) 

#define RECORD_LEN_2011				\
     (Tfirst_largo_code_len +			\
      Tsecond_largo_code_len +			\
      Tinteraction_type_len  +			\
      Tdur_message_len +			\
      Tdur_message_len) 

#define RECORD_LEN_2012				\
     (Tfirst_largo_code_len +			\
      Tsecond_largo_code_len) 

#define RECORD_LEN_2013				\
     (Tdoctor_id_len +				\
      Tfirst_name_len +				\
      Tlast_name_len +				\
      Tstreet_and_no_len +			\
      Tcity_len +					\
	  8  /* License # */		+	\
	  40 /* Contact Phone # */	+	\
      Tphone_len)  /* end of rec 2013 */

#define RECORD_LEN_2014				\
     (Tdoctor_id_len)

#define RECORD_LEN_2015				\
     (Tspeciality_code_len +			\
      Tdescription_len)

#define RECORD_LEN_2017				\
     (Tdoctor_id_len +				\
      Tspeciality_code_len)

#define RECORD_LEN_2018	 	RECORD_LEN_2017			

#define RECORD_LEN_2019				\
     (Tspeciality_code_len +			\
      Tlargo_code_len +				\
      Tprice_code_len	/* Basic */			+	\
      Tprice_code_len	/* Prv. Basic */	+	\
      Tprice_code_len	/* Zahav */			+	\
      Tprice_code_len	/* Prv. Zahav */	+	\
      Tprice_code_len	/* Kesef */			+	\
      Tprice_code_len	/* Prv. Kesef */	+	\
      Tfixed_price_len	/* Basic */			+	\
      Tfixed_price_len	/* Kesef */			+	\
      Tfixed_price_len	/* Zahav */			+	\
	  2					/* Enabled Flag */	+	\
	  TGenericFlag_len	/* In_health_basket */		+	\
	  Tprice_code_len	/* Yahalom Prc. Code */		+	\
	  Tfixed_price_len	/* Yahalom  Fixed Prc. */	+	\
	  Twait_months_len	/* Wait Months */				\
	)	
	/* end of rec 2019 */ 
      

// DonR 02Jan2012: HOT FIX - added 2 bytes for Enabled Status.
// DonR 02Jan2012: Another hot fix - added  price codes to determine Insurance Type.
#define RECORD_LEN_2020					\
     (Tspeciality_code_len	+			\
	  2						+			\
      Tprice_code_len			/* Basic */			+	\
      Tprice_code_len			/* Zahav */			+	\
      Tprice_code_len			/* Kesef */			+	\
      Tprice_code_len			/* Yahalom */		+	\
      Tlargo_code_len)

#define RECORD_LEN_2021				\
     (Tdoctor_id_len +				\
      Tlargo_code_len +				\
      Tprice_code_len			/* Basic */			+	\
      Tprice_code_len			/* Prv. Basic */	+	\
      Tprice_code_len			/* Zahav */			+	\
      Tprice_code_len			/* Prv. Zahav */	+	\
      Tprice_code_len			/* Kesef */			+	\
      Tprice_code_len			/* Prv. Kesef */	+	\
	  Tdoct_number_len	+	\
	  Ttreatment_category_len) /* end of rec 2021 */ //20020819

#define RECORD_LEN_2022				\
     (Tdoctor_id_len +				\
      Tlargo_code_len)

#define RECORD_LEN_2025				\
     (Tmember_id_len +				\
      Tmem_id_extension_len +			\
      Tcard_date_len +				\
      Tfather_name_len +			\
      Tfirst_name_len +				\
      Tlast_name_len +				\
      Tstreet_len +				\
      Thouse_num_len +				\
      Tcity_len +				\
      Tphone_len +				\
      Tzip_code_len +				\
      Tsex_len +				\
      Tdate_of_bearth_len +			\
      Tmarital_status_len +			\
      Tmaccabi_code_len +			\
      Tmaccabi_until_len +			\
      Tmac_magen_code_len +			\
      Tmac_magen_from_len +			\
      Tmac_magen_until_len +			\
      Tkeren_macabi_len +			\
      Tkeren_mac_from_len +			\
      Tkeren_mac_until_len +			\
      Tasaf_code_len +				\
      Tinsurance_status_len +			\
      Tdoctor_status_len +			\
      Tcredit_type_code_len +			\
      Tmember_discount_pt_len +			\
      Told_member_id_len +			\
      Told_id_extension_len+		\
	  Tkeren_wait_flag_len +		\
      Told_member_id_len +			\
      Told_id_extension_len+		\
	  Tdate_of_bearth_len+			\
	  Tenglish_name_len+			\
	  Tenglish_name_len	+			\
	  9+							\
	  9	+							\
	  1	+ /* DonR 16Mar2010: has_tikra */	\
	  Tmaccabi_code_len	+ /* Yahalom */		\
	  Tdate_len			+ /* Yahalom */		\
	  Tdate_len			+ /* Yahalom */		\
	  Tdate_len			+ /* Yahalom */		\
	  20				  /* Illness Codes 1-10 */	\
	  ) /* end of rec 2025 */

#define RECORD_LEN_2026				\
     (Tmember_id_len +				\
      Tmem_id_extension_len)


#define	RECORD_LEN_4029	/* member_drug_29g */	\
		(	9	/* v_member_id			*/	+	\
			1	/* v_mem_id_extension	*/	+	\
			9	/* v_largo_code			*/	+	\
			8	/* v_start_date			*/	+	\
			8	/* v_end_date			*/	+	\
			7	/* v_pharmacy_code		*/	+	\
			2	/* v_form_29g_type		*/		\
		)	/* End of rec 2029/4029. */

//change order for 4 last field and add Tfixed_price_len for magen 20020807
// DonR 29Dec2011: Added one more fixed price, for Magen Zahav.
#define RECORD_LEN_2030					\
     (Tlargo_code_len			+		\
      Textension_largo_co_len	+		\
	  Trule_number_len			+		\
	  Ttreatment_category_len	+		\
	  TGenericYYYYMMDD_len		+		\
	  Trule_name_len			+		\
      Tconfirm_authority__len	+		\
      Tfrom_age_len				+		\
      Tto_age_len				+		\
	  TGenericFlag_len			+		\
      Tprice_code_len			/* Basic */			+	\
      Tprice_code_len			/* Prv. Basic */	+	\
      Tprice_code_len			/* Zahav */			+	\
      Tprice_code_len			/* Prv. Zahav */	+	\
      Tprice_code_len			/* Kesef */			+	\
      Tprice_code_len			/* Prv. Kesef */	+	\
      Tmust_confirm_deliv_len	+		\
      Tmax_op_len				+		\
      Tmax_units_len			+		\
	  Tmax_amount_duratio_len	+		\
	  TPermissionType_len		+		\
	  Tno_presc_sale_flag_len	+		\
	  Tfixed_price_len			+		\
      Tfixed_price_len			+		\
      Tfixed_price_len			+		\
      Textend_rule_days_len		+		\
	  Tivf_flag_len				+		\
	  TGenericFlag_len			+		\
	  TGenericFlag_len			+		\
	  2		/* Enabled Flag */	+		\
	  Tprice_code_len	/* Yahalom Prc Code */	+	\
	  Tfixed_price_len	/* Yahalom Fixed Prc */	+	\
	  Twait_months_len	/* Wait Months */			\
	  )  /* end of rec 2030 */


#define RECORD_LEN_2031				\
     (Tprice_list_code_len +		\
      Tlargo_code_len +				\
      Tmacabi_price_len +			\
      Tyarpa_price_len +			\
      Tsupplier_price_len +			\
      Tdate_yarpa_len +				\
      Ttime_yarpa_len +				\
      Tdate_macabi_len +			\
      Ttime_macabi_len +			\
	  Tsupplier_price_len ) /*20021111 Yulia*//* end of rec 2031 */

#define RECORD_LEN_2032				\
     (Tprice_code_len		+			\
      Tmember_price_prcnt_len +			\
      Ttax_len +				\
      Tparticipation_name_len +			\
      Tcalc_type_code_len +			\
	  Tmax_price_len		+	\
      Tmember_institued_len +			\
      Tyarpa_part_code_len) /* end of rec 2032 */

#define RECORD_LEN_2035				\
     (Tsupplier_table_cod_len +			\
      Tsupplier_type_len +			\
      Tsupplier_code_len +			\
      Tsupplier_name_len +			\
      Tstreet_and_no_len +			\
      Tcity_len +				\
      Tzip_code_len +				\
      Tphone_1_len +				\
      Tphone_2_len +				\
      Tfax_num_len +				\
      Tcomm_supplier_len +			\
      Temployee_id_len +			\
      Tallowed_proc_list_len +			\
      Temployee_password_len +			\
      Tcheck_type_len +				\
	  Tdel_flg_len) /* end of rec 2035 */ //last field 20020128

#define RECORD_LEN_2036				\
     (Tmacabi_centers_num_len +			\
      Tfirst_center_type_len +			\
      Tmacabi_centers_nam_len +			\
      Tstreet_and_no_len +			\
      Tcity_len +				\
      Tzip_code_len +				\
      Tphone_1_len +				\
      Tphone_2_len +				\
      Tfax_num_len +				\
      Tdiscount_percent_len +			\
      Tcredit_flag_len +			\
      Tallowed_proc_list_len +			\
      Tallowed_belongings_len ) /* end of rec 2036 */ 

#define RECORD_LEN_2037				\
     (Tmacabi_centers_num_len)

#define RECORD_LEN_2038				\
     (Tsupplier_table_cod_len + 		\
      Tsupplier_code_len) 

#define RECORD_LEN_2039				\
     (Tpharmacist_id_len +			\
      Tpharmacist_id_exte_len)

#define RECORD_LEN_2040				\
     (Trefund_id_len)

#define RECORD_LEN_2041				\
     (Tprice_code_len)

#define RECORD_LEN_2042				\
     (Tprice_list_code_len +			\
      Tlargo_code_len)

#define RECORD_LEN_2043				\
     (Tlargo_code_len	+			\
	  Trule_number_len)

#define RECORD_LEN_2044				\
       (Tmember_id_len +			\
	Tmember_id_len +			\
	Tmem_id_extension_len +			\
	Tdate_len) /* end of rec 2044 */

#define RECORD_LEN_2045				\
     (Tmember_id_len)


#define RECORD_LEN_2048	\
    (Tgen_comp_code_len	+		\
	Tgen_comp_desc_len	+		\
	Tdel_flg_c_len) /* end of rec 2048 GeneryComponent */

#define RECORD_LEN_2049			\
    (Tlargo_code_len	+		\
	 Tgen_comp_code_len	+		\
	 Tdel_flg_c_len) /* end of rec 2049 DrugGenery */

#define RECORD_LEN_2050					\
    (Tgen_comp_code_len		+			\
	 Tgen_comp_code_len		+			\
	 Tinteraction_type_len	+			\
	 Tinter_note_code_len	+			\
	 Tdel_flg_c_len) /* end of rec 2050 GeneryInteraction */

#define RECORD_LEN_2051			\
    (Tgnrldrugnotetype_len	+	\
 	 Tgnrldrugnotecode_len	+	\
	 Tgnrldrugnote_len		+	\
	 Tgdn_type_new_len		+	\
	 Tgdn_long_note_len		+	\
	 Tgdn_category_len		+	\
	 Tgdn_sex_len			+	\
	 Tgdn_from_age_len		+	\
	 Tgdn_to_age_len		+	\
	 Tgdn_seq_num_len		+	\
	 Tgdn_connect_type_len	+	\
	 Tgdn_connect_desc_len	+	\
	 Tgdn_severity_len		+	\
	 Tgdn_sensitv_type_len	+	\
	 Tgdn_sensitv_desc_len	+	\
	 Tdel_flg_c_len) /* end of rec 2051 GeneryNote */

#define RECORD_LEN_2052					\
    (Tlargo_code_len		+			\
	 Tgnrldrugnotetype_len			+			\
 	 Tgnrldrugnotecode_len			+			\
	 Tdel_flg_c_len) /* end of rec 2052 DrugNote */

#define RECORD_LEN_2055					\
    (Tpharm_group_code_len		+			\
	 Tpharm_group_name_len		+			\
	 Tdel_flg_c_len) /* end of rec 2055 PharmacologyGroup */

#define RECORD_LEN_2056					\
    (Tinter_note_code_len	+			\
	 Tinteraction_note_len	+			\
	 100 /* inter_long_note */	+		\
	 Tdel_flg_c_len) /* end of rec 2056 GenInterNotes */

#define RECORD_LEN_2061	0

#define RECORD_LEN_2062					\
    (5	+			\
	 8	+			\
	 8	+			\
	 11	+			\
	 11	+			\
	 11	+			\
	 5  + /* FPS Group Code */	\
	 15   /* FPS Group Name */	\
	) /* end of rec 2062 Pharmacy Contract */

#define RECORD_LEN_2063	0

#define RECORD_LEN_2064					\
    (2	+			\
	 15	+			\
	 1	+			\
	 1	+			\
	 1) /* end of rec 2064 Prescription Source */

#define RECORD_LEN_2065	0

#define RECORD_LEN_2066					\
    (2	+			\
	 15) /* end of rec 2064 Confirming Authority */

// DonR 27May2004 "Gadgets" Begin.
#define RECORD_LEN_2067	0

#define RECORD_LEN_4068					\
	(	Tlargo_code_len				+	\
		Tservice_code_len			+	\
		Tservice_number_len			+	\
		25 /* service_desc */		+	\
		 5 /* gadget_code */		+	\
		 2 /* get_ptn_from_as400 */	+	\
		 1 /* Pvt. Pharm enabled */ +	\
		 1 /* OTC sale enabled   */ +	\
		Tservice_type_len	)
// DonR 27May2004 "Gadgets" End.

#define RECORD_LEN_2069				\
	(	Tdiscount_code_len		+	\
		Tshow2spec_len			+	\
		Tshow2nonspec_len		+	\
		Tshow_needs_ishur_len	+	\
		Tptn_spec_basic_len		+	\
		Tptn_nonspec_basic_len	+	\
		Tptn_spec_keren_len		+	\
		Tptn_nonspec_keren_len	+	\
		Tspec_msg_len			+	\
		Tnonspec_msg_len		+	\
		Tdel_flg_c_len			)	/* end of record */
                                               
#define RECORD_LEN_2070				\
	(	Tlargo_code_len			+	\
		Tform_number_len		+	\
		Tdate_len				+	\
		Tdate_len				+	\
		Tdel_flg_c_len			)	/* end of record */

#define RECORD_LEN_2071				\
	(	Tshape_num_len			+	\
		Tinst_shape_code_len	+	\
		Tshape_desc_len			+	\
		Tinst_code_len			+	\
		Tinst_msg_len			+	\
		Tinst_seq_len			+	\
		Tcalc_op_flag_len		+	\
		Tunit_code_len			+	\
		Tunit_seq_len			+	\
		Tunit_name_len			+	\
		Tunit_desc_len			+	\
		Topen_od_window_len		+	\
		Tconcentration_flag_len	+	\
		Ttotal_unit_name_len	+	\
		Tround_units_flag_len	+	\
		Tdel_flg_c_len			)	/* end of record */

#define RECORD_LEN_2072				\
	(	Tpharmacology_code_len	+	\
		Ttreatment_group_len	+	\
		Ttreat_grp_desc_len		+	\
		Tpresc_valid_days_len	+	\
		Tdel_flg_c_len			)	/* end of record */
                                             
#define RECORD_LEN_2073				\
	(	Tpresc_type_len			+	\
		Tmonth_len				+	\
		Tfrom_day_len			+	\
		Tto_day_len				+	\
		Tdel_flg_c_len			)	/* end of record */
                                             
#define RECORD_LEN_2075					\
   ( 6 +  /* v_sale_number        */	\
    75 +  /* v_sale_name          */	\
     1 +  /* v_sale_owner         */	\
     1 +  /* v_sale_audience      */	\
     8 +  /* v_start_date         */	\
     8 +  /* v_end_date           */	\
     2 +  /* v_sale_type          */	\
     5 +  /* v_min_op_to_buy      */	\
     5 +  /* v_op_to_receive      */	\
     9 +  /* v_min_purchase_amt   */	\
     5 +  /* v_purchase_discount  */	\
     9    /* v_tav_knia_amt       */	\
   )

#define RECORD_LEN_2076				\
   (6 +  /* v_sale_number        */	\
    5 +  /* v_largo_code         */	\
    9    /* v_sale_price         */	\
   )

#define RECORD_LEN_2077				\
   (6 +  /* v_sale_number        */	\
    5 +  /* v_largo_code         */	\
    5 +  /* v_op_to_receive      */	\
    5 +  /* v_discount_percent   */	\
    9    /* v_sale_price         */	\
   )
                                     
#define RECORD_LEN_2078				\
   (6 +  /* v_sale_number        */	\
    2 +  /* v_pharmacy_type      */	\
    1 +  /* v_pharmacy_size      */	\
    9 +  /* v_target_op          */	\
    9    /* v_max_op             */	\
   )

#define RECORD_LEN_2079				\
   (6 +  /* v_sale_number        */	\
    7    /* v_pharmacy_code      */	\
   )

#define RECORD_LEN_2080			\
    (Tgnrldrugnotetype_len	+	\
 	 Tgnrldrugnotecode_len	+	\
	 Tgnrldrugnote_len		+	\
	 Tgdn_category_len		+	\
	 Tgdn_seq_num_len		+	\
	 Tgdn_connect_type_len	+	\
	 Tgdn_connect_desc_len	+	\
	 Tgdn_severity_len		+	\
	 Tdel_flg_c_len) /* end of rec 2080 PharmDrugNotes */

#define RECORD_LEN_2081								\
	(	Tmember_id_len			+					\
		Tmem_id_extension_len	+					\
		Tlargo_code_len			+					\
		TGenericYYYYMMDD_len	+	/* End Dt */	\
		TGenericYYYYMMDD_len	+	/* Start Dt */	\
		Tconfirmation_type_len	+					\
		Treason_mess_len		+					\
		Tmess_text_len			+					\
		8						+					\
		Tduration_len			+					\
		Tauthority_id_len		+					\
		Tspec_pres_num_sors_len	+					\
		9						+	/* Ishur Num */	\
		Tdoctor_id_len			+					\
		TGenericFlag_len		+	/* Ishur seen*/	\
		TGenericFlag_len			/* Del flg */	\
	)

#define RECORD_LEN_2082											\
	(	Tlargo_code_len			+								\
		5						+	/* group code*/				\
		8						+	/* Max units - was 5 */		\
		Tduration_len			+								\
		5						+	/* Class Code */			\
		8						+	/* history_start_date */	\
		6						+	/* 3 2-char flags */		\
		5							/* Rx src + Y/N, 3 + 2 */	\
	)

#define RECORD_LEN_2084	0

#define RECORD_LEN_2086								\
	(	 9	/* member_id		*/		+			\
		 1	/* mem_id_extension	*/		+			\
		 9	/* seq_number		*/		+			\
		 7	/* pharmacy_code	*/		+			\
		 1	/* pharmacy_type	*/		+			\
		 8	/* open_date		*/		+			\
		 8	/* close_date		*/		+			\
		 7	/* pharm_code_temp	*/		+			\
		 1	/* pharm_type_temp	*/		+			\
		 8	/* open_date_temp	*/		+			\
		 8	/* close_date_temp	*/		+			\
		 1	/* restriction_type	*/		+			\
		70	/* description		*/		+			\
		70	/* description_temp	*/		+			\
		 1	/* del_flg			*/					\
	)

// DonR 25May2010 - "Stickim".
#define RECORD_LEN_2096						\
		(									\
			 5	/* Class Code		*/	+	\
			75	/* Long Desc.		*/	+	\
			25	/* Short Desc.		*/	+	\
			 1	/* Class Type		*/	+	\
			 1	/* Class Sex		*/	+	\
			 3	/* Class Min. Age	*/	+	\
			 3	/* Class Max. Age	*/	+	\
			 5	/* Class Priority	*/	+	\
			 2	/* History Months	*/	+	\
			 8	/* History Start Dt	*/		\
		)


#define RECORD_LEN_4053						\
    (Tlargo_code_len		+				\
	 Tprof_len				+				\
 	 Tprcnt_len				+				\
 	 Tprcnt_len				+				\
 	 Tprcnt_len				+				\
     Tfixed_price_len		+ /* Basic */	\
     Tfixed_price_len		+ /* Kesef */	\
     Tfixed_price_len		+ /* Zahav */	\
	 TGenericFlag_len		+	/* In_health_basket (DonR 18Dec2007) */	\
	 Tdel_flg_c_len			+				\
	 Tprcnt_len				+	/* Yahalom % */				\
	 Tfixed_price_len		+	/* Yahalom Fixed Prc. */	\
	 Twait_months_len			/* Wait Months */			\
	)
	// End of 4053.

#define RECORD_LEN_4054					\
    (Tdrug_group_len		+			\
	 Tdrug_group_code_len	+			\
	 Tgroup_nbr_len			+			\
	 Tlargo_code_len		+			\
	 Titem_seq_len			+			\
	 Tdel_flg_c_len			+			\
	 Tsystem_code_len) /* end of rec 4054 EconomyPri */

#define RECORD_LEN_4074				\
	(	Trule_number_len		+	\
		Tlargo_code_len			+	\
		Textension_largo_co_len	+	\
		Tconfirm_authority__len	+	\
		Tconf_auth_desc_len		+	\
		Tfrom_age_len			+	\
		Tto_age_len				+	\
		Tprice_code_len		/* Basic */	+	\
		Tprice_code_len		/* Kesef */	+	\
		Tprice_code_len		/* Zahav */	+	\
		Tprice_percent_len		+	\
		Tprice_percent_len		+	\
		Tprice_percent_len		+	\
		Tmax_op_long_len		+	\
		Tmax_units_long_len 	+	\
		Tmax_amount_duratio_len	+	\
		Tpermission_type_len	+	\
		Tno_presc_sale_flag_len	+	\
		Tfixed_price_len		+	\
		Tfixed_price_len		+	\
		Tfixed_price_len		+	\
		Tivf_flag_len			+	\
		Trefill_period_len		+	\
		Trule_name_len			+	\
		Tin_health_basket_len	+	\
		Ttreatment_category_len	+	\
		Tsex_len				+	\
		Tneeds_29_g_len			+	\
		TGenericFlag_len		+	\
		Tdel_flg_c_len			+	\
		Tprice_code_len			+	\
		Tprice_percent_len		+	\
		Tfixed_price_len		+	\
		Twait_months_len			\
	)	/* end of record 4074 */


// DonR 16Mar2010 - "Nihul Tikrot".
#define RECORD_LEN_4100	/* coupons */						\
	(	Tmember_id_len			+							\
		Tmem_id_extension_len	+							\
		8						+	/*Expiry date		*/	\
		1						+	/*Expiry status		*/	\
		3							/*Fund Code			*/	\
	)

#define RECORD_LEN_4101	/* subsidy_messages */		\
	(	 4 /* message code */		+				\
		10 /* short description */	+				\
		25 /* long description */	)

#define RECORD_LEN_4102	/* tikra_type */			\
	(	 1 /* type code (char 1) */	+				\
		15 /* short description */	+				\
		30 /* long description */	)

#define RECORD_LEN_4103	/* credit_reasons */		\
	(	 4 /* reason code */		+				\
		30 /* short description */	+				\
		50 /* long description */	)

#define RECORD_LEN_4104	/* hmo_membership */		\
	(	 4 /* hmo code */			+				\
		15 /* description */		)

#define RECORD_LEN_4105	/* drug_shape */			\
	(	 4	/* drug_shape_code    */	+			\
		 4	/* drug_shape_desc    */	+			\
		25	/* shape_name_eng     */	+			\
		25	/* shape_name_heb     */	+			\
		 1	/* calc_op_by_volume  */	+			\
		 1	/* home_delivery_ok   */	+			\
		 1	/* calc_actual_usage  */		)	

#define RECORD_LEN_4106	/* drug_diagnoses */		\
	(	 9 /* reason code */		+				\
		 4 /* illness code      */	+				\
		 6 /* diagnosis code   */	)

// Marianna 25Aug2020 CR #32620 
#define RECORD_LEN_4107	/* how_to_take */		\
	(	 3 	/* how_to_take_code */		+			\
		 40 /* how_to_take_desc */		)				

// Marianna 26Aug2020 CR #32620 
#define RECORD_LEN_4108	/* unit_of_measure */		\
	(	 3 	/* unit_abbreviation  */		+			\
		 8 	/* short_desc_english */		+			\
		 8  /* short_desc_hebrew  */		)

// Marianna 28Aug2020 CR #32620 
#define RECORD_LEN_4109	/* reason_not_sold */		\
	(	 3 		/* reason_code  */		+			\
		 50 	/* reason_desc */		)			

#define RECORD_LEN_2501				\
       (Tpharmacy_code_len +			\
	Tinstitued_code_len +			\
	Tterminal_id_len +			\
	Terror_code_len +			\
	Topen_close_flag_len +			\
	Tprice_list_code_len +			\
	Tprice_list_date_len +			\
	Tprice_list_macabi__len +		\
	Tprice_list_cost_da_len +		\
	Tdrug_list_date_len +			\
	Thardware_version_len +			\
	Towner_len +				\
	Tcomm_type_len +			\
	Tphone_1_len +				\
	Tphone_2_len +				\
	Tphone_3_len +				\
	Tmessage_flag_len +			\
	Tprice_list_flag_len +			\
	Tprice_l_mcbi_flag_len +		\
	Tdrug_list_flag_len +			\
	Tdiscounts_date_len +			\
	Tsuppliers_date_len +			\
	Tmacabi_centers_dat_len +		\
	Terror_list_date_len +			\
	Tdisk_space_len +			\
	Tnet_flag_len +				\
	Tcomm_fail_filesize_len +		\
	Tbackup_date_len +			\
	Tavailable_mem_len +			\
	Tpc_date_len +				\
	Tpc_time_len +				\
	Tdb_date_len +				\
	Tdb_time_len +				\
	Tclosing_type_len +			\
	Tuser_ident_len +			\
	Terror_code_list_fl_len +		\
	Tdiscounts_flag_len +			\
	Tsuppliers_flag_len +			\
	Tmacabi_centers_fla_len +		\
	Tsoftware_ver_need_len +		\
	Tsoftware_ver_pc_len) /* end of rec 2501 */

#define RECORD_LEN_2502				\
      (Tpharmacy_code_len +			\
       Tinstitued_code_len +			\
       Tterminal_id_len +			\
       Tprice_list_code_len +			\
       Tmember_institued_len +			\
       Tmember_id_len +				\
       Tmem_id_extension_len +			\
       Tcard_date_len +			 	\
       Tdoctor_id_type_len +			\
       Tdoctor_id_len +				\
       Tdoctor_insert_mode_len +		\
       Tpresc_source_len +			\
       Treceipt_num_len +			\
       Tuser_ident_len +			\
       Tdoctor_presc_id_len +			\
       Tpresc_pharmacy_id_len +			\
       Tprescription_id_len +			\
       Tprice_code_len +			\
       Tmember_discount_pt_len +		\
       Tcredit_type_code_len +			\
       Tmacabi_code_len +			\
       Tmacabi_centers_num_len +		\
	   /* DonR 28AUG2002 Begin (Electronic Prescriptions) */	\
       Tmacabi_centers_num_len /* Dr. Device Code */	+		\
	   Treason_for_discnt_len							+		\
	   Treason_to_disp_len								+		\
	   /* DonR 28AUG2002 End */									\
       Tdate_len +				\
       Ttime_len +				\
       Tspecial_presc_num_len +			\
       Tspec_pres_num_sors_len +		\
       Tdiary_month_len +			\
       Terror_code_len +			\
       Tcomm_error_code_len +			\
       Telect_prescr_lines_len	+ /* DonR 29AUG2002 Changed to new macro L=2 */	\
	   /* 17Jun2010: "Nihul Tikrot" fields. */	\
	   2	+	/* action_type */				\
	   9	+	/* del_presc_id */				\
	   8	+	/* del_sale_date */				\
	   9	+	/* card_owner_id */				\
	   1	+	/* card_owner_id_code */		\
	   2	+	/* num_payments */				\
	   2	+	/* payment_method */			\
	   2	+	/* credit_reason_code */		\
	   1	+	/* tikra_called_flag */			\
	   4	+	/* tikra_status_code */			\
	   9	+	/* tikra_discount */			\
	   9	+	/* subsidy_amount */			\
	   2		/* member insurance (short) */	\
	)
  /* end of record len 2502 */


#define RECORD_LEN_2504				\
       (Tpharmacy_code_len +			\
	Tinstitued_code_len +			\
	Tterminal_id_len + 			\
	Tuser_ident_len + 			\
	Taction_type_len + 			\
	Tserial_no_len + 			\
	Tdate_len +				\
	Ttime_len +				\
	Tsupplier_for_stock_len + 		\
	Tinvoice_no_len + 			\
	Tdiscount_sum_len +			\
    Tnum_of_lines_4_len + 			\
	Tdiary_new_month_len +			\
	Terror_code_len +			\
	Terror_code_len +			\
	TVatAmount_len +			\
	TBilAmount_len +			\
	Tdate_len	 +			\
	TSupplSendNum_len +			\
	TBilAmountWithVat_len +			\
	TBilAmountBeforeVat_len  +		\
	TBilConstr_len  +			\
	TBilInvDiff_len) /* end of rec 2504 */

	
#define RECORD_LEN_2505				\
       (Tpharmacy_code_len		+	\
		Tinstitued_code_len		+	\
		Tterminal_id_len		+	\
		Taction_type_len		+	\
		Tdate_len				+	\
		Ttime_len				+	\
		Treceipt_num_len		+	\
		Tserial_no_2_len		+	\
		Tserial_no_3_len		+	\
		Tdiary_month_len		+	\
		Tupdate_date_len		+	\
		Tupdate_time_len		+	\
		Tnum_of_lines_4_len		+	\
		Tcomm_error_code_len	+	\
		Terror_code_len)  /* end of rec 2505 */
    
#define RECORD_LEN_2506				\
		(	Tpharmacy_code_len		+	\
			Tinstitued_code_len		+	\
			Tprescription_id_len	+	\
			Tlargo_code_len			+	\
			Top_len					+	\
			Tunits_len				+	\
			Tduration_len			+	\
			Tprice_for_op_len		+	\
			Top_stock_len			+	\
			Tunits_stock_len		+	\
			Ttotal_member_price_len	+	\
			Telect_prescr_lines_len	+	\
			Tcomm_error_code_len	+	\
			Tdrug_answer_code_len	+	\
			Ttotal_drug_price_len	+	\
			Tstop_use_date_len		+	\
			Tstop_blood_date_len	+	\
			Tdel_flg_len			+	\
			Tprice_replace_len		+	\
			Tprice_extension_len	+	\
			Tlink_ext_to_drug_len	+	\
			Tmacabi_price_flg_len	+	\
			Tdoctor_presc_id_len	+	\
			Tdate_len				+	\
			Tparticip_method_len	+	\
			5 /* updated_by */		+	\
			Trrn_len				+	\
			Tlargo_code_len			+	\
			Tsubst_permitted_len	+	\
			Tunits_len				+	\
			Top_len					+	\
			Tunits_len				+	\
			Tunits_len				+	\
			Tdiscount_percent_len	+	\
			Tcalc_member_price_len	+	\
			Tspecial_presc_num_len 	+	\
			Tspec_pres_num_sors_len	+	\
			Tsupplier_price_len		+	\
			5  /* rule_number */	+	\
			1  /* tikra_type_code */	+	\
			1  /* subsidized */			+	\
			3  /* discount_code */		+	\
			3  /* why_future_sale_ok */	+	\
			3  /* qty_limit_chk_type */	+	\
			9  /* doctor_id */			+	\
			1  /* doctor_id_type */		+	\
			2  /* presc_source */		+	\
			7  /* doctor_facility */	+	\
			2  /* elect_pr_status */	+	\
			1  /* use_instr_template */	+	\
			3  /* how_to_take_code */	+	\
			3  /* unit_code */			+	\
			3  /* course_treat_days */	+	\
			3  /* course_length */		+	\
			10 /* course_units */		+	\
			20 /* days_of_week */		+	\
			40 /* times_of_day */		+	\
			10 /* side_of_body */		+	\
			1  /* use_instr_changed */	+	\
			9  /* discount computed */	+	\
			4  /* # linked rxes */		+	\
			1  /* Barcode Scanned */	+	\
			1  /* Digital RX status */	+	\
			9  /* ph_OTC_unit_price */	+	\
			6  /* Member Diagnosis  */		\
	)
	/* end of record 2506 */


#define RECORD_LEN_2507				\
       (Tpharmacy_code_len +			\
       Tinstitued_code_len +			\
       Tterminal_id_len +			\
       Tspecial_presc_num_len +			\
       Tspec_pres_num_sors_len +		\
       Tprescription_id_len +			\
       Tline_no_len +				\
       Tlargo_code_len +			\
       Tlargo_code_inter_len +			\
       Tinteraction_type_len +			\
       Tdate_len +				\
       Ttime_len +				\
       Tdel_flg_len +				\
       Tpresc_id_inter_len +			\
       Tline_no_inter_len +			\
       Tmember_id_len +				\
       Tmem_id_extension_len +			\
       Tdoctor_id_len +				\
       Tdoctor_id_type_len +			\
       Tsec_doctor_id_len +			\
       Tsec_doctor_id_type_len +		\
       Terror_code_len +			\
       Tduration_len +				\
       Top_len +				\
       Tunits_len +				\
       Tstop_blood_date_len +			\
       Tcheck_type_len +			\
       Tdestination_len +			\
       Tprint_flg_len +				\
       Tdate_len)      /* end of rec 2507 */ 

#define RECORD_LEN_2508				\
       (Tpharmacy_code_len +			\
	Tterminal_id_len +			\
	Taction_type_len +			\
	Tserial_no_len +			\
	Tdiary_new_month_len +			\
	Tlargo_code_len +			\
	Tinventory_op_len +			\
	Tunits_amount_len +			\
	Tquantity_type_len +			\
	Tprice_for_op_len +			\
	Ttotal_drug_price_len +			\
	Top_stock_len +				\
	Tunits_stock_len +			\
	Tmin_stock_in_op_len +                  \
        TLineNum_len +                          \
        TBasePriceFor1Op_len ) /* end of rec 2508 */


#define RECORD_LEN_2509				\
       (Tdiary_month_len +			\
	Tpharmacy_code_len +			\
	Tterminal_id_len + 			\
	Taction_type_len + 			\
	Treceipt_num_len +			\
	Tterminal_id_proc_len +			\
	Tuser_ident_len +			\
	Tcard_num_len +				\
	Tpayment_type_len +			\
	Tsale_total_len +			\
	Tsale_num_len +				\
	Trefund_total_len +			\
	Trefund_num_len) /* end of rec 2509 */
    
    
#define RECORD_LEN_2510				\
      (Tpharmacy_code_len +			\
       Tinstitued_code_len +			\
       Tmember_id_len +				\
       Tmem_id_extension_len +			\
       Tprescription_id_len +			\
       Tprice_code_len	+			\
       Tpresc_pharmacy_id_len +			\
       Tpresc_delete_id_len + 			\
       Treceipt_delete_id_len + 		\
       Tdiary_month_len +			\
       Tuser_ident_len +			\
       Tdate_len +				\
       Ttime_len +				\
       Tdelete_lines_len +			\
       Tcomm_error_code_len +			\
       Terror_code_len + 			\
       Tdeletion_id_len) /* end of rec 2510 */


#define RECORD_LEN_2511				\
      (Tprescription_id_len +			\
       Tlargo_code_len + 			\
       Tdeletion_id_len +			\
       Top_stock_len +				\
       Tunits_stock_len)  /* end of rec 2511 */

#define RECORD_LEN_2512					\
       (Terror_description_len +		\
        Terror_line_len +               \
		Terror_code_len +			    \
		Tseverity_level_len +	        \
		Tseverity_pharm_len +           \
        Tc_define_name_len)/* end of rec 2512 */ 

// PHARMACY ISHUR                      
#define RECORD_LEN_2515					\
   (TGenericFlag_len        +			\
	Tpharmacy_code_len      +			\
	Tinstitued_code_len     +			\
	Tmember_id_len          +			\
	Tmem_id_extension_len   +			\
	Tdoctor_id_type_len     +			\
	Tauthority_id_len       +			\
	Tlast_name_len          +			\
	Tfirst_name_len         +			\
	TGenericYYYYMMDD_len    +			\
	TGenericHHMMSS_len      +			\
	Tspecial_presc_num_len  +			\
	TGenericFlag_len        +			\
	Tspecial_presc_num_len  +			\
	Tspec_pres_num_sors_len +			\
	Tconfirm_authority__len +			\
	Trule_number_len        +			\
	Tin_health_pack_len     +			\
	Tspeciality_code_len    +			\
	Tlargo_code_len         +			\
	Tprice_code_len			+			\
	Tfixed_price_len        +			\
	TGenericYYYYMMDD_len    +			\
	TGenericHHMMSS_len      +			\
	Tcomm_error_code_len    +			\
	Terror_code_len         +			\
	TGenericYYYYMMDD_len    +			\
	Tishur_text_len         +			\
	Tprescription_id_len    +			\
	Tdiary_month_len        +			\
	5) /* AS400 Ishur Extension days */
   /* end of record 2515 */                                              

// Prescriptions Written
#define RECORD_LEN_2516												\
       (															\
			TGenericTZ_len			/* v_contractor			*/	+	\
			Tdoctor_id_type_len		/* v_contracttype		*/	+	\
			TGenericTZ_len			/* v_treatmentcontr		*/	+	\
			9						/* v_termid				*/	+	\
			Tmem_id_extension_len	/* v_idcode				*/	+	\
			TGenericTZ_len			/* v_idnumber			*/	+	\
			TGenericYYYYMMDD_len	/* v_authdate			*/	+	\
			TGenericHHMMSS_len		/* v_authtime			*/	+	\
			Tinstitued_code_len		/* v_payinginst			*/	+	\
			Tlargo_code_len			/* v_medicinecode		*/	+	\
			Tdoctor_presc_id_len	/* v_prescription		*/	+	\
			5						/* v_quantity			*/	+	\
			5						/* v_douse				*/	+	\
			5						/* v_days				*/	+	\
			Top_len					/* v_op					*/	+	\
			1						/* v_prescriptiontype	*/	+	\
			TGenericYYYYMMDD_len	/* v_prescriptiondate	*/	+	\
			2						/* v_status				*/	+	\
			5						/* v_origin_code		*/	+	\
			5						/* v_sold_units			*/	+	\
			Top_len					/* v_sold_op			*/	+	\
			TGenericYYYYMMDD_len	/* v_sold_date			*/	+	\
			TGenericHHMMSS_len		/* v_sold_time			*/	+	\
			TGenericYYYYMMDD_len	/* v_prescr_added_dt	*/	+	\
			TGenericHHMMSS_len		/* v_prescr_added_tm	*/	+	\
			Tprescription_id_len	/* v_prw_id				*/		\
		) /* end of rec 2516 */

#define RECORD_LEN_2520				\
       ( Tdoctor_id_len			+	\
	     Tdoctor_id_type_len	+	\
	     9		+	\
	     Tdoctor_id_len			+	\
		 Tmember_id_len			+	\
		 Tmem_id_extension_len	+	\
		 Tdate_len				+	\
		 Ttime_len				+	\
	     Tlargo_code_len		+	\
	     Tlargo_code_len		+	\
	     Tquantity_text_len		+	\
		 5+\
		 5+\
		 5+\
		 9+\
	     Treason_text_len		+	\
		 Targument_len			+	\
	     Targument_text_len		+	\
		 Tdate_len				+	\
		 Ttime_len)/* end of rec 2520*//* Y20030730*/

// spec_letter_seen
#define RECORD_LEN_2521												\
       (															\
			TGenericTZ_len			/* v_contractor			*/	+	\
			Tmem_id_extension_len	/* v_idcode				*/	+	\
			TGenericTZ_len			/* v_idnumber			*/	+	\
			Tlargo_code_len			/* v_medicinecode		*/	+	\
			TGenericTZ_len			/* v_specialist_id		*/	+	\
			22						/* v_specialist_name	*/	+	\
			2						/* v_specialist_group	*/	+	\
			TGenericYYYYMMDD_len	/* v_date_of_letter		*/	+	\
			TGenericYYYYMMDD_len	/* v_update_date		*/	+	\
			TGenericHHMMSS_len		/* v_update_time		*/		\
		) /* end of rec 2521 */

// prescription_msgs
#define RECORD_LEN_2522					\
	(	Tprescription_id_len		+	\
		Tlargo_code_len				+	\
		Telect_prescr_lines_len		+	\
		Tdrug_answer_code_len		+	\
		Tseverity_level_len				\
	)
	/* end of record 2522 */

// pd_rx_link
#define RECORD_LEN_2523					\
	(	 9 /* prescription_id	*/ 	+	\
		 3 /* line_no			*/	+	\
		11 /* visit_number		*/	+	\
		 5 /* clicks_line_number*/	+	\
		 6 /* doctor_presc_id	*/	+	\
		 9 /* largo_prescribed	*/	+	\
		 9 /* largo_sold		*/	+	\
		 8 /* valid_from_date	*/	+	\
		 5 /* prev_unsold_op	*/	+	\
		 5 /* prev_unsold_units	*/	+	\
		 6 /* op_sold			*/	+	\
		 6 /* units_sold		*/	+	\
		 1 /* close_by_rounding	*/	+	\
		 1 /* rx_sold_status	*/		\
	)
	/* end of record 2523 */

/* << -------------------------------------------------------- >> */
/*                                                           	  */
/*		message-array sorted in dispatch-order		  */
/*                                                           	  */
/* << -------------------------------------------------------- >> */

typedef struct
{
      int	ID ;
      int	len ;
} TMsgStruct;

TMsgStruct MessageSequence[] =
{
	{ 2001, RECORD_LEN_2001 },
	{ 2002, RECORD_LEN_2002 },
	{ 2003, RECORD_LEN_2003 },
	{ 2004, RECORD_LEN_2004 },
	{ 2005, RECORD_LEN_4005 },
	{ 2006, RECORD_LEN_2006 },
	{ 2007, RECORD_LEN_2007 },
	{ 2008, RECORD_LEN_2008 },
	{ 2009, RECORD_LEN_2009 },
	{ 2010, RECORD_LEN_2010 },
	{ 2011, RECORD_LEN_2011 },
	{ 2012, RECORD_LEN_2012 },
	{ 2013, RECORD_LEN_2013 },
	{ 2014, RECORD_LEN_2014 },
	{ 2015, RECORD_LEN_2015 },
	{ 2017, RECORD_LEN_2017 },
	{ 2018, RECORD_LEN_2018 },
	{ 2019, RECORD_LEN_2019 },
	{ 2020, RECORD_LEN_2020 },
	{ 2021, RECORD_LEN_2021 },
	{ 2022, RECORD_LEN_2022 },
	{ 2025, RECORD_LEN_2025 },
	{ 2026, RECORD_LEN_2026 },
	{ 2029, RECORD_LEN_4029 },	// DonR 25Aug2015: incremental Member_Drug_29G download - NOT IN REAL USE YET.
	{ 2030, RECORD_LEN_2030 },
	{ 2031, RECORD_LEN_2031 },
	{ 2032, RECORD_LEN_2032 },
	{ 2035, RECORD_LEN_2035 },
	{ 2036, RECORD_LEN_2036 },
	{ 2037, RECORD_LEN_2037 },
	{ 2038, RECORD_LEN_2038 },
	{ 2039, RECORD_LEN_2039 },
	{ 2040, RECORD_LEN_2040 },
	{ 2041, RECORD_LEN_2041 },
	{ 2042, RECORD_LEN_2042 },
	{ 2043, RECORD_LEN_2043 },
	{ 2044, RECORD_LEN_2044 },
	{ 2045, RECORD_LEN_2045 },
	{ 2048, RECORD_LEN_2048 },
	{ 2049, RECORD_LEN_2049 },
	{ 2050, RECORD_LEN_2050 },
	{ 2051, RECORD_LEN_2051 },
	{ 2052, RECORD_LEN_2052 },
	{ 2053, RECORD_LEN_4053 },
	{ 2054, RECORD_LEN_4054 },
	{ 2055, RECORD_LEN_2055 },
	{ 2056, RECORD_LEN_2056 },
	{ 2061, RECORD_LEN_2061 },
	{ 2062, RECORD_LEN_2062 },
	{ 2063, RECORD_LEN_2063 },
	{ 2064, RECORD_LEN_2064 },
	{ 2065, RECORD_LEN_2065 },
	{ 2066, RECORD_LEN_2066 },
	{ 2067, RECORD_LEN_2067 },
	{ 2068, RECORD_LEN_4068 },
	{ 2069, RECORD_LEN_2069 },
	{ 2070, RECORD_LEN_2070 },
	{ 2071, RECORD_LEN_2071 },
	{ 2072, RECORD_LEN_2072 },
	{ 2073, RECORD_LEN_2073 },
	{ 2074, RECORD_LEN_4074 },
	{ 2075, RECORD_LEN_2075 },
	{ 2076, RECORD_LEN_2076 },
	{ 2077, RECORD_LEN_2077 },
	{ 2078, RECORD_LEN_2078 },
	{ 2079, RECORD_LEN_2079 },
	{ 2080, RECORD_LEN_2080 },
	{ 2081, RECORD_LEN_2081 },
	{ 2082, RECORD_LEN_2082 },
	{ 2084, RECORD_LEN_2084 },
	{ 2085, RECORD_LEN_4005 },	// DonR 19Sep2005: 2085 is now 2005 w/no timestamp update.
	{ 2086, RECORD_LEN_2086 },
	{ 2096, RECORD_LEN_2096 },	// DonR 25May2010: Purchase Limit Class.
	{ 2100, RECORD_LEN_4100 },	// DonR 16Mar2010: Intra-day coupons update for "Nihul Tikrot".
	{ 2501, RECORD_LEN_2501 },
	{ 2504, RECORD_LEN_2504 },
	{ 2505, RECORD_LEN_2505 },
	{ 2507, RECORD_LEN_2507 },
	{ 2508, RECORD_LEN_2508 },
	{ 2509, RECORD_LEN_2509 },
	{ 2512, RECORD_LEN_2512 },
	{ 2515, RECORD_LEN_2515 },
	{ 2520, RECORD_LEN_2520 },
	{ 2521, RECORD_LEN_2521 },
	{ 4005, RECORD_LEN_4005 },
	{ 4007, RECORD_LEN_2007 },	// DonR 19Jan2012: Enable full-table Over_dose download.
	{ 4011, RECORD_LEN_2011 },	// DonR 12Jan2012: Enable full-table Drug_interaction download.
	{ 4013, RECORD_LEN_2013 },	// DonR 19Jan2012: Enable full-table Doctors download.
	{ 4017, RECORD_LEN_2017 },	// DonR 19Jan2012: Enable full-table Doctor_speciality download.
	{ 4019, RECORD_LEN_2019 },
	{ 4021, RECORD_LEN_2021 },	// DonR 15Jan2012: Enable full-table Doctor_percents download.
	{ 4029, RECORD_LEN_4029 },	// DonR 25Aug2015: Full-table Member_Drug_29G download.
	{ 4030, RECORD_LEN_2030 },
	{ 4031, RECORD_LEN_2031 },	// DonR 12Jan2012: Enable full-table Price_list download.
	{ 4032, RECORD_LEN_2032 },	// DonR 19Jan2012: Enable full-table Member_price download.
	{ 4035, RECORD_LEN_2035 },	// DonR 19Jan2012: Enable full-table Suppliers download.
	{ 4052, RECORD_LEN_2052 },	// DonR 16Jan2012: Enable full-table DrugNotes download.
	{ 4053, RECORD_LEN_4053 },	// DonR 11Jan2012: Enable full-table DrugDoctorProf download.
	{ 4054, RECORD_LEN_4054 },	// DonR 16Mar2010: Enable full-table EconomyPri download.
	{ 4068, RECORD_LEN_4068 },	// DonR 28Nov2018: Enable full-table Gadgets download.
	{ 4074, RECORD_LEN_4074 },
	{ 4100, RECORD_LEN_4100 },	// DonR 16Mar2010: Full-table coupons download for "Nihul Tikrot".
	{ 4101, RECORD_LEN_4101 },	// DonR 16Mar2010: Full-table subsidy_messages download for "Nihul Tikrot".
	{ 4102, RECORD_LEN_4102 },	// DonR 16Mar2010: Full-table tikra_type download for "Nihul Tikrot".
	{ 4103, RECORD_LEN_4103 },	// DonR 16Mar2010: Full-table credit_reasons download for "Nihul Tikrot".
	{ 4104, RECORD_LEN_4104 },	// DonR 16Mar2010: Full-table hmo_membership download for "Nihul Tikrot".
	{ 4105, RECORD_LEN_4105 },	// DonR 09May2018: Full-table drug_list download.
	{ 4106, RECORD_LEN_4106 },	// DonR 03Oct2018: Full-table drug_diagnoses download.
	{ 4107, RECORD_LEN_4107 },	// Marianna CR #32620 25Aug2020: Full-table how_to_take download.
	{ 4108, RECORD_LEN_4108 },	// Marianna CR #32620 26Aug2020: Full-table unit_of_measure download.
	{ 4109, RECORD_LEN_4109 },	// Marianna CR #32620 28Aug2020: Full-table reason_not_sold download.
	{ 0L, 0L }			// terminating rec - changed from NULL
								// 27Jun2005 for Linux compatibility */
} ;



TMsgStruct MessagesToAs400[] =
{
      { 2510, RECORD_LEN_2510 },   
      { 2511, RECORD_LEN_2511 },   
      { 2502, RECORD_LEN_2502 },
      { 2501, RECORD_LEN_2501 },
      { 2505, RECORD_LEN_2505 },
      { 2507, RECORD_LEN_2507 },
      { 2509, RECORD_LEN_2509 },
      { 2504, RECORD_LEN_2504 },
      { 2508, RECORD_LEN_2508 },
      { 2512, RECORD_LEN_2512 },
      { 0L, 0L }			// terminating rec - changed from NULL
							// 27Jun2005 for Linux compatibility */
} ;

#endif // #ifndef DOCTORS_SRC

#endif /* __PHARMDBMSGS__H_ */

