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
		// The operator ID's for all programs are defined in include/MacODBC_MyOperatorIDs.h.


		// Include operations used by GenSql.ec
		#include "GenSql_ODBC_Operators.c"

		case TestReadPcErrorMessage:		// TEMPORARY FOR TESTING.
					SQL_CommandText		=	"	SELECT error_line FROM pc_error_message WHERE c_define_name = ?	";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	varchar(60)	";
					NumInputColumns		=	1;
					InputColumnSpec		=	"	varchar(64)	";
					break;


		case bypass_basket_cur:
					SQL_CommandText		=	"	SELECT	illness_bit_number,		override_basket,	is_actual_disease,		use_diagnoses	"
											"	FROM	illness_codes	"
											"	WHERE	illness_bit_number	> 0	";
					NumOutputColumns	=	4;
					OutputColumnSpec	=	"	short,short,short,short	";
					break;


		case CheckDoctorPercentsTreatmentCategory:
					SQL_CommandText		=	"	SELECT	CAST (count(*) AS INTEGER)										"

											"	FROM	doctor_percents													"

											"	WHERE	largo_code			=  ?										"
											"	  AND	treatment_category	=  ?										"
											"	  AND	((? =  1 AND license	= ?) OR (? <> 1 AND doctor_id	= ?))	"
											"	  AND   del_flg				=  ?										";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					NumInputColumns		=	7;
					InputColumnSpec		=	"	int, short, short, int, short, int, short	";
					Convert_NF_to_zero	=	1;
					StatementIsSticky	=	1;	// DonR 30Jun2020. May not really be necessary, since most ishurim have treatment_category = 0.
					break;


		case CheckForFullySoldDocRx:
					SQL_CommandText		=	"	SELECT		CAST (COUNT(*) AS INTEGER)				"

											"	FROM		doctor_presc							"

											"	WHERE		member_id			=		?			"
											"	  AND		doctor_presc_id		=		?			"
											"	  AND		valid_from_date		=		?			"
											"	  AND		largo_prescribed	=		?			"
											"	  AND		doctor_id			=		?			"
											"	  AND		member_id_code		=		?			"
											"	  AND		sold_status			=		2			";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					NumInputColumns		=	6;
					InputColumnSpec		=	"	int, int, int, int, int, short	";
					StatementIsSticky	=	1;
					Convert_NF_to_zero	=	1;
					break;


		case CheckForPartlySoldDocRx_partly_sold_cur:
					SQL_CommandText		=	"	SELECT DISTINCT sold_status								"

											"	FROM			doctor_presc							"

											"	WHERE			member_id			=		?			"
											"	  AND			doctor_presc_id		=		?			"
											"	  AND			valid_from_date		BETWEEN	? AND ?		"
											"	  AND			doctor_id			=		?			"
											"	  AND		 	member_id_code		=		?			"
											"	  AND			sold_status			IN		(0, 1, 2)	"	 // Should be redundant - no other statuses really exist.

											"	ORDER BY		sold_status								";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	short	";
					NumInputColumns		=	6;
					InputColumnSpec		=	"	int, int, int, short, int, int	";
					StatementIsSticky	=	1;	// DonR 25Mar2020.
					break;


		case CheckForPartlySoldNarcoticsPrescription:
					SQL_CommandText		=	"	SELECT	CAST (COUNT(*) AS INTEGER)							"

											"	FROM	doctor_presc		as rx,							"
											"			pd_rx_link			as rxl,							"
											"			prescription_drugs	as pd							"

											"	WHERE	rx.member_id			=		?					"	// 6 conditions to identify the right doctor prescription.
											"	  AND	rx.doctor_presc_id		=		?					"
											"	  AND	rx.valid_from_date		=		?					"
											"	  AND	rx.largo_prescribed		=		?					"
											"	  AND	rx.doctor_id			=		?					"
											"	  AND	rx.member_id_code		=		?					"
											"	  AND	rx.sold_status			=		1					"	// The current prescription status is partially sold.
											"	  AND	rxl.member_id			=		rx.member_id		"	// Match pd_rx_link row to doctor_presc row.
											"	  AND	rxl.doctor_presc_id		=		rx.doctor_presc_id	"
											"	  AND	rxl.valid_from_date		=		rx.valid_from_date	"
											"	  AND	rxl.largo_prescribed	=		rx.largo_prescribed	"
											"	  AND	rxl.doctor_id			=		rx.doctor_id		"
											"	  AND	rxl.member_id_code		=		rx.member_id_code	"
											"	  AND	rxl.rx_sold_status		=		1					"	// This condition should really be redundant.
											"	  AND	rxl.sold_date			=		rx.last_sold_date	"	// Probably redundant as well - "belt & braces"!
											"	  AND	pd.prescription_id		=		rxl.prescription_id	"	// Match drug sale to pd_rx_link.
											"	  AND	pd.line_no				=		rxl.line_no			"	// ""
											"	  AND	pd.del_flg				=		0					"	// Drug sale is completed, non-deleted, and for a different pharmacy.
											"	  AND	pd.delivered_flg		=		1					"
											"	  AND	pd.pharmacy_code		<>		?					";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					NumInputColumns		=	7;
					InputColumnSpec		=	"	int, int, int, int, int, short, int	";
					Convert_NF_to_zero	=	1;
					break;


		case CheckForPartlySoldPrescriptionSamePharmSameDay:
					SQL_CommandText		=	"	SELECT	CAST (COUNT(*) AS INTEGER)							"

											"	FROM	doctor_presc		as rx,							"
											"			pd_rx_link			as rxl,							"
											"			prescription_drugs	as pd							"

											"	WHERE	rx.member_id			=		?					"	// 6 conditions to identify the right doctor prescription.
											"	  AND	rx.doctor_presc_id		=		?					"
											"	  AND	rx.valid_from_date		=		?					"
											"	  AND	rx.largo_prescribed		=		?					"
											"	  AND	rx.doctor_id			=		?					"
											"	  AND	rx.member_id_code		=		?					"
											"	  AND	rx.sold_status			=		1					"	// The current prescription status is partially sold.

											"	  AND	rxl.member_id			=		rx.member_id		"	// Match pd_rx_link row to doctor_presc row.
											"	  AND	rxl.doctor_presc_id		=		rx.doctor_presc_id	"
											"	  AND	rxl.valid_from_date		=		rx.valid_from_date	"
											"	  AND	rxl.largo_prescribed	=		rx.largo_prescribed	"
											"	  AND	rxl.doctor_id			=		rx.doctor_id		"
											"	  AND	rxl.member_id_code		=		rx.member_id_code	"
											"	  AND	rxl.rx_sold_status		=		1					"	// This condition should really be redundant.
											"	  AND	rxl.sold_date			=		?					"	// The sale happened on a specific date (today).

											"	  AND	pd.prescription_id		=		rxl.prescription_id	"	// Match drug sale to pd_rx_link.
											"	  AND	pd.line_no				=		rxl.line_no			"	// ""
											"	  AND	pd.del_flg				=		0					"	// Drug sale is completed, non-deleted, and for the same pharmacy.
											"	  AND	pd.delivered_flg		=		1					"
											"	  AND	pd.pharmacy_code		=		?					";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					NumInputColumns		=	8;
					InputColumnSpec		=	"	int, int, int, int, int, short, int, int	";
					Convert_NF_to_zero	=	1;
					StatementIsSticky	=	1;
					break;


		case CheckForServiceWithoutCard_READ_CountPharmSales:
					SQL_CommandText		=	"	SELECT		CAST (COUNT(*) AS INTEGER)										"

											"	FROM		prescription_drugs as pd										"

											"	WHERE		member_id			= ?											"
											"	  AND		mem_id_extension	= ?											"
											"	  AND		((date > ?) OR (date = ? AND time > ?))							"	// Sale occurred after member requested service without card...
											"	  AND		((date < ?) OR (date = ? AND time < ?))							"	// ...but before the last 15 minutes (or other interval set in sysparams).
											"	  AND		delivered_flg		= 1											"
											"	  AND		del_flg				= 0											"
											"	  AND		presc_source		> 0											"	// Non-prescription sales are irrelevant.
											"	  AND		EXISTS (SELECT		*											"
											"						FROM		pharmacy as ph								"
											"						WHERE		ph.pharmacy_code	=  pd.pharmacy_code		"
											"						  AND		del_flg				=  0					"
											"						  AND		permission_type		%s	)					";	// Private or Maccabi  pharmacy set by custom WHERE.
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					NumInputColumns		=	8;
					InputColumnSpec		=	"	int, short, int, int, int, int, int, int	";
					NeedsWhereClauseID	=	1;
					Convert_NF_to_zero	=	1;
					break;


		case CheckHealthAlerts_HAR_cursor:
											// DonR 21Apr2020 CR #32576: Added single-character timing flags for drug tests.
					SQL_CommandText		=	"	SELECT	rule_number,		rule_desc1,			rule_desc2,			rule_enabled,	"
											"			rule_valid_from,	rule_valid_until,	msg_code_assigned,					"
											"			member_age_min,		member_age_max,		member_sex,							"
											"			drug_test_mode,		minimum_count,		maximum_count,						"	// 13 columns.
					
											// 40 columns. (Changed from 35 for CR #32576 - added timing flags.)
											// DonR 02Aug2020: Added ASCII() to retrieve the Timing Code as a number, to be mapped
											// automatically to a 1-character TINYINT. This is the only way (at least in Informix)
											// to map to a simple character field without messing around with intermediate buffers.
											// DonR 04Dec2023 User Story #529100: Add Shape Code to drug groups. 45 columns now.
											// DonR 28Feb2024: Add Parent Group Code to drug groups. 50 columns now.
											"			grp1_largo_code,	grp1_pharmacology,	grp1_treatment_typ,	grp1_min_treat_typ,	grp1_max_treat_typ,	grp1_chronic_flag,	grp1_ingredient,	grp1_shape_code,	grp1_parent_group,	ASCII(grp1_timing),	"
											"			grp2_largo_code,	grp2_pharmacology,	grp2_treatment_typ,	grp2_min_treat_typ,	grp2_max_treat_typ,	grp2_chronic_flag,	grp2_ingredient,	grp2_shape_code,	grp2_parent_group,	ASCII(grp2_timing),	"
											"			grp3_largo_code,	grp3_pharmacology,	grp3_treatment_typ,	grp3_min_treat_typ,	grp3_max_treat_typ,	grp3_chronic_flag,	grp3_ingredient,	grp3_shape_code,	grp3_parent_group,	ASCII(grp3_timing),	"
											"			grp4_largo_code,	grp4_pharmacology,	grp4_treatment_typ,	grp4_min_treat_typ,	grp4_max_treat_typ,	grp4_chronic_flag,	grp4_ingredient,	grp4_shape_code,	grp4_parent_group,	ASCII(grp4_timing),	"
											"			grp5_largo_code,	grp5_pharmacology,	grp5_treatment_typ,	grp5_min_treat_typ,	grp5_max_treat_typ,	grp5_chronic_flag,	grp5_ingredient,	grp5_shape_code,	grp5_parent_group,	ASCII(grp5_timing),	"

											// 15 columns. (Changed from 14 for CR #32662 - added SMALLINT/short new_drug_wait_days.)
											"			purchase_hist_days,	presc_sale_only,	lab_check_type,						"
											"			lab_req_code,		lab_req_code_alt_1,	lab_req_code_alt_2,					"
											"			lab_req_code_alt_3,	lab_req_code_alt_4,	lab_req_code_alt_5,					"
											"			lab_result_low,		lab_result_high,	new_drug_wait_days,					"
											"			lab_check_days,		lab_min_age_mm,		lab_max_age_mm,						"

											// 11 columns.
											"			ok_if_seen_prof_1,	ok_if_seen_prof_2,	ok_if_seen_prof_3,	ok_if_seen_prof_4,	ok_if_seen_prof_5,	"
											"			ok_if_seen_prof_6,	ok_if_seen_prof_7,	ok_if_seen_prof_8,	ok_if_seen_prof_9,	ok_if_seen_prof_10,	"
											"			ok_if_seen_within,																					"

											// 6 columns.
											"			invert_rasham,																						"
											"			rasham_code_1,		rasham_code_2,		rasham_code_3,		rasham_code_4,		rasham_code_5,		"

											// 6 columns.
											"			invert_diagnosis,																					"
											"			diagnosis_code_1,	diagnosis_code_2,	diagnosis_code_3,	diagnosis_code_4,	diagnosis_code_5,	"

											// 4 columns.
											"			invert_illnesses,																					"
											"			illness_code_1,		illness_code_2,		illness_code_3,												"
											
											// Marianna 12May2024 Epic #178023 - 4 columns for warning frequency control.
											"			days_between_warnings,	max_num_of_warnings,	warning_period_length,								"
											"			explanation_largo_code,																				"

											// Marianna 20Aug2024 Epic #178023 - 15 columns for memeber/pharmacy location control.
											"			pharmacy_mahoz_1,	pharmacy_mahoz_2,	pharmacy_mahoz_3,											"								
											"			pharmacy_mahoz_4,	pharmacy_mahoz_5,																"

											"			pharmacy_code_1,	pharmacy_code_2,	pharmacy_code_3,											"
											"			pharmacy_code_4,	pharmacy_code_5,																"

											"			member_mahoz_1,		member_mahoz_2,		member_mahoz_3,												"
											"			member_mahoz_4,		member_mahoz_5																	"

											"	FROM	health_alert_rules											"

											"	WHERE	rule_enabled		>  0									"
											"	  AND	msg_code_assigned	>  0									"
											"	  AND	rule_valid_from		<= ?									"
											"	  AND	(rule_valid_until	>= ? OR rule_valid_until < 19900101)	";
					NumOutputColumns	=	124;	//before 108
					OutputColumnSpec	=	"	short,		varchar(80),	varchar(80),	short,		"
											"	int,		int,			short,						"
											"	short,		short,			short,						"
											"	short,		short,			short,						"
					
											// CR #32576: added char(0) timing flag for each drug test.
											// DonR 04Dec2023 User Story #529100: Add Shape Code to drug groups.
											// DonR 28Feb2024: Add Parent Group Code to drug groups. 50 columns now.
											"	int,	short,	short,	short,	short,	short,	short,	short,	int,	char(0),	"
											"	int,	short,	short,	short,	short,	short,	short,	short,	int,	char(0),	"
											"	int,	short,	short,	short,	short,	short,	short,	short,	int,	char(0),	"
											"	int,	short,	short,	short,	short,	short,	short,	short,	int,	char(0),	"
											"	int,	short,	short,	short,	short,	short,	short,	short,	int,	char(0),	"

											"	short,	short,	short,									"
											"	int,	int,	int,									"
											"	int,	int,	int,									"
											"	int,	int,	short,									"
											"	short,	short,	short,									"

											"	int,	int,	int,	int,	int,					"
											"	int,	int,	int,	int,	int,					"
											"	short,													"

											"	char(1),												"
											"	short,		short,		short,		short,		short,	"

											"	char(1),												"
											"	int,		int,		int,		int,		int,	"

											"	char(1),												"
											"	short,		short,		short							"

											// Marianna 12May2024 Epic #178023 - 4 columns for warning frequency control.
											"	short,		short,		short							"
											"	int														"
											
											// Marianna 20Aug2024 Epic #178023 - 15 columns for memeber/pharmacy location control.
											"	short,	short,	short,	short,	short					"
											"	int,	int,	int,	int,	int						"
											"	short,	short,	short,	short,	short					";
					NumInputColumns		=	2;
					InputColumnSpec		=	"	int,	int	";
					break;


		case CheckHealthAlerts_HAR_doctor_rx:
											// DonR 04Dec2023 User Story #529100: Get Shape Code from drug_list. Note that we
											// use shape_code_new, *not* the old shape_code value.
					SQL_CommandText		=	"	SELECT	rx.largo_prescribed,		rx.doctor_presc_id,		rx.valid_from_date,		rx.doctor_id,		"
											"			rx.sold_status,				rx.op,					rx.total_units,			rx.qty_method,		"
											"			rx.treatment_length,		rx.tot_units_sold,		rx.tot_op_sold,								"
											"			dl.pharm_group_code,		dl.treatment_typ_cod,	dl.package_volume,							"
					
											"			CAST(CASE WHEN (dl.chronic_flag = 1) THEN 1 ELSE 0 END AS SMALLINT) AS chronic_1_or_0,			"	// DonR 05Apr2020: Force numeric expression to output a SMALLINT.

											"			dl.ingr_1_code,				dl.ingr_2_code,			dl.ingr_3_code,			dl.shape_code_new,	"
											"			dl.parent_group_code																			"

											"	FROM	doctor_presc	AS rx,								"
											"			drug_list		AS dl								"

											"	WHERE	member_id			=		?						"
											"	  AND	valid_until_date	BETWEEN	?	AND ?				"
											"	  AND	valid_from_date		BETWEEN	?	AND ?				"
											"	  AND	sold_status			<>		?						"
											"	  AND	deleted_status		=		0						"
											"	  AND	member_id_code		=		?						"
											"	  AND	dl.largo_code		=		rx.largo_prescribed		";
					NumOutputColumns	=	20;
					OutputColumnSpec	=	"	int, int, int, int, short, short, short, short,	"
											"	short, short, short, short, short, double		"
											"	short, short, short, short, short, int			";
					NumInputColumns		=	7;
					InputColumnSpec		=	"	int, int, int, int, int, short, short	";
					StatementIsSticky	=	1;	// Made sticky 27Oct2020.
					break;


		case CheckHealthAlerts_READ_DiagnosisCount:
					SQL_CommandText		=	"	SELECT	CAST (COUNT(*) AS INTEGER)						"

											"	FROM	member_diagnoses								"

											"	WHERE	member_id			=  ?						"
											"	  AND	diagnosis_code		IN (	?, ?, ?, ?, ?	)	"
											"	  AND	mem_id_extension	=  ?						";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					NumInputColumns		=	7;
					InputColumnSpec		=	"	int, int, int, int, int, int, short	";
					Convert_NF_to_zero	=	1;
					break;


		case CheckHealthAlerts_READ_DiagnosisCountFromIshurim:
					SQL_CommandText		=	"	SELECT	CAST (COUNT(*) AS INTEGER)						"

											"	FROM	special_prescs									"

											"	WHERE	member_id			=  ?						"
											"	  AND	largo_code			>  0						"	// Just to conform to index structure.
											"	  AND	confirmation_type	=  1						"	// Ishur is active.
											"	  AND	stop_use_date		>= ?						"	// Ishur is still OK on Close Date.
											"	  AND	treatment_start		<= ?						"
											"	  AND	mem_id_extension	=  ?						"
											"	  AND	member_diagnosis	>  0						"
											"	  AND	member_diagnosis	IN (	?, ?, ?, ?, ?	)	";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					NumInputColumns		=	9;
					InputColumnSpec		=	"	int, int, int, short, int, int, int, int, int	";
					Convert_NF_to_zero	=	1;
					break;


		case CheckHealthAlerts_READ_DocVisitCount:
					SQL_CommandText		=	"	SELECT	CAST (COUNT(*) AS INTEGER)										"

											"	FROM	member_prof_visit												"

											"	WHERE	member_id			=  ?										"
											"	  AND	profession_code		IN (	?, ?, ?, ?, ?, ?, ?, ?, ?, ?	)	"
											"	  AND	mem_id_extension	=  ?										"
											"	  AND	visit_date			>= ?										"
											"	  AND	((visit_time		>= ?) OR (visit_date > ?))					";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					NumInputColumns		=	15;
					InputColumnSpec		=	"	int, int, int, int, int, int, int, int, int, int, int, short, int, int, int	";
					Convert_NF_to_zero	=	1;
					break;


		case CheckHealthAlerts_READ_LabLastTestDate:
					SQL_CommandText		=	"	SELECT	MAX (approvaldate),		CAST (COUNT(*) AS INTEGER)		"

											"	FROM	labresother												"

											"	WHERE	idnumber		=		?								"
											"	  AND	idcode			=		?								"
											"	  AND	reqcode			IN (	?, ?, ?, ?, ?, ?	)			"
											"	  AND	approvaldate	BETWEEN	? AND ?							"
											"	  AND	(result			>		?	OR  ? <> 3)					"
											"	  AND	(result			<		?	OR  ? <> 4)					"
											"	  AND	(result			>		?	OR  result <  ?  OR ? <> 5)	"
											"	  AND	((result		>=		?	AND result <= ?) OR ? <> 6)	";
					NumOutputColumns	=	2;
					OutputColumnSpec	=	"	int, int	";
					NumInputColumns		=	20;
					InputColumnSpec		=	"	int, short, int, int, int, int, int, int, int, int,			"
											"	int, short, int, short, int, int, short, int, int, short	";
					StatementIsSticky	=	1;	// Made sticky 27Oct2020.
					break;


		case CheckHealthAlerts_READ_LabLastTestTime:
					SQL_CommandText		=	"	SELECT	MAX (inserttime)									"

											"	FROM	labresother											"

											"	WHERE	idnumber		=  ?								"
											"	  AND	idcode			=  ?								"
											"	  AND	reqcode			IN (	?, ?, ?, ?, ?, ?	)		"
											"	  AND	approvaldate	>= ?								"	// Just to rule out bogus values, like zero.
											"	  AND	approvaldate	=  ?								"
											"	  AND	(result			>  ?	OR ? <> 3)					"
											"	  AND	(result			<  ?	OR ? <> 4)					"
											"	  AND	(result			>  ?	OR result < ? OR ? <> 5)	";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					NumInputColumns		=	17;
					InputColumnSpec		=	"	int, short, int, int, int, int, int, int, int, int,			"
											"	int, short, int, short, int, int, short	";
					break;


		case CheckHealthAlerts_READ_MaxIngredientCode:
					SQL_CommandText		=	"	SELECT MAX(gen_comp_code) FROM gencomponents	";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					StatementIsSticky	=	1;	// Made sticky 27Oct2020.
					break;


		case CheckHealthAlerts_READ_RashamCount:
					SQL_CommandText		=	"	SELECT	CAST (COUNT(*) AS INTEGER)						"

											"	FROM	member_rasham									"

											"	WHERE	member_id			=  ?						"
											"	  AND	rasham_code			IN (	?, ?, ?, ?, ?	)	"
											"	  AND	mem_id_extension	=  ?						";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					NumInputColumns		=	7;
					InputColumnSpec		=	"	int, short, short, short, short, short, short	";
					Convert_NF_to_zero	=	1;
					break;


		case CheckHealthAlerts_READ_RuleCount:
					SQL_CommandText		=	"	SELECT	CAST (COUNT(*) AS INTEGER)									"
											"	FROM	health_alert_rules											"
											"	WHERE	rule_enabled		> 0										"
											"	  AND	msg_code_assigned	> 0										"
											"	  AND	rule_valid_from		<= ?									"
											"	  AND	(rule_valid_until	>= ? OR rule_valid_until < 19900101)	";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					NumInputColumns		=	2;
					InputColumnSpec		=	"	int, int	";
					Convert_NF_to_zero	=	1;
					break;


		// Marianna 16May2024 Epic #178023 - warning frequency checking
		case CheckHealthAlerts_READ_CheckWarnings:
					SQL_CommandText		=	"	SELECT	CAST (COUNT(*) AS INTEGER), MAX(pm.date)		"		
											"	FROM prescription_msgs AS pm, prescriptions AS pr		"
											"	WHERE member_id					=  ? 					"
											"		AND pr.date					>= ?					"	/*warning_period_length*/
											"		AND pm.delivered_flg		=  1					"	/*16Sep2024 check after 6005*/
											"		AND mem_id_extension		=  ?					"
											"		AND pm.prescription_id		=  pr.prescription_id	"
											"		AND pm.error_code			=  ?					";
					NumOutputColumns	=	2;
					OutputColumnSpec	=	"	int, int	";
					NumInputColumns		=	4;
					InputColumnSpec		=	"	int,int,short,short	";
					Convert_NF_to_zero	=	1;
					break;


		// Marianna 15Aug2024 Epic #178023 - checking fictive sale of largo
		case CheckHealthAlerts_READ_PrescDrugs:
					SQL_CommandText		=	"	SELECT	CAST (COUNT(*) AS INTEGER)						"
											"	FROM prescription_drugs									"
											"	WHERE member_id					=  ? 					"
											"		AND largo_code				=  ?					"
											"		AND date					>= ?					"	/*warning_period_length*/
											"		AND delivered_flg			=  1					"
											"		AND mem_id_extension		=  ?					"
											"		AND del_flg					=  0					";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					NumInputColumns		=	4;
					InputColumnSpec		=	"	int, int, int, short	";
					Convert_NF_to_zero	=	1;
					break;


		case CheckPrescriptionDigitalStatus:
					SQL_CommandText		=	"	SELECT	digital_presc_flag				"

											"	FROM	doctor_presc					"

											"	WHERE	member_id			=		?	"	// 6 conditions to identify the right doctor prescription.
											"	  AND	doctor_presc_id		=		?	"
											"	  AND	valid_from_date		=		?	"
											"	  AND	largo_prescribed	=		?	"
											"	  AND	doctor_id			=		?	"
											"	  AND	member_id_code		=		?	";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	short	";
					NumInputColumns		=	6;
					InputColumnSpec		=	"	int, int, int, int, int, short	";
					break;


		case ReadPrescriptionDoctorRuleNumber:
					// DonR 13May2025 User Story #388766: Added Sold Status & Last Sold Date to output.
					SQL_CommandText		=	"	SELECT	rule_number, sold_status, last_sold_date	"

											"	FROM	doctor_presc					"

											"	WHERE	member_id			=		?	"	// 6 conditions to identify the right doctor prescription.
											"	  AND	doctor_presc_id		=		?	"
											"	  AND	valid_from_date		=		?	"
											"	  AND	largo_prescribed	=		?	"
											"	  AND	doctor_id			=		?	"
											"	  AND	member_id_code		=		?	";
					NumOutputColumns	=	3;
					OutputColumnSpec	=	"	int, short, int	";
					NumInputColumns		=	6;
					InputColumnSpec		=	"	int, int, int, int, int, short	";
					StatementIsSticky	=	1;	// DonR 18May2025.
					break;


		case check_sale_against_pharm_ishur_cur:
					SQL_CommandText		=	"	SELECT	largo_code,			used_flg,				"
											"			speciality_code,	as400_ishur_extend		"

											"	FROM	pharmacy_ishur								"

											"	WHERE	pharmacy_code		= ?						"
											"	  AND	member_id			= ?						"
											"	  AND	mem_id_extension	= ?						"
											"	  AND	pharm_ishur_num		= ?						"
											"	  AND	ishur_date			= ?						";	// Ishur is valid only on day of issue.
					NumOutputColumns	=	4;
					OutputColumnSpec	=	"	int, short, int, short	";
					NumInputColumns		=	5;
					InputColumnSpec		=	"	int, int, short, int, int	";
					break;


		case CheckMember29G_READ_member_drug_29g:
					SQL_CommandText		=	"	SELECT		end_date,	pharmacy_code,	form_29g_type	"

											"	FROM		member_drug_29g								"

											"	WHERE		member_id			=  ?					"
											"	  AND		largo_code			=  ?					"
											"	  AND		mem_id_extension	=  ?					"
											"	  AND		start_date			<= ?					"
											"	  AND		end_date			>= ?					";
					NumOutputColumns	=	3;
					OutputColumnSpec	=	"	int, int, short	";
					NumInputColumns		=	5;
					InputColumnSpec		=	"	int, int, short, int, int	";
					StatementIsSticky	=	1;	// DonR 25Mar2020.
					break;


		case FIND_DRUG_EXTENSION_row:		// DonR 18Jun2020: This singleton SELECT replaces FIND_DRUG_EXTENSION_cur, which
											// was used only to get the first rows SELECTEd anyway.
					SQL_CommandText		=	"	SELECT	{FIRST} 1																"
											"			largo_code,				from_age,				to_age,					"
											"			sex,					permission_type,		treatment_category,		"
											"			max_op,					max_units,				max_amount_duratio,		"
											"			no_presc_sale_flag,		rule_number,			price_code,				"
											"			fixed_price,			insurance_type,			needs_29_g,				"

											"			basic_price_code,		kesef_price_code,		zahav_price_code,		"
											"			basic_fixed_price,		kesef_fixed_price,		zahav_fixed_price,		"
											"			health_basket_new,		sort_sequence,			confirm_authority		"

											"	FROM	drug_extension															"
		
											"	WHERE	largo_code			=  ?												"
											"	  AND	rule_number			>  0												"	// DonR 24Apr2025 User Story #390071: Changed from -999.
											"	  AND	from_age			<= ?												"	// DonR 29Oct2003 changed to <= from <.
											"	  AND	to_age				>= ?												"	// DonR 29Oct2003 added to_age to select.
											"	  AND	(sex				=  ?				OR sex = 0)						"	// 4 input parameters so far...

												  // DonR 20Nov2003 - Permission Type 6 ("Prati Plus") applies to Maccabi pharmacies as well.
											"	  AND	(permission_type	=  ?				OR								"
											"			 (permission_type	=  6 AND ? = 1)		OR								"
											"			 permission_type	=  2)												"	// 6 input parameters so far...

											"	  AND	del_flg				=  0												"


											"	  AND	(no_presc_sale_flag =  ?	OR no_presc_sale_flag = 2)					"
											"	  AND	treatment_category	=  ?												"	// 8 input parameters so far...

											// DonR 10Nov2003 added restriction to select "normal" rules only.
											// DonR 24Apr2025 User Story #390071: Add ability to select a *non*-automatic
											// rule that was chosen by the doctor and written to doctor_presc.
											// DonR 13May2025 User Story #388766: Add ability to select a *non*-automatic
											// rule with a specific "gorem me'asher" (from sysparams/DrugShortageRuleConfAuthority)
											// to give users a discount after they possibly missed out on a Nihul Tikrot
											// reduction. (The first "?" in this case is whether the drug had a single
											// previous partial sale.)
											// DonR 18May2025: Reverting the last change, as we will be using special drug-shortage
											// nohalim only when there is an AS/400 ishur (with tikra) and not in other cases. But
											// I'm leaving the code here just in case someone changes their mind later on.
											"	  AND	(	confirm_authority	=  0											"	// "Automatic" nohal.
											"				OR (rule_number = ?	AND confirm_authority BETWEEN 1 and ?)			"	// Doctor-chosen nohal.
//											"				OR (? <> 0			AND confirm_authority = ?)						"	// Drug-shortage nohal. This would add new input parameters 11 & 12.
											"			)																		"	// 10 input parameters so far...

											"	  AND	send_and_use		<> 0												"	// DonR 19Dec2005

												  // DonR 212Nov2011 - Add selection criteria for member type.
											"	  AND	(((enabled_mac	> 0) AND (?	> 0))	OR									"
											"			 ((enabled_hova	> 0) AND (?	> 0))	OR									"
											"			 ((enabled_keva	> 0) AND (?	> 0)))										"	// 13 input parameters so far...

												  // DonR 09Dec2012 "Yahalom" - New insurance-type and "vetek" criteria.
												  // Note that the price code/fixed price combination is now validated
												  // without regard to which insurance is applicable.
											"	  AND	(price_code > 1 OR (price_code = 1 AND fixed_price > 0))				"

											"	  AND	((insurance_type = 'B')						OR							"
											"			 (insurance_type = ? AND ? >= wait_months)	OR							"
											"			 (insurance_type = ? AND ? >= wait_months))								"	// 17 input parameters.
												  // DonR 06Dec2012 "Yahalom" end.

											"	  ORDER BY	ins_type_sort DESC,		"	// DonR 21Oct2015 - Best ptn within member's best insurance.
											"				ptn_percent_sort,		"	// DonR 23Nov2011 - Added to get "best" rule first.
											"				fixed_price,			"	// DonR 11Dec2012 - Just in case there are different fixed prices available.
											"				sort_sequence			";	// DonR 20Nov2003 - sort_sequence ensures that we get the most specific
																						// rule for this pharmacy type first.
					NumOutputColumns	=	24;
					OutputColumnSpec	=	"			int,				short,				short,	"
											"			short,				short,				short,	"
											"			int,				int,				short,	"
											"			short,				int,				short,	"
											"			int,				char(1),			short,	"

											"			short,				short,				short,	"
											"			int,				int,				int,	"
											"			short,				short,				short	";
					NumInputColumns		=	17;
					InputColumnSpec		=	"	int,		short,	short,	short,		short,	"
											"	short,		short,	short,	int,		short,	"
											"	short,		short,	short,	char(1),	short,	"
											"	char(1),	short								";
					StatementIsSticky	=	1;
					break;


		case FindSpecialDrugShortageRule:	// DonR 18May2025 User Story #388766.
					SQL_CommandText		=	"	SELECT	{FIRST} 1																"
											"			rule_number,			price_code,				fixed_price,			"
											"			insurance_type,			health_basket_new								"

											"	FROM	drug_extension															"
		
											"	WHERE	largo_code			=  ?												"
											"	  AND	rule_number			>  0												"	// DonR 24Apr2025 User Story #390071: Changed from -999.
											"	  AND	from_age			<= ?												"	// DonR 29Oct2003 changed to <= from <.
											"	  AND	to_age				>= ?												"	// DonR 29Oct2003 added to_age to select.
											"	  AND	(sex				=  ?				OR sex = 0)						"	// 4 input parameters so far...

												  // DonR 20Nov2003 - Permission Type 6 ("Prati Plus") applies to Maccabi pharmacies as well.
											"	  AND	(permission_type	=  ?				OR								"
											"			 (permission_type	=  6 AND ? = 1)		OR								"
											"			 permission_type	=  2)												"	// 6 input parameters so far...

											"	  AND	del_flg				=  0												"

											// Since this query is run only when an AS/400 ishur is applicable, we don't really
											// have to worry about non-prescription sales.
//											"	  AND	(no_presc_sale_flag =  ?	OR no_presc_sale_flag = 2)					"

											"	  AND	treatment_category	=  ?												"	// 7 input parameters so far...

											// DonR 10Nov2003 added restriction to select "normal" rules only.
											// DonR 24Apr2025 User Story #390071: Add ability to select a *non*-automatic
											// rule that was chosen by the doctor and written to doctor_presc.
											// DonR 13May2025 User Story #388766: Add ability to select a *non*-automatic
											// rule with a specific "gorem me'asher" (from sysparams/DrugShortageRuleConfAuthority)
											// to give users a discount after they possibly missed out on a Nihul Tikrot
											// reduction. (The first "?" in this case is whether the drug had a single
											// previous partial sale.)
											// DonR 18May2025: Reverting the last change, as we will be using special drug-shortage
											// nohalim only when there is an AS/400 ishur (with tikra) and not in other cases. But
											// I'm leaving the code here just in case someone changes their mind later on.
											"	  AND	confirm_authority	=  ?												"	// Drug-shortage nohal *only* - parameter #8.

											"	  AND	send_and_use		<> 0												"	// DonR 19Dec2005

												  // DonR 212Nov2011 - Add selection criteria for member type.
											"	  AND	(((enabled_mac	> 0) AND (?	> 0))	OR									"
											"			 ((enabled_hova	> 0) AND (?	> 0))	OR									"
											"			 ((enabled_keva	> 0) AND (?	> 0)))										"	// 11 input parameters so far...

												  // DonR 09Dec2012 "Yahalom" - New insurance-type and "vetek" criteria.
												  // Note that the price code/fixed price combination is now validated
												  // without regard to which insurance is applicable.
											"	  AND	(price_code > 1 OR (price_code = 1 AND fixed_price > 0))				"

											"	  AND	((insurance_type = 'B')						OR							"
											"			 (insurance_type = ? AND ? >= wait_months)	OR							"
											"			 (insurance_type = ? AND ? >= wait_months))								"	// 15 input parameters.
												  // DonR 06Dec2012 "Yahalom" end.

											"	  ORDER BY	ins_type_sort DESC,		"	// DonR 21Oct2015 - Best ptn within member's best insurance.
											"				ptn_percent_sort,		"	// DonR 23Nov2011 - Added to get "best" rule first.
											"				fixed_price,			"	// DonR 11Dec2012 - Just in case there are different fixed prices available.
											"				sort_sequence			";	// DonR 20Nov2003 - sort_sequence ensures that we get the most specific
																						// rule for this pharmacy type first.
					NumOutputColumns	=	5;
					OutputColumnSpec	=	"	int, short, int, char(1), short	";
					NumInputColumns		=	15;
					InputColumnSpec		=	"	int,	short,	short,		short,		short,	"
											"	short,	short,	short,		short,		short,	"
											"	short,	char(1),	short,	char(1),	short	";
					StatementIsSticky	=	0;	// NOT sticky - this query shouldn't really run all that often.
					break;


		case find_preferred_drug_drugforms_cur:
					SQL_CommandText		=	"	SELECT		valid_from,		valid_until		"

											"	FROM		drug_forms						"

											"	WHERE		largo_code	 = ?				"
											"	  AND		form_number	IN (90,92)			"
											"	  AND		valid_from	<= ?				"	// Just in case someone enters a "future tofes".
											"	  AND		valid_until  > ?				"	// Tofes is still usable.

											"	ORDER BY	valid_until	DESC,				"
											"				valid_from	DESC				";
					NumOutputColumns	=	2;
					OutputColumnSpec	=	"	int, int	";
					NumInputColumns		=	3;
					InputColumnSpec		=	"	int, int, int	";
					StatementIsSticky	=	1;	// DonR 30Jun2020.
					break;


		case find_preferred_drug_READ_drugnotprior:
					SQL_CommandText		=	"	SELECT	CAST (COUNT(*) AS INTEGER)	"
											"	FROM	drugnotprior				"
											"	WHERE	idnumber		 = ?		"
											"	  AND	treatmentcontr	 = ?		"
											"	  AND	largo_code		 = ?		"
											"	  AND	idcode			 = ?		"
											"	  AND	authdate		>= ?		"
											"	  AND	authdate		<= ?		";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					NumInputColumns		=	6;
					InputColumnSpec		=	"	int, int, int, short, int, int	";
					Convert_NF_to_zero	=	1;
					StatementIsSticky	=	1;	// DonR 30Jun2020.
					break;


		case find_preferred_drug_READ_LastPrefDrugBought:
					SQL_CommandText		=	"	SELECT		{FIRST} 1 largo_code										"

											"	FROM		prescription_drugs											"

											"	WHERE		member_id			=  ?									"
											"	  AND		largo_code			IN (SELECT	largo_code					"
											"										FROM	drug_list					"
											"										WHERE	economypri_group	=  ?	"
											"										  AND	preferred_flg		=  1	"
											"										  AND	del_flg				=  0)	"
											"	  AND		date				>= ?									"
											"	  AND		delivered_flg		=  1									"
											"	  AND		del_flg				=  0									"
											"	  AND		mem_id_extension	=  ?									"

											"	ORDER BY	date DESC													";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					NumInputColumns		=	4;
					InputColumnSpec		=	"	int, int, int, short	";
					StatementIsSticky	=	1;	// DonR 30Jun2020.
					break;


//		case FindDrugPurchLimit_drug_purchase_lim_cur:
//					SQL_CommandText		=	"	SELECT		dpl.max_units,									"
//											"				dpl.duration_months,							"
//											"				dpl.history_start_date,							"
//											"				dpl.class_code,									"
//											"				plc.class_type,									"
//											"				plc.class_history_days							"
//
//											"	FROM		drug_purchase_lim	as dpl,						"
//											"				purchase_lim_class	as plc						"
//
//											"	WHERE		dpl.largo_code		=  ?						"
//											"	  AND		plc.class_code		=  dpl.class_code			"
//											"	  AND		(plc.class_sex		=  ? OR plc.class_sex = 0)	"
//											"	  AND		plc.class_min_age	<= ?						"
//											"	  AND		plc.class_max_age	>= ?						"
//
//											"	ORDER BY	plc.class_priority								";
//					NumOutputColumns	=	6;
//					OutputColumnSpec	=	"	int, short, int, short, short, int	";
//					NumInputColumns		=	4;
//					InputColumnSpec		=	"	int, short, short, short	";
//					StatementIsSticky	=	1;	// Made sticky 27Oct2020.
//					break;
//
//
		case GetPurchaseLimitClassHistoryLength:	// Added 28Feb2024 User Story #551403.
					SQL_CommandText		=	"	SELECT		class_history_days		"
											"	FROM		purchase_lim_class		"
											"	WHERE		class_code = ?			";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					NumInputColumns		=	1;
					InputColumnSpec		=	"	short	";
					StatementIsSticky	=	0;	// Very simple SQL, and it probably won't be called very often.
					break;


		case GetDrugsInBlood_blood_drugs_cur:
					SQL_CommandText		=	"	SELECT	largo_code,					stop_use_date,					duration,			"
											"			units,						op,								date,				"
											"			time,						pharmacy_code,					presc_source,		"
											"			doctor_id,					doctor_id_type,					prescription_id,	"

											"			line_no,					t_half,							stop_blood_date,	"
											"			price_replace,				macabi_price_flg,				member_price_code,	"
											"			particip_method,			ingr_1_code,					ingr_2_code,		"
											"			ingr_3_code,				ingr_1_quant_bot,				ingr_2_quant_bot,	"

											"			ingr_3_quant_bot,			ingr_1_quant_std,				ingr_2_quant_std,	"
											"			ingr_3_quant_std,			special_presc_num,				mem_id_extension,	"
											"			del_flg																			"

											"	FROM	prescription_drugs																"

												// DonR 21May2014 - Remove mem_id_extension from SELECT and relevant index.
												// NOTE - Got rid of the old_member_id logic, since it hasn't been in use basically forever.
											"	WHERE	member_id		=  ?															"
											"	  AND	delivered_flg	=  ?															"
											"	  AND	stop_blood_date	>= ?															"
			  
											"	ORDER BY	largo_code, date DESC, time DESC											";
					NumOutputColumns	=	31;
					OutputColumnSpec	=	"			int,						int,							int,				"	// DonR 05Apr2020: Changed duration back to an int - but when we switch to MS-SQL, it should really be defined as a SMALLINT.
											"			int,						int,							int,				"
											"			int,						int,							short,				"
											"			int,						short,							int,				"

											"			short,						int,							int,				"
											"			int,						short,							short,				"
											"			short,						short,							short,				"
											"			short,						double,							double,				"

											"			double,						double,							double,				"
											"			double,						int,							short,				"
											"			short																			";
					NumInputColumns		=	3;
					InputColumnSpec		=	"	int, short, int	";
					StatementIsSticky	=	1;	// Disabled 01Apr2020; re-enabled 06Apr2020.
					break;


		case GetDrugsInBlood_READ_drug_list:
											// DonR 04Dec2023 User Story #529100: Add Shape Code.
					SQL_CommandText		=	"	SELECT	package_size,	purchase_limit_flg,		qty_lim_grp_code,							"
											"			class_code,		pharm_group_code,		economypri_group,							"
											"			largo_type,		treatment_typ_cod,		in_drug_interact,							"

											"			CAST(CASE WHEN (chronic_flag = 1) THEN 1 ELSE 0 END AS SMALLINT) AS chronic_1_or_0,	"	// DonR 05Apr2020: Force numeric expression to output a SMALLINT.

											"			shape_code_new,	parent_group_code													"

											"	FROM	drug_list																			"

											"	WHERE	largo_code = ?																		";
					NumOutputColumns	=	12;
					OutputColumnSpec	=	"			int,			short,					int,							"
											"			short,			short,					int,							"
											"			char(1),		short,					short,							"
											"			short,			short,					int								";
					NumInputColumns		=	1;
					InputColumnSpec		=	"	int	";
					StatementIsSticky	=	1;
					break;


		case GetSpoolLock_INS_pharm_spool_lock:
											// DonR 27Aug2025: INSERT only the Pharmacy Code, leaving all other columns defaulted.
											// This statement is now executed immediately before GetSpoolLock_UPD_pharm_spool_lock
											// (and is expected to fail with "duplicate row" unless a new pharmacy has appeared),
											// so the update query will take care of setting all the other values.
					SQL_CommandText		=	"	INSERT INTO	pharm_spool_lock (pharmacy_code)	";
					NumInputColumns		=	1;
					InputColumnSpec		=	"	int	";
					GenerateVALUES		=	1;
					break;


		case GetSpoolLock_UPD_pharm_spool_lock:
											// DonR 27Aug2025: Added new columns locked and lock_timestamp to both the SET section
											// and the WHERE criteria. This new version uses "business logic" to determine if
											// another process has a lock on this pharmacy's activity rather than relying on the
											// database server to throw a "contention" error - the earlier approach wasnn't working.
					SQL_CommandText		=	"	UPDATE	pharm_spool_lock						"
											"	SET		pid					= ?,				"
											"			date				= ?,				"
											"			time				= ?,				"
											"			locked				= 1,				"
											"			lock_timestamp		= CURRENT_TIMESTAMP	"
											"	WHERE	pharmacy_code		= ?					"
											"	  AND	(		(locked		= 0)				"
											"				OR	(DATEDIFF (SECOND, lock_timestamp, CURRENT_TIMESTAMP) >= ?)	)	";
					NumInputColumns		=	5;
					InputColumnSpec		=	"	int, int, int, int, int	";
					break;


		case CLEAR_pharm_spool_lock:
					SQL_CommandText		=	"	UPDATE	pharm_spool_lock		"
											"	SET		locked			= 0		"
											"	WHERE	pharmacy_code	= ?		";
					NumInputColumns		=	1;
					InputColumnSpec		=	"	int	";
					break;


		case init_package_size_array_cur:
					SQL_CommandText		=	"	SELECT		largo_code,		package_size	"

											"	FROM		drug_list						"

											"	WHERE		largo_code		<  100000		"
											"	  AND		package_size	<> 1			"

											"	ORDER BY	largo_code						";
					NumOutputColumns	=	2;
					OutputColumnSpec	=	"	int, int	";
					break;


		case INS_gadget_spool:
					SQL_CommandText		=	"	INSERT INTO gadget_spool							"
											"		(	pharmacy_code,			prescription_id,	"
											"			member_id,				mem_id_extension,	"
											"			largo_code,				service_code,		"
											"			service_number,			service_type,		"
											"			op,						op_price,			"

											"			total_ptn,				event_date,			"
											"			doctor_id,				request_date,		"
											"			request_num,			action_code,		"
											"			as400_error_code,		update_date,		"
											"			update_time,			reported_to_as400	"
											"		)												";
					NumInputColumns		=	20;
					InputColumnSpec		=	"	int, int, int, short, int, int, int, int, int, int,		"
											"	int, int, int, int, int, short, short, int, int, short	";
					GenerateVALUES		=	1;
					break;


		case INS_messages_details:
					SQL_CommandText		=	"	INSERT INTO messages_details	"
											"	(								"
											"		msg_idx,		time,			date,		message_raw,	rec_id,			spool_flg,	"
											"		ok_flg,			error_code,		proc_time,	pharmacy_code,	terminal_id,	member_id,	"
											"		member_id_ext,	computer_id																"
											"	)																							"
											"	VALUES																						"
											"	(																							"
											"		?,				?,				?,			?,				?,				?,			"
											"		-999,			-999,			-999,		-999,			-999,			-999,		"
											"		-999,			?																		"
											"	)																							";
					NumInputColumns		=	7;
					InputColumnSpec		=	"		short,			int,			int,		char[2500],		int,			short,	char(2)	";
					StatementIsSticky	=	1;	// DonR 26Mar2020.
					break;


		case INS_online_orders:
					SQL_CommandText		=	"	INSERT INTO	online_orders					"
											"	(											"
											"		online_order_num,		member_id,		"
											"		mem_id_extension,		made_by_pr_id,	"
											"		made_by_pharmacy,		made_by_owner,	"
											"		made_date,				made_time		"
											"	)											";
					NumInputColumns		=	8;
					InputColumnSpec		=	"	long, int, short, int, int, short, int, int	";
					GenerateVALUES		=	1;
					break;


		case INS_pharmacy_daily_sum_19_columns:
					SQL_CommandText		=	"	INSERT INTO	pharmacy_daily_sum										"
											"	(																	"
											"		pharmacy_code,		institued_code,		diary_month,			"
											"		date,				terminal_id,		sum,					"
											"		presc_count,		presc_lines_count,	member_part_sum,		"
											"		purchase_sum,		delete_sum,			del_member_prt_sum,		"

											"		delete_lines,		deleted_sales,		del_purchase_sum,		"
											"		net_fail_sum,		net_fail_lines,		net_fail_presc,			"
											"		net_fail_mem_part												"
											"	)																	";
					NumInputColumns		=	19;
					InputColumnSpec		=	"	int, short, short, int, short, int, int, int, int, int, int, int,	"
											"	int, int, int, int, int, int, int	";
					GenerateVALUES		=	1;
					break;


		case INS_prescription_msgs:
					SQL_CommandText		=	"	INSERT INTO prescription_msgs									"
											"	(																"
											"		prescription_id,	largo_code,			line_no,			"
											"		error_code,			severity,			date,				"
											"		time,				delivered_flg,		reported_to_as400	"
											"	)																";
					NumInputColumns		=	9;
					InputColumnSpec		=	"	int, int, short, short, short, int, int, short, short	";
					GenerateVALUES		=	1;
					StatementIsSticky	=	1;	// DonR 25Mar2020.
					break;


		case INS_prescription_usage:
					SQL_CommandText		=	"	INSERT INTO prescription_usage							"
											"	(														"
											"		prescription_id,				line_no,			"
											"		largo_code,						course_len,			"
											"		course_treat_days,				duration,			"
											"		calc_courses,					calc_treat_days,	"
											"		total_units,					fully_used_units,	"

											"		partly_used_units,				discarded_units,	"
											"		avg_partial_unit,				proportion_used,	"
											"		net_lim_ingr_used,				date,				"
											"		time,							delivered_flg,		"
											"		reported_to_as400									"
											"	)														";
					NumInputColumns		=	19;
					InputColumnSpec		=	"	int, short, int, short, short, short, short, short, int, int,	"
											"	int, int, double, double, double, int, int, short, short		";
					GenerateVALUES		=	1;
					break;


		case INS_prescriptions:
											// DonR 20Apr2023 User Story #432608: Add Magento Order Number.
					SQL_CommandText		=	"	INSERT INTO prescriptions							"
											"	(													"
											"		pharmacy_code,			institued_code,			"
											"		price_list_code,		member_id,				"
											"		mem_id_extension,		member_institued,		"
											"		doctor_id,				doctor_id_type,			"
											"		presc_source,			prescription_id,		"

											"		special_presc_num,		spec_pres_num_sors,		"
											"		date,					time,					"
											"		error_code,				prescription_lines,		"
											"		credit_type_code,		card_date,				"
											"		member_discount_pt,		terminal_id,			"

											"		receipt_num,			user_ident,				"
											"		presc_pharmacy_id,		macabi_code,			"
											"		doctor_presc_id,		reported_to_as400,		"
											"		comm_error_code,		member_price_code,		"
											"		doctor_insert_mode,		macabi_centers_num,		"

											"		doc_device_code,		diary_month,			"
											"		memb_prc_code_ph,		next_presc_start,		"
											"		next_presc_stop,		waiting_msg_flg,		"
											"		delivered_flg,			del_flg,				"
											"		reason_for_discnt,		reason_to_disp,			"

											"		action_type,			del_presc_id,			"
											"		del_pharm_code,			del_sale_date,			"
											"		del_pharm_presc_id,		card_owner_id,			"
											"		card_owner_id_code,		num_payments,			"
											"		payment_method,			credit_reason_code,		"

											"		tikra_called_flag,		tikra_error_code,		"
											"		tikra_discount,			subsidy_amount,			"
											"		origin_code,			elect_pr_status,		"
											"		tikra_status_code,		insurance_type,			"
											"		online_order_num,		credit_type_used,		"

											"		sale_req_error,			darkonai_plus_sale,		"
											"		paid_for,				MagentoOrderNum			"
											"	)													";
					NumInputColumns		=	64;
					InputColumnSpec		=	"	int, short, short, int, short, short, int, short, short, int,		"
											"	int, short, int, int, short, short, short, short, short, short,		"
											"	int, int, int, short, int, short, short, short, short, int,			"
											"	int, short, short, int, int, short, short, short, int, short,		"
											"	short, int, int, int, int, int, short, short, short, short,			"
											"	short, short, int, int, short, short, short, char(1), long, short,	"
											"	short, short, short, long											";
					GenerateVALUES		=	1;
					StatementIsSticky	=	1;
					break;


		// DonR 04Jul2021: For "Chanut Virtualit" Trn. 6103, add a minimal INSERT command to insert a (mostly)
		// "placeholder" row in the presriptions table. Columns that have default values are mostly left to
		// use those defaults; columns that lack a defined default value but don't get a meaningful value are
		// hard-coded in the SQL rather than sent as an ExecSQL parameter. Note that we're hard-coding some
		// columns like delivered_flg that could, theoretically, get a meaningful value from the calling
		// routine; if it ever becomes necessary, these columns could be added to the first section with other
		// columns that get a calling-routine parameter.
		case INS_prescriptions_placeholder_row:
					SQL_CommandText		=	"	INSERT INTO prescriptions												"
											"	(																		"
													// Columns supplied by calling routine.
											"		prescription_id,		action_type,			member_id,			"
											"		mem_id_extension,		member_institued,		macabi_code,		"
											"		pharmacy_code,			institued_code,			terminal_id,		"
											"		date,					time,					prescription_lines,	"
											"		tikra_called_flag,		tikra_error_code,		tikra_status_code,	"
											"		tikra_discount,			subsidy_amount,			origin_code,		"
											"		error_code,															"

													// Hard-coded columns, present here only because they aren't defaulted
													// in the table definition.
											"		del_flg,				doctor_id,				doctor_id_type,		"
											"		member_price_code,		credit_type_code,		card_date,			"
											"		doctor_insert_mode,		receipt_num,			user_ident,			"
											"		presc_pharmacy_id,		macabi_centers_num,		diary_month,		"
											"		reported_to_as400,		delivered_flg,			comm_error_code,	"
											"		memb_prc_code_ph,		next_presc_start,		next_presc_stop,	"
											"		waiting_msg_flg														"
											"	)																		"
											"	VALUES																	"
											"	(																		"
													// Columns supplied by calling routine.
											"		?,						?,						?,					"
											"		?,						?,						?,					"
											"		?,						?,						?,					"
											"		?,						?,						?,					"
											"		?,						?,						?,					"
											"		?,						?,						?,					"
											"		?,																	"

													// Hard-coded columns, present here only because they aren't defaulted
													// in the table definition.
											"		0,						0,						0,					"
											"		0,						0,						0,					"
											"		0,						0,						0,					"
											"		0,						0,						0,					"
											"		0,						0,						0,					"
											"		0,						0,						0,					"
											"		0																	"
											"	)																		";
					NumInputColumns		=	19;
					InputColumnSpec		=	"	int, short, int, short, short, short, int, short, short, int,	"
											"	int, short, short, short, short, int, int, short, short			";
					break;


		case INS_sql_srv_audit:
					SQL_CommandText		=	"	INSERT INTO sql_srv_audit					"
											"				(	pid,			computer_id,	date,		time,		msg_id,			"
											"					in_progress,	msg_rec_id,		msg_len,	err_code,	msg_count	)	"
											"		VALUES	(	?,				?,				?,			?,			?,				"
											"					1,				?,				?,			0,			?			)	";
					NumInputColumns		=	8;
					InputColumnSpec		=	"	int, char(2), int, int, short, int, int, int	";
					break;


		case IsDrugInteract_READ_drug_interaction:	// Since this is used multiple times per drug sale, make it "sticky".
					SQL_CommandText		=	"	SELECT interaction_type FROM drug_interaction WHERE first_largo_code = ? AND second_largo_code = ?	";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	short	";
					NumInputColumns		=	2;
					InputColumnSpec		=	"	int, int	";
					StatementIsSticky	=	1;
					break;


		case IsPharmOpen_READ_pharmacy:	// Since this is used all over the place, make it "sticky".
										// DonR 9May2021: The unique index for pharmacy is pharmacy_code/del_flg - so there's
										// no point in SELECTing by institued_code. Instead, add institued_code to the list
										// of columns SELECTed, so a calling routine has the option of checking whether a
										// pharmacy sent the correct Institute Code.
										// DonR 29Jan2024 User Story #540234: Add CannabisPharmacy/can_sell_cannabis flag.
					SQL_CommandText		=	"	SELECT	permission_type,		price_list_code,		vat_exempt,				"
											"			open_close_flag,		pharm_category,			card,					"
											"			leumi_permission,		credit,					maccabicare_flag,		"
											"			meishar_enabled,		order_originator,		order_fulfiller,		"
											"			can_sell_future_rx,		hesder_category,		web_pharmacy_code,		"
											"			owner,					institued_code,			ConsignmentPharmacy,	"
											"			PharmNohalEnabled,		CannabisPharmacy,		mahoz,					"
											"			software_house															"

											"	FROM	pharmacy															"

											"	WHERE	pharmacy_code		=  ?											"
											//	  AND	institued_code		=  ?												DISABLED 09May2021
											//	  AND	((hesder_category	<> NonHesderPharmacy) OR (HesderMaccabiOnly = 0))	 DonR 10Jan2019
											"	  AND	del_flg				=  ?											";
					NumOutputColumns	=	22;
					OutputColumnSpec	=	"	short, short, short, short, short, short, short, short, short, short,	"
											"	short, short, short, short, int, short, short, short, short, short,		"
											"	short, short															";
					NumInputColumns		=	2;
					InputColumnSpec		=	"	int, short	";
					StatementIsSticky	=	1;
					break;


		case IsPharmOpen_READ_pharmacy_type_params:	// Since this is used all over the place, make it "sticky".
					SQL_CommandText		=	"	SELECT	max_order_sit_days,							max_order_work_hrs,						"
											"			maxdailybuy,								kill_nocard_svc_minutes,				"
											"			meishar_enabled,							allow_order_narcotic,					"
											"			allow_order_preparation,					allow_full_price_order,					"
											"			allow_refrigerated_delivery,				max_price_for_pickup,					"

											"			select_late_rx_days_buying_in_person,		select_late_rx_days_ordering_online,	"
											"			select_late_rx_days_filling_online_order,	allow_online_order_days_before_expiry,	"
											"			usage_memory_mons,							prior_sale_download_type,				"
											"			enable_ishur_download,						prior_sale_opioid_days,					"
											"			prior_sale_ADHD_days,						prior_sale_ordinary_days				"
//
// NOTE: I decided not to set up the maximum historical span as a separate, derived value;
// but I'm leaving this code in place because it uses a rather interesting SQL technique
// that I had to research since the "GREATEST" function is not available in our database
// implementation.
// 
//											// DonR 06Jan2026 Add new subquery to get the maximum of the three history-range columns.
//											// This works by using VALUES to build a three-row "virtual table" MyHistoryDays with one
//											// column "HistoryDays", then selecting the MAXimum from the three rows in the virtual table.
//											"			(	SELECT	MAX (HistoryDays)														"
//											"				FROM	(	VALUES	(prior_sale_opioid_days),									"
//											"									(prior_sale_ADHD_days),										"
//											"									(prior_sale_ordinary_days)									"
//											"						)																		"
//											"						AS	MyHistoryDays (HistoryDays)											"
//											"			)																					"

											"	FROM	pharmacy_type_params													"

											"	WHERE	permission_type		= ?													"
											"	  AND	owner				= ?													"
											"	  AND	web_pharmacy_code	= ?													";
					NumOutputColumns	=	20;
					OutputColumnSpec	=	"	short, short, int,   int,   short, short, short, short, short, int,		"
											"	short, short, short, short, short, short, short, short, short, short	";
					NumInputColumns		=	3;
					InputColumnSpec		=	"	short, short, int	";
					StatementIsSticky	=	1;
					break;



		case IsPharmOpen_READ_hebrew_encoding:
					SQL_CommandText		=	"	SELECT	ASCII(hebrew_encoding)			"
											"	FROM	transaction_hebrew_encoding		"
											"	WHERE	transaction_number	= ?			"
											"	  AND	software_house		= ?			";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	char(0)	";
					NumInputColumns		=	2;
					InputColumnSpec		=	"	short, short	";
					StatementIsSticky	=	1;
					break;


											// Transaction 6102 does not get a Pharmacy Code from the calling client (Chanut Virtualit) - so
											// we need to hard-code the criteria to read the date-adjustment values we'll use. The only one
											// that I think we really need is select_late_rx_days_ordering_online, but I'll include the others
											// just in case someone changes the requirement.
											// DonR 22Aug2022 HOT FIX: We changed the Web Pharmacy Code for Maccabi Chanut Virtualit, but I
											// forgot to change it in this query - and as a result, we're getting not-found errors every time
											// we run the query. To avoid problems like this in future, I'm changing the relevant criterion
											// to "> 1000" rather than hard-code a specific pharmacy code. (Note that "> 0" doesn't work,
											// since there is a row for "consignatsia" with Web Pharmacy Code = 100.)
		case Trn6102_READ_pharmacy_type_params:
					SQL_CommandText		=	"	SELECT	select_late_rx_days_buying_in_person,		select_late_rx_days_ordering_online,	"
											"			select_late_rx_days_filling_online_order,	allow_online_order_days_before_expiry	"

											"	FROM	pharmacy_type_params													"

											"	WHERE	permission_type		= 1													"
											"	  AND	owner				= 0													"
//											"	  AND	web_pharmacy_code	= 9999999											";
//											"	  AND	web_pharmacy_code	= 807030											";
											"	  AND	web_pharmacy_code	> 1000													";
					NumOutputColumns	=	4;
					OutputColumnSpec	=	"	short, short, short, short	";
					StatementIsSticky	=	1;
					break;



		case LoadPrescSourceCur:
					SQL_CommandText		=	"	SELECT  pr_src_code,		pr_src_docid_type,	id_type_accepted,	"
											"			pr_src_doc_device,	pr_src_priv_pharm,	allowed_by_default	"
											"	FROM    prescr_source					"
											"	WHERE	pr_src_code BETWEEN 0 AND 99	";
					NumOutputColumns	=	6;
					OutputColumnSpec	=	"	short, short, short, short, short, short	";
					break;


		case LoadTrnPermitCur:
					SQL_CommandText		=	"	SELECT		transaction_num,	spool,			ok_maccabi,				"
											"				ok_pvt_hesder,		ok_non_hesder,	min_version_permitted	"
											"	FROM		pharm_trn_permit		"
											"	WHERE		transaction_num > 0		"	// Which should always be true.
											"	ORDER BY	frequency DESC			";
					NumOutputColumns	=	6;
					OutputColumnSpec	=	"	short, short, short, short, short, short	";
					break;


		case MarkDrugSaleDeleted_INS_drugsale_del_queue:
					SQL_CommandText		=	"	INSERT INTO	drugsale_del_queue	(	prescription_id,	largo_code	)	";
					NumInputColumns		=	2;
					InputColumnSpec		=	"	int, int	";
					GenerateVALUES		=	1;
					break;


		case MarkDrugSaleDeleted_UPD_prescription_drugs:
					SQL_CommandText		=	"	UPDATE	prescription_drugs SET del_flg = 1 WHERE prescription_id = ? AND largo_code = ?	";
					NumInputColumns		=	2;
					InputColumnSpec		=	"	int, int	";
					break;


		case MarkDrugSaleDeleted_UPD_pd_rx_link:
					SQL_CommandText		=	"	UPDATE	pd_rx_link SET del_flg = 1 WHERE prescription_id = ? AND largo_sold = ?	";
					NumInputColumns		=	2;
					InputColumnSpec		=	"	int, int	";
					break;


		case MarkDrugSaleDeleted_UPD_pd_cannabis_products:
					SQL_CommandText		=	"	UPDATE	pd_cannabis_products		"
											"	SET		del_flg = 1					"
											"	WHERE	prescription_id		= ?		"
											"	  AND	group_largo_code	= ?		";
					NumInputColumns		=	2;
					InputColumnSpec		=	"	int, int	";
					break;


		case CountNonDeletedDrugLines:
					SQL_CommandText		=	"	SELECT		CAST (COUNT (*) AS INTEGER)			"
											"	FROM		prescription_drugs					"
											"	WHERE		prescription_id	= ?					"
											"	  AND		delivered_flg	= 1					"	// Only drug sales are relevant here.
											"	  AND		del_flg			= 0					";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					NumInputColumns		=	1;
					InputColumnSpec		=	"	int	";
					Convert_NF_to_zero	=	1;
					break;


		case MarkDrugSaleDeleted_UPD_prescriptions:
											// DonR 16Jun2022: In an attempt to fix a very strange problem - basically that a
											// previous, successful UPDATE of prescription_drugs for sale deletions seems to be
											// backed out when this UPDATE fails with a DB contention error (1205) - I'm changing
											// the subquery from a NOT EXISTS (SELECT *...) to a (SELECT COUNT(*)) = 0. The result
											// should be identical, but it's possible that there's some strange MS-SQL bug triggered
											// by the original subquery that will not be triggered by the new subquery.
											// DonR 19Jun2022: The previous fix didn't help - so I'm splitting this into two separate
											// bits of SQL: one to count non-deleted drug lines, and a simple, unconditional UPDATE
											// operation.
											// DonR 30Jun2022: None of these changes helped, so I'm reverting to the original version.
											// DonR 28Jul2022 HOT FIX: I accidentally left a semicolon at the end of the line containing
											// "WHERE	prescription_id	= ?", so the whole "AND NOT EXISTS" test wasn't being performed
											// and all deletions were being treated as full-sale deletions. Ooops!
					SQL_CommandText		=	"	UPDATE	prescriptions											"
											"	SET		del_flg			= 1										"
											"	WHERE	prescription_id	= ?										"
											"	  AND	NOT EXISTS		(	SELECT *							"
											"								FROM	prescription_drugs AS pd	"
											"								WHERE	pd.prescription_id	= ?		"
											"								  AND	pd.delivered_flg	= 1		"	// Only drug sales are relevant here.
											"								  AND	pd.del_flg			= 0	)	";
//											"	  AND	(	SELECT COUNT(*)										"
//											"				FROM	prescription_drugs AS pd					"
//											"				WHERE	pd.prescription_id	= ?						"
//											"				  AND	pd.delivered_flg	= 1						"	// Only drug sales are relevant here.
//											"				  AND	pd.del_flg			= 0		)	= 0			";
					NumInputColumns		=	2;
					InputColumnSpec		=	"	int, int	";
					break;


		case MemberFailedToPickUpDrugs_READ_NoShowCount:
					SQL_CommandText		=	"	SELECT		CAST (COUNT (DISTINCT pr.date) AS INTEGER)					"

											"	FROM		prescriptions	AS pr,										"
											"				macabi_centers	AS mc										"

											"	WHERE		pr.member_id			=			?						"	// Note that first three criteria match a table index.
											"	  AND		pr.date					BETWEEN		? AND ?					"
											"	  AND		pr.delivered_flg		=			2						"	// = completed sale deletion.
											"	  AND		pr.credit_reason_code	=			1						"	// = cancelled because member didn't show up.
											"	  AND		pr.mem_id_extension		=			?						"
											"	  AND		mc.macabi_centers_num	=			pr.macabi_centers_num	"
											"	  AND		mc.first_center_type	=			'01';					";	// = Al Tor order.
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					NumInputColumns		=	4;
					InputColumnSpec		=	"	int, int, int, short	";
					Convert_NF_to_zero	=	1;
					StatementIsSticky	=	1;	// Made sticky 27Oct2020.
					break;


		case predict_member_participation__READ_SpecLetters:
					SQL_CommandText		=	"	SELECT	CAST (COUNT(*) AS INTEGER)												"

											"	FROM	doctor_presc															"

											"	WHERE	member_id			=  ?												"
											"	  AND	doctor_presc_id		>  0												"	// Just to conform to index structure.
											"	  AND	valid_from_date		>  0												"	// Just to conform to index structure.

											"	  AND	((largo_prescribed	=  ?)	OR																"
											"			 (largo_prescribed	IN (	SELECT	DISTINCT largo_code										"
											"										FROM	spclty_drug_grp											"
											"										WHERE	(group_code = ? OR parent_group_code = ?)	)	)	)	"

											// DonR 27Mar2017 - Ishur must have come from current prescribing doctor.
											"	  AND	doctor_id			= ?													"
											"	  AND	member_id_code		= ?													"

												  // DonR 27Mar2017 - Ishur must have been issued on the same visit date as the current doctor prescription was issued.
												  // Note that for this to work properly, we need to have a valid Doctor Prescription ID (which we should have if we're
												  // dealing with Transaction 2003/5003) or a valid Visit Number (which we get in Transaction 6003.
											"	  AND	visit_date			= ?													"

											"	  AND	specialist_ishur	> 0													";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					NumInputColumns		=	7;
					InputColumnSpec		=	"	int, int, int, int, int, short, int	";
					Convert_NF_to_zero	=	1;
					StatementIsSticky	=	1;	// DonR 28Jun2020.
					break;


		case process_spools_gadget_spool_cur:
					SQL_CommandText		=	"	SELECT	pharmacy_code,		prescription_id,	member_id,		"
											"			mem_id_extension,	largo_code,			service_code,	"
											"			service_number,		service_type,		op,				"

											"			op_price,			total_ptn,			event_date,		"
											"			doctor_id,			request_date,		request_num,	"
											"			action_code												"

											"	FROM 	gadget_spool											"

											"	WHERE 	reported_to_as400	=  ?								"
											"	  AND	as400_error_code	=  -1								";
					NumOutputColumns	=	16;
					OutputColumnSpec	=	"	int, int, int, short, int, int, int, int, int, int, int, int, int, int, int, short	";
					NumInputColumns		=	1;
					InputColumnSpec		=	"	short	";
					CursorName			=	"gadget_spool_cur";
					break;


		case process_spools_DEL_purge_sql_srv_audit:
					SQL_CommandText		=	"	DELETE FROM sql_srv_audit WHERE date < ?	";
					NumInputColumns		=	1;
					InputColumnSpec		=	"	int	";
					break;


		case process_spools_DEL_purge_sql_srv_audit_doc:
					SQL_CommandText		=	"	DELETE FROM sql_srv_audit_doc WHERE date < ?	";
					NumInputColumns		=	1;
					InputColumnSpec		=	"	int	";
					break;


		case process_spools_READ_gadget_spool_run_interval:
					SQL_CommandText		=	"	SELECT run_interval FROM gadget_spool_timer	";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	short	";
					break;


		case process_spools_UPD_gadget_spool:
					SQL_CommandText		=	"	UPDATE	gadget_spool								"

											"	SET		as400_error_code	= ?,					"
											"			update_date			= ?,					"
											"			update_time			= ?,					"
											"			reported_to_as400	= ?						"

											"	WHERE CURRENT OF gadget_spool_cur					";
					NumInputColumns		=	4;
					InputColumnSpec		=	"	short, int, int, short	";
					break;


		case process_spools_UPD_gadget_spool_timer:
					SQL_CommandText		=	"	UPDATE	gadget_spool_timer							"

											"	SET		last_run_date = ?,							"
											"			last_run_time = ?							"

											"	WHERE	last_run_date	< ?							"
											"	   OR	(last_run_date	= ? AND last_run_time <= ?)	";
					NumInputColumns		=	5;
					InputColumnSpec		=	"	int, int, int, int, int	";
					break;


		case ProcessRxLateArrivalQ_ProcRxArrivals_cur:
											// DonR 17Mar2020: Added WHERE criterion for maximum age of rows to be processed, in order
											// to keep performance from degrading when/if the rx_late_arrival_q table gets big.
											// DonR 06May2021: In order to reduce (or, hopefully, avoid completely) database contention
											// problems, restrict the cursor to SELECT only the first 50 rows (meaning the oldest rows
											// that haven't already been marked with a future next_proc_time).
					SQL_CommandText		=	"	SELECT		{FIRST} 50																			"
											"				ActionCode_in,			Member_ID_in,			Member_ID_Code_in,					"
											"				PrescID_in,				LineNo_in,													"
											"				DocID_in,				DocIDType_in,												"
											"				DocPrID_in,				LargoPrescribed_in,		LargoSold_in,						"
											"				ValidFromDate_in,		SoldDate_in,			SoldTime_in,						"
											"				OP_sold_in,				UnitsSold_in,			PrevUnsoldOP_in,					"
											"				PrevUnsoldUnits_in																	"

											"	FROM		rx_late_arrival_q																	"

											"	WHERE		rx_found			=	0															"

											"	  AND		solddate_in			>=	?															"	// DonR 17Mar2020

															// Only one of the next two lines will be applicable as a "real" criterion.
															// In normal queue-processing mode, we're looking only at next_proc_time and
															// all the criteria that would identify a specific missing doctor prescription
															// will be disabled.
											"	  AND		(?	>  0	OR	next_proc_time		<= ?						)					"
											"	  AND		(?	<= 0	OR	prescid_in			=  ?						)					"

											"	  AND		(?	=  0	OR	LargoPrescribed_in	=  ?						)					"
											"	  AND		(?	=  0	OR	DocPrID_in			=  ?						)					"
											"	  AND		(?	=  0	OR	DocID_in			=  ?						)					"

											"	ORDER BY	sale_timestamp, solddate_in, soldtime_in, actioncode_in							";
					NumOutputColumns	=	17;
					OutputColumnSpec	=	"	short, int, short, int, short, int, short, int, int, int, int, int, int, short, short, short, short	";
					NumInputColumns		=	11;
					InputColumnSpec		=	"	int, int, long, int, int, int, int, int, int, int, int	";
					break;


		case ProcessRxLateArrivalQ_UPD_rx_late_arrival_q:
					SQL_CommandText		=	"	UPDATE	rx_late_arrival_q			"

											"	SET		rx_found			= 1,	"
											"			visitdate_out		= ?,	"
											"			visittime_out		= ?,	"
											"			rx_added_date_out	= ?,	"
											"			rx_added_time_out	= ?,	"
											"			proc_timestamp		= ?		"

											"	WHERE	PrescID_in			= ?		"
											"	  AND	LineNo_in			= ?		"
											"	  AND	LargoPrescribed_in	= ?		"
 											"	  AND	DocPrID_in			= ?		"
											"	  AND	DocID_in			= ?		"
											"	  AND	DocIDType_in		= ?		"
											"	  AND	ValidFromDate_in	= ?		";
					NumInputColumns		=	12;
					InputColumnSpec		=	"	int, int, int, int, long, int, short, int, int, int, short, int	";
					break;


		case ProcessRxLateArrivalQ_UPD_SetForNextRetry:
					SQL_CommandText		=	"	UPDATE	rx_late_arrival_q			"

											"	SET		next_proc_time		= ?,	"
											"			proc_timestamp		= ?		"

											"	WHERE	PrescID_in			= ?		"
											"	  AND	LineNo_in			= ?		"
											"	  AND	LargoPrescribed_in	= ?		"
 											"	  AND	DocPrID_in			= ?		"
											"	  AND	DocID_in			= ?		"
											"	  AND	ValidFromDate_in	= ?		";
					NumInputColumns		=	8;
					InputColumnSpec		=	"	long, long, int, short, int, int, int, int	";
					break;


		case read_drug_READ_drug_list:
											// DonR 02Aug2020: Added ASCII() to retrieve single-character columns as a number, to be mapped
											// automatically to a 1-character TINYINT. This is the only way (at least in Informix) to map
											// to a simple character field without messing around with intermediate buffers.
					SQL_CommandText		=	"	SELECT	long_name,							short_name,							name_25,					"
											"			preferred_flg,						preferred_largo,					multi_pref_largo,			"
											"			del_flg,							ASCII(del_flg_cont),				is_out_of_stock,			"	// DonR 02Aug2020 ASCII() for char(0) fields.
											"			parent_group_code,					parent_group_name,					economypri_group,			"
											"			specialist_drug,					has_shaban_rules,					in_doctor_percents,			"	//  15
				
											"			class_code,							drug_book_flg,						maccabicare_flag,			"
											"			assuta_drug_status,					t_half,															"
		
														// chronic_flag can also have a value of 2, which is non-chronic. Rather than have all
														// our functions have to deal with that, simply force a value of zero in for anything
														// other than 1.
											"			CAST(CASE WHEN (chronic_flag = 1) THEN 1 ELSE 0 END AS SMALLINT) AS chronic_1_or_0,					"	// DonR 05Apr2020: Force numeric expression to output a SMALLINT.

											"			cronic_months,						additional_price,					package,					"

														// DonR 13Apr2016 - Correct the package size for a group of treatment drugs where AS/400
														// puts the ingredient strength there instead of the correct unit count of 1.
														// DonR 05Apr2020: Note that since package_size is an integer, we *don't* need to
														// CAST it as a SMALLINT.
											"			CASE WHEN (	package_size			=  ingr_1_quant													"
											"						AND	ingr_1_quant		>  1.0															"
											"						AND	package_volume		=  1.0															"
											"						AND	largo_type			IN ('M', 'T')) THEN 1 ELSE package_size END AS corrected_size,	"

											"												package_volume,						openable_pkg,				"
											"			calc_op_by_volume,					expiry_date_flag,					shape_code,					"	//  30
				
											"			shape_code_new,						form_name,							split_pill_flag,			"
											"			supplemental_1,						supplemental_2,						supplemental_3,				"
											"			supplier_code,						computersoft_code,					bar_code_value,				"
											"			no_presc_sale_flag,					home_delivery_ok,					ASCII(largo_type),			"	// DonR 02Aug2020 ASCII() for char(0) fields.
											"			in_gadget_table,					treatment_typ_cod,					in_health_pack,				"	//  45
				
											"			health_basket_new,					pharm_group_code,					member_price_code,			"
											"			price_prcnt,						must_prescribe_qty,					price_prcnt_magen,			"
											"			price_prcnt_keren,					enabled_status,						enabled_mac,				"
											"			enabled_hova,						enabled_keva,						ASCII(drug_type),			"	// DonR 02Aug2020 ASCII() for char(0) fields.
											"			priv_pharm_sale_ok,					pharm_sale_new,						ASCII(pharm_sale_test),		"	// 60 DonR 02Aug2020 ASCII() for char(0) fields.
				
											"			status_send,						sale_price,							sale_price_strudel,			"
											"			ASCII(interact_flg),				in_drug_interact,					in_overdose_table,			"	// DonR 02Aug2020 ASCII() for char(0) fields.
											"			magen_wait_months,					keren_wait_months,					drug_book_doct_flg,			"
											"			ishur_required,						ingr_1_code,						ingr_2_code,				"
											"			ingr_3_code,						ingr_1_quant,						ingr_2_quant,				"	//  75
				
											"			ingr_3_quant,						ingr_1_units,						ingr_2_units,				"
											"			ingr_3_units,						per_1_quant,						per_2_quant,				"
											"			per_3_quant,						per_1_units,						per_2_units,				"
											"			per_3_units,						purchase_limit_flg,					qty_lim_grp_code,			"
											"			ishur_qty_lim_flg,					ishur_qty_lim_ingr,					rule_status,				"	//  90
				
											"			sensitivity_flag,					sensitivity_code,					sensitivity_severe,			"
											"			needs_29_g,							print_generic_flg,					copies_to_print,			"
											"			substitute_drug,					fps_group_code,						tikrat_mazon_flag,			"
											"			allow_future_sales,					ingr_1_quant_std,					ingr_2_quant_std,			"
											"			ingr_3_quant_std,					illness_bitmap,						cont_treatment,				"	// 105
				
											"			how_to_take_code,					time_of_day_taken,					treatment_days,				"
											"			unit_abbreviation,					has_diagnoses,						tight_ishur_limits,			"
											"			qualify_future_sales,				maccabi_profit_rating,				is_orthopedic_device,		"
											"			needs_preparation,					needs_patient_measurement,			needs_patient_instruction,	"
											"			needs_refrigeration,				online_order_pickup_ok,				delivery_ok_per_shape_code,	"	// 120

											"			needs_dilution,						purchase_limits_without_discount,	ConsignmentItem,			"	// DonR 16Apr2023 User Story #432608 add ConsignmentItem.
											"			has_member_type_exclusion,			bypass_member_pharm_restriction,	compute_duration,			"	// DonR 23-31Jul2023 User Story 448931/458942/469361
											"			tikrat_piryon_pharm_type,			is_narcotic,						psychotherapy_drug,			"	// DonR 22Jul2025 User Story #427521
											"			preparation_type,					chronic_days,													"	// 131 DonR 22Jul2025/06Aug2025 User Stories #417800/427521

											"			refresh_date,						refresh_time,						changed_date,				"
											"			changed_time,						pharm_update_date,					pharm_update_time,			"
											"			doc_update_date,					doc_update_time,					as400_batch_date,			"
											"			as400_batch_time,					update_date,						update_time,				"
											"			update_date_d,						update_time_d,						pharm_diff					"	// 146

											"	FROM	drug_list																							"

											"	WHERE	largo_code	=  ?																					";
					NumOutputColumns	=	146;
					OutputColumnSpec	=	"	varchar(30),	varchar(17),	varchar(25),	short,		int,			short,	short,	char(0),	short,		int,			char(25),	int,		short,	short,	short,		"	//  15
											"	short,			short,			short,			short,		short,			short,	short,	short,		char(10),	int,			double,		short,		short,	short,	short,		"	//  30
											"	short,			varchar(25),	short,			int,		int,			int,	int,	int,		char(14),	short,			short,		char(0),	short,	short,	short,		"	//  45
											"	short,			short,			short,			int,		short,			int,	int,	short,		short,		short,			short,		char(0),	short,	short,	char(0),	"	//  60
											"	short,			char(5),		char(5),		char(0),	short,			short,	short,	short,		short,		short,			short,		short,		short,	double,	double,		"	//  75
											"	double,			char(3),		char(3),		char(3),	double,			double,	double,	char(3),	char(3),	char(3),		short,		int,		short,	short,	short,		"	//  90
											"	short,			int,			short,			short,		short,			short,	int,	int,		short,		short,			double,		double,		double,	int,	short,		"	// 105
											"	short,			short,			short,			char(3),	short,			short,	short,	short,		short,		short,			short,		short,		short,	short,	short,		"	// 120
											"	short,			short,			short,			short,		short,			short,	short,	short,		short,		short,			int,												"	// 131 22Jul2025/06Aug2025 User Stories #427521/417800 3 new flags + chronic_days
											"	int,			int,			int,			int,		int,			int,	int,	int,		int,		int,			int,		int,		int,	int,	char(50)	";	// 146
					NumInputColumns		=	1;
					InputColumnSpec		=	"	int	";
					StatementIsSticky	=	1;
					break;


		case READ_29G_TableSize:
											// DonR 02Dec2020: Added WHERE criterion, so (in theory) AS/400 can pass Linux a
											// table with one row for Member ID = 0, and Linux will treat the resulting table
											// (where everything else will have been automatically deleted) as "logically empty"
											// and will thus disable 29G checking.
					SQL_CommandText		=	"	SELECT CAST (count(*) AS INTEGER) FROM member_drug_29g WHERE member_id > 0	";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					Convert_NF_to_zero	=	1;
					break;


		case READ_AlreadyBoughtToday:
											// DonR 14May2020: Sticky SQL operations returning a SUM seem to be somewhat unstable,
											// so I'm trying an explicit CAST to INTEGER to see if that helps.
//					SQL_CommandText		=	"	SELECT	SUM(total_drug_price)			"
					SQL_CommandText		=	"	SELECT	CAST (SUM (total_drug_price) AS INTEGER)	"

											"	FROM	prescription_drugs							"

											"	WHERE	member_id			=  ?					"
											"	  AND	date				=  ?					"
											"	  AND	pharmacy_code		=  ?					"
											"	  AND	delivered_flg		=  1					"
											"	  AND	action_type			IN (0,1)				"	// Drug sales only.
											"	  AND	presc_source		>  0					"	// Ignore non-prescription sales.
											"	  AND	del_flg				=  0					"
											"	  AND	mem_id_extension	=  ?					";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					NumInputColumns		=	4;
					InputColumnSpec		=	"	int, int, int, short	";
					StatementIsSticky	=	1;	// DonR 25Mar2020.
					break;


		case READ_CheckDrugBlockedForMember:
											// DonR 23Jul2023 User Story 448931/458942: Add logic to handle both the
											// previous type of per-member blocked drug (now "Restriction Type 1")
											// and global blocks on *all* users of a particular type (i.e. darkonaim)
											// set by Restriction Type 2.
											// DonR 13Mar2025 User Story #384811: We are adding 3 new restriction types for
											// subsets of the Darkonaim. Type 11 is for Maccabi Darkonaim (darkonai_type = 0),
											// Type 12 is for Harel Tourists (darkonai_type = 1), and Type 13 is for Harel
											// Foreign Workers (darkonai_type = 2). We could test for
											// "restriction_type = darkonai_type + 11", but I think it's cleaner to test
											// explicitly for each combination even though it means passing darkonai_type
											// multiple times.
					SQL_CommandText		=	"	SELECT	CAST (count(*) AS INTEGER)											"
											"	FROM	member_blocked_drugs												"
											"	WHERE	(	(restriction_type	=  1 AND member_id = ?)				OR		"
//											"				(restriction_type	=  2 AND member_id = 0)		)				"
											"				(restriction_type	=  2 AND member_id = 0)				OR		"
											"				(restriction_type	= 11 AND member_id = 0 AND ? = 0)	OR		"
											"				(restriction_type	= 12 AND member_id = 0 AND ? = 1)	OR		"
											"				(restriction_type	= 13 AND member_id = 0 AND ? = 2)		)	"
											"	  AND	largo_code				= ?											"
											"	  AND	mem_id_extension		= ?											";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					NumInputColumns		=	6;
					InputColumnSpec		=	"	int, short, short, short, int, short	";
					Convert_NF_to_zero	=	1;
					break;


		case READ_check_family_credit_used:
											// DonR 14May2020: Sticky SQL operations returning a SUM seem to be somewhat unstable,
											// so I'm trying an explicit CAST to INTEGER to see if that helps.
											// DonR 05Feb2023 User Story #232220: Treat non-prescription sales for which the
											// payment type has not yet been set (credit_type_used == 99) as if they will be
											// put on credit, so that the member can't use multiple sales to exceed the family
											// credit limit.
											// DonR 27Mar2023 User Story #232220 FIX: If the Credit Type sent to the bank in
											// Transaction 6003 was zero (= no credit available), do *not* include the sale in
											// the total of family credit used.
//					SQL_CommandText		=	"	SELECT	SUM (total_member_price)					"
					SQL_CommandText		=	"	SELECT	CAST (SUM (total_member_price) AS INTEGER)	"

											"	FROM	members				AS m,					"
											"			prescription_drugs	AS d,					"
											"			prescriptions		AS p					"

											"	WHERE	m.idnumber_main		=  ?					"
											"	  AND	m.idcode_main		=  ?					"
											"	  AND	d.member_id			=  m.member_id			"
											"	  AND	d.date				>= ?					"	// We can safely (?) assume nothing is future-dated.
											"	  AND	d.delivered_flg		=  1					"
											"	  AND	d.presc_source		=  0					"
											"	  AND	d.del_flg			=  0					"
											"	  AND	d.mem_id_extension	=  m.mem_id_extension	"
											"	  AND	p.prescription_id	=  d.prescription_id	"
//											"	  AND	p.credit_type_used	=  9					";	// DonR 07Apr2019
//											"	  AND	p.credit_type_used	IN (9, 99)				";	// DonR 05Feb2023 User Story #232220
											"	  AND	(p.credit_type_used	=  9 OR (p.credit_type_used	= 99 AND p.credit_type_code > 0))	";	// DonR 27Mar2023 User Story #232220 FIX.
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					NumInputColumns		=	3;
					InputColumnSpec		=	"	int, short, int	";
					StatementIsSticky	=	1;	// DonR 25Mar2020.
					break;


		case READ_check_first_doctor_prescription_is_in_database:
					SQL_CommandText		=	"	SELECT	CAST (count(*) AS INTEGER)												"

											"	FROM	doctor_presc															"

											"	WHERE	member_id				=		?										"
											"	  AND	doctor_presc_id			=		?										"
											"	  AND	valid_from_date			BETWEEN	? AND ?									"
											"	  AND	(	largo_prescribed	=		?										"
											"					OR																"
											"				largo_prescribed	IN		(										"
											"												SELECT	largo_code					"
											"												FROM	drug_list					"
											"												WHERE	economypri_group	> 0		"
											"												  AND	economypri_group	= ?		"
											"											)										"
											"			)																		"
											"	  AND	doctor_id				=		?										"
											"	  AND	member_id_code			=		?										"
											"	  AND	sold_status				IN		(0, 1)									";	 // Either unsold or partially sold.
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					NumInputColumns		=	8;
					InputColumnSpec		=	"	int, int, int, int, int, int, int, short	";
					StatementIsSticky	=	1;	// DonR 25Mar2020.
					Convert_NF_to_zero	=	1;
					break;


		case READ_check_for_early_refill:
											// DonR 24Nov2024 User Story #366220: Select prior sales of generic equivalents
											// as well as sales of the drug currently being sold.
					SQL_CommandText		=	"	SELECT	CAST (count(*) AS INTEGER)										"

											"	FROM	prescription_drugs												"

											"	WHERE	member_id				=  ?									"
//											"	  AND	largo_code				=  ?		"
											"	  AND	(	largo_code	=		?										"
											"					OR														"
											"				largo_code	IN		(										"
											"										SELECT	largo_code					"
											"										FROM	drug_list					"
											"										WHERE	economypri_group	> 0		"
											"										  AND	economypri_group	= ?		"
											"									)										"
											"			)																"
											"	  AND	date					>  ?									"
											"	  AND	delivered_flg			=  1									"	// DonR 14Jun2010 - Sales only!
											"	  AND	mem_id_extension		=  ?									"
											"	  AND	del_flg					=  0									"
											"	  AND	presc_source			<> 0									";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					NumInputColumns		=	5;
					InputColumnSpec		=	"	int, int, int, int, short	";
					StatementIsSticky	=	1;	// DonR 25Mar2020.
					Convert_NF_to_zero	=	1;
					break;


		case READ_count_one_day_OTC_purchases:
					// DonR 29Jan2024 User Story #205423/#545773: While the daily limit is expressed in OP,
					// we need to compute previous and current purchases using units as well as OP.
					SQL_CommandText		=	"	SELECT	CAST (sum(op)		AS INTEGER),	"
											"			CAST (sum(units)	AS INTEGER)		"

											"	FROM	prescription_drugs					"

											"	WHERE	member_id				= ?			"
											"	  AND	largo_code				= ?			"
											"	  AND	date					= ?			"
											"	  AND	delivered_flg			= 1			"	// Sales only.
											"	  AND	pharmacy_code			= ?			"
											"	  AND	mem_id_extension		= ?			"
											"	  AND	del_flg					= 0			"
											"	  AND	presc_source			= 0			";	// OTC only.
					NumOutputColumns	=	2;
					OutputColumnSpec	=	"	int, int	";
					NumInputColumns		=	5;
					InputColumnSpec		=	"	int, int, int, int, short	";
//					StatementIsSticky	=	1;	// Probably not super high volume - but maybe play with stickiness later.
					Convert_NF_to_zero	=	1;
					break;


		case READ_CheckForPreviousPurchaseOfDrug:
					SQL_CommandText		=	"	SELECT	CAST (count(*) AS INTEGER)				"

											"	FROM	prescription_drugs						"

											"	WHERE	member_id				=		?		"
											"	  AND	largo_code				=		?		"
											"	  AND	date					BETWEEN	? AND ?	"
											"	  AND	delivered_flg			=		1		"
											"	  AND	mem_id_extension		=		?		"
											"	  AND	del_flg					=		0		";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					NumInputColumns		=	5;
					InputColumnSpec		=	"	int, int, int, int, short	";
//					StatementIsSticky	=	1;	// At least for now, this query shouldn't be used very often - only for some AIDS drugs.
					Convert_NF_to_zero	=	1;
					break;


		case READ_check_for_specialist_discount_to_enable_full_price_sale:
					SQL_CommandText		=	"	SELECT	CAST (count(*) AS INTEGER)									"

											"	FROM	doctor_speciality	ds,										"
											"			spclty_largo_prcnt	lp										"

											"	WHERE	ds.doctor_id		= ?										"
											"	  AND	ds.speciality_code	NOT BETWEEN 58000 AND 58999				"	// Exclude dentist stuff
											"	  AND	ds.speciality_code	NOT BETWEEN  4000 AND  4999				"	// Exclude "home visit" stuff
											"	  AND	(ds.del_flg			= ? OR ds.del_flg IS NULL)				"
											"	  AND	lp.speciality_code	= ds.speciality_code					"
											"	  AND	(lp.largo_code		= ? OR lp.largo_code = ?)				"
											"	  AND	lp.largo_code		> 0										"

												  // DonR 212Nov2011 - Add selection criteria for member type.
											"	  AND	(((enabled_mac	> 0) AND (?	> 0))	OR						"
											"			 ((enabled_hova	> 0) AND (?	> 0))	OR						"
											"			 ((enabled_keva	> 0) AND (?	> 0)))							"

											"	  AND	(lp.del_flg			= ? OR lp.del_flg IS NULL)				"

												  // DonR 09Dec2012 "Yahalom" - New insurance-type and "vetek" criteria.
												  // Note that the price code/fixed price combination is now validated
												  // without regard to which insurance is applicable.
											"	  AND	(price_code > 1 OR (price_code = 1 AND fixed_price > 0))	"

											"	  AND	insurance_type IN ('Y', 'Z')								";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					NumInputColumns		=	8;
					InputColumnSpec		=	"	int, short, int, int, short, short, short, short	";
					Convert_NF_to_zero	=	1;
					StatementIsSticky	=	1;	// Made sticky 27Oct2020.
					break;


		case READ_check_pharmacy_message_count:
					SQL_CommandText		=	"	SELECT CAST (COUNT(*) AS INTEGER) FROM pharmacy_message WHERE pharmacy_code = ? AND delivery_date = 0		";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					NumInputColumns		=	1;
					InputColumnSpec		=	"	int	";
					StatementIsSticky	=	1;	// DonR 25Mar2020.
					Convert_NF_to_zero	=	1;
					break;


		case READ_count_duplicate_drug_sales:
												// DonR 07Jun2020: Made sticky.
					SQL_CommandText		=	"	SELECT	CAST (count(*) AS INTEGER)	"

											"	FROM	prescriptions				"

											"	WHERE	pharmacy_code		=  ?	"
											"	  AND	diary_month			=  ?	"
											"	  AND	presc_pharmacy_id	=  ?	"
											"	  AND	member_institued	=  ?	"
											"	  AND	delivered_flg		<> ?	";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					NumInputColumns		=	5;
					InputColumnSpec		=	"	int, short, int, short, short	";
					StatementIsSticky	=	1;
					Convert_NF_to_zero	=	1;
					break;


		case READ_deleted_sale_date:
					SQL_CommandText		=	"	SELECT date FROM prescriptions WHERE prescription_id = ?	";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					NumInputColumns		=	1;
					InputColumnSpec		=	"	int	";
					break;


		case READ_DoctorByLicenseNumber:
					SQL_CommandText		=	"	SELECT {FIRST} 1	dr.first_name,			dr.last_name,	dr.phone,	"
											"						dr.check_interactions,	dr.doctor_id				"

											"	FROM				doctors			as dr,								"
											"						doctor_percents	as dp								"

											"	WHERE				dp.license		= ?									"
											"	  AND				dp.license		> 0									"
											"	  AND				dp.del_flg		= 0									"	// Probably not needed.
											"	  AND				dr.doctor_id	= dp.doctor_id						"
											"	  AND				dr.doctor_id	> 0									"	// Probably not needed.
											"	  AND				dr.del_flg		= 0									";	// Probably not needed.
					NumOutputColumns	=	5;
					OutputColumnSpec	=	"	varchar(8), varchar(14), varchar(10), short, int	";
					NumInputColumns		=	1;
					InputColumnSpec		=	"	int	";
					StatementIsSticky	=	1;	// DonR 30Jun2020.
					break;


		case READ_doctors:
					SQL_CommandText		=	"	SELECT	first_name, last_name, phone, check_interactions	"
											"	FROM	doctors												"
											"	WHERE	doctor_id = ?										"
											"	  AND	doctor_id > 0										";
					NumOutputColumns	=	4;
					OutputColumnSpec	=	"	varchar(8), varchar(14), varchar(10), short	";
					NumInputColumns		=	1;
					InputColumnSpec		=	"	int	";
					StatementIsSticky	=	1;	// DonR 25Mar2020.
					break;


		case READ_FamilyMembers_cur:
					SQL_CommandText		=	"	SELECT	member_id,	mem_id_extension	"

											"	FROM	members							"

											"	WHERE	idnumber_main		=  ?		"
											"	  AND	idcode_main			=  ?		"
											"	  AND	member_id			<> ?		";
					NumOutputColumns	=	2;
					OutputColumnSpec	=	"	int, short	";
					NumInputColumns		=	3;
					InputColumnSpec		=	"	int, short, int	";
					StatementIsSticky	=	1;	// DonR 30Jun2020.
					break;


		case READ_FamilySales_cur:
					SQL_CommandText		=	"	SELECT	prescription_id									"

											"	FROM	members				AS m,						"
											"			prescriptions		AS p						"

												// Add fields/index to prescriptions table so that family
												// selection can be done directly w/o reading members?
											"	WHERE	m.idnumber_main		=		?					"
											"	  AND	m.idcode_main		=		?					"
											"	  AND	p.member_id			=		m.member_id			"
											"	  AND	p.date				BETWEEN	? AND ?				"
											"	  AND	p.delivered_flg		IN		(1,2)				"	// So sales and sale deletions are both included.
											"	  AND	p.presc_source		>		0					"
											"	  AND	p.tikra_called_flag =		1					"
											"	  AND	p.tikra_error_code	=		0					"
											"	  AND	p.mem_id_extension	=		m.mem_id_extension	"
											"	  AND	p.del_flg			=		0					"

											"	ORDER BY	date DESC, time DESC						";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					NumInputColumns		=	4;
					InputColumnSpec		=	"	int, short, int, int	";
					StatementIsSticky	=	1;	// DonR 25Mar2020.
					break;


		case READ_Find_member_diagnosis:
					SQL_CommandText		=	"	SELECT {FIRST} 1	diagnosis_code					"

											"	FROM				drug_diagnoses					"

											"	WHERE				largo_code		=  ?			"
											"	  AND				diagnosis_code	IN (	%s	)	"

											"	ORDER BY			diagnosis_code					";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					NumInputColumns		=	1;
					InputColumnSpec		=	"	int	";
					NeedsWhereClauseID	=	1;
					break;


		case READ_Find_Prescription_ID_to_delete:
					SQL_CommandText		=	"	SELECT	MIN (prescription_id)		"

											"	FROM	prescriptions				"

											"	WHERE	member_id			=  ?	"
											"	  AND	pharmacy_code		=  ?	"
											"	  AND	diary_month			=  ?	"
											"	  AND	presc_pharmacy_id	=  ?	"
											"	  AND	delivered_flg		=  ?	"	// Only drug sales are relevant here.
											"	  AND	mem_id_extension	=  ?	"
											"	  AND	member_institued	=  ?	"
											"	  AND	del_flg				<> 2	";	// <> ROW_SYS_DELETED, i.e. a duplicate sale from spool.
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					NumInputColumns		=	7;
					InputColumnSpec		=	"	int, int, short, int, short, short, short	";
					break;


		case READ_Gadgets_for_sale_completion:
					SQL_CommandText		=	"	SELECT	service_code,	service_number,	service_type	"
											"	FROM	gadgets											"
											"	WHERE	largo_code		= ?								"
											"	  AND	((gadget_code	= ?) OR (? = 0))				";
					NumOutputColumns	=	3;
					OutputColumnSpec	=	"	int, short, short	";
					NumInputColumns		=	3;
					InputColumnSpec		=	"	int, int, int	";
					break;


		case READ_get_deleted_sale_diary_month:
					SQL_CommandText		=	"	SELECT diary_month FROM prescriptions WHERE prescription_id = ?	";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	short	";
					NumInputColumns		=	1;
					InputColumnSpec		=	"	int	";
					break;


		case READ_GetStandardMaccabiPharmPriceForMeishar:
					SQL_CommandText		=	"	SELECT	yarpa_price				"
											"	FROM 	price_list				"
											"	WHERE 	largo_code      = ?		"
											"	  AND 	price_list_code = 2		";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					NumInputColumns		=	1;
					InputColumnSpec		=	"	int	";
					break;


		case READ_Get_X003_deleted_prescription_row_values:
					SQL_CommandText		=	"	SELECT	date,	subsidy_amount,		credit_type_used,	del_flg		"
											"	FROM	prescriptions												"
											"	WHERE	prescription_id	= ?											"
											"	  AND	delivered_flg	= ?											";	// Only drug sales are relevant here.
					NumOutputColumns	=	4;
					OutputColumnSpec	=	"	int, int, short, short	";
					NumInputColumns		=	2;
					InputColumnSpec		=	"	int, short	";
					break;


		case READ_Largo_99997_ishur:
					SQL_CommandText		=	"	SELECT special_presc_num, treatment_start FROM special_prescs WHERE %s	";
					NumOutputColumns	=	2;
					OutputColumnSpec	=	"	int, int	";
					NeedsWhereClauseID	=	1;
					StatementIsSticky	=	1;	// Made sticky 27Oct2020.
					break;


		case READ_MaccabiCenters_PatientFacility:
					SQL_CommandText		=	"	SELECT	credit_flag,	first_center_type	"
											"	FROM	macabi_centers						"
											"	WHERE	macabi_centers_num	= ?				"
											"	  AND	del_flg				= 0				";
					NumOutputColumns	=	2;
					OutputColumnSpec	=	"	short, char(2)	";
					NumInputColumns		=	1;
					InputColumnSpec		=	"	int	";
					break;


		case READ_MaccabiCenters_ValidateDocFacility:
					SQL_CommandText		=	"	SELECT	CAST (COUNT(*) AS INTEGER)	"
											"	FROM	macabi_centers				"
											"	WHERE	macabi_centers_num	= ?		"
											"	  AND	del_flg				= 0		";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					NumInputColumns		=	1;
					InputColumnSpec		=	"	int	";
					Convert_NF_to_zero	=	1;
					break;


		case READ_member_price_cur:
					SQL_CommandText		=	"	SELECT	participation_name,		calc_type_code,		member_institued,	"
											"			yarpa_part_code,		tax,				member_price_code,	"
											"			update_date,			update_time,		del_flg,			"
											"			member_price_prcnt,		max_pkg_price							"

											"	FROM	member_price													"
											"	WHERE	del_flg = 0														";
					NumOutputColumns	=	11;
					OutputColumnSpec	=	"	char(25), short, short, short, int, short, int, int, short, short, int	";
					break;


		case READ_MemberCard:
					SQL_CommandText		=	"	SELECT	cardissuedate			"
											"	FROM	membercard				"
											"	WHERE	idnumber		= ?		"
											"	  AND	idcode			= ?		"
											"	  AND	cardissuedate	= ?		";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					NumInputColumns		=	3;
					InputColumnSpec		=	"	int, short, int	";
					break;


		case READ_MemberPharm:
					SQL_CommandText		=	"	SELECT	pharmacy_code,			pharmacy_type,			open_date,				"
											"			close_date,				pharmacy_code_temp,		pharmacy_type_temp,		"
											"			open_date_temp,			close_date_temp,		restriction_type,		"
											"			permitted_pharm_owner													"

											"	FROM	MemberPharm																"

											"	WHERE	member_id			=  ?												"
											"	  AND	mem_id_extension	=  ?												"
											"	  AND	del_flg				=  ?												"
											"	  AND	((open_date      <= ? AND close_date      >= ?) OR						"
											"			 (open_date_temp <= ? AND close_date_temp >= ?))						";
					NumOutputColumns	=	10;
					OutputColumnSpec	=	"	int, short, int, int, int, short, int, int, short, short	";
					NumInputColumns		=	7;
					InputColumnSpec		=	"	int, short, short, int, int, int, int	";
					StatementIsSticky	=	1;	// DonR 30Jun2020.
					break;


		case READ_members_full:
											// DonR 29Jul2025 User Story #435969: Add dangerous_drug_status column.
					SQL_CommandText		=	"	SELECT	last_name,					first_name,				date_of_bearth,			"
											"			maccabi_code,				spec_presc,				maccabi_until,			"
											"			payer_tz,					payer_tz_code,			sex,					"
											"			phone,						house_num,				street,					"
											"			city,						zip_code,				insurance_type,			"	// 15

											"			keren_mac_code,				keren_mac_from,			keren_mac_until,		"
											"			mac_magen_code,				mac_magen_from,			mac_magen_until,		"
											"			yahalom_code,				yahalom_from,			yahalom_until,			"
											"			carry_over_vetek,			keren_wait_flag,		illness_bitmap,			"
											"			card_date,					update_date,			update_time,			"	// 30

											"			authorizealways,			updated_by,				check_od_interact,		"
											"			credit_type_code,			max_drug_date,			member_discount_pt,		"
											"			insurance_status,			idnumber_main,			idcode_main,			"
											"			has_tikra,					has_coupon,				in_hospital,			"
											"			asaf_code,					darkonai_type,			force_100_percent_ptn,	"	// 45 Name of "asaf_code" should be changed to ventilator_discount in MS-SQL!

											"			darkonai_no_card,			has_blocked_drugs,		died_in_hospital,		"
											"			mahoz,						dangerous_drug_status									"	// 50

											"	FROM	members																	"

											"	WHERE	member_id			= ?													"
											"	  AND	mem_id_extension	= ?													";
					NumOutputColumns	=	50;	// DonR 29Jul2025 User Story #435969: 49->50.
					OutputColumnSpec	=	"	varchar(14),	varchar(8),		int,			short,	short,			"
											"	int,			int,			short,			short,	varchar(10),	"
											"	char(4),		varchar(12),	varchar(20),	int,	char(1),		"	// 15

											"	short,	int,	int,	short,	int,	int,	short,	int,	int,	int,	short,	int,	short,	int,	int,	"	// 30
											"	short,	int,	short,	short,	int,	short,	short,	int,	short,	short,	short,	short,	short,	short,	short,	"	// 45
											"	short,	short,	short,	short,	short																					";	// 50
					NumInputColumns		=	2;
					InputColumnSpec		=	"	int, short	";
					StatementIsSticky	=	1;	// DonR 25Mar2020.
					break;


		case READ_members_MaxDrugDate_and_SpecPresc:
					SQL_CommandText		=	"	SELECT		max_drug_date,	spec_presc		"
											"	FROM		members							"
											"	WHERE		member_id			= ?			"
											"	  AND		mem_id_extension	= ?			";
					NumOutputColumns	=	2;
					OutputColumnSpec	=	"	int, short	";
					NumInputColumns		=	2;
					InputColumnSpec		=	"	int, short	";
					StatementIsSticky	=	1;	// Made sticky 27Oct2020.
					break;


		case READ_online_orders_TimestampsAndMember:
					SQL_CommandText		=	"	SELECT	member_id,				mem_id_extension,	"
											"			made_date,				made_time,			"
											"			start_work_date,		start_work_time		"

											"	FROM	online_orders								"

											"	WHERE	online_order_num	=  ?					"
											"	  AND	made_by_pharmacy	=  ?					"
											"	  AND	made_by_owner		=  ?					";
					NumOutputColumns	=	6;
					OutputColumnSpec	=	"	int, short, int, int, int, int	";
					NumInputColumns		=	3;
					InputColumnSpec		=	"	long, int, short	";
					break;


		case READ_pharmacy_message_undelivered:
					SQL_CommandText		=	"	SELECT message_id FROM pharmacy_message WHERE pharmacy_code = ? AND delivery_date = 0	";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					NumInputColumns		=	1;
					InputColumnSpec		=	"	int	";
					break;


		case READ_prescription_drugs_member_ptn_amt:
					SQL_CommandText		=	"	SELECT member_ptn_amt FROM prescription_drugs WHERE prescription_id = ? AND largo_code = ?	";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					NumInputColumns		=	2;
					InputColumnSpec		=	"	int, int	";
					break;


		case READ_prescriptions_check_for_duplicate_spooled_sale:
					SQL_CommandText		=	"	SELECT	CAST (count(*) AS INTEGER)	"

											"	FROM 	prescriptions				"

											"	WHERE	pharmacy_code		=  ?	"
											"	  AND 	diary_month			=  ?	"
											"	  AND  	presc_pharmacy_id	=  ?	"
											"	  AND	member_institued	=  ?	"
											"	  AND	member_price_code	=  ?	"
											"	  AND	del_flg				=  0	"
											"	  AND  	delivered_flg		<> ?	";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					NumInputColumns		=	6;
					InputColumnSpec		=	"	int, short, int, short, short, short	";
					Convert_NF_to_zero	=	1;
					break;


		case READ_prescriptions_for_sale_completion:
											// DonR 07Jun2020: Made sticky.
					SQL_CommandText		=	"	SELECT	pharmacy_code,			institued_code,			"
											"			mem_id_extension,		member_id,				"
											"			member_institued,		doctor_id,				"
											"			doctor_id_type,			presc_source,			"
											"			error_code,				prescription_lines,		"

											"			terminal_id,			price_list_code,		"
											"			member_discount_pt,		date,					"
											"			diary_month,			action_type,			"
											"			del_presc_id,			tikra_discount,			"
											"			subsidy_amount,			online_order_num,		"
											"			date,					time					"

											"	FROM	prescriptions									"
											"	WHERE	prescription_id = ?								";
					NumOutputColumns	=	22;
					OutputColumnSpec	=	"	int, short, short, int, short, int, short, short, short, short,			"
											"	short, short, short, int, short, short, int, int, int, long, int, int	";
					NumInputColumns		=	1;
					InputColumnSpec		=	"	int	";
					StatementIsSticky	=	1;
					break;


		case READ_prescriptions_validate_deleted_pr_ID:
											// DonR 23Jul2025: I don't understand why, but we're seeing deadlock errors
											// from this query. (I'm not sure if the error message is even accurate!)
											// So I'm going to change how this works: Instead of doing a conventional
											// SELECT and forcing an error if the Prescription ID isn't found, I'm going
											// to us an EXISTS test to return a boolean found/not found value. That way the
											// query should always "succeed", even if the Prescription ID wasn't found.
//					SQL_CommandText		=	"	SELECT prescription_id FROM prescriptions WHERE prescription_id = ?	";
					SQL_CommandText		=	"	SELECT	CASE	WHEN EXISTS (SELECT * FROM prescriptions WHERE prescription_id = ?)	"
											"					THEN 1	"
											"					ELSE 0	"
											"			END				";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	bool	";
					NumInputColumns		=	1;
					InputColumnSpec		=	"	int	";
					break;


		case READ_price_from_sale_bonus_recv:
					SQL_CommandText		=	"	SELECT	{FIRST} 1	sale_price									"

											"	FROM				sale_bonus_recv	AS sbr,						"
											"						sale										"

											"	WHERE				sbr.largo_code			=  ?				"
											"	  AND				sbr.sale_price			>  0				"
											"	  AND				sbr.sale_price			<  ?				"
											"	  AND				sbr.discount_percent	=  0				"
											"	  AND				sbr.del_flg				=  0				"
											"	  AND				sale.sale_number		=  sbr.sale_number	"
											"	  AND				sale.sale_type			=  14				"
											"	  AND				sale.sale_audience		=  1				"
											"	  AND				sale.start_date			<= ?				"
											"	  AND				sale.end_date			>= ?				"
											"	  AND				sale.del_flg			=  0				"

											"	ORDER BY			sbr.sale_price								";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					NumInputColumns		=	4;
					InputColumnSpec		=	"	int, int, int, int	";
					StatementIsSticky	=	1;	// DonR 25Mar2020.
					break;


		case READ_PriceList:
					SQL_CommandText		=	"	SELECT	yarpa_price,	macabi_price,	supplier_yarpa		"
											"	FROM 	price_list											"
											"	WHERE 	%s													";
					NumOutputColumns	=	3;
					OutputColumnSpec	=	"	int, int, int	";
					NeedsWhereClauseID	=	1;
					break;


		case READ_PriceList_simple:			// DonR 30Jun2020: Added this as a fast, sticky version of
											// READ_PriceList without the custom WHERE clause.
					SQL_CommandText		=	"	SELECT	yarpa_price,	macabi_price,	supplier_yarpa		"
											"	FROM 	price_list											"
											"	WHERE 	largo_code      = ?									"
											"	  AND 	price_list_code = ?									";
					NumOutputColumns	=	3;
					OutputColumnSpec	=	"	int, int, int	";
					NumInputColumns		=	2;
					InputColumnSpec		=	"	int, short	";
					StatementIsSticky	=	1;
					break;


		case READ_AlternateYarpaPrice:		// DonR 29Jun2023 User Story #461368 - conditional read of an "alternate"
											// Yarpa price for member-participation calculation, to enable an inflation-
											// reduction measure mandated by the Ministry of Health.
					SQL_CommandText		=	"	SELECT	yarpa_price								"
											"	FROM 	price_list								"
											"	WHERE 	largo_code      =		?				"
											"	  AND 	price_list_code =		?				"
											"	  AND	yarpa_price		>		0				" 	// Marianna 09Jul2023 User Story 463302
											"	  AND	((yarpa_price	<		?) OR (? = 0))	";	// Marianna 09Jul2023 User Story 463302
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					NumInputColumns		=	4;
					InputColumnSpec		=	"	int, short, int, short	";
					StatementIsSticky	=	1;
					break;




		case READ_TablesUpdate:
					SQL_CommandText		=	"	SELECT update_date, update_time FROM tables_update WHERE table_name = ?	";
					NumOutputColumns	=	2;
					OutputColumnSpec	=	"	int, int	";
					NumInputColumns		=	1;
					InputColumnSpec		=	"	varchar(32)	";
					break;


		case READ_tempmemb:
					SQL_CommandText		=	"	SELECT	idcode,		idnumber,	validuntil		"
											"	FROM	tempmemb								"
											"	WHERE	cardid  = ?								";
					NumOutputColumns	=	3;
					OutputColumnSpec	=	"			short,		int,		int	";
					NumInputColumns		=	1;
					InputColumnSpec		=	"	int	";
					break;


		case CheckIfValidTempmembExists:
					SQL_CommandText		=	"	SELECT	CAST (count(*) AS INTEGER)				"
											"	FROM	tempmemb								"
											"	WHERE	idcode		=  ?						"
											"	  AND	idnumber	=  ?						"
											"	  AND	validuntil	>= ?						";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					NumInputColumns		=	3;
					InputColumnSpec		=	"	short, int, int	";
					Convert_NF_to_zero	=	1;
					break;


		case READ_test_for_discount_with_pharmacy_ishur:
					// DonR 20Feb2024 User Story #540566: Pharmacy "nohalim" can have Confirming Authority
					// between 1 and 24, not just from 1 to 10.
					SQL_CommandText		=	"	SELECT	CAST (count(*) AS INTEGER)									"

											"	FROM	drug_extension												"

											"	WHERE	(largo_code			=  ?	OR largo_code = ?)				"
											"	  AND	largo_code			>  0									"
											"	  AND	from_age			<= ?									"
											"	  AND	to_age				>= ?									"
											"	  AND	(sex				=  ?		OR sex = 0)					"

											"	  AND	effective_date		<= ?									"
											"	  AND	confirm_authority	BETWEEN 1 AND 24						"	// Pharmacy-ishur level rules only.
											"	  AND	del_flg				=  ?									"

												  // DonR 212Nov2011 - Add selection criteria for member type.
											"	  AND	(((enabled_mac	> 0) AND (?	> 0))	OR						"
											"			 ((enabled_hova	> 0) AND (?	> 0))	OR						"
											"			 ((enabled_keva	> 0) AND (?	> 0)))							"

											"	  AND	send_and_use		<> 0									"

												  // Maccabi pharmacies can use all rules; Prati Plus pharmacies can use
												  // private-pharmacy rules as well as Prati Plus rules.
												  // Note that this works by Pharmacy Permission and *not* by ownership! For
												  // this purpose, "consignatzia" pharmacies look like Maccabi pharmacies.
											"	  AND	((? = 1)								OR					"
											"			 (? = 6 AND permission_type IN (0,2,6))	OR					"
											"			 (? = 0 AND permission_type IN (0,2)  ))					"

												  // DonR 06Dec2012 "Yahalom" - New insurance-type and "vetek" criteria.
												  // Note that the price code/fixed price combination is now validated
												  // without regard to which insurance is applicable.
											"	  AND	(price_code > 1 OR (price_code = 1 AND fixed_price > 0))	"

											"	  AND	((insurance_type = 'B')						OR				"
											"			 (insurance_type = ? AND ? >= wait_months)	OR				"
											"			 (insurance_type = ? AND ? >= wait_months))					";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					NumInputColumns		=	17;
					InputColumnSpec		=	"	int, int, short, short, short, int, short, short, short, short,		"
											"	short, short, short, char(1), short, char(1), short					";
					StatementIsSticky	=	1;	// DonR 25Mar2020.
					Convert_NF_to_zero	=	1;
					break;


		case READ_test_for_ERR_DISCOUNT_AT_MAC_PH_WARN:
					SQL_CommandText		=	"	SELECT	CAST (count(*) AS INTEGER)									"

											"	FROM	drug_extension												"

											"	WHERE	(largo_code			=  ?	OR largo_code = ?)				"
											"	  AND	largo_code			>  0									"
											"	  AND	rule_number			>  -999									"	// Just to conform to index structure.
											"	  AND	from_age			<= ?									"
											"	  AND	to_age				>= ?									"
											"	  AND	(sex				=  ?		OR sex = 0)					"

												  // Maccabi and Prati-Plus both outrank private pharmacies; and Maccabi
												  // pharmacies outrank Prati-Plus ones.
											"	  AND	((permission_type	IN (1,6)	AND ? =   0)	OR			"
											"			 (permission_type	=  1		AND ? IN (0,6)))			"

											"	  AND	del_flg				=  ?									"

												  // DonR 212Nov2011 - Add selection criteria for member type.
											"	  AND	(((enabled_mac	> 0) AND (?	> 0))	OR						"
											"			 ((enabled_hova	> 0) AND (?	> 0))	OR						"
											"			 ((enabled_keva	> 0) AND (?	> 0)))							"

											"	  AND	effective_date		<= ?									"
											"	  AND	confirm_authority	=  0									"
											"	  AND	send_and_use		<> 0									"

												  // DonR 06Dec2012 "Yahalom" - New insurance-type and "vetek" criteria.
												  // Note that the price code/fixed price combination is now validated
												  // without regard to which insurance is applicable.
											"	  AND	(price_code > 1 OR (price_code = 1 AND fixed_price > 0))	"

											"	  AND	((insurance_type = 'B')						OR				"
											"			 (insurance_type = ? AND ? >= wait_months)	OR				"
											"			 (insurance_type = ? AND ? >= wait_months))					";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					NumInputColumns		=	16;
					InputColumnSpec		=	"	int, int, short, short, short, short, short, short, short, short,	"
											"	short, int, char(1), short, char(1), short							";
					StatementIsSticky	=	1;	// DonR 25Mar2020.
					Convert_NF_to_zero	=	1;
					break;


		case READ_test_for_special_substitution:
					SQL_CommandText		=	"	SELECT	{FIRST} 1 dl.largo_code, dl.shape_code_new								"

											"	FROM	prescription_drugs	AS pd,												"
											"			drug_list			AS dl												"

											"	WHERE	pd.member_id			=  ?											"
											"	  AND	pd.date					>= ?											"
											"	  AND	pd.delivered_flg		=  1											"	// Drug sales only!
											"	  AND	pd.del_flg				=  0											"
											"	  AND	pd.presc_source			<> 0											"	// OTC sales don't count.
											"	  AND	pd.mem_id_extension		=  ?											"
											"	  AND	dl.largo_code			=  pd.largo_code								"
											"	  AND	dl.economypri_group		=  ?											"

											"	ORDER BY	pd.date DESC, pd.time DESC											";	// The last shall be {FIRST}}!
					NumOutputColumns	=	2;
					OutputColumnSpec	=	"	int, short	";
					NumInputColumns		=	4;
					InputColumnSpec		=	"	int, int, short, int	";
					StatementIsSticky	=	1;	// DonR 30Jun2020. This operation runs around 10,000 times per day, so it's "borderline" for stickiness.
					break;


		case READ_test_for_ERR_EARLY_REFILL_WARNING:
											// DonR 24Nov2024 User Story #366220: Multiple changes in this SELECT - see
											// the inline comments below.
											// DonR 03Feb2025 User Story #366220 bug fix: The two parameters for
											// pd.duration need to be type "short", not type "int".
					SQL_CommandText		=	"	SELECT	CAST (count(*) AS INTEGER)												"

											"	FROM	prescription_drugs	AS pd												"

											"	WHERE	pd.member_id			=	?											"

												// DonR 24Nov2024 User Story #366220: Select prior sales of generic
												// equivalents as well as sales of the drug currently being sold.
//											"	  AND	pd.largo_code			=	?											"
											"	  AND	(	pd.largo_code		=	?											"
											"					OR																"
											"				pd.largo_code		IN	(											"
											"											SELECT	largo_code						"
											"											FROM	drug_list						"
											"											WHERE	economypri_group	> 0			"
											"											  AND	economypri_group	= ?			"
											"										)											"
											"			)																		"
											"	  AND	pd.delivered_flg		=  1											"	// DonR 14Jun2010 - Sales only!
											"	  AND	pd.del_flg				=  0											"
											"	  AND	pd.presc_source			<> 0											"	// OTC sales don't count.
											"	  AND	pd.mem_id_extension		=  ?											"

												// DonR 27Nov2024 CODING NOTE: The two compound conditions below
												// (dealing with stop_use_date and whether the previous sale left
												// any doctor prescriptions partially filled) are both disabled
												// for sales that took place today - all those sales will trigger
												// a pharmacy early-refill warning. The condition "pd.date = ?"
												// could be used just once instead of being repeated for each of
												// the two tests - but I'm not sure the result would be any more
												// readable, and the current version allows a little more potential
												// flexibility. So I'm leaving the duplicate condition as it is.

												// DonR 24Nov2024 User Story #366220: *ANY* sale from today will count,
												// regardless of its duration. Also, replaced the hard-coded 25 with a
												// parameter - the maximum duration for a "short-duration" sale will now
												// come from sysparams/max_short_duration_days.
											"	  AND	((pd.date		=	?)								OR					"	// Prior sale happened today...
											"			 (pd.duration	<=	?	AND pd.stop_use_date > ?)	OR					"	// ...or short duration and too much overlap...
											"			 (pd.duration	>	?	AND pd.stop_use_date > ?))						"	// ...or long duration and too much overlap.

												// DonR 24Nov2024 User Story #366220: Because *any* sale of the same drug
												// (or a generic equivalent) from today will trigger the pharmacy warning,
												// this next condition is no longer relevant. Either the date is equal to
												// today and we want to send the message for that reason; or it's not equal
												// to today and the conditon below would have been TRUE anyway.
//											"	  AND	((pd.doctor_id			<> ?)		OR									"	// Either the previous prescription was from a different doctor...
//											"			 (pd.pharmacy_code		<> ?)		OR									"	// ... or it was sold at a different pharmacy...
//											"			 (pd.date				<> ?))											"	// ... or it was sold on a different day.

												// Check if earlier sale was complete for all affected doctor prescriptions.
												// DonR 24Nov2024 User Story #366220: Because *any* sale of the same drug (or
												// a generic equivalent) today should trigger the pharmacy warning, bypass the
												// NOT EXISTS test for prior sales that took place today.
											"	  AND	(	pd.date		=	?													"	// Prior sale happened today...
											"				OR																	"
											"				NOT EXISTS	(	SELECT	*											"	// ...Or didn't completely fill all connected prescriptions.
											"								FROM	pd_rx_link AS RXL							"
											"								WHERE	RXL.prescription_id	=  pd.prescription_id	"
											"								  AND	RXL.line_no			=  pd.line_no			"
											"								  AND	RXL.rx_sold_status	<> 2					"
											"							)														"
											"			)																		";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					NumInputColumns		=	10;
					InputColumnSpec		=	"	int, int, int, short, int, short, int, short, int, int	";	// DonR 03Feb2025 change 2 ints to shorts.
					StatementIsSticky	=	1;	// DonR 25Mar2020.
					Convert_NF_to_zero	=	1;
					break;


// DonR 27Oct2020 start here to check for more statements to make "sticky".
		case READ_test_for_ERR_SPCLTY_LRG_WRN:
					SQL_CommandText		=	"	SELECT	CAST (count(*) AS INTEGER)									"

											"	FROM	spclty_largo_prcnt											"

											"	WHERE	(largo_code			=  ?	OR largo_code = ?)				"

											"	  AND	largo_code			>  0									"

											"	  AND	speciality_code		NOT BETWEEN 58000 AND 58999				"	// Exclude dentist stuff
											"	  AND	speciality_code		NOT BETWEEN  4000 AND  4999				"	// Exclude "home visit" stuff

												  // DonR 212Nov2011 - Add selection criteria for member type.
											"	  AND	(((enabled_mac	> 0) AND (?	> 0))	OR						"
											"			 ((enabled_hova	> 0) AND (?	> 0))	OR						"
											"			 ((enabled_keva	> 0) AND (?	> 0)))							"

											"	  AND	(del_flg		= ? OR del_flg IS NULL)						"

												  // DonR 06Dec2012 "Yahalom" - New insurance-type and "vetek" criteria.
												  // Note that the price code/fixed price combination is now validated
												  // without regard to which insurance is applicable.
											"	  AND	(price_code > 1 OR (price_code = 1 AND fixed_price > 0))	"

											"	  AND	((insurance_type = 'B')						OR				"
											"			 (insurance_type = ? AND ? >= wait_months)	OR				"
											"			 (insurance_type = ? AND ? >= wait_months))					";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					NumInputColumns		=	10;
					InputColumnSpec		=	"	int, int, short, short, short, short, char(1), short, char(1), short	";
					Convert_NF_to_zero	=	1;
					break;


		case READ_test_for_ERR_UNSPEC_ISHUR_DISCNT_AVAIL:
					SQL_CommandText		=	"	SELECT	CAST (count(*) AS INTEGER)									"

											"	FROM	drug_extension												"

											"	WHERE	(largo_code			=  ?	OR largo_code = ?)				"
											"	  AND	largo_code			>  0									"
											"	  AND	rule_number			>  -999									"	// Just to conform to index structure.
											"	  AND	from_age			<= ?									"
											"	  AND	to_age				>= ?									"
											"	  AND	(sex				=  ?		OR sex = 0)					"

											"	  AND	effective_date		<= ?									"
											"	  AND	confirm_authority	>  0									"	// Everything other than "automatic" rules.
											"	  AND	del_flg				=  ?									"

												  // DonR 212Nov2011 - Add selection criteria for member type.
											"	  AND	(((enabled_mac	> 0) AND (?	> 0))	OR						"
											"			 ((enabled_hova	> 0) AND (?	> 0))	OR						"
											"			 ((enabled_keva	> 0) AND (?	> 0)))							"

											"	  AND	send_and_use		<> 0									"

												  // DonR 06Dec2012 "Yahalom" - New insurance-type and "vetek" criteria.
												  // Note that the price code/fixed price combination is now validated
												  // without regard to which insurance is applicable.
											"	  AND	(price_code > 1 OR (price_code = 1 AND fixed_price > 0))	"

											"	  AND	((insurance_type = 'B')						OR				"
											"			 (insurance_type = ? AND ? >= wait_months)	OR				"
											"			 (insurance_type = ? AND ? >= wait_months))					";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					NumInputColumns		=	14;
					InputColumnSpec		=	"	int, int, short, short, short, int, short, short, short,			"
											"	short, char(1), short, char(1), short								";
					StatementIsSticky	=	1;	// DonR 25Mar2020.
					Convert_NF_to_zero	=	1;
					break;


		case READ_test_for_Maccabi_Sheli_or_Zahav_rule:	// DonR 25Feb2020: ODBC (or at least Informix ODBC) wanted a GROUP BY
														// if I SELECTed COUNT(*), insurance_type. Rather than add a meaningless
														// GROUP BY (which I tried, and it worked, but yuck), I now SELECT
														// MAX(insurance_type); it turns out that Informix ODBC has no problem
														// combining aggregate functions in this way, and since we're SELECTing
														// by insurance_type anyway, the output is the same.
					SQL_CommandText		=	"	SELECT	CAST (COUNT(*) AS INTEGER),		MAX(insurance_type)			"

											"	FROM	drug_extension												"

											"	WHERE	(largo_code			=  ?	OR largo_code = ?)				"
											"	  AND	largo_code			>  0									"
											"	  AND	rule_number			>  -999									"	// Just to conform to index structure.
											"	  AND	from_age			<= ?									"
											"	  AND	to_age				>= ?									"
											"	  AND	(sex				=  ?		OR sex = 0)					"

											"	  AND	effective_date		<= ?									"
											"	  AND	confirm_authority	=  0									"	// "Automatic" rules.
											"	  AND	del_flg				=  ?									"

												  // DonR 212Nov2011 - Add selection criteria for member type.
											"	  AND	(((enabled_mac	> 0) AND (?	> 0))	OR						"
											"			 ((enabled_hova	> 0) AND (?	> 0))	OR						"
											"			 ((enabled_keva	> 0) AND (?	> 0)))							"

											"	  AND	send_and_use		<> 0									"

												  // DonR 06Dec2012 "Yahalom" - New insurance-type criteria.
												  // Note that the price code/fixed price combination is now validated
												  // without regard to which insurance is applicable.
											"	  AND	(price_code > 1 OR (price_code = 1 AND fixed_price > 0))	"

											"	  AND	insurance_type		= ?										";
					NumOutputColumns	=	2;
					OutputColumnSpec	=	"	int, char(1)	";
					NumInputColumns		=	11;
					InputColumnSpec		=	"	int, int, short, short, short, int, short, short, short, short, char(1)	";
					StatementIsSticky	=	1;	// DonR 25Mar2020.
					break;


		case READ_test_for_SomeRuleApplies:
					SQL_CommandText		=	"	SELECT	CAST (count(*) AS INTEGER)									"

											"	FROM	drug_extension												"

											"	WHERE	(largo_code			=  ?	OR largo_code = ?)				"
											"	  AND	largo_code			>  0									"
											"	  AND	rule_number			>  -999									"	// Just to make sure we use the right index!
											"	  AND	from_age			<= ?									"
											"	  AND	to_age				>= ?									"
											"	  AND	(sex				=  ?		OR sex = 0)					"
											"	  AND	effective_date		<= ?									"

												  // DonR 212Nov2011 - Add selection criteria for member type.
											"	  AND	(((enabled_mac	> 0) AND (?	> 0))	OR						"
											"			 ((enabled_hova	> 0) AND (?	> 0))	OR						"
											"			 ((enabled_keva	> 0) AND (?	> 0)))							"

												  // DonR 11Dec2012 - Check for valid Price Code/Fixed Price combinations.
											"	  AND	(price_code > 1 OR (price_code = 1 AND fixed_price > 0))	"

											"	  AND	del_flg				=  0									"
											"	  AND	send_and_use		<> 0									";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					NumInputColumns		=	9;
					InputColumnSpec		=	"	int, int, short, short, short, int, short, short, short	";
					StatementIsSticky	=	1;	// DonR 25Mar2020.
					Convert_NF_to_zero	=	1;
					break;


		case ReadDrugPurchaseLim_FindDrugPurchLimit_cur:
					SQL_CommandText		=	"	SELECT		dpl.largo_code,						dpl.class_code,						"
											"				dpl.qty_lim_grp_code,				dpl.limit_method,					"
											"				dpl.max_units,						dpl.ingr_qty_std,					"
											"				dpl.presc_source,					dpl.permit_source,					"

											"				dpl.warning_only,					dpl.ingredient_code,				"
											"				dpl.exempt_diseases,				dpl.duration_months,				"
											"				dpl.history_start_date,				dpl.month_is_28_days,				"
											"				plc.class_type,						plc.class_history_days,				"

											"				dpl.min_prev_purchases,				dpl.custom_error_code,				"
											"				dpl.include_full_price_sales,		dpl.no_ishur_for_pharmacology,		"
											"				dpl.no_ishur_for_treatment_type,	plc.prev_sales_span_days			"

											"	FROM		drug_purchase_lim	as dpl,												"
											"				purchase_lim_class	as plc												"

											"	WHERE		dpl.largo_code		=  ?												"
											"	  AND		dpl.limit_method	BETWEEN 0 AND 8										"	// Just in case someone puts something crazy into the table.
											"	  AND		((dpl.presc_source	=  ?)	OR (dpl.limit_method NOT IN (2,5,6,7,8)))	"	// Prescription Source is relevant only for Method 2/5/6.
											"	  AND		((?					=  0)	OR (dpl.limit_method	 IN (0,1,5,6,7,8)))	"	// DonR 18Dec2017 - support for fallback to "normal" limit if AS/400 Ishur is not applicable.
											"	  AND		plc.class_code		=  dpl.class_code									"
											"	  AND		(plc.class_sex		=  ?	OR plc.class_sex = 0)						"
											"	  AND		plc.class_min_age	<= ?												"
											"	  AND		plc.class_max_age	>= ?												"

											// Marianna 14Sep2023 BUG FIX #480141: Limits are visible for *all* prescription sales, and for
											// *all* limit methods other than the "traditional" 0, 1, 3, and 4 - but those "traditional"
											// limits will *not* be visible for non-prescription sales.
											"	  AND		((?					=  0)	OR (dpl.limit_method NOT IN (0,1,3,4)))		"

											// DonR 17Jun2024 BUG FIX - User Story #326094: Limit methods that specify a particular Prescription
											// Source (i.e. 5, 6, 7, and 8) need to sort *before* the "normal" limit methods.
											"	ORDER BY	CASE dpl.limit_method		"
											"					WHEN  2 THEN	 1		"	// Prescription Source validation - no quantity tests.
											"					WHEN  5 THEN	 2		"	// Limit for specific Prescription Source - test by units bought.
											"					WHEN  6 THEN	 3		"	// Limit for specific Prescription Source - test by ingredient usage.
											"					WHEN  7 THEN	 4		"	// Daily per-pharmacy unit limit for a specific Prescription Source.
											"					WHEN  8 THEN	 5		"	// Daily per-pharmacy ingredient limit for a specific Prescription Source.
											"					WHEN  3 THEN	 6		"	// Using AS/400 ishur dates - test by units bought. Methods 3/4 override other quantity-based methods.
											"					WHEN  4 THEN	 7		"	// Using AS/400 ishur dates - test by ingredient usage.
											"					WHEN  0 THEN	 8		"	// Conventional limit - test by units bought.
											"					WHEN  1 THEN	 9		"	// Conventional limit - test by ingredient usage.
											"					ELSE			99		"
											"				END,						"
											"				plc.class_priority			";
					NumOutputColumns	=	22;	// DonR 16Nov2025 User Story #453336 add exclude-ishur columns plus purchase history span.
					OutputColumnSpec	=	"	int,	short,	int,	short,	int,	double,	short,	short,	"	//  8
											"	short,	short,	short,	short,	int,	short,	short,	int,	"	// 16
											"	short,	short,	bool,	short,	short,	int						";	// 22
					NumInputColumns		=	7;	// Marianna 14Sep2023
					InputColumnSpec		=	"	int, short, short, short, short, short, short	";
					break;


		case ReadDrugPurchaseLim_CheckForDisqualifyingIshur:
					SQL_CommandText		=	"	SELECT	CASE																"
											"			WHEN EXISTS (	SELECT	*											"
											"							FROM	special_prescs	AS sp,						"
											"									drug_list		AS dl						"

											"							WHERE	sp.member_id			=  ?				"
											"							  AND	sp.mem_id_extension		=  ?				"
											"							  AND	sp.confirmation_type	=  1				"
											"							  AND	sp.treatment_start		<= ?				"
											"							  AND	sp.stop_use_date		>= ?				"
//	Disabled, at least for now.				"							  AND	sp.member_price_code	>  0				"	// Ishur gives a discount.
											"							  AND	dl.largo_code			=  sp.largo_code	"
											"							  AND	dl.pharm_group_code		=  ?				"
											"							  AND	dl.treatment_typ_cod	=  ?				"
											"						)														"

											"				THEN 1															"
											"				ELSE 0															"
											"			END																	";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	bool	";
					NumInputColumns		=	6;
					InputColumnSpec		=	"	int, short, int, int, short, short	";
					break;


		case ReadDrugPurchaseLim_FindGroupIngreds_cur:
					SQL_CommandText		=	"	SELECT DISTINCT	(ingredient_code)		"
											"	FROM			drug_purchase_lim		"
											"	WHERE			qty_lim_grp_code	= ?	"
											"	  AND			ingredient_code		> 0	";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	short	";
					NumInputColumns		=	1;
					InputColumnSpec		=	"	int	";
					break;


		case ReadDrugPurchaseLim_READ_Find_DPL_default_values:
					SQL_CommandText		=	"	SELECT {FIRST} 1	limit_method,		warning_only,		"
											"						ingredient_code,	exempt_diseases,	"
											"						qty_lim_grp_code,	month_is_28_days	"

											"	FROM				drug_purchase_lim						"

											"	WHERE				largo_code		=  ?					"
											"	  AND				limit_method	IN (0,1,3,4,5,6,7,8)	"	// Only "normal" quantity limits are relevant here.

											"	ORDER BY			limit_method, class_code				";
					NumOutputColumns	=	6;
					OutputColumnSpec	=	"	short, short, short, short, int, short	";
					NumInputColumns		=	1;
					InputColumnSpec		=	"	int	";
					break;


		case ReadDrugPurchaseLim_READ_ishur_MaxCloseDate:
					SQL_CommandText		=	"	SELECT	MAX(close_date)						"

											"	FROM	purchase_lim_ishur					"

											"	WHERE	member_id		=  ?				"
											"	  AND	largo_code		=  ?				"
											"	  AND	member_id_code	=  ?				"
											"	  AND	del_flg			=  0				"
											"	  AND	((ishur_type	=  6)	OR			"	// 6 = replacement limit
											"			 ((ishur_type	=  7) AND (? = 0)))	"	// 7 = vacation limit
											"	  AND	open_date		<= ?				"
											"	  AND	close_date		<  ?				";	// Ishur is closed.
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					NumInputColumns		=	6;
					InputColumnSpec		=	"	int, int, short, short, int, int	";
					break;


		case ReadDrugPurchaseLim_READ_purchase_lim_ishur:
					SQL_CommandText		=	"	SELECT	max_units,		ingr_qty_std,	duration_months,	"
											"			open_date,		ishur_num,		ishur_type			"

											"	FROM	purchase_lim_ishur									"

											"	WHERE	member_id		=  ?								"
											"	  AND	largo_code		=  ?								"
											"	  AND	member_id_code	=  ?								"
											"	  AND	del_flg			=  0								"
											"	  AND	( (ishur_type	=  6)	OR							"	// 6 = replacement limit
											"			 ((ishur_type	=  7) AND (? = 0)))					"	// 7 = vacation limit
											"	  AND	open_date		<= ?								"
											"	  AND	close_date		>= ?								"	// Ishur is still OK on Close Date.
											"	  AND	((close_date	>= ?) OR (ishur_type <> 7))			";
					NumOutputColumns	=	6;
					OutputColumnSpec	=	"	int, double, short, int, int, short	";
					NumInputColumns		=	7;
					InputColumnSpec		=	"	int, int, short, short, int, int, int	";
					break;


		case SumMemberDrugsUsed_READ_PD_totals:
											// DonR 14May2020: SQL operations returning a SUM seem to be somewhat unstable,
											// so I'm trying an explicit CAST to INTEGER to see if that helps.
					SQL_CommandText		=	"	SELECT	CAST (SUM (units)	AS INTEGER),			"
											"			CAST (SUM (op)		AS INTEGER)				"

											"	FROM	prescription_drugs							"

											"	WHERE	member_id				=  ?				"
											"	  AND	largo_code				=  ?				"
											"	  AND	date					>= ?				"
											"	  AND	mem_id_extension		=  ?				"
											"	  AND	delivered_flg			=  ?				"
											"	  AND	del_flg					=  ?				"
											"	  AND	presc_source			<> 0				"
											"	  AND	{MOD} (particip_method, 10)	IN (1,2,4,6,8)	";
					NumOutputColumns	=	2;
					OutputColumnSpec	=	"	int, int	";
					NumInputColumns		=	6;
					InputColumnSpec		=	"	int, int, int, short, short, short	";
					break;


		case TestMacDoctorDrugsElectronic_READ_PastPurchases:
					SQL_CommandText		=	"	SELECT	CAST (COUNT(*) AS INTEGER)												"

											"	FROM	prescription_drugs	pd,													"
											"			drug_list			dl													"

											"	WHERE	pd.member_id				=  ?										"
											"	  AND	pd.date						>= ?										"

											"	  AND	((pd.largo_code				=  ?)	OR																"
											"			 (pd.largo_code				IN	(	SELECT	DISTINCT largo_code										"
											"												FROM	spclty_drug_grp											"
											"												WHERE	(group_code = ? OR parent_group_code = ?)	)	)	)	"

													// DonR 06Dec2009 - New criteria for qualifying previous sales -
													// 1) Reason for Specialist Participation (the "hundreds" digit)
													//    is between 1 and 5 (excluding dentists, etc.).
													// 2) There was some form of discount given.
													// DonR 04Jun2015 - Previous purchases that got participation
													// from an AS/400 ishur also qualify.
											"	  AND	((pd.particip_method		BETWEEN 100 and 599) OR ({MOD} (pd.particip_method, 10) = 6))				"
											"	  AND	((pd.member_price_code		>  1) OR (pd.price_replace > 0))			"

											"	  AND	pd.delivered_flg			=  1										"	// DonR 14Jun2010 - Sales only!
											"	  AND	pd.del_flg					=  0										"
											"	  AND	pd.mem_id_extension			=  ?										"

											"	  AND	dl.largo_code				=  pd.largo_code							"
											"	  AND	(pd.date					>= ? OR dl.chronic_flag = 1)				";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					NumInputColumns		=	7;
					InputColumnSpec		=	"	int, int, int, int, int, short, int	";
					Convert_NF_to_zero	=	1;
					StatementIsSticky	=	1;	// DonR 28Jun2020.
					break;


		case TestMacDoctorDrugsElectronic_READ_SpecLetters:
					SQL_CommandText		=	"	SELECT	CAST (COUNT(*) AS INTEGER)												"

											"	FROM	doctor_presc															"

											"	WHERE	member_id			=  ?												"
											"	  AND	doctor_presc_id		>  0												"	// Just to conform to index structure.
											"	  AND	valid_from_date		>  0												"	// Just to conform to index structure.

											"	  AND	((largo_prescribed	=  ?)	OR																"
											"			 (largo_prescribed	IN (	SELECT	DISTINCT largo_code										"
											"										FROM	spclty_drug_grp											"
											"										WHERE	(group_code = ? OR parent_group_code = ?)	)	)	)	"

											// DonR 27Mar2017 - Ishur must have come from current prescribing doctor.
											"	  AND	doctor_id			= ?													"
											"	  AND	member_id_code		= ?													"

												  // DonR 27Mar2017 - Ishur must have been issued on the same visit date as the current doctor prescription was issued.
												  // Note that for this to work properly, we need to have a valid Doctor Prescription ID (which we should have if we're
												  // dealing with Transaction 2003/5003) or a valid Visit Number (which we get in Transaction 6003.
											"	  AND	visit_date			= (	SELECT		MAX(visit_date)						"
											"									FROM		doctor_presc						"
											"									WHERE		member_id			= ?				"
											"									  AND		((doctor_presc_id	= ?) OR (? = 0))"
											"									  AND		valid_from_date		> 0				"	// Just to conform to index structure.
											"									  AND		largo_prescribed	> 0				"	// Just to conform to index structure.
											"									  AND		doctor_id			= ?				"
											"									  AND		member_id_code		= ?				"
											"									  AND		((visit_number		= ?) OR (? = 0))	)	"

											"	  AND	specialist_ishur	> 0													";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					NumInputColumns		=	13;
					InputColumnSpec		=	"	int, int, int, int, int, short, int, int, int, int, short, long, long	";
					Convert_NF_to_zero	=	1;
					break;


		case TestMacDoctorDrugsElectronic_SpecialistPtn_1_cur:
					SQL_CommandText		=	"	SELECT	price_code,		fixed_price,	insurance_type,		health_basket_new	"
					
											"	FROM	doctor_speciality	ds,													"
											"			spclty_largo_prcnt	lp													"

											"	WHERE	ds.doctor_id		= ?													"
											"	  AND	ds.speciality_code	NOT BETWEEN 58000 AND 58999							"	// Exclude dentist stuff
											"	  AND	ds.speciality_code	NOT BETWEEN  4000 AND  4999							"	// Exclude "home visit" stuff
											"	  AND	ds.del_flg			= ?													"
											"	  AND	lp.speciality_code	= ds.speciality_code								"
											"	  AND	lp.largo_code		= ?													"

												  // DonR 212Nov2011 - Add selection criteria for member type.
											"	  AND	(((enabled_mac	> 0) AND (?	> 0))	OR									"
											"			 ((enabled_hova	> 0) AND (?	> 0))	OR									"
											"			 ((enabled_keva	> 0) AND (?	> 0)))										"

											"	  AND	lp.del_flg			= ?													"

												  // DonR 09Dec2012 "Yahalom" - New insurance-type and "vetek" criteria.
												  // Note that the price code/fixed price combination is now validated
												  // without regard to which insurance is applicable.
											"	  AND	(price_code > 1 OR (price_code = 1 AND fixed_price > 0))				"

											"	  AND	((insurance_type = 'B')						OR							"
											"			 (insurance_type = ? AND ? >= wait_months)	OR							"
											"			 (insurance_type = ? AND ? >= wait_months))								"
												  // DonR 06Dec2012 "Yahalom" end.

												// DonR 21Oct2015 - Give best ptn within member's best insurance.
											"	ORDER BY	ins_type_sort DESC, ptn_percent_sort, fixed_price					";
					NumOutputColumns	=	4;
					OutputColumnSpec	=	"	short,int, char(1), short	";
					NumInputColumns		=	11;
					InputColumnSpec		=	"	int, short, int, short, short, short, short, char(1), short, char(1), short	";
					StatementIsSticky	=	1;	// DonR 28Jun2020.
					break;


		case TestMacDoctorDrugsElectronic_SpecialistPtn_2_cur:
					SQL_CommandText		=	"	SELECT	price_code,			fixed_price,		insurance_type,					"
											"			health_basket_new,	speciality_code										"
					
											"	FROM	spclty_largo_prcnt														"

												// Are we correct in assuming that pharmacy isn't allowed to give an ishur
												// specifying a dentist or home-visit Speciality Code?
											"	WHERE	largo_code		= ?														"
											"	  AND	speciality_code	NOT BETWEEN 58000 AND 58999								"	// Exclude dentist stuff
											"	  AND	speciality_code	NOT BETWEEN  4000 AND  4999								"	// Exclude "home visit" stuff

												  // DonR 212Nov2011 - Add selection criteria for member type.
											"	  AND	(((enabled_mac	> 0) AND (?	> 0))	OR									"
											"			 ((enabled_hova	> 0) AND (?	> 0))	OR									"
											"			 ((enabled_keva	> 0) AND (?	> 0)))										"

											"	  AND	del_flg			= ?														"

												  // DonR 09Dec2012 "Yahalom" - New insurance-type and "vetek" criteria.
												  // Note that the price code/fixed price combination is now validated
												  // without regard to which insurance is applicable.
											"	  AND	(price_code > 1 OR (price_code = 1 AND fixed_price > 0))				"

											"	  AND	((insurance_type = 'B')						OR							"
											"			 (insurance_type = ? AND ? >= wait_months)	OR							"
											"			 (insurance_type = ? AND ? >= wait_months))								"
												  // DonR 06Dec2012 "Yahalom" end.

												// DonR 21Oct2015 - Give best ptn within member's best insurance.
											"	ORDER BY	ins_type_sort DESC, ptn_percent_sort, fixed_price					";
					NumOutputColumns	=	5;
					OutputColumnSpec	=	"	short, int, char(1), short, int	";
					NumInputColumns		=	9;
					InputColumnSpec		=	"	int, short, short, short, short, char(1), short, char(1), short	";
					StatementIsSticky	=	1;	// DonR 28Jun2020.
					break;


		case TestMacDoctorDrugsElectronic_SpecialistPtn_3_cur:
					SQL_CommandText		=	"	SELECT	price_code,		fixed_price,	insurance_type,		health_basket_new	"
					
											"	FROM	spclty_largo_prcnt														"

												// Are we correct in assuming that pharmacy isn't allowed to give an ishur
												// specifying a dentist or home-visit Speciality Code?
											"	WHERE	speciality_code	= ?														"
											"	  AND	speciality_code	NOT BETWEEN 58000 AND 58999								"	// Exclude dentist stuff
											"	  AND	speciality_code	NOT BETWEEN  4000 AND  4999								"	// Exclude "home visit" stuff
											"	  AND	largo_code		= ?														"

												  // DonR 212Nov2011 - Add selection criteria for member type.
											"	  AND	(((enabled_mac	> 0) AND (?	> 0))	OR									"
											"			 ((enabled_hova	> 0) AND (?	> 0))	OR									"
											"			 ((enabled_keva	> 0) AND (?	> 0)))										"

											"	  AND	del_flg			= ?														"

												  // DonR 09Dec2012 "Yahalom" - New insurance-type and "vetek" criteria.
												  // Note that the price code/fixed price combination is now validated
												  // without regard to which insurance is applicable.
											"	  AND	(price_code > 1 OR (price_code = 1 AND fixed_price > 0))				"

											"	  AND	((insurance_type = 'B')						OR							"
											"			 (insurance_type = ? AND ? >= wait_months)	OR							"
											"			 (insurance_type = ? AND ? >= wait_months))								"
												  // DonR 06Dec2012 "Yahalom" end.

												// DonR 21Oct2015 - Give best ptn within member's best insurance.
											"	ORDER BY	ins_type_sort DESC, ptn_percent_sort, fixed_price					";
					NumOutputColumns	=	4;
					OutputColumnSpec	=	"	short,int, char(1), short	";
					NumInputColumns		=	10;
					InputColumnSpec		=	"	int, int, short, short, short, short, char(1), short, char(1), short	";
					break;


		case TestPharmacyIshur_READ_pharmacy_ishur:
					SQL_CommandText		=	"	SELECT	member_price_code,		const_sum_to_pay,	insurance_used,		"
											"			rule_number,			error_code,			treatment_category,	"
											"			rrn,					in_health_pack,		no_presc_sale_flag,	"
											"			needs_29_g														"

											"	FROM	pharmacy_ishur													"
											"	WHERE	%s																";
					NumOutputColumns	=	10;
					OutputColumnSpec	=	"	short, int, short, int, short, short, int, short, short, short	";
					NeedsWhereClauseID	=	1;
					break;


		case TestPharmacyIshur_UPD_pharmacy_ishur_MarkUsedOnly:
					SQL_CommandText		=	"	UPDATE	pharmacy_ishur				"

											"	SET		used_flg			= ?,	"
											"			used_pr_id			= ?,	"
											"			used_pr_line_num	= ?,	"
											"			updated_date		= ?,	"
											"			updated_time		= ?,	"
											"			updated_by_trn		= ?,	"
											"			reported_to_as400	= ?		"

											"	WHERE	rrn					= ?		";
					NumInputColumns		=	8;
					InputColumnSpec		=	"	short, int, short, int, int, short, short, int	";
					break;


		case TestPharmacyIshur_UPD_pharmacy_ishur_specialist:
					SQL_CommandText		=	"	UPDATE	pharmacy_ishur				"

											"	SET		used_flg			= ?,	"
											"			used_pr_id			= ?,	"
											"			used_pr_line_num	= ?,	"
											"			member_price_code	= ?,	"
											"			const_sum_to_pay	= ?,	"
											"			insurance_used		= ?,	"
											"			in_health_pack		= ?,	"
											"			speciality_code		= ?,	"
											"			updated_date		= ?,	"
											"			updated_time		= ?,	"
											"			updated_by_trn		= ?,	"
											"			reported_to_as400	= ?		"

											"	WHERE	rrn					= ?		";
					NumInputColumns		=	13;
					InputColumnSpec		=	"	short, int, short, short, int, short, short, int, int, int, short, short, int	";
					break;


		case TestSpecialDrugs_SLP_cur:
					SQL_CommandText		=	"	SELECT	price_code,		fixed_price,	insurance_type,		health_basket_new	"
					
											"	FROM	spclty_largo_prcnt														"

											"	WHERE	largo_code		=		?												"
											"	  AND	del_flg			=		?												"

												  // DonR 212Nov2011 - Add selection criteria for member type.
											"	  AND	(((enabled_mac	> 0) AND (?	> 0))	OR									"
											"			 ((enabled_hova	> 0) AND (?	> 0))	OR									"
											"			 ((enabled_keva	> 0) AND (?	> 0)))										"

											"	  AND	speciality_code BETWEEN	? AND ?											"

												  // DonR 23Nov2011 - Added insurance-type criteria to SELECT.
												  // DonR 09Dec2012 "Yahalom" - New insurance-type and "vetek" criteria.
												  // Note that the price code/fixed price combination is now validated
												  // without regard to which insurance is applicable.
											"	  AND	(price_code > 1 OR (price_code = 1 AND fixed_price > 0))				"

											"	  AND	((insurance_type = 'B')						OR							"
											"			 (insurance_type = ? AND ? >= wait_months)	OR							"
											"			 (insurance_type = ? AND ? >= wait_months))								"
												  // DonR 06Dec2012 "Yahalom" end.

												// DonR 21Oct2015 - Give best ptn within member's best insurance.
											"	ORDER BY	ins_type_sort DESC, ptn_percent_sort, fixed_price					";
					NumOutputColumns	=	4;
					OutputColumnSpec	=	"	short, int, char(1), short	";
					NumInputColumns		=	11;
					InputColumnSpec		=	"	int, short, short, short, short, int, int, char(1), short, char(1), short	";
					break;


// DonR 16Jan2020 CR #31688: We no longer work with the force_new_ql_codes table; instead we use drug_list/tight_ishur_limits,
// which gets its value from RK9001P/C0AJSS.
// DonR 31Mar2020: May need to re-enable if Raya Karol isn't ready when the ODBC pilot begins.
//		case TestSpecialPresc_READ_force_new_ql_codes:
//					SQL_CommandText		=	"	SELECT COUNT(*) FROM force_new_ql_codes WHERE (parent_group_code = ?) AND (enabled_flag > 0)	";
//					NumOutputColumns	=	1;
//					OutputColumnSpec	=	"	int	";
//					NumInputColumns		=	1;
//					InputColumnSpec		=	"	int, short,	";
//					break;
//
//
		case TestSpecialPresc_READ_prescription_usage:
					SQL_CommandText		=	"	SELECT	net_lim_ingr_used		"
											"	FROM	prescription_usage		"
											"	WHERE	prescription_id	= ?		"
											"	  AND	line_no			= ?		";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	double	";
					NumInputColumns		=	2;
					InputColumnSpec		=	"	int, short	";
					break;


		case TestSpecialPresc_READ_special_prescs:
					SQL_CommandText		=	"	SELECT	{TOP} 1																"
											"			pharmacy_code,			largo_code,				member_price_code,	"
											"			priv_pharm_sale_ok,		treatment_start,		stop_use_date,		"
											"			const_sum_to_pay,		spec_pres_num_sors,		qty_lim_per_course,	"
											"			special_presc_num,		health_basket_new,		treatment_category,	"
											"			insurance_used,			abrupt_close_flg,		qty_lim_ingr,		"
											"			qty_lim_flg,			qty_lim_course_len,		qty_lim_all_ishur,	"

											"			qty_lim_all_warn,		qty_lim_all_err,		qty_lim_course_wrn,	"
											"			qty_lim_course_err,		tikra_flag,				tikra_type_code,	"
											"			ishur_source_send,		qlm_history_start,		qlm_all_ishur_std,	"
											"			qlm_all_warn_std,		qlm_all_err_std,		qlm_per_course_std,	"
											"			qlm_course_wrn_std,		qlm_course_err_std,		qty_lim_per_day,	"
											"			qlm_per_day_std,		qty_lim_treat_days,		needs_29_g,			"

											"			monthly_qlm_flag,		monthly_dosage,			monthly_dosage_std,	"	// Marianna 06Aug2020 
											"			monthly_duration,		cont_approval_flag,		orig_confirm_num,	"	// Marianna 06Aug2020
											"			exceptional_ishur													"	// Marianna 22Jan2023 User Story #276372

											"	FROM	special_prescs														"
											"	WHERE	member_id			=  ?	"
											"	  AND	mem_id_extension	=  ?	"
											"	  AND	largo_code			=  ?	"
											"	  AND	confirmation_type	=  1	"	// DonR 01Jul2004 - Ishur OK to use.
											"	  AND	treatment_start		<= ?	"
											"	  AND	member_price_code	>  0	"	// DonR 12Sep2012 - Ishur tells us something about pricing.
											"	  AND	stop_use_date		>= ?	"

											// 07Jul2011 - No extension allowed for automatic ishurim.
											"	  AND	(stop_use_date		>= ?	OR	spec_pres_num_sors <> 5)			"

//											// Get the best current row before any expired rows.
											// Marianna 25Jan2023 User Story #276372: "Exceptional" ishurim act as temporary overrides
											// to regular ishurim, e.g. in case the member needs to replace something that was lost
											// or broken. Accordingly, we want to see any applicable "exceptional" ishur as the first
											// item selected.
											// DonR 26Jan2023: Other than "exceptional" ishurim, there *should* never be more than one
											// current, valid ishur in the table for any given Member ID/Largo Code. However, there are
											// bugs in the AS/400 routine supplying updates to RK9002P, so that sometimes we do have
											// more than one valid ishur. In this case, the best we can do is to take the most recently
											// updated one, in the hope that it's the correct one.
//											"	ORDER BY stop_use_date DESC													";
											"	ORDER BY exceptional_ishur DESC, date DESC , time DESC		"; // Marianna 25Jan2023 :   User Story #276372
					NumOutputColumns	=	43;											// Marianna 06Aug2020 CR #28605/27955: added 6 columns.
					OutputColumnSpec	=	"			int,					int,					short,				"
											"			short,					int,					int,				"
											"			int,					short,					double,				"
											"			int,					short,					short,				"
											"			short,					short,					short,				"
											"			short,					short,					double,				"

											"			double,					double,					double,				"
											"			double,					short,					short,				"
											"			short,					int,					double,				"
											"			double,					double,					double,				"
											"			double,					double,					double,				"
											"			double,					short,					short,				"

											"			short,					double,					double,				"	// Marianna 06Aug2020 	
											"			short,					short,					int,				"	// Marianna 06Aug2020
											"			short																";	// Marianna 22Jan2023 User Story #276372
					NumInputColumns		=	6;
					InputColumnSpec		=	"	int, short, int, int, int, int	";
					StatementIsSticky	=	1; // Disabled 01Apr2020; re-enabled 06Apr2020.
					break;


		case TestSpecialPresc_READ_PreviousIshurDate:									// Marianna 06Apr2020 CR #28605: New SQL query.
					SQL_CommandText		=   "	SELECT	treatment_start				" 
											"	FROM	special_prescs				"
											
											"   WHERE  	member_id			= ?		"
											"	  AND  	largo_code			= ?		"
											"     AND 	special_presc_num	= ?		"
											"     AND 	mem_id_extension	= ?		"
											"	  AND	treatment_start		> 0		";	// DonR 11Aug2020: Added "sanity check" so we don't bother
																						// reading ishurim with bogus Treatment Start Dates.
					NumOutputColumns     = 	1;
					OutputColumnSpec     =  "   int						";
					NumInputColumns      =  4;
					InputColumnSpec      =  "	int, int, int, short	";
					break;


		case UPD_messages_details:
					SQL_CommandText		=	"	UPDATE	messages_details		"
											"	SET		pharmacy_code	= ?,	"
											"			terminal_id		= ?,	"
											"			member_id		= ?,	"
											"			member_id_ext	= ?,	"
											"			spool_flg		= ?,	"	// Shouldn't really be necessary to update.
											"			ok_flg			= ?,	"
											"			proc_time		= ?,	"
											"			num_processed	= ?,	"
											"			error_code		= ?		"
											"	WHERE	rec_id			= ?		";
					NumInputColumns		=	10;
					InputColumnSpec		=	"	int, short, int, short, short, short, int, int, short, int	";
					StatementIsSticky	=	1;	// DonR 26Mar2020.
					break;


		case UPD_online_orders_RecordSaleAgainstOrder:
					SQL_CommandText		=	"	UPDATE	online_orders				"
											"	SET		last_sold_pr_id		= ?,	"
											"			last_sold_pharmacy	= ?,	"
											"			last_sold_date		= ?,	"
											"			last_sold_time		= ?		"

											"	WHERE	online_order_num	= ?		"
											"	  AND	made_by_pharmacy	= ?		"
											"	  AND	made_by_owner		= ?		";
					NumInputColumns		=	7;
					InputColumnSpec		=	"	int, int, int, int, long, int, short	";
					break;


		case UPD_online_orders_StartWorkTimestamp:
					SQL_CommandText		=	"	UPDATE	online_orders				"
											"	SET		start_work_date		= ?,	"
											"			start_work_time		= ?		"
											"	WHERE	online_order_num	= ?		"
											"	  AND	made_by_pharmacy	= ?		"
											"	  AND	made_by_owner		= ?		";
					NumInputColumns		=	5;
					InputColumnSpec		=	"	int, int, long, int, short	";
					break;


		case UPD_pharmacy_daily_sum:
												// DonR 07Jun2020: Made sticky.
					SQL_CommandText		=	"	UPDATE	pharmacy_daily_sum									"

											"	SET		sum					= sum					+ ?,	"
											"			member_part_sum		= member_part_sum		+ ?,	"
											"			purchase_sum		= purchase_sum			+ ?,	"
											"			presc_count			= presc_count			+ ?,	"
											"			presc_lines_count	= presc_lines_count		+ ?,	"

											"			delete_sum			= delete_sum			+ ?,	"
											"			del_member_prt_sum	= del_member_prt_sum	+ ?,	"
											"			delete_lines		= delete_lines			+ ?,	"
											"			deleted_sales		= deleted_sales			+ ?,	"
											"			del_purchase_sum	= del_purchase_sum		+ ?		"

											"	WHERE	pharmacy_code	= ?									"
											"	  AND	institued_code	= ?									"
											"	  AND	diary_month		= ?									"
											"	  AND	date			= ?									"
											"	  AND	terminal_id		= ?									";
					NumInputColumns		=	15;
					InputColumnSpec		=	"	int, int, int, int, int, int, int, int, int, int, int, short, short, int, short	";
					StatementIsSticky	=	1;
					break;


		case UPD_pharmacy_daily_sum_from_prior_sale:
											// NOTE: At least for now, this query relies on drug_list/package_size not
											// having any zero values. That is the case at present, and it always
											// *should* be the case - but if we ever see someone sending us a drug_list
											// update with package_size = 0, we should add "CASE" logic to this statement
											// to substitute a package size of 1 so we don't try to divide by zero.
											// Note also that the subquery that selects values from prescriptions and
											// prescription_drugs will include *only* prescription_drugs rows with
											// paid_for set FALSE (which should mean that credit_type_used = 99, payment
											// type not yet determined). This means that this query will *not* add amounts
											// to pharmacy_daily_sum that have already been added, even if they are
											// included in the list of prior sales that's sent in Transaction 6005.
											// (Remember also that credit_type_used/paid_for are set on the transaction
											// level - so they should always be the same in prescriptions and in all the
											// associated prescription_drugs rows.)
											// DonR 22Feb2024 BUG FIX: totl_supplier_price was spelled inconsistently,
											// causing an SQL execution error.
					SQL_CommandText		=	"	UPDATE	pharmacy_daily_sum																"

											"	SET		sum					= sum				+ totl_drug_price,						"

											"			member_part_sum		= member_part_sum											"
											"									+ totl_member_ptn										"
											"									- (totl_tikra_discount + totl_shovar_discount),			"

											"			purchase_sum		= purchase_sum		+ totl_supplier_price,					"
											"			presc_count			= presc_count		+ 1,									"
											"			presc_lines_count	= presc_lines_count	+ num_pd_rows							"

											"	FROM	(	SELECT	SUM (member_ptn_amt)		AS totl_member_ptn,						"
											"						SUM (total_drug_price)		AS totl_drug_price,						"

											"						SUM (CAST (	CAST (price_for_op_ph AS FLOAT)		*					"
											"									(CAST(op AS FLOAT)										"
											"								  +	 (CAST(units AS FLOAT) / CAST(package_size AS FLOAT)))	"
											"									AS INTEGER	) )	AS totl_supplier_price,					"

											"						COUNT (*)					AS num_pd_rows,							"

																	// Since Tikra Discount and Subsidy Amount (shovarim) come from
																	// prescription_drugs, there will only be one of each in the
																	// SELECT. However, if we just leave them as simple variables,
																	// SQL Server will throw an error. Accordingly, we have to SUM
																	// them just so that we're consistent in SELECTing only aggregate
																	// functions in this subquery.
											"						SUM (tikra_discount)		AS totl_tikra_discount,					"
											"						SUM (subsidy_amount)		AS totl_shovar_discount					"

											"				FROM	prescription_drugs	AS pd,											"
											"						prescriptions		AS pr,											"
											"						drug_list			AS dl											"

												// Two input params for data source: Prescription ID and Mac Pharmacy.
											"				WHERE	pd.prescription_id		= ?											"
																									// Maccabi pharmacy
											"				  AND	(pd.presc_source		> 0 OR	? > 0)								"
											"				  AND	pd.paid_for				= 0											"
											"				  AND	pr.prescription_id		= pd.prescription_id						"
											"				  AND	dl.largo_code			= pd.largo_code								"

											"			)	AS tmp																		"

												// Five input parameters to identify the
												// pharmacy_daily_sum row to update.
											"	WHERE	pharmacy_code	= ?																"
											"	  AND	institued_code	= ?																"
											"	  AND	diary_month		= ?																"
											"	  AND	date			= ?																"
											"	  AND	terminal_id		= ?																"
											"	  AND	num_pd_rows		> 0																";
					NumInputColumns		=	7;
					InputColumnSpec		=	"	int, short, int, short, short, int, short	";
					StatementIsSticky	=	1;
					break;


		case UPD_prescriptions_mark_paid:
					// NOTE: Updating reported_to_as400 here should be redundant, but harmless.
					// The "paid_for = 0" condition should prevent spooled 6005's from resetting
					// the reported_to_as400 flag more than once even if the transaction is sent
					// multiple times by the pharmacy.
					// DonR 04Mar2024: Update Payment Method as well.
					SQL_CommandText		=	"	UPDATE	prescriptions				"
											"	SET		credit_type_used	= ?,	"
											"			paid_for			= 1,	"
											"			payment_method		= ?,	"
											"			reported_to_as400	= 0		"
											"	WHERE	prescription_id		= ?		"
											"	  AND	paid_for			= 0		";
					NumInputColumns		=	3;
					InputColumnSpec		=	"	short, short, int	";
					StatementIsSticky	=	1;
					break;


		case UPD_prescription_drugs_mark_paid:
					// NOTE: Updating reported_to_as400 here should be redundant, but harmless.
					// The "paid_for = 0" condition should prevent spooled 6005's from resetting
					// the reported_to_as400 flag more than once even if the transaction is sent
					// multiple times by the pharmacy.
					SQL_CommandText		=	"	UPDATE	prescription_drugs			"
											"	SET		credit_type_used	= ?,	"
											"			paid_for			= 1,	"
											"			reported_to_as400	= 0		"
											"	WHERE	prescription_id		= ?		"
											"	  AND	paid_for			= 0		";
					NumInputColumns		=	2;
					InputColumnSpec		=	"	short, int	";
					StatementIsSticky	=	1;
					break;


		case UPD_prescription_msgs_mark_delivered:
												// DonR 07Jun2020: Made sticky.
					SQL_CommandText		=	"	UPDATE	prescription_msgs			"
											"	SET		date				= ?,	"
											"			time				= ?,	"
											"			delivered_flg		= ?,	"
											"			reported_to_as400	= ?		"
											"	WHERE	prescription_id		= ?		";
					NumInputColumns		=	5;
					InputColumnSpec		=	"	int, int, short, short, int	";
					StatementIsSticky	=	1;
					break;


		case UPD_prescription_usage_mark_delivered:
					SQL_CommandText		=	"	UPDATE	prescription_usage			"
											"	SET		date				= ?,	"
											"			time				= ?,	"
											"			delivered_flg		= ?,	"
											"			reported_to_as400	= ?		"
											"	WHERE	prescription_id		= ?		";
					NumInputColumns		=	5;
					InputColumnSpec		=	"	int, int, short, short, int	";
					break;


		case UPD_prescriptions_set_error_code:
					SQL_CommandText		=	"	UPDATE prescriptions SET error_code = ? WHERE prescription_id = ?	";
					NumInputColumns		=	2;
					InputColumnSpec		=	"	short, int	";
					break;


		case UPD_SetPharmacyOpenStatus:
					SQL_CommandText		=	"	UPDATE	PHARMACY				"
											"	SET		open_close_flag	= ?		"
											"	WHERE	pharmacy_code	= ?		"
											"	  AND	del_flg			= ?		";
					NumInputColumns		=	3;
					InputColumnSpec		=	"	short, int, short	";
					break;


		case UPD_sql_srv_audit_InProgress:
					SQL_CommandText		=	"	UPDATE	sql_srv_audit			"
											"	SET		date			= ?,	"
											"			time			= ?,	"
											"			computer_id		= ?,	"
											"			msg_id			= ?,	"
											"			in_progress		= 1,	"
											"			msg_rec_id		= ?,	"
											"			msg_len			= ?,	"
											"			err_code		= 0,	"
											"			msg_count		= ?		"
											"	WHERE	pid				= ?		";
					NumInputColumns		=	8;
					InputColumnSpec		=	"	int, int, char(2), short, int, int, int, int	";
					StatementIsSticky	=	1;	// DonR 26Mar2020.
					break;


		case UPD_sql_srv_audit_NotInProgress:
					SQL_CommandText		=	"	UPDATE	sql_srv_audit		"
											"	SET		in_progress	= 0,	"
											"			err_code	= ?		"
											"	WHERE	pid			= ?		";
					NumInputColumns		=	2;
					InputColumnSpec		=	"	short, int	";	// Err_code is defined as an INTEGER, but it's fed from a short and should really be a SMALLINT.
					StatementIsSticky	=	1;	// DonR 26Mar2020.
					break;


		case UPD_sql_srv_audit_ProgramEnd:
					SQL_CommandText		=	"	UPDATE	sql_srv_audit		"
											"	SET		date		= ?,	"
											"			time		= ?,	"
											"			in_progress	= ?		"
											"	WHERE	pid			= ?		";
					NumInputColumns		=	4;
					InputColumnSpec		=	"	int, int, short, int	";
					break;


		case UPD_sysparams_TikrotEnabled:
					SQL_CommandText		=	"	UPDATE sysparams SET nihultikrotenabled = ? WHERE nihultikrotenabled <> ?	";
					NumInputColumns		=	2;
					InputColumnSpec		=	"	short,short";
					break;


		case UPD_members_ClearAuthorizeAlways:
					SQL_CommandText		=	"	UPDATE	members						"
											"	SET		AuthorizeAlways		= 0,	"
											"			updated_by			= ?		"
											"	WHERE	member_id			= ?		"
											"	  AND	mem_id_extension	= ?		";
					NumInputColumns		=	3;
					InputColumnSpec		=	"	int, int, short	";
					CommandIsMirrored	=	0;	// DonR 29Jul2025 - Turning this off, as we're pretty much letting Informix die.
					break;


		// DonR 11Jul2022: Temporarily (?) disabling mirroring on this command, since we've been experiencing locking problems
		// in Informix and this is the only SQL command in SqlServer.exe that's been sensitive to the problem. I don't believe
		// that max_drug_date is particularly important to the old Doctors system, so they'll just have to live without this
		// UPDATE.
		case UPD_members_max_drug_date:
					SQL_CommandText		=	"	UPDATE members SET max_drug_date = ? WHERE member_id = ? AND mem_id_extension = ?	";
					NumInputColumns		=	3;
					InputColumnSpec		=	"	int, int, short	";
//					CommandIsMirrored	=	1;	// DonR 21Feb2021.
					break;


		// Interaction/Overdose routines.
		case IntOD_max_units:
					SQL_CommandText		=	"	SELECT		{FIRST} 1	max_units_per_day, from_age		"
											"	FROM		over_dose									"
											"	WHERE		largo_code = ? AND from_age <= ?			"
											"	ORDER BY	from_age DESC								";
					NumOutputColumns	=	2;
					OutputColumnSpec	=	"	int, short	";
					NumInputColumns		=	2;
					InputColumnSpec		=	"	int, short	";
					StatementIsSticky	=	1;
					break;


		case UpdateDoctorPresc_INS_drx_status_queue:
					SQL_CommandText		=	"	INSERT INTO	drx_status_queue														"
											"	(																					"
											"		member_id,			member_id_code,		doctor_id,			visit_number,		"
											"		doctor_presc_id,	line_number,		largo_prescribed,	valid_from_date,	"
											"		sold_status,		close_by_rounding,	upd_units_sold,		upd_op_sold,		"
											"		last_sold_date,		last_sold_time,		linux_update_flag,	linux_update_date,	"
											"		linux_update_time,	queued_date,		queued_time,		reported_to_cds		"
											"	)																					";
					NumInputColumns		=	20;
					InputColumnSpec		=	"	int, short, int, long, int, short, int, int, short, short,	"
											"	short, short, int, int, short, int, int, int, int, short	";
					GenerateVALUES		=	1;
					break;


		case UpdateDoctorPresc_INS_LateArrivalQ:
					SQL_CommandText		=	"	INSERT INTO	rx_late_arrival_q										"
											"	(																	"
											"		ActionCode_in,		Member_ID_in,			Member_ID_Code_in,	"
											"		PrescID_in,			LineNo_in,									"
											"		DocID_in,			DocIDType_in,								"
											"		DocPrID_in,			LargoPrescribed_in,		LargoSold_in,		"
											"		ValidFromDate_in,	SoldDate_in,			SoldTime_in,		"
											"		OP_sold_in,			UnitsSold_in,			PrevUnsoldOP_in,	"
											"		PrevUnsoldUnits_in,	sale_timestamp,			next_proc_time		"
											"	)																	";
					NumInputColumns		=	19;
					InputColumnSpec		=	"	short, int, short, int, short, int, short, int, int, int,			"
											"	int, int, int, short, short, short, short, long, long				";
					GenerateVALUES		=	1;
					break;


		case UpdateDoctorPresc_INS_pd_rx_link:	// NOTE: OP Sold and Units Sold (for both prescribed and sold drugs) as
												// well as Previous Unsold OP/Units are integers in pd_rx_link, but
												// short integers in other tables. Sending the ODBC routine the address
												// of a short integer gave strange values in the table, since ODBC pulled
												// 32 bits from an address that had only 16 bits of meaningful data. It
												// turns out that changing the Input Column Spec to *expect* a short
												// integer does the trick - the variable gets mapped correctly, and ODBC
												// automatically takes care of any necessary conversion. This is worth
												// noting for other situations where the saved data has different types
												// in different tables - and the key thing to remember is that the data
												// type in the column specification *must* match the incoming variable's
												// data type.
												// DonR 17May2020: The implicit type conversion seems to be throwing errors
												// when a drug sale is deleted - so instead, I'm copying the short integers
												// to regular integers (for sold OP/units for original and sold drug) and
												// changing the column types here to match the Informix database.
												// DonR 07Jun2020: Made sticky.
												// DonR 01Aug2023 User Story #469361: Added economypri_group, prescription_type,
												// and del_flg to pd_rx_link to enable fast data retrieval without joins to
												// prescription_drugs, drug_list, and doctor_presc.
												// DonR 11Dec2024 User Story #373619: Add rx_origin to pd_rx_link columns inserted.
					SQL_CommandText		=	"	INSERT INTO	pd_rx_link												"
											"	(																	"
											"		prescription_id,	line_no,				member_id,			"
											"		member_id_code,		doctor_id,				doctor_presc_id,	"
											"		largo_prescribed,	largo_sold,				valid_from_date,	"
											"		op_sold,			units_sold,				close_by_rounding,	"

											"		sold_drug_op,		sold_drug_units,		rx_sold_status,		"
											"		sold_date,			prev_unsold_op,			prev_unsold_units,	"
											"		visit_number,		clicks_line_number,		visit_date,			"
											"		date,				time,					reported_to_as400,	"

											"		economypri_group,	prescription_type,		del_flg,			"
											"		rx_origin														"
											"	)																	";
					NumInputColumns		=	28;
					InputColumnSpec		=	"	int, short, int, short, int, int, int, int, int, int, int, short,	"
											"	int, int, short, int, int, int, long, short, int, int, int, short	"
											"	int, short, short, short											";
					GenerateVALUES		=	1;
					StatementIsSticky	=	1;
					break;


		case UpdateDoctorPresc_INS_rx_status_changes:
												// DonR 07Jun2020: Made sticky.
					SQL_CommandText		=	"	INSERT INTO	rx_status_changes									"
											"	(																"
											"		clicks_patient_id,	doctor_presc_id,	visit_number,		"
											"		line_number,		largo_prescribed,	prev_sold_status,	"
											"		sold_status,		insert_date,		insert_time			"
											"	)																";
					NumInputColumns		=	9;
					InputColumnSpec		=	"	int, int, long, short, int, short, short, int, int	";
					GenerateVALUES		=	1;
					StatementIsSticky	=	1;
					break;


		case UpdateDoctorPresc_READ_CountLateArrivalQ:
					SQL_CommandText		=	"	SELECT	CAST (COUNT(*) AS INTEGER)	"

											"	FROM	rx_late_arrival_q			"

											"	WHERE	prescid_in			= ?		"
											"	  AND	rx_found			= 0		"
											"	  AND	largoprescribed_in	= ?		"
											"	  AND	docprid_in			= ?		"
											"	  AND	docid_in			= ?		";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					NumInputColumns		=	4;
					InputColumnSpec		=	"	int, int, int, int	";
					Convert_NF_to_zero	=	1;
					break;


		case UpdateDoctorPresc_READ_Rx:		// NO LONGER IN USE!
					SQL_CommandText		=	"	SELECT		{FIRST} 1																		"
											"				clicks_patient_id,	sold_status,		visit_number,		visit_date,			"
											"				visit_time,			row_added_date,		row_added_time,		line_number,		"
											"				valid_from_date,	largo_prescribed,	tot_units_sold,		tot_op_sold,		"
											"				qty_method,			op,					total_units,		treatment_length,	"
											"				doctor_id																		"

											"	FROM		doctor_presc																	"

											"	WHERE		%s																				"

											"	ORDER BY	visit_date DESC, visit_time DESC, visit_number, line_number						";
					NumOutputColumns	=	17;
					OutputColumnSpec	=	"				int,				short,				long,				int,				"
											"				int,				int,				int,				short,				"
											"				int,				int,				short,				short,				"
											"				short,				short,				short,				short,				"
											"				int	";
					NeedsWhereClauseID	=	1;
					break;


		case UpdateDoctorPresc_READ_Rx_by_Generic_Group:
												// DonR 11Dec2024 User Story #373619: Add rx_origin to columns SELECTed.
					SQL_CommandText		=	"	SELECT		{FIRST} 1																		"
											"				clicks_patient_id,	sold_status,		visit_number,		visit_date,			"
											"				visit_time,			row_added_date,		row_added_time,		line_number,		"
											"				valid_from_date,	largo_prescribed,	tot_units_sold,		tot_op_sold,		"
											"				qty_method,			op,					total_units,		treatment_length,	"
											"				doctor_id,			prescription_type,	rx_origin								"

											"	FROM		doctor_presc																	"

											"	WHERE		member_id			=		?													"
											"	  AND		doctor_presc_id		=		?													"
											"	  AND		valid_from_date		BETWEEN	? AND ?												"

											"	  AND		largo_prescribed	IN		(	SELECT	largo_code								"
											"												FROM	economypri								"
											"												WHERE	group_code	= ?							"
											"												  AND	del_flg		= '0'	)					"

											"	  AND		(doctor_id			=		? OR ? <> 0)										"
											"	  AND		member_id_code		=		?													"

											"	  AND		(	(sold_status	IN		(0, 1)		AND (? <> 2))							"
											"					OR																			"
											"					(sold_status	IN		(0, 1, 2)	AND (? =  2))	)						"

											"	ORDER BY	visit_date DESC, visit_time DESC, visit_number, line_number						";
					NumOutputColumns	=	19;
					OutputColumnSpec	=	"				int,				short,				long,				int,				"
											"				int,				int,				int,				short,				"
											"				int,				int,				short,				short,				"
											"				short,				short,				short,				short,				"
											"				int,				short,				short									";
					NumInputColumns		=	10;
					InputColumnSpec		=	"	int, int, int, int, int, int, short, short, short, short	";
					StatementIsSticky	=	1;
					break;


		case UpdateDoctorPresc_READ_Rx_by_Largo_Prescribed:
												// DonR 08Jun2020: This is a "sticky" version of UpdateDoctorPresc_READ_Rx,
												// with a fixed WHERE clause.
												// DonR 11Dec2024 User Story #373619: Add rx_origin to columns SELECTed.
					SQL_CommandText		=	"	SELECT		{FIRST} 1																		"
											"				clicks_patient_id,	sold_status,		visit_number,		visit_date,			"
											"				visit_time,			row_added_date,		row_added_time,		line_number,		"
											"				valid_from_date,	largo_prescribed,	tot_units_sold,		tot_op_sold,		"
											"				qty_method,			op,					total_units,		treatment_length,	"
											"				doctor_id,			prescription_type,	rx_origin								"

											"	FROM		doctor_presc																	"

											"	WHERE		member_id			=		?													"
											"	  AND		doctor_presc_id		=		?													"
											"	  AND		largo_prescribed	=		?													"
											"	  AND		valid_from_date		BETWEEN	? AND ?												"
											"	  AND		(doctor_id			=		? OR  ? <> 0)										"
											"	  AND		member_id_code		=		?													"

											"	  AND		(	(sold_status	IN		(0, 1)		AND (? <> 2))							"
											"					OR																			"
											"					(sold_status	IN		(0, 1, 2)	AND (? =  2))	)						"

											"	ORDER BY	visit_date DESC, visit_time DESC, visit_number, line_number						";
					NumOutputColumns	=	19;
					OutputColumnSpec	=	"				int,				short,				long,				int,				"
											"				int,				int,				int,				short,				"
											"				int,				int,				short,				short,				"
											"				short,				short,				short,				short,				"
											"				int,				short,				short									";
					NumInputColumns		=	10;
					InputColumnSpec		=	"	int, int, int, int, int, int, short, short, short, short	";
					StatementIsSticky	=	1;
					break;


		case UpdateDoctorPresc_UPD_drx_status_queue:
					SQL_CommandText		=	"	UPDATE	drx_status_queue								"

											"	SET		sold_status			= ?,						"
											"			close_by_rounding	= ?,						"
											"			last_sold_date		= ?,						"
											"			last_sold_time		= ?,						"
											"			upd_units_sold		= (upd_units_sold	+ ?),	"
											"			upd_op_sold			= (upd_op_sold		+ ?),	"
											"			linux_update_flag	= 1,						"
											"			linux_update_date	= ?,						"
											"			linux_update_time	= ?,						"
											"			reported_to_cds		= 1							"

											"	WHERE	visit_number		= ?							"
											"	  AND	doctor_presc_id		= ?							"	// Should be redundant, since visit_number/line_number is already supposed to be unique.
											"	  AND	line_number			= ?							";
					NumInputColumns		=	11;
					InputColumnSpec		=	"	short, short, int, int, short, short, int, int, long, int, short	";
					break;


		case UpdateDoctorPresc_UPD_pd_rx_link:
											// DonR 01Aug2023 User Story #469361: Add prescription_type to the update.
											// DonR 01Aug2024 Bug #334612: Keep the value sent by pharmacy (use_instr_template)
											// when we inserted the pd_rx_link row instead of updating it from the doctor
											// prescription. (This reverses the previous change in this query.)
											// DonR 11Dec2024 User Story #373619: Add rx_origin to columns updated.
					SQL_CommandText		=	"	UPDATE	pd_rx_link											"

											"	SET		visit_number		= ?,							"
											"			clicks_line_number	= ?,							"	// DonR 29Apr2019.
											"			visit_date			= ?,							"
											"			rx_origin			= ?,							"	// DonR 11Dec2024 User Story #373619
											"			date				= ?,							"
											"			time				= ?								"

											"	WHERE	prescription_id		= ?								"
											"	  AND	line_no				= ?								"
											"	  AND	doctor_presc_id		= ?								"
											"	  AND	largo_prescribed	= ?								"
											"	  AND	valid_from_date		= ?								"
											"	  AND	(visit_number		< 1 OR visit_date < 20160000)	";
					NumInputColumns		=	11;
					InputColumnSpec		=	"	long, short, int, short, int, int, int, short, int, int, int	";
					break;


		case UpdateDoctorPresc_UPD_Rx:		// NO LONGER IN USE!
					SQL_CommandText		=	"	UPDATE	doctor_presc									"

											"	SET		sold_status			= ?,						"
											"			close_by_rounding	= ?,						"
											"			last_sold_date		= ?,						"
											"			last_sold_time		= ?,						"
											"			tot_units_sold		= (tot_units_sold	+ ?),	"
											"			tot_op_sold			= (tot_op_sold		+ ?),	"
											"			linux_update_flag	= 1,						"
											"			linux_update_date	= ?,						"
											"			linux_update_time	= ?,						"
											"			reported_to_cds		= 1							"
											"	WHERE	%s												";
					NumInputColumns		=	8;
					InputColumnSpec		=	"	short, short, int, int, short, short, int, int			";
					NeedsWhereClauseID	=	1;
					break;


		case UpdateDoctorPresc_UPD_Rx_CDS_keys:
					SQL_CommandText		=	"	UPDATE	doctor_presc									"

											"	SET		sold_status			= ?,						"
											"			close_by_rounding	= ?,						"
											"			last_sold_date		= ?,						"
											"			last_sold_time		= ?,						"
											"			tot_units_sold		= (tot_units_sold	+ ?),	"
											"			tot_op_sold			= (tot_op_sold		+ ?),	"
											"			linux_update_flag	= 1,						"
											"			linux_update_date	= ?,						"
											"			linux_update_time	= ?,						"
											"			reported_to_cds		= 1							"
											"	WHERE	visit_number		= ?							"
											"	  AND	doctor_presc_id		= ?							"	// Should be redundant, since visit_number/line_number is already supposed to be unique.
											"	  AND	line_number			= ?							";
					NumInputColumns		=	11;
					InputColumnSpec		=	"	short, short, int, int, short, short, int, int			"	// SET parameters.
											"	long, int, short										";	// WHERE parameters.
					StatementIsSticky	=	1;
					break;


		case UpdateDoctorPresc_UPD_Rx_std_keys:
					SQL_CommandText		=	"	UPDATE	doctor_presc									"

											"	SET		sold_status			= ?,						"
											"			close_by_rounding	= ?,						"
											"			last_sold_date		= ?,						"
											"			last_sold_time		= ?,						"
											"			tot_units_sold		= (tot_units_sold	+ ?),	"
											"			tot_op_sold			= (tot_op_sold		+ ?),	"
											"			linux_update_flag	= 1,						"
											"			linux_update_date	= ?,						"
											"			linux_update_time	= ?,						"
											"			reported_to_cds		= 1							"

											"	WHERE	member_id			= ?							"
											"	  AND	doctor_presc_id		= ?							"
											"	  AND	largo_prescribed	= ?							"
											"	  AND	valid_from_date		= ?							"
											"	  AND	doctor_id			= ?							"
											"	  AND	line_number			= ?							"	// DonR 29Apr2019 - won't do anything interesting until/unless it's added to the unique index.
											"	  AND	member_id_code		= ?							";
					NumInputColumns		=	15;
					InputColumnSpec		=	"	short, short, int, int, short, short, int, int			"	// SET parameters.
											"	int, int, int, int, int, short, short					";	// WHERE parameters.
					StatementIsSticky	=	1;
					break;


		case IntOD_INS_presc_drug_inter:
					SQL_CommandText		=	"	INSERT INTO	presc_drug_inter						"
											"	(													"

													// Variable header columns
											"		pharmacy_code,				institued_code,		"
											"		terminal_id,				date,				"
											"		time,						member_id,			"
											"		mem_id_extension,			error_code,			"
											"		dtl_error_code,				print_flg,			"
											"		elect_pr_flag,				error_mode,			"
											"		interaction_type,			check_type,			"

													// Constant header columns
											"		special_presc_num,			spec_pres_num_sors,	"
											"		del_flg,					reported_to_as400,	"
											"		destination,				doc_approved_flag,	"

													// Drug 1 columns
											"		prescription_id,			line_no,			"
											"		largo_code,					doctor_id,			"
											"		doctor_id_type,		"

													// Drug 2 columns
											"		largo_code_inter,			presc_id_inter,		"
											"		line_no_inter,				sec_doctor_id,		"
											"		sec_doctor_id_type,			duration,			"
											"		op,							units,				"
											"		sec_presc_date,				stop_blood_date		"
											"	)													";
					NumInputColumns		=	35;
					InputColumnSpec		=	"	int, short, short, int, int, int, short, short, short, short, short, short, short, short,	"	// 14
											"	int, short, short, short, short, short,														"	//  6
											"	int, short, int, int, short,																"	//  5
											"	int, int, short, int, short, short, int, int, int, int										";	// 10
					GenerateVALUES		=	1;
					break;


		case CheckForInteractionIshur_READ_IshurCount:
					SQL_CommandText		=	NULL;	// Variable SQL text set up by calling routine.
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					break;


		// Transaction-specific SQL calls.
		case TR1001_INS_pharmacy_log:
					SQL_CommandText		=	"	INSERT INTO	PHARMACY_LOG																				"
											"	(																										"
											"		pharmacy_code,		institued_code,		terminal_id,		error_code,			open_close_flag,	"
											"		price_list_code,	price_list_date,	price_list_macabi_,	price_list_cost_da,	drug_list_date,		"
											"		software_ver_pc,	hardware_version,	owner,				comm_type,			phone_1,			"
											"		phone_2,			phone_3,			message_flag,		price_list_flag,	price_l_mcbi_flag,	"

											"		drug_list_flag,		software_ver_need,	discounts_date,		suppliers_date,		macabi_centers_dat,	"
											"		error_list_date,	disk_space,			net_flag,			comm_fail_filesize,	backup_date,		"
											"		available_mem,		pc_date,			pc_time,			db_date,			db_time,			"
											"		user_ident,			error_code_list_fl,	discounts_flag,		suppliers_flag,		macabi_centers_fla,	"

											"		closing_type,		comm_error_code,	reported_to_as400											"
											"	)																										";
					NumInputColumns		=	43;
					InputColumnSpec		=	"		integer				smallint			smallint			smallint			smallint			"
											"		smallint			integer				integer				integer				integer				"
											"		char(9)				integer				smallint			smallint			varchar(10)			"
											"		varchar(10)			varchar(10)			smallint			smallint			smallint			"

											"		smallint			char(9)				integer				integer				integer				"
											"		integer				integer				smallint			integer				integer				"
											"		smallint			integer				integer				integer				integer				"
											"		integer				smallint			smallint			smallint			smallint			"

											"		smallint			smallint			smallint	";
					GenerateVALUES		=	1;
					break;


		case TR1001_READ_Pharmacy:
					SQL_CommandText		=	"	SELECT	phone_1,			phone_2,			phone_3,			institued_code,		price_list_code,	"
											"			hesder_category,	permission_type,	software_ver_need,	message_1,			message_2,			"
											"			message_3,			message_4,			message_5,			fps_fee_level,		fps_fee_lower,		"
											"			fps_fee_upper,		maccabicare_flag	"

											"	FROM	pharmacy								"
											"	WHERE	pharmacy_code   = ?						"
											"	  AND	del_flg			= ?						";
					NumOutputColumns	=	17;
					OutputColumnSpec	=	"	varchar(10), varchar(10), varchar(10), short, short,	"
											"	short, short, char(9), char(60), char(60),				"
											"	char(60), char(60), char(60), int, int,					"
											"	int, short	";
					NumInputColumns		=	2;
					InputColumnSpec		=	"	int, short	";
					break;


		case TR1011_2094_DrugListDownload_cur:
					// DonR 17Feb2019 CR #28225 - send shape_code_new (from C0EBRZ) instead shape_code (C0V6RW).
					// DonR 06Nov2025: Added is_narcotic and psychotherapy_drug to query output.
					SQL_CommandText		=	"	SELECT	largo_code,				del_flg,				assuta_drug_status,		long_name,			"
											"			package_size,			member_price_code,		computersoft_code,		additional_price,	"
											"			drug_book_flg,			no_presc_sale_flag,		drug_type,				fps_group_code,		"
											"			preferred_flg,			openable_pkg,			calc_op_by_volume,		per_1_units,		"

											"			package_volume,			maccabicare_flag,		in_gadget_table,		largo_type,			"
											"			shape_code_new,			how_to_take_code,		unit_abbreviation,		bar_code_value,		"
											"			ingr_1_code,			ingr_1_quant,			ingr_1_units,			per_1_quant,		"
											"			needs_refrigeration,	needs_dilution,			ConsignmentItem,		is_narcotic,		"	// DonR 16Apr2023 User Story #432608 add ConsignmentItem.
											"			psychotherapy_drug,		preparation_type,		update_date,			update_time			"

											"	FROM	drug_list																					"

											// Get stuff that's new to this pharmacy.
											"	WHERE	((update_date > ?)	OR																		"
											"			 (update_date = ?	AND update_time >= ?))													"

											// But ignore stuff that's future-dated!
											"	  AND	((update_date < ?)	OR																		"
											"			 (update_date = ?	AND update_time <=  ?))													";
					NumOutputColumns	=	36;
					OutputColumnSpec	=	"			int,					short,					short,					varchar(30),		"
											"			int,					short,					int,					short,				"
											"			short,					short,					char(1),				int,				"
											"			short,					short,					short,					char(3),			"

											"			double,					short,					short,					char(1),			"
											"			short,					short,					char(3),				char(14),			"
											"			short,					double,					char(3),				double,				"
											"			short,					short,					short,					short,				"
											"			short,					short,					int,					int					";
					NumInputColumns		=	6;
					InputColumnSpec		=	"	int, int, int, int, int, int	";
					break;


		case TR1013_INS_pharmacy_log:
					SQL_CommandText		=	"	INSERT INTO	PHARMACY_LOG																				"
											"	(																										"
											"		available_mem,		backup_date,		closing_type,		comm_error_code,	comm_fail_filesize,	"
											"		comm_type,			db_date,			db_time,			discounts_date,		discounts_flag,		"
											"		disk_space,			drug_list_date,		drug_list_flag,		error_code,			error_code_list_fl,	"
											"		error_list_date,	hardware_version,	institued_code,		macabi_centers_dat,	macabi_centers_fla,	"

											"		message_flag,		net_flag,			open_close_flag,	owner,				pc_date,			"
											"		pc_time,			pharmacy_code,		phone_1,			phone_2,			phone_3,			"
											"		price_l_mcbi_flag,	price_list_code,	price_list_cost_da,	price_list_date,	price_list_flag,	"
											"		price_list_macabi_,	software_ver_need,	software_ver_pc,	suppliers_date,		suppliers_flag,		"

											"		terminal_id,		user_ident,			reported_to_as400											"
											"	)																										"
											"	VALUES																									"
											"	(																										"
											"		0,					0,					?,					?,					0,					"
											"		0,					?,					?,					0,					0,					"
											"		0,					0,					0,					?,					0,					"
											"		0,					0,					?,					0,					0,					"

											"		0,					0,					?,					0,					?,					"
											"		?,					?,					0,					0,					0,					"
											"		0,					0,					0,					0,					0,					"
											"		0,					0,					0,					0,					0,					"

											"		?,					0,					?															"
											"	)																										";
					NumInputColumns		=	12;
					InputColumnSpec		=	"												smallint			smallint								"
											"							integer				integer														"
											"																	smallint								"
											"												smallint													"

											"												smallint								integer				"
											"		integer				integer																			"
											"																											"
											"																											"

											"		smallint								smallint	";
					break;


		case TR1014_PriceList_cur:
					SQL_CommandText		=	"	SELECT del_flg, largo_code, macabi_price, supplier_price FROM price_list WHERE %s	";
					NumOutputColumns	=	4;
					OutputColumnSpec	=	"	short, int, int, int	";
					NeedsWhereClauseID	=	1;
					break;


		case TR1014_READ_GetPharmMaccabicareFlag:
					SQL_CommandText		=	"	SELECT maccabicare_flag FROM pharmacy WHERE pharmacy_code = ? AND del_flg = ?	";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	short	";
					NumInputColumns		=	2;
					InputColumnSpec		=	"	int, short	";
					break;


		case TR1022_INS_Stock:
					SQL_CommandText		=	"	INSERT INTO	stock													"
											"	(																	"
											"		pharmacy_code,			institued_code,		terminal_id,		"
											"		user_ident,				action_type,		serial_no,			"
											"		date,					time,				supplier_for_stock,	"
											"		invoice_no,				discount_sum,		diary_month,		"
											"		num_of_lines_4,			update_date,		update_time,		"

											"		error_code,				comm_error_code,	del_flg,			"
											"		reported_to_as400,		vat_amount,			bil_amount,			"
											"		suppl_bill_date,		suppl_sendnum,		bil_amountwithvat,	"
											"		bil_amount_beforev,		bill_constr,		bill_invdiff		" 
											"	)																	";
					NumInputColumns		=	27;
					InputColumnSpec		=	"		int,					short,				short,			 	"
											"		int,					short,				int,				"
											"		int,					int,				int,				"
											"		varchar(10),			int,				int,				"
											"		short,					int,				int,				"

											"		short,					short,				short,				"
											"		short,					int,				int,				"
											"		int,					varchar(10),		int,				"
											"		int,					short,				int					";
					GenerateVALUES		=	1;
					break;


		case TR1022_INS_StockReport:
					SQL_CommandText		=	"	INSERT INTO	stock_report											"
											"	(																	"
											"		line_num,				largo_code,			inventory_op,		"
											"		units_amount,			quantity_type,		price_for_op,		"
											"		total_drug_price,		op_stock,			units_stock,		"
											"		min_stock_in_op,		comm_error_code,	update_date,		"

											"		update_time,			action_type,		diary_month,		"
											"		pharmacy_code,			serial_no,			terminal_id,		"
											"		del_flg,				reported_to_as400,	base_price			"
											"	)																	";
					NumInputColumns		=	21;
					InputColumnSpec		=	"		short,					int,				int,				"
											"		int,					short,				int,				"
											"		int,					int,				int,				"
											"		int,					short,				int,				"

											"		int,					short,				int,				"
											"		int,					int,				short,				"
											"		short,					short,				int					";
					GenerateVALUES		=	1;
					break;


		case TR1022_READ_CheckDuplicateStockRow:
					SQL_CommandText		=	"	SELECT	COUNT(*)				"
											"	FROM	stock					"

											"	WHERE	pharmacy_code      = ?	"
											"	  AND	date               = ?	"
											"	  AND	supplier_for_stock = ?	"
											"	  AND	terminal_id        = ?	"
											"	  AND	action_type        = ?	"
											"	  AND	serial_no          = ?	"
											"	  AND	diary_month        = ?	";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					NumInputColumns		=	7;
					InputColumnSpec		=	"	int, int, int, short, short, int, int	";
					Convert_NF_to_zero	=	1;
					break;


		case TR1026_message_id_cur:
					SQL_CommandText		=	"	SELECT		date, time, message_id					"
											"	FROM		pharmacy_message						"
											"	WHERE		pharmacy_code = ? AND delivery_date = 0	"
											"	ORDER BY	date, time								";
					NumOutputColumns	=	3;
					OutputColumnSpec	=	"	int, int, int	";
					NumInputColumns		=	1;
					InputColumnSpec		=	"	int	";
					break;


		case TR1026_messags_text_cur:
					SQL_CommandText		=	"	SELECT		message_id, message_line, message_text	"
											"	FROM		message_texts							"
											"	WHERE		message_id	= ?							"
											"	  AND		del_flg		= ?							"
											"	ORDER BY	message_id, message_line				";
					NumOutputColumns	=	3;
					OutputColumnSpec	=	"	int, short, char(60)	";
					NumInputColumns		=	2;
					InputColumnSpec		=	"	int, short	";
					break;


		case TR1026_UPD_pharmacy_message:
					SQL_CommandText		=	"	UPDATE	pharmacy_message		"
											"	SET		delivery_date	= ?,	"
											"			delivery_time	= ?		"
											"	WHERE	message_id		= ?		"
											"	  AND	pharmacy_code	= ?		";
					NumInputColumns		=	4;
					InputColumnSpec		=	"	int, int, int, int	";
					break;


		case TR1028_READ_PharmacyDailySum:
											// DonR 14May2020: Transaction 1028 isn't really in use any more, so I'm
											// not bothering to CAST the SUM's as INTEGERs.
					SQL_CommandText		=	"	SELECT	SUM (sum			),	SUM (member_part_sum	),	"
											"			SUM	(presc_count	),	SUM (presc_lines_count	),	"
											"			SUM (delete_sum		),	SUM (delete_lines		),	"
											"			SUM (net_fail_sum	),	SUM	(net_fail_lines		)	"
											"	FROM	pharmacy_daily_sum									"

											"	WHERE	pharmacy_code		=		?						"
											"	  AND	institued_code		=		?						"
											"	  AND	diary_month			=		?						"
											"	  AND	date				BETWEEN	? AND ?					";
					NumOutputColumns	=	8;
					OutputColumnSpec	=	"	int, int, int, int, int, int, int, int	";
					NumInputColumns		=	5;
					InputColumnSpec		=	"	int, short, short, int, int	";
					break;


		case TR1043_error_messags_cur:
					SQL_CommandText		=	"	SELECT error_code, error_line, severity_pharm FROM pc_error_message	";
					NumOutputColumns	=	3;
					OutputColumnSpec	=	"	short, varchar(60), short	";
					break;


		case TR1047_member_pric_up_cur:
					SQL_CommandText		=	"	SELECT	member_price_code,	member_price_prcnt,	tax,				"
											"			participation_name,	calc_type_code,		member_institued,	"
											"			yarpa_part_code,	max_pkg_price							"
											"	FROM	member_price												";
					NumOutputColumns	=	8;
					OutputColumnSpec	=	"	short, short, int, char(25), short, short, short, int	";
					break;


		case TR1049_supplier_up_cur:
					SQL_CommandText		=	"	SELECT		del_flg,			supplier_table_cod,		supplier_code,		"
											"				supplier_name,		supplier_type,			street_and_no,		"
											"				city,				zip_code,				phone_1,			"

											"				phone_2,			fax_num,				comm_supplier,		"
											"				employee_id,		allowed_proc_list,		employee_password,	"
											"				check_type,			update_date,			update_time			"

											"	FROM		suppliers														"
											"	WHERE		%s																";
					NeedsWhereClauseID	=	1;
					NumOutputColumns	=	18;
					OutputColumnSpec	=	"				short,				short,					int,				"
											"				varchar(25),		char(2),				varchar(20),		"
											"				varchar(20),		int,					varchar(10),		"

											"				varchar(10),		varchar(10),			int,				"
											"				int,				char(50),				int,				"
											"				short,				int,					int					";
					break;


		case TR1051_centers_up_cur:
					SQL_CommandText		=	"	SELECT	del_flg,			macabi_centers_num,		first_center_type,		"
											"			second_center_type,	macabi_centers_nam,		street_and_no,			"
											"			city,				zip_code,				phone_1,				"
											"			assuta_card_number,	fax_num,				discount_percent,		"
											"			credit_flag,		allowed_proc_list,		allowed_belongings		"
											"	FROM	macabi_centers														"
											"	WHERE	%s																	";
					NeedsWhereClauseID	=	1;
					NumOutputColumns	=	15;
					OutputColumnSpec	=	"			short,				int,					char(2),				"
											"			char(2),			varchar(40),			varchar(20),			"
											"			varchar(20),		int,					varchar(10),			"
											"			char(10),			varchar(10),			short,					"
											"			short,				char(50),				char(50)				";
					break;


		case TR1053_INS_money_empty:
					SQL_CommandText		=	"	INSERT INTO	money_empty														"
											"	(																			"
											"		pharmacy_code,		institued_code,	terminal_id,		action_type,	"
											"		diary_month,		date,			time,				update_date,	"
											"		update_time,		receipt_num,	serial_no_2,		serial_no_3,	"
											"		num_of_lines_4,		del_flg,		comm_error_code,	error_code,		"
											"		reported_to_as400														"
											"	)																			";
					NumInputColumns		=	17;
					InputColumnSpec		=	"		int,				short,			short,				short,			"
											"		short,				int,			int,				int,			"
											"		int,				int,			short,				int,			"
											"		short,				short,			short,				short,			"
											"		short																	";
					GenerateVALUES		=	1;
					break;


		case TR1053_INS_money_emp_lines:
					SQL_CommandText		=	"	INSERT INTO	money_emp_lines													"
											"	(																			"
											"		diary_month,		pharmacy_code,	action_type,		receipt_num,	"
											"		terminal_id,		user_ident,		card_num,			del_flg,		"
											"		terminal_id_proc,	payment_type,	sale_total,			sale_num,		"
											"		refund_total,		refund_num,		date,				time,			"
											"		reported_to_as400														"
											"	)																			";
					NumInputColumns		=	17;
					InputColumnSpec		=	"		short,				int,			short,				int,			"
											"		short,				int,			int,				short,			"
											"		short,				short,			int,				short,			"
											"		int,				short,			int,				int,			"
											"		short																	";
					GenerateVALUES		=	1;
					break;


		case TR1055_dyn_var_cur:
					SQL_CommandText		=	"	SELECT	dynam_code,	dynam_type,	dynam_var FROM dynamic_var WHERE dynam_type in (0,1,4)	";
					NumOutputColumns	=	3;
					OutputColumnSpec	=	"			short,		short,		varchar(40)	";
					break;


		case TR1055_msg_fmt_cur:
					// This cursor is opened 64 times each time Transaction 1055 is run - so it needs to be "locally sticky".
					// DonR 27Oct2020: There should be enough capacity, so I'm making it sticky again - "global" this time.
					SQL_CommandText		=	"	SELECT		msg_row,		msg_row_alternate,	msg_row_num,	msg_sub_cycle	"
											"	FROM		dur_msgs															"
											"	WHERE		msg_type	= ?														"
											"	  AND		msg_dest	= ?														"
											"	  AND		msg_part	= ?														"
											"	  AND		msg_sever	= ?														"
											"	ORDER BY 	msg_row_num															";
					NumOutputColumns	=	4;
					OutputColumnSpec	=	"				varchar(80),	varchar(80),		short,			short			";
					NumInputColumns		=	4;
					InputColumnSpec		=	"	short, short, short, short	";
					StatementIsSticky	=	1;
					break;


		case TR1055_presc_mes_cur:
					SQL_CommandText		=	"	SELECT	pharmacy_code,				institued_code,				terminal_id,		"
											"			prescription_id,			line_no,					largo_code,			"
											"			largo_code_inter,			interaction_type,			sec_presc_date,		"
											"			time,						del_flg,					presc_id_inter,		"
											"			line_no_inter,				member_id,					mem_id_extension,	"

											"			doctor_id,					doctor_id_type,				sec_doctor_id,		"
											"			sec_doctor_id_type,			error_code,					duration,			"
											"			op,							units,						stop_blood_date,	"
											"			check_type,					destination,				print_flg,			"
											"			doc_approved_flag															"

											"	FROM	presc_drug_inter															"

											"	WHERE	prescription_id	= ?															"
											"	AND		del_flg			= ?															";
					NumOutputColumns	=	28;
					OutputColumnSpec	=	"			int,						short,						short,				"
											"			int,						short,						int,				"
											"			int,						short,						int,				"
											"			int,						short,						int,				"
											"			short,						int,						short,				"

											"			int,						short,						int,				"
											"			short,						short,						short,				"
											"			int,						int,						int,				"
											"			short,						short,						short,				"
											"			short																		";
					NumInputColumns		=	2;
					InputColumnSpec		=	"	int, short	";
					break;


		case TR1055_READ_GetDL_LongName:
					SQL_CommandText		=	"	SELECT long_name FROM drug_list WHERE largo_code = ?	";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	varchar(30)	";
					NumInputColumns		=	1;
					InputColumnSpec		=	"	int	";
					break;


		case TR1055_READ_GetDrugInteractionNotes:
					SQL_CommandText		=	"	SELECT note1, note2 FROM drug_interaction WHERE first_largo_code = ? AND second_largo_code = ?	";
					NumOutputColumns	=	2;
					OutputColumnSpec	=	"	varchar(40), varchar(40)	";
					NumInputColumns		=	2;
					InputColumnSpec		=	"	int, int	";
					break;


		case TR1055_READ_interaction_type:
					SQL_CommandText		=	"	SELECT interaction_type FROM interaction_type WHERE interaction_type = ?	";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	short	";
					NumInputColumns		=	1;
					InputColumnSpec		=	"	short	";
					break;


		case TR1055_READ_members:
					SQL_CommandText		=	"	SELECT first_name, last_name FROM members WHERE member_id = ? AND mem_id_extension = ?	";
					NumOutputColumns	=	2;
					OutputColumnSpec	=	"	varchar(8), varchar(14)	";
					NumInputColumns		=	2;
					InputColumnSpec		=	"	int, short	";
					break;


		case TR1080_pharmacy_contract_cur:
					SQL_CommandText		=	"	SELECT		contract_min_amt,	contract_max_amt,	contract_fee,	"
											"				fps_group_code,		fps_group_name						"
											"	FROM		pharmacy_contract										"
											"	WHERE		contract_code		=  ?								"
											"	  AND		contract_from_date	<= ?								"
											"	  AND		contract_to_date	>= ?								"
											"	  AND		del_flg				=  ?								"
											"	ORDER BY	contract_min_amt										";
					NumOutputColumns	=	5;
					OutputColumnSpec	=	"	int, int, int, int, char(16)	";
					NumInputColumns		=	4;
					InputColumnSpec		=	"	int, int, int, short	";
					break;


		case TR1080_READ_pharmacy:
					SQL_CommandText		=	"	SELECT	contract_type, contract_incl_tax, contract_code, contract_effective	"
											"	FROM	pharmacy															"
											"	WHERE	pharmacy_code = ? AND del_flg = ?	";
					NumOutputColumns	=	4;
					OutputColumnSpec	=	"	short, short, int, int	";
					NumInputColumns		=	2;
					InputColumnSpec		=	"	int, short	";
					break;


		case TR2001_Rx_cursor:
					SQL_CommandText		=	"	SELECT	doctor_presc_id,									"
											"			largo_prescribed,									"
											"			amt_per_dose,										"
											"			FLOOR(amt_per_dose),								"
											"			doses_per_day,										"
											"			treatment_length,									"
											"			op,													"
											"			valid_from_date,									"
											"			'0',												"	// Placeholder for ASCII prescriptiontype column in prescr_wr_new.
											"			prescription_type,									"	// Numeric - needs to be translated to ASCII value for compatibility.
											"			sold_status,										"
											"			tot_units_sold,										"
											"			tot_op_sold,										"
											"			0													"	// No equivalent of prw_id in new doctor_presc table.

											"	FROM 	doctor_presc										"

											"	WHERE	%s													"

											"	ORDER BY	valid_from_date, doctor_presc_id, line_number	";
					NumOutputColumns	=	14;
					OutputColumnSpec	=	"	int, int, double, short, short, short, short, int, char(1), short, short, short, short, int	";
					NeedsWhereClauseID	=	1;
					break;


		case TR2001_READ_count_special_prescs:
					SQL_CommandText		=	"	SELECT	CAST (count(*) AS INTEGER)	"

											"	FROM	special_prescs				"

											"	WHERE	member_id			=  ?	"
											"	  AND	mem_id_extension	=  ?	"
											"	  AND	largo_code			=  ?	"
											"	  AND	confirmation_type	=  1	"	// Active ishur.
											"	  AND	treatment_start		<= ?	"
											"	  AND	del_flg				=  0	"
											"	  AND	member_price_code	>  0	"
											"	  AND	stop_use_date		>= ?	";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					NumInputColumns		=	5;
					InputColumnSpec		=	"	int, short, int, int, int	";
					Convert_NF_to_zero	=	1;
					break;


		case TR2001_READ_drug_list:
					SQL_CommandText		=	"	SELECT	largo_code,			economypri_group,	preferred_flg,	"
											"			preferred_largo,	multi_pref_largo,	package_size,	"
											"			openable_pkg,		chronic_flag,		cronic_months,	"
											"			del_flg,			assuta_drug_status					"

											"	FROM	drug_list												"

											"	WHERE	largo_code	= ?											";
					NumOutputColumns	=	11;
					OutputColumnSpec	=	"	int, int, short, int, short, int, short, short, short, short, short	";
					NumInputColumns		=	1;
					InputColumnSpec		=	"	int	";
					break;


		case TR2001_READ_drug_list_preferred_largo_fields:
					SQL_CommandText		=	"	SELECT	package_size,	openable_pkg,	chronic_flag,	cronic_months	"

											"	FROM	drug_list														"

											"	WHERE	largo_code	=  ?												"

												  // DonR 09Sep2009 - Added Assuta Pharmacy criteria for select.
											"	  AND	((del_flg <> ?) OR												"
											"			 (? = ? AND assuta_drug_status IN (?, ?)))						";
					NumOutputColumns	=	4;
					OutputColumnSpec	=	"	int, short, short, short	";
					NumInputColumns		=	6;
					InputColumnSpec		=	"	int, short, short, short, short, short	";
					break;


		case TR2001_READ_member_data:
					SQL_CommandText		=	"	SELECT	last_name,			first_name,		date_of_bearth,	"
											"			maccabi_code,		spec_presc,		maccabi_until,	"
											"			died_in_hospital									"

											"	FROM	members												"

											"	WHERE	member_id			= ?								"
											"	  AND	mem_id_extension	= ?								";
					NumOutputColumns	=	7;
					OutputColumnSpec	=	"	varchar(14), varchar(8), int, short, short, int, short	";
					NumInputColumns		=	2;
					InputColumnSpec		=	"	int, short	";
					break;


		case TR2003_INS_prescription_drugs:
					SQL_CommandText		=	"	INSERT INTO prescription_drugs								"
											"	(															"
											"		prescription_id,				line_no,				"
											"		member_id,						mem_id_extension,		"
											"		largo_code,						reported_to_as400,		"
											"		op,								units,					"
											"		duration,						stop_use_date,			"

											"		date,							time,					"
											"		del_flg,						delivered_flg,			"
											"		price_replace,					price_extension,		"
											"		link_ext_to_drug,				price_for_op,			"
											"		drug_answer_code,				member_price_code,		"

											"		calc_member_price,				presc_source,			"
											"		macabi_price_flg,				pharmacy_code,			"
											"		institued_code,					price_for_op_ph,		"
											"		stop_blood_date,				total_drug_price,		"
											"		op_stock,						units_stock,			"

											"		total_member_price,				comm_error_code,		"
											"		t_half,							doctor_id,				"
											"		doctor_id_type,					doctor_presc_id,		"
											"		dr_presc_date,					dr_largo_code,			"
											"		subst_permitted,				units_unsold,			"
													
											"		op_unsold,						units_per_dose,			"
											"		doses_per_day,					particip_method,		"
											"		prescr_wr_rrn,					updated_by,				"
											"		special_presc_num,				spec_pres_num_sors,		"
											"		rule_number,					action_type,			"

											"		op_prescribed,					units_prescribed,		"
											"		credit_type_code,				why_future_sale_ok,		"
											"		qty_limit_chk_type,				qty_lim_ishur_num,		"
											"		vacation_ishur_num,				dr_visit_date,			"
											"		doctor_facility											"
											"	)															";
					NumInputColumns		=	59;
					InputColumnSpec		=	"	int, short, int, short, int, short, int, int, short, int,		"	// DonR 20Feb2020: Duration is populated from a short int.
											"	int, int, short, short, int, int, int, int, short, short,		"
											"	short, short, short, int, short, int, int, int, int, int,		"
											"	int, short, short, int, short, int, int, int, short, int,		"	// DonR 11Mar2020: T_half is an integer in prescription_drugs, but is populated by a short. (Change column type in MS-SQL!)
											"	int, int, short, short, int, short, int, short, int, short,		"	// DonR 20Feb2020: "updated_by" is populated by in_health_pack, a short int. Doses_per_day is also populated from a short int.
											"	int, int, short, short, short, int, int, int, int				";
					GenerateVALUES		=	1;
					break;


		case TR2003_INS_prescriptions:
					SQL_CommandText		=	"	INSERT INTO prescriptions							"
											"	(													"
											"		pharmacy_code,			institued_code,			"
											"		price_list_code,		member_id,				"
											"		mem_id_extension,		member_institued,		"
											"		doctor_id,				doctor_id_type,			"
											"		presc_source,			prescription_id,		"

											"		special_presc_num,		spec_pres_num_sors,		"
											"		date,					time,					"
											"		error_code,				prescription_lines,		"
											"		credit_type_code,		card_date,				"
											"		member_discount_pt,		terminal_id,			"

											"		receipt_num,			user_ident,				"
											"		presc_pharmacy_id,		macabi_code,			"
											"		doctor_presc_id,		reported_to_as400,		"
											"		comm_error_code,		member_price_code,		"
											"		doctor_insert_mode,		macabi_centers_num,		"

											"		doc_device_code,		diary_month,			"
											"		memb_prc_code_ph,		next_presc_start,		"
											"		next_presc_stop,		waiting_msg_flg,		"
											"		delivered_flg,			del_flg,				"
											"		reason_for_discnt,		reason_to_disp,			"

											"		origin_code,			action_type,			"
											"		insurance_type,			darkonai_plus_sale		"
											"	)													";
					NumInputColumns		=	44;
					InputColumnSpec		=	"	int, short, short, int, short, short, int, short, short, int,	"
											"	int, short, int, int, short, short, short, short, short, short,	"
											"	int, int, int, short, int, short, short, short, short, int,		"
											"	int, short, short, int, int, short, short, short, int, short,	"
											"	short, short, char(1), short									";
					GenerateVALUES		=	1;
					break;


		case TR2003_5003_READ_doctor_presc:
					SQL_CommandText		=	"	SELECT	sold_status,							"
											"			FLOOR(amt_per_dose),					"
											"			amt_per_dose,							"
											"			CAST(prescription_type AS CHAR(1)),		"
											"			visit_date,								"
											"			valid_from_date,						"
											"			largo_prescribed						"

											"	FROM	doctor_presc							"

											"	WHERE	%s										";
					NumOutputColumns	=	7;
					OutputColumnSpec	=	"	short, short, double, char(1), int, int, int	";
					NeedsWhereClauseID	=	1;
					break;


		case TR2003_5003_READ_gadgets:
					SQL_CommandText		=	"	SELECT	service_code,		service_number,			"
											"			service_type,		enabled_mac,			"
											"			enabled_hova,		enabled_keva,			"
											"			enabled_pvt_pharm							"

											"	FROM	gadgets										"

											"	WHERE	largo_code		= ?							"
											"		AND	((gadget_code	= ?) OR  (? = 0))			"
											"		AND	(((enabled_mac	> 0) AND (?	> 0))	OR		"
											"			 ((enabled_hova	> 0) AND (?	> 0))	OR		"
											"			 ((enabled_keva	> 0) AND (?	> 0)))			";
					NumOutputColumns	=	7;
					OutputColumnSpec	=	"	int, short, short, short, short, short, short	";
					NumInputColumns		=	6;
					InputColumnSpec		=	"	int, int, int, short, short, short	";
					break;


		case TR2003_READ_members:
					SQL_CommandText		=	"	SELECT	last_name,					first_name,				"
											"			date_of_bearth,				credit_type_code,		"
											"			maccabi_code,										"
											"			maccabi_until,				mac_magen_code,			"
											"			mac_magen_from,				mac_magen_until,		"
											"			keren_mac_code,				keren_mac_from,			"
											"			keren_mac_until,			keren_wait_flag,		"
											"			max_drug_date,										"
											"			payer_tz,					payer_tz_code,			"
											"			member_discount_pt,									"
											"			insurance_type,				carry_over_vetek,		"
											"			insurance_status,			yahalom_code,			"
											"			yahalom_from,				yahalom_until,			"
											"			card_date,					authorizealways,		"
											"			idnumber_main,				idcode_main,			"
											"			spec_presc,					sex,					"
											"			illness_bitmap,				updated_by,				"
											"			check_od_interact,			asaf_code,				"
											"			darkonai_type,				force_100_percent_ptn,	"
											"			darkonai_no_card,			has_blocked_drugs,		"
											"			died_in_hospital									"

											"	FROM	members											"

											"	WHERE	member_id			= ?							"
											"	  AND	mem_id_extension	= ?							";
					NumOutputColumns	=	38;
					OutputColumnSpec	=	"	varchar(14), varchar(8), int, short, short, int, short, int, int,		"
											"	short, int, int, short, int, int, short, short, char(1), int, short,	"
											"	short, int, int, short, short, int, short, short, short, int, int,		"
											"	short, short, short, short, short, short, short							";
					NumInputColumns		=	2;
					InputColumnSpec		=	"	int, short	";
					break;


		case TR2005_presc_drug_cur:
					SQL_CommandText		=	"	SELECT		largo_code,				op,					units,				"
											"				line_no,				link_ext_to_drug,	price_for_op,		"
											"				price_for_op_ph,		drug_answer_code,	stop_use_date,		"
											"				t_half,					prescr_wr_rrn,		doctor_presc_id,	"
											"				particip_method,		macabi_price_flg,	stop_blood_date,	"
											"				presc_source													"

											"	FROM		prescription_drugs												"

											"	WHERE		prescription_id	= ?												"
											"	  AND		delivered_flg   = ?												";
					NumOutputColumns	=	16;
					OutputColumnSpec	=	"	int, int, int, short, int, int, int, short, int, short,		"	// DonR 11Mar2020: t_half is an INTEGER in Informix, but should be a SMALLINT - it's populated by short's and read into short's.
											"	int, int, short, short, int, short							";
					NumInputColumns		=	2;
					InputColumnSpec		=	"	int, short	";
					break;


		case TR2005_READ_prescriptions:
					SQL_CommandText		=	"	SELECT	pharmacy_code,			institued_code,			"
											"			mem_id_extension,		member_id,				"
											"			member_institued,		doctor_id,				"
											"			doctor_id_type,			presc_source,			"
											"			error_code,				prescription_lines,		"
											"			terminal_id,			price_list_code,		"
											"			member_discount_pt,		date,					"
											"			diary_month										"

											"	FROM	prescriptions									"
											"	WHERE	prescription_id = ?								";
					NumOutputColumns	=	15;
					OutputColumnSpec	=	"	int, short, short, int, short, int, short, short,	"
											"	short, short, short, short, short, int, short		";
					NumInputColumns		=	1;
					InputColumnSpec		=	"	int	";
					break;


		case TR2005_UPD_pharmacy_daily_sum:
					SQL_CommandText		=	"	UPDATE	pharmacy_daily_sum								"

											"	SET		sum					= sum				+ ?,	"
											"			member_part_sum		= member_part_sum	+ ?,	"	
											"			purchase_sum		= purchase_sum		+ ?,	"
											"			presc_count			= presc_count		+ 1,	"
											"			presc_lines_count	= presc_lines_count	+ ?		"

											"	WHERE	pharmacy_code	= ?								"
											"	  AND	institued_code	= ?								"
											"	  AND	diary_month		= ?								"
											"	  AND	date			= ?								"
											"	  AND	terminal_id		= ?								";
					NumInputColumns		=	9;
					InputColumnSpec		=	"	int, int, int, int, int, short, short, int, short	";
					break;


		case TR2005_UPD_prescription_drugs:
					SQL_CommandText		=	"	UPDATE	prescription_drugs			"

											"	SET		op					= ?,	"
											"			units				= ?,	"
											"			delivered_flg		= ?,	"
											"			reported_to_as400	= ?,	"
											"			op_stock			= ?,	"
											"			units_stock			= ?,	"
											"			total_drug_price	= ?,	"
											"			total_member_price	= ?,	"
											"			member_discount_pt	= ?,	"
											"			price_for_op_ph		= ?,	"
											"			date				= ?,	"
											"			time				= ?,	"
											"			macabi_price_flg	= ?,	"
											"			ingr_1_code			= ?,	"
											"			ingr_2_code			= ?,	"
											"			ingr_3_code			= ?,	"
											"			ingr_1_quant_bot	= ?,	"
											"			ingr_2_quant_bot	= ?,	"
											"			ingr_3_quant_bot	= ?,	"
											"			ingr_1_quant_std	= ?,	"
											"			ingr_2_quant_std	= ?,	"
											"			ingr_3_quant_std	= ?,	"
											"			dr_largo_code		= ?,	"
											"			dr_presc_date		= ?,	"
											"			link_ext_to_drug	= ?,	"
											"			num_linked_rxes		= ?,	"
											"			paid_for			= ?		"

											"	WHERE	prescription_id	= ?			"
											"	  AND	line_no			= ?			";
					NumInputColumns		=	29;
					InputColumnSpec		=	"	int, int, short, short, int, int, int, int, int, int,					"
											"	int, int, short, short, short, short, double, double, double, double,	"
											"	double, double, int, int, int, short, short, int, short					";
					break;


		case TR2005_UPD_prescriptions:
					SQL_CommandText		=	"	UPDATE	prescriptions				"

											"	SET		receipt_num			= ?,	"
											"			user_ident			= ?,	"
											"			presc_pharmacy_id	= ?,	"
											"			doctor_insert_mode	= ?,	"
											"			macabi_centers_num	= ?,	"
											"			doc_device_code		= ?,	"
											"			diary_month			= ?,	"
											"			delivered_flg		= ?,	"
											"			reported_to_as400	= ?,	"
											"			credit_type_used	= ?,	"
											"			date				= ?,	"
											"			time				= ?,	"
											"			error_code			= ?,	"	// DonR 24Jul2013 - the previous value unless we hit a new error.
											"			reason_for_discnt	= ?,	"
											"			reason_to_disp		= ?,	"
											"			subsidy_amount		= ?,	"
											"			paid_for			= ?		"

											"	WHERE	prescription_id	= ?			";
					NumInputColumns		=	18;
					InputColumnSpec		=	"	int, int, int, short, int, int, short, short,				"
											"	short, short, int, int, short, int, short, int, short, int	";
					break;


		case TR2005spool_INS_prescription_drugs:	// DonR 11Feb2020: Changed the input parameters for member_discount_pt
													// and price_extension from int to short. Both are INTEGERs in
													// prescription_drugs, but SMALLINTs elsewhere; and the host variables
													// for these values are short integers. Since we don't use any values
													// greater than 10000 (= 100%) for these numbers, there's no point in
													// using an int host variable; in fact, the database variables really
													// should be SMALLINTs.
					SQL_CommandText		=	"	INSERT INTO prescription_drugs								"
											"	(															"
											"		prescription_id,				line_no,				"
											"		member_id,						mem_id_extension,		"
											"		largo_code,						reported_to_as400,		"
											"		op,								units,					"
											"		duration,						stop_use_date,			"

											"		date,							time,					"
											"		del_flg,						delivered_flg,			"
											"		price_replace,					price_extension,		"
											"		link_ext_to_drug,				price_for_op,			"
											"		drug_answer_code,				member_price_code,		"

											"		calc_member_price,				presc_source,			"
											"		macabi_price_flg,				pharmacy_code,			"
											"		institued_code,					price_for_op_ph,		"
											"		stop_blood_date,				total_drug_price,		"
											"		op_stock,						units_stock,			"

											"		total_member_price,				comm_error_code,		"
											"		t_half,							doctor_id,				"
											"		doctor_id_type,					updated_by,				"
											"		special_presc_num,				spec_pres_num_sors,		"
											"		doctor_presc_id,				dr_presc_date,			"
													
											"		dr_largo_code,					subst_permitted,		"
											"		units_per_dose,					doses_per_day,			"
											"		member_discount_pt,				particip_method,		"
											"		prescr_wr_rrn,					ingr_1_code,			"
											"		ingr_2_code,					ingr_3_code,			"

											"		ingr_1_quant_bot,				ingr_2_quant_bot,		"
											"		ingr_3_quant_bot,				ingr_1_quant_std,		"
											"		ingr_2_quant_std,				ingr_3_quant_std,		"
											"		action_type,					credit_type_code,		"

											"		why_future_sale_ok,				qty_limit_chk_type,		"
											"		qty_lim_ishur_num,				vacation_ishur_num,		"
											"		dr_visit_date,					doctor_facility,		"
											"		num_linked_rxes,				paid_for				"
											"	)															";
					NumInputColumns		=	66;
					InputColumnSpec		=	"	int, short, int, short, int, short, int, int, short, int,		"	// DonR 20Feb2020: Duration is populated from a short int.
											"	int, int, short, short, int, short, int, int, short, short,		"
											"	short, short, short, int, short, int, int, int, int, int,		"
											"	int, short, short, int, short, short, int, short, int, int,		"	// DonR 20Feb2020: "updated_by" is populated from in_health_pack, a short int.
																													// DonR 11Mar2020: T_half is an integer in prescription_drugs, but is populated by a short. (Change column type in MS-SQL!)
											"	int, short, int, short, short, short, int, short, short, short,	"	// DonR 20Feb2020: doses_per_day is populated from a short integer.
											"	double, double, double, double, double, double, short, short,	"
											"	short, short, int, int, int, int, short, short					";
					GenerateVALUES		=	1;
					break;


		case TR2005spool_INS_prescriptions:
					SQL_CommandText		=	"	INSERT INTO prescriptions							"
											"	(													"
											"		pharmacy_code,			institued_code,			"
											"		price_list_code,		member_id,				"
											"		mem_id_extension,		member_institued,		"
											"		doctor_id,				doctor_id_type,			"
											"		presc_source,			prescription_id,		"

											"		special_presc_num,		spec_pres_num_sors,		"
											"		date,					time,					"
											"		error_code,				prescription_lines,		"
											"		credit_type_code,		card_date,				"
											"		member_discount_pt,		terminal_id,			"

											"		receipt_num,			user_ident,				"
											"		presc_pharmacy_id,		macabi_code,			"
											"		doctor_presc_id,		reported_to_as400,		"
											"		comm_error_code,		member_price_code,		"
											"		doctor_insert_mode,		macabi_centers_num,		"

											"		doc_device_code,		diary_month,			"
											"		memb_prc_code_ph,		next_presc_start,		"
											"		next_presc_stop,		waiting_msg_flg,		"
											"		delivered_flg,			del_flg,				"
											"		reason_for_discnt,		reason_to_disp,			"

											"		credit_type_used,		origin_code,			"
											"		subsidy_amount,			action_type,			"
											"		insurance_type,			paid_for				"
											"	)													";
					NumInputColumns		=	46;
					InputColumnSpec		=	"	int, short, short, int, short, short, int, short, short, int,	"
											"	int, short, int, int, short, short, short, short, short, short,	"
											"	int, int, int, short, int, short, short, short, short, int,		"
											"	int, short, short, int, int, short, short, short, int, short,	"
											"	short, short, int, short, char(1), short						";
					GenerateVALUES		=	1;
					break;


		case TR2005spool_READ_check_that_pharmacy_sent_correct_values:
					SQL_CommandText		=	"	SELECT	pharmacy_code,	member_id,		date	"
											"	FROM	prescriptions							"
											"	WHERE	prescription_id = ?						";
					NumOutputColumns	=	3;
					OutputColumnSpec	=	"	int, int, int	";
					NumInputColumns		=	1;
					InputColumnSpec		=	"	int	";
					break;


		case TR2005_5005spool_READ_members:
					SQL_CommandText		=	"	SELECT	credit_type_code,	maccabi_code,		maccabi_until,		"
											"			spec_presc,			max_drug_date,		insurance_type,		"
											"			keren_mac_code,		mac_magen_code,		yahalom_code,		"
											"			keren_mac_from,		keren_mac_until,	mac_magen_from,		"
											"			mac_magen_until,	yahalom_from,		yahalom_until,		"
											"			died_in_hospital											"

											"	FROM	members														"

											"	WHERE	member_id			= ?										"
											"	  AND	mem_id_extension	= ?										"
											"	  AND	(del_flg			= 0 OR del_flg IS NULL)					";
					NumOutputColumns	=	16;
					OutputColumnSpec	=	"	short, short, int, short, int, char(1), short, short,	"
											"	short, int, int, int, int, int, int, short				";
					NumInputColumns		=	2;
					InputColumnSpec		=	"	int, short	";
					break;


		case TR2005_5005spool_READ_PD_macabi_price_flg:
					SQL_CommandText		=	"	SELECT	line_no,	macabi_price_flg	"

											"	FROM	prescription_drugs				"

											"	WHERE	prescription_id	= ?				"
											"	  AND	largo_code		= ?				";
					NumOutputColumns	=	2;
					OutputColumnSpec	=	"	short, short	";
					NumInputColumns		=	2;
					InputColumnSpec		=	"	int, int	";
					break;


		case TR2005spool_UPD_prescription_drugs:	// DonR 11Feb2020: Changed the input parameters for member_discount_pt
													// and price_extension from int to short. Both are INTEGERs in
													// prescription_drugs, but SMALLINTs elsewhere; and the host variables
													// for these values are short integers. Since we don't use any values
													// greater than 10000 (= 100%) for these numbers, there's no point in
													// using an int host variable; in fact, the database variables really
													// should be SMALLINTs.
					SQL_CommandText		=	"	UPDATE	prescription_drugs			"

											"	SET		op					= ?,	"
											"			units				= ?,	"
											"			delivered_flg		= ?,	"
											"			reported_to_as400	= ?,	"
											"			comm_error_code		= ?,	"
											"			op_stock			= ?,	"
											"			units_stock			= ?,	"
											"			total_drug_price	= ?,	"
											"			total_member_price	= ?,	"
											"			member_discount_pt	= ?,	"
											"			special_presc_num	= ?,	"
											"			spec_pres_num_sors	= ?,	"
											"			ingr_1_code			= ?,	"
											"			ingr_2_code			= ?,	"
											"			ingr_3_code			= ?,	"
											"			ingr_1_quant_bot	= ?,	"
											"			ingr_2_quant_bot	= ?,	"
											"			ingr_3_quant_bot	= ?,	"
											"			ingr_1_quant_std	= ?,	"
											"			ingr_2_quant_std	= ?,	"
											"			ingr_3_quant_std	= ?,	"
											"			date				= ?,	"
											"			time				= ?,	"
											"			macabi_price_flg	= ?,	"
											"			dr_largo_code		= ?,	"
											"			dr_presc_date		= ?,	"
											"			link_ext_to_drug	= ?,	"
											"			num_linked_rxes		= ?,	"
											"			paid_for			= ?		"

											"	WHERE	prescription_id	= ?			"
											"	  AND	line_no			= ?			";
					NumInputColumns		=	31;
//					InputColumnSpec		=	"	int, int, short, short, short, int, int, int, int, int,						"
					InputColumnSpec		=	"	int, int, short, short, short, int, int, int, int, short,					"
											"	int, short, short, short, short, double, double, double, double, double,	"
											"	double, int, int, short, int, int, int, short, short, int, short			";
					break;


		case TR2005spool_UPD_prescriptions:
					SQL_CommandText		=	"	UPDATE	prescriptions				"

											"	SET		receipt_num			= ?,	"
											"			user_ident			= ?,	"
											"			presc_pharmacy_id	= ?,	"
											"			doctor_insert_mode	= ?,	"
											"			macabi_centers_num	= ?,	"
											"			doc_device_code		= ?,	"
											"			diary_month			= ?,	"
											"			delivered_flg		= ?,	"
											"			credit_type_used	= ?,	"
											"			date				= ?,	"
											"			time				= ?,	"
											"			error_code			= ?,	"
											"			reason_for_discnt	= ?,	"
											"			reason_to_disp		= ?,	"
											"			comm_error_code		= ?,	"
											"			reported_to_as400	= ?,	"
											"			subsidy_amount		= ?,	"
											"			paid_for			= ?		"

											"	WHERE	prescription_id	= ?			";
					NumInputColumns		=	19;
					InputColumnSpec		=	"	int, int, int, short, int, int, short, short, short, int,	"
											"	int, short, int, short, short, short, int, short, int		";
					break;


		case TR2033_DEL_previous_ishur_version:
					SQL_CommandText		=	"	DELETE FROM	pharmacy_ishur			"
											"	WHERE		pharmacy_code	= ?		"
											"	  AND		pharm_ishur_num	= ?		"
											"	  AND		diary_month		= ?		"
											"	  AND		member_id		= ?		";
					NumInputColumns		=	4;
					InputColumnSpec		=	"	int, int, short, int	";
					break;


		case TR2033_INS_pharmacy_ishur:
					SQL_CommandText		=	"	INSERT INTO	pharmacy_ishur											"
											"	(																	"
											"		pharmacy_code,		institute_code,			terminal_id,		"
											"		diary_month,		ishur_date,				ishur_time,			"
											"		ishur_source,		pharm_ishur_num,		member_id,			"
											"		mem_id_extension,	authority_level,		auth_id_type,		"
											"		authority_id,		auth_first_name,		auth_last_name,		"

											"		as400_ishur_src,	as400_ishur_num,		as400_ishur_extend,	"
											"		line_number,		largo_code,				preferred_largo,	"
											"		member_price_code,	const_sum_to_pay,		rule_number,		"
											"		speciality_code,	treatment_category,		in_health_pack,		"
											"		ishur_text,			error_code,				used_flg,			"

											"		used_pr_id,			used_pr_line_num,		updated_date,		"
											"		updated_time,		updated_by_trn,			reported_to_as400,	"
											"		comm_error_code,	insurance_used,			no_presc_sale_flag,	"
											"		needs_29_g														"
											"	)																	";
					NumInputColumns		=	40;
					InputColumnSpec		=	"	int, short, short, short, int, int, short, int, int, short, short, short, int, varchar(8), varchar(14),	"
											"	short, int, short, short, int, int, short, int, int, int, short, short, varchar(80), short, short,		"
											"	int, short, int, int, short, short, short, short, short, short											";
					GenerateVALUES		=	1;
					break;


		case TR2033_READ_check_extendable_as400_ishur:
					SQL_CommandText		=	"	SELECT	CAST (count(*) AS INTEGER)							"

											"	FROM	special_prescs										"

											"	WHERE	member_id			=  ?							"
											"	  AND	mem_id_extension	=  ?							"

											"	  AND	((largo_code		=  ?)	OR						"
											"			 ((largo_code		=  ?) AND (largo_code > 0)))	"

											"	  AND	confirmation_type	=  1							"	// DonR 25Dec2007 - Type 4 no longer exists.
											"	  AND	abrupt_close_flg	=  0							"	// OK to extend
											"	  AND	spec_pres_num_sors	<> 5							"	// Not an automatic ishur.
											"	  AND	insurance_used		<> 2							"	// Not a Magen Zahav ishur.
					  						"	  AND	member_price_code	>  0							"	// Ishur actually gives participation.
											"	  AND	del_flg				=  0							";	// Not really necessary, as it's always true.
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					NumInputColumns		=	4;
					InputColumnSpec		=	"	int, short, int, int	";
					Convert_NF_to_zero	=	1;
					break;


		case TR2033_READ_check_if_ishur_already_used:
					SQL_CommandText		=	"	SELECT	CAST (COUNT(*) AS INTEGER)	"

											"	FROM	pharmacy_ishur				"

											"	WHERE	pharmacy_code	= ?			"
											"	  AND	pharm_ishur_num	= ?			"
											"	  AND	diary_month		= ?			"
											"	  AND	member_id		= ?			"
											"	  AND	used_flg		= ?			";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					NumInputColumns		=	5;
					InputColumnSpec		=	"	int, int, short, int, short	";
					Convert_NF_to_zero	=	1;
					break;


		case TR2033_READ_check_nonextendable_as400_ishur:
					SQL_CommandText		=	"	SELECT	CAST (count(*) AS INTEGER)							"

											"	FROM	special_prescs										"

											"	WHERE	member_id			=  ?							"
											"	  AND	mem_id_extension	=  ?							"

											"	  AND	((largo_code		=  ?)	OR						"
											"			 ((largo_code		=  ?) AND (largo_code > 0)))	"

											"	  AND	confirmation_type	=  1							"	// DonR 25Dec2007 - Type 4 no longer exists.
					  						"	  AND	member_price_code	>  0							"	// Ishur actually gives participation.
											"	  AND	del_flg				=  0							";	// Not really necessary, as it's always true.
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					NumInputColumns		=	4;
					InputColumnSpec		=	"	int, short, int, int	";
					Convert_NF_to_zero	=	1;
					break;


		case TR2033_READ_check_spclty_largo_prcnt:
					SQL_CommandText		=	"	SELECT	CAST (count(*) AS INTEGER)									"

											"	FROM	spclty_largo_prcnt											"

											"	WHERE	((largo_code		= ?)		OR							"
											"			 ((largo_code		= ?) AND (? > 0)))						"
											"	  AND	(del_flg			= 0 OR del_flg IS NULL)					"
											"	  AND	speciality_code		NOT BETWEEN 58000 AND 58999				"	// Exclude dentist stuff
											"	  AND	speciality_code		NOT BETWEEN  4000 AND  4999				"	// Exclude "home visit" stuff

												  // DonR 212Nov2011 - Add selection criteria for member type.
											"	  AND	(((enabled_mac	> 0) AND (?	> 0))	OR						"
											"			 ((enabled_hova	> 0) AND (?	> 0))	OR						"
											"			 ((enabled_keva	> 0) AND (?	> 0)))							"

												  // DonR 06Dec2012 "Yahalom" - New insurance-type and "vetek" criteria.
												  // Note that the price code/fixed price combination is now validated
												  // without regard to which insurance is applicable.
											"	  AND	(price_code > 1 OR (price_code = 1 AND fixed_price > 0))	"

											"	  AND	((insurance_type = 'B')						OR				"
											"			 (insurance_type = ? AND ? >= wait_months)	OR				"
											"			 (insurance_type = ? AND ? >= wait_months))					";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					NumInputColumns		=	10;
					InputColumnSpec		=	"	int, int, int, short, short, short, char(1), short, char(1), short	";
					Convert_NF_to_zero	=	1;
					break;


		case TR2033_READ_drug_extension:
					SQL_CommandText		=	"	SELECT	treatment_category,		health_basket_new,		wait_months,		"
											"			insurance_type,			price_code,				fixed_price,		"
											"			from_age,				to_age,					needs_29_g,			"
											"			enabled_mac,			enabled_hova,			enabled_keva,		"
											"			sex,					no_presc_sale_flag,		extend_rule_days	"

											"	FROM	drug_extension														"
			
											"	WHERE	((largo_code		=  ?)	OR										"
											"			 ((largo_code		=  ?) AND (largo_code > 0)))					"

											"	  AND	rule_number			=  ?											"

												  // DonR 20Nov2003 - Permission Type 6 ("Prati Plus") applies to Maccabi pharmacies as well.
											"	  AND	(permission_type	=  ?			OR								"
											"			 (permission_type	=  6 AND ? = 1)	OR								"
											"			 permission_type	=  2)											"

												  // DonR 06Dec2012 - Check that price code/fixed price combination is valid.
											"	  AND	(price_code			>  1 OR (price_code = 1 AND fixed_price > 0))	"

											"	  AND	(effective_date		<= ?		OR effective_date = 0)				"
											"	  AND	confirm_authority	<= ?											"

											// DonR 27Apr2025 User Story #390071: Make the maximum confirm_authority value
											// for pharmacy ishurim a sysparams parameter.
											"	  AND	confirm_authority	<= ?											"
//											"	  AND	confirm_authority	<  25											"

											"	  AND	send_and_use		<> 0											"
											"	  AND	del_flg				=  0											";
					NumOutputColumns	=	15;
					OutputColumnSpec	=	"	short, short, short, char(1), short, int, short, short, short,	"
											"	short, short, short, short, short, short						";
					NumInputColumns		=	8;	// DonR 27Apr2025 User Story #390071 7->8.
					InputColumnSpec		=	"	int, int, int, short, short, int, short, short	";
					break;


		case TR2033_READ_drug_list:
					SQL_CommandText		=	"	SELECT	package_size,			magen_wait_months,		"
											"			keren_wait_months,		economypri_group,		"
											"			specialist_drug,		preferred_flg,			"
											"			preferred_largo									"

											"	FROM	drug_list										"

											"	WHERE	largo_code	=  ?								"

												  // DonR 09Sep2009 - Added Assuta Pharmacy criteria for select.
											"	  AND	((del_flg <> ?) OR								"
											"			 (? = ? AND assuta_drug_status IN (?, ?)))		";
					NumOutputColumns	=	7;
					OutputColumnSpec	=	"	int, short, short, int, short, short, int	";
					NumInputColumns		=	6;
					InputColumnSpec		=	"	int, short, short, short, short, short	";
					break;


		case TR2033_READ_members:
					SQL_CommandText		=	"	SELECT	maccabi_code,			maccabi_until,								"
											"			mac_magen_code,			mac_magen_from,			mac_magen_until,	"
											"			keren_mac_code,			keren_mac_from,			keren_mac_until,	"
											"			keren_wait_flag,		spec_presc,				date_of_bearth,		"
											"			sex,					insurance_type,			carry_over_vetek,	"
											"			yahalom_code,			yahalom_from,			yahalom_until,		"
											"			force_100_percent_ptn,	died_in_hospital							"

											"	FROM	members																"

											// DonR 12Jan2020 - got rid of del_flg test, since it's never NULL and it's always zero.
											"	WHERE	member_id			= ?												"
											"	  AND	mem_id_extension	= ?												";
					NumOutputColumns	=	19;
					OutputColumnSpec	=	"	short, int, short, int, int, short, int, int, short, short	"
											"	int, short, char(1), int, short, int, int, short, short		";
					NumInputColumns		=	2;
					InputColumnSpec		=	"	int, short	";
					break;


		case TR2060_how_to_take_cur:
					SQL_CommandText		=	"	SELECT how_to_take_code, how_to_take_desc FROM how_to_take ORDER BY how_to_take_code	";
					NumOutputColumns	=	2;
					OutputColumnSpec	=	"	short, char(40)	";
					break;


		case TR2062_drug_shape_cur:
					SQL_CommandText		=	"	SELECT drug_shape_code, drug_shape_desc FROM drug_shape ORDER BY drug_shape_code	";
					NumOutputColumns	=	2;
					OutputColumnSpec	=	"	short, char(4)	";
					break;


		case TR2064_unit_of_measure_cur:
					SQL_CommandText		=	"	SELECT		unit_abbreviation,	short_desc_english,		short_desc_hebrew	"
											"	FROM		unit_of_measure													"
											"	WHERE		send_to_pharmacy > 0											"
											"	ORDER BY	unit_abbreviation												";
					NumOutputColumns	=	3;
					OutputColumnSpec	=	"	char(3), char(8), char(8)	";
					break;


		case TR2066_reason_not_sold_cur:
					SQL_CommandText		=	"	SELECT reason_code, reason_desc FROM reason_not_sold ORDER BY reason_code	";
					NumOutputColumns	=	2;
					OutputColumnSpec	=	"	short, char(50)	";
					break;


		case TR2068_discount_remarks_cur:
					SQL_CommandText		=	"	SELECT remark_code, remark_text FROM discount_remarks ORDER BY remark_code	";
					NumOutputColumns	=	2;
					OutputColumnSpec	=	"	short, char(25)	";
					break;


		case TR2070_economypri_cur:
					SQL_CommandText		=	"	SELECT	group_code,		group_nbr,		item_seq,				"
											"			largo_code,		del_flg,		received_date,			"
											"			received_time,	received_seq							"

											"	FROM	economypri												"
		
											"	WHERE	(((update_date		=  ?) AND (update_time >= ?)) OR	"
											"			  (update_date		>  ?))								"
											"	  AND	send_and_use_pharm	<> 0								"
		  
											"	ORDER BY	del_flg DESC,										"	// DonR 02Jan2012 - make sure we don't delete the valid stuff at pharmacies!
											"				received_date,										"
											"				received_time,										"
											"				received_seq										";
					NumOutputColumns	=	8;
					OutputColumnSpec	=	"	int, short, short, int, char(1), int, int, int	";
					NumInputColumns		=	3;
					InputColumnSpec		=	"	int, int, int	";
					break;


		case TR2072_prescr_source_cur:
					SQL_CommandText		=	"	SELECT		pr_src_code,		pr_src_desc,		"
											"				pr_src_docid_type,	pr_src_doc_device,	"
											"				pr_src_priv_pharm						"

											"	FROM		prescr_source							"
		
											"	ORDER BY	pr_src_code								";
					NumOutputColumns	=	5;
					OutputColumnSpec	=	"	short, char(25), short, short, short	";
					break;


		case TR2074_drug_extension_cur:
											// DonR 30Jul2023 User Story #468171: For private pharmacies, select only rows
											// with no_presc_sale_flag = 0. Add one input parameter: the private-pharmacy flag.
					SQL_CommandText		=	"	SELECT		largo_code,			rule_number,		effective_date,		"
											"				rule_name,			confirm_authority,	basic_price_code,	"
											"				kesef_price_code,	zahav_price_code,	basic_fixed_price,	"
											"				kesef_fixed_price,	del_flg,			health_basket_new	"

											"	FROM		drug_extension												"
		
											"	WHERE		(((changed_date	=  ?) AND (changed_time >= ?))	OR			"
											"				 (changed_date	>  ?))										"
											"	  AND		((no_presc_sale_flag = 0) OR (? = 0))						"	// DonR 30Jul2023.
											"	  AND		send_and_use	<> 0										"
		  
											"	ORDER BY	largo_code, rule_number										";
					NumOutputColumns	=	12;
					OutputColumnSpec	=	"	int, int, int, varchar(75), short, short, short, short, int, int, short, short	";
					NumInputColumns		=	4;
					InputColumnSpec		=	"	int, int, int, short	";	// DonR 30Jul2023 added new (short) private-pharmacy flag.
					break;


		case TR2076_confirm_authority_cur:
					SQL_CommandText		=	"	SELECT authority_code, authority_desc FROM confirm_authority ORDER BY authority_code	";
					NumOutputColumns	=	2;
					OutputColumnSpec	=	"	short, char(25)	";
					break;


		case TR2078_sale_cur:
					SQL_CommandText		=	"	SELECT		sale_number,		sale_name,			start_date,			"
											"				end_date,			sale_audience,		sale_type,			"
											"				min_op_to_buy,		op_to_receive,		min_purchase_amt,	"
											"				purchase_discount,	tav_knia_amt							"

											"	FROM		sale														"

											"	WHERE		(((update_date	=  ?) AND (update_time >= ?)) OR			"
											"				 update_date	>  ?)										"

											"	ORDER BY	sale_number													";
					NumOutputColumns	=	11;
					OutputColumnSpec	=	"	int, char(75), int, int, short, short, int, int, int, short, int	";
					NumInputColumns		=	3;
					InputColumnSpec		=	"	int, int, int	";
					break;


		case TR2078_sale_bonus_recv_sale_NOT_type_14_cur:
					SQL_CommandText		=	"	SELECT		sale_number,	largo_code,		discount_percent,	sale_price					"

											"	FROM		sale_bonus_recv																	"

											"	WHERE		sale_number IN	(	SELECT	sale_number											"
											"									FROM	sale												"
											"									WHERE	(((update_date	=   ?) AND (update_time >= ?)) OR	"
											"											 update_date	>   ?)								"
											"									  AND	sale_type		<> 14								"
											"								)																"

											"	ORDER BY	sale_number, largo_code															";
					NumOutputColumns	=	4;
					OutputColumnSpec	=	"	int, int, short, int	";
					NumInputColumns		=	3;
					InputColumnSpec		=	"	int, int, int	";
					break;


		case TR2078_sale_bonus_recv_sale_type_14_cur:
					SQL_CommandText		=	"	SELECT		sale_number,		largo_code,		sale_price									"

											"	FROM		sale_bonus_recv																	"

											"	WHERE		sale_number IN	(	SELECT	sale_number											"
											"									FROM	sale												"
											"									WHERE	(((update_date	=  ?) AND (update_time >= ?)) OR	"
											"											 update_date	>  ?)								"
											"									  AND	sale_type		=  14								"
											"								)																"

											"	ORDER BY	sale_number, largo_code															";
					NumOutputColumns	=	3;
					OutputColumnSpec	=	"	int, int, int	";
					NumInputColumns		=	3;
					InputColumnSpec		=	"	int, int, int	";
					break;


		case TR2078_sale_fixed_price_cur:
					SQL_CommandText		=	"	SELECT		sale_number,	largo_code,	sale_price											"

											"	FROM		sale_fixed_price																"

											"	WHERE		sale_number IN	(	SELECT	sale_number											"
											"									FROM	sale												"
											"									WHERE	(((update_date	=  ?) AND (update_time >= ?)) OR	"
											"											 update_date	>  ?)								"
											"								)																"

											"	ORDER BY	sale_number, largo_code;														";
					NumOutputColumns	=	3;
					OutputColumnSpec	=	"	int, int, int	";
					NumInputColumns		=	3;
					InputColumnSpec		=	"	int, int, int	";
					break;


		case TR2078_sale_target_cur:
					SQL_CommandText		=	"	SELECT		sale_number,	pharmacy_type,	pharmacy_size,	target_op						"

											"	FROM		sale_target																		"

											"	WHERE		sale_number IN	(	SELECT	sale_number											"
											"									FROM	sale												"
											"									WHERE	(((update_date	=  ?) AND (update_time >= ?)) OR	"
											"											 update_date	>  ?)								"
											"								)																"

											"	ORDER BY	sale_number																		";
					NumOutputColumns	=	4;
					OutputColumnSpec	=	"	int, short, short, int	";
					NumInputColumns		=	3;
					InputColumnSpec		=	"	int, int, int	";
					break;


		case TR2080_gadgets_cur:
					SQL_CommandText		=	"	SELECT		gadget_code,		service_desc,		largo_code,		"
											"				service_code,		service_number,		service_type	"

											"	FROM		gadgets													"
		
											"	ORDER BY	largo_code, gadget_code									";
					NumOutputColumns	=	6;
					OutputColumnSpec	=	"	int, varchar(25), int, int, short, short	";
					break;


		case TR2082_pharmdrugnotes_cur:
					SQL_CommandText		=	"	SELECT		gnrldrugnotecode,	del_flg,			gdn_category,		"
											"				gdn_connect_type,	gdn_connect_desc,	gnrldrugnotetype,	"
											"				gdn_seq_num,		gdn_severity,		gnrldrugnote,		"
											"				update_date,		update_time								"

											"	FROM		pharmdrugnotes												"
		
											"	WHERE		(((update_date	=  ?) AND (update_time >= ?)) OR			"
											"				   update_date	>  ?)										"
		  
											"	ORDER BY	update_date,	update_time									";
					NumOutputColumns	=	11;
					OutputColumnSpec	=	"	int, char(1), short, short, varchar(25), char(1), short, short, varchar(50), int, int	";
					NumInputColumns		=	3;
					InputColumnSpec		=	"	int, int, int	";
					break;


		case TR2084_drugnotes_cur:
					SQL_CommandText		=	"	SELECT		largo_code,		gnrldrugnotecode,	del_flg,		"
											"				update_date,	update_time							"

											"	FROM		drugnotes											"
		
											"	WHERE		(((update_date	=  ?) AND (update_time >= ?)) OR	"
											"				   update_date	>  ?)								"
		  
											"	ORDER BY	update_date,	update_time							";
					NumOutputColumns	=	5;
					OutputColumnSpec	=	"	int, int, char(1), int, int	";
					NumInputColumns		=	3;
					InputColumnSpec		=	"	int, int, int	";
					break;


		case TR2086_pharmacy_daily_sum_date_cur:
					SQL_CommandText		=	"	SELECT DISTINCT	date				"

											"	FROM		pharmacy_daily_sum		"

											"	WHERE		pharmacy_code	= ?		"
											"	  AND		institued_code	= ?		"
											"	  AND		diary_month		= ?		"

											"	ORDER BY	date					";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					NumInputColumns		=	3;
					InputColumnSpec		=	"	int, short, short	";
					break;


		case TR2086_READ_pharmacy_daily_sum_one_day_totals:
											// DonR 14May2020: SQL operations returning a SUM seem to be somewhat unstable,
											// so I'm trying an explicit CAST to INTEGER to see if that helps.
					SQL_CommandText		=	"	SELECT		CAST (SUM (presc_count)			AS INTEGER),	"
											"				CAST (SUM (presc_lines_count)	AS INTEGER),	"
											"				CAST (SUM (sum)					AS INTEGER),	"
											"				CAST (SUM (member_part_sum)		AS INTEGER),	"
											"				CAST (SUM (purchase_sum)		AS INTEGER),	"
											"				CAST (SUM (deleted_sales)		AS INTEGER),	"
											"				CAST (SUM (delete_lines)		AS INTEGER),	"
											"				CAST (SUM (delete_sum)			AS INTEGER),	"
											"				CAST (SUM (del_member_prt_sum)	AS INTEGER),	"
											"				CAST (SUM (del_purchase_sum)	AS INTEGER)		"

											"	FROM		pharmacy_daily_sum			"

											"	WHERE		pharmacy_code	= ?			"
											"	  AND		institued_code	= ?			"
											"	  AND		diary_month		= ?			"
											"	  AND		date			= ?			"
					  
											"	GROUP BY	pharmacy_code				";	// DonR 13Jan2020 - Is "GROUP BY" really needed here?
					NumOutputColumns	=	10;
					OutputColumnSpec	=	"	int, int, int, int, int, int, int, int, int, int	";
					NumInputColumns		=	4;
					InputColumnSpec		=	"	int, short, short, int	";
					break;


		case TR2088_prescription_drugs_cur:	// (SQL code is generated dynamically.)
											//	SELECT	prescription_id,	largo_code,			total_drug_price,
											//			total_member_price,	price_for_op_ph,	del_flg,
											//			delivered_flg,		op,					units
											//
											//	FROM	prescription_drugs...
					SQL_CommandText		=	NULL;	// Variable SQL text set up by calling routine.
					NumOutputColumns	=	9;
					OutputColumnSpec	=	"	int, int, int, int, int, short, short, int, int	";
					break;


											// Added "AND delivered_flg <> 0". Informix had a problem with performance with
											// some WHERE criteria, but MS-SQL should be smarter - and adding this one
											// condition should cut down the volume selected by around 15%.
		case TR2088_prescriptions_cur:
					SQL_CommandText		=	"	SELECT		date,				member_id,			mem_id_extension,	"
											"				member_institued,	prescription_id,	credit_type_used,	"
											"				member_price_code,	presc_pharmacy_id,	diary_month,		"

											"				delivered_flg,		presc_source,		tikra_discount,		"
											"				subsidy_amount,		del_presc_id,		del_pharm_code,		"
											"				del_flg														"

											"	FROM		prescriptions												"

											"	WHERE		pharmacy_code		=  ?									"
											"	  AND		date				=  ?									"
											"	  AND		delivered_flg		<> 0									";
					NumOutputColumns	=	16;
					OutputColumnSpec	=	"	int, int, short, short, int, short, short, int, short,	"
											"	short, short, int, int, int, int, short					";
					NumInputColumns		=	2;
					InputColumnSpec		=	"	int, int	";
					break;


		case TR2088_sale_deletions_cur:
					SQL_CommandText		=	"	SELECT	total_drug_price,	total_member_price,		price_for_op_ph,	"
											"			largo_code,			op,						units				"

											"	FROM	prescriptions AS p, prescription_drugs as d						"

											"	WHERE	p.del_presc_id		= ?											"
											"	  AND	p.pharmacy_code		= ?											"	// DonR 06Mar2019 CR #28230
											"	  AND	d.prescription_id	= p.prescription_id							"
											"	  AND	d.del_flg			= 0											"	// DonR 31Jul2013 - 2 = redundant, 0 = "real".
											"	  AND	d.delivered_flg		> 0											";
					NumOutputColumns	=	6;
					OutputColumnSpec	=	"	int, int, int, int, int, int	";
					NumInputColumns		=	2;
					InputColumnSpec		=	"	int, int	";
					break;


		case TR2088_READ_presc_delete:
											// DonR 14May2020: SQL operations returning a SUM seem to be somewhat unstable,
											// so I'm trying an explicit CAST to INTEGER to see if that helps.
											// DonR 01Jun2022: As of code changes I'm making today, this query - like the
											// presc_delete table - is no longer in use.
					SQL_CommandText		=	"	SELECT	CAST (COUNT (*)				AS INTEGER),	"
											"			CAST (SUM (delete_lines)	AS INTEGER)		"
											"	FROM	presc_delete								"
											"	WHERE	prescription_id = ?							"
											"	  AND	pharmacy_code	= ?							";
					NumOutputColumns	=	2;
					OutputColumnSpec	=	"	int, int	";
					NumInputColumns		=	2;
					InputColumnSpec		=	"	int, int	";
					break;


		case TR2088_READ_sum_of_deletions_of_a_sale:
											// DonR 14May2020: SQL operations returning a SUM seem to be somewhat unstable,
											// so I'm trying an explicit CAST to INTEGER to see if that helps.
											// DonR 01Jun2022: Changed the delivered_flg test from "> 0" to "= 2".
											// This shouldn't change anything, but it's a little easier for humans
											// to understand.
					SQL_CommandText		=	"	SELECT	CAST (COUNT (*)					AS INTEGER),		"
											"			CAST (SUM (tikra_discount)		AS INTEGER),		"
											"			CAST (SUM (subsidy_amount)		AS INTEGER),		"
											"			CAST (SUM (prescription_lines)	AS INTEGER)			"

											"	FROM	prescriptions										"

											"	WHERE	del_presc_id	= ?									"
											"	  AND	pharmacy_code	= ?									"	// DonR 06Mar2019 CR #28230
//											"	  AND	delivered_flg	> 0									"
											"	  AND	delivered_flg	= 2									"	// DonR 01Jun2022 - changed from "> 0".
											"	  AND	del_flg			= 0									";	// DonR 31Jul2013 - 2 = redundant, 0 = "real".
					NumOutputColumns	=	4;
					OutputColumnSpec	=	"	int, int, int, int	";
					NumInputColumns		=	2;
					InputColumnSpec		=	"	int, int	";
					break;


		case TR2090_prescription_drugs_cur:
					SQL_CommandText		=	"	SELECT		line_no,				largo_code,			units,				"
											"				op,						link_ext_to_drug,	price_for_op,		"
											"				price_for_op_ph,		member_price_code,	price_replace,		"
											"				member_discount_pt,		total_drug_price,	total_member_price,	"
											"				del_flg															"

											"	FROM		prescription_drugs												"

											"	WHERE		prescription_id		= ?											"

											"	ORDER BY	line_no															";
					NumOutputColumns	=	13;
					OutputColumnSpec	=	"	short, int, int, int, int, int, int, short, int, int, int, int, short	";
					NumInputColumns		=	1;
					InputColumnSpec		=	"	int	";
					break;


		case TR2090_READ_old_deletion_tables:
											// DonR 14May2020: I saw a log error for this operation. These tables should actually
											// be dead and buried already; but the fix - I think - is to specify which table each
											// output field is coming from.
					SQL_CommandText		=	"	SELECT		dlt.deletion_id,	dlt.date,					"
											"				dlt.time,			dlt.receipt_delete_id,		"
											"				dlt.terminal_id								"

											"	FROM		presc_delete	AS dlt,					"
											"				del_presc_drugs	AS dld					"

											"	WHERE		dlt.prescription_id	= ?					"
											"	  AND		dld.prescription_id	= ?					"
											"	  AND		dld.largo_code		= ?					"
											"	  AND		dld.deletion_id		= dlt.deletion_id	";
					NumOutputColumns	=	5;
					OutputColumnSpec	=	"	int, int, int, int, short	";
					NumInputColumns		=	3;
					InputColumnSpec		=	"	int, int, int	";
					break;


		case TR2090_READ_prescriptions:
					SQL_CommandText		=	"	SELECT		member_id,			mem_id_extension,		member_institued,		"
											"				doctor_id_type,		doctor_id,				presc_source,			"
											"				presc_pharmacy_id,	prescription_id,		date,					"
											"				time,				credit_type_used,		receipt_num,			"

											"				terminal_id,		macabi_centers_num,		doc_device_code,		"
											"				diary_month,		tikra_discount,			subsidy_amount,			"
											"				del_presc_id,		del_pharm_code,			del_pharm_presc_id,		"
											"				del_sale_date,		action_type,			prescription_lines		"

											"	FROM		prescriptions														"

											"	WHERE		%s																	";
					NumOutputColumns	=	24;
					OutputColumnSpec	=	"	int, short, short, short, int, short, int, int, int, int, short, int,	"
											"	short, int, int, short, int, int, int, int, int, int, short, short		";
					NeedsWhereClauseID	=	1;
					break;


		// Marianna 15Oct2020 CR #32420		
		case TR2096_usage_instruct_cur:
					SQL_CommandText		=	"	SELECT	shape_num,		shape_code,		shape_desc,			inst_code,			inst_msg,			"
											"			inst_seq,		calc_op_flag,	unit_code,			unit_seq,			unit_name,			"
											"			unit_desc,		open_od_window,	concentration_flag,	total_unit_name,	round_units_flag,	"
											"			update_date,	update_time,	updatedby,			del_flg"
											
											"	FROM usage_instruct																					"
											"	ORDER BY shape_num, inst_code, inst_seq, unit_code	";
					
					NumOutputColumns	=	19;
					OutputColumnSpec	=	"	short, char(4), char(25) , short, char(40),		"
											"	short, short, char(3), short, char(8),			"
											"	char(25), short, short, char(10), short,		"
											"	int, int, int, char(1)							";
					break;


		// Marianna 18Oct2020 CR #32420		
		case TR2101_usage_instr_reason_changed_cur:
					SQL_CommandText		=	"	SELECT 	reason_changed_code,	reason_short_desc,	reason_long_desc,	"
											"			update_date,			update_time								"
											
											"	FROM usage_instr_reason_changed 	"
											"	ORDER BY reason_changed_code		";
					
					NumOutputColumns	=	5;
					OutputColumnSpec	=	"	short, char(30), char(50) , int, int";
					break;			


		case TR5003_deldrugs_cur:
					SQL_CommandText		=	"	SELECT	largo_code,				op,						units,					"
											"			total_member_price,		tikrat_mazon_flag,		member_price_code,		"
											"			particip_method,		special_presc_num,		spec_pres_num_sors,		"
											"			ishur_with_tikra,		ishur_tikra_type,		price_for_op,			"
											"			price_for_op_ph,		price_extension,		price_replace,			"
											"			rule_number,			updated_by,				prescr_wr_rrn,			"
											"			doctor_presc_id,		dr_presc_date,			dr_largo_code			"

											"	FROM	prescription_drugs														"

											"	WHERE	prescription_id	= ?														"
											"	  AND	delivered_flg	= ?														"	// Only drug sales are relevant here.
											"	  AND	del_flg			= ?														";
					NumOutputColumns	=	21;
					OutputColumnSpec	=	"	int, int, int, int, short, short, short, int, short, short, short,	"
											"	int, int, int, int, int, short, int, int, int, int					";	// DonR 24Mar2020: updated_by is read into a short integer (in-health-basket).
					NumInputColumns		=	3;
					InputColumnSpec		=	"	int, short, short	";
					break;


		case TR5003_INS_prescription_drugs:
					SQL_CommandText		=	"	INSERT INTO prescription_drugs								"
											"	(															"
											"		prescription_id,				line_no,				"
											"		member_id,						mem_id_extension,		"
											"		largo_code,						reported_to_as400,		"
											"		op,								units,					"
											"		duration,						stop_use_date,			"

											"		date,							time,					"
											"		del_flg,						delivered_flg,			"
											"		price_replace,					price_extension,		"
											"		link_ext_to_drug,				price_for_op,			"
											"		action_type,					drug_answer_code,		"

											"		member_price_code,				calc_member_price,		"
											"		presc_source,					macabi_price_flg,		"
											"		pharmacy_code,					institued_code,			"
											"		price_for_op_ph,				stop_blood_date,		"
											"		total_drug_price,				op_stock,				"

											"		units_stock,					total_member_price,		"
											"		comm_error_code,				t_half,					"
											"		doctor_id,						doctor_id_type,			"
											"		doctor_presc_id,				dr_presc_date,			"
											"		dr_largo_code,					subst_permitted,		"

											"		units_unsold,					op_unsold,				"
											"		units_per_dose,					doses_per_day,			"
											"		particip_method,				prescr_wr_rrn,			"
											"		updated_by,						special_presc_num,		"
											"		spec_pres_num_sors,				ishur_with_tikra,		"

											"		ishur_tikra_type,				op_prescribed,			"
											"		units_prescribed,				rule_number,			"
											"		tikra_type_code,				subsidized,				"
											"		tikrat_mazon_flag,				member_ptn_amt,			"
											"		elect_pr_status,				credit_type_code,		"

											"		credit_type_used,				why_future_sale_ok,		"
											"		qty_limit_chk_type,				qty_lim_ishur_num,		"
											"		vacation_ishur_num,				dr_visit_date,			"
											"		doctor_facility,				member_discount_amt,	"
											"		qty_limit_class_code									"
											"	)															";
					NumInputColumns		=	69;
					InputColumnSpec		=	"	int, short, int, short, int, short, int, int, short, int,		"	// DonR 20Feb2020: Duration is populated from a short int.
											"	int, int, short, short, int, int, int, int, short, short,		"
											"	short, short, short, short, int, short, int, int, int, int,		"
											"	int, int, short, short, int, short, int, int, int, short,		"	// DonR 11Mar2020: T_half is an integer in prescription_drugs, but is populated by a short. (Change column type in MS-SQL!)
											"	int, int, int, short, short, int, short, int, short, short,		"	// DonR 20Feb2020: "updated_by" is populated from in_health_pack, a short int. Doses_per_day is also populated from a short int.
											"	short, int, int, int, char(1), short, short, int, short, short,	"	// DonR 09Jul2020: BUG FIX: char(0) in input columns isn't supported, so tikra_type_code wasn't being written!
											"	short, short, short, int, int, int, int, int, short				";
					GenerateVALUES		=	1;
					break;


		case TR5003_READ_prescription_drugs_rows_to_delete:
					SQL_CommandText		=	"	SELECT	op,						units,					total_member_price,	"
											"			tikrat_mazon_flag,		member_price_code,		del_flg,			"
											"			particip_method,		special_presc_num,		spec_pres_num_sors,	"
											"			ishur_with_tikra,		ishur_tikra_type,		price_for_op,		"
											"			price_for_op_ph,		price_extension,		price_replace,		"
											"			rule_number,			updated_by,				prescr_wr_rrn,		"
											"			doctor_presc_id,		dr_presc_date,			dr_largo_code		"

											"	FROM	prescription_drugs													"

											"	WHERE	prescription_id	= ?													"
											"	  AND	delivered_flg	= ?													"	// Only drug sales are relevant here.
											"	  AND	largo_code		= ?													";
					NumOutputColumns	=	21;
					OutputColumnSpec	=	"	int, int, int, short, short, short, short, int, short, short, short,	"
											"	int, int, int, int, int, short, int, int, int, int						";	// DonR 24Mar2020: updated_by is read into health-basket, a short int.
					NumInputColumns		=	3;
					InputColumnSpec		=	"	int, short, int	";
					break;


		case TR5005_presc_drug_cur:
					SQL_CommandText		=	"	SELECT	largo_code,			op,					units,				"
											"			line_no,			link_ext_to_drug,	price_for_op,		"
											"			price_for_op_ph,	drug_answer_code,	stop_use_date,		"
											"			t_half,				prescr_wr_rrn,		doctor_presc_id,	"
											"			particip_method,	member_ptn_amt,		macabi_price_flg,	"
											"			stop_blood_date,	dr_visit_date,		dr_presc_date,		"
											"			dr_largo_code												"

											"	FROM	prescription_drugs											"

											"	WHERE	prescription_id	= ?											"
											"	  AND	delivered_flg   = ?											";
					NumOutputColumns	=	19;
					OutputColumnSpec	=	"	int, int, int, short, int, int, int, short, int, short,	"	// DonR 11Mar2020: t_half is an INTEGER in Informix, but should be a SMALLINT - it's populated by short's and read into short's.
											"	int, int, short, int, short, int, int, int, int			";
					NumInputColumns		=	2;
					InputColumnSpec		=	"	int, short	";
					break;


		case TR5005_UPD_prescription_drugs:
					SQL_CommandText		=	"	UPDATE	prescription_drugs			"

											"	SET		op					= ?,	"
											"			units				= ?,	"
											"			delivered_flg		= ?,	"	// Deletions are "delivered" with flg of 2!
											"			reported_to_as400	= ?,	"
											"			elect_pr_status		= ?,	"	// DonR 07Jul2015 - added for compatibility with Mersham Digitali.
											"			op_stock			= ?,	"
											"			units_stock			= ?,	"
											"			total_drug_price	= ?,	"
											"			total_member_price	= ?,	"
											"			member_discount_pt	= ?,	"
											"			price_for_op_ph		= ?,	"
											"			discount_granted	= ?,	"
											"			discount_code		= ?,	"
											"			ingr_1_code			= ?,	"
											"			ingr_2_code			= ?,	"
											"			ingr_3_code			= ?,	"
											"			ingr_1_quant_bot	= ?,	"
											"			ingr_2_quant_bot	= ?,	"
											"			ingr_3_quant_bot	= ?,	"
											"			ingr_1_quant_std	= ?,	"
											"			ingr_2_quant_std	= ?,	"
											"			ingr_3_quant_std	= ?,	"
											"			date				= ?,	"
											"			time				= ?,	"
											"			macabi_price_flg	= ?,	"
											"			dr_largo_code		= ?,	"
											"			dr_presc_date		= ?,	"
											"			link_ext_to_drug	= ?,	"
											"			num_linked_rxes		= ?,	"
											"			paid_for			= ?		"

											"	WHERE	prescription_id	= ?			"
											"	  AND	line_no			= ?			";
					NumInputColumns		=	32;
					InputColumnSpec		=	"	int, int, short, short, short, int, int, int, int, int, int,				"
											"	int, short, short, short, short, double, double, double, double, double,	"
											"	double, int, int, short, int, int, int, short, short, int, short			";
					break;


		case TR5005_UPD_prescriptions:
					SQL_CommandText		=	"	UPDATE	prescriptions				"

											"	SET		receipt_num			= ?,	"
											"			user_ident			= ?,	"
											"			presc_pharmacy_id	= ?,	"
											"			doctor_insert_mode	= ?,	"	// May disappear in future!
											"			macabi_centers_num	= ?,	"
											"			doc_device_code		= ?,	"
											"			diary_month			= ?,	"
											"			delivered_flg		= ?,	"	// Deletions are "delivered" with flg of 2!
											"			reported_to_as400	= ?,	"
											"			credit_type_used	= ?,	"
											"			date				= ?,	"
											"			time				= ?,	"
											"			error_code			= ?,	"	// DonR 24Jul2013 - the previous value unless we hit a new error.
											"			reason_for_discnt	= ?,	"
											"			credit_reason_code	= ?,	"
											"			num_payments		= ?,	"
											"			payment_method		= ?,	"
											"			tikra_discount		= ?,	"
											"			subsidy_amount		= ?,	"
											"			elect_pr_status		= ?,	"
											"			reason_to_disp		= ?,	"
											"			member_price_code	= ?,	"
											"			sale_req_error		= ?,	"
											"			paid_for			= ?		"

											"	WHERE	prescription_id	= ?			";
					NumInputColumns		=	25;
					InputColumnSpec		=	"	int, int, int, short, int, int, short, short, short, short,		"
											"	int, int, short, int, short, short, short, int, int, short,		"
											"	short, short, short, short, int	";
					break;


		case TR5005spool_deldrugs_cur:
					SQL_CommandText		=	"	SELECT	largo_code,				op,					units,					"
											"			total_member_price,		member_price_code,	member_discount_pt,		"
											"			particip_method,		price_for_op,		price_for_op_ph,		"

											"			price_extension,		price_replace,		updated_by,				"
											"			date,					total_drug_price,	discount_granted,		"
											"			discount_code,			tikrat_mazon_flag,	special_presc_num,		"

											"			spec_pres_num_sors,		ishur_with_tikra,	ishur_tikra_type,		"
											"			rule_number,			doctor_presc_id,	dr_presc_date,			"
											"			dr_largo_code,			prescr_wr_rrn								"

											"	FROM	prescription_drugs													"

											"	WHERE	prescription_id	= ?													"
											"	  AND	delivered_flg	= ?													"	// Only drug sales are relevant here.
											"	  AND	del_flg			= ?													";
					NumOutputColumns	=	26;
					OutputColumnSpec	=	"	int, int, int, int, short, int, short, int, int,	"
											"	int, int, short, int, int, int, short, short, int,	"	// DonR 24Mar2020: updated_by is read into health-basket, a short int!
											"	short, short, short, int, int, int, int, int		";
					NumInputColumns		=	3;
					InputColumnSpec		=	"	int, short, short	";
					break;


		case TR5005spool_INS_prescription_drugs:	// DonR 11Feb2020: Changed the input parameters for member_discount_pt
													// and price_extension from int to short. Both are INTEGERs in
													// prescription_drugs, but SMALLINTs elsewhere; and the host variables
													// for these values are short integers. Since we don't use any values
													// greater than 10000 (= 100%) for these numbers, there's no point in
													// using an int host variable; in fact, the database variables really
													// should be SMALLINTs.
					SQL_CommandText		=	"	INSERT INTO	prescription_drugs								"
											"	(															"
											"		prescription_id,					line_no,			"
											"		member_id,							mem_id_extension,	"
											"		largo_code,							reported_to_as400,	"
											"		op,									units,				"
											"		duration,							stop_use_date,		"

											"		date,								time,				"
											"		del_flg,							delivered_flg,		"
											"		price_replace,						price_extension,	"
											"		link_ext_to_drug,					price_for_op,		"
											"		action_type,						drug_answer_code,	"

											"		member_price_code,					calc_member_price,	"
											"		presc_source,						macabi_price_flg,	"
											"		pharmacy_code,						institued_code,		"
											"		price_for_op_ph,					stop_blood_date,	"
											"		total_drug_price,					op_stock,			"

											"		units_stock,						total_member_price,	"
											"		member_ptn_amt,						comm_error_code,	"
											"		t_half,								doctor_id,			"
											"		doctor_id_type,						updated_by,			"
											"		special_presc_num,					spec_pres_num_sors,	"

											"		doctor_presc_id,					dr_presc_date,		"
											"		dr_largo_code,						subst_permitted,	"
											"		units_per_dose,						doses_per_day,		"
											"		member_discount_pt,					particip_method,	"
											"		prescr_wr_rrn,						ingr_1_code,		"

											"		ingr_2_code,						ingr_3_code,		"
											"		ingr_1_quant_bot,					ingr_2_quant_bot,	"
											"		ingr_3_quant_bot,					ingr_1_quant_std,	"
											"		ingr_2_quant_std,					ingr_3_quant_std,	"
											"		discount_granted,					discount_code,		"

											"		tikrat_mazon_flag,					ishur_with_tikra,	"
											"		ishur_tikra_type,					rule_number,		"
											"		credit_type_code,					elect_pr_status,	"

											"		why_future_sale_ok,					qty_limit_chk_type,	"
											"		qty_lim_ishur_num,					vacation_ishur_num,	"
											"		dr_visit_date,						doctor_facility,	"
											"		num_linked_rxes,					paid_for			"
											"	)															";
					NumInputColumns		=	74;
					InputColumnSpec		=	"	int, short, int, short, int, short, int, int, short, int,					"	// DonR 20Feb2020: Duration is populated from a short int.
											"	int, int, short, short, int, short, int, int, short, short,					"
											"	short, short, short, short, int, short, int, int, int, int,					"
											"	int, int, int, short, short, int, short, short, int, short,					"	// DonR 20Feb2020: updated_by is actually in_health_basket, a short int.
																																// DonR 11Mar2020: T_half is an integer in prescription_drugs, but is populated by a short. (Change column type in MS-SQL!)
											"	int, int, int, short, int, short, short, short, int, short,					"	// DonR 20Feb2020: doses_per_day is populated from a short int.
											"	short, short, double, double, double, double, double, double, int, short,	"
											"	short, short, short, int, short, short,										"
											"	short, short, int, int, int, int, short, short								";
					GenerateVALUES		=	1;
					break;


		case TR5005spool_INS_prescriptions:
					SQL_CommandText		=	"	INSERT INTO prescriptions							"
											"	(													"
											"		pharmacy_code,			institued_code,			"
											"		price_list_code,		member_id,				"
											"		mem_id_extension,		member_institued,		"
											"		doctor_id,				doctor_id_type,			"
											"		presc_source,			prescription_id,		"

											"		special_presc_num,		spec_pres_num_sors,		"
											"		date,					time,					"
											"		error_code,				prescription_lines,		"
											"		credit_type_code,		card_date,				"
											"		member_discount_pt,		terminal_id,			"

											"		receipt_num,			user_ident,				"
											"		presc_pharmacy_id,		macabi_code,			"
											"		doctor_presc_id,		reported_to_as400,		"
											"		comm_error_code,		member_price_code,		"
											"		doctor_insert_mode,		macabi_centers_num,		"

											"		doc_device_code,		diary_month,			"
											"		memb_prc_code_ph,		next_presc_start,		"
											"		next_presc_stop,		waiting_msg_flg,		"
											"		delivered_flg,			del_flg,				"
											"		reason_for_discnt,		reason_to_disp,			"

											"		action_type,			del_presc_id,			"
											"		del_pharm_code,			del_sale_date,			"
											"		del_pharm_presc_id,		card_owner_id,			"
											"		card_owner_id_code,		tikra_called_flag,		"
											"		tikra_error_code,		origin_code,			"

											"		credit_type_used,		credit_reason_code,		"
											"		num_payments,			payment_method,			"
											"		tikra_discount,			subsidy_amount,			"
											"		elect_pr_status,		tikra_status_code,		"
											"		sale_req_error,			insurance_type,			"
											"		paid_for										"
											"	)													";
					NumInputColumns		=	61;
					InputColumnSpec		=	"	int, short, short, int, short, short, int, short, short, int,		"
											"	int, short, int, int, short, short, short, short, short, short,		"
											"	int, int, int, short, int, short, short, short, short, int,			"
											"	int, short, short, int, int, short, short, short, int, short,		"
											"	short, int, int, int, int, int, short, short, short, short,			"
											"	short, short, short, short, int, int, short, short, short, char(1),	"
											"	short		";
					GenerateVALUES		=	1;
					break;


		case TR5005spool_READ_deleted_prescription_drugs_values:
					SQL_CommandText		=	"	SELECT	op,						units,					total_member_price,		"
											"			member_price_code,		member_discount_pt,		particip_method,		"
											"			price_for_op,			price_for_op_ph,		price_extension,		"
											"			price_replace,			updated_by,				date,					"

											"			total_drug_price,		discount_granted,		discount_code,			"
											"			tikrat_mazon_flag,		special_presc_num,		spec_pres_num_sors,		"
											"			ishur_with_tikra,		ishur_tikra_type,		rule_number,			"
											"			doctor_presc_id,		dr_presc_date,			dr_largo_code,			"
											"			prescr_wr_rrn															"

											"	FROM	prescription_drugs														"

											"	WHERE	prescription_id	= ?														"
											"	  AND	delivered_flg	= ?														"	// Only drug sales are relevant here.
											"	  AND	largo_code		= ?														";
					NumOutputColumns	=	25;
					OutputColumnSpec	=	"	int, int, int, short, int, short, int, int, int, int, short, int,			"	// DonR 24Mar2020: updated_by is read into health-basket, a short int!
											"	int, int, short, short, int, short, short, short, int, int, int, int, int	";
					NumInputColumns		=	3;
					InputColumnSpec		=	"	int, short, int	";
					break;
	

		case TR5005spool_READ_deleted_prescriptions_row_values:
					SQL_CommandText		=	"	SELECT	subsidy_amount,		tikra_discount,			"
											"			presc_source,		doctor_id_type,			"
											"			doctor_id,			macabi_centers_num,		"
											"			del_flg										"

											"	FROM	prescriptions								"

											"	WHERE	prescription_id	= ?							";
					NumOutputColumns	=	7;
					OutputColumnSpec	=	"	int, int, short, short, int, int, short	";
					NumInputColumns		=	1;
					InputColumnSpec		=	"	int	";
					break;


		case TR5005spool_UPD_prescription_drugs:	// DonR 11Feb2020: Changed the input parameters for member_discount_pt
													// and price_extension from int to short. Both are INTEGERs in
													// prescription_drugs, but SMALLINTs elsewhere; and the host variables
													// for these values are short integers. Since we don't use any values
													// greater than 10000 (= 100%) for these numbers, there's no point in
													// using an int host variable; in fact, the database variables really
													// should be SMALLINTs.
					SQL_CommandText		=	"	UPDATE	prescription_drugs			"

											"	SET		op					= ?,	"
											"			units				= ?,	"
											"			delivered_flg		= ?,	"
											"			reported_to_as400	= ?,	"
											"			comm_error_code		= ?,	"
											"			op_stock			= ?,	"
											"			units_stock			= ?,	"
											"			total_drug_price	= ?,	"
											"			total_member_price	= ?,	"
											"			member_discount_pt	= ?,	"
											"			special_presc_num	= ?,	"
											"			spec_pres_num_sors	= ?,	"
											"			ingr_1_code			= ?,	"
											"			ingr_2_code			= ?,	"
											"			ingr_3_code			= ?,	"
											"			ingr_1_quant_bot	= ?,	"
											"			ingr_2_quant_bot	= ?,	"
											"			ingr_3_quant_bot	= ?,	"
											"			ingr_1_quant_std	= ?,	"
											"			ingr_2_quant_std	= ?,	"
											"			ingr_3_quant_std	= ?,	"
											"			date				= ?,	"
											"			time				= ?,	"
											"			macabi_price_flg	= ?,	"
											"			dr_largo_code		= ?,	"
											"			dr_presc_date		= ?,	"
											"			link_ext_to_drug	= ?,	"
											"			num_linked_rxes		= ?,	"
											"			elect_pr_status		= ?,	"	// DonR 07Jul2015 - added for compatibility with Mersham Digitali.
											"			price_for_op_ph		= ?,	"
											"			discount_granted	= ?,	"
											"			discount_code		= ?,	"
											"			paid_for			= ?		"

											"	WHERE	prescription_id	= ?			"
											"	  AND	line_no			= ?			";
					NumInputColumns		=	35;
//					InputColumnSpec		=	"	int, int, short, short, short, int, int, int, int, int,						"
					InputColumnSpec		=	"	int, int, short, short, short, int, int, int, int, short,					"
											"	int, short, short, short, short, double, double, double, double, double,	"
											"	double, int, int, short, int, int, int, short, short, int,					"
											"	int, short, short, int, short	";
					break;


		case TR5005spool_UPD_prescriptions:
					SQL_CommandText		=	"	UPDATE	prescriptions				"

											"	SET		receipt_num			= ?,	"
											"			user_ident			= ?,	"
											"			presc_pharmacy_id	= ?,	"
											"			doctor_insert_mode	= ?,	"
											"			macabi_centers_num	= ?,	"
											"			doc_device_code		= ?,	"
											"			diary_month			= ?,	"
											"			delivered_flg		= ?,	"
											"			credit_type_used	= ?,	"
											"			date				= ?,	"
											"			time				= ?,	"
											"			error_code			= ?,	"
											"			reason_for_discnt	= ?,	"
											"			reason_to_disp		= ?,	"
											"			comm_error_code		= ?,	"
											"			credit_reason_code	= ?,	"
											"			num_payments		= ?,	"
											"			payment_method		= ?,	"
											"			tikra_discount		= ?,	"
											"			subsidy_amount		= ?,	"
											"			elect_pr_status		= ?,	"
											"			member_price_code	= ?,	"
											"			sale_req_error		= ?,	"
											"			reported_to_as400	= ?,	"
											"			paid_for			= ?		"

											"	WHERE	prescription_id	= ?			";
					NumInputColumns		=	26;
					InputColumnSpec		=	"	int, int, int, short, int, int, short, short, short, int,		"
											"	int, short, int, short, short, short, short, short, int, int,	"
											"	short, short, short, short, short, int							";
					break;


		case TR5051_credit_reasons_cur:
					SQL_CommandText		=	"	SELECT		credit_reason_code,	short_desc,		"
											"				update_date,		update_time		"

											"	FROM		credit_reasons						"

											"	ORDER BY	update_date, update_time			";
					NumOutputColumns	=	4;
					OutputColumnSpec	=	"	short, char(30), int, int	";
					break;


		case TR5053_subsidy_messages_cur:
					SQL_CommandText		=	"	SELECT		subsidy_code,		short_desc,		"
											"				update_date,		update_time		"

											"	FROM		subsidy_messages					"

											"	ORDER BY	update_date, update_time			";
					NumOutputColumns	=	4;
					OutputColumnSpec	=	"	short, char(10), int, int	";
					break;


		case TR5055_tikra_type_cur:
											// DonR 02Aug2020: Added ASCII() to retrieve single-character columns as a number, to be mapped
											// automatically to a 1-character TINYINT. This is the only way (at least in Informix) to map
											// to a simple character field without messing around with intermediate buffers.
					SQL_CommandText		=	"	SELECT		ASCII(tikra_type_code),	short_desc,		"	// DonR 02Aug2020 ASCII() for char(0) fields.
											"				update_date,			update_time		"

											"	FROM		tikra_type							"

											"	ORDER BY	update_date, update_time			";
					NumOutputColumns	=	4;
					OutputColumnSpec	=	"	char(0), char(15), int, int	";
					break;


		case TR5057_hmo_membership_cur:
					SQL_CommandText		=	"	SELECT		organization_code,	description,	"
											"				update_date,		update_time		"

											"	FROM		hmo_membership						"

											"	ORDER BY	update_date, update_time			";
					NumOutputColumns	=	4;
					OutputColumnSpec	=	"	short, char(15), int, int	";
					break;


		case TR5059_virtual_store_reason_texts_cur:
					SQL_CommandText		=	"	SELECT		reason_code			,	reason_text		,	"
											"				display_to_member	,	pharmacy_text	,	"
											"				allow_online_order	,	allow_pickup	,	"
											"				allow_delivery								"

											"	FROM		virtual_store_reason_texts					"

											"	ORDER BY	reason_code		";
					NumOutputColumns	=	7;
					OutputColumnSpec	=	"	short, char(100), short, char(100), short, short, short	";
					break;


		case TR5061_DrugGenComponents_cur:
					SQL_CommandText		=	"	SELECT		largo_code,				gen_comp_code,		del_flg,			"
											"				ingr_description,		ingredient_type,	package_size,		"
											"				ingredient_qty,			ingredient_units,	ingredient_per_qty,	"
											"				ingredient_per_units,	update_date,		update_time			"

											"	FROM		druggencomponents											"
		
											"	WHERE		(((update_date	=  ?) AND (update_time >= ?)) OR	"
											"				   update_date	>  ?)								"
		  
											"	ORDER BY	update_date,	update_time							";
					NumOutputColumns	=	12;
					OutputColumnSpec	=	"	int, short, char(1), char(40), char(1), float, float, char(3), float, char(3), int, int	";
					NumInputColumns		=	3;
					InputColumnSpec		=	"	int, int, int	";
					break;


		case TR6001_validate_req_type_1:
					SQL_CommandText		=	"	SELECT	CAST (COUNT (*)					AS INTEGER)		"
											"	FROM	pd_rx_link AS rxl, prescription_drugs AS pd		"
											"	WHERE	rxl.member_id			= ?						"
											"	  AND	rxl.member_id_code		= ?						"
											"	  AND	((rxl.doctor_presc_id	= ?) or (? = 0))		"
											"	  AND	((rxl.visit_number		= ?) or (? = 0))		"
											"	  AND	pd.prescription_id		= rxl.prescription_id	"
											"	  AND	pd.pharmacy_code		= ?						";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					NumInputColumns		=	7;
					InputColumnSpec		=	"	int, short, int, int, long, long, int	";
					Convert_NF_to_zero	=	1;
					break;


		case TR6001_doctor_presc_cur:
											// DonR 07Nov2024 User Story #357209: Add 6 new columns to support external prescription
											// providers (initially MaccabiDent).
											// DonR 10Dec2025 User Story #441076: Get prescriber profession stuff
											// from new PrescriberProfession table.
					SQL_CommandText		=	"	SELECT	member_id,					member_id_code,				clicks_patient_id,			"
											"			doctor_id,					doctor_presc_id,			largo_prescribed,			"
											"			valid_from_date,			valid_until_date,			order_number,				"
											"			sold_status,				ordered_status,				deleted_status,				"
											"			visit_number,				visit_date,					visit_time,					"
						
											"			clinic_address,				line_number,				speciality_desc,			"
											"			specialist_ishur,			rule_number,				prescription_type,			"
											"			digital_presc_flag,			dose_number,				op,							"
											"			total_units,				qty_method,					usage_method_code,			"
											"			usage_instructions,			amt_per_dose,				unit_abbreviation,			"
						
											"			doses_per_day,				treatment_length,			course_treat_days,			"
											"			course_len,					rx.course_len_units,		num_courses,				"
											"			days_per_week,				morning_evening,			treatment_side,				"
											"			member_phone,				clinic_phone,				row_added_date,				"
											"			row_added_time,				last_sold_date,				last_sold_time,				"
						
											"			tot_units_sold,				tot_op_sold,				linux_update_flag,			"
											"			linux_update_date,			linux_update_time,			extern_update_flag,			"
											"			extern_update_date,			extern_update_time,			reported_to_as400,			"
											"			reported_to_cds,			cl2d.effective_unit_len,	rx.rx_origin,				"
											"			external_doc_first_name,	external_doc_last_name,		external_doc_license_num,	"

											"			external_doc_license_type,	internal_comments,			PharmacistAuthorization,	"
											"			ProfCodeToPharmacy,			ProfDescription											"

											"	FROM 	doctor_presc			as rx,														"
											"			course_len_to_days		as cl2d,													"
											"			PrescriberProfession	as prof														"

											"	WHERE	member_id			=  ?															"

													// DonR 24Oct2017 CR #13050 - If the prescription was for 45 days or longer, we will do
													// a "virtual select" on a fictitious Valid Until Date = 10 days before the end of the
													// treatment length. This will be implemented in the FETCH loop since there's no clean
													// way to do it in the SELECT itself. DonR 24Apr2018 - Added new variable MinVisitDate
													// to avoid sending old data when a requested Doctor Prescription ID has occurred in the
													// past for the same patient.
													// DonR 07Jan2021: Don't bother selecting ancient long-duration prescriptions.
											"	  AND	(((valid_until_date	>= ? OR (valid_from_date >= ? AND treatment_length >= 45)) AND ? IN (0,3))	"
													// Request Type 0 sends current data based on valid_until_date (the line above this comment);
													// Request Type 0 and 1 both send data based on any non-zero matches for Doctor Prescription
													// ID and/or Visit Number.
											"				OR	(		(doctor_presc_id	=  ?	OR ?	= 0	)							"
											"						AND	(visit_number		=  ?	OR ?	= 0	)							"
											"						AND	(visit_date			>= ?							)))				"

											"	  AND	member_id_code	= ?																	"

												  // DonR 20Dec2016 - Select the correct row from course_len_to_days to translate
												  // the course length from whatever units it's in to days.
											"	  AND	cl2d.course_len_units	=  rx.course_len_units										"

												  // DonR 10Dec2025 User Story #441076: Add join to get prescriber profession info.
											"	  AND	prof.rx_origin			=  rx.rx_origin												"
											"	  AND	prof.profession_type	=  0														"

											"	ORDER BY	doctor_id, visit_number, doctor_presc_id, largo_prescribed						";
					NumOutputColumns	=	65;
					OutputColumnSpec	=	"	int, short, int, int, int, int, int, int, int, short, short, short, long, int, int,												"
											"	varchar(255), short, varchar(25), short, int, short, short, short, short, short, short, short,  varchar(100), double, char(3),	"
											"	short, short, short, short, char(6), short,	varchar(20), varchar(200), char(10), char(10), char(10), int, int, int, int,		"
											"	short, short, short, int, int, short, int, int, short, short, short, short, varchar(8), varchar(14), int,						"
											"	short, varchar(500), varchar(20), short, char(25)																				";
					NumInputColumns		=	10;
					InputColumnSpec		=	"	int, int, int, short, int, int, long, long, int, short	";
					StatementIsSticky	=	1;
					break;


		case TR6001_doctor_presc_cur_simplified:
											// DonR 07Nov2024 User Story #357209: Add 6 new columns to support external prescription
											// providers (initially MaccabiDent).
											// DonR 10Dec2025 User Story #441076: Get prescriber profession stuff
											// from new PrescriberProfession table.
					SQL_CommandText		=	"	SELECT	member_id,					member_id_code,				clicks_patient_id,			"
											"			doctor_id,					doctor_presc_id,			largo_prescribed,			"
											"			valid_from_date,			valid_until_date,			order_number,				"
											"			sold_status,				ordered_status,				deleted_status,				"
											"			visit_number,				visit_date,					visit_time,					"
						
											"			clinic_address,				line_number,				speciality_desc,			"
											"			specialist_ishur,			rule_number,				prescription_type,			"
											"			digital_presc_flag,			dose_number,				op,							"
											"			total_units,				qty_method,					usage_method_code,			"
											"			usage_instructions,			amt_per_dose,				unit_abbreviation,			"
						
											"			doses_per_day,				treatment_length,			course_treat_days,			"
											"			course_len,					rx.course_len_units,		num_courses,				"
											"			days_per_week,				morning_evening,			treatment_side,				"
											"			member_phone,				clinic_phone,				row_added_date,				"
											"			row_added_time,				last_sold_date,				last_sold_time,				"
						
											"			tot_units_sold,				tot_op_sold,				linux_update_flag,			"
											"			linux_update_date,			linux_update_time,			extern_update_flag,			"
											"			extern_update_date,			extern_update_time,			reported_to_as400,			"
											"			reported_to_cds,			cl2d.effective_unit_len,	rx.rx_origin,				"
											"			external_doc_first_name,	external_doc_last_name,		external_doc_license_num,	"

											"			external_doc_license_type,	internal_comments,			PharmacistAuthorization,	"
											"			ProfCodeToPharmacy,			ProfDescription											"

											"	FROM 	doctor_presc			as rx,														"
											"			course_len_to_days		as cl2d,													"
											"			PrescriberProfession	as prof														"

											"	WHERE	member_id			=  ?															"

													// DonR 24Oct2017 CR #13050 - If the prescription was for 45 days or longer, we will do
													// a "virtual select" on a fictitious Valid Until Date = 10 days before the end of the
													// treatment length. This will be implemented in the FETCH loop since there's no clean
													// way to do it in the SELECT itself. DonR 24Apr2018 - Added new variable MinVisitDate
													// to avoid sending old data when a requested Doctor Prescription ID has occurred in the
													// past for the same patient.
													// DonR 07Jan2021: Don't bother selecting ancient long-duration prescriptions.
											"	  AND	(valid_until_date	>= ? OR (valid_from_date >= ? AND treatment_length >= 45))		"

											"	  AND	member_id_code	= ?																	"

												  // DonR 27Apr2021 "Chanut Virtualit": Add the option of excluding fully-sold prescriptions
												  // from the SELECT. This option is controlled by a new ExcludeFullySoldRx variable. (Note
												  // that this option is NOT used in TR6001_doctor_presc_cur - it's relevant only for the
												  // "simple" version of the SELECT.)
											"	  AND	((sold_status		<  2) OR (? = 0))												"

												  // DonR 20Dec2016 - Select the correct row from course_len_to_days to translate
												  // the course length from whatever units it's in to days.
											"	  AND	cl2d.course_len_units	=  rx.course_len_units										"

												  // DonR 10Dec2025 User Story #441076: Add join to get prescriber profession info.
											"	  AND	prof.rx_origin			=  rx.rx_origin												"
											"	  AND	prof.profession_type	=  0														"

											"	ORDER BY	doctor_id, visit_number, doctor_presc_id, largo_prescribed						";
					NumOutputColumns	=	65;
					OutputColumnSpec	=	"	int, short, int, int, int, int, int, int, int, short, short, short, long, int, int,												"
											"	varchar(255), short, varchar(25), short, int, short, short, short, short, short, short, short,  varchar(100), double, char(3),	"
											"	short, short, short, short, char(6), short,	varchar(20), varchar(200), char(10), char(10), char(10), int, int, int, int,		"
											"	short, short, short, int, int, short, int, int, short, short, short, short, varchar(8), varchar(14), int,						"
											"	short, varchar(500), varchar(20), short, char(25)																				";
					NumInputColumns		=	5;
					InputColumnSpec		=	"	int, int, int, short, short	";
					StatementIsSticky	=	1;
					break;


		case TR6001_FindAlternatePackageSizeGenerics:
					SQL_CommandText		=	"	SELECT	largo_code															"
											"	FROM	drug_list															"
											"	WHERE	economypri_group		=			?								"
											"	  AND	economypri_group		>			0								"	// Pure paranoia.
											"	  AND	preferred_flg			=			1								"	// Non-preferred = not interesting.
											"	  AND	multi_pref_largo		>			0								"
											"	  AND	largo_code				<>			?								"
											"	  AND	largo_code				>			0								"	// More paranoia.
											"	  AND	del_flg					=			0								";	// *Should* be redundant.
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					NumInputColumns		=	2;
					InputColumnSpec		=	"	int, int	";
					StatementIsSticky	=	1;
					break;


		case TR6001_READ_CheckFor99997Ishur:
					SQL_CommandText		=	"	SELECT CAST (COUNT(*) AS INTEGER) FROM special_prescs WHERE %s	";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					NeedsWhereClauseID	=	1;
					Convert_NF_to_zero	=	1;
					break;


		case TR6001_READ_ishur_list:
					SQL_CommandText		=	"	SELECT		largo_code					"
											"	FROM		special_prescs				"
											"	WHERE		member_id			=  ?	"
											"	  AND		largo_code			>  0	"	// To conform to index.
											"	  AND		confirmation_type	=  1	"
											"	  AND		stop_use_date		>= ?	"
											"	  AND		treatment_start		<= ?	"
											"	  AND		mem_id_extension	=  ?	"
											"	  AND		member_price_code	>  0	"
											"	ORDER BY	largo_code					";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					NumInputColumns		=	4;
					InputColumnSpec		=	"	int, int, int, short	";
					StatementIsSticky	=	1;
					break;


		case TR6001_READ_CheckForAutoShabanNohalDiscount:
					SQL_CommandText		=	"	SELECT	CAST (count(*) AS INTEGER)										"

											"	FROM	drug_extension													"

											"	WHERE	(largo_code			=  ?	OR largo_code = ?)					"
											"	  AND	largo_code			>  0										"
											"	  AND	rule_number			>  -999										"	// Just to conform to index structure.
											"	  AND	from_age			<= ?										"
											"	  AND	to_age				>= ?										"
											"	  AND	(sex				=  ?	OR sex = 0)							"

											"	  AND	effective_date		<= ?										"
											"	  AND	confirm_authority	=  0										"	// "Automatic" rule.
											"	  AND	del_flg				=  ?										"

												  // DonR 212Nov2011 - Add selection criteria for member type.
											"	  AND	(((enabled_mac	> 0) AND (?	> 0))	OR							"
											"			 ((enabled_hova	> 0) AND (?	> 0))	OR							"
											"			 ((enabled_keva	> 0) AND (?	> 0)))								"

											"	  AND	send_and_use		<> 0										"

												  // DonR 06Dec2012 "Yahalom" - New insurance-type and "vetek" criteria.
												  // Note that the price code/fixed price combination is now validated
												  // without regard to which insurance is applicable.
											"	  AND	(price_code > 1 OR (price_code = 1 AND fixed_price > 0))		"

											"	  AND	insurance_type		IN ('Y', 'Z')								";	// Either Maccabi Sheli or Magen Zahav.
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					NumInputColumns		=	10;
					InputColumnSpec		=	"	int, int, short, short, short, int, short, short, short, short	";
					Convert_NF_to_zero	=	1;
					StatementIsSticky	=	1;	// DonR 28Jun2020.
					break;


		case TR6001_READ_CheckForAutoShabanSpecialistDiscount:
					SQL_CommandText		=	"	SELECT	CAST (count(*) AS INTEGER)									"

											"	FROM	doctor_speciality	ds,										"
											"			spclty_largo_prcnt	lp										"

											"	WHERE	ds.doctor_id		= ?										"
											"	  AND	ds.speciality_code	NOT BETWEEN 58000 AND 58999				"	// Exclude dentist stuff
											"	  AND	ds.speciality_code	NOT BETWEEN  4000 AND  4999				"	// Exclude "home visit" stuff
											"	  AND	(ds.del_flg			= ? OR ds.del_flg IS NULL)				"
											"	  AND	lp.speciality_code	= ds.speciality_code					"
											"	  AND	(largo_code			=  ?	OR largo_code = ?)				"
											"	  AND	largo_code			>  0									"

												  // DonR 212Nov2011 - Add selection criteria for member type.
											"	  AND	(((enabled_mac	> 0) AND (?	> 0))	OR						"
											"			 ((enabled_hova	> 0) AND (?	> 0))	OR						"
											"			 ((enabled_keva	> 0) AND (?	> 0)))							"

											"	  AND	(lp.del_flg			= ? OR lp.del_flg IS NULL)				"

												  // DonR 06Dec2012 "Yahalom" - New insurance-type and "vetek" criteria.
												  // Note that the price code/fixed price combination is now validated
												  // without regard to which insurance is applicable.
											"	  AND	(price_code > 1 OR (price_code = 1 AND fixed_price > 0))	"

											"	  AND	insurance_type		IN ('Y', 'Z')							";	// Either Maccabi Sheli or Magen Zahav.
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					NumInputColumns		=	8;
					InputColumnSpec		=	"	int, short, int, int, short, short, short, short	";
					Convert_NF_to_zero	=	1;
					StatementIsSticky	=	1;	// DonR 28Jun2020.
					break;


		case TR6001_READ_CheckPharmacyIshurPossibility:
					SQL_CommandText		=	"	SELECT	CAST (count(*) AS INTEGER)									"

											"	FROM	drug_extension												"

											"	WHERE	(largo_code			=  ?	OR largo_code = ?)				"
											"	  AND	largo_code			>  0									"
											"	  AND	rule_number			>  -999									"	// Just to conform to index structure.
											"	  AND	from_age			<= ?									"
											"	  AND	to_age				>= ?									"
											"	  AND	(sex				=  ?	OR sex = 0)						"

											"	  AND	effective_date		<= ?									"
											"	  AND	confirm_authority	BETWEEN 1 AND 10						"	// Pharmacy-ishur level rules only.
											"	  AND	del_flg				=  ?									"

												  // DonR 212Nov2011 - Add selection criteria for member type.
											"	  AND	(((enabled_mac	> 0) AND (?	> 0))	OR						"
											"			 ((enabled_hova	> 0) AND (?	> 0))	OR						"
											"			 ((enabled_keva	> 0) AND (?	> 0)))							"

											"	  AND	send_and_use		<> 0									"

												  // Maccabi pharmacies can use all rules; Prati Plus pharmacies can use
												  // private-pharmacy rules as well as Prati Plus rules.
											"	  AND	((? = 1)								OR					"
											"			 (? = 6 AND permission_type IN (0,2,6))	OR					"
											"			 (? = 0 AND permission_type IN (0,2)  ))					"

												  // DonR 06Dec2012 "Yahalom" - New insurance-type and "vetek" criteria.
												  // Note that the price code/fixed price combination is now validated
												  // without regard to which insurance is applicable.
											"	  AND	(price_code > 1 OR (price_code = 1 AND fixed_price > 0))	"

											"	  AND	((insurance_type = 'B')							OR			"
											"			 (insurance_type = ?	AND ?	>= wait_months)	OR			"
											"			 (insurance_type = ?	AND ?	>= wait_months))			";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					NumInputColumns		=	17;
					InputColumnSpec		=	"	int, int, short, short, short, int, short, short, short, short,	"
											"	short, short, short, char(1), short, char(1), short				";
					Convert_NF_to_zero	=	1;
					StatementIsSticky	=	1;	// DonR 28Jun2020.
					break;


		case TR6001_READ_CheckPriorPurchaseToAllowDelivery:
					SQL_CommandText		=	"	SELECT	CAST (COUNT(*) AS INTEGER)																		"
											"	FROM	prescription_drugs																				"
											"	WHERE	member_id				=  ?																	"
											"	  AND	date					>= ?																	"
											"	  AND	((largo_code			=  ?)	OR																"
											"			 (largo_code			IN	(	SELECT	DISTINCT largo_code										"
											"											FROM	spclty_drug_grp											"
											"											WHERE	group_code = ? OR parent_group_code = ?	)	)	)		"
											"	  AND	delivered_flg			=  1																	"
											"	  AND	del_flg					=  0																	"
											"	  AND	mem_id_extension		=  ?																	";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					NumInputColumns		=	6;
					InputColumnSpec		=	"	int, int, int, int, int, short	";
					Convert_NF_to_zero	=	1;
					StatementIsSticky	=	1;	// DonR 28Jun2020.
					break;


		case TR6001_READ_CheckSpecialistDiscountPossibility:
					SQL_CommandText		=	"	SELECT	CAST (count(*) AS INTEGER)										"

											"	FROM	spclty_largo_prcnt												"

											"	WHERE	(largo_code			=  ?	OR largo_code = ?)					"
											"	  AND	largo_code			>  0										"

											"	  AND	speciality_code		NOT BETWEEN 58000 AND 58999					"	// Exclude dentist stuff
											"	  AND	speciality_code		NOT BETWEEN  4000 AND  4999					"	// Exclude "home visit" stuff

												  // DonR 212Nov2011 - Add selection criteria for member type.
											"	  AND	(((enabled_mac	> 0) AND (?	> 0))	OR							"
											"			 ((enabled_hova	> 0) AND (?	> 0))	OR							"
											"			 ((enabled_keva	> 0) AND (?	> 0)))								"

											"	  AND	(del_flg			= ? OR del_flg IS NULL)						"

												  // DonR 06Dec2012 "Yahalom" - New insurance-type and "vetek" criteria.
												  // Note that the price code/fixed price combination is now validated
												  // without regard to which insurance is applicable.
											"	  AND	(price_code > 1 OR (price_code = 1 AND fixed_price > 0))		"

											"	  AND	((insurance_type = 'B')							OR				"
											"			 (insurance_type = ?	AND ?	>= wait_months)	OR				"
											"			 (insurance_type = ?	AND ?	>= wait_months))				";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					NumInputColumns		=	10;
					InputColumnSpec		=	"	int, int, short, short, short, short, char(1), short, char(1), short	";
					Convert_NF_to_zero	=	1;
					StatementIsSticky	=	1;	// DonR 28Jun2020.
					break;


		case TR6001_READ_doctors:
					SQL_CommandText		=	"	SELECT	first_name,			last_name,		phone,				"
											"			check_interactions,	license_number,	drug_contact_phone,	"
											"			ProfCodeToPharmacy,	ProfDescription						"
											"	FROM	doctors													"
											"	WHERE	doctor_id = ?											"
											"	  AND	doctor_id > 0											";
					NumOutputColumns	=	8;
					OutputColumnSpec	=	"	varchar(8), varchar(14), varchar(10), short, int, char(40), short, char(25)		";
					NumInputColumns		=	1;
					InputColumnSpec		=	"	int	";
					StatementIsSticky	=	1;	// DonR 25Mar2020.
					break;


		case TR6001_READ_GetLastMatchingVisitDate:
					SQL_CommandText		=	"	SELECT	MAX(visit_date)				"

											"	FROM 	doctor_presc				"

											"	WHERE	member_id			=  ?	"
											"	  AND	doctor_presc_id		=  ?	"
											"	  AND	member_id_code		=  ?	"
											"	  AND	visit_date			<= ?	";	// Just in case there is strange data in the table with Visit Date in the future.
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					NumInputColumns		=	4;
					InputColumnSpec		=	"	int, int, short, int	";
					break;


		// DonR 03Oct2022 BUG 272058 FIX: If this is an ishur that's "attached" to a particular pharmacy,
		// Maccabi pharmacies other than the "attached" one can still sell the drug with the ishur
		// discount - they just get a ERR_SPEC_PR_FOR_OTHER_PHARM_WRN warning message in Transaction
		// 6003. Accordingly, we need these ishurim to be visible to all Maccabi pharmacies in
		// Transaction 6001/6101. To support this, added a second instance of the Maccabi Pharmacy
		// flag to the relevant SQL as the 8th input parameter.
		case TR6001_READ_special_prescs_prescribed_largo:
					SQL_CommandText		=	"	SELECT {FIRST} 1	member_price_code,		const_sum_to_pay,	pharmacy_code,		"
											"						priv_pharm_sale_ok,		largo_code,			health_basket_new	"

											"	FROM				special_prescs													"

											"	WHERE				member_id				=  ?									"
											"	  AND				largo_code				=  ?									"
											"	  AND				stop_use_date			>= ?									"
											"	  AND				treatment_start			<= ?									"
											"	  AND				confirmation_type		=  1									"	// Active ishur.
											"	  AND				member_price_code		>  0									"	// This is an ishur that actually does set participation.
											"	  AND				(priv_pharm_sale_ok		<> 1 OR ? <> 0)							"
											"	  AND				(priv_pharm_sale_ok		<> 6 OR ? =  0)							"
//											"	  AND				(pharmacy_code			=  ? OR pharmacy_code = 0)				"
											"	  AND				(pharmacy_code			=  ? OR pharmacy_code = 0 OR ? <> 0)	"
											"	  AND				mem_id_extension		=  ?									"
//											"	  AND				del_flg					=  0									"	// Del_flg is not used in special_prescs.
								  
											"	ORDER BY			stop_use_date DESC												";
					NumOutputColumns	=	6;
					OutputColumnSpec	=	"	short, int, int, short, int, short	";
					NumInputColumns		=	9;
					InputColumnSpec		=	"	int, int, int, int, short, short, int, short, short	";
					StatementIsSticky	=	1;	// DonR 28Jun2020.
					break;


		case TR6001_READ_special_prescs_substitute_largo:
											// DonR 13Jan2026 User Story #477601 (BUG FIX): Moved the
											// mem_id_extension condition from the end of the WHERE
											// clause to the second line, right after member_id. This
											// is how the query is called in DigitalRx.c, and it also
											// reflects the column-type list that was already set up
											// in InputColumnSpec.
					SQL_CommandText		=	"	SELECT {FIRST} 1	sp.member_price_code,	sp.const_sum_to_pay,	sp.pharmacy_code,	"
											"						sp.priv_pharm_sale_ok,	sp.largo_code,			dl.economypri_seq,	"
											"						sp.health_basket_new												"

											"	FROM				special_prescs	as sp,												"
											"						drug_list		as dl												"

											"	WHERE				member_id				=  ?										"
											"	  AND				mem_id_extension		=  ?										"	// DonR 13Jan2026 U.S. #477601 CORRECT POSITION OF CRITERION.
											"	  AND				sp.largo_code			<> ?										"	// Mostly just for fast indexing.
											"	  AND				confirmation_type		=  1										"	// Active ishur.
											"	  AND				treatment_start			<= ?										"
//											"	  AND				sp.del_flg				=  0										"	// Del_flg is not used in special_prescs.
											"	  AND				sp.member_price_code	>  0										"	// This is an ishur that actually does set participation.
											"	  AND				stop_use_date			>= ?										"
											"	  AND				(sp.priv_pharm_sale_ok	<> 1 OR ? <> 0)								"
											"	  AND				(sp.priv_pharm_sale_ok	<> 6 OR ? =  0)								"
											"	  AND				(pharmacy_code			=  ? OR pharmacy_code = 0)					"
											"	  AND				dl.economypri_group		=  ?										"
											"	  AND				dl.largo_code			=  sp.largo_code							"
//											"	  AND				mem_id_extension		=  ?										"	// DonR 13Jan2026 U.S. #477601 PREVIOUS INCORRECT POSITION.

											"	ORDER BY			dl.economypri_seq, stop_use_date DESC								";
					NumOutputColumns	=	7;
					OutputColumnSpec	=	"	short, int, int, short, int, short, short	";
					NumInputColumns		=	9;
					InputColumnSpec		=	"	int, short, int, int, int, short, short, int, int	";
					StatementIsSticky	=	1;	// DonR 28Jun2020.
					break;


		//Marianna 20Oct2022 BUG 272058: Check for "nohalim rokeach" that would allow the pharmacy to sell drugs at a discount
		// DonR 06Nov2022: I spoke to Iris about confirm_authority, and she says the correct range for pharmacy ishurim is
		// between 1 and 10.
		case TR6101_READ_CheckForDrugExtensionPharmacyIshurim:
					SQL_CommandText		=	"	SELECT	to_age	, extend_rule_days											"
											"	FROM	drug_extension														"
											"	WHERE	(largo_code			= ?)											"
											"	  AND	confirm_authority			BETWEEN	1 AND 10						"
											"	  AND	(sex				= ? 	OR sex = 0)								"
											"	  AND	from_age			<= ?											"
											"	  AND	(extend_rule_days > 0		OR to_age >= ?)							"
											"	  AND	(price_code > 1				OR (price_code = 1 AND fixed_price > 0))"
											"	  AND	(effective_date		<= ?	OR effective_date = 0)					"
											"	  AND	send_and_use		<> 0											"
											"	  AND	del_flg				=  0											"
											"	  AND	((insurance_type = 'B')							OR					"
											"			 (insurance_type = ?	AND ?	>= wait_months)	OR					"
											"			 (insurance_type = ?	AND ?	>= wait_months))					"
											"	ORDER BY to_age DESC														";
					NumOutputColumns	= 2;
					OutputColumnSpec	= "	short, short ";
					NumInputColumns		= 9;
					InputColumnSpec		 = " int, short, short, short, int, char(1), short, char(1), short ";
					break;


		case TR6101_GetDrugShapeName:
					SQL_CommandText		=  "	SELECT shape_name_eng, home_delivery_ok FROM drug_shape WHERE drug_shape_code = ?	";
					NumOutputColumns	=	2;
					OutputColumnSpec	=	"	char(25), short	";
					NumInputColumns		=	1;
					InputColumnSpec		=	"	short		";
					break;


		case TR6101_READ_CheckPriorPurchasesToAllowPickup:
												// DonR 18Apr2023 User Story #432608: times_of_day is now length 200 in the database.
												// DonR 22Oct2023 User Story #487170: Add a join to course_len_to_days to get the
												// effective unit length of the course length. Because pharmacies sometimes report
												// invalid values for Course Units, we need a LEFT JOIN with a conditional assignment.
												// And for the syntax to work, it looks like we also have to change how we JOIN to
												// drug_list.
					SQL_CommandText		=	"	SELECT		pd.largo_code, 		pd.use_instr_template, 	dl.shape_code_new,				"
											"			 	pd.units_per_dose,	pd.doses_per_day, 		pd.how_to_take_code,			" //Marianna 30Nov2021 : add instruction fields
											"				pd.unit_code, 		pd.course_treat_days, 	pd.course_length,				"
											"				pd.course_units, 	pd.days_of_week,		pd.times_of_day,				"
											"				pd.side_of_body,	pd.duration,											"

											"				CASE	WHEN (cl2d.effective_unit_len IS NULL)								"
											"							THEN 0															"
											"							ELSE cl2d.effective_unit_len									"
											"				END																			"
																													
											"	FROM		prescription_drugs	AS pd													"
											"	JOIN		drug_list			AS dl	ON dl.largo_code			= pd.largo_code		"
											"	LEFT JOIN	course_len_to_days	AS cl2d	ON cl2d.course_len_units	= pd.course_units	"

											"	WHERE	pd.member_id			=  ?													"
											"	  AND	pd.date					>= ?													"
											"	  AND	((pd.largo_code			=  ?)	OR												"
											"			 (pd.largo_code			IN	(	SELECT	DISTINCT largo_code						"
											"											FROM	spclty_drug_grp							"
											"											WHERE	group_code = ? 	)	)	)				" //Marianna 25Nov2021 : work only with a generic group code not need: OR parent_group_code = ?
											"	  AND	pd.delivered_flg		=  1													"
											"	  AND	pd.del_flg				=  0													"
											"	  AND	pd.mem_id_extension		=  ?													"
											"	ORDER BY pd.date DESC, pd.time DESC														";
					NumOutputColumns	=	15;
					OutputColumnSpec	=	"	int, short, short, int, int, short, char(3), short, short, char(10), char(20), char(200), char(10), int, short	";
					NumInputColumns		=	5;
					InputColumnSpec		=	"	int, int, int, int, short	";
					StatementIsSticky	=	1;	// ???.
					break;


		// Marianna 01Dec2021
		case TR6101_READ_CountSameShapeCode:
					SQL_CommandText		=	"	SELECT	CAST (COUNT(*) AS INTEGER)												"
											"	FROM	prescription_drugs 														"
											"	WHERE	member_id			=  ?												"
											"	  AND	date				>= ?												"
											"	  AND	largo_code IN (	SELECT	largo_code										"
											"							FROM	drug_list										"
											"							WHERE	shape_code_new	=  ?	)						"
											"	  AND	delivered_flg		=  1												"
											"	  AND	del_flg				=  0												"
											"	  AND	mem_id_extension	=  ?												";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					NumInputColumns		=	4;
					InputColumnSpec		=	"	int, int, short, short	";
					Convert_NF_to_zero	=	1;
					StatementIsSticky	=	1;	// ???.
					break;


		case TR6101_READ_GetReasonText:
					SQL_CommandText		=	"	SELECT	reason_text, display_to_member			"
											"	FROM	virtual_store_reason_texts				"
											"	WHERE	reason_code	=  ?						";
					NumOutputColumns	=	2;
					OutputColumnSpec	=	"	char(100), short	";
					NumInputColumns		=	1;
					InputColumnSpec		=	"	short	";
//					StatementIsSticky	=	1;	// ???
					break;
		

		// Marianna 19Dec2024 User Story #313648
		// Check if item exist in price_list 
		case TR6101_CheckItemExistsInPriceList:
					SQL_CommandText		=	"     SELECT case when exists                       "
											"			(	SELECT	*							"
											"				FROM	price_list					"
											"				WHERE	largo_code = ?)				"
											"			THEN 1 ELSE 0 END";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"   int ";
					NumInputColumns		=	1;
					InputColumnSpec		=	"   int ";
					break;


		case TR6003_deldrugs_cur:
					SQL_CommandText		=	"	SELECT	largo_code,				op,						units,					"
											"			total_member_price,		tikrat_mazon_flag,		member_price_code,		"
											"			particip_method,		special_presc_num,		spec_pres_num_sors,		"

											"			ishur_with_tikra,		ishur_tikra_type,		price_for_op,			"
											"			price_for_op_ph,		price_extension,		price_replace,			"
											"			rule_number,			updated_by,				dr_largo_code			"

											"	FROM	prescription_drugs														"

											"	WHERE	prescription_id	= ?														"
											"	  AND	delivered_flg	= ?														"	// Only drug sales are relevant here.
											"	  AND	del_flg			= ?														";
					NumOutputColumns	=	18;
					OutputColumnSpec	=	"	int, int, int,			int, short, short,		short, int, short,		"
											"	short, short, int,		int, int, int,			int, short, int			";	// DonR 24Mar2020: updated_by is read into health-basket, a short int!
					NumInputColumns		=	3;
					InputColumnSpec		=	"	int, short, short	";
					break;


		case TR6003_INS_prescription_drugs:			// DonR 11Feb2020: Changed the input parameter for member_discount_pt
													// from int to short. It's an INTEGER in prescription_drugs, but
													// a SMALLINT elsewhere; and the host variable for this value is a
													// short integer. Since we don't use any values greater than 10000
													// (= 100%) for this number, there's no point in using an int host
													// variable; in fact, the database variable really should be a SMALLINT.
													// DonR 27Mar2023 User Story #432608: Added IsConsignment to INSERT.
													// DonR 29Jun2023 User Story #461368: Added "frozen" alternate_yarpa_price.
					SQL_CommandText		=	"	INSERT INTO prescription_drugs								"
											"	(															"
											"		prescription_id,				line_no,				"
											"		member_id,						mem_id_extension,		"
											"		largo_code,						reported_to_as400,		"
											"		op,								units,					"
											"		duration,						stop_use_date,			"

											"		date,							time,					"
											"		del_flg,						delivered_flg,			"
											"		price_replace,					price_extension,		"
											"		link_ext_to_drug,				price_for_op,			"
											"		action_type,					drug_answer_code,		"

											"		member_price_code,				calc_member_price,		"
											"		presc_source,					macabi_price_flg,		"
											"		pharmacy_code,					institued_code,			"
											"		price_for_op_ph,				stop_blood_date,		"
											"		total_drug_price,				op_stock,				"

											"		units_stock,					total_member_price,		"
											"		comm_error_code,				t_half,					"
											"		doctor_id,						doctor_id_type,			"
											"		doctor_presc_id,				dr_presc_date,			"
											"		dr_largo_code,					subst_permitted,		"

											"		units_unsold,					op_unsold,				"
											"		units_per_dose,					doses_per_day,			"
											"		particip_method,				prescr_wr_rrn,			"
											"		updated_by,						special_presc_num,		"
											"		spec_pres_num_sors,				ishur_with_tikra,		"

											"		ishur_tikra_type,				op_prescribed,			"
											"		units_prescribed,				rule_number,			"
											"		tikra_type_code,				subsidized,				"
											"		tikrat_mazon_flag,				member_ptn_amt,			"
											"		elect_pr_status,				credit_type_code,		"

											"		credit_type_used,				why_future_sale_ok,		"
											"		qty_limit_chk_type,				qty_lim_ishur_num,		"
											"		vacation_ishur_num,				doctor_facility,		"
											"		member_discount_amt,			barcode_scanned,		"
											"		digital_rx,						member_diagnosis,		"

											"		ph_OTC_unit_price,				member_discount_pt,		"
											"		qty_limit_class_code,			IsConsignment,			"
											"		alternate_yarpa_price									"
											"	)															";
					NumInputColumns		=	75;
					InputColumnSpec		=	"	int, short, int, short, int, short, int, int, short, int,		"	// DonR 20Feb2020: Duration is populated from a short int.
											"	int, int, short, short, int, int, int, int, short, short,		"
											"	short, short, short, short, int, short, int, int, int, int,		"
											"	int, int, short, short, int, short, int, int, int, short,		"	// DonR 11Mar2020: T_half is an integer in prescription_drugs, but is populated by a short. (Change column type in MS-SQL!)
											"	int, int, int, int, short, int, short, int, short, short,		"	// DonR 20Feb2020: "updated_by" is populated from in_health_pack, a short int.
											"	short, int, int, int, char(1), short, short, int, short, short,	"	// DonR 09Jul2020: BUG FIX: char(0) in input columns isn't supported, so tikra_type_code wasn't being written!
											"	short, short, short, int, int, int, int, short, short, int,		"
											"	int, short, short, short, int									";
					GenerateVALUES		=	1;
					StatementIsSticky	=	1;
					break;


		case TR6003_READ_CheckCouponExpiryStatus:
					SQL_CommandText		=	"	SELECT	expiry_status					"
											"	FROM	coupons							"
											"	WHERE	member_id			=  ?		"
											"	  AND	mem_id_extension	=  ?		"
											"	  AND	del_flg				=  ?		"
											"	  AND	expiry_status		IN (1,2)	";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	short	";
					NumInputColumns		=	3;
					InputColumnSpec		=	"	int, short, short	";
					break;


		case TR6003_READ_gadgets:
					SQL_CommandText		=	"	SELECT	gadget_code,			service_code,		service_number,			"
											"			service_type,			enabled_mac,		enabled_hova,			"
											"			enabled_keva,			enabled_pvt_pharm,	enabled_without_rx,		"
											"			PrevPurchaseClassCode,	MinPrevPurchases,	MaxPrevPurchases		"

											"	FROM	gadgets																"

											"	WHERE	largo_code				= ?											"
											"	  AND	((gadget_code			= ?)	OR (?	= 0))						"
											"	  AND	((enabled_without_rx	> 0)	OR (?	> 0))						"	// DonR 05Nov2018
											"	  AND	(((enabled_mac	> 0) AND (?	> 0))	OR								"
											"			 ((enabled_hova	> 0) AND (?	> 0))	OR								"
											"			 ((enabled_keva	> 0) AND (?	> 0)))									";
					NumOutputColumns	=	12;
					OutputColumnSpec	=	"	int, int, short, short, short, short, short, short, short, short, short, short	";
					NumInputColumns		=	7;
					InputColumnSpec		=	"	int, int, int, short, short, short, short	";
					break;


		case TR6003_READ_Get_deleted_prescription_drugs_row_values:
					SQL_CommandText		=	"	SELECT	op,						units,					total_member_price,	"
											"			tikrat_mazon_flag,		member_price_code,		del_flg,			"
											"			particip_method,		special_presc_num,		spec_pres_num_sors,	"

											"			ishur_with_tikra,		ishur_tikra_type,		price_for_op,		"
											"			price_for_op_ph,		price_extension,		price_replace,		"
											"			rule_number,			updated_by,				dr_largo_code		"

											"	FROM	prescription_drugs													"

											"	WHERE	prescription_id	= ?													"
											"	  AND	delivered_flg	= ?													"	// Only drug sales are relevant here.
											"	  AND	largo_code		= ?													";
					NumOutputColumns	=	18;
					OutputColumnSpec	=	"	int, int, int,		short, short, short,	short, int, short, 			"
											"	short, short, int,	int, int, int,			int, short, int				";	// DonR 24Mar2020: Although updated_by is an INTEGER, it's read into a short int. In new MS-SQL database, this should be fixed (both name and type)!
					NumInputColumns		=	3;
					InputColumnSpec		=	"	int, short, int	";
					break;


		case TR6003_READ_Get_doctor_id_from_doctor_presc:
					SQL_CommandText		=	"	SELECT {FIRST} 1	doctor_id																				"
											"	FROM				doctor_presc																			"
											"	WHERE				member_id			=		?															"
											"	  AND				doctor_presc_id		=		?															"
											"	  AND				valid_from_date		BETWEEN	? AND ?														"
											"	  AND				((largo_prescribed	=		?)															"
											"					OR	 (largo_prescribed	IN		(SELECT	largo_code											"
											"													 FROM	drug_list											"
											"													 WHERE	economypri_group	>	0							"
											"													   AND	economypri_group	=	(SELECT	economypri_group	"
											"																					 FROM	drug_list			"
											"																					 WHERE	largo_code	= ?))))	"
											"	ORDER BY		visit_date DESC																				";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					NumInputColumns		=	6;
					InputColumnSpec		=	"	int, int, int, int, int, int	";
					break;


		case TR6003_CheckForExternalRxSupplier:
					SQL_CommandText		=	"	SELECT {FIRST} 1	rx_origin,					external_doc_first_name,	"
											"						external_doc_last_name,		clinic_phone				"
											"	FROM				doctor_presc											"
											"	WHERE				member_id			=		?							"
											"	  AND				member_id_code		=		?							"	// Mostly just to confirm to the index.
											"	  AND				doctor_id			=		?							"
											"	  AND				doctor_presc_id		=		?							"
											"	ORDER BY			visit_date DESC											";
					NumOutputColumns	=	4;
					OutputColumnSpec	=	"	short, varchar(8), varchar(14), char(10)	";
					NumInputColumns		=	4;
					InputColumnSpec		=	"	int, short, int, int	";
					StatementIsSticky	=	1;	// This will be a very high-volume query.
					break;


		case TR6003_READ_ValidateSpecialDrugSubstitution:
					SQL_CommandText		=	"	SELECT	qty_method,			op,					total_units,	"
											"			tot_op_sold,		tot_units_sold						"

											"	FROM	doctor_presc											"

											"	WHERE	member_id			=		?							"
											"	  AND	doctor_presc_id		=		?							"
											"	  AND	valid_from_date		BETWEEN	? AND ?						"
											"	  AND	largo_prescribed	=		?							"
											"	  AND	doctor_id			=		?							"
											"	  AND	member_id_code		=		?							"
											"	  AND	sold_status			IN		(0, 1)						";	 // Either unsold or partially sold.
					NumOutputColumns	=	5;
					OutputColumnSpec	=	"	short, short, short, short, short	";
					NumInputColumns		=	7;
					InputColumnSpec		=	"	int, int, int, int, int, int, short	";
					break;


// DonR Bug #334612: This query is no long in use, but it's worth keeping around just in case
// someone needs it later. So don't delete it!
//		case VerifyContinuousTreatmentRx:
//					SQL_CommandText		=	"	SELECT	CAST (COUNT(*) AS INTEGER)		"	// Should always return 0 or 1 - I think!
//											"	FROM	doctor_presc					"
//											"	WHERE	visit_number		= ?			"
//											"	  AND	doctor_presc_id		= ?			"
//											"	  AND	line_number			> -1		"	// Just to conform to index structure.
//											"	  AND	largo_prescribed	= ?			"
//											"	  AND	prescription_type	= 0			"
//											"	  AND	member_id			= ?			";	// Mostly paranoia - Visit Number really should be enough.
//					NumOutputColumns	=	1;
//					OutputColumnSpec	=	"	int	";
//					NumInputColumns		=	4;
//					InputColumnSpec		=	"	long, int, int, int	";
//					Convert_NF_to_zero	=	1;
//					StatementIsSticky	=	1;	// This will be a pretty high-volume query.
//					break;
//
//
		case GetPreviousDrugSaleTotalsByVisitNumber:
					// DonR 18Dec2023: Don't select sale deletions! Since we don't copy delivered_flg to pd_rx_link
					// (at least not yet), test for a positive sale quantity: (op_sold + units_sold) > 0.
					// DonR 19Dec2023: Also, don't select data from the current drug sale. This requires adding the
					// current Prescription ID to the WHERE list.
					// DonR 17Jul2024 Bug #334612: Calculate previously-sold quantities in terms of the sold drug,
					// *NOT* the prescribed drug.
					// DonR 10Sep2024 Bug #334612 RE-OPENED: In order to avoid errors when the member has bought drugs
					// with different package sizes OR the doctor has prescribed equivalent drugs with different
					// package sizes, we need to compute the number of pills/capsules/whatever *separately* for each
					// pd_rx_link row we retrieve. In order to do this, we need to join with drug_list to get the
					// package size for each Largo Code involved. Since most of the time there will be only one Largo
					// Code from the doctor, it's slightly more efficient to perform the computations based on the
					// Largo Prescribed and op_sold/units_sold, which are both expressed in terms of the prescribed drug.
//					SQL_CommandText		=	"	SELECT	MIN(sold_date), SUM(op_sold), SUM(units_sold),			"
//					SQL_CommandText		=	"	SELECT	MIN(sold_date), SUM(sold_drug_op), SUM(sold_drug_units),			"
					SQL_CommandText		=	"	SELECT	MIN(sold_date),															"
											"			SUM ((op_sold * package_size) + units_sold),							"
											"			CAST (COUNT(*) AS INTEGER)												"
											"	FROM	pd_rx_link as RXL, drug_list as DL										"
											"	WHERE	visit_number						=  ?								"
											"	  AND	prescription_id						<> ?								"
											"	  AND	(largo_sold							=  ? OR RXL.economypri_group = ?)	"
											"	  AND	prescription_type					=  0								"
											"	  AND	RXL.del_flg							=  0								"
											"	  AND	(sold_drug_op + sold_drug_units)	>  0								"	// DonR 18Dec2023: Don't select sale deletions!
											"	  AND	member_id							=  ?								"	// Pure paranoia!
											"	  AND	DL.largo_code						=  RXL.largo_prescribed				";
					NumOutputColumns	=	3;
					OutputColumnSpec	=	"	int, int, int	";
					NumInputColumns		=	5;
					InputColumnSpec		=	"	long, int, int, int, int	";
					StatementIsSticky	=	1;	// This will be a pretty high-volume query.
					break;


		case TR6005_doc_prescription_link_cur:
					SQL_CommandText		=	"	SELECT		RXL.doctor_presc_id,	RXL.valid_from_date,	RXL.visit_number,	"
											"				RXL.op_sold,			RXL.units_sold,			PD.dr_largo_code	"

											"	FROM		prescription_drugs	as PD,											"
											"				pd_rx_link			as RXL											"

											"	WHERE		PD.prescription_id		= ?											"
											"	  AND		PD.largo_code			= ?											"
											"	  AND		PD.doctor_id			= ?											"
											"	  AND		RXL.prescription_id		= ?											"
											"	  AND		RXL.line_no				= PD.line_no								";
					NumOutputColumns	=	6;
					OutputColumnSpec	=	"	int, int, long, int, int, int	";
					NumInputColumns		=	4;
					InputColumnSpec		=	"	int, int, int, int	";
					break;


		case TR6005_prescription_drugs_cur:
					SQL_CommandText		=	"	SELECT		largo_code,			doctor_id,			op,					"
											"				units,				line_no,			link_ext_to_drug,	"
											"				price_for_op,		price_for_op_ph,	drug_answer_code,	"

											"				stop_use_date,		t_half,				prescr_wr_rrn,		"
											"				particip_method,	member_ptn_amt,		macabi_price_flg,	"
											"				stop_blood_date,	ph_otc_unit_price,	presc_source		"	// DonR 14Jan2025: Added presc_source to columns read.

											"	FROM		prescription_drugs											"

											"	WHERE		prescription_id	= ?											"
											"	  AND		delivered_flg   = ?											";
					NumOutputColumns	=	18;
					OutputColumnSpec	=	"	int, int, int, int, short, int, int, int, short,	"
											"	int, short, int, short, int, short, int, int, short	";	// DonR 11Mar2020: t_half is an INTEGER in Informix, but should be a SMALLINT - it's populated by short's and read into short's.
					NumInputColumns		=	2;
					InputColumnSpec		=	"	int, short	";
					StatementIsSticky	=	1;	// DonR 12Jul2020.
					break;


		case TR6005_INS_prescription_vouchers:
											// Note that this is a simple, relatively low-volume INSERT, so it doesn't need to be "sticky".
					SQL_CommandText		=	"	INSERT INTO	prescription_vouchers														"
											"	(																						"
											"		prescription_id,			voucher_num,				member_id,					"
											"		mem_id_extension,			voucher_type,				voucher_discount_given,		"
											"		voucher_amount_remaining,	original_voucher_amount,	sold_date,					"
											"		date,						time,						reported_to_as400			"
											"	)																						";
					NumInputColumns		=	12;
					InputColumnSpec		=	"	int, long, int, short, char(15), int, int, int, int, int, int, short	";
					GenerateVALUES		=	1;
					break;


		case TR6005_INS_pd_cannabis_products:
											// Note that this is a simple, relatively low-volume INSERT, so it doesn't need to be "sticky".
					SQL_CommandText		=	"	INSERT INTO	pd_cannabis_products														"
											"	(																						"
											"		prescription_id,			line_no,					cannabis_product_code,		"
											"		cannabis_product_barcode,	group_largo_code,			op,							"
											"		units,						price_per_op,				product_sale_amount,		"
											"		product_ptn_amount,			del_flg,					reported_to_as400,			"
											"		date,						time													"
											"	)																						";
					NumInputColumns		=	14;
					InputColumnSpec		=	"	int, short, int, long, int, short, short, int, int, int, short, short, int, int	";
					GenerateVALUES		=	1;
					break;


		case TR6005_READ_check_online_order_validity:
					SQL_CommandText		=	"	SELECT	made_by_pr_id				"
											"	FROM	online_orders				"
											"	WHERE	online_order_num	= ?		"
											"	  AND	made_by_pharmacy	= ?		"
											"	  AND	made_by_owner		= ?		";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					NumInputColumns		=	3;
					InputColumnSpec		=	"	long, int, short	";
					break;


		case TR6005_READ_cannabis_product_details:
					SQL_CommandText		=	"	SELECT	cannabis_product_code,	cannabis_product_barcode,	"
											"			op,						units,						"
											"			price_per_op,			product_sale_amount,		"
											"			product_ptn_amount									"
											"	FROM	pd_cannabis_products								"
											"	WHERE	prescription_id		= ?								"
											"	  AND	group_largo_code	= ?								";
					NumOutputColumns	=	7;
					OutputColumnSpec	=	"	int, long, short, short, int, int, int	";
					NumInputColumns		=	2;
					InputColumnSpec		=	"	int, int	";
					break;


		case TR6005_UPD_prescription_drugs:		// DonR 11Feb2020: Changed the input parameters for member_discount_pt
												// and price_extension from int to short. Both are INTEGERs in
												// prescription_drugs, but SMALLINTs elsewhere; and the host variables
												// for these values are short integers. Since we don't use any values
												// greater than 10000 (= 100%) for these numbers, there's no point in
												// using an int host variable; in fact, the database variables really
												// should be SMALLINTs.
												// DonR 07Jun2020: Made sticky.
												// DonR 27Mar2023 User Story #432608: 8 new columns updated.
												// DonR 18Apr2023 User Story #432608: times_of_day is now length 200 in the database.
												// DonR 31Jul2023 User Story #469361: Add 4 columns for new blood-duration logic.
												// DonR 04Feb2024 User Story #540234: Add NumCannabisProducts to update.
					SQL_CommandText		=	"	UPDATE	prescription_drugs				"

											"	SET		op						= ?,	"
											"			units					= ?,	"
											"			delivered_flg			= ?,	"	// Deletions are "delivered" with flg of 2!
											"			reported_to_as400		= ?,	"
											"			op_stock				= ?,	"
											"			units_stock				= ?,	"
											"			total_drug_price		= ?,	"
											"			total_member_price		= ?,	"
											"			price_extension			= ?,	"	// 23May2019 CR #29169
											"			member_discount_pt		= ?,	"	// 23May2019 CR #29169	Really a bug, but leave in place until new SR.EC is migrated.

											"			price_for_op_ph			= ?,	"
											"			discount_granted		= ?,	"
											"			discount_code			= ?,	"
											"			ingr_1_code				= ?,	"
											"			ingr_2_code				= ?,	"
											"			ingr_3_code				= ?,	"
											"			ingr_1_quant_bot		= ?,	"
											"			ingr_2_quant_bot		= ?,	"
											"			ingr_3_quant_bot		= ?,	"
											"			ingr_1_quant_std		= ?,	"

											"			ingr_2_quant_std		= ?,	"
											"			ingr_3_quant_std		= ?,	"
														// DonR 06May2015 - Added update of more fields that pharmacy can change in Trn. 6005.
											"			use_instr_template		= ?,	"
											"			how_to_take_code		= ?,	"
											"			unit_code				= ?,	"
											"			doses_per_day			= ?,	"
											"			units_per_dose			= ?,	"
											"			duration				= ?,	"
											"			course_treat_days		= ?,	"
											"			course_length_days		= ?,	"

											"			course_length			= ?,	"
											"			course_units			= ?,	"
											"			days_of_week			= ?,	"
											"			times_of_day			= ?,	"
											"			side_of_body			= ?,	"
											"			elect_pr_status			= ?,	"
											"			use_instr_changed		= ?,	"
														// DonR 06May2015 end.
											"			date					= ?,	"
											"			time					= ?,	"
											"			macabi_price_flg		= ?,	"

											"			link_ext_to_drug		= ?,	"
											"			num_linked_rxes			= ?,	"
											"			voucher_amount_used		= ?,	"
											"			paid_for				= ?,	"
											"			UsageInstrGiven			= ?,	"	// DonR 27Mar2023 start.
											"			MaccabiOtcPrice			= ?,	"
											"			SalePkgPrice			= ?,	"
											"			SaleNum4Price			= ?,	"
											"			SaleNumBuy1Get1			= ?,	"
											"			Buy1Get1Savings			= ?,	"

											"			ByHandReduction			= ?,	"
											"			IsConsignment			= ?,	"
											"			NumCourses				= ?,	"	// DonR 27Mar2023 end.
											"			blood_start_date		= ?,	"	// DonR 31Jul2023 begin.
											"			blood_duration			= ?,	"
											"			blood_last_date			= ?,	"
											"			blood_data_calculated	= ?,	"	// DonR 31Jul2023 end.
											"			NumCannabisProducts		= ?		"

											"	WHERE	prescription_id	= ?				"
											"	  AND	line_no			= ?				";
					NumInputColumns		=	60;
//					InputColumnSpec		=	"	int, int, short, short, int, int, int, int, int,   int,   int,		"	// price_extension and member_discount_pt are INTEGERs fed from SHORTs.
					InputColumnSpec		=	"	int, int, short, short, int, int, int, int, short, short,								"
											"	int, int, short, short, short, short, double, double, double, double,					"
											"	double, double, short, short, char(3), short, int, short, short, short,					"	// DonR 20Feb2020: Doses_per_day and duration are populated from short integers.
											"	short, char(10), varchar(20), varchar(200), char(10), short, short, int, int, short,	"
											"	int, short, int, short, varchar(100), int, int, int, int, int,							"	// DonR 07Jun2021 User Story #163882: Add voucher_amount_used.
											"	int, short, short, int, short, int, short, short, int, short							";	// DonR 04Feb2024 User Story #540234: Add NumCannabisProducts.
					StatementIsSticky	=	1;
					break;


		case TR6005_UPD_prescriptions:
												// DonR 07Jun2020: Made sticky.
					SQL_CommandText		=	"	UPDATE	prescriptions				"

											"	SET		receipt_num			= ?,	"
											"			user_ident			= ?,	"
											"			presc_pharmacy_id	= ?,	"
											"			doctor_insert_mode	= ?,	"	// May disappear in future!
											"			macabi_centers_num	= ?,	"
												//		doc_device_code		= v_DocLocationCode,	NOT GETTING THIS FROM PHARMACY!
											"			diary_month			= ?,	"
											"			delivered_flg		= ?,	"	// Deletions are "delivered" with flg of 2!
											"			reported_to_as400	= ?,	"
											"			credit_type_used	= ?,	"
											"			date				= ?,	"

											"			time				= ?,	"
											"			error_code			= ?,	"	// DonR 24Jul2013 - the previous value unless we hit a new error.
											"			reason_for_discnt	= ?,	"
											"			credit_reason_code	= ?,	"
											"			num_payments		= ?,	"
											"			payment_method		= ?,	"
											"			tikra_discount		= ?,	"
											"			subsidy_amount		= ?,	"
											"			elect_pr_status		= ?,	"
											"			reason_to_disp		= ?,	"

											"			member_price_code	= ?,	"
											"			sale_req_error		= ?,	"
											"			card_owner_id		= ?,	"
											"			card_owner_id_code	= ?,	"
											"			paid_for			= ?,	"
											"			MagentoOrderNum		= ?,	"
											"			online_order_num	= ?		"

											"	WHERE	prescription_id	= ?			";
					NumInputColumns		=	28;
					InputColumnSpec		=	"	int, int, int, short, int, short, short, short, short, int,		"
											"	int, short,	short, short, short, short, int, int, short, short,	"
											"	short, short, int, short, short, long, long, int				";
					StatementIsSticky	=	1;
					break;


		case TR6005spool_deldrugs_cur:
					SQL_CommandText		=	"	SELECT	largo_code,				op,						units,				"
											"			total_member_price,		member_price_code,		particip_method,	"
											"			price_for_op,			price_for_op_ph,		price_replace,		"
											"			updated_by,				total_drug_price,		discount_granted,	"

											"			discount_code,			tikrat_mazon_flag,		special_presc_num,	"
											"			spec_pres_num_sors,		ishur_with_tikra,		ishur_tikra_type,	"
											"			rule_number,			doctor_id,				presc_source,		"
											"			link_ext_to_drug													"

											"	FROM	prescription_drugs													"

											"	WHERE	prescription_id	= ?													"
											"	  AND	delivered_flg	= ?													"	// Only drug sales are relevant here.
											"	  AND	del_flg			= ?													";
					NumOutputColumns	=	22;
					OutputColumnSpec	=	"	int, int, int, int, short, short, int, int, int, short, int, int,			"	// DonR 24Mar2020: updated_by is read into health-basket, a short int!
											"	short, short, int, short, short, short, int, int, short, int				";
					NumInputColumns		=	3;
					InputColumnSpec		=	"	int, short, short	";
					break;


		case TR6005spool_INS_prescription_drugs:	// DonR 11Feb2020: Changed the input parameters for member_discount_pt
													// and price_extension from int to short. Both are INTEGERs in
													// prescription_drugs, but SMALLINTs elsewhere; and the host variables
													// for these values are short integers. Since we don't use any values
													// greater than 10000 (= 100%) for these numbers, there's no point in
													// using an int host variable; in fact, the database variables really
													// should be SMALLINTs.
													// DonR 27Mar2023 User Story #432608: Insert 10 new columns.
													// DonR 18Apr2023 User Story #432608: times_of_day is now length 200 in the database.
													// DonR 31Jul2023 User Story #469361: Add 4 columns for new blood-duration logic.
													// DonR 04Feb2024 User Story #540234: Add NumCannabisProducts to insert.
					SQL_CommandText		=	"	INSERT INTO	prescription_drugs												"
											"	(																			"
											"		prescription_id,		line_no,				member_id,				"
											"		mem_id_extension,		largo_code,				reported_to_as400,		"
											"		op,						units,					duration,				"
											"		stop_use_date,			date,					time,					"
											"		del_flg,				delivered_flg,			price_replace,			"
															
											"		price_extension,		link_ext_to_drug,		price_for_op,			"
											"		action_type,			drug_answer_code,		member_price_code,		"
											"		calc_member_price,		presc_source,			macabi_price_flg,		"
											"		pharmacy_code,			institued_code,			price_for_op_ph,		"
											"		stop_blood_date,		total_drug_price,		op_stock,				"

											"		units_stock,			total_member_price,		member_ptn_amt,			"
											"		comm_error_code,		t_half,					doctor_id,				"
											"		doctor_id_type,			updated_by,				special_presc_num,		"
											"		spec_pres_num_sors,		doctor_presc_id,		dr_presc_date,			"
											"		dr_largo_code,			subst_permitted,		units_per_dose,			"
															
											"		doses_per_day,			member_discount_pt,		particip_method,		"
											"		prescr_wr_rrn,			ingr_1_code,			ingr_2_code,			"
											"		ingr_3_code,			ingr_1_quant_bot,		ingr_2_quant_bot,		"
											"		ingr_3_quant_bot,		ingr_1_quant_std,		ingr_2_quant_std,		"
											"		ingr_3_quant_std,		discount_granted,		discount_code,			"

											"		tikrat_mazon_flag,		ishur_with_tikra,		ishur_tikra_type,		"
											"		rule_number,			credit_type_code,		use_instr_template,		"
											"		how_to_take_code,		unit_code,				course_treat_days,		"
											"		course_length_days,		course_length,			course_units,			"
											"		days_of_week,			times_of_day,			side_of_body,			"
															
											"		elect_pr_status,		use_instr_changed,		why_future_sale_ok,		"
											"		qty_limit_chk_type,		qty_lim_ishur_num,		vacation_ishur_num,		"
											"		dr_visit_date,			doctor_facility,		num_linked_rxes,		"
											"		barcode_scanned,		digital_rx,				voucher_amount_used,	"
											"		qty_limit_class_code,	paid_for,				UsageInstrGiven,		"	// DonR 27Mar2023 begin.

											"		MaccabiOtcPrice,		SalePkgPrice,			SaleNum4Price,			"
											"		SaleNumBuy1Get1,		Buy1Get1Savings,		ByHandReduction,		"
											"		IsConsignment,			NumCourses,				blood_start_date,		"	// DonR 27Mar2023 end, 31Jul2023 begin.
											"		blood_duration,			blood_last_date,		blood_data_calculated,	"	// DonR 31Jul2023 end.
											"		NumCannabisProducts														"
											"	)																			";
					NumInputColumns		=	103;
					InputColumnSpec		=	"	int, short, int, short, int, short, int, int, short, int, int, int, short, short, int,										"	// DonR 20Feb2020: Duration is populated from a short int.
											"	short, int, int, short, short, short, short, short, short, int, short, int, int, int, int,									"
											"	int, int, int, short, short, int, short, short, int, short, int, int, int, short, int,										"	// DonR 20Feb2020: "updated_by" is populated from in_health_pack, a short int.
																																												// DonR 11Mar2020: T_half is an integer in prescription_drugs, but is populated by a short. (Change column type in MS-SQL!)
											"	short, short, short, int, short, short, short, double, double, double, double, double, double, int, short,					"	// DonR 20Feb2020: Doses_per_day is populated from a short int.
											"	short, short, short, int, short, short, short, char(3), short, short, short, char(10), varchar(20), varchar(200), char(10),	"
											"	short, short, short, short, int, int, int, int, short, short, short, int, short, short, varchar(100),						"	// DonR 07Jun2021 User Story #163882: Add voucher_amount_used.

											"	int, int, int, int, int, int, short, short, int, short, int, short, short													";	// DonR 04Feb2024 User Story #540234: Add NumCannabisProducts.
					GenerateVALUES		=	1;
					break;


		case TR6005spool_READ_check_that_pharmacy_sent_correct_values:
												// DonR 13Jul2020 CR #32984: Added error_code to columns SELECTed.
					SQL_CommandText		=	"	SELECT	pharmacy_code,		member_id,			date,				"
											"			tikra_discount,		subsidy_amount,		online_order_num,	"
											"			error_code													"

											"	FROM	prescriptions												"

											"	WHERE	prescription_id = ?											";
					NumOutputColumns	=	7;
					OutputColumnSpec	=	"	int, int, int, int, int, long, short	";
					NumInputColumns		=	1;
					InputColumnSpec		=	"	int	";
					break;


		case TR6005spool_READ_deleted_prescription_drugs_values:
					SQL_CommandText		=	"	SELECT	op,						units,					total_member_price,	"
											"			member_price_code,		particip_method,		price_for_op,		"
											"			price_for_op_ph,		price_replace,			updated_by,			"

											"			total_drug_price,		discount_granted,		discount_code,		"
											"			tikrat_mazon_flag,		special_presc_num,		spec_pres_num_sors,	"
											"			ishur_with_tikra,		ishur_tikra_type,		rule_number,		"
											"			link_ext_to_drug													"
											
											"	FROM	prescription_drugs													"

											"	WHERE	prescription_id	= ?													"
											"	  AND	delivered_flg	= ?													"	// Only drug sales are relevant here.
											"	  AND	largo_code		= ?													";
					NumOutputColumns	=	19;
					OutputColumnSpec	=	"	int, int, int,		short, short, int,		int, int, short,				"	// DonR 24Mar2020: updated_by is read into health-basket, a short int!
											"	int, int, short,	short, int, short,		short, short, int,		int		";
					NumInputColumns		=	3;
					InputColumnSpec		=	"	int, short, int	";
					break;


		case TR6005spool_READ_deleted_prescriptions_row_values:
											// DonR 20Aug2020: Added presc_source to columns read.
					SQL_CommandText		=	"	SELECT	subsidy_amount, tikra_discount, macabi_centers_num, del_flg, presc_source	"
											"	FROM	prescriptions			"
											"	WHERE	prescription_id	= ?		";
					NumOutputColumns	=	5;
					OutputColumnSpec	=	"	int, int, int, short, short	";
					NumInputColumns		=	1;
					InputColumnSpec		=	"	int	";
					break;


		case TR6005spool_READ_prescription_drugs_fields_before_update:
					SQL_CommandText		=	"	SELECT	line_no,			particip_method,		link_ext_to_drug,	"
											"			special_presc_num,	spec_pres_num_sors,		ph_otc_unit_price	"

											"	FROM	prescription_drugs												"

											"	WHERE	prescription_id	= ?												"
											"	  AND	largo_code		= ?												";
					NumOutputColumns	=	6;
					OutputColumnSpec	=	"	short, short, int, int, short, int	";
					NumInputColumns		=	2;
					InputColumnSpec		=	"	int, int	";
					break;


		case TR6005spool_UPD_prescription_drugs:	// DonR 11Feb2020: Changed the input parameters for member_discount_pt
													// and price_extension from int to short. Both are INTEGERs in
													// prescription_drugs, but SMALLINTs elsewhere; and the host variables
													// for these values are short integers. Since we don't use any values
													// greater than 10000 (= 100%) for these numbers, there's no point in
													// using an int host variable; in fact, the database variables really
													// should be SMALLINTs.
													// DonR 27Mar2023 User Story #432608: Added 10 new columns to update.
													// DonR 18Apr2023 User Story #432608: times_of_day is now length 200 in the database.
													// DonR 31Jul2023 User Story #469361: Add 4 columns for new blood-duration logic.
													// DonR 04Feb2024 User Story #540234: Add NumCannabisProducts to update.
					SQL_CommandText		=	"	UPDATE	prescription_drugs				"

											"	SET		op						= ?,	"
											"			units					= ?,	"
											"			delivered_flg			= ?,	"	// Deletions are "delivered" with flg of 2!
											"			reported_to_as400		= ?,	"
											"			op_stock				= ?,	"
											"			units_stock				= ?,	"
											"			total_drug_price		= ?,	"
											"			total_member_price		= ?,	"
											"			price_extension			= ?,	"	// 23May2019 CR #29169
											"			member_discount_pt		= ?,	"	// 23May2019 CR #29169	Really a bug, but leave in place until new SR.EC is migrated.

											"			comm_error_code			= ?,	"
											"			special_presc_num		= ?,	"
											"			spec_pres_num_sors		= ?,	"
											"			price_for_op_ph			= ?,	"
											"			discount_granted		= ?,	"
											"			discount_code			= ?,	"
											"			ingr_1_code				= ?,	"
											"			ingr_2_code				= ?,	"
											"			ingr_3_code				= ?,	"
											"			ingr_1_quant_bot		= ?,	"

											"			ingr_2_quant_bot		= ?,	"
											"			ingr_3_quant_bot		= ?,	"
											"			ingr_1_quant_std		= ?,	"
											"			ingr_2_quant_std		= ?,	"
											"			ingr_3_quant_std		= ?,	"
														// DonR 06May2015 - Added update of more fields that pharmacy can change in Trn. 6005.
											"			use_instr_template		= ?,	"
											"			how_to_take_code		= ?,	"
											"			unit_code				= ?,	"
											"			doses_per_day			= ?,	"
											"			units_per_dose			= ?,	"

											"			duration				= ?,	"
											"			course_treat_days		= ?,	"
											"			course_length_days		= ?,	"
											"			course_length			= ?,	"
											"			course_units			= ?,	"
											"			days_of_week			= ?,	"
											"			times_of_day			= ?,	"	// Getting 200 characters from pharmacy - expand DB field as well?
											"			side_of_body			= ?,	"
											"			elect_pr_status			= ?,	"
											"			use_instr_changed		= ?,	"

														// DonR 06May2015 end.
											"			date					= ?,	"
											"			time					= ?,	"
											"			macabi_price_flg		= ?,	"
											"			link_ext_to_drug		= ?,	"
											"			num_linked_rxes			= ?,	"
											"			voucher_amount_used		= ?,	"
											"			paid_for				= ?,	"
											"			UsageInstrGiven			= ?,	"	// DonR 27Mar2023 begin.
											"			MaccabiOtcPrice			= ?,	"
											"			SalePkgPrice			= ?,	"

											"			SaleNum4Price			= ?,	"
											"			SaleNumBuy1Get1			= ?,	"
											"			Buy1Get1Savings			= ?,	"
											"			ByHandReduction			= ?,	"
											"			IsConsignment			= ?,	"
											"			NumCourses				= ?,	"	// DonR 73Mar2023 end.
											"			blood_start_date		= ?,	"	// DonR 31Jul2023 begin.
											"			blood_duration			= ?,	"
											"			blood_last_date			= ?,	"
											"			blood_data_calculated	= ?,	"	// DonR 31Jul2023 end.
											"			NumCannabisProducts		= ?		"

											"	WHERE	prescription_id	= ?				"
											"	  AND	line_no			= ?				";
					NumInputColumns		=	63;
					InputColumnSpec		=	"	int, int, short, short, int, int, int, int, short, short,									"
											"	short, int, short, int, int, short, short, short, short, double,							"
											"	double, double, double, double, double, short, short, char(3), short, int,					"	// DonR 20Feb2020: Doses_per_day and duration are populated from short integers.
											"	short, short, short, short, char(10), varchar(20), varchar(200), char(10), short, short,	"
											"	int, int, short, int, short, int, short, varchar(100), int, int,							"	// DonR 07Jun2021 User Story #163882: Add voucher_amount_used.
											"	int, int, int, int, short, short, int, short, int, short, short, int, short					";	// DonR 04Feb2024 User Story #540234: Add NumCannabisProducts.
					break;


		case TR6011_AreDoctorVisitsAlive:
//					SQL_CommandText		=	"	SELECT	CASE	WHEN EXISTS										"
//											"							(										"
//											"								SELECT *							"
//											"								FROM 	doctor_presc				"
//											"								WHERE	visit_number		=  ?	"
//											"								  AND	member_id			=  ?	"
//											"								  AND	member_id_code		=  ?	"
//											"								  AND	valid_until_date	>= ?	"
//											"								  AND	sold_status			<  2	"
//											"								  AND	deleted_status		=  0	"
//											"							)										"
//											"					THEN	1										"
//											"					ELSE	0										"
//											"			END														";
					SQL_CommandText		=	"	SELECT	CAST (count(DISTINCT visit_number) AS INTEGER)	"
											"	FROM	doctor_presc									"
											"	WHERE	visit_number		IN (?, ?)					"
											"	  AND	member_id			=  ?						"
											"	  AND	member_id_code		=  ?						"
											"	  AND	valid_until_date	>= ?						"
											"	  AND	sold_status			<  2						"
											"	  AND	deleted_status		=  0						";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	" int ";
					NumInputColumns		=	5;
					InputColumnSpec		=	" long, long, int, short, int ";
					StatementIsSticky	=	1;
					break;


		case TR6011_DoctorVisitsSoldToday:
											// Note that Visit Number should really be enough to identify
											// the doctor visit; adding Member ID/Member ID Code is really
											// just paranoia.
					SQL_CommandText		=	"	SELECT	CAST (count(DISTINCT visit_number) AS INTEGER)	"
											"	FROM	pd_rx_link				AS rxl,					"
											"			prescription_drugs		AS pd					"
											"	WHERE	rxl.visit_number		IN (?, ?)				"
											"	  AND	rxl.member_id			=  ?					"
											"	  AND	rxl.member_id_code		=  ?					"
											"	  AND	rxl.sold_date			=  ?					"
											"	  AND	pd.prescription_id		=  rxl.prescription_id	"	// Join part 1.
											"	  AND	pd.line_no				=  rxl.line_no			"	// Join part 2.
											"	  AND	pd.action_type			=  1					"	// Sales only.
											"	  AND	pd.pharmacy_code		=  ?					";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	" int ";
					NumInputColumns		=	6;
					InputColumnSpec		=	" long, long, int, short, int, int ";
					StatementIsSticky	=	1;
					break;


		case TR6011_ReceiptNumberCheck:
					// Verify a Receipt Number sent by pharmacy for a prior sale to the same member, today,
					// from the same terminal as the current 6011 request. Note that we don't care if the
					// prior sale was deleted or not - as long as it actually happened.
					SQL_CommandText		=	"	SELECT	CASE	WHEN EXISTS										"
											"							(										"
											"								SELECT *							"
											"								FROM 	prescriptions				"
											"								WHERE	member_id			=  ?	"
											"								  AND	mem_id_extension	=  ?	"
											"								  AND	date				=  ?	"
											"								  AND	delivered_flg		>  0	"	// Redundant for Receipt Number > 0, but harmless.
											"								  AND	pharmacy_code		=  ?	"
											"								  AND	terminal_id			=  ?	"
											"								  AND	receipt_num			=  ?	"
											"							)										"
											"					THEN	1										"
											"					ELSE	0										"
											"			END														";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	" bool ";
					NumInputColumns		=	6;
					InputColumnSpec		=	" int, short, int, int, short, int ";
					StatementIsSticky	=	1;	// Dunno if this one really has to be "sticky", but I guess it can't hurt.
					break;


// DonR 06Nov2025: It turns out that this query is not necessary after all - but leave the code here, since
// I think it's the only place in the system where we're using this kind of search-in-string logic.
//		case TR6011_ListIncludesOpioidsOrADHD_drugs:
//											// How this works: To avoid having to PREPARE a query with an "IN (...)"
//											// criterion each time the transaction is run, we're using a sneaky (but
//											// effective) alternative. We build a string in the form "_X_Y_" of however
//											// many Largo Codes we're interested in; then we build a string "_Largo_"
//											// from each drug in the table and see if that string is found somewhere
//											// in the "interesting Largos" string. If so, that drug is of interest, and
//											// we look at its drug_type to see if it's an opioid or an ADHD treatment.
//											// The MAX logic returns two booleans: one if at least one drug on the list
//											// is an opioid, and another if at least one drug is for ADHD treatment.
//					SQL_CommandText		=	"	SELECT	MAX ( CASE WHEN drug_type = 'N' THEN 1 ELSE 0 END ),			"	// N = Opioids.
//											"			MAX ( CASE WHEN drug_type = 'T' THEN 1 ELSE 0 END )				"	// T = Narcotic stimulants (for ADHD)
//											"	FROM	drug_list														"
//
//											// The first WHERE criterion is just for performance - we don't want to waste
//											// the database server's time doing string manipulations/searches for drugs
//											// that we already know aren't opioids or ADHD treatments.
//											"	WHERE	drug_type IN ('N', 'T')											"
//											"	  AND	CHARINDEX ( ('_' + CAST(largo_code AS VARCHAR) + '_'), ?) > 0	";
//					NumOutputColumns	=	2;
//					OutputColumnSpec	=	" bool, bool ";
//					NumInputColumns		=	1;
//					InputColumnSpec		=	" char(1000) ";
//					StatementIsSticky	=	1;
//					break;
//
//
		case TR6011_PriorSaleDownload:
											// The first bunch of columns are SELECTed unconditionally. Note that in
											// order to get exactly one row selected for each prescription_drugs row,
											// we need to do a GROUP BY and select everything with MAX(); this way
											// we can successfully resolve the one-to-many relationship between drug
											// sales, doctor prescriptions, and so on.
					SQL_CommandText		=	"	SELECT	MAX (pd.date),						MAX (pd.largo_code),				"
											"			MAX (pd.op),						MAX (pd.units),						"
											"			MAX (pd.pharmacy_code),				MAX (ph.pharmacy_name),				"
											"			MAX (pd.presc_source),				MAX (src.source_category_desc),		"
											"			MAX (pd.doctor_id_type),			MAX (pd.doctor_id),					"	// 10
						
											"			MAX (pd.rule_number),				MAX (pd.qty_limit_chk_type),		"
											"			MAX (pd.ingr_1_code),				MAX (pd.ingr_2_code),				"
											"			MAX (pd.ingr_3_code),				MAX (pd.ingr_1_quant_bot),			"
											"			MAX (pd.ingr_2_quant_bot),			MAX (pd.ingr_3_quant_bot),			"
											"			MAX (pd.digital_rx),				MAX (pd.use_instr_template),		"	// 20

											"			MAX (pd.how_to_take_code),			MAX (pd.units_per_dose),			"
											"			MAX (pd.unit_code),					MAX (pd.doses_per_day),				"
											"			MAX (pd.duration),					MAX (pd.course_treat_days),			"
											"			MAX (pd.course_length),				MAX (pd.course_units),				"
											"			MAX (pd.days_of_week),				MAX (pd.times_of_day),				"	// 30

											"			MAX (pd.side_of_body),				MAX (pd.UsageInstrGiven),			"
											"			MAX (pd.NumCourses),				MAX (dl.economypri_group),			"	// 34

											// Doctor name and specialty description come from different places under
											// different circumstances. At least for now, leave lookup by Doctor License
											// Number up to the calling routine, since the vast majority of sales don't
											// use that method and we don't want to make the SQL code more complicated
											// than it already is.
											"			COALESCE (	MAX (dr.first_name),										"
											"						MAX (rx.external_doc_first_name),							"
											"						''									),						"
											"			COALESCE (	MAX (dr.last_name),											"
											"						MAX (rx.external_doc_last_name),							"
											"						''									),						"
											"			COALESCE (	MAX (rx.speciality_desc),									"
											"						''									),						"	// 37

											// We want to report the Ishur Number only when we actually got member
											// participation from an Ishur.
											"			CASE	WHEN ((MAX (pd.particip_method) % 10) = 6)						"
											"					THEN MAX (pd.special_presc_num)									"
											"					ELSE 0															"
											"			END,																	"	// 38

											// Quantity-limit fields from special_prescs/unit_of_measure, if found.
											"			COALESCE (	MAX (sp.qty_lim_ingr),			0 ),						"
											"			COALESCE (	MAX (sp.qty_lim_units),			''),						"
											"			COALESCE (	MAX (unit.short_desc_hebrew),	'')							"	// 41

											// Prescription_drugs is the main table driving the query.
											"	FROM		prescription_drugs	AS pd											"

											// Doctors: match by Doctor ID if Doctor ID Type is zero (= Doc. TZ). Note that
											// each prescription_drugs row can deal with multiple prescriptions, but they
											// are all from the same doctor.
											// SYNTAX NOTE: Left joins do NOT get separated by commas - but after the last
											// left join, we WOULD need a comma before listing other tables. (I used INNER
											// JOIN to avoid that kind of inconsistency.)
											"	LEFT JOIN	doctors			AS	dr												"
											"								ON	pd.doctor_id_type		= 0						"
											"								AND	dr.doctor_id			= pd.doctor_id			"

											// Pd_rx_link is here just so we can link to doctor_presc. Select rows that
											// represent drug sales rather than deletions, and are themselves non-deleted.
											"	LEFT JOIN	pd_rx_link		AS	rxl												"
											"								ON	rxl.prescription_id		= pd.prescription_id	"
											"								AND	rxl.line_no				= pd.line_no			"
											"								AND	rxl.rx_sold_status		> 0						"
											"								AND	rxl.del_flg				= 0						"

											// Doctor_presc provides the specialty description, as well as the doctor's name
											// for "external" doctors (MaccabiDent, hospital doctors, etc.) who aren't in
											// the Maccabi database. Note that since visit numbers are unique, we don't
											// really need the Member TZ number - it's just me being paranoid. I'm including
											// criteria for Doctor Prescription ID and Largo Prescribed just in case the doctor
											// entered different values for Specialty Description - although that probably isn't
											// even possible.
											"	LEFT JOIN	doctor_presc	AS	rx												"
											"								ON	rx.visit_number			= rxl.visit_number		"
											"								AND	rx.member_id			= pd.member_id			"
											"								AND rx.doctor_presc_id		= rxl.doctor_presc_id	"
											"								AND rx.largo_prescribed		= rxl.largo_prescribed	"

											// Special_prescs gives us the limit ingredient and limit units - which may or
											// may not actually be of interest; that's for the calling routine to decide.
											"	LEFT JOIN	special_prescs	AS	sp												"
											"								ON	sp.member_id			= pd.member_id			"
											"								AND	sp.largo_code			= pd.largo_code			"
											"								AND	sp.special_presc_num	= pd.special_presc_num	"
											"								AND sp.mem_id_extension		= pd.mem_id_extension	"

											// Unit_of_measure gives us the Hebrew description of the ishur quantity-limit unit.
											"	LEFT JOIN	unit_of_measure	AS	unit											"
											"								ON	unit.unit_abbreviation	= sp.qty_lim_units		"

											// Just for consistency, use INNER JOIN here instead of our usual "WHERE" syntax.
											// The actual "WHERE" clause will be used only to restrict which prescription_drugs
											// rows are included in the output.
											"	INNER JOIN	pharmacy		AS	ph												"
											"								ON	ph.pharmacy_code		= pd.pharmacy_code		"
											"								AND ph.del_flg				= 0						"

											"	INNER JOIN	prescr_source	AS	src												"
											"								ON	src.pr_src_code			= pd.presc_source		"

											"	INNER JOIN	drug_list		AS	dl												"
											"								ON	dl.largo_code			= pd.largo_code			"

											// And finally, here is the WHERE clause for controlling which prescription_drugs
											// rows will be SELECTed! (Note: pd.largo_code > 0 is there just to conform to the
											// tables's index structure - not that I don't trust SQL Server's optimization or
											// anything...
											// First, some basic stuff that applies in all cases:
											"	WHERE	pd.member_id		=  ?												"
											"	  AND	pd.mem_id_extension	=  ?												"
// REPLACED!								"	  AND	pd.date				>= ?												"
											"	  AND	pd.largo_code		>  0												"
											"	  AND	pd.delivered_flg	=  1												"
											"	  AND	pd.del_flg			=  0												"

											// In most cases, we want only prescription sales.
											"	  AND	pd.presc_source		>= ?												"

											// In most cases, we want only "treatment" drugs.
											// DonR 30Dec2025: In fact, we do *not* generally want to exclude
											// non-"treatment" items. I'll leave the logic here alone, but the
											// relevant switch (to include non-"treatment" items) in the calling
											// routine is always TRUE except for Download Type 3, which is not
											// currently in real-world use.
											"	  AND	(	(dl.largo_type in ('T', 'M')) OR (? <> 0)	)						"

											// We can select all drug types OR opioids OR ADHD drugs OR both
											// opioids and ADHD drugs.
											"	  AND	(										(? <> 0)						"
											"				OR	(	(dl.drug_type = 'N') AND	(? <> 0)	)					"	// N = Opioids.
											"				OR	(	(dl.drug_type = 'T') AND	(? <> 0)	)	)				"	// T = Narcotic stimulants for ADHD.

											// DonR 06Jan2026: Instead of a single date select for everything,
											// use three separate dates derived from three new date ranges from
											// pharmacy_type_params, based on the Drug Type of the item sold.
											"	  AND	((pd.date >= ?) OR (dl.drug_type !=		'N'			))					"	// Min. date for opioids.
											"	  AND	((pd.date >= ?) OR (dl.drug_type !=		'T'			))					"	// Min. date ADHD stimulants.
											"	  AND	((pd.date >= ?) OR (dl.drug_type IN	(	'N', 'T')	))					"	// Min. date for "regular" items.

											// Finally, we need to add the GROUP BY clause mentioned in the comment at
											// the beginning of the query - this tells the database server to return
											// exactly one row of output for each prescription_drugs row we SELECT.
											"	GROUP BY pd.prescription_id, pd.line_no											";

					NumOutputColumns	=	41;	// Added dl.economypri_group as output column 34.
					OutputColumnSpec	=	" int, int, int, int, int, char(30), short, char(15), short, int,							"	// 10
											" int, short, short, short, short, double, double, double, short, short,					"	// 20
											" short, int, char(3), int, int, short, short, char(6), varchar(20), varchar(200),			"	// 30
											" char(10), varchar(100), short, int, char(8), char(14), char(25), int, short, char(3),		"	// 40
											" char(8)																					";	// 41
					NumInputColumns		=	10;
					InputColumnSpec		=	" int, short, short, bool, bool, bool, bool, int, int, int ";
					StatementIsSticky	=	1;	// High volume *and* a lot of complexity!
					break;


		case TR6011_IshurDownload:
					SQL_CommandText		=	"	SELECT		main_largo_code,		special_presc_num,		treatment_start,	"
											"				stop_use_date,			qty_lim_flg,			qty_lim_per_day,	"
											"				qty_lim_treat_days,		qty_lim_course_len,		qty_lim_courses,	"
											"				largo_code,															"

											"				COALESCE (	unit.short_desc_hebrew,	'')								"

											"	FROM		special_prescs	AS sp												"
											"	LEFT JOIN	unit_of_measure	AS	unit											"
											"								ON	unit.unit_abbreviation	= sp.qty_lim_units		"

											"	WHERE		member_id			=  ?						"
											"	  AND		largo_code			>  0						"	// Just to conform to index structure.
											"	  AND		confirmation_type	=  1						"	// Ishur is active.
											"	  AND		stop_use_date		>= ?						"	// Ishur is still OK on Close Date.
											"	  AND		treatment_start		<= ?						"	// Ishur is active as of today.
											"	  AND		mem_id_extension	=  ?						"

											// DonR 18Dec2025: If there are duplicate valid ishurim for the same
											// member/Largo Code, we want to report only the one that will actually
											// be used when the member buys drugs. This is whichever one has the
											// most recent timestamp - so we can screen out the older copies with
											// this NOT EXISTS test.
											// NOTE: In the timestamp comparison, we have to use "greater than"
											// if there are two ishurim with the same update date; if we use ">=",
											// (A) we would suppress *both* copies (since the condition would be
											// TRUE in both directions), and (B) the SQL server seems to have a
											// strange problem with ">=" in this subquery anyway - it kills the
											// whole output.
											"	  AND		NOT EXISTS																	"
											"					(SELECT	*	FROM	special_prescs as sp2								"
											"								WHERE	sp2.member_id			=  sp.member_id				"
											"								  AND	sp2.largo_code			=  sp.largo_code			"
											"								  AND	sp2.confirmation_type	=  1						"
											"								  AND	sp2.stop_use_date		>= ?						"
											"								  AND	sp2.treatment_start		<= ?						"
											"								  AND	sp2.mem_id_extension	=  sp.mem_id_extension		"
											"								  AND	(	sp2.date > sp.date								"
											"												OR											"
											"											(sp2.date = sp.date AND sp2.time > sp.time)		"
											"										)													"
											"					)																		"

											"	ORDER BY	special_presc_num, largo_code												";
					NumOutputColumns	=	11;
					OutputColumnSpec	=	" 	int, int, int, int, short, double, short, short, short, int, char(8)	";
					NumInputColumns		=	6;
					InputColumnSpec		=	" int, int, int, short, int, int ";
					StatementIsSticky	=	1;	// High volume.
					break;


		case TR6011_WriteAudit:
					SQL_CommandText		=	"	INSERT INTO	trn_6011_audit													"
											"	(																			"
											"		pharmacy_code,			pharmacy_institution_code,	terminal_id,		"
											"		date,					time,						member_id,			"
											"		member_id_code,			card_date_passed,			presc_source,		"
											"		visit_number_1,			visit_number_2,				pharmacist_id,		"
											"		pharmacist_id_type,		error_code,					sold_drugs_count,	"
											"		ishurim_count,			service_without_card							"
											"	)																			";
					NumInputColumns		=	17;
					InputColumnSpec		=	"	int, short, short, int, int, int, short, short, short, long,	"
											"	long, int, short, short, short, short, short					";
					GenerateVALUES		=	1;
					break;


		case TR6102_READ_PrescriptionCount:
					SQL_CommandText		=	"	SELECT	CAST (count(*) AS INTEGER)		"

											"	FROM 	doctor_presc					"

											"	WHERE	member_id			=  ?		"
											"	  AND	member_id_code		=  ?		"
											"	  AND	valid_until_date	>= ?		"
											"	  AND	sold_status			<  2		"
											"	  AND	deleted_status		=  0		";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	" int ";
					NumInputColumns		=	3;
					InputColumnSpec		=	" int, short, int ";
					Convert_NF_to_zero	=	1;
//					StatementIsSticky	=	1;
					break;


		case TR6103_READ_gadgets:			// This query attempts to find a gadgets row that will work for a given Largo
											// Code, without knowing anything else about it; so, lacking a Gadget Code sent
											// by the pharmacy, it just SELECTs the first one it finds.
					SQL_CommandText		=	"	SELECT	{FIRST} 1															"
											"			gadget_code,			service_code,		service_number,			"
											"			service_type,			enabled_without_rx,	PrevPurchaseClassCode,	"
											"			MinPrevPurchases,		MaxPrevPurchases							"

											"	FROM	gadgets																"

											"	WHERE	largo_code				= ?											"
											"	  AND	((enabled_pvt_pharm > 0) OR (? <> 0))								"
											"	  AND	(((enabled_mac	> 0) AND (?	> 0))	OR								"
											"			 ((enabled_hova	> 0) AND (?	> 0))	OR								"
											"			 ((enabled_keva	> 0) AND (?	> 0)))									"

											"	ORDER BY	gadget_code														";
					NumOutputColumns	=	8;
					OutputColumnSpec	=	"	int, int, short, short, short, short, short, short	";
					NumInputColumns		=	5;
					InputColumnSpec		=	"	int, short, short, short, short	";
					break;


		// DonR 07Nov2022 BUG 272058 FIX: If this is an ishur that's "attached" to a particular pharmacy,
		// Maccabi pharmacies other than the "attached" one can still sell the drug with the ishur
		// discount - they just get a ERR_SPEC_PR_FOR_OTHER_PHARM_WRN warning message in Transaction
		// 6003. Accordingly, we need these ishurim to be visible to all Maccabi pharmacies in
		// Transaction 6001/6101. To support this, added a second instance of the Maccabi Pharmacy
		// flag to the relevant SQL as the 8th input parameter.
		// 
		// DonR 07Nov2022 BUG 275899 FIX: The only use of the Ishur Source is to send it to Nihul Tikrot - and
		// for this purpose, we want ishur_source_send (which is equal to 1 or 3) rather than spec_pres_num_sors.
		case TR6103_READ_special_prescs:
					SQL_CommandText		=	"	SELECT {FIRST} 1	member_price_code,		const_sum_to_pay,		pharmacy_code,	"
											"						priv_pharm_sale_ok,		largo_code,				tikra_flag,		"
											"						ishur_source_send,		tikra_type_code,		insurance_used,	"
											"						health_basket_new												"

											"	FROM				special_prescs													"

											"	WHERE				member_id				=  ?									"
											"	  AND				largo_code				=  ?									"
											"	  AND				stop_use_date			>= ?									"
											"	  AND				treatment_start			<= ?									"
											"	  AND				confirmation_type		=  1									"	// Active ishur.
											"	  AND				member_price_code		>  0									"	// This is an ishur that actually does set participation.
											"	  AND				(priv_pharm_sale_ok		<> 1 OR ? <> 0)							"
											"	  AND				(priv_pharm_sale_ok		<> 6 OR ? =  0)							"
//											"	  AND				(pharmacy_code			=  ? OR pharmacy_code = 0)				"
											"	  AND				(pharmacy_code			=  ? OR pharmacy_code = 0 OR ? <> 0)	"	// DonR 07Nov2022 BUG 272058 FIX.
											"	  AND				mem_id_extension		=  ?									"
//											"	  AND				del_flg					=  0									"	// Del_flg is not used in special_prescs.
								  
											"	ORDER BY			stop_use_date DESC												";
					NumOutputColumns	=	10;
					OutputColumnSpec	=	"	short, int, int, short, int, short, short, short, short, short	";
					NumInputColumns		=	9;
					InputColumnSpec		=	"	int, int, int, int, short, short, int, short, short	";
//					StatementIsSticky	=	1;
					break;


		// DonR 28Apr2025 User Story #390071: For Transaction 6103, add a (fairly) simple
		// lookup of the doctor-chosen Rule Number from the doctor prescriptions table.
		// In real life, we really should never see more than one row satisfy the WHERE
		// criteria - but we'll use a SELECT {FIRST} / ORDER BY just to be paranoid.
		case READ_Rx_DoctorRuleNumber:
					SQL_CommandText		=	"	SELECT	{FIRST} 1 rule_number		"

											"	FROM	doctor_presc				"

											"	WHERE	member_id			=  ?	"
											"	  AND	member_id_code		=  ?	"
											"	  AND	doctor_id 			=  ?	"
											"	  AND	doctor_presc_id		=  ?	"
											"	  AND	largo_prescribed	=  ?	"
											"	  AND	valid_from_date		>= ?	"	// The passed parameter will be the Visit Date.

											"	ORDER BY	rule_number DESC		";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					NumInputColumns		=	6;
					InputColumnSpec		=	"	int, short, int, int, int, int	";
//					StatementIsSticky	=	1;
					break;


		case READ_service_url_components:
					SQL_CommandText		=	"	SELECT	service_desc,				scheme,						"
											"			username,					password,					"
											"			host,						port,						"
											"			path,						method,						"
											"			num_additional_URL_bytes,	GrowOutputBufferIncrement	"

											"	FROM	service_url_components			"

											"	WHERE	service_name = ?				";
					NumOutputColumns	=	10;
					OutputColumnSpec	=	" varchar(100), char(10), varchar(100), varchar(100), varchar(100), int, varchar(500), char(10), short, int ";
					NumInputColumns		=	1;
					InputColumnSpec		=	" char(40) ";
					break;


		// Marianna 22Apr2025: User Story #391697
		case READ_service_http_headers:
					SQL_CommandText		=	"	SELECT	header_name, header_value	"
											"	FROM	service_header_values		"
											"	WHERE	service_name = ?			";
					NumOutputColumns	=	2;
					OutputColumnSpec	=	"	char(40), char(255)	";
					NumInputColumns		=	1;
					InputColumnSpec		=	"	char(40)	";
					break;
