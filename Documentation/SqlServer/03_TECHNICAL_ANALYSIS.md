# SqlServer - Technical Analysis

**Component**: SqlServer
**Task ID**: DOC-SQL-001
**Generated**: 2026-02-13
**Source**: `source_code/SqlServer/`

---

## Validation Pipeline (Cross-Cutting Concern #1)

The core sale handlers (HandlerToMsg_2003, HandlerToMsg_5003, HandlerToMsg_6003) share a common validation pipeline. According to the call graph analysis and code tracing, the pipeline appears to execute in the following order:

### Pipeline Stages

```
1. IS_PHARMACY_OPEN_X()              → Pharmacy authorization
2. test_special_prescription()        → Special/controlled substance validation
3. test_purchase_limits()             → Purchase limit rules (9 methods)
4. test_interaction_and_overdose()    → Drug-drug interaction + overdose detection
5. test_pharmacy_ishur()              → Pharmacy-level authorization
6. test_mac_doctor_drugs_electronic() → Doctor-drug participation/pricing
7. CheckHealthAlerts()                → Health alert rules engine (in-memory cache)
8. update_doctor_presc()              → Doctor prescription table updates
```

### Stage 1: IS_PHARMACY_OPEN_X (MessageFuncs.c:1057-1414)

According to code at `MessageFuncs.c:1057-1414` (358 lines):

- **Purpose**: Tests if the pharmacy is open and authorized for the current transaction
- **Data source**: Reads directly from database (not shared memory)
- **Output**: Populates `PHARMACY_INFO` structure with pharmacy type, permissions, Hebrew encoding flag
- **Retry logic**: Contains a do-while loop with access-conflict retry via `Conflict_Test`
- **Operators used**: `IsPharmOpen_READ_pharmacy`, `IsPharmOpen_READ_pharmacy_type_params`, `IsPharmOpen_READ_hebrew_encoding`
- **Called by**: Virtually all transaction handlers as the first validation step

### Stage 2: test_special_prescription (MessageFuncs.c:2201-3543)

According to code at `MessageFuncs.c:2201-3543` (1,343 lines):

- **Purpose**: Comprehensive validation of special-confirmation prescriptions
- **Checks performed** (based on code analysis):
  - Dosage limits and refill counts
  - Treatment intervals and usage patterns
  - Multi-source confirmation requirements
  - Doctor authorization validation
  - Pharmacist override logic
  - Partial narcotics prescriptions
  - Same-day same-pharmacy sales
  - Fully/partly sold status tracking
- **Operators used**: `TestSpecialPresc_READ_special_prescs` (15 output columns), `TestSpecialPresc_READ_prescription_usage`, `CheckForPartlySoldDocRx_partly_sold_cur`
- **Called by**: All sale handlers (2003, 5003, 6003) and request handlers (2001, 6001/6101)

### Stage 3: test_purchase_limits (MessageFuncs.c:4553-5283)

According to code at `MessageFuncs.c:4553-5283` (731 lines):

- **Purpose**: Tests current sale against purchase-limit rules
- **Supporting functions**:
  - `determine_actual_usage` (316 lines at `MessageFuncs.c:3548-3863`): Calculates actual drug usage based on treatment days, ingredient dosing, and supply units
  - `ReadDrugPurchaseLimData_x` (633 lines at `MessageFuncs.c:3908-4540`): Reads purchase-limit configuration from database
- **Limit methods**: Handles 9 distinct methods (0-8) for checking purchase history over a configurable month window
- **Called by**: Core sale handlers (2003, 5003, 6003)

### Stage 4: test_interaction_and_overdose (SqlHandlers.c:7444-8267)

According to code at `SqlHandlers.c:7444-8267` (823 lines):

- **Purpose**: Core DUR (Drug Utilization Review) and overdose testing
- **Mechanism**:
  - Checks drug-drug interactions within the current prescription
  - Checks interactions against member's blood drug history (`v_DrugsBuf`)
  - Calculates accumulated dosage for overdose detection
  - Handles duration-based arithmetic across overlapping prescriptions
- **Decision logic**: Uses static decision tables (`DurDecisionTable[2][2][2][6]`, `OD_DecisionTable[2][4][5]`) defined at `SqlHandlers.c:1-394` for severity determination
- **Supporting functions**:
  - `IS_DRUG_INTERACAT` (`MessageFuncs.c:1431-1514`): Database lookup for drug-drug interaction
  - `get_drugs_in_blood` (`MessageFuncs.c:1787-2195`): Retrieves drugs currently in member's blood, with same-member static caching
  - `write_interaction_and_overdose` (`SqlHandlers.c:8279-8442`): Persists validated DUR/OD error records for audit

### Stage 5: test_pharmacy_ishur (MessageFuncs.c:5297-5822)

According to code at `MessageFuncs.c:5297-5822` (526 lines):

- **Purpose**: Tests pharmacy-level authorization (ishur) for member drugs
- **Checks**: Handles '29 gimel' forms, specialist letters, old-age home overrides
- **Supporting functions**:
  - `check_sale_against_pharm_ishur` (195 lines at `MessageFuncs.c:5836-6030`): Checks individual sale lines against pharmacy ishur, populates extended-days and profession arrays
  - `test_special_drugs_electronic` (123 lines at `MessageFuncs.c:6101-6223`): Validates drug against special-drug rules (dentist/home-visit), determines participation and price code

### Stage 6: test_mac_doctor_drugs_electronic (MessageFuncs.c:6248-6873)

According to code at `MessageFuncs.c:6248-6873` (626 lines):

- **Purpose**: Tests participation percent for Maccabi-doctor drugs
- **Logic**: Handles specialist letters, old-age home rules, purchase history, preferred-drug swap
- **Related functions**:
  - `predict_member_participation` (394 lines at `MessageFuncs.c:6889-7282`): Predicts participation for prescription-request transactions (TR6001)
  - `GetParticipatingCode` (179 lines at `MessageFuncs.c:7566-7744`): Finds lowest participation code for a drug
  - `FIND_DRUG_EXTENSION` (81 lines at `MessageFuncs.c:7475-7555`): Looks up drug extension record

### Stage 7: CheckHealthAlerts (MessageFuncs.c:8094-9960)

According to code at `MessageFuncs.c:8094-9960` (1,867 lines):

- **Purpose**: In-memory rules cache engine for health alerts
- **Cache mechanism**: Refreshes from database periodically; uses 23-column cursor for rule definitions
- **5 test modes** (based on code analysis):
  1. Standard mode
  2. Compliance mode
  3. New-drug mode
  4. Ingredient-count mode
  5. All-tests combined mode
- **Supporting function**: `SumMemberDrugsUsed` (84 lines at `MessageFuncs.c:7991-8074`): Sums total drugs bought by member at reduced cost over a given duration, using dirty-read isolation

### Stage 8: update_doctor_presc (MessageFuncs.c:9969-11142)

According to code at `MessageFuncs.c:9969-11142` (1,174 lines):

- **Purpose**: Updates doctor prescription tables after drug sale or deletion
- **Features**:
  - Carry-forward unit tracking
  - Dosage tracking across prescription lines
  - Late-arrival queue insertion for deferred processing
  - Spool-on-failure logic for resilience
  - Supports both normal and late-arrival modes
- **Supporting function**: `ProcessRxLateArrivalQueue` (245 lines at `MessageFuncs.c:11146-11390`): Processes queue of late-arriving doctor prescriptions from `rx_late_arrival_q` table with retry logic

### Pipeline Coverage by Handler

| Validation Stage | TR2003 | TR5003 | TR6003 | TR6001/6101 | TR2001 |
|-----------------|:------:|:------:|:------:|:-----------:|:------:|
| IS_PHARMACY_OPEN_X | Yes | Yes | Yes | Yes | Yes |
| test_special_prescription | Yes | Yes | Yes | Yes | Yes |
| test_purchase_limits | Yes | Yes | Yes | - | - |
| test_interaction_and_overdose | Yes | Yes | Yes | - | - |
| test_pharmacy_ishur | Yes | - | Yes | - | - |
| test_mac_doctor_drugs_electronic | Yes | - | Yes | Predict | - |
| CheckHealthAlerts | Yes | Yes | Yes | - | - |
| update_doctor_presc | Yes | Yes | Yes | - | - |

---

## Error Severity Escalation Model

### SetErrorVar_x Engine (MessageFuncs.c:650-1043)

According to code at `MessageFuncs.c:650-1043` (394 lines):

The master error-tracking engine manages error arrays and severity comparison. It appears to support two modes:

1. **Old single-error mode**: Tracks one error code with severity
2. **New array mode**: Manages multiple error codes with deduplication

Accessed via macros (based on chunk analysis):
- `SetErrorVarInit` — Initialize error tracking
- `SetErrorVar` — Set a single error
- `SetErrorVarArr` — Add to error array
- `SetErrorVarDelete` — Remove an error
- `SetErrorVarTell` — Query current error state

### DUR/OD Decision Tables (SqlHandlers.c:1-394)

According to code at `SqlHandlers.c:1-394`:

- **DurDecisionTable[2][2][2][6]**: 3-dimensional decision matrix for Drug Utilization Review severity decisions
- **OD_DecisionTable[2][4][5]**: Decision matrix for overdose severity decisions
- **presc_err_comp**: A `qsort` comparator function that sorts `PRESC_ERR` records by: check_type (DUR before OVERDOSE), then severity, then drug code

### DUR/OD Message Retrieval (SqlHandlers.c:5566-7054)

According to code at `SqlHandlers.c:5566-7054` (1,489 lines), `HandlerToMsg_1055`:

- Retrieves and formats DUR/OD warning messages for pharmacy terminal display
- Multi-severity classification system
- Dynamic variable substitution in message templates
- Message destination routing
- Prescription error sorting via `presc_err_comp`
- Title/detail/summary formatting

---

## Drug Interaction and Overdose Engine

### Architecture (SqlHandlers.c:7444-8458)

According to code at `SqlHandlers.c:7444-8458`:

```
test_interaction_and_overdose()
    │
    ├── For each drug pair in current prescription:
    │   └── IS_DRUG_INTERACAT() → DB lookup for interaction
    │
    ├── For each drug against blood drug history:
    │   ├── get_drugs_in_blood() → Cached retrieval of member's blood drugs
    │   └── IS_DRUG_INTERACAT() → DB lookup for interaction
    │
    ├── Accumulated dosage calculation:
    │   └── Duration-based arithmetic for overlapping prescriptions
    │
    ├── Decision table lookup:
    │   ├── DurDecisionTable → DUR severity
    │   └── OD_DecisionTable → Overdose severity
    │
    └── write_interaction_and_overdose() → Persist audit records
```

### Blood Drug Caching (MessageFuncs.c:1787-2195)

According to code at `MessageFuncs.c:1787-2195` (409 lines):

`get_drugs_in_blood()` uses a static caching strategy:
- If called for the same member as the previous call, returns cached results
- Otherwise performs fresh database query via `GetDrugsInBlood_blood_drugs_cur`
- Handles multiple call types and deadlock/restart scenarios via `TransactionRestart()` which calls `RollbackWork`

### Interaction Ishur Check (MessageFuncs.c:1519-1754)

According to code at `MessageFuncs.c:1519-1754` (236 lines):

`CheckForInteractionIshur()` checks whether a doctor approved a drug-drug interaction by looking up doctor prescription IDs, member blood drugs, and interaction approval records. This function uses a dynamic SQL operator (`CheckForInteractionIshur_READ_IshurCount` at `MacODBC_MyOperators.c:4182`) where `SQL_CommandText=NULL` — the SQL is built at runtime by the calling function (VER-SQL-004).

---

## Reliability and Recovery Patterns

### Access Conflict Retry

According to code analysis, the `Conflict_Test` macro (defined at `SqlServer.c:40-46` and also in handler files) implements a standard SQL access-conflict retry pattern:

```c
#define Conflict_Test(r)
if (SQLERR_code_cmp(SQLERR_access_conflict) == MAC_TRUE) {
    SQLERR_error_test();
    r = MAC_TRUE;
    break;
}
```

This pattern appears in virtually every handler function, wrapped in a do-while retry loop.

### Deadlock Handling

According to code at `SqlServer.c:281-326`:
- `DEADLOCK_PRIORITY = 8` (above normal) — privileges realtime SqlServer operations over background processes
- ShrinkPharm (documented separately) uses `DEADLOCK_PRIORITY = -2` to yield to SqlServer

### Auto-Reconnect

According to MacODBC documentation (`Documentation/MacODBC/`):
- `ODBC_Exec` converts specific native errors to `DB_CONNECTION_BROKEN` return code
- Higher-level error handling in the main loop detects this and triggers full database reconnection
- Error categories triggering reconnect include: `SQL_TRN_CLOSED_BY_OTHER_SESSION`, `DB_TCP_PROVIDER_ERROR`, `TABLE_SCHEMA_CHANGED`, `DB_CONNECTION_BROKEN`

### Spool-on-Failure

According to code at `SqlServer.c:96-102, 1689-1918`:

The `SpoolErr[]` array defines error codes that trigger spool processing:
- `ERR_NO_MEMORY`
- `ERR_DATABASE_ERROR`
- `ERR_SHARED_MEM_ERROR`

When a transaction encounters these errors, the post-dispatch logic appears to queue the transaction for later retry processing via `process_spools()`.

### AS/400 Error Suppression (As400UnixMediator.c)

According to code at `As400UnixMediator.c:20-29, 234-241`:

The AS/400 Meishar TCP client implements a 180-second error suppression window to prevent sporadic connection failures from triggering emergency email alerts. Additionally, error alerts appear to be suppressed during the 23:00-01:00 maintenance window.

### TikrotRPC Retry (TikrotRPC.c)

According to code at `TikrotRPC.c:213-239`:

`CallTikrotSP()` implements retry-with-recursion on `SQL_ERROR`, using a 500ms sleep between attempts. The `RPC_Recursed` flag prevents infinite recursion by limiting to a single retry.

---

## Cache Loading Functions

### LoadPrescrSource (SqlServer.c:2220-2282)

According to code at `SqlServer.c:2220-2282`:
- Loads the `PrescrSource[]` array from the `prescr_source` database table via cursor
- Array is used by handlers to validate prescription source codes
- Refreshed hourly from the main loop

### LoadTrnPermissions (SqlServer.c:2285-2362)

According to code at `SqlServer.c:2285-2362`:
- Loads the `TrnPermissions[]` array from the `pharm_trn_permit` database table via cursor
- Controls which transactions are permitted for each pharmacy type
- Refreshed hourly from the main loop

---

## DUR/OD Specialized System (Phase 7)

### Complete DUR/OD Architecture

The Drug Utilization Review and Overdose Detection system spans multiple chunks:

1. **Engine**: `test_interaction_and_overdose` in SqlHandlers.c:7444-8267
2. **Drug interaction lookup**: `IS_DRUG_INTERACAT` in MessageFuncs.c:1431-1514
3. **Blood drug retrieval**: `get_drugs_in_blood` in MessageFuncs.c:1787-2195
4. **Interaction ishur**: `CheckForInteractionIshur` in MessageFuncs.c:1519-1754
5. **Decision tables**: DurDecisionTable and OD_DecisionTable in SqlHandlers.c:1-394
6. **Message retrieval**: HandlerToMsg_1055 in SqlHandlers.c:5566-7054
7. **Audit persistence**: `write_interaction_and_overdose` in SqlHandlers.c:8279-8442

### Ishur Reference Table Handling

According to code at `ElectronicPr.c:17253-18098`:

`GENERIC_HandlerToMsg_2033_6033` (793 lines) creates/updates pharmacy special recipe authorizations (ishurim). Validates member, checks for existing ishur, handles AS/400 ishur extension, validates specialty largo percentages.

---

## REST Service Integration (Cross-Cutting Concern #5)

### Architecture (MessageFuncs.c:12833-13631)

According to code at `MessageFuncs.c:12833-13631`, this appears to be the newest addition (approximately April 2025), replacing legacy database lookups with HTTP REST calls via libcurl:

| Function | Lines | Purpose |
|----------|------:|---------|
| TranslateTechnicalID | 12833-12958 | Translates technical member ID to TZ (Israeli national ID) via REST |
| EligibilityWithoutCard | 12962-13149 | Checks member eligibility without card via REST |
| set_up_consumed_REST_service | 13154-13392 | Initializes REST_SERVICE structure from database template |
| consume_REST_service | 13395-13568 | Executes REST call via CURL with URL construction and header management |
| REST_CurlMemoryCallback | 13571-13612 | CURL write callback (static) |
| REST_CurlMemoryClear | 13616-13631 | Resets REST buffer (static) |

### Call Chain

According to the graph analysis:
- `CheckForServiceWithoutCard()` (in `MessageFuncs.c:11559-11731`) calls `EligibilityWithoutCard()`
- `EligibilityWithoutCard()` calls `consume_REST_service()` which uses `curl_easy_init/setopt/perform`
- REST service URLs are stored in the database and assembled at runtime via `set_up_consumed_REST_service()`
- Response parsing uses `cJSON_Parse()` to extract approval-type, valid-from/valid-to fields

---

## Table Update Check (MessageFuncs.c:587-648)

According to code at `MessageFuncs.c:587-648`:

`GetTblUpFlg()` checks a database table's modification date. Returns `MAC_TRUE` if the table was updated after the pharmacy's last refresh. This appears to be used for incremental download support — pharmacy terminals only download tables that have changed since their last sync.

---

*Generated by CIDRA Documenter Agent - DOC-SQL-001*
