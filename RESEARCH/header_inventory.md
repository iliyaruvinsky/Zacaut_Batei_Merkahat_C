# Header File Inventory

This inventory covers:
- `source_code/Include/` (project headers used across components)
- `source_code/Served_includes/` (third-party Served HTTP REST Library plus local build artifacts)

All claims below are anchored to concrete symbols and/or file prologues with `file:Lx-Ly` citations.

## Include/

### Core platform / IPC / shared memory

- `Global.h`: central include for “all macabi project sources”; pulls in IPC/system headers and defines system-wide constants (table names, process types, subsystem flags) and shared-memory table engine types (`TABLE_DATA`, `TABLE`, etc.).  
  Evidence: `source_code/Include/Global.h:L28-L35`, `source_code/Include/Global.h:L46-L95`, `source_code/Include/Global.h:L129-L159`, `source_code/Include/Global.h:L219-L240`, `source_code/Include/Global.h:L505-L795`.

### Database / ODBC / SQL abstraction

- `GenSql.h`: “General definitions for working with databases”; provides `sqlca_t/sqlca/SQLCODE` and maps `SQLMD_connect()` to `INF_CONNECT(MacUser,MacPass,MacDb)` in Informix mode; also defines error code categories.  
  Evidence: `source_code/Include/GenSql.h:L1-L17`, `source_code/Include/GenSql.h:L37-L75`, `source_code/Include/GenSql.h:L184-L200`.

- `GenSql1.h`: older variant of `GenSql.h` (still maps `SQLMD_connect()`→`INF_CONNECT(...)` and defines Informix error codes / helper prototypes).  
  Evidence: `source_code/Include/GenSql1.h:L1-L17`, `source_code/Include/GenSql1.h:L112-L125`, `source_code/Include/GenSql1.h:L146-L161`.

- `MacODBC.h`: Maccabi ODBC “infrastructure” layer; defines ODBC provider/command enums, limits (`ODBC_MAX_STICKY_STATEMENTS`, `ODBC_MAX_PARAMETERS`), and exposes the macro API built around `ODBC_Exec()`; includes `GenSql.h` and `MacODBC_MyOperatorIDs.h`.  
  Evidence: `source_code/Include/MacODBC.h:L7-L41`, `source_code/Include/MacODBC.h:L90-L139`.

- `MacODBC_MyOperatorIDs.h`: global `enum ODBC_Operation` list (operation IDs used by the `MacODBC.h` macro layer for diagnostics and dispatch).  
  Evidence: `source_code/Include/MacODBC_MyOperatorIDs.h:L7-L35`.

- `GenSql_ODBC_OperatorIDs.h`: partial/legacy operator-ID list placed in the include directory (note the file header says this should be application-local).  
  Evidence: `source_code/Include/GenSql_ODBC_OperatorIDs.h:L7-L23`, `source_code/Include/GenSql_ODBC_OperatorIDs.h:L26-L44`.

- `marianna_Include_MacODBC_MyOperatorIDs.h`: a snapshot/variant of `MacODBC_MyOperatorIDs.h` (same goal: enumerate ODBC operation IDs).  
  Evidence: `source_code/Include/marianna_Include_MacODBC_MyOperatorIDs.h:L1-L47`.

### Message handling (T-SWITCH, parsing helpers, and client-facing errors)

- `MsgHndlr.h`: defines data types/sizes for data “coming from T-SWITCH”; includes `cJSON.h` and `libcurl`; declares handler prototypes and defines global runtime parameters/arrays (e.g., `PrescrSource`, `TrnPermissions`).  
  Evidence: `source_code/Include/MsgHndlr.h:L6-L16`, `source_code/Include/MsgHndlr.h:L28-L33`, `source_code/Include/MsgHndlr.h:L70-L83`.

- `MsgHndlr_Marianna.h`: snapshot copy of `MsgHndlr.h` (explicitly annotated as copied “from 22May2020”).  
  Evidence: `source_code/Include/MsgHndlr_Marianna.h:L1-L24`, `source_code/Include/MsgHndlr_Marianna.h:L28-L33`.

- `MessageFuncs.h`: shared message parsing/validation helpers and error handling; includes ASCII parsing helpers (`GetShort/GetInt/GetString`), `get_message_header`, and the `SetErrorVar_x` API + macros.  
  Evidence: `source_code/Include/MessageFuncs.h:L45-L75`, `source_code/Include/MessageFuncs.h:L113-L149`.

- `PharmacyErrors.h`: defines error codes sent to pharmacy clients (large set of numeric error codes; includes “3-digit” codes for Trn 6001 prediction).  
  Evidence: `source_code/Include/PharmacyErrors.h:L7-L20`, `source_code/Include/PharmacyErrors.h:L38-L46`.

- `TreatmentTypes.h`: treatment-type constants used in DB tables (doctor_percents, special_prescs, drug_extension).  
  Evidence: `source_code/Include/TreatmentTypes.h:L1-L7`, `source_code/Include/TreatmentTypes.h:L13-L20`.

- `GenericServer.h`: generic “microservice server” scaffolding (includes `Global.h`/`traceback.h`, defines a `Conflict_Test` macro with built-in delay, and declares signal handlers + `HandleRequest`).  
  Evidence: `source_code/Include/GenericServer.h:L1-L7`, `source_code/Include/GenericServer.h:L155-L165`, `source_code/Include/GenericServer.h:L176-L190`.

### AS/400 integration (bridge processes + message payload structures)

- `As400UnixClient.h`: header for the client process in AS400↔Unix comms system; includes `As400UnixGlob.h`, `Global.h`, `DB.h`, `DBFields.h`, and defines message/batch constants.  
  Evidence: `source_code/Include/As400UnixClient.h:L12-L15`, `source_code/Include/As400UnixClient.h:L24-L31`, `source_code/Include/As400UnixClient.h:L49-L67`.

- `As400UnixServer.h`: header for server process in AS400↔Unix comms model; defines server state enums and multiple constants for table-update message framing.  
  Evidence: `source_code/Include/As400UnixServer.h:L12-L15`, `source_code/Include/As400UnixServer.h:L53-L73`, `source_code/Include/As400UnixServer.h:L77-L84`.

- `As400UnixGlob.h`: shared types/definitions/constants for both client & server AS400↔Unix processes; defines comm message codes and (compile-time conditional) listen ports, plus ASCII↔EBCDIC mapping table.  
  Evidence: `source_code/Include/As400UnixGlob.h:L12-L15`, `source_code/Include/As400UnixGlob.h:L33-L53`, `source_code/Include/As400UnixGlob.h:L71-L84`, `source_code/Include/As400UnixGlob.h:L94-L106`.

- `PharmDBMsgs.h`: message record structs and record-length macros for AS400 bridge messages; explicitly says “Include AFTER DBFields.h”.  
  Evidence: `source_code/Include/PharmDBMsgs.h:L1-L6`, `source_code/Include/PharmDBMsgs.h:L19-L28`, `source_code/Include/PharmDBMsgs.h:L87-L124`.

- `marianna_PharmDBMsgs.h`: variant/snapshot of `PharmDBMsgs.h` (contains record structs and record length macros; some content is under `#if 0`).  
  Evidence: `source_code/Include/marianna_PharmDBMsgs.h:L1-L7`, `source_code/Include/marianna_PharmDBMsgs.h:L152-L181`.

- `DBFields.h`: typedefs and field/message lengths for DB types; contains many `T..._len` macros used by message structures and record-length expressions.  
  Evidence: `source_code/Include/DBFields.h:L11-L14`, `source_code/Include/DBFields.h:L38-L45`.

- `DB.h`: additional field-length macros (`*_len`) used by AS/400 and other code paths.  
  Evidence: `source_code/Include/DB.h:L1-L73`.

- `DB_TableStructures.h`: defines many empty `typedef struct { } T_<table>_row;` placeholders named after DB tables.  
  Evidence: `source_code/Include/DB_TableStructures.h:L1-L8`, `source_code/Include/DB_TableStructures.h:L35-L43`.

### Tikrot RPC integration

- `TikrotRPC.h`: defines RPC input/output buffer sizes and declares `CallTikrotSP()` / `InitTikrotSP()`; also includes ODBC handles/credentials in the `_TIKROTRPC_C_` private block (values must be treated as secrets).  
  Evidence: `source_code/Include/TikrotRPC.h:L11-L19`, `source_code/Include/TikrotRPC.h:L39-L49`, `source_code/Include/TikrotRPC.h:L58-L72`.

### MultiX / TPM / legacy infrastructure headers

- `multix.h`: MultiX compatibility + platform/compiler macros; defines host/network integer translation macros and many language-like macros (`BEGIN/END/IF/THEN`).  
  Evidence: `source_code/Include/multix.h:L15-L17`, `source_code/Include/multix.h:L46-L53`, `source_code/Include/multix.h:L139-L176`, `source_code/Include/multix.h:L201-L214`.

- `tpmintr.h`: TPM interface types/enums and message structures; uses `BEGIN_ENUM/END_ENUM` macros for Linux compatibility.  
  Evidence: `source_code/Include/tpmintr.h:L11-L18`, `source_code/Include/tpmintr.h:L20-L37`, `source_code/Include/tpmintr.h:L59-L66`.

- `macsql.h`: function prototypes for `MACSQL.EC` (cursor open/fetch/close, update/insert helpers for medical/lab records).  
  Evidence: `source_code/Include/macsql.h:L1-L16`, `source_code/Include/macsql.h:L109-L116`.

- `DocSqlServer.h`: constants and function prototypes related to “Clicks 8.5” doctor transactions (`AuthProc85_*`).  
  Evidence: `source_code/Include/DocSqlServer.h:L6-L21`, `source_code/Include/DocSqlServer.h:L33-L54`.

- `DbConsRef.h`: types and DB function prototypes for “consultant reference” flows (`DbInsertConsRefRecord`, cursor open/fetch/close functions, etc.).  
  Evidence: `source_code/Include/DbConsRef.h:L4-L21`, `source_code/Include/DbConsRef.h:L39-L70`.

- `global_1.h`: large legacy header (explicit note about stripping embedded SQL); defines many macros/enums and also contains hard-coded password string macros (values must be treated as secrets).  
  Evidence: `source_code/Include/global_1.h:L1-L7`, `source_code/Include/global_1.h:L18-L37`.

### Utilities / misc

- `AddTypes.h`: local typedefs for basic scalar/pointer types and a small enum (`Found`).  
  Evidence: `source_code/Include/AddTypes.h:L4-L43`.

- `SocketAPI.h`: small socket helper API (`SocketCreate/SocketReceive/SocketSend/SocketConnect`).  
  Evidence: `source_code/Include/SocketAPI.h:L40-L47`.

- `DebugPrint.h`: debug logging helper (`DebugPrint`) and default log filename/directory.  
  Evidence: `source_code/Include/DebugPrint.h:L9-L17`.

- `CCGlobal.h`: header for C++ sources, providing `extern "C"` declarations and compatibility items.  
  Evidence: `source_code/Include/CCGlobal.h:L12-L18`, `source_code/Include/CCGlobal.h:L23-L30`.

- `traceback.h`: in-process traceback stack for fault diagnostics (`TRACE_PUSH/POP/DUMP`, etc.).  
  Evidence: `source_code/Include/traceback.h:L1-L27`, `source_code/Include/traceback.h:L101-L127`.

- `cJSON.h`: third-party cJSON library, with local additions for global error state and Win1255→UTF8 translation switch.  
  Evidence: `source_code/Include/cJSON.h:L1-L21`, `source_code/Include/cJSON.h:L88-L97`, `source_code/Include/cJSON.h:L137-L175`.

- `mac_def.h`: auto-generated macros “from codes table” (generated by `codes` program).  
  Evidence: `source_code/Include/mac_def.h:L1-L11`, `source_code/Include/mac_def.h:L14-L21`.

## Served_includes/

`source_code/Served_includes/` contains the **third-party Served HTTP REST Library** (Meltwater/MediaSift) plus its tests, CMake build scripts, and local `out/build/...` artifacts.

- `served.hpp` is the primary include, delegating to `served/net/server.hpp` and `served/multiplexer.hpp`.  
  Evidence: `source_code/Served_includes/served.hpp:L23-L29`.

- `version.hpp` identifies the library as “Served HTTP REST Library” and includes an “AUTO-GENERATION WARNING”.  
  Evidence: `source_code/Served_includes/version.hpp:L28-L41`.

- `PharmWebServer.cpp` includes `<served/served.hpp>`, tying this dependency into the product code.  
  Evidence: `source_code/PharmWebServer/PharmWebServer.cpp:L24-L33`.

Build artifacts to treat as generated (not hand-authored headers):
- `source_code/Served_includes/out/build/x64-Debug/...` including `CMakeCache.txt`, `build.ninja`, and a `ShowIncludes/foo.h`.  
  Evidence: directory listing in `source_code/Served_includes/` (see `out/build/x64-Debug/CMakeFiles/ShowIncludes/foo.h`).

## Key Structures (system-wide)

All of the following shared-memory and process/IPC structures are defined in `Global.h`:

- `PARM_DATA` (rows of `parm_table`): key/value pairs loaded from DB into shared memory.  
  Evidence: `source_code/Include/Global.h:L514-L523`.
- `UPDT_DATA` (rows of `updt_table`): per-table update timestamp records.  
  Evidence: `source_code/Include/Global.h:L524-L533`.
- `TInstanceControl`: controls how many instances of each child process should be started/maintained.  
  Evidence: `source_code/Include/Global.h:L536-L553`.
- `PROC_DATA` (rows of `proc_table`): process registry (name, log file, named pipe, pid, type, subsystem flags, timestamps).  
  Evidence: `source_code/Include/Global.h:L555-L582`.
- `DIPR_DATA` (rows of `dipr_table`): child process status/pid pairs (used as a queue/record of died processes).  
  Evidence: `source_code/Include/Global.h:L585-L590`.
- `SQLP_DATA`: “process busy” and `named_pipe` for SQL server-like processes.  
  Evidence: `source_code/Include/Global.h:L592-L597`.
- `STAT_DATA` (rows of `stat_table`): global system status, per-subsystem status, parameter revision, temp file number, sons count.  
  Evidence: `source_code/Include/Global.h:L599-L608`.
- `SSMD_DATA`: per-message processing statistics payload (terminal, spool flag, start/proc times, ids, error).  
  Evidence: `source_code/Include/Global.h:L610-L624`.
- `TSTT_DATA`: “time statistics table” rows (message counters, time counters, last messages).  
  Evidence: `source_code/Include/Global.h:L626-L646`.
- `DSMD_DATA` / `DSTT_DATA`: doctor-side analogues of SSMD/TSTT.  
  Evidence: `source_code/Include/Global.h:L652-L668`, `source_code/Include/Global.h:L674-L693`.
- Shared-memory table engine:
  - `TABLE_DATA`: schema/metadata for a shared-memory table. (`record_size`, `sort_flg`, `refresh_func`, `row_comp`, etc.)  
    Evidence: `source_code/Include/Global.h:L720-L729`.
  - `TABLE_HEADER`, `TABLE_ROW`, `TABLE`, `EXTENT_PTR`, `EXTENT_HEADER`, `MASTER_SHM_HEADER`: core shared-memory table implementation types.  
    Evidence: `source_code/Include/Global.h:L735-L795`.
- `FINDITEMPARAMS`: describes key range/offset for searches in shared memory tables.  
  Evidence: `source_code/Include/Global.h:L805-L812`.
- `REPSTATE`: control-process state tracking structure.  
  Evidence: `source_code/Include/Global.h:L823-L831`.

## Key Macros / Constants (system-wide)

From `Global.h`:

- Shared-memory table names:
  - `PARM_TABLE`, `PROC_TABLE`, `UPDT_TABLE`, `STAT_TABLE`, `TSTT_TABLE`, `DSTT_TABLE`, `PRSC_TABLE`, `MSG_RECID_TABLE`, `DIPR_TABLE`  
  Evidence: `source_code/Include/Global.h:L127-L146`.

- Named pipe prefix: `NAMEDP_PREFIX` used when constructing per-process “named pipe”/Unix-domain socket paths.  
  Evidence: `source_code/Include/Global.h:L151-L152`.

- System status constants: `GOING_UP`, `SYSTEM_UP`, `GOING_DOWN`, `SYSTEM_DOWN`.  
  Evidence: `source_code/Include/Global.h:L171-L177`.

- Process type identifiers: `FATHERPROC_TYPE`, `SQLPROC_TYPE`, …, `WEBSERVERPROC_TYPE`, `DOC2AS400TOUNIX_TYPE`, etc.  
  Evidence: `source_code/Include/Global.h:L219-L240`.

## Security-sensitive findings (redacted values)

These locations contain **hard-coded secret values** (string literals) and should be treated as sensitive. The values are intentionally **not** copied into any research output.

- `TikrotRPC.h`: hard-coded RPC DSN username/password string literals in the `_TIKROTRPC_C_` private block.  
  Evidence: `source_code/Include/TikrotRPC.h:L69-L72`.

- `global_1.h`: hard-coded connect-password string macros.  
  Evidence: `source_code/Include/global_1.h:L5-L7`.

