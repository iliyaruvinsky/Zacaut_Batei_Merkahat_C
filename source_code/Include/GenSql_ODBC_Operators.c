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
		//
		// Note that this code is inserted inside a switch statement - all you need to do is supply the cases.
		// The operator ID's for all programs are defined in include/MacODBC_MyOperatorIDs.h.


		case RefreshSeverityTable_cur:
					SQL_CommandText		=	"	SELECT error_code, severity_level FROM pc_error_message	";
					NumOutputColumns	=	2;
					OutputColumnSpec	=	"	short, short,	";
					break;


		case READ_GET_ERROR_SEVERITY:
					SQL_CommandText		=	"	SELECT severity_level FROM pc_error_message WHERE error_code = ?	";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	short	";
					NumInputColumns		=	1;
					InputColumnSpec		=	"	short	";
					break;


		case READ_GetComputerShortName:
											// DonR 08Feb2021: Added FIRST 1 / ORDER BY so that the query won't throw an error if
											// there is more than one row for the same host name. This allows us to get new rows
											// ready for future use, as long as the "active" flag is set FALSE (= 0).
					SQL_CommandText		=	"	SELECT {FIRST} 1 short_name FROM presc_per_host WHERE host_name = ?	ORDER BY active DESC";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	char(2)	";
					NumInputColumns		=	1;
					InputColumnSpec		=	"	varchar(255)	";
					break;


											// DonR 25Aug2020: Added DB pointer to SQL_GetMainOperationParameters() argument list
											// so we can make this particular SQL operation work automatically for either Informix
											// or MS-SQL; it's the only one (so far) where the SQL required is completely different
											// between the two databases.
		case READ_GetCurrentDatabaseTime:
					SQL_CommandText		=	(Database == &INF_DB)	?	"	SELECT cast (CURRENT HOUR TO SECOND AS CHAR(8)) FROM sysparams	"	// Informix version
																	:	"	SELECT CONVERT (VARCHAR(8), GETDATE(), 108) 'hh:mi:ss'	";			// MS-SQL version
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	varchar(50)	";
					break;


		case READ_max_messages_details_rec_id:
					SQL_CommandText		=	"	SELECT	MAX (rec_id)		"
											"	FROM	messages_details	"
											"	WHERE	rec_id >= ?			"
											"	  AND	rec_id <  ?			";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					NumInputColumns		=	2;
					InputColumnSpec		=	"	int, int	";
					break;


//		case READ_max_presc_delete:
//					SQL_CommandText		=	"	SELECT	MAX (deletion_id)	"
//											"	FROM	presc_delete		"
//											"	WHERE	deletion_id >= ?	"
//											"	  AND	deletion_id <  ?	";
//					NumOutputColumns	=	1;
//					OutputColumnSpec	=	"	int	";
//					NumInputColumns		=	2;
//					InputColumnSpec		=	"	int, int	";
//					break;
//
//
		case READ_max_prescripton_id:
					SQL_CommandText		=	"	SELECT	MAX (prescription_id)		"
											"	FROM	prescriptions				"
											"	WHERE	prescription_id >= ?		"
											"	  AND	prescription_id <  ?		";
					NumOutputColumns	=	1;
					OutputColumnSpec	=	"	int	";
					NumInputColumns		=	2;
					InputColumnSpec		=	"	int, int	";
					break;


		case READ_presc_per_host:
					SQL_CommandText		=	"	SELECT	low_limit,			upper_limit,		msg_low_limit,		"
											"			msg_upr_limit,		delpr_low_limit,	delpr_upr_limit		"

											"	FROM	presc_per_host												"

											"	WHERE	host_name	=  ?											"
											"	  AND	active		<> 0											";
					NumOutputColumns	=	6;
					OutputColumnSpec	=	"	int, int, int, int, int, int	";
					NumInputColumns		=	1;
					InputColumnSpec		=	"	varchar(255)	";
					break;


		case SET_isolation_committed:
					SQL_CommandText		=	"	SET TRANSACTION ISOLATION LEVEL READ COMMITTED		";
					NumOutputColumns	=	0;
					OutputColumnSpec	=	"";
					NumInputColumns		=	0;
					InputColumnSpec		=	"";
					StatementIsSticky	=	1;
					break;


		case SET_isolation_dirty:
					SQL_CommandText		=	"	SET TRANSACTION ISOLATION LEVEL READ UNCOMMITTED	";
					NumOutputColumns	=	0;
					OutputColumnSpec	=	"";
					NumInputColumns		=	0;
					InputColumnSpec		=	"";
					StatementIsSticky	=	1;
					break;


		case SET_isolation_repeatable:
					SQL_CommandText		=	"	SET TRANSACTION ISOLATION LEVEL REPEATABLE READ		";
					NumOutputColumns	=	0;
					OutputColumnSpec	=	"";
					NumInputColumns		=	0;
					InputColumnSpec		=	"";
					StatementIsSticky	=	1;
					break;


		// DonR 27Mar2022: Add stuff for Microserver servers.
		case insert_microservice_server_audit:
					SQL_CommandText		=	"	INSERT INTO micro_server_audit													"
											"				(	computer_id,		pid,			server_type,				"
											"					server_name,		date,			time,						"
											"					err_code,			msg_count,		avg_time_microseconds,		"
											"					end_signal													)	";
					NumInputColumns		=	10;
					InputColumnSpec		=	"	char(2), int, short, char(30), int, int, int, int, double, short	";
					GenerateVALUES		=	1;
					break;


		case update_microservice_server_audit:
					SQL_CommandText		=	"	UPDATE	micro_server_audit				"
											"	SET		date					= ?,	"
											"			time					= ?,	"
											"			err_code				= ?,	"
											"			msg_count				= ?,	"
											"			avg_time_microseconds	= ?,	"
											"			end_signal				= ?		"
											"	WHERE	computer_id				= ?		"
											"	  AND	pid						= ?		";
					NumInputColumns		=	8;
					InputColumnSpec		=	"	int, int, int, int, double, short, char(2), int	";
					StatementIsSticky	=	1;	// Since this statement will execute a lot.
					break;


		case READ_sysparams:
											// DonR 21Aug2023: Got rid of max_future_presc, which was redundant - all we need is sale_ok_early_days.
											// Instead of "bubbling" up all the other parameters, moved AdjustCalculateDurationPercent from the end
											// to the tenth position where max_future_presc used to be.
											// DonR 19Oct2023 User Story #487690: Add svc_w_o_card_days_pvt to control semi-permanent service-without-card
											// at private pharmacies.
											// DonR 25Jan2024 User Story #545773: Added mac_pharm_max_gsl_op_per_day after its "twin" parameter
											// mac_pharm_max_otc_op_per_day.
											// DonR 28Jan2024 User Story #540234: Added five new parameters for Cannabis sale options.
											// DonR 23Jun2024 User Story #318200: Add two new "expensive drug" columns. The old column expensive_drug_prc
											// should be dropped when this change moves to Production. (A bunch of old doctors-system columns should also
											// be dropped!)
											// DonR 30Jul2024: Dropped the old NIU expensive_drug_prc column. Added a new column, NihulTikrotLargoLen,
											// in its place (to make the change to the query as simple as possible).
											// DonR 25Nov2024 User Story #366220: Added three new short integers to sysparams.
											// DonR 28Oct2025 User Story #429086: Four new columns for non-digital opioid prescriptions.
					SQL_CommandText		=	"	SELECT	min_refill_days,				NihulTikrotLargoLen,			nihultikrotenabled,						"
											"			vat_percent,					OverseasMaxSaleAmt,				enablepplusmeishar,						"
											"			maxdailybuy_pvt,				maxdailybuy_pplus,				maxdailybuy_mac,						"
											"			AdjustCalculateDurationPercent,	ishur_cycle_ovrlap,				sale_ok_early_days,						"
											"			sale_ok_late_days,				nocard_ph_maxvalid,				nocard_ph_pvt_kill,						"
						
											"			nocard_ph_mac_kill,				round_pkgs_2_pharm,				round_pkgs_sold,						"
											"			del_valid_months,				use_sale_tables,				test_sale_equality,						"
											"			check_card_date,				ddi_pilot_enabled,				no_show_reject_num,						"
											"			no_show_hist_days,				no_show_warn_code,				no_show_err_code,						"
											"			svc_w_o_card_days,				usage_memory_mons,				max_order_sit_days,						"
						
											"			max_order_work_hrs,				IngrSubstMaxRatio,				max_unsold_op_pct,						"
											"			max_units_op_off,				bakarakamutit_mons,				max_otc_credit_mon,						"
											"			sick_disc_4_types,				vent_disc_4_types,				memb_disc_4_types,						"
											"			MinNormalMemberTZ,				pkg_price_for_sms,				future_sales_max_hist,					"
											"			future_sales_min_hist,			mac_pharm_max_otc_op_per_day,	mac_pharm_max_gsl_op_per_day,			"

											"			EnableSecretInvestigatorLogic,	DarkonaiMaxHishtatfutPct,		online_sale_completion_max_delay_min,	"
											"			alt_price_list_first_date,		alt_price_list_last_date,		alt_price_list_code,					"
											"			alt_price_list_only_if_cheaper,	svc_w_o_card_days_pvt,			CannabisSaleEarlyDays,					"
											"			CannabisAllowFutureSales,		CannabisAllowDiscounts,			CannabisCallNihulTikrot,				"
											"			CannabisPermitPerPharmacy,		ExpensiveDrugMacDocRx,			ExpensiveDrugNotMacDocRx,				"

											"			MaxShortDurationDays,			MaxShortDurationOverlapDays,	MaxLongDurationOverlapDays,				"
											"			MaxDoctorRuleConfirmAuthority,	MaxPharmIshurConfirmAuthority,	DrugShortageRuleConfAuthority,			"
											"			default_narco_max_duration,		default_narco_max_validity,		exempt_narco_max_duration,				"
											"			exempt_narco_max_validity																				"

											"	FROM	sysparams	";
					NumOutputColumns	=	70;	// DonR 28Oct2025 User Story #429086.
					OutputColumnSpec	=	"			short,							short,									short,		"
											"			real,							int,									short,		"
											"			int,							int,									int,		"
											"			short,							int,									short,		"
											"			short,							int,									int,		"
						
											"			int,							short,									short,		"
											"			short,							short,									short,		"
											"			short,							short,									short,		"
											"			short,							short,									short,		"
											"			short,							short,									short,		"
						
											"			short,							double,									short,		"
											"			float,							short,									int,		"
											"			char(20),						char(20),								char(20),	"
											"			int,							int,									short,		"
											"			short,							short,									short,		"
						
											"			short,							short,									short,		"
											"			int,							int,									short,		"
											"			short,							short,									short,		"
											"			short,							short,									short,		"
											"			short,							int,									int,		"

											"			short,							short,									short,		"
											"			short,							short,									short,		"
											"			short,							short,									short,		"
											"			short																				";
					break;

