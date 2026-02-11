/* << -------------------------------------------------------- >> */
/*			      DBFields.h                          */
/*                                                           	  */
/*  -----------------------------------------------------------   */
/*                                                           	  */
/*  Date: 	Oct-Nov 1996					  */
/*  Written by: Gilad Haimov ( reshuma )			  */
/*                                                           	  */
/*  -----------------------------------------------------------   */
/*                                                           	  */
/*  Containes :							  */
/*		typedefs & lengthes of DB types			  */
/*              & length of messages-records.			  */
/*  -----------------------------------------------------------   */
/*                                                           	  */
/*  Comments:  							  */
/*                                                           	  */
/* << -------------------------------------------------------- >> */


#if !defined( __DB_FIELDS__H_ )
#define __DB_FIELDS__H_


/* << -------------------------------------------------------- >> */
/*                                                           	  */
/*                                                           	  */
/*	I.	  M E S S A G E     F I E L D S			  */
/*                                                           	  */
/*                                                           	  */
/* << -------------------------------------------------------- >> */

/* << -------------------------------------------------------- >> */
/*                                                           	  */
/*	Section:        Length of DB fields                       */
/*                                                           	  */
/* << -------------------------------------------------------- >> */
/*
  Attension:
  ----------- >>
  
	s  ==  short int
	l  ==  long int
	c  ==  string
*/

/*
 * Informix doesn't support mixing 
 * #define with $define no more
 */
#ifdef NON_SQL
#define  TCodeNoSign_len 			 2
#define  Tdelete_lines_len 			 2
#define  Tdeletion_id_len 			 9
#define  Tpresc_delete_id_len 			 6
#define  Treceipt_delete_id_len 			 7
#define  Tline_no_len 			 1
#define  Tcalc_member_price_len 			 4
#define  Tstop_blood_date_len 			 8
#define  Tcomm_error_code_len 			 4
#define  Tmacabi_price_flg_len 			 1
#define  Tdate_yarpa_len 			 8
#define  Top_len 					 6
#define  Ttime_yarpa_len 			 6
#define  Tdate_macabi_len 			 8
#define  Ttime_macabi_len 			 6
#define  Tstop_use_flg_len 			 1
#define  Tspec_pres_num_sors_len 			 1
#define  Tprice_replace_len 			 6
#define  Tdoctor_status_len 			 5
#define  Thouse_num_len 			 4
#define  Tkeren_macabi_len 			 3
#define  Tcalc_type_code_len 			 4
#define  Tyarpa_part_code_len 			 2
#define  Tparticipation_name_len 			 25
#define  Tterminal_id_proc_len 			 2
#define  Trefund_num_len 			 4
#define  Trefund_total_len 			 9
#define  Tsale_num_len 			 4
#define  Tsale_total_len 			 9
#define  Tpayment_type_len 			 2
#define  Tcard_num_len 			 7
#define  Tnum_of_lines_4_len 			 4
#define  Tupdate_time_len 			 6
#define  Tupdate_date_len 			 8
#define  Tserial_no_3_len 			 9
#define  Tserial_no_2_len 			 3
#define  Tsoftware_ver_pc_len 			 9
#define  Tsoftware_ver_need_len 			 9
#define  Tmacabi_centers_fla_len 			 1
#define  Tsuppliers_flag_len 			 1
#define  Tdiscounts_flag_len 			 1
#define  Terror_code_list_fl_len 			 1
#define  Tclosing_type_len 			 1
#define  Tpc_date_len 			 8
#define  Tpc_time_len 			 6
#define  Tdb_date_len 			 8
#define  Tdb_time_len 			 6
#define  Tavailable_mem_len 			 4
#define  Tbackup_date_len 			 8
#define  Tcomm_fail_filesize_len 			 9
#define  Tnet_flag_len 			 1
#define  Tdisk_space_len 			 9
#define  Terror_list_date_len 			 8
#define  Tmacabi_centers_dat_len 			 8
#define  Tsuppliers_date_len 			 8
#define  Tdiscounts_date_len 			 8
#define  Tdrug_list_flag_len 			 1
#define  Tprice_l_cost_flag_len 			 1
#define  Tprice_l_mcbi_flag_len 			 1
#define  Tprice_list_flag_len 			 1
#define  Tmessage_flag_len 			 1
#define  Thardware_version_len 			 9
#define  Tdrug_list_date_len 			 8
#define  Tprice_list_cost_da_len 			 8
#define  Tprice_list_macabi__len 			 8
#define  Tprice_list_date_len 			 8
#define  Topen_close_flag_len 			 4
#define  Tinteraction_largo__len 			 5
#define  Tdrug_answer_code_len 			 4
#define  Tprice_extension_len 			 7
#define  Tchange_price_len 			 6
#define  Tdel_flg_len 			 1
#define  Tlink_ext_to_drug_len 			 9 /*l Marianna 16Jul2024 User Story #330877 */ 
#define  Ttotal_member_price_len 			 9
#define  Tduration_len 			 3
#define  Tunits_long_len 			 5
#define  Top_long_len 			 5
#define  Tmacabi_code_len 			 3
#define  Tprescription_id_len 			 9
#define  Tpresc_pharmacy_id_len 			 6
#define  Tdoctor_presc_id_len 			 6
#define  Treceipt_num_len 			 7
#define  Tdoctor_insert_mode_len 			 1
#define  Tmember_institued_len 			 2
#define  Tcheck_type_len 			 1
#define  Temployee_password_len 			 9
#define  Tallowed_proc_list_len 			 50
#define  Tallowed_belongings_len 			 50
#define  Temployee_id_len 			 9
#define  Tsupplier_type_len 			 2
#define  Tsupplier_table_cod_len 			 2
#define  Treceive_date_len 			 8
#define  Tunits_amount_l_len 			 5
#define  Top_amount_long_len 			 5
#define  Tunits_len 			 6
#define  Ttax_len 			 5
#define  Tkeren_mac_long_len 			 8
#define  Tsupplier_price_len 			 9
#define  Tmax_op_len 			 5
#define  Tmember_discount_pt_len 			 5
#define  Tasaf_code_len 			 1
#define  Tmac_magen_until_len 			 8
#define  Tmac_magen_from_len 			 8
#define  Tprice_code_len	 			 4
#define  Tpriv_pharm_sale_ok_len 			 1
#define  Tlargo_type_len 			 1
#define  Tconfirmation_type_len 			 2
#define  Top_in_dose_len 			 5
#define  Tunits_in_dose_len 			 5
#define  Tinventory_op_len 			 5
#define  Tadditional_price_len 			 1
#define  Tall_extensions_len 			 1
#define  Tall_sources_len 			 1
#define  Tauthority_id_len 			 9
#define  Tcard_date_len 			 4
#define  Tchange_mem_prc_ok_len 			 1
#define  Taction_type_len 			 2
#define  Tsupplier_for_stock_len 			 7
#define  Ttotal_after_discou_len 			 9
#define  Terror_code_len 			 4
#define  Tcity_len 				20
#define  Tcomm_supplier_len 			 5
#define  Tcomm_type_len 			 1
#define  Tcomputersoft_code_len 			 5
#define  Tconfirm_authority__len 			 2
#define  Tconf_auth_desc_len				25
#define  Tconfirm_reason_len 			 3
#define  Tconfirm_deliver_me_len 			 1
#define  Tcredit_flag_len 			 1
#define  Tcredit_type_code_len 			 2
#define  Tdate_len 			 8
#define  Tdate_of_bearth_len 			 8
#define  Tdescription_len 			 24
#define  Tdiscount_percent_len 			 5
#define  Tprice_percent_len 			 5
#define  Tdiscount_sum_len 			 9
#define  Tdoctor_id_len 			 9
#define  Tdoctor_id_type_len 			 1
#define  Tdose_num_len 			 2
#define  Tdose_renew_freq_len 			 3
#define  Tdrug_type_len 			 1
#define  Tdur_message_len 			 40
#define  Textension_largo_co_len 			 2
#define  Trule_number_len 			 9
#define  Trule_name_len 			 75
#define  Tfather_name_len 			 8
#define  Tfax_num_len 			 10
#define  Tfirst_center_type_len 			 2
#define  Tfirst_largo_code_len 			 9	/*l Marianna 16Jul2024 User Story #330877*/
#define  Tfirst_name_len 			 8
#define  Tfrom_age_len 			 4
#define  Thospital_code_len 			 5
#define  Tconst_sum_to_pay_len 			 7
#define  Tin_health_pack_len			1
#define  Thouse_no_len 			 4
#define  Tinstitued_code_len 			 2
#define  Tinsurance_status_len 			 1
#define  Tinteraction_level_len 			 2
#define  Tinteraction_type_len 			 4
#define  Tinvoice_no_len 			 10/*s*/
#define  Tkeren_mac_from_len 			 8
#define  Tkeren_mac_until_len 			 8
#define  Tkeren_wait_flag_len			 1
#define  Tkrn_macbi_price_pc_len 			 4
#define  Tlargo_code_len 			 9	/*l Marianna 14Jul2024 User Story #330877*/
#define  Tlargo_code_type_len 			 1
#define  Tlast_name_len 			 14
#define  Tlong_name_len 			 30
#define  Tname_25_len				 25
#define  Tmacabi_centers_nam_len 			 40
#define  Tmacabi_centers_num_len 			 7
#define  Tmacabi_price_len 			 9
#define  Tmacbi_mgn_price_pt_len 			 4
#define  Tmaccabi_until_len 			 8
#define  Tmarital_status_len 			 1
#define  Tmax_amount_duratio_len 			 3
#define  Tmax_price_len 			 9
#define  Tmax_units_per_day_len 			 5
#define  Tmax_units_len 			 5
#define  Tmem_id_extension_len 			 1
#define  Tmember_id_len 			 9
#define  Tmember_price_prcnt_len 			 5
#define  Tmbr_price_prcnt_p_len 			 4
#define  Tmin_stock_in_op_len 			 5
#define  Tmust_confirm_deliv_len 			 1
#define  Tno_presc_sale_flag_len 			 1
#define  Tfixed_price_len 			 9
#define  Told_id_extension_len 			 1
#define  Told_member_id_len 			 9
#define  Top_stock_len 			 7
#define  Towner_len 			 1
#define  Tpackage_len 			 10
#define  Tpackage_size_len 			 5
#define  Tpharmacist_first_n_len 			 8
#define  Tpharmacist_id_len 			 9
#define  Tpharmacist_id_exte_len 			 1
#define  Tpharmacist_last_na_len 			 14
#define  Tpharmacy_code_len 			 7
#define  Tpharmacy_name_len 			 30
#define  Tpermission_type_len 			 2
#define  Tphone_len 			 10
#define  Tphone_1_len 			 10
#define  Tphone_2_len 			 10
#define  Tphone_3_len 			 10
#define  Tpresc_source_len 			 2
#define  Tpresc_source_list_len 			 200
#define  Tprescription_exten_len 			 100
#define  Tprescription_lines_len 			 1
#define  Tprice_for_op_len 			 9
#define  Tprice_list_code_len 			 3
#define  Tquantity_type_len 			 1
#define  Trefund_id_len 			 9
#define  Trepeat_allowed_fla_len 			 1
#define  Tsecond_center_type_len 			 2
#define  Tsecond_largo_code_len 			 9	/*l Marianna 16Jul2024 User Story #330877*/
#define  Tserial_no_len 			 7
#define  Tsex_len 			 1
#define  Tspecial_presc_ext_len 			 1
#define  Tspecial_presc_num_len 			 8
#define  Tspeciality_code_len 			 5
#define  Tstart_use_date_len 			 8
#define  Tstop_use_date_len 			 8
#define  Tstreet_len 			 12
#define  Tstreet_and_no_len 			 20
#define  Tsupplemental_1_len 			 5
#define  Tsupplemental_2_len 			 5
#define  Tsupplemental_3_len 			 5
#define  Tsupplier_code_len 			 5
#define  Tsupplier_name_len 			 25
#define  Tt_half_len 			 3
#define  Tterminal_id_len 			 2
#define  Ttime_len 			 6
#define  Tto_age_len 			 4
#define  Ttotal_after_vat_len 			 9
#define  Ttotal_drug_price_len 			 9
#define  Ttreatment_duration_len 			 3
#define  Ttreatment_start_len 			 8
#define  Ttreatment_stop_len 			 8
#define  Tunits_amount_len 			 5
#define  Tunits_stock_len 			 7
#define  Tuser_ident_len 			 9
#define  Tyarpa_price_len 			 9
#define  Tzip_code_len 			 5
#define  Tdrug_book_flg_len 			 1
#define  Tpresc_id_inter_len 			 9
#define  Tline_no_inter_len 			 1
#define  Tsec_doctor_id_len 			 9
#define  Tsec_doctor_id_type_len 			 1
#define  Tdestination_len 			 4
#define  Tprint_flg_len 			 1
#define  Terror_description_len 			 60
#define  Terror_line_len 			 60
#define  Tseverity_level_len 			 2
#define  Tseverity_pharm_len 			 2
#define  Tc_define_name_len 			 64
#define  Tin_health_pack_len 			 1
/*20010128 Yulia for new transaction 2047-2055*/
#define		Tgen_comp_code_len			4
#define		Tgen_comp_name_len			40
#define		Tdel_flg_c_len				1
//#define		Tgen_inter_type_len		4
#define		Tinter_note_code_len		3
#define		Tinteraction_note_len		40
#define		Tgnrldrugnotetype_len		1
#define		Tgnrldrugnotecode_len		5
#define		Tgnrldrugnote_len			50
#define		Tprof_len					2
#define		Tprcnt_len					5
#define		Tdrug_group_len				1
#define		Tpharm_group_code_len		2
#define		Tpharm_group_name_len		25
#define		Tdrug_group_code_len		6
#define		Tgroup_nbr_len				3
#define		Titem_seq_len				3	/* DonR 05Feb2003 */

#define     Tshort_name_len             17
#define     Tform_name_len              25
#define     Tprice_prcnt_len			5
//#define     Tdrug_type_len				1
#define     Tpharm_sale_new_len			1
#define     Tsale_price_len				5
#define		Tinteract_flg_len				1
#define		Tstatus_send_len			1
#define		Twait_months_len			3

#define		Tdoct_number_len			6

// DonR 28AUG2002 Begin (Electronic Prescriptions)
#define		Tparticip_method_len		5	// DonR 25Dec2008: Expanded to add new status fields.
#define		Trrn_len					9
#define		Tsubst_permitted_len		2
#define		Treason_for_discnt_len		5
#define		Treason_to_disp_len			2
#define		Telect_prescr_lines_len		2
#define		Topenable_package_len		1
#define		Tishur_required_len			1
// DonR 28AUG2002 End

// DonR 13APR2003 Begin (Fee Per Service)
#define		Tfee_len					9
// DonR 13APR2003 End
#define		Tsystem_code_len			1 /*Y20030609*/
#define		Tquantity_text_len			40 /*Y20030730*/
#define		Treason_text_len			50 /*Y20030730*/
#define		Targument_len				9  /*Y20030730*/
#define		Targument_text_len			50 /*Y20030730*/

#define		Ttreatment_category_len		3	/* DonR 26Aug2003*/
#define		Tcountry_center_len			1  /*Yulia add next 5 field 20050203*/
#define     Tmess_code_len				5
#define 	Treason_mess_len			3
#define 	Tmess_text_len				60
#define     Tmade_date_len				8

#define     Textend_rule_days_len		3	// DonR 21Feb2005
//Yulia 20060525
#define	 Tkey_len                   2
#define	 Taction_type_len           1
#define	 Tkod_tbl_len               9
#define	 Tdescr_len                 160
#define	 Tsmall_descr_len           3
#define	 Tfld_9_len                 9
#define	 Tfld_4_len                 4
#define	 Ttimes_len                 4
#define  Tenglish_name_len			15   //Yulia 20060914

#endif

#ifndef NON_SQL 

#define Tdelete_lines_len               2/*s*/
#define TCodeNoSign_len                 2/*s*/
#define Tdeletion_id_len                9/*l*/
#define Tpresc_delete_id_len            6/*l*/
#define Treceipt_delete_id_len          7/*l*/
#define Tline_no_len                    1/*s*/
#define Tcalc_member_price_len          4/*s*/
#define Tstop_blood_date_len            8/*l*/
#define Tcomm_error_code_len            4/*s*/
#define Tmacabi_price_flg_len           1/*s*/
#define Tdate_yarpa_len                 8/*l*/
#define Top_len                         6/*l*/
#define Ttime_yarpa_len                 6/*l*/
#define Tdate_macabi_len                8/*l*/
#define Ttime_macabi_len                6/*l*/
#define Tstop_use_flg_len               1/*s*/
#define Tspec_pres_num_sors_len         1/*s*/
#define Tprice_replace_len              6/*l*/
#define Tdoctor_status_len              5/*l*/
#define Thouse_num_len                  4/*c*/
#define Tkeren_macabi_len               3/*s*/
#define Tcalc_type_code_len             4/*s*/
#define Tyarpa_part_code_len            2/*s*/
#define Tparticipation_name_len         25/*c*/
#define Tterminal_id_proc_len			2/*s*/
#define Trefund_num_len					4/*s*/
#define Trefund_total_len				9/*l*/
#define Tsale_num_len					4/*s*/
#define Tsale_total_len					9/*l*/
#define Tpayment_type_len				2/*s*/
#define Tcard_num_len					7/*l*/
#define Tnum_of_lines_4_len				4/*s*/
#define Tupdate_time_len				6/*l*/
#define Tupdate_date_len				8/*l*/
#define Tserial_no_3_len				9/*l*/
#define Tserial_no_2_len				3/*s*/
#define Tsoftware_ver_pc_len			9/*c*/
#define Tsoftware_ver_need_len			9/*c*/
#define Tmacabi_centers_fla_len			1/*s*/
#define Tsuppliers_flag_len				1/*s*/
#define Tdiscounts_flag_len				1/*s*/
#define Terror_code_list_fl_len			1/*s*/
#define Tclosing_type_len				1/*s*/
#define Tpc_date_len					8/*l*/
#define Tpc_time_len					6/*l*/
#define Tdb_date_len					8/*l*/
#define Tdb_time_len					6/*l*/
#define Tavailable_mem_len				4/*s*/
#define Tbackup_date_len				8/*l*/
#define Tcomm_fail_filesize_len			9/*l*/
#define Tnet_flag_len					1/*s*/
#define Tdisk_space_len					9/*l*/
#define Terror_list_date_len			8/*l*/
#define Tmacabi_centers_dat_len			8/*l*/
#define Tsuppliers_date_len				8/*l*/
#define Tdiscounts_date_len				8/*l*/
#define Tdrug_list_flag_len				1/*s*/
#define Tprice_l_cost_flag_len			1/*s*/
#define Tprice_l_mcbi_flag_len			1/*s*/
#define Tprice_list_flag_len			1/*s*/
#define Tmessage_flag_len				1/*s*/
#define Thardware_version_len			9/*l*/
#define Tdrug_list_date_len				8/*l*/
#define Tprice_list_cost_da_len			8/*l*/
#define Tprice_list_macabi__len			8/*l*/
#define Tprice_list_date_len			8/*l*/
#define Topen_close_flag_len			4/*s*/
#define Tinteraction_largo__len			5/*l*/
#define Tdrug_answer_code_len			4/*s*/
#define Tprice_extension_len			7/*l*/
#define Tchange_price_len				6/*l*/
#define Tdel_flg_len					1/*s*/
#define Tlink_ext_to_drug_len			9/*l Marianna 16Jul2024 User Story #330877*/
#define Ttotal_member_price_len			9/*l*/
#define Tduration_len					3/*s*/
#define Tunits_long_len					5/*l*/
#define Top_long_len					5/*l*/
#define Tmacabi_code_len				3/*s*/
#define Tprescription_id_len			9/*l*/
#define Tpresc_pharmacy_id_len			6/*l*/
#define Tdoctor_presc_id_len			6/*l*/
#define Treceipt_num_len				7/*l*/
#define Tdoctor_insert_mode_len			1/*s*/
#define Tmember_institued_len			2/*s*/
#define Tcheck_type_len					1/*s*/
#define Temployee_password_len			9/*l*/
#define Tallowed_proc_list_len			50/*c*/
#define Tallowed_belongings_len			50/*c*/
#define Temployee_id_len				9/*l*/
#define Tsupplier_type_len				2/*c*/
#define Tsupplier_table_cod_len			2/*s*/
#define Treceive_date_len				8/*l*/
#define Tunits_amount_l_len				5/*l*/
#define Top_amount_long_len				5/*l*/
#define Tunits_len						6/*l*/
#define Ttax_len						5/*l*/
#define Tkeren_mac_long_len				8/*l*/
#define Tsupplier_price_len				9/*l*/
#define Tmax_op_len						5/*l*/
#define Tmember_discount_pt_len			5/*s*/
#define Tasaf_code_len					1/*s*/
#define Tmac_magen_until_len			8/*l*/
#define Tmac_magen_from_len				8/*l*/
#define Tprice_code_len					4/*s*/
#define Tpriv_pharm_sale_ok_len			1/*s*/
#define Tlargo_type_len					1/*c*/
#define Tconfirmation_type_len			2/*s*/
#define Top_in_dose_len					5/*l*/
#define Tunits_in_dose_len				5/*l*/
#define Tinventory_op_len			5/*l*/
#define Tadditional_price_len		1/*s*/
#define Tall_extensions_len			1/*s*/
#define Tall_sources_len			1/*s*/
#define Tauthority_id_len			9/*l*/
#define Tcard_date_len				4/*s*/
#define Tchange_mem_prc_ok_len		1/*s*/
#define Taction_type_len			2/*s*/
#define Tsupplier_for_stock_len		7/*l*/
#define Ttotal_after_discou_len		9/*l*/
#define Terror_code_len				4/*s*/
#define Tcity_len					20/*c*/
#define Tcomm_supplier_len			5/*l*/
#define Tcomm_type_len				1/*s*/
#define Tcomputersoft_code_len		6	// DonR 17Oct2017 5 -> 6.
#define Tconfirm_authority__len		2/*s*/
#define Tconf_auth_desc_len			25 /*c*/
#define Tconfirm_reason_len			3/*s*/
#define Tconfirm_deliver_me_len		1/*s*/
#define Tcredit_flag_len			1/*s*/
#define Tcredit_type_code_len		2/*s*/
#define Tdate_len					8/*l*/
#define Tdate_of_bearth_len			8/*l*/
#define Tdescription_len			24/*c*/
#define Tdiscount_percent_len		5/*s*/
#define Tprice_percent_len			5/*s*/
#define Tdiscount_sum_len			9/*l*/
#define Tdoctor_id_len				9/*l*/
#define Tdoctor_id_type_len			1/*s*/
#define Tdose_num_len				2/*s*/
#define Tdose_renew_freq_len		3/*s*/
#define Tdrug_type_len				1/*c*/
#define Tdur_message_len			40/*c*/
#define Textension_largo_co_len		2/*c*/
#define Trule_number_len 			9 /*l*/
#define Trule_name_len 				75 /*c*/
#define Tfather_name_len			8/*c*/
#define Tfax_num_len				10/*c*/
#define Tfirst_center_type_len		2/*c*/
#define Tfirst_largo_code_len		9/*l  Marianna 14Jul2024 User Story #330877 */
#define Tfirst_name_len				8/*c*/
#define Tfrom_age_len				4/*s*/
#define Thospital_code_len			5/*l*/
#define Tconst_sum_to_pay_len   	7/*l*/
#define Tin_health_pack_len			1/*c*/
#define Thouse_no_len				4/*c*/
#define Tinstitued_code_len			2/*s*/
#define Tinsurance_status_len		1/*s*/
#define Tinteraction_level_len		2/*s*/
#define Tinteraction_type_len		4/*s*/
#define Tinvoice_no_len				10/*l*/
#define Tkeren_mac_from_len			8/*l*/
#define Tkeren_mac_until_len		8/*l*/
#define Tkeren_wait_flag_len		1/*s*/
#define Tkrn_macbi_price_pc_len		4/*s*/
#define Tlargo_code_len				9/*l	Marianna 14Jul2024 User Story #330877*/
#define Tlargo_code_type_len		1/*s*/
#define Tlast_name_len				14/*c*/
#define Tlong_name_len				30/*c*/
#define Tname_25_len				25/*c*/
#define Tmacabi_centers_nam_len		40/*c*/
#define Tmacabi_centers_num_len		7/*l*/
#define Tmacabi_price_len			9/*l*/
#define Tmacbi_mgn_price_pt_len		4/*s*/
#define Tmaccabi_until_len			8/*l*/
#define Tmarital_status_len			1/*s*/
#define Tmax_amount_duratio_len		3/*s*/
#define Tmax_price_len				9/*l*/
#define Tmax_units_per_day_len		5/*l*/
#define Tmax_units_len              5/*l*/
#define Tmem_id_extension_len		1/*s*/
#define Tmember_id_len				9/*l*/
#define Tmember_price_prcnt_len		5/*s*/
#define Tmbr_price_prcnt_p_len		4/*s*/
#define Tmin_stock_in_op_len		5/*l*/
#define Tmust_confirm_deliv_len		1/*s*/
#define Tno_presc_sale_flag_len		1/*s*/
#define Tfixed_price_len			9/*l*/
#define Told_id_extension_len		1/*s*/
#define Told_member_id_len			9/*l*/
#define Top_stock_len				7/*l*/
#define Towner_len					1/*s*/
#define Tpackage_len				10/*c*/
#define Tpackage_size_len			5/*l*/
#define Tpharmacist_first_n_len		8/*c*/
#define Tpharmacist_id_len			9/*l*/
#define Tpharmacist_id_exte_len		1/*s*/
#define Tpharmacist_last_na_len		14/*c*/
#define Tpharmacy_code_len			7/*l*/
#define Tpharmacy_name_len			30/*c*/
#define Tpermission_type_len		2/*s*/
#define Tphone_len					10/*c*/
#define Tphone_1_len				10/*c*/
#define Tphone_2_len				10/*c*/
#define Tphone_3_len				10/*c*/
#define Tpresc_source_len			2/*s*/
#define Tpresc_source_list_len		200/*c*/
#define Tprescription_exten_len		100/*c*/
#define Tprescription_lines_len		1/*s*/
#define Tprice_for_op_len			9/*l*/
#define Tprice_list_code_len		3/*s*/
#define Tquantity_type_len			1/*s*/
#define Trefund_id_len				9/*l*/
#define Trepeat_allowed_fla_len		1/*s*/
#define Tsecond_center_type_len		2/*c*/
#define Tsecond_largo_code_len		9/*l Marianna 16Jul2024 User Story #330877 */
#define Tserial_no_len				7/*l*/
#define Tsex_len					1/*s*/
#define Tspecial_presc_ext_len		1/*s*/
#define Tspecial_presc_num_len		8/*l*/
#define Tspeciality_code_len		5/*l*/
#define Tstart_use_date_len			8/*l*/
#define Tstop_use_date_len			8/*l*/
#define Tstreet_len					12/*c*/
#define Tstreet_and_no_len			20/*c*/
#define Tsupplemental_1_len			5/*l*/
#define Tsupplemental_2_len			5/*l*/
#define Tsupplemental_3_len			5/*l*/
#define Tsupplier_code_len			5/*l*/
#define Tsupplier_name_len			25/*c*/
#define Tt_half_len					3/*s*/
#define Tterminal_id_len			2/*s*/
#define Ttime_len					6/*l*/
#define Tto_age_len					4/*s*/
#define Ttotal_after_vat_len		9/*l*/
#define Ttotal_drug_price_len		9/*l*/
#define Ttreatment_duration_len		3/*s*/
#define Ttreatment_start_len		8/*l*/
#define Ttreatment_stop_len			8/*l*/
#define Tunits_amount_len			5/*l*/
#define Tunits_stock_len			7/*l*/
#define Tuser_ident_len				9/*l*/
#define Tyarpa_price_len			9/*l*/
#define Tzip_code_len				5/*l*/
#define Tdrug_book_flg_len			1/*s*/
#define Tpresc_id_inter_len         9/*l*/
#define Tline_no_inter_len          1/*s*/
#define Tsec_doctor_id_len          9/*l*/
#define Tsec_doctor_id_type_len     1/*s*/
#define Tdestination_len            4/*s*/
#define Tprint_flg_len              1/*s*/ 
#define Terror_description_len     60/*c*/
#define Terror_line_len            60/*c*/
#define Tseverity_level_len         2/*s*/
#define Tseverity_pharm_len         2/*s*/
#define Tc_define_name_len         64/*c*/
#define Tin_health_pack_len         1/*s*/ 
/*20010128 Yulia for new transaction 2047-2055*/
#define		Tgen_comp_code_len			4 /*s*/
#define		Tinter_note_code_len		3 /*s*/
#define		Tgen_comp_desc_len			40/*c*/
#define		Tdel_flg_c_len				1 /*c*/
//#define		Tgen_inter_type_len			4 /*s*/
#define		Tinteraction_note_len		40/*c*/
#define		Tgnrldrugnotetype_len				1 /*c*/
#define		Tgnrldrugnotecode_len				5 /*l*/
#define		Tdrug_group_len				1
#define		Tgnrldrugnote_len				50/*c*/
#define		Tprof_len					2 /*c*/
#define		Tprcnt_len					5 /*l*/
#define		Tdrug_drug_group_len		1 /*c*/
#define		Tdrug_group_code_len		6 /* DonR 05Nov2014 changed to long. */
#define		Tpharm_group_name_len		25/*c*/
#define		Tpharm_group_code_len		2/*s*/
#define		Tgroup_nbr_len				3 /*s*/
#define		Titem_seq_len				3	/* DonR 05Feb2003 */


#define     Tshort_name_len             17  /*c*/
#define     Tform_name_len              25  /*c*/
#define     Tprice_prcnt_len			5   /*s*/
//#define     Tdrug_type_len				1  /*c*/   
#define     Tpharm_sale_new_len			1  /*s*/
#define     Tsale_price_len				5  /*c*/
#define		Tinteract_flg_len				1  /*c*/
#define		Tstatus_send_len			1/*s*/
#define		Twait_months_len				3/*s*/

#define		Tdoct_number_len				6/*l*/

// DonR 28AUG2002 Begin (Electronic Prescriptions)
#define		Tparticip_method_len		5	// DonR 25Dec2008: Expanded to add new status fields.
#define		Trrn_len					9
#define		Tsubst_permitted_len		2
#define		Treason_for_discnt_len		5
#define		Treason_to_disp_len			2
#define		Telect_prescr_lines_len		2
#define		Topenable_package_len		1
#define		Tishur_required_len			1
// DonR 28AUG2002 End

// DonR 13APR2003 Begin (Fee Per Service)
#define		Tfee_len					9
// DonR 13APR2003 End
#define		Tsystem_code_len			1/*Y20030609*/
#define		Tquantity_text_len			40 /*s*//*Y20030730*/
#define		Treason_text_len			50 /*s*//*Y20030730*/
#define		Targument_len				9  /*l*//*Y20030730*/
#define		Targument_text_len			50 /*s*//*Y20030730*/

#define		Ttreatment_category_len		3	/* DonR 26Aug2003 */
#define		Tcountry_center_len			1  //Yulia add next 5 field 20050203
#define     Tmess_code_len				5
#define 	Treason_mess_len			3
#define 	Tmess_text_len				60
#define     Tmade_date_len				8

#define		Tpharm_credit_flag_len		1	/* DonR 16Sep2003 */
#define		Tishur_text_len				75	/* DonR 14Mar2004 */

// DonR 27May2004 "Gadgets" Begin.
#define		Tside_len					1
#define		Tservice_code_len			5
#define		Tservice_number_len			4
#define		Tservice_type_len			2
// DonR 27May2004 "Gadgets" End.

// DonR 08Jun2004 Clicks Discount
#define		Tdiscount_code_len		1
#define		Tshow2spec_len			3
#define		Tshow2nonspec_len		3
#define		Tshow_needs_ishur_len	1
#define		Tptn_spec_basic_len		3
#define		Tptn_nonspec_basic_len	3
#define		Tptn_spec_keren_len		3
#define		Tptn_nonspec_keren_len	3
#define		Tspec_msg_len			75
#define		Tnonspec_msg_len		75

// DonR 08Jun2004 Usage Instructions
#define		Tshape_num_len			3	// DonR 25Nov2020 CR #30978/User Story 108997: Change Shape Number Length from 2 to 3.
#define		Tinst_shape_code_len	4
#define		Tshape_desc_len			25
#define		Tinst_code_len			2
#define		Tinst_msg_len			40
#define		Tinst_seq_len			1
#define		Tcalc_op_flag_len		1
#define		Tunit_code_len			3
#define		Tunit_seq_len			1
#define		Tunit_name_len			8
#define		Tunit_desc_len			25
#define		Topen_od_window_len		1
#define		Tconcentration_flag_len	1
#define		Ttotal_unit_name_len	10
#define		Tround_units_flag_len	1

// DonR 08Jun2004 GenInterNotes
#define		Tinter_long_note_len    100  //Yulia changr == to " "
                                  
// DonR 08Jun2004 Treatment Groups
#define		Tpharmacology_code_len	2
#define		Ttreatment_group_len	2
#define		Ttreat_grp_desc_len		40
#define		Tpresc_valid_days_len	3

// DonR 08Jun2004 Drug Forms
#define		Tform_number_len		3
                                 
// DonR 08Jun2004 Prescription Periods
#define		Tpresc_type_len			1
#define		Tmonth_len				2
#define		Tfrom_day_len			3
#define		Tto_day_len				3
                                 
// DonR 08Jun2004 Drug Extension for Doctors
#define		Tivf_flag_len			1
#define		Trefill_period_len		3
#define		Tmax_units_long_len		5
#define		Tmax_op_long_len		5
#define		Tin_health_basket_len	1
#define		Tneeds_29_g_len			1

// DonR 08Jun2004 GnrlDrugNotes new fields
#define		Tgdn_type_new_len		1
#define		Tgdn_long_note_len		200
#define		Tgdn_category_len		1
#define		Tgdn_sex_len			1
#define		Tgdn_from_age_len		4
#define		Tgdn_to_age_len			4
#define		Tgdn_seq_num_len		2
#define		Tgdn_connect_type_len	3
#define		Tgdn_connect_desc_len	25
#define		Tgdn_severity_len		1
#define		Tgdn_sensitv_type_len	5
#define		Tgdn_sensitv_desc_len	25

#define     Textend_rule_days_len		3	// DonR 21Feb2005

//Yulia 20060525
#define	 Tkey_len                   2
#define	 Taction1_type_len           1
#define	 Tkod_tbl_len               9
#define	 Tdescr_len                 160
#define	 Tsmall_descr_len           3
#define	 Tfld_9_len                 9
#define	 Tfld_4_len                 4
#define	 Ttimes_len                 4
#define  Tenglish_name_len			15   //Yulia 20060914

#endif


/* << -------------------------------------------------------- >> */
/*                                                           	  */
/*   Section:		  DF field types			  */
/*                                                           	  */
/* << -------------------------------------------------------- >> */
//long to int 20150503 Yulia
typedef char    TCodeNoSign[64];
typedef short   Tkeren_macabi;
typedef short   Tline_no;
typedef short   Tspec_pres_num_sors;
typedef short   Tcomm_error_code;
typedef short   Tstop_use_flg;
typedef short   Tcalc_type_code;
typedef short   Tyarpa_part_code;
typedef short   Tterminal_id_proc;
typedef short   Trefund_num;
typedef int     Trefund_total;
typedef int     Tdoctor_status;
typedef int     Tunits;
typedef short   Tsale_num;
typedef short   Tmacabi_price_flg;
typedef int     Tsale_total;
typedef int     Tdeletion_id;
typedef int     Top;
typedef short   Tpayment_type;
typedef int     Tcard_num;
typedef int     Tpresc_delete_id;
typedef int     Treceipt_delete_id;
typedef short   Tnum_of_lines_4;
typedef int     Tupdate_time;
typedef int     Tupdate_date;
typedef int     Tserial_no_3;
typedef short   Tserial_no_2;
typedef char	Tsoftware_ver_pc[9 + 1];
typedef char	Tsoftware_ver_need[9 + 1];
typedef char	Thouse_num[64];
typedef short   Tmacabi_centers_fla;
typedef short   Tsuppliers_flag;
typedef short   Tcalc_member_price;
typedef short   Tdiscounts_flag;
typedef short   Terror_code_list_fl;
typedef short   Tclosing_type;
typedef int     Tpc_date;
typedef int     Tpc_time;
typedef int     Tdb_date;
typedef int     Tdb_time;
typedef short	Tavailable_mem;
typedef int     Tbackup_date;
typedef int     Tcomm_fail_filesize;
typedef short   Tnet_flag;
typedef short   Tdelete_lines;
typedef int     Tdisk_space;
typedef int     Terror_list_date;
typedef int     Tmacabi_centers_dat;
typedef int     Tprice_replace;
typedef int     Tsuppliers_date;
typedef int     Tdiscounts_date;
typedef short   Tdrug_list_flag;
typedef short   Tprice_l_cost_flag;
typedef short   Tprice_l_mcbi_flag;
typedef short   Tprice_list_flag;
typedef short   Tmessage_flag;
typedef int     Thardware_version;
typedef int     Tstop_blood_date;
typedef int     Tdrug_list_date;
typedef int     Tprice_list_cost_da;
typedef int     Tprice_list_macabi_;
typedef int     Tprice_list_date;
typedef short   Topen_close_flag;
typedef int     Tinteraction_largo_;
typedef short   Tdrug_answer_code;
typedef int     Tprice_extension;
typedef int     Tchange_price;
typedef short   Tdel_flg;
typedef int     Tlink_ext_to_drug;
typedef int     Ttotal_member_price;
typedef short   Tduration;
typedef int     Tunits_long;
typedef int     Top_long;
typedef short   Tmacabi_code;
typedef int     Tprescription_id;
typedef int     Tpre3c_pharmacy_id;
typedef int     Tdoctor_presc_id;
typedef int     Treceipt_num;
typedef short   Tdoctor_insert_mode;
typedef short   Tmember_institued;
typedef short   Tcheck_type;
typedef int     Temployee_password;
typedef char	Tallowed_proc_list[50 + 1];
typedef char	Tallowed_belongings[50 + 1];
typedef int     Temployee_id;
typedef char	Tsupplier_type[2 /* Tsupplier_type_len */ + 1];
typedef short   Tsupplier_table_cod;
typedef int     Treceive_date;
typedef int     Tunits_amount_l;
typedef int     Top_amount_long;
typedef int     Ttax;
typedef int     Tkeren_mac_long;
typedef int     Tsupplier_price;
typedef int     Tmax_units;      
typedef int     Tmax_op;     
typedef short   Tmember_discount_pt;
typedef short   Tasaf_code;
typedef int     Tmac_magen_until;
typedef int     Tmac_magen_from;
typedef short   Tprice_code;
typedef short   Tpriv_pharm_sale_ok;
typedef char	Tlargo_type[1 /* Tlargo_type_len */ + 1];
typedef int     Tunits_in_dose;
typedef int     Tinventory_op;
typedef short   Taction_type;
typedef int     Tsupplier_for_stock;
typedef int     Ttotal_after_discou;
typedef short   Terror_code;
typedef short	Tadditional_price ;
typedef short	Tall_extensions ;
typedef short	Tall_sources ;
typedef short	Tauthority_id ;
typedef short	Tcard_date ;
typedef short	Tdrug_book_flg;
typedef short	Tchange_mem_prc_ok ;
typedef char	Tcity[20 + 1] ;
typedef int 	Tcomm_supplier ;
typedef short	Tcomm_type ;
typedef int 	Tcomputersoft_code ;
typedef short	Tconfirm_authority_ ;
typedef short	Tconfirm_reason ;
typedef short	Tconfirm_deliver_me ;
typedef short	Tconfirmation_type;
typedef short	Tcredit_flag ;
typedef short	Tcredit_type_code ;
typedef int 	Tdate ;
typedef int 	Tdate_of_bearth ;
typedef char	Tdescription[24 + 1] ;
typedef short	Tdiscount_percent ;
typedef short	Tprice_percent ;
typedef int 	Tdiscount_sum ;
typedef int 	Tdoctor_id ;
typedef short	Tdoctor_id_type ;
typedef short	Tdose_num ;
typedef short	Tdose_renew_freq ;
typedef short	Tpermission_type;
typedef char	Tdrug_type[1 /* Tdrug_type_len */ + 1] ;
typedef char	Tdur_message[40 /* Tdur_message_len */ + 1] ;
typedef char	Textension_largo_co[2 /* Textension_largo_co_len */ + 1] ;
typedef char	Tfather_name[64] ;
typedef char	Tfax_num[10 /* Tfax_num_len */ + 1] ;
typedef char	Tfirst_center_type[2 /* Tfirst_center_type_len */ + 1] ;
typedef int 	Tfirst_largo_code ;
typedef char	Tfirst_name[64] ;
typedef short	Tfrom_age ;
typedef int 	Thospital_code ;
typedef int 	Tconst_sum_to_pay ;
typedef short   Tin_health_pack;

typedef char	Thouse_no[Thouse_no_len + 1] ;
typedef short	Tinstitued_code;
typedef short	Tinsurance_status ;
typedef short	Tinteraction_level ;
typedef short	Tinteraction_type ;
/*typedef long	Tinvoice_no ;*/
typedef char	Tinvoice_no[11] ;
typedef int 	Tkeren_mac_from ;
typedef int 	Tkeren_mac_until ;
typedef short	Tkeren_wait_flag ;
typedef int 	Tpresc_pharmacy_id ;
typedef short	Tkrn_macbi_price_pc ;
typedef int 	Tlargo_code ;
typedef short	Tlargo_code_type ;
typedef char	Tlast_name[64] ;
typedef char	Tlong_name[30 /* Tlong_name_len */ + 1] ;
typedef char	Tname_25[25 /* Tname_25_len */ + 1] ;
typedef short	Tmac_magen_code ;
typedef char	Tmacabi_centers_nam[40 /* Tmacabi_centers_nam_len */ + 1] ;
typedef int 	Tmacabi_centers_num ;
typedef int 	Tmacabi_price ;
typedef short	Tmacbi_mgn_price_pt ;
typedef short	Tmaccabi_code ;
typedef int 	Tmaccabi_until ;
typedef short	Tmarital_status ;
typedef short	Tmax_amount_duratio ;
typedef int 	Tmax_price ;
typedef int 	Tmax_units_per_day ;
typedef short	Tmem_id_extension ;
typedef int 	Tmember_id ;
typedef short	Tmember_price_prcnt;
typedef short	Tmbr_price_prcnt_p;
typedef int 	Tmin_stock_in_op;
typedef short	Tmust_confirm_deliv;
typedef short	Tno_presc_sale_flag;
typedef short	Told_id_extension;
typedef int 	Told_member_id;
typedef int 	Top_in_dose ;
typedef int 	Top_stock ;
typedef short	Towner ;
typedef char	Tpackage[10 /* Tpackage_len */ + 1] ;
typedef int 	Tpackage_size ;
typedef char	Tpharmacist_first_n[8 /* Tpharmacist_first_n_len */ + 1] ;
typedef int 	Tpharmacist_id ;
typedef short	Tpharmacist_id_exte ;
typedef char	Tpharmacist_last_na[14 /* Tpharmacist_last_na_len */ + 1] ;
typedef int 	Tpharmacy_code;
typedef char	Tpharmacy_name[30 /* Tpharmacy_name_len */ + 1] ;
typedef char	Tphone[10 /* Tphone_len */ + 1] ;
typedef char	Tphone_1[10 + 1] ;
typedef char	Tphone_2[10 + 1] ;
typedef char	Tphone_3[10 + 1] ;
typedef short	Tpresc_source ;
typedef char	Tpresc_source_list[200 /* Tpresc_source_list_len */ + 1] ;
typedef char	Tprescription_exten[100 /* Tprescription_exten_len */ + 1] ;
typedef short	Tprescription_lines ;
typedef int 	Tprice_for_op ;
typedef short	Tprice_list_code ;
typedef short	Tquantity_type ;
typedef int 	Trefund_id ;
typedef short	Trepeat_allowed_fla ;
typedef char	Tsecond_center_type[2 /* Tsecond_center_type_len */ + 1] ;
typedef int 	Tsecond_largo_code ;
typedef int 	Tserial_no ;
typedef short	Tsex ;
typedef short	Tspecial_presc_ext ;
typedef int 	Tspecial_presc_num ;
typedef int 	Tspeciality_code ;
typedef int 	Tstart_use_date ;
typedef int 	Tstop_use_date ;
typedef char	Tstreet[64] ;
typedef char	Tstreet_and_no[20 /* Tstreet_and_no_len */ + 1] ;
typedef int 	Tsupplemental_1 ;
typedef int 	Tsupplemental_2 ;
typedef int 	Tsupplemental_3 ;
typedef int 	Tsupplier_code ;
typedef char	Tsupplier_name[25 /* Tsupplier_name_len */ + 1] ;
typedef short	Tt_half ;
typedef short	Tterminal_id ;
typedef int 	Ttime ;
typedef short	Tto_age ;
typedef int 	Ttotal_after_vat ;
typedef int 	Ttotal_drug_price ;
typedef short	Ttreatment_duration ;
typedef int 	Ttreatment_start ;
typedef int 	Ttreatment_stop ;
typedef int 	Tunits_amount ;
typedef char	Tparticipation_name[25 + 1];
typedef int 	Tunits_stock ;
typedef int 	Tuser_ident ;
typedef int 	Tyarpa_price ;
typedef int 	Tzip_code ;
typedef int     Tlargo_code_inter;
typedef int     Tpresc_id_inter;
typedef short   Tline_no_inter;
typedef int     Tsec_doctor_id;
typedef short   Tsec_doctor_id_type;
typedef short   Tdestination;
typedef short   Tprint_flg;
typedef char    Terror_description[60 + 1] ;
//typedef char    Terror_line[Terror_line_len + 1] ;
typedef char    Terror_line[60 + 1] ;
typedef short   Tseverity_level;
typedef short   Tseverity_pharm;
typedef char    Tc_define_name[64 + 1] ;
typedef short   Tspec_presc_num_sors;
typedef short   Tdiary_month_1;
//typedef short   Tin_health_pack;

/*20010128 Yulia for new transaction 2047-2055*/
typedef short	Tgen_comp_code;
typedef short 	Tinter_note_code;  //20011125
typedef char 	Tgen_comp_desc[40 /* Tgen_comp_desc_len */ + 1];
typedef char 	Tdel_flg_c[	1 /* Tdel_flg_c_len */ +	1];
typedef short	Tgen_inter_type;
typedef char 	Tinteraction_note[40 /* Tinteraction_note_len */ + 1];		/*40*/
typedef char 	Tgnrldrugnotetype[1 /* Tgnrldrugnotetype_len */	+1]; /*1*/
typedef int 	Tgnrldrugnotecode;
typedef char 	Tgnrldrugnote	[50 /* Tgnrldrugnote_len */ + 1];		/*50*/
typedef char 	Tprof		[2 /* Tprof_len */ + 1 ];			/*2*/
typedef int 	Tprcnt;
typedef char 	Tdrug_group[1 /* Tdrug_group_len */	+ 1 ];	/*1 */
typedef int 	Tdrug_group_code;	// DonR 05Nov2014 changed from short to long.
typedef short	Tpharm_group_code;
typedef char 	Tpharm_group_name[25 /* Tpharm_group_name_len */ + 1];			/*25*/
typedef short	Tgroup_nbr;
typedef short	Titem_seq;		/* DonR 05Feb2003 */



typedef char Tshort_name[17 /* Tshort_name_len */ + 1];
typedef char Tform_name[25 /* Tform_name_len */ + 1];
typedef int  Tprice_prcnt;
//typedef char Tdrug_type1[Tdrug_type_len	+1];
//typedef char Tpharm_sale_new[Tpharm_sale_new_len+1];
typedef short Tpharm_sale_new;
typedef char Tsale_price[5 /* Tsale_price_len */ +1];				
typedef char Tinteract_flg[1 /* Tinteract_flg_len */ + 1];
typedef short Tstatus_send;

//GenSql.h	<=typedef 	long	TCCharRowid20;  /*oracle INFORMIX now*///As400UnixClient.ec

typedef int   Tdoct_number;//20020819

typedef short	Topenable_package;
typedef short	Tishur_required;
typedef char	Tsystem_code[1 /* Tsystem_code_len */ + 1];  /*Y20030609*/
typedef short int	TTreatmentCategory;		/* DonR 26Aug2003 */
typedef short int	Tpharm_credit_flag;		/* DonR 16Sep2003 */
typedef char    Tishur_text[80];

// DonR 27May2004 "Gadgets" Begin.
typedef int 	Tservice_code;
typedef short	Tservice_number;
typedef char	Tservice_type[2 + 1];
// DonR 27May2004 "Gadgets" End.
typedef char	Tenglish_name[15 + 1 ];   //Yulia 20060914



/* << -------------------------------------------------------- >> */
/*                                                           	  */
/*                                                           	  */
/*	II.	  M E S S A G E     R E C O R D S		  */
/*                                                           	  */
/*                                                           	  */
/* << -------------------------------------------------------- >> */

/* << -------------------------------------------------------- >> */
/*                                                           	  */
/*    Section:		Length of record			  */
/*                                                           	  */
/* << -------------------------------------------------------- >> */

// DonR 18Dec2011: Everything below is #ifdef-enabled ONLY for doctors;
// all the equivalent stuff for the pharmacy programs is now in
// PharmDBMsgs.h, which will be included after DBFields.h in
// As400UnixClient and As400UnixServer.

#ifdef DOCTORS_SRC

#define RECORD_LEN_3001			\
	(IdCode_1_len		+	\
	IdNumber_9_len		+	\
	ReqId_9_len			+	\
	ReqCode_5_len		+	\
	NoteType_2_len		+	\
	LineIndex_2_len		+	\
	Note_30_len			+	\
	IdNumber_9_len		+   \
	2) /* 3001*/   //last 2 for docrot number for send

#define RECORD_LEN_3002			\
	(IdCode_1_len		+	\
	IdNumber_9_len		+	\
	LabId_3_len		+	\
	Route_9_len		+	\
	ReqId_9_len		+	\
	ReqCode_5_len		+	\
	Result_10_len		+	\
	Units_15_len		+	\
	Limit_10_len		+	\
	Limit_10_len		+	\
	1			+	\
	1			+	\
	DateYYYYMMDD_len	+	\
	DateYYYYMMDD_len	+	\
	DateYYYYMMDD_len	+	\
	IdNumber_9_len		+	\
	TermId_9_len		+	\
	5					+   \
	2) /* 3002*/    //last 2 for docrot number for send

#define RECORD_LEN_3003			\
	(IdNumber_9_len		+	\
	IdCode_1_len		+	\
	IdNumber_9_len		+	\
	DateYYYYMMDD_len) /* 3003*/

#define RECORD_LEN_3004			\
	(IdNumber_9_len	) /* 3004*/

#define RECORD_LEN_3005			\
	(IdNumber_9_len		+	\
	IdCode_1_len		+	\
	IdNumber_9_len) /* 3005*/
// Changed by GAl 13/01/2003
#define RECORD_LEN_3006			\
	(IdNumber_9_len		+	\
	ContractType_2_len	+	\
	FirstName_8_len 	+	\
	LastName_14_len 	+	\
	Street_16_len 		+	\
	City_20_len 		+	\
	Phone_10_len 		+	\
	Mahoz_len	 		  +	\
	2	 		+	\
	1	 		+	\
	1	 		+	\
	1	 		+	\
	3	 		+	\
	1	 		+	\
	1	 		+	\
	1	 		+	\
	1	 		+	\
	TermId_7_len		+	\
	1	 		+	\
	DateYYYYMMDD_len	+	\
	DateYYYYMMDD_len	+	\
	LocationId_7_len	+	\
	Mahoz_len	 		+	\
	5	 		+	\
	LocationId_7_len	+	\
	1	 		+	\
	1	 		+	\
	3	 		+	\
	IdNumber_9_len		+	\
	2	 		+	\
	DateYYYYMMDD_len	+	\
	DateYYYYMMDD_len	+	\
	2	 		+	\
	9	 		+	\
	9	 		+	\
	9			+   \
	2	 		+	\
	1	 		+	\
	8	 		+	\
	1	 		+	\
	7	 		+	\
	7	 		+	\
	10	 		+	\
	2	 		+	\
	1	 		+	\
	3	 		+	\
	4	 		+	\
	4	 		+	\
	1	 		+	\
	2	 		+	\
	2	 		+	\
	2	) /* 3006 */

#define RECORD_LEN_3031			\
	(IdNumber_9_len		+	\
	ContractType_2_len	+	\
	FirstName_8_len 	+	\
	LastName_14_len 	+	\
	Street_16_len 		+	\
	City_20_len 		+	\
	Phone_10_len 		+	\
	Mahoz_len	 		  +	\
	2	 		+	\
	1	 		+	\
	1	 		+	\
	1	 		+	\
	3	 		+	\
	1	 		+	\
	1	 		+	\
	1	 		+	\
	1	 		+	\
	2	 		+	\
	2	 		+	\
	2	 		+	\
	2	 		+	\
	2	) /* 3031 */
#define RECORD_LEN_3032			\
(	TermId_7_len		+	\
	IdNumber_9_len		+	\
	ContractType_2_len	+	\
	1			 		+	\
	DateYYYYMMDD_len	+	\
	DateYYYYMMDD_len	+	\
	LocationId_7_len	+	\
	Mahoz_len	 		+	\
	5	 				+	\
	LocationId_7_len	+	\
	1	 				+	\
	1	 				+	\
	3	 				+	\
	9	 				+	\
	2	) /* 3032 */

#define RECORD_LEN_3033			\
(	TermId_7_len		+	\
	38					+	\
	Street_16_len 		+	\
	City_20_len 		+	\
	Phone_10_len 		+	\
	2	) /* 3033 */
#define RECORD_LEN_3034			\
	(IdNumber_9_len		+	\
	ContractType_2_len	+	\
	IdNumber_9_len		+	\
	2	 				+	\
	DateYYYYMMDD_len	+	\
	DateYYYYMMDD_len	+	\
	2			 		+	\
	2	) /* 3034 */

#define RECORD_LEN_3035			\
	(IdNumber_9_len		+	\
	ContractType_2_len	+	\
	FirstName_8_len 	+	\
	LastName_14_len 	+	\
	Street_16_len 		+	\
	City_20_len 		+	\
	Phone_10_len 		+	\
	Mahoz_len	 		  +	\
	2	 		+	\
	1	 		+	\
	1	 		+	\
	1	 		+	\
	3	 		+	\
	1	 		+	\
	1	 		+	\
	1	 		+	\
	1	 		+	\
	2	 		+	\
	2	 		+	\
	2	 		+	\
	2	 		+	\
	2			+	\
	TermId_7_len) /* 3035 */

#define RECORD_LEN_3007			\
	(IdNumber_9_len		+	\
	ContractType_2_len	+	\
	TermId_7_len		+	\
	LocationId_7_len)/*3007*/	/*20001030*/

#define RECORD_LEN_3008			\
	(IdNumber_9_len		+	\
	IdCode_1_len		+	\
	IdNumber_9_len		+	\
	DateYYYYMMDD_len	+	\
	DateYYYYMMDD_len	+	\
	30					+	\
    ReqId_9_len			+	\
	1120				+	\
	25					+	\
	30					+	\
	1					+	\
	9	)/* 3008*/   

#define RECORD_LEN_3009			\
	(  Action_1_len		+	\
       DateYYYYMMDD_len	+	\
       IdCode_1_len     +	\
       IdNumber_9_len	+	\
       IdNumber_9_len)

#define RECORD_LEN_3010			\
	(	IdCode_1_len		+	\
		IdNumber_9_len		+	\
		IdNumber_9_len		+	\
		ReqId_9_len			+	\
		30					+	\
		DateYYYYMMDD_len	+	\
		DateYYYYMMDD_len	+	\
		25					+	\
		30					+	\
		1					+	\
		2048				) /* 3010*/    //paps ,pato,cito

#define RECORD_LEN_3011		\
	(IdCode_1_len		+	\
	IdNumber_9_len		+	\
	LocationId_7_len	+	\
	DateYYYYMMDD_len	+	\
	DateYYYYMMDD_len	) /* 3011*/    //speechclient
                            
#define RECORD_LEN_3012		\
	(IdCode_2_len		+	\
	IdNumber_9_len		+	\
	IdNumber_9_len		+	\
	ContractType_2_len	+	\
	2					+	\
	30					+	\
	TermId_7_len		+	\
	DateYYYYMMDD_len	) /* 3012*/    //RASHAMIM

#define RECORD_LEN_3013			\
	(	IdCode_1_len		+	\
		IdNumber_9_len		+	\
		40				) /* 3013*/    //update_member_msg
#define RECORD_LEN_3014			\
	(	IdCode_1_len		+	\
		IdNumber_9_len		+	\
		8				) /* 3014*/    //mammo2
#define RECORD_LEN_3015			\
	(	IdCode_1_len		+	\
		IdNumber_9_len	) /* 3015*/    //sapat

#define RECORD_LEN_3017			\
	(	IdCode_1_len		+	\
		IdNumber_9_len		+	\
		4					+	\
		DateYYYYMMDD_len) /* 3017*/    //memberremarks
#define RECORD_LEN_3018			\
	(   DateYYYYMMDD_len	+	\
		TimeHHMMSS_len		+	\
		Tkey_len            +	\
		Taction1_type_len   +	\
		Tkod_tbl_len        +	\
		Tdescr_len          +	\
		Tsmall_descr_len    +	\
		Tfld_9_len          +	\
		Tfld_9_len          +	\
		Tfld_4_len          +	\
		Tfld_4_len          +	\
		Tfld_4_len          +	\
		Tfld_4_len          +	\
		Tdescr_len          +	\
		Ttimes_len   )/*3018 */ //clickstblupdate
#define RECORD_LEN_3019		\
	(	IdNumber_9_len		+	\
		IdNumber_9_len		+	\
		LocationId_7_len	+	\
		DateYYYYMMDD_len	+	\
		DateYYYYMMDD_len	+	\
		ContractType_2_len	+	\
		TermId_7_len		+	\
		2					+	\
		2					)/*3019*/  //gastrodefinitions

#define RECORD_LEN_3023		\
	(	IdNumber_9_len		+	\
		IdNumber_9_len		+	\
		IdCode_1_len		+	\
		ContractType_2_len	+	\
		2					+	\
		TermId_7_len	)/*3023*/  //membercontr
                            
#define RECORD_LEN_3024		\
	(	IdNumber_9_len		+	\
		IdCode_1_len		+	\
		IdNumber_9_len		+	\
		8					+	\
		8	)/*3024*/  //labadmin

#define RECORD_LEN_3025		\
	(	5		+	\
		5		+	\
		5		+	\
		5	)/*3025*/  //city distance

#define RECORD_LEN_3027		\
	(	1		+	\
		9		+	\
		8		+	\
		8		+	\
		5	)/*3027*/  //city distance

#define RECORD_LEN_3026		\
	(	1		+	\
		9		+	\
		9		+	\
		1	)/*3026*/  //psiciatr


#define RECORD_LEN_3501			\
	(IdNumber_10_len	+	\
	IdCode_5_len		+	\
	IdNumber_10_len		+	\
	DateYYMM_5_len		+	\
	DateYYMM_5_len		+	\
	DateYYMM_5_len		+	\
	TimeHHMM_5_len		+	\
	ContractType_5_len	+	\
	DateYYMM_5_len		+	\
	Elegebility_6_len	+	\
	ObservationCode_len	+	\
	Action_10_len		+	\
	Action_10_len		+	\
	Action_10_len		+	\
	Action_10_len		+	\
	PayingInst_len		+	\
	TermId_10_len		+	\
	Profession_len		+	\
	LocationId_10_len	+	\
	Position_len		+	\
	PaymentType_len		+	\
	LocationId_10_len) /* 3501*/

#define RECORD_LEN_3502			\
      (IdCode_5_len		+	\
      IdNumber_10_len		+	\
      DateYYMM_5_len		+	\
      DateYYYYMMDD_len		+	\
      TimeHHMMSS_len		+	\
      Elegebility_6_len		+	\
      Elegebility_6_len		+	\
      IdNumber_10_len		+	\
      ContractType_5_len	+	\
      Profession_len		+	\
      AuthQuarter_len		+	\
      DateYYMM_5_len		+	\
      DateYYMM_5_len		+	\
      DateYYMM_5_len		+	\
      TimeHHMM_5_len		+	\
      Error_len			+	\
      DateYYYYMMDD_len		+	\
      TermId_10_len		+	\
      LocationId_10_len		+	\
      AuthType_len		+	\
      CharNew_len		+	\
      CharNew_len		+	\
      LocationId_10_len		+	\
      Cancled_5_len		+	\
      CrType_5_len		+	\
      TaxAm_5_len)/*3502*/

#define RECORD_LEN_3503			\
      (TermId_10_len		+	\
      IdNumber_10_len		+	\
      ContractType_5_len	+	\
      ShiftStatus_len		+	\
      DateYYMM_5_len		+	\
      DateYYMM_5_len		+	\
      DateYYMM_5_len		+	\
      TimeHHMM_5_len		+	\
      Error_len			+	\
      LocationId_10_len		+	\
      Position_len		+	\
      PaymentType_len		+	\
      LocationId_10_len)/*3503*/

#define RECORD_LEN_3504			\
      (IdNumber_9_len		+	\
      ContractType_2_len	+	\
      TermId_9_len		+	\
      IdCode_2_len		+	\
      IdNumber_9_len		+	\
      DateYYMM_4_len		+	\
      DateYYYYMMDD_len		+	\
      TimeHHMM_4_len		+	\
      Elegebility_5_len		+	\
      ObservationCode_len	+	\
      ObservationCode_len	+	\
      ObservationCode_len	+	\
      ObservationCode_len	+	\
      TreatmentContr_len	+	\
      MedicineCode_len		+	\
      Prescriptions_len		+	\
      Quantity_len		+	\
      Douse_len			+	\
      Days_len			+	\
      NumberOfPacages_len	+	\
      CharNew_len		+	\
      DateYYYYMMDD_len		+	\
      MedicineCode_len		+	\
      Prescriptions_len		+	\
      Quantity_len		+	\
      Douse_len			+	\
      Days_len			+	\
      NumberOfPacages_len	+	\
      CharNew_len		+	\
      DateYYYYMMDD_len		+	\
      MedicineCode_len		+	\
      Prescriptions_len		+	\
      Quantity_len		+	\
      Douse_len			+	\
      Days_len			+	\
      NumberOfPacages_len	+	\
      CharNew_len		+	\
      DateYYYYMMDD_len		+	\
      MedicineCode_len		+	\
      Prescriptions_len		+	\
      Quantity_len		+	\
      Douse_len			+	\
      Days_len			+	\
      NumberOfPacages_len	+	\
      CharNew_len		+	\
      DateYYYYMMDD_len		+	\
      MedicineCode_len		+	\
      Prescriptions_len		+	\
      Quantity_len		+	\
      Douse_len			+	\
      Days_len			+	\
      NumberOfPacages_len	+	\
      CharNew_len		+	\
      DateYYYYMMDD_len		+	\
      MedicineCode_len		+	\
      Prescriptions_len		+	\
      Quantity_len		+	\
      Douse_len			+	\
      Days_len			+	\
      NumberOfPacages_len	+	\
      CharNew_len		+	\
      DateYYYYMMDD_len		+	\
	  CharNew7_len		+	\
      CharNew7_len		+	\
      CharNew7_len		+	\
      CharNew7_len		+	\
      CharNew7_len		+	\
      CharNew7_len		+	\
      PayingInst_len		+	\
      Profession_len		+	\
      LocationId_9_len		+	\
      LocationId_9_len		+	\
      CharNew_len		+	\
      CharNew_len)/*3504*/
      
#define RECORD_LEN_3505			\
	(TermId_9_len		+	\
	IdNumber_9_len		+	\
	ContractType_2_len	+	\
	IdCode_2_len		+	\
	IdNumber_9_len		+	\
	DateYYYYMMDD_len	+	\
	TimeHHMM_4_len		+	\
	Elegebility_5_len	+	\
	ObservationCode_len	+	\
	Action_9_len		+	\
	ActionNum_5_len		+	\
	IdNumber_9_len		+	\
	PayingInst_len		+	\
	DateYYYYMMDD_len	+	\
	TimeHHMM_4_len		+	\
	4			+	\
	Profession_len		+	\
	LocationId_9_len	+	\
	LocationId_9_len	+	\
	Position_len		+	\
	PaymentType_len)/*3505*/
      
#define RECORD_LEN_3506			\
      (Employee_len		+	\
      ActionNum_5_len		+	\
      DateYYMM_5_len		+	\
      DateYYMM_5_len		+	\
      DateYYMM_5_len		+	\
      TimeHHMM_5_len		+	\
      TermId_10_len)/*3506*/

#define RECORD_LEN_3507			\
	(IdNumber_9_len		+	\
	ContractType_2_len	+	\
	IdCode_2_len		+	\
	IdNumber_9_len		+	\
	DateYYYYMMDD_len	+	\
	TimeHHMMSS_len		+	\
	TermId_9_len		+	\
	5		+	\
	5		+	\
	IdNumber_9_len)/*3507*/

#define RECORD_LEN_3508			\
	(IdNumber_9_len		+	\
	ContractType_2_len	+	\
	IdCode_2_len		+	\
	IdNumber_9_len		+	\
	TermId_7_len		+	\
    Error_len			+	\
	IdNumber_9_len		+	\
	DateYYYYMMDD_len	+	\
	TimeHHMMSS_len		+	\
	DateYYYYMMDD_len	+	\
	1					+	\
	2					+	\
	1					+	\
	5					+	\
	12					+	\
	4					+	\
	4					+	\
	4					+	\
	DateYYYYMMDD_len	+	\
	TimeHHMMSS_len	)/*3508*/

#define RECORD_LEN_3509		\
	(IdNumber_9_len		+	\
	ContractType_2_len	+	\
	IdCode_2_len		+	\
	IdNumber_9_len		+	\
	DateYYYYMMDD_len	+	\
	TimeHHMMSS_len		+	\
	ReqCode_5_len		+	\
	DateYYYYMMDD_len	+	\
	TimeHHMMSS_len		+	\
	3)/*3509*/

#define RECORD_LEN_3510		\
	(IdNumber_9_len		+	\
	ContractType_2_len	+	\
	IdCode_2_len		+	\
	IdNumber_9_len		+	\
	DateYYYYMMDD_len	+	\
	TimeHHMMSS_len		+	\
	ObservationCode_len	+	\
	DateYYYYMMDD_len	+	\
	TimeHHMMSS_len	)/*3510*/

#define RECORD_LEN_3511		\
	(IdNumber_9_len		+	\
	ContractType_2_len	+	\
	IdCode_2_len		+	\
	IdNumber_9_len		+	\
	DateYYYYMMDD_len	+	\
	TimeHHMMSS_len		+	\
	ObservationCode_len	+	\
	DateYYYYMMDD_len	+	\
	TimeHHMMSS_len	)/*3511*/
#define RECORD_LEN_3518			\
	(IdNumber_9_len		+	\
	ContractType_2_len	+	\
	IdCode_2_len		+	\
	IdNumber_9_len		+	\
	TermId_7_len		+	\
    Error_len			+	\
	IdNumber_9_len		+	\
	DateYYYYMMDD_len	+	\
	TimeHHMMSS_len		+	\
	DateYYYYMMDD_len	+	\
	TimeHHMMSS_len	)/*3518*/

#define RECORD_LEN_3512		\
	(IdNumber_9_len		+	\
	ContractType_2_len	+	\
	IdCode_2_len		+	\
	IdNumber_9_len		+	\
	TermId_7_len		+	\
	3					+	\
	DateYYYYMMDD_len	+	\
	TimeHHMMSS_len		+	\
	DateYYYYMMDD_len	+	\
	15					+	\
	15					+	\
	15					+	\
	Profession_len		+	\
	LocationId_9_len	+	\
	LocationId_9_len	+	\
	Position_len		+	\
	PaymentType_len)/*3512*/

#define RECORD_LEN_3513		\
      (IdCode_2_len		+	\
      IdNumber_9_len	+	\
      DateYYMM_5_len	)/*3513*/

#define RECORD_LEN_3514		\
	(IdNumber_9_len		+	\
	IdCode_2_len		+	\
	IdNumber_9_len		+	\
	DateYYYYMMDD_len	+	\
	TimeHHMMSS_len		+	\
	30					+	\
	DateYYYYMMDD_len	+	\
	TimeHHMMSS_len		+	\
	25					+	\
	2					+	\
	30					+	\
	30					+	\
	30					+	\
	30					+	\
	1					+	\
	1					+	\
	ReqId_9_len			+	\
	4					+	\
	DateYYYYMMDD_len	+	\
	TimeHHMMSS_len		+	\
	IdNumber_9_len		+	\
	6144				) /*3514*/

#define RECORD_LEN_3515		\
	(IdNumber_9_len		+	\
	IdCode_2_len		+	\
	IdNumber_9_len		+	\
	9					+	\
	DateYYYYMMDD_len	+	\
	TimeHHMMSS_len		+	\
	7)/*3515*/
/*Yulia 20060222*/
#define RECORD_LEN_3517		\
	(TermId_9_len		+	\
	IdNumber_9_len		+	\
	ContractType_2_len	+	\
	IdCode_2_len		+	\
	IdNumber_9_len		+	\
	DateYYYYMMDD_len	+	\
	TimeHHMMSS_len		+	\
	Elegebility_5_len	+	\
	Action_9_len		+	\
	IdNumber_9_len		+	\
	PayingInst_len		+	\
	FirstName_8_len 	+	\
	LastName_14_len 	+	\
	9					+	\
	9					+	\
	5					+	\
	Profession_len		+	\
	LocationId_9_len	+	\
	LocationId_9_len	+	\
	Position_len		+	\
	PaymentType_len)/*3517*/

#endif /* DOCTORS_SRC */

/* << -------------------------------------------------------- >> */
/*                                                           	  */
/*		message-array sorted in dispatch-order		  */
/*                                                           	  */
/* << -------------------------------------------------------- >> */

#ifdef DOCTORS_SRC

typedef struct
{
      int	ID ;
      int	len ;
} TMsgStruct;

TMsgStruct MessageSequence[] =
{
      { 3501, RECORD_LEN_3501 },
      { 3502, RECORD_LEN_3502 },
      { 3503, RECORD_LEN_3503 },
      { 3504, RECORD_LEN_3504 },
      { 3505, RECORD_LEN_3505 },
      { 3506, RECORD_LEN_3506 },
      { 3001, RECORD_LEN_3001 }, /*20050131 send to different task*/
      { 3002, RECORD_LEN_3002 }, /* -||-*/
      { 3003, RECORD_LEN_3003 },
      { 3004, RECORD_LEN_3004 },
      { 3005, RECORD_LEN_3005 },
      { 3006, RECORD_LEN_3006 },
	  { 3007, RECORD_LEN_3007 },     /*20001030*/
	  { 3008, RECORD_LEN_3008 },     /*20020619*/
	  { 3009, RECORD_LEN_3009 },     /*20030126*/
	  { 3010, RECORD_LEN_3010 },     /*20040623*/
	  { 3011, RECORD_LEN_3011 },     /*20040623*/
	  { 3012, RECORD_LEN_3012 },     /*20040811*/
	  { 3013, RECORD_LEN_3013 },     /*20050131*/
	  { 3014, RECORD_LEN_3014 },     /*20050829*/
	  { 3015, RECORD_LEN_3015 },     /*20050925*/
	  { 3016, RECORD_LEN_3015 },     /*20050925*/
	  { 3017, RECORD_LEN_3017 },     /*20060302*/
	  { 3018, RECORD_LEN_3018 },     /*20060522*/
 	  { 3019, RECORD_LEN_3019 },     /*20070909*/
      { 3021, RECORD_LEN_3001 },     /*20070214 mokeds in as400unixdocserver*/
      { 3022, RECORD_LEN_3002 },	 /* -||-*/
      { 3023, RECORD_LEN_3023 },     /*20071014*/
      { 3024, RECORD_LEN_3024 },     /*20080807*/
      { 3025, RECORD_LEN_3025 },     /*20090706*/
      { 3026, RECORD_LEN_3026 },     /*20100128*/
	  { 3027, RECORD_LEN_3027 },     /*20091207*/
	  { 3031, RECORD_LEN_3031 },     /*20130123*/
  	  { 3032, RECORD_LEN_3032 },     /*20130123*/
	  { 3033, RECORD_LEN_3033 },     /*20130128*/
 	  { 3034, RECORD_LEN_3034 },     /*20130128*/
 	  { 3035, RECORD_LEN_3035 },     /*20131020*/	  	  
      { 0L, 0L }			// terminating rec - changed from NULL
							// 27Jun2005 for Linux compatibility */
} ;


TMsgStruct MessagesToAs400[] =
{
      { 3506, RECORD_LEN_3506 },
      { 3501, RECORD_LEN_3501 },
      { 3502, RECORD_LEN_3502 },
      { 3503, RECORD_LEN_3503 },
      { 3504, RECORD_LEN_3504 },
      { 3505, RECORD_LEN_3505 },
      { 3507, RECORD_LEN_3507 },
  	  { 3512, RECORD_LEN_3512 },
	  { 3508, RECORD_LEN_3508 },
      { 3509, RECORD_LEN_3509 },
      { 3510, RECORD_LEN_3510 },
      { 3511, RECORD_LEN_3511 },
      { 3513, RECORD_LEN_3513 },
      { 3514, RECORD_LEN_3514 },
      { 3515, RECORD_LEN_3515 },
      { 3516, RECORD_LEN_3515 }, /*=3515,Yulia20041220*/
	  { 3517, RECORD_LEN_3517 }, /*Yulia20060222*/
 	  { 3518, RECORD_LEN_3518 }, /*Yulia20071217*/

      { 0L, 0L }			// terminating rec - changed from NULL
							// 27Jun2005 for Linux compatibility */
} ;
#endif /* DOCTORS_SRC */

#endif /* __DB_FIELDS__H_ */

