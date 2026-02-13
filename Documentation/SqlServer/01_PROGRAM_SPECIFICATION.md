# SqlServer - Program Specification

**Component**: SqlServer
**Task ID**: DOC-SQL-001
**Generated**: 2026-02-13
**Source**: `source_code/SqlServer/`

---

## Overview

According to the file header at `SqlServer.c:7-16`, SqlServer is described as:
> "Sql server process. When MACABI system is going up, the father process runs few copies of this process and keep minimum non-busy copies during system is up. This process opens a connection to db when waking up and wait for sql requests from PC. On request, this process executes the request and returns to wait. On MACABI system shutdown, this process closes connection to db."

This program appears to be the **primary pharmacy transaction processing server** for the MACCABI healthcare backend system, handling ~60+ transaction codes via a central dispatch switch.

**Author**: Ely Levy (Reshuma), as stated at `SqlServer.c:19-20`
**Original Date**: 30.05.1996

---

## Purpose

Based on code analysis, SqlServer appears to:

1. **Initialize as a child process** — Registers via `InitSonProcess(SQLPROC_TYPE, PHARM_SYS)` (according to code at `SqlServer.c:211`)
2. **Connect to the database** — `SQLMD_connect()` retry loop with ODBC configuration (as seen in `SqlServer.c:281-326`)
3. **Load runtime parameters** — Populates ~70+ global sysparams from database (as seen in `SqlServer.c:327-626`)
4. **Process transactions** — Central `MakeAndSendReturnMessage()` dispatch routes ~60+ transaction codes (as seen in `SqlServer.c:955-1918`)
5. **Handle signal events** — SIGPIPE, SIGTERM, SIGSEGV, SIGUSR1/SIGUSR2 handlers (as seen in `SqlServer.c:1920-2217`)
6. **Perform periodic maintenance** — Hourly sysparams refresh, spool processing, cache reloading (as seen in `SqlServer.c:627-952`)

---

## File Structure

| File | Lines | Purpose |
|------|------:|---------|
| SqlServer.c | 2,362 | Main server loop, dispatch switch, signal handlers, cache loaders |
| SqlHandlers.c | 8,458 | 1xxx pharmacy management handlers + DUR/OD engine (27 functions) |
| ElectronicPr.c | 26,161 | 2xxx electronic prescription + 5xxx subsidy handlers (41 functions) |
| DigitalRx.c | 22,955 | 6xxx digital prescription handlers (8 functions) |
| MessageFuncs.c | 13,631 | Shared business logic utilities (61 functions) |
| MacODBC_MyOperators.c | 8,418 | ODBC SQL operator definitions (265 active cases) |
| MacODBC_MyCustomWhereClauses.c | 274 | Custom WHERE clause fragments |
| TikrotRPC.c | 494 | DB2 stored procedure RPC for Nihul Tikrot |
| As400UnixMediator.c | 660 | TCP client for AS/400 Meishar system |
| As400UnixMediator.h | 153 | AS/400 Meishar client header and constants |
| SocketAPI.c | 265 | TCP socket wrapper implementation |
| SocketAPI.h | 47 | TCP socket API header |
| DebugPrint.c | 105 | Debug logging utility |
| **Total** | **83,983** | **13 source files** |

*Line counts sourced from Chunker verification (CH-SQL-001), based on reading every file.*

---

## Transaction Dispatch Map

Based on the central switch statement at `SqlServer.c:1286-1687`, the following transaction families are routed:

### 1xxx — Pharmacy Management (SqlHandlers.c)

| Transaction | Handler | Location | Purpose |
|-------------|---------|----------|---------|
| 1001 | GenHandlerToMsg_1001 | SqlHandlers.c:454-1000 | Open shift |
| 1011 | DownloadDrugList | SqlHandlers.c:1010-1496 | Drug list download |
| 1013 | GenHandlerToMsg_1013 | SqlHandlers.c:1556-1819 | Close shift |
| 1014 | HandlerToMsg_1014 | SqlHandlers.c:1830-2308 | Price list download |
| 1022/6022 | GENERIC_HandlerToMsg_1022_6022 | SqlHandlers.c:2322-2849 | Stock entry |
| 1026 | HandlerToMsg_1026 | SqlHandlers.c:2878-3317 | Display messages |
| 1028 | HandlerToMsg_1028 | SqlHandlers.c:3327-3637 | Financial status |
| 1043 | HandlerToMsg_1043 | SqlHandlers.c:3648-3970 | Error messages download |
| 1047 | HandlerToMsg_1047 | SqlHandlers.c:3982-4348 | Discount rules download |
| 1049 | HandlerToMsg_1049 | SqlHandlers.c:4359-4738 | Supplier download |
| 1051 | HandlerToMsg_1051 | SqlHandlers.c:4748-5158 | Centers download |
| 1053 | GENERIC_HandlerToMsg_1053 | SqlHandlers.c:5169-5554 | Money/employee items |
| 1055 | HandlerToMsg_1055 | SqlHandlers.c:5566-7054 | DUR/OD message retrieval |
| 1080 | HandlerToMsg_1080 | SqlHandlers.c:7080-7382 | Pharmacy contract download |

### 2xxx — Electronic Prescription (ElectronicPr.c)

| Transaction | Handler | Location | Purpose |
|-------------|---------|----------|---------|
| 2001 | HandlerToMsg_2001 | ElectronicPr.c:223-1045 | Prescription drugs list request |
| 2003 | HandlerToMsg_2003 | ElectronicPr.c:1057-5122 | **E-Rx drug sale (4,066 lines)** |
| 2005 | HandlerToMsg_2005 / HandlerToSpool_2005 | ElectronicPr.c:5135-7640 | E-Rx delivery (realtime + spool) |
| 2033/6033 | GENERIC_HandlerToMsg_2033_6033 | ElectronicPr.c:17253-18098 | Special recipe (ishur) |
| 2060-2068 | HandlerToMsg_206x | ElectronicPr.c:18109-18997 | Reference table downloads (5 tables) |
| 2070-2084 | HandlerToMsg_207x/208x | ElectronicPr.c:19008-21454 | Data update/download (8 handlers) |
| 2086-2101 | HandlerToMsg_208x/209x/2101 | ElectronicPr.c:21466-23519 | Reconciliation + reporting |

### 5xxx — Nihul Tikrot Subsidy (ElectronicPr.c)

| Transaction | Handler | Location | Purpose |
|-------------|---------|----------|---------|
| 5003 | HandlerToMsg_5003 | ElectronicPr.c:7653-13598 | **Subsidy drug sale (5,946 lines)** |
| 5005 | HandlerToMsg_5005 / HandlerToSpool_5005 | ElectronicPr.c:13611-17239 | Subsidy delivery (realtime + spool) |
| 5051-5061 | HandlerToMsg_505x/5061 | ElectronicPr.c:23531-25404 | Reference table downloads (6 tables) |
| 5090 | HandlerToMsg_X090 | ElectronicPr.c:22016-22424 | Single drug sale details |

### 6xxx — Digital Prescription (DigitalRx.c)

| Transaction | Handler | Location | Purpose |
|-------------|---------|----------|---------|
| 6001/6101 | HandlerToMsg_6001_6101 | DigitalRx.c:273-5340 | **Digital Rx request (5,067 lines)** |
| 6003 | HandlerToMsg_6003 | DigitalRx.c:5352-13280 | **Digital drug sale (7,928 lines)** |
| 6005 | HandlerToMsg_6005 / HandlerToSpool_6005 | DigitalRx.c:13293-19726 | Digital delivery (realtime + spool) |
| 6011 | HandlerToMsg_6011 | DigitalRx.c:19747-21028 | Member drug history download |
| 6102 | HandlerToMsg_6102 | DigitalRx.c:21041-21323 | Chanut Virtualit count |
| 6103 | HandlerToMsg_6103 | DigitalRx.c:21335-22913 | Chanut Virtualit pricing |

---

## The "Big Three" Sale Handlers

These three functions together represent approximately 21.4% of the entire codebase and contain the primary business logic:

| Handler | File | Lines | Transaction | Purpose |
|---------|------|------:|-------------|---------|
| HandlerToMsg_6003 | DigitalRx.c | 7,928 | 6003 | Digital prescription drug sale |
| HandlerToMsg_5003 | ElectronicPr.c | 5,946 | 5003 | Nihul Tikrot subsidy drug sale |
| HandlerToMsg_2003 | ElectronicPr.c | 4,066 | 2003 | Electronic prescription drug sale |
| **Total** | | **17,940** | | **21.4% of 83,983 lines** |

All three share a common validation pipeline (documented in detail in 03_TECHNICAL_ANALYSIS.md).

---

## Generic Handler Template Pattern

According to code analysis, many handlers use a `GENERIC_HandlerToMsg_XXXX` pattern with thin realtime/spool wrappers:

| Generic Function | Wrappers | Location |
|-----------------|----------|----------|
| GenHandlerToMsg_1001 | HandlerToMsg_1001 / HandlerToSpool_1001 | SqlHandlers.c:404-1000 |
| GENERIC_HandlerToMsg_1022_6022 | HandlerToMsg_1022_6022 / HandlerToSpool_1022_6022 | SqlHandlers.c:2322-2867 |
| GENERIC_HandlerToMsg_1053 | HandlerToMsg_1053 / HandlerToSpool_1053 | SqlHandlers.c:5169-5554, 7055-7070, 7410-7426 |
| GENERIC_HandlerToMsg_2033_6033 | HandlerToMsg_2033_6033 / HandlerToSpool_2033_6033 | ElectronicPr.c:17253-18098 |

The wrapper functions differ only in the `fromspool` flag parameter, which controls whether the handler applies duplicate detection and recovery logic for deferred/batch processing.

---

## Shared Business Logic Functions (MessageFuncs.c)

Based on chunk analysis, MessageFuncs.c contains 61 utility functions organized by domain:

| Domain | Key Functions | Lines |
|--------|--------------|------:|
| Message parsing | AsciiToBin, GetChar, GetShort, GetInt, GetLong, GetString, get_message_header (16 functions) | 100-571 |
| Error tracking | SetErrorVar_x, GetTblUpFlg | 587-1043 |
| Pharmacy authorization | IS_PHARMACY_OPEN_X | 1057-1414 |
| Drug interaction | IS_DRUG_INTERACAT, get_drugs_in_blood, CheckForInteractionIshur | 1431-2195 |
| Special prescription | test_special_prescription (1,343 lines) | 2201-3543 |
| Usage/purchase limits | determine_actual_usage, test_purchase_limits, ReadDrugPurchaseLimData_x | 3548-5283 |
| Pharmacy ishur | test_pharmacy_ishur, check_sale_against_pharm_ishur, test_special_drugs_electronic | 5297-6223 |
| Doctor-drug pricing | test_mac_doctor_drugs_electronic, predict_member_participation, FIND_DRUG_EXTENSION, GetParticipatingCode (7 functions) | 6248-7978 |
| Health alerts | CheckHealthAlerts (1,867 lines), SumMemberDrugsUsed | 7991-9960 |
| Doctor updates | update_doctor_presc (1,174 lines), ProcessRxLateArrivalQueue | 9969-11390 |
| Member services | CheckForServiceWithoutCard, find_preferred_drug, read_member_price, SetMemberFlags (12 functions) | 11394-12830 |
| REST integration | TranslateTechnicalID, EligibilityWithoutCard, consume_REST_service (6 functions) | 12833-13631 |

---

## Dependencies (#include)

According to lines 29-34 of `SqlServer.c`:

| Header | Line | Purpose |
|--------|------|---------|
| `<MacODBC.h>` | 29 | ODBC infrastructure layer (compiled with `#define MAIN` at line 25) |
| `"MsgHndlr.h"` | 30 | Message handling definitions, ~70+ runtime sysparams globals |
| `"PharmacyErrors.h"` | 31 | Error code definitions for pharmacy clients |
| `"TikrotRPC.h"` | 32 | Nihul Tikrot DB2 RPC declarations |
| `"MessageFuncs.h"` | 33 | Shared message utility function prototypes |
| `<cJSON.c>` | 34 | JSON parsing library (included as source) |

Note: `MacODBC.h` transitively includes `GenSql.h`, `Global.h`, and `MacODBC_MyOperatorIDs.h`, which provide the shared-memory table engine, process types, system constants, and ODBC operator identifiers.

---

## Preprocessor Defines

According to lines 24-108 of `SqlServer.c`:

| Define | Line | Value/Purpose |
|--------|------|---------------|
| `__IN_PROCESS_MESSAGE__` | 24 | Enables in-process message handling |
| `MAIN` | 25 | Instantiates all MacODBC and MsgHndlr globals |
| `GET_POS()` | 38 | Returns `curr_pos_in_mesg` pointer |
| `Conflict_Test(r)` | 40-46 | SQL access conflict retry macro |
| `NORMAL_EXIT` | 106 | Value: 0 |
| `ENDLESS_LOOP` | 107 | Value: 1 |

---

## Global Variables

### Runtime Configuration (from sysparams, lines 48-86)

According to `SqlServer.c:48-86`:

| Variable | Type | Line | Purpose |
|----------|------|------|---------|
| accept_sock | int | 49 | Socket descriptor for client connections |
| mypid | static int | 50 | Process ID |
| message_count | static int | 51 | Transaction counter |
| caught_signal | int | 52 | Signal number caught for shutdown |
| NeedToUpdateSysparams | int | 53 | Flag for sysparams refresh |
| no_vat_multiplier | double | 54 | VAT calculation (default 0.85470085 for 17% VAT) |
| Form29gMessagesEnabled | short | 55 | Form 29-G message control |
| ExpensiveDrugMacDocRx | int | 58 | Expensive drug threshold (Maccabi doctor) |
| ExpensiveDrugNotMacDocRx | int | 59 | Expensive drug threshold (non-Maccabi doctor) |
| min_refill_days | short | 60 | Minimum refill interval |
| vat_percent | float | 70 | VAT percentage (default 16.0) |
| MinimumNormalMemberTZ | int | 79 | Minimum normal member ID threshold |

### Signal Handler Structures (lines 117-120)

| Variable | Type | Purpose |
|----------|------|---------|
| sig_act_pipe | struct sigaction | SIGPIPE handler configuration |
| sig_act_terminate | struct sigaction | SIGTERM/SIGSEGV handler configuration |
| sig_act_NihulTikrotEnable | struct sigaction | SIGUSR1 handler configuration |
| sig_act_NihulTikrotDisable | struct sigaction | SIGUSR2 handler configuration |

### Dispatch State (line 88-104)

| Variable | Type | Purpose |
|----------|------|---------|
| send_state | SEND_STATES enum | Tracks dispatch phase (BEFORE_UPDATE, AFTER_UPDATE, CONN_HANGUP) |
| SpoolErr[] | int[] | Error codes that trigger spool processing |

---

## Build Configuration

According to the Makefile (as documented in `SqlServer_deepdive.md`):

- **Target**: `sqlserver.exe`
- **Source files**: SqlServer.c, SqlHandlers.c, SocketAPI.c, DebugPrint.c, As400UnixMediator.c, ElectronicPr.c, TikrotRPC.c, MessageFuncs.c, DigitalRx.c (`Makefile:14-16`)
- **Libraries**: `-lodbc -lcurl` (`Makefile:41-43`)
- **Shared build rules**: `../Util/Defines.mak` and `../Util/Rules.mak` (`Makefile:4, 31`)

---

## External Integrations

| Integration | File | Protocol | Purpose |
|-------------|------|----------|---------|
| AS/400 Meishar | As400UnixMediator.c | TCP socket (port 9400) | Eligibility tests + sale record submission |
| AS/400 DB2 | TikrotRPC.c | ODBC (stored procedure) | Nihul Tikrot subsidy calculation |
| REST services | MessageFuncs.c:12833-13631 | HTTP/CURL | Member ID translation + eligibility without card |
| ODBC database | MacODBC.h (via `#define MAIN`) | ODBC (MS-SQL/Informix) | All database operations via 265 SQL operators |

---

## JSON Dual-Mode Support

According to analysis, newer transactions support both JSON and legacy binary message formats via the cJSON library:

| Transaction | JSON Support | Added |
|-------------|-------------|-------|
| 6001/6101 | Request/response | Based on code analysis |
| 6011 | Response output | July 2025 (according to code at `DigitalRx.c:19747`) |
| 6102 | JSON-only | Based on code analysis |
| 6103 | JSON-only | Based on code analysis |
| 5061 | JSON-capable download | November 2025 |

The dispatch preamble at `SqlServer.c:955-1285` appears to handle JSON request parsing with fallback to binary format.

---

## Documentation Limitations

### What CANNOT be determined from code alone:
- Actual contents of database configuration tables (sysparams values)
- Runtime performance characteristics under load
- Complete wire protocol field layouts (appear to be defined in PharmDBMsgs.h but depend on runtime transaction type)
- Business specification details beyond what is encoded in handler logic

### What IS known from code:
- Complete transaction dispatch map (`SqlServer.c:1286-1687`)
- Validation pipeline call sequence (traced from all three core sale handlers)
- Signal handler installation and behavior (`SqlServer.c:129-280, 1920-2217`)
- Database connection and initialization sequence (`SqlServer.c:281-626`)
- ODBC operator catalog (265 cases in MacODBC_MyOperators.c)
- AS/400 integration paths and failure modes (TikrotRPC.c, As400UnixMediator.c)

---

*Generated by CIDRA Documenter Agent - DOC-SQL-001*
