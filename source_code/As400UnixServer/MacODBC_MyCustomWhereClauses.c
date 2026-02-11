

/*
		case WHERE_TEMPMEMB:	// Dummy just to show what needs to be set.
					WhereClauseText		=	"	cardid = ?	";
					NumInputColumns		=	1;
					InputColumnSpec		= "int";
					break;
*/

		case AS400SRV_UPD_tables_update_conditional:
					WhereClauseText		=	"			table_name		= ?												"
											"	  AND	((update_date	< ?) OR (update_date = ? AND update_time < ?))	";
					NumInputColumns		=	4;
					InputColumnSpec		=	"	varchar(32), int, int, int	";
					break;


		case AS400SRV_UPD_tables_update_unconditional:
					WhereClauseText		=	"			table_name	= ?		";
					NumInputColumns		=	1;
					InputColumnSpec		=	"	varchar(32)	";
					break;


		case AS400SRV_T2003_UPD_special_prescs_with_TZ_code:
												// DonR 01May2012 - Ishur Source is no longer part of the unique key for special_prescs.
					WhereClauseText		=	"			member_id			= ?		"
											"	  AND	largo_code			= ?		"
											"	  AND	special_presc_num	= ?		"
											"	  AND	mem_id_extension	= ?		";
					NumInputColumns		=	4;
					InputColumnSpec		=	"	int, int, int, short	";
					break;


		case AS400SRV_T2003_UPD_special_prescs_without_TZ_code:
												// DonR 01May2012 - Ishur Source is no longer part of the unique key for special_prescs.
					WhereClauseText		=	"			member_id			= ?		"
											"	  AND	largo_code			= ?		"
											"	  AND	special_presc_num	= ?		";
					NumInputColumns		=	3;
					InputColumnSpec		=	"	int, int, int	";
					break;


		case AS400SRV_T4030_READ_drug_extension_old_data_by_largo:
					WhereClauseText		=	"			largo_code			= ?		"
											"	  AND	rule_number			= ?		"
											"	  AND	from_age			= ?		"
											"	  AND	permission_type		= ?		";
					NumInputColumns		=	4;
					InputColumnSpec		=	"	int, int, short, short	";
					break;


		case AS400SRV_T4030_READ_drug_extension_old_data_by_rrn:
					WhereClauseText		=	"			rrn	 = ?		";
					NumInputColumns		=	1;
					InputColumnSpec		=	"	int	";
					break;

