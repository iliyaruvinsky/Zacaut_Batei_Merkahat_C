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


		case AS400SRV_INS_as400in_audit:
					SQL_CommandText		=	"	INSERT INTO	as400in_audit						"
											"	(												"
											"		trn_id,			from_table,		to_table,	"
											"		date,			time,			err_code,	"
											"		msg_count									"
											"	)												";
					NumInputColumns		=	7;
					InputColumnSpec		=	"	int, char(30), char(30), int, int, int, int	";
					GenerateVALUES		=	1;
					break;


		case AS400SRV_INS_tables_update:
					SQL_CommandText		=	"	INSERT INTO tables_update ( table_name, update_date, update_time )	";
					NumInputColumns		=	3;
					InputColumnSpec		=	"	varchar(32), int, int	";
					GenerateVALUES		=	1;
					break;


		case AS400SRV_READ_as400in_audit_table_size_control_columns:
					SQL_CommandText		=	"	SELECT	active_rows, max_grow_percent, max_shrink_percent	"
											"	FROM	as400in_audit										"
											"	WHERE	trn_id	= ?											";
					NumOutputColumns	=	3;
					OutputColumnSpec	=	"	int, short, short	";
					NumInputColumns		=	1;
					InputColumnSpec		=	"	int	";
					break;


		case AS400SRV_READ_member_price_ptn_percent_sort:
					SQL_CommandText		=	"	SELECT	ptn_percent_sort		"
											"	FROM	member_price			"
											"	WHERE	member_price_code = ?	";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	short	";
					NumInputColumns		=	1;
					InputColumnSpec		=	"	short	";
					break;


		case AS400SRV_READ_sysparams_svc_without_card_params:
					SQL_CommandText		=	"	SELECT	 nocard_ph_maxvalid, svc_w_o_card_days FROM	sysparams	";
					NumOutputColumns	=	2;
					OutputColumnSpec	=	"	int, short	";
					break;


		case AS400SRV_READ_tables_update:
					SQL_CommandText		=	"	SELECT	update_date,		update_time	"
											"	FROM	tables_update					"
											"	WHERE	table_name	= ?					";
					NumOutputColumns	=	2;
					OutputColumnSpec	=	"	int, int	";
					NumInputColumns		=	1;
					InputColumnSpec		=	"	varchar(32)	";
					break;


		case AS400SRV_READ_todays_update_size_clicks_discount:
					SQL_CommandText		=	"	SELECT	CAST (count(*) AS INTEGER)	FROM clicks_discount	WHERE	update_date		= ?	";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					NumInputColumns		=	1;
					InputColumnSpec		=	"	int	";
					Convert_NF_to_zero	=	1;
					break;


		case AS400SRV_READ_todays_update_size_drug_extn_doc:
					SQL_CommandText		=	"	SELECT	CAST (count(*) AS INTEGER)	FROM drug_extn_doc		WHERE	update_date		= ?	";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					NumInputColumns		=	1;
					InputColumnSpec		=	"	int	";
					Convert_NF_to_zero	=	1;
					break;


		case AS400SRV_READ_todays_update_size_drug_forms:
					SQL_CommandText		=	"	SELECT	CAST (count(*) AS INTEGER)	FROM drug_forms			WHERE	update_date		= ?	";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					NumInputColumns		=	1;
					InputColumnSpec		=	"	int	";
					Convert_NF_to_zero	=	1;
					break;


		case AS400SRV_READ_todays_update_size_drug_list:
					SQL_CommandText		=	"	SELECT	CAST (count(*) AS INTEGER)	FROM drug_list			WHERE	update_date_d	= ?	";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					NumInputColumns		=	1;
					InputColumnSpec		=	"	int	";
					Convert_NF_to_zero	=	1;
					break;


		case AS400SRV_READ_todays_update_size_drugdoctorprof:
					SQL_CommandText		=	"	SELECT	CAST (count(*) AS INTEGER)	FROM drugdoctorprof		WHERE	update_date		= ?	";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					NumInputColumns		=	1;
					InputColumnSpec		=	"	int	";
					Convert_NF_to_zero	=	1;
					break;


		case AS400SRV_READ_todays_update_size_druggencomponents:
					SQL_CommandText		=	"	SELECT	CAST (count(*) AS INTEGER)	FROM druggencomponents	WHERE	update_date		= ?	";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					NumInputColumns		=	1;
					InputColumnSpec		=	"	int	";
					Convert_NF_to_zero	=	1;
					break;


		case AS400SRV_READ_todays_update_size_drugnotes:
					SQL_CommandText		=	"	SELECT	CAST (count(*) AS INTEGER)	FROM drugnotes			WHERE	update_date		= ?	";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					NumInputColumns		=	1;
					InputColumnSpec		=	"	int	";
					Convert_NF_to_zero	=	1;
					break;


		case AS400SRV_READ_todays_update_size_economypri:
					SQL_CommandText		=	"	SELECT	CAST (count(*) AS INTEGER)	FROM economypri			WHERE	update_date		= ?	";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					NumInputColumns		=	1;
					InputColumnSpec		=	"	int	";
					Convert_NF_to_zero	=	1;
					break;


		case AS400SRV_READ_todays_update_size_gencomponents:
					SQL_CommandText		=	"	SELECT	CAST (count(*) AS INTEGER)	FROM gencomponents		WHERE	update_date		= ?	";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					NumInputColumns		=	1;
					InputColumnSpec		=	"	int	";
					Convert_NF_to_zero	=	1;
					break;


		case AS400SRV_READ_todays_update_size_generyinteraction:
					SQL_CommandText		=	"	SELECT	CAST (count(*) AS INTEGER)	FROM generyinteraction	WHERE	update_date		= ?	";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					NumInputColumns		=	1;
					InputColumnSpec		=	"	int	";
					Convert_NF_to_zero	=	1;
					break;


		case AS400SRV_READ_todays_update_size_geninternotes:
					SQL_CommandText		=	"	SELECT	CAST (count(*) AS INTEGER)	FROM geninternotes		WHERE	update_date		= ?	";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					NumInputColumns		=	1;
					InputColumnSpec		=	"	int	";
					Convert_NF_to_zero	=	1;
					break;


		case AS400SRV_READ_todays_update_size_gnrldrugnotes:
					SQL_CommandText		=	"	SELECT	CAST (count(*) AS INTEGER)	FROM gnrldrugnotes		WHERE	update_date		= ?	";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					NumInputColumns		=	1;
					InputColumnSpec		=	"	int	";
					Convert_NF_to_zero	=	1;
					break;


		case AS400SRV_READ_todays_update_size_pharmacology:
					SQL_CommandText		=	"	SELECT	CAST (count(*) AS INTEGER)	FROM pharmacology		WHERE	update_date		= ?	";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					NumInputColumns		=	1;
					InputColumnSpec		=	"	int	";
					Convert_NF_to_zero	=	1;
					break;


		case AS400SRV_READ_todays_update_size_presc_period:
					SQL_CommandText		=	"	SELECT	CAST (count(*) AS INTEGER)	FROM presc_period		WHERE	update_date		= ?	";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					NumInputColumns		=	1;
					InputColumnSpec		=	"	int	";
					Convert_NF_to_zero	=	1;
					break;


		case AS400SRV_READ_todays_update_size_treatment_group:
					SQL_CommandText		=	"	SELECT	CAST (count(*) AS INTEGER)	FROM treatment_group	WHERE	update_date		= ?	";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					NumInputColumns		=	1;
					InputColumnSpec		=	"	int	";
					Convert_NF_to_zero	=	1;
					break;


		case AS400SRV_READ_todays_update_size_usage_instruct:
					SQL_CommandText		=	"	SELECT	CAST (count(*) AS INTEGER)	FROM usage_instruct		WHERE	update_date		= ?	";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					NumInputColumns		=	1;
					InputColumnSpec		=	"	int	";
					Convert_NF_to_zero	=	1;
					break;


		case AS400SRV_UPD_as400in_audit_general_columns:
					SQL_CommandText		=	"	UPDATE	as400in_audit		"
											"	SET		from_table	= ?,	"
											"			to_table	= ?,	"
											"			date		= ?,	"
											"			time		= ?,	"
											"			err_code	= ?,	"
											"			msg_count	= ?		"
											"	WHERE	trn_id		= ?		";
					NumInputColumns		=	7;
					InputColumnSpec		=	"	char(30), char(30), int, int, int, int, int	";
					break;


		case AS400SRV_UPD_as400in_audit_table_size_columns:
					SQL_CommandText		=	"	UPDATE	as400in_audit		"
											"	SET		total_rows	= ?,	"
											"			active_rows	= ?		"
											"	WHERE	trn_id		= ?		";
					NumInputColumns		=	3;
					InputColumnSpec		=	"	int, int, int	";
					break;


		case AS400SRV_UPD_tables_update:
					SQL_CommandText		=	"	UPDATE	tables_update		"
											"	SET		update_date = ?,	"
											"			update_time = ?		"
											"	WHERE	%s					";
					NumInputColumns		=	2;
					InputColumnSpec		=	"	int, int	";
					NeedsWhereClauseID	=	1;
					break;


		case AS400SRV_T2001_INS_pharmacy:
					SQL_CommandText		=	"	INSERT INTO pharmacy													"
											"	(																		"
											"		pharmacy_code,			institued_code,			pharmacy_name,		"
											"		pharmacist_last_na,		pharmacist_first_n,		street_and_no,		"
											"		city,					phone,					owner,				"
											"		hesder_category,		comm_type,				phone_1,			"

											"		phone_2,				phone_3,				permission_type,	"
											"		price_list_code,		update_date,			update_time,		"
											"		software_ver_need,		open_close_flag,		del_flg,			"
											"		term_address,			term_quantity,			card,				"

											"		fps_fee_level,			fps_fee_lower,			fps_fee_upper,		"
											"		credit,					contract_type,			contract_incl_tax,	"
											"		contract_code,			contract_effective,		maccabicare_flag,	"
											"		meishar_enabled,		can_sell_future_rx,		pharm_category,		"

											"		web_pharmacy_code,		vat_exempt,				software_house,		"
											"		order_originator,		order_fulfiller,							"
											"		ConsignmentPharmacy,	PharmNohalEnabled,		mahoz				"	// Marianna 22May2024 User Story #314887
											"	 )																		";
					NumInputColumns		=	44; // Marianna changed from 43 to 44 User Story #314887
					InputColumnSpec		=	"	int, short, varchar(30), char(14), char(8), varchar(20), varchar(20), varchar(10), short, short, short, varchar(10),	"
											"	varchar(10), varchar(10), short, short, int, int, char(9), short, short, int, short, short,								"
											"	int, int, int, short, short, short, int, int, short, short, short, short,												"
											"	int, short, short, short, short, short, short, short																	";
					GenerateVALUES		=	1;
					break;


		case AS400SRV_T2001_UPD_pharmacy:
					SQL_CommandText		=	"	UPDATE	pharmacy					"
											"	SET		institued_code		= ?,	"
											"			pharmacy_name		= ?,	"
											"			pharmacist_last_na	= ?,	"
											"			pharmacist_first_n	= ?,	"
											"			street_and_no		= ?,	"
											"			city				= ?,	"
											"			phone				= ?,	"
											"			owner				= ?,	"
											"			hesder_category		= ?,	"
											"			comm_type			= ?,	"

											"			phone_1				= ?,	"
											"			phone_2				= ?,	"
											"			phone_3				= ?,	"
											"			permission_type		= ?,	"
											"			price_list_code		= ?,	"
											"			software_ver_need	= ?,	"
											"			update_date			= ?,	"
											"			update_time			= ?,	"
											"			card				= ?,	"
											"			fps_fee_level		= ?,	"

											"			fps_fee_lower		= ?,	"
											"			fps_fee_upper		= ?,	"
											"			credit				= ?,	"
											"			contract_type		= ?,	"
											"			contract_incl_tax	= ?,	"
											"			contract_code		= ?,	"
											"			contract_effective	= ?,	"
											"			maccabicare_flag	= ?,	"
											"			meishar_enabled		= ?,	"
											"			can_sell_future_rx	= ?,	"

											"			pharm_category		= ?,	"
											"			web_pharmacy_code	= ?,	"
											"			vat_exempt			= ?,	"
											"			software_house		= ?,	"
											"			order_originator	= ?,	"
											"			order_fulfiller		= ?,	"
											"			ConsignmentPharmacy	= ?,	"	// Marianna 20Apr2023 User Story #432608
											"			PharmNohalEnabled	= ?,	"	// Marianna 20Apr2023 User Story #432608
											"			mahoz				= ?		"	// Marianna 22May2024 User Story #314887

											"	WHERE	pharmacy_code	= ?			"
											"	  AND	del_flg			= ?			";
					NumInputColumns		=	41; // Marianna 01May2023 changed from 40 to 41 User Story #314887
					InputColumnSpec		=	"	short, varchar(30), char(14), char(8), varchar(20), varchar(20), varchar(10), short, short, short,	"
											"	varchar(10), varchar(10), varchar(10), short, short, char(9), int, int, short, int,					"
											"	int, int, short, short, short, int, int, short, short, short,										"
											"	short, int, short, short, short, short, short, short, short											"
											"	int, short																							";
					break;


		case AS400SRV_T2002_DEL_pharmacy:
					SQL_CommandText		=	"	DELETE FROM pharmacy WHERE pharmacy_code = ? AND del_flg = ?	";
					NumInputColumns		=	2;
					InputColumnSpec		=	"	int, short	";
					break;


		case AS400SRV_T2003_INS_special_prescs:
					SQL_CommandText		=	"	INSERT INTO special_prescs													"
											"	(																			"
											"		pharmacy_code		,	institued_code		,	member_id			,	"
											"		mem_id_extension	,	doctor_id_type		,	doctor_id			,	"
											"		special_presc_num	,	spec_pres_num_sors	,	ishur_source_send	,	"
											"		member_price_code	,	largo_code			,	op_in_dose			,	"

											"		units_in_dose		,	treatment_start		,	stop_use_date		,	"
											"		treatment_stop		,	authority_id		,	confirm_authority_	,	"
											"		presc_source		,	insurance_used		,	confirm_reason		,	"
											"		dose_num			,	dose_renew_freq		,	priv_pharm_sale_ok	,	"

											"		confirmation_type	,	hospital_code		,	date				,	"
											"		time				,	const_sum_to_pay	,	in_health_pack		,	"
											"		health_basket_new	,	treatment_category	,	mac_magen_code		,	"
											"		keren_mac_code		,	macabi_code			,	stop_use_flg		,	"

											"		reported_to_as400	,	pharmacist_id		,	pharmacist_id_ext	,	"
											"		phrm_card_date		,	error_code			,	terminal_id			,	"
											"		del_flg				,	updated_by			,	message_code		,	"
											"		message_text		,	country_center		,	reason_code			,	"

											"		rejection_date		,	doc_has_seen_ishur	,	portal_update_date	,	"
											"		abrupt_close_flg	,	qty_lim_ingr		,	qty_lim_flg			,	"
											"		qty_lim_units		,	qty_lim_qty_each	,	qty_lim_per_day		,	"
											"		qty_lim_treat_days	,	qty_lim_course_len	,	qty_lim_courses		,	"

											"		qty_lim_all_ishur	,	qty_lim_all_warn	,	qty_lim_all_err		,	"
											"		qty_lim_per_course	,	qty_lim_course_wrn	,	qty_lim_course_err	,	"
											"		qlm_all_ishur_std	,	qlm_all_warn_std	,	qlm_all_err_std		,	"
											"		qlm_per_course_std	,	qlm_course_wrn_std	,	qlm_course_err_std	,	"

											"		form_number			,	tikra_flag          ,	tikra_type_code		,	"
											"		qlm_per_day_std		,	qlm_history_start	,	needs_29_g			,	"
											"		member_diagnosis	,	monthly_qlm_flag	,	monthly_dosage		,	"	// Marianna 06Aug2020 CR #28605/27955: added 6 columns.
											"		monthly_dosage_std	,	monthly_duration	,	cont_approval_flag	,	"
											
											"		orig_confirm_num	,	exceptional_ishur								"	// Marianna 06Aug2020 end (orig_confirm_num). 19Jan2023 User Story #276372
											"	)																			";
					NumInputColumns		=	86;																					// Marianna 06Aug2020 CR #28605/27955: added 6 columns.19Jan2023 User Story 
																																// #276372 add 1 column.
					InputColumnSpec		=	"	int, short, int, short, short, int, int, short, short, short, int, int,							"
											"	int, int, int, int, int, short, short, short, short, short, short, short,						"
											"	short, int, int, int, int, short, short, short, short, short, short, short,						"
											"	short, int, short, int, short, short, short, int, int, varchar(60), short, short,				"
											"	int, short, int, short, short, short, char(3), double, double, short, short, short,				"
											"	double, double, double, double, double, double, double, double, double, double, double, double,	"
											"	short, short, short, double, int, short, int, short, double, double, short, short,				"	// Marianna 06Aug2020 +5 columns
											"	int, short																						";	// Marianna 06Aug2020 +1 column, 19Jan2023 User Story #276372
					GenerateVALUES		=	1;
					StatementIsSticky	=	1;	// DonR 03Mar2021: Possible performance enhancement.
					break;


		case AS400SRV_T2003_UPD_members_spec_presc:
					SQL_CommandText		=	"	UPDATE	members					"
											"	SET		spec_presc = 1			"
											"	WHERE	member_id			= ?	"
											"	  AND	mem_id_extension	= ?	";
					NumInputColumns		=	2;
					InputColumnSpec		=	"	int, short	";
					CommandIsMirrored	=	0;	// DonR 29Jul2025 - Turning this off, as we're pretty much letting Informix die.
					StatementIsSticky	=	1;	// DonR 03Mar2021: Possible performance enhancement.
					break;


		case AS400SRV_T2003_UPD_special_prescs:
					SQL_CommandText		=	"	UPDATE	special_prescs				"

											"	SET		pharmacy_code		= ?,	"
											"			institued_code		= ?,	"
											"			doctor_id_type		= ?,	"
											"			doctor_id			= ?,	"
											"			member_price_code	= ?,	"
											"			largo_code			= ?,	"
											"			op_in_dose			= ?,	"
											"			units_in_dose		= ?,	"
											"			treatment_start		= ?,	"
											"			stop_use_date		= ?,	"

											"			treatment_stop		= ?,	"
											"			authority_id		= ?,	"
											"			confirm_authority_	= ?,	"
											"			presc_source		= ?,	"
											"			insurance_used		= ?,	"
											"			confirm_reason		= ?,	"
											"			dose_num			= ?,	"
											"			dose_renew_freq		= ?,	"
											"			priv_pharm_sale_ok	= ?,	"
											"			confirmation_type	= ?,	"

											"			hospital_code		= ?,	"
											"			const_sum_to_pay	= ?,	"
											"			date				= ?,	"
											"			time				= ?,	"
											"			in_health_pack		= ?,	"
											"			health_basket_new	= ?,	"
											"			treatment_category	= ?,	"
											"			message_code		= ?,	"
											"			rejection_date		= ?,	"
											"			message_text		= ?,	"

											"			country_center		= ?,	"
											"			reason_code			= ?,	"
											"			doc_has_seen_ishur	= ?,	"
											"			portal_update_date	= ?,	"
											"			abrupt_close_flg	= ?,	"
											"			qty_lim_ingr		= ?,	"
											"			qty_lim_flg			= ?,	"
											"			qty_lim_units		= ?,	"
											"			qty_lim_qty_each	= ?,	"
											"			qty_lim_per_day		= ?,	"

											"			qty_lim_treat_days	= ?,	"
											"			qty_lim_course_len	= ?,	"
											"			qty_lim_courses		= ?,	"
											"			qty_lim_all_ishur	= ?,	"
											"			qty_lim_all_warn	= ?,	"
											"			qty_lim_all_err		= ?,	"
											"			qty_lim_per_course	= ?,	"
											"			qty_lim_course_wrn	= ?,	"
											"			qty_lim_course_err	= ?,	"
											"			qlm_all_ishur_std	= ?,	"

											"			qlm_all_warn_std	= ?,	"
											"			qlm_all_err_std		= ?,	"
											"			qlm_per_course_std	= ?,	"
											"			qlm_course_wrn_std	= ?,	"
											"			qlm_course_err_std	= ?,	"
											"			qlm_per_day_std		= ?,	"
											"			qlm_history_start	= ?,	"
											"			form_number			= ?,	"
											"			tikra_flag			= ?,	"
											"			tikra_type_code		= ?,	"

											"			ishur_source_send	= ?,	"
											"			needs_29_g			= ?,	"
											"			member_diagnosis	= ?,	"
											"			del_flg				= ?,	"
											"			monthly_qlm_flag	= ?,	"	// Marianna 06Aug2020 CR #28605/27955: added 6 columns.
											"			monthly_dosage		= ?,	"
											"			monthly_dosage_std	= ?,	"
											"			monthly_duration	= ?,	"
											"			cont_approval_flag	= ?,	"
											"			orig_confirm_num	= ?,	"	// Marianna 06Aug2020 CR #28605/27955 end.
											"			exceptional_ishur	= ?		"	// Marianna 19Jan2023 User Story #276372

												// DonR 01May2012 - Ishur Source is no longer part of the unique key for special_prescs.
											"	WHERE	%s							";
					NumInputColumns		=	71;											// Marianna 06Aug2020 CR #28605/27955: added 6 columns. 19Jan2023 User Story #276372 add 1 column
					InputColumnSpec		=	"	int, short, short, int, short, int, int, int, int, int,							"
											"	int, int, short, short, short, short, short, short, short, short,				"
											"	int, int, int, int, short, short, short, int, int, varchar(60),					"
											"	short, short, short, int, short, short, short, char(3), double, double,			"
											"	short, short, short, double, double, double, double, double, double, double,	"
											"	double, double, double, double, double, double, int, short, short, short,		"
											"	short, short, int, short, short, double, double, short, short, int,				"	// Marianna 06Aug2020 CR #28605/27955: added 6 columns.
											"	short																			";	// Marianna 19Jan2023 User Story #276372
					NeedsWhereClauseID	=	1;
					StatementIsSticky	=	1;	// DonR 03Mar2021: Possible performance enhancement.
					break;


		case AS400SRV_T2004_DEL_special_prescs:	// NOTE - This uses the same custom WHERE clauses
												// as AS400SRV_T2003_UPD_special_prescs.
					SQL_CommandText		=	"	DELETE FROM special_prescs WHERE %s	";
					NeedsWhereClauseID	=	1;
					break;


		case AS400SRV_T2006_DEL_doctor_percents:
					SQL_CommandText		=	"	DELETE FROM doctor_percents WHERE largo_code = ?	";
					NumInputColumns		=	1;
					InputColumnSpec		=	"	int	";
					break;


		case AS400SRV_T2006_DEL_drug_extension:
					SQL_CommandText		=	"	DELETE FROM drug_extension WHERE largo_code = ?	";
					NumInputColumns		=	1;
					InputColumnSpec		=	"	int	";
					break;


		case AS400SRV_T2006_DEL_drug_interaction:
					SQL_CommandText		=	"	DELETE FROM drug_interaction WHERE largo_code = ?	";
					NumInputColumns		=	1;
					InputColumnSpec		=	"	int	";
					break;


		case AS400SRV_T2006_DEL_drug_list:
					SQL_CommandText		=	"	DELETE FROM drug_list WHERE largo_code = ?	";
					NumInputColumns		=	1;
					InputColumnSpec		=	"	int	";
					break;


		case AS400SRV_T2006_DEL_over_dose:
					SQL_CommandText		=	"	DELETE FROM over_dose WHERE largo_code = ?	";
					NumInputColumns		=	1;
					InputColumnSpec		=	"	int	";
					break;


		case AS400SRV_T2006_DEL_price_list:
					SQL_CommandText		=	"	DELETE FROM price_list WHERE largo_code = ?	";
					NumInputColumns		=	1;
					InputColumnSpec		=	"	int	";
					break;


		case AS400SRV_T2006_DEL_spclty_largo_prcnt:
					SQL_CommandText		=	"	DELETE FROM spclty_largo_prcnt WHERE largo_code = ?	";
					NumInputColumns		=	1;
					InputColumnSpec		=	"	int	";
					break;


		case AS400SRV_T2010_DEL_interaction_type:
					SQL_CommandText		=	"	DELETE FROM interaction_type WHERE interaction_type = ?	";
					NumInputColumns		=	1;
					InputColumnSpec		=	"	short	";
					break;


		case AS400SRV_T2012_DEL_drug_interaction:
					SQL_CommandText		=	"	DELETE FROM drug_interaction	"
											"	WHERE   first_largo_code  = ?	"
											"	  AND   second_largo_code = ?	";
					NumInputColumns		=	2;
					InputColumnSpec		=	"	int, int	";
					break;


		case AS400SRV_T2014_DEL_doctors:
					SQL_CommandText		=	"	DELETE FROM doctors WHERE doctor_id = ?	";
					NumInputColumns		=	1;
					InputColumnSpec		=	"	int	";
					break;


		case AS400SRV_T2016_DEL_speciality_types:
					SQL_CommandText		=	"	DELETE FROM speciality_types WHERE speciality_code = ?	";
					NumInputColumns		=	1;
					InputColumnSpec		=	"	int	";
					break;


		case AS400SRV_T2018_DEL_doctor_speciality:
					SQL_CommandText		=	"	DELETE FROM	doctor_speciality		"
											"	WHERE		doctor_id		= ?		"
											"	  AND		speciality_code	= ?		";
					NumInputColumns		=	2;
					InputColumnSpec		=	"	int, int	";
					break;


		case AS400SRV_T2020_DEL_spclty_largo_prcnt:
					SQL_CommandText		=	"	DELETE FROM spclty_largo_prcnt	"

											"	WHERE	speciality_code	= ?		"
											"	  AND	largo_code		= ?		"
											"	  AND	enabled_status	= ?		"
											"	  AND	insurance_type	= ?		";
					NumInputColumns		=	4;
					InputColumnSpec		=	"	int, int, short, char(1)	";
					break;


		case AS400SRV_T2022_DEL_doctor_percents:
					SQL_CommandText		=	"	DELETE FROM doctor_percents WHERE doctor_id = ? AND largo_code = ?	";
					NumInputColumns		=	2;
					InputColumnSpec		=	"	int, int	";
					break;


		case AS400SRV_T2025_DEL_conmemb:
					SQL_CommandText		=	"	DELETE FROM	conmemb				"
											"	WHERE		IdNumber	 = ?	"
											"	  AND		IdCode		 = ?	"
											"	  AND		LinkType	!= ?	";
					NumInputColumns		=	3;
					InputColumnSpec		=	"	int, short, short	";
					StatementIsSticky	=	1;	// DonR 03Mar2021: Possible performance enhancement.
					break;


		case AS400SRV_T2025_INS_conmemb:
					SQL_CommandText		=	"	INSERT INTO conmemb											"
											"	(															"
											"		IdNumber,			IdCode,				ConSeq,			"
											"		Contractor,			ContractType,		Profession,		"
											"		LinkType,			ValidFrom,			ValidUntil,		"
											"		LastUpdateDate,		LastUpdateTime,		UpdatedBy,		"
											"		ConMembNote												"
											"	)															";
					NumInputColumns		=	13;
					InputColumnSpec		=	"	int, short, short, int, short, short, short, int, int, int, int, short, char(65)	";
					GenerateVALUES		=	1;
					break;


		case AS400SRV_T2025_INS_members:
											// DonR 29Jul2025 User Story #435969: Added new dangerous_drug_status
											// column. I left Old Member TZ Code alone, even though it's no longer
											// in use and now just gets a hard-coded zero from the calling function.
					SQL_CommandText		=	"	INSERT INTO members														"
											"	(																		"
											"		member_id,				mem_id_extension,		card_date,			"
											"		father_name,			first_name,				last_name,			"
											"		street,					house_num,				city,				"
											"		phone,					zip_code,				sex,				"
											"		date_of_bearth,			marital_status,			maccabi_code,		"

											"		maccabi_until,			mac_magen_code,			mac_magen_from,		"
											"		mac_magen_until,		keren_mac_code,			keren_mac_from,		"
											"		keren_mac_until,		keren_wait_flag,		asaf_code,			"
											"		insurance_status,		doctor_status,			credit_type_code,	"
											"		member_discount_pt,		old_member_id,			old_id_extension,	"

											"		update_date,			update_time,			updated_by,			"
											"		del_flg,				idnumber_main,			idcode_main,		"
											"		updateadr,				last_english,			first_english,		"
											"		spec_presc,				data_update_date,		data_update_time,	"
											"		mess,					snif,					has_tikra,			"

											"		has_coupon,				in_hospital,			yahalom_code,		"
											"		yahalom_from,			yahalom_until,			carry_over_vetek,	"
											"		insurance_type,			illness_1,				illness_2,			"
											"		illness_3,				illness_4,				illness_5,			"
											"		illness_6,				illness_7,				illness_8,			"

											"		illness_9,				illness_10,				illness_bitmap,		"
											"		check_od_interact,		force_100_percent_ptn,	darkonai_no_card,	"
											"		darkonai_type,			has_blocked_drugs,		died_in_hospital,	"
											"		dangerous_drug_status														"
											"	)																		";
					NumInputColumns		=	70;	// DonR 29Jul2025 User Story #435969: 69->70.
					InputColumnSpec		=	"	int, short, short, varchar(8), varchar(8), varchar(14), varchar(12), char(4), varchar(20), varchar(10), int, short, int, short, short,	"
											"	int, short, int, int, short, int, int, short, short, short, int, short, short, int, short,												"
											"	int, int, int, short, int, short, int, varchar(15), varchar(15), short, int, int, int, int, short,										"
											"	short, short, short, int, int, int, char(1), short, short, short, short, short, short, short, short,									"
											"	short, short, int, short, short, short, short, short, short, short																		";
					GenerateVALUES		=	1;
					CommandIsMirrored	=	0;	// DonR 29Jul2025 - Turning this off, as we're pretty much letting Informix die.
					break;


		case AS400SRV_T2025_READ_blocked_drugs_count:
					SQL_CommandText		=	"	SELECT	CAST (count(*) AS INTEGER)	"
											"	FROM	member_blocked_drugs		"
											"	WHERE	member_id			= ?		"
											"	  AND	mem_id_extension	= ?		";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					NumInputColumns		=	2;
					InputColumnSpec		=	"	int, short	";
					Convert_NF_to_zero	=	1;
					break;


		case AS400SRV_T2025_READ_coupons_count:
					SQL_CommandText		=	"	SELECT	CAST (count(*) AS INTEGER)	"
											"	FROM	coupons						"
											"	WHERE	member_id			= ?		"
											"	  AND	mem_id_extension	= ?		"
											"	  AND	del_flg				= ?		";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					NumInputColumns		=	3;
					InputColumnSpec		=	"	int, short, short	";
					Convert_NF_to_zero	=	1;
					break;


		case AS400SRV_T2025_READ_hospital_member_in_hospital:
					SQL_CommandText		=	"	SELECT	in_hospital					"
											"	FROM	hospital_member				"
											"	WHERE	member_id			= ?		"
											"	  AND	mem_id_extension	= ?		";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	short	";
					NumInputColumns		=	2;
					InputColumnSpec		=	"	int, short	";
					break;


		case AS400SRV_T2025_READ_special_prescs_count:
					SQL_CommandText		=	"	SELECT	CAST (count(*) AS INTEGER)	"
											"	FROM	special_prescs				"
											"	WHERE	member_id			= ?		"
											"	  AND	mem_id_extension	= ?		"
											"	  AND	confirmation_type	= 1		";	// Count only active ishurim.
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					NumInputColumns		=	2;
					InputColumnSpec		=	"	int, short	";
					Convert_NF_to_zero	=	1;
					break;


		case AS400SRV_T2025_UPD_conmemb:
					SQL_CommandText		=	"	UPDATE	conmemb					"

											"	SET		Contractor		= ?,	"
											"			ContractType	= ?,	"
											"			LinkType		= ?,	"
											"			LastUpdateDate	= ?,	"
											"			LastUpdateTime	= ?,	"
											"			ValidFrom		= ?,	"
											"			ValidUntil		= ?,	"
											"			UpdatedBy		= ?,	"
											"			ConMembNote		= ?		"

											"	WHERE	IdNumber		= ?		"
											"	  AND	IdCode			= ?		"
											"	  AND	ConSeq			= ?		"
											"	  AND	Profession		= ?		";
					NumInputColumns		=	13;
					InputColumnSpec		=	"	int, short, short, int, int, int, int, short, char(65), int, short, short, short	";
					break;


		case AS400SRV_T2025_UPD_members:
											// DonR 29Jul2025 User Story #435969: Added new dangerous_drug_status
											// column. I left Old Member TZ Code alone, even though it's no longer
											// in use and now just gets a hard-coded zero from the calling function.
					SQL_CommandText		=	"	UPDATE	members							"

											"	SET		card_date				= ?,	"
											"			father_name				= ?,	"
											"			first_name				= ?,	"
											"			last_name				= ?,	"
											"			street					= ?,	"
											"			house_num				= ?,	"
											"			city					= ?,	"
											"			phone					= ?,	"
											"			zip_code				= ?,	"
											"			sex						= ?,	"

											"			date_of_bearth			= ?,	"
											"			marital_status			= ?,	"
											"			maccabi_code			= ?,	"
											"			maccabi_until			= ?,	"
											"			mac_magen_code			= ?,	"
											"			mac_magen_from			= ?,	"
											"			mac_magen_until			= ?,	"
											"			keren_mac_code			= ?,	"
											"			keren_mac_from			= ?,	"
											"			keren_mac_until			= ?,	"

											"			keren_wait_flag			= ?,	"
											"			asaf_code				= ?,	"
											"			insurance_status		= ?,	"
											"			doctor_status			= ?,	"
											"			credit_type_code		= ?,	"
											"			member_discount_pt		= ?,	"
											"			old_member_id			= ?,	"
											"			old_id_extension		= ?,	"
											"			data_update_date		= ?,	"
											"			data_update_time		= ?,	"

											"			idnumber_main			= ?,	"
											"			idcode_main				= ?,	"
											"			updateadr				= ?,	"
											"			last_english			= ?,	"
											"			first_english			= ?,	"
											"			mess					= ?,	"
											"			snif					= ?,	"
											"			check_od_interact		= ?,	"
											"			has_tikra				= ?,	"
											"			yahalom_code			= ?,	"

											"			yahalom_from			= ?,	"
											"			yahalom_until			= ?,	"
											"			carry_over_vetek		= ?,	"
											"			insurance_type			= ?,	"
											"			illness_1				= ?,	"
											"			illness_2				= ?,	"
											"			illness_3				= ?,	"
											"			illness_4				= ?,	"
											"			illness_5				= ?,	"
											"			illness_6				= ?,	"

											"			illness_7				= ?,	"
											"			illness_8				= ?,	"
											"			illness_9				= ?,	"
											"			illness_10				= ?,	"
											"			illness_bitmap			= ?,	"
											"			force_100_percent_ptn	= ?,	"
											"			darkonai_no_card		= ?,	"
											"			darkonai_type			= ?,	"
											"			dangerous_drug_status	= ?		"

											"	WHERE 	member_id				= ?		"
											"	  AND	mem_id_extension		= ?		";
					NumInputColumns		=	61;	// DonR 29Jul2025 User Story #435969: 60->61.
					InputColumnSpec		=	"	short, varchar(8), varchar(8), varchar(14), varchar(12), char(4), varchar(20), varchar(10), int, short,	"
											"	int, short, short, int, short, int, int, short, int, int,												"
											"	short, short, short, int, short, short, int, short, int, int,											"
											"	int, short, int, varchar(15), varchar(15), int, int, short, short, short,								"
											"	int, int, int, char(1), short, short, short, short, short, short,										"
											"	short, short, short, short, int, short, short, short, short,											"
											"	int, short																								";
					CommandIsMirrored	=	0;	// DonR 29Jul2025 - Turning this off, as we're pretty much letting Informix die.
					StatementIsSticky	=	1;	// DonR 03Mar2021: Possible performance enhancement.
					break;


		case AS400SRV_T2026_DEL_hospital_member:
					SQL_CommandText		=	"	DELETE								"
											"	FROM	hospital_member				"
											"	WHERE	member_id			= ?		"
											"	  AND	mem_id_extension	= ?		";
					NumInputColumns		=	2;
					InputColumnSpec		=	"	int, short	";
					break;


		case AS400SRV_T2026_DEL_members:
					SQL_CommandText		=	"	DELETE								"
											"	FROM	members						"
											"	WHERE	member_id			= ?		"
											"	  AND	mem_id_extension	= ?		";
					NumInputColumns		=	2;
					InputColumnSpec		=	"	int, short	";
					CommandIsMirrored	=	0;	// DonR 29Jul2025 - Turning this off, as we're pretty much letting Informix die.
					break;


		case AS400SRV_T2036_INS_macabi_centers:
					SQL_CommandText		=	"	INSERT INTO macabi_centers													"
											"	(																			"
											"		macabi_centers_num,		first_center_type,		second_center_type,		"
											"		macabi_centers_nam,		street_and_no,			city,					"
											"		zip_code,				phone_1,				fax_num,				"

											"		discount_percent,		credit_flag,			allowed_proc_list,		"
											"		allowed_belongings,		assuta_card_number,		delivery_type,			"
											"		sms_code,				sms_format,				sms_subformat,			"

											"		update_date,			update_time,			del_flg					"
											"	)																			";
					NumInputColumns		=	21;
					InputColumnSpec		=	"	int, char(2), char(2), varchar(40), varchar(20), varchar(20), int, varchar(10), varchar(10),	"
											"	short, short, char(50), char(50), char(10), char(15), int, int, int,							"
											"	int, int, short																					";
					GenerateVALUES		=	1;
					break;


		case AS400SRV_T2036_UPD_macabi_centers:
					SQL_CommandText		=	"	UPDATE	macabi_centers				"

											"	SET		first_center_type	= ?,	"
											"			second_center_type	= ?,	"
											"			macabi_centers_nam	= ?,	"
											"			street_and_no		= ?,	"
											"			city				= ?,	"
											"			zip_code			= ?,	"
											"			phone_1				= ?,	"
											"			fax_num				= ?,	"
											"			discount_percent	= ?,	"
											"			credit_flag			= ?,	"

											"			allowed_proc_list	= ?,	"
											"			allowed_belongings	= ?,	"
											"			assuta_card_number	= ?,	"
											"			delivery_type		= ?,	"
											"			sms_code			= ?,	"
											"			sms_format			= ?,	"
											"			sms_subformat		= ?,	"
											"			update_date			= ?,	"
											"			update_time			= ?		"

											"	WHERE	macabi_centers_num	= ?		";
					NumInputColumns		=	20;
					InputColumnSpec		=	"	char(2), char(2), varchar(40), varchar(20), varchar(20), int, varchar(10), varchar(10), short, short,	"
											"	char(50), char(50), char(10), char(15), int, int, int, int, int, int									";
					break;


		case AS400SRV_T2037_DEL_macabi_centers:
					SQL_CommandText		=	"	DELETE FROM macabi_centers WHERE macabi_centers_num = ?	";
					NumInputColumns		=	1;
					InputColumnSpec		=	"	int	";
					break;

		case AS400SRV_T2041_DEL_member_price:
					SQL_CommandText		=	"	DELETE FROM member_price WHERE member_price_code = ?	";
					NumInputColumns		=	1;
					InputColumnSpec		=	"	short	";
					break;


		case AS400SRV_T2042_DEL_price_list:
					SQL_CommandText		=	"	DELETE FROM	price_list				"
											"	WHERE		price_list_code	= ?		"
											"	  AND		largo_code		= ?		";
					NumInputColumns		=	2;
					InputColumnSpec		=	"	short, int	";
					break;


		case AS400SRV_T2043_DEL_drug_extension:
					SQL_CommandText		=	"	UPDATE	drug_extension				"

											"	SET		del_flg				= 1,	"
											"			as400_batch_date	= ?,	"
											"			as400_batch_time	= ?,	"
											"			changed_date		= ?,	"
											"			changed_time		= ?		"

											"	WHERE		largo_code	= ?			"
											"	  AND		rule_number	= ?			";
					NumInputColumns		=	6;
					InputColumnSpec		=	"	int, int, int, int, int, int	";
					break;


		case AS400SRV_T2048_INS_GenComponents:
					SQL_CommandText		=	"	INSERT INTO GenComponents					"
											"	(											"
											"		gen_comp_code,		gen_comp_desc,		"
											"		update_date,		update_time,		"
											"		updatedby,			del_flg,			"
											"		as400_batch_date,	as400_batch_time	"
											"	)											";
					NumInputColumns		=	8;
					InputColumnSpec		=	"	short, varchar(40), int, int, int, char(1), int, int	";
					GenerateVALUES		=	1;
					break;


		case AS400SRV_T2048_READ_GenComponents_old_data:
					SQL_CommandText		=	"	SELECT	gen_comp_desc,	del_flg		"
											"	FROM	GenComponents				"
											"	WHERE	gen_comp_code = ?			";
					NumOutputColumns	=	2;
					OutputColumnSpec	=	"	varchar(40), char(1)	";
					NumInputColumns		=	1;
					InputColumnSpec		=	"	short	";
					break;


		case AS400SRV_T2048_UPD_GenComponents:
					SQL_CommandText		=	"	UPDATE	GenComponents				"
											"	SET		gen_comp_desc		= ?,	"
											"			update_date			= ?,	"
											"			update_time			= ?,	"
											"			updatedby			= ?,	"
											"			del_flg				= ?,	"
											"			as400_batch_date	= ?,	"
											"			as400_batch_time	= ?		"
											"	WHERE  gen_comp_code		= ?		";
					NumInputColumns		=	8;
					InputColumnSpec		=	"	varchar(40), int, int, int, char(1), int, int, short	";
					break;

		// Marianna 23Jul2023 User Story #456129: Added 3 new queries for postprocessor.
		case AS400SRV_T4048PP_postproc_cur:
					SQL_CommandText		=	"	SELECT  gen_comp_code												"

											"	FROM    gencomponents												"

											"	WHERE   (( as400_batch_date	<> ?) OR								"
											"			 ((as400_batch_date	=  ?) AND (as400_batch_time <> ?)))		";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	short	";
					NumInputColumns		=	3;
					InputColumnSpec		=	"	int, int, int	";
					CursorName			=	"postproc_4048_cur";
					break;


		case AS400SRV_T4048PP_DEL_gencomponents:
					SQL_CommandText		=	"	DELETE FROM	gencomponents WHERE CURRENT OF postproc_4048_cur	";
					break;


		case AS400SRV_T4048PP_READ_gencomponents_table_size:
					SQL_CommandText		=	"	SELECT CAST (count(*) AS INTEGER) FROM gencomponents	";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					Convert_NF_to_zero	=	1;
					break;


		case AS400SRV_T2049_INS_DrugGenComponents:
					SQL_CommandText		=	"	INSERT INTO DrugGenComponents					"
											"	(												"
											"		largo_code,				gen_comp_code,		"
											"		update_date,			update_time,		"
											"		updatedby,				del_flg,			"
											"		ingr_description,		ingredient_type,	"
											"		package_size,			ingredient_qty,		"
											"		ingredient_units,		ingredient_per_qty,	"
											"		ingredient_per_units						"
											"	)												";
					NumInputColumns		=	13;
					InputColumnSpec		=	"	int, short, int, int, int, char(1), char(40), char(1), float, float, char(3), float, char(3)	";
					GenerateVALUES		=	1;
					break;


		case AS400SRV_T2049_READ_DrugGenComponents_old_data:
					SQL_CommandText		=	"	SELECT	update_date,		update_time,		del_flg,			"
											"			ingr_description,	ingredient_type,	package_size,		"
											"			ingredient_qty,		ingredient_units,	ingredient_per_qty,	"
											"			ingredient_per_units										"
											"	FROM	DrugGenComponents							"
											"	WHERE	gen_comp_code	= ?							"
											"	  AND	largo_code		= ?							";
					NumOutputColumns	=	10;
					OutputColumnSpec	=	"	int, int, char(1), char(40), char(1), float, float, char(3), float, char(3)	";
					NumInputColumns		=	2;
					InputColumnSpec		=	"	short, int	";
					break;


		case AS400SRV_T2049_UPD_DrugGenComponents:
					SQL_CommandText		=	"	UPDATE	DrugGenComponents				"
											"	SET		update_date				= ?,	"
											"			update_time				= ?,	"
											"			updatedby				= ?,	"
											"			del_flg					= ?,	"
											"			ingr_description		= ?,	"
											"			ingredient_type			= ?,	"
											"			package_size			= ?,	"
											"			ingredient_qty			= ?,	"
											"			ingredient_units		= ?,	"
											"			ingredient_per_qty		= ?,	"
											"			ingredient_per_units	= ?		"
											"	WHERE	gen_comp_code		= ?		"
											"	  AND	largo_code			= ?		";
					NumInputColumns		=	13;
					InputColumnSpec		=	"	int, int, int, char(1), char(40), char(1), float, float, char(3), float, char(3), short, int	";
					break;


		case AS400SRV_T2050_INS_GeneryInteraction:
					SQL_CommandText		=	"	INSERT INTO GeneryInteraction				"
											"	(											"
											"		first_gen_code,		second_gen_code,	"
											"		interaction_type,	inter_note_code,	"
											"		update_date,		update_time,		"
											"		updatedby,			del_flg				"
											"	)											";
					NumInputColumns		=	8;
					InputColumnSpec		=	"	short, short, short, short, int, int, int, char(1)	";
					GenerateVALUES		=	1;
					break;


		case AS400SRV_T2050_READ_GeneryInteraction_old_data:
					SQL_CommandText		=	"	SELECT	interaction_type,	inter_note_code,	"
											"			update_date,		update_time,		"
											"			del_flg									"
											"	FROM	GeneryInteraction						"
											"	WHERE	first_gen_code	= ?						"
											"	  AND	second_gen_code	= ?						";
					NumOutputColumns	=	5;
					OutputColumnSpec	=	"	short, short, int, int, char(1)	";
					NumInputColumns		=	2;
					InputColumnSpec		=	"	short, short	";
					break;


		case AS400SRV_T2050_UPD_GeneryInteraction:
					SQL_CommandText		=	"	UPDATE	GeneryInteraction			"
											"	SET		inter_note_code		= ?,	"
											"			interaction_type	= ?,	"
											"			update_date			= ?,	"
											"			update_time			= ?,	"
											"			updatedby			= ?,	"
											"			del_flg				= ?		"
											"	WHERE	first_gen_code		= ?		"
											"	  AND	second_gen_code		= ?		";
					NumInputColumns		=	8;
					InputColumnSpec		=	"	short, short, int, int, int, char(1), short, short	";
					break;


		case AS400SRV_T2051_INS_GnrlDrugNotes:
					SQL_CommandText		=	"	INSERT INTO GnrlDrugNotes												"
											"	(																		"
											"		gnrldrugnotetype,		gnrldrugnotecode,		gnrldrugnote,		"
											"		gdn_type_new,			gdn_long_note,			gdn_category,		"
											"		gdn_sex,				gdn_from_age,			gdn_to_age,			"

											"		gdn_seq_num,			gdn_connect_type,		gdn_connect_desc,	"
											"		gdn_severity,			gdn_sensitv_type,		gdn_sensitv_desc,	"
											"		update_date,			update_time,			updatedby,			"
											"		del_flg																"
											"	)																		";
					NumInputColumns		=	19;
					InputColumnSpec		=	"	char(1), int, varchar(50), char(1), varchar(200), short, short, short, short,	"
											"	short, short, varchar(25), short, int, varchar(25), int, int, int, char(1)		";
					GenerateVALUES		=	1;
					break;


		case AS400SRV_T2051_UPD_GnrlDrugNotes:
					SQL_CommandText		=	"	UPDATE	GnrlDrugNotes				"
											"	SET		gnrldrugnote		= ?,	"
											"			gdn_type_new		= ?,	"
											"			gdn_long_note		= ?,	"
											"			gdn_category		= ?,	"
											"			gdn_sex				= ?,	"
											"			gdn_from_age		= ?,	"
											"			gdn_to_age			= ?,	"
											"			gdn_seq_num			= ?,	"
											"			gdn_connect_type	= ?,	"
											"			gdn_connect_desc	= ?,	"
											"			gdn_severity		= ?,	"
											"			gdn_sensitv_type	= ?,	"
											"			gdn_sensitv_desc	= ?,	"
											"			update_date			= ?,	"
											"			update_time			= ?,	"
											"			updatedby			= ?,	"
											"			del_flg				= ?		"
											"	WHERE	gnrldrugnotetype	= ?		"
											"	  AND	gnrldrugnotecode	= ?		";
					NumInputColumns		=	19;
					InputColumnSpec		=	"	varchar(50), char(1), varchar(200), short, short, short, short, short, short,	"
											"	varchar(25), short, int, varchar(25), int, int, int, char(1), char(1), int		";
					break;


		case AS400SRV_T2055_INS_pharmacology:
					SQL_CommandText		=	"	INSERT INTO Pharmacology					"
											"	(											"
											"		pharm_group_code,	pharm_group_name,	"
											"		update_date,		update_time,		"
											"		updatedby,			del_flg				"
											"	)											";
					NumInputColumns		=	6;
					InputColumnSpec		=	"	short, varchar(25), int, int, int, char(1)	";
					GenerateVALUES		=	1;
					break;


		case AS400SRV_T2055_READ_pharmacology_old_data:
					SQL_CommandText		=	"	SELECT	pharm_group_name,	update_date,	"
											"			update_time,		del_flg			"
											"	FROM	pharmacology						"
											"	WHERE	pharm_group_code  = ?				";
					NumOutputColumns	=	4;
					OutputColumnSpec	=	"	varchar(25), int, int, char(1)	";
					NumInputColumns		=	1;
					InputColumnSpec		=	"	short	";
					break;


		case AS400SRV_T2055_UPD_pharmacology:
					SQL_CommandText		=	"	UPDATE	Pharmacology				"
											"	SET		pharm_group_name	= ?,	"
											"			update_date			= ?,	"
											"			update_time			= ?,	"
											"			updatedby			= ?,	"
											"			del_flg				= ?		"
											"	WHERE	pharm_group_code	= ?		";
					NumInputColumns		=	6;
					InputColumnSpec		=	"	varchar(25), int, int, int, char(1), short	";
					break;


		case AS400SRV_T2056_INS_GenInterNotes:
					SQL_CommandText		=	"	INSERT INTO GenInterNotes					"
											"	(											"
											"		inter_note_code,	interaction_note,	"
											"		inter_long_note,	update_date,		"
											"		update_time,		updatedby,			"
											"		del_flg									"
											"	)											";
					NumInputColumns		=	7;
					InputColumnSpec		=	"	short, varchar(40), varchar(100), int, int, int, char(1)	";
					GenerateVALUES		=	1;
					break;


		case AS400SRV_T2056_READ_GenInterNotes_old_data:
					SQL_CommandText		=	"	SELECT	interaction_note,	inter_long_note,	"
											"			update_date,		update_time,		"
											"			del_flg									"
											"	FROM	GenInterNotes							"
											"	WHERE	inter_note_code = ?						";
					NumOutputColumns	=	5;
					OutputColumnSpec	=	"	varchar(40), varchar(100), int, int, char(1)	";
					NumInputColumns		=	1;
					InputColumnSpec		=	"	short	";
					break;


		case AS400SRV_T2056_UPD_GenInterNotes:
					SQL_CommandText		=	"	UPDATE	GenInterNotes				"
											"	SET		interaction_note	= ?,	"
											"			inter_long_note		= ?,	"
											"			update_date			= ?,	"
											"			update_time			= ?,	"
											"			updatedby			= ?,	"
											"			del_flg				= ?		"
											"	WHERE	inter_note_code		= ?		";
					NumInputColumns		=	7;
					InputColumnSpec		=	"	varchar(40), varchar(100), int, int, int, char(1), short	";
					break;


		case AS400SRV_T2061_DEL_pharmacy_contract_delete_all:
					SQL_CommandText		=	"	DELETE FROM pharmacy_contract WHERE del_flg = ?	";
					NumInputColumns		=	1;
					InputColumnSpec		=	"	short	";
					break;


		case AS400SRV_T2062_DEL_pharmacy_contract_duplicate_rows:
					SQL_CommandText		=	"	DELETE FROM pharmacy_contract			"
											"	WHERE		contract_code		= ?		"
											"	  AND		contract_min_amt	= ?		"
											"	  AND		contract_from_date	= ?		"
											"	  AND		contract_to_date	= ?		"
											"	  AND		fps_group_code		= ?		"
											"	  AND		del_flg				= 0		";
					NumInputColumns		=	5;
					InputColumnSpec		=	"	int, int, int, int, int	";
					break;


		case AS400SRV_T2062_INS_pharmacy_contract:
					SQL_CommandText		=	"	INSERT INTO pharmacy_contract					"
											"	(												"
											"		contract_code,			contract_from_date,	"
											"		contract_to_date,		contract_min_amt,	"
											"		contract_max_amt,		contract_fee,		"
											"		contract_status,		fps_group_code,		"
											"		fps_group_name,			update_date,		"
											"		update_time,			del_flg				"
											"	)												";
					NumInputColumns		=	12;
					InputColumnSpec		=	"	int, int, int, int, int, int, short, int, char(16), int, int, short	";
					GenerateVALUES		=	1;
					break;


		case AS400SRV_T2063_prescr_source_allowed_by_default_cur:
					SQL_CommandText		=	"	SELECT pr_src_code, allowed_by_default, id_type_accepted FROM prescr_source	";
					NumOutputColumns	=	3;
					OutputColumnSpec	=	"	short, short, short	";
					break;


		case AS400SRV_T2063_DEL_prescr_source:
					SQL_CommandText		=	"	DELETE FROM prescr_source WHERE pr_src_code IS NOT NULL	";
					break;


		case AS400SRV_T2064_INS_prescr_source:
					SQL_CommandText		=	"	INSERT INTO prescr_source						"
											"	(												"
											"		pr_src_code,			pr_src_desc,		"
											"		pr_src_docid_type,		id_type_accepted,	"
											"		pr_src_doc_device,		pr_src_priv_pharm,	"
											"		allowed_by_default							"
											"	)												";
					NumInputColumns		=	7;
					InputColumnSpec		=	"	short, char(25), short, short, short, short, short	";
					GenerateVALUES		=	1;
					break;


		case AS400SRV_T2065_DEL_confirm_authority:
					SQL_CommandText		=	"	DELETE FROM confirm_authority WHERE authority_code IS NOT NULL	";
					break;


		case AS400SRV_T2066_INS_confirm_authority:
					SQL_CommandText		=	"	INSERT INTO confirm_authority ( authority_code, authority_desc )	";
					NumInputColumns		=	2;
					InputColumnSpec		=	"	short, char(25)	";
					GenerateVALUES		=	1;
					break;


		case AS400SRV_T2067_del_gadget_cur:
					SQL_CommandText		=	"	SELECT		largo_code														"
											"	FROM		gadgets															"
											"	WHERE		(update_date < ?) OR ((update_date = ?) AND (update_time < ?))	"
											"	ORDER BY	largo_code														";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					NumInputColumns		=	3;
					InputColumnSpec		=	"	int, int, int	";
					break;


		case AS400SRV_T2067_DEL_gadgets:
					SQL_CommandText		=	"	DELETE FROM gadgets WHERE largo_code = ?	";
					NumInputColumns		=	1;
					InputColumnSpec		=	"	int	";
					break;


		case AS400SRV_T2067_UPD_drug_list_clear_gadget_columns:
					SQL_CommandText		=	"	UPDATE	drug_list				"
											"	SET		in_gadget_table	= 0,	"
											"			update_date		= ?,	"
											"			update_time		= ?		"
											"	WHERE	largo_code		= ?		"
											"	  AND	in_gadget_table	<> 0	";
					NumInputColumns		=	3;
					InputColumnSpec		=	"	int, int, int	";
					break;


		case AS400SRV_T2069_INS_clicks_discount:
					SQL_CommandText		=	"	INSERT INTO clicks_discount											"
											"	(																	"
											"		discount_code,			show2spec,			show2nonspec,		"
											"		show_needs_ishur,		ptn_spec_basic,		ptn_nonspec_basic,	"
											"		ptn_spec_keren,			ptn_nonspec_keren,	spec_msg,			"
											"		nonspec_msg,			update_date,		update_time,		"
											"		updatedby,				del_flg									"
											"	)																	";
					NumInputColumns		=	14;
					InputColumnSpec		=	"	char(1), char(3), char(3), short, char(3), char(3), char(3),	"
											"	char(3), varchar(75), varchar(75), int, int, int, char(1)		";
					GenerateVALUES		=	1;
					break;


		case AS400SRV_T2069_UPD_clicks_discount:
					SQL_CommandText		=	"	UPDATE	clicks_discount				"
												 
											"	SET		update_date			= ?,	"
											"			update_time			= ?,	"
											"			updatedby			= ?,	"
											"			show2spec			= ?,	"
											"			show2nonspec		= ?,	"
											"			show_needs_ishur	= ?,	"
											"			ptn_spec_basic		= ?,	"
											"			ptn_nonspec_basic	= ?,	"
											"			ptn_spec_keren		= ?,	"
											"			ptn_nonspec_keren	= ?,	"
											"			spec_msg			= ?,	"
											"			nonspec_msg			= ?,	"
											"			del_flg				= ?		"

											"	WHERE  	discount_code		= ?		";
					NumInputColumns		=	14;
					InputColumnSpec		=	"	int, int, int, char(3), char(3), short, char(3), char(3),		"
											"	char(3), char(3), varchar(75), varchar(75), char(1), char(1)	";
					break;


		case AS400SRV_T2070_INS_drug_forms:
					SQL_CommandText		=	"	INSERT INTO drug_forms								"
											"	(													"
											"		largo_code,		form_number,	update_date,	"
											"		update_time,	updatedby,		del_flg,		"
											"		valid_from,		valid_until						"
											"	)													";
					NumInputColumns		=	8;
					InputColumnSpec		=	"	int, short, int, int, int, char(1), int, int	";
					GenerateVALUES		=	1;
					break;


		case AS400SRV_T2070_UPD_drug_forms:
					SQL_CommandText		=	"	UPDATE	drug_forms					"
												 
											"	SET		update_date			= ?,	"
											"			update_time			= ?,	"
											"			updatedby			= ?,	"
											"			del_flg				= ?,	"
											"			valid_from			= ?,	"
											"			valid_until			= ?		"

											"	WHERE  	largo_code			= ?		"
											"	  AND	form_number			= ?		";
					NumInputColumns		=	8;
					InputColumnSpec		=	"	int, int, int, char(1), int, int, int, short	";
					break;


		case AS400SRV_T2071_INS_usage_instruct:
					SQL_CommandText		=	"	INSERT INTO usage_instruct												"
											"	(																		"
											"		shape_num,				shape_code,				shape_desc,			"
											"		inst_code,				inst_msg,				inst_seq,			"
											"		calc_op_flag,			unit_code,				unit_seq,			"

											"		unit_name,				unit_desc,				open_od_window,		"
											"		concentration_flag,		total_unit_name,		round_units_flag,	"
											"		update_date,			update_time,			updatedby,			"
											"		del_flg																"
											"	)																		";
					NumInputColumns		=	19;
					InputColumnSpec		=	"	short, char(4), char(25), short, varchar(40), short, short, char(3), short,	"
											"	char(8), varchar(25), short, short, char(10), short, int, int, int, char(1)	";
					GenerateVALUES		=	1;
					break;


		case AS400SRV_T2071_UPD_usage_instruct:
					SQL_CommandText		=	"	UPDATE	usage_instruct				"
												 
											"	SET		update_date			= ?,	"
											"			update_time			= ?,	"
											"			updatedby			= ?,	"
											"			shape_code			= ?,	"
											"			shape_desc			= ?,	"
											"			inst_msg			= ?,	"
											"			calc_op_flag		= ?,	"
											"			unit_seq			= ?,	"
											"			unit_name			= ?,	"
											"			unit_desc			= ?,	"
											"			open_od_window		= ?,	"
											"			concentration_flag	= ?,	"
											"			total_unit_name		= ?,	"
											"			round_units_flag	= ?,	"
											"			del_flg				= ?		"

											"	WHERE  	shape_num	= ?				"
											"	  AND	inst_code	= ?				"
											"	  AND	inst_seq	= ?				"
											"	  AND	unit_code	= ?				";
					NumInputColumns		=	19;
					InputColumnSpec		=	"	int, int, int, char(4), char(25), varchar(40), short, short, char(8), varchar(25),	"
											"	short, short, char(10), short, char(1), short, short, short, char(3)				";
					break;


		case AS400SRV_T2072_INS_treatment_group:
					SQL_CommandText		=	"	INSERT INTO treatment_group						"
											"	(												"
											"		pharmacology_code,		treatment_group,	"
											"		treat_grp_desc,			presc_valid_days,	"
											"		update_date,			update_time,		"
											"		updatedby,				del_flg				"
											"	)												";
					NumInputColumns		=	8;
					InputColumnSpec		=	"	short, short, varchar(40), short, int, int, int, char(1)	";
					GenerateVALUES		=	1;
					break;


		case AS400SRV_T2072_UPD_treatment_group:
					SQL_CommandText		=	"	UPDATE	treatment_group				"
												 
											"	SET		update_date			= ?,	"
											"			update_time			= ?,	"
											"			updatedby			= ?,	"
											"			treat_grp_desc		= ?,	"
											"			presc_valid_days	= ?,	"
											"			del_flg				= ?		"

											"	WHERE  	pharmacology_code	= ?		"
											"	  AND	treatment_group		= ?		";
					NumInputColumns		=	8;
					InputColumnSpec		=	"	int, int, int, varchar(40), short, char(1), short, short	";
					break;


		case AS400SRV_T2073_INS_presc_period:
					SQL_CommandText		=	"	INSERT INTO presc_period			"
											"	(									"
											"		presc_type,		month,			"
											"		from_day,		to_day,			"
											"		update_date,	update_time,	"
											"		updatedby,		del_flg			"
											"	)									";
					NumInputColumns		=	8;
					InputColumnSpec		=	"	short, short, short, short, int, int, int, char(1)	";
					GenerateVALUES		=	1;
					break;


		case AS400SRV_T2073_UPD_presc_period:
					SQL_CommandText		=	"	UPDATE	presc_period				"
												 
											"	SET		update_date			= ?,	"
											"			update_time			= ?,	"
											"			updatedby			= ?,	"
											"			from_day			= ?,	"
											"			to_day				= ?,	"
											"			del_flg				= ?		"

											"	WHERE  	presc_type			= ?		"
											"	  AND  	month				= ?		";
					NumInputColumns		=	8;
					InputColumnSpec		=	"	int, int, int, short, short, char(1), short, short	";
					break;


		case AS400SRV_T2075_INS_sale:
					SQL_CommandText		=	"	INSERT INTO sale												"
											"	(																"
											"		sale_number,			sale_name,			sale_owner,		"
											"		sale_audience,			start_date,			end_date,		"
											"		sale_type,				min_op_to_buy,		op_to_receive,	"
											"		min_purchase_amt,		purchase_discount,	tav_knia_amt,	"
											"		update_date,			update_time,		del_flg			"
											"	)																";
					NumInputColumns		=	15;
					InputColumnSpec		=	"	int, char(75), short, short, int, int, short, int, int, int, short, int, int, int, short	";
					GenerateVALUES		=	1;
					break;


		case AS400SRV_T2075_UPD_sale:
					SQL_CommandText		=	"	UPDATE	sale						"

											"	SET		sale_name			= ?,	"
											"			sale_owner			= ?,	"
											"			sale_audience		= ?,	"
											"			start_date			= ?,	"
											"			end_date			= ?,	"
											"			sale_type			= ?,	"
											"			min_op_to_buy		= ?,	"
											"			op_to_receive		= ?,	"
											"			min_purchase_amt	= ?,	"
											"			purchase_discount	= ?,	"
											"			tav_knia_amt		= ?,	"
											"			update_date			= ?,	"
											"			update_time			= ?,	"
											"			del_flg				= ?		"

											"	WHERE	sale_number			= ?		";
					NumInputColumns		=	15;
					InputColumnSpec		=	"	char(75), short, short, int, int, short, int, int, int, short, int, int, int, short, int	";
					break;


		case AS400SRV_T2076_INS_sale_fixed_price:
					SQL_CommandText		=	"	INSERT INTO sale_fixed_price						"
											"	(													"
											"		sale_number,	largo_code,		sale_price,		"
											"		update_date,	update_time,	del_flg			"
											"	)													";
					NumInputColumns		=	6;
					InputColumnSpec		=	"	int, int, int, int, int, short	";
					GenerateVALUES		=	1;
					break;


		case AS400SRV_T2076_UPD_sale_fixed_price:
					SQL_CommandText		=	"	UPDATE	sale_fixed_price		"

											"	SET		sale_price		= ?,	"
											"			update_date		= ?,	"
											"			update_time		= ?,	"
											"			del_flg			= ?		"

											"	WHERE	sale_number		= ?		"
											"	  AND	largo_code		= ?		";
					NumInputColumns		=	6;
					InputColumnSpec		=	"	int, int, int, short, int, int	";
					break;


		case AS400SRV_T2077_INS_sale_bonus_recv:
					SQL_CommandText		=	"	INSERT INTO sale_bonus_recv					"
											"	(											"
											"		sale_number,		largo_code,			"
											"		op_to_receive,		discount_percent,	"
											"		sale_price,			update_date,		"
											"		update_time,		del_flg				"
											"	)											";
					NumInputColumns		=	8;
					InputColumnSpec		=	"	int, int, int, short, int, int, int, short	";
					GenerateVALUES		=	1;
					break;


		case AS400SRV_T2077_UPD_sale_bonus_recv:
					SQL_CommandText		=	"	UPDATE	sale_bonus_recv				"

											"	SET		op_to_receive		= ?,	"
											"			discount_percent	= ?,	"
											"			sale_price			= ?,	"
											"			update_date			= ?,	"
											"			update_time			= ?,	"
											"			del_flg				= ?		"

											"	WHERE	sale_number			= ?		"
											"	  AND	largo_code			= ?		";
					NumInputColumns		=	8;
					InputColumnSpec		=	"	int, short, int, int, int, short, int, int	";
					break;


		case AS400SRV_T2078_INS_sale_target:
					SQL_CommandText		=	"	INSERT INTO sale_target					"
											"	(										"
											"		sale_number,		pharmacy_type,	"
											"		pharmacy_size,		target_op,		"
											"		max_op,				update_date,	"
											"		update_time,		del_flg			"
											"	)										";
					NumInputColumns		=	8;
					InputColumnSpec		=	"	int, short, short, int, int, int, int, short	";
					GenerateVALUES		=	1;
					break;


		case AS400SRV_T2078_UPD_sale_target:
					SQL_CommandText		=	"	UPDATE	sale_target				"

											"	SET		target_op		= ?,	"
											"			max_op			= ?,	"
											"			update_date		= ?,	"
											"			update_time		= ?,	"
											"			del_flg			= ?		"

											"	WHERE	sale_number		= ?		"
											"	  AND	pharmacy_type	= ?		"
											"	  AND	pharmacy_size	= ?		";
					NumInputColumns		=	8;
					InputColumnSpec		=	"	int, int, int, int, short, int, short, short	";
					break;


		case AS400SRV_T2079_INS_sale_pharmacy:
					SQL_CommandText		=	"	INSERT INTO sale_pharmacy				"
											"	(										"
											"		sale_number,		pharmacy_code,	"
											"		update_date,		update_time,	"
											"		del_flg								"
											"	)										";
					NumInputColumns		=	5;
					InputColumnSpec		=	"	int, int, int, int, short	";
					GenerateVALUES		=	1;
					break;


		case AS400SRV_T2079_UPD_sale_pharmacy:
					SQL_CommandText		=	"	UPDATE	sale_pharmacy			"

											"	SET		update_date		= ?,	"
											"			update_time		= ?,	"
											"			del_flg			= ?		"

											"	WHERE	sale_number		= ?		"
											"	  AND	pharmacy_code	= ?		";
					NumInputColumns		=	5;
					InputColumnSpec		=	"	int, int, short, int, int	";
					break;


		case AS400SRV_T2080_INS_PharmDrugNotes:
					SQL_CommandText		=	"	INSERT INTO PharmDrugNotes												"
											"	(																		"
											"		gnrldrugnotetype,		gnrldrugnotecode,		gnrldrugnote,		"
											"		gdn_category,			gdn_seq_num,			gdn_connect_type,	"
											"		gdn_connect_desc,		gdn_severity,			update_date,		"
											"		update_time,			updatedby,				del_flg,			"
											"		as400_batch_date,		as400_batch_time							"
											"	)																		";
					NumInputColumns		=	14;
					InputColumnSpec		=	"	char(1), int, varchar(50), short, short, short, varchar(25), short, int, int, int, char(1), int, int	";
					GenerateVALUES		=	1;
					break;


		case AS400SRV_T2080_UPD_PharmDrugNotes:
					SQL_CommandText		=	"	UPDATE	PharmDrugNotes				"

											"	SET		gnrldrugnote		= ?,	"
											"			gdn_category		= ?,	"
											"			gdn_seq_num			= ?,	"
											"			gdn_connect_type	= ?,	"
											"			gdn_connect_desc	= ?,	"
											"			gdn_severity		= ?,	"
											"			update_date			= ?,	"
											"			update_time			= ?,	"
											"			updatedby			= ?,	"
											"			del_flg				= ?,	"
											"			as400_batch_date	= ?,	"
											"			as400_batch_time	= ?		"

											"	WHERE	gnrldrugnotetype	= ?		"
											"	  AND	gnrldrugnotecode	= ?		";
					NumInputColumns		=	14;
					InputColumnSpec		=	"	varchar(50), short, short, short, varchar(25), short, int, int, int, char(1),int, int, char(1), int	";
					break;

					
		// Marianna 23Jul2023 User Story #456129: Added 4 new queries for postprocessor.
		case AS400SRV_T4080PP_postproc_cur:
					SQL_CommandText		=	"	SELECT gnrldrugnotetype, gnrldrugnotecode							"

											"	FROM    pharmdrugnotes												"

											"	  WHERE	del_flg			<> 'D'										"
											"	  AND   (( as400_batch_date	<> ?) OR								"
											"			 ((as400_batch_date	=  ?) AND (as400_batch_time <> ?)))		";	// Slightly redundant date condition.
					NumOutputColumns	=	2;
					OutputColumnSpec	=	"	char(1), int	";
					NumInputColumns		=	3;
					InputColumnSpec		=	"	int, int, int	";
					CursorName			=	"T4080PP_pharmdrugnotes_cur";
					break;


		case AS400SRV_T4080PP_READ_pharmdrugnotes_active_size:
					SQL_CommandText		=	"	SELECT	CAST (count(*) AS INTEGER)	"
											"	FROM	pharmdrugnotes				"
											"	WHERE	del_flg <> 'D'			";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					Convert_NF_to_zero	=	1;
					break;


		case AS400SRV_T4080PP_READ_pharmdrugnotes_table_size:
					SQL_CommandText		=	"	SELECT CAST (count(*) AS INTEGER) FROM pharmdrugnotes	";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					Convert_NF_to_zero	=	1;
					break;


		case AS400SRV_T4080PP_UPD_pharmdrugnotes_logically_delete:
					SQL_CommandText		=	"	UPDATE	pharmdrugnotes					"

											"	SET		del_flg	= 'D',					"
											"			update_date	= ?,				"
											"			update_time	= ?					"

											"	WHERE	CURRENT OF T4080PP_pharmdrugnotes_cur	";
					NumInputColumns		=	2;
					InputColumnSpec		=	"	int, int	";
					break;
			

		case AS400SRV_T2081_DEL_purchase_lim_ishur_previous_rows:
					SQL_CommandText		=	"	DELETE FROM	purchase_lim_ishur		"

											"	WHERE	member_id			= ?		"
											"	  AND	largo_code			= ?		"
											"	  AND	member_id_code		= ?		";
					NumInputColumns		=	3;
					InputColumnSpec		=	"	int, int, short	";
					break;


		case AS400SRV_T2081_INS_purchase_lim_ishur:
					SQL_CommandText		=	"	INSERT INTO purchase_lim_ishur										"
											"	(																	"
											"		member_id,			member_id_code,		largo_code,				"
											"		ishur_source,		ishur_num,			ishur_type,				"
											"		del_flg,			reason_code,		ishur_text,				"
											"		max_units,			ingr_qty_std,		duration_months,		"

											"		auth_tz,			request_doc_id,		doc_has_seen_ishur,		"
											"		received_date,		received_time,		changed_date,			"
											"		changed_time,		transmit_date,		transmit_time,			"
											"		open_date,			close_date									"
											"	)																	";
					NumInputColumns		=	23;
					InputColumnSpec		=	"	int, short, int, short, int, short, short, short, char(65), int, double, short,	"
											"	int, int, short, int, int, int, int, int, int, int, int							";
					GenerateVALUES		=	1;
					break;


		case AS400SRV_T2081_READ_drug_list_ingr_1_units:
					SQL_CommandText		=	"	SELECT	ingr_1_units	"
											"	FROM	drug_list		"
											"	WHERE	largo_code = ?	";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	char(3)	";
					NumInputColumns		=	1;
					InputColumnSpec		=	"	int	";
					break;


		case AS400SRV_T2082_INS_drug_purchase_lim:
					SQL_CommandText		=	"	INSERT INTO drug_purchase_lim																"
											"	(																							"
											"		largo_code,						class_code,					qty_lim_grp_code,			"
											"		limit_method,					max_units,					ingr_qty_std,				"
											"		presc_source,					permit_source,				warning_only,				"

											"		ingredient_code,				exempt_diseases,			duration_months,			"
											"		history_start_date,				month_is_28_days,			min_prev_purchases,			"
											"		custom_error_code,				include_full_price_sales,	no_ishur_for_pharmacology,	"
											"		no_ishur_for_treatment_type,															"
						
											"		received_date,					received_time,											"
											"		changed_date,					changed_time,											"
											"		transmit_date,					transmit_time											"
											"	)																							";
					NumInputColumns		=	25;
					InputColumnSpec		=	"	int, short, int, short, int, double, short, short, short,			"	//  9
											"	short, short, short, int, short, short, short, bool, short, short,	"	// 19
											"	int, int, int, int, int, int										";	// 25
					GenerateVALUES		=	1;
					break;


		case AS400SRV_T2082_READ_drug_list_ingr_1_columns:
											// DonR 18Mar2024 User Story #299818: Also read Ishur Limit Ingredient.
					SQL_CommandText		=	"	SELECT	ingr_1_code, ingr_1_units,	ishur_qty_lim_ingr	"
											"	FROM	drug_list										"
											"	WHERE	largo_code = ?									";
					NumOutputColumns	=	3;
					OutputColumnSpec	=	"	short, char(3), short	";
					NumInputColumns		=	1;
					InputColumnSpec		=	"	int	";
					break;


		case AS400SRV_T2082_UPD_drug_list_normal_purchase_limit:
					SQL_CommandText		=	"	UPDATE	drug_list					"
											"	SET		purchase_limit_flg	= 1,	"
											"			qty_lim_grp_code	= ?		"
											"	WHERE	largo_code			= ?		";
					NumInputColumns		=	2;
					InputColumnSpec		=	"	int, int	";
					break;


		case AS400SRV_T2082_UPD_drug_list_purchase_limit_flg_only:
					SQL_CommandText		=	"	UPDATE	drug_list					"
											"	SET		purchase_limit_flg	= 1		"
											"	WHERE	largo_code = ?				";
					NumInputColumns		=	1;
					InputColumnSpec		=	"	int	";
					break;


		case AS400SRV_T2084_del_druglim_cur:	// NOTE: Should this be SELECT DISTINCT?
					SQL_CommandText		=	"	SELECT		largo_code			"
											"	FROM		drug_purchase_lim	"
											"	ORDER BY	largo_code			";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					break;


		case AS400SRV_T2084_DEL_drug_purchase_lim_all_rows:
					SQL_CommandText		=	"	DELETE FROM drug_purchase_lim WHERE largo_code IS NOT NULL	";
					break;


		case AS400SRV_T2084_UPD_drug_list_clear_1_purchase_limit_flg:
					SQL_CommandText		=	"	UPDATE	drug_list					"
											"	SET		purchase_limit_flg	= 0,	"
											"			qty_lim_grp_code	= 0		"
											"	WHERE	largo_code = ?				";
					NumInputColumns		=	1;
					InputColumnSpec		=	"	int	";
					break;


		case AS400SRV_T2084_UPD_drug_list_clear_all_purchase_limit_flg:
					SQL_CommandText		=	"	UPDATE	drug_list					"

											"	SET		purchase_limit_flg	= 0,	"
											"			qty_lim_grp_code	= 0		"

											"	WHERE	purchase_limit_flg	!= 0	"
											"	   OR	qty_lim_grp_code	!= 0	";
					break;


		case AS400SRV_T2086_GetPharmOwner:
					SQL_CommandText		=	"	SELECT	owner				"
											"	FROM	pharmacy			"
											"	WHERE	pharmacy_code	= ?	"
											"	  AND	del_flg			= 0	";
					NumInputColumns		=	1;
					InputColumnSpec		=	"	int		";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	short	";
					break;


		case AS400SRV_T2086_INS_MemberPharm:
					SQL_CommandText		=	"	INSERT INTO MemberPharm														"
											"	(																			"
											"		member_id,				mem_id_extension,			seq_number,			"
											"		pharmacy_code,			pharmacy_type,				open_date,			"
											"		close_date,				pharmacy_code_temp,			pharmacy_type_temp,	"

											"		open_date_temp,			close_date_temp,			restriction_type,	"
											"		description,			description_temp,			unix_recv_date,		"
											"		unix_recv_time,			changed_date,				changed_time,		"
											"		del_flg,				permitted_pharm_owner							"
											"	)																			";
					NumInputColumns		=	20;
					InputColumnSpec		=	"	int, short, int, int, short, int, int, int, short,						"
											"	int, int, short, char(70), char(70), int, int, int, int, short, short	";
					GenerateVALUES		=	1;
					break;


		case AS400SRV_T2086_UPD_MemberPharm:	// Note that unlike most table UPDATEs, this one is conditional - if nothing
												// is new, the UPDATE doesn't happen.
					SQL_CommandText		=	"	UPDATE	MemberPharm								"

											"	SET		seq_number				= ?,			"
											"			pharmacy_code			= ?,			"
											"			pharmacy_type			= ?,			"
											"			open_date				= ?,			"
											"			close_date				= ?,			"
											"			pharmacy_code_temp		= ?,			"
											"			pharmacy_type_temp		= ?,			"
											"			open_date_temp			= ?,			"
											"			close_date_temp			= ?,			"
											"			restriction_type		= ?,			"
											"			description				= ?,			"
											"			description_temp		= ?,			"
											"			unix_recv_date			= ?,			"
											"			unix_recv_time			= ?,			"
											"			changed_date			= ?,			"
											"			changed_time			= ?,			"
											"			del_flg					= ?,			"
											"			permitted_pharm_owner	= ?				"

											"	WHERE	member_id			= ?					"
											"	  AND	mem_id_extension	= ?					"

											"	  AND	(		(seq_number				<> ?)	"
											"				OR	(pharmacy_code			<> ?)	"
											"				OR	(pharmacy_type			<> ?)	"
											"				OR	(open_date				<> ?)	"
											"				OR	(close_date				<> ?)	"
											"				OR	(pharmacy_code_temp		<> ?)	"
											"				OR	(pharmacy_type_temp		<> ?)	"
											"				OR	(open_date_temp			<> ?)	"
											"				OR	(close_date_temp		<> ?)	"
											"				OR	(restriction_type		<> ?)	"
											"				OR	(description			<> ?)	"
											"				OR	(description_temp		<> ?)	"
											"				OR	(del_flg				<> ?)	"
											"				OR	(permitted_pharm_owner	<> ?)	"
											"			)										";
					NumInputColumns		=	34;
					InputColumnSpec		=	"	int, int, short, int, int, int, short, int, int, short, char(70),			"
											"	char(70), int, int, int, int, short, short, int, short, int, int, short,	"
											"	int, int, int, short, int, int, short, char(70), char(70), short, short		";
					break;


		case AS400SRV_T2086_UPD_MemberPharm_recv_timestamp_only:
					SQL_CommandText		=	"	UPDATE	MemberPharm					"

											"	SET		unix_recv_date		= ?,	"
											"			unix_recv_time		= ?		"

											"	WHERE	member_id			= ?		"
											"	  AND	mem_id_extension	= ?		";
					NumInputColumns		=	4;
					InputColumnSpec		=	"	int, int, int, short	";
					break;


		case AS400SRV_T2086PP_DEL_MemberPharm_old_data:
					SQL_CommandText		=	"	DELETE FROM	MemberPharm				"
											"	WHERE		close_date		< ?		"
											"	  AND		close_date_temp	< ?		";
					NumInputColumns		=	2;
					InputColumnSpec		=	"	int, int	";
					break;


		case AS400SRV_T2086PP_READ_table_size:
					SQL_CommandText		=	"	SELECT CAST (count(*) AS INTEGER) FROM MemberPharm	";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					Convert_NF_to_zero	=	1;
					break;


		case AS400SRV_T2096_INS_purchase_lim_class:
					SQL_CommandText		=	"	INSERT INTO purchase_lim_class												"
											"	(																			"
											"		class_code,				class_long_desc,		class_short_desc,		"
											"		class_type,				class_sex,				class_min_age,			"
											"		class_max_age,			class_priority,			class_history_days,		"
											"		prev_sales_span_days,	update_date,			update_time,			"
											"		as400_batch_date,		as400_batch_time								"
											"	)																			";
					NumInputColumns		=	14;	// DonR 16Nov2025 User Story #453336 added prev_sales_span_days.
					InputColumnSpec		=	"	short, char(75), char(25), short, short, short, short, short, int, int, int, int, int, int	";
					GenerateVALUES		=	1;
					break;


		case AS400SRV_T2096_UPD_as400_batch_timestamp:
					SQL_CommandText		=	"	UPDATE	purchase_lim_class			"

											"	SET		as400_batch_date	= ?,	"
											"			as400_batch_time	= ?		"

											"	WHERE	class_code	= ?				";
					NumInputColumns		=	3;
					InputColumnSpec		=	"	int, int, short	";
					break;


		case AS400SRV_T2096_UPD_purchase_lim_class:
					SQL_CommandText		=	"	UPDATE	purchase_lim_class							"

											"	SET		class_long_desc			= ?,				"
											"			class_short_desc		= ?,				"
											"			class_type				= ?,				"
											"			class_sex				= ?,				"
											"			class_min_age			= ?,				"
											"			class_max_age			= ?,				"
											"			class_history_days		= ?,				"
											"			class_priority			= ?,				"
											"			prev_sales_span_days	= ?,				"
											"			update_date				= ?,				"
											"			update_time				= ?					"	// 11 updated columns

											"	WHERE			class_code				=  ?		"

											"	  AND	(		class_long_desc			!= ?		"
											"				OR	class_short_desc		!= ?		"
											"				OR	class_type				!= ?		"
											"				OR	class_sex				!= ?		"
											"				OR	class_min_age			!= ?		"
											"				OR	class_max_age			!= ?		"
											"				OR	class_history_days		!= ?		"
											"				OR	class_priority			!= ?		"
											"				OR	prev_sales_span_days	!= ?	)	";	// 10 criteria columns
					NumInputColumns		=	21;	// DonR 16Nov2025 User Story #453336 added prev_sales_span_days.
					InputColumnSpec		=	"	char(75), char(25), short, short, short, short, int, short, int, int, int,	"	// 11 updated columns
											"	short, char(75), char(25), short, short, short, short, int, short, int		";	// 10 criteria columns
					break;


		case AS400SRV_T2096PP_DEL_purchase_lim_class:
					SQL_CommandText		=	"	DELETE FROM	purchase_lim_class										"

											"	WHERE		(( as400_batch_date	< ?)								"
											"	   OR		 ((as400_batch_date	= ?) AND (as400_batch_time < ?)))	"	// The first part of this line is redundant.

											"	  AND		((class_code <> 0) OR (class_type <> 0))				";
					NumInputColumns		=	3;
					InputColumnSpec		=	"	int, int, int	";
					break;


		case AS400SRV_T2096PP_READ_purchase_lim_class_stale_count:
					SQL_CommandText		=	"	SELECT		CAST (count(*) AS INTEGER)								"

											"	FROM		purchase_lim_class										"

											"	WHERE		(( as400_batch_date	< ?)								"
											"	   OR		 ((as400_batch_date	= ?) AND (as400_batch_time < ?)))	"	// The first part of this line is redundant.

											"	  AND		((class_code <> 0) OR (class_type <> 0))				";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					NumInputColumns		=	3;
					InputColumnSpec		=	"	int, int, int	";
					Convert_NF_to_zero	=	1;
					break;


		case AS400SRV_T2096PP_READ_purchase_lim_class_table_size:
					SQL_CommandText		=	"	SELECT CAST (count(*) AS INTEGER) FROM purchase_lim_class	";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					Convert_NF_to_zero	=	1;
					break;


		case AS400SRV_T4005_INS_drug_list:
					SQL_CommandText		=	"	INSERT INTO drug_list															"
											"	(																				"
											"		largo_code				,													"
											"		largo_type				, priv_pharm_sale_ok	, long_name				,	"
											"		package					, package_size			, package_volume		,	"
											"		drug_book_flg			, member_price_code		,							"
											"		t_half					, cronic_months			, supplemental_1		,	"
											"		supplemental_2			, supplemental_3		, supplier_code			,	"	// 15

											"		computersoft_code		, additional_price		, no_presc_sale_flag	,	"
											"		ishur_qty_lim_ingr		, name_25				, del_flg				,	"
											"		in_health_pack			, pharm_group_code		, short_name			,	"
											"		form_name				, price_prcnt			, price_prcnt_magen		,	"
											"		price_prcnt_keren		, drug_type				, pharm_sale_new		,	"	// 30

											"		status_send				, sale_price			, interact_flg			,	"
											"		magen_wait_months		, keren_wait_months		, del_flg_cont			,	"
											"		drug_book_doct_flg		, openable_pkg			, ishur_required		,	"
											"		ingr_1_code				, ingr_2_code			, ingr_3_code			,	"
											"		ingr_1_quant			, ingr_2_quant			, ingr_3_quant			,	"	// 45

											"		ingr_1_units			, ingr_2_units			, ingr_3_units			,	"
											"		per_1_quant				, per_2_quant			, per_3_quant			,	"
											"		per_1_units				, per_2_units			, per_3_units			,	"
											"		pharm_sale_test			, rule_status			, sensitivity_flag		,	"
											"		sensitivity_code		, sensitivity_severe	, needs_29_g			,	"	// 60

											"		chronic_flag			, shape_code			, shape_code_new		,	"
											"		split_pill_flag			, treatment_typ_cod		, print_generic_flg		,	"
											"		substitute_drug			, copies_to_print		, sale_price_strudel	,	"
											"		preferred_flg			, economypri_group		, parent_group_code		,	"
											"		parent_group_name		, maccabicare_flag		, ishur_qty_lim_flg		,	"	// 75

											"		fps_group_code			, assuta_drug_status	, expiry_date_flag		,	"
											"		bar_code_value			, tikrat_mazon_flag		, class_code			,	"
											"		in_overdose_table		, enabled_status		, enabled_mac			,	"
											"		enabled_hova			, enabled_keva			,							"
											"		price_pct_yahalom		, yahalom_wait_mm		, allow_future_sales	,	"	// 89

											"		cont_treatment			, time_of_day_taken		, treatment_days		,	"
											"		for_cystic_fibro		, for_tuberculosis		, for_gaucher			,	"
											"		for_aids				, for_dialysis			, for_thalassemia		,	"
											"		for_hemophilia			, for_cancer			, for_car_accident		,	"	// 101

											"		for_reserved_1			, for_work_accident		, for_reserved_3		,	"
											"		for_reserved_99			, illness_bitmap		, how_to_take_code		,	"
											"		unit_abbreviation		, economypri_seq		, tight_ishur_limits	,	"	// 110

											"		refresh_date			, refresh_time			, health_basket_new		,	"
											"		as400_batch_date		, as400_batch_time		, ingr_1_quant_std		,	"
											"		changed_date			, changed_time			, ingr_2_quant_std		,	"
											"		pharm_update_date		, pharm_update_time		, ingr_3_quant_std		,	"	// 122

											"		doc_update_date			, doc_update_time		,							"
											"		update_date				, update_time			,							"
											"		update_date_d			, update_time_d			, qualify_future_sales	,	"	// 129 Marianna 14Sep2020 CR #30106
											"		maccabi_profit_rating	,													"	// 130 DonR 27Apr2021 User Story #149963

											"		needs_refrigeration		, needs_patient_instruction	, needs_dilution			,	"	// 133 DonR 22Aug2021 User Story #181694
											"		needs_preparation		, home_delivery_ok			, online_order_pickup_ok	,	"	// 136 DonR 22Aug2021 User Story #181694
											"		is_orthopedic_device	, needs_patient_measurement	,								"	// 138 DonR 22Aug2021 User Story #181694
											"		ConsignmentItem			, tikrat_piryon_pharm_type	,								"	// 140 User Stories 432608 / 376480
											"		is_narcotic				, psychotherapy_drug		, preparation_type			,	"
											"		chronic_days			,															"	// 144 DonR 22Jul2025 User Story #427521/417800

											"		two_pkg_size_levels		, orig_ingr_1_quant			, orig_ingr_2_quant			,	"
											"		orig_ingr_3_quant		, orig_ingr_1_units			, orig_ingr_2_units			,	"
											"		orig_ingr_3_units		, orig_per_1_quant			, orig_per_2_quant			,	"
											"		orig_per_3_quant		, orig_per_1_units			, orig_per_2_units			,	"
											"		orig_per_3_units		, orig_pkg_volume			, orig_pkg_size					"	// 159 DonR 15Jan2026 User Story #450353
											"	)																				";
					NumInputColumns		=	159; // DonR changed from 144 to 159 15Jan2026 User Story #450353
					InputColumnSpec		=	"	int,		char(1),	short,		varchar(30),	char(10),			int,		double,		short,		short,			short,				short,		int,	int,		int,		int,		"	//  15
											"	int,		short,		short,		short,			varchar(25),		short,		short,		short,		varchar(17),	varchar(25),		int,		int,	int,		char(1),	short,		"	//  30
											"	short,		char(5),	char(1),	short,			short,				char(1),	short,		short,		short,			short,				short,		short,	double,		double,		double,		"	//  45
											"	char(3),	char(3),	char(3),	double,			double,				double,		char(3),	char(3),	char(3),		char(1),			short,		short,	int,		short,		short,		"	//  60
											"	short,		short,		short,		short,			short,				short,		int,		short,		char(5),		short,				int,		int,	char(25),	short,		short,		"	//  75
											"	int,		short,		short,		char(14),		short,				short,		short,		short,		short,			short,				short,		int,	short,		short,					"	//  89
											"	short,		short,		short,		short,			short,				short,		short,		short,		short,			short,				short,		short,										"	// 101
											"	short,		short,		short,		short,			int,				short,		char(3),	short,		short,																						"	// 110
											"	int,		int,		short,		int,			int,				double,		int,		int,		double,			int,				int,		double,										"	// 122
											"	int,		int,		int,		int,			int,				int,		short,		short,																									"	// 130
											"	short,		short,		short,		short,			short,				short,		short,		short,		short,			short,				short,		short,	short,		int,					"	// 144 DonR 22Jul2025/06Aug2025 User Story #427521/417800
											"	int,		float,		float,		float,			char(3),			char(3),	char(3),	float,		float,			float,				char(3),	char(3),char(3),	float,		int			";	// 159 DonR 15Jan2026 User Story #450353
					GenerateVALUES		=	1;
					break;


		case AS400SRV_T4005_READ_get_count_from_economypri:
					SQL_CommandText		=	"	SELECT	CAST (count(*) AS INTEGER)	"
											"	FROM	economypri					"
											"	WHERE	largo_code			=  ?	"
											"	  AND	send_and_use_pharm	<> 0	"
											"	  AND	del_flg				<> 'D'	";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					NumInputColumns		=	1;
					InputColumnSpec		=	"	int	";
					Convert_NF_to_zero	=	1;
					break;


		case AS400SRV_T4005_READ_get_count_from_over_dose:
					SQL_CommandText		=	"	SELECT CAST (count(*) AS INTEGER) FROM over_dose WHERE largo_code = ?	";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					NumInputColumns		=	1;
					InputColumnSpec		=	"	int	";
					Convert_NF_to_zero	=	1;
					break;


		case AS400SRV_T4005_READ_get_economypri_item_seq:
					SQL_CommandText		=	"	SELECT	item_seq					"
											"	FROM	economypri					"
											"	WHERE	largo_code			=  ?	"
											"	  AND	group_code			=  ?	"
											"	  AND	send_and_use_pharm	<> 0	"
											"	  AND	del_flg				<> 'D'	";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	short	";
					NumInputColumns		=	2;
					InputColumnSpec		=	"	int, int	";
					break;


		case AS400SRV_T4005_READ_get_min_economypri_group_for_drug:
					SQL_CommandText		=	"	SELECT	MIN (group_code)			"
											"	FROM	economypri					"
											"	WHERE	largo_code			=  ?	"
											"	  AND	send_and_use_pharm	<> 0	"
											"	  AND	del_flg				<> 'D'	";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					NumInputColumns		=	1;
					InputColumnSpec		=	"	int	";
					break;


		case AS400SRV_T4005_READ_old_drug_list_values:
					SQL_CommandText		=	"	SELECT	largo_type				, priv_pharm_sale_ok	, long_name				,	"
											"			package					, package_size			, package_volume		,	"
											"			drug_book_flg			, member_price_code		,							"
											"			t_half					, cronic_months			, supplemental_1		,	"
											"			supplemental_2			, supplemental_3		, supplier_code			,	"	// 14

											"			computersoft_code		, additional_price		, no_presc_sale_flag	,	"
											"			ishur_qty_lim_ingr		, name_25				, del_flg				,	"
											"			in_health_pack			, pharm_group_code		, short_name			,	"
											"			form_name				, price_prcnt			, price_prcnt_magen		,	"
											"			price_prcnt_keren		, drug_type				, pharm_sale_new		,	"	// 29

											"			status_send				, sale_price			, interact_flg			,	"
											"			magen_wait_months		, keren_wait_months		, del_flg_cont			,	"
											"			drug_book_doct_flg		, openable_pkg			, ishur_required		,	"
											"			ingr_1_code				, ingr_2_code			, ingr_3_code			,	"
											"			ingr_1_quant			, ingr_2_quant			, ingr_3_quant			,	"	// 44

											"			ingr_1_units			, ingr_2_units			, ingr_3_units			,	"
											"			per_1_quant				, per_2_quant			, per_3_quant			,	"
											"			per_1_units				, per_2_units			, per_3_units			,	"
											"			pharm_sale_test			, rule_status			, sensitivity_flag		,	"
											"			sensitivity_code		, sensitivity_severe	, needs_29_g			,	"	// 59

											"			chronic_flag			, shape_code			, shape_code_new		,	"
											"			split_pill_flag			, treatment_typ_cod		, print_generic_flg		,	"
											"			substitute_drug			, copies_to_print		, sale_price_strudel	,	"
											"			preferred_flg			, economypri_group		, parent_group_code		,	"
											"			parent_group_name		, maccabicare_flag		, ishur_qty_lim_flg		,	"	// 74

											"			fps_group_code			, assuta_drug_status	, expiry_date_flag		,	"
											"			bar_code_value			, tikrat_mazon_flag		, class_code			,	"
											"			enabled_status			, price_pct_yahalom		, yahalom_wait_mm		,	"	// 83

											"			allow_future_sales		, cont_treatment		, time_of_day_taken		,	"
											"			treatment_days			, illness_bitmap		, how_to_take_code		,	"
											"			unit_abbreviation		, economypri_seq		, tight_ishur_limits	,	"	// 92

											"			changed_date			, changed_time									,	"
											"			pharm_update_date		, pharm_update_time								,	"
											"			doc_update_date			, doc_update_time								,	"
											"			update_date				, update_time			, health_basket_new		,	"
											"			update_date_d			, update_time_d			, pharm_diff			,	"	// 104
											"			qualify_future_sales	, maccabi_profit_rating	,							"	// 106 DonR 27Apr2021 User Story #149963
											"			is_narcotic				, psychotherapy_drug	, preparation_type		,	"
											"			chronic_days			,													"	// 110 DonR 22Jul2025/06Aug2025 User Story #427521/417800

											"			two_pkg_size_levels		, orig_ingr_1_quant		, orig_ingr_2_quant		,	"
											"			orig_ingr_3_quant		, orig_ingr_1_units		, orig_ingr_2_units		,	"
											"			orig_ingr_3_units		, orig_per_1_quant		, orig_per_2_quant		,	"
											"			orig_per_3_quant		, orig_per_1_units		, orig_per_2_units		,	"
											"			orig_per_3_units		, orig_pkg_volume		, orig_pkg_size				"	// 125 DonR 15Jan2026 User Story #450353

											"	FROM	drug_list																	"

											"	WHERE	largo_code = ?																";
					NumOutputColumns	=	125;	// DonR 15Jan2026 User Story #450353
					OutputColumnSpec	=	"	char(1),	short,		varchar(30),	char(10),	int,				double,		short,		short,		short,			short,			int,		int,		int,		int,				"	//  14
											"	int,		short,		short,			short,		varchar(25),		short,		short,		short,		varchar(17),	varchar(25),	int,		int,		int,		char(1),	short,	"	//  29
											"	short,		char(5),	char(1),		short,		short,				char(1),	short,		short,		short,			short,			short,		short,		double,		double,		double,	"	//  44
											"	char(3),	char(3),	char(3),		double,		double,				double,		char(3),	char(3),	char(3),		char(1),		short,		short,		int,		short,		short,	"	//  59
											"	short,		short,		short,			short,		short,				short,		int,		short,		char(5),		short,			int,		int,		char(25),	short,		short,	"	//  74
											"	int,		short,		short,			char(14),	short,				short,		short,		int,		short,																					"	//  83
											"	short,		short,		short,			short,		int,				short,		char(3),	short,		short,																					"	//  92
											"	int,		int,		int,			int,		int,				int,		int,		int,		short,			int,			int,		char(50),									"	// 104
											"	short,		short,		short,			short,		short,				int,																														"	// 110 DonR 22Jul2025/06Aug2025 User Story #427521/417800
											"	int,		float,		float,			float,		char(3),			char(3),	char(3),	float,		float,			float,			char(3),	char(3),	char(3),	float,		int		";	// 125 DonR 15Jan2026 User Story #450353
					NumInputColumns		=	1;
					InputColumnSpec		=	"	int	";
					break;


		case AS400SRV_T4005_UPD_drug_list:
					SQL_CommandText		=	"	UPDATE	drug_list														"

											"	SET		largo_type				= ?,	priv_pharm_sale_ok		= ?,	"
											"			long_name				= ?,	package					= ?,	"
											"			package_size			= ?,	package_volume			= ?,	"
											"			drug_book_flg			= ?,	member_price_code		= ?,	"
											"			t_half					= ?,	cronic_months			= ?,	"	// 10

											"			supplemental_1			= ?,	supplemental_2			= ?,	"
											"			supplemental_3			= ?,	supplier_code			= ?,	"
											"			computersoft_code		= ?,	additional_price		= ?,	"
											"			no_presc_sale_flag		= ?,	del_flg					= ?,	"
											"			in_health_pack			= ?,	health_basket_new		= ?,	"	// 20

											"			pharm_group_code		= ?,	short_name				= ?,	"
											"			form_name				= ?,	price_prcnt				= ?,	"
											"			price_prcnt_magen		= ?,	price_prcnt_keren		= ?,	"
											"			drug_type				= ?,	pharm_sale_new			= ?,	"
											"			status_send				= ?,	sale_price				= ?,	"	// 30

											"			interact_flg			= ?,	magen_wait_months		= ?,	"
											"			keren_wait_months		= ?,	drug_book_doct_flg		= ?,	"
											"			del_flg_cont			= ?,	openable_pkg			= ?,	"
											"			ishur_required			= ?,	ingr_1_code				= ?,	"
											"			ingr_2_code				= ?,	ingr_3_code				= ?,	"	// 40

											"			ingr_1_quant			= ?,	ingr_2_quant			= ?,	"
											"			ingr_3_quant			= ?,	ingr_1_quant_std		= ?,	"
											"			ingr_2_quant_std		= ?,	ingr_3_quant_std		= ?,	"
											"			ingr_1_units			= ?,	ingr_2_units			= ?,	"
											"			ingr_3_units			= ?,	per_1_quant				= ?,	"	// 50

											"			per_2_quant				= ?,	per_3_quant				= ?,	"
											"			per_1_units				= ?,	per_2_units				= ?,	"
											"			per_3_units				= ?,	pharm_sale_test			= ?,	"
											"			rule_status				= ?,	sensitivity_flag		= ?,	"
											"			sensitivity_code		= ?,	sensitivity_severe		= ?,	"	// 60

											"			needs_29_g				= ?,	chronic_flag			= ?,	"
											"			shape_code				= ?,	shape_code_new			= ?,	"
											"			split_pill_flag			= ?,	treatment_typ_cod		= ?,	"
											"			print_generic_flg		= ?,	substitute_drug			= ?,	"
											"			copies_to_print			= ?,	sale_price_strudel		= ?,	"	// 70

											"			preferred_flg			= ?,	parent_group_code		= ?,	"
											"			parent_group_name		= ?,	maccabicare_flag		= ?,	"
											"			ishur_qty_lim_flg		= ?,	tight_ishur_limits		= ?,	"
											"			ishur_qty_lim_ingr		= ?,	name_25					= ?,	"
											"			fps_group_code			= ?,	assuta_drug_status		= ?,	"	// 80

											"			expiry_date_flag		= ?,	bar_code_value			= ?,	"
											"			tikrat_mazon_flag		= ?,	class_code				= ?,	"
											"			enabled_status			= ?,	enabled_mac				= ?,	"
											"			enabled_hova			= ?,	enabled_keva			= ?,	"
											"			price_pct_yahalom		= ?,	yahalom_wait_mm			= ?,	"	// 90

											"			allow_future_sales		= ?,	cont_treatment			= ?,	"
											"			time_of_day_taken		= ?,	treatment_days			= ?,	"
											"			for_cystic_fibro		= ?,	for_tuberculosis		= ?,	"
											"			for_gaucher				= ?,	for_aids				= ?,	"
											"			for_dialysis			= ?,	for_thalassemia			= ?,	"	// 100

											"			for_hemophilia			= ?,	for_cancer				= ?,	"
											"			for_car_accident		= ?,	for_reserved_1			= ?,	"
											"			for_work_accident		= ?,	for_reserved_3			= ?,	"
											"			for_reserved_99			= ?,	illness_bitmap			= ?,	"
											"			how_to_take_code		= ?,	unit_abbreviation		= ?,	"	// 110

											"			refresh_date			= ?,	refresh_time			= ?,	"
											"			as400_batch_date		= ?,	as400_batch_time		= ?,	"
											"			changed_date			= ?,	changed_time			= ?,	"
											"			pharm_update_date		= ?,	pharm_update_time		= ?,	"
											"			doc_update_date			= ?,	doc_update_time			= ?,	"	// 120

											"			update_date				= ?,	update_time				= ?,	"
											"			pharm_diff				= ?,	update_date_d			= ?,	"
											"			update_time_d			= ?,	qualify_future_sales	= ?,	"	// 126 Marianna 14Sep2020 CR #30106
											"			maccabi_profit_rating	= ?,									"	// 127 DonR 27Apr2021 User Story #149963

											"			needs_refrigeration		= ?,	needs_patient_instruction	= ?,	"	// DonR 22Aug2021 User Story #181694
											"			needs_dilution			= ?,	needs_preparation			= ?,	"	// DonR 22Aug2021 User Story #181694
											"			home_delivery_ok		= ?,	online_order_pickup_ok		= ?,	"	// DonR 22Aug2021 User Story #181694
											"			is_orthopedic_device	= ?,	needs_patient_measurement	= ?,	"	// 135 DonR 22Aug2021 User Story #181694
											"			ConsignmentItem			= ?,										"	// 136 Marianna 20Apr2023 User Story #432608
											"			tikrat_piryon_pharm_type= ?,										"	// 137 DonR 12Feb2025 User Story #376480
											"			is_narcotic				= ?,	psychotherapy_drug			= ?,	"
											"			preparation_type		= ?,	chronic_days				= ?,	"	// 141 DonR 22Jul2025/06Aug2025 User Story #427521/417800

											"			two_pkg_size_levels		= ?,	orig_ingr_1_quant			= ?,	"
											"			orig_ingr_2_quant		= ?,	orig_ingr_3_quant			= ?,	"
											"			orig_ingr_1_units		= ?,	orig_ingr_2_units			= ?,	"
											"			orig_ingr_3_units		= ?,	orig_per_1_quant			= ?,	"
											"			orig_per_2_quant		= ?,	orig_per_3_quant			= ?,	"
											"			orig_per_1_units		= ?,	orig_per_2_units			= ?,	"
											"			orig_per_3_units		= ?,	orig_pkg_volume				= ?,	"
											"			orig_pkg_size			= ?											"	// 156 DonR 15Jan2026 User Story #450353

											"	WHERE	largo_code = ?														";	// 157
					NumInputColumns		=	157;	// DonR changed from 138 to 142 30Jul2025/06Aug2025 User Stories #427521/417800
					InputColumnSpec		=	"	char(1),	short,			varchar(30),	char(10),	int,				double,		short,		short,			short,		short,		"	//  10
											"	int,		int,			int,			int,		int,				short,		short,		short,			short,		short,		"	//  20
											"	short,		varchar(17),	varchar(25),	int,		int,				int,		char(1),	short,			short,		char(5),	"	//  30
											"	char(1),	short,			short,			short,		char(1),			short,		short,		short,			short,		short,		"	//  40
											"	double,		double,			double,			double,		double,				double,		char(3),	char(3),		char(3),	double,		"	//  50
											"	double,		double,			char(3),		char(3),	char(3),			char(1),	short,		short,			int,		short,		"	//  60
											"	short,		short,			short,			short,		short,				short,		short,		int,			short,		char(5),	"	//  70
											"	short,		int,			char(25),		short,		short,				short,		short,		varchar(25),	int,		short,		"	//  80
											"	short,		char(14),		short,			short,		short,				short,		short,		short,			int,		short,		"	//  90
											"	short,		short,			short,			short,		short,				short,		short,		short,			short,		short,		"	// 100
											"	short,		short,			short,			short,		short,				short,		short,		int,			short,		char(3),	"	// 110
											"	int,		int,			int,			int,		int,				int,		int,		int,			int,		int,		"	// 120
											"	int,		int,			char(50),		int,		int,				short,		short,												"	// 127
											"	short,		short,			short,			short,		short,				short,		short,		short,			short,		short,		"	// 137 DonR 12Feb2025 User Story #376480
											"	short,		short,			short,			int,																							"	// 141 DonR 30Jul2025/06Aug2025 User Stories #427521/417800
											"	int,		float,			float,			float,		char(3),			char(3),	char(3),	float,			float,		float,		"
											"	char(3),	char(3),		char(3),		float,		int,																				"	// 156 DonR 15Jan2026 User Story #450353
											"	int																																			";	// 157 (Put the WHERE value on a separate line for clarity.)
					break;


		case AS400SRV_T4005PP_READ_drug_list_num_active_rows:
					SQL_CommandText		=	"	SELECT	CAST (count(*) AS INTEGER)								"
											"	FROM	drug_list												"
											"	WHERE	del_flg <> 9											"
											"	   OR	((as400_batch_date = ?) AND (as400_batch_time = ?))		";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					NumInputColumns		=	2;
					InputColumnSpec		=	"	int, int	";
					Convert_NF_to_zero	=	1;
					break;



		case AS400SRV_T4005PP_READ_drug_list_table_size:
					SQL_CommandText		=	"	SELECT CAST (count(*) AS INTEGER) FROM drug_list	";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					Convert_NF_to_zero	=	1;
					break;


		case AS400SRV_T4007_INS_over_dose:
					SQL_CommandText		=	"	INSERT INTO over_dose						"
											"	(											"
											"		largo_code,			from_age,			"
											"		update_date,		update_time,		"
											"		as400_batch_date,	as400_batch_time,	"
											"		max_units_per_day,	del_flg				"
											"	)											";
					NumInputColumns		=	8;
					InputColumnSpec		=	"	int, short, int, int, int, int, int, short	";
					GenerateVALUES		=	1;
					break;


		case AS400SRV_T4007_UPD_drug_list:
					SQL_CommandText		=	"	UPDATE	drug_list					"
											"	SET		in_overdose_table	= 1		"
											"	WHERE	largo_code			= ?		"
											"	  AND	in_overdose_table	= 0		";
					NumInputColumns		=	1;
					InputColumnSpec		=	"	int	";
					break;


		case AS400SRV_T4007_UPD_over_dose:
					SQL_CommandText		=	"	UPDATE	over_dose					"
											"	SET		update_date         = ?,	"
											"			update_time         = ?,	"
											"			as400_batch_date	= ?,	"
											"			as400_batch_time	= ?,	"
											"			max_units_per_day	= ?		"
											"	WHERE	largo_code = ?				"
											"	  AND	from_age   = ?				";
					NumInputColumns		=	7;
					InputColumnSpec		=	"	int, int, int, int, int, int, short	";
					break;


		case AS400SRV_T4007PP_DEL_over_dose:
					SQL_CommandText		=	"	DELETE								"
											"	FROM	over_dose					"
											"	WHERE	((as400_batch_date <> ?)	"
											"	   OR	 (as400_batch_time <> ?))	";
					NumInputColumns		=	2;
					InputColumnSpec		=	"	int, int	";
					break;


		case AS400SRV_T4007PP_READ_over_dose_stale_count:
					SQL_CommandText		=	"	SELECT	CAST (count(*) AS INTEGER)	"
											"	FROM	over_dose					"
											"	WHERE	as400_batch_date <> ?		"
											"	   OR	as400_batch_time <> ?		";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					NumInputColumns		=	2;
					InputColumnSpec		=	"	int, int	";
					Convert_NF_to_zero	=	1;
					break;


		case AS400SRV_T4007PP_READ_over_dose_table_size:
					SQL_CommandText		=	"	SELECT CAST (count(*) AS INTEGER) FROM over_dose	";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					Convert_NF_to_zero	=	1;
					break;


		case AS400SRV_T4007PP_UPD_drug_list_clear_OD_flags:
					SQL_CommandText		=	"	UPDATE	drug_list																"
											"	SET		in_overdose_table	=  0												"
											"	WHERE	largo_code			NOT IN	(SELECT DISTINCT largo_code FROM over_dose)	"
											"	  AND	in_overdose_table	<> 0												";
					break;


		case AS400SRV_T4007PP_UPD_drug_list_set_OD_flags:
					SQL_CommandText		=	"	UPDATE	drug_list																"
											"	SET		in_overdose_table	=  1												"
											"	WHERE	largo_code			IN		(SELECT DISTINCT largo_code FROM over_dose)	"
											"	  AND	in_overdose_table	=  0												";
					break;


		case AS400SRV_T4009_INS_interaction_type:
					SQL_CommandText		=	"	INSERT INTO interaction_type					"
											"	(												"
											"		interaction_type,		dur_message,		"
											"		update_date,			update_time,		"
											"		as400_batch_date,		as400_batch_time,	"
											"		interaction_level,		del_flg				"
											"	)												";
					NumInputColumns		=	8;
					InputColumnSpec		=	"	short, varchar(40), int, int, int, int, short, short	";
					GenerateVALUES		=	1;
					break;


		case AS400SRV_T4009_UPD_interaction_type:
					SQL_CommandText		=	"	UPDATE	interaction_type			"
											"	SET		dur_message			= ?,	"
											"			interaction_level	= ?,	"
											"			as400_batch_date	= ?,	"
											"			as400_batch_time	= ?,	"
											"			update_date			= ?,	"
											"			update_time			= ?		"
											"	WHERE	interaction_type	= ?		";
					NumInputColumns		=	7;
					InputColumnSpec		=	"	varchar(40), short, int, int, int, int, short	";
					break;


		case AS400SRV_T4009PP_DEL_interaction_type:
					SQL_CommandText		=	"	DELETE								"
											"	FROM	interaction_type			"
											"	WHERE	((as400_batch_date <> ?)	"
											"	   OR	 (as400_batch_time <> ?))	";
					NumInputColumns		=	2;
					InputColumnSpec		=	"	int, int	";
					break;


		case AS400SRV_T4009PP_READ_interaction_type_stale_count:
					SQL_CommandText		=	"	SELECT	CAST (count(*) AS INTEGER)	"
											"	FROM	interaction_type			"
											"	WHERE	as400_batch_date <> ?		"
											"	   OR	as400_batch_time <> ?		";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					NumInputColumns		=	2;
					InputColumnSpec		=	"	int, int	";
					Convert_NF_to_zero	=	1;
					break;


		case AS400SRV_T4009PP_READ_interaction_type_table_size:
					SQL_CommandText		=	"	SELECT CAST (count(*) AS INTEGER) FROM interaction_type	";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					Convert_NF_to_zero	=	1;
					break;


		case AS400SRV_T4011_INS_drug_interaction:
					SQL_CommandText		=	"	INSERT INTO drug_interaction					"
											"	(												"
											"		first_largo_code,		second_largo_code,	"
											"		update_date,			update_time,		"
											"		as400_batch_date,		as400_batch_time,	"
											"		interaction_type,		del_flg,			"
											"		note1,					note2				"
											"	)												";
					NumInputColumns		=	10;
					InputColumnSpec		=	"	int, int, int, int, int, int, short, short, varchar(40), varchar(40)	";
					GenerateVALUES		=	1;
					break;


		case AS400SRV_T4011_UPD_drug_interaction:
					SQL_CommandText		=	"	UPDATE	drug_interaction			"
											"	SET		update_date			= ?,	"
											"			update_time			= ?,	"
											"			as400_batch_date	= ?,	"
											"			as400_batch_time	= ?,	"
											"			interaction_type	= ?,	"
											"			note1				= ?,	"
											"			note2				= ?		"
											"	WHERE	first_largo_code	= ?		"
											"	  AND	second_largo_code	= ?		";
					NumInputColumns		=	9;
					InputColumnSpec		=	"	int, int, int, int, short, varchar(40), varchar(40), int, int	";
					break;


		case AS400SRV_T4011_UPD_drug_list:
					SQL_CommandText		=	"	UPDATE	drug_list					"
											"	SET		in_drug_interact	= 1		"
											"	WHERE	largo_code			= ?		"
											"	   OR	largo_code			= ?		";
					NumInputColumns		=	2;
					InputColumnSpec		=	"	int, int	";
					break;


		case AS400SRV_T4011PP_DEL_drug_interaction:
					SQL_CommandText		=	"	DELETE								"
											"	FROM	drug_interaction			"
											"	WHERE	((as400_batch_date <> ?)	"
											"	   OR	 (as400_batch_time <> ?))	";
					NumInputColumns		=	2;
					InputColumnSpec		=	"	int, int	";
					break;


		case AS400SRV_T4011PP_READ_drug_interaction_stale_count:
					SQL_CommandText		=	"	SELECT	CAST (count(*) AS INTEGER)	"
											"	FROM	drug_interaction			"
											"	WHERE	as400_batch_date <> ?		"
											"	   OR	as400_batch_time <> ?		";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					NumInputColumns		=	2;
					InputColumnSpec		=	"	int, int	";
					Convert_NF_to_zero	=	1;
					break;


		case AS400SRV_T4011PP_READ_drug_interaction_table_size:
					SQL_CommandText		=	"	SELECT CAST (count(*) AS INTEGER) FROM drug_interaction	";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					Convert_NF_to_zero	=	1;
					break;


		case AS400SRV_T4011PP_UPD_drug_list:
					SQL_CommandText		=	"	UPDATE	drug_list																				"
											"	SET		in_drug_interact	= 0																	"
											"	WHERE	largo_code			NOT IN (SELECT DISTINCT first_largo_code	FROM drug_interaction)	"
											"	  AND	largo_code			NOT IN (SELECT DISTINCT second_largo_code	FROM drug_interaction)	"
											"	  AND	in_drug_interact	> 0																	";
					break;


		case AS400SRV_T4013_GetProfessionData:
					SQL_CommandText		=	"	SELECT	ProfCodeToPharmacy,		ProfDescription		"
											"	FROM	PrescriberProfession						"
											"	WHERE	rx_origin		= 0							"	// = providers in doctors table.
											"	  AND	profession_type	= ?							";
					NumInputColumns		=	1;
					InputColumnSpec		=	"	short	";
					NumOutputColumns	=	2;
					OutputColumnSpec	=	"	short, char(25)	";
					break;


		case AS400SRV_T4013_INS_doctors:
					SQL_CommandText		=	"	INSERT INTO doctors																		"
											"	(																						"
											"		doctor_id,				license_number,		first_name,			last_name,			"
											"		street_and_no,			city,				phone,				drug_contact_phone,	"
											"		profession_type,		ProfCodeToPharmacy,	ProfDescription,						"
											"		update_date,			update_time,		as400_batch_date,	as400_batch_time,	"
											"		del_flg																				"
											"	)																						";
					NumInputColumns		=	16;
					InputColumnSpec		=	"	int, int, varchar(8), varchar(14), varchar(20), varchar(20), varchar(10), char(40), short, short,	"
											"	char(25), int, int, int, int, short	";
					GenerateVALUES		=	1;
					break;


		case AS400SRV_T4013_UPD_doctors:
					SQL_CommandText		=	"	UPDATE	doctors						"
											"	SET		license_number		= ?,	"
											"			first_name			= ?,	"
											"			last_name			= ?,	"
											"			street_and_no		= ?,	"
											"			city				= ?,	"
											"			phone				= ?,	"
											"			drug_contact_phone	= ?,	"
											"			profession_type		= ?,	"
											"			ProfCodeToPharmacy	= ?,	"
											"			ProfDescription		= ?,	"
											"			as400_batch_date	= ?,	"
											"			as400_batch_time	= ?,	"
											"			update_date			= ?,	"
											"			update_time			= ?		"
											"	WHERE	doctor_id			= ?		";
					NumInputColumns		=	15;
					InputColumnSpec		=	"	int, varchar(8), varchar(14), varchar(20), varchar(20), varchar(10), char(40), short, short, char(25),	"
											"	int, int, int, int, int	";
					break;


		case AS400SRV_T4013PP_DEL_doctors:
					SQL_CommandText		=	"	DELETE								"
											"	FROM	doctors						"
											"	WHERE	((as400_batch_date <> ?)	"
											"	   OR	 (as400_batch_time <> ?))	";
					NumInputColumns		=	2;
					InputColumnSpec		=	"	int, int	";
					break;


		case AS400SRV_T4013PP_READ_doctors_stale_count:
					SQL_CommandText		=	"	SELECT	CAST (count(*) AS INTEGER)	"
											"	FROM	doctors						"
											"	WHERE	as400_batch_date <> ?		"
											"	   OR	as400_batch_time <> ?		";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					NumInputColumns		=	2;
					InputColumnSpec		=	"	int, int	";
					Convert_NF_to_zero	=	1;
					break;


		case AS400SRV_T4013PP_READ_doctors_table_size:
					SQL_CommandText		=	"	SELECT CAST (count(*) AS INTEGER) FROM doctors	";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					Convert_NF_to_zero	=	1;
					break;


		case AS400SRV_T4015_INS_speciality_types:
					SQL_CommandText		=	"	INSERT INTO speciality_types				"
											"	(											"
											"		speciality_code,	description,		"
											"		update_date,		update_time,		"
											"		as400_batch_date,	as400_batch_time,	"
											"		del_flg									"
											"	)											";
					NumInputColumns		=	7;
					InputColumnSpec		=	"	int, varchar(24), int, int, int, int, short	";
					GenerateVALUES		=	1;
					break;


		case AS400SRV_T4015_UPD_speciality_types:
					SQL_CommandText		=	"	UPDATE	speciality_types			"
											"	SET		description			= ?,	"
											"			as400_batch_date	= ?,	"
											"			as400_batch_time	= ?,	"
											"			update_date			= ?,	"
											"			update_time			= ?		"
											"	WHERE	speciality_code		= ?		";
					NumInputColumns		=	6;
					InputColumnSpec		=	"	varchar(24), int, int, int, int, short	";
					break;


		case AS400SRV_T4015PP_DEL_speciality_types:
					SQL_CommandText		=	"	DELETE								"
											"	FROM	speciality_types			"
											"	WHERE	((as400_batch_date <> ?)	"
											"	   OR	 (as400_batch_time <> ?))	";
					NumInputColumns		=	2;
					InputColumnSpec		=	"	int, int	";
					break;


		case AS400SRV_T4015PP_READ_speciality_types_stale_count:
					SQL_CommandText		=	"	SELECT	CAST (count(*) AS INTEGER)	"
											"	FROM	speciality_types			"
											"	WHERE	as400_batch_date <> ?		"
											"	   OR	as400_batch_time <> ?		";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					NumInputColumns		=	2;
					InputColumnSpec		=	"	int, int	";
					Convert_NF_to_zero	=	1;
					break;


		case AS400SRV_T4015PP_READ_speciality_types_table_size:
					SQL_CommandText		=	"	SELECT CAST (count(*) AS INTEGER) FROM speciality_types	";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					Convert_NF_to_zero	=	1;
					break;


		case AS400SRV_T4017_INS_doctor_speciality:
					SQL_CommandText		=	"	INSERT INTO doctor_speciality				"
											"	(											"
											"		doctor_id,			speciality_code,	"
											"		update_date,		update_time,		"
											"		as400_batch_date,	as400_batch_time,	"
											"		del_flg									"
											"	)											";
					NumInputColumns		=	7;
					InputColumnSpec		=	"	int, int, int, int, int, int, short	";
					GenerateVALUES		=	1;
					break;


		case AS400SRV_T4017_UPD_doctor_speciality:
					SQL_CommandText		=	"	UPDATE	doctor_speciality			"
											"	SET		as400_batch_date	= ?,	"
											"			as400_batch_time	= ?,	"
											"			update_date			= ?,	"
											"			update_time			= ?		"
											"	WHERE	doctor_id			= ?		"
											"	  AND	speciality_code		= ?		";
					NumInputColumns		=	6;
					InputColumnSpec		=	"	int, int, int, int, int, int	";
					break;


		case AS400SRV_T4017PP_DEL_doctor_speciality:
					SQL_CommandText		=	"	DELETE								"
											"	FROM	doctor_speciality			"
											"	WHERE	((as400_batch_date <> ?)	"
											"	   OR	 (as400_batch_time <> ?))	";
					NumInputColumns		=	2;
					InputColumnSpec		=	"	int, int	";
					break;


		case AS400SRV_T4017PP_READ_doctor_speciality_stale_count:
					SQL_CommandText		=	"	SELECT	CAST (count(*) AS INTEGER)	"
											"	FROM	doctor_speciality			"
											"	WHERE	as400_batch_date <> ?		"
											"	   OR	as400_batch_time <> ?		";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					NumInputColumns		=	2;
					InputColumnSpec		=	"	int, int	";
					Convert_NF_to_zero	=	1;
					break;


		case AS400SRV_T4017PP_READ_doctor_speciality_table_size:
					SQL_CommandText		=	"	SELECT CAST (count(*) AS INTEGER) FROM doctor_speciality	";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					Convert_NF_to_zero	=	1;
					break;


		case AS400SRV_T4019_INS_spclty_largo_prcnt:
					SQL_CommandText		=	"	INSERT INTO spclty_largo_prcnt											"
											"	(																		"
											"		speciality_code,		largo_code,				basic_price_code,	"
											"		basic_fixed_price,		kesef_price_code,		kesef_fixed_price,	"
											"		zahav_price_code,		zahav_fixed_price,		in_health_basket,	"
											"		health_basket_new,		del_flg,				enabled_status,		"

											"		enabled_mac,			enabled_hova,			enabled_keva,		"
											"		insurance_type,			ptn_percent_sort,		ins_type_sort,		"
											"		yahalom_price_code,		yahalom_fixed_prc,		wait_months,		"
											"		price_code,				fixed_price,								"

											"		update_date,			update_time,								"
											"		refresh_date,			refresh_time,								"
											"		as400_batch_date,		as400_batch_time							"
											"	)																		";
					NumInputColumns		=	29;
					InputColumnSpec		=	"	int, int, short, int, short, int, short, int, short, short, short, short,	"
											"	short, short, short, char(1), short, short, short, int, short, short, int,	"
											"	int, int, int, int, int, int												";
					GenerateVALUES		=	1;
					break;


		case AS400SRV_T4019_READ_spclty_largo_prcnt_old_values:
					SQL_CommandText		=	"	SELECT	basic_price_code,		basic_fixed_price,		kesef_price_code,	"
											"			kesef_fixed_price,		zahav_price_code,		zahav_fixed_price,	"
											"			in_health_basket,		health_basket_new,							"
											"			yahalom_price_code,		yahalom_fixed_prc,		wait_months,		"
											"			update_date,			update_time									"

											"	FROM	spclty_largo_prcnt													"

											"	WHERE	speciality_code	= ?													"
											"	  AND	largo_code		= ?													"
											"	  AND	enabled_status	= ?													"
											"	  AND	insurance_type	= ?													";
					NumOutputColumns	=	13;
					OutputColumnSpec	=	"	short, int, short, int, short, int, short, short, short, int, short, int, int	";
					NumInputColumns		=	4;
					InputColumnSpec		=	"	int, int, short, char(1)	";
					break;


		case AS400SRV_T4019_UPD_spclty_largo_prcnt:
					SQL_CommandText		=	"	UPDATE	spclty_largo_prcnt			"

											"	SET		basic_price_code	= ?,	"
											"			basic_fixed_price	= ?,	"
											"			kesef_price_code	= ?,	"
											"			kesef_fixed_price	= ?,	"
											"			zahav_price_code	= ?,	"
											"			zahav_fixed_price	= ?,	"
											"			ptn_percent_sort	= ?,	"
											"			ins_type_sort		= ?,	"
											"			in_health_basket	= ?,	"
											"			health_basket_new	= ?,	"
											"			enabled_mac			= ?,	"
											"			enabled_hova		= ?,	"
											"			enabled_keva		= ?,	"
											"			del_flg				= ?,	"
											"			yahalom_price_code	= ?,	"
											"			yahalom_fixed_prc	= ?,	"
											"			wait_months			= ?,	"
											"			price_code			= ?,	"
											"			fixed_price			= ?,	"
											"			refresh_date		= ?,	"
											"			refresh_time		= ?,	"
											"			as400_batch_date	= ?,	"
											"			as400_batch_time	= ?,	"
											"			update_date			= ?,	"
											"			update_time			= ?		"

											"	WHERE	speciality_code	= ?			"
											"	  AND	largo_code		= ?			"
											"	  AND	enabled_status	= ?			"
											"	  AND	insurance_type	= ?			";
					NumInputColumns		=	29;
					InputColumnSpec		=	"	short, int, short, int, short, int, short, short, short, short,	"
											"	short, short, short, short, short, int, short, short, int, int,	"
											"	int, int, int, int, int, int, int, short, char(1)				";
					break;


		case AS400SRV_T4019PP_DEL_spclty_largo_prcnt:
					SQL_CommandText		=	"	DELETE								"
											"	FROM	spclty_largo_prcnt			"
											"	WHERE	((as400_batch_date <> ?)	"
											"	   OR	 (as400_batch_time <> ?))	";
					NumInputColumns		=	2;
					InputColumnSpec		=	"	int, int	";
					break;


		case AS400SRV_T4019PP_READ_spclty_largo_prcnt_stale_count:
					SQL_CommandText		=	"	SELECT	CAST (count(*) AS INTEGER)	"
											"	FROM	spclty_largo_prcnt			"
											"	WHERE	as400_batch_date <> ?		"
											"	   OR	as400_batch_time <> ?		";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					NumInputColumns		=	2;
					InputColumnSpec		=	"	int, int	";
					Convert_NF_to_zero	=	1;
					break;


		case AS400SRV_T4019PP_READ_spclty_largo_prcnt_table_size:
					SQL_CommandText		=	"	SELECT CAST (count(*) AS INTEGER) FROM spclty_largo_prcnt	";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					Convert_NF_to_zero	=	1;
					break;


		case AS400SRV_T4021_INS_doctor_percents:
					SQL_CommandText		=	"	INSERT INTO doctor_percents											"
											"	(																	"
											"		doctor_id,				largo_code,			basic_price_code,	"
											"		zahav_price_code,		kesef_price_code,	license,			"
											"		treatment_category,		update_date,		update_time,		"
											"		as400_batch_date,		as400_batch_time,	del_flg				"
											"	)																	";
					NumInputColumns		=	12;
					InputColumnSpec		=	"	int, int, short, short, short, int, short, int, int, int, int, short	";
					GenerateVALUES		=	1;
					break;


		case AS400SRV_T4021_UPD_doctor_percents:
					SQL_CommandText		=	"	UPDATE	doctor_percents				"

											"	SET		basic_price_code	= ?,	"
											"			zahav_price_code	= ?,	"
											"			kesef_price_code	= ?,	"
											"			license				= ?,	"
											"			update_date			= ?,	"
											"			update_time			= ?,	"
											"			as400_batch_date	= ?,	"
											"			as400_batch_time	= ?,	"
											"			del_flg				= ?		"

											"	WHERE	doctor_id			= ?		"
											"	  AND	largo_code			= ?		"
											"	  AND	treatment_category	= ?		";
					NumInputColumns		=	12;
					InputColumnSpec		=	"	short, short, short, int, int, int, int, int, short, int, int, short	";
					break;


		case AS400SRV_T4021_UPD_drug_list_in_doctor_percents:
					SQL_CommandText		=	"	UPDATE	drug_list				"
											"	SET		in_doctor_percents = 1	"
											"	WHERE	largo_code = ?			";
					NumInputColumns		=	1;
					InputColumnSpec		=	"	int	";
					break;


		case AS400SRV_T4021PP_DEL_doctor_percents:
					SQL_CommandText		=	"	DELETE								"
											"	FROM	doctor_percents				"
											"	WHERE	((as400_batch_date <> ?)	"
											"	   OR	 (as400_batch_time <> ?))	";
					NumInputColumns		=	2;
					InputColumnSpec		=	"	int, int	";
					break;


		case AS400SRV_T4021PP_READ_doctor_percents_stale_count:
					SQL_CommandText		=	"	SELECT	CAST (count(*) AS INTEGER)	"
											"	FROM	doctor_percents				"
											"	WHERE	as400_batch_date <> ?		"
											"	   OR	as400_batch_time <> ?		";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					NumInputColumns		=	2;
					InputColumnSpec		=	"	int, int	";
					Convert_NF_to_zero	=	1;
					break;


		case AS400SRV_T4021PP_READ_doctor_percents_table_size:
					SQL_CommandText		=	"	SELECT CAST (count(*) AS INTEGER) FROM doctor_percents	";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					Convert_NF_to_zero	=	1;
					break;


		case AS400SRV_T4021PP_UPD_drug_list_clear_in_doctor_percents:
					SQL_CommandText		=	"	UPDATE	drug_list																						"
											"	SET		in_doctor_percents = 0																			"
											"	WHERE	in_doctor_percents > 0																			"
											"	  AND	NOT EXISTS	(SELECT * FROM doctor_percents														"
											"							WHERE	doctor_percents.largo_code = drug_list.largo_code						"
											"							  AND	(basic_price_code > 0 OR kesef_price_code > 0 OR zahav_price_code > 0)	"
											"							  AND	treatment_category = ?)													";
					NumInputColumns		=	1;
					InputColumnSpec		=	"	short	";
					break;


		case AS400SRV_T4029_INS_member_drug_29g:
					SQL_CommandText		=	"	INSERT INTO member_drug_29g									"
											"	(															"
											"		member_id,			mem_id_extension,	largo_code,		"
											"		start_date,			end_date,			pharmacy_code,	"
											"		form_29g_type,		refresh_date,		refresh_time,	"
											"		as400_batch_date,	as400_batch_time					"
											"	)															";
					NumInputColumns		=	11;
					InputColumnSpec		=	"	int, short, int, int, int, int, short, int, int, int, int	";
					GenerateVALUES		=	1;
					break;


		case AS400SRV_T4029_UPD_member_drug_29g:
					SQL_CommandText		=	"	UPDATE	member_drug_29g				"

											"	SET		start_date			= ?,	"
											"			end_date			= ?,	"
											"			pharmacy_code		= ?,	"
											"			form_29g_type		= ?,	"
											"			refresh_date		= ?,	"
											"			refresh_time		= ?,	"
											"			as400_batch_date	= ?,	"
											"			as400_batch_time	= ?		"

											"	WHERE	member_id			= ?		"
											"	  AND	largo_code			= ?		"
											"	  AND	mem_id_extension	= ?		";
					NumInputColumns		=	11;
					InputColumnSpec		=	"	int, int, int, short, int, int, int, int, int, int, short	";
					break;


		case AS400SRV_T4029PP_DEL_member_drug_29g:
					SQL_CommandText		=	"	DELETE								"
											"	FROM	member_drug_29g				"
											"	WHERE	((as400_batch_date <> ?)	"
											"	   OR	 (as400_batch_time <> ?))	";
					NumInputColumns		=	2;
					InputColumnSpec		=	"	int, int	";
					break;


		case AS400SRV_T4029PP_READ_member_drug_29g_stale_count:
					SQL_CommandText		=	"	SELECT	CAST (count(*) AS INTEGER)	"
											"	FROM	member_drug_29g				"
											"	WHERE	as400_batch_date <> ?		"
											"	   OR	as400_batch_time <> ?		";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					NumInputColumns		=	2;
					InputColumnSpec		=	"	int, int	";
					Convert_NF_to_zero	=	1;
					break;


		case AS400SRV_T4029PP_READ_member_drug_29g_table_size:
					SQL_CommandText		=	"	SELECT CAST (count(*) AS INTEGER) FROM member_drug_29g	";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					Convert_NF_to_zero	=	1;
					break;


		case AS400SRV_T4030_INS_drug_extension:
					SQL_CommandText		=	"	INSERT INTO drug_extension													"
											"	(																			"
											"		largo_code,				extension_largo_co,			rule_number,		"
											"		treatment_category,		effective_date,				rule_name,			"
											"		confirm_authority,		from_age,					to_age,				"
											"		sex,					must_confirm_deliv,			max_amount_duratio,	"
											"		permission_type,		no_presc_sale_flag,			basic_price_code,	"

											"		basic_fixed_price,		kesef_price_code,			kesef_fixed_price,	"
											"		zahav_price_code,		zahav_fixed_price,			ptn_percent_sort,	"
											"		max_op,					max_units,					update_date,		"
											"		update_time,			changed_date,				changed_time,		"
											"		as400_batch_date,		as400_batch_time,			del_flg,			"

											"		in_health_basket,		health_basket_new,			sort_sequence,		"
											"		extend_rule_days,		ivf_flag,					enabled_status,		"
											"		enabled_mac,			enabled_hova,				enabled_keva,		"
											"		send_and_use,			yahalom_price_code,			yahalom_fixed_prc,	"
											"		wait_months,			insurance_type,				price_code,			"
											"		fixed_price,			ins_type_sort									"
											"	)																			";
					NumInputColumns		=	47;
					InputColumnSpec		=	"	int, char(2), int, short, int, varchar(75), short, short, short, short, short, short, short, short, short,				"
											"	int, short, int, short, int, short, int, int, int, int, int, int, int, int, short,										"
											"	short, short, short, short, char(1), short, short, short, short, short, short, int, short, char(1), short, int, short	";
					GenerateVALUES		=	1;
					break;


		case AS400SRV_T4030_READ_drug_extension_old_data:
					SQL_CommandText		=	"	SELECT	extension_largo_co,		effective_date,			rule_name,			"
											"			confirm_authority,		from_age,				to_age,				"
											"			sex,					basic_price_code,		basic_fixed_price,	"
											"			kesef_price_code,		kesef_fixed_price,		zahav_price_code,	"

											"			zahav_fixed_price,		must_confirm_deliv,		max_op,				"
											"			max_units,				max_amount_duratio,		permission_type,	"
											"			no_presc_sale_flag,		treatment_category,		changed_date,		"
											"			changed_time,			del_flg,				in_health_basket,	"

											"			health_basket_new,		sort_sequence,			extend_rule_days,	"
											"			ivf_flag,				enabled_status,			send_and_use,		"
											"			yahalom_price_code,		yahalom_fixed_prc,		wait_months,		"
											"			rrn																	"

											"	FROM	drug_extension														"

											"	WHERE	%s																	";
					NumOutputColumns	=	34;
					OutputColumnSpec	=	"	char(2), int, varchar(75), short, short, short, short, short, int, short, int, short,	"
											"	int, short, int, int, short, short, short, short, int, int, short, short,				"
											"	short, short, short, char(1), short, short, short, int, short, int						";
					NeedsWhereClauseID	=	1;
					break;


		case AS400SRV_T4030_READ_drug_extension_rrn:
					SQL_CommandText		=	"	SELECT	rrn					"

											"	FROM	drug_extension		"

											"	WHERE	largo_code	= ?		"
											"	  AND	rule_number	= ?		";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					NumInputColumns		=	2;
					InputColumnSpec		=	"	int, int	";
					break;


		case AS400SRV_T4030_UPD_drug_extension:
					SQL_CommandText		=	"	UPDATE	drug_extension				"

											"	SET		extension_largo_co	= ?,	"
											"			effective_date		= ?,	"
											"			rule_name			= ?,	"
											"			confirm_authority	= ?,	"
											"			from_age			= ?,	"
											"			to_age				= ?,	"
											"			sex					= ?,	"
											"			basic_price_code	= ?,	"
											"			basic_fixed_price	= ?,	"
											"			kesef_price_code	= ?,	"

											"			kesef_fixed_price	= ?,	"
											"			zahav_price_code	= ?,	"
											"			zahav_fixed_price	= ?,	"
											"			yahalom_price_code	= ?,	"
											"			yahalom_fixed_prc	= ?,	"
											"			wait_months			= ?,	"
											"			insurance_type		= ?,	"
											"			price_code			= ?,	"
											"			fixed_price			= ?,	"
											"			ptn_percent_sort	= ?,	"

											"			ins_type_sort		= ?,	"
											"			must_confirm_deliv	= ?,	"
											"			max_op				= ?,	"
											"			max_units        	= ?,	"
											"			max_amount_duratio	= ?,	"
											"			permission_type		= ?,	"
											"			no_presc_sale_flag	= ?,	"
											"			treatment_category	= ?,	"
											"			update_date			= ?,	"
											"			update_time			= ?,	"

											"			changed_date		= ?,	"
											"			changed_time		= ?,	"
											"			as400_batch_date	= ?,	"
											"			as400_batch_time	= ?,	"
											"			del_flg				= ?,	"
											"			in_health_basket	= ?,	"
											"			health_basket_new	= ?,	"
											"			sort_sequence		= ?,	"
											"			extend_rule_days	= ?,	"
											"			ivf_flag			= ?,	"

											"			enabled_status		= ?,	"
											"			enabled_mac			= ?,	"
											"			enabled_hova		= ?,	"
											"			enabled_keva		= ?,	"
											"			send_and_use		= ?		"

											"	WHERE	rrn = ?						";
					NumInputColumns		=	46;
					InputColumnSpec		=	"	char(2), int, varchar(75), short, short, short, short, short, int, short,	"
											"	int, short, int, short, int, short, char(1), short, int, short,				"
											"	short, short, int, int, short, short, short, short, int, int,				"
											"	int, int, int, int, short, short, short, short, short, char(1),				"
											"	short, short, short, short, short, int										";
					break;


		case AS400SRV_T4030PP_READ_drug_extension_active_size:
					SQL_CommandText		=	"	SELECT	CAST (count(*) AS INTEGER)	"
											"	FROM	drug_extension				"
											"	WHERE	del_flg = 0					";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					Convert_NF_to_zero	=	1;
					break;


		case AS400SRV_T4030PP_READ_drug_extension_stale_count:
												// Note that this is a little different from the other "stale count" commands!
					SQL_CommandText		=	"	SELECT	CAST (count(*) AS INTEGER)								"
											"	FROM	drug_extension											"
											"	WHERE	del_flg = 0												"
											"	  AND	((as400_batch_date <> ?) OR (as400_batch_time <> ?))	";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					NumInputColumns		=	2;
					InputColumnSpec		=	"	int, int	";
					Convert_NF_to_zero	=	1;
					break;


		case AS400SRV_T4030PP_READ_drug_extension_table_size:
					SQL_CommandText		=	"	SELECT CAST (count(*) AS INTEGER) FROM drug_extension	";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					Convert_NF_to_zero	=	1;
					break;


		case AS400SRV_T4030PP_UPD_drug_extension_logical_delete:
					SQL_CommandText		=	"	UPDATE	drug_extension											"
											"	SET		del_flg			= 1,									"
											"			changed_date	= ?,									"
											"			changed_time	= ?										"
											"	WHERE	del_flg = 0												"
											"	  AND	((as400_batch_date <> ?) OR (as400_batch_time <> ?))	";
					NumInputColumns		=	4;
					InputColumnSpec		=	"	int, int, int, int	";
					break;


		case AS400SRV_T4031_INS_price_list:
					SQL_CommandText		=	"	INSERT	INTO price_list											"
											"	(																"
											"		price_list_code,	largo_code,			macabi_price,		"
											"		yarpa_price,		supplier_price,		supplier_yarpa,		"
											"		date_yarpa,			time_yarpa,			date_macabi,		"

											"		time_macabi,		update_date,		update_time,		"
											"		changed_date,		changed_time,		refresh_date,		"
											"		refresh_time,		as400_batch_date,	as400_batch_time,	"
											"		del_flg														"
											"	)																";
					NumInputColumns		=	19;
					InputColumnSpec		=	"	short, int, int, int, int, int, int, int, int,		"
											"	int, int, int, int, int, int, int, int, int, short	";
					GenerateVALUES		=	1;
					break;


		case AS400SRV_T4031_READ_price_list_old_data:
					SQL_CommandText		=	"	SELECT  macabi_price,		yarpa_price,	supplier_price,		"
											"			supplier_yarpa,		date_yarpa,		time_yarpa,			"
											"			date_macabi,		time_macabi,	changed_date,		"
											"			changed_time,		update_date,	update_time			"

											"	FROM    price_list												"

											"	WHERE   price_list_code = ?										"
											"	  AND   largo_code      = ?										";
					NumOutputColumns	=	12;
					OutputColumnSpec	=	"	int, int, int, int, int, int, int, int, int, int, int, int	";
					NumInputColumns		=	2;
					InputColumnSpec		=	"	short, int	";
					break;


		case AS400SRV_T4031_UPD_price_list:
					SQL_CommandText		=	"	UPDATE	price_list					"

											"	SET		macabi_price		= ?,	"
											"			yarpa_price			= ?,	"
											"			supplier_price		= ?,	"
											"			supplier_yarpa		= ?,	"
											"			date_yarpa			= ?,	"
											"			time_yarpa			= ?,	"
											"			date_macabi			= ?,	"
											"			time_macabi			= ?,	"
											"			refresh_date		= ?,	"
											"			refresh_time		= ?,	"
											"			changed_date		= ?,	"
											"			changed_time		= ?,	"
											"			as400_batch_date	= ?,	"
											"			as400_batch_time	= ?,	"
											"			update_date			= ?,	"
											"			update_time			= ?		"

											"	WHERE   price_list_code		= ?		"
											"	  AND   largo_code			= ?		";
					NumInputColumns		=	18;
					InputColumnSpec		=	"	int, int, int, int, int, int, int, int, int, int, int, int, int, int, int, int, short, int	";
					break;


		case AS400SRV_T4031PP_DEL_price_list:
					SQL_CommandText		=	"	DELETE								"
											"	FROM	price_list					"
											"	WHERE	((as400_batch_date <> ?)	"
											"	   OR	 (as400_batch_time <> ?))	";
					NumInputColumns		=	2;
					InputColumnSpec		=	"	int, int	";
					break;


		case AS400SRV_T4031PP_READ_price_list_stale_count:
					SQL_CommandText		=	"	SELECT	CAST (count(*) AS INTEGER)	"
											"	FROM	price_list					"
											"	WHERE	as400_batch_date <> ?		"
											"	   OR	as400_batch_time <> ?		";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					NumInputColumns		=	2;
					InputColumnSpec		=	"	int, int	";
					Convert_NF_to_zero	=	1;
					break;


		case AS400SRV_T4031PP_READ_price_list_table_size:
					SQL_CommandText		=	"	SELECT CAST (count(*) AS INTEGER) FROM price_list	";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					Convert_NF_to_zero	=	1;
					break;


		case AS400SRV_T4032_INS_member_price:
					SQL_CommandText		=	"	INSERT INTO member_price												"
											"	(																		"
											"		member_price_code,		member_price_prcnt,		ptn_percent_sort,	"
											"		tax,					participation_name,		calc_type_code,		"
											"		member_institued,		yarpa_part_code,		max_pkg_price,		"
											"		update_date,			update_time,			as400_batch_date,	"
											"		as400_batch_time,		del_flg										"
											"	)																		";
					NumInputColumns		=	14;
					InputColumnSpec		=	"	short, short, short, int, char(25), short, short, short, int, int, int, int, int, short	";
					GenerateVALUES		=	1;
					break;


		case AS400SRV_T4032_UPD_member_price:
					SQL_CommandText		=	"	UPDATE	member_price				"
											"	SET		member_price_prcnt	= ?,	"
											"			ptn_percent_sort	= ?,	"
											"			tax					= ?,	"
											"			participation_name	= ?,	"
											"			calc_type_code		= ?,	"
											"			member_institued	= ?,	"
											"			yarpa_part_code		= ?,	"
											"			max_pkg_price		= ?,	"
											"			update_date			= ?,	"
											"			update_time			= ?,	"
											"			as400_batch_date	= ?,	"
											"			as400_batch_time	= ?,	"
											"			del_flg				= ?		"
											"	WHERE	member_price_code	= ?		";
					NumInputColumns		=	14;
					InputColumnSpec		=	"	short, short, int, char(25), short, short, short, int, int, int, int, int, short, short	";
					break;


		case AS400SRV_T4032PP_DEL_member_price:
					SQL_CommandText		=	"	DELETE								"
											"	FROM	member_price				"
											"	WHERE	((as400_batch_date <> ?)	"
											"	   OR	 (as400_batch_time <> ?))	";
					NumInputColumns		=	2;
					InputColumnSpec		=	"	int, int	";
					break;


		case AS400SRV_T4032PP_READ_member_price_stale_count:
					SQL_CommandText		=	"	SELECT	CAST (count(*) AS INTEGER)	"
											"	FROM	member_price				"
											"	WHERE	as400_batch_date <> ?		"
											"	   OR	as400_batch_time <> ?		";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					NumInputColumns		=	2;
					InputColumnSpec		=	"	int, int	";
					Convert_NF_to_zero	=	1;
					break;


		case AS400SRV_T4032PP_READ_member_price_table_size:
					SQL_CommandText		=	"	SELECT CAST (count(*) AS INTEGER) FROM member_price	";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					Convert_NF_to_zero	=	1;
					break;


		case AS400SRV_T4035_INS_suppliers:
					SQL_CommandText		=	"	INSERT INTO suppliers												"
											"	(																	"
											"		supplier_table_cod,		supplier_code,			supplier_name,	"
											"		supplier_type,			street_and_no,			city,			"
											"		zip_code,				phone_1,				phone_2,		"
											"		fax_num,				comm_supplier,			employee_id,	"

											"		allowed_proc_list,		employee_password,		check_type,		"
											"		update_date,			update_time,			del_flg,		"
											"		as400_batch_date,		as400_batch_time,		refresh_date,	"
											"		refresh_time													"
											"	)																	";
					NumInputColumns		=	22;
					InputColumnSpec		=	"	short, int, varchar(25), char(2), varchar(20), varchar(20), int, varchar(10), varchar(10), varchar(10), int, int,	"
											"	char(50), int, short, int, int, short, int, int, int, int															";
					GenerateVALUES		=	1;
					break;


		case AS400SRV_T4035_READ_suppliers_old_data:
					SQL_CommandText		=	"	SELECT	supplier_name,		supplier_type,		street_and_no,		"
											"			city,				zip_code,			phone_1,			"
											"			phone_2,			fax_num,			comm_supplier,		"

											"			employee_id,		allowed_proc_list,	employee_password,	"
											"			check_type,			update_date,		update_time,		"
											"			del_flg														"

											"	FROM	suppliers													"
			
											"	WHERE	supplier_code		= ?										"
											"	  AND	supplier_table_cod	= ?										";
					NumOutputColumns	=	16;
					OutputColumnSpec	=	"	varchar(25), char(2), varchar(20), varchar(20), int, varchar(10), varchar(10), varchar(10), int,	"
											"	int, varchar(50), int, short, int, int, short														";
					NumInputColumns		=	2;
					InputColumnSpec		=	"	int, short	";
					break;


		case AS400SRV_T4035_UPD_suppliers:
					SQL_CommandText		=	"	UPDATE	suppliers					"

											"	SET		supplier_name		= ?,	"
											"			supplier_type		= ?,	"
											"			street_and_no		= ?,	"
											"			city				= ?,	"
											"			zip_code			= ?,	"
											"			phone_1				= ?,	"
											"			phone_2				= ?,	"
											"			fax_num				= ?,	"
											"			comm_supplier		= ?,	"
											"			employee_id			= ?,	"
											"			allowed_proc_list	= ?,	"
											"			employee_password	= ?,	"
											"			check_type			= ?,	"
											"			update_date			= ?,	"
											"			update_time			= ?,	"
											"			refresh_date		= ?,	"
											"			refresh_time		= ?,	"
											"			as400_batch_date	= ?,	"
											"			as400_batch_time	= ?,	"
											"			del_flg				= ?		"
							
											"	WHERE	supplier_code		= ?		"
											"	  AND	supplier_table_cod	= ?		";
					NumInputColumns		=	22;
					InputColumnSpec		=	"	varchar(25), char(2), varchar(20), varchar(20), int, varchar(10), varchar(10), varchar(10), int,	"
											"	int, char(50), int, short, int, int, int, int, int, int, short, int, short							";
					break;


		case AS400SRV_T4035PP_READ_suppliers_active_size:
					SQL_CommandText		=	"	SELECT	CAST (count(*) AS INTEGER)	"
											"	FROM	suppliers					"
											"	WHERE	del_flg = 0					";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					Convert_NF_to_zero	=	1;
					break;


		case AS400SRV_T4035PP_READ_suppliers_stale_count:
												// Note that this is a little different from the other "stale count" commands!
					SQL_CommandText		=	"	SELECT	CAST (count(*) AS INTEGER)								"
											"	FROM	suppliers												"
											"	WHERE	del_flg = 0												"
											"	  AND	((as400_batch_date <> ?) OR (as400_batch_time <> ?))	";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					NumInputColumns		=	2;
					InputColumnSpec		=	"	int, int	";
					Convert_NF_to_zero	=	1;
					break;


		case AS400SRV_T4035PP_READ_suppliers_table_size:
					SQL_CommandText		=	"	SELECT CAST (count(*) AS INTEGER) FROM suppliers	";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					Convert_NF_to_zero	=	1;
					break;


		case AS400SRV_T4035PP_UPD_suppliers_logical_delete:
					SQL_CommandText		=	"	UPDATE	suppliers												"

											"	SET		del_flg		= 1,										"
											"			update_date	= ?,										"
											"			update_time	= ?											"

											"	WHERE	del_flg = 0												"
											"	  AND	((as400_batch_date <> ?) OR (as400_batch_time <> ?))	";
					NumInputColumns		=	4;
					InputColumnSpec		=	"	int, int, int, int	";
					break;


		case AS400SRV_T4052_INS_DrugNotes:
					SQL_CommandText		=	"	INSERT INTO DrugNotes											"
											"	(																"
											"		largo_code,			gnrldrugnotetype,	gnrldrugnotecode,	"
											"		update_date,		update_time,		refresh_date,		"
											"		refresh_time,		as400_batch_date,	as400_batch_time,	"
											"		updatedby,			del_flg									"
											"	)																";
					NumInputColumns		=	11;
					InputColumnSpec		=	"	int, char(1), int, int, int, int, int, int, int, int, char(1)	";
					GenerateVALUES		=	1;
					break;


		case AS400SRV_T4052_READ_DrugNotes_old_data:
					SQL_CommandText		=	"	SELECT	update_date,		update_time	"

											"	FROM	DrugNotes						"

											"	WHERE	largo_code			= ?			"
											"	  AND	gnrldrugnotetype	= ?			"
											"	  AND	gnrldrugnotecode	= ?			"
											"	  AND	del_flg				= ?			";
					NumOutputColumns	=	2;
					OutputColumnSpec	=	"	int, int	";
					NumInputColumns		=	4;
					InputColumnSpec		=	"	int, char(1), int, char(1)	";
					break;


		case AS400SRV_T4052_UPD_DrugNotes:
					SQL_CommandText		=	"	UPDATE	DrugNotes					"
					
											"	SET		update_date			= ?,	"
											"			update_time			= ?,	"
											"			refresh_date		= ?,	"
											"			refresh_time		= ?,	"
											"			as400_batch_date	= ?,	"
											"			as400_batch_time	= ?,	"
											"			updatedby			= ?		"

											"	WHERE	largo_code			= ?		"
											"	  AND	gnrldrugnotetype	= ?		"
											"	  AND	gnrldrugnotecode	= ?		"
											"	  AND	del_flg				= ?		";
					NumInputColumns		=	11;
					InputColumnSpec		=	"	int, int, int, int, int, int, int, int, char(1), int, char(1)	";
					break;


		case AS400SRV_T4052PP_stale_DrugNotes_cur:
					SQL_CommandText		=	"	SELECT  largo_code,		gnrldrugnotetype,	gnrldrugnotecode	"

											"	FROM    drugnotes												"

											"	WHERE	del_flg	<> 'D'											"
											"	  AND   ((as400_batch_date	<> ?) OR (as400_batch_time <> ?))	"

											"	FOR UPDATE														";
					NumOutputColumns	=	3;
					OutputColumnSpec	=	"	int, char(1), int	";
					NumInputColumns		=	2;
					InputColumnSpec		=	"	int, int	";
					CursorName			=	"T4052PP_DrugNotes";
					break;


		case AS400SRV_T4052PP_DEL_DrugNotes_old_deletions:
					SQL_CommandText		=	"	DELETE FROM	drugnotes				"

											"	WHERE  largo_code			= ?		"
											"	  AND	gnrldrugnotetype	= ?		"
											"	  AND	gnrldrugnotecode	= ?		"
											"	  AND	del_flg				= 'D'	";
					NumInputColumns		=	3;
					InputColumnSpec		=	"	int, char(1), int	";
					break;


		case AS400SRV_T4052PP_READ_DrugNotes_active_size:
					SQL_CommandText		=	"	SELECT	CAST (count(*) AS INTEGER) FROM drugnotes	"
											"	WHERE	del_flg	<> 'D'								";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					Convert_NF_to_zero	=	1;
					break;


		case AS400SRV_T4052PP_READ_DrugNotes_table_size:
					SQL_CommandText		=	"	SELECT CAST (count(*) AS INTEGER) FROM drugnotes	";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					Convert_NF_to_zero	=	1;
					break;


		case AS400SRV_T4052PP_UPD_DrugNotes_logically_delete:
					SQL_CommandText		=	"	UPDATE	drugnotes						"

											"	SET		del_flg			= 'D',			"
											"			update_date		= ?,			"
											"			update_time		= ?				"

											"	WHERE	CURRENT OF T4052PP_DrugNotes	";
					NumInputColumns		=	2;
					InputColumnSpec		=	"	int, int	";
					break;


		case AS400SRV_T4053_INS_DrugDoctorProf:
					SQL_CommandText		=	"	INSERT INTO DrugDoctorProf												"
											"	(																		"
											"		largo_code,				professiongroup,		enabled_status,		"
											"		price_prcnt_gnrl,		fixed_price_basic,		price_prcnt_keren,	"
											"		fixed_price_kesef,		price_prcnt_magen,		fixed_price_zahav,	"

											"		in_health_basket,		health_basket_new,		price_pct_yahalom,	"
											"		fixed_prc_yahalom,		wait_months,			update_date,		"
											"		update_time,			as400_batch_date,		as400_batch_time,	"
											"		updatedby,				del_flg										"
											"	)																		";
					NumInputColumns		=	20;
					InputColumnSpec		=	"	int, char(2), short, int, int, int, int, int, int,				"
											"	short, short, int, int, short, int, int, int, int, int, char(1)	";
					GenerateVALUES		=	1;
					break;


		case AS400SRV_T4053_READ_DrugDoctorProf_old_data:
					SQL_CommandText		=	"	SELECT	price_prcnt_gnrl,		price_prcnt_keren,		price_prcnt_magen,		"
											"			fixed_price_basic,		fixed_price_kesef,		fixed_price_zahav,		"
											"			in_health_basket,		health_basket_new,		price_pct_yahalom,		"
											"			fixed_prc_yahalom,		wait_months,			update_date,			"
											"			update_time,			del_flg											"

											"	FROM	DrugDoctorProf															"

											"	WHERE	largo_code		= ?														"
											"	  AND	professiongroup	= ?														"
											"	  AND	enabled_status	= ?														";
					NumOutputColumns	=	14;
					OutputColumnSpec	=	"	int, int, int, int, int, int, short, short, int, int, short, int, int, char(1)	";
					NumInputColumns		=	3;
					InputColumnSpec		=	"	int, char(2), short	";
					break;


		case AS400SRV_T4053_UPD_DrugDoctorProf:
					SQL_CommandText		=	"	UPDATE	DrugDoctorProf				"
											"	SET		price_prcnt_gnrl	= ?,	"
											"			price_prcnt_keren	= ?,	"
											"			price_prcnt_magen	= ?,	"
											"			fixed_price_basic	= ?,	"
											"			fixed_price_kesef	= ?,	"
											"			fixed_price_zahav	= ?,	"
											"			in_health_basket	= ?,	"
											"			health_basket_new	= ?,	"
											"			price_pct_yahalom	= ?,	"
											"			fixed_prc_yahalom	= ?,	"
											"			wait_months			= ?,	"
											"			update_date			= ?,	"
											"			update_time			= ?,	"
											"			as400_batch_date	= ?,	"
											"			as400_batch_time	= ?,	"
											"			updatedby			= ?,	"
											"			del_flg				= ?		"
											"	WHERE	largo_code			= ?		"
											"	  AND	professiongroup		= ?		"
											"	  AND	enabled_status		= ?		";
					NumInputColumns		=	20;
					InputColumnSpec		=	"	int, int, int, int, int, int, short, short, int, int, short, int, int, int, int, int, char(1), int, char(2), short	";
					break;


		case AS400SRV_T4053PP_stale_DrugDoctorProf_cur:
					SQL_CommandText		=	"	SELECT  del_flg													"

											"	FROM    drugdoctorprof											"

											"	WHERE	del_flg	IN ('1', '2', '3', '4', '5')					"
											"	  AND   ((as400_batch_date	<> ?) OR (as400_batch_time <> ?))	"

											"	FOR UPDATE														";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	char(1)	";
					NumInputColumns		=	2;
					InputColumnSpec		=	"	int, int	";
					CursorName			=	"T4053PP_DrgDocProf";
					break;


		case AS400SRV_T4053PP_READ_DrugDoctorProf_active_size:
					SQL_CommandText		=	"	SELECT	CAST (count(*) AS INTEGER)				"
											"	FROM	drugdoctorprof							"
											"	WHERE	del_flg	IN ('1', '2', '3', '4', '5')	";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					Convert_NF_to_zero	=	1;
					break;


		case AS400SRV_T4053PP_READ_DrugDoctorProf_table_size:
					SQL_CommandText		=	"	SELECT CAST (count(*) AS INTEGER) FROM drugdoctorprof	";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					Convert_NF_to_zero	=	1;
					break;


		case AS400SRV_T4053PP_UPD_DrugDoctorProf_logically_delete:
					SQL_CommandText		=	"	UPDATE	drugdoctorprof					"

											"	SET		del_flg			= ?,			"
											"			update_date		= ?,			"
											"			update_time		= ?				"

											"	WHERE	CURRENT OF T4053PP_DrgDocProf	";
					NumInputColumns		=	3;
					InputColumnSpec		=	"	char(1), int, int	";
					break;


		case AS400SRV_T4054_INS_economypri:
					SQL_CommandText		=	"	INSERT INTO EconomyPri													"
											"	(																		"
											"		group_code,				group_nbr,				largo_code,			"
											"		drug_group,				item_seq,				del_flg,			"
											"		system_code,			send_and_use_pharm,		update_date,		"
											"		update_time,			updatedby,				received_date,		"

											"		received_time,			refresh_date,			refresh_time,		"
											"		changed_date,			changed_time,			as400_batch_date,	"
											"		as400_batch_time,		received_seq								"
											"	)																		";
					NumInputColumns		=	20;
					InputColumnSpec		=	"	int, short, int, char(1), short, char(1), char(1), short, int, int, int, int, int, int, int, int, int, int, int, int	";
					GenerateVALUES		=	1;
					break;


		case AS400SRV_T4054_READ_economypri_old_data:
					SQL_CommandText		=	"	SELECT	drug_group,			item_seq,		"
											"			system_code,		del_flg,		"
											"			update_date,		update_time,	"
											"			received_date,		received_time,	"
											"			changed_date,		changed_time	"

											"	FROM	economypri							"

											"	WHERE  	group_code	= ?						"
											"	  AND	group_nbr	= ?						"
											"	  AND	largo_code	= ?						";
					NumOutputColumns	=	10;
					OutputColumnSpec	=	"	char(1), short, char(1), char(1), int, int, int, int, int, int	";
					NumInputColumns		=	3;
					InputColumnSpec		=	"	int, short, int	";
					break;


		// DonR 03Jan2022: Split this UPDATE query into two versions. The first, to be used in intra-day
		// updates, leaves preferred_largo alone; this is necessary because sometimes we get the deletion
		// before we get the replacement, and AS400SRV_T4054_UPD_drug_list_set_economypri_columns does NOT
		// update preferred_largo. The second version is the original, and it's used in the 4054 postprocessor
		// where we run the query only after the full table has been downloaded so we know exactly what's what.
		case AS400SRV_T4054_UPD_drug_list_clear_2_economypri_columns:
					SQL_CommandText		=	"	UPDATE	drug_list											"

											"	SET		economypri_group	=  0,							"
											"			economypri_seq		=  0							"

											"	WHERE	largo_code			=  ?							"
											"	  AND	economypri_group	<> 0							"
											"	  AND	NOT EXISTS	(	SELECT	*							"
											"							FROM	economypri					"
											"							WHERE	largo_code			=  ?	"
											"							  AND	send_and_use_pharm	<> 0	"
											"							  AND	del_flg				<> 'D'	"
											"						)										";
					NumInputColumns		=	2;
					InputColumnSpec		=	"	int, int	";
					break;


		case AS400SRV_T4054_UPD_drug_list_clear_3_economypri_columns:
					SQL_CommandText		=	"	UPDATE	drug_list											"

											"	SET		economypri_group	=  0,							"
											"			economypri_seq		=  0,							"
											"			preferred_largo		=  0							"

											"	WHERE	largo_code			=  ?							"
											"	  AND	economypri_group	<> 0							"
											"	  AND	NOT EXISTS	(	SELECT	*							"
											"							FROM	economypri					"
											"							WHERE	largo_code			=  ?	"
											"							  AND	send_and_use_pharm	<> 0	"
											"							  AND	del_flg				<> 'D'	"
											"						)										";
					NumInputColumns		=	2;
					InputColumnSpec		=	"	int, int	";
					break;


		case AS400SRV_T4054_UPD_drug_list_set_economypri_columns:
					SQL_CommandText		=	"	UPDATE	drug_list					"
											"	SET		economypri_group	= ?,	"
											"			economypri_seq		= ?		"
											"	WHERE	largo_code			= ?		";
					NumInputColumns		=	3;
					InputColumnSpec		=	"	int, short, int	";
					break;


		case AS400SRV_T4054_UPD_economypri_logically_delete_old_version:
					SQL_CommandText		=	"	UPDATE		economypri										"

											"	SET			del_flg				= 'D',						"
											"				update_date			= ?,						"
											"				update_time			= ?,						"
											"				changed_date		= ?,						"
											"				changed_time		= ?,						"
											"				received_date		= ?,						"
											"				received_time		= ?							"

											"	WHERE		send_and_use_pharm	<> 0						"
											"	  AND		del_flg				<> 'D'						"
											"	  AND		del_flg				<> 'd'						"
											"	  AND		largo_code			=  ?						"
											"	  AND		(group_code			<> ?	OR group_nbr <> ?)	";
					NumInputColumns		=	9;
					InputColumnSpec		=	"	int, int, int, int, int, int, int, int, short	";
					break;


		case AS400SRV_T4054_UPD_economypri_new_data:
					SQL_CommandText		=	"	UPDATE	EconomyPri					"

											"	SET		drug_group			= ?,	"
											"			item_seq			= ?,	"
											"			send_and_use_pharm	= ?,	"
											"			del_flg				= ?,	"
											"			system_code			= ?,	"
											"			update_date			= ?,	"
											"			update_time			= ?,	"
											"			updatedby			= ?,	"
											"			received_date		= ?,	"
											"			received_time		= ?,	"
											"			refresh_date		= ?,	"
											"			refresh_time		= ?,	"
											"			changed_date		= ?,	"
											"			changed_time		= ?,	"
											"			as400_batch_date	= ?,	"
											"			as400_batch_time	= ?,	"
											"			received_seq		= ?		"

											"	WHERE  	group_code	= ?				"
											"	  AND	group_nbr	= ?				"
											"	  AND	largo_code	= ?				";
					NumInputColumns		=	20;
					InputColumnSpec		=	"	char(1), short, short, char(1), char(1), int, int, int, int, int, int, int, int, int, int, int, int, int, short, int	";
					break;


		case AS400SRV_T4054PP_stale_economypri_cur:
											// DonR 31Jul2022: Added output of as400_batch_date/time for diagnostics.
					SQL_CommandText		=	"	SELECT  largo_code,			group_code,			system_code,	"
											"			as400_batch_date,	as400_batch_time					"

											"	FROM    economypri												"

											"	WHERE	del_flg		<> 'D'										"
											"	  AND	del_flg		<> 'd'										"
											"	  AND   ((as400_batch_date	<> ?) OR (as400_batch_time <> ?))	"

											"	FOR UPDATE														";
					NumOutputColumns	=	5;
					OutputColumnSpec	=	"	int, int, char(1), int, int	";
					NumInputColumns		=	2;
					InputColumnSpec		=	"	int, int	";
					CursorName			=	"T4054PP_economypri";
					break;


		case AS400SRV_T4054PP_READ_economypri_active_size:
					SQL_CommandText		=	"	SELECT	CAST (count(*) AS INTEGER) FROM economypri	"
											"	WHERE	del_flg	NOT IN ('D', 'd')					";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					Convert_NF_to_zero	=	1;
					break;


		case AS400SRV_T4054PP_READ_economypri_table_size:
					SQL_CommandText		=	"	SELECT CAST (count(*) AS INTEGER) FROM economypri	";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					Convert_NF_to_zero	=	1;
					break;


		case AS400SRV_T4054PP_UPD_economypri_logically_delete:
					SQL_CommandText		=	"	UPDATE	economypri						"

											"	SET		del_flg			= 'D',			"
											"			update_date		= ?,			"
											"			update_time		= ?,			"
											"			received_date	= ?,			"
											"			received_time	= ?,			"
											"			received_seq	= ?				"

											"	WHERE	CURRENT OF T4054PP_economypri	";
					NumInputColumns		=	5;
					InputColumnSpec		=	"	int, int, int, int, int	";
					break;


		case AS400SRV_T4068_INS_gadgets:
					SQL_CommandText		=	"	INSERT INTO gadgets														"
											"	(																		"
											"		gadget_code,			service_desc,			largo_code,			"
											"		service_code,			service_number,			service_type,		"
											"		enabled_status,			enabled_mac,			enabled_hova,		"
											"		enabled_keva,			enabled_pvt_pharm,		enabled_without_rx,	"
											"		PrevPurchaseClassCode,	MinPrevPurchases,		MaxPrevPurchases,	"

											"		refresh_date,			refresh_time,								"
											"		changed_date,			changed_time,								"
											"		as400_batch_date,		as400_batch_time							"
											"	)																		";
					NumInputColumns		=	21;
					InputColumnSpec		=	"	int, varchar(25), int, int, short, short, short, short, short, short,	"
											"	short, short, short, short, short, int, int, int, int, int, int			";
					GenerateVALUES		=	1;
					break;


		case AS400SRV_T4068_READ_gadgets_old_data:
					SQL_CommandText		=	"	SELECT	service_desc,			service_code,		service_number,		"
											"			service_type,			enabled_pvt_pharm,	enabled_status,		"
											"			enabled_mac,			enabled_hova,		enabled_keva,		"
											"			enabled_without_rx,		changed_date,		changed_time,		"
											"			PrevPurchaseClassCode,	MinPrevPurchases,	MaxPrevPurchases	"

											"	FROM	gadgets															"

											"	WHERE	largo_code			= ?											"
											"	  AND	gadget_code			= ?											";
					NumOutputColumns	=	15;
					OutputColumnSpec	=	"	varchar(25), int, short, short, short, short, short, short, short, short, int, int, short, short, short	";
					NumInputColumns		=	2;
					InputColumnSpec		=	"	int, int	";
					break;


		case AS400SRV_T4068_UPD_drug_list_gadget_columns:
					SQL_CommandText		=	"	UPDATE	drug_list				"
											"	SET		in_gadget_table	= 1,	"
											"			update_date		= ?,	"
											"			update_time		= ?		"
											"	WHERE	largo_code		= ?		"
											"	  AND	in_gadget_table	<> 1	";
					NumInputColumns		=	3;
					InputColumnSpec		=	"	int, int, int	";
					break;


		case AS400SRV_T4068_UPD_gadgets:
					SQL_CommandText		=	"	UPDATE	gadgets							"

											"	SET		service_desc			= ?,	"
											"			service_code			= ?,	"
											"			service_number			= ?,	"
											"			service_type			= ?,	"
											"			enabled_status			= ?,	"
											"			enabled_mac				= ?,	"
											"			enabled_hova			= ?,	"
											"			enabled_keva			= ?,	"
											"			enabled_pvt_pharm		= ?,	"
											"			enabled_without_rx		= ?,	"
											"			PrevPurchaseClassCode	= ?,	"
											"			MinPrevPurchases		= ?,	"
											"			MaxPrevPurchases		= ?,	"
											"			refresh_date			= ?,	"
											"			refresh_time			= ?,	"
											"			changed_date			= ?,	"
											"			changed_time			= ?,	"
											"			as400_batch_date		= ?,	"
											"			as400_batch_time		= ?		"

											"	WHERE	largo_code				= ?		"
											"	  AND	gadget_code				= ?		";
					NumInputColumns		=	21;
					InputColumnSpec		=	"	varchar(25), int, short, short, short, short, short, short, short, short,	"
											"	short,	short,	short,	int, int, int, int, int, int, int, int				";
					break;


		case AS400SRV_T4068PP_DEL_gadgets:
					SQL_CommandText		=	"	DELETE								"
											"	FROM	gadgets						"
											"	WHERE	((as400_batch_date <> ?)	"
											"	   OR	 (as400_batch_time <> ?))	";
					NumInputColumns		=	2;
					InputColumnSpec		=	"	int, int	";
					break;


		case AS400SRV_T4068PP_READ_gadgets_stale_count:
					SQL_CommandText		=	"	SELECT	CAST (count(*) AS INTEGER)	"
											"	FROM	gadgets						"
											"	WHERE	as400_batch_date <> ?		"
											"	   OR	as400_batch_time <> ?		";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					NumInputColumns		=	2;
					InputColumnSpec		=	"	int, int	";
					Convert_NF_to_zero	=	1;
					break;


		case AS400SRV_T4068PP_READ_gadgets_table_size:
					SQL_CommandText		=	"	SELECT CAST (count(*) AS INTEGER) FROM gadgets	";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					Convert_NF_to_zero	=	1;
					break;


		case AS400SRV_T4068PP_UPD_drug_list_clear_gadget_flag:
					SQL_CommandText		=	"	UPDATE	drug_list																"
											"	SET		in_gadget_table	= 0,													"
											"			update_date		= ?,													"
											"			update_time		= ?														"
											"	WHERE	in_gadget_table > 0														"
											"	  AND	NOT EXISTS	(	SELECT	*												"
											"							FROM	gadgets											"
											"							WHERE	gadgets.largo_code = drug_list.largo_code	)	";
					NumInputColumns		=	2;
					InputColumnSpec		=	"	int, int	";
					break;


		case AS400SRV_T4071PP_postproc_cur:
					SQL_CommandText		=	"	SELECT  shape_num											"

											"	FROM    usage_instruct										"

											"	WHERE   (( update_date	<> ?) OR							"
											"			 ((update_date	=  ?) AND (update_time <> ?)))		";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	short	";
					NumInputColumns		=	3;
					InputColumnSpec		=	"	int, int, int	";
					CursorName			=	"postproc_4071_cur";
					break;


		case AS400SRV_T4071PP_DEL_usage_instruct:
					SQL_CommandText		=	"	DELETE FROM	usage_instruct WHERE CURRENT OF postproc_4071_cur	";
					break;


		case AS400SRV_T4071PP_READ_usage_instruct_table_size:
					SQL_CommandText		=	"	SELECT CAST (count(*) AS INTEGER) FROM usage_instruct	";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					Convert_NF_to_zero	=	1;
					break;


		case AS400SRV_T4074_INS_drug_extn_doc:
					SQL_CommandText		=	"	INSERT INTO drug_extn_doc															"
											"		(																				"
											"			rule_number				, largo_code			, extension_largo_co	,	"
											"			confirm_authority_		, conf_auth_desc		, from_age				,	"
											"			to_age					, member_price_code		, keren_price_code		,	"
											"			magen_price_code		, member_price_prcnt	, keren_price_prcnt		,	"
											"			magen_price_prcnt		, max_op				, max_units				,	"

											"			max_amount_duratio		, permission_type		, no_presc_sale_flag	,	"
											"			fixed_price				, fixed_price_keren		, fixed_price_magen		,	"
											"			ivf_flag				, refill_period			, rule_name				,	"
											"			in_health_basket		, health_basket_new		, treatment_category	,	"
											"			sex						, needs_29_g			, del_flg_c				,	"

											"			send_and_use			, yahalom_price_code	, yahalom_price_pct		,	"
											"			fixed_prc_yahalom		, wait_months			,							"

											"			update_date				, update_time			,							"
											"			refresh_date			, refresh_time			,							"
											"			as400_batch_date		, as400_batch_time									"
											"		)																				";
					NumInputColumns		=	41;
					InputColumnSpec		=	"	int, int, char(2), short, char(25), short, short, short, short, short, short, short, short, int, int,			"
											"	short, short, short, int, int, int, char(1), short, varchar(75), short, short, short, short, short, char(1),	"
											"	short, short, int, int, short, int, int, int, int, int, int														";
					GenerateVALUES		=	1;
					break;


		case AS400SRV_T4074_READ_drug_extn_doc_old_data:
					SQL_CommandText		=	"	SELECT	extension_largo_co		, confirm_authority_	, conf_auth_desc		,	"
											"			from_age				, to_age				, member_price_code		,	"
											"			keren_price_code		, magen_price_code		, member_price_prcnt	,	"
											"			keren_price_prcnt		, magen_price_prcnt		, max_op				,	"

											"			max_units				, max_amount_duratio	, permission_type		,	"
											"			no_presc_sale_flag		, fixed_price			, fixed_price_keren		,	"
											"			fixed_price_magen		, ivf_flag				, refill_period			,	"
											"			rule_name				, in_health_basket		, health_basket_new		,	"

											"			treatment_category		, sex					, needs_29_g			,	"
											"			del_flg_c				, send_and_use			, yahalom_price_code	,	"
											"			yahalom_price_pct		, fixed_prc_yahalom		, wait_months			,	"
											"			update_date				, update_time										"

											"	FROM	drug_extn_doc																"

											"	WHERE	largo_code	= ?																"
											"	  AND	rule_number	= ?																";
					NumOutputColumns	=	35;
					OutputColumnSpec	=	"	char(2), short, char(25), short, short, short, short, short, short, short, short, int,	"
											"	int, short, short, short, int, int, int, char(1), short, varchar(75), short, short,		"
											"	short, short, short, char(1), short, short, int, int, short, int, int					";
					NumInputColumns		=	2;
					InputColumnSpec		=	"	int, int	";
					break;


		case AS400SRV_T4074_UPD_drug_extn_doc:
					SQL_CommandText		=	"	UPDATE	drug_extn_doc				"

											"	SET		extension_largo_co	= ?,	"
											"			confirm_authority_	= ?,	"
											"			conf_auth_desc		= ?,	"
											"			from_age			= ?,	"
											"			to_age				= ?,	"
											"			member_price_code	= ?,	"
											"			keren_price_code	= ?,	"
											"			magen_price_code	= ?,	"
											"			member_price_prcnt	= ?,	"
											"			keren_price_prcnt	= ?,	"
											"			magen_price_prcnt	= ?,	"
											"			max_op				= ?,	"
											"			max_units        	= ?,	"
											"			max_amount_duratio	= ?,	"

											"			permission_type		= ?,	"
											"			no_presc_sale_flag	= ?,	"
											"			fixed_price			= ?,	"
											"			fixed_price_keren	= ?,	"
											"			fixed_price_magen	= ?,	"
											"			ivf_flag			= ?,	"
											"			refill_period		= ?,	"
											"			rule_name			= ?,	"
											"			in_health_basket	= ?,	"
											"			health_basket_new	= ?,	"
											"			treatment_category	= ?,	"
											"			sex					= ?,	"
											"			needs_29_g			= ?,	"
											"			del_flg_c			= ?,	"

											"			send_and_use		= ?,	"
											"			yahalom_price_code	= ?,	"
											"			yahalom_price_pct	= ?,	"
											"			fixed_prc_yahalom	= ?,	"
											"			wait_months			= ?,	"
											"			refresh_date		= ?,	"
											"			refresh_time		= ?,	"
											"			as400_batch_date	= ?,	"
											"			as400_batch_time	= ?,	"
											"			update_date			= ?,	"
											"			update_time			= ?		"

											"		WHERE	largo_code		= ?		"
											"		  AND	rule_number		= ?		";
					NumInputColumns		=	41;
					InputColumnSpec		=	"	char(2), short, char(25), short, short, short, short, short, short, short, short, int, int, short,		"
											"	short, short, int, int, int, char(1), short, varchar(75), short, short, short, short, short, char(1),	"
											"	short, short, int, int, short, int, int, int, int, int, int, int, int									";
					break;


		case AS400SRV_T4074PP_stale_drug_extn_doc_cur:
					SQL_CommandText		=	"	SELECT  rule_number													"

											"	FROM    drug_extn_doc												"

											"	WHERE   send_and_use		>  0									"
											"	  AND	del_flg_c			<> 'D'									"
											"	  AND   (( as400_batch_date	<> ?) OR								"
											"			 ((as400_batch_date	=  ?) AND (as400_batch_time <> ?)))		";	// Slightly redundant date condition.
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					NumInputColumns		=	3;
					InputColumnSpec		=	"	int, int, int	";
					CursorName			=	"T4074PP_dr_ext_doc";
					break;


		case AS400SRV_T4074PP_READ_drug_extn_doc_active_size:
					SQL_CommandText		=	"	SELECT	CAST (count(*) AS INTEGER)	"
											"	FROM	drug_extn_doc				"
											"	WHERE	del_flg_c <> 'D'			";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					Convert_NF_to_zero	=	1;
					break;


		case AS400SRV_T4074PP_READ_drug_extn_doc_table_size:
					SQL_CommandText		=	"	SELECT CAST (count(*) AS INTEGER) FROM drug_extn_doc	";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					Convert_NF_to_zero	=	1;
					break;


		case AS400SRV_T4074PP_UPD_drug_extn_doc_logically_delete:
					SQL_CommandText		=	"	UPDATE	drug_extn_doc					"

											"	SET		del_flg_c	= 'D',				"
											"			update_date	= ?,				"
											"			update_time	= ?					"

											"	WHERE	CURRENT OF T4074PP_dr_ext_doc	";
					NumInputColumns		=	2;
					InputColumnSpec		=	"	int, int	";
					break;


		case AS400SRV_T4100_INS_coupons:
					SQL_CommandText		=	"	INSERT INTO coupons													"
											"	(																	"
											"		member_id				, mem_id_extension		, del_flg	,	"
											"		expires_cyymm			, expiry_status			, fund_code	,	"
											"		update_date				, update_time			,				"
											"		insert_date				, insert_time			,				"
											"		as400_batch_date		, as400_batch_time						"
											"	)																	";
					NumInputColumns		=	12;
					InputColumnSpec		=	"	int, short, short, int, short, short, int, int, int, int, int, int	";
					GenerateVALUES		=	1;
					break;


		case AS400SRV_T4100_UPD_coupons:
					SQL_CommandText		=	"	UPDATE	coupons						"

											"	SET		expires_cyymm		= ?,	"
											"			expiry_status		= ?,	"
											"			fund_code			= ?,	"
											"			as400_batch_date	= ?,	"
											"			as400_batch_time	= ?		"

											"	WHERE	member_id			= ?		"
											"	  AND	mem_id_extension	= ?		"
											"	  AND	del_flg				= ?		";
					NumInputColumns		=	8;
					InputColumnSpec		=	"	int, short, short, int, int, int, short, short	";
					break;


		case AS400SRV_T4100_UPD_coupons_previously_deleted:
					SQL_CommandText		=	"	UPDATE	coupons						"

											"	SET		del_flg				= ?,	"
											"			expires_cyymm		= ?,	"
											"			expiry_status		= ?,	"
											"			fund_code			= ?,	"
											"			update_date			= ?,	"
											"			update_time			= ?,	"
											"			as400_batch_date	= ?,	"
											"			as400_batch_time	= ?		"

											"	WHERE	member_id			= ?		"
											"	  AND	mem_id_extension	= ?		";
					NumInputColumns		=	10;
					InputColumnSpec		=	"	short, int, short, short, int, int, int, int, int, short	";
					break;


		case AS400SRV_T4100_UPD_members_set_has_coupon_true:
					SQL_CommandText		=	"	UPDATE	members						"

											"	SET		has_coupon			= 1		"

											"	WHERE	member_id			= ?		"
											"	  AND	mem_id_extension	= ?		";
					NumInputColumns		=	2;
					InputColumnSpec		=	"	int, short	";
					CommandIsMirrored	=	0;	// DonR 29Jul2025 - Turning this off, as we're pretty much letting Informix die.
					break;


		case AS400SRV_T4100PP_postproc_cur:
					SQL_CommandText		=	"	SELECT  member_id, mem_id_extension									"

											"	FROM    coupons														"

											"	WHERE   del_flg				=  ?									"
											"	  AND   (( as400_batch_date	<> ?) OR								"
											"			 ((as400_batch_date	=  ?) AND (as400_batch_time <> ?)))		";
					NumOutputColumns	=	2;
					OutputColumnSpec	=	"	int, short	";
					NumInputColumns		=	4;
					InputColumnSpec		=	"	short, int, int, int	";
					CursorName			=	"postproc_4100_cur";
					break;


		case AS400SRV_T4100PP_READ_coupons_active_size:
					SQL_CommandText		=	"	SELECT CAST (count(*) AS INTEGER) FROM coupons WHERE del_flg = ?	";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					NumInputColumns		=	1;
					InputColumnSpec		=	"	short	";
					Convert_NF_to_zero	=	1;
					break;


		case AS400SRV_T4100PP_READ_coupons_table_size:
					SQL_CommandText		=	"	SELECT CAST (count(*) AS INTEGER) FROM coupons	";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					Convert_NF_to_zero	=	1;
					break;


		case AS400SRV_T4100PP_UPD_coupons_logically_delete:
					SQL_CommandText		=	"	UPDATE	coupons							"

											"	SET		del_flg		= ?,				"
											"			update_date	= ?,				"
											"			update_time	= ?					"

											"	WHERE	CURRENT OF postproc_4100_cur	";
					NumInputColumns		=	3;
					InputColumnSpec		=	"	short, int, int	";
					break;


		case AS400SRV_T4100PP_UPD_members_set_has_coupon_false:
					SQL_CommandText		=	"	UPDATE	members						"

											"	SET		has_coupon			= 0		"

											"	WHERE	member_id			= ?		"
											"	  AND	mem_id_extension	= ?		";
					NumInputColumns		=	2;
					InputColumnSpec		=	"	int, short	";
					CommandIsMirrored	=	0;	// DonR 29Jul2025 - Turning this off, as we're pretty much letting Informix die.
					break;


		case AS400SRV_T4101_INS_subsidy_messages:
					SQL_CommandText		=	"	INSERT INTO	subsidy_messages									"
											"	(																"
											"		subsidy_code,		short_desc,			long_desc,			"
											"		update_date,		update_time,		as400_batch_date,	"
											"		as400_batch_time											"
											"	)																";
					NumInputColumns		=	7;
					InputColumnSpec		=	"	short, char(10), char(25), int, int, int, int	";
					GenerateVALUES		=	1;
					break;


		case AS400SRV_T4101_UPD_subsidy_messages:
					SQL_CommandText		=	"	UPDATE	subsidy_messages			"

											"	SET		short_desc			= ?,	"
											"			long_desc			= ?,	"
											"			update_date			= ?,	"
											"			update_time			= ?,	"
											"			as400_batch_date	= ?,	"
											"			as400_batch_time	= ?		"

											"	WHERE	subsidy_code		= ?		";
					NumInputColumns		=	7;
					InputColumnSpec		=	"	char(10), char(25), int, int, int, int, short	";
					break;


		case AS400SRV_T4101PP_postproc_cur:
					SQL_CommandText		=	"	SELECT  subsidy_code												"

											"	FROM    subsidy_messages											"

											"	WHERE   (( as400_batch_date	<> ?) OR								"
											"			 ((as400_batch_date	=  ?) AND (as400_batch_time <> ?)))		";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	short	";
					NumInputColumns		=	3;
					InputColumnSpec		=	"	int, int, int	";
					CursorName			=	"postproc_4101_cur";
					break;


		case AS400SRV_T4101PP_DEL_subsidy_messages:
					SQL_CommandText		=	"	DELETE FROM	subsidy_messages WHERE CURRENT OF postproc_4101_cur	";
					break;


		case AS400SRV_T4101PP_READ_subsidy_messages_table_size:
					SQL_CommandText		=	"	SELECT CAST (count(*) AS INTEGER) FROM subsidy_messages	";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					Convert_NF_to_zero	=	1;
					break;


		case AS400SRV_T4102_INS_tikra_type:
					SQL_CommandText		=	"	INSERT INTO	tikra_type		"
											"	(							"
											"		tikra_type_code,		"
											"		short_desc,				"
											"		long_desc,				"
											"		update_date,			"
											"		update_time,			"
											"		as400_batch_date,		"
											"		as400_batch_time		"
											"	)							";
					NumInputColumns		=	7;
					InputColumnSpec		=	"	char(1), char(15), char(30), int, int, int, int	";
					GenerateVALUES		=	1;
					break;


		case AS400SRV_T4102_UPD_tikra_type:
					SQL_CommandText		=	"	UPDATE	tikra_type					"

											"	SET		short_desc			= ?,	"
											"			long_desc			= ?,	"
											"			update_date			= ?,	"
											"			update_time			= ?,	"
											"			as400_batch_date	= ?,	"
											"			as400_batch_time	= ?		"

											"	WHERE	tikra_type_code		= ?		";
					NumInputColumns		=	7;
					InputColumnSpec		=	"	char(15), char(30), int, int, int, int, char(1)	";
					break;


		case AS400SRV_T4102PP_postproc_cur:
					SQL_CommandText		=	"	SELECT  tikra_type_code												"

											"	FROM    tikra_type													"

											"	WHERE   (( as400_batch_date	<> ?) OR								"
											"			 ((as400_batch_date	=  ?) AND (as400_batch_time <> ?)))		";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	char(1)	";
					NumInputColumns		=	3;
					InputColumnSpec		=	"	int, int, int	";
					CursorName			=	"postproc_4102_cur";
					break;


		case AS400SRV_T4102PP_DEL_tikra_type:
					SQL_CommandText		=	"	DELETE FROM	tikra_type WHERE CURRENT OF postproc_4102_cur	";
					break;


		case AS400SRV_T4102PP_READ_tikra_type_table_size:
					SQL_CommandText		=	"	SELECT CAST (count(*) AS INTEGER) FROM tikra_type	";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					Convert_NF_to_zero	=	1;
					break;


		case AS400SRV_T4103_INS_credit_reasons:
					SQL_CommandText		=	"	INSERT INTO	credit_reasons	"
											"	(							"
											"		credit_reason_code,		"
											"		short_desc,				"
											"		long_desc,				"
											"		update_date,			"
											"		update_time,			"
											"		as400_batch_date,		"
											"		as400_batch_time		"
											"	)							";
					NumInputColumns		=	7;
					InputColumnSpec		=	"	short, char(30), char(50), int, int, int, int	";
					GenerateVALUES		=	1;
					break;


		case AS400SRV_T4103_UPD_credit_reasons:
					SQL_CommandText		=	"	UPDATE	credit_reasons				"

											"	SET		short_desc			= ?,	"
											"			long_desc			= ?,	"
											"			update_date			= ?,	"
											"			update_time			= ?,	"
											"			as400_batch_date	= ?,	"
											"			as400_batch_time	= ?		"

											"	WHERE	credit_reason_code	= ?		";
					NumInputColumns		=	7;
					InputColumnSpec		=	"	char(30), char(50), int, int, int, int, short	";
					break;


		case AS400SRV_T4103PP_postproc_cur:
					SQL_CommandText		=	"	SELECT  credit_reason_code											"

											"	FROM    credit_reasons												"

											"	WHERE   (( as400_batch_date	<> ?) OR								"
											"			 ((as400_batch_date	=  ?) AND (as400_batch_time <> ?)))		";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	short	";
					NumInputColumns		=	3;
					InputColumnSpec		=	"	int, int, int	";
					CursorName			=	"postproc_4103_cur";
					break;


		case AS400SRV_T4103PP_DEL_credit_reasons:
					SQL_CommandText		=	"	DELETE FROM	credit_reasons WHERE CURRENT OF postproc_4103_cur	";
					break;


		case AS400SRV_T4103PP_READ_credit_reasons_table_size:
					SQL_CommandText		=	"	SELECT CAST (count(*) AS INTEGER) FROM credit_reasons	";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					Convert_NF_to_zero	=	1;
					break;


		case AS400SRV_T4104_INS_hmo_membership:
					SQL_CommandText		=	"	INSERT INTO	hmo_membership	"
											"	(							"
											"		organization_code,		"
											"		description,			"
											"		update_date,			"
											"		update_time,			"
											"		as400_batch_date,		"
											"		as400_batch_time		"
											"	)							";
					NumInputColumns		=	6;
					InputColumnSpec		=	"	short, char(15), int, int, int, int	";
					GenerateVALUES		=	1;
					break;


		case AS400SRV_T4104_UPD_hmo_membership:
					SQL_CommandText		=	"	UPDATE	hmo_membership				"

											"	SET		description			= ?,	"
											"			update_date			= ?,	"
											"			update_time			= ?,	"
											"			as400_batch_date	= ?,	"
											"			as400_batch_time	= ?		"

											"	WHERE	organization_code	= ?		";
					NumInputColumns		=	6;
					InputColumnSpec		=	"	char(15), int, int, int, int, short	";
					break;


		case AS400SRV_T4104PP_postproc_cur:
					SQL_CommandText		=	"	SELECT  organization_code											"

											"	FROM    hmo_membership												"

											"	WHERE   (( as400_batch_date	<> ?) OR								"
											"			 ((as400_batch_date	=  ?) AND (as400_batch_time <> ?)))		";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	short	";
					NumInputColumns		=	3;
					InputColumnSpec		=	"	int, int, int	";
					CursorName			=	"postproc_4104_cur";
					break;


		case AS400SRV_T4104PP_DEL_hmo_membership:
					SQL_CommandText		=	"	DELETE FROM	hmo_membership WHERE CURRENT OF postproc_4104_cur	";
					break;


		case AS400SRV_T4104PP_READ_hmo_membership_table_size:
					SQL_CommandText		=	"	SELECT CAST (count(*) AS INTEGER) FROM hmo_membership	";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					Convert_NF_to_zero	=	1;
					break;


		case AS400SRV_T4105_INS_drug_shape:
					SQL_CommandText		=	"	INSERT INTO	drug_shape		"
											"	(							"
											"		drug_shape_code,		"
											"		drug_shape_desc,		"
											"		shape_name_eng,			"
											"		shape_name_heb,			"
											"		calc_op_by_volume,		"
											"		home_delivery_ok,		"
											"		calc_actual_usage,		"
											"		compute_duration,		"	//Marianna 10Aug2023 add compute_duration:User Story #469361
											"		del_flg,				"
											"		update_date,			"
											"		update_time,			"
											"		as400_batch_date,		"
											"		as400_batch_time		"
											
											"	)							";	
					NumInputColumns		=	13;	//Marianna 10Aug2023 12 to 13:User Story #469361
					InputColumnSpec		=	"	short, char(4), char(25), char(25), short, short, short, short, short, int, int, int, int	";
					GenerateVALUES		=	1;
					break;


		case AS400SRV_T4105_UPD_drug_shape:
					SQL_CommandText		=	"	UPDATE	drug_shape					"

											"	SET		drug_shape_desc		= ?,	"
											"			shape_name_eng		= ?,	"
											"			shape_name_heb		= ?,	"
											"			calc_op_by_volume	= ?,	"
											"			home_delivery_ok	= ?,	"
											"			calc_actual_usage	= ?,	"
											"			compute_duration	= ?,	"	//Marianna 10Aug2023 add compute_duration:User Story #469361
											"			del_flg				= ?,	"
											"			update_date			= ?,	"
											"			update_time			= ?,	"
											"			as400_batch_date	= ?,	"
											"			as400_batch_time	= ?		"
											

											"	WHERE	drug_shape_code		= ?		";
					NumInputColumns		=	13;	//Marianna 10Aug2023 12 to 13:User Story #469361
					InputColumnSpec		=	"	char(4), char(25), char(25), short, short, short, short, short, int, int, int, int, short	";
					break;


		case AS400SRV_T4105PP_postproc_cur:
					SQL_CommandText		=	"	SELECT  drug_shape_code												"

											"	FROM    drug_shape													"

											"	WHERE   (( as400_batch_date	<> ?) OR								"
											"			 ((as400_batch_date	=  ?) AND (as400_batch_time <> ?)))		";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	short	";
					NumInputColumns		=	3;
					InputColumnSpec		=	"	int, int, int	";
					CursorName			=	"postproc_4105_cur";
					break;


		case AS400SRV_T4105PP_DEL_drug_shape:
					SQL_CommandText		=	"	DELETE FROM	drug_shape WHERE CURRENT OF postproc_4105_cur	";
					break;


		case AS400SRV_T4105PP_READ_drug_shape_table_size:
					SQL_CommandText		=	"	SELECT CAST (count(*) AS INTEGER) FROM drug_shape	";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					Convert_NF_to_zero	=	1;
					break;


		//Marianna 10Aug2023 :User Story #469361
		case AS400SRV_UPD_drug_list_compute_duration:
					SQL_CommandText		=    "     UPDATE	drug_list																		"
											 "     SET		compute_duration    = (	SELECT compute_duration									"
											 "                                      FROM drug_shape											"     
											 "                                      WHERE drug_shape_code = drug_list.shape_code_new	)	"
											 "     WHERE EXISTS (	SELECT *																"
											 "                      FROM  drug_shape														"
											 "                      WHERE drug_shape_code = drug_list.shape_code_new AND					"
											 "							  drug_shape.compute_duration <> drug_list.compute_duration	)		";
					break;


		case AS400SRV_T4106_INS_drug_diagnoses:
					SQL_CommandText		=	"	INSERT INTO	drug_diagnoses					"
											"	(											"
											"		largo_code,			illness_code,		"
											"		diagnosis_code,							"
											"		as400_batch_date,	as400_batch_time	"
											"	)											";
					NumInputColumns		=	5;
					InputColumnSpec		=	"	int, short, int, int, int	";
					GenerateVALUES		=	1;
					break;


		case AS400SRV_T4106_UPD_drug_diagnoses:
					SQL_CommandText		=	"	UPDATE	drug_diagnoses				"

											"	SET		illness_code		= ?,	"
											"			as400_batch_date	= ?,	"
											"			as400_batch_time	= ?		"

											"	WHERE	largo_code			= ?		"
											"	  AND	diagnosis_code		= ?		";
					NumInputColumns		=	5;
					InputColumnSpec		=	"	short, int, int, int, int	";
					break;


		case AS400SRV_T4106PP_postproc_cur:
					SQL_CommandText		=	"	SELECT  diagnosis_code												"

											"	FROM    drug_diagnoses												"

											"	WHERE   (( as400_batch_date	<> ?) OR								"
											"			 ((as400_batch_date	=  ?) AND (as400_batch_time <> ?)))		";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					NumInputColumns		=	3;
					InputColumnSpec		=	"	int, int, int	";
					CursorName			=	"postproc_4106_cur";
					break;


		case AS400SRV_T4106PP_DEL_drug_diagnoses:
					SQL_CommandText		=	"	DELETE FROM	drug_diagnoses WHERE CURRENT OF postproc_4106_cur	";
					break;


		case AS400SRV_T4106PP_READ_drug_diagnoses_table_size:
					SQL_CommandText		=	"	SELECT CAST (count(*) AS INTEGER) FROM drug_diagnoses	";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					Convert_NF_to_zero	=	1;
					break;


		// how_to_take
		case AS400SRV_T4107_INS_how_to_take:
							SQL_CommandText		=	"	INSERT INTO	how_to_take					"
													"	(											"
													"		how_to_take_code,	how_to_take_desc,	"
													"		update_date,              update_time   "
													"	)											";
							NumInputColumns		=	4;
							InputColumnSpec		=	"	short, char(40), int, int	";
							GenerateVALUES		=	1;
							break;
							
		case AS400SRV_T4107_UPD_how_to_take:
							SQL_CommandText		=	"	UPDATE	how_to_take				"

													"	SET		how_to_take_desc	= ?,	"
													"			update_date			= ?,	"
													"			update_time			= ?		"

													"	WHERE	how_to_take_code	= ?		";
							NumInputColumns		=	4;
							InputColumnSpec		=	"	char(40), int, int, short			";
							break;
		 
		case AS400SRV_T4107PP_postproc_cur:
					SQL_CommandText		=	"	SELECT  how_to_take_code												"

											"	FROM    how_to_take													"

											"	WHERE   (( update_date	<> ?) OR								"
											"			 ((update_date	=  ?) AND (update_time <> ?)))		";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	short	";
					NumInputColumns		=	3;
					InputColumnSpec		=	"	int, int, int	";
					CursorName			=	"postproc_4107_cur";
					break;


		case AS400SRV_T4107PP_DEL_how_to_take:
					SQL_CommandText		=	"	DELETE FROM	how_to_take WHERE CURRENT OF postproc_4107_cur	";
					break;


		case AS400SRV_T4107PP_READ_how_to_take_table_size:
					SQL_CommandText		=	"	SELECT CAST (count(*) AS INTEGER) FROM how_to_take	";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					Convert_NF_to_zero	=	1;
					break;	
							
		// unit_of_measure
		case AS400SRV_T4108_INS_unit_of_measure:
							SQL_CommandText		=	"	INSERT INTO	unit_of_measure							"
													"	(													"
													"		unit_abbreviation  ,	short_desc_english  ,	"
													"		short_desc_hebrew  ,	update_date			,	"
													"		update_time										"
													"	)													";
							NumInputColumns		=	5;
							InputColumnSpec		=	"	char(3), char(8), char(8), int , int	";
							GenerateVALUES		=	1;
							break;
							
							
		case AS400SRV_T4108_UPD_unit_of_measure:
							SQL_CommandText		=	"	UPDATE	unit_of_measure				"

													"	SET		short_desc_english	= ?,	"
													"			short_desc_hebrew	= ?,	"
													"			update_date			= ?,	"
													"			update_time			= ?		"

													"	WHERE	unit_abbreviation	= ?		";
							NumInputColumns		=	5;
							InputColumnSpec		=	"	char(8), char(8), int, int , char(3)";
							break;
		
		case AS400SRV_T4108PP_postproc_cur:
					SQL_CommandText		=	"	SELECT  unit_abbreviation												"

											"	FROM    unit_of_measure													"

											"	WHERE   (( update_date	<> ?) OR								"
											"			 ((update_date	=  ?) AND (update_time <> ?)))		";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	char(3)	";
					NumInputColumns		=	3;
					InputColumnSpec		=	"	int, int, int	";
					CursorName			=	"postproc_4108_cur";
					break;


		case AS400SRV_T4108PP_DEL_unit_of_measure:
					SQL_CommandText		=	"	DELETE FROM	unit_of_measure WHERE CURRENT OF postproc_4108_cur	";
					break;


		case AS400SRV_T4108PP_READ_unit_of_measure_table_size:
					SQL_CommandText		=	"	SELECT CAST (count(*) AS INTEGER) FROM unit_of_measure	";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					Convert_NF_to_zero	=	1;
					break;
					
					
							
		// reason_not_sold
		case AS400SRV_T4109_INS_reason_not_sold:
							SQL_CommandText		=	"	INSERT INTO	reason_not_sold							"
													"	(													"
													"		reason_code			,	reason_desc			,	"
													"		update_date			,	update_time				"
													"	)													";
							NumInputColumns		=	4;
							InputColumnSpec		=	"	short, char(50), int , int	";
							GenerateVALUES		=	1;
							break;
				
				
		case AS400SRV_T4109_UPD_reason_not_sold:
							SQL_CommandText		=	"	UPDATE	reason_not_sold				"

													"	SET		reason_desc			= ?,	"
													"			update_date			= ?,	"
													"			update_time			= ?		"

													"	WHERE	reason_code	= ?		";
							NumInputColumns		=	4;
							InputColumnSpec		=	"	char(50), int, int , short				";
							break;
							
		case AS400SRV_T4109PP_postproc_cur:
					SQL_CommandText		=	"	SELECT  reason_code												"

											"	FROM    reason_not_sold													"

											"	WHERE   (( update_date	<> ?) OR								"
											"			 ((update_date	=  ?) AND (update_time <> ?)))		";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	short	";
					NumInputColumns		=	3;
					InputColumnSpec		=	"	int, int, int	";
					CursorName			=	"postproc_4109_cur";
					break;


		case AS400SRV_T4109PP_DEL_reason_not_sold:
					SQL_CommandText		=	"	DELETE FROM	reason_not_sold WHERE CURRENT OF postproc_4109_cur	";
					break;


		case AS400SRV_T4109PP_READ_reason_not_sold_table_size:
					SQL_CommandText		=	"	SELECT CAST (count(*) AS INTEGER) FROM reason_not_sold	";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					Convert_NF_to_zero	=	1;
					break;


		// usage_instr_reason_changed
		case AS400SRV_T4110_INS_usage_instr_reason_changed:
							SQL_CommandText		=	"	INSERT INTO	usage_instr_reason_changed				"
													"	(													"
													"		reason_changed_code  ,	reason_short_desc  ,	"
													"		reason_long_desc	 ,	update_date		   ,	"
													"		update_time										"
													"	)													";
							NumInputColumns		=	5;
							InputColumnSpec		=	"	short, char(30), char(50), int , int	";
							GenerateVALUES		=	1;
							break;
							
							
		case AS400SRV_T4110_UPD_usage_instr_reason_changed:
							SQL_CommandText		=	"	UPDATE	usage_instr_reason_changed	"

													"	SET		reason_short_desc	= ?,	"
													"			reason_long_desc	= ?,	"
													"			update_date			= ?,	"
													"			update_time			= ?		"

													"	WHERE	reason_changed_code	= ?		";
							NumInputColumns		=	5;
							InputColumnSpec		=	"	char(30), char(50), int, int , short";
							break;

		
		case AS400SRV_T4110PP_postproc_cur:
					SQL_CommandText		=	"	SELECT  reason_changed_code									"

											"	FROM    usage_instr_reason_changed							"

											"	WHERE   (( update_date	<> ?) OR							"
											"			 ((update_date	=  ?) AND (update_time <> ?)))		";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	short	";
					NumInputColumns		=	3;
					InputColumnSpec		=	"	int, int, int	";
					CursorName			=	"postproc_4110_cur";
					break;


		case AS400SRV_T4110PP_DEL_usage_instr_reason_changed:
					SQL_CommandText		=	"	DELETE FROM	usage_instr_reason_changed WHERE CURRENT OF postproc_4110_cur	";
					break;


		case AS400SRV_T4110PP_READ_usage_instr_reason_changed_table_size:
					SQL_CommandText		=	"	SELECT CAST (count(*) AS INTEGER) FROM usage_instr_reason_changed	";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					Convert_NF_to_zero	=	1;
					break;


		// member_blocked_drugs
		case AS400SRV_T4111_INS_member_blocked_drugs:
											// DonR 23Jul2023 User Story 448931/458942: Add restriction_type to
											// member_blocked_drugs; it's part of the table's unique index.
					SQL_CommandText		=	"	INSERT INTO	member_blocked_drugs					"
											"	(													"
											"		member_id			,	mem_id_extension	,	"
											"		largo_code			,	restriction_type	,	"
											"		as400_batch_date	,	as400_batch_time		"
											"	)													";
					NumInputColumns		=	6;
					InputColumnSpec		=	"	int, short, int, short, int, int	";
					GenerateVALUES		=	1;
					break;
							
							
		case AS400SRV_T4111_SetMemberBlockedDrugsFlag:
					SQL_CommandText		=	"	UPDATE	members						"
											"	SET		has_blocked_drugs	= 1		"
											"	WHERE	member_id			= ?		"
											"	  AND	has_blocked_drugs	= 0		"
											"	  AND	mem_id_extension	= ?		";
					NumInputColumns		=	2;
					InputColumnSpec		=	"	int, short	";
					break;


		// DonR 23Jul2023 User Story 448931/458942: Added two new UPDATE queries to set drug_list flags.
		case AS400SRV_T4111_SetMemberTypeExclusionFlag:
					SQL_CommandText		=	"	UPDATE	drug_list							"
											"	SET		has_member_type_exclusion	= 1		"
											"	WHERE	largo_code					= ?		";
					NumInputColumns		=	1;
					InputColumnSpec		=	"	int	";
					break;


		// DonR 23Jul2023 User Story 448931/458942: Added two new UPDATE queries to set drug_list flags.
		case AS400SRV_T4111_SetBypassMemberPharmFlag:
					SQL_CommandText		=	"	UPDATE	drug_list								"
											"	SET		bypass_member_pharm_restriction	= 1		"
											"	WHERE	largo_code						= ?		";
					NumInputColumns		=	1;
					InputColumnSpec		=	"	int	";
					break;


		case AS400SRV_T4111_UPD_member_blocked_drugs:
												// DonR 23Jul2023 User Story 448931/458942: Add restriction_type to
												// member_blocked_drugs; it's part of the table's unique index.
					SQL_CommandText		=	"	UPDATE	member_blocked_drugs		"

											"	SET		as400_batch_date	= ?,	"
											"			as400_batch_time	= ?		"

											"	WHERE	member_id			= ?		"
											"	  AND	largo_code			= ?		"
											"	  AND	mem_id_extension	= ?		"
											"	  AND	restriction_type	= ?		";
					NumInputColumns		=	6;
					InputColumnSpec		=	"	int, int, int, int, short, short	";
					break;

		
		case AS400SRV_T4111PP_postproc_cur:
												// DonR 23Jul2023 User Story 448931/458942: Add restriction_type
												// to member_blocked_drugs; only restriction_type 1 is relevant
												// to individual members, but we still need to SELECT these rows
												// so we can delete them from the table when they disappear from
												// the full-table RKUNIX download.
					SQL_CommandText		=	"	SELECT  member_id, mem_id_extension								"

											"	FROM    member_blocked_drugs									"

											"	WHERE   (( as400_batch_date	<> ?) OR (as400_batch_time <> ?))	";
					NumOutputColumns	=	2;
					OutputColumnSpec	=	"	int, short	";
					NumInputColumns		=	2;
					InputColumnSpec		=	"	int, int	";
					CursorName			=	"postproc_4111_cur";
					break;


		case AS400SRV_T4111PP_DEL_member_blocked_drugs:
					SQL_CommandText		=	"	DELETE FROM	member_blocked_drugs WHERE CURRENT OF postproc_4111_cur	";
					break;


		case AS400SRV_T4111PP_ClearMemberBlockedDrugsFlag:
												// DonR 23Jul2023 User Story 448931/458942: Add restriction_type
												// to member_blocked_drugs; only restriction_type 1 is relevant
												// to individual members.
					SQL_CommandText		=	"	UPDATE	members														"
											"	SET		has_blocked_drugs	= 0										"
											"	WHERE	member_id			= ?										"
											"	  AND	mem_id_extension	= ?										"
											"	  AND	NOT EXISTS (	SELECT	*									"
											"							FROM	member_blocked_drugs				"
											"							WHERE	member_id				= ?			"
											"							  AND	mem_id_extension		= ?			"
											"							  AND	restriction_type		= 1			"	// DonR 23Jul2023 User Story 448931/458942.
											"							  AND	as400_batch_date		= ?			"
											"							  AND	as400_batch_time		= ?		)	";
					NumInputColumns		=	6;
					InputColumnSpec		=	"	int, short, int, short, int, int	";
					break;


		// DonR 23Jul2023 User Story 448931/458942: Added two new UPDATE queries to clear drug_list flags.
		// DonR 13Man2025 User Story #384811: Add 3 new exclusion types for subsets of the Darkonaim.
		// This means changing "restriction_type = 2" to an "IN" criterion.
		case AS400SRV_T4111PP_ClearMemberTypeExclusionFlag:
					SQL_CommandText		=	"	UPDATE	drug_list																		"
											"	SET		has_member_type_exclusion	=  0												"
											"	WHERE	has_member_type_exclusion	<> 0												"
											"	  AND	NOT EXISTS (	SELECT	*														"
											"							FROM	member_blocked_drugs AS mbd								"
//											"							WHERE	restriction_type		=  2							"
											"							WHERE	restriction_type		IN (2, 11, 12, 13)				"
											"							  AND	mbd.largo_code			=  drug_list.largo_code			"
											"							  AND	as400_batch_date		=  ?							"
											"							  AND	as400_batch_time		=  ?						)	";
					NumInputColumns		=	2;
					InputColumnSpec		=	"	int, int	";
					break;


		// DonR 23Jul2023 User Story 448931/458942: Added two new UPDATE queries to clear drug_list flags.
		case AS400SRV_T4111PP_ClearBypassMemberPharmFlag:
					SQL_CommandText		=	"	UPDATE	drug_list																	"
											"	SET		bypass_member_pharm_restriction	=  0										"
											"	WHERE	bypass_member_pharm_restriction	<> 0										"
											"	  AND	NOT EXISTS (	SELECT	*													"
											"							FROM	member_blocked_drugs AS mbd							"
											"							WHERE	restriction_type		= 3							"
											"							  AND	mbd.largo_code			= drug_list.largo_code		"
											"							  AND	as400_batch_date		= ?							"
											"							  AND	as400_batch_time		= ?						)	";
					NumInputColumns		=	2;
					InputColumnSpec		=	"	int, int	";
					break;


		case AS400SRV_T4111PP_READ_member_blocked_drugs_table_size:
					SQL_CommandText		=	"	SELECT CAST (count(*) AS INTEGER) FROM member_blocked_drugs	";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					Convert_NF_to_zero	=	1;
					break;			

