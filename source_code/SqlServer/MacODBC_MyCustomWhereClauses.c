

/*
		case WHERE_TEMPMEMB:	// Dummy just to show what needs to be set.
					WhereClauseText		=	"	cardid = ?	";
					NumInputColumns		=	1;
					InputColumnSpec		= "int";
					break;
*/

		case CheckForServiceWithoutCard_READ_MaccabiPharmacy:
					WhereClauseText		=	"	=	1	";
					break;


		case CheckForServiceWithoutCard_READ_PrivatePharmacy:
					WhereClauseText		=	"	<>	1	";
					break;


		case ChooseNewlyUpdatedRows:
					WhereClauseText		=	"				(update_date = ? AND update_time >= ?)							"
											"	   OR		 update_date > ?												"
											"	ORDER BY	update_date, update_time										";
					NumInputColumns		=	3;
					InputColumnSpec		=	"	int, int, int	";
					break;


		case Find_diagnosis_from_member_diagnoses:
					WhereClauseText		=	"	SELECT DISTINCT	diagnosis_code				"
											"	FROM			member_diagnoses			"
											"	WHERE			member_id			= ?		"
											"		AND			mem_id_extension	= ?		"
											"		AND			diagnosis_code		> 0		";
					NumInputColumns		=	2;
					InputColumnSpec		=	"	int, short	";
					break;


		case Find_diagnosis_from_special_prescs:
					WhereClauseText		=	"	SELECT DISTINCT	member_diagnosis			"
											"	FROM			special_prescs				"
											"	WHERE			member_id			=  ?	"
											"		AND			largo_code			>  0	"	// Just to conform to index structure.
											"		AND			confirmation_type	=  1	"	// Ishur is active.
											"		AND			stop_use_date		>= ?	"	// Ishur is still OK on Close Date.
											"		AND			treatment_start		<= ?	"
											"		AND			mem_id_extension	=  ?	"
											"		AND			member_diagnosis	>  0	";
					NumInputColumns		=	4;
					InputColumnSpec		=	"	int, int, int, short	";
					break;


		case Largo_99997_ishur_standard_WHERE:
					WhereClauseText		=	"			member_id			=  ?		"
											"	  AND	largo_code			=  99997	"
											"	  AND	mem_id_extension	=  ?		"
											"	  AND	confirmation_type	=  1		"	// Ishur is active.
											"	  AND	treatment_start		<= ?		"
											"	  AND	stop_use_date		>= ?		";	// Ishur is still OK on Close Date.
					NumInputColumns		=	4;
					InputColumnSpec		=	"	int, short, int, int	";
					break;

// DonR 30Jun2020: This version of READ_PriceList is now implemented as a sticky
// operation without a custom WHERE clause.
//		case READ_PriceList_simple:
//					WhereClauseText		=	"			largo_code      = ?		"
//											"	  AND 	price_list_code = ?		";
//					NumInputColumns		=	2;
//					InputColumnSpec		=	"	int, short	";
//					break;
//
//
		case READ_PriceList_800_conditional:
					WhereClauseText		=	"			largo_code      = ?		"
											"	  AND 	price_list_code = 800	"
											"	  AND	macabi_price	> 0		"
											"	  AND	macabi_price	< ?		";	// DonR 07Jan2019 - take this price only if it's lower than what we already found.
					NumInputColumns		=	2;
					InputColumnSpec		=	"	int, int	";
					break;


		case TestPharmacyIshur_MainLargo:
					WhereClauseText		=	"			pharmacy_code		=  ?										"
											"	  AND	member_id			=  ?										"
											"	  AND	mem_id_extension	=  ?										"
											"	  AND	pharm_ishur_num		=  ?										"
											"	  AND	diary_month			>  -999										"	// Just to conform to index structure.
											"	  AND	largo_code			=  ?										"
											"	  AND	ishur_date			=  ?										"	// Ishur is valid only on day of issue.
											"	  AND	(((rule_number		=  3) AND (? = 1)) OR						"
											"			 ((rule_number		<> 3) AND (? = 0)))							"	// DonR 01Aug2019
											"	  AND	used_flg			=  ?										";
					NumInputColumns		=	9;
					InputColumnSpec		=	"	int, int, short, int, int, int, short, short, short	";
					break;


		case TestPharmacyIshur_PreferredLargo:
					WhereClauseText		=	"			pharmacy_code		=  ?										"
											"	  AND	member_id			=  ?										"
											"	  AND	mem_id_extension	=  ?										"
											"	  AND	pharm_ishur_num		=  ?										"
											"	  AND	diary_month			>  -999										"	// Just to conform to index structure.
											"	  AND	largo_code			=  ?										"
											"	  AND	ishur_date			=  ?										"	// Ishur is valid only on day of issue.
											"	  AND	(((rule_number		=  3) AND (? = 1)) OR						"
											"			 ((rule_number		<> 3) AND (? = 0)))							"	// DonR 01Aug2019
											"	  AND	used_flg			=  ?										"
											"	  AND	(preferred_largo	=  ?  AND preferred_largo > 0)				";
					NumInputColumns		=	10;
					InputColumnSpec		=	"	int, int, short, int, int, int, short, short, short, int	";
					break;


		case TestSpecialPresc_FindBestIshur:
											// DonR 19Dec2019 - got rid of the del_flg criterion, since we don't use del_flg in special_prescs.
					WhereClauseText		=	"			member_id			=  ?	"
											"	  AND	mem_id_extension	=  ?	"
											"	  AND	largo_code			=  ?	"
											"	  AND	confirmation_type	=  1	"	// DonR 01Jul2004 - Ishur OK to use.
											"	  AND	treatment_start		<= ?	"
											"	  AND	member_price_code	>  0	"	// DonR 12Sep2012 - Ishur tells us something about pricing.
											"	  AND	stop_use_date		>= ?	"

											// 07Jul2011 - No extension allowed for automatic ishurim.
											"	  AND	(stop_use_date		>= ?	OR	spec_pres_num_sors <> 5)			";
					NumInputColumns		=	6;
					InputColumnSpec		=	"	int, short, int, int, int, int	";
					break;


		case TestSpecialPresc_FindSpecificIshur:
											// DonR 19Dec2019 - got rid of the del_flg criterion, since we don't use del_flg in special_prescs.
					WhereClauseText		=	"			spec_pres_num_sors 	=  ?	"
											"	  AND	special_presc_num  	=  ?	"
											"	  AND	largo_code			=  ?	"
											"	  AND	confirmation_type	=  1	"	// DonR 01Jul2004 - Ishur OK to use.
	  										"	  AND	member_price_code	>  0	"	// DonR 12Sep2012 - Ishur tells us something about pricing.
											"	  AND	treatment_start		<= ?	";
					NumInputColumns		=	4;
					InputColumnSpec		=	"	short, int, int, int	";
					break;


		case TR1014_PriceListCodeAndDate:
					WhereClauseText		=	"	price_list_code = ? AND ((date_macabi = ? AND time_macabi >= ?) OR (date_macabi  > ?))	";
					NumInputColumns		=	4;
					InputColumnSpec		=	"	short, int, int, int	";
					break;


		case TR1014_PriceListCodeOnly:
					WhereClauseText		=	"	price_list_code = ?	";
					NumInputColumns		=	1;
					InputColumnSpec		=	"	short	";
					break;


		case TR1014_PriceListLargoGT90000:
					WhereClauseText		=	"	price_list_code = 1 AND largo_code > 90000	";
					break;


		case TR2001_Rx_cursor_by_Pr_ID:
					WhereClauseText		=	"			member_id		= ?		"
											"	  AND	doctor_presc_id	= ?		"
											"	  AND	doctor_id		= ?		"
											"	  AND	member_id_code	= ?		";
					NumInputColumns		=	4;
					InputColumnSpec		=	"	int, int, int, short	";
					break;



		case TR2001_Rx_cursor_without_Pr_ID:
					WhereClauseText		=	"			member_id		=		?			"
											"	  AND	doctor_id		=		?			"
											"	  AND	valid_from_date	BETWEEN	? AND ?		"
											"	  AND	member_id_code	=		?			";
					NumInputColumns		=	5;
					InputColumnSpec		=	"	int, int, int, int, short	";
					break;


		case TR2003_5003_READ_doctor_presc_by_date:
					WhereClauseText		=	"			member_id			=  ?				"
											"	  AND	doctor_presc_id		=  ?				"
											"	  AND	valid_from_date		=  ?				"
											"	  AND	largo_prescribed	=  ?				"
											"	  AND	doctor_id			=  ?				"
											"	  AND 	member_id_code		=  ?				"
											"	  AND	prescription_type	BETWEEN 0 AND 9		"	// Just to avoid conversion errors if there's a bogus value - which should never happen.
											"	  AND	sold_status			IN (0, 1, 3)		";
					NumInputColumns		=	6;
					InputColumnSpec		=	"	int, int, int, int, int, short	";
					break;


		case TR2003_5003_READ_doctor_presc_by_date_range:
					WhereClauseText		=	"			member_id			=  ?				"
											"	  AND	doctor_presc_id		=  ?				"
											"	  AND	valid_from_date		BETWEEN	? AND ?		"
											"	  AND	largo_prescribed	=  ?				"
											"	  AND	doctor_id			=  ?				"
											"	  AND 	member_id_code		=  ?				"
											"	  AND	prescription_type	BETWEEN 0 AND 9		"	// Just to avoid conversion errors if there's a bogus value - which should never happen.
											"	  AND	sold_status			IN (0, 1, 3)		";
					NumInputColumns		=	7;
					InputColumnSpec		=	"	int, int, int, int, int, int, short	";
					break;


		case TR2003_5003_READ_doctor_presc_date_range_with_largo_subst:
					WhereClauseText		=	"			member_id			=  ?				"
											"	  AND	doctor_presc_id		=  ?				"
											"	  AND	valid_from_date		BETWEEN	? AND ?		"
											"	  AND	largo_prescribed	IN (									"
											"										SELECT	largo_code				"
											"										FROM	drug_list				"
											"										WHERE	economypri_group	> 0	"
											"										  AND	economypri_group	= ?	"
											"								   )									"
											"	  AND	doctor_id			=  ?				"
											"	  AND 	member_id_code		=  ?				"
											"	  AND	prescription_type	BETWEEN 0 AND 9		"	// Just to avoid conversion errors if there's a bogus value - which should never happen.
											"	  AND	sold_status			IN (0, 1, 3)		";
					NumInputColumns		=	7;
					InputColumnSpec		=	"	int, int, int, int, int, int, short	";
					break;


		case TR2003_5003_READ_doctor_presc_date_with_largo_subst:
					WhereClauseText		=	"			member_id			=  ?				"
											"	  AND	doctor_presc_id		=  ?				"
											"	  AND	valid_from_date		=  ?				"
											"	  AND	largo_prescribed	IN (									"
											"										SELECT	largo_code				"
											"										FROM	drug_list				"
											"										WHERE	economypri_group	> 0	"
											"										  AND	economypri_group	= ?	"
											"								   )									"
											"	  AND	doctor_id			=  ?				"
											"	  AND 	member_id_code		=  ?				"
											"	  AND	prescription_type	BETWEEN 0 AND 9		"	// Just to avoid conversion errors if there's a bogus value - which should never happen.
											"	  AND	sold_status			IN (0, 1, 3)		";
					NumInputColumns		=	6;
					InputColumnSpec		=	"	int, int, int, int, int, short	";
					break;


		case TR2090_READ_prescriptions_by_pharmacy_fields:
					WhereClauseText		=	"			pharmacy_code		=  ?	"
											"  AND		diary_month			=  ?	"
											"  AND		presc_pharmacy_id	=  ?	"
											"  AND		member_institued	=  ?	"
											"  AND		member_price_code	=  ?	"
											"  AND		delivered_flg		<> ?	";
					NumInputColumns		=	6;
					InputColumnSpec		=	"	int, short, int, short, short, short	";
					break;


		case TR2090_READ_prescriptions_by_prescription_id:
					WhereClauseText		=	"			prescription_id		=  ?	"
											"  AND		pharmacy_code		=  ?	"
											"  AND		delivered_flg		<> ?	";
					NumInputColumns		=	3;
					InputColumnSpec		=	"	int, int, short	";
					break;
