# SqlServer - Business Logic

**Component**: SqlServer
**Task ID**: DOC-SQL-001
**Generated**: 2026-02-13
**Source**: `source_code/SqlServer/`

---

## Table of Contents

1. [Core Sale Handlers — The "Big Three"](#core-sale-handlers)
   - [TR2003: Electronic Prescription Drug Sale](#tr2003-electronic-prescription-drug-sale)
   - [TR5003: Nihul Tikrot Subsidy Drug Sale](#tr5003-nihul-tikrot-subsidy-drug-sale)
   - [TR6003: Digital Prescription Drug Sale](#tr6003-digital-prescription-drug-sale)
   - [Big Three Comparison Matrix](#big-three-comparison-matrix)
2. [Delivery Handlers](#delivery-handlers)
   - [TR2005: Electronic Prescription Delivery](#tr2005-electronic-prescription-delivery)
   - [TR5005: Nihul Tikrot Subsidy Delivery](#tr5005-nihul-tikrot-subsidy-delivery)
   - [TR6005: Digital Prescription Delivery](#tr6005-digital-prescription-delivery)
3. [Request and History Handlers](#request-and-history-handlers)
   - [TR2001: Prescription Drugs List Request](#tr2001-prescription-drugs-list-request)
   - [TR6001/6101: Digital Prescription Request](#tr60016101-digital-prescription-request)
   - [TR6011: Member Drug History Download](#tr6011-member-drug-history-download)
   - [TR6102/6103: Chanut Virtualit](#tr61026103-chanut-virtualit)
4. [1xxx Pharmacy Management Handlers](#1xxx-pharmacy-management-handlers)
   - [TR1001: Open Shift](#tr1001-open-shift)
   - [TR1011/1013/1014: Drug List, Close Shift, Price List](#tr10111013-drug-list-close-shift-price-list)
   - [TR1022/6022: Stock Entry](#tr10226022-stock-entry)
   - [TR1026: Display Messages](#tr1026-display-messages)
   - [TR1028: Financial Status](#tr1028-financial-status)
   - [Table Download Handlers (1043-1080)](#table-download-handlers-1043-1080)
   - [TR1053: Money/Employee Items](#tr1053-moneyemployee-items)
   - [TR1055: DUR/OD Message Retrieval](#tr1055-durod-message-retrieval)
5. [Specialized Handlers](#specialized-handlers)
   - [TR2033/6033: Special Recipe (Ishur)](#tr20336033-special-recipe-ishur)
   - [Reference Table Downloads (2060-2068, 5051-5061)](#reference-table-downloads)
   - [Data Update Handlers (2070-2084)](#data-update-handlers-2070-2084)
   - [Reconciliation and Reporting (2086-2101)](#reconciliation-and-reporting-2086-2101)
6. [AS/400 Integration (Cross-Cutting Concern)](#as400-integration)
7. [Utility Functions (ElectronicPr.c)](#utility-functions)

---

## Core Sale Handlers

The three core sale handlers — collectively referred to as the "Big Three" — implement the primary drug sale business logic and together account for approximately 17,940 lines (21.4% of the codebase). Based on code analysis, they share a common validation pipeline (documented in 03_TECHNICAL_ANALYSIS.md) but each adds transaction-specific processing.

---

### TR2003: Electronic Prescription Drug Sale

**Handler**: `HandlerToMsg_2003`
**File**: `ElectronicPr.c:1057-5122` (4,066 lines)
**Chunk**: ch_sql_022

According to repository.json and code analysis, this handler processes the actual sale of electronically prescribed drugs. It appears to be the original core sale handler, with TR5003 and TR6003 building upon its pattern.

#### Phase 1: Input Parsing and Validation (~lines 1057-2000)

According to code analysis:

- **Input extraction**: Parses binary message fields from pharmacy terminal — pharmacy ID, member ID, doctor ID, prescription details, drug codes, quantities
- **Pharmacy validation**: Calls `IS_PHARMACY_OPEN_X()` (MessageFuncs.c:1057-1414) to verify the pharmacy is open and authorized
- **Member validation**: Calls `read_member()` (ElectronicPr.c:25838-25888) to load member data into global `Member` structure
- **Prescription lookup**: Reads doctor prescription records via `ExecSQL(TR2003_5003_READ_doctor_presc)` — a shared operator between TR2003 and TR5003
- **Special prescription testing**: Calls `test_special_prescription()` (MessageFuncs.c:2201-3543) for controlled substance validation
- **Error initialization**: Calls `SetErrorVarInit` macro to prepare error tracking

#### Phase 2: Drug Processing Loop — Pricing and Participation (~lines 2000-3500)

According to code analysis, the drug-by-drug processing loop appears to perform the following for each drug in the prescription:

- **Drug data loading**: `read_drug()` (ElectronicPr.c:25897-26158) reads drug information from DrugList
- **Participation determination**: `test_mac_doctor_drugs_electronic()` (MessageFuncs.c:6248-6873) calculates participation percentages for Maccabi-doctor drugs, handling specialist letters, old-age home rules, purchase history, and preferred-drug swap
- **Pharmacy ishur testing**: `test_pharmacy_ishur()` (MessageFuncs.c:5297-5822) validates pharmacy-level authorization
- **Special drug rules**: `test_special_drugs_electronic()` (MessageFuncs.c:6101-6223) checks dentist/home-visit rules
- **Participating code lookup**: `GetParticipatingCode()` (MessageFuncs.c:7566-7744) finds the lowest participation code
- **AS/400 eligibility**: `as400EligibilityTest()` (As400UnixMediator.c:106-278) checks member eligibility via TCP to Meishar
- **Bakara kamutit**: Quantity checking logic for purchase limit validation
- **DUR/OD testing**: `test_interaction_and_overdose()` (SqlHandlers.c:7444-8267) checks drug-drug interactions and overdose risk
- **Purchase limits**: `test_purchase_limits()` (MessageFuncs.c:4553-5283) tests against 9 limit methods
- **Health alerts**: `CheckHealthAlerts()` (MessageFuncs.c:8094-9960) runs in-memory rules engine
- **Discount logic**: Pricing calculations including member discount, pharmacy discount, and discount code handling

#### Phase 3: Database Writes and Output (~lines 3500-5122)

According to code analysis:

- **Prescription insertion**: `ExecSQL(TR2003_INS_prescriptions)` writes the prescription header record
- **Drug line insertion**: `ExecSQL(TR2003_INS_prescription_drugs)` writes individual drug sale lines
- **Doctor prescription updates**: `update_doctor_presc()` (MessageFuncs.c:9969-11142) updates doctor prescription tables with carry-forward tracking
- **Error/warning assignment**: Final error severity determination via `SetErrorVar`/`SetErrorVarArr`
- **Output formatting**: Formats the response buffer with sale result, pricing details, warnings, and error codes for the pharmacy terminal

**Key operators used** (according to MacODBC_MyOperators.c):
- `TR2003_INS_prescriptions` (63 input columns — one of the largest INSERTs in the system)
- `TR2003_INS_prescription_drugs`
- `TR2003_5003_READ_doctor_presc` (shared with TR5003)

---

### TR5003: Nihul Tikrot Subsidy Drug Sale

**Handler**: `HandlerToMsg_5003`
**File**: `ElectronicPr.c:7653-13598` (5,946 lines)
**Chunk**: ch_sql_024

According to repository.json, this is the **largest function in the entire codebase**. It handles the Nihul Tikrot (subsidy management) drug sale, mirroring TR2003 logic but adding subsidy-specific calculations.

#### Phase 1: Input Parsing and Member/Pharmacy Validation (~lines 7653-9000)

According to code analysis:

- **Input extraction**: Parses the same base fields as TR2003, plus subsidy-specific fields (credit reason, tikra type, HMO membership)
- **Pharmacy validation**: `IS_PHARMACY_OPEN_X()` — identical to TR2003
- **Member validation**: `read_member()` — identical to TR2003
- **Subsidy eligibility**: Additional validation for Nihul Tikrot eligibility including HMO membership status and tikra type validation
- **Special prescription testing**: `test_special_prescription()` — identical to TR2003
- **Credit reason handling**: Validates and processes credit reason codes specific to the subsidy system

#### Phase 2: Drug Processing — Pricing, Subsidy Calculation, Meishar (~lines 9000-11000)

According to code analysis:

- **Base drug processing**: Same as TR2003 — `read_drug()`, participation determination, pharmacy ishur, special drug rules
- **Subsidy-specific pricing**: Calculates subsidy amounts based on credit reason, tikra type, and member HMO membership status
- **AS/400 eligibility**: `as400EligibilityTest()` — same Meishar integration as TR2003
- **TikrotRPC integration**: `CallTikrotSP()` (TikrotRPC.c:304-444) calls the AS/400 DB2 stored procedure `RKPGMPRD.TIKROT` with subsidy parameters — this is the key differentiator from TR2003
- **Subsidy calculation**: Determines the subsidy percentage and resulting member payment based on TikrotRPC return values

#### Phase 3: Drug Processing — DUR/OD, Purchase Limits, Health Alerts (~lines 11000-12500)

According to code analysis:

- **DUR/OD testing**: `test_interaction_and_overdose()` — identical to TR2003
- **Purchase limits**: `test_purchase_limits()` — identical to TR2003
- **Health alerts**: `CheckHealthAlerts()` — identical to TR2003
- **Doctor-drug pricing**: `test_mac_doctor_drugs_electronic()` — identical to TR2003
- **Error/warning escalation**: Same error severity model as TR2003 via `SetErrorVar_x`

#### Phase 4: Database Writes and Output Formatting (~lines 12500-13598)

According to code analysis:

- **Prescription insertion**: Uses `TR5003_INS_prescription_drugs` (69 input columns — the **largest INSERT in the entire operator file**, according to MacODBC_MyOperators.c:6004-6566)
- **Doctor prescription updates**: `update_doctor_presc()` — identical to TR2003
- **Subsidy-specific output**: Formats response with subsidy details including credit amount, tikra type, HMO discount applied
- **AS/400 gadget spool**: Queues sale record for AS/400 archival

**Key differentiators from TR2003**:
- TikrotRPC integration (DB2 stored procedure call)
- Subsidy eligibility validation
- Credit reason / tikra type handling
- HMO membership verification
- 69-column INSERT (vs TR2003's standard INSERT)

---

### TR6003: Digital Prescription Drug Sale

**Handler**: `HandlerToMsg_6003`
**File**: `DigitalRx.c:5352-13280` (7,928 lines)
**Chunk**: ch_sql_042

According to repository.json, this is the **second-largest function in the codebase** and represents the digital-era equivalent of HandlerToMsg_2003. It appears to be referred to internally as "Mersham Digitali" (Digital Prescription).

#### Phase 1: Input Parsing and Member/Pharmacy Validation (~lines 5352-6500)

According to code analysis:

- **Input extraction**: Parses binary message fields with digital-prescription-specific additions
- **Pharmacy validation**: `IS_PHARMACY_OPEN_X()` — same as TR2003/5003
- **Member validation**: `read_member()` — same as TR2003/5003
- **Digital prescription specifics**: Additional validation for digital prescription source, external Rx supplier checking, coupon expiry validation
- **Special prescription testing**: `test_special_prescription()` — same as TR2003/5003

#### Phase 2: Drug-by-Drug Processing — Pricing and Participation (~lines 6500-9000)

According to code analysis:

- **Drug data loading**: `read_drug()` — same as TR2003/5003
- **Participation determination**: `test_mac_doctor_drugs_electronic()` — same as TR2003
- **Pharmacy ishur testing**: `test_pharmacy_ishur()` — same as TR2003
- **Participating code lookup**: `GetParticipatingCode()` — same as TR2003
- **AS/400 eligibility**: `as400EligibilityTest()` — same Meishar integration
- **Digital-specific pricing**: Additional pricing logic for digital prescription sources and external Rx suppliers
- **Coupon expiry**: Validates prescription coupon dates specific to the digital system

#### Phase 3: Drug-by-Drug Processing — Discounts, DUR/OD, Purchase Limits (~lines 9000-11000)

According to code analysis:

- **Discount logic**: Extended discount calculations compared to TR2003
- **DUR/OD testing**: `test_interaction_and_overdose()` — same as TR2003/5003
- **Purchase limits**: `test_purchase_limits()` — same as TR2003/5003
- **Health alerts**: `CheckHealthAlerts()` — same as TR2003/5003
- **Bakara kamutit**: Quantity checking — same as TR2003
- **External Rx supplier**: Additional validation for prescriptions originating from external digital Rx suppliers

#### Phase 4: Database Writes and Output Formatting (~lines 11000-13280)

According to code analysis:

- **Drug insertion**: `ExecSQL(TR6003_INS_prescription_drugs)` — digital-prescription-specific INSERT
- **Doctor prescription updates**: `update_doctor_presc()` — same as TR2003/5003
- **Output formatting**: Extended output including digital-prescription-specific fields
- **AS/400 gadget spool**: Same archival path as TR2003/5003

**Key differentiators from TR2003**:
- Digital prescription source validation
- External Rx supplier checking
- Coupon expiry validation
- Extended discount calculations
- Digital-specific INSERT operator

---

### Big Three Comparison Matrix

| Feature | TR2003 | TR5003 | TR6003 |
|---------|:------:|:------:|:------:|
| **Lines** | 4,066 | 5,946 | 7,928 |
| **File** | ElectronicPr.c | ElectronicPr.c | DigitalRx.c |
| IS_PHARMACY_OPEN_X | Yes | Yes | Yes |
| test_special_prescription | Yes | Yes | Yes |
| test_purchase_limits | Yes | Yes | Yes |
| test_interaction_and_overdose | Yes | Yes | Yes |
| test_pharmacy_ishur | Yes | Yes | Yes |
| test_mac_doctor_drugs_electronic | Yes | Yes | Yes |
| CheckHealthAlerts | Yes | Yes | Yes |
| update_doctor_presc | Yes | Yes | Yes |
| as400EligibilityTest (Meishar) | Yes | Yes | Yes |
| CallTikrotSP (DB2 RPC) | No | **Yes** | No |
| Subsidy calculations | No | **Yes** | No |
| Digital Rx source validation | No | No | **Yes** |
| External Rx supplier | No | No | **Yes** |
| Coupon expiry validation | No | No | **Yes** |
| JSON support | No | No | No |
| INS_prescription_drugs columns | Standard | **69 (largest)** | Digital-specific |

---

## Delivery Handlers

Each sale transaction (2003, 5003, 6003) has a corresponding delivery transaction that finalizes the sale. Delivery handlers appear to come in realtime/spool pairs, where the spool variant adds duplicate detection and recovery logic.

---

### TR2005: Electronic Prescription Delivery

**File**: `ElectronicPr.c:5135-7640`
**Chunk**: ch_sql_023

According to repository.json:

**HandlerToMsg_2005** (`ElectronicPr.c:5135-6370`, 1,236 lines) — Realtime delivery:
- Reads prescription records to verify a preceding TR2003 sale
- Iterates prescription drugs via `DeclareCursor(TR2005_presc_drug_cur)`
- Updates `prescription_drugs` records with delivery status via `ExecSQL(TR2005_UPD_prescription_drugs)`
- Updates `prescriptions` header via `ExecSQL(TR2005_UPD_prescriptions)`
- Updates `pharmacy_daily_sum` totals via `ExecSQL(TR2005_UPD_pharmacy_daily_sum)`
- Queues AS/400 gadget spool for archival via `as400SaveSaleRecord()` (As400UnixMediator.c)

**HandlerToSpool_2005** (`ElectronicPr.c:6383-7640`, 1,258 lines) — Spooled delivery:
- Same business logic as realtime variant
- Adds `commfail_flag` parameter for communication failure handling
- Implements duplicate-spool detection to prevent double delivery
- Additional recovery logic for deferred/batch processing

**Key operators**: `TR2005_presc_drug_cur`, `TR2005_UPD_prescription_drugs`, `TR2005_UPD_prescriptions`, `TR2005_UPD_pharmacy_daily_sum`, plus `TR2005spool_*` variants

---

### TR5005: Nihul Tikrot Subsidy Delivery

**File**: `ElectronicPr.c:13611-17239`
**Chunk**: ch_sql_025

According to repository.json:

**HandlerToMsg_5005** (`ElectronicPr.c:13611-15179`, 1,569 lines) — Realtime delivery:
- Mirrors TR2005 delivery logic but with subsidy-specific fields
- Reads and updates subsidy-related prescription drug records
- Handles subsidy-specific pharmacy daily sum updates
- Queues AS/400 gadget spool via `as400SaveSaleRecord()`

**HandlerToSpool_5005** (`ElectronicPr.c:15192-17239`, 2,048 lines) — Spooled delivery:
- Same as realtime variant with `commfail_flag` and recovery logic
- Duplicate-spool detection for deferred processing
- The spool variant is 479 lines longer than the realtime variant, reflecting the additional recovery and validation paths

**Key operators**: `TR5005_presc_drug_cur`, `TR5005_UPD_prescription_drugs`, `TR5005_UPD_prescriptions`, plus `TR5005spool_*` variants

---

### TR6005: Digital Prescription Delivery

**Files**: `DigitalRx.c:13293-19726`
**Chunks**: ch_sql_043 (realtime), ch_sql_044 (spool)

According to repository.json, this is the most complex delivery handler pair:

**HandlerToMsg_6005** (`DigitalRx.c:13293-16308`, 3,015 lines) — Realtime delivery:
- Reads prescriptions and prescription drugs via cursors
- Updates prescription_drugs records — `ExecSQL(TR6005_UPD_prescription_drugs)` has **35+ SET columns**, according to repository.json, making it the **largest UPDATE statement** in the operator file
- Handles prescription vouchers via `ExecSQL(TR6005_INS_prescription_vouchers)`
- Cannabis product details handling (digital-specific)
- Online order validation
- Pharmacy daily sum updates
- AS/400 gadget spool via `as400SaveSaleRecord()`
- Doctor prescription updates via `update_doctor_presc()`

**HandlerToSpool_6005** (`DigitalRx.c:16321-19726`, 3,405 lines) — Spooled delivery:
- Same business logic with `commfail_flag` parameter
- Duplicate-spool detection
- Full INSERT/UPDATE paths: `TR6005spool_INS_prescription_drugs`, `TR6005spool_UPD_prescription_drugs`
- Spool-specific validation and recovery
- Doctor prescription updates via `update_doctor_presc()`

**Delivery Handler Size Comparison**:

| Transaction | Realtime | Spool | Total |
|-------------|----------|-------|-------|
| TR2005 (E-Rx) | 1,236 | 1,258 | 2,494 |
| TR5005 (Subsidy) | 1,569 | 2,048 | 3,617 |
| TR6005 (Digital) | 3,015 | 3,405 | 6,420 |

---

## Request and History Handlers

---

### TR2001: Prescription Drugs List Request

**Handler**: `HandlerToMsg_2001`
**File**: `ElectronicPr.c:223-1045` (823 lines)
**Chunk**: ch_sql_021

According to repository.json:

- **Purpose**: Retrieves valid electronic prescriptions for a member before a sale
- **Workflow**:
  1. Calls `IS_PHARMACY_OPEN_X()` to validate pharmacy
  2. Calls `read_member()` to load member data
  3. Opens `DeclareCursor(TR2001_Rx_cursor)` on doctor_presc table (join query)
  4. Iterates prescriptions, for each drug:
     - Calls `test_special_prescription()` for controlled substance validation
     - Calls `FIND_DRUG_EXTENSION()` (MessageFuncs.c:7475-7555) for drug extension lookup
     - Checks participation rates
  5. Returns formatted drug list to pharmacy terminal
- **Called from**: Central dispatch at `SqlServer.c:1286-1687`
- **Operators**: `TR2001_Rx_cursor` (doctor_presc join)

---

### TR6001/6101: Digital Prescription Request

**Handler**: `HandlerToMsg_6001_6101`
**File**: `DigitalRx.c:273-5340` (5,067 lines)
**Chunk**: ch_sql_041

According to repository.json, this is the fourth-largest function in the codebase, supporting both TR6001 (standard digital) and TR6101 (Chanut Virtualit variant).

#### Phase 1: Input Parsing and Member Validation (~lines 273-1500)

According to code analysis:

- **JSON/Binary dual mode**: Supports both JSON and legacy binary message formats via cJSON
- **Pharmacy validation**: `IS_PHARMACY_OPEN_X()` — same as sale handlers
- **Member validation**: `read_member()` + `SetMemberFlags()` (MessageFuncs.c:12713-12830) for deriving member flags from database records
- **Doctor visits validation**: Checks doctor visit records

#### Phase 2: Doctor Prescription Cursor Traversal and Drug Grouping (~lines 1500-3000)

According to code analysis:

- **Cursor traversal**: Opens `DeclareCursor(TR6001_doctor_presc_cur)` (18+ output columns, according to repository.json) on doctor prescription records
- **Drug extension lookups**: `FIND_DRUG_EXTENSION()` for each drug
- **Drug grouping**: Groups drugs by prescription for presentation
- **Alternate cursor**: `TR6001_doctor_presc_cur_simplified` used in some code paths

#### Phase 3: Special Prescription Testing and Participation Prediction (~lines 3000-4500)

According to code analysis:

- **Special prescription**: `test_special_prescription()` for controlled substance validation
- **Participation prediction**: `predict_member_participation()` (MessageFuncs.c:6889-7282) predicts member co-pay for prescription-request transactions — this is distinct from `test_mac_doctor_drugs_electronic()` used by sale handlers, as it provides cost estimates rather than actual pricing
- **Preferred drug substitution**: `find_preferred_drug()` (MessageFuncs.c:12238-12526) looks for generic substitutions with purchase history checking
- **Ishur availability**: Checks authorization (ishur) availability for each drug

#### Phase 4: Output Formatting and Online Orders (~lines 4500-5340)

According to code analysis:

- **Output formatting**: Formats prescription list with pricing estimates for pharmacy terminal display
- **Online order handling**: Processes and returns online order data
- **TR6101 variant**: The Chanut Virtualit variant appears to follow the same logic with different output formatting

**Key operators**: `TR6001_doctor_presc_cur` (18+ output columns), `TR6001_doctor_presc_cur_simplified`, TR6001 validation operators (ishur, discount, prior purchase checks)

---

### TR6011: Member Drug History Download

**Handler**: `HandlerToMsg_6011`
**File**: `DigitalRx.c:19747-21028` (1,281 lines)
**Chunk**: ch_sql_045

According to repository.json:

- **Added**: July 2025 (User Story #417785), according to code at `DigitalRx.c:19747`
- **Purpose**: Downloads member drug purchase history and ishurim (authorizations)
- **Six download modes** (based on code analysis):
  1. All sales
  2. All Rx sales
  3. Treatment sales
  4. Opioid sales
  5. ADHD sales
  6. Combined opioid + ADHD sales
- **JSON-capable**: Supports JSON output format
- **Workflow**:
  1. Doctor visits validation via `ExecSQL(TR6011_AreDoctorVisitsAlive)`
  2. Receipt number checking
  3. Prior sale download via `DeclareCursor(TR6011_PriorSaleDownload)` — according to repository.json, this is a 163-line multi-JOIN query, making it the **largest single operator** in the system
  4. Ishur download via `DeclareCursor(TR6011_IshurDownload)`
  5. Audit trail via `ExecSQL(TR6011_WriteAudit)`

---

### TR6102/6103: Chanut Virtualit

**File**: `DigitalRx.c:21041-22953`
**Chunk**: ch_sql_046

According to repository.json:

**HandlerToMsg_6102** (`DigitalRx.c:21041-21323`, 282 lines):
- **Purpose**: Counts valid prescriptions for a list of members (typically a family)
- **JSON-only**: The smallest handler, uses JSON exclusively
- **Operators**: `TR6102_READ_PrescriptionCount`

**HandlerToMsg_6103** (`DigitalRx.c:21335-22913`, 1,578 lines):
- **Purpose**: Predicted participation calculation for a group of drugs
- **Features**: Provides price estimates before actual sale, including Meishar and Nihul Tikrot reductions
- **JSON-only**: Uses JSON exclusively for the Chanut Virtualit application
- **Operators**: `TR6103_READ_gadgets`, `TR6103_READ_special_prescs`

**rx_source_found** (`DigitalRx.c:22916-22953`, 37 lines):
- Helper function testing whether a specific prescription source exists in a drug sale request

---

## 1xxx Pharmacy Management Handlers

All 1xxx handlers reside in `SqlHandlers.c`. These handlers manage pharmacy terminal operations including shift management, data downloads, stock tracking, and financial reporting.

---

### TR1001: Open Shift

**File**: `SqlHandlers.c:404-1000`
**Chunk**: ch_sql_011

According to repository.json:

- **Pattern**: Uses the Generic Handler Template pattern
  - `HandlerToMsg_1001` / `HandlerToSpool_1001`: Thin wrappers calling `GenHandlerToMsg_1001` with `fromspool` flag
  - `GenHandlerToMsg_1001` (SqlHandlers.c:454-1000): Core logic
- **Workflow**:
  1. Validates pharmacy via `IS_PHARMACY_OPEN_X()`
  2. Checks institute code
  3. Verifies/updates Maccabi price list
  4. Writes `TR1001_INS_pharmacy_log` (43 input columns, according to MacODBC_MyOperators.c)
  5. Handles access-conflict retry via `Conflict_Test` macro
- **Operators**: `TR1001_READ_Pharmacy`, `TR1001_INS_pharmacy_log`

---

### TR1011/1013/1014: Drug List, Close Shift, Price List

**File**: `SqlHandlers.c:1010-2308`
**Chunk**: ch_sql_012

According to repository.json:

**DownloadDrugList** (`SqlHandlers.c:1010-1496`):
- Downloads drug records from database to file for TR1011 and TR2094
- Handles drug deletion/update codes
- Pharmacy category filtering (Maccabi/Assuta)
- **Operators**: `TR1011_2094_DrugListDownload_cur` (shared cursor)

**GenHandlerToMsg_1013** (`SqlHandlers.c:1556-1819`):
- Close shift handler using Generic Handler Template pattern
- Records closing code, date, and hour
- Access-conflict retry logic
- **Operators**: `TR1013_INS_pharmacy_log`

**HandlerToMsg_1014** (`SqlHandlers.c:1830-2308`):
- Price list download to file
- Supports custom WHERE clauses and multiple cursor modes
- **Operators**: `TR1014_PriceList_cur`

---

### TR1022/6022: Stock Entry

**File**: `SqlHandlers.c:2322-2867`
**Chunk**: ch_sql_013

According to repository.json:

- **Pattern**: Uses the Generic Handler Template pattern
  - `GENERIC_HandlerToMsg_1022_6022` (`SqlHandlers.c:2322-2849`): Core logic
  - `HandlerToMsg_1022_6022` / `HandlerToSpool_1022_6022`: Thin wrappers
- **Purpose**: Inserts stock entry records into STOCK and STOCK_REPORT tables
- **Features**: Dynamic buffer allocation, supports realtime and spool modes
- **Operators**: `TR1022_INS_Stock`, `TR1022_INS_StockReport`

---

### TR1026: Display Messages

**Handler**: `HandlerToMsg_1026`
**File**: `SqlHandlers.c:2878-3317`
**Chunk**: ch_sql_013

According to repository.json:

- **Purpose**: Retrieves display messages for pharmacy terminals
- **Features**: Special character filtering for terminal display compatibility
- **Operators**: `TR1026_message_id_cur`

---

### TR1028: Financial Status

**Handler**: `HandlerToMsg_1028`
**File**: `SqlHandlers.c:3327-3637`
**Chunk**: ch_sql_013

According to repository.json:

- **Purpose**: Financial status/balance query for pharmacy
- **Operators**: `TR1028_READ_PharmacyDailySum`

---

### Table Download Handlers (1043-1080)

**File**: `SqlHandlers.c:3648-7426`
**Chunks**: ch_sql_014, ch_sql_016

According to repository.json, four table-download handlers share a common cursor-to-file pattern:

| Transaction | Handler | Lines | Purpose |
|-------------|---------|------:|---------|
| 1043 | HandlerToMsg_1043 | 3648-3970 | Error message table download |
| 1047 | HandlerToMsg_1047 | 3982-4348 | Discount pricing rules download |
| 1049 | HandlerToMsg_1049 | 4359-4738 | Supplier/vendor records download |
| 1051 | HandlerToMsg_1051 | 4748-5158 | Pharmacy center master data download |
| 1080 | HandlerToMsg_1080 | 7080-7382 | Pharmacy contract/authorization records download |

Each handler:
1. Opens a cursor on the respective database table
2. Iterates rows via `FETCH_NEXT` loop
3. Formats each row into the output buffer or file
4. Returns the formatted data to the pharmacy terminal

---

### TR1053: Money/Employee Items

**File**: `SqlHandlers.c:5169-5554` (generic), `SqlHandlers.c:7055-7070, 7410-7426` (wrappers)
**Chunk**: ch_sql_015, ch_sql_016

According to repository.json:

- **Pattern**: Uses the Generic Handler Template pattern
  - `GENERIC_HandlerToMsg_1053` (`SqlHandlers.c:5169-5554`): Core logic
  - `HandlerToMsg_1053` (`SqlHandlers.c:7055-7070`): Thin realtime wrapper
  - `HandlerToSpool_1053` (`SqlHandlers.c:7410-7426`): Thin spool wrapper
- **Purpose**: Inserts into MONEY_EMPTY and MONEY_EMP_LINES tables with dynamic record allocation
- **Operators**: `TR1053_INS_money_empty`, `TR1053_INS_money_emp_lines`

---

### TR1055: DUR/OD Message Retrieval

**Handler**: `HandlerToMsg_1055`
**File**: `SqlHandlers.c:5566-7054` (1,489 lines)
**Chunk**: ch_sql_015

According to repository.json, this is the **largest function in SqlHandlers.c**:

- **Purpose**: Retrieves and formats DUR/OD (Drug Utilization Review / Overdose) warning messages for pharmacy terminal display
- **Features**:
  - Multi-severity classification system for drug warnings
  - Dynamic variable substitution in message templates
  - Message destination routing
  - Prescription error sorting via `presc_err_comp` (SqlHandlers.c:1-394) — sorts by check_type (DUR before OVERDOSE), then severity, then drug code
  - Title/detail/summary formatting for terminal display
- **Relationship**: This handler retrieves messages generated by the DUR/OD engine (`test_interaction_and_overdose` at SqlHandlers.c:7444-8267) during prior sale transactions (2003, 5003, 6003)
- **Operators**: `TR1055_presc_mes_cur`, `TR1055_msg_fmt_cur`

---

## Specialized Handlers

---

### TR2033/6033: Special Recipe (Ishur)

**File**: `ElectronicPr.c:17253-18098`
**Chunk**: ch_sql_026

According to repository.json:

- **Pattern**: Uses the Generic Handler Template pattern
  - `GENERIC_HandlerToMsg_2033_6033` (`ElectronicPr.c:17253-18045`, 793 lines): Core logic
  - `HandlerToMsg_2033_6033` (`ElectronicPr.c:18054-18071`): Thin realtime wrapper
  - `HandlerToSpool_2033_6033` (`ElectronicPr.c:18080-18098`): Thin spool wrapper
- **Purpose**: Creates/updates pharmacy special recipe authorizations (ishurim)
- **Workflow**:
  1. Validates member via `IS_PHARMACY_OPEN_X()` and `read_member()`
  2. Checks for existing ishur via `ExecSQL(TR2033_READ_check_if_ishur_already_used)`
  3. Handles AS/400 ishur extension
  4. Validates specialty largo percentages
  5. Writes via `ExecSQL(TR2033_INS_pharmacy_ishur)`
- **Used by**: The DUR/OD system checks ishur records created by this handler

---

### Reference Table Downloads

According to repository.json, reference table downloads are organized in two groups:

#### 2060-2068: Electronic Rx Reference Tables (ElectronicPr.c:18109-18997, ch_sql_027)

| Transaction | Table | Lines |
|-------------|-------|------:|
| 2060 | how_to_take | ~163 |
| 2062 | drug_shape | ~163 |
| 2064 | unit_of_measure | ~163 |
| 2066 | reason_not_sold | ~191 |
| 2068 | discount_remarks | ~163 |

Each handler reads the full reference table via cursor and formats the output for terminal download.

#### 5051-5061: Nihul Tikrot Reference Tables (ElectronicPr.c:23531-25404, ch_sql_030)

| Transaction | Table | Lines | Notes |
|-------------|-------|------:|-------|
| 5051 | credit_reasons | ~256 | |
| 5053 | subsidy_messages | ~260 | |
| 5055 | tikra_type | ~309 | |
| 5057 | hmo_membership | ~303 | |
| 5059 | virtual_store_reason_texts | ~300 | |
| 5061 | druggencomponents | ~529 | JSON-capable, added November 2025 |

The TR5061 handler (`HandlerToMsg_5061`) is notable as the most recently added reference download, with incremental download support and JSON output capability.

---

### Data Update Handlers (2070-2084)

**File**: `ElectronicPr.c:19008-21454`
**Chunk**: ch_sql_028

According to repository.json, eight handlers for data update/download:

| Transaction | Handler | Purpose |
|-------------|---------|---------|
| 2070/2092 | HandlerToMsg_2070_2092 | Economy pricing (handles both transaction codes) |
| 2072 | HandlerToMsg_2072 | Prescription sources |
| 2074 | HandlerToMsg_2074 | Drug extension (simplified) |
| 2076 | HandlerToMsg_2076 | Authorizing authorities |
| 2078 | HandlerToMsg_2078 | Sale + sale_fp + sale_bonus + sale_target (503 lines — most complex in group) |
| 2080 | HandlerToMsg_2080 | Gadgets |
| 2082 | HandlerToMsg_2082 | PharmDrugNotes (incremental) |
| 2084 | HandlerToMsg_2084 | DrugNotes (incremental) |

The 2082 and 2084 handlers support incremental download — only transmitting records modified since the pharmacy terminal's last sync, using `GetTblUpFlg()` (MessageFuncs.c:587-648) for modification date checking.

---

### Reconciliation and Reporting (2086-2101)

**File**: `ElectronicPr.c:21466-23519`
**Chunk**: ch_sql_029

According to repository.json:

| Transaction | Handler | Lines | Purpose |
|-------------|---------|------:|---------|
| 2086 | HandlerToMsg_2086 | ~260 | Pharmacy daily summary reconciliation |
| 2088 | HandlerToMsg_2088 | ~753 | Detailed list of pharmacy drug sales |
| 2090/5090 | HandlerToMsg_X090 | ~409 | Single pharmacy drug sale details (dual-mode: TR2090 and TR5090) |
| 2096 | HandlerToMsg_2096 | ~210 | usage_instruct table download (October 2020) |
| 2101 | HandlerToMsg_2101 | ~172 | usage_instr_reason_changed table download (October 2020) |

**Supporting functions in this chunk**:
- `SALE_compare`: qsort comparator for sale records
- `DRUG_compare`: qsort comparator for drug records
- `init_package_size_array` (~143 lines): Re-initializes the cached drug package sizes from the drug_list table — called both at startup and during hourly refresh

---

## AS/400 Integration

AS/400 integration is a cross-cutting concern that spans multiple handlers and uses two distinct communication paths.

### Path 1: TikrotRPC — DB2 Stored Procedure

**File**: `TikrotRPC.c:1-494`
**Chunk**: ch_sql_083

According to code at `TikrotRPC.c:304-444`:

- **Target**: DB2 stored procedure `RKPGMPRD.TIKROT`
- **Called by**: `HandlerToMsg_5003` (Nihul Tikrot subsidy sale) exclusively
- **Function**: `CallTikrotSP()` — calls the stored procedure with 8 parameters (4 input, 4 output)
- **DSN selection**: Reads `/pharm/bin/NihulTikrot.cfg` — defaults to "ASTEST", uses "AS400" if file contains "PRODUCTION" (according to `TikrotRPC.c:213-239`)
- **Retry**: Retry-with-recursion on `SQL_ERROR` with 500ms sleep; `RPC_Recursed` flag prevents infinite recursion by limiting to a single retry
- **Timeout**: 3-second query timeout via `SQL_ATTR_QUERY_TIMEOUT`
- **Runtime control**: Enabled/disabled via SIGUSR1/SIGUSR2 signals (`TikrotRPC_Enabled` flag toggled by signal handlers at `SqlServer.c:2131-2135`)
- **Initialization**: `InitTikrotSP()` (public shell) → `RPC_Initialize()` (internal) which allocates ODBC handles and connects to AS/400 DB2

### Path 2: AS/400 Meishar — TCP Socket Client

**Files**: `As400UnixMediator.h:1-153`, `As400UnixMediator.c:1-660`
**Chunks**: ch_sql_084, ch_sql_085

According to code at `As400UnixMediator.h:38-44` and `As400UnixMediator.c:106-278`:

- **Target**: Server "AS400" on port 9400 (as defined in `As400UnixMediator.h`)
- **Timeouts**: RECV_TIMEOUT=10s, SEND_TIMEOUT=4s
- **Protocol**: Semicolon-delimited text, framed as `<length>:<payload>`
- **Functions**:
  - `as400EligibilityTest()`: Checks member eligibility against AS/400 Meishar system — called from TR2003, TR5003, TR6003
  - `as400SaveSaleRecord()`: Saves completed sale records to AS/400 for archival — called from delivery handlers (TR2005, TR5005, TR6005)
  - `ParseString()`: Semicolon-delimited response parser extracting 30+ fields via `strchr`/`strncpy`/`atol`/`atoi`
- **Error suppression**: 180-second window to prevent sporadic connection failures from triggering emergency email alerts (according to `As400UnixMediator.c:20-29`)
- **Maintenance window**: Alert suppression between 23:00-01:00 (according to `As400UnixMediator.c:234-241`)

### AS/400 Function Codes

According to `As400UnixMediator.h` enums:

| FUNCTION_NUM | Value | Purpose |
|-------------|-------|---------|
| SALE | 0 | Submit sale record |
| ELIGIBILITY | 1 | Check member eligibility |
| DELETE_SALE | 2 | Delete a sale record |
| SALE_STATUS_CHECK | 3 | Check sale status |

| ACTION_TYPE | Purpose |
|-------------|---------|
| Values defined in enum | Various AS/400 action codes |

| RETURN_CODE | Range | Purpose |
|-------------|-------|---------|
| 0-14 | Various | AS/400 response codes |

### Calling Pattern

```
Sale Handlers (TR2003, TR5003, TR6003)
    │
    ├── as400EligibilityTest()        → Meishar TCP (port 9400)
    │       └── SocketAPI.c wrappers
    │
    └── CallTikrotSP()                → DB2 stored proc (TR5003 only)
            └── ODBC 2.x APIs

Delivery Handlers (TR2005, TR5005, TR6005)
    │
    └── as400SaveSaleRecord()         → Meishar TCP (port 9400)
            └── SocketAPI.c wrappers
```

---

## Utility Functions

### ElectronicPr.c Utilities (ch_sql_031)

**File**: `ElectronicPr.c:25409-26158`

According to repository.json:

**process_spools** (`ElectronicPr.c:25409-25681`, 273 lines):
- Processes pending AS/400 Gadget spool queue on timed interval
- Purges old `sql_srv_audit` rows
- Called from the main loop at `SqlServer.c:627-952`

**FindDrugPurchLimit** (`ElectronicPr.c:25687-25831`):
- **DEAD CODE** — wrapped in `#if 0` since 16 November 2025
- Originally found purchase-limit rules for a drug; appears to have been replaced by `ReadDrugPurchaseLimData_x` in MessageFuncs.c

**read_member** (`ElectronicPr.c:25838-25888`, 51 lines):
- Reads member data into global `Member` structure via `ExecSQL(READ_members_full)`
- Called by virtually all transaction handlers
- Note: According to repository.json, this function contains no error handling

**read_drug** (`ElectronicPr.c:25897-26158`, 262 lines):
- Reads drug data from DrugList into `TDrugListRow` + `TPresDrugs` structures via `ExecSQL(read_drug_READ_drug_list)`
- Called by sale handlers (TR2003, TR5003, TR6003) during drug processing loops

---

*Generated by CIDRA Documenter Agent - DOC-SQL-001*
