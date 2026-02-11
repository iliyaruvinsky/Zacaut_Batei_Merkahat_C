/*=======================================================================
||																		||
||				PharmacyErrors.h										||
||																		||
||======================================================================||
||																		||
||  PURPOSE:															||
||																		||
||  Define all error codes send to a pharmacy client.					||
||																		||
||----------------------------------------------------------------------||
|| PROJECT:	SqlServer. (for macabi)										||
||----------------------------------------------------------------------||
|| PROGRAMMER:	Boaz Shnapp.											||
|| DATE:	Nov 1996.													||
|| COMPANY:	Reshuma Ltd.												||
|| WORK DONE:	S.Q.L transactions.										||
||----------------------------------------------------------------------||
||		  Make sure that file isn't included 2 times.					||
 =======================================================================*/
#ifndef PHARMACY_ERRORS_H

#define PHARMACY_ERRORS_H
/*=======================================================================
||			   General definitions										||
 =======================================================================*/
/*
 * If not in main -> define all global variables as "extern"
 * ---------------------------------------------------------
 */
#ifdef	MAIN
#define	DEF_TYPE
#else
#define	DEF_TYPE	extern
#endif


#define NO_ERROR			   0

// DonR 04Jun2015: new 3-digit "error" codes for Transaction 6001 participation prediction.
#define TR6001_PH_ISHUR_POSSIBLE			 101	// Equivalent to warning 6119.
#define TR6001_SPECIALIST_POSSIBLE			 102	// Equivalent to warning 6019.
#define TR6001_AS400_ISHUR_REQUIRED			 197	// Equivalent to error 6033.
#define TR6001_PH995482_IN_ISHUR			 198	// Equivalent to error 6091.
#define TR6001_100_PCT_FORBIDDEN			 199	// Equivalent to error 6172.
#define TR6001_SPEC_PR_FOR_OTHER_PHARM_WRN	 200	// Equivalent to error 6081. BUG 272058: 27Oct2022

#define ERR_FILE_TOO_SHORT					3001
#define ERR_FILE_TOO_LONG					3002
#define ERR_WRONG_FORMAT_FILE				3003
#define ERR_UNABLE_OPEN_INPUT_FILE			3004
#define ERR_ILLEGAL_MESSAGE_CODE			3005
#define ERR_UNABLE_TO_WRITE_RESPOND_MSG		3006
#define ERR_INCORRECT_CMD_LINE       		3007
#define ERR_UNABLE_OPEN_OUTPUT_FILE  		3008
#define ERR_NO_MEMORY						3009
#define	ERR_DATABASE_ERROR 					3010
#define ERR_SHARED_MEM_ERROR				3011
#define ERR_OBSOLETE_TRANSACTION_VERSION	3012

// DonR 08Feb2022: Added capability to get a "custom" error code in RK9069/drug_purchase_lim;
// these error codes will be in pc_error_message and will be noted here, but they will not be
// explicitly mentioned in the source code. (They will be read into PurchaseLimit.CustomErrorCode
// and then copied to ThisDrug->PurchaseLimitCustomErrorCode, and then assigned if the member
// has exceeded the relevant Drug Purchase Limit.)
#define CUSTOM_INSULIN_BEFORE_LIBRE_SENSOR	5001

/*=======================================================================
||			 Data-test Error codes.										||
 =======================================================================*/
#define ERR_PHARMACY_NOT_FOUND				6001
#define ERR_INSTITUTE_CODE_WRONG			6002
#define ERR_PHARMACY_TYPE_WRONG				6003
#define ERR_LOCATION_CODE_WRONG				6004
#define ERR_MEMBER_ID_CODE_WRONG			6005
#define ERR_DOCTOR_ID_CODE_WRONG			6006
#define ERR_SPCL_PRES_NOT_FOUND				6007
#define ERR_PARTICI_CODE_NOT_FOUND			6008
#define ERR_MEMBER_PARTICIPATING_WRN		6009
//#define ISHUR_TIKRA_PMT_RECOMMEND			6010	// DonR 25Dec2013 - disabled.

#define ERR_PRESCRIPTION_ID_NOT_FOUND		6011
#define ERR_PRESCRIPTION_ID_NOT_EQUAL		6012
#define ERR_SUPPLIMENT_NOT_APPRUVED			6013
#define ERR_NO_MORE_OTC_CREDIT				6014	// Severity 5.
#define ERR_NO_OUT_STENDING_MESSAGE			6015
#define ERR_DRUG_CODE_NOT_FOUND				6016
#define ERR_PHARMACY_NOT_OPEN				6017
#define ERR_SEVERE_INTERACTION_WARN			6018
#define ERR_SPCLTY_LRG_WRN					6019
#define ERR_PARTICIPATING_CODE_WRONG		6020

#define ERR_MEMBER_NOT_ELEGEBLE				6021
#define ERR_PRICE_CODE_WRONG				6022

// Overdose messages.
#define ERR_OVER_DOSE_WARRNING				6023	// Severity 4/5
#define ERR_DUR_WARRNING					6024	// Severity 4/5
#define ERR_OVER_DOSE_ERROR					6025	// Severity 10
#define ERR_DUR_ERROR						6026	// Severity 10
#define ERR_DRUG_REQUIRE_PRESCRIPTION		6027	// Severity 10
#define ERR_OVER_DOSE_ERROR_DAYS			6028	// 20011014 overdose * 2 Yulia (Severity 10)
#define ERR_NOT_PREFERRED_DRUG_WARNING		6029	// DonR 30Mar2003
#define ERR_100_PERCENT_GETS_DISCOUNT		6030	// DonR 04Dec2003 - Severity 10

#define ERR_RULE_NOT_FOUND_WARNING			6031	// DonR 04Jan2004 - Severity 5
#define ERR_AUTHORITY_LOW_WARNING			6032	// DonR 04Jan2004 - Severity 5
#define ERR_ISHUR_POSSIBLE_ERR				6033	// DonR 08Mar2004 - Severity 10

// Pharmacy Ishur (Trn. 2033) Errors.
#define ERR_NO_ISHUR_TO_EXTEND				6034	// DonR 08Mar2004 - Severity 10
#define ERR_PHARM_ISHUR_NOT_FOUND			6035	// Severity 10
#define ERR_PHARM_ISHUR_ALREADY_USED		6036	// Severity 10
#define ERR_PHARM_ISHUR_NOT_ALL_USED		6037	// Severity 10
#define ERR_PH_ISHUR_NO_MORE_FREEBIES		6038	// Severity 5 - in use from 01Mar2011 through 31Aug2011.
#define ERR_PHARM_ISHUR_NO_SLP_FOUND		6039	// Severity 5
#define ERR_PHARM_ISHUR_SLP_ROW_BAD			6040	// Severity 5

#define ERR_PHARM_ISHUR_SLP_DISCREP			6041	// Severity 5
#define ERR_PHARM_ISHUR_PRICE_HIGHER		6042	// Severity 5
#define ERR_PHARM_ISHUR_DUP_LARGO			6043	// Severity 10
#define ERR_PHARM_ISHUR_NO_DRP_FOUND		6044	// Severity 5
#define ERR_PHARM_ISHUR_DRP_ROW_BAD			6045	// Severity 5
#define ERR_PHARM_ISHUR_RULE_NOT_FOUND		6046	// Severity 10
#define ERR_PHARM_ISHUR_RULE_BAD			6047	// Severity 5
#define ERR_PHARM_ISHUR_RULE_NOT_FIT		6048	// Severity 5
#define ERR_PHARM_ISHUR_RULE_EXTENDED		6049	// Severity 5
#define ERR_NO_DOCTORS_ISHUR				6050	// DonR 28Mar2010 - Severity 10

#define ERR_WORNG_MEMBERSHIP_CODE			6051
#define ERR_QTY_REDUCE_PRC_QTY_OVER_LIM		6052
#define ERR_NO_DOCTORS_ISHUR_WARN			6053	// DonR 28Mar2010 - Severity 1
#define ERR_SUPPL_OP_NOT_MATCH_DRUG_OP		6054
#define ERR_DRUG_PRICE_NOT_FOUND			6055
#define ERR_PRESCRIPTION_SOURCE_REJECT		6056
#define ERR_PREPARATION_INCONSISTENT		6057	// DonR 07Dec2010 reused old NIU code, severity 10.
#define ERR_KEY_NOT_FOUND					6058
#define ERR_PHARM_CANT_SALE_PRECR_SRC		6059
#define ERR_PH_ISHUR_FREEBIE_POSSIBLE		6060	// Severity 5 - in use from 01Mar2011 through 31Aug2011.

#define ERR_QTY_REDUCE_PRC_QTY_LIM_EXT		6061
#define ERR_PARTICIPATING_PERCENT_WRONG		6062
#define ERR_PHARM_ISHUR_RULE_PRESC_ONLY		6063	// Severity 10
#define ERR_SPCL_PRES_OUT_OF_DATE			6064
#define ERR_SPCL_PRES_MEMB_ID_WRONG			6065
#define ERR_ISHUR_EXPIRY_45_WARNING			6066	// Severity 5
#define ERR_DRUG_DONT_MATCH_SPCL_PRES		6067
#define ERR_PHARM_CANT_SALE_SPCL_PRES		6068
#define ERR_SPCL_PRES_PIRIOD_QTY_0			6069
#define ERR_QTY_REDUCE_SPCL_PRES_LIM		6070

#define ERR_PARTI_NOT_FOUND_FOR_DRUG		6071
#define ERR_PRESCRIPTIONID_NOT_APPROVED		6072
#define ERR_PHARMACY_HAS_NO_PERMISSION		6073

// Purchase Limit messages.
#define ERR_DRUG_PURCH_LIMIT_EXCEEDED		6074	// DonR 04Jul2006 Re-used old NIU code.
#define ERR_DRUG_PURCH_ISHUR_EXCEEDED		6075	// DonR 04Jul2006 Re-used old NIU code.
#define WARN_DRUG_PURCH_LIMIT_EXCEEDED		6076	// DonR 04Jul2006 Re-used old NIU code.
#define WARN_DRUG_PURCH_ISHUR_EXCEEDED		6077	// DonR 04Jul2006 Re-used old NIU code.

#define WARN_MEMBER_IN_HOSPITAL				6078	// DonR 02Aug2011.
#define WARN_TZAHAL_NO_DISCOUNT				6079	// DonR 04Dec2011 Tzahal enhancement.
#define ERR_SEVERE_INTERACTION_SAMEDOC		6080	// For statistics only - not sent to pharmacy!

#define ERR_SPEC_PR_FOR_OTHER_PHARM_WRN		6081
#define ERR_MEMBER_PHARM_SALE_FORBIDDEN		6082	// DonR 12Dec2006 Re-used old NIU code.
#define ERR_MEMBER_PHARM_PERMITTED			6083	// DonR 12Dec2006 Re-used old NIU code.
#define ERR_MEMBER_PHARM_PERMITTED_TEMP		6084	// DonR 12Dec2006 Re-used old NIU code.
#define ERR_CANT_DEL_PRESC_OR_DRUGS			6085
#define ERR_MEMBER_LIVES_OVERSEAS			6086	// DonR 12Mar2012 Re-used old NIU code.
#define WARN_MEMBER_LIVES_OVERSEAS			6087	// DonR 12Mar2012 Re-used old NIU code.
#define ERR_BAD_TREAT_CATEGORY_FOR_DOC		6088	// DonR 26Aug2003. Severity = ?
#define ERR_DRUG_S_WRONG_IN_PRESC 			6089
#define ERR_NOT_MACCABICARE_DRUG 			6090

#define ERR_PHARM_995482_IN_ISHUR			6091	// NOTE: As of 26Mar2014, also applies to Pharmacy 997582.
#define ERR_DOC_LOCATION_INVALID			6092	// DonR 10Jul2012 Re-used old NIU code.
#define EXTENSION_NO_KEREN_ERR				6093	// Used only in Trn. 1063.
#define EXTENSION_NO_MAGEN_ERR				6094	// Used only in Trn. 1063.
#define ERR_DUPLICATE_DRUG_IN_SALE			6095	// DonR 11Jul2011: re-used old NIU code.
#define ERR_CODE_NOT_FOUND					6096
#define ERR_PRESC_ALREADY_DELETED			6097
#define ERR_DRUG_ALREADY_DELETED			6098
#define ERR_SPCL_PRES_FREQ_WRONG			6099
#define ERR_DRUG_NOT_IN_ORIG_SALE			6100

#define ERR_QTY_DIFFERENT_IN_ORIG_SALE		6101
#define ERR_CARD_EXPIRED					6102
#define ERR_PHARM_HAND_MADE					6103
#define ERR_LEUMI_NOT_VALID					6104
#define ERR_DOCTOR_EQ_MEMBER				6105
#define ERR_PRESCR_PROBL_DIARY_MONTH		6106
#define ERR_PRESCRIPTION_PROBL_DATE			6107
#define ERR_PRESCRIPTION_DOUBLE_NUM			6108
#define ERR_PRESCR_CARD_GET_WRN				6109
#define	ERR_DISCOUNT_IF_HAD_ZAHAV			6110	// severity 5

#define ERR_OUT_OF_STOCK_WARN				6111	// DonR 20Dec2011.
#define DAILY_PURCHASE_LIMIT_EXCEEDED		6112	// DonR 11Jul2012 Re-used old NIU code.

// DonR 28Mar2004: Warning versions of severe overdose errors - for
// Electronic Prescription.
#define ERR_HALF_DAILY_OD_WARN				6113	// For statistics only - not sent to pharmacy!
#define ERR_ZERO_OVERDOSE_WARN				6114	// For statistics only - not sent to pharmacy!
#define ERR_AVG_OVER_DOSE_WARN				6115	// Severity 5
#define ERR_DOUBLE_DAILY_OD_WARN			6116	// Daily overdose * 2 Severity 5)
#define ERR_OD_INTERACT_NEEDS_REVIEW		6117	// Severity 5

#define	ERR_DISCOUNT_AT_MAC_PH_WARN			6118	// severity 5
#define	ERR_DISCOUNT_WITH_PH_ISHUR_WRN		6119	// DonR 13Jun2012 Re-used old NIU code.
#define	ERR_UNSPEC_ISHUR_DISCNT_AVAIL		6120	// severity 5

#define ERR_ISHUR_EXPIRY_15_WARNING			6121	// Severity 5
#define ERR_ISHUR_EXPIRY_30_WARNING			6122	// Severity 5
#define ERR_USING_EXPIRED_ISHUR_WARN		6123	// Severity 5
#define ERR_ISHUR_DISCNT_PRATI_PLUS			6124	// DonR 13Jun2012 Re-used old NIU code.

// DonR 20Dec2004: New messages for specialist participation.
#define ERR_SPCLTY_LRG_ERR					6125	// Severity 10
#define ERR_SPCLTY_LRG_FP_ERR				6126	// Severity 10
#define ERR_SPCLTY_LRG_FP_WRN				6127	// Severity 5
#define ERR_SPCLTY_LRG_INFORM				6128	// Severity 5
#define ERR_INCONSISTENT_TRANSACTIONS		6129	// DonR 12Jul2012 Re-used old NIU code.

// DonR 21Jun2004: New message codes for "Gadgets".
#define	ERR_GADGET_COMM_PROBLEM				6130	// severity 5
#define	ERR_GADGET_NO_INSURANCE				6131	// severity 5
#define	ERR_GADGET_APPROVED_BASIC			6132	// severity 5
#define	ERR_GADGET_OP_REDUCED_BASIC			6133	// severity 5
#define	ERR_GADGET_ERROR					6134	// severity 5
#define	ERR_DUPLICATE_GADGET_REQ			6135	// severity 10
#define	ERR_GADGET_REQ_WITH_UNITS			6136	// severity 10
#define	ERR_GADGET_APPROVED_KEREN			6137	// severity 5
#define	ERR_GADGET_APPROVED_MAGEN			6138	// severity 5
#define	ERR_GADGET_OP_REDUCED_KEREN			6139	// severity 5
#define	ERR_GADGET_OP_REDUCED_MAGEN			6140	// severity 5

#define	ERR_GADGET_APPROVED_UNKNOWN			6141	// severity 5
#define	ERR_GADGET_OP_REDUCED_UNKNOWN		6142	// severity 5
#define	ERR_GADGET_DEADBEAT_MEMBER			6143	// severity 5
#define	ERR_GADGET_REQ_DISCREPANCY			6144	// severity 5
#define	ERR_GADGET_NOT_ELIGIBLE				6145	// severity 5
#define	ERR_GADGET_ALREADY_BOUGHT			6146	// severity 5
//		ERR_GADGET_APPROVED_YAHALOM			6153	// Copying here just for clarity.
//		ERR_GADGET_OP_REDUCED_YAHALOM		6154	// Copying here just for clarity.
//		ERR_GADGET_TRAFFIC_ACCIDENT			6178	// Copying here just for clarity.
//		GADGET_APPROVED_WORK_ACCIDENT		6179	// Copying here just for clarity.

#define ERR_PR_DATE_IN_FUTURE				6147	// DonR 11Sep2012 - severity 10.
#define DAILY_PURCH_LIM_EXCD_MACCABI		6148	// DonR 12Sep2012.
#define	ERR_DISCOUNT_IF_HAD_YAHALOM			6149	// DonR 11Dec2012 "Yahalom" - severity 5

// Early Refill messages.
#define ERR_EARLY_REFILL_CHEAP_WARNING		6150	// severity 5
#define ERR_EARLY_REFILL_EXPENSIVE_WARN		6151	// severity 6
#define ERR_EARLY_REFILL_WARNING			6152	// severity 6

#define	ERR_GADGET_APPROVED_YAHALOM			6153	// DonR 11Dec2012 "Yahalom" - severity 5
#define	ERR_GADGET_OP_REDUCED_YAHALOM		6154	// DonR 11Dec2012 "Yahalom" - severity 5
#define ERR_ISHUR_INGR_TOTAL_WARN			6155	// severity 2
#define ERR_ISHUR_INGR_COURSE_ERR			6156	// DonR 07Jul2013: restored to use.
#define ERR_ISHUR_INGR_COURSE_WARN			6157	// severity 2
#define ERR_ISHUR_INGR_TOTAL_ERR			6158	// DonR 07Jul2013: New code.
#define ISHUR_INGR_COURSE_100_MSG			6159	// DonR 24Jul2013: New, severity = 3 (not reported to pharmacy)

#define ERR_AUTO_CREDIT_PMT_W_CARD_MSG		6160	// severity 5
#define ERR_MUST_PASS_2_CARDS_TO_BUY		6161	// severity 4
#define ERR_MISSING_USAGE_INSTRUCTIONS		6162	// severity 5
#define ISHUR_INGR_COURSE_234_MSG			6163	// DonR 24Jul2013: New, severity = 3 (not reported to pharmacy)
#define ISHUR_INGR_COURSE_456_MSG			6164	// DonR 24Jul2013: New, severity = 3 (not reported to pharmacy)

// DonR 16Sep2013: New internal messages for adjusted AS/400 ishur ingredient usage - all severity 3 (not reported to pharmacy).
#define ADJ_ISHUR_INGR_TOTAL_ERR			6165
#define ADJ_ISHUR_INGR_TOTAL_WARN			6166
#define ADJ_ISHUR_INGR_COURSE_ERR			6167
#define ADJ_ISHUR_INGR_COURSE_WARN			6168
#define ADJ_ISHUR_COURSE_100_MSG			6169
#define ADJ_ISHUR_COURSE_234_MSG			6170
#define ADJ_ISHUR_COURSE_456_MSG			6171

#define ERR_MACCABI_100PCT_FORBIDDEN		6172	// DonR 02Dec2013 - severity 10
#define COULD_BUY_WITHOUT_PRESC				6173	// DonR 24Dec2014 - NO LONGER RELEVANT.
#define WHITE_PRESCRIPTION_PASS_2_CARDS		6174	// DonR 24Dec2013 - severity 4
#define HAND_PRESCRIPTION_PASS_2_CARDS		6175	// DonR 24Dec2013 - severity 4
#define LONG_DURATION_WARNING				6176	// DonR 21May2014 - severity 5
#define ERR_MACCABI_100_PER_CENT			6177	// DonR 02Dec2013 - severity 3 (not reported to pharmacy)
#define	ERR_GADGET_TRAFFIC_ACCIDENT			6178	// severity 5
#define	GADGET_APPROVED_WORK_ACCIDENT		6179	// severity 5
#define DRUG_DISEASE_MISMATCH_NO_DISCOUNT	6180	// severity 5
#define SHABAN_NOT_YET_APPLICABLE			6181	// DonR 19Jan2015 - severity 3 (not reported to pharmacy)
#define ERR_DUPLICATE_GADGET_ROW			6182	// DonR 01Sep2015

// DonR 30Aug2015: 29-G stuff.
#define FORM_29G_NOT_FOUND					6183
#define FORM_29G_EXP_IN_60_DAYS				6184
#define FORM_29G_EXP_IN_45_DAYS				6185
#define FORM_29G_EXP_IN_30_DAYS				6186
#define FORM_29G_EXP_IN_15_DAYS				6187

#define HAND_RX_MACCABI_NEED_2_CARDS		6188
#define PREV_PARTIAL_SALE_NEED_1_CARD		6189
#define PREV_PARTIAL_SALE_NEED_2_CARDS		6190
#define DELETION_ALLOWED_FOR_EX_MEMBER		6191
#define RX_FOUND_BUT_CARD_NOT_PASSED		6192
#define NARCOTICS_HYPNOTICS_RX_ONLY_ERR		6193
#define EMERGENCY_SUPPLY_100_PERCENT		6194
#define DRUG_RX_SOURCE_MEMBER_FORBIDDEN		6195
#define WARN_CARD_EXPIRED					6196
#define WARN_INGR_BASED_SUBSTITUTION		6197
#define ERR_DISALLOWED_SUBSTITUTION			6198
#define	AL_TOR_NO_SHOW_WARNING				6199
#define	AL_TOR_NO_SHOW_ERROR				6200

#define ONLINE_ORDER_NOT_FOUND				6201
#define PHARM_NOT_ORDER_ORIGINATOR			6202
#define PHARM_NOT_ORDER_FULFILLER			6203
#define ONLINE_ORDER_WRONG_MEMBER			6204
#define ONLINE_ORDER_EXPIRED				6205
#define ONLINE_ORDER_EXCESS_WORKTIME		6206
#define	ORDER_REQ_NOT_VALID_FOR_COMPLETION	6207

#define	WARN_METHOD_3_4_LIMIT_EXCEEDED		6208
#define	WARN_METHOD_3_4_ISHUR_EXCEEDED		6209
#define	ERR_METHOD_3_4_LIMIT_EXCEEDED		6210
#define	ERR_METHOD_3_4_ISHUR_EXCEEDED		6211
#define	WARN_METHOD_3_4_LAST_ALLOWED		6212
#define	WARN_METHOD_3_4_ISHUR_LAST_ALLOWED	6213
#define	ERR_EXCESSIVE_INGR_SUBSTITUTION		6214
#define	BARCODE_NOT_SCANNED_PASS_2_CARDS	6215
#define	COUPON_EXPIRES_WITHIN_1_MONTH		6216
#define	COUPON_EXPIRES_WITHIN_2_MONTHS		6217
#define	COUPON_EXPIRING_ADDITIONAL_TEXT		6218
#define	MEISHAR_DISCOUNT_REQUIRES_CARD		6219

#define	GADGET_PREG_QTY_NOT_USED			6220
#define	GADGET_PREG_NO_BASKET_OPENED		6221
#define	GADGET_PREG_STILL_WAIT_PERIOD		6222
#define	GADGET_PREG_BAD_INPUT_DATA			6223
#define	GADGET_PREG_ZERO_IN_BASKET			6224
#define	GADGET_PREG_BASKET_QTY_PROBLEM		6225
#define	GADGET_PREG_REJECTED_OTHER			6226
#define	GADGET_PREG_PERIOD_QTY_USED			6227

#define	NON_ZERO_DOC_PRESC_ID_REQUIRED		6228
#define MEMBER_CARD_MANDATORY_NON_HESDER	6229
#define SELLING_NARCOTIC_WITHOUT_MAG_CARD	6230
#define NARCOTIC_SPLIT_SALE_AT_OTHER_PHARM	6231
#define PRESCRIPTION_ALREADY_FULLY_SOLD		6232
#define TR6005_RX_ALREADY_FULLY_SOLD		6233
#define RX_PARTLY_SOLD_SAME_PHARM_TODAY		6234
#define MEMBER_LOW_MEMBER_TZ_AND_NO_CARD	6235
#define SPECIAL_SUBSTITUTION_WARNING		6236
#define EXPENSIVE_DRUG_SEND_MEMBER_SMS		6237
#define ISHUR_MONTHLY_LIMIT_ERR				6238	// Marianna 11Aug2020 CR #27955
#define ADJ_ISHUR_MONTHLY_LIMIT_ERR			6239	// Marianna 11Aug2020 CR #27955
#define BAD_REQUEST_TYPE_FOR_PHARM			6240	// DonR 09Sep2020 Online Orders.
#define ORDER_NUMBER_MISSING				6241	// DonR 09Sep2020 Online Orders.
#define ERR_FUTURE_SALE_NOT_YET_QUALIFIED	6242	// DonR 15Sep2020 CR #30106.
#define WARN_FUTURE_SALE_NOT_YET_QUALIFIED	6243	// DonR 01Oct2020 CR #30106.
#define NO_SERVICE_WITHOUT_CARD				6244	// DonR 03Nov2020 Online Orders.

#define NO_PHARM_ISHUR_FOR_DARKONAI_PLUS	6245	// DonR 29Aug2021 Darkonaim-Plus.
#define HAND_RX_DARKONAI_PLUS_PAYS_CASH		6246	// DonR 19Sep2021 Darkonaim-Plus.
#define LARGO_CODE_BLOCKED_FOR_MEMBER_MAC	6247	// DonR 21Oct2021 User Story #196891 (For everyone *other* than Darkonaim.)
#define LARGO_CODE_BLOCKED_FOR_MEMBER_PVT	6248	// DonR 21Oct2021 User Story #196891 (Darkonaim, at least for now).
#define OVER_DAILY_OTC_PER_PHARM_LIMIT_WARN	6249	// DonR 28Nov2021 User Story #205423.
#define OVER_DAILY_OTC_PER_PHARM_LIMIT_ERR	6250	// DonR 30Nov2021 User Story #205423.
#define MEMBER_DIED_IN_HOSPITAL				6251	// DonR 30Nov2021 User Story #206812.
#define OVER_DAILY_OTC_OP_PER_PHARM_LIMIT	6252	// DonR 12Dec2021 User Story #205423.
#define DOC_RX_OR_VISIT_NUMBER_NOT_SUPPLIED	6253	// DonR 20Nov2022 User Story #356642.
#define PHARMACY_DID_NOT_SELL_THIS_RX		6254	// DonR 20Nov2022 User Story #356642.

#define THIS_IS_A_CONSIGNMENT_ITEM			6255	// DonR 25Apr2023 User Story #432608.
#define THIS_IS_NOT_A_CONSIGNMENT_ITEM		6256	// DonR 25Apr2023 User Story #432608.
#define NOT_A_CONSIGNMENT_PHARMACY			6257	// DonR 25Apr2023 User Story #432608.
#define NOT_AUTHORIZED_FOR_PHARM_NOHALIM	6258	// DonR 25Apr2023 User Story #432608.
#define GENERIC_SUBSTITUTE_AVAILABLE		6259	// DonR 17May2023 User Story #450542.

#define ERR_MEMBER_PHARM_EXCEPTION			6260	// DonR 08Aug2023 User Story #448931.
#define LARGO_BLOCKED_FOR_DARKONAI_MAC		6261	// DonR 26Jan2023 User Story #196891 (Separate text for Darkonaim.)
#define CANNABIS_NEEDS_DIGITAL_RX			6262	// DonR 25Jan2024 User Story #540234.
#define CANNABIS_NEEDS_SEPARATE_SALE_REQ	6263	// DonR 25Jan2024 User Story #540234.
#define MACCABIPHARM_CANNOT_SELL_CANNABIS	6264	// DonR 25Jan2024 User Story #540234.
#define THIS_PHARMACY_CANNOT_SELL_CANNABIS	6265	// DonR 25Jan2024 User Story #540234.
#define CANNABIS_FORBIDDEN_FOR_FUTURE_RX	6266	// DonR 25Jan2024 User Story #540234.
#define OVER_DAILY_GSL_OP_PER_PHARM_LIMIT	6267	// DonR 04Feb2024 User Story #205423.
#define AUTO_CREDIT_LINE_PMT_AUTHORIZED		6268	// DonR 19May2024 User Story #309734.
#define MEMBER_DOES_NOT_HAVE_CREDIT_LINE	6269	// DonR 19May2024 User Story #309734.

#define USE_CARD_TO_PAY_BY_CREDIT_LINE		6270	// DonR 19May2024 User Story #309734.
#define CARD_OR_SVC_WITHOUT_4_CREDIT_LINE	6271	// DonR 19May2024 User Story #309734.
#define ERR_NARC_HAND_RX_DURATION_DEFAULT	6272	// DonR 19May2024 User Story #429086.
#define ERR_NARC_HAND_RX_DURATION_EXEMPT	6273	// DonR 19May2024 User Story #429086.
#define ERR_NARC_HAND_RX_VALIDITY_DEFAULT	6274	// DonR 19May2024 User Story #429086.
#define ERR_NARC_HAND_RX_VALIDITY_EXEMPT	6275	// DonR 19May2024 User Story #429086.
#define WARN_NARC_HAND_RX_DURATION_DEFAULT	6276	// DonR 19May2024 User Story #429086.
#define WARN_NARC_HAND_RX_DURATION_EXEMPT	6277	// DonR 19May2024 User Story #429086.
#define WARN_NARC_HAND_RX_VALIDITY_DEFAULT	6278	// DonR 19May2024 User Story #429086.
#define WARN_NARC_HAND_RX_VALIDITY_EXEMPT	6279	// DonR 19May2024 User Story #429086.

#define ERR_6011_SALE_OR_VISIT_NOT_FOUND	6280	// DonR 14Dec2025 User Story #417785.
#define ERR_MUST_PASS_1_CARD_TO_BUY			6281	// DonR 05Jan2026 User Story #475507 severity 2
#define WHITE_PRESCRIPTION_PASS_1_CARD		6282	// DonR 05Jan2026 User Story #475507 severity 2
#define HAND_PRESCRIPTION_PASS_1_CARD		6283	// DonR 05Jan2026 User Story #475507 severity 2

#endif /* PHARMACY_ERRORS_H */

/*=======================================================================
||			   End Of PharmacyErrors.h									||
 =======================================================================*/
