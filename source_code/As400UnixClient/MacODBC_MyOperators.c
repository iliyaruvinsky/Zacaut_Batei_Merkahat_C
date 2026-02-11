		// Variables to set:
		//
		// SQL_CommandText		=	the SQL command, stripped of any "INTO" clause and without a final semicolon.
		//
		// NumOutputColumns		=	the number of columns SELECTed - if there are none, just leave it out
		//							and it defaults to zero.
		//
		// NumInputColumns		=	the number of columns passed to SQL as parameters, either for the WHERE
		//							clause or as values for INSERT or UPDATE. If there are none, just leave
		//							the variable out and it defaults to zero.
		//
		// StatementIsSticky	=	1 if the statement should be left PREPAREd for repeated use. Since the ODBC
		//							driver has a finite capacity for open statements, this parameter should
		//							be used sparingly; it's best used for statements that are (A) complex and
		//							(B) called very frequently in transactions that require high performance.
		//							If a statement will be used many times by a particular operation and then
		//							not used for a while, set it "sticky" and then execute FreeStatement() on
		//							it when you're done with it. This variable defaults to zero, so it needs
		//							to be set only when it's TRUE.
		//
		// CursorName			=	the name of the cursor. This is required ONLY for update cursors, so that
		//							"WHERE CURRENT OF" clauses will work. Otherwise leave it out, and it will
		//							default to NULL (= NOT an updatable cursor). Note that the ODBC interface
		//							routines will automatically add the "FOR UPDATE" clause if it isn't already
		//							present. IMPORTANT: Cursor names MUST start with a letter, and should be
		//							18 characters or less in length.
		//
		// OutputColumnSpec		=	a string with all the required type/length specifications for output columns.
		//							This can be in the form of C declaration types ("int,char[30]", etc.) or
		//							in the form of SQL row types ("smallint,varchar(30),integer", etc.) - the
		//							parsing routine is smart enough to interpret either way, or any mix.
		//							Delimiters can be spaces, commas, parentheses, brackets, or tabs, and variable
		//							type matching is case-insensitive. If there are no output columns, just leave
		//							this out and it defaults to NULL.
		//
		//							IMPORTANT: Because most of our decimal numbers are C type "double" stored
		//							in SQL "float" columns, "float" is translated to "double". If the C variable
		//							type is float, USE "REAL" instead of "float" or "double".
		//
		// InputColumnSpec		=	a string with all the required type/length specifications for output columns.
		//							This works exactly the same as OutputColumnSpec, described above.
		//
		// GenerateVALUES		=	if non-zero, the VALUES clause of an INSERT statment will be generated
		//							automatically. Note that this will work properly *only* if all INSERTed values
		//							come from variables - if some fields get constant values, you need to build
		//							your own custom VALUES clause.
		//
		// NeedsWhereClauseID	=	if non-zero, the argument after the SQL Operator ID specifies a "custom"
		//							WHERE clause, with its own logic and input variables (if any). This can
		//							be used any time you want to have variable methods of SELECTing the same
		//							information, so you don't have to create duplicate SQL. The main SQL command
		//							text MUST include a %s where the WHERE clause is meant to go! The actual
		//							word "WHERE" can be in the main SQL text or the "custom" text - but I think
		//							it will be more human-friendly if it's in the main text - i.e. WHERE %s.
		//
		// Convert_NF_to_zero	=	if non-zero, automatically convert a not-found error (SQLCODE 100) to a zero
		//							stored in the (integer) output column. This should be used ONLY for SQL
		//							operations where the output consists entirely of a COUNT function. Really,
		//							this shouldn't be necessary at all, since the database is supposed to return
		//							zero in COUNT if nothing was found - but at least in Informix, we're getting
		//							SQLCODE = 100 back from some operations, and it's better not to have to add
		//							new error-handling code all over the place. (DonR 21Jun2020)
		//
		// SuppressDiagnostics	=	If non-zero, suppress some diagnostic error messages. This is intended to be
		//							used for things like DROP TABLE commands where we don't really expect the
		//							table to be present, so we don't want to log errors when the SQLPrepare
		//							command fails.
		//
		// CommandIsMirrored	=	If non-zero, enable mirroring for this command. Mirroring applies to
		//							statements that are *not* SELECTs, and is globally enabled based on the
		//							environment variable ODBC_MIRRORING_ENABLED as well as having two different
		//							databases connected. If all these things are true, mirrored commands will
		//							be carried out in both databases, to keep specific tables synchronized.
		//							Typically this would involve normal INSERT/UPDATE/DELETE operations. For
		//							now at least, we are assuming that all mirrored commands involve simple
		//							SQL that does not need "translation" - so no "{TOP} N" or other stuff that
		//							involves different phrasing for Informix versus MS-SQL. Also, mirrored
		//							commands won't support stuff like WHERE CURRENT OF, since the cursor will
		//							exist only for the primary database.
		//
		//
		// Note that this code is inserted inside a switch statement - all you need to do is supply the cases.
		// The operator ID's for your program are defined in MacODBC_MyOperatorIDs.h, which - like this file -
		// should be in your specific program directory, NOT in the INCLUDE directory.


		// Include operations used by GenSql.ec
		#include "GenSql_ODBC_Operators.c"

//		case bypass_basket_cur:
//					SQL_CommandText		=	"	SELECT	illness_bit_number,		override_basket,	is_actual_disease,		use_diagnoses	"
//											"	FROM	illness_codes	"
//											"	WHERE	illness_bit_number	> 0	";
//					NumOutputColumns	=	4;
//					OutputColumnSpec	=	"	short,short,short,short	";
//					break;


		case AS400CL_2501_pharmacy_log_cur:
					SQL_CommandText		=	"	SELECT	pharmacy_code,		institued_code,			terminal_id,		"
											"			error_code,			open_close_flag,		price_list_code,	"
											"			price_list_date,	price_list_macabi_,		price_list_cost_da,	"
											"			drug_list_date,		hardware_version,		owner,				"
											"			comm_type,			phone_1,				phone_2,			"

											"			phone_3,			message_flag,			price_list_flag,	"
											"			price_l_mcbi_flag,	drug_list_flag,			discounts_date,		"
											"			suppliers_date,		macabi_centers_dat,		error_list_date,	"
											"			disk_space,			net_flag,				comm_fail_filesize,	"
											"			backup_date,		available_mem,			pc_date,			"

											"			pc_time,			db_date,				db_time,			"
											"			closing_type,		user_ident,				error_code_list_fl,	"
											"			discounts_flag,		suppliers_flag,			macabi_centers_fla,	"
											"			software_ver_need,	software_ver_pc,		r_count				"

											"	FROM	pharmacy_log													"

											"	WHERE	reported_to_as400	=  ?										"
											"	  AND	pharmacy_code		>= ?										"	// DonR 31Jul2012 - Configurable parameter.
											"	  AND	r_count				<  ?										";
					NumOutputColumns	=	42;
					OutputColumnSpec	=	"	int, short, short, short, short, short, int, int, int, int, int, short, short, varchar(10), varchar(10),	"
											"	varchar(10), short, short, short, short, int, int, int, int, int, short, int, int, short, int,				"
											"	int, int, int, short, int, short, short, short, short, char(9), char(9), int								";
					NumInputColumns		=	3;
					InputColumnSpec		=	"	short, int, int	";
					CursorName			=	"pharmacy_log_cur";
					break;


		case AS400CL_2501_READ_max_r_count:
					SQL_CommandText		=	"	SELECT MAX (r_count) FROM pharmacy_log	";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					break;


		case AS400CL_2501_UPD_pharmacy_log_cur:
					SQL_CommandText		=	"	UPDATE pharmacy_log SET reported_to_as400 = ? WHERE CURRENT OF pharmacy_log_cur	";
					NumInputColumns		=	1;
					InputColumnSpec		=	"	short	";
					break;


		case AS400CL_2502_prescriptions_cur:
											// DonR 27Mar2023 User Story #232220: Exclude sales with Credit Type Used = 99
											// (= payment type not yet determined). These should be sent to AS/400 only after
											// the member has actually decided on how to pay, at which point Credit Type Used
											// will be updated to a "real" value.
					SQL_CommandText		=	"	SELECT	pharmacy_code,			institued_code,			terminal_id,			"
											"			price_list_code,		member_institued,		member_id,				"
											"			mem_id_extension,		card_date,				doctor_id_type,			"
											"			doctor_id,				doctor_insert_mode,		presc_source,			"

											"			receipt_num,			user_ident,				doctor_presc_id,		"
											"			presc_pharmacy_id,		prescription_id,		sale_req_error,			"
											"			member_discount_pt,		credit_type_code,		credit_type_used,		"
											"			macabi_code,			macabi_centers_num,		doc_device_code,		"

											"			reason_for_discnt,		reason_to_disp,			date,					"
											"			time,					special_presc_num,		spec_pres_num_sors,		"
											"			diary_month,			error_code,				comm_error_code,		"
											"			prescription_lines,		action_type,			del_presc_id,			"

											"			del_sale_date,			card_owner_id,			card_owner_id_code,		"
											"			num_payments,			payment_method,			credit_reason_code,		"
											"			tikra_called_flag,		tikra_status_code,		tikra_discount,			"
											"			subsidy_amount,			insurance_type,			origin_code	,			"

											"			online_order_num,		darkonai_plus_sale,		paid_for,				"
											"			MagentoOrderNum															"

											"	FROM	prescriptions															"

											"	WHERE	reported_to_as400	=   ?												"
											"	  AND	delivered_flg		>   0												"	// DonR 14Jun2010 - Include deletions, zicui, etc.
											"	  AND	((time				<   ?)	OR (date	<  ?))							"
											"	  AND	credit_type_used	<> 99												";	// DonR 27Mar2023
					NumOutputColumns	=	52;
					OutputColumnSpec	=	"	int, short, short, short, short, int, short, short, short, int, short, short,	"
											"	int, int, int, int, int, short, short, short, short, short, int, int,			"
											"	int, short, int, int, int, short, short, short, short, short, short, int,		"
											"	int, int, short, short, short, short, short, short, int, int, char(1), short,	"
											"	long, short, short, long														";
					NumInputColumns		=	3;
					InputColumnSpec		=	"	short, int, int	";
					CursorName			=	"T2502_presc_cur";
					break;


		case AS400CL_2502_READ_prescriptions_count:
					SQL_CommandText		=	"	SELECT	CAST (count(*) AS INTEGER)						"

											"	FROM	prescriptions									"

											"	WHERE	reported_to_as400	= ?							"
											"	  AND	delivered_flg		> 0							"	// DonR 14Jun2010 - Include deletions, zicui, etc.
											"	  AND	((time				< ?)	OR (date	<  ?))	";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					NumInputColumns		=	3;
					InputColumnSpec		=	"	short, int, int	";
					break;


		case AS400CL_2502_UPD_prescriptions:
					SQL_CommandText		=	"	UPDATE prescriptions SET reported_to_as400 = ? WHERE CURRENT OF T2502_presc_cur	";
					NumInputColumns		=	1;
					InputColumnSpec		=	"	short	";
					StatementIsSticky	=	1;	// DonR 03Mar2021: Possible performance enhancement.
					break;


		case AS400CL_2504_stock_cur:		// DonR 03Sep2020: For MS-SQL compatibility (rowid is not supported), swith to "CURRENT OF" for updates.
					SQL_CommandText		=	"	SELECT		pharmacy_code,			institued_code,			terminal_id,			"
											"				user_ident,				action_type,			serial_no,				"
											"				date,					time,					supplier_for_stock,		"
											"				invoice_no,				discount_sum,			num_of_lines_4,			"
											"				diary_month,			error_code,				comm_error_code,		"

											"				update_date,			update_time,			vat_amount,				"
											"				bil_amount,				suppl_bill_date,		suppl_sendnum,			"
											"				bil_amountwithvat,		bil_amount_beforev,		bill_constr,			"
											"				bill_invdiff,			r_count											"

											"	FROM		stock																	"

											"	WHERE		reported_to_as400	=  ?												"
											"	  AND		pharmacy_code		>= ?												"	// DonR 31Jul2012 - Configurable parameter.
											"	  AND		del_flg				=  ?												"
											"	  AND		r_count				<= ?												"
											"	  AND		update_date			<= ?												";
					NumOutputColumns	=	26;
					OutputColumnSpec	=	"	int, short, short, int, short, int, int, int, int,			"
											"	varchar(10), int, short, int, short, short, int, int, int,	"
											"	int, int, varchar(10), int, int, short, int, int			";
					NumInputColumns		=	5;
					InputColumnSpec		=	"	short, int, short, int, int	";
					CursorName			=	"T2504_stock";
					break;


		case AS400CL_2504_READ_max_stock_r_count:
					SQL_CommandText		=	"	SELECT MAX (r_count) FROM stock		";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					break;


		case AS400CL_2504_UPD_stock:
					SQL_CommandText		=	"	UPDATE stock SET reported_to_as400 = ? WHERE CURRENT OF T2504_stock	";
					NumInputColumns		=	1;
					InputColumnSpec		=	"	short	";
					break;


		case AS400CL_2505_money_empty_cur:		// DonR 03Sep2020: For MS-SQL compatibility (rowid is not supported), swith to "CURRENT OF" for updates.
					SQL_CommandText		=	"	SELECT		pharmacy_code,		institued_code,		terminal_id,	"
											"				action_type,		date,				time,			"
											"				receipt_num,		serial_no_2,		serial_no_3,	"
											"				diary_month,		update_date,		update_time,	"
											"				num_of_lines_4,		comm_error_code,	error_code,		"
											"				r_count													"

											"	FROM		money_empty												"

											"	WHERE		reported_to_as400	=  ?								"
											"	  AND		pharmacy_code		>= ?								"	// DonR 31Jul2012 - Configurable parameter.
											"	  AND		r_count				<= ?								";
					NumOutputColumns	=	16;
					OutputColumnSpec	=	"	int, short, short, short, int, int, int, short, int, short, int, int, short, short, short, int	";
					NumInputColumns		=	3;
					InputColumnSpec		=	"	short, int, int	";
					CursorName			=	"T2505_MoneyEmpty";
					break;


		case AS400CL_2505_READ_money_empty_max_r_count:
					SQL_CommandText		=	"	SELECT MAX (r_count) FROM money_empty	";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					break;


		case AS400CL_2505_UPD_money_empty:
					SQL_CommandText		=	"	UPDATE money_empty SET reported_to_as400 = ? WHERE CURRENT OF T2505_MoneyEmpty	";
					NumInputColumns		=	1;
					InputColumnSpec		=	"	short	";
					break;


		case AS400CL_2506_prescription_drugs_cur:
											// DonR 02Aug2020: Added ASCII() to retrieve Tikra Type Code as a number, to be mapped
											// automatically to a 1-character TINYINT. This is the only way (at least in Informix)
											// to map to a simple character field without messing around with intermediate buffers.
											// DonR 27Mar2023 User Story #232220: Exclude sales with Credit Type Used = 99
											// (= payment type not yet determined). These should be sent to AS/400 only after
											// the member has actually decided on how to pay, at which point Credit Type Used
											// will be updated to a "real" value.
											// DonR 29Mar2023 User Story #432608: Read 11 new columns. Also, times_of_day is now
											// length 200 - although we're sending only 100 bytes to AS/400.
					SQL_CommandText		=	"	SELECT	pharmacy_code,			institued_code,			prescription_id,		"
											"			largo_code,				op,						units,					"
											"			duration,				price_for_op,			op_stock,				"
											"			units_stock,			total_member_price,		line_no,				"

											"			comm_error_code,		drug_answer_code,		total_drug_price,		"
											"			stop_use_date,			stop_blood_date,		del_flg,				"
											"			price_replace,			price_extension,		link_ext_to_drug,		"
											"			macabi_price_flg,		member_price_code,		doctor_presc_id,		"

											"			dr_presc_date,			particip_method,		updated_by,				"	// DonR 24Mar2020: updated_by should really be a SMALLINT/short int.
											"			prescr_wr_rrn,			dr_largo_code,			subst_permitted,		"
											"			units_unsold,			op_unsold,				units_per_dose,			"
											"			doses_per_day,			member_discount_pt,		special_presc_num,		"

											"			spec_pres_num_sors,		price_for_op_ph,		rule_number,			"
											"			ASCII(tikra_type_code),	subsidized,				discount_code,			"	// DonR 02Aug2020 ASCII() for char(0) fields.
											"			why_future_sale_ok,		qty_limit_chk_type,		doctor_id,				"
											"			doctor_id_type,			presc_source,			doctor_facility,		"

											"			elect_pr_status,		use_instr_template,		how_to_take_code,		"
											"			unit_code,				course_treat_days,		course_length,			"
											"			course_units,			days_of_week,			times_of_day,			"
											"			side_of_body,			use_instr_changed,		member_discount_amt,	"

											"			num_linked_rxes,		barcode_scanned,		digital_rx,				"
											"			member_diagnosis,		ph_OTC_unit_price,		voucher_amount_used,	"
											"			qty_limit_class_code,	UsageInstrGiven,		MaccabiOtcPrice,		"	// DonR 29Mar2023 begin.
											"			SalePkgPrice,			SaleNum4Price,			SaleNumBuy1Get1,		"

											"			Buy1Get1Savings,		ByHandReduction,		IsConsignment,			"
											"			NumCourses,																"	// DonR 29Mar2023 end.
											"			alternate_yarpa_price,													"	// Marianna 29Jun2023 User Story #461368
											"			blood_start_date,		blood_duration,			blood_last_date,		"
											"			blood_data_calculated,													"	// Marianna 10Aug2023 User Story #469361: Added 4 new fields
											"			NumCannabisProducts														"	// Marianna 19Feb2024 User Story #540234: Added 1 field.
							
											"	FROM	prescription_drugs														"

											"	WHERE	reported_to_as400	=   ?												"
											"	  AND	delivered_flg		>   0												"	// DonR 14Jun2010 - Include deletions, zicui, etc.
											"	  AND	((time				<   ?)	OR (date	<  ?))							"
											"	  AND	credit_type_used	<> 99												";	// DonR 27Mar2023
					NumOutputColumns	=	82;	// Marianna 19Feb2024 User Story #540234: Added 1 field
					OutputColumnSpec	=	"	int, short, int, int, int, int, int, int, int, int, int, short,											"
											"	short, short, int, int, int, short, int, int, int, short, short, int,									"
											"	int, short, int, int, int, short, int, int, int, int, int, int,											"
											"	short, int, int, char(0), short, short, short, short, int, short, short, int,							"
											"	short, short, short, char(3), short, short, char(10), varchar(20), varchar(200), char(10), short, int,	"
											"	short, short, short, int, int, int, short, varchar(100), int, int, int, int,							"
											"	int, int, short, short, int, int, short, int, short, short												";	// Marianna 19Feb2024 User Story #540234: Added 1 field
					NumInputColumns		=	3;
					InputColumnSpec		=	"	short, int, int	";
					CursorName			=	"T2506_PD_cur";
					break;


		case AS400CL_2506_READ_prescription_drugs_count:
					SQL_CommandText		=	"	SELECT	CAST (count(*) AS INTEGER)						"

											"	FROM	prescription_drugs								"

											"	WHERE	reported_to_as400	= ?							"
											"	  AND	delivered_flg		> 0							"	// DonR 14Jun2010 - Include deletions, zicui, etc.
											"	  AND	((time				< ?)	OR (date	<  ?))	";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					NumInputColumns		=	3;
					InputColumnSpec		=	"	short, int, int	";
					break;


		case AS400CL_2506_UPD_prescription_drugs:
					SQL_CommandText		=	"	UPDATE prescription_drugs SET reported_to_as400 = ? WHERE CURRENT OF T2506_PD_cur	";
					NumInputColumns		=	1;
					InputColumnSpec		=	"	short	";
					StatementIsSticky	=	1;	// DonR 03Mar2021: Possible performance enhancement.
					break;


		case AS400CL_2507_presc_drug_inter_cur:	// DonR 03Sep2020: For MS-SQL compatibility (rowid is not supported), swith to "CURRENT OF" for updates.
					SQL_CommandText		=	"	SELECT		pharmacy_code,			institued_code,			terminal_id,		"
											"				special_presc_num,		spec_pres_num_sors,		prescription_id,	"
											"				line_no,				largo_code,				largo_code_inter,	"
											"				interaction_type,		date,					time,				"

											"				del_flg,				presc_id_inter,			line_no_inter,		"
											"				member_id,				mem_id_extension,		doctor_id,			"
											"				doctor_id_type,			sec_doctor_id,			sec_doctor_id_type,	"
											"				dtl_error_code,			duration,				op,					"	// Was error_code, DonR 31Aug2005 send detailed version to AS400.

											"				units,					stop_blood_date,		check_type,			"
											"				destination,			print_flg,				sec_presc_date		"

											"	FROM		presc_drug_inter													"

											"	WHERE		reported_to_as400	=  ?											"
											"	  AND		pharmacy_code		>= ?											";	// DonR 31Jul2012 - Configurable parameter.
					NumOutputColumns	=	30;
					OutputColumnSpec	=	"	int, short, short, int, short, int, short, int, int, short, int, int,		"
											"	short, int, short, int, short, int, short, int, short, short, short, int,	"
											"	int, int, short, short, short, int											";
					NumInputColumns		=	2;
					InputColumnSpec		=	"	short, int	";
					CursorName			=	"T2507_pd_inter";
					break;


		case AS400CL_2507_UPD_presc_drug_inter:
					SQL_CommandText		=	"	UPDATE presc_drug_inter SET reported_to_as400 = ? WHERE CURRENT OF T2507_pd_inter	";
					NumInputColumns		=	1;
					InputColumnSpec		=	"	short	";
					break;


		case AS400CL_2508_stock_report_cur:	// DonR 03Sep2020: For MS-SQL compatibility (rowid is not supported), swith to "CURRENT OF" for updates.
					SQL_CommandText		=	"	SELECT		line_num,			pharmacy_code,		terminal_id,		"
											"				action_type,		serial_no,			diary_month,		"
											"				largo_code,			inventory_op,		units_amount,		"
											"				quantity_type,		price_for_op,		total_drug_price,	"

											"				op_stock,			units_stock,		min_stock_in_op,	"
											"				update_date,		update_time,		base_price,			"
											"				r_count														"

											"	FROM		stock_report												"

											"	WHERE		reported_to_as400	=  ?									"
											"	  AND		pharmacy_code		>= ?									"	// DonR 31Jul2012 - Configurable parameter.
											"	  AND		r_count				<= ?									"
											"	  AND		update_date			<= ?									";
					NumOutputColumns	=	19;
					OutputColumnSpec	=	"	short, int, short, short, int, int, int, int, int, short, int, int,	"
											"	int, int, int, int, int, int, int									";
					NumInputColumns		=	4;
					InputColumnSpec		=	"	short, int, int, int	";
					CursorName			=	"T2508_stock_rept";
					break;


		case AS400CL_2508_READ_stock_report_max_r_count:
					SQL_CommandText		=	"	SELECT MAX (r_count) FROM stock_report	";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					break;


		case AS400CL_2508_UPD_stock_report:
					SQL_CommandText		=	"	UPDATE stock_report SET reported_to_as400 = ? WHERE CURRENT OF T2508_stock_rept	";
					NumInputColumns		=	1;
					InputColumnSpec		=	"	short	";
					break;


		case AS400CL_2509_money_emp_lines_cur:	// DonR 03Sep2020: For MS-SQL compatibility (rowid is not supported), swith to "CURRENT OF" for updates.
					SQL_CommandText		=	"	SELECT		diary_month,		pharmacy_code,		terminal_id,		"
											"				action_type,		receipt_num,		terminal_id_proc,	"
											"				user_ident,			card_num,			payment_type,		"
											"				sale_total,			sale_num,			refund_total,		"
											"				refund_num,			r_count									"

											"	FROM		money_emp_lines												"

											"	WHERE		reported_to_as400	=  ?									"
											"	  AND		pharmacy_code		>= ?									"	// DonR 31Jul2012 - Configurable parameter.
											"	  AND		r_count				<= ?									";
					NumOutputColumns	=	14;
					OutputColumnSpec	=	"	short, int, short, short, int, short, int, int, short, int, short, int, short, int	";
					NumInputColumns		=	3;
					InputColumnSpec		=	"	short, int, int	";
					CursorName			=	"T2509_MoneyEmpLin";
					break;


		case AS400CL_2509_READ_money_emp_lines_max_r_count:
					SQL_CommandText		=	"	SELECT MAX (r_count) FROM money_emp_lines	";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					break;


		case AS400CL_2509_UPD_money_emp_lines:
					SQL_CommandText		=	"	UPDATE money_emp_lines SET reported_to_as400 = ? WHERE CURRENT OF T2509_MoneyEmpLin	";
					NumInputColumns		=	1;
					InputColumnSpec		=	"	short	";
					break;


		case AS400CL_2512_pc_error_message_cur:	// DonR 03Sep2020: For MS-SQL compatibility (rowid is not supported), swith to "CURRENT OF" for updates.
					SQL_CommandText		=	"	SELECT	error_description,	error_line,			error_code,		"
											"			severity_level,		severity_pharm,		c_define_name	"
											"	FROM	pc_error_message										"
											"	WHERE	reported_to_as400 = ?									";
					NumOutputColumns	=	6;
					OutputColumnSpec	=	"	varchar(60), varchar(60), short, short, short, varchar(64)	";
					NumInputColumns		=	1;
					InputColumnSpec		=	"	short	";
					CursorName			=	"T2512_PcErrMessage";
					break;


		case AS400CL_2512_UPD_pc_error_message:
					SQL_CommandText		=	"	UPDATE pc_error_message SET reported_to_as400 = ? WHERE CURRENT OF T2512_PcErrMessage	";
					NumInputColumns		=	1;
					InputColumnSpec		=	"	short	";
					break;


		case AS400CL_2513_UPD_prescription_drugs:
					SQL_CommandText		=	"	UPDATE	prescription_drugs										"
											"	SET		reported_to_as400	= ?									"
											"	WHERE	reported_to_as400	= ?									"
											"	  AND	delivered_flg		= ?									"
											"	  AND	(time				< ? OR (date < ? AND ? > 10000))	";
					NumInputColumns		=	6;
					InputColumnSpec		=	"	short, short, short, int, int, int	";
					break;


		case AS400CL_2513_UPD_prescription_msgs:
					SQL_CommandText		=	"	UPDATE	prescription_msgs										"
											"	SET		reported_to_as400	= ?									"
											"	WHERE	reported_to_as400	= ?									"
											"	  AND	delivered_flg		= ?									"
											"	  AND	(time				< ? OR (date < ? AND ? > 10000))	";
					NumInputColumns		=	6;
					InputColumnSpec		=	"	short, short, short, int, int, int	";
					break;


		case AS400CL_2513_UPD_prescriptions:
					SQL_CommandText		=	"	UPDATE	prescriptions											"
											"	SET		reported_to_as400	= ?									"
											"	WHERE	reported_to_as400	= ?									"
											"	  AND	delivered_flg		= ?									"
											"	  AND	(time				< ? OR (date < ? AND ? > 10000))	";
					NumInputColumns		=	6;
					InputColumnSpec		=	"	short, short, short, int, int, int	";
					break;


		case AS400CL_2515_pharmacy_ishur_cur:	// NOTE: This table is updated by specific values, NOT using "CURRENT OF".
					SQL_CommandText		=	"	SELECT	pharmacy_code,			institute_code,			terminal_id,			"
											"			diary_month,			ishur_date,				ishur_time,				"
											"			ishur_source,			pharm_ishur_num,		member_id,				"
											"			mem_id_extension,		authority_level,		auth_id_type,			"
											"			authority_id,			auth_first_name,		auth_last_name,			"

											"			as400_ishur_src,		as400_ishur_num,		as400_ishur_extend,		"
											"			line_number,			largo_code,				preferred_largo,		"
											"			member_price_code,		const_sum_to_pay,		rule_number,			"
											"			speciality_code,		treatment_category,		in_health_pack,			"
											"			no_presc_sale_flag,		ishur_text,				error_code,				"

											"			comm_error_code,		used_flg,				used_pr_id,				"
											"			used_pr_line_num,		updated_date,			updated_time,			"
											"			updated_by_trn,			reported_to_as400,		rrn						"

											"	FROM	pharmacy_ishur															"

											"	WHERE	reported_to_as400	= ?													"
											"	  AND	(ishur_date			< ?	OR ishur_time		< ?)						"
											"	  AND	(updated_date		< ?	OR updated_time		< ?)						";
					NumOutputColumns	=	39;
					OutputColumnSpec	=	"	int, short, short, short, int, int, short, int, int, short, short, short, int, varchar(8), varchar(14),	"
											"	short, int, short, short, int, int, short, int, int, int, short, short, short, varchar(80), short,		"
											"	short, short, int, short, int, int, short, short, int													";
					NumInputColumns		=	5;
					InputColumnSpec		=	"	short, int, int, int, int	";
					break;


		case AS400CL_2515_UPD_pharmacy_ishur:
					SQL_CommandText		=	"	UPDATE	pharmacy_ishur				"
											"	SET		reported_to_as400	= ?		"
											"	WHERE	rrn					= ?		";
					NumInputColumns		=	2;
					InputColumnSpec		=	"	short, int	";
					break;


		case AS400CL_2516_doctor_presc_cur:
											// DonR 06Dec2020: Added WHERE criterion member_id_code >= 0 to avoid trying
											// to send logically deleted rows (with negative Member TZ Code) to AS/400.
					SQL_CommandText		=	"	SELECT	doctor_id,				doctor_id,																			"
											"			member_id_code,			member_id,				visit_date,				visit_time,							"

											"			largo_prescribed,		doctor_presc_id,		FLOOR(amt_per_dose),										"
											"			doses_per_day,			treatment_length,		op,						CAST(prescription_type AS CHAR(1)),	"

											"			valid_from_date,		sold_status,			tot_units_sold,												"
											"			tot_op_sold,			last_sold_date,			last_sold_time,			row_added_date,						"
											"			row_added_time																								"

											"	FROM	doctor_presc																								"

											"	WHERE	reported_to_as400	= 0																						"
											"	  AND	(row_added_time	< ?	OR	row_added_date		< ?)															"
											"	  AND	(last_sold_time	< ?	OR	last_sold_date		< ?)															"
											"	  AND	member_id_code >= 0																							";
					NumOutputColumns	=	21;
					OutputColumnSpec	=	"	int, int, short, int, int, int,					"
											"	int, int, short, short, short, short, char(1),	"
											"	int, short, short, short, int, int, int, int	";
					NumInputColumns		=	4;
					InputColumnSpec		=	"	int, int, int, int	";
					CursorName			=	"T2516_Rx_cur";
					break;


		case AS400CL_2516_UPD_doctor_presc:
					SQL_CommandText		=	"	UPDATE doctor_presc SET reported_to_as400 = ? WHERE CURRENT OF T2516_Rx_cur	";
//					SQL_CommandText		=	"	UPDATE doctor_presc SET reported_to_as400 = ? WHERE visit_number = -9999	";
					NumInputColumns		=	1;
					InputColumnSpec		=	"	short	";
					StatementIsSticky	=	1;	// DonR 03Mar2021: Possible performance enhancement.
					break;


		case AS400CL_2517_drug_list_get_non_ep_drugs_cur:
					SQL_CommandText		=	"	SELECT	largo_code,	economypri_group,	parent_group_code	"

											"	FROM	drug_list											"

											"	WHERE	del_flg				=  0							"
											"	  AND	parent_group_code	>  0							"
											"	  AND	economypri_group	=  0							";
					NumOutputColumns	=	3;
					OutputColumnSpec	=	"	int, int, int	";
					break;


		case AS400CL_2517_CountEconomypriGroups:
					SQL_CommandText		=	"	SELECT	CAST (COUNT (DISTINCT group_code) AS INTEGER)	"
											"	FROM	economypri										"
											"	WHERE	send_and_use_pharm	<> 0						"	// DonR 31Jan2018 - use send_and_use_pharm instead of a range of Group Codes OR System Codes.
											"	  AND	del_flg				<> 'D'						";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					Convert_NF_to_zero	=	1;
					break;


		case AS400CL_2517_economypri_get_ep_groups_cur:
					SQL_CommandText		=	"	SELECT DISTINCT	group_code					"

											"	FROM			economypri					"

											//	WHERE			group_code	BETWEEN 1 AND 1000	   DonR 05Nov2014 - 800->1000
											//	WHERE			system_code IN ('0', '1', '4')	   DonR 30Jan2018 - use System Code instead of a range of Group Codes.
											"	WHERE			send_and_use_pharm	<> 0	"	// DonR 31Jan2018 - use send_and_use_pharm instead of a range of Group Codes OR System Codes.
											"	AND				del_flg				<> 'D'	"

											"	ORDER BY		group_code					";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					break;


		case AS400CL_2517_drug_list_get_group_drugs_cur:
											// DonR 2Apr2022 Bug Fix #241853: Include deleted drugs in the SELECT, since otherwise
											// we're preventing members from getting continued specialist discounts when the drug
											// they bought earlier gets marked as "deleted".
					SQL_CommandText		=	"	SELECT	largo_code,	economypri_group,	parent_group_code															"

											"	FROM	drug_list																									"

//											"	WHERE	del_flg				=  0																					"
//											"	  AND	economypri_group	IN (SELECT DISTINCT	economypri_group													"
											"	WHERE	economypri_group	IN (SELECT DISTINCT	economypri_group													"
											"									FROM			drug_list															"
											"									WHERE			economypri_group	>  0											"
											"									  AND			del_flg				=  0											"
											"									  AND			((economypri_group	=  ?) OR										"
											"													 (parent_group_code	IN (SELECT DISTINCT	parent_group_code			"
											"																			FROM			drug_list					"
											"																			WHERE			economypri_group	=  ?	"
											"																			  AND			parent_group_code	>  0	"
											"																			  AND			del_flg				=  0)))	"
											"								   )																					";
					NumOutputColumns	=	3;
					OutputColumnSpec	=	"	int, int, int	";
					NumInputColumns		=	2;
					InputColumnSpec		=	"	int, int	";
					StatementIsSticky	=	1;
					break;


		case AS400CL_2517_economypri_get_pref_largo_cur:
					SQL_CommandText		=	"	SELECT		group_code,	group_nbr,	item_seq,	largo_code	"

											"	FROM		economypri										"

									//			WHERE		group_code	< 1000				   DonR 05Nov2014 - 800->1000
									//			WHERE		system_code IN ('0', '1', '4')	   DonR 30Jan2018 - use System Code instead of a range of Group Codes.
											"	WHERE		send_and_use_pharm	<> 0						"	// DonR 31Jan2018 - use send_and_use_pharm instead of a range of Group Codes OR System Codes.
											"	AND			del_flg				<> 'D'						"

											"	ORDER BY	group_code, group_nbr, item_seq, largo_code		";
					NumOutputColumns	=	4;
					OutputColumnSpec	=	"	int, short, short, int	";
					break;


		case AS400CL_2517_CREATE_spclty_drug_grp_n:	// This will need some changes for MS SQL Server!
					if (Database == &INF_DB)
					{
						// DonR 17Jan2021: Moved Largo Code to the end of the primary key, since it's the "payload" column
						// when we SELECT by group_code = X or parent_group_code = Y.
						SQL_CommandText		=	"	CREATE TABLE	spclty_drug_grp_n	(	group_code			INTEGER		DEFAULT 0  NOT NULL,		"
												"											largo_code			INTEGER		DEFAULT 0  NOT NULL,		"
												"											drug_ep_group		INTEGER		DEFAULT 0  NOT NULL,		"
												"											parent_group_code	INTEGER		DEFAULT 0  NOT NULL,		"
												"											update_date			INTEGER		DEFAULT 0  NOT NULL,		"
												"											update_time			INTEGER		DEFAULT 0  NOT NULL,		"
												"											PRIMARY KEY (group_code, parent_group_code, largo_code)		"
												"										)																"
												"										EXTENT SIZE 240 NEXT SIZE 60 LOCK MODE PAGE						";
					}
					else
					{
						// DonR 17Jan2021: Moved Largo Code to the end of the primary key, since it's the "payload" column
						// when we SELECT by group_code = X or parent_group_code = Y.
						SQL_CommandText		=	"	CREATE TABLE	spclty_drug_grp_n	(	group_code			INTEGER		DEFAULT 0  NOT NULL,		"
												"											largo_code			INTEGER		DEFAULT 0  NOT NULL,		"
												"											drug_ep_group		INTEGER		DEFAULT 0  NOT NULL,		"
												"											parent_group_code	INTEGER		DEFAULT 0  NOT NULL,		"
												"											update_date			INTEGER		DEFAULT 0  NOT NULL,		"
												"											update_time			INTEGER		DEFAULT 0  NOT NULL,		"
												"											PRIMARY KEY (group_code, parent_group_code, largo_code)		"
												"										)																";
					}
					break;


		case AS400CL_2517_DROP_spclty_drug_grp_n:
					if (Database == &INF_DB)
						SQL_CommandText		=	"	DROP TABLE spclty_drug_grp_n	";
					else
						SQL_CommandText		=	"	DROP TABLE IF EXISTS spclty_drug_grp_n	";
					SuppressDiagnostics	=	1;
					break;


		case AS400CL_2517_DROP_spclty_drug_grp_o:
					if (Database == &INF_DB)
						SQL_CommandText		=	"	DROP TABLE spclty_drug_grp_o	";
					else
						SQL_CommandText		=	"	DROP TABLE IF EXISTS spclty_drug_grp_o	";
					SuppressDiagnostics	=	1;
					break;


		case AS400CL_2517_DROP_temp_pref_largos:
					if (Database == &INF_DB)
						SQL_CommandText		=	"	DROP TABLE				temp_pref_largos	";
					else
						SQL_CommandText		=	"	DROP TABLE IF EXISTS	temp_pref_largos	";
					SuppressDiagnostics	=	1;
					break;


		case AS400CL_2517_INS_spclty_drug_grp_n:
					SQL_CommandText		=	"	INSERT INTO	spclty_drug_grp_n							"
											"	(														"
											"		group_code,			largo_code,		drug_ep_group,	"
											"		parent_group_code,	update_date,	update_time		"
											"	)														";
					NumInputColumns		=	6;
					InputColumnSpec		=	"	int, int, int, int, int, int	";
					GenerateVALUES		=	1;
					break;


		case AS400CL_2517_READ_tables_update:
					SQL_CommandText		=	"	SELECT	update_date,		update_time	"
											"	FROM	tables_update					"
											"	WHERE	table_name	= ?					";
					NumOutputColumns	=	2;
					OutputColumnSpec	=	"	int, int	";
					NumInputColumns		=	1;
					InputColumnSpec		=	"	varchar(32)	";
					break;


		case AS400CL_2517_RENAME_spclty_drug_grp_n_to_current:
					if (Database == &INF_DB)
						SQL_CommandText		=	"	RENAME TABLE spclty_drug_grp_n TO spclty_drug_grp	";
					else
						SQL_CommandText		=	"	EXEC sp_rename 'spclty_drug_grp_n', 'spclty_drug_grp'	";
					break;


		case AS400CL_2517_RENAME_spclty_drug_grp_to_old:
					if (Database == &INF_DB)
						SQL_CommandText		=	"	RENAME TABLE spclty_drug_grp TO spclty_drug_grp_o	";
					else
						SQL_CommandText		=	"	EXEC sp_rename 'spclty_drug_grp', 'spclty_drug_grp_o'	";
					break;


		case AS500CL_2517_SELECT_INTO_temp_pref_largos:
												// DonR 24Sep2020: MS-SQL has a somewhat different syntax for temporary tables:
												// instead of "INTO TEMP", MS-SQL just puts a "#" at the beginning of the table
												// name and otherwise works as normal. Rather than change all the SQL that
												// references temp_pref_largos to add that hash-sign, for now (at least) I'm
												// just creating it as an ordinary table - since we DROP the table explicitly
												// anyway, there shouldn't be any practical difference.
					if (Database == &INF_DB)
						SQL_CommandText		=	"	SELECT DISTINCT	preferred_largo			"						
												"	FROM			drug_list				"
												"	WHERE			preferred_largo > 0		"
												"	  AND			preferred_flg = 1		"
												"	INTO TEMP		temp_pref_largos		";
					else
						SQL_CommandText		=	"	SELECT DISTINCT	preferred_largo			"						
												"	INTO			temp_pref_largos		"
												"	FROM			drug_list				"
												"	WHERE			preferred_largo > 0		"
												"	  AND			preferred_flg = 1		";
					break;


		case AS400CL_2517_UPD_drug_list_calc_op_by_volume_clear:
					SQL_CommandText		=	"	UPDATE		drug_list																					"
											"	SET			calc_op_by_volume	=	0																	"

											"	WHERE		calc_op_by_volume	<>	0																	"
											"	  AND		(			largo_type			NOT IN	('M', 'T')											"
											"					OR		package_size		<>		1													"
											"					OR		package_volume		<		0.0001												"	// Maybe this should be 1.001?
											"					OR		NOT EXISTS	(	SELECT	*														"
											"											FROM	drug_shape												"
											"											WHERE	drug_shape.drug_shape_desc		= drug_list.form_name	"
											"											  AND	drug_shape.calc_op_by_volume	> 0						"
											"										)																	"
											"				)																							";
					break;


		case AS400CL_2517_UPD_drug_list_calc_op_by_volume_set_true:
					SQL_CommandText		=	"	UPDATE		drug_list																	"
											"	SET			calc_op_by_volume	=	1													"

											"	WHERE		calc_op_by_volume	=	0													"
											"	  AND		largo_type			IN	('M', 'T')											"
											"	  AND		package_size		=	1													"
											"	  AND		package_volume		>=	0.0001												"	// Maybe this should be 1.001?
											"	  AND		EXISTS	(	SELECT	*														"
											"							FROM	drug_shape												"
											"							WHERE	drug_shape.drug_shape_desc		= drug_list.form_name	"
											"							  AND	drug_shape.calc_op_by_volume	> 0						"
											"						)																	";
					break;


		case AS400CL_2517_UPD_drug_list_clear_stuff_for_drugs_not_in_economypri:
					SQL_CommandText		=	"	UPDATE	drug_list															"
											"	SET		preferred_largo		= 0,											"
											"			multi_pref_largo	= 0,											"
											"			economypri_group	= 0,											"
											"			economypri_seq		= 0												"
											"	WHERE	largo_code		NOT IN (SELECT DISTINCT	largo_code					"
											"									FROM			economypri					"
									//											WHERE			group_code	< 1000				   DonR 05Nov2014 - 800->1000
									//											WHERE			system_code IN ('0', '1', '4')	   DonR 30Jan2018 - Use System Code instead of a range of Group Codes.
											"									WHERE			send_and_use_pharm	<> 0	"	// DonR 31Jan2018 - use send_and_use_pharm instead of a range of Group Codes OR System Codes.
											"									  AND			del_flg				<> 'D')	"
											"	  AND	((preferred_largo <> 0) OR (economypri_group <> 0))					";
					break;


		case AS400CL_2517_UPD_drug_list_has_diagnoses_flip_false:
					SQL_CommandText		=	"	UPDATE	drug_list															"
											"	SET		has_diagnoses	=		0											"
											"	WHERE	has_diagnoses	=		1											"
											"	  AND	NOT EXISTS (	SELECT	*											"
											"							FROM	drug_diagnoses								"
											"							WHERE	largo_code		= drug_list.largo_code		"
											"							  AND	diagnosis_code	> 0						)	";
					break;


		case AS400CL_2517_UPD_drug_list_has_diagnoses_flip_true:
					SQL_CommandText		=	"	UPDATE	drug_list														"
											"	SET		has_diagnoses	=		1										"
											"	WHERE	has_diagnoses	=		0										"
											"	  AND	EXISTS (	SELECT	*											"
											"						FROM	drug_diagnoses								"
											"						WHERE	largo_code		= drug_list.largo_code		"
											"						  AND	diagnosis_code	> 0						)	";
					break;


		case AS400CL_2517_UPD_drug_list_has_shaban_rules_clear:
					SQL_CommandText		=	"	UPDATE	drug_list																										"

											"	SET		has_shaban_rules =  0																							"

											"	WHERE	has_shaban_rules <> 0																							"
											"	  AND	(NOT EXISTS	(SELECT * FROM spclty_largo_prcnt AS slp	WHERE slp.largo_code		=	drug_list.largo_code	"
											"																	  AND slp.speciality_code	NOT	BETWEEN  4000 AND  4999	"	// Exclude "home visit" stuff
											"																	  AND slp.speciality_code	NOT	BETWEEN 58000 AND 58999	"	// Exclude dentist stuff
											"																	  AND slp.del_flg			=	0						"
											"																	  AND slp.insurance_type	<>	'B'						"
											"																	  AND slp.enabled_mac		>	0)						"

											"				AND																											"

											"				NOT EXISTS (SELECT * FROM drug_extension AS dex		WHERE dex.largo_code		=	drug_list.largo_code	"
											"																	  AND dex.del_flg			=	0						"
											"																	  AND dex.confirm_authority =	0						"
											"																	  AND dex.send_and_use		<>	0						"
											"																	  AND dex.enabled_mac		>	0						"
											"																	  AND dex.insurance_type	<>	'B'						"
											"																	  AND dex.effective_date	<=	?))						";
					NumInputColumns		=	1;
					InputColumnSpec		=	"	int	";
					break;



		case AS400CL_2517_UPD_drug_list_has_shaban_rules_set_true:
					SQL_CommandText		=	"	UPDATE	drug_list																									"

											"	SET		has_shaban_rules =  1																						"

											"	WHERE	has_shaban_rules <> 1																						"
											"	  AND	(EXISTS	(SELECT * FROM spclty_largo_prcnt AS slp	WHERE slp.largo_code		=	drug_list.largo_code	"
											"																  AND slp.speciality_code	NOT	BETWEEN  4000 AND  4999	"	// Exclude "home visit" stuff
											"																  AND slp.speciality_code	NOT	BETWEEN 58000 AND 58999	"	// Exclude dentist stuff
											"																  AND slp.del_flg			=	0						"
											"																  AND slp.insurance_type	<>	'B'						"
											"																  AND slp.enabled_mac		>	0)						"

											"			 OR																											"

											"			 EXISTS (SELECT * FROM drug_extension AS dex		WHERE dex.largo_code		=	drug_list.largo_code	"
											"																  AND dex.del_flg			=	0						"
											"																  AND dex.confirm_authority =	0						"
											"																  AND dex.send_and_use		<>	0						"
											"																  AND dex.enabled_mac		>	0						"
											"																  AND dex.insurance_type	<>	'B'						"
											"																  AND dex.effective_date	<=	?))						";
					NumInputColumns		=	1;
					InputColumnSpec		=	"	int	";
					break;


		case AS400CL_2517_UPD_drug_list_delivery_ok_per_shape_code:
											// DonR 22Sep2020: added "WHERE	drug_list.form_name > '   '" to improve performance.
											// DonR 22Aug2021 User Story #181694: This now UPDATEs delivery_ok_per_shape_code
											// instead of home_delivery_ok; the latter field is now taken directly from RK9001P.
					SQL_CommandText		=	"	UPDATE	drug_list																											"

											"	SET		delivery_ok_per_shape_code	=	(	SELECT	MAX(home_delivery_ok)													"
											"												FROM	drug_shape																"
											"												WHERE	drug_shape.drug_shape_desc	 = drug_list.form_name	)					"

											"	WHERE	drug_list.form_name			>	'   '																				"
											"	  AND	EXISTS							(	SELECT	*																		"
											"												FROM	drug_shape																"
											"												WHERE	drug_shape.drug_shape_desc	 = drug_list.form_name						"
											"												  AND	drug_shape.home_delivery_ok	<> drug_list.delivery_ok_per_shape_code	)	";
					break;


		case AS400CL_2517_UPD_drug_list_multi_pref_largo_clear:
					SQL_CommandText		=	"	UPDATE	drug_list																	"
											"	SET		multi_pref_largo	=		0												"
											"	WHERE	multi_pref_largo	<>		0												"
											"	  AND	preferred_largo		NOT IN	(SELECT preferred_largo FROM temp_pref_largos)	"
											"	  AND	largo_code			NOT IN	(SELECT preferred_largo FROM temp_pref_largos)	";
					break;


		case AS400CL_2517_UPD_drug_list_must_prescribe_qty_clear:
					SQL_CommandText		=	"	UPDATE	drug_list															"

											"	SET		must_prescribe_qty = 0												"

											"	WHERE	must_prescribe_qty = 1												"
											"	  AND	NOT EXISTS	(	SELECT	*											"
											"							FROM	usage_instruct								"
											"							WHERE	shape_num		=  drug_list.shape_code		"
											"							  AND	open_od_window	>  0						"
											"							  AND	del_flg			<> 'D'					)	";
					break;


		case AS400CL_2517_UPD_drug_list_must_prescribe_qty_set_true:
					SQL_CommandText		=	"	UPDATE	drug_list														"

											"	SET		must_prescribe_qty = 1											"

											"	WHERE	must_prescribe_qty = 0											"
											"	  AND	EXISTS	(	SELECT	*											"
											"						FROM	usage_instruct								"
											"						WHERE	shape_num		=  drug_list.shape_code		"
											"						  AND	open_od_window	>  0						"
											"						  AND	del_flg			<> 'D'					)	";
					break;


		case AS400CL_2517_UPD_drug_list_preferred_largo_for_first_of_multiple:
					SQL_CommandText		=	"	UPDATE			drug_list															"
											"	SET				preferred_largo	=  largo_code										"
											"	WHERE			preferred_flg	=  1												"
											"	  AND			preferred_largo	=  0												"
											"	  AND			largo_code		IN (SELECT preferred_largo FROM temp_pref_largos)	";
					break;


		case AS400CL_2517_UPD_drug_list_preferred_largo_for_non_pref_drug:
					SQL_CommandText		=	"	UPDATE	drug_list												"
											"	SET		preferred_largo		=  ?,								"
											"			multi_pref_largo	=  1								"
											"	WHERE	largo_code			=  ?								"
											"	  AND	(preferred_largo	<> ?	OR	multi_pref_largo <> 1)	";
					NumInputColumns		=	3;
					InputColumnSpec		=	"	int, int, int	";
					break;


		case AS400CL_2517_UPD_drug_list_preferred_largo_to_zero:
					SQL_CommandText		=	"	UPDATE	drug_list												"
											"	SET		preferred_largo		=  0,								"
											"			multi_pref_largo	=  1								"
											"	WHERE	largo_code			=  ?								"
											"	  AND	(preferred_largo	<> 0	OR	multi_pref_largo <> 1)	";
					NumInputColumns		=	1;
					InputColumnSpec		=	"	int	";
					break;


		case AS400CL_2517_UPD_drug_list_rule_status_4:
					SQL_CommandText		=	"	UPDATE		drug_list														"
											"	SET			rule_status		= 4												"
											"	WHERE		largo_code	IN													"
											"							(	SELECT DISTINCT	largo_code						"
											"								FROM			drug_extension					"
											"								WHERE			del_flg				=  0		"
											"								  AND			send_and_use		<> 0		"
											"								  AND			confirm_authority	=  8		"
											"								  AND			effective_date		<= ?	)	"
											"	  AND		rule_status	= 0													";
					NumInputColumns		=	1;
					InputColumnSpec		=	"	int	";
					break;


		case AS400CL_2517_UPD_drug_list_set_specialist_drug_to_0:
					SQL_CommandText		=	"	UPDATE drug_list SET specialist_drug = 0 WHERE specialist_drug = 1	";
					break;


		case AS400CL_2517_UPD_drug_list_set_specialist_drug_to_1:
					SQL_CommandText		=	"	UPDATE drug_list SET specialist_drug = 1 WHERE specialist_drug = 2	";
					break;


		case AS400CL_2517_UPD_drug_list_set_specialist_drug_to_2:
					SQL_CommandText		=	"	UPDATE	drug_list												"
											"	SET		specialist_drug	=  2									"
											"	WHERE	largo_code		IN (SELECT DISTINCT	largo_code			"
											"								FROM			spclty_largo_prcnt	"
											"								WHERE			del_flg = 0)		";
					break;


		case AS400CL_2517_UPD_tables_update:
					SQL_CommandText		=	"	UPDATE	tables_update		"
											"	SET		update_date = ?,	"
											"			update_time = ?		"
											"	WHERE	table_name	= ?		";
					NumInputColumns		=	3;
					InputColumnSpec		=	"	int, int, varchar(32)	";
					break;


		case AS400CL_2522_prescription_msgs_cur:
					SQL_CommandText		=	"	SELECT	prescription_id,	largo_code,		line_no,	"
											"			error_code,			severity					"

											"	FROM	prescription_msgs								"

											"	WHERE	reported_to_as400	= ?							"
											"	  AND	delivered_flg		> 0							"
											"	  AND	((time				< ?)	OR (date	<  ?))	";
					NumOutputColumns	=	5;
					OutputColumnSpec	=	"	int, int, short, short, short	";
					NumInputColumns		=	3;
					InputColumnSpec		=	"	short, int, int	";
					CursorName			=	"T2522_pr_msgs_cur";
					break;


		case AS400CL_2522_UPD_prescription_msgs:
					SQL_CommandText		=	"	UPDATE prescription_msgs SET reported_to_as400 = ? WHERE CURRENT OF T2522_pr_msgs_cur	";
					NumInputColumns		=	1;
					InputColumnSpec		=	"	short	";
					StatementIsSticky	=	1;	// DonR 03Mar2021: Possible performance enhancement.
					break;


		case AS400CL_2523_pd_rx_link_cur:
											// DonR 11Dec2024 User Story #373619: Add rx_origin to columns SELECTed.
					SQL_CommandText		=	"	SELECT	prescription_id,		line_no,			visit_number,		"
											"			doctor_presc_id,		largo_prescribed,	largo_sold,			"
											"			valid_from_date,		prev_unsold_op,		prev_unsold_units,	"
											"			op_sold,				units_sold,			sold_drug_op,		"
											"			sold_drug_units,		close_by_rounding,	rx_sold_status,		"
											"			clicks_line_number,		rx_origin								"

											"	FROM	pd_rx_link														"

											"	WHERE	reported_to_as400	= ?											"
											"	  AND	((time				< ?)	OR (date	<  ?))					";
					NumOutputColumns	=	17;
					OutputColumnSpec	=	"	int, short, long, int, int, int, int, int, int, int, int, int, int, short, short, short, short	";
					NumInputColumns		=	3;
					InputColumnSpec		=	"	short, int, int	";
					CursorName			=	"T2523_pd_rx_lnkcur";
					break;


		case AS400CL_2523_UPD_pd_rx_link:
					SQL_CommandText		=	"	UPDATE pd_rx_link SET reported_to_as400 = ? WHERE CURRENT OF T2523_pd_rx_lnkcur	";
					NumInputColumns		=	1;
					InputColumnSpec		=	"	short	";
					StatementIsSticky	=	1;	// DonR 03Mar2021: Possible performance enhancement.
					break;


		case AS400CL_2524_prescription_vouchers_cur:
					SQL_CommandText		=	"	SELECT	prescription_id,			voucher_num,				member_id,				"
											"			mem_id_extension,			voucher_type,				voucher_discount_given,	"
											"			voucher_amount_remaining,	original_voucher_amount,	sold_date				"

											"	FROM	prescription_vouchers															"

											"	WHERE	reported_to_as400	= ?											"
											"	  AND	((time				< ?)	OR (date	<  ?))					";
					NumOutputColumns	=	9;
					OutputColumnSpec	=	"	int, long, int, short, char(15), int, int, int, int	";
					NumInputColumns		=	3;
					InputColumnSpec		=	"	short, int, int	";
					CursorName			=	"T2524_voucher_cur";
					break;


		case AS400CL_2524_UPD_prescription_vouchers:
					SQL_CommandText		=	"	UPDATE prescription_vouchers SET reported_to_as400 = ? WHERE CURRENT OF T2524_voucher_cur	";
					NumInputColumns		=	1;
					InputColumnSpec		=	"	short	";
					StatementIsSticky	=	1;	// DonR 03Mar2021: Possible performance enhancement.
					break;


		// Marianna 14Feb2024 User Story #540234 - pd_cannabis_products
		case AS400CL_2525_pd_cannabis_products_cur:
					SQL_CommandText		=	"	SELECT	prescription_id,			line_no,					cannabis_product_code,			"
											"			cannabis_product_barcode,	group_largo_code,			op,								"
											"			units,						price_per_op,				product_sale_amount,			"
											"			product_ptn_amount																		"

											"	FROM	pd_cannabis_products																	"

											"	WHERE	reported_to_as400	= ?																	"
											"	  AND	((time				< ?)	OR (date	<  ?))											";
					NumOutputColumns	= 10;
					OutputColumnSpec	= "	int, short, int, long, int, short, short, int, int, int	";
					NumInputColumns		= 3;
					InputColumnSpec		= "	short,int, int	";
					CursorName			= "T2525_pd_cp_cur";
					break;


		case AS400CL_2525_UPD_pd_cannabis_products:
					SQL_CommandText		=	"	UPDATE pd_cannabis_products SET reported_to_as400 = ? WHERE CURRENT OF T2525_pd_cp_cur	";
					NumInputColumns		= 1;
					InputColumnSpec		= "	short	";
					StatementIsSticky	= 1;
					break;


		case AS400CL_as400clientpar_cur:
					SQL_CommandText		=	"	SELECT		{transaction},		dim,				seq_number,			"
											"				frequency_min,		always_enabled,		period_1_start,		"
											"				period_1_end,		period_2_start,		period_2_end,		"
											"				period_3_start,		period_3_end							"

											"	FROM		as400clientpar												"

											"	WHERE		system = 1													"

											"	ORDER BY	seq_number													";
					NumOutputColumns	=	11;
					OutputColumnSpec	=	"	int, int, int, int, short, int, int, int, int, int, int	";
					break;


		case AS400CL_del_queue_cur:
											// DonR 14Jun2022: Added ORDER BY so we read the "header" row (with largo_code = 0) last.
											// We need this because we update the header's deletion flag based on whether all the
											// sale's drug lines have been marked as deleted.
					SQL_CommandText		=	"	SELECT prescription_id, largo_code FROM	drugsale_del_queue ORDER BY prescription_id, largo_code DESC	";
					NumOutputColumns	=	2;
					OutputColumnSpec	=	"	int, int	";
					CursorName			=	"del_queue_cur";
					break;


		case AS400CL_DEL_del_queue:
					SQL_CommandText		=	"	DELETE FROM drugsale_del_queue WHERE CURRENT OF del_queue_cur	";
					break;


		case AS400CL_drx_status_queue_cur:
					SQL_CommandText		=	"	SELECT	member_id,				member_id_code,			doctor_id,				visit_number,		"
											"			doctor_presc_id,		line_number,			largo_prescribed,		valid_from_date,	"
											"			sold_status,			close_by_rounding,		upd_units_sold,			upd_op_sold,		"

											"			last_sold_date,			last_sold_time,			linux_update_flag,		linux_update_date,	"
											"			linux_update_time,		queued_date,			queued_time,			reported_to_cds		"

											"	FROM	drx_status_queue																			";
					NumOutputColumns	=	20;
					OutputColumnSpec	=	"	int, short, int, long, int, short, int, int, short, short, short, short,	"
											"	int, int, short, int, int, int, int, short									";
					CursorName			=	"drx_status_q_cur";
					break;
				

		case AS400CL_DEL_drx_status_queue:
					SQL_CommandText		=	"	DELETE FROM drx_status_queue WHERE CURRENT OF drx_status_q_cur	";
					break;


		case AS400CL_READ_sysparams:
					SQL_CommandText		=	"	SELECT minpharmacytoas400 FROM sysparams	";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	short	";
					break;


		case AS400CL_UPD_as400clientpar_after_processing:
					SQL_CommandText		=	"	UPDATE	as400clientpar						"
											"	SET		seq_number = seq_number - 100000	"
											"	WHERE	{transaction}	=  ?				"	
											"	  AND	system			=  1				"
											"	  AND	seq_number		>= 100000			";
					NumInputColumns		=	1;
					InputColumnSpec		=	"	int	";
					break;


		case AS400CL_UPD_as400clientpar_while_processing:
					SQL_CommandText		=	"	UPDATE	as400clientpar						"
											"	SET		seq_number = seq_number + 100000	"
											"	WHERE	{transaction}	= ?					"
											"	  AND	system			= 1					"
											"	  AND	seq_number		< 100000			";
					NumInputColumns		=	1;
					InputColumnSpec		=	"	int	";
					break;


		case AS400CL_UPD_del_queue_prescription_drugs:
					SQL_CommandText		=	"	UPDATE	prescription_drugs		"
											"	SET		del_flg			= 1		"
											"	WHERE	prescription_id	= ?		"
											"	  AND	largo_code		= ?		"
											"	  AND	del_flg			= 0		";
					NumInputColumns		=	2;
					InputColumnSpec		=	"	int, int	";
					break;


		case AS400CL_UPD_del_queue_pd_rx_link:
					SQL_CommandText		=	"	UPDATE	pd_rx_link				"
											"	SET		del_flg			= 1		"
											"	WHERE	prescription_id	= ?		"
											"	  AND	largo_sold		= ?		"
											"	  AND	del_flg			= 0		";
					NumInputColumns		=	2;
					InputColumnSpec		=	"	int, int	";
					break;


		case AS400CL_UPD_del_queue_prescriptions:
					SQL_CommandText		=	"	UPDATE	prescriptions												"

											"	SET		del_flg = 1													"

											"	WHERE	prescription_id		= ?										"
											"	  AND	del_flg				= 0										"
											"	  AND	NOT EXISTS			(	SELECT	*							"
											"									FROM	prescription_drugs AS pd	"
											"									WHERE	pd.prescription_id	= ?		"
											"									  AND	pd.delivered_flg	= 1		"	// Only drug sales are relevant here.
											"									  AND	pd.del_flg			= 0	)	";
					NumInputColumns		=	2;
					InputColumnSpec		=	"	int, int	";
					break;


		case AS400CL_UPD_doctor_presc:
					SQL_CommandText		=	"	UPDATE	doctor_presc									"

											"	SET		sold_status			= ?,						"
											"			close_by_rounding	= ?,						"
											"			last_sold_date		= ?,						"
											"			last_sold_time		= ?,						"
											"			tot_units_sold		= (tot_units_sold	+ ?),	"
											"			tot_op_sold			= (tot_op_sold		+ ?),	"
											"			linux_update_flag	= ?,						"
											"			linux_update_date	= ?,						"
											"			linux_update_time	= ?,						"
											//			reported_to_as400	= 0,
											"			reported_to_cds		= ?							"

											"	WHERE	visit_number		= ?							"
											"	  AND	doctor_presc_id		= ?							"
											"	  AND	line_number			= ?							";
					NumInputColumns		=	13;
					InputColumnSpec		=	"	short, short, int, int, short, short, short, int, int, short, long, int, short	";
					break;

