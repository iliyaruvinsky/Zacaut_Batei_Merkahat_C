# SqlServer - Code Artifacts

**Component**: SqlServer
**Task ID**: DOC-SQL-001
**Generated**: 2026-02-13
**Source**: `source_code/SqlServer/`

---

## ODBC Operator Model

### Overview

SqlServer defines **265 active SQL operator cases** in `MacODBC_MyOperators.c` (8,418 lines), organized into 6 sections. These operators are injected into the MacODBC infrastructure via an include-injection pattern and are invoked at runtime through the `ExecSQL()` macro.

### Include-Injection Chain

According to research (MacODBC_deepdive.md) and code analysis:

```
Handler function calls ExecSQL(OPERATOR_NAME, ...)
  → ExecSQL macro expands to ODBC_Exec(SINGLETON_SQL_CALL, OPERATOR_NAME, ...)
    → ODBC_Exec looks up OPERATOR_NAME in SQL_GetMainOperationParameters()
      → SQL_GetMainOperationParameters() switch includes MacODBC_MyOperators.c
         (at MacODBC.h:2747)
        → MacODBC_MyOperators.c includes GenSql_ODBC_Operators.c (at line 87)
```

A parallel injection at `MacODBC.h:2866` includes `MacODBC_MyCustomWhereClauses.c` for custom WHERE clause fragments.

### Operator Configuration Variables

According to the documentation block at `MacODBC_MyOperators.c:1-87` (ch_sql_070):

| Variable | Purpose |
|----------|---------|
| `SQL_CommandText` | The SQL statement text (NULL for dynamic SQL) |
| `NumOutputColumns` | Number of output (SELECT) columns |
| `NumInputColumns` | Number of input (parameter) columns |
| `NumWhereColumns` | Number of WHERE clause parameter columns |
| `CommandIsMirrored` | Whether this command participates in MacODBC mirroring |
| `CommandIsSticky` | Whether the prepared statement should be cached |
| `WhereClause` | Custom WHERE clause fragment identifier |

---

## Operator Catalog by Section

### Section 1: General/Shared Operators (ch_sql_071)

**File**: `MacODBC_MyOperators.c:89-4185`
**Active cases**: 118

According to repository.json, these operators are used across multiple transaction handlers:

| Operator Group | Count | Purpose | Key Consumers |
|---------------|------:|---------|---------------|
| CheckHealthAlerts_* | 23 | Health alert rules engine | ch_sql_059 |
| GetDrugsInBlood_* | ~3 | Blood drug retrieval | ch_sql_054 |
| IsPharmOpen_* | ~3 | Pharmacy validation | ch_sql_053 |
| LoadPrescSourceCur / LoadTrnPermitCur | 2 | Cache loaders | ch_sql_009 |
| MarkDrugSaleDeleted_* | ~3 | Sale deletion | ch_sql_061 |
| ReadDrugPurchaseLim_* | ~5 | Purchase limit data | ch_sql_056 |
| TestMacDoctorDrugsElectronic_* | ~5 | Doctor drug pricing | ch_sql_058 |
| TestPharmacyIshur_* | ~4 | Pharmacy ishur validation | ch_sql_057 |
| TestSpecialPresc_* | ~5 | Special prescription | ch_sql_055 |
| UpdateDoctorPresc_* | ~8 | Doctor prescription updates | ch_sql_060 |
| UPD_* (audit/status) | ~10 | Audit and status tracking | ch_sql_005, ch_sql_007 |
| INS_prescriptions | 1 | Prescription header (63 input cols) | ch_sql_022, ch_sql_024 |
| SumMemberDrugsUsed_* | ~2 | Member drug usage totals | ch_sql_059 |
| Other shared operators | ~44 | Various shared operations | Multiple handlers |

**Notable operators**:
- `INS_prescriptions` — 63 input columns, according to repository.json; one of the largest INSERT statements in the system
- `CheckHealthAlerts_HAR_cursor` — 23-column cursor for health alert rule definitions

#### Dynamic SQL Hole #1 (VER-SQL-004)

According to `MacODBC_MyOperators.c:4182`:

`CheckForInteractionIshur_READ_IshurCount` has `SQL_CommandText=NULL` — the SQL is built at runtime by the calling function (`CheckForInteractionIshur` at `MessageFuncs.c:1519-1754`). This is a departure from the normal pattern where SQL text is statically defined in the operator case.

---

### Section 2: TR1xxx Operators (ch_sql_072)

**File**: `MacODBC_MyOperators.c:4188-4685`
**Active cases**: 23

According to repository.json:

| Operator | Transaction | Purpose |
|----------|-------------|---------|
| TR1001_INS_pharmacy_log | 1001 | Pharmacy shift log (43 input cols) |
| TR1001_READ_Pharmacy | 1001 | Pharmacy data read |
| TR1011_2094_DrugListDownload_cur | 1011/2094 | Drug list download (shared cursor) |
| TR1013_INS_pharmacy_log | 1013 | Close shift log |
| TR1014_PriceList_cur | 1014 | Price list download cursor |
| TR1022_INS_Stock | 1022/6022 | Stock entry insert |
| TR1022_INS_StockReport | 1022/6022 | Stock report insert |
| TR1026_message_id_cur | 1026 | Display message cursor |
| TR1028_READ_PharmacyDailySum | 1028 | Financial summary read |
| TR1043_error_messags_cur | 1043 | Error messages download cursor |
| TR1047_member_pric_up_cur | 1047 | Discount rules cursor |
| TR1049_supplier_up_cur | 1049 | Supplier records cursor |
| TR1051_centers_up_cur | 1051 | Centers data cursor |
| TR1053_INS_money_empty | 1053 | Money header insert |
| TR1053_INS_money_emp_lines | 1053 | Money line items insert |
| TR1055_presc_mes_cur | 1055 | DUR/OD prescription messages cursor |
| TR1055_msg_fmt_cur | 1055 | DUR/OD message format cursor |
| TR1080_pharmacy_contract_cur | 1080 | Pharmacy contract download cursor |
| *(5 additional operators)* | Various | Supporting reads/updates |

---

### Section 3: TR2xxx Operators (ch_sql_073)

**File**: `MacODBC_MyOperators.c:4686-6002`
**Active cases**: 50

According to repository.json:

| Operator Group | Count | Purpose |
|---------------|------:|---------|
| TR2001_Rx_cursor | 1 | Doctor prescription join for Rx list |
| TR2003_INS_prescription_drugs/prescriptions | ~4 | E-Rx sale inserts |
| TR2003_5003_READ_doctor_presc | 1 | Shared doctor presc read (TR2003 + TR5003) |
| TR2005_* (realtime) | ~6 | E-Rx delivery cursors and updates |
| TR2005spool_* | ~6 | E-Rx delivery spool variants |
| TR2033_* | ~4 | Special recipe (ishur) operators |
| TR2060-2068_* | 5 | Reference table download cursors |
| TR2070-2084_* | ~12 | Data update/download cursors |
| TR2086-2101_* | ~10 | Reconciliation/reporting operators |

#### Dynamic SQL Hole #2 (VER-SQL-005)

According to `MacODBC_MyOperators.c:5820`:

`TR2088_prescription_drugs_cur` has `SQL_CommandText=NULL` — the SQL is generated dynamically for the pharmacy drug sales detail report. The calling function (`HandlerToMsg_2088` at `ElectronicPr.c:21466-23519`) constructs the SQL at runtime based on transaction parameters.

---

### Section 4: TR5xxx Operators (ch_sql_074)

**File**: `MacODBC_MyOperators.c:6004-6566`
**Active cases**: 19

According to repository.json:

| Operator Group | Count | Purpose |
|---------------|------:|---------|
| TR5003_deldrugs_cur | 1 | Drug deletion cursor for subsidy sale |
| TR5003_INS_prescription_drugs | 1 | Subsidy sale insert (**69 input columns — largest INSERT**) |
| TR5003_READ_prescription_drugs_rows_to_delete | 1 | Pre-deletion read |
| TR5005_* (realtime) | ~4 | Subsidy delivery cursors and updates |
| TR5005spool_* | ~4 | Subsidy delivery spool variants |
| TR5051-5061_* | 6 | Nihul Tikrot reference table download cursors |
| *(2 additional operators)* | 2 | Supporting operations |

**Notable**: `TR5003_INS_prescription_drugs` with 69 input columns is the largest INSERT statement in the entire operator file.

---

### Section 5: TR6xxx Operators (ch_sql_075)

**File**: `MacODBC_MyOperators.c:6568-8418`
**Active cases**: 55 (52 TR6xxx + 3 tail utilities)

According to repository.json:

| Operator Group | Count | Purpose |
|---------------|------:|---------|
| TR6001_doctor_presc_cur | 1 | Doctor presc cursor (18+ output cols) |
| TR6001_doctor_presc_cur_simplified | 1 | Alternate simplified cursor |
| TR6001_validation_* | ~8 | Ishur, discount, prior purchase checks |
| TR6003_INS_prescription_drugs | 1 | Digital sale insert |
| TR6005_UPD_prescription_drugs | 1 | Digital delivery update (**35+ SET columns — largest UPDATE**) |
| TR6005spool_* | ~6 | Digital delivery spool variants |
| TR6011_* | ~6 | Member drug history operators |
| TR6011_PriorSaleDownload | 1 | **163-line multi-JOIN — largest single operator** |
| TR6102_* | ~2 | Chanut Virtualit count |
| TR6103_* | ~4 | Chanut Virtualit pricing |
| Tail utilities | 3 | READ_Rx_DoctorRuleNumber, READ_service_url_components, READ_service_http_headers |

**Notable operators**:
- `TR6005_UPD_prescription_drugs` — 35+ SET columns, the largest UPDATE in the system
- `TR6011_PriorSaleDownload` — 163-line multi-JOIN query, the largest single operator definition

---

### Section 6: Custom WHERE Clause Fragments (ch_sql_086)

**File**: `MacODBC_MyCustomWhereClauses.c:1-274`

According to repository.json, these fragments are injected into `MacODBC.h:SQL_GetWhereClauseParameters` at line 2866:

| WHERE Clause ID | Purpose |
|----------------|---------|
| CheckForServiceWithoutCard | Member sales count condition |
| ChooseNewlyUpdatedRows | Incremental sync (rows modified since last download) |
| Find_diagnosis_from_member_diagnoses | Diagnosis lookup |
| Find_diagnosis_from_special_prescs | Special prescription diagnosis |
| Largo_99997_ishur_standard | Standard ishur check |
| READ_PriceList_800_conditional | Conditional pricing WHERE clause |
| TestPharmacyIshur | Pharmacy authorization conditions |
| TestSpecialPresc | Special prescription conditions |
| TR1014 | Price list WHERE |
| TR2001 | Prescription drugs WHERE |
| TR2003_5003 | Sale WHERE (shared between TR2003 and TR5003) |
| TR2090 | Sale detail WHERE |

---

## Operator Statistics

### By Section

| Section | File Lines | Cases | Avg Lines/Case |
|---------|-----------|------:|:--------------:|
| General/Shared | 89-4185 | 118 | ~35 |
| TR1xxx | 4188-4685 | 23 | ~22 |
| TR2xxx | 4686-6002 | 50 | ~26 |
| TR5xxx | 6004-6566 | 19 | ~30 |
| TR6xxx | 6568-8418 | 55 | ~34 |
| **Total** | **8,418** | **265** | **~32** |

### Largest Operators

| Operator | Type | Metric | Location |
|----------|------|--------|----------|
| TR6011_PriorSaleDownload | SELECT (cursor) | 163-line JOIN | ch_sql_075 |
| TR5003_INS_prescription_drugs | INSERT | 69 input columns | ch_sql_074 |
| INS_prescriptions | INSERT | 63 input columns | ch_sql_071 |
| TR1001_INS_pharmacy_log | INSERT | 43 input columns | ch_sql_072 |
| TR6005_UPD_prescription_drugs | UPDATE | 35+ SET columns | ch_sql_075 |
| TR6001_doctor_presc_cur | SELECT (cursor) | 18+ output columns | ch_sql_075 |
| TestSpecialPresc_READ_special_prescs | SELECT | 15 output columns | ch_sql_071 |
| CheckHealthAlerts_HAR_cursor | SELECT (cursor) | 23 columns | ch_sql_071 |

### Dynamic SQL Holes

| ID | Operator | File:Line | Calling Function | Description |
|----|----------|-----------|-----------------|-------------|
| VER-SQL-004 | CheckForInteractionIshur_READ_IshurCount | MacODBC_MyOperators.c:4182 | CheckForInteractionIshur (MessageFuncs.c:1519-1754) | SQL_CommandText=NULL; SQL built at runtime for drug-drug interaction ishur check |
| VER-SQL-005 | TR2088_prescription_drugs_cur | MacODBC_MyOperators.c:5820 | HandlerToMsg_2088 (ElectronicPr.c:21466-23519) | SQL_CommandText=NULL; SQL generated dynamically for pharmacy sales detail report |

In both cases, the NULL `SQL_CommandText` signals to `ODBC_Exec` that the calling function has already prepared the SQL statement text directly, bypassing the normal operator-defined static SQL. This pattern appears to be used when the SQL structure depends on runtime parameters that cannot be expressed through the standard WHERE clause mechanism.

---

## Operator-to-Table Mapping

Based on operator name analysis, the following database tables appear to be accessed:

### Core Transaction Tables

| Table (inferred) | Read Operators | Write Operators |
|------------------|:-------------:|:--------------:|
| prescriptions | TR2003_5003_*, TR2005_*, TR5005_*, TR6005_* | INS_prescriptions, TR*_UPD_prescriptions |
| prescription_drugs | TR2001_*, TR2005_*, TR5005_*, TR6005_*, TR6011_* | TR*_INS_prescription_drugs, TR*_UPD_prescription_drugs |
| doctor_presc | TR2003_5003_READ_doctor_presc, TR6001_doctor_presc_cur | UpdateDoctorPresc_UPD_Rx_* |
| pharmacy | IsPharmOpen_READ_pharmacy | - |
| pharmacy_daily_sum | TR1028_READ_*, TR2086_* | TR2005_UPD_*, TR5005_UPD_* |
| pharmacy_log | - | TR1001_INS_pharmacy_log, TR1013_INS_pharmacy_log |
| members | READ_members_full | - |
| drug_list | read_drug_READ_drug_list | - |

### Support/Reference Tables

| Table (inferred) | Operator Pattern | Access Type |
|------------------|-----------------|-------------|
| drug_interaction | IsDrugInteract_READ_* | Read |
| blood_drugs | GetDrugsInBlood_* | Read |
| special_prescs | TestSpecialPresc_* | Read |
| pharmacy_ishur | TestPharmacyIshur_*, TR2033_INS_* | Read/Write |
| purchase_limits | ReadDrugPurchaseLim_* | Read |
| health_alert_rules | CheckHealthAlerts_* | Read |
| sysparams | READ_sysparams | Read |
| prescr_source | LoadPrescSourceCur | Read |
| pharm_trn_permit | LoadTrnPermitCur | Read |
| sql_srv_audit | INS_sql_srv_audit, UPD_sql_srv_audit_* | Read/Write |
| messages_details | INS_messages_details, UPD_messages_details | Read/Write |
| rx_late_arrival_q | ProcessRxLateArrivalQ_* | Read/Write |
| presc_drug_inter | IntOD_INS_presc_drug_inter | Write |

### Reference Download Tables

| Table (inferred) | Transaction | Operator |
|------------------|-------------|----------|
| how_to_take | TR2060 | TR2060_how_to_take_cur |
| drug_shape | TR2062 | TR2062_drug_shape_cur |
| unit_of_measure | TR2064 | TR2064_unit_of_measure_cur |
| reason_not_sold | TR2066 | TR2066_reason_not_sold_cur |
| discount_remarks | TR2068 | TR2068_discount_remarks_cur |
| error_messages | TR1043 | TR1043_error_messags_cur |
| member_price | TR1047 | TR1047_member_pric_up_cur |
| supplier | TR1049 | TR1049_supplier_up_cur |
| centers | TR1051 | TR1051_centers_up_cur |
| credit_reasons | TR5051 | TR5051_credit_reasons_cur |
| subsidy_messages | TR5053 | TR5053_subsidy_messages_cur |
| tikra_type | TR5055 | TR5055_tikra_type_cur |
| hmo_membership | TR5057 | TR5057_hmo_membership_cur |
| virtual_store_reason_texts | TR5059 | TR5059_virtual_store_reason_texts_cur |
| druggencomponents | TR5061 | TR5061_DrugGenComponents_cur |

Note: Table names are inferred from operator naming conventions and may not exactly match actual database table names.

---

## Macro and Preprocessor Artifacts

### Cross-File Macros

According to code analysis across multiple handler files:

| Macro | Defined In | Purpose |
|-------|-----------|---------|
| `Conflict_Test(r)` | SqlServer.c:40-46, SqlHandlers.c, ElectronicPr.c | SQL access conflict retry |
| `SET_POS()` / `GET_POS()` | MessageFuncs.c | Message buffer position tracking |
| `SetErrorVarInit` | MessageFuncs.h | Initialize error tracking |
| `SetErrorVar` | MessageFuncs.h | Set single error code |
| `SetErrorVarArr` | MessageFuncs.h | Add to error array |
| `SetErrorVarDelete` | MessageFuncs.h | Remove error from array |
| `SetErrorVarTell` | MessageFuncs.h | Query current error state |
| `LOG_CONFLICT` | MessageFuncs.c | Log access conflict events |

### Pharmacy-Type Macros (ElectronicPr.c, DigitalRx.c)

According to code at `ElectronicPr.c:1-222` and `DigitalRx.c:1-262`:

| Macro | Purpose |
|-------|---------|
| `MACCABI_PHARMACY` | Tests if pharmacy is Maccabi type |
| `FULL_PRICE_SALE` | Tests if sale is at full price |
| `MEMBER_GETS_DISCOUNTS` | Tests member discount eligibility |
| `ASSUTA_PHARMACY` | Tests if pharmacy is Assuta type |

### DUR/OD Test Macros (SqlHandlers.c)

According to code at `SqlHandlers.c:1-394`:

Various macros for accessing the DUR and overdose decision tables, simplifying the multi-dimensional array lookups in `DurDecisionTable[2][2][2][6]` and `OD_DecisionTable[2][4][5]`.

---

## Dead Code

According to repository.json anti-hallucination notes:

| Location | Code | Status |
|----------|------|--------|
| ElectronicPr.c:25687-25831 | `FindDrugPurchLimit` | `#if 0` since 16 November 2025 |
| MessageFuncs.c:11394-11555 | `CheckForServiceWithoutCard_Old` | `#if 0` — replaced by REST version |

---

## GenSql Shared Operators

According to `MacODBC_MyOperators.c:87`:

```c
#include "GenSql_ODBC_Operators.c"
```

This include seam injects shared GenSql operator definitions before component-specific operators. These shared operators are defined in the GenSql component and are available to all sibling components that use the MacODBC infrastructure.

According to the cross-reference table in DOCUMENTER_INSTRUCTIONS.md, the following sibling components also maintain their own `MacODBC_MyOperators.c`:

| Component | Has MyOperators.c | Has CustomWhereClauses.c |
|-----------|:-----------------:|:------------------------:|
| FatherProcess | Yes | Yes |
| SqlServer | Yes | Yes |
| ShrinkPharm | Yes | Yes |
| As400UnixServer | Yes | Yes |
| As400UnixClient | Yes | Yes |
| As400UnixDocServer | Yes | Yes |
| As400UnixDoc2Server | Yes | Yes |
| GenSql | Yes | N/A |

---

*Generated by CIDRA Documenter Agent - DOC-SQL-001*
