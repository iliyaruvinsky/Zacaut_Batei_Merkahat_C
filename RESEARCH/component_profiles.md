# Component Profiles

This file profiles each major component directory under `source_code/` with:
- Name, path, main entry file(s)
- **Exact main-file line counts** (counted by matching `^` across the file)
- Purpose (from file headers/comments)
- Key dependencies (from `#include` set near the top)
- Process type + subsystem bitmask where explicitly specified in code

## Components summary (main entrypoints)

| Component | Main file | Lines | Process type / role | Subsystem evidence |
|---|---|---:|---|---|
| FatherProcess | `source_code/FatherProcess/FatherProcess.c` | 1972 | Watchdog/supervisor daemon | Registers as `PHARM_SYS | DOCTOR_SYS` (`source_code/FatherProcess/FatherProcess.c:L350-L351`) |
| SqlServer | `source_code/SqlServer/SqlServer.c` | 2362 | SQL server worker process | `InitSonProcess(..., SQLPROC_TYPE, ..., PHARM_SYS)` (`source_code/SqlServer/SqlServer.c:L211-L211`) |
| As400UnixServer | `source_code/As400UnixServer/As400UnixServer.c` | 23042 | AS/400→Unix server bridge | `InitSonProcess(..., AS400TOUNIX_TYPE, ..., PHARM_SYS)` (`source_code/As400UnixServer/As400UnixServer.c:L194-L194`) |
| As400UnixClient | `source_code/As400UnixClient/As400UnixClient.c` | 8591 | Unix→AS/400 client bridge | `InitSonProcess(..., UNIXTOAS400_TYPE, ..., PHARM_SYS)` (`source_code/As400UnixClient/As400UnixClient.c:L174-L174`) |
| As400UnixDocServer | `source_code/As400UnixDocServer/As400UnixDocServer.c` | 2789 | Doctor-system AS/400→Unix bridge | `InitSonProcess(..., DOCAS400TOUNIX_TYPE, ..., DOCTOR_SYS)` (`source_code/As400UnixDocServer/As400UnixDocServer.c:L88-L88`) |
| As400UnixDoc2Server | `source_code/As400UnixDoc2Server/As400UnixDoc2Server.c` | 2307 | Doctor-system AS/400→Unix bridge (2nd) | `InitSonProcess(..., DOC2AS400TOUNIX_TYPE, ..., DOCTOR_SYS)` (`source_code/As400UnixDoc2Server/As400UnixDoc2Server.c:L101-L107`) |
| PharmTcpServer | `source_code/PharmTcpServer/PharmTcpServer.cpp` | 872 | TCP gateway/server | `SubSystem = DOCTOR_SYS` (`source_code/PharmTcpServer/PharmTcpServer.cpp:L91-L92`) and `InitSonProcess(..., PHARM_TCP_SERVER_TYPE, ..., SubSystem)` (`source_code/PharmTcpServer/PharmTcpServer.cpp:L124-L128`) |
| PharmWebServer | `source_code/PharmWebServer/PharmWebServer.cpp` | 1055 | HTTP gateway/server | `SubSystem = PHARM_SYS` (`source_code/PharmWebServer/PharmWebServer.cpp:L92-L92`) and `InitSonProcess(..., WEBSERVERPROC_TYPE, ..., SubSystem)` (`source_code/PharmWebServer/PharmWebServer.cpp:L123-L128`) |
| ShrinkPharm | `source_code/ShrinkPharm/ShrinkPharm.c` | 431 | Maintenance tool | Purpose explicitly “ODBC equivalent of ShrinkDoctor…” (`source_code/ShrinkPharm/ShrinkPharm.c:L7-L10`) |
| GenLib | `source_code/GenLib/*` | n/a | Shared runtime library | Implements shared memory, semaphores, unix-domain sockets, process spawn, error logging (see per-file notes below) |
| GenSql | `source_code/GenSql/GenSql.c` | 2836 | Shared DB/SQL library | “interface functions… connect/disconnect database” (`source_code/GenSql/GenSql.c:L7-L10`) |
| Include | `source_code/Include/*` | n/a | Shared headers | `Global.h` explicitly describes itself as an include for “all macabi project sources” (`source_code/Include/Global.h:L28-L30`) |
| Served_includes | `source_code/Served_includes/served.hpp` | n/a | Third-party HTTP library (headers+tests+build artifacts) | “Served HTTP REST Library” (`source_code/Served_includes/version.hpp:L33-L41`); used by `PharmWebServer` (`source_code/PharmWebServer/PharmWebServer.cpp:L30-L33`) |
| Served_source | `source_code/Served_source/served/*` | n/a | Third-party HTTP library (source distribution) | “Served HTTP REST Library” (`source_code/Served_source/served/version.hpp:L33-L41`) |

---

## FatherProcess

- **Path**: `source_code/FatherProcess/`
- **Main file**: `FatherProcess.c` (**1972 lines**)
- **Purpose**: “Server for starting the MACABI system and keeping all servers up. Servers to start up and parameters are read from database.” (`source_code/FatherProcess/FatherProcess.c:L7-L11`)
- **Key dependencies**:
  - `<MacODBC.h>` (`source_code/FatherProcess/FatherProcess.c:L29-L29`)
  - `MsgHndlr.h` (`source_code/FatherProcess/FatherProcess.c:L30-L30`)
- **Role / process type**: supervisor daemon; initializes shared resources and spawns children:
  - Creates semaphore + shared memory + tables (`source_code/FatherProcess/FatherProcess.c:L303-L334`)
  - Registers itself as `FATHERPROC_TYPE` for both subsystems (`source_code/FatherProcess/FatherProcess.c:L350-L351`)

## SqlServer

- **Path**: `source_code/SqlServer/`
- **Main file**: `SqlServer.c` (**2362 lines**)
- **Purpose**: “Sql server process… opens a connection to db… wait[s] for sql requests… executes… returns to wait.” (`source_code/SqlServer/SqlServer.c:L7-L16`)
- **Key dependencies**: `<MacODBC.h>`, `MsgHndlr.h`, `PharmacyErrors.h`, `TikrotRPC.h`, `MessageFuncs.h`, plus bundled `cJSON.c`. (`source_code/SqlServer/SqlServer.c:L29-L35`)
- **Subsystem/process type**: initialized as a son process with `SQLPROC_TYPE` and `PHARM_SYS`. (`source_code/SqlServer/SqlServer.c:L211-L211`)

## As400UnixServer

- **Path**: `source_code/As400UnixServer/`
- **Main file**: `As400UnixServer.c` (**23042 lines**)
- **Purpose**: AS400↔Unix comms module (header says “client-process”, but file name is “Server”; treat naming carefully). (`source_code/As400UnixServer/As400UnixServer.c:L7-L13`)
- **Key dependencies**: `<MacODBC.h>`, `MsgHndlr.h`, `As400UnixServer.h`, `DBFields.h`, `PharmDBMsgs.h`, `TreatmentTypes.h`, `PharmacyErrors.h`. (`source_code/As400UnixServer/As400UnixServer.c:L32-L40`)
- **Subsystem/process type**: initialized as `AS400TOUNIX_TYPE` with `PHARM_SYS`. (`source_code/As400UnixServer/As400UnixServer.c:L194-L194`)

## As400UnixClient

- **Path**: `source_code/As400UnixClient/`
- **Main file**: `As400UnixClient.c` (**8591 lines**)
- **Purpose**: “get records from LOCAL DB & pass them to REMOTE DB.” (`source_code/As400UnixClient/As400UnixClient.c:L17-L20`)
- **Key dependencies**: `<MacODBC.h>`, `MsgHndlr.h`, `As400UnixClient.h`, `PharmDBMsgs.h`. (`source_code/As400UnixClient/As400UnixClient.c:L29-L34`)
- **Subsystem/process type**: initialized as `UNIXTOAS400_TYPE` with `PHARM_SYS`. (`source_code/As400UnixClient/As400UnixClient.c:L174-L174`)

## As400UnixDocServer

- **Path**: `source_code/As400UnixDocServer/`
- **Main file**: `As400UnixDocServer.c` (**2789 lines**)
- **Purpose**: AS400↔Unix doctor bridge; explicitly described as a new ODBC-only version with many stub transactions. (`source_code/As400UnixDocServer/As400UnixDocServer.c:L12-L17`)
- **Key dependencies**: `<MacODBC.h>`, `MsgHndlr.h`, `multix.h`, `global_1.h`, `As400UnixServer.h`, `DB.h`, `GenSql.h`, `tpmintr.h`, `DBFields.h`, `PharmDBMsgs.h`. (`source_code/As400UnixDocServer/As400UnixDocServer.c:L31-L42`)
- **Subsystem/process type**: initialized as `DOCAS400TOUNIX_TYPE` with `DOCTOR_SYS`. (`source_code/As400UnixDocServer/As400UnixDocServer.c:L88-L88`)

## As400UnixDoc2Server

- **Path**: `source_code/As400UnixDoc2Server/`
- **Main file**: `As400UnixDoc2Server.c` (**2307 lines**)
- **Purpose**: AS400↔Unix doctor bridge (variant); described as an ODBC-only “new version” with stub transactions. (`source_code/As400UnixDoc2Server/As400UnixDoc2Server.c:L15-L20`)
- **Key dependencies**: `<MacODBC.h>`, `MsgHndlr.h`, `multix.h`, `global_1.h`, `As400UnixServer.h`, `DB.h`, `GenSql.h`, `tpmintr.h`, `DBFields.h`, `PharmDBMsgs.h`. (`source_code/As400UnixDoc2Server/As400UnixDoc2Server.c:L34-L45`)
- **Subsystem/process type**: initialized as `DOC2AS400TOUNIX_TYPE` with `DOCTOR_SYS`. (`source_code/As400UnixDoc2Server/As400UnixDoc2Server.c:L101-L107`)

## PharmTcpServer

- **Path**: `source_code/PharmTcpServer/`
- **Main file**: `PharmTcpServer.cpp` (**872 lines**)
- **Purpose**: “main source for network server for doctor system” (per file header). (`source_code/PharmTcpServer/PharmTcpServer.cpp:L6-L9`)
- **Key dependencies**: uses `Global.h` via `extern "C"` include (C++/C boundary). (`source_code/PharmTcpServer/PharmTcpServer.cpp:L38-L41`)
- **Subsystem/process type**:
  - `static int SubSystem = DOCTOR_SYS;` (`source_code/PharmTcpServer/PharmTcpServer.cpp:L91-L92`)
  - calls `InitSonProcess(..., PHARM_TCP_SERVER_TYPE, retrys, ClientPort, SubSystem)` (`source_code/PharmTcpServer/PharmTcpServer.cpp:L124-L128`)
- **Notable shared-memory usage**: opens `DIPR_TABLE` and `PROC_TABLE` for died-process and process tracking. (`source_code/PharmTcpServer/PharmTcpServer.cpp:L137-L142`)

## PharmWebServer

- **Path**: `source_code/PharmWebServer/`
- **Main file**: `PharmWebServer.cpp` (**1055 lines**)
- **Purpose**: “main source for network server for doctor system” (per file header). (`source_code/PharmWebServer/PharmWebServer.cpp:L6-L9`)
- **Key dependencies**:
  - Served HTTP library: `<served/served.hpp>` (`source_code/PharmWebServer/PharmWebServer.cpp:L30-L33`)
  - `Global.h` via `extern "C"` include (C++/C boundary). (`source_code/PharmWebServer/PharmWebServer.cpp:L37-L40`)
- **Subsystem/process type**:
  - `static int SubSystem = PHARM_SYS;` (`source_code/PharmWebServer/PharmWebServer.cpp:L92-L92`)
  - calls `InitSonProcess(..., WEBSERVERPROC_TYPE, retrys, 0, SubSystem)` (`source_code/PharmWebServer/PharmWebServer.cpp:L123-L128`)

## ShrinkPharm

- **Path**: `source_code/ShrinkPharm/`
- **Main file**: `ShrinkPharm.c` (**431 lines**)
- **Purpose**: “ODBC equivalent of ShrinkDoctor for use in the MS-SQL Pharmacy application”. (`source_code/ShrinkPharm/ShrinkPharm.c:L7-L10`)
- **Key dependencies**: `<MacODBC.h>` (`source_code/ShrinkPharm/ShrinkPharm.c:L16-L16`)
- **DB usage pattern**: sets ODBC query timeout/deadlock priority and connects via `SQLMD_connect()` retry loop. (`source_code/ShrinkPharm/ShrinkPharm.c:L119-L137`)

## GenLib (shared runtime library)

- **Path**: `source_code/GenLib/`
- **Role**: implements shared services used broadly by server processes:
  - Shared memory table engine implementation (`SharedMemory.cpp`: **4774 lines**)  
    Purpose statement: “library routines for shared memory handling.” (`source_code/GenLib/SharedMemory.cpp:L12-L17`)
  - Named unix-domain sockets (“named pipes”) and message framing (`Sockets.c`: **1758 lines**)  
    Purpose statement: “library routines for communication sockets handling.” (`source_code/GenLib/Sockets.c:L12-L17`)
  - Semaphores (`Semaphores.c`: **734 lines**)  
    Purpose statement: “Library routines for handling semaphores.” (`source_code/GenLib/Semaphores.c:L12-L16`)
  - Process bootstrap + spawn (`Memory.c`: **2196 lines**)  
    Purpose statement: “Library routines for handling memory buffers.” (`source_code/GenLib/Memory.c:L12-L16`)
  - Error logging (`GeneralError.c`: **770 lines**)  
    Purpose statement: “library routines for error handling.” (`source_code/GenLib/GeneralError.c:L12-L16`)

## GenSql (shared DB/SQL library)

- **Path**: `source_code/GenSql/`
- **Main file**: `GenSql.c` (**2836 lines**)
- **Purpose**: “interface functions for INFORMIX… connect , disconnect database.” (`source_code/GenSql/GenSql.c:L7-L10`)
- **Notable exported data**: defines the canonical shared-memory table schema via `TableTab[]`. (`source_code/GenSql/GenSql.c:L86-L126`)

## Include (shared headers directory)

- **Path**: `source_code/Include/`
- **Role**: central header collection shared across multiple binaries and libraries; `Global.h` explicitly positions itself as the include file “for all macabi project sources”. (`source_code/Include/Global.h:L28-L30`)
- **Primary index**: see `RESEARCH/header_inventory.md` for a per-header inventory.

## Served_includes / Served_source (third-party Served HTTP library)

- **Paths**:
  - `source_code/Served_includes/` (headers + tests + local build artifacts)
  - `source_code/Served_source/` (full source distribution, docs, examples, tests, and local build artifacts)
- **Identity**: “Served HTTP REST Library” and version metadata. (`source_code/Served_includes/version.hpp:L33-L41`, `source_code/Served_source/served/version.hpp:L33-L41`)
- **Product usage**: included by `PharmWebServer.cpp` via `<served/served.hpp>`. (`source_code/PharmWebServer/PharmWebServer.cpp:L30-L33`)

